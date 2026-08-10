#ifndef AUDIO_H
#define AUDIO_H

#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
	AUDIO_CMD_NONE,
	AUDIO_CMD_PLAY,
	AUDIO_CMD_STOP,
	AUDIO_CMD_PAUSE,
	AUDIO_CMD_RESUME,
	AUDIO_CMD_SEEK,
} audio_command_t;

// Actual playback status, maintained by the playback thread. This is the
// source of truth for "is it playing" -- distinct from audio_command_t,
// which is just the transient request channel into the playback thread.
typedef enum {
	AUDIO_STATUS_STOPPED,
	AUDIO_STATUS_PLAYING,
	AUDIO_STATUS_PAUSED,
} audio_status_t;

// Initialize the audio playback thread/subsystem
int audio_init();

// Play a new file (closes any currently playing file first)
int audio_play(const char *filepath);

// Pause playback
void audio_pause();

// Resume playback
void audio_resume();

// Stop playback completely
void audio_stop();

// Get the current playback status (playing/paused/stopped)
audio_status_t audio_get_status(void);

// Returns true exactly once if the current track reached its end on its own
// since the last call, clearing the internal flag. User-initiated stops
// (audio_stop/audio_play) do NOT set it. Lets the controller distinguish a
// natural track completion (auto-advance) from a deliberate stop.
bool audio_take_completion(void);

// Copies the path of the currently loaded file into out (empty string if
// nothing is loaded). out_size is the size of the out buffer.
void audio_get_current_file(char *out, size_t out_size);

// Get playback progress in seconds
void audio_get_progress(double *current_secs, double *total_secs);

// Get the sample rate and channel count of the currently playing stream.
// Both are 0 if nothing has started playing yet.
void audio_get_stream_info(int *sample_rate, int *channels);

// Seek playback to the specified time in seconds
void audio_seek(double seconds);

#endif // AUDIO_H
