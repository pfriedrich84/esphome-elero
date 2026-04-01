#include "elero/elero_light_logic.h"
#include <gtest/gtest.h>

using namespace esphome::elero::light_logic;

static constexpr uint32_t TIMEOUT = 120000;  // ELERO_TIMEOUT_MOVEMENT

// ─── recompute_brightness ────────────────────────────────────────────────────

TEST(RecomputeBrightness, ZeroDuration_NoChange) {
  EXPECT_FLOAT_EQ(recompute_brightness(0.5f, true, 0, 1000, TIMEOUT), 0.5f);
}

TEST(RecomputeBrightness, DimmingUp_NormalProgress) {
  // 50% of 10s duration, starting at 0.0
  EXPECT_FLOAT_EQ(recompute_brightness(0.0f, true, 10000, 5000, TIMEOUT), 0.5f);
}

TEST(RecomputeBrightness, DimmingDown_NormalProgress) {
  EXPECT_FLOAT_EQ(recompute_brightness(1.0f, false, 10000, 2500, TIMEOUT), 0.75f);
}

TEST(RecomputeBrightness, DimmingUp_ClampAtOne) {
  EXPECT_FLOAT_EQ(recompute_brightness(0.9f, true, 10000, 5000, TIMEOUT), 1.0f);
}

TEST(RecomputeBrightness, DimmingDown_ClampAtZero) {
  EXPECT_FLOAT_EQ(recompute_brightness(0.1f, false, 10000, 5000, TIMEOUT), 0.0f);
}

TEST(RecomputeBrightness, ElapsedExceedsTimeout_NoChange) {
  EXPECT_FLOAT_EQ(recompute_brightness(0.5f, true, 10000, TIMEOUT + 1, TIMEOUT), 0.5f);
}

TEST(RecomputeBrightness, ZeroElapsed_NoChange) {
  EXPECT_FLOAT_EQ(recompute_brightness(0.5f, true, 10000, 0, TIMEOUT), 0.5f);
}

TEST(RecomputeBrightness, FullDuration_ReachesMax) {
  EXPECT_FLOAT_EQ(recompute_brightness(0.0f, true, 10000, 10000, TIMEOUT), 1.0f);
}

TEST(RecomputeBrightness, FullDuration_ReachesMin) {
  EXPECT_FLOAT_EQ(recompute_brightness(1.0f, false, 10000, 10000, TIMEOUT), 0.0f);
}

TEST(RecomputeBrightness, SmallElapsed_MinimalChange) {
  float result = recompute_brightness(0.5f, true, 10000, 1, TIMEOUT);
  EXPECT_NEAR(result, 0.5001f, 0.00001f);
}

TEST(RecomputeBrightness, HalfwayUp_ContinueDimming) {
  EXPECT_FLOAT_EQ(recompute_brightness(0.5f, true, 10000, 2500, TIMEOUT), 0.75f);
}

TEST(RecomputeBrightness, HalfwayDown_ContinueDimming) {
  EXPECT_FLOAT_EQ(recompute_brightness(0.5f, false, 10000, 2500, TIMEOUT), 0.25f);
}

// ─── determine_action ────────────────────────────────────────────────────────

TEST(DetermineAction, TurnOff_ReturnsOff) {
  auto r = determine_action(false, 0.0f, 0.5f, 10000);
  EXPECT_EQ(r.action, LightAction::OFF);
  EXPECT_FLOAT_EQ(r.new_brightness, 0.0f);
  EXPECT_FALSE(r.new_is_dimming);
}

TEST(DetermineAction, TurnOn_NoDim_ReturnsOn) {
  auto r = determine_action(true, 1.0f, 0.0f, 0);  // dim_duration=0
  EXPECT_EQ(r.action, LightAction::ON);
  EXPECT_FLOAT_EQ(r.new_brightness, 1.0f);
  EXPECT_FALSE(r.new_is_dimming);
}

TEST(DetermineAction, FullBrightness_ReturnsOn) {
  auto r = determine_action(true, 1.0f, 0.5f, 10000);
  EXPECT_EQ(r.action, LightAction::ON);
  EXPECT_FLOAT_EQ(r.new_brightness, 1.0f);
}

TEST(DetermineAction, CurrentlyOff_DimToTarget) {
  // brightness < epsilon, target < 1.0 → ON then DIM_DOWN
  auto r = determine_action(true, 0.5f, 0.0f, 10000);
  EXPECT_EQ(r.action, LightAction::ON_THEN_DIM_DOWN);
  EXPECT_FLOAT_EQ(r.new_brightness, 1.0f);
  EXPECT_TRUE(r.new_is_dimming);
  EXPECT_FALSE(r.new_dim_up);  // dimming down
}

TEST(DetermineAction, CurrentlyOff_FullBrightness) {
  // brightness < epsilon, target ~= 1.0 → just ON
  auto r = determine_action(true, 1.0f, 0.005f, 10000);
  EXPECT_EQ(r.action, LightAction::ON);
}

TEST(DetermineAction, BrighterThanCurrent_ReturnsDimUp) {
  auto r = determine_action(true, 0.8f, 0.5f, 10000);
  EXPECT_EQ(r.action, LightAction::DIM_UP);
  EXPECT_TRUE(r.new_is_dimming);
  EXPECT_TRUE(r.new_dim_up);
}

TEST(DetermineAction, DimmerThanCurrent_ReturnsDimDown) {
  auto r = determine_action(true, 0.3f, 0.5f, 10000);
  EXPECT_EQ(r.action, LightAction::DIM_DOWN);
  EXPECT_TRUE(r.new_is_dimming);
  EXPECT_FALSE(r.new_dim_up);
}

TEST(DetermineAction, WithinEpsilon_ReturnsNone) {
  auto r = determine_action(true, 0.505f, 0.5f, 10000);
  EXPECT_EQ(r.action, LightAction::NONE);
  EXPECT_FALSE(r.new_is_dimming);
}

TEST(DetermineAction, ExactlyAtCurrent_ReturnsNone) {
  auto r = determine_action(true, 0.5f, 0.5f, 10000);
  EXPECT_EQ(r.action, LightAction::NONE);
}

TEST(DetermineAction, DimDuration0_OnlyOnOff_On) {
  auto r = determine_action(true, 0.5f, 0.0f, 0);
  EXPECT_EQ(r.action, LightAction::ON);
  EXPECT_FLOAT_EQ(r.new_brightness, 1.0f);
}

TEST(DetermineAction, DimDuration0_OnlyOnOff_Off) {
  auto r = determine_action(false, 0.0f, 1.0f, 0);
  EXPECT_EQ(r.action, LightAction::OFF);
}

TEST(DetermineAction, From0Point5_To0Point51_WithinEpsilon) {
  auto r = determine_action(true, 0.51f, 0.5f, 10000);
  EXPECT_EQ(r.action, LightAction::NONE);  // diff=0.01, within epsilon
}

TEST(DetermineAction, From0Point5_To0Point52_DimUp) {
  auto r = determine_action(true, 0.52f, 0.5f, 10000);
  EXPECT_EQ(r.action, LightAction::DIM_UP);
}

TEST(DetermineAction, From0Point5_To0Point48_DimDown) {
  auto r = determine_action(true, 0.48f, 0.5f, 10000);
  EXPECT_EQ(r.action, LightAction::DIM_DOWN);
}

TEST(DetermineAction, From1Point0_To0Point01_DimDown) {
  auto r = determine_action(true, 0.01f, 1.0f, 10000);
  EXPECT_EQ(r.action, LightAction::DIM_DOWN);
}

TEST(DetermineAction, TurnOff_IgnoresBrightness) {
  auto r = determine_action(false, 0.8f, 0.5f, 10000);
  EXPECT_EQ(r.action, LightAction::OFF);
  EXPECT_FLOAT_EQ(r.new_brightness, 0.0f);
}

TEST(DetermineAction, CurrentlyOff_TargetNearFull) {
  // current < epsilon → "currently off" branch
  // 0.995 < 1.0 - 0.01 = 0.99 is false → target is near full → just ON
  auto r = determine_action(true, 0.995f, 0.005f, 10000);
  EXPECT_EQ(r.action, LightAction::ON);
  EXPECT_FLOAT_EQ(r.new_brightness, 1.0f);
}
