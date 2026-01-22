/*
 * (C) Copyright 2001-2004
 * Stefan Roese, esd gmbh germany, stefan.roese@esd-electronics.com
 *
 * (C) Copyright 2005
 * Stefan Roese, DENX Software Engineering, sr@denx.de.
 *
 * (C) Copyright 2006-2007
 * Matthias Fuchs, esd GmbH, matthias.fuchs@esd-electronics.com
 *
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/processor.h>
#include <command.h>
#include <malloc.h>
#include <pci.h>
#include <pci_ids.h>
#include <video_fb.h>
#include "permedia2.h"

#ifdef CONFIG_VIDEO_PERMEDIA2

DECLARE_GLOBAL_DATA_PTR;

GraphicDevice pm2;

#define DISPLAY_WIDTH   640
#define DISPLAY_HEIGHT  480

#define read32(ptrReg) \
	(*(volatile u32 *)(pm2.isaBase + ptrReg))

#define write32(ptrReg, value) \
	(*(volatile u32 *)(pm2.isaBase + ptrReg) = value); SYNC()

inline static void WAIT_INFIFO(u32 x)
{
	while(read32(PM2R_IN_FIFO_SPACE) < x)
	{
		SYNC();
	}
}

extern unsigned char PM2INIT;
extern pci_dev_t dev_pm2;

/*
 * Returns pm2 register base address. First thing called in the driver.
 */
unsigned int pm2_video_get_regs (pci_dev_t devbusfn)
{
	u32 addr;

	if (devbusfn != -1)
	{
		pci_read_config_dword(devbusfn, PCI_BASE_ADDRESS_0, (u32 *)&addr);
		addr += 0x10000; // 64K offset for BE access
		return (addr & 0xfffffffe);
	}

	return 0;
}

/*
 * Returns pm2 framebuffer address
 */
unsigned int pm2_video_get_fb (pci_dev_t devbusfn)
{
	u32 addr;

	if (devbusfn != -1)
	{
		pci_read_config_dword(devbusfn, PCI_BASE_ADDRESS_1, (u32 *)&addr);
		addr &= 0xfffff000;
		return addr;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
 * PM2SetRegs --
 *-----------------------------------------------------------------------------
 */

inline static void pm2_RDAC_WR(s32 idx, u32 v)
{
	int index = PM2R_RD_INDEXED_DATA;
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, idx);
	write32(index, v);
}

static void PM2Reset_card(void)
{
		write32(PM2R_RESET_STATUS, 0);
		while (read32(PM2R_RESET_STATUS) & PM2F_BEING_RESET)
				;
		write32(PM2R_FIFO_DISCON, 1);

		/* Restore stashed memory config information from probe */
		//WAIT_INFIFO(3);
		//write32(PM2R_MEM_CONTROL, p->mem_control);
		//write32(PM2R_BOOT_ADDRESS, p->boot_address);
		//write32(PM2R_MEM_CONFIG, p->mem_config);
}

static void PM2SetRegs (void)
{
	write32(PM2R_RESET_STATUS, 0);
	while(read32(PM2R_RESET_STATUS) & PM2F_BEING_RESET)
		SYNC();

	write32(PM2R_BYPASS_WRITE_MASK, 0xFFFFFFFF);
	write32(PM2R_FRAMEBUFFER_WRITE_MASK, 0xFFFFFFFF);

	write32(PM2R_CHIP_CONFIG, read32(PM2R_CHIP_CONFIG) &= 0xFFF9);
	write32(PM2R_CHIP_CONFIG, read32(PM2R_CHIP_CONFIG) |= PM2F_DISABLE_RETRY);

	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_MISC_CONTROL);
	write32(PM2R_RD_INDEXED_DATA, 2);
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_MODE_CONTROL);
	write32(PM2R_RD_INDEXED_DATA, 0);
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_CURSOR_CONTROL);
	write32(PM2R_RD_INDEXED_DATA, 0);
	write32(PM2R_RD_PIXEL_MASK, 0xFF);
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, 0);

	write32(PM2R_MEM_CONTROL, 0);
	write32(PM2R_BOOT_ADDRESS, 0x30);
	write32(PM2R_MEM_CONFIG, 0xED415C32);
	write32(PM2R_REBOOT, 0);

	WAIT_INFIFO(12);
	write32(PM2R_FB_READ_MODE, 0);
	write32(PM2R_LB_READ_MODE, 0);
	write32(PM2R_DITHER_MODE, 0);
	write32(PM2R_AREA_STIPPLE_MODE, 0);
	write32(PM2R_DEPTH_MODE, 0);
	write32(PM2R_STENCIL_MODE, 0);
	write32(PM2R_TEXTURE_ADDRESS_MODE, 0);
	write32(PM2R_TEXTURE_READ_MODE, 0);
	write32(PM2R_TEXTURE_COLOR_MODE, 0);
	write32(PM2R_TEXEL_LUT_MODE, 0);
	write32(PM2R_YUV_MODE, 0);
	write32(PM2R_COLOR_DDA_MODE, 0);

	WAIT_INFIFO(11);
	write32(PM2R_FOG_MODE, 0);
	write32(PM2R_ALPHA_BLEND_MODE, 0);
	write32(PM2R_LOGICAL_OP_MODE, 0);
	write32(PM2R_STATISTICS_MODE, 0);
	write32(PM2R_WINDOW, (1 << 18));
	write32(PM2R_SCISSOR_MODE, 0);
	write32(PM2R_RASTERIZER_MODE, 0);
	write32(PM2R_FB_READ_PIXEL, 0);
	write32(PM2R_APERTURE_ONE, 0);
	write32(PM2R_APERTURE_TWO, 2);

	// 640x480x8
	WAIT_INFIFO(12);
	write32(PM2R_SCREEN_BASE, 0);
	write32(PM2R_SCREEN_STRIDE, 0x50);
	write32(PM2R_H_TOTAL, 207);
	write32(PM2R_HS_START, 12);
	write32(PM2R_HS_END, 32);
	write32(PM2R_HB_END, 48);
	write32(PM2R_HG_END, 48);
	write32(PM2R_V_TOTAL, 519);
	write32(PM2R_VS_START, 9);
	write32(PM2R_VS_END, 12);
	write32(PM2R_VB_END, 40);
	write32(PM2R_VIDEO_CONTROL, 0x229);

	u32 maxloops = 0x10000;

	WAIT_INFIFO(9);

	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_MEMORY_CLOCK_3);
	write32(PM2R_RD_INDEXED_DATA, 0);

	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_MEMORY_CLOCK_1);
	write32(PM2R_RD_INDEXED_DATA, (u32)0x3a);

	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_MEMORY_CLOCK_2);
	write32(PM2R_RD_INDEXED_DATA, (u32)(0x15 >> 2));

	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_MEMORY_CLOCK_3);
	write32(PM2R_RD_INDEXED_DATA, (u32)((0x15 & 3) | 8));

	do
	{
		write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_MEMORY_CLOCK_STATUS);
	}
	while((maxloops-- > 0) && !(read32(PM2R_RD_INDEXED_DATA) & PM2F_PLL_LOCKED));

	maxloops = 0x10000;
	WAIT_INFIFO(9);

	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_PIXEL_CLOCK_A3);
	write32(PM2R_RD_INDEXED_DATA, 0);

	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_PIXEL_CLOCK_A1);
	write32(PM2R_RD_INDEXED_DATA, (u32)0x68);

	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_PIXEL_CLOCK_A2);
	write32(PM2R_RD_INDEXED_DATA, (u32)(0x1b >> 2));

	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_PIXEL_CLOCK_A3);
	write32(PM2R_RD_INDEXED_DATA, (u32)((0x1b & 3) | 8));

	do
	{
		write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_PIXEL_CLOCK_STATUS);
	}
	while((maxloops-- > 0) && !(read32(PM2R_RD_INDEXED_DATA) & PM2F_PLL_LOCKED));

	WAIT_INFIFO(10);
	write32(PM2R_WINDOW_ORIGIN, 0);
	write32(PM2R_LB_READ_FORMAT, 0);
	write32(PM2R_LB_WRITE_FORMAT, 0);
	write32(PM2R_LB_SOURCE_OFFSET, 0);
	write32(PM2R_FB_SOURCE_OFFSET, 0);
	write32(PM2R_FB_PIXEL_OFFSET, 0);
	write32(PM2R_FB_WINDOW_BASE, 0);
	write32(PM2R_LB_WINDOW_BASE, 0);
	write32(PM2R_FB_SOFT_WRITE_MASK, 0xFFFFFFFF);
	write32(PM2R_FB_HARD_WRITE_MASK, 0xFFFFFFFF);

	WAIT_INFIFO(12);
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_COLOR_MODE);
	write32(PM2R_RD_INDEXED_DATA, 0x10);
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_COLOR_KEY_CONTROL);
	write32(PM2R_RD_INDEXED_DATA, 0);
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_OVERLAY_KEY);
	write32(PM2R_RD_INDEXED_DATA, 0);
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_RED_KEY);
	write32(PM2R_RD_INDEXED_DATA, 0);
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_GREEN_KEY);
	write32(PM2R_RD_INDEXED_DATA, 0);
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, PM2I_RD_BLUE_KEY);
	write32(PM2R_RD_INDEXED_DATA, 0);
}

/*-----------------------------------------------------------------------------
 * pm2_set_lut --
 *-----------------------------------------------------------------------------
 */
void pm2_set_lut (
	unsigned int index,           /* color number */
	unsigned char r,              /* red */
	unsigned char g,              /* green */
	unsigned char b               /* blue */
	)
{
	WAIT_INFIFO(4);
	write32(PM2R_RD_PALETTE_WRITE_ADDRESS, index);
	write32(PM2R_RD_PALETTE_DATA, r);
	write32(PM2R_RD_PALETTE_DATA, g);
	write32(PM2R_RD_PALETTE_DATA, b);
}

/*-----------------------------------------------------------------------------
 * pm2_hw_init --
 *-----------------------------------------------------------------------------
 */
void *pm2_hw_init (void)
{
	unsigned int *vm, ii;

	memset (&pm2, 0, sizeof (GraphicDevice));

	if ((pm2.isaBase = pm2_video_get_regs (dev_pm2)) == 0)
		return (NULL);

	if ((pm2.frameAdrs = pm2_video_get_fb (dev_pm2)) == 0)
		return (NULL);

	pm2.winSizeX = 640;
	pm2.winSizeY = 480;

	pm2.gdfIndex = GDF__8BIT_INDEX;
	pm2.gdfBytesPP = 1;

	pm2.memSize = pm2.winSizeX * pm2.winSizeY * pm2.gdfBytesPP;

	PM2Reset_card();

	/* Clear video memory */
	ii = pm2.memSize/4;
	vm = (unsigned int *)pm2.frameAdrs;
	while(ii--)
		*vm++ = 0;

	/* Load Smi registers */
	PM2SetRegs();

	for (ii=0;ii<256;ii++)
		pm2_set_lut(ii,ii,ii,ii);

	return (&pm2);
}

#endif /* CONFIG_VIDEO_PERMEDIA2 */
