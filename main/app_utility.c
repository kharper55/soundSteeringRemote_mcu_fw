#include "app_include/app_utility.h"

/*---------------------------------------------------------------
    Basic CRC32 Implementation (Little Endian)
---------------------------------------------------------------*/
uint32_t app_compute_crc32(char * str, int data_len) {
    return crc32_le(0, (const uint8_t *)str, strlen(str));
}