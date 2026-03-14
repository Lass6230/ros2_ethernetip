#include "ethernetip_master/assembly_map.hpp"

#include <algorithm>
#include <cstring>

namespace ethernetip_master
{

// ---------------------------------------------------------------------------
// Helper: compute the byte size of a single type name.
// ---------------------------------------------------------------------------
static std::size_t typeSize(const std::string & type)
{
  if (type == "float64" || type == "double") return 8;
  if (type == "float32" || type == "float")  return 4;
  if (type == "int32"   || type == "uint32") return 4;
  if (type == "int16"   || type == "uint16") return 2;
  if (type == "int8"    || type == "uint8")  return 1;
  if (type == "bool")                        return 0;  // bit-level
  return 4;  // default to 4 bytes
}

// ---------------------------------------------------------------------------
// ctor
// ---------------------------------------------------------------------------
AssemblyMap::AssemblyMap(
  const std::vector<FieldMapping> & fields,
  std::size_t total_size)
{
  std::size_t max_end = 0;
  for (const auto & f : fields) {
    fields_[f.name] = f;
    std::size_t end = f.offset + std::max<std::size_t>(typeSize(f.type), 1);
    if (end > max_end) { max_end = end; }
  }
  buffer_size_ = (total_size > 0) ? total_size : max_end;
}

// ---------------------------------------------------------------------------
// lookup
// ---------------------------------------------------------------------------
const FieldMapping & AssemblyMap::lookup(const std::string & name) const
{
  auto it = fields_.find(name);
  if (it == fields_.end()) {
    throw std::out_of_range("AssemblyMap: field '" + name + "' not found");
  }
  return it->second;
}

// ---------------------------------------------------------------------------
// fieldNames
// ---------------------------------------------------------------------------
std::vector<std::string> AssemblyMap::fieldNames() const
{
  std::vector<std::string> names;
  names.reserve(fields_.size());
  for (const auto & [n, _] : fields_) {
    names.push_back(n);
  }
  return names;
}

// ---------------------------------------------------------------------------
// getters
// ---------------------------------------------------------------------------
double AssemblyMap::getDouble(const std::string & name, const uint8_t * buf) const
{
  const auto & f = lookup(name);
  double v;
  std::memcpy(&v, buf + f.offset, sizeof(v));
  return v;
}

float AssemblyMap::getFloat(const std::string & name, const uint8_t * buf) const
{
  const auto & f = lookup(name);
  float v;
  std::memcpy(&v, buf + f.offset, sizeof(v));
  return v;
}

int32_t AssemblyMap::getInt32(const std::string & name, const uint8_t * buf) const
{
  const auto & f = lookup(name);
  int32_t v;
  std::memcpy(&v, buf + f.offset, sizeof(v));
  return v;
}

uint32_t AssemblyMap::getUint32(const std::string & name, const uint8_t * buf) const
{
  const auto & f = lookup(name);
  uint32_t v;
  std::memcpy(&v, buf + f.offset, sizeof(v));
  return v;
}

int16_t AssemblyMap::getInt16(const std::string & name, const uint8_t * buf) const
{
  const auto & f = lookup(name);
  int16_t v;
  std::memcpy(&v, buf + f.offset, sizeof(v));
  return v;
}

uint16_t AssemblyMap::getUint16(const std::string & name, const uint8_t * buf) const
{
  const auto & f = lookup(name);
  uint16_t v;
  std::memcpy(&v, buf + f.offset, sizeof(v));
  return v;
}

int8_t AssemblyMap::getInt8(const std::string & name, const uint8_t * buf) const
{
  const auto & f = lookup(name);
  return static_cast<int8_t>(buf[f.offset]);
}

uint8_t AssemblyMap::getUint8(const std::string & name, const uint8_t * buf) const
{
  const auto & f = lookup(name);
  return buf[f.offset];
}

bool AssemblyMap::getBool(const std::string & name, const uint8_t * buf) const
{
  const auto & f = lookup(name);
  uint8_t byte = buf[f.offset];
  return (byte >> f.bit_offset) & 0x01;
}

double AssemblyMap::getValue(const std::string & name, const uint8_t * buf) const
{
  const auto & f = lookup(name);
  const auto & t = f.type;
  if (t == "float64" || t == "double")   return getDouble(name, buf);
  if (t == "float32" || t == "float")    return static_cast<double>(getFloat(name, buf));
  if (t == "int32")                      return static_cast<double>(getInt32(name, buf));
  if (t == "uint32")                     return static_cast<double>(getUint32(name, buf));
  if (t == "int16")                      return static_cast<double>(getInt16(name, buf));
  if (t == "uint16")                     return static_cast<double>(getUint16(name, buf));
  if (t == "int8")                       return static_cast<double>(getInt8(name, buf));
  if (t == "uint8")                      return static_cast<double>(getUint8(name, buf));
  if (t == "bool")                       return getBool(name, buf) ? 1.0 : 0.0;
  return 0.0;
}

// ---------------------------------------------------------------------------
// setters
// ---------------------------------------------------------------------------
void AssemblyMap::setDouble(
  const std::string & name, uint8_t * buf, double val) const
{
  const auto & f = lookup(name);
  std::memcpy(buf + f.offset, &val, sizeof(val));
}

void AssemblyMap::setFloat(
  const std::string & name, uint8_t * buf, float val) const
{
  const auto & f = lookup(name);
  std::memcpy(buf + f.offset, &val, sizeof(val));
}

void AssemblyMap::setInt32(
  const std::string & name, uint8_t * buf, int32_t val) const
{
  const auto & f = lookup(name);
  std::memcpy(buf + f.offset, &val, sizeof(val));
}

void AssemblyMap::setUint32(
  const std::string & name, uint8_t * buf, uint32_t val) const
{
  const auto & f = lookup(name);
  std::memcpy(buf + f.offset, &val, sizeof(val));
}

void AssemblyMap::setInt16(
  const std::string & name, uint8_t * buf, int16_t val) const
{
  const auto & f = lookup(name);
  std::memcpy(buf + f.offset, &val, sizeof(val));
}

void AssemblyMap::setUint16(
  const std::string & name, uint8_t * buf, uint16_t val) const
{
  const auto & f = lookup(name);
  std::memcpy(buf + f.offset, &val, sizeof(val));
}

void AssemblyMap::setInt8(
  const std::string & name, uint8_t * buf, int8_t val) const
{
  const auto & f = lookup(name);
  buf[f.offset] = static_cast<uint8_t>(val);
}

void AssemblyMap::setUint8(
  const std::string & name, uint8_t * buf, uint8_t val) const
{
  const auto & f = lookup(name);
  buf[f.offset] = val;
}

void AssemblyMap::setBool(
  const std::string & name, uint8_t * buf, bool val) const
{
  const auto & f = lookup(name);
  if (val) {
    buf[f.offset] |= (1u << f.bit_offset);
  } else {
    buf[f.offset] &= ~(1u << f.bit_offset);
  }
}

void AssemblyMap::setValue(
  const std::string & name, uint8_t * buf, double val) const
{
  const auto & f = lookup(name);
  const auto & t = f.type;
  if (t == "float64" || t == "double")   { setDouble(name, buf, val); return; }
  if (t == "float32" || t == "float")    { setFloat(name, buf, static_cast<float>(val)); return; }
  if (t == "int32")                      { setInt32(name, buf, static_cast<int32_t>(val)); return; }
  if (t == "uint32")                     { setUint32(name, buf, static_cast<uint32_t>(val)); return; }
  if (t == "int16")                      { setInt16(name, buf, static_cast<int16_t>(val)); return; }
  if (t == "uint16")                     { setUint16(name, buf, static_cast<uint16_t>(val)); return; }
  if (t == "int8")                       { setInt8(name, buf, static_cast<int8_t>(val)); return; }
  if (t == "uint8")                      { setUint8(name, buf, static_cast<uint8_t>(val)); return; }
  if (t == "bool")                       { setBool(name, buf, val != 0.0); return; }
}

}  // namespace ethernetip_master
