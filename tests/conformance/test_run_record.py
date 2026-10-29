"""Record actual child argv/bytes and preserve failed attempts (Issue #424)."""

import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/record-run.py"
spec = importlib.util.spec_from_file_location("record_run", TOOL)
recorder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recorder)


class RunRecordTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.test_root = Path(os.environ.get("NENEFOLIO_RUN_RECORD_TEST_ROOT", ROOT / "out/run-record-tests"))
        if not cls.test_root.is_absolute():
            raise ValueError("test root must be absolute")
        cls.test_root.mkdir(parents=True, exist_ok=True)

    def setUp(self):
        # Retain each case until the task owner archives/audits it; never use system Temp.
        self.base = Path(tempfile.mkdtemp(prefix=self._testMethodName + "-", dir=self.test_root))

    def invoke(self, command, inputs=(), artifacts=(), run_dir=None, env=None):
        run_dir = self.base / "attempt" if run_dir is None else run_dir
        arguments = [sys.executable, "-B", str(TOOL), "--run-dir", str(run_dir), "--cwd", str(self.base)]
        for path in inputs:
            arguments += ["--input", str(path)]
        for path in artifacts:
            arguments += ["--artifact", str(path)]
        result = subprocess.run([*arguments, "--", *command], capture_output=True, env=env,
                                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        return result, run_dir

    def result(self, run_dir):
        return json.loads((run_dir / "result.json").read_text(encoding="utf-8"))

    def test_argv_raw_logs_nonzero_and_inherited_environment(self):
        arguments = ["space value", "雪とemoji🙂", 'quote" & ; | $(literal)', ""]
        program = ("import json,os,sys; "
                   "os.write(1,b'\\xff\\x00'+json.dumps(sys.argv[1:],ensure_ascii=False).encode('utf-8')); "
                   "os.write(2,b'\\x80\\x00'+os.environ['NENEFOLIO_RECORD_CHILD_TOKEN'].encode()); "
                   "assert sys.stdin.buffer.read()==b''; sys.exit(17)")
        command = [sys.executable, "-B", "-c", program, *arguments]
        child_env = {**os.environ, "NENEFOLIO_RECORD_CHILD_TOKEN": "inherited-value-only"}
        completed, run_dir = self.invoke(command, env=child_env)
        self.assertEqual(17, completed.returncode)
        manifest = json.loads((run_dir / "run.json").read_text(encoding="utf-8"))
        self.assertEqual(command, manifest["argv"])
        self.assertNotIn("inherited-value-only", json.dumps(manifest))
        self.assertNotIn("env", manifest)
        self.assertEqual(b"\xff\x00" + json.dumps(arguments, ensure_ascii=False).encode("utf-8"),
                         (run_dir / "stdout.log").read_bytes())
        self.assertEqual(b"\x80\x00inherited-value-only", (run_dir / "stderr.log").read_bytes())
        result = self.result(run_dir)
        self.assertEqual((17, 17, []),
                         (result["childExitCode"], result["wrapperExitCode"], result["recordingErrors"]))

    @unittest.skipUnless(os.name == "nt", "Windows DWORD process exit status")
    def test_windows_high_bit_exit_code_preserves_bitpattern(self):
        status = 0xC0000005
        command = [sys.executable, "-B", "-c",
                   "import ctypes; ctypes.windll.kernel32.ExitProcess(0xC0000005)"]
        completed, run_dir = self.invoke(command)
        result = self.result(run_dir)
        self.assertEqual(status, result["childExitCode"])
        self.assertEqual(status, result["wrapperExitCode"])
        self.assertEqual(status, completed.returncode & 0xFFFFFFFF)

    def test_pre_inputs_and_post_artifacts_with_same_basename(self):
        inputs = []
        for name, content in (("one", b"first\x00"), ("two", "日本語".encode("utf-8"))):
            folder = self.base / name
            folder.mkdir()
            source = folder / "same.md"
            source.write_bytes(content)
            inputs.append(source)
        artifact = self.base / "built file.bin"
        artifact.write_bytes(b"old")
        command = [sys.executable, "-B", "-c", "import pathlib,sys; pathlib.Path(sys.argv[1]).write_bytes(b'post\\x00')", str(artifact)]
        completed, run_dir = self.invoke(command, inputs, [artifact])
        self.assertEqual(0, completed.returncode)
        result = self.result(run_dir)
        snapshots = [entry["snapshot"] for entry in result["inputs"]]
        self.assertEqual(2, len(set(snapshots)))
        for source, entry in zip(inputs, result["inputs"]):
            self.assertEqual(source.read_bytes(), (run_dir / entry["snapshot"]).read_bytes())
            self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), entry["sha256"])
            self.assertEqual("unchanged", entry["after"]["status"])
        recorded = result["artifacts"][0]
        self.assertEqual(b"post\x00", (run_dir / recorded["snapshot"]).read_bytes())
        self.assertEqual(hashlib.sha256(b"post\x00").hexdigest(), recorded["sha256"])

    def test_same_directory_refuses_reexecution_and_preserves_all_bytes(self):
        sentinel = self.base / "child-sentinel"
        command = [sys.executable, "-B", "-c", "import pathlib,sys; pathlib.Path(sys.argv[1]).write_text(sys.argv[2])", str(sentinel), "first"]
        first, run_dir = self.invoke(command)
        self.assertEqual(0, first.returncode)
        before = {path.relative_to(run_dir): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in run_dir.rglob("*") if path.is_file()}
        second, _ = self.invoke([*command[:-1], "second"], run_dir=run_dir)
        self.assertEqual(recorder.RECORDING_FAILURE, second.returncode)
        self.assertEqual("first", sentinel.read_text())
        after = {path.relative_to(run_dir): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in run_dir.rglob("*") if path.is_file()}
        self.assertEqual(before, after)

    def test_input_change_and_disappearance_are_recording_failures(self):
        for action in ("change", "remove"):
            with self.subTest(action=action):
                source = self.base / f"{action}.md"
                source.write_bytes(b"before")
                program = "import pathlib,sys; p=pathlib.Path(sys.argv[1]); " + (
                    "p.write_bytes(b'after')" if action == "change" else "p.unlink()")
                completed, run_dir = self.invoke([sys.executable, "-B", "-c", program, str(source)],
                                                  [source], run_dir=self.base / action)
                self.assertEqual(recorder.RECORDING_FAILURE, completed.returncode)
                result = self.result(run_dir)
                self.assertEqual(0, result["childExitCode"])
                self.assertEqual(b"before", (run_dir / result["inputs"][0]["snapshot"]).read_bytes())
                self.assertEqual("changed" if action == "change" else "missing",
                                 result["inputs"][0]["after"]["status"])

    def test_missing_input_prevents_child_and_launch_failure_is_separate(self):
        sentinel = self.base / "not-started"
        command = [sys.executable, "-B", "-c", "import pathlib,sys; pathlib.Path(sys.argv[1]).touch()", str(sentinel)]
        completed, run_dir = self.invoke(command, [self.base / "missing.md"])
        self.assertEqual(recorder.RECORDING_FAILURE, completed.returncode)
        self.assertFalse(sentinel.exists())
        self.assertIsNone(self.result(run_dir)["childExitCode"])
        failed, launch_dir = self.invoke([str(self.base / "not-an-executable.exe")],
                                         run_dir=self.base / "launch-failure")
        self.assertEqual(recorder.RECORDING_FAILURE, failed.returncode)
        result = self.result(launch_dir)
        self.assertIsNone(result["childExitCode"])
        self.assertEqual("child_launch", result["recordingErrors"][0]["stage"])

    def test_missing_artifact_preserves_actual_child_exit(self):
        completed, run_dir = self.invoke([sys.executable, "-B", "-c", "raise SystemExit(9)"],
                                         artifacts=[self.base / "missing-output.bin"])
        self.assertEqual(recorder.RECORDING_FAILURE, completed.returncode)
        result = self.result(run_dir)
        self.assertEqual(9, result["childExitCode"])
        self.assertEqual("error", result["artifacts"][0]["status"])
        self.assertEqual("artifact_snapshot", result["recordingErrors"][0]["stage"])

    def test_malformed_relative_and_empty_command_reject_before_start(self):
        cases = (["--unknown"], ["--input", "relative.md", "--", sys.executable], ["--"])
        for index, extra in enumerate(cases):
            with self.subTest(extra=extra):
                run_dir = self.base / str(index)
                completed = subprocess.run([sys.executable, "-B", str(TOOL), "--run-dir", str(run_dir),
                                            "--cwd", str(self.base), *extra], capture_output=True,
                                           creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
                self.assertNotEqual(0, completed.returncode)
                self.assertFalse(run_dir.exists())
        missing_parent = self.base / "absent" / "attempt"
        rejected, _ = self.invoke([sys.executable, "-B", "-c", "pass"], run_dir=missing_parent)
        self.assertEqual(recorder.RECORDING_FAILURE, rejected.returncode)
        self.assertFalse(missing_parent.exists())

    def test_copy_source_mismatch_keeps_snapshot_and_blocks_child(self):
        source = self.base / "source.md"
        source.write_bytes(b"copied bytes")
        sentinel = self.base / "not-started"
        run_dir = self.base / "attempt"
        arguments = ["--run-dir", str(run_dir), "--cwd", str(self.base), "--input", str(source),
                     "--", sys.executable, "-B", "-c", "import pathlib,sys; pathlib.Path(sys.argv[1]).touch()", str(sentinel)]
        with mock.patch.object(recorder, "digest_file", return_value={"sha256": "different", "sizeBytes": 99}):
            self.assertEqual(recorder.RECORDING_FAILURE, recorder.main(arguments))
        result = self.result(run_dir)
        self.assertEqual("changedDuringCopy", result["inputs"][0]["status"])
        self.assertFalse(sentinel.exists())
        self.assertIsNone(result["childExitCode"])
        self.assertEqual(b"copied bytes", (run_dir / result["inputs"][0]["snapshot"]).read_bytes())

    def test_result_write_failure_preserves_raw_and_partial_records(self):
        source = self.base / "input.md"
        source.write_bytes(b"before")
        run_dir = self.base / "attempt"
        arguments = ["--run-dir", str(run_dir), "--cwd", str(self.base), "--input", str(source),
                     "--", sys.executable, "-B", "-c", "import os; os.write(1,b'out\\x00'); os.write(2,b'\\xfferr')"]
        original_write = recorder.write_json
        attempted = []

        def fail_result(path, value):
            if path.name == "result.json":
                attempted.append(value["childExitCode"])
                raise OSError("simulated final record failure")
            original_write(path, value)

        diagnostic = io.StringIO()
        with mock.patch.object(recorder, "write_json", side_effect=fail_result), contextlib.redirect_stderr(diagnostic):
            self.assertEqual(recorder.RECORDING_FAILURE, recorder.main(arguments))
        self.assertEqual([0], attempted)
        self.assertTrue((run_dir / "run.json").is_file())
        self.assertFalse((run_dir / "result.json").exists())
        self.assertEqual(b"before", (run_dir / "inputs/0001.bin").read_bytes())
        self.assertEqual(b"out\x00", (run_dir / "stdout.log").read_bytes())
        self.assertEqual(b"\xfferr", (run_dir / "stderr.log").read_bytes())
        self.assertIn("result.json could not be saved", diagnostic.getvalue())


if __name__ == "__main__":
    unittest.main()
