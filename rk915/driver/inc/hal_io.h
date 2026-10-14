#ifndef _COMMON_IO_H
#define _COMMON_IO_H


int rk915_clear_irq(struct hal_priv *priv);
void rk915_lock(struct hal_priv *priv);
void rk915_unlock(struct hal_priv *priv);
int rk915_read_data_len(struct hal_priv *priv);
int rk915_writeb(struct hal_priv *priv, unsigned int addr, int val);
int rk915_readb(struct hal_priv *priv, unsigned int addr);
int rk915_data_read(struct hal_priv *priv, unsigned int addr,
						    unsigned char *buf, unsigned int len);
int rk915_data_write(struct hal_priv *priv, unsigned int addr,
						    void *buf, size_t buf_len);
int rk915_data_write_sg(struct hal_priv *priv, unsigned int addr,
						    void *buf, size_t buf_len);
int rk915_io_init(struct hal_priv *priv);
int rk915_ejtag(struct hal_priv *priv);
int rk915_serias_read(struct hal_priv *priv, u16 addr,
						void *buf, size_t buf_len, u32 max_len);
int rk915_io_reset(struct hal_priv *priv);
void rk915_notify_pm(struct hal_priv *priv, int wakeup);

#endif //_COMMON_IO_H
