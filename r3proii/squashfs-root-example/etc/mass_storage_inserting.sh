#!/bin/sh
echo "mass_storage insert!" > /dev/console

if [ -e "/dev/$MDEV" ]; then
    mkdir -p /tmp/mass_storage/$MDEV
    mount -rw -o sync /dev/$MDEV /tmp/mass_storage/$MDEV
fi
