# ToDos

## stability

- [ ] reboots when started through adb from rockbox bootloader after killing with `killall bootloader.r3proii`. but it does not reboot when doing the same but killing with `killall -9 bootlaoder.r3proii`. seems like it might reboot a bit after touching somewhere? i think?
- [ ] playback breaks when switching back from output mode 4 (or maybe switching in general). play music with nothing plugged in, pause, plug something in, play, it won't play and will give i/o error.

## good code

- [ ] centralized state handling. battery, playing/paused, selected output, volume, etc. this should help prevent possible desyncs and allow stuff such as displaying the battery, current output, and play/paused on the topbar easily.
- [ ] `lv_screen_load()` should only be used within `switcher.c`, so that the switcher can handle going back screens and such. can i make it so that it's not possible to (or at least easy to accidentally) use `lv_screen_load()` outside of `switcher.c`?
- [ ] make UI timer lengths not just be hardcoded in-line. make it more organized and centralized

## little bugs

- [ ] fix progress slider jumping back briefly after seeking. this is likely due to the delay in performing the seek, so the old data is still being given from `audio_get_progress()` after the seek request occurred. (could probably just set the current seconds in `audio.c` directly whenever the seek function is called? -> already doing this, it improved it but issue still happens sometimes)
- [ ] side scrolling text in browser bugs out a little bit when scrolling up/down the page. it seems like scrolling on the page resets the text scrolling? or something?
- [ ] hitting prev when song finished and none others starting seeks back to 0 but does not start playback

## player features

- [x] seeking through file
- [x] prev button to seek to start of current playback file
- [x] load song title and artist to display in player (also loads album, genre, track, year, and computed bitrate/format info)
- [ ] previous page storage. make some data structure so that every page stores what its previous page was, so that the path back can be easily taken. set the previous page dynamically as you switch pages, so that nothing is hard-coded
- [ ] physical play/pause button handling
- [ ] next/prev handling to go to next/prev songs (just in folders for now) (do it like Spotify does. if you're in the first few seconds of the file, then go to prev track. if you're not in the first few seconds, then seek to the start of the file)
- [ ] physical next/prev handling

## display features

- [ ] backlight handling. turn on the backlight when starting up
- [ ] sleep mode. figure out how to enter a sleep mode to maintain maximum battery while having extremely fast wakeup time, just as the original firmware does it
