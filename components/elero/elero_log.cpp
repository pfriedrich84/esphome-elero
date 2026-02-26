#include "elero_log.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cstring>
#include <algorithm>

#ifdef USE_ESP32
#include "esp_spiffs.h"
#endif

namespace esphome {
namespace elero {

static const char *const TAG = "elero.log";

bool EleroEventLog::begin(uint16_t max_entries) {
#ifdef USE_ESP32
  // Mount SPIFFS if not already mounted
  esp_vfs_spiffs_conf_t spiffs_conf = {
      .base_path = "/spiffs",
      .partition_label = nullptr,
      .max_files = 2,
      .format_if_mount_failed = true,
  };

  esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
  if (ret == ESP_ERR_INVALID_STATE) {
    // Already mounted — that's fine
    ESP_LOGD(TAG, "SPIFFS already mounted");
  } else if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(ret));
    return false;
  } else {
    ESP_LOGD(TAG, "SPIFFS mounted successfully");
  }

  // Try to open existing file
  this->file_ = fopen(LOG_PATH, "r+b");
  if (this->file_ != nullptr) {
    // File exists — read and validate header
    this->read_header_();
    if (this->header_.magic != MAGIC || this->header_.version != FORMAT_VERSION ||
        this->header_.max_entries != max_entries) {
      ESP_LOGW(TAG, "Event log header invalid or max_entries changed, recreating");
      fclose(this->file_);
      this->file_ = nullptr;
    } else {
      // Recover next_sequence from the highest sequence in existing entries
      this->next_sequence_ = this->header_.total_written + 1;
      // Validate write_idx is within bounds
      if (this->header_.write_idx >= this->header_.max_entries) {
        this->header_.write_idx = 0;
      }
      ESP_LOGI(TAG, "Event log opened: %u entries, %u total written",
               this->get_entry_count(), this->header_.total_written);
      this->ready_ = true;
      return true;
    }
  }

  // Create new file
  this->header_.magic = MAGIC;
  this->header_.version = FORMAT_VERSION;
  this->header_.max_entries = max_entries;
  this->header_.write_idx = 0;
  this->header_.total_written = 0;
  memset(this->header_.reserved, 0, sizeof(this->header_.reserved));

  this->create_file_();
  if (this->file_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event log file");
    return false;
  }

  this->next_sequence_ = 1;
  this->ready_ = true;
  ESP_LOGI(TAG, "Event log created: max %u entries (%" PRIu32 " bytes)",
           max_entries, (uint32_t)(64 + max_entries * 64));
  return true;
#else
  ESP_LOGW(TAG, "Persistent logging only supported on ESP32");
  return false;
#endif
}

void EleroEventLog::create_file_() {
  this->file_ = fopen(LOG_PATH, "w+b");
  if (this->file_ == nullptr)
    return;

  // Write header
  this->write_header_();

  // Pre-allocate file with zero entries
  PersistentLogEntry empty{};
  memset(&empty, 0, sizeof(empty));
  for (uint16_t i = 0; i < this->header_.max_entries; i++) {
    fwrite(&empty, sizeof(PersistentLogEntry), 1, this->file_);
  }
  fflush(this->file_);
}

void EleroEventLog::write_header_() {
  if (this->file_ == nullptr)
    return;
  fseek(this->file_, 0, SEEK_SET);
  fwrite(&this->header_, sizeof(PersistentLogHeader), 1, this->file_);
  fflush(this->file_);
}

void EleroEventLog::read_header_() {
  if (this->file_ == nullptr)
    return;
  fseek(this->file_, 0, SEEK_SET);
  fread(&this->header_, sizeof(PersistentLogHeader), 1, this->file_);
}

void EleroEventLog::write_entry_(uint16_t idx, const PersistentLogEntry &entry) {
  if (this->file_ == nullptr)
    return;
  long offset = sizeof(PersistentLogHeader) + (long)idx * sizeof(PersistentLogEntry);
  fseek(this->file_, offset, SEEK_SET);
  fwrite(&entry, sizeof(PersistentLogEntry), 1, this->file_);
  fflush(this->file_);
}

PersistentLogEntry EleroEventLog::read_entry_(uint16_t idx) const {
  PersistentLogEntry entry{};
  if (this->file_ == nullptr)
    return entry;
  long offset = sizeof(PersistentLogHeader) + (long)idx * sizeof(PersistentLogEntry);
  fseek(this->file_, offset, SEEK_SET);
  fread(&entry, sizeof(PersistentLogEntry), 1, this->file_);
  return entry;
}

void EleroEventLog::append(const PersistentLogEntry &entry) {
  if (!this->ready_)
    return;

  PersistentLogEntry e = entry;
  e.sequence = this->next_sequence_++;
  e.timestamp_ms = millis();

  this->write_entry_(this->header_.write_idx, e);

  this->header_.write_idx = (this->header_.write_idx + 1) % this->header_.max_entries;
  this->header_.total_written++;

  // Batch header flushes to reduce flash wear
  this->append_count_++;
  if (this->append_count_ >= HEADER_FLUSH_INTERVAL) {
    this->write_header_();
    this->append_count_ = 0;
  }
}

uint16_t EleroEventLog::get_entry_count() const {
  if (this->header_.total_written >= this->header_.max_entries)
    return this->header_.max_entries;
  return static_cast<uint16_t>(this->header_.total_written);
}

std::vector<PersistentLogEntry> EleroEventLog::read_all() const {
  std::vector<PersistentLogEntry> result;
  if (!this->ready_)
    return result;

  uint16_t count = this->get_entry_count();
  result.reserve(count);

  if (this->header_.total_written <= this->header_.max_entries) {
    // Buffer hasn't wrapped — read from 0 to write_idx
    for (uint16_t i = 0; i < count; i++) {
      result.push_back(this->read_entry_(i));
    }
  } else {
    // Buffer has wrapped — oldest entry is at write_idx
    for (uint16_t i = 0; i < count; i++) {
      uint16_t idx = (this->header_.write_idx + i) % this->header_.max_entries;
      result.push_back(this->read_entry_(idx));
    }
  }
  return result;
}

std::vector<PersistentLogEntry> EleroEventLog::read_since(uint32_t since_seq) const {
  std::vector<PersistentLogEntry> all = this->read_all();
  std::vector<PersistentLogEntry> result;
  for (const auto &entry : all) {
    if (entry.sequence > since_seq) {
      result.push_back(entry);
    }
  }
  return result;
}

void EleroEventLog::clear() {
  if (!this->ready_)
    return;

  this->header_.write_idx = 0;
  this->header_.total_written = 0;
  this->next_sequence_ = 1;
  this->append_count_ = 0;
  this->write_header_();

  // Zero out all entries
  PersistentLogEntry empty{};
  memset(&empty, 0, sizeof(empty));
  for (uint16_t i = 0; i < this->header_.max_entries; i++) {
    this->write_entry_(i, empty);
  }

  ESP_LOGI(TAG, "Event log cleared");
}

// ─── Convenience logging methods ──────────────────────────────────────────

void EleroEventLog::log_rf_received(uint32_t blind_addr, uint8_t state_byte, int8_t rssi) {
  PersistentLogEntry entry{};
  entry.event_type = EVENT_RF_RECEIVED;
  entry.blind_address = blind_addr;
  entry.data1 = state_byte;
  entry.data2 = 0;
  entry.rssi = rssi;
  entry.operation = 0;
  entry.position_x100 = 0xFFFF;
  snprintf(entry.message, sizeof(entry.message), "0x%06x: rx state=0x%02x rssi=%d",
           blind_addr, state_byte, rssi);
  this->append(entry);
}

void EleroEventLog::log_state_change(uint32_t blind_addr, uint8_t old_state, uint8_t new_state,
                                     uint8_t operation, float position) {
  PersistentLogEntry entry{};
  entry.event_type = EVENT_STATE_CHANGE;
  entry.blind_address = blind_addr;
  entry.data1 = old_state;
  entry.data2 = new_state;
  entry.rssi = 0;
  entry.operation = operation;
  entry.position_x100 = (position >= 0.0f && position <= 1.0f)
                             ? static_cast<uint16_t>(position * 10000.0f)
                             : 0xFFFF;
  snprintf(entry.message, sizeof(entry.message), "0x%06x: 0x%02x->0x%02x op=%u pos=%u%%",
           blind_addr, old_state, new_state, operation,
           entry.position_x100 != 0xFFFF ? (unsigned)(entry.position_x100 / 100) : 0);
  this->append(entry);
}

void EleroEventLog::log_command_sent(uint32_t blind_addr, uint8_t cmd_byte) {
  PersistentLogEntry entry{};
  entry.event_type = EVENT_COMMAND_SENT;
  entry.blind_address = blind_addr;
  entry.data1 = cmd_byte;
  entry.data2 = 0;
  entry.rssi = 0;
  entry.operation = 0;
  entry.position_x100 = 0xFFFF;
  snprintf(entry.message, sizeof(entry.message), "0x%06x: tx cmd=0x%02x",
           blind_addr, cmd_byte);
  this->append(entry);
}

void EleroEventLog::log_system(const char *message) {
  PersistentLogEntry entry{};
  entry.event_type = EVENT_SYSTEM;
  entry.blind_address = 0;
  entry.data1 = 0;
  entry.data2 = 0;
  entry.rssi = 0;
  entry.operation = 0;
  entry.position_x100 = 0xFFFF;
  strncpy(entry.message, message, sizeof(entry.message) - 1);
  entry.message[sizeof(entry.message) - 1] = '\0';
  this->append(entry);
}

}  // namespace elero
}  // namespace esphome
