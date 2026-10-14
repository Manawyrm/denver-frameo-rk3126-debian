#!/bin/bash
#
# Build U-Boot and the Rockchip boot loader images for the Denver Frameo
# picture frame (RK3126B) and package them as a Debian package:
#
#   1. Clone the pinned U-Boot tag (uboot.env), check the commit
#   2. Download DDR init, miniloader and loaderimage from the pinned rkbin
#      commit, check their SHA-256
#   3. Apply patches/*.patch, add the device tree and boot environment
#   4. evb-rk3128_defconfig + config/rk3126-sdboot.config, check that every
#      option of the fragment made it into the final .config
#   5. Build u-boot.bin, pack idbloader.img (DDR init + miniloader, for
#      sector 64) and uboot.img (U-Boot for the miniloader, for sector
#      0x4000)
#
# Usage: uboot/build-uboot.sh
#
# Environment: see scripts/common.sh
#
# Result: OUTPUT_DIR/idbloader.img, OUTPUT_DIR/uboot.img and
# OUTPUT_DIR/u-boot-denver-frameo_<version>_armhf.deb
#

set -euo pipefail

UBOOT_DIR=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$UBOOT_DIR/.." && pwd)

. "$TOP/scripts/common.sh"
. "$UBOOT_DIR/uboot.env"

GIT_CACHE=$DOWNLOAD_DIR/u-boot-$UBOOT_TAG
RKBIN=$DOWNLOAD_DIR/rkbin-$RKBIN_COMMIT
WORK=$BUILD_DIR/uboot
SRC=$WORK/u-boot-$UBOOT_VERSION
OBJ=$WORK/obj
FRAGMENT=$UBOOT_DIR/config/rk3126-sdboot.config
PKG_NAME=u-boot-denver-frameo
PKG_VERSION=$UBOOT_VERSION-$UBOOT_PKG_REVISION
STAGE=$WORK/$PKG_NAME

# U-Boot is linked to this address, the miniloader loads it there
UBOOT_LOAD_ADDR=0x60000000

fetch_sources()
{
	log "U-Boot $UBOOT_TAG and rkbin ${RKBIN_COMMIT:0:12}"
	fetch_git "$UBOOT_GIT" "$UBOOT_TAG" "$UBOOT_COMMIT" "$GIT_CACHE"
	fetch_file "$RKBIN_URL/$RKBIN_DDR" "$RKBIN/$RKBIN_DDR" "$RKBIN_DDR_SHA256"
	fetch_file "$RKBIN_URL/$RKBIN_MINILOADER" "$RKBIN/$RKBIN_MINILOADER" "$RKBIN_MINILOADER_SHA256"
	fetch_file "$RKBIN_URL/$RKBIN_LOADERIMAGE" "$RKBIN/$RKBIN_LOADERIMAGE" "$RKBIN_LOADERIMAGE_SHA256"
	chmod +x "$RKBIN/$RKBIN_LOADERIMAGE"
}

prepare_source()
{
	log "Preparing the source tree"
	rm -rf "$WORK"
	mkdir -p "$SRC"
	git -C "$GIT_CACHE" archive HEAD | tar -x -C "$SRC"

	apply_patches "$UBOOT_DIR/patches" "$SRC"

	cp "$UBOOT_DIR/rk3126-generic.dts" "$SRC/dts/upstream/src/arm/rockchip/"
	cp "$UBOOT_DIR/config/rk3126-sdboot.env" "$SRC/board/rockchip/evb_rk3128/"
}

configure()
{
	log "Configuring"
	make -C "$SRC" O="$OBJ" evb-rk3128_defconfig > /dev/null
	"$SRC/scripts/kconfig/merge_config.sh" -m -O "$OBJ" "$OBJ/.config" "$FRAGMENT" > /dev/null
	make -C "$SRC" O="$OBJ" olddefconfig > /dev/null
	check_config "$FRAGMENT" "$OBJ/.config"
}

build()
{
	log "Building U-Boot"
	make -C "$SRC" O="$OBJ" -j"$JOBS"

	log "Packing the boot loader images"
	mkdir -p "$OUTPUT_DIR"

	# DDR init + miniloader, written to sector 64
	"$OBJ/tools/mkimage" -n rk3128 -T rksd \
		-d "$RKBIN/$RKBIN_DDR:$RKBIN/$RKBIN_MINILOADER" "$OUTPUT_DIR/idbloader.img"

	# U-Boot in the format the miniloader expects (2 copies of 1 MiB),
	# written to sector 0x4000
	run_x86 "$RKBIN/$RKBIN_LOADERIMAGE" --pack --uboot "$OBJ/u-boot.bin" \
		"$OUTPUT_DIR/uboot.img" "$UBOOT_LOAD_ADDR" --size 1024 2 > /dev/null
}

package()
{
	local dir=$STAGE/usr/lib/u-boot/denver-frameo

	log "Packaging $PKG_NAME $PKG_VERSION"
	mkdir -p "$STAGE/DEBIAN" "$dir" "$STAGE/usr/share/doc/$PKG_NAME"
	install -m 644 "$OUTPUT_DIR/idbloader.img" "$OUTPUT_DIR/uboot.img" "$dir/"
	install -m 644 "$SRC/Licenses/README" "$STAGE/usr/share/doc/$PKG_NAME/copyright"

	cat > "$STAGE/DEBIAN/control" <<-EOF
		Package: $PKG_NAME
		Version: $PKG_VERSION
		Architecture: armhf
		Maintainer: Denver Frameo Debian build <builder@denver-frameo>
		Section: admin
		Priority: optional
		Description: U-Boot and Rockchip loader for the Denver Frameo picture frame
		 Boot loader images for the RK3126B picture frame, in
		 /usr/lib/u-boot/denver-frameo/:
		 idbloader.img (Rockchip DDR init + miniloader, sector 64) and
		 uboot.img (U-Boot $UBOOT_VERSION for the miniloader, sector 0x4000).
		 They are not written to the boot medium automatically.
	EOF

	build_deb "$STAGE"
}

TOOLS="${CROSS_COMPILE}gcc make bison flex python3 git curl sha256sum dpkg-deb"
if [ "$(uname -m)" != "x86_64" ]
then
	TOOLS="$TOOLS qemu-x86_64"
fi
check_tools $TOOLS

fetch_sources
prepare_source
configure
build
package

log "Done"
ls -l "$OUTPUT_DIR/idbloader.img" "$OUTPUT_DIR/uboot.img" \
	"$OUTPUT_DIR/${PKG_NAME}_${PKG_VERSION}_armhf.deb"
