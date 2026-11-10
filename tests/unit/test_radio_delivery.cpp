#include "radio_hub.h"
#include "elero/elero_packet_parser.h"
#include <gtest/gtest.h>
using namespace esphome;
using namespace esphome::elero;

class RadioDeliveryTest : public ::testing::Test {
 protected:
  Elero hub;
  t_elero_command command{};
  void SetUp() override {
    test_now = 100;
    command.pck_inf[0] = 0x6a; command.remote_addr = 0x123456; command.num_dests = 1; command.dest_addrs[0] = 0x111111;
  }
  void queue() {
    ASSERT_TRUE(hub.send_command_internal_(&command, test_now));
    hub.active_tx_transaction_id_ = 1;
  }
  std::vector<uint8_t> packet(uint8_t counter) {
    std::vector<uint8_t> p(32); p[0] = 29; p[1] = counter; p[31] = 0x80; return p;
  }
  void receive(uint8_t counter) {
    auto p = packet(counter); hub.fifo.insert(hub.fifo.end(), p.begin(), p.end()); hub.rx_ready_ = true;
  }
};

TEST_F(RadioDeliveryTest, StxIsActuallyIssuedFromRxAfterRssiListenNotFromIdle) {
  queue();
  EXPECT_EQ(hub.tx_state_, TxState::CCA);
  hub.advance_tx(); EXPECT_EQ(hub.marc, CC1101_MARCSTATE_RX);
  test_now++; hub.advance_tx();
  ASSERT_EQ(hub.marc, CC1101_MARCSTATE_TX);
  EXPECT_TRUE(hub.stx_from_rx);
  EXPECT_TRUE(hub.completions.empty());
  hub.finish_tx(); hub.advance_tx();
  ASSERT_EQ(hub.completions.size(), 1u); EXPECT_TRUE(hub.completions[0].success);
  EXPECT_FALSE(hub.illegal_flush); EXPECT_FALSE(hub.illegal_fifo_read);
}

TEST_F(RadioDeliveryTest, BusyChannelReturnsBoundedFailureWithoutRxFlushOrFictitiousSuccess) {
  hub.channel_clear = false; queue();
  for (int i = 0; i < 51; i++) { test_now++; hub.advance_tx(); }
  ASSERT_EQ(hub.completions.size(), 1u); EXPECT_FALSE(hub.completions[0].success);
  EXPECT_EQ(std::count(hub.strobes.begin(), hub.strobes.end(), CC1101_STX), 0);
  EXPECT_EQ(std::count(hub.strobes.begin(), hub.strobes.end(), CC1101_SFRX), 0);
  EXPECT_EQ(hub.marc, CC1101_MARCSTATE_RX);
}

TEST_F(RadioDeliveryTest, HardwareCcaRejectionKeepsTxBytesAndRxOwnershipDuringBackoff) {
  hub.reject_stx = true; queue();
  const auto loaded = hub.tx_fifo;
  test_now++; hub.advance_tx();
  EXPECT_EQ(hub.tx_state_, TxState::CCA);
  EXPECT_EQ(hub.radio_mode_, static_cast<uint8_t>(RadioMode::RX));
  EXPECT_EQ(hub.tx_fifo, loaded); EXPECT_TRUE(hub.completions.empty());
  receive(7);
  for (int i = 0; i < 51; i++) { test_now++; hub.advance_tx(); }
  ASSERT_EQ(hub.received.size(), 1u); EXPECT_EQ(hub.received[0], packet(7));
  ASSERT_EQ(hub.completions.size(), 1u); EXPECT_FALSE(hub.completions[0].success);
  EXPECT_TRUE(hub.stx_from_rx); EXPECT_FALSE(hub.illegal_flush);
}

TEST_F(RadioDeliveryTest, FeedbackDuringCooldownIsDrainedBeforeNextTxPreparation) {
  queue(); test_now++; hub.advance_tx(); hub.finish_tx(); hub.advance_tx();
  receive(10); receive(11);
  test_now++; hub.advance_tx();
  ASSERT_EQ(hub.received.size(), 2u);
  EXPECT_EQ(hub.received[0], packet(10)); EXPECT_EQ(hub.received[1], packet(11));
  EXPECT_TRUE(hub.fifo.empty()); EXPECT_FALSE(hub.illegal_fifo_read);
  ASSERT_TRUE(hub.send_command_internal_(&command, test_now));
  EXPECT_EQ(hub.received.size(), 2u);
}

TEST_F(RadioDeliveryTest, CompletionFenceExcludesAlreadyBufferedStatusBeforeCoreOneDispatch) {
  queue(); test_now++; hub.advance_tx();
  hub.finish_tx(); receive(4); hub.advance_tx();
  ASSERT_EQ(hub.metadata.size(), 1u); ASSERT_EQ(hub.completions.size(), 1u);
  auto old = hub.metadata[0]; old.source = command.dest_addrs[0];
  EXPECT_FALSE(old.after(hub.completions[0].rx_cutoff, old.source));
  test_now += 10; receive(5); hub.advance_tx();
  ASSERT_EQ(hub.metadata.size(), 2u);
  auto fresh = hub.metadata[1]; fresh.source = command.dest_addrs[0];
  EXPECT_TRUE(fresh.after(hub.completions[0].rx_cutoff, fresh.source));
}

TEST_F(RadioDeliveryTest, UnderflowWithFallingGdoAndEmptyLowBitsNeverSucceeds) {
  for (bool marc_fault : {false, true}) {
    hub.completions.clear(); queue(); test_now++; hub.advance_tx(); hub.finish_tx();
    hub.tx_underflow = !marc_fault;
    if (marc_fault) hub.marc = CC1101_MARCSTATE_TXFIFO_UFLOW;
    hub.advance_tx();
    ASSERT_EQ(hub.completions.size(), 1u); EXPECT_FALSE(hub.completions[0].success);
    EXPECT_EQ(hub.tx_count_, 0u); EXPECT_FALSE(hub.illegal_flush);
  }
}

TEST_F(RadioDeliveryTest, UnexpectedIdleAndEmptyFifoAreNotProofOfTransmission) {
  queue(); test_now++; hub.advance_tx(); hub.finish_tx(); hub.marc = CC1101_MARCSTATE_IDLE;
  hub.advance_tx();
  ASSERT_EQ(hub.completions.size(), 1u); EXPECT_FALSE(hub.completions[0].success);
}

TEST_F(RadioDeliveryTest, DynamicRegisterReadsConvergeAndRetainObservedErrorBits) {
  uint8_t value = 0;
  hub.script[CC1101_RXBYTES] = {31, 32, 31, 32, 32};
  EXPECT_TRUE(hub.read_status_stable(CC1101_RXBYTES, value)); EXPECT_EQ(value, 32);
  hub.script[CC1101_TXBYTES] = {0x80, 0, 0};
  EXPECT_TRUE(hub.read_status_stable(CC1101_TXBYTES, value)); EXPECT_EQ(value, 0x80);
  hub.script[CC1101_MARCSTATE] = {CC1101_MARCSTATE_TX, CC1101_MARCSTATE_TXFIFO_UFLOW};
  EXPECT_FALSE(hub.read_status_stable(CC1101_MARCSTATE, value));
  EXPECT_EQ(value, CC1101_MARCSTATE_TXFIFO_UFLOW);
}

TEST_F(RadioDeliveryTest, UnstableCompletionSnapshotIsBoundedAndNeverReportsSuccess) {
  queue(); test_now++; hub.advance_tx(); hub.finish_tx();
  for (uint32_t elapsed : {1u, 50u}) {
    hub.script[CC1101_MARCSTATE] = {0x13, 0x0d, 0x13, 0x0d, 0x13};
    test_now = hub.tx_state_entered_ms_ + elapsed; hub.advance_tx();
    if (elapsed == 1) EXPECT_TRUE(hub.completions.empty());
  }
  ASSERT_EQ(hub.completions.size(), 1u); EXPECT_FALSE(hub.completions[0].success);
}

TEST_F(RadioDeliveryTest, LostRxEndInterruptStillDrainsCompletePacket) {
  const auto p = packet(9); hub.fifo.insert(hub.fifo.end(), p.begin(), p.end());
  hub.rx_ready_ = false;
  EXPECT_TRUE(hub.process_rx());
  ASSERT_EQ(hub.received.size(), 1u); EXPECT_EQ(hub.received[0], p);
  EXPECT_FALSE(hub.illegal_fifo_read);
}

TEST_F(RadioDeliveryTest, UnstablePreparationReturnsToRxAndDoesNotConsumeAPrefix) {
  hub.script[CC1101_RXBYTES] = {1, 2, 1, 2, 1};
  EXPECT_FALSE(hub.send_command_internal_(&command));
  EXPECT_EQ(hub.marc, CC1101_MARCSTATE_RX); EXPECT_TRUE(hub.tx_fifo.empty());
  EXPECT_FALSE(hub.illegal_fifo_read);
  EXPECT_TRUE(hub.send_command_internal_(&command));
}

TEST_F(RadioDeliveryTest, UnstableCooldownCountDoesNotBecomeAnInventedOverflow) {
  queue(); test_now++; hub.advance_tx(); hub.finish_tx(); hub.advance_tx();
  receive(10); hub.script[CC1101_RXBYTES] = {31, 32, 31, 32, 31};
  test_now++; hub.advance_tx();
  ASSERT_EQ(hub.received.size(), 1u); EXPECT_EQ(hub.received[0], packet(10));
  EXPECT_EQ(std::count(hub.strobes.begin(), hub.strobes.end(), CC1101_SFRX), 0);
}

TEST_F(RadioDeliveryTest, UnverifiedIdleFailsClosedWithoutIllegalRecoveryStrobes) {
  hub.script[CC1101_MARCSTATE] = {0, 2, 0, 2, 0, 2, 0, 2, 0, 2};
  hub.flush_and_rx();
  EXPECT_TRUE(hub.radio_fatal_error_);
  EXPECT_EQ(std::count(hub.strobes.begin(), hub.strobes.end(), CC1101_SFRX), 0);
  EXPECT_EQ(std::count(hub.strobes.begin(), hub.strobes.end(), CC1101_SFTX), 0);
}

TEST_F(RadioDeliveryTest, UnstableCcaStatusCannotAuthorizeStx) {
  queue(); test_now++;
  hub.script[CC1101_PKTSTATUS] = {0, 0x10, 0, 0x10, 0};
  hub.advance_tx();
  EXPECT_EQ(std::count(hub.strobes.begin(), hub.strobes.end(), CC1101_STX), 0);
  EXPECT_TRUE(hub.completions.empty());
}

TEST_F(RadioDeliveryTest, ShortChannelTypeCannotUseDirectAddressSerializer) {
  command.pck_inf[0] = 0x44;
  EXPECT_FALSE(hub.send_command_internal_(&command, test_now));
  EXPECT_TRUE(hub.tx_fifo.empty());
  EXPECT_TRUE(hub.completions.empty());
}

TEST_F(RadioDeliveryTest, CapturedCoverCheckSerializesToIndependentLoggedWireBytes) {
  // Full raw TX from capture line 224, not constructed with production crypto.
  command.counter = 2; command.pck_inf[0] = 0x6a; command.pck_inf[1] = 0;
  command.hop = 0x0a; command.channel = 2; command.remote_addr = 0x458130;
  command.dest_addrs[0] = 0x273d2e; command.payload[1] = 4;
  queue();
  const std::vector<uint8_t> observed = {
    0x1d,0x02,0x6a,0x00,0x0a,0x01,0x02,0x45,0x81,0x30,
    0x45,0x81,0x30,0x45,0x81,0x30,0x01,0x27,0x3d,0x2e,
    0x00,0x04,0x83,0xab,0x0f,0x50,0x79,0xa7,0xd3,0x6d};
  EXPECT_EQ(hub.tx_fifo, observed);
}

TEST_F(RadioDeliveryTest, LegacyLightCandidateHasDirectHeaderAndCommandAtWholePayloadOffsetFour) {
  command.counter = 8; command.pck_inf[0] = 0x6a; command.pck_inf[1] = 0x10;
  command.hop = 0; command.channel = 3; command.remote_addr = 0x458130;
  command.dest_addrs[0] = 0xe99b2b; command.payload[1] = 3; command.payload[4] = 0x20;
  queue();
  // Header fields independently recorded in remote RX line 344. Only the
  // candidate byte differs here. This is a layout check, NOT a light golden TX.
  const std::vector<uint8_t> direct_header = {
    0x1d,0x08,0x6a,0x10,0x00,0x01,0x03,0x45,0x81,0x30,
    0x45,0x81,0x30,0x45,0x81,0x30,0x01,0xe9,0x9b,0x2b,0x00,0x03};
  ASSERT_EQ(hub.tx_fifo.size(), 30u);
  EXPECT_TRUE(std::equal(direct_header.begin(), direct_header.end(), hub.tx_fifo.begin()));
  uint8_t fifo[64]{};
  std::copy(hub.tx_fifo.begin(), hub.tx_fifo.end(), fifo); fifo[31] = 0x80;
  auto parsed = packet_parser::parse_fifo_packet(fifo);
  ASSERT_TRUE(parsed.ok);
  EXPECT_EQ(parsed.packet.payload[2], 0x20); // decoded block offset 2 == whole payload offset 4
  EXPECT_EQ(parsed.packet.payload[4], 0x00); // NOT the command position
  EXPECT_EQ(parsed.packet.payload[5], 0x00); // observed short frames can instead carry 0x40
  // A short remote frame is 28 FIFO bytes, has type 0x44 and one dst byte.
  EXPECT_NE(hub.tx_fifo[0], 0x1b);
  EXPECT_NE(hub.tx_fifo[2], 0x44);
}

TEST_F(RadioDeliveryTest, CapturedDirectObservationDiffersInUnknownDynamicCheckByte) {
  command.counter = 8; command.pck_inf[0] = 0x6a; command.pck_inf[1] = 0x10;
  command.hop = 0; command.channel = 3; command.remote_addr = 0x458130;
  command.dest_addrs[0] = 0xe99b2b; command.payload[1] = 3; command.payload[4] = 0x10;
  queue();
  uint8_t fifo[64]{};
  std::copy(hub.tx_fifo.begin(), hub.tx_fifo.end(), fifo); fifo[31] = 0x80;
  auto parsed = packet_parser::parse_fifo_packet(fifo);
  ASSERT_TRUE(parsed.ok);
  // Entire decoded payload from remote RX line 344, independent of our encoder.
  const uint8_t observed[10] = {0x00,0x03,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0xc0};
  EXPECT_EQ(parsed.packet.payload_1, observed[0]);
  EXPECT_EQ(parsed.packet.payload_2, observed[1]);
  for (unsigned i = 0; i < 7; ++i) EXPECT_EQ(parsed.packet.payload[i], observed[i+2]);
  // Current key/parity generation does NOT reproduce the observed last byte.
  // Preserve this evidence instead of "fixing" it with a constant 0xc0.
  EXPECT_NE(parsed.packet.payload[7], observed[9]);
}

TEST_F(RadioDeliveryTest, CapturedFailedHaLightOnMatchesFullWireBytes) {
  // User hardware log 2026-10-10 11:39:32.635. This proves existing
  // serialization, NOT that 0x20 switches the physical light on.
  command.counter = 2; command.pck_inf[0] = 0x6a; command.pck_inf[1] = 0x10;
  command.hop = 0; command.channel = 3; command.remote_addr = 0x458130;
  command.dest_addrs[0] = 0xe99b2b; command.payload[1] = 3; command.payload[4] = 0x20;
  queue();
  const std::vector<uint8_t> observed = {
    0x1d,0x02,0x6a,0x10,0x00,0x01,0x03,0x45,0x81,0x30,
    0x45,0x81,0x30,0x45,0x81,0x30,0x01,0xe9,0x9b,0x2b,
    0x00,0x03,0x83,0xab,0x3f,0x50,0x79,0xa7,0xd3,0xad};
  EXPECT_EQ(hub.tx_fifo, observed);
}

TEST_F(RadioDeliveryTest, CapturedFailedHaLightOffMatchesFullWireBytes) {
  // User hardware log 2026-10-10 11:39:54.976; no OFF semantics established.
  command.counter = 3; command.pck_inf[0] = 0x6a; command.pck_inf[1] = 0x10;
  command.hop = 0; command.channel = 3; command.remote_addr = 0x458130;
  command.dest_addrs[0] = 0xe99b2b; command.payload[1] = 3; command.payload[4] = 0x40;
  queue();
  const std::vector<uint8_t> observed = {
    0x1d,0x03,0x6a,0x10,0x00,0x01,0x03,0x45,0x81,0x30,
    0x45,0x81,0x30,0x45,0x81,0x30,0x01,0xe9,0x9b,0x2b,
    0x00,0x03,0xc3,0xd6,0xef,0x3a,0x49,0x85,0xc3,0x91};
  EXPECT_EQ(hub.tx_fifo, observed);
}
