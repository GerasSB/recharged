# Application resources

## Header

`header.png` is the 1920 x 620 PNG selected for the launcher header, recoloured
to match the logo: the fire and the gold of the ball became electric blue, and
Mario keeps his colours. `tools/launcher_art.py header ORIGINAL.png` makes it
from the original. Preserve its aspect ratio when displaying it.

- Source: [SteamGridDB hero 8926](https://www.steamgriddb.com/hero/8926).
- Uploaded by [Jiquita](https://www.steamgriddb.com/profile/76561198074965726).
- Image: [original PNG](https://cdn2.steamgriddb.com/hero/82b04cd5aa016d979fe048f3ddf0e8d3.png).
- Downloaded: 2026-10-01.
- SHA-256 of the original: `d1451de5493ffecd9a50541456a823e5e464edb6cae39e6a99e615afdae92617`.
- Recoloured: 2026-10-25.

This third-party artwork and its recoloured version are outside the project's
CC0 dedication. The source metadata does not specify a redistribution license;
rights remain with the respective owners.

CMake copies the image into `assets/launcher/header.png` beside the `recharged`
executable. The launcher preserves its proportions and crops vertically to fit
the available header space.

## Logo

`logo.png` is the Recharged logo, cropped to its visible pixels and scaled to
1280 pixels wide from the 2732 x 2048 original (SHA-256
`61ce9890e904f4af3840482091c1504e49045f815300197e20529196e4f1ad5d`, added
2026-10-25) by `tools/launcher_art.py logo ORIGINAL.png`. The launcher shows it
in the sidebar, on the Play page and on the About page; for the smaller sizes it
draws halved copies it makes at start. Its colours set the launcher's palette.

The logo is outside the project's CC0 dedication; rights remain with its creator.

## Application icon

`icon.png` is the original, unmodified 512 x 512 Mario portrait.

- Source: [SteamGridDB icon 103028](https://www.steamgriddb.com/icon/103028).
- Uploaded by [roryypc](https://www.steamgriddb.com/profile/76561198982504400).
- Image: [original PNG](https://cdn2.steamgriddb.com/icon/7c867647488e862e745b6992a0f882e4.png).
- Downloaded: 2026-10-08.
- SHA-256: `b9a96660da3bfae23581b628705a6096e48aeda6887632551505ba980bd6d3a6`.

The PNG is embedded for the launcher and game window; SDL also uses it for the
running application's macOS Dock icon. Window icon support on Linux depends on
the desktop/compositor. `icon.ico` contains 16, 24, 32, 48, 64, 128 and 256 pixel
versions for the Windows executable, generated with:

```sh
magick icon.png -define icon:auto-resize=256,128,64,48,32,24,16 icon.ico
```

`icon.icns` is the macOS app icon (`Recharged.app`): the PNG itself at 512
pixels and 256, 128, 64, 32 and 16 pixel versions, written by
`tools/make_icns.py` (ImageMagick resizes).

The icon artwork and its format conversion are outside the project's CC0
dedication. The source metadata does not specify a redistribution license;
rights remain with the respective owners.

## Font

The launcher also copies `misc/fonts/Roboto-Medium.ttf` from the pinned
[Dear ImGui source](../../extern/imgui/misc/fonts/Roboto-Medium.ttf).
The upstream [font inventory](../../extern/imgui/docs/FONTS.md) identifies it
as Roboto Medium under the Apache License 2.0. Its embedded notice reads:
Copyright 2011 Google Inc. All Rights Reserved. The font is copied unchanged.
The full Apache 2.0 text is copied as `LICENSE-APACHE`
alongside the font, image, and this notice. That generic license text is stored
in [LICENSES/Apache-2.0.txt](../../LICENSES/Apache-2.0.txt), copied verbatim from
the pinned Abseil source's Apache 2.0 text.

Resources are located relative to the executable, independently of the current
working directory. Keep `assets/launcher/` with a copied executable.

## Welcome guide

`welcome.md` is the guide the launcher opens at its first start, and again with
the light bulb in the bar at the bottom of every page. It is read beside the executable like the other resources
and drawn with the release notes' Markdown subset: headings, paragraphs, lists,
code and **strong** text; its first `#` heading is the dialog's title. Edit it
like any other text; it is part of the project (CC0).

## Aurora logo

The About page shows Aurora's logo, `assets/aurora.png` from the pinned
[Aurora](https://github.com/encounter/aurora) source. It is not stored here:
CMake copies it from the prepared Aurora tree into `assets/launcher/aurora.png`
beside the executable. Aurora is MIT-licensed (Copyright (c) 2022 Luke Street).
