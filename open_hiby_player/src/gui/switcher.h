#ifndef GUI_UTILS_H
#define GUI_UTILS_H

#include "lvgl/lvgl.h"
#include "src/gui/gui.h"

void switch_screen(lv_obj_t *target_screen);
void switch_screen_cb(lv_event_t *e);
void back_btn_init(gui_config_t *cfg);

#endif
