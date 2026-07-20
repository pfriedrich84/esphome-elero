#include "elero/elero_runtime_blind_logic.h"
#include <gtest/gtest.h>

using namespace esphome::elero::runtime_blind_logic;

TEST(RuntimeBlindLogic, PollRequiresEnabledIntervalEmptyQueueAndElapsedTime) {
  EXPECT_FALSE(is_poll_enabled(0));
  EXPECT_FALSE(is_poll_enabled(UINT32_MAX));
  EXPECT_TRUE(is_poll_enabled(300000));

  EXPECT_FALSE(should_poll_runtime_blind(1000, 0, 0, true));
  EXPECT_FALSE(should_poll_runtime_blind(1000, 0, 1000, false));
  EXPECT_FALSE(should_poll_runtime_blind(999, 0, 1000, true));
  EXPECT_TRUE(should_poll_runtime_blind(1000, 0, 1000, true));
}

TEST(RuntimeBlindLogic, DirectionFollowsMovingAndEndpointStates) {
  EXPECT_EQ(direction_for_state(0x08, 0x01, 0x02, 0x08, 0x0a, 0x09, 0x0b, 0), 1);
  EXPECT_EQ(direction_for_state(0x0b, 0x01, 0x02, 0x08, 0x0a, 0x09, 0x0b, 0), -1);
  EXPECT_EQ(direction_for_state(0x01, 0x01, 0x02, 0x08, 0x0a, 0x09, 0x0b, 1), 0);
  EXPECT_EQ(direction_for_state(0xff, 0x01, 0x02, 0x08, 0x0a, 0x09, 0x0b, -1), -1);
}

TEST(RuntimeBlindLogic, StopStatesAreExplicit) {
  EXPECT_TRUE(state_stops_runtime_motion(0x0d, 0x0d, 0x03, 0x04, 0x05, 0x06, 0x07));
  EXPECT_TRUE(state_stops_runtime_motion(0x03, 0x0d, 0x03, 0x04, 0x05, 0x06, 0x07));
  EXPECT_FALSE(state_stops_runtime_motion(0x0a, 0x0d, 0x03, 0x04, 0x05, 0x06, 0x07));
}

TEST(RuntimeBlindLogic, PositionRecomputeRequiresDurationsKnownPositionAndMovement) {
  EXPECT_FALSE(can_recompute_position(0, 10000, 10000, 0.5f));
  EXPECT_FALSE(can_recompute_position(1, 0, 10000, 0.5f));
  EXPECT_FALSE(can_recompute_position(1, 10000, 0, 0.5f));
  EXPECT_FALSE(can_recompute_position(1, 10000, 10000, -1.0f));
  EXPECT_TRUE(can_recompute_position(1, 10000, 10000, 0.5f));
}

TEST(RuntimeBlindLogic, PositionRecomputeClampsToEndpoints) {
  EXPECT_FLOAT_EQ(recompute_position(0.5f, 1, 2500, 10000, 10000), 0.75f);
  EXPECT_FLOAT_EQ(recompute_position(0.5f, -1, 2500, 10000, 10000), 0.25f);
  EXPECT_FLOAT_EQ(recompute_position(0.9f, 1, 2500, 10000, 10000), 1.0f);
  EXPECT_FLOAT_EQ(recompute_position(0.1f, -1, 2500, 10000, 10000), 0.0f);
}
