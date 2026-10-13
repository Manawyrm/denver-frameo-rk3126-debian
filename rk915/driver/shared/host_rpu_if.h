/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _HOST_RPU_IF_H_
#define _HOST_RPU_IF_H_

#include "hal.h"

#define MCST_ADDR_LIMIT	48
#define WLAN_ADDR_LEN 6
#define TKIP_MIC_LEN 8
#define MICHAEL_LEN 16
#define MAX_KEY_LEN 16
#define MAX_VIFS 2

#define MAX_PEERS 15
/* Additional queue for unicast frames directed to non-associated peers (for
 * e.g. Probe Responses etc)
 */
#define MAX_PEND_Q_PER_AC (MAX_PEERS + MAX_VIFS)


#define WEP40_KEYLEN 5
#define WEP104_KEYLEN 13
#define MAX_WEP_KEY_LEN 13

#define WLAN_20MHZ_OPERATION 0
#define WLAN_40MHZ_OPERATION 1
#define WLAN_80MHZ_OPERATION 2
#define WLAN_SEC_UPPER 0
#define WLAN_SEC_LOWER 1

#define PWRSAVE_STATE_AWAKE 1
#define PWRSAVE_STATE_DOZE 0

/* because rpu do not support multi ssid scan now,
 * we now split multi ssid scan to several single ssid scan
 */
#define ENABLE_SPLIT_MULT_SSID_SCAN

#define MAX_SSID_LEN 32
#ifdef ENABLE_SPLIT_MULT_SSID_SCAN
#define MAX_NUM_SSIDS 4
#else
#define MAX_NUM_SSIDS 1
#endif
#define TOTAL_KEY_LEN 32
#define RX_SEQ_SIZE 6
#define MAX_IE_LEN 100
#define ETH_ALEN 6

#define MAX_TX_CMDS 16
#define MAX_GRAM_PAYLOAD_LEN 52

#define RF_PARAMS_SIZE	369
#define MAX_RF_CALIB_DATA 900


enum rpu_channel_bw {
	RPU_CHAN_WIDTH_20_NOHT,
	RPU_CHAN_WIDTH_20 = RPU_CHAN_WIDTH_20_NOHT,
};
/**
 * enum UMAC_QUEUE_NUM - Queues used to transmit frames.
 * @WLAN_AC_BK: Queue for frames belonging to the "Background" Access Category.
 * @WLAN_AC_BE: Queue for frames belonging to the "Best-Effort" Access Category.
 * @WLAN_AC_VI: Queue for frames belonging to the "Video" Access Category.
 * @WLAN_AC_VO: Queue for frames belonging to the "Voice" Access Category.
 * @WLAN_AC_BCN: Queue for frames belonging to the "Beacon" Access Category.
 * @WLAN_AC_MAX_CNT: Maximum number of transmit queues supported.
 *
 * This enum represents the queues used to segregate the TX frames depending on
 * their QoS categories. A separate queue is used for Beacon frames / frames
 * transmitted during DTIM intervals.
 */
enum UMAC_QUEUE_NUM {
	WLAN_AC_BK = 0,
	WLAN_AC_BE,
	WLAN_AC_VI,
	WLAN_AC_VO,
	WLAN_AC_BCN,
	WLAN_AC_MAX_CNT
};

/**
 * enum RPU_EVENT_ROC_STAT - Status of the Remain-On-Channel(ROC) operation.
 * @UMAC_ROC_STAT_STARTED: ROC has started.
 * @UMAC_ROC_STAT_STOPPED: ROC was stopped.
 * @UMAC_ROC_STAT_DONE: ROC completed.
 * @UMAC_ROC_STAT_ABORTED: ROC was aborted.
 *
 * This enum represents the different states in which a ROC operation can be in.
 * ROC is applicable only for P2P Mode.
 */
enum RPU_EVENT_ROC_STAT {
	UMAC_ROC_STAT_STARTED,
	UMAC_ROC_STAT_STOPPED,
	UMAC_ROC_STAT_DONE,
	UMAC_ROC_STAT_ABORTED
};

/**
 * enum UMAC_VIF_CHANCTX_TYPE - Types of channel contexts.
 * @UMAC_VIF_CHANCTX_TYPE_OPER: Operating channel context.
 * @UMAC_VIF_CHANCTX_TYPE_OFF: Off-channel context.
 * @MAX_UMAC_VIF_CHANCTX_TYPES: Maximum number of channel context types.
 *
 * This enum represents the different types of channel contexts that are
 * possible during a concurrent multichannel operation. At any point of time a
 * channel context can be either of the type "Operating" or "Off-channel"
 */
enum UMAC_VIF_CHANCTX_TYPE {
	UMAC_VIF_CHANCTX_TYPE_OPER,
	UMAC_VIF_CHANCTX_TYPE_OFF,
	MAX_UMAC_VIF_CHANCTX_TYPES
};

/**
 * enum RPU_SLEEP_CONFIG_CHANGED - Flags to indicate what changed in Sleep
				Controller configuration.

 * @PMFLAG_PWR_ON_VALUE_CHANGED - UCC_SLEEP_CTRL_PWR_ON_VALUE 
 * @PMFLAG_PWR_OFF_VALUE_CHANGED - UCC_SLEEP_CTRL_PWR_OFF_VALUE
 * @PMFLAG_RAM_ON_STATE_CHANGED - UCC_SLEEP_CTRL_PWR_OFF_VALUE
 * @PMFLAG_RAM_OFF_STATE_CHANGED - UCC_SLEEP_CTRL_RAM_OFF_STATE
 * @PMFLAG_PWR_ON_TIME_CHANGED - UCC_SLEEP_CTRL_PWR_ON_TIME 
 * @PMFLAG_PWR_OFF_TIME_CHANGED - UCC_SLEEP_CTRL_PWR_OFF_TIME 
 * @PMFLAG_RAM_ON_TIME_CHANGED - UCC_SLEEP_CTRL_RAM_ON_TIME 
 * @PMFLAG_RAM_OFF_TIME_CHANGED - UCC_SLEEP_CTRL_RAM_OFF_TIME 
 * @PMFLAG_SLEEP_FREQ_CHANGED - sleep_timer_freq_hz
 */


enum RPU_SLEEP_CONFIG_CHANGED {
	PMFLAG_PWR_ON_VALUE_CHANGED  =  0x0001,  
	PMFLAG_PWR_OFF_VALUE_CHANGED =  0x0002,  
	PMFLAG_RAM_ON_STATE_CHANGED  =  0x0004,  
	PMFLAG_RAM_OFF_STATE_CHANGED =  0x0008,  
	PMFLAG_PWR_ON_TIME_CHANGED   =  0x0010,  
	PMFLAG_PWR_OFF_TIME_CHANGED  =  0x0020,  
	PMFLAG_RAM_ON_TIME_CHANGED   =  0x0040,  
	PMFLAG_RAM_OFF_TIME_CHANGED  =  0x0080,  
	PMFLAG_SLEEP_FREQ_CHANGED    =  0x0100,   
	PMFLAG_CLK_ADJ_VAL_CHANGED   =	0x0200,
	PMFLAG_WAKEUP_TIME_CHANGED   =	0x8000,
};

/**
 * enum RPU_CMD_TAG - Commands that the Host can send to the Firmware.
 * @RPU_CMD_RESET: Used to reset/initialize the Firmware
 * @RPU_CMD_SCAN: Used to initiate a hardware SCAN operation.
 * @RPU_CMD_SCAN_ABORT: Used to abort a scan request raised previously from
 *                       host.
 * @RPU_CMD_UNUSED1: Unused.
 * @RPU_CMD_SETKEY: Used to program relevant security keys depending on the
 *                   security mode enabled.
 * @RPU_CMD_UNUSED2: Unused.
 * @RPU_CMD_UNUSED3: Unused.
 * @RPU_CMD_TX: Used to transmit a frame (MPDU/A-MPDU).
 * @RPU_CMD_UNUSED4: Unused.
 * @RPU_CMD_UNUSED5: Unused.
 * @RPU_CMD_TX_POWER: Used to specify maximum transmit power.
 * @RPU_CMD_UNUSED6: Unused.
 * @RPU_CMD_UNUSED7: Unused.
 * @RPU_CMD_PS: Used to enable/disable WLAN powersave.
 * @RPU_CMD_PS_ECON_CFG: Used to enable/disable Economy mode.
 * @RPU_CMD_VIF_CTRL: Used to add or remove a virtual interface.
 * @RPU_CMD_UNUSED8: Unused.
 * @RPU_CMD_UNUSED9: Unused.
 * @RPU_CMD_BA_SESSION_INFO: Used to pass Block ACK parameters to firmware.
 * @RPU_CMD_MCST_ADDR_CFG: Used to add or remove a multicast address to the
 *                          firmware.
 * @RPU_CMD_MCST_FLTR_CTRL: Used to enable/disable multicast filtering in the
 *                           firmware.
 * @RPU_CMD_VHT_BEAMFORM_CTRL: Used to pass beamforming related parameters to
 *                              the firmware.
 * @RPU_CMD_ROC_CTRL: Used to pass the Remain-On-Channel(ROC) related
 *                     information to firmware
 * @RPU_CMD_CHANNEL: Used to program a channel to the firmware.
 * @RPU_CMD_VIF_CFG: Used to change configuration parameters for an interface.
 * @RPU_CMD_UNUSED10: Unused.
 * @RPU_CMD_TXQ_PARAMS: Used to set transmission queue parameters.
 * @RPU_CMD_MIB_STATS: Used to get MIB stats.
 * @RPU_CMD_PHY_STATS: Used to get PHY stats.
 * @RPU_CMD_UNUSED11: Unused.
 * @RPU_CMD_AUX_ADC_CHAIN_SEL: Used to set AUX path in the PHY.
 * @RPU_CMD_DETECT_RADAR: Used to start/stop Radar detection operation.
 * @RPU_CMD_UNUSED12: Unused.
 * @RPU_CMD_UNUSED13: Unused.
 * @RPU_CMD_MEASURE: Used to initiate a measurement on a channel.
 * @RPU_CMD_BT_INFO: Used to provide Bluetooth related information to
 *                    the Firmware.
 * @RPU_CMD_CLEAR_STATS: Used to clear all the previous MAC and PHY stats.
 * @RPU_CMD_CHANCTX_TIME_INFO: Used to communicate information about
 *                              percentage occupancy of each channel during
 *                              Concurrent Multichannel operation.
 * @RPU_CMD_CONT_TX: Used in Production mode for continuous transmission tests.
 * @RPU_CMD_TX_DEINIT: Used to deinitialize the transmit path per peer on a
 *                      particular interface (or) per interface during
 *                      disconnection of peer (or) interface de-initialization.
 * @RPU_CMD_RX_CTRL: Used to enable/disable RX path in LMAC (currently unused).
 * @RPU_CMD_CFG_PWRMGMT: Used to configure RPU Sleep Controller.
 *
 * This enum contains the different commands which the Host can use to send to
 * the Firmware to carry out an operation like configuration, executing an
 * action, requesting some information etc.
 */
enum RPU_CMD_TAG {
	RPU_CMD_RESET = 0,
	RPU_CMD_SCAN,
	RPU_CMD_SCAN_ABORT,
	RPU_CMD_UNUSED1,
	RPU_CMD_SETKEY,
	RPU_CMD_UNUSED2,
	RPU_CMD_UNUSED3,
	RPU_CMD_TX,
	RPU_CMD_UNUSED4,
	RPU_CMD_UNUSED5,
	RPU_CMD_TX_POWER,
	RPU_CMD_UNUSED6,
	RPU_CMD_UNUSED7,
	RPU_CMD_PS,
	RPU_CMD_PS_ECON_CFG,
	RPU_CMD_VIF_CTRL,
	RPU_CMD_UNUSED8,
	RPU_CMD_UNUSED9,
	RPU_CMD_BA_SESSION_INFO,
	RPU_CMD_MCST_ADDR_CFG,
	RPU_CMD_MCST_FLTR_CTRL,
	RPU_CMD_UNUSED10,
	RPU_CMD_ROC_CTRL,
	RPU_CMD_CHANNEL,
	RPU_CMD_VIF_CFG,
	RPU_CMD_UNUSED11,
	RPU_CMD_TXQ_PARAMS,
	RPU_CMD_MIB_STATS,
	RPU_CMD_PHY_STATS,
	RPU_CMD_UNUSED12,
	RPU_CMD_UNUSED13,
	RPU_CMD_UNUSED14,
	RPU_CMD_UNUSED15,
	RPU_CMD_UNUSED16,
	RPU_CMD_UNUSED17,
	RPU_CMD_UNUSED18,
	RPU_CMD_CLEAR_STATS,
	RPU_CMD_CONT_TX,
	RPU_CMD_RX_CTRL,
	RPU_CMD_CFG_PWRMGMT,
	RPU_CMD_UPD_PHY_THRESH,
	RPU_CMD_TXRX_TEST,
	RPU_CMD_FW_PRIV_CMD,
	RPU_CMD_SL_WP_CTRL,
	RPU_CMD_READ_CSR,
	RPU_MAX_CMD_NUMBER
};

#define RPU_CMD_PATCH_FEATURES 100
#define RPU_CMD_IP_ADDRESS     101
#define RPU_CMD_MISC_CFG 200

/**
 * enum RPU_EVENT_TAG - Events that the firmware can send to the Host.
 * @RPU_EVENT_RX: Indicates a packet has been received.
 * @RPU_EVENT_TX_DONE: Indicates completion of a TX request.
 * @RPU_EVENT_DISCONNECTED: Indicates a disconnection with the peer
 *                           detected by RPU.
 * @RPU_EVENT_UNUSED1: Unused.
 * @RPU_EVENT_UNUSED2: Unused.
 * @RPU_EVENT_SCAN_COMPLETE: Indicates completion of a scan request.
 * @RPU_EVENT_SCAN_ABORT_COMPLETE: Indicates completion of a scan abort
 *                                  request.
 * @RPU_EVENT_UNUSED3: Unused.
 * @RPU_EVENT_RESET_COMPLETE: Indicates completion of a reset command.
 * @RPU_EVENT_UNUSED4: Unused.
 * @RPU_EVENT_UNUSED5: Unused.
 * @RPU_EVENT_UNUSED6: Unused.
 * @RPU_EVENT_MIB_STAT: Used to return MIB statistics.
 * @RPU_EVENT_PHY_STAT: Used to return PHY statistics.
 * @RPU_EVENT_NW_FOUND: Unused.
 * @RPU_EVENT_NOA: Indicates start/stop of Notice-Of-Absence period by the
 *                  connected P2P-GO.
 * @RPU_EVENT_CTRL_POOL_ACK: Unused.
 * @RPU_EVENT_COMMAND_PROC_DONE: Indicates completion of the processing of a
 *                                command (except RPU_CMD_RESET and
 *                                RPU_CMD_TX).
 * @RPU_EVENT_CH_PROG_DONE: Signals completion of the processing of the channel
 *                           programming command.
 * @RPU_EVENT_PS_ECON_CFG_DONE: Indicates completion of the processing of the
 *                               Economy mode configuration command.
 * @RPU_EVENT_PS_ECON_WAKE: Indicates a wakeup event when in Economy mode.
 * @RPU_EVENT_MAC_STATS: Used to return MAC statistics.
 * @RPU_EVENT_RF_CALIB_DATA: Used to return calibration data to the Host.
 * @RPU_EVENT_RADAR_DETECTED: Indicates Radar detection event.
 * @RPU_EVENT_MSRMNT_COMPLETE: Unused.
 * @RPU_EVENT_ROC_STATUS: Informs Host about status of a Remain-On-Channel
 *                         operation.
 * @RPU_EVENT_CHAN_SWITCH: Informs Host about a channel switch during
 *                          concurrent multichannel operation.
 * @RPU_EVENT_FW_ERROR: Firmware error.
 * @RPU_EVENT_TX_DEINIT_DONE: Informs Host about the status of a TX
 *                             deinitialization operation initiated by the Host.
 *
 * This enum represents the different types of events that the Firmware can
 * return to the Host. The events are used to communicate information like
 * status of command processing issue by the Host, return various statistics, or
 * some asynchronous events that the Host might be interested in.
 */
enum RPU_EVENT_TAG {
	RPU_EVENT_RX = 0,
	RPU_EVENT_TX_DONE,
	RPU_EVENT_DISCONNECTED,
	RPU_EVENT_UNUSED1,
	RPU_EVENT_UNUSED2,
	RPU_EVENT_SCAN_COMPLETE,
	RPU_EVENT_SCAN_ABORT_COMPLETE,
	RPU_EVENT_UNUSED3,
	RPU_EVENT_RESET_COMPLETE,
	RPU_EVENT_UNUSED4,
	RPU_EVENT_UNUSED5,
	RPU_EVENT_UNUSED6,
	RPU_EVENT_MIB_STAT,
	RPU_EVENT_PHY_STAT,
	RPU_EVENT_NW_FOUND,
	RPU_EVENT_NOA,
	RPU_EVENT_CTRL_POOL_ACK,
	RPU_EVENT_COMMAND_PROC_DONE,
	RPU_EVENT_CH_PROG_DONE,
	RPU_EVENT_PS_ECON_CFG_DONE,
	RPU_EVENT_PS_ECON_WAKE,
	RPU_EVENT_MAC_STATS,
	RPU_EVENT_UNUSED7,
	RPU_EVENT_UNUSED8,
#ifndef RK915	
	RPU_EVENT_MSRMNT_COMPLETE,
#endif
	RPU_EVENT_ROC_STATUS,
	RPU_EVENT_FW_ERROR,
	RPU_EVENT_BLOCK_ALL,
	RPU_EVENT_UNBLOCK_ALL,
	RPU_EVENT_FW_PRIV_CMD_DONE,
	RPU_EVENT_TXRX_TEST,
	RPU_EVENT_RX_SERIAS,
	RPU_EVENT_AIRKISS_STATUS,
	RPU_EVENT_READ_CSR_CMP,
	RPU_MAX_EVENT_NUMBER
};

/**
 * enum UMAC_TX_FLAGS - Flags to specify additional qualifiers for a transmit
 *                      frame.
 * @UMAC_TX_FLAG_OFFCHAN_FRM: Frame to transmitted on an Off-Channel.
 *
 * This enum represents the different qualifiers that can be specified for
 * transmit frames.
 */
enum UMAC_TX_FLAGS {
	UMAC_TX_FLAG_OFFCHAN_FRM
};

/**
 * enum UMAC_PS_ECON_WAKE_TRIG - Triggers which can cause a wake-up during
 *                               Economy mode operation.
 * @TRIG_PKT_RCV: Wake-up due to frame reception.
 * @TRIG_DISCONNECT: Wake-up due to a disconnection event.
 *
 * This enum represents the different triggers that can cause the wake-up after
 * a sleep during Economy mode operation.
 */
enum UMAC_PS_ECON_WAKE_TRIG {
	TRIG_PKT_RCV,
	TRIG_DISCONNECT
};

/**
 * enum POWER_SAVE_TAG - Options to enable/disable WLAN Power-Save.
 * @AWAKE: Disable Power-Save.
 * @SLEEP: Enable Power-Save.
 *
 * This enum represents the values that can be used to specify whether to
 * enable/disable WLAN power-save.
 */
enum POWER_SAVE_TAG {
	AWAKE = 0,
	SLEEP
};

/**
 * enum SCAN_TYPE_TAG - Different types of scanning operations.
 * @PASSIVE: Passive scan.
 * @ACTIVE: Active scan.
 *
 * This enum represents the different types of scanning operations.
 */
enum SCAN_TYPE_TAG {
	PASSIVE = 0,
	ACTIVE
};

/**
 * struct hal_data - Hardware Abstraction Layer's (HAL) private data.
 * @hal_data: This contains 8 bytes of information.
 *
 * Out of the 8 bytes of information contained in this structure the first 4
 * bytes differentiate between a HAL command and UMAC command. The next 4 bytes
 * are reserved for future use.
 */
struct hal_data {
	unsigned char hal_data[HAL_PRIV_DATA_SIZE];
} __IMG_PKD;

/**
 * struct host_rpu_msg_hdr - Header to be appended to all the Commands/Events
 *                           between Host and Firmware.
 * @hal_data: Hardware Abstraction Layer's (HAL) private data.
 * @descriptor_id: The 2 LSB bytes are TX pool id, the 2 MSB bytes are queue
 *                 number. Pool id of 0xffff indicates no TX payload.
 * @payload_length: Payload length excluding MAC header bytes.
 * @id: Host-FW Command/Event ID.
 * @length: Size of Command/Event, applicable only for TX/RX.
 *          For TX this is sizeof(cmd_tx_ctrl) and for RX this is
 *          sizeof(wlan_rx_pkt).
 * @more_cmd_data: The meaning of this bit depends on direction TX/RX
 * 		   a) TX: it is used for commands whose size is greater than
 *                    MAX_CMD_SIZE. (Set to 1, if command is split into two or
 *                    more. Set to 0 otherwise).
 *		   b) RX: it is used to indicated whether the incoming data is
 *                    Qos/Non-qos, accordingly the data is processed.
 *                    Needed for alignment of skb.
 *
 * This header contains control information about a Command/Event between Host
 * and Firmware. This must be present in every Command/Event.
 */
struct host_rpu_msg_hdr {
	struct hal_data hal_data;
	unsigned int descriptor_id; /* LSB 2 bytes as pool id, MSB 2 bytes
				     * queue num, pool ID of 0xFFFF indicates
				     * no payload
				     */
	unsigned int payload_length;
	unsigned int id;
	unsigned int length;
	unsigned int more_cmd_data;
} __IMG_PKD;

/**
 * struct bgscan_params - Background Scan parameters.
 * @enabled: Enable/Disable background scan.
 * @channel_list: List of channels to scan.
 * @channel_flags: Channel flags for each of the channels which are to be
 *                 scanned.
 * @scan_intval: Back ground scan is done at regular intervals. This
 *               value is set to the interval value (in ms).
 * @channel_dur: Time to be spent on each channel (in ms).
 * @serv_channel_dur: In "Connected State" scanning, we need to share the time
 *                    between operating channel and non-operating channels.
 *                    After scanning each channel, the firmware spends
 *                    "serv_channel_dur" (in ms) on the operating channel.
 * @num_channels: Number of channels to be scanned.
 *
 * This structure specifies the parameters which will be used during a
 * Background Scan.
 */
struct bgscan_params {
	unsigned int enabled;
	unsigned char channel_list[50];
	unsigned char channel_flags[50];
	unsigned int scan_intval;
	unsigned int channel_dur;
	unsigned int serv_channel_dur;
	unsigned int num_channels;
} __IMG_PKD;

/**
 * struct ssid - Structure to describe an SSID.
 * @len: Length of the SSID.
 * @ssid: SSID character buffer; according to IEEE it is of size varying
 *        from 1- 32.
 *
 * This structure is used to specify an Service Set Identifier (SSID).
 */
struct ssid {
	unsigned int len;
	unsigned char ssid[MAX_SSID_LEN];
} __IMG_PKD;



/* Commands */

/**
  * struct cmd_txrx_test - Command used for tx rx stability test.
  */
struct cmd_txrx_test {
	struct host_rpu_msg_hdr hdr;
#define TXRX_TEST_START_TX 0
#define TXRX_TEST_TX 1
#define TXRX_TEST_START_RX 2
	unsigned int status;
} __IMG_PKD;

struct fw_reg_info {
	unsigned int reg;
	unsigned int val;
	unsigned int len;
#define READ_REG 0
#define WRITE_REG 1
#define READ_REG_LPW 2
#define WRITE_REG_LPW 3
	unsigned int rw;
} __IMG_PKD;

enum FW_PARAM_SEQ {
	PARAM_ECHO_MODE = 0,
	PARAM_EJTAG_MODE,
	PARAM_DEBUG_LEVEL,
	PARAM_DEBUG_FLAG,
	PARAM_DIS_WIFI_ISR_THD,
	PARAM_EN_WIFI_ISR_THD,
};

struct fw_params {
	unsigned int mask;
	unsigned char echo_mode;
	unsigned char ejtag_mode;
	unsigned char debug_level;
	unsigned char debug_flag;
	unsigned char dis_wifi_isr_thd;
	unsigned char en_wifi_isr_thd;
} __attribute__ ((__packed__));

enum fw_priv_cmd_type{
	DUMP_TXRX_COUNT_INFO = 1,
	DUMP_TXRX_BUF_INFO,
	DUMP_REG_INFO,
	DUMP_FW_LOG,
	DUMP_TXRX_QUEUE_INFO,
	FW_PRIV_INIT,
	DUMP_FW_VERSION,
	FW_SET_PARAMS,
	FW_GET_PARAMS,
	DUMP_MEM_INFO,
	DUMP_IF_INFO,
	ADC_CAPTURE,
	DUMP_ADC_CAPTURE_DATA,
	ENABLE_SNIFFER,
	FORCE_ASSERT,
	DUMP_RF_CAL_DATA,
	ENABLE_EJTAG = 100,
};

/**
  * struct fw_priv_cmd - Command used for fw info dump.
  */
struct fw_priv_cmd {
	struct host_rpu_msg_hdr hdr;
#define DUMP_FW_CRASH_INFO			0x70616E63
	unsigned int type;
	struct fw_reg_info reg_info;
	int production_test;
	int fw_skip_rx_pkt_submit;
	struct fw_params params;
	/*
	 * Value = 0 --> normal operation 
	 * Value > 0 --> sniffer operation 
	 * Value = 1 --> receive all data and managment frames - both unicast and broadcast 
	 * Value = 2 --> receive broadcast data frames only 
	 * Value = 3 --> receive broadcast and unciast data only 
	 */
	 /* sniffer & 0x000F ->  Sniffer mode specified below
	     sniffer & 0x00F0 ->  Sniffer time interval when no broadcast info received(time unit 50ms)
	     sniffer & 0x0F00 ->  Sniffer timer interval when broadcast info received(time unit 50ms)
	  */
	int sniffer;
	/*
	 * first byte of wlan0 and p2p mac address
	 */
	unsigned char wlan_mac_addr;
	unsigned char p2p_mac_addr;
} __IMG_PKD;

/**
  * struct cmd_set_phy_thresh - set phy threshholds
  */
struct cmd_update_phy_thresh {
	struct host_rpu_msg_hdr hdr;
	unsigned char tx_boost;
	unsigned char ssd_enable;
	unsigned char spare[2];
	unsigned char thresholds[14];
} __IMG_PKD;

/**
 * struct cmd_tx_ctrl - Command used by Host to program TX control information
 *                      for 1 (A)MPDU to the Firmware for transmission.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_TX).
 * @if_index: Virtual interface number this packet is tied to.
 * @queue_num: The access category to which the frame belongs.
 * @descriptor_id: Token ID of the transmitted frame.
 * @num_frames_per_desc: Number of MPDU's to be sent in this command.
 * @pkt_length: Packet length(s) of each MPDU being sent as part of this
 *              command.
 * @more_frms: Used in AP mode operation; while transmitting a packet to a
 *             station which is in the power save mode, if the AP has more
 *             data in destination's station queue, this field is set to 1;
 *             0 in all other cases.
 * @force_tx: If this field is set for any packet, it needs to be transmitted
 *            even though TX has been disabled. This field is only meaningful
 *            in DFS.
 * @tx_flags: Flags to communicate special cases regarding the frame to the
 *            Firmware
 * @num_rates: Used to specify the number of possible rates a TX packet can
 *             be transmitted at (Max 4 possible rates).
 * @rate_protection_type: Per rate protection flags (RTS/CTS, CTS2SELF) set by
 *                        the rate control algorithm (or) /proc interface.
 * @rate_preamble_type: Per rate usage of short preamble as dictated by the rate
 *                      control algorithm (or) /proc interface.
 * @rate_retries: Number of times a packet should be transmitted at each
 *                possible rate.  Ex {(1, 3),( 5.5, 2), (36,1) }, this
 *                example is a set of (rate, rate_retries) couple with
 *                num_rates as 3. This means, transmit a TX packet with 3
 *                possible rates 1, 5.5 and  36. If the first rate is not
 *                successful after trying 3 times, try the second rate twice.
 *                If it is still failing it will try to transmit at rate 36
 *                once.
 * @rate: The rate value(s), at which the packet transmission needs to be
 *        attempted as dictated by the rate control algorithm (or) /proc
 *        interface.
 * 	  If the most significant bit is one it's a 11n rate.
 * @rate_flags: Per rate flags as dictated by the rate control algorithm
 *              (or) /proc interface.
 * @num_spatial_streams: Number of spatial streams to be used per rate as
 *                       dictated by the rate control algorithm (or)
 *                       /proc interface.
 * @stbc_enabled: STBC enabled/disabled as dictated by the rate control
 *                algorithm (or) /proc interface.
 * @bcc_or_ldpc: FEC type as dictated by the rate control algorithm (or)
 *               /proc interface.
 * @aggregate_mpdu: In 11n, this field informs firmware whether to club
 *                  the packet with other packets or not. For management,
 *                  control and broadcast frames this field is not set.
 *                  Currently if num_frames > 1, then this is set by default.
 * @encrypt: If the frame is to be encrypted by RPU this is
 *           set to 1 else this is 0.
 * @config_mac_hdr_len: Length of the MAC header of the first MPDU in an AMPDU.
 * @frame_source: Is the frame residing in GRAM or Ext-RAM. For all packets from
 *                host Ext-ram is default. GRAM is purely for testing purposes.
 * @config_mac_header: Complete MAC header of the first MPDU in an AMPDU.
 * @frame_ddr_pointer: Starting Physical address of each frame in Ext-RAM
 *                     after dma_mapping.
 * @per_pkt_crypto_params: Crypto parameters (sequence control, QoS control, IV)
 *                         per frame. Incremental to the MAC header sent above.
 * @gram_payload: MAC headers of all the frames transmitted using this
 *                command. (unused currently)
 *
 * This command is used to transmit a TX packet. A TX packet is divided into
 * two parts:
 *
 * First part consists of TX control information, this also has
 * complete 802.11 header of first packet (config_mac_header) and incremental
 * information (per_pkt_crypto_params) per frame. This will be used to configure
 * different blocks in RPU.
 *
 * Second part is 802.11 header and payload. This will directly streamed to
 * PHY for transmission using DMA controller within RPU.
 *
 * TX done event is generated after completing packet transmission.
 */
struct cmd_tx_ctrl {
	struct host_rpu_msg_hdr hdr;

	/* VIF number this packet belongs to */
	unsigned char if_index;

	/* Queue no will be VO, VI, BE, BK and BCN */
	unsigned char queue_num;

	unsigned int descriptor_id;

	/* number of frames in tx descriptors */
	unsigned int num_frames_per_desc;

	/*packet lengths of frames*/
	unsigned int pkt_length[MAX_TX_CMDS];

	/* If more number of frames buffered at UMAC */
	unsigned char more_frms;

	/* If this field is set for any packet,
	 * need to be transmit even though TX has been disabled
	 */
	unsigned int force_tx;

	/* Flags to communicate special cases regarding the frame to the FW */
	unsigned int tx_flags;

	unsigned char num_rates;

#define USE_PROTECTION_NONE 0
#define USE_PROTECTION_RTS 1
#define USE_PROTECTION_CTS2SELF 2
	unsigned char rate_protection_type[4];

#define USE_SHORT_PREAMBLE 0
#define DONT_USE_SHORT_PREAMBLE 1
	unsigned char rate_preamble_type[4];

	unsigned char rate_retries[4];

#define MARK_RATE_AS_MCS_INDEX 0x80
#define MARK_RATE_AS_RATE 0x00
	unsigned char rate[4];

#define ENABLE_SGI 0x04

	unsigned char rate_flags[4];
	unsigned char num_spatial_streams[4];
	unsigned char stbc_enabled;
	unsigned char bcc_or_ldpc;

#define AMPDU_AGGR_ENABLED 0x00000001
#define AMPDU_AGGR_DISABLED 0x00000000
	unsigned char aggregate_mpdu;

#define ENCRYPT_DISABLE 0
#define ENCRYPT_ENABLE 1
	unsigned char encrypt;
	unsigned int config_mac_hdr_len;
#define PKT_SRC_GRAM 1
#define PKT_SRC_EXTRAM 0
	unsigned char frame_source;
#define MAC_HDR_SIZE 52
	unsigned char config_mac_header[MAC_HDR_SIZE];
	unsigned int frame_ddr_pointer[MAX_TX_CMDS];
#define PER_PKT_CRYPTO_PARAMS_SIZE 12
#define PER_PKT_CRYPTO_PARAMS_SEQ_CTRL_OFFSET 0
#define PER_PKT_CRYPTO_PARAMS_QOS_CTRL_OFFSET 2
#define PER_PKT_CRYPTO_PARAMS_IV_OFFSET 4

	unsigned char per_pkt_crypto_params[MAX_TX_CMDS]
					   [PER_PKT_CRYPTO_PARAMS_SIZE];
#ifdef TX_SG_MODE
	unsigned char pad[59];
#endif
	unsigned int *p_frame_ddr_pointer[MAX_TX_CMDS];
} __IMG_PKD;

/**
 * struct cmd_reset - Command used to (De-)initialize the Firmware.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_RESET).
 * @type: Enable/Disable the firmware.
 * @unused1: Unused.
 * @unused2: Unused.
 * @rf_params: RF calibration data derived using FTM.
 * @unused3: Unused.
 * @bg_scan: Background Scan parameters.
 * @num_spatial_streams: Number of spatial streams.
 * @system_rev: Unused.
 * @lmac_mode: Normal/FTM mode.
 * @antenna_sel: Antenna configuration, applicable only for 1x1.
 *
 * This command is used to enable/disable the WLAN. A RESET_COMPLETE
 * event from the firmware indicates successful completion of this command.
 * After sending the RESET, the host should not issue further commands until a
 * RESET_COMPLETE is received.
 */
struct cmd_reset {
	struct host_rpu_msg_hdr hdr;
#define LMAC_ENABLE 0
#define LMAC_DISABLE 1
#define LMAC_DO_CALIB 0x0010 /* do RF calibration */
#define LMAC_NO_SLEEP   0x0020  /* LMAC will never sleep */
#define LOAD_FACTORY_CAL 0x0040
	unsigned int type;
        int unused1;
        unsigned int unused2;
	unsigned char rf_params[RF_PARAMS_SIZE];
        unsigned int unused3;
	struct bgscan_params bg_scan;
	unsigned char num_spatial_streams;
	unsigned int system_rev;
#define LMAC_MODE_NORMAL 0
#define LMAC_MODE_FTM 1
	unsigned int lmac_mode;
	unsigned int antenna_sel;
	unsigned int reserv[2];
} __IMG_PKD;

/**
 * struct cmd_scan - Command used to initiate a scan.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_SCAN).
 * @if_index: Interface on which the scan is to be initiated.
 * @type: Passive/Active scan.
 * @n_channel: Total number of channels to scan; channel numbers will be
 *             informed in the channel_list array. If n_channel value is 0, the
 *             firmware scans all possible channels.
 * @n_ssids: Number of SSIDs to scan; ssid information will be in the ssids
 *           array. This should always be >= 1. In case of wild card SSID, this
 *           value is 1 and the ssid_len of the first entry in the SSID list
 *           should be specified as 0.
 * @channel_list: List of channel numbers to scan. List can include from
 *                channel number can be either 2.4GHz or 5GHz
 * @chan_max_power: The maximum power limitation that can be used to send a
 *                  packet in a specific channel from the channel_list array.
 * @chan_flags: The scan type that can be used in a specific channel from the
 *             channel_list array.
 * @ssids: SSID list to scan.
 * @p2p_probe: Set in case scan command is issued as part of a P2P search
 *             operation. Unset for normal operation.
 * @extra_ies_len: If there are any extra information elements that need to be
 *                 part of probe request frames, extra_ies_len should be set to
 *                 the length of those IEs.
 * @extra_ies: As extra IEs can vary in size they are placed at the end of scan
 *             command payload. extra_ies_len should be set to total length of
 *             IEs contained in this field. Allocated memory should be of size
 *             sizeof (Cmd_scan_t) + extra_ies_len.
 *
 * This command is used to initiate a scan. The frames received during scan
 * are sent to UMAC (beacons (or) probe responses).
 *
 * RPU_EVENT_SCAN_COMPLETE event marks end of SCAN.
 */
struct cmd_scan {
	struct host_rpu_msg_hdr hdr;
	unsigned int if_index;
	enum SCAN_TYPE_TAG type;

	/* Total number of channels to scan; channel numbers will be
	 * informed in channel array. if n_channel value is zero,
	 * UMAC scans all possible channels.
	 */
	unsigned int n_channel;

	/* Number of SSIDs to scan; ssid information will be in ssid array.
	 * This is always >= 1. In case of wild card SSID, this value is 1 and
	 * the ssid_len of the first entry in the SSID list should be specifie
	 * as 0
	 */
	unsigned int n_ssids;
	unsigned char channel_list[50];
	unsigned char chan_max_power[50];
	unsigned char chan_flags[50];
#ifdef ENABLE_SPLIT_MULT_SSID_SCAN	
	struct ssid ssids[1];
#else
	struct ssid ssids[MAX_NUM_SSIDS];
#endif
	unsigned int p2p_probe;
	unsigned int extra_ies_len;
	unsigned char extra_ies[0];
} __IMG_PKD;

/**
 * struct cmd_scan_abort - Command used to abort a scan.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_SCAN_ABORT).
 * @if_index: Interface on which the scan is to be aborted.
 *
 * This command is used to abort a scan.
 */
struct cmd_scan_abort {
	struct host_rpu_msg_hdr hdr;
	unsigned int if_index;
} __IMG_PKD;

/**
 * struct cmd_setkey - Command used to set a security key.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_SETKEY).
 * @if_index: Interface on which the key is to be set.
 * @ctrl: Add/Delete the key.
 * @key_type: Unicast/Broadcast key.
 * @cipher_type: The cipher type for which the key is to be set.
 * @key_id: Key index. Values (0-3) used for WEP; for other cases it can be any
 *          value starting from zero.
 * @key_len: Length of the key; value depends on security mechanism chosen.
 * @rsc_len: Length of the Receive Sequence Counter value.
 * @mac_addr: Peer station address for the respective key. In case of a group
 *            key, it is set to a broadcast address, i.e. all 6 bytes are set
 *            to 0xFF.
 * @key: Encryption/Decryption key for Unicast/Multicast packets. Depends on
 *       the key type.
 * @rsc: Receive sequence count value.
 *
 * This command is used to program Pairwise/Group keys when
 * corresponding security modes are enabled.
 */
struct cmd_setkey {
	struct host_rpu_msg_hdr hdr;
	unsigned int if_index;
#define KEY_CTRL_ADD 0
#define KEY_CTRL_DEL 1
	unsigned int ctrl;
#define KEY_TYPE_UCAST 0
#define KEY_TYPE_BCAST 1
	unsigned int key_type;

#define CIPHER_TYPE_WEP40 0
#define CIPHER_TYPE_WEP104 1
#define CIPHER_TYPE_TKIP 2
#define CIPHER_TYPE_CCMP 3
#define CIPHER_TYPE_WAPI 4
	unsigned int cipher_type;
	unsigned int key_id;
	int key_len;
	int rsc_len;
	unsigned char mac_addr[ETH_ALEN];
	unsigned char key[TOTAL_KEY_LEN];
	unsigned char rsc[RX_SEQ_SIZE];
} __IMG_PKD;

/**
 * struct cmd_tx_pwr - Command used to specify maximum transmit power.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_TX_POWER).
 * @if_index: Interface on which the maximum transmit power is to be set.
 * @tx_pwr: Maximum tranmit power value(in dBm).
 *
 * This command is used to specify the maximum transmit power on an interface.
 * It takes values from 0-20 (in dBm) in the system level and any value in
 * production tests.
 */
struct cmd_tx_pwr {
	struct host_rpu_msg_hdr hdr;
	unsigned int if_index;
	int tx_pwr;
} __IMG_PKD;

struct cmd_cfg_misc {
    struct host_rpu_msg_hdr hdr;
#define RPU_MISC_CFG_SNIFF_MODE_MASK 0x1	
    unsigned int flags;  // set RPU_MISC_CFG_SNIFF_MODE_MASK bit
#define RPU_MISC_CFG_SNIFF_MODE_NONE   0x0
#define RPU_MISC_CFG_SNIFF_MODE_ALL    0x1
#define RPU_MISC_CFG_SNIFF_MODE_BCAST  0x2
#define RPU_MISC_CFG_SNIFF_MODE_BUCAST 0x3    
    unsigned int sniff_mode; // set RPU_MISC_CFG_SNIFF_MODE_NONE/ALL/BCAST/BUCAST
} __IMG_PKD;

/**
 * struct cmd_mcst_addr_cfg - Command used to add or remove a multicast address
 *                            to the firmware.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_MCST_ADDR_CFG).
 * @op: 0 to add a multicast address and 1 to remove it.
 * @mac_addr: The multicast address to be added or removed.
 *
 * This command is used to add/remove a multicast address to the firmware. Used
 * in conjunction wth the cmd_mcst_filter_ctrl for multicast filtering.
 */
struct cmd_mcst_addr_cfg {
	struct host_rpu_msg_hdr hdr;
	/* mcst_ctrl -
	 * 0 -- ADD multicast address
	 * 1 -- Remove multicast address
	 */
#define WLAN_MCAST_ADDR_ADD 0
#define WLAN_MCAST_ADDR_REM 1
	unsigned int op;
	/* addr to add or delete..
	 */
	unsigned char mac_addr[6];
} __IMG_PKD;

/**
 * struct cmd_mcst_filter_ctrl - Command used to enable/disable multicast
 *                               filtering in the firmware.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_MCST_FLTR_CTRL).
 * @ctrl: 0 to disable multicast filtering and 1 to enable it.
 *
 * This command is used to enable/disable multicast filtering in firmware. If
 * multicast filtering is enabled, then multicast packets with MAC addresses
 * added using RPU_CMD_MCST_ADDR_CFG are only allowed. If disabled all
 * multicast packets are received by the host.
 */
struct cmd_mcst_filter_ctrl {
	struct host_rpu_msg_hdr hdr;
	/* ctrl -
	 * 0 - disable multicast filtering in LMAC
	 * 1 - enable multicast filtering in LMAC
	 */
#define MCAST_FILTER_DISABLE 0
#define MCAST_FILTER_ENABLE 1
	unsigned int ctrl;
} __IMG_PKD;


/**
 * struct cmd_roc - Command used to pass the Remain-On-Channel(ROC) related
 *                  information to firmware.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_ROC_CTRL).
 * @roc_ctrl: Start/Stop ROC
 * @roc_channel: The channel to be on during the ROC period.
 * @roc_duration: Time to remain on channel during ROC.
 * @roc_type: Normal/Offchannel-TX ROC.
 *
 * This command is used to start/stop a Remain-On-Channel(ROC) period. This is
 * also used to define whether this is a normal or a offchannel-Tx type of ROC.
 * ROC is applicable only for P2P Mode.
 */
struct cmd_roc {
	struct host_rpu_msg_hdr hdr;
#define ROC_STOP 0
#define ROC_START 1
	unsigned int roc_ctrl;
	unsigned int roc_channel;
	unsigned int roc_duration;
#define ROC_TYPE_NORMAL 0
#define ROC_TYPE_OFFCHANNEL_TX 1
	unsigned int roc_type;
} __IMG_PKD;

/**
 * struct cmd_ps - Command used to enable/disable WLAN power-save or Economy
 *                 mode on an interface.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_PS/RPU_CMD_PS_ECON_CFG).
 * @if_index: Interface index on which to configure the power-save.
 * @mode: Enable/Disable power-save.
 *
 * This command is used to enable/disable wlan power-save on an interface.
 * This command is only applicable for STA mode and will be ignored when not
 * associated when it is not in an associated state.
 */
struct cmd_ps {
	struct host_rpu_msg_hdr hdr;
	unsigned int if_index;
	enum POWER_SAVE_TAG mode;
} __IMG_PKD;

/**
 * struct cmd_vifctrl - Command used to add or remove a virtual interface.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_VIF_CTRL).
 * @if_ctrl: Add/Remove interface.
 * @if_index: Index for the interface to be added/removed.
 * @mode: Operating mode of the interface.
 * @mac_addr: MAC address for the interface.
 *
 * This command is used to add/remove a virtual interface. The number of
 * interfaces supported is limited to MAX_VIFS.
 */
struct cmd_vifctrl {
	struct host_rpu_msg_hdr hdr;
	/* if_ctrl -
	 * 0 - add interface address
	 * 1 - remove interface address
	 */
#define IF_ADD 1
#define IF_REM 2

	unsigned int if_ctrl;
	unsigned int if_index;
	/* Interface mode -
	 * 0 - STA in infrastucture mode
	 * 1 - STA in AD-HOC mode
	 * 2 - AP
	 */
#define IF_MODE_STA_BSS 0
#define IF_MODE_STA_IBSS 1
#define IF_MODE_AP 2
#define IF_MODE_INVALID 3

	unsigned int mode;
	unsigned char mac_addr[ETH_ALEN];
} __IMG_PKD;

/**
 * struct cmd_ht_ba - Command used to pass Block ACK parameters to firmware.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_BA_SESSION_INFO).
 * @if_index: Index for the interface on which the BA parameters are to be
 *            set.
 * @op: BA session start/stop.
 * @tid: BA session traffic identifier.
 * @ssn: BA session startin sequence number.
 * @policy: BA policy (0 - Immediate BA, 1 - Delayed BA).
 * @vif_addr: MAC address of the virtual interface.
 * @peer_addr: MAC address of the peer with whom the BA session is to be
 *             established.
 *
 * This command is used to pass Block ACK parameters to the firmware. This is
 * needed during the setting up of a RX A-MPDU aggregation session.
 */
struct cmd_ht_ba {
	struct host_rpu_msg_hdr hdr;
	unsigned int if_index;
#define BLOCK_ACK_SESSION_STOP 0
#define BLOCK_ACK_SESSION_START 1
	unsigned int op;
	unsigned int tid;
	unsigned int ssn;
	unsigned int policy;
	unsigned char vif_addr[ETH_ALEN];
	unsigned char peer_addr[ETH_ALEN];
} __IMG_PKD;

/**
 * struct cmd_channel - Command used to program a channel to the firmware.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_CHANNEL).
 * @channel_bw: Bandwidth of the channel being configured (20/40/80/160 MHz).
 * @primary_ch_number: Center frequency of the primary channel.
 * @channel_number1: Center frequency of the total band, if total band is
 *                   contiguous. For non-contiguous bands, this is the center
 *                   frequency of the first band.
 * @channel_number2: Center frequency of the secondary band. Valid in an 80+80
 *                   band, to be set to 0 for other cases.
 * @freq_band: The band (2.4/5 GHz) to which the channel belongs.
 * @vif_index: Index of the virtual interface on which channel is to be set.
 *
 * This command is used to program a channel to the firmware. An
 * RPU_EVENT_CH_PROG_DONE event from the firmware indicates completion of
 * the channel programming.
 */
struct cmd_channel {
	struct host_rpu_msg_hdr hdr;
	enum rpu_channel_bw channel_bw;
	unsigned int primary_ch_number;
	/* Center frequency of total band, if total band is contiguous.
	 * First band center frequency for non contiguous bands,
	 */
	unsigned int channel_number1;
	/* center frequecny of secondary band.
	 * This is valid in 80+80 band set to zero for other cases
	 */
	unsigned int channel_number2;
	/* 0 - 2.4ghz
	 * 1 - 5ghz
	 */
	unsigned int freq_band;
} __IMG_PKD;

/**
 * struct cmd_vif_cfg - Command used to change configuration parameters for an
 *                      interface.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_VIF_CFG).
 * @changed_bitmap: Bitmap of the parameters that have changed.
 * @basic_rate_set: Bitmap of supported basic rates.
 * @use_short_slot: Set to use short slot, unset to use long slot.
 * @atim_window: Adhoc Traffic Indication Map window.
 * @aid: Association ID, useful in recognizing the buffered packets at AP when
 *       station is in power save.
 * @capability: The latest capability information to be programmed to firmware,
 *              when capabilities have changed.
 *
 * @short_retry: Number of retries to be tried for "short" frames (used only
 *               for the packets initiated by LMAC).
 * @long_retry: Number of retries to be tried for "long" frames (used only
 *               for the packets initiated by LMAC).
 * @bcn_mode: Beacon filtering mode.
 * @dtim_period: This value is in terms of multiple of beacon intervals.
 *               A station in power save mode should wake up from sleep and
 *               listen to DTIM beacons.
 * @beacon_interval: This field informs the beacon sending time(in ms).
 *                   Useful in AP /IBSS mode
 * @if_index: Index of the interface on which the parameters are to be
 *            configured.
 * @vif_addr: MAC address of the interface.
 * @bssid: BSSID of the network.
 * @smps_info: Used to configure SMPS information (SMPS enabled/SMPS mode)
 * @connect_state: Connection status (connected/disconnected) of the interface.
 * @op_channel: Operating channel of the interface.
 *
 * This Command used to program configuration parameters like basic rate set,
 * power save mode etc. in the firmware, for an interface. This command is
 * issued by the Host driver, whenever configuration parameters are changed.
 */
struct cmd_vif_cfg {
	struct host_rpu_msg_hdr hdr;

	/* Bitmap indicating whether value is changed or not */
#define BASICRATES_CHANGED (1<<0)
#define SHORTSLOT_CHANGED (1<<1)
#define POWERSAVE_CHANGED (1<<2) /* to be removed */
#define UAPSDTYPE_CHANGED (1<<3) /* to be removed */
#define ATIMWINDOW_CHANGED (1<<4)
#define AID_CHANGED (1<<5)
#define CAPABILITY_CHANGED (1<<6)
#define SHORTRETRY_CHANGED (1<<7)
#define LONGRETRY_CHANGED (1<<8)
#define BSSID_CHANGED (1<<9)
#define RCV_BCN_MODE_CHANGED (1<<10)
#define BCN_INT_CHANGED (1<<11)
#define DTIM_PERIOD_CHANGED (1<<12)
#define SMPS_CHANGED (1<<13)
#define CONNECT_STATE_CHANGED (1<<14)
#define OP_CHAN_CHANGED (1<<15)

	unsigned int changed_bitmap;

	/* bitmap of supported basic rates
	 */
	unsigned int basic_rate_set;

	/* slot type -
	 * 0 - long slot
	 * 1 - short slot
	 */
	unsigned int use_short_slot;

	/* ATIM window */
	unsigned int atim_window;

	unsigned int aid;

	unsigned int capability;

	unsigned int short_retry;

	unsigned int long_retry;

#define RCV_ALL_BCNS 0
#define RCV_ALL_NETWORK_ONLY 1
#define RCV_NO_BCNS 2

	unsigned int bcn_mode;

	unsigned char dtim_period;

	unsigned int beacon_interval;

	/* index of the intended interface */
	unsigned int if_index;
	unsigned char vif_addr[ETH_ALEN];

	/* bssid of interface */
	unsigned char bssid[ETH_ALEN];

	/* SMPS Info
	 *
	 * bit0 - 0 - Disabled, 1 - Enabled
	 * bit1 - 0 - Static,   1 - Dynamic
	 *
	 */
#define SMPS_ENABLED BIT(0)
#define SMPS_MODE BIT(1)
	unsigned char smps_info;

#define STA_CONN 0
#define STA_DISCONN 1
	unsigned char connect_state;
	unsigned char op_channel;
} __IMG_PKD;

/**
 * struct cmd_txq_params - Command used to set transmission queue parameters.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_TXQ_PARAMS).
 * @queue_num: The queue number for this queue.
 * @aifsn: Arbitration inter-frame spacing number for the queue.
 * @txop: Transmit Opportunity limit for the queue.
 * @cwmin: Minimum size of contention window
 * @cwmax: Maximum size of contention window
 * @uapsd: Power save mode (0 for legacy mode and 1 for UAPSD)
 * @if_index: Interface index.
 * @vif_addr: MAC address of the interface.
 *
 * This command is used to set transmission queue parameters. There are five
 * transmission queues; Background, Best Effort, Video, Voice and Beacon.
 */
struct cmd_txq_params {
	struct host_rpu_msg_hdr hdr;
	unsigned int queue_num;
	unsigned int aifsn;
	unsigned int txop;
	unsigned int cwmin;
	unsigned int cwmax;
	/* power save mode -
	 * 0 - indicates legacy mode powersave, 1 - indicates UAPSD for the
	 * corresponding AC.
	 */
	unsigned int uapsd;
	unsigned int if_index;
	unsigned char vif_addr[ETH_ALEN];
} __IMG_PKD;

/**
 * struct cmd_aux_adc_chain_sel - Command used to set AUX path in the PHY.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_AUX_ADC_CHAIN_SEL).
 * @chain_id: Aux ADC chain ID.
 *
 * This command is used to set the Aux path in PHY. Used to calibrate the power
 * (to be used only in Production mode).
 */
struct cmd_aux_adc_chain_sel {
	struct host_rpu_msg_hdr hdr;
#define AUX_ADC_CHAIN1	1
#define AUX_ADC_CHAIN2	2
	unsigned int chain_id;
} __IMG_PKD;

/**
 * struct cmd_cont_tx - Command used in Production mode continuous
 *                      transmission.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_CONT_TX).
 * @op: (0 = Stop continuous TX mode; 1 = Start continuous TX mode)
 *
 * This command is used in Production mode for continuously looping a tx
 * packet.
 */
struct cmd_cont_tx {
	struct host_rpu_msg_hdr hdr;
	unsigned int op;
} __IMG_PKD;



struct cmd_set_defaultkey {
	struct host_rpu_msg_hdr hdr;
	unsigned int if_index;
	unsigned int key_id;
} __IMG_PKD;


/**
 * struct cmd_cfg_pwrmgmt- Command used to communicate information
 *                                  about sleep control and configuration.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_CMD_CHANCTX_TIME_INFO).
 * @config_mask: Config Mask for setting various values related to sleep
		 controller. 
 * This command is used to specify information about configuring various values 
 * related to sleep controller.
 * @pwr_on_value: UCC_SLEEP_CTRL_PWR_ON_VALUE registers
 * @pwr_off_value: UCC_SLEEP_CTRL_PWR_OFF_VALUE registers 
 * @ram_on_state: UCC_SLEEP_CTRL_PWR_OFF_VALUE registers
 * @ram_off_state: UCC_SLEEP_CTRL_RAM_OFF_STATE registers
 * @pwr_on_time: UCC_SLEEP_CTRL_PWR_ON_TIME registers
 * @pwr_off_time: UCC_SLEEP_CTRL_PWR_OFF_TIME registers
 * @ram_on_time: UCC_SLEEP_CTRL_RAM_ON_TIME registers
 * @ram_off_time: UCC_SLEEP_CTRL_RAM_OFF_TIME register
 * @sleep_timer_freq_hz: sleep timer frequency in Hz
 *
 * Refer LPW TRM
 */
struct cmd_cfg_pwrmgmt {
	struct host_rpu_msg_hdr hdr;
	enum RPU_SLEEP_CONFIG_CHANGED sleep_config_changed;
	unsigned int pwr_on_value[2]; 
	unsigned int pwr_off_value[2];
	unsigned int ram_on_state[2];  
	unsigned int ram_off_state[2];
	unsigned int pwr_on_time[32];  
	unsigned int pwr_off_time[32];
	unsigned int ram_on_time[4];  
	unsigned int ram_off_time[4];
	unsigned int sleep_timer_freq_hz;
	unsigned int wakeup_time;
	int clk_adj_val;
}__IMG_PKD;

#define LMAC_WATCHDOG_PHY_HANG_RESET_ENABLE   0x1
#define LMAC_FILTER_PROBE_REQ_IN_PS_ENABLE    0x2
#define LMAC_FILTER_BCMC_DATA_IN_PS_ENABLE    0x4
#define LMAC_NULL_FRAME_IN_PS_ENABLE          0x8
struct cmd_patch_feature {
    struct host_rpu_msg_hdr hdr;
    unsigned int feature_val;
}__IMG_PKD;

struct cmd_ip_address {
    struct host_rpu_msg_hdr hdr;
    unsigned int addr;
}__IMG_PKD;

struct cmd_read_csr {
    struct host_rpu_msg_hdr hdr;
    unsigned int addr;
}__IMG_PKD;

/* Events */

struct dump_info {
	/*size of dump info*/
	unsigned int size;
	/*0: means this event contain only part of dump info*/
	/*1: means this event is the end of dump info part*/
	unsigned int end;
	unsigned char data[1];
}__IMG_PKD;

/**
 * struct fw_priv_cmd_done - Event used to signal the completion of info dump
 *        
 */
struct fw_priv_cmd_done {
	struct host_rpu_msg_hdr hdr;
	struct dump_info info; // must be first item after hdr	
}__IMG_PKD;

/**
 * struct umac_event_tx_done - Event used to signal the completion of
 *                             a transmission request for one or more frames.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_TX_DONE).
 * @pdout_voltage: Aux ADC reading. (Used only during FTM.)
 * @frm_status: Status of transmitted frames.
 * @retries_num: Number of times a frame transmission was retried.
 * @rate: This is the rate at which TX frame was successfully transmitted.
 *        (rate = Units of 500 Kbps or MCS index = 0 to 7).
 * @queue: Access category of the transmitted frame.
 * @descriptor_id: Token ID of the transmitted frame, used to match to the
 *                 CMD_TX.
 * @reserved: Reserved bytes for padding.
 *
 * This event is used by the Firmware to signal the completion of a
 * tranmsission request of frame(s) to the Host. It also contains the
 * transmission Success/Failure information for each frame.
 */
struct umac_event_tx_done {
	struct host_rpu_msg_hdr hdr;

	unsigned char pdout_voltage;
	/* frame_status -
	 * 0 - success
	 * 1 - discarded due to retry limit exceeded
	 * 2 - discarded due to msdu lifetime expiry
	 * 3 - discarded due to encryption key not available
	 */
#define TX_DONE_STAT_SUCCESS (0)
#define TX_DONE_STAT_ERR_RETRY_LIM (1)
#define TX_DONE_STAT_MSDU_LIFETIME (2)
#define TX_DONE_STAT_KEY_NOT_FOUND (3)
#define TX_DONE_STAT_DISCARD (4)
#define TX_DONE_STAT_DISCARD_BCN (5)
#define TX_DONE_STAT_DISCARD_OP_TX (7)
	unsigned char frm_status[MAX_TX_CMDS];
	unsigned char retries_num[MAX_TX_CMDS];
	/* rate = Units of 500 Kbps or mcs index = 0 to 7 */
	unsigned char rate[MAX_TX_CMDS];
	unsigned char queue;
	unsigned int descriptor_id;
	unsigned char reserved[12];
} __IMG_PKD;

/**
 * struct wlan_rx_pkt - Event used by Firmware to indicate a RX packet to the
 *                      host
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_RX).
 * @pkt_length: Length of the packet; this is header length + payload
 *              in bytes.
 * @rate_or_mcs: Most significant bit set to 1 implies rate is MCS rate
 *               otherwise legacy rate.
 * @rssi: RSSI strength for this packet in dBm.
 * @rx_pkt_status: Indicates MIC Failure status in case of TKIP/CCMP.
 * @rate_flags: Control info related to rate at which the frame is received.
 * @nss: Number of Spatial Streams.
 * @num_sts: Number of Space Time Streams.
 * @timestamp: Time at which the packet was received.
 * @stbc_enabled: Indication of whether packet is received using STBC.
 * @ldpc_enabled:  Indication of whether packet is received using LDPC.
 * @unused: Unused.
 * @channel: Channel number.
 * @reserved: Reserved bytes for padding.
 * @payload: 802.11 header + payload
 *
 * This event is used to indicate to the Host that a frame has been received,
 * along with the control information related to the frame.
 */
struct wlan_rx_pkt {
	struct host_rpu_msg_hdr hdr;
	/* MPDU/MSDU payload in bytes */
	unsigned int pkt_length;
	/* bit[8] = 0 - legacy data rate
	 *	  = 1 - MCS index
	 */
	unsigned char rate_or_mcs;
	/* RSSI in dbm */
	unsigned char rssi;
	/* packet status
	 * 1 - mic failed
	 * 0 - mic succes reserved for non encryped packet\
	 */
#define RX_MIC_SUCCESS 0 /* No MIC error in frame */
#define RX_MIC_FAILURE_TKIP 1 /* TKIP MIC error in frame */
#define RX_MIC_FAILURE_CCMP 2 /* CCMP MIC error in frame */
	unsigned char rx_pkt_status;
#define ENABLE_11N_FORMAT 0x08

	unsigned char rate_flags;
	unsigned char nss;
	unsigned char num_sts;
	unsigned char timestamp[8];
	unsigned char stbc_enabled;
	unsigned char ldpc_enabled;
	unsigned char unused;
	unsigned char channel;
	unsigned char reserved1[16];
	/* (qos_padding = 2) */
	unsigned char reserved2[2];
	/* Payload bytes */
	unsigned char payload[0];
} __IMG_PKD;

/**
 * struct umac_event_ch_prog_complete - Event used to signal the completion of
 *                                      channel programming command.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_CH_PROG_DONE).
 *
 * This event is used by the Firmware to inform the Host about the completion
 * of the channel programming action in response to the RPU_CMD_CHANNEL
 * command issued before. During the channel switch the Host should not issue
 * any other commands.
 */
struct umac_event_ch_prog_complete {
	struct host_rpu_msg_hdr hdr;
} __IMG_PKD;


/**
 * struct umac_event_noa - Event used to notify that the P2P GO to which we are
 *                         connected has gone into Power Save mode.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_NOA).
 * @if_index: Virtual interface index number.
 * @vif_addr: MAC address of the virtual interface.
 * @noa_active: Indicates whether NOA is active or not.
 * @ap_present: Indicates whether AP is present or not.
 *
 * This event is used to inform the Host about the absence of GO due to GO mode
 * power save in P2P. After receiving this event with ap_absent true, station
 * should refrain from transmitting frames. In this mode, a station can start
 * TX only after NOA event with ap_absent is set to false.
 */
struct umac_event_noa {
	struct host_rpu_msg_hdr hdr;
	unsigned int if_index;
	unsigned char vif_addr[ETH_ALEN];

	/* 1 indicates NoA feature is active
	 * 0 indicates NoA feature is not active
	 */
	unsigned int noa_active;
#define ABSENCE_START 0 /* Indicates AP is absent */
#define ABSENCE_STOP 1 /* Indicates AP is present */
	unsigned int ap_present;
} __IMG_PKD;



/**
 * struct umac_event_mac_stats - Event used by Firmware to communicate MAC
 *                               statistics to the Host.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_MAC_STATS).
 * @roc_start: Number of ROC start commands received from the host.
 * @roc_stop: Number of ROC stop commands received from the host.
 * @roc_complete: Number of ROC start command completes sent to host.
 * @roc_stop_complete: Number of ROC stop command completes sent to host.
 * @tx_cmd_cnt: Number of TX commands received from host.
 * @tx_done_cnt: Number of Tx done events sent to host.
 * @tx_edca_trigger_cnt: Number of times EDCA engine was triggered.
 * @tx_edca_isr_cnt: Number of times EDCA ISR was generated
 *		             (indicated channel win).
 * @tx_start_cnt: Number of TX starts to PHY.
 * @tx_abort_cnt: Number of TX aborts detected.
 * @tx_abort_isr_cnt: Number of TX aborts received from PHY.
 * @tx_underrun_cnt: Number of Tx under-runs.
 * @tx_rts_cnt: Num of RTS frames transmitted.
 * @tx_ampdu_cnt: Num of AMPDU's transmitted, incremented by 1 for each A-MPDU
 *                (consisting of one or more MPDUs)
 * @tx_mpdu_cnt: Number of MPDU's transmitted, incremented by 1 for each MPDU
 *               (1 for each A-MPDU subframe)
 * @tx_crypto_post: Number of TX jobs posted to crypto.
 * @tx_crypto_done: Number of TX jobs completed by crypto.
 * @rx_pkt_to_umac: Number of packets sent to host.
 * @rx_crypto_post: Number of RX jobs posted to crypto.
 * @rx_crypto_done: Number of RX jobs completed by crypto.
 * @rx_isr_cnt: Number of RX ISRs.
 * @rx_ack_cts_to_cnt: Number of times ACK/CTS was not received within a
 *                     expected time.
 * @rx_cts_cnt: Number of CTS frames received.
 * @rx_ack_resp_cnt: Number of ACK frames received.
 * @rx_ba_resp_cnt:  Number of BA frames received.
 * @rx_fail_in_ba_bitmap_cnt: Number of BA frames indicating at least one
 *                            failure in the BA bitmap
 * @rx_circular_buffer_free_cnt: Number of entries returned to RX circular
 *                               buffers
 * @rx_mic_fail_cnt: Number of MIC failures.
 * @hal_cmd_cnt: Number of commands received by HAL from the host.
 * @hal_event_cnt: Number of events sent by HAL to the host.
 * @hal_ext_ptr_null_cnt: Number of packets dropped due to lack of Ext-Ram
 *                        buffers from host.
 * @csync_timeout_cntr: Number of Coarse Sync Fails after packet start is
 *                      detected.
 * @fsync_timeout_cntr: Number of Frame Sync Fails after AutoCorrelation Drop
 *                      detected (in 11b) and acdrop_timeout_cntr.
 * @acdrop_timeout_cntr: Number of Fails in AutoCorrelation Drop Detection
 *                       after Csync detected.
 * @csync_abort_agctrig_cntr: Number of times receiver restarts due to inband
 *                            power change during Csync search for AGC
 *                            triggered cases.
 * @crc_success_cnt: FCS success count indicated by PHY.
 * @crc_fail_cnt: FCS failure count indicated by PHY.
 * @rpu_boot_cnt: Number of times the RPU performed warmboot.
 * @sleep_stats[12]: Stats related to Sleep
 *
 * This event is used to return the MAC stats requested by the Host using the
 * RPU_CMD_MAC_STATS command.
 */
struct umac_event_mac_stats {
	struct host_rpu_msg_hdr hdr;
	unsigned int roc_start;
	unsigned int roc_stop;
	unsigned int roc_complete;
	unsigned int roc_stop_complete;
	unsigned int tx_cmd_cnt;
	unsigned int tx_done_cnt;
	unsigned int tx_edca_trigger_cnt;
	unsigned int tx_edca_isr_cnt;
	unsigned int tx_start_cnt;
	unsigned int tx_abort_cnt;
	unsigned int tx_abort_isr_cnt;
	unsigned int tx_underrun_cnt;
	unsigned int tx_rts_cnt;
	unsigned int tx_ampdu_cnt;
	unsigned int tx_mpdu_cnt;
	unsigned int tx_crypto_post;
	unsigned int tx_crypto_done;
	unsigned int rx_pkt_to_umac;
	unsigned int rx_crypto_post;
	unsigned int rx_crypto_done;
	unsigned int rx_isr_cnt;
	unsigned int rx_ack_cts_to_cnt;
	unsigned int rx_cts_cnt;
	unsigned int rx_ack_resp_cnt;
	unsigned int rx_ba_resp_cnt;
	unsigned int rx_fail_in_ba_bitmap_cnt;
	unsigned int rx_circular_buffer_free_cnt;
	unsigned int rx_mic_fail_cnt;
	unsigned int hal_cmd_cnt;
	unsigned int hal_event_cnt;
	unsigned int hal_ext_ptr_null_cnt;
	unsigned int csync_timeout_cntr;
	unsigned int fsync_timeout_cntr;
	unsigned int acdrop_timeout_cntr;
	unsigned int csync_abort_agctrig_cntr;
	unsigned int crc_success_cnt;
	unsigned int crc_fail_cnt;
#ifdef RPU_SLEEP_ENABLE
	unsigned int rpu_boot_cnt;  
	unsigned int sleep_stats[12];
#endif
} __IMG_PKD;

/**
 * struct host_event_scanres - Event used to inform the Host, about completion
 *                             of the processing of the RPU_CMD_SCAN received
 *                             before.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_SCAN_COMPLETE).
 * @if_index: Index of the interface on which the RPU_CMD_SCAN was processed.
 *
 * This event is used by the Firmware to inform the Host about the completion
 * of the scan operation which the Host would have initiated earlier using the
 * RPU_CMD_SCAN command.
 */
struct host_event_scanres {
	struct host_rpu_msg_hdr hdr;
	int if_index;
} __IMG_PKD;

/**
 * struct host_event_disconnect - Event used to inform the Host, about a
 *                                disconnection.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_DISCONNECTED).
 * @if_index: Index of the interface on which the disconnection happened.
 * @reason_code: Reason for disconnection.
 * @mac_addr: MAC address of the peer which got disconnected.
 *
 * This event is used by the Firmware to inform the Host about the disconnection
 * of a peer along with the reason for disconnection.
 */
struct host_event_disconnect {
	struct host_rpu_msg_hdr hdr;
	int if_index;
#define REASON_DEAUTH 1
#define REASON_AUTH_FAILURE 2
#define REASON_NW_LOST 3
#define REASON_AUTH_TIMEOUT 4
#define REASON_TX_TOKEN_NOTAVAIL 5
#define REASON_ASSOC_TIMEOUT 6
	unsigned int reason_code;
	unsigned char mac_addr[ETH_ALEN];
} __IMG_PKD;

/**
 * struct host_event_reset_complete - Event used to inform the Host, about a
 *                                    completion of a reset command.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_RESET_COMPLETE).
 * @unused: Unused
 * @version: Firmware version number.
 *
 * This event is used by the Firmware to inform the Host about the completion of
 * a reset command (RPU_CMD_RESET) which the Host would have sent earlier.
 */
struct host_event_reset_complete {
	struct host_rpu_msg_hdr hdr;
#ifndef RK915	
	unsigned int unused[16];
#endif
	char version[6+24]; // lmac version + FW build time
} __IMG_PKD;


/**
 * struct umac_event_rf_calib_data - Event used to communicate calibration data
 *                                   to the Host.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_RF_CALIB_DATA).
 * @rf_calib_data_length: Size of the RF calibration data.
 * @rf_calib_data: The RF calibration data.
 *
 * This event is used by the Firmware to return the RF calibration data to the
 * Host. This is sent after every channel change, mainly to be used for debug
 * calibration issues.
 */
struct umac_event_rf_calib_data {
	struct host_rpu_msg_hdr hdr;
	unsigned int  rf_calib_data_length;
	unsigned char rf_calib_data[0];
} __IMG_PKD;

/**
 * struct umac_event_roc_status - Event used to inform host about the status of
 *                                a Remain-On-Channel(ROC) operation.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_ROC_STATUS).
 * @roc_status: Status of the ROC operation (Started/Stopped/Done).
 *
 * This event is used by the Firmware to provide the ROC related
 * information to the Host i.e. whether ROC has been started, stopped or has
 * completed in response to the previous RPU_CMD_ROC_CTRL command issued by the
 * Host.
 * ROC is applicable only for P2P Mode.
 */
struct umac_event_roc_status {
	struct host_rpu_msg_hdr hdr;
	unsigned int roc_status;
} __IMG_PKD;


/**
 * struct umac_event_ps_econ_wake - Event used by Firmware to indicate a
 *                                  Economy mode wakeup event to the
 *                                  host.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_PS_ECON_WAKE).
 * @trigger: Indicates the trigger event which caused the wakeup.
 *
 * This event is used to indicate an Economy mode wakeup event to the Host. It
 * also includes information about the actual trigger which caused the wakeup.
 */
struct umac_event_ps_econ_wake {
	struct host_rpu_msg_hdr hdr;
	enum UMAC_PS_ECON_WAKE_TRIG trigger;
} __IMG_PKD;

/**
 * struct umac_event_ps_econ_cfg_complete - Event used by Firmware to indicate
 *                                          that the firmware has finished
 *                                          processing the RPU_CMD_PS_ECON_CFG
 *                                          command.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       RPU_EVENT_PS_ECON_CFG_DONE).
 * @status: Indicates the whether the RPU_CMD_PS_ECON_CFG was processesed
 *          successfully or not.
 *
 * This event indicates that the firmware has finished processing the
 * RPU_CMD_PS_ECON_CFG command and the driver can return control back
 * to mac80211 to complete the suspend/resume.
 */
struct umac_event_ps_econ_cfg_complete {
	struct host_rpu_msg_hdr hdr;
	unsigned char status; /* SUCCESS/FAILURE */
} __IMG_PKD;

struct host_event_command_complete {
	struct host_rpu_msg_hdr hdr;
} __IMG_PKD;


struct umac_event_ch_switch_complete {
	struct host_rpu_msg_hdr hdr;
	int status;
} __IMG_PKD;

struct umac_event_read_csr_complete {
	struct host_rpu_msg_hdr hdr;
	unsigned int value;
} __IMG_PKD;

#endif /*_HOST_RPU_IF_H_*/
