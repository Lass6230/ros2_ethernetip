#include "ethernetip_master/ethernetip_master.hpp"

#include <chrono>
#include <fstream>
#include <iostream>

#include <rclcpp/rclcpp.hpp>
#include <yaml-cpp/yaml.h>

namespace ethernetip_master
{

// ---------------------------------------------------------------------------
// helpers – parse YAML
// ---------------------------------------------------------------------------
namespace
{

std::vector<FieldMapping> parseFields(const YAML::Node & node)
{
  std::vector<FieldMapping> fields;
  if (!node || !node.IsSequence()) {
    return fields;
  }
  for (const auto & f : node) {
    FieldMapping fm;
    fm.name = f["name"].as<std::string>();
    fm.type = f["type"].as<std::string>("float32");
    fm.offset = f["offset"].as<std::size_t>(0);
    fm.bit_offset = f["bit_offset"].as<std::size_t>(0);
    fields.push_back(fm);
  }
  return fields;
}

}  // namespace

// ---------------------------------------------------------------------------
// ctor / dtor
// ---------------------------------------------------------------------------
EthernetIPMaster::EthernetIPMaster() = default;

EthernetIPMaster::~EthernetIPMaster()
{
  stop();
}

// ---------------------------------------------------------------------------
// loadConfig
// ---------------------------------------------------------------------------
bool EthernetIPMaster::loadConfig(const std::string & yaml_path)
{
  try {
    YAML::Node root = YAML::LoadFile(yaml_path);
    const auto & devs = root["devices"];
    if (!devs || !devs.IsSequence()) {
      RCLCPP_ERROR(
        rclcpp::get_logger("EthernetIPMaster"),
        "YAML file '%s' has no 'devices' sequence", yaml_path.c_str());
      return false;
    }

    for (const auto & d : devs) {
      DeviceConfig cfg;
      cfg.name = d["name"].as<std::string>();
      cfg.ip = d["ip"].as<std::string>();
      cfg.input_assembly = d["input_assembly"].as<uint16_t>(0);
      cfg.output_assembly = d["output_assembly"].as<uint16_t>(0);
      cfg.config_assembly = d["config_assembly"].as<uint16_t>(0);
      cfg.rpi_ms = d["rpi"].as<double>(8.0);

      // Parse the rpi string – support "8ms" shorthand
      if (d["rpi"] && d["rpi"].IsScalar()) {
        std::string rpi_str = d["rpi"].as<std::string>();
        // Strip trailing "ms" if present
        auto pos = rpi_str.find("ms");
        if (pos != std::string::npos) {
          rpi_str = rpi_str.substr(0, pos);
        }
        cfg.rpi_ms = std::stod(rpi_str);
      }

      cfg.input_size = d["input_size"].as<std::size_t>(0);
      cfg.output_size = d["output_size"].as<std::size_t>(0);

      cfg.inputs = parseFields(d["inputs"]);
      cfg.outputs = parseFields(d["outputs"]);

      cfg.plugin_name = d["plugin"].as<std::string>(
        "ethernetip_generic_io_driver/GenericIODevice");

      if (!addDevice(cfg)) {
        RCLCPP_ERROR(
          rclcpp::get_logger("EthernetIPMaster"),
          "Failed to configure device '%s'", cfg.name.c_str());
        return false;
      }
    }
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(
      rclcpp::get_logger("EthernetIPMaster"),
      "Error loading YAML '%s': %s", yaml_path.c_str(), ex.what());
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// addDevice
// ---------------------------------------------------------------------------
bool EthernetIPMaster::addDevice(const DeviceConfig & config)
{
  auto conn = std::make_unique<DeviceConnection>();
  if (!conn->configure(config)) {
    return false;
  }
  devices_.push_back(std::move(conn));
  return true;
}

// ---------------------------------------------------------------------------
// start / stop
// ---------------------------------------------------------------------------
bool EthernetIPMaster::start()
{
  // Open connections
  for (auto & dev : devices_) {
    if (!dev->connect()) {
      RCLCPP_ERROR(
        rclcpp::get_logger("EthernetIPMaster"),
        "Failed to connect device '%s' at %s",
        dev->config().name.c_str(), dev->config().ip.c_str());
      return false;
    }
  }

  // Start cyclic IO thread
  running_ = true;
  cyclic_thread_ = std::thread(&EthernetIPMaster::cyclicLoop, this);

  RCLCPP_INFO(
    rclcpp::get_logger("EthernetIPMaster"),
    "Started cyclic IO with %zu device(s)", devices_.size());
  return true;
}

void EthernetIPMaster::stop()
{
  running_ = false;
  if (cyclic_thread_.joinable()) {
    cyclic_thread_.join();
  }
  for (auto & dev : devices_) {
    dev->disconnect();
  }
  RCLCPP_INFO(rclcpp::get_logger("EthernetIPMaster"), "Stopped");
}

// ---------------------------------------------------------------------------
// read / write  (called from the ros2_control thread)
// ---------------------------------------------------------------------------
void EthernetIPMaster::read()
{
  // In a full implementation the cyclic thread stores received UDP data
  // into shared double‑buffers.  Here we simply propagate.
  for (auto & dev : devices_) {
    dev->watchdog().check();
  }
}

void EthernetIPMaster::write()
{
  // Output buffers are already populated by the hardware interface's
  // write() via DeviceConnection::writeOutput().
}

// ---------------------------------------------------------------------------
// cyclicLoop
// ---------------------------------------------------------------------------
void EthernetIPMaster::cyclicLoop()
{
  // Determine the shortest RPI among all devices.
  double min_rpi_ms = 8.0;
  for (const auto & dev : devices_) {
    if (dev->config().rpi_ms < min_rpi_ms) {
      min_rpi_ms = dev->config().rpi_ms;
    }
  }
  const auto period = std::chrono::microseconds(
    static_cast<int64_t>(min_rpi_ms * 1000.0));

  RCLCPP_INFO(
    rclcpp::get_logger("EthernetIPMaster"),
    "Cyclic loop running at %.1f ms period", min_rpi_ms);

  while (running_) {
    const auto start = std::chrono::steady_clock::now();

    // ---- process IO for each device (send output, receive input) ----------
    for (auto & dev : devices_) {
      dev->tick(std::chrono::milliseconds(1));
    }

    // ---- sleep until next period -----------------------------------------
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed < period) {
      std::this_thread::sleep_for(period - elapsed);
    }
  }
}

}  // namespace ethernetip_master
