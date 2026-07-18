#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "../elero/elero.h"
#include <vector>

namespace esphome {
namespace elero {

/// A virtual cover that groups multiple EleroCover entities.
/// Compatible members share a dedicated native multi-destination delivery
/// module; incompatible members receive the same semantic intent through their
/// own delivery modules.
class EleroGroupCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  cover::CoverTraits get_traits() override;

  void set_elero_parent(Elero *parent) { parent_ = parent; }
  void set_assumed_state(bool assumed) { assumed_state_ = assumed; }
  void set_hide_members(bool hide) { hide_members_ = hide; }
  void add_member(EleroBlindBase *member) { members_.push_back(member); }

 protected:
  void control(const cover::CoverCall &call) override;

  void submit_group_intent_(const CommandIntent &intent);
  void submit_to_members_(const CommandIntent &intent);
  bool can_use_native_group_() const;
  CommandDeliveryConfig build_native_config_() const;
  void handle_native_outcome_(const DeliveryOutcome &outcome);
  void update_position_();

  Elero *parent_{nullptr};
  std::vector<EleroBlindBase *> members_;
  CommandIntentDelivery native_delivery_;
  uint32_t last_position_update_{0};
  bool native_group_{false};
  bool assumed_state_{true};
  bool hide_members_{false};
};

}  // namespace elero
}  // namespace esphome
