"""Linker-level determinism and dependency check (ARC-003 / ARC-007).

A static library compiled from a canonical module may only leave the undefined
symbols listed in eng/symbol-allowlist.json. Everything else is either a
non-deterministic input (ARC-007) or an undeclared dependency (ARC-003).
Python standard library only; the symbol table comes from llvm-nm.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


def undefined_symbols(nm_output: str) -> set[str]:
    symbols = set()
    for line in nm_output.splitlines():
        match = re.fullmatch(r"\s*(?:[0-9a-fA-F]+\s+)?U\s+(\S+)", line)
        if match:
            symbols.add(match[1])
    return symbols


def classify(symbols: set[str], module: str, allowlist: dict) -> list[str]:
    allowed = [re.compile(p) for p in allowlist["modules"].get(module, [])]
    nondeterministic = [re.compile(p) for p in allowlist["nondeterministic"]]
    findings = []
    for symbol in sorted(symbols):
        bare = symbol[1:] if symbol.startswith("_") and not symbol.startswith("__") else symbol
        if any(p.fullmatch(symbol) or p.fullmatch(bare) for p in nondeterministic):
            findings.append(f"ARC-007: {module}: non-deterministic input symbol {symbol}")
        elif not any(p.fullmatch(symbol) or p.fullmatch(bare) for p in allowed):
            findings.append(f"ARC-003: {module}: undeclared external symbol {symbol}")
    return findings


def run_nm(path: Path) -> str:
    result = subprocess.run(["llvm-nm", "--undefined-only", str(path)], capture_output=True, text=True,
                            encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(f"llvm-nm failed for {path}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def module_artifacts(root: Path, build_dir: Path, modules: dict) -> list[tuple[str, Path]]:
    reply = build_dir / ".cmake/api/v1/reply"
    indexes = sorted(reply.glob("index-*.json"))
    if not indexes:
        raise RuntimeError("ARC-002: CMake File API reply is missing")
    index = json.loads(indexes[-1].read_text(encoding="utf-8"))
    model = json.loads((reply / index["reply"]["codemodel-v2"]["jsonFile"]).read_text(encoding="utf-8"))
    artifacts = []
    for config in model["configurations"]:
        for entry in config["targets"]:
            target = json.loads((reply / entry["jsonFile"]).read_text(encoding="utf-8"))
            if target.get("type") != "STATIC_LIBRARY":
                continue
            sources = [s["path"].replace("\\", "/") for s in target.get("sources", [])]
            owners = {m for m, s in modules.items() if any(p.startswith(s["path"] + "/") for p in sources)}
            if len(owners) != 1:
                continue
            for artifact in target.get("artifacts", []):
                artifacts.append((next(iter(owners)), build_dir / artifact["path"]))
    return artifacts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--object", type=Path, help="check one object or library instead of the build")
    parser.add_argument("--module", help="module name for --object")
    parser.add_argument("--require", nargs="*", default=[], help="modules that must be present in the build")
    args = parser.parse_args()
    root = args.root.resolve()
    allowlist = json.loads((root / "eng/symbol-allowlist.json").read_text(encoding="utf-8"))
    modules = json.loads((root / "eng/architecture.json").read_text(encoding="utf-8"))["modules"]
    if args.object:
        if not args.module:
            raise SystemExit("--object requires --module")
        targets = [(args.module, args.object)]
    else:
        if not args.build_dir:
            raise SystemExit("--build-dir or --object is required")
        targets = [(m, p) for m, p in module_artifacts(root, args.build_dir.resolve(), modules) if m in allowlist["modules"]]
    findings = []
    for module, path in targets:
        findings.extend(classify(undefined_symbols(run_nm(path)), module, allowlist))
    present = {m for m, _ in targets}
    for module in args.require:
        if module not in present:
            findings.append(f"ARC-007: required module {module} has no static library in the build")
    for finding in findings:
        print(finding)
    print(f"Symbols: {len(targets)} librar{'y' if len(targets) == 1 else 'ies'} checked, {len(findings)} violation(s)")
    return int(bool(findings))


if __name__ == "__main__":
    raise SystemExit(main())
