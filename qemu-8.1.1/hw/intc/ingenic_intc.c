/*
 * Ingenic X1600E Interrupt Controller
 *
 * Copyright (c) 2025 Samuel Tibbs
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define TYPE_INGENIC_INTC "ingenic-intc"
#define INGENIC_INTC(obj) OBJECT_CHECK(IngenicINTCState, (obj), TYPE_INGENIC_INTC)

#define INTC_NUM_IRQS 64

/* Register offsets */
#define INTC_ISR0   0x00  /* Interrupt Status Register 0 */
#define INTC_IMR0   0x04  /* Interrupt Mask Register 0 */
#define INTC_IMSR0  0x08  /* Interrupt Mask Set Register 0 */
#define INTC_IMCR0  0x0C  /* Interrupt Mask Clear Register 0 */
#define INTC_IPR0   0x10  /* Interrupt Pending Register 0 */
#define INTC_ISR1   0x20  /* Interrupt Status Register 1 */
#define INTC_IMR1   0x24  /* Interrupt Mask Register 1 */
#define INTC_IMSR1  0x28  /* Interrupt Mask Set Register 1 */
#define INTC_IMCR1  0x2C  /* Interrupt Mask Clear Register 1 */
#define INTC_IPR1   0x30  /* Interrupt Pending Register 1 */

typedef struct IngenicINTCState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    qemu_irq parent_irq[8];  /* IRQs to CPU */
    qemu_irq irqs[INTC_NUM_IRQS];  /* Input IRQs from devices */

    uint32_t isr[2];   /* Interrupt Status (raw) */
    uint32_t imr[2];   /* Interrupt Mask */
    uint32_t ipr[2];   /* Interrupt Pending (masked status) */
} IngenicINTCState;

static void intc_update(IngenicINTCState *s)
{
    int i;

    /* Calculate pending interrupts (status & ~mask) */
    s->ipr[0] = s->isr[0] & ~s->imr[0];
    s->ipr[1] = s->isr[1] & ~s->imr[1];

    /* Assert parent IRQ if any pending interrupts */
    bool has_pending = (s->ipr[0] != 0) || (s->ipr[1] != 0);
    qemu_set_irq(s->parent_irq[0], has_pending);
}

static void intc_set_irq(void *opaque, int irq, int level)
{
    IngenicINTCState *s = INGENIC_INTC(opaque);
    int reg = irq / 32;
    int bit = irq % 32;

    if (level) {
        s->isr[reg] |= (1 << bit);
    } else {
        s->isr[reg] &= ~(1 << bit);
    }

    intc_update(s);
}

static uint64_t intc_read(void *opaque, hwaddr addr, unsigned size)
{
    IngenicINTCState *s = INGENIC_INTC(opaque);
    uint64_t value = 0;

    switch (addr) {
    case INTC_ISR0:
        value = s->isr[0];
        break;
    case INTC_IMR0:
        value = s->imr[0];
        break;
    case INTC_IPR0:
        value = s->ipr[0];
        break;
    case INTC_ISR1:
        value = s->isr[1];
        break;
    case INTC_IMR1:
        value = s->imr[1];
        break;
    case INTC_IPR1:
        value = s->ipr[1];
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "ingenic-intc: unimplemented read @ 0x%" HWADDR_PRIx "\n",
                      addr);
        break;
    }

    return value;
}

static void intc_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    IngenicINTCState *s = INGENIC_INTC(opaque);

    switch (addr) {
    case INTC_IMSR0:  /* Set mask bits (disable interrupts) */
        s->imr[0] |= value;
        break;
    case INTC_IMCR0:  /* Clear mask bits (enable interrupts) */
        s->imr[0] &= ~value;
        break;
    case INTC_IMSR1:
        s->imr[1] |= value;
        break;
    case INTC_IMCR1:
        s->imr[1] &= ~value;
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "ingenic-intc: unimplemented write @ 0x%" HWADDR_PRIx
                      " value 0x%" PRIx64 "\n", addr, value);
        break;
    }

    intc_update(s);
}

static const MemoryRegionOps intc_ops = {
    .read = intc_read,
    .write = intc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void intc_reset(DeviceState *dev)
{
    IngenicINTCState *s = INGENIC_INTC(dev);

    s->isr[0] = s->isr[1] = 0;
    s->imr[0] = s->imr[1] = 0xFFFFFFFF;  /* All masked by default */
    s->ipr[0] = s->ipr[1] = 0;
}

static void intc_realize(DeviceState *dev, Error **errp)
{
    IngenicINTCState *s = INGENIC_INTC(dev);
    int i;

    memory_region_init_io(&s->iomem, OBJECT(s), &intc_ops, s,
                          TYPE_INGENIC_INTC, 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);

    /* IRQ output to CPU */
    for (i = 0; i < 8; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->parent_irq[i]);
    }

    /* IRQ inputs from devices */
    qdev_init_gpio_in(dev, intc_set_irq, INTC_NUM_IRQS);
}

static const VMStateDescription vmstate_ingenic_intc = {
    .name = TYPE_INGENIC_INTC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(isr, IngenicINTCState, 2),
        VMSTATE_UINT32_ARRAY(imr, IngenicINTCState, 2),
        VMSTATE_UINT32_ARRAY(ipr, IngenicINTCState, 2),
        VMSTATE_END_OF_LIST()
    }
};

static void intc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = intc_reset;
    dc->realize = intc_realize;
    dc->vmsd = &vmstate_ingenic_intc;
}

static const TypeInfo intc_info = {
    .name          = TYPE_INGENIC_INTC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IngenicINTCState),
    .class_init    = intc_class_init,
};

static void intc_register_types(void)
{
    type_register_static(&intc_info);
}

type_init(intc_register_types)
