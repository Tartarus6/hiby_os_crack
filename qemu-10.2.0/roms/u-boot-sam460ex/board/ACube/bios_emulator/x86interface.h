#ifndef X86INTERFACE_H
#define X86INTERFACE_H

#define EMULATOR_MEM_SIZE		(1024*1024)
#define EMULATOR_BIOS_OFFSET	0xC0000
#define EMULATOR_STRAP_OFFSET	0x30000
#define EMULATOR_STACK_OFFSET	0x20000
#define EMULATOR_LOGO_OFFSET	0x40000 // If you change this, change the strap code, too

extern u16 A1_rdw(u32 addr);
extern int execute_bios(pci_dev_t gr_dev, void *reloc_addr);
extern void shutdown_bios(void);

#endif
