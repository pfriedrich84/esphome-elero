#include <gtest/gtest.h>
#include "elero/elero_managed_materialization.h"

using namespace esphome::elero;

static ManagedDevice make_device(const char *name, uint32_t address, ManagedDeviceType type, bool enabled = true) {
  ManagedDevice device{};
  device.enabled = enabled;
  device.type = type;
  device.name = name;
  device.blind_address = address;
  device.remote_address = 0xF0D008;
  device.channel = 4;
  return device;
}

TEST(ManagedMaterializationTests, BindsEnabledDevicesToTypeSpecificSlotsInRegistryOrder) {
  ManagedRegistry registry{};
  registry.devices.push_back(make_device("Cover A", 0xA00001, ManagedDeviceType::COVER));
  registry.devices.push_back(make_device("Light A", 0xA00002, ManagedDeviceType::LIGHT));
  registry.devices.push_back(make_device("Cover B", 0xA00003, ManagedDeviceType::COVER));

  auto plan = managed_materialization::build_preallocated_slot_plan(registry, 2, 1);

  EXPECT_TRUE(plan.fits());
  ASSERT_EQ(plan.bindings.size(), 3u);
  EXPECT_EQ(plan.bindings[0].type, ManagedDeviceType::COVER);
  EXPECT_EQ(plan.bindings[0].device_index, 0u);
  EXPECT_EQ(plan.bindings[0].slot_index, 0u);
  EXPECT_EQ(plan.bindings[1].type, ManagedDeviceType::LIGHT);
  EXPECT_EQ(plan.bindings[1].device_index, 1u);
  EXPECT_EQ(plan.bindings[1].slot_index, 0u);
  EXPECT_EQ(plan.bindings[2].type, ManagedDeviceType::COVER);
  EXPECT_EQ(plan.bindings[2].device_index, 2u);
  EXPECT_EQ(plan.bindings[2].slot_index, 1u);
}

TEST(ManagedMaterializationTests, IgnoresDisabledDevices) {
  ManagedRegistry registry{};
  registry.devices.push_back(make_device("Disabled", 0xA00001, ManagedDeviceType::COVER, false));
  registry.devices.push_back(make_device("Enabled", 0xA00002, ManagedDeviceType::COVER));

  auto plan = managed_materialization::build_preallocated_slot_plan(registry, 1, 0);

  EXPECT_TRUE(plan.fits());
  EXPECT_EQ(plan.enabled_cover_count, 1u);
  ASSERT_EQ(plan.bindings.size(), 1u);
  EXPECT_EQ(plan.bindings[0].device_index, 1u);
  EXPECT_EQ(plan.bindings[0].slot_index, 0u);
}

TEST(ManagedMaterializationTests, ReportsUnboundDevicesWhenSlotsAreInsufficient) {
  ManagedRegistry registry{};
  registry.devices.push_back(make_device("Cover A", 0xA00001, ManagedDeviceType::COVER));
  registry.devices.push_back(make_device("Cover B", 0xA00002, ManagedDeviceType::COVER));
  registry.devices.push_back(make_device("Light A", 0xA00003, ManagedDeviceType::LIGHT));

  auto plan = managed_materialization::build_preallocated_slot_plan(registry, 1, 0);

  EXPECT_FALSE(plan.fits());
  EXPECT_EQ(plan.used_slot_count(), 1u);
  EXPECT_EQ(plan.unbound_cover_count, 1u);
  EXPECT_EQ(plan.unbound_light_count, 1u);
}
