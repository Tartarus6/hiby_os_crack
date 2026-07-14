#!/bin/sh

case $1 in
    Recovery)
        flash_erase /dev/mtd5 0 1
        printf "%-256s" "ota:kernel2" | nandwrite -s 0 -p /dev/mtd5 -
    ;;
    *)
        flash_erase /dev/mtd5 0 1
        printf "%-256s" "ota:kernel" | nandwrite -s 0 -p /dev/mtd5 -
    ;;
esac
