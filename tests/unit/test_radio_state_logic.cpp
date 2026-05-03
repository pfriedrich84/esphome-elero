#include "elero/elero_radio_state_logic.h"
#include <gtest/gtest.h>

using namespace esphome::elero::radio_state_logic;

TEST(RadioStateLogic, TxProgressStatesAreRecognized) {
  EXPECT_TRUE(is_tx_progress_state(CC1101_MARCSTATE_TX));
  EXPECT_TRUE(is_tx_progress_state(CC1101_MARCSTATE_STARTCAL));
  EXPECT_TRUE(is_tx_progress_state(CC1101_MARCSTATE_RXTX_SWITCH));
  EXPECT_FALSE(is_tx_progress_state(CC1101_MARCSTATE_IDLE));
  EXPECT_FALSE(is_tx_progress_state(CC1101_MARCSTATE_TXFIFO_UFLOW));
}

TEST(RadioStateLogic, WatchdogStateClassification) {
  EXPECT_TRUE(is_watchdog_healthy_rx(CC1101_MARCSTATE_RX));
  EXPECT_FALSE(is_watchdog_healthy_rx(CC1101_MARCSTATE_IDLE));

  EXPECT_TRUE(is_watchdog_transient_state(CC1101_MARCSTATE_VCOON_MC));
  EXPECT_TRUE(is_watchdog_transient_state(CC1101_MARCSTATE_ENDCAL));
  EXPECT_TRUE(is_watchdog_transient_state(CC1101_MARCSTATE_RX_END));
  EXPECT_TRUE(is_watchdog_transient_state(CC1101_MARCSTATE_RX_RST));
  EXPECT_FALSE(is_watchdog_transient_state(CC1101_MARCSTATE_RXFIFO_OFLOW));

  EXPECT_TRUE(should_restart_rx_from_idle(CC1101_MARCSTATE_IDLE));
  EXPECT_FALSE(should_restart_rx_from_idle(CC1101_MARCSTATE_RX));
}

TEST(RadioStateLogic, RxDrainLimitPrioritizesPendingTx) {
  EXPECT_EQ(rx_drain_limit(false, false, 8), 8);
  EXPECT_EQ(rx_drain_limit(true, false, 8), 1);
  EXPECT_EQ(rx_drain_limit(false, true, 8), 1);
  EXPECT_EQ(rx_drain_limit(true, true, 8), 1);
}

TEST(RadioStateLogic, FifoCountAndCompleteness) {
  EXPECT_EQ(bounded_fifo_count(10, 64), 10);
  EXPECT_EQ(bounded_fifo_count(65, 64), 64);

  EXPECT_TRUE(has_complete_fifo_packet(29, 32));
  EXPECT_FALSE(has_complete_fifo_packet(29, 31));
}
