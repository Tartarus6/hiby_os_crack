#include "gui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/gui/browser.h"
#include "src/gui/main_menu.h"
#include "src/gui/player.h"
#include "src/gui/switcher.h"
#include "src/gui/topbar.h"

#include "lvgl/lvgl.h"

typedef struct {
	char text[64];
} popup_event_t;

static lv_obj_t *popup;
static lv_obj_t *popup_label;
static lv_timer_t *popup_timer;

// timer handler for hiding the popup
static void popup_hide_cb(lv_timer_t *timer) {
	if (popup) {
		lv_obj_add_flag(popup, LV_OBJ_FLAG_HIDDEN);
	}

	lv_timer_reset(popup_timer);
	lv_timer_pause(popup_timer);
}

// function to show the popup
void popup_show(const char *text) {
	// exit if popup doesn't exist
	if (!popup) {
		fprintf(stderr, "popup_show() was called, but popup is not initialized. this is unexpected...");
		return;
	}

	lv_label_set_text(popup_label, text);
	lv_obj_clear_flag(popup, LV_OBJ_FLAG_HIDDEN);
	lv_timer_reset(popup_timer);
	lv_timer_resume(popup_timer);
}

static void popup_async_cb(void *user_data) {
	popup_event_t *ev = user_data;

	popup_show(ev->text);

	free(ev);
}

void gui_notify_popup(const char *text) {
	popup_event_t *ev = malloc(sizeof(*ev));
	if (!ev) {
		return;
	}

	strncpy(ev->text, text, sizeof(ev->text) - 1);
	ev->text[sizeof(ev->text) - 1] = '\0';
	lv_async_call(popup_async_cb, ev);
}

void gui_init(gui_config_t *cfg) {
	main_menu_screen = lv_obj_create(NULL);
	player_screen = lv_obj_create(NULL);
	browser_screen = lv_obj_create(NULL);

	// Persistent topbar that stays above every screen.
	topbar_init(cfg);

	// Persistent back button that hides when on the main page
	back_btn_init(cfg);

	// main menu
	main_menu_init(cfg);

	// player
	player_init(cfg);

	// file browser
	browser_init(cfg);

	// TODO: add the back button to the top layer, so it's the same button shared accross all screens. just hide the button on the base page

	// popup
	popup = lv_obj_create(lv_layer_top());
	lv_obj_add_flag(popup, LV_OBJ_FLAG_HIDDEN);
	lv_obj_set_size(popup, 300, 200);
	lv_obj_center(popup);

	popup_label = lv_label_create(popup);
	lv_obj_center(popup_label);

	popup_timer = lv_timer_create(popup_hide_cb, 2000, NULL);
	lv_timer_pause(popup_timer); // dont run hide timer while it's already hidden

	// load into the main screen
	switch_screen(main_menu_screen);
}
