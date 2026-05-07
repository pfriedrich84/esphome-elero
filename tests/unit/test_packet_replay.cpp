#include "elero/elero_packet_parser.h"
#include "elero/elero_parser_diagnostics.h"
#include "elero/elero_dedup_logic.h"
#include "elero/elero_counter_logic.h"
#include <gtest/gtest.h>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>

using namespace esphome::elero;
using namespace esphome::elero::packet_parser;

#ifndef RF_REPLAY_FIXTURE_DIR
#define RF_REPLAY_FIXTURE_DIR "../fixtures/rf_replay"
#endif

namespace {

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> out;
  std::string item;
  std::stringstream ss(s);
  while (std::getline(ss, item, delim)) out.push_back(item);
  return out;
}

std::vector<uint8_t> parse_hex_bytes(const std::string &hex) {
  std::vector<uint8_t> out;
  std::stringstream ss(hex);
  std::string byte;
  while (ss >> byte) out.push_back(static_cast<uint8_t>(std::strtoul(byte.c_str(), nullptr, 16)));
  return out;
}

std::map<std::string, std::string> parse_expectations(const std::string &expect) {
  std::map<std::string, std::string> out;
  std::stringstream ss(expect);
  std::string token;
  while (ss >> token) {
    auto pos = token.find('=');
    if (pos != std::string::npos) out[token.substr(0, pos)] = token.substr(pos + 1);
  }
  return out;
}

uint32_t parse_u32(const std::string &value) {
  return static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 0));
}

}  // namespace

TEST(PacketReplay, ReplaysFixtureLinesThroughParser) {
  std::ifstream file(std::string(RF_REPLAY_FIXTURE_DIR) + "/basic.replay");
  ASSERT_TRUE(file.good());

  std::string line;
  uint32_t cases = 0;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    auto parts = split(line, '|');
    ASSERT_EQ(parts.size(), 3u) << line;
    const std::string &name = parts[0];
    auto bytes = parse_hex_bytes(parts[1]);
    auto expect = parse_expectations(parts[2]);
    ASSERT_LE(bytes.size(), 64u) << name;

    uint8_t fifo[64] = {};
    memcpy(fifo, bytes.data(), bytes.size());
    auto result = parse_fifo_packet(fifo);

    bool ok = expect["ok"] == "1";
    EXPECT_EQ(result.ok, ok) << name;
    if (!ok) {
      EXPECT_STREQ(result.reject_reason, expect["reason"].c_str()) << name;
      continue;
    }

    const auto &packet = result.packet;
    EXPECT_EQ(packet.typ, parse_u32(expect["typ"])) << name;
    EXPECT_EQ(packet.src, parse_u32(expect["src"])) << name;
    EXPECT_EQ(packet.first_dst, parse_u32(expect["dst"])) << name;
    EXPECT_EQ(packet.payload[6], parse_u32(expect["state"])) << name;
    EXPECT_EQ(packet.cnt, parse_u32(expect["cnt"])) << name;
    if (expect.count("cmd")) EXPECT_EQ(packet.payload[4], parse_u32(expect["cmd"])) << name;
    if (expect.count("num_dests")) EXPECT_EQ(packet.num_dests, parse_u32(expect["num_dests"])) << name;
    if (expect.count("dest1")) EXPECT_EQ(packet.dest_addrs[1], parse_u32(expect["dest1"])) << name;
    cases++;
  }
  EXPECT_GE(cases, 2u);
}

TEST(PacketReplay, DedupWindowSuppressesRelayDuplicate) {
  EXPECT_TRUE(dedup_logic::is_duplicate_within_window(1200, 1000, 500));
  EXPECT_FALSE(dedup_logic::is_duplicate_within_window(1500, 1000, 500));
  EXPECT_FALSE(dedup_logic::is_duplicate_within_window(1501, 1000, 500));
  EXPECT_FALSE(dedup_logic::should_prune_entry(1499, 1000, 500));
  EXPECT_TRUE(dedup_logic::should_prune_entry(1500, 1000, 500));
}

TEST(PacketReplay, CounterLogicRejectsStaleValuesAndAllowsWrapForward) {
  EXPECT_FALSE(counter_logic::is_stale_counter(7, 8));
  EXPECT_TRUE(counter_logic::is_stale_counter(8, 8));
  EXPECT_TRUE(counter_logic::is_stale_counter(8, 7));
  EXPECT_FALSE(counter_logic::is_stale_counter(250, 2));
  EXPECT_TRUE(counter_logic::is_stale_counter(2, 250));
}

TEST(PacketReplay, DropReasonsMapToStableDiagnosticBuckets) {
  using parser_diagnostics::DropBucket;
  EXPECT_EQ(parser_diagnostics::bucket_for_reject_reason("bad_crc"), DropBucket::CRC_FAIL);
  EXPECT_EQ(parser_diagnostics::bucket_for_reject_reason("stale_counter"), DropBucket::STALE_COUNTER);
  EXPECT_EQ(parser_diagnostics::bucket_for_reject_reason("too_many_dests"), DropBucket::TOO_MANY_DESTS);
  EXPECT_EQ(parser_diagnostics::bucket_for_reject_reason("too_short"), DropBucket::BOUNDS);
  EXPECT_EQ(parser_diagnostics::bucket_for_reject_reason("rssi_oob"), DropBucket::BOUNDS);
  EXPECT_EQ(parser_diagnostics::bucket_for_reject_reason("unknown"), DropBucket::OTHER);

  parser_diagnostics::DropCounters counters{};
  parser_diagnostics::increment(counters, DropBucket::CRC_FAIL);
  parser_diagnostics::increment(counters, DropBucket::BOUNDS);
  parser_diagnostics::increment(counters, DropBucket::BOUNDS);
  EXPECT_EQ(counters.crc_fail, 1u);
  EXPECT_EQ(counters.bounds, 2u);
}
