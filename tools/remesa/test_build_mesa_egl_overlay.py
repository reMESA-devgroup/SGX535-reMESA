#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Builder input-rejection tests; no EGL/DRM execution or GPU fixtures."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import tempfile
import unittest

DIRECTORY = Path(__file__).resolve().parent
sys.path.insert(0, str(DIRECTORY))
SPEC = importlib.util.spec_from_file_location("build_mesa_egl_overlay", DIRECTORY / "build_mesa_egl_overlay.py")
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)
from prepare_egl_primary import prepare
from prepare_mesa_loader import prepare as prepare_combined
CACHE = None


class InputValidation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.overlay = self.root / "overlay"
        prepare(CACHE / "mesa-25.2.0", self.overlay)

    def tearDown(self):
        self.temp.cleanup()

    def test_exact_pinned_overlay(self):
        manifest, sources = build.validate_overlay(CACHE, self.overlay)
        self.assertEqual(manifest["mesa"], "25.2.0")
        self.assertEqual(len(sources), 5)
        self.assertEqual(len(build.UNITS), 3)

    def test_stale_manifest_hash(self):
        path = self.overlay / "integration.json"
        manifest = json.loads(path.read_text())
        manifest["files"][build.UNITS[0]] = "0" * 64
        path.write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            build.validate_overlay(CACHE, self.overlay)

    def test_combined_mesa_overlay(self):
        combined = self.root / "combined"
        prepare_combined(CACHE / "pristine-contracts", DIRECTORY.parents[1],
                         combined, CACHE / "mesa-25.2.0")
        manifest, sources = build.validate_overlay(CACHE, combined)
        self.assertEqual(manifest["originals"], build.PIN)
        self.assertEqual(len(sources), 5)
        path = combined / "egl-primary-integration.json"
        driver = combined / "src/gallium/drivers/sgx535"
        for name in ("remesa_present.c", "remesa_present.h"):
            self.assertEqual((driver / name).read_bytes(),
                             (DIRECTORY.parents[1] / "mesa/sgx535" / name).read_bytes())
        self.assertIn("'remesa_present.c'", (driver / "meson.build").read_text())
        # Follow project-owned quoted includes, including transitive headers.
        # A build must not accidentally find omitted files in the worktree.
        repo = DIRECTORY.parents[1]
        for staged in driver.rglob('*'):
            if staged.suffix not in ('.c', '.h', '.inc'):
                continue
            relative = staged.relative_to(driver)
            origin = (repo / relative if relative.parts[0] == 'include' else
                      repo / 'mesa/sgx535' / relative)
            for name in re.findall(r'^\s*#\s*include\s+"([^"]+)"', staged.read_text(), re.M):
                original = next((p for p in (origin.parent/name,
                    repo/'mesa/sgx535'/name, repo/'include'/name) if p.is_file()), None)
                if original is None:
                    continue  # Mesa and system headers have separate build inputs.
                copied = next((p for p in (staged.parent/name, driver/name,
                    driver/'include'/name) if p.is_file()), None)
                self.assertIsNotNone(copied, f'{relative}: missing project include {name}')
                self.assertEqual(copied.read_bytes(), original.read_bytes())
        manifest["files"][build.UNITS[0]] = "0" * 64
        path.write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            build.validate_overlay(CACHE, combined)

    def test_changed_source_with_matching_new_hash_rejected(self):
        relative = build.UNITS[0]
        path = self.overlay / relative
        path.write_text(path.read_text() + "\n/* altered implementation */\n")
        manifest_path = self.overlay / "integration.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["files"][relative] = build.digest(path)
        manifest_path.write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            build.validate_overlay(CACHE, self.overlay)

    def test_missing_helper_dependency(self):
        path = self.overlay / "integration.json"
        manifest = json.loads(path.read_text())
        del manifest["files"]["src/egl/drivers/dri2/remesa_primary_device.h"]
        path.write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            build.validate_overlay(CACHE, self.overlay)

    def test_changed_original_identity_rejected(self):
        path = self.overlay / "integration.json"
        manifest = json.loads(path.read_text())
        manifest["originals"][build.UNITS[0]] = "0" * 64
        path.write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            build.validate_overlay(CACHE, self.overlay)

    def test_frontend_architecture(self):
        path = self.root / "header-only-fixture"
        header = bytearray(20)
        header[:6] = b"\x7fELF\x01\x01"
        struct.pack_into("<HH", header, 16, 3, 3)
        path.write_bytes(header)
        # Only validates the ELF header. The real linker validates actual full
        # libraries during compilation; this fixture is never linked/executed.
        build.validate_frontend(path)
        for bad in (b"not ELF", bytes(header[:4]) + b"\x02" + bytes(header[5:]),
                    bytes(header[:16]) + struct.pack("<HH", 3, 62)):
            path.write_bytes(bad)
            with self.assertRaises(ValueError):
                build.validate_frontend(path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--mesa-cache", required=True, type=Path)
    args, remaining = parser.parse_known_args()
    CACHE = args.mesa_cache.resolve()
    unittest.main(argv=[sys.argv[0], *remaining])
