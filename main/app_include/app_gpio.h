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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// DIGITAL OUTPUTS
#define LOWBATT_LED_PIN         GPIO_NUM_2
#define HEARTBEAT_LED_PIN       GPIO_NUM_26
#define OPAMP_LOWPWR_ENA_PIN    GPIO_NUM_12

#define HEARTBEAT_BLINK_PERIOD_MS 100

#define KEYPRESS_COMBO_LENGTH   3
#define NUM_COMBOS              8
#define COMBO_CHECK_DELAY       10

// Typedefs
typedef enum {
    LOW,
    HIGH
} gpio_status_t;

typedef struct {
    int buffer[KEYPRESS_COMBO_LENGTH];  // Buffer to store keypresses
    int head;  // Index of the first element in the buffer
    int tail;  // Index of the next empty slot in the buffer
    SemaphoreHandle_t mutex;  // Mutex to protect buffer access
    int size;  // Size of the circular buffer
} circularBuffer;

extern const char * gpio_status_names[2];
extern int keypress_combos[NUM_COMBOS][KEYPRESS_COMBO_LENGTH];
extern const char * keypress_combo_names[NUM_COMBOS];

void app_gpio_init(void);
void init_buffer(circularBuffer *cb);
void push_key(circularBuffer *cb, int key);
int pop_key(circularBuffer *cb);
bool check_combo(circularBuffer *cb, int * target);

#ifdef __cplusplus
}
#endif

#endif  // APP_GPIO_H