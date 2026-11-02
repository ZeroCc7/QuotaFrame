#pragma once

#include <atomic>
#include <cstdint>

#include "esp_lcd_touch.h"
#include "esp_lcd_panel_io.h"
#include "lvgl.h"
#include "orientation.hpp"
#include "usage_panel_state/display_settings.hpp"
#include "usage_panel_state/page_state.hpp"
#include "usage_panel_state/panel_presentation.hpp"
#include "usage_panel_state/screensaver_usage.hpp"
#include "usage_core/usage_state.hpp"
#include "usage_ota/session.hpp"

namespace usage_panel::amoled {

/**
 * Owns the LVGL object tree and the narrow event boundary to the app loop.
 *
 * LVGL callbacks never mutate application/OTA state directly. They publish
 * page requests or one-shot atomic event flags, which app_main consumes and
 * applies on its own loop. The flags carry no associated payload, so relaxed
 * ordering is sufficient for these event latches.
 */
class DisplayUi {
public:
    /** Initializes display/touch/LVGL and builds the complete object tree. */
    bool begin(ScreenRotation initial_rotation);

    void request_page(Page) { requested_page_.store(Page::Codex, std::memory_order_relaxed); }
    bool apply_settings(const DisplaySettings& settings, bool visible, bool save_error = false, bool modified = false);
    SettingsAction take_settings_action()
    {
        return settings_action_.exchange(SettingsAction::None, std::memory_order_relaxed);
    }

    /** Returns the single Codex usage page. */
    Page requested_page() const
    {
        return requested_page_.load(std::memory_order_relaxed);
    }

    /**
     * Renders the current application snapshot and interaction state.
     *
     * Screensaver visibility changes temporarily darken the panel. Brightness
     * is restored asynchronously after the repaint is transferred.
     */
    void render(const UsageModel& model, Page page, bool connected,
                bool has_passkey, uint32_t passkey, uint64_t now_ms,
                const OtaSnapshot& ota, uint32_t confirm_seconds,
                bool show_ota_error, const PanelPresentation& presentation,
                bool input_enabled);

    /** Consumes one pending touch-down event. */
    bool take_touch_down()
    {
        return touch_down_.exchange(false, std::memory_order_relaxed);
    }

    /** Applies display and touch transforms together for a new rotation. */
    bool set_rotation(ScreenRotation rotation);
#if CONFIG_WS_USAGE_PANEL_MEMORY_PROBE
    void run_memory_probe();
#endif

    /** Consumes one pending on-device OTA confirmation request. */
    bool take_ota_confirmation()
    {
        return ota_confirmation_requested_.exchange(
            false, std::memory_order_relaxed);
    }

    /** Consumes one pending on-device OTA denial request. */
    bool take_ota_denial()
    {
        return ota_denial_requested_.exchange(
            false, std::memory_order_relaxed);
    }

    struct MetricWidgets {
        lv_obj_t* value = nullptr;
        lv_obj_t* reset = nullptr;
        lv_obj_t* unit = nullptr;
        lv_obj_t* caption = nullptr;
        bool primary = false;
        bool guofeng = false;
        lv_obj_t* ring = nullptr;
        lv_obj_t* segments[28]{};
        bool available = false;
        bool fresh = false;
        int32_t target = -1;
        int32_t displayed = 0;  // Tenths of one percent.
        int32_t animation_from = 0;
        uint32_t animation_started = 0;
        uint32_t animation_duration = 600;
        int32_t painted_value = -2;
        int painted_breath = -1;
        bool painted_fresh = false;
        lv_obj_t* rollers = nullptr;
        lv_obj_t* decimal = nullptr;
        lv_obj_t* prefix = nullptr;
        lv_obj_t* suffix = nullptr;
        lv_obj_t* digit_clips[4]{};
        lv_obj_t* digit_labels[4][2]{};
        int reel_digits[4]{-1, -1, -1, -1};
        int digit_height = 0;
        int digit_width = 0;
        int32_t painted_progress = -1;
    };

    struct ProviderWidgets {
        lv_obj_t* panel = nullptr;
        lv_obj_t* status = nullptr;
        MetricWidgets short_window;
        MetricWidgets week_window;
    };

private:
    struct ScreensaverUsageWidgets {
        // The track moves with the fill: the pair slides half a percentage
        // slot when a number appears, so the ink stays centred in both states.
        lv_obj_t* track = nullptr;
        lv_obj_t* fill = nullptr;
        lv_obj_t* percent = nullptr;
    };

    void build_settings(lv_obj_t* screen);
    static void settings_event(lv_event_t* event);
    static void long_press_event(lv_event_t* event);
    void build_header(lv_obj_t* screen);
    void build_provider_page(lv_obj_t* screen);
    void build_guofeng_page(lv_obj_t* screen);
    void build_screensaver(lv_obj_t* screen);
    void apply_clock_theme();
    void build_pairing_overlay(lv_obj_t* screen);
    void build_ota_overlay(lv_obj_t* screen);
    void update_provider(ProviderWidgets& widgets, const UsageModel& model,
                         Provider provider, bool connected, uint64_t now_ms);
    void update_metric(MetricWidgets& widgets, const UsageWindow& window,
                       bool has_data, uint32_t epoch, bool fresh);
    void update_screensaver_usage(const ScreensaverUsageView& usage);
    static void animate_hud(lv_timer_t* timer);
    static void paint_metric(MetricWidgets& widgets, uint32_t tick);
    static void build_metric_rollers(MetricWidgets& widgets);
    static void paint_metric_rollers(MetricWidgets& widgets, int32_t progress, uint32_t color);
    void set_hud_motion(bool visible, bool replay);

    bool set_transition_brightness(int32_t percent);
    static void transition_brightness(void* target, int32_t percent);
    static void transition_refresh_ready(lv_event_t* event);

    static void touch_down_event(lv_event_t* event);
    static void ota_confirm_event(lv_event_t* event);
    static void ota_deny_event(lv_event_t* event);
#if CONFIG_WS_USAGE_PANEL_MEMORY_PROBE
    static void probe_refresh_event(lv_event_t* event);
    void start_next_probe_refresh();
#endif

    struct SettingsBinding { DisplayUi* ui; SettingsAction action; };
    SettingsBinding settings_bindings_[11]{};
    std::atomic<SettingsAction> settings_action_{SettingsAction::None};
    std::atomic<bool> settings_visible_{false};
    lv_obj_t* settings_panel_ = nullptr;
    lv_obj_t* settings_brightness_ = nullptr;
    lv_obj_t* settings_buttons_[11]{};
    lv_obj_t* settings_timeout_ = nullptr;
    lv_obj_t* settings_hint_ = nullptr;
    uint8_t brightness_percent_ = 50;  // Serialized by the LVGL lock.
    std::atomic<Page> requested_page_{Page::Codex};
    std::atomic<bool> ota_active_{false};
    std::atomic<bool> ota_confirming_{false};
    std::atomic<bool> ota_confirmation_requested_{false};
    std::atomic<bool> ota_denial_requested_{false};
    std::atomic<bool> touch_down_{false};
    std::atomic<bool> input_enabled_{true};
    lv_display_t* display_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    // Access from render() and LVGL callbacks is serialized by the LVGL lock.
    bool transition_refresh_pending_ = false;
    lv_indev_t* input_device_ = nullptr;
    esp_lcd_touch_handle_t touch_ = nullptr;
    ScreenRotation rotation_ = ScreenRotation::Deg0;
    lv_obj_t* connection_label_ = nullptr;
    lv_obj_t* status_dot_ = nullptr;
    lv_obj_t* clock_label_ = nullptr;
    lv_obj_t* battery_group_ = nullptr;
    lv_obj_t* charge_symbol_ = nullptr;
    lv_obj_t* battery_segments_[4]{};
    ProviderWidgets codex_page_{};
    ProviderWidgets guofeng_page_{};
    DisplayTheme theme_ = DisplayTheme::Eva;
    lv_timer_t* hud_timer_ = nullptr;
    lv_obj_t* screensaver_panel_ = nullptr;
    lv_obj_t* screensaver_content_ = nullptr;
    lv_obj_t* screensaver_background_ = nullptr;
    lv_obj_t* screensaver_seal_ = nullptr;
    lv_obj_t* screensaver_date_row_ = nullptr;
    lv_obj_t* screensaver_date_dot_ = nullptr;
    DisplayTheme clock_theme_ = DisplayTheme::Eva;
    bool clock_theme_applied_ = false;
    lv_obj_t* screensaver_time_ = nullptr;
    lv_obj_t* screensaver_weekday_ = nullptr;
    lv_obj_t* screensaver_month_day_ = nullptr;
    lv_obj_t* screensaver_hint_ = nullptr;
    ScreensaverUsageWidgets screensaver_usage_[1]{};
    lv_obj_t* screensaver_low_group_ = nullptr;
    lv_obj_t* screensaver_low_segments_[4]{};
    lv_obj_t* pairing_panel_ = nullptr;
    lv_obj_t* pairing_code_ = nullptr;
    lv_obj_t* ota_panel_ = nullptr;
    lv_obj_t* ota_card_ = nullptr;
    lv_obj_t* ota_card_label_ = nullptr;
    lv_obj_t* ota_card_countdown_ = nullptr;
    lv_obj_t* ota_card_value_ = nullptr;
    lv_obj_t* ota_card_note_ = nullptr;
    lv_obj_t* ota_card_hint_ = nullptr;
    lv_obj_t* ota_title_ = nullptr;
    lv_obj_t* ota_detail_ = nullptr;
    lv_obj_t* ota_progress_ = nullptr;
    lv_obj_t* ota_confirm_button_ = nullptr;
    lv_obj_t* ota_deny_button_ = nullptr;
    lv_obj_t* ota_fail_row_ = nullptr;
    lv_obj_t* ota_fail_title_ = nullptr;
#if CONFIG_WS_USAGE_PANEL_MEMORY_PROBE
    uint32_t probe_refresh_sample_ = 0;
    int64_t probe_refresh_started_us_ = 0;
    bool probe_refresh_active_ = false;
#endif
};

}  // namespace usage_panel::amoled
