#include "gui.h"

#include <stdint.h>
#include <stdio.h>

#include "src/display/lv_display.h"
#include "src/gui/main_menu.h"
#include "src/gui/player.h"
#include "src/gui/topbar.h"
#include "src/system/audio.h"
#include "src/system/system.h"
#include "src/events.h"

#include "src/core/lv_obj.h"
#include "src/core/lv_obj_pos.h"
#include "src/core/lv_obj_style.h"
#include "src/core/lv_obj_style_gen.h"
#include "src/layouts/flex/lv_flex.h"
#include "src/lv_api_map_v8.h"
#include "src/misc/lv_area.h"
#include "src/misc/lv_event.h"
#include "src/widgets/label/lv_label.h"
#include "src/widgets/slider/lv_slider.h"


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

void popup_async_cb(void *user_data) {
    popup_event_t *ev = user_data;

    popup_show(ev->text);

    free(ev);
}

void gui_init(gui_config_t *cfg) {
    main_menu_screen = lv_obj_create(NULL);
    player_screen = lv_obj_create(NULL);

    lv_screen_load(main_menu_screen);

    // main menu
    main_menu_init(cfg);

    // player
    player_init(cfg);

    // popup
    popup = lv_obj_create(lv_scr_act());
    lv_obj_add_flag(popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(popup, 300, 200);
    lv_obj_center(popup);

    popup_label = lv_label_create(popup);
    lv_obj_center(popup_label);

    popup_timer = lv_timer_create(popup_hide_cb, 2000, NULL);
    lv_timer_pause(popup_timer); // dont run hide timer while it's already hidden

    lv_screen_load(main_menu_screen);
}
