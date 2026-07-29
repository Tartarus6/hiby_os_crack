#ifndef ALSA_CONTROLS_H
#define ALSA_CONTROLS_H

#include <stdint.h>

int alsa_set_control(const char *name, long value);
void auto_set_output(void);
void set_volume(long volume);
void change_volume(long amount);
long get_volume(void);

#endif
