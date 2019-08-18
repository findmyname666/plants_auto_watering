#include <application.h>

// define subcribe topic for MQTT
// subscribe table, format: topic, expect payload type, callback, user param
static const bc_radio_sub_t subs[] = {
    // MQTT subscribe topic which will trigger pump
    { "task/pump/start/5", BC_RADIO_SUB_PT_INT, exec_tasks, NULL }
};

// core module LED instance
bc_led_t led;
// core module button instance
bc_button_t button;
// core module temperature sensor instance
bc_tmp112_t tmp112;

#if MODULE_SENSOR
    // define instances for BigClown soil sensors
    // BigClown Soil sensor instance
    bc_soil_sensor_t soil_sensor;
    // sensors array
    bc_soil_sensor_sensor_t sensors[MAX_SOIL_SENSORS];
    // time of next sensor temperature report
    bc_tick_t sensor_temperature_tick_report[MAX_SOIL_SENSORS];
    // last sensor temperature value used for change comparison
    float sensor_last_published_temperature[MAX_SOIL_SENSORS];
    // time of next sensor moisture report
    bc_tick_t sensor_moisture_tick_report[MAX_SOIL_SENSORS];
    // last sensor moisture value used for change comparison
    int sensor_last_published_moisture[MAX_SOIL_SENSORS];
#endif

void application_init(void)
{
    // initialize logging
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // initialize a LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    // initialize a button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // initialize radio - sleeping mode is suitable for battery
    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
    bc_radio_pairing_request(FIRMWARE, VERSION);
    // subscribe to MQTT topic
    bc_radio_set_subs((bc_radio_sub_t *) subs, sizeof(subs)/sizeof(bc_radio_sub_t));
    bc_radio_set_rx_timeout_for_sleeping_node(RADIO_RX_TIMEOUT);

    // initialize a battery module
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // initialize a temperature sensor
    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    // set measure inteval
    bc_tmp112_set_update_interval(&tmp112, SENSOR_UPDATE_SERVICE_INTERVAL);

    // initialize port for a transistor/switch to turn on a water pump
    bc_gpio_init(WATER_PUMP_POWER_ID);
    bc_gpio_set_mode(WATER_PUMP_POWER_ID, BC_GPIO_MODE_OUTPUT);
    // create tasks - stop water pump
    tasks._stop_water_pump_task_id = bc_scheduler_register(_stop_water_pump, NULL, 100);

    // initialize ports for water floats
    bc_gpio_init(water_float_ports.low);
    bc_gpio_init(water_float_ports.high);
    bc_gpio_set_mode(water_float_ports.low, BC_GPIO_MODE_INPUT);
    bc_gpio_set_mode(water_float_ports.high, BC_GPIO_MODE_INPUT);
    // set it up in pull up state to reports 0, 1
    bc_gpio_set_pull(water_float_ports.low, BC_GPIO_PULL_UP);
    bc_gpio_set_pull(water_float_ports.high, BC_GPIO_PULL_UP);
    // create task for reading sensor value + execute task
    tasks._measure_water_level_task_id = bc_scheduler_register(_measure_water_level, NULL, 1000);

#if MODULE_SENSOR
    // initialize BigClown soil sensor
    bc_soil_sensor_init_multiple(&soil_sensor, sensors, SOIL_MOISTURE_SENSOR_COUNT);
    bc_soil_sensor_set_event_handler(&soil_sensor, soil_sensor_event_handler, NULL);
    bc_soil_sensor_set_update_interval(&soil_sensor, SENSOR_UPDATE_SERVICE_INTERVAL);
    // switch BC soil sensor to normal mode
    bc_scheduler_register(switch_to_normal_mode_bc_soil_sensor_task, NULL, SERVICE_MODE_INTERVAL);
#else
    // initialize power for a soil sensor
    bc_gpio_init(SOIL_MOISTURE_POWER_ID);
    bc_gpio_set_mode(SOIL_MOISTURE_POWER_ID, BC_GPIO_MODE_OUTPUT);
    // initialization of analog port for reading values from a soil moisture sensor
    bc_adc_init();
    // set a first moisture value
    values.moisture = 0;
    // create task for regular measurements of moisture
    tasks._measure_moisture_id = bc_scheduler_register(_measure_moisture_task, NULL, 1000);
    // create task to stop moisture measurement
    tasks._stop_measuring_moisture_task_id = bc_scheduler_register(_stop_measuring_moisture, NULL, 1000);
#endif
    // switch to normal mode for temperature sensor on core module
    bc_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_MODE_INTERVAL);
}

// switch from service mode to normal mode
void switch_to_normal_mode_task(void *param)
{
    bc_tmp112_set_update_interval(&tmp112, SENSOR_UPDATE_NORMAL_INTERVAL);
    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}

// event handler for a core module's button
void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_pulse(&led, 100);
        static uint16_t event_count = 0;
        bc_radio_pub_push_button(&event_count);
        event_count++;
    }
}

// event handler for a battery module
void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;
    float voltage;

    if (bc_module_battery_get_voltage(&voltage))
    {
        values.battery_voltage = voltage;
        bc_radio_pub_battery(&voltage);
    }
}

// event handler for a temperature provided by core module
void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    // Time of next report
    static bc_tick_t tick_report = 0;

    // Last value used for change comparison
    static float last_published_temperature = NAN;

    if (event == BC_TMP112_EVENT_UPDATE)
    {
        float temperature;

        if (bc_tmp112_get_temperature_celsius(self, &temperature))
        {
            // Implicitly do not publish message on radio
            bool publish = false;

            // Is time up to report temperature?
            if (bc_tick_get() >= tick_report)
            {
                // Publish message on radio
                publish = true;
            }
            // Is temperature difference from last published value significant?
            else if (fabsf(temperature - last_published_temperature) >= TEMPERATURE_PUB_DIFFERENCE)
            {
                // Publish message on radio
                publish = true;
            }

            if (publish)
            {
                bool pub_success;
                // Publish temperature message on radio
                // It returns bool value
                pub_success = bc_radio_pub_temperature(BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE, &temperature);

                if (!pub_success) {
                    bc_log_error("Error occured while publishing temperature value.");
                }

                // Schedule next temperature report
                tick_report = bc_tick_get() + TEMPERATURE_PUB_DIFFERENCE;

                // Remember last published value
                last_published_temperature = temperature;
            }
        } else if (event == BC_TMP112_EVENT_UPDATE) {
            bc_log_error("Temperature error event received.");
        }
    }
}

// start a water pump
void _start_water_pump(int pump_runtime) {
    bc_led_pulse(&led, LED_LIGHT_DUR);
    bc_gpio_set_output(WATER_PUMP_POWER_ID, 1);
    bc_scheduler_plan_relative(tasks._stop_water_pump_task_id, pump_runtime);
}

// stop a water pump
void _stop_water_pump() {
    bc_gpio_set_output(WATER_PUMP_POWER_ID, 0);
}

// measure level of water with float sensors and published their states
// in case of a change
void _measure_water_level() {
    int low = bc_gpio_get_input(water_float_ports.low);
    int high = bc_gpio_get_input(water_float_ports.high);
    bc_log_debug("Float state input for sensor IDs: %i (low) - %i, %i (high) - %i",
        water_float_ports.low, low, water_float_ports.high, high);
    if (low != water_float_states.low) {
        bc_radio_pub_int("water/level/state/low", &low);
    }
    if (high != water_float_states.high) {
        bc_radio_pub_int("water/level/state/high", &high);
    }
    water_float_states.low = low;
    water_float_states.high = high;
    bc_scheduler_plan_relative(tasks._measure_water_level_task_id, WATER_FLOAT_DELAY);
}

// it is executed based on a request from MQTT
// it does run a water pump and check water level
// client can define how long a pump should run - see a value of request's payload
void exec_tasks(uint64_t *id, const char *topic, void *value, void *param)
{
    // use payload requested by client
    int pump_runtime = *((int*) value);
    bc_log_debug("Pump runtime in seconds received from a client: %i", pump_runtime);
    // TODO if payload is empty it gets bogus values + we want to set a reasonable value for a runtime
    if (pump_runtime < 1 || pump_runtime > (PUMP_RUNTIME * 10)) {
        bc_log_debug("Requested pump runtime is too low (<1) or too high "
            "(> default_value * 10). Setting default value: %i", PUMP_RUNTIME);
        pump_runtime = PUMP_RUNTIME;
    }
    bc_log_debug("Pump runtime: %i", pump_runtime);
    bc_log_debug("starting a water pump");
    _start_water_pump(pump_runtime);
    bc_log_debug("watering finished");
    // measure water level after watering
    bc_scheduler_plan_now(tasks._measure_water_level_task_id);
}

#if MODULE_SENSOR
    // event handler for BigClown soil moisture sensor
    // it reads&publishes moisture&temperature provided by soil moisture sensor
    void soil_sensor_event_handler(bc_soil_sensor_t *self, uint64_t device_address, bc_soil_sensor_event_t event, void *event_param)
    {
        static char topic[64];
        bc_log_debug("Number of soil sensors: %d", bc_soil_sensor_get_sensor_found(&soil_sensor));

        if (event == BC_SOIL_SENSOR_EVENT_UPDATE)
        {
            int index = bc_soil_sensor_get_index_by_device_address(self, device_address);

            if (index < 0)
            {
                return;
            }

            float temperature;

            if (bc_soil_sensor_get_temperature_celsius(self, device_address, &temperature))
            {
                bool publish = false;
                bc_log_debug("Soil temperature for sensor ID %llx: %f", device_address, temperature);

                // Is time up to report temperature?
                if (bc_tick_get() >= sensor_temperature_tick_report[index])
                {
                    // Publish message on radio
                    bc_log_debug("it's time to publish temperature due to defined interval");
                    publish = true;
                }
                // Is temperature difference from last published value significant?
                else if (fabsf(temperature - sensor_last_published_temperature[index]) >= TEMPERATURE_PUB_DIFFERENCE)
                {
                    // Publish message on radio
                    bc_log_debug("it's time to publish temperature as difference is significant: %f", fabsf(temperature - sensor_last_published_temperature[index]));
                    publish = true;
                }

                if (publish)
                {
                    snprintf(topic, sizeof(topic), "soil-sensor/%llx/temperature", device_address);
                    // Publish temperature message on radio
                    bc_radio_pub_float(topic, &temperature);
                    // Schedule next temperature report
                    sensor_temperature_tick_report[index] = bc_tick_get() + TEMPERATURE_PUB_INTERVAL;
                    // Remember last published value
                    sensor_last_published_temperature[index] = temperature;
                }
            }

            // going to fetch moisutre as raw capacitance
            // measured moisture can be retrieved in percentage but it's experimental
            // and it depends on sensor calibration
            int raw_cap;
            uint16_t raw_cap_u16;

            if (bc_soil_sensor_get_cap_raw(self, device_address, &raw_cap_u16))
            {
                bool publish = false;
                raw_cap = (int)raw_cap_u16;
                bc_log_debug("Soil moisture for sensor ID %llx: %u", device_address, raw_cap_u16);

                // Is time up to report sensor moisture?
                if (bc_tick_get() >= sensor_moisture_tick_report[index])
                {
                    // Publish message on radio
                    bc_log_debug("it's time to publish capacitance due to defined interval");
                    publish = true;
                }
                // Is sensor capacitance difference from last published value significant?
                else if (abs(raw_cap - sensor_last_published_moisture[index]) >= SENSOR_MOISTURE_PUB_DIFFERENCE)
                {
                    // Publish message on radio
                    bc_log_debug("it's time to publish capacitance as difference is significant: %d", abs(raw_cap - sensor_last_published_moisture[index]));
                    publish = true;
                }

                if (publish)
                {
                    snprintf(topic, sizeof(topic), "soil-sensor/%llx/moisture", device_address);

                    // Publish sensor moisture message on radio
                    bc_radio_pub_int(topic, &raw_cap);

                    // Schedule next moisture report
                    sensor_moisture_tick_report[index] = bc_tick_get() + MEASURE_MOISTURE_INTERVAL;

                    // Remember last published value
                    sensor_last_published_moisture[index] = raw_cap;
                }
            }
        }
        else if (event == BC_SOIL_SENSOR_EVENT_ERROR)
        {
            int error = bc_soil_sensor_get_error(self);
            bc_radio_pub_int("soil-sensor/-/error", &error);
        }
    }

    // switch from service mode to normal mode for BC soil moisture sensor
    void switch_to_normal_mode_bc_soil_sensor_task(void *param)
    {
        bc_soil_sensor_set_update_interval(&soil_sensor, SENSOR_UPDATE_NORMAL_INTERVAL);
        bc_scheduler_unregister(bc_scheduler_get_current_task_id());
    }
#else
    // Functions related to non BigClown moisture sensor
    // measure moisture and schedule jobs:
    // - to stop a measurement
    // - run next a measurement
    void _measure_moisture() {
        bc_gpio_set_output(SOIL_MOISTURE_POWER_ID, 1);
        bc_scheduler_plan_relative(tasks._stop_measuring_moisture_task_id, SOIL_MOISTURE_POWER_ON_INTERVAL);
        bc_scheduler_plan_relative(tasks._measure_moisture_id, MEASURE_MOISTURE_INTERVAL);
    }

    // get mositure, publish moisture to MQTT, stop moisture measurement
    void _stop_measuring_moisture() {
        static char topic[64];
        snprintf(topic, sizeof(topic), "soil-sensor/%d/moisture", SOIL_MOISTURE_PORT_ID);
        bc_log_debug("Publish topic: %s", topic);
        bc_adc_get_value(SOIL_MOISTURE_PORT_ID, &values.moisture);
        int moisture = values.moisture;
        // always publish moisture as it's never the same value
        bc_radio_pub_int(topic, &moisture);
        // power off port for a soil sensor
        bc_gpio_set_output(SOIL_MOISTURE_POWER_ID, 0);
        bc_log_debug("Soil moisture for sensor ID %d: %d", SOIL_MOISTURE_PORT_ID, values.moisture);
    }
#endif