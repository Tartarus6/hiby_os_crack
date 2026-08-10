#include "player.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "src/core/lv_obj.h"
#include "src/core/lv_obj_pos.h"
#include "src/gui/gui.h"
#include "src/misc/lv_area.h"
#include "src/system/decode/decode.h"
#include "src/system/device_state.h"
#include "src/system/playlist.h"
#include "src/system/utils.h"

#include "lvgl/lvgl.h"

lv_obj_t *player_screen;

static lv_obj_t *play_btn;
static lv_obj_t *play_btn_label;
static lv_obj_t *song_title_label;
static lv_obj_t *song_artist_label;
static lv_obj_t *song_album_label;
static lv_obj_t *song_format_label;
static lv_obj_t *progress_slider;
static lv_obj_t *progress_label;
static lv_obj_t *repeat_btn_label;
static lv_timer_t *progress_slider_timer;

static double current_total_length = 0;	  // cached from the last device_state snapshot, so slider math works between polls
static char progress_label_text[32];	  // string to hold the text for the progress label
static bool format_info_computed = false; // whether song_format_label has been filled in for the current track

// Applies a playback status to the play/pause button + progress timer.
// Called both optimistically (right after a button press, before the audio
// thread has caught up) and from update_progress() every poll, which is what
// keeps the UI from going stale if the track stops on its own.
static void apply_playback_status(audio_status_t status) {
	bool playing = (status == AUDIO_STATUS_PLAYING);

	lv_label_set_text(play_btn_label, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
	lv_obj_set_style_bg_color(play_btn, playing ? lv_color_make(220, 80, 60) : lv_color_make(60, 160, 220), 0);

	// dont keep checking playback progress when not playing
	if (playing) {
		lv_timer_resume(progress_slider_timer);
	} else {
		lv_timer_pause(progress_slider_timer);
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

// returns a short format label ("MP3", "FLAC", "OGG", "WAV") for the given file
static const char *format_name_for_file(const char *filepath) {
	switch (decode_detect_format(filepath)) {
	case DECODE_FORMAT_MP3:
		return "MP3";
	case DECODE_FORMAT_FLAC:
		return "FLAC";
	case DECODE_FORMAT_OGG_VORBIS:
		return "OGG";
	default:
		return "WAV";
	}
}

// once the stream's sample rate and the track's duration are both known,
// computes an average bitrate from the file size and fills in the technical
// info label. Only runs once per track.
static void update_format_info(const device_state_t *state) {
	if (format_info_computed || current_total_length <= 0)
		return;

	if (state->stream_sample_rate <= 0)
		return; // stream info not ready yet

	char format_text[64];
	long file_size = get_file_size(state->current_file);
	if (file_size > 0) {
		int bitrate_kbps = (int)((file_size * 8.0) / current_total_length / 1000.0 + 0.5);
		snprintf(format_text, sizeof(format_text), "%s | %.1f kHz | %d kbps", format_name_for_file(state->current_file), state->stream_sample_rate / 1000.0, bitrate_kbps);
	} else {
		snprintf(format_text, sizeof(format_text), "%s | %.1f kHz", format_name_for_file(state->current_file), state->stream_sample_rate / 1000.0);
	}
	lv_label_set_text(song_format_label, format_text);
	format_info_computed = true;
}

// Refreshes the now-playing info (title/artist/album) from the current device
// state and resets the per-track format/length caches. Used both when the user
// picks a file and when playback auto-advances to a new track.
static void refresh_now_playing(void) {
	current_total_length = 0;	  // unknown until the playback thread reports it
	format_info_computed = false; // recompute bitrate/format info for the new track
	lv_label_set_text(song_format_label, "...");

	device_state_t state;
	device_state_get(&state);

	const char *file = state.current_file;
	const char *slash = strrchr(file, '/');
	lv_label_set_text(song_title_label, state.metadata.title[0] ? state.metadata.title : (slash ? slash + 1 : file));
	lv_label_set_text(song_artist_label, state.metadata.artist);
	lv_label_set_text(song_album_label, state.metadata.album);
}

// Updates the repeat-mode button's icon/color to reflect the active mode.
static void update_repeat_button(void) {
	lv_color_t active = lv_color_make(60, 160, 220);
	lv_color_t inactive = lv_color_make(100, 100, 100);

	switch (playlist_get_mode()) {
	case PLAYBACK_MODE_REPEAT_ONE:
		lv_label_set_text(repeat_btn_label, LV_SYMBOL_LOOP " 1");
		lv_obj_set_style_text_color(repeat_btn_label, active, 0);
		break;
	case PLAYBACK_MODE_REPEAT_ALL:
		lv_label_set_text(repeat_btn_label, LV_SYMBOL_LOOP);
		lv_obj_set_style_text_color(repeat_btn_label, active, 0);
		break;
	case PLAYBACK_MODE_NORMAL:
	default:
		lv_label_set_text(repeat_btn_label, LV_SYMBOL_LOOP);
		lv_obj_set_style_text_color(repeat_btn_label, inactive, 0);
		break;
	}
}

// Called when the current track finished on its own: advances the folder queue
// per the active playback mode. If a next track starts, refresh the UI to it;
// otherwise playback simply stays stopped (end of folder in normal mode).
static void handle_track_finished(void) {
	if (device_state_advance_auto(NULL, 0)) {
		refresh_now_playing();
	}
}

// reads the current device state and reconciles the UI against it -- this is
// the single point that keeps the play/pause button and progress bar from
// going stale once a track finishes on its own.
static void update_progress(void) {
	// Auto-advance/loop the folder when the track has played through.
	if (device_state_take_completion()) {
		handle_track_finished();
	}

	device_state_t state;
	device_state_get(&state);

	// update cached total length
	if (state.progress_total_secs > 0) {
		int value = (state.progress_current_secs / state.progress_total_secs) * 1000;
		lv_slider_set_value(progress_slider, value, LV_ANIM_OFF);

		current_total_length = state.progress_total_secs;
	}

	set_progress_label(state.progress_current_secs, current_total_length);
	update_format_info(&state);
	apply_playback_status(state.status);
}

// Event handler for the Play/Pause button
static void play_btn_event_cb(lv_event_t *e) {
	audio_status_t new_status = device_state_toggle_play_pause();
	apply_playback_status(new_status); // optimistic UI update; next poll reconciles against the real status
}

// Event handler for the Prev button. Spotify-style: within the first few
// seconds of the track it goes to the previous track in the folder; otherwise
// it restarts the current track.
static void prev_btn_event_cb(lv_event_t *e) {
	device_state_t state;
	device_state_get(&state);

	// TODO: this 3 seconds into song number probably shouldn't be hard-coded
	if (state.progress_current_secs > 3.0) {
		device_state_seek(0); // far enough in -> restart current track
	} else if (device_state_prev(NULL, 0)) {
		refresh_now_playing(); // near the start -> go to previous track
	} else {
		device_state_seek(0); // no queue -> just restart
	}

	// TODO: progress still jumps back and forth a bit. need a robust fix to prevent getting stale data from audio.c
	// immediately update the progress
	update_progress();
}

// Event handler for the Next button: skip to the next track in the folder.
static void next_btn_event_cb(lv_event_t *e) {
	if (device_state_next(NULL, 0)) {
		refresh_now_playing();
	}
	update_progress();
}

// Event handler for the repeat-mode button: cycle normal -> loop folder ->
// loop track.
static void repeat_btn_event_cb(lv_event_t *e) {
	playlist_cycle_mode();
	update_repeat_button();
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

		device_state_seek(seconds);

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
static void progress_slider_timer_cb(lv_timer_t *timer) { update_progress(); }

// Public: load and start playing a new file, updating the now-playing info.
// This also (re)builds the folder playback queue inside device_state, so
// playback continues to the following tracks when this one finishes.
void player_play_file(const char *filepath) {
	device_state_play_file(filepath); // builds folder queue, loads metadata + starts playback

	refresh_now_playing();

	device_state_t state;
	device_state_get(&state);
	apply_playback_status(state.status); // update the UI
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

	song_album_label = lv_label_create(song_info);
	lv_label_set_text(song_album_label, "");
	lv_obj_set_style_text_color(song_album_label, lv_color_make(130, 130, 130), 0);
	lv_obj_set_style_text_font(song_album_label, &lv_font_montserrat_16, 0);

	song_format_label = lv_label_create(song_info);
	lv_label_set_text(song_format_label, "");
	lv_obj_set_style_text_color(song_format_label, lv_color_make(100, 100, 100), 0);
	lv_obj_set_style_text_font(song_format_label, &lv_font_montserrat_16, 0);

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

	lv_obj_t *below_slider_group = lv_obj_create(player_menu);
	lv_obj_set_size(below_slider_group, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(below_slider_group, 0, 0);
	lv_obj_set_style_border_width(below_slider_group, 0, 0);
	lv_obj_set_style_radius(below_slider_group, 0, 0);
	lv_obj_set_style_pad_all(below_slider_group, 0, 0);
	lv_obj_remove_flag(below_slider_group, LV_OBJ_FLAG_SCROLLABLE);

	progress_label = lv_label_create(below_slider_group);
	lv_label_set_text(progress_label, "..."); // TODO: make this more robust. automatically load in a placeholder using the same mechanism that sets it during playback
	lv_obj_set_style_text_color(progress_label, lv_color_make(255, 255, 255), 0);
	lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_16, 0);
	lv_obj_set_align(progress_label, LV_ALIGN_CENTER);

	// Repeat/loop mode button: cycles normal -> loop folder -> loop track
	lv_obj_t *repeat_btn = lv_btn_create(below_slider_group);
	lv_obj_set_size(repeat_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(repeat_btn, 0, 0);
	lv_obj_set_style_shadow_width(repeat_btn, 0, 0);
	lv_obj_add_event_cb(repeat_btn, repeat_btn_event_cb, LV_EVENT_CLICKED, NULL);
	lv_obj_set_align(repeat_btn, LV_ALIGN_LEFT_MID);

	repeat_btn_label = lv_label_create(repeat_btn);
	lv_obj_set_style_text_font(repeat_btn_label, &lv_font_montserrat_28, 0);
	lv_obj_center(repeat_btn_label);
	update_repeat_button(); // set the initial icon/color for the current mode

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

	// Next Song Button
	lv_obj_t *next_btn = lv_btn_create(player_controls_buttons);
	lv_obj_set_size(next_btn, 100, 100);
	lv_obj_set_style_bg_color(next_btn, lv_color_make(45, 45, 52), 0);
	lv_obj_t *next_label = lv_label_create(next_btn);
	lv_label_set_text(next_label, LV_SYMBOL_NEXT);
	lv_obj_set_style_text_font(next_label, &lv_font_montserrat_28, 0);
	lv_obj_center(next_label);

	lv_obj_add_event_cb(next_btn, next_btn_event_cb, LV_EVENT_CLICKED, NULL);
}
