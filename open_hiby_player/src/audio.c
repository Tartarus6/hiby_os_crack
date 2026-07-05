#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <linux/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>

#ifndef HOST_BUILD
#include <alsa/asoundlib.h>
#include <sndfile.h>
#endif

// Thread state variables
static pthread_t audio_thread;
static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t audio_cond = PTHREAD_COND_INITIALIZER;

static audio_state_t current_state = AUDIO_STATE_STOPPED;
static char pending_filepath[512] = {0};
static bool play_requested = false;
static bool stop_requested = false;
static bool exit_requested = false;

static double current_seconds = 0.0;
static double total_seconds = 0.0;

#ifdef HOST_BUILD
// Simulation helper for Host builds
static void *audio_worker_thread(void *arg) {
    (void)arg;
    char active_filepath[512] = {0};

    while (1) {
        pthread_mutex_lock(&audio_mutex);

        // Wait for commands if stopped
        while (!play_requested && !exit_requested && current_state == AUDIO_STATE_STOPPED) {
            pthread_cond_wait(&audio_cond, &audio_mutex);
        }

        if (exit_requested) {
            pthread_mutex_unlock(&audio_mutex);
            break;
        }

        // Handle play request
        if (play_requested) {
            strncpy(active_filepath, pending_filepath, sizeof(active_filepath) - 1);
            play_requested = false;
            stop_requested = false;
            current_seconds = 0.0;
            total_seconds = 180.0; // Mock: 3 minute track
            current_state = AUDIO_STATE_PLAYING;
            printf("[Audio Mock] Starting playback: %s\n", active_filepath);
        }

        audio_state_t state_local = current_state;
        pthread_mutex_unlock(&audio_mutex);

        if (state_local == AUDIO_STATE_PLAYING) {
            // Mock streaming
            usleep(100000); // 100ms

            pthread_mutex_lock(&audio_mutex);
            if (current_state == AUDIO_STATE_PLAYING) {
                current_seconds += 0.1;
                if (current_seconds >= total_seconds) {
                    current_state = AUDIO_STATE_STOPPED;
                    printf("[Audio Mock] Playback finished naturally.\n");
                }
            }
            pthread_mutex_unlock(&audio_mutex);
        } else if (state_local == AUDIO_STATE_PAUSED) {
            // Sleep and wait to be resumed/stopped/changed
            usleep(50000);
        } else if (state_local == AUDIO_STATE_STOPPED) {
            // Just idle
            usleep(50000);
        }

        // Handle stop request
        pthread_mutex_lock(&audio_mutex);
        if (stop_requested) {
            stop_requested = false;
            current_state = AUDIO_STATE_STOPPED;
            current_seconds = 0.0;
            printf("[Audio Mock] Playback stopped.\n");
        }
        pthread_mutex_unlock(&audio_mutex);
    }
    return NULL;
}
#else
// Target Build: Real ALSA and libsndfile implementation
static void *audio_worker_thread(void *arg) {
    (void)arg;
    char active_filepath[512] = {0};
    snd_pcm_t *pcm = NULL;
    SNDFILE *infile = NULL;
    SF_INFO sfinfo;
    short *buffer = NULL;
    const int FRAME_SIZE = 1024;
    int period_size = 1024;
    int buffer_size = 4096;

    while (1) {
        pthread_mutex_lock(&audio_mutex);
        while (!play_requested && !exit_requested && current_state == AUDIO_STATE_STOPPED) {
            pthread_cond_wait(&audio_cond, &audio_mutex);
        }
        if (exit_requested) {
            pthread_mutex_unlock(&audio_mutex);
            break;
        }

        if (play_requested) {
            // Clean up previous
            if (pcm) { snd_pcm_drain(pcm); snd_pcm_close(pcm); pcm = NULL; }
            if (infile) { sf_close(infile); infile = NULL; }
            if (buffer) { free(buffer); buffer = NULL; }

            strncpy(active_filepath, pending_filepath, sizeof(active_filepath)-1);
            play_requested = false;
            stop_requested = false;
            current_seconds = 0.0;
            total_seconds = 0.0;

            // Open file
            memset(&sfinfo, 0, sizeof(sfinfo));
            infile = sf_open(active_filepath, SFM_READ, &sfinfo);
            if (!infile) {
                fprintf(stderr, "[Audio] Failed to open file: %s\n", active_filepath);
                current_state = AUDIO_STATE_STOPPED;
                pthread_mutex_unlock(&audio_mutex);
                continue;
            }
            total_seconds = (double)sfinfo.frames / sfinfo.samplerate;

            // Open PCM (try raw first, fallback to plughw)
            const char *pcm_name = "hw:0,0";
            int err = snd_pcm_open(&pcm, pcm_name, SND_PCM_STREAM_PLAYBACK, 0);
            if (err < 0) {
                pcm_name = "plughw:0,0";
                err = snd_pcm_open(&pcm, pcm_name, SND_PCM_STREAM_PLAYBACK, 0);
            }
            if (err < 0) {
                fprintf(stderr, "[Audio] Cannot open PCM: %s\n", snd_strerror(err));
                sf_close(infile); infile = NULL;
                current_state = AUDIO_STATE_STOPPED;
                pthread_mutex_unlock(&audio_mutex);
                continue;
            }

            // Set hardware params
            snd_pcm_hw_params_t *hw_params;
            snd_pcm_hw_params_alloca(&hw_params);
            snd_pcm_hw_params_any(pcm, hw_params);
            snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);

            // Force 16-bit (since we use short buffer)
            snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE);
            snd_pcm_hw_params_set_channels(pcm, hw_params, sfinfo.channels);

            unsigned int rate = sfinfo.samplerate;
            err = snd_pcm_hw_params_set_rate_near(pcm, hw_params, &rate, 0);
            if (err < 0) {
                fprintf(stderr, "[Audio] Rate not supported: %s\n", snd_strerror(err));
                snd_pcm_close(pcm); pcm = NULL;
                sf_close(infile); infile = NULL;
                current_state = AUDIO_STATE_STOPPED;
                pthread_mutex_unlock(&audio_mutex);
                continue;
            }

            // Set period and buffer size
            snd_pcm_hw_params_set_period_size_near(pcm, hw_params, &period_size, 0);
            snd_pcm_hw_params_set_buffer_size_near(pcm, hw_params, &buffer_size);

            err = snd_pcm_hw_params(pcm, hw_params);
            if (err < 0) {
                fprintf(stderr, "[Audio] Cannot set hw params: %s\n", snd_strerror(err));
                snd_pcm_close(pcm); pcm = NULL;
                sf_close(infile); infile = NULL;
                current_state = AUDIO_STATE_STOPPED;
                pthread_mutex_unlock(&audio_mutex);
                continue;
            }

            snd_pcm_prepare(pcm);
            usleep(10000); // Let driver settle

            buffer = malloc(FRAME_SIZE * sfinfo.channels * sizeof(short));
            if (!buffer) {
                fprintf(stderr, "[Audio] Out of memory\n");
                snd_pcm_close(pcm); pcm = NULL;
                sf_close(infile); infile = NULL;
                current_state = AUDIO_STATE_STOPPED;
                pthread_mutex_unlock(&audio_mutex);
                continue;
            }

            current_state = AUDIO_STATE_PLAYING;
            printf("[Audio] Playback started: %s\n", active_filepath);
        }

        audio_state_t state_local = current_state;
        bool stop_local = stop_requested;
        pthread_mutex_unlock(&audio_mutex);

        if (state_local == AUDIO_STATE_PLAYING && !stop_local && !play_requested) {
            sf_count_t frames_read = sf_readf_short(infile, buffer, FRAME_SIZE);
            if (frames_read > 0) {
                snd_pcm_sframes_t frames_written = snd_pcm_writei(pcm, buffer, frames_read);
                if (frames_written < 0) {
                    if (frames_written == -EPIPE) {
                        snd_pcm_prepare(pcm);
                    } else if (frames_written == -EAGAIN) {
                        snd_pcm_wait(pcm, 1000);
                    } else {
                        fprintf(stderr, "[Audio] ALSA write error: %s\n", snd_strerror(frames_written));
                        pthread_mutex_lock(&audio_mutex);
                        current_state = AUDIO_STATE_STOPPED;
                        stop_requested = true;
                        pthread_mutex_unlock(&audio_mutex);
                        break;
                    }
                } else {
                    // Update progress
                    sf_count_t current_frame = sf_seek(infile, 0, SEEK_CUR);
                    pthread_mutex_lock(&audio_mutex);
                    current_seconds = (double)current_frame / sfinfo.samplerate;
                    pthread_mutex_unlock(&audio_mutex);
                }
            } else {
                // End of file
                pthread_mutex_lock(&audio_mutex);
                current_state = AUDIO_STATE_STOPPED;
                stop_requested = true;
                pthread_mutex_unlock(&audio_mutex);
                break;
            }
        } else if (state_local == AUDIO_STATE_PAUSED) {
            usleep(50000);
        } else {
            usleep(20000);
        }

        // Handle stop
        pthread_mutex_lock(&audio_mutex);
        if (stop_requested && !play_requested) {
            stop_requested = false;
            current_state = AUDIO_STATE_STOPPED;
            current_seconds = 0.0;
            if (pcm) { snd_pcm_drain(pcm); snd_pcm_close(pcm); pcm = NULL; }
            if (infile) { sf_close(infile); infile = NULL; }
            if (buffer) { free(buffer); buffer = NULL; }
            printf("[Audio] Playback stopped.\n");
        }
        pthread_mutex_unlock(&audio_mutex);
    }

    if (pcm) snd_pcm_close(pcm);
    if (infile) sf_close(infile);
    if (buffer) free(buffer);
    return NULL;
}
#endif

#define SA_CONFIG_IOCTL_INIT  _IOC(_IOC_NONE|_IOC_READ|_IOC_WRITE, 0, 0xf0, 0x1fff)

int audio_init(void) {
    // Start thread
    int ret = pthread_create(&audio_thread, NULL, audio_worker_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "[Audio] Failed to spawn audio thread!\n");
        return -1;
    }
    printf("[Audio] Subsystem initialized.\n");
    return 0;
}

int audio_play(const char *filepath) {
    if (!filepath) return -1;
    pthread_mutex_lock(&audio_mutex);
    strncpy(pending_filepath, filepath, sizeof(pending_filepath) - 1);
    pending_filepath[sizeof(pending_filepath) - 1] = '\0';
    play_requested = true;
    stop_requested = false;
    current_state = AUDIO_STATE_STOPPED; // reset to trigger play initialization
    pthread_cond_signal(&audio_cond);
    pthread_mutex_unlock(&audio_mutex);
    return 0;
}

void audio_pause(void) {
    pthread_mutex_lock(&audio_mutex);
    if (current_state == AUDIO_STATE_PLAYING) {
        current_state = AUDIO_STATE_PAUSED;
    }
    pthread_mutex_unlock(&audio_mutex);
}

void audio_resume(void) {
    pthread_mutex_lock(&audio_mutex);
    if (current_state == AUDIO_STATE_PAUSED) {
        current_state = AUDIO_STATE_PLAYING;
    }
    pthread_mutex_unlock(&audio_mutex);
}

void audio_stop(void) {
    pthread_mutex_lock(&audio_mutex);
    if (current_state != AUDIO_STATE_STOPPED) {
        stop_requested = true;
        pthread_cond_signal(&audio_cond);
    }
    pthread_mutex_unlock(&audio_mutex);
}

audio_state_t audio_get_state(void) {
    pthread_mutex_lock(&audio_mutex);
    audio_state_t state = current_state;
    pthread_mutex_unlock(&audio_mutex);
    return state;
}

void audio_get_progress(double *current_secs, double *total_secs) {
    pthread_mutex_lock(&audio_mutex);
    if (current_secs) *current_secs = current_seconds;
    if (total_secs) *total_secs = total_seconds;
    pthread_mutex_unlock(&audio_mutex);
}
