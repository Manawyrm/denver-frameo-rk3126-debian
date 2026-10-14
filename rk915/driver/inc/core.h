/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _CORE_H_
#define _CORE_H_

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/wireless.h>
#include <linux/firmware.h>
#include "wake_lock.h"

#include <net/mac80211.h>

#include "descriptors.h"
#include "rpu_if.h"
#include "hal_common.h"
#include "version.h"
#include "debug.h"
#include "firmware.h"

extern int uccp_reinit;
extern struct cmd_send_recv_cnt cmd_info;

#ifdef CONFIG_PM
extern unsigned char img_suspend_status;
extern unsigned char rx_interrupt_status;
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
enum ieee80211_band {
        IEEE80211_BAND_2GHZ = NL80211_BAND_2GHZ,
        IEEE80211_BAND_5GHZ = NL80211_BAND_5GHZ,
        IEEE80211_BAND_60GHZ = NL80211_BAND_60GHZ,

        /* keep last */
        IEEE80211_NUM_BANDS
};
#endif

/* Wrapper to check return values for all
 * umac_if layer calls.
 */
#define CALL_RPU(prog_rpu, ...) \
do {                            \
	ret = prog_rpu(__VA_ARGS__);   \
	if (ret != 0)                    \
		goto prog_rpu_fail;      \
} while (0)

#ifdef CONFIG_NL80211_TESTMODE
#define MAX_NL_DUMP_LEN (PAGE_SIZE-1024)
/* This section contains example code for using netlink
 * attributes with the testmode command in nl80211.
 */

/* These enums need to be kept in sync with userspace */
enum rpu_testmode_attr {
	__RPU_TM_ATTR_INVALID = 0,
	RPU_TM_ATTR_CMD      = 1,
	RPU_TM_ATTR_DUMP      = 2,
	/* keep last */
	__RPU_TM_ATTR_AFTER_LAST,
	RPU_TM_ATTR_MAX       = __RPU_TM_ATTR_AFTER_LAST - 1
};

enum rpu_testmode_cmd {
	RPU_TM_CMD_ALL	= 0,
	RPU_TM_CMD_GRAM  = 1,
	RPU_TM_CMD_COREA  = 2,
	RPU_TM_CMD_COREB  = 3,
	RPU_TM_CMD_PERIP = 4,
	RPU_TM_CMD_SYSBUS = 5,
};

#endif
extern unsigned int system_rev;
extern unsigned int ftm;
extern unsigned int down_fw_in_probe;

extern unsigned char vif_macs[2][ETH_ALEN];

extern spinlock_t tsf_lock;

#define RESET_RX_CONTROL_INFO 1
#define MAX_OUTSTANDING_CTRL_REQ 2
#define RESET_TIMEOUT 1000   /* In milli-seconds*/
#define RESET_TIMEOUT_TICKS msecs_to_jiffies(RESET_TIMEOUT)
/*100: For ROC, 500: For initial*/
#define CH_PROG_TIMEOUT 3000   /* In milli-seconds*/
#define CH_PROG_TIMEOUT_TICKS msecs_to_jiffies(CH_PROG_TIMEOUT)

#define QUEUE_FLUSH_TIMEOUT  2000   /* Specify delay in milli-seconds*/
#define QUEUE_FLUSH_TIMEOUT_TICKS   msecs_to_jiffies(QUEUE_FLUSH_TIMEOUT)

#define TX_DEINIT_TIMEOUT 5000
#define TX_DEINIT_TIMEOUT_TICKS msecs_to_jiffies(TX_DEINIT_TIMEOUT)

#ifdef CONFIG_PM
#define PS_ECON_CFG_TIMEOUT 1000
#define PS_ECON_CFG_TIMEOUT_TICKS msecs_to_jiffies(PS_ECON_CFG_TIMEOUT)
#endif

#define FW_ERR_PROCESS_TIMEOUT 5000   /* In milli-seconds*/
#define FW_ERR_PROCESS_TIMEOUT_TICKS msecs_to_jiffies(FW_ERR_PROCESS_TIMEOUT)

#define RXQ_EMPTY_TIMEOUT 5000   /* In milli-seconds*/
#define RXQ_EMPTY_TIMEOUT_TICKS msecs_to_jiffies(RXQ_EMPTY_TIMEOUT)

#define TX_COMPLETE_TIMEOUT 1000  /* In milli-seconds*/
#define TX_COMPLETE_TIMEOUT_TICKS msecs_to_jiffies(TX_COMPLETE_TIMEOUT)
#define SCAN_ABORT_TIMEOUT 1000
#define SCAN_ABORT_TIMEOUT_TICKS msecs_to_jiffies(SCAN_ABORT_TIMEOUT)
#define CANCEL_HW_ROC_TIMEOUT 1000
#define CANCEL_HW_ROC_TIMEOUT_TICKS msecs_to_jiffies(CANCEL_HW_ROC_TIMEOUT)

#define DEFAULT_TX_ANT_SELECT 3 /* bitmap of antennas for tx, 3=> both first and
				 * second antenna to be used
				 */
#define DEFAULT_TX_POWER 15
#define DEFAULT_RTS_THRESHOLD 2347
#define SUPPORTED_FILTERS (FIF_ALLMULTI | FIF_BCN_PRBRESP_PROMISC)
#define TX_DESC_BUCKET_BOUND 32

#define MAX_DATA_SIZE (0) /* Defined in HAL (or) can be configured from proc */
#define MAX_TX_QUEUE_LEN 192
#define MAX_AUX_ADC_SAMPLES 10

/* Maximum number of Tx streams supported */
/* Maximum number of RX streams supported */
	#define MAX_TX_STREAMS 1 
	#define MAX_RX_STREAMS 1 

#define   MAX_RSSI_SAMPLES 10
#define   RPU_DBG_DEFAULT		0

#define CLOCK_MASK 0x3FFFFFFF
#define TICK_NUMRATOR 12288 /* 12288 KHz  */
#define TICK_DENOMINATOR 1000 /* 1000 KHz */

#define BTS_AP_24GHZ_ETS 195 /* Microsecs */
#define BTS_AP_5GHZ_ETS 25 /* Microsecs */

#define RF_PARAMS_SIZE 369
#define DEFAULT_MAC_ADDRESS "001122334455"

#define LPW_RECOVERY_FROM_RPU

//#define PKTGEN_MULTI_TX

enum ptype {
	UCAST = 0,
	MCAST
};

enum noa_triggers {
	FROM_TX = 0,
	FROM_TX_DONE,
	FROM_EVENT_NOA
};

#define HW_SCAN_TIMEOUT_ABORT
#ifdef RK3036_DONGLE
#define HW_SCAN_TIMEOUT 5 // second
#else
#define HW_SCAN_TIMEOUT 10 // second
#endif
enum rpu_hw_scan_status {
	HW_SCAN_STATUS_NONE,
	HW_SCAN_STATUS_PROGRESS
};

struct wifi_sync {
	unsigned int  status;
	unsigned char ts1[8];
	unsigned long long atu;
	unsigned char  bssid[8];
	unsigned char  name[10];
	unsigned int  ts2;
};

struct wifi_params {
	int ed_sensitivity;
	int num_vifs;
	int tx_fixed_rate;
	int tx_fixed_mcs_indx;
	int mgd_mode_tx_fixed_rate;
	int mgd_mode_tx_fixed_mcs_indx;
#ifdef HAL_PCIE
	unsigned int pci_base_addr;
	int no_words;
#endif
	unsigned int peer_ampdu_factor;
	unsigned char is_associated;
	unsigned char rate_protection_type;
	unsigned char num_spatial_streams;
	unsigned char enable_early_agg_checks;
	unsigned char uccp_num_spatial_streams;
	unsigned char auto_sensitivity;
	/*RF Params: Input to the RF for operation*/
	unsigned char  rf_params[RF_PARAMS_SIZE];
	unsigned char  rf_params_vpd[RF_PARAMS_SIZE];
	unsigned char production_test;
	unsigned int dot11a_support;
	unsigned int dot11g_support;
	unsigned int chnl_bw;
	unsigned int prod_mode_chnl_bw_40_mhz;
	unsigned int sec_ch_offset_40_plus;
	unsigned int sec_ch_offset_40_minus;

	/*Multicast  Rate config options*/
	unsigned int mgd_mode_mcast_fixed_rate_flags;
	int mgd_mode_mcast_fixed_data_rate;
	unsigned int mgd_mode_mcast_fixed_bcc_or_ldpc;
	unsigned int mgd_mode_mcast_fixed_stbc_enabled;
	unsigned int mgd_mode_mcast_fixed_preamble;
	unsigned char mgd_mode_mcast_fixed_nss;
	/*End*/

	unsigned int prod_mode_rate_flag;
	unsigned int prod_mode_rate_preamble_type;
	unsigned int prod_mode_stbc_enabled;
	unsigned int prod_mode_bcc_or_ldpc;
	unsigned int max_tx_streams;
	unsigned int max_rx_streams;
	unsigned int max_data_size;
	unsigned int disable_power_save;
	unsigned int disable_sm_power_save;
	unsigned int max_tx_cmds;
	unsigned int prod_mode_chnl_bw_80_mhz;
	unsigned int sec_40_ch_offset_80_plus;
	unsigned int sec_40_ch_offset_80_minus;
	unsigned int disable_beacon_ibss;
	unsigned char bg_scan_channel_list[50];
	unsigned char bg_scan_channel_flags[50];
	unsigned int bg_scan_enable;
	unsigned int bg_scan_intval;
	unsigned int bg_scan_chan_dur;
	unsigned int bg_scan_serv_chan_dur;
	unsigned int bg_scan_num_channels;
	unsigned int nw_selection;
	unsigned int hw_scan_status;
	unsigned int scan_type;
	unsigned int set_tx_power;
	unsigned int aux_adc_chain_id;
	unsigned char pdout_voltage[MAX_AUX_ADC_SAMPLES];
	char rssi_average[MAX_RSSI_SAMPLES];
	unsigned int extra_scan_ies;
	unsigned int fw_loaded;
	struct wifi_sync sync[MAX_VIFS];
	unsigned int bt_state;
	unsigned int antenna_sel;
	int fw_skip_rx_pkt_submit;
	int pkt_gen_val;
	int init_pkt_gen;
	int payload_length;
	int start_prod_mode;
	int echo_mode;
	int init_prod;
	unsigned char bypass_vpd;
	unsigned int cont_tx;
#ifdef RPU_SLEEP_ENABLE
	unsigned char rpu_sleep_type;
#endif
	int dapt_thresh_offset;
	int dapt_thresh_exponent;
	int dapt_thresh_min;
	int dapt_thresh_max;

	int min_dtim_peroid;
};

struct cmd_send_recv_cnt {
	int tx_cmd_send_count;
	int tx_done_recv_count;
	int total_cmd_send_count;
	unsigned int outstanding_ctrl_req;
	unsigned long control_path_flags;
	spinlock_t control_path_lock;
	struct sk_buff_head outstanding_cmd;
};

#define OUTS_CMD_CHECK
#ifdef OUTS_CMD_CHECK
#define SET_TIME_TICKS_TO_SKB_CB(skb, ticks)	(*((unsigned long *)&((skb)->cb[0])) = ticks)
#define GET_TIME_TICKS_FROM_SKB_CB(skb)			(*((unsigned long *)&((skb)->cb[0])))
#endif

struct wifi_stats {
	unsigned int ht_tx_mcs0_packet_count;
	unsigned int ht_tx_mcs1_packet_count;
	unsigned int ht_tx_mcs2_packet_count;
	unsigned int ht_tx_mcs3_packet_count;
	unsigned int ht_tx_mcs4_packet_count;
	unsigned int ht_tx_mcs5_packet_count;
	unsigned int ht_tx_mcs6_packet_count;
	unsigned int ht_tx_mcs7_packet_count;
	unsigned int ht_tx_mcs8_packet_count;
	unsigned int ht_tx_mcs9_packet_count;
	unsigned int ht_tx_mcs10_packet_count;
	unsigned int ht_tx_mcs11_packet_count;
	unsigned int ht_tx_mcs12_packet_count;
	unsigned int ht_tx_mcs13_packet_count;
	unsigned int ht_tx_mcs14_packet_count;
	unsigned int ht_tx_mcs15_packet_count;
	unsigned int tx_cmds_from_stack;
	unsigned int tx_dones_to_stack;
	unsigned int system_rev;
	unsigned int outstanding_cmd_cnt;
	unsigned int max_outstanding_cmd_queue_cnt;
	unsigned int pending_tx_cnt;
	unsigned int umac_scan_req;
	unsigned int umac_scan_complete;
	unsigned int gen_cmd_send_count;
	unsigned int tx_cmd_send_count_single;
	unsigned int tx_cmd_send_count_multi;

	unsigned int tx_noagg_not_qos;
	unsigned int tx_noagg_not_ampdu;
	unsigned int tx_noagg_not_addr;

	unsigned int tx_cmd_send_count_beaconq;
	unsigned int tx_done_recv_count;
	unsigned int rx_packet_mgmt_count;
	unsigned int rx_packet_data_count;
	/*MAC Stats*/
	unsigned int roc_start;
	unsigned int roc_stop;
	unsigned int roc_complete;
	unsigned int roc_stop_complete;
	/* TX related */
	unsigned int tx_cmd_cnt; /* Num of TX commands received from host */
	unsigned int tx_done_cnt; /* Num of Tx done events sent to host */
	unsigned int tx_edca_trigger_cnt; /* Num times EDCA engine was
					   * triggered
					   */
	unsigned int tx_edca_isr_cnt; /* Num of times EDCA ISR was generated */
	unsigned int tx_start_cnt; /* Num of TX starts to MCP */
	unsigned int tx_abort_cnt; /* Num of TX aborts detected */
	unsigned int tx_abort_isr_cnt; /* Num of TX aborts received from MCP */
	unsigned int tx_underrun_cnt; /* Num of under-runs */
	unsigned int tx_rts_cnt; /* Num of RTS frames Txd */
	unsigned int tx_ampdu_cnt; /* Num of AMPDUs txd incremented by 1 for
				    * each A-MPDU (consisting of one or more
				    * MPDUs)
				    */
	unsigned int tx_mpdu_cnt; /* Num of MPDUs txd  incremented by 1 for
				   * MPDU (1 for each A-MPDU subframe)
				   */
	unsigned int tx_crypto_post; /* Num jobs posted to crypto */  
	unsigned int tx_crypto_done; /* Jobs completed by crypto */
	unsigned int rx_pkt_to_umac;  /* Num packets received by umac */
	unsigned int rx_crypto_post;  /* Num jobs posted to crypto */
	unsigned int rx_crypto_done;  /* Jobs completed by crypto */
	/* RX related */
	unsigned int rx_isr_cnt; /* Num of RX ISRs */
	unsigned int rx_ack_cts_to_cnt; /* Num of timeouts ACK */
	unsigned int rx_cts_cnt; /* Num of CTS frames received */
	unsigned int rx_ack_resp_cnt; /* Num of Ack frames received */
	unsigned int rx_ba_resp_cnt; /* Num of BA frames received */
	unsigned int rx_fail_in_ba_bitmap_cnt; /* Num of BA frames indicating at
						* least one failure in the BA
						* bitmap
						*/
	unsigned int rx_circular_buffer_free_cnt; /* Num of entries returned to
						   * RX circular buffers
						   */
	unsigned int rx_mic_fail_cnt; /* Num of MIC failures */

	/* HAL related */
	unsigned int hal_cmd_cnt; /* Num of commands received by HAL from the
				   * host
				   */
	unsigned int hal_event_cnt; /* Num of events sent by HAL to the host */
	unsigned int hal_ext_ptr_null_cnt; /* Num of packets dropped due to lack
					    * of Ext Ram buffers from host
					    */
	/* LPW PHY Related */
	unsigned int csync_timeout_cntr;      /* lpw phy stats - offset 0x120 */
	unsigned int fsync_timeout_cntr;      /* lpw phy stats - offset 0x124 */
	unsigned int acdrop_timeout_cntr;     /* lpw phy stats - offset 0x128 */
	unsigned int csync_abort_agctrig_cntr;/* lpw phy stats - offset 0x12c */
	unsigned int crc_success_cnt;  /* lmac crc succ cnt */
	unsigned int crc_fail_cnt;  /* lmac crc fail cnt */
#ifdef RPU_SLEEP_ENABLE
	unsigned int rpu_boot_cnt; /* num of times lmac booted */
	unsigned int sleep_stats[12];
#endif
	/*RF Calibration Data*/
	unsigned int rf_calib_data_length;
	unsigned char rf_calib_data[MAX_RF_CALIB_DATA];
	unsigned int pdout_val;
	unsigned char rpu_lmac_version[8];
	unsigned char fw_version[32];
};


struct tx_pkt_info {
	struct sk_buff_head pkt;
	unsigned int hdr_len;
	unsigned int queue;
	unsigned int vif_index;
	unsigned int rate[4];
	unsigned int retries[4];
	unsigned int curr_retries;
	unsigned int max_retries;
	int roc_peer_id;
	int peer_id;
	bool adjusted_rates;
};


struct tx_config {
	/* Used to protect the TX pool */
	spinlock_t lock;

	/* Used to store tx tokens(buff pool ids) */
	unsigned long buf_pool_bmp[(NUM_TX_DESCS/TX_DESC_BUCKET_BOUND) + 1];

	unsigned int outstanding_tokens[NUM_ACS];
	unsigned int next_spare_token_ac;

	/* Used to store the address of pending skbs per ac */
	struct sk_buff_head pending_pkt[MAX_PEND_Q_PER_AC]
				       [NUM_ACS];

	unsigned int curr_peer_opp[NUM_ACS];

	/* Used to store the address of tx'ed skb and len of 802.11 hdr
	 * it will be used in tx complete.
	 */
	struct tx_pkt_info pkt_info[NUM_TX_DESCS];

	unsigned int queue_stopped_bmp;
	struct sk_buff_head proc_tx_list[NUM_TX_DESCS];
	unsigned int outstanding_pkts[NUM_TX_DESCS];
	/* if is set to 1, means the tx desc id was sended to fw with no tx done */
	unsigned int tx_desc_had_send_to_io[NUM_TX_DESCS];
};

enum device_state {
	STOPPED = 0,
	STARTED
};

enum tid_aggr_state {
	TID_STATE_INVALID = 0,
	TID_STATE_AGGR_START,
	TID_STATE_AGGR_STOP,
	TID_STATE_AGGR_OPERATIONAL
};

#define TID_INITIATOR_STA 0x0000
#define TID_INITIATOR_AP 0x0010

struct sta_tid_info {
	unsigned short ssn;
	enum tid_aggr_state tid_state;
};

#ifdef CONFIG_PM
struct econ_ps_cfg_status {
	unsigned char completed;
	unsigned char result;
	int wake_trig;
	int processing;
};
#endif

struct current_channel {
	unsigned int pri_chnl_num;
	unsigned int center_freq1;
	unsigned int center_freq2;
	unsigned int freq_band;
	unsigned int ch_width;
};

struct roc_params {
	unsigned char roc_in_progress;
	unsigned char roc_starting;
	unsigned int roc_type;
	bool need_offchan;
	atomic_t roc_mgmt_tx_count;
};

/* send null-frame to keep-alive with AP */
//#define ENABLE_KEEP_ALIVE
#define SEND_NULL_FRAME_INTERVAL_SECONDS	30

/* enable recovery work when detect firmware error happens */
#define ENABLE_FW_ERROR_RECOVERY

/* Dynamic Adaptation of PHY Thresholds */
#ifndef STA_AP_COEXIST
#define ENABLE_DAPT
#endif
//#define ENABLE_DAPT_BEACON

// half-dBm units without the negative sign
// 140 represents -70 dBm
#define DAPT_DEFAULT_PHY_THRESH	140
#define DAPT_SCAN_PHY_THRESH 180
#define DAPT_P2P_SCAN_PHY_THRESH 140
#define DAPT_NON_STA_CONN_PHY_THRESH	140

#ifdef ENABLE_DAPT
/*
 * use latest e.g. 100 rx rssi within 1 seconds to calcute average rssi
 * use average rssi to calcute phy thresh
 */
#define DAPT_CALC_INTERVAL 1000 // ms
#define DAPT_MAX_RSSI_SAMPLE 100
#define DAPT_MIN_RSSI_SAMPLE 10

#define DAPT_PHY_THRESH_MIN	110
#define DAPT_PHY_THRESH_MAX	190

#define DAPT_THRESH_OFFSET 50	// means set the PHY thresholds 25 dB below the true RSSI
								// half-dB  steps  (e.g.,  60  means  30  dBm)
#define DAPT_THRESH_EXPONENT 4

#define DAPT_SETED_PHY_THRESH_COUNT	60

struct dapt_params {
	int main_index;
	int p2p_index;
	unsigned char vif_addr[MAX_VIFS][ETH_ALEN];
	unsigned char bssid[MAX_VIFS][ETH_ALEN];
	int conn_state[MAX_VIFS];

	int dapt_disable;
	struct timer_list dapt_timer;
	int timer_start;

	// save rx frame rssi samples
	s8 sam_history[MAX_VIFS][DAPT_MAX_RSSI_SAMPLE];
	int sam_read[MAX_VIFS];
	int sam_write[MAX_VIFS];
	int sam_size[MAX_VIFS];

	// save beacon rssi samples
	s8 bcn_history[MAX_VIFS][DAPT_MAX_RSSI_SAMPLE];
	int bcn_read[MAX_VIFS];
	int bcn_write[MAX_VIFS];
	int bcn_size[MAX_VIFS];

	unsigned int thresh_accum[MAX_VIFS];
	unsigned int avg_thresh[MAX_VIFS];
	unsigned int new_thresh[MAX_VIFS];	// calc from rssi

	unsigned int cur_seted_thresh[14]; // current seted phy thresh to rpu
	unsigned int save_seted_thresh[14]; // saved seted phy thresh to rpu

	// history of seted phy thresh	
	unsigned int thr_history[14][DAPT_SETED_PHY_THRESH_COUNT];
	int cur_thr_offset[14];
	int last_thresh;
	int both_zero_count;
};

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 6, 0))
extern void dapt_timer_expiry(struct timer_list *t);
#else
extern void dapt_timer_expiry(unsigned long data);
#endif
#endif

struct vif_info_s {
	unsigned char vif_addr[MAX_VIFS][ETH_ALEN];
	unsigned char bssid[MAX_VIFS][ETH_ALEN];
	int conn_state[MAX_VIFS];
};

struct img_priv {
	struct proc_dir_entry *umac_proc_dir_entry;
	struct device *dev;
	struct mac_address if_mac_addresses[MAX_VIFS];
	unsigned int current_vif_count;
	unsigned int active_vifs;
	struct mutex mutex;
	struct mutex scan_mutex;
	int state;
	int txpower;
	unsigned char mc_filters[MCST_ADDR_LIMIT][6];
	int mc_filter_count;
	void *net_dev;
	void *sdata;
	int sniffer;

	struct tasklet_struct proc_tx_tasklet;
	/*ROC Work*/
	struct delayed_work roc_complete_work;
	struct roc_params roc_params;
	struct current_channel cur_chan;
	struct tx_config tx;
	struct sk_buff_head pending_pkt[NUM_ACS];

	/* Regulatory stuff */
	char alpha2[2]; /* alpha2 country code */
#ifdef CONFIG_PM
	struct econ_ps_cfg_status econ_ps_cfg_stats;
#endif
	struct wifi_params *params;
	struct wifi_stats  *stats;
	struct fw_info_dump *fw_info;
	char name[20];
	char scan_abort_done;
	char cancel_hw_roc_done;
	char cancel_roc;
	char chan_prog_done;
	char reset_complete;
	char tx_deinit_complete;
	int power_save; /* Will be set only when a single VIF in
			 * STA mode is active
			 */
	struct ieee80211_vif *vifs[MAX_VIFS];
	struct ieee80211_sta *peers[MAX_PEERS];
	struct ieee80211_hw *hw;
	struct sta_tid_info  tid_info[32];
	spinlock_t bcast_lock; /* Used to ensure more_frames bit is set properly
				* when transmitting bcast frames in AP in IBSS
				* modes
				*/
	spinlock_t roc_lock;
	unsigned char tx_antenna;
	unsigned char tx_last_beacon;
	unsigned int rts_threshold;
#ifdef RPU_SLEEP_ENABLE
	struct timer_list init_sleep_timer;
#endif
#ifdef ENABLE_DAPT
	spinlock_t dapt_lock;
	struct dapt_params dapt_params;
#endif
#ifdef ENABLE_SPLIT_MULT_SSID_SCAN
	struct scan_req remain_scan_req;
	int scan_req_vif_iface;
#endif
#ifdef HW_SCAN_TIMEOUT_ABORT
	struct timer_list scan_timer;
#endif
	struct timer_list roc_timer;
	struct mutex scan_cancel_mutex;
	spinlock_t scan_cancel_lock;
	int in_scan_timeout;
	int cmd_reset_count;
	int iftype;
	int p2p_scan;
#ifdef ENABLE_KEEP_ALIVE
	struct timer_list keep_alive_timer;
#endif
	struct vif_info_s vif_info;
	int pri_chnl_num;

	/* null frame info */
	unsigned short null_frame_seq_no;
	int null_frame_desc_id;
	int null_frame_sending;
	int null_frame_send_count;
	struct sk_buff *null_frame_skb;

	int tx_retry_frm_cnt;

	char read_csr_complete;
	unsigned int read_csr_value;
};

struct fw_info_dump {
#define MAX_FW_INFO_SIZE (32*1024)
	unsigned char *info;
	unsigned int len;
	unsigned int offset;
	unsigned int finish;
	unsigned int type;
	unsigned int reg;
	/* following variable must be at end */
	unsigned long long last_total_tick;
	unsigned long long last_total_isr_tick;
};

extern struct wifi_dev *wifi;
struct wifi_dev {
	struct proc_dir_entry *umac_proc_dir_entry;
	struct wifi_params params;
	struct wifi_stats stats;
	struct ieee80211_hw *hw;
	struct fw_info_dump fw_info;
};

struct edca_params {
	unsigned short txop; /* units of 32us */
	unsigned short cwmin;/* units of 2^n-1 */
	unsigned short cwmax;/* units of 2^n-1 */
	unsigned char aifs;
	unsigned char uapsd;
};

struct umac_vif {
	struct timer_list bcn_timer;
	struct uvif_config {
		unsigned int atim_window;
		unsigned int aid;
		unsigned int bcn_lost_cnt;
		struct edca_params edca_params[NUM_ACS];
	} config;

	unsigned int noa_active;
	struct sk_buff_head noa_que;
	unsigned int noa_tx_allowed;

	int vif_index;
	struct ieee80211_vif *vif;
	struct img_priv *priv;
	unsigned char bssid[ETH_ALEN];
	unsigned int peer_ampdu_factor;

	/*Global Sequence no for non-qos and mgmt frames/vif*/
	__u16 seq_no;

};

struct umac_sta {
	int index;
	int vif_index;
};


struct curr_peer_info {
	int id;
	int op_chan_idx;
};

#ifdef ENABLE_DAPT
extern void dapt_param_init(struct img_priv *priv);
extern void dapt_param_late_init(struct img_priv *priv);
extern void dapt_param_deinit(struct img_priv *priv);
extern void dapt_notify_bssid_change(struct img_priv *priv,
					int index,
					unsigned char *vif_addr,
					unsigned char *bssid);
extern void dapt_notify_conn_state(struct img_priv *priv,
					int index,
					unsigned char *vif_addr,					
					unsigned int connect_state);
extern void dapt_timer_handler(struct img_priv *priv);
extern void dapt_disable(struct img_priv *priv, int disable);
extern int dapt_set_phy_thresh(struct img_priv *priv, int thresh, int ch, int set);
extern void dapt_scan(struct img_priv *priv);
extern void dapt_scan_complete(struct img_priv *priv);
extern void dapt_beacon(struct img_priv *priv, s8 rssi, int index);
#endif
extern void init_vif_info(struct img_priv *priv);
extern bool is_wlan_connected(struct img_priv *priv);
extern bool is_p2p_connected(struct img_priv *priv);
extern int  rpu_core_init(struct img_priv *priv, unsigned int ftm);
extern void rpu_core_deinit(struct img_priv *priv, unsigned int ftm);
extern void rpu_vif_add(struct umac_vif  *uvif);
extern void rpu_vif_remove(struct umac_vif *uvif);
extern void rpu_vif_set_edca_params(unsigned short queue,
					    struct umac_vif *uvif,
					    struct edca_params *params,
					    unsigned int vif_active);
extern void rpu_vif_bss_info_changed(struct umac_vif *uvif,
				     struct ieee80211_vif *vif,
				     struct ieee80211_hw *hw,
				     struct ieee80211_bss_conf *bss_conf,
				     unsigned int changed);
extern int  rpu_tx_frame(struct sk_buff *skb,
				 struct ieee80211_sta *sta,
				 struct img_priv *priv,
				 bool bcast);
extern int __rpu_tx_frame(struct img_priv *priv,
				  unsigned int queue,
				  unsigned int token_id,
				  unsigned int more_frames,
				  bool retry);
extern void rpu_tx_init(struct img_priv *priv);
extern void rpu_tx_deinit(struct img_priv *priv);
void rpu_tx_proc_send_pend_frms_all(struct img_priv *priv,
					   int chan_id);
extern void proc_bss_info_changed(unsigned char *mac_addr, int value);
extern void packet_generation(unsigned long data);
extern int wait_for_reset_complete(struct img_priv *priv, int enable);
extern int wait_for_read_csr_cmp(struct img_priv *priv);

extern int rpu_tx_proc_pend_frms(struct img_priv *priv,
				   int queue,
				   int token_id);
int get_token(struct img_priv *priv,
		     int queue);
void free_token(struct img_priv *priv,
		int token_id,
		int queue);

struct curr_peer_info get_curr_peer_opp(struct img_priv *priv,
		      int queue);

int rpu_flush_vif_queues(struct img_priv *priv,
			     struct umac_vif *uvif,
			     int chanctx_idx,
			     unsigned int hw_queue_map,
			     enum UMAC_VIF_CHANCTX_TYPE vif_chanctx_type,
			     bool drop);

int rpu_discard_sta_pend_q(struct img_priv *priv,
				   struct umac_vif *uvif,
				   int peer_id,
				   unsigned int hw_queue_map);

int rpu_discard_sta_tx_q(struct img_priv *priv,
				   struct umac_vif *uvif,
				   int peer_id,
				   unsigned int hw_queue_map,
				   int chanctx_idx);
/* Beacon TimeStamp */
__s32 __attribute__((weak)) frc_to_atu(__u32 frccnt, __u64 *patu, s32 dir);
int __attribute__((weak)) get_evt_timer_freq(unsigned int *mask,
						unsigned int *num,
						unsigned int *denom);

int tx_queue_map(int queue);
int tx_queue_unmap(int queue);

extern unsigned char *rf_params_vpd;
extern int num_streams_vpd;

static __always_inline long param_get_val(unsigned char *buf,
			  unsigned char *str,
			  unsigned long *val)
{
	unsigned char *temp;

	if (strstr(buf, str)) {
		temp = strstr(buf, "=") + 1;
		/*To handle the fixed rate 5.5Mbps case*/
		if (!strncmp(temp, "5.5", 3)) {
			*val = 55;
			return 1;
		} else if (!kstrtoul(temp, 0, val)) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

/*parse like: read_fw_reg=0x42000000,0x4 */
static __always_inline long param_get_val2(unsigned char *buf,
			  unsigned char *str,
			  unsigned long *val, unsigned long *val2)
{
	unsigned char *temp, *temp2;

	if (strstr(buf, str)) {
		temp = strstr(buf, "=") + 1;
		temp2 = strstr(buf, ",");
		if (temp2)
			*temp2 = 0;
		if (!kstrtoul(temp, 16, val)) {
			if (temp2 && !kstrtoul(temp2+1, 16, val2)) {
				return 1;
			}
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

static __always_inline long param_get_sval(unsigned char *buf,
			   unsigned char *str,
			   long *val)
{

	unsigned char *temp;

	if (strstr(buf, str)) {
		temp = strstr(buf, "=") + 1;
		/*To handle the fixed rate 5.5Mbps case*/
		if (!strncmp(temp, "5.5", 3)) {
			*val = 55;
			return 1;
		} else if (!kstrtol(temp, 0, val)) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}

}

static __always_inline long param_get_match(unsigned char *buf,
				unsigned char *str)
{

	if (strstr(buf, str))
		return 1;
	else
		return 0;
}

static __always_inline char *get_string_from_rate(int rate,
						   unsigned int flags)
{
	if (rate == -1)
		return "Disabled";


	if (flags & ENABLE_11N_FORMAT)
		return "HT";

	return "Legacy";
}

static __always_inline bool check_valid_rate_flags(struct img_priv *priv,
						   unsigned long val)
{
	bool ret = false;

	do {
		if (val != 8 && val != 0)
			break;
		ret = true;
	} while (0);

	return ret;
}

static __always_inline bool check_valid_data_rate(struct img_priv *priv,
						  int dr,
						  enum ptype type)
{
	bool is_mcs = dr & 0x80;
	bool ret = false;
	unsigned int rate;
	unsigned int nss;

	if (type == UCAST) {
		rate = priv->params->prod_mode_rate_flag;
		nss  = priv->params->num_spatial_streams;
	} else {
		rate = priv->params->mgd_mode_mcast_fixed_rate_flags;
		nss  = priv->params->mgd_mode_mcast_fixed_nss;
	}

	if (dr == -1)
		return true;

	if (is_mcs) {
		dr = dr & 0x7F;
		if (rate & ENABLE_11N_FORMAT) {
			if (nss == 1) {
				if ((dr >= 0) && (dr <= 7))
					ret = true;
				else
					RPU_ERROR_MAIN("Invalid SISO HT MCS: %d\n",
					       dr);
			}
		}

	} else {
		if (priv->params->dot11g_support == 1 &&
		    ((dr == 1) ||
		     (dr == 2) ||
		     (dr == 55) ||
		     (dr == 11))) {
			ret = true;
		} else if ((dr == 6) ||
			   (dr == 9) ||
			   (dr == 12) ||
			   (dr == 18) ||
			   (dr == 24) ||
			   (dr == 36) ||
			   (dr == 48) ||
			   (dr == 54) ||
			   (dr == -1)) {
			ret = true;
		} else
			RPU_ERROR_MAIN("Invalid Legacy Rate value: %d\n", dr);
		if ((rate & ENABLE_11N_FORMAT)
		    ) {
			ret = false;
			RPU_ERROR_MAIN("Invalid rate_flags for legacy: %d\n", dr);
		}
	}
	return ret;
}

static inline int vif_addr_to_index(unsigned char *addr,
				    struct img_priv *priv)
{
	int i;
	struct ieee80211_vif *vif = NULL;

	for (i = 0; i < MAX_VIFS; i++) {
		if (!((i < MAX_VIFS) && (priv->active_vifs & (1 << i))))
			continue;

		rcu_read_lock();
		vif = rcu_dereference(priv->vifs[i]);
		rcu_read_unlock();

		if (ether_addr_equal(addr, vif->addr))
			break;
	}

	if (i < priv->params->num_vifs)
		return i;
	else
		return -1;
}

static inline int ieee80211_is_unicast_robust_mgmt_frame(struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;

	if (skb->len < 24 || is_multicast_ether_addr(hdr->addr1))
		return 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	return ieee80211_is_robust_mgmt_frame(skb);
#else
	return ieee80211_is_robust_mgmt_frame(hdr);
#endif
}
static inline bool is_bufferable_mgmt_frame(struct ieee80211_hdr *hdr)
{
	__u16 fc = hdr->frame_control;
	/*TODO: Handle Individual Probe Response frame in IBSS*/
	if (ieee80211_is_action(fc) ||
		ieee80211_is_disassoc(fc) ||
		ieee80211_is_deauth(fc))
		return	true;

	return false;
}


static inline void img_ether_addr_copy(char *dst, const char *src)
{
	memcpy(dst, src, ETH_ALEN);
}

void set_rf_params(unsigned char *rf_params);
int proc_init(struct proc_dir_entry ***main_dir_entry);
int rpu_init(void);
void rpu_exit(void);
void proc_exit(void);
void update_mcs_packet_stat(int mcs_rate_num,
				   int rate_flags,
				   struct img_priv *priv);
int rpu_proc_tx(struct img_priv *priv, int descriptor_id, int queue);
void rpu_unblock_all_frames(struct img_priv *priv,
					    int ch_id);
int load_rompatch(struct ieee80211_hw *hw);
void stop(struct ieee80211_hw *hw, bool flags);
int start_prod_mode(struct img_priv *priv, unsigned int val);
int stop_prod_mode(struct img_priv *priv, unsigned int val);
int start_prod_rx_mode(struct img_priv *priv, unsigned int val,
					unsigned char *bssid, unsigned char *mac_addr);
int start_prod_echo_mode(struct img_priv *priv, unsigned int val);
int start_packet_gen(struct img_priv *priv, int sval);
int stop_packet_gen(struct img_priv *priv, int sval);
int get_rate_prod(struct cmd_tx_ctrl *txcmd,
		     struct img_priv *priv);
int img_resume(struct ieee80211_hw *hw);
int img_suspend(struct ieee80211_hw *hw,
		       struct cfg80211_wowlan *wowlan);
void init_beacon (struct umac_vif *uvif);
void deinit_beacon (struct umac_vif *uvif);
void modify_beacon_params (struct umac_vif *uvif,
				  struct ieee80211_bss_conf *bss_conf);
void trigger_wifi_power_save(int val);
void trigger_wifi_scan_abort(int if_idx);

extern void read_mem_region(unsigned int,int);
#define RPU_READY_TIMEOUT 200
#define RPU_READY_TIMEOUT_TICKS msecs_to_jiffies(RPU_READY_TIMEOUT)
#define RPU_EVENT_RPU_READY 0xDEAD

#ifdef RPU_SLEEP_ENABLE
#define OUTSTANDING_CMDS_COMPLETE_TIMEOUT 100
#define OUTSTANDING_CMDS_COMPLETE_TIMEOUT_TICKS msecs_to_jiffies(OUTSTANDING_CMDS_COMPLETE_TIMEOUT)

#define RPU_INIT_SLEEP_TIMEOUT 2000
#endif

extern bool rpu_is_cmd_has_data(unsigned char *data);
extern int rpu_send_cmd_datas(unsigned char *data, struct hal_priv *priv);
extern void dump_ieee80211_hdr_info(unsigned char *data, int len, int tx);
extern int rockchip_wifi_mac_addr(unsigned char *buf);
extern int iw_send_hang_event(struct img_priv *priv);

extern void init_roc_timeout_timer (struct img_priv *priv);
extern void start_roc_timeout_timer(struct img_priv *priv, int timeout);
extern void deinit_roc_timeout_timer (struct img_priv *priv);

#endif /* _CORE_H_ */
