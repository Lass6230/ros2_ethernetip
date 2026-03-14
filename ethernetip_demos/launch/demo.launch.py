"""
Launch file for the EtherNet/IP ros2_control demo.

Starts:
  1. robot_state_publisher  (with the example URDF)
  2. controller_manager     (loads the EthernetIPSystem hardware plugin)
  3. joint_state_broadcaster
  4. joint_trajectory_controller
"""

import os

from ament_index_python.packages import get_package_share_directory
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

    robot_description_content = Command(
        [
            PathJoinSubstitution([FindExecutable(name="xacro")]),
            " ",
            PathJoinSubstitution(
                [description_pkg, "urdf", "example_robot.urdf.xacro"]
            ),
        ]
    )

    controllers_yaml = PathJoinSubstitution(
        [demo_pkg, "config", "controllers.yaml"]
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

    # Spawners – delay a bit to let controller_manager initialise
    joint_state_broadcaster_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "joint_state_broadcaster",
            "--controller-manager",
            "/controller_manager",
        ],
    )

    joint_trajectory_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "joint_trajectory_controller",
            "--controller-manager",
            "/controller_manager",
        ],
    )

    # Chain: CM starts → spawn JSB → spawn JTC
    delay_jsb = RegisterEventHandler(
        event_handler=OnProcessStart(
            target_action=controller_manager,
            on_start=[
                TimerAction(
                    period=3.0,
                    actions=[joint_state_broadcaster_spawner],
                )
            ],
        )
    )

    delay_jtc = RegisterEventHandler(
        event_handler=OnProcessStart(
            target_action=controller_manager,
            on_start=[
                TimerAction(
                    period=5.0,
                    actions=[joint_trajectory_controller_spawner],
                )
            ],
        )
    )

    return LaunchDescription(
        [
            robot_state_publisher,
            controller_manager,
            delay_jsb,
            delay_jtc,
        ]
    )
