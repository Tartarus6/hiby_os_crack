#include "lvgl/lvgl.h"

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

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

static uint32_t custom_tick_get(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

static void post_gui_popup(const char *message, void *user_data) {
    (void)user_data;
    gui_notify_popup(message);
}

#ifdef HOST_BUILD
static lv_display_t *init_host_display(void) {
    lv_display_t *disp = lv_sdl_window_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!disp) {
        fprintf(stderr, "Error: Failed to create SDL2 window\n");
        return NULL;
    }

    lv_indev_t *mouse = lv_sdl_mouse_create();
    if (mouse) {
        lv_indev_set_display(mouse, disp);
    }

    lv_indev_t *kbd = lv_sdl_keyboard_create();
    if (kbd) {
        lv_indev_set_display(kbd, disp);
    }

    return disp;
}
#else
static lv_display_t *init_target_display(void) {
    lv_display_t *disp = lv_linux_fbdev_create();
    if (!disp) {
        fprintf(stderr, "Error: Failed to create Linux framebuffer display\n");
        return NULL;
    }
    lv_linux_fbdev_set_file(disp, "/dev/fb0");

    lv_indev_t *touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event1");
    if (!touch) {
        fprintf(stderr, "Warning: Failed to open /dev/input/event1. Trying event0...\n");
        touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event0");
    }

    if (touch) {
        lv_indev_set_display(touch, disp);
        printf("Touch screen input driver successfully registered.\n");
    } else {
        fprintf(stderr, "Warning: No touch input device found.\n");
    }

    return disp;
}
#endif

int main() {
    printf("Starting open_hiby_player...\n");

    // initialize LVGL core
    lv_init();

    // register the custom tick source
    lv_tick_set_cb(custom_tick_get);

#ifdef HOST_BUILD
    printf("Initializing Host Build (SDL2 Simulation at %dx%d)...\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_t *disp = init_host_display();
    if (!disp) {
        return 1;
    }
#else
    printf("Initializing Target Build (Linux Framebuffer and EVDEV touch)...\n");
    lv_display_t *disp = init_target_display();
    if (!disp) {
        return 1;
    }
#endif
    (void)disp;

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

    system_start_services(&system_cfg, post_gui_popup, NULL);

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
    while (1) {
        uint32_t time_till_next = lv_timer_handler();
        usleep(time_till_next * 1000); // Convert milliseconds to microseconds
    }

    return 0;
}
