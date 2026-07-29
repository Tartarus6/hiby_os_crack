#ifndef METADATA_H
#define METADATA_H

#include <stdbool.h>

typedef struct {
	char title[256];
	char artist[256];
	char album[256];
	char genre[128];
	int track_number; // 0 if unknown
	int year;		  // 0 if unknown
	bool has_tags;	  // true if any tag field was found
} song_metadata_t;

// Reads whatever tag metadata is available for the file (ID3v1/ID3v2 for MP3,
// Vorbis comments for FLAC/OGG, LIST/INFO chunk for WAV). Always fills every
// field of `out` (empty string / 0 if not found). Does not touch playback
// state or decode any audio samples.
void metadata_read(const char *filepath, song_metadata_t *out);

#endif // METADATA_H
