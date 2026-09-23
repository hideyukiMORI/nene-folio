"""Exercise the real compiler/lint/format/CMake/linker paths, including restoration (QLT-007)."""

import json
from pathlib import Path
import re
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


def copy_tracked_files(root: Path) -> None:
    """Every file git knows about, so the conformance checker sees a whole repository."""
    listing = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, check=True, capture_output=True)
    for name in listing.stdout.decode("utf-8").split("\0"):
        if not name:
            continue
        destination = root / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / name, destination)
    run(["git", "init", "-q"], root, True)


def remove_table_entry(text: str, value: str) -> str:
    """Delete one designated initializer, tracking string literals so the closing comma is the real one."""
    match = re.search(r"[ \t]*\[" + re.escape(value) + r"\][ \t]*=", text)
    if not match:
        raise RuntimeError(f"{value} has no table entry to remove")
    index, inside = match.end(), False
    while index < len(text):
        character = text[index]
        if inside:
            inside = character != '"'
            index += 2 if character == "\\" else 1
            continue
        if character == '"':
            inside = True
        elif character == ",":
            break
        index += 1
    start = text.rfind("\n", 0, match.start()) + 1
    end = index + 2 if text[index:index + 2] == ",\n" else index + 1
    return text[:start] + text[end:]


def remove_last_column(text: str, prefix: str) -> tuple[str, str]:
    """Drop the last column of the final `[VALUE] = { ... }` entry, tracking string literals."""
    starts = list(re.finditer(r"\[(" + re.escape(prefix) + r"\w+)\][ \t]*=[ \t\r\n]*\{", text))
    if not starts:
        raise RuntimeError(f"no {prefix} entry to strip")
    match = starts[-1]
    index, depth, inside, commas = match.end(), 1, False, []
    while index < len(text):
        character = text[index]
        if inside:
            inside = character != '"'
            index += 2 if character == "\\" else 1
            continue
        if character == '"':
            inside = True
        elif character in "{[(":
            depth += 1
        elif character in "}])":
            depth -= 1
            if depth == 0:
                break
        elif character == "," and depth == 1:
            commas.append(index)
        index += 1
    if not commas or index >= len(text):
        raise RuntimeError(f"{match[1]} has no column to strip")
    return text[:commas[-1]] + text[index:], match[1]


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
    with tempfile.TemporaryDirectory(prefix="conformance-", dir=output_root) as temporary:
        root = Path(temporary).resolve()
        if not root.is_relative_to(output_root):
            raise RuntimeError("Proof workspace escaped the intended output directory")
        copy_tracked_files(root)
        conformance = ["python", "eng/conformance.py", "--root", "."]
        run(conformance, root, True)
        declaration = json.loads((root / "eng/conformance-rules.json").read_text(encoding="utf-8"))["lineTables"][0]
        table = root / declaration["table"]
        original = table.read_text(encoding="utf-8")
        value = re.findall(r"\[(" + re.escape(declaration["prefix"]) + r"\w+)\]\s*=", original)[-1]
        table.write_text(remove_table_entry(original, value), encoding="utf-8", newline="\n")
        result = run(conformance, root, False, "CNF-009")
        table.write_text(original, encoding="utf-8", newline="\n")
        restoration = run(conformance, root, True)
        evidence.append({"rule": "CNF-009", "negative": result, "restorationExit": restoration["exitCode"]})
        print(f"CNF-009: dropping {value} from the table was rejected; restoration passed")
        catalog = json.loads((root / "eng/conformance-rules.json").read_text(encoding="utf-8"))["textCatalog"]
        source = Path("src/ui/win32/folio_window.c")
        if source.as_posix() in catalog["files"]:
            raise RuntimeError("The planted file must not be the declared catalog")
        window = root / source
        original = window.read_text(encoding="utf-8")
        window.write_text(original + '\nstatic const char planted_line[] = "直書き";\n', encoding="utf-8", newline="\n")
        result = run(conformance, root, False, "CNF-010")
        window.write_text(original, encoding="utf-8", newline="\n")
        restoration = run(conformance, root, True)
        evidence.append({"rule": "CNF-010", "negative": result, "restorationExit": restoration["exitCode"]})
        print(f"CNF-010: display text outside {catalog['files'][0]} was rejected; restoration passed")
        table = root / catalog["files"][0]
        original = table.read_text(encoding="utf-8")
        stripped, value = remove_last_column(original, catalog["entryPrefix"])
        table.write_text(stripped, encoding="utf-8", newline="\n")
        result = run(conformance, root, False, "CNF-011")
        table.write_text(original, encoding="utf-8", newline="\n")
        restoration = run(conformance, root, True)
        evidence.append({"rule": "CNF-011", "negative": result, "restorationExit": restoration["exitCode"]})
        print(f"CNF-011: dropping one language column from {value} was rejected; restoration passed")
        assets = json.loads((root / "eng/conformance-rules.json").read_text(encoding="utf-8"))["bundledAssets"]
        folder = root / Path(assets["manifest"]).parent
        asset = folder / "Arimo-Regular.ttf"
        original = asset.read_bytes()
        asset.write_bytes(original[:-1] + bytes([original[-1] ^ 0xFF]))
        result = run(conformance, root, False, "CNF-012")
        asset.write_bytes(original)
        restoration = run(conformance, root, True)
        evidence.append({"rule": "CNF-012", "negative": result, "restorationExit": restoration["exitCode"]})
        print(f"CNF-012: rewriting the last byte of {asset.name} was rejected; restoration passed")
        planted = folder / "Planted-Regular.ttf"
        planted.write_bytes(original)
        result = run(conformance, root, False, "CNF-012")
        planted.unlink()
        restoration = run(conformance, root, True)
        evidence.append({"rule": "CNF-012", "negative": result, "restorationExit": restoration["exitCode"]})
        print(f"CNF-012: a file absent from the manifest ({planted.name}) was rejected; restoration passed")
    (output_root / "results.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Gate proofs passed: {len(evidence)} real-tool proofs")


if __name__ == "__main__":
    main()
