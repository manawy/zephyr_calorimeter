#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include <zephyr/sys/printk.h>

#include <zephyr/drivers/gpio.h>

#include "heat_sensor.h"
#include "write.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define SLEEP_TIME_MS 5000

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

static const struct gpio_dt_spec led_ok = GPIO_DT_SPEC_GET(DT_ALIAS(ledok), gpios);
static const struct gpio_dt_spec led_busy = GPIO_DT_SPEC_GET(DT_ALIAS(ledbusy), gpios);

//static const struct adc_dt_spec fluxsensor = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

//static const struct adc_dt_spec fluxsensor = //ADC_DT_SPEC_GET_BY_IDX(DT_ALIAS(flux_sensor), 0);

int init_leds() {
	int ret;

	if (!gpio_is_ready_dt(&led_ok) || !gpio_is_ready_dt(&led_busy)) {
		LOG_ERR("LEDS not ready");
		return -1;
	}
	ret = gpio_pin_configure_dt(&led_ok, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Cannot set led_ok");
		return ret;
	}
	ret = gpio_pin_configure_dt(&led_busy, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Cannot set led_busy");
		return ret;
	}
	LOG_INF("Leds configured");
	return 0;
}


K_THREAD_STACK_DEFINE(heatsensor_stack_area, 8192);
struct k_thread heatsensor_thread_data;

K_THREAD_STACK_DEFINE(write_stack_area, 8192);
struct k_thread write_thread_data;

K_FIFO_DEFINE(ht_fifo);

int main(void)
{
	int ret;
	bool led_state = true;

	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;



	LOG_INF("Initialization console");
	/* Poll if the DTR flag was set */
	while (!dtr) {
		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		LOG_INF("Initialization console in progress");
		/* Give CPU resources to low priority threads. */
		k_sleep(K_MSEC(100));
	}
	LOG_PRINTK("Console ready !\n");

	k_sleep(K_SECONDS(1));

	if (init_leds() < 0) {
		LOG_ERR("LEDS Not ready, aborting");
		//return 0;
	}

	k_tid_t heat_sensor_tid = k_thread_create(
		&heatsensor_thread_data, heatsensor_stack_area,
		K_THREAD_STACK_SIZEOF(heatsensor_stack_area),
		heat_sensor_thread,
		(void *) &ht_fifo, NULL, NULL,
		5, 0, K_NO_WAIT);
	LOG_INF("Start heat sensor thread");
	k_tid_t write_tid = k_thread_create(
		&write_thread_data, write_stack_area,
		K_THREAD_STACK_SIZEOF(write_stack_area),
		write_thread,
		(void *) &ht_fifo, NULL, NULL,
		20, 0, K_NO_WAIT);
	LOG_INF("Start write thread");

	while (1) {
		ret = gpio_pin_toggle_dt(&led_ok);
		if (ret < 0) {
			return 0;
		}
		ret = gpio_pin_toggle_dt(&led_busy);
		if (ret < 0) {
			return 0;
		}

		led_state = !led_state;
		//LOG_PRINTK("LED state: %s\n", led_state ? "ON" : "OFF");

		k_sleep(K_SECONDS(1));

		ret = gpio_pin_toggle_dt(&led_ok);
		if (ret < 0) {
			return 0;
		}
		ret = gpio_pin_toggle_dt(&led_busy);
		if (ret < 0) {
			return 0;
		}

		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}
