# ToDos

## stability

- [ ] reboots when started through adb from rockbox bootloader after killing with `killall bootloader.r3proii`. but it does not reboot when doing the same but killing with `killall -9 bootlaoder.r3proii`. seems like it might reboot a bit after touching somewhere? i think?
- [ ] playback breaks when switching back from output mode 4 (or maybe switching in general). play music with nothing plugged in, pause, plug something in, play, it won't play and will give i/o error.

## good code

- [x] centralized state handling. battery, playing/paused, selected output, volume, etc. this should help prevent possible desyncs and allow stuff such as displaying the battery, current output, and play/paused on the topbar easily.
- [ ] `lv_screen_load()` should only be used within `switcher.c`, so that the switcher can handle going back screens and such. can i make it so that it's not possible to (or at least easy to accidentally) use `lv_screen_load()` outside of `switcher.c`?
- [ ] make UI timer lengths not just be hardcoded in-line. make it more organized and centralized
- [ ] (at least for volume and such) switch to an immediate event-driven update system rather than relying on a timer. keep it very lightweight though

## little bugs

- [ ] fix progress slider jumping back briefly after seeking. this is likely due to the delay in performing the seek, so the old data is still being given from `audio_get_progress()` after the seek request occurred. (could probably just set the current seconds in `audio.c` directly whenever the seek function is called? -> already doing this, it improved it but issue still happens sometimes)
- [ ] side scrolling text in browser bugs out a little bit when scrolling up/down the page. it seems like scrolling on the page resets the text scrolling? or something?
- [x] hitting prev when song finished and none others starting seeks back to 0 but does not start playback (device_state_seek now restarts the loaded track when playback has stopped)
- [ ] (idk how to reproduce it) sometimes the player UI will desync. the play/pause may be incorrect and the progress indicator may be incorrect and/or not updating

## player features

- [x] seeking through file
- [x] prev button to seek to start of current playback file
- [x] load song title and artist to display in player (also loads album, genre, track, year, and computed bitrate/format info)
- [x] folder playback: auto-advance to the next song in the folder when one finishes (src/system/playlist.c holds the folder queue; player.c polls device_state_take_completion() and calls device_state_advance_auto())
- [x] playback modes: loop folder / loop current song / stop at end of folder (playback_mode_t, cycled via the repeat button on the player screen)
- [x] play button always plays the currently-shown song, even after it ended (device_state_toggle_play_pause restarts the loaded track from STOPPED instead of a no-op resume)
- [x] next/prev handling to go to next/prev songs (just in folders for now) (on-screen buttons: prev goes to previous track within the first 3s, else restarts; next skips to the next track)
- [ ] previous page storage. make some data structure so that every page stores what its previous page was, so that the path back can be easily taken. set the previous page dynamically as you switch pages, so that nothing is hard-coded
- [ ] physical play/pause button handling
- [ ] physical next/prev handling

## display features

- [x] backlight handling. turn on the backlight when starting up (src/system/power.c: sets brightness from sysfs on init; runtime brightness control via power_set_brightness())
- [x] sleep mode. screen-off on idle/power-button, and SoC suspend-to-RAM on deep idle (src/system/power.c). NOTE: idle suspend is disabled by default until KEY_POWER is confirmed as a kernel wake source over ADB (`echo mem > /sys/power/state`, then press power).
- [ ] settings page to configure power behaviour (screen-off timeout + enable, idle-suspend timeout + enable, brightness). Runtime setters already exist in src/system/power.h (power_set_*).
- [ ] long-press power button handling (currently short-press toggles the screen; long-press is left to the kernel/init poweroff path)
