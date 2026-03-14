#include "ethernetip_master/device_connection.hpp"

#include <algorithm>
#include <iostream>

#include <rclcpp/rclcpp.hpp>

// EIPScanner headers
#include <SessionInfo.h>
#include <ConnectionManager.h>
#include <IOConnection.h>
#include <cip/connectionManager/ConnectionParameters.h>
#include <cip/Types.h>

namespace ethernetip_master
{

// ---------------------------------------------------------------------------
// Pimpl holding EIPScanner objects
// ---------------------------------------------------------------------------
struct DeviceConnection::Impl
{
  std::shared_ptr<eipScanner::SessionInfo> session;
  std::shared_ptr<eipScanner::ConnectionManager> connection_manager;
  eipScanner::cip::connectionManager::ConnectionParameters conn_params;
  eipScanner::IOConnection::WPtr io_connection;
};

// ---------------------------------------------------------------------------
// ctor / destructor (must be here where Impl is complete)
// ---------------------------------------------------------------------------
DeviceConnection::DeviceConnection() = default;
DeviceConnection::~DeviceConnection() = default;

// ---------------------------------------------------------------------------
// configure
// ---------------------------------------------------------------------------
bool DeviceConnection::configure(const DeviceConfig & config)
{
  config_ = config;

  // Build assembly maps
  input_map_ = std::make_unique<AssemblyMap>(config_.inputs, config_.input_size);
  output_map_ = std::make_unique<AssemblyMap>(config_.outputs, config_.output_size);

  // Allocate buffers
  input_buffer_.resize(input_map_->bufferSize(), 0);
  output_buffer_.resize(output_map_->bufferSize(), 0);

  // Configure watchdog – default timeout = 10 × RPI
  watchdog_.configure(
    config_.name,
    config_.rpi_ms * 10.0,
    [](const std::string & name) {
      RCLCPP_WARN(
        rclcpp::get_logger("DeviceConnection"),
        "Watchdog timeout for device '%s'", name.c_str());
    });

  // Allocate implementation
  impl_ = std::make_unique<Impl>();

  state_ = State::kConfigured;
  RCLCPP_INFO(
    rclcpp::get_logger("DeviceConnection"),
    "Configured device '%s' @ %s  I/O asm %u/%u  rpi=%.1f ms  "
    "input_buf=%zu  output_buf=%zu",
    config_.name.c_str(), config_.ip.c_str(),
    config_.input_assembly, config_.output_assembly,
    config_.rpi_ms,
    input_buffer_.size(), output_buffer_.size());

  return true;
}

// ---------------------------------------------------------------------------
// connect  (ForwardOpen)
// ---------------------------------------------------------------------------
bool DeviceConnection::connect()
{
  if (state_ != State::kConfigured) {
    RCLCPP_ERROR(
      rclcpp::get_logger("DeviceConnection"),
      "Cannot connect device '%s': not in Configured state",
      config_.name.c_str());
    return false;
  }

  try {
    // Open TCP session (default EIP port 0xAF12 = 44818)
    impl_->session = std::make_shared<eipScanner::SessionInfo>(
      config_.ip, 0xAF12);

    // Create connection manager
    impl_->connection_manager = std::make_shared<eipScanner::ConnectionManager>();

    // Set up connection parameters
    auto & p = impl_->conn_params;
    p.connectionPath = {
      static_cast<uint8_t>(config_.config_assembly),
      static_cast<uint8_t>(config_.output_assembly),
      static_cast<uint8_t>(config_.input_assembly)};
    p.o2tRPI = static_cast<uint32_t>(config_.rpi_ms * 1000);  // microseconds
    p.t2oRPI = static_cast<uint32_t>(config_.rpi_ms * 1000);
    p.o2tNetworkConnectionParams =
      static_cast<uint32_t>(0x4000 | output_buffer_.size());  // fixed size
    p.t2oNetworkConnectionParams =
      static_cast<uint32_t>(0x4000 | input_buffer_.size());

    // ForwardOpen – returns a weak_ptr to the IO connection
    impl_->io_connection = impl_->connection_manager->forwardOpen(
      impl_->session, impl_->conn_params);

    // Register receive callback
    if (auto io = impl_->io_connection.lock()) {
      io->setReceiveDataListener(
        [this](eipScanner::cip::CipUdint /*seqNum*/,
               eipScanner::cip::CipUint /*dataType*/,
               const std::vector<uint8_t> & data) {
          updateInputBuffer(data.data(), data.size());
        });
    }

    state_ = State::kConnected;
    RCLCPP_INFO(
      rclcpp::get_logger("DeviceConnection"),
      "Connected to device '%s'", config_.name.c_str());
    return true;

  } catch (const std::exception & ex) {
    RCLCPP_ERROR(
      rclcpp::get_logger("DeviceConnection"),
      "ForwardOpen failed for '%s': %s", config_.name.c_str(), ex.what());
    state_ = State::kError;
    return false;
  }
}

// ---------------------------------------------------------------------------
// disconnect  (ForwardClose)
// ---------------------------------------------------------------------------
void DeviceConnection::disconnect()
{
  if (state_ != State::kConnected) {
    return;
  }
  try {
    impl_->connection_manager->forwardClose(impl_->session, impl_->io_connection);
  } catch (const std::exception & ex) {
    RCLCPP_WARN(
      rclcpp::get_logger("DeviceConnection"),
      "ForwardClose error for '%s': %s", config_.name.c_str(), ex.what());
  }
  state_ = State::kConfigured;
}

// ---------------------------------------------------------------------------
// buffer access
// ---------------------------------------------------------------------------
void DeviceConnection::getInputBuffer(std::vector<uint8_t> & dest) const
{
  std::lock_guard<std::mutex> lk(input_mutex_);
  dest = input_buffer_;
}

void DeviceConnection::setOutputBuffer(const std::vector<uint8_t> & src)
{
  std::lock_guard<std::mutex> lk(output_mutex_);
  output_buffer_ = src;
}

double DeviceConnection::readInput(const std::string & name) const
{
  std::lock_guard<std::mutex> lk(input_mutex_);
  return input_map_->getValue(name, input_buffer_.data());
}

void DeviceConnection::writeOutput(const std::string & name, double value)
{
  std::lock_guard<std::mutex> lk(output_mutex_);
  output_map_->setValue(name, output_buffer_.data(), value);
}

void DeviceConnection::updateInputBuffer(const uint8_t * data, std::size_t len)
{
  std::lock_guard<std::mutex> lk(input_mutex_);
  std::size_t copy_len = std::min(len, input_buffer_.size());
  std::memcpy(input_buffer_.data(), data, copy_len);
  watchdog_.feed();
}

const std::vector<uint8_t> & DeviceConnection::outputBuffer() const
{
  // Caller is expected to hold output_mutex_ externally if needed.
  return output_buffer_;
}

std::string DeviceConnection::stateString() const
{
  switch (state_) {
    case State::kUnconfigured: return "Unconfigured";
    case State::kConfigured:   return "Configured";
    case State::kConnected:    return "Connected";
    case State::kError:        return "Error";
    default:                   return "Unknown";
  }
}

}  // namespace ethernetip_master
