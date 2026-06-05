#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "../elero/elero.h"
#include "../elero/elero_managed_registry.h"
#include <string>

namespace esphome {
namespace elero_managed {

class EleroManaged : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_parent(elero::Elero *parent) { this->parent_ = parent; }
  void set_enabled(bool enabled) { this->enabled_ = enabled; }
  void set_max_devices(uint8_t max_devices) { this->max_devices_ = max_devices; }

  bool is_enabled() const { return this->enabled_; }
  uint8_t get_max_devices() const { return this->max_devices_; }
  uint32_t get_hub_id() const { return this->hub_id_; }

  std::string get_elero_info() const;
  elero::ManagedRegistry get_elero_managed_registry() const;
  elero::ManagedRegistryValidation validate_elero_managed_registry(
      const elero::ManagedRegistry &registry) const;
  bool push_elero_managed_registry(const elero::ManagedRegistry &registry, std::string *error = nullptr);

 protected:
  void load_registry_();
  bool save_registry_();
  static uint32_t preference_hash_();

  elero::Elero *parent_{nullptr};
  bool enabled_{false};
  uint8_t max_devices_{elero::ELERO_MANAGED_MAX_DEVICES_LIMIT};
  uint32_t hub_id_{0};
  elero::ManagedRegistry registry_{};
  ESPPreferenceObject pref_{};
};

}  // namespace elero_managed
}  // namespace esphome
