"""Convert the committed game-center PNG layers to LVGL pixel data."""

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parent
SIZES = {
    "background": (480, 480),
    "title": (400, 131),
    "number_bomb": (211, 241),
    "penalty": (211, 241),
    "return_hint": (250, 69),
    "father": (112, 112),
    "mother": (112, 112),
    "brother": (112, 112),
    "sister": (112, 112),
    "grandfather": (112, 112),
    "grandmother": (112, 112),
    "safe_burst": (280, 280),
    "boom_burst": (280, 280),
}
INDEXED = {"background", "title", "number_bomb", "penalty", "return_hint", "safe_burst", "boom_burst"}


def convert() -> None:
    for name, size in SIZES.items():
        image = Image.open(ROOT / f"{name}.png").convert("RGBA")
        if image.size != size:
            raise ValueError(f"{name}.png must be {size[0]}x{size[1]}")

        if name in INDEXED:
            indexed = image.quantize(colors=256, method=Image.Quantize.FASTOCTREE)
            palette = indexed.getpalette("RGBA")
            bgra = bytearray(1024)
            for index in range(256):
                red, green, blue, alpha = palette[index * 4:index * 4 + 4]
                bgra[index * 4:index * 4 + 4] = bytes((blue, green, red, alpha))
            (ROOT / f"{name}.i8").write_bytes(bgra + indexed.tobytes())
        else:
            rgba = image.tobytes()
            bgra = bytearray(len(rgba))
            bgra[0::4] = rgba[2::4]
            bgra[1::4] = rgba[1::4]
            bgra[2::4] = rgba[0::4]
            bgra[3::4] = rgba[3::4]
            (ROOT / f"{name}.argb8888").write_bytes(bgra)


if __name__ == "__main__":
    convert()
