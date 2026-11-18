#include "radio_hub.h"
#include "elero/elero_radio_profile.h"
#include <gtest/gtest.h>
#include <future>
#include <thread>

using namespace esphome;
using namespace esphome::elero;

class RadioInitTest : public ::testing::Test {
 protected:
  Elero hub;
  void SetUp() override { test_now = 100; test_sub_us = 0; test_task = 1; }
};

TEST(RadioProfileTest, GoldenRegisterBytesMatchThePreHardeningInit) {
  const RadioRegisterValue golden[] = {
      {0x0b, 0x08}, {0x0c, 0x00}, {0x10, 0x7b}, {0x11, 0x83}, {0x12, 0x13},
      {0x13, 0x52}, {0x14, 0xf8}, {0x0a, 0x00}, {0x15, 0x43}, {0x21, 0xb6},
      {0x22, 0x10}, {0x18, 0x18}, {0x17, 0x3f}, {0x19, 0x1d}, {0x1a, 0x1f},
      {0x1b, 0xc7}, {0x1c, 0x00}, {0x1d, 0xb2}, {0x23, 0xea}, {0x24, 0x2a},
      {0x25, 0x00}, {0x26, 0x1f}, {0x29, 0x59}, {0x2c, 0x81}, {0x2d, 0x35},
      {0x2e, 0x09}, {0x02, 0x06}, {0x07, 0x8c}, {0x08, 0x45}, {0x09, 0x00},
      {0x06, 0x3c}, {0x04, 0xd3}, {0x05, 0x91},
  };
  ASSERT_EQ(sizeof(golden), sizeof(ELERO_RADIO_PROFILE));
  for (size_t i = 0; i < std::size(golden); ++i) {
    EXPECT_EQ(ELERO_RADIO_PROFILE[i].address, golden[i].address);
    EXPECT_EQ(ELERO_RADIO_PROFILE[i].value, golden[i].value);
  }
}

TEST_F(RadioInitTest, ExplicitRxAfterTxPreservesTheExistingProfileAndCustomFrequency) {
  hub.frequency_registers_ = 0x2171c0;
  hub.calibrate_on_rx = true;
  ASSERT_TRUE(hub.init());
  EXPECT_TRUE(hub.radio_profile_valid_);
  EXPECT_EQ(hub.enable_rx_calls, 1u);
  EXPECT_FALSE(hub.illegal_config_write);
  EXPECT_EQ(hub.read_reg(CC1101_MCSM1), 0x3f);  // CCA, RXOFF and TXOFF unchanged.
  EXPECT_EQ(hub.read_reg(CC1101_PKTCTRL1), 0x8c);
  EXPECT_EQ(hub.read_reg(CC1101_PKTCTRL0), 0x45);
  EXPECT_EQ(hub.read_reg(CC1101_MDMCFG4), 0x7b);
  EXPECT_EQ(hub.read_reg(CC1101_MDMCFG3), 0x83);
  EXPECT_EQ(hub.read_reg(CC1101_MDMCFG2), 0x13);
  EXPECT_EQ(hub.read_reg(CC1101_MDMCFG1), 0x52);
  EXPECT_EQ(hub.read_reg(CC1101_DEVIATN), 0x43);
  EXPECT_EQ(hub.read_reg(CC1101_FREQ0), 0xc0);
  for (auto value : hub.patable) EXPECT_EQ(value, 0xc0);
  EXPECT_EQ(hub.marc, CC1101_MARCSTATE_RX);
  // Verification must precede SRX; calibration is allowed to change FSCAL1.
  EXPECT_EQ(hub.read_reg(CC1101_FSCAL1), 1);
}

TEST_F(RadioInitTest, FailedIdleAcquisitionCannotWriteOrActivateAProfile) {
  hub.ignore_idle = true;
  EXPECT_FALSE(hub.init());
  EXPECT_FALSE(hub.radio_profile_valid_);
  EXPECT_EQ(hub.enable_rx_calls, 0u);
  EXPECT_FALSE(hub.illegal_config_write);
  EXPECT_EQ(std::count(hub.strobes.begin(), hub.strobes.end(), CC1101_SRX), 0);
}

TEST_F(RadioInitTest, EveryStaticRegisterMismatchFailsBeforeSrx) {
  for (const auto &reg : ELERO_RADIO_PROFILE) {
    Elero candidate;
    candidate.registers[reg.address] = 0xff;
    candidate.dropped_write = reg.address;
    EXPECT_FALSE(candidate.init()) << unsigned(reg.address);
    EXPECT_FALSE(candidate.radio_profile_valid_);
    EXPECT_EQ(std::count(candidate.strobes.begin(), candidate.strobes.end(), CC1101_SRX), 0);
    t_elero_command command{};
    EXPECT_FALSE(candidate.send_command_internal_(&command));
  }
}

TEST_F(RadioInitTest, FrequencyAndLastPatableEntryAreVerified) {
  hub.dropped_write = CC1101_FREQ2;
  EXPECT_FALSE(hub.init());
  hub.dropped_write = 0xff;
  hub.corrupt_patable = true;
  EXPECT_FALSE(hub.init());
  EXPECT_EQ(std::count(hub.strobes.begin(), hub.strobes.end(), CC1101_SRX), 0);
}

TEST_F(RadioInitTest, SetterErrorsAndSilentNoOpsCannotAuthorizeRx) {
  hub.enable_rx_error = true;
  EXPECT_FALSE(hub.init());
  hub.enable_rx_error = false; hub.enable_rx_noop = true;
  EXPECT_FALSE(hub.init());  // MCSM1 remains 0x3c without TXOFF=RX.
  EXPECT_EQ(std::count(hub.strobes.begin(), hub.strobes.end(), CC1101_SRX), 0);
}

TEST_F(RadioInitTest, FailedRxEntryLeavesProfileInvalidAndRadioIdle) {
  hub.reject_rx = true;
  EXPECT_FALSE(hub.init());
  EXPECT_FALSE(hub.radio_profile_valid_);
  EXPECT_EQ(hub.marc, CC1101_MARCSTATE_IDLE);
  const auto strobes = hub.strobes.size();
  hub.flush_rx(); hub.flush_and_rx();
  EXPECT_EQ(hub.strobes.size(), strobes);
}

TEST_F(RadioInitTest, ReinitReportsInitFailureAndRetainsPreviousRequestedFrequency) {
  const auto id = hub.reinit_request_.begin(test_now);
  hub.dropped_write = CC1101_PKTCTRL1;
  hub.handle_frequency_reinit_(id, 0x22, 0x72, 0x80);
  EXPECT_EQ(hub.reinit_request_.poll(id, test_now), FrequencyReinitRequest::Result::FAILED);
  EXPECT_EQ(hub.get_frequency_registers(), 0x21717au);
  EXPECT_FALSE(hub.radio_profile_valid_);
  EXPECT_FALSE(hub.process_rx());
  hub.dropped_write = 0xff;
  test_now += 5001;
  hub.check_radio_state_();  // Not a blind SRX restart of the failed profile.
  EXPECT_EQ(hub.resets, 2u);
  EXPECT_TRUE(hub.radio_profile_valid_);
  EXPECT_EQ(hub.read_reg(CC1101_FREQ2), 0x21);
  EXPECT_EQ(hub.read_reg(CC1101_FREQ0), 0x7a);
}

TEST_F(RadioInitTest, ReinitSuccessRequiresVerifiedRxAndBusyTxIsNotReset) {
  auto id = hub.reinit_request_.begin(test_now);
  hub.tx_state_ = TxState::TRANSMITTING;
  hub.handle_frequency_reinit_(id, 0x22, 0x72, 0x80);
  EXPECT_EQ(hub.reinit_request_.poll(id, test_now), FrequencyReinitRequest::Result::FAILED);
  EXPECT_EQ(hub.resets, 0u);
  hub.tx_state_ = TxState::IDLE;
  id = hub.reinit_request_.begin(test_now);
  hub.handle_frequency_reinit_(id, 0x22, 0x72, 0x80);
  EXPECT_EQ(hub.reinit_request_.poll(id, test_now), FrequencyReinitRequest::Result::SUCCEEDED);
  EXPECT_EQ(hub.get_frequency_registers(), 0x227280u); EXPECT_EQ(hub.read_reg(CC1101_FREQ0), 0x80);
}

TEST_F(RadioInitTest, PublicReinitRejectsSelfCallsAndQueueFailureWithoutLeavingABusySlot) {
  test_task = hub.radio_task_handle_;
  EXPECT_FALSE(hub.reinit_frequency(0x22, 0x72, 0x80));
  EXPECT_EQ(hub.resets, 0u);
  test_task = 1;
  hub.control_queue.full = true;
  EXPECT_FALSE(hub.reinit_frequency(0x22, 0x72, 0x80));
  hub.control_queue.full = false;
  EXPECT_TRUE(hub.reinit_frequency(0x22, 0x72, 0x80));
  EXPECT_EQ(hub.get_frequency_registers(), 0x227280u);
  hub.radio_fatal_error_ = true;
  EXPECT_FALSE(hub.reinit_frequency(0x21, 0x71, 0x7a));
  hub.radio_fatal_error_ = false;
  hub.radio_task_handle_ = 0;
  EXPECT_FALSE(hub.reinit_frequency(0x21, 0x71, 0x7a));
}

TEST_F(RadioInitTest, PublicReinitTimeoutCannotLeaveDanglingPointersOrRunALateQueuedRequest) {
  hub.control_queue.immediate = false;
  EXPECT_FALSE(hub.reinit_frequency(0x22, 0x72, 0x80));
  EXPECT_EQ(test_now, 100u + FrequencyReinitRequest::TIMEOUT_MS);
  ASSERT_EQ(hub.control_queue.messages.size(), 1u);
  const auto msg = hub.control_queue.messages.front();
  hub.handle_frequency_reinit_(msg.request_id, msg.freq.freq2, msg.freq.freq1, msg.freq.freq0);
  EXPECT_EQ(hub.resets, 0u);
  hub.control_queue.immediate = true;
  EXPECT_TRUE(hub.reinit_frequency(0x21, 0x71, 0x7a));
}

TEST_F(RadioInitTest, RunningReinitMayFinishAfterCallerTimeoutAndPublishesOnlyVerifiedFrequency) {
  hub.radio_storage.during_enable_rx = [&]() {
    // Simulate the caller timing out while Core 0 is inside initialization.
    EXPECT_EQ(hub.get_frequency_registers(), 0x21717au);
    test_now += FrequencyReinitRequest::TIMEOUT_MS;
    // This is the first request in a fresh component, so its queue token is 1.
    EXPECT_EQ(hub.reinit_request_.poll(1, test_now), FrequencyReinitRequest::Result::TIMED_OUT);
    EXPECT_EQ(hub.reinit_request_.begin(test_now), 0u);
  };
  EXPECT_FALSE(hub.reinit_frequency(0x22, 0x72, 0x80));
  EXPECT_TRUE(hub.radio_profile_valid_);
  EXPECT_EQ(hub.get_frequency_registers(), 0x227280u);
  EXPECT_NE(hub.reinit_request_.begin(test_now), 0u);
}

TEST_F(RadioInitTest, ExpiredOrCancelledQueueItemsCannotReconfigureTheRadio) {
  auto id = hub.reinit_request_.begin(test_now);
  hub.reinit_request_.cancel(id);  // Queue full or caller timeout before dequeue.
  hub.handle_frequency_reinit_(id, 0x22, 0x72, 0x80);
  EXPECT_EQ(hub.resets, 0u);
  id = hub.reinit_request_.begin(test_now);
  test_now += FrequencyReinitRequest::TIMEOUT_MS;
  hub.handle_frequency_reinit_(id, 0x22, 0x72, 0x80);
  EXPECT_EQ(hub.resets, 0u);
  EXPECT_EQ(hub.reinit_request_.poll(id, test_now), FrequencyReinitRequest::Result::FAILED);
}

TEST_F(RadioInitTest, InvalidProfileRecoveryKeepsExistingResetBudgetAndNeverBlindlyStartsRx) {
  hub.radio_profile_valid_ = false;
  hub.dropped_write = CC1101_PKTCTRL1;
  for (int i = 0; i < 4; ++i) { test_now += 5001; hub.check_radio_state_(); }
  EXPECT_EQ(hub.resets, 3u);
  EXPECT_TRUE(hub.radio_fatal_error_);
  EXPECT_EQ(std::count(hub.strobes.begin(), hub.strobes.end(), CC1101_SRX), 0);
}

TEST_F(RadioInitTest, IdlePollingHandlesSettlingAndMicrosWrapWithoutAllocating) {
  hub.script[CC1101_MARCSTATE] = {2, 2, 1, 1};
  EXPECT_TRUE(hub.enter_idle_());
  EXPECT_GE(test_sub_us, 500u);
  test_sub_us = 0xfffffff0u - test_now * 1000;
  hub.ignore_idle = true; hub.marc = CC1101_MARCSTATE_XOFF;
  const auto start = micros();
  EXPECT_FALSE(hub.enter_idle_());
  EXPECT_GE(uint32_t(micros() - start), 100000u);
  EXPECT_LE(uint32_t(micros() - start), 100500u);
}

TEST_F(RadioInitTest, AirtimeUsesFinalProfileAndIncludesLengthByteWithoutChangingState) {
  const auto before = hub.tx_state_.load();
  EXPECT_EQ(hub.estimate_airtime_us_(30), 5002u);
  EXPECT_EQ(hub.estimate_airtime_us_(58), 7920u);
  EXPECT_EQ(hub.radio_storage.last_rate.fsk.bitRate, 76767u);
  EXPECT_EQ(hub.radio_storage.last_packet.fsk.preambleLength, 96);
  EXPECT_EQ(hub.radio_storage.last_packet.fsk.syncWordLength, 32);
  EXPECT_EQ(hub.radio_storage.last_packet.fsk.crcLength, 2);
  EXPECT_EQ(hub.radio_storage.last_length, 58u);
  EXPECT_EQ(hub.tx_state_, before);
  EXPECT_TRUE(hub.strobes.empty());
}

TEST(FrequencyReinitRequestTest, LateRunningCompletionHasNoCallerLifetimeAndBlocksOverlappingAdmission) {
  FrequencyReinitRequest request;
  auto id = request.begin(0);
  ASSERT_TRUE(request.start(id, 1));
  EXPECT_EQ(request.begin(2), 0u);
  EXPECT_EQ(request.poll(id, 5000), FrequencyReinitRequest::Result::TIMED_OUT);
  EXPECT_EQ(request.begin(5001), 0u);  // Running operation still owns the slot.
  request.complete(id, true, 5002);  // Safe after caller returns; no semaphore/stack pointer.
  const auto next = request.begin(5003);
  ASSERT_NE(next, 0u); ASSERT_NE(next, id);
  request.complete(id, true, 5004);
  EXPECT_EQ(request.poll(next, 5004), FrequencyReinitRequest::Result::PENDING);
  EXPECT_FALSE(request.start(id, 5004));
  ASSERT_TRUE(request.start(next, 5004));
  request.complete(next, false, 5005);
  EXPECT_EQ(request.poll(next, 5005), FrequencyReinitRequest::Result::FAILED);
}

TEST(FrequencyReinitRequestTest, ConcurrentCallerTimeoutKeepsRunningWorkerOwnedStateAlive) {
  FrequencyReinitRequest request;
  const auto id = request.begin(0);
  std::promise<bool> started;
  auto started_result = started.get_future();
  std::promise<void> finish;
  auto finish_signal = finish.get_future();
  std::thread worker([&]() {
    started.set_value(request.start(id, 1));
    finish_signal.wait();
    request.complete(id, true, 5002);
  });
  EXPECT_TRUE(started_result.get());
  EXPECT_EQ(request.poll(id, 5000), FrequencyReinitRequest::Result::TIMED_OUT);
  EXPECT_EQ(request.begin(5001), 0u);
  finish.set_value();
  worker.join();
  EXPECT_NE(request.begin(5003), 0u);
}

TEST(FrequencyReinitRequestTest, CompletionRacingTimeoutNeverLeaksTheSingleFlightSlot) {
  for (int i = 0; i < 32; ++i) {
    FrequencyReinitRequest request;
    const auto id = request.begin(0);
    ASSERT_TRUE(request.start(id, 1));
    std::promise<void> release;
    auto gate = release.get_future().share();
    auto caller = std::async(std::launch::async, [&]() { gate.wait(); return request.poll(id, 5000); });
    auto worker = std::async(std::launch::async, [&]() { gate.wait(); request.complete(id, true, 4999); });
    release.set_value();
    const auto result = caller.get();
    worker.get();
    EXPECT_TRUE(result == FrequencyReinitRequest::Result::SUCCEEDED ||
                result == FrequencyReinitRequest::Result::TIMED_OUT);
    EXPECT_NE(request.begin(5001), 0u);
  }
}

TEST(FrequencyReinitRequestTest, QueuedTimeoutAndAdmissionFailureReleaseSlotAndHandleMillisWrap) {
  FrequencyReinitRequest request;
  auto id = request.begin(0xfffffff0u);
  EXPECT_EQ(request.poll(id, 0x10u), FrequencyReinitRequest::Result::PENDING);
  EXPECT_EQ(request.poll(id, uint32_t(0xfffffff0u + 5000u)), FrequencyReinitRequest::Result::TIMED_OUT);
  EXPECT_FALSE(request.start(id, 5000));
  id = request.begin(5001);
  ASSERT_NE(id, 0u);
  request.cancel(id);
  EXPECT_NE(request.begin(5002), 0u);
}
