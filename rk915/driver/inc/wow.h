/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _WOW_H_
#define _WOW_H_
#include <linux/syscore_ops.h>

extern struct syscore host_syscore;

void wow_enable_irq_wake(void);
void wow_disable_irq_wake(void);


#endif /* _WOW_H_ */
