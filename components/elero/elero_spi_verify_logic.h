#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace elero {
namespace spi_verify_logic {

// Complementary write/readback patterns: together they toggle every bit once.
static constexpr uint8_t TEST_PATTERNS[] = {0xAA, 0x55};
static constexpr size_t TEST_PATTERN_COUNT = sizeof(TEST_PATTERNS) / sizeof(TEST_PATTERNS[0]);

// The SPI write/readback self-test must use a register whose eight bits are all
// read/write. FSCTRL1 is NOT suitable: bits 7:6 are unused and always read 0, so
// 0xAA/0x55 read back as 0x2A/0x15 on healthy hardware (false "stuck MOSI" alarm,
// issue #250). ADDR (0x09, DEVICE_ADDR[7:0]) is fully R/W, reset value 0x00, and
// init() writes it again afterwards.
static constexpr uint8_t FULLY_WRITABLE_MASK = 0xFF;
static constexpr uint8_t FSCTRL1_WRITABLE_MASK = 0x3F;

// Bits that differ between what was written and what was read back, limited to
// the bits the register can actually store. Zero means the readback is healthy.
inline uint8_t mismatch_bits(uint8_t written, uint8_t readback, uint8_t writable_mask = FULLY_WRITABLE_MASK) {
  return static_cast<uint8_t>((written ^ readback) & writable_mask);
}

}  // namespace spi_verify_logic
}  // namespace elero
}  // namespace esphome
