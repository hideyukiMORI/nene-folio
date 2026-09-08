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
MODULES = json.loads((ROOT / "eng/architecture.json").read_text(encoding="utf-8"))["modules"]

NM_OUTPUT = """
note_id.c.obj:
0000000000000000 T note_id_parse
0000000000000000 R ??_C@_05MJPCMENP@notes?$AA@
0000000000000000 t local_helper
                 U memcpy
                 U _time64
                 U __imp_GetTickCount64
                 U __chkstk
                 U SomeUnknownLibraryCall

ledger.c.obj:
0000000000000000 T ledger_count
                 U note_id_parse
"""


class SymbolTests(unittest.TestCase):
    def test_parses_defined_and_undefined(self):
        defined, undefined = symbols.symbol_table(NM_OUTPUT)
        self.assertEqual({"note_id_parse", "??_C@_05MJPCMENP@notes?$AA@", "ledger_count"}, defined)
        self.assertEqual({"memcpy", "_time64", "__imp_GetTickCount64", "__chkstk", "SomeUnknownLibraryCall", "note_id_parse"},
                         undefined)

    def test_archive_internal_symbols_resolve(self):
        tables = {"core": symbols.symbol_table(NM_OUTPUT)}
        self.assertNotIn("note_id_parse", symbols.unresolved("core", tables, MODULES))
        self.assertIn("memcpy", symbols.unresolved("core", tables, MODULES))

    def test_declared_dependency_symbols_resolve(self):
        tables = {"core": ({"category_ledger_count"}, set()),
                  "application": (set(), {"category_ledger_count", "memcpy"})}
        self.assertEqual({"memcpy"}, symbols.unresolved("application", tables, MODULES))

    def test_undeclared_module_symbols_do_not_resolve(self):
        tables = {"application": ({"folio_state_create"}, set()),
                  "core": (set(), {"folio_state_create"})}
        self.assertEqual({"folio_state_create"}, symbols.unresolved("core", tables, MODULES))

    def test_allowed_symbols_pass(self):
        self.assertEqual([], symbols.classify({"memcpy", "__chkstk", "__asan_init", "__ubsan_handle_add_overflow"}, "core", ALLOWLIST))

    def test_time_is_arc007(self):
        findings = symbols.classify({"_time64"}, "core", ALLOWLIST)
        self.assertEqual(1, len(findings))
        self.assertTrue(findings[0].startswith("ARC-007"))

    def test_win32_tick_is_arc007(self):
        self.assertTrue(symbols.classify({"__imp_GetTickCount64"}, "application", ALLOWLIST)[0].startswith("ARC-007"))

    def test_unknown_is_arc003(self):
        self.assertTrue(symbols.classify({"SomeUnknownLibraryCall"}, "core", ALLOWLIST)[0].startswith("ARC-003"))

    def test_product_prefix_is_not_a_free_pass(self):
        self.assertTrue(symbols.classify({"nenefolio_now"}, "core", ALLOWLIST)[0].startswith("ARC-003"))

    def test_win32_import_in_core_is_arc003(self):
        self.assertTrue(symbols.classify({"__imp_CreateFileW"}, "core", ALLOWLIST)[0].startswith("ARC-003"))

    def test_thread_creation_is_arc003(self):
        self.assertTrue(symbols.classify({"__imp_CreateThread", "_beginthreadex"}, "application", ALLOWLIST))

    def test_partial_match_is_not_allowed(self):
        self.assertTrue(symbols.classify({"memcpy_s"}, "core", ALLOWLIST))


if __name__ == "__main__":
    unittest.main()
