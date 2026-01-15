#!/bin/bash

set -e

# Function to display help message
show_help() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Run QEMU emulation for MIPS-based device firmware.

Options:
    -initrd                Use initrd (CPIO archive) mode instead of rootfs image (might prevent need for sudo)
    -no-pause              Start QEMU running (default is paused, waiting for GDB/monitor)
    -example               Use squashfs-root-example instead of squashfs-root
    -quiet                 No terminal output (default is verbose terminal output)
    -no-logfile            Don't write to log file (default writes to /tmp/qemu.log)
    -capture-kernel-log    Automatically capture kernel log via GDB (writes to /tmp/qemu_kernel.log)
    -kernel-wait TIME      Set wait time for kernel panic in seconds (default: 35, used with -capture-kernel-log)
    -dminimal              Minimal debug logging: unimp,guest_errors (default)
    -dlight                Light debug logging: int,unimp,guest_errors
    -dmedium               Medium debug logging: int,exec,unimp,guest_errors
    -dfull                 Full debug logging: in_asm,int,cpu,unimp,guest_errors
    -dflags FLAGS          Custom debug flags (comma-separated, e.g., "in_asm,int,exec")
    -h, --help             Show this help message and exit

Output Modes:
    Default:        Terminal output + log file (/tmp/qemu.log)
    -quiet:         Log file only (no terminal output)
    -no-logfile:    Terminal output only (no log file)
    -quiet -no-logfile: No output (not recommended)

Modes:
    Default:        Uses rootfs-image as a raw disk drive
    -initrd:        Creates/uses a CPIO archive from squashfs-root as initrd

Examples:
    $(basename "$0")                          # Terminal + log file, minimal debug (default, paused)
    $(basename "$0") -quiet                   # Log file only, minimal debug
    $(basename "$0") -no-logfile              # Terminal only, minimal debug
    $(basename "$0") -dfull                   # Terminal + log, full debug logging
    $(basename "$0") -dflags "exec,int"       # Terminal + log, custom debug flags
    $(basename "$0") -quiet -dlight           # Log file only, light debug logging
    $(basename "$0") -capture-kernel-log      # Auto-capture kernel log via GDB

Notes:
    - QEMU listens on port 1234 for GDB debugging (-s flag)
    - QEMU monitor available on telnet port 4444
    - Serial console output shown in terminal only with -show
    - Logs are written to /tmp/qemu.log
    - Kernel logs (with -capture-kernel-log) written to /tmp/qemu_kernel.log and /tmp/qemu_backtrace.txt
    - Memory is fixed at 64M (required by BIOS)
    - Use 'telnet 127.0.0.1 4444' to access QEMU monitor
    - -capture-kernel-log requires QEMU to start paused (conflicts with -no-pause)

EOF
    exit 0
}


# Parse command line arguments
USE_INITRD=false
START_PAUSED=true
USE_EXAMPLE=false
QUIET_MODE=false       # Default: show terminal output
USE_LOGFILE=true       # Default: write to log file
CAPTURE_KERNEL_LOG=false   # Default: don't capture kernel log
KERNEL_WAIT_TIME=35        # Default wait time for kernel panic

# Debug flags presets
DEBUG_FLAGS="unimp,guest_errors"  # Default: minimal
DEBUG_PRESET_MINIMAL="unimp,guest_errors"
DEBUG_PRESET_LIGHT="int,unimp,guest_errors"
DEBUG_PRESET_MEDIUM="int,exec,unimp,guest_errors"
DEBUG_PRESET_FULL="in_asm,int,cpu,unimp,guest_errors"

while [[ $# -gt 0 ]]; do
    case $1 in
        -initrd)
            USE_INITRD=true
            shift
            ;;
        -no-pause)
            START_PAUSED=false
            shift
            ;;
        -example)
            USE_EXAMPLE=true
            shift
            ;;
        -quiet)
            QUIET_MODE=true
            shift
            ;;
        -no-logfile)
            USE_LOGFILE=false
            shift
            ;;
        -capture-kernel-log)
            CAPTURE_KERNEL_LOG=true
            shift
            ;;
        -kernel-wait)
            if [[ -z "$2" || "$2" == -* ]]; then
                echo "Error: -kernel-wait requires an argument"
                exit 1
            fi
            KERNEL_WAIT_TIME="$2"
            shift 2
            ;;
        -dminimal)
            DEBUG_FLAGS="$DEBUG_PRESET_MINIMAL"
            shift
            ;;
        -dlight)
            DEBUG_FLAGS="$DEBUG_PRESET_LIGHT"
            shift
            ;;
        -dmedium)
            DEBUG_FLAGS="$DEBUG_PRESET_MEDIUM"
            shift
            ;;
        -dfull)
            DEBUG_FLAGS="$DEBUG_PRESET_FULL"
            shift
            ;;
        -dflags)
            if [[ -z "$2" || "$2" == -* ]]; then
                echo "Error: -dflags requires an argument"
                exit 1
            fi
            DEBUG_FLAGS="$2"
            shift 2
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

# Validate flag combinations
if [ "$CAPTURE_KERNEL_LOG" = true ] && [ "$START_PAUSED" = false ]; then
    echo "Error: -capture-kernel-log cannot be used with -no-pause"
    echo "Kernel log capture requires QEMU to start paused for GDB connection."
    exit 1
fi

# --- Configuration Variables ---
if [ "$USE_EXAMPLE" = true ]; then
    SQUASHFS_ROOT="../squashfs-root-example"    # Path to squashfs-root for initrd
else
    SQUASHFS_ROOT="../squashfs-root"            # Path to squashfs-root for initrd
fi

BIOS_IMAGE="../xImage"              # Path to the BIOS image
KERNEL_IMAGE="../Linux-4.4.94+.elf" # Path to extracted ELF kernel
ROOTFS_IMAGE="rootfs-image"         # Path to your created root filesystem image
INITRD_IMAGE="initrd.cpio"          # Path to initrd CPIO archive
QEMU_ARCH="mipsel"                  # Use qemu-system-mipsel (little-endian)
QEMU_BOARD="halley6"                  # A common generic MIPS board. You might need to experiment.
MEMORY_SIZE="64M"                   # Amount of RAM for the emulated system
QEMU_CPU="XBurstR2"                 # CPU type to emulate

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
    # Create initrd if it doesn't exist or is older than squashfs-root
    if [ ! -f "$INITRD_IMAGE" ] || [ "$SQUASHFS_ROOT" -nt "$INITRD_IMAGE" ]; then
        echo "Creating initrd CPIO archive from ${SQUASHFS_ROOT}..."
        cd "$SQUASHFS_ROOT"
        find . | cpio -o -H newc | gzip > "../qemu/${INITRD_IMAGE}.gz"
        cd - > /dev/null
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
    
    MODE_SPECIFIC_ARGS+=(-bios ${BIOS_IMAGE})
    MODE_SPECIFIC_ARGS+=(-drive file="${ROOTFS_IMAGE}",format=raw)
fi

echo "QEMU Board: ${QEMU_BOARD}"
echo "Kernel Cmdline: ${KERNEL_CMDLINE}"
echo ""

# Build pause flag
PAUSE_FLAG=()
if [ "$START_PAUSED" = true ]; then
    PAUSE_FLAG+=(-S)
    echo "QEMU will start PAUSED. Connect via:"
    echo "  - GDB: target remote :1234, then 'continue'"
    echo "  - Monitor: telnet 127.0.0.1 4444, then 'c'"
    echo ""
else
    echo "QEMU will start RUNNING immediately"
    echo "Monitor available at: telnet 127.0.0.1 4444"
    echo ""
fi

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

# Display output configuration
if [ "$QUIET_MODE" = true ] && [ "$USE_LOGFILE" = true ]; then
    echo "Output: Log file only (/tmp/qemu.log)"
elif [ "$QUIET_MODE" = true ] && [ "$USE_LOGFILE" = false ]; then
    echo "Output: None (quiet mode without log file)"
elif [ "$QUIET_MODE" = false ] && [ "$USE_LOGFILE" = true ]; then
    echo "Output: Terminal + log file (/tmp/qemu.log)"
else
    echo "Output: Terminal only"
fi
echo "Debug flags: ${DEBUG_FLAGS}"
echo ""

# Build QEMU command with common arguments
QEMU_CMD=(
    "${QEMU_PATH}"
    -M "${QEMU_BOARD}"
    -cpu ${QEMU_CPU}
    -m ${MEMORY_SIZE}
    -kernel "${KERNEL_IMAGE}"
    "${MODE_SPECIFIC_ARGS[@]}"
    -append "${KERNEL_CMDLINE}"
    -s
    "${PAUSE_FLAG[@]}"
    -serial stdio
    -monitor telnet:127.0.0.1:4444,server,nowait
    -d ${DEBUG_FLAGS}
)

# Execute QEMU with appropriate output redirection
if [ "$CAPTURE_KERNEL_LOG" = true ]; then
    # Special mode: Start QEMU in background and capture kernel log via GDB
    echo ""
    echo "=========================================="
    echo "Automated Kernel Log Capture Mode"
    echo "=========================================="
    echo ""
    echo "This will:"
    echo "  1. Start QEMU in background"
    echo "  2. Connect GDB and capture kernel log"
    echo "  3. Save logs to /tmp/qemu_kernel.log and /tmp/qemu_backtrace.txt"
    echo ""

    # Start QEMU in background
    if [ "$QUIET_MODE" = true ] && [ "$USE_LOGFILE" = true ]; then
        "${QEMU_CMD[@]}" > /tmp/qemu.log 2>&1 &
    elif [ "$QUIET_MODE" = true ] && [ "$USE_LOGFILE" = false ]; then
        "${QEMU_CMD[@]}" > /dev/null 2>&1 &
    elif [ "$QUIET_MODE" = false ] && [ "$USE_LOGFILE" = true ]; then
        "${QEMU_CMD[@]}" 2>&1 | tee /tmp/qemu.log &
    else
        "${QEMU_CMD[@]}" &
    fi

    QEMU_PID=$!

    # Ensure QEMU is killed on script exit
    trap "echo ''; echo 'Cleaning up...'; kill ${QEMU_PID} 2>/dev/null || true" EXIT INT TERM

    # Give QEMU a moment to start
    sleep 2

    # Check if QEMU is still running
    if ! kill -0 ${QEMU_PID} 2>/dev/null; then
        echo "Error: QEMU failed to start or exited prematurely"
        exit 1
    fi

    echo "QEMU started in background (PID: ${QEMU_PID})"
    echo ""

    # Get the directory where this script is located
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

    # Invoke the kernel log capture script with custom wait time
    if [ -f "${SCRIPT_DIR}/capture_kernel_log.sh" ]; then
        ORIGINAL_WAIT=$(grep '^WAIT_TIME=' "${SCRIPT_DIR}/capture_kernel_log.sh" | head -1 | cut -d= -f2 | tr -d ' ')

        # Temporarily modify wait time if different from default
        if [ "${KERNEL_WAIT_TIME}" != "${ORIGINAL_WAIT}" ]; then
            sed -i.bak "s/^WAIT_TIME=.*/WAIT_TIME=${KERNEL_WAIT_TIME}/" "${SCRIPT_DIR}/capture_kernel_log.sh"
        fi

        # Run the capture script
        bash "${SCRIPT_DIR}/capture_kernel_log.sh"

        # Restore original wait time
        if [ "${KERNEL_WAIT_TIME}" != "${ORIGINAL_WAIT}" ]; then
            sed -i.bak "s/^WAIT_TIME=.*/WAIT_TIME=${ORIGINAL_WAIT}/" "${SCRIPT_DIR}/capture_kernel_log.sh"
            rm -f "${SCRIPT_DIR}/capture_kernel_log.sh.bak"
        fi
    else
        echo "Error: capture_kernel_log.sh not found in ${SCRIPT_DIR}"
        kill ${QEMU_PID} 2>/dev/null || true
        exit 1
    fi

    echo ""
    echo "=========================================="
    echo "Capture Complete"
    echo "=========================================="
    echo "Logs available at:"
    echo "  - QEMU debug:   /tmp/qemu.log"
    echo "  - Kernel log:   /tmp/qemu_kernel.log"
    echo "  - Backtrace:    /tmp/qemu_backtrace.txt"
    echo ""

    # Kill QEMU
    kill ${QEMU_PID} 2>/dev/null || true
    wait ${QEMU_PID} 2>/dev/null || true
    echo "QEMU stopped."

else
    # Normal mode: Execute QEMU directly
    if [ "$QUIET_MODE" = true ] && [ "$USE_LOGFILE" = true ]; then
        # Quiet with log file: redirect to log file only
        "${QEMU_CMD[@]}" > /tmp/qemu.log 2>&1
    elif [ "$QUIET_MODE" = true ] && [ "$USE_LOGFILE" = false ]; then
        # Quiet without log file: redirect to /dev/null
        "${QEMU_CMD[@]}" > /dev/null 2>&1
    elif [ "$QUIET_MODE" = false ] && [ "$USE_LOGFILE" = true ]; then
        # Terminal and log file: use tee
        "${QEMU_CMD[@]}" 2>&1 | tee /tmp/qemu.log
    else
        # Terminal only: normal output
        "${QEMU_CMD[@]}"
    fi
fi
