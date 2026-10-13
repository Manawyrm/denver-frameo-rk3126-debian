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
# Environment (all optional):
#   BUILD_DIR      work directory            (default: <project>/build)
#   OUTPUT_DIR     where the .deb files go   (default: <project>/output)
#   DOWNLOAD_DIR   source tarball cache      (default: <project>/downloads)
#   JOBS           parallel make jobs        (default: nproc)
#   CROSS_COMPILE  toolchain prefix          (default: arm-linux-gnueabihf-)
#   SOURCE_DATE_EPOCH  build timestamp for reproducible builds
#                  (default: last commit of the project, else now)
#
# Result: OUTPUT_DIR/linux-image-<release>_<version>_armhf.deb, plus the
# headers and linux-libc-dev packages.
#

set -euo pipefail

KERNEL_DIR=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$KERNEL_DIR/.." && pwd)

. "$KERNEL_DIR/kernel.env"

BUILD_DIR=${BUILD_DIR:-$TOP/build}
OUTPUT_DIR=${OUTPUT_DIR:-$TOP/output}
DOWNLOAD_DIR=${DOWNLOAD_DIR:-$TOP/downloads}
JOBS=${JOBS:-$(nproc)}
CROSS_COMPILE=${CROSS_COMPILE:-arm-linux-gnueabihf-}

SRC=$BUILD_DIR/kernel/linux-$LINUX_VERSION
TARBALL=$DOWNLOAD_DIR/linux-$LINUX_VERSION.tar.xz
FRAGMENT=$KERNEL_DIR/config/rk3126-frame.config
PKG_VERSION=$LINUX_VERSION-$LINUX_PKG_REVISION

export ARCH=arm
export CROSS_COMPILE

log()
{
	echo
	echo "=== $* ==="
}

die()
{
	echo "ERROR: $*" >&2
	exit 1
}

check_tools()
{
	local missing=""
	local tool

	for tool in "${CROSS_COMPILE}gcc" make bc bison flex perl rsync cpio \
		dpkg-buildpackage dpkg-architecture fakeroot xz sha256sum curl
	do
		if ! command -v "$tool" > /dev/null
		then
			missing="$missing $tool"
		fi
	done

	if [ -n "$missing" ]
	then
		die "missing tools:$missing (see README.md for the Debian packages)"
	fi
}

fetch_source()
{
	log "Kernel source $LINUX_VERSION"
	mkdir -p "$DOWNLOAD_DIR"

	if [ ! -e "$TARBALL" ]
	then
		curl -fsSL --retry 3 -o "$TARBALL.part" "$LINUX_URL"
		mv "$TARBALL.part" "$TARBALL"
	fi

	if ! echo "$LINUX_SHA256  $TARBALL" | sha256sum -c --quiet
	then
		rm -f "$TARBALL"
		die "checksum mismatch for $TARBALL (deleted, run again)"
	fi
}

prepare_source()
{
	local p
	local d

	log "Unpacking and patching"
	rm -rf "$SRC"
	mkdir -p "$BUILD_DIR/kernel"
	tar -xf "$TARBALL" -C "$BUILD_DIR/kernel"

	for p in "$KERNEL_DIR"/patches/*.patch
	do
		echo "Applying $(basename "$p")"
		patch -d "$SRC" -p1 --batch --forward --no-backup-if-mismatch -s < "$p"
	done

	# Board device trees, built and packaged like the in-tree ones
	for d in "$KERNEL_DIR"/dts/*.dts
	do
		cp "$d" "$SRC/arch/arm/boot/dts/rockchip/"
		echo "dtb-\$(CONFIG_ARCH_ROCKCHIP) += $(basename "$d" .dts).dtb" \
			>> "$SRC/arch/arm/boot/dts/rockchip/Makefile"
	done
}

# Every option of the fragment must have its value in the final .config:
# Kconfig silently drops options whose dependencies are not met.
check_config()
{
	local errors=0
	local line
	local name
	local value
	local actual

	while read -r line
	do
		case "$line" in
			CONFIG_*=*)
				name=${line%%=*}
				value=${line#*=}
				;;
			*)
				continue
				;;
		esac

		actual=$(grep -E "^$name=|^# $name is not set" "$SRC/.config" || true)
		if [ "$value" = "n" ]
		then
			if [ -n "$actual" ] && [ "$actual" != "# $name is not set" ]
			then
				echo "  $name: wanted n, got ${actual#*=}"
				errors=$((errors + 1))
			fi
		elif [ "$actual" != "$name=$value" ]
		then
			echo "  $name: wanted $value, got ${actual:-nothing}"
			errors=$((errors + 1))
		fi
	done < "$FRAGMENT"

	if [ $errors -gt 0 ]
	then
		die "$errors option(s) of $(basename "$FRAGMENT") not applied"
	fi
	echo "All options of $(basename "$FRAGMENT") applied."
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
	check_config
}

build_packages()
{
	local timestamp

	if [ -z "${SOURCE_DATE_EPOCH:-}" ]
	then
		SOURCE_DATE_EPOCH=$(git -C "$TOP" log -1 --format=%ct 2> /dev/null || date +%s)
	fi
	export SOURCE_DATE_EPOCH
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

check_tools
fetch_source
prepare_source
configure
build_packages

log "Done"
ls -l "$OUTPUT_DIR"/*.deb
