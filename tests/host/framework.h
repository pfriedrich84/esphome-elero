#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Logging is not a substitute for a delivery-state assertion in host tests.
#define ESP_LOGV(...) ((void) 0)
#define ESP_LOGVV(...) ((void) 0)
#define ESP_LOGD(...) ((void) 0)
#define ESP_LOGI(...) ((void) 0)
#define ESP_LOGW(...) ((void) 0)
#define ESP_LOGE(...) ((void) 0)
#define ESP_LOGCONFIG(...) ((void) 0)
#define LOG_COVER(...) ((void) 0)
#define YESNO(x) ((x) ? "yes" : "no")

namespace esphome {
inline uint32_t test_now = 1000;
inline uint32_t millis() { return test_now; }
template<class T> T clamp(T value, T low, T high) { return std::clamp(value, low, high); }
namespace setup_priority { inline constexpr float DATA = 600; }
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  virtual float get_setup_priority() const { return 0; }
  void mark_failed() { failed_ = true; }
  bool is_failed() const { return failed_; }
 private:
  bool failed_{false};
};
namespace cover {
enum CoverOperation { COVER_OPERATION_IDLE, COVER_OPERATION_OPENING, COVER_OPERATION_CLOSING };
inline constexpr float COVER_OPEN = 1.0f, COVER_CLOSED = 0.0f;
class CoverTraits {
 public:
  void set_supports_stop(bool) {}
  void set_supports_position(bool) {}
  void set_supports_toggle(bool) {}
  void set_is_assumed_state(bool) {}
  void set_supports_tilt(bool) {}
};
struct CoverCall {
  bool stop{false};
  std::optional<float> position, tilt;
  std::optional<bool> toggle;
  bool get_stop() const { return stop; }
  auto get_position() const { return position; }
  auto get_tilt() const { return tilt; }
  auto get_toggle() const { return toggle; }
};
class Cover {
 public:
  virtual ~Cover() = default;
  virtual CoverTraits get_traits() { return {}; }
  const std::string &get_name() const { return name_; }
  float position{0.5f}, tilt{0.0f};
  CoverOperation current_operation{COVER_OPERATION_IDLE};
  unsigned publications{0};
  void publish_state(bool = true) { ++publications; }
 protected:
  virtual void control(const CoverCall &) {}
  struct Restore { void apply(Cover *) {} };
  std::optional<Restore> restore_state_() { return {}; }
 private:
  std::string name_{"test cover"};
};
}
}
