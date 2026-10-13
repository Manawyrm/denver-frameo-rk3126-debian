#include <linux/rfkill.h>

#include "core.h"
#include "if_io.h"
#include "platform.h"

extern int hal_irq_handler(struct hal_priv *p);

void rk915_rescan_card(unsigned insert)
{
    //rockchip_wifi_set_carddetect(insert);
}

void rk915_poweron(void)
{
	//rockchip_wifi_power(0);
	//mdelay(RK915_POWER_ON_DELAY_MS);
	//rockchip_wifi_power(1);
}

void rk915_poweroff(void)
{
	//rockchip_wifi_power(0);
}

static irqreturn_t hal_interrupt(int irq, void *dev_id)
{
	hal_irq_handler(hpriv);
	return IRQ_HANDLED;
}

void rk915_irq_enable(int enable)
{
	/*if (enable) {
		enable_irq(hpriv->io_info->irq);
	} else {
		disable_irq(hpriv->io_info->irq);
	}*/
}

int rk915_register_irq(struct host_io_info *host)
{
	int ret;
	if (host->irq <= 0) { return 0; }
	
	ret = devm_request_irq(host->dev, host->irq, hal_interrupt,
				IRQF_TRIGGER_RISING|IRQF_NO_SUSPEND, "rk915", hpriv);
	if (ret == 0) {
		ret = enable_irq_wake(host->irq);
		rk915_irq_enable(0);
		host->irq_request = true;
	}

	return ret;
}

int rk915_free_irq(struct host_io_info *host)
{
	if (host->irq <= 0) { return 0; }
	if (host->irq_request) {
		devm_free_irq(host->dev, host->irq, hpriv);
		host->irq_request = false;
	}

	return 0;
}

int rk915_bus_register_driver(void)
{
	return rk915_sdio_register_driver();
}

void rk915_bus_unregister_driver(void)
{
	rk915_sdio_unregister_driver();
}

int rk915_platform_bus_init(struct host_io_info *phost)
{
	if (!phost->bus_init)
		return rk915_sdio_init(phost);
	else
		return 0;
}

int rk915_platform_bus_rec_init(struct host_io_info *phost)
{
	return rk915_sdio_recovery_init(phost);
}

int rk915_platform_bus_deinit(struct host_io_info *phost)
{
	if (phost->bus_init)
		return rk915_sdio_deinit(phost);
	else
		return 0;
}
