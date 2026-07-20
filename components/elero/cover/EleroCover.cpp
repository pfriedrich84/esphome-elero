#include "EleroCover.h"
#include "../elero_cover_logic.h"
#include "../elero_timed_action.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace elero {

using namespace esphome::cover;

static const char *const TAG = "elero.cover";

void EleroCover::dump_config() {
  LOG_COVER("", "Elero Cover", this);
  ESP_LOGCONFIG(TAG, "  Blind Address: 0x%06x", this->command_.blind_addr);
  ESP_LOGCONFIG(TAG, "  Remote Address: 0x%06x", this->command_.remote_addr);
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->command_.channel);
  ESP_LOGCONFIG(TAG, "  Hop: 0x%02x", this->command_.hop);
  ESP_LOGCONFIG(TAG, "  pck_inf1: 0x%02x, pck_inf2: 0x%02x", this->command_.pck_inf[0], this->command_.pck_inf[1]);
  if (this->open_duration_ > 0)
    ESP_LOGCONFIG(TAG, "  Open Duration: %dms", this->open_duration_);
  if (this->close_duration_ > 0)
    ESP_LOGCONFIG(TAG, "  Close Duration: %dms", this->close_duration_);
  ESP_LOGCONFIG(TAG, "  Poll Interval: %dms", this->poll_intvl_);
  ESP_LOGCONFIG(TAG, "  Supports Tilt: %s", YESNO(this->supports_tilt_));
  ESP_LOGCONFIG(TAG, "  Assumed State: %s", YESNO(this->assumed_state_));
}

void EleroCover::setup() {
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
  this->parent_->register_cover(this);
  // Apply stagger offset: shift initial last_poll_ backwards so the first poll
  // is delayed by poll_offset_ milliseconds relative to other covers.
  this->last_poll_ = millis() - this->poll_intvl_ + this->poll_offset_;
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    if((this->open_duration_ > 0) && (this->close_duration_ > 0))
      this->position = 0.5f;
  }
  // Queue an initial status CHECK so the text sensor populates shortly after
  // boot instead of waiting for the first poll_interval to elapse.
  // The poll_offset_ stagger ensures covers don't all poll simultaneously.
  this->submit_intent({CommandIntentKind::CHECK, 0});
}

void EleroCover::loop() {
  uint32_t intvl = this->poll_intvl_;
  uint32_t now = millis();
  if(this->current_operation != COVER_OPERATION_IDLE) {
    // Only poll frequently during movement when position tracking is NOT
    // active (no open/close duration).  With durations configured, the
    // cover dead-reckons position and auto-stops at target — the blind's
    // own status broadcasts (caught by set_rx_state) are sufficient.
    // This avoids burning message counter values on redundant CHECKs.
    if (!this->pending_movement_start_ &&
        (this->open_duration_ == 0 || this->close_duration_ == 0) &&
        (now - this->movement_start_) < ELERO_TIMEOUT_MOVEMENT) {
      intvl = ELERO_POLL_INTERVAL_MOVING + (this->poll_offset_ % ELERO_POLL_INTERVAL_MOVING);
    }
  }

  if (cover_logic::should_poll(now, this->last_poll_, intvl) &&
      intent_was_accepted(this->submit_intent({CommandIntentKind::CHECK, 0}))) {
    this->last_poll_ = now;
  }

  if(this->post_movement_poll_at_ > 0 && now >= this->post_movement_poll_at_) {
    this->post_movement_poll_at_ = 0;
    if (intent_was_accepted(this->submit_intent({CommandIntentKind::CHECK, 0}))) {
      ESP_LOGD(TAG, "Post-movement status poll for blind 0x%06x", this->command_.blind_addr);
      this->last_poll_ = now;
    }
  }

  // Stop verification: poll motor to confirm it actually stopped
  if (this->stop_verify_at_ > 0 && now >= this->stop_verify_at_) {
    if (this->stop_verify_retries_ < ELERO_STOP_VERIFY_MAX_RETRIES) {
      this->stop_verify_retries_++;
      ESP_LOGD(TAG, "Stop verify poll #%d for blind 0x%06x",
               this->stop_verify_retries_, this->command_.blind_addr);
      this->submit_intent({CommandIntentKind::CHECK, 0});
      // Reschedule in case no RF response arrives (prevents verification stall)
      this->stop_verify_at_ = now + ELERO_STOP_VERIFY_DELAY_MS;
    } else {
      // Exhausted retries — give up verification
      ESP_LOGW(TAG, "Stop verification exhausted %d retries for blind 0x%06x",
               ELERO_STOP_VERIFY_MAX_RETRIES, this->command_.blind_addr);
      this->stop_verify_at_ = 0;
      this->stop_trigger_ms_ = 0;
      this->finish_stop_verification_();
#ifdef USE_TEXT_SENSOR
      this->parent_->publish_text_sensor_state(this->command_.blind_addr, "stop_failed");
#endif
      // Position is uncertain after failed stop verification.
      // Keep the last known position rather than setting NAN — NAN breaks
      // all position-based commands and creates an irrecoverable state where
      // current_operation gets stuck, the poll queue fills, and group
      // commands are silently dropped.
      this->current_operation = cover::COVER_OPERATION_IDLE;
      // Clear queued polls to prevent rapid-fire flooding while preserving
      // deferred user movement intents.
      this->delivery_.discard_checks();
      // Cooldown: prevent delivery from sending anything for 3s,
      // giving the blind time to settle after the stop command storm.
      this->command_cooldown_until_ = now + 3000;
      this->delivery_.postpone_until(this->command_cooldown_until_);
      this->publish_state(false);
    }
  }

  if (!this->stop_verification_active_.load())
    this->delivery_.release_deferred();

  if (!this->pending_movement_start_ &&
      (this->current_operation != COVER_OPERATION_IDLE) &&
      (this->open_duration_ > 0) && (this->close_duration_ > 0)) {
    this->recompute_position();
    if(this->is_at_target()) {
      ESP_LOGI(TAG, "Blind 0x%06x reached target (pos=%.2f, target=%.2f), sending stop",
               this->command_.blind_addr, this->position, this->target_position_);
      if (!this->pending_stop_transition_) {
        // Queueing locally is not proof that the hub accepted the priority
        // packet. Keep tracking movement until advance() reports first RF queue
        // acceptance; only then transition to idle and start verification.
        this->stop_trigger_position_ = this->position;
        this->stop_trigger_ms_ = now;
        this->stop_verification_active_.store(true);
        if (!intent_was_accepted(this->submit_intent({CommandIntentKind::STOP, 0}))) {
          this->stop_trigger_ms_ = 0;
          this->stop_verification_active_.store(false);
        } else {
          this->pending_stop_transition_ = true;
        }
      }
    }

    // Endpoint arrival: position dead-reckoned to 0.0 or 1.0 (clamped).
    // The blind's own end-stop handles the physical stop -- no STOP command
    // needed.  Just transition to IDLE so we stop publishing OPENING/CLOSING
    // at the endpoint every second.  The post_movement_poll_at_ timer
    // (already scheduled by start_movement) confirms final state via RF.
    if ((this->current_operation == COVER_OPERATION_CLOSING && this->position <= 0.0f) ||
        (this->current_operation == COVER_OPERATION_OPENING && this->position >= 1.0f)) {
      this->current_operation = COVER_OPERATION_IDLE;
      this->publish_state(false);
      this->last_publish_ = now;
    }

    // Publish position every second
    if(cover_logic::should_publish_position(now, this->last_publish_, 1000)) {
      this->publish_state(false);
      this->last_publish_ = now;
    }
  }
}

bool EleroCover::is_at_target() {
  int operation = 0;
  if (this->current_operation == COVER_OPERATION_OPENING)
    operation = 1;
  else if (this->current_operation == COVER_OPERATION_CLOSING)
    operation = -1;

  return cover_logic::is_at_target(this->position, this->target_position_, operation,
                                   this->open_duration_, this->close_duration_,
                                   this->parent_->get_tx_queue_depth(),
                                   ELERO_TX_LATENCY_COMPENSATION_MS);
}

void EleroCover::handle_delivery_outcome_(const DeliveryOutcome &outcome) {
  const bool packet_accepted = delivery_packet_was_accepted(outcome.event);
  if (should_start_timed_action(this->pending_movement_start_, this->pending_movement_kind_, outcome)) {
    const auto operation = outcome.intent.kind == CommandIntentKind::OPEN
                               ? COVER_OPERATION_OPENING
                               : COVER_OPERATION_CLOSING;
    this->pending_movement_start_ = false;
    this->begin_movement_tracking_(operation, millis());
  }

  const bool first_stop_accepted = this->pending_stop_transition_ &&
      outcome.intent.kind == CommandIntentKind::STOP && packet_accepted;
  if (first_stop_accepted) {
    this->pending_stop_transition_ = false;
    if (!this->stop_urgent_active_) {
      this->parent_->increment_stop_urgent();
      this->stop_urgent_active_ = true;
    }
    this->current_operation = COVER_OPERATION_IDLE;
    this->stop_verify_at_ = millis() + ELERO_STOP_VERIFY_DELAY_MS;
    this->stop_verify_retries_ = 0;
    this->publish_state(false);
    this->last_publish_ = millis();
  }

  if (outcome.event == DeliveryEvent::DROPPED) {
    ESP_LOGE(TAG, "Delivery retries exhausted for blind 0x%06x", this->command_.blind_addr);
    this->parent_->increment_tx_drop_count();
    if (outcome.intent.kind == CommandIntentKind::STOP && this->pending_stop_transition_) {
      this->pending_stop_transition_ = false;
      this->stop_trigger_ms_ = 0;
      this->finish_stop_verification_();
    }
    if (this->pending_movement_start_ && outcome.intent.kind == this->pending_movement_kind_) {
      this->pending_movement_start_ = false;
      this->current_operation = COVER_OPERATION_IDLE;
      this->publish_state(false);
    }
  } else if (outcome.event == DeliveryEvent::STALE_CLEARED) {
    ESP_LOGW(TAG, "Stale Command queue cleared for blind 0x%06x", this->command_.blind_addr);
    this->parent_->increment_tx_drop_count();
    if (outcome.intent.kind == CommandIntentKind::STOP && this->pending_stop_transition_) {
      this->pending_stop_transition_ = false;
      this->stop_trigger_ms_ = 0;
      this->finish_stop_verification_();
    }
    if (this->pending_movement_start_) {
      this->pending_movement_start_ = false;
      this->current_operation = COVER_OPERATION_IDLE;
      this->publish_state(false);
    }
  }
#ifdef USE_TEXT_SENSOR
  if (this->queue_full_published_ && outcome.queue_size == 0)
    this->queue_full_published_ = false;
#endif
}

CommandDeliveryConfig EleroCover::get_command_delivery_config() const {
  CommandDeliveryConfig config{};
  config.profile.blind_address = this->command_.blind_addr;
  config.profile.remote_address = this->command_.remote_addr;
  config.profile.channel = this->command_.channel;
  config.profile.pck_inf[0] = this->command_.pck_inf[0];
  config.profile.pck_inf[1] = this->command_.pck_inf[1];
  config.profile.hop = this->command_.hop;
  config.profile.payload_1 = this->command_.payload[0];
  config.profile.payload_2 = this->command_.payload[1];
  config.mapping.open = this->command_up_;
  config.mapping.close = this->command_down_;
  config.mapping.stop = this->command_stop_;
  config.mapping.check = this->command_check_;
  config.mapping.tilt = this->command_tilt_;
  return config;
}

IntentSubmitResult EleroCover::submit_intent(const CommandIntent &intent) {
  const bool deferred = this->should_defer_intent(intent);
  auto result = this->delivery_.submit(intent, millis(), deferred);
  if (result == IntentSubmitResult::REJECTED) {
    ESP_LOGW(TAG, "Command queue full for blind 0x%06x", this->command_.blind_addr);
#ifdef USE_TEXT_SENSOR
    if (!this->queue_full_published_) {
      this->parent_->publish_text_sensor_state(this->command_.blind_addr, "queue_full");
      this->queue_full_published_ = true;
    }
#endif
  }
  return result;
}

float EleroCover::get_setup_priority() const { return setup_priority::DATA; }

cover::CoverTraits EleroCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_stop(true);
  if((this->open_duration_ > 0) && (this->close_duration_ > 0))
    traits.set_supports_position(true);
  else
    traits.set_supports_position(false);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(this->assumed_state_);
  traits.set_supports_tilt(this->supports_tilt_);
  return traits;
}

void EleroCover::set_rx_state(uint8_t state) {
  this->last_state_raw_ = state;
  ESP_LOGV(TAG, "Got state: 0x%02x (%s) for blind 0x%06x", state, elero_state_to_string(state), this->command_.blind_addr);
  float pos = this->position;
  float current_tilt = this->tilt;
  CoverOperation op = this->current_operation;

  switch(state) {
  case ELERO_STATE_TOP:
    pos = COVER_OPEN;
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
    break;
  case ELERO_STATE_BOTTOM:
    pos = COVER_CLOSED;
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
    break;
  case ELERO_STATE_INTERMEDIATE:
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
    // Keep current position estimate
    break;
  case ELERO_STATE_START_MOVING_UP:
  case ELERO_STATE_MOVING_UP:
    op = COVER_OPERATION_OPENING;
    current_tilt = 0.0;
    break;
  case ELERO_STATE_START_MOVING_DOWN:
  case ELERO_STATE_MOVING_DOWN:
    op = COVER_OPERATION_CLOSING;
    current_tilt = 0.0;
    break;
  case ELERO_STATE_TILT:
    op = COVER_OPERATION_IDLE;
    current_tilt = 1.0;
    break;
  case ELERO_STATE_TOP_TILT:
    pos = COVER_OPEN;
    op = COVER_OPERATION_IDLE;
    current_tilt = 1.0;
    break;
  case ELERO_STATE_BOTTOM_TILT: // also ELERO_STATE_OFF (0x0f)
    pos = COVER_CLOSED;
    op = COVER_OPERATION_IDLE;
    current_tilt = 1.0;
    break;
  case ELERO_STATE_STOPPED:
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
    break;
  case ELERO_STATE_BLOCKING:
    ESP_LOGW(TAG, "Blind 0x%06x reports BLOCKING", this->command_.blind_addr);
    op = COVER_OPERATION_IDLE;
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "blocking");
#endif
    break;
  case ELERO_STATE_OVERHEATED:
    ESP_LOGW(TAG, "Blind 0x%06x reports OVERHEATED", this->command_.blind_addr);
    op = COVER_OPERATION_IDLE;
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "overheated");
#endif
    break;
  case ELERO_STATE_TIMEOUT:
    ESP_LOGW(TAG, "Blind 0x%06x reports TIMEOUT", this->command_.blind_addr);
    op = COVER_OPERATION_IDLE;
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "timeout");
#endif
    break;
  default:
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
  }

  // Stop verification: if we sent a stop and are waiting for confirmation,
  // check whether the motor actually stopped or is still moving.
  // Guard on stop_verify_at_ (not retries) so a "stopped" response always
  // cancels verification — even after retries have been exhausted.
  if (this->stop_verify_at_ > 0) {
    if ((state == ELERO_STATE_MOVING_UP || state == ELERO_STATE_MOVING_DOWN ||
         state == ELERO_STATE_START_MOVING_UP || state == ELERO_STATE_START_MOVING_DOWN) &&
        this->stop_verify_retries_ < ELERO_STOP_VERIFY_MAX_RETRIES) {
      // Motor is still moving and retries remain — re-send stop via priority queue
      this->stop_verify_retries_++;
      ESP_LOGW(TAG, "Blind 0x%06x still moving after stop, retry #%d",
               this->command_.blind_addr, this->stop_verify_retries_);
      this->submit_intent({CommandIntentKind::STOP, 0});
      this->stop_verify_at_ = millis() + ELERO_STOP_VERIFY_DELAY_MS;
      op = COVER_OPERATION_IDLE;  // keep our side idle while retrying
    } else if (state != ELERO_STATE_MOVING_UP && state != ELERO_STATE_MOVING_DOWN &&
               state != ELERO_STATE_START_MOVING_UP && state != ELERO_STATE_START_MOVING_DOWN) {
      // Motor confirmed stopped — correct position for actual stop delay
      if (this->stop_trigger_ms_ > 0 && this->open_duration_ > 0 && this->close_duration_ > 0) {
        uint32_t actual_delay = millis() - this->stop_trigger_ms_;
        float correction_dur = (this->last_operation_ == COVER_OPERATION_OPENING)
                                ? static_cast<float>(this->open_duration_)
                                : static_cast<float>(this->close_duration_);
        float overshoot = static_cast<float>(actual_delay) / correction_dur;
        if (this->last_operation_ == COVER_OPERATION_OPENING)
          pos = clamp(this->stop_trigger_position_ + overshoot, 0.0f, 1.0f);
        else
          pos = clamp(this->stop_trigger_position_ - overshoot, 0.0f, 1.0f);
        ESP_LOGD(TAG, "Blind 0x%06x stop verified: corrected pos %.2f -> %.2f (delay %ums)",
                 this->command_.blind_addr, this->stop_trigger_position_, pos, actual_delay);
      }
      this->stop_trigger_ms_ = 0;
      this->stop_verify_retries_ = ELERO_STOP_VERIFY_MAX_RETRIES;
      this->stop_verify_at_ = 0;
      // Decrement stop_urgent so other covers can resume once all stops confirmed.
      this->finish_stop_verification_();
    }
    // else: still moving but retries exhausted — let the timer in loop() handle exhaustion
  }

  if((pos != this->position) || (op != this->current_operation) || (current_tilt != this->tilt)) {
    this->position = pos;
    this->tilt = current_tilt;
    this->current_operation = op;
    this->publish_state();
  }
}

void EleroCover::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->start_movement(COVER_OPERATION_IDLE);
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    this->target_position_ = pos;
    // Recover from NAN position: treat as 0.5 (mid-point) so direction
    // decisions work.  NAN breaks all comparisons (IEEE 754).
    float cur = this->position;
    if (std::isnan(cur)) {
      cur = 0.5f;
      this->position = cur;
      ESP_LOGW(TAG, "Blind 0x%06x position was NAN, reset to 0.5",
               this->command_.blind_addr);
    }
    // Short-circuit: already at fully open/closed — skip movement entirely.
    // Without this, commanding 100% when already at 100% sends a redundant
    // RF command and enters OPENING state with no auto-stop (is_at_target
    // returns false for endpoint targets), spamming logs every second until
    // the blind's RF response arrives (which may never come).
    if (pos == COVER_OPEN && cur >= (1.0f - 0.01f) &&
        this->current_operation == COVER_OPERATION_IDLE &&
        (this->open_duration_ > 0) && (this->close_duration_ > 0)) {
      // Already at open — no movement needed
    } else if (pos == COVER_CLOSED && cur <= (0.0f + 0.01f) &&
               this->current_operation == COVER_OPERATION_IDLE &&
               (this->open_duration_ > 0) && (this->close_duration_ > 0)) {
      // Already at closed — no movement needed
    } else if (pos != COVER_OPEN && pos != COVER_CLOSED &&
               (this->open_duration_ > 0) && (this->close_duration_ > 0) &&
               std::abs(pos - cur) < 0.01f) {
      // Already at intermediate target — no movement needed
    } else if((pos > cur) || (pos == COVER_OPEN)) {
      this->start_movement(COVER_OPERATION_OPENING);
    } else {
      this->start_movement(COVER_OPERATION_CLOSING);
    }
  }
  if (call.get_tilt().has_value()) {
    auto tilt = *call.get_tilt();
    if(tilt > 0) {
      if (intent_was_accepted(this->submit_intent({CommandIntentKind::TILT, 0})))
        this->tilt = 1.0;
    } else {
      this->tilt = 0.0;
    }
  }
  if (call.get_toggle().has_value()) {
    if(this->current_operation != COVER_OPERATION_IDLE) {
      this->start_movement(COVER_OPERATION_IDLE);
    } else {
      if(this->position == COVER_CLOSED || this->last_operation_ == COVER_OPERATION_CLOSING) {
        this->target_position_ = COVER_OPEN;
        this->start_movement(COVER_OPERATION_OPENING);
      } else {
        this->target_position_ = COVER_CLOSED;
        this->start_movement(COVER_OPERATION_CLOSING);
      }
    }
  }
}

void EleroCover::start_movement(CoverOperation dir) {
  switch(dir) {
    case COVER_OPERATION_OPENING:
      ESP_LOGV(TAG, "Sending OPEN command");
      if (!intent_was_accepted(this->submit_intent({CommandIntentKind::OPEN, 0})))
        return;
      // Reset tilt state on movement
      this->tilt = 0.0;
      this->last_operation_ = COVER_OPERATION_OPENING;
      this->pending_movement_kind_ = CommandIntentKind::OPEN;
    break;
    case COVER_OPERATION_CLOSING:
      ESP_LOGV(TAG, "Sending CLOSE command");
      if (!intent_was_accepted(this->submit_intent({CommandIntentKind::CLOSE, 0})))
        return;
      // Reset tilt state on movement
      this->tilt = 0.0;
      this->last_operation_ = COVER_OPERATION_CLOSING;
      this->pending_movement_kind_ = CommandIntentKind::CLOSE;
    break;
    case COVER_OPERATION_IDLE:
      ESP_LOGI(TAG, "Blind 0x%06x manual stop at position %.2f",
               this->command_.blind_addr, this->position);
      this->stop_trigger_position_ = this->position;
      this->stop_trigger_ms_ = millis();
      this->stop_verification_active_.store(true);
      if (!intent_was_accepted(this->submit_intent({CommandIntentKind::STOP, 0}))) {
        this->stop_trigger_ms_ = 0;
        this->stop_verification_active_.store(false);
        return;
      }
      this->pending_movement_start_ = false;
      this->pending_stop_transition_ = true;
      // The operation and verification state are committed by
      // handle_delivery_outcome_ after the first priority packet is accepted.
      return;
  }

  if(dir == this->current_operation)
    return;

  this->current_operation = dir;
  this->pending_movement_start_ = true;
  this->movement_start_ = 0;
  this->last_recompute_time_ = 0;
  this->post_movement_poll_at_ = 0;
  this->publish_state();
}

void EleroCover::begin_movement_tracking_(CoverOperation operation, uint32_t now) {
  this->current_operation = operation;
  this->movement_start_ = now;
  this->last_recompute_time_ = now;
  if (operation == COVER_OPERATION_OPENING && this->open_duration_ > 0) {
    this->post_movement_poll_at_ = now + this->open_duration_ + ELERO_POST_MOVEMENT_POLL_DELAY;
  } else if (operation == COVER_OPERATION_CLOSING && this->close_duration_ > 0) {
    this->post_movement_poll_at_ = now + this->close_duration_ + ELERO_POST_MOVEMENT_POLL_DELAY;
  } else {
    this->post_movement_poll_at_ = 0;
  }
  this->publish_state();
}

void EleroCover::finish_stop_verification_() {
  if (this->stop_urgent_active_) {
    this->parent_->decrement_stop_urgent();
    this->stop_urgent_active_ = false;
  }
  this->stop_verification_active_.store(false);
}

void EleroCover::schedule_immediate_poll() {
  uint32_t now = millis();
  if ((now - this->last_immediate_poll_ms_) >= ELERO_IMMEDIATE_POLL_MIN_INTERVAL_MS &&
      intent_was_accepted(this->submit_intent({CommandIntentKind::CHECK, 0})))
    this->last_immediate_poll_ms_ = now;
}

void EleroCover::recompute_position() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  int dir = (this->current_operation == COVER_OPERATION_OPENING) ? 1 : -1;
  float action_dur = (dir == 1) ? static_cast<float>(this->open_duration_)
                                : static_cast<float>(this->close_duration_);
  if (action_dur == 0.0f)
    return;

  const uint32_t now = millis();
  const uint32_t elapsed = (uint32_t)(now - this->last_recompute_time_);

  // Sanity check: skip recompute if elapsed time is implausibly large
  // (e.g., millis() wraparound glitch or stale last_recompute_time_)
  if (elapsed > ELERO_TIMEOUT_MOVEMENT) {
    ESP_LOGW(TAG, "Position recompute skipped for blind 0x%06x: elapsed %u ms exceeds timeout",
             this->command_.blind_addr, elapsed);
    this->last_recompute_time_ = now;
    return;
  }

  this->position = cover_logic::recompute_position(this->position, dir, action_dur, elapsed,
                                                   ELERO_TIMEOUT_MOVEMENT);

  this->last_recompute_time_ = now;
}

} // namespace elero
} // namespace esphome
