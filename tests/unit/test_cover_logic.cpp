#include "elero/elero_cover_logic.h"
#include <gtest/gtest.h>

using namespace esphome::elero::cover_logic;

static constexpr uint32_t TIMEOUT = 120000;  // ELERO_TIMEOUT_MOVEMENT
static constexpr uint32_t BASE_COMP = 300;   // ELERO_TX_LATENCY_COMPENSATION_MS

TEST(CommandCooldown, BlocksUntilDeadlineIncludingMillisWrap) {
  EXPECT_FALSE(command_cooldown_active(1000, 0));
  EXPECT_TRUE(command_cooldown_active(1000, 4000));
  EXPECT_FALSE(command_cooldown_active(4000, 4000));
  EXPECT_TRUE(command_cooldown_active(0xFFFFFFF0u, 20));
  EXPECT_FALSE(command_cooldown_active(20, 20));
}

TEST(RedundantPosition, SuppressesOnlyMatchingIntermediateTargets) {
  EXPECT_TRUE(is_redundant_intermediate_target(0.75f, 0.75f));
  EXPECT_TRUE(is_redundant_intermediate_target(0.754f, 0.75f));
  EXPECT_FALSE(is_redundant_intermediate_target(0.70f, 0.75f));
}

TEST(RedundantPosition, NeverSuppressesOpenOrCloseEndpoints) {
  EXPECT_FALSE(is_redundant_intermediate_target(1.0f, 1.0f));
  EXPECT_FALSE(is_redundant_intermediate_target(0.0f, 0.0f));
  EXPECT_FALSE(is_redundant_intermediate_target(0.995f, 1.0f));
  EXPECT_FALSE(is_redundant_intermediate_target(0.005f, 0.0f));
}

// ─── recompute_position ──────────────────────────────────────────────────────

TEST(RecomputePosition, Idle_NoChange) {
  EXPECT_FLOAT_EQ(recompute_position(0.5f, 0, 10000.0f, 1000, TIMEOUT), 0.5f);
}

TEST(RecomputePosition, Opening_NormalProgress) {
  // 50% of 10s duration elapsed, starting at 0.0
  EXPECT_FLOAT_EQ(recompute_position(0.0f, 1, 10000.0f, 5000, TIMEOUT), 0.5f);
}

TEST(RecomputePosition, Closing_NormalProgress) {
  // 25% of 10s duration elapsed, starting at 1.0
  EXPECT_FLOAT_EQ(recompute_position(1.0f, -1, 10000.0f, 2500, TIMEOUT), 0.75f);
}

TEST(RecomputePosition, Opening_ClampAtOne) {
  // elapsed exceeds remaining travel — should clamp at 1.0
  EXPECT_FLOAT_EQ(recompute_position(0.9f, 1, 10000.0f, 5000, TIMEOUT), 1.0f);
}

TEST(RecomputePosition, Closing_ClampAtZero) {
  EXPECT_FLOAT_EQ(recompute_position(0.1f, -1, 10000.0f, 5000, TIMEOUT), 0.0f);
}

TEST(RecomputePosition, ZeroDuration_NoChange) {
  EXPECT_FLOAT_EQ(recompute_position(0.5f, 1, 0.0f, 1000, TIMEOUT), 0.5f);
}

TEST(RecomputePosition, ElapsedExceedsTimeout_NoChange) {
  EXPECT_FLOAT_EQ(recompute_position(0.5f, 1, 10000.0f, TIMEOUT + 1, TIMEOUT), 0.5f);
}

TEST(RecomputePosition, ZeroElapsed_NoChange) {
  EXPECT_FLOAT_EQ(recompute_position(0.5f, 1, 10000.0f, 0, TIMEOUT), 0.5f);
}

TEST(RecomputePosition, SmallElapsed_1ms) {
  float result = recompute_position(0.0f, 1, 10000.0f, 1, TIMEOUT);
  EXPECT_NEAR(result, 0.0001f, 0.00001f);
}

TEST(RecomputePosition, FullDuration_OpensCompletely) {
  EXPECT_FLOAT_EQ(recompute_position(0.0f, 1, 10000.0f, 10000, TIMEOUT), 1.0f);
}

TEST(RecomputePosition, FullDuration_ClosesCompletely) {
  EXPECT_FLOAT_EQ(recompute_position(1.0f, -1, 10000.0f, 10000, TIMEOUT), 0.0f);
}

TEST(RecomputePosition, HalfwayOpen_ContinueClosing) {
  EXPECT_FLOAT_EQ(recompute_position(0.5f, -1, 10000.0f, 2500, TIMEOUT), 0.25f);
}

TEST(RecomputePosition, NearBoundary_Opening) {
  float result = recompute_position(0.999f, 1, 10000.0f, 100, TIMEOUT);
  EXPECT_FLOAT_EQ(result, 1.0f);  // clamped
}

TEST(RecomputePosition, NearBoundary_Closing) {
  float result = recompute_position(0.001f, -1, 10000.0f, 100, TIMEOUT);
  EXPECT_FLOAT_EQ(result, 0.0f);  // clamped
}

TEST(RecomputePosition, LargeDuration_SmallElapsed) {
  float result = recompute_position(0.0f, 1, 120000.0f, 100, TIMEOUT);
  EXPECT_NEAR(result, 100.0f / 120000.0f, 0.00001f);
}

TEST(RecomputePosition, SmallDuration_LargeElapsed) {
  float result = recompute_position(0.0f, 1, 1000.0f, 500, TIMEOUT);
  EXPECT_FLOAT_EQ(result, 0.5f);
}

TEST(RecomputePosition, MidPosition_Opening) {
  float result = recompute_position(0.3f, 1, 10000.0f, 200, TIMEOUT);
  EXPECT_NEAR(result, 0.32f, 0.0001f);
}

TEST(RecomputePosition, MidPosition_Closing) {
  float result = recompute_position(0.7f, -1, 10000.0f, 200, TIMEOUT);
  EXPECT_NEAR(result, 0.68f, 0.0001f);
}

TEST(RecomputePosition, ExactTimeout_NoChange) {
  // elapsed == timeout → exceeds (> not >=), so should still compute
  // Actually: elapsed > timeout_ms, so elapsed == timeout is NOT > timeout, should compute
  float result = recompute_position(0.0f, 1, 10000.0f, TIMEOUT, TIMEOUT);
  // 120000/10000 = 12.0 → clamps to 1.0
  EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(RecomputePosition, OneOverTimeout_NoChange) {
  EXPECT_FLOAT_EQ(recompute_position(0.5f, 1, 10000.0f, TIMEOUT + 1, TIMEOUT), 0.5f);
}

TEST(RecomputePosition, AsymmetricDurations_Opening) {
  // open_duration=5s, close_duration=15s — only open matters here
  EXPECT_FLOAT_EQ(recompute_position(0.0f, 1, 5000.0f, 2500, TIMEOUT), 0.5f);
}

TEST(RecomputePosition, AsymmetricDurations_Closing) {
  EXPECT_FLOAT_EQ(recompute_position(1.0f, -1, 15000.0f, 7500, TIMEOUT), 0.5f);
}

// ─── calculate_margin ────────────────────────────────────────────────────────

TEST(CalculateMargin, ZeroDuration) {
  EXPECT_FLOAT_EQ(calculate_margin(300, 0), 0.0f);
}

TEST(CalculateMargin, Normal) {
  EXPECT_FLOAT_EQ(calculate_margin(300, 10000), 0.03f);
}

TEST(CalculateMargin, LargeCompensation) {
  EXPECT_FLOAT_EQ(calculate_margin(600, 10000), 0.06f);
}

TEST(CalculateMargin, SmallDuration) {
  EXPECT_FLOAT_EQ(calculate_margin(300, 1000), 0.3f);
}

// ─── is_at_target ────────────────────────────────────────────────────────────

TEST(IsAtTarget, FullyOpen_ReturnsFalse) {
  EXPECT_FALSE(is_at_target(0.5f, 1.0f, 1, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, FullyClosed_ReturnsFalse) {
  EXPECT_FALSE(is_at_target(0.5f, 0.0f, -1, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, Idle_ReturnsTrue) {
  EXPECT_TRUE(is_at_target(0.5f, 0.8f, 0, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, Opening_NotYetReached) {
  // target=0.8, position=0.3, margin=0.03 → 0.3 < 0.77 → not reached
  EXPECT_FALSE(is_at_target(0.3f, 0.8f, 1, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, Opening_Reached) {
  // target=0.8, margin=300/10000=0.03, 0.78 >= 0.77 → reached
  EXPECT_TRUE(is_at_target(0.78f, 0.8f, 1, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, Closing_NotYetReached) {
  // target=0.3, position=0.8, margin=0.03 → 0.8 > 0.33 → not reached
  EXPECT_FALSE(is_at_target(0.8f, 0.3f, -1, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, Closing_Reached) {
  // target=0.3, margin=0.03 → 0.32 <= 0.33 → reached
  EXPECT_TRUE(is_at_target(0.32f, 0.3f, -1, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, ZeroQueueDepth_BaseCompensation) {
  // margin = 300.0/10000.0 ≈ 0.03, threshold ≈ 0.77
  // Avoid exact boundary (float rounding); use value safely above
  EXPECT_TRUE(is_at_target(0.771f, 0.8f, 1, 10000, 10000, 0, BASE_COMP));
  EXPECT_FALSE(is_at_target(0.76f, 0.8f, 1, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, LargeQueueDepth_IncreasedMargin) {
  // compensation = 300 + 10*15 = 450, margin = 450/10000 = 0.045
  // target=0.8, threshold=0.755
  EXPECT_TRUE(is_at_target(0.76f, 0.8f, 1, 10000, 10000, 10, BASE_COMP));
  EXPECT_FALSE(is_at_target(0.74f, 0.8f, 1, 10000, 10000, 10, BASE_COMP));
}

TEST(IsAtTarget, ZeroDuration_ZeroMargin_Opening) {
  // open_duration=0, margin=0 → must be exactly at target
  EXPECT_FALSE(is_at_target(0.79f, 0.8f, 1, 0, 10000, 0, BASE_COMP));
  EXPECT_TRUE(is_at_target(0.8f, 0.8f, 1, 0, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, ZeroDuration_ZeroMargin_Closing) {
  EXPECT_FALSE(is_at_target(0.31f, 0.3f, -1, 10000, 0, 0, BASE_COMP));
  EXPECT_TRUE(is_at_target(0.3f, 0.3f, -1, 10000, 0, 0, BASE_COMP));
}

TEST(IsAtTarget, ExactlyAtThreshold_Opening) {
  // margin ≈ 0.03, threshold ≈ 0.77; use value safely above boundary
  EXPECT_TRUE(is_at_target(0.771f, 0.8f, 1, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, JustBelowThreshold_Opening) {
  // 0.769 < 0.77 → not reached
  EXPECT_FALSE(is_at_target(0.769f, 0.8f, 1, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, ExactlyAtThreshold_Closing) {
  // margin=0.03, target=0.3, threshold=0.33
  EXPECT_TRUE(is_at_target(0.33f, 0.3f, -1, 10000, 10000, 0, BASE_COMP));
}

TEST(IsAtTarget, JustAboveThreshold_Closing) {
  // 0.331 > 0.33 → not reached
  EXPECT_FALSE(is_at_target(0.331f, 0.3f, -1, 10000, 10000, 0, BASE_COMP));
}
