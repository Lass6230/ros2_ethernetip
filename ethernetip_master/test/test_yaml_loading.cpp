// Copyright 2026, ethernetip_master contributors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <string>

#include "ethernetip_master/ethernetip_master.hpp"

using ethernetip_master::EthernetIPMaster;

// ---------------------------------------------------------------------------
// Helper – get path to test fixture files
// ---------------------------------------------------------------------------
static std::string fixture_path(const std::string & filename)
{
  // TEST_FIXTURES_DIR is defined via CMake add_definitions
  std::string dir = TEST_FIXTURES_DIR;
  return dir + "/" + filename;
}

// ---------------------------------------------------------------------------
// Load valid YAML
// ---------------------------------------------------------------------------
TEST(YamlLoadingTest, LoadValidConfig)
{
  EthernetIPMaster master;
  EXPECT_TRUE(master.loadConfig(fixture_path("test_devices.yaml")));
  EXPECT_EQ(master.devices().size(), 2u);

  // Check first device
  const auto & dev0 = master.devices()[0];
  EXPECT_EQ(dev0->config().name, "test_plc");
  EXPECT_EQ(dev0->config().ip, "192.168.1.50");
  EXPECT_EQ(dev0->config().input_assembly, 101);
  EXPECT_EQ(dev0->config().output_assembly, 100);
  EXPECT_EQ(dev0->config().config_assembly, 1);
  EXPECT_DOUBLE_EQ(dev0->config().rpi_ms, 8.0);
  EXPECT_EQ(dev0->config().inputs.size(), 3u);
  EXPECT_EQ(dev0->config().outputs.size(), 1u);

  // Check second device
  const auto & dev1 = master.devices()[1];
  EXPECT_EQ(dev1->config().name, "test_drive");
  EXPECT_EQ(dev1->config().ip, "192.168.1.51");
  EXPECT_DOUBLE_EQ(dev1->config().rpi_ms, 4.0);  // "4ms" parsed
  EXPECT_EQ(dev1->config().inputs.size(), 2u);
  EXPECT_EQ(dev1->config().outputs.size(), 2u);
}

// ---------------------------------------------------------------------------
// Load YAML with no 'devices' key
// ---------------------------------------------------------------------------
TEST(YamlLoadingTest, LoadBadConfigNoDevicesKey)
{
  EthernetIPMaster master;
  EXPECT_FALSE(master.loadConfig(fixture_path("bad_devices.yaml")));
  EXPECT_EQ(master.devices().size(), 0u);
}

// ---------------------------------------------------------------------------
// Load non-existent file
// ---------------------------------------------------------------------------
TEST(YamlLoadingTest, LoadNonExistentFile)
{
  EthernetIPMaster master;
  EXPECT_FALSE(master.loadConfig("/tmp/nonexistent_file_12345.yaml"));
  EXPECT_EQ(master.devices().size(), 0u);
}

// ---------------------------------------------------------------------------
// Field mapping details are correct
// ---------------------------------------------------------------------------
TEST(YamlLoadingTest, FieldMappingDetails)
{
  EthernetIPMaster master;
  ASSERT_TRUE(master.loadConfig(fixture_path("test_devices.yaml")));

  const auto & dev0 = master.devices()[0];

  // Check bool field has correct bit_offset
  const auto & inputs = dev0->config().inputs;
  ASSERT_EQ(inputs.size(), 3u);
  EXPECT_EQ(inputs[2].name, "alarm_bit");
  EXPECT_EQ(inputs[2].type, "bool");
  EXPECT_EQ(inputs[2].offset, 6u);
  EXPECT_EQ(inputs[2].bit_offset, 0u);

  // Plugin name defaults
  EXPECT_EQ(dev0->config().plugin_name,
    "ethernetip_generic_io_driver/GenericIODevice");
}

// ---------------------------------------------------------------------------
// addDevice manually
// ---------------------------------------------------------------------------
TEST(YamlLoadingTest, AddDeviceManually)
{
  EthernetIPMaster master;

  ethernetip_master::DeviceConfig cfg;
  cfg.name = "manual_dev";
  cfg.ip = "10.0.0.1";
  cfg.input_assembly = 50;
  cfg.output_assembly = 51;

  ethernetip_master::FieldMapping f;
  f.name = "val";
  f.type = "uint16";
  f.offset = 0;
  cfg.inputs.push_back(f);
  cfg.outputs.push_back(f);

  EXPECT_TRUE(master.addDevice(cfg));
  EXPECT_EQ(master.devices().size(), 1u);
  EXPECT_EQ(master.devices()[0]->config().name, "manual_dev");
}
