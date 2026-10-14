#!/usr/bin/env python3
"""Generate or verify Aurora's portable pipeline descriptions, never driver code.

The reviewed source pins each captured XXH3 key and configuration SHA-256.
Capture import independently recomputes XXH3 keys against configuration bytes;
ordinary builds require only the Python standard library and never inspect local
runtime caches. The bundled format is for the supported little-endian 64-bit hosts.
"""

import argparse
from contextlib import closing
import hashlib
import json
import math
import os
from pathlib import Path
import re
import sqlite3
import struct
import tempfile

ABI = "aurora-gx13-clear5-le64"
LAYOUTS = {0: (5, 8), 1: (13, 2772)}
TABLE_SQL = """CREATE TABLE aurora_schema(value INTEGER);
INSERT INTO aurora_schema VALUES (1);
CREATE TABLE pipeline_cache (
  type INTEGER NOT NULL,
  hash INTEGER NOT NULL,
  config_version INTEGER NOT NULL,
  config_size INTEGER NOT NULL,
  config BLOB NOT NULL,
  first_frame_used INTEGER NOT NULL,
  PRIMARY KEY (type, hash)
);
CREATE INDEX pipeline_cache_load_order_idx
  ON pipeline_cache(type, config_version, first_frame_used);
"""


class CatalogError(ValueError):
    pass


def _integer(value, low, high):
    return type(value) is int and low <= value <= high


def _object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise CatalogError(f"Duplicate JSON field: {key}")
        result[key] = value
    return result


def read_catalog(path):
    """Validate reviewed inputs and return stable SQLite tuples."""
    try:
        catalog = json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=_object)
    except (OSError, ValueError) as error:
        raise CatalogError(f"Cannot read pipeline catalogue: {error}") from error
    if (not isinstance(catalog, dict)
            or set(catalog) != {"format", "schema", "config_abi", "pipelines"}
            or type(catalog["format"]) is not int or catalog["format"] != 1
            or type(catalog["schema"]) is not int or catalog["schema"] != 1
            or catalog["config_abi"] != ABI):
        raise CatalogError("Unsupported pipeline catalogue format, schema or ABI")
    if not isinstance(catalog["pipelines"], list) or not 1 <= len(catalog["pipelines"]) <= 100000:
        raise CatalogError("Expected a nonempty, bounded pipeline list")
    rows, keys, configs = [], set(), set()
    for index, row in enumerate(catalog["pipelines"]):
        prefix = f"Pipeline {index}: "
        if not isinstance(row, dict) or set(row) != {"type", "hash", "version", "first_frame", "config", "sha256"}:
            raise CatalogError(prefix + "invalid fields")
        kind = row["type"]
        if type(kind) is not int or kind not in LAYOUTS:
            raise CatalogError(prefix + "unsupported shader type")
        version, size = LAYOUTS[kind]
        if type(row["version"]) is not int or row["version"] != version:
            raise CatalogError(prefix + "unsupported configuration version")
        if not _integer(row["first_frame"], 0, 0xFFFFFFFF):
            raise CatalogError(prefix + "invalid first-use frame")
        if not isinstance(row["hash"], str) or not re.fullmatch(r"[0-9a-f]{16}", row["hash"]):
            raise CatalogError(prefix + "invalid captured XXH3 key")
        if not isinstance(row["config"], str) or not re.fullmatch(r"[0-9a-f]{" + str(size * 2) + "}", row["config"]):
            raise CatalogError(prefix + "invalid configuration encoding/size")
        blob = bytes.fromhex(row["config"])
        if row["sha256"] != hashlib.sha256(blob).hexdigest():
            raise CatalogError(prefix + "configuration SHA-256 mismatch")
        if struct.unpack_from("<I", blob)[0] != version:
            raise CatalogError(prefix + "configuration has wrong byte order/version")
        if kind == 0:
            if any(value > 1 for value in blob[4:7]) or blob[7] != 0:
                raise CatalogError(prefix + "invalid clear flags/padding")
        else:
            # These stable v13 tail fields also catch a wrong structure layout.
            enums = struct.unpack_from("<6I", blob, size - 44)
            if any(value > limit for value, limit in zip(enums, (7, 3, 3, 7, 7, 15))):
                raise CatalogError(prefix + "invalid GX fixed-function enum")
            alpha = struct.unpack_from("<I", blob, size - 20)[0]
            if alpha > 255 and alpha != 0xFFFFFFFF:
                raise CatalogError(prefix + "invalid destination alpha")
            if any(value > 1 for value in blob[-4:]):
                raise CatalogError(prefix + "invalid GX flags")
            if not all(math.isfinite(value) for value in struct.unpack_from("<3f", blob, size - 16)):
                raise CatalogError(prefix + "invalid polygon offset")
        key = (kind, int(row["hash"], 16))
        if key in keys:
            raise CatalogError(prefix + "duplicate/colliding pipeline key")
        if (kind, blob) in configs:
            raise CatalogError(prefix + "duplicate configuration under different keys")
        keys.add(key)
        configs.add((kind, blob))
        signed_hash = key[1] if key[1] < (1 << 63) else key[1] - (1 << 64)
        rows.append((kind, signed_hash, version, size, blob, row["first_frame"]))
    return sorted(rows, key=lambda r: (r[0], r[1] & 0xFFFFFFFFFFFFFFFF))


def verify_database(path, rows):
    """Reject missing/stale/modified package resources without modifying them."""
    path = Path(path)
    if path.is_symlink() or not path.is_file():
        raise CatalogError(f"Missing or unsafe pipeline resource: {path}")
    try:
        with closing(sqlite3.connect(path.resolve().as_uri() + "?mode=ro&immutable=1", uri=True)) as db:
            if db.execute("PRAGMA integrity_check").fetchall() != [("ok",)]:
                raise CatalogError("Pipeline resource failed SQLite integrity check")
            tables = db.execute("SELECT name FROM sqlite_schema WHERE type='table' ORDER BY name").fetchall()
            if tables != [("aurora_schema",), ("pipeline_cache",)]:
                raise CatalogError("Unexpected tables in pipeline resource")
            if db.execute("SELECT value FROM aurora_schema").fetchall() != [(1,)]:
                raise CatalogError("Wrong pipeline resource schema version")
            actual = db.execute("SELECT type,hash,config_version,config_size,config,first_frame_used FROM pipeline_cache").fetchall()
            actual.sort(key=lambda r: (r[0], r[1] & 0xFFFFFFFFFFFFFFFF))
            if actual != rows:
                raise CatalogError("Pipeline resource differs from its reviewed catalogue")
    except (sqlite3.Error, TypeError) as error:
        raise CatalogError(f"Invalid pipeline resource: {error}") from error


def generate(path, rows):
    """Write a standalone, deterministic DB atomically; keep unchanged mtimes."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".pipeline-catalog-", dir=path.parent) as temporary:
        candidate = Path(temporary) / "initial_pipeline_cache.db"
        with closing(sqlite3.connect(candidate)) as db:
            db.executescript("PRAGMA page_size=4096; PRAGMA journal_mode=DELETE;" + TABLE_SQL)
            db.executemany("INSERT INTO pipeline_cache VALUES (?,?,?,?,?,?)", rows)
            db.commit()
            db.execute("VACUUM")
        verify_database(candidate, rows)
        data = candidate.read_bytes()
        if path.is_symlink():
            raise CatalogError(f"Refusing symlink output: {path}")
        if path.is_file() and path.read_bytes() == data:
            return
        candidate.chmod(0o644)
        os.replace(candidate, path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, required=True)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--output", type=Path)
    action.add_argument("--verify", type=Path)
    args = parser.parse_args()
    try:
        rows = read_catalog(args.catalog)
        if args.output:
            generate(args.output, rows)
        else:
            verify_database(args.verify, rows)
    except (CatalogError, OSError) as error:
        parser.exit(1, f"pipeline catalogue: {error}\n")
    print(f"Pipeline catalogue: {len(rows)} verified configurations")


if __name__ == "__main__":
    main()
