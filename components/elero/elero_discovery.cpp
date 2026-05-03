#include "elero.h"
#include "elero_utils.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome {
namespace elero {

static const char *TAG = "elero";

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


}  // namespace elero
}  // namespace esphome
