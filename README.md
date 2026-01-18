# HiBy OS Crack
Cracking the firmware of HiBy's linux devices

## Scope
- For now, this project focuses on the HiBy R3 Pro II, as it's the only one I have. As far as I know, there are some minor differences between the firmwares on the different HiBy linux devices, but most things apply universally.
- The goal of this project is to make it possible to modify the HiBy OS firmware to add custom functionality.
- For now, this project also only focuses on the HiBy OS firmware used by the generation including the R1, R3, and R3 Pro II
    - Older devices such as the R3 Pro and the R3 Pro Saber used a different format. see [hiby-firmware-tools](https://github.com/SuperTaiyaki/hiby-firmware-tools) by SuperTaiyaki on GitHub for that older type of firmware

### Note for Windows
For equivalent functionality on Windows, please see `docs/WIN_INSTALL.md`.


## Documentation
- [this README](README.md)
- [project TODO](TODO.md)
- [firmware file system structure](ROOTFS_STRUCTURE.md)
- Guides
    - [firmware unpacking](UNPACKING.md)
    - [firmware repacking](REPACKING.md)
- R3ProII
    - [specs](r3proii/SPECS.md)
    - [qemu readme](r3proii/qemu/README.md)
- R1
    - [specs](r1/SPECS.md)
    - [qemu readme](r1/qemu/README.md)
- Third Party
    - HiBy User Manuals
        - [R3 Pro II](thirdpartydocs/HiBy%20R3PROII%20User%20Manual%20_%20HiBy%20WiKi.pdf)
        - [R1](thirdpartydocs/HiBy%20R1%20User%20Manual%20_%20HiBy%20WiKi.pdf)
    - Ingenic x1600e (SOC)
        - [x1600e datasheet](thirdpartydocs/X1600_E+Data+Sheet.pdf)
        - [XBurst ISA MXU](thirdpartydocs/X1000_M200_XBurst_ISA_MXU_PM.pdf)
        - [XBurst ISA MXU2](thirdpartydocs/XBurst1+Instruction+Set+Architecture+MIPS+extension_enhanced+Unit+2.pdf)
        - [XBurst1 Programming Manual](thirdpartydocs/XBurst1%20CPU%20core%20-%20programming%20manual.pdf)
    - Halley 6 (Ingenic x1600 development board)
        - [hardware manual](thirdpartydocs/Halley6_hardware_develop_V2.1.pdf) ([translated](thirdpartydocs/Halley6_hardware_develop_V2.1.zh-CN_translated_EN.pdf))
        - [baseboard schematic](thirdpartydocs/halley6_baseboard_v2.0.pdf)
        - [coreboard schematic](thirdpartydocs/halley6_coreboard_v2.0.pdf)


## Workflow
**For HiBy R3 Pro II**
1. go to `r3proii/unpacking_and_repacking`
2. run `unpack.sh` (it will ask for sudo permissions for part of the script). this will create a gitignored folder called `squashfs-root`.
3. modify the contents of `squashfs-root` to make whatever custom firmware you want
4. run `repack.sh` (it will ask for sudo permissions for part of the script). this will create a gitignored file called `r3proii.upt`
5. flash that firmware file onto the device (how to do that is explained below)

**Workflow Notes**
- `squashfs-root` represents the root filesystem that will be flashed with the firmware.
- most/all of the files in `squashfs-root` will be owned by `root`, so it can be annoying to modify sometimes. This is also why it's gitignored
- `r3proii.upt` is the firmware file


## Notes
- (TODO, make sure the following is correct) The HiBy OS filesystem is read-only, since it's a squashfs image. Only mounted storage, like `sd_0` can be written to.
