#include "elero/elero_spi_verify_logic.h"
#include <gtest/gtest.h>

using namespace esphome::elero::spi_verify_logic;

TEST(SpiVerifyLogic, PatternsToggleEveryBit) {
  ASSERT_EQ(TEST_PATTERN_COUNT, 2u);
  EXPECT_EQ(static_cast<uint8_t>(TEST_PATTERNS[0] ^ TEST_PATTERNS[1]), 0xFF);
}

TEST(SpiVerifyLogic, HealthyReadbackHasNoMismatch) {
  for (uint8_t pattern : TEST_PATTERNS) {
    EXPECT_EQ(mismatch_bits(pattern, pattern), 0);
  }
}

TEST(SpiVerifyLogic, StuckBitsAreReported) {
  // MOSI bit 7 stuck low on a fully writable register
  EXPECT_EQ(mismatch_bits(0xAA, 0x2A), 0x80);
  // MOSI bit 0 stuck high
  EXPECT_EQ(mismatch_bits(0x55, 0x55 | 0x01), 0x00);
  EXPECT_EQ(mismatch_bits(0xAA, 0xAB), 0x01);
}

// Regression for #250: on healthy hardware FSCTRL1 returns 0x2A/0x15 for
// 0xAA/0x55 because bits 7:6 are unused. Those bits must not count as stuck,
// which is why the self-test now uses a fully writable register.
TEST(SpiVerifyLogic, Fsctrl1UnusedBitsAreNotAFault) {
  EXPECT_NE(mismatch_bits(0xAA, 0x2A), 0);  // what the old full-byte compare flagged
  EXPECT_NE(mismatch_bits(0x55, 0x15), 0);
  EXPECT_EQ(mismatch_bits(0xAA, 0x2A, FSCTRL1_WRITABLE_MASK), 0);
  EXPECT_EQ(mismatch_bits(0x55, 0x15, FSCTRL1_WRITABLE_MASK), 0);
}
