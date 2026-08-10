#include "main_menu.h"

#include <stdint.h>

#include "src/gui/browser.h"
#include "src/gui/gui.h"
#include "src/gui/player.h"
#include "src/gui/switcher.h"

#include "lvgl/lvgl.h"

lv_obj_t *main_menu_screen;

// Public: initialize the top bar
void main_menu_init(gui_config_t *cfg) {
	// Screen Style
	lv_obj_set_style_bg_color(main_menu_screen, lv_color_make(50, 50, 62), 0);

	// Title
	lv_obj_t *menu_title = lv_label_create(main_menu_screen);
	lv_label_set_text(menu_title, "Main Menu");
	lv_obj_set_style_text_font(menu_title, &lv_font_montserrat_28, 0);
	lv_obj_set_style_text_color(menu_title, lv_color_white(), 0);
	lv_obj_align(menu_title, LV_ALIGN_TOP_MID, 0, 80);

	// Button to go to the player
	lv_obj_t *go_to_player_btn = lv_btn_create(main_menu_screen);
	lv_obj_align(go_to_player_btn, LV_ALIGN_CENTER, 0, -40);
	lv_obj_t *go_to_player_btn_label = lv_label_create(go_to_player_btn);
	lv_label_set_text(go_to_player_btn_label, "Go to Player");
	lv_obj_set_style_text_font(go_to_player_btn_label, &lv_font_montserrat_28, 0);
	lv_obj_add_event_cb(go_to_player_btn, switch_screen_cb, LV_EVENT_CLICKED, player_screen);

	// Button to browse SD card files
	lv_obj_t *go_to_browser_btn = lv_btn_create(main_menu_screen);
	lv_obj_align(go_to_browser_btn, LV_ALIGN_CENTER, 0, 40);
	lv_obj_t *browser_btn_label = lv_label_create(go_to_browser_btn);
	lv_label_set_text(browser_btn_label, "Browse Files");
	lv_obj_set_style_text_font(browser_btn_label, &lv_font_montserrat_28, 0);
	lv_obj_add_event_cb(go_to_browser_btn, switch_screen_cb, LV_EVENT_CLICKED, browser_screen);
}
