/*
 * This file is largely LLM-written, be warned
 */

#include "power.h"

#include "src/system/audio.h"

#ifndef HOST_BUILD
// Framebuffer driver, for the one-shot scan-out kick on wake (see power_screen_on).
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// How often the power state machine polls. Kept short so a power-button press
// feels responsive; the work done per tick is trivial.
#define POWER_TICK_MS 250

// After resuming from suspend, ignore power-button presses for this long. The
// press that woke the SoC is delivered as a normal input event once execution
// resumes; without this guard it would immediately toggle the screen back off.
#define RESUME_GUARD_MS 600

// Backlight brightness range for this hardware, matching Rockbox's HiBy port
// (the panel's backlight sysfs takes 1..100; 0 is not a valid "off" value --
// the screen is turned off via the framebuffer blank node instead).
#define BRIGHTNESS_MIN 1
#define BRIGHTNESS_MAX_DEFAULT 100

// Never treat a backlight level below this as a valid "on" level. Guards against
// adopting a leftover dim value -- e.g. a previous run that exited with the
// screen off left the panel at BRIGHTNESS_MIN -- which would make "screen on"
// come back essentially black (backlight technically on, but too dim to see).
#define BRIGHTNESS_ON_FLOOR 20

// After unblanking, wait this long before pushing a new brightness at the panel.
// The fb blank notifier re-inits the panel + re-powers the backlight on unblank;
// a brightness write that lands mid-reinit can be swallowed, leaving the screen
// dark. A short settle makes the wake reliable.
#define SCREEN_ON_SETTLE_US 60000 // 60ms

// Smooth dim: step size and per-step delay when ramping the backlight up/down.
// ~17 steps * 12ms ~= 200ms for a full 1<->100 fade.
#define FADE_STEP 6
#define FADE_STEP_DELAY_US 12000

// --- Module state (owned by the LVGL/main thread unless noted) ---
static power_config_t g_cfg;
static lv_display_t *g_disp;
static lv_timer_t *g_refr_timer; // display refresh timer, paused while screen is off
static long g_max_brightness = -1;
static bool g_screen_on = true;

static long g_hw_brightness = -1;	   // last brightness value actually written to sysfs
static uint32_t g_last_playing_ms;	   // last tick audio was PLAYING (suspend clock)
static uint32_t g_ignore_button_until; // tick until which power presses are absorbed

// Cross-thread fields: written by input threads, read by the state machine.
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_power_button_pending = false;
static uint32_t g_last_button_activity_ms; // last physical-button/explicit activity

// --- Small sysfs helpers ---
static long read_long_from_file(const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) {
		return -1;
	}
	long value = -1;
	if (fscanf(f, "%ld", &value) != 1) {
		value = -1;
	}
	fclose(f);
	return value;
}

static void write_long_to_file(const char *path, long value) {
	FILE *f = fopen(path, "w");
	if (!f) {
		perror("power: open sysfs for write");
		return;
	}
	fprintf(f, "%ld", value); // no trailing newline, matching Rockbox's sysfs writer
	fclose(f);
}

static long brightness_max(void) { return g_max_brightness > 0 ? g_max_brightness : BRIGHTNESS_MAX_DEFAULT; }

// Write a backlight level to sysfs, clamped to [BRIGHTNESS_MIN, max]. No-op on
// the host build (no brightness path), but still tracks the value.
static void backlight_write(long value) {
	long max = brightness_max();
	if (value > max) {
		value = max;
	}
	if (value < BRIGHTNESS_MIN) {
		value = BRIGHTNESS_MIN;
	}
	g_hw_brightness = value;
	if (g_cfg.brightness_path) {
		write_long_to_file(g_cfg.brightness_path, value);
	}
}

// Smoothly ramp the backlight from its current level to target, mirroring the
// stock/Rockbox dim-to-off feel. Blocking, but only for ~200ms during a screen
// transition (the UI isn't visible mid-transition anyway).
static void fade_to(long target) {
	if (g_hw_brightness < 0) {
		backlight_write(target);
		return;
	}
	int dir = (target > g_hw_brightness) ? 1 : -1;
	while (g_hw_brightness != target) {
		long next = g_hw_brightness + dir * FADE_STEP;
		if ((dir > 0 && next > target) || (dir < 0 && next < target)) {
			next = target;
		}
		backlight_write(next);
		if (g_hw_brightness != target) {
			usleep(FADE_STEP_DELAY_US);
		}
	}
}

// Turn the LCD panel on/off via the sysfs framebuffer blank node -- the exact
// mechanism Rockbox's HiBy port uses. Writing here also powers the backlight
// and the touch controller off/on (via the fb blank notifier), so it is the
// real, reversible screen power switch on this hardware. 0 = on, 1 = off.
static void screen_power(bool on) {
	if (!g_cfg.blank_path) {
		return;
	}
	write_long_to_file(g_cfg.blank_path, on ? 0 : 1);
}

// Enable/disable every LVGL input device (i.e. the touchscreen). Physical
// buttons live in their own threads and are unaffected, so volume/power keys
// keep working while the screen is off -- only stray touches are ignored.
static void set_indevs_enabled(bool enable) {
	lv_indev_t *indev = lv_indev_get_next(NULL);
	while (indev) {
		lv_indev_enable(indev, enable);
		indev = lv_indev_get_next(indev);
	}
}

// current idle time in ms considering both touch (LVGL) and physical buttons
static uint32_t input_idle_ms(uint32_t now) {
	uint32_t touch_idle = lv_display_get_inactive_time(g_disp);

	pthread_mutex_lock(&g_lock);
	uint32_t btn_idle = now - g_last_button_activity_ms;
	pthread_mutex_unlock(&g_lock);

	return touch_idle < btn_idle ? touch_idle : btn_idle;
}

void power_screen_off(void) {
	if (!g_screen_on) {
		return;
	}
	g_screen_on = false;

	set_indevs_enabled(false); // belt-and-suspenders; the fb blank also kills touch
	fade_to(BRIGHTNESS_MIN);   // smooth dim down
	screen_power(false);	   // blank the panel -> backlight + touch off

	// stop rendering entirely while the panel is dark (input is a separate
	// timer, so physical buttons are still handled by the state machine)
	if (g_refr_timer) {
		lv_timer_pause(g_refr_timer);
	}

	printf("power: screen off\n");
}

void power_screen_on(void) {
	if (g_screen_on) {
		return;
	}
	g_screen_on = true;

	// Never come back "on" but pitch black: if the stored on-level is missing or
	// suspiciously dim (e.g. adopted from a panel a prior run left at MIN, or a
	// failed sysfs read at init), ramp up to a proper visible level instead.
	if (g_cfg.brightness < BRIGHTNESS_ON_FLOOR) {
		g_cfg.brightness = brightness_max();
	}

	screen_power(true);		   // unblank -> panel/backlight/touch powered back on
	usleep(SCREEN_ON_SETTLE_US); // let the panel + backlight finish re-initializing
	set_indevs_enabled(true);

	// resume ongoing refresh (if it was paused) ...
	if (g_refr_timer) {
		lv_timer_resume(g_refr_timer);
	}
	// ... and force an immediate, full repaint of the framebuffer. The blank
	// cleared the panel to black, so the whole UI must be redrawn -- this is NOT
	// gated on g_refr_timer, and lv_refr_now() draws even if the timer is paused.
	//
	// The sysfs blank/unblank cycle tears down the panel's scan-out; on this
	// hardware the controller won't re-present the framebuffer just because we
	// memcpy fresh pixels into it -- it must be explicitly kicked. Enabling
	// force_refresh makes the fbdev flush issue an FBIOPUT_VSCREENINFO
	// (ACTIVATE_NOW | FORCE) that re-arms scan-out. We do it as a one-shot around
	// this wake repaint, then disable it again to avoid the per-flush ioctl cost
	// during normal rendering.
#ifndef HOST_BUILD
	lv_linux_fbdev_set_force_refresh(g_disp, true);
#endif
	lv_obj_t *scr = lv_screen_active();
	if (scr) {
		lv_obj_invalidate(scr);
	}
	lv_obj_invalidate(lv_layer_top());
	lv_refr_now(g_disp);
#ifndef HOST_BUILD
	lv_linux_fbdev_set_force_refresh(g_disp, false);
#endif

	// The panel came back at its pre-blank (dim) level, and the backlight driver
	// may not have restored our brightness on unblank. Force the hardware to a
	// known-MIN state, then fade up -- this guarantees every ramp step is written
	// to sysfs *after* the unblank+settle, so the backlight reliably comes back.
	g_hw_brightness = BRIGHTNESS_MIN;
	// backlight_write(BRIGHTNESS_MIN);
	fade_to(g_cfg.brightness); // smooth ramp back up over the freshly-drawn UI
	// backlight_write(g_cfg.brightness);
	lv_display_trigger_activity(g_disp);

	printf("power: screen on (brightness=%ld)\n", g_cfg.brightness);
}

void power_toggle_screen(void) {
	if (g_screen_on) {
		power_screen_off();
	} else {
		power_screen_on();
	}
}

bool power_screen_is_on(void) { return g_screen_on; }

// Suspend the whole SoC to RAM and block until the power button wakes it. Runs
// on the main thread; the entire process (including the input threads) is
// frozen for the duration, so nothing else advances until resume.
static void do_suspend(void) {
	printf("power: entering suspend-to-RAM\n");
	power_screen_off();

	if (g_cfg.power_state_path) {
		int fd = open(g_cfg.power_state_path, O_WRONLY);
		if (fd >= 0) {
			ssize_t written = write(fd, "mem\n", 4); // blocks here until the SoC resumes
			(void)written;
			close(fd);
		} else {
			perror("power: open power_state_path");
		}
	} else {
		// host build: never actually suspend the developer's machine
		printf("power: (no power_state_path) skipping real suspend\n");
	}

	// --- resumed (or host no-op) ---
	// The wake press also arrives as a normal input event; drop any pending
	// toggle and guard against it for a moment so we don't bounce back off.
	pthread_mutex_lock(&g_lock);
	g_power_button_pending = false;
	pthread_mutex_unlock(&g_lock);
	g_ignore_button_until = lv_tick_get() + RESUME_GUARD_MS;

	power_screen_on();
	power_notify_activity(); // reset idle clocks so we don't immediately re-suspend
	printf("power: resumed\n");
}

static void power_timer_cb(lv_timer_t *timer) {
	(void)timer;
	uint32_t now = lv_tick_get();

	// 1. consume a queued power-button short-press
	bool btn;
	pthread_mutex_lock(&g_lock);
	btn = g_power_button_pending;
	g_power_button_pending = false;
	pthread_mutex_unlock(&g_lock);

	if (btn) {
		if ((int32_t)(now - g_ignore_button_until) >= 0) {
			power_toggle_screen();
		}
		power_notify_activity(); // a power press always counts as activity
		lv_display_trigger_activity(g_disp);
		now = lv_tick_get();
	}

	// 2. music playing keeps resetting the suspend clock (but not the screen
	//    clock -- the screen still dims while a track plays untouched)
	bool playing = (audio_get_status() == AUDIO_STATUS_PLAYING);
	if (playing) {
		g_last_playing_ms = now;
	}

	// 3. idle computations
	uint32_t input_idle = input_idle_ms(now);
	uint32_t play_idle = now - g_last_playing_ms;
	uint32_t full_idle = input_idle < play_idle ? input_idle : play_idle;

	// 4. auto screen-off after the configured input-idle timeout
	if (g_screen_on && g_cfg.screen_off_enabled && input_idle >= g_cfg.screen_off_timeout_ms) {
		power_screen_off();
	}

	// 5. idle suspend once nothing is playing and everything has been quiet
	if (g_cfg.idle_suspend_enabled && !playing && full_idle >= g_cfg.idle_suspend_timeout_ms) {
		do_suspend();
	}
}

void power_notify_activity(void) {
	pthread_mutex_lock(&g_lock);
	g_last_button_activity_ms = lv_tick_get();
	pthread_mutex_unlock(&g_lock);
}

void power_notify_power_button(void) {
	pthread_mutex_lock(&g_lock);
	g_power_button_pending = true;
	pthread_mutex_unlock(&g_lock);
}

// --- Runtime configuration ---
void power_set_brightness(long value) {
	long max = brightness_max();
	if (value > max) {
		value = max;
	}
	if (value < BRIGHTNESS_MIN) {
		value = BRIGHTNESS_MIN;
	}
	g_cfg.brightness = value;
	if (g_screen_on) {
		backlight_write(value); // instant (no fade) for a live settings adjustment
	}
}

long power_get_brightness(void) { return g_cfg.brightness; }
long power_get_max_brightness(void) { return brightness_max(); }

void power_set_screen_off_enabled(bool enabled) { g_cfg.screen_off_enabled = enabled; }
void power_set_screen_off_timeout(uint32_t ms) { g_cfg.screen_off_timeout_ms = ms; }
void power_set_idle_suspend_enabled(bool enabled) { g_cfg.idle_suspend_enabled = enabled; }
void power_set_idle_suspend_timeout(uint32_t ms) { g_cfg.idle_suspend_timeout_ms = ms; }

void power_get_config(power_config_t *out) {
	if (out) {
		*out = g_cfg;
	}
}

void power_init(const power_config_t *cfg, lv_display_t *disp) {
	g_cfg = *cfg;
	g_disp = disp;
	g_refr_timer = disp ? lv_display_get_refr_timer(disp) : NULL;
	g_screen_on = true;

	// discover the panel's maximum backlight level
	g_max_brightness = g_cfg.max_brightness_path ? read_long_from_file(g_cfg.max_brightness_path) : -1;

	// if no explicit on-brightness was configured, adopt whatever the panel is
	// currently set to (falling back to max). Never adopt a too-dim value: a
	// prior run may have exited with the screen off, leaving the panel at MIN --
	// adopting that would make the screen appear black once turned "on".
	if (g_cfg.brightness < BRIGHTNESS_MIN) {
		long cur = g_cfg.brightness_path ? read_long_from_file(g_cfg.brightness_path) : -1;
		g_cfg.brightness = cur >= BRIGHTNESS_ON_FLOOR ? cur : brightness_max();
	}
	screen_power(true); // make sure the panel is unblanked (in case a prior run left it off)
	backlight_write(g_cfg.brightness);

	uint32_t now = lv_tick_get();
	g_last_button_activity_ms = now;
	g_last_playing_ms = now;
	g_ignore_button_until = now;

	lv_timer_create(power_timer_cb, POWER_TICK_MS, NULL);

	printf("power: initialized (max_brightness=%ld, on=%ld, screen_off=%s/%ums, suspend=%s/%ums)\n", g_max_brightness, g_cfg.brightness, g_cfg.screen_off_enabled ? "on" : "off", g_cfg.screen_off_timeout_ms,
		   g_cfg.idle_suspend_enabled ? "on" : "off", g_cfg.idle_suspend_timeout_ms);
}
