#include "app_include/app_gpio.h"

static const char * GPIO_TAG = "GPIO";

/*---------------------------------------------------------------
    GPIO pin configuration function
---------------------------------------------------------------*/
static void gpio_pin_init(uint32_t gpio_num, gpio_mode_t direction) {
    gpio_reset_pin(gpio_num);
    gpio_set_direction(gpio_num, direction);
    ESP_LOGI(GPIO_TAG, "Pin no. %d configured as output\n", (uint8_t)gpio_num);
}

/*---------------------------------------------------------------
    Application specific GPIO init function
---------------------------------------------------------------*/
void app_gpio_init(void) {
    // GPIO Outputs
    gpio_pin_init(HEARTBEAT_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_pin_init(LOWBATT_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_pin_init(OPAMP_LOWPWR_ENA_PIN, GPIO_MODE_OUTPUT);

}