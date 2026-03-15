#include "ethernetip_hardware_interface/ethernetip_system.hpp"

#include <algorithm>
#include <limits>
#include <sstream>

#include "pluginlib/class_loader.hpp"
#include "rclcpp/rclcpp.hpp"

namespace ethernetip_hardware_interface
{

using CallbackReturn = hardware_interface::CallbackReturn;

// ---------------------------------------------------------------------------
// on_init
// ---------------------------------------------------------------------------
CallbackReturn EthernetIPSystem::on_init(
  const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SystemInterface::on_init(info) !=
    CallbackReturn::SUCCESS)
  {
    return CallbackReturn::ERROR;
  }

  // Read "config_file" parameter from the <hardware> tag
  auto it = info.hardware_parameters.find("config_file");
  if (it == info.hardware_parameters.end()) {
    RCLCPP_ERROR(
      rclcpp::get_logger("EthernetIPSystem"),
      "Missing 'config_file' hardware parameter");
    return CallbackReturn::ERROR;
  }
  config_file_ = it->second;

  RCLCPP_INFO(
    rclcpp::get_logger("EthernetIPSystem"),
    "on_init: config_file = %s", config_file_.c_str());

  // ---- Early initialisation ------------------------------------------------
  // The ResourceManager calls export_state/command_interfaces() right after
  // on_init(), *before* the configure lifecycle transition.  Therefore we
  // must parse the YAML and load device-driver plugins here so that the
  // interface storage vectors are populated in time.

  // 1. Parse the YAML
  if (!master_.loadConfig(config_file_)) {
    RCLCPP_ERROR(
      rclcpp::get_logger("EthernetIPSystem"),
      "Failed to load config '%s'", config_file_.c_str());
    return CallbackReturn::ERROR;
  }

  // 2. Load device-driver plugins via pluginlib
  pluginlib::ClassLoader<ethernetip_device_driver_interface::EthernetIPDevice>
    loader("ethernetip_device_driver_interface",
           "ethernetip_device_driver_interface::EthernetIPDevice");

  device_plugins_.clear();
  for (auto & dev : master_.devices()) {
    try {
      auto plugin = loader.createSharedInstance(dev->config().plugin_name);
      if (!plugin->configure(dev->config())) {
        RCLCPP_ERROR(
          rclcpp::get_logger("EthernetIPSystem"),
          "Device plugin '%s' configure() failed for device '%s'",
          dev->config().plugin_name.c_str(), dev->config().name.c_str());
        return CallbackReturn::ERROR;
      }
      device_plugins_.push_back(plugin);
    } catch (const pluginlib::PluginlibException & ex) {
      RCLCPP_ERROR(
        rclcpp::get_logger("EthernetIPSystem"),
        "Failed to load plugin '%s': %s",
        dev->config().plugin_name.c_str(), ex.what());
      return CallbackReturn::ERROR;
    }
  }

  // 3. Build the state/command value storage
  state_values_.clear();
  command_values_.clear();
  state_index_.clear();
  command_index_.clear();

  for (const auto & plugin : device_plugins_) {
    for (const auto & si : plugin->exportStateInterfaces()) {
      std::string key = si.joint_name + "/" + si.interface_name;
      state_index_[key] = state_values_.size();
      state_values_.push_back(0.0);
    }
    for (const auto & ci : plugin->exportCommandInterfaces()) {
      std::string key = ci.joint_name + "/" + ci.interface_name;
      command_index_[key] = command_values_.size();
      command_values_.push_back(0.0);
    }
  }

  RCLCPP_INFO(
    rclcpp::get_logger("EthernetIPSystem"),
    "Initialised %zu device(s), %zu state ifaces, %zu cmd ifaces",
    device_plugins_.size(), state_values_.size(), command_values_.size());

  return CallbackReturn::SUCCESS;
}

// ---------------------------------------------------------------------------
// on_configure
// ---------------------------------------------------------------------------
CallbackReturn EthernetIPSystem::on_configure(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // All heavy lifting (YAML parse, plugin load, interface storage) was done
  // in on_init() because the ResourceManager calls export_*_interfaces()
  // right after on_init(), before on_configure().
  //
  // on_configure() simply validates that everything is ready.
  if (device_plugins_.empty()) {
    RCLCPP_ERROR(
      rclcpp::get_logger("EthernetIPSystem"),
      "No device plugins loaded – was on_init() successful?");
    return CallbackReturn::ERROR;
  }

  RCLCPP_INFO(
    rclcpp::get_logger("EthernetIPSystem"),
    "Configured: %zu device(s) ready", device_plugins_.size());

  return CallbackReturn::SUCCESS;
}

// ---------------------------------------------------------------------------
// on_activate
// ---------------------------------------------------------------------------
CallbackReturn EthernetIPSystem::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // Set commands to NaN (no command sent until controllers write)
  std::fill(
    command_values_.begin(), command_values_.end(),
    std::numeric_limits<double>::quiet_NaN());

  if (!master_.start()) {
    RCLCPP_ERROR(
      rclcpp::get_logger("EthernetIPSystem"),
      "Failed to start EtherNet/IP scanner");
    return CallbackReturn::ERROR;
  }

  RCLCPP_INFO(rclcpp::get_logger("EthernetIPSystem"), "Activated");
  return CallbackReturn::SUCCESS;
}

// ---------------------------------------------------------------------------
// on_deactivate
// ---------------------------------------------------------------------------
CallbackReturn EthernetIPSystem::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  master_.stop();
  RCLCPP_INFO(rclcpp::get_logger("EthernetIPSystem"), "Deactivated");
  return CallbackReturn::SUCCESS;
}

// ---------------------------------------------------------------------------
// on_cleanup
// ---------------------------------------------------------------------------
CallbackReturn EthernetIPSystem::on_cleanup(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  device_plugins_.clear();
  state_values_.clear();
  command_values_.clear();
  RCLCPP_INFO(rclcpp::get_logger("EthernetIPSystem"), "Cleaned up");
  return CallbackReturn::SUCCESS;
}

// ---------------------------------------------------------------------------
// on_error
// ---------------------------------------------------------------------------
CallbackReturn EthernetIPSystem::on_error(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  master_.stop();
  RCLCPP_ERROR(rclcpp::get_logger("EthernetIPSystem"), "Error state entered");
  return CallbackReturn::SUCCESS;
}

// ---------------------------------------------------------------------------
// export_state_interfaces
// ---------------------------------------------------------------------------
std::vector<hardware_interface::StateInterface>
EthernetIPSystem::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> interfaces;

  for (const auto & plugin : device_plugins_) {
    for (const auto & si : plugin->exportStateInterfaces()) {
      std::string key = si.joint_name + "/" + si.interface_name;
      auto idx = state_index_.at(key);
      interfaces.emplace_back(
        si.joint_name, si.interface_name, &state_values_[idx]);
    }
  }
  return interfaces;
}

// ---------------------------------------------------------------------------
// export_command_interfaces
// ---------------------------------------------------------------------------
std::vector<hardware_interface::CommandInterface>
EthernetIPSystem::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> interfaces;

  for (const auto & plugin : device_plugins_) {
    for (const auto & ci : plugin->exportCommandInterfaces()) {
      std::string key = ci.joint_name + "/" + ci.interface_name;
      auto idx = command_index_.at(key);
      interfaces.emplace_back(
        ci.joint_name, ci.interface_name, &command_values_[idx]);
    }
  }
  return interfaces;
}

// ---------------------------------------------------------------------------
// read
// ---------------------------------------------------------------------------
hardware_interface::return_type EthernetIPSystem::read(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  // 1. Let the master swap in the latest UDP data
  master_.read();

  // 2. For each device, call the plugin's processInput() then copy values
  for (std::size_t i = 0; i < device_plugins_.size(); ++i) {
    auto & dev = *master_.devices()[i];
    auto & plugin = device_plugins_[i];

    std::vector<uint8_t> buf;
    dev.getInputBuffer(buf);
    plugin->processInput(buf.data(), buf.size());

    // Copy state values from plugin into our storage
    for (const auto & si : plugin->exportStateInterfaces()) {
      std::string key = si.joint_name + "/" + si.interface_name;
      auto idx = state_index_.at(key);
      state_values_[idx] = plugin->getStateValue(si.field_name);
    }
  }

  return hardware_interface::return_type::OK;
}

// ---------------------------------------------------------------------------
// write
// ---------------------------------------------------------------------------
hardware_interface::return_type EthernetIPSystem::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  // 1. Push command values into each plugin, then prepareOutput
  for (std::size_t i = 0; i < device_plugins_.size(); ++i) {
    auto & dev = *master_.devices()[i];
    auto & plugin = device_plugins_[i];

    // Write commands from ros2_control into plugin
    for (const auto & ci : plugin->exportCommandInterfaces()) {
      std::string key = ci.joint_name + "/" + ci.interface_name;
      auto idx = command_index_.at(key);
      double val = command_values_[idx];
      if (!std::isnan(val)) {
        plugin->setCommandValue(ci.field_name, val);
      }
    }

    // Let the plugin fill the output buffer
    std::vector<uint8_t> buf(dev.config().output_size, 0);
    plugin->prepareOutput(buf.data(), buf.size());
    dev.setOutputBuffer(buf);
  }

  // 2. Tell the master to push data to the cyclic thread
  master_.write();

  return hardware_interface::return_type::OK;
}

}  // namespace ethernetip_hardware_interface

// ---------------------------------------------------------------------------
// Register as a ros2_control plugin
// ---------------------------------------------------------------------------
#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(
  ethernetip_hardware_interface::EthernetIPSystem,
  hardware_interface::SystemInterface)
