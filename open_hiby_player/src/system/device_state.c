#include "device_state.h"

#include "src/system/alsa-controls.h"
#include "src/system/audio.h"
#include "src/system/metadata.h"
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

void device_state_play_file(const char *filepath) {
	strncpy(current_metadata_file, filepath, sizeof(current_metadata_file) - 1);
	current_metadata_file[sizeof(current_metadata_file) - 1] = '\0';

	metadata_read(current_metadata_file, &current_metadata);

	audio_play(current_metadata_file);
}

audio_status_t device_state_toggle_play_pause(void) {
	if (audio_get_status() == AUDIO_STATUS_PLAYING) {
		audio_pause();
		return AUDIO_STATUS_PAUSED;
	}

	audio_resume();
	return AUDIO_STATUS_PLAYING;
}

void device_state_seek(double seconds) { audio_seek(seconds); }

void device_state_stop(void) { audio_stop(); }

void device_state_set_volume(long volume) { set_volume(volume); }

void device_state_change_volume(long amount) { change_volume(amount); }

void device_state_refresh_battery(void) { sync_battery_from_sysfs(); }
