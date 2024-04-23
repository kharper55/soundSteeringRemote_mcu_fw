/*===================================================================================================
    Ultrasonic Drive Remote - Code to implement remote for phased array in order to demonstrate
                              the phenomena of "sound from ultrasound".
    Created By Kevin Harper, 01-08-2024 
    
    Written using ESP-IDF v5.1.1 API
    [1/3] espressif/led_strip (2.5.3)
    [2/3] idf (5.1.2)
    [3/3] lvgl/lvgl (8.3.11)
    LVGL HAL for ESP32... 
    rotary encoder support/drivers... 

    https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/index.html
    https://docs.lvgl.io/8.3/index.html
    https://github.com/lvgl/lvgl_esp32_drivers (these were ported from an existing fork, see VitorAlho on github)
    https://github.com/DavidAntliff/esp32-rotary-encoder
//==================================================================================================*/

/*============================================= INCLUDES ===========================================*/

#include "app_include/app_utility.h"    /* Application universal includes and some utility functions */
#include "app_include/app_adc.h"        /* ADC driver application specific code */
#include "app_include/app_uart2.h"      /* UART2 driver application specific code */
#include "app_include/app_gpio.h"       /* GPIO driver specific code */
#include "app_include/app_spi.h"    /* SPI driver application specific code and LVGL related */
#include "app_include/app_encoder.h"    /* Rotary encoder driver application specific code */

/*========================== CONSTANTS, MACROS, AND VARIABLE DECLARATIONS ==========================*/

#define ANIM_PERIOD_MS            7000
#define APP_ARC_SIZE              50
#define SCALE_VBAT(X)             (float)(X * 4 * 0.001)
#define PCT_MIN                   0
#define PCT_MAX                   100
#define ADC_MAX                   1100
#define ADC_MIN                   100
#define SCALE_VPOT(X)             (int)((((X - ADC_MIN) * (PCT_MAX - PCT_MIN))/(float)(ADC_MAX - ADC_MIN)) + PCT_MIN)
#define SCALE_VPOT_INVERT(X)      (int)(PCT_MAX - (((X - ADC_MIN) * (PCT_MAX - PCT_MIN))/(float)(ADC_MAX - ADC_MIN)));

/* Global Vars */
typedef enum {
    FULL_ARC = 0,
    PARTIAL_ARC
} app_arc_t;

typedef enum {
    ARC0 = 0,
    ARC1,
    ARC2,
    ARC3
} app_arc_names_t;

typedef enum {
    BAT_EMPTY = -1,
    BAT_LOW = 0,
    BAT_MED,
    BAT_FULL
} battery_states_t;

static battery_states_t batteryState = BAT_LOW;

uint32_t knobColors[2] = {APP_COLOR_UNH_GOLD, APP_COLOR_RED}; // SWITCH RELEASED COLOR , SWITCH PRESSED COLOR
uint32_t batteryColors[3] = {APP_COLOR_RED, APP_COLOR_UNH_GOLD, APP_COLOR_PCB_GREEN_ALT};

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;
lv_obj_t * arc0;
lv_obj_t * arc1;
lv_obj_t * arc2;
lv_obj_t * arc3;
QueueHandle_t xEncoderAQueue;
QueueHandle_t xEncoderBQueue;
rotary_encoder_info_t encA = { 0 };
rotary_encoder_info_t encB = { 0 };
rotary_encoder_event_t eventA = { 0 };
rotary_encoder_event_t eventB = { 0 };
rotary_encoder_state_t stateA = { 0 };
rotary_encoder_state_t stateB = { 0 };
int posA = 0;
int posB = 0;
int posC = 0;
int posD = 0;

lv_obj_t * vbatLabel;
static lv_style_t vbatLabel_style;
int vbat_raw = 0;
int vbat_cali = 0;
float vbat = 0;

lv_obj_t * chanSelect_label;

bool activeImage = true;
lv_obj_t * app_images[2];

int vpotd = 0;
int vpotd_raw = 0;
int vpotd_cali = 0;
int vpotd_pct = 0;
int vpotc = 0;
int vpotc_raw = 0;
int vpotc_cali = 0;
int vpotc_pct = 0;

typedef struct {
    int count;
    int sum;
    int buff[10];
    int buff_len;
    bool bufferFullFlag;
} adc_filter_t;
//try out adjustable length for the above so smaller buffers have no need to pass so much info 

adc_filter_t vbat_filt = {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 10, false};
adc_filter_t vpotd_filt = {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 10, false};
adc_filter_t vpotc_filt = {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 10, false}; // Try automatically getting length with sizeof(arr)/sizeof(firstEl)(this requires predeclaring an array and passing to the struct which I am not sure is possible)

disp_backlight_h bl;

/*================================ FUNCTION AND TASK DEFINITIONS ===================================*/

/*---------------------------------------------------------------
    UART2 TX FreeRTOS task
---------------------------------------------------------------*/
static void txTask(void *arg) {

    static const char * TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    const char * data = "Poop!";

    while (1) {
        //if (xQueueReceive(sendData(TX_TASK_TAG, "Hello world") == pdTrue)) {}
        const int len = strlen(data);
        const int txBytes = uart_write_bytes(UART_NUM_2, data, len);
        ESP_LOGI(TX_TASK_TAG, "Wrote %d bytes", txBytes);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

/*---------------------------------------------------------------
    UART2 RX FreeRTOS task
---------------------------------------------------------------*/
static void rxTask(void *arg) {

    static const char * RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);

    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_2, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }

    free(data);
}

/*---------------------------------------------------------------
    ADC Filter, Circular Buffer Averaging Function
---------------------------------------------------------------*/
// Refactored to use a custom struct type containing previously static defined information
static float adc_filter(int value, adc_filter_t * filterObject) {

    int denominator = 1;
    int temp = 0;

   if (filterObject->count < filterObject->buff_len) {
        if (filterObject->bufferFullFlag) temp = filterObject->buff[filterObject->count]; // Get the current value in the circular buffer about to be overwritten...
        filterObject->buff[filterObject->count] = value;   
        filterObject->sum += (filterObject->buff[filterObject->count] - temp); // Add to the running sum of circular buffer entries the difference between the newest value added and the value that was previously in its position, so that additions dont need to be recompleted.
        filterObject->count++;
    }

    if (filterObject->count == filterObject->buff_len) {
        if (!filterObject->bufferFullFlag) filterObject->bufferFullFlag = true;
        filterObject->count = 0;
    }

    denominator = (filterObject->bufferFullFlag) ? filterObject->buff_len : filterObject->count;

    return (filterObject->sum / (float)denominator);
}

/*---------------------------------------------------------------
    ADC Oneshot Spurious Read Task (All 3 channels)
---------------------------------------------------------------*/
static void adcTask(void) {

    const static char * VBAT_TAG = "VBAT";
    const static char * POTC_TAG = "POTC";
    const static char * POTD_TAG = "POTD";

    const bool VERBOSE_FLAG = false;

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

    while (1) {
        
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, ADC2_CHAN0, &vbat_raw));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_handle, vbat_raw, &vbat_cali));
        vbat = SCALE_VBAT(adc_filter(vbat_cali, &vbat_filt));
        if (VERBOSE_FLAG) {
            ESP_LOGI(VBAT_TAG, "ADC%d_%d filt: %fV", ADC_UNIT_2 + 1, ADC2_CHAN0, SCALE_VBAT(vbat));
        }

        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN0, &vpotd_raw));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, vpotd_raw, &vpotd_cali));
        vpotd = adc_filter(vpotd_cali, &vpotd_filt);
        vpotd_pct = SCALE_VPOT_INVERT(vpotd);
        // Issue!!! static vars in similar function call lead to problems...
        // factor out that buffer full flag and keep track? or ahve on per chann1
        if (VERBOSE_FLAG) {
            ESP_LOGI(POTD_TAG, "ADC%d_%d filt: %dmV", ADC_UNIT_1 + 1, ADC1_CHAN0, vpotd);
            ESP_LOGI(POTD_TAG, "ADC%d_%d pct: %d%%", ADC_UNIT_1 + 1, ADC1_CHAN0, vpotd_pct);
        }

        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN1, &vpotc_raw));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan1_handle, vpotc_raw, &vpotc_cali));
        vpotc = adc_filter(vpotc_cali, &vpotc_filt);
        vpotc_pct = SCALE_VPOT_INVERT(vpotc);
        if (VERBOSE_FLAG) {
            ESP_LOGI(POTC_TAG, "ADC%d_%d filt: %dmV", ADC_UNIT_1 + 1, ADC1_CHAN1, vpotc);
            ESP_LOGI(POTC_TAG, "ADC%d_%d filt: %d%%", ADC_UNIT_1 + 1, ADC1_CHAN1, vpotc_pct);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc2_handle));

    adc_calibration_deinit(adc1_cali_chan0_handle);
    adc_calibration_deinit(adc1_cali_chan1_handle);
    adc_calibration_deinit(adc2_cali_handle);

}

/*---------------------------------------------------------------
    LCD object x-position set. Used for scrolling LVGL objects.
---------------------------------------------------------------*/
static void set_x(void * var, int32_t v) {
    lv_obj_set_x(var, v);
}

/*---------------------------------------------------------------
    LCD arc objects for encoders and potentiometers.
---------------------------------------------------------------*/
lv_obj_t * app_arc_create(lv_obj_t * comp_parent, app_arc_t arc_t, lv_coord_t r, 
                          lv_align_t align, lv_coord_t x_offs, lv_coord_t y_offs) {

    lv_obj_t * newArc;
    const void * tempImg;
    newArc = lv_arc_create(comp_parent);

    lv_obj_set_width(newArc, r);
    lv_obj_set_height(newArc, r);
    lv_obj_align(newArc, align, x_offs, y_offs);
    lv_obj_clear_flag(newArc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);      /// Flags
    if (arc_t == FULL_ARC) {
        lv_arc_set_range(newArc, 0, 360);
        lv_arc_set_value(newArc, 160);
        lv_arc_set_bg_angles(newArc, 0, 360);
    }
    else {
        lv_arc_set_value(newArc, 10);
        /* Default bg_angles range is 120, 60 */
    }
    lv_obj_set_style_arc_width(newArc, 6, LV_PART_MAIN | LV_STATE_DEFAULT);


    if (arc_t == FULL_ARC) {
        lv_obj_set_style_bg_img_src(newArc, &tempImg, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_blend_mode(newArc, LV_BLEND_MODE_NORMAL, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(newArc, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    else {
        lv_obj_set_style_arc_color(newArc, lv_color_hex(APP_COLOR_UNH_NAVY_ALT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_opa(newArc, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    lv_obj_set_style_arc_width(newArc, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(newArc, lv_color_hex(APP_COLOR_UNH_GOLD), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(newArc, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(newArc, lv_color_hex(0x000000), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(newArc, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(newArc, 1, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(newArc, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(newArc, lv_color_hex(0x000000), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(newArc, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(newArc, 10, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(newArc, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(newArc, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(newArc, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(newArc, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(newArc, 2, LV_PART_KNOB | LV_STATE_DEFAULT);

    return newArc;
}

/*---------------------------------------------------------------
    LVGL Encoder Input Device Read Callback
---------------------------------------------------------------*/
static void lv_indev_encA_read_cb(lv_indev_drv_t * drv, lv_indev_data_t * data) {
    // Keep a temp count here, and grab current count from ISR
    // do the differencing and set the encoder_diff value

    const char * TAG = "ENC_RD_CB_A";

    static int temp = 0;
    
    if (posA > temp) {
        data->enc_diff = posA - temp;
    }
    else if (posA < temp) {
        data->enc_diff = -(temp - posA);
    }

    temp = posA;

}

/*---------------------------------------------------------------
    LVGL Encoder Input Device Read Callback
---------------------------------------------------------------*/
static void lv_indev_encB_read_cb(lv_indev_drv_t * drv, lv_indev_data_t * data) {
    // Keep a temp count here, and grab current count from ISR
    // do the differencing and set the encoder_diff value

    const char * TAG = "ENC_RD_CB_B";

    static int temp = 0;

    if (posB > temp) {
        data->enc_diff = posB - temp;
    }
    else if (posB < temp) {
        data->enc_diff = -(temp - posB);
    }
    temp = posB;

}

/*---------------------------------------------------------------
    LCD callback function for updating arc rotation
---------------------------------------------------------------*/
static void arc_encA_event_cb(lv_event_t * e) {

    const char * TAG = "LV_ARC_CHNG_A";

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * arc = lv_event_get_target(e);
    lv_obj_t * label = lv_event_get_user_data(e);

    /*if (event_code == LV_EVENT_PRESSED) {

    }*/
    //ESP_LOGI(TAG, "E: %dL: %d", event_code, (int)e->param);

    lv_arc_set_value(arc, posA * 15);
    lv_label_set_text_fmt(label, /*"%" LV_PRId32 "%%"*/"%d", lv_arc_get_value(arc)/15);

}

/*---------------------------------------------------------------
    LCD callback function for updating arc rotation
---------------------------------------------------------------*/
static void arc_encB_event_cb(lv_event_t * e) {

    const char * TAG = "LV_ARC_CHNG_B";

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * arc = lv_event_get_target(e);
    lv_obj_t * label = lv_event_get_user_data(e);

    //ESP_LOGI(TAG, "E: %d", event_code);

    lv_arc_set_value(arc, posB * 15);
    lv_label_set_text_fmt(label, "%d", lv_arc_get_value(arc)/15);

}

/*---------------------------------------------------------------
    LCD callback function for updating arc rotation ON POTS
---------------------------------------------------------------*/
static void arc_potC_event_cb(lv_event_t * e) {

    const char * TAG = "LV_ARC_CHNG_C";

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * arc = lv_event_get_target(e);
    lv_obj_t * label = lv_event_get_user_data(e);

    //ESP_LOGI(TAG, "E: %d", event_code);

    lv_arc_set_value(arc, (int16_t)vpotc_pct);
    lv_label_set_text_fmt(label, "%d", lv_arc_get_value(arc));

}

/*---------------------------------------------------------------
    LCD callback function for updating arc rotation ON POTS
---------------------------------------------------------------*/
static void arc_potD_event_cb(lv_event_t * e) {

    const char * TAG = "LV_ARC_CHNG_C";

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * arc = lv_event_get_target(e);
    lv_obj_t * label = lv_event_get_user_data(e);

    //ESP_LOGI(TAG, "E: %d", event_code);

    lv_arc_set_value(arc, (int16_t)vpotd_pct);
    lv_label_set_text_fmt(label, "%d", lv_arc_get_value(arc));

}

/*---------------------------------------------------------------
    Vbat event for updating battery voltage readout and text color
---------------------------------------------------------------*/
static void vbat_assign_state() {

    // vbat is heavily filtered in hardware and software prior to passing to the below logic blocks
    if (vbat > 3.9) {
        batteryState = BAT_FULL;  
    }
    else if (vbat > 3.4){
        batteryState = BAT_MED;
    }
    else {
        batteryState = BAT_LOW;
    }

}

/*---------------------------------------------------------------
    Vbat event for updating battery voltage readout and text color
---------------------------------------------------------------*/
static void value_changed_event_vbat(lv_event_t * e) {

    const char * TAG = "LVGL_VBAT_CHNG";

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * label = lv_event_get_user_data(e);

    lv_style_set_text_color(&vbatLabel_style, lv_color_hex(batteryColors[batteryState]));
    lv_label_set_text_fmt(vbatLabel, "%s %3.2fV", app_icons[batteryState], vbat);

}

/*---------------------------------------------------------------
    LCD LVGL main application init code (plot main objects)
---------------------------------------------------------------*/
void app_display_init(void) {

    int offset = 30;

    //------------- University Crest Image ---------------//
    app_images[0] = lv_img_create(lv_scr_act());
    app_images[1] = lv_img_create(lv_scr_act());
    LV_IMG_DECLARE(app_img_universityCrest160x160);
    LV_IMG_DECLARE(app_img_directivity160x160);
    lv_img_set_src(app_images[0], &app_img_universityCrest160x160);
    lv_img_set_src(app_images[1], &app_img_directivity160x160);
    //lv_img_set_angle(icon, -900);
    lv_obj_align(app_images[0], LV_ALIGN_CENTER, -70, 32);
    lv_obj_align(app_images[1], LV_ALIGN_CENTER, -70, 32);

    //lv_obj_set_style_img_recolor_opa(icon, 255, LV_PART_MAIN);
    //lv_obj_set_style_img_recolor(icon, lv_color_hex(/*COLOR_UNH_NAVY*/0xFFFFFF), LV_PART_MAIN);
    lv_obj_add_flag(app_images[0], LV_OBJ_FLAG_HIDDEN);
    //lv_obj_clear_flag(icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(app_images[1], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(app_images[1], LV_OBJ_FLAG_HIDDEN);

    //------------- Main display label (scrolling text) ---------------//
    lv_obj_t * obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(obj, 100, 30);  // width taken care of by below call
    lv_obj_center(obj);

    lv_obj_t * label = lv_label_create(obj);
    lv_label_set_text(label, "Sound Steering - Remote Module                By K. Harper");
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLOATING); /*To be ignored by scrollbars*/
    lv_obj_center(label);
    lv_obj_update_layout(obj);    /*To have the correct size for lv_obj_get_width*/
    lv_obj_align(obj, LV_ALIGN_CENTER, -70, -75);

    //------------- Enc A - Azimuth Encoder Arc ---------------//
    arc0 = app_arc_create(lv_scr_act(), FULL_ARC, APP_ARC_SIZE, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_t * label_arc0 = lv_label_create(lv_scr_act());
    lv_obj_t * label_data_arc0 = lv_label_create(arc0); // set parent to arc object
    lv_label_set_text(label_arc0, "Azim. Enc.");
    lv_obj_align(label_arc0, LV_ALIGN_TOP_RIGHT, -70, 30);
    lv_obj_align(label_data_arc0, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(arc0, arc_encA_event_cb, /*LV_EVENT_KEY*/ LV_EVENT_VALUE_CHANGED /*| LV_EVENT_PRESSED*/, label_data_arc0);
    //lv_obj_add_event_cb(arc0, sw_pressed_cbA, LV_EVENT_PRESSED, NULL);
    lv_event_send(arc0, LV_EVENT_VALUE_CHANGED, 160);

    //------------- Enc B - Elevation Encoder Arc ---------------//
    arc1 = app_arc_create(lv_scr_act(), FULL_ARC, APP_ARC_SIZE, LV_ALIGN_RIGHT_MID, -10, -offset);
    lv_obj_t * label_arc1 = lv_label_create(lv_scr_act());
    lv_obj_t * label_data_arc1 = lv_label_create(arc1);
    lv_label_set_text(label_arc1, "Elev. Enc.");
    lv_obj_align(label_arc1, LV_ALIGN_RIGHT_MID, -70, -offset);
    lv_obj_align(label_data_arc1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(arc1, arc_encB_event_cb, /*LV_EVENT_KEY*/ LV_EVENT_VALUE_CHANGED /*| LV_EVENT_PRESSED*/, label_data_arc1);
    lv_event_send(arc1, LV_EVENT_VALUE_CHANGED, 160);

    //------------- Pot C - Volume Potentiometer Arc ---------------//
    arc2 = app_arc_create(lv_scr_act(), PARTIAL_ARC, APP_ARC_SIZE, LV_ALIGN_RIGHT_MID, -10, +offset);
    lv_obj_t * label_arc2 = lv_label_create(lv_scr_act());
    lv_obj_t * label_data_arc2 = lv_label_create(arc2);
    lv_label_set_text(label_arc2, "Volm. Pot.");
    lv_obj_align(label_arc2, LV_ALIGN_RIGHT_MID, -70, +offset);
    lv_label_set_text(label_data_arc2, "0");
    lv_obj_align(label_data_arc2, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(arc2, arc_potC_event_cb, /*LV_EVENT_KEY*/ LV_EVENT_VALUE_CHANGED /*| LV_EVENT_PRESSED*/, label_data_arc2);
    lv_event_send(arc2, LV_EVENT_VALUE_CHANGED, NULL);

    //------------- Pot D - Volume Potentiometer Arc ---------------//
    arc3 = app_arc_create(lv_scr_act(), PARTIAL_ARC, APP_ARC_SIZE, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_t * label_arc3 = lv_label_create(lv_scr_act());
    lv_obj_t * label_data_arc3 = lv_label_create(arc3);
    lv_label_set_text(label_arc3, "Dist. Pot.");
    lv_obj_align(label_arc3, LV_ALIGN_BOTTOM_RIGHT, -70, -30);
    lv_label_set_text(label_data_arc3, "0");
    lv_obj_align(label_data_arc3, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(arc3, arc_potD_event_cb, /*LV_EVENT_KEY*/ LV_EVENT_VALUE_CHANGED /*| LV_EVENT_PRESSED*/, label_data_arc3);
    lv_event_send(arc3, LV_EVENT_VALUE_CHANGED, NULL);

    //------------- Battery Voltage Readout ---------------//
    vbatLabel = lv_label_create(lv_scr_act());
    lv_style_init(&vbatLabel_style);
    lv_style_set_text_color(&vbatLabel_style, lv_color_hex(batteryColors[batteryState]));
    lv_label_set_text_fmt(vbatLabel, LV_SYMBOL_BATTERY_EMPTY" %3.2fV", SCALE_VBAT(vbat));
    lv_obj_add_style(vbatLabel, &vbatLabel_style, 0);
    lv_obj_align(vbatLabel, LV_ALIGN_TOP_LEFT, 25, 5);
    lv_obj_add_event_cb(vbatLabel, value_changed_event_vbat, LV_EVENT_VALUE_CHANGED, NULL);
    lv_event_send(vbatLabel, LV_EVENT_VALUE_CHANGED, NULL);
    
    //------------- Controller Audio Chan Select 1 Indicator ---------------//
    chanSelect_label = lv_label_create(lv_scr_act());
    lv_label_set_text(chanSelect_label, " C1|C2|C3");
    lv_obj_align(chanSelect_label, LV_ALIGN_TOP_LEFT, 95, 5);

    //------------- Map arc0 and encA to LVGL input device and place into LVGL object group ---------------//
    //lv_disp_drv_register(&disp_drv); // Taken care of in init section of displayTask
    static lv_indev_drv_t indev_drv0;
    lv_indev_drv_init(&indev_drv0);         
    indev_drv0.type = LV_INDEV_TYPE_ENCODER; 
    indev_drv0.read_cb = lv_indev_encA_read_cb;      

    //Register the driver in LVGL and save the created input device object
    lv_indev_t * my_indev0 = lv_indev_drv_register(&indev_drv0);
    lv_group_t * encA_group = lv_group_create();
    lv_group_add_obj(encA_group, arc0);
    lv_indev_set_group(my_indev0, encA_group);
    lv_group_focus_obj(arc0);
    lv_group_set_editing(encA_group, true);

    //------------- Map arc1 and encB to LVGL input device and place into LVGL object group ---------------//
    static lv_indev_drv_t indev_drv1;
    lv_indev_drv_init(&indev_drv1);         
    indev_drv1.type = LV_INDEV_TYPE_ENCODER; 
    indev_drv1.read_cb = lv_indev_encB_read_cb;      

    //Register the driver in LVGL and save the created input device object
    lv_indev_t * my_indev1 = lv_indev_drv_register(&indev_drv1);
    lv_group_t * encB_group = lv_group_create();
    lv_group_add_obj(encB_group, arc1);
    lv_indev_set_group(my_indev1, encB_group);
    lv_group_focus_obj(arc1);
    lv_group_set_editing(encB_group, true);

    //------------- Scrolling text animation ---------------//
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, label);
    lv_anim_set_values(&anim, lv_obj_get_width(obj) + 160, -lv_obj_get_width(label) - 20);
    lv_anim_set_time(&anim, ANIM_PERIOD_MS);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim, set_x);
    lv_anim_start(&anim);
}

/*---------------------------------------------------------------
    LCD LVGL FreeRTOS tick timer callback
---------------------------------------------------------------*/
static void tickInc(void *arg) {
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

/*---------------------------------------------------------------
    LCD LVGL FreeRTOS main application task
---------------------------------------------------------------*/
static void displayTask(void *pvParameter) {

static const char *DISPLAY_TASK_TAG = "DISPLAY";

(void)pvParameter;
xGuiSemaphore = xSemaphoreCreateMutex();

lv_init();

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &tickInc,
        .name = "lvgl_tick_inc",
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    size_t display_buffer_size = DISP_BUF_SIZE;
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(display_buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, display_buffer_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 320;
    disp_drv.rotated = LV_DISP_ROT_270;
    lv_disp_drv_register(&disp_drv);

    lvgl_driver_init();
    disp_driver_init();

    ESP_LOGI(DISPLAY_TASK_TAG, "display init done.");

    const disp_backlight_config_t disp_bl_cfg = {
        .pwm_control = true,
        .output_invert = false,
        .gpio_num = 21,
        .timer_idx = 1,
        .channel_idx = 1,
    };

    bl = disp_backlight_new(&disp_bl_cfg);
    disp_backlight_set(bl, 85);

    app_display_init();

    while (1) {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(1);
        //pdTICKS_TO_MS
        /*vTaskDelay(pdMS_TO_TICKS(10));*/
        lv_event_send(vbatLabel, LV_EVENT_VALUE_CHANGED, NULL);
        disp_backlight_set(bl, vpotd_pct);
        lv_event_send(arc2, LV_EVENT_VALUE_CHANGED, NULL);
        lv_event_send(arc3, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_set_style_bg_color(arc0, lv_color_hex(knobColors[stateA.sw_status]), LV_PART_KNOB);
        lv_obj_set_style_bg_color(arc1, lv_color_hex(knobColors[stateB.sw_status]), LV_PART_KNOB);

        if (stateA.sw_status && stateB.sw_status) { // Swap the displayed image on concurrent encoder switch presses
            lv_obj_add_flag(app_images[activeImage], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(app_images[!activeImage], LV_OBJ_FLAG_HIDDEN);
            activeImage = !activeImage;
        }

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    /* A task should NEVER return */
    free(buf1);
    vTaskDelete(NULL);
}

/*---------------------------------------------------------------
    Rotary Encoder initialization function
---------------------------------------------------------------*/
static void encoder_init(rotary_encoder_info_t * encoder, gpio_num_t cha_pin, gpio_num_t chb_pin, 
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

/*---------------------------------------------------------------
    Rotary Encoder task
---------------------------------------------------------------*/
static void encATask(void * handle) {

    static const char * TAG = "ENC_A";

    const bool VERBOSE = false;

    rotary_encoder_info_t * encoder;
    encoder = (rotary_encoder_info_t *) handle;
    //int pos = encoder->state;

    encoder_init(encoder, ENCA_CHA_PIN, ENCA_CHB_PIN, ENCA_SW_PIN, false, true);

    // Create a queue for events from the rotary encoder driver.
    // Tasks can read from this queue to receive up to date position information.
    xEncoderAQueue = rotary_encoder_create_queue();
    ESP_ERROR_CHECK(rotary_encoder_set_queue(encoder, xEncoderAQueue));

    while(1) {

        // Wait for incoming events on the event queue.
        if (xQueueReceive(xEncoderAQueue, &eventA, ENC_QUEUE_DELAY / portTICK_PERIOD_MS) == pdTRUE) {
                     posA = (int)eventA.state.position;
        }
        else {
            // Poll current position and direction
            ESP_ERROR_CHECK(rotary_encoder_get_state(encoder, &stateA));
            posA = (int)stateA.position;
            if (VERBOSE) {
                ESP_LOGI(TAG, "Poll: pos %d, dir %s, sw %d", (int)stateA.position,
                        stateA.direction ? (stateA.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET", (int)stateA.sw_status);
            }
        }
        // Reset the encoder counts
        if (MAX_ENCODER_COUNTS && (posA >= MAX_ENCODER_COUNTS || posA < 0)) {
            ESP_ERROR_CHECK(rotary_encoder_reset(encoder));
        }
        else if ((posA == 0) && (int)stateA.direction == ROTARY_ENCODER_DIRECTION_COUNTER_CLOCKWISE) {
            ESP_ERROR_CHECK(rotary_encoder_wrap(encoder, MAX_ENCODER_COUNTS - 1));
            posA = (int)stateA.position;
        }

        //vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// should definitely refactor these and pass parameters to the callback function on task creation

/*---------------------------------------------------------------
    Rotary Encoder task
---------------------------------------------------------------*/
static void encBTask(void * handle) {

    static const char * TAG = "ENC_B";

    const bool VERBOSE = false;

    rotary_encoder_info_t * encoder;
    encoder = (rotary_encoder_info_t *) handle;

    encoder_init(encoder, ENCB_CHA_PIN, ENCB_CHB_PIN, ENCB_SW_PIN, false, true);

    // Create a queue for events from the rotary encoder driver.
    // Tasks can read from this queue to receive up to date position information.
    xEncoderBQueue = rotary_encoder_create_queue();
    ESP_ERROR_CHECK(rotary_encoder_set_queue(encoder, xEncoderBQueue));

    while(1) {

        // Wait for incoming events on the event queue.
        if (xQueueReceive(xEncoderBQueue, &eventB, ENC_QUEUE_DELAY / portTICK_PERIOD_MS) == pdTRUE) {
                       posB = (int)eventB.state.position;
        }
        else {
            // Poll current position and direction
            ESP_ERROR_CHECK(rotary_encoder_get_state(encoder, &stateB));
            posB = (int)stateB.position;
            if (VERBOSE) {
                ESP_LOGI(TAG, "Poll: pos %d, dir %s, sw %d", (int)stateB.position,
                        stateB.direction ? (stateB.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET", (int)stateB.sw_status);
            }
        }
        // Reset the encoder counts
        if (MAX_ENCODER_COUNTS && (posB >= MAX_ENCODER_COUNTS || posB < 0)) {
            ESP_ERROR_CHECK(rotary_encoder_reset(encoder));
        }
        
        else if ((posB == 0) && (int)stateB.direction == ROTARY_ENCODER_DIRECTION_COUNTER_CLOCKWISE) {
            ESP_ERROR_CHECK(rotary_encoder_wrap(encoder, MAX_ENCODER_COUNTS - 1));
            posB = (int)stateB.position;
        }

        //vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

/*============================================ APP_MAIN ============================================*/

/*---------------------------------------------------------------
    Main application task. By the time execution gets here, the 
    pro_cpu is initialized and both schedulers are running.
    This is one of the tasks on the app_cpu. This task initializes
    the other FreeRTOS tasks and proceeds to enter an infinite 
    loop flashing the blue heartbeat LED on the remote PCB.
---------------------------------------------------------------*/
void app_main(void) {

    static const char * TAG = "APP_MAIN";

    // Init UART2 for development port (to be replaced with BT)
    uart2_init(U2_BAUD);
    app_gpio_init();
    gpio_set_level(LOWBATT_LED_PIN, 1);
    
    // Create FreeRTOS tasks to handle various peripherals/functions
    xTaskCreate(rxTask,  "uart_rx_task",  1024*2, NULL, configMAX_PRIORITIES - 1,   NULL);
    xTaskCreate(txTask,  "uart_tx_task",  1024*2, NULL, configMAX_PRIORITIES - 2, NULL);
    xTaskCreate(adcTask, "adc_task", 1024*2,   NULL, configMAX_PRIORITIES - 2,  NULL);
    xTaskCreate(displayTask, "display_task", 4096 * 2, NULL, configMAX_PRIORITIES, NULL);

    // Play with half step resolution to register direction change immediately
    // Also, currently from 1 rolls back to 23
    xTaskCreate(encATask, "encA", 1024*2, (void *)&encA, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(encBTask, "encB", 1024*2, (void *)&encB, configMAX_PRIORITIES - 1, NULL);

    bool pin = 1;
    while(1) {
        gpio_set_level(HEARTBEAT_LED_PIN, pin);
        pin = !pin;
        vbat_assign_state();
        vTaskDelay(HEARTBEAT_BLINK_PERIOD_MS / portTICK_PERIOD_MS);
    }
}

/*========================================= END PROGRAM ============================================*/