/*
 * @file app_adc.c
 * @brief application code for adc operation of sound steering remote PCB.
 *
 */

#include "app_include/app_uart2.h"

static const char * UART2_TAG = "UART2";

/*---------------------------------------------------------------
    UART2 peripheral initialization
---------------------------------------------------------------*/
void uart2_init(int baud) {
    const uart_config_t uart_config = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_2, RX_BUF_SIZE, TX_BUF_SIZE, 0, NULL, 0);
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, U2TXD_PIN, U2RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}