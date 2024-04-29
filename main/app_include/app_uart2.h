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
#include "driver/adc.h"
#include "app_include/app_adc.h"

// UART0 setup is taken care of at startup and is used by the log library
// This can be changed via menuconfig

// UART2 (Development link to controller) on dedicated UART2 pins (UART 1 is still available)
#define U2TXD_PIN               GPIO_NUM_17
#define U2RXD_PIN               GPIO_NUM_16

#define U2_BAUD                      115200
#define RX_BUF_SIZE         (const int) 512
#define TX_BUF_SIZE         (const int) 512

typedef enum serial_cmds_t {
    NOP                      = 0x0,
    TOGGLE_ON_OFF            = 0x2,  // Hex code for togglining device on/off (i.e. power to the array)
    CHANGE_CHANNEL           = 0x4,  // Hex code for changing only channel with one transaction
    CHANGE_COORD             = 0x8,  // Hex code for changing only coordinate with one transaction
    CHANGE_VOLUME            = 0xA,  // Hex code for changing only volume with one transaction
    CHANGE_COORD_AND_VOLUME  = 0xC,  // Hex code for changing volume, channel, and coordinate with one transaction
    REQUEST_INFO             = 0xE   // Hex code for requesting readback from the device
};

typedef struct tx_task_parms_t {
    char * TAG;
    adc_oneshot_unit_handle_t * handle;
    adc_cali_handle_t * cali_handle;
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t atten;
    adc_filter_t * filt;
    int delay_ms;
    int * vraw;
    int * vcal;
    int * vfilt;
};

void uart2_init(int baud);

#ifdef __cplusplus
}
#endif

#endif  // APP_UART2_H