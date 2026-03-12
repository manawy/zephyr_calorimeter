/*
 * Shared information
 *
 */

#ifndef CALORIMETER_APP_H
#define CALORIMETER_APP_H

#include <stdbool.h>

// State of the SD card reader
// ---------------------------
typedef enum {
    SDCardError = -1, // Some error happen
    SDCardNoInit = 0,
    SDCardInit = 1
} SDCardState;

typedef enum {
    StartMeasurement,
    OngoingMeasurement,
    StopMeasurement,
    StoppedMeasurement
} MeasurementState;

// Holders for the various shared values
typedef struct {

    MeasurementState measurement_state;
    SDCardState sd_card_state;

} app_state_t;

#define INIT_APPSTATE(X) static app_state_t X = {.measurement_state=StoppedMeasurement, .sd_card_state=SDCardNoInit}

#endif // CALORIMETER_APP_H
