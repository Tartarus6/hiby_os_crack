#include "lvgl/lvgl.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "src/gui/gui.h"
#include "src/system/power.h"
#include "src/system/system.h"

// TODO: include more fonts. or at least sizes. gap between 16 and 28 is way too big
// TODO: have a global style config (colors, fonts, etc.)

static volatile sig_atomic_t running = 1;

static void sigint_handler(int sig) {
	(void)sig;
	running = 0;
}

#ifdef HOST_BUILD
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include <SDL2/SDL.h>

#define GET_MUSIC_DIR() (snprintf((char[256]){0}, 256, "%s/Music", getenv("HOME") ? getenv("HOME") : ""))
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

	// (DISALLOW RESIZING) This triggers automatic floating on tiling WMs
	lv_sdl_window_set_resizeable(disp, false);

	// (FIX FOR VIEWPORT SHIFT) Fetch SDL context and flush backbuffers
	SDL_Renderer *renderer = (SDL_Renderer *)lv_sdl_window_get_renderer(disp);
	if (renderer) {
		SDL_RenderClear(renderer);	 // Clear backbuffer state
		SDL_RenderPresent(renderer); // Flush context out to Wayland surface
		SDL_Delay(50);				 // Brief pause to allow the compositor coordinates to settle
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
	// register SIGINT handler
	signal(SIGINT, sigint_handler);

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

	// getting the Music directory under home
	char music_path[1024];
	snprintf(music_path, sizeof(music_path), "%s/Music", getenv("HOME") ? getenv("HOME") : "");
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
		.padding = 15,
#ifdef HOST_BUILD
		// .sd_root_path = getenv("HOME"),
		.sd_root_path = music_path,
#else
		.sd_root_path = "/usr/data/mnt/sd_0",
#endif
	};

	gui_init(&gui_cfg);

	// power / display idle management (backlight, screen-off, SoC suspend).
	// On the host build every device path is left NULL so nothing touches the
	// developer's own backlight/framebuffer/suspend.
	power_config_t power_cfg = {
#ifdef HOST_BUILD
		.brightness_path = NULL,
		.max_brightness_path = NULL,
		.power_state_path = NULL,
		.blank_path = NULL,
#else
		.brightness_path = "/sys/class/backlight/backlight_pwm0/brightness",
		.max_brightness_path = "/sys/class/backlight/backlight_pwm0/max_brightness",
		.power_state_path = "/sys/power/state",
		// Screen on/off via the framebuffer blank node -- the same mechanism
		// Rockbox's HiBy port uses. Writing here powers the backlight, panel,
		// and touch controller together, and is reliably reversible.
		.blank_path = "/sys/class/graphics/fb0/blank",
#endif
		.brightness = -1, // adopt the panel's current level as the on-brightness
		.screen_off_enabled = true,
		.screen_off_timeout_ms = 30000, // 30s of no input -> screen off
		// Off by default: SoC suspend only wakes if KEY_POWER is a registered
		// kernel wake source. Confirm that over ADB first (see the notes), then
		// flip this on (or wire it up from the settings page).
		.idle_suspend_enabled = false,
		.idle_suspend_timeout_ms = 120000, // 2min idle + nothing playing -> suspend
	};
	power_init(&power_cfg, disp);

	// main event loop
	// TODO: how does this event loop work? is this effieient? is this standard?
	printf("Entering main event loop...\n");
	while (running) {
		uint32_t time_till_next = lv_timer_handler();
		usleep(time_till_next * 1000); // Convert milliseconds to microseconds
	}

	printf("\n");

	return 0;
}
