#ifndef GUI_H
#define GUI_H

#include "lvgl/lvgl.h"
#include <stdint.h>


void gui_init(uint32_t screen_width, uint32_t screen_height);
void popup_async_cb(void *user_data);

#endif /* GUI_H */
