# Firmware Repacking

## requirements
- 7zip (for `7z` command) (could probably use other tools instead)
- squashfs-tools (for `mksquashfs` command)
- cdrtools (for the `mkisofs` command)

## script

There is a script that does this automatically for the R3 Pro II located in `r3proii/unpacking_and_repacking`. Just look at the script to see how it works, it should be pretty well commented.


## steps
### Creating the Firmware File
(TODO)
- for now, just look at the script

### Placing the Firmware on the Device
1. Place the firmware (`.upt` file) in the root directory of the SD card
    - It is fine to leave other files/folders (such as music) on the SD card. Just make sure the firmware is in the root of the SD card
    - Can be done by removing the SD and using it directly, or over USB, (TODO) might also be possible to do through http site
2. Perform the update using one of the methods below

***Common Problems, Symptoms, and Fixes***
- **IMPORTANT** After the upgrade finishes successfully, the device should start on its own after only a few seconds. If it doesnt, use the failsafe method below to upload fixed firmware.
- If something is messed up with the md5 sums on the files (i.e. the md5 check files weren't properly updated/formatted) the firmware update will hang around 1/5 full on the bottom progress bar (in my testing)
- If the file permissions are wrong (testing still required), the firmware can still successfully be installed. But after finishing the device won't start up

### Method 1: If the Device Can Turn On (if hiby_player opens and works)
1. Place the firmware onto the device using above instructions
2. Go to the *system settings* -> *firmware update*
3. Press "Via SD-card"
4. Press "Ok"
5. The device will freeze for a few seconds then reboot into firmware update mode
6. After finishing the update, the device should reboot on its own and automatically start the `hiby_player` binary

### Method 2: Failsafe
*I think the device has to be off in order for this to work. It can be hard to tell whether the device is on or off if the current firmware is broken.*
**Make sure you have placed the firmware file on the device using above instructions**
1. Place the firmware onto the device using above instructions
2. Press and hold the *volume up* button and the *power* button together until the "HIBY" logo shows up
3. After finishing the update, the device should reboot on its own and automatically start the `hiby_player` binary
