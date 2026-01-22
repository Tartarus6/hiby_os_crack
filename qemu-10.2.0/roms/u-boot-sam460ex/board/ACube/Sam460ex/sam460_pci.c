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
#include <sm501.h>
#include <asm/4xx_pcie.h>
#include <asm/errno.h>
#include <pci.h>

#undef DEBUG

#ifdef	DEBUG
#define PRINTF(fmt,args...)		printf (fmt ,##args)
#else
#define PRINTF(fmt,args...)
#endif

DECLARE_GLOBAL_DATA_PTR;

#define BOARD_CANYONLANDS_PCIE	1
#define BOARD_CANYONLANDS_SATA	2

extern struct pci_controller pcie_hose[CONFIG_SYS_PCIE_NR_PORTS];
extern int pci_skip_dev(struct pci_controller *hose, pci_dev_t dev);
extern void pciauto_config_init(struct pci_controller *hose);

struct pci_controller *ppc460_hose = NULL;

/*************************************************************************
 *	pci_pre_init
 *
 *	This routine is called just prior to registering the hose and gives
 *	the board the opportunity to check things. Returning a value of zero
 *	indicates that things are bad & PCI initialization should be aborted.
 *
 *	Different boards may wish to customize the pci controller structure
 *	(add regions, override default access routines, etc) or perform
 *	certain pre-initialization actions.
 *
 ************************************************************************/
#if defined(CONFIG_PCI)
int pci_pre_init(struct pci_controller * hose )
{
	ppc460_hose = hose;

	return 1;
}
#endif	/* defined(CONFIG_PCI) */

#if defined(CONFIG_PCI) && defined(CONFIG_SYS_PCI_MASTER_INIT)
void pci_master_init(struct pci_controller *hose)
{
	/*--------------------------------------------------------------------------+
	  | PowerPC440 PCI Master configuration.
	  | Map PLB/processor addresses to PCI memory space.
	  |	  PLB address 0xA0000000-0xCFFFFFFF ==> PCI address 0x80000000-0xCFFFFFFF
	  |	  Use byte reversed out routines to handle endianess.
	  | Make this region non-prefetchable.
	  +--------------------------------------------------------------------------*/
	out32r(PCIL0_POM0SA, 0 ); /* disable */
	out32r(PCIL0_POM1SA, 0 ); /* disable */
	out32r(PCIL0_POM2SA, 0 ); /* disable */

	out32r(PCIL0_POM0LAL, CONFIG_SYS_PCI_MEMBASE);		/* PMM0 Local Address */
	out32r(PCIL0_POM0LAH, 0x0000000c);					/* PMM0 Local Address */
	out32r(PCIL0_POM0PCIAL, CONFIG_SYS_PCI_MEMBASE);	/* PMM0 PCI Low Address */
	out32r(PCIL0_POM0PCIAH, 0x00000000);				/* PMM0 PCI High Address */
	out32r(PCIL0_POM0SA, ~(0x10000000 - 1) | 1);		/* 256MB + enable region */

	out32r(PCIL0_POM1LAL, CONFIG_SYS_PCI_MEMBASE2);		/* PMM0 Local Address */
	out32r(PCIL0_POM1LAH, 0x0000000c);					/* PMM0 Local Address */
	out32r(PCIL0_POM1PCIAL, CONFIG_SYS_PCI_MEMBASE2);	/* PMM0 PCI Low Address */
	out32r(PCIL0_POM1PCIAH, 0x00000000);				/* PMM0 PCI High Address */
	out32r(PCIL0_POM1SA, ~(0x10000000 - 1) | 1);		/* 256MB + enable region */

	out_le16((void *)PCIL0_CMD, in16r(PCIL0_CMD) | PCI_COMMAND_MASTER);
}
#endif /* defined(CONFIG_PCI) && defined(CONFIG_SYS_PCI_MASTER_INIT) */

#if defined(CONFIG_PCI)
int board_pcie_first(void)
{
	/*
	 * Canyonlands with SATA enabled has only one PCIe slot
	 * (2nd one).
	 */
	if (gd->board_type == BOARD_CANYONLANDS_SATA)
		return 1;

	return 0;
}
#endif /* CONFIG_PCI */

void pciauto_setup_device_mem(struct pci_controller *hose,
			  pci_dev_t dev, int bars_num,
			  struct pci_region *mem,
			  struct pci_region *prefetch,
			  struct pci_region *io,
			  pci_size_t bar_size_lower,
			  pci_size_t bar_size_upper)
{
	unsigned int bar_response, bar_back;
	pci_addr_t bar_value;
	pci_size_t bar_size;
	struct pci_region *bar_res;
	int bar, bar_nr = 0;
	int found_mem64 = 0;

	for (bar = PCI_BASE_ADDRESS_0; bar < PCI_BASE_ADDRESS_0 + (bars_num*4); bar += 4) {
		/* Tickle the BAR and get the response */
		pci_hose_read_config_dword(hose, dev, bar, &bar_back);
		pci_hose_write_config_dword(hose, dev, bar, 0xffffffff);
		pci_hose_read_config_dword(hose, dev, bar, &bar_response);
		pci_hose_write_config_dword(hose, dev, bar, bar_back);

		/* If BAR is not implemented go to the next BAR */
		if (!bar_response)
			continue;

		found_mem64 = 0;

		/* Check the BAR type and set our address mask */
		if ( ! (bar_response & PCI_BASE_ADDRESS_SPACE)) {
			if ( (bar_response & PCI_BASE_ADDRESS_MEM_TYPE_MASK) ==
				 PCI_BASE_ADDRESS_MEM_TYPE_64) {
				u32 bar_response_upper;
				u64 bar64;
				pci_hose_write_config_dword(hose, dev, bar+4, 0xffffffff);
				pci_hose_read_config_dword(hose, dev, bar+4, &bar_response_upper);

				bar64 = ((u64)bar_response_upper << 32) | bar_response;

				bar_size = ~(bar64 & PCI_BASE_ADDRESS_MEM_MASK) + 1;
				found_mem64 = 1;
			} else {
				bar_size = (u32)(~(bar_response & PCI_BASE_ADDRESS_MEM_MASK) + 1);
			}
			if (prefetch && (bar_response & PCI_BASE_ADDRESS_MEM_PREFETCH))
				bar_res = prefetch;
			else
				bar_res = mem;

			PRINTF("PCI Autoconfig: BAR %d, Mem, size=0x%llx, ", bar_nr, (u64)bar_size);

			if ((bar_size >= bar_size_lower) && (bar_size <= bar_size_upper)) {
				if (pciauto_region_allocate(bar_res, bar_size, &bar_value) == 0) {
					/* Write it out and update our limit */
					pci_hose_write_config_dword(hose, dev, bar, (u32)bar_value);
					PRINTF(" BAR written value=0x%8x, ", (u32)bar_value);

					if (found_mem64) {
						bar += 4;
#ifdef CONFIG_SYS_PCI_64BIT
						pci_hose_write_config_dword(hose, dev, bar, (u32)(bar_value>>32));
#else
						/*
						 * If we are a 64-bit decoder then increment to the
						 * upper 32 bits of the bar and force it to locate
						 * in the lower 4GB of memory.
						 */
						pci_hose_write_config_dword(hose, dev, bar, 0x00000000);
#endif
					}
				}
			}
		}

		PRINTF("\n");

		bar_nr++;
	}
}

void fix_pci_bars(void)
{
	struct pci_controller *hose = ppc460_hose;
	unsigned int found_multi=0,problem=0,sec_func=0;
	unsigned short vendor, class;
	unsigned char header_type;
	pci_dev_t dev;
	u32 bar0;

	PRINTF("fix_pci_bars ++++++++++++++++++++++++++++++++++\n");

	for (dev =	PCI_BDF(0,0,0);
		 dev <	PCI_BDF(0,PCI_MAX_PCI_DEVICES-1,PCI_MAX_PCI_FUNCTIONS-1);
		 dev += PCI_BDF(0,0,1)) {

		if (pci_skip_dev(hose, dev))
			continue;

		if (PCI_FUNC(dev) && !found_multi)
			continue;

		pci_hose_read_config_byte(hose, dev, PCI_HEADER_TYPE, &header_type);

		pci_hose_read_config_word(hose, dev, PCI_VENDOR_ID, &vendor);

		if (vendor != 0xffff && vendor != 0x0000) {

			if (!PCI_FUNC(dev))
				found_multi = header_type & 0x80;

			pci_hose_read_config_word(hose, dev, PCI_CLASS_DEVICE, &class);

			PRINTF("PCI Scan: Found Bus %d, Device %d, Function %d - %x\n",
				PCI_BUS(dev), PCI_DEV(dev), PCI_FUNC(dev), class );

			if (((PCI_BUS(dev)==0) && (PCI_DEV(dev)==4) && (PCI_FUNC(dev)==0)) &&
				((class==0x300)	|| (class==0x380))) problem +=1;

			if (((PCI_BUS(dev)==0) && (PCI_DEV(dev)==4) && (PCI_FUNC(dev)==1)) &&
				((class==0x300)	|| (class==0x380))) sec_func =1;

			if ((PCI_BUS(dev)==0) && (PCI_DEV(dev)==6) && (PCI_FUNC(dev)==0)) {
				pci_read_config_dword(dev, PCI_BASE_ADDRESS_0, &bar0);
				bar0 = bar0 & 0xfffff000;
				PRINTF("BAR0 = %8x\n",bar0);

				if ((bar0 == 0) || (bar0 >= 0x9c000000)) problem +=1;
			}
		}
	}

	PRINTF("problem = %d\n",problem);

	if (problem >= 2) {

		pciauto_config_init(hose);

		/* setup MEM SPACE for PCI gfx card (big BARs) */
		dev =  PCI_BDF(0,4,0);
		pciauto_setup_device_mem(hose, dev, 6, hose->pci_mem, hose->pci_prefetch, hose->pci_io, 0x00100000, 0xFFFFFFFF);

		if (sec_func) {
			dev =  PCI_BDF(0,4,1);
			pciauto_setup_device_mem(hose, dev, 6, hose->pci_mem, hose->pci_prefetch, hose->pci_io, 0x00100000, 0xFFFFFFFF);
		}

		/* setup MEM SPACE for the onboard gfx card */
		dev =  PCI_BDF(0,6,0);
		pciauto_setup_device(hose, dev, 6, hose->pci_mem, hose->pci_prefetch, hose->pci_io);

		/* setup MEM SPACE for PCI gfx card (small BARs) */
		dev =  PCI_BDF(0,4,0);
		pciauto_setup_device_mem(hose, dev, 6, hose->pci_mem, hose->pci_prefetch, hose->pci_io, 0x0, 0x000FFFFF);

		if (sec_func) {
			dev =  PCI_BDF(0,4,1);
			pciauto_setup_device_mem(hose, dev, 6, hose->pci_mem, hose->pci_prefetch, hose->pci_io, 0x0, 0x000FFFFF);
		}
	}
}

void assign_pci_irq (void)
{
	u8 ii, class, pin;
	int BusNum, Device, Function;
	unsigned char HeaderType;
	unsigned short VendorID;
	pci_dev_t dev;

	// On Board fixed PCI devices -------------------------

	// Silicon Motion SM502
	if ((dev = pci_find_device(PCI_VENDOR_SM, PCI_DEVICE_SM501, 0)) >= 0)
	{
		// video IRQ connected to UIC3-20 -----------------
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, 116);
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0x20);
	}

	// Optional PCI devices on PCI Slots 33/66 Mhz --------

	for (BusNum = 0; BusNum <= ppc460_hose->last_busno; BusNum++)
	{
		for (Device = 0; Device < PCI_MAX_PCI_DEVICES; Device++)
		{
			HeaderType = 0;

			for (Function = 0; Function < PCI_MAX_PCI_FUNCTIONS; Function++)
			{
				if (Function && !(HeaderType & 0x80))
					break;

				dev = PCI_BDF(BusNum, Device, Function);

				if (dev != -1)
				{
					pci_read_config_word(dev, PCI_VENDOR_ID, &VendorID);
					if ((VendorID == 0xFFFF) || (VendorID == 0x0000))
						continue;

					if (!Function)
						pci_read_config_byte(dev, PCI_HEADER_TYPE, &HeaderType);

					if ((BusNum == 0) && (Device == 0x06)) continue;

					pci_read_config_byte(dev, PCI_CLASS_CODE, &class);

					//if (class != PCI_BASE_CLASS_BRIDGE)
					{
						pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);

						if (pin > 0)
						{
							// all pci IRQ on external slot are connected to UIC1-0
							pci_write_config_byte(dev, PCI_INTERRUPT_LINE, 32);
						}

						pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0x20);
					}
				}
			}
		}
	}

	// PCI-Express bus ----------------------------------------------

	struct pci_controller *hose;

	for (ii = 0; ii < CONFIG_SYS_PCIE_NR_PORTS; ii++)
	{
		hose = &pcie_hose[ii];

		if (hose)
		{
			if (hose->last_busno > hose->first_busno)
			{
				// there is card in the PCIE slot
				// assume no bridge presents

				dev = PCI_BDF(hose->last_busno,0,0);

				if (dev != -1)
				{
					pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);

					if (pin > 0)
					{
						// PCIE 1x slot is connected to UIC3-0
						// PCIE 4x slot is connected to UIC3-6
						pci_write_config_byte(dev, PCI_INTERRUPT_LINE, 0x60 + ii*0x6);
					}
				}
			}
		}
	}
}

void config_pex8112(void)
{
	pci_dev_t dev;

	// special configuration for PCI-PCI Express bridge PEX8112
	if ((dev = pci_find_device(0x10b5,0x8112,0)) >= 0)
	{
		pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, 0x10);
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0xff);
		pci_write_config_byte(dev, PCI_SEC_LATENCY_TIMER, 0xff);
		pci_write_config_byte(dev, 0x48, 0x11);
		pci_write_config_byte(dev, 0x84, 0x0c); // index = 0x100c
		pci_write_config_dword(dev, 0x88, 0xcf008020); // data
	}
}

pci_dev_t pci_find_bridge_for_bus(struct pci_controller *hose, int busnr)
{
	pci_dev_t dev;
	int bus;
	unsigned int found_multi=0;
	unsigned char header_type;
	unsigned short vendor;
	unsigned char secondary_bus;

	if (hose == NULL) return PCI_ANY_ID;

	if (busnr < hose->first_busno || busnr > hose->last_busno) return PCI_ANY_ID; /* Not in range */

	/*
	 * The bridge must be on a lower bus number
	 */
	for (bus = hose->first_busno; bus < busnr; bus++)
	{
		for (dev =  PCI_BDF(bus,0,0);
		     dev <  PCI_BDF(bus,PCI_MAX_PCI_DEVICES-1,PCI_MAX_PCI_FUNCTIONS-1);
		     dev += PCI_BDF(0,0,1))
		{
		    if ( dev == PCI_BDF(hose->first_busno,0,0) )
			continue;

		    if (PCI_FUNC(dev) && !found_multi)
			continue;

		    pci_hose_read_config_byte(hose, dev, PCI_HEADER_TYPE, &header_type);

		    pci_hose_read_config_word(hose, dev, PCI_VENDOR_ID, &vendor);

		    if (vendor != 0xffff && vendor != 0x0000)
		    {
				if (!PCI_FUNC(dev))
				    found_multi = header_type & 0x80;
				if (header_type == 1) /* Bridge device header */
				{
				    pci_hose_read_config_byte(hose, dev, PCI_SECONDARY_BUS, &secondary_bus);
				    if ((int)secondary_bus == busnr) return dev;
				}
		    }
		}
	}
	return PCI_ANY_ID;
}

/*
void show_pcie_info(void)
{
	volatile void *mbase = NULL;

	mbase = (u32 *)CONFIG_SYS_PCIE0_XCFGBASE;

	printf("0:PEGPL_OMR1BA=%08x.%08x MSK=%08x.%08x\n",
		  mfdcr(DCRN_PEGPL_OMR1BAH(PCIE0)),
		  mfdcr(DCRN_PEGPL_OMR1BAL(PCIE0)),
		  mfdcr(DCRN_PEGPL_OMR1MSKH(PCIE0)),
		  mfdcr(DCRN_PEGPL_OMR1MSKL(PCIE0)));

	printf("0:PECFG_POM0LA=%08x.%08x\n", in_le32(mbase + PECFG_POM0LAH),
		  in_le32(mbase + PECFG_POM0LAL));

	printf("0:PECFG_POM2LA=%08x.%08x\n", in_le32(mbase + PECFG_POM2LAH),
		  in_le32(mbase + PECFG_POM2LAL));

	mbase = (u32 *)CONFIG_SYS_PCIE1_XCFGBASE;

	// pci-express bar0
	printf("1:PEGPL_OMR1BA=%08x.%08x MSK=%08x.%08x\n",
		  mfdcr(DCRN_PEGPL_OMR1BAH(PCIE1)),
		  mfdcr(DCRN_PEGPL_OMR1BAL(PCIE1)),
		  mfdcr(DCRN_PEGPL_OMR1MSKH(PCIE1)),
		  mfdcr(DCRN_PEGPL_OMR1MSKL(PCIE1)));

	printf("1:PECFG_POM0LA=%08x.%08x\n", in_le32(mbase + PECFG_POM0LAH),
		  in_le32(mbase + PECFG_POM0LAL));

	printf("1:PECFG_POM2LA=%08x.%08x\n", in_le32(mbase + PECFG_POM2LAH),
		  in_le32(mbase + PECFG_POM2LAL));

}
*/
