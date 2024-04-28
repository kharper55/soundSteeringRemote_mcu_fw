/*
 * @file app_adc.c
 * @brief application code for adc operation of sound steering remote PCB.
 *
 */

#include "app_include/app_adc.h"

static const char * ADC_TAG = "ADC";

/*---------------------------------------------------------------
    ADC Calibration Init
---------------------------------------------------------------*/
/*static*/ bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, 
                                     adc_atten_t atten, adc_cali_handle_t *out_handle) {

    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated) {
        ESP_LOGI(ADC_TAG, "Calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }

    *out_handle = handle;

    ESP_LOGI(ADC_TAG, "Attempting to calibrate ADC%d channel %d", unit + 1, channel);

    if (ret == ESP_OK) {
        ESP_LOGI(ADC_TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(ADC_TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(ADC_TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

/*---------------------------------------------------------------
    ADC Calibration Deinit
---------------------------------------------------------------*/
/*static*/ void adc_calibration_deinit(adc_cali_handle_t handle) {

    ESP_LOGI(ADC_TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
}

/*---------------------------------------------------------------
    ADC Unit Oneshot Read Init
---------------------------------------------------------------*/
/*static*/ void adc_oneshot_init(adc_oneshot_unit_handle_t * adc_handle, adc_unit_t unit, 
                             adc_channel_t channel) {

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = unit,
    };

    if (unit == ADC_UNIT_2) {
        init_config.ulp_mode = ADC_ULP_MODE_DISABLE;
    }

    adc_oneshot_new_unit(&init_config, adc_handle);

    // Attenuation to 0-1.1V range handled in hardware for all channels
    adc_oneshot_chan_cfg_t adc_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_APP_ATTEN,
    };
    adc_oneshot_config_channel(*adc_handle, channel, &adc_config);

}

/*---------------------------------------------------------------
    ADC Unit Continuous Read Init
---------------------------------------------------------------*/
/*static*/ void adc_continuous_init(adc_continuous_handle_t * adc_handle, adc_unit_t * units, 
                                adc_channel_t * channels, uint8_t num_channels) {

    /* Needs to be ported in */
}