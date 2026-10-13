/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/unaligned.h>

#include <linux/clk.h>
#include <linux/etherdevice.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/suspend.h>


#include "core.h"
#include "hal.h"
#include "hal_common.h"
#include "soc.h"
#include "wow.h"
#include "hal_io.h"
#include "if_io.h"
#include "platform.h"

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
#include <uapi/linux/sched/types.h>
#endif

#define ENABLE_RX_WORKQ		1
/* TODO: Remove this once we get proper register address for
 * sleep controller
 */
extern unsigned long pci_bar_addr;
#define COMMAND_START_MAGIC 0xDEAD

//static int is_mem_bounce(void *virt_addr, int len);

/* compliant ioremap */
#define VIRT_TO_PHYS(addr) \
		(HAL_HOST_RPU_RAM_START + \
		 (addr) - \
		 hpriv->base_addr_rpu_host_ram)
const char *hal_name = "RPU_WIFI_HAL";
static unsigned long shm_offset = HAL_SHARED_MEM_OFFSET;
module_param(shm_offset, ulong, S_IRUSR|S_IWUSR);

static unsigned int hal_cmd_sent = 0;
static unsigned int hal_cmd_tx_send = 0;
static unsigned int hal_event_recv = 0;
static unsigned int hal_event_tx_done = 0;
static unsigned int hal_event_cmd_proc_done = 0;
static unsigned int hal_event_rx_recv = 0;
static unsigned int hal_event_interrupts = 0;
static unsigned int hal_event_rx_counts_one_interrupts[8];
static unsigned int hal_event_rx_counts_one_packet[8];
//static struct timer_list stats_timer;
static unsigned int alloc_skb_failures;

//static unsigned int rpu_ddr_base;


/* for send and receive count. */
static unsigned long tx_cnt;
static unsigned long rx_cnt;
/*RPU_DEBUG_HAL */

bool block_rpu_comm;

#ifdef RPU_SLEEP_ENABLE
enum RPU_SLEEP_TYPE rpu_sleep_status = RPU_SLEEP;
bool waiting_for_rpu_ready = false;
#endif

unsigned char vif_macs[2][ETH_ALEN];
char *mac_addr;

module_param(mac_addr, charp, 0000);
MODULE_PARM_DESC(mac_addr, "Configure wifi base mac address");


/* Range check */
#define CHECK_EVENT_ADDR_RPU(x) ((x) >= HAL_RPU_GRAM_BASE && (x) <=\
				  (HAL_RPU_GRAM_BASE + \
				  hpriv->rpu_pkd_gram_len))

#define CHECK_EVENT_STATUS_ADDR_RPU(x) ((x) >= HAL_RPU_GRAM_BASE && (x) <=\
					 (HAL_RPU_GRAM_BASE + \
					 hpriv->rpu_pkd_gram_len))

#define CHECK_EVENT_LEN(x) ((x) < 0x5000)
#define CHECK_RX_PKT_CNT(x) ((x) >= 1 && (x) <= 16)
/* #define CHECK_SRC_PTR(x, y) ((x) >= (y) && (x) <= (y) +
 * HAL_HOST_BOUNCE_BUF_LEN)
 */
#define CHECK_PKT_DESC(x) ((x) < (hpriv->rx_bufs_2k + hpriv->rx_bufs_12k))
/* MAX_RX_BUFS */




#ifdef RPU_SLEEP_ENABLE

int check_and_wakeup_rpu_nonblocking(void)
{
	return true;
}

static void trigger_timed_sleep(int val)
{
}
 
static bool get_rpu_sleep_status(void)
{
	return RPU_AWAKE;
}

static void trigger_wakeup(enum RPU_SLEEP_TYPE val)
{
	return;
}
#endif /* RPU_SLEEP_ENABLE */

static int hal_reset_hal_params(void)
{
	hpriv->cmd_cnt = COMMAND_START_MAGIC;
	hpriv->event_cnt = 0;
	return 0;
}


static int hal_ready(struct hal_priv *priv)
{
	return 1;
}

#ifdef SDIO_TXRX_STABILITY_TEST
void hal_send_direct(void *msg)
{
	int ret;
	struct sk_buff *skb = (struct sk_buff *)msg;

	ret = rk915_data_write(hpriv, 0, skb->data, skb->len);
	if (ret)
		RPU_ERROR_HAL("%s: ret = %d, pkt_len = %d.\n", __func__, ret, skb->len);

	dev_kfree_skb_any(skb);
}
#endif

#ifdef TX_SG_MODE
#define ARRAYSIZE(a)		(sizeof(a) / sizeof(a[0]))
static struct scatterlist sg_list[12];
#endif

#include <../net/mac80211/ieee80211_i.h>

#define CMD_RESET_BIT           (1<<0)
#define CMD_TX_POWER_BIT        (1<<1)
#define CMD_VIF_CTRL_BIT        (1<<2)
#define CMD_VIF_CFG_BIT         (1<<3)
#define CMD_TXQ_PARAMS_BIT      (1<<4)
#define CMD_CHANNEL_BIT         (1<<5)
#define CMD_PS_BIT              (1<<6)
#define CMD_MCST_ADDR_CFG_BIT   (1<<7)
#define CMD_MCST_FLTR_CTRL_BIT  (1<<8)
#define CMD_SETKEY_BIT          (1<<9)

struct lpw_recovery_param{
	u32                          flag;
	//u32                          vif_count;
	u32                          active_vifs;
	u32							 mcst_addr_count;
	u32                          active_keyset[MAX_VIFS];
	u32                          vif_cfg_valid[MAX_VIFS];
	u32                          txq_params_valid[MAX_VIFS];
	u32                          ps_para_valid[MAX_VIFS];
	u32                          patch_size;
	u32                          patch_transfer;
	#define       RECOVERY_STATUS_WAIT_RESET_COMP  1
	#define       RECOVERY_STATUS_RESET_COMP       2
	u32                 status;
	u32                          latest_UMAC_cmd;
	struct cmd_reset             reset_param;
	struct cmd_tx_pwr            tx_pwr_param;
	struct cmd_vifctrl           vifctrl_param[MAX_VIFS];
	struct cmd_vif_cfg           vif_cfg_param[MAX_VIFS];
	struct cmd_txq_params        txq_params[MAX_VIFS][4];
	struct cmd_channel           channel_params;
	struct cmd_ps                ps_param[MAX_VIFS];
	struct cmd_mcst_addr_cfg     mcst_addr_cfg_param[MCST_ADDR_LIMIT];
	struct cmd_mcst_filter_ctrl  mcst_fltr_ctrl_param;
	struct cmd_setkey            setkey_param[MAX_VIFS][NUM_DEFAULT_KEYS+NUM_DEFAULT_MGMT_KEYS];
};

static struct lpw_recovery_param g_lpw_recovery_param;

static void init_fw_recovery_cmd(void)
{
	RPU_DEBUG_ROCOVERY("%s\n", __func__);
	memset(&g_lpw_recovery_param, 0, sizeof(struct lpw_recovery_param));
}

static int save_fw_recovery_cmd(void *cmd)
{
	struct host_rpu_msg_hdr *phdr = (struct host_rpu_msg_hdr *)cmd;

    switch(phdr->id)
	{
		case RPU_CMD_RESET:
		{
			u32  counter;
			struct cmd_reset *cmd_rst = (struct cmd_reset *)cmd;
			/* Reset context. 
			   UMAC_CMD_RESET should be the first or last command
			 */
			if (cmd_rst->type == LMAC_DISABLE)
				break;

			g_lpw_recovery_param.flag = 0;
			g_lpw_recovery_param.active_vifs = 0;
			g_lpw_recovery_param.mcst_addr_count = 0;
			//g_lpw_recovery_param.vif_count = 0;

			for(counter = 0; counter < MAX_VIFS; counter++)
			{
				g_lpw_recovery_param.vif_cfg_param[counter].changed_bitmap = 0;
				g_lpw_recovery_param.active_keyset[counter] = 0;
				g_lpw_recovery_param.vif_cfg_valid[counter] = 0;
				g_lpw_recovery_param.txq_params_valid[counter] = 0;
				g_lpw_recovery_param.ps_para_valid[counter] = 0;
			}
			
			memcpy((void *)&g_lpw_recovery_param.reset_param,
				(void *)cmd,sizeof(struct cmd_reset));

			//if(g_lpw_recovery_param.reset_param.type != LMAC_DISABLE)
			{
				g_lpw_recovery_param.flag |= CMD_RESET_BIT;
			}
			break;
		}
		case RPU_CMD_TX_POWER:
		{
			memcpy((void*)&g_lpw_recovery_param.tx_pwr_param, 
				(void *)cmd, sizeof(struct cmd_tx_pwr));
			g_lpw_recovery_param.flag |= CMD_TX_POWER_BIT;
			break;
		}
		case RPU_CMD_VIF_CTRL:
		{
			struct cmd_vifctrl *vifctrl;
			u32                 vif_index;

			vifctrl = (struct cmd_vifctrl *)cmd;
			vif_index = vifctrl->if_index;
			
			if(vifctrl->if_ctrl == IF_ADD)
			{
				/*
				for(vif_index = 0; vif_index < MAX_VIFS; vif_index ++)
				{
					if(!(g_lpw_recovery_param.active_vifs & (1<<vif_index)))
						break;
				}
				*/

				if(vif_index >= MAX_VIFS)
				{
					//TODO: Print Debug Info 
					break;
				}
				else
				{
					memcpy((void *)&g_lpw_recovery_param.vifctrl_param[vif_index],
						   (void *)vifctrl, sizeof(struct cmd_vifctrl));
					g_lpw_recovery_param.active_vifs |= (1<<vif_index);
					g_lpw_recovery_param.flag |= CMD_VIF_CTRL_BIT;
				}
			}
			else if(vifctrl->if_ctrl == IF_REM)
			{
				g_lpw_recovery_param.active_vifs &= ~(1<<vif_index);
				memset((void *)&g_lpw_recovery_param.vifctrl_param[vif_index], 
					0, sizeof(struct cmd_vifctrl));
				//Caution: Re-use of local variable 'vif_index'
				for(vif_index = 0; vif_index < MAX_VIFS; vif_index++)
				{
					if(g_lpw_recovery_param.active_vifs & (1<<vif_index))
						break;
				}
				if(vif_index == MAX_VIFS)
					g_lpw_recovery_param.flag &= ~CMD_VIF_CTRL_BIT;
			}
			
			break;
		}
		case RPU_CMD_VIF_CFG:
		{

			u32 vif_index;
			u32 changed_bitmap;
			struct cmd_vif_cfg *new_vif_cfg;
			struct cmd_vif_cfg *old_vif_cfg;

			new_vif_cfg = (struct cmd_vif_cfg *)cmd;

			vif_index = new_vif_cfg->if_index;
			changed_bitmap = new_vif_cfg->changed_bitmap;

			if(vif_index >= MAX_VIFS)
			{
				//TODO: Print Debug Info 
				break;
			}
			else
			{
			
				old_vif_cfg = (struct cmd_vif_cfg *)&g_lpw_recovery_param.vif_cfg_param[vif_index];

				if(g_lpw_recovery_param.vif_cfg_valid[vif_index] != 1)
				{
					memcpy((void *)old_vif_cfg, (void *)new_vif_cfg, sizeof(struct cmd_vif_cfg));
					g_lpw_recovery_param.vif_cfg_valid[vif_index] = 1;
				}
				else
				{
					//TODO: Need to revise changed item, because some items are not set via VIF_CFG
					if(changed_bitmap & BASICRATES_CHANGED)
					{
						old_vif_cfg->basic_rate_set = new_vif_cfg->basic_rate_set;
					}
					if(changed_bitmap & SHORTSLOT_CHANGED)
					{
						old_vif_cfg->use_short_slot = new_vif_cfg->use_short_slot;
					}
					if(changed_bitmap & ATIMWINDOW_CHANGED)
					{
						old_vif_cfg->atim_window = new_vif_cfg->atim_window;
					}
					if(changed_bitmap & AID_CHANGED)
					{
						old_vif_cfg->aid = new_vif_cfg->aid;
					}
					if(changed_bitmap & CAPABILITY_CHANGED)
					{
						old_vif_cfg->capability = new_vif_cfg->capability;
					}
					if(changed_bitmap & SHORTRETRY_CHANGED)
					{
						old_vif_cfg->short_retry = new_vif_cfg->short_retry;
					}
					if(changed_bitmap & LONGRETRY_CHANGED)
					{
						old_vif_cfg->long_retry = new_vif_cfg->long_retry;
					}
					if(changed_bitmap & BSSID_CHANGED)
					{
						memcpy((void *)&old_vif_cfg->bssid[0],
								(void *)&new_vif_cfg->bssid[0], ETH_ALEN*sizeof(unsigned char));
					}
					if(changed_bitmap & RCV_BCN_MODE_CHANGED)
					{
						old_vif_cfg->bcn_mode = new_vif_cfg->bcn_mode;
					}
					if(changed_bitmap & BCN_INT_CHANGED)
					{
						old_vif_cfg->beacon_interval = new_vif_cfg->beacon_interval;
					}
					if(changed_bitmap & DTIM_PERIOD_CHANGED)
					{
						old_vif_cfg->dtim_period = new_vif_cfg->dtim_period;
					}
					if(changed_bitmap & SMPS_CHANGED)
					{
						old_vif_cfg->smps_info = new_vif_cfg->smps_info;
					}
					if(changed_bitmap & CONNECT_STATE_CHANGED)
					{
						old_vif_cfg->connect_state = new_vif_cfg->connect_state;
					}
					if(changed_bitmap & OP_CHAN_CHANGED)
					{
						old_vif_cfg->op_channel = new_vif_cfg->op_channel;
					}
					old_vif_cfg->changed_bitmap |= changed_bitmap;

					//There is no vif_addr added in the RCV_BCN_MODE_CHANGED config
					if(changed_bitmap != RCV_BCN_MODE_CHANGED)
						memcpy((void*)&old_vif_cfg->vif_addr[0],
							(void *)&new_vif_cfg->vif_addr[0], ETH_ALEN*sizeof(unsigned char));
				}
			}
			
			g_lpw_recovery_param.flag |= CMD_VIF_CFG_BIT;
			break;
		}

		case RPU_CMD_TXQ_PARAMS:
		{
			u32 vif_index;
			u32 queue_num;
			struct cmd_txq_params * txq_params;

			txq_params = (struct cmd_txq_params *)cmd;

			vif_index = txq_params->if_index;
			queue_num = txq_params->queue_num;

			if(vif_index >= MAX_VIFS || queue_num >= 4)
			{
				//TODO: Print Debug Info here
				break;
			}
			else
			{
				memcpy((void *)&g_lpw_recovery_param.txq_params[vif_index][queue_num],
					(void *)txq_params, sizeof(struct cmd_txq_params));
				g_lpw_recovery_param.txq_params_valid[vif_index] |= (1<<queue_num);
				g_lpw_recovery_param.flag |= CMD_TXQ_PARAMS_BIT;
			}
			break;
		}
		case RPU_CMD_CHANNEL:
		{
			memcpy((void *)&g_lpw_recovery_param.channel_params,
				(void *)cmd, sizeof(struct cmd_channel));
			g_lpw_recovery_param.flag |= CMD_CHANNEL_BIT;
			break;
		}
		case RPU_CMD_PS:
		{
			u32 vif_index;
			struct cmd_ps * ps;

			ps = (struct cmd_ps *)cmd;
			vif_index = ps->if_index;

			if(vif_index >= MAX_VIFS)
			{
				//TODO: Print Debug Info
				break;
			}
			else
			{
				memcpy((void *)&g_lpw_recovery_param.ps_param[vif_index],
					(void *)ps, sizeof(struct cmd_ps));
				g_lpw_recovery_param.ps_para_valid[vif_index] = 1;
				g_lpw_recovery_param.flag |= CMD_PS_BIT;
			}
			break;
		}
		case RPU_CMD_MCST_ADDR_CFG:
		{
			u32 op;
			struct cmd_mcst_addr_cfg * mcst_addr_cfg;

			mcst_addr_cfg = (struct cmd_mcst_addr_cfg *)cmd;

			op = mcst_addr_cfg->op;

			if(op == WLAN_MCAST_ADDR_REM)
			{
				/* Play a little trick here, when calling prepare_multicast() function in Host Driver,
				   It always remove all the old mcst addresses, and config new ones
				*/
				g_lpw_recovery_param.mcst_addr_count = 0;
				g_lpw_recovery_param.flag &= ~CMD_MCST_ADDR_CFG_BIT;
			}
			else if(op == WLAN_MCAST_ADDR_ADD)
			{
				if(g_lpw_recovery_param.mcst_addr_count < MCST_ADDR_LIMIT)
				{
					memcpy((void *)&g_lpw_recovery_param.mcst_addr_cfg_param[g_lpw_recovery_param.mcst_addr_count],
						(void *)mcst_addr_cfg, sizeof(struct cmd_mcst_addr_cfg));
					g_lpw_recovery_param.mcst_addr_count++;
					g_lpw_recovery_param.flag |= CMD_MCST_ADDR_CFG_BIT;
				}
			}
			break;
		}
		case RPU_CMD_MCST_FLTR_CTRL:
		{
			memcpy((void *)&g_lpw_recovery_param.mcst_fltr_ctrl_param,
				(void *)cmd, sizeof(struct cmd_mcst_filter_ctrl));
			g_lpw_recovery_param.flag |= CMD_MCST_FLTR_CTRL_BIT;
			break;
		}
		case RPU_CMD_SETKEY:
		{
			u32 vif_index;
			u32 key_id;
			struct cmd_setkey * setkey;

			setkey = (struct cmd_setkey *)cmd;

			vif_index = setkey->if_index;
			key_id = setkey->key_id;

			if(vif_index < MAX_VIFS && key_id < (NUM_DEFAULT_KEYS+NUM_DEFAULT_MGMT_KEYS))
			{
				if(setkey->ctrl == KEY_CTRL_ADD)
				{
					g_lpw_recovery_param.active_keyset[vif_index] |= (1<<(key_id));
					memcpy((void *)&g_lpw_recovery_param.setkey_param[vif_index][key_id],
						(void *)setkey, sizeof(struct cmd_setkey));
				}
				else if(setkey->ctrl == KEY_CTRL_DEL)
				{
					g_lpw_recovery_param.active_keyset[vif_index] &= ~(1<<(key_id));
				}
			}

			//CAUTION: Re-use vif_index
			for(vif_index = 0; vif_index < MAX_VIFS; vif_index++)
			{
				if(g_lpw_recovery_param.active_keyset[vif_index] != 0)
					break;
			}
			if(vif_index == MAX_VIFS)
			{
				//No activated key
				g_lpw_recovery_param.flag &= ~CMD_SETKEY_BIT;
			}
			else
			{
				g_lpw_recovery_param.flag |= CMD_SETKEY_BIT;
			}
			break;
		}
		case RPU_CMD_TX:
		{
			struct cmd_tx_ctrl * cmd_tx;
			struct img_priv *priv = wifi->hw->priv;

			cmd_tx = (struct cmd_tx_ctrl *)cmd;
			priv->tx.tx_desc_had_send_to_io[cmd_tx->descriptor_id] = 1;
			break;
		}
		default:
			break;
	}

	return 0;
}

static void cmd_send(u8 *cmd, int len)
{
	struct sk_buff *cmd_skb;

	/* the address of cmd should 4bytes aligned */
	cmd_skb = alloc_skb(len, GFP_ATOMIC);
	if (cmd_skb) {
		hpriv->fw_error_cmd_done = 0;
		
		memcpy(skb_put(cmd_skb, len), cmd, len);
		rk915_data_write(hpriv, 0, (void *)cmd_skb->data, len);
		dev_kfree_skb_any(cmd_skb);

		wait_for_fw_error_cmd_done(NULL);
	}
}

static void send_fw_recovery_cmd(void)
{
	u32 vif_index;

	/* Re-set parameters */
	if(g_lpw_recovery_param.flag & CMD_RESET_BIT)
	{
		//RESET CMD(LMAC_ENABLE)
		RPU_DEBUG_ROCOVERY("send CMD_RESET\n");
		g_lpw_recovery_param.status = RECOVERY_STATUS_WAIT_RESET_COMP;
		cmd_send((u8 *)&g_lpw_recovery_param.reset_param, sizeof(struct cmd_reset));

		//Wait for RESET COMPLETE
		while(1)
		{
			//RPU_DEBUG_ROCOVERY("wait reset complet %d\n", g_lpw_recovery_param.status);
			if(g_lpw_recovery_param.status == RECOVERY_STATUS_RESET_COMP)
			{	
				break;
			}
			msleep(50);
		}

		if (hpriv->fw_error_reason == FW_ERR_RESET_CMD ||
			g_lpw_recovery_param.reset_param.type == LMAC_DISABLE) {
			return;
		}
		
		//TX POWER
		if(g_lpw_recovery_param.flag & CMD_TX_POWER_BIT)
		{
			RPU_DEBUG_ROCOVERY("send CMD_TX_POWER\n");
			cmd_send((u8 *)&g_lpw_recovery_param.tx_pwr_param, sizeof(struct cmd_tx_pwr));
		}

		//Add VIFs/VIF config/TXQ params/PS params/Set Key
		if(g_lpw_recovery_param.flag & CMD_VIF_CTRL_BIT)
		{
			RPU_DEBUG_ROCOVERY("send CMD_VIF_CTRL\n");
			for(vif_index = 0; vif_index < MAX_VIFS; vif_index++)
			{
				if(g_lpw_recovery_param.active_vifs & (1<<vif_index))
				{
					//Add VIF
					cmd_send((u8 *)&g_lpw_recovery_param.vifctrl_param[vif_index], sizeof(struct cmd_vifctrl));

					//VIF config
					if((g_lpw_recovery_param.flag & CMD_VIF_CFG_BIT) && (g_lpw_recovery_param.vif_cfg_valid[vif_index] == 1))
					{
						cmd_send((u8 *)&g_lpw_recovery_param.vif_cfg_param[vif_index], sizeof(struct cmd_vif_cfg));
					}

					//TXQ params
					if(g_lpw_recovery_param.flag & CMD_TXQ_PARAMS_BIT) 
					{
						u32 queue_num;
						for(queue_num = 0; queue_num < 4; queue_num++)
						{
							if(g_lpw_recovery_param.txq_params_valid[vif_index] & (1<<queue_num))
							{
								cmd_send((u8 *)&g_lpw_recovery_param.txq_params[vif_index][queue_num], sizeof(struct cmd_txq_params));
							}
						}
					}

					//PS param
					if((g_lpw_recovery_param.flag & CMD_PS_BIT) && (g_lpw_recovery_param.ps_para_valid[vif_index] == 1))
					{
						cmd_send((u8 *)&g_lpw_recovery_param.ps_param[vif_index], sizeof(struct cmd_ps));
					}

					//Set Key
					if((g_lpw_recovery_param.flag & CMD_SETKEY_BIT) && (g_lpw_recovery_param.active_keyset[vif_index] != 0))
					{
						u32 key_id;

						for(key_id = 0; key_id < (NUM_DEFAULT_KEYS+NUM_DEFAULT_MGMT_KEYS);key_id++)
						{
							if(g_lpw_recovery_param.active_keyset[vif_index] & (1<<key_id))
							{
								cmd_send((u8 *)&g_lpw_recovery_param.setkey_param[vif_index][key_id], sizeof(struct cmd_setkey));
							}
						}
					}
				}
			}
		}

		//Channel
		if(g_lpw_recovery_param.flag & CMD_CHANNEL_BIT)
		{
			RPU_DEBUG_ROCOVERY("send CMD_CHANNEL\n");
			cmd_send((u8 *)&g_lpw_recovery_param.channel_params, sizeof(struct cmd_channel));
		}

		//MCST ADDR
		if(g_lpw_recovery_param.flag & CMD_MCST_ADDR_CFG_BIT)
		{
			u32 counter;

			RPU_DEBUG_ROCOVERY("send CMD_MCST_ADDR_CFG\n");
			for(counter = 0; counter < g_lpw_recovery_param.mcst_addr_count; counter++)
			{
				cmd_send((u8 *)&g_lpw_recovery_param.mcst_addr_cfg_param[counter], sizeof(struct cmd_mcst_addr_cfg));
			}
		}

		//MCST FLTR CTRL
		if(g_lpw_recovery_param.flag & CMD_MCST_FLTR_CTRL_BIT)
		{
			RPU_DEBUG_ROCOVERY("send CMD_MCST_FLTR_CTRL\n");
			cmd_send((u8 *)&g_lpw_recovery_param.mcst_fltr_ctrl_param, sizeof(struct cmd_mcst_filter_ctrl));
		}
	}    
}

extern void rpu_process_pending_operates(void);
static void fw_err_work_fn(struct work_struct *work)
{
	RPU_DEBUG_ROCOVERY("%s\n", __func__);

	if (hpriv->shutdown)
		goto unlock_out;

	// clear pending rx_serias info 
	hpriv->io_info->rx_serias_count = 0;
	hpriv->io_info->rx_next_len = 0;

#if ENABLE_RX_WORKQ
	wait_for_rxq_empty(NULL);
#endif

	// 1. reset rk915
	RPU_DEBUG_ROCOVERY("reset\n");
	hpriv->fw_tear_down_func((void *)hpriv);
	hpriv->hal_disabled = 1;
	hpriv->fw_bring_up_func((void *)hpriv);

	// 2. recovery cmds
	RPU_DEBUG_ROCOVERY("recovery cmds\n");
	hpriv->hal_disabled = 0;
	send_fw_recovery_cmd();

	hpriv->fw_error_processing = 0;
	hpriv->fw_error = 0;

	if (hpriv->fw_error_reason != FW_ERR_RESET_CMD) {
		// 3. process unfinished tx done
		RPU_DEBUG_ROCOVERY("process unfinished tx done\n");
		rpu_tx_proc_unfi_tx_done();
	}

	// 4. restore pending command in fw error.
	RPU_DEBUG_ROCOVERY("restore pending command\n");
	rpu_process_pending_operates();

	RPU_ERROR_ROCOVERY("-------- fw error recovery end --------\n");

unlock_out:
	if (wake_lock_active(&hpriv->fw_err_lock))
		wake_unlock(&hpriv->fw_err_lock);
}

//static void tx_tasklet_fn(unsigned long data)
#ifdef TX_USE_THREAD
static int tx_thread(void *data)
#else
static void tx_work_fn(struct work_struct *work)
#endif
{
#ifdef TX_USE_THREAD
	tsk_ctl_t *tsk = (tsk_ctl_t *)data;
	struct hal_priv *priv = (struct hal_priv *)tsk->parent;
	static struct sched_param param = { .sched_priority = 1 };
#else
	struct hal_priv *priv = container_of(work, struct hal_priv, tx_work);
#endif
	struct sk_buff *skb;
	unsigned long start = 0;
	int ret;
#ifdef TX_SG_MODE
	struct cmd_tx_ctrl *tx_cmd;
	unsigned int pkt = 0;
	unsigned int new_pkt_len, total_size = 0, align_len = 0;
	sg_init_table(sg_list, ARRAYSIZE(sg_list));
#endif
	struct img_priv *imgpriv;
	struct host_rpu_msg_hdr *phdr;
#ifdef DUMP_MORE_DEBUG_INFO
	char cmd_str[64];
#endif

#ifndef TX_USE_THREAD
	if (hpriv->fw_error_processing)
		return;
#endif

#ifdef TX_USE_THREAD
	sched_set_fifo_low(current);
	//complete(&tsk->completed);
#endif

#ifdef TX_USE_THREAD
    while (1) {
    	if (down_interruptible(&tsk->sema) != 0) {
    		break;
    	}

    	if (tsk->terminated)
    		break;

    	if (hpriv->fw_error_processing || !hpriv->hal_init)
    		continue;
#endif        
    	while (1) {
    		if (skb_peek(&priv->txq) == NULL)
    			break;

    		if (block_rpu_comm) {
    			RPU_INFO_TX("%s: break with block_rpu_comm\n", __func__);
    			break;
    		}

    		skb = skb_dequeue(&priv->txq);
    		if (skb == NULL)
    			break;

    		tx_cnt++;
#if 0//def RPU_SLEEP_ENABLE
    		if (check_and_wakeup_rpu_nonblocking() == false) {
    			RPU_ERROR_HAL("%s:%d Dropping CMD , qlen: %d\n",
    				__func__,__LINE__,skb_queue_len(&priv->txq));
    			return;
    		}
#endif
    		RPU_DEBUG_HAL("%s: tx_cnt=%ld cmd_cnt=0x%X event_cnt=0x%X\n",
    				hal_name,
    				tx_cnt,
    				priv->cmd_cnt,
    				priv->event_cnt);
    		if (DUMP_HAL) {
    			RPU_DEBUG_HAL("%s: xmit dump\n", hal_name);
    			RPU_DEBUG_DUMP_HAL(" ", DUMP_PREFIX_NONE, 16, 4,
    					 skb->data, 32, 1);
    		}

    		start = jiffies;

    		while (!hal_ready(priv) &&
    		     time_before(jiffies, start + msecs_to_jiffies(1000))) {
    			cpu_relax();
    		}

    		if (!hal_ready(priv)) {
    			RPU_ERROR_HAL("%s: Intf not ready for 1000ms, dropping cmd\n",
    			       hal_name);
    			dev_kfree_skb_any(skb);
    			skb = NULL;
    		}

    		if (!skb)
    			continue;

    		/* workaround m0 died problem during resume */
    		phdr = (struct host_rpu_msg_hdr *)skb->data;
    		if (phdr->id != RPU_CMD_RESET && phdr->id != RPU_CMD_PS_ECON_CFG) {
    			imgpriv = wifi ? wifi->hw->priv:NULL;
    			wait_for_pm_resume_done(imgpriv);
            }

#ifdef DUMP_MORE_DEBUG_INFO
    		convert_cmd_to_str(phdr->id, cmd_str);
    		RPU_INFO_HAL("send %s(%d)\n", cmd_str, phdr->id);
#endif

    		save_fw_recovery_cmd(skb->data);

    		if (priv->hal_disabled) {
    			dev_kfree_skb_any(skb);
    			break;
    		}

#ifdef TXRX_DATA_LOCK
    		/*
    		  * when transmit tx cmd and continues tx data, it can't be interrupt by rx 
    		  */
    		mutex_lock(&priv->txrx_mutex);
#endif

#ifndef TX_SG_MODE
    {
    		//struct host_rpu_msg_hdr *tx_hdr = (struct host_rpu_msg_hdr *)skb->data;
    		//RPU_INFO_HAL("send %d\n", tx_hdr->id);
    }
    		// 1. send cmd first
    		ret = rk915_data_write(priv, 0, skb->data, skb->len);
    		if (ret) {
    			RPU_ERROR_HAL("%s: ret = %d, pkt_len = %d.\n",
    						  __func__, ret, skb->len);
    /*#ifdef TXRX_DATA_LOCK
    			mutex_unlock(&priv->txrx_mutex);
#endif			
    			break;*/
    		}
    		// 2. if cmd has data, send data second
    		if (rpu_is_cmd_has_data(skb->data)) {
    			hal_cmd_tx_send++;
#ifndef PKTGEN_MULTI_TX
    			rpu_send_cmd_datas(skb->data, priv);
#endif
    		}
#else
    		tx_cmd = (struct cmd_tx_ctrl *)skb->data;

    		if (tx_cmd->hdr.id == RPU_CMD_TX &&
    			tx_cmd->num_frames_per_desc > 0) {

    			RPU_INFO_TX("%s: num_frames_per_desc: %d, struct skb_shared_info size: %d.\n", __func__, tx_cmd->num_frames_per_desc, sizeof(struct skb_shared_info));

    			BUG_ON(skb->len != 512);
    			sg_set_buf(&sg_list[0], skb->data, skb->len);
    			total_size += skb->len;
    			RPU_INFO_TX("%s: ctrl frams: %p, %d\n", __func__, skb->data, skb->len);

    			for (pkt = 0; pkt < tx_cmd->num_frames_per_desc; pkt++) {
    				unsigned int pkt_len = tx_cmd->pkt_length[pkt];
    				unsigned char *pkt_data = (unsigned char *)tx_cmd->p_frame_ddr_pointer[pkt];

    				if (!pkt_data || pkt_len == 0) {
    					RPU_ERROR_TX("%s: pkt_data(%p), pkt_len(%d)\n", __func__, pkt_data, pkt_len);
    					continue;
    				}

    				if (pkt_len & 0x1f) {
    					new_pkt_len = roundup(pkt_len, 32);
    					tx_cmd->pkt_length[pkt] |= (new_pkt_len << 16);
    					sg_set_buf(&sg_list[pkt + 1], pkt_data, new_pkt_len);
    					total_size += new_pkt_len;
    				} else {
    					sg_set_buf(&sg_list[pkt + 1], pkt_data, pkt_len);
    					total_size += pkt_len;
    					tx_cmd->pkt_length[pkt] |= (pkt_len << 16);
    				}

    				RPU_INFO_TX("===%s: send: (pkt=%d) %p:[0x%x, 0x%x], %d, NEW: %d XLEN: 0x%x.\n",
    							 __func__, pkt, pkt_data, pkt_data[0], pkt_data[pkt_len -1], pkt_len, new_pkt_len, tx_cmd->pkt_length[pkt]);

    				if (((pkt + 1) == tx_cmd->num_frames_per_desc) && (total_size % 0x200)) {
    					align_len = 512 - (total_size % 0x200);
    					pr_err("align_len: %d.\n", align_len);
    					sg_list[pkt + 1].length += align_len;
    					tx_cmd->pkt_length[pkt] &= (0x0000ffff);
    					tx_cmd->pkt_length[pkt] |= ((sg_list[pkt + 1].length) << 16);
    					total_size += align_len;
    				}
    				pr_err("total_size: %d, pkt: %d, [%d, %d].\n", total_size, pkt, pkt + 1, total_size % 0x200);
    			}

    			pr_err("sg len: %d.\n", pkt + 1);
    			ret = rk915_data_write_sg(priv, 0, sg_list, pkt + 1);
    			if (ret)
    				pr_err("%s: ret = %d\n", __func__, ret);

    		} else if (tx_cmd->hdr.length > 512) {
    			if (skb->len & 0x1ff) {
    				new_pkt_len = roundup(skb->len, 512);
    			}
    			RPU_INFO_TX("%s: sdio send: %p %d, NEW: %d\n",
    						 __func__, skb->data, skb->len, new_pkt_len);
    			sg_set_buf(&sg_list[0], skb->data, new_pkt_len);
    			ret = rk915_data_write_sg(priv, 0, sg_list, 1);
    			if (ret)
    				pr_err("%s: ret = %d\n", __func__, ret);
    		} else {
    			pr_err("NO TX: skb->len: %d.\n", skb->len);
    			ret = rk915_data_write(priv, 0, skb->data, skb->len);
    			//if (ret)
    				RPU_ERROR_HAL("%s: ret = %d, pkt_len = %d.\n",
    							  __func__, ret, skb->len);
    		}
#endif

#ifdef TXRX_DATA_LOCK
    		mutex_unlock(&priv->txrx_mutex);
#endif
    		
    		priv->cmd_cnt++;
    		hal_cmd_sent++;

    		dev_kfree_skb_any(skb);
    	}
#ifdef TX_USE_THREAD        
    }

	kthread_complete_and_exit(&tsk->completed, 0);
	RPU_INFO_HAL("%s exit\n", __func__);    
#endif
}

#if ENABLE_RX_WORKQ
static void hal_rx_queue_work(struct hal_priv  *priv)
{
	/*if(1) {
		ieee80211_queue_work(hw, &priv->rx_work);
	} else */{
		queue_work(priv->rx_wkq, &priv->rx_work);
	}
}
#endif

static void hal_tx_queue_work(struct hal_priv *priv)
{
#ifdef TX_USE_THREAD
	tsk_ctl_t *tsk = &priv->thr_tx_ctl;
	if (tsk->thr_pid >= 0 && !priv->hal_disabled) {
		up(&tsk->sema);
	}
#else
	/*
	if(1) {
		ieee80211_queue_work(hw, &priv->tx_work);
	} else 
	*/
	queue_work(priv->tx_wkq, &priv->tx_work);
#endif
}

static void hal_rx_thread_trigger(struct hal_priv  *priv)
{
	tsk_ctl_t *tsk = &priv->thr_rx_ctl;
	if (tsk->thr_pid >= 0 && !priv->hal_disabled) {
		up(&tsk->sema);
		hal_event_interrupts++;
	}
}

static void _hal_send(struct hal_priv  *priv,
			  struct sk_buff   *skb)
{
	skb_queue_tail(&priv->txq, skb);
	//tasklet_schedule(&priv->tx_tasklet);
	hal_tx_queue_work(priv);
}

#if 0
static void _hal_send_head(struct hal_priv  *priv,
			  struct sk_buff   *skb)
{
	printk("%s: qhead.\n", __func__);
	skb_queue_head(&priv->txq, skb);
	//tasklet_schedule(&priv->tx_tasklet);
	hal_tx_queue_work(priv);
}
#endif

static void hal_send(void *msg,
		     void *payload,
		     unsigned int descriptor_id)
{

	_hal_send(hpriv, msg);

}

#if ENABLE_RX_WORKQ
static void hal_recv(struct hal_priv *priv, struct sk_buff *skb)
{
#define MAX_RX_QUEUE	8192
	if (skb_queue_len(&priv->rxq) > MAX_RX_QUEUE) {
		if (net_ratelimit()) {
			struct host_rpu_msg_hdr *hdr = (struct host_rpu_msg_hdr *)skb->data;

			RPU_ERROR_HAL("%s: rx queue large than %d, drop it(%p)(%d)!\n",
					__func__, MAX_RX_QUEUE, skb, hdr->id);
		}
		dev_kfree_skb_any(skb);
		return;
	}
	RPU_DEBUG_HAL("%s: rx enqueue %p\n", __func__, skb);
	skb_queue_tail(&priv->rxq, skb);
	if (priv->max_rxq_len < skb_queue_len(&hpriv->rxq))
		priv->max_rxq_len = skb_queue_len(&hpriv->rxq);
	hal_rx_queue_work(priv);
}
#endif

#if 0
static void recv_tasklet_fn(unsigned long data)
{
	struct hal_priv *priv = (struct hal_priv *)data;
	struct sk_buff *skb;

	while ((skb = skb_dequeue(&priv->refillq))) {
		/* As we refilled the buffers, now pass them UP */
		priv->rcv_handler(skb);
	}
}
#endif

static void rx_counts_one_statistics(unsigned int count)
{
	// statistics how many rx data received in one interrupts
	if (count <= 2*1024)
		hal_event_rx_counts_one_interrupts[0]++;
	else if (count <= 4*1024)
		hal_event_rx_counts_one_interrupts[1]++;
	else if (count <= 6*1024)
		hal_event_rx_counts_one_interrupts[2]++;
	else if (count <= 8*1024)
		hal_event_rx_counts_one_interrupts[3]++;
	else if (count <= 10*1024)
		hal_event_rx_counts_one_interrupts[4]++;
	else if (count <= 12*1024)
		hal_event_rx_counts_one_interrupts[5]++;
	else if (count <= 14*1024)
		hal_event_rx_counts_one_interrupts[6]++;
	else
		hal_event_rx_counts_one_interrupts[7]++;
}

void rx_counts_one_packet(unsigned int count)
{
	// statistics how many rx data received in one packet
	if (count <= 2*1024)
		hal_event_rx_counts_one_packet[0]++;
	else if (count <= 4*1024)
		hal_event_rx_counts_one_packet[1]++;
	else if (count <= 6*1024)
		hal_event_rx_counts_one_packet[2]++;
	else if (count <= 8*1024)
		hal_event_rx_counts_one_packet[3]++;
	else if (count <= 10*1024)
		hal_event_rx_counts_one_packet[4]++;
	else if (count <= 12*1024)
		hal_event_rx_counts_one_packet[5]++;
	else if (count <= 14*1024)
		hal_event_rx_counts_one_packet[6]++;
	else
		hal_event_rx_counts_one_packet[7]++;
}

//static void rx_tasklet_fn(unsigned long data)
#if ENABLE_RX_WORKQ
static void rx_work_fn(struct work_struct *work)
{
	struct hal_priv *priv = container_of(work, struct hal_priv, rx_work);
	struct sk_buff *skb;
	//struct img_priv *imgpriv = wifi ? wifi->hw->priv:NULL;

	while (1) {
		if (skb_peek(&priv->rxq) == NULL)
			break;

		skb = skb_dequeue(&priv->rxq);
		if (skb == NULL)
			break;

		RPU_DEBUG_HAL("%s: rx dequeue %p\n", __func__, skb);

		priv->rcv_handler(skb);
	}
}
#endif

static int rx_thread(void *data)
{
	tsk_ctl_t *tsk = (tsk_ctl_t *)data;
	struct hal_priv *priv = (struct hal_priv *)tsk->parent;
	unsigned char *nbuff;
	struct sk_buff *rx_skb;
	struct host_rpu_msg_hdr *hdr;	
	int data_length = 0;
	unsigned int max_data_size = MAX_DATA_SIZE_2K;
	unsigned int payload_length, length;
	unsigned int event;
	unsigned int rx_counts_one = 0;
	static struct sched_param param = { .sched_priority = 1 };
#ifdef DUMP_MORE_DEBUG_INFO
	char evt_str[64];
#endif

	RPU_DEBUG_HAL("%s in\n", __func__);

	memset(hal_event_rx_counts_one_interrupts, 0, 8*sizeof(unsigned int));
	memset(hal_event_rx_counts_one_packet, 0, 8*sizeof(unsigned int));

	sched_set_fifo_low(current);
	//complete(&tsk->completed);
	while (1) {
		if (priv->io_info->rx_serias_count == 0 &&
			priv->io_info->rx_next_len == 0) {
			if(rx_counts_one > 0) {
				rx_counts_one_statistics(rx_counts_one);
				rx_counts_one = 0;
			}
			if (down_interruptible(&tsk->sema) != 0) {
				break;
			}
		}

		if (tsk->terminated) {
			break;
		}
		
		rx_cnt++;
		RPU_DEBUG_HAL("%s:rx_cnt=%ld cmd_cnt=0x%X event_cnt=0x%X\n",
			 hal_name, rx_cnt, priv->cmd_cnt, priv->event_cnt);

#ifdef TXRX_DATA_LOCK
//		mutex_lock(&priv->txrx_mutex);
#endif
		data_length = rk915_serias_read(priv, 0, priv->io_info->rx_serias_buf,
									  0, MAX_RX_SERIAS_BYTES);
#ifdef TXRX_DATA_LOCK
//		mutex_unlock(&priv->txrx_mutex);
#endif
		if (data_length <= 0) {
			if (net_ratelimit())
				RPU_ERROR_HAL("%s: error datalen: %x.\n", __func__, data_length);
			priv->io_info->rx_serias_count = 0;
			priv->io_info->rx_next_len = 0;
			continue;
		}
		rx_counts_one += data_length;

		rx_skb = alloc_skb(data_length, GFP_ATOMIC);
		if (rx_skb) {
			memcpy(skb_put(rx_skb, data_length),
			       priv->io_info->rx_serias_buf_curr, data_length);
		} else {
			alloc_skb_failures++;
			RPU_ERROR_HAL("%s: alloc_skb %d failed\n", __func__, data_length);
			continue;
		}
		
		nbuff = rx_skb->data;
		hdr = (struct host_rpu_msg_hdr *)nbuff;
		event = hdr->id & 0xffff;
		if (event == RPU_EVENT_RX) {
				hal_event_rx_recv++;
				/* 802.11hdr + payload Len*/
				payload_length = hdr->payload_length;
				length = hdr->length;
				/* Control Info Len*/
				data_length = payload_length + length;

				//RPU_INFO_HAL("receive %d (%d)\n", event, data_length);
				/* Complete data length to be copied */
				RPU_DEBUG_HAL("%s: Payload Len =%d(0x%x), \n",
					   hal_name,
					   payload_length,
					   payload_length);

				RPU_DEBUG_HAL("Len=%d(0x%x), \n",
					   length,
					   length);

				RPU_DEBUG_HAL("Data Len = %d(0x%x)\n",
					   data_length,
					   data_length);
				if (data_length > max_data_size) {
					RPU_ERROR_HAL("Max length exceeded:\n");
					RPU_ERROR_HAL(" payload_len: %d len:%d\n",
						payload_length,
						length);
					continue;				
				}
		} else	{
			//RPU_INFO_HAL("receive %d\n", event);
			/* MSG from LMAC, non-data*/
			if (event == RPU_EVENT_TX_DONE) {
				hal_event_tx_done++;
			} else if (event == RPU_EVENT_COMMAND_PROC_DONE) {
				hal_event_cmd_proc_done++;
			}
			hal_event_recv++;
		}

		if (hpriv->fw_error_processing) {
			RPU_DEBUG_ROCOVERY("event %d\n", event);
			hpriv->fw_error_cmd_done = 1;
			if (event == RPU_EVENT_RESET_COMPLETE &&
				hdr->length == sizeof(struct host_event_reset_complete)) {
				g_lpw_recovery_param.status = RECOVERY_STATUS_RESET_COMP;
			}
			dev_kfree_skb_any(rx_skb);
			continue;
		}

		/*
		 * we should notify ieee80211_connection_loss after device resume finished
		 * else mac80211 will discard it
		 */
		if (event == RPU_EVENT_DISCONNECTED) {
			wait_for_pm_resume_done(NULL);
		}

#ifdef DUMP_MORE_DEBUG_INFO
		convert_event_to_str(event, evt_str);
		RPU_INFO_HAL("receive %s(%d)\n", evt_str, event);
#endif

#if ENABLE_RX_WORKQ
		hal_recv(priv, rx_skb);
#else
		priv->rcv_handler(rx_skb);
#endif
	}
	kthread_complete_and_exit(&tsk->completed, 0);
	RPU_INFO_HAL("%s exit\n", __func__);
}


static void hal_register_callback(msg_handler handler)
{
	hpriv->rcv_handler = handler;
}


int hal_irq_handler(struct hal_priv *p)
{
	struct hal_priv *priv = p;

#ifdef CONFIG_PM
	rx_interrupt_status = 1;
#endif

	hal_rx_thread_trigger(priv);

	priv->event_cnt++;

	return 0;
}


static void hal_enable_int(void)
{
	rk915_irq_enable(1);
}


static void hal_disable_int(void)
{
	rk915_irq_enable(0);
}

/*
int chg_irq_register(int val)
{
	RPU_DEBUG_HAL("%s: change irq regist state %s.\n",
		 hal_name, ((val == 1) ? "ON" : "OFF"));

	if (val == 0) {
		// Unregister irq handler 
		free_irq(hpriv->irq, hpriv);

	} else if (val == 1) {
		// Register irq handler 
		if (request_irq(hpriv->irq,
				hal_irq_handler,
				hpriv->irq_flags,
				"wlan",
				hpriv) != 0) {
			return -1;
		}
	}

	return 0;
}
*/


static ssize_t proc_write_hal_stats(struct file *file,
		const char __user    *buffer,
		size_t		     count,
		loff_t               *ppos)
{
	char buf[50];

	if (count >= sizeof(buf))
		count = sizeof(buf)-1;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';

	return count;
}

static int proc_read_hal_stats(struct seq_file *m, void *v)
{
	int i=0;

	seq_printf(m, "Alloc SKB Failures: %d\n",
		   alloc_skb_failures);


	seq_printf(m, "hal_cmd_sent_cnt: %d\n",
		   hal_cmd_sent - hal_cmd_tx_send);
	seq_printf(m, "hal_event_cmd_proc_done_cnt: %d\n",
		   hal_event_cmd_proc_done);
	seq_printf(m, "hal_cmd_tx_sent_cnt: %d\n",
		   hal_cmd_tx_send);
	seq_printf(m, "hal_event_tx_done_cnt: %d\n",
		   hal_event_tx_done);
	seq_printf(m, "hal_event_recv_cnt: %d\n",
		   hal_event_recv);
	seq_printf(m, "hal_event_rx_recv_cnt: %d\n",
		   hal_event_rx_recv);

	seq_printf(m, "hal_event_interrupts: %d\n",
		   hal_event_interrupts);

	for(i=0; i<8; i++) {
		seq_printf(m, "rx_counts_one_interrupts_%dK: %d\n",
			   2*(i+1), hal_event_rx_counts_one_interrupts[i]);
	}

	for(i=0; i<8; i++) {
		seq_printf(m, "rx_count_one_packet_%dK: %d\n",
			   2*(i+1), hal_event_rx_counts_one_packet[i]);
	}

	return 0;
}


static int proc_open_hal_stats(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read_hal_stats, NULL);
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))  
static const struct proc_ops params_fops_hal_stats = {
    .proc_open = proc_open_hal_stats,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_write = proc_write_hal_stats,
    .proc_release = single_release
};
#else
static const struct file_operations params_fops_hal_stats = {
	.open = proc_open_hal_stats,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = proc_write_hal_stats,
	.release = single_release
};
#endif

static int hal_proc_init(struct proc_dir_entry *hal_proc_dir_entry)
{
	struct proc_dir_entry *entry;
	int err = 0;

	entry = proc_create("hal_stats",
			    0444,
			    hal_proc_dir_entry,
			    &params_fops_hal_stats);

	if (!entry) {
		RPU_ERROR_HAL("Failed to create HAL proc entry\n");
		err = -ENOMEM;
	}

	return err;
}




int hal_start(void)
{
	hpriv->hal_disabled = 0;

	init_fw_recovery_cmd();

	/* Enable host_int and rpu_int */
	hal_enable_int();

	return 0;
}


int hal_stop(void)
{
	hpriv->hal_disabled = 1;

	/* Disable host_int and rpu_irq */
	hal_disable_int();
	return 0;
}

static int rk915_pm_notifier(struct notifier_block *nb, unsigned long action,
                        void *data)
{
	switch (action) {
		//case PM_HIBERNATION_PREPARE:
		case PM_SUSPEND_PREPARE:
			break;

		//case PM_POST_HIBERNATION:
		case PM_POST_SUSPEND:
			hpriv->during_pm_resume = 0;
			break;

		case PM_POST_RESTORE:
		case PM_RESTORE_PREPARE:
		default:
		break;
	}

	return NOTIFY_DONE;
}

/* Unmap and release all resoruces*/
static int cleanup_all_resources(void)
{
	unregister_pm_notifier(&hpriv->pm_notifier);
	unregister_syscore(&host_syscore);

	/* Free private structure
	kfree(hpriv);
	hpriv = NULL;
	*/

	return 0;
}
static int hal_deinit(void *dev)
{
	struct sk_buff *skb;

	if (!hpriv->hal_init) {
		proc_exit();
		return 0;
	}

	(void)(dev);

	_rpu_umac_if_exit();

#ifndef TX_USE_THREAD
	cancel_work_sync(&hpriv->tx_work);
	if (hpriv->tx_wkq != NULL) {
		destroy_workqueue(hpriv->tx_wkq);
		hpriv->tx_wkq = NULL;
	}
#endif    

#if ENABLE_RX_WORKQ
	cancel_work_sync(&hpriv->rx_work);
	if (hpriv->rx_wkq != NULL) {
		destroy_workqueue(hpriv->rx_wkq);
		hpriv->rx_wkq = NULL;
	}
#endif

	cancel_work_sync(&hpriv->fw_err_work);

	wake_lock_destroy(&hpriv->fw_err_lock);

#ifdef TX_USE_THREAD
	PROC_STOP(&hpriv->thr_tx_ctl);
#endif
	PROC_STOP(&hpriv->thr_rx_ctl);

	while ((skb = skb_dequeue(&hpriv->rxq)))
		dev_kfree_skb_any(skb);

	while ((skb = skb_dequeue(&hpriv->txq)))
		dev_kfree_skb_any(skb);

	cleanup_all_resources();

	proc_exit();

	hpriv->hal_init = 0;
	return 0;
}


static int hal_init(void *dev)
{
	struct proc_dir_entry *main_dir_entry;
	int err = 0;
	(void) (dev);

#ifdef TXRX_DATA_LOCK
	mutex_init(&hpriv->txrx_mutex);
#endif

	hpriv->hal_disabled = 1;

#ifdef TX_USE_THREAD
	PROC_START(tx_thread, hpriv, &hpriv->thr_tx_ctl, 0, "rk915_tx_thr");
#else
	hpriv->tx_wkq = create_singlethread_workqueue("rk915_tx_wkq");
	if (hpriv->tx_wkq == NULL) {
		RPU_ERROR_HAL("%s: create tx wkq failed\n", hal_name);
		err = -ENOMEM;
		goto error;
	}
	INIT_WORK(&hpriv->tx_work, tx_work_fn);
#endif    

	PROC_START(rx_thread, hpriv, &hpriv->thr_rx_ctl, 0, "rk915_rx_thr");

#if ENABLE_RX_WORKQ
	hpriv->rx_wkq = create_singlethread_workqueue("rk915_rx_wkq");
	if (hpriv->rx_wkq == NULL) {
		RPU_ERROR_HAL("%s: create rx wkq failed\n", hal_name);
		err = -ENOMEM;
		goto error1;
	}
	INIT_WORK(&hpriv->rx_work, rx_work_fn);
#endif
	skb_queue_head_init(&hpriv->rxq);
	skb_queue_head_init(&hpriv->txq);

	INIT_WORK(&hpriv->fw_err_work, fw_err_work_fn);

	wake_lock_init(&hpriv->fw_err_lock, WAKE_LOCK_SUSPEND, "rk915_lock");

	hpriv->pm_notifier.notifier_call = rk915_pm_notifier;
	register_pm_notifier(&hpriv->pm_notifier);

	register_syscore(&host_syscore);
	if (_rpu_umac_if_init(&main_dir_entry) < 0) {
		RPU_ERROR_HAL("%s: wlan_init failed\n", hal_name);
		err = -ENOMEM;
		goto error2;
	}

	err = hal_proc_init(main_dir_entry);
	if (err) {
		goto umac_if_deinit;
	}

	hpriv->cmd_cnt = COMMAND_START_MAGIC;
	hpriv->event_cnt = 0;
	hpriv->hal_init = 1;

	return err;
umac_if_deinit:
	_rpu_umac_if_exit();
error2:
	if (hpriv->rx_wkq != NULL)
		destroy_workqueue(hpriv->rx_wkq);
	unregister_syscore(&host_syscore);
	unregister_pm_notifier(&hpriv->pm_notifier);
error1:
#ifdef TX_USE_THREAD
    PROC_STOP(&hpriv->thr_tx_ctl);
#else
	if (hpriv->tx_wkq != NULL)
		destroy_workqueue(hpriv->tx_wkq);
#endif    
	PROC_STOP(&hpriv->thr_rx_ctl);
#ifndef TX_USE_THREAD
error:
#endif
	return err;
}

static void hal_deinit_bufs(void)
{
	if (hpriv->tx_buf_info)
		kfree(hpriv->tx_buf_info);
	hpriv->tx_buf_info = NULL;
	if (hpriv->rx_tmp_buf)
		kfree(hpriv->rx_tmp_buf);
	hpriv->rx_tmp_buf = NULL;
}


static int hal_init_bufs(unsigned int tx_bufs,
			 unsigned int rx_bufs_2k,
			 unsigned int rx_bufs_12k,
			 unsigned int tx_max_data_size)
{
	hpriv->rx_tmp_buf = kzalloc(MAX_DATA_SIZE_2K, GFP_KERNEL);
	if (!hpriv->rx_tmp_buf) {
		RPU_ERROR_HAL("%s out of memory\n", hal_name);
		goto err;
	}
	
	return 0;
err:

	hal_deinit_bufs();

	return -1;
}


int hal_map_tx_buf(int pkt_desc,
		   int frame_id,
		   unsigned char *data,
		   int len, dma_addr_t *phy_addr)
{
	*phy_addr = (dma_addr_t)data;
	return 0;
}


int hal_unmap_tx_buf(int pkt_desc, int frame_id)
{

	return 0;
}

#if 0
static int is_mem_bounce(void *virt_addr, int len)
{
	phys_addr_t phy_addr_start = 0;
	phys_addr_t phy_addr = 0;

	phy_addr = VIRT_TO_PHYS(virt_addr);
	phy_addr_start = VIRT_TO_PHYS(hpriv->base_addr_rpu_host_ram);

	if (phy_addr >= phy_addr_start &&
	    (phy_addr + len) < (phy_addr_start +
				HAL_HOST_RPU_RAM_LEN))
		return 1;

	pr_info("%s: Warning: Address is out of Bounce memory region\n",
		hal_name);

	return 0;
}
#endif

void wow_enable_irq_wake(void)
{
//	enable_irq_wake(hpriv->irq);
}

void wow_disable_irq_wake(void)
{
//	disable_irq_wake(hpriv->irq);
}

#if 0
static enum rpu_mem_region fwldr_chk_region(unsigned int src_addr, int len)
{
	unsigned int dst_addr = src_addr + len;

	if (((src_addr >= 0x03000000) && (src_addr <= 0x04FFFFFF))  ||
	    ((src_addr >= 0x02009000) && (src_addr <= 0x0203BFFF))  ||
	    ((src_addr >= 0x80000000) && (src_addr <= 0x87FFFFFF))) {
		if (len != 0) {
			if (((dst_addr >= 0x03000000) &&
			     (dst_addr <= 0x04FFFFFF)) ||
			    ((dst_addr >= 0x02009000) &&
			     (dst_addr <= 0x0203BFFF)) ||
			    ((dst_addr >= 0x80000000) &&
			     (dst_addr <= 0x87FFFFFF)))
				return RPU_MEM_CORE;
			else
				return RPU_MEM_ERR;
		}

		return RPU_MEM_CORE;
	} else if ((src_addr & 0xFF000000) == 0xB0000000) {
		return RPU_MEM_ERR;
	} else {
		return RPU_MEM_DIRECT;
	}
}


static void dir_mem_read(unsigned int addr,
			 unsigned int *data,
			 unsigned int len)
{
	int i = 0;
	unsigned long offset = (unsigned long)addr & RPU_OFFSET_MASK;
	unsigned long base = ((unsigned long)addr & RPU_BASE_MASK) >> 24;

	for (i = 0; i <= len / 4; i++) {
		hal_rpu_read(hpriv, base, offset, data+i);
		offset += 4;
	}
}

/* 32 bit write to RPU memory location 'addr'
 * 'addr' is always a 4 byte aligned address
 */
static void dir_mem_write(unsigned int addr,
			 unsigned int data)
{
	unsigned long offset = (unsigned long)addr & RPU_OFFSET_MASK;
	unsigned long base = ((unsigned long)addr & RPU_BASE_MASK) >> 24;

	/* Boot Exception vector 0xBFC00000 maps to -> 0xA4000050 */
        if (base == UCCP_BEV) {

		offset = (unsigned long)addr & 0xfffff;

                addr = MIPS_MCU_BOOT_EXCP_INSTR_0;

		offset += ((unsigned long)addr & RPU_OFFSET_MASK);
		base = ((unsigned long)addr & RPU_BASE_MASK) >> 24;
	}

	hal_rpu_write(hpriv, base, offset, data);
}


static void core_mem_set(unsigned int addr,
			 unsigned int data,
			 unsigned int len)
{
	addr = (addr & RPU_OFFSET_MASK)/4;

	dir_mem_write(MIPS_MCU_SYS_CORE_MEM_CTRL, addr);

	dir_mem_write(MIPS_MCU_SYS_CORE_MEM_WDATA, data);
}

static void core_mem_read(unsigned int addr,
			  unsigned int *data,
			  unsigned int len)
{
	unsigned int i = 0;
	unsigned int val = 0;

	/* Poll MSLVCTRL1 */
	do {
		dir_mem_read(MSLVCTRL1, &val, 1);
	} while (!MSLAVE_READY(val));

	dir_mem_write(MSLVCTRL0,
			((addr & SLAVE_ADDR_MODE_MASK) | SLAVE_BLOCK_READ));

	for (i = 0; i < len-1; i++) {
		do {
			dir_mem_read(MSLVCTRL1, &val, 1);
		} while (!MSLAVE_READY(val));

		dir_mem_read(MSLVDATAT, &data[i], 1);
	}

	/* Read the last word */
	do {
		dir_mem_read(MSLVCTRL1, &val, 1);
	} while (!MSLAVE_READY(val));

	dir_mem_read(MSLVDATAX, &data[len-1], 1);


}

static unsigned int fwldr_config_read(unsigned int dst_addr)
{
	int mem_region = 0;
	int val = 0;

	if (0 != (dst_addr % 4))
		RPU_ERROR_HAL("Destination Address is not 4 - byte aligned");

	mem_region = fwldr_chk_region(dst_addr, 0);

	switch (mem_region) {
	case RPU_MEM_CORE:
		core_mem_read(dst_addr, &val, 1);
		return val;

	case RPU_MEM_DIRECT:
		dir_mem_read(dst_addr, &val, 1);
		return val;

	default:
		RPU_ERROR_HAL("Region unknown. Skipped reading\n");
		return 0;
	}

	return 0;
}


static void fwldr_config_write(unsigned int dst_addr,
			unsigned int val)
{
	int mem_region = 0;

	if (0 != (dst_addr % 4))
		RPU_ERROR_HAL("Destination Address is not 4 - byte aligned");

	mem_region = fwldr_chk_region(dst_addr, 0);


	switch (mem_region) {
	case RPU_MEM_CORE:
		core_mem_set(dst_addr, val, 1);
		break;

	case RPU_MEM_DIRECT:
		dir_mem_write(dst_addr, val);
		break;

	default:
		RPU_ERROR_HAL("Region unknown. Skipped writing\n");
		break;
	}

}*/
#endif


int rpu_set_mem(unsigned int *dst,
		     unsigned int val,
		     unsigned int len)
{
	/*unsigned int index;

	for (index = 0; index < len/4; index++) {
		fwldr_config_write((unsigned int)(dst + index), val);
	}*/

	return 0;
}

int rpu_read_mem(unsigned int *src,
		     unsigned int *dst,
		     unsigned int len)
{
	/*unsigned int index;

	for (index = 0; index < len/4; index++) {
		*(src + index) = fwldr_config_read((unsigned int)(unsigned long)(dst + index));
	}*/

	return 0;
}


int rpu_write_mem(unsigned int *src,
		     unsigned int *dst,
		     unsigned int len)
{
	/*unsigned int index;

	for (index = 0; index < len/4; index++) {
		fwldr_config_write((unsigned int)(dst + index), *(src + index));
	}*/

	return 0;
}

void set_mem_region(unsigned int addr)
{
	soc_ops.set_mem_region(addr);
}

void request_mem_regions(unsigned char **gram_addr,
			    unsigned char **sysbus_addr,
			    unsigned char **gram_b4_addr)
{

        *gram_addr = (unsigned char *)hpriv->gram_base_addr;
        *sysbus_addr = (unsigned char *)hpriv->rpu_sysbus_base_addr;
        *gram_b4_addr = (unsigned char *)hpriv->gram_b4_addr;
}

struct hal_ops_tag hal_ops = {
	.init = hal_init,
	.deinit	= hal_deinit,
	.start = hal_start,
	.stop = hal_stop,
	.register_callback = hal_register_callback,
	.send = hal_send,
	.init_bufs = hal_init_bufs,
	.deinit_bufs = hal_deinit_bufs,
	.map_tx_buf = hal_map_tx_buf,
	.unmap_tx_buf = hal_unmap_tx_buf,
	.reset_hal_params	= hal_reset_hal_params,
	.get_dev = hal_get_dev,
#ifdef CONFIG_PM
	.enable_irq_wake = wow_enable_irq_wake,
	.disable_irq_wake = wow_disable_irq_wake,
#endif
#ifdef RPU_SLEEP_ENABLE
	.trigger_timed_sleep = trigger_timed_sleep,
	.trigger_wakeup = trigger_wakeup,
	.rpu_sleep_status = get_rpu_sleep_status,
#endif
	.rpu_set_mem = rpu_set_mem,
	.rpu_read_mem = rpu_read_mem,
	.rpu_write_mem = rpu_write_mem,
	.request_mem_regions = request_mem_regions,
	.set_mem_region = set_mem_region,
};

