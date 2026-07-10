#include "player.h"

#include <stdint.h>

#include "src/gui/gui.h"
#include "src/gui/main_menu.h"

#include "lvgl/lvgl.h"
#include "src/system/audio.h"

lv_obj_t *player_screen;

static lv_obj_t *play_btn;
static lv_obj_t *play_btn_label;
static bool is_playing = false;

// Event handler for the Play/Pause button
static void play_btn_event_cb(lv_event_t *e) {
    if (e != NULL) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code != LV_EVENT_CLICKED) return;
        is_playing = !is_playing;
    }

    if (is_playing) {
        // topbar_set_play_status("Playing...");    // use topbar function
        lv_label_set_text(play_btn_label, "Pause");
        lv_obj_set_style_bg_color(play_btn, lv_color_make(220, 80, 60), 0);
        audio_play("/media/M1F1-int12-AFsp.wav");
    } else {
        // topbar_set_play_status("Paused...");     // use topbar function
        lv_label_set_text(play_btn_label, "Play");
        lv_obj_set_style_bg_color(play_btn, lv_color_make(60, 160, 220), 0);
        audio_stop();
    }
}

// Event handler for the back button
static void back_btn_event_cb(lv_event_t *e) {
	lv_screen_load(main_menu_screen);
}

// Public: initialize the top bar
void player_init(gui_config_t *cfg) {
	// Screen Style
    lv_obj_set_style_bg_color(player_screen, lv_color_make(50, 50, 62), 0);

    // back button
    lv_obj_t *back_btn = lv_btn_create(player_screen);
    lv_obj_set_size(back_btn, 50, 50);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 20, 100);
    lv_obj_set_style_bg_color(back_btn, lv_color_make(60, 160, 220), 0);
    lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_btn_label = lv_label_create(back_btn);
    lv_label_set_text(back_btn_label, "<-");
    lv_obj_center(back_btn_label);
    lv_obj_set_style_text_font(back_btn_label, &lv_font_montserrat_28, 0);

	// Screen Title
    lv_obj_t *title = lv_label_create(player_screen);
    lv_label_set_text(title, "Open HiBy Player");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_text_color(title, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);

    // Container for player menu
    lv_obj_t * player_menu = lv_obj_create(player_screen);
    lv_obj_set_size(player_menu, cfg->screen_width, LV_SIZE_CONTENT);
    lv_obj_align(player_menu, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(player_menu, lv_color_make(0, 0, 0), 0);
    lv_obj_set_flex_flow(player_menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_width(player_menu, 0, 0);
    lv_obj_set_style_radius(player_menu, 0, 0);
    lv_obj_set_style_pad_gap(player_menu, 40, 0);


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
    // TODO: make slider knob bigger
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
    lv_obj_set_size(prev_btn, 100, 100);
    lv_obj_set_style_bg_color(prev_btn, lv_color_make(45, 45, 52), 0);
    lv_obj_t * prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, "|<<");
    lv_obj_center(prev_label);

    // Play/Pause Button
    play_btn = lv_btn_create(player_controls_buttons);
    lv_obj_set_size(play_btn, 150, 100);
    lv_obj_set_style_bg_color(play_btn, lv_color_make(60, 160, 220), 0);
    lv_obj_add_event_cb(play_btn, play_btn_event_cb, LV_EVENT_CLICKED, NULL);

    play_btn_label = lv_label_create(play_btn);
    lv_label_set_text(play_btn_label, "Play");
    lv_obj_center(play_btn_label);
    lv_obj_set_style_text_font(play_btn_label, &lv_font_montserrat_16, 0);

    // manually update the playing status
    play_btn_event_cb(NULL);

    // Next Song Button
    lv_obj_t * next_btn = lv_btn_create(player_controls_buttons);
    lv_obj_set_size(next_btn, 100, 100);
    lv_obj_set_style_bg_color(next_btn, lv_color_make(45, 45, 52), 0);
    lv_obj_t * next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, ">>|");
    lv_obj_center(next_label);
}
