#!/bin/bash
set -euo pipefail

# Script to repack HiBy R1 firmware based on rockbox r1_patcher.sh logic

if [[ ! -d "squashfs-root" ]] || [[ ! -f "xImage" ]]; then
    echo "Error: Missing squashfs-root/ directory or xImage file in current directory."
    echo "Run unpack.sh first and ensure xImage remains in the path."
    exit 1
fi

OUT_PKG="r1_repacked.upt"

echo "======================================"
echo "Repacking into $OUT_PKG..."
echo "======================================"

rm -rf __repack_tmp 2>/dev/null
mkdir -p __repack_tmp/image_contents/ota_v0

echo "1. Generating rootfs.squashfs..."
# Use -comp lzo and -all-root as expected by original format
sudo mksquashfs squashfs-root __repack_tmp/rootfs.squashfs -comp lzo -all-root >/dev/null

cd __repack_tmp/image_contents/ota_v0

# Process rootfs.squashfs
echo "2. Chunking and hashing rootfs..."
split -b 512k "../../rootfs.squashfs" --numeric-suffixes=0 -a 4 rootfs.squashfs.
rootfs_md5=($(md5sum "../../rootfs.squashfs"))
rootfs_size=$(stat -c%s "../../rootfs.squashfs")

md5=$rootfs_md5
ota_md5_rootfs="ota_md5_rootfs.squashfs.$md5"
> "$ota_md5_rootfs"

for part in $(ls rootfs.squashfs.[0-9]* | sort); do
    md5next=($(md5sum "$part"))
    echo $md5next >> "$ota_md5_rootfs"
    mv "$part" "$part.$md5"
    md5=$md5next
done

# Process xImage
echo "3. Chunking and hashing xImage..."
cp "../../../xImage" "../../xImage"
split -b 512k "../../xImage" --numeric-suffixes=0 -a 4 xImage.
ximage_md5=($(md5sum "../../xImage"))
ximage_size=$(stat -c%s "../../xImage")

md5=$ximage_md5
ota_md5_xImage="ota_md5_xImage.$md5"
> "$ota_md5_xImage"

for part in $(ls xImage.[0-9]* | sort); do
    md5next=($(md5sum "$part"))
    echo $md5next >> "$ota_md5_xImage"
    mv "$part" "$part.$md5"
    md5=$md5next
done

# Create meta files
echo "4. Generating OTA metadata..."
echo "ota_version=0

img_type=kernel
img_name=xImage
img_size=$ximage_size
img_md5=$ximage_md5

img_type=rootfs
img_name=rootfs.squashfs
img_size=$rootfs_size
img_md5=$rootfs_md5
" > ota_update.in

echo > ota_v0.ok

cd ../
echo "current_version=0" > ota_config.in

echo "5. Generating ISO image ($OUT_PKG) using genisoimage..."
# Use genisoimage with exact parameters from r1_patcher.sh
genisoimage -f -U -J -joliet-long -r -allow-lowercase -allow-multidot -o "../../$OUT_PKG" . >/dev/null 2>&1

cd ../../
rm -rf __repack_tmp

echo "======================================"
echo "✅ Repack complete! Flash $OUT_PKG to the device."
