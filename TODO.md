# Todo List

## This Project
- [ ] probably should move linux kernel bin and elf into qemu folder
- [ ] figure out what "burn" mode does (manual says entered by holding the next song button)
- [ ] store a copy of the r3proii [user manual](https://guide.hiby.com/en/docs/products/audio_player/hiby_r3proii/guide)
- [ ] figure out how to better manage file permissions in rootfs (currently, nearly every file is owned by root and has write protection. this makes it difficult to modify and difficult to upload through git)
- [ ] add a README somewhere that explains the major structure of the root filesystem (like where `hiby_player` is, where useful images are, etc.)
- [x] add vm image files `rootfs-image` and `initrd.cpio` to gitignore
- [x] firmware unpacking script
- [x] firmware repacking script


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
- [x] make the first functional change (tested by changing the number of presses to bring up dev mode dialog from 3 to 4)

## Custom Firmware
- [ ] keep developer mode page visible when developer mode is off (there is a dev mode toggle in the dev mode page)
- [ ] allow for much lower brightnesses (could use backlight to a point, then use overlay. point in slider where overlay gets used should be marked, like how vol over 100% is done in some programs)
- [ ] add audiobooks button to books menu
- [ ] create audiobooks page
- [ ] add support for playing audiobooks
- [ ] make device open onto the playback page rather than where it was
- [ ] add setting for opening onto playback page
- [ ] easier playlist access
- [ ] better playlist menu
- [ ] fix some album art not loading
- [ ] make main page style same as the rest of the pages (its styled different for some reason)
- [ ] charge limit (to conserve battery health)
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
