#!/usr/bin/env bash
# =============================================================================
# OpENer integration test runner  –  uses a veth pair + network namespace
# so that the adapter and scanner have separate network stacks.
#
# Usage:
#   sudo ./run_integration_test.sh [opener_binary] [test_binary] [yaml_config]
#
# The script:
#   1. Creates a veth pair  (veth-opener ↔ veth-scanner)
#   2. Moves veth-opener into a network namespace "ns_opener"
#   3. Assigns  10.10.10.2/24  to veth-opener,  10.10.10.1/24  to veth-scanner
#   4. Starts OpENer inside ns_opener on interface veth-opener
#   5. Runs the integration test (scanner connects to 10.10.10.2)
#   6. Tears everything down and reports PASS / FAIL
# =============================================================================
set -euo pipefail

# Require root for network namespace operations
if [ "$(id -u)" -ne 0 ]; then
  echo "ERROR: This script needs root.  Run with:  sudo $0 $*"
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --------------------------------------------------------------------------
# Resolve binary / config paths
# --------------------------------------------------------------------------
OPENER_BIN="${1:-${SCRIPT_DIR}/opener_build/src/ports/POSIX/OpENer}"
WS_ROOT="$(cd "${SCRIPT_DIR}/../../../../.." && pwd)"
TEST_BIN="${2:-${WS_ROOT}/install/ethernetip_master/lib/ethernetip_master/test_opener_connection}"
YAML_CFG="${3:-${SCRIPT_DIR}/opener_test_devices.yaml}"

# Also try the build directory if install doesn't have it
if [ ! -x "${TEST_BIN}" ]; then
  TEST_BIN="${WS_ROOT}/build/ethernetip_master/test_opener_connection"
fi

echo "=== OpENer Integration Test Runner (veth) ==="
echo "  OpENer binary : ${OPENER_BIN}"
echo "  Test binary   : ${TEST_BIN}"
echo "  YAML config   : ${YAML_CFG}"
echo ""

# --------------------------------------------------------------------------
# Pre-flight checks
# --------------------------------------------------------------------------
if [ ! -x "${OPENER_BIN}" ]; then
  echo "ERROR: OpENer binary not found at ${OPENER_BIN}"
  echo "       Run build_opener.sh first."
  exit 1
fi
if [ ! -x "${TEST_BIN}" ]; then
  echo "ERROR: Test binary not found at ${TEST_BIN}"
  echo "       Build with:  colcon build --cmake-args -DBUILD_INTEGRATION_TESTS=ON"
  exit 1
fi
if [ ! -f "${YAML_CFG}" ]; then
  echo "ERROR: YAML config not found at ${YAML_CFG}"
  exit 1
fi

# --------------------------------------------------------------------------
# Network namespace + veth setup
# --------------------------------------------------------------------------
NS="ns_opener"
VETH_OPENER="veth-opener"
VETH_SCANNER="veth-scanner"
OPENER_IP="10.10.10.2"
SCANNER_IP="10.10.10.1"

cleanup() {
  echo ""
  echo ">>> Cleaning up ..."
  if [ -n "${OPENER_PID:-}" ]; then
    kill "${OPENER_PID}" 2>/dev/null || true
    wait "${OPENER_PID}" 2>/dev/null || true
  fi
  ip netns del "${NS}" 2>/dev/null || true
  ip link del "${VETH_SCANNER}" 2>/dev/null || true
  echo "    done."
}
trap cleanup EXIT

echo ">>> Setting up network namespace '${NS}' + veth pair ..."

ip netns del "${NS}" 2>/dev/null || true
ip link del "${VETH_SCANNER}" 2>/dev/null || true

ip netns add "${NS}"
ip link add "${VETH_SCANNER}" type veth peer name "${VETH_OPENER}"
ip link set "${VETH_OPENER}" netns "${NS}"

ip netns exec "${NS}" ip addr add "${OPENER_IP}/24" dev "${VETH_OPENER}"
ip netns exec "${NS}" ip link set "${VETH_OPENER}" up
ip netns exec "${NS}" ip link set lo up

ip addr add "${SCANNER_IP}/24" dev "${VETH_SCANNER}" 2>/dev/null || true
ip link set "${VETH_SCANNER}" up

echo "    ${VETH_OPENER} (in ${NS}) = ${OPENER_IP}"
echo "    ${VETH_SCANNER} (host)    = ${SCANNER_IP}"

echo -n "    ping test: "
if ip netns exec "${NS}" ping -c1 -W1 "${SCANNER_IP}" >/dev/null 2>&1; then
  echo "OK"
else
  echo "FAILED"
  exit 1
fi

# --------------------------------------------------------------------------
# Start OpENer inside the namespace
# --------------------------------------------------------------------------
echo ">>> Starting OpENer in namespace '${NS}' on ${VETH_OPENER} ..."
ip netns exec "${NS}" "${OPENER_BIN}" "${VETH_OPENER}" &
OPENER_PID=$!

echo ">>> Waiting for OpENer to listen on port 44818 ..."
for i in $(seq 1 15); do
  if ip netns exec "${NS}" ss -tlnp 2>/dev/null | grep -q ':44818'; then
    echo "    OpENer ready after ${i}s"
    break
  fi
  sleep 1
done

if ! ip netns exec "${NS}" ss -tlnp 2>/dev/null | grep -q ':44818'; then
  echo "ERROR: OpENer did not start within 15 s"
  exit 1
fi

# --------------------------------------------------------------------------
# Run the integration test
# --------------------------------------------------------------------------
echo ""
echo ">>> Running integration test (scanner connects to ${OPENER_IP}) ..."
echo ""

# Temporarily disable nounset for ROS 2 setup scripts
set +u
if [ -f /opt/ros/jazzy/setup.bash ]; then
  source /opt/ros/jazzy/setup.bash
fi
if [ -f "${WS_ROOT}/install/setup.bash" ]; then
  source "${WS_ROOT}/install/setup.bash"
fi
set -u

# --------------------------------------------------------------------------
# Part A: Library-level integration test
# --------------------------------------------------------------------------
echo "============================================"
echo "  Part A: Library-level integration test"
echo "============================================"

set +e
"${TEST_BIN}" "${YAML_CFG}"
TEST_A_RC=$?
set -e

echo ""
if [ "${TEST_A_RC}" -eq 0 ]; then
  echo ">>> Part A PASSED <<<"
else
  echo ">>> Part A FAILED (exit code ${TEST_A_RC}) <<<"
fi

# --------------------------------------------------------------------------
# Part B: ros2_control-level integration test
# --------------------------------------------------------------------------
RC2_BIN="${WS_ROOT}/install/ethernetip_master/lib/ethernetip_master/test_ros2_control_opener"
if [ ! -x "${RC2_BIN}" ]; then
  RC2_BIN="${WS_ROOT}/build/ethernetip_master/test_ros2_control_opener"
fi

TEST_B_RC=0
if [ -x "${RC2_BIN}" ]; then
  echo ""
  echo "============================================"
  echo "  Part B: ros2_control-level integration test"
  echo "============================================"

  set +e
  "${RC2_BIN}" "${YAML_CFG}"
  TEST_B_RC=$?
  set -e

  echo ""
  if [ "${TEST_B_RC}" -eq 0 ]; then
    echo ">>> Part B PASSED <<<"
  else
    echo ">>> Part B FAILED (exit code ${TEST_B_RC}) <<<"
  fi
else
  echo ""
  echo ">>> Part B SKIPPED (test_ros2_control_opener not found) <<<"
fi

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------
TOTAL_RC=$(( TEST_A_RC + TEST_B_RC ))

echo ""
echo "============================================"
if [ "${TOTAL_RC}" -eq 0 ]; then
  echo "  ALL INTEGRATION TESTS PASSED"
else
  echo "  INTEGRATION TESTS FAILED"
  echo "    Part A: exit ${TEST_A_RC}"
  echo "    Part B: exit ${TEST_B_RC}"
fi
echo "============================================"

exit "${TOTAL_RC}"
