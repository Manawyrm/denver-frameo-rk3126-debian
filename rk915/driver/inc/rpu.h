/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _RPU_H_
#define _RPU_H_

/* RPU Porting information: Based on RPU Config.

 * These are the only values which need to be modified as per 
 * a) host memory map 
 * b) HOSt <->MCU interrupt configuration.
 * c) RPU Config specific details
 */

/*********************************************************************
 * RPU Config Specific Details
 *********************************************************************
 */
#define HAL_RPU_GRAM_BASE 0xB7000000
#define HAL_RPU_GRAM_LEN 0x1eac0
#define HAL_SHARED_MEM_OFFSET 0xffc
/* fwldr.c converts these to HOST addresses
 * so pass RPU addresses here.
 * From: uccrunTime/Platform/configs
 */
#define RPU_COREA_REGION_START 0x80880000
#define RPU_COREA_REGION_LEN 0x4C000

#define RPU_COREB_REGION_START 0x82000000
#define RPU_COREB_REGION_LEN 0x4C000
/*********************************************************************
 */

/********************************************************************
 * HOST processor to Meta Processor on RPU Communications Registers
 ********************************************************************
 * Refer: Volt RPU.Technical Reference Manual.pdf
	The host_to_mtx_cmd register is written to by the host in order to
	send data to the META. The act of writing causes an event on the META
	(the host_int interrupt occurs). The META handles this event by reading
	host_to_mtx_cmd, collecting the message data.

	The META clears the interrupt and in the process acknowledges reception
	of the message by writing to the mtx_to_host_ack register. The host checks
	for this acknowledgement by reading host_to_mtx_cmd, checking the state of
	the HOST_INT bit.

	A message initiated by the META and destined for the host uses the same
	scheme, but utilising the mtx_to_host_cmd and host_to_mtx_ack registers and
	by responding to the mtx_int interrupt.
*/

/* SYSBUS - System Control - REGSYSREG 
 * RPU_CORE_REG is a subset of System Bus Registers
 */
#define HAL_RPU_CORE_REG_OFFSET	0x400

/* Register HOST_TO_MTX_CMD */
#define HOST_TO_MTX_CMD 0x0030
#define HOST_TO_MTX_CMD_ADDR ((hpriv->rpu_mem_addr) + \
				    HOST_TO_MTX_CMD)
#define MTX_HOST_INT_SHIFT 31

/* Register MTX_TO_HOST_CMD */
#define MTX_TO_HOST_CMD 0x0034
#define MTX_TO_HOST_CMD_ADDR ((hpriv->rpu_mem_addr) + \
				    MTX_TO_HOST_CMD)

/* Register HOST_TO_MTX_ACK */
#define HOST_TO_MTX_ACK 0x0038
#define HOST_TO_MTX_ACK_ADDR ((hpriv->rpu_mem_addr) + \
				    HOST_TO_MTX_ACK)
#define MTX_INT_CLR_SHIFT 31

/* Register MTX_TO_HOST_ACK */
#define MTX_TO_HOST_ACK 0x003C
#define MTX_TO_HOST_ACK_ADDR ((hpriv->rpu_mem_addr) + \
				    MTX_TO_HOST_ACK)

/* Register MTX_INT_ENABLE
 * Enable INT line within META Block
 */
#define MTX_INT_ENABLE 0x0044
#define MTX_INT_ENABLE_ADDR ((hpriv->rpu_mem_addr) + \
				   MTX_INT_ENABLE)
#define MTX_INT_EN_SHIFT 31

/* System Level Interrupt Control for each block.
 * Enable INT line for META block.
 */
#define SYS_INT_ENAB 0x0000
#define SYS_INT_ENAB_ADDR ((hpriv->rpu_mem_addr) + SYS_INT_ENAB)
#define SYS_INT_MTX_IRQ_ENAB_SHIFT 15

/*********************************************************************
 */
/*********************************************************************
 * RPU MTX FW Download Registers
 *********************************************************************
 */

enum rpu_mem_region {
	RPU_MEM_CORE,
	RPU_MEM_DIRECT,
	RPU_MEM_ERR
};


#define RPU_GRAM_BASE	    0xB7000000

#define RPU_OFFSET_MASK    0x00FFFFFF
#define RPU_BASE_MASK      0xFF000000
#define RPU_GRAM_PACKED    0xB7
#define RPU_GRAM_MSB       0xB4


#define RPU_SYSBUS_REG     0xA4
#define UCCP_BEV	   0xBF

#define REGMIPSMCU 0xA4000000                	       /* 7.5 */
#define MIPS_MCU_CONTROL REGMIPSMCU + 0x0              /* 13.1.1 */
#define MIPS_MCU_BOOT_EXCP_INSTR_0 REGMIPSMCU + 0x50   /* 13.1.15 */
#define MIPS_MCU_BOOT_EXCP_INSTR_1 REGMIPSMCU + 0x54   /* 13.1.16 */
#define MIPS_MCU_BOOT_EXCP_INSTR_2 REGMIPSMCU + 0x58   /* 13.1.17 */
#define MIPS_MCU_BOOT_EXCP_INSTR_3 REGMIPSMCU + 0x5c   /* 13.1.18 */

#define MIPS_MCU_SYS_CORE_MEM_CTRL  REGMIPSMCU + 0x30  /* 13.1.10 */
#define MIPS_MCU_SYS_CORE_MEM_WDATA REGMIPSMCU + 0x34  /* 13.1.11 */
#define MIPS_MCU_SYS_CORE_MEM_RDATA REGMIPSMCU + 0x38  /* 13.1.11 */


#define MTX_REG_INDIRECT(unit, reg) (((reg & 0x7) << 4) | (unit & 0xF))

#define MTX_PC_REG_IND_ADDR        MTX_REG_INDIRECT(5, 0)
#define MTX_A0STP_REG_IND_ADDR     MTX_REG_INDIRECT(3, 0)

#define MTX_PCX_REG_IND_ADDR MTX_REG_INDIRECT(5, 1)
#define MTX_TXMASK_REG_IND_ADDR MTX_REG_INDIRECT(7, 1)
#define MTX_TXMASKI_REG_IND_ADDR MTX_REG_INDIRECT(7, 3)
#define MTX_TXPOLL_REG_IND_ADDR MTX_REG_INDIRECT(7, 4)
#define MTX_TXPOLLI_REG_IND_ADDR MTX_REG_INDIRECT(7, 6)
#define MTX_TXSTAT_REG_IND_ADDR MTX_REG_INDIRECT(7, 0)
#define MTX_TXSTATI_REG_IND_ADDR MTX_REG_INDIRECT(7, 2)

#define REG_IND_READ_FLAG (1 << 16)

#define MTX_TXPRIVEXT_ADDR 0x048000E8
#define MTX_TXSTATUS_ADDR 0x48000010
#define	MTX_TXENABLE_ADDR 0x04800000
#define	MTX_START_EXECUTION 1
#define	MTX_STOP_EXECUTION 0

#define MTX_TXUXXRXDT 0x0480FFF0
#define MTX_TXUXXRXRQ 0x0480FFF8

#define MSLV_BASE_ADDR 0x0203C000

/* DATA Exchange Register */
#define MSLVDATAX (MSLV_BASE_ADDR + 0x2000)

/* DATA Transfer Register */
#define MSLVDATAT (MSLV_BASE_ADDR + 0x2040)

/* Control Register 0 */
#define MSLVCTRL0 (MSLV_BASE_ADDR + 0x2080)

/* Soft Reset register */
#define MSLVSRST (MSLV_BASE_ADDR + 0x2600)

#define SLAVE_ADDR_MODE_MASK 0xFFFFFFFC
#define SLAVE_SINGLE_WRITE 0x00
#define SLAVE_SINGLE_READ 0x01
#define SLAVE_BLOCK_WRITE 0x02
#define SLAVE_BLOCK_READ 0x03

/* Control Register 1 */
#define MSLVCTRL1 (MSLV_BASE_ADDR + 0x20c0)

#define MSLVCTRL1_POLL_MASK 0x07000000
#define MSLAVE_READY(v) ((v & MSLVCTRL1_POLL_MASK) == MSLVCTRL1_POLL_MASK)
#define LTP_THREAD_NUM 0 /* Since, only one thread exists */

/* Thread completion signature */
#define RPU_THRD_EXEC_SIG_OFFSET 0x00066CBC
#define RPU_THRD_EXEC_SIG 0x5A5A5A5A

#define MAX_LOAD_MEM_LEN 4096
/*********************************************************************
 */
#endif /* _RPU_H_ */
