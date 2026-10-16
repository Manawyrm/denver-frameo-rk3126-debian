# Debian for the Denver Frameo picture frame (RK3126B)

Reproducible build of the software for the Denver Frameo picture frame
("F606C" board, Rockchip RK3126B, 800x1280 MIPI DSI panel, GSL3673 touch,
RK915 WiFi), meant to run unattended in CI: kernel, WiFi driver, boot
loader, a Debian 13 root file system and the images for the SD card and the
internal eMMC, all without root privileges.

## Layout

```
scripts/common.sh          shared helpers (downloads with checksums, config
                           fragment check, packaging, settings below)
kernel/
  kernel.env               pinned kernel release (version, URL, SHA-256),
                           kernel release suffix and package revision
  config/rk3126-frame.config
                           config fragment on top of multi_v7_defconfig
  patches/                 kernel patches, applied in order
  dts/                     device trees of the frame, built in-tree
  build-kernel.sh          builds the Debian kernel packages
rk915/
  driver/                  RK915 WiFi driver source (see rk915/README.md)
  firmware/                RK915 firmware from the stock Android image
  rk915.env                driver package version and revision
  build-rk915.sh           builds the driver + firmware package
uboot/
  uboot.env                pinned U-Boot (git tag + commit) and rkbin
                           commit with SHA-256 of each Rockchip binary
  config/                  config fragment and default environment
  patches/                 U-Boot patches
  rk3126-generic.dts       U-Boot device tree (with the eFuse for the MAC)
  build-uboot.sh           builds the boot loader images and package
rootfs/
  rootfs.env               Debian release, mirror, packages, root password
  overlay/                 files on top of Debian (network, USB gadget,
                           boot partition handling, touchscreen firmware)
  build-rootfs.sh          builds the root file system tarball
image/
  image.env                image layout
  build-image.sh           assembles the SD card or eMMC image
```

Build order:

```
kernel/build-kernel.sh
rk915/build-rk915.sh       # needs the kernel tree
uboot/build-uboot.sh
rootfs/build-rootfs.sh     # needs the packages of the three above
image/build-image.sh sd    # SD card image
image/build-image.sh emmc  # eMMC image
```

`./build.sh` runs all of them in this order.

Settings for all scripts via the environment: `BUILD_DIR`, `OUTPUT_DIR`,
`DOWNLOAD_DIR`, `JOBS`, `CROSS_COMPILE`, `SOURCE_DATE_EPOCH` (default: the
last commit, so every build of a commit is the same).

Generated (not in git): `downloads/` (source tarballs), `build/` (work
trees), `output/` (packages).

## Kernel

```
kernel/build-kernel.sh
```

Downloads the pinned kernel.org tarball (checked against its SHA-256),
unpacks a fresh tree, applies `kernel/patches/`, adds the frame device trees,
configures `multi_v7_defconfig` plus `kernel/config/rk3126-frame.config` and
fails if any option of the fragment did not end up in the final `.config`.
Then `make bindeb-pkg` builds:

```
output/linux-image-7.2.10-rk3126_7.2.10-1_armhf.deb
output/linux-image-7.2.10-rk3126-dbg_7.2.10-1_armhf.deb
output/linux-headers-7.2.10-rk3126_7.2.10-1_armhf.deb
output/linux-libc-dev_7.2.10-1_armhf.deb
```

The config fragment enables the Hantro VPU driver (`hantro-vpu` module,
`/dev/video1` decoder, `/dev/video0` JPEG encoder) with only the media core
and the memory-to-memory drivers. Measured with GStreamer (`v4l2slh264dec`
and friends, kiosk stopped): H.264 1080p 33 fps, 720p H.264/VP8/MPEG-2
55-64 fps, at 15-20 % CPU.

The image package contains the kernel, the modules and the device trees
(`/usr/lib/linux-image-7.2.10-rk3126/rk3126-denver-frame*.dtb`). The `-dbg`
package has the debug symbols (`vmlinux` and modules) for crash analysis.
The headers package carries ARM binaries of the kernel build tools, so
out-of-tree modules can be built on the frame itself.

Raise `LINUX_PKG_REVISION` in `kernel/kernel.env` whenever config, patches
or device trees change.

### Patches

| Patch | Why |
|---|---|
| 0001 dw-mipi-dsi: clock lane in LP in command mode | The JD9365 panel only starts its DSI receiver when it sees LP-11 on the clock lane after its reset; without it the display stays black. |
| 0002 mmc: quirks for the RK915 SDIO WiFi | Short CISTPL_FUNCE, continuous SDIO clock, CMD52 waits for DAT, matched by `compatible = "rockchip,rk915"`. From the RK915 driver port. |
| 0003 dw_mmc: survive an SDIO card that stops responding | Masks stray FIFO interrupts, software data timeout for SDIO writes (from PR #1 of the RK915 driver port, write timeout limited to SDIO). |
| 0004 ASoC: RK3128/RK3126 internal codec | Playback driver for the internal codec (`inno_rk3128`), work in progress: the output is heavily distorted. |
| 0005 clk: rk3128: reset like the Rockchip kernel | Restart with APLL, CPLL and GPLL in slow mode and the second global reset. With mainline's first global reset the Rockchip DDR init failed its memory test ("W FF != R") in about every second warm reboot after U-Boot had raised the DDR clock to 456 MHz; with this sequence 12 of 12 reboots went through. |

## RK915 WiFi driver

```
rk915/build-rk915.sh
```

Needs the kernel build tree of `kernel/build-kernel.sh` (the tools in the
headers package are ARM binaries). Builds the module and packages it with
the firmware:

```
output/rk915-modules-7.2.10-rk3126_8.0.0-1_armhf.deb
```

The package depends on exactly the kernel package it was built against and
runs `depmod` on installation. Origin and changes of the driver:
`rk915/README.md`.

With the same commit (or the same `SOURCE_DATE_EPOCH`) the driver package
is byte-identical between builds (checked). Both scripts use fixed build
timestamps; the kernel packages have not been checked for that yet.

## U-Boot

```
uboot/build-uboot.sh
```

Clones the pinned U-Boot tag (and checks that it is the pinned commit),
downloads the `loaderimage` tool from the pinned rkbin commit, checks it and
the stock DDR init + miniloader (`uboot/blobs/`) against their SHA-256,
applies
`uboot/patches/`, configures `evb-rk3128_defconfig` plus
`uboot/config/rk3126-sdboot.config` (checked like the kernel fragment) and
builds:

```
output/idbloader.img       DDR init + miniloader, for sector 64
output/uboot.img           U-Boot for the miniloader, for sector 0x4000
output/u-boot-denver-frameo_2026.10-1_armhf.deb
                           both in /usr/lib/u-boot/denver-frameo/
```

There is no mainline DDR init (TPL) for the RK3126, so the boot chain is
BootROM -> Rockchip DDR init + miniloader -> U-Boot.

`idbloader.img` is the one of the stock firmware (DDR init 2.08, miniloader
2.59), copied unchanged from sector 64 of the frame's eMMC. The miniloader
of rkbin (2.63) does not work here: it only starts U-Boot through a trust
OS (OP-TEE, `trust.img`). Without one it searches for it, gives up and
switches to Rockchip USB download mode, which never shows up on the host
with this board. The stock miniloader runs into the same failed trust
search and then starts U-Boot directly.

`loaderimage` only
exists as an x86-64 binary; on other build hosts it runs through
qemu-user. The boot environment (`uboot/config/rk3126-sdboot.env`) loads
the kernel from the FAT boot partition (`uEnv.txt`, rescue initramfs or
Debian with `rootdev`), first from the SD card (`sdboot`), then from the
eMMC (`emmcboot`). A bootable SD card always wins, so it is also the way
back from a broken eMMC installation.

### U-Boot patches

| Patch | |
|---|---|
| 0001 | clock driver: SD card and eMMC clocks of the RK3128 |
| 0002 | dw_mmc: honour `bus-width = <1>` (SD card D1 is UART2_RX) |
| 0003 | `dramtest` command |
| 0004 | enable the I-cache: with `CONFIG_SKIP_LOWLEVEL_INIT` start.S never did, U-Boot ran 6-8 times slower in code with many instructions per byte |
| 0005 | `ddrfreq` command |

### DDR clock

The Rockchip DDR init leaves the DDR3 at 300 MHz (DDR3-600). `ddrfreq MHz`
changes the clock from U-Boot; it is a port of the frequency change of the
Rockchip 3.10 kernel (`ddr_rk3126b.c`). The change runs from the 8 KiB
on-chip SRAM with the MMU off: DRAM into self-refresh, new DPLL dividers,
PCTL/NoC timings, PHY settings and DRAM mode registers, data training,
back. When the training or a quick DRAM check at the new clock fails, the
SRAM code switches back to the old clock. `ddrfreq check` compares the
timings computed for the current clock with the controller registers
(no difference to the Rockchip DDR init at 300 MHz). `ddrfreq -v` prints
progress letters from the SRAM code.

`dramtest` writes the whole range (by default all DRAM below U-Boot) before
it reads it back, with the data cache cleaned and invalidated in between,
so the DRAM is tested and not the cache. `dramtest -u` runs with the data
cache off.

Results on the frame (one `dramtest` pass over 974 MiB takes ~2 minutes):

| DDR clock | CL/CWL | Result | memcpy |
|---|---|---|---|
| 300 MHz (DDR init) | 6/5 | ok | 457 MB/s |
| 396 MHz (stock firmware) | 6/5 | ok | 573 MB/s |
| 456 MHz | 8/6 | ok: 5 `dramtest` passes and an uncached one; Linux for 40 min with `cpu-verify` (3 threads), memtester (400 MiB, 11 of its tests) and the kiosk running | 639 MB/s |
| 528 MHz | 8/6 | U-Boot crashes after the change | |

The boot environment sets `ddr_mhz=456` right before it starts Debian; an
`ddr_mhz=` line in `uEnv.txt` overrides it (empty: stay at 300 MHz). The
rescue initramfs always runs at 300 MHz.

Raise `UBOOT_PKG_REVISION` in `uboot/uboot.env` whenever patches, device
tree, config or environment change.

## Root file system

```
rootfs/build-rootfs.sh
```

`mmdebstrap` (unshare mode, no root needed) builds Debian 13 (trixie) armhf,
variant minbase plus the packages of `rootfs/rootfs.env` (ifupdown with
dhcpcd, openssh-server, wpasupplicant, htop, nano, wget, curl, ...), puts
`rootfs/overlay/` on top and installs the kernel, RK915 and U-Boot packages
from `output/`. Result: `output/rootfs.tar`.

- **Boot partition like on a Raspberry Pi**: the FAT partition is mounted
  on `/boot/firmware`. On every kernel package installation
  `/etc/kernel/postinst.d/zz-frame-boot-partition` copies `zImage` and the
  device trees there, so `apt` kernel updates work. `uEnv.txt` there holds
  the boot settings for U-Boot. `/etc/kernel/postinst.d/depmod` rebuilds
  the module index: the kernel package ships its own `modules.dep`, which
  would drop the RK915 driver (`updates/rk915.ko`) on every kernel update.
- **WiFi**: `/etc/network/interfaces.d/wlan0` (DHCP) with
  `/etc/wpa_supplicant/wpa_supplicant.conf`.
- **Configuration from the boot partition** (like Raspberry Pi OS,
  `boot-config.service`): a `wpa_supplicant.conf` put on the FAT partition
  becomes `/etc/wpa_supplicant/wpa_supplicant.conf`, the keys of an
  `authorized_keys` file are added to `/root/.ssh/authorized_keys`. Both
  files are removed from the boot partition afterwards.
- **Touchscreen firmware**: the GSL3673 needs
  `/usr/lib/firmware/silead/gsl3673-denver-frame.fw` (from the stock
  Android image), otherwise `silead_ts` does not probe.
- **USB gadget**: serial console (`ttyGS0`) and Ethernet `usb0` with the
  static address of `interfaces.d/usb0` (10.23.99.5/16).
- **First boot**: the root partition grows to the size of the card, SSH
  host keys and the machine ID are generated (the image has none).
- **No age-based cleanup of `/tmp`** (`/etc/tmpfiles.d/tmp.conf`): without
  an RTC the frame boots with a clock months in the past, after the NTP
  jump the cleanup would delete everything created at boot (e.g.
  `/tmp/.X11-unix`, Xwayland then fails for non-root users).
- Root login (also via SSH) with the password from `rootfs.env`.
- The eMMC (stock Android) is kept read-only by a udev rule (only in the SD
  card image).

## SD card and eMMC image

```
image/build-image.sh sd
image/build-image.sh emmc
```

Both images contain the same root file system and boot loader:

| Where | What |
|---|---|
| sector 64 | `idbloader.img` (stock DDR init + miniloader) |
| sector 0x4000 (8 MiB) | `uboot.img` |
| partition 1, 16 MiB, 256 MiB | FAT32 `BOOT`: kernel, device trees, `uEnv.txt` |
| partition 2, 1.5 GiB | ext4 `rootfs`, grows on the first boot |

They differ in where the root file system is (`uEnv.txt`, `/etc/fstab`,
`growroot.service`), the device tree and the IDs of disk and file systems:

| | `sd` | `emmc` |
|---|---|---|
| Result | `output/sdcard.img` | `output/emmc.img` |
| Root | `/dev/mmcblk1p2` | `/dev/mmcblk0p2` |
| Device tree | `rk3126-denver-frame-sdroot.dtb`: the SD card gets the SDMMC_D0 pin shared with UART2_TX, no serial console under Linux | `rk3126-denver-frame.dtb`: UART2 works, the SD card is only usable in U-Boot |
| eMMC | read-only (udev rule) | root file system |

`mke2fs -d` builds the ext4 file system straight from the tarball (owners
and permissions included, no root needed; it loads `libarchive13t64` at
run time), `mtools` the FAT partition. Each image comes with a `.bmap`:

```
bmaptool copy output/sdcard.img /dev/sdX
```

The eMMC image replaces the stock Android firmware. One way to install it:
boot the SD card image, then

```
blockdev --setrw /dev/mmcblk0
xzcat emmc.img.xz | dd of=/dev/mmcblk0 bs=4M
```

and reboot without the SD card.

The boot ROM tries the eMMC before the SD card. If the loader on the eMMC is
broken (it starts, but does not get to U-Boot), the SD card is never used:
short the eMMC clock to ground while switching on and release it after
about a second. The boot ROM then falls back to the SD card, and its
miniloader brings up the eMMC again. From the SD card system, write a
working loader back:

```
blockdev --setrw /dev/mmcblk0
dd if=/usr/lib/u-boot/denver-frameo/idbloader.img of=/dev/mmcblk0 seek=64 conv=fsync
```

## Build host

Debian 13 (trixie), any architecture, with armhf as foreign architecture:

```
dpkg --add-architecture armhf
apt-get update
apt-get install gcc-arm-linux-gnueabihf make bc bison flex perl rsync cpio \
	dpkg-dev debhelper fakeroot kmod xz-utils curl ca-certificates git python3 \
	libssl-dev libelf-dev libdw-dev libssl-dev:armhf \
	mmdebstrap arch-test uidmap e2fsprogs libarchive13t64 dosfstools mtools \
	fdisk bmap-tools
```

`rootfs/build-rootfs.sh` needs user namespaces with subuids for the build
user (`/etc/subuid`, `/etc/subgid`) or root.

`libssl-dev:armhf` is needed for the ARM build tools in the headers package.
On build hosts other than ARM also `qemu-user-binfmt` (to run armhf
programs in the root file system), on build hosts other than x86-64
`qemu-user` and `libc6-amd64-cross` (for `loaderimage`).

## CI

`.github/workflows/main.yml` builds the image on GitHub (new branches and
tags, pull requests): on an Ubuntu runner with `qemu-user-static` (binfmt
for armhf) it runs `scripts/ci-build.sh` as root in a `debian:trixie`
container. That script installs the build host packages above, runs
`./build.sh` and packs `release/`: the xz-compressed SD card and eMMC
images, their bmaps and the release notes (`version-info`). For a tag these become a GitHub
release.
