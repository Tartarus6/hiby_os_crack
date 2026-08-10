#include "device_state.h"

#include "src/system/alsa-controls.h"
#include "src/system/audio.h"
#include "src/system/metadata.h"
#include "src/system/playlist.h"
#include "src/system/system.h"

#include <string.h>

// Cache of the currently loaded track's metadata, refreshed whenever a new
// file is loaded via device_state_play_file(). Only ever touched from the
// LVGL/UI thread (same as the rest of player.c today), so no lock needed.
static song_metadata_t current_metadata;
static char current_metadata_file[512] = {0};

void device_state_get(device_state_t *out) {
	if (!out)
		return;

	out->status = audio_get_status();
	audio_get_current_file(out->current_file, sizeof(out->current_file));
	audio_get_progress(&out->progress_current_secs, &out->progress_total_secs);
	audio_get_stream_info(&out->stream_sample_rate, &out->stream_channels);

	out->metadata = current_metadata;

	char *battery = read_battery_percent();
	strncpy(out->battery_percent, battery ? battery : "!!", sizeof(out->battery_percent) - 1);
	out->battery_percent[sizeof(out->battery_percent) - 1] = '\0';

	out->volume = get_volume();
}

// Loads metadata for `filepath` and starts playback, without touching the
// folder queue. Shared by fresh selections, queue advances, and replays.
static void load_and_play(const char *filepath) {
	// Callers may replay the already-loaded track by passing current_metadata_file
	// itself, so guard against copying a buffer onto itself.
	if (filepath != current_metadata_file) {
		strncpy(current_metadata_file, filepath, sizeof(current_metadata_file) - 1);
		current_metadata_file[sizeof(current_metadata_file) - 1] = '\0';
	}

	metadata_read(current_metadata_file, &current_metadata);

	audio_play(current_metadata_file);
}

void device_state_play_file(const char *filepath) {
	// Build the folder queue from the selected file's directory so playback
	// can continue past this track. Split the path into folder + filename.
	char folder[512];
	strncpy(folder, filepath, sizeof(folder) - 1);
	folder[sizeof(folder) - 1] = '\0';

	const char *filename = filepath;
	char *slash = strrchr(folder, '/');
	if (slash) {
		*slash = '\0';		   // terminate the folder portion
		filename = slash + 1;  // the bare file name within the folder
	}

	playlist_load_folder(folder, filename);

	load_and_play(filepath);
}

bool device_state_advance_auto(char *out_path, size_t out_size) {
	char path[512];
	if (!playlist_advance_auto(path, sizeof(path)))
		return false;

	load_and_play(path);

	if (out_path && out_size > 0) {
		strncpy(out_path, path, out_size - 1);
		out_path[out_size - 1] = '\0';
	}
	return true;
}

bool device_state_next(char *out_path, size_t out_size) {
	char path[512];
	if (!playlist_next(path, sizeof(path)))
		return false;

	load_and_play(path);

	if (out_path && out_size > 0) {
		strncpy(out_path, path, out_size - 1);
		out_path[out_size - 1] = '\0';
	}
	return true;
}

bool device_state_prev(char *out_path, size_t out_size) {
	char path[512];
	if (!playlist_prev(path, sizeof(path)))
		return false;

	load_and_play(path);

	if (out_path && out_size > 0) {
		strncpy(out_path, path, out_size - 1);
		out_path[out_size - 1] = '\0';
	}
	return true;
}

bool device_state_take_completion(void) { return audio_take_completion(); }

audio_status_t device_state_toggle_play_pause(void) {
	switch (audio_get_status()) {
	case AUDIO_STATUS_PLAYING:
		audio_pause();
		return AUDIO_STATUS_PAUSED;
	case AUDIO_STATUS_PAUSED:
		audio_resume();
		return AUDIO_STATUS_PLAYING;
	case AUDIO_STATUS_STOPPED:
	default:
		// Playback ended (or was stopped): restart the currently-loaded track
		// from the beginning so pressing play always plays the shown song.
		if (current_metadata_file[0]) {
			load_and_play(current_metadata_file);
			return AUDIO_STATUS_PLAYING;
		}
		return AUDIO_STATUS_STOPPED;
	}
}

void device_state_seek(double seconds) {
	// If playback already ended, restart the current track first so seeking
	// (e.g. dragging the slider or hitting prev) resumes playback intuitively
	// instead of silently doing nothing.
	if (audio_get_status() == AUDIO_STATUS_STOPPED && current_metadata_file[0]) {
		load_and_play(current_metadata_file);
	}
	audio_seek(seconds);
}

void device_state_stop(void) { audio_stop(); }

void device_state_set_volume(long volume) { set_volume(volume); }

void device_state_change_volume(long amount) { change_volume(amount); }

void device_state_refresh_battery(void) { sync_battery_from_sysfs(); }
