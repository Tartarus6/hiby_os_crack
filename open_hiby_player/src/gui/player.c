#include "player.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "src/core/lv_obj_event.h"
#include "src/gui/gui.h"
#include "src/layouts/flex/lv_flex.h"
#include "src/misc/lv_area.h"
#include "src/misc/lv_event.h"
#include "src/misc/lv_timer.h"
#include "src/system/audio.h"

#include "lvgl/lvgl.h"
#include "src/system/utils.h"
#include "src/widgets/slider/lv_slider.h"

lv_obj_t *player_screen;

static lv_obj_t *play_btn;
static lv_obj_t *play_btn_label;
static lv_obj_t *song_title_label;
static lv_obj_t *song_artist_label;
static lv_obj_t *progress_slider;
static lv_obj_t *progress_label;
static lv_timer_t *progress_slider_timer;

static bool is_playing = false;
static char current_filepath[512] = {0};
static double current_total_length = 0; // initialize total length to 0, so that progress display starts as "0:00/0:00"
static char progress_label_text[32];	// string to hold the text for the progress label

// TODO: make playing state handling much more robust. reference audio state directly to make sure desync isn't possible but do retain optimistic UI updates
static void set_playing(bool playing) {
	if (current_filepath[0] == '\0') {
		return; // nothing loaded yet
	}

	is_playing = playing;
	if (is_playing) {
		lv_timer_resume(progress_slider_timer); // dont keep checking playback progress when not playing
		lv_label_set_text(play_btn_label, LV_SYMBOL_PAUSE);
		lv_obj_set_style_bg_color(play_btn, lv_color_make(220, 80, 60), 0);
		audio_resume();
	} else {
		lv_timer_pause(progress_slider_timer); // dont keep checking playback progress when not playing
		lv_label_set_text(play_btn_label, LV_SYMBOL_PLAY);
		lv_obj_set_style_bg_color(play_btn, lv_color_make(60, 160, 220), 0);
		audio_pause();
	}
}

static void set_progress_label(double current_secs, double total_secs) {
	if (total_secs > 0) {
		int value = (current_secs / total_secs) * 1000;
		lv_slider_set_value(progress_slider, value, LV_ANIM_OFF);

		formatDoubleProgress(current_secs, total_secs, progress_label_text, sizeof(progress_label_text));
		lv_label_set_text(progress_label, progress_label_text);
	} else {
		// if total length is 0, then just display 0 seconds out of 0 seconds
		formatDoubleProgress(0, 0, progress_label_text, sizeof(progress_label_text));
		lv_label_set_text(progress_label, progress_label_text);
	}
}

// Event handler for the Play/Pause button
static void play_btn_event_cb(lv_event_t *e) {
	set_playing(!is_playing); // update ui
}

// Event handler for the Prev button
static void prev_btn_event_cb(lv_event_t *e) {
	audio_seek(0);

	lv_timer_ready(progress_slider_timer);
}

// Event handler for the progress slider
static void progress_slider_event_cb(lv_event_t *e) {
	lv_event_code_t code = lv_event_get_code(e);

	if (code == LV_EVENT_PRESSED) {
		lv_timer_pause(progress_slider_timer); // pause updating the progress bar based on playback
	}

	if (code == LV_EVENT_RELEASED) {
		lv_timer_resume(progress_slider_timer); // resume updating the progress bar based on playback

		// TODO: seek playback
		int value = lv_slider_get_value(progress_slider);

		double seconds = current_total_length * value / 1000.0;

		audio_seek(seconds);

		lv_timer_resume(progress_slider_timer);
	}

	if (code == LV_EVENT_VALUE_CHANGED) {
		int value = lv_slider_get_value(progress_slider);

		double seconds = current_total_length * value / 1000.0;

		formatDoubleProgress(seconds, current_total_length, progress_label_text, sizeof(progress_label_text));

		lv_label_set_text(progress_label, progress_label_text);
	}
}

// timer to update the progress slider text based on playback progress
static void progress_slider_timer_cb(lv_timer_t *timer) {
	double cur, total;
	audio_get_progress(&cur, &total);

	// update cached total length
	if (total > 0) {
		int value = (cur / total) * 1000;
		lv_slider_set_value(progress_slider, value, LV_ANIM_OFF);

		current_total_length = total;
	}

	set_progress_label(cur, current_total_length);
}

// Public: load and start playing a new file, updating the now-playing info
void player_play_file(const char *filepath) {
	strncpy(current_filepath, filepath, sizeof(current_filepath) - 1);
	current_filepath[sizeof(current_filepath) - 1] = '\0';

	const char *slash = strrchr(filepath, '/');
	lv_label_set_text(song_title_label, slash ? slash + 1 : filepath);
	lv_label_set_text(song_artist_label, "");

	set_playing(true);			  // update the UI
	audio_play(current_filepath); // start playback of the filew
}

// Public: initialize the top bar
void player_init(gui_config_t *cfg) {
	// Screen Style
	lv_obj_set_style_bg_color(player_screen, lv_color_make(50, 50, 62), 0);

	// Screen Title
	lv_obj_t *title = lv_label_create(player_screen);
	lv_label_set_text(title, "Open HiBy Player");
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 80);
	lv_obj_set_style_text_color(title, lv_color_make(255, 255, 255), 0);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);

	// Container for player menu
	lv_obj_t *player_menu = lv_obj_create(player_screen);
	lv_obj_set_size(player_menu, cfg->screen_width, LV_SIZE_CONTENT);
	lv_obj_align(player_menu, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_obj_set_style_bg_color(player_menu, lv_color_make(0, 0, 0), 0);
	lv_obj_set_flex_flow(player_menu, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(player_menu, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_border_width(player_menu, 0, 0);
	lv_obj_set_style_radius(player_menu, 0, 0);
	lv_obj_set_style_pad_gap(player_menu, 40, 0);

	// Song Info
	lv_obj_t *song_info = lv_obj_create(player_menu);
	lv_obj_set_size(song_info, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(song_info, 0, 0);
	lv_obj_set_style_border_width(song_info, 0, 0);
	lv_obj_set_style_radius(song_info, 0, 0);
	lv_obj_set_style_pad_all(song_info, 0, 0);
	lv_obj_set_flex_flow(song_info, LV_FLEX_FLOW_COLUMN);
	lv_obj_remove_flag(song_info, LV_OBJ_FLAG_SCROLLABLE);

	song_title_label = lv_label_create(song_info);
	lv_label_set_text(song_title_label, "No track loaded");
	lv_obj_set_style_text_color(song_title_label, lv_color_make(255, 255, 255), 0);
	lv_obj_set_style_text_font(song_title_label, &lv_font_montserrat_16, 0);

	song_artist_label = lv_label_create(song_info);
	lv_label_set_text(song_artist_label, "");
	lv_obj_set_style_text_color(song_artist_label, lv_color_make(130, 130, 130), 0);
	lv_obj_set_style_text_font(song_artist_label, &lv_font_montserrat_16, 0);

	// Playback Progress Slider
	// TODO: make slider knob bigger
	progress_slider = lv_slider_create(player_menu);
	lv_obj_set_width(progress_slider, lv_pct(100));
	lv_slider_set_range(progress_slider, 0, 1000);
	lv_obj_set_style_bg_color(progress_slider, lv_color_make(255, 255, 255), LV_PART_MAIN);
	lv_obj_set_style_bg_color(progress_slider, lv_color_make(60, 160, 220), LV_PART_INDICATOR);
	lv_obj_set_style_bg_color(progress_slider, lv_color_make(255, 255, 255), LV_PART_KNOB);

	lv_obj_add_event_cb(progress_slider, progress_slider_event_cb, LV_EVENT_ALL, NULL);

	progress_slider_timer = lv_timer_create(progress_slider_timer_cb, 500, NULL); // timer to update the progress slider as the song progresses
	lv_timer_pause(progress_slider_timer);										  // dont keep checking playback progress when not playing

	progress_label = lv_label_create(player_menu);
	lv_label_set_text(progress_label, "..."); // TODO: make this more robust. automatically load in a placeholder using the same mechanism that sets it during playback
	lv_obj_set_style_text_color(progress_label, lv_color_make(255, 255, 255), 0);
	lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_16, 0);

	// Controls buttons: Back, Play/Pause, Next
	// Player Controls Buttons Container
	lv_obj_t *player_controls_buttons = lv_obj_create(player_menu);
	lv_obj_set_size(player_controls_buttons, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(player_controls_buttons, 0, 0);
	lv_obj_set_style_border_width(player_controls_buttons, 0, 0);
	lv_obj_set_style_radius(player_controls_buttons, 0, 0);
	lv_obj_set_style_pad_all(player_controls_buttons, 0, 0);
	lv_obj_set_flex_flow(player_controls_buttons, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(player_controls_buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

	// Prev Song Button
	lv_obj_t *prev_btn = lv_btn_create(player_controls_buttons);
	lv_obj_set_size(prev_btn, 100, 100);
	lv_obj_set_style_bg_color(prev_btn, lv_color_make(45, 45, 52), 0);
	lv_obj_t *prev_label = lv_label_create(prev_btn);
	lv_label_set_text(prev_label, LV_SYMBOL_PREV);
	lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_28, 0);
	lv_obj_center(prev_label);

	lv_obj_add_event_cb(prev_btn, prev_btn_event_cb, LV_EVENT_CLICKED, NULL);

	// Play/Pause Button
	play_btn = lv_btn_create(player_controls_buttons);
	lv_obj_set_size(play_btn, 150, 100);
	lv_obj_set_style_bg_color(play_btn, lv_color_make(60, 160, 220), 0);
	lv_obj_add_event_cb(play_btn, play_btn_event_cb, LV_EVENT_CLICKED, NULL);

	play_btn_label = lv_label_create(play_btn);
	lv_label_set_text(play_btn_label, "..."); // TODO: fix placeholder. do something to indicate no song is picked yet
	lv_obj_center(play_btn_label);
	lv_obj_set_style_text_font(play_btn_label, &lv_font_montserrat_28, 0);

	// manually update the playing status
	set_playing(is_playing);

	// Next Song Button
	lv_obj_t *next_btn = lv_btn_create(player_controls_buttons);
	lv_obj_set_size(next_btn, 100, 100);
	lv_obj_set_style_bg_color(next_btn, lv_color_make(45, 45, 52), 0);
	lv_obj_t *next_label = lv_label_create(next_btn);
	lv_label_set_text(next_label, LV_SYMBOL_NEXT);
	lv_obj_set_style_text_font(next_label, &lv_font_montserrat_28, 0);
	lv_obj_center(next_label);
}
