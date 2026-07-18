#ifndef AUDIO_H
#define AUDIO_H

#include <alsa/asoundlib.h>
#include <stdbool.h>

typedef enum {
    AUDIO_STATE_STOPPED,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED
} audio_state_t;

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

// Get current playback state
audio_state_t audio_get_state();

// Get playback progress in seconds
void audio_get_progress(double *current_secs, double *total_secs);

#endif // AUDIO_H
