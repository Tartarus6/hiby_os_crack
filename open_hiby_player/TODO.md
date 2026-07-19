# ToDos

## stability
- [ ] reboots when started through adb from rockbox bootloader after killing with `killall bootloader.r3proii`. but it does not reboot when doing the same but killing with `killall -9 bootlaoder.r3proii`. seems like it might reboot a bit after touching somewhere? i think?
- [ ] playback breaks when switching back from output mode 4 (or maybe switching in general). play music with nothing plugged in, pause, plug something in, play, it won't play and will give i/o error.


## good code
- [ ] centralized state handling. battery, playing/paused, selected output, volume, etc. this should help prevent possible desyncs and allow stuff such as displaying the battery, current output, and play/paused on the topbar easily.
- [ ] `lv_screen_load()` should only be used within `switcher.c`, so that the switcher can handle going back screens and such. can i make it so that it's not possible to (or at least easy to accidentally) use `lv_screen_load()` outside of `switcher.c`?
- [ ] make UI timer lengths not just be hardcoded in-line. make it more organized and centralized


## little bugs
- [ ] side scrolling text in browser bugs out a little bit when scrolling up/down the page. it seems like scrolling on the page resets the text scrolling? or something?


## player features
- [ ] load song title and artist to display in player
- [ ] previous page storage. make some data structure so that every page stores what its previous page was, so that the path back can be easily taken. set the previous page dynamically as you switch pages, so that nothing is hard-coded
- [ ] physical play/pause button handling
- [ ] next/prev handling (just in folders for now)
- [ ] physical next/prev handling


## display features
- [ ] backlight handling. turn on the backlight when starting up
- [ ] sleep mode. figure out how to enter a sleep mode to maintain maximum battery while having extremely fast wakeup time, just as the original firmware does it
