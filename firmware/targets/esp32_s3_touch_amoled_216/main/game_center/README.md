# Game center artwork

The user-provided family party game design sheet and generated lobby mockup
guided these five independent layers: background, title, number bomb card,
penalty card, and return hint. The generated image assets are stored as PNGs
beside their firmware-ready pixel data. `convert.py` regenerates indexed I8
background, decorations and portraits from those PNGs.

LVGL places and handles the cards independently in `display_ui.cpp`. The
full-screen generated concept remains in `docs/design/` as a visual reference;
it is not embedded in the firmware.

The six role portraits are separate 112 x 112 indexed I8 layers. The four
supplied Math Boom portraits replace father, mother, brother and sister;
grandfather and grandmother retain the earlier portraits. `convert_math_boom.py`
prepares those portraits and the 12 keypad images from `assets/math_boom`.
The keypad uses a single 4-bit indexed asset with a palette per key.
Key images fit inside 100 x 80 bounds without changing their aspect ratio.
The same converter extracts the supplied button skins, title, SAFE/BOOM,
failure, range, preparation panel, role-name tags and selection check artwork.
`math_ui.c` and `math_ui.h` describe offsets in `math_ui.i4`; despite the blob
extension, each descriptor specifies its own I4 or I8 format. Small decorative
button skins and the preparation panel use I4; text-bearing artwork uses I8.
The game reuses the generated lobby landscape under native LVGL cards, score text,
range text, and buttons. The static lobby decorations and 280 x 280 SAFE and
BOOM effects use indexed I8 palettes to fit the OTA application partition.
`game_zh_font.c` is a 24 px subset of Noto Sans CJK SC Bold;
`game_number_font.c` contains the 48 px bold numeric glyphs. Both use
4 bpp uncompressed LVGL bitmaps generated with `lv_font_conv`; the exact
symbol lists and generation options are recorded in their headers. Their
SIL OFL license is in `NotoSansCJK-LICENSE.txt`. Role names use the supplied
artwork instead of font glyphs. Avatar widgets use explicit square dimensions
and pre-sized 80 px / 36 px bitmaps with transparent margins for selection
and order rows. These images draw at native resolution without runtime scaling.
Order rows use the same native 80 px portraits as round-result cards. With
two to six players, the order reads left to right across two columns. Both
order and round-result cards retain the same 212 x 88 size; an odd last card
is centered and the group is centered vertically in the available area. Round
results show only accumulated strikes as supplied bomb icons. Players with
no strikes have an empty score area.
