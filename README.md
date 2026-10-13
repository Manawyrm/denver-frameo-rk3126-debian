# Debian for the Denver Frameo picture frame (RK3126B)

Reproducible build of the software for the Denver Frameo picture frame
("F606C" board, Rockchip RK3126B, 800x1280 MIPI DSI panel, GSL3673 touch,
RK915 WiFi), meant to run unattended in CI. Work in progress: so far it
builds the kernel.

## Layout

```
kernel/
  kernel.env               pinned kernel release (version, URL, SHA-256),
                           kernel release suffix and package revision
  config/rk3126-frame.config
                           config fragment on top of multi_v7_defconfig
  patches/                 kernel patches, applied in order
  dts/                     device trees of the frame, built in-tree
  build-kernel.sh          builds the Debian kernel packages
```

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
or device trees change. Settings via the environment: `BUILD_DIR`,
`OUTPUT_DIR`, `DOWNLOAD_DIR`, `JOBS`, `CROSS_COMPILE`, `SOURCE_DATE_EPOCH`
(see the header of the script).

### Patches

| Patch | Why |
|---|---|
| 0001 dw-mipi-dsi: clock lane in LP in command mode | The JD9365 panel only starts its DSI receiver when it sees LP-11 on the clock lane after its reset; without it the display stays black. |
| 0002 mmc: quirks for the RK915 SDIO WiFi | Short CISTPL_FUNCE, continuous SDIO clock, CMD52 waits for DAT, matched by `compatible = "rockchip,rk915"`. From the RK915 driver port. |
| 0003 dw_mmc: survive an SDIO card that stops responding | Masks stray FIFO interrupts, software data timeout for SDIO writes (from PR #1 of the RK915 driver port, write timeout limited to SDIO). |

## Build host

Debian 13 (trixie), any architecture, with armhf as foreign architecture:

```
dpkg --add-architecture armhf
apt-get update
apt-get install gcc-arm-linux-gnueabihf make bc bison flex perl rsync cpio \
	dpkg-dev debhelper fakeroot kmod xz-utils curl \
	libssl-dev libelf-dev libdw-dev libssl-dev:armhf
```

`libssl-dev:armhf` is needed for the ARM build tools in the headers package.
