# HiBy OS Crack
Cracking the firmware of HiBy's linux devices


This repo is for:
- Tools to unpack/repack hiby os firmwares
- Documentation on the structure of the firmware
- Documentation on the device (datasheets, ISA, etc.)
- Other tools and information helpful for creating custom firmwares

> [!NOTE]
> This project is part of the [hiby-modding organization](https://github.com/hiby-modding). Also see:
> - [hiby-r3proii-custom-firmware](https://github.com/hiby-modding/hiby-r3proii-custom-firmware) by noisetta — complementary firmware modding project that adds Arabic text rendering support and documents the proprietary OTA firmware format.

> [!TIP]
> **How Can I Help❓**: If you want to help with this project, whether you're a developer or not, look at [HOW_CAN_I_HELP.md](/HOW_CAN_I_HELP.md).

> [!TIP]
> If you just want to see instructions on how to install custom firmware onto your device: [here's the guide](/guides/INSTALLING_FIRMWARE))

## Scope
- The goal of this project is to make it possible to modify the HiBy OS firmware to add custom functionality.
- For now, this project also only focuses on the HiBy OS firmware used by the generation including the R1, R3, and R3 Pro II
    - Older devices such as the R3 Pro and the R3 Pro Saber used a different format. see [hiby-firmware-tools](https://github.com/SuperTaiyaki/hiby-firmware-tools) by SuperTaiyaki on GitHub for that older type of firmware

### Note for Windows
For equivalent functionality on Windows, please see [docs/WIN_INSTALL.md](/docs/WIN_INSTALL.md).

## Supported Devices
This repo is applicable to any HiBy OS device that uses the `.upt` firmware type. However, each device has it's own stock firmware and slightly different hardware.

We have collected documentation and created firmware unpacking/repacking scripts for the following devices:
- HiBy R1
- HiBy R3Pro II

## Documentation
- [this README](/README.md)
- [project TODO](/TODO.md)
- [firmware file system structure](/docs/ROOTFS_STRUCTURE.md)
- Guides
    - [installing firmware](/guides/INSTALLING_FIRMWARE.md)
    - [firmware unpacking](/guides/UNPACKING.md)
    - [firmware repacking](/guides/REPACKING.md)
    - [accessing ADB](/guides/ACCESSING_ADB.md)
    - [easter egg](/guides/EASTER_EGG.md)
- R3ProII
    - [specs](/docs/r3proii/SPECS.md)
    - [output modes](/docs/r3proii/OUTPUT_MODES.md)
    - [qemu readme](/r3proii/qemu/README.md)
    - [squashfs-root-example readme](/r3proii/squashfs-root-example/README.md)
- R1
    - [specs](/docs/r1/SPECS.md)
    - [qemu readme](/r1/qemu/README.md)
- Third Party
    - Curated
        - HiBy User Manuals
            - [R3 Pro II](/docs/third_party/hiby/HiBy_R3PROII_User_Manual_HiBy_WiKi.pdf)
            - [R1](/docs/third_party/hiby/HiBy_R1_User_Manual_HiBy_WiKi.pdf)
        - Ingenic x1600e (SOC)
            - [x1600e datasheet](/docs/third_party/soc/X1600_E+Data+Sheet.pdf)
            - [XBurst ISA MXU](/docs/third_party/soc/X1000_M200_XBurst_ISA_MXU_PM.pdf)
            - [XBurst ISA MXU2](/docs/third_party/soc/XBurst1+Instruction+Set+Architecture+MIPS+extension_enhanced+Unit+2.pdf)
            - [XBurst1 Programming Manual](/docs/third_party/soc/XBurst1_CPU_core-programming_manual.pdf)
        - Halley 6 (Ingenic x1600 development board)
            - [hardware manual](/docs/third_party/halley6/Halley6_hardware_develop_V2.1.pdf) ([translated](/docs/third_party/halley6/Halley6_hardware_develop_V2.1.zh-CN_translated_EN.pdf))
            - [baseboard schematic](/docs/third_party/halley6/halley6_baseboard_v2.0.pdf)
            - [coreboard schematic](/docs/third_party/halley6/halley6_coreboard_v2.0.pdf)
            - [software platform rapid development](/docs/third_party/halley6/00-Halley6_Software_Platform_Rapid_Development_Guide-v2.0.pdf)
            - [uboot development](/docs/third_party/halley6/01-Halley6_uboot_Development_Manual_v20.pdf)
            - [kernel development](/docs/third_party/halley6/02_Halley6_Kernel_development_manual_v2.pdf)
            - [platform application development](/docs/third_party/halley6/03-X1600-halley6_Platform_Application_Development_Manual-v2.0.pdf)
            - [linux 4.4 kernel modularity](/docs/third_party/halley6/04-Linux-4.4_Kernel_Modularity_Instruction_Manual_v2.0.pdf)
            - [reserved memory configuration description](/docs/third_party/halley6/05-Halley6_Reserved_Memory_Configuration_Description_v2.0.pdf)
    - Sources
        - [Ingenic Docs Git](https://gitee.com/ingenic-dev/ingenic-linux-docs/tree/ingenic-master)
            - [X16XX-halley6](https://gitee.com/ingenic-dev/ingenic-linux-docs/blob/ingenic-master/zh-cn/X16XX/X16XX-halley6/01_%E5%BF%AB%E9%80%9F%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97.md)
        - Ingenic Public FTP Server
            - URL: `ftp://ftp.ingenic.com.cn`
            - Username: `ingenic_public`
            - Password: `BFdg2f9B12`

## Tools
- [general upt firmware unpacking script](/scripts/unpack.sh)
- [general upt firmware repacking script](/scripts/repack.sh)
- Unpacking and repacking helper scripts (convenient selection of base firmware and automatic file output selection) called `unpack-helper.sh` and `repack-helper.sh` exist in the `unpacking_and_repacking` directory for the relevant device.


## Unpacking and Repacking Workflow
1. Depending on your device, enter either the `r1` directory or the `r3proii` directory
2. if you want to unpack a firmware not in this repo already, add it in the `firmware/custom` directory
3. enter the `unpacking_and_repacking` directory for your device
4. to unpack your firmware use the `unpack-helper.sh` script and choose the firmware you want to unpack
5. to repack your firmware, just run the `repack-helper.sh` script

> [!NOTE]
> - You might need to create the `firmware/custom` directory if it doesn't exist
> - I've made it so that the unpacking and repacking scripts no longer need sudo in order to run
