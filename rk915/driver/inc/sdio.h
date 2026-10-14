#ifndef IF_SDIO_H
#define IF_SDIO_H

#define TX_MSG_CMD		1
#define TX_MSG_PKT		2
#define TX_MSG_PATCH		3

#define PKT_TYPE_MPDU		1
#define PKT_TYPE_AMSDU		2
#define PKT_TYPE_AMPDU		3

#define READ_EVENT_REQ		1
#define READ_PKT_REQ		2

enum sdio_rw_op_type {
	SDIO_RW_DONE_STATUS = 0,
	SDIO_WRITE_STATUS,
	SDIO_READ_STATUS,
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Host Read Only Regs
//////////////////////////////////////////////////////////////////////////////////////////////

/* Host need to check this reg before each write operation.
 * for write to read
 * 8bit
 * device:W/R   host:R
 * SDIO_HOST_READ_REG_WRITE_EN[0]=1'b1: host write enable
 * SDIO_HOST_READ_REG_WRITE_EN[0]=1'b0: host write disable
 * SDIO_HOST_READ_REG_WRITE_EN[7:1]: reason code(reserved)
 */
#define SDIO_HOST_READ_REG_WRITE_EN			0 

/* Host need to check this reg before read operation.
 * for read to read
 * 16bit
 * device:W/R   host:R
 * SDIO_RX_CTRL_REG[14:0]: length
 * SDIO_RX_CTRL_REG[15]: 1'b1 more data, 1'b0 no more data
 */
#define SDIO_HOST_READ_REG_RECV_LEN_L		2
#define SDIO_HOST_READ_REG_RECV_LEN_H		3

/* 8bit
 * device:W/R   host:R
 * SDIO_RX_CTRL_REG[7:0]: firmware state
 */
enum FW_STATE {
	WAIT_FW = 0,
	M0_STARTED,
	WAIT_PATCH,
	WAIT_PATCH2,
	LPW_READY,
	M0_READY,
	M0_SLEEP,
	RESET_COMPLETE,
};

#define SDIO_HOST_READ_REG_FW_STATE			64

//////////////////////////////////////////////////////////////////////////////////////////////
// Host Write Only Regs
//////////////////////////////////////////////////////////////////////////////////////////////

/* Host need to write this reg before write operation.
 * for write to write
 * 8bit
 * device:R     host:W/R
 * SDIO_HOST_WRITE_REG_PKT_INFO[1:0]: type(CMD/PKT/PATCH)
 * type:
 *      2'b01:CMD
 *      2'b10:PKT
 *      2'b11:PATCH
 *
 * SDIO_HOST_WRITE_REG_PKT_INFO[1:0]=2'b00: type=CMD, 
 * SDIO_HOST_WRITE_REG_PKT_INFO[7:2]:CMDID
 *
 * SDIO_HOST_WRITE_REG_PKT_INFO[1:0]=2'b01: type=PKT, 
 * SDIO_HOST_WRITE_REG_PKT_INFO[3:2]:pkt type(AMPDU/AMSDU/MPDU)
 * pkt type:
 *      2'b01:MPDU
 *      2'b10:AMSDU
 *      2'b11:AMPDU
 * SDIO_HOST_WRITE_REG_PKT_INFO[3:2]=2'b11, 
 * SDIO_HOST_WRITE_REG_PKT_INFO[7:4]:AMPDU PKT SEQ
 */
#define SDIO_HOST_WRITE_REG_PKT_INFO		17

/* Host need to write this reg before patch transmited.
 * only for patch download
 * 16bit
 * device:R		host:W/R
 * rom patch total length
 */
#define SDIO_HOST_WRITE_REG_PATCH_LEN_L		18
#define SDIO_HOST_WRITE_REG_PATCH_LEN_H		19
#define SDIO_HOST_WRITE_REG_NOTIFY_ADDR	17
#define SDIO_HOST_WRITE_REG_NOTIFY_VAL	1
#define SDIO_HOST_WRITE_REG_INT_ADDR	32
#define SDIO_HOST_WRITE_REG_CLR_INT		2
#define SDIO_HOST_WRITE_REQ_INT_STA		35

#define SDIO_CMD_ADDR			0x10000
#define SDIO_HOST_PATCH_ADDR	0
#define SDIO_START_CMD_ID		0x5A5A5A5A

/* hwio addr info */
#define IO_START_CMD_ADDR		SDIO_CMD_ADDR

#define IO_FW_STATE			SDIO_HOST_READ_REG_FW_STATE

#define IO_PATCH_LEN_L			SDIO_HOST_WRITE_REG_PATCH_LEN_L
#define IO_PATCH_LEN_H			SDIO_HOST_WRITE_REG_PATCH_LEN_H

#define IO_PATCH_ADDR			SDIO_HOST_PATCH_ADDR
#define IO_START_CMD_ID			SDIO_START_CMD_ID

#define IO_RECV_LEN_L 			SDIO_HOST_READ_REG_RECV_LEN_L
#define IO_RECV_LEN_H 			SDIO_HOST_READ_REG_RECV_LEN_H

#define IO_INT_ADDR			SDIO_HOST_WRITE_REG_INT_ADDR
#define IO_INT_CLR_IRQ_VAL		SDIO_HOST_WRITE_REG_CLR_INT

#define IO_NOTIFY_ADDR			SDIO_HOST_WRITE_REG_NOTIFY_ADDR
#define IO_NOTIFY_VAL			SDIO_HOST_WRITE_REG_NOTIFY_VAL
#define IO_NOTIFY_SLEEP			2
#define IO_NOTIFY_WAKEUP		3

#endif

