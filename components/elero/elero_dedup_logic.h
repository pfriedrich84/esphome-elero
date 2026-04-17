#pragma once
// Elero dedup helpers — pure window predicates for consistency and testability.

#include <cstdint>

namespace esphome {
namespace elero {
namespace dedup_logic {

inline bool is_duplicate_within_window(uint32_t now_ms, uint32_t seen_ms,
                                       uint32_t window_ms) {
  return (now_ms - seen_ms) < window_ms;
}

inline bool should_prune_entry(uint32_t now_ms, uint32_t seen_ms,
                               uint32_t window_ms) {
  return (now_ms - seen_ms) >= window_ms;
}

}  // namespace dedup_logic
}  // namespace elero
}  // namespace esphome
