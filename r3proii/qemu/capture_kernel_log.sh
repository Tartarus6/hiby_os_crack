#!/bin/bash

set -e

# Script to automatically capture kernel log buffer from QEMU via GDB
# This automates the manual GDB process documented in the README

# --- Configuration ---
KERNEL_ELF="../Linux-4.4.94+.elf"
GDB_PORT=1234
KERNEL_LOG_OUTPUT="/tmp/qemu_kernel.log"
BACKTRACE_OUTPUT="/tmp/qemu_backtrace.txt"
WAIT_TIME=35#Timetowaitforkernelpanic(seconds)

# Check for gdb-multiarch or gdb
if command -v gdb-multiarch &> /dev/null; then
    GDB_CMD="gdb-multiarch"
elif command -v gdb &> /dev/null; then
    GDB_CMD="gdb"
else
    echo "Error: Neither gdb nor gdb-multiarch found in PATH"
    exit 1
fi

echo "=== QEMU Kernel Log Capture ==="
echo "Kernel ELF: ${KERNEL_ELF}"
echo "GDB command: ${GDB_CMD}"
echo "Wait time: ${WAIT_TIME}s"
echo "Output files:"
echo "  - Kernel log: ${KERNEL_LOG_OUTPUT}"
echo "  - Backtrace: ${BACKTRACE_OUTPUT}"
echo ""

# Create a GDB command file
GDB_COMMANDS=$(mktemp)
trap "rm -f ${GDB_COMMANDS}" EXIT

cat > "${GDB_COMMANDS}" << 'EOF'
# Connect to QEMU
target remote :1234

# Set pagination off so we can capture everything
set pagination off

# Configure logging for kernel log buffer (set file/options BEFORE enabling)
set logging file /tmp/gdb_kernel_log_raw.txt
set logging overwrite on
set logging redirect on
set logging enabled on

# Continue execution
continue

# After panic (or interrupt), extract kernel log buffer
x/2000s __log_buf

# Turn off logging and configure for backtrace
set logging enabled off
set logging file /tmp/gdb_backtrace_raw.txt
set logging overwrite on
set logging redirect on
set logging enabled on

# Get backtrace
bt

set logging enabled off

# Quit GDB
quit
EOF

echo "Connecting GDB to QEMU (port ${GDB_PORT})..."
echo "Waiting for kernel to boot and panic (up to ${WAIT_TIME}s)..."
echo ""

# Run GDB with timeout
# Use SIGINT (Ctrl+C) to interrupt the continue command gracefully
timeout --signal=SIGINT --kill-after=5 ${WAIT_TIME} ${GDB_CMD} -batch -x "${GDB_COMMANDS}" "${KERNEL_ELF}" 2>&1 | tee /tmp/gdb_session.log || {
    EXIT_CODE=$?
    if [ ${EXIT_CODE} -eq 124 ]; then
        echo ""
        echo "Note: Timed out after ${WAIT_TIME}s (continuing with log extraction)"
    elif [ ${EXIT_CODE} -eq 130 ]; then
        echo ""
        echo "Note: Execution interrupted (continuing with log extraction)"
    elif [ ${EXIT_CODE} -eq 0 ]; then
        echo ""
        echo "GDB completed successfully"
    else
        echo ""
        echo "Warning: GDB exited with code ${EXIT_CODE}"
    fi
}

echo ""
echo "Processing captured logs..."

# Process kernel log - extract meaningful content
if [ -f /tmp/gdb_kernel_log_raw.txt ]; then
    # Remove GDB prompts and extract readable kernel messages
    # The log buffer contains addresses followed by string content
    {
        echo "=== Kernel Log Buffer ==="
        echo "Captured at: $(date)"
        echo "Method: Automated GDB extraction from __log_buf"
        echo "=========================================="
        echo ""

        # Extract lines that contain actual log content
        # Format: 0x808f6d67 <__log_buf+15>:	"Linux version 4.4.94+ ..."
        grep -E '0x[0-9a-f]+ <__log_buf' /tmp/gdb_kernel_log_raw.txt | \
            sed 's/^0x[0-9a-f]* <__log_buf[^>]*>:\s*//g' | \
            sed 's/^"\(.*\)"$/\1/g' | \
            grep -v '^\\[0-9][0-9]*$' | \
            grep -v '^[0-9]$' | \
            grep -v '^$' || echo "(No kernel log data captured)"
    } > "${KERNEL_LOG_OUTPUT}"

    echo "Kernel log saved to: ${KERNEL_LOG_OUTPUT}"
else
    echo "Warning: Kernel log capture file not found at /tmp/gdb_kernel_log_raw.txt"
    echo "(No kernel log data captured)" > "${KERNEL_LOG_OUTPUT}"
fi

# Process backtrace
if [ -f /tmp/gdb_backtrace_raw.txt ]; then
    {
        echo "=== Backtrace ==="
        echo "Captured at: $(date)"
        echo "=========================================="
        echo ""
        grep -E '^#[0-9]+' /tmp/gdb_backtrace_raw.txt || echo "No backtrace available (kernel may not have panicked)"
    } > "${BACKTRACE_OUTPUT}"

    echo "Backtrace saved to: ${BACKTRACE_OUTPUT}"
else
    echo "Warning: Backtrace file not found at /tmp/gdb_backtrace_raw.txt"
    {
        echo "=== Backtrace ==="
        echo "Captured at: $(date)"
        echo "=========================================="
        echo ""
        echo "No backtrace available (kernel may not have panicked)"
    } > "${BACKTRACE_OUTPUT}"
fi

# Cleanup temporary files
rm -f /tmp/gdb_kernel_log_raw.txt /tmp/gdb_backtrace_raw.txt /tmp/gdb_session.log

echo ""
echo "=== Capture Complete ==="
echo "Logs written to:"
echo "  ${KERNEL_LOG_OUTPUT}"
echo "  ${BACKTRACE_OUTPUT}"
