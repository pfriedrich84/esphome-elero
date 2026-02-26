#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

namespace esphome {
namespace elero {

// Event types for persistent log entries
enum EleroEventType : uint8_t {
  EVENT_RF_RECEIVED    = 0x01,  // RF packet decoded from a blind
  EVENT_STATE_CHANGE   = 0x02,  // Cover state transition (old -> new)
  EVENT_COMMAND_SENT   = 0x03,  // Command transmitted to a blind
  EVENT_SYSTEM         = 0x04,  // System event (boot, error, scan)
};

// 64-byte fixed-size log entry
struct __attribute__((packed)) PersistentLogEntry {
  uint32_t sequence;         // Monotonic sequence number
  uint32_t timestamp_ms;     // millis() at event time
  uint8_t  event_type;       // EleroEventType
  uint32_t blind_address;    // 3-byte blind address (0 for system events)
  uint8_t  data1;            // Type-specific: old_state / state_byte / cmd_byte
  uint8_t  data2;            // Type-specific: new_state / 0 / 0
  int8_t   rssi;             // RSSI in dBm (0 if N/A)
  uint8_t  operation;        // Cover operation enum (0=idle, 1=opening, 2=closing)
  uint16_t position_x100;    // Position * 100 (0-10000), 0xFFFF if unknown
  char     message[45];      // Human-readable summary, null-terminated
};
// sizeof = 4+4+1+4+1+1+1+1+2+45 = 64 bytes (packed, no padding)
static_assert(sizeof(PersistentLogEntry) == 64, "PersistentLogEntry must be 64 bytes");

// File header (64 bytes, same as one entry for alignment)
struct __attribute__((packed)) PersistentLogHeader {
  uint32_t magic;            // 0x454C4F47 ("ELOG")
  uint16_t version;          // Format version (1)
  uint16_t max_entries;      // Max entries in ring buffer
  uint32_t write_idx;        // Next write index (slot in ring buffer)
  uint32_t total_written;    // Total entries ever written (monotonic)
  uint8_t  reserved[48];     // Future use, zero-filled
};
static_assert(sizeof(PersistentLogHeader) == 64, "PersistentLogHeader must be 64 bytes");

class EleroEventLog {
 public:
  bool begin(uint16_t max_entries);
  void append(const PersistentLogEntry &entry);
  std::vector<PersistentLogEntry> read_all() const;
  std::vector<PersistentLogEntry> read_since(uint32_t since_seq) const;
  void clear();
  uint32_t get_total_written() const { return header_.total_written; }
  uint16_t get_max_entries() const { return header_.max_entries; }
  uint16_t get_entry_count() const;
  bool is_ready() const { return ready_; }

  // Convenience logging methods
  void log_rf_received(uint32_t blind_addr, uint8_t state_byte, int8_t rssi);
  void log_state_change(uint32_t blind_addr, uint8_t old_state, uint8_t new_state,
                        uint8_t operation, float position);
  void log_command_sent(uint32_t blind_addr, uint8_t cmd_byte);
  void log_system(const char *message);

 private:
  void write_header_();
  void read_header_();
  PersistentLogEntry read_entry_(uint16_t idx) const;
  void write_entry_(uint16_t idx, const PersistentLogEntry &entry);
  void create_file_();

  FILE *file_{nullptr};
  PersistentLogHeader header_{};
  uint32_t next_sequence_{1};
  uint32_t append_count_{0};  // Count appends for header flush batching
  bool ready_{false};
  static constexpr const char *LOG_PATH = "/spiffs/elero_events.bin";
  static constexpr uint32_t MAGIC = 0x454C4F47;  // "ELOG"
  static constexpr uint16_t FORMAT_VERSION = 1;
  static constexpr uint32_t HEADER_FLUSH_INTERVAL = 10;  // Flush header every N appends
};

}  // namespace elero
}  // namespace esphome
