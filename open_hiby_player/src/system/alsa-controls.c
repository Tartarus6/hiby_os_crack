#include "alsa-controls.h"
#include "src/system/utils.h"

#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static long current_volume;

// used to set an alsa control value such as volume or output port
int alsa_set_control(const char *name, long value) {
	snd_ctl_t *ctl;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *elem;
	int err;

	err = snd_ctl_open(&ctl, "hw:0", 0);
	if (err < 0) {
		return err;
	}

	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_value_alloca(&elem);

	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_id_set_name(id, name);

	snd_ctl_elem_value_set_id(elem, id);
	snd_ctl_elem_value_set_integer(elem, 0, value);

	err = snd_ctl_elem_write(ctl, elem);

	snd_ctl_close(ctl);
	return err;
}

// TODO: bluetooth? how does outputting over bluetooth work?
// returns a valid value for ALSA "Output Port Switch" based on what's plugged in
int detect_output(void) {
	const char *const sysfs_hs_switch = "/sys/class/switch/headset/state";
	const char *const sysfs_bal_switch = "/sys/class/switch/balance/state";

	if (file_matches(sysfs_bal_switch, "1")) {
		return 3; // 4.4mm balanced output
	}

	if (file_matches(sysfs_hs_switch, "1")) {
		return 2; // 3.5mm headset output
	}

	// return 4; // Default to i2s (S/PDIF) (USB) device
	return 2; // Default to 3.5mm device
}

// TODO: set some state variable to show which output is currently selected
// automatically sets ALSA "Output Port Switch" based on what's plugged in
void auto_set_output(void) {
	int output = detect_output();
	alsa_set_control("Output Port Switch", output);

	printf("set output to %d\n", output);
}

// TODO: add balance setting support
// TODO: maybe switch to a non-reverse volume?
// set left and right ALSA volumes
// NOTE: volume is 0-255
void set_volume(long volume) {
	current_volume = volume;

	alsa_set_control("Right Playback Volume", current_volume);
	alsa_set_control("Left Playback Volume", current_volume);

	printf("set volume to %d\n", current_volume);
}

// returns the last volume value set via set_volume()/change_volume()
long get_volume(void) { return current_volume; }

// change volume by given amount, positive or negative
void change_volume(long amount) {
	current_volume += amount;

	if (current_volume > 255) {
		current_volume = 255;
	}

	if (current_volume < 0) {
		current_volume = 0;
	}

	set_volume(current_volume);
}
