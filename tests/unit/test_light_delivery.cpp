#include "EleroLight.h"
#include <gtest/gtest.h>
using namespace esphome;
using namespace esphome::elero;

class LightDeliveryTest : public ::testing::Test {
 protected:
  Elero hub;
  EleroLight light;
  light::LightState state;
  void SetUp() override {
    test_now = 1000;
    light.set_elero_parent(&hub);
    light.set_blind_address(0xe99b2b);
    light.set_remote_address(0x458130);
    light.set_channel(3);
    light.set_pckinf_1(0x6a);
    light.set_assumed_state(true);
    light.setup();
    state.output = &light;
  }
  void request(bool on) {
    state.current_values.on = on;
    light.write_state(&state);
  }
};

TEST_F(LightDeliveryTest, RequestAndLocalTxDoNotInventRawReceiverState) {
  EXPECT_EQ(light.get_last_state_raw(), ELERO_STATE_UNKNOWN);
  request(true);
  EXPECT_TRUE(light.get_is_on()); // explicitly assumed after admission
  EXPECT_TRUE(hub.packets.empty()); // requested != transmitted
  EXPECT_EQ(light.get_last_state_raw(), ELERO_STATE_UNKNOWN);
  auto id = hub.advance(1000);
  ASSERT_NE(id, 0u);
  hub.complete(id, true, 1010);
  EXPECT_EQ(hub.status, "delivery_unconfirmed");
  EXPECT_EQ(light.get_last_state_raw(), ELERO_STATE_UNKNOWN);
  // Missing response: no protocol timeout/retry can be inferred from the capture.
  test_now = 60000; light.loop();
  EXPECT_EQ(hub.advance(60000), 0u);
  EXPECT_EQ(hub.packets.size(), 1u);
  EXPECT_EQ(light.get_last_state_raw(), ELERO_STATE_UNKNOWN);
}

TEST_F(LightDeliveryTest, CapturedContradictoryAndLateStatesCannotOverrideAssumedOff) {
  request(false);
  auto id = hub.advance(1000); ASSERT_NE(id, 0u);
  hub.complete(id, true, 1010);
  for (uint8_t raw : {0x10, 0x03, 0x11, 0x10, 0x03}) {
    test_now += 10000;
    light.set_rx_state(raw);
    EXPECT_EQ(light.get_last_state_raw(), raw);
    EXPECT_FALSE(light.get_is_on());
    EXPECT_FALSE(state.current_values.on);
    EXPECT_EQ(hub.status, "response_unknown");
  }
  EXPECT_EQ(hub.packets.size(), 1u);
  EXPECT_EQ(hub.advance(test_now), 0u);
}

TEST_F(LightDeliveryTest, DuplicateResponsesAndRemotePollsGenerateNoRfOrStateActions) {
  request(true);
  auto id = hub.advance(1000); ASSERT_NE(id, 0u);
  hub.complete(id, true, 1010);
  for (int i = 0; i < 20; ++i) {
    light.set_rx_state(0x03);
    light.schedule_immediate_poll();
  }
  EXPECT_TRUE(light.get_is_on());
  EXPECT_EQ(state.publications, 0u);
  EXPECT_EQ(hub.advance(2000), 0u);
  EXPECT_EQ(hub.packets.size(), 1u);
}

TEST_F(LightDeliveryTest, LegacyMappingRemainsAvailableAndRxDoesNotFeedBackIntoTx) {
  light.set_assumed_state(false);
  request(false);
  auto id = hub.advance(1000); ASSERT_NE(id, 0u);
  hub.complete(id, true, 1010);
  light.set_rx_state(ELERO_STATE_ON);
  EXPECT_TRUE(light.get_is_on());
  EXPECT_TRUE(state.current_values.on);
  EXPECT_EQ(light.get_last_state_raw(), ELERO_STATE_ON);
  EXPECT_EQ(hub.advance(2000), 0u);
  light.set_rx_state(ELERO_STATE_OFF);
  EXPECT_FALSE(light.get_is_on());
  EXPECT_EQ(hub.advance(3000), 0u);
}

TEST_F(LightDeliveryTest, ExplicitConfiguredOnOffBytesUseSharedCounterAndDirectProfile) {
  // Configurable byte plumbing only; these test values have NO hardware semantics.
  // Command profile setters must run before setup attaches the delivery lane.
  hub.unregister_command_delivery(light.get_command_delivery());
  light.set_command_on(0x31); light.set_command_off(0x52);
  light.setup();
  request(true);
  auto id = hub.advance(1000); ASSERT_NE(id, 0u);
  hub.complete(id, true, 1010);
  request(false);
  id = hub.advance(2000); ASSERT_NE(id, 0u);
  hub.complete(id, true, 2010);
  ASSERT_EQ(hub.packets.size(), 2u);
  EXPECT_EQ(hub.packets[0].payload[4], 0x31);
  EXPECT_EQ(hub.packets[1].payload[4], 0x52);
  EXPECT_EQ(hub.packets[1].counter, hub.packets[0].counter + 1);
  for (const auto &p : hub.packets) {
    EXPECT_EQ(p.channel, 3);
    EXPECT_EQ(p.remote_addr, 0x458130u);
    EXPECT_EQ(p.dest_addrs[0], 0xe99b2bu);
    EXPECT_EQ(p.pck_inf[0], 0x6a);
  }
}

TEST(LightDelivery, AssumedModeDoesNotQueryAtBootEvenWithConfiguredCheck) {
  Elero hub;
  EleroLight light;
  light.set_elero_parent(&hub);
  light.set_blind_address(0xe99b2b);
  light.set_remote_address(0x458130);
  light.set_channel(3);
  light.set_pckinf_1(0x6a);
  light.set_command_check(0x77); // deliberately configured but not verified
  light.set_assumed_state(true);
  light.setup();
  EXPECT_EQ(light.get_last_state_raw(), ELERO_STATE_UNKNOWN);
  EXPECT_EQ(hub.advance(1000), 0u);
  EXPECT_TRUE(hub.packets.empty());
}
