/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/device.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/hex.h>
#include <linux/of_net.h>
#include <linux/udp.h>
#include <linux/version.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>

#include <net/cfg80211.h>
#include <net/mac80211.h>
#include <../net/mac80211/ieee80211_i.h>

#include "core.h"
#include "p2p.h"
#include "utils.h"
//#include "fwldr.h"
#include "hal_io.h"
#include "if_io.h"

#define UMAC_IF_TAG "UMAC_IF"

/* Its value will be the default mac address and it can only be updated with the
 * command line arguments
 */
unsigned int ht_support = 1;
module_param(ht_support, int, 0);
MODULE_PARM_DESC(ht_support, "Configure the 11n support for this device");

unsigned int ftm;
module_param(ftm, int, 0);
MODULE_PARM_DESC(ftm, "Factory Test Mode, should be used only for calibrations.");

unsigned int down_fw_in_probe = 0;
module_param(down_fw_in_probe, int, 0);
MODULE_PARM_DESC(down_fw_in_probe, "Downlaod firmware in driver probe or not");

unsigned int system_rev = 0x494D47;

static unsigned int g_cipher_type;

int rpu_debug =
	RPU_DEBUG_SCAN			|
	RPU_DEBUG_ROC			|
	RPU_DEBUG_TX			|
	RPU_DEBUG_MAIN			|
	RPU_DEBUG_IF			|
	RPU_DEBUG_UMACIF		|
	RPU_DEBUG_RX			|
	RPU_DEBUG_HAL			|
	RPU_DEBUG_CRYPTO		|
	//RPU_DEBUG_DUMP_RX		|
	//RPU_DEBUG_DUMP_HAL		|
	RPU_DEBUG_TSMC			|
	RPU_DEBUG_P2P			|
	RPU_DEBUG_VIF			|
	//RPU_DEBUG_DUMP_TX		|
	RPU_DEBUG_SDIO			|
	RPU_DEBUG_HALIO			|
	RPU_DEBUG_DAPT			|
	RPU_DEBUG_ROCOVERY		|
	RPU_DEBUG_FIRMWARE;

int rpu_debug_level =
	RPU_DEBUG_LEVEL_ERROR	|
	RPU_DEBUG_LEVEL_INFO/*	|
	RPU_DEBUG_LEVEL_DEBUG*/;

module_param(rpu_debug, uint, 0);
MODULE_PARM_DESC(rpu_debug, " rpu_debug: Configure Debugging Mask");
int uccp_reinit;

int load_fw(struct ieee80211_hw *hw);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
#undef IEEE80211_BAND_2GHZ
#define IEEE80211_BAND_2GHZ NL80211_BAND_2GHZ
#endif

#define CHAN2G(_freq, _idx)  {		\
	.band = IEEE80211_BAND_2GHZ,	\
	.center_freq = (_freq),		\
	.hw_value = (_idx),		\
	.max_power = 20,		\
}

#define CHAN5G(_freq, _idx, _flags) {	\
	.band = IEEE80211_BAND_5GHZ,	\
	.center_freq = (_freq),		\
	.hw_value = (_idx),		\
	.max_power = 20,		\
	.flags = (_flags),		\
}

static struct ieee80211_channel dsss_chantable[] = {
	CHAN2G(2412, 0),  /* Channel 1 */
	CHAN2G(2417, 1),  /* Channel 2 */
	CHAN2G(2422, 2),  /* Channel 3 */
	CHAN2G(2427, 3),  /* Channel 4 */
	CHAN2G(2432, 4),  /* Channel 5 */
	CHAN2G(2437, 5),  /* Channel 6 */
	CHAN2G(2442, 6),  /* Channel 7 */
	CHAN2G(2447, 7),  /* Channel 8 */
	CHAN2G(2452, 8),  /* Channel 9 */
	CHAN2G(2457, 9),  /* Channel 10 */
	CHAN2G(2462, 10), /* Channel 11 */
	CHAN2G(2467, 11), /* Channel 12 */
	CHAN2G(2472, 12), /* Channel 13 */
	CHAN2G(2484, 13), /* Channel 14 */
};


static struct ieee80211_rate dsss_rates[] = {
	{ .bitrate = 10, .hw_value = 2},
	{ .bitrate = 20, .hw_value = 4,
	.flags = IEEE80211_RATE_SHORT_PREAMBLE},
	{ .bitrate = 55, .hw_value = 11,
	.flags = IEEE80211_RATE_SHORT_PREAMBLE},
	{ .bitrate = 110, .hw_value = 22,
	.flags = IEEE80211_RATE_SHORT_PREAMBLE},
	{ .bitrate = 60, .hw_value = 12},
	{ .bitrate = 90, .hw_value = 18},
	{ .bitrate = 120, .hw_value = 24},
	{ .bitrate = 180, .hw_value = 36},
	{ .bitrate = 240, .hw_value = 48},
	{ .bitrate = 360, .hw_value = 72},
	{ .bitrate = 480, .hw_value = 96},
	{ .bitrate = 540, .hw_value = 108}
};

static struct ieee80211_supported_band band_2ghz = {
	.channels = dsss_chantable,
	.n_channels = ARRAY_SIZE(dsss_chantable),
	.band = IEEE80211_BAND_2GHZ,
	.bitrates = dsss_rates,
	.n_bitrates = ARRAY_SIZE(dsss_rates),
};


/* Interface combinations for Virtual interfaces */
static const struct ieee80211_iface_limit if_limit1[] = {
		{ .max = 2, .types = BIT(NL80211_IFTYPE_STATION)}
};

static const struct ieee80211_iface_limit if_limit2[] = {
		{ .max = 1, .types = BIT(NL80211_IFTYPE_STATION)},
		{ .max = 1, .types = BIT(NL80211_IFTYPE_AP) |
				     BIT(NL80211_IFTYPE_P2P_CLIENT) |
				     BIT(NL80211_IFTYPE_ADHOC) |
				     BIT(NL80211_IFTYPE_P2P_GO)}
};

static const struct ieee80211_iface_limit if_limit3[] = {
		{ .max = 2, .types = BIT(NL80211_IFTYPE_P2P_CLIENT)}
};

static const struct ieee80211_iface_limit if_limit4[] = {
		{ .max = 1, .types = BIT(NL80211_IFTYPE_ADHOC)},
		{ .max = 1, .types = BIT(NL80211_IFTYPE_P2P_CLIENT)}
};


static const struct ieee80211_iface_limit if_limit6[] = {
		{ .max = 1, .types = BIT(NL80211_IFTYPE_AP)}
};


static const struct ieee80211_iface_combination if_comb[] = {
	{ .limits = if_limit1,
	  .n_limits = ARRAY_SIZE(if_limit1),
	  .max_interfaces = 2,
	  .num_different_channels = 1},
	{ .limits = if_limit2,
	  .n_limits = ARRAY_SIZE(if_limit2),
	  .max_interfaces = 2,
	  .num_different_channels = 1},
	{ .limits = if_limit3,
	  .n_limits = ARRAY_SIZE(if_limit3),
	  .max_interfaces = 2,
	  .num_different_channels = 1},
	{ .limits = if_limit4,
	  .n_limits = ARRAY_SIZE(if_limit4),
	  .max_interfaces = 2,
	  .num_different_channels = 1},
};

#ifdef WOWLAN_SUPPORT
static const struct wiphy_wowlan_support uccp_wowlan_support = {
	.flags = WIPHY_WOWLAN_ANY,
};
#endif

#ifdef DUMP_TX_RX_FRAME_INFO

#include <net/cfg80211.h>
#include <net/ip.h>
#include <linux/tcp.h>

static int ieee80211_crypt_hdrlen(u16 fc)
{
	int hdrlen = 0;

	if (ieee80211_has_protected(fc)) {
		switch (g_cipher_type) {
		case CIPHER_TYPE_WEP40:
		case CIPHER_TYPE_WEP104:
			hdrlen = 4;//WEP_IV_LEN;
			break;
		case CIPHER_TYPE_TKIP:
		case CIPHER_TYPE_CCMP:
			hdrlen = 8;
			break;
		}
	}
	return hdrlen;
}

static int ieee8022_ll_hdrlen(u8 *payload, u16 ethertype)
{
	if ((ether_addr_equal(payload, rfc1042_header) &&
		    ethertype != ETH_P_AARP && ethertype != ETH_P_IPX) ||
		   ether_addr_equal(payload, bridge_tunnel_header)) {
		return 8;
	} else {
		return 0;
	}
}

/*
 * 802.11 frame struct  (for example ICMP):
 *
 * | 802.11 header | crypt header | 802.2 LL header | IP header| ICMP |
 *
 *                                option              option
 */
static void dump_ip_info(u8 *data, int len, u8 *str)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)data;
	u16 fc = hdr->frame_control;

	if (ieee80211_is_data(fc) /*&&
		is_unicast_ether_addr(ieee80211_get_DA(hdr))*/) {
		int offset, ieee8022_hdrlen;
		u8 *ieee8022_payload;
		int ethertype, n;
		struct iphdr *ip;

		offset = ieee80211_hdrlen(fc); /* 802.11 header */
		offset += ieee80211_crypt_hdrlen(fc); /* crypt header */

		if (offset >= len) {
			return;
		}

		ieee8022_payload = data + offset; /* ieee802.2 ll header */
		ethertype = (ieee8022_payload[6] << 8) | ieee8022_payload[7];
		ieee8022_hdrlen =ieee8022_ll_hdrlen(ieee8022_payload, ethertype);

		n = sprintf(str, "ethertype %04x ", ethertype);
		str += n;
		if (ieee8022_hdrlen &&
			ethertype == ETH_P_IP/* &&
			ethertype == ETH_P_IPV6*/) {
			offset += ieee8022_hdrlen;
			ip = (struct iphdr *)(data + offset); /* IP header */
			n = sprintf(str, "protocol %03d %pI4 -> %pI4 ", ip->protocol, &(ip->saddr), &(ip->daddr));
			str += n;
			if (ip->protocol == IPPROTO_UDP) {
				struct udphdr *uh = (struct udphdr *)((u8 *)ip + ip->ihl * 4);

				sprintf(str, "UDP Port %d -> %d ", ntohs(uh->source), ntohs(uh->dest));
			} else if (ip->protocol == IPPROTO_TCP) {
				struct tcphdr *th = (struct tcphdr *)((u8 *)ip + ip->ihl * 4);

				sprintf(str, "TCP Port %d -> %d ", ntohs(th->source), ntohs(th->dest));
			}
		}
	}
}

enum p2p_action_frame_type {
        P2P_GO_NEG_REQ = 0,
        P2P_GO_NEG_RESP = 1,
        P2P_GO_NEG_CONF = 2,
        P2P_INVITATION_REQ = 3,
        P2P_INVITATION_RESP = 4,
        P2P_DEV_DISC_REQ = 5,
        P2P_DEV_DISC_RESP = 6,
        P2P_PROV_DISC_REQ = 7,
        P2P_PROV_DISC_RESP = 8
};

static inline u32 WPA_GET_BE32(const u8 *a)
{
        return ((u32) a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
}

static char *dump_p2p_action_type(struct ieee80211_hdr *hdr)
{
	u8 *payload;
	u8 category;

	payload = (u8 *)hdr + sizeof(struct ieee80211_hdr_3addr);
	category = *payload++;

#define WLAN_ACTION_PUBLIC 4
#define WLAN_PA_VENDOR_SPECIFIC 9
#define P2P_IE_VENDOR_TYPE 0x506f9a09

	if (category == WLAN_ACTION_PUBLIC) {
        switch (payload[0]) {
        case WLAN_PA_VENDOR_SPECIFIC:
            payload++;
            if (WPA_GET_BE32(payload) != P2P_IE_VENDOR_TYPE)
                    return "";

            payload += 4;
	        switch (payload[0]) {
	        case P2P_GO_NEG_REQ:
	                return "P2P_GO_NEG_REQ";
	        case P2P_GO_NEG_RESP:
	                return "P2P_GO_NEG_RESP";
	        case P2P_GO_NEG_CONF:
	                return "P2P_GO_NEG_CONF";
	        case P2P_INVITATION_REQ:
	                return "P2P_INVITATION_REQ";
	        case P2P_INVITATION_RESP:
	                return "P2P_INVITATION_RESP";
	        case P2P_PROV_DISC_REQ:
	                return "P2P_PROV_DISC_REQ";
	        case P2P_PROV_DISC_RESP:
	                return "P2P_PROV_DISC_RESP";
	        case P2P_DEV_DISC_REQ:
	                return "P2P_DEV_DISC_REQ";
	        case P2P_DEV_DISC_RESP:
	                return "P2P_DEV_DISC_RESP";
	        }
			break;
        }
	}
	return "";
}

void dump_ieee80211_hdr_info(unsigned char *data, int len, int tx)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)data;
	char direct_str[256];
	u8 *DA = ieee80211_get_DA(hdr);
	u8 *SA = ieee80211_get_SA(hdr);
	int n;

	if (wifi->params.hw_scan_status != HW_SCAN_STATUS_NONE)
		return;

	n = sprintf(direct_str, "%s len %04d %pM -> %pM SN %d ", tx?"tx":"rx", len, SA, DA, hdr->seq_ctrl>>4);
	dump_ip_info(data, len, &direct_str[n]);

	if (hdr != NULL) {
		//RPU_DEBUG_UMACIF("%s\n", __func__);
		if (ieee80211_is_mgmt(hdr->frame_control)) {
			if (ieee80211_is_assoc_req(hdr->frame_control)) {
				RPU_INFO_RX("%s assoc req\n", direct_str);
			} else if (ieee80211_is_assoc_resp(hdr->frame_control)) {
				RPU_INFO_RX("%s assoc resp\n", direct_str);
			} else if (ieee80211_is_reassoc_req(hdr->frame_control)) {
				RPU_INFO_RX("%s reassoc req\n", direct_str);
			} else if (ieee80211_is_reassoc_resp(hdr->frame_control)) {
				RPU_INFO_RX("%s reassoc resp\n", direct_str);
			} else if (ieee80211_is_probe_req(hdr->frame_control)) {
				RPU_INFO_RX("%s probe req\n", direct_str);
			} else if (ieee80211_is_probe_resp(hdr->frame_control)) {
				RPU_INFO_RX("%s probe resp\n", direct_str);
			} else if (ieee80211_is_beacon(hdr->frame_control)) {
				RPU_INFO_RX("%s beacon\n", direct_str);
			} else if (ieee80211_is_atim(hdr->frame_control)) {
				RPU_INFO_RX("%s atim\n", direct_str);
			} else if (ieee80211_is_disassoc(hdr->frame_control)) {
				RPU_INFO_RX("%s disassoc\n", direct_str);
			} else if (ieee80211_is_auth(hdr->frame_control)) {
				RPU_INFO_RX("%s auth\n", direct_str);
			} else if (ieee80211_is_deauth(hdr->frame_control)) {
				RPU_INFO_RX("%s deauth\n", direct_str);
			} else if (ieee80211_is_action(hdr->frame_control)) {
				RPU_INFO_RX("%s action %s\n", direct_str, dump_p2p_action_type(hdr));
			} else {
				RPU_INFO_RX("%s mgmt\n", direct_str);
			}
		} else if (ieee80211_is_ctl(hdr->frame_control)) {
			RPU_INFO_RX("%s ctl\n", direct_str);
		} else if (ieee80211_is_data(hdr->frame_control)) {
			RPU_INFO_RX("%s data\n", direct_str);
		} else {
			RPU_INFO_RX("%s unknow\n", direct_str);
		}
	}
}
#else
void dump_ieee80211_hdr_info(unsigned char *data, int len, int tx)
{
}
#endif

#ifdef DUMP_MORE_DEBUG_INFO
#define BSS_CHANGED_INFO_NUM 22
static char bss_changed_info_tbl[BSS_CHANGED_INFO_NUM][32] = {
	"BSS_CHANGED_ASSOC",
	"BSS_CHANGED_ERP_CTS_PROT",
	"BSS_CHANGED_ERP_PREAMBLE",
	"BSS_CHANGED_ERP_SLOT",
	"BSS_CHANGED_HT",
	"BSS_CHANGED_BASIC_RATES",
	"BSS_CHANGED_BEACON_INT",
	"BSS_CHANGED_BSSID",
	"BSS_CHANGED_BEACON",
	"BSS_CHANGED_BEACON_ENABLED",
	"BSS_CHANGED_CQM",
	"BSS_CHANGED_IBSS",
	"BSS_CHANGED_ARP_FILTER",
	"BSS_CHANGED_QOS",
	"BSS_CHANGED_IDLE",
	"BSS_CHANGED_SSID",
	"BSS_CHANGED_AP_PROBE_RESP",
	"BSS_CHANGED_PS",
	"BSS_CHANGED_TXPOWER",
	"BSS_CHANGED_P2P_PS",
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	"BSS_CHANGED_BEACON_INFO",
#else
	"BSS_CHANGED_DTIM_PERIOD",
#endif
	"BSS_CHANGED_BANDWIDTH"
};

#define CONF_CHANGED_INFO_NUM 9
static char conf_changed_info_tbl[CONF_CHANGED_INFO_NUM][40] = {
	" ",
	"IEEE80211_CONF_CHANGE_SMPS",
	"IEEE80211_CONF_CHANGE_LISTEN_INTERVAL",
	"IEEE80211_CONF_CHANGE_MONITOR",
	"IEEE80211_CONF_CHANGE_PS",
	"IEEE80211_CONF_CHANGE_POWER",
	"IEEE80211_CONF_CHANGE_CHANNEL",
	"IEEE80211_CONF_CHANGE_RETRY_LIMITS",
	"IEEE80211_CONF_CHANGE_IDLE"
};
static void dump_conf_changed_info(unsigned int changed)
{
	unsigned int i;
	char prt_str[256];

	memset(prt_str, 0, sizeof(prt_str));
	for (i = 0; i < CONF_CHANGED_INFO_NUM; i++) {
		if (changed & (1<<i))
			sprintf(prt_str + strlen(prt_str), "%s|", conf_changed_info_tbl[i]);
	}
	
	RPU_DEBUG_UMACIF("%s: changed = %08x (%s)\n", __func__, changed, prt_str);
}

static void dump_bss_changed_info(unsigned int changed)
{
	unsigned int i;
	char prt_str[756];

	memset(prt_str, 0, sizeof(prt_str));
	for (i = 0; i < BSS_CHANGED_INFO_NUM; i++) {
		if (changed & (1<<i))
			sprintf(prt_str + strlen(prt_str), "%s|", bss_changed_info_tbl[i]);
	}
	
	RPU_DEBUG_UMACIF("%s: changed = %08x (%s)\n", __func__, changed, prt_str);
}

#define AMPDU_ACTION_NUM 7
static char ampdu_action_tbl[AMPDU_ACTION_NUM][32] = {
	"AMPDU_RX_START",
	"AMPDU_RX_STOP",
	"AMPDU_TX_START",
	"AMPDU_TX_STOP_CONT",
	"AMPDU_TX_STOP_FLUSH",
	"AMPDU_TX_STOP_FLUSH_CONT",
	"AMPDU_TX_OPERATIONAL"
};

static void dump_ampdu_action_info(unsigned int action)
{
	RPU_DEBUG_UMACIF("%s: (%s)\n", __func__, ampdu_action_tbl[action]);
}
#else
static void dump_conf_changed_info(unsigned int changed)
{
}

static void dump_bss_changed_info(unsigned int changed)
{
}

static void dump_ampdu_action_info(unsigned int action)
{
}

#endif

void rk915_signal_io_error(int reason)
{
	if (hpriv->shutdown || hpriv->during_fw_download)
		return;
	hpriv->fw_error = 1;
	/*
	 * The rx and tx threads both signal io errors: claim the recovery
	 * atomically so only one runs (from PR #1 of the driver port).
	 */
	if (cmpxchg(&hpriv->fw_error_processing, 0, 1) == 0) {
		if (!wake_lock_active(&hpriv->fw_err_lock))
			wake_lock(&hpriv->fw_err_lock);

		hpriv->fw_error_counter++;
		hpriv->fw_error_reason = reason;

		RPU_ERROR_UMACIF("%s\n", __func__);
		RPU_ERROR_ROCOVERY("-------- fw error recovery (%d) start --------\n", reason);

		// trigger recovery work
		schedule_work(&hpriv->fw_err_work);
	}
}

/* only for wlan0 interface
 * param val:
 *   0, enter power save
 *   1, exit power save
 */
void trigger_wifi_power_save(int val)
{
	struct img_priv *priv = wifi->hw->priv;
	int if_index;

	if (priv->state != STARTED) {
		return;
	}

	if_index = find_main_iface(priv);
	priv->power_save = val;

	rpu_prog_ps_state(if_index,
				priv->vifs[if_index]->addr,
				val);
}

void cancel_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
void trigger_wifi_scan_abort(int if_idx)
{
	struct ieee80211_vif *vif;
	struct img_priv *priv = wifi->hw->priv;

	if (if_idx > 1)
		return;

	rcu_read_lock();

	vif = (struct ieee80211_vif *)rcu_dereference(priv->vifs[if_idx]);
	if (vif == NULL) {
		rcu_read_unlock();
		return;
	}

	cancel_hw_scan(wifi->hw, vif);

	rcu_read_unlock();
}

#ifdef ENABLE_KEEP_ALIVE
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
void keep_alive_expiry(struct timer_list *t)
#else
extern void keep_alive_expiry(unsigned long data);
#endif
static void init_keep_alive_timer (struct img_priv *priv)
{
	RPU_DEBUG_UMACIF("%s: %p\n", __func__, priv);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
	timer_setup(&priv->keep_alive_timer, keep_alive_expiry, 0);
#else
	init_timer(&priv->keep_alive_timer);
	priv->keep_alive_timer.data = (unsigned long)priv;
	priv->keep_alive_timer.function = keep_alive_expiry;
#endif
	priv->null_frame_seq_no = 0;
	priv->null_frame_sending = 0;
	priv->null_frame_send_count = 0;
}

static void start_keep_alive_timer(struct img_priv *priv, int index)
{
	if (is_wlan_connected(priv) && index == find_main_iface(priv)) {
		RPU_DEBUG_UMACIF("%s: %p\n", __func__, priv);
		mod_timer(&priv->keep_alive_timer, jiffies + SEND_NULL_FRAME_INTERVAL_SECONDS * HZ);
	}
}

static void deinit_keep_alive_timer (struct img_priv *priv)
{
	RPU_DEBUG_UMACIF("%s: %p\n", __func__, priv);
	timer_delete(&priv->keep_alive_timer);
}
#endif

static void tx(struct ieee80211_hw *hw,
	       struct ieee80211_tx_control *txctl,
	       struct sk_buff *skb)
{
	struct img_priv *priv = hw->priv;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);
	struct umac_vif *uvif;
	unsigned char null_bssid[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	struct iphdr *iphdr;
	unsigned char *pktgen_magic;
	unsigned int orig_pktgen_magic = 0x55e99bbe; /*Endianness 0xbe9be955*/
	struct umac_event_noa noa_event;

	if (tx_info->control.vif == NULL) {
		RPU_ERROR_UMACIF("%s: Dropping injected TX frame\n",
			 priv->name);
		dev_kfree_skb_any(skb);
		return;
	}

	if (hpriv->fw_error_processing) {
		dev_kfree_skb_any(skb);
		return;
	}

	uvif = (struct umac_vif *)(tx_info->control.vif->drv_priv);

#ifdef ENABLE_KEEP_ALIVE
	start_keep_alive_timer(priv, uvif->vif_index);
#endif

	RPU_DEBUG_UMACIF("%s: %s: %s\n", 
		VIF_INDEX_TO_INTERFACE_NAME(uvif->vif_index),
		UMAC_IF_TAG, __func__);

	if (wifi->params.production_test) {
		if (((hdr->frame_control &
		      IEEE80211_FCTL_FTYPE) != IEEE80211_FTYPE_DATA) ||
		    (tx_info->control.vif == NULL))
			goto tx_status;

		iphdr = (struct iphdr *) skb_network_header(skb);
		if (iphdr->protocol == IPPROTO_UDP) {
			pktgen_magic = skb_transport_header(skb);
			pktgen_magic += sizeof(struct udphdr);
			/*If not PKTGEN, then drop it*/
			if (memcmp(pktgen_magic, &orig_pktgen_magic, 4) != 0) {
				RPU_DEBUG_UMACIF("%s:%d Prod_Mode: The pkt ",
						   __func__, __LINE__);
				RPU_DEBUG_UMACIF("is NOT PKTGEN so ");
				RPU_DEBUG_UMACIF("dropping it\n");
				goto tx_status;
			}
		} else {
			RPU_DEBUG_UMACIF("%s:%d prod_mode: The pkt is NOT ",
					   __func__, __LINE__);
			RPU_DEBUG_UMACIF("PKTGEN so dropping it\n");
			goto tx_status;
		}
	}
	if (ether_addr_equal(hdr->addr3, null_bssid)) {
		RPU_INFO_UMACIF("%s: null bssid\n", __func__);
		goto tx_status;
	}

	if (uvif->vif->type != NL80211_IFTYPE_AP) {
		if ((priv->power_save == PWRSAVE_STATE_DOZE) &&
		    (!wifi->params.disable_power_save) &&
		    (((hdr->frame_control &
		      IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_DATA) ||
			 is_bufferable_mgmt_frame(hdr))) {
			hdr->frame_control |= IEEE80211_FCTL_PM;
		}
	}
#ifdef RPU_SLEEP_ENABLE
#ifdef PS_SLEEP_TEST
	hdr->frame_control |= IEEE80211_FCTL_PM;
#endif
#endif

	if (uvif->noa_active) {
		memset(&noa_event, 0, sizeof(noa_event));
		noa_event.if_index = uvif->vif_index;
		rpu_noa_event(FROM_TX, &noa_event, priv, skb);
		return;
	}


	rpu_tx_frame(skb,
			     txctl->sta,
			     priv,
			     false);

	return;

tx_status:
	tx_info->flags |= IEEE80211_TX_STAT_ACK;
	tx_info->status.rates[0].count = 1;
	ieee80211_tx_status_skb(hw, skb);
}

static int start(struct ieee80211_hw *hw)
{
	struct img_priv *priv = (struct img_priv *)hw->priv;
	int ret = 0;

	RPU_DEBUG_UMACIF("%s-80211IF: In start\n", priv->name);

	mutex_lock(&priv->mutex);

	hpriv->fw_error = 0;
	if (!down_fw_in_probe && !wifi->params.fw_loaded) {
		if (hpriv->fw_bring_up_func((void *)hpriv) != 0) {
			ret = -ENODEV;
			goto out;
		}
		wifi->params.fw_loaded = 1;
	}

	if ((rpu_core_init(priv, ftm)) < 0) {
		RPU_ERROR_UMACIF("%s-80211IF: umac init failed\n", priv->name);
		ret = -ENODEV;
		goto out;
	}

	INIT_DELAYED_WORK(&priv->roc_complete_work, rpu_roc_complete_work);

	priv->state = STARTED;

	/*
	 * mac80211 re-adds the interfaces after a hardware restart without
	 * removing them first, so reset the vif accounting here (from PR #1
	 * of the driver port). Otherwise the second restart fails with
	 * "Exceeded Maximum supported VIF's".
	 */
	priv->current_vif_count = 0;
	priv->active_vifs = 0;

	memset(priv->params->pdout_voltage, 0,
	       sizeof(char) * MAX_AUX_ADC_SAMPLES);

	priv->roc_params.roc_in_progress = 0;
	priv->roc_params.roc_starting = 0;
	priv->params->hw_scan_status = HW_SCAN_STATUS_NONE;

out:
	mutex_unlock(&priv->mutex);
	return ret;
}

void stop(struct ieee80211_hw *hw, bool flag)
{
	struct img_priv    *priv= (struct img_priv *)hw->priv;

	RPU_DEBUG_UMACIF("%s-80211IF:In stop\n", priv->name);

	mutex_lock(&priv->mutex);

	rpu_core_deinit(priv, ftm);
	priv->state = STOPPED;

	if (hpriv->fw_error && !down_fw_in_probe && wifi->params.fw_loaded) {
		hpriv->fw_tear_down_func((void *)hpriv);
		wifi->params.fw_loaded = 0;
	}

	mutex_unlock(&priv->mutex);

	hal_ops.reset_hal_params();

}

static int add_interface(struct ieee80211_hw *hw,
		struct ieee80211_vif *vif)
{
	struct img_priv    *priv= hw->priv;
	struct ieee80211_vif *v;
	struct umac_vif   *uvif;
	struct ieee80211_sub_if_data *sdata;
	int vif_index, iftype;

	/*if (priv->fw_error) {
		return 0;
	}*/

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	mutex_lock(&priv->mutex);
	iftype = vif->type;
	v = vif;
	vif->driver_flags |= IEEE80211_VIF_BEACON_FILTER;

	sdata = vif_to_sdata(vif);
	if (sdata) {
		if (is_main_iface(vif->addr)) {
			priv->net_dev = (void *)sdata->dev;
			priv->sdata = (void *)sdata;
		}
	}

	if (priv->current_vif_count == wifi->params.num_vifs) {
		RPU_ERROR_UMACIF("%s: Exceeded Maximum supported VIF's cur:%d max: %d.\n",
		       __func__,
		       priv->current_vif_count,
		       wifi->params.num_vifs);

		mutex_unlock(&priv->mutex);
		return -ENOTSUPP;
	}

	priv->iftype = iftype;
	if (!(iftype == NL80211_IFTYPE_STATION ||
	      iftype == NL80211_IFTYPE_ADHOC ||
	      iftype == NL80211_IFTYPE_AP)) {
		RPU_ERROR_UMACIF("Invalid Interface type\n");
		mutex_unlock(&priv->mutex);
		return -ENOTSUPP;
	}

	if (wifi->params.production_test) {
		if (priv->active_vifs || iftype != NL80211_IFTYPE_ADHOC) {
			mutex_unlock(&priv->mutex);
			return -EBUSY;
		}
	}

	for (vif_index = 0; vif_index < wifi->params.num_vifs; vif_index++) {
		if (!(priv->active_vifs & (1 << vif_index)))
			break;
	}

	/* This should never happen, we have taken care of this above */
	if (vif_index == wifi->params.num_vifs) {
		RPU_ERROR_UMACIF("%s: All VIF's are busy: %pM\n", __func__, vif->addr);
		mutex_unlock(&priv->mutex);
		return -EINVAL;
	}

	uvif = (struct umac_vif *)&v->drv_priv;
	uvif->vif_index = vif_index;
	uvif->vif = v;
	uvif->priv = priv;
	uvif->seq_no = 0;
	rpu_vif_add(uvif);
	priv->active_vifs |= (1 << vif_index);
	priv->current_vif_count++;

	if (iftype == NL80211_IFTYPE_ADHOC)
		priv->tx_last_beacon = 0;

	rcu_assign_pointer(priv->vifs[vif_index], v);
	synchronize_rcu();

#ifdef ENABLE_DAPT
	dapt_param_late_init(priv);
#endif

	mutex_unlock(&priv->mutex);

	return 0;
}

static void remove_interface(struct ieee80211_hw *hw,
		struct ieee80211_vif *vif)
{
	struct img_priv    *priv= hw->priv;
	struct ieee80211_vif *v;
	int vif_index;

#ifdef RK3036_DONGLE
	wait_for_scan_complete(priv);
#endif

	/*if (priv->fw_error) {
		return;
	}*/

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	mutex_lock(&priv->mutex);
	v = vif;
	vif_index = ((struct umac_vif *)&v->drv_priv)->vif_index;

	rpu_vif_remove((struct umac_vif *)&v->drv_priv);
	priv->active_vifs &= ~(1 << vif_index);
	rcu_assign_pointer(priv->vifs[vif_index], NULL);
	synchronize_rcu();
	wifi->params.sync[vif_index].status = 0;
	priv->current_vif_count--;
	mutex_unlock(&priv->mutex);

}

static int change_interface(struct ieee80211_hw *dev,
		struct ieee80211_vif *vif,
		enum nl80211_iftype new_type,
		bool p2p)
{
	int ret = 0;

	RPU_DEBUG_UMACIF("change_interface new: %d (%d), old: %d (%d)\n", new_type,
			p2p, vif->type, vif->p2p);

#ifdef RK3036_DONGLE
	if (new_type != vif->type /*|| vif->p2p != p2p*/) {
#else
	if (new_type != vif->type || vif->p2p != p2p) {
#endif
		remove_interface(dev, vif);
		vif->type = new_type;
		vif->p2p = p2p;
		ret = add_interface(dev, vif);
	}

	return ret;
}

static int config(struct ieee80211_hw *hw, int radio_idx,
		unsigned int changed)
{
	struct img_priv *priv = hw->priv;
	struct ieee80211_conf *conf = &hw->conf;
	unsigned int pri_chnl_num;
	unsigned int freq_band;
	unsigned int ch_width;
	unsigned int center_freq = 0;
	unsigned int center_freq1 = 0;
	unsigned int center_freq2 = 0;
	int i = 0;
	int err = 0;
	struct ieee80211_vif *vif = NULL;
	int ret = 0;

	/*if (priv->fw_error) {
		return 0;
	}*/

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	dump_conf_changed_info(changed);

	mutex_lock(&priv->mutex);

	if (changed & IEEE80211_CONF_CHANGE_POWER) {
		priv->txpower = conf->power_level;
		CALL_RPU(rpu_prog_txpower, priv->txpower);
	}

	/* Check for change in channel */
	if (changed & IEEE80211_CONF_CHANGE_CHANNEL) {
		center_freq = conf->chandef.chan->center_freq;
		center_freq1 = conf->chandef.center_freq1;
		center_freq2 = conf->chandef.center_freq2;
		freq_band = conf->chandef.chan->band;
		ch_width = conf->chandef.width;

		pri_chnl_num = ieee80211_frequency_to_channel(center_freq);
		RPU_DEBUG_UMACIF("%s-80211IF:Primary Channel is %d\n",
				   priv->name,
				   pri_chnl_num);
		priv->pri_chnl_num = pri_chnl_num;

		err = rpu_prog_channel(pri_chnl_num,
					       center_freq1, center_freq2,
					       ch_width,
					       freq_band);

		if (err) {
			RPU_ERROR_UMACIF("%s: rpu_prog_channel failed\n", __func__);
			mutex_unlock(&priv->mutex);
			return err;
		}
	}

	/* Check for change in Power save state */
	for (i = 0; i < MAX_VIFS; i++) {
		if (!(changed & IEEE80211_CONF_CHANGE_PS))
			break;

		if (!(priv->active_vifs & (1 << i)))
			continue;

		/* When ROC is in progress, do not mess with
		 * PS state
		 */
		if (priv->roc_params.roc_in_progress)
			continue;

		if (wifi->params.disable_power_save)
			continue;

		if (conf->flags & IEEE80211_CONF_PS)
			priv->power_save = PWRSAVE_STATE_DOZE;
		else
			priv->power_save = PWRSAVE_STATE_AWAKE;

		RPU_DEBUG_UMACIF("%s-80211IF:PS state of VIF", priv->name);
		RPU_DEBUG_UMACIF(" %d changed to %d\n", i, priv->power_save);

		rcu_read_lock();
		vif = rcu_dereference(priv->vifs[i]);
		rcu_read_unlock();

		rpu_prog_ps_state(i,
					  vif->addr,
					  priv->power_save);
	}

	/* TODO: Make this global config as it effects all VIF's */
	for (i = 0; i < MAX_VIFS; i++) {
		if (!(changed & IEEE80211_CONF_CHANGE_SMPS))
			break;

		if (wifi->params.production_test == 1)
			break;

		if (!(priv->active_vifs & (1 << i)))
			continue;

		RPU_DEBUG_UMACIF("%s-80211IF:MIMO PS state of VIF %d -> %d\n",
				   priv->name,
				   i,
				   conf->smps_mode);

		rcu_read_lock();
		vif = rcu_dereference(priv->vifs[i]);
		rcu_read_unlock();

		rpu_prog_vif_smps(i,
					  vif->addr,
					  conf->smps_mode);
	}

	/* Check for change in Retry Limits */
	if (changed & IEEE80211_CONF_CHANGE_RETRY_LIMITS) {
		RPU_DEBUG_UMACIF("%s-80211IF:Retry Limits changed",
				   priv->name);
		RPU_DEBUG_UMACIF(" to %d and %d\n",
				   conf->short_frame_max_tx_count,
				   conf->long_frame_max_tx_count);
	}

	for (i = 0; i < MAX_VIFS; i++) {
		if (!(changed & IEEE80211_CONF_CHANGE_RETRY_LIMITS))
			break;

		if (!(priv->active_vifs & (1 << i)))
			continue;

		rcu_read_lock();
		vif = rcu_dereference(priv->vifs[i]);
		rcu_read_unlock();

		rpu_prog_short_retry(i,
					     vif->addr,
					     conf->short_frame_max_tx_count);
		rpu_prog_long_retry(i,
					    vif->addr,
					    conf->long_frame_max_tx_count);
	}

	RPU_DEBUG_UMACIF("%s: %s exit\n", UMAC_IF_TAG, __func__);
prog_rpu_fail:
	mutex_unlock(&priv->mutex);
	return ret;
}


static u64 prepare_multicast(struct ieee80211_hw *hw,
			     struct netdev_hw_addr_list *mc_list)
{
	struct img_priv *priv = hw->priv;
	int i;
	struct netdev_hw_addr *ha;
	int mc_count = 0;
	int ret = 0;

	/*if (priv->fw_error) {
		return 0;
	}*/

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	if (priv->state != STARTED) {
		RPU_ERROR_UMACIF("%s: state != STARTED\n", __func__);
		return 0;
	}

	mc_count = netdev_hw_addr_list_count(mc_list);
	{
		if (mc_count > MCST_ADDR_LIMIT) {
			mc_count = 0;
			RPU_INFO_UMACIF("%s-80211IF:Disabling MCAST filter (cnt=%d)\n",
				priv->name, mc_count);
			goto out;
		}
	}
	RPU_DEBUG_UMACIF("%s-80211IF: Multicast filter count\n", priv->name);
	RPU_DEBUG_UMACIF("adding: %d removing: %d\n", mc_count,
			priv->mc_filter_count);

	if (priv->mc_filter_count > 0) {
		/* Remove all previous multicast addresses from the LMAC */
		for (i = 0; i < priv->mc_filter_count; i++)
			rpu_prog_mcast_addr_cfg(priv->mc_filters[i],
							WLAN_MCAST_ADDR_REM);
	}

	i = 0;

	netdev_hw_addr_list_for_each(ha, mc_list) {
		/* Prog the multicast address into the LMAC */
		CALL_RPU(rpu_prog_mcast_addr_cfg,
			  ha->addr,
			  WLAN_MCAST_ADDR_ADD);
		memcpy(priv->mc_filters[i], ha->addr, 6);
		i++;
	}

	priv->mc_filter_count = mc_count;
out:
	return mc_count;
prog_rpu_fail:
	return ret;
}


static void configure_filter(struct ieee80211_hw *hw,
		unsigned int changed_flags,
		unsigned int *new_flags,
		u64 mc_count)
{
	struct img_priv *priv = hw->priv;
	int ret = 0;

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	mutex_lock(&priv->mutex);

	changed_flags &= SUPPORTED_FILTERS;
	*new_flags &= SUPPORTED_FILTERS;

	/*if (priv->fw_error) {
		mutex_unlock(&priv->mutex);
		return;
	}*/

	if (priv->state != STARTED) {
		RPU_ERROR_UMACIF("%s: state != STARTED\n", __func__);
		mutex_unlock(&priv->mutex);
		return;
	}

	if ((*new_flags & FIF_ALLMULTI) || (mc_count == 0)) {
		/* Disable the multicast filter in LMAC */
		RPU_DEBUG_UMACIF("%s-80211IF: Multicast filters disabled\n",
				   priv->name);
		CALL_RPU(rpu_prog_mcast_filter_control,
			  MCAST_FILTER_DISABLE);
	} else if (mc_count) {
		/* Enable the multicast filter in LMAC */
		RPU_DEBUG_UMACIF("%s-80211IF: Multicast filters enabled\n",
			       priv->name);
		CALL_RPU(rpu_prog_mcast_filter_control,
			  MCAST_FILTER_ENABLE);
	}

	if (changed_flags == 0)
		/* No filters which we support changed */
		goto out;

	if (wifi->params.production_test == 0) {
		if (*new_flags & FIF_BCN_PRBRESP_PROMISC) {
			/* Receive all beacons and probe responses */
			RPU_DEBUG_UMACIF("%s-80211IF: RCV ALL bcns\n",
				       priv->name);
			CALL_RPU(rpu_prog_rcv_bcn_mode, RCV_ALL_BCNS);
		} else {
			/* Receive only network beacons and probe responses */
			RPU_DEBUG_UMACIF("%s-80211IF: RCV NW bcns\n",
					   priv->name);
			CALL_RPU(rpu_prog_rcv_bcn_mode,
				  RCV_ALL_NETWORK_ONLY);
		}
	}
out:
	if (wifi->params.production_test == 1) {
		RPU_DEBUG_UMACIF("%s-80211IF: RCV ALL bcns\n", priv->name);
		CALL_RPU(rpu_prog_rcv_bcn_mode, RCV_ALL_BCNS);
	}

prog_rpu_fail:
	mutex_unlock(&priv->mutex);
}


static int conf_vif_tx(struct ieee80211_hw  *hw,
		       struct ieee80211_vif *vif,
		       unsigned int link_id, u16 queue,
		       const struct ieee80211_tx_queue_params *txq_params)
{
	struct img_priv *priv = hw->priv;
	int vif_index, vif_active;
	struct edca_params params;
	struct ieee80211_vif *vif_local = NULL;

	/*if (priv->fw_error) {
		return 0;
	}*/

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	mutex_lock(&priv->mutex);

	for (vif_index = 0; vif_index < wifi->params.num_vifs; vif_index++) {
		if (!(priv->active_vifs & (1 << vif_index)))
			continue;

		rcu_read_lock();
		vif_local = rcu_dereference(priv->vifs[vif_index]);
		rcu_read_unlock();

		if (ether_addr_equal(vif_local->addr,
				     vif->addr))
			break;
	}

	if (WARN_ON(vif_index == wifi->params.num_vifs)) {
		mutex_unlock(&priv->mutex);
		return -EINVAL;
	}

	vif_active = 0;

	if ((priv->active_vifs & (1 << vif_index)))
		vif_active = 1;

	memset(&params, 0, sizeof(params));
	params.aifs = txq_params->aifs;
	params.txop = txq_params->txop;
	params.cwmin = txq_params->cw_min;
	params.cwmax = txq_params->cw_max;
	params.uapsd = txq_params->uapsd;

	rpu_vif_set_edca_params(queue,
					(struct umac_vif *)&vif->drv_priv,
					&params,
					vif_active);
	mutex_unlock(&priv->mutex);
	return 0;
}


static int set_key(struct ieee80211_hw *hw,
		   enum set_key_cmd cmd,
		   struct ieee80211_vif *vif,
		   struct ieee80211_sta *sta,
		   struct ieee80211_key_conf *key_conf)
{

	struct umac_key sec_key;
	unsigned int result = 0;
	struct img_priv *priv = hw->priv;
	unsigned int cipher_type, key_type;
	int vif_index;
	struct umac_vif *uvif;

	/*if (priv->fw_error) {
		return 0;
	}*/

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	uvif = ((struct umac_vif *)&vif->drv_priv);

	memset(&sec_key, 0, sizeof(struct umac_key));

	switch (key_conf->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
		sec_key.key = key_conf->key;
		cipher_type = CIPHER_TYPE_WEP40;
		break;
	case WLAN_CIPHER_SUITE_WEP104:
		sec_key.key = key_conf->key;
		cipher_type = CIPHER_TYPE_WEP104;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		key_conf->flags |= IEEE80211_KEY_FLAG_GENERATE_MMIC;
		/* We get the key in the following form:
		 * KEY (16 bytes) - TX MIC (8 bytes) - RX MIC (8 bytes)
		 */
		sec_key.key = key_conf->key;
		sec_key.tx_mic = key_conf->key + 16;
		sec_key.rx_mic = key_conf->key + 24;
		cipher_type = CIPHER_TYPE_TKIP;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		sec_key.key = key_conf->key;
		cipher_type = CIPHER_TYPE_CCMP;
		break;
	default:
		result = -EOPNOTSUPP;
		RPU_ERROR_CRYPTO("%s: not support cipher (%x)\n", __func__, key_conf->cipher);
		mutex_unlock(&priv->mutex);
		goto out;
	}

	g_cipher_type = cipher_type;
	vif_index = ((struct umac_vif *)&vif->drv_priv)->vif_index;

	mutex_lock(&priv->mutex);

	if (cmd == SET_KEY) {
		key_conf->hw_key_idx = 0; /* Don't really use this */

		/* This flag indicate that it requires IV generation */
		key_conf->flags |= IEEE80211_KEY_FLAG_GENERATE_IV;


		if (cipher_type == CIPHER_TYPE_WEP40 ||
		    cipher_type == CIPHER_TYPE_WEP104) {
			RPU_DEBUG_CRYPTO("%s-80211IF: ADD IF KEY (WEP).",
					  priv->name);
			RPU_DEBUG_CRYPTO(" vif_index = %d,", vif_index);
			RPU_DEBUG_CRYPTO(" keyidx = %d, cipher_type = %d\n",
					  key_conf->keyidx, cipher_type);

			rpu_prog_if_key(vif_index,
						vif->addr,
						KEY_CTRL_ADD,
						key_conf->keyidx,
						cipher_type,
						&sec_key);
		} else if (sta) {
			sec_key.peer_mac = sta->addr;

			if (key_conf->flags & IEEE80211_KEY_FLAG_PAIRWISE)
				key_type = KEY_TYPE_UCAST;
			else
				key_type = KEY_TYPE_BCAST;
			RPU_DEBUG_CRYPTO("%s-80211IF: ADD PEER KEY (WPA/WPA2)",
					  priv->name);
			RPU_DEBUG_CRYPTO(" vif_index = %d,", vif_index);
			RPU_DEBUG_CRYPTO(" keyidx = %d, keytype = %d,",
					  key_conf->keyidx, key_type);
			RPU_DEBUG_CRYPTO(" cipher_type = %d\n", cipher_type);

			rpu_prog_peer_key(vif_index,
						  vif->addr,
						  KEY_CTRL_ADD,
						  key_conf->keyidx,
						  key_type,
						  cipher_type,
						  &sec_key);
		} else {
			key_type = KEY_TYPE_BCAST;

			if (vif->type == NL80211_IFTYPE_STATION) {
				sec_key.peer_mac =
					(unsigned char *)vif->bss_conf.bssid;

				memcpy(uvif->bssid,
				       (vif->bss_conf.bssid),
				       ETH_ALEN);
				RPU_DEBUG_CRYPTO("%s-80211IF: ADD PEER KEY ",
						  priv->name);
				RPU_DEBUG_CRYPTO("(BCAST-STA). vif_index = %d",
						  vif_index);
				RPU_DEBUG_CRYPTO(", keyidx = %d, keytype = %d",
						key_conf->keyidx, key_type);
				RPU_DEBUG_CRYPTO(", cipher_type = %d\n",
						  cipher_type);

				rpu_prog_peer_key(vif_index,
							  vif->addr,
							  KEY_CTRL_ADD,
							  key_conf->keyidx,
							  key_type, cipher_type,
							  &sec_key);

			} else if (vif->type == NL80211_IFTYPE_AP) {
				RPU_DEBUG_CRYPTO("%s-80211IF: ADD IF KEY ",
						  priv->name);
				RPU_DEBUG_CRYPTO("(BCAST-AP). vif_index = %d",
						  vif_index);
				RPU_DEBUG_CRYPTO(", keyidx = %d",
						  key_conf->keyidx);
				RPU_DEBUG_CRYPTO(", cipher_type = %d\n",
						  cipher_type);

				rpu_prog_if_key(vif_index,
							vif->addr,
							KEY_CTRL_ADD,
							key_conf->keyidx,
							cipher_type,
							&sec_key);
			} else {
				/* ADHOC */
				/* TODO: Check this works for IBSS RSN */
				RPU_DEBUG_CRYPTO("%s-80211IF: ADD IF KEY ",
						  priv->name);
				RPU_DEBUG_CRYPTO("(BCAST-IBSS).vif_index = %d",
						  vif_index);
				RPU_DEBUG_CRYPTO(", keyidx = %d",
						  key_conf->keyidx);
				RPU_DEBUG_CRYPTO(", cipher_type = %d\n",
						  cipher_type);

				rpu_prog_if_key(vif_index,
							vif->addr,
							KEY_CTRL_ADD,
							key_conf->keyidx,
							cipher_type,
							&sec_key);
			}
		}
	} else if (cmd == DISABLE_KEY) {
		if ((cipher_type == CIPHER_TYPE_WEP40) ||
		    (cipher_type == CIPHER_TYPE_WEP104)) {
			rpu_prog_if_key(vif_index,
						vif->addr,
						KEY_CTRL_DEL,
						key_conf->keyidx,
						cipher_type,
						&sec_key);
			RPU_DEBUG_CRYPTO("%s-80211IF: DEL IF KEY (WEP).",
					  priv->name);
			RPU_DEBUG_CRYPTO(" vif_index = %d, keyidx = %d",
					  vif_index, key_conf->keyidx);
			RPU_DEBUG_CRYPTO(", cipher_type = %d\n", cipher_type);
		} else if (sta) {
			sec_key.peer_mac = sta->addr;

			if (key_conf->flags & IEEE80211_KEY_FLAG_PAIRWISE)
				key_type = KEY_TYPE_UCAST;
			else
				key_type = KEY_TYPE_BCAST;
			RPU_DEBUG_CRYPTO("%s-80211IF: DEL IF KEY (WPA/WPA2).",
					  priv->name);
			RPU_DEBUG_CRYPTO(" vif_index = %d, keyidx = %d",
					  vif_index, key_conf->keyidx);
			RPU_DEBUG_CRYPTO(", cipher_type = %d\n", cipher_type);

			rpu_prog_peer_key(vif_index,
						  vif->addr,
						  KEY_CTRL_DEL,
						  key_conf->keyidx,
						  key_type,
						  cipher_type,
						  &sec_key);
		} else {
			if (vif->type == NL80211_IFTYPE_STATION) {
				sec_key.peer_mac = uvif->bssid;
				RPU_DEBUG_CRYPTO("%s-80211IF: DEL IF KEY ",
						  priv->name);
				RPU_DEBUG_CRYPTO("(BCAST-STA). vif_index = %d",
						  vif_index);
				RPU_DEBUG_CRYPTO(", keyidx = %d",
						  key_conf->keyidx);
				RPU_DEBUG_CRYPTO(", cipher_type = %d\n",
						  cipher_type);

				rpu_prog_peer_key(vif_index,
							  vif->addr,
							  KEY_CTRL_DEL,
							  key_conf->keyidx,
							  KEY_TYPE_BCAST,
							  cipher_type,
							  &sec_key);

			} else if (vif->type == NL80211_IFTYPE_AP) {
				RPU_DEBUG_CRYPTO("%s-80211IF: DEL IF KEY ",
						  priv->name);
				RPU_DEBUG_CRYPTO("(BCAST-AP). vif_index = %d",
						  vif_index);
				RPU_DEBUG_CRYPTO(", keyidx = %d",
						  key_conf->keyidx);
				RPU_DEBUG_CRYPTO(", cipher_type = %d\n",
						  cipher_type);

				rpu_prog_if_key(vif_index,
							vif->addr,
							KEY_CTRL_DEL,
							key_conf->keyidx,
							cipher_type,
							&sec_key);
			} else {
				RPU_DEBUG_CRYPTO("%s-80211IF: DEL IF KEY ",
						  priv->name);
				RPU_DEBUG_CRYPTO("(BCAST-IBSS).vif_index = %d",
						  vif_index);
				RPU_DEBUG_CRYPTO(", keyidx = %d",
						  key_conf->keyidx);
				RPU_DEBUG_CRYPTO(", cipher_type = %d\n",
						  cipher_type);

				rpu_prog_if_key(vif_index,
							vif->addr,
							KEY_CTRL_DEL,
							key_conf->keyidx,
							cipher_type,
							&sec_key);
			}
		}
	}

	mutex_unlock(&priv->mutex);

out:
	return result;
}


static void bss_info_changed(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif,
			     struct ieee80211_bss_conf *bss_conf,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
                             u64 changed)
#else
			     unsigned int changed)
#endif
{
	struct img_priv   *priv= hw->priv;

	/*if (priv->fw_error) {
		return;
	}*/

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	dump_bss_changed_info(changed);

	mutex_lock(&priv->mutex);

	if (wifi->params.production_test || wifi->params.disable_beacon_ibss) {
		/* Disable beacon generation when running pktgen
		 * for performance
		 */
		changed &= ~BSS_CHANGED_BEACON_INT;
		changed &= ~BSS_CHANGED_BEACON_ENABLED;
	}

	rpu_vif_bss_info_changed((struct umac_vif *)&vif->drv_priv, vif, hw,
					 bss_conf,
					 changed);
	mutex_unlock(&priv->mutex);
}

/* 802.11 high throughput*/
static void setup_ht_cap(struct ieee80211_sta_ht_cap *ht_info)
{
	int i;

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	memset(ht_info, 0, sizeof(*ht_info));
	ht_info->ht_supported = true;
	//RPU_DEBUG_IF("SETUP HT CALLED\n");

	ht_info->cap = 0;
	ht_info->cap |= IEEE80211_HT_CAP_MAX_AMSDU;
	/*We support SMPS*/

	ht_info->ampdu_factor = IEEE80211_HT_MAX_AMPDU_32K;
	ht_info->ampdu_density = IEEE80211_HT_MPDU_DENSITY_4;

	memset(&ht_info->mcs, 0, sizeof(ht_info->mcs));

	if (wifi->params.max_tx_streams != wifi->params.max_rx_streams) {
		ht_info->mcs.tx_params |= IEEE80211_HT_MCS_TX_RX_DIFF;
		ht_info->mcs.tx_params |= ((wifi->params.max_tx_streams - 1)
				<< IEEE80211_HT_MCS_TX_MAX_STREAMS_SHIFT);
	}

	ht_info->mcs.tx_params |= IEEE80211_HT_MCS_TX_DEFINED;

	for (i = 0; i < wifi->params.max_rx_streams; i++)
#ifdef RK3036_DONGLE	
		ht_info->mcs.rx_mask[i] = 0x1f;
#else
		ht_info->mcs.rx_mask[i] = 0xff;
#endif
}



static void set_hw_flags(struct ieee80211_hw *hw)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	ieee80211_hw_set(hw, SIGNAL_DBM);
	ieee80211_hw_set(hw, SUPPORTS_PS);
	ieee80211_hw_set(hw, HOST_BROADCAST_PS_BUFFERING);
	ieee80211_hw_set(hw, AMPDU_AGGREGATION);
	ieee80211_hw_set(hw, MFP_CAPABLE);
	ieee80211_hw_set(hw, REPORTS_TX_ACK_STATUS);
	ieee80211_hw_set(hw, SUPPORTS_PER_STA_GTK);
	ieee80211_hw_set(hw, CONNECTION_MONITOR);
#else
	hw->flags = IEEE80211_HW_SIGNAL_DBM;
	hw->flags |= IEEE80211_HW_SUPPORTS_PS;
	hw->flags |= IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING;
	hw->flags |= IEEE80211_HW_AMPDU_AGGREGATION;
	hw->flags |= IEEE80211_HW_MFP_CAPABLE;
	hw->flags |= IEEE80211_HW_REPORTS_TX_ACK_STATUS;
	hw->flags |= IEEE80211_HW_SUPPORTS_PER_STA_GTK;
	hw->flags |= IEEE80211_HW_CONNECTION_MONITOR;
#endif
	if (!wifi->params.disable_power_save &&
	    !wifi->params.disable_sm_power_save) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		hw->wiphy->features |= NL80211_FEATURE_STATIC_SMPS |
					NL80211_FEATURE_DYNAMIC_SMPS;
#else
		hw->flags |= IEEE80211_HW_SUPPORTS_STATIC_SMPS;
		hw->flags |= IEEE80211_HW_SUPPORTS_DYNAMIC_SMPS;
#endif
	}
#ifdef RPU_SLEEP_ENABLE
#ifdef PS_SLEEP_TEST
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	ieee80211_hw_set(hw, SUPPORTS_DYNAMIC_PS);
#else
	hw->flags |= IEEE80211_HW_SUPPORTS_DYNAMIC_PS;
#endif
#endif
#endif
}

static void init_hw(struct ieee80211_hw *hw)
{
	struct img_priv  *priv= (struct img_priv *)hw->priv;
	int num_if_comb = 0;

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	/* Supported Interface Types and other Default values*/
	hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) |
				     BIT(NL80211_IFTYPE_ADHOC) |
				     BIT(NL80211_IFTYPE_AP) |
				     BIT(NL80211_IFTYPE_P2P_CLIENT) |
				     BIT(NL80211_IFTYPE_P2P_GO);

	hw->wiphy->iface_combinations = if_comb;

	num_if_comb = (sizeof(if_comb) /
		       sizeof(struct ieee80211_iface_combination));
	hw->wiphy->n_iface_combinations = num_if_comb;

	set_hw_flags(hw);
	hw->wiphy->max_scan_ssids = MAX_NUM_SSIDS; /* 4 */
	 /* Low priority bg scan */
	hw->wiphy->features |= NL80211_FEATURE_LOW_PRIORITY_SCAN;
	hw->wiphy->max_scan_ie_len = IEEE80211_MAX_DATA_LEN;
	hw->max_listen_interval = 10;
	hw->wiphy->max_remain_on_channel_duration = 5000; /*ROC*/
	hw->offchannel_tx_hw_queue = WLAN_AC_VO;
	hw->max_rates = 4;
	hw->max_rate_tries = 5;
	hw->queues = 4;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	/*
	 * The value is a bit-shift of 1 second, 
	 * so 5 is ~31ms (1000ms >> 5) of queued data
	 */
	hw->tx_sk_pacing_shift = 5;
#endif

	//hw->max_rx_aggregation_subframes = 32;

	/* Size */
	hw->extra_tx_headroom = 0;
	hw->vif_data_size = sizeof(struct umac_vif);
	hw->sta_data_size = sizeof(struct umac_sta);

	if (wifi->params.dot11g_support) {
		hw->wiphy->bands[IEEE80211_BAND_2GHZ] = &band_2ghz;
		if (ht_support)
			setup_ht_cap(&hw->wiphy->bands[IEEE80211_BAND_2GHZ]->ht_cap);
	}


	memset(hw->wiphy->addr_mask, 0, sizeof(hw->wiphy->addr_mask));

	if (wifi->params.num_vifs == 1) {
		hw->wiphy->addresses = NULL;
		SET_IEEE80211_PERM_ADDR(hw, priv->if_mac_addresses[0].addr);
	} else {
		hw->wiphy->n_addresses = wifi->params.num_vifs;
		hw->wiphy->addresses = priv->if_mac_addresses;
	}

	hw->wiphy->flags |= WIPHY_FLAG_AP_UAPSD;
	hw->wiphy->flags |= WIPHY_FLAG_IBSS_RSN;
	hw->wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
#ifdef CONFIG_PM
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
#ifdef WOWLAN_SUPPORT
	hw->wiphy->wowlan = &uccp_wowlan_support;
#else
	hw->wiphy->wowlan = NULL;
#endif
#else
        hw->wiphy->wowlan.flags = WIPHY_WOWLAN_ANY;
#endif
#endif
}


static int ampdu_action(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
				struct ieee80211_ampdu_params *params)
#else
				enum ieee80211_ampdu_mlme_action action,
				struct ieee80211_sta *sta,
				u16 tid, u16 *ssn, u8 buf_size)
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
        struct ieee80211_sta *sta = params->sta;
        enum ieee80211_ampdu_mlme_action action = params->action;
        u16 tid = params->tid;
        u16 *ssn = &params->ssn;
	u8 buf_size = params->buf_size;
#endif
	int ret = 0;
	unsigned int val = 0;
	struct img_priv *priv = (struct img_priv *)hw->priv;

	/*if (priv->fw_error) {
		return 0;
	}*/

	RPU_DEBUG_UMACIF("%s: %s: tid = %d, ssn = %d, buf_szie = %d\n",
					UMAC_IF_TAG, __func__, tid, (ssn!=NULL)?*ssn:0, buf_size);
	dump_ampdu_action_info(action);
	switch (action) {
	case IEEE80211_AMPDU_RX_START:
		{
		val = tid | TID_INITIATOR_AP;
		priv->tid_info[val].tid_state = TID_STATE_AGGR_START;
		priv->tid_info[val].ssn = *ssn;
		rpu_prog_ba_session_data(1,
						 tid,
						 &priv->tid_info[val].ssn,
						 1,
						 vif->addr,
				   (unsigned char *)(vif->bss_conf.bssid));
		}
		break;
	case IEEE80211_AMPDU_RX_STOP:
		{
		val = tid | TID_INITIATOR_AP;
		priv->tid_info[val].tid_state = TID_STATE_AGGR_STOP;
		rpu_prog_ba_session_data(0,
						 tid,
						 &priv->tid_info[val].ssn,
						 1,
						 vif->addr,
				   (unsigned char *)(vif->bss_conf.bssid));
		}
		break;
	case IEEE80211_AMPDU_TX_START:
		{
		val = tid | TID_INITIATOR_STA;
		ieee80211_start_tx_ba_cb_irqsafe(vif, sta->addr, tid);
		priv->tid_info[val].tid_state = TID_STATE_AGGR_START;
		priv->tid_info[val].ssn = *ssn;
		}
		break;
	case IEEE80211_AMPDU_TX_STOP_FLUSH:
	case IEEE80211_AMPDU_TX_STOP_FLUSH_CONT:
	case IEEE80211_AMPDU_TX_STOP_CONT:
		{
		val = tid | TID_INITIATOR_STA;
		priv->tid_info[val].tid_state = TID_STATE_AGGR_STOP;
		ieee80211_stop_tx_ba_cb_irqsafe(vif, sta->addr, tid);
		}
		break;
	case IEEE80211_AMPDU_TX_OPERATIONAL:
		{
		val = tid | TID_INITIATOR_STA;
		priv->tid_info[val].tid_state = TID_STATE_AGGR_OPERATIONAL;
		}
		break;
	default:
		RPU_ERROR_UMACIF("%s: Invalid command (%d), ignoring\n",
		       __func__, action);
	}
	return ret;
}


static int set_antenna(struct ieee80211_hw *hw, int radio_idx, u32 tx_ant, u32 rx_ant)
{
	struct img_priv *priv = (struct img_priv *)hw->priv;

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	/* Maximum no of antenna supported =2 */
	if (!tx_ant || (tx_ant & ~3) || !rx_ant || (rx_ant & ~3)) {
		RPU_ERROR_UMACIF("%s: invalid antenna parameter (%x, %x)\n", __func__, tx_ant, rx_ant);
		return -EINVAL;
	}

	priv->tx_antenna = (tx_ant & 3);

	return 0;
}



/* Needed in case of IBSS to send out probe responses when we are beaconing */
static int tx_last_beacon(struct ieee80211_hw *hw)
{
	struct img_priv *priv = (struct img_priv *)hw->priv;

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	return priv->tx_last_beacon;
}

#ifdef HW_SCAN_TIMEOUT_ABORT
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
extern void scan_timer_expiry(struct timer_list *t);
#else
extern void scan_timer_expiry(unsigned long data);
#endif
static void init_scan_timeout_timer (struct img_priv *priv)
{
	RPU_DEBUG_UMACIF("%s: %p\n", __func__, priv);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
	timer_setup(&priv->scan_timer, scan_timer_expiry, 0);
#else
	init_timer(&priv->scan_timer);
	priv->scan_timer.data = (unsigned long)NULL;
	priv->scan_timer.function = scan_timer_expiry;
#endif
	priv->in_scan_timeout = 0;
}

static void start_scan_timeout_timer(struct img_priv *priv, int p2p)
{
	RPU_DEBUG_UMACIF("%s: %p\n", __func__, priv);
#ifdef RK3036_DONGLE	
	if (p2p)
		mod_timer(&priv->scan_timer, jiffies + 2 * HZ);
	else
#endif	
		mod_timer(&priv->scan_timer, jiffies + HW_SCAN_TIMEOUT * HZ);
}

static void deinit_scan_timeout_timer (struct img_priv *priv)
{
	RPU_DEBUG_UMACIF("%s: %p\n", __func__, priv);
	timer_delete(&priv->scan_timer);
}
#endif

#ifdef ENABLE_SPLIT_MULT_SSID_SCAN
static int split_mult_ssid_scan(struct img_priv *priv, int do_scan, int vif_index)
{
	struct scan_req *scan_req = &priv->remain_scan_req;
	int ret = 0;

	//mutex_lock(&priv->scan_mutex);

	if (scan_req->n_ssids > 0) {
		RPU_DEBUG_SCAN("%s: n_ssids = %d, do_scan = %d\n",
							__func__, scan_req->n_ssids, do_scan);	
		//mutex_unlock(&priv->scan_mutex);
		
		if (do_scan) {
			struct scan_req req;

			ret = 1;
			memcpy(&req, scan_req, sizeof(struct scan_req));
			req.n_ssids = 1;
			rpu_scan(vif_index, &req);
		}

		//mutex_lock(&priv->scan_mutex);
		scan_req->n_ssids -= 1;
		if (scan_req->n_ssids > 0) {
			memcpy(&scan_req->ssids[0], &scan_req->ssids[1], scan_req->n_ssids*sizeof(struct ssid_desc));
		}
	}

	//mutex_unlock(&priv->scan_mutex);

	return ret;
}
#endif

int scan(struct ieee80211_hw *hw,
	 struct ieee80211_vif *vif,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	 struct ieee80211_scan_request *hw_req)
#else
	 struct cfg80211_scan_request *req)
#endif
{
	struct umac_vif *uvif = (struct umac_vif *)vif->drv_priv;
	struct scan_req scan_req = {0};
	int i = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	struct cfg80211_scan_request *req = &hw_req->req;
#endif

	/*if (uvif->priv->fw_error) {
		return -EBUSY;
	}*/

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	scan_req.n_ssids = req->n_ssids;
	scan_req.n_channels = req->n_channels;
	scan_req.ie_len = req->ie_len;

	if (wifi->params.hw_scan_status != HW_SCAN_STATUS_NONE) {
		RPU_INFO_UMACIF("%s: Already in HW SCAN State\n", __func__);
		return -EBUSY; /* Already in HW SCAN State */
	}

	if (uvif->priv->roc_params.roc_starting == 1) {
		RPU_INFO_UMACIF("%s: Already in roc_starting State\n", __func__);
		return -EBUSY;
	}

#ifdef RK3036_DONGLE
	if (req->n_channels == 3 && req->no_cck) {
		ieee80211_scan_completed(uvif->priv->hw, false);
		return 0;
	}
#endif

	/* Keep track of HW Scan requests and compeltes */
	wifi->params.hw_scan_status = HW_SCAN_STATUS_PROGRESS;

	if (uvif->priv->params->production_test == 1) {
		/* Drop scan, its just intended for IBSS
		 * and some data traffic
		 */
		if (wifi->params.hw_scan_status != HW_SCAN_STATUS_NONE) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
			struct cfg80211_scan_info info = {
				.aborted = false,
 			};
			ieee80211_scan_completed(uvif->priv->hw, &info);
#else
			ieee80211_scan_completed(uvif->priv->hw, false);
#endif
			wifi->params.hw_scan_status = HW_SCAN_STATUS_NONE;
		}

		return 0;
	}

#ifdef HW_SCAN_TIMEOUT_ABORT
	start_scan_timeout_timer(uvif->priv, req->no_cck && (req->n_channels <= 3));
#endif

	if (req->ie_len)
		memcpy(scan_req.ie, req->ie, req->ie_len);

	for (i = 0; i < req->n_channels; i++) {
		scan_req.center_freq[i] = req->channels[i]->center_freq;
		scan_req.freq_max_power[i] = req->channels[i]->max_power;
		scan_req.chan_flags[i] = req->channels[i]->flags;
		/* The type of scan comes from mac80211 so its taken care of */
	}

	scan_req.p2p_probe = req->no_cck;

	/* For hostapd scan (40MHz) and scan_type=passive, n_ssids=0
	 * and req->ssids is NULL
	 */
	if (req->n_ssids > 0) {
		for (i = 0; i < req->n_ssids; i++) {
			scan_req.ssids[i].ssid_len = req->ssids[i].ssid_len;
			if (req->ssids[i].ssid_len > 0)
				memcpy(scan_req.ssids[i].ssid,
				       req->ssids[i].ssid,
				       req->ssids[i].ssid_len);
		}
	}

	uvif->priv->p2p_scan = scan_req.p2p_probe;
#ifdef ENABLE_DAPT
	dapt_scan(uvif->priv);
#endif

#ifdef ENABLE_SPLIT_MULT_SSID_SCAN
	if (req->n_ssids > 1) {
		memcpy(&uvif->priv->remain_scan_req, &scan_req, sizeof(struct scan_req));
		uvif->priv->scan_req_vif_iface = uvif->vif_index;

		RPU_DEBUG_SCAN("start split ssid scan: n_ssids = %d\n", scan_req.n_ssids);
		for (i = 0; i < scan_req.n_ssids; i++) {
			if (scan_req.ssids[i].ssid_len != 0)
				RPU_DEBUG_SCAN("SSID: %s\n", scan_req.ssids[i].ssid);
			else
				RPU_DEBUG_SCAN("SSID: EMPTY\n");
		}

		split_mult_ssid_scan(uvif->priv, 0, uvif->vif_index);
		scan_req.n_ssids = 1;
	} else {
		uvif->priv->remain_scan_req.n_ssids = 0;
	}
#endif

	return rpu_scan(uvif->vif_index, &scan_req);
}


void rpu_scan_complete(void *context,
			       struct host_event_scanres *scan_res,
			       unsigned char *skb,
			       unsigned int len)
{
	struct img_priv *priv = (struct img_priv *)context;

#ifdef ENABLE_SPLIT_MULT_SSID_SCAN
	if (split_mult_ssid_scan(priv, 1, priv->scan_req_vif_iface)) {
		// split_mult_ssid_scan do not finish
		priv->stats->umac_scan_complete++;
		return;
	}
#endif

	RPU_DEBUG_SCAN("Event Scan Complete from RPU:");
	RPU_DEBUG_SCAN(" More_results: 0, if_index = %d, Scan is Completed\n", scan_res->if_index);
	/* There can be a race where we receive remove_interface and
	 * abort the scan(1)
	 * But we get scan_complete from the FW(2), this check will make
	 * sure we are not calling scan_complete when we have already
	 * aborted the scan. Eg: Killing wpa_supplicant in middle of
	 * scanning
	 */
	spin_lock_bh(&priv->scan_cancel_lock);
	if (wifi->params.hw_scan_status != HW_SCAN_STATUS_NONE) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
		struct cfg80211_scan_info info = {
			.aborted = false,
		};
#endif

		/* Keep track of HW Scan requests and compeltes */
		wifi->params.hw_scan_status = HW_SCAN_STATUS_NONE;
		spin_unlock_bh(&priv->scan_cancel_lock);

		priv->stats->umac_scan_complete++;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
		ieee80211_scan_completed(priv->hw, &info);
#else
		ieee80211_scan_completed(priv->hw, false);
#endif

#ifdef ENABLE_DAPT
		dapt_scan_complete(priv);
#endif
	} else {
		spin_unlock_bh(&priv->scan_cancel_lock);
	}

}

void cancel_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	//struct umac_vif *uvif = (struct umac_vif *)vif->drv_priv;
	struct umac_vif *uvif = NULL;
	struct img_priv *priv = NULL;
	int lock = 1;

	if(vif != NULL)
		uvif = (struct umac_vif *)vif->drv_priv;
	else
		lock = 0;
	
	priv= (struct img_priv *)hw->priv;
	if (wifi->hw == NULL || priv->state != STARTED)
		return;

	/*if (priv->fw_error) {
		return;
	}*/

#ifdef ENABLE_SPLIT_MULT_SSID_SCAN
	//mutex_lock(&priv->scan_mutex);
	priv->remain_scan_req.n_ssids = 0;
	//mutex_unlock(&priv->scan_mutex);
#endif

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);

	spin_lock_bh(&priv->scan_cancel_lock);
	if (wifi->params.hw_scan_status == HW_SCAN_STATUS_PROGRESS) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
		struct cfg80211_scan_info info = {
 			.aborted = true,
		};
#endif
		wifi->params.hw_scan_status = HW_SCAN_STATUS_NONE;
		spin_unlock_bh(&priv->scan_cancel_lock);

		RPU_INFO_UMACIF("Aborting pending scan request...\n");
		
		if(vif != NULL)
		{
			//when FW error and recovery, no need to call rpu scan abort
			priv->scan_abort_done = 0;
			if (rpu_scan_abort(uvif->vif_index)) {
				return;
			}

			//As wait for scan abort should always return 0
			wait_for_scan_abort(priv);
		}
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
		ieee80211_scan_completed(hw, &info);
#else
		ieee80211_scan_completed(hw, true);
#endif
		priv->stats->umac_scan_complete++;

#ifdef ENABLE_DAPT
		dapt_scan_complete(priv);
#endif
#ifdef RK3036_DONGLE
		if (priv->in_scan_timeout /*&& !priv->p2p_scan*/) {
#else
		if (priv->in_scan_timeout && !priv->p2p_scan) {
#endif
			priv->in_scan_timeout = 0;
			hpriv->fw_error_counter_scan++;
			rk915_io_reset(hpriv);
			rk915_signal_io_error(FW_ERR_SDIO);
		}
 	} else {
		spin_unlock_bh(&priv->scan_cancel_lock);
 	}
}


int set_rts_threshold(struct ieee80211_hw *hw, int radio_idx,
		      u32 value)
{
	struct img_priv *priv = NULL;

	priv = (struct img_priv *)hw->priv;
	/*if thres>=2347 (default case) hostapd sends down (u32) -1*/
	if (value > 65536)
		priv->rts_threshold = 65536;
	else
		priv->rts_threshold = value;
	return 0;

}


#if 0
int load_fw(struct ieee80211_hw *hw)
{
        int err = 0;
        int i = 0;
        struct img_priv *dev = (struct img_priv *)hw->priv;
        const struct firmware *fw = NULL;

        do {
                err = request_firmware(&fw, bin_name[i], dev->dev);

		/* Proceed even if there is no patch file
		 */
		if (err)
			err = fwldr_load_fw(NULL, fw->size, i);
		else
			err = fwldr_load_fw(fw->data, fw->size, i);

                if (err == FWLDR_SUCCESS)
                        pr_info("%s is loaded\n", bin_name[i]);
                else
                        pr_err("Loading of %s failed\n", bin_name[i]);

                release_firmware(fw);

                i++;

        } while ((i < FWLDR_NUM_BINS) && (!err));

        return err;
}
#endif




static struct ieee80211_ops ops = {
	.tx                 = tx,
	.start              = start,
	.stop               = stop,
	.add_interface      = add_interface,
	.remove_interface   = remove_interface,
	.change_interface   = change_interface,
	.config             = config,
	.prepare_multicast  = prepare_multicast,
	.configure_filter   = configure_filter,
	.sw_scan_start      = NULL,
	.sw_scan_complete   = NULL,
	.get_stats          = NULL,
	.sta_notify         = NULL,
	.conf_tx            = conf_vif_tx,
	.bss_info_changed   = bss_info_changed,
	.set_tim            = NULL,
	.set_key            = set_key,
	.tx_last_beacon     = tx_last_beacon,
	.ampdu_action       = ampdu_action,
	.set_antenna	    = set_antenna,
	.remain_on_channel = remain_on_channel,
	.cancel_remain_on_channel = cancel_remain_on_channel,
#ifdef CONFIG_PM
	.suspend	    = img_suspend,
	.resume		    = img_resume,
#endif
	.hw_scan	    = scan,
	.cancel_hw_scan	    = cancel_hw_scan,
	.set_rekey_data     = NULL,
	.set_rts_threshold  = set_rts_threshold,
	/* dummy required callbacks */
	.wake_tx_queue	    = ieee80211_handle_wake_tx_queue,
	.add_chanctx = ieee80211_emulate_add_chanctx,
	.remove_chanctx = ieee80211_emulate_remove_chanctx,
	.change_chanctx = ieee80211_emulate_change_chanctx,
	.switch_vif_chanctx = ieee80211_emulate_switch_vif_chanctx,

};

void rpu_exit(void)
{
	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);

	wifi->params.pkt_gen_val = 0;

#ifdef HW_SCAN_TIMEOUT_ABORT
	deinit_scan_timeout_timer(wifi->hw->priv);
#endif
#ifdef RK3036_DONGLE
    deinit_roc_timeout_timer(wifi->hw->priv);
#endif
#ifdef ENABLE_KEEP_ALIVE
	deinit_keep_alive_timer(wifi->hw->priv);
#endif

	if (wifi->hw) {
		ieee80211_unregister_hw(wifi->hw);
		ieee80211_free_hw(wifi->hw);
		wifi->hw = NULL;
	}
}

/*
 * Base MAC address, in this order: "local-mac-address"/"mac-address" of the
 * SDIO function node (U-Boot fills it in through the ethernet0 alias), the
 * mac_addr module parameter, a random address. The BSP read it from the
 * Rockchip vendor storage instead.
 */
void init_mac_addr(void)
{
	struct device *dev = hal_ops.get_dev();

	if (dev && dev->of_node &&
	    !of_get_mac_address(dev->of_node, vif_macs[0])) {
		RPU_INFO_UMACIF("MAC address from the device tree\n");
	} else if (mac_addr && mac_pton(mac_addr, vif_macs[0]) &&
		   is_valid_ether_addr(vif_macs[0])) {
		RPU_INFO_UMACIF("MAC address from the mac_addr parameter\n");
	} else {
		eth_random_addr(vif_macs[0]);
		RPU_INFO_UMACIF("random MAC address\n");
	}
	img_ether_addr_copy(vif_macs[1], vif_macs[0]);

	/* Set the Locally Administered bit*/
	vif_macs[1][0] |= 0x02;

	/* Increment the MSB by 1 (excluding 2 special bits)*/
	vif_macs[1][0] += (1 << 2);
}

int rpu_init(void)
{
	struct ieee80211_hw *hw;
	int error;
	struct img_priv *priv = NULL;
	int i;

	RPU_DEBUG_UMACIF("%s: %s\n", UMAC_IF_TAG, __func__);
	/* Allocate new hardware device */
	hw = ieee80211_alloc_hw(sizeof(struct img_priv), &ops);

	if (hw == NULL) {
		RPU_ERROR_UMACIF("Failed to allocate memory for ieee80211_hw\n");
		error = -ENOMEM;
		goto out;
	}

	priv = (struct img_priv *)hw->priv;
	memset(priv, 0, sizeof(struct img_priv));

	init_mac_addr();
	RPU_INFO_UMACIF("MAC ADDR: %pM\n", vif_macs);

	priv->dev = hal_ops.get_dev();
	SET_IEEE80211_DEV(hw, priv->dev);

	mutex_init(&priv->mutex);
	mutex_init(&priv->scan_mutex);
	mutex_init(&priv->scan_cancel_mutex);
#ifdef ENABLE_DAPT	
	spin_lock_init(&priv->dapt_lock);
#endif
	spin_lock_init(&priv->bcast_lock);
	spin_lock_init(&priv->scan_cancel_lock);

	spin_lock_init(&priv->roc_lock);
	priv->state = STOPPED;
	priv->active_vifs = 0;
	priv->txpower = DEFAULT_TX_POWER;
	priv->tx_antenna = DEFAULT_TX_ANT_SELECT;
	priv->rts_threshold = DEFAULT_RTS_THRESHOLD;
	strscpy(priv->name, RPU_DRIVER_NAME, sizeof(priv->name));

	for (i = 0; i < wifi->params.num_vifs; i++)
		img_ether_addr_copy(priv->if_mac_addresses[i].addr, vif_macs[i]);

	/* Initialize HW parameters */
	init_hw(hw);
	priv->hw = hw;
	priv->params = &wifi->params;
	priv->stats = &wifi->stats;
	priv->fw_info = &wifi->fw_info;
	priv->umac_proc_dir_entry = wifi->umac_proc_dir_entry;
	priv->current_vif_count = 0;
	priv->stats->system_rev = system_rev;

	/*Register hardware*/
	error = ieee80211_register_hw(hw);

	/* Production test hack: Set all channel flags to 0 to allow IBSS
	 * creation in all channels
	 */
	if (wifi->params.production_test && !error) {
		enum ieee80211_band band;
		struct ieee80211_supported_band *sband;

		for (band = 0; band < IEEE80211_NUM_BANDS; band++) {
			sband = hw->wiphy->bands[band];
			if (sband)
				for (i = 0; i < sband->n_channels; i++)
					sband->channels[i].flags = 0;
		}
	}

#ifdef HW_SCAN_TIMEOUT_ABORT
	init_scan_timeout_timer(priv);
#endif
#ifdef RK3036_DONGLE
    init_roc_timeout_timer(priv);
#endif
#ifdef ENABLE_KEEP_ALIVE
	init_keep_alive_timer(priv);
#endif
	init_vif_info(priv);

	if (!error) {
		wifi->hw = hw;
		//rpu_if_init(priv, priv->name);
		goto out;
	} else {
		RPU_ERROR_UMACIF("%s: ieee80211_register_hw failed\n", __func__);
		rpu_exit();
		goto out;
	}

out:
	return error;
}

#ifdef CONFIG_WIRELESS_EXT
int iw_send_hang_event(struct img_priv *priv)
{
	struct net_device *dev;
	union iwreq_data wrqu;
	char extra[IW_CUSTOM_MAX + 1];
	int cmd;

	dev = (struct net_device *)priv->net_dev;
	if (!dev) {
		RPU_ERROR_UMACIF("%s failed\n", __func__);
		return -1;
	}

	cmd = IWEVCUSTOM;
	memset(&wrqu, 0, sizeof(wrqu));

	strcpy(extra, "HANG");
	wrqu.data.length = strlen(extra);
	wireless_send_event(dev, cmd, &wrqu, extra);
	RPU_INFO_UMACIF("Send IWEVCUSTOM Event as %s\n", extra);

	return 0;
}
#endif

