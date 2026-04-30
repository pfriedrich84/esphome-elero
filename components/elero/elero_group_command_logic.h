#pragma once
// Group cover command dispatch policy — pure decisions for testability.

#include <cstdint>

namespace esphome {
namespace elero {
namespace group_command_logic {

/// Abstracted outcome of submitting a command to the Elero hub.
/// Kept independent from SendResult so this policy stays unit-testable without
/// pulling in ESPHome/FreeRTOS headers.
enum class HubSubmitResult : uint8_t {
  OK,
  QUEUE_FULL,
  FAILED,
};

/// A native group packet consumes the group counter only when it was actually
/// accepted by the hub TX queue.
inline bool should_increment_group_counter(HubSubmitResult result) {
  return result == HubSubmitResult::OK;
}

/// If a direct/native group send was not accepted by the hub, retry through
/// member command queues so normal per-cover dispatch aging/retry rules apply.
inline bool should_fallback_to_member_queues(HubSubmitResult result) {
  return result != HubSubmitResult::OK;
}

inline uint8_t next_group_counter(uint8_t current) {
  return current == 0xFF ? 1 : static_cast<uint8_t>(current + 1);
}

}  // namespace group_command_logic
}  // namespace elero
}  // namespace esphome
