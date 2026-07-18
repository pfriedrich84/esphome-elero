#include "EleroGroupCover.h"
#include "../elero/elero_group_delivery_policy.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace elero {

static const char *const TAG = "elero.group";

void EleroGroupCover::setup() {
  this->native_group_ = this->can_use_native_group_();
  if (this->native_group_) {
    this->native_delivery_.configure(this->build_native_config_());
    this->native_delivery_.set_outcome_callback(
        [this](const DeliveryOutcome &outcome) { this->handle_native_outcome_(outcome); });
    std::vector<CommandIntentDelivery *> member_deliveries;
    member_deliveries.reserve(this->members_.size());
    for (auto *member : this->members_)
      member_deliveries.push_back(member->get_command_delivery());
    if (this->parent_ == nullptr || !this->parent_->register_command_delivery(&this->native_delivery_) ||
        !this->native_delivery_.set_native_fallback(member_deliveries.data(), member_deliveries.size())) {
      if (this->parent_ != nullptr)
        this->parent_->unregister_command_delivery(&this->native_delivery_);
      ESP_LOGW(TAG, "Native group coordinator wiring failed; using atomic member delivery");
      this->native_group_ = false;
    }
  }
  ESP_LOGD(TAG, "Group '%s' with %d members, native_group=%s, hide_members=%s",
           this->get_name().c_str(), (int) this->members_.size(),
           this->native_group_ ? "true" : "false", this->hide_members_ ? "true" : "false");
  this->update_position_();
}

void EleroGroupCover::loop() {
  const uint32_t now = millis();
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
  if (call.get_stop() && intent_was_accepted(
          this->submit_group_intent_({CommandIntentKind::STOP, 0}))) {
    this->current_operation = cover::COVER_OPERATION_IDLE;
    this->publish_state();
  }
  if (call.get_position().has_value()) {
    const float pos = *call.get_position();
    if (pos == cover::COVER_OPEN) {
      if (intent_was_accepted(this->submit_group_intent_({CommandIntentKind::OPEN, 0})))
        this->current_operation = cover::COVER_OPERATION_OPENING;
    } else if (pos == cover::COVER_CLOSED) {
      if (intent_was_accepted(this->submit_group_intent_({CommandIntentKind::CLOSE, 0})))
        this->current_operation = cover::COVER_OPERATION_CLOSING;
    } else {
      std::vector<CommandIntent> intents;
      intents.reserve(this->members_.size());
      for (auto *member : this->members_) {
        const auto kind = pos > member->get_cover_position() ? CommandIntentKind::OPEN
                                                              : CommandIntentKind::CLOSE;
        intents.push_back({kind, 0});
      }
      if (intent_was_accepted(this->submit_member_targets_(intents)))
        this->current_operation = cover::COVER_OPERATION_OPENING;
    }
    this->publish_state();
  }
  if (call.get_tilt().has_value() && *call.get_tilt() > 0)
    this->submit_group_intent_({CommandIntentKind::TILT, 0});
  if (call.get_toggle().has_value()) {
    if (this->current_operation != cover::COVER_OPERATION_IDLE) {
      if (intent_was_accepted(this->submit_group_intent_({CommandIntentKind::STOP, 0})))
        this->current_operation = cover::COVER_OPERATION_IDLE;
    } else {
      if (intent_was_accepted(this->submit_group_intent_({CommandIntentKind::OPEN, 0})))
        this->current_operation = cover::COVER_OPERATION_OPENING;
    }
    this->publish_state();
  }
}

IntentSubmitResult EleroGroupCover::submit_group_intent_(const CommandIntent &intent) {
  if (!this->native_group_)
    return this->submit_to_members_(intent);
  const auto result = this->native_delivery_.submit(intent, millis());
  if (group_delivery_policy::reject_native_submit_without_fanout(result)) {
    // Keep all compatible group work in the native lane. Sending only this
    // newest command through members would overtake its older native backlog.
    ESP_LOGW(TAG, "Native group command queue full; rejecting newest group command");
    if (this->parent_ != nullptr)
      this->parent_->increment_tx_drop_count();
  }
  return result;
}

IntentSubmitResult EleroGroupCover::submit_to_members_(const CommandIntent &intent) {
  std::vector<CommandIntent> intents(this->members_.size(), intent);
  return this->submit_member_targets_(intents);
}

IntentSubmitResult EleroGroupCover::submit_member_targets_(const std::vector<CommandIntent> &intents) {
  if (intents.size() != this->members_.size() || intents.empty())
    return IntentSubmitResult::REJECTED;
  std::vector<CommandIntentDelivery::AtomicIntentTarget> targets;
  targets.reserve(this->members_.size());
  for (size_t i = 0; i < this->members_.size(); i++)
    targets.push_back({this->members_[i]->get_command_delivery(), intents[i],
                       this->members_[i]->should_defer_intent(intents[i])});
  const auto result = CommandIntentDelivery::submit_atomic(targets.data(), targets.size(), millis());
  if (result == IntentSubmitResult::REJECTED) {
    ESP_LOGW(TAG, "Atomic member command rejected; no member queue was changed");
    if (this->parent_ != nullptr)
      this->parent_->increment_tx_drop_count();
  }
  return result;
}

void EleroGroupCover::handle_native_outcome_(const DeliveryOutcome &outcome) {
  if (outcome.event == DeliveryEvent::DROPPED ||
      outcome.event == DeliveryEvent::FALLBACK_MEMBER_DROPPED) {
    this->parent_->increment_tx_drop_count();
    if (outcome.had_partial_delivery)
      ESP_LOGE(TAG, "Native group delivery failed after partial RF acceptance; not fanning out");
    else
      ESP_LOGE(TAG, "Native group/member fallback delivery exhausted retries");
  } else if (outcome.event == DeliveryEvent::FALLBACK_STARTED) {
    ESP_LOGW(TAG, "Native group delivery failed before RF acceptance; falling back to members");
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
