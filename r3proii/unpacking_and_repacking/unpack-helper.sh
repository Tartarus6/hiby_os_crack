#!/usr/bin/env bash

set -euo pipefail

# Script to extract the root filesystem from the original firmware

# Color Consts (just for decoration)
NC='\033[0m' # No Color
Black='\033[0;30m'        # Black
Red='\033[0;31m'          # Red
Green='\033[0;32m'        # Green
Yellow='\033[0;33m'       # Yellow
Blue='\033[0;34m'         # Blue
Purple='\033[0;35m'       # Purple
Cyan='\033[0;36m'         # Cyan
White='\033[0;37m'        # White

PROJECT_ROOT=$(git rev-parse --show-toplevel)
FIRMWARE_DIR="${PROJECT_ROOT}/r3proii/firmware"
UNPACKING_AND_REPACKING_DIR="${PROJECT_ROOT}/r3proii/unpacking_and_repacking"
SQUASHFS_OUT_ROOT="${UNPACKING_AND_REPACKING_DIR}/squashfs-root"
XIMAGE_OUT_PATH="${UNPACKING_AND_REPACKING_DIR}/xImage"

echo -e "${Red}##########################"
echo -e "### SELECTING FIRMWARE ###"
echo -e "##########################${NC}"
echo -e ""

# Find all .upt files in original and custom folders
ORIGINAL_DIR="${FIRMWARE_DIR}/original"
CUSTOM_DIR="${FIRMWARE_DIR}/custom"

mapfile -t ORIGINAL_FW < <(find "$ORIGINAL_DIR" -maxdepth 1 -type f -name "*.upt" | sort)
mapfile -t CUSTOM_FW   < <(find "$CUSTOM_DIR"   -maxdepth 1 -type f -name "*.upt" | sort)

declare -a ALL_FW
index=1

echo -e "Original"

for ((i=0; i<${#ORIGINAL_FW[@]}; i++)); do
    file="${ORIGINAL_FW[$i]}"
    ALL_FW[$index]="$file"

    if [ $i -eq $((${#ORIGINAL_FW[@]}-1)) ]; then
        prefix="└──"
    else
        prefix="├──"
    fi

    echo -e "$prefix ${Red}$index)${NC} ${Cyan}$(basename "$file")${NC}"
    ((index++))
done

echo -e
echo -e "Custom"

if [ ${#CUSTOM_FW[@]} -eq 0 ]; then
    echo -e "└── ${Cyan}[No custom firmwares loaded...] ${Red}(Place any custom firmwares in ${CUSTOM_DIR})${NC}"
else
    for ((i=0; i<${#CUSTOM_FW[@]}; i++)); do
        file="${CUSTOM_FW[$i]}"
        ALL_FW[$index]="$file"

        if [ $i -eq $((${#CUSTOM_FW[@]}-1)) ]; then
            prefix="└──"
        else
            prefix="├──"
        fi

        echo -e "$prefix ${Red}$index${NC}) ${Cyan}$(basename "$file")${NC}"
        ((index++))
    done
fi

echo -e

while true; do
    read -rp "$(echo -e "Select firmware ${Yellow}[1-$((index-1))]${NC}: ")" choice

    if [[ "$choice" =~ ^[0-9]+$ ]] && [ -n "${ALL_FW[$choice]}" ]; then
        FW_PATH="${ALL_FW[$choice]}"
        break
    fi

    echo -e "Invalid selection."
done


# run the unpacking script
"${PROJECT_ROOT}/scripts/unpack.sh" -i "${FW_PATH}" -k "${XIMAGE_OUT_PATH}" -o "${SQUASHFS_OUT_ROOT}"
