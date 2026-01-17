#!/bin/bash
# Script to create a bootable rootfs image from squashfs-root directory
set -e

# --- Get project root and set up absolute paths ---
PROJECT_ROOT=$(git rev-parse --show-toplevel)
QEMU_DIR="${PROJECT_ROOT}/r1/qemu"

# --- Configuration & Defaults ---
SOURCE_DIR="${PROJECT_ROOT}/r1/unpacking_and_repacking/squashfs-root"
OUTPUT_IMAGE="${QEMU_DIR}/rootfs-image"
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
    echo "To fix this, run unpack.sh in unpacking_and_repacking"
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

# --- Determine mkfs.ext4 path ---
if [ "$CHECK_SUDO" = false ]; then
    # Cygwin scenario - use direct path
    MKFS_CMD="/usr/sbin/mkfs.ext4"
else
    # Normal Linux - rely on PATH
    MKFS_CMD="mkfs.ext4"
fi

# --- Create empty image file ---
echo "[1/5] Creating empty image file (${IMAGE_SIZE})..."
# Calculate seek size in MiB for dd (bs=1M), handling G/g and M/m suffixes
RAW_SIZE="${IMAGE_SIZE}"
UNIT="${RAW_SIZE: -1}"
VALUE="${RAW_SIZE%[GgMm]}"
case "$UNIT" in
    G|g)
        # Convert GiB to MiB
        SEEK_SIZE=$((VALUE * 1024))
        ;;
    M|m)
        # Already in MiB
        SEEK_SIZE="$VALUE"
        ;;
    *)
        # No recognized suffix; assume the value is already in MiB
        SEEK_SIZE="$RAW_SIZE"
        ;;
esac
dd if=/dev/zero of="${OUTPUT_IMAGE}" bs=1M count=0 seek="${SEEK_SIZE}" status=none

# --- Format as ext4 ---
echo "[2/5] Formatting image as ext4..."
"${MKFS_CMD}" -F "${OUTPUT_IMAGE}"

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
echo "   (Located at: ${OUTPUT_IMAGE})"
