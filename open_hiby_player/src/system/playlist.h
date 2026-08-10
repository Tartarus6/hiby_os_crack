#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <stdbool.h>
#include <stddef.h>

// How playback proceeds when a track finishes.
typedef enum {
	PLAYBACK_MODE_NORMAL,	  // play through the folder, then stop
	PLAYBACK_MODE_REPEAT_ALL, // loop the whole folder
	PLAYBACK_MODE_REPEAT_ONE, // loop the current track
} playback_mode_t;

// Builds the queue from every playable file in `folder`, ordered the same way
// the browser lists them (case-insensitive alphabetical). `selected_filename`
// is the bare name (not a full path) that becomes the current track; if it
// isn't found, the current index falls back to 0. The playback mode is left
// unchanged.
void playlist_load_folder(const char *folder, const char *selected_filename);

// Empties the queue.
void playlist_clear(void);

// Copies the current track's full path into `out`. Returns false (leaving
// `out` untouched) if the queue is empty.
bool playlist_current_path(char *out, size_t out_size);

// Advances for an automatic (track-finished) transition, honoring the mode:
// REPEAT_ONE keeps the current track, REPEAT_ALL wraps to the start, NORMAL
// stops after the last track. On success copies the next track's path into
// `out` and returns true; returns false when playback should stop (end of a
// NORMAL folder, or empty queue).
bool playlist_advance_auto(char *out, size_t out_size);

// User-initiated next/prev. These always step to the adjacent track (wrapping
// at the ends) regardless of the mode. Copy the new track's path into `out`;
// return false only when the queue is empty.
bool playlist_next(char *out, size_t out_size);
bool playlist_prev(char *out, size_t out_size);

void playlist_set_mode(playback_mode_t mode);
playback_mode_t playlist_get_mode(void);
// Advances to the next mode (NORMAL -> REPEAT_ALL -> REPEAT_ONE -> NORMAL) and
// returns the new mode.
playback_mode_t playlist_cycle_mode(void);

#endif // PLAYLIST_H
