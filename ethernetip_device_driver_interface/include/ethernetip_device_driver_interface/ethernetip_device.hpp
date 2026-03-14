#ifndef ETHERNETIP_DEVICE_DRIVER_INTERFACE__ETHERNETIP_DEVICE_HPP_
#define ETHERNETIP_DEVICE_DRIVER_INTERFACE__ETHERNETIP_DEVICE_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "ethernetip_master/device_config.hpp"

namespace ethernetip_device_driver_interface
{

/// Describes a single state or command interface to be exported into
/// ros2_control.
struct InterfaceDescription
{
  std::string joint_name;       ///< Joint / sensor / gpio name
  std::string interface_name;   ///< e.g. "position", "velocity", "effort"
  std::string field_name;       ///< Matching field name in the assembly map
};

/// Pure‑virtual base class for EtherNet/IP device‑driver plugins.
///
/// Implementations are loaded via pluginlib by the hardware interface.  Each
/// plugin knows how to interpret its device's assembly layout and how to
/// export ros2_control interfaces.
class EthernetIPDevice
{
public:
  virtual ~EthernetIPDevice() = default;

  // ----- lifecycle ----------------------------------------------------------

  /// Called once with the device configuration parsed from YAML.
  /// @return true on success.
  virtual bool configure(const ethernetip_master::DeviceConfig & config) = 0;

  // ----- cyclic data --------------------------------------------------------

  /// Process a newly received input assembly buffer.
  /// Called every cycle in the ros2_control read() path.
  /// @param data  Pointer to the raw input assembly bytes.
  /// @param size  Number of bytes in the buffer.
  virtual void processInput(const uint8_t * data, std::size_t size) = 0;

  /// Fill the output assembly buffer before it is sent.
  /// Called every cycle in the ros2_control write() path.
  /// @param data  Pointer to the raw output assembly bytes.
  /// @param size  Number of bytes in the buffer.
  virtual void prepareOutput(uint8_t * data, std::size_t size) = 0;

  // ----- ros2_control interfaces -------------------------------------------

  /// Return the list of state interfaces this device exports.
  virtual std::vector<InterfaceDescription> exportStateInterfaces() const = 0;

  /// Return the list of command interfaces this device exports.
  virtual std::vector<InterfaceDescription> exportCommandInterfaces() const = 0;

  // ----- data access -------------------------------------------------------

  /// Read the current value of a state interface.
  /// @param interface_name  Fully‑qualified <joint>/<interface> or just the
  ///                        field name.
  virtual double getStateValue(const std::string & interface_name) const = 0;

  /// Set the value of a command interface.
  virtual void setCommandValue(const std::string & interface_name, double value) = 0;
};

}  // namespace ethernetip_device_driver_interface

#endif  // ETHERNETIP_DEVICE_DRIVER_INTERFACE__ETHERNETIP_DEVICE_HPP_
