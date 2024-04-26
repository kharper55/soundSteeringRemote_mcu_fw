/**
 * @file app_adc.h
 * @brief application specific definitions for UART2 operation on remote PCB for sound steering project.
 *        This UART port is intended for use only in development and is to be replaced with a wireless BT link.
 *        (courtesy of PRO_CPU :P)
 * 
 */

#ifndef APP_SPI_H
#define APP_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

// --------------- Includes --------------- //
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "lvgl_helpers.h"
#include "esp_timer.h"

// Image files
#include "app_img_universityCrest160x160.c"
#include "app_img_directivity160x160.c"

// Pins
// SPI "VSPI" & TFT ILI9341 DISPLAY
#define SPI_CS_PIN              GPIO_NUM_5  // VSPICS0
#define SPI_DC_PIN              GPIO_NUM_15 // DC
#define SPI_SCK_PIN             GPIO_NUM_18 // VSPICLK
#define SPI_MISO_PIN            GPIO_NUM_19 // VSPIQ
#define SPI_MOSI_PIN            GPIO_NUM_23 // VSPID
#define TFT_RST_PIN             GPIO_NUM_22 // VSPIWP
#define TFT_BACKLIGHT_PIN       GPIO_NUM_21 // PWM backlight adjustable through ESP-IDF SDKCONFIG

// Settings
#define LCD_HOST                  VSPI_HOST
#define LV_TICK_PERIOD_MS         1

typedef enum lv_color_t {
    APP_COLOR_RED           = 0xB11C1C,
    APP_COLOR_UNH_GOLD      = 0xADA50C,
    APP_COLOR_UNH_NAVY      = 0x003C65,
    APP_COLOR_UNH_NAVY_ALT  = 0x2A6082, /* A lighter shade of the above */
    APP_COLOR_GREEN         = 0x118F09,
    APP_COLOR_PCB_GREEN     = 0x002D04,
    APP_COLOR_PCB_GREEN_ALT = 0x007A0A,  /* A lighter shade of the above */
    APP_COLOR_BLACK         = 0x000000
} app_colors_t;

enum {
    APP_ICON_BATTERY_LOW,
    APP_ICON_BATTERY_MED,
    APP_ICON_BATTERY_FULL,
    APP_ICON_BATTERY_EMPTY,
    APP_ICON_CHARGE,
    APP_ICON_BLUETOOTH,
    APP_ICON_TX,
    APP_ICON_RX,
};

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

uint32_t knobColors[2] = {APP_COLOR_UNH_GOLD, APP_COLOR_RED}; // SWITCH RELEASED COLOR , SWITCH PRESSED COLOR
uint32_t textColors[2] = {APP_COLOR_BLACK, APP_COLOR_RED}; // STANDARD TEXT COLOR , HIGHLIGHTED TEXT COLOR
uint32_t batteryColors[3] = {APP_COLOR_RED, APP_COLOR_UNH_GOLD, APP_COLOR_PCB_GREEN_ALT};

const char app_icons[][4] = {
    "\xEF\x89\x83", /* LV_SYMBOL_BATTERY_1 */
    "\xEF\x89\x82", /* LV_SYMBOL_BATTERY_2 */
    "\xEF\x89\x80", /* LV_SYMBOL_BATTERY_FULL */
    "\xEF\x89\x84", /* LV_SYMBOL_BATTERY_EMPTY */
    "\xEF\x83\xA7", /* LV_SYMBOL_CHARGE */
    "\xEF\x8a\x93", /* LV_SYMBOL_BLUETOOTH */
    "\xEF\x82\x95", /* LV_SYMBOL_BELL */
    "\xEF\x83\xB3"  /* LV_SYMBOL_CALL */
};

void app_display_init(void);


#ifdef __cplusplus
}
#endif

#endif  // APP_SPI_H