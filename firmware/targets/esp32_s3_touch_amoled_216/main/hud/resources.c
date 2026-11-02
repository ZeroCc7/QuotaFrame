#include "lvgl.h"

extern const uint8_t background_start[] asm("_binary_background_rgb565_start");
const lv_image_dsc_t eva_hud_background = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = 480, .h = 480, .stride = 960},
    .data_size = 480 * 480 * 2,
    .data = background_start,
};

extern const uint8_t guofeng_start[] asm("_binary_guofeng_rgb565_start");
const lv_image_dsc_t guofeng_background = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = 480, .h = 480, .stride = 960},
    .data_size = 480 * 480 * 2,
    .data = guofeng_start,
};

extern const uint8_t eva_clock_start[] asm("_binary_eva_clock_rgb565_start");
const lv_image_dsc_t eva_clock_background = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = 480, .h = 480, .stride = 960},
    .data_size = 480 * 480 * 2,
    .data = eva_clock_start,
};

extern const uint8_t guofeng_clock_start[] asm("_binary_guofeng_clock_rgb565_start");
const lv_image_dsc_t guofeng_clock_background = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = 480, .h = 480, .stride = 960},
    .data_size = 480 * 480 * 2,
    .data = guofeng_clock_start,
};
