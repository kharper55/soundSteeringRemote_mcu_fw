#include "app_include/app_timer.h"

gptimer_state_t GPTIMER_DEFAULT_STATE = {
    .timerHandle = -1,
    .running = false
};

esp_err_t app_reconfigTimerAlarm(gptimer_handle_t * timerHandle, gptimer_state_t * state, uint16_t time_ms, bool oneShot) {

    esp_err_t ret = ESP_OK;

    if (state->running && state) { // Don't allow a reconfiguration if the timer is currently running...
        ret = ESP_FAIL;
    }
    else {
        gptimer_alarm_config_t gptimer_config = {
            .alarm_count = GPTIMER_MS_TO_TICKS(time_ms),
            .reload_count = 0,
            .flags.auto_reload_on_alarm = false ? oneShot : true
        };

        ret = gptimer_set_alarm_action(*timerHandle, &gptimer_config);
    }

    if (ret != ESP_OK) {
        ESP_LOGI((const char *) "DEBUG", "Couldn't initialize timer!");
    }
    else {
       ESP_LOGI((const char *) "DEBUG", "Time intialized!"); 
    }

    return ret;
}

// Initialize a timer with a predeclared NULL timerHandle. Allows a user to specify a callback in the main application source file 
esp_err_t app_initTimer(gptimer_handle_t * timerHandle, void (*cb)(int), uint16_t time_ms, bool oneShot) {

    esp_err_t ret = ESP_OK;

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = GPTIMER_BASE_TICK_FREQ_HZ, // 1MHz, 1 tick = 1us
    };
    
    ret = gptimer_new_timer(&timer_config, timerHandle);

    /*
    gptimer_alarm_config_t gptimer_alarm_config = {
        .alarm_count = GPTIMER_MS_TO_TICKS(time_ms),
        .reload_count = 0,
        .flags.auto_reload_on_alarm = false ? oneShot : true
    };
    ret = gptimer_set_alarm_action(*timerHandle, &gptimer_alarm_config);
    */
    ret = app_reconfigTimerAlarm(timerHandle, &GPTIMER_DEFAULT_STATE, time_ms, oneShot);
    
    gptimer_event_callbacks_t cbs = {
        .on_alarm = cb, // register user callback
    };

    ret = gptimer_register_event_callbacks(*timerHandle, &cbs, NULL);
    ret = gptimer_enable(*timerHandle);
    return ret;
}

esp_err_t app_toggleTimerRun(gptimer_handle_t * timerHandle, gptimer_state_t * state) {

    esp_err_t ret = ESP_OK;

    if (state->running == true) {
        ret = gptimer_stop(*timerHandle);
        state->running = false;
    }
    else {
        ret = gptimer_start(*timerHandle);
        state->running = true;
    }

    return ret;
}

esp_err_t app_setTimerCount(gptimer_handle_t * timerHandle, uint64_t value) {

    esp_err_t ret = ESP_OK;
    ret = gptimer_set_raw_count(*timerHandle, value);

    return ret;
}

esp_err_t app_getTimerCount(gptimer_handle_t * timerHandle, uint64_t * value) {

    esp_err_t ret = ESP_OK;
    ret = gptimer_get_raw_count(*timerHandle, *value);

    return ret;
}

