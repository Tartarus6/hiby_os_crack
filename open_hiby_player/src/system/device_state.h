#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

#include "src/system/audio.h"
#include "src/system/metadata.h"

// Full snapshot of everything the UI cares about: what's playing, how far
// into it we are, the loaded track's metadata, battery, and volume. Each
// field is sourced from the subsystem that actually owns/locks it --
// this struct itself is just a snapshot, not a store.
typedef struct {
	audio_status_t status;
	char current_file[512];
	double progress_current_secs;
	double progress_total_secs;
	int stream_sample_rate;
	int stream_channels;

	song_metadata_t metadata; // metadata of current_file; has_tags=false if none loaded

	char battery_percent[8];
	long volume;
} device_state_t;

// Fills out with a fresh snapshot of the current device state.
void device_state_get(device_state_t *out);

// Loads and starts playing a new file: reads its metadata (cached for
// subsequent device_state_get() calls) and starts playback.
void device_state_play_file(const char *filepath);

// Toggles play/pause based on the current playback status and returns the
// intended new status immediately, so callers can update UI optimistically
// without waiting for the playback thread to catch up.
audio_status_t device_state_toggle_play_pause(void);

void device_state_seek(double seconds);
void device_state_stop(void);

void device_state_set_volume(long volume);
void device_state_change_volume(long amount);

// Re-reads the battery percentage from sysfs; call before device_state_get()
// to refresh the value it reports.
void device_state_refresh_battery(void);

#endif // DEVICE_STATE_H
