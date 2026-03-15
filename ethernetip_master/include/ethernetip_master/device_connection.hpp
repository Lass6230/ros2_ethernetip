#ifndef ETHERNETIP_MASTER__DEVICE_CONNECTION_HPP_
#define ETHERNETIP_MASTER__DEVICE_CONNECTION_HPP_

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ethernetip_master/assembly_map.hpp"
#include "ethernetip_master/device_config.hpp"
#include "ethernetip_master/watchdog.hpp"

namespace ethernetip_master
{

/// Represents an active implicit‑IO connection to one EtherNet/IP target.
/// Owns the raw assembly buffers and the assembly maps for packing/unpacking.
class DeviceConnection
{
public:
  enum class State
  {
    kUnconfigured,
    kConfigured,
    kConnected,
    kError
  };

  DeviceConnection();
  ~DeviceConnection();

  // Non‑copyable, non‑movable (contains mutex members)
  DeviceConnection(const DeviceConnection &) = delete;
  DeviceConnection & operator=(const DeviceConnection &) = delete;
  DeviceConnection(DeviceConnection &&) = delete;
  DeviceConnection & operator=(DeviceConnection &&) = delete;

  /// Initialise from a parsed device config.
  bool configure(const DeviceConfig & config);

  /// Open the CIP ForwardOpen connection via EIPScanner.
  bool connect();

  /// Gracefully close with ForwardClose.
  void disconnect();

  // --- cyclic data access ---------------------------------------------------
  /// Copy the latest received input assembly into \p dest.
  void getInputBuffer(std::vector<uint8_t> & dest) const;
  /// Write the output assembly from \p src.
  void setOutputBuffer(const std::vector<uint8_t> & src);

  /// Thread‑safe convenience wrappers that go through the AssemblyMap.
  double readInput(const std::string & name) const;
  void writeOutput(const std::string & name, double value);

  /// Called by the master after receiving new UDP input data.
  void updateInputBuffer(const uint8_t * data, std::size_t len);
  /// Called by the master before sending UDP output data.
  const std::vector<uint8_t> & outputBuffer() const;

  /// Process one IO cycle: push output data and handle UDP send/receive.
  /// Must be called periodically (at least as often as the RPI).
  void tick(std::chrono::milliseconds timeout);

  // --- accessors ------------------------------------------------------------
  const DeviceConfig & config() const { return config_; }
  State state() const { return state_; }
  std::string stateString() const;

  /// Watchdog access.
  Watchdog & watchdog() { return watchdog_; }
  const Watchdog & watchdog() const { return watchdog_; }

  /// Assembly maps (available after configure()).
  const AssemblyMap & inputMap() const { return *input_map_; }
  const AssemblyMap & outputMap() const { return *output_map_; }

private:
  DeviceConfig config_;
  State state_{State::kUnconfigured};

  std::unique_ptr<AssemblyMap> input_map_;
  std::unique_ptr<AssemblyMap> output_map_;

  mutable std::mutex input_mutex_;
  mutable std::mutex output_mutex_;

  std::vector<uint8_t> input_buffer_;
  std::vector<uint8_t> output_buffer_;

  Watchdog watchdog_;

  // EIPScanner handles (opaque here, defined in .cpp)
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ethernetip_master

#endif  // ETHERNETIP_MASTER__DEVICE_CONNECTION_HPP_
