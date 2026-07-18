# Open HiBy Player

(TODO) reboots when started through adb from rockbox bootloader after killing with `killall bootloader.r3proii`. but it does not reboot when doing the same but killing with `killall -9 bootlaoder.r3proii`. seems like it might reboot a bit after touching somewhere? i think?
(TODO) playback breaks when switching back from output mode 4 (or maybe switching in general). play music with nothing plugged in, pause, plug something in, play, it won't play and will give i/o error.

**Terminology**:
- "host device", "host": This refers to the device you are developing on, such as your laptop or PC.
- "target device", "target": This refers to the device you are developing for, such as the R1 or R3Pro II.

## 1. Local Development (Arch Linux Host Simulation)

Running the GUI simulated on the host system (i.e. your laptop).

### Requirements
The following packages are required:
- `sdl2`
- `make`
- `gcc`
- `pkg-config`
- `git`

### Running the Simulator
Run the default Makefile build target:
```bash
make
```
This command automatically clones the LVGL repository (if not already done) and compiles the project for your host architecture.

Launch the compiled executable:
```bash
./open_hiby_player_host
```
This will open an SDL2 graphical window. Clicking and stuff works. Scrolling is handled through holding click and dragging.

---

## 2. Cross-Compiling for the HiBy Device (MIPS Target)

Building a binary that can be placed on the HiBy OS device.

### Build for Target
Build the target-specific binary:
```bash
make target
```
This builds the cross compilers, then generates the binary `open_hiby_player_target`. This generated binary can be transfered to the device, and run from anywhere.

### Modifying Unpacked Firmware
(TODO)
