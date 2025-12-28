#!/bin/sh

INTERFACE=wlan0
WPA_CONF_DEFAULT=/etc/wpa_supplicant_default.conf
WPA_CONF=/data/wpa_supplicant.conf
HOSTNAME_FILE="/usr/resource/hostname"

# stop already exist process
killall udhcpc > /dev/null
killall wpa_supplicant > /dev/null

# wpa_supplicant config file
if [ ! -f "$WPA_CONF" ]; then
    cp $WPA_CONF_DEFAULT $WPA_CONF
fi

HOSTNAME=HiBy_Music

if [ -f "$HOSTNAME_FILE" ]
then
	HOSTNAME=`cat /usr/resource/hostname`
fi

nvram_patch=`sa_config bcmpatch_path /firmware/nvram_patch.txt`
fw_patch=`sa_config bcmpatch_path /firmware/fw_patch.txt`

if [ -f "$nvram_patch" ]; then
    echo $nvram_patch
    echo $nvram_patch > /sys/module/bcmdhd/parameters/nvram_path
fi

if [ -f "$fw_patch" ]; then
    echo $fw_patch
    echo $fw_patch > /sys/module/bcmdhd/parameters/firmware_path
fi

ifconfig $INTERFACE up
wpa_supplicant -Dnl80211 -i$INTERFACE -c$WPA_CONF -B
usleep 1300000
echo $HOSTNAME
udhcpc -b -i $INTERFACE -q -x hostname:$HOSTNAME &

exit 0

