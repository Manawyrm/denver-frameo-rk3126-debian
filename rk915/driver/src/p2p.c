/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <net/cfg80211.h>
#include <net/mac80211.h>

#include "core.h"
#include "p2p.h"
#include "utils.h"

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
extern void roc_timer_expiry(struct timer_list *t);
#else
extern void roc_timer_expiry(unsigned long data);
#endif

void init_roc_timeout_timer (struct img_priv *priv)
{
	RPU_DEBUG_ROC("%s: %p\n", __func__, priv);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
	timer_setup(&priv->roc_timer, roc_timer_expiry, 0);
#else
	init_timer(&priv->roc_timer);
	priv->roc_timer.data = (unsigned long)NULL;
	priv->roc_timer.function = roc_timer_expiry;
#endif
}

void start_roc_timeout_timer(struct img_priv *priv, int timeout)
{
	RPU_DEBUG_ROC("%s: %p\n", __func__, priv);
	mod_timer(&priv->roc_timer, jiffies + msecs_to_jiffies(timeout));
}

void deinit_roc_timeout_timer (struct img_priv *priv)
{
	RPU_DEBUG_ROC("%s: %p\n", __func__, priv);
	timer_delete(&priv->roc_timer);
}

void rpu_roc_complete_work(struct work_struct *work)
{
	struct delayed_work *dwork = NULL;
	struct img_priv *priv = NULL;
	struct tx_config *tx = NULL;
	u32 roc_queue = 0;

	dwork = container_of(work, struct delayed_work, work);
	priv = container_of(dwork, struct img_priv, roc_complete_work);
	tx = &priv->tx;

	if (priv->roc_params.roc_in_progress == 0) {
		priv->roc_params.roc_starting = 0;
		return;
	}

	mutex_lock(&priv->mutex);

	roc_queue = tx_queue_unmap(UMAC_ROC_AC);

	/* Stop the ROC queue */
	ieee80211_stop_queue(priv->hw, roc_queue);
	/* Unlock RCU immediately as we are freeing off_chanctx in this funciton
	 * only and because flush_vif_queues sleep
	 */
	rcu_read_lock();
	rcu_read_unlock();

	priv->roc_params.roc_in_progress = 0;
	priv->roc_params.roc_starting = 0;

	if (priv->cancel_roc == 0) {
		ieee80211_remain_on_channel_expired(priv->hw);
		RPU_DEBUG_ROC("%s:%d ROC STOPPED..\n", __func__, __LINE__);
	} else {
		priv->cancel_hw_roc_done = 1;
		priv->cancel_roc = 0;
		RPU_DEBUG_ROC("%s:%d ROC CANCELLED..\n", __func__, __LINE__);
	}

	/* Start the ROC queue */
	ieee80211_wake_queue(priv->hw, roc_queue);
	mutex_unlock(&priv->mutex);
}

int remain_on_channel(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif,
			     struct ieee80211_channel *channel,
			     int duration,
			     enum ieee80211_roc_type type)
{
	struct img_priv *priv = (struct img_priv *)hw->priv;
	unsigned int pri_chnl_num =
		ieee80211_frequency_to_channel(channel->center_freq);
	int ret = 0;

	mutex_lock(&priv->mutex);
	RPU_DEBUG_ROC("%s:%d The Params are:",
					__func__,
					__LINE__);
	RPU_DEBUG_ROC(" channel:%d duration:%d type: %d\n",
			ieee80211_frequency_to_channel(channel->center_freq),
			duration,
			type);

	if (priv->roc_params.roc_in_progress ||
		priv->roc_params.roc_starting || 
	    priv->params->hw_scan_status != HW_SCAN_STATUS_NONE) {
		RPU_INFO_ROC("%s:%d Dropping roc...Busy\n",
				__func__,
				__LINE__);
		mutex_unlock(&priv->mutex);
#if 0//def RK915
		return 0;
#else		
		return -EBUSY;
#endif
	}

#ifdef RK3036_DONGLE
	start_roc_timeout_timer(priv, duration*3);
#endif

	priv->roc_params.roc_starting = 1;

	/* Inform FW that ROC is started:
	 * For pure TX we send OFFCHANNEL_TX so that driver can terminate ROC
	 * For Tx + Rx we use NORMAL, FW will terminate ROC based on duration.
	 */
	if (duration != 10 && type == ROC_TYPE_OFFCHANNEL_TX)
		type = ROC_TYPE_NORMAL;

	CALL_RPU(rpu_prog_roc,
		  ROC_START,
		  pri_chnl_num,
		  duration,
		  type);


prog_rpu_fail:
	mutex_unlock(&priv->mutex);
	return ret;
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
int cancel_remain_on_channel(struct ieee80211_hw *hw,
						struct ieee80211_vif *vif)
#else
int cancel_remain_on_channel(struct ieee80211_hw *hw)
#endif
{
	struct img_priv *priv = (struct img_priv *)hw->priv;
	int ret = 0;
#ifdef RK3036_DONGLE
	int index;
	int skip = 0;

	index = find_main_iface(priv);
	if (index != MAX_VIFS) {
		if (priv->vifs[index] && 
		    priv->vifs[index]->bss_conf.enable_beacon)
			skip = 1;
	}
	if (skip) {
		RPU_INFO_ROC("%s:%d Cancel HW ROC skip\n",
			__func__, __LINE__);
		return -1;
	}
#endif
	mutex_lock(&priv->mutex);

	if (priv->roc_params.roc_in_progress) {
		priv->cancel_hw_roc_done = 0;
		priv->cancel_roc = 1;
		RPU_DEBUG_ROC("%s:%d Cancelling HW ROC....\n",
				__func__, __LINE__);
		CALL_RPU(rpu_prog_roc, ROC_STOP, 0, 0, 0);

		mutex_unlock(&priv->mutex);

		if (!wait_for_cancel_hw_roc(priv)) {
			RPU_DEBUG_ROC("%s:%d Cancel HW ROC....done\n",
							__func__,
							__LINE__);
			ret = 0;
		} else {
			RPU_ERROR_ROC("%s:%d Cancel HW ROC..timedout\n",
							__func__,
							__LINE__);
			ret = -1;
		}
	}
prog_rpu_fail:
	mutex_unlock(&priv->mutex);
	return ret;
}


void rpu_noa_event(int event, struct umac_event_noa *noa, void *context,
			   struct sk_buff *skb)
{
	struct img_priv  *priv = (struct img_priv *)context;
	struct ieee80211_vif *vif;
	struct umac_vif *uvif;
	bool transmit = false;

	rcu_read_lock();

	vif = (struct ieee80211_vif *)rcu_dereference(priv->vifs[noa->if_index]);

	if (vif == NULL) {
		rcu_read_unlock();
		return;
	}

	uvif = (struct umac_vif *)vif->drv_priv;

	spin_lock_bh(&uvif->noa_que.lock);

	if (event == FROM_TX) {
		if (uvif->noa_active) {
			if (!uvif->noa_tx_allowed || skb_peek(&uvif->noa_que))
				__skb_queue_tail(&uvif->noa_que, skb);
			else
				transmit = true;
		} else
			transmit = true;
	} else if (event == FROM_TX_DONE) {
		if (uvif->noa_active && uvif->noa_tx_allowed) {
			skb = __skb_dequeue(&uvif->noa_que);

			if (skb)
				transmit = true;
		}
	} else { /* event = FROM_EVENT_NOA */

		uvif->noa_active = noa->noa_active;

		if (uvif->noa_active) {
			RPU_DEBUG_P2P("%s: noa active = %d, ",
					priv->name, noa->noa_active);
			RPU_DEBUG_P2P("ap_present = %d\n",
					noa->ap_present);

			uvif->noa_tx_allowed = noa->ap_present;

			if (uvif->noa_tx_allowed) {
				skb = __skb_dequeue(&uvif->noa_que);
				if (skb)
					transmit = true;
			}
		} else {
			RPU_DEBUG_P2P("%s: noa active = %d\n",
				 priv->name, noa->noa_active);

			uvif->noa_tx_allowed = 1;

			/* Can be done in a better way. For now, just flush the
			 * NoA Queue
			 */
			while ((skb = __skb_dequeue(&uvif->noa_que)))
				dev_kfree_skb_any(skb);
		}
	}

	spin_unlock_bh(&uvif->noa_que.lock);

	rcu_read_unlock();

	if (transmit) {
		rpu_tx_frame(skb,
				     NULL,
				     priv,
				     false);
	}
}

