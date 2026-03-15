#!/usr/bin/env bash
# Build the OpENer EtherNet/IP adapter for integration testing.
#
# Usage:   ./build_opener.sh [install_prefix]
# Default: installs to ./opener_install
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${1:-${SCRIPT_DIR}/opener_install}"
BUILD_DIR="${SCRIPT_DIR}/opener_build"
SRC_DIR="${SCRIPT_DIR}/opener_src"

echo "=== OpENer EtherNet/IP adapter build ==="
echo "  Source:  ${SRC_DIR}"
echo "  Build:   ${BUILD_DIR}"
echo "  Install: ${INSTALL_DIR}"

# --------------------------------------------------------------------------
# 1) Clone if not present
# --------------------------------------------------------------------------
if [ ! -d "${SRC_DIR}" ]; then
  echo ">>> Cloning OpENer ..."
  git clone --depth 1 https://github.com/EIPStackGroup/OpENer.git "${SRC_DIR}"
else
  echo ">>> OpENer source already present, skipping clone"
fi

# --------------------------------------------------------------------------
# 2) Build using the POSIX port
# --------------------------------------------------------------------------
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake "${SRC_DIR}/source" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DOpENer_PLATFORM:STRING="POSIX" \
  -DCMAKE_BUILD_TYPE=Release

make -j"$(nproc)"

echo ""
echo "=== OpENer built successfully ==="
echo "  Binary: ${BUILD_DIR}/src/ports/POSIX/OpENer"
echo ""
echo "To run:  sudo ${BUILD_DIR}/src/ports/POSIX/OpENer lo"
echo "  (use 'lo' for loopback / integration tests, or your real NIC name)"
