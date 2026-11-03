"""Convert the supplied Math Boom portraits and keypad into LVGL assets."""

from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT.parents[4] / "assets" / "math_boom"


def main():
    portraits = SOURCE / "头像"
    sources = {
        "father": portraits / "爸爸程序员科技徽章.png",
        "brother": portraits / "足球小将庆祝头像.png",
        "mother": next(portraits.glob("*09_38_28-1.png")),
        "sister": next(portraits.glob("*09_38_29-2.png")),
    }
    for name, path in sources.items():
        image = Image.open(path).convert("RGBA").resize((112, 112), Image.Resampling.LANCZOS)
        if name in ("father", "brother"):
            mask = Image.new("L", (112, 112), 0)
            ImageDraw.Draw(mask).ellipse((0, 0, 111, 111), fill=255)
            image.putalpha(mask)
        image.save(ROOT / f"{name}.png")

    sheet = Image.open(next((SOURCE / "UI").glob("*09_37_01-3.png"))).convert("RGBA")
    data = bytearray()
    boxes = [(310 + col * 283, 240 + row * 214, 570 + col * 283, 448 + row * 214)
             for row in range(3) for col in range(3)]
    boxes += [(482, 885, 715, 1075), (732, 885, 966, 1075), (981, 885, 1218, 1075)]
    for box in boxes:
        image = Image.new("RGBA", (100, 80))
        crop = sheet.crop(box)
        crop.thumbnail((100, 80), Image.Resampling.LANCZOS)
        image.alpha_composite(crop, ((100 - crop.width) // 2, (80 - crop.height) // 2))
        indexed = image.quantize(colors=16, method=Image.Quantize.FASTOCTREE)
        palette = indexed.getpalette("RGBA")
        for i in range(16):
            r, g, b, a = palette[4*i:4*i+4]
            data.extend((b, g, r, a))
        pixels = indexed.tobytes()
        data.extend((pixels[i] << 4) | pixels[i+1] for i in range(0, len(pixels), 2))
    (ROOT / "math_keys.i4").write_bytes(data)

    sheets = sorted((SOURCE / "UI").glob("*.png"))
    # Crop coordinates are measured on the supplied 1448 x 1086 sheets.
    pieces = {
        "button_green": (1, (25, 887, 486, 1058), (226, 70)),
        "button_blue": (1, (492, 887, 954, 1058), (226, 70)),
        "button_red": (1, (961, 887, 1424, 1058), (226, 70)),
        "title": (5, (80, 0, 650, 352), (120, 58)),
        "safe": (5, (102, 368, 640, 554), (260, 90)),
        "boom": (5, (684, 314, 1370, 564), (320, 116)),
        "failed": (5, (704, 707, 1388, 870), (300, 72)),
        "range": (5, (68, 560, 514, 704), (164, 52)),
        "nav_blue": (1, (492, 887, 954, 1058), (100, 48)),
        "nav_red": (1, (961, 887, 1424, 1058), (100, 48)),
        "ready_panel": (4, (12, 188, 358, 604), (248, 300)),
        "check": (3, (33, 320, 256, 540), (32, 32)),
        "bomb_on": (3, (760, 52, 996, 300), (32, 32)),
        "bomb_off": (3, (1200, 52, 1448, 300), (32, 32)),
    }
    for role in range(6):
        pieces[f"name_{role}"] = (0, (role * 240, 836, (role + 1) * 240, 984), (100, 36))
    packed = bytearray()
    declarations = ['#pragma once', '#include "lvgl.h"', 'extern "C" {']
    definitions = ['#include "lvgl.h"',
                   'extern const uint8_t math_ui_start[] asm("_binary_math_ui_i4_start");']
    for name, (sheet_index, box, size) in pieces.items():
        crop = Image.open(sheets[sheet_index]).convert("RGBA").crop(box)
        crop.thumbnail(size, Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", size)
        canvas.alpha_composite(crop, ((size[0]-crop.width)//2, (size[1]-crop.height)//2))
        colors = 16 if name.startswith("button_") or name == "ready_panel" else 256
        indexed = canvas.quantize(colors=colors, method=Image.Quantize.FASTOCTREE)
        palette = indexed.getpalette("RGBA")
        offset = len(packed)
        for i in range(colors):
            r, g, b, a = palette[4*i:4*i+4]
            packed.extend((b, g, r, a))
        pixels = indexed.tobytes()
        if colors == 16:
            packed.extend((pixels[i] << 4) | pixels[i+1] for i in range(0, len(pixels), 2))
        else:
            packed.extend(pixels)
        declarations.append(f'extern const lv_image_dsc_t math_ui_{name};')
        color_format = "I4" if colors == 16 else "I8"
        stride = size[0] // 2 if colors == 16 else size[0]
        definitions.append(f'const lv_image_dsc_t math_ui_{name} = {{.header = {{.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_{color_format}, .w = {size[0]}, .h = {size[1]}, .stride = {stride}}}, .data_size = {len(packed)-offset}, .data = math_ui_start + {offset}}};')
    for role, name in enumerate(("father", "mother", "brother", "sister", "grandfather", "grandmother")):
        for size in (80, 36):
            portrait = Image.open(ROOT / f"{name}.png").convert("RGBA")
            portrait.thumbnail((size - 4, size - 4), Image.Resampling.LANCZOS)
            canvas = Image.new("RGBA", (size, size))
            canvas.alpha_composite(portrait, ((size - portrait.width)//2, (size - portrait.height)//2))
            colors = 256 if size == 80 else 16
            indexed = canvas.quantize(colors=colors, method=Image.Quantize.FASTOCTREE)
            palette = indexed.getpalette("RGBA")
            offset = len(packed)
            for i in range(colors):
                r, g, b, a = palette[4*i:4*i+4]
                packed.extend((b, g, r, a))
            pixels = indexed.tobytes()
            if colors == 16:
                packed.extend((pixels[i] << 4) | pixels[i+1] for i in range(0, len(pixels), 2))
            else:
                packed.extend(pixels)
            symbol = f"math_avatar_{role}_{size}"
            declarations.append(f'extern const lv_image_dsc_t {symbol};')
            cf, stride = ("I8", size) if colors == 256 else ("I4", size//2)
            definitions.append(f'const lv_image_dsc_t {symbol} = {{.header = {{.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_{cf}, .w = {size}, .h = {size}, .stride = {stride}}}, .data_size = {len(packed)-offset}, .data = math_ui_start + {offset}}};')
    (ROOT / "math_ui.i4").write_bytes(packed)
    (ROOT / "math_ui.h").write_text('\n'.join(declarations + ['}']) + '\n')
    (ROOT / "math_ui.c").write_text('\n'.join(definitions) + '\n')


if __name__ == "__main__":
    main()
