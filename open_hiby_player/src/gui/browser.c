#include "browser.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "lvgl/lvgl.h"

#include "src/gui/gui.h"
#include "src/gui/player.h"
#include "src/gui/switcher.h"
#include "src/system/utils.h"

lv_obj_t *browser_screen;

static lv_obj_t *file_list;
static lv_obj_t *path_label;

static lv_style_t style_list_btn;
static lv_style_t style_list_btn_pressed;
static lv_style_t style_list_error;

// Root of the browsable area (the SD card mount point). Navigation is not
// allowed to go above this directory.
static char root_path[512];
static char current_path[512];

typedef struct {
	char name[256];
	bool is_dir;
} browser_entry_t;

static void init_list_styles(void) {
	// error text style
	lv_style_init(&style_list_error);
	lv_style_set_bg_opa(&style_list_error, LV_OPA_TRANSP);
	lv_style_set_border_width(&style_list_error, 0);
	lv_style_set_radius(&style_list_error, 0);
	lv_style_set_text_color(&style_list_error, lv_color_make(212, 94, 76));
	lv_style_set_text_align(&style_list_error, LV_TEXT_ALIGN_CENTER);

	// button style
	lv_style_init(&style_list_btn);
	lv_style_set_bg_color(&style_list_btn, lv_color_make(45, 45, 52));
	lv_style_set_bg_opa(&style_list_btn, LV_OPA_COVER);
	// lv_style_set_border_width(&style_list_btn, 0);
	lv_style_set_radius(&style_list_btn, 0);
	lv_style_set_text_color(&style_list_btn, lv_color_white());

	// button pressed style
	lv_style_init(&style_list_btn_pressed);
	lv_style_set_bg_color(&style_list_btn_pressed, lv_color_make(60, 160, 220));
}

static void browser_open_dir(const char *path);

static bool is_playable_file(const char *name) {
	static const char *const playable_exts[] = {".wav", ".mp3", ".flac", ".ogg"};

	for (size_t i = 0; i < sizeof(playable_exts) / sizeof(playable_exts[0]); i++) {
		if (has_extension(name, playable_exts[i]))
			return true;
	}
	return false;
}

// Directories sort before files, otherwise alphabetical (case-insensitive).
static int entry_cmp(const void *a, const void *b) {
	const browser_entry_t *ea = a;
	const browser_entry_t *eb = b;

	if (ea->is_dir != eb->is_dir) {
		return ea->is_dir ? -1 : 1;
	}

	return strcasecmp(ea->name, eb->name);
}

static void entry_delete_cb(lv_event_t *e) { free(lv_event_get_user_data(e)); }

static void entry_clicked_cb(lv_event_t *e) {
	browser_entry_t *entry = lv_event_get_user_data(e);

	char full_path[sizeof(current_path) + sizeof(entry->name) + 1];
	snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry->name);

	if (entry->is_dir) {
		browser_open_dir(full_path);
	} else {
		player_play_file(full_path);
		switch_screen(player_screen);
	}
}

static void up_btn_cb(lv_event_t *e) {
	(void)e;

	if (strcmp(current_path, root_path) == 0) {
		return; // already at root
	}

	char parent[512];
	strncpy(parent, current_path, sizeof(parent) - 1);
	parent[sizeof(parent) - 1] = '\0';

	char *slash = strrchr(parent, '/');
	if (slash && slash != parent) {
		*slash = '\0';
	}

	// Don't allow navigating above the SD card root.
	if (strncmp(parent, root_path, strlen(root_path)) != 0) {
		strncpy(parent, root_path, sizeof(parent) - 1);
		parent[sizeof(parent) - 1] = '\0';
	}

	browser_open_dir(parent);
}

static void browser_open_dir(const char *path) {
	strncpy(current_path, path, sizeof(current_path) - 1);
	current_path[sizeof(current_path) - 1] = '\0';

	lv_label_set_text(path_label, current_path);
	lv_obj_clean(file_list);

	// add the directory back button, if not in base directory
	if (strcmp(current_path, root_path) != 0) {
		lv_obj_t *up_btn = lv_list_add_button(file_list, LV_SYMBOL_LEFT, "..");
		lv_obj_add_style(up_btn, &style_list_btn, 0);
		lv_obj_add_style(up_btn, &style_list_btn_pressed, LV_STATE_PRESSED);
		lv_obj_add_event_cb(up_btn, up_btn_cb, LV_EVENT_CLICKED, NULL);
	}

	// add error text if not able to open directory
	DIR *dir = opendir(current_path);
	if (!dir) {
		lv_obj_t *lbl = lv_list_add_text(file_list, "Unable to open directory");
		lv_obj_add_style(lbl, &style_list_error, 0);

		return;
	}

	browser_entry_t *entries = NULL;
	size_t count = 0, capacity = 0;

	struct dirent *de;
	while ((de = readdir(dir)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
			continue;
		}

		char entry_path[sizeof(current_path) + sizeof(de->d_name) + 1];
		snprintf(entry_path, sizeof(entry_path), "%s/%s", current_path, de->d_name);

		struct stat st;
		if (stat(entry_path, &st) != 0) {
			continue;
		}

		bool is_dir = S_ISDIR(st.st_mode);
		if (!is_dir && !is_playable_file(de->d_name)) {
			continue; // hide files we can't play
		}

		if (count == capacity) {
			capacity = capacity ? capacity * 2 : 32;
			entries = realloc(entries, capacity * sizeof(*entries));
		}

		strncpy(entries[count].name, de->d_name, sizeof(entries[count].name) - 1);
		entries[count].name[sizeof(entries[count].name) - 1] = '\0';
		entries[count].is_dir = is_dir;
		count++;
	}
	closedir(dir);

	qsort(entries, count, sizeof(*entries), entry_cmp);

	if (count == 0) {
		lv_obj_t *lbl = lv_list_add_text(file_list, "No audio files found");
		lv_obj_add_style(lbl, &style_list_error, 0);
	}

	for (size_t i = 0; i < count; i++) {
		browser_entry_t *heap_entry = malloc(sizeof(browser_entry_t));
		*heap_entry = entries[i];

		const char *icon = heap_entry->is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_AUDIO;
		lv_obj_t *btn = lv_list_add_button(file_list, icon, heap_entry->name);
		lv_obj_add_style(btn, &style_list_btn, 0);
		lv_obj_add_style(btn, &style_list_btn_pressed, LV_STATE_PRESSED);
		lv_obj_add_event_cb(btn, entry_clicked_cb, LV_EVENT_CLICKED, heap_entry);
		lv_obj_add_event_cb(btn, entry_delete_cb, LV_EVENT_DELETE, heap_entry);
	}

	free(entries);
}

// initialize the file browser
void browser_init(gui_config_t *cfg) {
	// initialize list styles
	init_list_styles();

	// TODO: is this necessary? could we just use the string from the config directly?
	strncpy(root_path, cfg->sd_root_path, sizeof(root_path) - 1);
	root_path[sizeof(root_path) - 1] = '\0';

	// Screen Style
	lv_obj_set_style_bg_color(browser_screen, lv_color_make(50, 50, 62), 0);

	// Page Container
	lv_obj_t *screen_container = lv_obj_create(browser_screen);
	lv_obj_set_size(screen_container, lv_pct(100), cfg->screen_height - cfg->top_bar_height);
	lv_obj_align(screen_container, LV_ALIGN_TOP_LEFT, 0, cfg->top_bar_height);
	lv_obj_set_style_bg_opa(screen_container, 0, 0);
	lv_obj_set_style_border_width(screen_container, 0, 0);
	lv_obj_set_style_pad_hor(screen_container, 0, 0);
	lv_obj_set_style_radius(screen_container, 0, 0);
	lv_obj_set_flex_flow(screen_container, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(screen_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_gap(screen_container, cfg->padding, 0);

	// Screen Title
	lv_obj_t *title = lv_label_create(screen_container);
	lv_label_set_text(title, "Browse Files");
	// lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_text_color(title, lv_color_white(), 0);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);

	// Current path breadcrumb
	path_label = lv_label_create(screen_container);
	lv_label_set_long_mode(path_label, LV_LABEL_LONG_DOT);
	lv_obj_set_width(path_label, lv_pct(50));
	// lv_obj_align(path_label, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_set_style_text_color(path_label, lv_color_make(160, 160, 160), 0);
	lv_obj_set_style_text_align(path_label, LV_TEXT_ALIGN_CENTER, 0);

	// TODO: set scroll bar area width to be padding, for consistency
	// File/directory list
	file_list = lv_list_create(screen_container);
	lv_obj_set_width(file_list, lv_pct(100));
	lv_obj_set_style_bg_opa(file_list, 0, 0);
	lv_obj_set_style_border_width(file_list, 0, 0);
	lv_obj_set_style_radius(file_list, 0, 0);
	lv_obj_set_flex_grow(file_list, 1);

	browser_open_dir(root_path);
}
