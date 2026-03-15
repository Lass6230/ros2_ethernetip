"""
Launch file for the OpENer EtherNet/IP demo.

Prerequisites:
  Start OpENer in a network namespace first (see README).

Starts:
  1. robot_state_publisher  (with the opener_demo URDF)
  2. ros2_control_node      (loads EthernetIPSystem hardware plugin)
  3. gpio_command_controller (publishes state, accepts commands via topics)

Usage:
  # Terminal 1: start OpENer adapter
  sudo ip netns exec ns_opener <path>/OpENer veth-opener

  # Terminal 2: launch ros2_control
  ros2 launch ethernetip_demos opener_demo.launch.py

  # Terminal 3: read GPIO state
  ros2 topic echo /gpio_command_controller/gpio_states

  # Terminal 4: send a command
  ros2 topic pub --once /gpio_command_controller/commands \
    control_msgs/msg/DynamicInterfaceGroupValues \
    "{interface_groups: ['opener_io'], interface_values: [{interface_names: ['mirror_float', 'mirror_uint32', 'mirror_int16', 'mirror_uint8'], values: [3.14, 42.0, -123.0, 171.0]}]}"
"""

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    RegisterEventHandler,
    TimerAction,
)
from launch.event_handlers import OnProcessStart
from launch.substitutions import (
    Command,
    FindExecutable,
    LaunchConfiguration,
    PathJoinSubstitution,
)
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    # ---- paths --------------------------------------------------------------
    description_pkg = FindPackageShare("ethernetip_description")
    demo_pkg = FindPackageShare("ethernetip_demos")

    devices_yaml = PathJoinSubstitution(
        [demo_pkg, "config", "opener_devices.yaml"]
    )

    robot_description_content = Command(
        [
            PathJoinSubstitution([FindExecutable(name="xacro")]),
            " ",
            PathJoinSubstitution(
                [description_pkg, "urdf", "opener_demo.urdf.xacro"]
            ),
            " config_file:=",
            devices_yaml,
        ]
    )

    controllers_yaml = PathJoinSubstitution(
        [demo_pkg, "config", "opener_controllers.yaml"]
    )

    # ---- nodes --------------------------------------------------------------
    robot_state_publisher = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="both",
        parameters=[{"robot_description": robot_description_content}],
    )

    controller_manager = Node(
        package="controller_manager",
        executable="ros2_control_node",
        parameters=[
            {"robot_description": robot_description_content},
            controllers_yaml,
        ],
        output="both",
    )

    gpio_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "gpio_command_controller",
            "--controller-manager",
            "/controller_manager",
        ],
    )

    # Spawn the controller after controller_manager is ready
    delay_gpio = RegisterEventHandler(
        event_handler=OnProcessStart(
            target_action=controller_manager,
            on_start=[
                TimerAction(
                    period=3.0,
                    actions=[gpio_controller_spawner],
                )
            ],
        )
    )

    return LaunchDescription(
        [
            robot_state_publisher,
            controller_manager,
            delay_gpio,
        ]
    )
