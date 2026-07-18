#include "EleroGroupCover.h"
#include "../elero/elero_group_delivery_policy.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace elero {

static const char *const TAG = "elero.group";

void EleroGroupCover::setup() {
  this->native_group_ = this->can_use_native_group_();
  if (this->native_group_)
    this->native_delivery_.configure(this->build_native_config_());
  ESP_LOGD(TAG, "Group '%s' with %d members, native_group=%s, hide_members=%s",
           this->get_name().c_str(), (int) this->members_.size(),
           this->native_group_ ? "true" : "false", this->hide_members_ ? "true" : "false");
  this->update_position_();
}

void EleroGroupCover::loop() {
  const uint32_t now = millis();
  if (this->native_group_ && this->parent_ != nullptr && !this->parent_->is_failed()) {
    auto outcome = this->native_delivery_.advance(
        now, this->parent_->get_send_delay(), this->parent_->get_send_repeats(),
        [this](const t_elero_command &packet, bool priority) {
          t_elero_command copy = packet;
          if (!priority)
            return this->parent_->send_command(&copy);
          return this->parent_->send_command_priority(&copy);
        });
    this->handle_native_outcome_(outcome);
  }
  if (now - this->last_position_update_ >= 1000) {
    this->last_position_update_ = now;
    this->update_position_();
  }
}

void EleroGroupCover::dump_config() {
  ESP_LOGCONFIG(TAG, "Elero Group Cover '%s':", this->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Members: %d", (int) this->members_.size());
  ESP_LOGCONFIG(TAG, "  Native multi-dest: %s", this->native_group_ ? "yes" : "no (incompatible profiles)");
  ESP_LOGCONFIG(TAG, "  Hide members: %s", YESNO(this->hide_members_));
  for (auto *member : this->members_)
    ESP_LOGCONFIG(TAG, "    - 0x%06x (ch=%d, remote=0x%06x)", member->get_blind_address(),
                  member->get_channel(), member->get_remote_address());
}

cover::CoverTraits EleroGroupCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(false);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(this->assumed_state_);
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
    this->submit_group_intent_({CommandIntentKind::STOP, 0});
    this->current_operation = cover::COVER_OPERATION_IDLE;
    this->publish_state();
  }
  if (call.get_position().has_value()) {
    const float pos = *call.get_position();
    if (pos == cover::COVER_OPEN) {
      this->submit_group_intent_({CommandIntentKind::OPEN, 0});
      this->current_operation = cover::COVER_OPERATION_OPENING;
    } else if (pos == cover::COVER_CLOSED) {
      this->submit_group_intent_({CommandIntentKind::CLOSE, 0});
      this->current_operation = cover::COVER_OPERATION_CLOSING;
    } else {
      for (auto *member : this->members_) {
        const auto kind = pos > member->get_cover_position() ? CommandIntentKind::OPEN
                                                              : CommandIntentKind::CLOSE;
        member->submit_intent({kind, 0});
      }
      this->current_operation = cover::COVER_OPERATION_OPENING;
    }
    this->publish_state();
  }
  if (call.get_tilt().has_value() && *call.get_tilt() > 0)
    this->submit_group_intent_({CommandIntentKind::TILT, 0});
  if (call.get_toggle().has_value()) {
    if (this->current_operation != cover::COVER_OPERATION_IDLE) {
      this->submit_group_intent_({CommandIntentKind::STOP, 0});
      this->current_operation = cover::COVER_OPERATION_IDLE;
    } else {
      this->submit_group_intent_({CommandIntentKind::OPEN, 0});
      this->current_operation = cover::COVER_OPERATION_OPENING;
    }
    this->publish_state();
  }
}

void EleroGroupCover::submit_group_intent_(const CommandIntent &intent) {
  if (!this->native_group_) {
    this->submit_to_members_(intent);
    return;
  }
  const auto result = this->native_delivery_.submit(intent);
  if (group_delivery_policy::after_native_submit(result) == group_delivery_policy::Route::MEMBERS) {
    ESP_LOGW(TAG, "Native group command queue full; using member delivery");
    this->submit_to_members_(intent);
  }
}

void EleroGroupCover::submit_to_members_(const CommandIntent &intent) {
  for (auto *member : this->members_) {
    if (member->submit_intent(intent) == IntentSubmitResult::REJECTED)
      ESP_LOGW(TAG, "Member command queue full for blind 0x%06x", member->get_blind_address());
  }
}

void EleroGroupCover::handle_native_outcome_(const DeliveryOutcome &outcome) {
  if (outcome.event == DeliveryEvent::DROPPED) {
    this->parent_->increment_tx_drop_count();
    if (group_delivery_policy::after_native_outcome(outcome) == group_delivery_policy::Route::MEMBERS) {
      ESP_LOGW(TAG, "Native group delivery failed before RF acceptance; falling back to members");
      this->submit_to_members_(outcome.intent);
    } else {
      ESP_LOGE(TAG, "Native group delivery failed after partial RF acceptance; not fanning out");
    }
  } else if (outcome.event == DeliveryEvent::STALE_CLEARED) {
    ESP_LOGW(TAG, "Stale native group command queue cleared without fan-out");
    this->parent_->increment_tx_drop_count();
  }
}

bool EleroGroupCover::can_use_native_group_() const {
  std::vector<CommandDeliveryConfig> configs;
  configs.reserve(this->members_.size());
  for (auto *member : this->members_)
    configs.push_back(member->get_command_delivery_config());
  return group_delivery_policy::native_profiles_compatible(configs);
}

CommandDeliveryConfig EleroGroupCover::build_native_config_() const {
  CommandDeliveryConfig config = this->members_[0]->get_command_delivery_config();
  config.destination_count = static_cast<uint8_t>(this->members_.size());
  for (size_t i = 0; i < this->members_.size(); i++)
    config.destinations[i] = this->members_[i]->get_command_delivery_config().profile.blind_address;
  return config;
}

void EleroGroupCover::update_position_() {
  if (this->members_.empty())
    return;
  float sum = 0.0f;
  int count = 0;
  bool any_opening = false;
  bool any_closing = false;
  for (auto *member : this->members_) {
    const float pos = member->get_cover_position();
    if (!std::isnan(pos)) {
      sum += pos;
      count++;
    }
    const char *op = member->get_operation_str();
    if (op[0] == 'o') any_opening = true;
    if (op[0] == 'c') any_closing = true;
  }
  const float new_pos = count > 0 ? sum / count : 0.5f;
  cover::CoverOperation new_op = cover::COVER_OPERATION_IDLE;
  if (any_opening) new_op = cover::COVER_OPERATION_OPENING;
  else if (any_closing) new_op = cover::COVER_OPERATION_CLOSING;
  if (std::abs(new_pos - this->position) > 0.01f || new_op != this->current_operation) {
    this->position = new_pos;
    this->current_operation = new_op;
    this->publish_state(false);
  }
}

}  // namespace elero
}  // namespace esphome
