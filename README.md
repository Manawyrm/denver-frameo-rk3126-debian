# Debian for the Denver Frameo picture frame (RK3126B)

Reproducible build of the software for the Denver Frameo picture frame
("F606C" board, Rockchip RK3126B, 800x1280 MIPI DSI panel, GSL3673 touch,
RK915 WiFi), meant to run unattended in CI: kernel, WiFi driver, boot
loader, a Debian 13 root file system and the SD card image, all without root
privileges.

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
                           boot partition handling)
  build-rootfs.sh          builds the root file system tarball
image/
  image.env                SD card layout
  build-image.sh           assembles the SD card image
```

Build order:

```
kernel/build-kernel.sh
rk915/build-rk915.sh       # needs the kernel tree
uboot/build-uboot.sh
rootfs/build-rootfs.sh     # needs the packages of the three above
image/build-image.sh
```

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
downloads the Rockchip DDR init, miniloader and `loaderimage` tool from the
pinned rkbin commit (each checked against its SHA-256), applies
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
BootROM -> Rockchip DDR init + miniloader -> U-Boot. `loaderimage` only
exists as an x86-64 binary; on other build hosts it runs through
qemu-user. The boot environment (`uboot/config/rk3126-sdboot.env`) loads
the kernel from the FAT boot partition (`uEnv.txt`, rescue initramfs or
Debian with `rootdev`).

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
  the boot settings for U-Boot.
- **WiFi**: `/etc/network/interfaces.d/wlan0` (DHCP) with
  `/etc/wpa_supplicant/wpa_supplicant.conf`.
- **Configuration from the boot partition** (like Raspberry Pi OS,
  `boot-config.service`): a `wpa_supplicant.conf` put on the FAT partition
  becomes `/etc/wpa_supplicant/wpa_supplicant.conf`, the keys of an
  `authorized_keys` file are added to `/root/.ssh/authorized_keys`. Both
  files are removed from the boot partition afterwards.
- **USB gadget**: serial console (`ttyGS0`) and Ethernet `usb0` with the
  static address of `interfaces.d/usb0` (10.23.99.5/16).
- **First boot**: the root partition grows to the size of the card, SSH
  host keys and the machine ID are generated (the image has none).
- Root login (also via SSH) with the password from `rootfs.env`.
- The eMMC (stock Android) is kept read-only by a udev rule.

## SD card image

```
image/build-image.sh
```

| Where | What |
|---|---|
| sector 64 | `idbloader.img` (DDR init + miniloader) |
| sector 0x4000 (8 MiB) | `uboot.img` |
| partition 1, 16 MiB, 256 MiB | FAT32 `BOOT`: kernel, device trees, `uEnv.txt` |
| partition 2, 1.5 GiB | ext4 `rootfs`, grows on the first boot |

`mke2fs -d` builds the ext4 file system straight from the tarball (owners
and permissions included, no root needed), `mtools` the FAT partition.
Result: `output/sdcard.img` and `output/sdcard.img.bmap`:

```
bmaptool copy output/sdcard.img /dev/sdX
```

## Build host

Debian 13 (trixie), any architecture, with armhf as foreign architecture:

```
dpkg --add-architecture armhf
apt-get update
apt-get install gcc-arm-linux-gnueabihf make bc bison flex perl rsync cpio \
	dpkg-dev debhelper fakeroot kmod xz-utils curl git python3 \
	libssl-dev libelf-dev libdw-dev libssl-dev:armhf \
	mmdebstrap uidmap e2fsprogs dosfstools mtools fdisk bmap-tools
```

`rootfs/build-rootfs.sh` needs user namespaces with subuids for the build
user (`/etc/subuid`, `/etc/subgid`) or root.

`libssl-dev:armhf` is needed for the ARM build tools in the headers package.
On build hosts other than ARM also `qemu-user-binfmt` (to run armhf
programs in the root file system), on build hosts other than x86-64
`qemu-user` and `libc6-amd64-cross` (for `loaderimage`).
