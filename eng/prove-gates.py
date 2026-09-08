"""Exercise the real compiler/lint/format/CMake/linker paths, including restoration (QLT-007)."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def run(command: list[str], cwd: Path, succeeds: bool, diagnostic: str = "") -> dict:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    output = result.stdout + result.stderr
    if (result.returncode == 0) != succeeds or (diagnostic and diagnostic not in output):
        raise RuntimeError(f"Unexpected gate proof: {command}\n{output}")
    return {"command": command, "exitCode": result.returncode, "output": output}


def copy_repository_files(root: Path) -> None:
    files = ["CMakeLists.txt", "eng/targets.cmake", "eng/architecture.json", "eng/symbol-allowlist.json",
             "eng/symbols.py", ".clang-tidy", ".clang-format", "tests/build/toolchain_smoke.c"]
    for folder in ["src", "tests/unit"]:
        if (ROOT / folder).is_dir():
            files += [p.relative_to(ROOT).as_posix() for p in (ROOT / folder).rglob("*") if p.is_file()]
    for path in files:
        destination = root / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / path, destination)


COMPILER_PROBES = [
    ("QLT-002", "int main(void) { int unused; return 0; }\n", "-Wunused-variable"),
    ("QLT-002", "int lonely(void) { return 1; }\nint main(void) { return lonely() - 1; }\n", "-Wmissing-prototypes"),
    ("C-002", "enum mode { MODE_VIEW, MODE_EDIT };\nstatic int pick(enum mode m) { switch (m) { case MODE_VIEW: return 0; } return 1; }\nint main(void) { return pick(MODE_VIEW); }\n", "-Wswitch-enum"),
    ("C-002", "enum mode { MODE_VIEW, MODE_EDIT };\nstatic int pick(enum mode m) { switch (m) { case MODE_VIEW: return 0; case MODE_EDIT: return 1; default: return 2; } }\nint main(void) { return pick(MODE_VIEW); }\n", "-Wcovered-switch-default"),
    ("C-003", "struct note { int width; };\nstatic void touch(const struct note *n) { ((struct note *)n)->width = 1; }\nint main(void) { struct note n = {0}; touch(&n); return n.width - 1; }\n", "-Wcast-qual"),
    ("C-004", "int main(void) { int *p = nullptr; return *p; }\n", "clang-analyzer-core.NullDereference"),
    ("C-012", "static int pick(int a, int b, int c, int d, int e) { return a + b + c + d + e; }\nint main(void) { return pick(0, 0, 0, 0, 0); }\n", "readability-function-size"),
    ("C-016", "int main(int argc, char **argv) { (void)argv; int a[argc]; a[0] = 0; return a[0]; }\n", "vla"),
]


def main() -> None:
    output_root = (ROOT / "out/proofs").resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    evidence = []
    with tempfile.TemporaryDirectory(prefix="build-", dir=output_root) as temporary:
        root = Path(temporary).resolve()
        if not root.is_relative_to(output_root):
            raise RuntimeError("Proof workspace escaped the intended output directory")
        copy_repository_files(root)
        source = root / "tests/build/toolchain_smoke.c"
        original = source.read_text(encoding="utf-8")
        configure = ["cmake", "-S", ".", "-B", "build", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"]
        build = ["cmake", "--build", "build", "--target", "toolchain_smoke", "--clean-first"]
        run(configure, root, True)
        run(build, root, True)
        for rule, text, diagnostic in COMPILER_PROBES:
            source.write_text(text, encoding="utf-8")
            result = run(build, root, False, diagnostic)
            source.write_text(original, encoding="utf-8")
            restoration = run(build, root, True)
            evidence.append({"rule": rule, "negative": result, "restorationExit": restoration["exitCode"]})
            print(f"{rule}: rejected {diagnostic}; restored build passed")
        source.write_text("int main(void){return 0;}\n", encoding="utf-8")
        result = run(["clang-format", "--dry-run", "--Werror", f"--style=file:{root / '.clang-format'}", str(source)], root, False, "clang-format-violations")
        source.write_text(original, encoding="utf-8")
        run(["clang-format", "--dry-run", "--Werror", f"--style=file:{root / '.clang-format'}", str(source)], root, True)
        evidence.append({"rule": "QLT-004", "negative": result, "restorationExit": 0})
        cmake_file = root / "CMakeLists.txt"
        cmake_original = cmake_file.read_text(encoding="utf-8")
        cmake_file.write_text(cmake_original + "\nnenefolio_target(other verification STATIC tests/build/toolchain_smoke.c)\nnenefolio_link(toolchain_smoke other)\n", encoding="utf-8")
        result = run(configure, root, False, "ARC-002")
        cmake_file.write_text(cmake_original, encoding="utf-8")
        run(configure, root, True)
        run(build, root, True)
        evidence.append({"rule": "ARC-002", "negative": result, "restorationExit": 0})
        print("QLT-004 / ARC-002: real tools rejected violations; restoration passed")
        cmake_file.write_text(cmake_original + "\nnenefolio_system_link(toolchain_smoke user32)\n", encoding="utf-8")
        result = run(configure, root, False, "ARC-002")
        cmake_file.write_text(cmake_original, encoding="utf-8")
        run(configure, root, True)
        run(build, root, True)
        evidence.append({"rule": "ARC-002", "negative": result, "restorationExit": 0})
        print("ARC-002: verification platform library rejected; restoration passed")
        probe = root / "probe.c"
        compile_probe = ["clang-cl", "/nologo", "/clang:-std=c23", "/W4", "/WX", "/c", str(probe), f"/Fo{root / 'probe.obj'}"]
        symbols = ["python", str(root / "eng/symbols.py"), "--root", str(root), "--object", str(root / "probe.obj"), "--module", "core"]
        probe.write_text("#include <time.h>\nlong nenefolio_now(void);\nlong nenefolio_now(void) { return (long)time(0); }\n", encoding="utf-8")
        run(compile_probe, root, True)
        result = run(symbols, root, False, "ARC-007")
        probe.write_text("#include <string.h>\nint nenefolio_same(const char *a, const char *b);\nint nenefolio_same(const char *a, const char *b) { return memcmp(a, b, strlen(a)) == 0; }\n", encoding="utf-8")
        run(compile_probe, root, True)
        restoration = run(symbols, root, True)
        evidence.append({"rule": "ARC-007", "negative": result, "restorationExit": restoration["exitCode"]})
        probe.write_text("#include <windows.h>\nunsigned long nenefolio_tick(void);\nunsigned long nenefolio_tick(void) { return GetTickCount(); }\n", encoding="utf-8")
        run(compile_probe, root, True)
        result = run(symbols, root, False, "ARC-007")
        probe.write_text("#include <windows.h>\nvoid *nenefolio_open(void);\nvoid *nenefolio_open(void) { return CreateFileW(L\"x\", 0, 0, nullptr, 3, 0, nullptr); }\n", encoding="utf-8")
        run(compile_probe, root, True)
        result2 = run(symbols, root, False, "ARC-003")
        evidence.append({"rule": "ARC-003", "negative": result2, "win32Tick": result, "restorationExit": 0})
        print("ARC-007 / ARC-003: llvm-nm symbol check rejected time, GetTickCount and CreateFileW in core; memcmp passed")
        probe.write_text("#include <string.h>\nint nenefolio_same(const char *a, const char *b);\nint nenefolio_same(const char *a, const char *b) { return memcmp(a, b, strlen(a)) == 0; }\n", encoding="utf-8")
        run(compile_probe, root, True)
        result = run(symbols + ["--require", "application"], root, False, "required module application has no static library")
        restoration = run(symbols + ["--require", "core"], root, True)
        evidence.append({"rule": "ARC-003", "negative": result, "restorationExit": restoration["exitCode"]})
        print("ARC-003: a required module without a static library is rejected; the present module passes")
    (output_root / "results.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Gate proofs passed: {len(evidence)} real-tool proofs")


if __name__ == "__main__":
    main()
