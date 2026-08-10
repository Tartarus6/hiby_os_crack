#ifndef POWER_H
#define POWER_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl/lvgl.h"

// Power / display management: mirrors the stock firmware's idle behaviour.
//
//   - Short-pressing the power button toggles the screen off/on. Music keeps
//     playing while the screen is off.
//   - After screen_off_timeout_ms with no user input the screen turns off on
//     its own (even while music is playing).
//   - After idle_suspend_timeout_ms with no user input AND no music playing,
//     the whole SoC is put into suspend-to-RAM. A power-button press wakes it
//     and resumes exactly where it left off.
//
// The state machine runs on the LVGL/main thread via an lv_timer. Input
// threads (physical buttons) feed it through the thread-safe power_notify_*
// functions; touch input is tracked automatically via LVGL's inactivity clock.
//
// Every knob here is meant to be driven from a future settings page, so the
// config is copied into the module and exposed through runtime setters/getters.

typedef struct {
	// sysfs / device paths. A NULL path disables the corresponding action,
	// which is how the host (SDL) build stays safe -- e.g. it must never write
	// "mem" to the developer's own /sys/power/state.
	const char *brightness_path;	 // e.g. /sys/class/backlight/backlight_pwm0/brightness
	const char *max_brightness_path; // e.g. /sys/class/backlight/backlight_pwm0/max_brightness
	const char *power_state_path;	 // e.g. /sys/power/state           (suspend)
	const char *blank_path;			 // e.g. /sys/class/graphics/fb0/blank (screen on/off: 0=on, 1=off)

	// Desired on-screen backlight level in raw backlight units. If <= 0 at
	// init, the current sysfs value is adopted as the on-level instead.
	long brightness;

	bool screen_off_enabled;
	uint32_t screen_off_timeout_ms;

	bool idle_suspend_enabled;
	uint32_t idle_suspend_timeout_ms;
} power_config_t;

// Initialize the power manager and start its state-machine timer. Copies cfg
// (does not retain the pointer) and remembers disp for inactivity tracking and
// display blanking. Call once, after lv_init() and gui_init().
void power_init(const power_config_t *cfg, lv_display_t *disp);

// --- Screen control (must be called on the LVGL/main thread) ---
void power_screen_on(void);
void power_screen_off(void);
void power_toggle_screen(void);
bool power_screen_is_on(void);

// --- Activity notifications (thread-safe; call from any thread) ---
// Reset the idle timers because the user did something (e.g. a physical
// button press). Touch input is already accounted for automatically.
void power_notify_activity(void);
// Queue a power-button short-press. Consumed on the next state-machine tick,
// which toggles the screen (or is absorbed as the wake event after suspend).
void power_notify_power_button(void);

// --- Runtime configuration (for the future settings page) ---
void power_set_brightness(long value); // raw backlight units, clamped to [0, max]
long power_get_brightness(void);	   // current on-level
long power_get_max_brightness(void);   // -1 if unknown

void power_set_screen_off_enabled(bool enabled);
void power_set_screen_off_timeout(uint32_t ms);
void power_set_idle_suspend_enabled(bool enabled);
void power_set_idle_suspend_timeout(uint32_t ms);

// Snapshot the live configuration (reflects any runtime changes).
void power_get_config(power_config_t *out);

#endif // POWER_H
