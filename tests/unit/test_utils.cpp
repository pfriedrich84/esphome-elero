#include "elero/elero_utils.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace esphome::elero::utils;

// --- registers_to_mhz ---

TEST(RegistersToMhz, Default868) {
  // (26.0/65536.0) * 0x21717a = 869.525 MHz (the CC1101 register value for "868.35 MHz" setting)
  float mhz = registers_to_mhz(0x21, 0x71, 0x7a);
  EXPECT_NEAR(mhz, 869.525, 0.01);
}

TEST(RegistersToMhz, Alternative868) {
  // (26.0/65536.0) * 0x2171c0 = 869.553 MHz
  float mhz = registers_to_mhz(0x21, 0x71, 0xc0);
  EXPECT_NEAR(mhz, 869.553, 0.01);
}

TEST(RegistersToMhz, AllZeros) {
  EXPECT_FLOAT_EQ(registers_to_mhz(0x00, 0x00, 0x00), 0.0f);
}

TEST(RegistersToMhz, MaxRegisters) {
  float mhz = registers_to_mhz(0xFF, 0xFF, 0xFF);
  // (26.0 / 65536.0) * 0xFFFFFF = (26.0 / 65536.0) * 16777215 ≈ 6656.0
  EXPECT_NEAR(mhz, 6656.0, 0.01);
}

TEST(RegistersToMhz, Freq433) {
  // 433.92 MHz: freq2=0x10, freq1=0xB2, freq0=0x14
  float mhz = registers_to_mhz(0x10, 0xB2, 0x14);
  EXPECT_NEAR(mhz, 433.92, 0.02);
}

// --- calculate_rssi ---

TEST(CalculateRssi, Zero) {
  EXPECT_FLOAT_EQ(calculate_rssi(0), -74.0f);
}

TEST(CalculateRssi, Boundary127) {
  // 127 = last value on the unsigned path
  EXPECT_FLOAT_EQ(calculate_rssi(127), 127.0f / 2.0f + (-74.0f));  // -10.5
}

TEST(CalculateRssi, Boundary128) {
  // 128 = first value on the signed path: (int8_t)(128) = -128
  EXPECT_FLOAT_EQ(calculate_rssi(128), -128.0f / 2.0f + (-74.0f));  // -138.0
}

TEST(CalculateRssi, Max255) {
  // (int8_t)(255) = -1 → -1/2 + (-74) = -74.5
  EXPECT_FLOAT_EQ(calculate_rssi(255), -1.0f / 2.0f + (-74.0f));  // -74.5
}

TEST(CalculateRssi, Typical200) {
  // (int8_t)(200) = -56 → -56/2 + (-74) = -102.0
  EXPECT_FLOAT_EQ(calculate_rssi(200), -56.0f / 2.0f + (-74.0f));  // -102.0
}

TEST(CalculateRssi, GoodSignal50) {
  // 50 (unsigned) → 50/2 + (-74) = -49.0
  EXPECT_FLOAT_EQ(calculate_rssi(50), 50.0f / 2.0f + (-74.0f));  // -49.0
}
