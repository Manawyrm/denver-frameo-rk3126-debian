/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _SOC_H_
#define _SOC_H_

#include <linux/clk.h>
#include <linux/iio/consumer.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_device.h>

#include "utils.h"
#include "hal_common.h"

/*SoC Porting information:
 */

struct soc_ops_tag {
	void (*program_rpu_dma_start)(struct hal_priv *priv);
	void (*clock_init)(struct hal_priv *priv);
	void (*clock_deinit)(struct hal_priv *priv);
	int (*parse_dtb) (struct hal_priv *priv);
	void (*set_mem_region)(unsigned int addr);
};

extern struct soc_ops_tag soc_ops;

/* As per LPW TRM, refer 4.1 section, mapped in FPGA
 */
#ifdef HAL_PCIE
#define WAKEUP_NOW_OFFSET 0x00F10030
#define HP_RPU_READY 0x00F10034
#endif
#ifdef HAL_HOSTPORT
#endif

#endif /* _SOC_H_ */
