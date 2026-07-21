#pragma once

// Dependency-light radio-wide TX admission gate.
//
// Delivery coordinators own semantic ordering per RF profile, while this gate
// ensures that only one command packet can be queued or in flight at the
// shared CC1101 radio. Urgent work is selected before the next reservation;
// it never overtakes an older packet already admitted to a physical queue.

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace elero {

inline bool delivery_profile_is_eligible(bool urgent_waiting, bool profile_has_urgent) {
  return !urgent_waiting || profile_has_urgent;
}

inline size_t delivery_profile_round_robin_index(size_t start, size_t offset, size_t count) {
  return count == 0 ? 0 : (start + offset) % count;
}

class RadioTxAdmission {
 public:
  bool try_reserve(uint32_t transaction_id) {
    if (transaction_id == 0)
      return false;
    uint32_t expected = 0;
    return this->transaction_id_.compare_exchange_strong(
        expected, transaction_id, std::memory_order_acq_rel, std::memory_order_acquire);
  }

  bool release(uint32_t transaction_id) {
    if (transaction_id == 0)
      return false;
    uint32_t expected = transaction_id;
    return this->transaction_id_.compare_exchange_strong(
        expected, 0, std::memory_order_acq_rel, std::memory_order_acquire);
  }

  bool busy() const { return this->transaction_id_.load(std::memory_order_acquire) != 0; }
  uint32_t transaction_id() const {
    return this->transaction_id_.load(std::memory_order_acquire);
  }

 private:
  std::atomic<uint32_t> transaction_id_{0};
};

}  // namespace elero
}  // namespace esphome
