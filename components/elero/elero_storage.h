#pragma once

#include "esphome/core/defines.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <map>
#include <string>

namespace esphome {
namespace elero {

// Forward declarations (defined in elero.h)
struct RuntimeBlind;
struct RawPacket;

static constexpr uint32_t STORAGE_MAGIC_BLINDS  = 0x454C4252;  // "ELBR"
static constexpr uint32_t STORAGE_MAGIC_PACKETS  = 0x454C504B;  // "ELPK"
static constexpr uint32_t STORAGE_MAGIC_STATES   = 0x454C5354;  // "ELST"
static constexpr uint16_t STORAGE_VERSION_1      = 1;

/// State snapshot persisted per blind address.
struct PersistedBlindState {
  uint32_t blind_address;
  uint8_t  last_state;
  float    last_rssi;
  uint32_t timestamp_ms;
};

/// Fixed-size record for serializing RuntimeBlind to flash.
struct RuntimeBlindRecord {
  uint32_t blind_address;
  uint32_t remote_address;
  uint8_t  channel;
  uint8_t  pck_inf[2];
  uint8_t  hop;
  uint8_t  payload_1;
  uint8_t  payload_2;
  char     name[32];
  uint32_t open_duration_ms;
  uint32_t close_duration_ms;
  uint32_t poll_intvl_ms;
  uint8_t  last_state;
  uint8_t  cmd_counter;
};

class EleroStorage {
 public:
  /// Mount LittleFS on the flash partition. Call once during setup().
  bool begin();
  bool is_mounted() const { return mounted_; }

  // Runtime adopted blinds — saved on adopt/remove/settings change
  bool save_runtime_blinds(const std::map<uint32_t, RuntimeBlind> &blinds);
  bool load_runtime_blinds(std::map<uint32_t, RuntimeBlind> &blinds);

  // RF packet log — saved/loaded on demand (not every packet, to reduce flash wear)
  bool save_packets(const std::vector<RawPacket> &packets, uint8_t write_idx);
  bool load_packets(std::vector<RawPacket> &packets, uint8_t &write_idx);

  // Blind state snapshots — saved periodically and on significant state changes
  bool save_blind_states(const std::vector<PersistedBlindState> &states);
  bool load_blind_states(std::vector<PersistedBlindState> &states);

 private:
  bool mounted_{false};
};

}  // namespace elero
}  // namespace esphome
