"""Single release layout, artifact validation and ZIP path (ADR0045; stdlib only)."""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile

from conformance import bundled_asset_checks


ROOT = Path(__file__).resolve().parents[1]


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def metadata(path: Path) -> dict:
    value = read_json(path)
    version = value["version"]
    if not re.fullmatch(r"\d+(?:\.\d+){0,3}", version):
        raise ValueError("FR-014: invalid product version")
    numbers = [int(part) for part in version.split(".")]
    numbers += [0] * (4 - len(numbers))
    if any(number > 65535 for number in numbers):
        raise ValueError("FR-014: version component out of range")
    if value["windowsVersion"] != ".".join(map(str, numbers)):
        raise ValueError("FR-014: inconsistent Windows version")
    return value


def safe_relative(value: str) -> str:
    path = PurePosixPath(value)
    if not value or path.is_absolute() or "\\" in value or ":" in value:
        raise ValueError("release: unsafe relative path")
    if any(part in {"", ".", ".."} for part in value.split("/")):
        raise ValueError("release: unsafe relative path")
    return value


def layout_files(build: Path) -> dict[str, Path]:
    layout = read_json(ROOT / "eng/release-layout.json")
    result = {}
    for item in layout["files"]:
        folder = {"build": build, "repository": ROOT}[item["source"]]
        destination = safe_relative(item["destination"])
        if destination in result:
            raise ValueError("release: duplicate declared destination")
        result[destination] = folder / safe_relative(item["path"])
    rules = read_json(ROOT / "eng/conformance-rules.json")["bundledAssets"]
    manifest = safe_relative(rules["manifest"])
    assets = read_json(ROOT / manifest)
    folder = PurePosixPath(manifest).parent
    for leaf in rules["ignore"] + [item["filename"] for item in assets["entries"] + assets["licenses"]]:
        destination = safe_relative((folder / leaf).as_posix())
        if destination in result:
            raise ValueError("release: duplicate declared destination")
        result[destination] = ROOT / destination
    if len({name.casefold() for name in result}) != len(result):
        raise ValueError("release: colliding declared destinations")
    return result


def bundle_name(value: dict) -> str:
    name = read_json(ROOT / "eng/release-layout.json")["bundleName"].format(version=value["version"])
    if "/" in safe_relative(name):
        raise ValueError("release: bundle name is not a leaf")
    return name


def reject_reparse(path: Path) -> None:
    for part in [path, *path.parents]:
        try:
            info = part.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError(f"release: reparse point is not allowed: {part}")


def fresh_destinations(work: Path, output: Path, source: Path = ROOT) -> None:
    for path in (work, output):
        if not path.is_absolute():
            raise ValueError("release: work and output require absolute paths")
        reject_reparse(path)
        if path.exists():
            raise ValueError(f"release: destination already exists: {path}")
    work, output, source = work.resolve(), output.resolve(), source.resolve()
    for first, second in ((work, output), (work, source), (output, source)):
        if first.is_relative_to(second) or second.is_relative_to(first):
            raise ValueError("release: destinations overlap each other or the source")
    if not work.parent.is_dir() or not output.parent.is_dir():
        raise ValueError("release: destination parents must already exist")


def tree_files(folder: Path) -> dict[str, Path]:
    result = {}
    directories = set()
    for path in folder.rglob("*"):
        reject_reparse(path)
        relative = safe_relative(path.relative_to(folder).as_posix())
        if path.is_file():
            result[relative] = path
        elif path.is_dir():
            directories.add(relative)
        else:
            raise ValueError("release: unsupported filesystem entry")
    expected_dirs = {parent.as_posix() for name in result for parent in PurePosixPath(name).parents
                     if parent.as_posix() != "."}
    if directories != expected_dirs:
        raise ValueError("release: unexpected empty directory")
    if len({name.casefold() for name in result}) != len(result):
        raise ValueError("release: colliding files")
    return result


def validate_file_set(folder: Path, expected: set[str]) -> dict[str, Path]:
    files = tree_files(folder)
    if set(files) != expected:
        raise ValueError(f"release: file set differs; missing={sorted(expected - files.keys())}, "
                         f"extra={sorted(files.keys() - expected)}")
    return files


def validate_fonts(folder: Path) -> None:
    rules = read_json(ROOT / "eng/conformance-rules.json")
    manifest = rules["bundledAssets"]["manifest"]
    if (folder / manifest).read_bytes() != (ROOT / manifest).read_bytes():
        raise ValueError("release: bundled font manifest differs from source")
    findings = bundled_asset_checks(folder, rules)
    if findings:
        raise ValueError("release: font integrity: " + "; ".join(str(item) for item in findings))


def validate_imports(imports: list[str]) -> None:
    architecture = read_json(ROOT / "eng/architecture.json")
    allowed = {"kernel32.dll"} | {library.lower() + ".dll"
                                for libraries in architecture["platformLibraries"].values()
                                for library in libraries}
    if not imports or any(name.lower() not in allowed for name in imports):
        raise ValueError("release: undeclared runtime import")


def validate_manifest(xml: bytes, value: dict) -> None:
    tree = ET.fromstring(xml.rstrip(b"\0"))
    namespaces = {"v1": "urn:schemas-microsoft-com:asm.v1", "v3": "urn:schemas-microsoft-com:asm.v3",
                  "dpi": "http://schemas.microsoft.com/SMI/2016/WindowsSettings",
                  "old": "http://schemas.microsoft.com/SMI/2005/WindowsSettings",
                  "compat": "urn:schemas-microsoft-com:compatibility.v1"}
    identity = tree.find("v1:assemblyIdentity", namespaces)
    if identity is None or identity.attrib != {"name": "NeNeFolio", "version": value["windowsVersion"],
                                               "processorArchitecture": "amd64", "type": "win32"}:
        raise ValueError("FR-014: manifest identity differs")
    for expression, wanted in ((".//dpi:dpiAwareness", "PerMonitorV2"), (".//old:dpiAware", "true")):
        element = tree.find(expression, namespaces)
        if element is None or element.text != wanted:
            raise ValueError("release: DPI manifest differs")
    privileges = tree.findall(".//v3:requestedExecutionLevel", namespaces)
    if len(privileges) != 1 or privileges[0].attrib != {"level": "asInvoker", "uiAccess": "false"}:
        raise ValueError("release: UAC manifest differs")
    supported = tree.findall(".//compat:supportedOS", namespaces)
    if len(supported) != 1 or supported[0].get("Id") != "{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}":
        raise ValueError("release: supported OS manifest differs")


def windows_versions(path: Path) -> dict:
    api = ctypes.WinDLL("version", use_last_error=True)
    api.GetFileVersionInfoSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
    api.GetFileVersionInfoSizeW.restype = wintypes.DWORD
    api.GetFileVersionInfoW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    api.GetFileVersionInfoW.restype = wintypes.BOOL
    api.VerQueryValueW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_void_p),
                                  ctypes.POINTER(wintypes.UINT)]
    api.VerQueryValueW.restype = wintypes.BOOL
    unused = wintypes.DWORD()
    size = api.GetFileVersionInfoSizeW(str(path), ctypes.byref(unused))
    if not size:
        raise ValueError("FR-014: VERSIONINFO is missing")
    block = ctypes.create_string_buffer(size)
    if not api.GetFileVersionInfoW(str(path), 0, size, block):
        raise ValueError("FR-014: VERSIONINFO is unreadable")

    def query(name: str) -> tuple[int, int]:
        pointer, count = ctypes.c_void_p(), wintypes.UINT()
        if not api.VerQueryValueW(block, name, ctypes.byref(pointer), ctypes.byref(count)):
            raise ValueError("FR-014: version field is missing: " + name)
        return pointer.value, count.value

    address, length = query("\\")
    if length != 52:
        raise ValueError("FR-014: invalid fixed version")
    fixed = struct.unpack("<13I", ctypes.string_at(address, length))
    if fixed[0] != 0xFEEF04BD:
        raise ValueError("FR-014: invalid fixed version signature")
    address, length = query("\\VarFileInfo\\Translation")
    if length != 4:
        raise ValueError("FR-014: ambiguous version translation")
    language, codepage = struct.unpack("<HH", ctypes.string_at(address, length))
    strings = {}
    for name in ("FileVersion", "ProductVersion"):
        address, length = query(f"\\StringFileInfo\\{language:04x}{codepage:04x}\\{name}")
        strings[name] = ctypes.wstring_at(address, length).rstrip("\0")
    return {"fixedFile": [fixed[2] >> 16, fixed[2] & 65535, fixed[3] >> 16, fixed[3] & 65535],
            "fixedProduct": [fixed[4] >> 16, fixed[4] & 65535, fixed[5] >> 16, fixed[5] & 65535],
            **strings}


def embedded_manifest(path: Path) -> bytes:
    api = ctypes.WinDLL("kernel32", use_last_error=True)
    api.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
    api.LoadLibraryExW.restype = wintypes.HMODULE
    api.FreeLibrary.argtypes = [wintypes.HMODULE]
    api.FindResourceW.argtypes = [wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p]
    api.FindResourceW.restype = wintypes.HRSRC
    api.LoadResource.argtypes = [wintypes.HMODULE, wintypes.HRSRC]
    api.LoadResource.restype = wintypes.HGLOBAL
    api.LockResource.argtypes = [wintypes.HGLOBAL]
    api.LockResource.restype = ctypes.c_void_p
    api.SizeofResource.argtypes = [wintypes.HMODULE, wintypes.HRSRC]
    api.SizeofResource.restype = wintypes.DWORD
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HMODULE, ctypes.c_void_p,
                                       ctypes.c_void_p, ctypes.c_ssize_t)
    api.EnumResourceNamesW.argtypes = [wintypes.HMODULE, ctypes.c_void_p, callback_type, ctypes.c_ssize_t]
    api.EnumResourceNamesW.restype = wintypes.BOOL
    module = api.LoadLibraryExW(str(path.resolve()), None, 0x02 | 0x20)
    if not module:
        raise ValueError("release: cannot read executable resources")
    try:
        names = []
        callback = callback_type(lambda module, kind, name, context: names.append(name) is None)
        if not api.EnumResourceNamesW(module, 24, callback, 0) or names != [1]:
            raise ValueError("release: manifest must be the unique resource ID 1")
        resource = api.FindResourceW(module, 1, 24)
        size = api.SizeofResource(module, resource)
        address = api.LockResource(api.LoadResource(module, resource))
        if not resource or not size or not address:
            raise ValueError("release: embedded manifest is unreadable")
        return ctypes.string_at(address, size)
    finally:
        api.FreeLibrary(module)


def validate_versions(actual: dict, value: dict) -> None:
    expected = list(map(int, value["windowsVersion"].split(".")))
    if actual["fixedFile"] != expected or actual["fixedProduct"] != expected:
        raise ValueError("FR-014: fixed version differs")
    if actual["FileVersion"] != value["windowsVersion"] or actual["ProductVersion"] != value["version"]:
        raise ValueError("FR-014: string version differs")


def validate_identity(path: Path, value: dict) -> dict:
    result = subprocess.run(["llvm-readobj", "--file-headers", "--coff-imports", str(path)],
                            capture_output=True, text=True, encoding="utf-8", check=False)
    if not re.search(r"Machine:\s+IMAGE_FILE_MACHINE_AMD64\b", result.stdout):
        raise ValueError("release: executable is not AMD64")
    # A mismatched machine can also make import decoding fail; valid AMD64 still requires full success.
    result.check_returncode()
    imports = re.findall(r"^\s+Name:\s+(\S+)\s*$", result.stdout, re.M)
    versions = windows_versions(path)
    validate_versions(versions, value)
    validate_manifest(embedded_manifest(path), value)
    return {"imports": imports, "versions": versions, "machine": "AMD64"}


def validate_executable(path: Path, value: dict) -> dict:
    result = validate_identity(path, value)
    validate_imports(result["imports"])
    return result


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def source_inputs() -> dict:
    listing = subprocess.check_output(["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
                                      cwd=ROOT).decode("utf-8").split("\0")
    prefixes = ("src/", "eng/", "tests/", "fonts/", "cmake/", "docs/release/")
    files = {name: digest(ROOT / name) for name in sorted(set(listing)) if name and (ROOT / name).is_file() and
             (name.startswith(prefixes) or name in {"CMakeLists.txt", ".clang-tidy", ".clang-format", "LICENSE", "THIRD_PARTY_NOTICES.txt"})}
    return {"sourceCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "sourceDirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT)),
            "sourceInputsSHA256": files}


def publish_files(archive: Path, checksums: Path, output: Path) -> Path:
    reject_reparse(output)
    if os.name != "nt" or output.exists():
        raise ValueError("release: publishing requires a new Windows output directory")
    partial = Path(tempfile.mkdtemp(prefix="." + output.name + ".partial-", dir=output.parent))
    (archive.parent / "publish-partial.json").write_text(json.dumps({"partial": str(partial)}) + "\n",
                                                        encoding="utf-8")
    for source in (archive, checksums):
        destination = partial / source.name
        shutil.copyfile(source, destination)
        if digest(source) != digest(destination):
            raise ValueError("release: copied output differs; partial retained: " + str(partial))
    # Windows rename fails when the destination exists; it never replaces another directory.
    partial.rename(output)
    return output


def validate_stage(folder: Path, build: Path, value: dict) -> dict:
    declared = layout_files(build)
    actual = validate_file_set(folder, set(declared))
    for name, source in declared.items():
        if digest(actual[name]) != digest(source):
            raise ValueError("release: staged file differs from source: " + name)
    validate_fonts(folder)
    artifact = validate_executable(folder / "NeNeFolio.exe", value)
    return {"artifact": artifact, "sha256": {name: digest(path) for name, path in sorted(actual.items())}}


def validate_archive(archive: Path, stage: Path, prefix: str) -> None:
    expected = tree_files(stage)
    with zipfile.ZipFile(archive) as bundle:
        names = [entry.filename for entry in bundle.infolist()]
        if len(names) != len(set(names)) or len(names) != len({name.casefold() for name in names}):
            raise ValueError("release: duplicate ZIP entries")
        if set(names) != {prefix + "/" + name for name in expected}:
            raise ValueError("release: ZIP file set differs")
        for name, path in expected.items():
            entry = bundle.getinfo(prefix + "/" + name)
            if entry.is_dir() or stat.S_ISLNK(entry.external_attr >> 16):
                raise ValueError("release: unsupported ZIP entry")
            if hashlib.sha256(bundle.read(entry)).hexdigest() != digest(path):
                raise ValueError("release: ZIP content differs")


def assemble(build: Path, work: Path, output: Path) -> dict:
    recorded = read_json(work / "source-inputs.json")
    if recorded["sourceInputsSHA256"] != source_inputs()["sourceInputsSHA256"]:
        raise ValueError("release: source inputs changed during build")
    value = metadata(build / "product-version.json")
    if value["buildType"] != "Release":
        raise ValueError("release: package requires a Release build")
    prefix = bundle_name(value)
    stage = work / "stage" / prefix
    stage.mkdir(parents=True)
    for name, source in layout_files(build).items():
        target = stage / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    checks = validate_stage(stage, build, value)
    archive = work / (prefix + ".zip")
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for name, path in sorted(tree_files(stage).items()):
            bundle.write(path, prefix + "/" + name)
    validate_archive(archive, stage, prefix)
    expanded = work / "expanded"
    with zipfile.ZipFile(archive) as bundle:
        bundle.extractall(expanded)
    validate_stage(expanded / prefix, build, value)
    checks.update({"version": value, "bundle": prefix, "zipSHA256": digest(archive), **recorded})
    (work / "release-evidence.json").write_text(json.dumps(checks, ensure_ascii=False, indent=2) + "\n",
                                               encoding="utf-8")
    checksums = work / "SHA256SUMS"
    checksums.write_text(checks["zipSHA256"] + "  " + archive.name + "\n", encoding="ascii")
    publish_files(archive, checksums, output)
    return {"archive": str(output / archive.name), "sha256": checks["zipSHA256"],
            "stage": str(stage), "expanded": str(expanded / prefix),
            "evidence": str(work / "release-evidence.json")}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["destinations", "record-inputs", "assemble"])
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    if arguments.command == "destinations":
        fresh_destinations(arguments.work, arguments.output)
        print("Release destinations accepted.")
    if arguments.command == "assemble":
        print(json.dumps(assemble(arguments.work / "build", arguments.work, arguments.output), indent=2))
    if arguments.command == "record-inputs":
        (arguments.work / "source-inputs.json").write_text(json.dumps(source_inputs(), indent=2) + "\n",
                                                          encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
