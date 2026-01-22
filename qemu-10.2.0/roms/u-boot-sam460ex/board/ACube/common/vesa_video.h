#ifndef VESA_VIDEO_H
#define VESA_VIDEO_H

#define VIDEO_VISIBLE_COLS	(fbi->XSize)
#define VIDEO_VISIBLE_ROWS	(fbi->YSize)
#define VIDEO_PIXEL_SIZE	(fbi->BitsPerPixel / 8)
#define VIDEO_DATA_FORMAT	(fbi->Index)
#define VIDEO_FB_ADRS		(fbi->BaseAddress)

#define VIDEO_COLS			VIDEO_VISIBLE_COLS
#define VIDEO_ROWS			VIDEO_VISIBLE_ROWS
#define VIDEO_SIZE			(VIDEO_ROWS*VIDEO_COLS*VIDEO_PIXEL_SIZE)
#define VIDEO_PIX_BLOCKS	(VIDEO_SIZE >> 2)
#define VIDEO_LINE_LEN		(VIDEO_COLS*VIDEO_PIXEL_SIZE)
#define VIDEO_BURST_LEN		(VIDEO_COLS/8)

#define CONSOLE_ROWS		(VIDEO_ROWS / VIDEO_FONT_HEIGHT)
#define CONSOLE_COLS		(VIDEO_COLS / VIDEO_FONT_WIDTH)
#define CONSOLE_ROW_SIZE	(VIDEO_FONT_HEIGHT * VIDEO_LINE_LEN)
#define CONSOLE_ROW_START	28
#define CONSOLE_ROW_FIRST	(video_console_address)
#define CONSOLE_ROW_SECOND	(video_console_address + CONSOLE_ROW_SIZE)
#define CONSOLE_ROW_LAST	(video_console_address + CONSOLE_SIZE - CONSOLE_ROW_SIZE)
#define CONSOLE_SIZE		(CONSOLE_ROW_SIZE * CONSOLE_ROWS)
#define CONSOLE_SCROLL_SIZE	(CONSOLE_SIZE - CONSOLE_ROW_SIZE)

#define CURSOR_ON
#define CURSOR_OFF video_putchar(console_col * VIDEO_FONT_WIDTH,\
				 console_row * VIDEO_FONT_HEIGHT, ' ');
#define CURSOR_SET video_set_cursor();

#define SWAP16(x)	 ((((x) & 0x00ff) << 8) | ( (x) >> 8))
#define SWAP32(x)	 ((((x) & 0x000000ff) << 24) | (((x) & 0x0000ff00) << 8)|\
			  (((x) & 0x00ff0000) >>  8) | (((x) & 0xff000000) >> 24) )
#define SHORTSWAP32(x)	 ((((x) & 0x000000ff) <<  8) | (((x) & 0x0000ff00) >> 8)|\
			  (((x) & 0x00ff0000) <<  8) | (((x) & 0xff000000) >> 8) )

#define SCALE31(x) ((x * 0x1f) / 0xff)
#define SCALE63(x) ((x * 0x3f) / 0xff)
#define RGB16(y) (SCALE31(y) << 11 | SCALE63(y) << 5 | SCALE31(y))

#define PIXEL_BLACK    0x00
#define PIXEL_WHITE    0xFF
#define PIXEL_GREY     0xC0
#define PIXEL_DARKGREY 0x60

#define SAM_CONSOLE_BG_COL 0xC0
#define SAM_CONSOLE_FG_COL 0x00

#define GBOX_START_X 130
#define GBOX_START_Y 40
#define GBOX_END_X 510
#define GBOX_END_Y 255

#define IBOX_START_X 130
#define IBOX_START_Y 262
#define IBOX_END_X 510
#define IBOX_END_Y 302

extern void putpixel(int xx, int yy, int color);
extern void video_puts (const char *s);
extern void video_putc (const char c);
extern int video_get_key(void);
extern int drv_video_init (void);
extern void gfxbox(int x1, int y1, int x2, int y2, int filled);
extern void video_minidrawchars (int xx, int yy, char *s, int count);
extern void video_draw_text(int x, int y, int attr, char *text, int field);
extern void video_clear_box(int x, int y, int w, int h, int clearchar, int attr);
extern void video_draw_box(int style, int attr, char *title, int separate,
	int xp, int yp, int ww, int hh);

#endif
