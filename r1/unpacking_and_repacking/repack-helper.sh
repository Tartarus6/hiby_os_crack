#!/bin/bash

set -euo pipefail

# Script to repack modified firmware files into a new firmware UPT file for r1

PROJECT_ROOT=$(git rev-parse --show-toplevel)
UNPACKING_AND_REPACKING_DIR="${PROJECT_ROOT}/r1/unpacking_and_repacking"
SQUASHFS_ROOT="${UNPACKING_AND_REPACKING_DIR}/squashfs-root"
XIMAGE_PATH="${UNPACKING_AND_REPACKING_DIR}/xImage"
OUT_PKG="${UNPACKING_AND_REPACKING_DIR}/r1.upt"

# Pre-checks
if [[ ! -d "${SQUASHFS_ROOT}" ]] || [[ ! -f "${XIMAGE_PATH}" ]]; then
    echo "Error: Missing squashfs-root/ directory or xImage file."
    echo "Run unpack-helper.sh first and ensure xImage remains in the path."
    exit 1
fi

# run the repacking script
"${PROJECT_ROOT}/scripts/repack.sh" -i "${SQUASHFS_ROOT}" -k "${XIMAGE_PATH}" -o "${OUT_PKG}"
