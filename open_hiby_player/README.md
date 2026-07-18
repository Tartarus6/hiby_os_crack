# Open HiBy Player

**TODOS**: look at [TODO.md](open_hiby_player/TODO.md) to see what has to be done

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

Note that the battery display doesnt work. That's intended. No real point in hooking that up for host testing.

---

## 2. Cross-Compiling for the HiBy Device (MIPS Target)

Building a binary that can be placed on the HiBy OS device.

### Build for Target
Build the target-specific binary:
```bash
make target
```
This builds the cross compilers, then generates the binary `open_hiby_player_target`. This generated binary can be transfered to the device, and run from anywhere.

> ![TIP]
> You can use `-j$(nproc)` to speed up builds significantly. Or you can manually set the number of compile threads. `-j4` sets it to 4 threads, for example.
> 
> So in practice, that would be running `make target -j$(nproc)` to make the target build with multithreading.

### Modifying Unpacked Firmware
(TODO)
