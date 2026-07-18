#include "elero/elero_group_delivery_policy.h"
#include <gtest/gtest.h>

using namespace esphome::elero;
using namespace esphome::elero::group_delivery_policy;

static CommandDeliveryConfig group_config(uint32_t address) {
  CommandDeliveryConfig config{};
  config.profile.blind_address = address;
  config.profile.remote_address = 0xF0D008;
  config.profile.channel = 2;
  config.profile.pck_inf[0] = 0x6A;
  config.profile.hop = 0x0A;
  config.profile.payload_2 = 2;
  return config;
}

TEST(GroupDeliveryPolicy, IncompatibleProfilesUseMemberFallback) {
  auto first = group_config(0x111111);
  auto second = group_config(0x222222);
  second.profile.channel++;
  EXPECT_EQ(initial_route({first, second}, {CommandIntentKind::OPEN, 0}), Route::MEMBERS);
}

TEST(GroupDeliveryPolicy, CompatibleProfilesUseNativeDeliveryIncludingStop) {
  auto first = group_config(0x111111);
  auto second = group_config(0x222222);
  EXPECT_EQ(initial_route({first, second}, {CommandIntentKind::STOP, 0}), Route::NATIVE);
}

TEST(GroupDeliveryPolicy, ZeroAcceptedNativeFailureFallsBack) {
  DeliveryOutcome outcome{};
  outcome.event = DeliveryEvent::DROPPED;
  outcome.intent = {CommandIntentKind::CLOSE, 0};
  outcome.had_partial_delivery = false;
  EXPECT_EQ(after_native_outcome(outcome), Route::MEMBERS);
}

TEST(GroupDeliveryPolicy, PartialNativeFailureNeverFansOut) {
  DeliveryOutcome outcome{};
  outcome.event = DeliveryEvent::DROPPED;
  outcome.intent = {CommandIntentKind::STOP, 0};
  outcome.had_partial_delivery = true;
  EXPECT_EQ(after_native_outcome(outcome), Route::NONE);
}

TEST(GroupDeliveryPolicy, LocalNativeQueueRejectionPreservesNativeLaneOrder) {
  EXPECT_TRUE(reject_native_submit_without_fanout(IntentSubmitResult::REJECTED));
  EXPECT_FALSE(reject_native_submit_without_fanout(IntentSubmitResult::ACCEPTED));
  EXPECT_FALSE(reject_native_submit_without_fanout(IntentSubmitResult::COALESCED));
}
