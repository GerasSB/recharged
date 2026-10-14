"""Portable catalogue validation and generated-resource corruption gates."""
import copy
from contextlib import closing
import hashlib
import importlib.util
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("pipeline_catalog", ROOT / "tools/pipeline_catalog.py")
CATALOG = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CATALOG)


class PipelineCatalogTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "catalog.json"
        blob = bytes.fromhex("0500000001010100")
        self.catalog = {"format": 1, "schema": 1, "config_abi": CATALOG.ABI, "pipelines": [
            {"type": 0, "version": 5, "hash": "ffffffffffffffff", "first_frame": 12,
             "config": blob.hex(), "sha256": hashlib.sha256(blob).hexdigest()}]}

    def read(self, catalog=None):
        self.source.write_text(json.dumps(self.catalog if catalog is None else catalog))
        return CATALOG.read_catalog(self.source)

    def test_signed_hash_and_byte_exact_roundtrip(self):
        rows = self.read()
        self.assertEqual(rows[0][1], -1)
        target = self.root / "folder with spaces/initial_pipeline_cache.db"
        CATALOG.generate(target, rows)
        CATALOG.verify_database(target, rows)
        with closing(sqlite3.connect(target)) as db:
            self.assertEqual(db.execute("SELECT * FROM pipeline_cache").fetchall(), rows)
        self.assertFalse(Path(str(target) + "-wal").exists())

    def test_repeated_generation_is_deterministic_and_keeps_mtime(self):
        # Different flags, priorities and signed/unsigned key representations
        # make input-order mistakes observable in the SQLite rows and file bytes.
        for key, flags, frame in (("0000000000000001", "01000000", 27),
                                  ("8000000000000000", "00010100", 3)):
            blob = bytes.fromhex("05000000" + flags)
            self.catalog["pipelines"].append({
                "type": 0, "version": 5, "hash": key, "first_frame": frame,
                "config": blob.hex(), "sha256": hashlib.sha256(blob).hexdigest()})
        rows = self.read()
        shuffled = copy.deepcopy(self.catalog)
        shuffled["pipelines"].reverse()
        reordered = self.read(shuffled)
        self.assertEqual(len(rows), 3)
        self.assertEqual(rows, reordered)
        self.assertEqual([row[1] for row in rows], [1, -(1 << 63), -1])
        one, two = self.root / "one.db", self.root / "two.db"
        CATALOG.generate(one, rows)
        before = one.stat().st_mtime_ns
        CATALOG.generate(two, reordered)
        self.assertEqual(one.read_bytes(), two.read_bytes())
        CATALOG.generate(one, rows)
        self.assertEqual(one.stat().st_mtime_ns, before)

    def test_wrong_format_version_size_hash_and_endian_rejected(self):
        cases = [("version", 4), ("type", 2), ("hash", "1"), ("first_frame", -1),
                 ("first_frame", 0x100000000), ("first_frame", True),
                 ("config", "00"), ("sha256", "0" * 64)]
        for field, value in cases:
            candidate = copy.deepcopy(self.catalog)
            candidate["pipelines"][0][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(CATALOG.CatalogError):
                self.read(candidate)
        for blob in (bytes.fromhex("0000000501010100"), bytes.fromhex("0500000002010100")):
            candidate = copy.deepcopy(self.catalog)
            candidate["pipelines"][0].update(config=blob.hex(), sha256=hashlib.sha256(blob).hexdigest())
            with self.assertRaises(CATALOG.CatalogError):
                self.read(candidate)
        candidate = copy.deepcopy(self.catalog)
        candidate["config_abi"] = "other"
        with self.assertRaises(CATALOG.CatalogError):
            self.read(candidate)

    def test_duplicate_and_colliding_keys_rejected(self):
        candidate = copy.deepcopy(self.catalog)
        candidate["pipelines"].append(copy.deepcopy(candidate["pipelines"][0]))
        with self.assertRaisesRegex(CATALOG.CatalogError, "duplicate/colliding"):
            self.read(candidate)
        candidate["pipelines"][1]["hash"] = "0000000000000001"
        with self.assertRaisesRegex(CATALOG.CatalogError, "duplicate configuration"):
            self.read(candidate)

    def test_duplicate_json_fields_rejected(self):
        self.source.write_text('{"format":1,"format":1}')
        with self.assertRaisesRegex(CATALOG.CatalogError, "Duplicate JSON"):
            CATALOG.read_catalog(self.source)

    def test_corrupt_and_stale_databases_rejected(self):
        rows = self.read()
        target = self.root / "resource.db"
        target.write_bytes(b"not sqlite")
        with self.assertRaises(CATALOG.CatalogError):
            CATALOG.verify_database(target, rows)
        CATALOG.generate(target, rows)
        with closing(sqlite3.connect(target)) as db:
            db.execute("UPDATE pipeline_cache SET first_frame_used=13")
            db.commit()
        with self.assertRaisesRegex(CATALOG.CatalogError, "differs"):
            CATALOG.verify_database(target, rows)
        CATALOG.generate(target, rows)
        with closing(sqlite3.connect(target)) as db:
            db.execute("CREATE TABLE private_data(value BLOB)")
            db.commit()
        with self.assertRaisesRegex(CATALOG.CatalogError, "Unexpected tables"):
            CATALOG.verify_database(target, rows)

    def test_symlinks_rejected_and_failed_validation_keeps_output(self):
        rows = self.read()
        real = self.root / "real.db"
        CATALOG.generate(real, rows)
        link = self.root / "link.db"
        link.symlink_to(real)
        before = real.read_bytes()
        with self.assertRaises(CATALOG.CatalogError):
            CATALOG.generate(link, rows)
        with self.assertRaises(CATALOG.CatalogError):
            CATALOG.verify_database(link, rows)
        self.assertEqual(before, real.read_bytes())

    def test_reviewed_catalogue_generates_and_verifies(self):
        rows = CATALOG.read_catalog(ROOT / "assets/graphics/pipelines.json")
        self.assertTrue(any(row[0] == 0 for row in rows))
        self.assertTrue(any(row[0] == 1 for row in rows))
        target = self.root / "initial_pipeline_cache.db"
        CATALOG.generate(target, rows)
        CATALOG.verify_database(target, rows)


if __name__ == "__main__":
    unittest.main()
