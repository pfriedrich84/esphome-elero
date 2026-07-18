#pragma once

// Dependency-light, per-device semantic Command intent delivery.
// This Module owns queueing, coalescing, repeats, retries, stale aging and the
// rolling RF counter. It deliberately has no ESPHome, FreeRTOS or radio dependency.

#include "elero_command_profile.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>

namespace esphome {
namespace elero {

static const uint8_t ELERO_MAX_DESTS = 10;
static const uint8_t ELERO_MAX_COMMAND_QUEUE = 10;
static const uint8_t ELERO_SEND_RETRIES = 3;
static const uint32_t ELERO_COMMAND_QUEUE_MAX_AGE_MS = 30000;

enum class SendResult : uint8_t {
  OK,
  QUEUE_FULL,
  FAILED,
};

typedef struct {
  uint8_t counter;
  uint32_t blind_addr;
  uint32_t remote_addr;
  uint8_t channel;
  uint8_t pck_inf[2];
  uint8_t hop;
  uint8_t payload[10];
  uint8_t num_dests{1};
  uint32_t dest_addrs[ELERO_MAX_DESTS]{};
} t_elero_command;

enum class CommandIntentKind : uint8_t {
  OPEN,
  CLOSE,
  STOP,
  CHECK,
  TILT,
  ON,
  OFF,
  DIM_UP,
  DIM_DOWN,
  CUSTOM,
};

struct CommandIntent {
  CommandIntentKind kind{CommandIntentKind::CHECK};
  uint8_t custom_byte{0};

  static CommandIntent custom(uint8_t value) { return {CommandIntentKind::CUSTOM, value}; }

  bool operator==(const CommandIntent &other) const {
    return this->kind == other.kind &&
           (this->kind != CommandIntentKind::CUSTOM || this->custom_byte == other.custom_byte);
  }
};

struct CommandMapping {
  uint8_t open{0x20};
  uint8_t close{0x40};
  uint8_t stop{0x10};
  uint8_t check{0x00};
  uint8_t tilt{0x24};
  uint8_t on{0x20};
  uint8_t off{0x40};
  uint8_t dim_up{0x20};
  uint8_t dim_down{0x40};

  uint8_t resolve(const CommandIntent &intent) const {
    switch (intent.kind) {
      case CommandIntentKind::OPEN: return this->open;
      case CommandIntentKind::CLOSE: return this->close;
      case CommandIntentKind::STOP: return this->stop;
      case CommandIntentKind::CHECK: return this->check;
      case CommandIntentKind::TILT: return this->tilt;
      case CommandIntentKind::ON: return this->on;
      case CommandIntentKind::OFF: return this->off;
      case CommandIntentKind::DIM_UP: return this->dim_up;
      case CommandIntentKind::DIM_DOWN: return this->dim_down;
      case CommandIntentKind::CUSTOM: return intent.custom_byte;
    }
    return intent.custom_byte;
  }
};

struct CommandDeliveryConfig {
  BlindCommandProfile profile{};
  CommandMapping mapping{};
  uint8_t destination_count{1};
  uint32_t destinations[ELERO_MAX_DESTS]{};
};

enum class IntentSubmitResult : uint8_t {
  ACCEPTED,
  COALESCED,
  REJECTED,
};

enum class DeliveryEvent : uint8_t {
  IDLE,
  WAITING,
  QUEUE_FULL,
  PACKET_ACCEPTED,
  COMPLETED,
  RETRY_SCHEDULED,
  DROPPED,
  STALE_CLEARED,
};

struct DeliveryOutcome {
  DeliveryEvent event{DeliveryEvent::IDLE};
  CommandIntent intent{};
  bool had_partial_delivery{false};
  uint8_t accepted_repeats{0};
  uint8_t queue_size{0};
};

class CommandIntentDelivery {
 public:
  using SubmitCallback = std::function<SendResult(const t_elero_command &, bool priority)>;

  explicit CommandIntentDelivery(const CommandDeliveryConfig &config = CommandDeliveryConfig())
      : config_(config) {
    this->normalise_destinations_();
  }

  CommandIntentDelivery(const CommandIntentDelivery &) = delete;
  CommandIntentDelivery &operator=(const CommandIntentDelivery &) = delete;

  void configure(const CommandDeliveryConfig &config) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    this->config_ = config;
    this->normalise_destinations_();
  }

  IntentSubmitResult submit(const CommandIntent &intent) {
    std::lock_guard<std::mutex> lock(this->mutex_);

    // STOP supersedes all older intent. A partially accepted intent consumed
    // its rolling counter even though it never completed.
    if (intent.kind == CommandIntentKind::STOP) {
      if (this->queue_.size() == 1 && this->accepted_repeats_ == 0 && this->queue_.front() == intent)
        return IntentSubmitResult::COALESCED;
      if (this->accepted_repeats_ > 0)
        this->advance_counter_();
      this->queue_.clear();
      this->accepted_repeats_ = 0;
      this->failure_count_ = 0;
      this->age_started_ = false;
      this->queue_.push_back(intent);
      return IntentSubmitResult::ACCEPTED;
    }

    // CHECK is polling, not ordered user work. One pending CHECK is enough.
    if (intent.kind == CommandIntentKind::CHECK) {
      for (const auto &queued : this->queue_) {
        if (queued.kind == CommandIntentKind::CHECK)
          return IntentSubmitResult::COALESCED;
      }
    }

    // Identical unsent semantic intents collapse. CUSTOM intentionally remains
    // ordered because its meaning is not known to this Module.
    if (intent.kind != CommandIntentKind::CUSTOM) {
      for (size_t i = 0; i < this->queue_.size(); i++) {
        if (this->queue_[i] == intent && !(i == 0 && this->accepted_repeats_ > 0))
          return IntentSubmitResult::COALESCED;
      }
    }

    // A newer conflicting target state replaces only an unsent older target.
    for (size_t i = this->queue_.size(); i > 0; i--) {
      const size_t index = i - 1;
      if (this->conflicts_(this->queue_[index].kind, intent.kind)) {
        if (index == 0 && this->accepted_repeats_ > 0)
          break;
        this->queue_[index] = intent;
        return IntentSubmitResult::COALESCED;
      }
    }

    if (this->queue_.size() >= ELERO_MAX_COMMAND_QUEUE)
      return IntentSubmitResult::REJECTED;
    this->queue_.push_back(intent);
    return IntentSubmitResult::ACCEPTED;
  }

  DeliveryOutcome advance(uint32_t now, uint32_t base_delay_ms, uint8_t required_repeats,
                          const SubmitCallback &submitter) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (this->queue_.empty()) {
      this->age_started_ = false;
      return this->outcome_(DeliveryEvent::IDLE, CommandIntent{});
    }
    if (!this->age_started_) {
      this->last_progress_ms_ = now;
      this->age_started_ = true;
    } else if ((now - this->last_progress_ms_) > ELERO_COMMAND_QUEUE_MAX_AGE_MS) {
      CommandIntent stale = this->queue_.front();
      const bool partial = this->accepted_repeats_ > 0;
      if (partial)
        this->advance_counter_();
      this->queue_.clear();
      this->accepted_repeats_ = 0;
      this->failure_count_ = 0;
      this->age_started_ = false;
      return this->outcome_(DeliveryEvent::STALE_CLEARED, stale, partial);
    }

    const CommandIntent intent = this->queue_.front();
    const bool urgent = intent.kind == CommandIntentKind::STOP;
    const uint8_t shift = std::min(this->failure_count_, static_cast<uint8_t>(3));
    const uint32_t delay = (urgent ? 0u : base_delay_ms) +
                           (this->failure_count_ == 0 ? 0u : (10u << shift));
    if (this->attempt_started_ && (now - this->last_attempt_ms_) <= delay)
      return this->outcome_(DeliveryEvent::WAITING, intent);

    t_elero_command packet = this->build_packet_(intent);
    const SendResult result = submitter(packet, urgent);
    if (result == SendResult::QUEUE_FULL)
      return this->outcome_(DeliveryEvent::QUEUE_FULL, intent);

    this->last_attempt_ms_ = now;
    this->attempt_started_ = true;
    if (result == SendResult::FAILED) {
      this->failure_count_++;
      if (this->failure_count_ <= ELERO_SEND_RETRIES)
        return this->outcome_(DeliveryEvent::RETRY_SCHEDULED, intent,
                              this->accepted_repeats_ > 0);

      const bool partial = this->accepted_repeats_ > 0;
      if (partial)
        this->advance_counter_();
      this->queue_.pop_front();
      this->accepted_repeats_ = 0;
      this->failure_count_ = 0;
      this->last_progress_ms_ = now;
      if (this->queue_.empty())
        this->age_started_ = false;
      return this->outcome_(DeliveryEvent::DROPPED, intent, partial);
    }

    this->failure_count_ = 0;
    this->accepted_repeats_++;
    const uint8_t repeats = required_repeats == 0 ? 1 : required_repeats;
    if (this->accepted_repeats_ < repeats)
      return this->outcome_(DeliveryEvent::PACKET_ACCEPTED, intent, true);

    this->queue_.pop_front();
    this->accepted_repeats_ = 0;
    this->advance_counter_();
    this->last_progress_ms_ = now;
    if (this->queue_.empty())
      this->age_started_ = false;
    return this->outcome_(DeliveryEvent::COMPLETED, intent);
  }

  void discard_pending() {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (this->accepted_repeats_ > 0)
      this->advance_counter_();
    this->queue_.clear();
    this->accepted_repeats_ = 0;
    this->failure_count_ = 0;
    this->age_started_ = false;
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->queue_.size();
  }
  bool empty() const { return this->size() == 0; }
  uint8_t counter() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->counter_;
  }
  uint8_t accepted_repeats() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->accepted_repeats_;
  }
  CommandDeliveryConfig config() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->config_;
  }

 private:
  static bool conflicts_(CommandIntentKind current, CommandIntentKind next) {
    return ((current == CommandIntentKind::OPEN || current == CommandIntentKind::CLOSE) &&
            (next == CommandIntentKind::OPEN || next == CommandIntentKind::CLOSE)) ||
           ((current == CommandIntentKind::ON || current == CommandIntentKind::OFF) &&
            (next == CommandIntentKind::ON || next == CommandIntentKind::OFF)) ||
           ((current == CommandIntentKind::DIM_UP || current == CommandIntentKind::DIM_DOWN) &&
            (next == CommandIntentKind::DIM_UP || next == CommandIntentKind::DIM_DOWN));
  }

  void normalise_destinations_() {
    if (this->config_.destination_count == 0 || this->config_.destination_count > ELERO_MAX_DESTS)
      this->config_.destination_count = 1;
    if (this->config_.destinations[0] == 0)
      this->config_.destinations[0] = this->config_.profile.blind_address;
  }

  t_elero_command build_packet_(const CommandIntent &intent) const {
    t_elero_command packet{};
    packet.counter = this->counter_;
    packet.blind_addr = this->config_.profile.blind_address;
    packet.remote_addr = this->config_.profile.remote_address;
    packet.channel = this->config_.profile.channel;
    packet.pck_inf[0] = this->config_.profile.pck_inf[0];
    packet.pck_inf[1] = this->config_.profile.pck_inf[1];
    packet.hop = this->config_.profile.hop;
    packet.payload[0] = this->config_.profile.payload_1;
    packet.payload[1] = this->config_.profile.payload_2;
    packet.payload[4] = this->config_.mapping.resolve(intent);
    packet.num_dests = this->config_.destination_count;
    for (uint8_t i = 0; i < packet.num_dests; i++)
      packet.dest_addrs[i] = this->config_.destinations[i];
    return packet;
  }

  void advance_counter_() { this->counter_ = this->counter_ == 0xFF ? 1 : this->counter_ + 1; }

  DeliveryOutcome outcome_(DeliveryEvent event, const CommandIntent &intent,
                           bool partial = false) const {
    DeliveryOutcome outcome{};
    outcome.event = event;
    outcome.intent = intent;
    outcome.had_partial_delivery = partial;
    outcome.accepted_repeats = this->accepted_repeats_;
    outcome.queue_size = static_cast<uint8_t>(this->queue_.size());
    return outcome;
  }

  mutable std::mutex mutex_;
  CommandDeliveryConfig config_{};
  std::deque<CommandIntent> queue_;
  uint8_t counter_{1};
  uint8_t accepted_repeats_{0};
  uint8_t failure_count_{0};
  uint32_t last_attempt_ms_{0};
  uint32_t last_progress_ms_{0};
  bool age_started_{false};
  bool attempt_started_{false};
};

inline bool intent_was_accepted(IntentSubmitResult result) {
  return result != IntentSubmitResult::REJECTED;
}

}  // namespace elero
}  // namespace esphome
