#pragma once
#include <cstdint>
namespace esphome { namespace elero {

// CC1101 erratum: dynamic status registers can be corrupted by asynchronous
// updates. Require two consecutive identical full-byte reads, at most five
// SPI transactions. Retain every observed FIFO error bit / TX-underflow state;
// instability may delay/abort a transaction, never turn it into success.
template<class Read>
bool read_stable_status(Read read, uint8_t &value, uint8_t fault_mask = 0,
                        uint8_t fault_state = 0xff) {
  uint8_t previous = read();
  uint8_t faults = previous & fault_mask;
  if (previous == fault_state) { value = previous; return false; }
  for (uint8_t attempt = 1; attempt < 5; ++attempt) {
    const uint8_t current = read();
    faults |= current & fault_mask;
    if (current == fault_state) { value = current; return false; }
    if (current == previous) { value = current | faults; return true; }
    previous = current;
  }
  value = previous | faults;
  return false;
}
}}
