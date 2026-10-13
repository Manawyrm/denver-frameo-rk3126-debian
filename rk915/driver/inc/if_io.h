#ifndef _IF_IO_H_
#define _IF_IO_H_

enum IO_IF_TYPE {
	IO_IF_SDIO = 0,
	IO_IF_USB,
	IO_IF_SPI,
	IO_IF_PCIE,
};

enum FW_ERR_REASON {
	FW_ERR_SDIO,
	FW_ERR_RESET_CMD,
	FW_ERR_LPW_RECOVERY
};

#define SUPPORT_SDIO_SLEEP	0
#define NOTIFY_M0_SLEEP		1
#define SDIO_AUTO_SLEEP		0

#define MAX_RX_SERIAS_COUNT	16
#define MAX_RX_SERIAS_BYTES	(MAX_RX_SERIAS_COUNT * MAX_DATA_SIZE_2K + 512)
struct host_io_info {
	struct device		*dev;
	unsigned int		irq;
	bool				irq_request;
	struct host_io_ops 	*io_ops;
	unsigned char		type;
	void*			rx_serias_buf;
	void*			rx_serias_buf_curr;
	unsigned short		rx_serias_len[MAX_RX_SERIAS_COUNT];
	int			rx_serias_count;
	int			rx_serias_idx;
	unsigned int		rx_next_len;
	void			*priv_data;
	struct firmware_info	firmware;
#if SDIO_AUTO_SLEEP
	struct delayed_work	sleep_work;
#endif
	bool				bus_init;
};

struct cmd_rx_ctrl {
	struct host_rpu_msg_hdr	mac_hdr;
	unsigned int		pkt_count;
	unsigned short		pkt_len[MAX_RX_SERIAS_COUNT];
} __IMG_PKD;

struct host_io_ops {
	int	(*io_send)(struct host_io_info *host, unsigned int addr, unsigned char *buf, unsigned int len);
	int	(*io_send_sg)(struct host_io_info *host, unsigned int addr, unsigned char *buf, unsigned int len);
	int	(*io_recv)(struct host_io_info *host, unsigned int addr, unsigned char *buf, unsigned int len);
	int	(*io_readb)(struct host_io_info *host, unsigned int addr);
	int	(*io_writeb)(struct host_io_info *host, unsigned int addr, unsigned char val);
	int	(*io_writeb_comp)(struct host_io_info *host);
	int	(*io_init)(struct host_io_info *host);
	int	(*io_register_irq)(struct host_io_info *host);
	void	(*lock)(struct host_io_info *host);
	void	(*unlock)(struct host_io_info *host);
	int	(*io_ejtag)(struct host_io_info *host);
	int	(*io_reset)(struct host_io_info *host);
	int	(*io_notify_pm)(struct host_io_info *host, int wakeup);
#if SUPPORT_SDIO_SLEEP
	int	(*sleep)(struct host_io_info *host);
	int	(*wakeup)(struct host_io_info *host);
#endif
};

void rk915_sdio_pre_init(void);
int rk915_sdio_init(struct host_io_info *phost);
int rk915_sdio_deinit(struct host_io_info *phost);
int rk915_sdio_recovery_init(struct host_io_info *phost);
void rk915_signal_io_error(int reason);
int rk915_sdio_register_driver(void);
void rk915_sdio_unregister_driver(void);
void rk915_sdio_set_clock(struct host_io_info *phost, int hz);

#endif

