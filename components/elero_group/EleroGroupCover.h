#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "../elero/elero.h"
#include <vector>

namespace esphome {
namespace elero {

/// A virtual cover that groups multiple EleroCover entities.
/// On open/close/stop/tilt it sends a single multi-destination RF packet when all
/// members share the same remote_address and channel, otherwise falls back to
/// sequential individual commands.
class EleroGroupCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  cover::CoverTraits get_traits() override;

  void set_elero_parent(Elero *parent) { parent_ = parent; }
  void set_assumed_state(bool assumed) { assumed_state_ = assumed; }
  void add_member(EleroBlindBase *member) { members_.push_back(member); }

 protected:
  void control(const cover::CoverCall &call) override;
  /// Send a command byte to all group members via native multi-dest or sequential fallback.
  void send_group_command_(uint8_t cmd_byte);
  /// True if all members share remote_address and channel (native multi-dest possible).
  bool can_use_native_group_() const;
  /// Build a multi-dest t_elero_command from the first member's RF params.
  void build_group_command_(t_elero_command &cmd, uint8_t cmd_byte);
  /// Update group position from member positions.
  void update_position_();

  Elero *parent_{nullptr};
  std::vector<EleroBlindBase *> members_;
  uint8_t group_counter_{1};
  uint32_t last_position_update_{0};
  bool native_group_{false};  // cached result of can_use_native_group_()
  bool assumed_state_{true};
};

}  // namespace elero
}  // namespace esphome
