#!/bin/bash

set -e

# Function to display help message
show_help() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Run QEMU emulation for MIPS-based device firmware.

Options:
    -initrd         Use initrd (CPIO archive) mode instead of rootfs image (might prevent need for sudo)
    -h, --help      Show this help message and exit

Modes:
    Default:        Uses rootfs-image as a raw disk drive
    -initrd:        Creates/uses a CPIO archive from squashfs-root as initrd

Examples:
    $(basename "$0")                    # Run with rootfs-image
    $(basename "$0") -initrd            # Run with initrd

Notes:
    - QEMU starts paused (-S flag) and listens on port 1234 for GDB debugging (-s flag)
    - Logs are written to /tmp/qemu.log
    - Memory is fixed at 64M (required by BIOS)

EOF
    exit 0
}

# Parse command line arguments
USE_INITRD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -initrd)
            USE_INITRD=true
            shift
            ;;
        -h|--help)
            show_help
            ;;
        *)
            echo "Error: Unknown option: $1"
            echo "Use -h or --help for usage information."
            exit 1
            ;;
    esac
done

# --- Get project root and set up absolute paths ---
PROJECT_ROOT=$(git rev-parse --show-toplevel)
QEMU_DIR="${PROJECT_ROOT}/r3proii/qemu"

# --- Configuration Variables ---
BIOS_IMAGE="${PROJECT_ROOT}/r3proii/xImage"                                    # Path to the BIOS image
KERNEL_IMAGE="${PROJECT_ROOT}/r3proii/Linux-4.4.94+.elf"                       # Path to extracted ELF kernel
ROOTFS_IMAGE="${QEMU_DIR}/rootfs-image"                                        # Path to your created root filesystem image
SQUASHFS_ROOT="${PROJECT_ROOT}/r3proii/unpacking_and_repacking/squashfs-root"  # Path to squashfs-root for initrd
INITRD_IMAGE="${QEMU_DIR}/initrd.cpio"                                         # Path to initrd CPIO archive
QEMU_ARCH="mipsel"                                                             # Use qemu-system-mipsel (little-endian)
QEMU_BOARD="malta"                                                             # A common generic MIPS board. You might need to experiment.
MEMORY_SIZE="64M"                                                              # Amount of RAM for the emulated system
QEMU_CPU="XBurstR2"                                                            # CPU type to emulate

# QEMU path (adjust if qemu-system-mips is not in your PATH)
QEMU_PATH=$(which qemu-system-${QEMU_ARCH})

# Check if QEMU is found
if [ -z "$QEMU_PATH" ]; then
    echo "Error: qemu-system-${QEMU_ARCH} not found in PATH."
    echo "Please ensure QEMU is installed and the correct MIPS system emulator is available."
    exit 1
fi

# Kernel command line arguments
# - rw: Mount root filesystem read-write.
# - init=/sbin/init: Specifies the first process to run (adjust if your init is elsewhere).
# - console=ttyS0: Routes kernel messages and a login prompt to the serial port (MIPS uses ttyS0).
KERNEL_CMDLINE="rw init=/sbin/init mem=${MEMORY_SIZE} earlyprintk debug"

# --- Prepare mode-specific options ---
MODE_SPECIFIC_ARGS=()

if [ "$USE_INITRD" = true ]; then
    # Check that squashfs-root exists
    if [ ! -d "$SQUASHFS_ROOT" ]; then
        echo "Error: Initrd source directory '$SQUASHFS_ROOT' not found!"
        echo "To fix this, run unpack.sh in unpacking_and_repacking"
        exit 1
    fi

    # Create initrd if it doesn't exist or is older than squashfs-root
    if [ ! -f "$INITRD_IMAGE" ] || [ "$SQUASHFS_ROOT" -nt "$INITRD_IMAGE" ]; then
        echo "Creating initrd CPIO archive from ${SQUASHFS_ROOT}..."
        cd "$SQUASHFS_ROOT"
        echo "Sudo is required due to the permissions of some files in squashfs-root"
        sudo find . | sudo cpio -o -H newc | gzip > "${INITRD_IMAGE}.gz"
        cd "$QEMU_DIR"
        mv "${INITRD_IMAGE}.gz" "${INITRD_IMAGE}"
        echo "Initrd created: ${INITRD_IMAGE}"
    fi

    echo "Starting QEMU for ${QEMU_ARCH} (initrd mode)..."
    echo "Kernel: ${KERNEL_IMAGE}"
    echo "Initrd: ${INITRD_IMAGE}"
    
    MODE_SPECIFIC_ARGS+=(-initrd "${INITRD_IMAGE}")
else
    echo "Starting QEMU for ${QEMU_ARCH} (rootfs mode)..."
    echo "Kernel: ${KERNEL_IMAGE}"
    echo "RootFS: ${ROOTFS_IMAGE}"
    
    MODE_SPECIFIC_ARGS+=(-bios "${BIOS_IMAGE}")
    MODE_SPECIFIC_ARGS+=(-drive file="${ROOTFS_IMAGE}",format=raw)
fi

# Change to QEMU directory to ensure relative paths work correctly
cd "$QEMU_DIR"

echo "QEMU Board: ${QEMU_BOARD}"
echo "Kernel Cmdline: ${KERNEL_CMDLINE}"
echo ""

# Note: the -s and -S options are for debugging (start QEMU paused and listen on port 1234)
#       remove them if you don't need gdb debugging.

# -M sets the machine type (board)
# -cpu sets the CPU type (architecture)
# -m sets the memory size (i think our bios is expecting specifically 64M, in my tests it crashes with anything else)
# -bios specifies the BIOS image to use (i dont think it actually matters here since we are loading our own kernel)
# -kernel specifies the kernel image to load (must be an ELF file for QEMU to load it directly, for some reason)
# -initrd specifies the initrd image (used only in initrd mode)
# -append passes the kernel command line arguments
# -drive specifies the root filesystem image (format=raw for raw disk images, used only in rootfs mode)
# -d sets the debug options (log various events)
# -D specifies the log file location (remove -D to log to terminal)

"${QEMU_PATH}" \
    -M "${QEMU_BOARD}" \
    -cpu ${QEMU_CPU} \
    -m ${MEMORY_SIZE} \
    -kernel "${KERNEL_IMAGE}" \
    "${MODE_SPECIFIC_ARGS[@]}" \
    -append "${KERNEL_CMDLINE}" \
    -s \
    -S \
    -serial stdio \
    -d in_asm,int,cpu,unimp,guest_errors 2>&1 | tee /tmp/qemu.log