#pragma once
#include <algorithm>
#include <cstdint>
namespace esphome { namespace elero {

// Radio attempt budget, distinct from the coordinator's bounded delivery
// retries. Backoff remains in RX and never flushes a busy channel's RX FIFO.
class CcaBackoff {
 public:
  void start(uint32_t now, uint32_t seed) {
    started_ = scheduled_ = now; wait_ = 1; attempts_ = 0; seed_ = seed;
  }
  bool expired(uint32_t now) const { return now - started_ >= 50 || attempts_ >= 5; }
  bool due(uint32_t now) const { return now - scheduled_ >= wait_; }
  void listen(uint32_t now) { scheduled_ = now; wait_ = 1; }
  void busy(uint32_t now) {
    ++attempts_;
    scheduled_ = now;
    seed_ = seed_ * 1664525u + 1013904223u;
    wait_ = (1u << std::min<uint8_t>(attempts_, 4)) + ((seed_ >> 16) & 3u);
  }
 private:
  uint32_t started_{0}, scheduled_{0}, wait_{1}, seed_{0};
  uint8_t attempts_{0};
};

// Survives intent changes and, at the hub, profile rotation. Unsigned elapsed
// arithmetic intentionally supports millis() wrap and a completion at time 0.
class CompletionSpacing {
 public:
  void completed(uint32_t now) { at_ = now; valid_ = true; }
  bool ready(uint32_t now, uint32_t delay_ms) const { return !valid_ || now - at_ >= delay_ms; }
 private:
  uint32_t at_{0};
  bool valid_{false};
};
}}
