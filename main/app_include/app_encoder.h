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
#define MAX_ENCODER_COUNTS        23 + 1     // 24PPR encoders. 0-23.
#define ENC_QUEUE_DELAY           10

// Static functions

// User functions

#ifdef __cplusplus
}
#endif

#endif  // APP_UART2_H