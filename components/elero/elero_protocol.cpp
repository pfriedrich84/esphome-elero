#include "elero.h"
#include "elero_crypto.h"
#include "elero_utils.h"
#include "elero_packet_validation.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace elero {

static const char *TAG = "elero";

// ---------------------------------------------------------------------------
// interpret_msg — parse CC1101 FIFO, extract header/payload, decode
// ---------------------------------------------------------------------------
void Elero::interpret_msg() {
  uint8_t length = this->msg_rx_[0];
  // Sanity check
  if(length > ELERO_MAX_PACKET_SIZE) {
    uint8_t dump_len = (length <= (uint8_t)(CC1101_FIFO_LENGTH - 3)) ? (length + 3) : CC1101_FIFO_LENGTH;
    ESP_LOGE(TAG, "Received invalid packet: too long (%d)", length);
    ESP_LOGD(TAG, "  Raw [%d bytes]: %s", dump_len,
             format_hex_pretty(this->msg_rx_, dump_len).c_str());
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "too_long");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  static const uint8_t ELERO_MIN_PACKET_SIZE = 17;
  if (length < ELERO_MIN_PACKET_SIZE) {
    ESP_LOGD(TAG, "Received non-Elero packet: too short (%d bytes)", length);
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "too_short");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  uint8_t cnt = this->msg_rx_[1];
  uint8_t typ = this->msg_rx_[2];
  uint8_t typ2 = this->msg_rx_[3];
  uint8_t hop = this->msg_rx_[4];
  uint8_t syst = this->msg_rx_[5];
  uint8_t chl = this->msg_rx_[6];
  uint32_t src = ((uint32_t)this->msg_rx_[7] << 16) | ((uint32_t)this->msg_rx_[8] << 8) | (this->msg_rx_[9]);
  uint32_t bwd = ((uint32_t)this->msg_rx_[10] << 16) | ((uint32_t)this->msg_rx_[11] << 8) | (this->msg_rx_[12]);
  uint32_t fwd = ((uint32_t)this->msg_rx_[13] << 16) | ((uint32_t)this->msg_rx_[14] << 8) | (this->msg_rx_[15]);
  uint8_t num_dests = this->msg_rx_[16];
  uint32_t dst;
  uint8_t dests_len;

  static const uint8_t MAX_SAFE_DESTS = (ELERO_MAX_PACKET_SIZE - 27) / 3;
  if (num_dests == 0 || num_dests > MAX_SAFE_DESTS) {
    ESP_LOGW(TAG, "Received invalid packet: invalid destination count (%d)", num_dests);
    ESP_LOGW(TAG, "  Raw [%d bytes]: %s", length + 3,
             format_hex_pretty(this->msg_rx_, length + 3).c_str());
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, (num_dests == 0) ? "zero_dests" : "too_many_dests");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  if(typ > 0x60) {
    dests_len = num_dests * 3;
    dst = ((uint32_t)this->msg_rx_[17] << 16) | ((uint32_t)this->msg_rx_[18] << 8) | (this->msg_rx_[19]);
  } else {
    dests_len = num_dests;
    dst = this->msg_rx_[17];
  }

  if((uint16_t)(26 + dests_len) > length || (uint16_t)(26 + dests_len) >= CC1101_FIFO_LENGTH) {
    ESP_LOGW(TAG, "Received invalid packet: dests_len too long (%d) for length %d", dests_len, length);
    ESP_LOGW(TAG, "  Raw [%d bytes]: %s", length + 3,
             format_hex_pretty(this->msg_rx_, length + 3).c_str());
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "dests_len_too_long");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  if ((uint16_t)(length + 2) >= CC1101_FIFO_LENGTH) {
    ESP_LOGW(TAG, "Received invalid packet: RSSI/LQI out of buffer bounds (length=%d)", length);
    ESP_LOGW(TAG, "  Raw [%d bytes]: %s", CC1101_FIFO_LENGTH,
             format_hex_pretty(this->msg_rx_, CC1101_FIFO_LENGTH).c_str());
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "rssi_oob");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  uint8_t payload1 = this->msg_rx_[17 + dests_len];
  uint8_t payload2 = this->msg_rx_[18 + dests_len];
  uint8_t status_byte = this->msg_rx_[length + 2];
  uint8_t crc = packet_validation::extract_crc(status_byte);
  uint8_t lqi = packet_validation::extract_lqi(status_byte);

  if (!packet_validation::is_crc_valid_status_byte(status_byte)) {
    ESP_LOGV(TAG, "Received packet with bad CRC, dropping");
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, "bad_crc");
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  float rssi = utils::calculate_rssi(this->msg_rx_[length + 1]);
  uint8_t *payload = &this->msg_rx_[19 + dests_len];
  crypto::msg_decode(payload);
  if (this->packet_dump_pending_update_) {
    this->mark_last_raw_packet_(true, nullptr);
    this->packet_dump_pending_update_ = false;
  }
  this->rx_count_.fetch_add(1, std::memory_order_relaxed);
  ESP_LOGD(TAG, "rcv'd from 0x%06x: state=0x%02x rssi=%.1f", src, payload[6], rssi);
  ESP_LOGV(TAG, "rcv'd: len=%02d, cnt=%02d, typ=0x%02x, typ2=0x%02x, hop=0x%02x, syst=0x%02x, chl=%02d, src=0x%06x, bwd=0x%06x, fwd=0x%06x, #dst=%02d, dst=0x%06x, rssi=%2.1f, lqi=%2d, crc=%2d, payload=[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]", length, cnt, typ, typ2, hop, syst, chl, src, bwd, fwd, num_dests, dst, rssi, lqi, crc, payload1, payload2, payload[0], payload[1], payload[2], payload[3], payload[4], payload[5], payload[6], payload[7]);

  // Build RxResult and push to queue for Core 1 dispatch.
  bool is_status = (typ == 0xca) || (typ == 0xc9);
  bool is_command = (typ == 0x6a) || (typ == 0x69);

  if (is_status && this->is_duplicate_packet_(src, cnt)) {
    ESP_LOGV(TAG, "Duplicate status from 0x%06x cnt=%d (relay hop), skipping", src, cnt);
    return;
  }

  RxResult rx{};
  rx.blind_address = src;
  rx.remote_address = is_status ? fwd : src;
  rx.channel = chl;
  rx.pck_inf[0] = typ;
  rx.pck_inf[1] = typ2;
  rx.hop = hop;
  rx.state = payload[6];
  rx.rssi = rssi;
  rx.timestamp_ms = millis();
  memcpy(rx.payload, payload, 10);
  rx.cnt = cnt;
  rx.is_status = is_status;
  rx.is_command = is_command;
  rx.payload_1 = payload1;
  rx.payload_2 = payload2;

  rx.scan_hit = this->scan_mode_.load(std::memory_order_acquire) && (is_status || is_command);
  rx.params_from_command = is_command;

  rx.num_dests = 0;
  rx.is_own_echo = false;
  if (is_command) {
    rx.is_own_echo = (this->own_remote_addresses_.count(src) > 0);
    uint8_t safe_num = (num_dests > 10) ? 10 : num_dests;
    rx.num_dests = safe_num;
    for (uint8_t i = 0; i < safe_num; i++) {
      if (typ > 0x60) {
        rx.dest_addrs[i] = ((uint32_t)this->msg_rx_[17 + i * 3] << 16) |
                            ((uint32_t)this->msg_rx_[18 + i * 3] << 8) |
                            this->msg_rx_[19 + i * 3];
      } else {
        rx.dest_addrs[i] = this->msg_rx_[17 + i];
      }
    }
  }

  if (this->rx_queue_) {
    if (xQueueSend(this->rx_queue_, &rx, 0) != pdTRUE) {
      ESP_LOGW(TAG, "RX queue full, dropping packet from 0x%06x", src);
    }
  }
}

// ---------------------------------------------------------------------------
// dispatch_rx_result_ — route decoded RX result to entities/sensors/discovery
// ---------------------------------------------------------------------------
void Elero::dispatch_rx_result_(const RxResult &rx) {
  // 1. RSSI sensor update
#ifdef USE_SENSOR
  {
    auto rssi_it = this->address_to_rssi_sensor_.find(rx.blind_address);
    if (rssi_it != this->address_to_rssi_sensor_.end()) {
      rssi_it->second->publish_state(rx.rssi);
    }
  }
#endif

  // 2. Discovery tracking (scan mode)
  if (rx.scan_hit) {
    this->track_discovered_blind(rx.blind_address, rx.remote_address, rx.channel,
                                  rx.pck_inf[0], rx.pck_inf[1], rx.hop,
                                  rx.payload_1, rx.payload_2,
                                  rx.rssi, rx.state, rx.params_from_command);
    if (rx.is_command) {
      for (uint8_t i = 0; i < rx.num_dests; i++) {
        this->track_discovered_blind(rx.dest_addrs[i], rx.blind_address, rx.channel,
                                      rx.pck_inf[0], rx.pck_inf[1], rx.hop,
                                      rx.payload_1, rx.payload_2,
                                      rx.rssi, 0, true);
      }
    }
  }

  // 3. Status packets (0xca/0xc9): dispatch state to entities
  if (rx.is_status) {

#ifdef USE_TEXT_SENSOR
    {
      auto text_it = this->address_to_text_sensor_.find(rx.blind_address);
      if (text_it != this->address_to_text_sensor_.end()) {
        text_it->second->publish_state(elero_state_to_string(rx.state));
      }
    }
#endif

    auto search = this->address_to_cover_mapping_.find(rx.blind_address);
    if (search != this->address_to_cover_mapping_.end()) {
      search->second->notify_rx_meta(rx.timestamp_ms, rx.rssi);
      search->second->set_rx_state(rx.state);
    }

    auto light_search = this->address_to_light_mapping_.find(rx.blind_address);
    if (light_search != this->address_to_light_mapping_.end()) {
      light_search->second->notify_rx_meta(rx.timestamp_ms, rx.rssi);
      light_search->second->set_rx_state(rx.state);
    }

    // Update runtime adopted blinds
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      auto it = this->runtime_blinds_.find(rx.blind_address);
      if (it != this->runtime_blinds_.end()) {
        it->second.last_seen_ms = rx.timestamp_ms;
        it->second.last_rssi = rx.rssi;
        it->second.last_state = rx.state;
        this->update_runtime_blind_direction_(it->second, rx.state);
      }
    }
  } else {
    // Non-status packets: still update RSSI/last_seen for known blinds
    auto search = this->address_to_cover_mapping_.find(rx.blind_address);
    if (search != this->address_to_cover_mapping_.end()) {
      search->second->notify_rx_meta(rx.timestamp_ms, rx.rssi);
    }
    auto light_search = this->address_to_light_mapping_.find(rx.blind_address);
    if (light_search != this->address_to_light_mapping_.end()) {
      light_search->second->notify_rx_meta(rx.timestamp_ms, rx.rssi);
    }
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      auto rb_it = this->runtime_blinds_.find(rx.blind_address);
      if (rb_it != this->runtime_blinds_.end()) {
        rb_it->second.last_seen_ms = rx.timestamp_ms;
        rb_it->second.last_rssi = rx.rssi;
      }
    }

    // Remote command packets: trigger immediate polls
    if (rx.is_command && !rx.is_own_echo) {
      for (uint8_t i = 0; i < rx.num_dests; i++) {
        auto c_it = this->address_to_cover_mapping_.find(rx.dest_addrs[i]);
        if (c_it != this->address_to_cover_mapping_.end()) {
          c_it->second->schedule_immediate_poll();
        }
        auto l_it = this->address_to_light_mapping_.find(rx.dest_addrs[i]);
        if (l_it != this->address_to_light_mapping_.end()) {
          l_it->second->schedule_immediate_poll();
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Registration methods
// ---------------------------------------------------------------------------
void Elero::register_cover(EleroBlindBase *cover) {
  uint32_t address = cover->get_blind_address();
  if(this->address_to_cover_mapping_.find(address) != this->address_to_cover_mapping_.end()) {
    ESP_LOGE(TAG, "A blind with this address is already registered - this is currently not supported");
    return;
  }
  this->address_to_cover_mapping_.insert({address, cover});
  this->own_remote_addresses_.insert(cover->get_remote_address());
  cover->set_poll_offset((this->address_to_cover_mapping_.size() - 1) * ELERO_POLL_STAGGER_MS);
}

void Elero::register_light(EleroLightBase *light) {
  uint32_t address = light->get_blind_address();
  if(this->address_to_light_mapping_.find(address) != this->address_to_light_mapping_.end()) {
    ESP_LOGE(TAG, "A light with this address is already registered - this is currently not supported");
    return;
  }
  this->address_to_light_mapping_.insert({address, light});
  this->own_remote_addresses_.insert(light->get_remote_address());
}

#ifdef USE_SENSOR
void Elero::register_rssi_sensor(uint32_t address, sensor::Sensor *sensor) {
  this->address_to_rssi_sensor_[address] = sensor;
}
#endif

#ifdef USE_TEXT_SENSOR
void Elero::register_text_sensor(uint32_t address, text_sensor::TextSensor *sensor) {
  this->address_to_text_sensor_[address] = sensor;
}

void Elero::publish_text_sensor_state(uint32_t address, const std::string &state) {
  auto it = this->address_to_text_sensor_.find(address);
  if (it != this->address_to_text_sensor_.end()) {
    it->second->publish_state(state);
  }
}
#endif

// ---------------------------------------------------------------------------
// Packet capture & discovery
// ---------------------------------------------------------------------------
void Elero::start_packet_dump() {
  packet_dump_mode_.store(true, std::memory_order_release);
  ESP_LOGI(TAG, "Packet dump mode started");
}

void Elero::stop_packet_dump() {
  packet_dump_mode_.store(false, std::memory_order_release);
  ESP_LOGI(TAG, "Packet dump mode stopped");
}

void Elero::clear_raw_packets() {
  std::lock_guard<std::mutex> lock(packet_dump_mutex_);
  raw_packets_.clear();
  raw_packet_write_idx_ = 0;
}

void Elero::capture_raw_packet_(uint8_t fifo_len) {
  uint8_t actual_len = (fifo_len > CC1101_FIFO_LENGTH) ? CC1101_FIFO_LENGTH : fifo_len;
  RawPacket pkt{};
  pkt.timestamp_ms = millis();
  pkt.fifo_len = actual_len;
  memcpy(pkt.data, this->msg_rx_, actual_len);
  pkt.valid = false;
  pkt.reject_reason[0] = '\0';

  std::lock_guard<std::mutex> lock(packet_dump_mutex_);
  if (raw_packets_.size() < ELERO_MAX_RAW_PACKETS) {
    raw_packets_.push_back(pkt);
    raw_packet_write_idx_ = (uint16_t)(raw_packets_.size() - 1);
  } else {
    raw_packet_write_idx_ = (raw_packet_write_idx_ + 1) % ELERO_MAX_RAW_PACKETS;
    raw_packets_[raw_packet_write_idx_] = pkt;
  }
}

void Elero::mark_last_raw_packet_(bool valid, const char *reason) {
  std::lock_guard<std::mutex> lock(packet_dump_mutex_);
  if (raw_packets_.empty()) return;
  auto &pkt = raw_packets_[raw_packet_write_idx_];
  pkt.valid = valid;
  if (!valid && reason != nullptr) {
    strncpy(pkt.reject_reason, reason, sizeof(pkt.reject_reason) - 1);
    pkt.reject_reason[sizeof(pkt.reject_reason) - 1] = '\0';
  }
}

void Elero::track_discovered_blind(uint32_t src, uint32_t remote, uint8_t channel,
                                    uint8_t pck_inf0, uint8_t pck_inf1, uint8_t hop,
                                    uint8_t payload_1, uint8_t payload_2,
                                    float rssi, uint8_t state, bool from_command) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  for (auto &blind : this->discovered_blinds_) {
    if (blind.blind_address == src) {
      blind.rssi = rssi;
      blind.last_seen = millis();
      if (state != 0) blind.last_state = state;
      blind.times_seen++;
      if (from_command && !blind.params_from_command) {
        blind.remote_address = remote;
        blind.channel = channel;
        blind.pck_inf[0] = pck_inf0;
        blind.pck_inf[1] = pck_inf1;
        blind.hop = hop;
        blind.payload_1 = payload_1;
        blind.payload_2 = payload_2;
        blind.params_from_command = true;
        ESP_LOGI(TAG, "Upgraded blind 0x%06x params from command packet: ch=%d, pck_inf=0x%02x/0x%02x, hop=0x%02x, payload=0x%02x/0x%02x",
                 src, channel, pck_inf0, pck_inf1, hop, payload_1, payload_2);
      }
      return;
    }
  }
  if (this->discovered_blinds_.size() < ELERO_MAX_DISCOVERED) {
    DiscoveredBlind blind{};
    blind.blind_address = src;
    blind.remote_address = remote;
    blind.channel = channel;
    blind.pck_inf[0] = pck_inf0;
    blind.pck_inf[1] = pck_inf1;
    blind.hop = hop;
    blind.payload_1 = payload_1;
    blind.payload_2 = payload_2;
    blind.rssi = rssi;
    blind.last_seen = millis();
    blind.last_state = state;
    blind.times_seen = 1;
    blind.params_from_command = from_command;
    this->discovered_blinds_.push_back(blind);
    ESP_LOGI(TAG, "Discovered new device: addr=0x%06x, remote=0x%06x, ch=%d, rssi=%.1f, src=%s",
             src, remote, channel, rssi, from_command ? "cmd_pkt" : "status_pkt");
  }
}

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
      if ((now - rb.last_queue_drain_ms) > ELERO_COMMAND_QUEUE_MAX_AGE_MS) {
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
          if (rb.cmd_counter == 0xFF)
            rb.cmd_counter = 1;
          else
            rb.cmd_counter++;
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
    if (rb.poll_intvl_ms == 0 || rb.poll_intvl_ms == UINT32_MAX)
      continue;
    if (!rb.command_queue.empty())
      continue;
    if ((now - rb.last_poll_ms) >= rb.poll_intvl_ms) {
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
  switch (state) {
    case ELERO_STATE_START_MOVING_UP:
    case ELERO_STATE_MOVING_UP:
      rb.moving_direction = 1;
      break;
    case ELERO_STATE_START_MOVING_DOWN:
    case ELERO_STATE_MOVING_DOWN:
      rb.moving_direction = -1;
      break;
    case ELERO_STATE_TOP:
      rb.moving_direction = 0;
      rb.position = 1.0f;
      break;
    case ELERO_STATE_BOTTOM:
      rb.moving_direction = 0;
      rb.position = 0.0f;
      break;
    case ELERO_STATE_STOPPED:
    case ELERO_STATE_INTERMEDIATE:
    case ELERO_STATE_TILT:
    case ELERO_STATE_BLOCKING:
    case ELERO_STATE_OVERHEATED:
    case ELERO_STATE_TIMEOUT:
      rb.moving_direction = 0;
      break;
    default:
      break;
  }
  if (old_dir != rb.moving_direction) {
    rb.last_recompute_ms = millis();
  }
}

void Elero::recompute_runtime_positions_() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  uint32_t now = millis();
  for (auto &entry : this->runtime_blinds_) {
    auto &rb = entry.second;
    if (rb.moving_direction == 0)
      continue;
    if (rb.open_duration_ms == 0 || rb.close_duration_ms == 0)
      continue;
    if (rb.position < 0.0f)
      continue;

    uint32_t elapsed = now - rb.last_recompute_ms;
    if (elapsed == 0)
      continue;

    if (elapsed > ELERO_TIMEOUT_MOVEMENT) {
      rb.last_recompute_ms = now;
      continue;
    }

    float delta;
    if (rb.moving_direction > 0) {
      delta = static_cast<float>(elapsed) / static_cast<float>(rb.open_duration_ms);
      rb.position += delta;
    } else {
      delta = static_cast<float>(elapsed) / static_cast<float>(rb.close_duration_ms);
      rb.position -= delta;
    }
    if (rb.position > 1.0f) rb.position = 1.0f;
    if (rb.position < 0.0f) rb.position = 0.0f;
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

// ─── Log buffer ───────────────────────────────────────────────────────────

void Elero::append_log(uint8_t level, const char *tag, const char *fmt, ...) {
  if (!this->log_capture_) return;
  LogEntry entry{};
  entry.timestamp_ms = millis();
  entry.level = level;
  strncpy(entry.tag, tag, sizeof(entry.tag) - 1);
  va_list args;
  va_start(args, fmt);
  vsnprintf(entry.message, sizeof(entry.message), fmt, args);
  va_end(args);
  std::lock_guard<std::mutex> lock(this->log_mutex_);
  if (!this->log_buffer_full_ && this->log_entries_.size() < ELERO_LOG_BUFFER_SIZE) {
    this->log_entries_.push_back(entry);
  } else {
    this->log_buffer_full_ = true;
    this->log_entries_[this->log_write_idx_] = entry;
    this->log_write_idx_ = (this->log_write_idx_ + 1) % ELERO_LOG_BUFFER_SIZE;
  }
}

}  // namespace elero
}  // namespace esphome
