/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _RPU_IF_H_
#define _RPU_IF_H_

#include <linux/skbuff.h>

#include "hal.h"
#include "host_rpu_if.h"

#define UMAC_ROC_AC WLAN_AC_VO

struct umac_key {
	unsigned char *peer_mac;
	unsigned char *tx_mic;
	unsigned char *rx_mic;
	unsigned char *key;
};

struct ssid_desc {
	unsigned char ssid[MAX_SSID_LEN];
	unsigned char ssid_len;
};

struct scan_req {
	unsigned int n_channels;
	int n_ssids;
	unsigned int ie_len;
	unsigned char ie[256];
	unsigned int p2p_probe;
	/*TODO: Make this a structure*/
	unsigned short center_freq[50];
	unsigned char freq_max_power[50];
	unsigned char chan_flags[50];
	struct ssid_desc ssids[MAX_NUM_SSIDS];
};

/*commands*/
extern int rpu_scan(int index,
			    struct scan_req *req);

extern int rpu_scan_abort(int index);


extern int rpu_prog_tx(unsigned int queue,
			       unsigned int more_data,
			       unsigned int tokenid,
			       bool retry);

extern int rpu_prog_reset(unsigned int reset_type,
				  unsigned int rpu_mode);

extern int rpu_prog_vif_ctrl(int index,
				     unsigned char *vif_addr,
				     unsigned int  vif_type,
				     unsigned int  add_vif);

extern int rpu_prog_vif_basic_rates(int index,
					    unsigned char *vif_addr,
					    unsigned int basic_rate_set);

extern int rpu_prog_vif_short_slot(int index,
					   unsigned char *vif_addr,
					   unsigned int use_short_slot);

extern int rpu_prog_vif_atim_window(int index,
					    unsigned char *vif_addr,
					    unsigned int atim_window);

extern int rpu_prog_vif_aid(int index,
				    unsigned char *vif_addr,
				    unsigned int aid);

extern int rpu_prog_vif_op_channel(int index,
					   unsigned char *vif_addr,
					   unsigned char op_channel);

extern int rpu_prog_vif_conn_state(int index,
					      unsigned char *vif_addr,
					      unsigned int state);

extern int rpu_prog_vif_assoc_cap(int index,
					  unsigned char *vif_addr,
					  unsigned int caps);

extern int rpu_prog_vif_beacon_int(int index,
					   unsigned char *vif_addr,
					   unsigned int bcn_int);

extern int rpu_prog_vif_dtim_period(int index,
					    unsigned char *vif_addr,
					    unsigned int dtim_period);

extern int rpu_prog_vif_apsd_type(int index,
					  unsigned char *vif_addr,
					  unsigned int uapsd_type);

extern int rpu_prog_long_retry(int index,
				       unsigned char *vif_addr,
				       unsigned int long_retry);

extern int rpu_prog_short_retry(int index,
					unsigned char *vif_addr,
					unsigned int short_retry);

extern int rpu_prog_vif_bssid(int index,
				      unsigned char *vif_addr,
				      unsigned char *bssid);

extern int rpu_prog_vif_smps(int index,
				     unsigned char *vif_addr,
				     unsigned char smps_mode);

extern int rpu_prog_ps_state(int index,
				     unsigned char *vif_addr,
				     unsigned int powersave_state);

extern int rpu_prog_global_cfg(unsigned int rx_msdu_lifetime,
				       unsigned int tx_msdu_lifetime,
				       unsigned int sensitivity,
				       unsigned int dyn_ed_enabled,
				       unsigned char *rf_params);

extern int rpu_prog_cfgmisc(unsigned int flag);

extern int rpu_prog_txpower(unsigned int txpower);

extern int rpu_prog_patch_feature(unsigned int feature);

extern int rpu_prog_btinfo(unsigned int bt_state);

extern int rpu_prog_mcast_addr_cfg(unsigned char  *mcast_addr,
					   unsigned int add_filter);

extern int rpu_prog_mcast_filter_control(unsigned int
						 enable_mcast_filtering);

extern int rpu_prog_rcv_bcn_mode(unsigned int  bcn_rcv_mode);
extern int rpu_prog_cont_tx(int val);
extern int rpu_prog_txq_params(int index,
				       unsigned char *vif_addr,
				       unsigned int queue,
				       unsigned int aifs,
				       unsigned int txop,
				       unsigned int cwmin,
				       unsigned int cwmax,
				       unsigned int uapsd);

extern int rpu_prog_channel(unsigned int prim_ch,
				    unsigned int center_freq1,
				    unsigned int center_freq2,
				    unsigned int ch_width,
				    unsigned int freq_band);

extern int rpu_prog_peer_key(int index,
				     unsigned char *vif_addr,
				     unsigned int op,
				     unsigned int key_id,
				     unsigned int key_type,
				     unsigned int cipher_type,
				     struct umac_key *key);

extern int rpu_prog_if_key(int   index,
				   unsigned char *vif_addr,
				   unsigned int op,
				   unsigned int key_id,
				   unsigned int cipher_type,
				   struct umac_key *key);

extern int rpu_prog_mib_stats(void);

extern int rpu_prog_clear_stats(void);

extern int rpu_prog_phy_stats(void);

extern int rpu_prog_ba_session_data(unsigned int op,
					    unsigned short tid,
					    unsigned short *ssn,
					    unsigned short ba_policy,
					    unsigned char *sta_addr,
					    unsigned char *peer_add);


extern int rpu_prog_roc(unsigned int roc_status,
				unsigned int roc_channel,
				unsigned int roc_duration,
				unsigned int roc_type);
extern int rpu_prog_read_csr(unsigned int addr);

#ifdef CONFIG_PM
extern int rpu_prog_econ_ps_state(int if_index,
					  unsigned int ps_state);
#endif

extern int rpu_prog_phy_thresh(unsigned int *thresh);

extern int rpu_prog_txrx_test(int status);
extern int rpu_fw_priv_cmd(unsigned int type, void *priv);
extern int rpu_fw_priv_cmd_sync(unsigned int type, void *priv);

/* Events  */
extern void rpu_scan_complete(void *context,
				      struct host_event_scanres *scan_res,
				      unsigned char *skb,
				      unsigned int len);
extern void rpu_scan_abort_complete(void *context);

extern void rpu_reset_complete(char *rpu_version,
				       void *context);

extern void rpu_proc_tx_complete(struct umac_event_tx_done *txdone,
				    void *context);

extern void rpu_proc_rx_event(void *nbuff, void *context);

extern void rpu_tx_complete(struct umac_event_tx_done *txdone,
				    void *context);

extern void rpu_tx_proc_unfi_tx_done(void);

extern void rpu_rx_frame(struct sk_buff *skb,
				 void *context);


extern void rpu_mac_stats(struct umac_event_mac_stats *mac_stats,
				  void *context);

extern void rpu_fw_priv_cmd_done(struct fw_priv_cmd_done *event,
			   void *context);

extern void rpu_fw_info_dump_start(void *context, unsigned int type, unsigned int reg);

extern void rpu_noa_event(int event,
				  struct umac_event_noa *noa_event,
				  void *context,
				  struct sk_buff *skb);

extern void rpu_ch_prog_complete(int event,
					 struct umac_event_ch_prog_complete *ch,
					 void *context);

/*Sleep Controll config API's
 */
int rpu_prog_pwrmgmt_pwr_on_value(unsigned int *pwr_on_values, unsigned int size);
int rpu_prog_pwrmgmt_pwr_off_value(unsigned int *pwr_off_values, unsigned int size);
int rpu_prog_pwrmgmt_ram_on_state(unsigned int *ram_on_states, unsigned int size);
int rpu_prog_pwrmgmt_ram_off_state(unsigned int *ram_off_states, unsigned int size);
int rpu_prog_pwrmgmt_pwr_on_time(unsigned int *pwr_on_times, unsigned int size);
int rpu_prog_pwrmgmt_pwr_off_time(unsigned int *pwr_off_times, unsigned int size) ;
int rpu_prog_pwrmgmt_ram_on_time(unsigned int *ram_on_times, unsigned int size);
int rpu_prog_pwrmgmt_ram_off_time(unsigned int *ram_off_times, unsigned int size) ;
int rpu_prog_pwrmgmt_sleep_freq(unsigned int sleep_freq);
int rpu_prog_pwrmgmt_clk_adj(unsigned int clk_adj_val);
int rpu_prog_pwrmgmt_wakeup_time(unsigned int wakeup_time);
/* Init/Deinit */

extern int rpu_if_init(void *context,
				    const char *name);

extern void rpu_if_deinit(void);

extern void rpu_if_free_outstnding(void);



#endif /* _RPU_IF_H_ */

/* EOF */

