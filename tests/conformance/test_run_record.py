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

    def invoke(self, command, inputs=(), artifacts=(), run_dir=None, env=None, input_list=None):
        run_dir = self.base / "attempt" if run_dir is None else run_dir
        arguments = [sys.executable, "-B", str(TOOL), "--run-dir", str(run_dir), "--cwd", str(self.base)]
        for path in inputs:
            arguments += ["--input", str(path)]
        if input_list is not None:
            arguments += ["--input-list", str(input_list)]
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

    def test_input_list_replaces_long_outer_argv_and_preserves_order_and_child_args(self):
        source_folder = self.base / "inputs with spaces 雪"
        source_folder.mkdir()
        listed = []
        for index in range(739):
            source = source_folder / f"{index:04d}.md"
            source.write_bytes(f"input-{index}".encode("ascii"))
            listed.append(source)
        direct = self.base / "direct input 日本語.md"
        direct.write_bytes(b"direct\x00input")
        input_list = self.base / "input list 雪.json"
        list_bytes = json.dumps([str(path) for path in listed], ensure_ascii=False).encode("utf-8")
        input_list.write_bytes(list_bytes)
        child_arguments = ["@foo", "--input-list", "--args-file", ""]
        program = ("import json,sys; "
                   "sys.stdout.buffer.write(json.dumps(sys.argv[1:],ensure_ascii=False).encode('utf-8'))")
        command = [sys.executable, "-B", "-c", program, *child_arguments]
        completed, run_dir = self.invoke(command, inputs=[direct], input_list=input_list)
        self.assertEqual(0, completed.returncode)

        expanded_inputs = [input_list, direct, *listed]
        expanded_cli = [sys.executable, "-B", str(TOOL), "--run-dir", str(run_dir),
                        "--cwd", str(self.base)]
        for path in expanded_inputs:
            expanded_cli += ["--input", str(path)]
        expanded_cli += ["--", *command]
        expanded_units = len(subprocess.list2cmdline(expanded_cli).encode("utf-16-le")) // 2 + 1
        self.assertGreater(expanded_units, 32767)

        run_manifest = json.loads((run_dir / "run.json").read_text(encoding="utf-8"))
        self.assertEqual([str(path) for path in expanded_inputs], run_manifest["inputs"])
        self.assertEqual(command, run_manifest["argv"])
        self.assertEqual(json.dumps(child_arguments, ensure_ascii=False).encode("utf-8"),
                         (run_dir / "stdout.log").read_bytes())
        result = self.result(run_dir)
        self.assertEqual(len(expanded_inputs), len(result["inputs"]))
        for source, entry in zip(expanded_inputs, result["inputs"]):
            expected = list_bytes if source == input_list else source.read_bytes()
            self.assertEqual(expected, (run_dir / entry["snapshot"]).read_bytes())
            self.assertEqual(hashlib.sha256(expected).hexdigest(), entry["sha256"])
            self.assertEqual("unchanged", entry["after"]["status"])

    def test_input_list_accepts_bom_and_empty_array_and_rejects_invalid_values(self):
        command = [sys.executable, "-B", "-c", "import pathlib,sys; pathlib.Path(sys.argv[1]).touch()",
                   str(self.base / "child-started")]
        valid_contents = (b"[]", b"\xef\xbb\xbf[]")
        for index, content in enumerate(valid_contents):
            with self.subTest(valid_index=index):
                input_list = self.base / f"valid-{index}.json"
                input_list.write_bytes(content)
                completed, run_dir = self.invoke(command, input_list=input_list,
                                                  run_dir=self.base / f"valid-run-{index}")
                self.assertEqual(0, completed.returncode)
                result = self.result(run_dir)
                self.assertEqual([str(input_list)], [item["source"] for item in result["inputs"]])
                self.assertEqual(content, (run_dir / result["inputs"][0]["snapshot"]).read_bytes())

        invalid_contents = (b"{}", b"[1]", b'["relative.md"]', b'["\\u0000bad"]',
                            b"[", b"\xff")
        for index, content in enumerate(invalid_contents):
            with self.subTest(invalid_index=index):
                input_list = self.base / f"invalid-{index}.json"
                input_list.write_bytes(content)
                sentinel = self.base / f"invalid-child-{index}"
                child = [sys.executable, "-B", "-c",
                         "import pathlib,sys; pathlib.Path(sys.argv[1]).touch()", str(sentinel)]
                run_dir = self.base / f"invalid-run-{index}"
                completed, _ = self.invoke(child, input_list=input_list, run_dir=run_dir)
                self.assertEqual(recorder.RECORDING_FAILURE, completed.returncode)
                self.assertFalse(run_dir.exists())
                self.assertFalse(sentinel.exists())

        for name, make_directory in (("missing", False), ("directory", True)):
            with self.subTest(unreadable_list=name):
                input_list = self.base / f"{name}-input-list.json"
                if make_directory:
                    input_list.mkdir()
                sentinel = self.base / f"unreadable-child-{name}"
                child = [sys.executable, "-B", "-c",
                         "import pathlib,sys; pathlib.Path(sys.argv[1]).touch()", str(sentinel)]
                run_dir = self.base / f"unreadable-run-{name}"
                completed, _ = self.invoke(child, input_list=input_list, run_dir=run_dir)
                self.assertEqual(recorder.RECORDING_FAILURE, completed.returncode)
                self.assertFalse(run_dir.exists())
                self.assertFalse(sentinel.exists())

        relative_run = self.base / "relative-list-run"
        relative, _ = self.invoke(command, input_list=Path("relative.json"), run_dir=relative_run)
        self.assertEqual(recorder.RECORDING_FAILURE, relative.returncode)
        self.assertFalse(relative_run.exists())

        nul_run = self.base / "nul-list-run"
        diagnostic = io.StringIO()
        with contextlib.redirect_stderr(diagnostic):
            self.assertEqual(recorder.RECORDING_FAILURE,
                             recorder.main(["--run-dir", str(nul_run), "--cwd", str(self.base),
                                            "--input-list", "\0", "--", *command]))
        self.assertFalse(nul_run.exists())

        duplicate_run = self.base / "duplicate-list-run"
        duplicate = subprocess.run(
            [sys.executable, "-B", str(TOOL), "--run-dir", str(duplicate_run), "--cwd", str(self.base),
             "--input-list", str(self.base / "valid-0.json"), "--input-list",
             str(self.base / "valid-0.json"), "--", *command],
            capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        self.assertNotEqual(0, duplicate.returncode)
        self.assertFalse(duplicate_run.exists())

    def test_input_list_captured_bytes_survive_change_or_disappearance_before_snapshot(self):
        for action in ("change", "remove"):
            with self.subTest(action=action):
                source = self.base / f"listed-source-{action}.md"
                source.write_bytes(b"listed source")
                input_list = self.base / f"input-list-{action}.json"
                captured = json.dumps([str(source)]).encode("utf-8")
                input_list.write_bytes(captured)
                sentinel = self.base / f"snapshot-child-{action}"
                command = [sys.executable, "-B", "-c",
                           "import pathlib,sys; pathlib.Path(sys.argv[1]).touch()", str(sentinel)]
                run_dir = self.base / f"snapshot-run-{action}"
                arguments = ["--run-dir", str(run_dir), "--cwd", str(self.base),
                             "--input-list", str(input_list), "--", *command]
                original_record_run = recorder.record_run

                def mutate_after_validation(options):
                    if action == "change":
                        input_list.write_bytes(b"[]")
                    else:
                        input_list.unlink()
                    return original_record_run(options)

                diagnostic = io.StringIO()
                with mock.patch.object(recorder, "record_run", side_effect=mutate_after_validation), \
                        contextlib.redirect_stderr(diagnostic):
                    self.assertEqual(recorder.RECORDING_FAILURE, recorder.main(arguments))
                self.assertTrue(run_dir.is_dir())
                self.assertFalse(sentinel.exists())
                result = self.result(run_dir)
                entry = result["inputs"][0]
                self.assertEqual(str(input_list), entry["source"])
                self.assertEqual(hashlib.sha256(captured).hexdigest(), entry["sha256"])
                self.assertEqual(captured, (run_dir / entry["snapshot"]).read_bytes())
                self.assertIsNone(result["childExitCode"])
                self.assertEqual(recorder.RECORDING_FAILURE, result["wrapperExitCode"])
                if action == "change":
                    self.assertEqual("changedDuringCopy", entry["status"])
                    self.assertEqual("changed", entry["after"]["status"])
                else:
                    self.assertEqual("error", entry["status"])
                    self.assertEqual("missing", entry["after"]["status"])

    def test_input_list_validation_failure_creates_no_run_directory(self):
        input_list = self.base / "becomes-directory.json"
        input_list.write_text("[]", encoding="utf-8")
        run_dir = self.base / "directory-validation-run"
        command = [sys.executable, "-B", "-c", "pass"]
        original_validate = recorder.validate_paths

        def replace_with_directory(options):
            input_list.unlink()
            input_list.mkdir()
            return original_validate(options)

        diagnostic = io.StringIO()
        with mock.patch.object(recorder, "validate_paths", side_effect=replace_with_directory), \
                contextlib.redirect_stderr(diagnostic):
            self.assertEqual(recorder.RECORDING_FAILURE,
                             recorder.main(["--run-dir", str(run_dir), "--cwd", str(self.base),
                                            "--input-list", str(input_list), "--", *command]))
        self.assertFalse(run_dir.exists())

    def test_input_list_child_change_preserves_snapshot_exit_and_run_dir(self):
        listed = self.base / "child-listed.md"
        listed.write_bytes(b"listed bytes")
        input_list = self.base / "child-inputs.json"
        original = json.dumps([str(listed)]).encode("utf-8")
        input_list.write_bytes(original)
        sentinel = self.base / "input-list-child-sentinel"
        for action in ("change", "remove"):
            with self.subTest(action=action):
                input_list.write_bytes(original)
                if sentinel.exists():
                    sentinel.unlink()
                mutation = ("pathlib.Path(sys.argv[1]).write_bytes(b'[]')" if action == "change"
                            else "pathlib.Path(sys.argv[1]).unlink()")
                program = (f"import pathlib,sys; {mutation}; "
                           "pathlib.Path(sys.argv[2]).write_text('started'); sys.exit(17)")
                command = [sys.executable, "-B", "-c", program, str(input_list), str(sentinel)]
                run_dir = self.base / f"child-mutation-run-{action}"
                completed, _ = self.invoke(command, input_list=input_list, run_dir=run_dir)
                self.assertEqual(recorder.RECORDING_FAILURE, completed.returncode)
                result = self.result(run_dir)
                self.assertEqual((17, recorder.RECORDING_FAILURE),
                                 (result["childExitCode"], result["wrapperExitCode"]))
                self.assertEqual(original, (run_dir / result["inputs"][0]["snapshot"]).read_bytes())
                self.assertEqual("started", sentinel.read_text(encoding="utf-8"))
                self.assertEqual("changed" if action == "change" else "missing",
                                 result["inputs"][0]["after"]["status"])

                # Restore a valid list so the second invocation reaches the existing run-dir refusal.
                input_list.write_bytes(original)
                before = {path.relative_to(run_dir): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in run_dir.rglob("*") if path.is_file()}
                replay = ("import pathlib,sys; pathlib.Path(sys.argv[1]).write_bytes(b'[]'); "
                          "pathlib.Path(sys.argv[2]).write_text('reexecuted')")
                replay_command = [sys.executable, "-B", "-c", replay, str(input_list), str(sentinel)]
                second, _ = self.invoke(replay_command, input_list=input_list, run_dir=run_dir)
                self.assertEqual(recorder.RECORDING_FAILURE, second.returncode)
                self.assertEqual("started", sentinel.read_text(encoding="utf-8"))
                self.assertEqual(original, input_list.read_bytes())
                after = {path.relative_to(run_dir): hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in run_dir.rglob("*") if path.is_file()}
                self.assertEqual(before, after)


if __name__ == "__main__":
    unittest.main()
