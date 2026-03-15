# ethernetip_demos

Demo launch files for the **ros2_ethernetip** stack.

Two demos are included:

| Demo | Launch file | Target device | Controller |
|------|-------------|---------------|------------|
| **OpENer demo** | `opener_demo.launch.py` | OpENer POSIX sample app (loopback mirror) | `GpioCommandController` |
| **Robot demo** | `demo.launch.py` | Generic 2-joint robot (real hardware) | `JointTrajectoryController` |

---

## OpENer Demo (recommended for testing)

Uses the open-source [OpENer](https://github.com/EIPStackGroup/OpENer) EtherNet/IP adapter as a
fake device. OpENer mirrors all output assembly data back to the input assembly, so you can verify
the full round-trip: **ros2 topic → controller → hardware interface → EIPScanner → UDP → OpENer → UDP → back**.

### Prerequisites

```bash
# Build the workspace
cd ~/github/ethernet_ip_ws
source /opt/ros/jazzy/setup.bash
colcon build
source install/setup.bash

# Build OpENer (one-time)
./src/ros2_ethernetip/ethernetip_master/test/integration/build_opener.sh
```

### Step 1 — Set up network and start OpENer (Terminal 1)

OpENer and the scanner both use UDP port 2222, so they must be on separate
network stacks. A veth pair with a Linux network namespace solves this:

```bash
# Create veth pair + namespace
sudo ip netns del ns_opener 2>/dev/null; sudo ip link del veth-scanner 2>/dev/null

sudo ip netns add ns_opener
sudo ip link add veth-scanner type veth peer name veth-opener
sudo ip link set veth-opener netns ns_opener
sudo ip netns exec ns_opener ip addr add 10.10.10.2/24 dev veth-opener
sudo ip netns exec ns_opener ip link set veth-opener up
sudo ip netns exec ns_opener ip link set lo up
sudo ip addr add 10.10.10.1/24 dev veth-scanner 2>/dev/null
sudo ip link set veth-scanner up

# Start OpENer (runs in foreground, Ctrl+C to stop)
sudo ip netns exec ns_opener \
  ./src/ros2_ethernetip/ethernetip_master/test/integration/opener_build/src/ports/POSIX/OpENer veth-opener
```

You should see OpENer start without errors. Leave this terminal running.

### Step 2 — Launch ros2_control (Terminal 2)

```bash
cd ~/github/ethernet_ip_ws
source /opt/ros/jazzy/setup.bash
source install/setup.bash

ros2 launch ethernetip_demos opener_demo.launch.py
```

This starts:
- **robot_state_publisher** — publishes the URDF to `/robot_description`
- **ros2_control_node** — loads the `EthernetIPSystem` hardware plugin, opens a
  ForwardOpen connection to OpENer, and starts cyclic IO
- **gpio_command_controller** — exposes ROS 2 topics for reading/writing the IO
  data

### Step 3 — Read GPIO state (Terminal 3)

```bash
source /opt/ros/jazzy/setup.bash
ros2 topic echo /gpio_command_controller/gpio_states
```

### Step 4 — Send a command (Terminal 4)

```bash
source /opt/ros/jazzy/setup.bash

ros2 topic pub --once /gpio_command_controller/commands \
  control_msgs/msg/DynamicInterfaceGroupValues \
  "{interface_groups: ['opener_io'], \
    interface_values: [{interface_names: \
      ['mirror_float', 'mirror_uint32', 'mirror_int16', 'mirror_uint8'], \
      values: [3.14, 42.0, -123.0, 171.0]}]}"
```

Then check Terminal 3 — the state topic should now show the mirrored values
(`3.14`, `42`, `-123`, `171`).

### Cleanup

1. **Ctrl+C** in Terminal 2 to stop `ros2_control_node` (sends ForwardClose)
2. **Ctrl+C** in Terminal 1 to stop OpENer
3. Remove the network namespace:
   ```bash
   sudo ip netns del ns_opener
   sudo ip link del veth-scanner 2>/dev/null
   ```

### Automated test

You can also run both parts (OpENer + scanner) automatically:

```bash
sudo bash ./src/ros2_ethernetip/ethernetip_master/test/integration/run_integration_test.sh
```

This sets up the network, starts OpENer, runs the library-level and
ros2_control-level integration tests, and cleans up.

---

## Robot Demo (real hardware)

For connecting to an actual EtherNet/IP device (PLC, drive, IO block, etc.).

### Configuration

1. Edit `config/demo_devices.yaml` — set the device IP, assembly numbers, RPI,
   and field mappings to match your hardware.
2. Edit `config/controllers.yaml` — configure the controllers for your joints.
3. Edit `ethernetip_description/urdf/example_robot.urdf.xacro` — update the
   URDF to match your robot's kinematic chain.

### Launch

```bash
ros2 launch ethernetip_demos demo.launch.py
```

---

## Package structure

```
ethernetip_demos/
├── config/
│   ├── opener_controllers.yaml   # GpioCommandController for OpENer demo
│   ├── opener_devices.yaml       # OpENer device/assembly config
│   ├── controllers.yaml          # JointTrajectoryController for robot demo
│   └── demo_devices.yaml         # Example robot device config
├── launch/
│   ├── opener_demo.launch.py     # OpENer demo launch file
│   └── demo.launch.py            # Robot demo launch file
├── CMakeLists.txt
├── package.xml
└── README.md
```

## Topics (OpENer demo)

| Topic | Type | Direction |
|-------|------|-----------|
| `/gpio_command_controller/gpio_states` | `control_msgs/msg/DynamicInterfaceGroupValues` | Published (state) |
| `/gpio_command_controller/commands` | `control_msgs/msg/DynamicInterfaceGroupValues` | Subscribed (command) |

## Assembly layout (OpENer)

| Byte offset | Field | Type |
|-------------|-------|------|
| 0 | `mirror_float` | float32 |
| 4 | `mirror_uint32` | uint32 |
| 8 | `mirror_int16` | int16 |
| 10 | `mirror_uint8` | uint8 |

Input assembly 100 (32 bytes, T→O) and output assembly 150 (32 bytes, O→T)
use the same layout. OpENer copies output → input each cycle.
