#!/usr/bin/env bash

set -euo pipefail

# Color Consts (just for decoration)
NC=$'\033[0m' # No Color
Black=$'\033[0;30m'        # Black
Red=$'\033[0;31m'          # Red
Green=$'\033[0;32m'        # Green
Yellow=$'\033[0;33m'       # Yellow
Blue=$'\033[0;34m'         # Blue
Purple=$'\033[0;35m'       # Purple
Cyan=$'\033[0;36m'         # Cyan
White=$'\033[0;37m'        # White

TEMP_DIR="/tmp/hiby-modding"

show_help() {
    cat <<EOF
${Green}Usage:${NC} $(basename "$0") \
${Cyan}-i${NC} ${Yellow}SQUASHFS_ROOT${NC} \
${Cyan}-k${NC} ${Yellow}XIMAGE${NC} \
${Cyan}-o${NC} ${Yellow}OUTPUT_FILE${NC}

${Green}Options:${NC}
  ${Cyan}-i${NC} DIR     squashfs-root directory   ${Red}(required)${NC}
  ${Cyan}-k${NC} FILE    xImage                    ${Red}(required)${NC}
  ${Cyan}-o${NC} FILE    Output .upt file          ${Red}(required)${NC}
  ${Cyan}-h${NC}         Show this help
EOF
}

error() {
    echo -e "${Red}Error: $1${NC}" >&2
    echo >&2
    show_help >&2
    exit 1
}

input_root=""
input_ximage=""
output_file=""

while getopts ":i:k:o:h" opt; do
    case "$opt" in
        i) input_root="$OPTARG" ;;
        k) input_ximage="$OPTARG" ;;
        o) output_file="$OPTARG" ;;
        h)
            show_help
            exit 0
            ;;
        :)
            error "Option -$OPTARG requires an argument."
            ;;
        \?)
            error "Unknown option: -$OPTARG"
            ;;
    esac
done

[[ -n "$input_root" ]]   || error "Input directory is required."
[[ -n "$input_ximage" ]] || error "xImage is required."
[[ -n "$output_file" ]]  || error "Output file is required."

[[ -d "$input_root" ]]   || error "'$input_root' does not exist."
[[ -f "$input_ximage" ]] || error "'$input_ximage' does not exist."

# convert to absolute paths
input_root=$(realpath "$input_root")
input_ximage=$(realpath "$input_ximage")
output_file=$(realpath "$output_file")

# check if output already exists, ask if it should be overwritten
if [[ -e "$output_file" ]]; then
    echo
    echo -e "${Yellow}Output file '$output_file' already exists.${NC}"
    read -rp "Overwrite it? [y/N] " reply

    case "$reply" in
        [yY]|[yY][eE][sS])
            rm -f "$output_file"
            ;;
        *)
            echo "Aborted."
            exit 0
            ;;
    esac
fi

rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR/ota_v0"

pushd "$TEMP_DIR/ota_v0" >/dev/null

echo "${Yellow}#####################################"
echo "### GENERATING NEW SQUASHFS FILES ###"
echo "#####################################${NC}"
echo ""

# Build squashfs filesystem (preserving original owners/permissions)
mksquashfs "${input_root}" rootfs.squashfs -comp lzo -all-root

# Get metadata of the full rootfs.squashfs
rootfs_size=$(stat -c%s rootfs.squashfs)
rootfs_md5=$(md5sum rootfs.squashfs | awk '{print $1}')

# Split rootfs.squashfs into 512k chunks
split rootfs.squashfs -d -a 4 -b 512k rootfs.squashfs.

# Remove the full squashfs file so it's not packaged in the final image
rm rootfs.squashfs

# Hashing and renaming rootfs chunks
# Each chunk is suffixed with the MD5 of the previous chunk (or the full squashfs for chunk 0)
md5=$rootfs_md5
ota_md5_rootfs="ota_md5_rootfs.squashfs.${rootfs_md5}"
> "${ota_md5_rootfs}"

for part in $(ls rootfs.squashfs.[0-9]* | sort); do
    md5next=$(md5sum "${part}" | awk '{print $1}')
    echo "${md5next}" >> "${ota_md5_rootfs}"
    mv "${part}" "${part}.${md5}"
    md5="${md5next}"
done

echo "${Yellow}###################################"
echo "### GENERATING NEW xImage FILES ###"
echo "###################################${NC}"
echo ""

# Copy xImage to staging to process
cp "${input_ximage}" xImage

# Get metadata of the full xImage
ximage_size=$(stat -c%s xImage)
ximage_md5=$(md5sum xImage | awk '{print $1}')

# Split xImage into 512k chunks
split xImage -d -a 4 -b 512k xImage.

# Remove the full xImage file so it's not packaged in the final image
rm xImage

# Hashing and renaming xImage chunks
# Each chunk is suffixed with the MD5 of the previous chunk (or the full xImage for chunk 0)
md5=$ximage_md5
ota_md5_xImage="ota_md5_xImage.${ximage_md5}"
> "${ota_md5_xImage}"

for part in $(ls xImage.[0-9]* | sort); do
    md5next=$(md5sum "${part}" | awk '{print $1}')
    echo "${md5next}" >> "${ota_md5_xImage}"
    mv "${part}" "${part}.${md5}"
    md5="${md5next}"
done

echo "${Yellow}#################################"
echo "### GENERATING METADATA FILES ###"
echo "#################################${NC}"
echo ""

# Generate ota_update.in dynamically
cat > ota_update.in <<- EOM
ota_version=0

img_type=kernel
img_name=xImage
img_size=${ximage_size}
img_md5=${ximage_md5}

img_type=rootfs
img_name=rootfs.squashfs
img_size=${rootfs_size}
img_md5=${rootfs_md5}
EOM

# Generate ota_v0.ok
echo > ota_v0.ok

# Generate ota_config.in in parent directory (temp/)
echo "current_version=0" > ../ota_config.in

echo "${Yellow}#################################"
echo "### GENERATING FIRMWARE FILE ###"
echo "#################################${NC}"
echo ""

mkisofs -o "${output_file}" -J -r "${TEMP_DIR}"

popd >/dev/null

rm -rf "$TEMP_DIR"

echo ""
echo "Repacking complete!"
echo "Firmware image saved as ${OUT_PKG}"
echo ""
echo "Now you can flash this to the device"
