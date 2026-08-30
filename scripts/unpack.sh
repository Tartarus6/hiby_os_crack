#!/usr/bin/env bash

set -euo pipefail

# Color Consts (just for decoration)
NC=$'\033[0m'              # No Color
Black=$'\033[0;30m'        # Black
Red=$'\033[0;31m'          # Red
Green=$'\033[0;32m'        # Green
Yellow=$'\033[0;33m'       # Yellow
Blue=$'\033[0;34m'         # Blue
Purple=$'\033[0;35m'       # Purple
Cyan=$'\033[0;36m'         # Cyan
White=$'\033[0;37m'        # White

TEMP_DIR='/tmp/hiby-modding/' # Directory where in-progress unpacking/repacking files are stored

show_help() {
    cat <<EOF
${Green}Usage:${NC} $(basename "$0") ${Cyan}-i${NC} ${Yellow}INPUT_FILE${NC} ${Cyan}-o${NC} ${Yellow}OUTPUT_DIRECTORY${NC}

${Green}Options:${NC}
  ${Cyan}-i${NC} FILE    Input .upt file          ${Red}(required)${NC}
  ${Cyan}-k${NC} FILE    Output xImage file       ${Red}(required)${NC}
  ${Cyan}-o${NC} DIR     Output directory         ${Red}(required)${NC}
  ${Cyan}-h${NC}         Show this help message

${Green}Example:${NC}
  $(basename "$0") -i ./r1.upt -k ./xImage -o ./unpacked-squashfs-root
EOF
}

error() {
    echo -e "${Red}Error: $1${NC}" >&2
    echo -e >&2
    show_help >&2
    exit 1
}

input_file=""
output_ximage=""
output_dir=""

while getopts ":i:k:o:h" opt; do
    case "$opt" in
        i)
            input_file="$OPTARG"
            ;;
        k)
            output_ximage="$OPTARG"
            ;;
        o)
            output_dir="$OPTARG"
            ;;
        h)
            show_help
            exit 0
            ;;
        :)
            error "Option ${Cyan}-$OPTARG${NC} requires an argument."
            ;;
        \?)
            error "Unknown option: ${Cyan}-$OPTARG${NC}"
            ;;
    esac
done

# Ensure required arguments are present
[[ -n "$input_file" ]] || error "Input file is required."
[[ -n "$output_ximage" ]] || error "Out ximage is required."
[[ -n "$output_dir" ]] || error "Output directory is required."

# Ensure input file exists
[[ -f "$input_file" ]] || error "Input file '$input_file' does not exist."

# Check whether either output already exists
if [[ -e "$output_dir" || -e "$output_ximage" ]]; then
    echo
    echo -e "${Red}One or more output files already exist:${NC}"

    [[ -e "$output_dir" ]]    && echo "  - ${Cyan}${output_dir}${NC}"
    [[ -e "$output_ximage" ]] && echo "  - ${Cyan}${output_ximage}${NC}"

    read -rp "Overwrite them? ${Yellow}[y/N]${NC} " reply

    case "$reply" in
        [yY]|[yY][eE][sS])
            [[ -e "$output_dir" ]]    && rm -rf "$output_dir"
            [[ -e "$output_ximage" ]] && rm -f "$output_ximage"
            ;;
        *)
            echo "Aborted."
            exit 0
            ;;
    esac
fi

# Create the output directory
mkdir -p "$output_dir"

# Convert input and output into absolute path
input_file=$(realpath "$input_file")
output_dir=$(realpath "$output_dir")

if [[ -d $TEMP_DIR ]]; then
	rm -rf $TEMP_DIR  # clean up old temp folder in case it exists (can happen if a script fails to finish)
fi

mkdir -p $TEMP_DIR

pushd $TEMP_DIR > /dev/null
7z x $input_file  # extract the contents of the firmware iso image

pushd ota_v0 > /dev/null

cat rootfs.squashfs.* > rootfs.squashfs  # combine the squashfs file parts into one
cat xImage.* > "${output_ximage}"  # combine the xImage file parts into one

echo "${Red}##################################"
echo "### EXTRACTING SQUASHFS-ROOTFS ###"
echo "##################################${NC}"
echo ""

popd > /dev/null # go back to the starting directory

# extracting the file system
unsquashfs -d $output_dir "${TEMP_DIR}ota_v0/rootfs.squashfs"

rm -r $TEMP_DIR  # clean up temp folder

echo ""
echo "Unpacking complete!"
echo "Original filesystem extracted to: ${output_dir}"
echo "xImage extracted to: ${output_ximage}"
echo ""
echo "${Red}Now you can modify files in ${output_dir}/${NC}"
