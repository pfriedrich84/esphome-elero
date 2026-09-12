#include "radio_hub.h"
#include <gtest/gtest.h>
using namespace esphome;
using namespace esphome::elero;

class RadioDeliveryTest : public ::testing::Test {
 protected:
  Elero hub;
  t_elero_command command{};
  void SetUp() override {
    test_now = 100;
    command.remote_addr = 0x123456; command.num_dests = 1; command.dest_addrs[0] = 0x111111;
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
