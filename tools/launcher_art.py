#!/usr/bin/env python3
"""Make the launcher's logo and header images in assets/launcher/ from their originals.

    launcher_art.py logo ORIGINAL.png    # logo.png: the Recharged logo, trimmed, 1280 pixels wide
    launcher_art.py header ORIGINAL.png  # header.png: SteamGridDB hero 8926, recoloured

The header keeps the artwork and changes only colours: the fire and the gold of
the ball become the logo's electric blue (hot yellow to icy cyan, orange to sky
blue, red to deep blue), the grass turns blue too, Mario's reds and skin stay.
Needs numpy and Pillow. assets/launcher/README.md lists the originals.
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image

ASSETS = Path(__file__).resolve().parent.parent / "assets/launcher"
LOGO_WIDTH = 1280


def make_logo(source):
    image = Image.open(source).convert("RGBA")
    alpha = np.asarray(image)[..., 3]
    ys, xs = np.nonzero(alpha > 8)  # a few nearly invisible specks lie further out
    pad = 8
    image = image.crop((max(xs.min() - pad, 0), max(ys.min() - pad, 0),
                        min(xs.max() + 1 + pad, image.width), min(ys.max() + 1 + pad, image.height)))
    height = round(image.height * LOGO_WIDTH / image.width)
    # Premultiplied resampling: the transparent surroundings add no colour to the edges.
    scaled = np.asarray(image.convert("RGBa").resize((LOGO_WIDTH, height), Image.LANCZOS).convert("RGBA")).copy()
    scaled[scaled[..., 3] == 0, :3] = 0
    Image.fromarray(scaled).save(ASSETS / "logo.png", optimize=True)
    return LOGO_WIDTH, height


def smooth(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0, 1)
    return t * t * (3 - 2 * t)


def to_hsv(rgb):
    high, low = rgb.max(2), rgb.min(2)
    chroma = high - low
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    c = np.maximum(chroma, 1e-6)
    hue = np.where(high == r, ((g - b) / c) % 6, np.where(high == g, (b - r) / c + 2, (r - g) / c + 4)) * 60
    saturation = np.where(high > 0, chroma / np.maximum(high, 1e-6), 0)
    return hue, saturation, high


def to_rgb(hue, saturation, value):
    h = (hue % 360) / 60
    c = value * saturation
    x = c * (1 - np.abs(h % 2 - 1))
    z = np.zeros_like(h)
    sector = np.floor(h).astype(int) % 6
    rgb = np.select([sector[..., None] == n for n in range(6)],
                    [np.stack(t, -1) for t in ((c, x, z), (x, c, z), (z, c, x), (z, x, c), (x, z, c), (c, z, x))])
    return rgb + (value - c)[..., None]


def make_header(source):
    rgb = np.asarray(Image.open(source).convert("RGB")).astype(np.float32) / 255
    height, width = rgb.shape[:2]
    ys, xs = np.mgrid[0:height, 0:width].astype(np.float32)
    # Places are given in the original's 1920 x 620 pixels.
    xs, ys = xs * 1920 / width, ys * 620 / height

    def ellipse(cx, cy, rx, ry, feather):
        distance = np.sqrt(((xs - cx) / rx) ** 2 + ((ys - cy) / ry) ** 2)
        return 1 - smooth(1 - feather, 1 + feather, distance)

    hue, saturation, value = to_hsv(rgb)
    hue = np.where(hue > 300, hue - 360, hue)  # reds around 0 as small negatives
    colourful = smooth(0.12, 0.30, saturation) * smooth(0.06, 0.18, value)
    yellow = smooth(10, 20, hue) * (1 - smooth(70, 85, hue))  # orange, gold and yellow
    red = (1 - smooth(8, 18, hue)) * smooth(-40, -25, hue)
    green = smooth(75, 90, hue) * (1 - smooth(150, 170, hue))
    flames = 1 - smooth(640, 780, xs)  # the red flames' side, left of Boo
    face = np.maximum(ellipse(1318, 335, 205, 160, 0.18), ellipse(1460, 300, 60, 90, 0.25))  # skin and ear
    tongue = ellipse(850, 110, 75, 70, 0.2)  # Boo's
    glow = ellipse(1715, 395, 115, 125, 0.25)  # the red glow behind the Bob-omb
    fire = colourful * np.maximum(yellow * (1 - face), red * np.maximum(flames * (1 - tongue), glow))
    grass = colourful * green

    fire_rgb = to_rgb(193 + (50 - np.clip(hue, -20, 75)) * 0.36, np.clip(saturation * 0.92, 0, 1), value)
    grass_rgb = to_rgb(np.full_like(hue, 205), saturation * 0.75, value * 0.9)
    out = rgb * (1 - fire - grass)[..., None] + fire_rgb * fire[..., None] + grass_rgb * grass[..., None]
    Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8)).save(ASSETS / "header.png", optimize=True)
    return width, height


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
