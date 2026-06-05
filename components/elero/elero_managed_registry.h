#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace esphome {
namespace elero {

static const uint16_t ELERO_MANAGED_SCHEMA_VERSION = 1;
static const uint8_t ELERO_MANAGED_MAX_DEVICES_LIMIT = 32;
static const uint32_t ELERO_MANAGED_DEFAULT_POLL_INTERVAL_MS = 300000;

enum class ManagedDeviceType : uint8_t { COVER = 0, LIGHT = 1 };

struct ManagedDevice {
  bool enabled{true};
  ManagedDeviceType type{ManagedDeviceType::COVER};
  std::string name;
  uint32_t blind_address{0};
  uint32_t remote_address{0};
  uint8_t channel{0};
  uint8_t pck_inf[2]{0, 0};
  uint8_t hop{0};
  uint8_t payload[10]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint32_t open_duration_ms{0};
  uint32_t close_duration_ms{0};
  uint32_t dim_duration_ms{0};
  uint32_t poll_interval_ms{ELERO_MANAGED_DEFAULT_POLL_INTERVAL_MS};
  bool supports_tilt{false};
};

struct ManagedRegistry {
  uint16_t schema_version{ELERO_MANAGED_SCHEMA_VERSION};
  uint32_t registry_revision{0};
  uint32_t hub_id{0};
  std::vector<ManagedDevice> devices;
  uint32_t checksum{0};
};

struct ManagedRegistryValidation {
  bool ok{false};
  std::string error;
};

namespace managed_registry {

inline uint32_t fnv1a_update(uint32_t hash, uint8_t value) {
  hash ^= value;
  hash *= 16777619UL;
  return hash;
}

inline uint32_t fnv1a_update_u16(uint32_t hash, uint16_t value) {
  hash = fnv1a_update(hash, static_cast<uint8_t>(value & 0xff));
  return fnv1a_update(hash, static_cast<uint8_t>((value >> 8) & 0xff));
}

inline uint32_t fnv1a_update_u32(uint32_t hash, uint32_t value) {
  for (uint8_t i = 0; i < 4; i++)
    hash = fnv1a_update(hash, static_cast<uint8_t>((value >> (i * 8)) & 0xff));
  return hash;
}

inline uint32_t fnv1a_update_string(uint32_t hash, const std::string &value) {
  hash = fnv1a_update_u32(hash, static_cast<uint32_t>(value.size()));
  for (char c : value)
    hash = fnv1a_update(hash, static_cast<uint8_t>(c));
  return hash;
}

inline uint32_t calculate_checksum(const ManagedRegistry &registry) {
  uint32_t hash = 2166136261UL;
  hash = fnv1a_update_u16(hash, registry.schema_version);
  hash = fnv1a_update_u32(hash, registry.registry_revision);
  hash = fnv1a_update_u32(hash, registry.hub_id);
  hash = fnv1a_update_u32(hash, static_cast<uint32_t>(registry.devices.size()));
  for (const auto &device : registry.devices) {
    hash = fnv1a_update(hash, device.enabled ? 1 : 0);
    hash = fnv1a_update(hash, static_cast<uint8_t>(device.type));
    hash = fnv1a_update_string(hash, device.name);
    hash = fnv1a_update_u32(hash, device.blind_address);
    hash = fnv1a_update_u32(hash, device.remote_address);
    hash = fnv1a_update(hash, device.channel);
    hash = fnv1a_update(hash, device.pck_inf[0]);
    hash = fnv1a_update(hash, device.pck_inf[1]);
    hash = fnv1a_update(hash, device.hop);
    for (uint8_t value : device.payload)
      hash = fnv1a_update(hash, value);
    hash = fnv1a_update_u32(hash, device.open_duration_ms);
    hash = fnv1a_update_u32(hash, device.close_duration_ms);
    hash = fnv1a_update_u32(hash, device.dim_duration_ms);
    hash = fnv1a_update_u32(hash, device.poll_interval_ms);
    hash = fnv1a_update(hash, device.supports_tilt ? 1 : 0);
  }
  return hash == 0 ? 1 : hash;
}

inline ManagedRegistryValidation validate(
    const ManagedRegistry &registry, uint8_t max_devices, uint32_t expected_hub_id,
    uint32_t expected_registry_revision = std::numeric_limits<uint32_t>::max()) {
  if (registry.schema_version != ELERO_MANAGED_SCHEMA_VERSION)
    return {false, "unsupported schema_version"};
  if (registry.hub_id != expected_hub_id)
    return {false, "hub_id does not match this ESP32"};
  if (expected_registry_revision != std::numeric_limits<uint32_t>::max() &&
      registry.registry_revision != expected_registry_revision)
    return {false, "registry_revision is stale"};
  if (registry.devices.size() > max_devices)
    return {false, "too many devices"};
  if (registry.checksum != calculate_checksum(registry))
    return {false, "checksum mismatch"};

  for (size_t i = 0; i < registry.devices.size(); i++) {
    const auto &device = registry.devices[i];
    if (device.name.empty())
      return {false, "device name is required"};
    if (device.name.size() > 63)
      return {false, "device name is too long"};
    if (device.blind_address == 0 || device.blind_address > 0xFFFFFF)
      return {false, "blind_address must be a 24-bit non-zero address"};
    if (device.remote_address == 0 || device.remote_address > 0xFFFFFF)
      return {false, "remote_address must be a 24-bit non-zero address"};
    if (device.channel == 0)
      return {false, "channel must be non-zero"};
    if (device.poll_interval_ms != 0 && device.poll_interval_ms < 10000)
      return {false, "poll_interval_ms must be 0 or at least 10000"};
    for (size_t j = i + 1; j < registry.devices.size(); j++) {
      if (device.blind_address == registry.devices[j].blind_address)
        return {false, "duplicate blind_address"};
    }
  }
  return {true, ""};
}

inline void refresh_checksum(ManagedRegistry &registry) { registry.checksum = calculate_checksum(registry); }

}  // namespace managed_registry
}  // namespace elero
}  // namespace esphome
