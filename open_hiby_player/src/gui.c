#include "gui.h"
#include "src/core/lv_obj.h"
#include "src/core/lv_obj_pos.h"
#include "src/core/lv_obj_style.h"
#include "src/core/lv_obj_style_gen.h"
#include "src/layouts/flex/lv_flex.h"
#include "src/misc/lv_area.h"
#include <stdint.h>

static lv_obj_t * play_btn;
static lv_obj_t * play_btn_label;
static bool is_playing = false;

// Event handler for the Play/Pause button
static void play_btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        is_playing = !is_playing;
        if(is_playing) {
            lv_label_set_text(play_btn_label, "Pause");
            /* Change button background to a modern warning/orange color */
            lv_obj_set_style_bg_color(play_btn, lv_color_make(220, 80, 60), 0);
        } else {
            lv_label_set_text(play_btn_label, "Play");
            /* Change button background to a sleek theme/blue color */
            lv_obj_set_style_bg_color(play_btn, lv_color_make(60, 160, 220), 0);
        }
    }
}

void gui_init(uint32_t screen_width, uint32_t screen_height) {
	const int8_t TOP_BAR_PADDING = 15;
	const int8_t TOP_BAR_HEIGHT = 45;

    // Base Screen Style: Dark Mode Background
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(50, 50, 62), 0);

    // Top Status Bar
    lv_obj_t * top_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(top_bar, screen_width, TOP_BAR_HEIGHT);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_remove_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Time on the left of status bar
    lv_obj_t * time_label = lv_label_create(top_bar);
    lv_label_set_text(time_label, "69:69");
    lv_obj_align(time_label, LV_ALIGN_LEFT_MID, TOP_BAR_PADDING, 0);
    lv_obj_set_style_text_color(time_label, lv_color_make(220, 220, 220), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_16, 0);

    // Battery on the right of status bar
    lv_obj_t * bat_label = lv_label_create(top_bar);
    lv_label_set_text(bat_label, "100%");
    lv_obj_align(bat_label, LV_ALIGN_RIGHT_MID, -TOP_BAR_PADDING, 0);
    lv_obj_set_style_text_color(bat_label, lv_color_make(220, 220, 220), 0);
    lv_obj_set_style_text_font(bat_label, &lv_font_montserrat_16, 0);

    // Screen Title
    lv_obj_t * title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "Open HiBy Player");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_text_color(title, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);


    // Container for player menu
    lv_obj_t * player_menu = lv_obj_create(lv_scr_act());
    lv_obj_set_size(player_menu, screen_width, LV_SIZE_CONTENT);
    lv_obj_align(player_menu, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(player_menu, lv_color_make(0, 0, 0), 0);
    lv_obj_set_flex_flow(player_menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(player_menu, 0, 0);
    lv_obj_set_style_radius(player_menu, 0, 0);
    lv_obj_set_style_pad_gap(player_menu, 20, 0);


    // Song Info
    lv_obj_t * song_info = lv_obj_create(player_menu);
    lv_obj_set_size(song_info, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(song_info, 0, 0);
    lv_obj_set_style_border_width(song_info, 0, 0);
    lv_obj_set_style_radius(song_info, 0, 0);
    lv_obj_set_style_pad_all(song_info, 0, 0);
    lv_obj_set_flex_flow(song_info, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(song_info, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * song_title = lv_label_create(song_info);
    lv_label_set_text(song_title, "PLACEHOLDER SONG");
    lv_obj_set_style_text_color(song_title, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_font(song_title, &lv_font_montserrat_16, 0);

    lv_obj_t * song_artist = lv_label_create(song_info);
    lv_label_set_text(song_artist, "PLACEHOLDER ARTIST");
    lv_obj_set_style_text_color(song_artist, lv_color_make(130, 130, 130), 0);
    lv_obj_set_style_text_font(song_artist, &lv_font_montserrat_16, 0);

    // Playback Progress Slider
    lv_obj_t * progress_slider = lv_slider_create(player_menu);
    lv_obj_set_width(progress_slider, lv_pct(100));
    lv_slider_set_value(progress_slider, 25, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_slider, lv_color_make(80, 80, 96), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_slider, lv_color_make(60, 160, 220), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(progress_slider, lv_color_make(255, 255, 255), LV_PART_KNOB);

    // Controls buttons: Back, Play/Pause, Next
    // Player Controls Buttons Container
    lv_obj_t * player_controls_buttons = lv_obj_create(player_menu);
    lv_obj_set_size(player_controls_buttons, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(player_controls_buttons, 0, 0);
    lv_obj_set_style_border_width(player_controls_buttons, 0, 0);
    lv_obj_set_style_radius(player_controls_buttons, 0, 0);
    lv_obj_set_style_pad_all(player_controls_buttons, 0, 0);
    lv_obj_set_flex_flow(player_controls_buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(player_controls_buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Prev Song Button
    lv_obj_t * prev_btn = lv_btn_create(player_controls_buttons);
    lv_obj_set_size(prev_btn, 70, 50);
    lv_obj_set_style_bg_color(prev_btn, lv_color_make(45, 45, 52), 0);
    lv_obj_t * prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, "|<<");
    lv_obj_center(prev_label);

    // Play/Pause Button
    play_btn = lv_btn_create(player_controls_buttons);
    lv_obj_set_size(play_btn, 120, 50);
    lv_obj_set_style_bg_color(play_btn, lv_color_make(60, 160, 220), 0);
    lv_obj_add_event_cb(play_btn, play_btn_event_cb, LV_EVENT_ALL, NULL);

    play_btn_label = lv_label_create(play_btn);
    lv_label_set_text(play_btn_label, "Play");
    lv_obj_center(play_btn_label);
    lv_obj_set_style_text_font(play_btn_label, &lv_font_montserrat_16, 0);

    // Next Song Button
    lv_obj_t * next_btn = lv_btn_create(player_controls_buttons);
    lv_obj_set_size(next_btn, 70, 50);
    lv_obj_set_style_bg_color(next_btn, lv_color_make(45, 45, 52), 0);
    lv_obj_t * next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, ">>|");
    lv_obj_center(next_label);
}
