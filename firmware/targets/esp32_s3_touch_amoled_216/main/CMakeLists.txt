idf_component_register(
    SRCS
        "app_main.cpp"
        "display_ui.cpp"
        "number_bomb.cpp"
        "game_audio.cpp"
        "hud/resources.c"
        "game_center/game_zh_font.c"
        "hud/rollers/resources.c"
        "hud/hud_digits.c"
        "hud/hud_small.c"
        "hud/guofeng/guofeng_digits.c"
        "hud/clocks/eva_clock_font.c"
        "hud/clocks/guofeng_clock_font.c"
        "hud/clocks/guofeng_date_font.c"
        "memory_telemetry.cpp"
        "performance_probe.cpp"
        "orientation.cpp"
        "power_monitor.cpp"
    EMBED_FILES "hud/background.rgb565" "hud/guofeng/guofeng.rgb565"
        "game_center/background.i8"
        "game_center/title.i8"
        "game_center/number_bomb.i8"
        "game_center/penalty.i8"
        "game_center/return_hint.i8"
        "game_center/father.argb8888"
        "game_center/mother.argb8888"
        "game_center/brother.argb8888"
        "game_center/sister.argb8888"
        "game_center/grandfather.argb8888"
        "game_center/grandmother.argb8888"
        "game_center/safe_burst.i8"
        "game_center/boom_burst.i8"
        "hud/rollers/digits.a8"
        "hud/clocks/eva_clock.rgb565" "hud/clocks/guofeng_clock.rgb565"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_hw_support
        esp_driver_i2c
        esp_driver_gpio
        heap
        esp_lvgl_adapter
        usage_core
        usage_panel_state
        usage_fonts
        usage_ble
        usage_ota
        usage_rtc
        cube32esp__xpowerslib
        espressif__esp_codec_dev
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_sources(${lvgl_lib} PRIVATE "lvgl_psram_pool.c")
target_include_directories(${lvgl_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_compile_definitions(${lvgl_lib} PRIVATE
    LV_MEM_POOL_INCLUDE=\"lvgl_psram_pool.h\"
    LV_MEM_POOL_ALLOC=lvgl_psram_pool_alloc
)
