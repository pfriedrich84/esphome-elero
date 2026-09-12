#pragma once

#include <cstdint>

namespace esphome {
namespace elero {

// Owned by the radio task. Sequence numbers are assigned when a packet is
// captured, never at entity dispatch. Epoch fences also exclude a packet whose
// prefix was buffered at a local TX completion but parsed only afterwards.
struct RxCutoff {
  uint64_t sequence{0};
  uint64_t epoch{0};
  uint32_t completed_at_ms{0};
  bool valid{false};
};

struct RxMetadata {
  uint64_t rx_sequence{0};
  uint64_t epoch{0};
  uint32_t received_at_ms{0};
  uint32_t source{0};
  uint32_t backward{0};
  uint32_t forward{0};
  uint8_t channel{0};
  uint8_t counter{0};
  uint8_t type{0};
  uint8_t type2{0};
  uint8_t hop{0};
  uint8_t system{0};
  uint8_t destination_count{0};
  uint32_t destinations[10]{};

  bool after(const RxCutoff &cutoff, uint32_t motor) const {
    return cutoff.valid && this->source == motor && this->rx_sequence > cutoff.sequence &&
           this->epoch >= cutoff.epoch &&
           static_cast<int32_t>(this->received_at_ms - cutoff.completed_at_ms) >= 0;
  }
};

class RxTimeline {
 public:
  RxMetadata capture(uint32_t received_at_ms) {
    RxMetadata meta{};
    meta.rx_sequence = ++this->sequence_;
    meta.epoch = this->epoch_;
    meta.received_at_ms = received_at_ms;
    return meta;
  }
  // Call only after capturing complete buffered RX packets and anchoring any
  // incomplete prefix. Never relabel pre-fence buffered data with the new epoch.
  RxCutoff fence(uint32_t completed_at_ms) {
    return {this->sequence_, ++this->epoch_, completed_at_ms, true};
  }
  uint64_t epoch() const { return this->epoch_; }

 private:
  uint64_t sequence_{0};
  uint64_t epoch_{0};
};

// These are observed motor states, not protocol ACK bytes or echoed TX counters.
enum class StopFeedback : uint8_t { INAPPLICABLE, STOPPED, MOVING, BLOCKING, OVERHEATED };

inline StopFeedback classify_stop_feedback(uint8_t state) {
  switch (state) {
    case 0x01: case 0x02: case 0x03: case 0x04: case 0x0d: case 0x0e: case 0x0f:
      return StopFeedback::STOPPED;
    case 0x08: case 0x09: case 0x0a: case 0x0b:
      return StopFeedback::MOVING;
    case 0x05: return StopFeedback::BLOCKING;
    case 0x06: return StopFeedback::OVERHEATED;
    default: return StopFeedback::INAPPLICABLE;  // includes UNKNOWN and TIMEOUT
  }
}

inline StopFeedback fresh_stop_feedback(uint8_t state, const RxMetadata &meta,
                                        const RxCutoff &cutoff, uint32_t motor) {
  return meta.after(cutoff, motor) ? classify_stop_feedback(state) : StopFeedback::INAPPLICABLE;
}

}  // namespace elero
}  // namespace esphome
