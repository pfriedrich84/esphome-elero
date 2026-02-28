#include "EleroCover.h"
#include "esphome/core/log.h"

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
}

void EleroCover::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Elero parent not configured for cover '%s'", this->get_name().c_str());
    this->mark_failed();
    return;
  }
  this->parent_->register_cover(this);
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    if((this->open_duration_ > 0) && (this->close_duration_ > 0))
      this->position = 0.5f;
  }
  // Publish initial state so Home Assistant has correct state on boot
  this->publish_state(false);
  ESP_LOGI(TAG, "Cover '%s' ready: blind=0x%06x, remote=0x%06x, ch=%d, poll=%dms",
           this->get_name().c_str(), this->command_.blind_addr,
           this->command_.remote_addr, this->command_.channel, this->poll_intvl_);
}

void EleroCover::loop() {
  uint32_t intvl = this->poll_intvl_;
  uint32_t now = millis();
  if(this->current_operation != COVER_OPERATION_IDLE) {
    // Compute a tighter timeout when the travel duration is known:
    // 3× the expected duration is generous enough to handle slow blinds
    // but avoids the full 120 s hard timeout for typical 30 s movements.
    uint32_t expected_dur_for_timeout = (this->current_operation == COVER_OPERATION_OPENING)
                                            ? this->open_duration_
                                            : this->close_duration_;
    uint32_t effective_timeout = ELERO_TIMEOUT_MOVEMENT;  // default 120 s
    if (expected_dur_for_timeout > 0) {
      uint32_t travel_timeout = expected_dur_for_timeout * 3;
      if (travel_timeout < effective_timeout)
        effective_timeout = travel_timeout;
    }
    if(this->movement_start_ > 0 && (now - this->movement_start_) > effective_timeout) {
      // Force idle if movement timed out with no RF feedback
      ESP_LOGW(TAG, "Blind 0x%06x: movement timed out after %us with no RF feedback, forcing idle",
               this->command_.blind_addr, effective_timeout / 1000);
      this->current_operation = COVER_OPERATION_IDLE;
      this->movement_start_ = 0;
      this->publish_state();
    } else {
      // Determine expected travel duration for the current direction
      uint32_t expected_dur = (this->current_operation == COVER_OPERATION_OPENING)
                                  ? this->open_duration_
                                  : this->close_duration_;
      if (expected_dur > 0 &&
          (now - this->movement_start_) > expected_dur + ELERO_POST_MOVEMENT_POLL_DELAY) {
        // Travel time + post-movement poll already elapsed — blind should
        // have responded by now.  Switch to a relaxed poll rate to reduce
        // RF traffic while we wait for the 120 s movement timeout.
        intvl = ELERO_POLL_INTERVAL_POST_TRAVEL;
      } else {
        intvl = ELERO_POLL_INTERVAL_MOVING;  // Poll frequently while moving
      }
    }
  }

  if((now > this->poll_offset_) && (now - this->poll_offset_ - this->last_poll_) > intvl) {
    if (this->commands_to_send_.size() < ELERO_MAX_COMMAND_QUEUE) {
      this->commands_to_send_.push(this->command_check_);
      this->last_poll_ = now - this->poll_offset_;
    }
  }

  if(this->post_movement_poll_at_ > 0 && now >= this->post_movement_poll_at_) {
    this->post_movement_poll_at_ = 0;
    if (this->commands_to_send_.size() < ELERO_MAX_COMMAND_QUEUE) {
      ESP_LOGD(TAG, "Post-movement status poll for blind 0x%06x", this->command_.blind_addr);
      this->commands_to_send_.push(this->command_check_);
      this->last_poll_ = now - this->poll_offset_;
    }
  }

  this->handle_commands(now);

  if((this->current_operation != COVER_OPERATION_IDLE) && (this->open_duration_ > 0) && (this->close_duration_ > 0)) {
    this->recompute_position();
    if(this->is_at_target()) {
      if (this->commands_to_send_.size() < ELERO_MAX_COMMAND_QUEUE) {
        this->commands_to_send_.push(this->command_stop_);
        this->current_operation = COVER_OPERATION_IDLE;
        this->target_position_ = COVER_OPEN;
      }
    } else if ((this->current_operation == COVER_OPERATION_OPENING && this->position >= COVER_OPEN) ||
               (this->current_operation == COVER_OPERATION_CLOSING && this->position <= COVER_CLOSED)) {
      // Dead-reckoned position reached the physical limit — the motor stops
      // itself at the end-stops, so transition to idle without sending STOP.
      ESP_LOGD(TAG, "Blind 0x%06x: position reached limit (%.0f%%), transitioning to idle",
               this->command_.blind_addr, this->position * 100.0f);
      this->current_operation = COVER_OPERATION_IDLE;
      this->movement_start_ = 0;
      this->target_position_ = COVER_OPEN;
      // Schedule a confirmation poll if none is already pending
      if (this->post_movement_poll_at_ == 0 || now >= this->post_movement_poll_at_) {
        this->post_movement_poll_at_ = now + ELERO_POST_MOVEMENT_POLL_DELAY;
      }
    }

    // Publish position every second
    if(now - this->last_publish_ > 1000) {
      this->publish_state(false);
      this->last_publish_ = now;
    }
  }
}

bool EleroCover::is_at_target() {
  // We return false as we don't want to send a stop command for completely open or
  // close - this is handled by the cover
  if((this->target_position_ == COVER_OPEN) || (this->target_position_ == COVER_CLOSED))
    return false;

  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      return this->position >= this->target_position_;
    case COVER_OPERATION_CLOSING:
      return this->position <= this->target_position_;
    case COVER_OPERATION_IDLE:
    default:
      return true;
  }
}

void EleroCover::handle_commands(uint32_t now) {
  if((now - this->last_command_) > ELERO_DELAY_SEND_PACKETS) {
    if(this->commands_to_send_.size() > 0) {
      this->command_.payload[4] = this->commands_to_send_.front();
      if(this->parent_->send_command(&this->command_)) {
        this->send_packets_++;
        this->send_retries_ = 0;
        if(this->send_packets_ >= ELERO_SEND_PACKETS) {
          ESP_LOGD(TAG, "Blind 0x%06x: command 0x%02x sent successfully",
                   this->command_.blind_addr, this->commands_to_send_.front());
          this->commands_to_send_.pop();
          this->send_packets_ = 0;
          this->increase_counter();
        }
      } else {
        ESP_LOGD(TAG, "TX busy, retry #%d for blind 0x%06x", this->send_retries_, this->command_.blind_addr);
        this->send_retries_++;
        if(this->send_retries_ > ELERO_SEND_RETRIES) {
          ESP_LOGE(TAG, "Blind 0x%06x: hit maximum retries, giving up", this->command_.blind_addr);
          this->send_retries_ = 0;
          this->commands_to_send_.pop();
        }
      }
      this->last_command_ = now;
    }
  }
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
  traits.set_is_assumed_state(true);
  traits.set_supports_tilt(this->supports_tilt_);
  return traits;
}

void EleroCover::set_rx_state(uint8_t state) {
  uint8_t old_state = this->last_state_raw_;  // Capture before overwrite for logging
  this->last_state_raw_ = state;
  ESP_LOGD(TAG, "Blind 0x%06x state: 0x%02x (%s)", this->command_.blind_addr, state, elero_state_to_string(state));
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
  case ELERO_STATE_OVERHEATED:
  case ELERO_STATE_TIMEOUT:
    ESP_LOGW(TAG, "Blind 0x%06x reports %s", this->command_.blind_addr,
             elero_state_to_string(state));
    op = COVER_OPERATION_IDLE;
    // Flush pending commands — motor is in error, don't send more
    while (!this->commands_to_send_.empty()) this->commands_to_send_.pop();
    this->send_packets_ = 0;
    this->send_retries_ = 0;
    // Cancel movement tracking
    this->movement_start_ = 0;
    // Schedule recovery poll (10s later) to check if blind has recovered
    this->post_movement_poll_at_ = millis() + 10000;
    break;
  default:
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
  }

  if((pos != this->position) || (op != this->current_operation) || (current_tilt != this->tilt)) {
    ESP_LOGI(TAG, "Blind 0x%06x state change: %s -> %s (pos=%.0f%%, op=%s)",
             this->command_.blind_addr,
             elero_state_to_string(old_state), elero_state_to_string(state),
             pos * 100.0f,
             op == cover::COVER_OPERATION_IDLE ? "idle" :
             op == cover::COVER_OPERATION_OPENING ? "opening" : "closing");
    // If the operation direction changed (e.g. physical remote reversed the blind),
    // restart dead-reckoning from the current position.
    if (op != COVER_OPERATION_IDLE && op != this->current_operation) {
      this->movement_start_ = millis();
      this->last_recompute_time_ = millis();
    }
    // If movement just stopped, clear the tracking timestamp.
    if (op == COVER_OPERATION_IDLE && this->current_operation != COVER_OPERATION_IDLE) {
      this->movement_start_ = 0;
    }
    this->position = pos;
    this->tilt = current_tilt;
    this->current_operation = op;
    this->publish_state();
  }
}

void EleroCover::increase_counter() {
  if(this->command_.counter == 0xff)
    this->command_.counter = 1;
  else
    this->command_.counter += 1;
}

void EleroCover::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->start_movement(COVER_OPERATION_IDLE);
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    this->target_position_ = pos;
    if((pos > this->position) || (pos == COVER_OPEN)) {
      this->start_movement(COVER_OPERATION_OPENING);
    } else {
      this->start_movement(COVER_OPERATION_CLOSING);
    }
  }
  if (call.get_tilt().has_value()) {
    auto tilt = *call.get_tilt();
    if(tilt > 0) {
      if (this->commands_to_send_.size() < ELERO_MAX_COMMAND_QUEUE) {
        this->commands_to_send_.push(this->command_tilt_);
        this->tilt = 1.0;
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

void EleroCover::start_movement(CoverOperation dir) {
  switch(dir) {
    case COVER_OPERATION_OPENING:
      ESP_LOGD(TAG, "Blind 0x%06x: sending OPEN (cmd=0x%02x)", this->command_.blind_addr, this->command_up_);
      if (this->commands_to_send_.size() < ELERO_MAX_COMMAND_QUEUE) {
        this->commands_to_send_.push(this->command_up_);
        // Reset tilt state on movement
        this->tilt = 0.0;
        this->last_operation_ = COVER_OPERATION_OPENING;
      }
    break;
    case COVER_OPERATION_CLOSING:
      ESP_LOGD(TAG, "Blind 0x%06x: sending CLOSE (cmd=0x%02x)", this->command_.blind_addr, this->command_down_);
      if (this->commands_to_send_.size() < ELERO_MAX_COMMAND_QUEUE) {
        this->commands_to_send_.push(this->command_down_);
        // Reset tilt state on movement
        this->tilt = 0.0;
        this->last_operation_ = COVER_OPERATION_CLOSING;
      }
    break;
    case COVER_OPERATION_IDLE:
      ESP_LOGD(TAG, "Blind 0x%06x: sending STOP (cmd=0x%02x)", this->command_.blind_addr, this->command_stop_);
      // Clear any pending movement commands so STOP is sent immediately
      while (!this->commands_to_send_.empty())
        this->commands_to_send_.pop();
      this->commands_to_send_.push(this->command_stop_);
    break;
  }

  if(dir == this->current_operation)
    return;

  this->current_operation = dir;
  this->movement_start_ = millis();
  this->last_recompute_time_ = millis();

  if(dir == COVER_OPERATION_OPENING && this->open_duration_ > 0) {
    float travel = this->target_position_ - this->position;
    if (travel < 0.0f) travel = 1.0f;  // fallback to full duration
    uint32_t travel_ms = static_cast<uint32_t>(travel * this->open_duration_);
    this->post_movement_poll_at_ = this->movement_start_ + travel_ms + ELERO_POST_MOVEMENT_POLL_DELAY;
  } else if(dir == COVER_OPERATION_CLOSING && this->close_duration_ > 0) {
    float travel = this->position - this->target_position_;
    if (travel < 0.0f) travel = 1.0f;  // fallback to full duration
    uint32_t travel_ms = static_cast<uint32_t>(travel * this->close_duration_);
    this->post_movement_poll_at_ = this->movement_start_ + travel_ms + ELERO_POST_MOVEMENT_POLL_DELAY;
  } else {
    this->post_movement_poll_at_ = 0;
  }

  this->publish_state();
}

void EleroCover::schedule_immediate_poll() {
  if (this->commands_to_send_.size() < ELERO_MAX_COMMAND_QUEUE) {
    this->commands_to_send_.push(this->command_check_);
  }
}

void EleroCover::recompute_position() {
  if(this->current_operation == COVER_OPERATION_IDLE)
    return;

  float dir;
  float action_dur;
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      dir = 1.0f;
      action_dur = this->open_duration_;
      break;
    case COVER_OPERATION_CLOSING:
      dir = -1.0f;
      action_dur = this->close_duration_;
      break;
    default:
      return;
  }

  // Guard against division by zero (happens if durations not configured)
  if (action_dur == 0.0f)
    return;

  const uint32_t now = millis();
  this->position += dir * (now - this->last_recompute_time_) / action_dur;
  this->position = clamp(this->position, 0.0f, 1.0f);

  this->last_recompute_time_ = now;
}

} // namespace elero
} // namespace esphome
