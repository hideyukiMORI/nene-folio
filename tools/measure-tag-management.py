"""Measure real three-note management, including pipe, UI and OS scheduling costs.

The deterministic native fixture owns only phase handshakes and assertions.
This driver owns the stopwatch, and never operates the user's data directory.
"""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: measure-tag-management.py <tags_native_tests.exe>")
    executable = Path(sys.argv[1]).resolve(strict=True)
    with subprocess.Popen(
        [str(executable), "--tag-management-measure"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        bufsize=1,
        creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
    ) as child:
        assert child.stdin is not None and child.stdout is not None
        for sample in range(7):
            intervals = []
            started = 0.0
            for phase in ("ready", "prepared", "current", "finished"):
                raw = child.stdout.readline()
                ended = time.perf_counter()
                if not raw:
                    raise RuntimeError(f"fixture ended before {sample}/{phase}")
                event = json.loads(raw)
                if event != {"phase": phase, "sample": sample}:
                    raise RuntimeError(f"unexpected phase: {event!r}")
                if phase != "ready":
                    intervals.append((ended - started) * 1000)
                started = time.perf_counter()
                child.stdin.write("\n")
                child.stdin.flush()
            print(json.dumps({
                "sample": sample,
                "managedNotes": 3,
                "changedNotes": 2,
                "unavailableNotes": 1,
                "preparationAndPreviewMs": intervals[0],
                "startFirstPublicationAndUiMs": intervals[1],
                "remainingPublicationAndResultMs": intervals[2],
                "includesPipeUiAndScheduling": True,
            }), flush=True)
        child.stdin.close()
        final = child.stdout.read().strip()
        if not final.startswith("image/status: ") or not final.endswith(" checks passed"):
            raise RuntimeError(f"unexpected completion: {final!r}")
        return child.wait(timeout=30)


if __name__ == "__main__":
    raise SystemExit(main())
