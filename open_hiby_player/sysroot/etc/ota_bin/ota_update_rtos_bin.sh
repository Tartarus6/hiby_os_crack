#!/bin/sh

ota_rtos_name=$1
ota_rtos_size=$2
ota_rtos_dev=$3

if [ $# != 3 ] ; then
    echo "usage: $0 ota/rtos/file/name total_size /dev/mtdN" 1>&2
    exit 1
fi

set_write_failed()
{
    echo "ota failed: $1" > $ota_rtos_name.failed
}

which nandwrite > /dev/null
if [ $? != 0 ]; then
    echo "error: nandwrite not installed\n" 1>&2
    set_write_failed "nandwrite not installed"
    exit 1
fi

which flash_erase > /dev/null
if [ $? != 0 ]; then
    echo "error: flash_erase not installed\n" 1>&2
    set_write_failed "flash_erase not installed"
    exit 1
fi

flash_erase $ota_rtos_dev 0 0
if [ $? != 0 ]; then
    set_write_failed "flash erase error $?"
    exit 1
fi

/etc/ota_bin/ota_img_data_provider.sh $ota_rtos_name $ota_rtos_size \
    | nandwrite -m -p --input-size=$ota_rtos_size $ota_rtos_dev -

if [ $? != 0 ]; then
    set_write_failed "write error $?"
    exit 1
fi

if [ -e $ota_rtos_name.failed ]; then
    exit 1
fi

if [ ! -e $ota_rtos_name.ok ]; then
    set_write_failed "ota_img_data_provider terminated"
    exit 1
fi

exit 0
