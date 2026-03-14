# Copyright 2026, ethernetip_description contributors
# Licensed under the Apache License, Version 2.0

import os
import subprocess
import tempfile


def get_package_share_directory(package_name: str) -> str:
    """Simple helper to find the installed share directory."""
    result = subprocess.run(
        ["ros2", "pkg", "prefix", "--share", package_name],
        capture_output=True, text=True
    )
    if result.returncode == 0:
        return result.stdout.strip()
    from ament_index_python.packages import get_package_share_directory as _get
    return _get(package_name)


def _run_xacro_string(xacro_content: str) -> subprocess.CompletedProcess:
    """Write xacro content to a temp file and process it."""
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".xacro", delete=False
    ) as tmp:
        tmp.write(xacro_content)
        tmp_path = tmp.name
    try:
        result = subprocess.run(
            ["xacro", tmp_path],
            capture_output=True, text=True
        )
    finally:
        os.unlink(tmp_path)
    return result


def test_xacro_system_macro():
    """Test that the ethernetip_system.xacro macro processes without errors."""
    share_dir = get_package_share_directory("ethernetip_description")
    xacro_file = os.path.join(share_dir, "urdf", "ethernetip_system.xacro")
    assert os.path.isfile(xacro_file), f"Xacro file not found: {xacro_file}"

    test_urdf = (
        '<?xml version="1.0"?>\n'
        '<robot xmlns:xacro="http://www.ros.org/wiki/xacro"'
        ' name="test_robot">\n'
        f'  <xacro:include filename="{xacro_file}"/>\n'
        '  <link name="base_link"/>\n'
        '  <xacro:ethernetip_system\n'
        '    name="test_eip"\n'
        '    config_file="/tmp/dummy.yaml"\n'
        '  />\n'
        '</robot>\n'
    )

    result = _run_xacro_string(test_urdf)
    assert result.returncode == 0, \
        f"xacro processing failed:\n{result.stderr}"
    assert "<ros2_control" in result.stdout
    assert 'name="test_eip"' in result.stdout
    assert "EthernetIPSystem" in result.stdout


def test_xacro_system_macro_with_joints():
    """Test the xacro macro with joint definitions added by the caller."""
    share_dir = get_package_share_directory("ethernetip_description")
    xacro_file = os.path.join(share_dir, "urdf", "ethernetip_system.xacro")

    # Joints are defined separately by the caller, not by the macro
    test_urdf = (
        '<?xml version="1.0"?>\n'
        '<robot xmlns:xacro="http://www.ros.org/wiki/xacro"'
        ' name="test_robot">\n'
        f'  <xacro:include filename="{xacro_file}"/>\n'
        '  <link name="base_link"/>\n'
        '  <xacro:ethernetip_system\n'
        '    name="eip_joints"\n'
        '    config_file="/tmp/dummy.yaml"\n'
        '  />\n'
        '</robot>\n'
    )

    result = _run_xacro_string(test_urdf)
    assert result.returncode == 0, \
        f"xacro processing failed:\n{result.stderr}"
    assert "<ros2_control" in result.stdout
    assert 'name="eip_joints"' in result.stdout


def test_example_robot_xacro():
    """Test that example_robot.urdf.xacro processes without errors."""
    share_dir = get_package_share_directory("ethernetip_description")
    xacro_file = os.path.join(share_dir, "urdf", "example_robot.urdf.xacro")
    assert os.path.isfile(xacro_file), f"Xacro file not found: {xacro_file}"

    result = subprocess.run(
        ["xacro", xacro_file],
        capture_output=True, text=True
    )
    assert result.returncode == 0, \
        f"xacro processing failed:\n{result.stderr}"
    assert "<robot" in result.stdout
    assert "base_link" in result.stdout
    assert "joint1" in result.stdout
    assert "joint2" in result.stdout
