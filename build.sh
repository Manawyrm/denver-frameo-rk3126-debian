#!/bin/bash
#
# Build everything in order: kernel packages, RK915 driver, boot loader,
# root file system, SD card and eMMC image (see README.md).
#
# Usage: ./build.sh
#
# Environment: see scripts/common.sh
#
# Result: OUTPUT_DIR/sdcard.img and emmc.img with their .bmap files, plus
# the packages
#

set -euo pipefail

TOP=$(cd "$(dirname "$0")" && pwd)

"$TOP/kernel/build-kernel.sh"
"$TOP/rk915/build-rk915.sh"
"$TOP/uboot/build-uboot.sh"
"$TOP/rootfs/build-rootfs.sh"
"$TOP/image/build-image.sh" sd
"$TOP/image/build-image.sh" emmc
