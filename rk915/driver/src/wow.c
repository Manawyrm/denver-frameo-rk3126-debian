/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "wow.h"
#include "core.h"
#include "hal_io.h"

#ifdef CONFIG_PM

unsigned char img_suspend_status;
static int host_suspend(void *data);
static const struct syscore_ops host_syscore_ops = {
	.suspend = host_suspend,
};
struct syscore host_syscore = {
	.ops = &host_syscore_ops,
};

#if 0
static int wait_for_econ_ps_cfg(struct img_priv *priv)
{
	int count = 0;
	char econ_ps_cfg_done = 0;

check_econ_ps_cfg_complete:
	mutex_lock(&priv->mutex);
	econ_ps_cfg_done = priv->econ_ps_cfg_stats.completed;
	mutex_unlock(&priv->mutex);

	if (!econ_ps_cfg_done && (count < PS_ECON_CFG_TIMEOUT_TICKS)) {
		count++;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
		goto check_econ_ps_cfg_complete;
	}

	if (!econ_ps_cfg_done) {
		RPU_INFO_UMACIF("%s: Didn't get ECON_PS_CFG_DONE event\n",
		       __func__);
		return -1;
	}

	RPU_DEBUG_UMACIF("%s : Received ECON_PS_CFG_DONE event\n",
						__func__);
	return 0;
}
#endif

static int wait_for_all_cmd_done(struct img_priv *priv)
{
	int count = 0;

check_all_cmd_done:

	if (cmd_info.outstanding_ctrl_req && (count < msecs_to_jiffies(1000))) {
		count++;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
		goto check_all_cmd_done;
	}

	if (cmd_info.outstanding_ctrl_req) {
		RPU_INFO_UMACIF("%s: Failed to wait all cmd done\n",
		       __func__);
		return -1;
	}

	RPU_DEBUG_UMACIF("%s : All cmd done\n",
						__func__);
	return 0;
}

int img_resume(struct ieee80211_hw *hw)
{
	//int i = 0, ret = 0;
	//int active_vif_index = -1;
	struct img_priv *priv = NULL;

	if (hw == NULL) {
		RPU_ERROR_UMACIF("%s: Invalid parameters\n",
		       __func__);
		return -EINVAL;
	}

	priv = (struct img_priv *)hw->priv;

	if (!priv->params->is_associated) {
		rk915_notify_pm(hpriv, 1);
		return 0;
	}

#if 1
	rk915_notify_pm(hpriv, 1);
	hal_ops.disable_irq_wake();
	img_suspend_status = 0;

	return 0;
#else
	mutex_lock(&priv->mutex);

	for (i = 0; i < MAX_VIFS; i++) {
		if (priv->active_vifs & (1 << i))
			active_vif_index = i;
	}

	priv->econ_ps_cfg_stats.completed = 0;
	priv->econ_ps_cfg_stats.result = 0;
	priv->econ_ps_cfg_stats.processing = 1;

	ret = rpu_prog_econ_ps_state(active_vif_index,
					     PWRSAVE_STATE_AWAKE);
	if (ret) {
		RPU_ERROR_UMACIF("%s : prog econ ps failed\n",
		       __func__);
		mutex_unlock(&priv->mutex);
		priv->econ_ps_cfg_stats.processing = 0;
		return ret;
	}

	mutex_unlock(&priv->mutex);

	if (!wait_for_econ_ps_cfg(priv)) {
		if (!priv->econ_ps_cfg_stats.result) {
			RPU_INFO_UMACIF("%s: Successful\n",
				 __func__);
			rk915_notify_pm(hpriv, 1);
			hal_ops.disable_irq_wake();
			img_suspend_status = 0;
			return 0;
		}
		RPU_INFO_UMACIF("%s: Unable to Resume\n", __func__);
	}
	priv->econ_ps_cfg_stats.processing = 0;	

	return -ETIME;
#endif	
}

int img_suspend(struct ieee80211_hw *hw,
		       struct cfg80211_wowlan *wowlan)
{
	int i = 0, ret = 0;
	int active_vif_index = -1;
	int count = 0;
	struct img_priv *priv = NULL;
	struct ieee80211_vif *vif = NULL;

	if (hw == NULL) {
		RPU_ERROR_UMACIF("%s: Invalid parameters\n",
		       __func__);
		return -EINVAL;
	}

	priv = (struct img_priv *)hw->priv;

	if (!priv->params->is_associated) {
		rk915_notify_pm(hpriv, 0);
		return ret;
	}

	if ((wifi->params.hw_scan_status == HW_SCAN_STATUS_PROGRESS) ||
		(priv->roc_params.roc_starting == 1))
		return -EBUSY;

	/*if (priv->power_save == PWRSAVE_STATE_AWAKE)
		return -EBUSY;*/

	// TODO: need to wait all outstanding cmds and tx cmd done before suspend

	mutex_lock(&priv->mutex);

	for (i = 0; i < MAX_VIFS; i++) {
		if (priv->active_vifs & (1 << i)) {
			active_vif_index = i;
			count++;
		}
	}

	if (count != 1) {
		RPU_ERROR_UMACIF("%s: Economy mode supp only for single VIF(STA mode)\n",
		       __func__);
		mutex_unlock(&priv->mutex);
		return -ENOTSUPP;
	}

	rcu_read_lock();
	vif = rcu_dereference(priv->vifs[active_vif_index]);
	rcu_read_unlock();

	if (vif->type != NL80211_IFTYPE_STATION) {
		RPU_ERROR_UMACIF("%s: VIF is not in STA Mode\n",
		       __func__);
		mutex_unlock(&priv->mutex);
		return -ENOTSUPP;
	 }

	if (priv->power_save == PWRSAVE_STATE_AWAKE) {
		priv->power_save = PWRSAVE_STATE_DOZE;
		rpu_prog_ps_state(active_vif_index, vif->addr, priv->power_save);
		if (wait_for_all_cmd_done(priv) != 0) {
			mutex_unlock(&priv->mutex);
			return -EBUSY;
		}
	}

#if 1
	mutex_unlock(&priv->mutex);

	rk915_notify_pm(hpriv, 0);
	hal_ops.enable_irq_wake();
	img_suspend_status = 1;

	return 0;
#else
	priv->econ_ps_cfg_stats.completed = 0;
	priv->econ_ps_cfg_stats.result = 0;
	priv->econ_ps_cfg_stats.wake_trig = -1;
	priv->econ_ps_cfg_stats.processing = 1;

	ret = rpu_prog_econ_ps_state(active_vif_index,
				PWRSAVE_STATE_DOZE);
	if (ret) {
		RPU_ERROR_UMACIF("%s : Error Occured\n",
		       __func__);
		mutex_unlock(&priv->mutex);
		priv->econ_ps_cfg_stats.processing = 0;
		return ret;
	}

	mutex_unlock(&priv->mutex);

	if (!wait_for_econ_ps_cfg(priv)) {
		if (!priv->econ_ps_cfg_stats.result) {
			RPU_INFO_UMACIF("%s: Successful\n",
				 __func__);
			rk915_notify_pm(hpriv, 0);
			hal_ops.enable_irq_wake();
			img_suspend_status = 1;
			return 0;
		}
		RPU_INFO_UMACIF("%s: Unable to Suspend: Active Traffic.\n", __func__);
	}
	priv->econ_ps_cfg_stats.processing = 0;

	return -ETIME;
#endif
}


static int host_suspend(void *data)
{
	/*if ((img_suspend_status == 1) && (rx_interrupt_status == 1)) {
		RPU_ERROR_UMACIF("%s: Interrupt raised during Suspend, cancel suspend",
				__func__);
		return -EBUSY;
	} else */{
		return 0;
	}
}
#endif


