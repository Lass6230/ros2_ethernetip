// ==========================================================================
// ros2_control-level integration test against a running OpENer adapter.
//
// This test exercises the FULL ros2_control stack:
//   • hardware_interface::ResourceManager loads our EthernetIPSystem plugin
//   • The plugin loads the GenericIODevice driver plugin via pluginlib
//   • Lifecycle transitions: UNCONFIGURED → INACTIVE → ACTIVE
//   • Command interfaces (write) and state interfaces (read) go through
//     the hardware interface read()/write() methods
//   • The underlying EthernetIPMaster performs real ForwardOpen/ForwardClose
//     and cyclic IO against OpENer over the veth pair
//
// Prerequisites:
//   1. OpENer running at 10.10.10.2 (ns_opener namespace, veth pair)
//   2. All packages installed:  colcon build && source install/setup.bash
//   3. Run:  ./test_ros2_control_opener <path-to-yaml>
//
// Exit code: 0 = PASS, 1 = FAIL
// ==========================================================================

#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <hardware_interface/resource_manager.hpp>
#include <lifecycle_msgs/msg/state.hpp>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static int g_failures = 0;

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "  [FAIL] " << (msg) << "  (" #cond ")\n"; \
      ++g_failures; \
    } else { \
      std::cout << "  [PASS] " << (msg) << "\n"; \
    } \
  } while (0)

#define CHECK_NEAR(a, b, eps, msg) \
  CHECK(std::fabs((a) - (b)) < (eps), msg)

// ---------------------------------------------------------------------------
// Build a minimal URDF with the EthernetIPSystem hardware plugin.
//
// The <joint> declarations must match what the GenericIODevice plugin
// exports:  joint_name = device "name" from YAML,  interface_name = field
// "name" from YAML.
// ---------------------------------------------------------------------------
static std::string build_urdf(const std::string & yaml_path)
{
  std::ostringstream urdf;
  urdf << R"(<?xml version="1.0"?>
<robot name="test_eip_robot" xmlns:xacro="http://www.ros.org/wiki/xacro">

  <!-- Minimal kinematic chain so URDF is valid -->
  <link name="base_link">
    <visual>
      <geometry><box size="0.1 0.1 0.1"/></geometry>
    </visual>
  </link>

  <!-- ros2_control hardware description -->
  <ros2_control name="eip_test_system" type="system">
    <hardware>
      <plugin>ethernetip_hardware_interface/EthernetIPSystem</plugin>
      <param name="config_file">)" << yaml_path << R"(</param>
    </hardware>

    <!-- Use <gpio> for generic IO: doesn't need a URDF kinematic joint -->
    <gpio name="opener_veth">
      <state_interface name="mirror_float"/>
      <state_interface name="mirror_uint32"/>
      <state_interface name="mirror_int16"/>
      <state_interface name="mirror_uint8"/>
      <command_interface name="mirror_float"/>
      <command_interface name="mirror_uint32"/>
      <command_interface name="mirror_int16"/>
      <command_interface name="mirror_uint8"/>
    </gpio>
  </ros2_control>

</robot>
)";
  return urdf.str();
}

// ---------------------------------------------------------------------------
// TEST 1 – Hardware interface loads, exports correct interfaces, lifecycle OK
// ---------------------------------------------------------------------------
bool test_lifecycle(
  hardware_interface::ResourceManager & rm,
  const std::string & hw_name)
{
  std::cout << "\n=== TEST 1: ros2_control lifecycle ===\n";

  // Check component exists and starts UNCONFIGURED
  auto status = rm.get_components_status();
  auto it = status.find(hw_name);
  CHECK(it != status.end(), "Hardware component '" + hw_name + "' exists");
  if (it == status.end()) {
    return false;
  }

  CHECK(it->second.state.id() ==
        lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED,
        "Initial state is UNCONFIGURED");

  // Transition to INACTIVE (configure)
  rclcpp_lifecycle::State inactive(
    lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE, "inactive");
  auto rc = rm.set_component_state(hw_name, inactive);
  CHECK(rc == hardware_interface::return_type::OK,
        "Transition to INACTIVE (configure)");

  status = rm.get_components_status();
  CHECK(status[hw_name].state.id() ==
        lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE,
        "State is now INACTIVE");

  // After configure, interfaces should be available
  auto si = rm.available_state_interfaces();
  auto ci = rm.available_command_interfaces();
  std::cout << "  Available state interfaces:   " << si.size() << "\n";
  for (const auto & s : si) {
    std::cout << "    " << s << "\n";
  }
  std::cout << "  Available command interfaces: " << ci.size() << "\n";
  for (const auto & c : ci) {
    std::cout << "    " << c << "\n";
  }

  CHECK(rm.state_interface_is_available("opener_veth/mirror_float"),
        "state interface opener_veth/mirror_float available after configure");

  // Transition to ACTIVE (activate → ForwardOpen)
  rclcpp_lifecycle::State active(
    lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE, "active");
  rc = rm.set_component_state(hw_name, active);
  CHECK(rc == hardware_interface::return_type::OK,
        "Transition to ACTIVE (activate / ForwardOpen)");

  status = rm.get_components_status();
  CHECK(status[hw_name].state.id() ==
        lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE,
        "State is now ACTIVE");

  return true;
}

// ---------------------------------------------------------------------------
// TEST 2 – Write commands → read states via ros2_control interfaces
// ---------------------------------------------------------------------------
bool test_io_round_trip(hardware_interface::ResourceManager & rm)
{
  std::cout << "\n=== TEST 2: ros2_control IO round-trip (mirror) ===\n";

  // Claim command interfaces
  auto cmd_float  = rm.claim_command_interface("opener_veth/mirror_float");
  auto cmd_uint32 = rm.claim_command_interface("opener_veth/mirror_uint32");
  auto cmd_int16  = rm.claim_command_interface("opener_veth/mirror_int16");
  auto cmd_uint8  = rm.claim_command_interface("opener_veth/mirror_uint8");

  // Claim state interfaces
  auto st_float  = rm.claim_state_interface("opener_veth/mirror_float");
  auto st_uint32 = rm.claim_state_interface("opener_veth/mirror_uint32");
  auto st_int16  = rm.claim_state_interface("opener_veth/mirror_int16");
  auto st_uint8  = rm.claim_state_interface("opener_veth/mirror_uint8");

  // Write test values via command interfaces
  const double test_float  = 3.14;
  const double test_uint32 = 42.0;
  const double test_int16  = -123.0;
  const double test_uint8  = 171.0;  // 0xAB

  bool ok = true;
  ok &= cmd_float.set_value(test_float);
  ok &= cmd_uint32.set_value(test_uint32);
  ok &= cmd_int16.set_value(test_int16);
  ok &= cmd_uint8.set_value(test_uint8);
  CHECK(ok, "set_value on all command interfaces");

  // Run several write→read cycles so the hardware interface pushes the
  // output buffer, the cyclic thread sends it to OpENer, OpENer mirrors it,
  // and the next read cycle picks it up.
  rclcpp::Time now(0, 0, RCL_ROS_TIME);
  rclcpp::Duration period(0, 20000000);  // 20 ms

  // Initial write to push command values into the hardware
  rm.write(now, period);

  // Wait for several IO cycles (OpENer RPI = 100 ms, need ~3 round trips)
  std::cout << "  Waiting for IO mirror cycles (2.0 s) ...\n";
  for (int i = 0; i < 100; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    rm.write(now, period);
    rm.read(now, period);
  }

  // Read back state interface values
  auto read_float  = st_float.get_optional<double>();
  auto read_uint32 = st_uint32.get_optional<double>();
  auto read_int16  = st_int16.get_optional<double>();
  auto read_uint8  = st_uint8.get_optional<double>();

  CHECK(read_float.has_value(),  "state mirror_float has value");
  CHECK(read_uint32.has_value(), "state mirror_uint32 has value");
  CHECK(read_int16.has_value(),  "state mirror_int16 has value");
  CHECK(read_uint8.has_value(),  "state mirror_uint8 has value");

  double f_val  = read_float.value_or(0.0);
  double u32val = read_uint32.value_or(0.0);
  double i16val = read_int16.value_or(0.0);
  double u8val  = read_uint8.value_or(0.0);

  std::cout << "  mirror_float : wrote " << test_float
            << " , read " << f_val << "\n";
  std::cout << "  mirror_uint32: wrote " << test_uint32
            << " , read " << u32val << "\n";
  std::cout << "  mirror_int16 : wrote " << test_int16
            << " , read " << i16val << "\n";
  std::cout << "  mirror_uint8 : wrote " << test_uint8
            << " , read " << u8val << "\n";

  CHECK_NEAR(f_val, test_float, 1e-4,
             "mirror_float round-trip via ros2_control");
  CHECK_NEAR(u32val, test_uint32, 0.5,
             "mirror_uint32 round-trip via ros2_control");
  CHECK_NEAR(i16val, test_int16, 0.5,
             "mirror_int16 round-trip via ros2_control");
  CHECK_NEAR(u8val, test_uint8, 0.5,
             "mirror_uint8 round-trip via ros2_control");

  return true;
}

// ---------------------------------------------------------------------------
// TEST 3 – Deactivate (ForwardClose) and verify cleanup
// ---------------------------------------------------------------------------
bool test_deactivate(
  hardware_interface::ResourceManager & rm,
  const std::string & hw_name)
{
  std::cout << "\n=== TEST 3: ros2_control deactivate / cleanup ===\n";

  rclcpp_lifecycle::State inactive(
    lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE, "inactive");
  auto rc = rm.set_component_state(hw_name, inactive);
  CHECK(rc == hardware_interface::return_type::OK,
        "Transition ACTIVE → INACTIVE (ForwardClose)");

  auto status = rm.get_components_status();
  CHECK(status[hw_name].state.id() ==
        lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE,
        "State is INACTIVE after deactivate");

  // Cleanup
  rclcpp_lifecycle::State unconfigured(
    lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED, "unconfigured");
  rc = rm.set_component_state(hw_name, unconfigured);
  CHECK(rc == hardware_interface::return_type::OK,
        "Transition INACTIVE → UNCONFIGURED (cleanup)");

  return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0]
              << " <opener_test_devices.yaml>\n";
    return 1;
  }
  const std::string yaml_path = argv[1];

  std::cout << "==========================================================\n";
  std::cout << "  ros2_control Integration Test (OpENer)\n";
  std::cout << "  YAML : " << yaml_path << "\n";
  std::cout << "==========================================================\n";

  const std::string hw_name = "eip_test_system";

  // Build URDF and create ResourceManager
  std::string urdf = build_urdf(yaml_path);
  std::cout << "\n--- Generated URDF ---\n" << urdf << "---\n";

  try {
    auto clock = std::make_shared<rclcpp::Clock>(RCL_ROS_TIME);
    auto logger = rclcpp::get_logger("test_ros2_control");

    hardware_interface::ResourceManager rm(
      urdf, clock, logger,
      /*activate_all=*/false,
      /*update_rate=*/50);

    // Run the tests
    if (!test_lifecycle(rm, hw_name)) {
      std::cerr << "Lifecycle test failed, skipping IO tests.\n";
    } else {
      test_io_round_trip(rm);
      test_deactivate(rm, hw_name);
    }

    // Note: test_deactivate already transitions back to UNCONFIGURED,
    // so no need to call shutdown_components() (which would double-cleanup).
  } catch (const std::exception & ex) {
    std::cerr << "\n  [FATAL] Exception: " << ex.what() << "\n";
    ++g_failures;
  }

  std::cout << "\n==========================================================\n";
  if (g_failures == 0) {
    std::cout << "  ALL ros2_control CHECKS PASSED\n";
  } else {
    std::cout << "  " << g_failures << " CHECK(S) FAILED\n";
  }
  std::cout << "==========================================================\n";

  rclcpp::shutdown();
  return g_failures == 0 ? 0 : 1;
}
