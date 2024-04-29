#include "app_include/app_gpio.h"

static const char * GPIO_TAG = "GPIO";

const char * gpio_status_names[2] = {
    "LOW",
    "HIGH"
};

const uint8_t keypress_combos[KEYPRESS_COMBO_LENGTH][NUM_COMBOS] = {
    {0, 0, 0},
    //{1, 0, 0},
    //{0, 1, 1}
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
    //"BAA",
    //"ABB"
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
    Circular Buffer Stuff
---------------------------------------------------------------*/
// Initialize the circular buffer
void init_buffer(circularBuffer *cb) {
    cb->head = 0;
    cb->tail = 0;
    cb->mutex = xSemaphoreCreateMutex();  // Create mutex
}

/*---------------------------------------------------------------
    Circular Buffer Stuff
---------------------------------------------------------------*/
// Push a key into the circular buffer
void push_key(circularBuffer *cb, uint8_t key) {
    xSemaphoreTake(cb->mutex, COMBO_CHECK_DELAY);  // Take mutex before accessing buffer
    cb->buffer[cb->tail] = key;
    cb->tail = (cb->tail + 1) % KEYPRESS_COMBO_LENGTH;
    if (cb->tail == cb->head) {
        cb->head = (cb->head + 1) % KEYPRESS_COMBO_LENGTH;
    }
    xSemaphoreGive(cb->mutex);  // Release mutex after accessing buffer
}


/*---------------------------------------------------------------
    Circular Buffer Stuff
---------------------------------------------------------------*/
// Pop a key from the circular buffer
uint8_t pop_key(circularBuffer *cb) {
    xSemaphoreTake(cb->mutex, COMBO_CHECK_DELAY);  // Take mutex before accessing buffer
    if (cb->head == cb->tail) {
        xSemaphoreGive(cb->mutex);  // Release mutex if buffer is empty
        return -1;
    }
    uint8_t key = cb->buffer[cb->head];
    cb->head = (cb->head + 1) % KEYPRESS_COMBO_LENGTH;
    xSemaphoreGive(cb->mutex);  // Release mutex after accessing buffer
    return key;
}

/*---------------------------------------------------------------
    Circular Buffer Stuff
---------------------------------------------------------------*/
// Check if a certain key combination exists in the circular buffer
bool check_combo(circularBuffer *cb, uint8_t * target) {
    xSemaphoreTake(cb->mutex, COMBO_CHECK_DELAY);  // Take mutex before accessing buffer
    int target_index = 0;
    for (int i = cb->head; i != cb->tail; i = (i + 1) % KEYPRESS_COMBO_LENGTH) {
        if (cb->buffer[i] == target[target_index]) {
            target_index++;
            if (target_index == KEYPRESS_COMBO_LENGTH) {
                xSemaphoreGive(cb->mutex);  // Release mutex before returning
                return true; // Full combo matched
            }
        } else {
            target_index = 0;
        }
    }
    xSemaphoreGive(cb->mutex);  // Release mutex before returning
    return false; // Combo not found
}
