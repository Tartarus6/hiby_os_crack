#ifndef GUI_H
#define GUI_H

#include <stdint.h>

typedef struct {
	const uint32_t screen_width;
	const uint32_t screen_height;
	const int8_t padding;
	const int8_t top_bar_height;
	const char *sd_root_path;
} gui_config_t;

void gui_init(gui_config_t *cfg);
void gui_notify_popup(const char *text);

#endif /* GUI_H */
