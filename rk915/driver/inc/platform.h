#ifndef __RK915_PLATFORM_H_
#define __RK915_PLATFORM_H_

void rk915_poweroff(void);
void rk915_poweron(void);
void rk915_rescan_card(unsigned insert);
int rk915_platform_bus_init(struct host_io_info *phost);
int rk915_platform_bus_rec_init(struct host_io_info *phost);
int rk915_platform_bus_deinit(struct host_io_info *phost);
int rk915_register_irq(struct host_io_info *host);
int rk915_free_irq(struct host_io_info *host);
void rk915_irq_enable(int enable);
int rk915_bus_register_driver(void);
void rk915_bus_unregister_driver(void);

#define RK915_POWER_ON_DELAY_MS	20

#endif //__RK915_PLATFORM_H_
