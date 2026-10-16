# Source patches

The port never edits its dependencies' checkouts. Every change to upstream
source (the decompilation, Aurora, Dawn, nod, SDL) is a patch in this folder,
applied to a generated copy during the build. How this fits into the build is
described in the [architecture](../docs/ARCHITECTURE.md#from-the-decompilation-to-the-program).

## Layout

Each `patches/<dependency>/` folder contains:

- `base`: the full commit the patches apply to. It must equal the submodule's
  recorded commit.
- `series`: the patch files in application order. Blank lines and lines
  starting with `#` are ignored.
- The patches. Each starts with a header that says what it changes and why:

  ```text
  From: mscharged-port
  Subject: Test desire update results as 32-bit codes
  Upstream-base: <full commit>
  Classification: TEMPORARY ...; remove when the pin includes decomp 0a6e72402.

  Why the change is needed and what it keeps unchanged.

  diff --git a/src/... b/src/...
  ```

  `Classification` is optional; a temporary patch names the upstream change
  that retires it.

| Folder | Applies to |
| --- | --- |
| `mscharged-decomp/` | The game: the faithful series (compiler, 64-bit, byte order and native platform adaptations) |
| `mscharged-decomp-tweaks/` | The game: every intended behavior change, applied after the faithful series |
| `aurora/` | Aurora: native platform services, graphics, presentation |
| `dawn/` | Dawn: build integration, Vulkan robustness, Windows DirectX shader compilers |
| `nod/` | nod: locked Cargo dependencies, reader lifetimes, disc read performance |
| `sdl/` | SDL: audio channel map fix |

## How patches are applied

`tools/prepare_sources.py` runs before CMake configures the build:

- It checks each submodule's commit against `base` and rejects tracked edits
  in the checkout.
- It exports the committed content (for the decomp, only `include/`, `libs/`
  and `src/`) into `build/<preset>/prepared/<dependency>/`, including required
  nested submodules at their pinned commits. Untracked or ignored files are
  never copied.
- It applies the series one patch at a time with `git apply`, strictly: no
  three-way merges, no ignored whitespace, no partial `.rej` files. Any failure
  stops the build.
- Only a completely prepared tree becomes available to CMake. A prepared tree
  is reused only while its recorded inputs (commits, patch contents, tool
  version, options) and contents match.

The decomp is prepared twice: `patched/` holds the exact export plus the
series, and `source/` the copy that is compiled. With clang-format 16+,
`source/` is formatted with
[`tools/formatting/prepared-sources.clang-format`](../tools/formatting/prepared-sources.clang-format);
every formatted file is compared with its input as preprocessing tokens, and
files whose program depends on their layout (`__LINE__`, stringified macro
arguments, `#line`) stay as patched (`kept_unformatted` in `manifest.json`).
Patches are always written against `patched/`, never against the formatted
copy.

## The two decomp series

**The faithful series** (`mscharged-decomp/`) adapts the reconstructed source
to modern compilers and replaces the Wii hardware: Wii compiler syntax and
intrinsics, native pointer widths, the byte order of serialized game data, and
the hand-over to native graphics, audio, input, file, clock, memory and thread
services. It preserves the game's algorithms, control flow, resource
selection, timing and presentation, including retail bugs and quirks. Suspected
reconstruction errors are fixed upstream in the decomp, not here.

**The tweak series** (`mscharged-decomp-tweaks/`, with its own `base` and
`series`) holds every intended change. Its code is compiled only into the game
module built with `MSCHARGED_TWEAKS`; every other target compiles the original
code. The optional tweaks take the original path unless their switch in
`[gameplay]` is on, and with all of them off the game plays as on the Wii. The
game module receives the enabled set from the host before original `main()`
(`src/platform/game_tweaks.cpp`). Refresh this series on every pin update.

| Patch | Kind | Change |
| --- | --- | --- |
| 0001 | Interface | Declares the hooks the tweaks use |
| 0002 | Tweak | Unlock everything |
| 0003 | Tweak | No Mega Strikes |
| 0004 | Tweak | Skip the intro |
| 0005 | Tweak | Fast menus |
| 0006 | Fix, always on | Resting goal nets are redrawn ([#19](https://github.com/yannicksuter/mscharged-port/issues/19)) |
| 0008 | Tweak | CPU vs CPU |
| 0009 | Fix, always on | Loads the electrocution overlay texture, which the disc keeps only in an unused debug bundle ([#33](https://github.com/yannicksuter/mscharged-port/issues/33)) |
| 0010 | Fix, always on | A ball lost by an intangible player gets its player and goalie collisions back ([#38](https://github.com/yannicksuter/mscharged-port/issues/38)) |
| 0011 | Port, always on | Starts without the boot "Loading" bitmap, which followed the port's own graphics preparation |
| 0012 | Tweak | Fast terrain |
| 0013, 0014 | Option | Frame rates above the original: the host presents frames, and the steps the original makes once per drawn frame keep their pace |
| 0015 | Tweak | Mirror match |

## Develop a new patch

The examples use `build/release`.

1. Configure once, or run
   `python3 tools/prepare_sources.py --build-dir build/release`, to create a
   clean prepared tree.
2. Edit the files under `build/release/prepared/mscharged-decomp/patched/`
   (`source/` for the other dependencies). Do not run a formatter there, and do
   not edit the decomp's compiled `source/` copy; the tool rejects such edits.
3. Export the edits relative to the currently applied series (add
   `--dependency aurora` and so on for another series):

   ```sh
   python3 tools/prepare_sources.py --build-dir build/release --export-patch build/new-change.patch
   ```

4. Review the diff, move it under `patches/<dependency>/` with a focused name,
   add the header, and append its filename to `series`. Keep new port code in
   `src/` where possible, so patches stay small.
5. Regenerate explicitly, then build and test:

   ```sh
   python3 tools/prepare_sources.py --build-dir build/release --discard-generated
   cmake --build --preset release
   ```

The tool refuses to overwrite edited generated sources without
`--discard-generated`. Export before changing the pin, the series or the
preparation tool, so the comparison has the right baseline. Exported patches
can contain new files and deletions; review both. Dawn's selected nested
submodules (listed in `cmake/Dawn.cmake`) must be passed as repeated
`--nested-submodule PATH` arguments when preparing or exporting a Dawn patch
by hand.

If an interrupted preparation leaves an empty
`build/<preset>/prepared/.<dependency>.lock` directory, make sure the process
has stopped before removing it.

## Update a dependency

Pick an explicit commit, update and stage the submodule pin, review the
upstream changes, update `base`, and refresh or retire the affected patches:
a patch whose fix has landed upstream is removed in the same change. Then
configure, compile and test the affected behavior, and commit it all as one
reviewable change. Previous pins and patch sets stay reproducible from Git
history. Builds never follow a branch tip by themselves.
