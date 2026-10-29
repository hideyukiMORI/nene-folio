"""Measure the canonical core sources with LLVM; prove QLT-009 using real profiles.

The instrumented build compiles the same core / application sources and the same
unit tests with clang-cl (-fprofile-instr-generate -fcoverage-mapping). It is a
measurement build for verification, not a second product build (ADR 0004).
The unit test executable is run twice: once with --coverage-negative (which skips
most tests and must fail the threshold) and once completely (which must pass).
Python standard library only; ported from NeNe Loupe eng/coverage.py.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import subprocess

from conformance import inventory


def assess(report: dict, root: Path, required: set[Path], scopes: list[Path], minimum: int) -> dict:
    measured = {}
    for unit in report["data"]:
        for item in unit["files"]:
            path = Path(item["filename"]).resolve()
            if any(path.is_relative_to(scope) for scope in scopes):
                if path in measured:
                    raise ValueError("QLT-009: duplicate coverage file")
                measured[path] = item["summary"]["branches"]
    missing = required - measured.keys()
    if missing:
        raise ValueError("QLT-009: missing translation units: " + ", ".join(str(p) for p in sorted(missing)))
    count = sum(item["count"] for item in measured.values())
    covered = sum(item["covered"] for item in measured.values())
    if not required or not count:
        raise ValueError("QLT-009: empty measurement")
    if any(not 0 <= item["covered"] <= item["count"] for item in measured.values()):
        raise ValueError("QLT-009: invalid branch counts")
    result = {"branches": count, "covered": covered, "percent": 100 * covered / count,
              "minimum": minimum,
              "files": {p.relative_to(root).as_posix(): v for p, v in sorted(measured.items())}}
    if 100 * covered < minimum * count:
        raise ValueError(f"QLT-009: branch coverage {result['percent']:.2f}% < {minimum}%")
    return result


def run(command: list[str], cwd: Path, **kwargs) -> subprocess.CompletedProcess:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, encoding="utf-8",
                            errors="replace", **kwargs)
    if result.returncode:
        raise RuntimeError(f"{command[0]} failed ({result.returncode})\n{result.stdout}\n{result.stderr}")
    return result


def sanitizer_vector(commands: list[dict], required: set[Path]) -> list[str]:
    """Borrow one consistent Debug sanitizer selection; never invent a fallback."""
    selected = None
    seen = set()
    for record in commands:
        path = Path(record["file"]).resolve()
        if path not in required:
            continue
        seen.add(path)
        if "arguments" in record:
            arguments = record["arguments"]
        elif "command" in record:
            arguments = shlex.split(record["command"], posix=False)
        else:
            raise ValueError(f"C-016: canonical compile arguments missing: {path}")
        flags = [token.strip('"').removeprefix("/clang:") for token in arguments]
        flags = [flag for flag in flags if flag.startswith(("-fsanitize", "-fno-sanitize", "/fsanitize"))]
        enabled = set()
        no_recover = set()
        for flag in flags:
            if flag.startswith("-fsanitize="):
                enabled.update(flag.split("=", 1)[1].split(","))
            elif flag.startswith("-fno-sanitize-recover="):
                no_recover.update(flag.split("=", 1)[1].split(","))
            else:
                raise ValueError(f"C-016: unsupported or weakening sanitizer option: {flag}")
        if not {"address", "undefined", "nullability"}.issubset(enabled) or "all" not in no_recover:
            raise ValueError(f"C-016: incomplete Debug sanitizer selection: {path}")
        if selected is not None and flags != selected:
            raise ValueError(f"C-016: inconsistent Debug sanitizer selection: {path}")
        selected = flags
    if not required or seen != required or selected is None:
        raise ValueError("C-016: canonical sanitizer records missing")
    return selected


def check_binding_failure(result: subprocess.CompletedProcess, source: Path) -> None:
    diagnostic = result.stdout + result.stderr
    if (not result.returncode or source.name not in diagnostic or
            "runtime error: _Nonnull binding to null pointer" not in diagnostic):
        raise ValueError("QLT-007: nullable binding counterexample did not produce its sanitizer diagnostic")


def prove_nullability(output: Path, sanitizers: list[str]) -> None:
    proof = output / "nullability-proof"
    proof.mkdir(parents=True, exist_ok=True)
    source = proof / "binding.c"
    source.write_text(
        '#include <stdlib.h>\n'
        'static char *_Nullable candidate(int argc) { return argc > 1 ? nullptr : malloc(1); }\n'
        'int main(int argc, char **argv) { (void)argv; char *_Nonnull bytes = candidate(argc); '
        'free(bytes); return 0; }\n', encoding="utf-8")
    executable = proof / "binding.exe"
    compile_command = ["clang-cl", "/nologo", "/clang:-std=c23", "/MT", "/W4", "/WX", *sanitizers,
                       str(source), f"/Fo:{proof / 'binding.obj'}", f"/Fe:{executable}"]
    run(compile_command, proof)
    positive = run([str(executable)], proof)
    negative = subprocess.run([str(executable), "--null"], cwd=proof, capture_output=True,
                              text=True, encoding="utf-8", errors="replace")
    (proof / "results.json").write_text(json.dumps({
        "sanitizers": sanitizers, "compileCommand": compile_command,
        "positive": {"exitCode": positive.returncode, "stdout": positive.stdout, "stderr": positive.stderr},
        "negative": {"exitCode": negative.returncode, "stdout": negative.stdout, "stderr": negative.stderr},
    }, indent=2) + "\n", encoding="utf-8")
    check_binding_failure(negative, source)


def build_measurement(canonical_build: Path, output: Path, sources: list[Path], tests: list[Path],
                      scopes: list[Path], test_scope: Path, sanitizers: list[str]) -> Path:
    output.mkdir(parents=True, exist_ok=True)
    prove_nullability(output, sanitizers)
    executable = output / "folio_coverage.exe"
    objects = output / "objects"
    objects.mkdir(exist_ok=True)
    for stale in objects.glob("*.obj"):
        stale.unlink()
    common = ["clang-cl", "/nologo", "/clang:-std=c23", "/MT", "/W4", "/WX", "/utf-8", *sanitizers,
              "/clang:-fprofile-instr-generate", "/clang:-fcoverage-mapping", "/c", f"/Fo:{objects}\\"]
    common += [f"/I{scope}" for scope in scopes + [test_scope]]
    version = json.loads((canonical_build / "product-version.json").read_text(encoding="utf-8"))
    common += [f'/DNENEFOLIO_PRODUCT_VERSION="{version["version"]}"']
    # 中核の確保だけをテストの注入口へ向ける（tests/unit/allocation_probe.h）。製品の正典ビルドは触らない。
    probes = [f"/D{name}=folio_probe_{name}" for name in ("malloc", "calloc", "realloc")]
    run(common + probes + [str(p) for p in sources], output)
    run(common + [str(p) for p in tests], output)
    run(["clang-cl", "/nologo", "/MT", *sanitizers, "/clang:-fprofile-instr-generate", f"/Fe:{executable}"]
        + [str(o) for o in sorted(objects.glob("*.obj"))], output)
    return executable


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    policy = json.loads((root / "eng/coverage-policy.json").read_text(encoding="utf-8"))
    architecture = json.loads((root / "eng/architecture.json").read_text(encoding="utf-8"))
    scopes = [(root / architecture["modules"][name]["path"]).resolve() for name in policy["modules"]]
    test_scope = (root / architecture["modules"][policy["testModule"]]["path"]).resolve()
    all_paths = [(root / p).resolve() for p in inventory(root)]
    sources = sorted(p for p in all_paths if p.suffix == ".c" and any(p.is_relative_to(s) for s in scopes))
    tests = sorted(p for p in all_paths if p.suffix == ".c" and p.is_relative_to(test_scope))
    if not sources or not tests:
        raise ValueError("QLT-009: no canonical sources or tests")
    canonical_build = root / "build"
    commands = json.loads((canonical_build / "compile_commands.json").read_text(encoding="utf-8"))
    compiled = {Path(c["file"]).resolve() for c in commands}
    if not set(sources + tests).issubset(compiled):
        raise ValueError("QLT-009: measurement source absent from canonical build")
    sanitizers = sanitizer_vector(commands, set(sources + tests))
    output = root / "out/coverage"
    executable = build_measurement(canonical_build, output, sources, tests, scopes, test_scope, sanitizers)
    results = {}
    for scenario, arguments in (("negative", ["--coverage-negative"]), ("complete", [])):
        raw = output / f"{scenario}.profraw"
        raw.unlink(missing_ok=True)
        profile = output / f"{scenario}.profdata"
        run([str(executable), *arguments], output, env={**os.environ, "LLVM_PROFILE_FILE": str(raw)})
        run(["llvm-profdata", "merge", "-sparse", str(raw), "-o", str(profile)], output)
        exported = run(["llvm-cov", "export", str(executable), f"-instr-profile={profile}"], output)
        (output / f"{scenario}.json").write_text(exported.stdout, encoding="utf-8")
        try:
            result = assess(json.loads(exported.stdout), root, set(sources), scopes, policy["minimumBranchPercent"])
        except ValueError as error:
            if scenario != "negative" or not str(error).startswith("QLT-009: branch coverage"):
                raise
            results[scenario] = str(error)
        else:
            if scenario == "negative":
                raise ValueError("QLT-007: incomplete test execution did not fail QLT-009")
            results[scenario] = result
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(results, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
