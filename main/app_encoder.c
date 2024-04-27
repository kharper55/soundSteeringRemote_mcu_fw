#include "app_include/app_encoder.h"

/*---------------------------------------------------------------
    Rotary Encoder initialization function
---------------------------------------------------------------*/
void encoder_init(rotary_encoder_info_t * encoder, gpio_num_t cha_pin, gpio_num_t chb_pin, 
                         gpio_num_t sw_pin, bool en_half_steps, bool flip_dir) {

    esp_err_t err;
    // esp32-rotary-encoder requires that the GPIO ISR service is installed before calling rotary_encoder_register()
    gpio_install_isr_service(0);    // This function has protection around it to ensure that it cannot be called over itself

    // Initialise the rotary encoder device with the GPIOs for A and B signals
    ESP_ERROR_CHECK(rotary_encoder_init(encoder, cha_pin, chb_pin, sw_pin));
    ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(encoder, en_half_steps));
    if (flip_dir) {
        ESP_ERROR_CHECK(rotary_encoder_flip_direction(encoder));
    }
}