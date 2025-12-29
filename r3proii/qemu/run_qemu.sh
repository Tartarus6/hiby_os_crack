#!/bin/bash

# --- Configuration Variables ---
KERNEL_IMAGE="../Linux-4.4.94+.elf" # Path to extracted ELF kernel
ROOTFS_IMAGE="rootfs-image"         # Path to your created root filesystem image
QEMU_ARCH="mipsel"                  # Use qemu-system-mipsel (little-endian)
QEMU_BOARD="malta"                  # A common generic MIPS board. You might need to experiment.
QEMU_CPU="XBurstR1"                # CPU type to emulate

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
# - console=ttyS0: Routes kernel messages and a login prompt to the serial port (MIPS uses ttyS0).
# - rw: Mount root filesystem read-write.
# - init=/sbin/init: Specifies the first process to run (adjust if your init is elsewhere).
KERNEL_CMDLINE="root=/dev/sda console=ttyS0 rw init=/sbin/init"

# --- QEMU Command ---
echo "Starting QEMU for ${QEMU_ARCH}..."
echo "Kernel: ${KERNEL_IMAGE}"
echo "RootFS: ${ROOTFS_IMAGE}"
echo "QEMU Board: ${QEMU_BOARD}"
echo "Kernel Cmdline: ${KERNEL_CMDLINE}"
echo ""

"${QEMU_PATH}" \
    -M "${QEMU_BOARD}" \
    -cpu ${QEMU_CPU} \
    -kernel "${KERNEL_IMAGE}" \
    -append "${KERNEL_CMDLINE}" \
    -drive file="${ROOTFS_IMAGE}",format=raw \
    -serial stdio