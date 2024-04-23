/**
 * @file app_adc.h
 * @brief application specific definitions for UART2 operation on remote PCB for sound steering project.
 *        This UART port is intended for use only in development and is to be replaced with a wireless BT link.
 *        (courtesy of PRO_CPU :P)
 * 
 */

#ifndef APP_UART2_H
#define APP_UART2_H

#ifdef __cplusplus
extern "C" {
#endif

// --------------- Includes --------------- //
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

// UART0 setup is taken care of at startup and is used by the log library
// This can be changed via menuconfig

// UART2 (Development link to controller) on dedicated UART2 pins (UART 1 is still available)
#define U2TXD_PIN               GPIO_NUM_17
#define U2RXD_PIN               GPIO_NUM_16

#define U2_BAUD                      115200
#define RX_BUF_SIZE         (const int) 512
#define TX_BUF_SIZE         (const int) 512

void uart2_init(int baud);

#ifdef __cplusplus
}
#endif

#endif  // APP_UART2_H