#ifndef ETHERNETIP_MASTER__DEVICE_CONFIG_HPP_
#define ETHERNETIP_MASTER__DEVICE_CONFIG_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace ethernetip_master
{

/// Describes a single field inside an assembly.
struct FieldMapping
{
  std::string name;         ///< ROS interface name, e.g. "joint1_pos"
  std::string type;         ///< "float32", "float64", "int8", "uint8", "int16", "uint16",
                            ///  "int32", "uint32", "bool"
  std::size_t offset{0};    ///< Byte offset inside the assembly buffer
  std::size_t bit_offset{0};///< Bit offset (only for bool type)
};

/// Describes one EtherNet/IP target device.
struct DeviceConfig
{
  std::string name;
  std::string ip;

  uint16_t input_assembly{0};
  uint16_t output_assembly{0};
  uint16_t config_assembly{0};

  /// Requested Packet Interval in milliseconds
  double rpi_ms{8.0};

  /// Size of the input assembly in bytes (auto‑computed from mappings if 0)
  std::size_t input_size{0};
  /// Size of the output assembly in bytes (auto‑computed from mappings if 0)
  std::size_t output_size{0};

  std::vector<FieldMapping> inputs;
  std::vector<FieldMapping> outputs;

  /// Optional: name of the device‑driver plugin to load
  std::string plugin_name{"ethernetip_generic_io_driver/GenericIODevice"};
};

}  // namespace ethernetip_master

#endif  // ETHERNETIP_MASTER__DEVICE_CONFIG_HPP_
