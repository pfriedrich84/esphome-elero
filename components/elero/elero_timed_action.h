#pragma once

// Dependency-light policy for starting dead-reckoning clocks. Optimistic entity
// state may be published at semantic enqueue time, but elapsed RF action time
// starts only after the hub accepts the first relevant packet.

#include "elero_command_delivery.h"

namespace esphome {
namespace elero {

inline bool should_start_timed_action(bool pending, CommandIntentKind expected,
                                      const DeliveryOutcome &outcome) {
  return pending && outcome.intent.kind == expected &&
         delivery_packet_was_accepted(outcome.event);
}

inline bool should_reanchor_transmitted_action(CommandIntentKind current,
                                                const DeliveryOutcome &outcome) {
  return outcome.first_transmission && outcome.intent.kind != current &&
         delivery_packet_was_accepted(outcome.event);
}

}  // namespace elero
}  // namespace esphome
