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

### Method 1: Automated Kernel Log Capture (Recommended)
Automatically captures kernel log buffer via GDB - no manual steps required.

**Run with automated capture:**
```bash
./run_qemu.sh -initrd -capture-kernel-log
```

This will:
1. Start QEMU in background
2. Connect GDB automatically
3. Wait for kernel panic (or timeout after 35s)
4. Extract kernel log buffer (`__log_buf`)
5. Save logs to `/tmp/qemu_kernel.log` and `/tmp/qemu_backtrace.txt`

**Customize wait time:**
```bash
./run_qemu.sh -initrd -capture-kernel-log -kernel-wait 45
```

**Output files:**
- `/tmp/qemu.log` - QEMU debug output (instructions, interrupts, etc.)
- `/tmp/qemu_kernel.log` - Kernel log buffer contents
- `/tmp/qemu_backtrace.txt` - Stack backtrace at panic

### Method 2: QEMU Monitor (Good for Windows/Cygwin)
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

### Method 3: GDB (Manual Method)
For detailed debugging with full GDB features. Use this when you need interactive control.

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

### Method 4: QEMU Log Files
QEMU writes detailed logs to `/tmp/qemu.log` including instruction traces, interrupts, and unimplemented device accesses. Use `-dlight`, `-dmedium`, or `-dfull` flags to control verbosity.

## Quick Examples
```bash
# Automated kernel log capture (recommended for analysis)
./run_qemu.sh -initrd -capture-kernel-log

# Automated capture with custom wait time
./run_qemu.sh -initrd -capture-kernel-log -kernel-wait 45

# Automated capture with quiet mode (logs only)
./run_qemu.sh -initrd -capture-kernel-log -quiet

# Run with monitor access, start immediately
./run_qemu.sh -initrd -no-pause

# Run with example rootfs, paused for debugging
./run_qemu.sh -initrd -example

# Test individual binary in usermode
./run_qemu_usermode.sh /usr/bin/hiby_player --help

# Test with example rootfs
./run_qemu_usermode.sh -example /usr/bin/some-app
```
