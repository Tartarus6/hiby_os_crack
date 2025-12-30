#!/bin/bash

# Script to create a bootable rootfs image from squashfs-root directory
set -e  

# --- Configuration & Defaults ---
SOURCE_DIR="../squashfs-root"
OUTPUT_IMAGE="rootfs-image"
IMAGE_SIZE="256M"  
CHECK_SUDO=true  # Default to true

# --- Parse Arguments ---
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -n|--no-sudo) CHECK_SUDO=false; shift ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
done

# --- Checks ---
if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory '$SOURCE_DIR' not found!"
    exit 1
fi

# Conditional Sudo Check
if [ "$CHECK_SUDO" = true ]; then
    if [ "$EUID" -ne 0 ]; then 
        echo "Error: This script requires root privileges."
        echo "Use -n or --no-sudo to bypass this check (ensure you have loop device permissions)."
        echo "Usage: sudo $0"
        exit 1
    fi
fi

# --- Create empty image file ---
echo "[1/5] Creating empty image file (${IMAGE_SIZE})..."
# Using a more robust size calculation for the seek command
SEEK_SIZE=$(echo ${IMAGE_SIZE} | sed 's/[GgMm]//')
dd if=/dev/zero of="${OUTPUT_IMAGE}" bs=1M count=0 seek="${SEEK_SIZE}" status=none

# --- Format as ext4 ---
echo "[2/5] Formatting image as ext4..."
mkfs.ext4 -F "${OUTPUT_IMAGE}"

# --- Mount the image ---
echo "[3/5] Mounting image..."
MOUNT_POINT=$(mktemp -d)

# If we aren't root, mounting will likely fail unless using user-namespaces or specialized tools
mount -o loop "${OUTPUT_IMAGE}" "${MOUNT_POINT}"

# Ensure unmount on exit
trap "umount ${MOUNT_POINT} 2>/dev/null && rmdir ${MOUNT_POINT} 2>/dev/null" EXIT

# --- Copy files ---
echo "[4/5] Copying files from ${SOURCE_DIR} to image..."
cp -a "${SOURCE_DIR}"/. "${MOUNT_POINT}"/

# --- Verify and unmount ---
echo "[5/5] Syncing and unmounting..."
sync
umount "${MOUNT_POINT}"
rmdir "${MOUNT_POINT}"
trap - EXIT 

# --- Finalize ---
chmod 666 "${OUTPUT_IMAGE}"
echo "✓ Success! Root filesystem image created: ${OUTPUT_IMAGE}"
