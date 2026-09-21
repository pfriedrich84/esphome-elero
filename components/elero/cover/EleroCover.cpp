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
  ESP_LOGCONFIG(TAG, "  Blind Address: 0x%06lx", static_cast<unsigned long>(this->command_.blind_addr));
  ESP_LOGCONFIG(TAG, "  Remote Address: 0x%06lx", static_cast<unsigned long>(this->command_.remote_addr));
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->command_.channel);
  ESP_LOGCONFIG(TAG, "  Hop: 0x%02x", this->command_.hop);
  ESP_LOGCONFIG(TAG, "  pck_inf1: 0x%02x, pck_inf2: 0x%02x", this->command_.pck_inf[0], this->command_.pck_inf[1]);
  if (this->open_duration_ > 0)
    ESP_LOGCONFIG(TAG, "  Open Duration: %lums", static_cast<unsigned long>(this->open_duration_));
  if (this->close_duration_ > 0)
    ESP_LOGCONFIG(TAG, "  Close Duration: %lums", static_cast<unsigned long>(this->close_duration_));
  ESP_LOGCONFIG(TAG, "  Poll Interval: %lums", static_cast<unsigned long>(this->poll_intvl_));
  ESP_LOGCONFIG(TAG, "  Supports Tilt: %s", YESNO(this->supports_tilt_));
  if (this->tilt_close_pulse_duration_ > 0) {
    ESP_LOGCONFIG(TAG, "  Tilt Close: CLOSE pulse, %lums auto-stop",
                  static_cast<unsigned long>(this->tilt_close_pulse_duration_));
  }
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
  std::lock_guard<std::recursive_mutex> lock(this->cover_mutex_);
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

  if (!this->stop_verification_active_.load() &&
      cover_logic::should_poll(now, this->last_poll_, intvl) &&
      intent_was_accepted(this->submit_intent({CommandIntentKind::CHECK, 0}))) {
    this->last_poll_ = now;
  }

  if(this->post_movement_poll_at_ > 0 && now >= this->post_movement_poll_at_) {
    this->post_movement_poll_at_ = 0;
    if (intent_was_accepted(this->submit_intent({CommandIntentKind::CHECK, 0}))) {
      ESP_LOGD(TAG, "Post-movement status poll for blind 0x%06lx",
               static_cast<unsigned long>(this->command_.blind_addr));
      this->last_poll_ = now;
    }
  }

  // Auto-stop a tilt_close_pulse_duration_ CLOSE pulse (see control()). This
  // never engages start_movement()/position tracking — it is a short,
  // untracked side pulse, always followed by an explicit STOP.
  if (this->tilt_close_pulse_at_ != 0 && static_cast<int32_t>(now - this->tilt_close_pulse_at_) >= 0) {
    this->tilt_close_pulse_at_ = 0;
    this->submit_intent({CommandIntentKind::STOP, 0});
  }

  // Stop verification: poll motor to confirm it actually stopped. If no
  // status arrives, retry one bounded STOP burst before failing open.
  if (this->stop_verification_active_.load() &&
      static_cast<int32_t>(now - this->stop_verify_at_) >= 0 &&
      !this->pending_stop_transition_ && !this->stop_burst_pending_) {
    if (this->stop_verify_retries_ < ELERO_STOP_VERIFY_MAX_RETRIES) {
      this->stop_verify_retries_++;
      ESP_LOGD(TAG, "Stop verify poll #%d for blind 0x%06lx",
               this->stop_verify_retries_, static_cast<unsigned long>(this->command_.blind_addr));
      this->submit_intent({CommandIntentKind::CHECK, 0});
      // Reschedule in case no RF response arrives.
      this->stop_verify_at_ = now + ELERO_STOP_VERIFY_DELAY_MS;
    } else if (!this->retry_stop_verification_(now, "missing status feedback")) {
      this->fail_stop_verification_(now);
    }
  }

  if (!this->stop_verification_active_.load())
    this->delivery_.release_deferred();

  if (!this->pending_movement_start_ &&
      (this->current_operation != COVER_OPERATION_IDLE) &&
      (this->open_duration_ > 0) && (this->close_duration_ > 0)) {
    this->recompute_position();
    if(this->is_at_target()) {
      ESP_LOGI(TAG, "Blind 0x%06lx reached target (pos=%.2f, target=%.2f), sending stop",
               static_cast<unsigned long>(this->command_.blind_addr), this->position, this->target_position_);
      if (!this->pending_stop_transition_) {
        // Queueing locally is not proof that the hub accepted the priority
        // packet. Keep tracking movement until advance() reports first RF queue
        // acceptance; only then transition to idle and start verification.
        this->request_stop();
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
  std::lock_guard<std::recursive_mutex> lock(this->cover_mutex_);
  if (outcome.intent.kind == CommandIntentKind::STOP &&
      (outcome.event == DeliveryEvent::COMPLETED || outcome.event == DeliveryEvent::DROPPED ||
       outcome.event == DeliveryEvent::STALE_CLEARED))
    this->stop_burst_pending_ = false;
  if (outcome.intent.kind == CommandIntentKind::STOP &&
      (outcome.event == DeliveryEvent::DROPPED || outcome.event == DeliveryEvent::STALE_CLEARED)) {
    this->parent_->increment_tx_drop_count();
    if (this->stop_verification_active_.load())
      this->fail_stop_verification_(millis());
    return;
  }
  if (const char *result = ordinary_delivery_result(outcome)) {
    ESP_LOGI(TAG, "Blind 0x%06lx: %s (no protocol ACK)",
             static_cast<unsigned long>(this->command_.blind_addr), result);
#ifdef USE_TEXT_SENSOR
    if (!this->stop_verification_active_.load())
      this->parent_->publish_text_sensor_state(this->command_.blind_addr, result);
#endif
  }
  const bool packet_accepted = delivery_packet_was_accepted(outcome.event);
  const bool is_movement = outcome.intent.kind == CommandIntentKind::OPEN ||
                           outcome.intent.kind == CommandIntentKind::CLOSE;
  const auto operation = outcome.intent.kind == CommandIntentKind::OPEN
                             ? COVER_OPERATION_OPENING
                             : COVER_OPERATION_CLOSING;
  // A first transmission without a matching pending flag represents an older
  // command overtaken optimistically by a queued direction change. Re-anchor
  // only for that real direction transition; repeated same-direction commands
  // must not discard already elapsed travel time.
  const CommandIntentKind current_movement =
      this->current_operation == COVER_OPERATION_OPENING ? CommandIntentKind::OPEN
                                                         : CommandIntentKind::CLOSE;
  const bool transmitted_direction_change = is_movement &&
      this->current_operation != COVER_OPERATION_IDLE &&
      should_reanchor_transmitted_action(current_movement, outcome);
  if (transmitted_direction_change ||
      should_start_timed_action(this->pending_movement_start_, this->pending_movement_kind_, outcome)) {
    if (this->pending_movement_kind_ == outcome.intent.kind)
      this->pending_movement_start_ = false;
    this->begin_movement_tracking_(operation, outcome.transmitted_at_ms != 0
                                                 ? outcome.transmitted_at_ms
                                                 : millis());
  }

  const bool first_stop_accepted = this->pending_stop_transition_ &&
      outcome.intent.kind == CommandIntentKind::STOP && packet_accepted;
  if (first_stop_accepted) {
    this->pending_stop_transition_ = false;
    this->current_operation = COVER_OPERATION_IDLE;  // UI estimate, not physical confirmation
    this->stop_rx_cutoff_ = outcome.rx_cutoff;
    this->schedule_stop_verification_(outcome.transmitted_at_ms != 0 ? outcome.transmitted_at_ms : millis());
    this->publish_stop_result_("stop_verifying");
    ESP_LOGD(TAG, "STOP verification begins: cutoff seq=%llu epoch=%llu tx=%lu",
             static_cast<unsigned long long>(this->stop_rx_cutoff_.sequence),
             static_cast<unsigned long long>(this->stop_rx_cutoff_.epoch),
             static_cast<unsigned long>(this->stop_rx_cutoff_.completed_at_ms));
    this->publish_state(false);
    this->last_publish_ = millis();
  }

  if (outcome.event == DeliveryEvent::DROPPED) {
    ESP_LOGE(TAG, "Delivery retries exhausted for blind 0x%06lx",
             static_cast<unsigned long>(this->command_.blind_addr));
    this->parent_->increment_tx_drop_count();
    if (this->pending_movement_start_ && outcome.intent.kind == this->pending_movement_kind_) {
      this->pending_movement_start_ = false;
      this->current_operation = COVER_OPERATION_IDLE;
      this->publish_state(false);
    }
  } else if (outcome.event == DeliveryEvent::STALE_CLEARED) {
    ESP_LOGW(TAG, "Stale Command queue cleared for blind 0x%06lx",
             static_cast<unsigned long>(this->command_.blind_addr));
    this->parent_->increment_tx_drop_count();
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
  std::lock_guard<std::recursive_mutex> lock(this->cover_mutex_);
  if (intent.kind == CommandIntentKind::STOP)
    return this->request_stop();
  const bool deferred = this->should_defer_intent(intent);
  auto result = this->delivery_.submit(intent, millis(), deferred);
  if (result == IntentSubmitResult::REJECTED) {
    ESP_LOGW(TAG, "Command queue full for blind 0x%06lx",
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

IntentSubmitResult EleroCover::submit_control_intent(const CommandIntent &intent) {
  std::lock_guard<std::recursive_mutex> lock(this->cover_mutex_);
  switch (intent.kind) {
    case CommandIntentKind::OPEN:
      this->target_position_ = COVER_OPEN;
      return this->start_movement(COVER_OPERATION_OPENING);
    case CommandIntentKind::CLOSE:
      this->target_position_ = COVER_CLOSED;
      return this->start_movement(COVER_OPERATION_CLOSING);
    case CommandIntentKind::STOP:
      return this->start_movement(COVER_OPERATION_IDLE);
    case CommandIntentKind::TILT: {
      const auto result = this->submit_intent(intent);
      if (intent_was_accepted(result))
        this->tilt = 1.0f;
      return result;
    }
    default:
      return this->submit_intent(intent);
  }
}

IntentSubmitResult EleroCover::request_stop(bool already_admitted) {
  std::lock_guard<std::recursive_mutex> lock(this->cover_mutex_);
  const uint32_t now = millis();
  const auto result = already_admitted ? IntentSubmitResult::ACCEPTED
      : this->delivery_.submit({CommandIntentKind::STOP, 0}, now);
  if (!intent_was_accepted(result)) {
    this->fail_stop_verification_(now);
    return result;
  }
  if (this->stop_verification_active_.load())
    return result;  // repeated user requests do not reset the bounded budget
  if (this->current_operation != COVER_OPERATION_IDLE && !this->pending_movement_start_ &&
      this->open_duration_ > 0 && this->close_duration_ > 0)
    this->recompute_position();
  this->stop_trigger_position_ = this->position;
  this->stop_trigger_ms_ = now;
  this->stop_verify_stop_retries_ = 0;
  this->stop_verify_at_ = 0;
  this->post_movement_poll_at_ = 0;  // STOP owns its bounded CHECK schedule
  this->stop_rx_cutoff_ = {};
  this->stop_verification_active_.store(true);
  this->delivery_.set_stop_verifying(true);
  this->pending_movement_start_ = false;
  this->pending_stop_transition_ = true;
  this->stop_burst_pending_ = true;
  this->publish_stop_result_("stop_queued");
  return result;
}

void EleroCover::publish_stop_result_(const char *result) {
  if (this->stop_result_ != result) {
    ESP_LOGI(TAG, "Blind 0x%06lx delivery=%s", static_cast<unsigned long>(this->command_.blind_addr), result);
    this->stop_result_ = result;
  }
#ifdef USE_TEXT_SENSOR
  this->parent_->publish_text_sensor_state(this->command_.blind_addr, result);
#endif
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
  this->set_rx_status(state, {});  // metadata-free callers cannot confirm a command
}

void EleroCover::set_rx_status(uint8_t state, const RxMetadata &meta) {
  std::lock_guard<std::recursive_mutex> lock(this->cover_mutex_);
  // An old buffered status must not change movement or position while stopping.
  if (this->stop_verification_active_.load() &&
      !meta.after(this->stop_rx_cutoff_, this->command_.blind_addr))
    return;
  this->apply_rx_state_(state, meta);
}

void EleroCover::apply_rx_state_(uint8_t state, const RxMetadata &meta) {
  this->last_state_raw_ = state;
  ESP_LOGV(TAG, "Got state: 0x%02x (%s) for blind 0x%06lx", state, elero_state_to_string(state),
           static_cast<unsigned long>(this->command_.blind_addr));
  float pos = this->position;
  float current_tilt = this->tilt;
  CoverOperation op = this->current_operation;

  switch(state) {
  case ELERO_STATE_TOP:
    pos = COVER_OPEN;
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
    this->position_trusted_for_redundancy_ = true;
    break;
  case ELERO_STATE_BOTTOM:
    pos = COVER_CLOSED;
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
    this->position_trusted_for_redundancy_ = true;
    break;
  case ELERO_STATE_INTERMEDIATE:
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
    // Keep current position estimate, but the receiver did not report an
    // exact percentage. Do not suppress future set-position commands.
    this->position_trusted_for_redundancy_ = false;
    break;
  case ELERO_STATE_START_MOVING_UP:
  case ELERO_STATE_MOVING_UP:
    op = COVER_OPERATION_OPENING;
    current_tilt = 0.0;
    this->position_trusted_for_redundancy_ = false;
    break;
  case ELERO_STATE_START_MOVING_DOWN:
  case ELERO_STATE_MOVING_DOWN:
    op = COVER_OPERATION_CLOSING;
    current_tilt = 0.0;
    this->position_trusted_for_redundancy_ = false;
    break;
  case ELERO_STATE_TILT:
    op = COVER_OPERATION_IDLE;
    current_tilt = 1.0;
    this->position_trusted_for_redundancy_ = false;
    break;
  case ELERO_STATE_TOP_TILT:
    pos = COVER_OPEN;
    op = COVER_OPERATION_IDLE;
    current_tilt = 1.0;
    this->position_trusted_for_redundancy_ = true;
    break;
  case ELERO_STATE_BOTTOM_TILT: // also ELERO_STATE_OFF (0x0f)
    pos = COVER_CLOSED;
    op = COVER_OPERATION_IDLE;
    current_tilt = 1.0;
    this->position_trusted_for_redundancy_ = true;
    break;
  case ELERO_STATE_STOPPED:
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
    this->position_trusted_for_redundancy_ = false;
    break;
  case ELERO_STATE_BLOCKING:
    ESP_LOGW(TAG, "Blind 0x%06lx reports BLOCKING", static_cast<unsigned long>(this->command_.blind_addr));
    op = COVER_OPERATION_IDLE;
    this->position_trusted_for_redundancy_ = false;
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "blocking");
#endif
    break;
  case ELERO_STATE_OVERHEATED:
    ESP_LOGW(TAG, "Blind 0x%06lx reports OVERHEATED", static_cast<unsigned long>(this->command_.blind_addr));
    op = COVER_OPERATION_IDLE;
    this->position_trusted_for_redundancy_ = false;
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "overheated");
#endif
    break;
  case ELERO_STATE_TIMEOUT:
    ESP_LOGW(TAG, "Blind 0x%06lx reports TIMEOUT", static_cast<unsigned long>(this->command_.blind_addr));
    op = COVER_OPERATION_IDLE;
    this->position_trusted_for_redundancy_ = false;
#ifdef USE_TEXT_SENSOR
    this->parent_->publish_text_sensor_state(this->command_.blind_addr, "timeout");
#endif
    break;
  default:
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
    this->position_trusted_for_redundancy_ = false;
  }

  if (this->stop_verification_active_.load()) {
    const auto feedback = fresh_stop_feedback(state, meta, this->stop_rx_cutoff_, this->command_.blind_addr);
    switch (feedback) {
      case StopFeedback::MOVING:
        // Finish the guaranteed initial burst before spending the one extra
        // burst. A fast response must not coalesce that retry into packet two.
        if (!this->stop_burst_pending_ && !this->retry_stop_verification_(millis(), "fresh moving status"))
          this->fail_stop_verification_(millis());
        op = COVER_OPERATION_IDLE;
        break;
      case StopFeedback::STOPPED:
        // RF/dispatch latency is not measured mechanical overshoot. Preserve
        // explicit endpoints and the last estimate instead of inventing travel.
        this->stop_trigger_ms_ = 0;
        this->finish_stop_verification_();
        this->publish_stop_result_("stop_confirmed");
        break;
      case StopFeedback::BLOCKING:
      case StopFeedback::OVERHEATED:
        this->finish_stop_verification_();
        this->publish_stop_result_(feedback == StopFeedback::BLOCKING ? "stop_motor_blocking" : "stop_motor_overheated");
        break;
      case StopFeedback::INAPPLICABLE:
        // UNKNOWN/TIMEOUT never turn a locally transmitted STOP into success.
        this->publish_stop_result_("stop_verifying");
        break;
    }
  }

  if((pos != this->position) || (op != this->current_operation) || (current_tilt != this->tilt)) {
    this->position = pos;
    this->tilt = current_tilt;
    this->current_operation = op;
    this->publish_state();
  }
}

void EleroCover::control(const cover::CoverCall &call) {
  std::lock_guard<std::recursive_mutex> lock(this->cover_mutex_);
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
      ESP_LOGW(TAG, "Blind 0x%06lx position was NAN, reset to 0.5",
               static_cast<unsigned long>(this->command_.blind_addr));
    }
    // ESPHome represents OPEN/CLOSE as endpoint positions in CoverCall, so an
    // old-position == new-position check must not suppress those commands.
    // Only an already-reached intermediate set-position target is redundant.
    if ((this->open_duration_ > 0) && (this->close_duration_ > 0) &&
        cover_logic::is_redundant_intermediate_target(cur, pos, this->position_trusted_for_redundancy_)) {
      // Already at the requested intermediate position — no movement needed.
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
    } else if (this->tilt_close_pulse_duration_ > 0) {
      // No distinct RF byte for the close direction on this hardware: send the
      // normal CLOSE command and auto-stop it after tilt_close_pulse_duration_
      // (see loop()), instead of a real full-travel close.
      if (intent_was_accepted(this->submit_intent({CommandIntentKind::CLOSE, 0}))) {
        this->tilt_close_pulse_at_ = millis() + this->tilt_close_pulse_duration_;
        this->tilt = 0.0;
      }
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

IntentSubmitResult EleroCover::start_movement(CoverOperation dir) {
  std::lock_guard<std::recursive_mutex> lock(this->cover_mutex_);
  // A real tracked movement (including an explicit STOP) supersedes any
  // pending tilt_close_pulse_duration_ auto-stop — that timer must never
  // fire mid-way through a real, separately-started movement.
  this->tilt_close_pulse_at_ = 0;
  IntentSubmitResult result = IntentSubmitResult::REJECTED;
  switch(dir) {
    case COVER_OPERATION_OPENING:
      ESP_LOGV(TAG, "Sending OPEN command");
      result = this->submit_intent({CommandIntentKind::OPEN, 0});
      if (!intent_was_accepted(result))
        return result;
      // Reset tilt state on movement
      this->tilt = 0.0;
      this->last_operation_ = COVER_OPERATION_OPENING;
      this->pending_movement_kind_ = CommandIntentKind::OPEN;
    break;
    case COVER_OPERATION_CLOSING:
      ESP_LOGV(TAG, "Sending CLOSE command");
      result = this->submit_intent({CommandIntentKind::CLOSE, 0});
      if (!intent_was_accepted(result))
        return result;
      // Reset tilt state on movement
      this->tilt = 0.0;
      this->last_operation_ = COVER_OPERATION_CLOSING;
      this->pending_movement_kind_ = CommandIntentKind::CLOSE;
    break;
    case COVER_OPERATION_IDLE:
      ESP_LOGI(TAG, "Blind 0x%06lx manual stop at position %.2f",
               static_cast<unsigned long>(this->command_.blind_addr), this->position);
      return this->request_stop();
  }

  if (this->stop_verification_active_.load()) {
    this->pending_movement_start_ = true;
    return result;  // admitted deferred work is not movement yet
  }
  this->apply_movement_state_(dir);
  return result;
}

void EleroCover::apply_movement_state_(CoverOperation dir) {
  if (dir == this->current_operation)
    return;

  this->current_operation = dir;
  this->position_trusted_for_redundancy_ = false;
  this->pending_movement_start_ = true;
  this->movement_start_ = 0;
  this->last_recompute_time_ = 0;
  this->post_movement_poll_at_ = 0;
  this->publish_state();
}

void EleroCover::begin_movement_tracking_(CoverOperation operation, uint32_t now) {
  this->current_operation = operation;
  this->position_trusted_for_redundancy_ = false;
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

void EleroCover::schedule_stop_verification_(uint32_t now) {
  this->stop_verify_at_ = now + ELERO_STOP_VERIFY_DELAY_MS;
  this->stop_verify_retries_ = 0;
}

bool EleroCover::retry_stop_verification_(uint32_t now, const char *reason) {
  if (this->stop_verify_stop_retries_ >= ELERO_STOP_VERIFY_MAX_STOP_RETRIES)
    return false;
  this->stop_verify_stop_retries_++;
  ESP_LOGW(TAG, "Blind 0x%06lx stop not verified (%s), retry STOP burst #%d",
           static_cast<unsigned long>(this->command_.blind_addr), reason,
           this->stop_verify_stop_retries_);
  this->stop_verify_at_ = 0;
  this->stop_verify_retries_ = 0;
  if (!intent_was_accepted(this->delivery_.submit({CommandIntentKind::STOP, 0}, now))) {
    this->stop_verify_at_ = now + ELERO_STOP_VERIFY_DELAY_MS;
    return true;
  }
  this->pending_stop_transition_ = true;
  this->stop_burst_pending_ = true;
  this->stop_rx_cutoff_ = {};
  return true;
}

void EleroCover::fail_stop_verification_(uint32_t now) {
  ESP_LOGW(TAG, "Stop verification exhausted for blind 0x%06lx after %d STOP retry burst(s)",
           static_cast<unsigned long>(this->command_.blind_addr), ELERO_STOP_VERIFY_MAX_STOP_RETRIES);
  this->stop_verify_at_ = 0;
  this->stop_trigger_ms_ = 0;
  this->finish_stop_verification_();
  this->publish_stop_result_("stop_failed");
  // Position is uncertain after failed stop verification. Keep last estimate
  // but mark it untrusted so future commands are never suppressed as redundant.
  this->position_trusted_for_redundancy_ = false;
  this->current_operation = cover::COVER_OPERATION_IDLE;
  this->delivery_.discard_checks();
  this->command_cooldown_until_ = now + 3000;
  this->delivery_.postpone_until(this->command_cooldown_until_);
  this->publish_state(false);
}

void EleroCover::finish_stop_verification_() {
  if (this->stop_urgent_active_) {
    this->parent_->decrement_stop_urgent();
    this->stop_urgent_active_ = false;
  }
  this->stop_verify_at_ = 0;
  this->stop_verify_retries_ = ELERO_STOP_VERIFY_MAX_RETRIES;
  this->stop_verify_stop_retries_ = 0;
  this->pending_stop_transition_ = false;
  this->stop_burst_pending_ = false;
  this->stop_verification_active_.store(false);
  this->delivery_.set_stop_verifying(false);
  this->stop_rx_cutoff_ = {};
}

void EleroCover::prepare_group_intent(const CommandIntent &intent, float target_position) {
  std::lock_guard<std::recursive_mutex> lock(this->cover_mutex_);
  switch (intent.kind) {
    case CommandIntentKind::OPEN:
    case CommandIntentKind::CLOSE: {
      const auto operation = intent.kind == CommandIntentKind::OPEN
                                 ? COVER_OPERATION_OPENING
                                 : COVER_OPERATION_CLOSING;
      this->target_position_ = target_position >= COVER_CLOSED && target_position <= COVER_OPEN
                                   ? target_position
                                   : (operation == COVER_OPERATION_OPENING ? COVER_OPEN : COVER_CLOSED);
      this->tilt = 0.0f;
      this->last_operation_ = operation;
      this->pending_movement_kind_ = intent.kind;
      if (this->stop_verification_active_.load())
        this->pending_movement_start_ = true;
      else
        this->apply_movement_state_(operation);
      break;
    }
    case CommandIntentKind::STOP:
      this->request_stop(true);  // group admission already succeeded atomically
      break;
    case CommandIntentKind::TILT:
      this->tilt = 1.0f;
      this->publish_state();
      break;
    default:
      break;
  }
}

void EleroCover::handle_group_delivery_outcome(const DeliveryOutcome &outcome) {
  std::lock_guard<std::recursive_mutex> lock(this->cover_mutex_);
  if (delivery_packet_was_accepted(outcome.event)) {
    this->handle_delivery_outcome_(outcome);
    return;
  }
  const bool terminal_failure = outcome.event == DeliveryEvent::DROPPED ||
      outcome.event == DeliveryEvent::STALE_CLEARED ||
      (outcome.event == DeliveryEvent::FALLBACK_MEMBER_DROPPED &&
       (outcome.fallback_member || outcome.queue_size == 0));
  if (!terminal_failure)
    return;
  if (outcome.intent.kind == CommandIntentKind::STOP && this->stop_verification_active_.load()) {
    this->pending_stop_transition_ = false;
    this->stop_trigger_ms_ = 0;
    this->stop_burst_pending_ = false;
    this->fail_stop_verification_(millis());
  }
  if (this->pending_movement_start_ && outcome.intent.kind == this->pending_movement_kind_) {
    this->pending_movement_start_ = false;
    this->current_operation = COVER_OPERATION_IDLE;
    this->publish_state(false);
  }
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
    ESP_LOGW(TAG, "Position recompute skipped for blind 0x%06lx: elapsed %lu ms exceeds timeout",
             static_cast<unsigned long>(this->command_.blind_addr), static_cast<unsigned long>(elapsed));
    this->last_recompute_time_ = now;
    return;
  }

  this->position = cover_logic::recompute_position(this->position, dir, action_dur, elapsed,
                                                   ELERO_TIMEOUT_MOVEMENT);

  this->last_recompute_time_ = now;
}

} // namespace elero
} // namespace esphome
