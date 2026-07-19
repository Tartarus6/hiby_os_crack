#ifndef PLAYER_H
#define PLAYER_H

#include "src/gui/gui.h"

#include "lvgl/lvgl.h"

extern lv_obj_t *player_screen;

void player_init(gui_config_t *cfg);

// Start playing the given file and switch its now-playing info in the UI.
// Does not switch screens; call lv_screen_load(player_screen) separately.
void player_play_file(const char *filepath);

#endif
