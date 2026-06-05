#include <gtest/gtest.h>
#include "elero/elero_managed_registry.h"

using namespace esphome::elero;

static ManagedRegistry make_valid_registry() {
  ManagedRegistry registry{};
  registry.schema_version = ELERO_MANAGED_SCHEMA_VERSION;
  registry.registry_revision = 1;
  registry.hub_id = 0x12345678;

  ManagedDevice cover{};
  cover.enabled = true;
  cover.type = ManagedDeviceType::COVER;
  cover.name = "Kitchen Blind";
  cover.blind_address = 0xA831E5;
  cover.remote_address = 0xF0D008;
  cover.channel = 4;
  cover.pck_inf[0] = 0x6A;
  cover.pck_inf[1] = 0x00;
  cover.hop = 1;
  cover.payload[0] = 0x01;
  cover.payload[1] = 0x02;
  cover.open_duration_ms = 25000;
  cover.close_duration_ms = 26000;
  cover.poll_interval_ms = 300000;
  cover.supports_tilt = true;
  registry.devices.push_back(cover);

  managed_registry::refresh_checksum(registry);
  return registry;
}

TEST(ManagedRegistryTests, AcceptsValidRegistry) {
  auto registry = make_valid_registry();
  auto result = managed_registry::validate(registry, 32, 0x12345678);
  EXPECT_TRUE(result.ok) << result.error;
}

TEST(ManagedRegistryTests, RejectsChecksumMismatch) {
  auto registry = make_valid_registry();
  registry.devices[0].channel = 5;
  auto result = managed_registry::validate(registry, 32, 0x12345678);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, "checksum mismatch");
}

TEST(ManagedRegistryTests, RejectsWrongHub) {
  auto registry = make_valid_registry();
  auto result = managed_registry::validate(registry, 32, 0x87654321);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, "hub_id does not match this ESP32");
}

TEST(ManagedRegistryTests, RejectsStaleRegistryRevisionWhenExpected) {
  auto registry = make_valid_registry();
  auto result = managed_registry::validate(registry, 32, 0x12345678, 2);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, "registry_revision is stale");
}

TEST(ManagedRegistryTests, RejectsDuplicateBlindAddress) {
  auto registry = make_valid_registry();
  auto duplicate = registry.devices[0];
  duplicate.name = "Duplicate";
  registry.devices.push_back(duplicate);
  managed_registry::refresh_checksum(registry);

  auto result = managed_registry::validate(registry, 32, 0x12345678);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, "duplicate blind_address");
}

TEST(ManagedRegistryTests, RejectsTooManyDevices) {
  auto registry = make_valid_registry();
  auto result = managed_registry::validate(registry, 0, 0x12345678);
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, "too many devices");
}
