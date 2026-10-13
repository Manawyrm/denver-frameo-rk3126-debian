/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "core.h"
#include "soc.h"

void program_rpu_dma_start(struct hal_priv *priv) 
{
	return;
}

void clock_init(struct hal_priv *priv)
{
	return;
}
void clock_deinit(struct hal_priv *priv)
{
	return;
}



int parse_dtb_config (struct hal_priv *priv)
{
	return 0;
}



void config_mem_region(unsigned int addr)
{

}

struct soc_ops_tag soc_ops = {
	.program_rpu_dma_start = program_rpu_dma_start,
	.parse_dtb = parse_dtb_config,
	.clock_init = clock_init,
	.clock_deinit = clock_deinit,
	.set_mem_region = config_mem_region,
};

