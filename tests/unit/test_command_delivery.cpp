#include "elero/elero_profile_delivery_coordinator.h"
#include "elero/elero_timed_action.h"
#include "elero/elero_tx_admission.h"
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>

using namespace esphome::elero;

class AttachedDelivery {
 public:
  explicit AttachedDelivery(const CommandDeliveryConfig &config)
      : coordinator(DeliveryProfileKey::from(config.profile)), delivery(config) {
    EXPECT_TRUE(coordinator.attach(&delivery));
  }

  IntentSubmitResult submit(const CommandIntent &intent, uint32_t submitted_at_ms = 0,
                            bool deferred = false) {
    return delivery.submit(intent, submitted_at_ms, deferred);
  }
  IntentSubmitResult submit_batch(std::initializer_list<CommandIntent> intents) {
    return delivery.submit_batch(intents.begin(), intents.size(), 0);
  }
  DeliveryOutcome advance(uint32_t now, uint32_t delay, uint8_t repeats,
                          const ProfileDeliveryCoordinator::SubmitCallback &submitter) {
    return coordinator.advance(now, delay, repeats, submitter);
  }
  DeliveryOutcome complete(uint32_t transaction_id, bool success, uint32_t completed_at_ms) {
    return coordinator.complete(transaction_id, success, completed_at_ms);
  }
  size_t size() const { return delivery.size(); }
  bool empty() const { return delivery.empty(); }
  uint8_t counter() const { return coordinator.counter(); }
  void release_deferred() { delivery.release_deferred(); }

  ProfileDeliveryCoordinator coordinator;
  CommandIntentDelivery delivery;
};

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

TEST(RadioTxAdmission, AllowsOnlyOneRadioWideTransaction) {
  RadioTxAdmission admission;
  EXPECT_FALSE(admission.busy());
  EXPECT_FALSE(admission.try_reserve(0));
  EXPECT_TRUE(admission.try_reserve(10));
  EXPECT_TRUE(admission.busy());
  EXPECT_EQ(admission.transaction_id(), 10u);
  EXPECT_FALSE(admission.try_reserve(11));
  EXPECT_FALSE(admission.release(11));
  EXPECT_EQ(admission.transaction_id(), 10u);
  EXPECT_TRUE(admission.release(10));
  EXPECT_FALSE(admission.busy());
  EXPECT_TRUE(admission.try_reserve(11));
  EXPECT_TRUE(delivery_profile_is_eligible(false, false));
  EXPECT_FALSE(delivery_profile_is_eligible(true, false));
  EXPECT_TRUE(delivery_profile_is_eligible(true, true));
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
  AttachedDelivery delivery(config());
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

TEST(CommandDelivery, AsyncSubmissionCompletesOnlyAfterActualRadioOutcome) {
  AttachedDelivery delivery(config());
  ASSERT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}), IntentSubmitResult::ACCEPTED);
  int submissions = 0;
  auto submit = [&](const t_elero_command &, bool) {
    submissions++;
    return PacketSubmission::queued(42);
  };

  auto queued = delivery.advance(10, 0, 1, submit);
  EXPECT_EQ(queued.event, DeliveryEvent::WAITING);
  EXPECT_FALSE(delivery_packet_was_accepted(queued.event));
  EXPECT_EQ(delivery.size(), 1u);
  EXPECT_EQ(delivery.counter(), 1);

  EXPECT_EQ(delivery.advance(20, 0, 1, submit).event, DeliveryEvent::WAITING);
  EXPECT_EQ(submissions, 1);

  auto completed = delivery.complete(42, true, 25);
  EXPECT_EQ(completed.event, DeliveryEvent::COMPLETED);
  EXPECT_EQ(completed.transmitted_at_ms, 25u);
  EXPECT_TRUE(completed.first_transmission);
  EXPECT_TRUE(delivery_packet_was_accepted(completed.event));
  EXPECT_TRUE(delivery.empty());
  EXPECT_EQ(delivery.counter(), 2);
}

TEST(CommandDelivery, PendingMovementCompletesBeforeReplacingStop) {
  AttachedDelivery delivery(config());
  ASSERT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}), IntentSubmitResult::ACCEPTED);
  t_elero_command open{};
  EXPECT_EQ(delivery.advance(10, 0, 3, [&](const auto &packet, bool priority) {
              EXPECT_FALSE(priority);
              open = packet;
              return PacketSubmission::queued(10);
            }).event,
            DeliveryEvent::WAITING);

  ASSERT_EQ(delivery.submit({CommandIntentKind::STOP, 0}, 11), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.counter(), 1);
  EXPECT_EQ(delivery.complete(10, true, 20).event, DeliveryEvent::COMPLETED);
  EXPECT_EQ(open.counter, 1);
  EXPECT_EQ(delivery.counter(), 2);

  t_elero_command stop{};
  EXPECT_EQ(delivery.advance(21, 0, 1, [&](const auto &packet, bool priority) {
              EXPECT_TRUE(priority);
              stop = packet;
              return PacketSubmission::queued(11);
            }).event,
            DeliveryEvent::WAITING);
  EXPECT_EQ(stop.counter, 2);
  EXPECT_EQ(stop.payload[4], config().mapping.stop);
  EXPECT_EQ(delivery.complete(11, true, 25).event, DeliveryEvent::COMPLETED);
  EXPECT_TRUE(delivery.empty());
}

TEST(CommandDelivery, StopAfterFailedPendingRepeatRetiresUsedCounter) {
  AttachedDelivery delivery(config());
  ASSERT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.advance(1, 0, 3, [](const auto &, bool) {
              return PacketSubmission::queued(20);
            }).event,
            DeliveryEvent::WAITING);
  EXPECT_EQ(delivery.complete(20, true, 2).event, DeliveryEvent::PACKET_ACCEPTED);
  EXPECT_EQ(delivery.advance(3, 0, 3, [](const auto &, bool) {
              return PacketSubmission::queued(21);
            }).event,
            DeliveryEvent::WAITING);

  CommandIntentDelivery::AtomicIntentTarget target{
      &delivery.delivery, {CommandIntentKind::STOP, 0}, false};
  ASSERT_EQ(CommandIntentDelivery::submit_atomic(&target, 1, 4), IntentSubmitResult::ACCEPTED);
  auto failed = delivery.complete(21, false, 5);
  EXPECT_EQ(failed.event, DeliveryEvent::DROPPED);
  EXPECT_TRUE(failed.had_partial_delivery);
  EXPECT_EQ(delivery.counter(), 2);

  t_elero_command stop{};
  EXPECT_EQ(delivery.advance(6, 0, 1, [&](const auto &packet, bool priority) {
              EXPECT_TRUE(priority);
              stop = packet;
              return PacketSubmission::queued(22);
            }).event,
            DeliveryEvent::WAITING);
  EXPECT_EQ(stop.counter, 2);
  EXPECT_EQ(delivery.complete(22, true, 7).event, DeliveryEvent::COMPLETED);
}

TEST(CommandDelivery, AsyncRadioFailureKeepsIntentForRetry) {
  AttachedDelivery delivery(config());
  ASSERT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.advance(10, 0, 1, [](const auto &, bool) {
              return PacketSubmission::queued(7);
            }).event,
            DeliveryEvent::WAITING);

  auto failed = delivery.complete(7, false, 20);
  EXPECT_EQ(failed.event, DeliveryEvent::RETRY_SCHEDULED);
  EXPECT_EQ(delivery.size(), 1u);
  EXPECT_EQ(delivery.counter(), 1);

  EXPECT_EQ(delivery.advance(41, 0, 1, [](const auto &, bool) {
              return PacketSubmission::queued(8);
            }).event,
            DeliveryEvent::WAITING);
  auto completed = delivery.complete(8, true, 45);
  EXPECT_EQ(completed.event, DeliveryEvent::COMPLETED);
  EXPECT_EQ(completed.transmitted_at_ms, 45u);
  EXPECT_TRUE(delivery.empty());
  EXPECT_EQ(delivery.counter(), 2);
}

TEST(CommandDelivery, CoalescesChecksAndUnsentTargetChanges) {
  AttachedDelivery delivery(config());
  EXPECT_EQ(delivery.submit({CommandIntentKind::CHECK, 0}), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.submit({CommandIntentKind::CHECK, 0}), IntentSubmitResult::COALESCED);
  EXPECT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.submit({CommandIntentKind::CLOSE, 0}), IntentSubmitResult::COALESCED);
  EXPECT_EQ(delivery.size(), 2u);
}

TEST(CommandDelivery, CoalescingDoesNotCrossOrderedIntentBarriers) {
  AttachedDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  delivery.submit(CommandIntent::custom(0x77));
  delivery.submit({CommandIntentKind::CLOSE, 0});
  delivery.submit({CommandIntentKind::TILT, 0});
  delivery.submit({CommandIntentKind::OPEN, 0});

  std::vector<uint8_t> bytes;
  auto submit = [&](const t_elero_command &packet, bool) {
    bytes.push_back(packet.payload[4]);
    return SendResult::OK;
  };
  for (uint32_t now = 1; !delivery.empty(); now++)
    delivery.advance(now, 0, 1, submit);
  EXPECT_EQ(bytes, (std::vector<uint8_t>{0x21, 0x77, 0x41, 0x24, 0x21}));
}

TEST(CommandDelivery, NeverRewritesAnActiveTarget) {
  AttachedDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  EXPECT_EQ(delivery.advance(1, 0, 1, [](const auto &, bool) { return SendResult::FAILED; }).event,
            DeliveryEvent::RETRY_SCHEDULED);
  EXPECT_EQ(delivery.submit({CommandIntentKind::CLOSE, 0}), IntentSubmitResult::ACCEPTED);

  std::vector<uint8_t> bytes;
  auto submit = [&](const auto &packet, bool) {
    bytes.push_back(packet.payload[4]);
    return SendResult::OK;
  };
  EXPECT_EQ(delivery.advance(22, 0, 1, submit).event, DeliveryEvent::COMPLETED);
  EXPECT_EQ(delivery.advance(23, 0, 1, submit).event, DeliveryEvent::COMPLETED);
  EXPECT_EQ(bytes, (std::vector<uint8_t>{0x21, 0x41}));
}

TEST(CommandDelivery, NeverRewritesPartiallyDeliveredTarget) {
  AttachedDelivery delivery(config());
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

TEST(CommandDelivery, StopIsNotAcceptedWhilePriorityQueueIsCongested) {
  AttachedDelivery delivery(config());
  delivery.submit({CommandIntentKind::STOP, 0});
  auto queued = delivery.advance(1, 0, 1, [](const auto &, bool priority) {
    EXPECT_TRUE(priority);
    return SendResult::QUEUE_FULL;
  });
  EXPECT_FALSE(delivery_packet_was_accepted(queued.event));
  EXPECT_EQ(delivery.size(), 1u);

  auto accepted = delivery.advance(2, 0, 1, [](const auto &, bool) { return SendResult::OK; });
  EXPECT_TRUE(delivery_packet_was_accepted(accepted.event));
}

TEST(CommandDelivery, StopPreemptsAndRetiresPartialCounter) {
  AttachedDelivery delivery(config());
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

TEST(CommandDelivery, AcceptedRepeatsRefreshStaleProgress) {
  AttachedDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  auto ok = [](const auto &, bool) { return SendResult::OK; };
  EXPECT_EQ(delivery.advance(1, 0, 3, ok).event, DeliveryEvent::PACKET_ACCEPTED);
  EXPECT_EQ(delivery.advance(ELERO_COMMAND_QUEUE_MAX_AGE_MS, 0, 3, ok).event,
            DeliveryEvent::PACKET_ACCEPTED);
  EXPECT_EQ(delivery.advance(2 * ELERO_COMMAND_QUEUE_MAX_AGE_MS - 1, 0, 3, ok).event,
            DeliveryEvent::COMPLETED);
}

TEST(CommandDelivery, QueueFullPreservesIntentCounterAndFailureBudget) {
  AttachedDelivery delivery(config());
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
  AttachedDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  DeliveryOutcome outcome;
  for (uint32_t now : {1u, 22u, 63u, 144u})
    outcome = delivery.advance(now, 0, 1, [](const auto &, bool) { return SendResult::FAILED; });
  EXPECT_EQ(outcome.event, DeliveryEvent::DROPPED);
  EXPECT_FALSE(outcome.had_partial_delivery);
  EXPECT_EQ(delivery.counter(), 1);
}

TEST(CommandDelivery, FinalFailureAfterPartialRepeatRetiresCounter) {
  AttachedDelivery delivery(config());
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
  AttachedDelivery delivery(config());
  delivery.submit({CommandIntentKind::OPEN, 0});
  delivery.advance(1, 0, 2, [](const auto &, bool) { return SendResult::OK; });
  auto outcome = delivery.advance(ELERO_COMMAND_QUEUE_MAX_AGE_MS + 2, 0, 2,
                                  [](const auto &, bool) { return SendResult::OK; });
  EXPECT_EQ(outcome.event, DeliveryEvent::STALE_CLEARED);
  EXPECT_TRUE(outcome.had_partial_delivery);
  EXPECT_EQ(delivery.counter(), 2);
  EXPECT_TRUE(delivery.empty());
}

TEST(CommandDelivery, DetachingActiveLaneRetiresPartiallyUsedCounter) {
  auto delivery_config = config();
  ProfileDeliveryCoordinator coordinator(DeliveryProfileKey::from(delivery_config.profile));
  CommandIntentDelivery delivery(delivery_config);
  ASSERT_TRUE(coordinator.attach(&delivery));
  ASSERT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}, 0), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(coordinator.advance(1, 0, 2, [](const auto &, bool) { return SendResult::OK; }).event,
            DeliveryEvent::PACKET_ACCEPTED);

  EXPECT_TRUE(coordinator.detach(&delivery));
  EXPECT_EQ(coordinator.counter(), 2);
  EXPECT_TRUE(delivery.empty());
}

TEST(CommandDelivery, DiscardingActiveCheckRetiresPartiallyUsedCounter) {
  auto delivery_config = config();
  ProfileDeliveryCoordinator coordinator(DeliveryProfileKey::from(delivery_config.profile));
  CommandIntentDelivery delivery(delivery_config);
  ASSERT_TRUE(coordinator.attach(&delivery));
  ASSERT_EQ(delivery.submit({CommandIntentKind::CHECK, 0}, 0), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(coordinator.advance(1, 0, 2, [](const auto &, bool) { return SendResult::OK; }).event,
            DeliveryEvent::PACKET_ACCEPTED);

  delivery.discard_checks();
  EXPECT_EQ(coordinator.counter(), 2);
  EXPECT_TRUE(delivery.empty());
}

TEST(CommandDelivery, RejectsNewestWhenCapacityCannotBeCoalesced) {
  AttachedDelivery delivery(config());
  for (uint8_t i = 0; i < ELERO_MAX_COMMAND_QUEUE; i++)
    EXPECT_EQ(delivery.submit(CommandIntent::custom(i)), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.submit(CommandIntent::custom(0xFE)), IntentSubmitResult::REJECTED);
}

TEST(CommandDelivery, BatchSubmissionRollsBackWhenWholeSequenceDoesNotFit) {
  AttachedDelivery delivery(config());
  for (uint8_t i = 0; i < ELERO_MAX_COMMAND_QUEUE - 1; i++)
    ASSERT_EQ(delivery.submit(CommandIntent::custom(i)), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.submit_batch({{CommandIntentKind::ON, 0},
                                   {CommandIntentKind::DIM_DOWN, 0}}),
            IntentSubmitResult::REJECTED);
  EXPECT_EQ(delivery.size(), ELERO_MAX_COMMAND_QUEUE - 1);
}

TEST(CommandDelivery, BatchSubmissionPreservesMultiIntentOrder) {
  AttachedDelivery delivery(config());
  ASSERT_TRUE(intent_was_accepted(delivery.submit_batch({{CommandIntentKind::ON, 0},
                                                          {CommandIntentKind::DIM_DOWN, 0}})));
  std::vector<uint8_t> bytes;
  auto submit = [&](const t_elero_command &packet, bool) {
    bytes.push_back(packet.payload[4]);
    return SendResult::OK;
  };
  delivery.advance(1, 0, 1, submit);
  delivery.advance(2, 0, 1, submit);
  EXPECT_EQ(bytes, (std::vector<uint8_t>{0x20, 0x40}));
}

TEST(CommandDelivery, ButtonBytesPreserveStopUrgencyAndSemanticDeferral) {
  const auto mapping = config().mapping;
  EXPECT_EQ(cover_intent_for_command_byte(mapping, mapping.stop).kind, CommandIntentKind::STOP);
  EXPECT_EQ(cover_intent_for_command_byte(mapping, mapping.open).kind, CommandIntentKind::OPEN);
  EXPECT_EQ(light_intent_for_command_byte(mapping, mapping.stop).kind, CommandIntentKind::STOP);
  EXPECT_EQ(light_intent_for_command_byte(mapping, mapping.on).kind, CommandIntentKind::ON);
  const auto cover_custom = cover_intent_for_command_byte(mapping, 0x77);
  EXPECT_EQ(cover_custom.kind, CommandIntentKind::CUSTOM);
  EXPECT_EQ(cover_custom.custom_byte, 0x77);
  const auto light_custom = light_intent_for_command_byte(mapping, 0x77);
  EXPECT_EQ(light_custom.kind, CommandIntentKind::CUSTOM);
  EXPECT_EQ(light_custom.custom_byte, 0x77);
}

TEST(CommandDelivery, DeferredIntentDoesNotTransmitUntilReleasedAfterStop) {
  AttachedDelivery delivery(config());
  ASSERT_EQ(delivery.submit({CommandIntentKind::STOP, 0}), IntentSubmitResult::ACCEPTED);
  ASSERT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}, 0, true), IntentSubmitResult::ACCEPTED);

  std::vector<uint8_t> bytes;
  auto submit = [&](const t_elero_command &packet, bool) {
    bytes.push_back(packet.payload[4]);
    return SendResult::OK;
  };
  delivery.advance(1, 0, 1, submit);
  ASSERT_EQ(bytes, (std::vector<uint8_t>{0x10}));
  EXPECT_EQ(delivery.size(), 1u);
  EXPECT_EQ(delivery.advance(2, 0, 1, submit).event, DeliveryEvent::IDLE);

  delivery.release_deferred();
  delivery.advance(3, 0, 1, submit);
  EXPECT_EQ(bytes, (std::vector<uint8_t>{0x10, 0x21}));
  EXPECT_TRUE(delivery.empty());
}

TEST(CommandDelivery, DeferredIntentsCountAgainstTheSameBound) {
  AttachedDelivery delivery(config());
  for (uint8_t i = 0; i < ELERO_MAX_COMMAND_QUEUE; i++)
    ASSERT_EQ(delivery.submit(CommandIntent::custom(i), 0, true), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}, 0, true), IntentSubmitResult::REJECTED);
  EXPECT_EQ(delivery.size(), ELERO_MAX_COMMAND_QUEUE);
}

TEST(ProfileCoordinator, RadioWideAdmissionKeepsStopBehindInflightThenSelectsItNext) {
  auto normal_config = config();
  auto urgent_config = config();
  urgent_config.profile.remote_address++;
  AttachedDelivery normal(normal_config);
  AttachedDelivery urgent(urgent_config);
  RadioTxAdmission admission;

  ASSERT_EQ(normal.submit({CommandIntentKind::OPEN, 0}), IntentSubmitResult::ACCEPTED);
  t_elero_command first{};
  EXPECT_EQ(normal.advance(1, 0, 1, [&](const auto &packet, bool priority) {
              EXPECT_FALSE(priority);
              EXPECT_TRUE(admission.try_reserve(100));
              first = packet;
              return PacketSubmission::queued(100);
            }).event,
            DeliveryEvent::WAITING);
  EXPECT_EQ(first.payload[4], normal_config.mapping.open);

  ASSERT_EQ(urgent.submit({CommandIntentKind::STOP, 0}), IntentSubmitResult::ACCEPTED);
  EXPECT_TRUE(urgent.coordinator.has_urgent(2));
  EXPECT_TRUE(admission.busy());
  EXPECT_FALSE(admission.try_reserve(101));

  EXPECT_EQ(normal.complete(100, true, 3).event, DeliveryEvent::COMPLETED);
  EXPECT_TRUE(admission.release(100));

  t_elero_command second{};
  EXPECT_TRUE(delivery_profile_is_eligible(true, urgent.coordinator.has_urgent(4)));
  EXPECT_EQ(urgent.advance(4, 0, 1, [&](const auto &packet, bool priority) {
              EXPECT_TRUE(priority);
              EXPECT_TRUE(admission.try_reserve(101));
              second = packet;
              return PacketSubmission::queued(101);
            }).event,
            DeliveryEvent::WAITING);
  EXPECT_EQ(second.payload[4], urgent_config.mapping.stop);
  EXPECT_EQ(urgent.complete(101, true, 5).event, DeliveryEvent::COMPLETED);
  EXPECT_TRUE(admission.release(101));
}

TEST(ProfileCoordinator, RadioWideSelectionPrefersStopAcrossProfiles) {
  auto normal_config = config();
  auto urgent_config = config();
  urgent_config.profile.remote_address++;
  AttachedDelivery normal(normal_config);
  AttachedDelivery urgent(urgent_config);
  ASSERT_EQ(normal.submit({CommandIntentKind::OPEN, 0}), IntentSubmitResult::ACCEPTED);
  ASSERT_EQ(urgent.submit({CommandIntentKind::STOP, 0}), IntentSubmitResult::ACCEPTED);

  const bool urgent_waiting = normal.coordinator.has_urgent(1) ||
                              urgent.coordinator.has_urgent(1);
  EXPECT_TRUE(urgent_waiting);
  EXPECT_FALSE(delivery_profile_is_eligible(urgent_waiting,
                                            normal.coordinator.has_urgent(1)));
  EXPECT_TRUE(delivery_profile_is_eligible(urgent_waiting,
                                           urgent.coordinator.has_urgent(1)));
}

TEST(ProfileCoordinator, ReportsUrgentWorkForRadioWideSelection) {
  AttachedDelivery delivery(config());
  EXPECT_FALSE(delivery.coordinator.has_urgent(1));
  ASSERT_EQ(delivery.submit({CommandIntentKind::OPEN, 0}), IntentSubmitResult::ACCEPTED);
  EXPECT_FALSE(delivery.coordinator.has_urgent(1));
  ASSERT_EQ(delivery.submit({CommandIntentKind::STOP, 0}), IntentSubmitResult::ACCEPTED);
  EXPECT_TRUE(delivery.coordinator.has_urgent(1));
}

TEST(ProfileCoordinator, SerializesLanesWithOneRollingCounter) {
  auto first_config = config();
  first_config.destination_count = 1;
  auto second_config = first_config;
  second_config.profile.blind_address = 0x222222;
  second_config.destinations[0] = 0x222222;
  ProfileDeliveryCoordinator coordinator(DeliveryProfileKey::from(first_config.profile));
  CommandIntentDelivery first(first_config);
  CommandIntentDelivery second(second_config);
  ASSERT_TRUE(coordinator.attach(&first));
  ASSERT_TRUE(coordinator.attach(&second));
  ASSERT_EQ(first.submit({CommandIntentKind::OPEN, 0}, 0), IntentSubmitResult::ACCEPTED);
  ASSERT_EQ(second.submit({CommandIntentKind::CLOSE, 0}, 0), IntentSubmitResult::ACCEPTED);

  std::vector<std::pair<uint32_t, uint8_t>> packets;
  auto submit = [&](const t_elero_command &packet, bool) {
    packets.push_back({packet.blind_addr, packet.counter});
    return SendResult::OK;
  };
  coordinator.advance(1, 0, 1, submit);
  coordinator.advance(2, 0, 1, submit);
  EXPECT_EQ(packets, (std::vector<std::pair<uint32_t, uint8_t>>{{0x111111, 1}, {0x222222, 2}}));
  EXPECT_EQ(coordinator.counter(), 3);
}

TEST(ProfileCoordinator, QueueAgeIncludesTimeWaitingBehindAnotherLane) {
  auto first_config = config();
  first_config.destination_count = 1;
  auto second_config = first_config;
  second_config.profile.blind_address = 0x222222;
  second_config.destinations[0] = 0x222222;
  ProfileDeliveryCoordinator coordinator(DeliveryProfileKey::from(first_config.profile));
  CommandIntentDelivery first(first_config);
  CommandIntentDelivery second(second_config);
  ASSERT_TRUE(coordinator.attach(&first));
  ASSERT_TRUE(coordinator.attach(&second));
  ASSERT_EQ(first.submit({CommandIntentKind::OPEN, 0}, 0), IntentSubmitResult::ACCEPTED);
  ASSERT_EQ(second.submit({CommandIntentKind::CLOSE, 0}, 1), IntentSubmitResult::ACCEPTED);

  auto ok = [](const auto &, bool) { return SendResult::OK; };
  EXPECT_EQ(coordinator.advance(1, 0, 4, ok).event, DeliveryEvent::PACKET_ACCEPTED);
  EXPECT_EQ(coordinator.advance(10001, 0, 4, ok).event, DeliveryEvent::PACKET_ACCEPTED);
  EXPECT_EQ(coordinator.advance(20001, 0, 4, ok).event, DeliveryEvent::PACKET_ACCEPTED);
  EXPECT_EQ(coordinator.advance(30001, 0, 4, ok).event, DeliveryEvent::COMPLETED);

  auto stale = coordinator.advance(30002, 0, 1, ok);
  EXPECT_EQ(stale.event, DeliveryEvent::STALE_CLEARED);
  EXPECT_EQ(stale.intent.kind, CommandIntentKind::CLOSE);
  EXPECT_TRUE(second.empty());
}

TEST(ProfileCoordinator, AtomicMemberSubmissionRollsBackEveryLane) {
  auto first_config = config();
  first_config.destination_count = 1;
  auto second_config = first_config;
  second_config.profile.remote_address++;
  ProfileDeliveryCoordinator first_coordinator(DeliveryProfileKey::from(first_config.profile));
  ProfileDeliveryCoordinator second_coordinator(DeliveryProfileKey::from(second_config.profile));
  CommandIntentDelivery first(first_config);
  CommandIntentDelivery second(second_config);
  ASSERT_TRUE(first_coordinator.attach(&first));
  ASSERT_TRUE(second_coordinator.attach(&second));
  for (uint8_t i = 0; i < ELERO_MAX_COMMAND_QUEUE; i++)
    ASSERT_EQ(second.submit(CommandIntent::custom(i), 0), IntentSubmitResult::ACCEPTED);
  CommandIntentDelivery::AtomicIntentTarget targets[] = {
      {&first, {CommandIntentKind::OPEN, 0}, false},
      {&second, {CommandIntentKind::OPEN, 0}, false},
  };
  EXPECT_EQ(CommandIntentDelivery::submit_atomic(targets, 2, 0), IntentSubmitResult::REJECTED);
  EXPECT_TRUE(first.empty());
  EXPECT_EQ(second.size(), ELERO_MAX_COMMAND_QUEUE);
}

TEST(ProfileCoordinator, NativeFailureFallsBackInOrderBeforeAnyAcceptance) {
  auto native_config = config();
  auto first_config = native_config;
  first_config.destination_count = 1;
  first_config.destinations[0] = 0x111111;
  auto second_config = first_config;
  second_config.profile.blind_address = 0x222222;
  second_config.destinations[0] = 0x222222;
  ProfileDeliveryCoordinator coordinator(DeliveryProfileKey::from(native_config.profile));
  CommandIntentDelivery native(native_config);
  CommandIntentDelivery first(first_config);
  CommandIntentDelivery second(second_config);
  ASSERT_TRUE(coordinator.attach(&native));
  ASSERT_TRUE(coordinator.attach(&first));
  ASSERT_TRUE(coordinator.attach(&second));
  CommandIntentDelivery *members[] = {&first, &second};
  ASSERT_TRUE(native.set_native_fallback(members, 2));
  // Fallback owns immutable packet configurations rather than member-lane
  // pointers that can dangle after detach or destruction.
  ASSERT_TRUE(coordinator.detach(&first));
  ASSERT_TRUE(coordinator.detach(&second));
  ASSERT_EQ(native.submit({CommandIntentKind::OPEN, 0}, 0), IntentSubmitResult::ACCEPTED);

  std::vector<uint8_t> destination_counts;
  auto submit = [&](const t_elero_command &packet, bool) {
    destination_counts.push_back(packet.num_dests);
    return packet.num_dests == 2 ? SendResult::FAILED : SendResult::OK;
  };
  DeliveryOutcome outcome;
  for (uint32_t now : {1u, 22u, 63u, 144u})
    outcome = coordinator.advance(now, 0, 1, submit);
  EXPECT_EQ(outcome.event, DeliveryEvent::FALLBACK_STARTED);
  auto first_fallback = coordinator.advance(145, 0, 1, submit);
  EXPECT_TRUE(first_fallback.fallback_member);
  EXPECT_EQ(first_fallback.fallback_member_index, 0);
  auto second_fallback = coordinator.advance(146, 0, 1, submit);
  EXPECT_TRUE(second_fallback.fallback_member);
  EXPECT_EQ(second_fallback.fallback_member_index, 1);
  EXPECT_EQ(destination_counts.back(), 1);
  EXPECT_TRUE(native.empty());
  EXPECT_EQ(coordinator.counter(), 3);
}

TEST(ProfileCoordinator, ActiveFallbackKeepsMemberIntentBeforeNewTarget) {
  auto native_config = config();
  auto first_config = native_config;
  first_config.destination_count = 1;
  first_config.destinations[0] = 0x111111;
  auto second_config = first_config;
  second_config.profile.blind_address = 0x222222;
  second_config.destinations[0] = 0x222222;
  ProfileDeliveryCoordinator coordinator(DeliveryProfileKey::from(native_config.profile));
  CommandIntentDelivery native(native_config);
  CommandIntentDelivery first(first_config);
  CommandIntentDelivery second(second_config);
  ASSERT_TRUE(coordinator.attach(&native));
  ASSERT_TRUE(coordinator.attach(&first));
  ASSERT_TRUE(coordinator.attach(&second));
  CommandIntentDelivery *members[] = {&first, &second};
  ASSERT_TRUE(native.set_native_fallback(members, 2));
  ASSERT_EQ(native.submit({CommandIntentKind::OPEN, 0}, 0), IntentSubmitResult::ACCEPTED);

  std::vector<std::pair<uint32_t, uint8_t>> accepted;
  auto submit = [&](const t_elero_command &packet, bool) {
    if (packet.num_dests == 2 && packet.payload[4] == native_config.mapping.open)
      return SendResult::FAILED;
    accepted.push_back({packet.dest_addrs[0], packet.payload[4]});
    return SendResult::OK;
  };
  for (uint32_t now : {1u, 22u, 63u, 144u})
    coordinator.advance(now, 0, 1, submit);
  EXPECT_EQ(coordinator.advance(145, 0, 1, submit).event, DeliveryEvent::PACKET_ACCEPTED);

  ASSERT_EQ(native.submit({CommandIntentKind::CLOSE, 0}, 146), IntentSubmitResult::ACCEPTED);
  EXPECT_EQ(coordinator.advance(146, 0, 1, submit).event, DeliveryEvent::COMPLETED);
  EXPECT_EQ(coordinator.advance(147, 0, 1, submit).event, DeliveryEvent::COMPLETED);
  EXPECT_EQ(accepted, (std::vector<std::pair<uint32_t, uint8_t>>{
                          {0x111111, native_config.mapping.open},
                          {0x222222, native_config.mapping.open},
                          {0x111111, native_config.mapping.close}}));
}

TEST(TimedAction, StartsOnlyOnFirstRelevantAcceptedPacket) {
  DeliveryOutcome outcome{};
  outcome.intent = {CommandIntentKind::DIM_DOWN, 0};
  outcome.event = DeliveryEvent::WAITING;
  EXPECT_FALSE(should_start_timed_action(true, CommandIntentKind::DIM_DOWN, outcome));

  outcome.event = DeliveryEvent::PACKET_ACCEPTED;
  EXPECT_TRUE(should_start_timed_action(true, CommandIntentKind::DIM_DOWN, outcome));
  EXPECT_FALSE(should_start_timed_action(false, CommandIntentKind::DIM_DOWN, outcome));
  EXPECT_FALSE(should_start_timed_action(true, CommandIntentKind::DIM_UP, outcome));

  outcome.event = DeliveryEvent::COMPLETED;
  EXPECT_TRUE(should_start_timed_action(true, CommandIntentKind::DIM_DOWN, outcome));

  outcome.first_transmission = true;
  EXPECT_FALSE(should_reanchor_transmitted_action(CommandIntentKind::DIM_DOWN, outcome));
  EXPECT_TRUE(should_reanchor_transmitted_action(CommandIntentKind::DIM_UP, outcome));
}

TEST(CommandDelivery, SubmissionIsThreadSafe) {
  AttachedDelivery delivery(config());
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
