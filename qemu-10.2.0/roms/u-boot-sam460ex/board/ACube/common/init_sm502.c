/*
 * (C) Copyright 2009-2024
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
#include <pci.h>
#include <video_fb.h>
#include <sm501.h>

#ifdef CONFIG_VIDEO_SM502

DECLARE_GLOBAL_DATA_PTR;

#undef DEBUG

#ifdef  DEBUG
#define PRINTF(fmt,args...)	 printf (fmt ,##args)
#else
#define PRINTF(fmt,args...)
#endif

unsigned char SM502INIT = 0;
pci_dev_t dev_sm502 = ~0;

void *init_sm502(void)
{
	unsigned short cmd;
	GraphicDevice *sm502 = NULL;

	dev_sm502 = pci_find_device(PCI_VENDOR_SM, PCI_DEVICE_SM501, 0);

	puts("SM502: ");
	if (dev_sm502 != -1)
	{
		puts("found\n");
		PRINTF("calling video_hw_init\n");
		sm502 = (GraphicDevice *)sm502_hw_init();

		SM502INIT = 1;

		// shutdown gfx card
		pci_read_config_word(dev_sm502, PCI_COMMAND, &cmd);
		cmd &= ~(PCI_COMMAND_IO|PCI_COMMAND_MEMORY);
		pci_write_config_word(dev_sm502, PCI_COMMAND, cmd);
	}
	else puts("not found\n");

	return(sm502);
}
#endif
