#include "audio.h"

#include "src/rockbox/hibylinux_codec.h"
#include "src/system/utils.h"

#include <stdio.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

static ma_engine engine;
static ma_sound sound;

int audio_init(void)
{
    audiohw_init();

    ma_result result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS)
        return -1;

    // TODO: volume should probably not be set to some const here, either leave default or remember prev setting
    audiohw_set_volume(200, 200);

    hiby_auto_set_output();

    return 0;
}

int audio_play(const char *filepath)
{
    // hiby_auto_set_output();

    ma_sound_uninit(&sound);

    ma_result result = ma_sound_init_from_file(
        &engine,
        filepath,
        0,
        NULL,
        NULL,
        &sound);

    if (result != MA_SUCCESS)
        return -1;

    ma_sound_start(&sound);

    return 0;
}

void audio_pause(void)
{
    ma_sound_stop(&sound);
}

void audio_resume(void)
{
    ma_sound_start(&sound);
}

void audio_stop(void)
{
    ma_sound_stop(&sound);
    ma_sound_seek_to_pcm_frame(&sound, 0);
}
