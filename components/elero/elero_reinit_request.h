#pragma once

#include <cstdint>
#include <mutex>

namespace esphome {
namespace elero {

// Component-owned completion state. FreeRTOS messages carry only a token and
// frequency bytes, never a caller's stack address or a short-lived semaphore.
class FrequencyReinitRequest {
 public:
  static constexpr uint32_t TIMEOUT_MS = 5000;
  enum class Result : uint8_t { PENDING, SUCCEEDED, FAILED, TIMED_OUT };

  uint32_t begin(uint32_t now) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (this->phase_ != Phase::IDLE) return 0;
    if (++this->next_id_ == 0) ++this->next_id_;
    this->id_ = this->next_id_;
    this->submitted_at_ms_ = now;
    this->abandoned_ = false;
    this->phase_ = Phase::QUEUED;
    return this->id_;
  }

  bool start(uint32_t id, uint32_t now) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (id == 0 || id != this->id_ || this->phase_ != Phase::QUEUED) return false;
    if (now - this->submitted_at_ms_ >= TIMEOUT_MS) {
      this->success_ = false;
      this->phase_ = Phase::COMPLETED;
      return false;
    }
    this->phase_ = Phase::RUNNING;
    return true;
  }

  void complete(uint32_t id, bool success, uint32_t now) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (id != this->id_ || this->phase_ != Phase::RUNNING) return;
    this->success_ = success && now - this->submitted_at_ms_ < TIMEOUT_MS;
    this->phase_ = this->abandoned_ ? Phase::IDLE : Phase::COMPLETED;
  }

  Result poll(uint32_t id, uint32_t now) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (id == 0 || id != this->id_ || this->phase_ == Phase::IDLE) return Result::FAILED;
    if (this->phase_ == Phase::COMPLETED) {
      this->phase_ = Phase::IDLE;
      return this->success_ ? Result::SUCCEEDED : Result::FAILED;
    }
    if (now - this->submitted_at_ms_ < TIMEOUT_MS) return Result::PENDING;
    this->abandon_();
    return Result::TIMED_OUT;
  }

  // Used when queue admission fails. Also safe if the caller abandons a
  // running request: the slot remains busy until Core 0 finishes the operation.
  void cancel(uint32_t id) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (id == this->id_ && this->phase_ != Phase::IDLE) this->abandon_();
  }

 private:
  enum class Phase : uint8_t { IDLE, QUEUED, RUNNING, COMPLETED };
  void abandon_() {
    this->abandoned_ = this->phase_ == Phase::RUNNING;
    if (!this->abandoned_) this->phase_ = Phase::IDLE;
  }
  std::mutex mutex_;
  Phase phase_{Phase::IDLE};
  uint32_t next_id_{0}, id_{0}, submitted_at_ms_{0};
  bool abandoned_{false}, success_{false};
};

}  // namespace elero
}  // namespace esphome
