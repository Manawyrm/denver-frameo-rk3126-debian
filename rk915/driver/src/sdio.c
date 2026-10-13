/* Copyright (c) 2008 -2014 Rockchip System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * init , call sdio_init
 *
 */
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
#include <linux/rfkill.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/delay.h>

#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sd.h>

#include "core.h"
#include "if_io.h"
#include "sdio.h"
//#include "fw_data.h"
//#include "rom_patch.h"
#include "soc.h"
#include "hal.h"
#include "utils.h"
#include "platform.h"

static struct semaphore powerup_sem;

#define RK915_SDIO_RESCAN_COUNT 3

#define MANUFACTURER_ID_EAGLE_BASE        0x5347
#define MANUFACTURER_ID_EAGLE_BASEX       0x5348
#define MANUFACTURER_CODE                 0x296

struct sdio_func *gfunc1 = NULL;
struct sdio_func *gfunc2 = NULL;

static const struct sdio_device_id rk915_sdio_devices[] = {
        {SDIO_DEVICE(MANUFACTURER_CODE, MANUFACTURER_ID_EAGLE_BASE)},
	{SDIO_DEVICE(MANUFACTURER_CODE, MANUFACTURER_ID_EAGLE_BASEX)},
        {},
};
MODULE_DEVICE_TABLE(sdio, rk915_sdio_devices);

struct device *hal_get_dev(void)
{
	return &gfunc1->dev;
}

//extern u32 mmc_debug_level;
extern int sdio_reset_comm(struct mmc_card *card);
//static unsigned char resetdata[1024];
static bool sdio_reset;
int _sdio_reset(struct host_io_info *host)
{
#if 1
	sdio_reset = true;
	return 0;
#else
	struct sdio_func *func = (struct sdio_func *)host->priv_data;
	//struct mmc_host *shost = func->card->host;
	int ret, error, value, i;

	sdio_reset = true;
//	mmc_debug_level = 0xFFFF;

	printk(" start _sdio_reset ... \n");
	mdelay(2000);

	if (sdio_reset_comm(func->card))
		pr_err("sdio_reset_comm fail!!!\n");

	sdio_claim_host(func);

	ret = sdio_enable_func(func);
	if (ret) {
		pr_err("%s: failed to enable func, error %d\n", __func__, ret);
		sdio_release_host(func);
		return -1;
	}

	/* Interrupt Enable for Function x */
	sdio_f0_writeb(func, 0x07, 0x04, &error);
	if (error)
		goto fail;

	/* Default is GPIO interrupt, if it's "0", DATA1 interrupt */
	sdio_f0_writeb(func, 0x80, 0x16, &error);
	if (error)
		goto fail;

	/* Block Size for Function 0 */
	error = sdio_set_block_size(func, 512);
	if (error)
		goto fail;

	/* It can generate an interrupt to host */
	sdio_writeb(func, 0x02, 34, &error);
	if (error)
		goto fail;

	sdio_writeb(func, 0x02, 33, &error);
	if (error)
		goto fail;

	/* clear interrupt to host */
	value = sdio_readb(func, 32, &error);
	if (error)
		goto fail;

	sdio_writeb(func, value, 32, &error);
	if (error)
		goto fail;

	for (i = 0; i < 3; i++) {
		if (sdio_memcpy_fromio(func, resetdata, 0x0000008, 462)) {
			pr_err("sdio_memcpy_fromio fail!!!\n");
			mdelay(3000);
		} else {
			pr_info("sdio_memcpy_fromio ok !\n");
			break;
		}
	}

	RPU_ERROR_SDIO("TX FW CRASH:\n");
	pr_err("%s\n", resetdata);

	sdio_release_host(func);

	return 0;

fail:
	return -1;
#endif	
}

#if SUPPORT_SDIO_SLEEP
int lpw_is_ready = 0;
static int is_sdio_sleep = 0;

int sdio_clk_sleep(struct host_io_info *host, int val)
{
	struct sdio_func *func = (struct sdio_func *)host->priv_data;
	struct mmc_host *shost = func->card->host;
	static int mmc_working_clk = 0;

	RPU_INFO_SDIO("%s: %d\n", __func__, val);

	// 1. config sdio clock for save power
	if (val) {
		// backup working clock, and set sdio clock to 0
		if (shost->ios.clock > 0)
			mmc_working_clk = shost->ios.clock;
		shost->ios.clock = 0;
	} else {
		// restore sdio clock
		shost->ios.clock = mmc_working_clk;
	}
	if (shost->ios.clock > shost->f_max) {
		shost->ios.clock = shost->f_max;
	}
	RPU_INFO_SDIO("%s: change clock to %d\n", __func__, shost->ios.clock);

	// 2. config sdio pin ctrl for save power
#define MMC_POWER_CLK_SLEEP	10
#define MMC_POWER_CLK_WAKEUP	11
	shost->ios.power_mode = val?MMC_POWER_CLK_SLEEP:MMC_POWER_CLK_WAKEUP;
	RPU_INFO_SDIO("%s: change power mode %d\n", __func__, shost->ios.power_mode);
	shost->ops->set_ios(shost, &shost->ios);

	msleep(5);

	return 0;
}

// change sdio clock to zero and iomux to gpio
static int sdio_sleep(struct host_io_info *host)
{
	struct sdio_func *func = (struct sdio_func *)host->priv_data;

	if (lpw_is_ready == 0) {
		RPU_ERROR_SDIO("%s: LPW is not ready!\n", __func__);
		return 0;
	}

	sdio_claim_host(func);

	if (is_sdio_sleep == 0) {
		sdio_clk_sleep(host, 1);
		is_sdio_sleep = 1;
	}

	sdio_release_host(func);

	return 0;
}

static int sdio_wakeup_unlock(struct host_io_info *host)
{
	if (is_sdio_sleep) {
		sdio_clk_sleep(host, 0);
		is_sdio_sleep = 0;
	}

	return 0;
}

static int sdio_wakeup(struct host_io_info *host)
{
	struct sdio_func *func = (struct sdio_func *)host->priv_data;

	sdio_claim_host(func);
	sdio_wakeup_unlock(host);
	sdio_release_host(func);

	return 0;
}

#if SDIO_AUTO_SLEEP
static void sleep_timer_expiry(struct work_struct *work)
{
        struct host_io_info *host =
                container_of(work, struct host_io_info, sleep_work.work);

	sdio_sleep(host);
}
#endif
#endif

static void sdio_lock(struct host_io_info *host)
{
	struct sdio_func *func = (struct sdio_func *)host->priv_data;

#if SDIO_AUTO_SLEEP
	cancel_delayed_work_sync(&host->sleep_work);
#endif
	sdio_claim_host(func);
#if SDIO_AUTO_SLEEP
	sdio_wakeup_unlock(host);
#endif
}

static void sdio_unlock(struct host_io_info *host)
{
	struct sdio_func *func = (struct sdio_func *)host->priv_data;

#if SDIO_AUTO_SLEEP
	#define SLEEP_TIMEOUT_MS	100
	schedule_delayed_work(&host->sleep_work, HZ/(1000/SLEEP_TIMEOUT_MS));
#endif
	sdio_release_host(func);
}

static int _sdio_readb(struct host_io_info *host, u32 addr)
{
	int val, error;
	struct sdio_func *func = (struct sdio_func *)host->priv_data;

	if (sdio_reset == true)
		return 0;

	val = sdio_readb(func, addr, &error);

	if (val == 0xff)
		return error;
	else
		return val;
}

static int _sdio_writeb(struct host_io_info *host, u32 addr, u8 val)
{
	struct sdio_func *func = (struct sdio_func *)host->priv_data;
	int error;

	if (sdio_reset == true)
		return 0;

	sdio_writeb(func, val, addr, &error);
	return error;
}

static int sdio_send_data_sg(struct host_io_info *host, u32 addr, u8 *buf, u32 len)
{
#ifdef TX_SG_MODE
	struct sdio_func *func = (struct sdio_func *)host->priv_data;
#endif
	int ret = 0;

	if (sdio_reset == true)
		return 0;

#ifdef TX_SG_MODE
	ret = sdio_memcpy_toio_sg(func, addr, buf, len);
#endif

	return ret;
}

static int sdio_send_data(struct host_io_info *host, u32 addr, u8 *buf, u32 len)
{
	struct sdio_func *func = (struct sdio_func *)host->priv_data;

	if (sdio_reset == true)
		return 0;

	return sdio_memcpy_toio(func, addr, buf, len);
}

static int sdio_recv_data(struct host_io_info *host, u32 addr, u8 *buf, u32 len)
{
	struct sdio_func *func = (struct sdio_func *)host->priv_data;

	if (sdio_reset == true)
		return 0;

	return sdio_memcpy_fromio(func, buf, addr, len);
}

/*
 * Devices that remain active during a system suspend are
 * put back into 1-bit mode.
 */
static int sdio_disable_wide(struct host_io_info *host)
{
	int error;
	u8 ctrl;
	struct sdio_func *func = (struct sdio_func *)host->priv_data;
	struct mmc_host *shost = func->card->host;

	if (!(func->card->host->caps & MMC_CAP_4_BIT_DATA))
		return 0;

	if (func->card->cccr.low_speed && !func->card->cccr.wide_bus)
		return 0;

	ctrl = sdio_f0_readb(func, SDIO_CCCR_IF, &error);
	if (error)
		return error;

	if (!(ctrl & SDIO_BUS_WIDTH_4BIT))
		return 0;

	ctrl &= ~SDIO_BUS_WIDTH_4BIT;
	ctrl |= SDIO_BUS_ASYNC_INT;

	sdio_f0_writeb(func, ctrl, SDIO_CCCR_IF, &error);
	if (error)
		return error;

	shost->ios.bus_width = MMC_BUS_WIDTH_1;
	shost->ops->set_ios(shost, &shost->ios);

	return 0;
}

static int sdio_device_init(struct host_io_info *host)
{
	int error;
	unsigned char value;
	struct sdio_func *func = (struct sdio_func *)host->priv_data;

	RPU_DEBUG_SDIO("enter %s.\n", __func__);

	sdio_claim_host(func);

	/* Interrupt Enable for Function x */
	sdio_f0_writeb(func, 0x07, 0x04, &error);
	if (error)
		goto fail;

	/* Default is GPIO interrupt, if it's "0", DATA1 interrupt */
	sdio_f0_writeb(func, 0x80, 0x16, &error);
	if (error)
		goto fail;

	/* Block Size for Function 0 */
	error = sdio_set_block_size(func, 512);
	if (error)
		goto fail;

	/* It can generate an interrupt to host */
	sdio_writeb(func, 0x02, 34, &error);
	if (error)
		goto fail;

	sdio_writeb(func, 0x02, 33, &error);
	if (error)
		goto fail;

	/* clear interrupt to host */
	value = sdio_readb(func, 32, &error);
	if (error)
		goto fail;

	sdio_writeb(func, value, 32, &error);
	if (error)
		goto fail;

	sdio_release_host(func);
#if SUPPORT_SDIO_SLEEP
	lpw_is_ready = 1;
#endif
	return 0;

fail:
	sdio_release_host(func);
	return error;
}

static int sdio_writeb_comp(struct host_io_info *host)
{
	int val;
	int onetime = 1; // us
	int count = 500*1000; // wait total (onetime*count) ms

	while (count--) {
		if (hpriv->fw_error_processing)
			return -1;

		val = _sdio_readb(host, SDIO_HOST_WRITE_REQ_INT_STA);
		if (val == 0) {
			//RPU_INFO_SDIO("%s: %d\n", __func__, count);
			return 0; // success
		} else if (val < 0) {
			RPU_ERROR_SDIO("%s: error %d\n", __func__, val);
			return -1;
		}
		udelay(onetime);
		//RPU_INFO_SDIO("count = %d, val = %d\n", count, val);
	}

	RPU_ERROR_SDIO("%s: timeout val = %d\n", __func__, val);
	return -1; // wait timeout failed
}

static int sdio_notify_fw_pm(struct host_io_info *host, int wakeup)
{
#if NOTIFY_M0_SLEEP
	int msg = wakeup?IO_NOTIFY_WAKEUP:IO_NOTIFY_SLEEP;

	RPU_INFO_SDIO("notify m0 %s\n", wakeup?"wakeup":"sleep");
	_sdio_writeb(host, IO_NOTIFY_ADDR, msg);
	return sdio_writeb_comp(host);
#else
	return 0;
#endif
}

static struct host_io_ops sdio_host_ops = {
	.io_init			= sdio_device_init,
	.io_send			= sdio_send_data,
	.io_send_sg			= sdio_send_data_sg,
	.io_recv			= sdio_recv_data,
	.lock				= sdio_lock,
	.unlock				= sdio_unlock,
	.io_readb			= _sdio_readb,
	.io_writeb			= _sdio_writeb,
	.io_writeb_comp		= sdio_writeb_comp,
	.io_ejtag			= sdio_disable_wide,
	.io_reset			= _sdio_reset,
	.io_notify_pm		= sdio_notify_fw_pm,
#if SUPPORT_SDIO_SLEEP
	.sleep				= sdio_sleep,
	.wakeup				= sdio_wakeup,
#endif
};

static int add_rk915_device(struct sdio_func *func)
{
	int ret = -1;

	RPU_INFO_SDIO("%s.\n", __func__);

	hpriv->plat_dev = platform_device_alloc("rk915", -1);
	if (!hpriv->plat_dev) {
		RPU_ERROR_SDIO("%s: can't allocate platform_device\n", __func__);
		goto err;
	}

	hpriv->plat_dev->dev.parent = &func->dev;

	ret = platform_device_add(hpriv->plat_dev);
	if (ret) {
		RPU_ERROR_SDIO("%s: can't add platform_device\n", __func__);
		goto err;
	}

	return 0;

err:
	if (hpriv->plat_dev) {
		platform_device_put(hpriv->plat_dev);
	}
	return ret;
}

static void del_rk915_device(void)
{
	RPU_INFO_SDIO("%s.\n", __func__);

	platform_device_unregister(hpriv->plat_dev);
	hpriv->plat_dev = NULL;
}

static int sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	int ret = 0;

	RPU_DEBUG_SDIO("%s.\n", __func__);
	RPU_DEBUG_SDIO("sdio_func_num: 0x%X, vendor id: 0x%X, dev id: 0x%X, block size: 0x%X/0x%X\n",
			func->num, func->vendor, func->device, func->max_blksize, func->cur_blksize);

	if (func->num == 1)
		gfunc1 = func;
	else
		gfunc2 = func;

	RPU_INFO_SDIO("f1: 0x%p, f2: 0x%p.\n", gfunc1, gfunc2);
	if (!(gfunc1 && gfunc2)) {
		RPU_DEBUG_SDIO("%s: no valid func\n", __func__);
		return 0;
	}

	sdio_claim_host(gfunc1);
	ret = sdio_enable_func(gfunc1);
	if (ret) {
		RPU_ERROR_SDIO("%s: failed to enable func, error %d\n", __func__, ret);
		sdio_release_host(gfunc1);
		return -1;
	}
	RPU_INFO_SDIO("%s: enable func ok.\n", __func__);
	sdio_release_host(gfunc1);

	gfunc1->card->quirks |= MMC_QUIRK_LENIENT_FN0;
	sdio_reset = false;

	up(&powerup_sem);

	return ret;
}

static void sdio_remove(struct sdio_func *func) 
{
	if (hpriv->plat_dev &&
		hpriv->plat_dev->dev.parent == &func->dev) {
		del_rk915_device();
	}
	gfunc1 = NULL;
	gfunc2 = NULL;
}

#define dev_to_sdio_func(d)	container_of(d, struct sdio_func, dev)

#ifdef CONFIG_PM
static int sdio_suspend(struct device *dev)
{
	int ret = 0;
	mmc_pm_flag_t sdio_flags;
	struct sdio_func *func = dev_to_sdio_func(dev);

	if ((void*)func != hpriv->io_info->priv_data) {
		RPU_DEBUG_SDIO("%s: is not rk915 sdio, skip it!\n", __func__);
		return 0;
	}

	RPU_INFO_SDIO("%s enter\n", __func__);

	sdio_flags = sdio_get_host_pm_caps(func);
	if (!(sdio_flags & MMC_PM_KEEP_POWER)) {
		dev_err(dev, "can't keep power while host "
						"is suspended\n");
		ret = -EINVAL;
		goto out;
	}

	/* keep power while host suspended */
	ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);
	if (ret) {
		dev_err(dev, "error while trying to keep power\n");
		goto out;
	}

	hpriv->during_pm_resume = 1;

#if SUPPORT_SDIO_SLEEP
	// change sdio clock to zero and iomux to gpio.
	sdio_sleep(hpriv->io_info);
#endif
	// disable interrupt
	// disable_irq(hpriv->io_info->irq);

out:
	return ret;
}

static int sdio_resume(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);

	if ((void*)func != hpriv->io_info->priv_data) {
		RPU_DEBUG_SDIO("%s: is not rk915 sdio, skip it!\n", __func__);
		return 0;
	}

	RPU_INFO_SDIO("%s enter\n", __func__);

	hpriv->during_pm_resume = 1;
	// enable_irq(hpriv->io_info->irq);

#if SUPPORT_SDIO_SLEEP
	// change sdio clock to last clk
	sdio_wakeup(hpriv->io_info);
#endif

	return 0;
}

static const struct dev_pm_ops sdio_pm_ops = {
	.suspend = sdio_suspend,
	.resume  = sdio_resume,
};
#endif

static struct sdio_driver rk915_sdio_driver = {
		.name = "rk915_sdio",
		.id_table = rk915_sdio_devices,
		.probe = sdio_probe,
		.remove = sdio_remove,
#ifdef CONFIG_PM		
		.drv = {
			.pm = &sdio_pm_ops,
		}
#endif
};

int rk915_sdio_register_driver(void)
{
	return sdio_register_driver(&rk915_sdio_driver);
}

void rk915_sdio_unregister_driver(void)
{
	sdio_unregister_driver(&rk915_sdio_driver);
}

void rk915_sdio_pre_init(void)
{
    sema_init(&powerup_sem, 0);
}

int rk915_sdio_init(struct host_io_info *phost)
{
	int retry;
	struct host_io_info *host = phost;

//	mmc_debug_level = 0;

	retry = RK915_SDIO_RESCAN_COUNT;

reinit:
    if (!gfunc1) {
    	/* power up and rescan */
    	rk915_poweron();
    	mdelay(200);
    	rk915_rescan_card(1);

    	if (down_timeout(&powerup_sem, msecs_to_jiffies(800))) {
    		rk915_rescan_card(0);
    		rk915_poweroff();
    		mdelay(200);

    		if (retry == 0)
    			goto fail;

    		RPU_ERROR_SDIO("rk915 sdio probe failed, retry (%d)\n", retry);
    		retry--;
    		goto reinit;
    	}
    }

	RPU_DEBUG_SDIO("%s rk915 sdio probe success\n", __func__);

	host->priv_data = (void *)gfunc1;
	host->dev = &gfunc1->dev;

	host->io_ops = &sdio_host_ops;
	host->irq = of_irq_get_byname(host->dev->of_node, "host-wake");
#if SDIO_AUTO_SLEEP
	// init delay sleep worker
	INIT_DELAYED_WORK(&host->sleep_work, sleep_timer_expiry);
#endif
	phost->bus_init = true;

	add_rk915_device(gfunc1);

	return 0;

fail:
	return -1;
}

int rk915_sdio_deinit(struct host_io_info *phost)
{
	rk915_rescan_card(0);
	rk915_poweroff();

	phost->bus_init = false;

	return 0;
}

#ifdef ENABLE_FW_ERROR_RECOVERY
static int rk915_mmc_io_rw_direct_host(struct mmc_host *host, int write, unsigned fn,
	unsigned addr, u8 in, u8 *out)
{
	struct mmc_command cmd = {0};
	int err;

	/* sanity check */
	if (addr & ~0x1FFFF)
		return -EINVAL;

	cmd.opcode = SD_IO_RW_DIRECT;
	cmd.arg = write ? 0x80000000 : 0x00000000;
	cmd.arg |= fn << 28;
	cmd.arg |= (write && out) ? 0x08000000 : 0x00000000;
	cmd.arg |= addr << 9;
	cmd.arg |= in;
	cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(host, &cmd, 0);
	if (err)
		return err;

	{
		if (cmd.resp[0] & R5_ERROR)
			return -EIO;
		if (cmd.resp[0] & R5_FUNCTION_NUMBER)
			return -EINVAL;
		if (cmd.resp[0] & R5_OUT_OF_RANGE)
			return -ERANGE;
	}

	if (out) {
		*out = cmd.resp[0] & 0xFF;
	}

	return 0;
}

static int rk915_mmc_select_card(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	struct mmc_command cmd = {0};

	cmd.opcode = MMC_SELECT_CARD;

	if (card) {
		cmd.arg = card->rca << 16;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	} else {
		cmd.arg = 0;
		cmd.flags = MMC_RSP_NONE | MMC_CMD_AC;
	}

	err = mmc_wait_for_cmd(host, &cmd, 3);
	if (err)
		return err;

	return 0;
}

static int rk915_sdio_reset(struct mmc_card *card)
{
	int ret;
	u8 abort;

	ret = rk915_mmc_io_rw_direct_host(card->host, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
	if (ret)
		abort = 0x08;
	else
		abort |= 0x08;

	ret = rk915_mmc_io_rw_direct_host(card->host, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
	return ret;
}

static int rk915_mmc_go_idle(struct mmc_host *host)
{
	int err;
	struct mmc_command cmd = {0};

	cmd.opcode = MMC_GO_IDLE_STATE;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_NONE | MMC_CMD_BC;

	err = mmc_wait_for_cmd(host, &cmd, 0);

	mdelay(1);

	host->use_spi_crc = 0;

	return err;
}

int rk915_mmc_send_io_op_cond(struct mmc_host *host, u32 ocr, u32 *rocr)
{
	struct mmc_command cmd = {0};
	int i, err = 0;

	cmd.opcode = SD_IO_SEND_OP_COND;
	cmd.arg = ocr;
	cmd.flags = MMC_RSP_SPI_R4 | MMC_RSP_R4 | MMC_CMD_BCR;

	for (i = 100; i; i--) {
		err = mmc_wait_for_cmd(host, &cmd, 3);
		if (err)
			break;

		/* if we're just probing, do a single pass */
		if (ocr == 0)
			break;

		/* otherwise wait until reset completes */
		{
			if (cmd.resp[0] & MMC_CARD_BUSY)
				break;
		}

		err = -ETIMEDOUT;

		mdelay(10);
	}

	return err;
}

int rk915_mmc_send_relative_addr(struct mmc_host *host, unsigned int *rca)
{
	int err;
	struct mmc_command cmd = {0};

	cmd.opcode = SD_SEND_RELATIVE_ADDR;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R6 | MMC_CMD_BCR;

	err = mmc_wait_for_cmd(host, &cmd, 3);
	if (err)
		return err;

	*rca = cmd.resp[0] >> 16;

	return 0;
}

static int rk915_mmc_sdio_switch_hs(struct mmc_card *card, int enable)
{
	int ret;
	u8 speed;

	if (!(card->host->caps & MMC_CAP_SD_HIGHSPEED))
		return 0;

	if (!card->cccr.high_speed)
		return 0;

	ret = rk915_mmc_io_rw_direct_host(card->host, 0, 0, SDIO_CCCR_SPEED, 0, &speed);
	if (ret)
		return ret;

	if (enable)
		speed |= SDIO_SPEED_EHS;
	else
		speed &= ~SDIO_SPEED_EHS;

	ret = rk915_mmc_io_rw_direct_host(card->host, 1, 0, SDIO_CCCR_SPEED, speed, NULL);
	if (ret)
		return ret;

	return 1;
}

static int rk915_sdio_enable_hs(struct mmc_card *card)
{
	int ret;

	ret = rk915_mmc_sdio_switch_hs(card, true);
	/*if (ret <= 0 || card->type == MMC_TYPE_SDIO)
		return ret;

	ret = mmc_sd_switch_hs(card);
	if (ret <= 0)
		mmc_sdio_switch_hs(card, false);*/

	return ret;
}

static int rk915_sdio_enable_wide(struct mmc_card *card)
{
	int ret;
	u8 ctrl;

	if (!(card->host->caps & MMC_CAP_4_BIT_DATA))
		return 0;

	if (card->cccr.low_speed && !card->cccr.wide_bus)
		return 0;

	ret = rk915_mmc_io_rw_direct_host(card->host, 0, 0, SDIO_CCCR_IF, 0, &ctrl);
	if (ret)
		return ret;

	/* set as 4-bit bus width */
	ctrl &= ~SDIO_BUS_WIDTH_MASK;
	ctrl |= SDIO_BUS_WIDTH_4BIT;

	ret = rk915_mmc_io_rw_direct_host(card->host, 1, 0, SDIO_CCCR_IF, ctrl, NULL);
	if (ret)
		return ret;

	return 1;
}

static int rk915_sdio_enable_4bit_bus(struct mmc_card *card)
{
	return rk915_sdio_enable_wide(card);
}

static void rk915_mmc_power_up(struct mmc_host *host, u32 ocr)
{

	host->ios.chip_select = MMC_CS_DONTCARE;
	host->ios.bus_mode = MMC_BUSMODE_PUSHPULL;
	host->ios.power_mode = MMC_POWER_UP;
	host->ios.bus_width = MMC_BUS_WIDTH_1;
	host->ios.timing = MMC_TIMING_LEGACY;
	host->ops->set_ios(host, &host->ios);

	mdelay(10);

	host->ios.clock = host->f_init;

	host->ios.power_mode = MMC_POWER_ON;
	host->ops->set_ios(host, &host->ios);

	mdelay(10);
}

static void rk915_mmc_set_clock(struct mmc_host *host, unsigned int hz)
{
	if (hz > host->f_max)
		hz = host->f_max;

	host->ios.clock = hz;
	host->ops->set_ios(host, &host->ios);
}

static void rk915_mmc_set_bus_width(struct mmc_host *host, unsigned int width)
{
	host->ios.bus_width = width;
	host->ops->set_ios(host, &host->ios);
}


void rk915_sdio_set_clock(struct host_io_info *phost, int hz)
{
	struct sdio_func *func = (struct sdio_func *)phost->priv_data;
	struct mmc_host *shost = func->card->host;

	sdio_claim_host(func);
	rk915_mmc_set_clock(shost, hz);
	sdio_release_host(func);
}

int rk915_sdio_recovery_init(struct host_io_info *phost)
{
	struct sdio_func *func = (struct sdio_func *)phost->priv_data;
	struct mmc_host *shost = func->card->host;
	int err;
	u32 rocr, ocr, rca;

	RPU_DEBUG_ROCOVERY("%s\n", __func__);

	sdio_claim_host(func);

	shost->ios.power_mode = MMC_POWER_OFF;
	rk915_mmc_power_up(shost, 1);

	err = rk915_sdio_reset(func->card);
	/*if (err) {
		RPU_ERROR_ROCOVERY("rk915_sdio_reset failed (%d)\n", err);
		goto err_out;
	}*/

	err = rk915_mmc_go_idle(shost);
	if (err) {
		RPU_ERROR_ROCOVERY("rk915_mmc_go_idle failed (%d)\n", err);
		goto err_out;
	}

	ocr = 0;
	err = rk915_mmc_send_io_op_cond(shost, ocr, &rocr);
	if (err) {
		RPU_ERROR_ROCOVERY("rk915_mmc_send_io_op_cond1 failed (%d)\n", err);
		goto err_out;
	}

	ocr = 0x1800000;
	err = rk915_mmc_send_io_op_cond(shost, ocr, &rocr);
	if (err) {
		RPU_ERROR_ROCOVERY("rk915_mmc_send_io_op_cond2 failed (%d)\n", err);
		goto err_out;
	}

	err = rk915_mmc_send_relative_addr(shost, &rca);
	if (err) {
		RPU_ERROR_ROCOVERY("rk915_mmc_send_relative_addr failed (%d)\n", err);
		goto err_out;
	}

	err = rk915_mmc_select_card(shost, func->card);
	if (err) {
		RPU_ERROR_ROCOVERY("rk915_mmc_select_card failed (%d)\n", err);
		goto err_out;
	}

	err = rk915_sdio_enable_hs(func->card);
	if (err <= 0) {
		RPU_ERROR_ROCOVERY("rk915_sdio_enable_hs failed (%d)\n", err);
		goto err_out;
	}

	rk915_mmc_set_clock(shost, 50000000);

	err = rk915_sdio_enable_4bit_bus(func->card);
	if (err > 0) {
		rk915_mmc_set_bus_width(shost, MMC_BUS_WIDTH_4);
	} else {
		RPU_ERROR_ROCOVERY("rk915_sdio_enable_4bit_bus failed (%d)\n", err);
		goto err_out;
	}

	err = sdio_set_block_size(func, 512);
	if (err) {
		RPU_ERROR_ROCOVERY("sdio_set_block_size failed (%d)\n", err);
		goto err_out;
	}

	err = sdio_enable_func(func);
	if (err) {
		RPU_ERROR_ROCOVERY("sdio_enable_func failed (%d)\n", err);
		goto err_out;
	}

	sdio_reset = false;

err_out:
	sdio_release_host(func);
	return err;
}
#else
int rk915_sdio_recovery_init(struct host_io_info *phost)
{
	return 0;
}
#endif
