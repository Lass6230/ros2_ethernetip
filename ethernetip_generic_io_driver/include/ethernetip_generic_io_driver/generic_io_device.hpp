#ifndef ETHERNETIP_GENERIC_IO_DRIVER__GENERIC_IO_DEVICE_HPP_
#define ETHERNETIP_GENERIC_IO_DRIVER__GENERIC_IO_DEVICE_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ethernetip_device_driver_interface/ethernetip_device.hpp"
#include "ethernetip_master/assembly_map.hpp"
#include "ethernetip_master/device_config.hpp"

namespace ethernetip_generic_io_driver
{

/// Generic EtherNet/IP device driver that maps YAML‑described assembly fields
/// 1:1 to ros2_control state/command interfaces.
///
/// This is the default plugin; it should work for any device whose IO
/// assemblies are fully described in YAML.
class GenericIODevice
  : public ethernetip_device_driver_interface::EthernetIPDevice
{
public:
  GenericIODevice() = default;
  ~GenericIODevice() override = default;

  // EthernetIPDevice interface ------------------------------------------------

  bool configure(const ethernetip_master::DeviceConfig & config) override;

  void processInput(const uint8_t * data, std::size_t size) override;
  void prepareOutput(uint8_t * data, std::size_t size) override;

  std::vector<ethernetip_device_driver_interface::InterfaceDescription>
  exportStateInterfaces() const override;

  std::vector<ethernetip_device_driver_interface::InterfaceDescription>
  exportCommandInterfaces() const override;

  double getStateValue(const std::string & interface_name) const override;
  void setCommandValue(const std::string & interface_name, double value) override;

private:
  ethernetip_master::DeviceConfig config_;
  std::unique_ptr<ethernetip_master::AssemblyMap> input_map_;
  std::unique_ptr<ethernetip_master::AssemblyMap> output_map_;

  /// Latest unpacked state values (key = field name).
  std::unordered_map<std::string, double> state_values_;
  /// Pending command values (key = field name).
  std::unordered_map<std::string, double> command_values_;
};

}  // namespace ethernetip_generic_io_driver

#endif  // ETHERNETIP_GENERIC_IO_DRIVER__GENERIC_IO_DEVICE_HPP_
