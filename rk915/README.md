# RK915 WiFi driver

Out-of-tree driver for the Rockchip RK915 SDIO WiFi chip of the frame.

## Origin

`driver/` is the mainline port of the Rockchip BSP driver by
https://github.com/sunshineinabox/rk915 (`main` branch, commit 590fe1d,
GPL-2.0, see `driver/LICENSE`), with these changes:

- builds against Linux 7.2 (timer, wakeup source, syscore, mac80211 and
  MODULE_IMPORT_NS API changes)
- fixes from https://github.com/sunshineinabox/rk915/pull/1 (pkilar): keep
  the LMAC awake (`lpw_no_sleep=1`), claim the firmware error recovery
  atomically, reset the vif accounting on hardware restart
- MAC address from the device tree (`local-mac-address`, filled in by
  U-Boot), then the `mac_addr` module parameter, random only as fallback
- firmware loaded from `rockchip/`

The newer `WIP-RK915_7.1` branch of the port is not used: on this board it
reaches only 145-185 KB/s with about 15 firmware recoveries per minute, this
version 1.6-1.9 MB/s without any.

The SDIO quirks the chip needs are kernel patches
(`kernel/patches/0002-*`, `0003-*`), activated by `compatible =
"rockchip,rk915"` in the device tree.

`firmware/` holds `rk915_fw.bin` and `rk915_patch.bin` from the stock
Android firmware of the frame (identical to the files of the driver port).

## Build

```
kernel/build-kernel.sh      # first: the module is built against its tree
rk915/build-rk915.sh
```

Result: `output/rk915-modules-<kernel release>_<version>_armhf.deb` with
the module (`/usr/lib/modules/<release>/updates/rk915.ko`) and the
firmware (`/usr/lib/firmware/rockchip/`). It depends on exactly the kernel
package it was built against.
