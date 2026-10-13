
/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "core.h"
#include "utils.h"

struct wlan_rx_pkt rx_control_info;

spinlock_t tsf_lock;

#ifdef ENABLE_DAPT

#define DAPT_ENABLE_SCAN

#define CURRENT_CHANNEL(x) ieee80211_frequency_to_channel(x->cur_chan.center_freq1)

#define INVALID_VIF_IDX(x) (x > MAX_VIFS -1)

static inline bool dapt_is_ap_mode(struct img_priv *priv)
{
	if (priv->iftype == NL80211_IFTYPE_AP)
		return true;
	return false;
}

static void dapt_find_main_iface(struct img_priv *priv)
{
	struct dapt_params *dapt = &priv->dapt_params;

	dapt->main_index = find_main_iface(priv);
}

static void dapt_find_p2p_iface(struct img_priv *priv)
{
	struct dapt_params *dapt = &priv->dapt_params;

	dapt->p2p_index = find_p2p_iface(priv);
}

void dapt_start_timer(struct img_priv *priv, int msecs)
{
	struct dapt_params *dapt = &priv->dapt_params;

	RPU_DEBUG_DAPT("%s\n", __func__);

	if (!dapt->dapt_disable) {
		mod_timer(&dapt->dapt_timer, jiffies + msecs_to_jiffies(msecs));
		dapt->timer_start = 1;
	}
}

void dapt_stop_timer(struct img_priv *priv)
{
	struct dapt_params *dapt = &priv->dapt_params;

	RPU_DEBUG_DAPT("%s\n", __func__);

	if (!dapt->dapt_disable) {
		timer_delete_sync(&dapt->dapt_timer);
		dapt->timer_start = 0;
	}
}

void dapt_param_init(struct img_priv *priv)
{
	struct dapt_params *dapt = &priv->dapt_params;
	int i;

	if (priv->params->production_test == 1)
		return;

	RPU_DEBUG_DAPT("%s\n", __func__);

	memset(dapt, 0, sizeof(struct dapt_params));

	for (i = 0; i < 14; i++) {
		dapt->cur_seted_thresh[i] = DAPT_DEFAULT_PHY_THRESH;
	}

	dapt->main_index = MAX_VIFS;
	dapt->p2p_index = MAX_VIFS;

	priv->sniffer = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
	timer_setup(&dapt->dapt_timer, dapt_timer_expiry, 0);
#else
	init_timer(&dapt->dapt_timer);
	dapt->dapt_timer.data = (unsigned long)priv;
	dapt->dapt_timer.function = dapt_timer_expiry;
#endif

	dapt_start_timer(priv, DAPT_CALC_INTERVAL);
}

void dapt_param_late_init(struct img_priv *priv)
{
	dapt_find_main_iface(priv);
	dapt_find_p2p_iface(priv);
}

void dapt_param_deinit(struct img_priv *priv)
{
	if (priv->params->production_test == 1)
		return;

	RPU_DEBUG_DAPT("%s\n", __func__);

	dapt_stop_timer(priv);
}

void dapt_disable(struct img_priv *priv, int disable)
{
	struct dapt_params *dapt = &priv->dapt_params;

	RPU_DEBUG_DAPT("%s: disable = %d\n", __func__, disable);

	dapt->dapt_disable = disable;
}

void dapt_clr_history(struct img_priv *priv, int index)
{
	struct dapt_params *dapt = &priv->dapt_params;

	if (INVALID_VIF_IDX(index))
		return;

	dapt->sam_read[index] = 0;
	dapt->sam_write[index] = 0;
	dapt->sam_size[index] = 0;

	dapt->bcn_read[index] = 0;
	dapt->bcn_write[index] = 0;
	dapt->bcn_size[index] = 0;
}

/*
 *         read           write
 *  |_____|_________|______|
 *                   size
 */
#define SAM_STEP(x, y)						\
	do {								\
		x++;							\
		x = x % y;	\
	} while(0)

static inline s8 dapt_get_sample(struct img_priv *priv, int index)
{
	struct dapt_params *dapt = &priv->dapt_params;
	s8 sample;

	if (INVALID_VIF_IDX(index))
		return 0;

	sample = dapt->sam_history[index][dapt->sam_read[index]];
	SAM_STEP(dapt->sam_read[index], DAPT_MAX_RSSI_SAMPLE);

	return sample;
}

static inline void dapt_set_sample(struct img_priv *priv, s8 rss, int index)
{
	struct dapt_params *dapt = &priv->dapt_params;

	if (INVALID_VIF_IDX(index))
		return;

	dapt->sam_history[index][dapt->sam_write[index]] = rss;
	SAM_STEP(dapt->sam_write[index], DAPT_MAX_RSSI_SAMPLE);

	dapt->sam_size[index]++;
	if (dapt->sam_size[index] > DAPT_MAX_RSSI_SAMPLE) {
		SAM_STEP(dapt->sam_read[index], DAPT_MAX_RSSI_SAMPLE);
	}
}

static inline s8 dapt_get_bcn_sample(struct img_priv *priv, int index)
{
	struct dapt_params *dapt = &priv->dapt_params;
	s8 sample;

	if (INVALID_VIF_IDX(index))
		return 0;

	sample = dapt->bcn_history[index][dapt->bcn_read[index]];
	SAM_STEP(dapt->bcn_read[index], DAPT_MAX_RSSI_SAMPLE);

	return sample;
}

static inline void dapt_set_bcn_sample(struct img_priv *priv, s8 rss, int index)
{
	struct dapt_params *dapt = &priv->dapt_params;

	if (INVALID_VIF_IDX(index))
		return;

	dapt->bcn_history[index][dapt->bcn_write[index]] = rss;
	SAM_STEP(dapt->bcn_write[index], DAPT_MAX_RSSI_SAMPLE);

	dapt->bcn_size[index]++;
	if (dapt->bcn_size[index] > DAPT_MAX_RSSI_SAMPLE) {
		SAM_STEP(dapt->bcn_read[index], DAPT_MAX_RSSI_SAMPLE);
	}
}

static void dapt_clr_accum(struct img_priv *priv, int index)
{
	struct dapt_params *dapt = &priv->dapt_params;

	if (INVALID_VIF_IDX(index))
		return;

	RPU_DEBUG_DAPT("%s\n", __func__);

	dapt->thresh_accum[index] = 0;
	dapt->avg_thresh[index] = 0;
	dapt->new_thresh[index] = 0;
}

/*
 * Input:
 * phy_threshold:  current  PHY  start  threshold  in  negative  half-dB  steps  (e.g.,  180  means  -90  dBm)
 * rss: received signal strength expressed as  the number of half-dB steps above -110 dBm (e.g., 40 means -90  dBm)
 * adapt_thresh_offset:  the  number  of  half-dB  steps  below  rss  to  set  the  PHY  start  threshold
 * adapt_thresh_exponent:  e  in  the  adaptation  equation  thresh(n)  =  ((2^e  ¡§C  1)/(2^e))*thresh(n-1)  + 1/(2^e)*x(n)
 * where x(n) = (rss ¡§C adapt_thresh_offset) for Packet n
 * Output:
 * phy_threshold: updated PHY start threshold
 */
static void dapt_calc_accum(struct img_priv *priv, s8 rss, int index)
{
	struct dapt_params *dapt = &priv->dapt_params;
	unsigned int update, b, c_rss;
	int adapt_thresh_offset = priv->params->dapt_thresh_offset;
	int adapt_thresh_exponent = priv->params->dapt_thresh_exponent;

	if (INVALID_VIF_IDX(index))
		return;

	if (rss < -110)
		rss = -110;

	// convert rss from dBm
	c_rss = (110 + rss) * 2;

	if (c_rss > adapt_thresh_offset)
		update = (c_rss - adapt_thresh_offset) << 4;
	else
		update = 0;
	b = (1 << adapt_thresh_exponent) - 1;

	if (!dapt->thresh_accum[index])
		dapt->thresh_accum[index] = update << adapt_thresh_exponent;

	dapt->thresh_accum[index] = b * (dapt->thresh_accum[index] >> adapt_thresh_exponent) + update;

	//RPU_DEBUG_DAPT("rss = %d, c_rss = %d, update = %d, accum = %d\n",
	//			rss, c_rss, update,
	//			dapt->thresh_accum);
}

static void dapt_calc_new_thresh(struct img_priv *priv, int index)
{
	struct dapt_params *dapt = &priv->dapt_params;
	int adapt_thresh_exponent = 4;

	if (INVALID_VIF_IDX(index))
		return;

	dapt->avg_thresh[index] = dapt->thresh_accum[index] >> (adapt_thresh_exponent + 4);
	dapt->new_thresh[index] = max((unsigned int)priv->params->dapt_thresh_min,
				min(220 - dapt->avg_thresh[index], (unsigned int)priv->params->dapt_thresh_max));

	RPU_DEBUG_DAPT("index %d: accum = %d, avg = %d, new = %d\n", index,
				dapt->thresh_accum[index], dapt->avg_thresh[index], dapt->new_thresh[index]);
}

void dapt_save_history_thresh(struct img_priv *priv, int thresh, int channel)
{
	struct dapt_params *dapt = &priv->dapt_params;

	if (thresh >= dapt->last_thresh + 10 ||
		thresh <= dapt->last_thresh - 10) {
		RPU_DEBUG_DAPT("cur thresh = %d, last thresh = %d\n", thresh, dapt->last_thresh);
	}
	dapt->last_thresh = thresh;

	dapt->thr_history[channel][dapt->cur_thr_offset[channel]] = thresh;
	SAM_STEP(dapt->cur_thr_offset[channel], DAPT_SETED_PHY_THRESH_COUNT);
}

bool dapt_need_update_thresh(struct img_priv *priv, int thresh, int channel)
{
	struct dapt_params *dapt = &priv->dapt_params;

	if (dapt->cur_seted_thresh[channel] != thresh) {
		return true;
	}

	return false;
}

int dapt_set_phy_thresh(struct img_priv *priv, int thresh, int ch, int set)
{
	struct dapt_params *dapt = &priv->dapt_params;
	int channel;
	int need_update = 0;

	if (dapt->dapt_disable) {
		return 0;
	}

	if (!thresh)
		return 0;

	if (set)
		RPU_DEBUG_DAPT("%s: thresh = %d\n", __func__, thresh);

#ifdef DAPT_ENABLE_SCAN
	if (priv->params->hw_scan_status == HW_SCAN_STATUS_PROGRESS) {
		if (thresh != DAPT_SCAN_PHY_THRESH &&
			thresh != DAPT_NON_STA_CONN_PHY_THRESH) {
			return 0;
		}
	}
#endif

	thresh = max((unsigned int)priv->params->dapt_thresh_min,
			min((unsigned int)thresh, (unsigned int)priv->params->dapt_thresh_max));

	if (ch == -1) {
		channel = CURRENT_CHANNEL(priv);
	} else {
		channel = ch;
	}
	if (channel > 14 || channel < 1) {
		return 0;
	}

	if (dapt_need_update_thresh(priv, thresh, channel-1)) {
		dapt->cur_seted_thresh[channel-1] = thresh;
		need_update = 1;

		//set thresh to rpu
		if (set)
			rpu_prog_phy_thresh(dapt->cur_seted_thresh);

		dapt_save_history_thresh(priv, thresh, channel-1);
	}

	return need_update;
}

void dapt_save_cur_thresh_all(struct img_priv *priv)
{
	struct dapt_params *dapt = &priv->dapt_params;

	memcpy(dapt->save_seted_thresh, dapt->cur_seted_thresh, 14*sizeof(unsigned int));
}

void dapt_restore_thresh_all(struct img_priv *priv)
{
	int i, need_update = 0;
	struct dapt_params *dapt = &priv->dapt_params;

	for (i = 0; i < 14; i++) {
		need_update += dapt_set_phy_thresh(priv, dapt->save_seted_thresh[i], i+1, 0);
	}
	if (need_update)
		rpu_prog_phy_thresh(dapt->cur_seted_thresh);
}

void dapt_set_phy_thresh_all(struct img_priv *priv, int thresh)
{
	int i, need_update = 0;
	struct dapt_params *dapt = &priv->dapt_params;

	for (i = 0; i < 14; i++) {
		need_update += dapt_set_phy_thresh(priv, thresh, i+1, 0);
	}
	if (need_update)
		rpu_prog_phy_thresh(dapt->cur_seted_thresh);
}

static void dapt_process(struct img_priv *priv, int index)
{
	struct dapt_params *dapt = &priv->dapt_params;
	int i;

	if (INVALID_VIF_IDX(index))
		return;

	RPU_DEBUG_DAPT("%s: index %d sam_size = %d\n", __func__, index, dapt->sam_size[index]);

	if (dapt->dapt_disable) {
		return;
	}

	dapt_clr_accum(priv, index);

	if (dapt->sam_size[index] < DAPT_MIN_RSSI_SAMPLE) {
		/* if no rx frames, but have lots of tx frame, like wfd source or udp tx
		 * use beacon rssi to calculate
		*/
		if (dapt->bcn_size[index] >= DAPT_MIN_RSSI_SAMPLE) {
			RPU_DEBUG_DAPT("%s: use beacon rssi sam_size = %d\n",
					__func__, dapt->bcn_size[index]);
			dapt->sam_size[index] = dapt->bcn_size[index];
			dapt->sam_read[index] = dapt->bcn_read[index];
			dapt->sam_write[index] = dapt->bcn_write[index];
			memcpy(dapt->sam_history[index],
					dapt->bcn_history[index], dapt->sam_size[index]);
		} else {
			dapt_clr_history(priv, index);
			return;
		}
	}

	if (dapt->sam_size[index] > DAPT_MAX_RSSI_SAMPLE)
		dapt->sam_size[index] = DAPT_MAX_RSSI_SAMPLE;

	for (i = 0; i < dapt->sam_size[index]; i++) {
		dapt_calc_accum(priv, dapt_get_sample(priv, index), index);
	}

	dapt_clr_history(priv, index);

	dapt_calc_new_thresh(priv, index);
}

void dapt_timer_handler(struct img_priv *priv)
{
	struct dapt_params *dapt = &priv->dapt_params;

	if (dapt->dapt_disable) {
		return;
	}

	spin_lock_bh(&priv->dapt_lock);

#ifdef DAPT_ENABLE_SCAN
	if (priv->params->hw_scan_status == HW_SCAN_STATUS_PROGRESS) {
		goto handle_out;
	}
#endif

	if (INVALID_VIF_IDX(dapt->main_index) &&
		INVALID_VIF_IDX(dapt->p2p_index))
		goto handle_out;

	dapt_process(priv, dapt->main_index);
	dapt_process(priv, dapt->p2p_index);

	if (INVALID_VIF_IDX(dapt->main_index)) {
		dapt_set_phy_thresh(priv, dapt->new_thresh[dapt->p2p_index], -1, 1);
		goto handle_out;
	} else if (INVALID_VIF_IDX(dapt->p2p_index)) {
		dapt_set_phy_thresh(priv, dapt->new_thresh[dapt->main_index], -1, 1);
		goto handle_out;
	}

	if (!dapt->new_thresh[dapt->main_index] &&
		!dapt->new_thresh[dapt->p2p_index]) {
		dapt_clr_history(priv, dapt->main_index);
		dapt_clr_history(priv, dapt->p2p_index);

		dapt->both_zero_count++;
		if (dapt->both_zero_count > 5) {
			dapt->both_zero_count = 0;
			RPU_DEBUG_DAPT("%s: set default thresh because of no"
					" rx in both interface\n", __func__);
			/*
			* when no rx frames after connected, set back defult phy thresh
			* this is to compatible wlan0/p2p coexit case like:
			* 1. when wlan0 is connected with high phy thresh (like 90),
			*     maybe hard to do p2p connect at this time because of improper phy thresh
			* 2. when p2p0 is connected phy high thresh (like 90),
			*     maybe hard to do wlan0 connect at this time because of improper phy thresh
			* 3. wlan0 and p2p0 both connected, there is no rx on wlan0,
			*     maybe block p2p throughput of improper phy thresh
			*/
			dapt_set_phy_thresh_all(priv, DAPT_DEFAULT_PHY_THRESH);
		}
	} else {
		unsigned int new_thresh =
				dapt->new_thresh[dapt->main_index] > dapt->new_thresh[dapt->p2p_index] ?
				dapt->new_thresh[dapt->main_index] : dapt->new_thresh[dapt->p2p_index];

		dapt->both_zero_count = 0;
		dapt_set_phy_thresh(priv, new_thresh, -1, 1);
	}

handle_out:

	dapt_start_timer(priv, DAPT_CALC_INTERVAL);

	spin_unlock_bh(&priv->dapt_lock);
}

static bool dapt_is_target_frame(struct img_priv *priv, struct ieee80211_hdr *hdr)
{
	struct dapt_params *dapt = &priv->dapt_params;

	if (dapt->dapt_disable)
		return false;

	if (priv->sniffer)
		return false;

	if (ieee80211_is_probe_req(hdr->frame_control) ||
		ieee80211_is_probe_resp(hdr->frame_control) ||
		ieee80211_is_beacon(hdr->frame_control)) {
		return false;
	}

	return true;
}

/*
 * set defaut phy thresh when scan
 * and stop dapt
 */
void dapt_scan(struct img_priv *priv)
{
#ifdef DAPT_ENABLE_SCAN
	RPU_DEBUG_DAPT("%s\n", __func__);

	spin_lock_bh(&priv->dapt_lock);

	dapt_save_cur_thresh_all(priv);
	if (priv->p2p_scan)
		dapt_set_phy_thresh_all(priv, DAPT_P2P_SCAN_PHY_THRESH);
	else
		dapt_set_phy_thresh_all(priv, DAPT_SCAN_PHY_THRESH);

	spin_unlock_bh(&priv->dapt_lock);
#endif	
}

/* restart dapt */
void dapt_scan_complete(struct img_priv *priv)
{
#ifdef DAPT_ENABLE_SCAN
	RPU_DEBUG_DAPT("%s\n", __func__);

	spin_lock_bh(&priv->dapt_lock);

	dapt_restore_thresh_all(priv);

	spin_unlock_bh(&priv->dapt_lock);
#endif	
}

void dapt_notify_bssid_change(struct img_priv *priv,
					int index,
					unsigned char *vif_addr,
					unsigned char *bssid)
{
	struct dapt_params *dapt = &priv->dapt_params;

	RPU_DEBUG_DAPT("%s: index = %d, vif_addr = %pM, bssid = %pM\n",
			__func__, index, vif_addr, bssid);

	if (INVALID_VIF_IDX(index))
		return;

	if (vif_addr)
		memcpy(dapt->vif_addr[index], vif_addr, ETH_ALEN);
	else
		memset(dapt->vif_addr[index], 0, ETH_ALEN);
	if (bssid)
		memcpy(dapt->bssid[index], bssid, ETH_ALEN);
	else
		memset(dapt->bssid[index], 0, ETH_ALEN);
}

void dapt_notify_conn_state(struct img_priv *priv,
					int index,
					unsigned char *vif_addr,					
					unsigned int connect_state)
{
	struct dapt_params *dapt = &priv->dapt_params;

	RPU_DEBUG_DAPT("%s: index = %d, vif_addr = %pM, connect_state = %s\n",
			__func__, index, vif_addr, connect_state==STA_CONN ? "CONN":"DISCONN");

	if (INVALID_VIF_IDX(index))
		return;

	memcpy(dapt->vif_addr[index], vif_addr, ETH_ALEN);
	if (connect_state == STA_CONN) {
		dapt->conn_state[index] = 1;
	} else {
		dapt->conn_state[index] = 0;
	}

	dapt_find_main_iface(priv);
	dapt_find_p2p_iface(priv);
	RPU_DEBUG_DAPT("%s, main_index = %d, p2p_index = %d\n", __func__, dapt->main_index, dapt->p2p_index);
}

static void dapt_rx(struct img_priv *priv, struct ieee80211_hdr * hdr)
{
	if (dapt_is_target_frame(priv, hdr)) {
		int index = -1;
		struct dapt_params *dapt = &priv->dapt_params;
		u8 *bssid = ieee80211_get_BSSID(hdr);

		//RPU_DEBUG_DAPT("BSSID: %pM, SA: %pM, DA: %pM\n",
		//			bssid, ieee80211_get_SA(hdr), ieee80211_get_DA(hdr));
		if (bssid) {
			if (!INVALID_VIF_IDX(dapt->main_index)) {
				if (ether_addr_equal(bssid, dapt->bssid[dapt->main_index]) ||
					ether_addr_equal(bssid, dapt->vif_addr[dapt->main_index])) { // wlan0
					index = dapt->main_index;
				}
			}
			if (!INVALID_VIF_IDX(dapt->p2p_index)) {
				if (ether_addr_equal(bssid, dapt->bssid[dapt->p2p_index]) ||
					ether_addr_equal(bssid, dapt->vif_addr[dapt->p2p_index])) { // p2p0
					index = dapt->p2p_index;
				}
			}

			if (index != -1) {
				spin_lock_bh(&priv->dapt_lock);
				dapt_set_sample(priv, rx_control_info.rssi, index);
				spin_unlock_bh(&priv->dapt_lock);
			}
		}
	}
}

void dapt_beacon(struct img_priv *priv, s8 rssi, int index)
{
	struct dapt_params *dapt = &priv->dapt_params;

	if (rssi >= 0)
		return;

	// there is beacon only in station or p2p GC mode
	if (!dapt->conn_state[index])
		return;

	//if (net_ratelimit())
	//	RPU_DEBUG_DAPT("%s: idx %d rssi %d\n", __func__, index, rssi);

	spin_lock_bh(&priv->dapt_lock);
	dapt_set_bcn_sample(priv, rssi, index);
	spin_unlock_bh(&priv->dapt_lock);
}
#endif

static u64 get_systime_us(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
	struct timespec64 ts;
#else
	struct timespec ts;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
	ts = ktime_to_timespec64(ktime_get_boottime());
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
	ts = ktime_to_timespec(ktime_get_boottime());
#else
	get_monotonic_boottime(&ts);
#endif
	return ((u64)ts.tv_sec * 1000000) + ts.tv_nsec / 1000;
#else
	struct timeval tv;

	do_gettimeofday(&tv);
	return ((u64)tv.tv_sec * 1000000) + tv.tv_usec;
#endif
}

void rpu_add_scan_resp_timestamp(struct ieee80211_hdr *hdr)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)hdr;

	if (ieee80211_is_beacon(hdr->frame_control)) {
		mgmt->u.beacon.timestamp = cpu_to_le64(get_systime_us());
	}

	if (ieee80211_is_probe_resp(hdr->frame_control)) {
		mgmt->u.probe_resp.timestamp = cpu_to_le64(get_systime_us());
	}
}

void rpu_rx_frame(struct sk_buff *skb, void *context)
{
	struct img_priv *priv = (struct img_priv *)context;
	struct ieee80211_hdr *hdr;
	struct ieee80211_rx_status rx_status;
	struct ieee80211_supported_band *band = NULL;
	int i;
	static unsigned int rssi_index;
	unsigned char mic_status;

	memcpy(&rx_control_info, skb->data, sizeof(struct wlan_rx_pkt));
        /* Remove RX control information:
         * unused more_cmd_data in RX direction is used to indicate QoS/Non-Qos
         * frames
         */
        if (rx_control_info.hdr.more_cmd_data == 0) {
                /* Non-QOS case*/
                skb_pull(skb, sizeof(struct wlan_rx_pkt));
        } else {
                /* Qos Case: The RPU overwrites the 2 reserved bytes with data
                 * to maintain the 4 byte alignment of total length and 2 byte
                 * alignment
                 * of starting address (as expected by mac80211).
                 */
                skb_pull(skb, sizeof(struct wlan_rx_pkt) - 2);
                skb_trim(skb, skb->len - 2);
        }

	dump_ieee80211_hdr_info(skb->data, skb->len, 0);

	hdr = (struct ieee80211_hdr *)skb->data;

#ifdef ENABLE_DAPT
	dapt_rx(priv, hdr);
#endif

	rpu_add_scan_resp_timestamp(hdr);

	/* Stats for debugging */
	if (ieee80211_is_data(hdr->frame_control)) {
		priv->stats->rx_packet_data_count++;
	} else if (ieee80211_is_mgmt(hdr->frame_control)) {
		priv->stats->rx_packet_mgmt_count++;
	}

	memset(&rx_status, 0, sizeof(struct ieee80211_rx_status));

	/* Remove this once hardware supports bip(11w) is available*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	if (!ieee80211_is_robust_mgmt_frame(skb))
#else
	if (!ieee80211_is_robust_mgmt_frame(hdr))
#endif
		rx_status.flag |= RX_FLAG_DECRYPTED;

	rx_status.flag |= RX_FLAG_MMIC_STRIPPED;

	mic_status = rx_control_info.rx_pkt_status;

	if (mic_status == RX_MIC_FAILURE_TKIP) {
		rx_status.flag |= RX_FLAG_MMIC_ERROR;
	} else if (mic_status == RX_MIC_FAILURE_CCMP) {
		RPU_INFO_RX("%s: Drop the Frame\n", __func__);
		/*Drop the Frame*/
		dev_kfree_skb_any(skb);
		return;
	}

	if (rx_control_info.channel < 15) {
		rx_status.band = IEEE80211_BAND_2GHZ;
	} else {
		WARN_ON_ONCE(1);
		rx_status.band = IEEE80211_BAND_5GHZ;
	}

	rx_status.freq = ieee80211_channel_to_frequency(rx_control_info.channel,
							rx_status.band);
	rx_status.signal = rx_control_info.rssi;

	/* RSSI Average for Production Mode*/
	if (priv->params->production_test == 1) {
		priv->params->rssi_average[rssi_index++] = (char)(rx_control_info.rssi);
		if (rssi_index >= MAX_RSSI_SAMPLES)
			rssi_index = 0;
	}

	rx_status.antenna = 0;

#if 0
	if (ieee80211_is_data(hdr->frame_control)) {
		unsigned char *ccmp = (unsigned char *)hdr + ieee80211_hdrlen(hdr->frame_control);
		unsigned char PN[8];
		u8 *DA = ieee80211_get_DA(hdr);
		u8 *SA = ieee80211_get_SA(hdr);
		unsigned int txif_status;
		unsigned int rxif_status;

		memcpy(&txif_status, rx_control_info.timestamp, 4);
		memcpy(&rxif_status, rx_control_info.timestamp + 4, 4);
		PN[0] = ccmp[0];
		PN[1] = ccmp[1];
		PN[2] = ccmp[4];
		PN[3] = ccmp[5];
		PN[4] = ccmp[6];
		PN[5] = ccmp[7];
		PN[6] = 0;
		PN[7] = 0;
		pr_info("SA: %pM -> DA: %pM FRAG %d SEQ %d rssi %d %s txif %x rxif %x\n",
			SA, DA, hdr->seq_ctrl&0x000F, hdr->seq_ctrl>>4, rx_status.signal,
			ieee80211_has_retry(hdr->frame_control)?"retry":"", txif_status, rxif_status);
	}
#endif	

	if (rx_control_info.rate_flags & ENABLE_11N_FORMAT) {
		/* Rate */
		if ((rx_control_info.rate_or_mcs & MARK_RATE_AS_MCS_INDEX) != 0x80) {
			RPU_DEBUG_RX("Invalid HT MCS Information\n");
			rx_control_info.rate_or_mcs = 0;/*default to MCS0*/
		} else {
			rx_status.rate_idx = (rx_control_info.rate_or_mcs & 0x7f);
		}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
		rx_status.encoding = RX_ENC_HT;
#else
		rx_status.flag |= RX_FLAG_HT;
#endif
	} else {
		band = priv->hw->wiphy->bands[rx_status.band];

		if (!WARN_ON_ONCE(!band)) {
			for (i = 0; i < band->n_bitrates; i++) {
				if (rx_control_info.rate_or_mcs ==
				    band->bitrates[i].hw_value) {
					rx_status.rate_idx = i;
					break;
				}
			}
		} else {
			RPU_DEBUG_DUMP_RX(" ", DUMP_PREFIX_NONE, 16, 1,
				 &rx_control_info, sizeof(struct wlan_rx_pkt), 1);
			RPU_INFO_RX("%s: Drop the Frame(band=%p)\n", __func__, band);
			dev_kfree_skb_any(skb);
			return;
		}
	}

	if (((hdr->frame_control & IEEE80211_FCTL_FTYPE) ==
	     IEEE80211_FTYPE_MGMT) &&
	    ((hdr->frame_control & IEEE80211_FCTL_STYPE) ==
	     IEEE80211_STYPE_BEACON)) {
		rx_status.mactime = get_unaligned_le64(rx_control_info.timestamp);
		rx_status.flag |= RX_FLAG_MACTIME_START;
	}

	RPU_DEBUG_RX(
		      "%s-RX: RX frame, length = %d, RSSI = %d, rate = %d\n",
		      priv->name,
		      skb->len,
		      rx_status.signal/*rx_control_info.rssi*/,
		      rx_control_info.rate_or_mcs);

	RPU_DEBUG_DUMP_RX(" ",
			DUMP_PREFIX_NONE, 16, 1,
			skb->data, (skb->len>64)?64:skb->len, 1);

	memcpy(IEEE80211_SKB_RXCB(skb), &rx_status, sizeof(rx_status));
	local_bh_disable();
	ieee80211_rx(priv->hw, skb);
	local_bh_enable();
}




