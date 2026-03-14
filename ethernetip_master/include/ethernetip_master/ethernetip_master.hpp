#ifndef ETHERNETIP_MASTER__ETHERNETIP_MASTER_HPP_
#define ETHERNETIP_MASTER__ETHERNETIP_MASTER_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ethernetip_master/device_config.hpp"
#include "ethernetip_master/device_connection.hpp"

namespace ethernetip_master
{

/// Top‑level scanner object.
/// Manages all device connections and owns the cyclic IO thread.
class EthernetIPMaster
{
public:
  EthernetIPMaster();
  ~EthernetIPMaster();

  // Non‑copyable / non‑movable
  EthernetIPMaster(const EthernetIPMaster &) = delete;
  EthernetIPMaster & operator=(const EthernetIPMaster &) = delete;

  // ----- configuration ------------------------------------------------------
  /// Load device configurations from a YAML file.
  bool loadConfig(const std::string & yaml_path);

  /// Add a single device.
  bool addDevice(const DeviceConfig & config);

  /// Return all configured devices.
  std::vector<std::unique_ptr<DeviceConnection>> & devices() { return devices_; }
  const std::vector<std::unique_ptr<DeviceConnection>> & devices() const { return devices_; }

  // ----- lifecycle -----------------------------------------------------------
  /// Open TCP sessions and ForwardOpen for every device.
  bool start();

  /// ForwardClose and tear down.
  void stop();

  /// True while the cyclic IO thread is running.
  bool isRunning() const { return running_.load(); }

  // ----- synchronous read/write used by ros2_control -------------------------
  /// Copy latest input buffers from the cyclic thread into the device
  /// connections.  Non‑blocking (swaps shared buffers).
  void read();

  /// Push the device output buffers to the cyclic thread for the next
  /// UDP send.  Non‑blocking (swaps shared buffers).
  void write();

private:
  /// The real‑time cyclic IO loop (runs in its own thread).
  void cyclicLoop();

  std::vector<std::unique_ptr<DeviceConnection>> devices_;
  std::thread cyclic_thread_;
  std::atomic<bool> running_{false};
};

}  // namespace ethernetip_master

#endif  // ETHERNETIP_MASTER__ETHERNETIP_MASTER_HPP_
