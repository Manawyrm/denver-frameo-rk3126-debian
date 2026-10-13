#!/bin/bash
#
# CI build of the SD card image (.github/workflows/main.yml). Runs as root
# in a Debian 13 container (debian:trixie, --privileged: mmdebstrap uses
# its root mode there):
#
#   1. Install the build host packages (README.md, "Build host")
#   2. ./build.sh
#   3. Release files in release/: the xz-compressed SD card image, its bmap
#      and version-info (text for the release notes)
#
# Usage: scripts/ci-build.sh
#
# The host has to register qemu-user in binfmt_misc with the F flag
# (Ubuntu: qemu-user-static) for the armhf programs in the root file
# system, a container cannot do that itself.
#

set -euo pipefail

TOP=$(cd "$(dirname "$0")/.." && pwd)
RELEASE_DIR=$TOP/release

PACKAGES="
	gcc-arm-linux-gnueabihf make bc bison flex perl rsync cpio
	dpkg-dev debhelper fakeroot kmod xz-utils curl ca-certificates git python3
	libssl-dev libelf-dev libdw-dev libssl-dev:armhf
	mmdebstrap arch-test uidmap e2fsprogs dosfstools mtools fdisk bmap-tools
"

install_packages()
{
	local packages=$PACKAGES

	# loaderimage (U-Boot) is an x86-64 binary
	if [ "$(uname -m)" != "x86_64" ]
	then
		packages="$packages qemu-user libc6-amd64-cross"
	fi

	dpkg --add-architecture armhf
	apt-get update -qq
	DEBIAN_FRONTEND=noninteractive apt-get install -yqq $packages

	# The checkout belongs to the runner user, not to root: without this git
	# refuses to work in it (and SOURCE_DATE_EPOCH would fall back to now)
	git config --global --add safe.directory "$TOP"
}

pack_release()
{
	local name

	. "$TOP/kernel/kernel.env"
	. "$TOP/uboot/uboot.env"
	. "$TOP/rk915/rk915.env"
	. "$TOP/rootfs/rootfs.env"

	name=denver-frameo-rk3126-debian-$(git -C "$TOP" describe --tags --always)

	log "Release files $name"
	rm -rf "$RELEASE_DIR"
	mkdir -p "$RELEASE_DIR"
	xz -T0 -c "$OUTPUT_DIR/sdcard.img" > "$RELEASE_DIR/$name.img.xz"
	cp "$OUTPUT_DIR/sdcard.img.bmap" "$RELEASE_DIR/$name.img.bmap"

	cat > "$RELEASE_DIR/version-info" <<-EOF
		Debian $DEBIAN_SUITE armhf for the Denver Frameo picture frame (RK3126B):
		kernel $LINUX_VERSION$LINUX_LOCALVERSION, U-Boot $UBOOT_VERSION, RK915 driver $RK915_VERSION.

		Write it to an SD card (2 GB or more) with
		\`bmaptool copy $name.img.xz /dev/sdX\` (uses the .bmap next to it).
		A \`wpa_supplicant.conf\` and an \`authorized_keys\` file on the FAT
		partition are taken over on boot. Login: root / $ROOT_PASSWORD
	EOF

	ls -l "$RELEASE_DIR"
}

install_packages

. "$TOP/scripts/common.sh"

"$TOP/build.sh"
pack_release
