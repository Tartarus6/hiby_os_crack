# Accessing ADB

The HiBy OS devices use ADB (Android Debug Bridge) to allow us to connect to their terminals through a USB connection.

> [!NOTE]
> While ADB does have "Android" in the name, the devices do not use android and you do not need any android sdk or anything to connect. You can just install ADB on your computer, and then use the ADB commands in your terminal to interface with the device.

## Requirements
- `adb` command (on Arch you can get this from the `android-tools` package)

## Enabling ADB on the HiBy OS Device
### With Stock Firmware
ADB can be enabled on unmodified firmware using the steps below.
1. Enter the main settings menu
2. Scroll all the way to the bottom and enter the "About" page
3. Press the "About" title text a bunch of times (about 10 times i think) until a popup appears saying that test mode is on
4. Now ADB should be on :)

> [!NOTE]
> If you don't have the "About" page, it can be enabled through a single-line modification in the firmware.
>
> In `/usr/resource/set_functions.json`, change `{"about":0}` into `{"about":1}`

### With the Rockbox Bootloader
If you're using a modded firmware which includes the Rockbox bootloader, you can just enter the tools menu from the bootloader and select "ADB Enable".

## Using ADB
The primary use for ADB is connecting to the terminal, but ADB also has other features, such as transfering files.

With ADB enabled on the HiBy OS device and with it plugged into your computer, run `adb shell` in your console. Now you should be in the HiBy OS device's terminal.
