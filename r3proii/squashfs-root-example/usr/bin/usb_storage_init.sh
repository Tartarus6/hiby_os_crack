#!/bin/sh

if [ ! -d /sys/kernel/config/usb_gadget ];then
    echo "mount /sys/kernel/config"
	mount -t configfs none /sys/kernel/config
fi

if [ ! -d /sys/kernel/config/usb_gadget/android0 ];then
    echo "mkdir /sys/kernel/usb_gadget/android0"
	mkdir /sys/kernel/config/usb_gadget/android0
fi

if [ ! -d /sys/kernel/config/usb_gadget/android0/strings/0x409 ];then
    echo "mkdir /sys/kernel/config/usb_gadget/android0/strings/0x409"
	mkdir /sys/kernel/config/usb_gadget/android0/strings/0x409
fi

if [ ! -d /sys/kernel/config/usb_gadget/android0/configs/c.1 ];then
    echo "mkdir /sys/kernel/config/usb_gadget/android0/configs/c.1"
	mkdir /sys/kernel/config/usb_gadget/android0/configs/c.1
fi

if [ ! -d /sys/kernel/config/usb_gadget/android0/configs/c.1/strings/0x409 ];then
    echo "mkdir /sys/kernel/config/usb_gadget/android0/configs/c.1/strings/0x409"
    mkdir /sys/kernel/config/usb_gadget/android0/configs/c.1/strings/0x409
fi

if [ ! -d /sys/kernel/config/usb_gadget/android0/functions/mass_storage.0 ];then
    echo "mkdir /sys/kernel/config/usb_gadget/android0/functions/mass_storage.0"
	mkdir /sys/kernel/config/usb_gadget/android0/functions/mass_storage.0
fi

exit 0

