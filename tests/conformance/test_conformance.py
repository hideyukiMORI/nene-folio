import datetime
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "eng"))
import conformance as cnf

spec = importlib.util.spec_from_file_location("git_conventions", ROOT / "eng/git-conventions.py")
git_conventions = importlib.util.module_from_spec(spec)
spec.loader.exec_module(git_conventions)
RULES = json.loads((ROOT / "eng/conformance-rules.json").read_text(encoding="utf-8"))
TODAY = datetime.date(2026, 9, 9)


class SourceChecks(unittest.TestCase):
    def ids(self, source, path="src/core/note_id.h", waivers=None):
        return {f.rule for f in cnf.source_checks(path, source, RULES, waivers or {})}

    def test_cnf001_positive(self):
        self.assertFalse(self.ids('struct note_id; // struct note_helper {};\n'))

    def test_cnf001_negative_snake(self):
        self.assertIn("CNF-001", self.ids("struct note_helper { int v; };", "src/core/note_helper.h"))

    def test_cnf001_negative_camel(self):
        self.assertIn("CNF-001", self.ids("typedef struct { int v; } NoteManager;", "src/core/NoteManager.h"))

    def test_cnf001_module(self):
        self.assertIn("CNF-001", self.ids("", "src/core/utils/note_id.h"))

    def test_cnf002_positive(self):
        self.assertNotIn("CNF-002", self.ids("struct note_id { int value; };"))

    def test_cnf002_opaque_positive(self):
        self.assertNotIn("CNF-002", self.ids("struct note_id;\nstruct note_id *note_id_parse(const char *text);"))

    def test_cnf002_negative_two_types(self):
        self.assertIn("CNF-002", self.ids("struct note_id { int v; };\nenum note_kind { NOTE_A };"))

    def test_cnf002_filename(self):
        self.assertIn("CNF-002", self.ids("struct category { int v; };"))

    def test_cnf002_nested_positive(self):
        self.assertNotIn("CNF-002", self.ids("struct note_id { struct { int v; } inner; };"))

    def test_cnf002_function_prefix_positive(self):
        source = "#include \"note_id.h\"\nstatic int count(void) { return 1; }\nint note_id_value(void)\n{\n    return count();\n}\n"
        self.assertNotIn("CNF-002", self.ids(source, "src/core/note_id.c"))

    def test_cnf002_function_prefix_negative(self):
        source = "int read_value(void)\n{\n    return 1;\n}\n"
        self.assertIn("CNF-002", self.ids(source, "src/core/note_id.c"))

    def test_cnf002_app_main_allowed(self):
        self.assertNotIn("CNF-002", self.ids("int wWinMain(void)\n{\n    return 0;\n}\n", "src/app/main.c"))

    def test_cnf003_quoted_pragma_positive(self):
        self.assertNotIn("CNF-003", self.ids('const char *text = "#pragma clang diagnostic ignored";'))

    def test_cnf003_positive(self):
        waivers = {"WVR-0001": {"scope": "src/core/note_id.h#note_id", "rule": "C-003"}}
        self.assertNotIn("CNF-003", self.ids("// Waiver: WVR-0001\n// NOLINTNEXTLINE(readability-function-size)\nstruct note_id;", waivers=waivers))

    def test_cnf003_negative(self):
        self.assertIn("CNF-003", self.ids("// NOLINTNEXTLINE(check)\nstruct note_id;"))

    def test_cnf003_broad(self):
        for source in ["#pragma clang diagnostic ignored \"-Wswitch-enum\"", "#pragma warning(disable: 4062)", "_Pragma(\"clang diagnostic push\")", "// NOLINT", "// NOLINTBEGIN(check)"]:
            with self.subTest(source=source):
                self.assertIn("CNF-003", self.ids(source))

    def test_cnf003_wrong_scope(self):
        waivers = {"WVR-0001": {"scope": "src/core/other.h#other", "rule": "C-003"}}
        self.assertIn("CNF-003", self.ids("// Waiver: WVR-0001\n// NOLINTNEXTLINE(check)", waivers=waivers))

    def test_arc007_negative(self):
        for path in ["src/core/note_id.c", "tests/build/probe.c", "src/ui/win32/drawer.c"]:
            with self.subTest(path=path):
                self.assertIn("ARC-007", self.ids("long now = (long)time(0);", path))

    def test_arc007_adapter_positive(self):
        self.assertNotIn("ARC-007", self.ids("long now = (long)time(0);", "src/adapters/win32/clock.c"))

    def test_arc007_literals_positive(self):
        self.assertNotIn("ARC-007", self.ids('const char *s = "time(0)"; // getenv()'))

    def test_arc003_negative(self):
        for header in ["<windows.h>", "<stdio.h>", "<time.h>"]:
            with self.subTest(header=header):
                self.assertIn("ARC-003", self.ids(f"#include {header}", "src/core/note_id.c"))

    def test_arc003_adapter_positive(self):
        self.assertNotIn("ARC-003", self.ids("#include <windows.h>", "src/adapters/win32/file_store.c"))


class RepositoryChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write(self, path, text):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text, encoding="utf-8")

    def paths(self):
        return [p.relative_to(self.root) for p in self.root.rglob("*") if p.is_file()]

    def seed_waiver(self, expires="2026-09-10"):
        self.write("src/core/note_id.h", "struct note_id;")
        self.write("docs/waivers/README.md", "[WVR-0001](WVR-0001-note-id.md)")
        self.write("docs/waivers/WVR-0001-note-id.md", f"- Status: active\n- Rule: C-003\n- Issue: #1\n- Owner: hide\n- Created: 2026-09-01\n- Expires: {expires}\n- Scope: src/core/note_id.h#note_id\n")

    def test_cnf004_positive(self):
        self.seed_waiver()
        errors, valid = cnf.waiver_checks(self.root, self.paths(), TODAY)
        self.assertEqual([], errors)
        self.assertIn("WVR-0001", valid)

    def test_cnf004_expired(self):
        self.seed_waiver("2026-09-08")
        errors, valid = cnf.waiver_checks(self.root, self.paths(), TODAY)
        self.assertIn("CNF-004", {f.rule for f in errors})
        self.assertNotIn("WVR-0001", valid)

    def test_cnf004_expiry_day_valid(self):
        self.seed_waiver("2026-09-09")
        self.assertFalse(cnf.waiver_checks(self.root, self.paths(), TODAY)[0])

    def test_cnf004_missing_field(self):
        self.seed_waiver()
        self.write("docs/waivers/WVR-0001-note-id.md", "- Status: active")
        self.assertTrue(cnf.waiver_checks(self.root, self.paths(), TODAY)[0])

    def test_cnf004_stale_index(self):
        self.write("docs/waivers/README.md", "WVR-0001")
        self.assertTrue(cnf.waiver_checks(self.root, self.paths(), TODAY)[0])

    def test_cnf004_bad_scope_is_not_valid(self):
        self.seed_waiver()
        path = self.root / "docs/waivers/WVR-0001-note-id.md"
        path.write_text(path.read_text(encoding="utf-8").replace("src/core/note_id.h#note_id", "../outside.h#note_id"), encoding="utf-8")
        errors, valid = cnf.waiver_checks(self.root, self.paths(), TODAY)
        self.assertTrue(errors)
        self.assertNotIn("WVR-0001", valid)

    def seed_config(self):
        self.write("eng/config-bindings.json", '{".clang-tidy": ["CMakeLists.txt"]}')
        self.write(".clang-tidy", "WarningsAsErrors: '*'\n")
        self.write("CMakeLists.txt", '# read .clang-tidy\n')

    def config_ids(self):
        return {f.rule for f in cnf.configuration_checks(self.root, self.paths(), RULES)}

    def test_cnf005_positive(self):
        self.seed_config()
        self.assertFalse(self.config_ids())

    def test_cnf005_filename(self):
        self.seed_config()
        self.write("baseline.json", "{}")
        self.assertIn("CNF-005", self.config_ids())

    def test_cnf005_option(self):
        self.seed_config()
        for option in ["add_compile_options(/WX-)", "add_compile_options(-Wno-error)", "add_compile_options(-w)"]:
            with self.subTest(option=option):
                self.write("eng/options.cmake", option)
                self.assertIn("CNF-005", self.config_ids())

    def test_cnf005_named_warning_switch_is_not_disabling(self):
        self.seed_config()
        self.write("eng/options.cmake", "add_compile_options(-Wno-switch-default -Wswitch-enum)")
        self.assertNotIn("CNF-005", self.config_ids())

    def test_cnf007_positive(self):
        self.seed_config()
        self.assertNotIn("CNF-007", self.config_ids())

    def test_cnf007_missing_binding(self):
        self.seed_config()
        self.write("CMakeLists.txt", "project(test)")
        self.assertIn("CNF-007", self.config_ids())

    def test_cnf007_extra_config(self):
        self.seed_config()
        self.write("src/.clang-tidy", "Checks: '*'")
        self.assertIn("CNF-007", self.config_ids())

    def test_cnf008_positive(self):
        self.seed_config()
        self.write("src/note_id.c", "// TODO #1: implement")
        self.assertNotIn("CNF-008", self.config_ids())

    def test_cnf008_negative(self):
        self.seed_config()
        self.write("src/note_id.c", "// FIXME: implement")
        self.assertIn("CNF-008", self.config_ids())

    def seed_docs(self):
        self.write("docs/QUALITY_GATES.md", "### CNF-006 — documents\n- 機械強制: **active**\n\n## 3. 強制マトリクス\n| CNF-006 | active | checker |\n\n## 4. Gates\n")
        self.write("docs/quality/gate-proofs.md", "| CNF-006 | test | passed |")

    def doc_errors(self):
        return cnf.document_checks(self.root, self.paths(), {"normativeFiles": ["docs/QUALITY_GATES.md"]})

    def test_cnf006_positive(self):
        self.seed_docs()
        self.assertFalse(self.doc_errors())

    def test_cnf006_undefined(self):
        self.seed_docs()
        self.write("README.md", "C-999")
        self.assertTrue(any("undefined" in f.detail for f in self.doc_errors()))

    def test_cnf006_placeholder(self):
        self.seed_docs()
        self.write("README.md", "{{TOOL}}")
        self.assertTrue(any("placeholder" in f.detail for f in self.doc_errors()))

    def test_cnf006_missing_proof(self):
        self.seed_docs()
        self.write("docs/quality/gate-proofs.md", "")
        self.assertTrue(any("proof" in f.detail for f in self.doc_errors()))

    def test_cnf006_duplicate(self):
        self.seed_docs()
        path = self.root / "docs/QUALITY_GATES.md"
        path.write_text(path.read_text(encoding="utf-8") + "\n### CNF-006 — duplicate\n- 機械強制: **active**\n", encoding="utf-8")
        self.assertTrue(any("duplicate" in f.detail for f in self.doc_errors()))

    def test_cnf006_status_mismatch(self):
        self.seed_docs()
        path = self.root / "docs/QUALITY_GATES.md"
        path.write_text(path.read_text(encoding="utf-8").replace("**active**", "**planned**"), encoding="utf-8")
        self.assertTrue(any("mismatch" in f.detail for f in self.doc_errors()))

    def test_arc002_graph_cycle(self):
        self.write("eng/architecture.json", json.dumps({"modules": {"core": {"path": "src/core", "dependencies": ["core"]}}, "runtimeDependencies": []}))
        self.assertIn("ARC-002", {f.rule for f in cnf.architecture_checks(self.root, self.paths(), None)})

    def test_arc002_relative_include(self):
        graph = json.loads((ROOT / "eng/architecture.json").read_text(encoding="utf-8"))
        self.write("eng/architecture.json", json.dumps(graph))
        self.write("src/core/note_id.h", '#include "../ui/win32/drawer.h"')
        self.write("src/ui/win32/drawer.h", "struct drawer;")
        self.assertIn("ARC-002", {f.rule for f in cnf.architecture_checks(self.root, self.paths(), None)})

    def seed_build_graph(self, include_unit):
        graph = json.loads((ROOT / "eng/architecture.json").read_text(encoding="utf-8"))
        self.write("eng/architecture.json", json.dumps(graph))
        self.write("tests/unit/sample.c", "int main(void) { return 0; }")
        reply = "build/.cmake/api/v1/reply/"
        self.write(reply + "index-1.json", json.dumps({"reply": {"codemodel-v2": {"jsonFile": "model.json"}}}))
        targets = [{"id": "unit", "name": "unit", "jsonFile": "unit.json"}] if include_unit else []
        self.write(reply + "model.json", json.dumps({"configurations": [{"targets": targets}]}))
        self.write(reply + "unit.json", json.dumps({"name": "unit", "sources": [{"path": "tests/unit/sample.c"}]}))

    def test_arc002_unit_translation_present(self):
        self.seed_build_graph(True)
        self.assertFalse(cnf.architecture_checks(self.root, self.paths(), self.root / "build"))

    def test_arc002_unit_translation_missing(self):
        self.seed_build_graph(False)
        findings = cnf.architecture_checks(self.root, self.paths(), self.root / "build")
        self.assertTrue(any(f.rule == "ARC-002" and "absent from the build" in f.detail for f in findings))


class GitChecks(unittest.TestCase):
    def test_valid(self):
        self.assertEqual([], git_conventions.validate("build: 検査を固定する (#1)"))

    def test_missing_issue(self):
        self.assertTrue(git_conventions.validate("build: 検査を固定する"))

    def test_english_description(self):
        self.assertTrue(git_conventions.validate("build: enforce checks (#1)"))

    def test_breaking_footer(self):
        self.assertTrue(git_conventions.validate("build!: 契約を変える (#1)"))
        self.assertFalse(git_conventions.validate("build!: 契約を変える (#1)\n\nBREAKING CHANGE: 契約を更新"))


if __name__ == "__main__":
    unittest.main()
