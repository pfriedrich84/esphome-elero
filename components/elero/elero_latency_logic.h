#pragma once

#include <cstdint>

namespace esphome {
namespace elero {
namespace latency_logic {

struct LastMax {
  uint32_t last{0};
  uint32_t max{0};
};

inline LastMax observe(uint32_t current_max, uint32_t sample) {
  return LastMax{sample, sample > current_max ? sample : current_max};
}

}  // namespace latency_logic
}  // namespace elero
}  // namespace esphome
