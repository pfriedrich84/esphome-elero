#include "elero.h"
#include "elero_runtime_blind_logic.h"
#include "elero_dispatch_logic.h"
#include "esphome/core/log.h"
#include <mutex>

namespace esphome {
namespace elero {

static const char *TAG = "elero";

// ---------------------------------------------------------------------------
// Runtime blind management
// ---------------------------------------------------------------------------

void Elero::drain_runtime_queues() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  uint32_t now = millis();
  for (auto &entry : this->runtime_blinds_) {
    auto &rb = entry.second;
    if (rb.command_queue.empty()) {
      rb.last_queue_drain_ms = now;  // reset timer while idle
    } else {
      // Queue aging: clear stale commands if blind is offline
      if (dispatch_logic::should_clear_stale_queue(now, rb.last_queue_drain_ms,
                                                    ELERO_COMMAND_QUEUE_MAX_AGE_MS)) {
        ESP_LOGW(TAG, "Runtime blind 0x%06x queue stale, clearing %d commands",
                 rb.blind_address, (int)rb.command_queue.size());
        while (!rb.command_queue.empty()) rb.command_queue.pop();
        rb.send_packets_count = 0;
        rb.last_queue_drain_ms = now;
        continue;
      }
      uint8_t cmd_byte = rb.command_queue.front();
      t_elero_command cmd{};
      cmd.counter = rb.cmd_counter;
      cmd.blind_addr = rb.blind_address;
      cmd.remote_addr = rb.remote_address;
      cmd.channel = rb.channel;
      cmd.pck_inf[0] = rb.pck_inf[0];
      cmd.pck_inf[1] = rb.pck_inf[1];
      cmd.hop = rb.hop;
      cmd.payload[0] = rb.payload_1;
      cmd.payload[1] = rb.payload_2;
      cmd.payload[4] = cmd_byte;
      if (this->send_command(&cmd) == SendResult::OK) {
        rb.send_packets_count++;
        if (rb.send_packets_count >= this->send_repeats_) {
          rb.command_queue.pop();
          rb.send_packets_count = 0;
          rb.last_queue_drain_ms = now;
          rb.cmd_counter = runtime_blind_logic::next_command_counter(rb.cmd_counter);
        }
      } else {
        break;  // TX queue full or failed — stop enqueuing, retry next loop
      }
    }
  }
}

void Elero::poll_runtime_blinds_() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  uint32_t now = millis();
  for (auto &entry : this->runtime_blinds_) {
    auto &rb = entry.second;
    if (runtime_blind_logic::should_poll_runtime_blind(now, rb.last_poll_ms,
                                                       rb.poll_intvl_ms, rb.command_queue.empty())) {
      rb.last_poll_ms = now;
      if (rb.command_queue.size() < ELERO_MAX_COMMAND_QUEUE) {
        rb.command_queue.push(ELERO_COMMAND_COVER_CHECK);
        ESP_LOGD(TAG, "Periodic poll for runtime blind 0x%06x", rb.blind_address);
      }
    }
  }
}

void Elero::update_runtime_blind_direction_(RuntimeBlind &rb, uint8_t state) {
  int8_t old_dir = rb.moving_direction;
  rb.moving_direction = runtime_blind_logic::direction_for_state(
      state, ELERO_STATE_TOP, ELERO_STATE_BOTTOM,
      ELERO_STATE_START_MOVING_UP, ELERO_STATE_MOVING_UP,
      ELERO_STATE_START_MOVING_DOWN, ELERO_STATE_MOVING_DOWN,
      rb.moving_direction);
  if (state == ELERO_STATE_TOP)
    rb.position = 1.0f;
  else if (state == ELERO_STATE_BOTTOM)
    rb.position = 0.0f;
  else if (runtime_blind_logic::state_stops_runtime_motion(
               state, ELERO_STATE_STOPPED, ELERO_STATE_INTERMEDIATE, ELERO_STATE_TILT,
               ELERO_STATE_BLOCKING, ELERO_STATE_OVERHEATED, ELERO_STATE_TIMEOUT))
    rb.moving_direction = 0;
  if (old_dir != rb.moving_direction) {
    rb.last_recompute_ms = millis();
  }
}

void Elero::recompute_runtime_positions_() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  uint32_t now = millis();
  for (auto &entry : this->runtime_blinds_) {
    auto &rb = entry.second;
    if (!runtime_blind_logic::can_recompute_position(rb.moving_direction,
                                                     rb.open_duration_ms,
                                                     rb.close_duration_ms,
                                                     rb.position))
      continue;

    uint32_t elapsed = now - rb.last_recompute_ms;
    if (elapsed == 0)
      continue;

    if (elapsed > ELERO_TIMEOUT_MOVEMENT) {
      rb.last_recompute_ms = now;
      continue;
    }

    rb.position = runtime_blind_logic::recompute_position(
        rb.position, rb.moving_direction, elapsed,
        rb.open_duration_ms, rb.close_duration_ms);
    rb.last_recompute_ms = now;
  }
}

// ─── Runtime blind adoption ───────────────────────────────────────────────

bool Elero::adopt_blind(const DiscoveredBlind &discovered, const std::string &name,
                        DeviceType type) {
  if (this->is_cover_configured(discovered.blind_address))
    return false;
  if (this->is_light_configured(discovered.blind_address))
    return false;
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (this->runtime_blinds_.find(discovered.blind_address) != this->runtime_blinds_.end())
    return false;
  RuntimeBlind rb{};
  rb.blind_address = discovered.blind_address;
  rb.remote_address = discovered.remote_address;
  rb.channel = discovered.channel;
  rb.pck_inf[0] = discovered.pck_inf[0];
  rb.pck_inf[1] = discovered.pck_inf[1];
  rb.hop = discovered.hop;
  rb.payload_1 = discovered.payload_1;
  rb.payload_2 = discovered.payload_2;
  rb.name = name.empty() ? "Adopted" : name;
  rb.device_type = type;
  rb.last_seen_ms = discovered.last_seen;
  rb.last_rssi = discovered.rssi;
  rb.last_state = discovered.last_state;
  this->runtime_blinds_.insert({discovered.blind_address, std::move(rb)});
  this->own_remote_addresses_.insert(discovered.remote_address);
  ESP_LOGI(TAG, "Adopted runtime %s 0x%06x as \"%s\"",
           type == DeviceType::LIGHT ? "light" : "blind",
           discovered.blind_address, rb.name.c_str());
  return true;
}

bool Elero::remove_runtime_blind(uint32_t addr) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  auto it = this->runtime_blinds_.find(addr);
  if (it != this->runtime_blinds_.end()) {
    ESP_LOGI(TAG, "Removed runtime blind 0x%06x", addr);
    this->runtime_blinds_.erase(it);
    return true;
  }
  return false;
}

bool Elero::send_runtime_command(uint32_t addr, uint8_t cmd_byte) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  auto it = this->runtime_blinds_.find(addr);
  if (it != this->runtime_blinds_.end()) {
    if (it->second.command_queue.size() < ELERO_MAX_COMMAND_QUEUE) {
      it->second.command_queue.push(cmd_byte);
      return true;
    }
    return false;
  }
  return false;
}

bool Elero::update_runtime_blind_settings(uint32_t addr, uint32_t open_dur_ms,
                                          uint32_t close_dur_ms, uint32_t poll_intvl_ms) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  auto it = this->runtime_blinds_.find(addr);
  if (it != this->runtime_blinds_.end()) {
    it->second.open_duration_ms = open_dur_ms;
    it->second.close_duration_ms = close_dur_ms;
    it->second.poll_intvl_ms = poll_intvl_ms;
    return true;
  }
  return false;
}

bool Elero::is_blind_adopted(uint32_t addr) const {
  std::lock_guard<std::mutex> lock(state_mutex_);
  return this->runtime_blinds_.find(addr) != this->runtime_blinds_.end();
}


}  // namespace elero
}  // namespace esphome
