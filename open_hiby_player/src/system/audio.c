#include "audio.h"
#include "src/system/alsa-controls.h"
#include "src/system/decode/decode.h"
#include "src/system/utils.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <alloca.h>
#include <alsa/asoundlib.h>

// WAV File format parsing structure
typedef struct {
	int channels;
	int sample_rate;
	int bits_per_sample;
	long data_offset;
	long data_size;
} wav_info_t;

// Playback thread state variables
static pthread_t playback_thread;
static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t audio_cond = PTHREAD_COND_INITIALIZER;

static audio_command_t audio_command = AUDIO_CMD_NONE;
static audio_status_t playback_status = AUDIO_STATUS_STOPPED;
static char current_filepath[512] = {0};
static bool play_request = false;
static bool stop_thread = false;

static double seek_target_secs = -1.0;
static bool seek_request = false;

// Set true by the playback thread when a track plays through to its end (as
// opposed to being stopped/replaced by the user). Consumed via
// audio_take_completion() so the controller can auto-advance.
static bool track_completed = false;

static double progress_current_secs = 0.0;
static double progress_total_secs = 0.0;

static int stream_sample_rate = 0;
static int stream_channels = 0;

// Helper: Parse WAV file headers
static int parse_wav(const char *filepath, wav_info_t *info, FILE **file_out) {
	FILE *f = fopen(filepath, "rb");
	if (!f) {
		fprintf(stderr, "Audio: Failed to open file: %s\n", filepath);
		return -1;
	}

	char riff_header[12];
	if (fread(riff_header, 1, 12, f) != 12) {
		fclose(f);
		return -1;
	}

	if (memcmp(riff_header, "RIFF", 4) != 0 || memcmp(riff_header + 8, "WAVE", 4) != 0) {
		fclose(f);
		return -1; // Not a valid WAVE file
	}

	info->channels = 0;
	info->sample_rate = 0;
	info->bits_per_sample = 0;
	info->data_offset = 0;
	info->data_size = 0;

	struct {
		char id[4];
		uint32_t size;
	} chunk;

	while (fread(&chunk, 1, sizeof(chunk), f) == sizeof(chunk)) {
		if (memcmp(chunk.id, "fmt ", 4) == 0) {
			struct {
				uint16_t format;
				uint16_t channels;
				uint32_t rate;
				uint32_t byterate;
				uint16_t align;
				uint16_t bps;
			} fmt;
			if (chunk.size < 16) {
				fclose(f);
				return -1;
			}
			if (fread(&fmt, 1, 16, f) != 16) {
				fclose(f);
				return -1;
			}
			info->channels = fmt.channels;
			info->sample_rate = fmt.rate;
			info->bits_per_sample = fmt.bps;

			if (chunk.size > 16) {
				fseek(f, chunk.size - 16, SEEK_CUR);
			}
		} else if (memcmp(chunk.id, "data", 4) == 0) {
			info->data_offset = ftell(f);
			info->data_size = chunk.size;
			break;
		} else {
			// Skip unrecognized chunks, align chunk size
			uint32_t skip_sz = (chunk.size + 1) & ~1;
			fseek(f, skip_sz, SEEK_CUR);
		}
	}

	if (info->channels == 0 || info->sample_rate == 0 || info->bits_per_sample == 0 || info->data_offset == 0) {
		fclose(f);
		return -1;
	}

	*file_out = f;
	return 0;
}

// Opens and configures the ALSA PCM device for playback. Returns NULL on
// failure (already logged to stderr). On success, *period_size_out holds the
// negotiated period size in frames.
static snd_pcm_t *open_pcm_device(int channels, int sample_rate, int bits_per_sample, snd_pcm_uframes_t *period_size_out) {
	snd_pcm_t *pcm_handle = NULL;
	int err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		fprintf(stderr, "Audio: Cannot open PCM device 'default': %s\n", snd_strerror(err));
		return NULL;
	}

	snd_pcm_hw_params_t *hw_params;
	snd_pcm_hw_params_alloca(&hw_params);
	snd_pcm_hw_params_any(pcm_handle, hw_params);
	snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);

	snd_pcm_format_t format;
	if (bits_per_sample == 8) {
		format = SND_PCM_FORMAT_U8;
	} else if (bits_per_sample == 16) {
		format = SND_PCM_FORMAT_S16_LE;
	} else if (bits_per_sample == 24) {
		format = SND_PCM_FORMAT_S24_3LE;
	} else if (bits_per_sample == 32) {
		format = SND_PCM_FORMAT_S32_LE;
	} else {
		fprintf(stderr, "Audio: Unsupported bits per sample: %d\n", bits_per_sample);
		snd_pcm_close(pcm_handle);
		return NULL;
	}

	snd_pcm_hw_params_set_format(pcm_handle, hw_params, format);
	snd_pcm_hw_params_set_channels(pcm_handle, hw_params, channels);

	unsigned int val = sample_rate;
	int dir = 0;
	snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &val, &dir);

	// Period size configuration (1024 frames)
	unsigned int periods = 4;
	snd_pcm_uframes_t period_size = 1024;
	snd_pcm_hw_params_set_periods_near(pcm_handle, hw_params, &periods, &dir);
	snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &period_size, &dir);

	err = snd_pcm_hw_params(pcm_handle, hw_params);
	if (err < 0) {
		fprintf(stderr, "Audio: Cannot apply HW parameters: %s\n", snd_strerror(err));
		snd_pcm_close(pcm_handle);
		return NULL;
	}

	*period_size_out = period_size;
	return pcm_handle;
}

// Play routine for uncompressed WAV files: PCM data is streamed straight from
// the file, no decode step needed.
static void play_wav_file(const char *filepath) {
	FILE *f = NULL;
	wav_info_t info;
	if (parse_wav(filepath, &info, &f) < 0) {
		fprintf(stderr, "Audio: Failed to parse WAV metadata: %s\n", filepath);
		pthread_mutex_lock(&audio_mutex);
		audio_command = AUDIO_CMD_STOP;
		playback_status = AUDIO_STATUS_STOPPED;
		pthread_mutex_unlock(&audio_mutex);
		return;
	}

	double bytes_per_sec = info.sample_rate * info.channels * (info.bits_per_sample / 8.0);
	pthread_mutex_lock(&audio_mutex);
	progress_total_secs = (double)info.data_size / bytes_per_sec;
	progress_current_secs = 0.0;
	stream_sample_rate = info.sample_rate;
	stream_channels = info.channels;
	playback_status = AUDIO_STATUS_PLAYING;
	pthread_mutex_unlock(&audio_mutex);

	snd_pcm_uframes_t period_size;
	snd_pcm_t *pcm_handle = open_pcm_device(info.channels, info.sample_rate, info.bits_per_sample, &period_size);
	if (!pcm_handle) {
		fclose(f);
		pthread_mutex_lock(&audio_mutex);
		audio_command = AUDIO_CMD_STOP;
		playback_status = AUDIO_STATUS_STOPPED;
		pthread_mutex_unlock(&audio_mutex);
		return;
	}

	int frame_bytes = info.channels * (info.bits_per_sample / 8);
	char *buffer = malloc(period_size * frame_bytes);
	if (!buffer) {
		fprintf(stderr, "Audio: Out of memory for period buffer\n");
		snd_pcm_close(pcm_handle);
		fclose(f);
		pthread_mutex_lock(&audio_mutex);
		audio_command = AUDIO_CMD_STOP;
		playback_status = AUDIO_STATUS_STOPPED;
		pthread_mutex_unlock(&audio_mutex);
		return;
	}

	long bytes_played = 0;
	bool is_paused = false;

	while (1) {
		pthread_mutex_lock(&audio_mutex);
		if (audio_command == AUDIO_CMD_STOP || play_request) {
			pthread_mutex_unlock(&audio_mutex);
			break;
		}
		if (seek_request) {
			double target = seek_target_secs;
			seek_request = false;
			pthread_mutex_unlock(&audio_mutex);

			long target_byte_pos = (long)(target * bytes_per_sec);
			target_byte_pos = (target_byte_pos / frame_bytes) * frame_bytes;
			if (target_byte_pos < 0)
				target_byte_pos = 0;
			if (target_byte_pos > info.data_size)
				target_byte_pos = info.data_size;

			fseek(f, info.data_offset + target_byte_pos, SEEK_SET);

			snd_pcm_drop(pcm_handle);
			snd_pcm_prepare(pcm_handle);
			is_paused = false;

			bytes_played = target_byte_pos;
			pthread_mutex_lock(&audio_mutex);
			progress_current_secs = (double)bytes_played / bytes_per_sec;
			pthread_mutex_unlock(&audio_mutex);
			continue;
		}
		if (audio_command == AUDIO_CMD_PAUSE) {
			if (!is_paused) {
				snd_pcm_state_t pcm_state = snd_pcm_state(pcm_handle);
				if (pcm_state == SND_PCM_STATE_RUNNING) {
					snd_pcm_pause(pcm_handle, 1);
				}
				is_paused = true;
				playback_status = AUDIO_STATUS_PAUSED;
			}
			pthread_mutex_unlock(&audio_mutex);
			// TODO: is this sleep needed? or is there a more robust way of handling this?
			usleep(50000); // 50ms latency sleep
			continue;
		} else {
			if (is_paused) {
				snd_pcm_state_t pcm_state = snd_pcm_state(pcm_handle);
				if (pcm_state == SND_PCM_STATE_PAUSED) {
					snd_pcm_pause(pcm_handle, 0);
				}
				is_paused = false;
				playback_status = AUDIO_STATUS_PLAYING;
			}
		}
		pthread_mutex_unlock(&audio_mutex);

		size_t read_bytes = fread(buffer, 1, period_size * frame_bytes, f);
		if (read_bytes <= 0) {
			// Track completed naturally
			pthread_mutex_lock(&audio_mutex);
			audio_command = AUDIO_CMD_STOP;
			playback_status = AUDIO_STATUS_STOPPED;
			track_completed = true;
			pthread_mutex_unlock(&audio_mutex);
			break;
		}

		snd_pcm_uframes_t frames_to_write = read_bytes / frame_bytes;
		snd_pcm_sframes_t written = snd_pcm_writei(pcm_handle, buffer, frames_to_write);

		if (written == -EPIPE) {
			snd_pcm_prepare(pcm_handle);
		} else if (written < 0) {
			fprintf(stderr, "Audio: Write failed: %s\n", snd_strerror(written));
			pthread_mutex_lock(&audio_mutex);
			audio_command = AUDIO_CMD_STOP;
			playback_status = AUDIO_STATUS_STOPPED;
			pthread_mutex_unlock(&audio_mutex);
			break;
		} else {
			bytes_played += written * frame_bytes;
			pthread_mutex_lock(&audio_mutex);
			progress_current_secs = (double)bytes_played / bytes_per_sec;
			pthread_mutex_unlock(&audio_mutex);
		}
	}

	pthread_mutex_lock(&audio_mutex);
	playback_status = AUDIO_STATUS_STOPPED;
	pthread_mutex_unlock(&audio_mutex);

	snd_pcm_drain(pcm_handle);
	snd_pcm_close(pcm_handle);
	free(buffer);
	fclose(f);
}

// Play routine for compressed formats (MP3/FLAC/OGG Vorbis) via the decoder
// abstraction in decode.h. Always decodes to interleaved signed 16-bit PCM.
static void play_decoded_file(const char *filepath, decode_format_t format) {
	decoder_t *dec = decoder_open(filepath, format);
	if (!dec) {
		fprintf(stderr, "Audio: Failed to open decoder for: %s\n", filepath);
		pthread_mutex_lock(&audio_mutex);
		audio_command = AUDIO_CMD_STOP;
		playback_status = AUDIO_STATUS_STOPPED;
		pthread_mutex_unlock(&audio_mutex);
		return;
	}

	int channels = decoder_channels(dec);
	int sample_rate = decoder_sample_rate(dec);
	uint64_t total_frames = decoder_total_pcm_frames(dec);

	double bytes_per_sec = (double)sample_rate * channels * 2; // 16-bit output
	pthread_mutex_lock(&audio_mutex);
	progress_total_secs = (double)total_frames / sample_rate;
	progress_current_secs = 0.0;
	stream_sample_rate = sample_rate;
	stream_channels = channels;
	playback_status = AUDIO_STATUS_PLAYING;
	pthread_mutex_unlock(&audio_mutex);

	snd_pcm_uframes_t period_size;
	snd_pcm_t *pcm_handle = open_pcm_device(channels, sample_rate, 16, &period_size);
	if (!pcm_handle) {
		decoder_close(dec);
		pthread_mutex_lock(&audio_mutex);
		audio_command = AUDIO_CMD_STOP;
		playback_status = AUDIO_STATUS_STOPPED;
		pthread_mutex_unlock(&audio_mutex);
		return;
	}

	int frame_bytes = channels * 2;
	short *buffer = malloc(period_size * frame_bytes);
	if (!buffer) {
		fprintf(stderr, "Audio: Out of memory for period buffer\n");
		snd_pcm_close(pcm_handle);
		decoder_close(dec);
		pthread_mutex_lock(&audio_mutex);
		audio_command = AUDIO_CMD_STOP;
		playback_status = AUDIO_STATUS_STOPPED;
		pthread_mutex_unlock(&audio_mutex);
		return;
	}

	long bytes_played = 0;
	bool is_paused = false;

	while (1) {
		pthread_mutex_lock(&audio_mutex);
		if (audio_command == AUDIO_CMD_STOP || play_request) {
			pthread_mutex_unlock(&audio_mutex);
			break;
		}
		if (seek_request) {
			double target = seek_target_secs;
			seek_request = false;
			pthread_mutex_unlock(&audio_mutex);

			uint64_t target_frame = (uint64_t)(target * sample_rate);
			if (target_frame > total_frames) {
				target_frame = total_frames;
			}

			decoder_seek_to_frame(dec, target_frame);

			snd_pcm_drop(pcm_handle);
			snd_pcm_prepare(pcm_handle);
			is_paused = false;

			bytes_played = target_frame * frame_bytes;
			pthread_mutex_lock(&audio_mutex);
			progress_current_secs = (double)bytes_played / bytes_per_sec;
			pthread_mutex_unlock(&audio_mutex);
			continue;
		}
		if (audio_command == AUDIO_CMD_PAUSE) {
			if (!is_paused) {
				snd_pcm_state_t pcm_state = snd_pcm_state(pcm_handle);
				if (pcm_state == SND_PCM_STATE_RUNNING) {
					snd_pcm_pause(pcm_handle, 1);
				}
				is_paused = true;
				playback_status = AUDIO_STATUS_PAUSED;
			}
			pthread_mutex_unlock(&audio_mutex);
			usleep(50000); // 50ms latency sleep
			continue;
		} else {
			if (is_paused) {
				snd_pcm_state_t pcm_state = snd_pcm_state(pcm_handle);
				if (pcm_state == SND_PCM_STATE_PAUSED) {
					snd_pcm_pause(pcm_handle, 0);
				}
				is_paused = false;
				playback_status = AUDIO_STATUS_PLAYING;
			}
		}
		pthread_mutex_unlock(&audio_mutex);

		uint64_t frames_read = decoder_read_pcm_frames_s16(dec, period_size, buffer);
		if (frames_read == 0) {
			// Track completed naturally
			pthread_mutex_lock(&audio_mutex);
			audio_command = AUDIO_CMD_STOP;
			playback_status = AUDIO_STATUS_STOPPED;
			track_completed = true;
			pthread_mutex_unlock(&audio_mutex);
			break;
		}

		snd_pcm_sframes_t written = snd_pcm_writei(pcm_handle, buffer, frames_read);

		if (written == -EPIPE) {
			snd_pcm_prepare(pcm_handle);
		} else if (written < 0) {
			fprintf(stderr, "Audio: Write failed: %s\n", snd_strerror(written));
			pthread_mutex_lock(&audio_mutex);
			audio_command = AUDIO_CMD_STOP;
			playback_status = AUDIO_STATUS_STOPPED;
			pthread_mutex_unlock(&audio_mutex);
			break;
		} else {
			bytes_played += written * frame_bytes;
			pthread_mutex_lock(&audio_mutex);
			progress_current_secs = (double)bytes_played / bytes_per_sec;
			pthread_mutex_unlock(&audio_mutex);
		}
	}

	pthread_mutex_lock(&audio_mutex);
	playback_status = AUDIO_STATUS_STOPPED;
	pthread_mutex_unlock(&audio_mutex);

	snd_pcm_drain(pcm_handle);
	snd_pcm_close(pcm_handle);
	free(buffer);
	decoder_close(dec);
}

// Play file routine running inside the playback thread
static void play_file(const char *filepath) {
	decode_format_t format = decode_detect_format(filepath);
	if (format == DECODE_FORMAT_UNKNOWN) {
		play_wav_file(filepath);
	} else {
		play_decoded_file(filepath, format);
	}
}

// Background thread loop
static void *playback_thread_func(void *arg) {
	while (1) {
		pthread_mutex_lock(&audio_mutex);
		while (!play_request && !stop_thread) {
			pthread_cond_wait(&audio_cond, &audio_mutex);
		}
		if (stop_thread) {
			pthread_mutex_unlock(&audio_mutex);
			break;
		}

		char filepath[512];
		strncpy(filepath, current_filepath, sizeof(filepath));
		play_request = false;
		pthread_mutex_unlock(&audio_mutex);

		play_file(filepath);
	}
	return NULL;
}

int audio_init(void) {
	static bool initialized = false;
	if (initialized)
		return 0;

	// Apply ALSA mixer routing configurations
	set_volume(125);
	auto_set_output();

	pthread_mutex_lock(&audio_mutex);
	stop_thread = false;
	play_request = false;
	audio_command = AUDIO_CMD_STOP;
	playback_status = AUDIO_STATUS_STOPPED;
	pthread_mutex_unlock(&audio_mutex);

	int err = pthread_create(&playback_thread, NULL, playback_thread_func, NULL);
	if (err != 0) {
		fprintf(stderr, "Audio: Failed to create background thread\n");
		return -1;
	}

	initialized = true;
	return 0;
}

// play a new file
int audio_play(const char *filepath) {
	printf("playing\n");

	auto_set_output();

	pthread_mutex_lock(&audio_mutex);
	strncpy(current_filepath, filepath, sizeof(current_filepath) - 1);
	play_request = true;
	audio_command = AUDIO_CMD_PLAY;
	playback_status = AUDIO_STATUS_PLAYING;
	seek_request = false;
	seek_target_secs = -1.0;
	track_completed = false; // fresh playback; any prior completion is consumed
	pthread_cond_signal(&audio_cond);
	pthread_mutex_unlock(&audio_mutex);
	return 0;
}

// pause the current playback
// TODO: should we filter and make sure pausing is valid?
void audio_pause(void) {
	printf("pausing\n");
	pthread_mutex_lock(&audio_mutex);
	audio_command = AUDIO_CMD_PAUSE;
	pthread_mutex_unlock(&audio_mutex);
}

// resume paused playback
// TODO: should we filter and make sure resuming is valid?
void audio_resume(void) {
	printf("resuming\n");

	auto_set_output();

	pthread_mutex_lock(&audio_mutex);
	audio_command = AUDIO_CMD_RESUME;
	pthread_mutex_unlock(&audio_mutex);
}

// stop playback
// TODO: how exactly is this different than pausing? do we need both
void audio_stop(void) {
	printf("stopping\n");
	pthread_mutex_lock(&audio_mutex);
	audio_command = AUDIO_CMD_STOP;
	playback_status = AUDIO_STATUS_STOPPED;
	seek_request = false;
	seek_target_secs = -1.0;
	track_completed = false; // deliberate stop is not a natural completion
	pthread_mutex_unlock(&audio_mutex);
}

audio_status_t audio_get_status(void) {
	pthread_mutex_lock(&audio_mutex);
	audio_status_t status = playback_status;
	pthread_mutex_unlock(&audio_mutex);
	return status;
}

bool audio_take_completion(void) {
	pthread_mutex_lock(&audio_mutex);
	bool completed = track_completed;
	track_completed = false;
	pthread_mutex_unlock(&audio_mutex);
	return completed;
}

void audio_get_current_file(char *out, size_t out_size) {
	if (!out || out_size == 0)
		return;
	pthread_mutex_lock(&audio_mutex);
	strncpy(out, current_filepath, out_size - 1);
	out[out_size - 1] = '\0';
	pthread_mutex_unlock(&audio_mutex);
}

void audio_get_progress(double *current_secs, double *total_secs) {
	pthread_mutex_lock(&audio_mutex);
	if (current_secs)
		*current_secs = progress_current_secs;
	if (total_secs)
		*total_secs = progress_total_secs;
	pthread_mutex_unlock(&audio_mutex);
}

void audio_get_stream_info(int *sample_rate, int *channels) {
	pthread_mutex_lock(&audio_mutex);
	if (sample_rate)
		*sample_rate = stream_sample_rate;
	if (channels)
		*channels = stream_channels;
	pthread_mutex_unlock(&audio_mutex);
}

void audio_seek(double seconds) {
	printf("seeking to %.2f seconds\n", seconds);

	pthread_mutex_lock(&audio_mutex);
	progress_current_secs = seconds; // immediately update current secs
	seek_target_secs = seconds;
	seek_request = true;
	pthread_mutex_unlock(&audio_mutex);
}
