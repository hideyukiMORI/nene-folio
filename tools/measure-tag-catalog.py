"""Measure the catalog fixture externally; results include pipe/scheduling overhead.

Usage: python tools/measure-tag-catalog.py build/tag_catalog_tests.exe
The fixture owns deterministic inputs and functional assertions. This driver owns
the stopwatch; source creation and inter-sample pauses are outside each interval.
"""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: measure-tag-catalog.py <fixture.exe>")
    executable = Path(sys.argv[1]).resolve(strict=True)
    with subprocess.Popen(
        [str(executable), "--measure"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        bufsize=1,
    ) as child:
        assert child.stdin is not None and child.stdout is not None
        for count in (100, 1000, 10000):
            for sample in range(7):
                intervals = []
                started = 0.0
                for phase in ("ready", "built", "released"):
                    raw = child.stdout.readline()
                    ended = time.perf_counter()
                    if not raw:
                        raise RuntimeError(f"fixture ended before {count}/{sample}/{phase}")
                    event = json.loads(raw)
                    expected = {"phase": phase, "notes": count, "sample": sample}
                    if event != expected:
                        raise RuntimeError(f"unexpected phase: {event!r}; expected {expected!r}")
                    if phase != "ready":
                        intervals.append((ended - started) * 1000)
                    started = time.perf_counter()
                    child.stdin.write("\n")
                    child.stdin.flush()
                print(json.dumps({
                    "notes": count,
                    "unique": count + 1,
                    "sample": sample,
                    "catalogMs": intervals[0],
                    "projectionAndReleaseMs": intervals[1],
                    "includesPipeAndScheduling": True,
                }), flush=True)
        child.stdin.close()
        if child.stdout.read():
            raise RuntimeError("unexpected fixture output after the last sample")
        return child.wait(timeout=30)


if __name__ == "__main__":
    raise SystemExit(main())
