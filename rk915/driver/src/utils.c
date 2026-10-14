/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "core.h"
#include "if_io.h"

int wait_for_fw_error_cmd_done(struct img_priv *priv);

int conv_str_to_byte(unsigned char *byte,
		     unsigned char *str,
		     int len)
{
	int  i, j = 0;
	unsigned char ch, val = 0;

	for (i = 0; i < (len * 2); i++) {
		/*convert to lower*/
		ch = ((str[i] >= 'A' && str[i] <= 'Z') ? str[i] + 32 : str[i]);

		if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
			return -1;

		if (ch >= '0' && ch <= '9')  /*check is digit*/
			ch = ch - '0';
		else
			ch = ch - 'a' + 10;

		val += ch;

		if (!(i%2))
			val <<= 4;
		else {
			byte[j] = val;
			j++;
			val = 0;
		}
	}

	return 0;
}

int wait_for_scan_abort(struct img_priv *priv)
{
	int count;

	count = 0;

check_scan_abort_complete:
	if (!hpriv->fw_error && !priv->scan_abort_done && (count < SCAN_ABORT_TIMEOUT_TICKS)) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (schedule_timeout(1) == 0)
			count++;

		goto check_scan_abort_complete;
	}

	if (!priv->scan_abort_done) {
		RPU_ERROR_SCAN("%s-UMAC: No SCAN_ABORT_DONE after %ld ticks\n",
			   priv->name, SCAN_ABORT_TIMEOUT_TICKS);
		return 0;
	}

	RPU_INFO_SCAN("%s-UMAC: Scan abort complete after %d timer ticks\n",
					priv->name,
					count);

	return 0;

}

int wait_for_scan_complete(struct img_priv *priv)
{
        int count;

        count = 0;

check_scan_complete:
        if (!hpriv->fw_error && wifi->params.hw_scan_status != HW_SCAN_STATUS_NONE &&
		(count < msecs_to_jiffies(5000))) {
                set_current_state(TASK_INTERRUPTIBLE);

                if (schedule_timeout(1) == 0)
                        count++;

                goto check_scan_complete;
        }

        if (wifi->params.hw_scan_status != HW_SCAN_STATUS_NONE) {
                RPU_ERROR_SCAN("%s-UMAC: No Scan complete after %ld ticks\n",
                           priv->name, msecs_to_jiffies(5000));
                return 0;
        }

        RPU_INFO_SCAN("%s-UMAC: Scan complete after %d timer ticks\n",
                                        priv->name,
                                        count);

        return 0;

}

int wait_for_cancel_hw_roc(struct img_priv *priv)
{
	int count = 0;

check_cancel_hw_roc_complete:
	if (!hpriv->fw_error && !priv->cancel_hw_roc_done && (count < CANCEL_HW_ROC_TIMEOUT_TICKS)) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (schedule_timeout(1) == 0)
			count++;
		goto check_cancel_hw_roc_complete;
	}

	if (!priv->cancel_hw_roc_done) {
		RPU_ERROR_ROC("%s-UMAC: Warning: Didn't get CANCEL_HW_ROC_DONE after %ld timer ticks\n",
		       priv->name,
		       CANCEL_HW_ROC_TIMEOUT_TICKS);
		if (hpriv->fw_error_processing)
			return 0;
		return -1;
	}

	RPU_DEBUG_ROC("%s-UMAC: Cancel HW RoC complet after %d timer ticks\n",
					priv->name,
					count);

	return 0;

}

int wait_for_channel_prog_complete(struct img_priv *priv)
{
	int count;

	count = 0;

	if (hpriv->during_pm_resume)
		return 0;

check_ch_prog_complete:
	if (!hpriv->fw_error && !priv->chan_prog_done && (count < CH_PROG_TIMEOUT_TICKS)) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (schedule_timeout(1) == 0)
			count++;

		goto check_ch_prog_complete;
	}

	if (!priv->chan_prog_done) {
		RPU_ERROR_UMACIF("%s-UMAC: No channel prog done after %ld ticks\n",
			   priv->name, CH_PROG_TIMEOUT_TICKS);
		return -1;
	}

	RPU_DEBUG_UMACIF("%s-UMAC: Channel Prog Complete after %d timer ticks\n",
			priv->name, count);

	return 0;

}


int wait_for_reset_complete(struct img_priv *priv, int enable)
{
	int count;
	int timeout;

	count = 0;

	if (enable)
		timeout = RESET_TIMEOUT_TICKS;
	else
		timeout = msecs_to_jiffies(3000);

check_reset_complete:
	if (/*!hpriv->fw_error &&*/ !priv->reset_complete && (count < timeout)) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (schedule_timeout(1) == 0)
			count++;

		goto check_reset_complete;
	}

	if (!priv->reset_complete) {
		RPU_ERROR_MAIN("%s-UMAC: No reset complete after %d ticks\n",
			   priv->name, timeout);
		rk915_signal_io_error(FW_ERR_RESET_CMD);
		wait_for_fw_error_cmd_done(priv);
		return -1;
	}

	RPU_DEBUG_MAIN("%s-UMAC: Reset complete after %d timer ticks\n",
		   priv->name, count);
	return 0;

}

int wait_for_read_csr_cmp(struct img_priv *priv)
{
	int count;

	count = 0;

check_read_csr_complete:
	if (!hpriv->fw_error && !priv->read_csr_complete && (count < msecs_to_jiffies(1000))) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (schedule_timeout(1) == 0)
			count++;

		goto check_read_csr_complete;
	}

	if (!priv->read_csr_complete) {
		RPU_ERROR_SCAN("%s-UMAC: No read_csr_complete after %ld ticks\n",
			   priv->name, msecs_to_jiffies(1000));
		return 0;
	}

	RPU_INFO_SCAN("%s-UMAC: read_csr_complete after %d timer ticks\n",
					priv->name,
					count);

	return 0;

}

#ifdef RPU_SLEEP_ENABLE
int wait_for_hp_ready_blocking_sleep(void)
{
	int count;

	count = 0;

check_rpu_ready:
	if (!waiting_for_rpu_ready && (count < RPU_READY_TIMEOUT_TICKS)) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (0 == schedule_timeout(1))
			count++;

		goto check_rpu_ready;
	}

	if (!waiting_for_rpu_ready) {
		RPU_ERROR_MAIN("%s-UMAC: No RPU ready interrupt after %ld ticks\n",
			   __func__, RPU_READY_TIMEOUT_TICKS);
		return -1;
	}

	RPU_DEBUG_MAIN("%s-UMAC: RPU is ready after %d timer ticks\n",
					__func__,
					count);

	return 0;

}

int wait_for_hp_ready_blocking_busy_wait(void)
{
	int count;
	unsigned long start = 0;

	count = 0;

	start = jiffies;

	while (!waiting_for_rpu_ready &&
	     time_before(jiffies, start + msecs_to_jiffies(1000))) {
		cpu_relax();
	}


	if (!waiting_for_rpu_ready) {
		RPU_ERROR_MAIN("%s-UMAC: No RPU ready interrupt after %ld ticks\n",
			   hal_name, RPU_READY_TIMEOUT_TICKS);
		return -1;
	}

	RPU_DEBUG_MAIN("%s-UMAC: RPU is ready after %d timer ticks\n",
					hal_name,
					count);

	return 0;

}
#endif

int wait_for_fw_error_process_complete(struct img_priv *priv)
{
	int count;

	count = 0;

fw_error_processing_complete:
	if (hpriv->fw_error_processing && (count < FW_ERR_PROCESS_TIMEOUT_TICKS)) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (schedule_timeout(1) == 0)
			count++;

		goto fw_error_processing_complete;
	}

	if (hpriv->fw_error_processing) {
		RPU_ERROR_UMACIF("%s-UMAC: No fw_error_process complete after %ld ticks\n",
			   priv->name, FW_ERR_PROCESS_TIMEOUT_TICKS);
		return -1;
	}

	RPU_DEBUG_UMACIF("%s-UMAC: fw_error_process complete after %d timer ticks\n",
			priv->name, count);

	return 0;

}

int wait_for_fw_error_cmd_done(struct img_priv *priv)
{
	int count;

	count = 0;

fw_error_cmd_done:
	if (!hpriv->fw_error_cmd_done && (count < msecs_to_jiffies(1000))) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (schedule_timeout(1) == 0)
			count++;

		goto fw_error_cmd_done;
	}

	if (!hpriv->fw_error_cmd_done) {
		RPU_ERROR_UMACIF("No fw_error_cmd done after %ld ticks\n",
			   msecs_to_jiffies(1000));
		return -1;
	}

	RPU_DEBUG_UMACIF("fw_error_cmd_done after %d timer ticks\n",
			count);

	return 0;

}

int wait_for_pm_resume_done(struct img_priv *priv)
{
	int count;

	count = 0;

pm_resume_done:
	if (hpriv->during_pm_resume && (count < msecs_to_jiffies(1000))) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (schedule_timeout(1) == 0)
			count++;

		goto pm_resume_done;
	}

	if (hpriv->during_pm_resume) {
		RPU_ERROR_UMACIF("No pm_resume done after %ld ticks\n",
			   msecs_to_jiffies(1000));
		return -1;
	}

	RPU_DEBUG_UMACIF("pm_resume done after %d timer ticks\n",
			count);

	return 0;

}

int wait_for_rxq_empty(struct img_priv *priv)
{
	int count;

	count = 0;

rxq_empty:
	if (skb_queue_len(&hpriv->rxq) > 0 && (count < RXQ_EMPTY_TIMEOUT_TICKS)) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (schedule_timeout(1) == 0)
			count++;

		goto rxq_empty;
	}

	if (skb_queue_len(&hpriv->rxq) > 0) {
		RPU_ERROR_ROCOVERY("rxq not empty after %ld ticks\n",
			   RXQ_EMPTY_TIMEOUT_TICKS);
		return -1;
	}

	RPU_DEBUG_ROCOVERY("wait_for_rxq_empty complete after %d timer ticks\n",
			count);

	return 0;

}

void update_aux_adc_voltage(struct img_priv *priv,
				   unsigned char pdout)
{
	static unsigned int index;

	if (index > MAX_AUX_ADC_SAMPLES)
		index = 0;

	priv->params->pdout_voltage[index++] = pdout;
}

/*
 * find iterface index of main interface of wlan0
 */
int find_main_iface(struct img_priv *priv)
{
	int i, index = MAX_VIFS;

	for (i = 0; i < MAX_VIFS; i++) {
		if (priv->vifs[i] &&
			ether_addr_equal(priv->vifs[i]->addr, vif_macs[0])) {
			index = i;
			break;
		}
	}
	return index;
}

/*
 * find iterface index of main interface of p2p0
 */
int find_p2p_iface(struct img_priv *priv)
{
	int i, index = MAX_VIFS;

	for (i = 0; i < MAX_VIFS; i++) {
		if (priv->vifs[i] &&
			ether_addr_equal(priv->vifs[i]->addr, vif_macs[1])) {
			index = i;
			break;
		}
	}
	return index;
}

/*
 * is main interface wlan0
 */
bool is_main_iface(u8 *if_addr)
{
	if (ether_addr_equal(if_addr, vif_macs[0])) {
		return true;
	}
	return false;
}
