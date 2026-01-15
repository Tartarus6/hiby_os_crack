/*
 * Ingenic X1600E Clock Power Management Unit
 *
 * Copyright (c) 2025 Your Name
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define TYPE_INGENIC_CPM "ingenic-cpm"
#define INGENIC_CPM(obj) OBJECT_CHECK(IngenicCPMState, (obj), TYPE_INGENIC_CPM)

/* Register offsets - from X1600 datasheet */
#define CPM_CPCCR       0x00  /* Clock Control Register */
#define CPM_CPCSR       0x04  /* Clock Status Register */
#define CPM_CPPCR       0x0C  /* PLL Control Register */
#define CPM_CPAPCR      0x10  /* APLL Control Register */
#define CPM_CPMPCR      0x14  /* MPLL Control Register */
#define CPM_CLKGR0      0x20  /* Clock Gate Register 0 */
#define CPM_CLKGR1      0x28  /* Clock Gate Register 1 */
#define CPM_DDRCDR      0x2C  /* DDR Clock Divider Register */
#define CPM_CPPCR1      0x30  /* PLL Control Register 1 */
#define CPM_CPAPCR1     0x34  /* APLL Control Register 1 */
#define CPM_CPMPCR1     0x38  /* MPLL Control Register 1 */

typedef struct IngenicCPMState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    /* Register state */
    uint32_t cpccr;
    uint32_t cpcsr;
    uint32_t cppcr;
    uint32_t cpapcr;
    uint32_t cpmpcr;
    uint32_t clkgr0;
    uint32_t clkgr1;
    uint32_t ddrcdr;

    /* Clock values (simulated) */
    uint32_t ext_clk;   /* 24MHz external oscillator */
    uint32_t apll;      /* Audio PLL */
    uint32_t mpll;      /* Memory PLL */
} IngenicCPMState;

/* Calculate simulated clock values based on register settings */
static void cpm_update_clocks(IngenicCPMState *s)
{
    /* Simplified clock calculation - adjust based on real formulas from datasheet
     *
     * For now, return fixed reasonable values:
     * - APLL = 1200 MHz
     * - MPLL = 1000 MHz
     *
     * Real implementation should calculate from PLL registers:
     * PLL_FREQ = EXT_CLK * M / N / OD
     * where M, N, OD are extracted from CPAPCR/CPMPCR registers
     */
    s->apll = 1200000000;  /* 1.2 GHz */
    s->mpll = 1000000000;  /* 1.0 GHz */
}

static uint64_t cpm_read(void *opaque, hwaddr addr, unsigned size)
{
    IngenicCPMState *s = INGENIC_CPM(opaque);
    uint32_t value = 0;

    switch (addr) {
    case CPM_CPCCR:
        /* Clock Control Register
         * Return divider settings that yield reasonable clock frequencies
         * Bits [31:30]: Reserved
         * Bits [29:28]: PDIV (APB divider)
         * Bits [27:24]: H2DIV (AHB2 divider)
         * Bits [23:20]: H0DIV (AHB0 divider)
         * Bits [19:16]: L2DIV (L2 cache divider)
         * Bits [15:12]: Reserved
         * Bits [11:8]:  CDIV (CPU divider)
         * Bits [7:4]:   Reserved
         * Bits [3:0]:   SEL_SRC (clock source select)
         */
        value = s->cpccr;
        break;

    case CPM_CPAPCR:
        /* APLL Control Register */
        value = s->cpapcr;
        break;

    case CPM_CPMPCR:
        /* MPLL Control Register */
        value = s->cpmpcr;
        break;

    case CPM_CLKGR0:
        /* Clock Gate Register 0 - show all clocks enabled */
        value = s->clkgr0;
        break;

    case CPM_CLKGR1:
        /* Clock Gate Register 1 */
        value = s->clkgr1;
        break;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "ingenic-cpm: unimplemented read @ 0x%" HWADDR_PRIx "\n",
                      addr);
        break;
    }

    return value;
}

static void cpm_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    IngenicCPMState *s = INGENIC_CPM(opaque);

    switch (addr) {
    case CPM_CPCCR:
        s->cpccr = value;
        cpm_update_clocks(s);
        break;

    case CPM_CPAPCR:
        s->cpapcr = value;
        cpm_update_clocks(s);
        break;

    case CPM_CPMPCR:
        s->cpmpcr = value;
        cpm_update_clocks(s);
        break;

    case CPM_CLKGR0:
        s->clkgr0 = value;
        break;

    case CPM_CLKGR1:
        s->clkgr1 = value;
        break;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "ingenic-cpm: unimplemented write @ 0x%" HWADDR_PRIx
                      " value 0x%" PRIx64 "\n", addr, value);
        break;
    }
}

static const MemoryRegionOps cpm_ops = {
    .read = cpm_read,
    .write = cpm_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void cpm_reset(DeviceState *dev)
{
    IngenicCPMState *s = INGENIC_CPM(dev);

    /* Initialize to reasonable defaults */
    s->ext_clk = 24000000;  /* 24MHz crystal */

    /* Set up PLL registers for 1200MHz APLL, 1000MHz MPLL
     * These values are examples - adjust based on datasheet formulas
     */
    s->cpapcr = 0x8C000000;  /* APLL enabled, reasonable M/N/OD values */
    s->cpmpcr = 0x8A000000;  /* MPLL enabled */

    /* CPU divider = /1, L2 = /2, AHB = /4, APB = /8
     * This gives: CPU=1000MHz, L2=500MHz, AHB=250MHz, APB=125MHz
     */
    s->cpccr = 0x10210100;

    s->clkgr0 = 0x00000000;  /* All clocks ungated */
    s->clkgr1 = 0x00000000;

    cpm_update_clocks(s);
}

static void cpm_realize(DeviceState *dev, Error **errp)
{
    IngenicCPMState *s = INGENIC_CPM(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &cpm_ops, s,
                          TYPE_INGENIC_CPM, 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static const VMStateDescription vmstate_ingenic_cpm = {
    .name = TYPE_INGENIC_CPM,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(cpccr, IngenicCPMState),
        VMSTATE_UINT32(cpapcr, IngenicCPMState),
        VMSTATE_UINT32(cpmpcr, IngenicCPMState),
        VMSTATE_UINT32(clkgr0, IngenicCPMState),
        VMSTATE_UINT32(clkgr1, IngenicCPMState),
        VMSTATE_END_OF_LIST()
    }
};

static void cpm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = cpm_reset;
    dc->realize = cpm_realize;
    dc->vmsd = &vmstate_ingenic_cpm;
}

static const TypeInfo cpm_info = {
    .name          = TYPE_INGENIC_CPM,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IngenicCPMState),
    .class_init    = cpm_class_init,
};

static void cpm_register_types(void)
{
    type_register_static(&cpm_info);
}

type_init(cpm_register_types)
