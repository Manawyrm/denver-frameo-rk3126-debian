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

static int rpu_proc_tx_pending = 0;

int get_rate_prod(struct cmd_tx_ctrl *txcmd,
		     struct img_priv *priv)
{
	
	unsigned int index;

	if (priv->params->production_test != 1)
		return -80;

	index = 0;
	if (priv->params->tx_fixed_mcs_indx != -1) {
		txcmd->rate[index] = 0x80;
		txcmd->rate[index] |=
		    (priv->params->tx_fixed_mcs_indx);
		txcmd->num_spatial_streams[index] =
		    priv->params->num_spatial_streams;
		txcmd->bcc_or_ldpc =
		    priv->params->prod_mode_bcc_or_ldpc;
		txcmd->stbc_enabled =
		    priv->params->prod_mode_stbc_enabled;
		update_mcs_packet_stat(
		    priv->params->tx_fixed_mcs_indx,
		    txcmd->rate_flags[index], priv);
	} else if (priv->params->production_test == 1 &&
		   priv->params->tx_fixed_rate != -1) {
		txcmd->rate[index] = 0x00;
		if (priv->params->tx_fixed_rate == 55)
			txcmd->rate[index] |=
			 ((priv->params->tx_fixed_rate) /
			  5);
		else
			txcmd->rate[index] |=
			  ((priv->params->tx_fixed_rate *
			    10) / 5);
		txcmd->num_spatial_streams[index] = 1;
		txcmd->bcc_or_ldpc = 0;
		txcmd->stbc_enabled = 0;
	} else {
		WARN_ON(1);
		rcu_read_unlock();
		return -90;
	}
	txcmd->num_rates = 1;
	txcmd->rate_retries[index] = 1;
	txcmd->rate_flags[index] =
		priv->params->prod_mode_rate_flag;
	txcmd->rate_preamble_type[index] =
		priv->params->prod_mode_rate_preamble_type;

	return 0;
}


unsigned char bss_addr[6] = {72, 14, 29, 35, 31, 52};
void proc_bss_info_changed(unsigned char *mac_addr, int value)
{
		int temp = 0, i = 0, j = 0, ret = 0;

		get_random_bytes(&j, sizeof(j));
		for (i = 5; i > 0; i--) {
			j = j % (i+1);
			temp = bss_addr[i];
			bss_addr[i] = bss_addr[j];
			bss_addr[j] = temp;
			}
		CALL_RPU(rpu_prog_vif_bssid,
			  0,
			  mac_addr,
			  bss_addr);
prog_rpu_fail:
	return;
}

void packet_generation(unsigned long data)
{
		struct img_priv *priv = (struct img_priv *)data;
		unsigned char *mac_addr = priv->if_mac_addresses[0].addr;
		struct ieee80211_hdr hdr = {0};
		struct sk_buff *skb;
		unsigned char broad_addr[6] = {0xff, 0xff, 0xff,
					       0xff, 0xff, 0xff};
		u16 hdrlen = 26;
		static unsigned char fill_data = 0;
		int token_id = 0;
		int queue = WLAN_AC_BE;
#ifdef PKTGEN_MULTI_TX
		queue = WLAN_AC_BK;
#endif
		rpu_proc_tx_pending = 1;

#ifdef PKTGEN_MULTI_TX
	while (1) {
		token_id = get_token(priv, queue);
		if (token_id == NUM_TX_DESCS) {
			queue++;
			if (queue == WLAN_AC_BCN)
				break;
			continue;
		}
#endif
		/*LOOP_START*/
		/*PREPARE_SKB_LIST and SEND*/

		skb = alloc_skb(priv->params->payload_length + hdrlen,
				GFP_ATOMIC);
		img_ether_addr_copy(hdr.addr1, broad_addr);
		img_ether_addr_copy(hdr.addr2, mac_addr);
		img_ether_addr_copy(hdr.addr3, bss_addr);
		hdr.frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA |
						IEEE80211_STYPE_QOS_DATA);
		memcpy(skb_put(skb, hdrlen), &hdr, hdrlen);
		memset(skb_put(skb, priv->params->payload_length),
			priv->params->echo_mode?fill_data++:0xAB,
			priv->params->payload_length);

		/*LOOP_END*/
#ifdef PKTGEN_MULTI_TX
		skb_queue_tail(&priv->tx.proc_tx_list[token_id], skb);
#else
		skb_queue_tail(&priv->tx.proc_tx_list[0], skb);
#endif
		rpu_proc_tx(priv, token_id, queue);
#ifdef PKTGEN_MULTI_TX
	}
#endif
}

int rpu_proc_tx(struct img_priv *priv, int descriptor_id, int queue)
{
	struct cmd_tx_ctrl *tx_cmd;
	struct sk_buff *tmp, *skb, *skb_first, *tx_cmd_skb;
	struct sk_buff_head *skb_list;
	struct ieee80211_hdr *mac_hdr;
	unsigned int pkt = 0;
	u16 hdrlen = 26;
	dma_addr_t phy_addr;
	unsigned int cmd_tx_size;

	//pr_info("%s: desc_id %d queue %d\n", __func__, descriptor_id, queue);

	if (!priv->params->start_prod_mode) {
		return -1;
	}

	spin_lock_bh(&priv->tx.lock);

	skb_list = &priv->tx.proc_tx_list[descriptor_id];

	skb_first = skb_peek(skb_list);
	if (!skb_first) {
		spin_unlock_bh(&priv->tx.lock);
		return -10;
	}

	cmd_tx_size = sizeof(struct cmd_tx_ctrl);

	tx_cmd_skb = alloc_skb(cmd_tx_size, GFP_KERNEL);

	if (!tx_cmd_skb) {
		spin_unlock_bh(&priv->tx.lock);
		rcu_read_unlock();
		return -ENOMEM;
	}

	skb_put(tx_cmd_skb, cmd_tx_size);
	
	tx_cmd = kzalloc(cmd_tx_size, GFP_KERNEL);

	if (!tx_cmd) {
		spin_unlock_bh(&priv->tx.lock);
		rcu_read_unlock();
		kfree(tx_cmd_skb);
		return -ENOMEM;
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
	tx_cmd->num_frames_per_desc = skb_queue_len(skb_list); 
	tx_cmd->aggregate_mpdu = AMPDU_AGGR_DISABLED;

	memcpy(tx_cmd->config_mac_header, skb_first->data,
	      hdrlen);
	tx_cmd->config_mac_hdr_len = hdrlen;

	get_rate_prod(tx_cmd, priv);

	pkt = 0;
	skb_queue_walk_safe(skb_list, skb, tmp) {
		struct ieee80211_tx_info *tx_info;
		unsigned char *crypto_params;
		unsigned int mac_hdr_len;

		if (!skb || (pkt > tx_cmd->num_frames_per_desc))
			break;

		tx_info = IEEE80211_SKB_CB(skb_first);
		mac_hdr = (struct ieee80211_hdr *)skb->data;
		/* Complete packet length*/
		tx_cmd->pkt_length[pkt] = skb->len;

		if (hal_ops.map_tx_buf(descriptor_id, pkt,
				       skb->data, skb->len, &phy_addr)) {
			spin_unlock_bh(&priv->tx.lock);
			kfree(tx_cmd_skb);
			kfree(tx_cmd);
			return -30;
		}
		tx_cmd->p_frame_ddr_pointer[pkt] = (unsigned int *)phy_addr;
		crypto_params = tx_cmd->per_pkt_crypto_params[pkt];
		memset(crypto_params, 0, PER_PKT_CRYPTO_PARAMS_SIZE);

		memcpy(crypto_params + PER_PKT_CRYPTO_PARAMS_SEQ_CTRL_OFFSET,
		       &mac_hdr->seq_ctrl,
		       2);

		if (ieee80211_is_data_qos(mac_hdr->frame_control)) {
			struct ieee80211_qos_hdr *qos_mac_hdr;
			unsigned char *iv_offset;

			iv_offset = crypto_params +
				    PER_PKT_CRYPTO_PARAMS_QOS_CTRL_OFFSET;
			qos_mac_hdr = (struct ieee80211_qos_hdr *) skb->data;
			memcpy(iv_offset,
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
	memcpy(tx_cmd_skb->data, tx_cmd, cmd_tx_size);
		hal_ops.send((void *)tx_cmd_skb,
			     NULL,
			     0);
	/* increment tx_cmd_send_count to keep track of number of
	 * tx_cmd send
	 */
	if (skb_queue_len(skb_list) == 1)
		priv->stats->tx_cmd_send_count_single++;
	else if (skb_queue_len(skb_list) > 1)
		priv->stats->tx_cmd_send_count_multi++;

	spin_unlock_bh(&priv->tx.lock);
	kfree(tx_cmd);
	return 0;
}

int start_prod_mode(struct img_priv *priv, unsigned int val)
{

	unsigned int pri_chnl_num = 0;
	unsigned int freq_band = IEEE80211_BAND_5GHZ;
	int center_freq = 0;
	int ret = -1;
#ifdef PKTGEN_MULTI_TX
	int i;
#endif

	rpu_proc_tx_pending = 0;

	if (priv->params->production_test != 1) {
		pr_err("start_prod_mode: Can be set in only in production mode.\n");
		goto error;
	}

	if (!priv->params->init_prod) {
		tasklet_init(&priv->proc_tx_tasklet, packet_generation,
		     (unsigned long)priv);
	}

	pri_chnl_num = val;
	priv->params->start_prod_mode = val;
	if (pri_chnl_num < 15)
		freq_band = IEEE80211_BAND_2GHZ;
	else
		freq_band = IEEE80211_BAND_5GHZ;

	center_freq =
	ieee80211_channel_to_frequency(pri_chnl_num,
				       freq_band);

	if (priv->params->init_prod) {
		rpu_prog_channel(pri_chnl_num,
					center_freq,
					 0,
					 0,
			/*It will be overwritten anyway*/
					 freq_band);
		return 0;
	}

	if (!rpu_core_init(priv, ftm)) {
		rpu_prog_vif_ctrl(0,
				priv->if_mac_addresses[0].addr,
				IF_MODE_STA_IBSS,
				IF_ADD);

		proc_bss_info_changed(
				priv->if_mac_addresses[0].addr,
				val);

		rpu_prog_channel(pri_chnl_num,
					center_freq,
					 0,
					 0,
			/*It will be overwritten anyway*/
					 freq_band);
#ifdef PKTGEN_MULTI_TX
		for (i = 0; i < NUM_TX_DESCS; i++)
		    skb_queue_head_init(&priv->tx.proc_tx_list[i]);
#else
		skb_queue_head_init(&priv->tx.proc_tx_list[0]);
#endif
		priv->params->init_prod = 1;
		priv->state = STARTED;
		uccp_reinit = 0;
	 } else {
		pr_err("RPU Initialization Failed\n");
		priv->params->init_prod = 0;
	}
	ret = 0;
error:
	return ret;
}

int stop_prod_mode(struct img_priv *priv, unsigned int val)
{
	int ret = -1;
	if (!priv->params->init_prod) {
		pr_err("Prod mode is not initialized\n");
		goto error;
	}

	tasklet_kill(&priv->proc_tx_tasklet);

	priv->params->start_prod_mode = 0;
	priv->params->pkt_gen_val = 1;
	priv->params->init_prod = 0;
	priv->params->init_pkt_gen = 0;
	while (rpu_proc_tx_pending) {
		msleep(1);
	}

	if (!uccp_reinit)
		//stop(priv->hw);
		rpu_core_deinit(priv, ftm);

	ret = 0;
error:
	return -1;
}

int start_prod_rx_mode(struct img_priv *priv, unsigned int val,
					unsigned char *bssid, unsigned char *mac_addr)
{

	unsigned int pri_chnl_num = 0;
	unsigned int freq_band = IEEE80211_BAND_5GHZ;
	int center_freq = 0;
	int ret = -1;
	int cw_mode = 0;
	/*unsigned char */mac_addr = priv->if_mac_addresses[0].addr;
	bssid = priv->if_mac_addresses[0].addr;

	bssid[0] = 0;

	if (priv->params->production_test != 1) {
		pr_err("start_prod_mode: Can be set in only in production mode.\n");
		goto error;
	}

	cw_mode = val & 0x80;
	val &= 0x7f;
	pr_info("%s: channel = %d, cw_mode %x, bssid = %pM\n",
                    __func__, val, cw_mode, bssid);

	pri_chnl_num = val;
	priv->params->start_prod_mode = val;
	if (pri_chnl_num < 15)
		freq_band = IEEE80211_BAND_2GHZ;
	else
		freq_band = IEEE80211_BAND_5GHZ;

	center_freq =
	ieee80211_channel_to_frequency(pri_chnl_num,
				       freq_band);

	if (priv->params->init_prod) {
		rpu_prog_channel(pri_chnl_num,
					center_freq,
					 0,
					 0,
			/*It will be overwritten anyway*/
					 freq_band);		
		return 0;
	}

	if (!rpu_core_init(priv, ftm)) {
		rpu_prog_vif_ctrl(0,
				mac_addr,
				IF_MODE_STA_IBSS,
				IF_ADD);

		rpu_prog_vif_bssid(0,
			  mac_addr,
			  bssid);

		pri_chnl_num |= cw_mode;
		rpu_prog_channel(pri_chnl_num,
					center_freq,
					 0,
					 0,
			/*It will be overwritten anyway*/
					 freq_band);
		
		priv->params->init_prod = 1;
		priv->state = STARTED;
		uccp_reinit = 0;
	 } else {
		pr_err("RPU Initialization Failed\n");
		priv->params->init_prod = 0;
	}
	ret = 0;
error:
	return ret;
}

int start_prod_echo_mode(struct img_priv *priv, unsigned int val)
{
	int ret = -1;
#ifdef PKTGEN_MULTI_TX
	int i;
#endif

	if (priv->params->production_test != 1) {
		pr_err("start_prod_mode: Can be set in only in production mode.\n");
		goto error;
	}

	if (priv->params->init_prod) {
		pr_err("Production Test is already initialized.\n");
		goto error;
	}

	priv->params->start_prod_mode = val;
	tasklet_init(&priv->proc_tx_tasklet, packet_generation,
		     (unsigned long)priv);

	if (!rpu_core_init(priv, ftm)) {
		// notify FW into tx echo mode
		struct fw_params params;
		memset(&params, 0, sizeof(struct fw_params));
		params.mask |= 1<<PARAM_ECHO_MODE;
		params.echo_mode=1;
		msleep(200);
		if (rpu_fw_priv_cmd_sync(FW_SET_PARAMS, &params) != 0) {
			goto error;
		}
		priv->params->echo_mode = params.echo_mode;
#ifdef PKTGEN_MULTI_TX
		for (i = 0; i < NUM_TX_DESCS; i++)
		    skb_queue_head_init(&priv->tx.proc_tx_list[i]);
#else
		skb_queue_head_init(&priv->tx.proc_tx_list[0]);
#endif
		priv->params->init_prod = 1;
		priv->state = STARTED;
		uccp_reinit = 0;
	 } else {
		pr_err("RPU Initialization Failed\n");
		priv->params->init_prod = 0;
	}
	ret = 0;
error:
	return ret;
}

int start_packet_gen(struct img_priv *priv, int sval)
{
	int ret = -1;

	if (!priv->params->init_prod) {
		pr_err("NEW Production Mode is not Initialized\n");
		goto error;
	}

	if (priv->params->init_pkt_gen) {
		pr_err("packet gen is already running\n");
		goto error;
	}

	if (priv->params->tx_fixed_mcs_indx == -1 &&
		priv->params->tx_fixed_rate == -1) {
		pr_err("Either tx_fixed_mcs_index Or tx_fixed_rate should be set, both can't be NULL.\n");
		goto error;
	}

	priv->params->init_pkt_gen = 1;

	priv->params->pkt_gen_val = sval;

	if (sval != 0)
		tasklet_schedule(&priv->proc_tx_tasklet);
	ret = 0;
error:
	return -1;
}

int stop_packet_gen(struct img_priv *priv, int sval)
{
	int ret = -1;

	if (!priv->params->init_prod) {
		pr_err("NEW Production Mode is not Initialized\n");
		goto error;
	}

	priv->params->pkt_gen_val = 1;
	priv->params->init_pkt_gen = 0;
	tasklet_kill(&priv->proc_tx_tasklet);
	ret = 0;
error:
	return -1;

}

static struct sk_buff *s_tmp_skb = NULL;

void rpu_proc_tx_complete(struct umac_event_tx_done *tx_done,
			     void *context)
{

	struct img_priv *priv = (struct img_priv *)context;
	struct sk_buff *skb, *tmp;
	struct sk_buff_head *tx_done_list;
	unsigned int pkt = 0;

//	pr_info("%s: desc_id %d queue %d\n", __func__,
//            tx_done->descriptor_id, tx_done->queue);

#ifdef PKTGEN_MULTI_TX
	free_token(priv, tx_done->descriptor_id, tx_done->queue);
#endif
	tx_done_list = &priv->tx.proc_tx_list[tx_done->descriptor_id];
	priv->stats->tx_done_recv_count++;
	update_aux_adc_voltage(priv, tx_done->pdout_voltage);
	skb_queue_walk_safe(tx_done_list, skb, tmp) {
		__skb_unlink(skb, tx_done_list);
		if (!skb)
			continue;
		hal_ops.unmap_tx_buf(tx_done->descriptor_id, pkt);
		if (priv->params->echo_mode && s_tmp_skb!=NULL) {
			if (memcmp(skb->data+26, s_tmp_skb->data+94, s_tmp_skb->len-94) != 0) {
				pr_err("packet compare fail!\n");
				//dump
				print_hex_dump(KERN_DEBUG, "tx ", DUMP_PREFIX_NONE,
						16,
						  1,
						  skb->data,
						  skb->len,
						  1);
				print_hex_dump(KERN_DEBUG, "rx ", DUMP_PREFIX_NONE,
						16,
						  1,
						  s_tmp_skb->data,
						  s_tmp_skb->len,
						  1);
			} else {
				pr_info("echo pass\n");
			}
			// free
			dev_kfree_skb_any(s_tmp_skb);
			s_tmp_skb = NULL;
		}
		dev_kfree_skb_any(skb);
		pkt++;
	}

#ifdef PKTGEN_MULTI_TX
    if (!priv->tx.buf_pool_bmp[0])
        rpu_proc_tx_pending = 0;
#else
    rpu_proc_tx_pending = 0;
#endif

	/*send NEXT packet list*/
	if ((priv->params->pkt_gen_val == -1) ||
	    (--priv->params->pkt_gen_val > 0)) {
	    if (priv->params->start_prod_mode)
		    tasklet_schedule(&priv->proc_tx_tasklet);
    }
}

void rpu_proc_rx_event(void *nbuff, void *context)
{
	struct sk_buff *skb = (struct sk_buff *)nbuff;
	struct img_priv *priv = (struct img_priv *)context;

	priv->stats->rx_packet_data_count++;
	dump_ieee80211_hdr_info(skb->data, skb->len, 0);
	if (priv->params->echo_mode) {
		s_tmp_skb = skb;
	} else {
		dev_kfree_skb_any(skb);
	}
}

