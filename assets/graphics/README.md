# Graphics pipeline catalogue

`initial_pipeline_cache.db` contains portable renderer configurations collected
from original-game runs. It contains no textures, models, sound, game executable
code, shaders from the disc, or GPU/driver binaries. Each graphics backend builds
its own pipelines on the current device before the game starts. Compiled results
remain in memory and the user's ordinary local GPU cache.

The reviewed source is `pipelines.json`: 1,032 GX v13 configurations and three
clear v5 configurations, using Aurora's little-endian 64-bit native configuration
layout. Captured XXH3 keys were independently checked against their complete
configuration bytes when importing the catalogue. The build generator validates
SHA-256, versions, byte lengths, basic field domains and duplicates. Packaging
checks that the generated database exactly matches this source catalogue.

This is an initial catalogue from existing original-game menu, match, lesson and
rendering-regression runs. It is not proof of complete coverage of every team,
stadium, effect, language or mode. A previously unseen combination still compiles
on demand and is remembered for the next run. Change the catalogue deliberately
when new coverage is collected; builds never scan a developer's runtime caches.

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
