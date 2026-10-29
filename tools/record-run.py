"""Record one argv invocation without replacing an earlier attempt (Issue #424)."""

import argparse
import datetime
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys

RECORDING_FAILURE = 125
CHUNK_BYTES = 1024 * 1024


def utc_now():
    return datetime.datetime.now(datetime.UTC).isoformat().replace("+00:00", "Z")


def write_json(path, value):
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def digest_file(path):
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while chunk := stream.read(CHUNK_BYTES):
            digest.update(chunk)
            size += len(chunk)
    return {"sha256": digest.hexdigest(), "sizeBytes": size}


def snapshot(source, destination, run_dir, errors, stage, captured_bytes=None):
    entry = {"source": str(source), "snapshot": destination.relative_to(run_dir).as_posix()}
    try:
        if captured_bytes is None and not source.is_file():
            raise OSError(f"not an existing file: {source}")
        digest = hashlib.sha256()
        size = 0
        original = io.BytesIO(captured_bytes) if captured_bytes is not None else source.open("rb")
        with original, destination.open("xb") as copied:
            while chunk := original.read(CHUNK_BYTES):
                copied.write(chunk)
                digest.update(chunk)
                size += len(chunk)
        entry.update(sha256=digest.hexdigest(), sizeBytes=size)
        observed = digest_file(source)
        if observed != {"sha256": entry["sha256"], "sizeBytes": size}:
            entry["status"] = "changedDuringCopy"
            entry["sourceAfterCopy"] = observed
            errors.append({"stage": stage, "message": f"source changed during snapshot: {source}"})
        else:
            entry["status"] = "copied"
    except (OSError, ValueError) as error:
        entry["status"] = "error"
        entry["error"] = str(error)
        errors.append({"stage": stage, "message": str(error)})
    return entry


def compare_inputs(entries, errors):
    for entry in entries:
        source = Path(entry["source"])
        try:
            observed = digest_file(source)
            expected = {"sha256": entry.get("sha256"), "sizeBytes": entry.get("sizeBytes")}
            status = "unchanged" if observed == expected else "changed"
            entry["after"] = {"status": status, **observed}
            if status != "unchanged":
                errors.append({"stage": "input_after", "message": f"input changed: {source}"})
        except FileNotFoundError as error:
            entry["after"] = {"status": "missing"}
            errors.append({"stage": "input_after", "message": str(error)})
        except (OSError, ValueError) as error:
            entry["after"] = {"status": "error", "error": str(error)}
            errors.append({"stage": "input_after", "message": str(error)})


def parse_cli(arguments):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-dir", required=True, type=Path)
    parser.add_argument("--cwd", required=True, type=Path)
    parser.add_argument("--input", action="append", type=Path, default=[], metavar="ABS_FILE")
    parser.add_argument("--input-list", action="append", metavar="ABS_JSON")
    parser.add_argument("--artifact", action="append", type=Path, default=[], metavar="ABS_FILE")
    if "--" not in arguments:
        if arguments in (["--help"], ["-h"]):
            parser.parse_args(arguments)
        parser.error("use -- followed by the executable and its argv")
    separator = arguments.index("--")
    options = parser.parse_args(arguments[:separator])
    if options.input_list is not None and len(options.input_list) > 1:
        parser.error("--input-list may be specified only once")
    options.input_list = options.input_list[0] if options.input_list else None
    options.command = arguments[separator + 1:]
    if not options.command or not options.command[0] or any("\0" in item for item in options.command):
        parser.error("a nonempty executable argv without NUL is required")
    return options


def resolve_input_list(options):
    options.captured_input_list_bytes = None
    if options.input_list is None:
        return
    input_list_value = options.input_list
    if "\0" in input_list_value:
        raise ValueError("input-list path must not contain NUL")
    input_list_path = Path(input_list_value)
    if not input_list_path.is_absolute():
        raise ValueError(f"input-list path must be absolute: {input_list_value}")
    captured_bytes = input_list_path.read_bytes()
    input_list = json.loads(captured_bytes.decode("utf-8-sig"))
    if not isinstance(input_list, list):
        raise ValueError("input-list JSON must be an array of absolute file paths")
    resolved_paths = []
    for index, value in enumerate(input_list):
        if not isinstance(value, str):
            raise ValueError(f"input-list item {index} must be a string path")
        if "\0" in value:
            raise ValueError(f"input-list item {index} must not contain NUL")
        path = Path(value)
        if not path.is_absolute():
            raise ValueError(f"input-list item {index} must be absolute: {value}")
        resolved_paths.append(path)
    options.input = [input_list_path, *options.input, *resolved_paths]
    options.captured_input_list_bytes = captured_bytes


def validate_paths(options):
    paths = [options.run_dir, options.cwd, *options.input, *options.artifact]
    if any(not path.is_absolute() or "\0" in str(path) for path in paths):
        raise ValueError("run-dir, cwd, inputs and artifacts must be absolute paths without NUL")
    if not options.cwd.is_dir():
        raise ValueError(f"cwd is not an existing directory: {options.cwd}")
    if not options.run_dir.parent.is_dir() or options.run_dir.parent.is_symlink():
        raise ValueError(f"run-dir parent must be an existing directory: {options.run_dir.parent}")
    if options.run_dir.exists() or options.run_dir.is_symlink():
        raise ValueError(f"run-dir already exists: {options.run_dir}")
    for path in [*options.input, *options.artifact]:
        if path.exists() and not path.is_file():
            raise ValueError(f"input/artifact is not a file: {path}")


def record_run(options):
    run_dir = options.run_dir
    # The atomic mkdir is the ownership boundary, including a concurrent second invocation.
    run_dir.mkdir(exist_ok=False)
    result = {"schemaVersion": 1, "childExitCode": None, "inputs": [], "artifacts": [],
              "recordingErrors": []}
    errors = result["recordingErrors"]
    try:
        write_json(run_dir / "run.json", {
            "schemaVersion": 1, "startedAtUtc": utc_now(), "cwd": str(options.cwd),
            "argv": options.command, "inputs": [str(path) for path in options.input],
            "artifacts": [str(path) for path in options.artifact],
        })
        (run_dir / "inputs").mkdir()
        (run_dir / "artifacts").mkdir()
        for index, source in enumerate(options.input, 1):
            result["inputs"].append(snapshot(source, run_dir / "inputs" / f"{index:04d}.bin",
                                             run_dir, errors, "input_snapshot",
                                             options.captured_input_list_bytes if index == 1 else None))
        if not errors:
            with (run_dir / "stdout.log").open("xb") as output, (run_dir / "stderr.log").open("xb") as error:
                try:
                    child = subprocess.run(options.command, shell=False, cwd=options.cwd,
                                           stdin=subprocess.DEVNULL, stdout=output, stderr=error,
                                           creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
                    result["childExitCode"] = child.returncode
                except (OSError, ValueError) as failure:
                    errors.append({"stage": "child_launch", "message": str(failure)})
            for index, source in enumerate(options.artifact, 1):
                result["artifacts"].append(snapshot(source, run_dir / "artifacts" / f"{index:04d}.bin",
                                                    run_dir, errors, "artifact_snapshot"))
        compare_inputs(result["inputs"], errors)
    except (OSError, ValueError) as failure:
        errors.append({"stage": "recording", "message": str(failure)})
    wrapper_exit = RECORDING_FAILURE if errors else result["childExitCode"]
    result.update(wrapperExitCode=wrapper_exit, finishedAtUtc=utc_now())
    try:
        write_json(run_dir / "result.json", result)
    except (OSError, ValueError) as failure:
        print(f"record-run: result.json could not be saved: {failure}", file=sys.stderr)
        return RECORDING_FAILURE
    for error in errors:
        print(f"record-run: {error['stage']}: {error['message']}", file=sys.stderr)
    return wrapper_exit


def main(arguments=None):
    options = parse_cli(sys.argv[1:] if arguments is None else arguments)
    try:
        resolve_input_list(options)
        validate_paths(options)
        return record_run(options)
    except (OSError, ValueError) as failure:
        print(f"record-run: preparation/recording failed: {failure}", file=sys.stderr)
        return RECORDING_FAILURE


if __name__ == "__main__":
    exit_code = main()
    # CPython SystemExit accepts a signed C int; preserve a Windows DWORD's bits.
    if os.name == "nt" and exit_code > 0x7FFFFFFF:
        exit_code -= 0x100000000
    raise SystemExit(exit_code)
