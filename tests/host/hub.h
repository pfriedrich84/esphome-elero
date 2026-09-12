#pragma once
#include "framework.h"
#include "interfaces.h"
#include "elero/elero_tx_admission.h"
#include <map>
#include <memory>
#include <utility>

namespace esphome { namespace elero {
inline const char *elero_state_to_string(uint8_t) { return "test raw state"; }

// Only framework/SPI/queue boundaries are fake. Actual command lanes and
// coordinators are used, including asynchronous admission and completion.
class Elero {
 public:
  bool register_command_delivery(CommandIntentDelivery *lane) {
    auto key = DeliveryProfileKey::from(lane->config().profile);
    auto &co = profiles[key];
    if (!co) co = std::make_unique<ProfileDeliveryCoordinator>(key);
    return co->attach(lane);
  }
  void unregister_command_delivery(CommandIntentDelivery *lane) {
    for (auto &item : profiles) item.second->detach(lane);
  }
  void register_cover(EleroBlindBase *) {}
  uint32_t get_tx_queue_depth() const { return admission.busy() ? 1 : 0; }
  void increment_tx_drop_count() { ++drops; }
  void decrement_stop_urgent() {}
  void publish_text_sensor_state(uint32_t, const std::string &s) { status = s; }
  uint32_t advance(uint32_t now, uint32_t delay = 0) {
    test_now = now;
    if (admission.busy()) return 0;
    bool urgent = false;
    for (auto &item : profiles) urgent = urgent || item.second->has_urgent(now);
    for (auto &item : profiles) {
      if (urgent && !item.second->has_urgent(now)) continue;
      uint32_t id = 0;
      item.second->advance(now, delay, 1, 2, [&](const t_elero_command &p, bool priority) {
        id = next_id++;
        admission.try_reserve(id);
        packets.push_back(p);
        priorities.push_back(priority);
        return PacketSubmission::queued(id);
      });
      if (id != 0) return id;
    }
    return 0;
  }
  DeliveryOutcome complete(uint32_t id, bool success, uint32_t now, RxCutoff cutoff = {}) {
    test_now = now;
    DeliveryOutcome result{};
    for (auto &item : profiles) {
      auto outcome = item.second->complete(id, success, now, cutoff);
      if (outcome.event != DeliveryEvent::IDLE) result = outcome;
    }
    admission.release(id);
    return result;
  }
  unsigned drops{0};
  std::string status;
  std::vector<t_elero_command> packets;
  std::vector<bool> priorities;
  std::map<DeliveryProfileKey, std::unique_ptr<ProfileDeliveryCoordinator>> profiles;
  RadioTxAdmission admission;
  uint32_t next_id{1};
};
}}
