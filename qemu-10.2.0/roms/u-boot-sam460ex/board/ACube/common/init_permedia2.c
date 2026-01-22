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
#include <pci_ids.h>
#include <video_fb.h>
#include "permedia2.h"

#ifdef CONFIG_VIDEO_PERMEDIA2

DECLARE_GLOBAL_DATA_PTR;

#undef DEBUG

#ifdef  DEBUG
#define PRINTF(fmt,args...)     printf (fmt ,##args)
#else
#define PRINTF(fmt,args...)
#endif

unsigned char PM2INIT = 0;
pci_dev_t dev_pm2 = ~0;

void *init_permedia2(void)
{
	unsigned short cmd;
	GraphicDevice *pm2 = NULL;
	static struct pci_device_id ids[4] = {{}, {}, {}, {0, 0}};

	ids[0].vendor = PCI_VENDOR_ID_3DLABS;
	ids[0].device = PCI_DEVICE_ID_3DLABS_PERMEDIA2;
	ids[1].vendor = PCI_VENDOR_ID_TI;
	ids[1].device = PCI_DEVICE_ID_TI_TVP4020;

	dev_pm2 = pci_find_devices(ids, 0);

	puts("PERMD2:");

	if (dev_pm2 != -1)
	{
		puts("found\n");
		PM2INIT = 1;
		pm2 = (GraphicDevice *)pm2_hw_init();

		// shutdown gfx card
		pci_read_config_word(dev_pm2, PCI_COMMAND, &cmd);
		cmd &= ~(PCI_COMMAND_IO|PCI_COMMAND_MEMORY);
		pci_write_config_word(dev_pm2, PCI_COMMAND, cmd);
	}
	else puts("not found\n");

	return (pm2);
}
#endif
