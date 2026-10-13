#include <linux/module.h>
#include <net/mac80211.h>
#include <linux/time.h>
#include <linux/pm.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/delay.h>

#include "core.h"
#include "if_io.h"
#include "soc.h"
#include "hal.h"
#include "utils.h"
#include "platform.h"
#include "hal_io.h"

struct hal_priv *hpriv;
bool m0_jtag_enable = false;

static int fw_bring_up(void *p)
{
	struct hal_priv *priv = (struct hal_priv *)p;

	if (hpriv->fw_error_processing) {
		rk915_poweron();
		mdelay(RK915_POWER_ON_DELAY_MS);
		rk915_platform_bus_rec_init(priv->io_info);
		mdelay(RK915_POWER_ON_DELAY_MS);
	} else {
		if (rk915_platform_bus_init(priv->io_info)) {
			RPU_ERROR_MAIN("%s: platform_bus_init failed\n", __func__);
			return -1;
		}
	}

	if (rk915_download_firmware(priv)) {
		RPU_ERROR_MAIN("%s: rk915_download_firmware failed\n", __func__);
		return -1;
	}

	if (rk915_io_init(priv)) {
		RPU_ERROR_MAIN("%s: rk915_io_init failed\n", __func__);
		return -1;
	}

	if (!down_fw_in_probe && !hpriv->fw_error_processing) {
		if (rk915_register_irq(priv->io_info)) {
			RPU_ERROR_MAIN("%s: rk915_irq_register failed\n", __func__);
			return -1;
		}
	}

	return 0;
}

static int fw_tear_down(void *p)
{
	if (hpriv->fw_error_processing) {
		rk915_poweroff();
		return 0;
	}

	rk915_platform_bus_deinit(hpriv->io_info);

	return 0;
}

static void rk915_core_deinit(void)
{
	struct host_io_info *host;

	if (!hpriv)
		return;

	host = hpriv->io_info;
	if (host)
		rk915_free_firmware_buf(&host->firmware);
	if (host && host->rx_serias_buf)
		kfree(host->rx_serias_buf);
	if (hpriv)
		kfree(hpriv);
	if (host)
		kfree(host);
}

static int rk915_core_init(void)
{
	struct host_io_info *host = NULL;
	struct hal_priv *priv = NULL;

	RPU_INFO_MAIN("%s.\n", __func__);

	host = kzalloc(sizeof(struct host_io_info), GFP_KERNEL);
	if (!host) {
		RPU_ERROR_MAIN("%s: kalloc hal_priv failed\n", __func__);
		goto err;
	}

	host->rx_serias_buf = kzalloc(MAX_RX_SERIAS_BYTES, GFP_KERNEL);
	if (!host->rx_serias_buf) {
		RPU_ERROR_MAIN("%s: kalloc hal_priv failed\n", __func__);
		goto err;
	}

	host->rx_serias_idx = -1;
	host->rx_serias_count = 0;
	host->rx_next_len = 0;
	host->bus_init = false;

	if (rk915_alloc_firmware_buf(&host->firmware) != 0) {
		RPU_ERROR_MAIN("%s: rk915_alloc_firmware_buf failed\n", __func__);
		goto err;
	}

	priv = kzalloc(sizeof(struct hal_priv), GFP_KERNEL);
	if (!priv) {
		RPU_ERROR_MAIN("%s: kalloc hal_priv failed\n", __func__);
		goto err;
	}

	hpriv = priv;
	priv->io_info = host;

	rk915_sdio_pre_init();

	return 0;

err:
	rk915_core_deinit();

	return -1;
}

int rk915_probe(struct platform_device *pdev)
{
	int ret;
	struct host_io_info *host = hpriv->io_info;

	RPU_INFO_MAIN("%s\n", __func__);

	hpriv->fw_bring_up_func = fw_bring_up;
	hpriv->fw_tear_down_func = fw_tear_down;

	/* Initialize the rest of the layer */
	ret = hal_ops.init(host->dev);
	if (ret < 0) {
		RPU_ERROR_MAIN("%s: hal_ops.init failed\n", __func__);
		return -1;
	}

	if (down_fw_in_probe) {
		if (fw_bring_up(hpriv)) {
			RPU_ERROR_MAIN("%s: fw_bring_up failed\n", __func__);
			return -1;
		}

		if (rk915_register_irq(hpriv->io_info)) {
			RPU_ERROR_MAIN("%s: rk915_irq_register failed\n", __func__);
			return -1;
		}
	}

	return 0;
}

void rk915_remove(struct platform_device *pdev)
{
	RPU_INFO_MAIN("%s\n", __func__);

	hal_ops.deinit(NULL);

	rk915_free_irq(hpriv->io_info);
}

void rk915_shutdown(struct platform_device *pdev)
{
	//RPU_INFO_MAIN("%s\n", __func__);
	hpriv->shutdown = 1;
}

static const struct platform_device_id rk915_id_table[] = {
    {
        .name = "rk915",
        .driver_data = 0x00,
    },
    {},
};
MODULE_DEVICE_TABLE(platform, rk915_id_table);

static struct platform_driver rk915_driver =
{
    .probe = rk915_probe,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
    .remove = __devexit_p(rk915_remove),
#else
    .remove = rk915_remove,
#endif
    .shutdown = rk915_shutdown,
    .id_table = rk915_id_table,
    .driver = {
        .name = "rk915-wifi",
        .owner = THIS_MODULE,
    }
};

static int __init rk915_init(void)
{
	int ret;

	RPU_INFO_MAIN("=======================================================\n");
	RPU_INFO_MAIN("==== Launching Wi-Fi driver! (Powered by Rockchip) ====\n");
	RPU_INFO_MAIN("=======================================================\n");
	RPU_INFO_MAIN("RK915 WiFi Ver: %s\n", VERSION_INFO);
	RPU_INFO_MAIN("Build time: %s %s\n", __DATE__, __TIME__);

	ret = rk915_core_init();
	if (ret) {
		RPU_ERROR_MAIN("%s: rk915_core_init failed\n", __func__);
		goto error;
	}

	ret = rk915_bus_register_driver();
	if (ret) {
		RPU_ERROR_MAIN("%s: rk915_bus_register_driver failed\n", __func__);
		goto error;
	}

	ret = platform_driver_register(&rk915_driver);
	if (ret) {
		RPU_ERROR_MAIN("%s: rk915_platform_driver_register failed\n", __func__);
		goto error1;
	}

	ret = rk915_platform_bus_init(hpriv->io_info);
	if (ret) {
		RPU_ERROR_MAIN("%s: platform_bus_init failed\n", __func__);
		goto error2;
	}

	return ret;
error2:
	platform_driver_unregister(&rk915_driver);
error1:
	rk915_bus_unregister_driver();
error:
	return ret;
}

static void __exit rk915_exit(void)
{
	RPU_INFO_MAIN("==========================================================\n");
	RPU_INFO_MAIN("==== Dislaunching Wi-Fi driver! (Powered by Rockchip) ====\n");
	RPU_INFO_MAIN("==========================================================\n");

	platform_driver_unregister(&rk915_driver);

	rk915_bus_unregister_driver();

	rk915_platform_bus_deinit(hpriv->io_info);

	rk915_core_deinit();
}

module_init(rk915_init);
module_exit(rk915_exit);

module_param_named(jtag, m0_jtag_enable, bool, 0644);
MODULE_AUTHOR("Rockchips");
MODULE_DESCRIPTION("Driver for Rockchips RK915 SDIO WiFi Devices");
MODULE_LICENSE("GPL");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
MODULE_IMPORT_NS("VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver");
#endif
