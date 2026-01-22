#ifndef A1_VIDEO_H
#define A1_VIDEO_H

void video_clear(void);
void video_draw_box(int style, int attr, char *title, int separate, int x, int y, int w, int h);

/* Ok, I'm not the author of this madness but it looks like it works like this:
x and y are the coordinates,
attr is the "style"
text is the text to be displayed
field is the length of the field to write. If shorter than the text length, the text
will be truncated. If longer, padding spaces will be added to erase the remaining field
*/
void video_draw_text(int x, int y, int attr, char *text, int field);
void video_draw_text2(int x, int y, int attr, char *text, int field);

void get_partial_scroll_limits(short * const start, short * const end);
unsigned short set_partial_scroll_limits(const short start, const short end);
int video_get_key(void);

extern void video_repeat_char2(int x, int y, int repcnt, int repchar, int attr);

#endif /* A1_VIDEO_H */
