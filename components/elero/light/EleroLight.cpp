#include "EleroLight.h"
#include "../elero_light_logic.h"
#include "../elero_timed_action.h"
#include "esphome/core/log.h"

namespace esphome {
namespace elero {

using namespace esphome::light;

static const char *const TAG = "elero.light";

void EleroLight::dump_config() {
  ESP_LOGCONFIG(TAG, "Elero Light:");
  ESP_LOGCONFIG(TAG, "  Blind Address: 0x%06lx", static_cast<unsigned long>(this->command_.blind_addr));
  ESP_LOGCONFIG(TAG, "  Remote Address: 0x%06lx", static_cast<unsigned long>(this->command_.remote_addr));
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->command_.channel);
  ESP_LOGCONFIG(TAG, "  Hop: 0x%02x", this->command_.hop);
  ESP_LOGCONFIG(TAG, "  pck_inf1: 0x%02x, pck_inf2: 0x%02x",
                this->command_.pck_inf[0], this->command_.pck_inf[1]);
  if (this->dim_duration_ > 0)
    ESP_LOGCONFIG(TAG, "  Dim Duration: %lums", static_cast<unsigned long>(this->dim_duration_));
  ESP_LOGCONFIG(TAG, "  cmd_on: 0x%02x, cmd_off: 0x%02x, cmd_stop: 0x%02x",
                this->command_on_, this->command_off_, this->command_stop_);
  ESP_LOGCONFIG(TAG, "  cmd_dim_up: 0x%02x, cmd_dim_down: 0x%02x",
                this->command_dim_up_, this->command_dim_down_);
}

void EleroLight::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Elero parent not configured");
    this->mark_failed();
    return;
  }
  this->delivery_.configure(this->get_command_delivery_config());
  this->delivery_.set_outcome_callback(
      [this](const DeliveryOutcome &outcome) { this->handle_delivery_outcome_(outcome); });
  if (!this->parent_->register_command_delivery(&this->delivery_)) {
    ESP_LOGE(TAG, "Failed to register command delivery");
    this->mark_failed();
    return;
  }
  this->parent_->register_light(this);
  // Queue an initial status CHECK so the text sensor populates shortly after
  // boot instead of waiting for the first external event.
  if (this->command_check_ != 0x00) {
    this->submit_intent({CommandIntentKind::CHECK, 0});
  }
}

LightTraits EleroLight::get_traits() {
  auto traits = LightTraits();
  if (this->dim_duration_ > 0) {
    traits.set_supported_color_modes({ColorMode::BRIGHTNESS});
  } else {
    traits.set_supported_color_modes({ColorMode::ON_OFF});
  }
  return traits;
}

void EleroLight::write_state(LightState *state) {
  if (this->ignore_write_state_) return;
  this->state_ = state;

  const bool new_on = state->current_values.is_on();
  const float new_brightness = state->current_values.get_brightness();
  std::vector<CommandIntent> intents;
  bool start_dimming = false;
  bool dim_up = true;
  float accepted_brightness = this->brightness_;

  if (!new_on) {
    intents.push_back({CommandIntentKind::OFF, 0});
    accepted_brightness = 0.0f;
  } else if (this->dim_duration_ == 0 || new_brightness >= 1.0f) {
    intents.push_back({CommandIntentKind::ON, 0});
    accepted_brightness = 1.0f;
  } else if (this->brightness_ < 0.01f) {
    // This pair must be accepted atomically: DIM_DOWN without its preceding ON
    // would act on an unknown starting brightness.
    intents.push_back({CommandIntentKind::ON, 0});
    intents.push_back({CommandIntentKind::DIM_DOWN, 0});
    accepted_brightness = 1.0f;
    start_dimming = true;
    dim_up = false;
  } else if (new_brightness > this->brightness_ + 0.01f) {
    intents.push_back({CommandIntentKind::DIM_UP, 0});
    start_dimming = true;
    dim_up = true;
  } else if (new_brightness < this->brightness_ - 0.01f) {
    intents.push_back({CommandIntentKind::DIM_DOWN, 0});
    start_dimming = true;
    dim_up = false;
  }

  if (!intents.empty() && !intent_was_accepted(this->submit_intents_(intents)))
    return;

  // Commit local state only after the complete RF intent transaction has queue
  // capacity. A rejected batch leaves all prior state untouched.
  this->is_on_ = new_on;
  this->target_brightness_ = new_brightness;
  this->brightness_ = accepted_brightness;
  this->is_dimming_ = start_dimming;
  this->pending_dimming_start_ = start_dimming;
  if (start_dimming) {
    ESP_LOGD(TAG, "Dimming %s 0x%06lx from %.2f to %.2f", dim_up ? "up" : "down",
             static_cast<unsigned long>(this->command_.blind_addr), this->brightness_, new_brightness);
    this->dim_up_ = dim_up;
    this->pending_dimming_kind_ = dim_up ? CommandIntentKind::DIM_UP : CommandIntentKind::DIM_DOWN;
    this->dimming_start_ = 0;
    this->last_recompute_time_ = 0;
  }
}

void EleroLight::loop() {
  const uint32_t now = millis();

  if (this->is_dimming_ && !this->pending_dimming_start_ && this->dim_duration_ > 0) {
    this->recompute_brightness();

    bool at_target;
    if (this->dim_up_) {
      at_target = this->brightness_ >= this->target_brightness_;
    } else {
      at_target = this->brightness_ <= this->target_brightness_;
    }

    if (at_target && intent_was_accepted(this->submit_intent({CommandIntentKind::STOP, 0}))) {
      this->brightness_ = this->target_brightness_;
      this->is_dimming_ = false;
    }

    // Publish estimated brightness every second while dimming
    if (light_logic::should_publish_dimming(now, this->last_publish_, 1000)) {
      if (this->state_ != nullptr)
        this->state_->publish_state();
      this->last_publish_ = now;
    }
  }
}

void EleroLight::handle_delivery_outcome_(const DeliveryOutcome &outcome) {
  const bool transmitted_dimming = outcome.first_transmission &&
      (outcome.intent.kind == CommandIntentKind::DIM_UP ||
       outcome.intent.kind == CommandIntentKind::DIM_DOWN);
  if (transmitted_dimming ||
      should_start_timed_action(this->pending_dimming_start_, this->pending_dimming_kind_, outcome)) {
    if (this->pending_dimming_kind_ == outcome.intent.kind)
      this->pending_dimming_start_ = false;
    this->dim_up_ = outcome.intent.kind == CommandIntentKind::DIM_UP;
    this->dimming_start_ = outcome.transmitted_at_ms != 0 ? outcome.transmitted_at_ms : millis();
    this->last_recompute_time_ = this->dimming_start_;
  }

  if (outcome.event == DeliveryEvent::DROPPED) {
    ESP_LOGE(TAG, "Delivery retries exhausted for light 0x%06lx",
             static_cast<unsigned long>(this->command_.blind_addr));
    this->parent_->increment_tx_drop_count();
    if (this->pending_dimming_start_ && outcome.intent.kind == this->pending_dimming_kind_) {
      this->pending_dimming_start_ = false;
      this->is_dimming_ = false;
    }
  } else if (outcome.event == DeliveryEvent::STALE_CLEARED) {
    ESP_LOGW(TAG, "Stale Command queue cleared for light 0x%06lx",
             static_cast<unsigned long>(this->command_.blind_addr));
    this->parent_->increment_tx_drop_count();
    if (this->pending_dimming_start_) {
      this->pending_dimming_start_ = false;
      this->is_dimming_ = false;
    }
  }
#ifdef USE_TEXT_SENSOR
  if (this->queue_full_published_ && outcome.queue_size == 0)
    this->queue_full_published_ = false;
#endif
}

CommandDeliveryConfig EleroLight::get_command_delivery_config() const {
  CommandDeliveryConfig config{};
  config.profile.blind_address = this->command_.blind_addr;
  config.profile.remote_address = this->command_.remote_addr;
  config.profile.channel = this->command_.channel;
  config.profile.pck_inf[0] = this->command_.pck_inf[0];
  config.profile.pck_inf[1] = this->command_.pck_inf[1];
  config.profile.hop = this->command_.hop;
  config.profile.payload_1 = this->command_.payload[0];
  config.profile.payload_2 = this->command_.payload[1];
  config.mapping.on = this->command_on_;
  config.mapping.off = this->command_off_;
  config.mapping.dim_up = this->command_dim_up_;
  config.mapping.dim_down = this->command_dim_down_;
  config.mapping.stop = this->command_stop_;
  config.mapping.check = this->command_check_;
  return config;
}

IntentSubmitResult EleroLight::submit_intent(const CommandIntent &intent) {
  return this->submit_intents_({intent});
}

IntentSubmitResult EleroLight::submit_intents_(const std::vector<CommandIntent> &intents) {
  auto result = this->delivery_.submit_batch(intents.data(), intents.size(), millis());
  if (result == IntentSubmitResult::REJECTED) {
    ESP_LOGW(TAG, "Command queue full for light 0x%06lx",
             static_cast<unsigned long>(this->command_.blind_addr));
#ifdef USE_TEXT_SENSOR
    if (!this->queue_full_published_) {
      this->parent_->publish_text_sensor_state(this->command_.blind_addr, "queue_full");
      this->queue_full_published_ = true;
    }
#endif
  }
  return result;
}

void EleroLight::schedule_immediate_poll() {
  uint32_t now = millis();
  if ((now - this->last_immediate_poll_ms_) >= ELERO_IMMEDIATE_POLL_MIN_INTERVAL_MS &&
      intent_was_accepted(this->submit_intent({CommandIntentKind::CHECK, 0})))
    this->last_immediate_poll_ms_ = now;
}

void EleroLight::recompute_brightness() {
  if (!this->is_dimming_)
    return;

  const uint32_t now = millis();
  const uint32_t elapsed = now - this->last_recompute_time_;

  // Sanity check: skip recompute if elapsed time is implausibly large
  // (e.g., millis() wraparound glitch or stale last_recompute_time_)
  if (elapsed > ELERO_TIMEOUT_MOVEMENT) {
    this->last_recompute_time_ = now;
    return;
  }

  this->brightness_ = light_logic::recompute_brightness(this->brightness_, this->dim_up_,
                                                        this->dim_duration_, elapsed,
                                                        ELERO_TIMEOUT_MOVEMENT);
  this->last_recompute_time_ = now;
}

void EleroLight::set_rx_state(uint8_t state) {
  ESP_LOGV(TAG, "Got state: 0x%02x for light 0x%06lx",
           state, static_cast<unsigned long>(this->command_.blind_addr));

  if (state == ELERO_STATE_ON) {
    if (!this->is_on_) {
      this->is_on_ = true;
      this->brightness_ = 1.0f;
      if (this->state_ != nullptr) {
        this->ignore_write_state_ = true;
        auto call = this->state_->make_call();
        call.set_state(true);
        if (this->dim_duration_ > 0)
          call.set_brightness(1.0f);
        call.perform();
        this->ignore_write_state_ = false;
      }
    }
  } else if (state == ELERO_STATE_OFF) {
    if (this->is_on_) {
      this->is_on_ = false;
      this->brightness_ = 0.0f;
      if (this->state_ != nullptr) {
        this->ignore_write_state_ = true;
        auto call = this->state_->make_call();
        call.set_state(false);
        call.perform();
        this->ignore_write_state_ = false;
      }
    }
  } else if (state == ELERO_STATE_BLOCKING) {
    ESP_LOGW(TAG, "Light 0x%06lx reports BLOCKING", static_cast<unsigned long>(this->command_.blind_addr));
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "blocking");
#endif
  } else if (state == ELERO_STATE_OVERHEATED) {
    ESP_LOGW(TAG, "Light 0x%06lx reports OVERHEATED", static_cast<unsigned long>(this->command_.blind_addr));
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "overheated");
#endif
  } else if (state == ELERO_STATE_TIMEOUT) {
    ESP_LOGW(TAG, "Light 0x%06lx reports TIMEOUT", static_cast<unsigned long>(this->command_.blind_addr));
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "timeout");
#endif
  }
}

}  // namespace elero
}  // namespace esphome
