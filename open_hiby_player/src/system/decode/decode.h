#ifndef DECODE_H
#define DECODE_H

#include <stddef.h>
#include <stdint.h>

// Compressed formats supported through the unified decoder below. WAV is
// uncompressed PCM and is handled separately in audio.c.
typedef enum {
	DECODE_FORMAT_UNKNOWN,
	DECODE_FORMAT_MP3,
	DECODE_FORMAT_FLAC,
	DECODE_FORMAT_OGG_VORBIS,
} decode_format_t;

typedef struct decoder decoder_t;

// Determine which decoder (if any) handles this file, based on its extension.
decode_format_t decode_detect_format(const char *filepath);

// Opens a decoder for the given file/format. Returns NULL on failure.
decoder_t *decoder_open(const char *filepath, decode_format_t format);

int decoder_channels(const decoder_t *dec);
int decoder_sample_rate(const decoder_t *dec);

// Total PCM frames in the stream, or 0 if unknown.
uint64_t decoder_total_pcm_frames(const decoder_t *dec);

// Reads up to frame_count PCM frames, interleaved signed 16-bit, into pBuffer.
// Returns the number of frames actually read; 0 means end of stream.
// Buffer type is `short` (not int16_t) to exactly match the underlying
// decoder libraries' signatures across translation units.
uint64_t decoder_read_pcm_frames_s16(decoder_t *dec, uint64_t frame_count, short *pBuffer);

// Seek decoder to the specified frame index. Returns nonzero on success, 0 on failure.
int decoder_seek_to_frame(decoder_t *dec, uint64_t frame_index);

void decoder_close(decoder_t *dec);

#endif // DECODE_H
