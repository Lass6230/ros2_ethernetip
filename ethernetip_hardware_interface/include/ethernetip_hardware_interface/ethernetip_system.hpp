#ifndef ETHERNETIP_HARDWARE_INTERFACE__ETHERNETIP_SYSTEM_HPP_
#define ETHERNETIP_HARDWARE_INTERFACE__ETHERNETIP_SYSTEM_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "ethernetip_master/ethernetip_master.hpp"
#include "ethernetip_device_driver_interface/ethernetip_device.hpp"

namespace ethernetip_hardware_interface
{

/// ros2_control SystemInterface that acts as a generic EtherNet/IP scanner.
///
/// Lifecycle:
///   on_init   → load YAML config, create device plugins
///   on_activate → ForwardOpen on all devices, start cyclic IO
///   read()    → copy latest input assembly data into state interfaces
///   write()   → push command interface values into output assembly buffers
///   on_deactivate → ForwardClose, stop cyclic IO
class EthernetIPSystem : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(EthernetIPSystem)

  // ----- SystemInterface overrides ------------------------------------------

  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareInfo & info) override;

  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_cleanup(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_error(
    const rclcpp_lifecycle::State & previous_state) override;

  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  /// The EtherNet/IP scanner.
  ethernetip_master::EthernetIPMaster master_;

  /// Loaded device‑driver plugins (one per device in the YAML).
  std::vector<
    std::shared_ptr<ethernetip_device_driver_interface::EthernetIPDevice>>
    device_plugins_;

  /// Path to the YAML device description file.
  std::string config_file_;

  // Storage for state/command values (ros2_control owns the pointers).
  std::vector<double> state_values_;
  std::vector<double> command_values_;

  /// Map from fully‑qualified interface name → index into state_values_.
  std::unordered_map<std::string, std::size_t> state_index_;
  /// Map from fully‑qualified interface name → index into command_values_.
  std::unordered_map<std::string, std::size_t> command_index_;
};

}  // namespace ethernetip_hardware_interface

#endif  // ETHERNETIP_HARDWARE_INTERFACE__ETHERNETIP_SYSTEM_HPP_
