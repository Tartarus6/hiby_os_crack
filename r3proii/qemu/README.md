# QEMU Section Readme

this folder is for stuff needed to emulate the device

## Folder Structure
### Scripts
- `run_qemu.sh` starts a qemu vm with multiple debug/access options
  - `-initrd` flag to use initrd image (otherwise needs image from `create_rootfs_image.sh`)
  - `-no-pause` to start running immediately (default is paused)
  - `-example` to use squashfs-root-example instead of squashfs-root
- `run_qemu_usermode.sh` can run individual binaries emulating the XBurstR2 CPU
  - `-example` to use squashfs-root-example
- `create_rootfs_image.sh` (requires sudo) bundles squashfs-root into an ext4 image

### Other Files
- `initrd.cpio` - initrd image for `-initrd` mode
- `rootfs-image` - rootfs image for default mode

## Debugging Methods

### Method 1: QEMU Monitor (Recommended for Windows/Cygwin)
The QEMU monitor provides interactive access without needing GDB.

**Start QEMU:**
```bash
./run_qemu.sh -initrd          # Starts paused, serial output in terminal
```

**Connect to Monitor (in another terminal):**
```bash
telnet 127.0.0.1 4444
```

**Useful Monitor Commands:**
- `c` - Continue execution
- `stop` - Pause execution
- `info registers` - Show CPU registers
- `x /10i $pc` - Disassemble 10 instructions at PC
- `x /32xw $sp` - Examine 32 words at stack pointer
- `info mem` - Show memory mappings
- `system_reset` - Reset the system
- `q` - Quit QEMU

**Run without pausing:**
```bash
./run_qemu.sh -initrd -no-pause    # Starts running immediately
```

### Method 2: GDB (Traditional Method)
For detailed debugging with full GDB features.

**Start QEMU:**
```bash
./run_qemu.sh -initrd
```

**Connect GDB (in another terminal):**
```bash
gdb ../Linux-4.4.94+.elf
(gdb) target remote :1234
(gdb) continue
# Wait for panic (10-30 seconds)
(gdb) print __log_buf
(gdb) x/2000s __log_buf
(gdb) bt
```

### Method 3: Log Files
QEMU writes detailed logs to `/tmp/qemu.log` including instruction traces, interrupts, and unimplemented device accesses.

## Quick Examples
```bash
# Run with monitor access, start immediately
./run_qemu.sh -initrd -no-pause

# Run with example rootfs, paused for debugging
./run_qemu.sh -initrd -example

# Test individual binary in usermode
./run_qemu_usermode.sh /usr/bin/hiby_player --help

# Test with example rootfs
./run_qemu_usermode.sh -example /usr/bin/some-app
```
