/**
 * @file app_adc.h
 * @brief application specific definitions for UART2 operation on remote PCB for sound steering project.
 *        This UART port is intended for use only in development and is to be replaced with a wireless BT link.
 *        (courtesy of PRO_CPU :P)
 * 
 */

#ifndef APP_ENCODER_H
#define APP_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

// --------------- Includes --------------- //
#include "rotary_encoder.h"

// ENCODER/SWITCH INPUTS -- ENCA is for menu navigation in addition to steering
#define ENCA_CHA_PIN            GPIO_NUM_27 
#define ENCA_CHB_PIN            GPIO_NUM_14  
#define ENCA_SW_PIN             GPIO_NUM_33 
#define ENCB_CHA_PIN            GPIO_NUM_13
#define ENCB_CHB_PIN            GPIO_NUM_4 
#define ENCB_SW_PIN             GPIO_NUM_32

// Settings
#define MAX_ENCODER_COUNTS      (int8_t)(30)          // 24PPR encoders. 0-23 WOULDA BEEN NICE but we NEEED angles here baby (excuse to utilize all BRAMs on the FPGA tehe)
#define MIN_ENCODER_COUNTS      -MAX_ENCODER_COUNTS   // 24PPR encoders. 0-23.
#define ENC_QUEUE_DELAY         10

// Define a structure to hold both encoder object and queue handle
typedef struct {
    char * TAG;
    rotary_encoder_info_t * encoder;
    rotary_encoder_event_t * event;
    rotary_encoder_state_t * state;
    gpio_num_t pinA;
    gpio_num_t pinB;
    gpio_num_t pinSW;
    QueueHandle_t queue;
} encParams_t;

// Static functions

// User functions
void encoder_init(rotary_encoder_info_t * encoder, gpio_num_t cha_pin, gpio_num_t chb_pin, 
                         gpio_num_t sw_pin, bool en_half_steps, bool flip_dir);

#ifdef __cplusplus
}
#endif

#endif  // APP_UART2_H