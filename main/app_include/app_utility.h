/**
 * @file app_utility.h
 * @brief A place to store utlity functions and general include statements.
 * 
 */

#ifndef APP_UTILITY_H
#define APP_UTILITY_H

#ifdef __cplusplus
extern "C" {
#endif

// --------------- Includes --------------- //
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "rom/crc.h"            // For crc32 calculations

// Static functions

// User functions
uint32_t app_compute_crc32(char * str, int data_len);



#ifdef __cplusplus
}
#endif

#endif  // APP_UTILITY_H