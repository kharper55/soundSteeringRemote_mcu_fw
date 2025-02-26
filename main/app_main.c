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
    https://github.com/khoih-prog/ESP32_New_TimerInterrupt/tree/main was inspiration behind timer interrupts (unfortunately confused by the documentation so the attempted cpp -> c port was unsusccessful)
//==================================================================================================*/

/*============================================= INCLUDES ===========================================*/

#include "app_include/app_utility.h"    /* Application universal includes and some utility functions */
#include "app_include/app_adc.h"        /* ADC driver application specific code */
#include "app_include/app_uart2.h"      /* UART2 driver application specific code */
#include "app_include/app_gpio.h"       /* GPIO driver application specific code */
#include "app_include/app_spi.h"        /* SPI driver application specific code and LVGL/display related */
#include "app_include/app_encoder.h"    /* Rotary encoder driver application specific code */
#include "app_include/app_timer.h"      /* Provide hardware timer units for measuring button actuation durations. Expand possibilities of user input with dedicated keys (2x encoder switches) */
#include "app_include/app_bluetooth.h"  /* Bluetooth peripheral application specific code. Nothing implemented yet. A bit nervous for the impending overhead. */

/*========================== CONSTANTS, MACROS, AND VARIABLE DECLARATIONS ==========================*/

/* Global Vars */
static battery_states_t batteryState = BAT_LOW;

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

lv_obj_t * vbatLabel;
static lv_style_t vbatLabel_style;
lv_obj_t * chanSelect_label;
lv_obj_t * app_images[2];
bool activeImage = true;

// These vars are defined in app_spi.c
extern const char * batteryStateNames[4];
extern uint32_t knobColors[2];    // Declare as extern
extern uint32_t textColors[2];    // Declare as extern
extern uint32_t batteryColors[3]; // Declare as extern
extern const char app_icons[][4]; // Declare as extern
extern const char * gpio_status_names[2];
extern const char * keypress_combo_names[NUM_COMBOS];
extern int keypress_combos[NUM_COMBOS][KEYPRESS_COMBO_LENGTH];

int posA = 0; // Azimuth angle, -30 -> 30
int posB = 0; // Elevation angle, -30 -> 30
int posC = 0; // Scaled percentage of potC
int posD = 0; // Scaled percentage of potD

float vbat = 0;
int vbat_raw = 0;
int vbat_cali = 0;
int vbat_filt = 0;

int vpotd_raw = 0;
int vpotd_cali = 0;
int vpotd_pct = 0;
int vpotd_filt = 0;

int vpotc_raw = 0;
int vpotc_cali = 0;
int vpotc_pct = 0;
int vpotc_filt = 0;

adc_filter_t vbat_filt_handle = {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 10, false};
adc_filter_t vpotd_filt_handle = {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 10, false};
adc_filter_t vpotc_filt_handle = {0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 10, false}; // Try automatically getting length with sizeof(arr)/sizeof(firstEl)(this requires predeclaring an array and passing to the struct which I am not sure is possible)

disp_backlight_h bl;

volatile bool timerAFlag = false;
volatile bool timerBFlag = false;
volatile bool change_channel_flag = false;
volatile bool toggle_on_off_flag = false;

SemaphoreHandle_t xChannelFlagSemaphore;
SemaphoreHandle_t xToggleOnOffFlagSemaphore;

bool swAlevel = LOW;
bool swBlevel = LOW;


/*================================ FUNCTION AND TASK DEFINITIONS ===================================*/

/*---------------------------------------------------------------
    UART2 TX FreeRTOS task
---------------------------------------------------------------*/
static void txTask(void *arg) {

    const bool VERBOSE_FLAG = true;

    static const char * TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);

    char * txData = (char *) malloc(TX_BUF_SIZE + 1);
    char * txBuff_preCRC = (char *) malloc(TX_BUF_SIZE + 1);

    const int16_t POS_OFFSET = 30;
    const int CRC_LEN_HEX = 8;
    const int8_t LENS[5] = {1, 1, 5, 5, 9};

    // both encoders should spit out counts between +=30
    // both pots -> might want to filter the adc counts, and then scale via bit shift (12 bit to 8 bit?)
    static int temp_azimuthPos = MIN_ENCODER_COUNTS; // -30
    static int temp_elevPos = MIN_ENCODER_COUNTS;    // -30
    //static uint8_t temp_potc_counts = PCT_MIN; // 0
    //static uint8_t temp_potd_counts = PCT_MIN; // 0
    static int temp_potc_counts = 0; // 0
    static int temp_potd_counts = 0; // 0
    int16_t potc_scaled = 0;
    int16_t potd_scaled = 0;
    uint32_t crc = 0;
    int flag = 0;

    while (1) {
        //if (xQueueReceive(sendData(TX_TASK_TAG, "Hello world") == pdTrue)) {}
        potc_scaled = SCALE_VPOT_INVERT((vpotc_filt));
        potd_scaled = SCALE_VPOT_INVERT((vpotd_filt));
        flag = NOP;

        /* Pertinent typedef
        typedef enum serial_cmds_t {
            NOP                      = 0x0,
            TOGGLE_ON_OFF            = 0x2,  // Hex code for togglining device on/off (i.e. power to the array)
            CHANGE_CHANNEL           = 0x4,  // Hex code for changing only channel with one transaction
            CHANGE_COORD             = 0x8,  // Hex code for changing only coordinate with one transaction
            CHANGE_VOLUME            = 0xA,  // Hex code for changing only volume with one transaction
            CHANGE_COORD_AND_VOLUME  = 0xC,  // Hex code for changing volume, channel, and coordinate with one transaction
            REQUEST_INFO             = 0xE   // Hex code for requesting readback from the device
        };
        */

        // On received end, should pop 8 bytes off the end, compute a local crc, then compare the crc with the one that was transmitted
        // If the crc mismatchyes, the n request another copy of the data until ok...
        // This will require a generic info resend (just send all the info back becasue we dont know exactly what info was lost in the time past)

        if (!toggle_on_off_flag) {        // flag handled by pair of gptimer and gpio interrupt
            if (!(change_channel_flag)) { // flag handled by pair of gptimer and gpio interrupt
                if((posA != temp_azimuthPos) || (posB != temp_elevPos)) {
                    temp_azimuthPos = posA;
                    temp_elevPos = posB;
                    flag = CHANGE_COORD; 
                }
                if((potc_scaled != temp_potc_counts) || (potd_scaled != temp_potd_counts)) {
                    temp_potc_counts = potc_scaled;
                    temp_potd_counts = potd_scaled;
                    flag = (flag == CHANGE_COORD) ? CHANGE_COORD_AND_VOLUME : CHANGE_VOLUME;
                }
            }
            else {
                flag = CHANGE_CHANNEL;
            }
        }
        else {
            flag = TOGGLE_ON_OFF;
        }

        if(flag) {
            switch(flag) {
                case TOGGLE_ON_OFF:           // flag = 0x0. requires action from artix7, or alternatively just send pwm_buff_en and load_switch_en low on the controller.
                    sprintf(txData, "%01X", flag);
                    
                    //xSemaphoreTake(xToggleOnOffFlagSemaphore, 10);
                    toggle_on_off_flag = false; // reset the flag, else everyones unhappy (trust me)
                    //xSemaphoreGive(xToggleOnOffFlagSemaphore);
                    break;
                case CHANGE_CHANNEL:          // flag = 0x2. requires action from artix7
                    sprintf(txData, "%01X", flag);
                    //xSemaphoreTake(xChannelFlagSemaphore, 10);
                    change_channel_flag = false;
                    //xSemaphoreGive(xChannelFlagSemaphore);
                    
                    break;
                case CHANGE_COORD:            // flag = 0x4. requires action from artix7
                    sprintf(txData, "%01X%02X%02X", flag, (uint8_t)(temp_azimuthPos + POS_OFFSET), (uint8_t)(temp_elevPos + POS_OFFSET));
                    break;
                case CHANGE_VOLUME:           // flag = 0x6. requires nothing from artix7
                    sprintf(txData, "%01X%02X%02X", flag, temp_potc_counts, temp_potd_counts);
                    break;
                case CHANGE_COORD_AND_VOLUME: // flag = 0x8. requires action from artix7 and esp32
                    sprintf(txData, "%01X%02X%02X%02X%02X", flag, (uint8_t)(temp_azimuthPos + POS_OFFSET), (uint8_t)(temp_elevPos + POS_OFFSET), temp_potc_counts, temp_potd_counts);
                    break;
                default:                      // Break
                    break;
            }

            const int len = strlen(txData);
            crc = app_compute_crc32(txData, len);

            if (VERBOSE_FLAG) {
                ESP_LOGI(TX_TASK_TAG, "Flag: %d, Data: %s, CRC: %08lX", flag, txData, crc);
            }
        
            snprintf(txBuff_preCRC, len + CRC_LEN_HEX + 1, "%s%08lX", txData, crc); // Append CRC to data
            const int txBytes = uart_write_bytes(UART_NUM_2, txBuff_preCRC, strlen(txBuff_preCRC)); // Write data to UART

            if (VERBOSE_FLAG) {
                ESP_LOGI(TX_TASK_TAG, "Wrote %d bytes: '%s'", txBytes, txBuff_preCRC);
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    free(txData);
    free(txBuff_preCRC);
}

/*---------------------------------------------------------------
    UART2 RX FreeRTOS task
---------------------------------------------------------------*/
static void rxTask(void *arg) {

    static const bool (VERBOSE_FLAG) = true;

    static const char * RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);

    // With the opposite end of the twisted pair harness floaitng, when txing, ew get erroneous reads
    
    // Pertinent flag here will be to rerequest info from the controller

    // if bad_data_flag then send CMD, wait for response

    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_2, data, RX_BUF_SIZE, 10 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            if (VERBOSE_FLAG) ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
        }
    }

    free(data);
}

/*---------------------------------------------------------------
    ADC Oneshot-Mode Continuous Read Task
    
    Generic oneshot adc task for various MCU adc channels
---------------------------------------------------------------*/
static void adcTask(void * pvParameters) {

    const bool VERBOSE_FLAG = false;
    esp_err_t err = ESP_OK;

    adcOneshotParams_t * params = (adcOneshotParams_t *) pvParameters;
    const char * TAG = params->TAG;
    adc_oneshot_unit_handle_t * adc_handle = params->handle;
    adc_cali_handle_t * cali_handle = params->cali_handle;
    adc_unit_t unit = params->unit;
    adc_channel_t chan = params->channel;
    adc_atten_t atten = params->atten;
    int delay_ms = params->delay_ms;
    adc_filter_t * filt = params->filt;
    int * vraw = params->vraw;
    int * vcal = params->vcal;
    int * vfilt = params->vfilt;
    //int * vpct = params->vpct;    // HADNLED elsewhee=re, as pct calculation is not needed for vbat
    
    adc_oneshot_init(adc_handle, unit, chan); // VPOTC adc16
    adc_calibration_init(unit, chan, atten, cali_handle);

    while (1) {

        // add members to hold/globalize the various readings, ie raw,cali,filt
        
        err = adc_oneshot_read(*adc_handle, chan, vraw);
        if (err != ESP_OK) {
            if(VERBOSE_FLAG) {
                // Handle timeout or other errors
                if (err == ESP_ERR_TIMEOUT) {
                    // Timeout occurred, handle it
                    ESP_LOGW(TAG, "ADC read timeout occurred");
                } else {
                    // Handle other errors
                    ESP_LOGE(TAG, "ADC read failed with error code: %d", err);
                }
            }
        } 
        else {
            err = adc_cali_raw_to_voltage(*cali_handle, *vraw, vcal);
            if (err == ESP_OK) {

                *vfilt = adc_filter(*vcal, filt);
            
                if (VERBOSE_FLAG) {
                    ESP_LOGI(TAG, "ADC%d_%d raw  : %d counts", unit + 1, chan, *vraw);
                    ESP_LOGI(TAG, "ADC%d_%d cal  : %dmV", unit + 1, chan, *vcal);
                    ESP_LOGI(TAG, "ADC%d_%d filt : %dmV", unit + 1, chan, *vfilt);
                    //ESP_LOGI(TAG, "ADC%d_%d pct  : %d%%", unit + 1, chan, *vpct);
                }

            }
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    
    adc_oneshot_del_unit(*adc_handle);
    adc_calibration_deinit(*cali_handle);
}

/*---------------------------------------------------------------
    LCD object x-position set. Used for scrolling LVGL objects.
---------------------------------------------------------------*/
static void set_x(void * var, int32_t v) {
    lv_obj_set_x(var, v);
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
    
    // Me thinks we should probably use a queue here with 0 timeout,
    // then check with the static temp value, and update accordingly.

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

    lv_arc_set_value(arc, posA * 2);
    lv_label_set_text_fmt(label, /*"%" LV_PRId32 "%%"*/"%d", lv_arc_get_value(arc) / 2);

}

/*---------------------------------------------------------------
    LCD callback function for updating arc rotation
---------------------------------------------------------------*/
static void arc_encB_event_cb(lv_event_t * e) {

    const char * TAG = "LV_ARC_CHNG_B";

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * arc = lv_event_get_target(e);
    lv_obj_t * label = lv_event_get_user_data(e);

    lv_arc_set_value(arc, posB * 2);
    lv_label_set_text_fmt(label, "%d", lv_arc_get_value(arc) / 2);

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

    lv_arc_set_value(arc, (int16_t)SCALE_VPOT_INVERT(vpotc_filt));
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

    lv_arc_set_value(arc, (int16_t)SCALE_VPOT_INVERT(vpotd_filt));
    lv_label_set_text_fmt(label, "%d", lv_arc_get_value(arc));

}

/*---------------------------------------------------------------
    Vbat event for updating battery voltage readout and text color
---------------------------------------------------------------*/
static void vbat_assign_state(battery_states_t * state, float value) {

    // ADD some temp vars here to determine when charging, and display info to screen
    // Also to gate any flickering of the vbat
    //battery_states_t * batteryState = BAT_EMPTY;

    // also drive the low batt led pin high on low battery conditions

    // vbat is heavily filtered in hardware and software prior to passing to the below logic blocks
    if (value > 3.9) {
        *state = BAT_FULL;  
    }
    else if (value > 3.4) {
        *state = BAT_MED;
    }
    else {
        *state = BAT_LOW;
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
    lv_label_set_text_fmt(vbatLabel, "%s %3.2fV", app_icons[batteryState], SCALE_VBAT(vbat_filt));

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
    arc0 = app_arc_create(lv_scr_act(), PARTIAL_ARC, APP_ARC_SIZE, LV_ALIGN_TOP_RIGHT, -10, 10, true);
    lv_obj_t * label_arc0 = lv_label_create(lv_scr_act());
    lv_obj_t * label_data_arc0 = lv_label_create(arc0); // set parent to arc object
    lv_label_set_text(label_arc0, "Azim. Enc.");
    lv_obj_align(label_arc0, LV_ALIGN_TOP_RIGHT, -70, 30);
    lv_obj_align(label_data_arc0, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(arc0, arc_encA_event_cb, /*LV_EVENT_KEY*/ LV_EVENT_VALUE_CHANGED /*| LV_EVENT_PRESSED*/, label_data_arc0);
    //lv_obj_add_event_cb(arc0, sw_pressed_cbA, LV_EVENT_PRESSED, NULL);
    lv_event_send(arc0, LV_EVENT_VALUE_CHANGED, 160);

    //------------- Enc B - Elevation Encoder Arc ---------------//
    arc1 = app_arc_create(lv_scr_act(), PARTIAL_ARC, APP_ARC_SIZE, LV_ALIGN_RIGHT_MID, -10, -offset, true);
    lv_obj_t * label_arc1 = lv_label_create(lv_scr_act());
    lv_obj_t * label_data_arc1 = lv_label_create(arc1);
    lv_label_set_text(label_arc1, "Elev. Enc.");
    lv_obj_align(label_arc1, LV_ALIGN_RIGHT_MID, -70, -offset);
    lv_obj_align(label_data_arc1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(arc1, arc_encB_event_cb, /*LV_EVENT_KEY*/ LV_EVENT_VALUE_CHANGED /*| LV_EVENT_PRESSED*/, label_data_arc1);
    lv_event_send(arc1, LV_EVENT_VALUE_CHANGED, 160);

    //------------- Pot C - Volume Potentiometer Arc ---------------//
    arc2 = app_arc_create(lv_scr_act(), PARTIAL_ARC, APP_ARC_SIZE, LV_ALIGN_RIGHT_MID, -10, +offset, false);
    lv_obj_t * label_arc2 = lv_label_create(lv_scr_act());
    lv_obj_t * label_data_arc2 = lv_label_create(arc2);
    lv_label_set_text(label_arc2, "Volm. Pot.");
    lv_obj_align(label_arc2, LV_ALIGN_RIGHT_MID, -70, +offset);
    lv_label_set_text(label_data_arc2, "0");
    lv_obj_align(label_data_arc2, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(arc2, arc_potC_event_cb, /*LV_EVENT_KEY*/ LV_EVENT_VALUE_CHANGED /*| LV_EVENT_PRESSED*/, label_data_arc2);
    lv_event_send(arc2, LV_EVENT_VALUE_CHANGED, NULL);

    //------------- Pot D - Volume Potentiometer Arc ---------------//
    arc3 = app_arc_create(lv_scr_act(), PARTIAL_ARC, APP_ARC_SIZE, LV_ALIGN_BOTTOM_RIGHT, -10, -10, false);
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
    lv_label_set_text_fmt(vbatLabel, LV_SYMBOL_BATTERY_EMPTY" %3.2fV", SCALE_VBAT(vbat_filt));
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
        .gpio_num = TFT_BACKLIGHT_PIN,
        .timer_idx = 1,
        .channel_idx = 1,
    };

    bl = disp_backlight_new(&disp_bl_cfg);
    disp_backlight_set(bl, 85);

    app_display_init();

    // LVGL TASK LOOP
    while (1) {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(1);
        //pdTICKS_TO_MS
        /*vTaskDelay(pdMS_TO_TICKS(10));*/
        lv_event_send(vbatLabel, LV_EVENT_VALUE_CHANGED, NULL);
        disp_backlight_set(bl, SCALE_VPOT_INVERT(vpotd_filt));
        lv_event_send(arc2, LV_EVENT_VALUE_CHANGED, NULL);
        lv_event_send(arc3, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_set_style_bg_color(arc0, lv_color_hex(knobColors[encA.state.sw_status]), LV_PART_KNOB);
        lv_obj_set_style_bg_color(arc1, lv_color_hex(knobColors[encB.state.sw_status]), LV_PART_KNOB);

        
        if (encA.state.sw_status && encB.state.sw_status) {
        //if (encA.state.sw_status && stateB.sw_status) { // Swap the displayed image on concurrent encoder switch presses
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

// should definitely refactor these and pass parameters to the callback function on task creation
static void gptimerB_callback(void) {
    timerAFlag = true;
}

static void gptimerA_callback(void) {
    timerBFlag = true;
}

/*---------------------------------------------------------------
    Rotary Encoder task
    Generic, pass params for particular behavior
---------------------------------------------------------------*/
static void encTask(void * pvParameters) {
    
    static const bool VERBOSE = false;
    const int timer_flag_thresh = 32;

    encParams_t * params = (encParams_t *) pvParameters;
    char * TAG = params->TAG;
    rotary_encoder_info_t * encoder = params->encoder;
    rotary_encoder_event_t * event = params->event;
    rotary_encoder_state_t * state = params->state;
    gptimer_handle_t * timer_handle = params->timer_handle;
    void (*timer_cb)(int) = params->timer_cb;
    uint32_t timer_top = params->timer_top;
    volatile bool * timerFlag = params->timerFlag;
    gpio_num_t pinA = params->pinA;
    gpio_num_t pinB = params->pinB;
    gpio_num_t pinSW = params->pinSW;
    circularBuffer * circBuff = params->comboBuff;
    int id = params->id;
    int * pos = params->pos;
    int delay_ms = params->delay_ms;
    QueueHandle_t queue = params->queue;

    bool change_flag = true;
    bool sw_level = LOW;
    int push_count = 0;

    uint32_t timer_count = 0;

    esp_err_t ret = ESP_OK;

    gptimer_state_t timer_state = {
        .timerHandle = timer_handle,
        .running = false
    };

    ret = app_initTimer(timer_handle, timer_cb, timer_top, false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Couldn't initialize timer!");
    }

    encoder_init(encoder, pinA, pinB, pinSW, false, true);

    // Create a queue for events from the rotary encoder driver.
    // Tasks can read from this queue to receive up to date position information.
    queue = rotary_encoder_create_queue();
    ESP_ERROR_CHECK(rotary_encoder_set_queue(encoder, queue));

    while(1) {

        // Wait for incoming events on the event queue.
        if (xQueueReceive(queue, event, pdMS_TO_TICKS(delay_ms))) {
            *pos = (int)event->state.position;
        }
        // update gpio state
        rotary_encoder_poll_switch(encoder);
        change_flag = (sw_level == (encoder->state.sw_status)) ? false : true;
        switch(change_flag) {
            // Button is actuated
            case(true): 
                if (VERBOSE) ESP_LOGI(TAG, "CHANGE FLAG REGISTERED");
                change_flag = false;
                timer_count = 0;                                   // Reset the timer counts
                sw_level = encoder->state.sw_status;               // Assign new level to temp var
                app_toggleTimerRun(timer_handle, &timer_state);    // Start encoder switch timer
                if (push_count % 2 == 0 || push_count == 0) {      // Register every other edge
                    push_key(circBuff, id);                        // Push recent press to circular buffer
                    if (VERBOSE) ESP_LOGI(TAG, "PUSHED KEY: %d", id);
                    // Debug print the circular buffer entries
                    int start = circBuff->head;
                    //ESP_LOGI(TAG, "BUFF ENTRIES:");
                    for (int i = 0; i < KEYPRESS_COMBO_LENGTH; i++) {
                        //ESP_LOGI(TAG, "Buffer Entry %d: %d", start, circBuff->buffer[start]);
                        start = (start + 1) % KEYPRESS_COMBO_LENGTH;
                    }
                }
                push_count++;
                break;
            // Button is not actuated
            default:
                if (timer_count >= timer_flag_thresh) {
                    if (VERBOSE) ESP_LOGI(TAG, "LONG PRESS DETECTED");
                    timer_count = 0;
                }
                break;
        }

        int target[] = {1, 1, 2};
        int target2[] = {1, 2, 2};
        
        if(check_combo(circBuff, target)) {
            if (VERBOSE) ESP_LOGI(TAG, "GOT COMBO! '%s'", "on off"/*keypress_combo_names[7]*/);
            toggle_on_off_flag = true;
            clear_buffer(circBuff);
        }
        else if(check_combo(circBuff, target2)) {
            if (VERBOSE) ESP_LOGI(TAG, "GOT COMBO! '%s'", "change chan"/*keypress_combo_names[7]*/);
            change_channel_flag = true;
            clear_buffer(circBuff);         // This is mutexed, circbuff entries are cleared to 0, so dont use 0 for a key
        }
        
        if (VERBOSE) ESP_LOGI(TAG, "SW POLL - %s", gpio_status_names[encoder->state.sw_status]);
        
        if (*timerFlag) {
            timer_count+=1; // Increment count pre-emptively, as the timer is enabled and we wait for the flag to be initially set indicating a timer clock increment has passed
            if (VERBOSE) ESP_LOGI(TAG, "TIMER TRIG: %lu", timer_count);
            *timerFlag = false; // poll the switch and reset flags as necessary
        }
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
    static const bool VERBOSE = false;
    esp_err_t ret = ESP_OK;
    
    static battery_states_t temp_battery_state;

    // Init UART2 for development port (to be replaced with/accompanied by BT)
    uart2_init(U2_BAUD);
    app_gpio_init();

    circularBuffer keyPress_combo_buff = {0};
    init_buffer(&keyPress_combo_buff);

    gptimer_handle_t gptimerA = NULL;
    gptimer_handle_t gptimerB = NULL;

    encParams_t encAParams = {
        .TAG = "ENC_A",
        .encoder = &encA,
        .event = &eventA,
        .state = &stateA,
        .timer_handle = &gptimerA,
        .timer_cb = &gptimerA_callback,
        .timer_top = 1, // Timer compare match will reset this when reached. Use to count a number of elapsed timer cycles, and signify button press durations, or a count of presses within a duration
        .timerFlag = &timerAFlag,
        .pinA = ENCA_CHA_PIN,
        .pinB = ENCA_CHB_PIN,
        .pinSW = ENCA_SW_PIN,
        .comboBuff = &keyPress_combo_buff,
        .id = 1, // Using nonzero id's is essential as the circular buffer resets to zero
        .delay_ms = ENC_QUEUE_DELAY,
        .pos = &posA,
        .queue = xEncoderAQueue
    };

    // Create an instance of encParams_t for encoder B
    encParams_t encBParams = {
        .TAG = "ENC_B",
        .encoder = &encB,
        .event = &eventB,
        .state = &stateB,
        .timer_handle = &gptimerB,
        .timer_cb = &gptimerB_callback,
        .timer_top = 1, // 1uS software timer, 1000 ticks per 1mS
        .timerFlag = &timerBFlag,
        .pinA = ENCB_CHA_PIN,
        .pinB = ENCB_CHB_PIN,
        .pinSW = ENCB_SW_PIN,
        .comboBuff = &keyPress_combo_buff,
        .id = 2,                    // used for filling the button keypress combo circ buff
        .pos = &posB,
        .delay_ms = ENC_QUEUE_DELAY,
        .queue = xEncoderBQueue
    };

    adc_oneshot_unit_handle_t adc1_handle = NULL;
    adc_cali_handle_t adc1_cali_chan0_handle = NULL;
    adc_cali_handle_t adc1_cali_chan1_handle = NULL;

    adc_oneshot_unit_handle_t adc2_handle = NULL;
    adc_cali_handle_t adc2_cali_handle = NULL;

    adcOneshotParams_t vbatParams = {
        .TAG = "VBAT",
        .handle = &adc2_handle,
        .cali_handle = &adc2_cali_handle,
        .unit = ADC_UNIT_2,
        .channel = ADC2_CHAN0,
        .atten = ADC_APP_ATTEN,
        .delay_ms = 1000,
        .filt = &vbat_filt_handle,
        .vraw = &vbat_raw,
        .vcal = &vbat_cali,
        .vfilt = &vbat_filt,
    };

    adcOneshotParams_t vpotcParams = {
        .TAG = "VPOTC",
        .handle = &adc1_handle,
        .cali_handle = &adc1_cali_chan0_handle,
        .unit = ADC_UNIT_1,
        .channel = ADC1_CHAN1,
        .atten = ADC_APP_ATTEN,
        .delay_ms = 10,
        .filt = &vpotc_filt_handle,
        .vraw = &vpotc_raw,
        .vcal = &vpotc_cali,
        .vfilt = &vpotc_filt,
    };

    adcOneshotParams_t vpotdParams = {
        .TAG = "VPOTD",
        .handle = &adc1_handle,
        .cali_handle = &adc1_cali_chan1_handle,
        .unit = ADC_UNIT_1,
        .channel = ADC1_CHAN0,
        .atten = ADC_APP_ATTEN,
        .delay_ms = 10,
        .filt = &vpotd_filt_handle,
        .vraw = &vpotd_raw,
        .vcal = &vpotd_cali,
        .vfilt = &vpotd_filt,
    };

    // Create FreeRTOS tasks to handle various peripherals/functions
    xTaskCreate(rxTask,  "uart_rx_task",  1024*2, NULL, configMAX_PRIORITIES - 1,   NULL);
    xTaskCreate(txTask,  "uart_tx_task",  1024*4, NULL, configMAX_PRIORITIES - 2, NULL);
    xTaskCreate(adcTask, "vpotd_task", 1024*2, (void *)&vpotdParams, configMAX_PRIORITIES - 2,  NULL);
    xTaskCreate(adcTask, "vpotc_task", 1024*2, (void *)&vpotcParams, configMAX_PRIORITIES - 2,  NULL);
    xTaskCreate(adcTask, "vbat_task", 1024*2, (void *)&vbatParams, configMAX_PRIORITIES - 2,  NULL);
    xTaskCreate(displayTask, "display_task", 4096 * 2, NULL, configMAX_PRIORITIES, NULL);

    // Play with half step resolution to register direction change immediately
    xTaskCreate(encTask, "encA", 2048*4, (void *)&encAParams, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(encTask, "encB", 2048*4, (void *)&encBParams, configMAX_PRIORITIES - 1, NULL);

    //temp_battery_state = BAT_LOW;

    bool pin = 1;
    while(1) {

        vbat_assign_state(&batteryState, SCALE_VBAT(vbat_filt));
        if (temp_battery_state != batteryState) {
            temp_battery_state = batteryState;
            if (temp_battery_state == BAT_LOW) {
                gpio_set_level(LOWBATT_LED_PIN, HIGH);
            }
            else {
                gpio_set_level(LOWBATT_LED_PIN, LOW);
            }
        }
            
        if (VERBOSE) {
            ESP_LOGI(TAG, "Battery State   : %s", batteryStateNames[batteryState]);
            ESP_LOGI(TAG, "Heartbeat State : %s", gpio_status_names[pin]);
        }

        gpio_set_level(HEARTBEAT_LED_PIN, pin);
        pin = !pin;

        vTaskDelay(HEARTBEAT_BLINK_PERIOD_MS / portTICK_PERIOD_MS);
    }
}

/*========================================= END PROGRAM ============================================*/