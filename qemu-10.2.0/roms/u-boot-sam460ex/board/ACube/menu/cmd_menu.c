#include <common.h>
#include <command.h>
#include <asm/processor.h>
#include "menu.h"
#include "bootselect_menu.h"
#include "creation.h"
#include "bios_menu.h"
#include "../common/vesa_video.h"
#include "../common/nanojpeg.h"
#include "../common/logo96.h"
#include "../../../drivers/bios_emulator/vesa.h"

DECLARE_GLOBAL_DATA_PTR;

#define MAIN_MENU_NAME "U-BOOT Preferences Menu"
#define BOOT_MENU_NAME "U-Boot Boot Select"

#define XOFFSET 7
#define YOFFSET 6

static form_t *root = 0;
static form_t *bootselect = 0;

static void establish_menu_settings(void);
static int do_menu_countdown(void);

extern struct FrameBufferInfo *fbi;
extern int console_col;
extern int console_row;
extern int scrolled;
extern int isgadget;

extern int atoi(const char *string);
extern void video_puts (const char *s);
extern void video_putc (const char c);
extern void video_clear(void);
extern void video_clear_attr(void);
extern void video_set_color(unsigned char attr);

int show_and_do_boot_select(void)
{
	unsigned long delta = TEXT_BASE - gd->relocaddr;

	if (gd->flags & GD_FLG_SILENT)
	{
		console_row = 1;
		console_col = 0;
		gd->flags &= ~GD_FLG_SILENT;
	}

	video_clear();

	menu_item_relocate(delta);

	bootselect = new_form(BOOT_MENU_NAME);
	if (!bootselect) return 0;

	make_bootselect_menu(bootselect);
	menu_set_form(bootselect);
	menu_form_switch_menu(bootselect, BOOT_MENU_NAME);
	menu_do(false);

   	return return_value;
}

void show_and_do_bios_menu(void)
{
	unsigned long delta = TEXT_BASE - gd->relocaddr;

	menu_item_relocate(delta);

	root = new_form("U-BOOT Setup Menu");
	if (!root) return;

	make_menus(root);
	menu_set_form(root);
	menu_form_switch_menu(root, MAIN_MENU_NAME);
	menu_do(true);
	console_row = 1;
}

static int do_menu_countdown(void)
{
	int ii, bootdelay;
	int current;
	char *s, c;

	bootdelay = 0;
	s = GETENV("menuboot_delay");

	if (s) bootdelay = atoi(s);

	if (bootdelay == 0)
	{
		if (tstc() != -1) return 1;
		else return 0;
	}

	if (gd->flags & GD_FLG_SILENT)
	{
		console_row = CONSOLE_ROW_START - 1;
		console_col = 29;
		video_puts("SPACE = Menu, ESC or Q = Prompt, Enter = Boot source");
		console_row = CONSOLE_ROW_START;
		console_col = 29;
		video_puts("                                                    ");
		console_col = 29;
		video_puts("Countdown");
		video_putc(0x20);
		video_putc(0xfd);
	}
	else
	{
		scrolled = 0;
		isgadget = 0;
		puts("Press SPACE for menu, ESC or Q for prompt, Enter for boot source\n");
		puts("Countdown.... ");
	}

	if (bootdelay > 10) bootdelay = 10;

	current = 0;
	int step = 3300 / bootdelay;

	while (current < bootdelay*100)
	{
		if (gd->flags & GD_FLG_SILENT)
		{
			console_col = 40;
			for (ii = 0; ii < step*(current+1)/10000; ii++) video_putc(0xfe);
		}
		else
			printf("\b\b\b%2d ", (bootdelay*100 - current)/100);

		for (ii = 0; ii < 10; ii++)
		{
			if (tstc())
			{
				c = getc();

				if ((c == 5) || (c == 113)) return -10; // ESC or Q
				if (c == 13) return show_and_do_boot_select(); // ENTER
				if (c == 32) return 1; // SPACE
			}

			udelay(500);
		}

		current++;
	}

	if (gd->flags & GD_FLG_SILENT)
	{
		console_col = 39;
		video_puts("                                    ");

		console_col = 39;
		video_putc(0xfd);
		for (ii = 0; ii < 33; ii++) video_putc(0xfe);
		video_putc(0xfc);

		console_row = CONSOLE_ROW_START - 1;
		console_col = 29;
		video_puts("                                                    ");

		console_row = CONSOLE_ROW_START + 4;
	}

	return 0;
}


void plot_logo(void)
{
	// load logo image -------------------------------------------------------
	unsigned int xx, yy, ww = 0, hh = 0;

	if (fbi)
	{
		njInit();
		if (njDecode(logo96, sizeof(logo96)) == NJ_OK)
		{
			unsigned char* buf = njGetImage();
			ww = njGetWidth();
			hh = njGetHeight();

			if (ww && hh)
			{
				for (yy = 0; yy < hh; yy++)
				{
					for (xx = 0; xx < ww; xx++)
					{
						putpixel(fbi->XSize - ww - XOFFSET + xx, yy + YOFFSET, *buf);

						buf += 3;
					}
				}
			}
		}
		njDone();
	}
}

int do_menu( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[] )
{
	int ret = 0;

	// only if there is an active vga -------------------------------

	if (fbi->BaseAddress)
	{
		if (flag==0)
		{
			video_set_color(0);
			plot_logo();
	  		show_and_do_bios_menu();
			puts("\n");
			video_set_color(0);
	  		return 0;
		}
		else
		{
			ret = do_menu_countdown();

			if (ret == -10) // ESC or Q
			{
				puts(" break...\n");
				setenv("menuboot_cmd", " ");

				if (gd->flags & GD_FLG_SILENT)
				{
					console_row = 1;
					console_col = 0;
				}

				if (gd->flags & GD_FLG_SILENT)
					gd->flags &= ~GD_FLG_SILENT;

				video_clear();

				return 0;
			}

			if (ret == 1) // SPACE
			{
				if (gd->flags & GD_FLG_SILENT)
					gd->flags &= ~GD_FLG_SILENT;

				video_set_color(0);
		  		show_and_do_bios_menu();
			}

			video_clear_attr();
			establish_menu_settings();
			puts("\n");
			video_set_color(0);
			console_row = 20;

			return 0;
		}
	}
	else return -1;
}

/*
 * This routine establishes all settings from the menu that aren't already done
 * by the standard setup.
 */

static void establish_menu_settings(void)
{
	boot_establish();
}

U_BOOT_CMD(
	   menu,	1,	1,	 do_menu,
	   "Show preferences menu",
	   "Show the preferences menu that is used to boot an OS\n"
	   );
