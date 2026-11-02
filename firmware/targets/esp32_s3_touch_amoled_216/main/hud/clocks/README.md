# Clock themes

The clock follows DisplaySettings.theme; there is no second theme preference.
EVA uses the code-native eva-clock.svg and the existing Oxanium font.
Guofeng uses a solid warm paper background (#E5DDCD), an ink-colored serif
clock and a small cinnabar square. The previous generated landscape files are
retained as source artwork but hidden on this clock. All time/date values are live.

Both backgrounds are 480 x 480 RGB565. EVA artwork is drawn at opacity 210/255,
Guofeng hides its image and uses native LVGL objects instead. The entire content group,
including artwork, retains the existing small burn-in mitigation translations.
Wake inputs, clock availability, quota underline and low-battery indicators
continue to use the existing presentation state.

Run render.cjs with @resvg/resvg-js to regenerate the EVA image and RGB565 asset.
Guofeng is resized with Lanczos from the source image and encoded little-endian RGB565.
Fonts use the existing SIL-OFL Oxanium and Noto Serif SC assets. Generate with lv_font_conv:
- eva_clock_font.c: Oxanium-ExtraBold.ttf, 100 px, 4 bpp, symbols 0123456789:-.
- guofeng_clock_font.c: NotoSerifSC-Clock.ttf (weight 600), 100 px, 8 bpp, symbols 0123456789:-.
Both use --lv-include lvgl.h --no-compress. Fonts and binary resources are committed,
so the firmware build does not need the asset tooling.

Guofeng dates use a 20 px Noto Serif SC subset (weight 600), including Chinese
year/month/day units and weekdays. The subset retains its font name/license
metadata; see NotoSerifSC-LICENSE.txt. Regenerate guofeng_date_font.c with
NotoSerifSC-Date.ttf and symbols 0123456789- 年月日星期一二三四五六时间未同步.
Dates use validated RTC calendar fields; EVA keeps its original English layout.
