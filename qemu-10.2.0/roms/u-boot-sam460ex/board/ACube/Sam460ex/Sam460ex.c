/*
 * (C) Copyright 2009-2024
 * Max Tretene, ACube Systems Srl. mtretene@acube-systems.com.
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
#include <malloc.h>
#include <ppc440.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <i2c.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/mmu.h>
#include <asm/4xx_pcie.h>
#include <asm/gpio.h>
#include <asm/errno.h>
#include <sm501.h>
#include <pci.h>
#include <usb.h>
#include <video_fb.h>
#include "sam460_pci.h"
#include "../menu/menu.h"
#include "../common/sam_ide.h"
#include "../common/misc_utils.h"
#include "../common/logo460-fullscreen.h"
#include "../common/nanojpeg.h"
#include "../common/vesa_video.h"
#include "../../../drivers/bios_emulator/vesa.h"

#undef DEBUG

#ifdef	DEBUG
#define PRINTF(fmt,args...)		printf (fmt ,##args)
#else
#define PRINTF(fmt,args...)
#endif

DECLARE_GLOBAL_DATA_PTR;

#define BOARD_CANYONLANDS_PCIE	1
#define BOARD_CANYONLANDS_SATA	2

extern int onbus;
extern int console_col;
extern int console_row;
extern int isgadget;
extern int scrolled;

extern pci_dev_t dev_sm502;
extern pci_dev_t dev_pm2;

extern struct FrameBufferInfo *fbi;
extern struct pci_controller *ppc460_hose;

extern unsigned char SM502INIT;
extern unsigned char PM2INIT;

unsigned char ACTIVATESM502 = 0;
unsigned char model_value = 0;

char *chipset_name = NULL;

u8 *logo_buf8 = NULL;
u16 *logo_buf16 = NULL;

extern void *init_sm502(void);
extern void *init_permedia2(void);
extern int init_radeon(struct pci_controller *hose);
extern int do_fpga(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

/*************************************************************************
 * wait_ms
 ************************************************************************/

inline void wait_ms(unsigned long ms)
{
	while (ms--)
		udelay(1000);
}

/*
 * Override the default functions in cpu/ppc4xx/44x_spd_ddr2.c with
 * board specific values.
 */
u32 ddr_wrdtr(u32 default_val) {
	return (SDRAM_WRDTR_LLWP_1_CYC | SDRAM_WRDTR_WTR_180_DEG_ADV | 0x823);
}

u32 ddr_clktr(u32 default_val) {
	return (SDRAM_CLKTR_CLKP_90_DEG_ADV);
}

static int pvr_460ex(void)
{
	u32 pvr = get_pvr();

	if ((pvr == PVR_460EX_RA) || (pvr == PVR_460EX_SE_RA) ||
		(pvr == PVR_460EX_RB))
		return 1;

	return 0;
}

int board_early_init_f(void)
{
	u32 sdr0_cust0;

	/*
	 * Setup the interrupt controller polarities, triggers, etc.
	 */

	// Sam460ex IRQ MAP:
	// IRQ0	 = ETH_INT
	// IRQ1	 = FPGA_INT
	// IRQ2	 = PCI_INT (PCIA, PCIB, PCIC, PCIB)
	// IRQ3	 = FPGA_INT2
	// IRQ11 = RTC_INT
	// IRQ12 = SM502_INT

	mtdcr(UIC0SR, 0xffffffff);	/* clear all */
	mtdcr(UIC0ER, 0x00000000);	/* disable all */
	mtdcr(UIC0CR, 0x00000005);	/* ATI & UIC1 crit are critical */
	mtdcr(UIC0PR, 0xffffffff);	/* per ref-board manual */
	mtdcr(UIC0TR, 0x00000000);	/* per ref-board manual */
	mtdcr(UIC0VR, 0x00000000);	/* int31 highest, base=0x000 */
	mtdcr(UIC0SR, 0xffffffff);	/* clear all */

	mtdcr(UIC1SR, 0xffffffff);	/* clear all */
	mtdcr(UIC1ER, 0x00000000);	/* disable all */
	mtdcr(UIC1CR, 0x00000000);	/* all non-critical */
	mtdcr(UIC1PR, 0xefffffff);	/* IRQ2 neg */
	mtdcr(UIC1TR, 0x00000000);	/* per ref-board manual */
	mtdcr(UIC1VR, 0x00000000);	/* int31 highest, base=0x000 */
	mtdcr(UIC1SR, 0xffffffff);	/* clear all */

	mtdcr(UIC2SR, 0xffffffff);	/* clear all */
	mtdcr(UIC2ER, 0x00000000);	/* disable all */
	mtdcr(UIC2CR, 0x00000000);	/* all non-critical */
	mtdcr(UIC2PR, 0xffffffff);	/* per ref-board manual */
	mtdcr(UIC2TR, 0x00000000);	/* per ref-board manual */
	mtdcr(UIC2VR, 0x00000000);	/* int31 highest, base=0x000 */
	mtdcr(UIC2SR, 0xffffffff);	/* clear all */

	mtdcr(UIC3SR, 0xffffffff);	/* clear all */
	mtdcr(UIC3ER, 0x00000000);	/* disable all */
	mtdcr(UIC3CR, 0x00000000);	/* all non-critical */
	mtdcr(UIC3PR, 0xffefffff);	/* IRQ12 neg */
	mtdcr(UIC3TR, 0x00000000);	/* per ref-board manual */
	mtdcr(UIC3VR, 0x00000000);	/* int31 highest, base=0x000 */
	mtdcr(UIC3SR, 0xffffffff);	/* clear all */

	/* SDR Setting - enable NDFC */
	mfsdr(SDR0_CUST0, sdr0_cust0);
	sdr0_cust0 = SDR0_CUST0_MUX_NDFC_SEL	|
		SDR0_CUST0_NDFC_ENABLE		|
		SDR0_CUST0_NDFC_BW_8_BIT	|
		SDR0_CUST0_NDFC_ARE_MASK	|
		SDR0_CUST0_NDFC_BAC_ENCODE(3)	|
		(0x80000000 >> (28 + CONFIG_SYS_NAND_CS));
	mtsdr(SDR0_CUST0, sdr0_cust0);

	/*
	 * Configure PFC (Pin Function Control) registers
	 * Enable GPIO 49-63
	 * UART0: 8 pins
	 */
	mtsdr(SDR0_PFC0, 0x00007fff);
	mtsdr(SDR0_PFC1, 0x00000000);

	/* Enable PCI host functionality in SDR0_PCI0 */
	mtsdr(SDR0_PCI0, 0xa0000000);

	mtsdr(SDR0_SRST1, 0);	/* Pull AHB out of reset default=1 */

	/* Setup PLB4-AHB bridge based on the system address map */
	mtdcr(AHB_TOP, 0x8000004B);
	mtdcr(AHB_BOT, 0x8000004B);

	return 0;
}

static void canyonlands_sata_init(int board_type)
{
	u32 reg;

	if (board_type == BOARD_CANYONLANDS_SATA) {
		/* Put SATA in reset */
		SDR_WRITE(SDR0_SRST1, 0x00020001);

		/* Set the phy for SATA, not PCI-E port 0 */
		reg = SDR_READ(PESDR0_PHY_CTL_RST);
		SDR_WRITE(PESDR0_PHY_CTL_RST, (reg & 0xeffffffc) | 0x00000001);
		reg = SDR_READ(PESDR0_L0CLK);
		SDR_WRITE(PESDR0_L0CLK, (reg & 0xfffffff8) | 0x00000007);
		SDR_WRITE(PESDR0_L0CDRCTL, 0x00003111);
		SDR_WRITE(PESDR0_L0DRV, 0x00000104);

		/* Bring SATA out of reset */
		SDR_WRITE(SDR0_SRST1, 0x00000000);
	}
}

int checkboard(void)
{
	char s[64] = { 0 };

	gd->board_type = BOARD_CANYONLANDS_PCIE;
	getenv_r("serdes",s,64);

	if (strcmp(s,"sata2") == 0)
		gd->board_type = BOARD_CANYONLANDS_SATA;

	puts("Board: Sam460, PCIe 4x + ");

	switch (gd->board_type) {
	case BOARD_CANYONLANDS_PCIE:
		puts("PCIe 1x\n");
		break;

	case BOARD_CANYONLANDS_SATA:
		puts("SATA-2\n");
		break;
	}

	canyonlands_sata_init(gd->board_type);

	return (0);
}

int board_early_init_r (void)
{
	/*
	 * Clear potential errors resulting from auto-calibration.
	 * If not done, then we could get an interrupt later on when
	 * exceptions are enabled.
	 */

	set_mcsr(get_mcsr());

	return 0;
}

int misc_init_r(void)
{
	u32 sdr0_srst1 = 0;
	u32 eth_cfg;

	/*
	 * Set EMAC mode/configuration (GMII, SGMII, RGMII...).
	 * This is board specific, so let's do it here.
	 */
	mfsdr(SDR0_ETH_CFG, eth_cfg);
	/* disable SGMII mode */
	eth_cfg &= ~(SDR0_ETH_CFG_SGMII2_ENABLE |
			 SDR0_ETH_CFG_SGMII1_ENABLE |
			 SDR0_ETH_CFG_SGMII0_ENABLE);
	/* Set the for 2 RGMII mode */
	/* GMC0 EMAC4_0, GMC0 EMAC4_1, RGMII Bridge 0 */
	eth_cfg &= ~SDR0_ETH_CFG_GMC0_BRIDGE_SEL;
	if (pvr_460ex())
		eth_cfg |= SDR0_ETH_CFG_GMC1_BRIDGE_SEL;
	else
		eth_cfg &= ~SDR0_ETH_CFG_GMC1_BRIDGE_SEL;
	mtsdr(SDR0_ETH_CFG, eth_cfg);

	/*
	 * The AHB Bridge core is held in reset after power-on or reset
	 * so enable it now
	 */
	mfsdr(SDR0_SRST1, sdr0_srst1);
	sdr0_srst1 &= ~SDR0_SRST1_AHB;
	mtsdr(SDR0_SRST1, sdr0_srst1);

	return 0;
}

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
void ft_board_setup(void *blob, bd_t *bd)
{
	u32 val[4];
	int rc;

	ft_cpu_setup(blob, bd);

	/* Fixup NOR mapping */
	val[0] = 0;				/* chip select number */
	val[1] = 0;				/* always 0 */
	val[2] = CONFIG_SYS_FLASH_BASE_PHYS_L;		/* we fixed up this address */
	val[3] = gd->bd->bi_flashsize;
	rc = fdt_find_and_setprop(blob, "/plb/opb/ebc", "ranges",
				  val, sizeof(val), 1);
	if (rc) {
		printf("Unable to update property NOR mapping, err=%s\n",
			   fdt_strerror(rc));
	}

	if (gd->board_type == BOARD_CANYONLANDS_SATA) {
		/*
		 * When SATA is selected we need to disable the first PCIe
		 * node in the device tree, so that Linux doesn't initialize
		 * it.
		 */
		rc = fdt_find_and_setprop(blob, "/plb/pciex@d00000000", "status",
					  "disabled", sizeof("disabled"), 1);
		if (rc) {
			printf("Unable to update property status in PCIe node, err=%s\n",
				   fdt_strerror(rc));
		}
	}

	if (gd->board_type == BOARD_CANYONLANDS_PCIE) {
		/*
		 * When PCIe is selected we need to disable the SATA
		 * node in the device tree, so that Linux doesn't initialize
		 * it.
		 */
		rc = fdt_find_and_setprop(blob, "/plb/sata@bffd1000", "status",
					  "disabled", sizeof("disabled"), 1);
		if (rc) {
			printf("Unable to update property status in PCIe node, err=%s\n",
				   fdt_strerror(rc));
		}
	}
}
#endif /* defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP) */

int last_stage_init (void)
{
	char *model_name = NULL, *boost = NULL;
	int jj, ret = 0;
	u16 fpga_val = 0;
	unsigned short cmd;

	u8 fail = i2c_reg_read(CONFIG_SYS_I2C_RTC_ADDR, 0xe);
	if (fail == 2) // there was an error - disable ddr2_boost
	{
		puts("removing ddr2_boost env var\n");
		setenv("ddr2_boost", NULL);
		saveenv();
	}

	i2c_reg_write(CONFIG_SYS_I2C_RTC_ADDR, 0xe, 0);

	u32 val = mfspr(SPRN_MMUCR);
	val = 0x00010000;
	mtspr(SPRN_MMUCR,val);

	do_fpga(NULL, 0, 0, NULL);

	// Red Led OFF -----------------------------------------------------------
	fpga_val = in_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E);
	fpga_val &= ~0x0002;
	out_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E, fpga_val);

	// fix possible menuboot_cmd misconfiguration ----------------------------
	char *s = getenv("menuboot_cmd");
	if ((!s) ||
		((s) && (strlen(s) < 3)) ||
		((s) && (strcmp(s,"noboot") == 0)))
	{
		setenv("menuboot_cmd","boota");
		saveenv();
	}

	// last pci configurations -----------------------------------------------

	fix_pci_bars();
	assign_pci_irq();
	config_pex8112();

	// cache on --------------------------------------------------------------
	change_tlb(0, 256*1024*1024, 0);

	// load logo image -------------------------------------------------------
	unsigned int xx, yy, ww = 0, hh = 0;

	njInit();
	if (njDecode(bin_data, sizeof(bin_data)) == NJ_OK)
	{
		unsigned char* buf = njGetImage();
		ww = njGetWidth();
		hh = njGetHeight();
		logo_buf8 = malloc(ww*hh); // for 8 bit depth screens
		logo_buf16 = malloc(ww*hh*2); // for 16 bit depth screens

		if (logo_buf8 && logo_buf16)
		{
			u8 *tmp8 = logo_buf8;
			u16 *tmp16 = logo_buf16;

			for (yy = 0; yy < hh; yy++)
			{
				for (xx = 0; xx < ww; xx++)
				{
					*tmp8++ = *buf;
					*tmp16++ = SWAP16(RGB16(*buf));

					buf += 3;
				}
			}
		}
	}
	njDone();

#ifdef CONFIG_VIDEO_SM502
	// SM502 Graphic card on PCI ---------------------------------------------
	GraphicDevice *sm502 = init_sm502();
#endif

#ifdef CONFIG_VIDEO_PERMEDIA2
	// PERMEDIA2 Graphic card on PCI -----------------------------------------
	GraphicDevice *pm2 = init_permedia2();
#endif

	// x86 Graphic card on PCI -----------------------------------------------
	ret = init_radeon(ppc460_hose);

	// active gfx card -------------------------------------------------------
	ACTIVATESM502 = 0; // default VGA card

#ifdef CONFIG_VIDEO_SM502
	s = getenv("video_activate");
	if ((strcmp(s, "sm502") == 0) && (SM502INIT)) ACTIVATESM502 = 1;
	else if ((SM502INIT) && (PM2INIT == 0) && (ret == 0)) ACTIVATESM502 = 1;
#endif

	if (ACTIVATESM502 && SM502INIT)
	{
		pci_read_config_word(dev_sm502, PCI_COMMAND, &cmd);
		cmd |= (PCI_COMMAND_IO|PCI_COMMAND_MEMORY);
		pci_write_config_word(dev_sm502, PCI_COMMAND, cmd);

		if (chipset_name == NULL) chipset_name = "Silicon Motion";

		fbi = (struct FrameBufferInfo *)(malloc(sizeof(struct FrameBufferInfo)));
		if (fbi)
		{
			fbi->BaseAddress   = (void *)sm502->frameAdrs;
			fbi->XSize		   = sm502->winSizeX;
			fbi->YSize		   = sm502->winSizeY;
			fbi->BitsPerPixel  = 8;
			fbi->Modulo		   = sm502->winSizeX;
			fbi->Index		   = GDF__8BIT_INDEX;

			onbus = 0;
			drv_video_init();
		}
	}
	else if (ret > 0)
	{
		// nothing to be done here...
	}
	else if (PM2INIT)
	{
		pci_read_config_word(dev_pm2, PCI_COMMAND, &cmd);
		cmd |= (PCI_COMMAND_IO|PCI_COMMAND_MEMORY);
		pci_write_config_word(dev_pm2, PCI_COMMAND, cmd);

		if (chipset_name == NULL) chipset_name = "Permedia 2";

		fbi = (struct FrameBufferInfo *)(malloc(sizeof(struct FrameBufferInfo)));
		if (fbi)
		{
			fbi->BaseAddress   = (void *)pm2->frameAdrs;
			fbi->XSize		   = pm2->winSizeX;
			fbi->YSize		   = pm2->winSizeY;
			fbi->BitsPerPixel  = 8;
			fbi->Modulo		   = pm2->winSizeX;
			fbi->Index		   = GDF__8BIT_INDEX;

			onbus = 0;
			drv_video_init();
		}
	}

	// custom silent mode ----------------------------------------------------
	int hush = 0;
	s = getenv("hush");
	if (s) hush = atoi(s);
	if (hush)
	{
		s = getenv("stdout");
		if ((s) && (strncmp(s,"vga",3) == 0))
			gd->flags |= GD_FLG_SILENT;
	}

	// Sam460cr doesn't have the onboard SM502 chip --------------------------
	if (SM502INIT)
	{
		model_name = "Sam460ex";
		model_value = 0;
	}
	else
	{
		model_name = "Sam460cr/le";
		model_value = 1;
	}

	// Welcome Screen --------------------------------------------------------
	if (fbi && logo_buf8 && logo_buf16)
	{
		if (fbi->BitsPerPixel == 8)
		{
			memcpy(fbi->BaseAddress, logo_buf8, fbi->XSize * fbi->YSize);
		}

		if (fbi->BitsPerPixel == 16)
		{
			memcpy(fbi->BaseAddress, logo_buf16, fbi->XSize * fbi->YSize * 2);
		}

		if ( ! (gd->flags & GD_FLG_SILENT))
		{
			console_row = 4;
		}
		else
		{
			isgadget = 0;
			scrolled = 0;

			console_row = 1;
			console_col = 38;

			video_puts(model_name); video_puts(" Early Startup Control");

			console_col = 0;
			char buffer[128] = { 0 };
			sprintf(buffer,"%s, %d Mhz, %d MB RAM",model_name,(int)(gd->cpu_clk/1000000), (int)((gd->bd->bi_memsize)/1024/1024));

			s = getenv("ddr2_boost");
			if (s)
			{
				ret = atoi(s);
				if ((ret > 0) && (ret < 4))
				{
					switch(ret)
					{
						case 1: boost = "Read";
						break;
						case 2: boost = "Write";
						break;
						case 3: boost = "Read+Write";
						break;
					}

					sprintf(buffer,"%s - DDR2 Boost = %s",buffer,boost);
				}
			}

			if (chipset_name != NULL)
				sprintf(buffer,"%s - Chipset: %s",buffer,chipset_name);

			int xx = (fbi->XSize - 5 * strlen(buffer)) / 2;
			video_minidrawchars (xx, fbi->YSize-10, buffer, strlen(buffer));

			isgadget = 1;
		}
	}

	printf("Model: %s, PCIe 4x + ",model_name);

	if (gd->board_type == BOARD_CANYONLANDS_PCIE)
		puts("PCIe 1x\n");
	else
		puts("SATA-2\n");

	// cache off -------------------------------------------------------------
	change_tlb(0, 256*1024*1024, TLB_WORD2_I_ENABLE);

	// Yellow LED OFF --------------------------------------------------------
	fpga_val = in_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E);
	fpga_val &= ~0x0004;
	out_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E, fpga_val);

	// Catweasel keyboard ----------------------------------------------------
	//ret = catw_kb_init();

	/*
	 * RTC/M41T62:
	 * Disable square wave output: Batterie will be drained
	 * quickly, when this output is not disabled
	 */
	val = i2c_reg_read(CONFIG_SYS_I2C_RTC_ADDR, 0xa);
	val &= ~0x40;
	i2c_reg_write(CONFIG_SYS_I2C_RTC_ADDR, 0xa, val);

	// cleanup last bytes of the RTC registers bank --------------------------
	// byte 0x0e is used to check if ddr2_boost worked

	i2c_reg_write(CONFIG_SYS_I2C_RTC_ADDR, 0x8, 0);
	i2c_reg_write(CONFIG_SYS_I2C_RTC_ADDR, 0x9, 0);
	i2c_reg_write(CONFIG_SYS_I2C_RTC_ADDR, 0xb, 0);
	i2c_reg_write(CONFIG_SYS_I2C_RTC_ADDR, 0xc, 0);
	i2c_reg_write(CONFIG_SYS_I2C_RTC_ADDR, 0xd, 0);
	i2c_reg_write(CONFIG_SYS_I2C_RTC_ADDR, 0xf, 0);

	// USB Init --------------------------------------------------------------

	u32 usbval;

	SDR_WRITE(SDR0_SRST1, 0x00000008);

	//gpio_config(19, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1);
	gpio_config(16, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1);
	wait_ms(200);

	fpga_val = in_be16((void *)CONFIG_SYS_FPGA_BASE + 0x30);
	fpga_val |= 0x0004; //0x0014;
	out_be16((void *)CONFIG_SYS_FPGA_BASE + 0x30, fpga_val);
	wait_ms(200);

	SDR_WRITE(SDR0_SRST1, 0);

	usbval = in_le32((void *)CONFIG_SYS_AHB_BASE + 0xd0410);
	usbval |= 1 << 1;
	out_le32((void *)CONFIG_SYS_AHB_BASE + 0xd0410, usbval);
	wait_ms(10);

	usbval = in_le32((void *)CONFIG_SYS_AHB_BASE + 0xd0454);
	usbval |= 1 << 12;
	out_le32((void *)CONFIG_SYS_AHB_BASE + 0xd0454, usbval);
	wait_ms(10);

	usbval = in_le32((void *)CONFIG_SYS_AHB_BASE + 0xd0410);
	usbval |= 1 << 1;
	out_le32((void *)CONFIG_SYS_AHB_BASE + 0xd0410, usbval);
	wait_ms(10);

	out_le32((void *)CONFIG_SYS_AHB_BASE + 0xd0048,0xff000001);

	s = getenv("usb_delay");
	if (s)
	{
		ret = atoi(s) * 10;
		if (ret <= 0) ret = 0;
		if (ret > 2000) ret = 2000;
		for (jj=0;jj<ret;jj++) udelay(10000);
	}

	if (gd->flags & GD_FLG_SILENT)
	{
		gfxbox(IBOX_START_X, IBOX_START_Y, IBOX_END_X, IBOX_END_Y, 1);

		console_row = CONSOLE_ROW_START;
		console_col = 30;
		video_puts("Init HW	  ");
		video_putc(0xfd); for (jj=0;jj<10;jj++) video_putc(0xfe);
	}

	ret = usb_init();
	if (ret >= 0)
	{
		drv_usb_kbd_init();
	}

#ifdef CONFIG_USB_STORAGE
	// try to recognize storage devices immediately	--------------------------
	if (ret >= 0)
	{
		//usb_event_poll();
		s = getenv("scan_usb_storage");
		if (s) usb_stor_scan(1);
	}
#endif

	// Init SATA controller --------------------------------------------------
	if (gd->flags & GD_FLG_SILENT) {
		for (jj=0;jj<12;jj++) video_putc(0xfe);
	}

	ide_controllers_init();
/*
	int res = init_sata2(0);
	printf("init_sata2: %x\n",res);
	if (res == 0)
	{
		res = scan_sata2(0);
		printf("scan_sata2: %x\n",res);
	}
*/
	// Ambra LED OFF ---------------------------------------------------------
	fpga_val = in_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E);
	fpga_val &= ~0x0008;
	out_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E, fpga_val);

	if (gd->flags & GD_FLG_SILENT) {
		gd->flags &= ~GD_FLG_SILENT;
		for (jj=0;jj<11;jj++) video_putc(0xfe);
		video_putc(0xfc);
		gd->flags |= GD_FLG_SILENT;
	}

	//show_pcie_info();
	//show_tlb();

	return 0;
}

int do_fpga(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	u8	tmp,dd,mm,yy,rv;
	u16 fpga_val;

	fpga_val = in_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2a);
	tmp = fpga_val & 0xff;
	mm = (tmp/16)*10 + (tmp%16);
	tmp = (fpga_val >> 8) & 0xff;
	dd = (tmp/16)*10 + (tmp%16);

	fpga_val = in_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2c);
	tmp = fpga_val & 0xff;
	rv = (tmp/16)*10 + (tmp%16);
	tmp = (fpga_val >> 8) & 0xff;;
	yy = (tmp/16)*10 + (tmp%16);

	printf("FPGA:  Revision %02d (20%2d-%02d-%02d)\n",rv,yy,mm,dd);

	return 0;
}

int do_shutdown(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	u16 fpga_val;

	fpga_val = 0x000f;
	out_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E, fpga_val);
	wait_ms(300);
	fpga_val = 0x0000;
	out_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E, fpga_val);
	wait_ms(300);
	fpga_val = 0x000f;
	out_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E, fpga_val);
	wait_ms(300);

	fpga_val = 0x0010;
	out_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E, fpga_val);

	while(1); // never return

	return 0;
}

void board_reset(void)
{
	u16 fpga_val;
	fpga_val = in_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E);
	fpga_val |= 0x0010;
	out_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E, fpga_val);
	wait_ms(25);
	fpga_val &= ~0x0010;
	out_be16((void *)CONFIG_SYS_FPGA_BASE + 0x2E, fpga_val);
}

U_BOOT_CMD( fpga,	   1,	   0,	   do_fpga,
	"show FPGA firmware revision",
	"show FPGA firmware revision");

U_BOOT_CMD( shutdown,	   1,	   0,	   do_shutdown,
	"switch machine off",
	"switch machine off");

/*
void show_tlb(void)
{
	int i;
	unsigned long tlb_word0_value;
	unsigned long tlb_word1_value;
	unsigned long tlb_word2_value;

	for (i=0; i<PPC4XX_TLB_SIZE; i++)
	{
		tlb_word0_value = mftlb1(i);
		tlb_word1_value = mftlb2(i);
		tlb_word2_value = mftlb3(i);

		printf("TLB %i, %08x %08x %08x\n",i,tlb_word0_value,tlb_word1_value,tlb_word2_value);

		if ((tlb_word0_value & TLB_WORD0_V_MASK) == TLB_WORD0_V_DISABLE)
			break;
	}
}
*/