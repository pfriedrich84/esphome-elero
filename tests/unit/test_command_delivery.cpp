#include "elero/elero_command_delivery.h"
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>

using namespace esphome::elero;

static CommandDeliveryConfig config() {
  CommandDeliveryConfig c{};
  c.profile.blind_address = 0x111111;
  c.profile.remote_address = 0xF0D008;
  c.profile.channel = 4;
  c.profile.pck_inf[0] = 0x6A;
  c.profile.hop = 0x0A;
  c.profile.payload_2 = 4;
  c.mapping.open = 0x21;
  c.mapping.close = 0x41;
  c.destination_count = 2;
  c.destinations[0] = 0x111111;
  c.destinations[1] = 0x222222;
  return c;
}

TEST(CommandProfile, NativeGroupRequiresEverySharedRfField) {
  auto first = config().profile;
  auto second = first;
  second.blind_address = 0x222222;
  EXPECT_TRUE(command_profile::can_share_native_group(first, second));
  second.channel++;
  EXPECT_FALSE(command_profile::can_share_native_group(first, second));
  second = first;
  second.payload_2++;
  EXPECT_FALSE(command_profile::can_share_native_group(first, second));
}

TEST(CommandDelivery, BuildsSemanticMultiDestinationPacketAndCompletesRepeats) {
  CommandIntentDelivery delivery(config());
  ASSERT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}), IntentSubmitResult::ACCEPTED);
  std::vector<t_elero_command> packets;
  auto submit = [&](const t_elero_command &packet, bool priority) {
    EXPECT_FALSE(priority);
    packets.push_back(packet);
    return SendResult::OK;
  };
  EXPECT_EQ(delivery.advance(1, 0, 2, submit).event, DeliveryEvent::PACKET_ACCEPTED);
  EXPECT_EQ(delivery.advance(2, 0, 2, submit).event, DeliveryEvent::COMPLETED);
  ASSERT_EQ(packets.size(), 2u);
  EXPECT_EQ(packets[0].payload[4], 0x21);
  EXPECT_EQ(packets[0].counter, 1);
  EXPECT_EQ(packets[1].counter, 1);
  EXPECT_EQ(packets[0].num_dests, 2);
  EXPECT_EQ(packets[0].dest_addrs[1], 0x222222u);
  EXPECT_EQ(delivery.counter(), 2);
}

TEST(CommandDelivery, CoalescesChecksAndUnsentTargetChanges) {
  CommandIntentDelivery delivery(config());
  EXPECT_EQ(delivery.submit({CommandIntentKind::CHECK, 0}), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.submit({CommandIntentKind::CHECK, 0}), IntentSubmitResult::COALESCED);
  EXPECT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.submit({CommandIntentKind::CLOSE, 0}), IntentSubmitResult::COALESCED);
  EXPECT_EQ(delivery.size(), 2u);
}

TEST(CommandDelivery, NeverRewritesPartiallyDeliveredTarget) {
  CommandIntentDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  EXPECT_EQ(delivery.advance(1, 0, 2, [](const auto &, bool) { return SendResult::OK; }).event,
            DeliveryEvent::PACKET_ACCEPTED);
  EXPECT_EQ(delivery.submit({CommandIntentKind::CLOSE, 0}), IntentSubmitResult::ACCEPTED);
  std::vector<uint8_t> bytes;
  auto submit = [&](const t_elero_command &packet, bool) {
    bytes.push_back(packet.payload[4]);
    return SendResult::OK;
  };
  delivery.advance(2, 0, 2, submit);
  delivery.advance(3, 0, 1, submit);
  ASSERT_EQ(bytes.size(), 2u);
  EXPECT_EQ(bytes[0], 0x21);
  EXPECT_EQ(bytes[1], 0x41);
}

TEST(CommandDelivery, StopPreemptsAndRetiresPartialCounter) {
  CommandIntentDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  delivery.advance(1, 0, 2, [](const auto &, bool) { return SendResult::OK; });
  EXPECT_EQ(delivery.submit({CommandIntentKind::STOP, 0}), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.counter(), 2);
  t_elero_command observed{};
  bool priority = false;
  auto outcome = delivery.advance(2, 1000, 1, [&](const auto &packet, bool urgent) {
    observed = packet;
    priority = urgent;
    return SendResult::OK;
  });
  EXPECT_EQ(outcome.event, DeliveryEvent::COMPLETED);
  EXPECT_TRUE(priority);
  EXPECT_EQ(observed.counter, 2);
  EXPECT_EQ(observed.payload[4], 0x10);
}

TEST(CommandDelivery, QueueFullPreservesIntentCounterAndFailureBudget) {
  CommandIntentDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  for (uint32_t now = 1; now < 10; now++)
    EXPECT_EQ(delivery.advance(now, 0, 1, [](const auto &, bool) { return SendResult::QUEUE_FULL; }).event,
              DeliveryEvent::QUEUE_FULL);
  EXPECT_EQ(delivery.counter(), 1);
  EXPECT_EQ(delivery.size(), 1u);
  EXPECT_EQ(delivery.advance(10, 0, 1, [](const auto &, bool) { return SendResult::OK; }).event,
            DeliveryEvent::COMPLETED);
}

TEST(CommandDelivery, FinalFailureFallsOutWithoutRetiringUnusedCounter) {
  CommandIntentDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  DeliveryOutcome outcome;
  for (uint32_t now : {1u, 22u, 63u, 144u})
    outcome = delivery.advance(now, 0, 1, [](const auto &, bool) { return SendResult::FAILED; });
  EXPECT_EQ(outcome.event, DeliveryEvent::DROPPED);
  EXPECT_FALSE(outcome.had_partial_delivery);
  EXPECT_EQ(delivery.counter(), 1);
}

TEST(CommandDelivery, FinalFailureAfterPartialRepeatRetiresCounter) {
  CommandIntentDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  delivery.advance(1, 0, 2, [](const auto &, bool) { return SendResult::OK; });
  DeliveryOutcome outcome;
  for (uint32_t now : {2u, 23u, 64u, 145u})
    outcome = delivery.advance(now, 0, 2, [](const auto &, bool) { return SendResult::FAILED; });
  EXPECT_EQ(outcome.event, DeliveryEvent::DROPPED);
  EXPECT_TRUE(outcome.had_partial_delivery);
  EXPECT_EQ(delivery.counter(), 2);
}

TEST(CommandDelivery, StaleClearRetiresOnlyPartiallyUsedCounter) {
  CommandIntentDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  delivery.advance(1, 0, 2, [](const auto &, bool) { return SendResult::OK; });
  auto outcome = delivery.advance(ELERO_COMMAND_QUEUE_MAX_AGE_MS + 2, 0, 2,
                                  [](const auto &, bool) { return SendResult::OK; });
  EXPECT_EQ(outcome.event, DeliveryEvent::STALE_CLEARED);
  EXPECT_TRUE(outcome.had_partial_delivery);
  EXPECT_EQ(delivery.counter(), 2);
  EXPECT_TRUE(delivery.empty());
}

TEST(CommandDelivery, RejectsNewestWhenCapacityCannotBeCoalesced) {
  CommandIntentDelivery delivery(config());
  for (uint8_t i = 0; i < ELERO_MAX_COMMAND_QUEUE; i++)
    EXPECT_EQ(delivery.submit(CommandIntent::custom(i)), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.submit(CommandIntent::custom(0xFE)), IntentSubmitResult::REJECTED);
}

TEST(CommandDelivery, SubmissionIsThreadSafe) {
  CommandIntentDelivery delivery(config());
  std::atomic<int> accepted{0};
  std::vector<std::thread> threads;
  for (int i = 0; i < 8; i++) {
    threads.emplace_back([&] {
      if (intent_was_accepted(delivery.submit({CommandIntentKind::CHECK, 0})))
        accepted++;
    });
  }
  for (auto &thread : threads) thread.join();
  EXPECT_EQ(accepted.load(), 8);
  EXPECT_EQ(delivery.size(), 1u);
}
