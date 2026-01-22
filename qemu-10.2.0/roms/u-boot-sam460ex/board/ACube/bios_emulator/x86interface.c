#define _STDDEF_H
#include <common.h>
#include "glue.h"
#include "scitech/include/x86emu/x86emu.h"
#include "x86interface.h"
#include "../common/misc_utils.h"

/*
 * This isn't nice, but there are a lot of incompatibilities in the U-Boot and scitech include
 * files that this is the only really workable solution.
 * Might be cleaned out later.
 */

#undef DEBUG
#undef SINGLESTEP
#undef FORCE_SINGLESTEP

#undef IO_LOGGING
#undef MEM_LOGGING

#ifdef IO_LOGGING
#define LOGIO(port, format, args...) if (dolog(port)) printf(format , ## args)
#else
#define LOGIO(port, format, args...)
#endif

#ifdef MEM_LOGGIN
#define LOGMEM(format, args...) printf(format , ## args)
#else
#define LOGMEM(format, args...)
#endif

#define log_printf(format, args...)	 if (getenv("x86_log")) printf(format, ## args);

#ifdef DEBUG
#define PRINTF(format, args...) printf(format , ## args)
#else
#define PRINTF(format, argc...)
#endif

typedef unsigned char UBYTE;
typedef unsigned short UWORD;
typedef unsigned long ULONG;

typedef char BYTE;
typedef short WORD;
typedef long LONG;

extern int tstc(void);
extern int getc(void);
extern void reloc_mode_table(void *reloc_addr);

extern int onbus;
extern u32 mmio_base_phys;
extern u32 io_base_phys;

extern void bios_set_mode(int mode);

char *bios_date = "08/14/02";
UBYTE model = 0xFC;
UBYTE submodel = 0x00;

static u32 dummy;

u32 screen_addr(u32 addr)
{
	return &dummy;
}

// Converts an emulator address to a physical address.
// Handles all special cases (bios date, model etc), and might need work
u32 memaddr(u32 addr)
{
//	  if (addr >= 0xF0000 && addr < 0xFFFFF) printf("WARNING: Segment F access (0x%x)\n", addr);
//	  printf("MemAddr=%p\n", addr);
	if (addr >= 0xA0000 && addr < 0xC0000)
		return screen_addr(addr); //CFG_ISA_IO_BASE_ADDRESS + addr;
	else if (addr >= 0xFFFF5 && addr < 0xFFFFE)
	{
		return (u32)bios_date+addr-0xFFFF5;
	}
	else if (addr == 0xFFFFE)
		return (u32)&model;
	else if (addr == 0xFFFFF)
		return (u32)&submodel;
	else if (addr >= 0x80000000)
	{
		//printf("Warning: High memory access at 0x%x\n", addr);
		return addr;
	}
	else
		return (u32)M.mem_base+addr;
}

u8 A1_rdb(u32 addr)
{
	u8 a = in8((UBYTE *)memaddr(addr));
	LOGMEM("rdb: %x -> %x\n", addr, a);
	return a;
}

u16 A1_rdw(u32 addr)
{
	u16 a = in16r((UWORD *)memaddr(addr));
	LOGMEM("rdw: %x -> %x\n", addr, a);
	return a;
}

u32 A1_rdl(u32 addr)
{
	u32 a = in32r((ULONG *)memaddr(addr));
	LOGMEM("rdl: %x -> %x\n", addr, a);
	return a;
}

void A1_wrb(u32 addr, u8 val)
{
	LOGMEM("wrb: %x <- %x\n", addr, val);
	out8((UBYTE *)memaddr(addr), val);
}

void A1_wrw(u32 addr, u16 val)
{
	LOGMEM("wrw: %x <- %x\n", addr, val);
	out16r((UWORD *)memaddr(addr), val);
}

void A1_wrl(u32 addr, u32 val)
{
	LOGMEM("wrl: %x <- %x\n", addr, val);
	out32r((ULONG *)memaddr(addr), val);
}

static X86EMU_memFuncs _A1_mem;

#define in_byte(from) in8( (UBYTE *)port_to_mem(from))
#define in_word(from) in16r((UWORD *)port_to_mem(from))
#define in_long(from) in32r((ULONG *)port_to_mem(from))
#define out_byte(to, val) out8((UBYTE *)port_to_mem(to), val)
#define out_word(to, val) out16r((UWORD *)port_to_mem(to), val)
#define out_long(to, val) out32r((ULONG *)port_to_mem(to), val)

u32 port_to_mem(int port)
{
#ifdef CONFIG_SAM460EX
	/* here we assume that a Radeon is on bus 0 (PCI)		  */
	/* and a RadeonHD is on bus 2 or higher (PCI or PCI-E)	  */

	if (onbus >= 2)
	{
		if (port >= 0xcfc && port <= 0xcff)
			return 0xE3001004;
		else if (port >= 0xcf8 && port <= 0xcfb)
			return 0xE3001000;

		if (port >= io_base_phys) port -= io_base_phys;

		return mmio_base_phys + port;
	}
	else
	{
		if (port >= 0xcfc && port <= 0xcff)
			return 0xDEC00004;
		else if (port >= 0xcf8 && port <= 0xcfb)
			return 0xDEC00000;

		return CFG_ISA_IO_BASE_ADDRESS + port;
	}
#else
	if (port >= 0xcfc && port <= 0xcff)
		return 0xEEC00004;
	else if (port >= 0xcf8 && port <= 0xcfb)
		return 0xEEC00000;

	return CFG_ISA_IO_BASE_ADDRESS + port;
#endif
}

u8 A1_inb(int port)
{
	u8 a;
	//if (port == 0x3BA) return 0;
	a = in_byte(port);
	LOGIO(port, "inb: %Xh -> %d (%Xh)\n", port, a, a);
	return a;
}

u16 A1_inw(int port)
{
	u16 a = in_word(port);
	LOGIO(port, "inw: %Xh -> %d (%Xh)\n", port, a, a);
	return a;
}

u32 A1_inl(int port)
{
	u32 a = in_long(port);
	LOGIO(port, "inl: %Xh -> %d (%Xh)\n", port, a, a);
	return a;
}

void A1_outb(int port, u8 val)
{
	LOGIO(port, "outb: %Xh <- %d (%Xh)\n", port, val, val);
	out_byte(port, val);
}

void A1_outw(int port, u16 val)
{
	LOGIO(port, "outw: %Xh <- %d (%Xh)\n", port, val, val);
	out_word(port, val);
}

int blocked_port = 0;

void A1_outl(int port, u32 val)
{
	LOGIO(port, "outl: %Xh <- %d (%Xh)\n", port, val, val);

	// Workaround
	if (port != blocked_port)
	out_long(port, val);
	else
	LOGIO(port, "blocked\n");
}

static X86EMU_pioFuncs _A1_pio;

static int reloced_ops = 0;

void reloc_ops(void *reloc_addr)
{
	extern void (*x86emu_optab[256])(u8);
	extern void (*x86emu_optab2[256])(u8);
	extern void tables_relocate(unsigned int offset);
	int i;
	unsigned long delta;
	if (reloced_ops == 1) return;
	reloced_ops = 1;

	PRINTF("reloc_addr = %p\n", reloc_addr);
	delta = TEXT_BASE - (unsigned long)reloc_addr;
	PRINTF("delta = %p\n", delta);
	PRINTF("x86emu_optab %p\n",x86emu_optab);
	PRINTF("x86emu_optab %p\n",x86emu_optab-delta);

	for (i=0; i<256; i++)
	{
		x86emu_optab[i] -= delta;
		x86emu_optab2[i] -= delta;
	}

	_A1_mem.rdb = A1_rdb;
	_A1_mem.rdw = A1_rdw;
	_A1_mem.rdl = A1_rdl;
	_A1_mem.wrb = A1_wrb;
	_A1_mem.wrw = A1_wrw;
	_A1_mem.wrl = A1_wrl;

	_A1_pio.inb = (u8 (X86APIP)(X86EMU_pioAddr))A1_inb;
	_A1_pio.inw = (u16 (X86APIP)(X86EMU_pioAddr))A1_inw;
	_A1_pio.inl = (u32 (X86APIP)(X86EMU_pioAddr))A1_inl;
	_A1_pio.outb = (void (X86APIP)(X86EMU_pioAddr, u8))A1_outb;
	_A1_pio.outw = (void (X86APIP)(X86EMU_pioAddr, u16))A1_outw;
	_A1_pio.outl = (void (X86APIP)(X86EMU_pioAddr, u32))A1_outl;

	tables_relocate(delta);
}

unsigned char more_strap[] = {
		0xb4, 0x0, 0xb0, 0x2, 0xcd, 0x10,
};
#define MORE_STRAP_BYTES 6 // Additional bytes of strap code

int execute_bios(pci_dev_t gr_dev, void *reloc_addr)
{
	u8 *strap;
	int i;

	extern void bios_init(void);
	extern void remove_init_data(void);

	reloc_ops(reloc_addr);
	reloc_mode_table(reloc_addr);

	PRINTF("Attempting to run emulator on %02x:%02x:%02x\n",
	   PCI_BUS(gr_dev), PCI_DEV(gr_dev), PCI_FUNC(gr_dev));

	// Allocate memory
	memset(&M, 0, sizeof(X86EMU_sysEnv));
	M.mem_base = (unsigned long)malloc(EMULATOR_MEM_SIZE);
	M.mem_size = (unsigned long)EMULATOR_MEM_SIZE;

	if (!M.mem_base)
	{
		PRINTF("Unable to allocate one megabyte for emulator\n");
		return 0;
	}

	if (attempt_map_rom(gr_dev, (void *)(M.mem_base + EMULATOR_BIOS_OFFSET)) == 0)
	{
		PRINTF("Error mapping rom. Emulation terminated\n");
		return 0;
	}


	strap = (u8*)M.mem_base + EMULATOR_STRAP_OFFSET;

	/*
	 * Poke the strap routine. This might need a bit of extending
	 * if there is a mode switch involved, i.e. we want to int10
	 * afterwards to set a different graphics mode, or alternatively
	 * there might be a different start address requirement if the
	 * ROM doesn't have an x86 image in its first image.
	 */

	PRINTF("Poking strap...\n");

	// FAR CALL c000:0003
	*strap++ = 0x9A; *strap++ = 0x03; *strap++ = 0x00;
	*strap++ = 0x00; *strap++ = 0xC0;

#if 1
	// insert additional strap code
	for (i=0; i < MORE_STRAP_BYTES; i++)
	{
		*strap++ = more_strap[i];
	}
#endif
	// HALT
	*strap++ = 0xF4;

	PRINTF("Done poking strap\n");

	/*
	 * Setup the init parameters.
	 * Per PCI specs, AH must contain the bus and AL
	 * must contain the devfn, encoded as (dev<<3)|fn
	 */

	PRINTF("Settingup init parameters\n");
	// Execution starts here
	M.x86.R_CS = SEG(EMULATOR_STRAP_OFFSET);
	M.x86.R_IP = OFF(EMULATOR_STRAP_OFFSET);

	// Stack at top of ram
	M.x86.R_SS = SEG(EMULATOR_STACK_OFFSET);
	M.x86.R_SP = OFF(EMULATOR_STACK_OFFSET);

	// Input parameters
	M.x86.R_AH = PCI_BUS(gr_dev);
	M.x86.R_AL = (PCI_DEV(gr_dev)<<3) | PCI_FUNC(gr_dev);

	PRINTF("Setting up I/O and memory access functions\n");
	// Set the I/O and memory access functions
	X86EMU_setupMemFuncs(&_A1_mem);
	PRINTF("PIO\n");
	X86EMU_setupPioFuncs(&_A1_pio);

	// If the initializing card is an ATI card, block access to port 0x34
	unsigned short vendor;
	pci_read_config_word(gr_dev, PCI_VENDOR_ID, &vendor);
	if (vendor == 0x1002)
	{
		PRINTF("Initializing a Radeon, blocking port access\n");
		int bar;

		for (bar = PCI_BASE_ADDRESS_0; bar <= PCI_BASE_ADDRESS_5; bar += 4)
		{
			unsigned int val;
			pci_read_config_dword(gr_dev, bar, &val);
			if (val & PCI_BASE_ADDRESS_SPACE_IO)
			{
				blocked_port = val & PCI_BASE_ADDRESS_IO_MASK;
				blocked_port += 0x34;
				break;
			}
		}
	}
	else
		blocked_port = 0;
	PRINTF("Blocked port %x\n",blocked_port);

	// Init the "BIOS".
	PRINTF("BIOS init\n");
	bios_init();

	// Ready set go...
	PRINTF("Running emulator\n");
	X86EMU_exec();
	PRINTF("Done running emulator\n");

	return 1;
}

// Clean up the x86 mess
void shutdown_bios(void)
{

}
