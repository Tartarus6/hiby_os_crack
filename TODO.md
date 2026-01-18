# Todo List

## This Project
- [x] add vm image files `rootfs-image` and `initrd.cpio` to gitignore
- [x] firmware unpacking script
- [x] firmware repacking script
- [x] add a README somewhere that explains the major structure of the root filesystem (like where `hiby_player` is, where useful images are, etc.)
- [x] simplify project README by splitting it up and linking
- [x] get a copy of the r3proii [user manual](https://guide.hiby.com/en/docs/products/audio_player/hiby_r3proii/guide) on this repo
- [ ] probably should move linux kernel bin and elf into qemu folder
- [ ] figure out what "burn" mode does (user manual says entered by holding the next song button)
- [ ] figure out how to better manage file permissions in rootfs (currently, nearly every file is owned by root and has write protection. this makes it difficult to modify and difficult to upload through git)
- [ ] write out step by step how to repack the firmware (just like unpacking instructions have)
- [ ] make a list of supported devices on README
- [ ] add HiBy R1 to supported devices


## Emulator
*Goal: creating a workflow that allows emulating the hiby devices to greatly speed up testing and let people test without hardware*
- [ ] get kernel to run the init kernel function
- [ ] successfully load into file system
- [ ] run with ingenic x1600e features
- [ ] display output
- [ ] fake touch control interface
- [ ] sound output
- [ ] (maybe) usb interface
- [ ] console output
- [ ] kernel boot text output

## hiby_player Decomp
*Goal: get `hiby_player` in a state where new buttons, pages, and features (i.e. audiobook support) can be added*
- [?] de-obfuscate gui rendering
- [?] figure out what each system setting value is (and make an enum type to organize the getter/setter function calls)
- [ ] figure out how to add a new button
- [ ] figure out how to add a new page

## Custom Firmware
- [x] make the first functional change (tested by changing the number of presses to bring up dev mode dialog from 3 to 4)
- [x] get a working adb connection (done by adding `/usr/bin/adbon` call to `/usr/bin/hiby_player.sh`)
- [?] adb debug usb mode option
- [ ] fix usb connection issues (sometimes, plugging it into my laptop will cause the usb screen to show up for a second then dissapear and not connect. sometimes plugging it in won't cause a usb connection other than charging)
- [ ] test HiBy R1 firmware unpacking and repacking (modify an image, or something)
- [ ] keep developer mode page visible when developer mode is off (there is a dev mode toggle in the dev mode page)
- [ ] allow for much lower brightnesses (could use backlight to a point, then use overlay. point in slider where overlay gets used should be marked, like how vol over 100% is done in some programs)
- [ ] charge limit (to conserve battery health)
- [ ] add audiobooks button to books menu
- [ ] create audiobooks page
- [ ] add support for playing audiobooks
- [ ] make device open onto the playback page rather than where it was
- [ ] add setting for opening onto playback page
- [ ] easier playlist access
- [ ] better playlist menu
- [ ] fix some album art not loading
- [ ] make main page style same as the rest of the pages (its styled different for some reason)
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
