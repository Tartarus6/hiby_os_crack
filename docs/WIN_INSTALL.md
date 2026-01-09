## Windows
For equivalent functionality on Windows, choose:

(A) Install the Cygwin project, which keeps the majority of the software used in it's native packaging for Windows, which is translated in a Linux terminal for equivalent command functionality. The details of this install are kept below in this document.

(B - TODO: UNTESTED) Install the Windows Subsystem for Linux and follow the original README.md

(C - TODO: UNTESTED) Use a virtual machine to run a Linux distribution and follow the original README.md

### Cygwin Setup for HiBy OS Crack
*If you are using the Cygwin project, please install the following packages as instructed INSTEAD of following the traditional linux dependencies*

**1. Use the Cygwin installer to install the latest (preferably non-test) versions of:**

`make`, `gcc-core`, `g++`, `python3`, `python3-pip`, `p7zip`, `git`, `python3-zstandard`, `python3-cffi`, `python3-devel`, `libffi-devel`, `libzstd-devel`, `zlib-devel`, `e2fsprogs`, `qemu-integration`

Also install: `python312-devel` or the equivalent development package for the python build on your system.

**2. Install squashfs-tools**

`wget https://github.com/plougher/squashfs-tools/archive/refs/tags/4.5.1.tar.gz`

`tar -xf 4.5.1.tar.gz`

`cd squashfs-tools-4.5.1/squashfs-tools`

`wget http://files.glitchtech.top/squashfs-tools-cygwin.patch`

`patch -p0 < squashfs-tools-cygwin.patch`

`make`

`make install`

**3. Install vmlinux-to-elf**

`pip3 install git+https://github.com/marin-m/vmlinux-to-elf`

**4. Install binwalk**

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

**5. Install QEMU (Windows)**

Download and run in Windows: `https://qemu.weilnetz.de/w64/qemu-w64-setup-20251224.exe`
