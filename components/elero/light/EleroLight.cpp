#include "EleroLight.h"
#include "../elero_light_logic.h"
#include "esphome/core/log.h"

namespace esphome {
namespace elero {

using namespace esphome::light;

static const char *const TAG = "elero.light";

void EleroLight::dump_config() {
  ESP_LOGCONFIG(TAG, "Elero Light:");
  ESP_LOGCONFIG(TAG, "  Blind Address: 0x%06x", this->command_.blind_addr);
  ESP_LOGCONFIG(TAG, "  Remote Address: 0x%06x", this->command_.remote_addr);
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->command_.channel);
  ESP_LOGCONFIG(TAG, "  Hop: 0x%02x", this->command_.hop);
  ESP_LOGCONFIG(TAG, "  pck_inf1: 0x%02x, pck_inf2: 0x%02x",
                this->command_.pck_inf[0], this->command_.pck_inf[1]);
  if (this->dim_duration_ > 0)
    ESP_LOGCONFIG(TAG, "  Dim Duration: %dms", this->dim_duration_);
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

  bool new_on = state->current_values.is_on();
  float new_brightness = state->current_values.get_brightness();
  if (!new_on) {
    if (!intent_was_accepted(this->submit_intent({CommandIntentKind::OFF, 0})))
      return;
    this->is_on_ = false;
    this->is_dimming_ = false;
    this->brightness_ = 0.0f;
    return;
  }

  // Light should be on
  this->is_on_ = true;

  if (this->dim_duration_ == 0) {
    // No brightness support: just toggle on
    if (!intent_was_accepted(this->submit_intent({CommandIntentKind::ON, 0})))
      return;
    this->brightness_ = 1.0f;
    return;
  }

  // Brightness control via timing
  this->target_brightness_ = new_brightness;
  this->is_dimming_ = false;

  if (new_brightness >= 1.0f) {
    // Full brightness shortcut
    if (!intent_was_accepted(this->submit_intent({CommandIntentKind::ON, 0})))
      return;
    this->brightness_ = 1.0f;
    return;
  }

  if (this->brightness_ < 0.01f) {
    // Currently off; turn on to full first, then dim down
    if (!intent_was_accepted(this->submit_intent({CommandIntentKind::ON, 0})))
      return;
    this->brightness_ = 1.0f;
    // Now fall through and initiate dim-down
  }

  if (new_brightness > this->brightness_ + 0.01f) {
    ESP_LOGD(TAG, "Dimming up 0x%06x from %.2f to %.2f",
             this->command_.blind_addr, this->brightness_, new_brightness);
    if (!intent_was_accepted(this->submit_intent({CommandIntentKind::DIM_UP, 0})))
      return;
    this->is_dimming_ = true;
    this->dim_up_ = true;
    this->dimming_start_ = millis();
    this->last_recompute_time_ = millis();
  } else if (new_brightness < this->brightness_ - 0.01f) {
    ESP_LOGD(TAG, "Dimming down 0x%06x from %.2f to %.2f",
             this->command_.blind_addr, this->brightness_, new_brightness);
    if (!intent_was_accepted(this->submit_intent({CommandIntentKind::DIM_DOWN, 0})))
      return;
    this->is_dimming_ = true;
    this->dim_up_ = false;
    this->dimming_start_ = millis();
    this->last_recompute_time_ = millis();
  }
  // If within tolerance: no action needed, current level is already correct
}

void EleroLight::loop() {
  const uint32_t now = millis();

  this->handle_commands(now);

  if (this->is_dimming_ && this->dim_duration_ > 0) {
    this->recompute_brightness();

    bool at_target;
    if (this->dim_up_) {
      at_target = this->brightness_ >= this->target_brightness_;
    } else {
      at_target = this->brightness_ <= this->target_brightness_;
    }

    if (at_target) {
      this->submit_intent({CommandIntentKind::STOP, 0});
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

void EleroLight::handle_commands(uint32_t now) {
  if (this->parent_->is_failed())
    return;
  auto outcome = this->delivery_.advance(
      now, this->parent_->get_send_delay(), this->parent_->get_send_repeats(),
      [this](const t_elero_command &packet, bool priority) {
        t_elero_command copy = packet;
        if (!priority)
          return this->parent_->send_command(&copy);
        if (this->parent_->send_command_priority(&copy))
          return SendResult::OK;
        return this->parent_->is_failed() ? SendResult::FAILED : SendResult::QUEUE_FULL;
      });
  this->handle_delivery_outcome_(outcome);
}

void EleroLight::handle_delivery_outcome_(const DeliveryOutcome &outcome) {
  if (outcome.event == DeliveryEvent::DROPPED) {
    ESP_LOGE(TAG, "Delivery retries exhausted for light 0x%06x", this->command_.blind_addr);
    this->parent_->increment_tx_drop_count();
  } else if (outcome.event == DeliveryEvent::STALE_CLEARED) {
    ESP_LOGW(TAG, "Stale Command queue cleared for light 0x%06x", this->command_.blind_addr);
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
  auto result = this->delivery_.submit(intent);
  if (result == IntentSubmitResult::REJECTED) {
    ESP_LOGW(TAG, "Command queue full for light 0x%06x", this->command_.blind_addr);
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
  ESP_LOGV(TAG, "Got state: 0x%02x for light 0x%06x",
           state, this->command_.blind_addr);

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
    ESP_LOGW(TAG, "Light 0x%06x reports BLOCKING", this->command_.blind_addr);
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "blocking");
#endif
  } else if (state == ELERO_STATE_OVERHEATED) {
    ESP_LOGW(TAG, "Light 0x%06x reports OVERHEATED", this->command_.blind_addr);
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "overheated");
#endif
  } else if (state == ELERO_STATE_TIMEOUT) {
    ESP_LOGW(TAG, "Light 0x%06x reports TIMEOUT", this->command_.blind_addr);
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "timeout");
#endif
  }
}

}  // namespace elero
}  // namespace esphome
