#ifndef __NCR710_SCSI_H
#define __NCR710_SCSI_H

// NCR 53c710 SCSI definitions
//
// Copyright (C) Soumyajyotii Ssarkar <soumyajyotisarkar23@gmail.com> 2025 QEMU project
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

int ncr710_scsi_process_op(struct disk_op_s *op);
void ncr710_scsi_setup(void);

#endif // __NCR710_SCSI_H
