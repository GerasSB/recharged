#!/usr/bin/env python3
"""Make the launcher's logo and header images in assets/launcher/ from their originals.

    launcher_art.py logo ORIGINAL.png    # logo.png: the Recharged logo, trimmed, 1280 pixels wide
    launcher_art.py header MSC_Scene.jpg # header.png: Bowser from the game's key art, 1920 x 960

The header is the upper part of the key art (3500 x 4700): Bowser and his claws
in the storm over the stadium, mirrored so that he roars beside the launcher's
logo instead of behind it, scaled and given a little more light and colour.
Needs Pillow. assets/launcher/README.md lists the originals.
"""

import sys
from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps

ASSETS = Path(__file__).resolve().parent.parent / "assets/launcher"
LOGO_WIDTH = 1280


def make_logo(source):
    image = Image.open(source).convert("RGBA")
    # A few nearly invisible specks lie further out than the logo.
    left, top, right, bottom = image.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
    pad = 8
    image = image.crop((max(left - pad, 0), max(top - pad, 0), min(right + pad, image.width),
                        min(bottom + pad, image.height)))
    height = round(image.height * LOGO_WIDTH / image.width)
    # Premultiplied resampling: the transparent surroundings add no colour to the edges.
    scaled = image.convert("RGBa").resize((LOGO_WIDTH, height), Image.LANCZOS).convert("RGBA")
    visible = scaled.getchannel("A").point(lambda a: 255 if a else 0)
    Image.composite(scaled, Image.new("RGBA", scaled.size), visible).save(ASSETS / "logo.png", optimize=True)
    return LOGO_WIDTH, height


def make_header(source):
    image = Image.open(source).convert("RGB")
    if image.size != (3500, 4700):
        raise SystemExit(f"{source}: expected the 3500 x 4700 key art, found {image.width} x {image.height}")
    part = ImageOps.mirror(image.crop((0, 640, 3500, 2390))).resize((1920, 960), Image.LANCZOS)
    part = ImageEnhance.Color(ImageEnhance.Brightness(part).enhance(1.08)).enhance(1.15)
    part.save(ASSETS / "header.png", optimize=True)
    return part.width, part.height


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("logo", "header"):
        print(__doc__.strip(), file=sys.stderr)
        return 2
    make = make_logo if sys.argv[1] == "logo" else make_header
    width, height = make(Path(sys.argv[2]))
    print(f"{ASSETS / (sys.argv[1] + '.png')}: {width} x {height}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
