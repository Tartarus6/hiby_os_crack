#include "lvgl/lvgl.h"

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "src/system/system.h"
#include "src/gui/gui.h"

#ifdef HOST_BUILD
  #include "src/drivers/sdl/lv_sdl_window.h"
  #include "src/drivers/sdl/lv_sdl_mouse.h"
  #include "src/drivers/sdl/lv_sdl_keyboard.h"
#else
  #include "src/drivers/display/fb/lv_linux_fbdev.h"
  #include "src/drivers/evdev/lv_evdev.h"
#endif

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 720

// TODO: clean up host build. make it so that all of the host stuff is all in one place, like setting up where the "SD" card is, and the audio, and such, so that all the rest of the code can be clean

// custom tick interface for LVGL timing (replaces older thread-based ticks)
static uint32_t custom_tick_get() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

int main() {
    printf("Starting open_hiby_player...\n");

    // initialize LVGL core
    lv_init();

    // register the custom tick source
    lv_tick_set_cb(custom_tick_get);

#ifdef HOST_BUILD
    printf("Initializing Host Build (SDL2 Simulation at %dx%d)...\n", SCREEN_WIDTH, SCREEN_HEIGHT);

    // create SDL2 window and display
    lv_display_t * disp = lv_sdl_window_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!disp) {
        fprintf(stderr, "Error: Failed to create SDL2 window\n");
        return 1;
    }

    // register mouse as pointer device (maps mouse click -> touch)
    lv_indev_t * mouse = lv_sdl_mouse_create();
    if (mouse) {
        lv_indev_set_display(mouse, disp);
    }

    // Register Keyboard as Keypad device
    lv_indev_t * kbd = lv_sdl_keyboard_create();
    if (kbd) {
        lv_indev_set_display(kbd, disp);
    }
#else
    printf("Initializing Target Build (Linux Framebuffer and EVDEV touch)...\n");

    // Create Framebuffer display
    lv_display_t * disp = lv_linux_fbdev_create();
    if (!disp) {
        fprintf(stderr, "Error: Failed to create Linux framebuffer display\n");
        return 1;
    }
    lv_linux_fbdev_set_file(disp, "/dev/fb0");

    // TODO: how can we make it figure out whuch event device is the right one? instead of hardcoding it

    // Create Touch Input device via evdev (Goodix GT9xx controller)
    lv_indev_t * touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event1");
    if (!touch) {
        fprintf(stderr, "Warning: Failed to open /dev/input/event1. Trying event0...\n");
        touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event1");
    }

    if (touch) {
        lv_indev_set_display(touch, disp);
        printf("Touch screen input driver successfully registered.\n");
    } else {
        fprintf(stderr, "Warning: No touch input device found.\n");
    }
#endif

	// start system services
	storage_config_t storage_cfg = {
        .device = "/dev/mmcblk0p1",
        .mount_point = "/media",
    };

	battery_config_t battery_cfg = {
		.battery_capacity_file = "/sys/class/power_supply/battery/capacity",
	};

	system_config_t system_cfg = {
		.battery_cfg = &battery_cfg,
		.storage_cfg = &storage_cfg,
	};

    system_start_services(&system_cfg);

    // initialize the application GUI
    gui_config_t gui_cfg = {
        .screen_width = SCREEN_WIDTH,
        .screen_height = SCREEN_HEIGHT,
        .top_bar_height = 45,
        .top_bar_padding = 15,
    };

    gui_init(&gui_cfg);

    // main event loop
    // TODO: how does this event loop work? is this effieient? is this standard?
    printf("Entering main event loop...\n");
    while(1) {
        uint32_t time_till_next = lv_timer_handler();
        usleep(time_till_next * 1000); // Convert milliseconds to microseconds
    }

    return 0;
}
