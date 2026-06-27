# Open HiBy Player

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

***(TODO): cross-compilation is not working right now. could be a me problem, not sure***
Building a binary that can be placed on the HiBy OS device.

### Install Target Cross-Compiler
On your host machine, ensure you have the little-endian MIPS cross-compiler:
```bash
# Ubuntu/Debian host package name:
sudo apt install gcc-mipsel-linux-gnu

# Arch Linux:
# Install 'mipsel-linux-gnu-gcc' from the AUR or build from source
```

### Build for Target
Build the target-specific binary:
```bash
make target
```
This generates the binary `open_hiby_player_target`.

### Modifying Unpacked Firmware
(TODO)
