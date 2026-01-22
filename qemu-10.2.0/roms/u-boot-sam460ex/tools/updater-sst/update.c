#include <common.h>
#include <ppc_asm.tmpl>
#include <ppc4xx.h>
#include <asm/processor.h>
#include <exports.h>
#include "config.h"
#include "flash_local.h"

DECLARE_GLOBAL_DATA_PTR;

extern flash_info_t  flash_info[]; /* info for FLASH chips */
extern unsigned long __dummy;
extern unsigned long padding[];

void do_reset (void);
int do_updater(void);
int flash_print_info(flash_info_t * info);
static ulong flash_get_size(vu_long * addr, flash_info_t * info);

static u8 pll_fwdv_multi_bits[] = {
	/* values for:  1 - 16 */
	0x00, 0x01, 0x0f, 0x04, 0x09, 0x0a, 0x0d, 0x0e, 0x03, 0x0c,
	0x05, 0x08, 0x07, 0x02, 0x0b, 0x06
};

u32 get_cpr0_fwdv(unsigned long cpr_reg_fwdv)
{
	u32 index;

	for (index = 0; index < ARRAY_SIZE(pll_fwdv_multi_bits); index++)
		if (cpr_reg_fwdv == (u32)pll_fwdv_multi_bits[index])
			return index + 1;

	return 0;
}

static u8 pll_fbdv_multi_bits[] = {
	/* values for:  1 - 100 */
	0x00, 0xff, 0x7e, 0xfd, 0x7a, 0xf5, 0x6a, 0xd5, 0x2a, 0xd4,
	0x29, 0xd3, 0x26, 0xcc, 0x19, 0xb3, 0x67, 0xce, 0x1d, 0xbb,
	0x77, 0xee, 0x5d, 0xba, 0x74, 0xe9, 0x52, 0xa5, 0x4b, 0x96,
	0x2c, 0xd8, 0x31, 0xe3, 0x46, 0x8d, 0x1b, 0xb7, 0x6f, 0xde,
	0x3d, 0xfb, 0x76, 0xed, 0x5a, 0xb5, 0x6b, 0xd6, 0x2d, 0xdb,
	0x36, 0xec, 0x59, 0xb2, 0x64, 0xc9, 0x12, 0xa4, 0x48, 0x91,
	0x23, 0xc7, 0x0e, 0x9c, 0x38, 0xf0, 0x61, 0xc2, 0x05, 0x8b,
	0x17, 0xaf, 0x5f, 0xbe, 0x7c, 0xf9, 0x72, 0xe5, 0x4a, 0x95,
	0x2b, 0xd7, 0x2e, 0xdc, 0x39, 0xf3, 0x66, 0xcd, 0x1a, 0xb4,
	0x68, 0xd1, 0x22, 0xc4, 0x09, 0x93, 0x27, 0xcf, 0x1e, 0xbc,
	/* values for:  101 - 200 */
	0x78, 0xf1, 0x62, 0xc5, 0x0a, 0x94, 0x28, 0xd0, 0x21, 0xc3,
	0x06, 0x8c, 0x18, 0xb0, 0x60, 0xc1, 0x02, 0x84, 0x08, 0x90,
	0x20, 0xc0, 0x01, 0x83, 0x07, 0x8f, 0x1f, 0xbf, 0x7f, 0xfe,
	0x7d, 0xfa, 0x75, 0xea, 0x55, 0xaa, 0x54, 0xa9, 0x53, 0xa6,
	0x4c, 0x99, 0x33, 0xe7, 0x4e, 0x9d, 0x3b, 0xf7, 0x6e, 0xdd,
	0x3a, 0xf4, 0x69, 0xd2, 0x25, 0xcb, 0x16, 0xac, 0x58, 0xb1,
	0x63, 0xc6, 0x0d, 0x9b, 0x37, 0xef, 0x5e, 0xbd, 0x7b, 0xf6,
	0x6d, 0xda, 0x35, 0xeb, 0x56, 0xad, 0x5b, 0xb6, 0x6c, 0xd9,
	0x32, 0xe4, 0x49, 0x92, 0x24, 0xc8, 0x11, 0xa3, 0x47, 0x8e,
	0x1c, 0xb8, 0x70, 0xe1, 0x42, 0x85, 0x0b, 0x97, 0x2f, 0xdf,
	/* values for:  201 - 255 */
	0x3e, 0xfc, 0x79, 0xf2, 0x65, 0xca, 0x15, 0xab, 0x57, 0xae,
	0x5c, 0xb9, 0x73, 0xe6, 0x4d, 0x9a, 0x34, 0xe8, 0x51, 0xa2,
	0x44, 0x89, 0x13, 0xa7, 0x4f, 0x9e, 0x3c, 0xf8, 0x71, 0xe2,
	0x45, 0x8a, 0x14, 0xa8, 0x50, 0xa1, 0x43, 0x86, 0x0c, 0x98,
	0x30, 0xe0, 0x41, 0x82, 0x04, 0x88, 0x10, 0xa0, 0x40, 0x81,
	0x03, 0x87, 0x0f, 0x9f, 0x3f  /* END */
};

u32 get_cpr0_fbdv(unsigned long cpr_reg_fbdv)
{
	u32 index;

	for (index = 0; index < ARRAY_SIZE(pll_fbdv_multi_bits); index++)
		if (cpr_reg_fbdv == (u32)pll_fbdv_multi_bits[index])
			return index + 1;

	return 0;
}

void get_sys_info (sys_info_t * sysInfo)
{
	unsigned long strp0;
	unsigned long strp1;
	unsigned long temp;

	/* Extract configured divisors */
	mfsdr(SDR0_SDSTP0, strp0);
	mfsdr(SDR0_SDSTP1, strp1);

	temp = ((strp0 & PLLSYS0_FWD_DIV_A_MASK) >> 4);
	sysInfo->pllFwdDivA = get_cpr0_fwdv(temp);

	temp = (strp0 & PLLSYS0_FB_DIV_MASK) >> 8;
	sysInfo->pllFbkDiv = get_cpr0_fbdv(temp);
}

void _main(void)
{
	sys_info_t sysinfo;
	unsigned long temp, config_sys_clk_freq, cpu_freq;

	get_sys_info(&sysinfo);

	config_sys_clk_freq = CONFIG_SYS_CLK_FREQ / 100000;
	cpu_freq = gd->cpu_clk / 100000;
	temp = (cpu_freq * sysinfo.pllFwdDivA) / sysinfo.pllFbkDiv;

	if (temp != config_sys_clk_freq)
	{
		printf("\n*****************************************************\n");
		printf("* WARNING!! WARNING!! WARNING!! WARNING!! WARNING!! *\n");
		printf("*****************************************************\n\n");
		printf("You are trying to update U-Boot with a wrong version\n");
		printf("Please download the right version from ACube website\n\n");
		printf("Updater CLK freq : %4ld MHz\n",config_sys_clk_freq / 10);
		printf("Found CPU freq   : %4ld MHz (%d %d)\n\n",cpu_freq / 10,sysinfo.pllFbkDiv,sysinfo.pllFwdDivA);
		return;
	}

	printf("\nU-Boot Firmware Updater\n\n");
	printf("****************************************************\n");
	printf("*  ATTENTION!! PLEASE READ THIS NOTICE CAREFULLY!  *\n");
	printf("****************************************************\n\n");
	printf("This program  will update your computer's  firmware.\n");
	printf("Do NOT  remove the disk,  reset the  machine,  or do\n");
	printf("anything that  might disrupt functionality.  If this\n");
	printf("Program fails, your computer  might be unusable, and\n");
	printf("you will  need to return your  board for reflashing.\n");
	printf("If you find this  too  risky,  remove the cdrom  and\n");
	printf("switch off your  machine now.  Otherwise  press the \n");
	printf("SPACE key now to start the process\n\n");

	do
	{
		char x;
		while (!tstc());
		x = getc();
		if (x == ' ') break;
		if (x == 'q') return;
	} while (1);

	int rc = do_updater();

	if (rc == 0)
	{
		printf("\nUpdate done. Please remove the cdrom.\n");
		printf("You can switch off/reset now when the cdrom is removed\n\n");
	}
	printf("Press the R key to reset\n\n");

	do
	{
		char x;
		while (!tstc());
		x = getc();
		if (x == 'r') do_reset();
	} while (1);
}

int do_updater(void)
{
	unsigned long *addr = &__dummy + 65;
	int rc = 0;

	flash_get_size(0xfff80000,&flash_info[0]);
	rc = flash_print_info(&flash_info[0]);
	if (rc != 0) printf("Error: you need to use another updater with this FLASH chip\n");
	else
	{
		flash_sect_protect(0, 0xFFF80000, 0xFFFFFFFF);

		printf("\nErasing ");
		flash_sect_erase(0xFFF80000, 0xFFFFFFFF);
		printf("Writing ");
		rc = flash_write((uchar *)addr, 0xFFF80000, 0x80000);
		if (rc != 0) printf(" Flashing failed due to error %d\n", rc);
		else printf(" done\n");

		flash_sect_protect(1, 0xFFF80000, 0xFFFFFFFF);
	}

	return rc;
}
