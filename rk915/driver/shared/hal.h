/*
 * Copyright (c) 2021, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _HAL_H_
#define _HAL_H_

//#define TX_SG_MODE

#define __IMG_PKD  __attribute__((packed))

#define HAL_INT_CMD_MAX_RX 16
#define HAL_INT_EVENT_MAX_RX 16

#define MAX_DATA_SIZE_12K (12 * 1024)
#define MAX_DATA_SIZE_8K (8 * 1024)
#define MAX_DATA_SIZE_2K (2 * 1024)

#define NUM_TX_DESC 12
#define NUM_FRAMES_IN_TX_DESC 32
#define SHAREDMEM_BUFF_SIZE             ((HAL_SHARED_MEM_MAX_MSG_SIZE * 2))

#define HAL_PRIV_DATA_SIZE 8

#define HAL_SHARED_MEM_MAX_MSG_SIZE 60

#define MSG_STATUS_OFFSET 4
#define MSG_LEN_OFFSET 8

/* Command, Event, Tx Data and Buff mappping offsets */
#define HAL_COMMAND_OFFSET (0)
#define HAL_GRAM_CMD_START HAL_COMMAND_OFFSET
#define HAL_GRAM_CMD_STATUS HAL_COMMAND_OFFSET + MSG_STATUS_OFFSET
#define HAL_GRAM_CMD_LEN HAL_GRAM_CMD_START + MSG_LEN_OFFSET

#define HAL_EVENT_OFFSET (HAL_COMMAND_OFFSET + HAL_SHARED_MEM_MAX_MSG_SIZE)
#define HAL_GRAM_EVENT_START HAL_EVENT_OFFSET
#define HAL_GRAM_EVENT_STATUS HAL_EVENT_OFFSET + MSG_STATUS_OFFSET
#define HAL_GRAM_EVENT_LEN HAL_EVENT_OFFSET + MSG_LEN_OFFSET



/**
 * struct hal_hdr - Header accompanying each message exchanged between the Host
 *                  and Firmware.
 * @id: Used to differentiate between a HAL(Host)<->HAL(Firmware) message
 *      (0xFFFFFFFF) vs UMAC(Host)<->LMAC(FW) message (0x00000000).
 * @unused: Unused.
 *
 * This structure contains the header information which needs to be prepended to
 * each message exchanged between the Host and the Firmware. The @id field is
 * used to differentiate between HAL messages (i.e. messages meant to
 * be processed only within the Host HAL or Firmware HAL) vs MAC messages
 * (i.e. messages which the Host/Firmware HAL will forward to the next layer
 * for further processing).
 */
struct hal_hdr {
	unsigned int id;
	unsigned int unused;
} __IMG_PKD;

/**
 * struct hal_rx_pkt_info - Structure which contains information about a RX
 *                          buffer allocated by the Host.
 * @desc: Descriptor ID of the RX buffer.
 * @ptr: DMAable address of the RX buffer.
 *
 * This structure is used to describe a RX buffer allocated in the Host memory
 * where the RPU can DMA an incoming RX frame.
 */
struct hal_rx_pkt_info {
	/* Rx descriptor */
	unsigned int desc;
	unsigned int ptr;
} __IMG_PKD;

/**
 * struct hal_rx_command - Structure which describes collection of RX buffers.
 * @rx_pkt_cnt: The number of RX buffers described.
 * @rx_pkt: Information (as described by @hal_rx_pkt_info) for multiple RX
 *          buffers.
 *
 * This structure aggregates information (as described by @hal_rx_pkt_info) for
 * multiple RX buffers, so that they can be sent in a single command to the
 * RPU.
 */
struct hal_rx_command {
	unsigned int rx_pkt_cnt;
	struct hal_rx_pkt_info rx_pkt[HAL_INT_CMD_MAX_RX];
} __IMG_PKD;

/**
 * struct cmd_hal - Command used by Host HAL to convey information about RX
 *                  buffers to the RPU HAL.
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       0xFFFFFFFF).
 * @rx_pkt_data: Information (as described by @hal_rx_command) for multiple RX
 *               buffers.
 *
 * This command is used to program information about RX buffers by the Host HAL
 * to the RPU HAL. The information needs to be programmed in 2 instances:
 *   a) Initialization:  This is when the initial pool of RX buffers is
 *                       allocated and the information about these buffers
 *                       needs to be communicated to the RPU.
 *   b) Refilling: This will happen when we receive the HAL event @event_hal
 *                 which has information about one or more RX frames (as
 *                 indicated by the @rx_pkt_cnt member of the event @event_hal)
 *                 that have been DMAed by the RPU to the Host in the RX
 *                 buffers from the existing pool. These buffers now need to be
 *                 replaced by fresh buffers for further RX frames and the
 *                 information about these fresh buffers needs to be
 *                 communicated to the RPU.
 */
struct cmd_hal {
	struct hal_hdr hdr;
	struct hal_rx_command rx_pkt_data;
} __IMG_PKD;

/**
 * struct event_hal - Structure which contains information about RX
 *                    buffers which the RPU has consumed (by DMAing to them).
 * @hdr: Host-Firmware message header (id field needs to be set to
 *       0xFFFFFFFF).
 * @rx_pkt_cnt: The number of RX frames received.
 * @rx_pkt_desc: Descriptor ID's of the RX buffers where the RX frames have
 *               been DMAed.
 * @rx_pkt_len: Length of the Payload for each incoming RX frame.
 * @rx_align_offset: The offset of payload from the DMA start address.
 *                   (needed to align Qos data).
 *
 * This event is used by the Firmware HAL to inform the Host HAL about the RX
 * frames that have been received and DMAed to the buffers in the RX buffer
 * pool. This event can convey information about one or more RX frames at a
 * time. The information conveyed is in the form of Descriptor ID's of the RX
 * buffers (which the Host had programmed to the RPU using @cmd_hal). The Host
 * can then use the Descriptor ID's as an index to retrieve the RX buffer
 * information (as described by @buf_info) from a mapping table that it
 * maintains internally.
 */
struct event_hal {
	struct hal_hdr hdr;
	unsigned int rx_pkt_cnt;
	unsigned int rx_pkt_desc[HAL_INT_EVENT_MAX_RX];
} __IMG_PKD;

#endif /* _HAL_H_ */

/* EOF */
