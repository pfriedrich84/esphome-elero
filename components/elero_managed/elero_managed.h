#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_traits.h"
#ifdef USE_API
#include "esphome/components/api/custom_api_device.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "../elero/elero.h"
#include "../elero/elero_managed_materialization.h"
#include "../elero/elero_managed_registry.h"
#include <queue>
#include <string>
#include <vector>

namespace esphome {
namespace elero_managed {

class EleroManagedCoverSlot : public cover::Cover, public Component, public elero::EleroBlindBase {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  cover::CoverTraits get_traits() override;

  void set_elero_parent(elero::Elero *parent) { this->parent_ = parent; }
  void set_managed_slot(bool managed) { this->managed_slot_ = managed; }
  void apply_managed_device(const elero::ManagedDevice &device);
  void set_rx_state(uint8_t state) override;
  void notify_rx_meta(uint32_t ms, float rssi) override {
    this->last_seen_ms_ = ms;
    this->last_rssi_ = rssi;
  }
  uint32_t get_blind_address() override { return this->command_.blind_addr; }
  void set_poll_offset(uint32_t offset) override { this->poll_offset_ = offset; }
  std::string get_blind_name() const override { return std::string(this->get_name().c_str()); }
  float get_cover_position() const override { return this->position; }
  const char *get_operation_str() const override;
  uint32_t get_last_seen_ms() const override { return this->last_seen_ms_; }
  float get_last_rssi() const override { return this->last_rssi_; }
  uint8_t get_last_state_raw() const override { return this->last_state_raw_; }
  uint8_t get_channel() const override { return this->command_.channel; }
  uint32_t get_remote_address() const override { return this->command_.remote_addr; }
  uint32_t get_poll_interval_ms() const override { return this->poll_intvl_; }
  uint32_t get_open_duration_ms() const override { return this->open_duration_; }
  uint32_t get_close_duration_ms() const override { return this->close_duration_; }
  bool get_supports_tilt() const override { return this->supports_tilt_; }
  uint8_t get_command_up() const override { return this->command_up_; }
  uint8_t get_command_down() const override { return this->command_down_; }
  uint8_t get_command_stop() const override { return this->command_stop_; }
  uint8_t get_command_check() const override { return this->command_check_; }
  uint8_t get_command_tilt() const override { return this->command_tilt_; }
  uint8_t get_hop() const override { return this->command_.hop; }
  uint8_t get_pck_inf0() const override { return this->command_.pck_inf[0]; }
  uint8_t get_pck_inf1() const override { return this->command_.pck_inf[1]; }
  uint8_t get_payload_1() const override { return this->command_.payload[0]; }
  uint8_t get_payload_2() const override { return this->command_.payload[1]; }
  elero::t_elero_command build_tx_command(uint8_t cmd_byte) override;
  void enqueue_command(uint8_t cmd_byte) override;
  void apply_runtime_settings(uint32_t open_dur_ms, uint32_t close_dur_ms, uint32_t poll_intvl_ms) override;
  void schedule_immediate_poll() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void handle_commands_(uint32_t now);
  void increase_counter_();

  elero::Elero *parent_{nullptr};
  bool managed_slot_{false};
  bool active_{false};
  elero::t_elero_command command_{.counter = 1};
  std::queue<uint8_t> commands_to_send_;
  uint32_t last_command_{0};
  uint32_t poll_intvl_{0};
  uint32_t poll_offset_{0};
  uint32_t last_poll_{0};
  uint32_t open_duration_{0};
  uint32_t close_duration_{0};
  uint32_t last_seen_ms_{0};
  uint32_t last_queue_drain_ms_{0};
  float last_rssi_{0.0f};
  uint8_t last_state_raw_{elero::ELERO_STATE_UNKNOWN};
  uint8_t send_retries_{0};
  uint8_t send_packets_{0};
  uint8_t command_up_{0x20};
  uint8_t command_down_{0x40};
  uint8_t command_stop_{0x10};
  uint8_t command_check_{0x00};
  uint8_t command_tilt_{0x24};
  bool supports_tilt_{false};
  bool queue_full_published_{false};
};

class EleroManagedLightSlot : public light::LightOutput, public Component, public elero::EleroLightBase {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

  void set_elero_parent(elero::Elero *parent) { this->parent_ = parent; }
  void set_managed_slot(bool managed) { this->managed_slot_ = managed; }
  void apply_managed_device(const elero::ManagedDevice &device);
  uint32_t get_blind_address() override { return this->command_.blind_addr; }
  void set_rx_state(uint8_t state) override;
  void notify_rx_meta(uint32_t ms, float rssi) override {
    this->last_seen_ms_ = ms;
    this->last_rssi_ = rssi;
  }
  void enqueue_command(uint8_t cmd_byte) override;
  void schedule_immediate_poll() override;
  std::string get_light_name() const override;
  float get_brightness() const override { return this->brightness_; }
  bool get_is_on() const override { return this->is_on_; }
  const char *get_operation_str() const override { return "idle"; }
  uint32_t get_last_seen_ms() const override { return this->last_seen_ms_; }
  float get_last_rssi() const override { return this->last_rssi_; }
  uint8_t get_last_state_raw() const override { return this->is_on_ ? elero::ELERO_STATE_ON : elero::ELERO_STATE_OFF; }
  uint8_t get_channel() const override { return this->command_.channel; }
  uint32_t get_remote_address() const override { return this->command_.remote_addr; }
  uint32_t get_dim_duration_ms() const override { return this->dim_duration_; }
  uint8_t get_command_on() const override { return this->command_on_; }
  uint8_t get_command_off() const override { return this->command_off_; }
  uint8_t get_command_stop() const override { return this->command_stop_; }
  uint8_t get_command_check() const override { return this->command_check_; }

 protected:
  void handle_commands_(uint32_t now);
  void increase_counter_();

  elero::Elero *parent_{nullptr};
  light::LightState *state_{nullptr};
  bool managed_slot_{false};
  bool active_{false};
  elero::t_elero_command command_{.counter = 1};
  std::queue<uint8_t> commands_to_send_;
  uint32_t last_command_{0};
  uint32_t last_seen_ms_{0};
  uint32_t last_queue_drain_ms_{0};
  float last_rssi_{0.0f};
  float brightness_{0.0f};
  bool is_on_{false};
  bool state_initialized_{false};
  uint32_t dim_duration_{0};
  uint8_t send_retries_{0};
  uint8_t send_packets_{0};
  uint8_t command_on_{0x20};
  uint8_t command_off_{0x40};
  uint8_t command_stop_{0x10};
  uint8_t command_check_{0x00};
  bool queue_full_published_{false};
};

class EleroManaged : public Component
#ifdef USE_API
    , public api::CustomAPIDevice
#endif
{
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA + 1.0f; }

  void set_parent(elero::Elero *parent) { this->parent_ = parent; }
  void set_enabled(bool enabled) { this->enabled_ = enabled; }
  void set_max_devices(uint8_t max_devices) { this->max_devices_ = max_devices; }
  void set_preallocated_cover_slots(uint8_t slots) { this->preallocated_cover_slots_ = slots; }
  void set_preallocated_light_slots(uint8_t slots) { this->preallocated_light_slots_ = slots; }
  void add_preallocated_cover_slot(EleroManagedCoverSlot *slot) { this->preallocated_cover_slot_entities_.push_back(slot); }
  void add_preallocated_light_slot(EleroManagedLightSlot *slot) { this->preallocated_light_slot_entities_.push_back(slot); }
#ifdef USE_TEXT_SENSOR
  void set_response_text_sensor(text_sensor::TextSensor *sensor) { this->response_text_sensor_ = sensor; }
  void set_last_call_text_sensor(text_sensor::TextSensor *sensor) { this->last_call_text_sensor_ = sensor; }
  void set_last_ok_text_sensor(text_sensor::TextSensor *sensor) { this->last_ok_text_sensor_ = sensor; }
  void set_last_error_text_sensor(text_sensor::TextSensor *sensor) { this->last_error_text_sensor_ = sensor; }
  void set_managed_enabled_text_sensor(text_sensor::TextSensor *sensor) { this->managed_enabled_text_sensor_ = sensor; }
  void set_schema_version_text_sensor(text_sensor::TextSensor *sensor) { this->schema_version_text_sensor_ = sensor; }
  void set_component_version_text_sensor(text_sensor::TextSensor *sensor) { this->component_version_text_sensor_ = sensor; }
  void set_hub_id_text_sensor(text_sensor::TextSensor *sensor) { this->hub_id_text_sensor_ = sensor; }
  void set_hub_mac_text_sensor(text_sensor::TextSensor *sensor) { this->hub_mac_text_sensor_ = sensor; }
  void set_max_devices_text_sensor(text_sensor::TextSensor *sensor) { this->max_devices_text_sensor_ = sensor; }
  void set_registry_revision_text_sensor(text_sensor::TextSensor *sensor) { this->registry_revision_text_sensor_ = sensor; }
  void set_device_count_text_sensor(text_sensor::TextSensor *sensor) { this->device_count_text_sensor_ = sensor; }
  void set_entity_materialization_text_sensor(text_sensor::TextSensor *sensor) {
    this->entity_materialization_text_sensor_ = sensor;
  }
  void set_materialized_cover_count_text_sensor(text_sensor::TextSensor *sensor) {
    this->materialized_cover_count_text_sensor_ = sensor;
  }
  void set_materialized_light_count_text_sensor(text_sensor::TextSensor *sensor) {
    this->materialized_light_count_text_sensor_ = sensor;
  }
#endif

  bool is_enabled() const { return this->enabled_; }
  uint8_t get_max_devices() const { return this->max_devices_; }
  uint32_t get_hub_id() const { return this->hub_id_; }
  std::string get_hub_mac() const { return this->hub_mac_; }

  std::string get_elero_info() const;
  elero::ManagedRegistry get_elero_managed_registry() const;
  elero::ManagedRegistryValidation validate_elero_managed_registry(
      const elero::ManagedRegistry &registry) const;
  bool push_elero_managed_registry(const elero::ManagedRegistry &registry, std::string *error = nullptr);
  bool clear_elero_managed_registry(uint32_t expected_registry_revision, std::string *error = nullptr);

#ifdef USE_API
  void api_get_elero_info();
  void api_get_elero_managed_registry();
  void api_validate_elero_managed_registry(std::string registry_json);
  void api_push_elero_managed_registry(std::string registry_json);
  void api_clear_elero_managed_registry(std::string registry_revision, std::string confirm);
#endif

 protected:
  void load_registry_();
  bool save_registry_();
  void publish_response_(const std::string &request, const std::string &json, bool ok = true,
                         const std::string &error = "");
  void publish_diagnostic_fields_(const std::string &request, bool ok, const std::string &error);
  void bind_preallocated_slots_();
  std::string registry_to_json_(const elero::ManagedRegistry &registry) const;
  elero::managed_materialization::Plan materialization_plan_() const;
  std::string entity_materialization_status_() const;
  bool registry_from_json_(const std::string &json, elero::ManagedRegistry *registry, std::string *error) const;
  static uint32_t preference_hash_();
  static uint32_t hub_id_from_mac_(const uint8_t mac[6]);

  elero::Elero *parent_{nullptr};
  bool enabled_{false};
  uint8_t max_devices_{elero::ELERO_MANAGED_MAX_DEVICES_LIMIT};
  uint32_t hub_id_{0};
  std::string hub_mac_;
  uint8_t preallocated_cover_slots_{0};
  uint8_t preallocated_light_slots_{0};
  std::vector<EleroManagedCoverSlot *> preallocated_cover_slot_entities_;
  std::vector<EleroManagedLightSlot *> preallocated_light_slot_entities_;
  elero::ManagedRegistry registry_{};
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *response_text_sensor_{nullptr};
  text_sensor::TextSensor *last_call_text_sensor_{nullptr};
  text_sensor::TextSensor *last_ok_text_sensor_{nullptr};
  text_sensor::TextSensor *last_error_text_sensor_{nullptr};
  text_sensor::TextSensor *managed_enabled_text_sensor_{nullptr};
  text_sensor::TextSensor *schema_version_text_sensor_{nullptr};
  text_sensor::TextSensor *component_version_text_sensor_{nullptr};
  text_sensor::TextSensor *hub_id_text_sensor_{nullptr};
  text_sensor::TextSensor *hub_mac_text_sensor_{nullptr};
  text_sensor::TextSensor *max_devices_text_sensor_{nullptr};
  text_sensor::TextSensor *registry_revision_text_sensor_{nullptr};
  text_sensor::TextSensor *device_count_text_sensor_{nullptr};
  text_sensor::TextSensor *entity_materialization_text_sensor_{nullptr};
  text_sensor::TextSensor *materialized_cover_count_text_sensor_{nullptr};
  text_sensor::TextSensor *materialized_light_count_text_sensor_{nullptr};
#endif
  ESPPreferenceObject pref_{};
};

}  // namespace elero_managed
}  // namespace esphome
