#include "elero/elero_poll_logic.h"
#include <gtest/gtest.h>

using namespace esphome::elero::poll_logic;

static constexpr uint32_t BASE_INTERVAL = 300000;  // 5 min
static constexpr uint32_t MOVING_INTERVAL = 5000;   // ELERO_POLL_INTERVAL_MOVING
static constexpr uint32_t TIMEOUT = 120000;          // ELERO_TIMEOUT_MOVEMENT
static constexpr uint32_t MIN_IMMEDIATE = 2000;      // ELERO_IMMEDIATE_POLL_MIN_INTERVAL_MS
static constexpr uint32_t POST_DELAY = 5000;         // ELERO_POST_MOVEMENT_POLL_DELAY

// ─── should_poll_now ─────────────────────────────────────────────────────────

TEST(ShouldPoll, ElapsedAboveInterval_True) {
  EXPECT_TRUE(should_poll_now(10000, 0, 5000));
}

TEST(ShouldPoll, ElapsedBelowInterval_False) {
  EXPECT_FALSE(should_poll_now(3000, 0, 5000));
}

TEST(ShouldPoll, ExactlyAtInterval_False) {
  // 5000 - 0 = 5000 > 5000 is false
  EXPECT_FALSE(should_poll_now(5000, 0, 5000));
}

TEST(ShouldPoll, OneOverInterval_True) {
  EXPECT_TRUE(should_poll_now(5001, 0, 5000));
}

// ─── should_immediate_poll ───────────────────────────────────────────────────

TEST(ImmediatePoll, FirstCall_Allowed) {
  EXPECT_TRUE(should_immediate_poll(5000, 0, MIN_IMMEDIATE));
}

TEST(ImmediatePoll, WithinMinInterval_Blocked) {
  EXPECT_FALSE(should_immediate_poll(1000, 0, MIN_IMMEDIATE));
}

TEST(ImmediatePoll, AtMinInterval_Allowed) {
  // 2000 - 0 = 2000 >= 2000 → true
  EXPECT_TRUE(should_immediate_poll(2000, 0, MIN_IMMEDIATE));
}

TEST(ImmediatePoll, AfterMinInterval_Allowed) {
  EXPECT_TRUE(should_immediate_poll(3000, 0, MIN_IMMEDIATE));
}

// ─── should_post_movement_poll ───────────────────────────────────────────────

TEST(PostMovementPoll, ZeroTimestamp_NoPoll) {
  EXPECT_FALSE(should_post_movement_poll(10000, 0));
}

TEST(PostMovementPoll, BeforeTimestamp_NoPoll) {
  EXPECT_FALSE(should_post_movement_poll(4999, 5000));
}

TEST(PostMovementPoll, AtTimestamp_Polls) {
  EXPECT_TRUE(should_post_movement_poll(5000, 5000));
}

TEST(PostMovementPoll, AfterTimestamp_Polls) {
  EXPECT_TRUE(should_post_movement_poll(6000, 5000));
}

// ─── calculate_post_movement_poll_time ───────────────────────────────────────

TEST(PostMovementPollTime, Opening_Normal) {
  // movement_start + open_duration + delay = 1000 + 10000 + 5000 = 16000
  EXPECT_EQ(calculate_post_movement_poll_time(1, 1000, 10000, 15000, POST_DELAY), 16000u);
}

TEST(PostMovementPollTime, Closing_Normal) {
  // movement_start + close_duration + delay = 1000 + 15000 + 5000 = 21000
  EXPECT_EQ(calculate_post_movement_poll_time(-1, 1000, 10000, 15000, POST_DELAY), 21000u);
}

TEST(PostMovementPollTime, ZeroDuration_ReturnsZero) {
  EXPECT_EQ(calculate_post_movement_poll_time(1, 1000, 0, 15000, POST_DELAY), 0u);
}

TEST(PostMovementPollTime, ClosingZeroDuration_ReturnsZero) {
  EXPECT_EQ(calculate_post_movement_poll_time(-1, 1000, 10000, 0, POST_DELAY), 0u);
}

// ─── should_stop_verify ──────────────────────────────────────────────────────

TEST(StopVerify, Inactive_False) {
  EXPECT_FALSE(should_stop_verify(10000, 0));
}

TEST(StopVerify, BeforeTime_False) {
  EXPECT_FALSE(should_stop_verify(4999, 5000));
}

TEST(StopVerify, AtTime_True) {
  EXPECT_TRUE(should_stop_verify(5000, 5000));
}

TEST(StopVerify, AfterTime_True) {
  EXPECT_TRUE(should_stop_verify(6000, 5000));
}

// ─── calculate_poll_interval ─────────────────────────────────────────────────

TEST(PollInterval, Idle_ReturnsBaseInterval) {
  EXPECT_EQ(calculate_poll_interval(0, BASE_INTERVAL, 10000, 10000,
                                    MOVING_INTERVAL, 0, 0, 10000, TIMEOUT), BASE_INTERVAL);
}

TEST(PollInterval, Moving_WithBothDurations_ReturnsBase) {
  // Both durations set → dead-reckoning active → use base interval
  EXPECT_EQ(calculate_poll_interval(1, BASE_INTERVAL, 10000, 10000,
                                    MOVING_INTERVAL, 0, 1000, 5000, TIMEOUT), BASE_INTERVAL);
}

TEST(PollInterval, Moving_NoDurations_ReturnsMovingInterval) {
  // No durations → poll-based tracking
  uint32_t result = calculate_poll_interval(1, BASE_INTERVAL, 0, 0,
                                            MOVING_INTERVAL, 0, 1000, 5000, TIMEOUT);
  EXPECT_EQ(result, MOVING_INTERVAL);
}

TEST(PollInterval, Moving_NoDurations_StaggerOffset) {
  // poll_offset=3000 → stagger = 3000 % 5000 = 3000
  uint32_t result = calculate_poll_interval(1, BASE_INTERVAL, 0, 0,
                                            MOVING_INTERVAL, 3000, 1000, 5000, TIMEOUT);
  EXPECT_EQ(result, MOVING_INTERVAL + 3000);
}

TEST(PollInterval, Moving_NoDurations_PastTimeout) {
  // Movement started at 0, now=130000 → 130000 > 120000 timeout → use base
  EXPECT_EQ(calculate_poll_interval(1, BASE_INTERVAL, 0, 0,
                                    MOVING_INTERVAL, 0, 0, 130000, TIMEOUT), BASE_INTERVAL);
}

TEST(PollInterval, Moving_OnlyOpenDuration_ReturnsMoving) {
  // open_duration set but close_duration=0 → condition (0==0 || 0==0) true
  uint32_t result = calculate_poll_interval(1, BASE_INTERVAL, 10000, 0,
                                            MOVING_INTERVAL, 0, 1000, 5000, TIMEOUT);
  EXPECT_EQ(result, MOVING_INTERVAL);
}

TEST(PollInterval, Closing_NoDurations_ReturnsMoving) {
  uint32_t result = calculate_poll_interval(-1, BASE_INTERVAL, 0, 0,
                                            MOVING_INTERVAL, 0, 1000, 5000, TIMEOUT);
  EXPECT_EQ(result, MOVING_INTERVAL);
}
