/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <linux/netdevice.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/of_platform.h>
#include <linux/of.h>

#include "core.h"
#include "utils.h"
#include "if_io.h"
#include "hal_io.h"
#include "hal_common.h"

//#define TX_CMD_SYNC_WITH_OTHER_CMD

static int rpu_send_cmd_without_delay(unsigned char *buf,
				unsigned int len,
				unsigned char id);
static inline void rpu_process_pending_cmd(struct img_priv *priv, int lock);

unsigned char wildcard_ssid[7] = "DIRECT-";
#ifdef CONFIG_PM
unsigned char rx_interrupt_status;
#endif

struct cmd_send_recv_cnt cmd_info;

struct rpu_if_data {
	char *name;
	void *context;
};

static struct rpu_if_data __rcu *rpu_if;

#ifdef DUMP_MORE_DEBUG_INFO
static char cmd_str_tbl[RPU_MAX_CMD_NUMBER][32] = {
	"RPU_CMD_RESET",
	"RPU_CMD_SCAN",
	"RPU_CMD_SCAN_ABORT",
	"RPU_CMD_UNUSED1",
	"RPU_CMD_SETKEY",
	"RPU_CMD_UNUSED2",
	"RPU_CMD_UNUSED3",
	"RPU_CMD_TX",
	"RPU_CMD_UNUSED4",
	"RPU_CMD_UNUSED5",
	"RPU_CMD_TX_POWER",
	"RPU_CMD_UNUSED6",
	"RPU_CMD_UNUSED7",
	"RPU_CMD_PS",
	"RPU_CMD_PS_ECON_CFG",
	"RPU_CMD_VIF_CTRL",
	"RPU_CMD_UNUSED8",
	"RPU_CMD_UNUSED9",
	"RPU_CMD_BA_SESSION_INFO",
	"RPU_CMD_MCST_ADDR_CFG",
	"RPU_CMD_MCST_FLTR_CTRL",
	"RPU_CMD_UNUSED10",
	"RPU_CMD_ROC_CTRL",
	"RPU_CMD_CHANNEL",
	"RPU_CMD_VIF_CFG",
	"RPU_CMD_UNUSED11",
	"RPU_CMD_TXQ_PARAMS",
	"RPU_CMD_MIB_STATS",
	"RPU_CMD_PHY_STATS",
	"RPU_CMD_UNUSED12",
	"RPU_CMD_UNUSED13",
	"RPU_CMD_UNUSED14",
	"RPU_CMD_UNUSED15",
	"RPU_CMD_UNUSED16",
	"RPU_CMD_UNUSED17",
	"RPU_CMD_UNUSED18",
	"RPU_CMD_CLEAR_STATS",
	"RPU_CMD_CONT_TX",
	"RPU_CMD_RX_CTRL",
	"RPU_CMD_CFG_PWRMGMT",
	"RPU_CMD_UPD_PHY_THRESH",
	"RPU_CMD_TXRX_TEST",
	"RPU_CMD_FW_PRIV_CMD",
	"RPU_CMD_SL_WP_CTRL",
	"RPU_CMD_READ_CSR",
}; 

static char event_str_tbl[RPU_MAX_EVENT_NUMBER][32] = {
	"RPU_EVENT_RX",
	"RPU_EVENT_TX_DONE",
	"RPU_EVENT_DISCONNECTED",
	"RPU_EVENT_UNUSED1",
	"RPU_EVENT_UNUSED2",
	"RPU_EVENT_SCAN_COMPLETE",
	"RPU_EVENT_SCAN_ABORT_COMPLETE",
	"RPU_EVENT_UNUSED3",
	"RPU_EVENT_RESET_COMPLETE",
	"RPU_EVENT_UNUSED4",
	"RPU_EVENT_UNUSED5",
	"RPU_EVENT_UNUSED6",
	"RPU_EVENT_MIB_STAT",
	"RPU_EVENT_PHY_STAT",
	"RPU_EVENT_NW_FOUND",
	"RPU_EVENT_NOA",
	"RPU_EVENT_CTRL_POOL_ACK",
	"RPU_EVENT_COMMAND_PROC_DONE",
	"RPU_EVENT_CH_PROG_DONE",
	"RPU_EVENT_PS_ECON_CFG_DONE",
	"RPU_EVENT_PS_ECON_WAKE",
	"RPU_EVENT_MAC_STATS",
	"RPU_EVENT_UNUSED7",
	"RPU_EVENT_UNUSED8",
#ifndef RK915	
	"RPU_EVENT_MSRMNT_COMPLETE",
#endif	
	"RPU_EVENT_ROC_STATUS",
	"RPU_EVENT_FW_ERROR",
	"RPU_EVENT_BLOCK_ALL",
	"RPU_EVENT_UNBLOCK_ALL",
	"RPU_EVENT_TXRX_TEST",
	"RPU_EVENT_FW_PRIV_CMD_DONE",
	"RPU_EVENT_AIRKISS_STATUS",
	"RPU_EVENT_READ_CSR_CMP",	
};
void convert_cmd_to_str(int id, char *str)
{
	if (id < 0 || id >= RPU_MAX_CMD_NUMBER) {
		strcpy(str, "ILLEGAL_CMD_ID");
	} else {
		strcpy(str, cmd_str_tbl[id]);
	}
}

void convert_event_to_str(int id, char *str)
{
	if (id < 0 || id >= RPU_MAX_EVENT_NUMBER) {
		strcpy(str, "ILLEGAL_EVENT_ID");
	} else {
		strcpy(str, event_str_tbl[id]);
	}
}

#define VIF_CONF_CHANGED_INFO_NUM 16
static char vif_conf_changed_info_tbl[VIF_CONF_CHANGED_INFO_NUM][24] = {
	"BASICRATES_CHANGED",
	"SHORTSLOT_CHANGED",
	"POWERSAVE_CHANGED",
	"UAPSDTYPE_CHANGED",
	"ATIMWINDOW_CHANGED ",
	"AID_CHANGED",
	"CAPABILITY_CHANGED",
	"SHORTRETRY_CHANGED",
	"LONGRETRY_CHANGED",
	"BSSID_CHANGED",
	"RCV_BCN_MODE_CHANGED",
	"BCN_INT_CHANGED",
	"DTIM_PERIOD_CHANGED",
	"SMPS_CHANGED",
	"CONNECT_STATE_CHANGED",
	"OP_CHAN_CHANGED"
};

static int dump_vif_cfg_changed_info(unsigned char *buf)
{
	struct cmd_vif_cfg *cfg = (struct cmd_vif_cfg *)buf;
	unsigned int i;
	char prt_str[384];

	memset(prt_str, 0, sizeof(prt_str));
	for (i = 0; i < VIF_CONF_CHANGED_INFO_NUM; i++) {
		if (cfg->changed_bitmap & (1<<i))
			sprintf(prt_str + strlen(prt_str), "%s|", vif_conf_changed_info_tbl[i]);
	}
	
	RPU_DEBUG_IF("%s: changed_bitmap = %08x (%s)\n", __func__, cfg->changed_bitmap, prt_str);

	if (cfg->changed_bitmap & BSSID_CHANGED)
		return 0;
	else
		return 1;
}
#else

void convert_cmd_to_str(int id, char *str)
{
	strcpy(str, "");
}

void convert_event_to_str(int id, char *str)
{
	strcpy(str, "");
}

int dump_vif_cfg_changed_info(unsigned char *buf)
{
	return 0;
}

#endif

#ifdef REGENERATE_CMD_TEST
static void regenerate_reset_cmd_data(unsigned char* buf, int type) {
      unsigned int *init_add = (unsigned int *)buf;

      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0xFFFF    ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=type		; init_add++;
      *(init_add)=0xFFFFFFAC; init_add++;
      *(init_add)=0x1       ; init_add++;
      *(init_add)=0x1E      ; init_add++;
      *(init_add)=0x26240000; init_add++;
      *(init_add)=0x2E2C2A29; init_add++;
      *(init_add)=0x3F393732; init_add++;
      *(init_add)=0x57524A45; init_add++;
      *(init_add)=0x6660    ; init_add++;
      *(init_add)=0x2C2B0000; init_add++;
      *(init_add)=0x3A373330; init_add++;
      *(init_add)=0x4D47443D; init_add++;
      *(init_add)=0x615A5751; init_add++;
      *(init_add)=0x6F6B65  ; init_add++;
      *(init_add)=0x2B000000; init_add++;
      *(init_add)=0x3733302C; init_add++;
      *(init_add)=0x47443D3A; init_add++;
      *(init_add)=0x5A57514D; init_add++;
      *(init_add)=0x6F6B6561; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x33302C2B; init_add++;
      *(init_add)=0x443D3A37; init_add++;
      *(init_add)=0x57514D47; init_add++;
      *(init_add)=0x6B65615A; init_add++;
      *(init_add)=0x6F      ; init_add++;
      *(init_add)=0x302C2B00; init_add++;
      *(init_add)=0x3D3A3733; init_add++;
      *(init_add)=0x514D4744; init_add++;
      *(init_add)=0x65615A57; init_add++;
      *(init_add)=0x6F6B    ; init_add++;
      *(init_add)=0x24000000; init_add++;
      *(init_add)=0x2C2A2926; init_add++;
      *(init_add)=0x3937322E; init_add++;
      *(init_add)=0x524A453F; init_add++;
      *(init_add)=0x666057  ; init_add++;
      *(init_add)=0x2B000000; init_add++;
      *(init_add)=0x3733302C; init_add++;
      *(init_add)=0x47443D3A; init_add++;
      *(init_add)=0x5A57514D; init_add++;
      *(init_add)=0x6F6B6561; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x33302C2B; init_add++;
      *(init_add)=0x443D3A37; init_add++;
      *(init_add)=0x57514D47; init_add++;
      *(init_add)=0x6B65615A; init_add++;
      *(init_add)=0x6F      ; init_add++;
      *(init_add)=0x302C2B00; init_add++;
      *(init_add)=0x3D3A3733; init_add++;
      *(init_add)=0x514D4744; init_add++;
      *(init_add)=0x65615A57; init_add++;
      *(init_add)=0x6F6B    ; init_add++;
      *(init_add)=0x2C2B0000; init_add++;
      *(init_add)=0x3A373330; init_add++;
      *(init_add)=0x4D47443D; init_add++;
      *(init_add)=0x615A5751; init_add++;
      *(init_add)=0x86F6B65 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x8080808 ; init_add++;
      *(init_add)=0x108     ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x3020100 ; init_add++;
      *(init_add)=0x7060504 ; init_add++;
      *(init_add)=0xB0A0908 ; init_add++;
      *(init_add)=0xE0D0C   ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x138800  ; init_add++;
      *(init_add)=0x12C00   ; init_add++;
      *(init_add)=0xC800    ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x100     ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x10000   ; init_add++;
      //*(init_add)=0xAAAA0000; init_add++;
}

static void regenerate_channel_cmd_data(unsigned char* buf, int channel) {
      unsigned int *init_add = (unsigned int *)buf;	

      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0xFFFF    ; init_add++;
      *(init_add)=0x20      ; init_add++;
      *(init_add)=0x17      ; init_add++;
      *(init_add)=0x21      ; init_add++;
      *(init_add)=0xB0012800; init_add++;
      *(init_add)=0x0       ; init_add++;
      /**(init_add)=channel   ; */init_add++;
      /**(init_add)=channel   ; */init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      //*(init_add)=0x0       ; init_add++;
}

static void regenerate_vif_ctrl_cmd_data(unsigned char* buf, int vif_add) {
      unsigned int *init_add = (unsigned int *)buf;	
	  unsigned char *init_add_char;
	  unsigned char *LMAC = vif_macs[0];

      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0xFFFF    ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0xF       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=IF_ADD    ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=0x0       ; init_add++;
      *(init_add)=LMAC[3]<<24 | LMAC[2]<<16 | LMAC[1]<<8 | LMAC[0]; init_add++;
	  init_add_char = (unsigned char *)init_add;
	  *(init_add_char) = LMAC[4];  init_add++;
	  *(init_add_char) = LMAC[5];  init_add++;
      //*(init_add)=0xAAAA0000 | LMAC[5]<<8 | LMAC[4]  ; init_add++;
}

static void regenerate_vif_cfg_cmd_data(unsigned char* buf) {
        unsigned char *LMAC = vif_macs[0];
		struct cmd_vif_cfg *vif_cfg = (struct cmd_vif_cfg*)buf;		

        //memset(vif_cfg, 0, sizeof(struct cmd_vif_cfg));
        vif_cfg->hdr.descriptor_id = 0xFFFF;
        vif_cfg->hdr.id = RPU_CMD_VIF_CFG;
        vif_cfg->hdr.length = sizeof(struct cmd_vif_cfg);
        vif_cfg->changed_bitmap = BSSID_CHANGED | CONNECT_STATE_CHANGED;
        vif_cfg->connect_state = 0;
        vif_cfg->basic_rate_set = 0;//0xffff;
        vif_cfg->use_short_slot = 0;//1;
        vif_cfg->bcn_mode = 0;
        vif_cfg->atim_window = 0;
        vif_cfg->aid = 0;//1;
        vif_cfg->capability = 0;
        vif_cfg->short_retry = 0;//8;
        vif_cfg->long_retry = 0;//8 ;
        vif_cfg->smps_info = 0;
        //memcpy(vif_cfg->bssid, BSSID, 6);
        vif_cfg->if_index = 0;
        //memcpy(vif_cfg->vif_addr, LMAC, 6);
}
#endif

void update_mcs_packet_stat(int mcs_rate_num,
				   int rate_flags,
				   struct img_priv *priv)
{
	if (rate_flags & ENABLE_11N_FORMAT) {
		switch (mcs_rate_num) {
		case 0:
			priv->stats->ht_tx_mcs0_packet_count++;
			break;
		case 1:
			priv->stats->ht_tx_mcs1_packet_count++;
			break;
		case 2:
			priv->stats->ht_tx_mcs2_packet_count++;
			break;
		case 3:
			priv->stats->ht_tx_mcs3_packet_count++;
			break;
		case 4:
			priv->stats->ht_tx_mcs4_packet_count++;
			break;
		case 5:
			priv->stats->ht_tx_mcs5_packet_count++;
			break;
		case 6:
			priv->stats->ht_tx_mcs6_packet_count++;
			break;
		case 7:
			priv->stats->ht_tx_mcs7_packet_count++;
			break;
		case 8:
			priv->stats->ht_tx_mcs8_packet_count++;
			break;
		case 9:
			priv->stats->ht_tx_mcs9_packet_count++;
			break;
		case 10:
			priv->stats->ht_tx_mcs10_packet_count++;
			break;
		case 11:
			priv->stats->ht_tx_mcs11_packet_count++;
			break;
		case 12:
			priv->stats->ht_tx_mcs12_packet_count++;
			break;
		case 13:
			priv->stats->ht_tx_mcs13_packet_count++;
			break;
		case 14:
			priv->stats->ht_tx_mcs14_packet_count++;
			break;
		case 15:
			priv->stats->ht_tx_mcs15_packet_count++;
			break;
		default:
			break;
		}
	}
}


static void get_rate(struct sk_buff *skb,
		     struct cmd_tx_ctrl *txcmd,
		     struct tx_pkt_info *pkt_info,
		     bool retry,
		     struct img_priv *priv)
{
	struct ieee80211_rate *rate;
	struct ieee80211_tx_info *c = IEEE80211_SKB_CB(skb);
	unsigned int index, min_rate;
	bool is_mcs = false, is_mgd = false;
	struct ieee80211_tx_rate *txrate;
	unsigned char mcs_rate_num = 0;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
	int mcs_indx;
	int mgd_rate;
	int mgd_mcast_rate;
	int prot_type;
	unsigned char nss = 1;
	bool all_rates_invalid = true;


	/* Normal Mode*/
	rate = ieee80211_get_tx_rate(priv->hw, c);
	min_rate = priv->hw->wiphy->bands[c->band]->bitrates[0].hw_value;

	if (rate == NULL) {
		RPU_DEBUG_IF("%s:%d rate is null taking defaults: min: %d\n",
			      __func__,
			      __LINE__,
			      c->control.rates[0].idx);
		txcmd->num_rates = 1;
		txcmd->rate[0] = min_rate;
		txcmd->rate_retries[0] = 5;
		txcmd->rate_protection_type[0] = USE_PROTECTION_NONE;
		txcmd->rate_preamble_type[0] = DONT_USE_SHORT_PREAMBLE;
		txcmd->num_spatial_streams[0] = 1;
		txcmd->bcc_or_ldpc = 0;
		txcmd->stbc_enabled = 0;
		txcmd->rate_flags[0] = 0;
		return;
	}

	/* Some defaults*/
	txcmd->num_rates = 0;
	txcmd->stbc_enabled = 0;

	/* BCC (or) LDPC */
	if (c->flags & IEEE80211_TX_CTL_LDPC)
		txcmd->bcc_or_ldpc = 1;
	else
		txcmd->bcc_or_ldpc = 0;

	if (ieee80211_is_data(hdr->frame_control) &&
	    c->flags & IEEE80211_TX_CTL_AMPDU) {
		txcmd->aggregate_mpdu = AMPDU_AGGR_ENABLED;
	}

	for (index = 0; index < 4; index++) {
		bool skip_rate = false;

		txrate = (&c->control.rates[index]);
		txcmd->rate_flags[index] = 0;

		if (txrate->idx < 0)
			continue;

		txcmd->num_rates++;
		txcmd->num_spatial_streams[index] = 1;

		/* No input from production_test proc, continue and use
		 * info from mac80211 RC
		 */

		if (txrate->flags & IEEE80211_TX_RC_MCS) {
			is_mcs = true;
			mcs_rate_num  = txrate->idx;
			nss = mcs_rate_num/8 + 1;
			txcmd->rate_flags[index] |= ENABLE_11N_FORMAT;
		}

		mcs_indx = priv->params->mgd_mode_tx_fixed_mcs_indx;
		mgd_rate = priv->params->mgd_mode_tx_fixed_rate;
		mgd_mcast_rate = priv->params->mgd_mode_mcast_fixed_data_rate;

		/* Rate Index:
		 * From proc:
		 *    ** Multicast data packets
		 *    ** Unicast data packets
		 * From RC in mac80211
		 * Can be MCS(HT/VHT) or Rate (11abg)
		 */
		if (ieee80211_is_data(hdr->frame_control) &&
		    is_multicast_ether_addr(hdr->addr1) &&
		    (mgd_mcast_rate != -1)) {
			/* proc: Fixed MCS/Legacy rate for Multicast packets
			 */
			is_mgd = true;
			is_mcs = (mgd_mcast_rate & 0x80) == 0x80 ? true : false;

			if (!is_mcs) {
				if (mgd_mcast_rate == 55)
					mgd_mcast_rate = 11;
				else
					mgd_mcast_rate *= 2;
			}

			txcmd->rate[index] = mgd_mcast_rate;
			txcmd->rate_flags[index] =
				priv->params->mgd_mode_mcast_fixed_rate_flags;
			txcmd->bcc_or_ldpc =
				priv->params->mgd_mode_mcast_fixed_bcc_or_ldpc;
			if (txcmd->rate_flags[index] & ENABLE_11N_FORMAT)
				nss = (mgd_mcast_rate & 0x7F)/8 + 1;
			else
				nss = priv->params->mgd_mode_mcast_fixed_nss;
			txcmd->stbc_enabled =
				priv->params->mgd_mode_mcast_fixed_stbc_enabled;
			txcmd->rate_preamble_type[index] =
				priv->params->mgd_mode_mcast_fixed_preamble;
			if (is_mcs)
				update_mcs_packet_stat(mgd_mcast_rate & 0x7F,
						       txcmd->rate_flags[index],
						       priv);
		} else if (ieee80211_is_data(hdr->frame_control) &&
			   mcs_indx != -1) {
			/* proc: Fixed MCS for unicast
			 */
			is_mgd = true;

			txcmd->rate[index] = 0x80;
			txcmd->rate[index] |= (mcs_indx);
			txcmd->rate_flags[index] =
				priv->params->prod_mode_rate_flag;
			if (txcmd->rate_flags[index] & ENABLE_11N_FORMAT)
				nss = (mcs_indx)/8 + 1;
			else
				nss = priv->params->num_spatial_streams;
			txcmd->bcc_or_ldpc =
				priv->params->prod_mode_bcc_or_ldpc;
			txcmd->stbc_enabled =
				priv->params->prod_mode_stbc_enabled;

			update_mcs_packet_stat(mcs_indx,
					       txcmd->rate_flags[index],
					       priv);
		} else if (ieee80211_is_data(hdr->frame_control) &&
			   mgd_rate != -1) {
			/* proc: Fixed Legacy Rate for unicast
			 */
			is_mgd = true;
			txcmd->rate[index] = 0x80;
			txcmd->rate[index] = 0x00;

			if (mgd_rate == 55)
				txcmd->rate[index] |= ((mgd_rate) / 5);
			else
				txcmd->rate[index] |= ((mgd_rate * 10) / 5);

			txcmd->rate_flags[index] = 0;
			nss = 1;
			txcmd->bcc_or_ldpc         = 0;
			txcmd->stbc_enabled        = 0;
			txcmd->rate_preamble_type[index] =
				priv->params->prod_mode_rate_preamble_type;
		} else if (is_mcs) {
			txcmd->rate[index] = MARK_RATE_AS_MCS_INDEX;
			txcmd->rate[index] |= mcs_rate_num;
			update_mcs_packet_stat(mcs_rate_num,
					      txcmd->rate_flags[index],
					      priv);
		} else if (!is_mcs) {
			rate = &priv->hw->wiphy->bands[
				c->band]->bitrates[
				c->control.rates[index].idx];
			txcmd->rate[index] = MARK_RATE_AS_RATE;
			txcmd->rate[index] |= rate->hw_value;
			nss = 1;
		}

		txcmd->num_spatial_streams[index] = nss;

		if (is_mgd) {
			if (priv->params->rate_protection_type)
				txcmd->rate_protection_type[index] =
					USE_PROTECTION_RTS;
			else
				txcmd->rate_protection_type[index] =
					USE_PROTECTION_NONE;
			txcmd->rate_retries[index] = 1;
			all_rates_invalid = false;
			break;
		}

		/* STBC Enabled/Disabled: valid if n_antennas > Nss */
		if (priv->params->uccp_num_spatial_streams > nss &&
		    (c->flags & IEEE80211_TX_CTL_STBC))
			txcmd->stbc_enabled = 1;

		txcmd->rate_retries[index] =
			c->control.rates[index].count;

		if (c->control.rates[index].flags &
		    IEEE80211_TX_RC_USE_SHORT_PREAMBLE)
			txcmd->rate_preamble_type[index] =
				USE_SHORT_PREAMBLE;
		else
			txcmd->rate_preamble_type[index] =
				DONT_USE_SHORT_PREAMBLE;

		prot_type = USE_PROTECTION_NONE;
		if (priv->params->rate_protection_type == 1) {
			/* Protection*/
			if (c->control.rates[index].flags &
			    IEEE80211_TX_RC_USE_CTS_PROTECT)
				prot_type = USE_PROTECTION_CTS2SELF;
			else if (c->control.rates[index].flags &
				 IEEE80211_TX_RC_USE_RTS_CTS)
				prot_type = USE_PROTECTION_RTS;
			else
				prot_type = USE_PROTECTION_NONE;

			/*RTS threshold: Check for PSDU length
			 * Need to add all HW added lenghts to skb,
			 * sw added lengths are already part of skb->len
			 * IV ==> Always SW
			 * MIC for CCMP ==> HW (MMIC for TKIP ==> SW)
			 * ICV ==> HW
			 * FCS ==> HW
			*/
			if (ieee80211_is_data(hdr->frame_control) &&
			    !is_multicast_ether_addr(hdr->addr1) &&
			    ieee80211_has_protected(hdr->frame_control)) {
				if (skb->len +
				    c->control.hw_key->icv_len +
				    priv->rts_threshold < FCS_LEN)
					prot_type = USE_PROTECTION_RTS;
			}

			if (ieee80211_is_data(hdr->frame_control) &&
			    !is_multicast_ether_addr(hdr->addr1) &&
			    !ieee80211_has_protected(hdr->frame_control) &&
			    (skb->len + FCS_LEN > priv->rts_threshold))
				prot_type = USE_PROTECTION_RTS;
		}

		txcmd->rate_protection_type[index] = prot_type;


		/*Some Sanity Checks*/
		if (nss <= max(MAX_TX_STREAMS, MAX_RX_STREAMS))
			/*Got at-least one valid rate*/
			all_rates_invalid = false;
		else {
			if (net_ratelimit())
				RPU_DEBUG_IF("RPU_WIFI:Skip Nss: %d\n",
					      nss);
			skip_rate = true;
		}

		/*First Time*/
			if (!index)
				pkt_info->max_retries = 0;
			pkt_info->max_retries +=
				txcmd->rate_retries[index];
		if (skip_rate)
			txcmd->rate_retries[index] = 0;

	}

	if (all_rates_invalid) {
		/*use min supported rate*/
		if (net_ratelimit())
			RPU_INFO_TX("RPU_WIFI:invalid rates\n");
		txcmd->num_rates = 1;
		txcmd->rate[0] = min_rate;
		txcmd->rate_retries[0] = 4;
		txcmd->rate_protection_type[0] = USE_PROTECTION_NONE;
		txcmd->rate_preamble_type[0] = DONT_USE_SHORT_PREAMBLE;
		txcmd->num_spatial_streams[0] = 1;
		txcmd->bcc_or_ldpc = 0;
		txcmd->stbc_enabled = 0;
		txcmd->rate_flags[0] = 0;
	}
}

static inline void rpu_send(void *nbuf, struct img_priv *priv)
{
	struct sk_buff *pending_cmd;
	int send_pnd_cnt = 0;
	int need_pending = 0;
	int num_of_pending;
#ifdef OUTS_CMD_CHECK
	struct sk_buff *skb, *tmp;
#endif

	/* Take lock to make the control commands sequential in case of SMP*/
	spin_lock_bh(&cmd_info.control_path_lock);
	if (!block_rpu_comm && cmd_info.outstanding_ctrl_req < MAX_OUTSTANDING_CTRL_REQ) {
		while (1) {
			pending_cmd = skb_dequeue(&cmd_info.outstanding_cmd);
			if (!pending_cmd)
				break;
			hal_ops.send((void *)pending_cmd , NULL, 0);
			if (cmd_info.outstanding_ctrl_req > 0)
				cmd_info.outstanding_ctrl_req--;
			priv->stats->gen_cmd_send_count++;
			send_pnd_cnt++;
			if (send_pnd_cnt >= MAX_OUTSTANDING_CTRL_REQ) {
				need_pending = 1;
				break;
			}
		}
	} else {
		need_pending = 1;
	}
	
	if (need_pending) {
		RPU_DEBUG_IF("Sending the CMD, Waiting in Queue: %d\n",
			     cmd_info.outstanding_ctrl_req);
#ifdef OUTS_CMD_CHECK
		if (hpriv->fw_error_processing)
			SET_TIME_TICKS_TO_SKB_CB((struct sk_buff *)nbuf, 0);
		else
			SET_TIME_TICKS_TO_SKB_CB((struct sk_buff *)nbuf, jiffies);
#endif
		skb_queue_tail(&cmd_info.outstanding_cmd, nbuf);
	} else {
		RPU_DEBUG_IF("Sending the CMD, got Access\n");
		hal_ops.send((void *)nbuf, NULL, 0);
		priv->stats->gen_cmd_send_count++;
	}

	/* sent but still no proc_done / unsent due to pending requests */
	cmd_info.outstanding_ctrl_req++;

	num_of_pending = skb_queue_len(&cmd_info.outstanding_cmd);
#ifdef OUTS_CMD_CHECK
	/* we had found that,  loss of cmd_proc_done will happens low probability
	 * that will cause outstanding_ctrl_req >= MAX_OUTSTANDING_CTRL_REQ
	 * Finally cause driver stop send any commands to rpu
	 * Add a check mechanism here
	*/
	if (num_of_pending) {
		int pending = 0;

		skb_queue_walk_safe(&cmd_info.outstanding_cmd, skb, tmp) {
			if (!skb || GET_TIME_TICKS_FROM_SKB_CB(skb) == 0)
				continue;
			if (time_after(jiffies, GET_TIME_TICKS_FROM_SKB_CB(skb) + msecs_to_jiffies(300))) {
				RPU_INFO_IF("detect loss of cmd_proc_done (%lu, %lu)\n",
								jiffies, GET_TIME_TICKS_FROM_SKB_CB(skb));
				rpu_process_pending_cmd(priv, 0);
				pending = 1;
			}
			if (pending) { // update other outstanding cmd
				SET_TIME_TICKS_TO_SKB_CB(skb, jiffies);
			}
		}
	}
#endif

	if (num_of_pending > priv->stats->max_outstanding_cmd_queue_cnt)
		priv->stats->max_outstanding_cmd_queue_cnt = num_of_pending;
	priv->stats->outstanding_cmd_cnt = cmd_info.outstanding_ctrl_req;
	spin_unlock_bh(&cmd_info.control_path_lock);
}

#ifdef REGENERATE_CMD_TEST
static int cmd_channel = 0;
static int cmd_vif_cfg = 0;
#endif
static int rpu_send_cmd(unsigned char *buf,
				unsigned int len,
				unsigned char id)
{
	struct host_rpu_msg_hdr *hdr = (struct host_rpu_msg_hdr *)buf;
	struct sk_buff *nbuf;
	struct rpu_if_data *p;
	struct img_priv *priv;
	char cmd_str[64];

	convert_cmd_to_str(id, cmd_str);

#ifdef DUMP_MORE_DEBUG_INFO
	if (id == RPU_CMD_VIF_CFG) {
		int skip;
		skip = dump_vif_cfg_changed_info(buf);
	}
#endif

	rcu_read_lock();

	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));

	if (!p) {
		RPU_ERROR_IF("%s: Unable to retrieve rpu_if\n", __func__);
		WARN_ON(1);
		rcu_read_unlock();
		return -1;
	}
	priv= p->context;
	nbuf = alloc_skb(len, GFP_ATOMIC);

	if (!nbuf) {
		RPU_ERROR_IF("%s: ENOMEM\n", __func__);
		rcu_read_unlock();
		//WARN_ON(1);
		return -ENOMEM;
	}
	hdr->id = id;
	RPU_DEBUG_IF("%s-RPUIF: Sending command:%d(%s), outstanding_cmds: %d\n",
		     p->name, hdr->id, cmd_str, cmd_info.outstanding_ctrl_req);
	hdr->length = len;
	hdr->descriptor_id = 0;
	hdr->descriptor_id |= 0x0000ffff;

	memcpy(skb_put(nbuf, len), buf, len);
	//priv->stats->outstanding_cmd_cnt = cmd_info.outstanding_ctrl_req;

	rpu_send(nbuf, priv);
	
	rcu_read_unlock();

	return 0;
}

#ifdef SDIO_TXRX_STABILITY_TEST
#include <asm/div64.h>
#define CALCULATE_RATE_LENGTH (50*1024*1024)
#define TX_SKB_NUM 10
INIT_GET_SPEND_TIME(start_time, end_time);
extern void hal_send_direct(void *msg);
static u8 test_data[2048];
int rpu_prog_txrx_test(int status)
{
	struct cmd_txrx_test test;

#if 1
#if 1 // only cmd_tx test
	int i, j, count = 1024*1024, total = 0;
	struct sk_buff *test_skb[TX_SKB_NUM];
	struct sk_buff *skb;
	int test_skb_len = 1568;
	int len;
	struct cmd_tx_ctrl cmd_tx_ctrl;
	struct cmd_tx_ctrl *tx_cmd = &cmd_tx_ctrl;
	int queue = 3;
	int descriptor_id = 3;

	memset(test_data, 0xAA, 2048);
	skb = alloc_skb(sizeof(struct cmd_tx_ctrl), GFP_ATOMIC);
	for (i = 0; i < TX_SKB_NUM; i++) {
		test_skb[i] = alloc_skb(test_skb_len, GFP_ATOMIC);
		memcpy(skb_put(test_skb[i], test_skb_len), test_data, test_skb_len);
	}

	/* HAL UMAC-LMAC HDR*/
	tx_cmd->hdr.id = RPU_CMD_TX;
	/* Keep the queue num and pool id in descriptor id */
	tx_cmd->hdr.descriptor_id = 0;
	tx_cmd->hdr.descriptor_id |= ((queue & 0x0000FFFF) << 16);
	tx_cmd->hdr.descriptor_id |= (descriptor_id & 0x0000FFFF);
	/* Not used anywhere currently */
	tx_cmd->hdr.length = sizeof(struct cmd_tx_ctrl);

	/* RPU_CMD_TX*/
	tx_cmd->if_index = 0;
	tx_cmd->queue_num = queue;
	tx_cmd->more_frms = 0;
	tx_cmd->descriptor_id = descriptor_id;
	tx_cmd->num_frames_per_desc = TX_SKB_NUM; 

	for (j = 0; j < TX_SKB_NUM; j++) {
		tx_cmd->pkt_length[j] = test_skb_len;
		tx_cmd->p_frame_ddr_pointer[j] = (unsigned int *)test_skb[j]->data;	
	}
	memcpy(skb_put(skb, sizeof(struct cmd_tx_ctrl)), tx_cmd, sizeof(struct cmd_tx_ctrl));

	// start cmd_tx test
	RPU_INFO_IF("%s: start cmd_tx test\n", __func__);
	memset(&test, 0, sizeof(struct cmd_txrx_test));
	test.status = status;
	rpu_send_cmd((unsigned char *) &test,
				    sizeof(struct cmd_txrx_test),
				    RPU_CMD_TXRX_TEST);

	// do cmd_tx test
	START_GET_SPEND_TIME(start_time, end_time);
	for (i = 0; i < count; i++) {
		//RPU_INFO_IF("rk915_data_write: %d\n", skb->len);
		rk915_data_write(hpriv, 0, skb->data, skb->len);
		if (rpu_is_cmd_has_data(skb->data)) {
			rpu_send_cmd_datas(skb->data, hpriv);
		}

		len = test_skb_len*TX_SKB_NUM;
		total += len;
		if (total >= CALCULATE_RATE_LENGTH) {
			int spend_time;
			END_GET_SPEND_TIME(start_time, end_time);
			spend_time = GET_SPEND_TIME_US(start_time, end_time)/1000;
			RPU_INFO_IF("send %d Mbytes (every pkgs len = %d) use %d ms\n", total/1024/1024, len, spend_time);
			total = 0;
			START_GET_SPEND_TIME(start_time, end_time);
		}
	}
#else // only sdio RX test
	// start rx test
	RPU_INFO_IF("%s: start rx test\n", __func__);
	memset(&test, 0, sizeof(struct cmd_txrx_test));
	test.status = TXRX_TEST_START_RX;
	rpu_send_cmd_without_delay((unsigned char *) &test,
				    sizeof(struct cmd_txrx_test),
				    RPU_CMD_TXRX_TEST);
	START_GET_SPEND_TIME(start_time, end_time);
#endif
#else
#if 0 // only sdio TX test
	int i, count = 1024*1024, total = 0;
	// start tx test
	RPU_INFO_IF("%s: start tx test\n", __func__);
	memset(&test, 0, sizeof(struct cmd_txrx_test));
	test.status = status;
	rpu_send_cmd((unsigned char *) &test,
				    sizeof(struct cmd_txrx_test),
				    RPU_CMD_TXRX_TEST);

	// do tx test
	START_GET_SPEND_TIME(start_time, end_time);
	for (i = 0; i < count; i++) {
		char tx_test_data[4096];
		struct cmd_txrx_test *cmd;
		int len, data_len ;

		data_len = 512*3;
		len = data_len + sizeof(struct cmd_txrx_test);

		memset(tx_test_data, 0xAA, len);
		memset(tx_test_data, 0, sizeof(struct cmd_txrx_test));
		cmd = (struct cmd_txrx_test *)tx_test_data;
		cmd->status = TXRX_TEST_TX;
		//RPU_INFO_IF("%s: do tx test (len=%d)\n", __func__, len);
		rpu_send_cmd_without_delay((unsigned char *) &tx_test_data,
				    len,
				    RPU_CMD_TXRX_TEST);
		total += len;
		if (total >= CALCULATE_RATE_LENGTH) {
			int spend_time;
			END_GET_SPEND_TIME(start_time, end_time);
			spend_time = GET_SPEND_TIME_US(start_time, end_time)/1000;
			RPU_INFO_IF("send %d Mbytes (every pkgs len = %d) use %d ms\n", total/1024/1024, len, spend_time);
			total = 0;
			START_GET_SPEND_TIME(start_time, end_time);
		}
	}
#else // only sdio RX test
	// start rx test
	RPU_INFO_IF("%s: start rx test\n", __func__);
	memset(&test, 0, sizeof(struct cmd_txrx_test));
	test.status = TXRX_TEST_START_RX;
	rpu_send_cmd_without_delay((unsigned char *) &test,
				    sizeof(struct cmd_txrx_test),
				    RPU_CMD_TXRX_TEST);
	START_GET_SPEND_TIME(start_time, end_time);
#endif
#endif

	while (1) {
		msleep(1000);
	}
	return 0;
}

static int rx_total_len = 0;
static void rpu_txrx_test_receive(struct host_rpu_msg_hdr *hdr)
{
	rx_total_len += hdr->length;
	if (rx_total_len >= CALCULATE_RATE_LENGTH) {
		int spend_time;
		END_GET_SPEND_TIME(start_time, end_time);
		spend_time = GET_SPEND_TIME_US(start_time, end_time)/1000;
		RPU_INFO_IF("receive %d Mbytes (every pkgs len = %d) use %d ms\n", rx_total_len/1024/1024, hdr->length, spend_time);
		rx_total_len = 0;
		START_GET_SPEND_TIME(start_time, end_time);
	}
}
#endif

/* rpu_send_cmd_without_delay, will not block by MAX_OUTSTANDING_CTRL_REQ */
static int rpu_send_cmd_without_delay(unsigned char *buf,
				unsigned int len,
				unsigned char id)
{
	struct host_rpu_msg_hdr *hdr = (struct host_rpu_msg_hdr *)buf;
	struct sk_buff *nbuf;
	struct rpu_if_data *p;
	struct img_priv *priv;
	char cmd_str[64];

	convert_cmd_to_str(id, cmd_str);

	rcu_read_lock();

	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));

	if (!p) {
		RPU_ERROR_IF("%s: Unable to retrieve rpu_if\n", __func__);
		WARN_ON(1);
		rcu_read_unlock();
		return -1;
	}
	priv= p->context;
	nbuf = alloc_skb(len, GFP_ATOMIC);

	if (!nbuf) {
		rcu_read_unlock();
		WARN_ON(1);
		return -ENOMEM;
	}
	hdr->id = id;
	hdr->length = len;
	hdr->descriptor_id = 0;
	hdr->descriptor_id |= 0x0000ffff;

	memcpy(skb_put(nbuf, len), buf, len);
	//priv->stats->outstanding_cmd_cnt = cmd_info.outstanding_ctrl_req;

#ifdef SDIO_TXRX_STABILITY_TEST
	hal_send_direct(nbuf);
#else
	spin_lock_bh(&cmd_info.control_path_lock);
	RPU_DEBUG_IF("Sending the CMD, got Access\n");
	hal_ops.send((void *)nbuf, NULL, 0);
	priv->stats->gen_cmd_send_count++;
	spin_unlock_bh(&cmd_info.control_path_lock);
#endif

	rcu_read_unlock();

	return 0;
}

static int fw_priv_init(void *context, struct fw_priv_cmd *info) {
	struct img_priv *priv = (struct img_priv *)context;

	info->production_test = priv->params->production_test;
	info->fw_skip_rx_pkt_submit = priv->params->fw_skip_rx_pkt_submit;
        info->wlan_mac_addr = vif_macs[0][0];
        info->p2p_mac_addr = vif_macs[1][0];
	return 0;
}

int rpu_fw_priv_cmd(unsigned int type, void *priv)
{
	struct fw_priv_cmd info;
	struct rpu_if_data *p;
	struct fw_reg_info *reg = (struct fw_reg_info *)priv;

	rcu_read_lock();
	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));

	if (!p) {
		WARN_ON(1);
		rcu_read_unlock();
		return -1;
	}
	rcu_read_unlock();

	if (type == DUMP_REG_INFO)
		rpu_fw_info_dump_start(p->context, type, reg->reg);
	else
		rpu_fw_info_dump_start(p->context, type, 0);

	memset(&info, 0, sizeof(struct fw_priv_cmd));
	info.type = type;
	if (type == DUMP_REG_INFO) {
		int mod = reg->len % 4;
		if (mod) {
			reg->len += 4 - mod;
		}
		
		info.reg_info.reg = reg->reg;
		info.reg_info.val = reg->val;
		info.reg_info.len = reg->len;
		info.reg_info.rw  = reg->rw;
	} else if (type == FW_PRIV_INIT) {
		fw_priv_init(p->context, &info);
	} else if (type == FW_SET_PARAMS) {
		memcpy(&info.params, priv, sizeof(struct fw_params));
	} else if (type == ENABLE_SNIFFER) {
		int sniffer = *((int *)priv);

		if (sniffer > 3)
			sniffer = 0;
		info.sniffer = sniffer;
		pr_info("sniffer %d\n", info.sniffer);
	}

	return rpu_send_cmd_without_delay((unsigned char *) &info,
				sizeof(struct fw_priv_cmd),
				RPU_CMD_FW_PRIV_CMD);
}

int rpu_fw_priv_cmd_sync(unsigned int type, void *priv)
{
	int ret = 0;
	int count = 100;
	struct fw_info_dump *fw_info = &wifi->fw_info;

	ret = rpu_fw_priv_cmd(type, priv);
	if (ret != 0) {
		pr_err("%s: cmd(%d) send fail!\n", __func__, type);
		return ret;
	}

	while (fw_info->finish == 0 || fw_info->type != type) {
		if (count-- < 0) {
			pr_err("%s: cmd(%d) timeout\n", __func__, type);
			return -1;
		}
		msleep(10);
	}

	return 0;
}

int rpu_prog_reset(unsigned int reset_type, unsigned int rpu_mode)
{
	struct cmd_reset reset;
	struct img_priv *priv;
	struct rpu_if_data *p;
	unsigned int i;

	rcu_read_lock();
	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));

	if (!p) {
		WARN_ON(1);
		rcu_read_unlock();
		return -1;
	}
	rcu_read_unlock();
	priv= p->context;

	memset(&reset, 0, sizeof(struct cmd_reset));

	reset.type = reset_type;

	reset.reserv[0] = 0x01b50052; // 0x468bc
	reset.reserv[1] = 0x017901b5; // 0x468c0

	if (reset_type != LMAC_DISABLE) {
		priv->cmd_reset_count++;
		reset.type |= LOAD_FACTORY_CAL;
#ifdef RK915
		reset.type |= LMAC_DO_CALIB;		
#endif
		RPU_DEBUG_IF("ed = %d auto = %d\n",
			priv->params->ed_sensitivity,
			priv->params->auto_sensitivity);
		reset.num_spatial_streams =
			priv->params->uccp_num_spatial_streams;
		reset.lmac_mode = rpu_mode;
		/* Always force RF calibration */
		reset.lmac_mode |= LMAC_DO_CALIB;
		reset.antenna_sel = priv->params->antenna_sel;

		if (priv->params->production_test == 0 &&
			priv->params->bypass_vpd == 0) {
			memcpy(reset.rf_params, priv->params->rf_params_vpd,
			       RF_PARAMS_SIZE);
		} else {
			memcpy(reset.rf_params, priv->params->rf_params,
			       RF_PARAMS_SIZE);
		}

		reset.system_rev = priv->stats->system_rev;
		reset.bg_scan.enabled = priv->params->bg_scan_enable;

		if (reset.bg_scan.enabled) {
			for (i = 0; i < priv->params->bg_scan_num_channels;
			     i++) {
				reset.bg_scan.channel_list[i] =
					priv->params->bg_scan_channel_list[i];
				reset.bg_scan.channel_flags[i] =
					priv->params->bg_scan_channel_flags[i];
			}
			reset.bg_scan.num_channels =
				priv->params->bg_scan_num_channels;
			reset.bg_scan.scan_intval =
				priv->params->bg_scan_intval;
			reset.bg_scan.channel_dur =
				/* Channel spending time */
				priv->params->bg_scan_chan_dur;

			reset.bg_scan.serv_channel_dur =
				/* operating channel spending time */
				priv->params->bg_scan_serv_chan_dur;
		}
	}

	RPU_INFO_IF("%s: reset_type=0x%x, rpu_mode=0x%x\n", __func__, reset.type, rpu_mode);

	return rpu_send_cmd((unsigned char *) &reset,
				    sizeof(struct cmd_reset), RPU_CMD_RESET);
}

int rpu_prog_phy_thresh(unsigned int *thresh)
{
	struct cmd_update_phy_thresh cmd;
	int i;

	RPU_DEBUG_DAPT("%s: thresh: "
					"%03d %03d %03d %03d "
					"%03d %03d %03d %03d "
					"%03d %03d %03d %03d "
					"%03d %03d\n", __func__,
					thresh[0], thresh[1], thresh[2], thresh[3],
					thresh[4], thresh[5], thresh[6], thresh[7],
					thresh[8], thresh[9], thresh[10], thresh[11],
					thresh[12], thresh[13]);

	memset(&cmd, 0, sizeof(struct cmd_update_phy_thresh));
	for (i = 0; i < 14; i++) {
		cmd.thresholds[i] = (unsigned char)thresh[i];
	}

	return rpu_send_cmd((unsigned char *) &cmd,
				    sizeof(struct cmd_update_phy_thresh),
				    RPU_CMD_UPD_PHY_THRESH);
}

int rpu_prog_cfgmisc(unsigned int flag)
{
	struct cmd_cfg_misc cmisc;

	memset(&cmisc, 0, sizeof(struct cmd_cfg_misc));
	cmisc.flags = RPU_MISC_CFG_SNIFF_MODE_MASK;
	cmisc.sniff_mode = flag;

	return rpu_send_cmd((unsigned char *) &cmisc,
				    sizeof(struct cmd_cfg_misc),
				    RPU_CMD_MISC_CFG);
}

int rpu_prog_txpower(unsigned int txpower)
{
	struct cmd_tx_pwr power;

	memset(&power, 0, sizeof(struct cmd_tx_pwr));
	power.tx_pwr = txpower;
	power.if_index = 0;

	return rpu_send_cmd((unsigned char *) &power,
				    sizeof(struct cmd_tx_pwr),
				    RPU_CMD_TX_POWER);
}

int rpu_prog_patch_feature(unsigned int feature)
{
	struct cmd_patch_feature patch;

	memset(&patch, 0, sizeof(struct cmd_patch_feature));
	patch.feature_val = feature;

	return rpu_send_cmd((unsigned char *) &patch,
				    sizeof(struct cmd_patch_feature),
				    RPU_CMD_PATCH_FEATURES);
}

int rpu_prog_vif_ctrl(int index,
		unsigned char *mac_addr,
		unsigned int vif_type,
		unsigned int op)
{
	struct cmd_vifctrl vif_ctrl;

	memset(&vif_ctrl, 0, sizeof(struct cmd_vifctrl));
	vif_ctrl.mode = vif_type;
	memcpy(vif_ctrl.mac_addr, mac_addr, 6);
	vif_ctrl.if_index = index;
	vif_ctrl.if_ctrl = op;

	return rpu_send_cmd((unsigned char *) &vif_ctrl,
				    sizeof(struct cmd_vifctrl),
				    RPU_CMD_VIF_CTRL);
}


int rpu_prog_mcast_addr_cfg(unsigned char *mcast_addr,
				    unsigned int op)
{
	struct cmd_mcst_addr_cfg mcast_config;

	//RPU_INFO_UMACIF("RPU_CMD_MCST_ADDR_CFG: %s %pM\n",
	//		op==WLAN_MCAST_ADDR_ADD ? "ADD":"REM", mcast_addr);

	memset(&mcast_config, 0, sizeof(struct cmd_mcst_addr_cfg));

	mcast_config.op = op;
	memcpy(mcast_config.mac_addr, mcast_addr, 6);

	return rpu_send_cmd((unsigned char *) &mcast_config,
				    sizeof(struct cmd_mcst_addr_cfg),
				    RPU_CMD_MCST_ADDR_CFG);
}


int rpu_prog_mcast_filter_control(unsigned int mcast_filter_enable)
{
	struct cmd_mcst_filter_ctrl mcast_ctrl;

	//RPU_INFO_UMACIF("RPU_CMD_MCST_FLTR_CTRL: %s\n",
	//		mcast_filter_enable==MCAST_FILTER_ENABLE ? "ENABLE":"DISABLE");

	memset(&mcast_ctrl, 0, sizeof(struct cmd_mcst_filter_ctrl));
	mcast_ctrl.ctrl = mcast_filter_enable;

	return rpu_send_cmd((unsigned char *) &mcast_ctrl,
				    sizeof(struct cmd_mcst_filter_ctrl),
				    RPU_CMD_MCST_FLTR_CTRL);
}



int rpu_prog_roc(unsigned int roc_ctrl,
			 unsigned int roc_channel,
			 unsigned int roc_duration,
			 unsigned int roc_type)
{
	struct cmd_roc cmd_roc;

	memset(&cmd_roc, 0, sizeof(struct cmd_roc));

	cmd_roc.roc_ctrl = roc_ctrl;
	cmd_roc.roc_channel	= roc_channel;
	cmd_roc.roc_duration = roc_duration;
	cmd_roc.roc_type = roc_type;

	return rpu_send_cmd((unsigned char *) &cmd_roc,
			sizeof(struct cmd_roc), RPU_CMD_ROC_CTRL);
}


int rpu_prog_peer_key(int vif_index,
			      unsigned char *vif_addr,
			      unsigned int op,
			      unsigned int key_id,
			      unsigned int key_type,
			      unsigned int cipher_type,
			      struct umac_key *key)
{
	struct cmd_setkey peer_key;

	memset(&peer_key, 0, sizeof(struct cmd_setkey));

	peer_key.if_index = vif_index;
	/* memcpy(peer_key.vif_addr, vif_addr, ETH_ALEN); */
	peer_key.ctrl = op;
	peer_key.key_id = key_id;
	img_ether_addr_copy(peer_key.mac_addr, key->peer_mac);

	peer_key.key_type = key_type;
	peer_key.cipher_type = cipher_type;
	memcpy(peer_key.key, key->key, MAX_KEY_LEN);
	peer_key.key_len = MAX_KEY_LEN;

	if (key->tx_mic) {
		memcpy(peer_key.key + MAX_KEY_LEN, key->tx_mic, TKIP_MIC_LEN);
		peer_key.key_len += TKIP_MIC_LEN;
	}
	if (key->rx_mic) {
		memcpy(peer_key.key + MAX_KEY_LEN + TKIP_MIC_LEN, key->rx_mic,
		       TKIP_MIC_LEN);
		peer_key.key_len += TKIP_MIC_LEN;
	}
	peer_key.rsc_len = 6;
	memset(peer_key.rsc, 0, 6);

	return rpu_send_cmd((unsigned char *) &peer_key,
				    sizeof(struct cmd_setkey), RPU_CMD_SETKEY);
}


int rpu_prog_if_key(int vif_index,
			    unsigned char *vif_addr,
			    unsigned int op,
			    unsigned int key_id,
			    unsigned int cipher_type,
			    struct umac_key *key)
	{
	struct cmd_setkey if_key;

	memset(&if_key, 0, sizeof(struct cmd_setkey));

	if_key.if_index = vif_index;
	/* memcpy(if_key.vif_addr, vif_addr, 6); */
	if_key.key_id = key_id;
	if_key.ctrl = op;

	if (op == KEY_CTRL_ADD) {
		if_key.cipher_type = cipher_type;

		if (cipher_type == CIPHER_TYPE_TKIP ||	cipher_type ==
		    CIPHER_TYPE_CCMP) {
			memcpy(if_key.key, key->key, MAX_KEY_LEN);
			if_key.key_len = MAX_KEY_LEN;

			if (key->tx_mic) {
				memcpy(if_key.key + MAX_KEY_LEN, key->tx_mic,
				       TKIP_MIC_LEN);
				if_key.key_len += TKIP_MIC_LEN;
			}
		} else {
			if_key.key_len =
				(cipher_type == CIPHER_TYPE_WEP40) ? 5 : 13;
			memcpy(if_key.key, key->key, if_key.key_len);
		}
	}

	if_key.rsc_len = 6;
	if_key.key_type = KEY_TYPE_BCAST;
	memset(if_key.rsc, 0, 6);
	memset(if_key.mac_addr, 0xff, 6);

	return rpu_send_cmd((unsigned char *) &if_key,
				    sizeof(struct cmd_setkey), RPU_CMD_SETKEY);
}

int rpu_prog_ba_session_data(unsigned int op,
				     unsigned short tid,
				     unsigned short *ssn,
				     unsigned short ba_policy,
				     unsigned char *vif_addr,
				     unsigned char *peer_addr)
{
	struct cmd_ht_ba ba_cmd;
	int index;
	struct img_priv *priv;
	struct rpu_if_data *p;
	struct ieee80211_vif *vif = NULL;

	rcu_read_lock();
	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));

	if (!p) {
		WARN_ON(1);
		rcu_read_unlock();
		return -1;
	}

	priv= p->context;

	memset(&ba_cmd, 0, sizeof(struct cmd_ht_ba));

	for (index = 0; index < priv->params->num_vifs; index++) {
		if (!(priv->active_vifs & (1 << index)))
			continue;

		vif = rcu_dereference(priv->vifs[index]);

		if (ether_addr_equal(vif->addr, vif_addr))
			break;
	}

	if (index == priv->params->num_vifs) {
		RPU_INFO_IF("no VIF found\n");
		rcu_read_unlock();
		return -1;
	}

	ba_cmd.if_index = index;
	ba_cmd.op = op;
	ba_cmd.policy = ba_policy;
	ba_cmd.tid = tid;
	ba_cmd.ssn = *ssn;
	img_ether_addr_copy(ba_cmd.vif_addr, vif_addr);
	img_ether_addr_copy(ba_cmd.peer_addr, peer_addr);

	rcu_read_unlock();

	return rpu_send_cmd((unsigned char *) &ba_cmd,
				    sizeof(struct cmd_ht_ba),
				    RPU_CMD_BA_SESSION_INFO);
}

#ifdef ENABLE_KEEP_ALIVE
extern void rpu_send_nullframe(struct img_priv *priv);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
void keep_alive_expiry(struct timer_list *t)
#else
void keep_alive_expiry(unsigned long data)
#endif
{
	struct rpu_if_data *p;
	struct img_priv *priv;

	rcu_read_lock();

	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));
	if (!p) {
		//WARN_ON(1);
		rcu_read_unlock();
		return;
	}
	priv = (struct img_priv *)p->context;

	RPU_DEBUG_UMACIF("%s: %p\n", __func__, priv);

	if (hpriv->fw_error_processing ||
		priv->state != STARTED) {
		rcu_read_unlock();
		return;
	}

	if (is_wlan_connected(priv)) {
		RPU_DEBUG_UMACIF("%s: send null f\n", __func__);
		rpu_send_nullframe(priv);

		mod_timer(&priv->keep_alive_timer,
				jiffies + SEND_NULL_FRAME_INTERVAL_SECONDS * HZ);
	}

	rcu_read_unlock();
}
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
void roc_timer_expiry(struct timer_list *t)
#else
void roc_timer_expiry(unsigned long data)
#endif
{
	struct rpu_if_data *p;
	struct img_priv *priv;
	struct delayed_work *work = NULL;

	RPU_DEBUG_UMACIF("%s: \n", __func__);

	rcu_read_lock();

	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));
	if (!p) {
		//WARN_ON(1);
		rcu_read_unlock();
		return;
	}

	priv = (struct img_priv *)p->context;
	if (priv->roc_params.roc_in_progress == 1) {
		work = &priv->roc_complete_work;
		ieee80211_queue_delayed_work(priv->hw,
    				     work,
    				     0);
	}

	rcu_read_unlock();
}

#ifdef HW_SCAN_TIMEOUT_ABORT
extern void cancel_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
void scan_timer_expiry(struct timer_list *t)
#else
void scan_timer_expiry(unsigned long data)
#endif
{
	struct rpu_if_data *p;
	struct img_priv *priv;

	RPU_DEBUG_UMACIF("%s: \n", __func__);

	rcu_read_lock();

	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));
	if (!p) {
		//WARN_ON(1);
		rcu_read_unlock();
		return;
	}

	priv = (struct img_priv *)p->context;
	priv->in_scan_timeout = 1;
	cancel_hw_scan(priv->hw, NULL);
	priv->in_scan_timeout = 0;

	rcu_read_unlock();
}
#endif

int rpu_scan(int index,
		     struct scan_req *req)
{
	struct cmd_scan *scan;
	unsigned char i;
	struct img_priv *priv;
	struct rpu_if_data *p;

	rcu_read_lock();
	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));

	if (!p) {
		WARN_ON(1);
		rcu_read_unlock();
		return -1;
	}

	rcu_read_unlock();
	priv = p->context;

	scan = kmalloc(sizeof(struct cmd_scan) +
		       req->ie_len, GFP_KERNEL);

	if (scan == NULL) {
		RPU_ERROR_IF("%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	memset(scan, 0, sizeof(struct cmd_scan));

	scan->if_index = index;

	/* We support 4 SSIDs */
	scan->n_ssids = req->n_ssids;
	scan->n_channel = req->n_channels;
	scan->type = priv->params->scan_type;

	for (i = 0; i < scan->n_channel; i++) {
		scan->channel_list[i] =
			(ieee80211_frequency_to_channel(req->center_freq[i]));
		scan->chan_max_power[i] = req->freq_max_power[i];

		/* scan->chan_max_antenna_gain[i] =
		 * req->freq_max_antenna_gain[i];
		 */

		/* In mac80211 the flags are u32 but for scanning we need
		 * only first PASSIVE_SCAN flag, remaining flags may be used
		 * in future.
		 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		if (req->chan_flags[i] & IEEE80211_CHAN_NO_IR) {
#else
		if (req->chan_flags[i] & IEEE80211_CHAN_PASSIVE_SCAN) {
#endif
			scan->chan_flags[i] = PASSIVE;
		} else {
			scan->chan_flags[i] = ACTIVE;
		}
	}

	scan->p2p_probe = req->p2p_probe;

	scan->extra_ies_len = req->ie_len;

	if (req->ie_len)
		memcpy(scan->extra_ies, req->ie, req->ie_len);

	if (req->n_ssids > 0) {
		for (i = 0; i < scan->n_ssids; i++) {
			scan->ssids[i].len = req->ssids[i].ssid_len;
			if (scan->ssids[i].len > 0)
				memcpy(scan->ssids[i].ssid, req->ssids[i].ssid,
				       req->ssids[i].ssid_len);
		}
	}
	RPU_DEBUG_SCAN("Scan request ie len = %d n_channel = %d,",
						req->ie_len,
						scan->n_channel);
	RPU_DEBUG_SCAN(" n_ssids = %d, if_index = %d type = %d p2p = %d\n",
						scan->n_ssids,
						scan->if_index,
						scan->type,
						scan->p2p_probe);

	for (i = 0; i < scan->n_ssids; i++) {
		if (scan->ssids[i].len != 0)
			RPU_DEBUG_SCAN("SSID: %s\n", scan->ssids[i].ssid);
		else
			RPU_DEBUG_SCAN("SSID: EMPTY\n");
	}

	RPU_DEBUG_SCAN("CHANNEL_LIST: Channel ==> Channel Flags\n");

	for (i = 0; i < scan->n_channel; i++)
		RPU_DEBUG_SCAN("Index %d: %d ==> %d\n", i,
				scan->channel_list[i], scan->chan_flags[i]);

	priv->stats->umac_scan_req++;

	rpu_send_cmd((unsigned char *)scan, sizeof(struct cmd_scan) +
			     req->ie_len, RPU_CMD_SCAN);
	kfree(scan);

	return 0;
}


int rpu_scan_abort(int index)
{
	struct cmd_scan_abort *scan_abort = NULL;

	scan_abort = (struct cmd_scan_abort *)
		kmalloc(sizeof(struct cmd_scan_abort), GFP_KERNEL);

	if (scan_abort == NULL) {
		RPU_ERROR_IF("%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	memset(scan_abort, 0, sizeof(struct cmd_scan_abort));

	scan_abort->if_index = index;

	rpu_send_cmd((unsigned char *)scan_abort,
			     sizeof(struct cmd_scan_abort),
			     RPU_CMD_SCAN_ABORT);

	kfree(scan_abort);
	scan_abort = NULL;

	return 0;
}


int rpu_prog_channel(unsigned int prim_ch,
			     unsigned int center_freq1,
			     unsigned int center_freq2,
			     unsigned int ch_width,
			     unsigned int freq_band)
{
	struct cmd_channel channel;
	struct rpu_if_data *p;
	struct img_priv *priv;
	int ch_no1, ch_no2;
	int err = 0;
	unsigned int cf_offset = center_freq1;
#ifdef SDIO_CLOCK_SWITCH
	int hz = 50000000;

	RPU_INFO_UMACIF("set ch %d\n", prim_ch);

	if (of_machine_is_compatible("rockchip,rk3036") ||
	    of_machine_is_compatible("rockchip,rk3126")) {
		if (prim_ch == 2 || prim_ch == 3)
			hz = 30000000;
		else if (prim_ch == 4 || prim_ch == 5 ||
		         prim_ch == 12 || prim_ch == 13)
			hz = 40000000;
	} else {
		if (prim_ch == 7 || prim_ch == 8)
			hz = 30000000;
		else if (prim_ch == 9 || prim_ch == 10)
			hz = 40000000;
	}
	rk915_sdio_set_clock(hpriv->io_info, hz);
#endif
	memset(&channel, 0, sizeof(struct cmd_channel));

	rcu_read_lock();
	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));

	if (!p) {
		WARN_ON(1);
		rcu_read_unlock();
		return -1;
	}
	priv = p->context;

	ch_no1 = ieee80211_frequency_to_channel(cf_offset);
	ch_no2 = 0;

	channel.primary_ch_number = prim_ch;
	channel.channel_number1 = ch_no1;
	channel.channel_number2 = ch_no2;

	switch (ch_width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		channel.channel_bw = RPU_CHAN_WIDTH_20_NOHT;
		break;
	default:
		break;
	}

	channel.freq_band = freq_band;
	priv->cur_chan.center_freq1 = cf_offset;
	priv->cur_chan.center_freq2 = ch_no2;
	priv->cur_chan.pri_chnl_num = prim_ch;
	priv->cur_chan.ch_width  = ch_width;
	priv->cur_chan.freq_band = freq_band;

	rcu_read_unlock();

	priv->chan_prog_done = 0;

	err = rpu_send_cmd((unsigned char *) &channel,
				   sizeof(struct cmd_channel),
				   RPU_CMD_CHANNEL);


	if (err)
		return err;

	if (wait_for_channel_prog_complete(priv))
		return -1;

	return 0;
}




int rpu_prog_ps_state(int index,
			      unsigned char *vif_addr,
			      unsigned int powersave_state)
{
#ifdef RPU_ENABLE_PS
	struct cmd_ps ps_cfg;

	memset(&ps_cfg, 0, sizeof(struct cmd_ps));
	ps_cfg.mode = powersave_state;
	ps_cfg.if_index = index;

	RPU_INFO_UMACIF("%s: %pM (%s)\n", "CMD_PS",
			vif_addr, powersave_state ? "AWAKE":"SLEEP");

	return rpu_send_cmd((unsigned char *)&ps_cfg,
				    sizeof(struct cmd_ps), RPU_CMD_PS);
#else
	return 0;
#endif
}

bool rpu_is_cmd_has_data(unsigned char *data)
{
	struct cmd_tx_ctrl *tx_cmd = (struct cmd_tx_ctrl *)data;

	if (tx_cmd->hdr.id == RPU_CMD_TX &&
		tx_cmd->num_frames_per_desc > 0) {
		//RPU_INFO_HAL("tx desc %d\n", tx_cmd->descriptor_id);
		return true;
	}

	return false;
}

int rpu_send_cmd_datas(unsigned char *data, struct hal_priv *priv)
{
	struct cmd_tx_ctrl *tx_cmd = (struct cmd_tx_ctrl *)data;
	unsigned int pkt = 0;
	int ret;

	for (pkt = 0; pkt < tx_cmd->num_frames_per_desc; pkt++) {
		unsigned int pkt_len = tx_cmd->pkt_length[pkt];
		unsigned char *pkt_data = (unsigned char *)tx_cmd->p_frame_ddr_pointer[pkt];
		if (!pkt_data || pkt_len == 0) {
			RPU_ERROR_TX("%s: pkt_data(%p), pkt_len(%d)\n", __func__, pkt_data, pkt_len);
			continue;
		}
		RPU_DEBUG_TX("%s: sdio send: (pkt=%d) %p, %d\n",
					 __func__, pkt, pkt_data, pkt_len);
		dump_ieee80211_hdr_info(pkt_data, pkt_len, 1);
		RPU_DEBUG_DUMP_HAL(" ", DUMP_PREFIX_NONE, 16, 1, pkt_data, pkt_len, 1);
		ret = rk915_data_write(priv, 0, pkt_data, pkt_len);
		if (ret)
			RPU_ERROR_TX("%s: ret = %d, pkt_len = %d.\n", __func__, ret, pkt_len);
	}

	return 0;
}

void rpu_prog_tx_send(void *skb)
{
	spin_lock_bh(&cmd_info.control_path_lock);
	hal_ops.send(skb, NULL, 0);
	spin_unlock_bh(&cmd_info.control_path_lock);
}

int rpu_prog_tx(unsigned int queue,
			unsigned int more_frms,
			unsigned int descriptor_id,
			bool retry)
{
	struct cmd_tx_ctrl *tx_cmd;
	struct rpu_if_data *p;
	struct img_priv *priv;
	struct umac_vif *uvif;
	struct sk_buff *skb, *skb_first, *tmp, *tx_cmd_skb;
	struct sk_buff_head *txq = NULL;
	struct ieee80211_hdr *mac_hdr;
	struct ieee80211_tx_info *tx_info_first;
	unsigned int hdrlen, pkt = 0, extra_copy_gram = 0;
	int vif_index;
	__u16 fc;
	//dma_addr_t phy_addr;
	struct tx_pkt_info *pkt_info = NULL;
	unsigned int cmd_tx_size;
	rcu_read_lock();
	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));

	if (!p) {
		WARN_ON(1);
		rcu_read_unlock();
		return -1;
	}

	priv = p->context;
	spin_lock_bh(&priv->tx.lock);
	txq = &priv->tx.pkt_info[descriptor_id].pkt;
	pkt_info = &priv->tx.pkt_info[descriptor_id];
	skb_first = skb_peek(txq);

	if (!skb_first) {
		spin_unlock_bh(&priv->tx.lock);
		rcu_read_unlock();
		return -10;
	}

	cmd_tx_size = sizeof(struct cmd_tx_ctrl);

	tx_cmd_skb = alloc_skb(cmd_tx_size, GFP_ATOMIC);

	if (!tx_cmd_skb) {
		spin_unlock_bh(&priv->tx.lock);
		rcu_read_unlock();
		return -ENOMEM;
	}

	skb_put(tx_cmd_skb, cmd_tx_size);

	tx_cmd = (struct cmd_tx_ctrl *)tx_cmd_skb->data;
	memset(tx_cmd, 0, sizeof(struct cmd_tx_ctrl));
	
	tx_info_first = IEEE80211_SKB_CB(skb_first);

	mac_hdr = (struct ieee80211_hdr *)skb_first->data;
	fc = mac_hdr->frame_control;
	hdrlen = ieee80211_hdrlen(fc);
	vif_index = vif_addr_to_index(mac_hdr->addr2, priv);

	/* GET The security Header Length only for data/qos-data/unicast PMF
	 * for 11W case.
	 */
	if ((ieee80211_is_data(fc) ||
	     ieee80211_is_data_qos(fc))
	    && ieee80211_has_protected(fc)) {
		/* hw_key == NULL: Encrypted in SW (injected frames)
		 * iv_len = 0: treat as SW encryption.
		 */
		if (tx_info_first->control.hw_key == NULL ||
		    !tx_info_first->control.hw_key->iv_len ||
		    tx_info_first->control.hw_key->cipher == -1) {
			RPU_DEBUG_IF("%s: hw_key is %s and iv_len: 0\n",
			  __func__,
			  tx_info_first->control.hw_key?"valid":"NULL");
			tx_cmd->encrypt = ENCRYPT_DISABLE;
		 } else {
			RPU_DEBUG_IF("%s: cipher: %d, icv: %d",
				  __func__,
				  tx_info_first->control.hw_key->cipher,
				  tx_info_first->control.hw_key->icv_len);
			RPU_DEBUG_IF("iv: %d, key: %d\n",
				  tx_info_first->control.hw_key->iv_len,
				  tx_info_first->control.hw_key->keylen);
			/* iv_len is always the header and icv_len is always
			 * the trailer include only iv_len
			 */
			extra_copy_gram += tx_info_first->control.hw_key->iv_len;
			tx_cmd->encrypt = ENCRYPT_ENABLE;
		}
	}


	/* For injected frames (wlantest) hw_key is not set,as PMF uses
	 * CCMP always so hardcode this to CCMP IV LEN 8.
	 * For Auth3: It is completely handled in SW (mac80211).
	 */
	if (ieee80211_is_unicast_robust_mgmt_frame(skb_first) &&
	    ieee80211_has_protected(fc)) {
		extra_copy_gram += 8;
		tx_cmd->encrypt = ENCRYPT_ENABLE;
	}

	/* separate in to up to TSF and From TSF*/
	if (ieee80211_is_beacon(fc) || ieee80211_is_probe_resp(fc))
		extra_copy_gram += 8; /* Timestamp*/

	/* HAL Host-RPU HDR*/
	tx_cmd->hdr.id = RPU_CMD_TX;
	/* Keep the queue num and pool id in descriptor id */
	tx_cmd->hdr.descriptor_id = 0;
	tx_cmd->hdr.descriptor_id |= ((queue & 0x0000FFFF) << 16);
	tx_cmd->hdr.descriptor_id |= (descriptor_id & 0x0000FFFF);
	/* Not used anywhere currently */
	tx_cmd->hdr.length = sizeof(struct cmd_tx_ctrl);

	/* RPU_CMD_TX*/
	tx_cmd->if_index = vif_index;
	tx_cmd->queue_num = queue;
	tx_cmd->more_frms = more_frms;
	tx_cmd->descriptor_id = descriptor_id;
	tx_cmd->num_frames_per_desc = skb_queue_len(txq);
	tx_cmd->aggregate_mpdu = AMPDU_AGGR_DISABLED;

	/* These extra fields will be copied to GRAM along with 
	 * tx_cmd and 802.11 header
	 */
	hdrlen += extra_copy_gram;
	memcpy(tx_cmd->config_mac_header, skb_first->data,
	      hdrlen);
	tx_cmd->config_mac_hdr_len = hdrlen;

	priv->tx.pkt_info[descriptor_id].vif_index = vif_index;
	priv->tx.pkt_info[descriptor_id].queue = queue;

	uvif = (struct umac_vif *) (tx_info_first->control.vif->drv_priv);

	if (priv->params->production_test == 1)
		get_rate_prod(tx_cmd, priv);
	else
		/* Get the rate for first packet as all packets have same rate */
		get_rate(skb_first,
			 tx_cmd,
			 pkt_info,
			 retry,
			 priv);

	RPU_DEBUG_TX("%s-UMACTX: TX Frame, Queue = %d, descriptord_id = %d\n",
		     priv->name,
		     tx_cmd->queue_num, tx_cmd->descriptor_id);
	RPU_DEBUG_TX("num_frames= %d qlen: %d\n",
		     tx_cmd->num_frames_per_desc, skb_queue_len(txq));

	RPU_DEBUG_TX("%s-UMACTX: Num rates = %d, %x, %x, %x, %x\n",
		     priv->name,
		     tx_cmd->num_rates,
		     tx_cmd->rate[0],
		     tx_cmd->rate[1],
		     tx_cmd->rate[2],
		     tx_cmd->rate[3]);

	RPU_DEBUG_TX("%s-UMACTX: Retries   = %d, %d, %d, %d, %d\n",
		  priv->name,
		  pkt_info->max_retries,
		  tx_cmd->rate_retries[0],
		  tx_cmd->rate_retries[1],
		  tx_cmd->rate_retries[2],
		  tx_cmd->rate_retries[3]);


	skb_queue_walk_safe(txq, skb, tmp) {
		struct ieee80211_tx_info *tx_info;
		unsigned char *crypto_params;
		unsigned int mac_hdr_len;

		if (!skb || (pkt > tx_cmd->num_frames_per_desc))
			break;

		tx_info = IEEE80211_SKB_CB(skb_first);

		mac_hdr = (struct ieee80211_hdr *)skb->data;

		/* Only for Non-Qos and MGMT frames, for Qos-Data
		 * mac80211 handles the sequence no generation
		 */
		if (!retry &&
		    tx_info->flags &
		    IEEE80211_TX_CTL_ASSIGN_SEQ) {
			if (tx_info->flags &
			    IEEE80211_TX_CTL_FIRST_FRAGMENT) {
				uvif->seq_no += 0x10;
			}

			mac_hdr->seq_ctrl &= cpu_to_le16(IEEE80211_SCTL_FRAG);
			mac_hdr->seq_ctrl |= cpu_to_le16(uvif->seq_no);
		}

		/* Complete packet length */
		tx_cmd->pkt_length[pkt] = skb->len;

		/*RPU_DEBUG_DUMP_TX(DUMP_PREFIX_NONE,
				  16,
				  1,
				  skb->data,
				  skb->len,
				  1);*/
#if 0
		if (hal_ops.map_tx_buf(descriptor_id, pkt,
				       skb->data, skb->len, &phy_addr)) {
			spin_unlock_bh(&priv->tx.lock);
			rcu_read_unlock();
			kfree(tx_cmd_skb);
			return -30;
		}
#endif
		tx_cmd->p_frame_ddr_pointer[pkt] = (unsigned int *)skb->data;
	//	pr_err("%s:%d phy_addr:0x%x\n",__func__,__LINE__,phy_addr);
		crypto_params = tx_cmd->per_pkt_crypto_params[pkt];
		memset(crypto_params, 0, PER_PKT_CRYPTO_PARAMS_SIZE);

		memcpy(crypto_params + PER_PKT_CRYPTO_PARAMS_SEQ_CTRL_OFFSET,
		       &mac_hdr->seq_ctrl,
		       2);

		if (ieee80211_is_data_qos(mac_hdr->frame_control)) {
			struct ieee80211_qos_hdr *qos_mac_hdr;
			unsigned char *qos_offset;

			qos_offset = crypto_params +
				     PER_PKT_CRYPTO_PARAMS_QOS_CTRL_OFFSET;
			qos_mac_hdr = (struct ieee80211_qos_hdr *) skb->data;
			memcpy(qos_offset,
			       &qos_mac_hdr->qos_ctrl,
			       2);
		}

		mac_hdr_len =  ieee80211_hdrlen(mac_hdr->frame_control);
		if (tx_cmd->encrypt == ENCRYPT_ENABLE) {
			unsigned char *iv_offset;

			iv_offset = crypto_params +
				    PER_PKT_CRYPTO_PARAMS_IV_OFFSET;

			memcpy(iv_offset,
			       skb->data + mac_hdr_len,
			       tx_info->control.hw_key->iv_len);
		}

		pkt++;
	}

		/* SDK: Check if we can use the same txq initialized before in
		 * the function here
		 */
		txq = &priv->tx.pkt_info[descriptor_id].pkt;

		RPU_DEBUG_TX("%s:%d Sending TX_CMD\n",__func__,__LINE__);
  		RPU_DEBUG_DUMP_TX(DUMP_PREFIX_NONE,
				  16,
				  1,
				  tx_cmd_skb->data,
				  tx_cmd_skb->len,
				  1);

#ifdef TX_CMD_SYNC_WITH_OTHER_CMD
		rpu_send((void *)tx_cmd_skb, priv);
#else
		spin_lock_bh(&cmd_info.control_path_lock);
		hal_ops.send((void *)tx_cmd_skb,
			NULL,
			0);
		spin_unlock_bh(&cmd_info.control_path_lock);
#endif

		/* increment tx_cmd_send_count to keep track of number of
		 * tx_cmd send
		 */
		if (queue != WLAN_AC_BCN) {
			if (skb_queue_len(txq) == 1)
				priv->stats->tx_cmd_send_count_single++;
			else if (skb_queue_len(txq) > 1)
				priv->stats->tx_cmd_send_count_multi++;
		} else
			priv->stats->tx_cmd_send_count_beaconq++;

	spin_unlock_bh(&priv->tx.lock);
	rcu_read_unlock();

	return 0;
}


int rpu_prog_vif_short_slot(int index,
				    unsigned char *vif_addr,
				    unsigned int use_short_slot)
{
	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = SHORTSLOT_CHANGED;
	vif_cfg.use_short_slot = use_short_slot;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);
}


int rpu_prog_vif_atim_window(int index,
				     unsigned char *vif_addr,
				     unsigned int atim_window)
{
	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = ATIMWINDOW_CHANGED;
	vif_cfg.atim_window = atim_window;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);
}


int rpu_prog_long_retry(int index,
				unsigned char *vif_addr,
				unsigned int long_retry)
{
	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = LONGRETRY_CHANGED;
	vif_cfg.long_retry = long_retry;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);

}


int rpu_prog_short_retry(int index,
				 unsigned char *vif_addr,
				 unsigned int short_retry)
{

	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = SHORTRETRY_CHANGED;
	vif_cfg.short_retry = short_retry;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);


}

extern void cancel_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
int rpu_prog_vif_basic_rates(int index,
				     unsigned char *vif_addr,
				     unsigned int basic_rate_set)
{
	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = BASICRATES_CHANGED;
	vif_cfg.basic_rate_set = basic_rate_set;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);


}


int rpu_prog_vif_aid(int index,
			     unsigned char *vif_addr,
			     unsigned int aid)
{
	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = AID_CHANGED;
	vif_cfg.aid = aid;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);
}


int rpu_prog_vif_op_channel(int index,
				    unsigned char *vif_addr,
				    unsigned char op_channel)
{

	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = OP_CHAN_CHANGED;
	vif_cfg.op_channel = op_channel;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);
}


int rpu_prog_vif_conn_state(int index,
				       unsigned char *vif_addr,
				       unsigned int connect_state)
{

	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = CONNECT_STATE_CHANGED;
	vif_cfg.connect_state = connect_state;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);
	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);
}


int rpu_prog_vif_assoc_cap(int index,
				   unsigned char *vif_addr,
				   unsigned int caps)
{
	struct cmd_vif_cfg vif_cfg;


	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = CAPABILITY_CHANGED;
	vif_cfg.capability = caps;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);

}


int rpu_prog_vif_beacon_int(int index,
				    unsigned char *vif_addr,
				    unsigned int bcn_int)
{
	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));

	vif_cfg.changed_bitmap = BCN_INT_CHANGED;
	vif_cfg.beacon_interval = bcn_int;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);
}


int rpu_prog_vif_dtim_period(int index,
				     unsigned char *vif_addr,
				     unsigned int dtim_period)
{
	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));

	if (dtim_period < wifi->params.min_dtim_peroid)
		dtim_period = wifi->params.min_dtim_peroid;

	vif_cfg.changed_bitmap = DTIM_PERIOD_CHANGED;
	vif_cfg.dtim_period = dtim_period;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	RPU_INFO_IF("dtim_period = %d\n", dtim_period);
	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);
}


int rpu_prog_vif_bssid(int index,
			       unsigned char *vif_addr,
			       unsigned char *bssid)
{
	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = BSSID_CHANGED;
	img_ether_addr_copy(vif_cfg.bssid, bssid);
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);
	vif_cfg.if_index = index;

	RPU_DEBUG_IF("BSSID MAC ADDR: %pM\n", vif_cfg.bssid);
	RPU_DEBUG_IF("VIF MAC ADDR: %pM\n", vif_cfg.vif_addr);
	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);
}


int rpu_prog_vif_smps(int index,
			      unsigned char *vif_addr,
			      unsigned char smps_mode)
{
	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = SMPS_CHANGED;
	vif_cfg.if_index = index;
	img_ether_addr_copy(vif_cfg.vif_addr, vif_addr);

	switch (smps_mode) {
	case IEEE80211_SMPS_STATIC:
		vif_cfg.smps_info |= SMPS_ENABLED;
		break;
	case IEEE80211_SMPS_DYNAMIC:
		vif_cfg.smps_info |= SMPS_ENABLED;
		vif_cfg.smps_info |= SMPS_MODE;
		break;
	case IEEE80211_SMPS_AUTOMATIC:/* will be one of the above*/
	case IEEE80211_SMPS_OFF:
		break;
	default:
		WARN(1, "Invalid SMPS Mode: %d\n", smps_mode);
	}

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);
}

int rpu_prog_txq_params(int index,
				unsigned char *addr,
				unsigned int queue,
				unsigned int aifs,
				unsigned int txop,
				unsigned int cwmin,
				unsigned int cwmax,
				unsigned int uapsd)
{
	struct cmd_txq_params params;

	memset(&params, 0, (sizeof(struct cmd_txq_params)));

	params.if_index = index;
	img_ether_addr_copy(params.vif_addr, addr);
	params.queue_num = queue;
	params.aifsn = aifs;
	params.txop = txop;
	params.cwmin = cwmin;
	params.cwmax = cwmax;
	params.uapsd = uapsd;
	RPU_DEBUG_IF("%s: queue=%d, aifs=%d, txop=%d, cwmin=%d, cwmax=%d, uapsd=%d\n",
							__func__, queue, aifs, txop, cwmin, cwmax, uapsd);

	return rpu_send_cmd((unsigned char *) &params,
				    sizeof(struct cmd_txq_params),
				    RPU_CMD_TXQ_PARAMS);
}

int rpu_prog_rcv_bcn_mode(unsigned int bcn_rcv_mode)
{
	struct cmd_vif_cfg vif_cfg;

	memset(&vif_cfg, 0, sizeof(struct cmd_vif_cfg));
	vif_cfg.changed_bitmap = RCV_BCN_MODE_CHANGED;
	vif_cfg.bcn_mode = bcn_rcv_mode;

	return rpu_send_cmd((unsigned char *)&vif_cfg,
				    sizeof(struct cmd_vif_cfg),
				    RPU_CMD_VIF_CFG);

}


int rpu_prog_cont_tx(int val)
{
	struct cmd_cont_tx status;

	memset(&status, 0, sizeof(struct cmd_cont_tx));
	status.op = val;

	return rpu_send_cmd((unsigned char *)&status,
				    sizeof(struct cmd_cont_tx),
				    RPU_CMD_CONT_TX);
}



int rpu_prog_mib_stats(void)
{
	struct host_rpu_msg_hdr mib_stats_cmd;

	RPU_DEBUG_IF("cmd mib stats\n");
	memset(&mib_stats_cmd, 0, sizeof(struct host_rpu_msg_hdr));

	return rpu_send_cmd_without_delay((unsigned char *)&mib_stats_cmd,
				    sizeof(struct host_rpu_msg_hdr),
				    RPU_CMD_MIB_STATS);
}



#ifdef CONFIG_PM
int rpu_prog_econ_ps_state(int if_index,
				   unsigned int ps_state)
{
	struct cmd_ps ps_cfg;

	memset(&ps_cfg, 0, sizeof(struct cmd_ps));
	ps_cfg.mode = ps_state;
	ps_cfg.if_index = if_index;

	return rpu_send_cmd((unsigned char *)&ps_cfg,
				    sizeof(struct cmd_ps),
				    RPU_CMD_PS_ECON_CFG);
}
#endif


int rpu_prog_pwrmgmt_pwr_on_value(unsigned int *pwr_on_values, unsigned int size) {
	struct cmd_cfg_pwrmgmt pwrmgmt;
	
	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_PWR_ON_VALUE_CHANGED;
	memcpy(pwrmgmt.pwr_on_value, pwr_on_values, size);

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_pwrmgmt_pwr_off_value(unsigned int *pwr_off_values, unsigned int size) {
	struct cmd_cfg_pwrmgmt pwrmgmt;
	
	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_PWR_OFF_VALUE_CHANGED;
	memcpy(pwrmgmt.pwr_off_value, pwr_off_values, size);

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_pwrmgmt_ram_on_state(unsigned int *ram_on_states, unsigned int size) {
	struct cmd_cfg_pwrmgmt pwrmgmt;
	
	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_RAM_ON_STATE_CHANGED;
	memcpy(pwrmgmt.ram_on_state, ram_on_states, size);

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_pwrmgmt_ram_off_state(unsigned int *ram_off_states, unsigned int size) {
	struct cmd_cfg_pwrmgmt pwrmgmt;
	
	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_RAM_OFF_STATE_CHANGED;
	memcpy(pwrmgmt.ram_off_state, ram_off_states, size);

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_pwrmgmt_pwr_on_time(unsigned int *pwr_on_times, unsigned int size) {
	struct cmd_cfg_pwrmgmt pwrmgmt;
	
	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_PWR_ON_TIME_CHANGED;
	memcpy(pwrmgmt.pwr_on_time, pwr_on_times, size);

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_pwrmgmt_pwr_off_time(unsigned int *pwr_off_times, unsigned int size) {
	struct cmd_cfg_pwrmgmt pwrmgmt;
	
	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_PWR_OFF_TIME_CHANGED;
	memcpy(pwrmgmt.pwr_off_time, pwr_off_times, size);

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_pwrmgmt_ram_on_time(unsigned int *ram_on_times, unsigned int size) {
	struct cmd_cfg_pwrmgmt pwrmgmt;
	
	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_RAM_ON_TIME_CHANGED;
	memcpy(pwrmgmt.ram_on_time, ram_on_times, size);

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_pwrmgmt_ram_off_time(unsigned int *ram_off_times, unsigned int size) {
	struct cmd_cfg_pwrmgmt pwrmgmt;
	
	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_RAM_OFF_TIME_CHANGED;
	memcpy(pwrmgmt.ram_off_time, ram_off_times, size);

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_pwrmgmt_sleep_freq(unsigned int sleep_freq) {
	struct cmd_cfg_pwrmgmt pwrmgmt;
	
	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_SLEEP_FREQ_CHANGED;
	pwrmgmt.sleep_timer_freq_hz = sleep_freq;

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_pwrmgmt_clk_adj(unsigned int clk_adj_val) {
	struct cmd_cfg_pwrmgmt pwrmgmt;

	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_CLK_ADJ_VAL_CHANGED;
	pwrmgmt.clk_adj_val = clk_adj_val;

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_pwrmgmt_wakeup_time(unsigned int wakeup_time) {
	struct cmd_cfg_pwrmgmt pwrmgmt;

	memset(&pwrmgmt, 0, sizeof(struct cmd_cfg_pwrmgmt));

	pwrmgmt.sleep_config_changed = PMFLAG_WAKEUP_TIME_CHANGED;
	pwrmgmt.wakeup_time = wakeup_time;

	return rpu_send_cmd((unsigned char *) &pwrmgmt,
				    sizeof(struct cmd_cfg_pwrmgmt),
				    RPU_CMD_CFG_PWRMGMT);
}

int rpu_prog_read_csr(unsigned int addr) {
	struct cmd_read_csr readcsr;

	memset(&readcsr, 0, sizeof(struct cmd_read_csr));

	readcsr.addr = addr;

	return rpu_send_cmd((unsigned char *) &readcsr,
				    sizeof(struct cmd_read_csr),
				    RPU_CMD_READ_CSR);
}

static inline void rpu_process_pending_cmd(struct img_priv *priv, int lock)
{
	struct sk_buff *pending_cmd;
	struct host_rpu_msg_hdr *cmd_hdr;
	char cmd_str[64];

	if (lock)
		spin_lock_bh(&cmd_info.control_path_lock);

	if (cmd_info.outstanding_ctrl_req == 0) {
		RPU_DEBUG_IF("RPUIF: Unexpected outstanding_ctrl_req. Ignoring and continuing.\n");
	} else {
		cmd_info.outstanding_ctrl_req--;
		priv->stats->outstanding_cmd_cnt = cmd_info.outstanding_ctrl_req;

		if (block_rpu_comm) {
			if (lock)
				spin_unlock_bh(&cmd_info.control_path_lock);
			return;
		}

		RPU_DEBUG_IF("After DEC: outstanding cmd: %d\n",
			     cmd_info.outstanding_ctrl_req);
		pending_cmd = skb_dequeue(&cmd_info.outstanding_cmd);

		//if (unlikely(pending_cmd != NULL)) {
		if (likely(pending_cmd != NULL)) {
			cmd_hdr = (struct host_rpu_msg_hdr *)pending_cmd->data;
			convert_cmd_to_str(cmd_hdr->id, cmd_str);
			RPU_DEBUG_IF("Send 1 outstanding cmd(%s)\n", cmd_str);
			hal_ops.send((void *)pending_cmd, NULL, 0);
			priv->stats->gen_cmd_send_count++;
		}
	}

	if (lock)
		spin_unlock_bh(&cmd_info.control_path_lock);
}

extern int rk915_download_firmware_patch_only(struct hal_priv *priv);

void rpu_process_pending_operates(void)
{
	struct img_priv *priv = wifi->hw->priv;

	cmd_info.outstanding_ctrl_req = skb_queue_len(&cmd_info.outstanding_cmd);
	RPU_INFO_IF("%s: outstanding_ctrl_req %d\n",
					__func__, cmd_info.outstanding_ctrl_req);

	// clear pending operates, like reset cmd, channel prog
	if (!cmd_info.outstanding_ctrl_req && !priv->reset_complete)
		priv->reset_complete = 1;
	if (!priv->scan_abort_done)
		priv->scan_abort_done = 1;
	if (!priv->cancel_hw_roc_done)
		priv->cancel_hw_roc_done = 1;
	if (!priv->chan_prog_done)
		priv->chan_prog_done = 1;
	// clear pending scan
	if (wifi->params.hw_scan_status == HW_SCAN_STATUS_PROGRESS)
		cancel_hw_scan(priv->hw, NULL);
	// clear pending roc
	if (priv->roc_params.roc_in_progress == 0 &&
		priv->roc_params.roc_starting == 1) {
		ieee80211_ready_on_channel(priv->hw);
        	priv->roc_params.roc_in_progress = 1;
	}
	if (priv->roc_params.roc_in_progress == 1 &&
		priv->roc_params.roc_starting == 1) {
		//priv->roc_params.roc_starting = 0;
		ieee80211_queue_delayed_work(priv->hw,
					&priv->roc_complete_work,
					0);
	}
	// clear pending econ ps
	if (priv->econ_ps_cfg_stats.processing) {
		priv->econ_ps_cfg_stats.completed = 1;
		priv->econ_ps_cfg_stats.processing = 0;
		priv->econ_ps_cfg_stats.result = 0;
		rx_interrupt_status = 0;
	}
	// because outstanding_ctrl_req will mismatch after lpw died recovery, reset here.
	rpu_process_pending_cmd(priv, 1);
}

int rpu_msg_handler(void *nbuff)
{
	unsigned int event;
	unsigned char *buff;
	struct host_rpu_msg_hdr *hdr;
	//struct host_rpu_msg_hdr *cmd_hdr;
	struct rpu_if_data *p;
	struct sk_buff *skb = (struct sk_buff *)nbuff;
	//struct sk_buff *pending_cmd;
	struct img_priv *priv;
	char event_str[64];
	//char cmd_str[64];

	rcu_read_lock();

	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));

	if (!p) {
		WARN_ON(1);
		dev_kfree_skb_any(skb);
		rcu_read_unlock();
		return 0;
	}

	buff = skb->data;
	hdr = (struct host_rpu_msg_hdr *)buff;

	event = hdr->id & 0xffff;

	priv = (struct img_priv *)p->context;

	convert_event_to_str(event, event_str);
	RPU_DEBUG_IF("%s-RPUIF: event %d(%s) received\n", p->name, event, event_str);
	if (event == RPU_EVENT_RESET_COMPLETE) {
		struct host_event_reset_complete *r =
				(struct host_event_reset_complete *)buff;

		RPU_DEBUG_IF("rpu_reset_complete\n");

		block_rpu_comm = false;
		rpu_reset_complete(r->version, p->context);
		rpu_process_pending_cmd(priv, 1);
	} else if (event == RPU_EVENT_SCAN_ABORT_COMPLETE) {
		priv->scan_abort_done = 1;
#if 0//def HW_SCAN_TIMEOUT_ABORT
		if (wifi->params.hw_scan_status == HW_SCAN_STATUS_PROGRESS)
			cancel_hw_scan(priv->hw, NULL);
#endif
#ifdef CONFIG_PM
	} else if (event == RPU_EVENT_PS_ECON_CFG_DONE) {
		struct umac_event_ps_econ_cfg_complete *econ_cfg_complete_data =
				(struct umac_event_ps_econ_cfg_complete *)buff;
		priv->econ_ps_cfg_stats.completed = 1;
		priv->econ_ps_cfg_stats.processing = 0;
		priv->econ_ps_cfg_stats.result = econ_cfg_complete_data->status;
		rx_interrupt_status = 0;
	} else if (event == RPU_EVENT_PS_ECON_WAKE) {
		struct umac_event_ps_econ_wake *econ_wake_data =
					(struct umac_event_ps_econ_wake *)buff;
		priv->econ_ps_cfg_stats.wake_trig = econ_wake_data->trigger;
#endif
	} else if (event == RPU_EVENT_SCAN_COMPLETE) {
		rpu_scan_complete(p->context,
			(struct host_event_scanres *) buff,
			buff +  sizeof(struct host_event_scanres), skb->len);
	} else if (event == RPU_EVENT_RX) {
		if (priv->params->production_test) {
			rpu_proc_rx_event((void*)skb, p->context);
		} else {
			rpu_rx_frame(skb, p->context);
		}
	} else if (event == RPU_EVENT_TX_DONE) {
		if (priv->params->production_test/* &&
		    priv->params->start_prod_mode*/)
			rpu_proc_tx_complete((void *)buff,
						     p->context);
		else {
			/* Increment tx_done_recv_count to keep track of number
			 * of tx_done received do not count tx dones from host.
			 */
			priv->stats->tx_done_recv_count++;

			rpu_tx_complete((void *)buff,
						p->context);
#ifdef TX_CMD_SYNC_WITH_OTHER_CMD		
			rpu_process_pending_cmd(priv, 1);
#endif
		}
		cmd_info.tx_done_recv_count++;
	} else if (event == RPU_EVENT_DISCONNECTED) {
		struct host_event_disconnect *dis =
			(struct host_event_disconnect *)buff;
		struct img_priv *priv = (struct img_priv *)p->context;
		struct ieee80211_vif *vif = NULL;
		int i = 0;

		if (dis->reason_code == REASON_NW_LOST) {
			RPU_INFO_IF("connection lost\n");
			if (!wake_lock_active(&hpriv->fw_err_lock))
				wake_lock_timeout(&hpriv->fw_err_lock, msecs_to_jiffies(3*1000));
			for (i = 0; i < MAX_VIFS; i++) {
				if (!(priv->active_vifs & (1 << i)))
					continue;

				vif = rcu_dereference(priv->vifs[i]);

				if (ether_addr_equal(vif->addr,
						     dis->mac_addr)) {
					ieee80211_connection_loss(vif);
					break;
				}
			}
		}
	} else if (event == RPU_EVENT_MAC_STATS) {
		struct umac_event_mac_stats  *mac_stats =
			(struct umac_event_mac_stats *) buff;

		rpu_mac_stats(mac_stats, p->context);
	} else if (event == RPU_EVENT_NOA) {
		rpu_noa_event(FROM_EVENT_NOA, (void *)buff,
				      p->context, NULL);

	} else if (event == RPU_EVENT_COMMAND_PROC_DONE) {
		/*struct host_event_command_complete *cmd =
		 * (struct host_event_command_complete*)buff;
		 */
		RPU_DEBUG_IF("Received  PROC_DONE\n");
		rpu_process_pending_cmd(priv, 1);
	} else if (event == RPU_EVENT_CH_PROG_DONE) {
		rpu_ch_prog_complete(event,
			(struct umac_event_ch_prog_complete *)buff, p->context);
		//rpu_process_pending_cmd(priv, 1);
	} else if (event == RPU_EVENT_ROC_STATUS) {
		struct umac_event_roc_status *roc_status = (void *)buff;
		struct delayed_work *work = NULL;

		RPU_DEBUG_ROC("%s:%d ROC status is %d\n",
			__func__, __LINE__, roc_status->roc_status);

		switch (roc_status->roc_status) {
		case UMAC_ROC_STAT_STARTED:
			if (priv->roc_params.roc_in_progress == 0) {
				priv->roc_params.roc_in_progress = 1;
				ieee80211_ready_on_channel(priv->hw);
				RPU_DEBUG_ROC("%s-RPUIF: ROC READY..\n",
					  priv->name);
			}
			break;
		case UMAC_ROC_STAT_DONE:
		case UMAC_ROC_STAT_STOPPED:
			if (priv->roc_params.roc_in_progress == 1) {
				work = &priv->roc_complete_work;
				ieee80211_queue_delayed_work(priv->hw,
							     work,
							     0);
			}
			break;
		}
	} else if (event == RPU_EVENT_FW_ERROR) {
#if defined(LPW_RECOVERY_FROM_RPU)
		unsigned char *err_str;
		int ret = 0;

		err_str = buff + sizeof(struct host_rpu_msg_hdr);
		RPU_ERROR_IF("%s: FW is in Error State (%s)\n", __func__, err_str);
		if (strstr(err_str, "beacon tx without done")) {
			rk915_signal_io_error(FW_ERR_LPW_RECOVERY);
		} else {
#ifdef TXRX_DATA_LOCK
			mutex_lock(&hpriv->txrx_mutex);
#endif
			ret = rk915_download_firmware_patch_only(hpriv);
#ifdef TXRX_DATA_LOCK
			mutex_unlock(&hpriv->txrx_mutex);
#endif
        }
		//if (!ret)
			rpu_process_pending_operates();
#else //defined(LPW_RECOVERY_FROM_RPU)
		RPU_ERROR_IF("%s: FW is in Error State, reboot wifi.\n", __func__);
#ifdef CONFIG_WIRELESS_EXT
		iw_send_hang_event(priv);
#endif
#endif //defined(LPW_RECOVERY_FROM_RPU)
		hpriv->lpw_error_counter++;
	} else if (event == RPU_EVENT_BLOCK_ALL) {
		RPU_INFO_IF("RPU_EVENT_BLOCK_ALL\n");
		block_rpu_comm = true;
	} else if (event == RPU_EVENT_UNBLOCK_ALL) {
		RPU_INFO_IF("RPU_EVENT_UNBLOCK_ALL\n");
		if (block_rpu_comm) {
			block_rpu_comm = false;
			rpu_process_pending_cmd(priv, 1);

			rpu_unblock_all_frames(priv, 0);
		}
	} else if (event == RPU_EVENT_READ_CSR_CMP) {
		struct umac_event_read_csr_complete *csr_cmp =
			(struct umac_event_read_csr_complete *)buff;
		priv->read_csr_value = csr_cmp->value;
		priv->read_csr_complete = 1;
	} else if (event == RPU_EVENT_FW_PRIV_CMD_DONE) {
		struct fw_priv_cmd_done *info =
				(struct fw_priv_cmd_done *)buff;
		rpu_fw_priv_cmd_done(info, p->context);
#ifdef SDIO_TXRX_STABILITY_TEST		
	} else if (event == RPU_EVENT_TXRX_TEST) {
		struct host_rpu_msg_hdr *hdr = (struct host_rpu_msg_hdr *)buff;
		rpu_txrx_test_receive(hdr);
#endif
	} else {
		RPU_ERROR_IF("%s: Unknown event received %d\n", __func__, event);
	}

	if (event != RPU_EVENT_RX)
		dev_kfree_skb_any(skb);

	rcu_read_unlock();

	return 0;
}


int rpu_if_init(void *context, const char *name)
{
	struct rpu_if_data *p;

	RPU_DEBUG_IF("%s-RPUIF: rpu_if init called\n", name);

	p = kzalloc(sizeof(struct rpu_if_data), GFP_KERNEL);

	if (!p) {
		WARN_ON(1);
		return -ENOMEM;
	}

	p->name = (char *)name;
	p->context = context;
	hal_ops.register_callback(rpu_msg_handler);
	rcu_assign_pointer(rpu_if, p);
	skb_queue_head_init(&cmd_info.outstanding_cmd);
	spin_lock_init(&cmd_info.control_path_lock);
	cmd_info.outstanding_ctrl_req = 0;

	return 0;
}


void rpu_if_deinit(void)
{
	struct rpu_if_data *p;

	RPU_DEBUG_IF("%s-RPUIF: Deinit called\n", rpu_if->name);

	p = rcu_dereference(rpu_if);
	rcu_assign_pointer(rpu_if, NULL);
	synchronize_rcu();
	kfree(p);
}


void rpu_if_free_outstnding(void)
{

	struct sk_buff *skb;

	spin_lock_bh(&cmd_info.control_path_lock);

	/* First free the outstanding commands, we are not sending
	 * anymore commands to the FW except RESET.
	 */
	while ((skb = __skb_dequeue(&cmd_info.outstanding_cmd))) {
		struct host_rpu_msg_hdr *hdr = (struct host_rpu_msg_hdr *)skb->data;
		char cmd_str[64];

		convert_cmd_to_str(hdr->id, cmd_str);
		RPU_DEBUG_IF("%s: free outstanding command %s(%d)\n", __func__, cmd_str, hdr->id);
		dev_kfree_skb_any(skb);
	}

	cmd_info.outstanding_ctrl_req = 0;

	spin_unlock_bh(&cmd_info.control_path_lock);
}

#ifdef ENABLE_DAPT
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
void dapt_timer_expiry(struct timer_list *t)
#else
void dapt_timer_expiry(unsigned long data)
#endif
{
	struct rpu_if_data *p;
	struct img_priv *priv;

	RPU_DEBUG_DAPT("%s\n", __func__);

	rcu_read_lock();

	p = (struct rpu_if_data *)(rcu_dereference(rpu_if));
	if (!p) {
		WARN_ON(1);
		rcu_read_unlock();
		return;
	}

	priv = (struct img_priv *)p->context;
	if (priv)
		dapt_timer_handler(priv);

	rcu_read_unlock();
}
#endif

