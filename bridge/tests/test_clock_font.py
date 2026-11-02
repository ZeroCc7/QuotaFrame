"""Check the firmware bitmap itself, rather than the desktop TTF preview."""

from pathlib import Path
import re
import unittest


class ClockFontTests(unittest.TestCase):
    def test_guofeng_colon_has_two_complete_dots(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "firmware/targets/esp32_s3_touch_amoled_216/main/hud/clocks/guofeng_clock_font.c").read_text(encoding="utf-8")
        self.assertIn(".bpp = 8", source)
        bitmap = bytes(int(value, 16) for value in re.findall(
            r"0x([0-9a-fA-F]+)", source.split("glyph_bitmap[] = {", 1)[1].split("};", 1)[0]))
        descriptors = [dict((key, int(value)) for key, value in re.findall(
            r"\.(\w+)\s*=\s*(-?\d+)", line)) for line in
            source.split("glyph_dsc[] = {", 1)[1].split("};", 1)[0].splitlines()
            if ".bitmap_index" in line]
        offsets = [int(value) for value in re.findall(r"\d+",
            source.split("glyph_id_ofs_list_0[] = {", 1)[1].split("};", 1)[0])]
        start = int(re.search(r"\.range_start = (\d+)", source)[1])
        glyph_start = int(re.search(r"\.glyph_id_start = (\d+)", source)[1])
        glyph = descriptors[glyph_start + offsets[ord(":") - start]]
        width, height = glyph["box_w"], glyph["box_h"]
        self.assertGreater(width, 0)
        self.assertGreater(height, 0)
        data = bitmap[glyph["bitmap_index"]:glyph["bitmap_index"] + width * height]
        self.assertEqual(len(data), width * height)
        pixels = {(i % width, i // width) for i, opacity in enumerate(data) if opacity > 127}
        components = []
        while pixels:
            pending = [pixels.pop()]
            component = []
            while pending:
                x, y = pending.pop()
                component.append((x, y))
                for neighbor in ((x-1, y), (x+1, y), (x, y-1), (x, y+1)):
                    if neighbor in pixels:
                        pixels.remove(neighbor)
                        pending.append(neighbor)
            components.append(component)
        self.assertEqual(len(components), 2)
        top, bottom = sorted(components, key=lambda part: min(y for _, y in part))
        self.assertLess(max(y for _, y in top), min(y for _, y in bottom))
