# Installing Firmware
This guide explains how to install `.upt` firmware on HiBy OS devices. This guide works for the R1, R3, R3 Pro II, and any other device using `.upt` HiBy OS firmware.

Older HiBy devices, including the R3 Pro and R3 Pro Saber, use a different firmware file type. So I cannot guarantee that this guide is fully correct for those devices. For those devices, check out [hiby-firmware-tools](https://github.com/SuperTaiyaki/hiby-firmware-tools) by SuperTaiyaki on GitHub.


## Common Problems, Symptoms, and Fixes
- **IMPORTANT** After the upgrade finishes successfully, the device should start on its own after only a few seconds. If it doesnt, use the failsafe method below to upload fixed firmware.
- If something is messed up with the md5 sums on the files (i.e. the md5 check files weren't properly updated/formatted) the firmware update will hang around 1/5 full on the bottom progress bar (in my testing)
    - *To fix this issue*: correct the md5 hashing
    - *To recover the device*: Just hold the power button to shut down the device, remove the bad `.upt` file from the SD card, place the fixed `.upt` file on the device, then use the failsafe install method below to install the fixed firmware.
- If the file permissions are wrong (testing still required), the firmware can still successfully be installed. But after finishing the device won't start up
    - *To fix this issue*: correct the file permissions (more testing needs to be done to know exactly what works and what doesnt)
    - *To recover the device*: Shut down the device (if it's not already), remove the bad `.upt` file from the SD card, place the fixed `.upt` file on the device, then use the failsafe install method below to install the fixed firmware.


## Restoring the Original Firmware
Installing custom firmware on a HiBy OS device is completely, and easily, reversible. The process for restoring the original firmware is the exact same process as installing custom firmware.

To restore the stock firmware, just get the firmware `.upt` file from HiBy (or whatever) and follow the steps below to place the firmware file on the device and to install that firmware.

Once the stock firmware has been installed, your device will be back to it's stock state.


## Placing the Firmware File on the Device
Place the firmware (`.upt` file) in the root directory of the SD card
- It is fine to leave other files/folders (such as music) on the SD card. Just make sure the firmware is in the root of the SD card
- Can be done by removing the SD and using it directly, or over USB, (TODO) might also be possible to do through http site

## Installing the New Firmware
*Make sure you've placed the firmware file on the device before trying to install a new one. Otherwise nothing will happen.*
Once the firmware file is on the device, you can install it.

### Method 1: If the Device Can Turn On (if `hiby_player` opens and works)
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
