// Copyright 2026, ethernetip_master contributors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "ethernetip_master/device_connection.hpp"

using ethernetip_master::DeviceConfig;
using ethernetip_master::DeviceConnection;
using ethernetip_master::FieldMapping;

// ---------------------------------------------------------------------------
// Helper – build a minimal DeviceConfig
// ---------------------------------------------------------------------------
static DeviceConfig make_test_config()
{
  DeviceConfig cfg;
  cfg.name = "test_device";
  cfg.ip = "192.168.1.100";
  cfg.input_assembly = 101;
  cfg.output_assembly = 100;
  cfg.config_assembly = 1;
  cfg.rpi_ms = 8.0;

  // 2 input fields: float32 position + uint16 status
  FieldMapping pos_in;
  pos_in.name = "position";
  pos_in.type = "float32";
  pos_in.offset = 0;
  cfg.inputs.push_back(pos_in);

  FieldMapping status_in;
  status_in.name = "status";
  status_in.type = "uint16";
  status_in.offset = 4;
  cfg.inputs.push_back(status_in);

  // 1 output field: float32 command
  FieldMapping cmd_out;
  cmd_out.name = "command";
  cmd_out.type = "float32";
  cmd_out.offset = 0;
  cfg.outputs.push_back(cmd_out);

  cfg.input_size = 6;   // 4 (float32) + 2 (uint16)
  cfg.output_size = 4;  // 4 (float32)

  return cfg;
}

// ---------------------------------------------------------------------------
// Initial state is kUnconfigured
// ---------------------------------------------------------------------------
TEST(DeviceConnectionTest, InitialState)
{
  DeviceConnection conn;
  EXPECT_EQ(conn.state(), DeviceConnection::State::kUnconfigured);
  EXPECT_EQ(conn.stateString(), "Unconfigured");
}

// ---------------------------------------------------------------------------
// configure() transitions to kConfigured
// ---------------------------------------------------------------------------
TEST(DeviceConnectionTest, ConfigureSucceeds)
{
  DeviceConnection conn;
  auto cfg = make_test_config();
  EXPECT_TRUE(conn.configure(cfg));
  EXPECT_EQ(conn.state(), DeviceConnection::State::kConfigured);
  EXPECT_EQ(conn.stateString(), "Configured");
  EXPECT_EQ(conn.config().name, "test_device");
  EXPECT_EQ(conn.config().ip, "192.168.1.100");
}

// ---------------------------------------------------------------------------
// Buffer sizes match config
// ---------------------------------------------------------------------------
TEST(DeviceConnectionTest, BufferSizes)
{
  DeviceConnection conn;
  conn.configure(make_test_config());

  // Check buffer sizes through getInputBuffer
  std::vector<uint8_t> in_buf;
  conn.getInputBuffer(in_buf);
  EXPECT_EQ(in_buf.size(), 6u);

  // outputBuffer() returns a reference
  EXPECT_EQ(conn.outputBuffer().size(), 4u);
}

// ---------------------------------------------------------------------------
// readInput / writeOutput through AssemblyMap
// ---------------------------------------------------------------------------
TEST(DeviceConnectionTest, ReadWriteThroughMap)
{
  DeviceConnection conn;
  conn.configure(make_test_config());

  // Write a command value
  conn.writeOutput("command", 3.14);

  // Verify it was packed into the output buffer
  const auto & out = conn.outputBuffer();
  float packed;
  std::memcpy(&packed, out.data(), sizeof(packed));
  EXPECT_NEAR(packed, 3.14f, 1e-5);
}

// ---------------------------------------------------------------------------
// updateInputBuffer + readInput
// ---------------------------------------------------------------------------
TEST(DeviceConnectionTest, UpdateInputAndRead)
{
  DeviceConnection conn;
  conn.configure(make_test_config());

  // Simulate received data: float32 position=1.5 at offset 0, uint16 status=7 at offset 4
  std::vector<uint8_t> fake_data(6, 0);
  float pos = 1.5f;
  uint16_t status = 7;
  std::memcpy(fake_data.data(), &pos, sizeof(pos));
  std::memcpy(fake_data.data() + 4, &status, sizeof(status));

  conn.updateInputBuffer(fake_data.data(), fake_data.size());

  EXPECT_NEAR(conn.readInput("position"), 1.5, 1e-5);
  EXPECT_DOUBLE_EQ(conn.readInput("status"), 7.0);
}

// ---------------------------------------------------------------------------
// updateInputBuffer feeds the watchdog
// ---------------------------------------------------------------------------
TEST(DeviceConnectionTest, UpdateInputFeedsWatchdog)
{
  DeviceConnection conn;
  conn.configure(make_test_config());

  // Feed with data – watchdog should not be expired
  std::vector<uint8_t> fake_data(6, 0);
  conn.updateInputBuffer(fake_data.data(), fake_data.size());

  EXPECT_FALSE(conn.watchdog().isExpired());
}

// ---------------------------------------------------------------------------
// setOutputBuffer / getInputBuffer round-trip
// ---------------------------------------------------------------------------
TEST(DeviceConnectionTest, SetOutputBuffer)
{
  DeviceConnection conn;
  conn.configure(make_test_config());

  std::vector<uint8_t> out_data(4, 0xAA);
  conn.setOutputBuffer(out_data);

  EXPECT_EQ(conn.outputBuffer(), out_data);
}

// ---------------------------------------------------------------------------
// Assembly maps are accessible after configure
// ---------------------------------------------------------------------------
TEST(DeviceConnectionTest, AssemblyMapsAvailable)
{
  DeviceConnection conn;
  conn.configure(make_test_config());

  auto in_names = conn.inputMap().fieldNames();
  auto out_names = conn.outputMap().fieldNames();

  EXPECT_EQ(in_names.size(), 2u);
  EXPECT_EQ(out_names.size(), 1u);
}

// ---------------------------------------------------------------------------
// connect() fails if not configured
// ---------------------------------------------------------------------------
TEST(DeviceConnectionTest, ConnectFailsIfUnconfigured)
{
  DeviceConnection conn;
  // connect() without configure() should fail
  EXPECT_FALSE(conn.connect());
}
