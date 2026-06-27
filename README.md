# HiBy OS Crack
Cracking the firmware of HiBy's linux devices

This repo is for:
- Tools to unpack/repack hiby os firmwares
- Documentation on the structure of the firmware
- Documentation on the device (datasheets, ISA, etc.)
- Other tools and information helpful for creating custom firmwares

This project is part of the [hiby-modding organization](https://github.com/hiby-modding). Also see:
- [hiby-r3proii-custom-firmware](https://github.com/hiby-modding/hiby-r3proii-custom-firmware) by noisetta — complementary firmware modding project that adds Arabic text rendering support and documents the proprietary OTA firmware format.

If you just want to see instructions on how to install custom firmware onto your device: [here's the guide](guides/INSTALLING_FIRMWARE))

## Scope
- The goal of this project is to make it possible to modify the HiBy OS firmware to add custom functionality.
- For now, this project also only focuses on the HiBy OS firmware used by the generation including the R1, R3, and R3 Pro II
    - Older devices such as the R3 Pro and the R3 Pro Saber used a different format. see [hiby-firmware-tools](https://github.com/SuperTaiyaki/hiby-firmware-tools) by SuperTaiyaki on GitHub for that older type of firmware

### Note for Windows
For equivalent functionality on Windows, please see [docs/WIN_INSTALL.md](docs/WIN_INSTALL.md).

## Supported Devices
This repo is applicable to any HiBy OS device that uses the `.upt` firmware type. However, each device has it's own stock firmware and slightly different hardware.

We have collected documentation and created firmware unpacking/repacking scripts for the following devices:
- HiBy R1
- HiBy R3Pro II

## Documentation
- [this README](README.md)
- [project TODO](TODO.md)
- [firmware file system structure](docs/ROOTFS_STRUCTURE.md)
- Guides
    - [installing firmware](guides/INSTALLING_FIRMWARE.md)
    - [firmware unpacking](guides/UNPACKING.md)
    - [firmware repacking](guides/REPACKING.md)
- R3ProII
    - [specs](r3proii/SPECS.md)
    - [qemu readme](r3proii/qemu/README.md)
    - [squashfs-root-example readme](r3proii/squashfs-root-example/README.md)
- R1
    - [specs](r1/SPECS.md)
    - [qemu readme](r1/qemu/README.md)
- Third Party
    - HiBy User Manuals
        - [R3 Pro II](thirdpartydocs/hiby/HiBy%20R3PROII%20User%20Manual%20_%20HiBy%20WiKi.pdf)
        - [R1](thirdpartydocs/hiby/HiBy%20R1%20User%20Manual%20_%20HiBy%20WiKi.pdf)
    - Ingenic x1600e (SOC)
        - [x1600e datasheet](thirdpartydocs/soc/X1600_E+Data+Sheet.pdf)
        - [XBurst ISA MXU](thirdpartydocs/soc/X1000_M200_XBurst_ISA_MXU_PM.pdf)
        - [XBurst ISA MXU2](thirdpartydocs/soc/XBurst1+Instruction+Set+Architecture+MIPS+extension_enhanced+Unit+2.pdf)
        - [XBurst1 Programming Manual](thirdpartydocs/soc/XBurst1%20CPU%20core%20-%20programming%20manual.pdf)
    - Halley 6 (Ingenic x1600 development board)
        - [hardware manual](thirdpartydocs/halley6/Halley6_hardware_develop_V2.1.pdf) ([translated](thirdpartydocs/Halley6_hardware_develop_V2.1.zh-CN_translated_EN.pdf))
        - [baseboard schematic](thirdpartydocs/halley6/halley6_baseboard_v2.0.pdf)
        - [coreboard schematic](thirdpartydocs/halley6/halley6_coreboard_v2.0.pdf)
        - [software platform rapid development](thirdpartydocs/halley6/00-Halley6_Software_Platform_Rapid_Development_Guide-v2.0.pdf)
        - [uboot development](thirdpartydocs/halley6/01-Halley6_uboot_Development_Manual_v20.pdf)
        - [kernel development](thirdpartydocs/halley6/02_Halley6_Kernel_development_manual_v2.pdf)
        - [platform application development](thirdpartydocs/halley6/03-X1600-halley6_Platform_Application_Development_Manual-v2.0.pdf)
        - [linux 4.4 kernel modularity](thirdpartydocs/halley6/04-Linux-4.4_Kernel_Modularity_Instruction_Manual_v2.0.pdf)
        - [reserved memory configuration description](thirdpartydocs/halley6/05-Halley6_Reserved_Memory_Configuration_Description_v2.0.pdf)
    - [Ingenic Docs Git](https://gitee.com/ingenic-dev/ingenic-linux-docs/tree/ingenic-master)
        - [X16XX-halley6](https://gitee.com/ingenic-dev/ingenic-linux-docs/blob/ingenic-master/zh-cn/X16XX/X16XX-halley6/01_%E5%BF%AB%E9%80%9F%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97.md)


## Workflow
**Example For HiBy R3 Pro II**
1. go to `r3proii/unpacking_and_repacking`
2. run `unpack.sh` (it will ask for sudo permissions for part of the script). this will create a gitignored folder called `squashfs-root`.
3. modify the contents of `squashfs-root` to make whatever custom firmware you want
4. run `repack.sh` (it will ask for sudo permissions for part of the script). this will create a gitignored file called `r3proii.upt`
5. flash that firmware file onto the device (this is explained in-depth in [INSTALLING_FIRMWARE.md](guides/INSTALLING_FIRMWARE.md))

**Workflow Notes**
- `squashfs-root` represents the root filesystem that will be flashed with the firmware.
- most/all of the files in `squashfs-root` will be owned by `root`, so it can be annoying to modify sometimes. This is also why it's gitignored
- `r3proii.upt` is the firmware file


## Notes
- (TODO, make sure the following is correct) The HiBy OS filesystem is read-only, since it's a squashfs image. Only mounted storage, like `sd_0` can be written to.
