/*
 * Ingenic X1600E Halley6 Board Emulation
 *
 * Copyright (c) 2025 Greenturtle537
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
#include "hw/boards.h"
#include "sysemu/qtest.h"
#include "sysemu/reset.h"
#include "sysemu/runstate.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "target/mips/cpu.h"

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

#define TYPE_HALLEY6_MACHINE MACHINE_TYPE_NAME("halley6")
OBJECT_DECLARE_SIMPLE_TYPE(Halley6MachineState, HALLEY6_MACHINE)

struct Halley6MachineState {
    MachineState parent;

    Clock *cpuclk;
};

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
//    intc = sysbus_create_simple("ingenic-intc", HALLEY6_INTC_BASE,
//                                 env->irq[2]);  /* Connect to CPU INT2 */

    /* Clock Power Management */
//    dev = sysbus_create_simple("ingenic-cpm", HALLEY6_CPM_BASE, NULL);

    /* Operating System Timer */
//    dev = sysbus_create_simple("ingenic-ost", HALLEY6_OST_BASE,
//                                qdev_get_gpio_in(intc, HALLEY6_OST_IRQ));



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
