#!/bin/sh

#clear the bluetooth
rm -rf /var/lib/bluetooth
rm -rf /var/lib/dbus
rm -rf /usr/var

# factory_mode file is special for SMSL_DP5 project, other items can be ignored
myFile_c2="/data/factory_mode_c2"
myFile_c3="/data/factory_mode_c3"

if [ -f "$myFile_c2" ]; then
        cp /data/factory_mode_c2 /usr/resource/
fi

if [ -f "$myFile_c3" ]; then
        cp /data/factory_mode_c3 /usr/resource/
fi

#clear data vol

if [ $1 = 1 ];then
	filelist=`ls /data/`
	for file in $filelist
	do
		if [ "$file" != 'usrlocal_media.db' ];then
			rm -rf "/data/"$file
			#echo "/data/"$file
		fi
	done
else
	rm -rf /data/*
fi

u_myFile_c2="/usr/resource/factory_mode_c2"
u_myFile_c3="/usr/resource/factory_mode_c3"

if [ -f "$u_myFile_c2" ]; then
        cp /usr/resource/factory_mode_c2 /data/
		echo "/copy factory_mode_c2/"
fi

if [ -f "$u_myFile_c3" ]; then
        cp /usr/resource/factory_mode_c3 /data/
		echo "/copy factory_mode_c3/"
fi
