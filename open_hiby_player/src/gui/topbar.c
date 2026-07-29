#include "topbar.h"

#include <stdint.h>
#include <stdio.h>

#include "lvgl/lvgl.h"

#include "src/system/device_state.h"

// Static objects belonging to the top bar
static lv_obj_t *top_bar;
static lv_obj_t *bat_label;
static lv_obj_t *vol_label;
static lv_timer_t *battery_timer;

// Battery update callback (runs periodically)
static void timer_update_cb(lv_timer_t *timer) {
	(void)timer;

	device_state_refresh_battery();

	device_state_t state;
	device_state_get(&state);

	lv_label_set_text_fmt(bat_label, "%s%%", state.battery_percent);
	lv_label_set_text_fmt(vol_label, "%li", state.volume);
}

// Public: initialize the top bar
void topbar_init(gui_config_t *cfg) {
	if (top_bar != NULL) {
		return;
	}

	// Create the top bar container
	top_bar = lv_obj_create(lv_layer_top());
	lv_obj_set_size(top_bar, cfg->screen_width, cfg->top_bar_height);
	lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_set_style_bg_color(top_bar, lv_color_make(0, 0, 0), 0);
	lv_obj_set_style_border_width(top_bar, 0, 0);
	lv_obj_set_style_radius(top_bar, 0, 0);
	lv_obj_remove_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

	// FIX FOR FLOATING WINDOWS: Strip away default container padding
	lv_obj_set_style_pad_left(top_bar, 0, 0);
	lv_obj_set_style_pad_right(top_bar, 0, 0);
	lv_obj_set_style_pad_top(top_bar, 0, 0);
	lv_obj_set_style_pad_bottom(top_bar, 0, 0);

	// Container for right-side elements
	lv_obj_t *container_right = lv_obj_create(top_bar);
	lv_obj_set_size(container_right, cfg->screen_width, cfg->top_bar_height);
	lv_obj_align(container_right, LV_ALIGN_RIGHT_MID, -cfg->padding, 0);
	lv_obj_set_style_bg_opa(container_right, 0, 0);
	lv_obj_set_style_border_width(container_right, 0, 0);
	lv_obj_set_style_radius(container_right, 0, 0);
	lv_obj_remove_flag(container_right, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_flex_flow(container_right, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(container_right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

	// Volume label (right side)
	vol_label = lv_label_create(container_right);
	lv_label_set_text(vol_label, "--");
	lv_obj_set_style_text_color(vol_label, lv_color_make(220, 220, 220), 0);
	lv_obj_set_style_text_font(vol_label, &lv_font_montserrat_16, 0);

	// Battery label (right side)
	bat_label = lv_label_create(container_right);
	lv_label_set_text(bat_label, "--");
	lv_obj_set_style_text_color(bat_label, lv_color_make(220, 220, 220), 0);
	lv_obj_set_style_text_font(bat_label, &lv_font_montserrat_16, 0);

	// Create timer (update every 60 seconds)
	battery_timer = lv_timer_create(timer_update_cb, 60000, NULL);
	lv_timer_ready(battery_timer); // run immediately on startup
}
