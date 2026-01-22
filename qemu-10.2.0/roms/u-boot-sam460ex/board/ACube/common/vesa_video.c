/*
 * modified 2008-2024 by
 * Max Tretene, ACube Systems Srl. mtretene@acube-systems.com.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <stdio_dev.h>
#include <part.h>
#include <malloc.h>
#include <asm/io.h>
#include <video_fb.h>
#include <video_font.h>
#include <video_font_5x8.h>
#include "radeon.h"
#include "vesa_video.h"
#include "../menu/menu.h"
#include "../../../drivers/bios_emulator/vesa.h"

#undef DEBUG

#ifdef DEBUG
#define PRINTF(format, args...) _printf(format , ## args)
#else
#define PRINTF(format, argc...)
#endif

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_VIDEO_SM502
extern unsigned char ACTIVATESM502;
extern unsigned char SM502INIT;
#endif

#ifdef CONFIG_VIDEO_PERMEDIA2
extern unsigned char PM2INIT;
#endif

extern int onbus;
extern u32 mmio_base_phys;
extern struct FrameBufferInfo *fbi;
extern u8 *logo_buf8;
extern u16 *logo_buf16;

#define OUTREG(addr,val)	writel(val, mmio_base_phys + addr)

const int video_font_draw_table8[] = {
		0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff,
		0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
		0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff,
		0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff };

const int video_font_draw_table8a[] = {
		0xffffffff, 0xffffff00, 0xffff00ff, 0xffff0000,
		0xff00ffff, 0xff00ff00, 0xff0000ff, 0xff000000,
		0x00ffffff, 0x00ffff00, 0x00ff00ff, 0x00ff0000,
		0x0000ffff, 0x0000ff00, 0x000000ff, 0x00000000 };

static const int video_font_draw_table16[] = {
	    0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff };

static const int video_font_draw_table16a[] = {
	    0xffffffff, 0xffff0000, 0x0000ffff, 0x00000000 };

/*
static char video_single_box[] =
{
	218, 196, 191,
	179,	  179,
	192, 196, 217
};

static char video_single_title[] =
{
	195, 196, 180, 180, 195
};
*/

void *video_fb_address;			/* frame buffer address */
void *video_console_address;	/* console buffer start address */

int console_col; /* cursor col */
int console_row; /* cursor row */

int scrolled = 0;
int isgadget = 0;

u32 eorx, fgx, bgx;  /* color pats */

#define DC_LUT_RW_SELECT			0x6480
#define DC_LUT_RW_MODE				0x6484
#define DC_LUT_RW_INDEX				0x6488
#define DC_LUT_30_COLOR				0x6494
#define DC_LUT_WRITE_EN_MASK		0x649C
#define DC_LUTA_CONTROL				0x64C0
#define DC_LUTA_BLACK_OFFSET_BLUE	0x64C4
#define DC_LUTA_BLACK_OFFSET_GREEN	0x64C8
#define DC_LUTA_BLACK_OFFSET_RED	0x64CC
#define DC_LUTA_WHITE_OFFSET_BLUE	0x64D0
#define DC_LUTA_WHITE_OFFSET_GREEN	0x64D4
#define DC_LUTA_WHITE_OFFSET_RED	0x64D8

//***************************************************************************

void video_set_lut2 (unsigned int index,	/* color number */
		   unsigned int rr,	/* red */
		   unsigned int gg,	/* green */
		   unsigned int bb	/* blue */
		   )
{
#ifdef CONFIG_VIDEO_SM502
	if (ACTIVATESM502 && SM502INIT)
	{
		// already done
	}
	else
#endif
#ifdef CONFIG_VIDEO_PERMEDIA2
	if (PM2INIT)
	{
		// already done
	}
	else
#endif
	{
		if (onbus >= 2)
		{
			/* RadeonHD on PCI-E */
			OUTREG(DC_LUT_30_COLOR, (rr << 20) | (gg << 10) | bb);
		}
		else
		{
			/* Radeon or RadeonHD on PCI */
			OUTREG(PALETTE_INDEX, index | index << 16);
			OUTREG(PALETTE_DATA, (rr << 16) | (gg << 8) | bb);
		}
	}
}

void memsetl (int *p, int c, int v)
{
	while (c--)
		*(p++) = v;
}

static void memcpyl (int *d, int *s, int c)
{
	while (c--)
		*(d++) = *(s++);
}

/******************************************************************************/

static void console_scrollup (void)
{
	/* copy up rows ignoring the first one */

	memcpyl (CONSOLE_ROW_FIRST, CONSOLE_ROW_SECOND, (CONSOLE_SCROLL_SIZE >> 2));

	memsetl (CONSOLE_ROW_LAST, (CONSOLE_ROW_SIZE >> 2), SHORTSWAP32(bgx));

	scrolled = 1;
}

static void video_drawchars (int xx, int yy, char *s, int count)
{
	u8 *cdat, *dest, *dest0, *src, *src0;
	int rows, offset, c;

	if (count <= 0) return;

	offset = yy * VIDEO_LINE_LEN + xx * VIDEO_PIXEL_SIZE;
	dest0 = video_fb_address + offset;

	switch (VIDEO_DATA_FORMAT) {
	case GDF__8BIT_INDEX:
	case GDF__8BIT_332RGB:
		src0 = logo_buf8 + offset;
		while (count--)
		{
			c = *s;
			cdat = video_fontdata + c * VIDEO_FONT_HEIGHT;

			if (scrolled)
			{
				for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
					 rows--;
					 dest += VIDEO_LINE_LEN)
				{
					u8 bits = *cdat++;

					((u32 *) dest)[0] = (video_font_draw_table8[bits >> 4] & eorx) ^ bgx;
					((u16 *) dest)[2] = (video_font_draw_table8[(bits & 0x0c) >> 2] & eorx) ^ bgx;

				}
				dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			}
			else if (isgadget)
			{
				for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
					 rows--;
					 dest += VIDEO_LINE_LEN)
				{
					u8 bits = *cdat++;

					((u32 *) dest)[0] = 0xC0C0C0C0 & (video_font_draw_table8a[bits >> 4]);
					((u16 *) dest)[2] = 0xC0C0 & (video_font_draw_table8a[(bits & 0x0c) >> 2]);
				}
				dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			}
			else
			{
				for (rows = VIDEO_FONT_HEIGHT, dest = dest0, src = src0;
					 rows--;
					 dest += VIDEO_LINE_LEN, src += VIDEO_LINE_LEN)
				{
					u8 bits = *cdat++;

					((u32 *) dest)[0] = ((u32 *) src)[0] & (video_font_draw_table8a[bits >> 4]);
					((u16 *) dest)[2] = ((u32 *) src)[1] & (video_font_draw_table8a[(bits & 0x0c) >> 2]);
				}
				dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
				src0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			}

			s++;
		}
		break;
	case GDF_16BIT_565RGB:
		src0 = (u8 *)logo_buf16 + offset;
		while (count--)
		{
			c = *s;
			cdat = video_fontdata + c * VIDEO_FONT_HEIGHT;

			if (scrolled)
			{
				for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
					 rows--;
					 dest += VIDEO_LINE_LEN)
				{
					u8 bits = *cdat++;

					((u32 *) dest)[0] = SHORTSWAP32 (video_font_draw_table16a [bits >> 6] & bgx);
					((u32 *) dest)[1] = SHORTSWAP32 (video_font_draw_table16a [bits >> 4 & 3] & bgx);
					((u32 *) dest)[2] = SHORTSWAP32 (video_font_draw_table16a [bits >> 2 & 3] & bgx);
				}
				dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			}
			else if (isgadget)
			{
				u32 gadget_bgx = ((RGB16(PIXEL_GREY) << 16) | (RGB16(PIXEL_GREY)));

				for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
					 rows--;
					 dest += VIDEO_LINE_LEN)
				{
					u8 bits = *cdat++;

					((u32 *) dest)[0] = SHORTSWAP32 (video_font_draw_table16a [bits >> 6] & gadget_bgx);
					((u32 *) dest)[1] = SHORTSWAP32 (video_font_draw_table16a [bits >> 4 & 3] & gadget_bgx);
					((u32 *) dest)[2] = SHORTSWAP32 (video_font_draw_table16a [bits >> 2 & 3] & gadget_bgx);
				}
				dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			}
			else
			{
				for (rows = VIDEO_FONT_HEIGHT, dest = dest0, src = src0;
					 rows--;
					 dest += VIDEO_LINE_LEN, src += VIDEO_LINE_LEN)
				{
					u8 bits = *cdat++;

					((u32 *) dest)[0] = (video_font_draw_table16a [bits >> 6]) & ((u32 *) src)[0];
					((u32 *) dest)[1] = (video_font_draw_table16a [bits >> 4 & 3]) & ((u32 *) src)[1];
					((u32 *) dest)[2] = (video_font_draw_table16a [bits >> 2 & 3]) & ((u32 *) src)[2];
				}
				dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
				src0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
			}

			s++;
		}
		break;
	}
}

void video_minidrawchars (int xx, int yy, char *s, int count)
{
	u8 *cdat, *dest, *dest0, *src, *src0;
	int rows, offset, c;

	if (count <= 0) return;

	offset = yy * VIDEO_LINE_LEN + xx * VIDEO_PIXEL_SIZE;
	dest0 = video_fb_address + offset;

	switch (VIDEO_DATA_FORMAT) {
		case GDF__8BIT_INDEX:
		case GDF__8BIT_332RGB:
			src0 = logo_buf8 + offset;
			while (count--)
			{
				c = *s;
				cdat = video_minifontdata + c * VIDEO_MINI_FONT_HEIGHT;

				for (rows = VIDEO_MINI_FONT_HEIGHT, dest = dest0, src = src0;
					 rows--;
					 dest += VIDEO_LINE_LEN, src += VIDEO_LINE_LEN)
				{
					u8 bits = *cdat++;

					((u32 *) dest)[0] = ((u32 *) src)[0] & (video_font_draw_table8a[bits >> 4]);
				}
				dest0 += VIDEO_MINI_FONT_WIDTH * VIDEO_PIXEL_SIZE;
				src0 += VIDEO_MINI_FONT_WIDTH * VIDEO_PIXEL_SIZE;

				s++;
			}
			break;

		case GDF_16BIT_565RGB:
			src0 = (u8 *)logo_buf16 + offset;
			while (count--)
			{
				c = *s;
				cdat = video_minifontdata + c * VIDEO_MINI_FONT_HEIGHT;

				for (rows = VIDEO_MINI_FONT_HEIGHT, dest = dest0, src = src0;
					 rows--;
					 dest += VIDEO_LINE_LEN, src += VIDEO_LINE_LEN)
				{
					u8 bits = *cdat++;

					((u32 *) dest)[0] = (video_font_draw_table16a [bits >> 6]) & ((u32 *) src)[0];
					((u32 *) dest)[1] = (video_font_draw_table16a [bits >> 4 & 3]) & ((u32 *) src)[1];
					((u32 *) dest)[2] = (video_font_draw_table16a [bits >> 2 & 3]) & ((u32 *) src)[2];
				}
				dest0 += VIDEO_MINI_FONT_WIDTH * VIDEO_PIXEL_SIZE;
				src0 += VIDEO_MINI_FONT_WIDTH * VIDEO_PIXEL_SIZE;

				s++;
			}
			break;
			break;
	}
}

static void video_putchar (int xx, int yy, char c)
{
	video_drawchars (xx, yy, &c, 1);
}

static void console_back (void)
{
	CURSOR_OFF
	console_col--;

	if (console_col < 0)
	{
		console_col = CONSOLE_COLS - 1;
		console_row--;
		if (console_row < 0)
			console_row = 0;
	}
	video_putchar (console_col * VIDEO_FONT_WIDTH,
			       console_row * VIDEO_FONT_HEIGHT,
			       ' ');
}

static void console_newline (void)
{
	CURSOR_OFF
	console_row++;
	console_col = 0;

	/* Check if we need to scroll the terminal */
	if (console_row >= CONSOLE_ROWS)
	{
		/* Scroll everything up */
		console_scrollup ();

		/* Decrement row number */
		console_row--;
	}
}

static void video_set_cursor (void)
{
	/* swap drawing colors */
	eorx = fgx;
	fgx = bgx;
	bgx = eorx;
	eorx = fgx ^ bgx;
	/* draw cursor */
	CURSOR_OFF
	/* restore drawing colors */
	eorx = fgx;
	fgx = bgx;
	bgx = eorx;
	eorx = fgx ^ bgx;
}

unsigned short set_partial_scroll_limits(const short start, const short end)
{
/*
	if(!PARTIAL_SCROLL_ACTIVE(start, end))
	{
		// Deactivates the partial scroll
		partial_scroll_start=-1;
		partial_scroll_end=-1;

		return 1;
	}

	if(	(start < end) &&
		((start >= 0) && (start <= video_numrows-1)) &&
		((end >= 1) && (end <= video_numrows)))
	{
		partial_scroll_start = start;
		partial_scroll_end = end;

		cursor_row = start;
		cursor_col = 0;
		video_set_cursor(start,0);

		return 1;
	}
*/
	return 0;
}

void get_partial_scroll_limits(short * const start, short * const end)
{
/*
	*start = partial_scroll_start;
	*end = partial_scroll_end;
*/
}

// used in menu
int video_get_key(void)
{
	int c = getc();

	switch(c)
	{
		case 0x1B:
			return KEY_ABORT;
		case 0x0D:
			return KEY_ACTIVATE;
		case 0x08:
			return KEY_DELETE;
	}

	return c;
}

void video_clear(void)
{
	if ( ! (gd->flags & GD_FLG_SILENT))
	{
		u32 color = SAM_CONSOLE_BG_COL;

		switch (VIDEO_DATA_FORMAT)
		{
			case GDF__8BIT_INDEX:
			case GDF__8BIT_332RGB:
				color = bgx;
				break;

			case GDF_16BIT_565RGB:
				color = SHORTSWAP32(bgx);
				break;
		}

		memsetl (CONSOLE_ROW_FIRST, CONSOLE_SIZE >> 2, color);
		scrolled = 1;
	}
}

void video_set_color(unsigned char attr)
{
	if ( ! (gd->flags & GD_FLG_SILENT))
	{
		u32 color = SAM_CONSOLE_BG_COL;

		switch (VIDEO_DATA_FORMAT)
		{
			case GDF__8BIT_INDEX:
			case GDF__8BIT_332RGB:
				color = bgx;
				break;

			case GDF_16BIT_565RGB:
				color = SHORTSWAP32(bgx);
				break;
		}

		memsetl (CONSOLE_ROW_FIRST, CONSOLE_SIZE >> 2, color);
		scrolled = 1;
	}
}

static void video_drawchars_color (int xx, int yy, char *s, int count, int attr)
{
	u8 *cdat, *dest, *dest0;
	u32 oldfgx, oldbgx;
	int rows, offset, c;

	offset = yy * VIDEO_LINE_LEN + xx * VIDEO_PIXEL_SIZE;
	dest0 = video_fb_address + offset;

	/* change drawing colors */
	oldfgx = fgx;
	oldbgx = bgx;

	switch (VIDEO_DATA_FORMAT)
	{
		case GDF__8BIT_INDEX:
		case GDF__8BIT_332RGB:
			switch (attr)
			{
				case 0:
				case 1:
				case 3:
				case 4: // Black on Light Gray
					bgx = (SAM_CONSOLE_BG_COL << 24) | (SAM_CONSOLE_BG_COL << 16) | (SAM_CONSOLE_BG_COL << 8) | SAM_CONSOLE_BG_COL;
					fgx = 0x00000000;
					break;
				case 2: // Light Gray on Black
					bgx = 0x00000000;
					fgx = (SAM_CONSOLE_BG_COL << 24) | (SAM_CONSOLE_BG_COL << 16) | (SAM_CONSOLE_BG_COL << 8) | SAM_CONSOLE_BG_COL;
					break;
			}

			eorx = fgx ^ bgx;

			while (count--)
			{
				c = *s;
				cdat = video_fontdata + c * VIDEO_FONT_HEIGHT;
				for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
					 rows--;
					 dest += VIDEO_LINE_LEN)
				{
					u8 bits = *cdat++;

					((u32 *) dest)[0] = (video_font_draw_table8[bits >> 4] & eorx) ^ bgx;
					((u16 *) dest)[2] = (video_font_draw_table8[(bits & 0x0c) >> 2] & eorx) ^ bgx;
				}
				dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
				s++;
			}
			break;

		case GDF_16BIT_565RGB:
			switch (attr)
			{
				case 0:
				case 1:
				case 3:
				case 4: // Black on Light Gray
					bgx = (RGB16(SAM_CONSOLE_BG_COL) << 16) | RGB16(SAM_CONSOLE_BG_COL);
					fgx = 0x00000000;
					break;
				case 2: // Light Gray on Black
					bgx = 0x00000000;
					fgx = (RGB16(SAM_CONSOLE_BG_COL) << 16) | RGB16(SAM_CONSOLE_BG_COL);
					break;
			}

			eorx = fgx ^ bgx;

			while (count--)
			{
				c = *s;
				cdat = video_fontdata + c * VIDEO_FONT_HEIGHT;
				for (rows = VIDEO_FONT_HEIGHT, dest = dest0;
					 rows--;
					 dest += VIDEO_LINE_LEN)
				{
					u8 bits = *cdat++;

					((u32 *) dest)[0] = SHORTSWAP32 ((video_font_draw_table16 [bits >> 6] & eorx) ^ bgx);
					((u32 *) dest)[1] = SHORTSWAP32 ((video_font_draw_table16 [bits >> 4 & 3] & eorx) ^ bgx);
					((u32 *) dest)[2] = SHORTSWAP32 ((video_font_draw_table16 [bits >> 2 & 3] & eorx) ^ bgx);
				}
				dest0 += VIDEO_FONT_WIDTH * VIDEO_PIXEL_SIZE;
				s++;
			}
			break;
	}

	/* restore drawing colors */
	fgx = oldfgx;
	bgx = oldbgx;
	eorx = fgx ^ bgx;
}

void video_clear_attr(void)
{
	//video_set_color(0); //current_attr);
}

void video_attr(int which, int color)
{
/*
	if (which > 4)
		return;

	int back = (color & 0x70) >> 4;
	color = color & 0x0f;

	color *= 3;
	back *= 3;

	video_fore[which] = pack_color(vga_color_table[color], vga_color_table[color+1], vga_color_table[color+2]);
	video_back[which] = pack_color(vga_color_table[back], vga_color_table[back+1], vga_color_table[back+2]);
*/
}

void video_clear_box(int x, int y, int w, int h, int clearchar, int attr)
{
	int line, col;
	char c = (char)clearchar;

	for (line=y; line<y+h; line++)
	{
		for (col=x; col<x+w; col++)
		{
			video_drawchars_color(col*VIDEO_FONT_WIDTH,
								  line*VIDEO_FONT_HEIGHT,
								  &c, 1, attr);
		}
	}
}

void video_draw_text(int x, int y, int attr, char *text, int field)
{
	x *= VIDEO_FONT_WIDTH;
	y *= VIDEO_FONT_HEIGHT;

	while (*text)
	{
		video_drawchars_color(x, y, (char *)text, 1, attr);
		x += VIDEO_FONT_WIDTH;

		if (field != -1) field--;
		if (field == 0) break;

		text++;
	}

	while (field > 0)
	{
		video_drawchars_color(x, y, (char *)" ", 1, attr);
		x += VIDEO_FONT_WIDTH;
		field--;
	}
}

void video_repeat_char(int x, int y, int repcnt, int repchar, int attr)
{
	char c = (char)repchar;

	x *= VIDEO_FONT_WIDTH;
	y *= VIDEO_FONT_HEIGHT;

	while (repcnt--)
	{
		video_drawchars_color(x, y, &c, 1, attr);
		x += VIDEO_FONT_WIDTH;
	}
}

// these are used to work with SLB showing the boot process ------------------

void putpixel(int xx, int yy, int color)
{
	int offset;

	switch (VIDEO_DATA_FORMAT)
	{
		case GDF__8BIT_INDEX:
		case GDF__8BIT_332RGB:
		{
			u8 *dest;

			offset = yy * VIDEO_LINE_LEN + xx * VIDEO_PIXEL_SIZE;
			dest = video_fb_address + offset;

			*dest = color;
			break;
		}

		case GDF_16BIT_565RGB:
		{
			u16 *dest16;

			offset = yy * VIDEO_LINE_LEN + xx * VIDEO_PIXEL_SIZE;
			dest16 = video_fb_address + offset;

			*dest16 = SWAP16(RGB16(color));
			break;
		}
	}
}

void hline(int x1, int x2, int yy, int color)
{
	int ii, swap;

	if (x1 > x2)
	{
		swap = x1;
		x1 = x2;
		x2 = swap;
	}

	for (ii=x1; ii<=x2; ii++) putpixel(ii, yy, color);
}

void vline(int xx, int y1, int y2, int color)
{
	int ii, swap;

	if (y1 > y2)
	{
		swap = y1;
		y1 = y2;
		y2 = swap;
	}

	for (ii=y1; ii<=y2; ii++) putpixel(xx, ii, color);
}

void gfxbox(int x1, int y1, int x2, int y2, int filled)
{
	int ii, swap;

	if (y1 > y2)
	{
		swap = y1;
		y1 = y2;
		y2 = swap;
	}

	if (filled)
	{
		for (ii=y1;ii<y2-1;ii++)
			hline(x1+1, x2-1, ii, PIXEL_GREY);
	}
	else
	{
		vline(x1+2, y1, y2, PIXEL_GREY);
		hline(x1, x2, y1+2, PIXEL_GREY);
		hline(x1, x2, y1+3, PIXEL_GREY);

		vline(x2-1, y1, y2, PIXEL_GREY);
		vline(x2-2, y1, y2, PIXEL_GREY);
		vline(x2-3, y1, y2, PIXEL_GREY);
		vline(x2+2, y1+1, y2+1, PIXEL_GREY);

		hline(x1, x2, y2-4, PIXEL_GREY);
		hline(x1, x2, y2-3, PIXEL_GREY);
		hline(x1, x2, y2-2, PIXEL_GREY);
		hline(x1, x2, y2-1, PIXEL_GREY);
		hline(x1, x2, y2+2, PIXEL_GREY);
	}

	hline(x1, x2, y1, PIXEL_BLACK);
	hline(x1, x2, y2, PIXEL_BLACK);
	vline(x1, y1, y2, PIXEL_BLACK);
	vline(x2, y1, y2, PIXEL_BLACK);

	hline(x1+1, x2+1, y1+1, PIXEL_WHITE);
	hline(x1+1, x2+1, y2+1, PIXEL_WHITE);
	vline(x1+1, y1+1, y2+1, PIXEL_WHITE);
	vline(x2+1, y1+1, y2+1, PIXEL_WHITE);
}

void gadget(int x1, int y1, int x2, int y2, int recessed)
{
	int col1, col2;

	if (recessed)
	{
		col1 = PIXEL_BLACK;
		col2 = PIXEL_WHITE;
	}
	else
	{
		col1 = PIXEL_WHITE;
		col2 = PIXEL_BLACK;
	}

	hline(x1, x2, y1, col1);
	hline(x1, x2, y2, col2);
	vline(x1, y1, y2, col1);
	vline(x2, y1, y2, col2);

	hline(x1+1, x2-1, y1+1, col1);
	hline(x1+1, x2-1, y2-1, col2);
	vline(x1+1, y1+1, y2-1, col1);
	vline(x2-1, y1+1, y2-1, col2);
}

void video_draw_text2(int x, int y, int attr, char *text, int field)
{
	if (gd->flags & GD_FLG_SILENT)
	{
		static int firsttime = 0;
		char tmp[55] = { 32 };

		isgadget = 1;

		if (firsttime == 0)
		{
			gfxbox(GBOX_START_X, GBOX_START_Y, GBOX_END_X, GBOX_END_Y, 1);
			firsttime = 1;
		}

		// kickstart modules name been loaded... -----------------------------
		if (x==5 && y==9)
		{
			x = 29;
			y = CONSOLE_ROW_START + 1;
			video_drawchars (x*VIDEO_FONT_WIDTH, (y-2)*VIDEO_FONT_HEIGHT, (char *)tmp, sizeof(tmp));
			video_drawchars (x*VIDEO_FONT_WIDTH, (y-2)*VIDEO_FONT_HEIGHT, text, strlen(text));
			return;
		}

		// configuration names -----------------------------------------------
		if (strncmp(text,"Booting selected", 16) != 0)
		{
			if (strlen(text) > 0)
			{
				int yy = 25*(y-6);

				if (yy >= GBOX_END_Y - 25) // too many configs to fit on screen
					return;

				if (strncmp(text,"->",2) == 0)
					gadget(GBOX_START_X+20, yy, GBOX_END_X-20, 20+yy, 1);
				else
					gadget(GBOX_START_X+20, yy, GBOX_END_X-20, 20+yy, 0);

				int len = strlen(text) - 2;
				if (len > 53) len = 53;

				video_drawchars (GBOX_START_X+25, yy+VIDEO_FONT_HEIGHT/2, text+2, len);
			}
		}
		else
		{
			video_drawchars (29*VIDEO_FONT_WIDTH, 27*VIDEO_FONT_HEIGHT, text, strlen(text));
			video_drawchars (29*VIDEO_FONT_WIDTH, 28*VIDEO_FONT_HEIGHT, (char *)tmp, sizeof(tmp));

#undef SCREENSHOT
#ifdef SCREENSHOT
			static ss = 0;
			u16 xx, yy;
			u8 val, *addr;
			extern struct FrameBufferInfo *fbi;

			if (ss == 0)
			{
				if (gd->flags & GD_FLG_SILENT)
				{
					gd->flags &= ~GD_FLG_SILENT;
					setenv("stdout", "serial");

					if (fbi->BaseAddress)
					{
						addr = (u8 *)fbi->BaseAddress;

						for (yy = 0; yy < fbi->YSize; yy++)
						{
							for (xx = 0; xx < fbi->XSize; xx++)
							{
								val = *(addr + xx + yy*fbi->XSize);

								printf("0x%02x,",val);
							}
							printf("\n");
						}
						printf("\n");
					}

					setenv("stdout", "vga");
					gd->flags |= GD_FLG_SILENT;
				}

				ss = 1;
			}
#endif
		}
	}
	else
		video_draw_text(x, y, attr, text, field);
}

void video_repeat_char2(int x, int y, int repcnt, int repchar, int attr)
{
	if (repcnt <= 0) return;

	if (gd->flags & GD_FLG_SILENT)
	{
		char c, d;

		d = (char)0xfd;
		if (repchar == 177) c = (char)0x20;
		else c = (char)0xfe;

		x += 35;
		x *= VIDEO_FONT_WIDTH;
		y += 21;
		y *= VIDEO_FONT_HEIGHT;

		video_drawchars (29*VIDEO_FONT_WIDTH, y, "Loading   ", 10);
		video_drawchars (39*VIDEO_FONT_WIDTH, y, &d, 1);

		--repcnt;
		repcnt /= 2;

		while (repcnt--)
		{
			video_drawchars(x, y, &c, 1);
			x += VIDEO_FONT_WIDTH;
		}
	}
	else
		video_repeat_char(x, y, repcnt, repchar, attr);
}

void video_draw_box(int style, int attr, char *title, int separate,
	int xp, int yp, int ww, int hh)
{
	if (gd->flags & GD_FLG_SILENT) return;

	int x1, x2, y1;
	char blank[75] = { 32 };

	xp *= VIDEO_FONT_WIDTH;
	yp *= VIDEO_FONT_HEIGHT;

	ww *= VIDEO_FONT_WIDTH;
	hh *= VIDEO_FONT_HEIGHT;

	gfxbox(xp+3, yp+5, xp+ww-3, yp+hh-5, 0);

	// Draw title
	if (title)
	{
		char *tmp = NULL;

		if (strncmp(title,"Loading kickstart files",23) == 0)
		{
			tmp = malloc(strlen(title));
			if (tmp)
			{
				strcpy(tmp,"Loading kickstart ");
				char *ret = strstr(title,"named");
				if (ret) strcat(tmp,ret);

				title = tmp;
			}
		}

		if (separate == 0)
		{
			x1 = xp + 2 * VIDEO_FONT_WIDTH;
			y1 = yp + 1;
			video_drawchars_color(x1, y1, (char *)" ", 1, attr);

			x1 += VIDEO_FONT_WIDTH;
			video_drawchars_color(x1, y1, (char *)title, strlen(title), attr);

			x1 += strlen(title) * VIDEO_FONT_WIDTH;
			video_drawchars_color(x1, y1, (char *)" ", 1, attr);
		}
		else
		{
			x1 = xp;
			y1 = yp + 2 * VIDEO_FONT_HEIGHT;
			x2 = x1 + ww;

			hline(xp+3, x2-3, y1+3, PIXEL_BLACK);
			hline(xp+4, x2-3, y1+4, PIXEL_WHITE);
			putpixel(xp+3, y1+4, PIXEL_GREY);

			hline(xp+1, x2-1, y1+5, PIXEL_GREY);
			hline(xp+1, x2-1, y1+6, PIXEL_GREY);

			hline(xp+3, x2-3, y1+7, PIXEL_BLACK);
			hline(xp+4, x2-3, y1+8, PIXEL_WHITE);
			putpixel(x2-2, y1+7, PIXEL_GREY);

			x1 = xp + 2 * VIDEO_FONT_WIDTH;
			y1 = yp + VIDEO_FONT_HEIGHT;

			video_drawchars_color(x1, y1, (char *)blank, sizeof(blank), attr);
			video_drawchars_color(x1, y1, (char *)title, strlen(title), attr);
		}

		if (tmp) free(tmp);
	}
}

void video_putc (const char c)
{
	static char oldc=0;

	switch (c) {
	case '\r':
	//	console_col = 0;
		break;

	case '\n':		/* next line */
		console_newline ();
		break;

	case 9:		/* tab 8 */
		CURSOR_OFF
		console_col |= 0x0008;
		console_col &= ~0x0007;

		if (console_col >= CONSOLE_COLS)
			console_newline ();
		break;

	case 8:		/* backspace */
		console_back ();
		break;

	default:		/* draw the char */
		if (oldc == '\r')
		{
			CURSOR_OFF
			console_col = 0;
		}

		video_putchar (console_col * VIDEO_FONT_WIDTH,
				console_row * VIDEO_FONT_HEIGHT,
				c);
		console_col++;

		/* check for newline */
		if (console_col >= CONSOLE_COLS)
			console_newline ();
	}

	CURSOR_SET

	oldc = c;
}

void video_puts (const char *s)
{
	int count = strlen (s);

	if (count <= 0) return;

	if (gd->flags & GD_FLG_SILENT)
	{
		if ((strncmp(s,"A1 Second-",10) != 0) &&
			(strncmp(s,"SFS and ISO9660",15) != 0) &&
			(strncmp(s,"Linux booting ext",17) != 0) &&
			(strncmp(s,"Booting configuration",21) != 0))
		{
			while (count--)
				video_putc (*s++);
		}
	}
	else
	{
		while (count--)
			video_putc (*s++);
	}
}

static int video_init (void)
{
	int ii;

	video_fb_address = (void *) VIDEO_FB_ADRS;

	/* Init drawing pats */
	switch (VIDEO_DATA_FORMAT)
	{
		case GDF__8BIT_INDEX:
			if (onbus >= 2)
			{
				/* RadeonHD on PCI-E */
				OUTREG(DC_LUTA_CONTROL, 0);

				OUTREG(DC_LUTA_BLACK_OFFSET_BLUE, 0);
				OUTREG(DC_LUTA_BLACK_OFFSET_GREEN, 0);
				OUTREG(DC_LUTA_BLACK_OFFSET_RED, 0);

				OUTREG(DC_LUTA_WHITE_OFFSET_BLUE, 0x0000FFFF);
				OUTREG(DC_LUTA_WHITE_OFFSET_GREEN, 0x0000FFFF);
				OUTREG(DC_LUTA_WHITE_OFFSET_RED, 0x0000FFFF);

				OUTREG(DC_LUT_RW_SELECT, 0);

				OUTREG(DC_LUT_RW_MODE, 0); /* table */
				OUTREG(DC_LUT_WRITE_EN_MASK, 0x0000003F);

				OUTREG(DC_LUT_RW_INDEX, 0);

				for (ii=0;ii<256;ii++)
					video_set_lut2 (ii, ii<<2, ii<<2, ii<<2);
			}
			else
			{
				/* Radeon or RadeonHD on PCI */
				for (ii=0;ii<256;ii++)
					video_set_lut2 (ii, ii, ii, ii);
			}

			bgx = (SAM_CONSOLE_BG_COL << 24) | (SAM_CONSOLE_BG_COL << 16) | (SAM_CONSOLE_BG_COL << 8) | SAM_CONSOLE_BG_COL;
			fgx = 0x00000000;
			break;

		case GDF_16BIT_565RGB:
			bgx = (RGB16(SAM_CONSOLE_BG_COL) << 16) | RGB16(SAM_CONSOLE_BG_COL);
			fgx = 0x00000000;
	}
	eorx = fgx ^ bgx;

	video_console_address = video_fb_address;

	/* Initialize the console */
	console_col = 0;
	console_row = 0;

	return 0;
}

int overwrite_console(void)
{
	return 0;
}

/*****************************************************************************/

int drv_video_init (void)
{
	int skip_dev_init;
	struct stdio_dev console_dev;

	skip_dev_init = 0;

	/* Init video chip - returns with framebuffer cleared */
	if (video_init () == -1)
		skip_dev_init = 1;

	/* Devices VGA and Keyboard will be assigned seperately */
	/* Init vga device */
	if (!skip_dev_init) {
		memset (&console_dev, 0, sizeof (console_dev));
		strcpy (console_dev.name, "vga");
		console_dev.ext = DEV_EXT_VIDEO;	/* Video extensions */
		console_dev.flags = DEV_FLAGS_OUTPUT | DEV_FLAGS_SYSTEM;
		console_dev.putc = video_putc;	/* 'putc' function */
		console_dev.puts = video_puts;	/* 'puts' function */
		console_dev.tstc = NULL;	/* 'tstc' function */
		console_dev.getc = NULL;	/* 'getc' function */

		int error = stdio_register (&console_dev);

		if (error == 0)
		{
			char *s = getenv("stdout");
			if (s && strcmp(s, "vga")==0)
			{
				if (overwrite_console()) return 1;
				error = console_assign(stdout, "vga");
				if (error == 0)
					return 1;
				else
					return error;
			}
			return 1;
		}

		return error;
	}

	/* No console dev available */
	return 0;
}
