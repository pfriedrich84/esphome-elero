#include "elero_managed.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include <cstdlib>
#include <cstring>
#include <esp_mac.h>
#include <cJSON.h>

namespace esphome {
namespace elero_managed {

static const char *const TAG = "elero_managed";
static const uint8_t STORED_NAME_LEN = 64;
static const uint32_t STORED_MAGIC = 0x454d5247;  // EMRG
static const uint32_t PREF_HASH = 0x9d08f4b5;
static const char *const COMPONENT_VERSION = "spike-1";

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

static const char *const COVER_SLOT_TAG = "elero_managed.cover";
static const char *const LIGHT_SLOT_TAG = "elero_managed.light";

void EleroManagedCoverSlot::setup() {
  if (!this->active_) {
    ESP_LOGCONFIG(COVER_SLOT_TAG, "Managed cover slot '%s' is unbound", this->get_name().c_str());
    this->mark_failed();
    return;
  }
  if (this->parent_ == nullptr) {
    ESP_LOGE(COVER_SLOT_TAG, "Elero parent not configured for managed cover slot");
    this->mark_failed();
    return;
  }
  this->parent_->register_cover(this);
  this->last_poll_ = millis() - this->poll_intvl_ + this->poll_offset_;
  if (this->command_check_ != 0x00)
    this->enqueue_command(this->command_check_);
}

void EleroManagedCoverSlot::dump_config() {
  LOG_COVER("", "Elero Managed Cover Slot", this);
  ESP_LOGCONFIG(COVER_SLOT_TAG, "  Active: %s", YESNO(this->active_));
  ESP_LOGCONFIG(COVER_SLOT_TAG, "  Blind Address: 0x%06lx", (unsigned long) this->command_.blind_addr);
}

cover::CoverTraits EleroManagedCoverSlot::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_toggle(true);
  traits.set_supports_position(false);
  traits.set_supports_tilt(this->supports_tilt_);
  traits.set_is_assumed_state(true);
  return traits;
}

void EleroManagedCoverSlot::apply_managed_device(const elero::ManagedDevice &device) {
  this->active_ = true;
  this->command_.blind_addr = device.blind_address;
  this->command_.remote_addr = device.remote_address;
  this->command_.channel = device.channel;
  this->command_.pck_inf[0] = device.pck_inf[0];
  this->command_.pck_inf[1] = device.pck_inf[1];
  this->command_.hop = device.hop;
  memcpy(this->command_.payload, device.payload, sizeof(this->command_.payload));
  this->open_duration_ = device.open_duration_ms;
  this->close_duration_ = device.close_duration_ms;
  this->poll_intvl_ = device.poll_interval_ms;
  this->supports_tilt_ = device.supports_tilt;
}

void EleroManagedCoverSlot::loop() {
  const uint32_t now = millis();
  if (this->poll_intvl_ != 0 && this->poll_intvl_ != UINT32_MAX && now - this->last_poll_ > this->poll_intvl_) {
    this->enqueue_command(this->command_check_);
    this->last_poll_ = now;
  }
  this->handle_commands_(now);
}

void EleroManagedCoverSlot::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->enqueue_command(this->command_stop_);
    this->current_operation = cover::COVER_OPERATION_IDLE;
  } else if (call.get_position().has_value()) {
    float pos = *call.get_position();
    if (pos <= 0.01f) {
      this->enqueue_command(this->command_down_);
      this->current_operation = cover::COVER_OPERATION_CLOSING;
    } else if (pos >= 0.99f) {
      this->enqueue_command(this->command_up_);
      this->current_operation = cover::COVER_OPERATION_OPENING;
    }
  } else if (call.get_tilt().has_value() && this->supports_tilt_) {
    this->enqueue_command(this->command_tilt_);
  }
  this->publish_state(false);
}

void EleroManagedCoverSlot::set_rx_state(uint8_t state) {
  this->last_state_raw_ = state;
  switch (state) {
    case elero::ELERO_STATE_TOP:
      this->position = 1.0f;
      this->current_operation = cover::COVER_OPERATION_IDLE;
      break;
    case elero::ELERO_STATE_BOTTOM:
      this->position = 0.0f;
      this->current_operation = cover::COVER_OPERATION_IDLE;
      break;
    case elero::ELERO_STATE_STOPPED:
      this->current_operation = cover::COVER_OPERATION_IDLE;
      break;
    case elero::ELERO_STATE_MOVING_UP:
      this->current_operation = cover::COVER_OPERATION_OPENING;
      break;
    case elero::ELERO_STATE_MOVING_DOWN:
      this->current_operation = cover::COVER_OPERATION_CLOSING;
      break;
    default:
      break;
  }
  this->publish_state(false);
}

const char *EleroManagedCoverSlot::get_operation_str() const {
  return this->current_operation == cover::COVER_OPERATION_IDLE ? "idle" :
         this->current_operation == cover::COVER_OPERATION_OPENING ? "opening" : "closing";
}

elero::t_elero_command EleroManagedCoverSlot::build_tx_command(uint8_t cmd_byte) {
  elero::t_elero_command cmd = this->command_;
  cmd.payload[4] = cmd_byte;
  this->increase_counter_();
  return cmd;
}

void EleroManagedCoverSlot::enqueue_command(uint8_t cmd_byte) {
  if (this->commands_to_send_.size() < elero::ELERO_MAX_COMMAND_QUEUE) {
    this->commands_to_send_.push(cmd_byte);
  } else {
    ESP_LOGW(COVER_SLOT_TAG, "Command queue full for managed cover 0x%06lx", (unsigned long) this->command_.blind_addr);
  }
}

void EleroManagedCoverSlot::apply_runtime_settings(uint32_t open_dur_ms, uint32_t close_dur_ms,
                                                   uint32_t poll_intvl_ms) {
  this->open_duration_ = open_dur_ms;
  this->close_duration_ = close_dur_ms;
  this->poll_intvl_ = poll_intvl_ms;
}

void EleroManagedCoverSlot::schedule_immediate_poll() { this->enqueue_command(this->command_check_); }

void EleroManagedCoverSlot::handle_commands_(uint32_t now) {
  elero::dispatch_commands(this->parent_, this->commands_to_send_, this->command_, this->send_packets_,
                           this->send_retries_, this->last_command_, this->queue_full_published_, now,
                           COVER_SLOT_TAG, this->command_.blind_addr, [](void *ctx) {
                             static_cast<EleroManagedCoverSlot *>(ctx)->increase_counter_();
                           }, this, false, &this->last_queue_drain_ms_);
}

void EleroManagedCoverSlot::increase_counter_() {
  this->command_.counter++;
  if (this->command_.counter == 0)
    this->command_.counter = 1;
}

void EleroManagedLightSlot::setup() {
  if (!this->active_) {
    ESP_LOGCONFIG(LIGHT_SLOT_TAG, "Managed light slot '%s' is unbound", this->get_light_name().c_str());
    this->mark_failed();
    return;
  }
  if (this->parent_ == nullptr) {
    ESP_LOGE(LIGHT_SLOT_TAG, "Elero parent not configured for managed light slot");
    this->mark_failed();
    return;
  }
  this->parent_->register_light(this);
  if (this->command_check_ != 0x00)
    this->enqueue_command(this->command_check_);
}

void EleroManagedLightSlot::dump_config() {
  ESP_LOGCONFIG(LIGHT_SLOT_TAG, "Elero Managed Light Slot:");
  ESP_LOGCONFIG(LIGHT_SLOT_TAG, "  Active: %s", YESNO(this->active_));
  ESP_LOGCONFIG(LIGHT_SLOT_TAG, "  Blind Address: 0x%06lx", (unsigned long) this->command_.blind_addr);
}

light::LightTraits EleroManagedLightSlot::get_traits() {
  auto traits = light::LightTraits();
  if (this->dim_duration_ > 0)
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  else
    traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void EleroManagedLightSlot::apply_managed_device(const elero::ManagedDevice &device) {
  this->active_ = true;
  this->command_.blind_addr = device.blind_address;
  this->command_.remote_addr = device.remote_address;
  this->command_.channel = device.channel;
  this->command_.pck_inf[0] = device.pck_inf[0];
  this->command_.pck_inf[1] = device.pck_inf[1];
  this->command_.hop = device.hop;
  memcpy(this->command_.payload, device.payload, sizeof(this->command_.payload));
  this->dim_duration_ = device.dim_duration_ms;
}

void EleroManagedLightSlot::write_state(light::LightState *state) {
  this->state_ = state;
  bool new_on = state->current_values.is_on();
  if (new_on) {
    this->enqueue_command(this->command_on_);
    this->is_on_ = true;
    this->brightness_ = state->current_values.get_brightness();
  } else {
    this->enqueue_command(this->command_off_);
    this->is_on_ = false;
    this->brightness_ = 0.0f;
  }
}

void EleroManagedLightSlot::loop() { this->handle_commands_(millis()); }

void EleroManagedLightSlot::set_rx_state(uint8_t state) {
  if (state == elero::ELERO_STATE_ON) {
    this->is_on_ = true;
    this->brightness_ = 1.0f;
  } else if (state == elero::ELERO_STATE_OFF) {
    this->is_on_ = false;
    this->brightness_ = 0.0f;
  }
  if (this->state_ != nullptr)
    this->state_->publish_state();
}

void EleroManagedLightSlot::enqueue_command(uint8_t cmd_byte) {
  if (this->commands_to_send_.size() < elero::ELERO_MAX_COMMAND_QUEUE) {
    this->commands_to_send_.push(cmd_byte);
  } else {
    ESP_LOGW(LIGHT_SLOT_TAG, "Command queue full for managed light 0x%06lx", (unsigned long) this->command_.blind_addr);
  }
}

void EleroManagedLightSlot::schedule_immediate_poll() { this->enqueue_command(this->command_check_); }

std::string EleroManagedLightSlot::get_light_name() const {
  return this->state_ != nullptr ? std::string(this->state_->get_name().c_str()) : "Elero Managed Light Slot";
}

void EleroManagedLightSlot::handle_commands_(uint32_t now) {
  elero::dispatch_commands(this->parent_, this->commands_to_send_, this->command_, this->send_packets_,
                           this->send_retries_, this->last_command_, this->queue_full_published_, now,
                           LIGHT_SLOT_TAG, this->command_.blind_addr, [](void *ctx) {
                             static_cast<EleroManagedLightSlot *>(ctx)->increase_counter_();
                           }, this, false, &this->last_queue_drain_ms_);
}

void EleroManagedLightSlot::increase_counter_() {
  this->command_.counter++;
  if (this->command_.counter == 0)
    this->command_.counter = 1;
}

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
  this->bind_preallocated_slots_();
#ifdef USE_API
  this->register_service(&EleroManaged::api_get_elero_info, "get_elero_info");
  this->register_service(&EleroManaged::api_get_elero_managed_registry, "get_elero_managed_registry");
  this->register_service(&EleroManaged::api_validate_elero_managed_registry,
                         "validate_elero_managed_registry", {"registry_json"});
  this->register_service(&EleroManaged::api_push_elero_managed_registry,
                         "push_elero_managed_registry", {"registry_json"});
  this->register_service(&EleroManaged::api_clear_elero_managed_registry,
                         "clear_elero_managed_registry", {"registry_revision", "confirm"});
#endif
}

void EleroManaged::dump_config() {
  ESP_LOGCONFIG(TAG, "Elero managed mode:");
  ESP_LOGCONFIG(TAG, "  Enabled: %s", YESNO(this->enabled_));
  ESP_LOGCONFIG(TAG, "  Max devices: %u", this->max_devices_);
  ESP_LOGCONFIG(TAG, "  Hub ID: 0x%08lx", (unsigned long) this->hub_id_);
  ESP_LOGCONFIG(TAG, "  Hub MAC: %s", this->hub_mac_.c_str());
  ESP_LOGCONFIG(TAG, "  Registry revision: %lu", (unsigned long) this->registry_.registry_revision);
  ESP_LOGCONFIG(TAG, "  Registry devices: %u", (unsigned) this->registry_.devices.size());
  ESP_LOGCONFIG(TAG, "  Preallocated cover slots: %u", this->preallocated_cover_slots_);
  ESP_LOGCONFIG(TAG, "  Preallocated light slots: %u", this->preallocated_light_slots_);
  ESP_LOGCONFIG(TAG, "  Entity materialization: %s", this->entity_materialization_status_().c_str());
  this->publish_diagnostic_fields_("boot", true, "");
}

std::string EleroManaged::get_elero_info() const {
  auto plan = this->materialization_plan_();
  char buffer[512];
  snprintf(buffer, sizeof(buffer),
           "{\"managed_enabled\":%s,\"schema_version\":%u,\"component_version\":\"%s\","
           "\"hub_id\":%lu,\"hub_mac\":\"%s\",\"max_devices\":%u,\"registry_revision\":%lu,\"device_count\":%u,"
           "\"entity_materialization\":\"%s\",\"preallocated_cover_slots\":%u,\"preallocated_light_slots\":%u,"
           "\"materialized_cover_count\":%u,\"materialized_light_count\":%u,"
           "\"unbound_cover_count\":%u,\"unbound_light_count\":%u}",
           this->enabled_ ? "true" : "false", elero::ELERO_MANAGED_SCHEMA_VERSION, COMPONENT_VERSION,
           (unsigned long) this->hub_id_, this->hub_mac_.c_str(), this->max_devices_,
           (unsigned long) this->registry_.registry_revision,
           (unsigned) this->registry_.devices.size(), this->entity_materialization_status_().c_str(),
           this->preallocated_cover_slots_, this->preallocated_light_slots_,
           (unsigned) plan.enabled_cover_count, (unsigned) plan.enabled_light_count,
           (unsigned) plan.unbound_cover_count, (unsigned) plan.unbound_light_count);
  return std::string(buffer);
}

elero::ManagedRegistry EleroManaged::get_elero_managed_registry() const { return this->registry_; }

elero::managed_materialization::Plan EleroManaged::materialization_plan_() const {
  return elero::managed_materialization::build_preallocated_slot_plan(
      this->registry_, this->preallocated_cover_slots_, this->preallocated_light_slots_);
}

std::string EleroManaged::entity_materialization_status_() const {
  if (this->preallocated_cover_slots_ == 0 && this->preallocated_light_slots_ == 0)
    return "static_codegen_only_preallocated_slots_disabled";
  auto plan = this->materialization_plan_();
  if (!plan.fits())
    return "preallocated_slots_insufficient_capacity";
  return "preallocated_slots_bound_at_boot_restart_required";
}

void EleroManaged::bind_preallocated_slots_() {
  auto plan = this->materialization_plan_();
  if (!plan.fits()) {
    ESP_LOGW(TAG, "Managed registry exceeds preallocated entity slots: %u cover(s), %u light(s) unbound",
             (unsigned) plan.unbound_cover_count, (unsigned) plan.unbound_light_count);
  }

  for (const auto &binding : plan.bindings) {
    const auto &device = this->registry_.devices[binding.device_index];
    if (binding.type == elero::ManagedDeviceType::LIGHT) {
      if (binding.slot_index < this->preallocated_light_slot_entities_.size()) {
        auto *slot = this->preallocated_light_slot_entities_[binding.slot_index];
        slot->set_elero_parent(this->parent_);
        slot->apply_managed_device(device);
        ESP_LOGI(TAG, "Bound managed light slot %u to '%s' (0x%06lx)", (unsigned) binding.slot_index,
                 device.name.c_str(), (unsigned long) device.blind_address);
      }
    } else {
      if (binding.slot_index < this->preallocated_cover_slot_entities_.size()) {
        auto *slot = this->preallocated_cover_slot_entities_[binding.slot_index];
        slot->set_elero_parent(this->parent_);
        slot->apply_managed_device(device);
        ESP_LOGI(TAG, "Bound managed cover slot %u to '%s' (0x%06lx)", (unsigned) binding.slot_index,
                 device.name.c_str(), (unsigned long) device.blind_address);
      }
    }
  }
}

elero::ManagedRegistryValidation EleroManaged::validate_elero_managed_registry(
    const elero::ManagedRegistry &registry) const {
  if (!this->enabled_)
    return {false, "managed mode is disabled"};
  auto validation = elero::managed_registry::validate(registry, this->max_devices_, this->hub_id_,
                                                      this->registry_.registry_revision);
  if (!validation.ok)
    return validation;
  if (this->preallocated_cover_slots_ != 0 || this->preallocated_light_slots_ != 0) {
    auto plan = elero::managed_materialization::build_preallocated_slot_plan(
        registry, this->preallocated_cover_slots_, this->preallocated_light_slots_);
    if (!plan.fits())
      return {false, "managed registry exceeds preallocated entity slots"};
  }
  return validation;
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

bool EleroManaged::clear_elero_managed_registry(uint32_t expected_registry_revision, std::string *error) {
  if (!this->enabled_) {
    if (error != nullptr)
      *error = "managed mode is disabled";
    return false;
  }
  if (expected_registry_revision != this->registry_.registry_revision) {
    if (error != nullptr)
      *error = "registry_revision is stale";
    ESP_LOGW(TAG, "Rejected managed registry clear: stale revision %lu (active %lu)",
             (unsigned long) expected_registry_revision, (unsigned long) this->registry_.registry_revision);
    return false;
  }

  auto previous = this->registry_;
  elero::ManagedRegistry cleared{};
  cleared.schema_version = elero::ELERO_MANAGED_SCHEMA_VERSION;
  cleared.registry_revision = this->registry_.registry_revision + 1;
  cleared.hub_id = this->hub_id_;
  elero::managed_registry::refresh_checksum(cleared);
  this->registry_ = cleared;
  if (!this->save_registry_()) {
    this->registry_ = previous;
    if (error != nullptr)
      *error = "failed to persist registry";
    return false;
  }
  ESP_LOGI(TAG, "Cleared managed registry; new revision %lu", (unsigned long) this->registry_.registry_revision);
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

void EleroManaged::publish_diagnostic_fields_(const std::string &request, bool ok, const std::string &error) {
#ifdef USE_TEXT_SENSOR
  if (this->last_call_text_sensor_ != nullptr) {
    this->last_call_text_sensor_->publish_state(std::to_string(millis()) + "ms " + request);
  }
  if (this->last_ok_text_sensor_ != nullptr)
    this->last_ok_text_sensor_->publish_state(ok ? "true" : "false");
  if (this->last_error_text_sensor_ != nullptr)
    this->last_error_text_sensor_->publish_state(error);
  if (this->managed_enabled_text_sensor_ != nullptr)
    this->managed_enabled_text_sensor_->publish_state(this->enabled_ ? "true" : "false");
  if (this->schema_version_text_sensor_ != nullptr)
    this->schema_version_text_sensor_->publish_state(std::to_string(elero::ELERO_MANAGED_SCHEMA_VERSION));
  if (this->component_version_text_sensor_ != nullptr)
    this->component_version_text_sensor_->publish_state(COMPONENT_VERSION);
  if (this->hub_id_text_sensor_ != nullptr)
    this->hub_id_text_sensor_->publish_state(std::to_string(this->hub_id_));
  if (this->hub_mac_text_sensor_ != nullptr)
    this->hub_mac_text_sensor_->publish_state(this->hub_mac_);
  if (this->max_devices_text_sensor_ != nullptr)
    this->max_devices_text_sensor_->publish_state(std::to_string(this->max_devices_));
  if (this->registry_revision_text_sensor_ != nullptr)
    this->registry_revision_text_sensor_->publish_state(std::to_string(this->registry_.registry_revision));
  if (this->device_count_text_sensor_ != nullptr)
    this->device_count_text_sensor_->publish_state(std::to_string(this->registry_.devices.size()));
  if (this->entity_materialization_text_sensor_ != nullptr)
    this->entity_materialization_text_sensor_->publish_state(this->entity_materialization_status_());
#endif
}

void EleroManaged::publish_response_(const std::string &request, const std::string &json, bool ok,
                                     const std::string &error) {
#ifdef USE_TEXT_SENSOR
  if (this->response_text_sensor_ != nullptr)
    this->response_text_sensor_->publish_state(json);
#endif
  this->publish_diagnostic_fields_(request, ok, error);
  ESP_LOGD(TAG, "Native API response: %s", json.c_str());
}

std::string escape_json_string_(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (char c : value) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
      out.push_back(c);
    } else if (c == '\n') {
      out += "\\n";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string EleroManaged::registry_to_json_(const elero::ManagedRegistry &registry) const {
  std::string out;
  out.reserve(256 + registry.devices.size() * 256);
  out += "{\"schema_version\":" + std::to_string(registry.schema_version);
  out += ",\"registry_revision\":" + std::to_string(registry.registry_revision);
  out += ",\"hub_id\":" + std::to_string(registry.hub_id);
  out += ",\"checksum\":" + std::to_string(registry.checksum);
  out += ",\"devices\":[";
  for (size_t i = 0; i < registry.devices.size(); i++) {
    const auto &device = registry.devices[i];
    if (i > 0)
      out += ",";
    out += "{\"enabled\":";
    out += device.enabled ? "true" : "false";
    out += ",\"type\":\"";
    out += device.type == elero::ManagedDeviceType::LIGHT ? "light" : "cover";
    out += "\",\"name\":\"" + escape_json_string_(device.name) + "\"";
    out += ",\"blind_address\":" + std::to_string(device.blind_address);
    out += ",\"remote_address\":" + std::to_string(device.remote_address);
    out += ",\"channel\":" + std::to_string(device.channel);
    out += ",\"pck_inf\":[" + std::to_string(device.pck_inf[0]) + "," + std::to_string(device.pck_inf[1]) + "]";
    out += ",\"hop\":" + std::to_string(device.hop);
    out += ",\"payload\":[";
    for (size_t j = 0; j < 10; j++) {
      if (j > 0)
        out += ",";
      out += std::to_string(device.payload[j]);
    }
    out += "]";
    out += ",\"open_duration_ms\":" + std::to_string(device.open_duration_ms);
    out += ",\"close_duration_ms\":" + std::to_string(device.close_duration_ms);
    out += ",\"dim_duration_ms\":" + std::to_string(device.dim_duration_ms);
    out += ",\"poll_interval_ms\":" + std::to_string(device.poll_interval_ms);
    out += ",\"supports_tilt\":";
    out += device.supports_tilt ? "true" : "false";
    out += "}";
  }
  out += "]}";
  return out;
}

uint32_t json_u32_(const cJSON *object, const char *name, uint32_t fallback) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
  return cJSON_IsNumber(item) ? static_cast<uint32_t>(item->valuedouble) : fallback;
}

uint8_t json_u8_(const cJSON *object, const char *name, uint8_t fallback) {
  return static_cast<uint8_t>(json_u32_(object, name, fallback) & 0xff);
}

bool json_bool_(const cJSON *object, const char *name, bool fallback) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
  if (cJSON_IsBool(item))
    return cJSON_IsTrue(item);
  return fallback;
}

const char *json_string_(const cJSON *object, const char *name, const char *fallback) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
  return cJSON_IsString(item) && item->valuestring != nullptr ? item->valuestring : fallback;
}

bool EleroManaged::registry_from_json_(const std::string &json, elero::ManagedRegistry *registry,
                                       std::string *error) const {
  cJSON *root = cJSON_ParseWithLength(json.c_str(), json.size());
  if (root == nullptr) {
    if (error != nullptr)
      *error = "invalid JSON";
    return false;
  }
  if (!cJSON_IsObject(root)) {
    cJSON_Delete(root);
    if (error != nullptr)
      *error = "registry_json must be a JSON object";
    return false;
  }

  elero::ManagedRegistry parsed{};
  parsed.schema_version = static_cast<uint16_t>(json_u32_(root, "schema_version", elero::ELERO_MANAGED_SCHEMA_VERSION));
  parsed.registry_revision = json_u32_(root, "registry_revision", 0);
  parsed.hub_id = json_u32_(root, "hub_id", 0);
  parsed.checksum = json_u32_(root, "checksum", 0);

  const cJSON *devices = cJSON_GetObjectItemCaseSensitive(root, "devices");
  if (!cJSON_IsArray(devices)) {
    cJSON_Delete(root);
    if (error != nullptr)
      *error = "devices must be an array";
    return false;
  }

  const cJSON *src = nullptr;
  cJSON_ArrayForEach(src, devices) {
    if (!cJSON_IsObject(src))
      continue;
    elero::ManagedDevice dst{};
    dst.enabled = json_bool_(src, "enabled", true);
    const char *type = json_string_(src, "type", "cover");
    dst.type = strcmp(type, "light") == 0 ? elero::ManagedDeviceType::LIGHT : elero::ManagedDeviceType::COVER;
    dst.name = std::string(json_string_(src, "name", ""));
    dst.blind_address = json_u32_(src, "blind_address", 0);
    dst.remote_address = json_u32_(src, "remote_address", 0);
    dst.channel = json_u8_(src, "channel", 0);

    const cJSON *pck_inf = cJSON_GetObjectItemCaseSensitive(src, "pck_inf");
    if (cJSON_IsArray(pck_inf) && cJSON_GetArraySize(pck_inf) >= 2) {
      const cJSON *p0 = cJSON_GetArrayItem(pck_inf, 0);
      const cJSON *p1 = cJSON_GetArrayItem(pck_inf, 1);
      dst.pck_inf[0] = cJSON_IsNumber(p0) ? static_cast<uint8_t>(p0->valuedouble) : 0;
      dst.pck_inf[1] = cJSON_IsNumber(p1) ? static_cast<uint8_t>(p1->valuedouble) : 0;
    } else {
      dst.pck_inf[0] = json_u8_(src, "pck_inf1", 0);
      dst.pck_inf[1] = json_u8_(src, "pck_inf2", 0);
    }

    dst.hop = json_u8_(src, "hop", 0);
    const cJSON *payload = cJSON_GetObjectItemCaseSensitive(src, "payload");
    if (cJSON_IsArray(payload)) {
      uint8_t i = 0;
      const cJSON *payload_value = nullptr;
      cJSON_ArrayForEach(payload_value, payload) {
        if (i >= 10)
          break;
        dst.payload[i++] = cJSON_IsNumber(payload_value) ? static_cast<uint8_t>(payload_value->valuedouble) : 0;
      }
    }
    dst.open_duration_ms = json_u32_(src, "open_duration_ms", 0);
    dst.close_duration_ms = json_u32_(src, "close_duration_ms", 0);
    dst.dim_duration_ms = json_u32_(src, "dim_duration_ms", 0);
    dst.poll_interval_ms = json_u32_(src, "poll_interval_ms", elero::ELERO_MANAGED_DEFAULT_POLL_INTERVAL_MS);
    dst.supports_tilt = json_bool_(src, "supports_tilt", false);
    parsed.devices.push_back(dst);
  }

  cJSON_Delete(root);
  *registry = parsed;
  return true;
}

#ifdef USE_API
void EleroManaged::api_get_elero_info() {
  this->publish_response_("get_elero_info", this->get_elero_info());
}

void EleroManaged::api_get_elero_managed_registry() {
  this->publish_response_("get_elero_managed_registry", this->registry_to_json_(this->registry_));
}

void EleroManaged::api_validate_elero_managed_registry(std::string registry_json) {
  elero::ManagedRegistry registry{};
  std::string error;
  if (!this->registry_from_json_(registry_json, &registry, &error)) {
    this->publish_response_("validate_elero_managed_registry",
                            "{\"ok\":false,\"request\":\"validate_elero_managed_registry\",\"error\":\"" +
                                escape_json_string_(error) + "\"}",
                            false, error);
    return;
  }
  auto result = this->validate_elero_managed_registry(registry);
  this->publish_response_("validate_elero_managed_registry",
                          "{\"ok\":" + std::string(result.ok ? "true" : "false") +
                              ",\"request\":\"validate_elero_managed_registry\",\"error\":\"" +
                              escape_json_string_(result.error) + "\"}",
                          result.ok, result.error);
}

void EleroManaged::api_push_elero_managed_registry(std::string registry_json) {
  elero::ManagedRegistry registry{};
  std::string error;
  if (!this->registry_from_json_(registry_json, &registry, &error)) {
    this->publish_response_("push_elero_managed_registry",
                            "{\"ok\":false,\"request\":\"push_elero_managed_registry\",\"error\":\"" +
                                escape_json_string_(error) + "\"}",
                            false, error);
    return;
  }
  bool ok = this->push_elero_managed_registry(registry, &error);
  this->publish_response_("push_elero_managed_registry",
                          "{\"ok\":" + std::string(ok ? "true" : "false") +
                              ",\"request\":\"push_elero_managed_registry\",\"error\":\"" +
                              escape_json_string_(error) + "\",\"registry_revision\":" +
                              std::to_string(this->registry_.registry_revision) + "}",
                          ok, error);
}

void EleroManaged::api_clear_elero_managed_registry(std::string registry_revision, std::string confirm) {
  std::string error;
  if (confirm != "CLEAR") {
    error = "confirm must be CLEAR";
    this->publish_response_("clear_elero_managed_registry",
                            "{\"ok\":false,\"request\":\"clear_elero_managed_registry\",\"error\":\"" +
                                escape_json_string_(error) + "\"}",
                            false, error);
    return;
  }

  char *end = nullptr;
  uint32_t expected_revision = static_cast<uint32_t>(strtoul(registry_revision.c_str(), &end, 10));
  if (end == registry_revision.c_str() || (end != nullptr && *end != '\0')) {
    error = "registry_revision must be an integer";
    this->publish_response_("clear_elero_managed_registry",
                            "{\"ok\":false,\"request\":\"clear_elero_managed_registry\",\"error\":\"" +
                                escape_json_string_(error) + "\"}",
                            false, error);
    return;
  }
  bool ok = this->clear_elero_managed_registry(expected_revision, &error);
  this->publish_response_("clear_elero_managed_registry",
                          "{\"ok\":" + std::string(ok ? "true" : "false") +
                              ",\"request\":\"clear_elero_managed_registry\",\"error\":\"" +
                              escape_json_string_(error) + "\",\"registry_revision\":" +
                              std::to_string(this->registry_.registry_revision) + "}",
                          ok, error);
}
#endif

}  // namespace elero_managed
}  // namespace esphome
