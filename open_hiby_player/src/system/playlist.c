#include "playlist.h"

#include "src/system/utils.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// The queue is only ever touched from the LVGL/UI thread (browser selection,
// the progress poll timer, and the player control buttons all run there), so
// like the metadata cache in device_state.c it needs no locking.

typedef struct {
	char name[256];
} playlist_entry_t;

static char folder_path[512] = {0};
static playlist_entry_t *entries = NULL;
static size_t entry_count = 0;
static size_t current_index = 0;
static playback_mode_t mode = PLAYBACK_MODE_NORMAL;

static bool is_playable_file(const char *name) {
	static const char *const playable_exts[] = {".wav", ".mp3", ".flac", ".ogg"};

	for (size_t i = 0; i < sizeof(playable_exts) / sizeof(playable_exts[0]); i++) {
		if (has_extension(name, playable_exts[i]))
			return true;
	}
	return false;
}

// Case-insensitive alphabetical, matching the browser's file ordering.
static int entry_cmp(const void *a, const void *b) {
	const playlist_entry_t *ea = a;
	const playlist_entry_t *eb = b;
	return strcasecmp(ea->name, eb->name);
}

void playlist_clear(void) {
	free(entries);
	entries = NULL;
	entry_count = 0;
	current_index = 0;
	folder_path[0] = '\0';
}

void playlist_load_folder(const char *folder, const char *selected_filename) {
	playlist_clear();

	strncpy(folder_path, folder, sizeof(folder_path) - 1);
	folder_path[sizeof(folder_path) - 1] = '\0';

	DIR *dir = opendir(folder);
	if (!dir)
		return;

	size_t capacity = 0;
	struct dirent *de;
	while ((de = readdir(dir)) != NULL) {
		if (!is_playable_file(de->d_name))
			continue;

		if (entry_count == capacity) {
			capacity = capacity ? capacity * 2 : 32;
			playlist_entry_t *grown = realloc(entries, capacity * sizeof(*entries));
			if (!grown)
				break; // keep whatever we have rather than crashing
			entries = grown;
		}

		strncpy(entries[entry_count].name, de->d_name, sizeof(entries[entry_count].name) - 1);
		entries[entry_count].name[sizeof(entries[entry_count].name) - 1] = '\0';
		entry_count++;
	}
	closedir(dir);

	qsort(entries, entry_count, sizeof(*entries), entry_cmp);

	// Point the current index at the selected file, defaulting to the first.
	current_index = 0;
	if (selected_filename) {
		for (size_t i = 0; i < entry_count; i++) {
			if (strcmp(entries[i].name, selected_filename) == 0) {
				current_index = i;
				break;
			}
		}
	}
}

bool playlist_current_path(char *out, size_t out_size) {
	if (entry_count == 0 || !out)
		return false;
	snprintf(out, out_size, "%s/%s", folder_path, entries[current_index].name);
	return true;
}

bool playlist_advance_auto(char *out, size_t out_size) {
	if (entry_count == 0)
		return false;

	switch (mode) {
	case PLAYBACK_MODE_REPEAT_ONE:
		break; // replay the same track
	case PLAYBACK_MODE_REPEAT_ALL:
		current_index = (current_index + 1) % entry_count;
		break;
	case PLAYBACK_MODE_NORMAL:
	default:
		if (current_index + 1 >= entry_count)
			return false; // reached the end of the folder; stop
		current_index++;
		break;
	}

	return playlist_current_path(out, out_size);
}

bool playlist_next(char *out, size_t out_size) {
	if (entry_count == 0)
		return false;
	current_index = (current_index + 1) % entry_count;
	return playlist_current_path(out, out_size);
}

bool playlist_prev(char *out, size_t out_size) {
	if (entry_count == 0)
		return false;
	current_index = (current_index + entry_count - 1) % entry_count;
	return playlist_current_path(out, out_size);
}

void playlist_set_mode(playback_mode_t new_mode) { mode = new_mode; }

playback_mode_t playlist_get_mode(void) { return mode; }

playback_mode_t playlist_cycle_mode(void) {
	switch (mode) {
	case PLAYBACK_MODE_NORMAL:
		mode = PLAYBACK_MODE_REPEAT_ALL;
		break;
	case PLAYBACK_MODE_REPEAT_ALL:
		mode = PLAYBACK_MODE_REPEAT_ONE;
		break;
	case PLAYBACK_MODE_REPEAT_ONE:
	default:
		mode = PLAYBACK_MODE_NORMAL;
		break;
	}
	return mode;
}
