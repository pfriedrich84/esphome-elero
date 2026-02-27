#include "elero_storage.h"
#include "elero.h"
#include "esphome/core/log.h"

#ifdef USE_ARDUINO
#include <LittleFS.h>
#endif

#ifdef USE_ESP_IDF
#if __has_include("esp_littlefs.h")
#include "esp_littlefs.h"
#elif __has_include(<esp_littlefs.h>)
#include <esp_littlefs.h>
#else
#define ELERO_NO_ESP_LITTLEFS
#endif
#endif

namespace esphome {
namespace elero {

static const char *const ELERO_STORAGE_TAG = "elero.storage";

static const char *const BLINDS_PATH  = "/littlefs/elero_rt.bin";
static const char *const PACKETS_PATH = "/littlefs/elero_pkt.bin";
static const char *const STATES_PATH  = "/littlefs/elero_st.bin";

bool EleroStorage::begin() {
  if (mounted_)
    return true;

#ifdef USE_ARDUINO
  if (!LittleFS.begin(true)) {  // true = format partition on first use
    ESP_LOGE(ELERO_STORAGE_TAG, "Failed to mount LittleFS");
    return false;
  }
  mounted_ = true;
  ESP_LOGI(ELERO_STORAGE_TAG, "LittleFS mounted");
#endif

#ifdef USE_ESP_IDF
#ifdef ELERO_NO_ESP_LITTLEFS
  // esp_littlefs.h not available — assume the filesystem will be mounted
  // externally or try to use POSIX file I/O directly.
  ESP_LOGW(ELERO_STORAGE_TAG, "esp_littlefs.h not found; assuming LittleFS is already mounted");
  mounted_ = true;
#else
  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = "/littlefs";
  conf.partition_label = "spiffs";
  conf.max_files = 5;
  conf.format_if_mount_failed = true;

  esp_err_t ret = esp_vfs_littlefs_register(&conf);
  if (ret == ESP_ERR_INVALID_STATE) {
    ESP_LOGD(ELERO_STORAGE_TAG, "LittleFS already mounted");
    mounted_ = true;
  } else if (ret != ESP_OK) {
    ESP_LOGE(ELERO_STORAGE_TAG, "Failed to mount LittleFS: %s", esp_err_to_name(ret));
    return false;
  } else {
    mounted_ = true;
    ESP_LOGI(ELERO_STORAGE_TAG, "LittleFS mounted");
  }
#endif
#endif

  return mounted_;
}

// ─── Runtime blind persistence ────────────────────────────────────────────

bool EleroStorage::save_runtime_blinds(const std::map<uint32_t, RuntimeBlind> &blinds) {
  if (!mounted_) return false;

  FILE *f = fopen(BLINDS_PATH, "wb");
  if (!f) {
    ESP_LOGE(ELERO_STORAGE_TAG, "Failed to open %s for writing", BLINDS_PATH);
    return false;
  }

  uint32_t magic = STORAGE_MAGIC_BLINDS;
  uint16_t version = STORAGE_VERSION_1;
  uint16_t count = static_cast<uint16_t>(blinds.size());

  fwrite(&magic, sizeof(magic), 1, f);
  fwrite(&version, sizeof(version), 1, f);
  fwrite(&count, sizeof(count), 1, f);

  for (const auto &entry : blinds) {
    const auto &rb = entry.second;
    RuntimeBlindRecord rec{};
    rec.blind_address = rb.blind_address;
    rec.remote_address = rb.remote_address;
    rec.channel = rb.channel;
    rec.pck_inf[0] = rb.pck_inf[0];
    rec.pck_inf[1] = rb.pck_inf[1];
    rec.hop = rb.hop;
    rec.payload_1 = rb.payload_1;
    rec.payload_2 = rb.payload_2;
    strncpy(rec.name, rb.name.c_str(), sizeof(rec.name) - 1);
    rec.name[sizeof(rec.name) - 1] = '\0';
    rec.open_duration_ms = rb.open_duration_ms;
    rec.close_duration_ms = rb.close_duration_ms;
    rec.poll_intvl_ms = rb.poll_intvl_ms;
    rec.last_state = rb.last_state;
    rec.cmd_counter = rb.cmd_counter;
    fwrite(&rec, sizeof(rec), 1, f);
  }

  fclose(f);
  ESP_LOGD(ELERO_STORAGE_TAG, "Saved %d runtime blind(s) to flash", count);
  return true;
}

bool EleroStorage::load_runtime_blinds(std::map<uint32_t, RuntimeBlind> &blinds) {
  if (!mounted_) return false;

  FILE *f = fopen(BLINDS_PATH, "rb");
  if (!f) {
    ESP_LOGD(ELERO_STORAGE_TAG, "No runtime blinds file found (first boot)");
    return false;
  }

  uint32_t magic = 0;
  uint16_t version = 0;
  uint16_t count = 0;

  if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != STORAGE_MAGIC_BLINDS) {
    ESP_LOGW(ELERO_STORAGE_TAG, "Invalid runtime blinds file (bad magic)");
    fclose(f);
    return false;
  }
  if (fread(&version, sizeof(version), 1, f) != 1 || version != STORAGE_VERSION_1) {
    ESP_LOGW(ELERO_STORAGE_TAG, "Unsupported runtime blinds file version %d", version);
    fclose(f);
    return false;
  }
  if (fread(&count, sizeof(count), 1, f) != 1) {
    ESP_LOGW(ELERO_STORAGE_TAG, "Failed to read runtime blinds count");
    fclose(f);
    return false;
  }

  for (uint16_t i = 0; i < count; i++) {
    RuntimeBlindRecord rec{};
    if (fread(&rec, sizeof(rec), 1, f) != 1) {
      ESP_LOGW(ELERO_STORAGE_TAG, "Truncated runtime blinds file at record %d", i);
      break;
    }

    RuntimeBlind rb{};
    rb.blind_address = rec.blind_address;
    rb.remote_address = rec.remote_address;
    rb.channel = rec.channel;
    rb.pck_inf[0] = rec.pck_inf[0];
    rb.pck_inf[1] = rec.pck_inf[1];
    rb.hop = rec.hop;
    rb.payload_1 = rec.payload_1;
    rb.payload_2 = rec.payload_2;
    rb.name = rec.name;
    rb.open_duration_ms = rec.open_duration_ms;
    rb.close_duration_ms = rec.close_duration_ms;
    rb.poll_intvl_ms = rec.poll_intvl_ms;
    rb.last_state = rec.last_state;
    rb.cmd_counter = rec.cmd_counter;
    blinds.insert({rb.blind_address, std::move(rb)});
  }

  fclose(f);
  ESP_LOGI(ELERO_STORAGE_TAG, "Loaded %d runtime blind(s) from flash", count);
  return true;
}

// ─── RF packet log persistence ────────────────────────────────────────────

bool EleroStorage::save_packets(const std::vector<RawPacket> &packets, uint8_t write_idx) {
  if (!mounted_) return false;

  FILE *f = fopen(PACKETS_PATH, "wb");
  if (!f) {
    ESP_LOGE(ELERO_STORAGE_TAG, "Failed to open %s for writing", PACKETS_PATH);
    return false;
  }

  uint32_t magic = STORAGE_MAGIC_PACKETS;
  uint16_t version = STORAGE_VERSION_1;
  uint16_t count = static_cast<uint16_t>(packets.size());
  uint8_t widx = write_idx;

  fwrite(&magic, sizeof(magic), 1, f);
  fwrite(&version, sizeof(version), 1, f);
  fwrite(&count, sizeof(count), 1, f);
  fwrite(&widx, sizeof(widx), 1, f);

  for (const auto &pkt : packets) {
    fwrite(&pkt.timestamp_ms, sizeof(pkt.timestamp_ms), 1, f);
    fwrite(&pkt.fifo_len, sizeof(pkt.fifo_len), 1, f);
    fwrite(pkt.data, sizeof(pkt.data), 1, f);
    fwrite(&pkt.valid, sizeof(pkt.valid), 1, f);
    fwrite(pkt.reject_reason, sizeof(pkt.reject_reason), 1, f);
  }

  fclose(f);
  ESP_LOGD(ELERO_STORAGE_TAG, "Saved %d RF packet(s) to flash", count);
  return true;
}

bool EleroStorage::load_packets(std::vector<RawPacket> &packets, uint8_t &write_idx) {
  if (!mounted_) return false;

  FILE *f = fopen(PACKETS_PATH, "rb");
  if (!f) {
    ESP_LOGD(ELERO_STORAGE_TAG, "No packet log file found");
    return false;
  }

  uint32_t magic = 0;
  uint16_t version = 0;
  uint16_t count = 0;
  uint8_t widx = 0;

  if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != STORAGE_MAGIC_PACKETS) {
    ESP_LOGW(ELERO_STORAGE_TAG, "Invalid packet log file (bad magic)");
    fclose(f);
    return false;
  }
  if (fread(&version, sizeof(version), 1, f) != 1 || version != STORAGE_VERSION_1) {
    ESP_LOGW(ELERO_STORAGE_TAG, "Unsupported packet log version %d", version);
    fclose(f);
    return false;
  }
  if (fread(&count, sizeof(count), 1, f) != 1 ||
      fread(&widx, sizeof(widx), 1, f) != 1) {
    ESP_LOGW(ELERO_STORAGE_TAG, "Failed to read packet log header");
    fclose(f);
    return false;
  }

  packets.clear();
  packets.reserve(count);

  for (uint16_t i = 0; i < count; i++) {
    RawPacket pkt{};
    if (fread(&pkt.timestamp_ms, sizeof(pkt.timestamp_ms), 1, f) != 1) break;
    if (fread(&pkt.fifo_len, sizeof(pkt.fifo_len), 1, f) != 1) break;
    if (fread(pkt.data, sizeof(pkt.data), 1, f) != 1) break;
    if (fread(&pkt.valid, sizeof(pkt.valid), 1, f) != 1) break;
    if (fread(pkt.reject_reason, sizeof(pkt.reject_reason), 1, f) != 1) break;
    packets.push_back(pkt);
  }

  write_idx = widx;
  fclose(f);
  ESP_LOGI(ELERO_STORAGE_TAG, "Loaded %zu RF packet(s) from flash", packets.size());
  return true;
}

// ─── Blind state snapshot persistence ─────────────────────────────────────

bool EleroStorage::save_blind_states(const std::vector<PersistedBlindState> &states) {
  if (!mounted_) return false;

  FILE *f = fopen(STATES_PATH, "wb");
  if (!f) {
    ESP_LOGE(ELERO_STORAGE_TAG, "Failed to open %s for writing", STATES_PATH);
    return false;
  }

  uint32_t magic = STORAGE_MAGIC_STATES;
  uint16_t version = STORAGE_VERSION_1;
  uint16_t count = static_cast<uint16_t>(states.size());

  fwrite(&magic, sizeof(magic), 1, f);
  fwrite(&version, sizeof(version), 1, f);
  fwrite(&count, sizeof(count), 1, f);

  for (const auto &state : states) {
    fwrite(&state, sizeof(state), 1, f);
  }

  fclose(f);
  ESP_LOGD(ELERO_STORAGE_TAG, "Saved %d blind state(s) to flash", count);
  return true;
}

bool EleroStorage::load_blind_states(std::vector<PersistedBlindState> &states) {
  if (!mounted_) return false;

  FILE *f = fopen(STATES_PATH, "rb");
  if (!f) {
    ESP_LOGD(ELERO_STORAGE_TAG, "No blind state file found");
    return false;
  }

  uint32_t magic = 0;
  uint16_t version = 0;
  uint16_t count = 0;

  if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != STORAGE_MAGIC_STATES) {
    ESP_LOGW(ELERO_STORAGE_TAG, "Invalid blind state file (bad magic)");
    fclose(f);
    return false;
  }
  if (fread(&version, sizeof(version), 1, f) != 1 || version != STORAGE_VERSION_1) {
    ESP_LOGW(ELERO_STORAGE_TAG, "Unsupported blind state version %d", version);
    fclose(f);
    return false;
  }
  if (fread(&count, sizeof(count), 1, f) != 1) {
    ESP_LOGW(ELERO_STORAGE_TAG, "Failed to read blind state count");
    fclose(f);
    return false;
  }

  states.clear();
  states.reserve(count);

  for (uint16_t i = 0; i < count; i++) {
    PersistedBlindState state{};
    if (fread(&state, sizeof(state), 1, f) != 1) break;
    states.push_back(state);
  }

  fclose(f);
  ESP_LOGI(ELERO_STORAGE_TAG, "Loaded %zu blind state(s) from flash", states.size());
  return true;
}

}  // namespace elero
}  // namespace esphome
