#!/bin/bash
#
# Build the Debian root file system of the Denver Frameo picture frame
# (armhf) as a tarball, without root privileges:
#
#   1. mmdebstrap (unshare mode) of rootfs.env's suite and packages
#   2. overlay/ on top (network, USB gadget, boot partition handling),
#      owned by root
#   3. Install our packages from OUTPUT_DIR: kernel, RK915 driver, boot
#      loader. The kernel hook puts zImage and device trees into
#      /boot/firmware, which becomes the FAT boot partition.
#   4. Root password, services; no SSH host keys (generated on the first
#      boot, every frame gets its own)
#
# Usage: rootfs/build-rootfs.sh
#
# Needs: the packages of kernel/build-kernel.sh, rk915/build-rk915.sh and
# uboot/build-uboot.sh in OUTPUT_DIR; mmdebstrap, user namespaces with
# subuids for the user (or run as root), qemu-user-binfmt on non-ARM hosts.
#
# Environment: see scripts/common.sh
#
# Result: OUTPUT_DIR/rootfs.tar
#

set -euo pipefail

ROOTFS_DIR=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$ROOTFS_DIR/.." && pwd)

. "$TOP/scripts/common.sh"
. "$ROOTFS_DIR/rootfs.env"
. "$TOP/kernel/kernel.env"
. "$TOP/rk915/rk915.env"
. "$TOP/uboot/uboot.env"

KERNEL_RELEASE=$LINUX_VERSION$LINUX_LOCALVERSION
APT_CACHE=$DOWNLOAD_DIR/apt-cache
TARBALL=$OUTPUT_DIR/rootfs.tar
OVERLAY_TAR=$BUILD_DIR/rootfs/overlay.tar

DEBS="
	$OUTPUT_DIR/linux-image-${KERNEL_RELEASE}_${LINUX_VERSION}-${LINUX_PKG_REVISION}_armhf.deb
	$OUTPUT_DIR/rk915-modules-${KERNEL_RELEASE}_${RK915_VERSION}-${RK915_PKG_REVISION}_armhf.deb
	$OUTPUT_DIR/u-boot-denver-frameo_${UBOOT_VERSION}-${UBOOT_PKG_REVISION}_armhf.deb
"

SERVICES="usb-gadget.service growroot.service boot-config.service ssh-host-keys.service
	serial-getty@ttyGS0.service"

check_debs()
{
	local deb

	for deb in $DEBS
	do
		if [ ! -e "$deb" ]
		then
			die "$deb missing, build it first (see README.md)"
		fi
	done
}

build_rootfs()
{
	local include
	local debs_in_chroot=""
	local deb

	include=$(echo $ROOTFS_PACKAGES | tr ' ' ',')
	for deb in $DEBS
	do
		debs_in_chroot="$debs_in_chroot /tmp/debs/$(basename "$deb")"
	done

	log "Debian $DEBIAN_SUITE armhf root file system"
	mkdir -p "$APT_CACHE" "$(dirname "$OVERLAY_TAR")"
	rm -f "$TARBALL"

	# The overlay owned by root, not writable for group/others. As a tarball:
	# in unshare mode the hooks cannot read the user's files, copy-in can.
	tar -C "$ROOTFS_DIR/overlay" --owner=0 --group=0 --mode=go-w \
		--sort=name --mtime="@$SOURCE_DATE_EPOCH" -cf "$OVERLAY_TAR" .

	# Downloaded packages are kept in APT_CACHE for the next build
	mmdebstrap --mode=auto --architectures=armhf --variant=minbase \
		--include="$include" \
		--skip=essential/unlink \
		--setup-hook='mkdir -p "$1/var/cache/apt/archives"' \
		--setup-hook="sync-in $APT_CACHE /var/cache/apt/archives" \
		--customize-hook='mkdir -p "$1/tmp/debs"' \
		--customize-hook="copy-in $OVERLAY_TAR /tmp" \
		--customize-hook='tar -C "$1" -xf "$1/tmp/overlay.tar" && rm "$1/tmp/overlay.tar"' \
		--customize-hook="copy-in $(echo $DEBS) /tmp/debs" \
		--customize-hook="chroot \"\$1\" env FRAME_IMAGE_BUILD=1 DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends $debs_in_chroot" \
		--customize-hook='rm -rf "$1/tmp/debs"' \
		--customize-hook="echo root:$ROOT_PASSWORD | chroot \"\$1\" chpasswd" \
		--customize-hook="chroot \"\$1\" systemctl enable $(echo $SERVICES)" \
		--customize-hook='rm -f "$1"/etc/ssh/ssh_host_*' \
		--customize-hook='echo "nameserver 8.8.8.8" > "$1/etc/resolv.conf"' \
		--customize-hook="sync-out /var/cache/apt/archives $APT_CACHE" \
		--customize-hook='rm -f "$1"/var/cache/apt/archives/*.deb' \
		"$DEBIAN_SUITE" "$TARBALL" "$DEBIAN_MIRROR"
}

check_tools mmdebstrap tar
check_debs
build_rootfs

log "Done"
ls -l "$TARBALL"
