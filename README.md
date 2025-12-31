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

## Windows
(TODO)
For equivalent functionality on Windows, please use the Cygwin project and follow the Linux commands above.
### Cygwin Specific Depedencies
*If you are using the Cygwin project, please install the following packages as instructed INSTEAD of following the traditional linux dependencies*

**1. Use the Cygwin installer to install the latest (preferably non-test) versions of:

`make`, `gcc-core`, `g++`, `python3`, `python3-pip`, `p7zip`, `git`, `python3-zstandard`, `python3-cffi`, `python3-devel`, `libffi-devel`, `libzstd-devel`, `mingw64-x86_64-zstd`, `mingw64-x86_64-gcc-core`, `mingw64-x86_64-gcc-g++`, `mingw64-x86_64-binutils`, `mingw64-x86_64-runtime`, `mingw64-x86_64-headers`, `mingw64-x86_64-expat`**

Also install: `python312-devel` or the equivalent development package for the python build on your system.

Note: *GCC is not sufficient without g++.*

Note: *If you already use Python's Windows build in Cygwin natively, please make sure to ALSO INSTALL the Cygwin variant (conversion will be automatic due to path preferencing) to prevent filepath errors in step 4. (ADVANCED) If necessary, please create a bash alias so you can install invoke the windows copy where needed. Please make sure that pip is not the Windows copy, as it breaks many filepaths.*

**3. Install squashfs-tools**

Download the latest source copy of squashfs-tools or the version below (any version should suffice)

(TODO: Not working until I add some patch fixes for Cygwin. Users can manipulate binwalk for equivalent functionality in the meantime.)

`wget https://github.com/plougher/squashfs-tools/releases/download/4.7.4/squashfs-tools-4.7.4.tar.gz`

`tar -xf squashfs-tools-4.7.4.tar.gz`

`cd squashfs-tools-4.7.4/squashfs-tools`

`make`

Working solution:

We only need unsquashfs, which will be formatted into a cheap .bashrc alias from binwalk (Requires step 5 to work). (TODO: Remove once patch is available.)

`echo 'unsquashfs() { binwalk -e "$1" && [ -d "_${1}.extracted/squashfs-root" ] && echo "Extracted to _${1}.extracted/squashfs-root"; }' >> ~/.bashrc`

**4. Install vmlinux-to-elf**

`pip3 install git+https://github.com/marin-m/vmlinux-to-elf`

**5. Install binwalk**

Install Rust for Linux with default configuration: `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh`

Note: *Rust on Windows will not work, you will need the additional Cygwin local install to keep things from breaking.*

`wget https://static.rust-lang.org/dist/rust-1.92.0-x86_64-unknown-linux-gnu.tar.xz`

`tar -xvf rust-1.92.0-x86_64-unknown-linux-gnu.tar.xz`

`cd rust-1.92.0-x86_64-unknown-linux-gnu`

`./install.sh`

Note: *This command may take an awfully long time to complete*

Now reopen Cygwin and run the following

`pip3 install uefi_firmware jefferson ubi-reader`

`git clone https://github.com/ReFirmLabs/binwalk.git`

`cd binwalk`

`cargo build --release`

**6. Install QEMU (Windows)**

Download and run in Windows: `https://qemu.weilnetz.de/w64/qemu-w64-setup-20251224.exe`

Note: *We recommend you choose the default program filepath to avoid breaking Cygwin*

Install `qemu-integration` from the Cygwin installer (this is just a linking program that translates the windows binaries into linux-reachable commands)

**7. Install e2fsprogs**

Install `e2fsprogs` through the Cygwin installer

## Repacking the firmware
(TODO)


## Notes
- (TODO, make sure the following is correct) The HiBy OS filesystem is read-only, since it's a squashfs image. Only mounted storage, like `sd_0` can be written to.
