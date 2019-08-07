#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef FIRMWARE
#define FIRMWARE "watering-node"
#endif

#ifndef VERSION
#define VERSION "vdev"
#endif

#include <bcl.h>

#define MODULE_SENSOR true

// how often will be moisture measured (ms)
#define MEASURE_MOISTURE_DELAY              (30 * 1000)
// how often will be water float checked (ms)
#define WATER_FLOAT_DELAY                   (10 * 60 * 1000)
// how long will led lighting
#define LED_LIGHT_DUR                       200
// define default time how long (in miliseconds) pump should run
// a value can be overriden by client
#define PUMP_RUNTIME                        (3 * 1000)
// how often will be temperature published (ms)
#define TEMPERATURE_PUB_INTERVAL            (15 * 60 * 1000)
// threshold which define difference in temperature
// if is threshold exceeded temperature will be published
#define TEMPERATURE_PUB_DIFFERENCE          1.0f
// how often will be temperature measured (ms)
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (10 * 60 * 1000)
// Set variable for water pump GPIO pin
#define WATER_PUMP_POWER_ID BC_GPIO_P7
// Set variable for water float sensors for low/high water level
#define WATER_FLOAT_LOW_POWER_ID BC_GPIO_P8
#define WATER_FLOAT_HIGH_POWER_ID BC_GPIO_P9
#if MODULE_SENSOR
    // BigClown soil sensor macros
    #define MAX_SOIL_SENSORS 5
    #define SENSOR_UPDATE_SERVICE_INTERVAL      (30 * 1000)
    //#define SENSOR_UPDATE_NORMAL_INTERVAL       (60 * 1000)
    //#define SENSOR_MOISTURE_PUB_INTERVAL        (5 * 60 * 1000)
    // threshold which define difference in moisture
    // if is threshold exceeded moisture will be published
    #define SENSOR_MOISTURE_PUB_DIFFERENCE      5.0f
    // function definition for BigClown soil sensor
    void soil_sensor_event_handler(bc_soil_sensor_t *self, uint64_t device_address, bc_soil_sensor_event_t event, void *event_param);
#else
    // Set macros for moisture soil sensor
    #define SOIL_MOISTURE_PORT_ID BC_ADC_CHANNEL_A5
    #define SOIL_MOISTURE_POWER_ID BC_GPIO_P6
    // function definition
    void _measure_moisture_task();
    void _stop_measuring_moisture_task();
#endif
//#if BATTERY_MINI
//    #define BC_MODULE_BATTERY_FORMAT BC_MODULE_BATTERY_FORMAT_MINI
//#else
//    #define BC_MODULE_BATTERY_FORMAT BC_MODULE_BATTERY_FORMAT_STANDARD
//#endif
// how often will be battery voltage measured (ms)
#define BATTERY_UPDATE_INTERVAL             (60 * 60 * 1000)

struct {
    bc_scheduler_task_id_t _measure_moisture_task_id;
    bc_scheduler_task_id_t _stop_measuring_moisture_task_id;
    bc_scheduler_task_id_t _start_water_pump_task_id;
    bc_scheduler_task_id_t _stop_water_pump_task_id;
    bc_scheduler_task_id_t _measure_water_level_task_id;
} tasks;

struct {
    int low;
    int high;
} water_float_ports = { WATER_FLOAT_LOW_POWER_ID, WATER_FLOAT_HIGH_POWER_ID };

struct {
    int low;
    int high;
} water_float_states;

struct {
    float_t battery_voltage;
    float_t battery_pct;
    uint16_t moisture;
} values;

// function definition
void _start_water_pump();
void _stop_water_pump();
void _measure_water_level();
void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
void battery_event_handler(bc_module_battery_event_t event, void *event_param);
void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param);
void exec_tasks(uint64_t *id, const char *topic, void *value, void *param);

#endif // _APPLICATION_H
