#pragma once

// Dependency-light profile-scoped delivery coordinator.
//
// There is one instance for each RF remote profile. It is the sole authority
// for cross-device ordering, rolling-counter allocation, repeat/retry state,
// and packet submission. Per-device CommandIntentDelivery objects are bounded
// semantic lanes only.

#include "elero_command_delivery.h"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

namespace esphome {
namespace elero {

struct DeliveryProfileKey {
  uint32_t remote_address{0};
  uint8_t channel{0};
  uint8_t pck_inf[2]{};
  uint8_t hop{0};
  uint8_t payload_1{0};
  uint8_t payload_2{0};

  static DeliveryProfileKey from(const BlindCommandProfile &profile) {
    return {profile.remote_address, profile.channel, {profile.pck_inf[0], profile.pck_inf[1]},
            profile.hop, profile.payload_1, profile.payload_2};
  }

  bool operator==(const DeliveryProfileKey &other) const {
    return std::tie(this->remote_address, this->channel, this->pck_inf[0], this->pck_inf[1],
                    this->hop, this->payload_1, this->payload_2) ==
           std::tie(other.remote_address, other.channel, other.pck_inf[0], other.pck_inf[1],
                    other.hop, other.payload_1, other.payload_2);
  }
  bool operator!=(const DeliveryProfileKey &other) const { return !(*this == other); }
  bool operator<(const DeliveryProfileKey &other) const {
    return std::tie(this->remote_address, this->channel, this->pck_inf[0], this->pck_inf[1],
                    this->hop, this->payload_1, this->payload_2) <
           std::tie(other.remote_address, other.channel, other.pck_inf[0], other.pck_inf[1],
                    other.hop, other.payload_1, other.payload_2);
  }
};

class ProfileDeliveryCoordinator {
 public:
  using SubmitCallback = std::function<PacketSubmission(const t_elero_command &, bool priority)>;

  explicit ProfileDeliveryCoordinator(const DeliveryProfileKey &key) : key_(key) {}
  ProfileDeliveryCoordinator(const ProfileDeliveryCoordinator &) = delete;
  ProfileDeliveryCoordinator &operator=(const ProfileDeliveryCoordinator &) = delete;

  const DeliveryProfileKey &key() const { return this->key_; }

  bool attach(CommandIntentDelivery *delivery) {
    if (delivery == nullptr || DeliveryProfileKey::from(delivery->config_.profile) != this->key_)
      return false;
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (delivery->coordinator_ == this)
      return true;
    if (delivery->coordinator_ != nullptr)
      return false;
    delivery->coordinator_ = this;
    this->lanes_.push_back(delivery);
    return true;
  }

  bool detach(CommandIntentDelivery *delivery) {
    if (delivery == nullptr)
      return false;
    std::lock_guard<std::mutex> lock(this->mutex_);
    auto it = std::find(this->lanes_.begin(), this->lanes_.end(), delivery);
    if (it == this->lanes_.end())
      return false;
    if (this->active_lane_ == delivery) {
      if (this->accepted_repeats_ > 0 || this->pending_transaction_id_ != 0)
        this->advance_counter_();
      this->reset_active_();
    }
    this->lanes_.erase(it);
    delivery->queue_.size = 0;
    delivery->coordinator_ = nullptr;
    return true;
  }

  DeliveryOutcome advance(uint32_t now, uint32_t base_delay_ms, uint8_t required_repeats,
                          const SubmitCallback &submitter) {
    AttemptDispatch dispatch{};
    {
      std::unique_lock<std::mutex> lock(this->mutex_);
      if (this->active_lane_ == nullptr && !this->select_next_locked_(now))
        return {};

      auto &entry = this->active_lane_->queue_.entries[this->active_index_];
      const CommandIntent intent = entry.intent;
      if (this->pending_transaction_id_ != 0)
        return this->outcome_locked_(DeliveryEvent::WAITING, intent);

      if ((now - entry.last_progress_ms) > ELERO_COMMAND_QUEUE_MAX_AGE_MS) {
        const bool partial = this->accepted_repeats_ > 0;
        if (partial)
          this->advance_counter_();
        this->active_lane_->queue_.size = 0;
        dispatch.outcome = this->outcome_locked_(DeliveryEvent::STALE_CLEARED, intent, partial);
        dispatch.callback = this->active_lane_->outcome_callback_;
        dispatch.notify = true;
        this->reset_active_();
      } else {
        const bool urgent = intent.kind == CommandIntentKind::STOP;
        const uint8_t shift = std::min(this->failure_count_, static_cast<uint8_t>(3));
        const uint32_t delay = (urgent ? 0u : base_delay_ms) +
                               (this->failure_count_ == 0 ? 0u : (10u << shift));
        if (this->attempt_started_ && (now - this->last_attempt_ms_) <= delay)
          return this->outcome_locked_(DeliveryEvent::WAITING, intent);

        const CommandDeliveryConfig &packet_config = this->active_config_locked_(entry);
        const PacketSubmission submission = submitter(this->build_packet_(packet_config, intent), urgent);
        if (submission.result == SendResult::QUEUE_FULL)
          return this->outcome_locked_(DeliveryEvent::QUEUE_FULL, intent);
        if (submission.result == SendResult::OK && !submission.completed_inline) {
          if (submission.transaction_id == 0)
            dispatch = this->finish_attempt_locked_(false, now, required_repeats);
          else {
            this->pending_transaction_id_ = submission.transaction_id;
            this->pending_required_repeats_ = required_repeats;
            return this->outcome_locked_(DeliveryEvent::WAITING, intent);
          }
        } else {
          dispatch = this->finish_attempt_locked_(submission.result == SendResult::OK, now,
                                                  required_repeats);
        }
      }
    }
    if (dispatch.notify && dispatch.callback)
      dispatch.callback(dispatch.outcome);
    return dispatch.outcome;
  }

  // Complete an asynchronous packet submission after Core 0 has observed the
  // real RF outcome. Unknown/stale transaction IDs are deliberately ignored.
  DeliveryOutcome complete(uint32_t transaction_id, bool success, uint32_t completed_at_ms) {
    AttemptDispatch dispatch{};
    {
      std::lock_guard<std::mutex> lock(this->mutex_);
      if (transaction_id == 0 || transaction_id != this->pending_transaction_id_ ||
          this->active_lane_ == nullptr)
        return {};
      this->pending_transaction_id_ = 0;
      dispatch = this->finish_attempt_locked_(success, completed_at_ms,
                                              this->pending_required_repeats_);
    }
    if (dispatch.notify && dispatch.callback)
      dispatch.callback(dispatch.outcome);
    return dispatch.outcome;
  }

  uint8_t counter() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->counter_;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->lanes_.empty();
  }

 private:
  friend class CommandIntentDelivery;

  struct AttemptDispatch {
    DeliveryOutcome outcome{};
    CommandIntentDelivery::OutcomeCallback callback{};
    bool notify{false};
  };

  AttemptDispatch finish_attempt_locked_(bool success, uint32_t completed_at_ms,
                                          uint8_t required_repeats) {
    AttemptDispatch dispatch{};
    if (this->active_lane_ == nullptr)
      return dispatch;

    auto &entry = this->active_lane_->queue_.entries[this->active_index_];
    const CommandIntent intent = entry.intent;
    const bool fallback_member = entry.fallback_active;
    const uint8_t fallback_member_index = entry.fallback_index;
    this->last_attempt_ms_ = completed_at_ms;
    this->attempt_started_ = true;

    if (!success) {
      if (this->stop_after_pending_) {
        const bool partial = this->accepted_repeats_ > 0;
        if (partial)
          this->advance_counter_();
        dispatch.callback = this->active_lane_->outcome_callback_;
        dispatch.notify = true;
        this->active_lane_->erase_at_(this->active_index_);
        dispatch.outcome = this->outcome_locked_(DeliveryEvent::DROPPED, intent, partial);
        this->reset_active_();
        return dispatch;
      }
      this->failure_count_++;
      if (this->failure_count_ <= ELERO_SEND_RETRIES) {
        dispatch.outcome = this->outcome_locked_(DeliveryEvent::RETRY_SCHEDULED, intent,
                                                this->accepted_repeats_ > 0);
        return dispatch;
      }

      const bool partial = this->accepted_repeats_ > 0;
      if (partial)
        this->advance_counter_();
      dispatch.callback = this->active_lane_->outcome_callback_;
      dispatch.notify = true;

      if (!entry.fallback_active && !partial && this->active_lane_->fallback_member_count_ > 0) {
        entry.fallback_active = true;
        entry.fallback_index = 0;
        this->reset_attempt_state_();
        dispatch.outcome = this->outcome_locked_(DeliveryEvent::FALLBACK_STARTED, intent);
      } else if (entry.fallback_active &&
                 entry.fallback_index + 1 < this->active_lane_->fallback_member_count_) {
        entry.fallback_index++;
        this->reset_attempt_state_();
        dispatch.outcome = this->outcome_locked_(DeliveryEvent::FALLBACK_MEMBER_DROPPED,
                                                intent, partial, 0, false,
                                                fallback_member, fallback_member_index);
      } else {
        const bool fallback_active = entry.fallback_active;
        this->active_lane_->erase_at_(this->active_index_);
        dispatch.outcome = this->outcome_locked_(fallback_active
                                                    ? DeliveryEvent::FALLBACK_MEMBER_DROPPED
                                                    : DeliveryEvent::DROPPED,
                                                intent, partial, 0, false,
                                                fallback_member, fallback_member_index);
        this->reset_active_();
      }
      return dispatch;
    }

    this->failure_count_ = 0;
    const bool first_transmission = this->accepted_repeats_ == 0;
    this->accepted_repeats_++;
    entry.last_progress_ms = completed_at_ms;
    dispatch.callback = this->active_lane_->outcome_callback_;
    dispatch.notify = true;
    if (this->stop_after_pending_) {
      this->advance_counter_();
      this->accepted_repeats_ = 0;
      this->active_lane_->erase_at_(this->active_index_);
      dispatch.outcome = this->outcome_locked_(DeliveryEvent::COMPLETED, intent, false,
                                              completed_at_ms, first_transmission,
                                              fallback_member, fallback_member_index);
      this->reset_active_();
      return dispatch;
    }
    const uint8_t repeats = required_repeats == 0 ? 1 : required_repeats;
    if (this->accepted_repeats_ < repeats) {
      dispatch.outcome = this->outcome_locked_(DeliveryEvent::PACKET_ACCEPTED, intent, true,
                                              completed_at_ms, first_transmission,
                                              fallback_member, fallback_member_index);
      return dispatch;
    }

    this->advance_counter_();
    this->accepted_repeats_ = 0;
    if (entry.fallback_active &&
        entry.fallback_index + 1 < this->active_lane_->fallback_member_count_) {
      entry.fallback_index++;
      this->reset_attempt_state_();
      dispatch.outcome = this->outcome_locked_(DeliveryEvent::PACKET_ACCEPTED, intent, true,
                                              completed_at_ms, first_transmission,
                                              fallback_member, fallback_member_index);
    } else {
      this->active_lane_->erase_at_(this->active_index_);
      dispatch.outcome = this->outcome_locked_(DeliveryEvent::COMPLETED, intent, false,
                                              completed_at_ms, first_transmission,
                                              fallback_member, fallback_member_index);
      this->reset_active_();
    }
    return dispatch;
  }

  bool select_next_locked_(uint32_t now) {
    CommandIntentDelivery *best = nullptr;
    size_t best_index = 0;
    uint64_t best_sequence = std::numeric_limits<uint64_t>::max();
    bool found_urgent = false;
    for (auto *lane : this->lanes_) {
      if (lane->not_before_ms_ != 0 && static_cast<int32_t>(now - lane->not_before_ms_) < 0)
        continue;
      for (size_t i = 0; i < lane->queue_.size; i++) {
        const auto &entry = lane->queue_.entries[i];
        const bool urgent = entry.intent.kind == CommandIntentKind::STOP;
        const bool eligible = !entry.deferred || urgent || entry.intent.kind == CommandIntentKind::CHECK;
        if (!eligible)
          continue;
        if ((urgent && !found_urgent) || (urgent == found_urgent && entry.sequence < best_sequence)) {
          best = lane;
          best_index = i;
          best_sequence = entry.sequence;
          found_urgent = urgent;
        }
        // Preserve meaningful per-lane order. Only CHECK may cross deferred work.
        if (!entry.deferred || entry.intent.kind != CommandIntentKind::CHECK)
          break;
      }
    }
    if (best == nullptr)
      return false;
    this->active_lane_ = best;
    this->active_lane_->not_before_ms_ = 0;
    this->active_index_ = best_index;
    this->reset_attempt_state_();
    return true;
  }

  const CommandDeliveryConfig &active_config_locked_(const CommandIntentDelivery::QueuedIntent &entry) const {
    if (entry.fallback_active)
      return this->active_lane_->fallback_configs_[entry.fallback_index];
    return this->active_lane_->config_;
  }

  void preempt_for_stop_locked_(CommandIntentDelivery *stop_lane) {
    if (this->active_lane_ == nullptr)
      return;
    if (this->pending_transaction_id_ != 0) {
      this->stop_after_pending_ = true;
      return;
    }
    if (this->accepted_repeats_ > 0) {
      // A priority STOP cannot reuse a partially transmitted profile counter.
      // The same-lane STOP candidate replaces its queue below; a cross-lane
      // STOP must explicitly remove the interrupted item here.
      this->advance_counter_();
      if (this->active_lane_ != stop_lane)
        this->active_lane_->erase_at_(this->active_index_);
    }
    this->reset_active_();
  }

  void reset_attempt_state_() {
    this->accepted_repeats_ = 0;
    this->failure_count_ = 0;
    this->last_attempt_ms_ = 0;
    this->attempt_started_ = false;
    this->pending_transaction_id_ = 0;
    this->pending_required_repeats_ = 1;
  }

  void reset_active_() {
    this->active_lane_ = nullptr;
    this->active_index_ = 0;
    this->stop_after_pending_ = false;
    this->reset_attempt_state_();
  }

  void advance_counter_() { this->counter_ = this->counter_ == 0xFF ? 1 : this->counter_ + 1; }

  t_elero_command build_packet_(const CommandDeliveryConfig &config,
                                const CommandIntent &intent) const {
    t_elero_command packet{};
    packet.counter = this->counter_;
    packet.blind_addr = config.profile.blind_address;
    packet.remote_addr = config.profile.remote_address;
    packet.channel = config.profile.channel;
    packet.pck_inf[0] = config.profile.pck_inf[0];
    packet.pck_inf[1] = config.profile.pck_inf[1];
    packet.hop = config.profile.hop;
    packet.payload[0] = config.profile.payload_1;
    packet.payload[1] = config.profile.payload_2;
    packet.payload[4] = config.mapping.resolve(intent);
    packet.num_dests = config.destination_count;
    for (uint8_t i = 0; i < packet.num_dests; i++)
      packet.dest_addrs[i] = config.destinations[i];
    return packet;
  }

  DeliveryOutcome outcome_locked_(DeliveryEvent event, const CommandIntent &intent,
                                  bool partial = false, uint32_t transmitted_at_ms = 0,
                                  bool first_transmission = false,
                                  bool fallback_member = false,
                                  uint8_t fallback_member_index = 0) const {
    DeliveryOutcome outcome{};
    outcome.event = event;
    outcome.intent = intent;
    outcome.had_partial_delivery = partial;
    outcome.accepted_repeats = this->accepted_repeats_;
    outcome.queue_size = this->active_lane_ == nullptr ? 0 : this->active_lane_->queue_.size;
    outcome.transmitted_at_ms = transmitted_at_ms;
    outcome.first_transmission = first_transmission;
    outcome.fallback_member = fallback_member;
    outcome.fallback_member_index = fallback_member_index;
    return outcome;
  }

  DeliveryProfileKey key_{};
  mutable std::mutex mutex_;
  std::vector<CommandIntentDelivery *> lanes_;
  CommandIntentDelivery *active_lane_{nullptr};
  size_t active_index_{0};
  uint64_t next_sequence_{1};
  uint8_t counter_{1};
  uint8_t accepted_repeats_{0};
  uint8_t failure_count_{0};
  uint32_t last_attempt_ms_{0};
  bool attempt_started_{false};
  uint32_t pending_transaction_id_{0};
  uint8_t pending_required_repeats_{1};
  bool stop_after_pending_{false};
};

// ---------------------------------------------------------------------------
// Per-device lane implementation (requires the complete coordinator type).
// ---------------------------------------------------------------------------

inline bool CommandIntentDelivery::is_target_(CommandIntentKind kind) {
  return kind == CommandIntentKind::OPEN || kind == CommandIntentKind::CLOSE ||
         kind == CommandIntentKind::ON || kind == CommandIntentKind::OFF ||
         kind == CommandIntentKind::DIM_UP || kind == CommandIntentKind::DIM_DOWN;
}

inline bool CommandIntentDelivery::conflicts_(CommandIntentKind current, CommandIntentKind next) {
  return ((current == CommandIntentKind::OPEN || current == CommandIntentKind::CLOSE) &&
          (next == CommandIntentKind::OPEN || next == CommandIntentKind::CLOSE)) ||
         ((current == CommandIntentKind::ON || current == CommandIntentKind::OFF) &&
          (next == CommandIntentKind::ON || next == CommandIntentKind::OFF)) ||
         ((current == CommandIntentKind::DIM_UP || current == CommandIntentKind::DIM_DOWN) &&
          (next == CommandIntentKind::DIM_UP || next == CommandIntentKind::DIM_DOWN));
}

inline bool CommandIntentDelivery::same_target_family_(CommandIntentKind current,
                                                        CommandIntentKind next) {
  return current == next || conflicts_(current, next);
}

inline IntentSubmitResult CommandIntentDelivery::submit_to_state_(QueueState &state,
                                                                   const CommandIntent &intent,
                                                                   bool deferred,
                                                                   uint64_t sequence,
                                                                   uint32_t submitted_at_ms,
                                                                   uint64_t protected_sequence) const {
  if (intent.kind == CommandIntentKind::STOP) {
    if (state.size == 1 && state.entries[0].intent == intent)
      return IntentSubmitResult::COALESCED;
    state.size = 1;
    state.entries[0] = {intent, sequence, submitted_at_ms, false, false, 0};
    return IntentSubmitResult::ACCEPTED;
  }

  if (intent.kind == CommandIntentKind::CHECK) {
    for (size_t i = 0; i < state.size; i++) {
      if (state.entries[i].intent.kind == CommandIntentKind::CHECK)
        return IntentSubmitResult::COALESCED;
    }
  } else if (is_target_(intent.kind)) {
    for (size_t i = state.size; i > 0; i--) {
      const size_t index = i - 1;
      const auto queued_kind = state.entries[index].intent.kind;
      if (queued_kind == CommandIntentKind::CHECK)
        continue;
      if (!same_target_family_(queued_kind, intent.kind))
        break;
      if (state.entries[index].sequence == protected_sequence)
        break;
      if (state.entries[index].intent == intent)
        return IntentSubmitResult::COALESCED;
      state.entries[index].intent = intent;
      state.entries[index].last_progress_ms = submitted_at_ms;
      state.entries[index].deferred = deferred;
      return IntentSubmitResult::COALESCED;
    }
  } else if (intent.kind != CommandIntentKind::CUSTOM) {
    for (size_t i = state.size; i > 0; i--) {
      const size_t index = i - 1;
      if (state.entries[index].intent.kind == CommandIntentKind::CHECK)
        continue;
      if (state.entries[index].intent == intent &&
          state.entries[index].sequence != protected_sequence)
        return IntentSubmitResult::COALESCED;
      break;
    }
  }

  if (state.size >= ELERO_MAX_COMMAND_QUEUE)
    return IntentSubmitResult::REJECTED;
  state.entries[state.size++] = {intent, sequence, submitted_at_ms, deferred, false, 0};
  return IntentSubmitResult::ACCEPTED;
}

inline void CommandIntentDelivery::erase_at_(size_t index) {
  if (index >= this->queue_.size)
    return;
  for (size_t i = index + 1; i < this->queue_.size; i++)
    this->queue_.entries[i - 1] = this->queue_.entries[i];
  this->queue_.size--;
}

inline void CommandIntentDelivery::normalise_destinations_() {
  if (this->config_.destination_count == 0 || this->config_.destination_count > ELERO_MAX_DESTS)
    this->config_.destination_count = 1;
  if (this->config_.destinations[0] == 0)
    this->config_.destinations[0] = this->config_.profile.blind_address;
}

inline void CommandIntentDelivery::configure(const CommandDeliveryConfig &config) {
  std::lock_guard<std::mutex> lock(this->unbound_mutex_);
  if (this->coordinator_ != nullptr)
    return;
  this->config_ = config;
  this->normalise_destinations_();
}

inline void CommandIntentDelivery::set_outcome_callback(OutcomeCallback callback) {
  if (this->coordinator_ == nullptr) {
    std::lock_guard<std::mutex> lock(this->unbound_mutex_);
    this->outcome_callback_ = std::move(callback);
    return;
  }
  std::lock_guard<std::mutex> lock(this->coordinator_->mutex_);
  this->outcome_callback_ = std::move(callback);
}

inline IntentSubmitResult CommandIntentDelivery::submit(const CommandIntent &intent,
                                                         uint32_t submitted_at_ms,
                                                         bool deferred) {
  return this->submit_batch(&intent, 1, submitted_at_ms, deferred);
}

inline IntentSubmitResult CommandIntentDelivery::submit_batch(const CommandIntent *intents,
                                                               size_t count,
                                                               uint32_t submitted_at_ms,
                                                               bool deferred) {
  if (intents == nullptr || count == 0 || count > ELERO_MAX_COMMAND_QUEUE ||
      this->coordinator_ == nullptr)
    return IntentSubmitResult::REJECTED;
  auto *coordinator = this->coordinator_;
  std::lock_guard<std::mutex> lock(coordinator->mutex_);
  QueueState candidate = this->queue_;
  uint64_t next_sequence = coordinator->next_sequence_;
  const uint64_t protected_sequence =
      coordinator->active_lane_ == this
          ? this->queue_.entries[coordinator->active_index_].sequence
          : 0;
  IntentSubmitResult combined = IntentSubmitResult::COALESCED;
  bool accepted_stop = false;
  for (size_t i = 0; i < count; i++) {
    const auto result = this->submit_to_state_(candidate, intents[i], deferred, next_sequence,
                                               submitted_at_ms, protected_sequence);
    if (result == IntentSubmitResult::REJECTED)
      return result;
    if (result == IntentSubmitResult::ACCEPTED) {
      next_sequence++;
      combined = result;
      accepted_stop = accepted_stop || intents[i].kind == CommandIntentKind::STOP;
    }
  }
  if (accepted_stop && coordinator->active_lane_ == this &&
      coordinator->pending_transaction_id_ != 0) {
    // The active RF transaction can no longer be cancelled. Preserve it ahead
    // of the replacing STOP so its completion retires the correct counter;
    // discard only other unsent work from this lane.
    const QueuedIntent active = this->queue_.entries[coordinator->active_index_];
    const QueuedIntent stop = candidate.entries[0];
    candidate.size = 2;
    candidate.entries[0] = active;
    candidate.entries[1] = stop;
    coordinator->active_index_ = 0;
  }
  if (accepted_stop)
    coordinator->preempt_for_stop_locked_(this);
  this->queue_ = candidate;
  coordinator->next_sequence_ = next_sequence;
  return combined;
}

inline IntentSubmitResult CommandIntentDelivery::submit_atomic(const AtomicIntentTarget *targets,
                                                                size_t count,
                                                                uint32_t submitted_at_ms) {
  if (targets == nullptr || count == 0 || count > ELERO_MAX_DESTS)
    return IntentSubmitResult::REJECTED;
  std::array<ProfileDeliveryCoordinator *, ELERO_MAX_DESTS> coordinators{};
  size_t coordinator_count = 0;
  for (size_t i = 0; i < count; i++) {
    if (targets[i].delivery == nullptr || targets[i].delivery->coordinator_ == nullptr)
      return IntentSubmitResult::REJECTED;
    for (size_t j = 0; j < i; j++) {
      if (targets[j].delivery == targets[i].delivery)
        return IntentSubmitResult::REJECTED;
    }
    auto *coordinator = targets[i].delivery->coordinator_;
    bool seen = false;
    for (size_t j = 0; j < coordinator_count; j++)
      seen = seen || coordinators[j] == coordinator;
    if (!seen)
      coordinators[coordinator_count++] = coordinator;
  }
  std::sort(coordinators.begin(), coordinators.begin() + coordinator_count);
  std::array<std::unique_lock<std::mutex>, ELERO_MAX_DESTS> locks;
  for (size_t i = 0; i < coordinator_count; i++)
    locks[i] = std::unique_lock<std::mutex>(coordinators[i]->mutex_);

  std::array<QueueState, ELERO_MAX_DESTS> candidates{};
  std::array<IntentSubmitResult, ELERO_MAX_DESTS> results{};
  std::array<uint64_t, ELERO_MAX_DESTS> assigned_sequences{};
  std::array<uint64_t, ELERO_MAX_DESTS> candidate_next_sequences{};
  for (size_t i = 0; i < coordinator_count; i++)
    candidate_next_sequences[i] = coordinators[i]->next_sequence_;
  for (size_t i = 0; i < count; i++) {
    auto *lane = targets[i].delivery;
    auto *coordinator = lane->coordinator_;
    size_t coordinator_index = 0;
    while (coordinators[coordinator_index] != coordinator)
      coordinator_index++;
    candidates[i] = lane->queue_;
    assigned_sequences[i] = candidate_next_sequences[coordinator_index];
    const uint64_t protected_sequence =
        coordinator->active_lane_ == lane
            ? lane->queue_.entries[coordinator->active_index_].sequence
            : 0;
    results[i] = lane->submit_to_state_(candidates[i], targets[i].intent, targets[i].deferred,
                                        assigned_sequences[i], submitted_at_ms,
                                        protected_sequence);
    if (results[i] == IntentSubmitResult::REJECTED)
      return IntentSubmitResult::REJECTED;
    if (results[i] == IntentSubmitResult::ACCEPTED)
      candidate_next_sequences[coordinator_index]++;
  }

  for (size_t i = 0; i < coordinator_count; i++)
    coordinators[i]->next_sequence_ = candidate_next_sequences[i];

  bool any_accepted = false;
  for (size_t i = 0; i < count; i++) {
    auto *lane = targets[i].delivery;
    auto *coordinator = lane->coordinator_;
    if (results[i] == IntentSubmitResult::ACCEPTED &&
        targets[i].intent.kind == CommandIntentKind::STOP) {
      if (coordinator->active_lane_ == lane && coordinator->pending_transaction_id_ != 0) {
        const QueuedIntent active = lane->queue_.entries[coordinator->active_index_];
        const QueuedIntent stop = candidates[i].entries[0];
        candidates[i].size = 2;
        candidates[i].entries[0] = active;
        candidates[i].entries[1] = stop;
        coordinator->active_index_ = 0;
      }
      coordinator->preempt_for_stop_locked_(lane);
    }
    lane->queue_ = candidates[i];
    any_accepted = any_accepted || results[i] == IntentSubmitResult::ACCEPTED;
  }
  return any_accepted ? IntentSubmitResult::ACCEPTED : IntentSubmitResult::COALESCED;
}

inline void CommandIntentDelivery::release_deferred() {
  if (this->coordinator_ == nullptr)
    return;
  std::lock_guard<std::mutex> lock(this->coordinator_->mutex_);
  for (size_t i = 0; i < this->queue_.size; i++)
    this->queue_.entries[i].deferred = false;
}

inline void CommandIntentDelivery::postpone_until(uint32_t not_before_ms) {
  if (this->coordinator_ == nullptr)
    return;
  std::lock_guard<std::mutex> lock(this->coordinator_->mutex_);
  this->not_before_ms_ = not_before_ms;
  if (this->coordinator_->active_lane_ == this &&
      this->coordinator_->accepted_repeats_ == 0 &&
      this->coordinator_->pending_transaction_id_ == 0)
    this->coordinator_->reset_active_();
}

inline void CommandIntentDelivery::discard_pending() {
  if (this->coordinator_ == nullptr)
    return;
  std::lock_guard<std::mutex> lock(this->coordinator_->mutex_);
  if (this->coordinator_->active_lane_ == this) {
    if (this->coordinator_->accepted_repeats_ > 0 ||
        this->coordinator_->pending_transaction_id_ != 0)
      this->coordinator_->advance_counter_();
    this->coordinator_->reset_active_();
  }
  this->queue_.size = 0;
}

inline void CommandIntentDelivery::discard_checks() {
  if (this->coordinator_ == nullptr)
    return;
  std::lock_guard<std::mutex> lock(this->coordinator_->mutex_);
  for (size_t i = this->queue_.size; i > 0; i--) {
    const size_t index = i - 1;
    if (this->queue_.entries[index].intent.kind == CommandIntentKind::CHECK) {
      if (this->coordinator_->active_lane_ == this && this->coordinator_->active_index_ == index) {
        if (this->coordinator_->accepted_repeats_ > 0 ||
            this->coordinator_->pending_transaction_id_ != 0)
          this->coordinator_->advance_counter_();
        this->coordinator_->reset_active_();
      }
      this->erase_at_(index);
    }
  }
}

inline size_t CommandIntentDelivery::size() const {
  if (this->coordinator_ == nullptr) {
    std::lock_guard<std::mutex> lock(this->unbound_mutex_);
    return this->queue_.size;
  }
  std::lock_guard<std::mutex> lock(this->coordinator_->mutex_);
  return this->queue_.size;
}

inline CommandDeliveryConfig CommandIntentDelivery::config() const {
  if (this->coordinator_ == nullptr) {
    std::lock_guard<std::mutex> lock(this->unbound_mutex_);
    return this->config_;
  }
  std::lock_guard<std::mutex> lock(this->coordinator_->mutex_);
  return this->config_;
}

inline bool CommandIntentDelivery::set_native_fallback(CommandIntentDelivery *const *members,
                                                        size_t count) {
  if (members == nullptr || count == 0 || count > ELERO_MAX_DESTS)
    return false;
  auto *coordinator = this->coordinator_;
  if (coordinator == nullptr)
    return false;
  std::lock_guard<std::mutex> lock(coordinator->mutex_);
  for (size_t i = 0; i < count; i++) {
    if (members[i] == nullptr || members[i]->coordinator_ != coordinator)
      return false;
  }
  this->fallback_member_count_ = static_cast<uint8_t>(count);
  for (size_t i = 0; i < count; i++)
    this->fallback_configs_[i] = members[i]->config_;
  return true;
}

}  // namespace elero
}  // namespace esphome
