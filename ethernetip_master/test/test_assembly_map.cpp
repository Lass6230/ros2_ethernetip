// Copyright 2026, ethernetip_master contributors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "ethernetip_master/assembly_map.hpp"
#include "ethernetip_master/device_config.hpp"

using ethernetip_master::AssemblyMap;
using ethernetip_master::FieldMapping;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static FieldMapping make_field(
  const std::string & name, const std::string & type,
  std::size_t offset, std::size_t bit_offset = 0)
{
  FieldMapping f;
  f.name = name;
  f.type = type;
  f.offset = offset;
  f.bit_offset = bit_offset;
  return f;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
TEST(AssemblyMapTest, EmptyFieldList)
{
  AssemblyMap map({}, 0);
  EXPECT_EQ(map.bufferSize(), 0u);
  EXPECT_TRUE(map.fieldNames().empty());
}

TEST(AssemblyMapTest, BufferSizeInferred)
{
  // float32 at offset 0 → needs 4 bytes, float64 at offset 4 → needs 12
  std::vector<FieldMapping> fields = {
    make_field("a", "float32", 0),
    make_field("b", "float64", 4),
  };
  AssemblyMap map(fields);
  EXPECT_EQ(map.bufferSize(), 12u);
}

TEST(AssemblyMapTest, ExplicitBufferSize)
{
  std::vector<FieldMapping> fields = {make_field("x", "uint8", 0)};
  AssemblyMap map(fields, 64);
  EXPECT_EQ(map.bufferSize(), 64u);
}

TEST(AssemblyMapTest, FieldNamesReturnsAll)
{
  std::vector<FieldMapping> fields = {
    make_field("alpha", "uint16", 0),
    make_field("beta", "uint16", 2),
    make_field("gamma", "uint16", 4),
  };
  AssemblyMap map(fields);
  auto names = map.fieldNames();
  ASSERT_EQ(names.size(), 3u);
  // names may be in any order (unordered_map), so sort
  std::sort(names.begin(), names.end());
  EXPECT_EQ(names[0], "alpha");
  EXPECT_EQ(names[1], "beta");
  EXPECT_EQ(names[2], "gamma");
}

// ---------------------------------------------------------------------------
// Float64 (double) round-trip
// ---------------------------------------------------------------------------
TEST(AssemblyMapTest, Float64RoundTrip)
{
  std::vector<FieldMapping> fields = {make_field("val", "float64", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setDouble("val", buf.data(), 3.14159265);
  EXPECT_DOUBLE_EQ(map.getDouble("val", buf.data()), 3.14159265);
}

// ---------------------------------------------------------------------------
// Float32 round-trip
// ---------------------------------------------------------------------------
TEST(AssemblyMapTest, Float32RoundTrip)
{
  std::vector<FieldMapping> fields = {make_field("val", "float32", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setFloat("val", buf.data(), 2.5f);
  EXPECT_FLOAT_EQ(map.getFloat("val", buf.data()), 2.5f);
}

// ---------------------------------------------------------------------------
// Integer types
// ---------------------------------------------------------------------------
TEST(AssemblyMapTest, Int32RoundTrip)
{
  std::vector<FieldMapping> fields = {make_field("val", "int32", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setInt32("val", buf.data(), -123456);
  EXPECT_EQ(map.getInt32("val", buf.data()), -123456);
}

TEST(AssemblyMapTest, Uint32RoundTrip)
{
  std::vector<FieldMapping> fields = {make_field("val", "uint32", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setUint32("val", buf.data(), 0xDEADBEEF);
  EXPECT_EQ(map.getUint32("val", buf.data()), 0xDEADBEEF);
}

TEST(AssemblyMapTest, Int16RoundTrip)
{
  std::vector<FieldMapping> fields = {make_field("val", "int16", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setInt16("val", buf.data(), -1234);
  EXPECT_EQ(map.getInt16("val", buf.data()), -1234);
}

TEST(AssemblyMapTest, Uint16RoundTrip)
{
  std::vector<FieldMapping> fields = {make_field("val", "uint16", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setUint16("val", buf.data(), 0xABCD);
  EXPECT_EQ(map.getUint16("val", buf.data()), 0xABCD);
}

TEST(AssemblyMapTest, Int8RoundTrip)
{
  std::vector<FieldMapping> fields = {make_field("val", "int8", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setInt8("val", buf.data(), -42);
  EXPECT_EQ(map.getInt8("val", buf.data()), -42);
}

TEST(AssemblyMapTest, Uint8RoundTrip)
{
  std::vector<FieldMapping> fields = {make_field("val", "uint8", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setUint8("val", buf.data(), 0xFE);
  EXPECT_EQ(map.getUint8("val", buf.data()), 0xFE);
}

// ---------------------------------------------------------------------------
// Bool with bit offset
// ---------------------------------------------------------------------------
TEST(AssemblyMapTest, BoolBitOffset)
{
  std::vector<FieldMapping> fields = {
    make_field("b0", "bool", 0, 0),
    make_field("b3", "bool", 0, 3),
    make_field("b7", "bool", 0, 7),
  };
  AssemblyMap map(fields, 1);
  std::vector<uint8_t> buf(1, 0);

  // Set bit 3
  map.setBool("b3", buf.data(), true);
  EXPECT_FALSE(map.getBool("b0", buf.data()));
  EXPECT_TRUE(map.getBool("b3", buf.data()));
  EXPECT_FALSE(map.getBool("b7", buf.data()));

  // Set bit 0
  map.setBool("b0", buf.data(), true);
  EXPECT_TRUE(map.getBool("b0", buf.data()));
  EXPECT_TRUE(map.getBool("b3", buf.data()));

  // Clear bit 3
  map.setBool("b3", buf.data(), false);
  EXPECT_TRUE(map.getBool("b0", buf.data()));
  EXPECT_FALSE(map.getBool("b3", buf.data()));
}

// ---------------------------------------------------------------------------
// getValue / setValue  generic dispatch
// ---------------------------------------------------------------------------
TEST(AssemblyMapTest, GenericGetSetFloat32)
{
  std::vector<FieldMapping> fields = {make_field("f", "float32", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setValue("f", buf.data(), 1.5);
  EXPECT_NEAR(map.getValue("f", buf.data()), 1.5, 1e-5);
}

TEST(AssemblyMapTest, GenericGetSetInt16)
{
  std::vector<FieldMapping> fields = {make_field("i", "int16", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setValue("i", buf.data(), -500.0);
  EXPECT_DOUBLE_EQ(map.getValue("i", buf.data()), -500.0);
}

TEST(AssemblyMapTest, GenericGetSetBool)
{
  std::vector<FieldMapping> fields = {make_field("flag", "bool", 0, 2)};
  AssemblyMap map(fields, 1);
  std::vector<uint8_t> buf(1, 0);

  map.setValue("flag", buf.data(), 1.0);
  EXPECT_DOUBLE_EQ(map.getValue("flag", buf.data()), 1.0);

  map.setValue("flag", buf.data(), 0.0);
  EXPECT_DOUBLE_EQ(map.getValue("flag", buf.data()), 0.0);
}

// ---------------------------------------------------------------------------
// Multiple fields – no overlap
// ---------------------------------------------------------------------------
TEST(AssemblyMapTest, MultipleFieldsNoOverlap)
{
  std::vector<FieldMapping> fields = {
    make_field("pos", "float32", 0),
    make_field("vel", "float32", 4),
    make_field("status", "uint16", 8),
  };
  AssemblyMap map(fields);
  EXPECT_EQ(map.bufferSize(), 10u);

  std::vector<uint8_t> buf(map.bufferSize(), 0);
  map.setFloat("pos", buf.data(), 1.0f);
  map.setFloat("vel", buf.data(), 2.0f);
  map.setUint16("status", buf.data(), 42);

  EXPECT_FLOAT_EQ(map.getFloat("pos", buf.data()), 1.0f);
  EXPECT_FLOAT_EQ(map.getFloat("vel", buf.data()), 2.0f);
  EXPECT_EQ(map.getUint16("status", buf.data()), 42);
}

// ---------------------------------------------------------------------------
// Unknown field throws
// ---------------------------------------------------------------------------
TEST(AssemblyMapTest, UnknownFieldThrows)
{
  std::vector<FieldMapping> fields = {make_field("x", "float32", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  EXPECT_THROW(map.getValue("nonexistent", buf.data()), std::out_of_range);
  EXPECT_THROW(map.setValue("nonexistent", buf.data(), 0.0), std::out_of_range);
}

// ---------------------------------------------------------------------------
// Aliased type names ("float" == "float32", "double" == "float64")
// ---------------------------------------------------------------------------
TEST(AssemblyMapTest, AliasedTypeFloat)
{
  std::vector<FieldMapping> fields = {make_field("a", "float", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setValue("a", buf.data(), 7.5);
  EXPECT_NEAR(map.getValue("a", buf.data()), 7.5, 1e-5);
}

TEST(AssemblyMapTest, AliasedTypeDouble)
{
  std::vector<FieldMapping> fields = {make_field("a", "double", 0)};
  AssemblyMap map(fields);
  std::vector<uint8_t> buf(map.bufferSize(), 0);

  map.setValue("a", buf.data(), 99.99);
  EXPECT_DOUBLE_EQ(map.getValue("a", buf.data()), 99.99);
}
