#include "elero/elero_group_position_logic.h"
#include <gtest/gtest.h>
#include <limits>

using namespace esphome::elero;
using namespace esphome::elero::group_position_logic;

TEST(GroupPositionLogic, SupportsPositionOnlyWhenAllMembersCalibrated) {
  MemberState members[] = {{0.2f, 20000, 22000, true}, {0.3f, 21000, 23000, true}};
  EXPECT_TRUE(supports_group_position(members, 2));
  members[1].close_duration_ms = 0;
  EXPECT_FALSE(supports_group_position(members, 2));
  EXPECT_FALSE(supports_group_position(members, 1));
}

TEST(GroupPositionLogic, RejectsUnknownIntermediatePositions) {
  MemberState members[] = {{0.2f, 20000, 22000, true},
                           {std::numeric_limits<float>::quiet_NaN(), 20000, 22000, true}};
  const auto plan = plan_intermediate_target(members, 2, 0.75f, true);
  EXPECT_EQ(plan.route, Route::REJECT);
}

TEST(GroupPositionLogic, RejectsFiniteButUntrustedPositions) {
  MemberState members[] = {{0.5f, 20000, 22000, false}, {0.4f, 20000, 22000, true}};
  EXPECT_EQ(plan_intermediate_target(members, 2, 0.75f, true).route, Route::REJECT);
}

TEST(GroupPositionLogic, UsesNativeStartWhenAllMembersNeedSameDirection) {
  MemberState members[] = {{0.1f, 20000, 22000, true}, {0.4f, 26000, 30000, true}};
  const auto plan = plan_intermediate_target(members, 2, 0.75f, true);
  EXPECT_EQ(plan.route, Route::NATIVE_OPEN);
  EXPECT_EQ(plan.native_direction, CommandIntentKind::OPEN);
}

TEST(GroupPositionLogic, FallsBackToMemberTargetsForMixedDirectionsOrIncompatibleProfiles) {
  MemberState mixed[] = {{0.1f, 20000, 22000, true}, {0.9f, 26000, 30000, true}};
  EXPECT_EQ(plan_intermediate_target(mixed, 2, 0.75f, true).route, Route::MEMBER_TARGETS);

  MemberState same_direction[] = {{0.1f, 20000, 22000, true}, {0.4f, 26000, 30000, true}};
  EXPECT_EQ(plan_intermediate_target(same_direction, 2, 0.75f, false).route,
            Route::MEMBER_TARGETS);
}

TEST(GroupPositionLogic, DoesNotUseNativeStartWhenAnyMemberAlreadyAtTarget) {
  MemberState members[] = {{0.75f, 20000, 22000, true}, {0.4f, 26000, 30000, true}};
  EXPECT_EQ(plan_intermediate_target(members, 2, 0.75f, true).route, Route::MEMBER_TARGETS);
}

TEST(GroupPositionLogic, CoalescesWhenAllMembersAlreadyAtTarget) {
  MemberState members[] = {{0.75f, 20000, 22000, true}, {0.754f, 26000, 30000, true}};
  EXPECT_EQ(plan_intermediate_target(members, 2, 0.75f, true).route, Route::ALREADY_AT_TARGET);
}
