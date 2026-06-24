#include "lvgl/lvgl.h"
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef HOST_BUILD
  #include "src/drivers/sdl/lv_sdl_window.h"
  #include "src/drivers/sdl/lv_sdl_mouse.h"
  #include "src/drivers/sdl/lv_sdl_keyboard.h"
#else
  #include "src/drivers/display/fb/lv_linux_fbdev.h"
  #include "src/drivers/indev/evdev/lv_evdev.h"
#endif

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 720

/* Custom tick interface for LVGL timing (replaces older thread-based ticks) */
static uint32_t custom_tick_get(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

/* GUI initialization declaration */
extern void gui_init(uint32_t screen_width, uint32_t screen_height);

int main(void) {
    printf("Starting open_hiby_player...\n");

    /* 1. Initialize LVGL core */
    lv_init();

    /* Register the custom tick source */
    lv_tick_set_cb(custom_tick_get);

#ifdef HOST_BUILD
    printf("Initializing Host Build (SDL2 Simulation at %dx%d)...\n", SCREEN_WIDTH, SCREEN_HEIGHT);

    /* Create SDL2 window and display */
    lv_display_t * disp = lv_sdl_window_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!disp) {
        fprintf(stderr, "Error: Failed to create SDL2 window\n");
        return 1;
    }

    /* Register Mouse as Pointer device (maps mouse click -> touch) */
    lv_indev_t * mouse = lv_sdl_mouse_create();
    if (mouse) {
        lv_indev_set_display(mouse, disp);
    }

    /* Register Keyboard as Keypad device */
    lv_indev_t * kbd = lv_sdl_keyboard_create();
    if (kbd) {
        lv_indev_set_display(kbd, disp);
    }
#else
    printf("Initializing Target Build (Linux Framebuffer and EVDEV touch)...\n");

    /* Create Framebuffer display */
    lv_display_t * disp = lv_linux_fbdev_create();
    if (!disp) {
        fprintf(stderr, "Error: Failed to create Linux framebuffer display\n");
        return 1;
    }
    lv_linux_fbdev_set_file(disp, "/dev/fb0");

    /* Create Touch Input device via evdev (Goodix GT9xx controller) */
    lv_indev_t * touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event0");
    if (!touch) {
        fprintf(stderr, "Warning: Failed to open /dev/input/event0. Trying event1...\n");
        touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event1");
    }

    if (touch) {
        lv_indev_set_display(touch, disp);
        printf("Touch screen input driver successfully registered.\n");
    } else {
        fprintf(stderr, "Warning: No touch input device found.\n");
    }
#endif

    /* 2. Initialize the application GUI */
    gui_init(SCREEN_WIDTH, SCREEN_HEIGHT);

    /* 3. Main event loop */
    printf("Entering main event loop...\n");
    while(1) {
        uint32_t time_till_next = lv_timer_handler();
        usleep(time_till_next * 1000); /* Convert milliseconds to microseconds */
    }

    return 0;
}
