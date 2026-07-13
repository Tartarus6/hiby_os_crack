# Firmware Unpacking

This file describes how to unpack the firmware, as well as the main structure of the firmware files.

## Requirements
*The following packages need to be installed*
- 7zip (for `7z` command) (could probably use other tools instead)
- squashfs-tools (for `unsquashfs` command)
- vmlinux-to-elf (for formatting the kernel to use with qemu)
- `gdb`, `gdb-multiarch` for a QEMU debugger interface
- binwalk (to extract uImage. it's overkill for this, but it does all of the uImage extraction work automatically)


## Script
There is a script that does this automatically [for the R3ProII](/r3proii/unpacking_and_repacking/unpack-helper.sh) and another script [for the R1](/r1/unpacking_and_repacking/unpack-helper.sh).

## Steps for Manual Unpacking
Here's the instructions for manually unpacking the firmware. Just here for documentation, since the scripts can do it automatically.
### extracting the upt file
- The original firmware file is a `.upt` file. It's an ISO image.
    - *Note: in this guide, it will be refered to as `r3proii.upt`, since this is written for the R3ProII, but most, if not all, steps are the same for extracting the UPT firmware of other HiBy OS devices such as the R1*
- To extract that image, run `7z x r3proii.upt`
- That will have created a file and a folder. The file, `ota_config.in`, just says "current_version=0". The folder, `ota_v0/`, contains a bunch of stuff
![ota_v0](/resources/UNPACKING.md/ota_v0.png)
- There are a bunch of squashfs files, and a bunch of xImage files

### extracting the squashfs file
- The squashfs files need to be concatenated to get the actual squashfs file that can be extracted
- To concatenate the squashfs files into one, run `cat rootfs.squashfs.* > rootfs.squashfs.all`. This created a new file called `rootfs.squashfs.all` that includes all of the concatenated data.
    - *This should concatenate them in the proper numerical order. But if you're experiencing problems with the squashfs file, try making sure that the files are being concatenated in the correct order.*
- To extract the squashfs file, run `sudo unsquashfs rootfs.squashfs.all`. This will create a new folder called `squashfs-root`
    - ***Note: Due to the file permissions in `squashfs-root`, you will likely need sudo permission in order to write to many of the system files***
    - Running this command with sudo is needed in order to maintain the original file ownership and permissions. This is required in order to make a working repack of the firmware.
    - If run without sudo, the rootfs will be extracted just file, but with all of the file permissions set to your user.
- The `squashfs-root` folder is what acts as the linux root directory on the device when firmware is flashed
![squashfs-root](/resources/UNPACKING.md/squashfs-root.png)

### extracting the xImage file
- To concatenate the xImage files into one, run `cat xImage.* > xImage.all`. This creates a new file called `xImage.all` that contains the concatenated data
    - *This should concatenate them in the proper numerical order. But if you're experiencing problems with the xImage file, try making sure that the files are being concatenated in the correct order.*
- `xImage.all` is a u-boot image that contains a raw binary for the linux kernel
- To extract the kernel binary from the uImage, run `dd if=xImage.all of=Linux-4.4.94+.bin bs=64 skip=1`. that will create a file called `Linux-4.4.94+.bin`
- qemu wants the kernel in `.elf` format, so we need to turn the raw binary into an elf file
- to do that, run `vmlinux-to-elf Linux-4.4.94+.bin Linux-4.4.94+.elf`
