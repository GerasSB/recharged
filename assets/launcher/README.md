# Application resources

## Header

`header.png` is made from the game's artwork of Mario volleying the ball
(`MarioStrikers2j.jpg`, 3800 x 3500, on white) by
`tools/launcher_art.py header MarioStrikers2j.jpg`: the white page and the gaps
in the brush stroke behind Mario are cut away, the stroke turns deep blue and
the fire and the golden ball the logo's electric blue, while Mario keeps his
colours; it is placed beside the logo on a night-blue background with a glow
behind the ball (1920 x 960). Preserve its aspect ratio when displaying it.

- Source: [File:MarioStrikers2j.jpg](https://www.mariowiki.com/File:MarioStrikers2j.jpg) on the Super Mario Wiki.
- Image: [original JPEG](https://www.mariowiki.com/images/5/55/MarioStrikers2j.jpg).
- Downloaded: 2026-10-25.
- SHA-256 of the original: `096d6cd0ba9abd223630318500df35e81b43b136277ba74896542c6390367ba1`.

This artwork is Nintendo's and outside the project's CC0 dedication. The wiki
publishes it as fair-use artwork; no license for redistribution is given, and
rights remain with the respective owners. (Until 2026-10-25 the header was
[SteamGridDB hero 8926](https://www.steamgriddb.com/hero/8926) by Jiquita, a
crop of the game's key art.)

CMake copies the image into `assets/launcher/header.png` beside the `recharged`
executable. The launcher preserves its proportions and crops it to fit the
available header space.

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
