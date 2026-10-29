"""Reject measurement/Debug sanitizer drift and unrelated counterexample failures (C-016/QLT-007)."""

import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "eng"))
spec = importlib.util.spec_from_file_location("folio_coverage", ROOT / "eng/coverage.py")
coverage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(coverage)

FLAGS = ["-fsanitize=address", "-fsanitize=undefined", "-fsanitize=nullability",
         "-fno-sanitize-recover=all"]
SOURCES = {(ROOT / "src/core/text_buffer.c").resolve(),
           (ROOT / "tests/unit/allocation_tests.c").resolve()}


def records(flags=None):
    return [{"file": str(path), "arguments": ["clang-cl", *(FLAGS if flags is None else flags)]}
            for path in sorted(SOURCES)]


class CoverageSanitizerTests(unittest.TestCase):
    def test_borrows_exact_complete_debug_vector(self):
        self.assertEqual(FLAGS, coverage.sanitizer_vector(records(), SOURCES))

    def test_reads_windows_command_and_driver_wrappers(self):
        commands = [{"file": str(path),
                     "command": '"C:\\Program Files\\LLVM\\clang-cl.exe" /c '
                                + " ".join("/clang:" + flag for flag in FLAGS)}
                    for path in sorted(SOURCES)]
        self.assertEqual(FLAGS, coverage.sanitizer_vector(commands, SOURCES))

    def test_missing_sanitizer_or_recover_prohibition_is_rejected(self):
        for missing in FLAGS:
            with self.subTest(missing=missing), self.assertRaisesRegex(ValueError, "incomplete Debug"):
                coverage.sanitizer_vector(records([flag for flag in FLAGS if flag != missing]), SOURCES)

    def test_all_records_must_agree_even_when_both_are_complete(self):
        commands = records()
        commands[1]["arguments"].append("-fsanitize=signed-integer-overflow")
        with self.assertRaisesRegex(ValueError, "inconsistent Debug"):
            coverage.sanitizer_vector(commands, SOURCES)

    def test_disabling_or_recovering_options_are_not_dropped(self):
        for option in ("-fno-sanitize=nullability", "-fno-sanitize=all", "-fsanitize-recover=all",
                       "/clang:-fno-sanitize=undefined", "-fsanitize-ignorelist=ignored.txt",
                       "/fsanitize=address"):
            with self.subTest(option=option), self.assertRaisesRegex(ValueError, "weakening sanitizer"):
                coverage.sanitizer_vector(records(FLAGS + [option]), SOURCES)

    def test_missing_empty_and_duplicate_target_records(self):
        with self.assertRaisesRegex(ValueError, "records missing"):
            coverage.sanitizer_vector(records()[:-1], SOURCES)
        with self.assertRaisesRegex(ValueError, "records missing"):
            coverage.sanitizer_vector([], set())
        commands = records()
        commands.append({"file": commands[0]["file"], "arguments": ["clang-cl", *FLAGS[:-1]]})
        with self.assertRaisesRegex(ValueError, "incomplete Debug"):
            coverage.sanitizer_vector(commands, SOURCES)

    def test_non_target_flags_cannot_supply_a_missing_target_vector(self):
        commands = [{"file": str(ROOT / "src/ui/win32/note_pane.c"), "arguments": FLAGS}]
        with self.assertRaisesRegex(ValueError, "records missing"):
            coverage.sanitizer_vector(commands, SOURCES)

    def test_counterexample_requires_nonzero_and_its_own_binding_diagnostic(self):
        source = ROOT / "binding.c"
        diagnostic = "binding.c:3: runtime error: _Nonnull binding to null pointer of type char*"
        good = subprocess.CompletedProcess([], 1, "", diagnostic)
        coverage.check_binding_failure(good, source)
        for code, output in ((0, diagnostic), (1, "link failed"), (1, "other.c: runtime error: overflow")):
            with self.subTest(code=code, output=output), self.assertRaisesRegex(ValueError, "counterexample"):
                coverage.check_binding_failure(subprocess.CompletedProcess([], code, "", output), source)


if __name__ == "__main__":
    unittest.main()
