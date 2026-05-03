#pragma once
// Elero light brightness logic — extracted as pure functions for testability.

#include <algorithm>
#include <cstdint>

#define ELERO_HAS_LIGHT_PUBLISH_HELPER 1
#define ELERO_HAS_LIGHT_IMMEDIATE_POLL_HELPER 1
#define ELERO_HAS_LIGHT_ACTION_QUEUE_HELPER 1
#define ELERO_HAS_LIGHT_ACTION_PRECHECK_HELPER 1

namespace esphome {
namespace elero {
namespace light_logic {

inline bool should_publish_dimming(uint32_t now, uint32_t last_publish_ms,
                                   uint32_t publish_interval_ms) {
  return (now - last_publish_ms) >= publish_interval_ms;
}

inline bool should_schedule_immediate_poll(uint8_t command_check, uint8_t queue_size,
                                           uint8_t max_queue_size, uint32_t now,
                                           uint32_t last_immediate_ms,
                                           uint32_t min_interval_ms) {
  return command_check != 0x00 && queue_size < max_queue_size &&
         (now - last_immediate_ms) >= min_interval_ms;
}

// Returns the updated brightness after dead-reckoning.
// dim_up: true = increasing brightness, false = decreasing
// dim_duration_ms: time for full 0→1 or 1→0 sweep
// elapsed_ms: time since last recompute
// timeout_ms: max plausible elapsed time (ELERO_TIMEOUT_MOVEMENT)
inline float recompute_brightness(float brightness, bool dim_up, uint32_t dim_duration_ms,
                                  uint32_t elapsed_ms, uint32_t timeout_ms) {
  if (dim_duration_ms == 0)
    return brightness;
  if (elapsed_ms > timeout_ms)
    return brightness;
  float dir = dim_up ? 1.0f : -1.0f;
  float delta = dir * static_cast<float>(elapsed_ms) / static_cast<float>(dim_duration_ms);
  return std::clamp(brightness + delta, 0.0f, 1.0f);
}

enum class LightAction : uint8_t {
  NONE,           // no command needed (within epsilon of target)
  OFF,            // send OFF command
  ON,             // send ON command (on/off only or full-brightness shortcut)
  ON_THEN_DIM_DOWN,  // send ON first (from off state), then start dimming down
  DIM_UP,         // start dimming up
  DIM_DOWN,       // start dimming down
};

inline uint8_t required_command_slots(LightAction action) {
  switch (action) {
    case LightAction::NONE:
      return 0;
    case LightAction::ON_THEN_DIM_DOWN:
      return 2;
    case LightAction::OFF:
    case LightAction::ON:
    case LightAction::DIM_UP:
    case LightAction::DIM_DOWN:
    default:
      return 1;
  }
}

inline bool can_enqueue_action(LightAction action, uint8_t queue_size, uint8_t max_queue_size) {
  return static_cast<uint16_t>(queue_size) + required_command_slots(action) <= max_queue_size;
}

inline bool should_reject_action_before_state_update(LightAction action, uint8_t queue_size,
                                                     uint8_t max_queue_size) {
  return !can_enqueue_action(action, queue_size, max_queue_size);
}

struct LightActionResult {
  LightAction action;
  float new_brightness;   // brightness to set after action
  bool new_is_dimming;    // whether dimming is active
  bool new_dim_up;        // direction if dimming
};

// Pure decision tree for write_state: determines which RF commands to send.
// new_on: target on/off state
// new_brightness: target brightness (0.0-1.0)
// current_brightness: current tracked brightness
// dim_duration_ms: 0 = on/off only, >0 = brightness control
// epsilon: tolerance for brightness comparison
inline LightActionResult determine_action(bool new_on, float new_brightness,
                                          float current_brightness, uint32_t dim_duration_ms,
                                          float epsilon = 0.01f) {
  if (!new_on) {
    return {LightAction::OFF, 0.0f, false, false};
  }

  // On/off only mode (no brightness control)
  if (dim_duration_ms == 0) {
    return {LightAction::ON, 1.0f, false, false};
  }

  // Full brightness shortcut
  if (new_brightness >= 1.0f) {
    return {LightAction::ON, 1.0f, false, false};
  }

  // Currently off — must turn on first then dim down to target
  if (current_brightness < epsilon) {
    // After ON, brightness becomes 1.0; if target < 1.0, need to dim down
    if (new_brightness < 1.0f - epsilon) {
      return {LightAction::ON_THEN_DIM_DOWN, 1.0f, true, false};
    }
    // Target is essentially full brightness
    return {LightAction::ON, 1.0f, false, false};
  }

  // Already on — dim to target
  if (new_brightness > current_brightness + epsilon) {
    return {LightAction::DIM_UP, current_brightness, true, true};
  }
  if (new_brightness < current_brightness - epsilon) {
    return {LightAction::DIM_DOWN, current_brightness, true, false};
  }

  // Within tolerance — no action needed
  return {LightAction::NONE, current_brightness, false, false};
}

}  // namespace light_logic
}  // namespace elero
}  // namespace esphome
