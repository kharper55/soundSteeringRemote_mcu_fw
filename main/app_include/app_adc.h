/**
 * @file app_adc.h
 * @brief application specific definitions for adc operation of remote PCB for sound steering project.
 * 
 */

#ifndef APP_ADC_H
#define APP_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

// --------------- Includes --------------- //
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

// ADC INPUTS     
#define VBATT_PIN                        GPIO_NUM_25 // ADC28 or ADC17 depending on R42 placement
#define ADC2_CHAN0                       ADC_CHANNEL_8
#define POTC_PIN                         GPIO_NUM_34 // ADC16
#define ADC1_CHAN1                       ADC_CHANNEL_6
#define POTD_PIN                         GPIO_NUM_36 // ADC10 (SENSOR_VP)
#define ADC1_CHAN0                       ADC_CHANNEL_0

// Configurations
#define ADC_APP_ATTEN                    ADC_ATTEN_DB_0
#define EXAMPLE_ADC_GET_CHANNEL(p_data)  ((p_data)->type1.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)     ((p_data)->type1.data)

// Settings
#define SCALE_VBAT(X)             (float)(X * 4 * 0.001)
#define PCT_MIN                   0
#define PCT_MAX                   100
#define ADC_MAX                   1100
#define ADC_MIN                   100
#define SCALE_VPOT(X)             (int)((((X - ADC_MIN) * (PCT_MAX - PCT_MIN))/(float)(ADC_MAX - ADC_MIN)) + PCT_MIN)
#define SCALE_VPOT_INVERT(X)      (int)(PCT_MAX - (((X - ADC_MIN) * (PCT_MAX - PCT_MIN))/(float)(ADC_MAX - ADC_MIN)))

typedef struct {
    int count;
    int sum;
    int buff[10];
    int buff_len;
    bool bufferFullFlag;
} adc_filter_t;
//try out adjustable length for the above so smaller buffers have no need to pass so much info 

typedef struct {
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
} adcOneshotParams_t;

//info
/*
adc_oneshot_unit_handle_t adc1_handle = NULL;
    adc_cali_handle_t adc1_cali_chan0_handle = NULL;
    adc_cali_handle_t adc1_cali_chan1_handle = NULL;

    adc_oneshot_unit_handle_t adc2_handle = NULL;
    adc_cali_handle_t adc2_cali_handle = NULL;

    adc_oneshot_init(&adc1_handle, ADC_UNIT_1, ADC1_CHAN0); // VPOTD adc10
    adc_calibration_init(ADC_UNIT_1, ADC1_CHAN0, ADC_APP_ATTEN, &adc1_cali_chan0_handle);

    adc_oneshot_init(&adc2_handle, ADC_UNIT_2, ADC2_CHAN0); // VBAT adc28 or adc17
    adc_calibration_init(ADC_UNIT_2, ADC2_CHAN0, ADC_APP_ATTEN, &adc2_cali_handle);

    adc_oneshot_init(&adc1_handle, ADC_UNIT_1, ADC1_CHAN1); // VPOTC adc16
    adc_calibration_init(ADC_UNIT_1, ADC1_CHAN1, ADC_APP_ATTEN, &adc1_cali_chan1_handle);

*/


/*static*/ bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, 
                                 adc_atten_t atten, adc_cali_handle_t *out_handle);

/*static*/ void adc_calibration_deinit(adc_cali_handle_t handle);

/*static*/ void adc_oneshot_init(adc_oneshot_unit_handle_t * adc_handle, adc_unit_t unit, 
                             adc_channel_t channel);

/*static*/ void adc_continuous_init(adc_continuous_handle_t * adc_handle, adc_unit_t * units, 
                                adc_channel_t * channels, uint8_t num_channels);

float adc_filter(int value, adc_filter_t * filterObject);

void app_adc_init();

#ifdef __cplusplus
}
#endif

#endif  // APP_ADC_H