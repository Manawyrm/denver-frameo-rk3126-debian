#!/bin/bash
#
# Build the RK915 WiFi driver against the kernel built by
# kernel/build-kernel.sh and package it together with its firmware as a
# Debian package (armhf).
#
# Usage: rk915/build-rk915.sh
#
# Environment: see scripts/common.sh
#
# Result: OUTPUT_DIR/rk915-modules-<kernel release>_<version>_armhf.deb
#

set -euo pipefail

RK915_DIR=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$RK915_DIR/.." && pwd)

. "$TOP/scripts/common.sh"
. "$TOP/kernel/kernel.env"
. "$RK915_DIR/rk915.env"

KERNEL_SRC=$BUILD_DIR/kernel/linux-$LINUX_VERSION
KERNEL_RELEASE=$LINUX_VERSION$LINUX_LOCALVERSION
KERNEL_PKG_VERSION=$LINUX_VERSION-$LINUX_PKG_REVISION
WORK=$BUILD_DIR/rk915
PKG_NAME=rk915-modules-$KERNEL_RELEASE
PKG_VERSION=$RK915_VERSION-$RK915_PKG_REVISION
STAGE=$WORK/$PKG_NAME

check_kernel()
{
	local release

	if [ ! -e "$KERNEL_SRC/Module.symvers" ]
	then
		die "no kernel build in $KERNEL_SRC, run kernel/build-kernel.sh first"
	fi

	release=$(make -s -C "$KERNEL_SRC" LOCALVERSION="$LINUX_LOCALVERSION" kernelrelease)
	if [ "$release" != "$KERNEL_RELEASE" ]
	then
		die "kernel tree is $release, kernel.env says $KERNEL_RELEASE"
	fi
}

build_module()
{
	log "Building rk915.ko for $KERNEL_RELEASE"
	rm -rf "$WORK"
	mkdir -p "$WORK"
	cp -a "$RK915_DIR/driver" "$WORK/driver"

	make -C "$KERNEL_SRC" LOCALVERSION="$LINUX_LOCALVERSION" \
		M="$WORK/driver" -j"$JOBS" modules
}

# maintainer_script NAME ACTION: run depmod for the kernel when dpkg calls
# the script NAME with ACTION (and the kernel is installed)
maintainer_script()
{
	local file=$STAGE/DEBIAN/$1

	{
		echo "#!/bin/sh"
		echo "set -e"
		echo "if [ \"\$1\" = \"$2\" ] && [ -e /usr/lib/modules/$KERNEL_RELEASE/modules.dep ]"
		echo "then"
		echo "	depmod -a $KERNEL_RELEASE"
		echo "fi"
	} > "$file"
	chmod 755 "$file"
}

package()
{
	log "Packaging $PKG_NAME $PKG_VERSION"
	mkdir -p "$STAGE/DEBIAN" \
		"$STAGE/usr/lib/modules/$KERNEL_RELEASE/updates" \
		"$STAGE/usr/lib/firmware/rockchip" \
		"$STAGE/usr/share/doc/$PKG_NAME"

	"${CROSS_COMPILE}strip" --strip-debug -o \
		"$STAGE/usr/lib/modules/$KERNEL_RELEASE/updates/rk915.ko" \
		"$WORK/driver/rk915.ko"
	install -m 644 "$RK915_DIR"/firmware/rk915_*.bin "$STAGE/usr/lib/firmware/rockchip/"
	install -m 644 "$RK915_DIR/driver/LICENSE" "$STAGE/usr/share/doc/$PKG_NAME/copyright"
	install -m 644 "$RK915_DIR/README.md" "$STAGE/usr/share/doc/$PKG_NAME/README.md"

	cat > "$STAGE/DEBIAN/control" <<-EOF
		Package: $PKG_NAME
		Version: $PKG_VERSION
		Architecture: armhf
		Maintainer: Denver Frameo Debian build <builder@denver-frameo>
		Depends: linux-image-$KERNEL_RELEASE (= $KERNEL_PKG_VERSION)
		Provides: rk915-modules
		Section: kernel
		Priority: optional
		Description: Rockchip RK915 SDIO WiFi driver and firmware
		 Out-of-tree driver for the RK915 WiFi chip of the Denver Frameo
		 picture frame, built for kernel $KERNEL_RELEASE, with the chip
		 firmware (rk915_fw.bin, rk915_patch.bin).
	EOF

	maintainer_script postinst configure
	maintainer_script postrm remove

	build_deb "$STAGE"
}

check_tools "${CROSS_COMPILE}gcc" "${CROSS_COMPILE}strip" make dpkg-deb
check_kernel
build_module
package

log "Done"
ls -l "$OUTPUT_DIR/${PKG_NAME}_${PKG_VERSION}_armhf.deb"
