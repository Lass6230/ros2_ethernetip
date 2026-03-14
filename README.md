# ros2_ethernetip

A generic **ros2_control** hardware-interface stack for **EtherNet/IP** devices,
built on top of [EIPScanner](https://github.com/nimbuscontrols/EIPScanner).

Designed in the same spirit as
[ethercat_driver_ros2](https://github.com/ICube-Robotics/ethercat_driver_ros2)
but for the CIP / EtherNet/IP protocol.

---

## Architecture

```
ros2_control  (controllers)
       │
EthernetIPSystem  (hardware_interface plugin)
       │
EthernetIPMaster  (scanner / cyclic IO thread)
       │
Device Plugins  (loaded via pluginlib)
       │
Assembly Mapping  (YAML → raw byte buffers)
       │
EIPScanner  (ForwardOpen, implicit IO, explicit messaging)
       │
EtherNet/IP network → robots / drives / IO
```

## Packages

| Package | Purpose |
|---------|---------|
| `ethernetip_master` | CIP connection manager wrapping EIPScanner |
| `ethernetip_device_driver_interface` | Abstract plugin base class for device drivers |
| `ethernetip_hardware_interface` | `ros2_control` `SystemInterface` plugin |
| `ethernetip_generic_io_driver` | Default assembly-mapping driver (works with any device) |
| `ethernetip_description` | YAML device configs, URDF/xacro macros |
| `ethernetip_demos` | Example launch files and controller configs |

## Quick start

```bash
# 1. Clone into your workspace
cd ~/ethernet_ip_ws/src
# (already done)

# 2. Install dependencies
rosdep install --from-paths . --ignore-src -r -y

# 3. Build
cd ~/ethernet_ip_ws
colcon build --symlink-install

# 4. Source
source install/setup.bash

# 5. Launch demo (needs a real or simulated EtherNet/IP device)
ros2 launch ethernetip_demos demo.launch.py
```

## YAML device description

Devices are fully described in a YAML file (see
`ethernetip_description/config/example_devices.yaml`):

```yaml
devices:
  - name: my_device
    ip: 192.168.1.10
    input_assembly: 101
    output_assembly: 100
    rpi: 8ms

    inputs:
      - name: joint1_pos
        type: float32
        offset: 0

    outputs:
      - name: joint1_cmd
        type: float32
        offset: 0
```

Each field becomes a ros2_control **state** or **command** interface
automatically.

## Writing a custom device driver

1. Create a new package that depends on `ethernetip_device_driver_interface`.
2. Implement `EthernetIPDevice`:
   - `configure()` – parse your device-specific config
   - `processInput()` – unpack input assembly
   - `prepareOutput()` – pack output assembly
   - `exportStateInterfaces()` / `exportCommandInterfaces()`
3. Register with `pluginlib` (see `ethernetip_generic_io_driver` as a
   reference).
4. Set `plugin: your_package/YourDriver` in the YAML.

## Development roadmap

| Phase | Scope |
|-------|-------|
| **1** | Minimal scanner – connect, read/write assemblies, generic IO driver |
| **2** | Full ros2_control integration, device plugins, YAML mapping |
| **3** | Diagnostics node, explicit messaging utilities, CLI tools |
| **4** | Industrial features – redundancy, device discovery, time sync |

## License

Apache-2.0
