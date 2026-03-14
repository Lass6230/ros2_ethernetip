// Copyright 2026, ethernetip_generic_io_driver contributors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "ethernetip_generic_io_driver/generic_io_device.hpp"
#include "ethernetip_master/device_config.hpp"

using ethernetip_generic_io_driver::GenericIODevice;
using ethernetip_master::DeviceConfig;
using ethernetip_master::FieldMapping;

// ---------------------------------------------------------------------------
// Helper – build a test config with 2 inputs and 1 output
// ---------------------------------------------------------------------------
static DeviceConfig make_test_config()
{
  DeviceConfig cfg;
  cfg.name = "test_joint";
  cfg.ip = "192.168.1.10";
  cfg.input_assembly = 101;
  cfg.output_assembly = 100;
  cfg.rpi_ms = 8.0;

  FieldMapping pos;
  pos.name = "position";
  pos.type = "float32";
  pos.offset = 0;
  cfg.inputs.push_back(pos);

  FieldMapping vel;
  vel.name = "velocity";
  vel.type = "float32";
  vel.offset = 4;
  cfg.inputs.push_back(vel);

  FieldMapping cmd;
  cmd.name = "target_position";
  cmd.type = "float32";
  cmd.offset = 0;
  cfg.outputs.push_back(cmd);

  cfg.input_size = 8;
  cfg.output_size = 4;

  return cfg;
}

// ---------------------------------------------------------------------------
// configure
// ---------------------------------------------------------------------------
TEST(GenericIODeviceTest, ConfigureSucceeds)
{
  GenericIODevice dev;
  EXPECT_TRUE(dev.configure(make_test_config()));
}

// ---------------------------------------------------------------------------
// exportStateInterfaces
// ---------------------------------------------------------------------------
TEST(GenericIODeviceTest, ExportStateInterfaces)
{
  GenericIODevice dev;
  dev.configure(make_test_config());

  auto si = dev.exportStateInterfaces();
  ASSERT_EQ(si.size(), 2u);

  EXPECT_EQ(si[0].joint_name, "test_joint");
  EXPECT_EQ(si[0].interface_name, "position");
  EXPECT_EQ(si[0].field_name, "position");

  EXPECT_EQ(si[1].joint_name, "test_joint");
  EXPECT_EQ(si[1].interface_name, "velocity");
}

// ---------------------------------------------------------------------------
// exportCommandInterfaces
// ---------------------------------------------------------------------------
TEST(GenericIODeviceTest, ExportCommandInterfaces)
{
  GenericIODevice dev;
  dev.configure(make_test_config());

  auto ci = dev.exportCommandInterfaces();
  ASSERT_EQ(ci.size(), 1u);

  EXPECT_EQ(ci[0].joint_name, "test_joint");
  EXPECT_EQ(ci[0].interface_name, "target_position");
}

// ---------------------------------------------------------------------------
// processInput unpacks buffer → getStateValue
// ---------------------------------------------------------------------------
TEST(GenericIODeviceTest, ProcessInputAndGetState)
{
  GenericIODevice dev;
  dev.configure(make_test_config());

  // Build a fake input buffer: position=1.5, velocity=-2.0
  std::vector<uint8_t> buf(8, 0);
  float pos = 1.5f;
  float vel = -2.0f;
  std::memcpy(buf.data(), &pos, sizeof(pos));
  std::memcpy(buf.data() + 4, &vel, sizeof(vel));

  dev.processInput(buf.data(), buf.size());

  EXPECT_NEAR(dev.getStateValue("position"), 1.5, 1e-5);
  EXPECT_NEAR(dev.getStateValue("velocity"), -2.0, 1e-5);
}

// ---------------------------------------------------------------------------
// setCommandValue → prepareOutput packs buffer
// ---------------------------------------------------------------------------
TEST(GenericIODeviceTest, SetCommandAndPrepareOutput)
{
  GenericIODevice dev;
  dev.configure(make_test_config());

  dev.setCommandValue("target_position", 42.0);

  std::vector<uint8_t> buf(4, 0);
  dev.prepareOutput(buf.data(), buf.size());

  float packed;
  std::memcpy(&packed, buf.data(), sizeof(packed));
  EXPECT_NEAR(packed, 42.0f, 1e-5);
}

// ---------------------------------------------------------------------------
// processInput with insufficient buffer is a no-op
// ---------------------------------------------------------------------------
TEST(GenericIODeviceTest, ProcessInputShortBuffer)
{
  GenericIODevice dev;
  dev.configure(make_test_config());

  // Buffer too short → should not crash, values stay at 0
  std::vector<uint8_t> short_buf(2, 0xFF);
  dev.processInput(short_buf.data(), short_buf.size());

  EXPECT_DOUBLE_EQ(dev.getStateValue("position"), 0.0);
  EXPECT_DOUBLE_EQ(dev.getStateValue("velocity"), 0.0);
}

// ---------------------------------------------------------------------------
// prepareOutput with insufficient buffer is a no-op
// ---------------------------------------------------------------------------
TEST(GenericIODeviceTest, PrepareOutputShortBuffer)
{
  GenericIODevice dev;
  dev.configure(make_test_config());

  dev.setCommandValue("target_position", 99.0);

  // Buffer too short → should not crash
  std::vector<uint8_t> short_buf(2, 0);
  dev.prepareOutput(short_buf.data(), short_buf.size());
  // Expect buffer unchanged (no write happened)
  EXPECT_EQ(short_buf[0], 0);
  EXPECT_EQ(short_buf[1], 0);
}

// ---------------------------------------------------------------------------
// getStateValue for unknown key returns 0
// ---------------------------------------------------------------------------
TEST(GenericIODeviceTest, GetStateUnknownKey)
{
  GenericIODevice dev;
  dev.configure(make_test_config());
  EXPECT_DOUBLE_EQ(dev.getStateValue("nonexistent"), 0.0);
}

// ---------------------------------------------------------------------------
// Full round-trip: set commands, prepare, process, get states
// ---------------------------------------------------------------------------
TEST(GenericIODeviceTest, FullRoundTrip)
{
  GenericIODevice dev;
  auto cfg = make_test_config();
  dev.configure(cfg);

  // Simulate: set command, pack output, then feed that as input
  dev.setCommandValue("target_position", 7.77);

  // Prepare the output buffer
  std::vector<uint8_t> out_buf(4, 0);
  dev.prepareOutput(out_buf.data(), out_buf.size());

  // Now create a separate input device that would read similar data
  GenericIODevice dev2;
  DeviceConfig cfg2;
  cfg2.name = "mirror";
  cfg2.input_size = 4;
  cfg2.output_size = 0;

  FieldMapping f;
  f.name = "target_position";
  f.type = "float32";
  f.offset = 0;
  cfg2.inputs.push_back(f);
  dev2.configure(cfg2);

  dev2.processInput(out_buf.data(), out_buf.size());
  EXPECT_NEAR(dev2.getStateValue("target_position"), 7.77, 1e-3);
}
