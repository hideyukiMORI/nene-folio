"""Reject incomplete/ambiguous measured intervals before reporting read cost (#833)."""

import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("discovery_trace", ROOT / "tools/discovery_trace.py")
trace_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(trace_module)
ReadCostTrace = trace_module.ReadCostTrace


def boundary(phase, idle=1, command=1):
    return {"event": "cost", "phase": phase, "idle": idle, "command": command}


class DiscoveryTraceTests(unittest.TestCase):
    def test_two_real_reads_and_empty_idle_exclude_gaps_between_idle_steps(self):
        trace = ReadCostTrace(2)
        for idle, phases in ((1, (("idle_begin", 10), ("read_begin", 11),
                                 ("read_end", 13), ("idle_end", 17))),
                             (2, (("idle_begin", 20), ("idle_end", 23))),
                             (3, (("idle_begin", 30), ("read_begin", 32),
                                 ("read_end", 35), ("idle_end", 40)))):
            for phase, arrived in phases:
                trace.accept(boundary(phase, idle), arrived)
        trace.seal(50)
        result = trace.result()
        self.assertEqual(2, result["readCalls"])
        self.assertEqual(3, result["idleSteps"])
        self.assertEqual(20, result["idleArrivalMs"]["total"])
        self.assertEqual(5, result["readPortArrivalMs"]["total"])
        self.assertEqual(15, result["outsideReadWithinIdleArrivalMs"]["total"])
        self.assertEqual(2.5, result["readPortArrivalMs"]["median"])
        self.assertEqual(32, result["steps"][2]["readBeginMs"])
        self.assertTrue(result["includesMarkerPipeAndScheduling"])

    def test_malformed_or_foreign_marker_cannot_enter_an_idle(self):
        invalid = [boundary("idle_begin", 2), boundary("idle_begin", 0),
                   boundary("idle_begin", True), boundary("idle_begin", 1.0),
                   boundary("idle_begin", command=2), boundary("idle_begin", command=True),
                   {"event": "cost", "phase": "idle_begin", "command": 1},
                   {"event": "result", "phase": "idle_begin", "idle": 1, "command": 1}]
        for event in invalid:
            with self.subTest(event=event), self.assertRaises(RuntimeError):
                ReadCostTrace(1).accept(event, 1)

    def test_nested_duplicate_missing_and_out_of_order_boundaries_fail(self):
        invalid = [
            ["read_begin"], ["read_end"], ["idle_end"],
            ["idle_begin", "idle_begin"],
            ["idle_begin", "read_end"],
            ["idle_begin", "read_begin", "read_begin"],
            ["idle_begin", "read_begin", "idle_end"],
            ["idle_begin", "read_begin", "read_end", "read_end"],
            ["idle_begin", "read_begin", "read_end", "read_begin"],
            ["idle_begin", "unknown"],
            ["idle_begin", "idle_end", "idle_end"],
        ]
        for phases in invalid:
            trace = ReadCostTrace(1)
            with self.subTest(phases=phases), self.assertRaises(RuntimeError):
                for arrived, phase in enumerate(phases, 1):
                    trace.accept(boundary(phase), arrived)

    def test_idle_identifier_must_match_the_open_step(self):
        trace = ReadCostTrace(1)
        trace.accept(boundary("idle_begin"), 1)
        with self.assertRaises(RuntimeError):
            trace.accept(boundary("read_begin", 2), 2)

    def test_invalid_or_reversed_arrivals_never_produce_negative_cost(self):
        for bad in (-1, float("nan"), float("inf"), float("-inf")):
            with self.subTest(value=bad), self.assertRaises(RuntimeError):
                ReadCostTrace(1).accept(boundary("idle_begin"), bad)
        trace = ReadCostTrace(1)
        trace.accept(boundary("idle_begin"), 10)
        with self.assertRaises(RuntimeError):
            trace.accept(boundary("read_begin"), 9)

    def test_incomplete_and_wrong_read_count_cannot_be_finalized(self):
        for phases in ([], ["idle_begin"], ["idle_begin", "read_begin"],
                       ["idle_begin", "read_begin", "read_end"],
                       ["idle_begin", "idle_end"]):
            trace = ReadCostTrace(1)
            for arrived, phase in enumerate(phases, 1):
                trace.accept(boundary(phase), arrived)
            with self.subTest(phases=phases), self.assertRaises(RuntimeError):
                trace.seal(10)
            with self.assertRaises(RuntimeError):
                trace.result()
        trace = ReadCostTrace(2)
        for arrived, phase in enumerate(("idle_begin", "read_begin", "read_end", "idle_end"), 1):
            trace.accept(boundary(phase), arrived)
        with self.assertRaises(RuntimeError):
            trace.seal(5)

    def test_finished_trace_rejects_late_or_repeated_records(self):
        trace = ReadCostTrace(1)
        for phase in ("idle_begin", "read_begin", "read_end", "idle_end"):
            trace.accept(boundary(phase), 1)
        trace.seal(1)
        self.assertEqual(0, trace.result()["readPortArrivalMs"]["total"])
        with self.assertRaises(RuntimeError):
            trace.accept(boundary("idle_begin", 2), 2)
        with self.assertRaises(RuntimeError):
            trace.seal(2)

    def test_unrequested_trace_cannot_be_mistaken_for_measured_success(self):
        trace = ReadCostTrace(None)
        self.assertIsNone(trace.result())
        with self.assertRaises(RuntimeError):
            trace.accept(boundary("idle_begin"), 1)
        with self.assertRaises(RuntimeError):
            trace.seal(1)
        for count in (0, -1, True, 1.0):
            with self.subTest(count=count), self.assertRaises(ValueError):
                ReadCostTrace(count)


if __name__ == "__main__":
    unittest.main()
