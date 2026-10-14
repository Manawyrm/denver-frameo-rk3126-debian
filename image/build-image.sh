#!/bin/bash
#
# Assemble the SD card or eMMC image of the Denver Frameo picture frame from
# the root file system tarball and the boot loader, without root privileges:
#
#   sector 64       idbloader.img (Rockchip DDR init + miniloader)
#   sector 0x4000   uboot.img (U-Boot)
#   partition 1     FAT32 "BOOT": /boot/firmware of the root file system
#                   (zImage, device trees, uEnv.txt)
#   partition 2     ext4 "rootfs": the root file system
#
# Usage: image/build-image.sh [sd|emmc]
#
#   sd    SD card image (default): root on /dev/mmcblk1, device tree
#         rk3126-denver-frame-sdroot.dtb (the SD card gets the SDMMC_D0 pin
#         shared with UART2_TX, so no serial console under Linux)
#   emmc  eMMC image: root on /dev/mmcblk0, device tree
#         rk3126-denver-frame.dtb (UART stays usable), without the udev rule
#         that keeps the eMMC read-only
#
# Both use the same root file system tarball, only uEnv.txt, fstab and
# growroot.service differ (and the IDs of disk and file systems, so an SD
# card and an eMMC written from these images can be told apart).
#
# Needs: OUTPUT_DIR/rootfs.tar (rootfs/build-rootfs.sh), OUTPUT_DIR/
# idbloader.img and uboot.img (uboot/build-uboot.sh)
#
# Environment: see scripts/common.sh
#
# Result: OUTPUT_DIR/sdcard.img or emmc.img, each with a .bmap. Write it with
# "bmaptool copy sdcard.img /dev/sdX" (only the used blocks) or dd.
#

set -euo pipefail

IMAGE_DIR=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$IMAGE_DIR/.." && pwd)

. "$TOP/scripts/common.sh"
. "$IMAGE_DIR/image.env"

export PATH=$PATH:/usr/sbin:/sbin

TARGET=${1:-sd}

case "$TARGET" in
sd)
	IMG_NAME=sdcard
	ROOT_DISK=/dev/mmcblk1
	ROOT_FDT=rk3126-denver-frame-sdroot.dtb
	;;
emmc)
	IMG_NAME=emmc
	ROOT_DISK=/dev/mmcblk0
	ROOT_FDT=rk3126-denver-frame.dtb
	;;
*)
	die "unknown target $TARGET (sd or emmc)"
	;;
esac

ROOTFS_TAR=$OUTPUT_DIR/rootfs.tar
WORK=$BUILD_DIR/image-$TARGET
IMG=$OUTPUT_DIR/$IMG_NAME.img

# Files of the root file system that name the boot medium (/dev/mmcblk1 in
# the overlay)
MEDIUM_FILES="./etc/fstab ./etc/systemd/system/growroot.service"

# Fixed IDs derived from the build timestamp and the target: same image for
# the same commit, different IDs for SD card and eMMC
VOLUME_ID=$(echo "boot-$TARGET-$SOURCE_DATE_EPOCH" | sha256sum | cut -c1-8)
DISK_ID=0x$(echo "disk-$TARGET-$SOURCE_DATE_EPOCH" | sha256sum | cut -c1-8)
ROOT_UUID=$(echo "rootfs-$TARGET-$SOURCE_DATE_EPOCH" | sha256sum | \
	sed -E 's/^(.{8})(.{4})(.{4})(.{4})(.{12}).*/\1-\2-\3-\4-\5/')

check_inputs()
{
	local f

	for f in "$ROOTFS_TAR" "$OUTPUT_DIR/idbloader.img" "$OUTPUT_DIR/uboot.img"
	do
		if [ ! -e "$f" ]
		then
			die "$f missing, build it first (see README.md)"
		fi
	done
}

build_boot_partition()
{
	local uenv=$WORK/boot/boot/firmware/uEnv.txt

	log "Boot partition (FAT32, $BOOT_SIZE_MB MiB)"
	rm -rf "$WORK"
	mkdir -p "$WORK/boot"
	tar -C "$WORK/boot" -xf "$ROOTFS_TAR" ./boot/firmware

	sed -i -e "s|^rootdev=.*|rootdev=${ROOT_DISK}p2|" \
		-e "s|^rootfdtfile=.*|rootfdtfile=$ROOT_FDT|" "$uenv"
	grep -E "^(rootdev|rootfdtfile)=" "$uenv"

	mkfs.vfat -F 32 -n BOOT -i "$VOLUME_ID" -C "$WORK/boot.vfat" \
		$((BOOT_SIZE_MB * 1024)) > /dev/null
	mcopy -m -i "$WORK/boot.vfat" "$WORK/boot/boot/firmware/"* ::
	mdir -i "$WORK/boot.vfat" -b ::
}

build_root_partition()
{
	local members

	log "Root partition (ext4, $ROOT_SIZE_MB MiB)"

	# Without the files of /boot/firmware: there the boot partition is
	# mounted
	cp "$ROOTFS_TAR" "$WORK/root.tar"
	members=$(tar -tf "$WORK/root.tar" | grep "^./boot/firmware/." || true)
	if [ -n "$members" ]
	then
		tar --delete -f "$WORK/root.tar" $members
	fi

	adapt_to_medium

	# libarchive refuses non-ASCII file names in the tarball unless the
	# locale is UTF-8 (C.UTF-8 is built into glibc, always available)
	LC_ALL=C.UTF-8 E2FSPROGS_FAKE_TIME=$SOURCE_DATE_EPOCH mke2fs -q -t ext4 -L rootfs \
		-U "$ROOT_UUID" -E "hash_seed=$ROOT_UUID" \
		-d "$WORK/root.tar" "$WORK/root.ext4" "${ROOT_SIZE_MB}M"
	rm "$WORK/root.tar"
}

# Root and boot partition on ROOT_DISK in fstab and growroot.service;
# without the eMMC read-only rule if the root file system is on the eMMC
adapt_to_medium()
{
	local dir=$WORK/medium

	mkdir -p "$dir"
	tar -C "$dir" -xf "$WORK/root.tar" $MEDIUM_FILES
	(
		cd "$dir"
		sed -i "s|/dev/mmcblk1|$ROOT_DISK|g" $MEDIUM_FILES
	)
	tar --delete -f "$WORK/root.tar" $MEDIUM_FILES
	tar -C "$dir" --owner=0 --group=0 --mtime="@$SOURCE_DATE_EPOCH" \
		-rf "$WORK/root.tar" $MEDIUM_FILES
	grep -h "$ROOT_DISK" "$dir/etc/fstab" "$dir/etc/systemd/system/growroot.service"

	if [ "$TARGET" = emmc ]
	then
		tar --delete -f "$WORK/root.tar" ./etc/udev/rules.d/60-emmc-readonly.rules
	fi
}

assemble()
{
	local boot_start=$((BOOT_START_MB * 2048))
	local boot_sectors=$((BOOT_SIZE_MB * 2048))
	local root_start=$((boot_start + boot_sectors))
	local root_sectors=$((ROOT_SIZE_MB * 2048))
	local total_mb=$((BOOT_START_MB + BOOT_SIZE_MB + ROOT_SIZE_MB))

	log "$TARGET image $IMG_NAME.img ($total_mb MiB)"
	rm -f "$IMG" "$IMG.bmap"
	truncate -s "${total_mb}M" "$IMG"

	sfdisk -q "$IMG" <<-EOF
		label: dos
		label-id: $DISK_ID
		start=$boot_start, size=$boot_sectors, type=c, bootable
		start=$root_start, size=$root_sectors, type=83
	EOF

	dd if="$OUTPUT_DIR/idbloader.img" of="$IMG" seek="$IDBLOADER_SECTOR" conv=notrunc status=none
	dd if="$OUTPUT_DIR/uboot.img" of="$IMG" seek="$UBOOT_SECTOR" conv=notrunc status=none
	dd if="$WORK/boot.vfat" of="$IMG" bs=1M seek="$BOOT_START_MB" conv=notrunc status=none
	dd if="$WORK/root.ext4" of="$IMG" bs=1M seek=$((BOOT_START_MB + BOOT_SIZE_MB)) \
		conv=notrunc,sparse status=none

	bmaptool -q create -o "$IMG.bmap" "$IMG"
	sfdisk -l "$IMG"
}

check_tools tar mkfs.vfat mcopy mdir mke2fs sfdisk bmaptool
check_inputs
build_boot_partition
build_root_partition
assemble

log "Done"
ls -ls "$IMG" "$IMG.bmap"
