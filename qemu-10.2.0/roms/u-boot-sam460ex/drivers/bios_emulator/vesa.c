#include <common.h>
#include "biosemui.h"
#include "vesa.h"

#define OFF(addr)	(u16)(((addr) >> 0) & 0xffff)
#define SEG(addr)	(u16)(((addr) >> 4) & 0xf000)

#define EMULATOR_STRAP_OFFSET   0x30000
#define EMULATOR_STACK_OFFSET   0x20000
#define EMULATOR_VESA_OFFSET    0x40000
#define EMULATOR_BIOS_OFFSET    0xC0000

extern pci_dev_t video_dev;

typedef short WORD;
typedef unsigned char BYTE;
typedef unsigned long DWORD;

struct MODEINFO {
   // Mandatory information for all VBE revision
   WORD  modeattributes;     // Mode attributes
   BYTE  winaattributes;     // Window A attributes
   BYTE  winbattributes;     // Window B attributes
   WORD  wingranularity;     // Window granularity
   WORD  winsize;    	     // Window size
   WORD  winasegment;        // Window A start segment
   WORD  winbsegment;        // Window B start segment
   DWORD winfuncptr;         // pointer to window function
   WORD  bytesperscanline;   // Bytes per scan line

   // Mandatory information for VBE 1.2 and above
   WORD  xresolution;         // Horizontal resolution in pixel or chars
   WORD  yresolution;         // Vertical resolution in pixel or chars
   BYTE  xcharsize;           // Character cell width in pixel
   BYTE  ycharsize;           // Character cell height in pixel
   BYTE  numberofplanes;      // Number of memory planes
   BYTE  bitsperpixel;        // Bits per pixel
   BYTE  numberofbanks;       // Number of banks
   BYTE  memorymodel;         // Memory model type
   BYTE  banksize;            // Bank size in KB
   BYTE  numberofimagepages;  // Number of images
   BYTE  reserved1;           // Reserved for page function

   // Direct Color fields (required for direct/6 and YUV/7 memory models)
   BYTE  redmasksize;         // Size of direct color red mask in bits
   BYTE  redfieldposition;    // Bit position of lsb of red bask
   BYTE  greenmasksize;       // Size of direct color green mask in bits
   BYTE  greenfieldposition;  // Bit position of lsb of green bask
   BYTE  bluemasksize;        // Size of direct color blue mask in bits
   BYTE  bluefieldposition;   // Bit position of lsb of blue bask
   BYTE  rsvdmasksize;        // Size of direct color reserved mask in bits
   BYTE  rsvdfieldposition;   // Bit position of lsb of reserved bask
   BYTE  directcolormodeinfo; // Direct color mode attributes

   // Mandatory information for VBE 2.0 and above
   DWORD physbaseptr;         // Physical address for flat frame buffer
   DWORD offscreenmemoffset;  // Pointer to start of off screen memory
   WORD  offscreenmemsize;    // Amount of off screen memory in 1Kb units
   char  reserved2[206];      // Remainder of ModeInfoBlock
} __attribute__((packed));

/* WARNING: Must be kept in line with the OS 4 bootloader. */

#define SWAPWORD(x) mi->x = (WORD)le16_to_cpu((mi->x))
#define SWAPLONG(x) mi->x = (DWORD)le32_to_cpu((mi->x))

unsigned short makemask(int bits, int shift)
{
	unsigned short mask = 0;
	while (bits)
	{
		bits--;
		mask = mask << 1;
		mask = mask | 1;
	}

	if (shift) mask = mask << shift;
	return mask;
}

#define PRFBI(x) debug("%s = %ld (%lx)\n", #x, (unsigned long)fbi->x, (unsigned long)fbi->x)

void fill_fbi(struct MODEINFO *mi, struct FrameBufferInfo *fbi)
{
	int i;
    unsigned char *a;

	fbi->BaseAddress   = (void *)mi->physbaseptr;
	fbi->XSize         = mi->xresolution;
	fbi->YSize         = mi->yresolution;
	fbi->BitsPerPixel  = mi->bitsperpixel;
	fbi->Modulo        = mi->bytesperscanline;

	fbi->RedMask       = makemask(mi->redmasksize, 8-mi->redmasksize);
	fbi->RedShift      = mi->redfieldposition;

	fbi->GreenMask     = makemask(mi->greenmasksize, 8-mi->greenmasksize);
	fbi->GreenShift    = mi->greenfieldposition;

	fbi->BlueMask      = makemask(mi->bluemasksize, 8-mi->bluemasksize);
	fbi->BlueShift     = mi->bluefieldposition;


#if 0
	PRFBI(BaseAddress);
	PRFBI(XSize);
	PRFBI(YSize);
	PRFBI(BitsPerPixel);
	PRFBI(Modulo);
	PRFBI(RedMask);
	PRFBI(RedShift);
	PRFBI(GreenMask);
	PRFBI(GreenShift);
	PRFBI(BlueMask);
	PRFBI(BlueShift);
#endif

	a = (unsigned char *)mi->physbaseptr;
    if (!a) return;

#if 0
    i = mi->bytesperscanline * mi->yresolution;
    while (i)
    {
    	*a = 0;
        i--;
        a++;
    }
#endif
}

void swap_modeinfo(struct MODEINFO *mi)
{
	SWAPWORD(modeattributes);
	SWAPWORD(wingranularity);
	SWAPWORD(winsize);
	SWAPWORD(winasegment);
	SWAPWORD(winbsegment);
	SWAPLONG(winfuncptr);
	SWAPWORD(bytesperscanline);
	SWAPWORD(xresolution);
	SWAPWORD(yresolution);
	SWAPLONG(physbaseptr);
	SWAPLONG(offscreenmemoffset);
	SWAPWORD(offscreenmemsize);
}

#define PRF(x) debug("%s = %ld (%lx)\n", #x, (unsigned long)mi->x, (unsigned long)mi->x)

void print_modeinfo(struct MODEINFO *mi)
{
#if 0
	PRF(modeattributes);
	PRF(winaattributes);
	PRF(winbattributes);
	PRF(wingranularity);
	PRF(winsize);
	PRF(winasegment);
	PRF(winbsegment);
	PRF(winfuncptr);
	PRF(bytesperscanline);
	PRF(xresolution);
	PRF(yresolution);
	PRF(xcharsize);
	PRF(ycharsize);
	PRF(numberofplanes);
	PRF(bitsperpixel);
	PRF(numberofbanks);
	PRF(memorymodel);
	PRF(banksize);
	PRF(numberofimagepages);
	PRF(redmasksize);
	PRF(redfieldposition);
	PRF(greenmasksize);
	PRF(greenfieldposition);
	PRF(bluemasksize);
	PRF(bluefieldposition);
	PRF(directcolormodeinfo);
	PRF(physbaseptr);
	PRF(offscreenmemoffset);
	PRF(offscreenmemsize);
#endif
}

// List modes - return 0 if mode we want not found.
static int list_vesa_modes(int mode)
{
	RMREGS regs;
	u8 *mem = (u8*)M.mem_base + 0x2000;
	int i;
	u32 pointer;
	u16 *table;

	regs.x.ax = 0x4f00;
	regs.e.esi = SEG(0x2000);
	regs.e.edi = OFF(0x2000);

	BE_int86(0x010, &regs, &regs);

	if (regs.x.ax != 0x004f) {
	    debug("VESA call failed\n");
	    return 0;
	}

	// Check for VGA signature
	if (*(u32 *)mem != 0x56455341) {
	    debug("VESA mode list not found (%x)\n", *(u32 *)mem);
	    return 0;
	}

	pointer = le32_to_cpu(*(u32 *)(mem + 0xe));
	table = BE_mapRealPointer(SEG(pointer), OFF(pointer));

	while((*table != 0xffff)) {
		int found_mode = le16_to_cpu(*table);
		if (found_mode == (mode & 0x3ff)) {
		    debug("Found required VESA mode\n");
		    return 1;
		}
		table++;
	}

	debug("Required VESA mode (%d) not found\n", mode);
	return 0;
}

static int get_current_vesa_mode()
{
	RMREGS regs;

	regs.x.ax = 0x4f03;

	debug("About to get mode\n");
	BE_int86(0x010, &regs, &regs);
	debug("Got mode %d (0x%x)\n", regs.x.bx, regs.x.bx);
	return regs.x.bx;
}

static void _set_vesa_mode(int mode)
{
	RMREGS regs;

	// Don't clear screen, linear frame
	regs.x.ax = 0x4f02;
	regs.x.bx = 0xc000 | mode;

	debug("About to set mode %d\n", mode);
	BE_int86(0x010, &regs, &regs);
}

void *get_vesa_mode(int mode)
{
	RMREGS regs;
	u8 *mem = (u8*)M.mem_base + 0x2000;
	int i;
	u32 pointer;
	u16 *table;

	regs.x.ax = 0x4f01;
	regs.x.cx = mode;
	regs.e.esi = SEG(0x2000);
	regs.e.edi = OFF(0x2000);

	BE_int86(0x010, &regs, &regs);


	return BE_mapRealPointer(SEG(0x2000), OFF(0x2000));

}

void *set_vesa_mode(int mode)
{
    struct MODEINFO *mi;

    if (! list_vesa_modes(mode))
	return NULL;
    _set_vesa_mode(mode);
    mi = get_vesa_mode(get_current_vesa_mode());
    // print_modeinfo(mi);
	swap_modeinfo(mi);

	fbi = (struct FrameBufferInfo *)(malloc(sizeof(struct FrameBufferInfo)));
    if (fbi) fill_fbi(mi,fbi);

    return fbi;
}

// needed for old PCI cards
void *old_set_vesa_mode(int mode)
{
    struct MODEINFO *mi;

    _set_vesa_mode(mode);
    mi = get_vesa_mode(get_current_vesa_mode());
    // print_modeinfo(mi);
	swap_modeinfo(mi);

	fbi = (struct FrameBufferInfo *)(malloc(sizeof(struct FrameBufferInfo)));
    if (fbi) fill_fbi(mi,fbi);

    return fbi;
}

void *DoVesa(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf ("usage: vesa <hex mode> - example vesa 0x111\n");
		return (0);
	}

	u32 mode = (int)simple_strtoul (argv[1], NULL, 16);

	printf("mode: %d\n",mode);

	return set_vesa_mode(mode);
}
