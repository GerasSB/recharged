#!/usr/bin/env python3
"""Make the launcher's logo and header images in assets/launcher/ from their originals.

    launcher_art.py logo ORIGINAL.png          # logo.png: the Recharged logo, trimmed, 1280 pixels wide
    launcher_art.py header MarioStrikers2j.jpg # header.png: Mario's volley on night blue, 1920 x 960

The header puts the game's artwork of Mario volleying the ball (3800 x 3500 on
white) on a night-blue background, beside where the launcher draws its logo.
The white page and the gaps in the brush stroke behind Mario are cut away, the
stroke turns deep blue, the fire and the golden ball the logo's electric blue;
Mario keeps his colours. Needs Pillow, numpy and scipy. assets/launcher/README.md
lists the originals.
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

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


def smooth(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0, 1)
    return t * t * (3 - 2 * t)


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
    if (width, height) != (3800, 3500):
        raise SystemExit(f"{source}: expected the 3800 x 3500 artwork, found {width} x {height}")
    ys, xs = np.mgrid[0:height, 0:width].astype(np.float32)
    high, low = rgb.max(2), rgb.min(2)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    c = np.maximum(high - low, 1e-6)
    hue = np.where(high == r, ((g - b) / c) % 6, np.where(high == g, (b - r) / c + 2, (r - g) / c + 4)) * 60
    hue = np.where(hue > 300, hue - 360, hue)  # reds around 0 as small negatives
    saturation = np.where(high > 0, (high - low) / np.maximum(high, 1e-6), 0)
    value = high

    # The brush stroke behind Mario: warm, greyish, mid-bright.
    stroke = (smooth(12, 24, hue) * (1 - smooth(58, 72, hue)) * smooth(0.04, 0.09, saturation)
              * (1 - smooth(0.30, 0.42, saturation)) * smooth(0.28, 0.38, value) * (1 - smooth(0.74, 0.84, value)))
    # White: the page and the gaps in the stroke are background, the white inside
    # Mario's ink outlines (glove, eyes, the M) is not. Each white area is judged
    # by what borders it: the stroke, or ink.
    white = low > 0.84
    labels, count = ndimage.label(white)
    ring = ndimage.binary_dilation(white, iterations=3) & ~white
    owners = ndimage.grey_dilation(labels, size=(7, 7))[ring]
    border = np.bincount(owners, minlength=count + 1).astype(np.float64)
    by_stroke = np.bincount(owners, weights=(stroke[ring] > 0.5).astype(np.float64), minlength=count + 1)
    by_ink = np.bincount(owners, weights=(value[ring] < 0.30).astype(np.float64), minlength=count + 1)
    page = np.zeros(count + 1, bool)
    page[np.unique(np.concatenate([labels[0], labels[-1], labels[:, 0], labels[:, -1]]))] = True
    cut = page | ((by_stroke / np.maximum(border, 1) > 0.35) & (by_ink / np.maximum(border, 1) < 0.25))
    cut[0] = False
    alpha = 1 - ndimage.gaussian_filter(cut[labels].astype(np.float32), 1.2)

    # Recolour: the stroke to deep blue (its texture kept), the fire and the ball
    # to electric blue (hot yellow to icy cyan, orange to sky blue, red to deep
    # blue). Mario's face and, right of the ball, his reds stay.
    def ellipse(cx, cy, rx, ry, feather):
        distance = np.sqrt(((xs - cx) / rx) ** 2 + ((ys - cy) / ry) ** 2)
        return 1 - smooth(1 - feather, 1 + feather, distance)

    face = np.maximum(ellipse(2560, 1440, 330, 270, 0.2), ellipse(2330, 1560, 170, 140, 0.25))
    colourful = smooth(0.20, 0.38, saturation) * smooth(0.10, 0.22, value)
    yellow = smooth(10, 20, hue) * (1 - smooth(70, 85, hue))
    red = (1 - smooth(8, 18, hue)) * smooth(-40, -25, hue)
    ball_side = 1 - smooth(1650, 1950, xs)
    fire = np.clip(colourful * np.maximum(yellow * (1 - face), red * ball_side) * (1 - stroke), 0, 1)
    fire_rgb = to_rgb(193 + (50 - np.clip(hue, -20, 75)) * 0.36, np.clip(saturation * 0.95, 0, 1), value)
    luma = 0.299 * r + 0.587 * g + 0.114 * b
    deep, steel = np.array([0.035, 0.075, 0.17]), np.array([0.13, 0.25, 0.48])
    stroke_rgb = deep + (steel - deep) * np.clip((luma - 0.28) / 0.45, 0, 1)[..., None]
    out = rgb * (1 - fire - stroke)[..., None] + fire_rgb * fire[..., None] + stroke_rgb * stroke[..., None]
    art = Image.fromarray((np.dstack([np.clip(out, 0, 1), alpha[..., None]]) * 255).astype(np.uint8), "RGBA")

    # Scaled so that the ball (at 980, 1920) lands beside the logo, Mario to its right.
    scale, ball = 0.32, (1160, 470)
    size = (round(width * scale), round(height * scale))
    art = art.convert("RGBa").resize(size, Image.LANCZOS).convert("RGBA")
    layer = Image.new("RGBA", (1920, 960))
    layer.paste(art, (round(ball[0] - 980 * scale), round(ball[1] - 1920 * scale)))  # cut off at the edges

    # The background: night blue, lighter at the top, a cyan glow behind the ball.
    gy, gx = np.mgrid[0:960, 0:1920].astype(np.float32)
    top, bottom = np.array([0.055, 0.10, 0.22]), np.array([0.02, 0.04, 0.09])
    canvas = top + (bottom - top) * (gy / 960)[..., None]
    glow = np.exp(-(((gx - ball[0]) / 520) ** 2 + ((gy - ball[1]) / 340) ** 2))
    canvas = canvas + np.array([0.05, 0.20, 0.32]) * (glow * 0.55)[..., None]
    base = Image.fromarray((np.clip(canvas, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB").convert("RGBA")
    Image.alpha_composite(base, layer).convert("RGB").save(ASSETS / "header.png", optimize=True)
    return 1920, 960


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
