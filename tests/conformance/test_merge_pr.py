"""Observe the real merge command's external effects around Ready and failed CI (#769)."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/merge-pr.ps1"
PROBE = Path(__file__).with_name("merge_pr_probe.ps1")


def check(name, conclusion, *, started="9999-12-31T00:00:00Z", suite=2):
    return {"name": name, "status": "in_progress" if conclusion is None else "completed",
            "conclusion": conclusion, "suite": suite, "started": started}


def frame(conclusion, *, suite=2):
    return {"rows": [check("full-gate", conclusion, suite=suite),
                     check("check", conclusion, suite=suite)]}


class MergePrTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.shell = shutil.which("pwsh")
        if cls.shell is None:
            raise RuntimeError("PowerShell is required to verify the merge command")
        cls.test_root = Path(os.environ.get("NENEFOLIO_MERGE_TEST_ROOT", ROOT / "out/merge-pr-tests"))
        if not cls.test_root.is_absolute():
            raise ValueError("test root must be absolute")
        cls.test_root.mkdir(parents=True, exist_ok=True)

    def invoke(self, draft, frames, *, known_suites=(1,), suite_exit=0):
        # Keep raw logs and each scenario for the task owner's preservation and cleanup.
        directory = Path(tempfile.mkdtemp(prefix=self._testMethodName + "-", dir=self.test_root))
        scenario = directory / "scenario.json"
        calls = directory / "calls.jsonl"
        scenario.write_text(json.dumps({"draft": draft, "frames": frames,
                                        "knownSuites": known_suites, "suiteExit": suite_exit}),
                            encoding="utf-8")
        command = [self.shell, "-NoProfile", "-File", str(PROBE), "-Tool", str(TOOL),
                   "-Scenario", str(scenario), "-Calls", str(calls)]
        result = subprocess.run(command, cwd=directory, capture_output=True, timeout=20,
                                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        (directory / "stdout.log").write_bytes(result.stdout)
        (directory / "stderr.log").write_bytes(result.stderr)
        (directory / "process.json").write_text(
            json.dumps({"argv": command, "exitCode": result.returncode}), encoding="utf-8")
        recorded = [json.loads(line) for line in calls.read_text(encoding="utf-8-sig").splitlines()]
        return result, recorded

    def actions(self, recorded, program, prefix):
        return [row["arguments"] for row in recorded
                if row["program"] == program and row["arguments"][:len(prefix)] == prefix]

    def check_reads(self, recorded):
        return [args for args in self.actions(recorded, "gh", ["api"])
                if "/check-runs?" in args[1]]

    def expect_stopped(self, result, recorded, ready):
        self.assertEqual(1, result.returncode, result.stderr.decode("utf-8", errors="replace"))
        self.assertIn(b"checks failed:", result.stderr)
        self.assertEqual(ready, self.actions(recorded, "gh", ["pr", "ready"]))
        self.assertEqual([], self.actions(recorded, "gh", ["pr", "merge"]))
        self.assertEqual([], self.actions(recorded, "git", []))
        self.assertEqual([], self.actions(recorded, "gh", ["issue", "view"]))

    def expect_merged(self, result, recorded, ready):
        self.assertEqual(0, result.returncode, result.stderr.decode("utf-8", errors="replace"))
        self.assertEqual(b"", result.stderr)
        self.assertEqual(ready, self.actions(recorded, "gh", ["pr", "ready"]))
        self.assertEqual([["pr", "merge", "99", "--squash", "--subject",
                           "fix(tools): 確定した失敗で停止する (#123)"]],
                         self.actions(recorded, "gh", ["pr", "merge"]))
        self.assertEqual([["rev-parse", "--abbrev-ref", "HEAD"], ["pull", "--ff-only"]],
                         self.actions(recorded, "git", []))
        payload = json.loads(result.stdout.decode("utf-8-sig"))
        self.assertTrue(payload["merged"] and payload["mainSynced"])
        self.assertEqual([{"number": 123, "state": "CLOSED"}], payload["closedIssues"])

    def test_ready_failure_does_not_restart_ci(self):
        result, calls = self.invoke(True, [frame("failure")])
        self.expect_stopped(result, calls, [["pr", "ready", "99"]])
        self.assertEqual([], self.actions(calls, "sleep", []))

    def test_ready_cancellation_does_not_restart_ci(self):
        result, calls = self.invoke(True, [frame("cancelled")])
        self.expect_stopped(result, calls, [["pr", "ready", "99"]])
        self.assertEqual([], self.actions(calls, "sleep", []))

    def test_already_ready_ambiguous_failure_resynchronizes_only_once(self):
        result, calls = self.invoke(False, [frame("failure"), frame("failure", suite=3)],
                                    known_suites=(1, 2))
        self.expect_stopped(result, calls,
                            [["pr", "ready", "99", "--undo"], ["pr", "ready", "99"]])
        self.assertEqual(2, len(self.check_reads(calls)))

    def test_old_draft_failure_is_ignored_while_current_ci_finishes(self):
        stale = check("check", "failure", started="2000-01-01T00:00:00Z", suite=4)
        result, calls = self.invoke(True, [{"rows": [stale]},
                                          {"rows": [stale, check("full-gate", None)]},
                                          frame("success")])
        self.expect_merged(result, calls, [["pr", "ready", "99"]])
        self.assertEqual(3, len(self.check_reads(calls)))

    def test_known_draft_failure_inside_clock_tolerance_is_ignored(self):
        stale = frame("failure", suite=1)["rows"]
        result, calls = self.invoke(True, [{"rows": stale},
                                          {"rows": stale + frame(None)["rows"]},
                                          {"rows": stale + frame("success")["rows"]}])
        self.expect_merged(result, calls, [["pr", "ready", "99"]])
        self.assertEqual(3, len(self.check_reads(calls)))

    def test_known_success_cannot_hide_failure_of_new_run(self):
        stale = frame("success", suite=1)["rows"]
        result, calls = self.invoke(True, [{"rows": stale},
                                          {"rows": stale + frame(None)["rows"]},
                                          {"rows": stale + frame("failure")["rows"]}])
        self.expect_stopped(result, calls, [["pr", "ready", "99"]])
        self.assertEqual(3, len(self.check_reads(calls)))

    def test_suite_read_failure_prevents_ready_and_undo(self):
        for draft in (True, False):
            with self.subTest(draft=draft):
                result, calls = self.invoke(draft, [frame("failure")], suite_exit=1)
                self.assertEqual(1, result.returncode)
                self.assertIn(b"suite read failed", result.stderr)
                self.assertEqual([], self.actions(calls, "gh", ["pr", "ready"]))
                self.assertEqual([], self.actions(calls, "gh", ["pr", "merge"]))
                self.assertEqual([], self.actions(calls, "git", []))

    def test_already_ready_success_merges_without_restarting_ci(self):
        result, calls = self.invoke(False, [frame("success")])
        self.expect_merged(result, calls, [])
        self.assertEqual(1, len(self.check_reads(calls)))


if __name__ == "__main__":
    unittest.main()
