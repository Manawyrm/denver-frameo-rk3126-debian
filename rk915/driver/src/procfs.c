/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/proc_fs.h>
#include <linux/moduleparam.h>

#include "core.h"
#include "utils.h"
#include "version.h"
#include "hal_io.h"

/*
 * Keep the LMAC awake: the driver has no wake path (the wake functions are
 * empty stubs, also in the stock driver), and a sleeping chip reads back
 * 0xF0F0 once the link goes idle (from PR #1 of the driver port).
 */
unsigned int lpw_no_sleep = 1;
module_param(lpw_no_sleep, uint, 0444);
MODULE_PARM_DESC(lpw_no_sleep, "keep the LMAC awake (default 1)");

unsigned int default_phy_threshold = DAPT_DEFAULT_PHY_THRESH;
module_param(default_phy_threshold, int, 0);
MODULE_PARM_DESC(default_phy_threshold, "default phy threshold");

struct wifi_dev *wifi;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
#undef IEEE80211_BAND_2GHZ
#define IEEE80211_BAND_2GHZ NL80211_BAND_2GHZ
#endif

#ifdef RPU_SLEEP_ENABLE
#include "sdio.h"
static int proc_read_sleep_stats(struct seq_file *m, void *v)
{
	int i;
	for(i=0;i<12;i++)
		seq_printf(m, "stats[%d] = %d\n", i,
			wifi->stats.sleep_stats[i]);
	seq_printf(m, "rpu_boot_cnt=%d\n",
		   wifi->stats.rpu_boot_cnt);
	seq_printf(m, "fw state: %d\n", rk915_readb(hpriv, IO_FW_STATE));

	return 0;
}

static int proc_open_sleep_stats(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read_sleep_stats, NULL);
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
static const struct proc_ops params_fops_sleep_stats = {
    .proc_open = proc_open_sleep_stats,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_write = NULL,
    .proc_release = single_release
};
#else
static const struct file_operations params_fops_sleep_stats = {
	.open = proc_open_sleep_stats,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = NULL,
	.release = single_release
};
#endif
#endif

static int proc_read_config(struct seq_file *m, void *v)
{
	int i = 0;
	int rf_params_size = sizeof(wifi->params.rf_params) /
			     sizeof(wifi->params.rf_params[0]);
	struct img_priv *priv;
        unsigned int nss, flags;
        int mcs;
	
	if (!wifi->hw)
		return -ENODEV;

	priv = ((struct img_priv *)(wifi->hw->priv));

#ifdef HAL_PCIE
	seq_puts(m, "*********read pci_mem_region************\n");
	seq_printf(m,"pci_base_addr= %x\n",wifi->params.pci_base_addr);
	seq_printf(m,"num_words=%d\n",wifi->params.no_words);
#endif
	seq_puts(m, "************* Configurable Parameters ***********\n");
	seq_printf(m, "sensitivity = %d\n", wifi->params.ed_sensitivity);
	seq_printf(m, "auto_sensitivity = %d\n", wifi->params.auto_sensitivity);
	/*RF Input params*/
	seq_puts(m, "rf_params =");
	for (i = 0; i < rf_params_size; i++)
		seq_printf(m, " %02X", wifi->params.rf_params[i]);

	seq_puts(m, "\n");

	seq_puts(m, "rf_params_vpd =");
	for (i = 0; i < rf_params_size; i++)
		seq_printf(m, " %02X", wifi->params.rf_params_vpd[i]);

	seq_puts(m, "\n");
	seq_printf(m, "bypass_vpd = %d\n", wifi->params.bypass_vpd);
	seq_printf(m, "production_test = %d\n", wifi->params.production_test);
	seq_printf(m, "enable_early_agg_checks = %d\n",
		   wifi->params.enable_early_agg_checks);
	seq_printf(m, "antenna_sel (RPU Init) = %d\n",
		   wifi->params.antenna_sel);
	seq_printf(m, "max_data_size = %d (%dK)\n",
		   wifi->params.max_data_size,
		   wifi->params.max_data_size/1024);
	seq_printf(m, "max_tx_cmds = %d\n",
		   wifi->params.max_tx_cmds);
	seq_printf(m, "disable_power_save (Disables all power save's) = %d\n",
		   wifi->params.disable_power_save);
	seq_printf(m, "disable_sm_power_save (Disables MIMO PS only) = %d\n",
		   wifi->params.disable_sm_power_save);

	seq_printf(m, "num_vifs = %d\n",
		   wifi->params.num_vifs);

	seq_printf(m, "chnl_bw = %d\n",
		   wifi->params.chnl_bw);

	seq_printf(m, "rate_protection_type = %d (0: Disable, 1: Enable)\n",
		   wifi->params.rate_protection_type);
	if (wifi->params.production_test) {
		mcs = wifi->params.tx_fixed_mcs_indx;
		flags = wifi->params.prod_mode_rate_flag;
		seq_puts(m, "***Production Mode Rate config options\n");
		seq_printf(m, "tx_fixed_mcs_indx = %d (%s)\n",
			   mcs, get_string_from_rate(mcs, flags));
		if (wifi->params.tx_fixed_rate > -1) {
			if (wifi->params.tx_fixed_rate == 55)
				seq_puts(m, "tx_fixed_rate = 5.5\n");
			else
				seq_printf(m, "tx_fixed_rate = %d\n",
					   wifi->params.tx_fixed_rate);
		} else
			seq_printf(m, "tx_fixed_rate = %d\n",
				   wifi->params.tx_fixed_rate);
	} else {
		seq_puts(m, "***Unicast Fixed rate config options\n");
		mcs = wifi->params.mgd_mode_tx_fixed_mcs_indx;
		flags = wifi->params.prod_mode_rate_flag;
		seq_printf(m, "mgd_mode_tx_fixed_mcs_indx = %d (%s)\n",
			   mcs, get_string_from_rate(mcs, flags));
		if (wifi->params.mgd_mode_tx_fixed_rate > -1) {
			if (wifi->params.mgd_mode_tx_fixed_rate == 55)
				seq_puts(m, "mgd_mode_tx_fixed_rate = 5.5\n");
			else
				seq_printf(m, "mgd_mode_tx_fixed_rate = %d\n",
					   wifi->params.mgd_mode_tx_fixed_rate);
		} else
			seq_printf(m, "mgd_mode_tx_fixed_rate = %d\n",
				   wifi->params.mgd_mode_tx_fixed_rate);
	}

	if (wifi->params.prod_mode_rate_flag & ENABLE_11N_FORMAT)
		nss = wifi->params.mgd_mode_tx_fixed_mcs_indx/8 + 1;
	else
		nss = wifi->params.num_spatial_streams;

	seq_puts(m, "Bits:11N-NA-NA-NA\n");
	seq_printf(m, "prod_mode_rate_flag = %d\n",
		   wifi->params.prod_mode_rate_flag);
	seq_printf(m, "prod_mode_rate_preamble_type (0: Short, 1: Long) = %d\n",
		   wifi->params.prod_mode_rate_preamble_type);

	seq_puts(m, "***Multicast Fixed rate config options\n");

	mcs = wifi->params.mgd_mode_mcast_fixed_data_rate;
	flags = wifi->params.mgd_mode_mcast_fixed_rate_flags;
	if ((mcs != -1) && (mcs & 0x80))
		mcs = mcs & 0x7F;

	if (flags & ENABLE_11N_FORMAT)
		nss = mcs/8 + 1;
	else
		nss = wifi->params.mgd_mode_mcast_fixed_nss;


	seq_printf(m, "mgd_mode_mcast_fixed_data_rate = %d (%s)\n",
		   mcs, get_string_from_rate(mcs, flags));
	seq_puts(m, "Bits:11N-NA-NA-NA\n");
	seq_printf(m, "mgd_mode_mcast_fixed_rate_flags = %d\n",
		   wifi->params.mgd_mode_mcast_fixed_rate_flags);
	seq_printf(m, "mgd_mode_mcast_fixed_preamble = %d\n",
		   wifi->params.mgd_mode_mcast_fixed_preamble);
	seq_printf(m, "bg_scan_enable = %d\n",
		   wifi->params.bg_scan_enable);
	seq_puts(m, "bg_scan_channel_list =");

	for (i = 0; i < wifi->params.bg_scan_num_channels;  i++) {
		if (wifi->params.bg_scan_channel_list[i])
			seq_printf(m, " %d",
				   wifi->params.bg_scan_channel_list[i]);
	}

	seq_puts(m, "\n");
	seq_puts(m, "bg_scan_channel_flags =");

	for (i = 0; i < wifi->params.bg_scan_num_channels;  i++) {
		if (wifi->params.bg_scan_channel_flags[i])
			seq_printf(m, " %d",
				   wifi->params.bg_scan_channel_flags[i]);
	}

	seq_puts(m, "\n");
	seq_printf(m, "bg_scan_intval = %dms\n",
		   wifi->params.bg_scan_intval/1000);
	seq_printf(m, "bg_scan_num_channels = %d\n",
		   wifi->params.bg_scan_num_channels);
	seq_printf(m, "scan_type = %d (PASSIVE: 0, ACTIVE: 1)\n",
		   wifi->params.scan_type);
 	seq_printf(m, "fw_loaded = %d\n", wifi->params.fw_loaded);


	seq_puts(m, "****** Production Test (or) FTM Parameters *******\n");
	seq_printf(m, "start_packet_gen = %d (-1: Infinite loop)\n",
		   wifi->params.pkt_gen_val);
	seq_printf(m, "payload_length = %d bytes\n",
		   wifi->params.payload_length);
	seq_printf(m, "start_prod_mode = channel: %d\n",
		   wifi->params.start_prod_mode);
	seq_printf(m, "fw_skip_rx_pkt_submit = %d\n",
		   wifi->params.fw_skip_rx_pkt_submit);	

	if (ftm || wifi->params.production_test)
		seq_printf(m, "set_tx_power = %d dB\n",
			   wifi->params.set_tx_power);

	seq_printf(m, "center_frequency = %d\n",
		   ieee80211_frequency_to_channel(priv->cur_chan.center_freq1));

	if (ftm)
		seq_printf(m, "aux_adc_chain_id = %d\n",
			   wifi->params.aux_adc_chain_id);
#ifdef RPU_SLEEP_ENABLE
	seq_printf(m, "sleep_status=%x\n", hal_ops.rpu_sleep_status());
	seq_puts(m, "RPU Sleep Type 0: Sleep Enabled, 32: Sleep Disabled)\n");
	seq_printf(m, "rpu_sleep_type= %d\n", wifi->params.rpu_sleep_type);
#endif
	seq_puts(m, "RPU Runtime Debug Support Configuration.\n");
	seq_printf(m, "rpu_debug = %d.\n", rpu_debug);
#define PRINT_DEBUG_MOD(MOD_NAME, MOD_ID) \
	do {\
		seq_printf(m, "***rpu_debug: %s: val= %d, status= %s\n", MOD_NAME, MOD_ID, rpu_debug & MOD_ID ? "ENABLED": "DISABLED");\
	} while(0);\

	PRINT_DEBUG_MOD("SCAN", RPU_DEBUG_SCAN);
	PRINT_DEBUG_MOD("ROC", RPU_DEBUG_ROC);
	PRINT_DEBUG_MOD("TX", RPU_DEBUG_TX);
	PRINT_DEBUG_MOD("MAIN", RPU_DEBUG_MAIN);
	PRINT_DEBUG_MOD("RPU_IF", RPU_DEBUG_IF);
	PRINT_DEBUG_MOD("UMACIF", RPU_DEBUG_UMACIF);
	PRINT_DEBUG_MOD("RX", RPU_DEBUG_RX);
	PRINT_DEBUG_MOD("HAL", RPU_DEBUG_HAL);
	PRINT_DEBUG_MOD("CRYPTO", RPU_DEBUG_CRYPTO);
	PRINT_DEBUG_MOD("TX_DUMP", RPU_DEBUG_DUMP_TX);
	PRINT_DEBUG_MOD("RX_DUMP", RPU_DEBUG_DUMP_RX);
	PRINT_DEBUG_MOD("HAL_DUMP", RPU_DEBUG_DUMP_HAL);

	seq_puts(m, "HELP: Add the values beside each module and\n");
	seq_puts(m, " echo rpu_debug=<SUM> to enable logging\n");
	seq_puts(m, " for those modules.\n");

	seq_puts(m, "To see the updated stats\n");
	seq_puts(m, "please run: echo get_stats=1 > /proc/rpu/params\n");
	seq_puts(m, "To read/write fw register\n");
	seq_puts(m, "for example: read  reg 0x42000000, len is 4 bytes (max len is 1024 bytes)\n");
	seq_puts(m, "for example: write val 0x10101010 to reg 0x42000000\n");
	seq_puts(m, "please run for read  : echo read_fw_reg =0x42000000,0x4 > /proc/rpu/params\n");
	seq_puts(m, "please run for wirete: echo write_fw_reg=0x42000000,0x10101010 > /proc/rpu/params\n");
	seq_puts(m, "************* VERSION ***********\n");
	seq_printf(m, "RPU_DRIVER_VERSION = %s\n", RPU_DRIVER_VERSION);

	if (wifi->hw &&
	    (((struct img_priv *)(wifi->hw->priv))->state != STARTED)) {
		seq_printf(m, "LMAC_VERSION = %s\n", "UNKNOWN");
		seq_printf(m, "Firmware version = %s\n", "UNKNOWN");
	} else {
		seq_printf(m, "LMAC_VERSION = %s\n",
			   wifi->stats.rpu_lmac_version);
		seq_printf(m, "Firmware version= %d.%d\n",
			   (wifi->stats.rpu_lmac_version[0] - '0'),
			   (wifi->stats.rpu_lmac_version[2] - '0'));
		seq_printf(m, "FW version= %s\n", wifi->stats.fw_version);
	}

	return 0;
}


static int proc_read_phy_stats(struct seq_file *m, void *v)
{
	int i = 0;

	seq_puts(m, "************* BB Stats ***********\n");

	seq_printf(m, "csync_timeout_cntr  =%x\n",
		   wifi->stats.csync_timeout_cntr);
	seq_printf(m, "fsync_timeout_cntr  =%x\n",
		   wifi->stats.fsync_timeout_cntr);
	seq_printf(m, "acdrop_timeout_cntr  =%x\n",
		   wifi->stats.acdrop_timeout_cntr);
	seq_printf(m, "csync_abort_agctrig_cntr  =%x\n",
		   wifi->stats.csync_abort_agctrig_cntr);
	seq_printf(m, "crc_success_cnt  =%d\n",
		   wifi->stats.crc_success_cnt);
	seq_printf(m, "crc_fail_cnt  =%d\n",
		   wifi->stats.crc_fail_cnt);

	seq_puts(m, "************* RF Stats ***********\n");
	/*RF output data*/
	seq_puts(m, "rf_calib_data =");
	for (i = 0; i < wifi->stats.rf_calib_data_length; i++)
		seq_printf(m, "%02X", wifi->stats.rf_calib_data[i]);

	seq_puts(m, "\n");
	return 0;
}

static void dump_tx_buff_info(struct seq_file *m, struct tx_config *tx)
{
	int i, j;
	struct sk_buff_head *pend_pkt_q;

	seq_printf(m, "tx_buff_pool_map (LE) = \n\t");
	for (i = 0; i < NUM_TX_DESCS; i++) {
		if (test_bit(i, &tx->buf_pool_bmp[0]))
			seq_printf(m, "1 ");
		else
			seq_printf(m, "0 ");
		if (((i+1)%5) == 0)
			seq_printf(m, ", ");
	}
	seq_printf(m, "\n");

	seq_printf(m, "outstanding_pkts = \n\t");
	for (i = 0; i < NUM_TX_DESCS; i++) {
		seq_printf(m, "%d ", tx->outstanding_pkts[i]);
		if (((i+1)%5) == 0)
			seq_printf(m, ", ");
	}
	seq_printf(m, "\n");	

	seq_printf(m, "outstanding_tokens = \n\t");
	for (i = 0; i < NUM_ACS; i++) {
		seq_printf(m, "%d ", tx->outstanding_tokens[i]);
	}
	seq_printf(m, "\n");

	seq_printf(m, "curr_peer_opp = \n\t");
	for (i = 0; i < NUM_ACS; i++) {
		seq_printf(m, "%d ", tx->curr_peer_opp[i]);
	}
	seq_printf(m, "\n");

	seq_printf(m, "queue_stopped_bmp = \n\t");
	for (i = 0; i < NUM_ACS; i++) {
		if (tx->queue_stopped_bmp & (1 << i))
			seq_printf(m, "1 ");
		else
			seq_printf(m, "0 ");
	}
	seq_printf(m, "\n");

	seq_printf(m, "pending_pkt = \n");
	for (j = 0; j < MAX_PEND_Q_PER_AC; j++) {
		seq_printf(m, "\t");
		for (i = 0; i < NUM_ACS; i++) {
			pend_pkt_q = &tx->pending_pkt[j][i];
			seq_printf(m, "%03d ", skb_queue_len(pend_pkt_q));
		}
		seq_printf(m, "\n");
	}
}

static int proc_read_mac_stats(struct seq_file *m, void *v)
{
	unsigned int index;
	unsigned int total_samples = 0;
	unsigned int total_value = 0;
	int total_rssi_samples = 0;
	int total_rssi_value = 0;
	struct img_priv *priv = NULL;

	if (!wifi->hw)
		return -ENODEV;

	priv= (struct img_priv *)(wifi->hw->priv);

	if (ftm) {
		for (index = 0; index < MAX_AUX_ADC_SAMPLES; index++) {
			if (!wifi->params.pdout_voltage[index])
				continue;

			total_value += wifi->params.pdout_voltage[index];
			total_samples++;
		}
	}

	for (index = 0; index < MAX_RSSI_SAMPLES; index++) {

		if (!wifi->params.production_test)
			break;

		if (!wifi->params.rssi_average[index])
			continue;

		total_rssi_value += wifi->params.rssi_average[index];
		total_rssi_samples++;
	}

	seq_puts(m, "************* UMAC STATS ***********\n");
	seq_printf(m, "rx_packet_mgmt_count = %d\n",
		   wifi->stats.rx_packet_mgmt_count);
	seq_printf(m, "rx_packet_data_count = %d\n",
		   wifi->stats.rx_packet_data_count);
	seq_printf(m, "tx_packet_count(HT MCS0) = %d\n",
		   wifi->stats.ht_tx_mcs0_packet_count);
	seq_printf(m, "tx_packet_count(HT MCS1) = %d\n",
		   wifi->stats.ht_tx_mcs1_packet_count);
	seq_printf(m, "tx_packet_count(HT MCS2) = %d\n",
		   wifi->stats.ht_tx_mcs2_packet_count);
	seq_printf(m, "tx_packet_count(HT MCS3) = %d\n",
		   wifi->stats.ht_tx_mcs3_packet_count);
	seq_printf(m, "tx_packet_count(HT MCS4) = %d\n",
		   wifi->stats.ht_tx_mcs4_packet_count);
	seq_printf(m, "tx_packet_count(HT MCS5) = %d\n",
		   wifi->stats.ht_tx_mcs5_packet_count);
	seq_printf(m, "tx_packet_count(HT MCS6) = %d\n",
		   wifi->stats.ht_tx_mcs6_packet_count);
	seq_printf(m, "tx_packet_count(HT MCS7) = %d\n",
		   wifi->stats.ht_tx_mcs7_packet_count);

	if (wifi->params.uccp_num_spatial_streams == 2) {
		seq_printf(m, "tx_packet_count(HT MCS8) = %d\n",
			   wifi->stats.ht_tx_mcs8_packet_count);
		seq_printf(m, "tx_packet_count(HT MCS9) = %d\n",
			   wifi->stats.ht_tx_mcs9_packet_count);
		seq_printf(m, "tx_packet_count(HT MCS10) = %d\n",
			   wifi->stats.ht_tx_mcs10_packet_count);
		seq_printf(m, "tx_packet_count(HT MCS11) = %d\n",
			   wifi->stats.ht_tx_mcs11_packet_count);
		seq_printf(m, "tx_packet_count(HT MCS12) = %d\n",
			   wifi->stats.ht_tx_mcs12_packet_count);
		seq_printf(m, "tx_packet_count(HT MCS13) = %d\n",
			   wifi->stats.ht_tx_mcs13_packet_count);
		seq_printf(m, "tx_packet_count(HT MCS14) = %d\n",
			   wifi->stats.ht_tx_mcs14_packet_count);
		seq_printf(m, "tx_packet_count(HT MCS15) = %d\n",
			   wifi->stats.ht_tx_mcs15_packet_count);
	}
	seq_printf(m, "tx_cmds_from_stack= %d\n",
		   wifi->stats.tx_cmds_from_stack);
	seq_printf(m, "tx_dones_to_stack= %d\n",
		   wifi->stats.tx_dones_to_stack);
	seq_printf(m, "tx_noagg_not_addr= %d\n",
		   wifi->stats.tx_noagg_not_addr);
	seq_printf(m, "tx_noagg_not_ampdu= %d\n",
		   wifi->stats.tx_noagg_not_ampdu);
	seq_printf(m, "tx_noagg_not_qos= %d\n",
		   wifi->stats.tx_noagg_not_qos);
	seq_printf(m, "outstanding_cmd_cnt = %d (%d %d)\n",
		   wifi->stats.outstanding_cmd_cnt, skb_queue_len(&cmd_info.outstanding_cmd),
		   priv->stats->max_outstanding_cmd_queue_cnt);
	seq_printf(m, "gen_cmd_send_count = %d\n",
		   wifi->stats.gen_cmd_send_count);
	seq_printf(m, "umac_scan_req = %d\n",
		   wifi->stats.umac_scan_req);
	seq_printf(m, "umac_scan_complete = %d\n",
		   wifi->stats.umac_scan_complete);
	seq_printf(m, "hw_scan_status = %d\n",
		   wifi->params.hw_scan_status);
	seq_printf(m, "roc_in_progress = %d\n",
			priv->roc_params.roc_in_progress);
	seq_printf(m, "roc_starting = %d\n",
			priv->roc_params.roc_starting);
	seq_printf(m, "tx_cmd_send_count_single = %d\n",
		   wifi->stats.tx_cmd_send_count_single);
	seq_printf(m, "tx_cmd_send_count_multi = %d\n",
		   wifi->stats.tx_cmd_send_count_multi);
	seq_printf(m, "tx_cmd_send_count_beacon_q = %d\n",
		   wifi->stats.tx_cmd_send_count_beaconq);
	seq_printf(m, "tx_done_recv_count = %d\n",
		   wifi->stats.tx_done_recv_count);

	seq_printf(m, "tx_buff_pool_map = %x\n",
		   (unsigned int)priv->tx.buf_pool_bmp[0]);
	dump_tx_buff_info(m, &priv->tx);

	if (ftm)
		seq_printf(m, "pdout_val = %d (total samples: %d)\n",
			   total_samples ? (total_value/total_samples) : 0,
			   total_samples);
	if (wifi->params.production_test)
		seq_printf(m,
			   "rssi_average = %d dBm (total rssi samples: %d)\n",
			   total_rssi_samples ?
			   (total_rssi_value/total_rssi_samples) : 0,
			   total_rssi_samples);

	seq_puts(m, "************* LMAC STATS ***********\n");
	seq_printf(m, "roc_start =%d\n",
		   wifi->stats.roc_start);
	seq_printf(m, "roc_stop =%d\n",
		   wifi->stats.roc_stop);
	seq_printf(m, "roc_complete =%d\n",
		   wifi->stats.roc_complete);
	seq_printf(m, "roc_stop_complete =%d\n",
		   wifi->stats.roc_stop_complete);
	/* TX related */
	seq_printf(m, "tx_cmd_cnt =%d\n",
		   wifi->stats.tx_cmd_cnt);
	seq_printf(m, "tx_done_cnt =%d\n",
		   wifi->stats.tx_done_cnt);
	seq_printf(m, "tx_edca_trigger_cnt =%d\n",
		   wifi->stats.tx_edca_trigger_cnt);
	seq_printf(m, "tx_edca_isr_cnt =%d\n",
		   wifi->stats.tx_edca_isr_cnt);
	seq_printf(m, "tx_start_cnt =%d\n",
		   wifi->stats.tx_start_cnt);
	seq_printf(m, "tx_abort_cnt =%d\n",
		   wifi->stats.tx_abort_cnt);
	seq_printf(m, "tx_abort_isr_cnt =%d\n",
		   wifi->stats.tx_abort_isr_cnt);
	seq_printf(m, "tx_underrun_cnt =%d\n",
		   wifi->stats.tx_underrun_cnt);
	seq_printf(m, "tx_rts_cnt =%d\n",
		   wifi->stats.tx_rts_cnt);
	seq_printf(m, "tx_ampdu_cnt =%d\n",
		   wifi->stats.tx_ampdu_cnt);
	seq_printf(m, "tx_mpdu_cnt =%d\n",
		   wifi->stats.tx_mpdu_cnt);
	seq_printf(m, "tx_crypto_post =%d\n",
		   wifi->stats.tx_crypto_post);
	seq_printf(m, "tx_crypto_done =%d\n",
		   wifi->stats.tx_crypto_done);
	seq_printf(m, "rx_pkt_to_umac =%d\n",
		   wifi->stats.rx_pkt_to_umac);
	seq_printf(m, "rx_crypto_post =%d\n",
		   wifi->stats.rx_crypto_post);
	seq_printf(m, "rx_crypto_done =%d\n",
		   wifi->stats.rx_crypto_done);
	/* RX related */
	seq_printf(m, "rx_isr_cnt  =%d\n",
		   wifi->stats.rx_isr_cnt);
	seq_printf(m, "rx_ack_cts_to_cnt =%d\n",
		   wifi->stats.rx_ack_cts_to_cnt);
	seq_printf(m, "rx_cts_cnt =%d\n",
		   wifi->stats.rx_cts_cnt);
	seq_printf(m, "rx_ack_resp_cnt =%d\n",
		   wifi->stats.rx_ack_resp_cnt);
	seq_printf(m, "rx_ba_resp_cnt =%d\n",
		   wifi->stats.rx_ba_resp_cnt);
	seq_printf(m, "rx_fail_in_ba_bitmap_cnt =%d\n",
		   wifi->stats.rx_fail_in_ba_bitmap_cnt);
	seq_printf(m, "rx_circular_buffer_free_cnt =%d\n",
		   wifi->stats.rx_circular_buffer_free_cnt);
	seq_printf(m, "rx_mic_fail_cnt =%d\n",
		   wifi->stats.rx_mic_fail_cnt);

	/* HAL related */
	seq_printf(m, "hal_cmd_cnt  =%d\n",
		   wifi->stats.hal_cmd_cnt);
	seq_printf(m, "hal_event_cnt =%d\n",
		   wifi->stats.hal_event_cnt);
	seq_printf(m, "hal_ext_ptr_null_cnt =%d\n",
		   wifi->stats.hal_ext_ptr_null_cnt);
	seq_printf(m, "fw_error_counter = %d\n",
			hpriv->fw_error_counter);
	seq_printf(m, "fw_error_counter_scan = %d\n",
			hpriv->fw_error_counter_scan);
	seq_printf(m, "lpw_error_counter = %d\n",
			hpriv->lpw_error_counter);

	/* power save */
	seq_printf(m, "wifi power save (%s)\n",
			priv->power_save ? "AWAKE":"SLEEP");

	/* interface info */
	seq_printf(m, "current_vif_count = %d\n", priv->current_vif_count);
	seq_printf(m, "active_vifs = %d\n", priv->active_vifs);
	for (index = 0; index < MAX_VIFS; index++) {
		struct ieee80211_vif *vif = priv->vifs[index];
		struct umac_vif *uvif;

		if (!vif)
			break;
		uvif = (struct umac_vif *)&vif->drv_priv;
		if (!uvif)
			break;
		seq_printf(m, "\tvif_index %d\n", uvif->vif_index);
		seq_printf(m, "\ttype = %d\n", vif->type);
		seq_printf(m, "\taddr %pM\n", vif->addr);
		seq_printf(m, "\tbssid %pM\n", uvif->bssid);
		/*seq_printf(m, "atim_window = %d, aid = %d\n",
				uvif->config.atim_window, uvif->config.aid);*/
	}

#ifdef ENABLE_DAPT
	/*dapt info */
	seq_printf(m, "dapt info:\n");
	seq_printf(m, "main_index = %d\n", priv->dapt_params.main_index);
	seq_printf(m, "p2p_index = %d\n", priv->dapt_params.p2p_index);
	seq_printf(m, "conn_state[0] = %d, conn_state[1] = %d\n",
				priv->dapt_params.conn_state[0], priv->dapt_params.conn_state[1]);
	seq_printf(m, "iftype = %d\n", priv->iftype);
	seq_printf(m, "dapt_thresh_offset = %d\n", priv->params->dapt_thresh_offset);
	seq_printf(m, "dapt_thresh_exponent = %d\n", priv->params->dapt_thresh_exponent);
	seq_printf(m, "dapt_thresh_min = %d\n", priv->params->dapt_thresh_min);
	seq_printf(m, "dapt_thresh_max = %d\n", priv->params->dapt_thresh_max);
	for (index = 0; index < MAX_VIFS; index++) {
		seq_printf(m, "\tvif_addr = %pM, bssid = %pM, conn_state = %d\n",
					priv->dapt_params.vif_addr[index],
					priv->dapt_params.bssid[index],
					priv->dapt_params.conn_state[index]);
		seq_printf(m, "thresh_accum = %d\n", priv->dapt_params.thresh_accum[index]);
		seq_printf(m, "avg_thresh = %d\n", priv->dapt_params.avg_thresh[index]);
		seq_printf(m, "new_thresh = %d\n", priv->dapt_params.new_thresh[index]);
	}
	seq_printf(m, "cur_seted_thresh:\n\t");
	for (index = 0; index < 14; index++) {
		seq_printf(m, "%03d ", priv->dapt_params.cur_seted_thresh[index]);
	}

	seq_printf(m, "\nthreld history:\n");
	for (index = 0; index < 14; index++) {
		int s;

		if (ieee80211_frequency_to_channel(priv->cur_chan.center_freq1) == index + 1) {
			seq_printf(m, "\t ***channel %02d: offset %02d: ", index + 1, priv->dapt_params.cur_thr_offset[index]);
		} else {
			seq_printf(m, "\t channel %02d: offset %02d: ", index + 1, priv->dapt_params.cur_thr_offset[index]);
		}
		for (s = 0; s < DAPT_SETED_PHY_THRESH_COUNT; s++) {
			if (priv->dapt_params.cur_thr_offset[index] == s + 1) {
				seq_printf(m, "***");
			} else if (priv->dapt_params.cur_thr_offset[index] == 0) {
				if (s == DAPT_SETED_PHY_THRESH_COUNT - 1)
					seq_printf(m, "***");
			}
			seq_printf(m, "%03d ", priv->dapt_params.thr_history[index][s]);
		}
		seq_printf(m, "\n");
	}
#endif

	seq_printf(m, "rxq len = %d\n", skb_queue_len(&hpriv->rxq));
	seq_printf(m, "max_rxq len = %d\n", hpriv->max_rxq_len);
	seq_printf(m, "txq len = %d\n", skb_queue_len(&hpriv->txq));

	seq_printf(m, "cmd_reset_count = %d\n", priv->cmd_reset_count);

	seq_printf(m, "null_frame_send_count = %d\n", priv->null_frame_send_count);

	seq_printf(m, "tx_retry_frm_cnt: %d\n", priv->tx_retry_frm_cnt);
	return 0;

}

struct time_info {
	unsigned int count;
	unsigned int max_time;
	unsigned long long total_time;
};

struct vif_info {
	int if_ctrl;
	int if_idx;
	int if_mode;
	int if_conn_sta;
	unsigned char if_addr[6];
	unsigned char bssid[6];
	int key_ctrl;
	int key_type;
};

#define MAX_IF 2
struct if_info {
	int num;
	struct vif_info vif_info[MAX_IF];
};

struct filter_pkt_info {
	unsigned int total_pkt;	
	unsigned int probe_req_pkt;
	unsigned int bcast_pkt;
	unsigned int mcast_pkt;	
};

struct tx_rx_count_info {
	unsigned int cmd_tx_send;
	unsigned int cmd_send;
	unsigned int event_recv;
	unsigned int event_rx_recv;
	unsigned int event_rx_pkt_recv;
	unsigned int event_rx_pkt_crc_ok;
	unsigned int event_rx_pkt_crc_err;
	unsigned int event_tx_done_recv;
	unsigned int event_rx_serias;
	unsigned short err_desc_id_lmac;
	unsigned short err_desc_id_host;
	unsigned short lpw_hang;
	unsigned short lpw_hang_cnt;
	unsigned short cmd_cnt_dur_lpw_hang;
	unsigned short cmd_txcnt_dur_lpw_hang;
	unsigned short cmd_rxcnt_dur_lpw_hang;
	struct time_info wifi_isr_info;
	struct time_info sdio_isr_info[4];
	struct time_info cmd_send_info;
	unsigned int cmd_id;
	unsigned long long total_tick;

	struct time_info rx_notify;
	struct time_info rx_begin;
	struct time_info rx_end;
	struct time_info rx_interval;
	int lpw_rx_q_min;
	short wifi_int_disabled;

	struct time_info tx_done;
	struct time_info scan_hang;

	struct filter_pkt_info filter_info;
};

static void dump_time_info(struct seq_file *m,
					struct time_info *info, char *str)
{
	unsigned long long value;

	/* total_time from fw unit is 25ns, so need to div 40 (convert to us) */
	value = info->total_time>>2;
	if (info->count != 0)
		do_div(value, info->count);
	seq_printf(m, "%s:\n"
				"\ttotal_time = %lld\b us\n"
				"\tmax_time = %d us\n"
				"\tcount = %d\n"
				"\tavg = %d us\n",
				str,
				info->total_time>>2,
				info->max_time/40,
				info->count,
				(unsigned int)value/10);
}

static void dump_if_info(struct seq_file *m,
					struct if_info *info)
{
	int i;

	for (i = 0; i < MAX_IF; i++) {
		seq_printf(m, "if_idx %d:\n", info->vif_info[i].if_idx);
		seq_printf(m, "\tif_ctrl=%s\n", info->vif_info[i].if_ctrl==IF_ADD ? "ADD":"DEL");
		switch (info->vif_info[i].if_mode) {
		case IF_MODE_STA_BSS:
			seq_printf(m, "\tif_mode=IF_MODE_STA_BSS\n");
			break;
		case IF_MODE_STA_IBSS:
			seq_printf(m, "\tif_mode=IF_MODE_STA_IBSS\n");
			break;
		case IF_MODE_AP:
			seq_printf(m, "\tif_mode=IF_MODE_AP\n");
			break;
		default:
			seq_printf(m, "\tif_mode=UNKNOW\n");
			break;
		}
		seq_printf(m, "\tif_conn_sta=%s\n", info->vif_info[i].if_conn_sta==STA_CONN ? "STA_CONN":"STA_DISCONN");
		seq_printf(m, "\tif_addr=%pM\n", info->vif_info[i].if_addr);
		seq_printf(m, "\tbssid=%pM\n", info->vif_info[i].bssid);
		seq_printf(m, "\tkey_ctrl=%s\n", info->vif_info[i].key_ctrl==KEY_CTRL_ADD ? "ADD":"DEL");
		seq_printf(m, "\tkey_type=%d\n", info->vif_info[i].key_type);
	}
}

static void* fw_log_seq_start(struct seq_file *s, loff_t *pos)
{
	static int read_finish = 0;
	struct img_priv *priv;
	struct fw_info_dump *fw_info = &wifi->fw_info;

	pr_debug("%s: %d\n", __func__, read_finish);

	if (read_finish == 1) {
		read_finish = 0;
		return NULL;// no more data to read, exit
	}

	if (!wifi->hw)
		return NULL;

	priv = (struct img_priv *)(wifi->hw->priv);
	if (priv->state != STARTED) {
		pr_err("Interface is not initialized\n");
		return NULL;
	}

	if (rpu_fw_priv_cmd_sync(DUMP_FW_LOG, NULL) != 0) {
		pr_err("%s: send cmd failed\n", __func__);
		return NULL;
	}
	pr_debug("%s: read log %d\n", __func__, fw_info->offset);

	fw_info->finish = 0;

#define PRIV_CMD_DONE_EVENT_HEADER_SIZE (sizeof(struct host_rpu_msg_hdr) + sizeof(struct dump_info) - 1)

	read_finish = (fw_info->offset<(128-PRIV_CMD_DONE_EVENT_HEADER_SIZE))?1:0;

	return fw_info->info;
}

static void* fw_log_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void fw_log_seq_stop(struct seq_file *s, void *v)
{
}

static int fw_log_seq_show(struct seq_file *s, void *v)
{
	seq_printf(s, "%s", (char*)v);
	return 0;
}

static struct seq_operations fw_log_seq_ops = {
	.start = fw_log_seq_start,
	.next  = fw_log_seq_next,
	.stop  = fw_log_seq_stop,
	.show  = fw_log_seq_show
};

static void dump_txrx_count_info(struct seq_file *m, struct fw_info_dump *fw_info)
{
	unsigned long long result = 0, delta1, delta2;
	struct tx_rx_count_info *info = (struct tx_rx_count_info*)fw_info->info;
	
	seq_printf(m, "cmd_tx_send = %d\n"
				 "cmd_send = %d\n"
				 "event_recv = %d\n"
				 "event_rx_recv = %d\n"
				 "event_rx_pkt_recv = %d\n"
				 "event_rx_pkt_crc_ok = %d\n"
				 "event_rx_pkt_crc_err = %d\n"
				 "event_tx_done_recv = %d\n"
				 "event_rx_serias = %d\n"
				 "err_desc_id_lmac = %d\n"
				 "err_desc_id_host = %d\n"
				 "lpw_hang = %d\n"
				 "lpw_hang_cnt = %d\n"
				 "cmd_cnt_dur_lpw_hang = %d\n"
				 "cmd_txcnt_dur_lpw_hang = %d\n"
				 "cmd_rxcnt_dur_lpw_hang = %d\n",
				 info->cmd_tx_send,
				 info->cmd_send,
				 info->event_recv,
				 info->event_rx_recv,
				 info->event_rx_pkt_recv,
				 info->event_rx_pkt_crc_ok,
				 info->event_rx_pkt_crc_err,
				 info->event_tx_done_recv,
				 info->event_rx_serias,
				 info->err_desc_id_lmac,
				 info->err_desc_id_host,
				 info->lpw_hang,
				 info->lpw_hang_cnt,
				 info->cmd_cnt_dur_lpw_hang,
				 info->cmd_txcnt_dur_lpw_hang,
				 info->cmd_rxcnt_dur_lpw_hang);
	
	dump_time_info(m, &info->wifi_isr_info, "wifi isr info");
	result += info->wifi_isr_info.total_time;
	
	dump_time_info(m, &info->sdio_isr_info[0], "sdio isr (COMP)");
	result += info->sdio_isr_info[0].total_time;
	dump_time_info(m, &info->sdio_isr_info[1], "sdio isr (DMA)");
	result += info->sdio_isr_info[1].total_time;
	dump_time_info(m, &info->sdio_isr_info[2], "sdio isr (WR)");
	result += info->sdio_isr_info[2].total_time;
	dump_time_info(m, &info->sdio_isr_info[3], "sdio isr (RD)");
	result += info->sdio_isr_info[3].total_time;
	
	delta1 = result - fw_info->last_total_isr_tick;
	delta2 = info->total_tick - fw_info->last_total_tick;
	do_div(delta1, 40*1000);
	do_div(delta2, 40*1000);
	seq_printf(m, "isr/total tick (%lld/%lld ms)\n",
			delta1, delta2);
	fw_info->last_total_isr_tick = result;
	fw_info->last_total_tick = info->total_tick;
	
	dump_time_info(m, &info->cmd_send_info, "cmd send info");
	seq_printf(m, "cmd id: %d\n", info->cmd_id);
	
	dump_time_info(m, &info->rx_notify, "rx_notify");
	dump_time_info(m, &info->rx_begin, "rx_begin");
	dump_time_info(m, &info->rx_end, "rx_end");
	dump_time_info(m, &info->rx_interval, "rx_interval");
	
	seq_printf(m, "min lpw q: %d\n", info->lpw_rx_q_min);
	
	dump_time_info(m, &info->tx_done, "tx_done");
	
	dump_time_info(m, &info->scan_hang, "scan_hang");
	
	seq_printf(m, "wifi_int_disabled: %d\n", info->wifi_int_disabled);

	seq_printf(m, "dump filter info: \n");
	seq_printf(m, "\ttotal_pkt: %d\n", info->filter_info.total_pkt);
	seq_printf(m, "\tprobe_req_pkt: %d\n", info->filter_info.probe_req_pkt);
	seq_printf(m, "\tbcast_pkt: %d\n", info->filter_info.bcast_pkt);
	seq_printf(m, "\tmcast_pkt: %d\n", info->filter_info.mcast_pkt);
}

static void* fw_info_seq_start(struct seq_file *s, loff_t *pos)
{
	static int read_finish = 0;
	struct img_priv *priv;
	struct fw_info_dump *fw_info = &wifi->fw_info;

	pr_debug("%s: %d\n", __func__, read_finish);

	if (read_finish == 1) {
		read_finish = 0;
		return NULL;// no more data to read, exit
	}

	if (!wifi->hw)
		return NULL;

	priv = (struct img_priv *)(wifi->hw->priv);
	if (priv->state != STARTED) {
		pr_err("Interface is not initialized\n");
		return NULL;
	}

	if (fw_info->type == ADC_CAPTURE || fw_info->type == DUMP_ADC_CAPTURE_DATA) {
		if (rpu_fw_priv_cmd_sync(DUMP_ADC_CAPTURE_DATA, NULL) != 0) {
			pr_err("%s: send cmd failed\n", __func__);
			return NULL;
		}
#define PRIV_CMD_DONE_EVENT_HEADER_SIZE (sizeof(struct host_rpu_msg_hdr) + sizeof(struct dump_info) - 1)
		read_finish = (fw_info->offset<(128-PRIV_CMD_DONE_EVENT_HEADER_SIZE))?1:0;
	} else {
		if (!fw_info->finish)
			return NULL;
		read_finish = 1;
	}
	pr_debug("%s: read %d\n", __func__, fw_info->offset);

	fw_info->finish = 0;


	return (void*)fw_info;
}

static void dump_bytes(struct seq_file *s, struct fw_info_dump *info, int word)
{
	int i, j, line;
	u8 *buf_byte = (u8 *)info->info;
	u32 *buf_word = (u32 *)info->info;

	if (word)
		line = 4;
	else
		line = 16;

	for (i = 0; i < round_up(info->offset, 16)/16; i++) {
		for (j = 0; j < line; j++) {
			if (word)
				seq_printf(s, "%08x ", *buf_word++);
			else
				seq_printf(s, "%02x ", *buf_byte++);
		}
		seq_printf(s, "\n");
	}
	seq_printf(s, "\n");
}

#define RF_CAL_DATA_DIR		"/data"
static void save_rf_cal_data(struct fw_info_dump *info)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0))
	struct file *fp;
	loff_t pos;
    mm_segment_t fs;
	u8 path[64];

	sprintf(path, "%s/%s", RF_CAL_DATA_DIR, RF_CAL_DATA_FILE);
	fp = filp_open(path, O_RDWR | O_CREAT, 0644);
	if (fp == NULL || IS_ERR(fp)) {
		pr_err("%s: create %s failed\n", __func__, path);
		return;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, info->info, info->offset, &pos);

	filp_close(fp, NULL);
#endif
}

static int fw_info_seq_show(struct seq_file *s, void *v)
{
	struct fw_info_dump *fw_info = (struct fw_info_dump*)v;

	if (fw_info->type == DUMP_REG_INFO) {
		int i;
		unsigned int *buf = (unsigned int *)fw_info->info;

		for (i = 0; i < fw_info->offset/4; i++) {
			if ((i%4) == 0) {
				seq_printf(s, "\n%08x: ", fw_info->reg);
			}
			seq_printf(s, "%08x ", *buf++);
			fw_info->reg += 4;
		}
		seq_printf(s, "\n");
	} else if (fw_info->type == DUMP_TXRX_COUNT_INFO) {
		dump_txrx_count_info(s, fw_info);
	} else if (fw_info->type == DUMP_IF_INFO) {
		dump_if_info(s, (struct if_info *)fw_info->info);
	} else if (fw_info->type == DUMP_ADC_CAPTURE_DATA) {
		unsigned int len = fw_info->offset;
		unsigned int *pt = (unsigned int *)fw_info->info;
		while (len > 0) {
			seq_printf(s, "%08x\n", *pt);
			len -= 4;
			pt++;
		}
	} else if (fw_info->type == DUMP_RF_CAL_DATA) {
		dump_bytes(s, fw_info, 1);
		save_rf_cal_data(fw_info);
	} else {
		seq_printf(s, "%s", (char*)fw_info->info);
	}

	return 0;
}
static void* fw_info_seq_next(struct seq_file *s, void *v, loff_t *pos){return NULL;}
static void fw_info_seq_stop(struct seq_file *s, void *v){}

static struct seq_operations fw_info_seq_ops = {
	.start = fw_info_seq_start,
	.next  = fw_info_seq_next,
	.stop  = fw_info_seq_stop,
	.show  = fw_info_seq_show
};

void rpu_reinit(void)
{

	if (wifi->hw)
		rpu_exit();
	rpu_init();
	uccp_reinit = 1;
}

static int hex2num(char c)
{
        if (c >= '0' && c <= '9')
                return c - '0';
        if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
        return -1;
}

/**
 * hwaddr_aton - Convert ASCII string to MAC address (colon-delimited format)
 * @txt: MAC address as a string (e.g., "00:11:22:33:44:55")
 * @addr: Buffer for the MAC address (ETH_ALEN = 6 bytes)
 * Returns: 0 on success, -1 on failure (e.g., string not a MAC address)
 */
static int hwaddr_aton(const char *txt, unsigned char *addr)
{
        int i;

        for (i = 0; i < 6; i++) {
                int a, b;

                a = hex2num(*txt++);
                if (a < 0)
                        return -1;
                b = hex2num(*txt++);
                if (b < 0)
                        return -1;
                *addr++ = (a << 4) | b;
                if (i < 5 && *txt++ != ':')
                        return -1;
        }

        return 0;
}

static ssize_t proc_write_config(struct file *file,
				 const char __user *buffer,
				 size_t count,
				 loff_t *ppos)
{
	char buf[(RF_PARAMS_SIZE * 2) + 50];
	unsigned long val = 0, val2 = 0;
	long sval;
	struct img_priv *priv;
	int ret = 0;
	static unsigned char bssid[6], mac_addr[6];

	if (!wifi->hw)
		return -ENODEV;

	priv = (struct img_priv *)(wifi->hw->priv);

	if (count >= sizeof(buf))
		count = sizeof(buf) - 1;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';

	pr_info("%s: %s\n", __func__, buf);
	if (0) {
#ifdef HAL_PCIE
	} else if(param_get_val(buf,"pci_mem_addr=",&val)) {
		wifi->params.pci_base_addr=val;
	} else if(param_get_val(buf,"no_words=",&val)){
		wifi->params.no_words=val;
		read_mem_region(wifi->params.pci_base_addr,
				wifi->params.no_words);
#endif
	} else if (param_get_val(buf, "production_test=", &val)) {
		if ((val == 0) || (val == 1)) {
			if (wifi->params.production_test != val) {
				if (wifi->params.production_test)
					wifi->params.num_vifs = 1;

				wifi->params.production_test = val;

				//rpu_reinit();
				//pr_err("Re-initializing UMAC ..\n");
			}
		} else
			pr_err("Invalid parameter value\n");
	} else if (param_get_val(buf, "bypass_vpd=", &val)) {
		if ((val == 0) || (val == 1)) {
			if (wifi->params.bypass_vpd != val)
				wifi->params.bypass_vpd = val;
		} else
			pr_err("Invalid parameter value\n");
	} else if (param_get_val(buf, "num_vifs=", &val)) {
		if (val > 0 && val <= MAX_VIFS) {
			if (wifi->params.num_vifs != val) {
				rpu_reinit();
				pr_err("Re-initializing UMAC ..\n");
				wifi->params.num_vifs = val;
			}
		}
	} else if (param_get_match(buf, "rf_params=")) {
		conv_str_to_byte(wifi->params.rf_params,
				strstr(buf, "=") + 1,
				RF_PARAMS_SIZE);
	} else if (param_get_val(buf, "rx_packet_mgmt_count=", &val)) {
		wifi->stats.rx_packet_mgmt_count = val;
	} else if (param_get_val(buf, "rx_packet_data_count=", &val)) {
		wifi->stats.rx_packet_data_count = val;
	} else if (param_get_val(buf, "pdout_val=", &val)) {
		wifi->stats.pdout_val = val;
	} else if (param_get_val(buf, "get_stats=", &val)) {
		if (priv->state != STARTED) {
			pr_err("Interface is not initialized\n");
			goto error;
		}
		CALL_RPU(rpu_prog_mib_stats);
	} else if (param_get_val2(buf, "read_fw_reg=", &val, &val2)) {
		struct fw_reg_info reg;

		reg.reg = val;
		reg.len = val2==0 ? 4:val2;
		reg.rw = READ_REG;
		pr_info("read_fw_reg=0x%x,0x%x\n", reg.reg, reg.len);
		CALL_RPU(rpu_fw_priv_cmd, DUMP_REG_INFO, (void *)&reg);
	} else if (param_get_val2(buf, "read_lpw_reg=", &val, &val2)) {
		struct fw_reg_info reg;

		reg.reg = val;
		reg.len = val2==0 ? 4:val2;
		reg.rw = READ_REG_LPW;
		pr_info("read_lpw_reg=0x%x,0x%x\n", reg.reg, reg.len);
		CALL_RPU(rpu_fw_priv_cmd, DUMP_REG_INFO, (void *)&reg);		
	} else if (param_get_val2(buf, "write_fw_reg=", &val, &val2)) {
		struct fw_reg_info reg;
	
		reg.reg = val;
		reg.val = val2;
		reg.rw = WRITE_REG;
		pr_info("write_fw_reg=0x%x,0x%x\n", reg.reg, reg.val);
		CALL_RPU(rpu_fw_priv_cmd, DUMP_REG_INFO, &reg);
	} else if (param_get_val2(buf, "write_lpw_reg=", &val, &val2)) {
		struct fw_reg_info reg;
	
		reg.reg = val;
		reg.val = val2;
		reg.rw = WRITE_REG_LPW;
		pr_info("write_fw_reg=0x%x,0x%x\n", reg.reg, reg.val);
		CALL_RPU(rpu_fw_priv_cmd, DUMP_REG_INFO, &reg);
	} else if (param_get_val(buf, "set_channel=", &val)) {
		unsigned int pri_chnl_num = val;
		unsigned int freq_band = IEEE80211_BAND_2GHZ;
		int center_freq = ieee80211_channel_to_frequency(pri_chnl_num,
					       freq_band);
		pr_info("set channel: %d %d %d\n", pri_chnl_num, center_freq, freq_band);
		CALL_RPU(rpu_prog_channel, pri_chnl_num, center_freq, 0, 0, freq_band);
		pr_info("set channel success.\n");
	} else if (strstr(buf, "fw_txrx_count_info")) {
		if (priv->state != STARTED) {
			pr_err("Interface is not initialized\n");
			goto error;
		}
		CALL_RPU(rpu_fw_priv_cmd, DUMP_TXRX_COUNT_INFO, NULL);
	} else if (strstr(buf, "adc_capture")) {
		if (priv->state != STARTED) {
			pr_err("Interface is not initialized\n");
			goto error;
		}
		CALL_RPU(rpu_fw_priv_cmd, ADC_CAPTURE, NULL);
	} else if (strstr(buf, "fw_txrx_queue_info")) {
		if (priv->state != STARTED) {
			pr_err("Interface is not initialized\n");
			goto error;
		}
		CALL_RPU(rpu_fw_priv_cmd, DUMP_TXRX_QUEUE_INFO, NULL);
	} else if (strstr(buf, "fw_version_info")) {
		if (priv->state != STARTED) {
			pr_err("Interface is not initialized\n");
			goto error;
		}
		CALL_RPU(rpu_fw_priv_cmd, DUMP_FW_VERSION, NULL);
	} else if (strstr(buf, "fw_enable_ejtag")) {
		/*if (priv->state != STARTED) {
			pr_err("Interface is not initialized\n");
			goto error;
		}*/
		rk915_ejtag(hpriv);
		CALL_RPU(rpu_fw_priv_cmd, ENABLE_EJTAG, NULL);
	} else if (strstr(buf, "fw_if_info")) {
		if (priv->state != STARTED) {
			pr_err("Interface is not initialized\n");
			goto error;
		}
		CALL_RPU(rpu_fw_priv_cmd, DUMP_IF_INFO, NULL);
	} else if (strstr(buf, "rf_cal_data")) {
		CALL_RPU(rpu_fw_priv_cmd, DUMP_RF_CAL_DATA, NULL);
	} else if (param_get_val(buf, "sniffer=", &val)) {
		/*
		 * Value = 0 --> normal operation 
		 * Value > 0 --> sniffer operation 
		 * Value = 1 --> receive all data and managment frames - both unicast and broadcast 
		 * Value = 2 --> receive broadcast data frames only 
		 * Value = 3 --> receive broadcast and unciast data only 
		 */
		if (priv->state != STARTED) {
			pr_err("Interface is not initialized\n");
			goto error;
		}
		priv->sniffer = val;
		CALL_RPU(rpu_fw_priv_cmd, ENABLE_SNIFFER, (void *)&val);
		CALL_RPU(rpu_prog_cfgmisc, priv->sniffer);
	} else if (param_get_val(buf, "max_data_size=", &val)) {
		if (wifi->params.max_data_size != val) {
			if ((wifi->params.max_data_size >= 2 * 1024) &&
			    (wifi->params.max_data_size <= (12 * 1024))) {
				wifi->params.max_data_size = val;

				rpu_reinit();
				pr_err("Re-initalizing RPU with %ld as max data size\n",
				       val);

			} else
				pr_err("Invalid Value for max data size: should be (2K-12K)\n");
		}
	} else if (param_get_val(buf, "max_tx_cmds=", &val)) {
		int max_tx_cmd_limit = MAX_SUBFRAMES_IN_AMPDU_HT;

		if (val >= 1 && val <= max_tx_cmd_limit)
			wifi->params.max_tx_cmds = val;
		else
			pr_err("Please enter value between 1 and %d\n",
			       max_tx_cmd_limit);
	} else if (param_get_val(buf, "disable_power_save=", &val)) {
		if ((val == 0) || (val == 1)) {
			if (val != wifi->params.disable_power_save) {
				wifi->params.disable_power_save = val;

				rpu_reinit();
				pr_err("Re-initalizing RPU with global powerave %s\n",
				       val ? "DISABLED" : "ENABLED");
			}
		}
	} else if (param_get_val(buf, "disable_sm_power_save=", &val)) {
		if ((val == 0) || (val == 1)) {
			if (val != wifi->params.disable_sm_power_save) {
				wifi->params.disable_sm_power_save = val;

				rpu_reinit();
				pr_err("Re-initalizing RPU with smps %s\n",
				       val ? "DISABLED" : "ENABLED");

			}
		}
	} else if (param_get_val(buf, "uccp_num_spatial_streams=", &val)) {
		if (val > 0 && val <= min(MAX_TX_STREAMS, MAX_RX_STREAMS)) {
			if (val != wifi->params.uccp_num_spatial_streams) {
				wifi->params.uccp_num_spatial_streams = val;
				wifi->params.num_spatial_streams = val;
				wifi->params.max_tx_streams = val;
				wifi->params.max_rx_streams = val;
				rpu_reinit();
				pr_err("Re-initalizing RPU with %ld spatial streams\n",
				       val);
			}
		} else
			pr_err("Invalid parameter value: Allowed Range: 1 to %d\n",
			       min(MAX_TX_STREAMS, MAX_RX_STREAMS));
	} else if (param_get_val(buf, "enable_early_agg_checks=", &val)) {
		if ((val == 0) || (val == 1)) {
			if (val != wifi->params.enable_early_agg_checks)
				wifi->params.enable_early_agg_checks = val;
		} else
			pr_err("Invalid parameter value: Allowed: 0/1\n");
	} else if (param_get_val(buf, "antenna_sel=", &val)) {
		if (val == 1 || val == 2) {
			if (val != wifi->params.antenna_sel) {
				wifi->params.antenna_sel = val;
				rpu_reinit();
				pr_err("Re-initalizing RPU with %ld antenna selection\n",
				       val);
			}
		} else
			pr_err("Invalid parameter value: Allowed Values: 1 or 2\n");
	} else if (param_get_val(buf, "num_spatial_streams=", &val)) {
		if (val > 0 && val <= wifi->params.uccp_num_spatial_streams)
			wifi->params.num_spatial_streams = val;
		else
			pr_err("Invalid parameter value, should be less than or equal to uccp_num_spatial_streams\n");
	} else if (param_get_sval(buf, "mgd_mode_tx_fixed_mcs_indx=", &sval)) {
		if (wifi->params.mgd_mode_tx_fixed_rate == -1) {
			if (check_valid_data_rate(priv, sval | 0x80, UCAST))
				wifi->params.mgd_mode_tx_fixed_mcs_indx = sval;
		} else
			pr_err("Fixed rate other than MCS index is currently set\n");
	} else if (param_get_sval(buf, "mgd_mode_tx_fixed_rate=", &sval)) {
		if (wifi->params.mgd_mode_tx_fixed_mcs_indx == -1) {
			if (check_valid_data_rate(priv, sval, UCAST))
				wifi->params.mgd_mode_tx_fixed_rate = sval;
		} else
			pr_err("MCS data rate(index) is currently set\n");
	/* Multicast Rate configuration options.
	 */
	} else if (param_get_sval(buf, "mgd_mode_mcast_fixed_data_rate=",
		   &sval)) {
		if (check_valid_data_rate(priv, sval, MCAST))
			wifi->params.mgd_mode_mcast_fixed_data_rate = sval;
	} else if (param_get_val(buf, "mgd_mode_mcast_fixed_rate_flags=",
		   &val)) {
		if (check_valid_rate_flags(priv, val))
			wifi->params.mgd_mode_mcast_fixed_rate_flags = val;
	} else if (param_get_val(buf, "mgd_mode_mcast_fixed_bcc_or_ldpc=",
		   &val)) {
		wifi->params.mgd_mode_mcast_fixed_bcc_or_ldpc = val;
	} else if (param_get_val(buf, "mgd_mode_mcast_fixed_stbc_enabled=",
		   &val)) {
		wifi->params.mgd_mode_mcast_fixed_stbc_enabled = val;
	} else if (param_get_val(buf, "mgd_mode_mcast_fixed_preamble=",
		   &val)) {
		wifi->params.mgd_mode_mcast_fixed_preamble = val;
	} else if (param_get_val(buf, "mgd_mode_mcast_fixed_nss=", &val)) {
		wifi->params.mgd_mode_mcast_fixed_nss = val;

	/* Production mode rate configuration
	 */
	} else if (param_get_sval(buf, "tx_fixed_mcs_indx=", &sval)) {
		if (wifi->params.production_test != 1) {
			pr_err("Only can be set in production mode.\n");
			goto error;
		}

		if (sval == -1) {
			wifi->params.tx_fixed_mcs_indx = -1;
			goto error;
		}

		if (wifi->params.tx_fixed_rate != -1) {
			pr_err("Fixed rate other than MCS index is currently set\n");
			goto error;
		}

		if (check_valid_data_rate(priv, sval | 0x80, UCAST))
			wifi->params.tx_fixed_mcs_indx = sval;


	} else if (param_get_sval(buf, "tx_fixed_rate=", &sval)) {
		if (wifi->params.production_test != 1) {
			pr_err("Only can be set in production mode.\n");
			goto error;
		}

		if (sval == -1) {
			wifi->params.tx_fixed_rate = -1;
			goto error;
		}
		if (wifi->params.tx_fixed_mcs_indx != -1) {
			pr_err("MCS Index is currently set.\n");
			goto error;
		}

		if (check_valid_data_rate(priv, sval, UCAST))
			wifi->params.tx_fixed_rate = sval;

	} else if (param_get_val(buf, "chnl_bw=", &val)) {
		if (((val == 0) ||
		     (val == 1))) {
			wifi->params.chnl_bw = val;

			rpu_reinit();
			pr_err("Re-initializing UMAC ..\n");
		} else
			pr_err("Invalid parameter value.\n");
	} else if (param_get_val(buf, "prod_mode_chnl_bw_40_mhz=", &val)) {

		do {
			if (wifi->params.production_test != 1) {
				pr_err("Can be set in only in production mode.\n");
				break;
			}

			if (!((val == 0) || (val == 1))) {
				pr_err("Invalid parameter value.\n");
				break;
			}

			wifi->params.prod_mode_chnl_bw_40_mhz = val;


		} while (0);
	} else if (param_get_val(buf, "prod_mode_rate_flag=", &val)) {
		if (check_valid_rate_flags(priv, val))
			wifi->params.prod_mode_rate_flag = val;
	} else if (param_get_val(buf, "rate_protection_type=", &val)) {
		/* 0 is None, 1 is RTS/CTS, 2 is for CTS2SELF */
		if ((val == 0) || (val == 1) /*|| (val == 2)*/)
			wifi->params.rate_protection_type = val;
		else
			pr_err("Invalid parameter value");
	} else if (param_get_val(buf, "prod_mode_rate_preamble_type=", &val)) {
		/*0 is short, 1 is Long*/
		if ((val == 0) || (val == 1))
			wifi->params.prod_mode_rate_preamble_type = val;
		else
			pr_err("Invalid parameter value");
	} else if (param_get_val(buf, "prod_mode_stbc_enabled=", &val)) {
		if (val <= 1)
			wifi->params.prod_mode_stbc_enabled = val;
		else
			pr_err("Invalid parameter value\n");
	} else if (param_get_val(buf, "prod_mode_bcc_or_ldpc=", &val)) {
		if (val <= 1)
			wifi->params.prod_mode_bcc_or_ldpc = val;
		else
			pr_err("Invalid parameter value\n");
	} else if (param_get_val(buf, "reset_hal_params=", &val)) {
		if (priv->state != STARTED) {
			if (val != 1)
				pr_err("Invalid parameter value\n");
			else
				hal_ops.reset_hal_params();
		} else
			pr_err("HAL parameters reset can be done only when all interface are down\n");
	} else if (param_get_val(buf, "scan_type=", &val)) {
		if ((val == 0) || (val == 1))
			wifi->params.scan_type = val;
		else
			pr_err("Invalid scan type value %d, should be 0 or 1\n",
			       (unsigned int)val);
	} else if (param_get_val(buf, "start_prod_mode=", &val)) {
		start_prod_mode(priv, val);
	} else if (param_get_sval(buf, "stop_prod_mode=", &sval)) {
		stop_prod_mode(priv, sval);
	} else if (strstr(buf, "set_rx_bssid=")) {
		hwaddr_aton(strstr(buf, "=") + 1, bssid);
		pr_info("bssid = %pM\n", bssid);
	} else if (strstr(buf, "set_rx_mac_addr=")) {
		hwaddr_aton(strstr(buf, "=") + 1, mac_addr);
		pr_info("mac_addr = %pM\n", mac_addr);
	} else if (param_get_val(buf, "fw_skip_rx_pkt_submit=", &val)) {
		wifi->params.fw_skip_rx_pkt_submit = val;
	} else if (param_get_val(buf, "start_prod_rx_mode=", &val)) {
		start_prod_rx_mode(priv, val, bssid, mac_addr);
	} else if (param_get_val(buf, "start_prod_cw_mode=", &val)) {
		start_prod_rx_mode(priv, val|0x80, bssid, mac_addr);
	} else if (param_get_val(buf, "start_prod_echo_mode=", &val)) {
		start_prod_echo_mode(priv, val);
	} else if (param_get_sval(buf, "start_packet_gen=", &sval)) {
		start_packet_gen(priv, sval);
	} else if (param_get_sval(buf, "stop_packet_gen=", &sval)) {
		stop_packet_gen(priv, sval);
	} else if (param_get_val(buf, "payload_length=", &val)) {
#ifdef PKTGEN_MULTI_TX
		if (val > 2317)
		    val = 2317;
		pr_info("payload_length = %d\n", (int)val);
#endif
		wifi->params.payload_length = val;
	} else if (param_get_sval(buf, "set_tx_power=", &sval)) {
		if (wifi->params.production_test != 1 && !ftm) {
			pr_err("set_tx_power: Can be set in only in FTM/production mode.\n");
			goto error;
		}

		if (!wifi->params.init_prod) {
			pr_err("NEW Production Mode is not Initialized\n");
			goto error;
		}

		memset(wifi->params.pdout_voltage, 0,
		       sizeof(char) * MAX_AUX_ADC_SAMPLES);
		wifi->params.set_tx_power = sval;
		CALL_RPU(rpu_prog_txpower, sval);
 	} else if (param_get_val(buf, "fw_loaded=", &val)) {
 		wifi->params.fw_loaded = val;
	} else if (param_get_val(buf, "disable_beacon_ibss=", &val)) {
		if ((val == 1) || (val == 0))
			wifi->params.disable_beacon_ibss = val;
		else
			pr_err("Invalid driver_tput value should be 1 or 0\n");
	} else if (param_get_val(buf, "rpu_debug=", &val)) {
		rpu_debug = val;
	} else if (param_get_val(buf, "rpu_debug_level=", &val)) {
		rpu_debug_level = val;
#ifdef RPU_SLEEP_ENABLE
	} else if (param_get_val(buf, "sleep=", &val)) {
			hal_ops.trigger_timed_sleep(val);
	} else if (param_get_val(buf, "wakeup=", &val)) {
			hal_ops.trigger_wakeup(val);
	} else if (param_get_val(buf, "ps=", &val)) {
		trigger_wifi_power_save(val);
	} else if (param_get_val(buf, "scan_abort=", &val)) {
		trigger_wifi_scan_abort(val);
	} else if (param_get_val(buf, "rpu_sleep_type=", &val)) {
		wifi->params.rpu_sleep_type = val;
#endif
#ifdef ENABLE_DAPT
	} else if (param_get_val(buf, "dapt_thresh_offset=", &val)) {
		wifi->params.dapt_thresh_offset = val;
	} else if (param_get_val(buf, "dapt_thresh_exponent=", &val)) {
		wifi->params.dapt_thresh_exponent = val;
	} else if (param_get_val(buf, "dapt_thresh_min=", &val)) {
		wifi->params.dapt_thresh_min = val;
	} else if (param_get_val(buf, "dapt_thresh_max=", &val)) {
		wifi->params.dapt_thresh_max = val;
	} else if (param_get_val(buf, "dapt_disable=", &val)) {
		dapt_disable(priv, val);
#endif		
	} else if (param_get_val(buf, "dapt_set_phy_thresh=", &val)) {
		int c;
		unsigned int thresh[14]; 

		//dapt_set_phy_thresh(priv, val);
		for (c = 0; c < 14; c++)
			thresh[c] = val;
		rpu_prog_phy_thresh(thresh);
	} else if (param_get_val(buf, "prog_channel=", &val)) {
		int freq = ieee80211_channel_to_frequency(val, IEEE80211_BAND_2GHZ);

		rpu_prog_channel(val, freq, 0, 1, 0);
	} else if (param_get_val(buf, "dtim=", &val)) {
		wifi->params.min_dtim_peroid = val;
	} else if (param_get_val2(buf, "mem_read=", &val, &val2)) {
		pr_info("mem_read=0x%x,0x%x\n", (unsigned int)val, (unsigned int)val2);
		rk915_mem_check2(hpriv, val, val2);
	} else if (param_get_val(buf, "disconnect=", &val)) {
		struct ieee80211_vif *vif = NULL;
		int i = 0;

		if (val == 1) {
			for (i = 0; i < MAX_VIFS; i++) {
				if (!(priv->active_vifs & (1 << i)))
					continue;

				vif = rcu_dereference(priv->vifs[i]);

				if (ether_addr_equal(vif->addr,
						     vif_macs[0])) {
					ieee80211_connection_loss(vif);
					break;
				}
			}
		}
	} else
		pr_err("Invalid parameter name: %s\n", buf);
error:
	return count;
prog_rpu_fail:
	return ret;
}


static int proc_open_config(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read_config, NULL);
}


static int proc_open_phy_stats(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read_phy_stats, NULL);
}

static int proc_open_mac_stats(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read_mac_stats, NULL);
}

static int proc_open_fw_info(struct inode *inode, struct file *file)
{
	return seq_open(file, &fw_info_seq_ops);
}

static int proc_open_fw_log(struct inode *inode, struct file *file)
{
	return seq_open(file, &fw_log_seq_ops);
}

static int proc_read_fw_params(struct seq_file *m, void *v)
{
	struct fw_info_dump *fw_info = &wifi->fw_info;
	struct fw_params *params;
	struct img_priv *priv;

	priv = (struct img_priv *)(wifi->hw->priv);
	if (priv->state != STARTED) {
		pr_err("Interface is not initialized\n");
		goto exit;
	}

	if (rpu_fw_priv_cmd_sync(FW_GET_PARAMS, NULL) != 0) {
		goto exit;
	}

	params = (struct fw_params*)fw_info->info;
	seq_printf(m, "firmware params:\n"
				 "\techo_mode=%d\n"
				 "\tejtag_mode=%d\n"
				 "\tdebug_level=%d\n"
				 "\tdebug_flag=0x%x\n"
				 "\tdis_wifi_isr_thd=%d\n"
				 "\ten_wifi_isr_thd=%d\n",
				 params->echo_mode,
				 params->ejtag_mode,
				 params->debug_level,
				 params->debug_flag,
				 params->dis_wifi_isr_thd,
				 params->en_wifi_isr_thd);

	seq_printf(m, "\nfirmware command:\n"
				"\tread_fw_reg=0xb000c800,0x4\n"
				"\twrite_fw_reg=0xb000c800,0x80000000\n"
				"\tfw_txrx_count_info\n"
				"\tfw_txrx_queue_info\n"
				"\tfw_version_info\n"
				"\tfw_enable_ejtag\n");
	seq_printf(m, "\tdebug_level=n, n is: 0(close) 1(err) 2(info) 3(debug)\n"
			"\tdebug_flag=n, n is: 1(rx) 2(tx) 4(sdio) 8(memory)\n"
			"\techo_mode=n, n is: 0(disable) 1(enable)\n"
			"\tejtag_mode=n, n is: 0(disable) 1(enable), not available now\n"
			"\tdump_mem_info, dump firmware memory information.\n"
			);

exit:
	fw_info->finish = 0;
	return 0;
}

static ssize_t proc_write_fw_params(struct file *file,
				 const char __user *buffer,
				 size_t count,
				 loff_t *ppos)
{
	char buf[128];
	unsigned long val = 0;
	struct img_priv *priv;
	int ret = 0;
	struct fw_params params;
	int cmd = 0;

	if (!wifi->hw)
		return -ENODEV;

	priv = (struct img_priv *)(wifi->hw->priv);

	if (priv->state != STARTED) {
		pr_err("Interface is not initialized\n");
		return count;
	}

	if (count >= sizeof(buf))
		count = sizeof(buf) - 1;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	memset(&params, 0, sizeof(struct fw_params));
	buf[count] = '\0';

	pr_info("%s: %s\n", __func__, buf);

	memset(&params, 0, sizeof(struct fw_params));
	if(param_get_val(buf,"echo_mode=", &val)) {
		params.mask |= 1<<PARAM_ECHO_MODE;
		params.echo_mode=val;
	} else if(param_get_val(buf,"ejtag_mode=", &val)){
		params.mask |= 1<<PARAM_EJTAG_MODE;
		params.ejtag_mode=val;
	} else if(param_get_val(buf,"debug_level=", &val)){
		params.mask |= 1<<PARAM_DEBUG_LEVEL;
		params.debug_level=val;
	} else if(param_get_val(buf,"debug_flag=", &val)){
		params.mask |= 1<<PARAM_DEBUG_FLAG;
		params.debug_flag=val;
	} else if(param_get_val(buf,"dis_wifi_isr_thd=", &val)){
		params.mask |= 1<<PARAM_DIS_WIFI_ISR_THD;
		params.dis_wifi_isr_thd=val;
	} else if(param_get_val(buf,"en_wifi_isr_thd=", &val)){
		params.mask |= 1<<PARAM_EN_WIFI_ISR_THD;
		params.en_wifi_isr_thd=val;
	} else if (strstr(buf, "dump_mem_info")) {
		cmd = DUMP_MEM_INFO;
	} else
		pr_err("Invalid parameter name: %s\n", buf);

	if (params.mask)
		ret = rpu_fw_priv_cmd_sync(FW_SET_PARAMS, &params);
	else
		ret = rpu_fw_priv_cmd(cmd, NULL);

	if (ret)
		return ret;

	return count;
}

static int proc_open_fw_params(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read_fw_params, NULL);
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
static const struct proc_ops params_fops_config = {
	.proc_open = proc_open_config,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = proc_write_config,
	.proc_release = single_release
};
#else
static const struct file_operations params_fops_config = {
	.open = proc_open_config,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = proc_write_config,
	.release = single_release
};
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
static const struct proc_ops params_fops_phy_stats = {
    .proc_open = proc_open_phy_stats,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_write = NULL,
    .proc_release = single_release
};
#else
static const struct file_operations params_fops_phy_stats = {
	.open = proc_open_phy_stats,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = NULL,
	.release = single_release
};
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
static const struct proc_ops params_fops_mac_stats = {
    .proc_open = proc_open_mac_stats,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_write = NULL,
    .proc_release = single_release
};
#else
static const struct file_operations params_fops_mac_stats = {
	.open = proc_open_mac_stats,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = NULL,
	.release = single_release
};
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
static const struct proc_ops params_fops_fw_info = {
    .proc_open = proc_open_fw_info,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_write = NULL,
    .proc_release = seq_release
};
#else
static const struct file_operations params_fops_fw_info = {
	.open = proc_open_fw_info,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = NULL,
	.release = seq_release
};
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
static const struct proc_ops params_fops_fw_log = {
    .proc_open = proc_open_fw_log,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_write = NULL,
    .proc_release = seq_release
};
#else
static const struct file_operations params_fops_fw_log = {
	.open = proc_open_fw_log,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = NULL,
	.release = seq_release
};
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 10, 0))
static const struct proc_ops params_fops_fw_params = {
    .proc_open = proc_open_fw_params,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_write = proc_write_fw_params,
    .proc_release = single_release
};
#else
static const struct file_operations params_fops_fw_params = {
	.open = proc_open_fw_params,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = proc_write_fw_params,
	.release = single_release
};
#endif

static void set_default_phy_thresh(unsigned char *rf_params, int len)
{
	int i;
	unsigned char def[3];

	sprintf(def, "%02x", default_phy_threshold);

	for (i = 0; i < 14; i++) {
		memcpy(&rf_params[(len - i)*2 - 2], def, 2);
	}
}

void set_rf_params(unsigned char *rf_params)
{
	set_default_phy_thresh(rf_params, 94);

	/* TODO: Make this a struct */
	memset(wifi->params.rf_params, 0xFF, sizeof(wifi->params.rf_params));
	conv_str_to_byte(wifi->params.rf_params, rf_params, RF_PARAMS_SIZE);

	memcpy(wifi->params.rf_params_vpd, wifi->params.rf_params, RF_PARAMS_SIZE);
}

int proc_init(struct proc_dir_entry ***main_dir_entry)
{
	struct proc_dir_entry *entry;
	int err = 0;
	unsigned int i = 0;
	/*2.4GHz and 5 GHz PD and TX-PWR calibration params*/
	unsigned char rf_params[RF_PARAMS_SIZE * 2];

	//strncpy(rf_params,
	//	"1E00000000002426292A2C2E3237393F454A52576066000000002B2C3033373A3D44474D51575A61656B6F000000002B2C3033373A3D44474D51575A61656B6F000000002B2C3033373A3D44474D51575A61656B6F000000002B2C3033373A3D44474D51575A61656B6F00000000002426292A2C2E3237393F454A52576066000000002B2C3033373A3D44474D51575A61656B6F000000002B2C3033373A3D44474D51575A61656B6F000000002B2C3033373A3D44474D51575A61656B6F000000002B2C3033373A3D44474D51575A61656B6F0808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808",
	//	(RF_PARAMS_SIZE * 2));
	
	//From above check PHY Start Thresholds in the last 14 bytes (corerspond to all 14 channels). Each value specifies the threshold in half-dBm units without the negative sign.
	//The default is 0xB4 = 180, which represents -90 dBm.
	//strncpy(rf_params,"00204000240210020403040404050406040704080409040A040B040C040CF80C0C04FEF8F0E80000000000000000000000000000040403030201000001020303040405050505050505050505050505058C8C8C8C8C8C8C8C8C8C8C8C8C8C",
	//	(94 * 2));
	//strncpy(rf_params,"00204000240210020403040404050406040704080409040A040B040C040CF80b0C04FEF8ECEC0000000000000000000000000000000000000000000000000000000008080808080808080808080808088C8C8C8C8C8C8C8C8C8C8C8C8C8C",
	//	(94 * 2));
        memcpy(rf_params,"00204000240210020403040404050406040704080409040A040B040C040CF80B0C04FFFAF2EC0000000000000000000000000000000000000000000000000000000008080808080808080808080808088C8C8C8C8C8C8C8C8C8C8C8C8C8C",
                (94 * 2)); 
	set_default_phy_thresh(rf_params, 94);

	wifi = kzalloc(sizeof(struct wifi_dev), GFP_KERNEL);
	if (!wifi) {
		err = -ENOMEM;
		goto out;
	}

	wifi->umac_proc_dir_entry = proc_mkdir("rk915", init_net.proc_net);
	if (!wifi->umac_proc_dir_entry) {
		pr_err("Failed to create proc dir\n");
		err = -ENOMEM;
		goto  proc_dir_fail;
	}

	entry = proc_create("params", 0644, wifi->umac_proc_dir_entry,
			    &params_fops_config);
	if (!entry) {
		pr_err("Failed to create proc entry\n");
		err = -ENOMEM;
		goto  proc_entry1_fail;
	}

	entry = proc_create("phy_stats", 0444, wifi->umac_proc_dir_entry,
			    &params_fops_phy_stats);
	if (!entry) {
		pr_err("Failed to create proc entry\n");
		err = -ENOMEM;
		goto  proc_entry2_fail;
	}

	entry = proc_create("mac_stats", 0444, wifi->umac_proc_dir_entry,
			    &params_fops_mac_stats);
	if (!entry) {
		pr_err("Failed to create proc entry\n");
		err = -ENOMEM;
		goto  proc_entry3_fail;
	}
#ifdef RPU_SLEEP_ENABLE
	entry = proc_create("sleep_stats", 0444, wifi->umac_proc_dir_entry,
			    &params_fops_sleep_stats);
	if (!entry) {
		pr_err("Failed to create proc entry\n");
		err = -ENOMEM;
		goto  proc_entry4_fail;
	}
#endif

	entry = proc_create("fw_info", 0444, wifi->umac_proc_dir_entry,
			    &params_fops_fw_info);
	if (!entry) {
		pr_err("Failed to create proc entry\n");
		err = -ENOMEM;
		goto  proc_entry5_fail;
	}

	entry = proc_create("fw_log", 0444, wifi->umac_proc_dir_entry,
			    &params_fops_fw_log);
	if (!entry) {
		pr_err("Failed to create proc entry\n");
		err = -ENOMEM;
		goto  proc_entry6_fail;
	}

	entry = proc_create("fw_params", 0644, wifi->umac_proc_dir_entry,
			    &params_fops_fw_params);
	if (!entry) {
		pr_err("Failed to create proc entry\n");
		err = -ENOMEM;
		goto  proc_entry7_fail;
	}

	/* Initialize WLAN params */
	memset(&wifi->params, 0, sizeof(struct wifi_params));

	/* TODO: Make this a struct */
	memset(wifi->params.rf_params, 0xFF, sizeof(wifi->params.rf_params));
	conv_str_to_byte(wifi->params.rf_params, rf_params, RF_PARAMS_SIZE);

	if (!rf_params_vpd)
		rf_params_vpd = wifi->params.rf_params;

	memcpy(wifi->params.rf_params_vpd, rf_params_vpd, RF_PARAMS_SIZE);

	wifi->params.is_associated = 0;
	wifi->params.ed_sensitivity = -89;
	wifi->params.auto_sensitivity = 1;
	wifi->params.dot11a_support = 0;
	wifi->params.dot11g_support = 1;
	wifi->params.num_vifs = 2;

	/* Check, if required add it */
	wifi->params.tx_fixed_mcs_indx = -1;
	wifi->params.tx_fixed_rate = -1;
	wifi->params.num_spatial_streams = min(MAX_TX_STREAMS, MAX_RX_STREAMS);
	wifi->params.uccp_num_spatial_streams = min(MAX_TX_STREAMS,
						    MAX_RX_STREAMS);
	wifi->params.antenna_sel = 1;

	if (num_streams_vpd > 0)
		wifi->params.uccp_num_spatial_streams = num_streams_vpd;

	wifi->params.enable_early_agg_checks = 1;
	wifi->params.bt_state = 1;

	/* Defaults optimized for all clients
	 */
	wifi->params.mgd_mode_tx_fixed_mcs_indx = -1;
	wifi->params.mgd_mode_mcast_fixed_data_rate = -1;
	wifi->params.mgd_mode_tx_fixed_rate = -1;
	wifi->params.mgd_mode_mcast_fixed_nss = 1;
	wifi->params.mgd_mode_mcast_fixed_bcc_or_ldpc = 1;
	wifi->params.mgd_mode_mcast_fixed_stbc_enabled = 1;
	wifi->params.chnl_bw = WLAN_20MHZ_OPERATION;

	wifi->params.max_tx_streams = MAX_TX_STREAMS;
	wifi->params.max_rx_streams = MAX_RX_STREAMS;
	wifi->params.max_data_size  = 8 * 1024;

	wifi->params.max_tx_cmds = MAX_SUBFRAMES_IN_AMPDU_HT;
	wifi->params.disable_power_save = 0;
	wifi->params.disable_sm_power_save = 0;
	wifi->params.rate_protection_type = 0; 
	wifi->params.prod_mode_rate_preamble_type = 1; /* LONG */
	wifi->params.prod_mode_stbc_enabled = 0;
	wifi->params.prod_mode_bcc_or_ldpc = 0;
	wifi->params.bg_scan_enable = 0;
	memset(wifi->params.bg_scan_channel_list, 0, 50);
	memset(wifi->params.bg_scan_channel_flags, 0, 50);

	if (wifi->params.dot11g_support) {
		wifi->params.bg_scan_num_channels = 3;

		wifi->params.bg_scan_channel_list[i] = 1;
		wifi->params.bg_scan_channel_flags[i++] = ACTIVE;

		wifi->params.bg_scan_channel_list[i] = 6;
		wifi->params.bg_scan_channel_flags[i++] = ACTIVE;

		wifi->params.bg_scan_channel_list[i] = 11;
		wifi->params.bg_scan_channel_flags[i++] = ACTIVE;
	}

	wifi->params.disable_beacon_ibss = 0;
	wifi->params.pkt_gen_val = -1;
	wifi->params.init_pkt_gen = 0;
	wifi->params.payload_length = 4000;
	wifi->params.start_prod_mode = 0;
	wifi->params.fw_skip_rx_pkt_submit = 0;
	wifi->params.init_prod = 0;
	wifi->params.bg_scan_intval = 5000 * 1000; /* Once in 5 seconds */
	wifi->params.bg_scan_chan_dur = 300; /* Channel spending time */
	wifi->params.bg_scan_serv_chan_dur = 100; /* Oper chan spending time */
	wifi->params.nw_selection = 0;
	wifi->params.scan_type = ACTIVE;
	wifi->params.hw_scan_status = HW_SCAN_STATUS_NONE;
	wifi->params.fw_loaded = 0;
#ifdef RPU_SLEEP_ENABLE
#ifdef RPU_NO_SLEEP_FLAG
	wifi->params.rpu_sleep_type = LMAC_NO_SLEEP;
#else
	/* Default RPU Sleep is enabled
	 */
	if (lpw_no_sleep)
		wifi->params.rpu_sleep_type = LMAC_NO_SLEEP;
	else
		wifi->params.rpu_sleep_type = 0;
#endif
#endif

#ifdef ENABLE_DAPT
	wifi->params.dapt_thresh_offset = DAPT_THRESH_OFFSET;
	wifi->params.dapt_thresh_exponent = DAPT_THRESH_EXPONENT;
	wifi->params.dapt_thresh_min = DAPT_PHY_THRESH_MIN;
	wifi->params.dapt_thresh_max = DAPT_PHY_THRESH_MAX;
#endif

	wifi->params.min_dtim_peroid = 4;

	memset(&wifi->fw_info, 0, sizeof(struct fw_info_dump));
	wifi->fw_info.info = kzalloc(MAX_FW_INFO_SIZE, GFP_KERNEL);
	if (!wifi->fw_info.info) {
		err = -ENOMEM;
		goto proc_entry8_fail;
	}
	wifi->fw_info.len = MAX_FW_INFO_SIZE;
	wifi->fw_info.offset = 0;

	**main_dir_entry = wifi->umac_proc_dir_entry;
	return err;
proc_entry8_fail:
	remove_proc_entry("fw_params", wifi->umac_proc_dir_entry);
proc_entry7_fail:
	remove_proc_entry("fw_log", wifi->umac_proc_dir_entry);
proc_entry6_fail:
	remove_proc_entry("fw_info", wifi->umac_proc_dir_entry);
proc_entry5_fail:
	remove_proc_entry("sleep_stats", wifi->umac_proc_dir_entry);
#ifdef RPU_SLEEP_ENABLE
proc_entry4_fail:
	remove_proc_entry("mac_stats", wifi->umac_proc_dir_entry);
#endif
proc_entry3_fail:
	remove_proc_entry("phy_stats", wifi->umac_proc_dir_entry);
proc_entry2_fail:
	remove_proc_entry("params", wifi->umac_proc_dir_entry);
proc_entry1_fail:
	remove_proc_entry("rk915", init_net.proc_net);
proc_dir_fail:
	kfree(wifi);
out:
	return err;

}

void proc_exit(void)
{
	remove_proc_entry("fw_params", wifi->umac_proc_dir_entry);
	remove_proc_entry("fw_log", wifi->umac_proc_dir_entry);
	remove_proc_entry("fw_info", wifi->umac_proc_dir_entry);
#ifdef RPU_SLEEP_ENABLE
	remove_proc_entry("sleep_stats", wifi->umac_proc_dir_entry);
#endif
	remove_proc_entry("hal_stats", wifi->umac_proc_dir_entry);
	remove_proc_entry("mac_stats", wifi->umac_proc_dir_entry);
	remove_proc_entry("phy_stats", wifi->umac_proc_dir_entry);
	remove_proc_entry("params", wifi->umac_proc_dir_entry);
	remove_proc_entry("rk915", init_net.proc_net);
	kfree(wifi->fw_info.info);
	kfree(wifi);
	wifi = NULL;
}


