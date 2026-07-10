#ifndef HIBYLINUX_CODEC_H
#define HIBYLINUX_CODEC_H

extern void audiohw_mute(int mute);
extern int hiby_has_valid_output(void);
extern void audiohw_set_volume(int vol_l, int vol_r);
extern void hiby_set_output(int ps);
extern int hiby_get_outputs(void);
extern void audiohw_preinit(void);
extern void audiohw_postinit(void);
extern void audiohw_set_frequency(int fsel);
extern void audiohw_set_filter_roll_off(int value);

#endif
