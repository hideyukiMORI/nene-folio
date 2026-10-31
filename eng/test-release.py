"""Focused release proofs using the existing artifact; never rebuild (QLT-007)."""

import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zipfile
import xml.etree.ElementTree as ET
from unittest.mock import patch
from types import SimpleNamespace
import stat

import release


class ReleaseProofs(unittest.TestCase):
    artifact: Path
    version: dict
    workspace: Path

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="case-", dir=self.workspace)
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)

    def test_actual_artifact(self):
        result = release.validate_identity(self.artifact, self.version)
        self.assertEqual(result["machine"], "AMD64")

    def test_wrong_machine_artifact(self):
        data = bytearray(self.artifact.read_bytes())
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        struct.pack_into("<H", data, pe + 4, 0x14C)
        path = self.folder / "wrong-machine.exe"
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "not AMD64"):
            release.validate_identity(path, self.version)

    def test_wrong_fixed_version(self):
        actual = release.windows_versions(self.artifact)
        actual["fixedFile"][0] ^= 1
        with self.assertRaisesRegex(ValueError, "fixed version differs"):
            release.validate_versions(actual, self.version)

    def test_amd64_import_decode_failure_is_rejected(self):
        failed = subprocess.CompletedProcess(["llvm-readobj"], 1,
                                             stdout="Machine: IMAGE_FILE_MACHINE_AMD64\n",
                                             stderr="import decoding failed")
        with patch.object(release.subprocess, "run", return_value=failed):
            with self.assertRaises(subprocess.CalledProcessError):
                release.validate_identity(self.artifact, self.version)

    def test_wrong_string_version(self):
        actual = release.windows_versions(self.artifact)
        actual["ProductVersion"] = "unrelated version"
        with self.assertRaisesRegex(ValueError, "string version differs"):
            release.validate_versions(actual, self.version)

    def test_undeclared_import(self):
        with self.assertRaisesRegex(ValueError, "undeclared runtime import"):
            release.validate_imports(["KERNEL32.dll", "VCRUNTIME140.dll"])

    def test_wrong_manifest_version(self):
        changed = copy.deepcopy(self.version)
        changed["windowsVersion"] = "1.2.3.4"
        with self.assertRaisesRegex(ValueError, "manifest identity differs"):
            release.validate_manifest(release.embedded_manifest(self.artifact), changed)

    def test_wrong_uac(self):
        manifest = release.embedded_manifest(self.artifact).replace(b"asInvoker", b"requireAdministrator")
        with self.assertRaisesRegex(ValueError, "UAC manifest differs"):
            release.validate_manifest(manifest, self.version)

    def test_wrong_dpi(self):
        manifest = release.embedded_manifest(self.artifact).replace(b"PerMonitorV2", b"System")
        with self.assertRaisesRegex(ValueError, "DPI manifest differs"):
            release.validate_manifest(manifest, self.version)

    def test_exact_files_then_missing_and_extra(self):
        (self.folder / "expected.txt").write_text("expected", encoding="utf-8")
        release.validate_file_set(self.folder, {"expected.txt"})
        with self.assertRaisesRegex(ValueError, "file set differs"):
            release.validate_file_set(self.folder, {"expected.txt", "missing.txt"})
        (self.folder / "data").mkdir()
        with self.assertRaisesRegex(ValueError, "unexpected empty directory"):
            release.validate_file_set(self.folder, {"expected.txt"})
        (self.folder / "data/note.md").write_text("user data", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "file set differs"):
            release.validate_file_set(self.folder, {"expected.txt"})

    def test_font_integrity_and_restore(self):
        shutil.copytree(release.ROOT / "fonts", self.folder / "fonts")
        release.validate_fonts(self.folder)
        rules = release.read_json(release.ROOT / "eng/conformance-rules.json")
        manifest = release.read_json(release.ROOT / rules["bundledAssets"]["manifest"])
        font = self.folder / "fonts" / manifest["entries"][0]["filename"]
        original = font.read_bytes()
        font.write_bytes(bytes([original[0] ^ 1]) + original[1:])
        with self.assertRaisesRegex(ValueError, "font integrity"):
            release.validate_fonts(self.folder)
        font.write_bytes(original)
        release.validate_fonts(self.folder)
        font.unlink()
        with self.assertRaisesRegex(ValueError, "font integrity"):
            release.validate_fonts(self.folder)

    def test_path_rejections(self):
        existing = self.folder / "existing"
        existing.mkdir()
        with self.assertRaisesRegex(ValueError, "already exists"):
            release.fresh_destinations(existing, self.folder / "output")
        with self.assertRaisesRegex(ValueError, "absolute paths"):
            release.fresh_destinations(Path("relative"), self.folder / "output")
        with self.assertRaisesRegex(ValueError, "overlap"):
            release.fresh_destinations(self.folder / "work", self.folder / "work/output")
        with self.assertRaisesRegex(ValueError, "overlap"):
            release.fresh_destinations(release.ROOT / "new-work", self.folder / "output")
        release.fresh_destinations(self.folder / "work", self.folder / "output")

    def test_reparse_attribute_is_rejected(self):
        with patch.object(Path, "lstat", return_value=SimpleNamespace(st_mode=stat.S_IFDIR,
                                                                     st_file_attributes=0x400)):
            with self.assertRaisesRegex(ValueError, "reparse point"):
                release.reject_reparse(self.folder / "junction/descendant")

    def test_output_failure_is_never_published(self):
        archive, sums = self.folder / "bundle.zip", self.folder / "SHA256SUMS"
        archive.write_bytes(b"archive")
        sums.write_bytes(b"sums")
        output = self.folder / "output"
        original_copy = shutil.copyfile

        def fail_second(source, destination):
            if source == sums:
                raise OSError("intentional second-copy failure")
            return original_copy(source, destination)

        with patch.object(shutil, "copyfile", side_effect=fail_second):
            with self.assertRaisesRegex(OSError, "second-copy failure"):
                release.publish_files(archive, sums, output)
        self.assertFalse(output.exists())
        partial = release.read_json(self.folder / "publish-partial.json")["partial"]
        self.assertTrue((Path(partial) / archive.name).is_file())

    def test_output_publish_is_complete_and_non_overwriting(self):
        archive, sums = self.folder / "bundle.zip", self.folder / "SHA256SUMS"
        archive.write_bytes(b"archive")
        sums.write_bytes(b"sums")
        output = self.folder / "output"
        release.publish_files(archive, sums, output)
        self.assertEqual((output / archive.name).read_bytes(), b"archive")
        self.assertEqual((output / sums.name).read_bytes(), b"sums")
        with self.assertRaisesRegex(ValueError, "new Windows output"):
            release.publish_files(archive, sums, output)

    def test_zip_content_and_path_rejection(self):
        stage = self.folder / "stage"
        stage.mkdir()
        (stage / "file.txt").write_text("original", encoding="ascii")
        archive = self.folder / "bundle.zip"
        with zipfile.ZipFile(archive, "w") as bundle:
            bundle.write(stage / "file.txt", "product/file.txt")
        release.validate_archive(archive, stage, "product")
        with zipfile.ZipFile(archive, "w") as bundle:
            bundle.writestr("product/file.txt", "changed")
        with self.assertRaisesRegex(ValueError, "ZIP content differs"):
            release.validate_archive(archive, stage, "product")
        with zipfile.ZipFile(archive, "w") as bundle:
            bundle.writestr("../file.txt", "outside")
        with self.assertRaisesRegex(ValueError, "ZIP file set differs"):
            release.validate_archive(archive, stage, "product")

    def test_version_metadata_rejections(self):
        path = self.folder / "metadata.json"
        value = copy.deepcopy(self.version)
        value["version"] = "65536.0.0"
        path.write_text(json.dumps(value), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "out of range"):
            release.metadata(path)
        value = copy.deepcopy(self.version)
        value["windowsVersion"] = "1.2.3.4"
        path.write_text(json.dumps(value), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "inconsistent Windows version"):
            release.metadata(path)
        for version, windows in (("1", "1.0.0.0"), ("1.2", "1.2.0.0")):
            value = {"version": version, "windowsVersion": windows, "buildType": "Release"}
            path.write_text(json.dumps(value), encoding="utf-8")
            self.assertEqual(release.metadata(path)["windowsVersion"], windows)

    def test_msix_manifest_identity_and_execution(self):
        tree = ET.fromstring(release.msix_manifest({"windowsVersion": "1.2.3.0"}))
        ns = {"f": "http://schemas.microsoft.com/appx/manifest/foundation/windows10",
              "r": "http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"}
        configured = release.read_json(release.ROOT / "eng/msix.json")["identity"]
        self.assertEqual(tree.find("f:Identity", ns).attrib,
                         {"Name": configured["name"], "Publisher": configured["publisher"],
                          "Version": "1.2.3.0", "ProcessorArchitecture": "x64"})
        self.assertEqual(tree.find("f:Properties/f:PublisherDisplayName", ns).text, configured["publisherDisplayName"])
        app = tree.find("f:Applications/f:Application", ns)
        self.assertEqual(app.get("Executable"), "NeNeFolio.exe")
        uap10 = "{http://schemas.microsoft.com/appx/manifest/uap/windows10/10}"
        self.assertEqual(app.get(uap10 + "RuntimeBehavior"), "packagedClassicApp")
        self.assertEqual(app.get(uap10 + "TrustLevel"), "mediumIL")
        capabilities = tree.find("f:Capabilities", ns)
        self.assertEqual(len(capabilities), 1)
        self.assertEqual(capabilities.find("r:Capability", ns).attrib, {"Name": "runFullTrust"})
        self.assertEqual([node.get("Language") for node in tree.find("f:Resources", ns)], ["ja-JP", "en-US", "zh-CN"])

    def test_msix_store_version_rejections(self):
        for version in ("0.3.0.0", "1.0.0.1", "65536.0.0.0", "1.0", "1.0.0.-1", "１.0.0.0"):
            with self.subTest(version=version), self.assertRaisesRegex(ValueError, "Store version"):
                release.msix_manifest({"windowsVersion": version})

    def test_msix_and_zip_share_payload(self):
        portable = release.layout_files(self.folder)
        installed = release.layout_files(self.folder, "msix")
        self.assertEqual(set(portable), set(installed))
        self.assertNotEqual(portable.pop("README.txt"), installed.pop("README.txt"))
        self.assertEqual(portable, installed)

    def test_msix_identity_xml_escaping(self):
        configuration = release.read_json(release.ROOT / "eng/msix.json")
        configuration["identity"]["publisherDisplayName"] = 'Example & "Writer" <Name>'
        with patch.object(release, "read_json", return_value=configuration):
            tree = ET.fromstring(release.msix_manifest({"windowsVersion": "1.0.0.0"}))
        ns = "{http://schemas.microsoft.com/appx/manifest/foundation/windows10}"
        self.assertEqual(tree.find(ns + "Properties/" + ns + "PublisherDisplayName").text,
                         configuration["identity"]["publisherDisplayName"])

    def test_msix_asset_integrity_and_dimensions(self):
        self.assertEqual(len(release.msix_assets()), 3)
        configuration = release.read_json(release.ROOT / "eng/msix.json")
        for field, value, message in (("width", 43, "dimensions differ"), ("sha256", "0" * 64, "hash differs")):
            changed = copy.deepcopy(configuration)
            changed["assets"][0][field] = value
            with patch.object(release, "read_json", return_value=changed):
                with self.assertRaisesRegex(ValueError, message):
                    release.msix_assets()
        changed = copy.deepcopy(configuration)
        changed["assets"].append(changed["assets"][0])
        with patch.object(release, "read_json", return_value=changed):
            with self.assertRaisesRegex(ValueError, "duplicate MSIX asset"):
                release.msix_assets()

    def test_msix_archive_content_and_block_map_rejections(self):
        stage = self.folder / "stage"
        stage.mkdir()
        data = b"a" * 65536 + b"last block"
        (stage / "payload.txt").write_bytes(data)
        namespace = "{http://schemas.microsoft.com/appx/2010/blockmap}"
        block_map = ET.Element(namespace + "BlockMap", {"HashMethod": "http://www.w3.org/2001/04/xmlenc#sha256"})
        file = ET.SubElement(block_map, namespace + "File", {"Name": "payload.txt", "Size": str(len(data))})
        for block in (data[:65536], data[65536:]):
            ET.SubElement(file, namespace + "Block", {"Hash": base64.b64encode(hashlib.sha256(block).digest()).decode("ascii")})
        archive = self.folder / "fixture.msix"

        def write_bundle(tree, content=data, extra=False):
            with zipfile.ZipFile(archive, "w") as bundle:
                bundle.writestr("payload.txt", content)
                bundle.writestr("[Content_Types].xml", b"<Types/>")
                bundle.writestr("AppxBlockMap.xml", ET.tostring(tree))
                if extra:
                    bundle.writestr("data/private.md", "must not ship")

        write_bundle(block_map)
        release.validate_msix_archive(archive, stage)
        with_file_hash = copy.deepcopy(block_map)
        whole_file = ET.SubElement(with_file_hash[0], "{http://schemas.microsoft.com/appx/2021/blockmap}FileHash",
                                   {"Hash": base64.b64encode(hashlib.sha256(data).digest()).decode("ascii")})
        write_bundle(with_file_hash)
        release.validate_msix_archive(archive, stage)
        whole_file.set("Hash", "incorrect")
        write_bundle(with_file_hash)
        with self.assertRaisesRegex(ValueError, "whole-file hash differs"):
            release.validate_msix_archive(archive, stage)
        write_bundle(block_map, content=b"corrupt")
        with self.assertRaisesRegex(ValueError, "MSIX content differs"):
            release.validate_msix_archive(archive, stage)
        write_bundle(block_map, extra=True)
        with self.assertRaisesRegex(ValueError, "MSIX file set differs"):
            release.validate_msix_archive(archive, stage)
        for attribute, wrong, message in (("HashMethod", "sha1", "must use SHA256"),):
            changed = copy.deepcopy(block_map)
            changed.set(attribute, wrong)
            write_bundle(changed)
            with self.assertRaisesRegex(ValueError, message):
                release.validate_msix_archive(archive, stage)
        changed = copy.deepcopy(block_map)
        changed[0][1].set("Hash", "incorrect")
        write_bundle(changed)
        with self.assertRaisesRegex(ValueError, "block map hash differs"):
            release.validate_msix_archive(archive, stage)
        changed = copy.deepcopy(block_map)
        changed.append(copy.deepcopy(changed[0]))
        write_bundle(changed)
        with self.assertRaisesRegex(ValueError, "block map file set differs"):
            release.validate_msix_archive(archive, stage)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", type=Path, default=release.ROOT / "build/NeNeFolio.exe")
    parser.add_argument("--metadata", type=Path, default=release.ROOT / "build/product-version.json")
    parser.add_argument("--workspace", type=Path, default=Path("D:/NeNeFolio/outputs/release-proofs"))
    arguments = parser.parse_args()
    ReleaseProofs.artifact = arguments.artifact
    ReleaseProofs.version = release.metadata(arguments.metadata)
    if not arguments.workspace.is_absolute():
        raise ValueError("release proofs: workspace requires an absolute path")
    release.reject_reparse(arguments.workspace)
    arguments.workspace.mkdir(parents=True, exist_ok=True)
    ReleaseProofs.workspace = arguments.workspace
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(ReleaseProofs))
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
