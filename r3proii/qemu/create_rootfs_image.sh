#!/bin/bash

# Script to create a bootable rootfs image from squashfs-root directory
# This creates an ext4 filesystem image suitable for QEMU

set -e  # Exit on error

# --- Configuration ---
SOURCE_DIR="../squashfs-root"
OUTPUT_IMAGE="rootfs-image"
IMAGE_SIZE="256M"  # Adjust size as needed (e.g., 256M, 512M, 1G)

# --- Checks ---
if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory '$SOURCE_DIR' not found!"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then 
    echo "This script needs root privileges to preserve ownership and permissions."
    echo "Please run with sudo:"
    echo "  sudo $0"
    exit 1
fi

# --- Create empty image file ---
echo "[1/5] Creating empty image file (${IMAGE_SIZE})..."
dd if=/dev/zero of="${OUTPUT_IMAGE}" bs=1M count=0 seek=$(echo ${IMAGE_SIZE} | sed 's/M//') status=progress

# --- Format as ext4 ---
echo "[2/5] Formatting image as ext4..."
mkfs.ext4 "${OUTPUT_IMAGE}"

# --- Mount the image ---
echo "[3/5] Mounting image..."
MOUNT_POINT=$(mktemp -d)
mount -o loop "${OUTPUT_IMAGE}" "${MOUNT_POINT}"

# Ensure unmount on exit
trap "umount ${MOUNT_POINT} && rmdir ${MOUNT_POINT}" EXIT

# --- Copy files ---
echo "[4/5] Copying files from ${SOURCE_DIR} to image..."
echo "This may take a while..."
cp -a "${SOURCE_DIR}"/* "${MOUNT_POINT}"/

# --- Verify and unmount ---
echo "[5/5] Syncing and unmounting..."
sync
umount "${MOUNT_POINT}"
rmdir "${MOUNT_POINT}"

# --- Update file permissions ---
chmod 666 "${OUTPUT_IMAGE}"

trap - EXIT  # Remove trap since we manually unmounted

# --- Done ---
echo ""
echo "✓ Success! Root filesystem image created: ${OUTPUT_IMAGE}"
echo "  Size: ${IMAGE_SIZE}"
echo "  Format: ext4"
echo ""
echo "You can now use this image with QEMU:"
echo "  ./run_qemu.sh"
