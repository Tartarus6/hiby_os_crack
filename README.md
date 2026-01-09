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
- binwalk (to extract uImage. it's overkill for this, but it does all of the uImage extraction work automatically)
- vmlinux-to-elf (for formatting the kernel to use with qemu)

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
- To concatenate the xImage files into one, run `cat xImage.* > xImage.all`. This creates a new file called `xImage.all` that contains the concatenated data
- `xImage.all` is a u-boot image that contains a raw binary for the linux kernel
- To extract the kernel binary from the uImage, run `dd if=xImage of=Linux-4.4.94+.bin bs=64 skip=1`. that will create a file called `Linux-4.4.94+.bin`
- qemu wants the kernel in elf format, so we need to turn the raw binary into an elf file
- to do that, run `vmlinux-to-bin Linux-4.4.94+.bin Linux-4.4.94+.elf`

## Windows
(TODO) For equivalent functionality on Windows, please use the Cygwin project and follow the Linux commands above. I broke a lot of things here, need to be patched before release ready.

### Cygwin Specific Depedencies
*If you are using the Cygwin project, please install the following packages as instructed INSTEAD of following the traditional linux dependencies*

**1. Use the Cygwin installer to install the latest (preferably non-test) versions of:**

`make`, `gcc-core`, `g++`, `python3`, `python3-pip`, `p7zip`, `git`, `python3-zstandard`, `python3-cffi`, `python3-devel`, `libffi-devel`, `libzstd-devel`, `zlib-devel`**

Also install: `python312-devel` or the equivalent development package for the python build on your system.

Note: *GCC is not sufficient without g++.*

Note: *If you already use Python's Windows build in Cygwin natively, please make sure to ALSO INSTALL the Cygwin variant (conversion will be automatic due to path preferencing) to prevent filepath errors in step 4. (ADVANCED) If necessary, please create a bash alias so you can install invoke the windows copy where needed. Please make sure that pip is not the Windows copy, as it breaks many filepaths.*

**3. Install squashfs-tools**

`wget https://github.com/plougher/squashfs-tools/archive/refs/tags/4.5.1.tar.gz`

`tar -xf 4.5.1.tar.gz`

`cd squashfs-tools-4.5.1/squashfs-tools`

`wget http://files.glitchtech.top/squashfs-tools-cygwin.patch`

`patch -p0 < squashfs-tools-cygwin.patch`

`make`

`make install`

**4. Install vmlinux-to-elf**

`pip3 install git+https://github.com/marin-m/vmlinux-to-elf`

**5. Install binwalk**

First install rustup (for Windows)

`cd ~`

`curl -o rustup-init.exe https://win.rustup.rs/x86_64`

`./rustup-init.exe`

Add this to your .bashrc

`export PATH="/cygdrive/c/Users/YOUR_USERNAME/.cargo/bin:$PATH"`

`source ~/.bashrc`

Now install dependencies and binwalk

`cargo install maturin --locked`

`pip3 install uefi_firmware jefferson ubi-reader setuptools-rust`

`pip3 install --no-deps --no-build-isolation maturin || true`

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
