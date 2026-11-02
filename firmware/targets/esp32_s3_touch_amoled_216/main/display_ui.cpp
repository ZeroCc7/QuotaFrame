#include "display_ui.hpp"

#include <cstdio>
#include <ctime>

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "display_transport.hpp"
#include "hud/rolling_digits.hpp"
#include "performance_probe.hpp"
#include "usage_fonts/usage_fonts.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "usage_ota/presentation.hpp"

extern "C" {
LV_FONT_DECLARE(hud_digits);
LV_FONT_DECLARE(hud_small);
LV_FONT_DECLARE(guofeng_digits);
LV_FONT_DECLARE(eva_clock_font);
LV_FONT_DECLARE(guofeng_clock_font);
LV_FONT_DECLARE(guofeng_date_font);
LV_IMAGE_DECLARE(eva_hud_background);
LV_IMAGE_DECLARE(guofeng_background);
LV_IMAGE_DECLARE(eva_clock_background);
LV_IMAGE_DECLARE(guofeng_clock_background);
}

namespace usage_panel::amoled {
namespace {
constexpr char TAG[] = "amoled_ui";
constexpr uint32_t COLOR_BG     = 0x0A0C10;
// Black backing keeps uncovered pixels unlit during clock mode.
constexpr uint32_t COLOR_SAVER_BG = 0x000000;
constexpr uint32_t COLOR_PANEL  = 0x171B22;
constexpr uint32_t COLOR_BORDER = 0x272C35;
constexpr uint32_t COLOR_LINE   = 0x1D2128;
constexpr uint32_t COLOR_TRACK  = 0x252A33;
constexpr uint32_t COLOR_TEXT   = 0xF2F4F7;
constexpr uint32_t COLOR_MUTED  = 0x8B93A1;
constexpr uint32_t COLOR_DIM    = 0x565D68;
constexpr uint32_t COLOR_RESET  = 0xC7CCD4;
constexpr uint32_t COLOR_GOOD   = 0x3ECF8E;
constexpr uint32_t COLOR_WARN   = 0xF5B84C;
constexpr uint32_t COLOR_BAD    = 0xF26D6D;
constexpr uint32_t COLOR_BATTERY_OFF = 0x2C313B;
constexpr uint32_t COLOR_LOW_DIM = 0x8A6A2E;
// Screensaver usage underline colours. A thin underline needs more contrast
// than the clock digits to stay legible at viewing distance, so the base fill
// matches COLOR_DIM (the dimmest reliably legible element) while the track is
// dimmer still to keep a visible reference length.
constexpr uint32_t COLOR_SAVER_TRACK = 0x2C313B;
constexpr uint32_t COLOR_SAVER_BASE  = COLOR_DIM;
constexpr uint32_t COLOR_SAVER_WARN  = 0x737B86;
constexpr uint32_t COLOR_SAVER_BAD   = COLOR_MUTED;
constexpr int MARGIN      = 24;
constexpr int CARD_W      = 432;
constexpr int CARD_PAD    = 20;
constexpr int CARD_RADIUS = 16;
constexpr int HEADER_H    = 50;
constexpr int BAR_H_DETAIL   = 16;
constexpr int PAIRING_BOTTOM = 52;
constexpr int SCREEN_W    = 480;
constexpr int SCREEN_H    = 480;
constexpr int BATTERY_GROUP_W = 44;
constexpr int BATTERY_BODY_W = 30;
constexpr int BATTERY_BODY_H = 14;
constexpr int OTA_CARD_Y         = 98;
constexpr int OTA_CARD_H         = 212;
constexpr int OTA_CARD_ROW_Y     = CARD_PAD;
constexpr int OTA_CARD_VALUE_Y   = 74;
constexpr int OTA_CARD_THIRD_ROW_Y = 142;
constexpr int OTA_CARD_HINT_Y    = 172;
constexpr int OTA_BUTTON_Y       = OTA_CARD_Y + OTA_CARD_H + MARGIN;
constexpr int OTA_BUTTON_W       = 204;
constexpr int OTA_BUTTON_H       = 66;
constexpr int OTA_WAIT_TITLE_Y   = 210;
constexpr int OTA_WAIT_DETAIL_Y  = 258;
constexpr int OTA_FAIL_ROW_Y     = 200;
constexpr int OTA_FAIL_DETAIL_Y  = 254;
constexpr int SAVER_CLOCK_Y      = 182;
constexpr int SAVER_CLOCK_H      = 69;   // eva_clock_font line height.
constexpr int SAVER_DATE_Y       = 317;
constexpr int SAVER_DATE_H       = 28;
constexpr int SAVER_USAGE_Y      = 354;
constexpr int SAVER_ROW_H        = 16;
constexpr int SAVER_ROW_GAP      = 2;
constexpr int SAVER_BAR_W        = 220;
// A hairline underline loses to the clock at viewing distance no matter
// how its colour is tuned.
constexpr int SAVER_BAR_H        = 3;
constexpr int SAVER_PCT_GAP      = 12;
// Fits a three-digit percentage without wrapping.
constexpr int SAVER_PCT_W        = 40;
// Both rows share the same centred bar and label positions.
constexpr int SAVER_PCT_SLOT     = SAVER_PCT_GAP + SAVER_PCT_W;
static_assert(SAVER_PCT_SLOT % 2 == 0);  // Halved below, so it must be even.
constexpr int SAVER_BAR_X        = SAVER_PCT_SLOT;
constexpr int SAVER_BAR_ALERT_X  = SAVER_PCT_SLOT / 2;
constexpr int SAVER_PCT_X        = SAVER_BAR_ALERT_X + SAVER_BAR_W + SAVER_PCT_GAP;
constexpr int SAVER_PCT_Y        = 2;
constexpr int SAVER_USAGE_W      = SAVER_BAR_X + SAVER_BAR_W + SAVER_PCT_SLOT;
constexpr int SAVER_USAGE_X      = (SCREEN_W - SAVER_USAGE_W) / 2;
// The row is centred, the resting bar is centred within it, and so is the
// alerting bar-plus-number. The group therefore reads as an underline of the
// clock rather than as a component of its own, in either state.
static_assert(2 * SAVER_USAGE_X + SAVER_USAGE_W == SCREEN_W);
static_assert(2 * (SAVER_USAGE_X + SAVER_BAR_X) + SAVER_BAR_W == SCREEN_W);
static_assert(
    2 * (SAVER_USAGE_X + SAVER_BAR_ALERT_X) + SAVER_BAR_W + SAVER_PCT_GAP +
        SAVER_PCT_W ==
    SCREEN_W);
static_assert(SAVER_PCT_X + SAVER_PCT_W <= SAVER_USAGE_W);
static_assert(SAVER_PCT_Y + 12 <= SAVER_ROW_H);  // 12 px is the font's line height.
static_assert(SAVER_CLOCK_Y + SAVER_CLOCK_H < SAVER_DATE_Y);
static_assert(SAVER_DATE_Y + SAVER_DATE_H <= SAVER_USAGE_Y);

void rounder_event(lv_event_t* event)
{
    auto* area = static_cast<lv_area_t*>(lv_event_get_param(event));
    area->x1 = (area->x1 >> 1) << 1;
    area->y1 = (area->y1 >> 1) << 1;
    area->x2 = ((area->x2 >> 1) << 1) + 1;
    area->y2 = ((area->y2 >> 1) << 1) + 1;
}

lv_display_t* start_display(
    ScreenRotation initial_rotation, esp_lcd_touch_handle_t* ret_touch,
    lv_indev_t** ret_input_device, esp_lcd_panel_io_handle_t* ret_panel_io)
{
    if (ret_touch != nullptr) {
        *ret_touch = nullptr;
    }
    if (ret_input_device != nullptr) {
        *ret_input_device = nullptr;
    }

    constexpr DisplayBufferPlan plan = make_display_buffer_plan();
    static_assert(
        display_buffer_plan_is_dma_safe(plan),
        "display buffers must be internal and fit the SPI DMA budget");

    esp_lv_adapter_config_t adapter_config{};
    adapter_config.task_stack_size = ESP_LV_ADAPTER_DEFAULT_STACK_SIZE;
    adapter_config.task_priority = ESP_LV_ADAPTER_DEFAULT_TASK_PRIORITY;
    adapter_config.task_core_id = ESP_LV_ADAPTER_DEFAULT_TASK_CORE_ID;
    adapter_config.tick_period_ms = ESP_LV_ADAPTER_DEFAULT_TICK_PERIOD_MS;
    adapter_config.task_min_delay_ms =
        ESP_LV_ADAPTER_DEFAULT_TASK_MIN_DELAY_MS;
    adapter_config.task_max_delay_ms =
        ESP_LV_ADAPTER_DEFAULT_TASK_MAX_DELAY_MS;
    adapter_config.stack_in_psram = false;
    adapter_config.auto_sleep.enable = false;
    adapter_config.auto_sleep.mode =
        ESP_LV_ADAPTER_AUTO_SLEEP_MODE_DISABLED;
    adapter_config.auto_sleep.idle_timeout_ms =
        ESP_LV_ADAPTER_DEFAULT_AUTO_SLEEP_TIMEOUT_MS;
    esp_err_t error = esp_lv_adapter_init(&adapter_config);
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG, "LVGL adapter initialization failed: %s",
            esp_err_to_name(error));
        return nullptr;
    }

    const bsp_display_config_t panel_config{
        .max_transfer_sz = static_cast<int>(plan.transfer_bytes),
    };
    esp_lcd_panel_handle_t panel = nullptr;
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    error = bsp_display_new(&panel_config, &panel, &panel_io);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "display initialization failed: %s", esp_err_to_name(error));
        return nullptr;
    }
    error = bsp_display_rotation_set(
        static_cast<bsp_display_rotation_t>(initial_rotation));
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "initial display rotation failed: %s",
                 esp_err_to_name(error));
        return nullptr;
    }

    esp_lv_adapter_display_config_t display_config{};
    display_config.panel = panel;
    display_config.panel_io = panel_io;
    *ret_panel_io = panel_io;
    display_config.profile.interface = ESP_LV_ADAPTER_PANEL_IF_OTHER;
    display_config.profile.rotation = ESP_LV_ADAPTER_ROTATE_0;
    display_config.profile.hor_res = BSP_LCD_H_RES;
    display_config.profile.ver_res = BSP_LCD_V_RES;
    display_config.profile.buffer_height = plan.buffer_height;
    display_config.profile.use_psram = plan.use_psram;
    display_config.profile.enable_ppa_accel = false;
    display_config.profile.require_double_buffer =
        plan.require_double_buffer;
    display_config.profile.mono_layout =
        ESP_LV_ADAPTER_MONO_LAYOUT_NONE;
    display_config.tear_avoid_mode =
        ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
    display_config.te_sync.gpio_num = -1;
#if CONFIG_WS_USAGE_PANEL_PERFORMANCE_PROBE
    ESP_ERROR_CHECK(esp_lv_adapter_set_default_display_idf_callback_registration_enabled(false));
#endif
    lv_display_t* display = esp_lv_adapter_register_display(&display_config);
    if (display == nullptr) {
        ESP_LOGE(TAG, "LVGL display registration failed");
        return nullptr;
    }
    lv_display_add_event_cb(
        display, rounder_event, LV_EVENT_INVALIDATE_AREA, nullptr);

    bsp_display_cfg_t touch_board_config{};
    touch_board_config.rotation = ESP_LV_ADAPTER_ROTATE_0;
    touch_board_config.tear_avoid_mode =
        ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
    const auto initial_touch_mapping = touch_mapping(initial_rotation);
    touch_board_config.touch_flags.swap_xy = initial_touch_mapping.swap_xy;
    touch_board_config.touch_flags.mirror_x = initial_touch_mapping.mirror_x;
    touch_board_config.touch_flags.mirror_y = initial_touch_mapping.mirror_y;
    esp_lcd_touch_handle_t touch = nullptr;
    error = bsp_touch_new(&touch_board_config, &touch);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "touch initialization failed: %s", esp_err_to_name(error));
        return nullptr;
    }
    esp_lv_adapter_touch_config_t touch_config{};
    touch_config.disp = display;
    touch_config.handle = touch;
    touch_config.scale.x = 1.0f;
    touch_config.scale.y = 1.0f;
    touch_config.multi_touch.mode =
        ESP_LV_ADAPTER_TOUCH_MODE_SINGLE;
    touch_config.multi_touch.pointers = 1;
    lv_indev_t* input_device = esp_lv_adapter_register_touch(&touch_config);
    if (input_device == nullptr) {
        ESP_LOGE(TAG, "LVGL touch registration failed");
        return nullptr;
    }
    if (ret_touch != nullptr) {
        *ret_touch = touch;
    }
    if (ret_input_device != nullptr) {
        *ret_input_device = input_device;
    }

    error = bsp_display_brightness_init();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "display brightness initialization failed: %s",
                 esp_err_to_name(error));
        return nullptr;
    }
    error = bsp_display_brightness_set(
        CONFIG_WS_USAGE_PANEL_DISPLAY_BRIGHTNESS_PERCENT);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "display brightness setting failed: %s",
                 esp_err_to_name(error));
        return nullptr;
    }
#if CONFIG_WS_USAGE_PANEL_PERFORMANCE_PROBE
    if (!attach_performance_probe(display, panel_io)) return nullptr;
#endif
    error = esp_lv_adapter_start();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "LVGL task start failed: %s", esp_err_to_name(error));
        return nullptr;
    }
    return display;
}

class DisplayLock {
public:
    DisplayLock() : locked_(esp_lv_adapter_lock(-1) == ESP_OK) {}

    ~DisplayLock()
    {
        if (locked_) {
            esp_lv_adapter_unlock();
        }
    }

    explicit operator bool() const { return locked_; }

    DisplayLock(const DisplayLock&) = delete;
    DisplayLock& operator=(const DisplayLock&) = delete;

private:
    bool locked_;
};

esp_err_t apply_touch_mapping(
    esp_lcd_touch_handle_t touch, const TouchMapping& mapping)
{
    const esp_err_t swap_error =
        esp_lcd_touch_set_swap_xy(touch, mapping.swap_xy);
    const esp_err_t mirror_x_error =
        esp_lcd_touch_set_mirror_x(touch, mapping.mirror_x);
    const esp_err_t mirror_y_error =
        esp_lcd_touch_set_mirror_y(touch, mapping.mirror_y);
    return swap_error != ESP_OK ? swap_error :
           mirror_x_error != ESP_OK ? mirror_x_error : mirror_y_error;
}

constexpr uint32_t quota_color_hex(unsigned remaining)
{
    uint32_t low = 0xFF424C;
    uint32_t high = 0xFF674F;
    unsigned position = remaining;
    unsigned span = 20;
    if (remaining > 60) {
        low = 0xFF965F;
        high = 0xFFBC83;
        position = remaining - 60;
        span = 40;
    } else if (remaining > 20) {
        low = 0xFF674F;
        high = 0xFF965F;
        position = remaining - 20;
        span = 40;
    }
    uint32_t color = 0xFF0000;
    for (unsigned shift = 0; shift <= 8; shift += 8) {
        const unsigned start = (low >> shift) & 0xFF;
        const unsigned end = (high >> shift) & 0xFF;
        color |= ((start * (span - position) + end * position + span / 2) / span) << shift;
    }
    return color;
}

static_assert(quota_color_hex(100) == 0xFFBC83);
static_assert(quota_color_hex(80) == 0xFFA971);
static_assert(quota_color_hex(60) == 0xFF965F);
static_assert(quota_color_hex(40) == 0xFF7F57);
static_assert(quota_color_hex(20) == 0xFF674F);
static_assert(quota_color_hex(10) == 0xFF554E);
static_assert(quota_color_hex(0) == 0xFF424C);

constexpr uint32_t saver_usage_color_hex(ScreensaverUsageLevel level)
{
    return level == ScreensaverUsageLevel::Bad    ? COLOR_SAVER_BAD
         : level == ScreensaverUsageLevel::Warn   ? COLOR_SAVER_WARN
                                                  : COLOR_SAVER_BASE;
}

// The resting tiers stay below the clock, so the clock keeps being the subject.
// The top tier is allowed to reach it, but never exceed it: at 95% the row is
// showing its number as well, and being noticed is the whole point.
static_assert(saver_usage_color_hex(ScreensaverUsageLevel::Base) < COLOR_MUTED);
static_assert(saver_usage_color_hex(ScreensaverUsageLevel::Warn) < COLOR_MUTED);
static_assert(saver_usage_color_hex(ScreensaverUsageLevel::Bad) <= COLOR_MUTED);

int saver_fill_width(uint8_t percent)
{
    return (int(percent) * SAVER_BAR_W + 50) / 100;
}

lv_color_t state_color(DisplayState state)
{
    switch (state) {
        case DisplayState::Online:
        case DisplayState::Partial:
            return lv_color_hex(COLOR_MUTED);
        case DisplayState::NoData:
        case DisplayState::Offline:
        default:
            return lv_color_hex(COLOR_DIM);
    }
}

void style_panel(lv_obj_t* panel, uint32_t background = COLOR_PANEL,
                 int radius = CARD_RADIUS)
{
    lv_obj_set_style_bg_color(panel, lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(panel, radius, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* make_label(lv_obj_t* parent, const char* text, int x, int y,
                     const lv_font_t* font, uint32_t color = COLOR_TEXT)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

void style_bar(lv_obj_t* bar, int height)
{
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_TRACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_TEXT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, height / 2, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, height / 2, LV_PART_INDICATOR);
}

}  // namespace

bool DisplayUi::begin(ScreenRotation initial_rotation)
{
    display_ = start_display(
        initial_rotation, &touch_, &input_device_, &panel_io_);
    if (display_ == nullptr || input_device_ == nullptr || touch_ == nullptr) {
        ESP_LOGE(TAG, "BSP display initialization failed");
        return false;
    }
    rotation_ = initial_rotation;
    brightness_percent_ = CONFIG_WS_USAGE_PANEL_DISPLAY_BRIGHTNESS_PERCENT;
    DisplayLock lock;
    if (!lock) {
        ESP_LOGE(TAG, "failed to acquire display lock");
        return false;
    }

    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, touch_down_event, LV_EVENT_PRESSED, this);
    // Child widgets do not always bubble presses to the screen.
    lv_indev_add_event_cb(
        input_device_, touch_down_event, LV_EVENT_PRESSED, this);

    lv_indev_set_long_press_time(input_device_, 800);
    lv_indev_add_event_cb(input_device_, long_press_event, LV_EVENT_LONG_PRESSED, this);
    build_header(screen);
    build_provider_page(screen);
    build_guofeng_page(screen);
    build_screensaver(screen);
    build_settings(screen);
    build_pairing_overlay(screen);
    build_ota_overlay(screen);
    lv_display_add_event_cb(
        display_, transition_refresh_ready, LV_EVENT_REFR_READY, this);

    return true;
}

bool DisplayUi::set_rotation(ScreenRotation rotation)
{
    if (display_ == nullptr || input_device_ == nullptr || touch_ == nullptr) {
        ESP_LOGE(TAG, "cannot rotate before display initialization");
        return false;
    }
    if (rotation == rotation_) {
        return true;
    }

    DisplayLock lock;
    if (!lock) {
        ESP_LOGE(TAG, "failed to acquire display lock for rotation");
        return false;
    }

    const auto previous_mapping = touch_mapping(rotation_);
    const auto mapping = touch_mapping(rotation);
    const esp_err_t display_error = bsp_display_rotation_set(
        static_cast<bsp_display_rotation_t>(rotation));
    if (display_error != ESP_OK) {
        ESP_LOGE(TAG, "display rotation failed: %s",
                 esp_err_to_name(display_error));
        return false;
    }

    const esp_err_t touch_error = apply_touch_mapping(touch_, mapping);
    if (touch_error != ESP_OK) {
        const esp_err_t rollback_display_error = bsp_display_rotation_set(
            static_cast<bsp_display_rotation_t>(rotation_));
        const esp_err_t rollback_touch_error =
            apply_touch_mapping(touch_, previous_mapping);
        ESP_LOGE(TAG, "touch rotation failed: %s",
                 esp_err_to_name(touch_error));
        if (rollback_display_error != ESP_OK ||
            rollback_touch_error != ESP_OK) {
            ESP_LOGE(TAG, "rotation rollback failed: display=%s touch=%s",
                     esp_err_to_name(rollback_display_error),
                     esp_err_to_name(rollback_touch_error));
        }
        return false;
    }

    rotation_ = rotation;
    lv_obj_invalidate(lv_screen_active());
    return true;
}

#if CONFIG_WS_USAGE_PANEL_MEMORY_PROBE
void DisplayUi::run_memory_probe()
{
    DisplayLock lock;
    if (!lock) {
        ESP_LOGE(TAG, "failed to acquire display lock for memory probe");
        return;
    }

    constexpr uint32_t internal_caps =
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const size_t internal_free_before =
        heap_caps_get_free_size(internal_caps);
    const size_t largest_before =
        heap_caps_get_largest_free_block(internal_caps);
    lv_mem_monitor_t monitor_before{};
    lv_mem_monitor(&monitor_before);

    lv_obj_t* probe = lv_obj_create(lv_screen_active());
    lv_obj_add_flag(probe, LV_OBJ_FLAG_HIDDEN);
    for (uint32_t index = 0; index < 32; ++index) {
        lv_obj_t* label = lv_label_create(probe);
        lv_label_set_text_fmt(label, "PROBE %lu",
                              static_cast<unsigned long>(index));
    }
    for (uint32_t index = 0; index < 32; ++index) {
        lv_obj_t* button = lv_button_create(probe);
        lv_obj_set_size(button, 96, 40);
    }

    const size_t internal_free_after =
        heap_caps_get_free_size(internal_caps);
    const size_t largest_after =
        heap_caps_get_largest_free_block(internal_caps);
    lv_mem_monitor_t monitor_after{};
    lv_mem_monitor(&monitor_after);
    ESP_LOGI(
        TAG,
        "MEMORY_PROBE stress internal_free_before=%u internal_free_after=%u "
        "largest_before=%u largest_after=%u lvgl_used_before=%u "
        "lvgl_used_after=%u",
        static_cast<unsigned>(internal_free_before),
        static_cast<unsigned>(internal_free_after),
        static_cast<unsigned>(largest_before),
        static_cast<unsigned>(largest_after),
        static_cast<unsigned>(monitor_before.total_size -
                              monitor_before.free_size),
        static_cast<unsigned>(monitor_after.total_size -
                              monitor_after.free_size));

    lv_obj_delete(probe);
    lv_display_remove_event_cb_with_user_data(display_, probe_refresh_event, this);
    probe_refresh_sample_ = 0;
    probe_refresh_active_ = true;
    lv_display_add_event_cb(
        display_, probe_refresh_event, LV_EVENT_REFR_READY, this);
    start_next_probe_refresh();
}

void DisplayUi::start_next_probe_refresh()
{
    if (probe_refresh_sample_ >= 90) {
        probe_refresh_active_ = false;
            lv_obj_invalidate(lv_screen_active());
        return;
    }

    probe_refresh_started_us_ = esp_timer_get_time();
    lv_obj_invalidate(lv_screen_active());
}

void DisplayUi::probe_refresh_event(lv_event_t* event)
{
    auto* ui = static_cast<DisplayUi*>(lv_event_get_user_data(event));
    if (!ui->probe_refresh_active_) {
        return;
    }

    const uint64_t elapsed_us = static_cast<uint64_t>(
        esp_timer_get_time() - ui->probe_refresh_started_us_);
    const uint32_t sample = ui->probe_refresh_sample_;
    ESP_LOGI(
        TAG, "MEMORY_PROBE refresh cycle=%u page=%u elapsed_us=%llu",
        static_cast<unsigned>(sample / 3),
        static_cast<unsigned>(sample % 3),
        static_cast<unsigned long long>(elapsed_us));
    ++ui->probe_refresh_sample_;
    ui->start_next_probe_refresh();
}
#endif

void DisplayUi::build_header(lv_obj_t* screen)
{
    clock_label_ = make_label(
        screen, "--:--", MARGIN, 13,
        &lv_font_montserrat_22, COLOR_TEXT);

    lv_obj_t* status_group = lv_obj_create(screen);
    lv_obj_set_pos(status_group, 311, 13);
    lv_obj_set_size(status_group, 91, 28);
    lv_obj_set_style_bg_opa(status_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_group, 0, 0);
    lv_obj_set_style_pad_all(status_group, 0, 0);
    lv_obj_remove_flag(status_group, LV_OBJ_FLAG_SCROLLABLE);

    status_dot_ = lv_obj_create(status_group);
    lv_obj_set_pos(status_dot_, 0, 10);
    lv_obj_set_size(status_dot_, 8, 8);
    lv_obj_set_style_radius(status_dot_, 4, 0);
    lv_obj_set_style_bg_color(status_dot_, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_bg_opa(status_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_dot_, 0, 0);
    lv_obj_set_style_pad_all(status_dot_, 0, 0);
    lv_obj_remove_flag(status_dot_, LV_OBJ_FLAG_SCROLLABLE);

    connection_label_ = make_label(
        status_group, "WAITING", 16, 6,
        &lv_font_montserrat_14, COLOR_RESET);
    lv_obj_set_width(connection_label_, 75);
    lv_obj_set_style_text_align(connection_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_letter_space(connection_label_, 1, 0);

    battery_group_ = lv_obj_create(screen);
    lv_obj_set_pos(battery_group_, SCREEN_W - MARGIN - BATTERY_GROUP_W, 13);
    lv_obj_set_size(battery_group_, BATTERY_GROUP_W, 28);
    lv_obj_set_style_bg_opa(battery_group_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(battery_group_, 0, 0);
    lv_obj_set_style_pad_all(battery_group_, 0, 0);
    lv_obj_remove_flag(battery_group_, LV_OBJ_FLAG_SCROLLABLE);

    charge_symbol_ = make_label(
        battery_group_, LV_SYMBOL_CHARGE, 0, 6,
        &lv_font_montserrat_14, COLOR_RESET);
    lv_obj_set_width(charge_symbol_, 7);
    lv_obj_set_style_text_align(charge_symbol_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_opa(charge_symbol_, LV_OPA_TRANSP, 0);

    lv_obj_t* body = lv_obj_create(battery_group_);
    lv_obj_set_pos(body, 12, 7);
    lv_obj_set_size(body, BATTERY_BODY_W, BATTERY_BODY_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 1, 0);
    lv_obj_set_style_border_color(body, lv_color_hex(COLOR_DIM), 0);
    lv_obj_set_style_radius(body, 3, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 4; ++i) {
        battery_segments_[i] = lv_obj_create(body);
        lv_obj_set_pos(battery_segments_[i], 2 + i * 6, 2);
        lv_obj_set_size(battery_segments_[i], 5, 8);
        lv_obj_set_style_border_width(battery_segments_[i], 0, 0);
        lv_obj_set_style_radius(battery_segments_[i], 1, 0);
        lv_obj_set_style_pad_all(battery_segments_[i], 0, 0);
        lv_obj_remove_flag(battery_segments_[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_t* terminal = lv_obj_create(battery_group_);
    lv_obj_set_pos(terminal, 42, 11);
    lv_obj_set_size(terminal, 2, 6);
    lv_obj_set_style_bg_color(terminal, lv_color_hex(COLOR_DIM), 0);
    lv_obj_set_style_bg_opa(terminal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(terminal, 0, 0);
    lv_obj_set_style_radius(terminal, 1, 0);
    lv_obj_set_style_pad_all(terminal, 0, 0);
    lv_obj_remove_flag(terminal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* divider = lv_obj_create(screen);
    lv_obj_set_pos(divider, MARGIN, HEADER_H);
    lv_obj_set_size(divider, CARD_W, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
}

void DisplayUi::build_screensaver(lv_obj_t* screen)
{
    screensaver_panel_ = lv_obj_create(screen);
    lv_obj_set_pos(screensaver_panel_, 0, 0);
    lv_obj_set_size(screensaver_panel_, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(
        screensaver_panel_, lv_color_hex(COLOR_SAVER_BG), 0);
    lv_obj_set_style_bg_opa(screensaver_panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screensaver_panel_, 0, 0);
    lv_obj_set_style_pad_all(screensaver_panel_, 0, 0);
    lv_obj_remove_flag(screensaver_panel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screensaver_panel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        screensaver_panel_, touch_down_event, LV_EVENT_PRESSED, this);

    screensaver_content_ = lv_obj_create(screensaver_panel_);
    lv_obj_set_pos(screensaver_content_, 0, 0);
    lv_obj_set_size(screensaver_content_, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_opa(screensaver_content_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(screensaver_content_, 0, 0);
    lv_obj_set_style_pad_all(screensaver_content_, 0, 0);
    lv_obj_remove_flag(screensaver_content_, LV_OBJ_FLAG_SCROLLABLE);

    screensaver_background_ = lv_image_create(screensaver_content_);
    lv_obj_remove_flag(screensaver_background_, LV_OBJ_FLAG_CLICKABLE);

    screensaver_seal_ = lv_obj_create(screensaver_content_);
    lv_obj_remove_style_all(screensaver_seal_);
    lv_obj_set_pos(screensaver_seal_, 234, 108);
    lv_obj_set_size(screensaver_seal_, 12, 12);
    lv_obj_set_style_bg_color(screensaver_seal_, lv_color_hex(0xA95443), 0);
    lv_obj_set_style_bg_opa(screensaver_seal_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screensaver_seal_, LV_OBJ_FLAG_CLICKABLE);

    screensaver_time_ = make_label(
        screensaver_content_, "--:--", 0, SAVER_CLOCK_Y,
        &eva_clock_font, COLOR_MUTED);
    lv_obj_set_width(screensaver_time_, SCREEN_W);
    lv_obj_set_style_text_align(screensaver_time_, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* date_row = lv_obj_create(screensaver_content_);
    screensaver_date_row_ = date_row;
    lv_obj_set_pos(date_row, 0, SAVER_DATE_Y);
    lv_obj_set_size(date_row, SCREEN_W, SAVER_DATE_H);
    lv_obj_set_style_bg_opa(date_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(date_row, 0, 0);
    lv_obj_set_style_pad_all(date_row, 0, 0);
    lv_obj_set_style_pad_column(date_row, 12, 0);
    lv_obj_set_flex_flow(date_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        date_row, LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(date_row, LV_OBJ_FLAG_SCROLLABLE);
    screensaver_weekday_ = make_label(
        date_row, "---", 0, 0, &montserrat_semibold_20, COLOR_DIM);
    lv_obj_set_style_text_letter_space(screensaver_weekday_, 2, 0);
    lv_obj_t* date_dot = lv_obj_create(date_row);
    screensaver_date_dot_ = date_dot;
    lv_obj_set_size(date_dot, 4, 4);
    lv_obj_set_style_radius(date_dot, 2, 0);
    lv_obj_set_style_bg_color(date_dot, lv_color_hex(COLOR_DIM), 0);
    lv_obj_set_style_bg_opa(date_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(date_dot, 0, 0);
    lv_obj_set_style_pad_all(date_dot, 0, 0);
    screensaver_month_day_ = make_label(
        date_row, "-- --", 0, 0, &montserrat_semibold_20, COLOR_DIM);
    lv_obj_set_style_text_letter_space(screensaver_month_day_, 2, 0);

    for (size_t i = 0; i < sizeof(screensaver_usage_) / sizeof(screensaver_usage_[0]); ++i) {
        lv_obj_t* row = lv_obj_create(screensaver_content_);
        lv_obj_set_pos(
            row, SAVER_USAGE_X,
            SAVER_USAGE_Y + i * (SAVER_ROW_H + SAVER_ROW_GAP));
        lv_obj_set_size(row, SAVER_USAGE_W, SAVER_ROW_H);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* track = lv_obj_create(row);
        lv_obj_set_pos(track, SAVER_BAR_X, (SAVER_ROW_H - SAVER_BAR_H) / 2);
        lv_obj_set_size(track, SAVER_BAR_W, SAVER_BAR_H);
        lv_obj_set_style_bg_color(track, lv_color_hex(COLOR_SAVER_TRACK), 0);
        lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(track, 0, 0);
        lv_obj_set_style_radius(track, SAVER_BAR_H / 2, 0);
        lv_obj_set_style_pad_all(track, 0, 0);
        lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);
        screensaver_usage_[i].track = track;

        lv_obj_t* fill = lv_obj_create(row);
        lv_obj_set_pos(fill, SAVER_BAR_X, (SAVER_ROW_H - SAVER_BAR_H) / 2);
        lv_obj_set_size(fill, SAVER_BAR_W, SAVER_BAR_H);
        lv_obj_set_style_bg_color(fill, lv_color_hex(COLOR_SAVER_BASE), 0);
        lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(fill, 0, 0);
        lv_obj_set_style_radius(fill, SAVER_BAR_H / 2, 0);
        lv_obj_set_style_pad_all(fill, 0, 0);
        lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(fill, LV_OBJ_FLAG_HIDDEN);
        screensaver_usage_[i].fill = fill;

        lv_obj_t* percent = make_label(
            row, "", SAVER_PCT_X, SAVER_PCT_Y,
            &montserrat_semibold_13, COLOR_SAVER_BASE);
        lv_obj_set_width(percent, SAVER_PCT_W);
        // Clip rather than wrap, so no future glyph change can push a second
        // line into the row below.
        lv_label_set_long_mode(percent, LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_style_text_letter_space(percent, 0, 0);
        lv_obj_add_flag(percent, LV_OBJ_FLAG_HIDDEN);
        screensaver_usage_[i].percent = percent;
    }

    screensaver_hint_ = make_label(
        screensaver_content_, "TAP OR SHAKE TO WAKE", 0, 433,
        &montserrat_semibold_13, COLOR_BATTERY_OFF);
    lv_obj_set_width(screensaver_hint_, SCREEN_W);
    lv_obj_set_style_text_align(screensaver_hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(screensaver_hint_, 1, 0);

    screensaver_low_group_ = lv_obj_create(screensaver_content_);
    lv_obj_set_pos(screensaver_low_group_, 382, 18);
    lv_obj_set_size(screensaver_low_group_, 74, 22);
    lv_obj_set_style_bg_opa(screensaver_low_group_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(screensaver_low_group_, 0, 0);
    lv_obj_set_style_pad_all(screensaver_low_group_, 0, 0);
    lv_obj_remove_flag(screensaver_low_group_, LV_OBJ_FLAG_SCROLLABLE);
    make_label(
        screensaver_low_group_, "LOW", 0, 4,
        &montserrat_semibold_13, COLOR_LOW_DIM);
    lv_obj_t* low_body = lv_obj_create(screensaver_low_group_);
    lv_obj_set_pos(low_body, 42, 4);
    lv_obj_set_size(low_body, 30, 14);
    lv_obj_set_style_bg_opa(low_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(low_body, 1, 0);
    lv_obj_set_style_border_color(low_body, lv_color_hex(0x3A4048), 0);
    lv_obj_set_style_radius(low_body, 2, 0);
    lv_obj_set_style_pad_all(low_body, 0, 0);
    for (int i = 0; i < 4; ++i) {
        screensaver_low_segments_[i] = lv_obj_create(low_body);
        lv_obj_set_pos(screensaver_low_segments_[i], 2 + i * 6, 2);
        lv_obj_set_size(screensaver_low_segments_[i], 5, 8);
        lv_obj_set_style_border_width(screensaver_low_segments_[i], 0, 0);
        lv_obj_set_style_radius(screensaver_low_segments_[i], 1, 0);
        lv_obj_set_style_pad_all(screensaver_low_segments_[i], 0, 0);
    }
    lv_obj_t* low_terminal = lv_obj_create(screensaver_low_group_);
    lv_obj_set_pos(low_terminal, 72, 8);
    lv_obj_set_size(low_terminal, 2, 6);
    lv_obj_set_style_bg_color(low_terminal, lv_color_hex(0x3A4048), 0);
    lv_obj_set_style_bg_opa(low_terminal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(low_terminal, 0, 0);
    lv_obj_set_style_radius(low_terminal, 1, 0);
    lv_obj_set_style_pad_all(low_terminal, 0, 0);
    apply_clock_theme();
    lv_obj_add_flag(screensaver_panel_, LV_OBJ_FLAG_HIDDEN);
}

void DisplayUi::apply_clock_theme()
{
    if (clock_theme_applied_ && clock_theme_ == theme_) return;
    const bool guofeng = theme_ == DisplayTheme::Guofeng;
    lv_image_set_src(screensaver_background_,
        guofeng ? &guofeng_clock_background : &eva_clock_background);
    lv_obj_set_style_bg_color(screensaver_panel_,
        lv_color_hex(guofeng ? 0xE5DDCD : COLOR_SAVER_BG), 0);
    if (guofeng) {
        lv_obj_add_flag(screensaver_background_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(screensaver_seal_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(screensaver_background_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(screensaver_seal_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_opa(screensaver_background_, 210, 0);
    lv_obj_set_style_text_font(screensaver_time_,
        guofeng ? &guofeng_clock_font : &eva_clock_font, 0);
    lv_obj_set_width(screensaver_time_, LV_SIZE_CONTENT);
    lv_obj_set_pos(screensaver_time_, guofeng ? 90 : 64, guofeng ? 158 : 182);
    lv_obj_set_style_transform_pivot_x(screensaver_time_, 0, 0);
    lv_obj_set_style_transform_pivot_y(screensaver_time_, 0, 0);
    lv_obj_set_y(screensaver_date_row_, guofeng ? 292 : SAVER_DATE_Y);
    lv_obj_set_style_text_font(screensaver_month_day_,
        guofeng ? &guofeng_date_font : &montserrat_semibold_20, 0);
    lv_obj_set_style_text_letter_space(screensaver_month_day_, guofeng ? 0 : 2, 0);
    if (guofeng) {
        lv_obj_add_flag(screensaver_weekday_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(screensaver_date_dot_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(screensaver_weekday_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(screensaver_date_dot_, LV_OBJ_FLAG_HIDDEN);
    }
    const lv_color_t date_color = lv_color_hex(guofeng ? 0x6D7064 : 0xBE9B83);
    lv_obj_set_style_text_color(screensaver_weekday_, date_color, 0);
    lv_obj_set_style_text_color(screensaver_month_day_, date_color, 0);
    lv_obj_set_style_bg_color(screensaver_date_dot_, date_color, 0);
    lv_obj_set_style_text_color(screensaver_hint_,
        lv_color_hex(guofeng ? 0x99988A : 0x895C4C), 0);
    lv_obj_set_y(lv_obj_get_parent(screensaver_usage_[0].track),
                 guofeng ? 361 : SAVER_USAGE_Y);
    lv_obj_set_style_bg_color(screensaver_usage_[0].track,
        lv_color_hex(guofeng ? 0xCBC6B6 : COLOR_SAVER_TRACK), 0);
    clock_theme_ = theme_;
    clock_theme_applied_ = true;
}

void DisplayUi::build_provider_page(lv_obj_t* screen)
{
    auto& widgets = codex_page_;
    widgets.panel = lv_obj_create(screen);
    lv_obj_remove_style_all(widgets.panel);
    lv_obj_set_pos(widgets.panel, 0, 0);
    lv_obj_set_size(widgets.panel, SCREEN_W, SCREEN_H);
    lv_obj_remove_flag(widgets.panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* background = lv_image_create(widgets.panel);
    lv_image_set_src(background, &eva_hud_background);
    lv_obj_remove_flag(background, LV_OBJ_FLAG_CLICKABLE);

    auto& metric = widgets.short_window;
    metric.primary = true;
    // Match the source ring scaled by 0.56 around the full-screen point (240, 200).
    for (int i = 0; i < 28; ++i) {
        lv_obj_t* segment = lv_arc_create(widgets.panel);
        lv_obj_remove_style_all(segment);
        lv_obj_set_pos(segment, 58, 18);
        lv_obj_set_size(segment, 364, 364);
        lv_arc_set_rotation(segment, 135);
        lv_arc_set_bg_angles(segment, i * 270 / 28 + 1, (i + 1) * 270 / 28 - 1);
        lv_obj_set_style_arc_width(segment, 29, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(segment, false, LV_PART_MAIN);
        lv_obj_set_style_arc_color(segment, lv_color_hex(0xFF3D34), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(segment, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_remove_flag(segment, LV_OBJ_FLAG_CLICKABLE);
        metric.segments[i] = segment;
    }
    metric.value = make_label(widgets.panel, "--", 131, 158, &hud_digits, 0xFF424C);
    lv_obj_set_style_transform_scale_y(metric.value, 384, 0);
    metric.unit = make_label(widgets.panel, "%", 340, 229, &hud_small, 0xFF424C);
    lv_obj_set_style_transform_scale_x(metric.unit, 384, 0);
    lv_obj_set_style_transform_scale_y(metric.unit, 384, 0);
    metric.caption = make_label(widgets.panel, "QUOTA UNAVAILABLE", 155, 278,
                                &lv_font_montserrat_12, 0xA7A7A7);
    lv_obj_set_width(metric.caption, 170);
    lv_obj_set_style_text_align(metric.caption, LV_TEXT_ALIGN_CENTER, 0);
    metric.reset = make_label(widgets.panel, "--/-- --:--", 217, 395,
                              &hud_small, 0xFFF7EE);
    auto& week = widgets.week_window;
    week.value = make_label(widgets.panel, "1W --%", 158, 293,
                            &hud_small, 0xFFB9A8);
    lv_obj_set_style_transform_scale_y(week.value, 320, 0);
    week.reset = make_label(widgets.panel, "--/-- --:--", 217, 436,
                            &hud_small, 0xFFF7EE);
    build_metric_rollers(metric);
    build_metric_rollers(week);
}

void DisplayUi::build_guofeng_page(lv_obj_t* screen)
{
    auto& widgets = guofeng_page_;
    widgets.panel = lv_obj_create(screen);
    lv_obj_remove_style_all(widgets.panel);
    lv_obj_set_size(widgets.panel, SCREEN_W, SCREEN_H);
    lv_obj_remove_flag(widgets.panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* background = lv_image_create(widgets.panel);
    lv_image_set_src(background, &guofeng_background);
    lv_obj_remove_flag(background, LV_OBJ_FLAG_CLICKABLE);

    auto& metric = widgets.short_window;
    metric.primary = true;
    metric.guofeng = true;
    metric.ring = lv_arc_create(widgets.panel);
    lv_obj_remove_style_all(metric.ring);
    lv_obj_set_pos(metric.ring, 122, 40);
    lv_obj_set_size(metric.ring, 268, 268);
    lv_arc_set_rotation(metric.ring, 270);
    lv_arc_set_bg_angles(metric.ring, 0, 360);
    lv_obj_set_style_arc_width(metric.ring, 17, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(metric.ring, false, LV_PART_MAIN);
    lv_obj_set_style_arc_color(metric.ring, lv_color_hex(0xF3D18B), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(metric.ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(metric.ring, LV_OBJ_FLAG_CLICKABLE);
    metric.value = make_label(widgets.panel, "--", 164, 139, &guofeng_digits, 0xFFF0C7);
    metric.unit = make_label(widgets.panel, "%", 339, 176,
                             &lv_font_montserrat_30, 0xFFF0C7);
    metric.reset = make_label(widgets.panel, "--/-- --:--", 254, 341,
                              &lv_font_montserrat_20, 0x171C1C);
    auto& week = widgets.week_window;
    week.guofeng = true;
    week.value = make_label(widgets.panel, "1W --%", 196, 208,
                            &lv_font_montserrat_14, 0xFFF0C7);
    week.reset = make_label(widgets.panel, "--/-- --:--", 254, 397,
                            &lv_font_montserrat_20, 0x171C1C);
    build_metric_rollers(metric);
    build_metric_rollers(week);
    lv_obj_add_flag(widgets.panel, LV_OBJ_FLAG_HIDDEN);
}

void DisplayUi::build_settings(lv_obj_t* screen)
{
    settings_panel_ = lv_obj_create(screen);
    lv_obj_remove_style_all(settings_panel_);
    lv_obj_set_pos(settings_panel_, 0, HEADER_H + 1);
    lv_obj_set_size(settings_panel_, SCREEN_W, SCREEN_H - HEADER_H - 1);
    lv_obj_set_style_bg_color(settings_panel_, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(settings_panel_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(settings_panel_, LV_OBJ_FLAG_SCROLLABLE);
    auto* title = make_label(settings_panel_, "BRIGHTNESS", MARGIN, 12,
                            &lv_font_montserrat_16, COLOR_MUTED);
    lv_obj_set_style_text_letter_space(title, 1, 0);
    settings_brightness_ = make_label(settings_panel_, "", MARGIN, 38, &lv_font_montserrat_48);
    lv_obj_t* divider = lv_obj_create(settings_panel_);
    lv_obj_remove_style_all(divider);
    lv_obj_set_pos(divider, MARGIN, 119);
    lv_obj_set_size(divider, CARD_W, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    title = make_label(settings_panel_, "AUTO CLOCK", MARGIN, 216,
                       &lv_font_montserrat_16, COLOR_MUTED);
    lv_obj_set_style_text_letter_space(title, 1, 0);
    settings_timeout_ = make_label(settings_panel_, "AFTER IDLE / MIN", MARGIN, 241,
                                   &lv_font_montserrat_14, COLOR_MUTED);
    settings_hint_ = make_label(settings_panel_, "", MARGIN, 330,
                               &montserrat_semibold_13, COLOR_TEXT);
    make_label(settings_panel_, "THEME", MARGIN, 126, &lv_font_montserrat_16, COLOR_MUTED);
    const char* labels[] = {"-", "+", "OFF", "1", "5", "10", "30", "SAVE", "CANCEL", "EVA", "GUOFENG"};
    const SettingsAction actions[] = {SettingsAction::Dimmer, SettingsAction::Brighter,
        SettingsAction::TimeoutOff, SettingsAction::Timeout1, SettingsAction::Timeout5,
        SettingsAction::Timeout10, SettingsAction::Timeout30, SettingsAction::Save, SettingsAction::Cancel,
        SettingsAction::ThemeEva, SettingsAction::ThemeGuofeng};
    const int x[] = {288, 376, 24, 112, 200, 288, 376, 250, MARGIN, MARGIN, 250};
    const int y[] = {38, 38, 268, 268, 268, 268, 268, 355, 355, 150, 150};
    for (size_t i = 0; i < 11; ++i) {
        lv_obj_t* button = lv_button_create(settings_panel_);
        settings_buttons_[i] = button;
        lv_obj_remove_style_all(button);
        lv_obj_set_pos(button, x[i], y[i]);
        lv_obj_set_size(button, i < 7 ? 80 : 206, i < 2 ? 64 : i < 7 ? 52 : i < 9 ? 58 : 50);
        style_panel(button);
        lv_obj_set_style_text_color(button, lv_color_hex(COLOR_MUTED), 0);
        lv_obj_set_style_text_font(button, i < 2 ? &lv_font_montserrat_30 : &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_letter_space(button, 1, 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(COLOR_TRACK), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(button, lv_color_hex(COLOR_MUTED), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(button, lv_color_hex(COLOR_TEXT), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(button, lv_color_hex(COLOR_BG), LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(button, lv_color_hex(COLOR_RESET), LV_STATE_CHECKED | LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(button, lv_color_hex(COLOR_PANEL), LV_STATE_DISABLED);
        lv_obj_set_style_text_color(button, lv_color_hex(COLOR_DIM), LV_STATE_DISABLED);
        if (i == 7) lv_obj_add_state(button, LV_STATE_CHECKED);
        settings_bindings_[i] = {this, actions[i]};
        lv_obj_add_event_cb(button, settings_event, LV_EVENT_CLICKED, &settings_bindings_[i]);
        lv_obj_t* label = lv_label_create(button);
        lv_label_set_text(label, labels[i]);
        lv_obj_center(label);
    }
    lv_obj_add_flag(settings_panel_, LV_OBJ_FLAG_HIDDEN);
}

bool DisplayUi::apply_settings(const DisplaySettings& settings, bool visible, bool save_error, bool modified)
{
    DisplayLock lock;
    if (!lock) return false;
    const bool brightness_changed = brightness_percent_ != settings.brightness;
    if (brightness_changed) {
        lv_anim_delete(this, transition_brightness);
        if (!transition_refresh_pending_ && !set_transition_brightness(settings.brightness))
            return false;
        brightness_percent_ = settings.brightness;
    }
    theme_ = settings.theme;
    lv_obj_set_state(settings_buttons_[9], LV_STATE_CHECKED, theme_ == DisplayTheme::Eva);
    lv_obj_set_state(settings_buttons_[10], LV_STATE_CHECKED, theme_ == DisplayTheme::Guofeng);
    const bool was_visible = settings_visible_.exchange(visible, std::memory_order_relaxed);
    lv_label_set_text_fmt(settings_brightness_, "%u%%", settings.brightness);
    lv_obj_set_state(settings_buttons_[0], LV_STATE_DISABLED, settings.brightness <= 1);
    lv_obj_set_state(settings_buttons_[1], LV_STATE_DISABLED, settings.brightness >= 100);
    lv_obj_set_state(settings_buttons_[7], LV_STATE_DISABLED, !modified && !save_error);
    bool standard_timeout = false;
    for (size_t i = 0; i < 5; ++i) {
        const bool selected = settings.clock_timeout_seconds == CLOCK_TIMEOUT_CHOICES[i];
        lv_obj_set_state(settings_buttons_[i + 2], LV_STATE_CHECKED, selected);
        standard_timeout = standard_timeout || selected;
    }
    if (standard_timeout) lv_label_set_text(settings_timeout_, "AFTER IDLE / MIN");
    else lv_label_set_text_fmt(settings_timeout_, "AFTER IDLE / MIN - CURRENT %lu SEC",
                              static_cast<unsigned long>(settings.clock_timeout_seconds));
    lv_label_set_text(settings_hint_, save_error ? "SAVE FAILED - TRY AGAIN" : "");
    if (visible && !was_visible) {
        lv_obj_remove_flag(settings_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(settings_panel_);
    } else if (!visible) lv_obj_add_flag(settings_panel_, LV_OBJ_FLAG_HIDDEN);
    return true;
}

void DisplayUi::settings_event(lv_event_t* event)
{
    auto* binding = static_cast<SettingsBinding*>(lv_event_get_user_data(event));
    if (binding->ui->settings_visible_.load(std::memory_order_relaxed))
        binding->ui->settings_action_.store(binding->action, std::memory_order_relaxed);
}

void DisplayUi::long_press_event(lv_event_t* event)
{
    auto* ui = static_cast<DisplayUi*>(lv_event_get_user_data(event));
    if (!ui->settings_visible_.load(std::memory_order_relaxed) &&
        !ui->ota_active_.load(std::memory_order_relaxed) &&
        ui->input_enabled_.load(std::memory_order_relaxed)) {
        ui->settings_action_.store(SettingsAction::Open, std::memory_order_relaxed);
        lv_indev_wait_release(ui->input_device_);
    }
}

void DisplayUi::build_pairing_overlay(lv_obj_t* screen)
{
    pairing_panel_ = lv_obj_create(screen);
    lv_obj_set_pos(pairing_panel_, 0, HEADER_H + 1);
    lv_obj_set_size(
        pairing_panel_, SCREEN_W,
        SCREEN_H - (HEADER_H + 1) - PAIRING_BOTTOM);
    lv_obj_set_style_bg_color(pairing_panel_, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(pairing_panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pairing_panel_, 0, 0);
    lv_obj_set_style_pad_all(pairing_panel_, 0, 0);
    lv_obj_remove_flag(pairing_panel_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* pairing_card = lv_obj_create(pairing_panel_);
    lv_obj_set_size(pairing_card, 380, 200);
    style_panel(pairing_card, COLOR_PANEL, 18);
    lv_obj_set_style_border_color(
        pairing_card, lv_color_hex(0x2C313B), 0);
    lv_obj_center(pairing_card);

    lv_obj_t* title =
        make_label(
            pairing_card, "PAIRING", 0, 22,
            &lv_font_montserrat_14, COLOR_MUTED);
    lv_obj_set_width(title, 380);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    pairing_code_ =
        make_label(
            pairing_card, "000000", 0, 56,
            &lv_font_montserrat_30, COLOR_TEXT);
    lv_obj_set_width(pairing_code_, 380);
    lv_obj_set_style_text_align(pairing_code_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(pairing_code_, 10, 0);
    lv_obj_t* instruction =
        make_label(
            pairing_card, "Enter this code on your computer", 0, 119,
            &lv_font_montserrat_14, COLOR_RESET);
    lv_obj_set_width(instruction, 380);
    lv_obj_set_style_text_align(instruction, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t* supporting =
        make_label(
            pairing_card, "The panel will connect after confirmation", 0, 157,
            &lv_font_montserrat_14, COLOR_DIM);
    lv_obj_set_width(supporting, 380);
    lv_obj_set_style_text_align(supporting, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(pairing_panel_, LV_OBJ_FLAG_HIDDEN);
}

void DisplayUi::build_ota_overlay(lv_obj_t* screen)
{
    ota_panel_ = lv_obj_create(screen);
    lv_obj_set_pos(ota_panel_, 0, 0);
    lv_obj_set_size(ota_panel_, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(ota_panel_, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(ota_panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ota_panel_, 0, 0);
    lv_obj_set_style_pad_all(ota_panel_, 0, 0);
    lv_obj_remove_flag(ota_panel_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* header = make_label(
        ota_panel_, "FIRMWARE", MARGIN, 18,
        &lv_font_montserrat_14, COLOR_MUTED);
    lv_obj_set_style_text_letter_space(header, 2, 0);
    lv_obj_t* divider = lv_obj_create(ota_panel_);
    lv_obj_set_pos(divider, MARGIN, HEADER_H);
    lv_obj_set_size(divider, CARD_W, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

    // Confirming and receiving share one card: only the text inside and
    // which secondary row (countdown/note vs. progress bar/hint) is shown
    // changes between the two phases.
    ota_card_ = lv_obj_create(ota_panel_);
    lv_obj_set_pos(ota_card_, MARGIN, OTA_CARD_Y);
    lv_obj_set_size(ota_card_, CARD_W, OTA_CARD_H);
    style_panel(ota_card_);

    ota_card_label_ = make_label(
        ota_card_, "", CARD_PAD, OTA_CARD_ROW_Y,
        &lv_font_montserrat_14, COLOR_MUTED);
    lv_obj_set_style_text_letter_space(ota_card_label_, 1, 0);

    ota_card_countdown_ = make_label(
        ota_card_, "", CARD_PAD, OTA_CARD_ROW_Y,
        &lv_font_montserrat_14, COLOR_RESET);
    lv_obj_set_width(ota_card_countdown_, CARD_W - 2 * CARD_PAD);
    lv_obj_set_style_text_align(ota_card_countdown_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_letter_space(ota_card_countdown_, 1, 0);

    ota_card_value_ = make_label(
        ota_card_, "", 0, OTA_CARD_VALUE_Y,
        &lv_font_montserrat_48, COLOR_TEXT);
    lv_obj_set_width(ota_card_value_, CARD_W);
    lv_label_set_long_mode(ota_card_value_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(ota_card_value_, LV_TEXT_ALIGN_CENTER, 0);

    ota_card_note_ = make_label(
        ota_card_, "AUTO-CANCELS", 0, OTA_CARD_THIRD_ROW_Y,
        &lv_font_montserrat_12, COLOR_DIM);
    lv_obj_set_width(ota_card_note_, CARD_W);
    lv_obj_set_style_text_align(ota_card_note_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(ota_card_note_, 1, 0);

    ota_progress_ = lv_bar_create(ota_card_);
    lv_obj_set_pos(ota_progress_, CARD_PAD, OTA_CARD_THIRD_ROW_Y);
    lv_obj_set_size(ota_progress_, CARD_W - 2 * CARD_PAD, BAR_H_DETAIL);
    style_bar(ota_progress_, BAR_H_DETAIL);
    lv_bar_set_value(ota_progress_, 0, LV_ANIM_OFF);

    ota_card_hint_ = make_label(
        ota_card_, "DO NOT POWER OFF", 0, OTA_CARD_HINT_Y,
        &lv_font_montserrat_14, COLOR_TEXT);
    lv_obj_set_width(ota_card_hint_, CARD_W);
    lv_obj_set_style_text_align(ota_card_hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(ota_card_hint_, 1, 0);

    ota_confirm_button_ = lv_button_create(ota_panel_);
    lv_obj_set_pos(ota_confirm_button_, MARGIN, OTA_BUTTON_Y);
    lv_obj_set_size(ota_confirm_button_, OTA_BUTTON_W, OTA_BUTTON_H);
    lv_obj_set_style_bg_color(
        ota_confirm_button_, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_radius(ota_confirm_button_, 14, 0);
    lv_obj_add_event_cb(
        ota_confirm_button_, ota_confirm_event, LV_EVENT_CLICKED, this);
    lv_obj_t* label = make_label(
        ota_confirm_button_, "CONFIRM", 0, 0,
        &lv_font_montserrat_16, COLOR_BG);
    lv_obj_center(label);

    ota_deny_button_ = lv_button_create(ota_panel_);
    lv_obj_set_pos(
        ota_deny_button_, MARGIN + OTA_BUTTON_W + MARGIN, OTA_BUTTON_Y);
    lv_obj_set_size(ota_deny_button_, OTA_BUTTON_W, OTA_BUTTON_H);
    lv_obj_set_style_bg_color(ota_deny_button_, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_border_width(ota_deny_button_, 1, 0);
    lv_obj_set_style_border_color(
        ota_deny_button_, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_radius(ota_deny_button_, 14, 0);
    lv_obj_add_event_cb(
        ota_deny_button_, ota_deny_event, LV_EVENT_CLICKED, this);
    label = make_label(
        ota_deny_button_, "CANCEL", 0, 0,
        &lv_font_montserrat_16, COLOR_TEXT);
    lv_obj_center(label);

    // Verifying/rebooting: plain centered status text, no card.
    ota_title_ = make_label(
        ota_panel_, "", 0, OTA_WAIT_TITLE_Y,
        &lv_font_montserrat_30, COLOR_TEXT);
    lv_obj_set_width(ota_title_, SCREEN_W);
    lv_obj_set_style_text_align(ota_title_, LV_TEXT_ALIGN_CENTER, 0);

    ota_detail_ = make_label(
        ota_panel_, "", 0, OTA_WAIT_DETAIL_Y,
        &lv_font_montserrat_14, COLOR_MUTED);
    lv_obj_set_width(ota_detail_, SCREEN_W);
    lv_obj_set_style_text_align(ota_detail_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(ota_detail_, 1, 0);

    // Failed: a status dot beside the headline, sized to its content and
    // centered as a unit rather than assuming a fixed text width.
    ota_fail_row_ = lv_obj_create(ota_panel_);
    lv_obj_set_size(ota_fail_row_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ota_fail_row_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ota_fail_row_, 0, 0);
    lv_obj_set_style_pad_all(ota_fail_row_, 0, 0);
    lv_obj_set_style_pad_column(ota_fail_row_, 12, 0);
    lv_obj_set_flex_flow(ota_fail_row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        ota_fail_row_, LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(ota_fail_row_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(ota_fail_row_, LV_ALIGN_TOP_MID, 0, OTA_FAIL_ROW_Y);

    lv_obj_t* fail_dot = lv_obj_create(ota_fail_row_);
    lv_obj_set_size(fail_dot, 14, 14);
    lv_obj_set_style_radius(fail_dot, 7, 0);
    lv_obj_set_style_bg_color(fail_dot, lv_color_hex(COLOR_BAD), 0);
    lv_obj_set_style_bg_opa(fail_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fail_dot, 0, 0);
    lv_obj_remove_flag(fail_dot, LV_OBJ_FLAG_SCROLLABLE);

    ota_fail_title_ = make_label(
        ota_fail_row_, "UPDATE FAILED", 0, 0,
        &lv_font_montserrat_34, COLOR_BAD);

    lv_obj_add_flag(ota_panel_, LV_OBJ_FLAG_HIDDEN);
}

void DisplayUi::update_metric(
    MetricWidgets& widgets, const UsageWindow& window,
    bool has_data, uint32_t, bool fresh)
{
    const bool available = has_data && window.present;
    const unsigned percent = available ? 100 - window.used_percent : 0;
    const uint32_t color = fresh && available
        ? (widgets.guofeng ? 0xFFF0C7 : quota_color_hex(percent)) : COLOR_DIM;
    if (widgets.unit != nullptr) {
        if (available) lv_obj_remove_flag(widgets.unit, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(widgets.unit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(widgets.unit, lv_color_hex(color), 0);
    }
    if (widgets.caption != nullptr) {
        lv_label_set_text(widgets.caption, available ? "QUOTA LEFT" : "QUOTA UNAVAILABLE");
    }
    widgets.available = available;
    widgets.fresh = fresh;
    const int32_t target = static_cast<int32_t>(percent * 10);
    if (widgets.target != target) {
        widgets.animation_from = widgets.displayed;
        if (widgets.target >= 0) widgets.animation_duration = 600;
        widgets.target = target;
        widgets.animation_started = lv_tick_get();
    }
    if (!available) {
        widgets.target = -1;
        widgets.displayed = 0;
    }
    paint_metric(widgets, lv_tick_get());
    if (available && window.has_reset) {
        // Match the desktop HUD's explicit Asia/Shanghai (UTC+8) reset dates.
        const time_t reset = static_cast<time_t>(window.reset_at) + 8 * 60 * 60;
        struct tm calendar{};
        if (gmtime_r(&reset, &calendar) != nullptr) {
            lv_label_set_text_fmt(widgets.reset, "%02d/%02d %02d:%02d",
                calendar.tm_mon + 1, calendar.tm_mday, calendar.tm_hour, calendar.tm_min);
        } else {
            lv_label_set_text(widgets.reset, "--/-- --:--");
        }
    } else {
        lv_label_set_text(widgets.reset, "--/-- --:--");
    }
    lv_obj_set_style_text_color(widgets.reset,
        lv_color_hex(widgets.guofeng ? (fresh ? 0x171C1C : 0x69736B)
                                   : (fresh ? 0xFFF7EE : COLOR_DIM)), 0);
    lv_obj_update_layout(widgets.reset);
    const int date_width = lv_obj_get_width(widgets.reset);
    if (date_width > 0) lv_obj_set_style_transform_scale_x(widgets.reset, (widgets.guofeng ? 154 : 145) * 256 / date_width, 0);
}

void DisplayUi::build_metric_rollers(MetricWidgets& widgets)
{
    const lv_font_t* font = lv_obj_get_style_text_font(widgets.value, LV_PART_MAIN);
    widgets.digit_height = lv_font_get_line_height(font);
    for (char digit = '0'; digit <= '9'; ++digit) {
        const char text[] = {digit, 0};
        const int width = lv_text_get_width(text, 1, font, 0);
        if (width > widgets.digit_width) widgets.digit_width = width;
    }
    const int width = widgets.digit_width;
    widgets.rollers = lv_obj_create(lv_obj_get_parent(widgets.value));
    lv_obj_remove_style_all(widgets.rollers);
    lv_obj_set_pos(widgets.rollers, lv_obj_get_style_x(widgets.value, LV_PART_MAIN), lv_obj_get_style_y(widgets.value, LV_PART_MAIN));
    lv_obj_set_size(widgets.rollers, 4 * width + width / 3, widgets.digit_height);
    lv_obj_remove_flag(widgets.rollers, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(widgets.rollers, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(widgets.rollers, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_transform_scale_y(widgets.rollers,
        lv_obj_get_style_transform_scale_y(widgets.value, LV_PART_MAIN), 0);
    for (int i = 0; i < 4; ++i) {
        auto* clip = lv_obj_create(widgets.rollers);
        widgets.digit_clips[i] = clip;
        lv_obj_remove_style_all(clip);
        lv_obj_set_size(clip, width, widgets.digit_height);
        lv_obj_remove_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(clip, LV_OBJ_FLAG_CLICKABLE);
        for (int j = 0; j < 2; ++j) {
            auto* label = make_label(clip, "0", 0, j * widgets.digit_height, font, COLOR_DIM);
            widgets.digit_labels[i][j] = label;
            lv_obj_set_width(label, width);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        }
    }
    // Punctuation and the weekly prefix stay still while the reels move.
    widgets.decimal = make_label(widgets.rollers, ".", 3 * width, 0, font, COLOR_DIM);
    if (!widgets.primary) {
        widgets.prefix = make_label(widgets.rollers, "1W", 0, 0, font, COLOR_DIM);
        widgets.suffix = make_label(widgets.rollers, "%", 0, 0, font, COLOR_DIM);
    }
    lv_obj_add_flag(widgets.rollers, LV_OBJ_FLAG_HIDDEN);
}

void DisplayUi::paint_metric_rollers(MetricWidgets& widgets, int32_t progress, uint32_t color)
{
    if (!widgets.available) {
        lv_obj_add_flag(widgets.rollers, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(widgets.value, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(widgets.value, widgets.primary ? "--" : "1W --%");
        return;
    }
    lv_obj_add_flag(widgets.value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(widgets.rollers, LV_OBJ_FLAG_HIDDEN);
    const int width = widgets.digit_width;
    const int from_leading = widgets.animation_from >= 1000 ? 0 : widgets.animation_from >= 100 ? 1 : 2;
    const int target_leading = widgets.target >= 1000 ? 0 : widgets.target >= 100 ? 1 : 2;
    const int leading_progress = progress < 128 ? progress * 8 : 1024;
    const int skip = width * from_leading +
        width * (target_leading - from_leading) * leading_progress / 1024;
    const int natural_width = 4 * width + width / 3 - skip;
    const int target_width = widgets.guofeng ? (widgets.primary ? 172 : 120)
                                            : (widgets.primary ? 207 : 165);
    // The weekly prefix has two fixed cells; retain its original total width.
    const int prefix_width = widgets.primary ? 0 : 2 * width;
    const int suffix_width = widgets.primary ? 0 : width;
    lv_obj_set_x(widgets.rollers, lv_obj_get_style_x(widgets.value, LV_PART_MAIN));
    lv_obj_set_style_transform_scale_x(widgets.rollers,
        target_width * 256 / (natural_width + prefix_width + suffix_width), 0);
    const int places[] = {1000, 100, 10, 1};
    for (int i = 0; i < 4; ++i) {
        auto* clip = widgets.digit_clips[i];
        if ((i < from_leading && i < target_leading) ||
            (leading_progress == 1024 && i < target_leading)) {
            lv_obj_add_flag(clip, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(clip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(clip, i * width + (i == 3 ? width / 3 : 0) - skip + prefix_width);
        const auto frame = reel_frame(widgets.animation_from, widgets.target, places[i],
            i < target_leading ? leading_progress : progress,
            widgets.animation_duration == 2000 && i >= 2);
        const int offset = frame.phase * widgets.digit_height / 1024;
        auto* old_label = widgets.digit_labels[i][0];
        auto* new_label = widgets.digit_labels[i][1];
        const char old_text[] = {i < target_leading && frame.digit == 0 ? '\0' : static_cast<char>('0' + frame.digit), '\0'};
        const char next_text[] = {i < target_leading && frame.next == 0 ? '\0' : static_cast<char>('0' + frame.next), '\0'};
        if (widgets.reel_digits[i] != frame.digit || lv_label_get_text(new_label)[0] != next_text[0]) {
            lv_label_set_text(old_label, old_text);
            lv_label_set_text(new_label, next_text);
            widgets.reel_digits[i] = frame.digit;
        }
        lv_obj_set_y(old_label, frame.direction * offset);
        lv_obj_set_y(new_label, frame.direction * (offset - widgets.digit_height));
        lv_obj_set_style_text_color(old_label, lv_color_hex(color), 0);
        lv_obj_set_style_text_color(new_label, lv_color_hex(color), 0);
    }
    lv_obj_set_x(widgets.decimal, 3 * width - skip + prefix_width);
    lv_obj_set_style_text_color(widgets.decimal, lv_color_hex(color), 0);
    if (widgets.prefix != nullptr) {
        lv_obj_set_style_text_color(widgets.prefix, lv_color_hex(color), 0);
        lv_obj_set_x(widgets.suffix, natural_width + prefix_width);
        lv_obj_set_style_text_color(widgets.suffix, lv_color_hex(color), 0);
    }
}

void DisplayUi::paint_metric(MetricWidgets& widgets, uint32_t tick)
{
    int32_t eased = 1024;
    if (widgets.available) {
        const uint32_t elapsed = tick - widgets.animation_started;
        const int32_t t = elapsed >= widgets.animation_duration ? 1024
            : static_cast<int32_t>(elapsed * 1024 / widgets.animation_duration);
        eased = t * (2048 - t) / 1024;
        widgets.displayed = widgets.animation_from +
            (widgets.target - widgets.animation_from) * eased / 1024;
    }
    // Quantize the idle breath, so a 16 ms animation tick need not repaint it.
    const unsigned phase = tick % 4800;
    const unsigned wave = phase < 2400 ? phase : 4800 - phase;
    const int breath = widgets.fresh ? 231 + static_cast<int>(wave / 200) * 2 : 255;
    const int32_t value = widgets.available ? widgets.displayed : -1;
    if (value == widgets.painted_value && breath == widgets.painted_breath &&
        widgets.fresh == widgets.painted_fresh && eased == widgets.painted_progress) return;
    widgets.painted_progress = eased;
    widgets.painted_value = value;
    widgets.painted_breath = breath;
    widgets.painted_fresh = widgets.fresh;
    const unsigned percent = static_cast<unsigned>(widgets.displayed / 10);
    const uint32_t color = widgets.fresh && widgets.available
        ? (widgets.guofeng ? 0xFFF0C7 : quota_color_hex(percent)) : COLOR_DIM;
    lv_obj_set_style_text_color(widgets.value, lv_color_hex(color), 0);
    paint_metric_rollers(widgets, eased, color);
    if (widgets.ring != nullptr) {
        const unsigned angle = static_cast<unsigned>(widgets.displayed) * 360 / 1000;
        lv_arc_set_bg_angles(widgets.ring, 360 - angle, 360);
        lv_obj_set_style_arc_color(widgets.ring,
            lv_color_hex(widgets.fresh ? (percent < 10 ? 0xDCA365 : 0xF3D18B) : COLOR_DIM), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(widgets.ring,
            widgets.available && widgets.displayed > 0 ? breath : 0, LV_PART_MAIN);
    }
    for (size_t i = 0; i < 28; ++i) {
        if (widgets.segments[i] == nullptr) continue;
        const int coverage = widgets.displayed * 28 - static_cast<int>(i * 1000);
        const int opacity = !widgets.available || coverage <= 0 ? 0
            : coverage >= 1000 ? breath : coverage * breath / 1000;
        lv_obj_set_style_arc_color(widgets.segments[i], lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(widgets.segments[i], opacity, LV_PART_MAIN);
    }
}

void DisplayUi::animate_hud(lv_timer_t* timer)
{
    auto* ui = static_cast<DisplayUi*>(lv_timer_get_user_data(timer));
    auto& page = ui->theme_ == DisplayTheme::Guofeng ? ui->guofeng_page_ : ui->codex_page_;
    const uint32_t tick = lv_tick_get();
    paint_metric(page.short_window, tick);
    paint_metric(page.week_window, tick);
}

void DisplayUi::set_hud_motion(bool visible, bool replay)
{
    if (hud_timer_ == nullptr) {
        hud_timer_ = lv_timer_create(animate_hud, 16, this);
    }
    if (hud_timer_ == nullptr) return;
    if (!visible) {
        lv_timer_pause(hud_timer_);
        return;
    }
    auto& page = theme_ == DisplayTheme::Guofeng ? guofeng_page_ : codex_page_;
    MetricWidgets* metrics[] = {&page.short_window, &page.week_window};
    for (auto* metric : metrics) {
        if (replay) {
            metric->target = -1;
            metric->displayed = 1000;
            metric->animation_duration = 2000;
        }
    }
    lv_timer_resume(hud_timer_);
}

void DisplayUi::update_screensaver_usage(const ScreensaverUsageView& usage)
{
    // Keep the bar and its optional percentage centred together.
    const bool any_percent = usage.rows[0].show_percent;
    const int bar_x = any_percent ? SAVER_BAR_ALERT_X : SAVER_BAR_X;

    for (size_t i = 0; i < sizeof(screensaver_usage_) / sizeof(screensaver_usage_[0]); ++i) {
        const ScreensaverUsageRow& row = usage.rows[i];
        const ScreensaverUsageWidgets& widgets = screensaver_usage_[i];
        const lv_color_t color =
            lv_color_hex(theme_ == DisplayTheme::Guofeng
                ? (row.level == ScreensaverUsageLevel::Bad ? 0xA95443
                   : row.level == ScreensaverUsageLevel::Warn ? 0x8D7955 : 0x777D6A)
                : saver_usage_color_hex(row.level));
        lv_obj_set_x(widgets.track, bar_x);
        lv_obj_set_x(widgets.fill, bar_x);
        const int width = row.has_data ? saver_fill_width(row.percent) : 0;
        if (width > 0) {
            lv_obj_set_width(widgets.fill, width);
            lv_obj_set_style_bg_color(widgets.fill, color, 0);
            lv_obj_remove_flag(widgets.fill, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widgets.fill, LV_OBJ_FLAG_HIDDEN);
        }
        if (row.show_percent) {
            lv_label_set_text_fmt(widgets.percent, "%u%%", row.percent);
            lv_obj_set_style_text_color(widgets.percent, color, 0);
            lv_obj_remove_flag(widgets.percent, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widgets.percent, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void DisplayUi::update_provider(
    ProviderWidgets& widgets, const UsageModel& model,
    Provider provider, bool connected, uint64_t now_ms)
{
    const auto state = model.display_state(provider, connected);
    const auto& snapshot = model.snapshot(provider);
    if (widgets.status != nullptr) {
        if (state == DisplayState::Online) {
            const uint32_t epoch = model.estimated_epoch(provider, now_ms);
            const uint32_t age = epoch > snapshot.sampled_at
                ? epoch - snapshot.sampled_at : 0;
            lv_label_set_text_fmt(widgets.status, "LAST %luS AGO",
                static_cast<unsigned long>(age));
        } else {
            lv_label_set_text(widgets.status, display_state_name(state));
        }
        lv_obj_set_style_text_color(widgets.status, state_color(state), 0);
    }

    const uint32_t epoch =
        snapshot.has_valid_data ? model.estimated_epoch(provider, now_ms) : 0;
    const bool fresh = state == DisplayState::Online || state == DisplayState::Partial;
    const bool partial = snapshot.source_state == SourceState::Partial;
    update_metric(
        widgets.short_window, snapshot.short_window,
        snapshot.has_valid_data && (!partial || snapshot.latest_short_present), epoch, fresh);
    update_metric(
        widgets.week_window, snapshot.week_window,
        snapshot.has_valid_data && (!partial || snapshot.latest_week_present), epoch, fresh);
}

bool DisplayUi::set_transition_brightness(int32_t percent)
{
    const uint8_t level = static_cast<uint8_t>(percent * 255 / 100);
    // CO5300 QSPI encodes brightness register 0x51 with opcode 0x02
    // and an 8-bit address shift.
    constexpr int brightness_command = (0x02 << 24) | (0x51 << 8);
    const esp_err_t error = esp_lcd_panel_io_tx_param(
        panel_io_, brightness_command, &level, sizeof(level));
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "transition brightness failed: %s", esp_err_to_name(error));
        return false;
    }
    return true;
}

void DisplayUi::transition_brightness(void* target, int32_t percent)
{
    static_cast<DisplayUi*>(target)->set_transition_brightness(percent);
}

void DisplayUi::transition_refresh_ready(lv_event_t* event)
{
    auto* ui = static_cast<DisplayUi*>(lv_event_get_user_data(event));
    if (!ui->transition_refresh_pending_) return;
    ui->transition_refresh_pending_ = false;

    // REFR_READY can precede the final DMA completion. lv_anim_start() applies
    // zero brightness synchronously; the SPI parameter command waits for queued
    // pixel transfers to finish before the fade can illuminate the new frame.
    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, ui);
    lv_anim_set_exec_cb(&fade, transition_brightness);
    lv_anim_set_values(&fade, 0, ui->brightness_percent_);
    lv_anim_set_duration(&fade, 120);
    lv_anim_set_path_cb(&fade, lv_anim_path_ease_out);
    if (lv_anim_start(&fade) == nullptr) {
        ui->set_transition_brightness(ui->brightness_percent_);
    }
}

void DisplayUi::render(
    const UsageModel& model, Page, bool connected,
    bool has_passkey, uint32_t passkey, uint64_t now_ms,
    const OtaSnapshot& ota, uint32_t confirm_seconds,
    bool show_ota_error, const PanelPresentation& presentation,
    bool input_enabled)
{
    DisplayLock lock;
    if (!lock) {
        ESP_LOGE(TAG, "failed to acquire display lock for render");
        return;
    }

    const bool ota_visible = ota.phase != OtaPhase::Idle || show_ota_error;
    const bool screensaver_visible =
        presentation.screensaver.active && !has_passkey && !ota_visible;
    const bool show_hud = !ota_visible && !screensaver_visible && !has_passkey &&
        !settings_visible_.load(std::memory_order_relaxed);
    lv_obj_add_flag(codex_page_.panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(guofeng_page_.panel, LV_OBJ_FLAG_HIDDEN);
    if (show_hud) {
        lv_obj_remove_flag(theme_ == DisplayTheme::Guofeng ? guofeng_page_.panel : codex_page_.panel,
                           LV_OBJ_FLAG_HIDDEN);
    }
    const bool was_screensaver_visible =
        !lv_obj_has_flag(screensaver_panel_, LV_OBJ_FLAG_HIDDEN);
    set_hud_motion(show_hud, show_hud && was_screensaver_visible);
    if (screensaver_visible != was_screensaver_visible) {
        // Cancel an active fade before concealing the next screensaver repaint.
        lv_anim_delete(this, transition_brightness);
        transition_refresh_pending_ = set_transition_brightness(0);
    }
    input_enabled_.store(input_enabled && !has_passkey, std::memory_order_relaxed);
    ota_active_.store(ota.phase != OtaPhase::Idle, std::memory_order_relaxed);
    ota_confirming_.store(
        ota.phase == OtaPhase::Confirming, std::memory_order_relaxed);
    if (ota_visible) {
        lv_obj_add_flag(screensaver_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(ota_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ota_panel_);

        lv_obj_add_flag(ota_card_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ota_card_countdown_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ota_card_note_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ota_progress_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ota_card_hint_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ota_confirm_button_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ota_deny_button_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ota_title_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ota_detail_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ota_fail_row_, LV_OBJ_FLAG_HIDDEN);

        if (show_ota_error) {
            const OtaErrorPresentation message =
                ota_error_presentation(ota.error);
            lv_obj_remove_flag(ota_fail_row_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(ota_detail_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(ota_detail_, OTA_FAIL_DETAIL_Y);
            lv_label_set_text(ota_fail_title_, message.title);
            lv_label_set_text(ota_detail_, message.detail);
        } else if (ota.phase == OtaPhase::Confirming) {
            lv_obj_remove_flag(ota_card_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(ota_card_countdown_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(ota_card_note_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(ota_confirm_button_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(ota_deny_button_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ota_card_label_, "NEW VERSION AVAILABLE");
            lv_label_set_text_fmt(
                ota_card_countdown_, "%luS",
                static_cast<unsigned long>(confirm_seconds));
            lv_label_set_text(ota_card_value_, ota.version.c_str());
        } else if (ota.phase == OtaPhase::Receiving) {
            lv_obj_remove_flag(ota_card_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(ota_progress_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(ota_card_hint_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ota_card_label_, "UPDATING FIRMWARE");
            const uint8_t percent = ota_progress_percent(ota.offset, ota.size);
            lv_label_set_text_fmt(ota_card_value_, "%u%%", percent);
            lv_bar_set_value(ota_progress_, percent, LV_ANIM_OFF);
        } else {
            lv_obj_remove_flag(ota_title_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(ota_detail_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(ota_detail_, OTA_WAIT_DETAIL_Y);
            lv_label_set_text(
                ota_title_, ota.phase == OtaPhase::Verifying
                    ? "VERIFYING" : "REBOOTING");
            lv_label_set_text(ota_detail_, "PLEASE WAIT");
        }
        return;
    }
    lv_obj_add_flag(ota_panel_, LV_OBJ_FLAG_HIDDEN);
    const bool settings_visible = settings_visible_.load(std::memory_order_relaxed) && !has_passkey;
    lv_label_set_text(clock_label_, settings_visible ? "DISPLAY" : presentation.clock.time);
    lv_obj_set_style_text_color(
        clock_label_,
        lv_color_hex(settings_visible || presentation.clock.available ? COLOR_TEXT : COLOR_DIM), 0);

    const bool battery_available = presentation.battery.available;
    const int visible_segments = battery_available
        ? static_cast<int>(presentation.battery.band) : 0;
    const uint32_t battery_color =
        presentation.battery.low_on_battery ? COLOR_WARN : COLOR_RESET;
    lv_obj_set_style_opa(
        charge_symbol_, presentation.battery.charging
            ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    for (int i = 0; i < 4; ++i) {
        lv_obj_set_style_bg_color(
            battery_segments_[i],
            lv_color_hex(i < visible_segments
                ? battery_color : COLOR_BATTERY_OFF), 0);
        lv_obj_set_style_bg_opa(battery_segments_[i], LV_OPA_COVER, 0);
    }

    if (settings_visible) {
        lv_label_set_text(connection_label_, connected ? "LINKED" : "WAITING");
        lv_obj_set_style_bg_color(status_dot_, lv_color_hex(connected ? COLOR_GOOD : COLOR_MUTED), 0);
        lv_obj_add_flag(screensaver_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pairing_panel_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (screensaver_visible) {
        apply_clock_theme();
        lv_label_set_text(screensaver_time_, presentation.clock.time);
        if (theme_ == DisplayTheme::Guofeng) {
            // UTF-8 weekday names keep the source portable across Windows code pages.
            constexpr const char* weekdays[] = {
                "\xe6\x98\x9f\xe6\x9c\x9f\xe6\x97\xa5",
                "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x80",
                "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x8c",
                "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x89",
                "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x9b\x9b",
                "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x94",
                "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x85\xad"};
            if (presentation.clock.available) {
                lv_label_set_text_fmt(screensaver_month_day_,
                    "%04u\xe5\xb9\xb4%02u\xe6\x9c\x88%02u\xe6\x97\xa5 %s",
                    static_cast<unsigned>(presentation.clock.year),
                    static_cast<unsigned>(presentation.clock.month),
                    static_cast<unsigned>(presentation.clock.day),
                    weekdays[presentation.clock.weekday_index]);
            } else {
                lv_label_set_text(screensaver_month_day_, "\xe6\x97\xb6\xe9\x97\xb4\xe6\x9c\xaa\xe5\x90\x8c\xe6\xad\xa5");
            }
        } else {
            lv_label_set_text(screensaver_weekday_, presentation.clock.available
                ? presentation.clock.weekday : "---");
            lv_label_set_text(screensaver_month_day_, presentation.clock.available
                ? presentation.clock.month_day : "-- --");
        }
        lv_obj_set_style_text_color(
            screensaver_time_,
            lv_color_hex(presentation.clock.available
                ? (theme_ == DisplayTheme::Guofeng ? 0x353E36 : 0xD89D7F)
                : (theme_ == DisplayTheme::Guofeng ? 0x929487 : COLOR_DIM)),
            0);
        lv_obj_update_layout(screensaver_time_);
        const int time_width = lv_obj_get_width(screensaver_time_);
        if (time_width > 0) {
            lv_obj_set_style_transform_scale_x(screensaver_time_,
                (theme_ == DisplayTheme::Guofeng ? 300 : 352) * 256 / time_width, 0);
        }
        ScreensaverUsageView usage{};
        usage.rows[0] = make_screensaver_usage_row(model, Provider::Codex, connected);
        usage.offline = !usage.rows[0].has_data;
        update_screensaver_usage(usage);
        lv_label_set_text(
            screensaver_hint_,
            usage.offline ? (connected ? "DATA UNAVAILABLE" : "OFFLINE")
                          : "TAP OR SHAKE TO WAKE");
        lv_obj_set_style_opa(
            screensaver_hint_, usage.offline ? static_cast<lv_opa_t>(LV_OPA_COVER)
                : presentation.screensaver.hint_opacity, 0);
        lv_obj_set_style_translate_x(
            screensaver_content_, presentation.screensaver.offset_x, 0);
        lv_obj_set_style_translate_y(
            screensaver_content_, presentation.screensaver.offset_y, 0);
        if (presentation.battery.low_on_battery) {
            lv_obj_remove_flag(screensaver_low_group_, LV_OBJ_FLAG_HIDDEN);
            const int low_segments = static_cast<int>(presentation.battery.band);
            for (int i = 0; i < 4; ++i) {
                lv_obj_set_style_bg_color(
                    screensaver_low_segments_[i],
                    lv_color_hex(i < low_segments
                        ? COLOR_LOW_DIM : COLOR_LINE), 0);
                lv_obj_set_style_bg_opa(
                    screensaver_low_segments_[i], LV_OPA_COVER, 0);
            }
        } else {
            lv_obj_add_flag(screensaver_low_group_, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_add_flag(pairing_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(screensaver_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(screensaver_panel_);
        return;
    }
    lv_obj_add_flag(screensaver_panel_, LV_OBJ_FLAG_HIDDEN);
    update_provider(theme_ == DisplayTheme::Guofeng ? guofeng_page_ : codex_page_,
                    model, Provider::Codex, connected, now_ms);

    if (has_passkey) {
        lv_label_set_text(connection_label_, "PAIRING");
        lv_label_set_text_fmt(
            pairing_code_, "%06lu", static_cast<unsigned long>(passkey));
        lv_obj_remove_flag(pairing_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(pairing_panel_);
    } else {
        lv_label_set_text(connection_label_, connected ? "LINKED" : "WAITING");
        lv_obj_add_flag(pairing_panel_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_bg_color(
        status_dot_,
        lv_color_hex(
            has_passkey ? COLOR_WARN : connected ? COLOR_GOOD : COLOR_MUTED),
        0);
}

void DisplayUi::touch_down_event(lv_event_t* event)
{
    auto* ui = static_cast<DisplayUi*>(lv_event_get_user_data(event));
    ui->touch_down_.store(true, std::memory_order_relaxed);
}

void DisplayUi::ota_confirm_event(lv_event_t* event)
{
    auto* ui = static_cast<DisplayUi*>(lv_event_get_user_data(event));
    if (ui->ota_confirming_.load(std::memory_order_relaxed)) {
        ui->ota_confirmation_requested_.store(true, std::memory_order_relaxed);
    }
}

void DisplayUi::ota_deny_event(lv_event_t* event)
{
    auto* ui = static_cast<DisplayUi*>(lv_event_get_user_data(event));
    if (ui->ota_confirming_.load(std::memory_order_relaxed)) {
        ui->ota_denial_requested_.store(true, std::memory_order_relaxed);
    }
}

}  // namespace usage_panel::amoled
