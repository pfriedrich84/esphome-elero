#pragma once
// Elero command profile — RF identity and command-shaping data for one blind.

#include <cstdint>

namespace esphome {
namespace elero {

struct BlindCommandProfile {
  uint32_t blind_address{0};
  uint32_t remote_address{0};
  uint8_t channel{0};
  uint8_t pck_inf[2]{};
  uint8_t hop{0};
  uint8_t payload_1{0};
  uint8_t payload_2{0};
};

namespace command_profile {

/// Native multi-destination packets share one remote/channel/protocol profile.
/// Destination addresses may differ; everything else must be compatible because
/// the RF packet has only one header/payload profile for all destinations.
inline bool can_share_native_group(const BlindCommandProfile &a, const BlindCommandProfile &b) {
  return a.remote_address == b.remote_address &&
         a.channel == b.channel &&
         a.pck_inf[0] == b.pck_inf[0] &&
         a.pck_inf[1] == b.pck_inf[1] &&
         a.hop == b.hop &&
         a.payload_1 == b.payload_1 &&
         a.payload_2 == b.payload_2;
}

}  // namespace command_profile
}  // namespace elero
}  // namespace esphome
