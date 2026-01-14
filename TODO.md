# Todo List

## This Project
- [ ] probably should move linux kernel bin and elf into qemu folder
- [ ] figure out what "burn" mode does (manual says entered by holding the next song button)
- [ ] store a copy of the r3proii [user manual](https://guide.hiby.com/en/docs/products/audio_player/hiby_r3proii/guide)
- [ ] figure out how to better manage file permissions in rootfs (currently, nearly every file is owned by root and has write protection. this makes it difficult to modify and difficult to upload through git)
- [ ] add a README somewhere that explains the major structure of the root filesystem (like where `hiby_player` is, where useful images are, etc.)
- [x] add vm image files `rootfs-image` and `initrd.cpio` to gitignore
- [x] firmware unpacking script
- [x] firmware repacking script


## Emulator
*Goal: creating a workflow that allows emulating the hiby devices to speed up testing and let people test without hardware*
- [ ] get kernel to run the init kernel function
- [ ] successfully load into file system
- [ ] run with ingenic x1600e features
- [ ] display output
- [ ] fake touch control interface
- [ ] sound output
- [ ] (maybe) usb interface

## hiby_player Decomp
*Goal: get `hiby_player` in a state where new buttons, pages, and features (i.e. audiobook support) can be added*
- [ ] de-obfuscate gui rendering
- [ ] figure out how to add a new button
- [ ] figure out how to add a new page
- [x] make the first functional change (tested by changing the number of presses to bring up dev mode dialog from 3 to 4)

## Custom Firmware
- [ ] keep developer mode page visible when developer mode is off (there is a dev mode toggle in the dev mode page)
- [ ] allow for much lower brightnesses (could use backlight to a point, then use overlay. point in slider where overlay gets used should be marked, like how vol over 100% is done in some programs)
- [ ] add audiobooks button to books menu
- [ ] create audiobooks page
- [ ] add support for playing audiobooks
- [ ] make device open onto the playback page rather than where it was
- [ ] add setting for opening onto playback page
- [ ] easier playlist access
- [ ] better playlist menu
- [ ] fix some album art not loading
- [ ] make main page style same as the rest of the pages (its styled different for some reason)
- [ ] charge limit (to conserve battery health)
- [ ] combine all setting menus by using settings tabs (i.e. general settings, playback settings, Bluetooth settings, etc.)
- [ ] built-in custom radio creation/management (currently have to put it in the right format in a txt file)
- [ ] fix setting font size bringing you to the all songs menu (no idea why this happens)
- [ ] (if possible) fix Bluetooth connection usually taking multiple attempts
- [ ] fix bluetooth menu slow response (after turning on bluetooth, it can take quite a while for the rest of the bluetooth settings to appear)
- [ ] fix wifi menu slow response (after turning on wifi, it can take quite a while for the rest of the wifi settings to appear)
- [ ] fix very inconsistent and unintuitive settings (backlight settings vs. time setting, USB working mode needs descriptions, etc.)
- [ ] shrink file system where possible

## Windows Support
- [x] Windows devices should be able to install all project dependencies and run qemu

## QEMU Development Progress Tracking Guide

This section provides a comprehensive methodology for testing and tracking QEMU emulation development progress based on log file analysis. Use this to systematically determine what stage of development you're at and what needs to be done next.

### Quick Progress Assessment

Run `./run_qemu.sh -no-pause` and check the output. Find your current stage by matching log patterns below:

| Stage | Log Signature | Status |
|-------|---------------|--------|
| **Stage 0** | "qemu-system-mipsel not found" | Not Started |
| **Stage 1** | QEMU starts, kernel loads, early boot messages appear | In Progress |
| **Stage 2** | "x1600 Clock Power Management Unit init!" + all clocks = 0 | **CURRENT** |
| **Stage 3** | Clock values non-zero, kernel proceeds past workqueue init | Not Started |
| **Stage 4** | "VFS: Mounted root" or filesystem mount messages | Not Started |
| **Stage 5** | init process starts, "INIT: version X.XX booting" | Not Started |
| **Stage 6** | User applications run, hiby_player launches | Not Started |
| **Stage 7** | Display/touch/audio peripherals functional | Not Started |

### Detailed Stage Descriptions

#### **STAGE 0: Build Environment Setup**
**Goal:** Get QEMU built and runnable

**Success Criteria:**
- `qemu-system-mipsel` binary exists and is executable
- Running `qemu-system-mipsel --version` shows XBurstR2 CPU support
- Can execute `./run_qemu.sh -h` without errors

**Log Patterns to Look For:**
- Build logs show successful compilation
- No "command not found" errors

**How to Test:**
```bash
which qemu-system-mipsel
qemu-system-mipsel --cpu help | grep -i xburst
./run_qemu.sh -h
```

**Troubleshooting:**
- If qemu-system-mipsel not found: Run `scripts/build_qemu.sh`
- Check PATH includes `/usr/local/bin`

**Current Status:** ✅ COMPLETE

---

#### **STAGE 1: Kernel Loading & Early Boot**
**Goal:** Get kernel to load into memory and start basic CPU initialization

**Success Criteria:**
- Kernel ELF file loads without errors
- CPU revision detected: "CPU0 revision is: 2ed1024f (Xburst)"
- FPU detected: "FPU revision is: 00739300"
- Machine type identified: "MIPS: machine is ingenic,x1600_halley6_module_base"
- Memory zones initialized

**Log Patterns to Look For:**
```
Linux version 4.4.94+ (zcz@androidserver3)
bootconsole [early0] enabled
CPU0 revision is: 2ed1024f (Xburst)
FPU revision is: 00739300
MIPS: machine is ingenic,x1600_halley6_module_base
Determined physical RAM map:
 memory: 04000000 @ 00000000 (usable)
```

**How to Test:**
```bash
cd r3proii/qemu
./run_qemu.sh -no-pause -dminimal > test_stage1.log 2>&1
grep "CPU0 revision" test_stage1.log
grep "machine is" test_stage1.log
```

**Log File Locations:**
- Real-time: Terminal output or `/tmp/qemu.log`
- Kernel buffer: Extract via GDB: `x /3000s __log_buf+8`

**Troubleshooting:**
- No output: Check kernel path in run_qemu.sh:136
- Wrong machine: Kernel expecting x1600_halley6 but QEMU uses malta board
- Memory issues: Memory must be exactly 64M

**Current Status:** ✅ COMPLETE

---

#### **STAGE 2: Clock Power Management (CPM) Initialization** ⚠️ **CURRENT BLOCKER**
**Goal:** Get CPM hardware to return non-zero clock values

**Success Criteria:**
- CPM init message appears: "x1600 Clock Power Management Unit init!"
- All major clocks report NON-ZERO values:
  - apll > 0 (should be ~1000000000 Hz / 1 GHz)
  - mpll > 0 (should be ~1200000000 Hz / 1.2 GHz)
  - cpu_clk > 0
  - ddr > 0
- Kernel proceeds past clock initialization without panic

**Log Patterns to Look For - FAILURE (current state):**
```
========== x1600 clocks: =============
    apll     = 0 , mpll     = 0, ddr = 0
    cpu_clk  = 0 , l2c_clk  = 0
    ahb0_clk = 0 , ahb2_clk = 0
    apb_clk  = 0 , ext_clk  = 24000000

CPU 0 Unable to handle kernel paging request at virtual address 00000080
```

**Log Patterns to Look For - SUCCESS (target state):**
```
========== x1600 clocks: =============
    apll     = 1000000000 , mpll     = 1200000000, ddr = 600000000
    cpu_clk  = 1000000000 , l2c_clk  = 500000000
    ahb0_clk = 200000000 , ahb2_clk = 300000000
    apb_clk  = 100000000 , ext_clk  = 24000000
```

**How to Test:**
```bash
cd r3proii/qemu
./run_qemu.sh -no-pause 2>&1 | grep -A 5 "x1600 clocks"
# Check if all values are non-zero

# For detailed analysis:
./run_qemu.sh -no-pause -dlight > cpm_test.log 2>&1
grep "0x10000000" cpm_test.log  # CPM register accesses
```

**Log File Locations:**
- Kernel messages: `/tmp/qemu.log`
- Clock dump: `r3proii/qemu/logs/log_buffer.txt` (lines 107-115)
- Backtrace: `r3proii/qemu/logs/backtrace.txt`

**Why This Fails:**
- QEMU using generic Malta board (no X1600-specific hardware)
- CPM registers at 0x10000000 not implemented
- Kernel reads return 0 or garbage values
- Division by zero in clock calculations causes NULL pointer dereference

**What's Needed:**
- Create `qemu/hw/mips/x1600_halley6.c` machine definition
- Implement CPM device at memory address 0x10000000
- Register reads must return realistic PLL values:
  - CPCCR (0x10000000): CPU clock control register
  - CPPCR (0x10000010): PLL control register
  - CPAPCR (0x10000010): APLL control register
  - CPMPCR (0x10000014): MPLL control register

**Troubleshooting:**
- Enable QEMU MMIO tracing: `./run_qemu.sh -dflags "guest_errors,unimp,exec" -no-pause`
- Look for unimplemented device warnings at 0x10000000
- Check kernel source: Should be reading from CPM_BASE + offsets

**Current Status:** ❌ BLOCKED - CPM hardware not implemented

---

#### **STAGE 3: Operating System Timer (OST) & Interrupts**
**Goal:** Get system timer working and interrupt controller operational

**Success Criteria:**
- OST device initializes: "clocksource: ingenic_clocksource: mask: 0xffffffffffffffff max_cycles: 0x1623fa770"
- Sched clock working: "sched_clock: 64 bits at 1500kHz"
- Timer interrupts fire correctly
- Workqueue subsystem initializes successfully
- RCU (Read-Copy-Update) operational
- No "Unable to handle kernel paging request" errors

**Log Patterns to Look For:**
```
clocksource: ingenic_clocksource: mask: 0xffffffffffffffff
sched_clock: 64 bits at 1500kHz, resolution 666ns
Preemptible hierarchical RCU implementation.
workqueue: round-robin CPU selection forced, expect performance impact
Calibrating delay loop...
```

**How to Test:**
```bash
./run_qemu.sh -no-pause -dlight 2>&1 | grep -E "(clocksource|sched_clock|workqueue|timer)"

# Check for interrupt handling
./run_qemu.sh -no-pause -dflags "int,guest_errors" 2>&1 | grep -i interrupt
```

**Log File Locations:**
- `/tmp/qemu.log` - Real-time boot progress
- Check for timer-related kernel panics

**What's Needed:**
- Implement OST device at 0x12000000
- Ingenic interrupt controller (INTC) at 0x10001000
- Timer must generate interrupts at correct frequency (1500 kHz)
- INTC must route interrupts to MIPS CPU IRQ lines

**Troubleshooting:**
- If stuck after CPM but before workqueue: OST not implemented
- If "do_IRQ" spam in logs: Interrupt controller misconfigured
- If no timer interrupts: Check OST frequency and interrupt routing

**Current Status:** ❌ BLOCKED - Dependent on Stage 2

---

#### **STAGE 4: Filesystem Mount**
**Goal:** Successfully mount root filesystem

**Success Criteria:**
- VFS (Virtual File System) initializes
- Root device detected
- Filesystem type identified (ext4 or squashfs)
- Successful mount: "VFS: Mounted root (ext4 filesystem) readonly on device X:Y"
- No "cannot open root device" errors

**Log Patterns to Look For:**
```
VFS: Mounted root (ext4 filesystem) readonly on device 254:0
Freeing unused kernel memory: 1272K
This architecture does not have kernel memory protection.
```

**How to Test:**
```bash
./run_qemu.sh -no-pause 2>&1 | grep -E "(VFS|mount|root device)"

# Verify rootfs image is valid
file rootfs-image
ls -lh rootfs-image  # Should be ~268M

# Test with initrd mode (alternative)
./run_qemu.sh -initrd -no-pause 2>&1 | grep -i initrd
```

**Log File Locations:**
- `/tmp/qemu.log` - Mount messages appear here
- QEMU monitor: `telnet 127.0.0.1 4444`, then `info block` to check drives

**What's Needed:**
- Working kernel up through Stage 3
- Valid rootfs-image file created by `create_rootfs_image.sh`
- Correct drive configuration in run_qemu.sh:184
- Kernel command line includes correct root= parameter

**Troubleshooting:**
- "cannot open root device": Check `-drive` parameter in QEMU command
- "bad superblock": Recreate rootfs-image with `./create_rootfs_image.sh`
- Wrong filesystem type: Verify squashfs-root was properly converted
- Mount read-only: Expected behavior, kernel remounts RW later

**Current Status:** ❌ BLOCKED - Dependent on Stage 3

---

#### **STAGE 5: Init System Startup**
**Goal:** First userspace process (init) starts and runs

**Success Criteria:**
- Kernel successfully exec's /sbin/init
- Init system messages appear: "INIT: version X.XX booting"
- System services begin starting
- No kernel panic after "Freeing unused kernel memory"

**Log Patterns to Look For:**
```
Freeing unused kernel memory: 1272K
This architecture does not have kernel memory protection.
INIT: version 2.88 booting
Starting system...
```

**How to Test:**
```bash
./run_qemu.sh -no-pause 2>&1 | grep -E "(init|INIT|Starting)"

# Check if init process exists
# (requires working QEMU monitor or GDB)
telnet 127.0.0.1 4444
# In monitor: info processes (if available)
```

**Log File Locations:**
- `/tmp/qemu.log` - Init messages
- Serial console output in terminal

**What's Needed:**
- Working kernel through Stage 4
- Valid /sbin/init in rootfs
- Required shared libraries present in rootfs
- Proper device nodes in /dev

**Troubleshooting:**
- "init not found": Check `ls squashfs-root/sbin/init` exists
- "cannot execute": Check init binary architecture: `file squashfs-root/sbin/init`
- Kernel panic after mount: Init failed to exec, check library dependencies
- Use user-mode QEMU to test init: `qemu-mipsel -L ../squashfs-root ../squashfs-root/sbin/init --help`

**Current Status:** ❌ BLOCKED - Dependent on Stage 4

---

#### **STAGE 6: User Application Execution**
**Goal:** hiby_player and other user applications can start

**Success Criteria:**
- Init completes startup scripts
- hiby_player process starts
- No segmentation faults or library errors
- Application logs appear in /var/log or console

**Log Patterns to Look For:**
```
Starting hiby_player...
hiby_player: version X.X.X
Display initialized
Audio system ready
```

**How to Test:**
```bash
# Full system test
./run_qemu.sh -no-pause 2>&1 | grep -i hiby

# Direct binary test with user-mode QEMU
cd ../squashfs-root/usr/bin
qemu-mipsel -L ../.. ./hiby_player --help
```

**Log File Locations:**
- `/tmp/qemu.log` - Console messages
- `squashfs-root/var/log/hiby_player.log` (if logging implemented)
- Check hiby_player stdout/stderr

**What's Needed:**
- Working system through Stage 5
- All hiby_player dependencies present
- Configuration files in correct locations
- Sufficient /dev devices (framebuffer, input, audio)

**Troubleshooting:**
- Segfault: Use GDB to debug: `qemu-mipsel -g 1234 -L ../.. ./hiby_player`
- Missing libraries: `qemu-mipsel -L ../.. ldd ./hiby_player`
- Can't open device: Check /dev entries exist in rootfs
- User-mode emulation works but system emulation doesn't: Device driver issues

**Current Status:** ❌ BLOCKED - Dependent on Stage 5

---

#### **STAGE 7: Peripheral Emulation**
**Goal:** Display, touch, audio, and other hardware peripherals work

**Success Criteria:**
- Framebuffer device operational
- Touch input events processed
- Audio output functional
- USB interface working (optional)
- Full user interface responsive

**Log Patterns to Look For:**
```
fb0: framebuffer device registered
ingenic-gpio: GPIO driver initialized
ingenic-i2s: audio device ready
USB: EHCI controller initialized
```

**How to Test:**
```bash
# Check for device initialization
./run_qemu.sh -no-pause 2>&1 | grep -E "(fb[0-9]|gpio|i2s|audio|usb)"

# Monitor QEMU peripherals
telnet 127.0.0.1 4444
# In monitor: info qtree (shows device tree)
```

**Log File Locations:**
- `/tmp/qemu.log` - Device driver messages
- Kernel dmesg output

**What's Needed:**
- Implement X1600 GPIO controller
- Framebuffer emulation (720x480)
- I2S audio device emulation
- Touch controller (I2C)
- USB controller (optional)

**Troubleshooting:**
- No display: Implement framebuffer device or use -nographic
- Touch not working: May need I2C touch controller emulation
- Audio issues: I2S and DAC emulation complex, consider stub
- For testing without full emulation: Use VNC or SDL output

**Current Status:** ❌ NOT STARTED - Dependent on Stage 6

---

### Log File Reference

#### Primary Log Files
| File | Purpose | How to Generate |
|------|---------|----------------|
| `/tmp/qemu.log` | Real-time QEMU/kernel output | Auto-generated by run_qemu.sh |
| `r3proii/qemu/logs/log_buffer.txt` | Kernel log buffer dump | GDB: `x /3000s __log_buf+8` |
| `r3proii/qemu/logs/backtrace.txt` | Stack trace at crash | GDB: `backtrace` after panic |

#### How to Extract Kernel Log Buffer
```bash
# Start QEMU paused
./run_qemu.sh

# In another terminal:
cd r3proii
gdb Linux-4.4.94+.elf
(gdb) target remote :1234
(gdb) set logging file qemu/logs/log_buffer.txt
(gdb) set logging on
(gdb) x /3000s __log_buf+8
(gdb) quit

# Logs saved to qemu/logs/log_buffer.txt
```

#### Debug Log Levels

Use different debug flags to get more detailed information:

| Flag | Command | Use Case |
|------|---------|----------|
| Minimal | `./run_qemu.sh -dminimal` | Normal operation (default) |
| Light | `./run_qemu.sh -dlight` | Interrupt debugging |
| Medium | `./run_qemu.sh -dmedium` | Instruction execution tracking |
| Full | `./run_qemu.sh -dfull` | Deep debugging (very verbose) |
| Custom | `./run_qemu.sh -dflags "in_asm,int,cpu"` | Specific subsystems |

**Warning:** Full debug mode generates MASSIVE log files (100+ MB). Use sparingly.

#### QEMU Debug Flags Reference
- `unimp` - Unimplemented device accesses (look for missing hardware)
- `guest_errors` - Guest OS errors (memory access violations)
- `int` - Interrupt handling (timer, peripheral interrupts)
- `exec` - Instruction execution traces (very verbose)
- `in_asm` - Assembly instruction disassembly (extremely verbose)
- `cpu` - CPU state changes (register dumps)

### Diagnostic Commands

#### Check Current Progress
```bash
cd r3proii/qemu
./run_qemu.sh -no-pause 2>&1 | tee current_progress.log
grep -E "(x1600 clocks|Unable to handle|VFS: Mounted|INIT: version)" current_progress.log
```

#### Quick Stage Identification
```bash
# Stage 2 check (CPM)
grep "x1600 clocks" /tmp/qemu.log

# Stage 3 check (Timer)
grep "sched_clock" /tmp/qemu.log

# Stage 4 check (Filesystem)
grep "VFS: Mounted" /tmp/qemu.log

# Stage 5 check (Init)
grep "INIT:" /tmp/qemu.log
```

#### Monitor QEMU in Real-Time
```bash
# Terminal 1: Run QEMU
cd r3proii/qemu
./run_qemu.sh -no-pause -dlight

# Terminal 2: Monitor log
tail -f /tmp/qemu.log | grep --color -E "(error|panic|unable|fail|success|mounted)"

# Terminal 3: QEMU Monitor
telnet 127.0.0.1 4444
```

### Next Steps Based on Current Stage

**Current Stage: 2 (CPM Initialization) - BLOCKED**

**Immediate Next Actions:**
1. Create minimal X1600 QEMU machine definition in `qemu/hw/mips/x1600_halley6.c`
2. Implement CPM device emulation at 0x10000000
3. Have CPM registers return non-zero clock values
4. Recompile QEMU: `cd qemu && make && sudo make install`
5. Update run_qemu.sh line 139: `QEMU_BOARD="x1600_halley6"`
6. Test: Look for non-zero clock values in output

**Success Criteria for Moving to Stage 3:**
- No kernel panic at clock initialization
- All clock values non-zero
- Kernel continues to "sched_clock" message

---

### Testing Methodology Summary

**For Each Development Session:**

1. **Identify Current Stage** - Run quick progress check
2. **Set Stage Goal** - Know exactly what log pattern you're trying to achieve
3. **Make Changes** - Implement required hardware/fixes
4. **Test** - Run QEMU and capture logs
5. **Compare Logs** - Check against expected patterns
6. **Debug** - Use appropriate debug flags if stuck
7. **Document** - Update logs/ directory with findings
8. **Repeat** - Move to next stage when success criteria met

**Key Principle:** Each stage builds on the previous. Don't skip stages. If stuck, focus on completing the current stage before moving forward.

---

## Plan: Create X1600/Halley6 QEMU Board Support (ai generated, be warned)
**TL;DR**: The Malta board is fundamentally incompatible with your X1600E chip. All critical clocks show 0Hz, causing the workqueue crash. The kernel needs X1600-specific hardware (Clock Power Management, interrupt controller, timers) that Malta doesn't provide. You'll need to add minimal X1600/Halley6 board emulation to QEMU.

**Steps**
1. Create minimal X1600 QEMU machine definition in QEMU source under hw/mips/x1600_halley6.c with basic memory map, CPM at 0x10000000, INTC at 0x10001000, OST at 0x12000000, GPIO controllers, and stub implementations returning sane register values
2. Implement Clock Power Management (CPM) emulation with simulated PLLs (APLL, MPLL) returning non-zero values, clock dividers for CPU/L2/AHB/APB derived from 24MHz ext_clk, and register reads at CPM base matching kernel expectations
3. Add Operating System Timer (OST) device providing 1500kHz clocksource as kernel expects, timer interrupt generation for workqueue/scheduler initialization, and MMIO register interface at OST base address
4. Implement Ingenic interrupt controller routing timer/peripheral IRQs to MIPS CPU, handling interrupt enable/mask/status registers, and supporting the kernel's plat_irq_dispatch function
5. Generate device tree blob matching kernel's expected ingenic,x1600_halley6_module_base compatible string, with clock definitions, interrupt routing, memory regions, and peripheral nodes the kernel probes
6. Update QEMU launch script in run_qemu.sh to use new -M x1600_halley6 machine type, provide device tree with -dtb option, and adjust memory/peripheral mappings

**Further Considerations**
1. Development scope - Start with absolute minimum (CPM returning non-zero clocks, basic OST/INTC) to get past current crash, then incrementally add devices as boot progresses? Or implement more complete hardware upfront?
2. Alternative approach - Would QEMU user-mode emulation (running userspace binaries only) meet your reverse engineering goals without full system emulation complexity?
3. Existing work - Check if Ingenic has any QEMU patches or if similar Ingenic SoCs (X1000, X1830) have QEMU support you could adapt?
