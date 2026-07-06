#include "switcher.h"
#include "src/display/lv_display.h"
#include "src/misc/lv_types.h"

void switch_screen_cb(lv_event_t *e) {
    // Get the screen to load from the event's user data
    lv_obj_t *target_screen = (lv_obj_t *) lv_event_get_user_data(e);

    // Load the target screen, making it active
    lv_screen_load(target_screen);
}
