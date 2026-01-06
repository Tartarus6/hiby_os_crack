#!/bin/sh

VID=0x32BB
PID=0x0004

UDC=`ls /sys/class/udc/` # UDC=13500000.otg

prg_name=$0
status=$1
path_array=$2

num=0
for file_path in $@
do
	if [ "$num" -ge "2" ]; then
		path_array="$path_array $file_path"
	fi
	let num=num+1
done

set_usb_descriptor() {
	# 配置设备描述符
	echo "Setting Device Descriptor..."
	echo "0x00" > bDeviceClass
	echo "0x0200" > bcdUSB
	echo "0x0100" > bcdDevice
	echo $VID > idVendor
	echo $PID > idProduct

	# 配置字符串描述符
	echo "Setting English strings..."
	mkdir strings/0x409
	echo "HiBy" > strings/0x409/manufacturer
	echo "R1" > strings/0x409/product
	echo "HiBy R1" > strings/0x409/serialnumber

	# 配置配置描述符
	echo "Creating Config..."
	mkdir configs/c.1/
	echo "500" > configs/c.1/MaxPower
	echo "0x80" > configs/c.1/bmAttributes
	mkdir configs/c.1/strings/0x409/
	echo "storage" > configs/c.1/strings/0x409/configuration
}

get_mass_storage_dev_name() {
	local mount_path=$1

	mass_storage_all_dev=`ls functions/mass_storage.* -d` &> /dev/null
	for mass_storage_dev in $mass_storage_all_dev
	do
		mass_storage_dev=${mass_storage_dev#functions/}
		dev_path=`cat functions/$mass_storage_dev/lun.0/file`
		if [ "$dev_path" = "$mount_path" ]; then
			echo $mass_storage_dev
			return 0
		fi
	done

	return 1
}

add_img() {
	local mount_path=$1

	if [ ! -f $mount_path ] && [ ! -b $mount_path ]; then
		echo "mount_path is null"
		#exit 1
	fi

	get_mass_storage_dev_name $mount_path
	if [ "$?" = "0" ]; then
		echo "Warning: $mount_path already mount!"
		exit 1
	fi

	for i in $(seq 0 15)
	do
		ls functions/mass_storage.$i &> /dev/null
		if [ "$?" = "0" ]; then
			continue
		fi

		#初始化挂载目录
		mkdir functions/mass_storage.$i/
		if [ "$?" != "0" ]; then
			echo "mkdir functions/mass_storage.$i faild"
			exit 1
		fi

		echo 0 > functions/mass_storage.$i/lun.0/ro
		echo 1 > functions/mass_storage.$i/lun.0/removable
		echo 0 > functions/mass_storage.$i/lun.0/nofua
		echo 0 > functions/mass_storage.$i/lun.0/cdrom
		
		if [ ! -f $mount_path ] && [ ! -b $mount_path ]; then
			echo "" > functions/mass_storage.$i/lun.0/file
		else
			echo $mount_path > functions/mass_storage.$i/lun.0/file
		fi

		ln -s functions/mass_storage.$i configs/c.1
		break
	done
}

# 配置mass storage设备
storage_start() {
	if [ ! -d /sys/kernel/config/usb_gadget ]; then
		echo "Creating the USB gadget"
		mount -t configfs none /sys/kernel/config
	fi

	if [ ! -d /sys/kernel/config/usb_gadget/android0 ]; then
		echo "Creating gadget directory storage_demo"
		mkdir /sys/kernel/config/usb_gadget/android0
		if [ ! -d /sys/kernel/config/usb_gadget/android0 ]; then
			echo "Error: usb_gadget busy, can't create gadget directory!"
			exit 1
		fi

		cd /sys/kernel/config/usb_gadget/android0
		set_usb_descriptor
	fi

	cd /sys/kernel/config/usb_gadget/android0
	udc_status=`cat UDC`
	if [ "$udc_status" != "" ]; then
		echo "" > UDC
	fi

	#初始化挂载目录
	for path in $path_array
	do
		add_img $path
	done

	echo "Binding USB Device Controller"
	echo $UDC > UDC
	echo "ok"
}

# 卸载mass storage设备
storage_stop() {
	echo "stopping the USB gadget"

	if [ ! -d /sys/kernel/config/usb_gadget/android0 ]; then
		echo "Error: usb configfs storage_demo uninitialized"
		exit 1
	fi
	cd /sys/kernel/config/usb_gadget/android0

	echo "Unbinding USB Device controller..."
	echo "" > UDC
	echo "ok"

	for path in $path_array
	do
		mass_storage_path=`get_mass_storage_dev_name $path`
		if [ "$?" != "0" ]; then
			#echo "mass_storage dev:$path has no find! Please input true dev name"
			#exit 1
			mass_storage_path=mass_storage.0
		fi

		echo "Deleting mass storage gadget functionality : "
		rm configs/c.1/$mass_storage_path
		rmdir functions/$mass_storage_path
		echo "Delete mass_storage dev:$path ok"

	done

	ls functions/mass_storage.* -d &> /dev/null
	if [ "$?" = "0" ]; then
		echo $UDC > UDC
		return 0
	fi

	echo "Cleaning up configuration..."
	rmdir configs/c.1/strings/0x409
	rmdir configs/c.1
	echo "ok"

	echo "cleaning English string..."
	rmdir strings/0x409
	echo "ok"

	echo "Removing gadget directory..."
	cd - &> /dev/null
	rmdir /sys/kernel/config/usb_gadget/android0/
	echo "ok"

	umount /sys/kernel/config
}

case "$status" in
  start)
	if [ "$#" -le "1" ]; then
		echo "Usage1: $prg_name start <mount_path,mount_path.....>"
		exit 1
	fi

	# 配置storage设备函数
	storage_start
	;;

  stop)
	if [ "$#" -le "1" ]; then
		echo "Usage: $prg_name stop <mount_path mount_path....."
		exit 1
	fi

	if [ ! -d /sys/kernel/config/usb_gadget/android0/ ]; then
		echo "Error: usb configfs storage_demo uninitialized"
		exit 1
	fi

	# 卸载mass storage设备
	storage_stop
	;;
	*)
	echo "Usage1: $prg_name start <mount_path mount_path.....>"
	echo "Usage2: $prg_name stop <mount_path mount_path....."
	exit 1
esac