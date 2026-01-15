/*
 * Ingenic X1600E Operating System Timer
 *
 * Copyright (c) 2025 Samuel Tibbs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "hw/ptimer.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"

#define TYPE_INGENIC_OST "ingenic-ost"
#define INGENIC_OST(obj) OBJECT_CHECK(IngenicOSTState, (obj), TYPE_INGENIC_OST)

/* OST runs at 1.5MHz according to kernel logs */
#define OST_FREQ_HZ 1500000

/* Register offsets */
#define OST_ER      0x00  /* Enable Register */
#define OST_DR      0x04  /* Disable Register */
#define OST_CNTH    0x08  /* Counter High 32 bits */
#define OST_CNTL    0x0C  /* Counter Low 32 bits */
#define OST_TCSR    0x10  /* Control/Status Register */
#define OST_TCRB    0x14  /* Compare Register B */
#define OST_TFR     0x18  /* Flag Register */
#define OST_TMR     0x1C  /* Mask Register */

typedef struct IngenicOSTState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    QEMUTimer *timer;
    qemu_irq irq;

    /* Registers */
    uint32_t enable;
    uint32_t control;
    uint32_t compare;
    uint32_t flags;
    uint32_t mask;

    /* Counter state */
    uint64_t counter;
    int64_t last_update_time;
} IngenicOSTState;

static void ost_update_counter(IngenicOSTState *s)
{
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    int64_t delta_ns = now - s->last_update_time;

    if (s->enable && delta_ns > 0) {
        /* Convert nanoseconds to OST ticks (1.5MHz = 666.67ns per tick) */
        uint64_t ticks = muldiv64(delta_ns, OST_FREQ_HZ, NANOSECONDS_PER_SECOND);
        s->counter += ticks;
        s->last_update_time = now;
    }
}

static void ost_timer_cb(void *opaque)
{
    IngenicOSTState *s = INGENIC_OST(opaque);

    /* Update counter and check for compare match */
    ost_update_counter(s);

    if (s->counter >= s->compare) {
        s->flags |= 0x01;  /* Set compare match flag */
        if (!(s->mask & 0x01)) {
            qemu_irq_raise(s->irq);
        }
    }

    /* Schedule next timer callback */
    if (s->enable) {
        int64_t next_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 1000000; /* 1ms */
        timer_mod(s->timer, next_ns);
    }
}

static uint64_t ost_read(void *opaque, hwaddr addr, unsigned size)
{
    IngenicOSTState *s = INGENIC_OST(opaque);
    uint64_t value = 0;

    ost_update_counter(s);

    switch (addr) {
    case OST_ER:
        value = s->enable;
        break;
    case OST_CNTH:
        value = (s->counter >> 32) & 0xFFFFFFFF;
        break;
    case OST_CNTL:
        value = s->counter & 0xFFFFFFFF;
        break;
    case OST_TCSR:
        value = s->control;
        break;
    case OST_TCRB:
        value = s->compare;
        break;
    case OST_TFR:
        value = s->flags;
        break;
    case OST_TMR:
        value = s->mask;
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "ingenic-ost: unimplemented read @ 0x%" HWADDR_PRIx "\n",
                      addr);
        break;
    }

    return value;
}

static void ost_write(void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    IngenicOSTState *s = INGENIC_OST(opaque);

    ost_update_counter(s);

    switch (addr) {
    case OST_ER:
        s->enable = value & 0x01;
        if (s->enable) {
            s->last_update_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
            timer_mod(s->timer, s->last_update_time + 1000000);
        } else {
            timer_del(s->timer);
        }
        break;
    case OST_DR:
        s->enable &= ~(value & 0x01);
        if (!s->enable) {
            timer_del(s->timer);
        }
        break;
    case OST_TCSR:
        s->control = value;
        break;
    case OST_TCRB:
        s->compare = value;
        break;
    case OST_TFR:
        /* Writing 1 clears flags */
        s->flags &= ~value;
        if (!(s->flags & 0x01)) {
            qemu_irq_lower(s->irq);
        }
        break;
    case OST_TMR:
        s->mask = value;
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "ingenic-ost: unimplemented write @ 0x%" HWADDR_PRIx
                      " value 0x%" PRIx64 "\n", addr, value);
        break;
    }
}

static const MemoryRegionOps ost_ops = {
    .read = ost_read,
    .write = ost_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void ost_reset(DeviceState *dev)
{
    IngenicOSTState *s = INGENIC_OST(dev);

    s->enable = 0;
    s->control = 0;
    s->compare = 0;
    s->flags = 0;
    s->mask = 0;
    s->counter = 0;
    s->last_update_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    timer_del(s->timer);
}

static void ost_realize(DeviceState *dev, Error **errp)
{
    IngenicOSTState *s = INGENIC_OST(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &ost_ops, s,
                          TYPE_INGENIC_OST, 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);

    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);

    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, ost_timer_cb, s);
}

static const VMStateDescription vmstate_ingenic_ost = {
    .name = TYPE_INGENIC_OST,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(enable, IngenicOSTState),
        VMSTATE_UINT32(control, IngenicOSTState),
        VMSTATE_UINT32(compare, IngenicOSTState),
        VMSTATE_UINT32(flags, IngenicOSTState),
        VMSTATE_UINT32(mask, IngenicOSTState),
        VMSTATE_UINT64(counter, IngenicOSTState),
        VMSTATE_INT64(last_update_time, IngenicOSTState),
        VMSTATE_TIMER_PTR(timer, IngenicOSTState),
        VMSTATE_END_OF_LIST()
    }
};

static void ost_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = ost_reset;
    dc->realize = ost_realize;
    dc->vmsd = &vmstate_ingenic_ost;
}

static const TypeInfo ost_info = {
    .name          = TYPE_INGENIC_OST,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IngenicOSTState),
    .class_init    = ost_class_init,
};

static void ost_register_types(void)
{
    type_register_static(&ost_info);
}

type_init(ost_register_types)
