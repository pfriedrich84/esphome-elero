#include "elero/elero_group_command_logic.h"
#include "elero/elero_command_profile.h"
#include <gtest/gtest.h>

using namespace esphome::elero;
using namespace esphome::elero::group_command_logic;

TEST(GroupCommandLogic, IncrementCounterOnlyAfterAcceptedSubmit) {
  EXPECT_TRUE(should_increment_group_counter(HubSubmitResult::OK));
  EXPECT_FALSE(should_increment_group_counter(HubSubmitResult::QUEUE_FULL));
  EXPECT_FALSE(should_increment_group_counter(HubSubmitResult::FAILED));
}

TEST(GroupCommandLogic, FallbackToMemberQueuesWhenHubDoesNotAcceptCommand) {
  EXPECT_FALSE(should_fallback_to_member_queues(HubSubmitResult::OK));
  EXPECT_TRUE(should_fallback_to_member_queues(HubSubmitResult::QUEUE_FULL));
  EXPECT_TRUE(should_fallback_to_member_queues(HubSubmitResult::FAILED));
}

TEST(GroupCommandLogic, GroupCounterWrapsAndSkipsZero) {
  EXPECT_EQ(next_group_counter(1), 2);
  EXPECT_EQ(next_group_counter(0xFE), 0xFF);
  EXPECT_EQ(next_group_counter(0xFF), 1);
}

TEST(CommandProfile, NativeGroupRequiresSharedRfProfile) {
  BlindCommandProfile first{};
  first.blind_address = 0x111111;
  first.remote_address = 0xf0d008;
  first.channel = 4;
  first.pck_inf[0] = 0x6a;
  first.pck_inf[1] = 0x00;
  first.hop = 0x0a;
  first.payload_1 = 0x00;
  first.payload_2 = 0x04;

  BlindCommandProfile second = first;
  second.blind_address = 0x222222;
  EXPECT_TRUE(command_profile::can_share_native_group(first, second));

  second.channel = 5;
  EXPECT_FALSE(command_profile::can_share_native_group(first, second));
  second = first;
  second.payload_2 = 0x05;
  EXPECT_FALSE(command_profile::can_share_native_group(first, second));
}
