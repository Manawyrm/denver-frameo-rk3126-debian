/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "core.h"

void modify_beacon_params (struct umac_vif *uvif,
				  struct ieee80211_bss_conf *bss_conf)
{
	unsigned int bcn_int = 0;
	unsigned long bcn_tim_val = 0;
	int ret = 0;

	RPU_DEBUG_VIF("%s: enable_beacon=%d\n", __func__, uvif->vif->bss_conf.enable_beacon);

	if (uvif->vif->bss_conf.enable_beacon == true) {

		bcn_int = bss_conf->beacon_int;
		bcn_tim_val =  msecs_to_jiffies(bcn_int - 10);

		mod_timer(&uvif->bcn_timer,
			  jiffies + bcn_tim_val);

		CALL_RPU(rpu_prog_vif_beacon_int,
			  uvif->vif_index,
			  uvif->vif->addr,
			  bcn_int);
	} else {
		timer_delete(&uvif->bcn_timer);
	}
prog_rpu_fail:
	return;

}

//INIT_GET_SPEND_TIME(bcn_start_time, bcn_stop_time);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
static void vif_bcn_timer_expiry(struct timer_list *t)
{
	struct umac_vif *uvif = timer_container_of(uvif, t, bcn_timer);
#else
static void vif_bcn_timer_expiry(unsigned long data)
{
	struct umac_vif *uvif = (struct umac_vif *)data;
#endif
	struct sk_buff *skb, *temp;
	struct sk_buff_head bcast_frames;

	RPU_DEBUG_VIF("%s: enable_beacon=%d\n", __func__, uvif->vif->bss_conf.enable_beacon);
	/*{ // check beacon frame interval
		unsigned long itv;
		START_GET_SPEND_TIME(bcn_start_time, bcn_stop_time);
		itv = GET_SPEND_TIME_US(bcn_stop_time, bcn_start_time)/1000;
		if (itv > 120 || itv < 90)
			RPU_ERROR_VIF("beacon interval = %ld\n", itv);
		END_GET_SPEND_TIME(bcn_start_time, bcn_stop_time);
	}*/

	if (uvif->vif->bss_conf.enable_beacon == false)
		return;

	if (uvif->vif->type == NL80211_IFTYPE_AP) {
		temp = skb = ieee80211_beacon_get(uvif->priv->hw, uvif->vif, 0);

		if (!skb) {
			/* No beacon, so dont transmit braodcast frames*/
			goto reschedule_timer;
		}

		skb_queue_head_init(&bcast_frames);
		skb->priority = 1;
		skb_queue_tail(&bcast_frames, skb);

		skb = ieee80211_get_buffered_bc(uvif->priv->hw, uvif->vif);

		while (skb) {
			/* Hack: skb->priority is used to indicate more
			 * frames
			 */
			skb->priority = 1;
			skb_queue_tail(&bcast_frames, skb);
			temp = skb;
			skb = ieee80211_get_buffered_bc(uvif->priv->hw,
							uvif->vif);
		}

		if (temp)
			temp->priority = 0;

		spin_lock_bh(&uvif->priv->bcast_lock);

		while ((skb = skb_dequeue(&bcast_frames))) {
			/* For a Beacon queue we will let the frames pass
			 * through irrespective of the current channel context.
			 * The FW will take care of transmitting them in the
			 * appropriate channel. Hence pass the interfaces
			 * channel context instead of the actual current channel
			 * context.
			 */
			rpu_tx_frame(skb,
					     NULL,
					     uvif->priv,
					     true);
		}

		spin_unlock_bh(&uvif->priv->bcast_lock);

	} else {
		skb = ieee80211_beacon_get(uvif->priv->hw, uvif->vif, 0);

		if (!skb)
			goto reschedule_timer;

		/* For a Beacon queue we will let the frames pass through
		 * irrespective of the current channel context. The FW will take
		 * care of transmitting them in the appropriate channel.  Hence
		 * pass the interfaces channel context instead of the actual
		 * current channel context.
		 */
		rpu_tx_frame(skb,
				     NULL,
				     uvif->priv,
				     true);

	}
reschedule_timer:
	return;

}

void init_beacon (struct umac_vif *uvif)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
	timer_setup(&uvif->bcn_timer, vif_bcn_timer_expiry, 0);
#else
	init_timer(&uvif->bcn_timer);
	uvif->bcn_timer.data = (unsigned long)uvif;
	uvif->bcn_timer.function = vif_bcn_timer_expiry;
#endif
}

void deinit_beacon (struct umac_vif *uvif)
{
	timer_delete(&uvif->bcn_timer);

}
