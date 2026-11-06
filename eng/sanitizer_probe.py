"""Prove both instrumented writes and the CRT frexp interceptor on owned memory (#713)."""

import json
from pathlib import Path
import subprocess


SOURCE = r'''#include <math.h>
#include <sanitizer/asan_interface.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *_Nonnull *_Nonnull argv)
{
    if (argc != 2)
        return 2;
    bool valid = strcmp(argv[1], "valid") == 0;
    bool direct = strcmp(argv[1], "direct") == 0;
    if (!valid && !direct && strcmp(argv[1], "frexp") != 0)
        return 2;
    int *allocation = malloc(16);
    if (allocation == nullptr)
        return 3;
    *allocation = 0;
    if (!valid)
        __asan_poison_memory_region(allocation, 8);
    int poisoned = __asan_address_is_poisoned(allocation);
    printf("mode=%s poisoned=%d\n", argv[1], poisoned);
    fflush(stdout);
    if ((poisoned != 0) == valid)
    {
        __asan_unpoison_memory_region(allocation, 16);
        free(allocation);
        return 4;
    }
    if (direct)
    {
        *(volatile int *)allocation = 7;
        __asan_unpoison_memory_region(allocation, 16);
        free(allocation);
        return 0;
    }
    volatile double input = 1.5;
    double (*volatile convert)(double, int *) = frexp;
    double fraction = convert(input, allocation);
    __asan_unpoison_memory_region(allocation, 16);
    int exponent = *allocation;
    free(allocation);
    printf("fraction=%g exponent=%d\n", fraction, exponent);
    return fraction == 0.75 && exponent == 1 ? 0 : 5;
}
'''


def check_results(results: dict) -> None:
    positive = results["valid"]
    if (positive["exitCode"] != 0 or positive["stderr"] or
            positive["stdout"].splitlines() != ["mode=valid poisoned=0", "fraction=0.75 exponent=1"]):
        raise ValueError("C-016: normal CRT sanitizer probe failed or emitted a diagnostic")
    for mode in ("direct", "frexp"):
        result = results[mode]
        diagnostic = result["stderr"]
        if (result["exitCode"] == 0 or result["stdout"].splitlines() != [f"mode={mode} poisoned=1"] or
                "ERROR: AddressSanitizer: use-after-poison" not in diagnostic or
                "WRITE of size 4" not in diagnostic or
                "interception_win: unhandled instruction" in diagnostic or
                (mode == "frexp" and " in frexp " not in diagnostic)):
            raise ValueError(f"QLT-007: {mode} counterexample did not produce its ASan diagnostic")


def prove(executable: Path, output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    results = {}
    record = output / "results.json"
    record.write_text("{}\n", encoding="utf-8")
    for mode in ("valid", "direct", "frexp"):
        command = [str(executable), mode]
        try:
            result = subprocess.run(command, cwd=output, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=15)
        except (subprocess.TimeoutExpired, OSError) as error:
            partial = {"command": command, "exitCode": None, "error": str(error)}
            for name in ("stdout", "stderr"):
                value = getattr(error, name, None) or ""
                partial[name] = value.decode("utf-8", errors="replace") if isinstance(value, bytes) else value
            results[mode] = partial
            record.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
            raise
        results[mode] = {"command": command, "exitCode": result.returncode,
                         "stdout": result.stdout, "stderr": result.stderr}
        record.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    check_results(results)
    return results
