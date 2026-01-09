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

