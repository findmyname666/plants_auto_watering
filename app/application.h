#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef FIRMWARE
#define FIRMWARE "watering-node"
#endif

#ifndef VERSION
#define VERSION "vdev"
#endif

#include <bcl.h>
#include <bc_gpio.h>
#include <bc_adc.h>

// Set variable for water pump GPIO pin
#define WATER_PUMP_POWER_ID BC_GPIO_P3
// Set variables for humidity soil sensor
#define SOIL_HUM_BASIL_PORT_ID BC_ADC_CHANNEL_A2
#define SOIL_HUM_BASIL_POWER_ID BC_GPIO_P1

#if BATTERY_MINI
    #define BC_MODULE_BATTERY_FORMAT BC_MODULE_BATTERY_FORMAT_MINI
#else
    #define BC_MODULE_BATTERY_FORMAT BC_MODULE_BATTERY_FORMAT_STANDARD
#endif
#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)

struct {
    bc_scheduler_task_id_t _measure_humidity_task_id;
    bc_scheduler_task_id_t _stop_measuring_humidity_task_id;
    bc_scheduler_task_id_t _start_water_pump_task_id;
    bc_scheduler_task_id_t _stop_water_pump_task_id;
    uint16_t _measured_humidity;
} watering;

struct {
    float_t battery_voltage;
    float_t battery_pct;
} battery_values;

#endif // _APPLICATION_H
