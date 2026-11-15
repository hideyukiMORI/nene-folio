"""Measure private native discovery with synthetic sources and an external clock.

Requires a built tags_native_tests.exe and its adjacent fonts directory. Each
profile gets a new runtime directory; repeated processes retain its exact notes.
The OS cache is not cleared. Times include pipe, UI and scheduling costs. Pings
are observed message acknowledgements, not hardware keyboard latency.
"""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import queue
import shutil
import statistics
import subprocess
import threading
import time


MESSAGE = 0x8000 + 197
PROFILES = {"small": (100, 4096), "many": (1000, 4096), "long": (32, 1024 * 1024)}
COMMANDS = {1: "freshBodyUnique", 2: "warmBodyAbsent", 3: "warmBodyCommon",
            4: "warmBodyUnique", 5: "freshNames"}


def emit(value: dict) -> None:
    print(json.dumps(value, ensure_ascii=False), flush=True)


class NativeApi:
    def __init__(self) -> None:
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.user = ctypes.WinDLL("user32", use_last_error=True)
        signatures = (
            (self.kernel, "OpenThread", (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD), wintypes.HANDLE),
            (self.kernel, "GetThreadTimes", (wintypes.HANDLE,) +
             (ctypes.POINTER(wintypes.FILETIME),) * 4, wintypes.BOOL),
            (self.kernel, "CloseHandle", (wintypes.HANDLE,), wintypes.BOOL),
            (self.kernel, "GetCurrentThreadId", (), wintypes.DWORD),
            (self.user, "PostThreadMessageW", (wintypes.DWORD, wintypes.UINT,
             wintypes.WPARAM, wintypes.LPARAM), wintypes.BOOL),
            (self.user, "GetThreadDesktop", (wintypes.DWORD,), wintypes.HANDLE),
            (self.user, "OpenDesktopW", (wintypes.LPCWSTR, wintypes.DWORD,
             wintypes.BOOL, wintypes.DWORD), wintypes.HANDLE),
            (self.user, "SetThreadDesktop", (wintypes.HANDLE,), wintypes.BOOL),
            (self.user, "CloseDesktop", (wintypes.HANDLE,), wintypes.BOOL),
        )
        for library, name, arguments, result in signatures:
            function = getattr(library, name)
            function.argtypes, function.restype = arguments, result

    def post(self, thread_id: int, command: int, identifier: int = 0) -> None:
        if not self.user.PostThreadMessageW(thread_id, MESSAGE, command, identifier):
            raise ctypes.WinError(ctypes.get_last_error())

    def cpu_ms(self, handle: int) -> float:
        created, exited, system, application = (wintypes.FILETIME() for _ in range(4))
        if not self.kernel.GetThreadTimes(handle, ctypes.byref(created), ctypes.byref(exited),
                                          ctypes.byref(system), ctypes.byref(application)):
            raise ctypes.WinError(ctypes.get_last_error())
        return sum((part.dwHighDateTime << 32) | part.dwLowDateTime
                   for part in (system, application)) / 10000


def read_events(stream, events: queue.Queue, raw_path: Path) -> None:
    with raw_path.open("x", encoding="utf-8") as raw:
        for line in stream:
            arrived = time.perf_counter()
            raw.write(line)
            raw.flush()
            if line.startswith("image/status: ") and line.strip().endswith(" checks passed"):
                event = {"event": "completion", "fixture": line.strip()}
            else:
                try:
                    event = json.loads(line)
                except json.JSONDecodeError:
                    event = {"event": "invalid", "line": line}
            events.put((event, arrived))
    events.put(({"event": "eof"}, time.perf_counter()))


def measure_command(api: NativeApi, events: queue.Queue, thread_id: int,
                    handle: int, command: int) -> dict:
    cpu_before = api.cpu_ms(handle)
    started = time.perf_counter()
    api.post(thread_id, command)
    phases: dict[str, float] = {}
    ping_ms: list[float] = []
    ping_id = 0
    pending = None
    next_ping = started
    result = None
    while result is None or pending is not None:
        now = time.perf_counter()
        if now - started > 120:
            raise TimeoutError(f"scale command {command} did not finish within 120 seconds")
        if result is None and pending is None and now >= next_ping:
            ping_id += 1
            pending = (ping_id, time.perf_counter())
            api.post(thread_id, 7, ping_id)
            next_ping = now + 0.02
        try:
            event, arrived = events.get(timeout=0.02)
        except queue.Empty:
            continue
        kind = event.get("event")
        if kind == "ping":
            if pending is None or event.get("id") != pending[0]:
                raise RuntimeError(f"unexpected ping: {event}")
            ping_ms.append((arrived - pending[1]) * 1000)
            pending = None
        elif kind in ("begin", "read", "matched", "result"):
            if event.get("command") != command or kind in phases:
                raise RuntimeError(f"unexpected measurement phase: {event}")
            phases[kind] = (arrived - started) * 1000
            if kind == "result":
                result = event
        else:
            raise RuntimeError(f"fixture failed during command {command}: {event}")
    if "begin" not in phases or "matched" not in phases or (command == 1 and "read" not in phases):
        raise RuntimeError(f"missing observable boundary: {phases}")
    assert result is not None
    return {"operation": COMMANDS[command], "postedToResultMs": phases["result"],
            "phaseArrivalMs": phases, "mainThreadCpuMs": api.cpu_ms(handle) - cpu_before,
            "pingSamplesMs": ping_ms, "pingMaxMs": max(ping_ms),
            "result": result, "includesPipeUiAndScheduling": True}


def run_sample(api: NativeApi, executable: Path, profile: str, sample: int) -> list[dict]:
    started = time.perf_counter()
    events: queue.Queue = queue.Queue()
    measurements = []
    with (executable.parent / f"sample-{sample}.stderr.log").open("x", encoding="utf-8") as errors:
        with subprocess.Popen([str(executable), f"--discovery-scale-{profile}"],
                              stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=errors,
                              text=True, encoding="utf-8", bufsize=1,
                              creationflags=subprocess.CREATE_NO_WINDOW) as child:
            assert child.stdout is not None
            reader = threading.Thread(target=read_events, args=(child.stdout, events,
                                      executable.parent / f"sample-{sample}.stdout.log"), daemon=True)
            reader.start()
            original = api.user.GetThreadDesktop(api.kernel.GetCurrentThreadId())
            desktop = handle = None
            try:
                ready, arrived = events.get(timeout=120)
                notes, source_bytes = PROFILES[profile]
                if (ready.get("event"), ready.get("notes"), ready.get("sourceBytes")) != (
                        "ready", notes, source_bytes):
                    raise RuntimeError(f"unexpected fixture startup: {ready}")
                thread_id = ready["thread"]
                handle = api.kernel.OpenThread(0x0040, False, thread_id)
                desktop = api.user.OpenDesktopW("NeNeFolioImageStatus520", 0, False, 0x0001 | 0x0080)
                if not handle or not desktop or not api.user.SetThreadDesktop(desktop):
                    raise ctypes.WinError(ctypes.get_last_error())
                emit({"phase": "setup", "profile": profile, "sample": sample,
                      "processStartToReadyMs": (arrived - started) * 1000, "coldOsCache": False,
                      "includesSeedingAndInitialDocument": True, "notes": notes,
                      "sourceBytesEach": source_bytes, "freshApplicationProcess": True})
                for command in COMMANDS:
                    measurement = {"phase": "measurement", "profile": profile, "sample": sample,
                                   **measure_command(api, events, thread_id, handle, command)}
                    emit(measurement)
                    measurements.append(measurement)
                api.post(thread_id, 6)
                completed, _ = events.get(timeout=30)
                if completed.get("event") != "completion":
                    raise RuntimeError(f"unexpected fixture completion: {completed}")
                code = child.wait(timeout=30)
                if code != 0:
                    raise RuntimeError(f"fixture exited with {code}")
                emit({"phase": "completion", "profile": profile, "sample": sample, **completed})
            finally:
                if child.poll() is None:
                    child.terminate()
                    child.wait(timeout=30)
                reader.join(timeout=5)
                if handle:
                    api.kernel.CloseHandle(handle)
                if desktop:
                    if not api.user.SetThreadDesktop(original):
                        raise ctypes.WinError(ctypes.get_last_error())
                    if not api.user.CloseDesktop(desktop):
                        raise ctypes.WinError(ctypes.get_last_error())
    return measurements


def source_snapshot(runtime: Path, profile: str) -> list[dict]:
    count, size = PROFILES[profile]
    sources = []
    for index in range(count):
        category = "schedule" if index % 2 == 0 else "target"
        path = runtime / "data" / category / f"note-{index:04}.md"
        data = path.read_bytes()
        tail = b"\ncommon-scale\nunique-scale\n" if index + 1 == count else b"\ncommon-scale\nordinary-end\n"
        if len(data) != size or not data.endswith(tail) or data.count(b"common-scale") != 1:
            raise RuntimeError(f"synthetic source differs from its declared shape: {path}")
        status = path.stat()
        sources.append({"path": path.relative_to(runtime).as_posix(), "bytes": len(data),
                        "sha256": hashlib.sha256(data).hexdigest(),
                        "createdNs": status.st_birthtime_ns, "modifiedNs": status.st_mtime_ns})
    if len(list((runtime / "data").glob("*/*.md"))) != count:
        raise RuntimeError("unexpected extra notes in the dedicated scale runtime")
    return sources


def summarize(measurements: list[dict]) -> None:
    for profile in PROFILES:
        for operation in COMMANDS.values():
            group = [row for row in measurements if row["profile"] == profile and
                     row["operation"] == operation]
            if not group:
                continue
            values = [row["postedToResultMs"] for row in group]
            emit({"phase": "summary", "profile": profile, "operation": operation,
                  "samples": len(values), "minMs": min(values), "medianMs": statistics.median(values),
                  "maxMs": max(values), "pingMaxMs": max(row["pingMaxMs"] for row in group),
                  "coldOsCache": False, "notAPerformanceGuarantee": True})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("runtime_root", type=Path)
    parser.add_argument("--profiles", nargs="+", choices=tuple(PROFILES), default=list(PROFILES))
    parser.add_argument("--samples", type=int, choices=range(1, 8), default=3)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    root = args.runtime_root
    if not root.is_absolute() or root.exists():
        raise ValueError("runtime_root must be a new absolute directory")
    if any(parent.is_symlink() or parent.is_junction() for parent in root.parents):
        raise ValueError("runtime_root may not pass through a link")
    root.mkdir()
    api = NativeApi()
    measurements = []
    for profile in dict.fromkeys(args.profiles):
        runtime = root / profile
        runtime.mkdir()
        copied = runtime / executable.name
        shutil.copy2(executable, copied)
        shutil.copytree(executable.parent / "fonts", runtime / "fonts")
        before = None
        for sample in range(args.samples):
            measurements.extend(run_sample(api, copied, profile, sample))
            after = source_snapshot(runtime, profile)
            if before is not None and before != after:
                raise RuntimeError(f"retained synthetic sources changed during {profile}/{sample}")
            before = after
            (runtime / f"sample-{sample}.sources.json").write_text(
                json.dumps(after, indent=2) + "\n", encoding="utf-8")
    summarize(measurements)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
