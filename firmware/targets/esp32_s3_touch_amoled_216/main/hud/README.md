# EVA HUD assets

Adapted from the local EVA desktop HUD v4 renderer, not the unmasked design plate.
`reference.svg` preserves its 1254 x 1150 viewBox, module clipping, and static text.
`plate.png` is the exact image embedded in that renderer. The artwork is supplied
for personal use; redistribution rights have not been established.
Oxanium is instantiated at weight 800; its SIL OFL license is in OFL.txt.

The 480 x 480 RGB565 background uses layout.svg and the original source artwork.
The ring is enlarged around (240, 200), with the reset panel in its lower opening.
The side badge is retained at the right edge; the EVA title is omitted.
The main HUD covers the status header. Settings, pairing and OTA restore the
existing header and interaction layout. reference.svg retains the desktop layout.
Remaining quota interpolates continuously through warm amber (100%, #FFBC83),
orange (60%, #FF965F), coral (20%, #FF674F), and warning red (0%, #FF424C).
Short and weekly windows are evaluated independently; offline is gray.
The ring, numeric values, and reset dates remain live LVGL objects.
The firmware protocol carries integer percentages; the trailing .0 is formatting.
Reset dates use UTC+8, matching the desktop HUD.

Regenerate the background with `node render.cjs` after installing
`@resvg/resvg-js` in a separate tool environment. Generate fonts with
lv_font_conv with `--lv-include lvgl.h --no-compress`, 4 bpp: size 86 with symbols `0123456789.-`, and size 26
with symbols `0123456789.W%:/- `. `render.cjs` also writes the little-endian RGB565 background.rgb565. Generated files are committed; no Node
tools are needed for firmware builds.
