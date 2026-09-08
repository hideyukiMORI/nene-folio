"""Positive and negative inputs for the linker-level symbol check (ARC-003 / ARC-007)."""

import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("symbols", ROOT / "eng/symbols.py")
symbols = importlib.util.module_from_spec(spec)
spec.loader.exec_module(symbols)
ALLOWLIST = json.loads((ROOT / "eng/symbol-allowlist.json").read_text(encoding="utf-8"))

NM_OUTPUT = """
note_id.c.obj:
0000000000000000 T note_id_parse
                 U memcpy
                 U _time64
                 U __imp_GetTickCount64
                 U __chkstk
                 U SomeUnknownLibraryCall
"""


class SymbolTests(unittest.TestCase):
    def test_parses_only_undefined(self):
        self.assertEqual({"memcpy", "_time64", "__imp_GetTickCount64", "__chkstk", "SomeUnknownLibraryCall"},
                         symbols.undefined_symbols(NM_OUTPUT))

    def test_allowed_symbols_pass(self):
        self.assertEqual([], symbols.classify({"memcpy", "__chkstk", "nenefolio_note_id_parse"}, "core", ALLOWLIST))

    def test_time_is_arc007(self):
        findings = symbols.classify({"_time64"}, "core", ALLOWLIST)
        self.assertEqual(1, len(findings))
        self.assertTrue(findings[0].startswith("ARC-007"))

    def test_win32_tick_is_arc007(self):
        self.assertTrue(symbols.classify({"__imp_GetTickCount64"}, "application", ALLOWLIST)[0].startswith("ARC-007"))

    def test_unknown_is_arc003(self):
        self.assertTrue(symbols.classify({"SomeUnknownLibraryCall"}, "core", ALLOWLIST)[0].startswith("ARC-003"))

    def test_win32_import_in_core_is_arc003(self):
        self.assertTrue(symbols.classify({"__imp_CreateFileW"}, "core", ALLOWLIST)[0].startswith("ARC-003"))

    def test_partial_match_is_not_allowed(self):
        self.assertTrue(symbols.classify({"memcpy_s"}, "core", ALLOWLIST))


if __name__ == "__main__":
    unittest.main()
