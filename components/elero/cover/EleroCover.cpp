#include "EleroCover.h"
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
}

void EleroCover::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "Elero parent not configured");
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
}

void EleroCover::loop() {
  uint32_t intvl = this->poll_intvl_;
  uint32_t now = millis();
  if(this->current_operation != COVER_OPERATION_IDLE) {
    if((now - this->movement_start_) < ELERO_TIMEOUT_MOVEMENT)  // Poll frequently while moving (up to 2 min timeout)
      intvl = ELERO_POLL_INTERVAL_MOVING;
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

  // Stop verification: poll motor to confirm it actually stopped
  if (this->stop_verify_at_ > 0 && now >= this->stop_verify_at_) {
    if (this->stop_verify_retries_ < ELERO_STOP_VERIFY_MAX_RETRIES) {
      this->stop_verify_retries_++;
      ESP_LOGD(TAG, "Stop verify poll #%d for blind 0x%06x",
               this->stop_verify_retries_, this->command_.blind_addr);
      if (this->commands_to_send_.size() < ELERO_MAX_COMMAND_QUEUE)
        this->commands_to_send_.push(this->command_check_);
      // Reschedule in case no RF response arrives (prevents verification stall)
      this->stop_verify_at_ = now + ELERO_STOP_VERIFY_DELAY_MS;
    } else {
      // Exhausted retries — give up verification
      ESP_LOGW(TAG, "Stop verification exhausted %d retries for blind 0x%06x",
               ELERO_STOP_VERIFY_MAX_RETRIES, this->command_.blind_addr);
      this->stop_verify_at_ = 0;
    }
  }

  this->handle_commands(now);

  if((this->current_operation != COVER_OPERATION_IDLE) && (this->open_duration_ > 0) && (this->close_duration_ > 0)) {
    this->recompute_position();
    if(this->is_at_target()) {
      ESP_LOGI(TAG, "Blind 0x%06x reached target (pos=%.2f, target=%.2f), sending stop",
               this->command_.blind_addr, this->position, this->target_position_);
      // Clear queue so stop goes out immediately (mirrors manual stop behavior)
      while (!this->commands_to_send_.empty())
        this->commands_to_send_.pop();
      // Send stop multiple times for RF reliability
      for (uint8_t i = 0; i < ELERO_STOP_REPEAT_COUNT; i++)
        this->commands_to_send_.push(this->command_stop_);
      this->current_operation = COVER_OPERATION_IDLE;
      this->target_position_ = COVER_OPEN;
      // Schedule verification poll to confirm motor actually stopped
      this->stop_verify_at_ = now + ELERO_STOP_VERIFY_DELAY_MS;
      this->stop_verify_retries_ = 0;
      // Publish final position immediately — the 1-second throttle below may
      // skip this, and the next loop() won't enter this block (operation is IDLE),
      // leaving HA stuck showing "opening/closing" with a stale position.
      this->publish_state(false);
      this->last_publish_ = now;
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

  // Compensate for TX pipeline latency: trigger stop slightly early so the
  // RF packet reaches the motor before it overshoots the target position.
  float margin = 0.0f;
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      if (this->open_duration_ > 0)
        margin = static_cast<float>(ELERO_TX_LATENCY_COMPENSATION_MS) / this->open_duration_;
      return this->position >= (this->target_position_ - margin);
    case COVER_OPERATION_CLOSING:
      if (this->close_duration_ > 0)
        margin = static_cast<float>(ELERO_TX_LATENCY_COMPENSATION_MS) / this->close_duration_;
      return this->position <= (this->target_position_ + margin);
    case COVER_OPERATION_IDLE:
    default:
      return true;
  }
}

void EleroCover::handle_commands(uint32_t now) {
  // Don't attempt TX while the radio is busy — try again next loop()
  if (!this->parent_->is_tx_idle()) return;

  if((now - this->last_command_) > ELERO_DELAY_SEND_PACKETS) {
    if(this->commands_to_send_.size() > 0) {
      this->command_.payload[4] = this->commands_to_send_.front();
      if(this->parent_->send_command(&this->command_)) {
        this->send_packets_++;
        this->send_retries_ = 0;
        if(this->send_packets_ >= ELERO_SEND_PACKETS) {
          this->commands_to_send_.pop();
          this->send_packets_ = 0;
          this->increase_counter();
        }
        this->last_command_ = now;
      } else {
        ESP_LOGD(TAG, "Retry #%d for blind 0x%06x", this->send_retries_, this->command_.blind_addr);
        this->send_retries_++;
        if(this->send_retries_ > ELERO_SEND_RETRIES) {
          ESP_LOGE(TAG, "Hit maximum number of retries, giving up.");
          this->send_retries_ = 0;
          this->commands_to_send_.pop();
        }
        this->last_command_ = now;
      }
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
    break;
  case ELERO_STATE_OVERHEATED:
    ESP_LOGW(TAG, "Blind 0x%06x reports OVERHEATED", this->command_.blind_addr);
    op = COVER_OPERATION_IDLE;
    break;
  case ELERO_STATE_TIMEOUT:
    ESP_LOGW(TAG, "Blind 0x%06x reports TIMEOUT", this->command_.blind_addr);
    op = COVER_OPERATION_IDLE;
    break;
  default:
    op = COVER_OPERATION_IDLE;
    current_tilt = 0.0;
  }

  // Stop verification: if we sent a stop and are waiting for confirmation,
  // check whether the motor actually stopped or is still moving.
  if (this->stop_verify_retries_ < ELERO_STOP_VERIFY_MAX_RETRIES) {
    if (state == ELERO_STATE_MOVING_UP || state == ELERO_STATE_MOVING_DOWN ||
        state == ELERO_STATE_START_MOVING_UP || state == ELERO_STATE_START_MOVING_DOWN) {
      // Motor is still moving — re-send stop
      this->stop_verify_retries_++;
      ESP_LOGW(TAG, "Blind 0x%06x still moving after stop, retry #%d",
               this->command_.blind_addr, this->stop_verify_retries_);
      while (!this->commands_to_send_.empty())
        this->commands_to_send_.pop();
      for (uint8_t i = 0; i < ELERO_STOP_REPEAT_COUNT; i++)
        this->commands_to_send_.push(this->command_stop_);
      this->stop_verify_at_ = millis() + ELERO_STOP_VERIFY_DELAY_MS;
      op = COVER_OPERATION_IDLE;  // keep our side idle while retrying
    } else {
      // Motor confirmed stopped — clear verification state
      this->stop_verify_retries_ = ELERO_STOP_VERIFY_MAX_RETRIES;
      this->stop_verify_at_ = 0;
    }
  }

  if((pos != this->position) || (op != this->current_operation) || (current_tilt != this->tilt)) {
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
    // Dead-zone: if already within 1% of target (and not requesting fully
    // open/closed), skip movement.  Without this, requesting the exact
    // current position would incorrectly start closing because
    // (pos > this->position) is false when they're equal.
    if (pos != COVER_OPEN && pos != COVER_CLOSED &&
        (this->open_duration_ > 0) && (this->close_duration_ > 0) &&
        std::abs(pos - this->position) < 0.01f) {
      // Already at target — no movement needed
    } else if((pos > this->position) || (pos == COVER_OPEN)) {
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
  // Cancel any pending stop verification — a new movement command supersedes it
  this->stop_verify_at_ = 0;
  this->stop_verify_retries_ = ELERO_STOP_VERIFY_MAX_RETRIES;

  // When reversing direction while moving, clear the queue so the old
  // direction command isn't sent before the new one.  Without this, a
  // pending UP in the queue would be transmitted before the new DOWN,
  // causing a brief wrong-direction movement and position desync.
  if (dir != COVER_OPERATION_IDLE &&
      this->current_operation != COVER_OPERATION_IDLE &&
      dir != this->current_operation) {
    while (!this->commands_to_send_.empty())
      this->commands_to_send_.pop();
  }

  switch(dir) {
    case COVER_OPERATION_OPENING:
      ESP_LOGV(TAG, "Sending OPEN command");
      if (this->commands_to_send_.size() < ELERO_MAX_COMMAND_QUEUE) {
        this->commands_to_send_.push(this->command_up_);
        // Reset tilt state on movement
        this->tilt = 0.0;
        this->last_operation_ = COVER_OPERATION_OPENING;
      }
    break;
    case COVER_OPERATION_CLOSING:
      ESP_LOGV(TAG, "Sending CLOSE command");
      if (this->commands_to_send_.size() < ELERO_MAX_COMMAND_QUEUE) {
        this->commands_to_send_.push(this->command_down_);
        // Reset tilt state on movement
        this->tilt = 0.0;
        this->last_operation_ = COVER_OPERATION_CLOSING;
      }
    break;
    case COVER_OPERATION_IDLE:
      ESP_LOGI(TAG, "Blind 0x%06x manual stop at position %.2f",
               this->command_.blind_addr, this->position);
      // Clear any pending movement commands so STOP is sent immediately
      while (!this->commands_to_send_.empty())
        this->commands_to_send_.pop();
      // Send stop multiple times for RF reliability (mirrors auto-stop behavior)
      for (uint8_t i = 0; i < ELERO_STOP_REPEAT_COUNT; i++)
        this->commands_to_send_.push(this->command_stop_);
      // Schedule verification to confirm motor actually stopped
      this->stop_verify_at_ = millis() + ELERO_STOP_VERIFY_DELAY_MS;
      this->stop_verify_retries_ = 0;
    break;
  }

  if(dir == this->current_operation)
    return;

  this->current_operation = dir;
  this->movement_start_ = millis();
  this->last_recompute_time_ = millis();

  if(dir == COVER_OPERATION_OPENING && this->open_duration_ > 0) {
    this->post_movement_poll_at_ = this->movement_start_ + this->open_duration_ + ELERO_POST_MOVEMENT_POLL_DELAY;
  } else if(dir == COVER_OPERATION_CLOSING && this->close_duration_ > 0) {
    this->post_movement_poll_at_ = this->movement_start_ + this->close_duration_ + ELERO_POST_MOVEMENT_POLL_DELAY;
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
