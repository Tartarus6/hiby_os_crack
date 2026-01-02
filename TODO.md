# Todo List


## Project Structure
- [ ] probably should move linux kernel bin and elf into qemu folder
- [x] add vm image files `rootfs-image` and `initrd.cpio` to gitignore


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

## Custom Firmware
- [ ] add audiobooks button to books menu
- [ ] create audiobooks page
- [ ] add support for playing audiobooks
- [ ] easier playlist access
- [ ] better playlist menu
- [ ] allow for much lower brightnesses (could use backlight to a point, then use overlay. point in slider where overlay gets used should be marked, like how vol over 100% is done in some programs)
- [ ] fix some album art not loading
- [ ] make main page style same as the rest of the pages (its styled different for some reason)
- [ ] charge limit (to conserve battery health)
- [ ] combine all setting menus by using settings tabs (i.e. general settings, playback settings, Bluetooth settings, etc.)
- [ ] built-in custom radio creation/management (currently have to put it in the right format in a txt file)
- [ ] fix setting font size bringing you to the all songs menu (no idea why this happens)
- [ ] (if possible) fix Bluetooth connection usually taking multiple attempts
- [ ] fix very inconsistent and unintuitive settings (backlight settings vs. time setting, USB working mode needs descriptions, etc.)
- [ ] shrink file system where possible

## Windows Support
- [ ] Windows devices should be able to install all project dependencies and run qemu

## Plan: Create X1600/Halley6 QEMU Board Support (ai generated, be warned)
**TL;DR**: The Malta board is fundamentally incompatible with your X1600E chip. All critical clocks show 0Hz, causing the workqueue crash. The kernel needs X1600-specific hardware (Clock Power Management, interrupt controller, timers) that Malta doesn't provide. You'll need to add minimal X1600/Halley6 board emulation to QEMU.

Steps
1. Create minimal X1600 QEMU machine definition in QEMU source under hw/mips/x1600_halley6.c with basic memory map, CPM at 0x10000000, INTC at 0x10001000, OST at 0x12000000, GPIO controllers, and stub implementations returning sane register values

2. Implement Clock Power Management (CPM) emulation with simulated PLLs (APLL, MPLL) returning non-zero values, clock dividers for CPU/L2/AHB/APB derived from 24MHz ext_clk, and register reads at CPM base matching kernel expectations

3. Add Operating System Timer (OST) device providing 1500kHz clocksource as kernel expects, timer interrupt generation for workqueue/scheduler initialization, and MMIO register interface at OST base address

4. Implement Ingenic interrupt controller routing timer/peripheral IRQs to MIPS CPU, handling interrupt enable/mask/status registers, and supporting the kernel's plat_irq_dispatch function

5. Generate device tree blob matching kernel's expected ingenic,x1600_halley6_module_base compatible string, with clock definitions, interrupt routing, memory regions, and peripheral nodes the kernel probes

6. Update QEMU launch script in run_qemu.sh to use new -M x1600_halley6 machine type, provide device tree with -dtb option, and adjust memory/peripheral mappings

Further Considerations
1. Development scope - Start with absolute minimum (CPM returning non-zero clocks, basic OST/INTC) to get past current crash, then incrementally add devices as boot progresses? Or implement more complete hardware upfront?
2. Alternative approach - Would QEMU user-mode emulation (running userspace binaries only) meet your reverse engineering goals without full system emulation complexity?
3. Existing work - Check if Ingenic has any QEMU patches or if similar Ingenic SoCs (X1000, X1830) have QEMU support you could adapt?

