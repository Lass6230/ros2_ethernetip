#ifndef ETHERNETIP_MASTER__ASSEMBLY_MAP_HPP_
#define ETHERNETIP_MASTER__ASSEMBLY_MAP_HPP_

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ethernetip_master/device_config.hpp"

namespace ethernetip_master
{

/// Utility that packs / unpacks typed values into a raw byte buffer using the
/// field mapping definitions from a DeviceConfig.
class AssemblyMap
{
public:
  /// Build the map from a list of field mappings.  \p total_size is the
  /// expected buffer length in bytes; if 0 it will be inferred from the
  /// mappings.
  explicit AssemblyMap(
    const std::vector<FieldMapping> & fields,
    std::size_t total_size = 0);

  /// Return the minimum buffer size required.
  std::size_t bufferSize() const { return buffer_size_; }

  // ----- typed getters (from raw buffer) -----------------------------------
  double getDouble(const std::string & name, const uint8_t * buf) const;
  float getFloat(const std::string & name, const uint8_t * buf) const;
  int32_t getInt32(const std::string & name, const uint8_t * buf) const;
  uint32_t getUint32(const std::string & name, const uint8_t * buf) const;
  int16_t getInt16(const std::string & name, const uint8_t * buf) const;
  uint16_t getUint16(const std::string & name, const uint8_t * buf) const;
  int8_t getInt8(const std::string & name, const uint8_t * buf) const;
  uint8_t getUint8(const std::string & name, const uint8_t * buf) const;
  bool getBool(const std::string & name, const uint8_t * buf) const;

  /// Generic getter that returns a double regardless of the underlying type.
  double getValue(const std::string & name, const uint8_t * buf) const;

  // ----- typed setters (into raw buffer) -----------------------------------
  void setDouble(const std::string & name, uint8_t * buf, double val) const;
  void setFloat(const std::string & name, uint8_t * buf, float val) const;
  void setInt32(const std::string & name, uint8_t * buf, int32_t val) const;
  void setUint32(const std::string & name, uint8_t * buf, uint32_t val) const;
  void setInt16(const std::string & name, uint8_t * buf, int16_t val) const;
  void setUint16(const std::string & name, uint8_t * buf, uint16_t val) const;
  void setInt8(const std::string & name, uint8_t * buf, int8_t val) const;
  void setUint8(const std::string & name, uint8_t * buf, uint8_t val) const;
  void setBool(const std::string & name, uint8_t * buf, bool val) const;

  /// Generic setter that accepts a double and converts to the underlying type.
  void setValue(const std::string & name, uint8_t * buf, double val) const;

  /// List all field names.
  std::vector<std::string> fieldNames() const;

private:
  const FieldMapping & lookup(const std::string & name) const;

  std::unordered_map<std::string, FieldMapping> fields_;
  std::size_t buffer_size_{0};
};

}  // namespace ethernetip_master

#endif  // ETHERNETIP_MASTER__ASSEMBLY_MAP_HPP_
