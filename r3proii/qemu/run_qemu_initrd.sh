#!/bin/bash

# Run QEMU system emulation using initrd approach
# This creates a CPIO archive from squashfs-root and uses it as initrd

set -e

# --- Configuration Variables ---
KERNEL_IMAGE="../Linux-4.4.94+.elf"
SQUASHFS_ROOT="../squashfs-root"
INITRD_IMAGE="initrd.cpio"
QEMU_ARCH="mipsel"
QEMU_BOARD="malta"
MEMORY_SIZE="64M"                   # Amount of RAM for the emulated system
QEMU_CPU="XBurstR2"

# Check if QEMU is found
QEMU_PATH=$(which qemu-system-${QEMU_ARCH})
if [ -z "$QEMU_PATH" ]; then
    echo "Error: qemu-system-${QEMU_ARCH} not found in PATH."
    exit 1
fi

# Create initrd if it doesn't exist or is older than squashfs-root
if [ ! -f "$INITRD_IMAGE" ] || [ "$SQUASHFS_ROOT" -nt "$INITRD_IMAGE" ]; then
    echo "Creating initrd CPIO archive from ${SQUASHFS_ROOT}..."
    cd "$SQUASHFS_ROOT"
    find . | cpio -o -H newc | gzip > "../qemu/${INITRD_IMAGE}.gz"
    cd - > /dev/null
    mv "${INITRD_IMAGE}.gz" "${INITRD_IMAGE}"
    echo "Initrd created: ${INITRD_IMAGE}"
fi

# Kernel command line arguments
# - console=ttyS0: Routes output to serial port
# - rw: Mount root filesystem read-write
# - init=/linuxrc: Your init script (adjust if needed)
KERNEL_CMDLINE="rw init=/sbin/init mem=${MEMORY_SIZE} earlyprintk debug"

# --- QEMU Command ---
echo "Starting QEMU for ${QEMU_ARCH}..."
echo "Kernel: ${KERNEL_IMAGE}"
echo "Initrd: ${INITRD_IMAGE}"
echo "QEMU Board: ${QEMU_BOARD}"
echo "Kernel Cmdline: ${KERNEL_CMDLINE}"
echo ""

"${QEMU_PATH}" \
    -M "${QEMU_BOARD}" \
    -cpu ${QEMU_CPU} \
    -m ${MEMORY_SIZE} \
    -kernel "${KERNEL_IMAGE}" \
    -initrd "${INITRD_IMAGE}" \
    -append "${KERNEL_CMDLINE}" \
    -s \
    -S \
    -d in_asm,int,cpu,unimp,guest_errors -D /tmp/qemu.log
