#include "ethernetip_generic_io_driver/generic_io_device.hpp"

#include <cstring>

namespace ethernetip_generic_io_driver
{

using ethernetip_device_driver_interface::InterfaceDescription;

// ---------------------------------------------------------------------------
// configure
// ---------------------------------------------------------------------------
bool GenericIODevice::configure(
  const ethernetip_master::DeviceConfig & config)
{
  config_ = config;

  input_map_ = std::make_unique<ethernetip_master::AssemblyMap>(
    config_.inputs, config_.input_size);
  output_map_ = std::make_unique<ethernetip_master::AssemblyMap>(
    config_.outputs, config_.output_size);

  // Pre‑fill value maps
  for (const auto & f : config_.inputs) {
    state_values_[f.name] = 0.0;
  }
  for (const auto & f : config_.outputs) {
    command_values_[f.name] = 0.0;
  }

  return true;
}

// ---------------------------------------------------------------------------
// processInput
// ---------------------------------------------------------------------------
void GenericIODevice::processInput(const uint8_t * data, std::size_t size)
{
  if (!input_map_ || size < input_map_->bufferSize()) {
    return;
  }
  for (const auto & f : config_.inputs) {
    state_values_[f.name] = input_map_->getValue(f.name, data);
  }
}

// ---------------------------------------------------------------------------
// prepareOutput
// ---------------------------------------------------------------------------
void GenericIODevice::prepareOutput(uint8_t * data, std::size_t size)
{
  if (!output_map_ || size < output_map_->bufferSize()) {
    return;
  }
  // Zero the buffer first
  std::memset(data, 0, size);
  for (const auto & f : config_.outputs) {
    auto it = command_values_.find(f.name);
    if (it != command_values_.end()) {
      output_map_->setValue(f.name, data, it->second);
    }
  }
}

// ---------------------------------------------------------------------------
// export interfaces
// ---------------------------------------------------------------------------
std::vector<InterfaceDescription>
GenericIODevice::exportStateInterfaces() const
{
  std::vector<InterfaceDescription> result;
  for (const auto & f : config_.inputs) {
    InterfaceDescription desc;
    desc.joint_name = config_.name;
    desc.interface_name = f.name;
    desc.field_name = f.name;
    result.push_back(desc);
  }
  return result;
}

std::vector<InterfaceDescription>
GenericIODevice::exportCommandInterfaces() const
{
  std::vector<InterfaceDescription> result;
  for (const auto & f : config_.outputs) {
    InterfaceDescription desc;
    desc.joint_name = config_.name;
    desc.interface_name = f.name;
    desc.field_name = f.name;
    result.push_back(desc);
  }
  return result;
}

// ---------------------------------------------------------------------------
// value access
// ---------------------------------------------------------------------------
double GenericIODevice::getStateValue(const std::string & interface_name) const
{
  auto it = state_values_.find(interface_name);
  return (it != state_values_.end()) ? it->second : 0.0;
}

void GenericIODevice::setCommandValue(
  const std::string & interface_name, double value)
{
  command_values_[interface_name] = value;
}

}  // namespace ethernetip_generic_io_driver

// ---------------------------------------------------------------------------
// Register plugin
// ---------------------------------------------------------------------------
#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(
  ethernetip_generic_io_driver::GenericIODevice,
  ethernetip_device_driver_interface::EthernetIPDevice)
