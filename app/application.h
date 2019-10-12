#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef FIRMWARE
#define FIRMWARE "watering-node"
#endif

#ifndef VERSION
#define VERSION "vdev"
#endif

#include <bcl.h>

// set to true when you will use sensor module&BigClown soil moisture sensor
#define MODULE_SENSOR                           true
// set how long will a node listen for MQTT reply after publishing a data (ms)
#define RADIO_RX_TIMEOUT                        50
// set how long the APP will run in sevice mode (ms)
#define SERVICE_MODE_INTERVAL                   (5 * 60 * 1000)
// how often will be checked battery for update (ms)
#define BATTERY_UPDATE_INTERVAL                 (60 * 60 * 1000)
// how often will be moisture measured (ms)
#define MEASURE_MOISTURE_INTERVAL               (15 * 60 * 1000)
// how often will be water float checked (ms)
#define WATER_FLOAT_DELAY                       (15 * 60 * 1000)
// how long will led lighting
#define LED_LIGHT_DUR                           200
// define default time how long (in miliseconds) pump should run
// a value can be overriden by client
#define PUMP_RUNTIME                            (3 * 1000)
// how often will be temperature published (ms)
#define TEMPERATURE_PUB_INTERVAL                (15 * 60 * 1000)
// threshold which define difference in temperature
// if is threshold exceeded temperature will be published
#define TEMPERATURE_PUB_DIFFERENCE              1.0f
// how often will be temperature, moisture, etc measured in service mode (ms)
#define SENSOR_UPDATE_SERVICE_INTERVAL          (60 * 1000)
// how often will be temperature, moisture, etc. measured in normal mode (ms)
#define SENSOR_UPDATE_NORMAL_INTERVAL           (10 * 60 * 1000)
// Set variable for water pump GPIO pin
#define WATER_PUMP_POWER_ID                     BC_GPIO_P17
// Set variables for water float sensors for low/high water level
#define WATER_FLOAT_LOW_POWER_ID                BC_GPIO_P8
#define WATER_FLOAT_HIGH_POWER_ID               BC_GPIO_P9
#if MODULE_SENSOR
    // BigClown soil sensor macros
    #define MAX_SOIL_SENSORS                    5
    #define SOIL_MOISTURE_SENSOR_COUNT          1
    // threshold which define difference in moisture
    // if is threshold exceeded moisture will be published
    #define SENSOR_MOISTURE_PUB_DIFFERENCE      1
    // function definition for BigClown soil sensor
    void soil_sensor_event_handler(bc_soil_sensor_t *self, uint64_t device_address, bc_soil_sensor_event_t event, void *event_param);
    void switch_to_normal_mode_bc_soil_sensor_task(void *param);
#else
    // set ports for power&analog connections to a soil moisture sensor
    #define SOIL_MOISTURE_PORT_ID               BC_ADC_CHANNEL_A5
    #define SOIL_MOISTURE_POWER_ID              BC_GPIO_P6
    #define SOIL_MOISTURE_POWER_ON_INTERVAL     25
    // function definition
    void _measure_moisture();
    void _stop_measuring_moisture();
#endif

// structre for all tasks
struct {
    bc_scheduler_task_id_t _measure_moisture_id;
    bc_scheduler_task_id_t _stop_measuring_moisture_task_id;
    bc_scheduler_task_id_t _start_water_pump_task_id;
    bc_scheduler_task_id_t _stop_water_pump_task_id;
    bc_scheduler_task_id_t _measure_water_level_task_id;
} tasks;

// structure for water float ports
struct {
    int low;
    int high;
} water_float_ports = { WATER_FLOAT_LOW_POWER_ID, WATER_FLOAT_HIGH_POWER_ID };

// structure for values of water float ports
struct {
    int low;
    int high;
} water_float_states;

// structure for all measured values
struct {
    float_t battery_voltage;
    float_t battery_pct;
    uint16_t moisture;
} values;

// function definition
void _measure_water_level();
void _start_water_pump();
void _stop_water_pump();
void battery_event_handler(bc_module_battery_event_t event, void *event_param);
void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
void exec_tasks(uint64_t *id, const char *topic, void *value, void *param);
void switch_to_normal_mode_task(void *param);
void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param);

#endif // _APPLICATION_H
