#ifndef CALO_HEAT_SENSOR_H
#define CALO_HEAT_SENSOR_H

#include <zephyr/drivers/rtc.h>

struct heat_measure_t {
    // A type to store the reseult of a measurement
    // aimed to be send through a FIFO queue for further processing
    void *fifo_reserved;
    struct rtc_time time;
    int32_t uv;
};

/* The main entry point of the thread responsible to measure the heat flux
 *
 */
void heat_sensor_thread(void *ht_fifo, void *p2, void *p3);

#endif //CALO_HEAT_SENSOR_H
