/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _HAL_COMMON_H_
#define _HAL_COMMON_H_
#include "rpu.h"

typedef int (*msg_handler)(void *nbuff);
extern struct hal_priv *hpriv;
extern char *mac_addr;
extern const char *hal_name;
struct device *hal_get_dev(void);
extern bool waiting_for_rpu_ready;
extern bool block_rpu_comm;

enum PROBE_STATUS {
	PROBE_INIT,
	PROBE_TRIGGERED,
	PROBE_CALLED,
	PROBE_SUCCESS,
	PROBE_FAILED
};
	
enum RPU_SLEEP_TYPE {
	RPU_SLEEP = 0,
	RPU_AWAKE,
};

enum IO_TX_CMD_TYPE {
	IO_TX_PKT_CMD = 1,
	IO_TX_PKT_DATA,
	IO_TX_PKT_PATCH,
};

enum IO_TX_PKT_TYPE {
	IO_TX_PKT_MPDU = 1,
	IO_TX_PKT_AMSDU,
	IO_TX_PKT_AMPDU,
};

struct io_tx_ctrl_info {
	unsigned char type;
	unsigned char cmd_id;
	unsigned char pkt_type;
	unsigned char ampdu_seq;
	unsigned int patch_len;
};

typedef struct {
	void *parent;  /* some external entity that the thread supposed to work for */
	char *proc_name;
	struct task_struct *p_task;
	long thr_pid;
	int prio; /* priority */
	struct semaphore sema;
	int terminated;
	struct completion completed;
	spinlock_t spinlock;
	int up_cnt;
} tsk_ctl_t;

#define TRUE 1
#define FALSE 0

#define PROC_START(thread_func, owner, tsk_ctl, flags, name) \
{ \
        sema_init(&((tsk_ctl)->sema), 0); \
        init_completion(&((tsk_ctl)->completed)); \
        (tsk_ctl)->parent = owner; \
        (tsk_ctl)->proc_name = name;  \
        (tsk_ctl)->terminated = FALSE; \
        (tsk_ctl)->p_task  = kthread_run(thread_func, tsk_ctl, (char*)name); \
        (tsk_ctl)->thr_pid = (tsk_ctl)->p_task->pid; \
        spin_lock_init(&((tsk_ctl)->spinlock)); \
}

#define PROC_STOP(tsk_ctl) \
{ \
        (tsk_ctl)->terminated = TRUE; \
        smp_wmb(); \
        up(&((tsk_ctl)->sema)); \
        wait_for_completion(&((tsk_ctl)->completed)); \
        RPU_INFO_HAL("%s(): thread:%s:%lx terminated OK\n", __func__, \
                         (tsk_ctl)->proc_name, (tsk_ctl)->thr_pid); \
        (tsk_ctl)->thr_pid = -1; \
}

#define TXRX_DATA_LOCK
#define TX_USE_THREAD

typedef int (*fw_bring_up_func)(void *priv);
typedef int (*fw_tear_down_func)(void *priv);

extern enum PROBE_STATUS probe_status;
struct hal_priv {
        /* Pointer to the bus device for e.g. PCI dev, Platform dev etc */
        void *bus_dev;
	
	/* RPU Host RAM mappings*/
	void __iomem *base_addr_rpu_host_ram;
	void __iomem *tx_base_addr_rpu_host_ram;
	void __iomem *rx_base_addr_rpu_host_ram;

	/* RPU and GRAM mappings */
	unsigned long rpu_mem_addr;
	unsigned long gram_mem_addr;
	unsigned long rpu_sysbus_base_addr;
	unsigned long rpu_perip_base_addr;
	unsigned long gram_base_addr;
	unsigned long shm_offset;
	unsigned long hal_disabled;
	unsigned long hal_init;
	unsigned long gram_b4_addr;

	/* DTS entries */
	unsigned long rpu_sysbus_base;
	unsigned long rpu_sysbus_len;
	unsigned long rpu_pkd_gram_base;
	unsigned long rpu_pkd_gram_len;
	unsigned long rpu_gram_base;
	unsigned long rpu_gram_len;

	/* TX */
	struct sk_buff_head txq;
	//struct tasklet_struct tx_tasklet;
#ifdef TX_USE_THREAD
	tsk_ctl_t thr_tx_ctl;
#else
	struct work_struct tx_work;
	struct workqueue_struct *tx_wkq;
#endif
	unsigned short cmd_cnt;
	struct buf_info *tx_buf_info;
	struct hal_tx_data *hal_tx_data;
	int max_txq_len;

	/* RX */
	struct sk_buff_head rxq;
	//struct tasklet_struct rx_tasklet;
	//struct tasklet_struct recv_tasklet;
	struct work_struct rx_work;
	struct workqueue_struct *rx_wkq;
	tsk_ctl_t thr_rx_ctl;
	unsigned short event_cnt;
	msg_handler rcv_handler;
	struct buf_info *rx_buf_info;
	unsigned char *rx_tmp_buf;
	int max_rxq_len;

	/* Buffers info from IF layer*/
	unsigned int tx_bufs;
	unsigned int rx_bufs_2k;
	unsigned int rx_bufs_12k;
	unsigned int max_data_size;

	/* Temp storage to refill first and process next*/
	struct sk_buff_head refillq;
	int irq;
	int irq_flags;
	unsigned char *rf_params;
	struct tasklet_struct rpu_ready_tasklet;

	struct platform_device *plat_dev;

	struct host_io_info *io_info;

#ifdef TXRX_DATA_LOCK
	struct mutex txrx_mutex;
#endif
	fw_bring_up_func fw_bring_up_func;
	fw_tear_down_func fw_tear_down_func;

	struct work_struct fw_err_work;
	int fw_error;
	int fw_error_counter;
	int fw_error_counter_scan;
	int fw_error_processing;
	int fw_error_reason;
	int lpw_error_counter;
	int fw_error_cmd_done;
	struct wake_lock fw_err_lock;
	int during_fw_download;
	int shutdown;

	int during_pm_resume;
	struct notifier_block pm_notifier;
};

#define HAL_HOST_ZONE_DMA_LEN (64 * 1024 * 1024)
#define HAL_HOST_BOUNCE_BUF_LEN (4 * 1024 * 1024)
#define HAL_HOST_NON_BOUNCE_BUF_LEN (60 * 1024 * 1024)

/*Porting information:
 * HAL_RPU_IRQ_LINE: This is the interrupt number assigned to RPU host port
 *                    interrupt.
 * HAL_HOST_RPU_RAM_START: This is the physical address of the start of
 *                          Host RAM which is reserved for RPU
 * HAL_HOST_ZONE_DMA_START: This is the physical address of the start of 64MB
 *                          ZONE_DMA area which is currently assigned a dummy
 *                          value of 0xABABABAB. TSB needs to provide the actual
 *                          value for this.
 *
 * These are the only values which need to be modified as per host memory
 * map and interrupt configuration.
 * The values for HAL_SHARED_MEM_OFFSET, HAL_WLAN_GRAM_LEN,  HAL_COMMAND_OFFSET,
 * and  HAL_EVENT_OFFSET can be changed by in future software releases.
 */

#define HAL_HOST_SYSBUS_BASE_OFF 0x00e00000
#define HAL_HOST_PKD_GRAM_BASE_OFF 0x00c00000
#define HAL_HOST_B4_GRAM_BASE_OFF 0x00000000

#define HAL_HOST_RPU_LEN 0x0003E800
#define HAL_RPU_GRAM_BASE 0xB7000000

#ifdef RPU_SLEEP_ENABLE
/* RPU Sleep Controller registers
 */
#define SLEEP_CONTROLLER_BASE_ADDR (hpriv->rpu_sysbus_base_addr + 0x02C00)
#define UCC_SLEEP_CTRL_WAKEUP_TIME (SLEEP_CONTROLLER_BASE_ADDR + 0x14)
#endif

/* DDR_PHYS_WLN_BASE */
#define HAL_HOST_RPU_RAM_START 0x02C00000

#define HAL_HOST_RPU_RAM_LEN (4 * 1024 * 1024)


/**
 * struct buf_info - Structure to hold context information for TX/RX buffers.
 * @dma_buf: The DMA mapped address of the TX/RX buffer.
 * @src_ptr: Starting address of the buffer containing the TX/RX frame.
 * @dma_buf_len: The length of the DMA mapped buffer.
 * @dma_buf_priv: Indicates whether the TX/RX buffer is in the bounce buffer
 *                area.
 * @skb: Address of the network buffer which is being used for the TX/RX frame.
 *
 * This structure contains context information about a TX/RX buffer and holds
 * information which is needed once the Firmware has finished processing the
 * buffer.
 */
struct buf_info {
	dma_addr_t dma_buf;
	void __iomem *src_ptr;
	unsigned int dma_buf_len;
	unsigned int dma_buf_priv;   /* Is the DMA buffer in our private area */
	struct sk_buff *skb;
} __packed;

int _rpu_umac_if_init(struct proc_dir_entry **);
void _rpu_umac_if_exit(void);
int reset_hal_params(void);

static inline void hal_rpu_read(struct hal_priv *hpriv,
				    unsigned long base,
				    unsigned long offset,
				    unsigned int *data)
{
	if (base == RPU_SYSBUS_REG)
		*data = readl((void __iomem *)(hpriv->rpu_sysbus_base_addr + offset));
	else if (base == RPU_GRAM_PACKED)
		*data = readl((void __iomem *)(hpriv->gram_base_addr + offset));
	else if (base == RPU_GRAM_MSB)
		*data = readl((void __iomem *)(hpriv->gram_b4_addr + offset));
}

static inline void hal_rpu_write(struct hal_priv *hpriv,
				     unsigned long base,
				     unsigned long offset,
				     unsigned int data)
{
	if (base == RPU_SYSBUS_REG)
		writel(data, (void __iomem *)(hpriv->rpu_sysbus_base_addr+ offset));
	else if (base == RPU_GRAM_PACKED)
		writel(data, (void __iomem *)(hpriv->gram_base_addr + offset));
	else if (base == RPU_GRAM_MSB)
		writel(data, (void __iomem *)(hpriv->gram_b4_addr + offset));
}

/**
 * struct hal_ops_tag - This structure has ops which are used by the Host to
 *                      interface with the RPU.
 * @init: Setup Memory Mapping of RPU Regions.
 *        This op is called at startup before any messages are sent/received.
 *        The HAL performs local initializations and sets up the memory
 *        mappings of different RPU regions (like GRAM/SYSBUS etc).
 *        This op can sleep.
 *
 * @deinit: Unmap the RPU regions.
 *          This op is called when the services of HAL are no longer needed.
 *          The HAL frees up resources and unmaps the RPU regions mapped by
 *          @init.
 *          This op can sleep.
 *
 * @start: This op is used to kick off the communication between the Host and
 *         the RPU once the Host is ready to process messages from the RPU.
 *
 * @stop: This op is used to shutdown the communication between the Host and
 *        RPU.
 *
 * @register_callback: This op is used to register a handler which will be
 *                     invoked by Host HAL to pass RPU events to the next
 *                     layer.
 *
 * @send: This op is used to send commands to RPU.
 *
 * @init_bufs: Program the information about RX buffers to RPU and creates
 *             mapping tables to maintain context information for TX and RX
 *             buffers.
 *             This op is invoked to inform the HAL that it has to prepare for
 *             receiving WLAN data packets from the RPU. This op allocates a
 *             pool of RX buffers in Host memory and then programs the
 *             Descriptor ID and DMAable address of each buffer to the RPU
 *             using @cmd_hal.
 *             This op can sleep.
 *             Returns 0 on success and non-zero on failure.
 *
 * @deinit_bufs: This op is used to free the resources allocated using
 *               @init_bufs.
 *
 * @map_tx_buf: DMA mapping for a Tx buffer.
 *              This op is invoked prior to invoking the @send op for messages
 *              which have payloads associated with them (currently only
 *              @cmd_tx_ctrl). The Host HAL performs the following things via
 *              this op -
 *                 a. Bounce Buffer handling - Making sure that the TX buffer
 *                    falls within a DMAable region, else copying it to a bounce
 *                    buffer.
 *                 b. DMA mapping - DMA mapping the TX buffer/Bounce buffer. The
 *                    result of the mapping is communicated back to the caller.
 *                 c. Mapping table updation - Updates the information about the
 *                    TX buffer (@buf_info) in a mapping table indexed by
 *                    Descriptor ID's.
 *              Returns 0 on success and error on failure.
 *
 * @unmap_tx_buf: Unmapping of a TX buffer.
 *                This op is invoked when driver receives @umac_event_tx_done
 *                from the RPU which indicates that the processing of a TX
 *                frame has been completed. The Host HAL performs the following
 *                things via this op -
 *                 a. DMA unmapping - DMA unmapping the TX buffer/Bounce buffer
 *                    which was mapped using @map_tx_buf. The information about the
 *                    buffer is retrieved from the mapping table using the
 *                    Descriptor ID as an index.
 *                 b. Mapping table updation - Clears off the entry in the
 *                    mapping table corresponding to the Descriptor ID.
 *
 * @reset_hal_params: This op initializes the Command and Event counts (which are
 *                    used by the Host and Firmware HAL's to validate the
 *                    interrupts).
 *
 * @enable_irq_wake: This op is used to inform the Host to keep the IRQ active
 *                   even when it goes to a low power state.
 *
 * @disable_irq_wake: This op is used to inform the Host to disable the IRQ
 *                    when it goes to a low power state.
 *
 * @get_dev: This op is used to return OS specific device structure depending on
 *           the bus type.
 *
 * @trigger_timed_sleep: Trigger the LPW to enter in to Sleep and wakeup
 *			 after a timeout.
 *
 * @trigger_wakeup: Trigger the LPW to wakeup, this will assert/de-assert
 *		    the WAKEUP_NOW signal.
 *
 * @rpu_sleep_status: Query the sleep controller about the state of RPU Sleep.
 *
 * @get_dump_gram: This op is is used to return the starting pointer
 *		   to GRAM dump.
 *
 * @get_dump_core: This op is is used to return the starting pointer
 *		   to CORE dump.
 *
 * @get_dump_perip: This op is is used to return the starting pointer
 *		    to PERIP dump.
 *
 * @get_dump_sysbus: This op is is used to return the starting pointer
 *		     to SYSBUS dump.
 *
 * @get_dump_len: This op is is used to return the length of the dump
 * 		  for a give region.
 *
 * @rpu_set_mem: This op is is used to set the memory of RPU.
 *
 * @rpu_read_mem: This op is is used to to read from the RPU memory
 *		  to local memory.
 *
 * @rpu_write_mem: This op is is used to write from the local memory
 *		   to RPU memory.
 *
 * @mtx_start_thread: This op is is used to start the MTX threads
 *		      (Applicable only for META)
 *
 * @mtx_stop_thread: This op is is used to stop the MTX threads.
 *		      (Applicable only for META)
 *
 * These APIs allow the upper parts of the Host driver to control the HAL.
 * None of these APIs can put the caller to sleep unless stated explicitly in
 * the description.
 */
struct hal_ops_tag {
	int (*init)(void *dev);
	int (*deinit)(void *dev);
	int (*start)(void);
	int (*stop)(void);
	void (*register_callback)(msg_handler);
	void (*send)(void* msg, void* payload, unsigned int descriptor_id);
	int (*init_bufs)(unsigned int tx_bufs,
			 unsigned int rx_bufs_2k,
			 unsigned int rx_bufs_12k,
			 unsigned int tx_max_data_size);
	void (*deinit_bufs)(void);
	int (*map_tx_buf)(int pkt_desc,
			  int frame_id,
			  unsigned char * data,
			  int len,
			  dma_addr_t *dma_addr);
	int (*unmap_tx_buf)(int pkt_desc, int frame_id);
	int (*reset_hal_params)(void);
#ifdef CONFIG_PM
	void (*enable_irq_wake)(void);
	void (*disable_irq_wake)(void);
#endif
	struct device * (*get_dev)(void);
#ifdef RPU_SLEEP_ENABLE
	void (*trigger_timed_sleep)(int val);
	void (*trigger_wakeup)(enum RPU_SLEEP_TYPE);
	bool (*rpu_sleep_status)(void);
#endif
	int (*rpu_set_mem)(unsigned int *dst,
			     unsigned int val,
			     unsigned int len);
	int (*rpu_read_mem)(unsigned int *src,
			     unsigned int *dst,
			     unsigned int len);
	int (*rpu_write_mem)(unsigned int *src,
			     unsigned int *dst,
			     unsigned int len);
	void (*mtx_start_thread)(unsigned int thrd_num,
				 unsigned int stack_ptr,
				 unsigned int prog_ctr,
				 unsigned int catch_state_addr);
	void (*mtx_stop_thread)(unsigned int thrd_num);

        void (*set_mem_region)(unsigned int);
        void (*request_mem_regions)(unsigned char **,
                                    unsigned char **,
                                    unsigned char **);

};

extern struct hal_ops_tag hal_ops;
#endif /* _HAL_H_ */

/* EOF */
