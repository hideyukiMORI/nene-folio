"""Observe the private motion fixture's real message loop from outside the app.

The C fixture never reads a clock. All durations include pipe/UI/OS scheduling.
The driver joins the child's private desktop; the visible desktop never changes.
"""

from __future__ import annotations

import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import queue
import subprocess
import sys
import threading
import time


MESSAGE = 0x8000 + 197


def main() -> int:
    executable = Path(sys.argv[1]).resolve(strict=True)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    user = ctypes.WinDLL("user32", use_last_error=True)
    kernel.OpenThread.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    kernel.OpenThread.restype = wintypes.HANDLE
    kernel.GetThreadTimes.argtypes = (wintypes.HANDLE,) + (ctypes.POINTER(wintypes.FILETIME),) * 4
    kernel.GetThreadTimes.restype = wintypes.BOOL
    kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
    kernel.GetCurrentThreadId.restype = wintypes.DWORD
    user.PostThreadMessageW.argtypes = (wintypes.DWORD, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)
    user.PostThreadMessageW.restype = wintypes.BOOL
    user.GetThreadDesktop.argtypes = (wintypes.DWORD,)
    user.GetThreadDesktop.restype = wintypes.HANDLE
    user.OpenDesktopW.argtypes = (wintypes.LPCWSTR, wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    user.OpenDesktopW.restype = wintypes.HANDLE
    user.SetThreadDesktop.argtypes = (wintypes.HANDLE,)
    user.SetThreadDesktop.restype = wintypes.BOOL
    user.CloseDesktop.argtypes = (wintypes.HANDLE,)
    user.CloseDesktop.restype = wintypes.BOOL
    with subprocess.Popen([str(executable), "--motion-live"], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1,
                          creationflags=subprocess.CREATE_NO_WINDOW) as child:
        assert child.stdout is not None and child.stdin is not None
        lines: queue.Queue[str] = queue.Queue()

        def read_lines() -> None:
            for line in child.stdout:
                lines.put(line)
            lines.put("")

        threading.Thread(target=read_lines, daemon=True).start()
        delayed = 0

        def receive() -> dict:
            nonlocal delayed
            while True:
                raw = lines.get(timeout=60)
                if not raw:
                    raise RuntimeError(f"fixture exited early: {child.poll()}")
                event = json.loads(raw)
                if event.get("event") != "delay":
                    return event
                delayed += 1
                time.sleep(0.035)
                child.stdin.write("\n")
                child.stdin.flush()

        ready = receive()
        if ready.get("event") != "ready":
            raise RuntimeError(f"unexpected startup: {ready}")
        thread_id = ready["thread"]
        handle = kernel.OpenThread(0x0040, False, thread_id)
        if not handle:
            raise ctypes.WinError(ctypes.get_last_error())

        def command(number: int) -> tuple[dict, float]:
            started = time.perf_counter()
            if not user.PostThreadMessageW(thread_id, MESSAGE, number, 0):
                raise ctypes.WinError(ctypes.get_last_error())
            event = receive()
            elapsed = (time.perf_counter() - started) * 1000
            if event.get("event") != "result" or event.get("command") != number:
                raise RuntimeError(f"unexpected command result: {event}")
            return event, elapsed

        def thread_cpu_ms() -> float:
            created, exited, system, application = (wintypes.FILETIME() for _ in range(4))
            if not kernel.GetThreadTimes(handle, ctypes.byref(created), ctypes.byref(exited),
                                         ctypes.byref(system), ctypes.byref(application)):
                raise ctypes.WinError(ctypes.get_last_error())
            ticks = sum((value.dwHighDateTime << 32) | value.dwLowDateTime
                        for value in (system, application))
            return ticks / 10000

        original_desktop = user.GetThreadDesktop(kernel.GetCurrentThreadId())
        desktop = None
        try:
            desktop = user.OpenDesktopW("NeNeFolioImageStatus520", 0, False, 0x0001 | 0x0080)
            if not desktop or not user.SetThreadDesktop(desktop):
                raise ctypes.WinError(ctypes.get_last_error())
            time.sleep(1)
            before, _ = command(5)
            cpu_before = thread_cpu_ms()
            idle_started = time.perf_counter()
            time.sleep(20)
            idle_elapsed = (time.perf_counter() - idle_started) * 1000
            cpu_after = thread_cpu_ms()
            after, _ = command(5)
            if before != after:
                raise RuntimeError(f"static loop changed: {before} -> {after}")
            print(json.dumps({"phase": "static", "elapsedMs": idle_elapsed,
                              "mainThreadCpuMs": cpu_after - cpu_before,
                              "additionalMotionTicks": after["ticks"] - before["ticks"],
                              "realMessageLoop": True}), flush=True)
            for enabled in (False, True):
                command(4 if enabled else 3)
                for sample in range(7):
                    one, first_ms = command(1)
                    all_notes, second_ms = command(2)
                    if one["shown"] != 1 or all_notes["shown"] != 3:
                        raise RuntimeError(f"different query meaning: {one}, {all_notes}")
                    print(json.dumps({"phase": "warm", "motion": enabled, "sample": sample,
                                      "twoQueriesMs": first_ms + second_ms,
                                      "savedNotes": 3, "coldOsCache": False,
                                      "includesPipeUiAndScheduling": True}), flush=True)
            result, elapsed = command(7)
            if result["shown"] != 1 or delayed == 0:
                raise RuntimeError(f"slow delivery did not exercise matching: {result}, {delayed}")
            print(json.dumps({"phase": "slowDelivery", "elapsedMs": elapsed,
                              "delayedTicks": delayed, "delayPerTickMs": 35,
                              "sourceBytes": 256 * 1024 + 10, **result}), flush=True)
            if not user.PostThreadMessageW(thread_id, MESSAGE, 6, 0):
                raise ctypes.WinError(ctypes.get_last_error())
            final = lines.get(timeout=30).strip()
            if not final.startswith("image/status: ") or not final.endswith(" checks passed"):
                raise RuntimeError(f"unexpected completion: {final}")
            print(json.dumps({"phase": "completion", "fixture": final}), flush=True)
            return child.wait(timeout=30)
        finally:
            kernel.CloseHandle(handle)
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=30)
            if desktop:
                if not user.SetThreadDesktop(original_desktop):
                    raise ctypes.WinError(ctypes.get_last_error())
                if not user.CloseDesktop(desktop):
                    raise ctypes.WinError(ctypes.get_last_error())


if __name__ == "__main__":
    raise SystemExit(main())
