#include "audio.h"
#include "src/system/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <alloca.h>

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

static audio_state_t audio_state = AUDIO_STATE_STOPPED;
static char current_filepath[512] = {0};
static bool play_request = false;
static bool stop_thread = false;

static double progress_current_secs = 0.0;
static double progress_total_secs = 0.0;

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

// Play file routine running inside the playback thread
static void play_file(const char *filepath) {
    FILE *f = NULL;
    wav_info_t info;
    if (parse_wav(filepath, &info, &f) < 0) {
        fprintf(stderr, "Audio: Failed to parse WAV metadata: %s\n", filepath);
        pthread_mutex_lock(&audio_mutex);
        audio_state = AUDIO_STATE_STOPPED;
        pthread_mutex_unlock(&audio_mutex);
        return;
    }

    double bytes_per_sec = info.sample_rate * info.channels * (info.bits_per_sample / 8.0);
    pthread_mutex_lock(&audio_mutex);
    progress_total_secs = (double)info.data_size / bytes_per_sec;
    progress_current_secs = 0.0;
    pthread_mutex_unlock(&audio_mutex);

    snd_pcm_t *pcm_handle = NULL;
    int err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Audio: Cannot open PCM device 'default': %s\n", snd_strerror(err));
        fclose(f);
        pthread_mutex_lock(&audio_mutex);
        audio_state = AUDIO_STATE_STOPPED;
        pthread_mutex_unlock(&audio_mutex);
        return;
    }

    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(pcm_handle, hw_params);
    snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_format_t format;
    if (info.bits_per_sample == 8) {
        format = SND_PCM_FORMAT_U8;
    } else if (info.bits_per_sample == 16) {
        format = SND_PCM_FORMAT_S16_LE;
    } else if (info.bits_per_sample == 24) {
        format = SND_PCM_FORMAT_S24_3LE;
    } else if (info.bits_per_sample == 32) {
        format = SND_PCM_FORMAT_S32_LE;
    } else {
        fprintf(stderr, "Audio: Unsupported bits per sample: %d\n", info.bits_per_sample);
        snd_pcm_close(pcm_handle);
        fclose(f);
        pthread_mutex_lock(&audio_mutex);
        audio_state = AUDIO_STATE_STOPPED;
        pthread_mutex_unlock(&audio_mutex);
        return;
    }

    snd_pcm_hw_params_set_format(pcm_handle, hw_params, format);
    snd_pcm_hw_params_set_channels(pcm_handle, hw_params, info.channels);

    unsigned int val = info.sample_rate;
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
        fclose(f);
        pthread_mutex_lock(&audio_mutex);
        audio_state = AUDIO_STATE_STOPPED;
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
        audio_state = AUDIO_STATE_STOPPED;
        pthread_mutex_unlock(&audio_mutex);
        return;
    }

    long bytes_played = 0;
    bool is_paused = false;

    while (1) {
        pthread_mutex_lock(&audio_mutex);
        if (audio_state == AUDIO_STATE_STOPPED || play_request) {
            pthread_mutex_unlock(&audio_mutex);
            break;
        }
        if (audio_state == AUDIO_STATE_PAUSED) {
            if (!is_paused) {
                snd_pcm_pause(pcm_handle, 1);
                is_paused = true;
            }
            pthread_mutex_unlock(&audio_mutex);
            usleep(50000); // 50ms latency sleep
            continue;
        } else {
            if (is_paused) {
                snd_pcm_pause(pcm_handle, 0);
                is_paused = false;
            }
        }
        pthread_mutex_unlock(&audio_mutex);

        size_t read_bytes = fread(buffer, 1, period_size * frame_bytes, f);
        if (read_bytes <= 0) {
            // Track completed naturally
            pthread_mutex_lock(&audio_mutex);
            audio_state = AUDIO_STATE_STOPPED;
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
            audio_state = AUDIO_STATE_STOPPED;
            pthread_mutex_unlock(&audio_mutex);
            break;
        } else {
            bytes_played += written * frame_bytes;
            pthread_mutex_lock(&audio_mutex);
            progress_current_secs = (double)bytes_played / bytes_per_sec;
            pthread_mutex_unlock(&audio_mutex);
        }
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    free(buffer);
    fclose(f);
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
    static bool inited = false;
    if (inited) return 0;

    // Apply ALSA mixer routing configurations
    system("sh -c \"amixer -c 0 cset numid=2 100\"");
    system("sh -c \"amixer -c 0 cset numid=1 100\"");
    system("sh -c \"amixer -c 0 cset numid=9 2\"");

    pthread_mutex_lock(&audio_mutex);
    stop_thread = false;
    play_request = false;
    audio_state = AUDIO_STATE_STOPPED;
    pthread_mutex_unlock(&audio_mutex);

    int err = pthread_create(&playback_thread, NULL, playback_thread_func, NULL);
    if (err != 0) {
        fprintf(stderr, "Audio: Failed to create background thread\n");
        return -1;
    }

    inited = true;
    return 0;
}

int audio_play(const char *filepath) {
	printf("playing\n");
    pthread_mutex_lock(&audio_mutex);
    strncpy(current_filepath, filepath, sizeof(current_filepath) - 1);
    play_request = true;
    audio_state = AUDIO_STATE_PLAYING;
    pthread_cond_signal(&audio_cond);
    pthread_mutex_unlock(&audio_mutex);
    return 0;
}

void audio_pause(void) {
	printf("pausing\n");
    pthread_mutex_lock(&audio_mutex);
    if (audio_state == AUDIO_STATE_PLAYING) {
        audio_state = AUDIO_STATE_PAUSED;
    }
    pthread_mutex_unlock(&audio_mutex);
}

void audio_resume(void) {
	printf("resuming\n");
    pthread_mutex_lock(&audio_mutex);
    if (audio_state == AUDIO_STATE_PAUSED) {
        audio_state = AUDIO_STATE_PLAYING;
    }
    pthread_mutex_unlock(&audio_mutex);
}

void audio_stop(void) {
	printf("stopping\n");
    pthread_mutex_lock(&audio_mutex);
    audio_state = AUDIO_STATE_STOPPED;
    pthread_mutex_unlock(&audio_mutex);
}

audio_state_t audio_get_state(void) {
    pthread_mutex_lock(&audio_mutex);
    audio_state_t state = audio_state;
    pthread_mutex_unlock(&audio_mutex);
    return state;
}

void audio_get_progress(double *current_secs, double *total_secs) {
    pthread_mutex_lock(&audio_mutex);
    if (current_secs) *current_secs = progress_current_secs;
    if (total_secs) *total_secs = progress_total_secs;
    pthread_mutex_unlock(&audio_mutex);
}
