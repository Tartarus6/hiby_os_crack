#!/bin/sh
dev_node="/sys/kernel/config/usb_gadget/adb_demo/UDC"
otg_name=`ls /sys/class/udc/`

echo $otg_name > $dev_node

