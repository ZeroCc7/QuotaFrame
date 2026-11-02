"""Bake existing LVGL glyphs into native-size A8 masks; requires Pillow."""

from pathlib import Path
import re

from PIL import Image


ROOT = Path(__file__).resolve().parent


def load_font(path):
    text = path.read_text(encoding="utf-8")
    assert ".bpp = 4" in text and ".bitmap_format = 0" in text
    data = bytes(int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]+)",
        text.split("glyph_bitmap[] = {", 1)[1].split("};", 1)[0]))
    glyphs = [dict((k, int(v)) for k, v in re.findall(r"\.(\w+)\s*=\s*(-?\d+)", line))
        for line in text.split("glyph_dsc[] = {", 1)[1].split("};", 1)[0].splitlines()
        if ".bitmap_index" in line]
    offsets = [int(v) for v in re.findall(r"\d+",
        text.split("glyph_id_ofs_list_0[] = {", 1)[1].split("};", 1)[0])]
    start = int(re.search(r"\.range_start = (\d+)", text)[1])
    first = int(re.search(r"\.glyph_id_start = (\d+)", text)[1])
    height = int(re.search(r"\.line_height = (\d+)", text)[1])
    baseline = int(re.search(r"\.base_line = (\d+)", text)[1])
    result = []
    for char in "0123456789.":
        glyph = glyphs[first + offsets[ord(char) - start]]
        w, h = glyph["box_w"], glyph["box_h"]
        packed = data[glyph["bitmap_index"]:glyph["bitmap_index"] + (w*h+1)//2]
        pixels = bytes(((packed[i//2] >> (4 if i % 2 == 0 else 0)) & 15) * 17
                       for i in range(w*h))
        advance = (glyph["adv_w"] + 8) // 16
        canvas = Image.new("L", (advance, height))
        canvas.paste(Image.frombytes("L", (w, h), pixels),
                     (glyph["ofs_x"], height-baseline-glyph["ofs_y"]-h))
        result.append(canvas)
    return result


def main():
    output = bytearray()
    lines = ['#include "lvgl.h"',
             'extern const uint8_t masks[] asm("_binary_digits_a8_start");']
    for name, source, height, widths in (
        ("eva", ROOT.parent / "hud_digits.c", 90, ((64, 15), (48, 15))),
        ("guofeng", ROOT.parent / "guofeng/guofeng_digits.c", 69, ((53, 13), (40, 12))),
    ):
        glyphs = load_font(source)
        lines.append(f"const lv_image_dsc_t {name}_roller_digits[2][11] = {{")
        for digit_width, dot_width in widths:
            lines.append("    {")
            for index, image in enumerate(glyphs):
                width = dot_width if index == 10 else digit_width
                pixels = image.resize((width, height), Image.Resampling.LANCZOS).tobytes()
                lines.append("        {.header = {.magic = LV_IMAGE_HEADER_MAGIC, "
                    f".cf = LV_COLOR_FORMAT_A8, .w = {width}, .h = {height}, .stride = {width}}}, "
                    f".data_size = {len(pixels)}, .data = masks + {len(output)}}},")
                output.extend(pixels)
            lines.append("    },")
        lines.append("};")
    (ROOT / "digits.a8").write_bytes(output)
    (ROOT / "resources.c").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Baked 44 glyph masks: {len(output)} bytes")


if __name__ == "__main__":
    main()
