#!/bin/bash

# --- Configuration Variables ---
KERNEL_IMAGE="../vmlinux.elf"            # Path to extracted ELF kernel
ROOTFS_IMAGE="rootfs-image"          # Path to your created root filesystem image
QEMU_ARCH="mipsel"                  # Use qemu-system-mipsel (little-endian)
QEMU_BOARD="malta"                  # A common generic MIPS board. You might need to experiment.

# QEMU path (adjust if qemu-system-mips is not in your PATH)
QEMU_PATH=$(which qemu-system-${QEMU_ARCH})

# Check if QEMU is found
if [ -z "$QEMU_PATH" ]; then
    echo "Error: qemu-system-${QEMU_ARCH} not found in PATH."
    echo "Please ensure QEMU is installed and the correct MIPS system emulator is available."
    exit 1
fi

# Kernel command line arguments
# - root=/dev/sda: Tells the kernel where the root filesystem is.
#                  Assuming our first QEMU drive is /dev/sda.
# - console=ttyS0: Routes kernel messages and a login prompt to the serial port.
# - rw: Mount root filesystem read-write.
# - init=/sbin/init: Specifies the first process to run (adjust if your init is elsewhere).
KERNEL_CMDLINE="root=/dev/sda console=ttyS0 rw init=/sbin/init"

# --- QEMU Command ---
echo "Starting QEMU for MIPS32..."
echo "Kernel: ${KERNEL_IMAGE}"
echo "RootFS: ${ROOTFS_IMAGE}"
echo "QEMU Board: ${QEMU_BOARD}"
echo "Kernel Cmdline: ${KERNEL_CMDLINE}"
echo ""

"${QEMU_PATH}" \
    -M "${QEMU_BOARD}" \
    -kernel "${KERNEL_IMAGE}" \
    -append "${KERNEL_CMDLINE}" \
    -drive file="${ROOTFS_IMAGE}",format=raw \
    -serial mon:stdio \
    -vga std \
    -display gtk \
    -cpu XBurstR1