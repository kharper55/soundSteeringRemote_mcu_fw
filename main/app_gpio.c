#include "app_include/app_gpio.h"

static const char * GPIO_TAG = "GPIO";

const char * gpio_status_names[2] = {
    "LOW",
    "HIGH"
};

int keypress_combos[NUM_COMBOS][KEYPRESS_COMBO_LENGTH] = {
    {0, 0, 0},
    {0, 0, 1},
    {0, 1, 0},
    {0, 1, 1},
    {1, 0, 0},
    {1, 0, 1},
    {1, 1, 0},
    {1, 1, 1},
};

const char * keypress_combo_names[NUM_COMBOS] = {
    "AAA",
    "AAB"
    "ABA"
    "ABB"
    "BAA"
    "BAB"
    "BBA"
    "BBB"
};

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

/*---------------------------------------------------------------
    Circular buffer stuff for registering more key combinations
---------------------------------------------------------------*/
// Initialize the circular buffer
void init_buffer(circularBuffer *cb) {
    for (int i = 0; i < KEYPRESS_COMBO_LENGTH; i++) {
        cb->buffer[i] = 0;
    }
    cb->head = 0;
    cb->tail = 0;
    cb->mutex = xSemaphoreCreateMutex();  // Create mutex
    cb->size = KEYPRESS_COMBO_LENGTH;
}

/*---------------------------------------------------------------
    Circular buffer stuff for registering more key combinations
---------------------------------------------------------------*/
void clear_buffer(circularBuffer *cb) {
    xSemaphoreTake(cb->mutex, COMBO_CHECK_DELAY);  // Take mutex before accessing buffer
    for (int i = 0; i < KEYPRESS_COMBO_LENGTH; i++) {
        cb->buffer[i] = 0;
    }
    cb->head = 0;
    cb->tail = 0;
    xSemaphoreGive(cb->mutex);  // Release mutex after accessing buffer
}

/*---------------------------------------------------------------
    Circular buffer stuff for registering more key combinations
---------------------------------------------------------------*/
// Push a key into the circular buffer
void push_key(circularBuffer *cb, int key) {
    xSemaphoreTake(cb->mutex, COMBO_CHECK_DELAY);  // Take mutex before accessing buffer
    cb->buffer[cb->tail] = key;
    cb->tail = (cb->tail + 1) % cb->size;
    if (cb->tail == cb->head) {
        cb->head = (cb->head + 1) % cb->size; // Increment head to drop oldest element
    }
    xSemaphoreGive(cb->mutex);  // Release mutex after accessing buffer
}

/*---------------------------------------------------------------
    Circular buffer stuff for registering more key combinations
---------------------------------------------------------------*/
// Pop a key from the circular buffer
int pop_key(circularBuffer *cb) {
    xSemaphoreTake(cb->mutex, COMBO_CHECK_DELAY);  // Take mutex before accessing buffer
    if (cb->head == cb->tail) {
        xSemaphoreGive(cb->mutex);  // Release mutex if buffer is empty
        return -1;
    }
    int key = cb->buffer[cb->head];
    cb->head = (cb->head + 1) % cb->size;
    xSemaphoreGive(cb->mutex);  // Release mutex after accessing buffer
    return key;
}

/*---------------------------------------------------------------
    Circular buffer stuff for registering more key combinations
---------------------------------------------------------------*/
bool check_combo(circularBuffer *cb, int *target) {

    static const bool VERBOSE = false;

    xSemaphoreTake(cb->mutex, COMBO_CHECK_DELAY);  // Take mutex before accessing buffer
    int target_index = 0;
    int buffer_index = cb->head;
    for (int i = 0; i < cb->size; i++) {
        if (VERBOSE) ESP_LOGI(GPIO_TAG, "Buffer[%d]: %d, Target[%d]: %d", buffer_index, cb->buffer[buffer_index], target_index, target[target_index]);
        if (cb->buffer[buffer_index] == target[target_index]) {
            target_index++;
            if (target_index == cb->size) {
                xSemaphoreGive(cb->mutex);  // Release mutex before returning
                return true; // Full combo matched
            }
        } else {
            target_index = 0;
        }
        buffer_index = (buffer_index + 1) % cb->size;
    }
    xSemaphoreGive(cb->mutex);  // Release mutex before returning
    return false; // Combo not found
}