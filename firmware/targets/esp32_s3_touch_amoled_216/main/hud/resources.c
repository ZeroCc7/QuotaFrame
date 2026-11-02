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

extern const uint8_t game_center_start[] asm("_binary_background_i8_start");
const lv_image_dsc_t game_center_background = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_I8,
               .w = 480, .h = 480, .stride = 480},
    .data_size = 1024 + 480 * 480,
    .data = game_center_start,
};

#define GAME_CENTER_INDEXED_IMAGE(name, width, height)                           \
    extern const uint8_t name##_start[] asm("_binary_" #name "_i8_start");     \
    const lv_image_dsc_t game_center_##name = {                                \
        .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_I8,    \
                   .w = width, .h = height, .stride = width},                   \
        .data_size = 1024 + width * height,                                     \
        .data = name##_start,                                                   \
    }

#define GAME_CENTER_ARGB_IMAGE(name, width, height)                             \
    extern const uint8_t name##_start[] asm("_binary_" #name "_argb8888_start"); \
    const lv_image_dsc_t game_center_##name = {                                \
        .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888, \
                   .w = width, .h = height, .stride = width * 4},              \
        .data_size = width * height * 4,                                        \
        .data = name##_start,                                                   \
    }

GAME_CENTER_INDEXED_IMAGE(title, 400, 131);
GAME_CENTER_INDEXED_IMAGE(number_bomb, 211, 241);
GAME_CENTER_INDEXED_IMAGE(penalty, 211, 241);
GAME_CENTER_INDEXED_IMAGE(return_hint, 250, 69);
GAME_CENTER_ARGB_IMAGE(father, 112, 112);
GAME_CENTER_ARGB_IMAGE(mother, 112, 112);
GAME_CENTER_ARGB_IMAGE(brother, 112, 112);
GAME_CENTER_ARGB_IMAGE(sister, 112, 112);
GAME_CENTER_ARGB_IMAGE(grandfather, 112, 112);
GAME_CENTER_ARGB_IMAGE(grandmother, 112, 112);
GAME_CENTER_INDEXED_IMAGE(safe_burst, 280, 280);
GAME_CENTER_INDEXED_IMAGE(boom_burst, 280, 280);
#undef GAME_CENTER_INDEXED_IMAGE
#undef GAME_CENTER_ARGB_IMAGE

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
