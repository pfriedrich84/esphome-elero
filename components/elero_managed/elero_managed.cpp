#include "elero_managed.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include <cstring>
#include <esp_mac.h>

namespace esphome {
namespace elero_managed {

static const char *const TAG = "elero_managed";
static const uint8_t STORED_NAME_LEN = 64;
static const uint32_t STORED_MAGIC = 0x454d5247;  // EMRG
static const uint32_t PREF_HASH = 0x9d08f4b5;

uint32_t hash_bytes_(const uint8_t *value, size_t len) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < len; i++) {
    hash ^= value[i];
    hash *= 16777619UL;
  }
  return hash == 0 ? 1 : hash;
}

size_t bounded_strlen_(const char *value, size_t max_len) {
  size_t len = 0;
  while (len < max_len && value[len] != '\0')
    len++;
  return len;
}

struct StoredManagedDevice {
  uint8_t enabled;
  uint8_t type;
  char name[STORED_NAME_LEN];
  uint32_t blind_address;
  uint32_t remote_address;
  uint8_t channel;
  uint8_t pck_inf[2];
  uint8_t hop;
  uint8_t payload[10];
  uint32_t open_duration_ms;
  uint32_t close_duration_ms;
  uint32_t dim_duration_ms;
  uint32_t poll_interval_ms;
  uint8_t supports_tilt;
};

struct StoredManagedRegistry {
  uint32_t magic;
  uint16_t schema_version;
  uint8_t device_count;
  uint8_t reserved;
  uint32_t registry_revision;
  uint32_t hub_id;
  uint32_t checksum;
  StoredManagedDevice devices[elero::ELERO_MANAGED_MAX_DEVICES_LIMIT];
};

uint32_t EleroManaged::preference_hash_() { return PREF_HASH; }

uint32_t EleroManaged::hub_id_from_mac_(const uint8_t mac[6]) { return hash_bytes_(mac, 6); }

void EleroManaged::setup() {
  uint8_t mac[6]{};
  if (esp_efuse_mac_get_default(mac) == ESP_OK) {
    char mac_buffer[18];
    snprintf(mac_buffer, sizeof(mac_buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    this->hub_mac_ = mac_buffer;
    this->hub_id_ = hub_id_from_mac_(mac);
  } else {
    ESP_LOGW(TAG, "Could not read ESP32 eFuse MAC; using configured node-name hash as fallback hub_id");
    this->hub_mac_ = "unknown";
    const auto *name = App.get_name().c_str();
    this->hub_id_ = hash_bytes_(reinterpret_cast<const uint8_t *>(name), strlen(name));
  }
  this->registry_.schema_version = elero::ELERO_MANAGED_SCHEMA_VERSION;
  this->registry_.hub_id = this->hub_id_;
  this->pref_ = global_preferences->make_preference<StoredManagedRegistry>(preference_hash_());
  this->load_registry_();
}

void EleroManaged::dump_config() {
  ESP_LOGCONFIG(TAG, "Elero managed mode:");
  ESP_LOGCONFIG(TAG, "  Enabled: %s", YESNO(this->enabled_));
  ESP_LOGCONFIG(TAG, "  Max devices: %u", this->max_devices_);
  ESP_LOGCONFIG(TAG, "  Hub ID: 0x%08lx", (unsigned long) this->hub_id_);
  ESP_LOGCONFIG(TAG, "  Hub MAC: %s", this->hub_mac_.c_str());
  ESP_LOGCONFIG(TAG, "  Registry revision: %lu", (unsigned long) this->registry_.registry_revision);
  ESP_LOGCONFIG(TAG, "  Registry devices: %u", (unsigned) this->registry_.devices.size());
  ESP_LOGCONFIG(TAG, "  Entity materialization: static ESPHome codegen only; restart/recompile spike pending");
}

std::string EleroManaged::get_elero_info() const {
  char buffer[256];
  snprintf(buffer, sizeof(buffer),
           "{\"managed_enabled\":%s,\"schema_version\":%u,\"component_version\":\"spike-1\","
           "\"hub_id\":%lu,\"hub_mac\":\"%s\",\"max_devices\":%u,\"registry_revision\":%lu,\"device_count\":%u,"
           "\"entity_materialization\":\"not_hot_addable_in_spike_restart_required\"}",
           this->enabled_ ? "true" : "false", elero::ELERO_MANAGED_SCHEMA_VERSION,
           (unsigned long) this->hub_id_, this->hub_mac_.c_str(), this->max_devices_,
           (unsigned long) this->registry_.registry_revision,
           (unsigned) this->registry_.devices.size());
  return std::string(buffer);
}

elero::ManagedRegistry EleroManaged::get_elero_managed_registry() const { return this->registry_; }

elero::ManagedRegistryValidation EleroManaged::validate_elero_managed_registry(
    const elero::ManagedRegistry &registry) const {
  if (!this->enabled_)
    return {false, "managed mode is disabled"};
  return elero::managed_registry::validate(registry, this->max_devices_, this->hub_id_);
}

bool EleroManaged::push_elero_managed_registry(const elero::ManagedRegistry &registry, std::string *error) {
  auto validation = this->validate_elero_managed_registry(registry);
  if (!validation.ok) {
    if (error != nullptr)
      *error = validation.error;
    ESP_LOGW(TAG, "Rejected managed registry push: %s", validation.error.c_str());
    return false;
  }

  auto candidate = registry;
  candidate.registry_revision = this->registry_.registry_revision + 1;
  elero::managed_registry::refresh_checksum(candidate);

  auto previous = this->registry_;
  this->registry_ = candidate;
  if (!this->save_registry_()) {
    this->registry_ = previous;
    if (error != nullptr)
      *error = "failed to persist registry";
    return false;
  }
  ESP_LOGI(TAG, "Accepted managed registry revision %lu with %u devices; restart/reconnect required for entity changes",
           (unsigned long) this->registry_.registry_revision, (unsigned) this->registry_.devices.size());
  return true;
}

void EleroManaged::load_registry_() {
  StoredManagedRegistry stored{};
  if (!this->pref_.load(&stored) || stored.magic != STORED_MAGIC ||
      stored.schema_version != elero::ELERO_MANAGED_SCHEMA_VERSION ||
      stored.device_count > this->max_devices_ ||
      stored.device_count > elero::ELERO_MANAGED_MAX_DEVICES_LIMIT ||
      stored.hub_id != this->hub_id_) {
    elero::managed_registry::refresh_checksum(this->registry_);
    ESP_LOGI(TAG, "No accepted managed registry found; starting empty");
    return;
  }

  elero::ManagedRegistry loaded{};
  loaded.schema_version = stored.schema_version;
  loaded.registry_revision = stored.registry_revision;
  loaded.hub_id = stored.hub_id;
  loaded.checksum = stored.checksum;
  for (uint8_t i = 0; i < stored.device_count; i++) {
    const auto &src = stored.devices[i];
    elero::ManagedDevice dst{};
    dst.enabled = src.enabled != 0;
    dst.type = src.type == static_cast<uint8_t>(elero::ManagedDeviceType::LIGHT)
                   ? elero::ManagedDeviceType::LIGHT
                   : elero::ManagedDeviceType::COVER;
    dst.name = std::string(src.name, bounded_strlen_(src.name, STORED_NAME_LEN));
    dst.blind_address = src.blind_address;
    dst.remote_address = src.remote_address;
    dst.channel = src.channel;
    dst.pck_inf[0] = src.pck_inf[0];
    dst.pck_inf[1] = src.pck_inf[1];
    dst.hop = src.hop;
    memcpy(dst.payload, src.payload, sizeof(dst.payload));
    dst.open_duration_ms = src.open_duration_ms;
    dst.close_duration_ms = src.close_duration_ms;
    dst.dim_duration_ms = src.dim_duration_ms;
    dst.poll_interval_ms = src.poll_interval_ms;
    dst.supports_tilt = src.supports_tilt != 0;
    loaded.devices.push_back(dst);
  }

  auto validation = elero::managed_registry::validate(loaded, this->max_devices_, this->hub_id_);
  if (!validation.ok) {
    ESP_LOGW(TAG, "Stored managed registry is invalid and will be ignored: %s", validation.error.c_str());
    return;
  }
  this->registry_ = loaded;
  ESP_LOGI(TAG, "Loaded managed registry revision %lu with %u devices",
           (unsigned long) this->registry_.registry_revision, (unsigned) this->registry_.devices.size());
}

bool EleroManaged::save_registry_() {
  StoredManagedRegistry stored{};
  stored.magic = STORED_MAGIC;
  stored.schema_version = this->registry_.schema_version;
  stored.device_count = static_cast<uint8_t>(this->registry_.devices.size());
  stored.registry_revision = this->registry_.registry_revision;
  stored.hub_id = this->registry_.hub_id;
  stored.checksum = this->registry_.checksum;
  for (uint8_t i = 0; i < stored.device_count; i++) {
    const auto &src = this->registry_.devices[i];
    auto &dst = stored.devices[i];
    dst.enabled = src.enabled ? 1 : 0;
    dst.type = static_cast<uint8_t>(src.type);
    strncpy(dst.name, src.name.c_str(), STORED_NAME_LEN - 1);
    dst.blind_address = src.blind_address;
    dst.remote_address = src.remote_address;
    dst.channel = src.channel;
    dst.pck_inf[0] = src.pck_inf[0];
    dst.pck_inf[1] = src.pck_inf[1];
    dst.hop = src.hop;
    memcpy(dst.payload, src.payload, sizeof(dst.payload));
    dst.open_duration_ms = src.open_duration_ms;
    dst.close_duration_ms = src.close_duration_ms;
    dst.dim_duration_ms = src.dim_duration_ms;
    dst.poll_interval_ms = src.poll_interval_ms;
    dst.supports_tilt = src.supports_tilt ? 1 : 0;
  }
  return this->pref_.save(&stored);
}

}  // namespace elero_managed
}  // namespace esphome
