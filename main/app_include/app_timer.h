/**
 * @file app_adc.h
 * @brief application specific definitions for UART2 operation on remote PCB for sound steering project.
 *        This UART port is intended for use only in development and is to be replaced with a wireless BT link.
 *        (courtesy of PRO_CPU :P)
 * 
 */

#ifndef APP_TIMER_H
#define APP_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

// --------------- Includes --------------- //
#include "driver/gptimer.h"
#include "app_utility.h"

#define GPTIMER_BASE_TICK_FREQ_HZ (uint32_t)(1 * 1000 * 1000) // Set 1MHz base resolution
#define GPTIMER_MS_TO_TICKS(X)    (uint32_t)((X / 1000.0) * GPTIMER_BASE_TICK_FREQ_HZ)
#define GPTIMER_TICKS_TO_MS(X)    (uint32_t)((X * 1000.0) / GPTIMER_BASE_TICK_FREQ_HZ)

//typedef void (*gptimer_callback_t) (void *);

typedef struct {
    gptimer_handle_t * timerHandle;
    bool running;
} gptimer_state_t;

esp_err_t app_initTimer(gptimer_handle_t * timerHandle, void (*cb)(int), uint16_t time_ms, bool oneShot);
esp_err_t app_toggleTimerRun(gptimer_handle_t * timerHandle, gptimer_state_t * state);
esp_err_t app_reconfigTimerAlarm(gptimer_handle_t * timerHandle, gptimer_state_t * state, uint16_t time_ms, bool oneShot);

#ifdef __cplusplus
}
#endif

#endif  // APP_TIMER_H