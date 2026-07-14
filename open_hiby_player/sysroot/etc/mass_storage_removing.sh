#!/bin/sh
echo "mass_storage remove!" > /dev/console

sync
umount -l /tmp/mass_storage/$MDEV*
rm -rf /tmp/mass_storage/$MDEV*
