/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/proc_fs.h>
#include "core.h"
#include "hal_common.h"
#include "hal_io.h"
#include "utils.h"

static int print_version = 1;

int _rpu_umac_if_init(struct proc_dir_entry **main_dir_entry)
{
	int error;

	error = proc_init(&main_dir_entry);
	if (error)
		return error;

	error = rpu_init();

	return error;
}

void _rpu_umac_if_exit(void)
{
	rpu_exit();
}

#if 0
static void prog_sleep_controller_default(void)
{
#define PWR_ON_VALUES_SIZE 2 * sizeof(unsigned int)
#define PWR_OFF_VALUES_SIZE 2 * sizeof(unsigned int)
#define RAM_ON_STATES_SIZE 2 * sizeof(unsigned int)
#define RAM_OFF_STATES_SIZE 2 * sizeof(unsigned int)
#define PWR_ON_TIMES_SIZE 14 * sizeof(unsigned int)
#define PWR_OFF_TIMES_SIZE 14 * sizeof(unsigned int)
#define RAM_ON_TIMES_SIZE 4 * sizeof(unsigned int)
#define RAM_OFF_TIMES_SIZE 4 * sizeof(unsigned int)

	/* There are the LMAC defaults, change this to customize sleep 
	 * controller configuration as per SoC.
	 */
	unsigned int pwr_on_values[PWR_ON_VALUES_SIZE] = {0x65FE, 0x5};
	unsigned int pwr_off_values[PWR_OFF_VALUES_SIZE] = {0x9A01, 0x2};
	unsigned int ram_on_states[RAM_ON_STATES_SIZE] = {0x0, 0x0};
	unsigned int ram_off_states[RAM_OFF_STATES_SIZE] = {0x3D, 0x0};
	unsigned int pwr_on_times[PWR_ON_TIMES_SIZE] = {3, 1, 4, 4, 64, 73, 64, 73, 69, 74, 75, 73, 73,0};
	unsigned int pwr_off_times[PWR_OFF_TIMES_SIZE] = {12, 14, 11, 13, 2, 1, 2, 0, 10, 5, 4, 6, 6, 15};
	unsigned int ram_on_times[RAM_ON_TIMES_SIZE] = {71, 0, 0, 0};
	unsigned int ram_off_times[RAM_OFF_TIMES_SIZE] = {5, 0, 0, 0};
	unsigned int sleep_freq = 32768 ;

	rpu_prog_pwrmgmt_pwr_on_value(pwr_on_values, PWR_ON_VALUES_SIZE);
	rpu_prog_pwrmgmt_pwr_off_value(pwr_off_values, PWR_OFF_VALUES_SIZE);
	rpu_prog_pwrmgmt_ram_on_state(ram_on_states, RAM_ON_STATES_SIZE);
	rpu_prog_pwrmgmt_ram_off_state(ram_off_states, RAM_OFF_STATES_SIZE);
	rpu_prog_pwrmgmt_pwr_on_time(pwr_on_times, PWR_ON_TIMES_SIZE);
	rpu_prog_pwrmgmt_pwr_off_time(pwr_off_times, PWR_OFF_TIMES_SIZE) ;
	rpu_prog_pwrmgmt_ram_on_time(ram_on_times, RAM_ON_TIMES_SIZE);
	rpu_prog_pwrmgmt_ram_off_time(ram_off_times, RAM_OFF_TIMES_SIZE) ;
	rpu_prog_pwrmgmt_sleep_freq(sleep_freq);

	//rpu_prog_pwrmgmt_clk_adj(-10000);
	//rpu_prog_pwrmgmt_wakeup_time(5000);
}
#endif

#ifdef RK915
static int rpu_lmac_feature_init(void)
{
#if 0
	unsigned int feature_val = 0;

	//feature_val |= LMAC_WATCHDOG_PHY_HANG_RESET_ENABLE;
	//feature_val |= LMAC_FILTER_PROBE_REQ_IN_PS_ENABLE;
	feature_val |= LMAC_FILTER_BCMC_DATA_IN_PS_ENABLE;
	feature_val |= LMAC_NULL_FRAME_IN_PS_ENABLE;
	return rpu_prog_patch_feature(feature_val);
#else
	return 0;
#endif
}
#endif

int rpu_core_init(struct img_priv *priv, unsigned int ftm)
{
	int ret = 0;
	unsigned int reset_type = LMAC_ENABLE;

	if (priv->state == STARTED)
		return ret;

	RPU_DEBUG_MAIN("%s-UMAC: Init called\n", priv->name);
	spin_lock_init(&tsf_lock);
	rpu_if_init(priv, priv->name);

	/* Enable the LMAC, set defaults and initialize TX */
	priv->reset_complete = 0;

#ifdef RPU_SLEEP_ENABLE
	reset_type |= priv->params->rpu_sleep_type;
#endif

	RPU_INFO_MAIN("%s-UMAC: Reset (ENABLE) reset_type %x\n", priv->name, reset_type);

	if (hal_ops.init_bufs(NUM_TX_DESCS,
			      NUM_RX_BUFS_2K,
			      NUM_RX_BUFS_12K,
			      priv->params->max_data_size) < 0) {
		ret = -1;
		RPU_ERROR_MAIN("%s: init_bufs failed\n", __func__);
		goto hal_stop;
	}

	if (hal_ops.start()) {
		ret = -1;
		RPU_ERROR_MAIN("%s: hal_ops.start failed\n", __func__);
		goto rpu_if_deinit;
	}

#ifndef SDIO_TXRX_STABILITY_TEST
	/* notify fw wakeup */
	rk915_notify_pm(hpriv, 1);

	if (ftm)
		CALL_RPU(rpu_prog_reset,
			  reset_type,
			  LMAC_MODE_FTM);
	else
		CALL_RPU(rpu_prog_reset,
			  reset_type,
			  LMAC_MODE_NORMAL);

	if (wait_for_reset_complete(priv, 1) < 0) {
		ret = -1;
		RPU_ERROR_MAIN("%s: wait_for_reset_complete failed\n", __func__);
		goto hal_deinit_bufs;
	}
#endif

	CALL_RPU(rpu_fw_priv_cmd, FW_PRIV_INIT, NULL);

#ifdef SDIO_TXRX_STABILITY_TEST
	priv->state = STARTED;
	CALL_RPU(rpu_prog_txrx_test, TXRX_TEST_START_TX);
#endif

#ifdef RK915
	rpu_lmac_feature_init();
#endif

	//prog_sleep_controller_default();

	CALL_RPU(rpu_prog_txpower, priv->txpower);

	rpu_tx_init(priv);

#ifdef ENABLE_DAPT
	dapt_param_init(priv);
#endif

	return 0;
hal_deinit_bufs:
	hal_ops.deinit_bufs();
prog_rpu_fail:
hal_stop:
	hal_ops.stop();
rpu_if_deinit:
	rpu_if_deinit();
	return ret;
}


void rpu_core_deinit(struct img_priv *priv, unsigned int ftm)
{
	int ret = 0;

	RPU_DEBUG_MAIN("%s-UMAC: De-init called\n", priv->name);

#ifdef ENABLE_DAPT
	dapt_param_deinit(priv);
#endif	

	/* De initialize tx  and disable LMAC*/
	rpu_tx_deinit(priv);

	if (!hpriv->fw_error) {
		/* Disable the LMAC */
		priv->reset_complete = 0;
		RPU_INFO_MAIN("%s-UMAC: Reset (DISABLE)\n", priv->name);

		if (ftm)
			CALL_RPU(rpu_prog_reset,
				  LMAC_DISABLE,
				  LMAC_MODE_FTM);
		else
			CALL_RPU(rpu_prog_reset,
				  LMAC_DISABLE,
				  LMAC_MODE_NORMAL);

		if (wait_for_reset_complete(priv, 0) < 0) {
			ret = -1;
			RPU_ERROR_MAIN("%s: wait_for_reset_complete failed\n", __func__);
			goto prog_rpu_fail;
		}

		/* notify fw sleep */
		rk915_notify_pm(hpriv, 0);
	}

prog_rpu_fail:
	wait_for_fw_error_process_complete(priv);

	rpu_if_free_outstnding();

	hal_ops.stop();
	hal_ops.deinit_bufs();

	rpu_if_deinit();

	priv->state = STOPPED;
}


void rpu_reset_complete(char *lmac_version, void *context)
{
	struct img_priv *priv = (struct img_priv *)context;

	memcpy(priv->stats->rpu_lmac_version, lmac_version, 5);
	priv->stats->rpu_lmac_version[5] = '\0';
	priv->reset_complete = 1;
	if (print_version) {
		print_version = 0;
		memcpy(priv->stats->fw_version, lmac_version+6, 20);
		priv->stats->fw_version[20] = '\0';
		RPU_INFO_MAIN("%s: Patch: %s FW: %s\n", __func__,
						priv->stats->rpu_lmac_version, priv->stats->fw_version);
	}
}

void rpu_fw_info_dump_start(void *context, unsigned int type, unsigned int reg)
{
	struct img_priv *priv = (struct img_priv *)context;

	priv->fw_info->finish = 0;
	priv->fw_info->offset = 0;
	priv->fw_info->type = type;
	if (type == DUMP_REG_INFO)
		priv->fw_info->reg = reg;
}

void rpu_fw_priv_cmd_done(struct fw_priv_cmd_done *event,
			   void *context)
{
	struct img_priv *priv = (struct img_priv *)context;

	if (priv->fw_info->offset+event->info.size >= priv->fw_info->len) {
		RPU_ERROR_MAIN("%s: fw_info buf overflow\n", __func__);
		return;
	}

	memcpy(priv->fw_info->info + priv->fw_info->offset,
					event->info.data, event->info.size);
	priv->fw_info->type = event->hdr.descriptor_id;
	priv->fw_info->offset += event->info.size;
	if (event->info.end) {
		priv->fw_info->info[priv->fw_info->offset] = 0;
		priv->fw_info->finish = 1;
	}

	if (event->hdr.descriptor_id == DUMP_FW_CRASH_INFO) {
		RPU_ERROR_MAIN("\n%s\n", priv->fw_info->info);
	}
}

void rpu_mac_stats(struct umac_event_mac_stats *mac_stats,
			   void *context)
{
	struct img_priv *priv = (struct img_priv *)context;

	/* TX related */
	priv->stats->roc_start = mac_stats->roc_start;
	priv->stats->roc_stop = mac_stats->roc_stop;
	priv->stats->roc_complete = mac_stats->roc_complete;
	priv->stats->roc_stop_complete = mac_stats->roc_stop_complete;
	priv->stats->tx_cmd_cnt = mac_stats->tx_cmd_cnt;
	priv->stats->tx_done_cnt = mac_stats->tx_done_cnt;
	priv->stats->tx_edca_trigger_cnt = mac_stats->tx_edca_trigger_cnt;
	priv->stats->tx_edca_isr_cnt = mac_stats->tx_edca_isr_cnt;
	priv->stats->tx_start_cnt = mac_stats->tx_start_cnt;
	priv->stats->tx_abort_cnt = mac_stats->tx_abort_cnt;
	priv->stats->tx_abort_isr_cnt = mac_stats->tx_abort_isr_cnt;
	priv->stats->tx_underrun_cnt = mac_stats->tx_underrun_cnt;
	priv->stats->tx_rts_cnt = mac_stats->tx_rts_cnt;
	priv->stats->tx_ampdu_cnt = mac_stats->tx_ampdu_cnt;
	priv->stats->tx_mpdu_cnt = mac_stats->tx_mpdu_cnt;
	priv->stats->tx_crypto_post = mac_stats->tx_crypto_post;
	priv->stats->tx_crypto_done = mac_stats->tx_crypto_done;
	priv->stats->rx_pkt_to_umac = mac_stats->rx_pkt_to_umac;
	priv->stats->rx_crypto_post = mac_stats->rx_crypto_post;
	priv->stats->rx_crypto_done = mac_stats->rx_crypto_done;
	/* RX related */
	priv->stats->rx_isr_cnt = mac_stats->rx_isr_cnt;
	priv->stats->rx_ack_cts_to_cnt = mac_stats->rx_ack_cts_to_cnt;
	priv->stats->rx_cts_cnt = mac_stats->rx_cts_cnt;
	priv->stats->rx_ack_resp_cnt = mac_stats->rx_ack_resp_cnt;
	priv->stats->rx_ba_resp_cnt = mac_stats->rx_ba_resp_cnt;
	priv->stats->rx_fail_in_ba_bitmap_cnt =
		mac_stats->rx_fail_in_ba_bitmap_cnt;
	priv->stats->rx_circular_buffer_free_cnt =
		mac_stats->rx_circular_buffer_free_cnt;
	priv->stats->rx_mic_fail_cnt = mac_stats->rx_mic_fail_cnt;

	/* HAL related */
	priv->stats->hal_cmd_cnt = mac_stats->hal_cmd_cnt;
	priv->stats->hal_event_cnt = mac_stats->hal_event_cnt;
	priv->stats->hal_ext_ptr_null_cnt = mac_stats->hal_ext_ptr_null_cnt;

	/* LPW PHY Related */
	priv->stats->csync_timeout_cntr = mac_stats->csync_timeout_cntr;
	priv->stats->fsync_timeout_cntr = mac_stats->fsync_timeout_cntr;
	priv->stats->acdrop_timeout_cntr = mac_stats->acdrop_timeout_cntr;
	priv->stats->csync_abort_agctrig_cntr = mac_stats->csync_abort_agctrig_cntr;
	priv->stats->crc_success_cnt = mac_stats->crc_success_cnt;
	priv->stats->crc_fail_cnt = mac_stats->crc_fail_cnt;
#ifdef RPU_SLEEP_ENABLE
	priv->stats->rpu_boot_cnt = mac_stats->rpu_boot_cnt;
	memcpy(priv->stats->sleep_stats, mac_stats->sleep_stats,
		sizeof(priv->stats->sleep_stats));
#endif
}
void rpu_rf_calib_data(struct umac_event_rf_calib_data *rf_data,
			       void *context)
{
	struct img_priv  *priv = (struct img_priv *)context;

	if (rf_data->rf_calib_data_length > MAX_RF_CALIB_DATA) {
		RPU_ERROR_MAIN("%s: RF calib data exceeded the max size: %d\n",
			    __func__,
			    MAX_RF_CALIB_DATA);
		return;
	}
	priv->stats->rf_calib_data_length = rf_data->rf_calib_data_length;
	memset(priv->stats->rf_calib_data, 0x00,
	       MAX_RF_CALIB_DATA);
	memcpy(priv->stats->rf_calib_data, rf_data->rf_calib_data,
	       rf_data->rf_calib_data_length);
}



void rpu_ch_prog_complete(int event,
				  struct umac_event_ch_prog_complete *prog_ch,
				  void *context)
{
	struct img_priv *priv = (struct img_priv *)context;

	priv->chan_prog_done = 1;
}

int rk915_wait_fw_ready_to_sleep(void)
{
    struct img_priv *imgpriv =
            wifi ? wifi->hw->priv : NULL;

    if (imgpriv) {
        imgpriv->read_csr_complete = 0;
        imgpriv->read_csr_value = 0;
        rpu_prog_read_csr(0xbf2);
        wait_for_read_csr_cmp(imgpriv);
        //RPU_INFO_MAIN("read_csr_value = %x\n", imgpriv->read_csr_value);

        if (imgpriv->read_csr_value & (1<<14)) {
            imgpriv->read_csr_complete = 0;
            imgpriv->read_csr_value = 0;
            rpu_prog_read_csr(0xbf2);
            wait_for_read_csr_cmp(imgpriv);
            //RPU_INFO_MAIN("read_csr_value = %x\n", imgpriv->read_csr_value);
        }

        if (imgpriv->read_csr_value & (1<<15)) {
            return 1;
        }
        return 0;
    }

    return 0;
}

