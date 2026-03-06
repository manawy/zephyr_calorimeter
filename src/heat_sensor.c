#include "heat_sensor.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>


LOG_MODULE_REGISTER(heat_sensor, LOG_LEVEL_INF);

static const struct adc_dt_spec fluxsensor = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static struct adc_sequence sequence;

const struct device *const rtc = DEVICE_DT_GET(DT_ALIAS(rtc));

static uint16_t buf;

static int get_date_time(struct rtc_time* tm) {
    int ret = rtc_get_time(rtc, tm);
    if (ret < 0) {
        LOG_ERR("Cannot read date time: %d\n", ret);
        return ret;
    }
    return 0;
}

bool measure_heat(struct heat_measure_t *measure) {
    int ret = adc_read_dt(&fluxsensor, &sequence);
    if (ret < 0) {
        LOG_ERR("Could not read channel %d\n", ret);
    }
    struct rtc_time tm;
    get_date_time(&tm);

    int32_t val_uv = (int32_t)((int16_t) buf);
    ret = adc_raw_to_microvolts_dt(&fluxsensor, &val_uv);
    /* conversion to mV may not be supported, skip if not */
    if (ret < 0) {
        LOG_ERR(" (value in mV not available)\n");
        measure->uv = 0;
        return false;
    } else {
        LOG_PRINTK(" = %"PRId32" uV\n", val_uv);
    }

    measure->uv = val_uv;
    measure->time = tm;
    return true;
}

static int init_adc() {
    /* Configure channels individually prior to sampling. */
    adc_sequence_init_dt(&fluxsensor, &sequence);
    if (!adc_is_ready_dt(&fluxsensor)) {
        LOG_ERR("ADC controller device %s not ready\n", fluxsensor.dev->name);
        return -1;
    }
    int err = adc_channel_setup_dt(&fluxsensor);
    if (err < 0) {
        LOG_ERR("Could not setup channel %d\n", err);
        return err;
    }
    LOG_INF("ADC: Initialized");
    return 0;
}

void heat_sensor_thread(void *p1, void *p2, void *p3) {
    struct k_fifo *ht_fifo = (struct k_fifo*) p1;

    sequence.buffer = &buf;
    sequence.buffer_size = sizeof(buf);/* buffer size in bytes, not number of samples */
    init_adc();

    while (1) {
        struct heat_measure_t measure;
        measure_heat(&measure);

        k_fifo_put(ht_fifo, &measure);

        k_sleep(K_SECONDS(2));
    }
}
