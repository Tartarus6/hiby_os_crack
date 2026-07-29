#ifndef AUDIO_H
#define AUDIO_H

#include <alsa/asoundlib.h>
#include <stdbool.h>

typedef enum {
	AUDIO_CMD_NONE,
	AUDIO_CMD_PLAY,
	AUDIO_CMD_STOP,
	AUDIO_CMD_PAUSE,
	AUDIO_CMD_RESUME,
	AUDIO_CMD_SEEK,
} audio_command_t;

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

// Get playback progress in seconds
void audio_get_progress(double *current_secs, double *total_secs);

// Get the sample rate and channel count of the currently playing stream.
// Both are 0 if nothing has started playing yet.
void audio_get_stream_info(int *sample_rate, int *channels);

// Seek playback to the specified time in seconds
void audio_seek(double seconds);

#endif // AUDIO_H
