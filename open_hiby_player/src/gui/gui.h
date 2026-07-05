#ifndef GUI_H
#define GUI_H

#include "lvgl/lvgl.h"
#include <stdint.h>

typedef struct {
    const uint32_t screen_width;
    const uint32_t screen_height;
    const int8_t top_bar_padding;
    const int8_t top_bar_height;
} gui_config_t;

void gui_init(gui_config_t *cfg);
void popup_async_cb(void *user_data);

#endif /* GUI_H */
