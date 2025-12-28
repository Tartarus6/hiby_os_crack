# HiBy OS Crack
Cracking the firmware of HiBy's linux devices

## Scope
- For now, this project focuses on the HiBy R3ProII, as it's the only one I have. As far as I know, there are some minor differences between the firmwares on the different HiBy linux devices, but most things apply universally.
- The goal of this project is to make it possible to modify the HiBy OS firmware to add custom functionality.


## Unpacking the firmware
### Linux
**dependencies**
- 7zip (for `7z` command) (could probably use other tools instead)
- squashfs-tools (for `unsquashfs` command)

**extracting the upt file**
- The original firmware file is `r3proii.upt`. It's an ISO image.
- To extract that image, run `7z x r3proii.upt`
- That will have created a file and a folder. The file, `ota_config.in`, just says "current_version=0". The folder, `ota_v0/`, contains a bunch of stuff
![alt text](resources/ota_v0.png)
- There are a bunch of squashfs files, and a bunch of xImage files

**extracting the squashfs file**
- The squashfs files need to be concatenated to get the actual squashfs file that can be extracted
- To concatenate the squashfs files into one, run `cat rootfs.squashfs.* > rootfs.squashfs.all`. This created a new file called `rootfs.squashfs.all` that includes all of the concatenated data.
- To extract the squashfs file, run `unsquashfs rootfs.squashfs.all`. This will create a new folder called `squashfs-root`
- The `squashfs-root` folder is what acts as the linux root directory on the device when firmware is flashed
![alt text](resources/squashfs-root.png)

**extracting the xImage file**
- (TODO) I'm not sure what, if anything, should/could be done with this xImage file. I think it's just the kernel
- To concatenate the xImage files into one, run `cat xImage.* > xImage.all`. This creates a new file called `xImage.all` that contains the concatenated data
- Running `file xImage.all` returns "xImage.all: u-boot legacy uImage, Linux-4.4.94+, Linux/MIPS, OS Kernel Image (Not compressed), 3760128 bytes, Sat Aug 30 09:46:50 2025, Load Address: 0X80F00000, Entry Point: 0X80F00000, Header CRC: 0XA4A80BB9, Data CRC: 0XC79EED8C"

### Windows
(TODO)
For equivalent functionality on Windows, pleas use the Cygwin project and follow the Linux commands above.

## Repacking the firmware
(TODO)


## Notes
- (TODO, make sure the following is correct) The HiBy OS filesystem is read-only, since it's a squashfs image. Only mounted storage, like `sd_0` can be written to.
