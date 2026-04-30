#pragma once
// Elero packet parser — turns raw CC1101 FIFO bytes into protocol fields.
// This module is deliberately pure: no logging, counters, queues, packet-dump
// mutation, deduplication, or ESPHome state. The hub owns those side effects.

#include "elero_crypto.h"
#include "elero_packet_validation.h"
#include "elero_utils.h"
#include <cstdint>
#include <cstring>

namespace esphome {
namespace elero {
namespace packet_parser {

static constexpr uint8_t MAX_DESTS = 10;
static constexpr uint8_t PAYLOAD_SIZE = 10;

struct ParsedPacket {
  uint8_t length{0};
  uint8_t cnt{0};
  uint8_t typ{0};
  uint8_t typ2{0};
  uint8_t hop{0};
  uint8_t syst{0};
  uint8_t channel{0};
  uint32_t src{0};
  uint32_t bwd{0};
  uint32_t fwd{0};
  uint8_t num_dests{0};
  uint8_t dests_len{0};
  uint32_t first_dst{0};
  uint32_t dest_addrs[MAX_DESTS]{};
  uint8_t payload_1{0};
  uint8_t payload_2{0};
  uint8_t payload[PAYLOAD_SIZE]{};
  uint8_t crc{0};
  uint8_t lqi{0};
  float rssi{0.0f};
  bool is_status{false};
  bool is_command{false};
};

struct ParseResult {
  bool ok{false};
  const char *reject_reason{nullptr};
  ParsedPacket packet{};
};

inline uint32_t read_u24(const uint8_t *buf, uint8_t offset) {
  return (static_cast<uint32_t>(buf[offset]) << 16) |
         (static_cast<uint32_t>(buf[offset + 1]) << 8) |
         static_cast<uint32_t>(buf[offset + 2]);
}

inline ParseResult parse_fifo_packet(const uint8_t *fifo) {
  ParseResult result{};
  ParsedPacket &packet = result.packet;
  packet.length = fifo[0];

  if (packet.length > packet_validation::MAX_PACKET_SIZE) {
    result.reject_reason = "too_long";
    return result;
  }
  if (packet.length < packet_validation::MIN_PACKET_SIZE) {
    result.reject_reason = "too_short";
    return result;
  }

  packet.cnt = fifo[1];
  packet.typ = fifo[2];
  packet.typ2 = fifo[3];
  packet.hop = fifo[4];
  packet.syst = fifo[5];
  packet.channel = fifo[6];
  packet.src = read_u24(fifo, 7);
  packet.bwd = read_u24(fifo, 10);
  packet.fwd = read_u24(fifo, 13);
  packet.num_dests = fifo[16];

  if (packet.num_dests == 0) {
    result.reject_reason = "zero_dests";
    return result;
  }
  if (packet.num_dests > packet_validation::max_safe_dests()) {
    result.reject_reason = "too_many_dests";
    return result;
  }

  packet.dests_len = packet_validation::calculate_dests_length(packet.typ, packet.num_dests);
  if (packet.typ > 0x60) {
    packet.first_dst = read_u24(fifo, 17);
  } else {
    packet.first_dst = fifo[17];
  }

  if (!packet_validation::is_valid_packet_bounds(packet.length, packet.dests_len)) {
    result.reject_reason = "dests_len_too_long";
    return result;
  }
  if (!packet_validation::is_rssi_in_bounds(packet.length)) {
    result.reject_reason = "rssi_oob";
    return result;
  }

  packet.payload_1 = fifo[17 + packet.dests_len];
  packet.payload_2 = fifo[18 + packet.dests_len];
  uint8_t status_byte = fifo[packet.length + 2];
  packet.crc = packet_validation::extract_crc(status_byte);
  packet.lqi = packet_validation::extract_lqi(status_byte);
  if (!packet_validation::is_crc_valid_status_byte(status_byte)) {
    result.reject_reason = "bad_crc";
    return result;
  }

  packet.rssi = utils::calculate_rssi(fifo[packet.length + 1]);
  memcpy(packet.payload, &fifo[19 + packet.dests_len], PAYLOAD_SIZE);
  crypto::msg_decode(packet.payload);

  packet.is_status = (packet.typ == 0xca) || (packet.typ == 0xc9);
  packet.is_command = (packet.typ == 0x6a) || (packet.typ == 0x69);

  if (packet.is_command) {
    uint8_t safe_num = (packet.num_dests > MAX_DESTS) ? MAX_DESTS : packet.num_dests;
    for (uint8_t i = 0; i < safe_num; i++) {
      if (packet.typ > 0x60) {
        packet.dest_addrs[i] = read_u24(fifo, 17 + i * 3);
      } else {
        packet.dest_addrs[i] = fifo[17 + i];
      }
    }
  }

  result.ok = true;
  result.reject_reason = nullptr;
  return result;
}

}  // namespace packet_parser
}  // namespace elero
}  // namespace esphome
