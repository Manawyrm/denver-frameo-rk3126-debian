#!/bin/bash
#
# Build the Linux kernel for the Denver Frameo picture frame (RK3126B) as
# Debian packages (armhf):
#
#   1. Download the pinned kernel.org release (kernel.env), check SHA-256
#   2. Unpack it into a fresh source tree
#   3. Apply patches/*.patch, add dts/*.dts to the Rockchip device trees
#   4. multi_v7_defconfig + config/rk3126-frame.config, check that every
#      option of the fragment made it into the final .config
#   5. make bindeb-pkg
#
# Usage: kernel/build-kernel.sh
#
# Environment: see scripts/common.sh
#
# Result: OUTPUT_DIR/linux-image-<release>_<version>_armhf.deb, plus the
# -dbg, headers and linux-libc-dev packages.
#

set -euo pipefail

KERNEL_DIR=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$KERNEL_DIR/.." && pwd)

. "$TOP/scripts/common.sh"
. "$KERNEL_DIR/kernel.env"

SRC=$BUILD_DIR/kernel/linux-$LINUX_VERSION
TARBALL=$DOWNLOAD_DIR/linux-$LINUX_VERSION.tar.xz
FRAGMENT=$KERNEL_DIR/config/rk3126-frame.config
PKG_VERSION=$LINUX_VERSION-$LINUX_PKG_REVISION

prepare_source()
{
	local d

	log "Unpacking and patching"
	rm -rf "$SRC"
	mkdir -p "$BUILD_DIR/kernel"
	tar -xf "$TARBALL" -C "$BUILD_DIR/kernel"

	apply_patches "$KERNEL_DIR/patches" "$SRC"

	# Board device trees, built and packaged like the in-tree ones
	for d in "$KERNEL_DIR"/dts/*.dts
	do
		cp "$d" "$SRC/arch/arm/boot/dts/rockchip/"
		echo "dtb-\$(CONFIG_ARCH_ROCKCHIP) += $(basename "$d" .dts).dtb" \
			>> "$SRC/arch/arm/boot/dts/rockchip/Makefile"
	done
}

configure()
{
	log "Configuring"
	make -C "$SRC" multi_v7_defconfig > /dev/null
	(
		cd "$SRC"
		scripts/kconfig/merge_config.sh -m .config "$FRAGMENT" > /dev/null
	)
	make -C "$SRC" olddefconfig > /dev/null
	check_config "$FRAGMENT" "$SRC/.config"
}

build_packages()
{
	local timestamp

	timestamp=$(date -u -d "@$SOURCE_DATE_EPOCH" "+%Y-%m-%d %H:%M:%S UTC")

	log "Building Debian packages $PKG_VERSION"
	make -C "$SRC" -j"$JOBS" \
		LOCALVERSION="$LINUX_LOCALVERSION" \
		KDEB_PKGVERSION="$PKG_VERSION" \
		KBUILD_DEBARCH=armhf \
		KBUILD_BUILD_USER=builder \
		KBUILD_BUILD_HOST=denver-frameo \
		KBUILD_BUILD_TIMESTAMP="$timestamp" \
		bindeb-pkg

	# bindeb-pkg puts the packages next to the source tree
	mkdir -p "$OUTPUT_DIR"
	mv "$BUILD_DIR"/kernel/*.deb "$OUTPUT_DIR/"
	rm -f "$BUILD_DIR"/kernel/*.buildinfo "$BUILD_DIR"/kernel/*.changes
}

check_tools "${CROSS_COMPILE}gcc" make bc bison flex perl rsync cpio \
	dpkg-buildpackage dpkg-architecture fakeroot xz sha256sum curl

log "Kernel source $LINUX_VERSION"
fetch_file "$LINUX_URL" "$TARBALL" "$LINUX_SHA256"
prepare_source
configure
build_packages

log "Done"
ls -l "$OUTPUT_DIR"/linux-*"$PKG_VERSION"*.deb
