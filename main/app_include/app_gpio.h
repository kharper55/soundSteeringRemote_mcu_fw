/**
 * @file app_adc.h
 * @brief application specific definitions for UART2 operation on remote PCB for sound steering project.
 *        This UART port is intended for use only in development and is to be replaced with a wireless BT link.
 *        (courtesy of PRO_CPU :P)
 * 
 */

#ifndef APP_GPIO_H
#define APP_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

// --------------- Includes --------------- //
#include "driver/gpio.h"
#include "esp_log.h"

// DIGITAL OUTPUTS
#define LOWBATT_LED_PIN         GPIO_NUM_2
#define HEARTBEAT_LED_PIN       GPIO_NUM_26
#define OPAMP_LOWPWR_ENA_PIN    GPIO_NUM_12

#define HEARTBEAT_BLINK_PERIOD_MS 100

static void gpio_pin_init(uint32_t gpio_num, gpio_mode_t direction);
void app_gpio_init(void);

#ifdef __cplusplus
}
#endif

#endif  // APP_GPIO_H