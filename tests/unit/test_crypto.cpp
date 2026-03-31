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
// Note: The Elero protocol uses msg[0:1] as XOR key during decode AFTER partial
// decoding (sub_r20). This means bytes 0-1 are part of the key derivation and
// the round-trip only recovers bytes 2-6 reliably. Byte 7 is parity (overwritten by encode).

TEST(MsgCodec, EncodeThenDecodeRecoversBytesTwoToSix) {
  uint8_t original[8] = {0x00, 0x00, 0x03, 0x04, 0x20, 0x00, 0x00, 0x00};
  uint8_t msg[8];
  std::memcpy(msg, original, 8);

  msg_encode(msg);
  // Encoded data should differ from original
  bool any_differ = false;
  for (int i = 0; i < 8; i++) {
    if (msg[i] != original[i]) any_differ = true;
  }
  EXPECT_TRUE(any_differ) << "Encoding should change the data";

  msg_decode(msg);
  // Bytes 2-6 should round-trip (bytes 0-1 are XOR key, byte 7 is parity)
  for (int i = 2; i < 7; i++) {
    EXPECT_EQ(msg[i], original[i]) << "Mismatch at byte " << i;
  }
}

TEST(MsgCodec, RoundTripAllZeros) {
  uint8_t msg[8] = {};
  msg_encode(msg);
  msg_decode(msg);
  // All-zeros is a special case: XOR with 0 is identity, so bytes 0-1 also recover
  for (int i = 0; i < 7; i++) EXPECT_EQ(msg[i], 0);
}

TEST(MsgCodec, EncodeProducesDifferentOutput) {
  uint8_t msg[8] = {0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00};
  uint8_t encoded[8];
  std::memcpy(encoded, msg, 8);
  msg_encode(encoded);
  // At least some bytes should change after encoding
  bool changed = false;
  for (int i = 0; i < 8; i++) {
    if (encoded[i] != msg[i]) changed = true;
  }
  EXPECT_TRUE(changed);
}

// --- calc_parity ---

TEST(Parity, AllZerosProducesZeroParity) {
  uint8_t msg[8] = {};
  calc_parity(msg);
  EXPECT_EQ(msg[7], 0x00);  // All even pairs → all zero bits → 0 shifted
}

TEST(Parity, KnownInput) {
  // Trace calc_parity for msg = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}:
  // i=0: a=count_bits(0x01)=1, b=count_bits(0x00)=0, p = 0|1^0 = 1, p<<=1 → p=2
  // i=1: a=count_bits(0x00)=0, b=count_bits(0x00)=0, p = 2|0^0 = 2, p<<=1 → p=4
  // i=2: a=count_bits(0x00)=0, b=count_bits(0x00)=0, p = 4|0^0 = 4, p<<=1 → p=8
  // i=3: a=count_bits(0x00)=0, b=count_bits(0x00)=0, p = 8|0^0 = 8, p<<=1 → p=16
  // msg[7] = 16 << 3 = 128
  uint8_t msg[8] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  calc_parity(msg);
  EXPECT_EQ(msg[7], 128);
}
