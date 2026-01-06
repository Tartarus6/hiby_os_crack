#!/bin/bash

# Script to extract the root filesystem from the original firmware
# This captures all the original permissions, ownership, and special files

# Getting project root dir
PROJECT_ROOT=`git rev-parse --show-toplevel`

echo "##################################"
echo "### EXTRACTING SQUASHFS-ROOTFS ###"
echo "##################################"
echo ""

rm -r ./temp/  # clean up old temp folder in case it exists (can happen if a script fails to finish)

mkdir temp

cd temp
7z x ${PROJECT_ROOT}/r3proii/original_firmware/r3proii.upt  # extract the contents of the firmware iso image

cd ota_v0
cat rootfs.squashfs.* > rootfs.squashfs  # combine the squashfs file parts into one

echo "##################################"
echo "### EXTRACTING SQUASHFS-ROOTFS ###"
echo "##################################"
echo ""

cd ${PROJECT_ROOT}/r3proii/unpacking_and_repacking

rm -r squashfs-root  # remove old extraction if it exists

# extracting the file system
echo "Note: Unsquashfs requires sudo permissions in order to retain original rootfs file permissions (like which user owns a file)"
sudo unsquashfs -d squashfs-root ./temp/ota_v0/rootfs.squashfs

rm -r ./temp/  # clean up temp folder

echo ""
echo "Unpacking complete!"
echo "Original filesystem extracted to: squashfs-root/"
echo ""
echo "Now you can modify files in squashfs-root/"
echo "Note: You'll probably need sudo privilages to modify many of the system files"
