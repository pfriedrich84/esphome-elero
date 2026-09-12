#include "elero/elero_rx_fifo.h"
#include "elero/elero_radio_timing.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <deque>
#include <string>
#include <vector>
using namespace esphome::elero;

struct FifoIO {
  uint32_t time{100};
  bool active{false}, idle{false}, overflow{false}, crc_autoflush{true};
  std::deque<uint8_t> fifo;
  std::vector<std::vector<uint8_t>> received;
  std::vector<RxMetadata> metadata;
  std::vector<std::string> trace;
  unsigned flushes{0};
  uint32_t now() const { return time; }
  bool packet_active() const { return active; }
  bool enter_idle() { idle = true; active = false; trace.push_back("SIDLE"); return true; }
  void resume_rx() { idle = false; trace.push_back("SRX"); }
  bool rx_bytes(uint8_t &n) { n = fifo.size() | (overflow ? 0x80 : 0); return true; }
  void read_fifo(uint8_t *bytes, uint8_t n) {
    ASSERT_TRUE(idle) << "CRC_AUTOFLUSH may rewrite FIFO pointers during RX";
    ASSERT_GE(fifo.size(), n);
    trace.push_back("READ:" + std::to_string(n));
    while (n--) { *bytes++ = fifo.front(); fifo.pop_front(); }
  }
  void discard(const char *) { EXPECT_TRUE(idle); fifo.clear(); overflow = false; ++flushes; }
  void packet(const uint8_t *bytes, uint8_t n, const RxMetadata &meta) {
    received.emplace_back(bytes, bytes + n); metadata.push_back(meta);
  }
  void append(const std::vector<uint8_t> &p) { fifo.insert(fifo.end(), p.begin(), p.end()); }
  void crc_failure() { ASSERT_FALSE(idle); if (crc_autoflush) fifo.clear(); active = false; }
};
static std::vector<uint8_t> packet(uint8_t counter) {
  std::vector<uint8_t> bytes(32, 0); bytes[0] = 29; bytes[1] = counter; bytes[31] = 0x80;
  return bytes;
}

TEST(RxFifo, TwoCompletePacketsAreReadSeparatelyWithBothStatusBytes) {
  FifoIO io; RxFifoReader reader; RxTimeline timeline;
  io.append(packet(1)); io.append(packet(2));
  reader.drain(io, timeline);
  ASSERT_EQ(io.received.size(), 2u);
  EXPECT_EQ(io.received[0], packet(1)); EXPECT_EQ(io.received[1], packet(2));
  EXPECT_LT(io.metadata[0].rx_sequence, io.metadata[1].rx_sequence);
  EXPECT_EQ(io.trace, (std::vector<std::string>{"SIDLE", "READ:1", "READ:31", "READ:1", "READ:31", "SRX"}));
  EXPECT_EQ(io.flushes, 0u);
}

TEST(RxFifo, CompletePacketAndFollowingPrefixRemainUntouchedUntilCrcCompletion) {
  FifoIO io; RxFifoReader reader; RxTimeline timeline;
  auto next = packet(2);
  io.append(packet(1)); io.append({next.begin(), next.begin() + 5}); io.active = true;
  EXPECT_EQ(reader.drain(io, timeline), RxFifoReader::Result::RECEIVING);
  EXPECT_TRUE(io.trace.empty()); EXPECT_EQ(io.fifo.size(), 37u);
  const auto cutoff = timeline.fence(101);
  io.append({next.begin() + 5, next.end()}); io.active = false; io.time = 105;
  reader.drain(io, timeline);
  ASSERT_EQ(io.received.size(), 2u); EXPECT_EQ(io.received[1], next);
  auto meta = io.metadata[1]; meta.source = 0x111111;
  EXPECT_FALSE(meta.after(cutoff, meta.source)) << "parsing an old prefix later must not make it fresh";
}

TEST(RxFifo, CrcAutoflushBetweenSlicesNeverSplicesAnOldLengthToANewPacket) {
  FifoIO io; RxFifoReader reader; RxTimeline timeline;
  auto bad = packet(1); io.append({bad.begin(), bad.begin() + 5}); io.active = true;
  reader.drain(io, timeline);
  EXPECT_TRUE(io.trace.empty());
  io.crc_failure(); io.append(packet(2)); io.time += 2;
  reader.drain(io, timeline);
  ASSERT_EQ(io.received.size(), 1u); EXPECT_EQ(io.received[0], packet(2));
}

TEST(RxFifo, StuckPrefixRecoveryPreservesCompletePredecessorAndIsBounded) {
  FifoIO io; RxFifoReader reader; RxTimeline timeline;
  io.append(packet(1)); io.append({29, 2, 0, 0}); io.active = true;
  reader.drain(io, timeline); io.time += 20;
  EXPECT_EQ(reader.drain(io, timeline), RxFifoReader::Result::RECOVERED);
  ASSERT_EQ(io.received.size(), 1u); EXPECT_EQ(io.received.front(), packet(1));
  EXPECT_EQ(io.flushes, 1u);
}

TEST(RxFifo, OverflowAndMalformedLengthRecoverWithoutGuessingPacketBoundaries) {
  for (bool overflow : {false, true}) {
    FifoIO io; RxFifoReader reader; RxTimeline timeline;
    io.append({255, 1, 2, 3}); io.overflow = overflow;
    EXPECT_EQ(reader.drain(io, timeline), RxFifoReader::Result::RECOVERED);
    EXPECT_TRUE(io.received.empty()); EXPECT_EQ(io.flushes, 1u); EXPECT_FALSE(io.idle);
  }
}

TEST(RadioTiming, BusyCcaBackoffIsBoundedAndMillisWrapSafe) {
  CcaBackoff cca; const uint32_t start = UINT32_MAX - 10;
  cca.start(start, 42);
  EXPECT_FALSE(cca.due(start)); EXPECT_TRUE(cca.due(start + 1));
  unsigned attempts = 0;
  for (uint32_t elapsed = 1; elapsed < 100 && !cca.expired(start + elapsed); elapsed++) {
    if (cca.due(start + elapsed)) { cca.busy(start + elapsed); ++attempts; }
  }
  EXPECT_LE(attempts, 5u); EXPECT_TRUE(cca.expired(start + 50));
}

TEST(RadioTiming, SendDelaySurvivesIntentAndProfileChangesIncludingMillisZeroAndWrap) {
  CompletionSpacing spacing;
  EXPECT_TRUE(spacing.ready(0, 1000));
  spacing.completed(0);
  EXPECT_FALSE(spacing.ready(999, 1000)); EXPECT_TRUE(spacing.ready(1000, 1000));
  spacing.completed(UINT32_MAX - 20);
  EXPECT_FALSE(spacing.ready(5, 1000)); EXPECT_TRUE(spacing.ready(1000, 1000));
}
