#include "elero.h"
#include "elero_crypto.h"
#include "elero_utils.h"
#include "elero_packet_validation.h"
#include "elero_packet_parser.h"
#include "elero_counter_logic.h"
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
  auto parsed = packet_parser::parse_fifo_packet(this->msg_rx_);
  const auto &packet = parsed.packet;

  if (!parsed.ok) {
    uint8_t length = packet.length;
    const char *reason = parsed.reject_reason;
    if (strcmp(reason, "too_long") == 0) {
      uint8_t dump_len = (length <= (uint8_t)(CC1101_FIFO_LENGTH - 3)) ? (length + 3) : CC1101_FIFO_LENGTH;
      ESP_LOGE(TAG, "Received invalid packet: too long (%d)", length);
      ESP_LOGD(TAG, "  Raw [%d bytes]: %s", dump_len,
               format_hex_pretty(this->msg_rx_, dump_len).c_str());
    } else if (strcmp(reason, "too_short") == 0) {
      ESP_LOGD(TAG, "Received non-Elero packet: too short (%d bytes)", length);
    } else if (strcmp(reason, "zero_dests") == 0 || strcmp(reason, "too_many_dests") == 0) {
      ESP_LOGW(TAG, "Received invalid packet: invalid destination count (%d)", packet.num_dests);
      ESP_LOGW(TAG, "  Raw [%d bytes]: %s", length + 3,
               format_hex_pretty(this->msg_rx_, length + 3).c_str());
    } else if (strcmp(reason, "dests_len_too_long") == 0) {
      ESP_LOGW(TAG, "Received invalid packet: dests_len too long (%d) for length %d", packet.dests_len, length);
      ESP_LOGW(TAG, "  Raw [%d bytes]: %s", length + 3,
               format_hex_pretty(this->msg_rx_, length + 3).c_str());
    } else if (strcmp(reason, "rssi_oob") == 0) {
      ESP_LOGW(TAG, "Received invalid packet: RSSI/LQI out of buffer bounds (length=%d)", length);
      ESP_LOGW(TAG, "  Raw [%d bytes]: %s", CC1101_FIFO_LENGTH,
               format_hex_pretty(this->msg_rx_, CC1101_FIFO_LENGTH).c_str());
    } else if (strcmp(reason, "bad_crc") == 0) {
      ESP_LOGV(TAG, "Received packet with bad CRC, dropping");
    }

    this->increment_parser_drop_count(reason);

    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(false, reason);
      this->packet_dump_pending_update_ = false;
    }
    return;
  }

  if (packet.is_status && this->is_duplicate_packet_(packet.src, packet.cnt)) {
    if (this->packet_dump_pending_update_) {
      this->mark_last_raw_packet_(true, nullptr);
      this->packet_dump_pending_update_ = false;
    }
    ESP_LOGV(TAG, "Duplicate status from 0x%06x cnt=%d (relay hop), skipping", packet.src, packet.cnt);
    return;
  }

  if (packet.is_status) {
    const uint32_t now_ms = millis();
    auto counter_it = this->last_seen_counter_.find(packet.src);
    if (counter_it != this->last_seen_counter_.end() &&
        counter_logic::is_stale_counter(counter_it->second, packet.cnt)) {
      auto counter_ms_it = this->last_seen_counter_ms_.find(packet.src);
      const bool can_resync = counter_ms_it == this->last_seen_counter_ms_.end() ||
                              counter_logic::should_resync_counter(counter_ms_it->second, now_ms);
      if (!can_resync) {
        ESP_LOGV(TAG, "Stale status counter from 0x%06x cnt=%d last=%d, dropping",
                 packet.src, packet.cnt, counter_it->second);
        this->increment_parser_drop_count("stale_counter");
        if (this->packet_dump_pending_update_) {
          this->mark_last_raw_packet_(false, "stale_counter");
          this->packet_dump_pending_update_ = false;
        }
        return;
      }
      ESP_LOGD(TAG, "Resyncing status counter from 0x%06x cnt=%d last=%d after %lums gap",
               packet.src, packet.cnt, counter_it->second,
               static_cast<unsigned long>(counter_ms_it == this->last_seen_counter_ms_.end()
                                             ? 0u
                                             : static_cast<uint32_t>(now_ms - counter_ms_it->second)));
    }
    this->last_seen_counter_[packet.src] = packet.cnt;
    this->last_seen_counter_ms_[packet.src] = now_ms;
  }

  if (this->packet_dump_pending_update_) {
    this->mark_last_raw_packet_(true, nullptr);
    this->packet_dump_pending_update_ = false;
  }

  this->rx_count_.fetch_add(1, std::memory_order_relaxed);
  ESP_LOGD(TAG, "rcv'd from 0x%06x: state=0x%02x rssi=%.1f", packet.src, packet.payload[6], packet.rssi);
  ESP_LOGV(TAG, "rcv'd: len=%02d, cnt=%02d, typ=0x%02x, typ2=0x%02x, hop=0x%02x, syst=0x%02x, chl=%02d, src=0x%06x, bwd=0x%06x, fwd=0x%06x, #dst=%02d, dst=0x%06x, rssi=%2.1f, lqi=%2d, crc=%2d, payload=[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]", packet.length, packet.cnt, packet.typ, packet.typ2, packet.hop, packet.syst, packet.channel, packet.src, packet.bwd, packet.fwd, packet.num_dests, packet.first_dst, packet.rssi, packet.lqi, packet.crc, packet.payload_1, packet.payload_2, packet.payload[0], packet.payload[1], packet.payload[2], packet.payload[3], packet.payload[4], packet.payload[5], packet.payload[6], packet.payload[7]);

  RxResult rx{};
  rx.blind_address = packet.src;
  rx.remote_address = packet.is_status ? packet.fwd : packet.src;
  rx.channel = packet.channel;
  rx.pck_inf[0] = packet.typ;
  rx.pck_inf[1] = packet.typ2;
  rx.hop = packet.hop;
  rx.state = packet.payload[6];
  rx.rssi = packet.rssi;
  rx.timestamp_ms = millis();
  memcpy(rx.payload, packet.payload, sizeof(rx.payload));
  rx.cnt = packet.cnt;
  rx.is_status = packet.is_status;
  rx.is_command = packet.is_command;
  rx.payload_1 = packet.payload_1;
  rx.payload_2 = packet.payload_2;

  rx.scan_hit = this->scan_mode_.load(std::memory_order_acquire) && (packet.is_status || packet.is_command);
  rx.params_from_command = packet.is_command;

  rx.num_dests = 0;
  rx.is_own_echo = false;
  if (packet.is_command) {
    rx.is_own_echo = (this->own_remote_addresses_.count(packet.src) > 0);
    rx.num_dests = (packet.num_dests > packet_parser::MAX_DESTS) ? packet_parser::MAX_DESTS : packet.num_dests;
    for (uint8_t i = 0; i < rx.num_dests; i++) {
      rx.dest_addrs[i] = packet.dest_addrs[i];
    }
  }

  if (this->rx_queue_) {
    if (xQueueSend(this->rx_queue_, &rx, 0) != pdTRUE) {
      ESP_LOGW(TAG, "RX queue full, dropping packet from 0x%06x", packet.src);
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

  this->observe_dispatch_latency_(rx.timestamp_ms);
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
