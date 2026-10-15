# Graphics pipeline catalogue

`initial_pipeline_cache.db` lists renderer configurations recorded from runs of
the original game. It contains no textures, models, sound, game executable code,
shaders from the disc, or GPU/driver binaries. Before the game starts, the
graphics backend builds these pipelines for the current GPU. The first launch on
a PC compiles them once, on several processor cores; later launches load the
compiled results from the user's local cache.

The reviewed source is `pipelines.json`: 532 GX v16 configurations and 2
clear v5 configurations, in Aurora's little-endian 64-bit native configuration
layout. The configurations were recorded by scripted runs: the boot sequence and
menus, exhibition matches in all 17 stadiums with all 12 captains (default and
random sidekicks; Mega Strikes and skillshots during play), nine Striker
Challenges and eight Strikers 101 lessons. This is not proof of complete
coverage of every effect, language or mode.

The game draws an effect with the fog of the stadium it appears in, and the fog
type is part of the pipeline configuration. The generated resource therefore
also holds, for each GX configuration recorded with fog, the same configuration
under every other fog type the recordings use, including none (340
configurations). Configurations only recorded without fog, such as menus, have
no variants.

A configuration missing from the resource still compiles when it is first
drawn, and is prepared from the next launch on. The draw waits for that compile,
so the session log (`logs/mscharged.log`) records it, with the configuration
itself (zlib, base64) for a configuration new on that PC:

```text
Graphics: drawing waited 15.0 ms to compile a new pipeline: pipeline=gx16-d7855b8409c7cf9b frame=62 config=eNrt...
```

The build generator validates SHA-256, versions, byte lengths, basic field
domains and duplicates, and computes each fog variant's XXH3 key as Aurora does.
Packaging checks that the generated database exactly matches the source
catalogue and its fog variants.

Change the catalogue deliberately when new coverage is collected; builds never
scan a developer's runtime caches. To merge the configurations a game run
recorded (`original-main-credits-data/pipeline_cache.db` beside the executable;
the fog variants the resource seeded there are skipped), or the new ones that
testers' session logs list (each line is checked against its key):

```sh
python3 tools/pipeline_catalog.py --catalog assets/graphics/pipelines.json \
    --import path/to/pipeline_cache.db path/to/mscharged.log
```

Regenerate the standalone resource with Python 3.10 or later:

```sh
python3 tools/pipeline_catalog.py --catalog assets/graphics/pipelines.json \
    --output build/assets/graphics/initial_pipeline_cache.db
```

The JSON is the reviewed source; SQLite files are generated build artifacts.
There are no wall-clock timestamps, machine paths or device identities in the
resource. The table contents and row ordering are deterministic. SQLite's on-disk
encoding may differ between SQLite versions; package validation compares the
complete logical contents, rather than requiring identical SQLite file bytes.
