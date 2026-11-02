# Game center artwork

The user-provided family party game design sheet and generated lobby mockup
guided these five independent layers: background, title, number bomb card,
penalty card, and return hint. The generated image assets are stored as PNGs
beside their firmware-ready pixel data. `convert.py` regenerates indexed I8
background and decorations plus ARGB8888 portraits from those PNGs.

LVGL places and handles the cards independently in `display_ui.cpp`. The
full-screen generated concept remains in `docs/design/` as a visual reference;
it is not embedded in the firmware.

The six generated role portraits are separate 112 x 112 ARGB8888 layers. The
game reuses the generated lobby landscape under native LVGL cards, score text,
range text, and buttons. The static lobby decorations and 280 x 280 SAFE and
BOOM effects use indexed I8 palettes to fit the OTA application partition.
`game_zh_font.c` is a 24 px subset of the SIL OFL
Noto Serif CJK SC font; its license is in `../hud/clocks/NotoSerifSC-LICENSE.txt`.
