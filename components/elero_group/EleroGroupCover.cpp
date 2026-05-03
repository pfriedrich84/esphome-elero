#include "EleroGroupCover.h"
#include "../elero/elero_group_command_logic.h"
#include "esphome/core/log.h"

namespace esphome {
namespace elero {

static const char *const TAG = "elero.group";

static group_command_logic::HubSubmitResult to_group_submit_result(SendResult result) {
  switch (result) {
    case SendResult::OK:
      return group_command_logic::HubSubmitResult::OK;
    case SendResult::QUEUE_FULL:
      return group_command_logic::HubSubmitResult::QUEUE_FULL;
    case SendResult::FAILED:
    default:
      return group_command_logic::HubSubmitResult::FAILED;
  }
}

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
    // Native multi-dest: single RF packet to all members (same channel/remote).
    // If the hub cannot accept it, fall back to member queues so normal cover
    // dispatch retry/aging semantics take over instead of silently dropping it.
    t_elero_command cmd{};
    this->build_group_command_(cmd, cmd_byte);
    auto result = to_group_submit_result(this->parent_->send_command(&cmd));
    if (group_command_logic::should_increment_group_counter(result)) {
      this->group_counter_ = group_command_logic::next_group_counter(this->group_counter_);
      ESP_LOGD(TAG, "Sent native group command 0x%02x to %d dests", cmd_byte, (int) this->members_.size());
      return;
    }
    ESP_LOGW(TAG, "Native group command 0x%02x was not accepted by hub, queueing %d member commands",
             cmd_byte, (int) this->members_.size());
    for (auto *member : this->members_) {
      member->enqueue_command(cmd_byte);
    }
  } else if (this->parent_ != nullptr) {
    // Direct TX: enqueue each member's command directly to the hub's TX queue.
    // Queue-full/failed sends fall back to the member's own command queue so
    // per-cover dispatch delay, retry, and stale-queue rules remain the retry path.
    uint8_t sent = 0;
    uint8_t queued = 0;
    for (auto *member : this->members_) {
      t_elero_command cmd = member->build_tx_command(cmd_byte);
      auto result = to_group_submit_result(this->parent_->send_command(&cmd));
      if (group_command_logic::should_fallback_to_member_queues(result)) {
        member->enqueue_command(cmd_byte);
        queued++;
      } else {
        sent++;
      }
    }
    ESP_LOGD(TAG, "Direct group command 0x%02x: sent=%d queued=%d", cmd_byte, sent, queued);
  } else {
    // Last resort fallback: use per-cover command queues.
    for (auto *member : this->members_) {
      member->enqueue_command(cmd_byte);
    }
  }
}

bool EleroGroupCover::can_use_native_group_() const {
  if (this->members_.size() < 2 || this->members_.size() > ELERO_MAX_DESTS)
    return false;
  auto first_profile = this->members_[0]->get_command_profile();
  for (size_t i = 1; i < this->members_.size(); i++) {
    if (!command_profile::can_share_native_group(first_profile, this->members_[i]->get_command_profile()))
      return false;
  }
  return true;
}

void EleroGroupCover::build_group_command_(t_elero_command &cmd, uint8_t cmd_byte) {
  auto first_profile = this->members_[0]->get_command_profile();
  cmd.counter = this->group_counter_;
  cmd.remote_addr = first_profile.remote_address;
  cmd.channel = first_profile.channel;
  cmd.pck_inf[0] = first_profile.pck_inf[0];
  cmd.pck_inf[1] = first_profile.pck_inf[1];
  cmd.hop = first_profile.hop;
  cmd.payload[0] = first_profile.payload_1;
  cmd.payload[1] = first_profile.payload_2;
  cmd.payload[4] = cmd_byte;
  cmd.num_dests = static_cast<uint8_t>(this->members_.size());
  cmd.blind_addr = first_profile.blind_address;
  for (size_t i = 0; i < this->members_.size(); i++) {
    cmd.dest_addrs[i] = this->members_[i]->get_command_profile().blind_address;
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
