# HiBy R3Pro II Output Modes

HiBy OS firmware uses ALSA for it's output management. The setting that controls where the output is routed to is `'Output Port Switch'`.

From the base `hiby_player` binary, in `FUN_00732a20()`, there is a description for `'Output Port Switch'` values 1, 2, 3, 4, and 5 (henceforth refered to as "modes"). However, in my testing, playing audio while in mode 5 causes the device to reboot. This does kind of make sense, as the part of `FUN_00732a20()` that sets it to mode 5 is inaccessible. It seems that, if all checks fail, it will default to mode 4.

Additionally, playing audio in mode 0 causes the device to reboot. Modes 1 through 4 work without causing a reboot though.


## Table of Output Modes
| Output Port Switch | Description                                               | Notes                                      |
|--------------------|-----------------------------------------------------------|--------------------------------------------|
| 1                  | ot_device_analog_switch_port lineout                      | (3.5mm) Seems to still respect ALSA volume |
| 2                  | ot_device_analog_switch_port headset                      | (3.5mm)                                    |
| 3                  | ot_device_analog_switch_port balance                      | (4.4mm)                                    |
| 4                  | ot_device_analog_digital_switch_port (i2s to spdif)/(i2s) | (USB-C) Outputs raw SPDIF over pins A8/B8  |
| 5                  | ot_device_analog_digital_switch_port none                 | Seems to cause reboot                      |


## Line Out
(TODO: figure out exactly what line out does. since it still seems to respect alsa volume)


## S/PDIF Output
The HiBy R3Pro II can output a raw S/PIDF signal out of its USB-C port.

HiBy's documentation for this is [here](https://guide.hiby.com/en/docs/knowledge/audio_port/coaxial) on their wiki. That wiki page is also archived in this repository [here](docs/third_party/hiby/Coaxial_HiBy_WiKi.pdf)

Note that you will need a very specific cable in order to use this S/PDIF output. Most cables available are meant to convert from standard USB audio to S/PDIF, and would not be able to handle a signal that's already S/PDIF. As mentioned on the wiki entry, HiBy does sell "Type C to RCA Coaxial Cable", which should work properly.


*From the Wiki*
> One end is the Typc-C interface, and the other end is the RCA interface.
> On the existing USB TypeC interface, SUB1/SUB2 (pins A8/B8 below) on the Type-C interface are used to transmit the SPDIF signal. Since SUB1/SBU2 is used, other functions on the Type-C interface are still normal.
> 
> ![usb-c diagram](resources/docs/r3proii/OUTPUT_MODES.md/usb-c_diagram.png)
