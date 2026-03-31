#include "elero/elero_crypto.h"
#include <gtest/gtest.h>
#include <cstring>

using namespace esphome::elero::crypto;

// --- count_bits: returns popcount(byte) & 1 (parity bit) ---

TEST(CountBits, Zero) { EXPECT_EQ(count_bits(0x00), 0); }
TEST(CountBits, One) { EXPECT_EQ(count_bits(0x01), 1); }
TEST(CountBits, Two) { EXPECT_EQ(count_bits(0x03), 0); }
TEST(CountBits, Three) { EXPECT_EQ(count_bits(0x07), 1); }
TEST(CountBits, AllOnes) { EXPECT_EQ(count_bits(0xFF), 0); }  // 8 bits = even
TEST(CountBits, HighBit) { EXPECT_EQ(count_bits(0x80), 1); }

TEST(CountBits, All256Values) {
  for (int i = 0; i < 256; i++) {
    uint8_t byte = static_cast<uint8_t>(i);
    int expected = __builtin_popcount(byte) & 1;
    EXPECT_EQ(count_bits(byte), expected) << "Failed for byte=" << i;
  }
}

// --- Lookup table consistency: decode is inverse of encode ---

TEST(LookupTables, EncodeDecodeInverse) {
  for (uint8_t i = 0; i < 16; i++) {
    EXPECT_EQ(flash_table_decode[flash_table_encode[i]], i)
        << "encode→decode failed for nibble " << (int)i;
  }
}

TEST(LookupTables, DecodeEncodeInverse) {
  for (uint8_t i = 0; i < 16; i++) {
    EXPECT_EQ(flash_table_encode[flash_table_decode[i]], i)
        << "decode→encode failed for nibble " << (int)i;
  }
}

// --- add/sub r20 nibbles round-trip ---

TEST(NibbleOps, AddSubRoundTrip) {
  uint8_t original[8] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
  uint8_t msg[8];
  std::memcpy(msg, original, 8);

  add_r20_to_nibbles(msg, 0xFE, 0, 8);
  sub_r20_from_nibbles(msg, 0xFE, 0, 8);

  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(msg[i], original[i]) << "Mismatch at index " << i;
  }
}

TEST(NibbleOps, AddSubRoundTripBA) {
  uint8_t original[8] = {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89};
  uint8_t msg[8];
  std::memcpy(msg, original, 8);

  add_r20_to_nibbles(msg, 0xBA, 2, 8);
  sub_r20_from_nibbles(msg, 0xBA, 2, 8);

  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(msg[i], original[i]) << "Mismatch at index " << i;
  }
}

TEST(NibbleOps, ZeroLengthNoop) {
  uint8_t msg[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  uint8_t original[8];
  std::memcpy(original, msg, 8);

  add_r20_to_nibbles(msg, 0xFE, 0, 0);  // start==length → no-op
  for (int i = 0; i < 8; i++) EXPECT_EQ(msg[i], original[i]);
}

// --- encode/decode nibbles round-trip ---

TEST(EncodeDecodeNibbles, RoundTrip) {
  uint8_t original[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
  uint8_t msg[8];
  std::memcpy(msg, original, 8);

  encode_nibbles(msg);
  decode_nibbles(msg, 8);

  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(msg[i], original[i]) << "Mismatch at index " << i;
  }
}

TEST(EncodeDecodeNibbles, AllZeros) {
  uint8_t msg[8] = {};
  encode_nibbles(msg);
  // 0x00 encodes as flash_table_encode[0]=0x08, flash_table_encode[0]=0x08 → 0x88
  for (int i = 0; i < 8; i++) EXPECT_EQ(msg[i], 0x88);
}

// --- xor encode/decode ---

TEST(XorOps, EncodeWithZeroIsPartialIdentity) {
  uint8_t msg[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  uint8_t original[8];
  std::memcpy(original, msg, 8);

  xor_2byte_in_array_encode(msg, 0x00, 0x00);
  // Encode XORs indices 2-7, so indices 0-1 unchanged
  EXPECT_EQ(msg[0], original[0]);
  EXPECT_EQ(msg[1], original[1]);
  for (int i = 2; i < 8; i++) EXPECT_EQ(msg[i], original[i]);
}

TEST(XorOps, DecodeWithZeroIsIdentity) {
  uint8_t msg[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  uint8_t original[8];
  std::memcpy(original, msg, 8);

  xor_2byte_in_array_decode(msg, 0x00, 0x00);
  for (int i = 0; i < 8; i++) EXPECT_EQ(msg[i], original[i]);
}

TEST(XorOps, DoubleDecodeIsIdentity) {
  uint8_t msg[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
  uint8_t original[8];
  std::memcpy(original, msg, 8);

  xor_2byte_in_array_decode(msg, 0x42, 0x37);
  xor_2byte_in_array_decode(msg, 0x42, 0x37);
  for (int i = 0; i < 8; i++) EXPECT_EQ(msg[i], original[i]);
}

// --- Full msg_encode → msg_decode round-trip ---

TEST(MsgCodec, EncodeThenDecodeRecoversBytesZeroToSix) {
  // msg_encode overwrites msg[7] with parity, so we only check bytes 0-6
  uint8_t original[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00};
  uint8_t msg[8];
  std::memcpy(msg, original, 8);

  msg_encode(msg);
  msg_decode(msg);

  for (int i = 0; i < 7; i++) {
    EXPECT_EQ(msg[i], original[i]) << "Mismatch at byte " << i;
  }
}

TEST(MsgCodec, RoundTripAllZeros) {
  uint8_t msg[8] = {};
  uint8_t original[8] = {};
  msg_encode(msg);
  msg_decode(msg);
  for (int i = 0; i < 7; i++) EXPECT_EQ(msg[i], original[i]);
}

TEST(MsgCodec, RoundTripVariousPayloads) {
  uint8_t payloads[][8] = {
      {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0x00},
      {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x00},
      {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x00},
  };
  for (auto &payload : payloads) {
    uint8_t original[8];
    std::memcpy(original, payload, 8);
    msg_encode(payload);
    msg_decode(payload);
    for (int i = 0; i < 7; i++) {
      EXPECT_EQ(payload[i], original[i]) << "Payload round-trip failed at byte " << i;
    }
  }
}

// --- calc_parity ---

TEST(Parity, AllZerosProducesZeroParity) {
  uint8_t msg[8] = {};
  calc_parity(msg);
  EXPECT_EQ(msg[7], 0x00);  // All even pairs → all zero bits → 0 shifted
}

TEST(Parity, KnownInput) {
  uint8_t msg[8] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  calc_parity(msg);
  // msg[0]=0x01 has 1 bit (odd), msg[1]=0x00 has 0 bits (even) → a^b = 1^0 = 1
  // Remaining pairs are all 0^0 = 0
  // p shifts: 1,0,0,0 → binary 1000 → p=8 after loop → msg[7] = 8<<3 = 64
  EXPECT_EQ(msg[7], 64);
}
