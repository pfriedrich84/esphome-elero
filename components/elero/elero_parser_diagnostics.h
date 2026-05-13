#pragma once

#include <cstdint>
#include <cstring>

namespace esphome {
namespace elero {
namespace parser_diagnostics {

enum class DropBucket : uint8_t {
  CRC_FAIL,
  STALE_COUNTER,
  TOO_MANY_DESTS,
  BOUNDS,
  OTHER,
};

struct DropCounters {
  uint32_t crc_fail{0};
  uint32_t stale_counter{0};
  uint32_t too_many_dests{0};
  uint32_t bounds{0};
  uint32_t other{0};
};

inline DropBucket bucket_for_reject_reason(const char *reason) {
  if (reason == nullptr)
    return DropBucket::OTHER;
  if (strcmp(reason, "bad_crc") == 0)
    return DropBucket::CRC_FAIL;
  if (strcmp(reason, "stale_counter") == 0)
    return DropBucket::STALE_COUNTER;
  if (strcmp(reason, "too_many_dests") == 0)
    return DropBucket::TOO_MANY_DESTS;
  if (strcmp(reason, "too_long") == 0 || strcmp(reason, "too_short") == 0 ||
      strcmp(reason, "zero_dests") == 0 || strcmp(reason, "dests_len_too_long") == 0 ||
      strcmp(reason, "rssi_oob") == 0)
    return DropBucket::BOUNDS;
  return DropBucket::OTHER;
}

inline void increment(DropCounters &counters, DropBucket bucket) {
  switch (bucket) {
    case DropBucket::CRC_FAIL:
      counters.crc_fail++;
      break;
    case DropBucket::STALE_COUNTER:
      counters.stale_counter++;
      break;
    case DropBucket::TOO_MANY_DESTS:
      counters.too_many_dests++;
      break;
    case DropBucket::BOUNDS:
      counters.bounds++;
      break;
    case DropBucket::OTHER:
    default:
      counters.other++;
      break;
  }
}

}  // namespace parser_diagnostics
}  // namespace elero
}  // namespace esphome
