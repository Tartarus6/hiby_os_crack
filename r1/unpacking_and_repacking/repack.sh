#!/bin/bash

# Script to repack modified firmware files into a new firmware file

# Getting project root dir
PROJECT_ROOT=`git rev-parse --show-toplevel`
FOLDER_ROOT=${PROJECT_ROOT}/r1/unpacking_and_repacking

cd $FOLDER_ROOT

rm -r temp
rm r1.upt

# making temporary directory, where operations will be done
mkdir temp
cd temp

echo "#######################################"
echo "### COPYING ORIGINAL FIRMWARE FILES ###"
echo "#######################################"
echo ""

cp ${PROJECT_ROOT}/r1/original_firmware/r1.upt ./
7z x r1.upt
rm r1.upt

echo "###################################"
echo "### REMOVING OLD SQUASHFS FILES ###"
echo "###################################"
echo ""

cd ota_v0
rm ota_md5_rootfs.squashfs.*
rm rootfs.*


echo "#####################################"
echo "### GENERATING NEW SQUASHFS FILES ###"
echo "#####################################"
echo ""

sudo mksquashfs ${PROJECT_ROOT}/r1/unpacking_and_repacking/squashfs-root ./rootfs.squashfs -comp lzo
sudo chown $USER:$USER rootfs.squashfs
split rootfs.squashfs -d -a 4 -b 512k rootfs.squashfs.

# getting md5 sum of full squashfs file (needed in some md5 checks)
SUM=`md5sum rootfs.squashfs | awk '{print $1}'`
SIZE=`du rootfs.squashfs -b | awk '{print $1}'`

cat > ota_update.in <<- EOM
ota_version=0

img_type=kernel
img_name=xImage
img_size=3760192
img_md5=4a459b51a152014bfab6c1114f2701e3

img_type=rootfs
img_name=rootfs.squashfs
img_size=$SIZE
img_md5=$SUM
EOM

# removing full squashfs file
rm rootfs.squashfs

# adding sums to file names and to md5 sum check file (each file has the sum of previous file in its name)
touch ota_md5_rootfs.squashfs.$SUM
for filename in rootfs.squashfs.*;
do
    mv $filename "$filename.$SUM"

    SUM=`md5sum "$filename.$SUM" | awk '{print $1}'`

    echo $SUM >> ota_md5_rootfs.squashfs.*
done

echo "#################################"
echo "### GENERATING FIRMWARE FILE ###"
echo "#################################"
echo ""

cd $FOLDER_ROOT
mkisofs -o r1.upt -J -r ./temp/

# cleanup
rm -r ./temp/

echo ""
echo "Repacking complete!"
echo "Firmware image saved as r1.upt"
echo ""
echo "Now you can flash this to the device"
