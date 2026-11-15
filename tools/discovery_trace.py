"""Validate external pipe observations of the native fixture's initial read pass."""

import math
import statistics


class ReadCostTrace:
    def __init__(self, expected_reads: int | None) -> None:
        if expected_reads is not None and (type(expected_reads) is not int or expected_reads <= 0):
            raise ValueError("read trace needs a positive expected read count")
        self.expected_reads = expected_reads
        self.steps: list[dict] = []
        self.current: dict | None = None
        self.last_ms = 0.0
        self.sealed = False

    def _time(self, arrived_ms: float) -> None:
        if not math.isfinite(arrived_ms) or arrived_ms < self.last_ms:
            raise RuntimeError("read trace has a negative, nonfinite, or reversed arrival time")
        self.last_ms = arrived_ms

    def accept(self, event: dict, arrived_ms: float) -> None:
        if self.expected_reads is None or self.sealed:
            raise RuntimeError("read trace arrived outside the requested initial pass")
        if event.get("event") != "cost" or type(event.get("command")) is not int or event["command"] != 1:
            raise RuntimeError("read trace belongs to an unexpected command")
        identifier = event.get("idle")
        if type(identifier) is not int or identifier != len(self.steps) + 1:
            raise RuntimeError("read trace has a duplicate, missing, or mismatched idle identifier")
        self._time(arrived_ms)
        phase = event.get("phase")
        if phase == "idle_begin":
            if self.current is not None:
                raise RuntimeError("read trace contains overlapping idle steps")
            self.current = {"idle": identifier, "idleBeginMs": arrived_ms}
            return
        if self.current is None:
            raise RuntimeError("read trace has no open idle step")
        if phase == "read_begin":
            if "readBeginMs" in self.current:
                raise RuntimeError("read trace contains multiple reads in one idle step")
            self.current["readBeginMs"] = arrived_ms
        elif phase == "read_end":
            if "readBeginMs" not in self.current or "readEndMs" in self.current:
                raise RuntimeError("read trace ends a missing or already completed read")
            self.current["readEndMs"] = arrived_ms
        elif phase == "idle_end":
            if ("readBeginMs" in self.current) != ("readEndMs" in self.current):
                raise RuntimeError("read trace ends an idle step with an outstanding read")
            self.current["idleEndMs"] = arrived_ms
            self.steps.append(self.current)
            self.current = None
        else:
            raise RuntimeError("read trace contains an unknown boundary")

    def seal(self, arrived_ms: float) -> None:
        if self.expected_reads is None or self.sealed or self.current is not None:
            raise RuntimeError("read trace cannot finish an absent, repeated, or incomplete pass")
        self._time(arrived_ms)
        reads = sum("readEndMs" in row for row in self.steps)
        if reads != self.expected_reads:
            raise RuntimeError(f"read trace observed {reads} reads, expected {self.expected_reads}")
        self.sealed = True

    def result(self) -> dict | None:
        if self.expected_reads is None:
            return None
        if not self.sealed:
            raise RuntimeError("read trace did not reach the complete source boundary")
        idle = [row["idleEndMs"] - row["idleBeginMs"] for row in self.steps]
        reads = [row["readEndMs"] - row["readBeginMs"] for row in self.steps if "readEndMs" in row]
        outside = [((row["readBeginMs"] - row["idleBeginMs"]) +
                    (row["idleEndMs"] - row["readEndMs"])) if "readEndMs" in row else
                   row["idleEndMs"] - row["idleBeginMs"]
                   for row in self.steps]
        return {"idleSteps": len(idle), "readCalls": len(reads),
                "idleArrivalMs": summarize(idle), "readPortArrivalMs": summarize(reads),
                "outsideReadWithinIdleArrivalMs": summarize(outside), "steps": self.steps,
                "includesMarkerPipeAndScheduling": True, "notPureDiskOrPaintTime": True}


def summarize(values: list[float]) -> dict:
    return {"count": len(values), "total": sum(values), "min": min(values),
            "median": statistics.median(values), "max": max(values)}
