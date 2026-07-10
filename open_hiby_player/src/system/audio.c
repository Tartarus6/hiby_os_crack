#include "audio.h"

#include "src/rockbox/hibylinux_codec.h"
#include "src/system/utils.h"
#include <stdio.h>

char * headset_state;

int audio_init(void) {
	audiohw_preinit();
    return 0;
}

int audio_play(const char *filepath) {
	printf("output: %d\n", hiby_has_valid_output());
	headset_state = read_file_content("/sys/class/switch/headset/state");
	printf("%s\n", headset_state);

	hiby_get_outputs(); // setting output automatically

    return 0;
}

void audio_pause(void) {
}

void audio_resume(void) {
}

void audio_stop(void) {
}
