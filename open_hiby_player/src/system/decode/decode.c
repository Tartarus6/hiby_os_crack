/*
 * This file is mostly LLM written, be warned.
 */

#include "decode.h"

#include "src/system/utils.h"

#include <stdlib.h>

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

#include "stb_vorbis_decl.h"

struct decoder {
	decode_format_t format;
	int channels;
	int sample_rate;
	uint64_t total_pcm_frames;
	union {
		drmp3 mp3;
		drflac *flac;
		stb_vorbis *vorbis;
	} impl;
};

decode_format_t decode_detect_format(const char *filepath) {
	if (has_extension(filepath, ".mp3"))
		return DECODE_FORMAT_MP3;
	if (has_extension(filepath, ".flac"))
		return DECODE_FORMAT_FLAC;
	if (has_extension(filepath, ".ogg"))
		return DECODE_FORMAT_OGG_VORBIS;
	return DECODE_FORMAT_UNKNOWN;
}

decoder_t *decoder_open(const char *filepath, decode_format_t format) {
	decoder_t *dec = calloc(1, sizeof(decoder_t));
	if (!dec)
		return NULL;
	dec->format = format;

	switch (format) {
	case DECODE_FORMAT_MP3:
		if (!drmp3_init_file(&dec->impl.mp3, filepath, NULL)) {
			free(dec);
			return NULL;
		}
		dec->channels = (int)dec->impl.mp3.channels;
		dec->sample_rate = (int)dec->impl.mp3.sampleRate;
		dec->total_pcm_frames = drmp3_get_pcm_frame_count(&dec->impl.mp3);
		break;

	case DECODE_FORMAT_FLAC:
		dec->impl.flac = drflac_open_file(filepath, NULL);
		if (!dec->impl.flac) {
			free(dec);
			return NULL;
		}
		dec->channels = dec->impl.flac->channels;
		dec->sample_rate = (int)dec->impl.flac->sampleRate;
		dec->total_pcm_frames = dec->impl.flac->totalPCMFrameCount;
		break;

	case DECODE_FORMAT_OGG_VORBIS: {
		int error = 0;
		dec->impl.vorbis = stb_vorbis_open_filename(filepath, &error, NULL);
		if (!dec->impl.vorbis) {
			free(dec);
			return NULL;
		}
		stb_vorbis_info info = stb_vorbis_get_info(dec->impl.vorbis);
		dec->channels = info.channels;
		dec->sample_rate = (int)info.sample_rate;
		dec->total_pcm_frames = stb_vorbis_stream_length_in_samples(dec->impl.vorbis);
		break;
	}

	default:
		free(dec);
		return NULL;
	}

	if (dec->channels <= 0 || dec->sample_rate <= 0) {
		decoder_close(dec);
		return NULL;
	}

	return dec;
}

int decoder_channels(const decoder_t *dec) { return dec->channels; }

int decoder_sample_rate(const decoder_t *dec) { return dec->sample_rate; }

uint64_t decoder_total_pcm_frames(const decoder_t *dec) {
	return dec->total_pcm_frames;
}

uint64_t decoder_read_pcm_frames_s16(decoder_t *dec, uint64_t frame_count, short *pBuffer) {
	switch (dec->format) {
	case DECODE_FORMAT_MP3:
		return drmp3_read_pcm_frames_s16(&dec->impl.mp3, frame_count, pBuffer);

	case DECODE_FORMAT_FLAC:
		return drflac_read_pcm_frames_s16(dec->impl.flac, frame_count, pBuffer);

	case DECODE_FORMAT_OGG_VORBIS: {
		int frames_read = stb_vorbis_get_samples_short_interleaved(
			dec->impl.vorbis,
			dec->channels,
			pBuffer,
			(int)(frame_count * (uint64_t)dec->channels));
		return frames_read > 0 ? (uint64_t)frames_read : 0;
	}

	default:
		return 0;
	}
}

int decoder_seek_to_frame(decoder_t *dec, uint64_t frame_index) {
	if (!dec)
		return 0;

	switch (dec->format) {
	case DECODE_FORMAT_MP3:
		return (int)drmp3_seek_to_pcm_frame(&dec->impl.mp3, frame_index);

	case DECODE_FORMAT_FLAC:
		return (int)drflac_seek_to_pcm_frame(dec->impl.flac, frame_index);

	case DECODE_FORMAT_OGG_VORBIS:
		return stb_vorbis_seek(dec->impl.vorbis, (unsigned int)frame_index);

	default:
		return 0;
	}
}

void decoder_close(decoder_t *dec) {
	if (!dec)
		return;

	switch (dec->format) {
	case DECODE_FORMAT_MP3:
		drmp3_uninit(&dec->impl.mp3);
		break;
	case DECODE_FORMAT_FLAC:
		drflac_close(dec->impl.flac);
		break;
	case DECODE_FORMAT_OGG_VORBIS:
		stb_vorbis_close(dec->impl.vorbis);
		break;
	default:
		break;
	}

	free(dec);
}
