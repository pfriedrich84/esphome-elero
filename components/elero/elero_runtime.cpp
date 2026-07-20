#include "elero.h"
#include "elero_runtime_blind_logic.h"
#include "esphome/core/log.h"
#include <mutex>

namespace esphome {
namespace elero {

static const char *TAG = "elero";

// ---------------------------------------------------------------------------
// Runtime blind management
// ---------------------------------------------------------------------------

void Elero::poll_runtime_blinds_() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  uint32_t now = millis();
  for (auto &entry : this->runtime_blinds_) {
    auto &rb = entry.second;
    if (rb.delivery && runtime_blind_logic::should_poll_runtime_blind(
                           now, rb.last_poll_ms, rb.poll_intvl_ms, rb.delivery->empty())) {
      if (intent_was_accepted(rb.delivery->submit({CommandIntentKind::CHECK, 0}, now))) {
        rb.last_poll_ms = now;
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
  CommandDeliveryConfig delivery_config{};
  delivery_config.profile.blind_address = discovered.blind_address;
  delivery_config.profile.remote_address = discovered.remote_address;
  delivery_config.profile.channel = discovered.channel;
  delivery_config.profile.pck_inf[0] = discovered.pck_inf[0];
  delivery_config.profile.pck_inf[1] = discovered.pck_inf[1];
  delivery_config.profile.hop = discovered.hop;
  delivery_config.profile.payload_1 = discovered.payload_1;
  delivery_config.profile.payload_2 = discovered.payload_2;
  rb.delivery = std::make_shared<CommandIntentDelivery>(delivery_config);
  rb.delivery->set_outcome_callback([this, address = discovered.blind_address](const DeliveryOutcome &outcome) {
    if (outcome.event == DeliveryEvent::DROPPED) {
      ESP_LOGE(TAG, "Delivery retries exhausted for runtime blind 0x%06x", address);
      this->increment_tx_drop_count();
    } else if (outcome.event == DeliveryEvent::STALE_CLEARED) {
      ESP_LOGW(TAG, "Stale Command queue cleared for runtime blind 0x%06x", address);
      this->increment_tx_drop_count();
    }
  });
  if (!this->register_command_delivery(rb.delivery.get()))
    return false;
  const std::string adopted_name = rb.name;
  this->runtime_blinds_.insert({discovered.blind_address, std::move(rb)});
  this->own_remote_addresses_.insert(discovered.remote_address);
  ESP_LOGI(TAG, "Adopted runtime %s 0x%06x as \"%s\"",
           type == DeviceType::LIGHT ? "light" : "blind",
           discovered.blind_address, adopted_name.c_str());
  return true;
}

bool Elero::remove_runtime_blind(uint32_t addr) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  auto it = this->runtime_blinds_.find(addr);
  if (it != this->runtime_blinds_.end()) {
    ESP_LOGI(TAG, "Removed runtime blind 0x%06x", addr);
    this->unregister_command_delivery(it->second.delivery.get());
    this->runtime_blinds_.erase(it);
    return true;
  }
  return false;
}

IntentSubmitResult Elero::send_runtime_command(uint32_t addr, const CommandIntent &intent) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  auto it = this->runtime_blinds_.find(addr);
  if (it == this->runtime_blinds_.end() || !it->second.delivery)
    return IntentSubmitResult::REJECTED;
  return it->second.delivery->submit(intent, millis());
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
