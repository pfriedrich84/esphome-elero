#pragma once

// Dependency-light semantic Command intent lane.
//
// A lane owns only per-device queue policy and its immutable packet profile.
// Ordering, repeat/retry state, rolling counters, and RF submission belong
// exclusively to ProfileDeliveryCoordinator.

#include "elero_command_profile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>

namespace esphome {
namespace elero {

static const uint8_t ELERO_MAX_DESTS = 10;
static const uint8_t ELERO_MAX_COMMAND_QUEUE = 10;
static const uint8_t ELERO_SEND_RETRIES = 3;
static const uint32_t ELERO_COMMAND_QUEUE_MAX_AGE_MS = 30000;

enum class SendResult : uint8_t { OK, QUEUE_FULL, FAILED };

// Result of handing one RF packet to the hub. Production submissions are
// asynchronous and carry a transaction ID completed by the radio task. The
// converting constructor keeps dependency-light tests and synchronous users
// able to report an immediate result.
struct PacketSubmission {
  SendResult result{SendResult::FAILED};
  uint32_t transaction_id{0};
  bool completed_inline{false};

  PacketSubmission() = default;
  PacketSubmission(SendResult immediate_result)
      : result(immediate_result), completed_inline(immediate_result == SendResult::OK) {}

  static PacketSubmission queued(uint32_t transaction_id) {
    return {SendResult::OK, transaction_id, false};
  }

 private:
  PacketSubmission(SendResult result, uint32_t transaction_id, bool completed_inline)
      : result(result), transaction_id(transaction_id), completed_inline(completed_inline) {}
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

inline CommandIntent cover_intent_for_command_byte(const CommandMapping &mapping, uint8_t value) {
  if (value == mapping.stop)
    return {CommandIntentKind::STOP, 0};
  if (value == mapping.open)
    return {CommandIntentKind::OPEN, 0};
  if (value == mapping.close)
    return {CommandIntentKind::CLOSE, 0};
  if (value == mapping.check)
    return {CommandIntentKind::CHECK, 0};
  if (value == mapping.tilt)
    return {CommandIntentKind::TILT, 0};
  return CommandIntent::custom(value);
}

inline CommandIntent light_intent_for_command_byte(const CommandMapping &mapping, uint8_t value) {
  if (value == mapping.stop)
    return {CommandIntentKind::STOP, 0};
  if (value == mapping.check)
    return {CommandIntentKind::CHECK, 0};
  if (value == mapping.on)
    return {CommandIntentKind::ON, 0};
  if (value == mapping.off)
    return {CommandIntentKind::OFF, 0};
  if (value == mapping.dim_up)
    return {CommandIntentKind::DIM_UP, 0};
  if (value == mapping.dim_down)
    return {CommandIntentKind::DIM_DOWN, 0};
  return CommandIntent::custom(value);
}

struct CommandDeliveryConfig {
  BlindCommandProfile profile{};
  CommandMapping mapping{};
  uint8_t destination_count{1};
  uint32_t destinations[ELERO_MAX_DESTS]{};
};

enum class IntentSubmitResult : uint8_t { ACCEPTED, COALESCED, REJECTED };

enum class DeliveryEvent : uint8_t {
  IDLE,
  WAITING,
  QUEUE_FULL,
  PACKET_ACCEPTED,
  COMPLETED,
  RETRY_SCHEDULED,
  DROPPED,
  STALE_CLEARED,
  FALLBACK_STARTED,
  FALLBACK_MEMBER_DROPPED,
};

struct DeliveryOutcome {
  DeliveryEvent event{DeliveryEvent::IDLE};
  CommandIntent intent{};
  bool had_partial_delivery{false};
  uint8_t accepted_repeats{0};
  uint8_t queue_size{0};
  // Monotonic timestamp reported by Core 0 after the RF packet actually
  // completed. Zero for queueing, waiting, and failure outcomes.
  uint32_t transmitted_at_ms{0};
  bool first_transmission{false};
  bool fallback_member{false};
  uint8_t fallback_member_index{0};
};

class ProfileDeliveryCoordinator;

class CommandIntentDelivery {
 public:
  using OutcomeCallback = std::function<void(const DeliveryOutcome &)>;

  explicit CommandIntentDelivery(const CommandDeliveryConfig &config = CommandDeliveryConfig())
      : config_(config) {
    this->normalise_destinations_();
  }

  CommandIntentDelivery(const CommandIntentDelivery &) = delete;
  CommandIntentDelivery &operator=(const CommandIntentDelivery &) = delete;

  void configure(const CommandDeliveryConfig &config);
  void set_outcome_callback(OutcomeCallback callback);

  // Deferred intents count against the same per-device bound. CHECK and STOP
  // remain eligible while deferred movement work waits for verification.
  // submitted_at_ms must use the monotonic clock passed to coordinator.advance().
  IntentSubmitResult submit(const CommandIntent &intent, uint32_t submitted_at_ms,
                            bool deferred = false);
  IntentSubmitResult submit_batch(const CommandIntent *intents, size_t count,
                                  uint32_t submitted_at_ms, bool deferred = false);

  struct AtomicIntentTarget {
    CommandIntentDelivery *delivery;
    CommandIntent intent;
    bool deferred{false};
  };
  static IntentSubmitResult submit_atomic(const AtomicIntentTarget *targets, size_t count,
                                          uint32_t submitted_at_ms);

  void release_deferred();
  void postpone_until(uint32_t not_before_ms);
  void discard_pending();
  void discard_checks();
  size_t size() const;
  bool empty() const { return this->size() == 0; }
  CommandDeliveryConfig config() const;

  // A native group lane uses these member lanes only after final native
  // failure with zero accepted repeats. The coordinator keeps the original
  // sequence position while delivering the member fallback.
  bool set_native_fallback(CommandIntentDelivery *const *members, size_t count);

 private:
  friend class ProfileDeliveryCoordinator;

  struct QueuedIntent {
    CommandIntent intent{};
    uint64_t sequence{0};
    uint32_t last_progress_ms{0};
    bool deferred{false};
    bool fallback_active{false};
    uint8_t fallback_index{0};
  };

  struct QueueState {
    std::array<QueuedIntent, ELERO_MAX_COMMAND_QUEUE> entries{};
    uint8_t size{0};
  };

  static bool is_target_(CommandIntentKind kind);
  static bool same_target_family_(CommandIntentKind current, CommandIntentKind next);
  static bool conflicts_(CommandIntentKind current, CommandIntentKind next);
  IntentSubmitResult submit_to_state_(QueueState &state, const CommandIntent &intent,
                                      bool deferred, uint64_t sequence,
                                      uint32_t submitted_at_ms,
                                      uint64_t protected_sequence = 0) const;
  void erase_at_(size_t index);
  void normalise_destinations_();

  mutable std::mutex unbound_mutex_;
  CommandDeliveryConfig config_{};
  QueueState queue_{};
  ProfileDeliveryCoordinator *coordinator_{nullptr};
  OutcomeCallback outcome_callback_{};
  std::array<CommandDeliveryConfig, ELERO_MAX_DESTS> fallback_configs_{};
  uint8_t fallback_member_count_{0};
  uint32_t not_before_ms_{0};
};

inline bool intent_was_accepted(IntentSubmitResult result) {
  return result != IntentSubmitResult::REJECTED;
}

inline bool delivery_packet_was_accepted(DeliveryEvent event) {
  return event == DeliveryEvent::PACKET_ACCEPTED || event == DeliveryEvent::COMPLETED;
}

}  // namespace elero
}  // namespace esphome
