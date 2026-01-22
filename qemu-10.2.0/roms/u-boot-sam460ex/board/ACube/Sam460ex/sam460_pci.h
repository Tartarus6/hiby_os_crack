#ifndef SAM460_PCI_H
#define SAM460_PCI_H

extern void pciauto_setup_device_mem(struct pci_controller *hose,
			  pci_dev_t dev, int bars_num,
			  struct pci_region *mem,
			  struct pci_region *prefetch,
			  struct pci_region *io,
			  pci_size_t bar_size_lower,
			  pci_size_t bar_size_upper);

extern void fix_pci_bars(void);
extern void assign_pci_irq (void);
extern void config_pex8112(void);
extern pci_dev_t pci_find_bridge_for_bus(struct pci_controller *hose, int busnr);

#endif
