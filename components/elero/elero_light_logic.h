#pragma once
// Elero light brightness logic — extracted as pure functions for testability.

#include <algorithm>
#include <cstdint>

namespace esphome {
namespace elero {
namespace light_logic {

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
