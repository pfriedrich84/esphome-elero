#include "elero/elero_dispatch_logic.h"
#include <gtest/gtest.h>

using namespace esphome::elero::dispatch_logic;

static constexpr uint32_t MAX_AGE = 30000;     // ELERO_COMMAND_QUEUE_MAX_AGE_MS
static constexpr uint8_t MAX_RETRIES = 3;       // ELERO_SEND_RETRIES
static constexpr uint8_t CMD_STOP = 0x10;       // ELERO_COMMAND_COVER_STOP
static constexpr uint8_t CMD_CHECK = 0x00;      // ELERO_COMMAND_COVER_CHECK
static constexpr uint8_t CMD_UP = 0x20;
static constexpr uint8_t CMD_DOWN = 0x40;

// ─── should_clear_stale_queue ────────────────────────────────────────────────

TEST(StaleQueue, FreshQueue_NoStale) {
  EXPECT_FALSE(should_clear_stale_queue(1000, 1000, MAX_AGE));
}

TEST(StaleQueue, JustUnderAge_NoStale) {
  // 29999 - 0 = 29999, not > 30000
  EXPECT_FALSE(should_clear_stale_queue(29999, 0, MAX_AGE));
}

TEST(StaleQueue, ExactlyAtAge_NoStale) {
  // 30000 - 0 = 30000, > 30000 is false
  EXPECT_FALSE(should_clear_stale_queue(30000, 0, MAX_AGE));
}

TEST(StaleQueue, JustOverAge_IsStale) {
  EXPECT_TRUE(should_clear_stale_queue(30001, 0, MAX_AGE));
}

TEST(StaleQueue, LargeAge_IsStale) {
  EXPECT_TRUE(should_clear_stale_queue(60000, 0, MAX_AGE));
}

// ─── should_defer_for_stop ───────────────────────────────────────────────────

TEST(DeferForStop, NotUrgent_NeverDefers) {
  EXPECT_FALSE(should_defer_for_stop(false, CMD_UP, CMD_STOP, CMD_CHECK));
  EXPECT_FALSE(should_defer_for_stop(false, CMD_DOWN, CMD_STOP, CMD_CHECK));
}

TEST(DeferForStop, Urgent_StopCommand_NotDeferred) {
  EXPECT_FALSE(should_defer_for_stop(true, CMD_STOP, CMD_STOP, CMD_CHECK));
}

TEST(DeferForStop, Urgent_CheckCommand_NotDeferred) {
  EXPECT_FALSE(should_defer_for_stop(true, CMD_CHECK, CMD_STOP, CMD_CHECK));
}

TEST(DeferForStop, Urgent_UpCommand_Deferred) {
  EXPECT_TRUE(should_defer_for_stop(true, CMD_UP, CMD_STOP, CMD_CHECK));
}

TEST(DeferForStop, Urgent_DownCommand_Deferred) {
  EXPECT_TRUE(should_defer_for_stop(true, CMD_DOWN, CMD_STOP, CMD_CHECK));
}

// ─── calculate_dispatch_delay ────────────────────────────────────────────────

TEST(DispatchDelay, ZeroRetries_BaseDelay) {
  EXPECT_EQ(calculate_dispatch_delay(20, 0), 20u);
}

TEST(DispatchDelay, OneRetry_Plus20ms) {
  // 10 << 1 = 20
  EXPECT_EQ(calculate_dispatch_delay(20, 1), 40u);
}

TEST(DispatchDelay, TwoRetries_Plus40ms) {
  // 10 << 2 = 40
  EXPECT_EQ(calculate_dispatch_delay(20, 2), 60u);
}

TEST(DispatchDelay, ThreeRetries_Plus80ms) {
  // 10 << 3 = 80
  EXPECT_EQ(calculate_dispatch_delay(20, 3), 100u);
}

TEST(DispatchDelay, FourRetries_CappedAt80ms) {
  // min(4, 3) = 3, 10 << 3 = 80
  EXPECT_EQ(calculate_dispatch_delay(20, 4), 100u);
}

TEST(DispatchDelay, TenRetries_StillCapped) {
  EXPECT_EQ(calculate_dispatch_delay(20, 10), 100u);
}

// ─── is_dispatch_ready ───────────────────────────────────────────────────────

TEST(DispatchReady, StopCommand_AlwaysReady) {
  EXPECT_TRUE(is_dispatch_ready(true, 0, 0, 1000));
  EXPECT_TRUE(is_dispatch_ready(true, 10, 10, 1000));
}

TEST(DispatchReady, ElapsedAboveDelay_Ready) {
  EXPECT_TRUE(is_dispatch_ready(false, 1100, 0, 1000));
}

TEST(DispatchReady, ElapsedBelowDelay_NotReady) {
  EXPECT_FALSE(is_dispatch_ready(false, 500, 0, 1000));
}

TEST(DispatchReady, ExactlyAtDelay_NotReady) {
  // 1000 - 0 = 1000 > 1000 is false
  EXPECT_FALSE(is_dispatch_ready(false, 1000, 0, 1000));
}

TEST(DispatchReady, OneOverDelay_Ready) {
  EXPECT_TRUE(is_dispatch_ready(false, 1001, 0, 1000));
}

// ─── should_drop_after_retries ───────────────────────────────────────────────

TEST(RetryExhaustion, BelowMax_NotDropped) {
  EXPECT_FALSE(should_drop_after_retries(2, MAX_RETRIES));
}

TEST(RetryExhaustion, AtMax_NotDropped) {
  // retries == 3, max == 3 → 3 > 3 is false
  EXPECT_FALSE(should_drop_after_retries(MAX_RETRIES, MAX_RETRIES));
}

TEST(RetryExhaustion, AboveMax_Dropped) {
  EXPECT_TRUE(should_drop_after_retries(MAX_RETRIES + 1, MAX_RETRIES));
}

TEST(RetryExhaustion, ZeroRetries_NotDropped) {
  EXPECT_FALSE(should_drop_after_retries(0, MAX_RETRIES));
}

TEST(RetryExhaustion, WayOverMax_Dropped) {
  EXPECT_TRUE(should_drop_after_retries(10, MAX_RETRIES));
}
