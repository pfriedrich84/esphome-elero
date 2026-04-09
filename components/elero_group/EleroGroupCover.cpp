#include "EleroGroupCover.h"
#include "esphome/core/log.h"

namespace esphome {
namespace elero {

static const char *const TAG = "elero.group";

void EleroGroupCover::setup() {
  this->native_group_ = this->can_use_native_group_();
  ESP_LOGD(TAG, "Group '%s' with %d members, native_group=%s",
           this->get_name().c_str(), (int) this->members_.size(),
           this->native_group_ ? "true" : "false");
  // Start with average position of members
  this->update_position_();
}

void EleroGroupCover::loop() {
  uint32_t now = millis();
  // Update position every 1 second from member states
  if (now - this->last_position_update_ >= 1000) {
    this->last_position_update_ = now;
    this->update_position_();
  }
}

void EleroGroupCover::dump_config() {
  ESP_LOGCONFIG(TAG, "Elero Group Cover '%s':", this->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Members: %d", (int) this->members_.size());
  ESP_LOGCONFIG(TAG, "  Native multi-dest: %s", this->native_group_ ? "yes" : "no (different remote/channel)");
  for (auto *member : this->members_) {
    ESP_LOGCONFIG(TAG, "    - 0x%06x (ch=%d, remote=0x%06x)",
                  member->get_blind_address(), member->get_channel(), member->get_remote_address());
  }
}

cover::CoverTraits EleroGroupCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(false);  // Group doesn't support intermediate positions
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(this->assumed_state_);
  // Support tilt only if ALL members support it
  bool all_tilt = true;
  for (auto *member : this->members_) {
    if (!member->get_supports_tilt()) {
      all_tilt = false;
      break;
    }
  }
  traits.set_supports_tilt(all_tilt);
  return traits;
}

void EleroGroupCover::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->send_group_command_(0x10);  // STOP
    this->current_operation = cover::COVER_OPERATION_IDLE;
    this->publish_state();
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (pos == cover::COVER_OPEN) {
      this->send_group_command_(0x20);  // UP
      this->current_operation = cover::COVER_OPERATION_OPENING;
    } else if (pos == cover::COVER_CLOSED) {
      this->send_group_command_(0x40);  // DOWN
      this->current_operation = cover::COVER_OPERATION_CLOSING;
    } else {
      // Intermediate positions: delegate to each member individually
      // (each member has its own open/close duration for dead-reckoning)
      for (auto *member : this->members_) {
        uint8_t cmd = (pos > member->get_cover_position()) ? 0x20 : 0x40;
        member->enqueue_command(cmd);
      }
      this->current_operation = cover::COVER_OPERATION_OPENING;
    }
    this->publish_state();
  }
  if (call.get_tilt().has_value()) {
    auto tilt = *call.get_tilt();
    if (tilt > 0) {
      this->send_group_command_(0x24);  // TILT
    }
  }
  if (call.get_toggle().has_value()) {
    if (this->current_operation != cover::COVER_OPERATION_IDLE) {
      this->send_group_command_(0x10);  // STOP
      this->current_operation = cover::COVER_OPERATION_IDLE;
    } else {
      this->send_group_command_(0x20);  // UP
      this->current_operation = cover::COVER_OPERATION_OPENING;
    }
    this->publish_state();
  }
}

void EleroGroupCover::send_group_command_(uint8_t cmd_byte) {
  if (this->native_group_ && this->parent_ != nullptr) {
    // Native multi-dest: single RF packet to all members (same channel/remote)
    t_elero_command cmd{};
    this->build_group_command_(cmd, cmd_byte);
    this->parent_->send_command(&cmd);
    // Increment counter (wraps 255 → 1, skip 0)
    if (this->group_counter_ == 0xFF)
      this->group_counter_ = 1;
    else
      this->group_counter_++;
    ESP_LOGD(TAG, "Sent native group command 0x%02x to %d dests", cmd_byte, (int) this->members_.size());
  } else if (this->parent_ != nullptr) {
    // Direct TX: build each member's command and enqueue directly to the hub's
    // TX queue, bypassing per-cover command queues and dispatch_commands() delays.
    // This fills the FreeRTOS TX queue in one burst — the radio task sends them
    // back-to-back with only 1ms cooldown between each (~5ms per blind total).
    for (auto *member : this->members_) {
      t_elero_command cmd = member->build_tx_command(cmd_byte);
      this->parent_->send_command(&cmd);
    }
    ESP_LOGD(TAG, "Sent direct burst 0x%02x to %d members", cmd_byte, (int) this->members_.size());
  } else {
    // Last resort fallback: use per-cover command queues
    for (auto *member : this->members_) {
      member->enqueue_command(cmd_byte);
    }
  }
}

bool EleroGroupCover::can_use_native_group_() const {
  if (this->members_.size() < 2 || this->members_.size() > ELERO_MAX_DESTS)
    return false;
  uint32_t remote = this->members_[0]->get_remote_address();
  uint8_t channel = this->members_[0]->get_channel();
  for (size_t i = 1; i < this->members_.size(); i++) {
    if (this->members_[i]->get_remote_address() != remote ||
        this->members_[i]->get_channel() != channel)
      return false;
  }
  return true;
}

void EleroGroupCover::build_group_command_(t_elero_command &cmd, uint8_t cmd_byte) {
  auto *first = this->members_[0];
  cmd.counter = this->group_counter_;
  cmd.remote_addr = first->get_remote_address();
  cmd.channel = first->get_channel();
  cmd.pck_inf[0] = first->get_pck_inf0();
  cmd.pck_inf[1] = first->get_pck_inf1();
  cmd.hop = first->get_hop();
  cmd.payload[0] = first->get_payload_1();
  cmd.payload[1] = first->get_payload_2();
  cmd.payload[4] = cmd_byte;
  cmd.num_dests = static_cast<uint8_t>(this->members_.size());
  cmd.blind_addr = first->get_blind_address();
  for (size_t i = 0; i < this->members_.size(); i++) {
    cmd.dest_addrs[i] = this->members_[i]->get_blind_address();
  }
}

void EleroGroupCover::update_position_() {
  if (this->members_.empty())
    return;
  float sum = 0.0f;
  int count = 0;
  bool any_opening = false;
  bool any_closing = false;
  for (auto *member : this->members_) {
    float pos = member->get_cover_position();
    if (!std::isnan(pos)) {
      sum += pos;
      count++;
    }
    const char *op = member->get_operation_str();
    if (op[0] == 'o') any_opening = true;  // "opening"
    if (op[0] == 'c') any_closing = true;  // "closing"
  }
  float new_pos = (count > 0) ? sum / count : 0.5f;
  cover::CoverOperation new_op = cover::COVER_OPERATION_IDLE;
  if (any_opening)
    new_op = cover::COVER_OPERATION_OPENING;
  else if (any_closing)
    new_op = cover::COVER_OPERATION_CLOSING;

  bool changed = (std::abs(new_pos - this->position) > 0.01f) ||
                 (new_op != this->current_operation);
  if (changed) {
    this->position = new_pos;
    this->current_operation = new_op;
    this->publish_state(false);
  }
}

}  // namespace elero
}  // namespace esphome
