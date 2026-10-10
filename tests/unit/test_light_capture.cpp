#include "elero/elero_packet_parser.h"
#include "elero/elero_counter_logic.h"
#include <gtest/gtest.h>
#include <array>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
using namespace esphome::elero;

TEST(LightCapture, EveryDecodedFramePreservesCapturedFieldsAndAddressDomains) {
  std::ifstream file(LIGHT_CAPTURE_PATH);
  ASSERT_TRUE(file.good());
  std::string line;
  unsigned frames = 0, short_frames = 0, direct_frames = 0, receiver_frames = 0;
  std::set<unsigned> states, hops, receiver_channels;
  std::map<std::pair<unsigned, unsigned>, unsigned> repeated;
  unsigned duplicates = 0;
  while (std::getline(file, line)) {
    if (line.find("rcv'd: len=") == std::string::npos) continue;
    std::map<std::string, unsigned> fields;
    const std::regex field("(len|cnt|typ|typ2|hop|syst|chl|src|bwd|fwd|dst)=([^, ]+)");
    for (auto it = std::sregex_iterator(line.begin(), line.end(), field); it != std::sregex_iterator(); ++it) {
      auto text = (*it)[2].str();
      fields[(*it)[1].str()] = std::stoul(text, nullptr, text.find("0x") == 0 ? 16 : 10);
    }
    std::smatch match;
    ASSERT_TRUE(std::regex_search(line, match, std::regex("payload=\\[(.*?)\\]")));
    std::istringstream payload_text(match[1]);
    std::array<uint8_t, 10> observed{};
    for (auto &byte : observed) { std::string word; ASSERT_TRUE(bool(payload_text >> word)); byte = std::stoul(word, nullptr, 16); }
    uint8_t fifo[64]{};
    fifo[0] = fields["len"]; fifo[1] = fields["cnt"]; fifo[2] = fields["typ"];
    fifo[3] = fields["typ2"]; fifo[4] = fields["hop"]; fifo[5] = fields["syst"]; fifo[6] = fields["chl"];
    auto u24 = [&](int offset, unsigned value) { fifo[offset] = value >> 16; fifo[offset+1] = value >> 8; fifo[offset+2] = value; };
    u24(7, fields["src"]); u24(10, fields["bwd"]); u24(13, fields["fwd"]); fifo[16] = 1;
    int payload_offset;
    if (fields["typ"] > 0x60) { u24(17, fields["dst"]); payload_offset = 20; }
    else { fifo[17] = fields["dst"]; payload_offset = 18; }
    std::copy(observed.begin(), observed.end(), fifo + payload_offset);
    // The log contains decoded RX, NOT raw RX bytes. Reconstruct a synthetic
    // FIFO with the inverse transforms, preserving the captured parity byte.
    // This exercises parsing, but is deliberately not a golden-wire assertion.
    auto *encoded = fifo + payload_offset + 2;
    const auto xor0 = encoded[0], xor1 = encoded[1];
    crypto::add_r20_to_nibbles(encoded, 0xfe, 0, 8);
    crypto::xor_2byte_in_array_encode(encoded, xor0, xor1);
    crypto::encode_nibbles(encoded);
    fifo[fifo[0] + 1] = 100; fifo[fifo[0] + 2] = 0x80 | 48;
    auto result = packet_parser::parse_fifo_packet(fifo);
    ASSERT_TRUE(result.ok) << line;
    const auto &p = result.packet;
    EXPECT_EQ(p.src, fields["src"]); EXPECT_EQ(p.first_dst, fields["dst"]);
    EXPECT_EQ(p.channel, fields["chl"]); EXPECT_EQ(p.cnt, fields["cnt"]); EXPECT_EQ(p.hop, fields["hop"]);
    EXPECT_EQ(p.payload_1, observed[0]); EXPECT_EQ(p.payload_2, observed[1]);
    for (unsigned i = 0; i < 8; ++i) EXPECT_EQ(p.payload[i], observed[i+2]) << line;
    EXPECT_EQ(p.is_status, p.typ == 0xca);
    if (p.typ == 0x44) {
      ++short_frames; EXPECT_TRUE(p.is_channel_command); EXPECT_FALSE(p.is_command);
      EXPECT_EQ(p.first_dst, 3u); EXPECT_EQ(p.channel, 3);
      // A channel selector must never become device 0x000003 in cover dispatch.
      EXPECT_EQ(p.dest_addrs[0], 0u);
    } else if (p.typ == 0x6a) {
      ++direct_frames; EXPECT_TRUE(p.is_command); EXPECT_FALSE(p.is_channel_command);
      EXPECT_EQ(p.dest_addrs[0], 0xe99b2bu); EXPECT_EQ(p.channel, 3);
    }
    if (p.src == 0xe99b2b) { ++receiver_frames; EXPECT_TRUE(p.is_status); states.insert(p.payload[6]); receiver_channels.insert(p.channel); }
    if (++repeated[{p.src, p.cnt}] > 1) ++duplicates;
    hops.insert(p.hop); ++frames;
  }
  EXPECT_EQ(frames, 186u); EXPECT_EQ(short_frames, 151u); EXPECT_EQ(direct_frames, 21u);
  EXPECT_EQ(receiver_frames, 11u);
  EXPECT_EQ(states, (std::set<unsigned>{0x03, 0x10}));
  EXPECT_GT(duplicates, 0u); EXPECT_GT(hops.size(), 1u); EXPECT_GT(receiver_channels.size(), 1u);
}
