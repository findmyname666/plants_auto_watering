#include <application.h>

#define MEASURE_HUMIDITY_DELAY 60000

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
void battery_event_handler(bc_module_battery_event_t event, void *event_param);

void _start_water_pump();
void _stop_water_pump();
void _measure_humidity_task();
void _stop_measuring_humidity_task();

// exec watering
void exec_watering(uint64_t *id, const char *topic, void *value, void *param);

// subscribe table, format: topic, expect payload type, callback, user param
static const bc_radio_sub_t subs[] = {
    // state/set
    {"watering/basil/pump/start", BC_RADIO_SUB_PT_NULL, exec_watering, NULL }
};

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

void application_init(void)
{
    // Initialize logging
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize radio
    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
    //bc_radio_init(BC_RADIO_MODE_NODE_LISTENING);
    bc_radio_pairing_request(FIRMWARE, VERSION);
    // subscribe to topic
    bc_radio_set_subs((bc_radio_sub_t *) subs, sizeof(subs)/sizeof(bc_radio_sub_t));
    bc_radio_set_rx_timeout_for_sleeping_node(50);

    // Initialize battery module
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // initialize P0 for a transistor/switch to turn on water pump
    bc_gpio_init(WATER_PUMP_POWER_ID);
    bc_gpio_set_mode(WATER_PUMP_POWER_ID, BC_GPIO_MODE_OUTPUT);
    // bc_gpio_set_output(WATER_PUMP_POWER_ID, 1);
    // create tasks - stop water pump
    watering._stop_water_pump_task_id = bc_scheduler_register(_stop_water_pump, NULL, 100);
    // initialize power for soil sensor
    bc_gpio_init(SOIL_HUM_BASIL_POWER_ID);
    bc_gpio_set_mode(SOIL_HUM_BASIL_POWER_ID, BC_GPIO_MODE_OUTPUT);
    // initialization of analog sensors
    bc_adc_init();
    // measure soil humidity
    watering._measured_humidity = 0;
    watering._measure_humidity_task_id = bc_scheduler_register(_measure_humidity_task, NULL, 1000);
    watering._stop_measuring_humidity_task_id = bc_scheduler_register(_stop_measuring_humidity_task, NULL, 1000);
}

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

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    float voltage;
    int percentage;

    if (bc_module_battery_get_voltage(&voltage))
    {
        battery_values.battery_voltage = voltage;
        bc_radio_pub_battery(&voltage);
    }

    //if (bc_module_battery_get_charge_level(&percentage))
    //{
    //    battery_values.battery_pct = percentage;
    //    bc_radio_pub_battery(&percentage);
    //}
}

// Tasks
void _start_water_pump() {
    bc_gpio_set_output(WATER_PUMP_POWER_ID, 1);
    bc_scheduler_plan_relative(watering._stop_water_pump_task_id, 6000);
}

void _stop_water_pump() {
    bc_gpio_set_output(WATER_PUMP_POWER_ID, 0);
}

void _measure_humidity_task() {
    bc_gpio_set_output(SOIL_HUM_BASIL_POWER_ID, 1);
    bc_scheduler_plan_relative(watering._stop_measuring_humidity_task_id, 25);
    bc_scheduler_plan_relative(watering._measure_humidity_task_id, MEASURE_HUMIDITY_DELAY);
}

void _stop_measuring_humidity_task() {
    bc_adc_get_value(SOIL_HUM_BASIL_PORT_ID, &watering._measured_humidity);
    int humidity = watering._measured_humidity;
    bc_radio_pub_int("soil_humidity/basil", &humidity);
    // power off port for soil sensor
    bc_gpio_set_output(SOIL_HUM_BASIL_POWER_ID, 0);
}

void exec_watering(uint64_t *id, const char *topic, void *value, void *param)
{
    bc_log_debug("starting watering");
    bc_radio_pub_string("water/-/pump/state", "on");
    _start_water_pump();
    bc_log_debug("watering done");
    bc_radio_pub_string("water/-/pump/state", "off");
}

void application_task(void)
{
    // Logging in action
    //bc_log_debug("application_task run");
    bc_log_debug("Soil humidity: %d", watering._measured_humidity);

    // Plan next run this function after 3000 ms
    bc_scheduler_plan_current_from_now(5000);
}
