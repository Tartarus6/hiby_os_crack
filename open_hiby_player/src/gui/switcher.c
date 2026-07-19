#include "switcher.h"

#include "lvgl/lvgl.h"

#include "src/font/lv_symbol_def.h"
#include "src/gui/gui.h"
#include "src/gui/main_menu.h"

static lv_obj_t *back_btn;
static lv_obj_t *back_btn_label;

// TODO: add memory of previous screen for back button behaviour. maybe can make some data struct to have each screen hold what it's source screen was
void switch_screen(lv_obj_t *target_screen) {
	// hide the back button if we're going to the main menu screen
	if (back_btn) {
		if (target_screen == main_menu_screen) {
			lv_obj_add_flag(back_btn, LV_OBJ_FLAG_HIDDEN);
		} else {
			lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_HIDDEN);
		}
	}
	// Load the target screen, making it active
	lv_screen_load(target_screen);
}

void back_btn_cb(lv_event_t *e) { switch_screen(main_menu_screen); }

void switch_screen_cb(lv_event_t *e) {
	// Get the screen to load from the event's user data
	lv_obj_t *target_screen = (lv_obj_t *)lv_event_get_user_data(e);

	switch_screen(target_screen);
}

// initialize a floating back button on the top layer
void back_btn_init(gui_config_t *cfg) {
	back_btn = lv_btn_create(lv_layer_top());
	lv_obj_set_size(back_btn, 80, 80);
	lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, cfg->padding, cfg->top_bar_height + cfg->padding);
	lv_obj_set_style_bg_color(back_btn, lv_color_make(60, 160, 220), 0);
	lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

	back_btn_label = lv_label_create(back_btn);
	lv_label_set_text(back_btn_label, LV_SYMBOL_LEFT);
	lv_obj_center(back_btn_label);
	lv_obj_set_style_text_font(back_btn_label, &lv_font_montserrat_28, 0);
}
