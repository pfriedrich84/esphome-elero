#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#ifdef USE_API
#include "esphome/components/api/custom_api_device.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "../elero/elero.h"
#include "../elero/elero_managed_registry.h"
#include <string>

namespace esphome {
namespace elero_managed {

class EleroManaged : public Component
#ifdef USE_API
    , public api::CustomAPIDevice
#endif
{
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_parent(elero::Elero *parent) { this->parent_ = parent; }
  void set_enabled(bool enabled) { this->enabled_ = enabled; }
  void set_max_devices(uint8_t max_devices) { this->max_devices_ = max_devices; }
#ifdef USE_TEXT_SENSOR
  void set_response_text_sensor(text_sensor::TextSensor *sensor) { this->response_text_sensor_ = sensor; }
  void set_last_call_text_sensor(text_sensor::TextSensor *sensor) { this->last_call_text_sensor_ = sensor; }
  void set_last_ok_text_sensor(text_sensor::TextSensor *sensor) { this->last_ok_text_sensor_ = sensor; }
  void set_last_error_text_sensor(text_sensor::TextSensor *sensor) { this->last_error_text_sensor_ = sensor; }
  void set_hub_id_text_sensor(text_sensor::TextSensor *sensor) { this->hub_id_text_sensor_ = sensor; }
  void set_registry_revision_text_sensor(text_sensor::TextSensor *sensor) { this->registry_revision_text_sensor_ = sensor; }
  void set_device_count_text_sensor(text_sensor::TextSensor *sensor) { this->device_count_text_sensor_ = sensor; }
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

#ifdef USE_API
  void api_get_elero_info();
  void api_get_elero_managed_registry();
  void api_validate_elero_managed_registry(std::string registry_json);
  void api_push_elero_managed_registry(std::string registry_json);
#endif

 protected:
  void load_registry_();
  bool save_registry_();
  void publish_response_(const std::string &request, const std::string &json, bool ok = true,
                         const std::string &error = "");
  void publish_diagnostic_fields_(const std::string &request, bool ok, const std::string &error);
  std::string registry_to_json_(const elero::ManagedRegistry &registry) const;
  bool registry_from_json_(const std::string &json, elero::ManagedRegistry *registry, std::string *error) const;
  static uint32_t preference_hash_();
  static uint32_t hub_id_from_mac_(const uint8_t mac[6]);

  elero::Elero *parent_{nullptr};
  bool enabled_{false};
  uint8_t max_devices_{elero::ELERO_MANAGED_MAX_DEVICES_LIMIT};
  uint32_t hub_id_{0};
  std::string hub_mac_;
  elero::ManagedRegistry registry_{};
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *response_text_sensor_{nullptr};
  text_sensor::TextSensor *last_call_text_sensor_{nullptr};
  text_sensor::TextSensor *last_ok_text_sensor_{nullptr};
  text_sensor::TextSensor *last_error_text_sensor_{nullptr};
  text_sensor::TextSensor *hub_id_text_sensor_{nullptr};
  text_sensor::TextSensor *registry_revision_text_sensor_{nullptr};
  text_sensor::TextSensor *device_count_text_sensor_{nullptr};
#endif
  ESPPreferenceObject pref_{};
};

}  // namespace elero_managed
}  // namespace esphome
