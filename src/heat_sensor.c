#include "heat_sensor.h"
#include "app.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>

// Configuration
// -------------

// time between measure
#define TIME_BETWEEN_MEASURE K_SECONDS(2)

// Logging
// --------
LOG_MODULE_REGISTER(heat_sensor, LOG_LEVEL_INF);

// Devices from DT
// ---------------
static const struct adc_dt_spec fluxsensor = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
const struct device *const rtc = DEVICE_DT_GET(DT_ALIAS(rtc));

/* Get time from the RTC module
 */
static int get_date_time(struct rtc_time* tm) {
    int ret = rtc_get_time(rtc, tm);
    if (ret < 0) {
        LOG_ERR("Cannot read date time: %d\n", ret);
        return ret;
    }
    return 0;
}

/* @brief Measure heat flow
 *
 * @param sequence Structure defining an ADC sampling sequence
 * @param measure  Structure contenant le résultat
 *
 * @return True if measure and conversion was successful
 */
bool measure_heat(struct adc_sequence* sequence, heat_measure_t *measure) {
    int ret = adc_read_dt(&fluxsensor, sequence);
    if (ret < 0) {
        LOG_ERR("Could not read channel %d\n", ret);
        return false;
    }
    get_date_time(&(measure->time));
    measure->uv  = (int32_t)(*((int16_t*) sequence->buffer));
    ret = adc_raw_to_microvolts_dt(&fluxsensor, &(measure->uv));
    LOG_PRINTK(" = %"PRId32" uV\n", measure->uv);
    return true;
}

/* @brief Initialize the ADC
 *
 * @param sequence Structure defining an ADC sampling sequence
 *
 * @return 0 si initialization succesful, <0 if error
 */
static int init_adc(struct adc_sequence* sequence) {
    /* Configure channels individually prior to sampling. */
    adc_sequence_init_dt(&fluxsensor, sequence);
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
    struct k_fifo* ht_fifo = (struct k_fifo*) p1;
    app_state_t* app_state = (app_state_t*) p2;
    uint16_t buf; // The main buffer

    struct adc_sequence sequence;
    sequence.buffer = &buf;
    sequence.buffer_size = sizeof(buf);/* buffer size in bytes, not number of samples */

    // Initialize The ADC
    init_adc(&sequence);

    while (1) {
        // TODO use ISR
        if (app_state->measurement_state == OngoingMeasurement) {
            heat_measure_t measure;
            measure_heat(&sequence, &measure);

            k_fifo_put(ht_fifo, &measure);
        }

        k_sleep(TIME_BETWEEN_MEASURE);
    }
}
