/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _P2P_H_
#define _P2P_H_
void rpu_roc_complete_work(struct work_struct *work);
int remain_on_channel(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif,
			     struct ieee80211_channel *channel,
			     int duration,
			     enum ieee80211_roc_type type);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
int cancel_remain_on_channel(struct ieee80211_hw *hw,
				struct ieee80211_vif *vif);
#else
int cancel_remain_on_channel(struct ieee80211_hw *hw);
#endif
void rpu_noa_event(int event, struct umac_event_noa *noa, void *context,
			   struct sk_buff *skb);
#endif /* _P2P_H_*/
