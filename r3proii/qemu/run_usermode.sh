#!/bin/bash

# Simple script to run MIPS binaries from squashfs-root in QEMU user mode
# Usage: ./run_usermode.sh <path-to-binary> [args...]
#
# Examples:
#   ./run_usermode.sh /usr/bin/hiby_player
#   ./run_usermode.sh /bin/busybox ls -la
#   ./run_usermode.sh /bin/busybox uname -a
#   ./run_usermode.sh /usr/bin/some-app

SQUASHFS_ROOT="../squashfs-root"
BINARY_PATH="$1"
ARCH="mipsel"  # Use qemu-mipsel for little-endian MIPS
CPU="XBurstR1"  # CPU type to emulate
ENABLE_COREDUMP=0  # Set to 1 to enable core dumps
shift  # Remove first argument, leaving the rest as args

if [ -z "$BINARY_PATH" ]; then
    echo "Usage: $0 <path-to-binary-relative-to-squashfs-root> [args...]"
    echo ""
    echo "Examples:"
    echo "  $0 /bin/busybox ls -la"
    echo "  $0 /bin/busybox uname -a"
    echo "  $0 /bin/echo Hello World"
    exit 1
fi

# Remove leading slash if present
BINARY_PATH="${BINARY_PATH#/}"

FULL_PATH="${SQUASHFS_ROOT}/${BINARY_PATH}"

if [ ! -f "$FULL_PATH" ]; then
    echo "Error: Binary not found at ${FULL_PATH}"
    exit 1
fi

# Run with clean environment to avoid locale issues
# -E sets environment variables inside the emulated process
# -L specifies the root directory for the guest system
# Core dumps disabled by default - set ENABLE_COREDUMP=1 to enable
if [ "$ENABLE_COREDUMP" = "1" ]; then
    ulimit -c unlimited
else
    ulimit -c 0
fi

env -i \
    qemu-mipsel \
    -E LD_LIBRARY_PATH=/lib:/lib32:/usr/lib \
    -L "${SQUASHFS_ROOT}" \
    "${FULL_PATH}" \
    "$@"
