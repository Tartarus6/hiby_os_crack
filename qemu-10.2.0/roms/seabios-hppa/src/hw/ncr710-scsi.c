// NCR 53c710 SCSI definitions
//
// Copyright (C) 2025 Soumyajyotii Ssarkar <soumyajyotisarkar23@gmail.com>
//
// This driver was developed as part of the Google Summer of Code 2025
// program for the SeaBIOS-hppa firmware of the QEMU project.
// Under mentorship of Helge Deller <deller@gmx.de>.
//
// Based on the lsi-scsi.c, adjusted to support non-PCI device.
// This is the bios side for the LASI's NCR53C710 SCSI Controller for QEMU.
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "biosvar.h" // GET_GLOBALFLAT
#include "block.h" // struct drive_s
#include "blockcmd.h" // scsi_drive_setup
#include "config.h" // CONFIG_*
#include "fw/paravirt.h" // runningOnQEMU
#include "malloc.h" // free
#include "output.h" // dprintf
#include "parisc/hppa_hardware.h" // LASI_SCSI_HPA
#include "stacks.h" // run_thread
#include "std/disk.h" // DISK_RET_SUCCESS
#include "string.h" // memset
#include "util.h" // usleep

#define ENABLE_DEBUG 0
#if ENABLE_DEBUG
#define DBG(x)          x
#else
#define DBG(x)          do { } while (0)
#endif

// PA-RISC is big-endian, but NCR710 registers are little-endian.
// We follow the same  endianness conversion that is done in the Linux kernel.
// For byte accesses on big-endian systems, XOR the register address with 3.
// This converts: BE -> LE NCR710 registers
#define bE 3

#define NCR_REG_SCNTL0    0x00
#define NCR_REG_SCNTL1    0x01
#define NCR_REG_SCID      0x04
#define NCR_REG_SXFER     0x05
#define NCR_REG_DSTAT     0x0C
#define NCR_REG_SSTAT0    0x0D
#define NCR_REG_SSTAT1    0x0E
#define NCR_REG_ISTAT     0x21
#define NCR_REG_CTEST8    0x22
#define NCR_REG_DSP0      0x2C
#define NCR_REG_DSP1      0x2D
#define NCR_REG_DSP2      0x2E
#define NCR_REG_DSP3      0x2F
#define NCR_REG_DSPS      0x30
#define NCR_REG_DCNTL     0x3B

// Helper macros for register access with endianness conversion
#define NCR_READ_REG(iobase, reg)  inb((iobase) + ((reg) ^ bE))
#define NCR_WRITE_REG(iobase, reg, val)  outb((val), (iobase) + ((reg) ^ bE))

#define NCR_DSTAT_SIR     0x04
#define NCR_ISTAT_RST     0x40

#define LASI_SCSI_CORE_OFFSET 0x100

struct ncr_lun_s {
    struct drive_s drive;
    u32 iobase;
    u8 target;
    u8 lun;
};

static void
ncr710_reset(u32 iobase)
{
    NCR_WRITE_REG(iobase, NCR_REG_ISTAT, NCR_ISTAT_RST);
    usleep(25000);
    NCR_WRITE_REG(iobase, NCR_REG_ISTAT, 0);
    usleep(5000);

    NCR_WRITE_REG(iobase, NCR_REG_SCID, 0x07);
    NCR_WRITE_REG(iobase, NCR_REG_SXFER, 0x00);
    NCR_WRITE_REG(iobase, NCR_REG_DCNTL, 0x40);
}

int
ncr710_scsi_process_op(struct disk_op_s *op)
{
    if (!CONFIG_NCR710_SCSI)
        return DISK_RET_EBADTRACK;
    struct ncr_lun_s *llun_gf =
        container_of(op->drive_fl, struct ncr_lun_s, drive);
    u16 target = GET_GLOBALFLAT(llun_gf->target);
#if ENABLE_DEBUG
    u16 lun = GET_GLOBALFLAT(llun_gf->lun);
#endif
    u8 cdbcmd[16];
    int blocksize = scsi_fill_cmd(op, cdbcmd, sizeof(cdbcmd));
    if (blocksize < 0)
        return default_process_op(op);
    u32 iobase = GET_GLOBALFLAT(llun_gf->iobase);
    u32 dma = ((scsi_is_read(op) ? 0x01000000 : 0x00000000) |
               (op->count * blocksize));
    u8 status = 0xff;
    u8 msgin = 0xff;

    u32 script[] = {
        0x40000000 | (1 << target) << 16, /* SELECT target */
        0x00000000,
        0x02000010,                        /* Send CDB (16 bytes) */
        (u32)MAKE_FLATPTR(GET_SEG(SS), cdbcmd),
        dma,                               /* DATA IN/OUT transfer */
        (u32)op->buf_fl,
        0x03000001,                        /* Receive STATUS (1 byte) */
        (u32)MAKE_FLATPTR(GET_SEG(SS), &status),
        0x07000001,                        /* Receive MESSAGE IN (1 byte) */
        (u32)MAKE_FLATPTR(GET_SEG(SS), &msgin),
        0x98080000,                        /* INT with success code */
        0x00000401,
    };
    u32 dsp = (u32)MAKE_FLATPTR(GET_SEG(SS), &script);

    NCR_WRITE_REG(iobase, NCR_REG_DSP0, dsp & 0xff);
    NCR_WRITE_REG(iobase, NCR_REG_DSP1, (dsp >> 8) & 0xff);
    NCR_WRITE_REG(iobase, NCR_REG_DSP2, (dsp >> 16) & 0xff);
    NCR_WRITE_REG(iobase, NCR_REG_DSP3, (dsp >> 24) & 0xff);

    DBG(printf("SeaBIOS: ncr710: Script started, DSP=0x%08x\n", dsp));

    int poll_count = 0;
    for (;;) {
        poll_count++;
        u8 istat = NCR_READ_REG(iobase, NCR_REG_ISTAT);
        u8 dstat = NCR_READ_REG(iobase, NCR_REG_DSTAT);
        u8 sstat0 = NCR_READ_REG(iobase, NCR_REG_SSTAT0);
        u8 sstat1 = NCR_READ_REG(iobase, NCR_REG_SSTAT1);
        if (dstat & NCR_DSTAT_SIR) {
            u8 dsps_bytes[4];
            dsps_bytes[0] = NCR_READ_REG(iobase, NCR_REG_DSPS + 0);
            dsps_bytes[1] = NCR_READ_REG(iobase, NCR_REG_DSPS + 1);
            dsps_bytes[2] = NCR_READ_REG(iobase, NCR_REG_DSPS + 2);
            dsps_bytes[3] = NCR_READ_REG(iobase, NCR_REG_DSPS + 3);

            u32 dsps = (dsps_bytes[3] << 24) | (dsps_bytes[2] << 16) |
                       (dsps_bytes[1] << 8) | dsps_bytes[0];

            DBG(printf("SeaBIOS: ncr710: SCRIPTS interrupt (poll_count=%d), DSTAT=0x%02x, DSPS=0x%08x\n", poll_count, dstat, dsps));

            if (dsps == 0x00000401) {
                DBG(printf("SeaBIOS: ncr710: Command completed successfully! (target=%d, lun=%d)\n", target, lun));
                return DISK_RET_SUCCESS;
            } else {
                DBG(printf("SeaBIOS: ncr710: Unexpected SCRIPTS interrupt code 0x%08x\n", dsps));
                goto fail;
            }
        }

        if (istat & 0x02) {  /* SIP bit - SCSI interrupt pending */
            DBG(printf("SeaBIOS: ncr710: SCSI interrupt (poll_count=%d), ISTAT=0x%02x, SSTAT0=0x%02x, SSTAT1=0x%02x\n",
                poll_count, istat, sstat0, sstat1));

            if (sstat0 & 0x80) {  /* MA - Message Acknowledge / Phase Mismatch */
                DBG(printf("SeaBIOS: ncr710: Phase mismatch detected (poll_count=%d), command may have failed\n", poll_count));
                goto fail;
            }

            if (sstat0 & 0x04) {  /* UDC - Unexpected Disconnect */
                DBG(printf("SeaBIOS: ncr710: Device disconnected (poll_count=%d)\n", poll_count));
                return DISK_RET_SUCCESS;
            }
        }

        if (sstat0 & 0x20) {
            goto fail;
        }
        if (sstat0 & ~0xA0) {
            DBG(printf("SeaBIOS: ncr710: SCSI error, SSTAT0=0x%02x, SSTAT1=0x%02x\n", sstat0, sstat1));
            goto fail;
        }
        if (sstat1 & 0x1B) {
            DBG(printf("SeaBIOS: ncr710: SCSI error, SSTAT0=0x%02x, SSTAT1=0x%02x\n", sstat0, sstat1));
            goto fail;
        }

        if (dstat & 0x70) {
            DBG(printf("SeaBIOS: ncr710: DMA error, DSTAT=0x%02x\n", dstat));
            goto fail;
        }

        usleep(5);
    }

fail:
    return DISK_RET_EBADTRACK;
}

static int
ncr710_detect_controller(u32 iobase)
{
    DBG(printf("SeaBIOS: ncr710: Starting controller detection at 0x%x\n", iobase));
    // TEMP register is at 0x1C - using direct byte access with XOR
    u32 temp_reg_base = 0x1C;

    u8 original_temp[4];
    original_temp[0] = NCR_READ_REG(iobase, temp_reg_base + 0);
    original_temp[1] = NCR_READ_REG(iobase, temp_reg_base + 1);
    original_temp[2] = NCR_READ_REG(iobase, temp_reg_base + 2);
    original_temp[3] = NCR_READ_REG(iobase, temp_reg_base + 3);
    DBG(printf("SeaBIOS: ncr710: Original TEMP register: 0x%02x%02x%02x%02x\n",
           original_temp[3], original_temp[2], original_temp[1], original_temp[0]));

    NCR_WRITE_REG(iobase, temp_reg_base + 0, 0x12);
    NCR_WRITE_REG(iobase, temp_reg_base + 1, 0x34);
    NCR_WRITE_REG(iobase, temp_reg_base + 2, 0x56);
    NCR_WRITE_REG(iobase, temp_reg_base + 3, 0x78);

    u8 read_back[4];
    read_back[0] = NCR_READ_REG(iobase, temp_reg_base + 0);
    read_back[1] = NCR_READ_REG(iobase, temp_reg_base + 1);
    read_back[2] = NCR_READ_REG(iobase, temp_reg_base + 2);
    read_back[3] = NCR_READ_REG(iobase, temp_reg_base + 3);
    DBG(printf("SeaBIOS: ncr710: TEMP test - wrote 0x12345678, read bytes: 0x%02x 0x%02x 0x%02x 0x%02x\n",
           read_back[0], read_back[1], read_back[2], read_back[3]));

    NCR_WRITE_REG(iobase, temp_reg_base + 0, original_temp[0]);
    NCR_WRITE_REG(iobase, temp_reg_base + 1, original_temp[1]);
    NCR_WRITE_REG(iobase, temp_reg_base + 2, original_temp[2]);
    NCR_WRITE_REG(iobase, temp_reg_base + 3, original_temp[3]);

    if (read_back[0] == 0x12 && read_back[1] == 0x34 &&
        read_back[2] == 0x56 && read_back[3] == 0x78) {
        DBG(printf("SeaBIOS: ncr710: Controller detected successfully at 0x%x\n", iobase));
        return 0;
    }
    DBG(printf("SeaBIOS: ncr710: Controller detection failed at 0x%x\n", iobase));
    return -1;
}

static void
ncr710_scsi_init_lun(struct ncr_lun_s *nlun, u32 iobase, u8 target, u8 lun)
{
    memset(nlun, 0, sizeof(*nlun));
    nlun->drive.type = DTYPE_NCR710_SCSI;
    nlun->drive.cntl_id = 0;
    nlun->drive.max_bytes_transfer = 64*1024;   /* 64 kb */
    nlun->target = target;
    nlun->lun = lun;
    nlun->iobase = iobase;
}

static int
ncr710_scsi_add_lun(u32 lun, struct drive_s *tmpl_drv)
{
    struct ncr_lun_s *tmpl_nlun = container_of(tmpl_drv, struct ncr_lun_s, drive);
    struct ncr_lun_s *nlun = malloc_fseg(sizeof(*nlun));
    if (!nlun) {
        warn_noalloc();
        return -1;
    }

    ncr710_scsi_init_lun(nlun, tmpl_nlun->iobase, tmpl_nlun->target, lun);

    char *name = znprintf(MAXDESCSIZE, "ncr710 %d:%d", nlun->target, nlun->lun);
    int prio = bootprio_find_scsi_device(NULL, nlun->target, nlun->lun);
    int ret = scsi_drive_setup(&nlun->drive, name, prio, nlun->target, nlun->lun);
    free(name);

    if (ret) {
        free(nlun);
        return -1;
    }
    return 0;
}

static void
ncr710_scsi_scan_target(u32 iobase, u8 target)
{
    DBG(printf("SeaBIOS: ncr710: Starting scan of target %d\n", target));
    struct ncr_lun_s nlun0;
    ncr710_scsi_init_lun(&nlun0, iobase, target, 0);

    DBG(printf("SeaBIOS: ncr710: Trying scsi_rep_luns_scan for target %d\n", target));
    if (scsi_rep_luns_scan(&nlun0.drive, ncr710_scsi_add_lun) < 0) {
        DBG(printf("SeaBIOS: ncr710: scsi_rep_luns_scan failed, trying scsi_sequential_scan for target %d\n", target));
        scsi_sequential_scan(&nlun0.drive, 8, ncr710_scsi_add_lun);
    }
    DBG(printf("SeaBIOS: ncr710: Finished scanning target %d\n", target));
}

static void
init_ncr710_scsi(u32 base_addr)
{
    u32 iobase = base_addr + LASI_SCSI_CORE_OFFSET;
    DBG(printf("SeaBIOS: ncr710: Base addr=0x%x, Core offset=0x%x, IO base=0x%x\n",
            base_addr, LASI_SCSI_CORE_OFFSET, iobase));

    ncr710_reset(iobase);

    if (ncr710_detect_controller(iobase) < 0) {
        DBG(printf("SeaBIOS: ncr710: Controller not found at 0x%x\n", iobase));
        return;
    }

    DBG(printf("SeaBIOS: ncr710: Found controller at 0x%x\n", iobase));

    int i;
    for (i = 0; i < 7; i++) {
        DBG(printf("SeaBIOS: ncr710: Scanning target %d\n", i));
        ncr710_scsi_scan_target(iobase, i);
    }
}

void
ncr710_scsi_setup(void)
{
    ASSERT32FLAT();
    if (!CONFIG_NCR710_SCSI || !runningOnQEMU())
        return;
    if (!CONFIG_PARISC || sizeof(long) != 4 || !lasi_hpa)
        return;
    /* check for PARISC device ID: (HPHW_FIO << 24) | LASI_710_SVERSION */
    if (CONFIG_PARISC && inl(lasi_hpa + 0x6000) != 0x5000082)
        return;
    DBG(printf("Initializing NCR 53c710 SCSI controllers\n"));
    init_ncr710_scsi(lasi_hpa + 0x6000);
}
