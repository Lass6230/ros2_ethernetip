// Integration test: connect to a running OpENer adapter on 127.0.0.1
//
// Prerequisites:
//   1.  Build OpENer   →  ./build_opener.sh
//   2.  Start OpENer   →  sudo <build>/src/ports/POSIX/OpENer lo
//   3.  Run this test  →  ./test_opener_connection <path‑to‑yaml>
//
// The test exercises:
//   A) Explicit messaging – read the Identity Object product name
//   B) Implicit messaging – ForwardOpen, write output data, verify the
//      OpENer mirror copies it to the input assembly, ForwardClose
//
// Exit code:  0 = PASS, 1 = FAIL

#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <thread>

#include <rclcpp/rclcpp.hpp>

#include "ethernetip_master/ethernetip_master.hpp"
#include "ethernetip_master/explicit_messaging.hpp"

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
// TEST A – Explicit messaging
// ---------------------------------------------------------------------------
bool test_explicit_messaging(const std::string & ip)
{
  std::cout << "\n=== TEST A: Explicit messaging ===\n";

  ethernetip_master::ExplicitMessaging em;

  bool opened = em.openSession(ip);
  CHECK(opened, "openSession to " + ip);
  if (!opened) {
    return false;
  }

  CHECK(em.isConnected(), "session is connected");

  // Read Identity Object – Class 0x01, Instance 1, Attribute 7 (Product Name)
  std::string identity = em.readIdentity();
  std::cout << "  Identity: \"" << identity << "\"\n";
  CHECK(identity != "<unknown>" && identity != "<malformed>",
        "readIdentity returned a valid product name");

  // Read Vendor ID – Class 0x01, Instance 1, Attribute 1 (uint16)
  auto vendor = em.readAttribute(0x01, 1, 1);
  CHECK(vendor.size() >= 2, "readAttribute(Identity, VendorID) returned data");

  em.closeSession();
  CHECK(!em.isConnected(), "session closed");

  return true;
}

// ---------------------------------------------------------------------------
// TEST B – Implicit messaging (ForwardOpen + IO data exchange)
// ---------------------------------------------------------------------------
bool test_implicit_messaging(const std::string & yaml_path)
{
  std::cout << "\n=== TEST B: Implicit messaging (ForwardOpen + IO) ===\n";

  ethernetip_master::EthernetIPMaster master;

  bool loaded = master.loadConfig(yaml_path);
  CHECK(loaded, "loadConfig from YAML");
  if (!loaded) {
    return false;
  }

  CHECK(master.devices().size() == 1u, "exactly one device configured");

  auto & dev = *master.devices().front();
  CHECK(dev.state() == ethernetip_master::DeviceConnection::State::kConfigured,
        "device is in Configured state");

  // ---- start (ForwardOpen + cyclic thread) --------------------------------
  bool started = master.start();
  CHECK(started, "master.start() (ForwardOpen)");
  if (!started) {
    return false;
  }

  CHECK(dev.state() == ethernetip_master::DeviceConnection::State::kConnected,
        "device is in Connected state after start()");
  CHECK(master.isRunning(), "cyclic IO thread is running");

  // ---- write known output values ------------------------------------------
  const float   test_float  = 3.14f;
  const int32_t test_uint32 = 42;
  const int16_t test_int16  = -123;
  const uint8_t test_uint8  = 0xAB;

  dev.writeOutput("mirror_float",  static_cast<double>(test_float));
  dev.writeOutput("mirror_uint32", static_cast<double>(test_uint32));
  dev.writeOutput("mirror_int16",  static_cast<double>(test_int16));
  dev.writeOutput("mirror_uint8",  static_cast<double>(test_uint8));

  // ---- wait for a few IO cycles so OpENer mirrors the data ----------------
  // OpENer mirrors output→input in AfterAssemblyDataReceived; we need
  // at least 2–3 full RPI cycles for the round‑trip.
  std::cout << "  Waiting for IO cycles (1.5 s) …\n";
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  master.read();  // update watchdog / sync buffers

  // ---- read back input values (should be mirrored) ------------------------
  double read_float  = dev.readInput("mirror_float");
  double read_uint32 = dev.readInput("mirror_uint32");
  double read_int16  = dev.readInput("mirror_int16");
  double read_uint8  = dev.readInput("mirror_uint8");

  std::cout << "  mirror_float : wrote " << test_float
            << " , read " << read_float << "\n";
  std::cout << "  mirror_uint32: wrote " << test_uint32
            << " , read " << read_uint32 << "\n";
  std::cout << "  mirror_int16 : wrote " << test_int16
            << " , read " << read_int16 << "\n";
  std::cout << "  mirror_uint8 : wrote " << static_cast<int>(test_uint8)
            << " , read " << read_uint8 << "\n";

  CHECK_NEAR(read_float, static_cast<double>(test_float), 1e-5,
             "mirror_float round-trip");
  CHECK_NEAR(read_uint32, static_cast<double>(test_uint32), 0.5,
             "mirror_uint32 round-trip");
  CHECK_NEAR(read_int16, static_cast<double>(test_int16), 0.5,
             "mirror_int16 round-trip");
  CHECK_NEAR(read_uint8, static_cast<double>(test_uint8), 0.5,
             "mirror_uint8 round-trip");

  // ---- stop (ForwardClose + join cyclic thread) ---------------------------
  master.stop();
  CHECK(!master.isRunning(), "cyclic thread stopped");
  CHECK(dev.state() == ethernetip_master::DeviceConnection::State::kConfigured,
        "device back to Configured after stop()");

  return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <opener_test_devices.yaml> [ip]\n";
    return 1;
  }
  const std::string yaml_path = argv[1];
  // Allow overriding IP; default = read from YAML or 10.10.10.2 (veth)
  const std::string ip = (argc >= 3) ? argv[2] : "10.10.10.2";

  std::cout << "======================================================\n";
  std::cout << "  OpENer Integration Test\n";
  std::cout << "  YAML : " << yaml_path << "\n";
  std::cout << "  IP   : " << ip << "\n";
  std::cout << "======================================================\n";

  test_explicit_messaging(ip);
  test_implicit_messaging(yaml_path);

  std::cout << "\n======================================================\n";
  if (g_failures == 0) {
    std::cout << "  ALL CHECKS PASSED\n";
  } else {
    std::cout << "  " << g_failures << " CHECK(S) FAILED\n";
  }
  std::cout << "======================================================\n";

  rclcpp::shutdown();
  return g_failures == 0 ? 0 : 1;
}
