# Guofeng theme

source.png was edited from the user reference with the built-in image generator.
The exact prompt is saved in prompt.txt.
The sample number, filled arc and dates were removed; live LVGL objects supply them.
background.png is a 480 x 480 Lanczos resize, and guofeng.rgb565 is little-endian RGB565.
Caveat-Bold.ttf is instantiated from Google Fonts Caveat at weight 700.
Font source: https://github.com/google/fonts/tree/main/ofl/caveat (SIL OFL, see OFL.txt).
Generate guofeng_digits.c with lv_font_conv, size 108, 4 bpp, symbols 0123456789.-,
--lv-include lvgl.h --no-compress.
