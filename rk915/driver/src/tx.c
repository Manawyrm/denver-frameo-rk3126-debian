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

#define TX_TO_MACDEV(x) ((struct img_priv *) \
			 (container_of(x, struct img_priv, tx)))

#ifdef STA_AP_COEXIST
static int find_ie(u8 *frame, int len, int ie, int ie_start)
{
	int offset = 0;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)frame;

	offset += ieee80211_hdrlen(hdr->frame_control);
	offset += ie_start;
	while(offset < len) {
		if (frame[offset] == ie) {
			return offset;
		}
		offset += frame[offset+1] + 2;
	}

	return 0;
}

static void change_channel(u8 *frame, int len, int ch, u8 ie_id)
{
	int offset;
	
	offset = find_ie(frame, len, ie_id, 12);
	if (offset)
		frame[offset + 2] = ch;
}

static void adjust_beacon_ie(struct img_priv *priv, struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
    
	if (ieee80211_is_beacon(hdr->frame_control) ||
	    ieee80211_is_probe_resp(hdr->frame_control)) {
		// change the channel of DSPS IE
		change_channel(skb->data, skb->len, priv->pri_chnl_num, 0x3);
		// change the Primary Channel in HT Info
		change_channel(skb->data, skb->len, priv->pri_chnl_num, 0x3d);
	}
}
#endif

void rpu_unblock_all_frames(struct img_priv *priv,
					    int ch_id)
{
	int txq_len = 0;
	int i = 0, cnt = 0;
	int queue = 0;
	int curr_bit = 0;
	int pool_id = 0;
	int ret = 0;
	int start_ac, end_ac;
	unsigned int pkts_pend = 0;
	struct tx_config *tx = NULL;
	struct sk_buff_head *txq = NULL;

	tx = &priv->tx;

	for (i = 0; i < NUM_ACS; i++) {
		if (tx->queue_stopped_bmp & (1 << i))
		{
			ieee80211_wake_queue(priv->hw, tx_queue_unmap(i));
			tx->queue_stopped_bmp &= ~(1 << (i));
		}
	}

	for (i = 0; i < NUM_TX_DESCS; i++) {
		spin_lock_bh(&tx->lock);

		curr_bit = (i % TX_DESC_BUCKET_BOUND);
		pool_id = (i / TX_DESC_BUCKET_BOUND);

		if (test_and_set_bit(curr_bit, &tx->buf_pool_bmp[pool_id])) {
			spin_unlock_bh(&tx->lock);
			continue;
		}

		txq = &tx->pkt_info[i].pkt;
		txq_len = skb_queue_len(txq);

		/* Not valid when txq len is 0 */
		queue = tx->pkt_info[i].queue;

		if (!txq_len) {
			/* Reserved token */
			if (i < (NUM_TX_DESCS_PER_AC * NUM_ACS)) {
				queue = (i % NUM_ACS);
				start_ac = end_ac = queue;
			} else {
				/* Spare token:
				 * Loop through all AC's
				 */
				start_ac = WLAN_AC_VO;
				end_ac = WLAN_AC_BK;
			}

			for (cnt = start_ac; cnt >= end_ac; cnt--) {
				pkts_pend = rpu_tx_proc_pend_frms(priv,
									  cnt,
									  i);
				if (pkts_pend) {
					queue = cnt;
					break;
				}
			}

			if (pkts_pend == 0) {
				__clear_bit(curr_bit,
					    &tx->buf_pool_bmp[pool_id]);
				spin_unlock_bh(&tx->lock);
				continue;
			}
		}

		tx->outstanding_tokens[queue]++;
		spin_unlock_bh(&tx->lock);

		ret = __rpu_tx_frame(priv,
					     queue,
					     i,
					     0,
					     0); /* TODO: Currently sending 0
						  * since this param is not used
						  * as expected in the orig
						  * code for multiple frames etc
						  * Need to set this
						  * properly when the orig code
						  * logic is corrected
						  */
		if (ret < 0) {
			RPU_ERROR_TX("%s: Queueing of TX frame to FW failed\n",
			       __func__);
		}
	}
}

static void wait_for_tx_complete(struct tx_config *tx)
{
	int count = 0;
	struct img_priv *priv = TX_TO_MACDEV(tx);

	/*if (priv->fw_error)
		return;*/

	/* Find_last_bit: Returns the bit number of the first set bit,
	 * or size.
	 */
	while (find_last_bit(tx->buf_pool_bmp,
			     NUM_TX_DESCS) != NUM_TX_DESCS) {
		count++;

		if (count < TX_COMPLETE_TIMEOUT_TICKS) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(1);
		} else {
			RPU_DEBUG_TX("%s-UMACTX:WARNING: ", priv->name);
			RPU_DEBUG_TX("TX complete failed!!\n");
			RPU_DEBUG_TX("%s-UMACTX:After ", priv->name);
			RPU_DEBUG_TX("%ld: bitmap is: 0x%lx\n",
			       TX_COMPLETE_TIMEOUT_TICKS,
			       tx->buf_pool_bmp[0]);
			break;
		}
	}

	if (count && (count < TX_COMPLETE_TIMEOUT_TICKS)) {
		RPU_DEBUG_TX("%s-UMACTX:TX complete after %d timer ticks\n",
			priv->name, count);
	}
}

int tx_queue_map(int queue)
{
	unsigned int ac[4] = {WLAN_AC_VO, WLAN_AC_VI, WLAN_AC_BE, WLAN_AC_BK};
	//unsigned int ac[4] = {WLAN_AC_VO, WLAN_AC_VO, WLAN_AC_VO, WLAN_AC_VO};

	if (queue < 4)
		return ac[queue];

	return WLAN_AC_VO;
}

int tx_queue_unmap(int queue)
{
	unsigned int ac[4] = {3, 2, 1, 0};

	return ac[queue];
}


static int check_80211_aggregation(struct img_priv *priv,
				struct sk_buff *skb,
				struct sk_buff *skb_first)
{

	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);
	struct ieee80211_hdr *mac_hdr = NULL, *mac_hdr_first = NULL;
	//struct sk_buff *skb_first;
	//struct sk_buff_head *pend_pkt_q = NULL;
	//struct tx_config *tx = &priv->tx;
	bool ampdu = false, is_qos = false, addr = true;

	mac_hdr = (struct ieee80211_hdr *)skb->data;
	//pend_pkt_q = &tx->pending_pkt[peer_id][ac];
	//skb_first = skb_peek(pend_pkt_q);
	if (skb_first)
		mac_hdr_first = (struct ieee80211_hdr *)skb_first->data;

	ampdu = (tx_info->flags & IEEE80211_TX_CTL_AMPDU) ? true : false;
	is_qos = ieee80211_is_data_qos(mac_hdr->frame_control);

	/* RPU has a limitation, it expects A1-A2-A3 to be same
	* for all MPDU's within an AMPDU. This is a temporary
	* solution, remove it when RPU has fix for this.
	*/
	if (skb_first &&
	    ((!ether_addr_equal(mac_hdr->addr1,
		       mac_hdr_first->addr1)) ||
	    (!ether_addr_equal(mac_hdr->addr2,
		       mac_hdr_first->addr2)) ||
	    (!ether_addr_equal(mac_hdr->addr3,
		       mac_hdr_first->addr3)))) {
		addr = false;
	}

	/*stats and debug*/
	if (!is_qos) {
		RPU_DEBUG_TX("Not Qos\n");
		priv->stats->tx_noagg_not_qos++;
	} else if (!ampdu) {
		RPU_DEBUG_TX("Not AMPDU\n");
		priv->stats->tx_noagg_not_ampdu++;
	} else if (!addr) {
		if (skb_first) {
			RPU_DEBUG_TX("first: A1: %pM-A2:%pM -A3%pM not same\n",
				      mac_hdr_first->addr1,
				      mac_hdr_first->addr2,
				      mac_hdr_first->addr3);
			RPU_DEBUG_TX("curr:  A1: %pM-A2:%pM -A3%pM not same\n",
				      mac_hdr->addr1,
				      mac_hdr->addr2,
				      mac_hdr->addr3);
		}
		priv->stats->tx_noagg_not_addr++;
	}

	return (ampdu && is_qos && addr);
}


static void tx_status(struct sk_buff *skb,
		      struct umac_event_tx_done *tx_done,
		      unsigned int frame_idx,
		      struct img_priv *priv,
		      struct ieee80211_tx_info tx_info_1st_mpdu)
{
	int index, i;
	char idx = 0;
	struct ieee80211_tx_rate *txrate;
	struct ieee80211_tx_rate *tx_inf_rate = NULL;
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);
	int tx_fixed_mcs_idx = 0;
	int tx_fixed_rate = 0;
	struct ieee80211_supported_band *band = NULL;
	struct umac_vif *uvif = NULL;
	int ret = 0;

	uvif = (struct umac_vif *)(tx_info->control.vif->drv_priv);

	/*Just inform ma8c0211, it will free the skb*/
	if (tx_done->frm_status[frame_idx] == TX_DONE_STAT_DISCARD) {
		ieee80211_free_txskb(priv->hw, skb);
		priv->stats->tx_dones_to_stack++;
		return;
	}

	/* Rate info will be retained, except the count*/
	ieee80211_tx_info_clear_status(tx_info);

	if (tx_done->retries_num[frame_idx] > 0) {
		//RPU_INFO_TX("retries_num[%d] %d\n", frame_idx, tx_done->retries_num[frame_idx]);
		priv->tx_retry_frm_cnt++;
	}

	if (tx_done->frm_status[frame_idx] == TX_DONE_STAT_SUCCESS)
		tx_info->flags |= IEEE80211_TX_STAT_ACK;
	else if (tx_info->flags & IEEE80211_TX_CTL_AMPDU)
		tx_info->flags |= IEEE80211_TX_STAT_AMPDU_NO_BACK;

	tx_info->flags &= ~IEEE80211_TX_STAT_AMPDU;
	tx_info->flags &= ~IEEE80211_TX_CTL_AMPDU;

	band = priv->hw->wiphy->bands[tx_info->band];

	for (index = 0; index < 4; index++) {
		tx_inf_rate = &tx_info->status.rates[index];

		/* Populate tx_info based on 1st MPDU in an AMPDU */
		txrate = (&tx_info_1st_mpdu.control.rates[index]);

		if (txrate->idx < 0)
			break;

		if ((priv->params->production_test == 1) &&
		    ((priv->params->tx_fixed_mcs_indx != -1) ||
		     (priv->params->tx_fixed_rate != -1))) {
			tx_fixed_mcs_idx = priv->params->tx_fixed_mcs_indx;
			tx_fixed_rate = priv->params->tx_fixed_rate;

			/* This index is always zero */
			/* TODO: See if we need to send channel bw information
			 * taken from proc, since in Production mode the bw
			 * advised by Minstrel can be overwritten by proc
			 * settings
			 */
			tx_inf_rate->flags = txrate->flags;

			if (tx_fixed_mcs_idx != -1) {
				if (priv->params->prod_mode_rate_flag & ENABLE_11N_FORMAT) {
					tx_inf_rate->flags |=
						IEEE80211_TX_RC_MCS;
					/* So that actual sent rate is seen in
					 * sniffer
					 */
					idx = tx_done->rate[frame_idx] & 0x7F;
					tx_inf_rate->idx = idx;
				} else 
				if (tx_fixed_rate != -1) {
					for (i = 0; i < band->n_bitrates; i++) {
						if ((band->bitrates[i]).hw_value ==
						    tx_done->rate[frame_idx])
							tx_inf_rate->idx = i;
					}
				}
			}

			tx_inf_rate->count = (tx_done->retries_num[frame_idx] +
					      1);
			break;
		}

		if ((tx_done->rate[frame_idx] &
		     MARK_RATE_AS_MCS_INDEX) == 0x80) {
			if ((txrate->flags & IEEE80211_TX_RC_MCS) &&
				   ((tx_done->rate[frame_idx] & 0x7F) ==
				    (txrate->idx & 0x7F))) {
				tx_inf_rate->count =
					(tx_done->retries_num[frame_idx] + 1);
			}

			break;
		} else if (tx_done->rate[frame_idx] ==
			   (band->bitrates[tx_inf_rate->idx]).hw_value) {
			tx_inf_rate->count =
				(tx_done->retries_num[frame_idx] + 1);

			break;
		}
	}

	/* Invalidate the remaining indices */
	while (((index + 1) < 4)) {
		tx_info->status.rates[index + 1].idx = -1;
		tx_info->status.rates[index + 1].count = 0;
		index++;
	}

	if (((tx_info->flags & IEEE80211_TX_CTL_TX_OFFCHAN)
	    ) &&
	    (atomic_dec_return(&priv->roc_params.roc_mgmt_tx_count) == 0)) {
		RPU_DEBUG_ROC("%s:%d TXDONE Frame: %d\n",
			__func__,
			__LINE__,
			atomic_read(&priv->roc_params.roc_mgmt_tx_count));
		if (priv->roc_params.roc_in_progress &&
		    priv->roc_params.roc_type == ROC_TYPE_OFFCHANNEL_TX) {
			CALL_RPU(rpu_prog_roc, ROC_STOP, 0, 0, 0);
			RPU_DEBUG_ROC("%s:%d", __func__, __LINE__);
			RPU_DEBUG_ROC("all offchan pending frames cleared\n");
		}
	}

	priv->stats->tx_dones_to_stack++;

	ieee80211_tx_status_skb(priv->hw, skb);
prog_rpu_fail:
	return;
}


int get_token(struct img_priv *priv,
		     int queue)
{
	int cnt = 0;
	int curr_bit = 0;
	int pool_id = 0;
	int token_id = NUM_TX_DESCS;
	struct tx_config *tx = &priv->tx;

	/* First search for a reserved token */
	for (cnt = 0; cnt < NUM_TX_DESCS_PER_AC; cnt++) {
		curr_bit = ((queue + (NUM_ACS * cnt)) % TX_DESC_BUCKET_BOUND);
		pool_id = ((queue + (NUM_ACS * cnt)) / TX_DESC_BUCKET_BOUND);

		if (!test_and_set_bit(curr_bit, &tx->buf_pool_bmp[pool_id])) {
			token_id = queue + (NUM_ACS * cnt);
			tx->outstanding_tokens[queue]++;
			break;
		}
	}

	/* If reserved token is not found search for a spare token
	 * (only for non beacon queues)
	 */
	if ((cnt == NUM_TX_DESCS_PER_AC) && (queue != WLAN_AC_BCN)) {
		for (token_id = NUM_TX_DESCS_PER_AC * NUM_ACS;
		     token_id < NUM_TX_DESCS;
		     token_id++) {
			curr_bit = (token_id % TX_DESC_BUCKET_BOUND);
			pool_id = (token_id / TX_DESC_BUCKET_BOUND);
			if (!test_and_set_bit(curr_bit,
					      &tx->buf_pool_bmp[pool_id])) {
				tx->outstanding_tokens[queue]++;
				break;
			}
		}
	}

	tx->tx_desc_had_send_to_io[token_id] = 0;

	return token_id;
}

void free_token(struct img_priv *priv,
		int token_id,
		int queue)
{
	struct tx_config *tx = &priv->tx;
	int bit = -1;
	int pool_id = -1;
	int test = 0;
	unsigned int old_token = tx->outstanding_tokens[queue];

	//if (old_token <= 0)
	//	return;

	bit = (token_id % TX_DESC_BUCKET_BOUND);
	pool_id = (token_id / TX_DESC_BUCKET_BOUND);

	__clear_bit(bit, &tx->buf_pool_bmp[pool_id]);
	tx->outstanding_pkts[token_id] = 0;
	tx->tx_desc_had_send_to_io[token_id] = 0;

	if (tx->outstanding_tokens[queue] != 0)
		tx->outstanding_tokens[queue]--;

	test = tx->outstanding_tokens[queue];
	if (WARN_ON_ONCE(test < 0 || test > 4)) {
		RPU_ERROR_TX("%s: invalid outstanding_tokens: %d, old:%d\n",
			      __func__,
			      test,
			      old_token);
	}
}


struct curr_peer_info get_curr_peer_opp(struct img_priv *priv,
					int ac)
{
	unsigned int curr_peer_opp = 0;
	unsigned int curr_vif_op_chan = UMAC_VIF_CHANCTX_TYPE_OPER;
	unsigned int i = 0;
	struct tx_config *tx = NULL;
	unsigned int init_peer_opp = 0;
	struct curr_peer_info peer_info;
	unsigned int pend_q_len;
	struct sk_buff_head *pend_q = NULL;

	tx = &priv->tx;

	init_peer_opp = tx->curr_peer_opp[ac];
	/*TODO: Optimize this loop for BCN_Q
	 */
	for (i = 0; i < MAX_PEND_Q_PER_AC; i++) {
		curr_peer_opp = (init_peer_opp + i) % MAX_PEND_Q_PER_AC;

		pend_q = &tx->pending_pkt[curr_peer_opp][ac];
		pend_q_len = skb_queue_len(pend_q);

		if (pend_q_len) {
			tx->curr_peer_opp[ac] =
				(curr_peer_opp + 1) % MAX_PEND_Q_PER_AC;
			break;
		}
	}

	if (i == MAX_PEND_Q_PER_AC) {
		peer_info.id = -1;
		peer_info.op_chan_idx = -1;
	} else {
		peer_info.id = curr_peer_opp;
		peer_info.op_chan_idx = curr_vif_op_chan;
		RPU_DEBUG_TX("%s: Queue: %d Peer: %d op_chan: %d ",
			__func__,
			ac,
			curr_peer_opp,
			curr_vif_op_chan);
		RPU_DEBUG_TX("Pending: %d\n",
			pend_q_len);
	}

	return peer_info;
}

static int get_outstanding_pkts(struct tx_config *tx, int token_id)
{
	int i, count = 0;
	int bit = -1;
	int pool_id = -1;

	for (i = 0; i < NUM_TX_DESCS; i++) {
		if (i == token_id)
			continue;

		bit = (i % TX_DESC_BUCKET_BOUND);
		pool_id = (i / TX_DESC_BUCKET_BOUND);
		if (test_bit(bit, &tx->buf_pool_bmp[pool_id])) {
			//RPU_DEBUG_TX("%s: outstanding_pkts[%d] = %d\n",
			//	__func__, i, tx->outstanding_pkts[i]);
			count += tx->outstanding_pkts[i];
		}
	}
	if (count >= MAX_FW_TX_PKGS) {
		RPU_INFO_TX("%s: count(%d), reached MAX_FW_TX_PKGS(%d) (%d)\n",
					__func__, count, MAX_FW_TX_PKGS, token_id);
		count = MAX_FW_TX_PKGS;
	}
	return count;
}

int rpu_tx_proc_pend_frms(struct img_priv *priv,
				  int ac,
				  int token_id)
{
	struct tx_config *tx = &priv->tx;
	unsigned long ampdu_len = 0;
	struct sk_buff *loop_skb = NULL;
	struct sk_buff *tmp = NULL;
	struct ieee80211_hdr *mac_hdr = NULL;
	struct ieee80211_tx_info *tx_info = NULL;
	struct umac_vif *uvif = NULL;
	struct ieee80211_vif *ivif = NULL;
	unsigned char *data = NULL;
	unsigned int max_tx_cmds = priv->params->max_tx_cmds, fw_free_pkts;
	struct sk_buff_head *txq = NULL;
	struct sk_buff_head *pend_pkt_q = NULL;
	unsigned int total_pending_processed = 0;
	int pend_pkt_q_len = 0;
	struct curr_peer_info peer_info;
	int loop_cnt = 0;
	struct tx_pkt_info *pkt_info = NULL;
	struct sk_buff *skb_first;

	if (block_rpu_comm) {
		RPU_INFO_TX("%s: skip with block_rpu_comm\n", __func__);
		return 0;
	}

	peer_info = get_curr_peer_opp(priv,
				       ac);

	/* No pending frames for any peer in that AC.
	 */
	if (peer_info.id == -1)
		return 0;

	pend_pkt_q = &tx->pending_pkt[peer_info.id][ac];

	pkt_info = &priv->tx.pkt_info[token_id];
	txq = &pkt_info->pkt;

#if 0
	fw_free_pkts = max_tx_cmds;
#else
	fw_free_pkts = MAX_FW_TX_PKGS - get_outstanding_pkts(tx, token_id);
	if (fw_free_pkts == 0 && token_id != WLAN_AC_BCN) {
		return 0;
	}
#endif

	/* Aggregate Only MPDU's with same RA, same Rate,
	 * same Rate flags, same Tx Info flags
	 */
	skb_first = skb_peek(pend_pkt_q);
	skb_queue_walk_safe(pend_pkt_q,
			    loop_skb,
			    tmp) {
		data = loop_skb->data;
		mac_hdr = (struct ieee80211_hdr *)data;

		tx_info = IEEE80211_SKB_CB(loop_skb);

		ivif = tx_info->control.vif;
		uvif = (struct umac_vif *)(ivif->drv_priv);

		ampdu_len += loop_skb->len;

		if (!check_80211_aggregation(priv,
					     loop_skb,
					     skb_first) ||
		    (skb_queue_len(txq) >= max_tx_cmds) ||
		    (skb_queue_len(txq) >= fw_free_pkts)) {
			break;
		}
		loop_cnt++;
		__skb_unlink(loop_skb, pend_pkt_q);
		skb_queue_tail(txq, loop_skb);
	}

	/* If our criterion rejects all pending frames, or
	 * pend_q is empty, send only 1
	 */
	if (!skb_queue_len(txq))
		skb_queue_tail(txq, skb_dequeue(pend_pkt_q));

	total_pending_processed = skb_queue_len(txq);
	tx->outstanding_pkts[token_id] = total_pending_processed;

	pend_pkt_q_len = skb_queue_len(pend_pkt_q);
	if ((ac != WLAN_AC_BCN) &&
	    (tx->queue_stopped_bmp & (1 << ac)) &&
	    pend_pkt_q_len < (MAX_TX_QUEUE_LEN / 2)) {
		ieee80211_wake_queue(priv->hw, tx_queue_unmap(ac));
		tx->queue_stopped_bmp &= ~(1 << (ac));
	}

	pkt_info->peer_id = peer_info.id;
	RPU_DEBUG_TX("%s-UMACTX: token_id: %d ",
				priv->name,
				token_id);
	RPU_DEBUG_TX("total_pending_packets_process: %d\n",
		skb_queue_len(txq));

	return total_pending_processed;
}


int rpu_tx_alloc_token(struct img_priv *priv,
			       int ac,
			       int peer_id,
			       struct sk_buff *skb)
{
	int token_id = NUM_TX_DESCS;
	struct tx_config *tx = &priv->tx;
	struct sk_buff_head *pend_pkt_q = NULL;
	unsigned int pkts_pend = 0;
	struct ieee80211_tx_info *tx_info;
	struct sk_buff *skb_first;

	spin_lock_bh(&tx->lock);
	pend_pkt_q = &tx->pending_pkt[peer_id][ac];
	RPU_DEBUG_TX("%s-UMACTX:Alloc buf Req q = %d\n",
		      priv->name,
		      ac);
	RPU_DEBUG_TX("peerid: %d,\n", peer_id);

	/* Queue the frame to the pending frames queue */
	skb_queue_tail(pend_pkt_q, skb);

	tx_info = IEEE80211_SKB_CB(skb);

	if (tx->outstanding_tokens[ac] >= NUM_TX_DESCS_PER_AC) {
		bool agg_status = false;

		skb_first = skb_peek(pend_pkt_q);
		agg_status = check_80211_aggregation(priv,
						     skb,
						     skb_first);

		if (agg_status || !priv->params->enable_early_agg_checks) {
			int max_cmds = priv->params->max_tx_cmds;

			/* encourage aggregation to the max size
			 * supported (priv->params->max_tx_cmds)
			 */
			if (skb_queue_len(pend_pkt_q) < max_cmds) {
				RPU_DEBUG_TX("pend_q not full out_tok:%d\n",
					      tx->outstanding_tokens[ac]);
				goto out;
			 } else {
				RPU_DEBUG_TX("pend_q full out_tok:%d\n",
					      tx->outstanding_tokens[ac]);
			}
		}
	}

	/* Take steps to stop the TX traffic if we have reached
	 * the queueing limit.
	 * We dont this for the ROC queue to avoid the case where we are in the
	 * OFF channel but there is lot of traffic for the operating channel on
	 * the shared ROC queue (which is VO right now), since this would block
	 * ROC traffic too.
	 */
	if (skb_queue_len(pend_pkt_q) >= MAX_TX_QUEUE_LEN) {
		if ((!priv->roc_params.roc_in_progress) ||
		    (priv->roc_params.roc_in_progress &&
		     (ac != UMAC_ROC_AC))) {
			ieee80211_stop_queue(priv->hw,
					     skb->queue_mapping);
			tx->queue_stopped_bmp |= (1 << ac);
		}
	}

	token_id = get_token(priv,
			     ac);

	RPU_DEBUG_TX("%s-UMACTX:Alloc buf Result *id= %d q = %d out_tok: %d",
					priv->name,
					token_id,
					ac, tx->outstanding_tokens[ac]);
	RPU_DEBUG_TX(", peerid: %d,\n", peer_id);

	if (token_id == NUM_TX_DESCS)
		goto out;

	pkts_pend = rpu_tx_proc_pend_frms(priv,
						  ac,
						  token_id);

	/* We have just added a frame to pending_q but channel context is
	 * mismatch.
	 */

	if (!pkts_pend) {
		free_token(priv, token_id, ac);
		token_id = NUM_TX_DESCS;
	}

out:
	spin_unlock_bh(&tx->lock);

	RPU_DEBUG_TX("%s-UMACTX:Alloc buf Result *id= %d out_tok:%d\n",
					priv->name,
					token_id, tx->outstanding_tokens[ac]);
	/* If token is available, just return tokenid, list will be sent*/
	return token_id;
}



int rpu_tx_free_buff_req(struct img_priv *priv,
				 struct umac_event_tx_done *tx_done,
				 unsigned char *ac,
				 int *vif_index_bitmap)
{
	int i = 0;
	unsigned int pkts_pend = 0;
	struct tx_config *tx = &priv->tx;
	struct ieee80211_hdr *mac_hdr;
	struct ieee80211_tx_info *tx_info_bcn;
	struct ieee80211_tx_info tx_info_1st_mpdu;
	struct sk_buff *skb, *tmp, *skb_first = NULL;
	struct sk_buff_head *skb_list, tx_done_list;
	int vif_index = -1;
	unsigned int pkt = 0;
	int cnt = 0;
	unsigned int desc_id = tx_done->descriptor_id;
	struct umac_vif *uvif = NULL;
	struct ieee80211_vif *ivif = NULL;
	unsigned long bcn_int = 0;
	int start_ac, end_ac;

	skb_queue_head_init(&tx_done_list);

	spin_lock_bh(&tx->lock);

	RPU_DEBUG_TX("%s-UMACTX:Free buf Req q = %d",
				priv->name,
				tx_done->queue);
	RPU_DEBUG_TX(", desc_id: %d out_tok: %d\n",
				desc_id,
				priv->tx.outstanding_tokens[tx_done->queue]);


	/* Defer Tx Done Processsing */
	skb_list = &priv->tx.pkt_info[desc_id].pkt;

	if (skb_queue_len(skb_list)) {
		/* Cut the list to new one, tx_pkt will be re-initialized */
		skb_queue_splice_tail_init(skb_list, &tx_done_list);
	} else {
		RPU_DEBUG_TX("%s-UMACTX:Got Empty List: list_addr: %p\n",
						priv->name,
						skb_list);
	}

	tx->outstanding_pkts[desc_id] = 0;
	tx->tx_desc_had_send_to_io[desc_id] = 0;

	/* Reserved token */
	if (desc_id < (NUM_TX_DESCS_PER_AC * NUM_ACS)) {
		start_ac = end_ac = tx_done->queue;
	} else {
		/* Spare token:
		 * Loop through all AC's
		 */
		start_ac = WLAN_AC_VO;
		end_ac = WLAN_AC_BK;
	}
	for (cnt = start_ac; cnt >= end_ac; cnt--) {
		pkts_pend = rpu_tx_proc_pend_frms(priv,
					      cnt,
					      desc_id);

		if (pkts_pend) {
			*ac = cnt;
			/* Spare Token Case*/
			if (tx_done->queue != *ac) {
				/*Adjust the counters*/
				tx->outstanding_tokens[tx_done->queue]--;
				tx->outstanding_tokens[*ac]++;
			}
			break;
		}
	}

	/* Unmap here before release lock to avoid race */
	if (skb_queue_len(&tx_done_list)) {
		skb_queue_walk_safe(&tx_done_list, skb, tmp) {
			hal_ops.unmap_tx_buf(tx_done->descriptor_id, pkt);
			RPU_DEBUG_TX("%s-UMACTX:TXDONE: ID=%d",
				priv->name,
				tx_done->descriptor_id);
			RPU_DEBUG_TX("Stat=%d (%d, %d)\n",
				tx_done->frm_status[pkt],
				tx_done->rate[pkt],
				tx_done->retries_num[pkt]);

			pkt++;
		}
	}

	if (!pkts_pend) {
		/* Mark the token as available */
		free_token(priv, desc_id, tx_done->queue);
	}

	/* Unlock: Give a chance for Tx to add to pending lists */
	spin_unlock_bh(&tx->lock);

	/* Protection from mac80211 _ops especially stop */
	if (priv->state != STARTED)
		goto out;

	if (!skb_queue_len(&tx_done_list))
		goto out;

	skb_first = skb_peek(&tx_done_list);

	memcpy(&tx_info_1st_mpdu,
	       (struct ieee80211_tx_info *)IEEE80211_SKB_CB(skb_first),
	       sizeof(struct ieee80211_tx_info));

	pkt = 0;

	skb_queue_walk_safe(&tx_done_list, skb, tmp) {
		__skb_unlink(skb, &tx_done_list);

		if (!skb)
			continue;
		/* In the Tx path we move the .11hdr from skb to CMD_TX
		 * Hence pushing it here, not required for loopback case
		 */
		skb_push(skb,
			 priv->tx.pkt_info[tx_done->descriptor_id].hdr_len);
		mac_hdr = (struct ieee80211_hdr *)(skb->data);

		if (!ieee80211_is_beacon(mac_hdr->frame_control)) {
			vif_index = vif_addr_to_index(mac_hdr->addr2,
						      priv);
			if (vif_index > -1)
				*vif_index_bitmap |= (1 << vif_index);

			/* Same Rate info for all packets */
			tx_status(skb,
				  tx_done,
				  pkt,
				  priv,
				  tx_info_1st_mpdu);
		} else {
			struct ieee80211_bss_conf *bss_conf;
			bool bcn_status;

			if (tx_done->frm_status[pkt] ==
			    TX_DONE_STAT_DISCARD_BCN) {
				/* We did not send beacon */
				priv->tx_last_beacon = 0;
			} else if (tx_done->frm_status[pkt] ==
				   TX_DONE_STAT_SUCCESS) {
				/* We did send beacon */
				priv->tx_last_beacon = 1;
			}

			tx_info_bcn = IEEE80211_SKB_CB(skb);
			ivif = tx_info_bcn->control.vif;
			uvif = (struct umac_vif *)(ivif->drv_priv);

			bss_conf = &uvif->vif->bss_conf;
			bcn_status = bss_conf->enable_beacon;
			bcn_int = bss_conf->beacon_int - 10;
			bcn_int = msecs_to_jiffies(bcn_int);

			for (i = 0; i < MAX_VIFS; i++) {
				if (priv->active_vifs & (1 << i)) {
					if ((priv->vifs[i] == ivif) &&
					    (bcn_status == true)) {
						mod_timer(&uvif->bcn_timer,
							  jiffies +
							  bcn_int);
					}
				}
			}

			dev_kfree_skb_any(skb);
		}

		pkt++;
	}
out:
	return pkts_pend;
}






void rpu_tx_init(struct img_priv *priv)
{
	int i = 0;
	int j = 0;
	struct tx_config *tx = &priv->tx;

	memset(&tx->buf_pool_bmp,
	       0,
	       sizeof(long) * ((NUM_TX_DESCS/TX_DESC_BUCKET_BOUND) + 1));

	tx->queue_stopped_bmp = 0;
	tx->next_spare_token_ac = WLAN_AC_BE;

	for (i = 0; i < NUM_ACS; i++) {
		for (j = 0; j < MAX_PEND_Q_PER_AC; j++) {
				skb_queue_head_init(&tx->pending_pkt[j][i]);
		}

		tx->outstanding_tokens[i] = 0;
	}

	for (i = 0; i < NUM_TX_DESCS; i++) {
		skb_queue_head_init(&tx->pkt_info[i].pkt);
	}

	for (j = 0; j < NUM_ACS; j++)
		tx->curr_peer_opp[j] = 0;

	for (i = 0; i < NUM_TX_DESCS; i++) {
		tx->outstanding_pkts[i] = 0;
		tx->tx_desc_had_send_to_io[i] = 0;
	}

	spin_lock_init(&tx->lock);
	ieee80211_wake_queues(priv->hw);

	RPU_DEBUG_TX("%s-UMACTX: initialization successful\n",
			TX_TO_MACDEV(tx)->name);
}


void rpu_tx_deinit(struct img_priv *priv)
{
	int i = 0;
	int j = 0;
	struct tx_config *tx = &priv->tx;
	struct sk_buff *skb = NULL;
	struct sk_buff_head *pend_q = NULL;

	ieee80211_stop_queues(priv->hw);

	wait_for_tx_complete(tx);

	spin_lock_bh(&tx->lock);

	for (i = 0; i < NUM_TX_DESCS; i++) {
		while ((skb = skb_dequeue(&tx->pkt_info[i].pkt)) != NULL)
			dev_kfree_skb_any(skb);
	}

	for (i = 0; i < NUM_ACS; i++) {
		for (j = 0; j < MAX_PEND_Q_PER_AC; j++) {
			pend_q = &tx->pending_pkt[j][i];

			while ((skb = skb_dequeue(pend_q)) != NULL)
				dev_kfree_skb_any(skb);
		}
	}

	spin_unlock_bh(&tx->lock);

	RPU_DEBUG_TX("%s-UMACTX: deinitialization successful\n",
			TX_TO_MACDEV(tx)->name);
}


int __rpu_tx_frame(struct img_priv *priv,
			   unsigned int queue,
			   unsigned int token_id,
			   unsigned int more_frames,
			   bool retry)
{
	struct umac_event_tx_done tx_done;
	struct sk_buff_head *txq = NULL;
	int ret = 0;
	int pkt = 0;

	ret = rpu_prog_tx(queue,
				  more_frames,
				  token_id,
				  retry);

	if (ret < 0) {
		RPU_ERROR_TX("%s-UMACTX: Unable to send frame, dropping ..%d\n",
		       priv->name, ret);

		memset(&tx_done, 0, sizeof(struct umac_event_tx_done));
		tx_done.descriptor_id = token_id;
		tx_done.queue = queue;

		txq = &priv->tx.pkt_info[token_id].pkt;

		for (pkt = 0; pkt < skb_queue_len(txq); pkt++) {
			tx_done.frm_status[pkt] = TX_DONE_STAT_ERR_RETRY_LIM;
			tx_done.rate[pkt] = 0;
		}

		rpu_tx_complete(&tx_done,
					priv);
	}

	return ret;
}

static void rpu_tx_wake_lock(void)
{
	if (wake_lock_active(&hpriv->fw_err_lock))
		wake_unlock(&hpriv->fw_err_lock);
	wake_lock_timeout(&hpriv->fw_err_lock, msecs_to_jiffies(3*1000));
}

int rpu_tx_frame(struct sk_buff *skb,
			 struct ieee80211_sta *sta,
			 struct img_priv *priv,
			 bool bcast)
{
	unsigned int queue = 0;
	unsigned int token_id = 0;
	unsigned int more_frames = 0;
	int ret = 0;
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);
	struct ieee80211_hdr *mac_hdr = NULL;
	struct umac_vif *uvif = NULL;
	struct umac_sta *usta = NULL;
	int peer_id = -1;

	uvif = (struct umac_vif *)(tx_info->control.vif->drv_priv);
	mac_hdr = (struct ieee80211_hdr *)(skb->data);

	if (ieee80211_is_data(mac_hdr->frame_control))
		rpu_tx_wake_lock();

	if (sta) {
		usta = (struct umac_sta *)sta->drv_priv;
		peer_id = usta->index;
	} else {
		peer_id = MAX_PEERS + uvif->vif_index;

#ifdef STA_AP_COEXIST
		if (uvif->vif && uvif->vif->type == NL80211_IFTYPE_AP)
			adjust_beacon_ie(priv, skb);
#endif
	}

	if (bcast == false) {
		queue = tx_queue_map(skb->queue_mapping);
		more_frames = 0;
	} else {
		queue = WLAN_AC_BCN;
		/* Hack: skb->priority is used to indicate more frames */
		more_frames = skb->priority;
	}

	if (!ieee80211_is_beacon(mac_hdr->frame_control))
		priv->stats->tx_cmds_from_stack++;

	if (priv->params->production_test == 1)
		tx_info->flags |= IEEE80211_TX_CTL_AMPDU;



	RPU_DEBUG_TX("%s-UMACTX:%s:%d ",
			priv->name,
			 __func__,
			 __LINE__);
	RPU_DEBUG_TX("Wait Alloc:queue: %d qmap: %d is_bcn: %d bcast:%d\n",
			queue,
			skb->queue_mapping,
			ieee80211_is_beacon(mac_hdr->frame_control),
			is_multicast_ether_addr(mac_hdr->addr1) ? true : false);

	token_id = rpu_tx_alloc_token(priv,
						 queue,
						 peer_id,
						 skb);

	/* The frame was unable to find a reserved token */
	if (token_id == NUM_TX_DESCS) {
		RPU_DEBUG_TX("%s-UMACTX:%s:%d Token Busy Queued:\n",
			priv->name, __func__, __LINE__);
		return NETDEV_TX_OK;
	}

	ret = __rpu_tx_frame(priv,
				     queue,
				     token_id,
				     more_frames,
				     0);


	return NETDEV_TX_OK;
}

extern void rpu_prog_tx_send(void *skb);

static void make_null_frame(struct ieee80211_hdr_3addr *nullfunc, int index)
{
	struct img_priv *priv = wifi->hw->priv;

#ifdef RPU_ENABLE_PS
	if (priv->power_save == PWRSAVE_STATE_AWAKE)
		nullfunc->frame_control = 0x0148;
	else
		nullfunc->frame_control = 0x1148;
#else
	nullfunc->frame_control = 0x0148;
#endif
	nullfunc->duration_id = 0x0034;
	memcpy(nullfunc->addr1, priv->vif_info.bssid[index], ETH_ALEN);
	memcpy(nullfunc->addr2, priv->vif_info.vif_addr[index], ETH_ALEN);
	memcpy(nullfunc->addr3, priv->vif_info.bssid[index], ETH_ALEN);
	nullfunc->seq_ctrl = priv->null_frame_seq_no;
	priv->null_frame_seq_no += 0x10;
}

// only with 1 pkt
static void make_cmd_tx_ctrl(struct cmd_tx_ctrl *tx_cmd, void *data, int length, int desc_id, int queue, int index)
{
	memset(tx_cmd, 0, sizeof(struct cmd_tx_ctrl));

	tx_cmd->hdr.id = RPU_CMD_TX;
	/* Keep the queue num and pool id in descriptor id */
	tx_cmd->hdr.descriptor_id |= (queue << 16) | desc_id;
	tx_cmd->hdr.length = sizeof(struct cmd_tx_ctrl);

	tx_cmd->if_index = index;
	tx_cmd->queue_num = queue;
	tx_cmd->more_frms = 0;
	tx_cmd->descriptor_id = desc_id;
	tx_cmd->num_frames_per_desc = 1;
	tx_cmd->pkt_length[0] = length;
	tx_cmd->p_frame_ddr_pointer[0] = (unsigned int *)data;

	tx_cmd->num_rates = 1;
	tx_cmd->rate[0] = 2; // 1Mbps
	tx_cmd->rate_retries[0] = 5;
	tx_cmd->rate_preamble_type[0] = 1;
	tx_cmd->num_spatial_streams[0] = 1;

	tx_cmd->config_mac_hdr_len = sizeof(struct ieee80211_hdr_3addr);
	memcpy(tx_cmd->config_mac_header, data,
	      tx_cmd->config_mac_hdr_len );

	tx_cmd->per_pkt_crypto_params[0][0] = 0x10;
}

void rpu_send_nullframe(struct img_priv *priv)
{
	int desc_id, queue = 3;
	struct cmd_tx_ctrl *tx_cmd;
	int index = find_main_iface(priv);
	struct sk_buff *cmd_skb = NULL, *data_skb = NULL;
	unsigned int cmd_tx_size = sizeof(struct cmd_tx_ctrl);
	unsigned int data_size = sizeof(struct ieee80211_hdr_3addr);

	cmd_skb = alloc_skb(cmd_tx_size, GFP_ATOMIC);
	if (!cmd_skb)
		return;

	data_skb = alloc_skb(data_size, GFP_ATOMIC);
	if (!data_skb) {
		dev_kfree_skb_any(cmd_skb);
		return;
	}

	skb_put(cmd_skb, cmd_tx_size);
	skb_put(data_skb, data_size);

	spin_lock_bh(&priv->tx.lock);

	if (get_outstanding_pkts(&priv->tx, NUM_TX_DESCS) > 0)
		goto skip_send_null;

	desc_id = get_token(priv, queue);
	if (desc_id == NUM_TX_DESCS)
		goto skip_send_null;

	priv->null_frame_sending = 1;
	priv->null_frame_send_count++;
	tx_cmd = (struct cmd_tx_ctrl *)cmd_skb->data;
	make_null_frame((struct ieee80211_hdr_3addr *)data_skb->data, index);
	make_cmd_tx_ctrl(tx_cmd, (void *)data_skb->data,
			sizeof(struct ieee80211_hdr_3addr), desc_id, queue, index);
	priv->null_frame_skb = data_skb;

	rpu_prog_tx_send(cmd_skb);
	priv->stats->tx_cmd_send_count_single++;

	spin_unlock_bh(&priv->tx.lock);
	return;

skip_send_null:

	spin_unlock_bh(&priv->tx.lock);
	if (cmd_skb)
		dev_kfree_skb_any(cmd_skb);
	if (data_skb)
		dev_kfree_skb_any(data_skb);
}

void rpu_send_nullframe_cmp(struct img_priv *priv, int token_id, int queue)
{
	priv->null_frame_sending = 0;

	spin_lock_bh(&priv->tx.lock);
	free_token(priv, token_id, queue);
	spin_unlock_bh(&priv->tx.lock);
	dev_kfree_skb_any(priv->null_frame_skb);
}

void rpu_tx_complete(struct umac_event_tx_done *tx_done,
			     void *context)
{
	struct img_priv *priv = (struct img_priv *)context;
	unsigned int more_frames = 0;
	int vif_index = 0, vif_index_bitmap = 0, ret = 0;
	unsigned int pkts_pending = 0;
	unsigned char queue = 0;
	struct umac_event_noa noa_event;
	int token_id = 0;
	int qlen = 0;

	token_id = tx_done->descriptor_id;
	//RPU_INFO_TX("tx done %d %d\n", token_id, tx_done->queue);

	if (token_id < 0 || token_id > 11) {
		RPU_ERROR_TX("%s:%d Invalid token_id: %d\n",
			     __func__,
			     __LINE__,
			     token_id);
		RPU_DEBUG_DUMP_TX(DUMP_PREFIX_NONE,
			          16,
			          1,
			          tx_done,
			          sizeof(struct umac_event_tx_done),
			          1);
		return;
	}

	/* check send null frame complete */
	if (priv->null_frame_sending &&
		priv->null_frame_desc_id == token_id) {
		rpu_send_nullframe_cmp(priv, token_id, tx_done->queue);
		return;
	}

#ifdef ENABLE_DAPT_BEACON
	vif_index = priv->tx.pkt_info[token_id].vif_index;
	if (vif_index < MAX_VIFS)
		dapt_beacon(priv, *(short *)&tx_done->reserved[2*vif_index], vif_index);
#endif

	qlen = skb_queue_len(&priv->tx.pkt_info[token_id].pkt);

	RPU_DEBUG_TX("%s-UMACTX:TX Done Rx for desc_id: %d",
			  priv->name,
			  tx_done->descriptor_id);
	RPU_DEBUG_TX("Q: %d qlen: %d status: %d out_tok: %d\n",
			  tx_done->queue,
			  qlen,
			  tx_done->frm_status[0],
			  priv->tx.outstanding_tokens[tx_done->queue]);

	update_aux_adc_voltage(priv, tx_done->pdout_voltage);

	pkts_pending = rpu_tx_free_buff_req(priv,
						    tx_done,
						    &queue,
						    &vif_index_bitmap);

	if (pkts_pending) {
		/*TODO..Do we need to check each skb for more_frames??*/
		more_frames = 0;

		RPU_DEBUG_TX("%s-UMACTX:%s:%d Transfer Pending Frames:\n",
			       priv->name,
			       __func__,
			       __LINE__);

		ret = __rpu_tx_frame(priv,
					     queue,
					     token_id,
					     more_frames,
					     0);

	} else {
		RPU_DEBUG_TX("%s-UMACTX:No Pending Packets\n", priv->name);
	}


	for (vif_index = 0; vif_index < MAX_VIFS; vif_index++) {
		if (vif_index_bitmap & (1 << vif_index)) {
			memset(&noa_event, 0, sizeof(noa_event));
			noa_event.if_index = vif_index;
			rpu_noa_event(FROM_TX_DONE,
					      &noa_event,
					      (void *)priv,
					      NULL);
		}
	}
}

/* process unfinished tx done when fw error happened */
void rpu_tx_proc_unfi_tx_done(void)
{
	int i, j;
	struct umac_event_tx_done tx_done;
	struct umac_event_tx_done *ptx_done;
	struct img_priv *priv = wifi->hw->priv;

	RPU_DEBUG_ROCOVERY("%s: buf_pool_bmp = %x\n", __func__, (unsigned int)priv->tx.buf_pool_bmp[0]);
	for (i = 0; i < NUM_TX_DESCS; i++) {
		if (test_bit(i, &priv->tx.buf_pool_bmp[0])) {
			if (priv->tx.tx_desc_had_send_to_io[i] == 0)
				continue;

			memset(&tx_done, 0, sizeof(struct umac_event_tx_done));
			ptx_done = (struct umac_event_tx_done *)&tx_done;

			//Setup msg according to TX done message processing in mac handle event
			ptx_done->hdr.length = sizeof(struct umac_event_tx_done);
			ptx_done->hdr.payload_length = 0;
			ptx_done->hdr.id = RPU_EVENT_TX_DONE;

			ptx_done->pdout_voltage = 0;
			ptx_done->queue = priv->tx.pkt_info[i].queue;
			ptx_done->descriptor_id = i;

			for(j = 0; j < MAX_TX_CMDS; j++) {
				ptx_done->frm_status[j] = TX_DONE_STAT_SUCCESS;
				ptx_done->retries_num[j] = 0;
				ptx_done->rate[j] = 2;
			}

			RPU_DEBUG_ROCOVERY("desc %d, queue %d\n", i, ptx_done->queue);
			rpu_tx_complete(ptx_done, (void *)priv);
		}
	}
}

