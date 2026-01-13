# QEMU Board Emulation Guide for Ingenic X1600/Halley6

## Table of Contents
- [Overview](#overview)
- [The Challenge](#the-challenge)
- [Understanding QEMU Architecture](#understanding-qemu-architecture)
- [X1600/Halley6 Hardware Requirements](#x1600halley6-hardware-requirements)
- [Implementation Roadmap](#implementation-roadmap)
- [Step-by-Step Implementation Guide](#step-by-step-implementation-guide)
- [Leveraging Existing Resources](#leveraging-existing-resources)
- [Testing and Debugging](#testing-and-debugging)
- [Learning Resources](#learning-resources)
- [Alternative Approaches](#alternative-approaches)

---

## Overview

This document provides a comprehensive guide to implementing full QEMU board-level emulation for the Ingenic X1600E SoC and Halley6 development board used in the HiBy R3ProII digital audio player.

**Current Status:**
- ✅ CPU emulation: QEMU 8.1.0+ supports XBurstR2 MIPS32 CPUs
- ✅ User-mode emulation: Can run individual MIPS binaries
- ❌ System emulation: Kernel panics due to missing SoC peripherals

**Goal:** Create a QEMU machine type that emulates enough X1600/Halley6 hardware to boot the Linux 4.4.94 kernel and run the HiBy OS.

---

## The Challenge

### Current Problem

When running the X1600 kernel in QEMU with the generic Malta board (`-M malta`), the kernel panics during early boot:

```
========== x1600 clocks: =============
	apll     = 0 , mpll     = 0, ddr = 0
	cpu_clk  = 0 , l2c_clk  = 0
	ahb0_clk = 0 , ahb2_clk = 0
	apb_clk  = 0 , ext_clk  = 24000000

CPU 0 Unable to handle kernel paging request at virtual address 00000080
[<800441e0>] __queue_work+0x58/0x2f0
```

**Root Cause:** All clocks report 0 Hz because the Malta board lacks X1600-specific hardware:
- **CPM** (Clock Power Management Unit) @ 0x10000000
- **OST** (Operating System Timer) @ 0x12000000
- **INTC** (Ingenic Interrupt Controller) @ 0x10001000

Without proper clock initialization, the kernel's workqueue subsystem cannot initialize, causing an immediate panic.

### Why Malta Doesn't Work

The Malta board is a generic MIPS development board with:
- Different memory map
- Different interrupt controller (i8259)
- Different timer hardware
- No Ingenic-specific peripherals

Our kernel expects X1600-specific register addresses and behavior that Malta cannot provide.

---

## Understanding QEMU Architecture

### QEMU System Emulation Overview

QEMU's system emulation consists of several layers:

```
┌─────────────────────────────────────┐
│   Guest OS (Linux 4.4.94)           │
├─────────────────────────────────────┤
│   Virtual Hardware Layer            │
│   - CPU (XBurstR2)                  │
│   - RAM (64MB)                      │
│   - Devices (CPM, OST, INTC, etc.)  │
├─────────────────────────────────────┤
│   QEMU Device Models (C code)       │
│   - Memory-mapped I/O handlers      │
│   - Interrupt routing               │
│   - State management                │
├─────────────────────────────────────┤
│   QEMU Core (TCG, Device Framework) │
└─────────────────────────────────────┘
```

### Key QEMU Concepts

#### 1. **Machine Type** (`hw/mips/`)
Defines the overall board architecture:
- CPU type and count
- RAM size and location
- Device instantiation and memory mapping
- Interrupt controller routing

Example: `hw/mips/malta.c` defines the Malta board machine.

#### 2. **Device Models** (`hw/*/`)
Implement individual hardware peripherals:
- Memory-mapped I/O registers
- Interrupt generation
- DMA controllers
- Timers, clocks, GPIO, etc.

Example: `hw/timer/mips_gictimer.c` implements a MIPS GIC timer.

#### 3. **Memory Regions** (`include/exec/memory.h`)
QEMU's memory API manages address spaces:
- `MemoryRegion`: Represents a memory-mapped region
- `memory_region_init_io()`: Creates MMIO region with read/write callbacks
- `memory_region_add_subregion()`: Maps region to address space

#### 4. **QOM (QEMU Object Model)**
Object-oriented framework for devices:
- Type registration with `TypeInfo`
- Property system for configuration
- Inheritance and composition

#### 5. **Interrupt Routing**
Connects device IRQs to CPU:
- `qemu_irq`: Opaque interrupt line type
- `qemu_allocate_irqs()`: Create interrupt handlers
- Device raises interrupt → INTC → CPU

---

## X1600/Halley6 Hardware Requirements

Based on kernel panic analysis and datasheets, we need to implement:

### Critical (Minimum for Boot)

#### 1. **Clock Power Management (CPM)**
- **Base Address:** 0x10000000
- **Purpose:** Provides system clocks
- **Key Registers:**
  - `CPAPCR` (0x10): APLL Control Register
  - `CPMPCR` (0x14): MPLL Control Register
  - `CPCCR` (0x00): Clock Control Register
  - `CPPCR` (0x0C): PLL Control Register
  - Clock dividers for CPU, L2, AHB, APB

- **Required Behavior:**
  - Return non-zero clock values
  - Simulate 24MHz external crystal (`ext_clk`)
  - Calculate derived clocks: APLL, MPLL, CPU, L2C, AHB, APB
  - Typical values:
    - APLL: 1200 MHz
    - MPLL: 1000 MHz
    - CPU: 600-1000 MHz
    - L2: 300-500 MHz
    - AHB: 200 MHz
    - APB: 100 MHz

#### 2. **Operating System Timer (OST)**
- **Base Address:** 0x12000000 (verify in datasheet)
- **Purpose:** Provides clocksource for scheduler and timekeeping
- **Key Registers:**
  - `OSTCCR`: OST Clock Control Register
  - `OSTER`: OST Enable Register
  - `OSTCR`: OST Control Register
  - `OSTCNT`: Counter value (64-bit)
  - `OSTFR`: Flag Register
  - `OSTMR`: Mask Register

- **Required Behavior:**
  - Provide 1.5MHz clocksource (as kernel expects)
  - Generate timer interrupts
  - 64-bit counter incrementing at 1.5MHz
  - Interrupt on counter match/overflow

#### 3. **Interrupt Controller (INTC)**
- **Base Address:** 0x10001000
- **Purpose:** Routes peripheral interrupts to CPU
- **Key Registers:**
  - `INTC_ISR`: Interrupt Status Register
  - `INTC_IMR`: Interrupt Mask Register
  - `INTC_IMSR`: Interrupt Mask Set Register
  - `INTC_IMCR`: Interrupt Mask Clear Register
  - `INTC_IPR`: Interrupt Pending Register

- **Required Behavior:**
  - 32 or 64 interrupt sources (check datasheet)
  - Mask/unmask individual IRQs
  - Priority handling
  - Connect to MIPS CPU interrupt pins

#### 4. **Memory Map**
```
0x00000000 - 0x03FFFFFF  RAM (64MB)
0x10000000 - 0x10000FFF  CPM registers
0x10001000 - 0x10001FFF  INTC registers
0x12000000 - 0x12000FFF  OST registers
0x13420000 - 0x1342FFFF  UART0
0x13430000 - 0x1343FFFF  UART1
0x134D0000 - 0x134DFFFF  UART2
```

### Secondary (For Full Functionality)

#### 5. **GPIO Controllers**
- Multiple GPIO ports (A-F)
- Base addresses vary by port
- Needed for: buttons, LEDs, peripheral control

#### 6. **UART Serial Ports**
- 3 UARTs for console and debug output
- Standard 16550-compatible interface

#### 7. **I2C Controllers**
- Multiple I2C buses
- Needed for: PMU, audio codec, touch controller

#### 8. **LCD Controller**
- Framebuffer support for 720x480 display
- RGB interface for ST7701 LCD controller

#### 9. **Audio Interface (AIC)**
- I2S/PCM audio interface
- Connects to CS43131 DAC chips

#### 10. **SD/MMC Controller (MSC)**
- For SD card access
- Boot device support

---

## Implementation Roadmap

### Phase 1: Minimal Boot Support (Critical)
**Goal:** Kernel boots to initramfs/panic without crashing

- [ ] Implement CPM device (stub with fixed clock values)
- [ ] Implement OST device (basic timer)
- [ ] Implement INTC device (basic interrupt routing)
- [ ] Create X1600/Halley6 machine type
- [ ] Test kernel boot

**Expected Result:** Kernel prints boot messages, clock values are non-zero, gets further than current panic.

### Phase 2: Console and Init (Secondary)
**Goal:** Kernel boots to shell prompt

- [ ] Implement UART device
- [ ] Improve OST timer accuracy
- [ ] Add missing interrupt sources
- [ ] Test init system startup

**Expected Result:** Can see kernel messages and interact via serial console.

### Phase 3: Storage and Filesystem (Secondary)
**Goal:** Mount rootfs and run userspace

- [ ] Implement SD/MMC controller (or use `-initrd` with ramdisk)
- [ ] Test mounting SquashFS rootfs
- [ ] Verify BusyBox and init scripts

**Expected Result:** Full userspace boot, can run `hiby_player`.

### Phase 4: Peripherals (Optional)
**Goal:** Full device functionality

- [ ] GPIO controllers
- [ ] I2C buses (PMU, codec, touchscreen)
- [ ] LCD controller and framebuffer
- [ ] Audio interface
- [ ] WiFi/Bluetooth (stub or passthrough)

**Expected Result:** Can test display, touch, audio in emulation.

---

## Step-by-Step Implementation Guide

### Prerequisites

1. **Development Environment:**
   - Linux build environment (Cygwin/WSL on Windows)
   - Build tools: `gcc`, `make`, `git`, `pkg-config`
   - Dependencies: `libglib2.0-dev`, `libpixman-1-dev`, `ninja-build`

2. **QEMU Source Code:**
   ```bash
   git clone https://gitlab.com/qemu-project/qemu.git
   cd qemu
   git checkout v8.1.0  # Or latest stable
   ```

3. **Project Documentation:**
   - X1600E datasheet (`thirdpartydocs/X1600_E+Data+Sheet.pdf`)
   - Halley6 hardware guide (`thirdpartydocs/Halley6_hardware_develop_V2.1.pdf`)
   - XBurst programming manual (`thirdpartydocs/XBurst1 CPU core - programming manual.pdf`)
   - Kernel source (if available) or driver code from `r3proii/squashfs-root-example/module_driver/`

---

### Step 1: Create the X1600 CPM Device

#### 1.1 Create Device Files

Create `qemu/hw/misc/ingenic_cpm.c`:

```c
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
```

#### 1.2 Add to Build System

Edit `qemu/hw/misc/meson.build`:

```meson
system_ss.add(when: 'CONFIG_INGENIC_X1600', if_true: files('ingenic_cpm.c'))
```

Edit `qemu/hw/misc/Kconfig`:

```kconfig
config INGENIC_X1600
    bool
    depends on MIPS
```

---

### Step 2: Create the X1600 OST Device

Create `qemu/hw/timer/ingenic_ost.c`:

```c
/*
 * Ingenic X1600E Operating System Timer
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
```

Add to `qemu/hw/timer/meson.build`:

```meson
system_ss.add(when: 'CONFIG_INGENIC_X1600', if_true: files('ingenic_ost.c'))
```

---

### Step 3: Create the Ingenic INTC (Interrupt Controller)

Create `qemu/hw/intc/ingenic_intc.c`:

```c
/*
 * Ingenic X1600E Interrupt Controller
 *
 * Copyright (c) 2025 Your Name
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
```

Add to `qemu/hw/intc/meson.build`:

```meson
system_ss.add(when: 'CONFIG_INGENIC_X1600', if_true: files('ingenic_intc.c'))
```

---

### Step 4: Create the X1600/Halley6 Machine Type

Create `qemu/hw/mips/ingenic_halley6.c`:

```c
/*
 * Ingenic X1600E Halley6 Board Emulation
 *
 * Copyright (c) 2025 Your Name
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/datadir.h"
#include "hw/clock.h"
#include "hw/mips/mips.h"
#include "hw/mips/cpudevs.h"
#include "hw/char/serial.h"
#include "hw/loader.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "sysemu/qtest.h"
#include "sysemu/reset.h"
#include "sysemu/runstate.h"
#include "qemu/error-report.h"
#include "qemu/log.h"

#define PHYS_TO_VIRT(x) ((x) | ~0x7fffffff)

/* Memory map */
#define HALLEY6_RAM_BASE      0x00000000
#define HALLEY6_RAM_SIZE      (64 * MiB)
#define HALLEY6_CPM_BASE      0x10000000
#define HALLEY6_INTC_BASE     0x10001000
#define HALLEY6_OST_BASE      0x12000000
#define HALLEY6_UART0_BASE    0x13420000
#define HALLEY6_UART1_BASE    0x13430000
#define HALLEY6_UART2_BASE    0x13440000

/* IRQ numbers - verify from datasheet */
#define HALLEY6_UART0_IRQ     0
#define HALLEY6_UART1_IRQ     1
#define HALLEY6_UART2_IRQ     2
#define HALLEY6_OST_IRQ       15

typedef struct {
    MachineState parent;

    Clock *cpuclk;
} Halley6MachineState;

#define TYPE_HALLEY6_MACHINE MACHINE_TYPE_NAME("halley6")
OBJECT_DECLARE_SIMPLE_TYPE(Halley6MachineState, HALLEY6_MACHINE)

static void halley6_init(MachineState *machine)
{
    Halley6MachineState *s = HALLEY6_MACHINE(machine);
    const char *kernel_filename = machine->kernel_filename;
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    DeviceState *dev;
    DeviceState *cpudev;
    DeviceState *intc;
    CPUMIPSState *env;
    qemu_irq *cpu_irq;
    int i;

    /* CPU */
    cpudev = qdev_new(MIPS_CPU_TYPE_NAME("XBurstR2"));

    /* Set up CPU clock */
    s->cpuclk = qdev_init_clock_in(cpudev, "clk-in", NULL, NULL, 0);
    clock_set_hz(s->cpuclk, 1000000000); /* 1GHz */

    qdev_realize(cpudev, NULL, &error_fatal);

    env = &MIPS_CPU(cpudev)->env;

    /* RAM */
    memory_region_init_ram(ram, NULL, "halley6.ram", HALLEY6_RAM_SIZE,
                           &error_fatal);
    memory_region_add_subregion(system_memory, HALLEY6_RAM_BASE, ram);

    /* Interrupt controller */
    intc = sysbus_create_simple("ingenic-intc", HALLEY6_INTC_BASE,
                                 env->irq[2]);  /* Connect to CPU INT2 */

    /* Clock Power Management */
    dev = sysbus_create_simple("ingenic-cpm", HALLEY6_CPM_BASE, NULL);

    /* Operating System Timer */
    dev = sysbus_create_simple("ingenic-ost", HALLEY6_OST_BASE,
                                qdev_get_gpio_in(intc, HALLEY6_OST_IRQ));

    /* UARTs */
    serial_mm_init(system_memory, HALLEY6_UART0_BASE, 2,
                   qdev_get_gpio_in(intc, HALLEY6_UART0_IRQ),
                   115200, serial_hd(0), DEVICE_LITTLE_ENDIAN);

    serial_mm_init(system_memory, HALLEY6_UART1_BASE, 2,
                   qdev_get_gpio_in(intc, HALLEY6_UART1_IRQ),
                   115200, serial_hd(1), DEVICE_LITTLE_ENDIAN);

    serial_mm_init(system_memory, HALLEY6_UART2_BASE, 2,
                   qdev_get_gpio_in(intc, HALLEY6_UART2_IRQ),
                   115200, serial_hd(2), DEVICE_LITTLE_ENDIAN);

    /* Load kernel */
    if (kernel_filename) {
        uint64_t entry, kernel_low, kernel_high;
        long kernel_size;

        kernel_size = load_elf(kernel_filename, NULL,
                               cpu_mips_kseg0_to_phys, NULL,
                               &entry, &kernel_low, &kernel_high,
                               NULL, 0, EM_MIPS, 1, 0);

        if (kernel_size < 0) {
            error_report("could not load kernel '%s': %s",
                         kernel_filename,
                         load_elf_strerror(kernel_size));
            exit(1);
        }

        env->active_tc.PC = entry;
    }

    /* Load initrd if specified */
    if (machine->initrd_filename) {
        int64_t initrd_size;
        ram_addr_t initrd_offset;

        initrd_size = get_image_size(machine->initrd_filename);
        if (initrd_size > 0) {
            initrd_offset = HALLEY6_RAM_SIZE - initrd_size;
            initrd_offset &= ~(4096 - 1);  /* Align to 4KB */

            if (load_image_targphys(machine->initrd_filename,
                                    HALLEY6_RAM_BASE + initrd_offset,
                                    initrd_size) != initrd_size) {
                error_report("could not load initrd '%s'",
                             machine->initrd_filename);
                exit(1);
            }

            /* Pass initrd location to kernel via bootloader args */
            /* TODO: Implement bootloader parameter passing */
        }
    }
}

static void halley6_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Ingenic X1600E Halley6 Board";
    mc->init = halley6_init;
    mc->block_default_type = IF_IDE;
    mc->default_ram_size = HALLEY6_RAM_SIZE;
    mc->default_cpu_type = MIPS_CPU_TYPE_NAME("XBurstR2");
}

static const TypeInfo halley6_machine_type = {
    .name = TYPE_HALLEY6_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(Halley6MachineState),
    .class_init = halley6_machine_class_init,
};

static void halley6_machine_register_types(void)
{
    type_register_static(&halley6_machine_type);
}

type_init(halley6_machine_register_types)
```

Add to `qemu/hw/mips/meson.build`:

```meson
mips_ss.add(when: 'CONFIG_INGENIC_X1600', if_true: files('ingenic_halley6.c'))
```

Edit `qemu/hw/mips/Kconfig`:

```kconfig
config INGENIC_X1600
    bool
    select MIPS_CPS
    select SERIAL
    select INGENIC_X1600_DEVICES

config INGENIC_X1600_DEVICES
    bool
    select INGENIC_X1600
```

---

### Step 5: Build QEMU

```bash
cd qemu
mkdir build
cd build

../configure --target-list=mipsel-softmmu \
             --enable-debug \
             --enable-kvm \
             --prefix=/usr/local

make -j$(nproc)
sudo make install
```

---

### Step 6: Test the New Machine

Update `r3proii/qemu/run_qemu.sh`:

```bash
QEMU_BOARD="halley6"  # Changed from "malta"
QEMU_CPU="XBurstR2"

# Remove -bios flag as we're loading kernel directly

"${QEMU_PATH}" \
    -M "${QEMU_BOARD}" \
    -cpu ${QEMU_CPU} \
    -m ${MEMORY_SIZE} \
    -kernel "${KERNEL_IMAGE}" \
    -initrd "${INITRD_IMAGE}" \
    -append "${KERNEL_CMDLINE}" \
    -serial stdio \
    -d guest_errors,unimp \
    -s \
    -S
```

Run QEMU and debug:

```bash
cd r3proii/qemu
./run_qemu.sh -initrd

# In another terminal:
gdb ../Linux-4.4.94+.elf
(gdb) target remote :1234
(gdb) continue
```

---

## Leveraging Existing Resources

### Use Project Documentation

1. **X1600E Datasheet** (`thirdpartydocs/X1600_E+Data+Sheet.pdf`)
   - Chapter on CPM: Extract exact register layouts and PLL formulas
   - Chapter on OST: Timer frequency, register offsets, interrupt numbers
   - Chapter on INTC: Number of IRQ sources, priority handling
   - Memory map: Verify all base addresses

2. **Halley6 Hardware Guide** (`thirdpartydocs/Halley6_hardware_develop_V2.1.pdf`)
   - Board schematic: Component connections, GPIO assignments
   - Boot process: Understand bootloader → kernel → userspace flow
   - Peripheral configuration: Default states

3. **XBurst Programming Manual** (`thirdpartydocs/XBurst1 CPU core - programming manual.pdf`)
   - CPU features that QEMU needs to emulate
   - Coprocessor 0 registers
   - TLB and cache behavior

### Extract Information from Driver Source

Even without full kernel source, you can learn from the limited driver code:

```bash
cd r3proii/squashfs-root-example/module_driver/

# Find register definitions
strings *.ko | grep -E '0x[0-9a-fA-F]{8}'

# Use modinfo to get module parameters
for ko in *.ko; do
    echo "=== $ko ==="
    modinfo $ko
done

# Disassemble drivers to understand register access patterns
mipsel-linux-gnu-objdump -d -M little-endian gpio.ko > gpio_disasm.txt
```

Look for patterns like:
- `lui` + `ori` instructions loading addresses → Register base addresses
- `lw`/`sw` instructions → Register offsets and access patterns
- Function calls → Driver initialization sequences

### Reverse Engineer from Kernel Binary

```bash
# Extract symbols
mipsel-linux-gnu-nm ../Linux-4.4.94+.elf | grep -E 'cpm|ost|intc'

# Find register definitions in kernel
strings ../Linux-4.4.94+.elf | grep -E 'CPM|INTC|OST'

# Disassemble clock init function
# (find address from symbols, then disassemble)
mipsel-linux-gnu-objdump -d ../Linux-4.4.94+.elf \
    --start-address=0x8027a318 \
    --stop-address=0x8027a400
```

### Cross-Reference with Linux Mainline

Search the mainline Linux kernel for similar Ingenic SoCs:

```bash
git clone https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
cd linux

# Find Ingenic drivers
find . -name '*ingenic*' -o -name '*jz47*'

# Study similar drivers
less drivers/clk/ingenic/cgu.c
less arch/mips/boot/dts/ingenic/x1000.dtsi
less arch/mips/jz4740/
```

Adapt code patterns from these drivers to your QEMU device models.

---

## Testing and Debugging

### GDB Debugging Workflow

```bash
# Terminal 1: Start QEMU (paused)
./run_qemu.sh -initrd

# Terminal 2: Connect GDB
mipsel-linux-gnu-gdb ../Linux-4.4.94+.elf

# Set breakpoints
(gdb) break start_kernel
(gdb) break do_IRQ
(gdb) break panic

# Connect and run
(gdb) target remote :1234
(gdb) continue

# When panic occurs:
(gdb) backtrace
(gdb) info registers
(gdb) x/32x $sp  # Examine stack

# View kernel log buffer
(gdb) print __log_buf
(gdb) x/2000s __log_buf
```

### QEMU Logging

Use QEMU's extensive logging:

```bash
qemu-system-mipsel -M halley6 \
    -d in_asm,int,cpu,unimp,guest_errors,exec \
    -D /tmp/qemu_detailed.log \
    ...
```

Log categories:
- `in_asm`: Translated instructions
- `int`: Interrupt delivery
- `cpu`: CPU state changes
- `unimp`: Unimplemented device access
- `guest_errors`: Invalid guest behavior
- `exec`: Execution trace

### Device Register Tracing

Add debug prints to your device models:

```c
#define DEBUG_CPM 1

#ifdef DEBUG_CPM
#define DPRINTF(fmt, ...) \
    do { fprintf(stderr, "CPM: " fmt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do { } while (0)
#endif

static uint64_t cpm_read(void *opaque, hwaddr addr, unsigned size)
{
    // ...
    DPRINTF("read addr=0x%lx value=0x%lx\n", addr, value);
    return value;
}
```

### Incremental Testing Strategy

1. **Boot to early init:**
   - Goal: See "Booting Linux on physical CPU" message
   - Verify: CPU detection, RAM detection

2. **Boot to timer init:**
   - Goal: See "clocksource: ingenic_clocksource" message
   - Verify: CPM returns non-zero clocks, OST registers accessible

3. **Boot to interrupt handling:**
   - Goal: No panic in `__queue_work`
   - Verify: Timer interrupts fire, INTC routes IRQs correctly

4. **Boot to console:**
   - Goal: See login prompt or kernel panic (but later in boot)
   - Verify: UART works, init runs

5. **Boot to userspace:**
   - Goal: Root filesystem mounts, init scripts run
   - Verify: Can execute `hiby_player` or other binaries

---

## Learning Resources

### Essential Reading

#### QEMU Documentation
- **Official QEMU Documentation:** https://www.qemu.org/docs/master/
  - System Emulation: https://www.qemu.org/docs/master/system/index.html
  - QEMU Internals: https://qemu.readthedocs.io/en/latest/devel/index.html

#### QEMU Device Development
- **Adding a New Device:**
  https://www.qemu.org/docs/master/devel/writing-qemu-test.html

- **QOM (QEMU Object Model):**
  https://www.qemu.org/docs/master/devel/qom.html

- **Memory API:**
  https://www.qemu.org/docs/master/devel/memory.html

#### Example Machine Implementations
Study existing QEMU MIPS boards in the source:
- `hw/mips/malta.c` - Generic MIPS board (what we're replacing)
- `hw/mips/boston.c` - MIPS I6400 board
- `hw/arm/virt.c` - ARM virt board (good QOM examples)

### Tutorials and Guides

#### Creating Custom QEMU Boards
- **Emulating Raspberry Pi:**
  https://github.com/trebisky/qemu/blob/main/rpi/README.md

- **Adding a New ARM Board:**
  https://balau82.wordpress.com/2010/02/28/hello-world-for-bare-metal-arm-using-qemu/

#### MIPS Architecture Resources
- **MIPS Architecture Guide:**
  https://www.mips.com/products/architectures/

- **MIPS Linux Kernel:**
  https://git.kernel.org/pub/scm/linux/kernel/git/mips/linux.git

### Community Resources

- **QEMU Mailing List:** qemu-devel@nongnu.org
  - Archives: https://lists.gnu.org/archive/html/qemu-devel/

- **QEMU IRC:** #qemu on irc.oftc.net

- **Ingenic Community:**
  - GitHub: https://github.com/Ingenic-community
  - Linux patches: https://lore.kernel.org/linux-devicetree/ (search "Ingenic")

### Books

- **"Professional Linux Kernel Architecture"** by Wolfgang Mauerer
  - Understanding kernel initialization and device drivers

- **"Understanding the Linux Kernel"** by Daniel P. Bovet
  - Deep dive into kernel internals

---

## Alternative Approaches

If full QEMU board emulation proves too complex, consider these alternatives:

### 1. Malta Kernel Approach (OpenDingux Method)

**What:** Build a Malta-compatible kernel that boots your rootfs

**Steps:**
1. Clone kernel: `git clone https://github.com/dmitrysmagin/linux.git -b jz-3.16-qemu`
2. Port to X1600: Adapt `gcw0-qemu_defconfig` for your device
3. Use stock QEMU: `qemu-system-mipsel -M malta`
4. Tweak rootfs: Adjust `/etc/init.d/` scripts for Malta hardware

**Pros:**
- No QEMU modification needed
- Proven approach (OpenDingux used it successfully)
- Faster development

**Cons:**
- Not accurate for kernel/driver testing
- Can't test hardware-specific features
- Requires kernel recompilation

### 2. User-Mode Only

**What:** Stick with `qemu-mipsel` for testing userspace

**Use for:**
- Reverse engineering `hiby_player`
- Testing firmware modifications that don't touch kernel
- Running individual binaries

**Example:**
```bash
qemu-mipsel -L ../squashfs-root /usr/bin/hiby_player --help
```

**Pros:**
- Already working
- Simple and fast

**Cons:**
- No kernel testing
- No device drivers
- No full system boot

### 3. Hybrid Approach

**What:** Use Malta kernel for basic testing, build full X1600 emulation incrementally

**Strategy:**
1. **Phase 1:** Malta kernel + your rootfs (immediate testing)
2. **Phase 2:** Implement CPM/OST/INTC (kernel boots on "real" hardware)
3. **Phase 3:** Add peripherals as needed (display, audio, etc.)

**Best of both worlds:** Quick initial progress, accurate long-term solution.

---

## Next Steps

### Immediate Actions

1. **Study the datasheets:**
   - Read CPM chapter, note all register offsets and formulas
   - Read OST chapter, understand timer operation
   - Read INTC chapter, map out interrupt numbers

2. **Set up QEMU build environment:**
   ```bash
   git clone https://gitlab.com/qemu-project/qemu.git
   cd qemu
   git checkout v8.1.0
   ```

3. **Start with CPM device:**
   - Implement basic stub that returns fixed clock values
   - Verify kernel can read non-zero clocks

4. **Test incrementally:**
   - After each device implementation, test boot progress
   - Use GDB to verify register access

### Milestones

- [ ] **Milestone 1:** Kernel prints "x1600 clocks: apll=1200000000" (non-zero)
- [ ] **Milestone 2:** Kernel initializes clocksource without panic
- [ ] **Milestone 3:** Kernel enables interrupts and starts scheduler
- [ ] **Milestone 4:** Kernel boots to init system
- [ ] **Milestone 5:** Userspace runs, can see shell prompt
- [ ] **Milestone 6:** Can run `hiby_player` in full system emulation
