# Open HiBy Player Boilerplate

This folder contains the source code structure, configuration, and build configurations to develop a completely open-source userspace UI replacement for the proprietary `hiby_player` binary.

It uses **LVGL v9** for lightweight graphics rendering, which can run directly in a simulated window on your development host using SDL2, or render directly to the target Linux Framebuffer (`/dev/fb0`) with touch panel events (`/dev/input/event0`).

---

## Directory Structure

*   `Makefile`: The central build system file. Automates cloning LVGL v9.1.0 at parse-time if it's not present.
*   `lv_conf.h`: Lightweight configuration file defining memory settings, fonts, and driver switches.
*   `src/main.c`: Hardware interface setup and entry point. Detects host vs target configurations.
*   `src/gui.h` / `src/gui.c`: User interface layout structure and interactivity event logic.

---

## 1. Local Development (Arch Linux Host Simulation)

To write and test your player GUI entirely from your Arch Linux terminal (running inside Zed or a separate command line):

### Install Dependencies
Run the following command to install the required system libraries on Arch Linux:
```bash
sudo pacman -S sdl2 make gcc pkg-config git
```

### Build and Run the Simulator
Run the default Makefile build target:
```bash
make
```
This command automatically clones the LVGL repository (if not already done) and compiles the project for your host architecture.

Launch the compiled executable:
```bash
./open_hiby_player_host
```
This will open an SDL2 graphical window on your Arch Linux screen mirroring the **480x720** display. You can click on the buttons with your mouse to test click events and verify they register successfully!

---

## 2. Cross-Compiling for the HiBy Device (MIPS Target)

*(TODO): cross-compilation is not working right now. could be a me problem, not sure*
Once you are satisfied with your UI changes and want to flash it onto the device:

### Install Target Cross-Compiler
On your development machine, ensure you have the little-endian MIPS cross-compiler:
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
