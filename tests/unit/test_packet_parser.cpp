#include "elero/elero_packet_parser.h"
#include "elero/elero_crypto.h"
#include <gtest/gtest.h>
#include <cstring>

using namespace esphome::elero::crypto;
using namespace esphome::elero::packet_parser;

namespace {

void write_u24(uint8_t *buf, uint8_t offset, uint32_t value) {
  buf[offset] = (value >> 16) & 0xff;
  buf[offset + 1] = (value >> 8) & 0xff;
  buf[offset + 2] = value & 0xff;
}

void write_encoded_payload(uint8_t *buf, uint8_t offset, uint8_t state, uint8_t cmd = 0x00) {
  uint8_t payload[8] = {};
  payload[4] = cmd;
  payload[6] = state;
  msg_encode(payload);
  memcpy(&buf[offset], payload, sizeof(payload));
}

}  // namespace

TEST(PacketParser, ParsesStatusPacketWithoutHubState) {
  uint8_t fifo[64] = {};
  fifo[0] = 29;       // length
  fifo[1] = 7;        // counter
  fifo[2] = 0xca;     // status packet
  fifo[3] = 0x00;
  fifo[4] = 0x0a;     // hop
  fifo[5] = 0x01;     // system
  fifo[6] = 4;        // channel
  write_u24(fifo, 7, 0xa831e5);   // src/blind
  write_u24(fifo, 10, 0xf0d008);  // bwd
  write_u24(fifo, 13, 0xf0d008);  // fwd/remote for status
  fifo[16] = 1;       // destination count
  write_u24(fifo, 17, 0xf0d008);
  fifo[20] = 0x00;    // payload_1
  fifo[21] = 0x04;    // payload_2
  write_encoded_payload(fifo, 22, 0x01);
  fifo[30] = 100;     // RSSI raw
  fifo[31] = 0x80 | 42;  // CRC ok + LQI

  auto result = parse_fifo_packet(fifo);

  ASSERT_TRUE(result.ok);
  const auto &packet = result.packet;
  EXPECT_TRUE(packet.is_status);
  EXPECT_FALSE(packet.is_command);
  EXPECT_EQ(packet.src, 0xa831e5u);
  EXPECT_EQ(packet.fwd, 0xf0d008u);
  EXPECT_EQ(packet.channel, 4);
  EXPECT_EQ(packet.payload_1, 0x00);
  EXPECT_EQ(packet.payload_2, 0x04);
  EXPECT_EQ(packet.payload[6], 0x01);
  EXPECT_EQ(packet.lqi, 42);
}

TEST(PacketParser, ParsesCommandPacketDestinations) {
  uint8_t fifo[64] = {};
  fifo[0] = 32;
  fifo[1] = 8;
  fifo[2] = 0x6a;
  fifo[3] = 0x00;
  fifo[4] = 0x0a;
  fifo[5] = 0x01;
  fifo[6] = 4;
  write_u24(fifo, 7, 0xf0d008);   // src/remote
  write_u24(fifo, 10, 0xf0d008);
  write_u24(fifo, 13, 0xf0d008);
  fifo[16] = 2;
  write_u24(fifo, 17, 0xa831e5);
  write_u24(fifo, 20, 0xb74211);
  fifo[23] = 0x00;
  fifo[24] = 0x04;
  write_encoded_payload(fifo, 25, 0x00, 0x20);
  fifo[33] = 100;
  fifo[34] = 0x80 | 11;

  auto result = parse_fifo_packet(fifo);

  ASSERT_TRUE(result.ok);
  const auto &packet = result.packet;
  EXPECT_FALSE(packet.is_status);
  EXPECT_TRUE(packet.is_command);
  EXPECT_EQ(packet.first_dst, 0xa831e5u);
  EXPECT_EQ(packet.num_dests, 2);
  EXPECT_EQ(packet.dest_addrs[0], 0xa831e5u);
  EXPECT_EQ(packet.dest_addrs[1], 0xb74211u);
  EXPECT_EQ(packet.payload[4], 0x20);
  EXPECT_EQ(packet.lqi, 11);
}

TEST(PacketParser, RejectsInvalidPacketsWithStableReasons) {
  uint8_t fifo[64] = {};

  fifo[0] = 58;
  EXPECT_STREQ(parse_fifo_packet(fifo).reject_reason, "too_long");

  fifo[0] = 16;
  EXPECT_STREQ(parse_fifo_packet(fifo).reject_reason, "too_short");

  fifo[0] = 29;
  fifo[16] = 0;
  EXPECT_STREQ(parse_fifo_packet(fifo).reject_reason, "zero_dests");

  fifo[16] = 11;
  EXPECT_STREQ(parse_fifo_packet(fifo).reject_reason, "too_many_dests");

  fifo[16] = 1;
  fifo[2] = 0x6a;
  fifo[31] = 0x00;
  EXPECT_STREQ(parse_fifo_packet(fifo).reject_reason, "bad_crc");
}
