#!/usr/bin/env python3
"""Create and validate a complete, host-independent ClassMngr Next package manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import plistlib
import re
import stat
import sys
import tempfile
from pathlib import Path, PurePosixPath
from typing import Any

from phase4.audit_resource_stage import StageAuditError, audit_resource_stage


MANIFEST_NAME = "package-manifest.json"
MANIFEST_FORMAT = "ClassMngrNext.PackageManifest"
MANIFEST_SCHEMA_VERSION = 1
VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
QT_VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")

FONT_LICENSES = {
    "licenses/fonts/inter/LICENSE.txt": ("Inter font", "SIL OFL 1.1"),
    "licenses/fonts/pretendard/LICENSE.txt": ("Pretendard font", "SIL OFL 1.1"),
    "licenses/fonts/just-another-hand/LICENSE.txt": ("Just Another Hand font", "Apache-2.0"),
    "licenses/fonts/caveat/LICENSE.txt": ("Caveat font", "SIL OFL 1.1"),
    "licenses/fonts/dancing-script/LICENSE.txt": ("Dancing Script font", "SIL OFL 1.1"),
    "licenses/fonts/great-vibes/LICENSE.txt": ("Great Vibes font", "SIL OFL 1.1"),
}
EVENTCALENDAR_LICENSE = (
    "licenses/qt-eventcalendar/BSD-3-Clause.txt",
    "Qt EventCalendar QML module",
    "BSD-3-Clause",
)
QT_LICENSES = {
    "licenses/qt/Qt-LICENSE.txt": ("Qt runtime and modules", "LGPL-3.0 / GPL-3.0 terms"),
    "licenses/qt/Qt-GPL3-EXCEPTION.txt": (
        "Qt modules covered by the Qt GPL exception",
        "GPL-3.0 WITH Qt-GPL-exception-1.0",
    ),
}
QT_SBOM_MODULES = {"qtbase", "qtdeclarative", "qtpdf"}
INNO_UNINSTALLER_METADATA = {"unins000.exe", "unins000.dat", "unins000.msg"}


class PackageError(ValueError):
    """Raised when a staged package is incomplete or differs from its manifest."""


def _manifest_relative_path(platform: str) -> str:
    if platform == "macos":
        return "ClassMngr.package-manifest.json"
    return MANIFEST_NAME


def _platform_generated_paths(platform: str) -> list[str]:
    return []


def _resource_relative_root(platform: str) -> str:
    if platform == "macos":
        return "ClassMngr.app/Contents/Resources/ClassMngr"
    if platform == "windows":
        return "resources"
    return "share/ClassMngr"


def _license_relative_root(platform: str) -> str:
    if platform == "macos":
        return "ClassMngr.app/Contents/Resources/licenses"
    return "licenses"


def _executable_relative_path(platform: str) -> str:
    return {
        "macos": "ClassMngr.app/Contents/MacOS/ClassMngr",
        "windows": "ClassMngr.exe",
        "linux": "bin/ClassMngr",
    }[platform]


def _updater_asset_name(platform: str, architecture: str, version: str) -> str:
    if platform == "macos":
        return f"ClassMngr-{version}-macos-universal.dmg"
    if platform == "windows":
        return f"ClassMngr-{version}-win-{architecture}.exe"
    return "ClassMngr-linux-x86_64.tar.gz"


def _checked_relative_path(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise PackageError(f"{label} must be a non-empty relative POSIX path")
    if "\\" in value or "\x00" in value or value.startswith("/"):
        raise PackageError(f"{label} is not a canonical relative path: {value!r}")
    parts = value.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise PackageError(f"{label} contains an unsafe path segment: {value!r}")
    pure = PurePosixPath(value)
    if pure.is_absolute() or pure.as_posix() != value:
        raise PackageError(f"{label} is not canonical: {value!r}")
    return value


def _manifest_path(root: Path, platform: str) -> Path:
    return root / _manifest_relative_path(platform)


def _entry_for_path(path: Path, root: Path, platform: str) -> dict[str, Any]:
    relative = path.relative_to(root).as_posix()
    info = path.lstat()
    if stat.S_ISDIR(info.st_mode):
        return {"path": relative, "kind": "directory"}
    if stat.S_ISLNK(info.st_mode):
        target = os.readlink(path)
        if os.path.isabs(target):
            if platform != "macos" or relative != "Applications" or target != "/Applications":
                raise PackageError(f"package symlink must have a relative target: {relative}")
            target_bytes = os.fsencode(target)
            return {
                "path": relative,
                "kind": "symlink",
                "target": target,
                "sha256": hashlib.sha256(target_bytes).hexdigest(),
            }
        try:
            resolved_target = path.resolve(strict=False)
        except (OSError, RuntimeError) as error:
            raise PackageError(f"cannot safely resolve package symlink {relative}: {error}") from error
        try:
            resolved_target.relative_to(root)
        except ValueError as error:
            raise PackageError(f"package symlink escapes its root: {relative} -> {target}") from error
        target_bytes = os.fsencode(target)
        return {
            "path": relative,
            "kind": "symlink",
            "target": target,
            "sha256": hashlib.sha256(target_bytes).hexdigest(),
        }
    if not stat.S_ISREG(info.st_mode):
        raise PackageError(f"package contains a special filesystem entry: {relative}")
    try:
        contents = path.read_bytes()
    except OSError as error:
        raise PackageError(f"cannot read package file {relative}: {error}") from error
    return {
        "path": relative,
        "kind": "file",
        "bytes": len(contents),
        "sha256": hashlib.sha256(contents).hexdigest(),
        "executable": bool(info.st_mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)),
    }


def _package_entries(
    root: Path, manifest_relative_path: str, platform: str
) -> list[dict[str, Any]]:
    entries: list[dict[str, Any]] = []
    for path in root.rglob("*"):
        relative = path.relative_to(root).as_posix()
        if relative == manifest_relative_path:
            continue
        entries.append(_entry_for_path(path, root, platform))
    return sorted(entries, key=lambda entry: entry["path"])


def _license_inventory(root: Path, platform: str) -> list[dict[str, str]]:
    prefix = _license_relative_root(platform)
    inventory: list[dict[str, str]] = []
    for relative, (component, license_name) in FONT_LICENSES.items():
        path = f"{prefix}/{relative.removeprefix('licenses/')}"
        inventory.append({"path": path, "component": component, "license": license_name})

    relative, component, license_name = EVENTCALENDAR_LICENSE
    inventory.append(
        {
            "path": f"{prefix}/{relative.removeprefix('licenses/')}",
            "component": component,
            "license": license_name,
        }
    )
    for relative, (component, license_name) in QT_LICENSES.items():
        inventory.append(
            {
                "path": f"{prefix}/{relative.removeprefix('licenses/')}",
                "component": component,
                "license": license_name,
            }
        )

    qt_sbom_root = root / prefix / "qt/sbom"
    for path in sorted(qt_sbom_root.glob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        module = next((name for name in QT_SBOM_MODULES if path.name.startswith(name + "-")), None)
        if module is None:
            raise PackageError(f"unexpected Qt SBOM package was staged: {path.name}")
        inventory.append(
            {
                "path": f"{prefix}/qt/sbom/{path.name}",
                "component": f"Qt {module} runtime and third-party component inventory",
                "license": "SPDX or CycloneDX SBOM",
            }
        )

    inventory.sort(key=lambda entry: entry["path"])
    _validate_license_inventory(root, platform, inventory)
    return inventory


def _read_regular_file(path: Path, label: str) -> bytes:
    try:
        info = path.lstat()
    except OSError as error:
        raise PackageError(f"missing {label} {path}: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise PackageError(f"{label} must be a regular, non-symlink file: {path}")
    try:
        return path.read_bytes()
    except OSError as error:
        raise PackageError(f"cannot read {label} {path}: {error}") from error


def _validate_license_inventory(
    root: Path, platform: str, inventory: list[dict[str, str]]
) -> None:
    prefix = _license_relative_root(platform)
    expected: dict[str, tuple[str, str]] = {}
    for relative, metadata in FONT_LICENSES.items():
        expected[f"{prefix}/{relative.removeprefix('licenses/')}"] = metadata
    relative, component, license_name = EVENTCALENDAR_LICENSE
    expected[f"{prefix}/{relative.removeprefix('licenses/')}"] = (component, license_name)
    for relative, metadata in QT_LICENSES.items():
        expected[f"{prefix}/{relative.removeprefix('licenses/')}"] = metadata

    actual_paths = {entry.get("path") for entry in inventory}
    if len(actual_paths) != len(inventory):
        raise PackageError("license inventory contains duplicate paths")
    for path, metadata in expected.items():
        if path not in actual_paths:
            raise PackageError(f"license inventory is missing required notice {path}")
        entry = next(entry for entry in inventory if entry.get("path") == path)
        if (entry.get("component"), entry.get("license")) != metadata:
            raise PackageError(f"license inventory has incorrect metadata for {path}")
        contents = _read_regular_file(root / path, "license notice")
        if not contents.strip():
            raise PackageError(f"license notice is empty: {path}")

    sbom_paths: dict[str, list[str]] = {module: [] for module in QT_SBOM_MODULES}
    for entry in inventory:
        path = entry.get("path")
        if not isinstance(path, str):
            raise PackageError("license inventory contains an invalid path")
        if path.startswith(f"{prefix}/qt/sbom/"):
            name = PurePosixPath(path).name
            module = next((value for value in QT_SBOM_MODULES if name.startswith(value + "-")), None)
            if module is None:
                raise PackageError(f"license inventory contains an unexpected Qt SBOM: {path}")
            if entry.get("component") != f"Qt {module} runtime and third-party component inventory":
                raise PackageError(f"license inventory has incorrect Qt SBOM metadata for {path}")
            if entry.get("license") != "SPDX or CycloneDX SBOM":
                raise PackageError(f"license inventory has incorrect Qt SBOM license label for {path}")
            contents = _read_regular_file(root / path, "Qt SBOM")
            if name.endswith(".spdx") and b"SPDXVersion:" not in contents[:512]:
                raise PackageError(f"Qt SPDX SBOM has no SPDX header: {path}")
            if name.endswith(".spdx.json") or name.endswith(".cdx.json"):
                try:
                    value = json.loads(contents)
                except (UnicodeDecodeError, json.JSONDecodeError) as error:
                    raise PackageError(f"Qt SBOM is not valid JSON: {path}: {error}") from error
                if not isinstance(value, dict):
                    raise PackageError(f"Qt SBOM JSON must contain an object: {path}")
            sbom_paths[module].append(path)
    for module, paths in sbom_paths.items():
        if not any(path.endswith(".spdx") or path.endswith(".spdx.json") for path in paths):
            raise PackageError(f"license inventory has no Qt SPDX SBOM for {module}")

    license_root = root / prefix
    actual_license_files: set[str] = set()
    if not license_root.is_dir():
        raise PackageError(f"license directory is missing: {license_root}")
    for path in license_root.rglob("*"):
        if path.is_symlink():
            raise PackageError(f"license tree contains a symlink: {path.relative_to(root)}")
        if path.is_file():
            actual_license_files.add(path.relative_to(root).as_posix())
    if actual_license_files != actual_paths:
        missing = sorted(actual_paths - actual_license_files)
        unexpected = sorted(actual_license_files - actual_paths)
        raise PackageError(
            f"license inventory differs from staged notices; missing={missing}, unexpected={unexpected}"
        )


def _check_platform_architecture(platform: str, architecture: str) -> None:
    allowed = {
        "macos": {"universal"},
        "windows": {"x64", "arm64"},
        "linux": {"x86_64"},
    }
    if platform not in allowed:
        raise PackageError(f"unsupported platform {platform!r}")
    if architecture not in allowed[platform]:
        raise PackageError(f"unsupported {platform} architecture {architecture!r}")


def create_package_manifest(
    package_root: Path,
    platform: str,
    architecture: str,
    version: str,
    qt_version: str,
) -> dict[str, Any]:
    _check_platform_architecture(platform, architecture)
    if not VERSION_PATTERN.fullmatch(version):
        raise PackageError(f"invalid application version {version!r}")
    if not QT_VERSION_PATTERN.fullmatch(qt_version):
        raise PackageError(f"invalid Qt version {qt_version!r}")
    try:
        root = package_root.resolve(strict=True)
    except OSError as error:
        raise PackageError(f"cannot resolve package root {package_root}: {error}") from error
    if not root.is_dir():
        raise PackageError(f"package root is not a directory: {root}")

    manifest_relative_path = _manifest_relative_path(platform)
    manifest_path = root / manifest_relative_path
    if manifest_path.exists() or manifest_path.is_symlink():
        manifest_path.unlink()
    license_inventory = _license_inventory(root, platform)
    manifest = {
        "format": MANIFEST_FORMAT,
        "schema_version": MANIFEST_SCHEMA_VERSION,
        "product": "ClassMngr",
        "version": version,
        "platform": platform,
        "architecture": architecture,
        "qt_version": qt_version,
        "updater_assets": [_updater_asset_name(platform, architecture, version)],
        "executable": _executable_relative_path(platform),
        "resource_root": _resource_relative_root(platform),
        "manifest_path": manifest_relative_path,
        "platform_generated_paths": _platform_generated_paths(platform),
        "license_inventory": license_inventory,
        "entries": _package_entries(root, manifest_relative_path, platform),
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    payload = (json.dumps(manifest, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode(
        "utf-8"
    )
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=manifest_path.parent, prefix=f".{MANIFEST_NAME}.", delete=False
        ) as stream:
            temp_path = Path(stream.name)
            stream.write(payload)
        os.replace(temp_path, manifest_path)
    except OSError as error:
        raise PackageError(f"cannot write package manifest {manifest_path}: {error}") from error
    return manifest


def _load_manifest(root: Path, platform: str) -> dict[str, Any]:
    path = _manifest_path(root, platform)
    raw = _read_regular_file(path, "package manifest")
    try:
        manifest = json.loads(raw)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise PackageError(f"package manifest is not valid UTF-8 JSON: {error}") from error
    if (
        not isinstance(manifest, dict)
        or manifest.get("format") != MANIFEST_FORMAT
        or manifest.get("schema_version") != MANIFEST_SCHEMA_VERSION
    ):
        raise PackageError("package manifest has an unsupported format or schema")
    return manifest


def _validate_entries(
    root: Path,
    manifest: dict[str, Any],
    platform: str,
    allow_inno_uninstaller: bool,
) -> tuple[int, list[str]]:
    entries = manifest.get("entries")
    if not isinstance(entries, list):
        raise PackageError("package manifest entries must be an array")
    manifest_relative_path = _manifest_relative_path(platform)
    expected: dict[str, dict[str, Any]] = {}
    for entry in entries:
        if not isinstance(entry, dict):
            raise PackageError("package manifest entries must be objects")
        relative = _checked_relative_path(entry.get("path"), "package entry path")
        if relative == manifest_relative_path:
            raise PackageError("package manifest must not list itself")
        if relative in expected:
            raise PackageError(f"package manifest repeats path {relative}")
        kind = entry.get("kind")
        if kind not in {"directory", "file", "symlink"}:
            raise PackageError(f"package entry has an invalid kind: {relative}")
        expected[relative] = entry

    actual_paths = {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.relative_to(root).as_posix() != manifest_relative_path
    }
    ignored_installer_paths: list[str] = []
    if allow_inno_uninstaller and platform == "windows":
        for relative in sorted(actual_paths):
            if relative not in INNO_UNINSTALLER_METADATA:
                continue
            path = root / relative
            info = path.lstat()
            if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
                raise PackageError(f"Inno Setup metadata must be a regular file: {relative}")
            ignored_installer_paths.append(relative)
        actual_paths -= set(ignored_installer_paths)
    if actual_paths != set(expected):
        missing = sorted(set(expected) - actual_paths)
        unexpected = sorted(actual_paths - set(expected))
        raise PackageError(f"package paths differ from manifest; missing={missing}, unexpected={unexpected}")

    hash_checked = 0
    for relative, entry in expected.items():
        path = root / relative
        actual = _entry_for_path(path, root, platform)
        if actual != entry:
            raise PackageError(f"package content differs from manifest for {relative}")
        if entry["kind"] == "file":
            hash_checked += 1
    return hash_checked, ignored_installer_paths


def _check_required_runtime(root: Path, platform: str) -> list[str]:
    folded_paths = [path.relative_to(root).as_posix().casefold() for path in root.rglob("*")]
    folded_names = [PurePosixPath(path).name.casefold() for path in folded_paths]

    required_modules = {
        "core": "qtcore",
        "gui": "qtgui",
        "widgets": "qtwidgets",
        "sql": "qtsql",
        "network": "qtnetwork",
        "concurrent": "qtconcurrent",
        "pdf": "qtpdf",
        "pdfwidgets": "qtpdfwidgets",
        "qml": "qtqml",
        "quick": "qtquick",
        "quickcontrols2": "qtquickcontrols2",
    }
    for module, token in required_modules.items():
        versioned_token = token.replace("qt", "qt6", 1)
        if not any(
            token in name or versioned_token in name for name in folded_names
        ):
            raise PackageError(f"Qt runtime module is missing from the package: {module}")

    platform_plugin_names = {
        "macos": "libqcocoa.dylib",
        "windows": "qwindows.dll",
        "linux": "libqxcb.so",
    }
    platform_plugin = platform_plugin_names[platform]
    if not any("/platforms/" in f"/{path}" and path.endswith(platform_plugin) for path in folded_paths):
        raise PackageError(f"Qt platform plugin is missing: {platform_plugin}")
    sqlite_extensions = {"macos": ".dylib", "windows": ".dll", "linux": ".so"}
    sqlite_plugin = f"qsqlite{sqlite_extensions[platform]}"
    if not any("/sqldrivers/" in f"/{path}" and path.endswith(sqlite_plugin) for path in folded_paths):
        raise PackageError(f"Qt SQLite SQL driver plugin is missing: {sqlite_plugin}")
    if platform == "macos":
        unexpected_sql_drivers = {
            "libqsqlmimer.dylib",
            "libqsqlodbc.dylib",
            "libqsqlpsql.dylib",
        }
        found_unexpected = sorted(
            name for name in unexpected_sql_drivers if name in folded_names
        )
        if found_unexpected:
            raise PackageError(
                "macOS package contains unused Qt SQL drivers: " + ", ".join(found_unexpected)
            )

    for qml_module in ("qtquick/controls", "qtquick/layouts"):
        marker = f"/qml/{qml_module}/"
        if not any(
            marker in f"/{path}" and path.endswith("/qmldir") for path in folded_paths
        ):
            raise PackageError(f"Qt QML runtime import is missing: {qml_module}")

    return [platform_plugin, sqlite_plugin]


def _check_macos_bundle_identity(root: Path) -> None:
    plist_path = root / "ClassMngr.app/Contents/Info.plist"
    try:
        with plist_path.open("rb") as stream:
            plist = plistlib.load(stream)
    except (OSError, plistlib.InvalidFileException) as error:
        raise PackageError(f"macOS app bundle Info.plist is invalid: {error}") from error
    expected = {
        "CFBundleExecutable": "ClassMngr",
        "CFBundleName": "ClassMngr",
        "CFBundleIdentifier": "com.PaperCloud.ClassMngr",
        "LSMinimumSystemVersion": "14.4",
    }
    for key, value in expected.items():
        if plist.get(key) != value:
            raise PackageError(f"macOS app bundle identity has {key}={plist.get(key)!r}; expected {value!r}")


def verify_package(
    package_root: Path,
    platform: str,
    architecture: str,
    version: str,
    qt_version: str,
    allow_inno_uninstaller: bool = False,
) -> dict[str, Any]:
    _check_platform_architecture(platform, architecture)
    try:
        root = package_root.resolve(strict=True)
    except OSError as error:
        raise PackageError(f"cannot resolve package root {package_root}: {error}") from error
    if not root.is_dir():
        raise PackageError(f"package root is not a directory: {root}")

    manifest = _load_manifest(root, platform)
    expected_metadata = {
        "product": "ClassMngr",
        "version": version,
        "platform": platform,
        "architecture": architecture,
        "qt_version": qt_version,
        "updater_assets": [_updater_asset_name(platform, architecture, version)],
        "executable": _executable_relative_path(platform),
        "resource_root": _resource_relative_root(platform),
        "manifest_path": _manifest_relative_path(platform),
        "platform_generated_paths": _platform_generated_paths(platform),
    }
    for key, value in expected_metadata.items():
        if manifest.get(key) != value:
            raise PackageError(f"package manifest has {key}={manifest.get(key)!r}; expected {value!r}")

    hash_checked, ignored_installer_paths = _validate_entries(
        root,
        manifest,
        platform,
        allow_inno_uninstaller,
    )
    executable = root / _executable_relative_path(platform)
    executable_contents = _read_regular_file(executable, "ClassMngr executable")
    if not executable_contents:
        raise PackageError(f"ClassMngr executable is empty: {executable}")
    if platform != "windows" and not executable.stat().st_mode & (
        stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH
    ):
        raise PackageError("ClassMngr executable is not marked executable")

    if platform == "macos":
        _check_macos_bundle_identity(root)
        signature_resources = root / "ClassMngr.app/Contents/_CodeSignature/CodeResources"
        if not signature_resources.is_file() or signature_resources.stat().st_size == 0:
            raise PackageError("macOS app bundle has no non-empty ad-hoc signature resource seal")
        applications_link = root / "Applications"
        if not applications_link.is_symlink() or os.readlink(applications_link) != "/Applications":
            raise PackageError("macOS package is missing its /Applications shortcut")

    resource_root = root / _resource_relative_root(platform)
    try:
        resource_summary = audit_resource_stage(resource_root)
    except (StageAuditError, OSError) as error:
        raise PackageError(f"Next resource catalog or direct resource verification failed: {error}") from error

    path_names = [path.relative_to(root).as_posix().casefold() for path in root.rglob("*")]
    forbidden_resource_assets = [
        path for path in path_names if path.endswith(".rcc") or "resource-packs/" in path
    ]
    if forbidden_resource_assets:
        raise PackageError(
            "Next package contains Legacy resource-pack payloads: "
            + ", ".join(sorted(forbidden_resource_assets))
        )

    license_inventory = manifest.get("license_inventory")
    if not isinstance(license_inventory, list) or not all(
        isinstance(entry, dict) for entry in license_inventory
    ):
        raise PackageError("package manifest license_inventory must be an array of objects")
    _validate_license_inventory(root, platform, license_inventory)
    required_plugins = _check_required_runtime(root, platform)

    return {
        "format": "ClassMngrNext.PackageVerification",
        "schema_version": 1,
        "product": "ClassMngr",
        "version": version,
        "platform": platform,
        "architecture": architecture,
        "qt_version": qt_version,
        "updater_assets": manifest["updater_assets"],
        "package_root": str(root),
        "manifest_entries": len(manifest["entries"]),
        "sha256_checked": hash_checked,
        "inno_uninstaller_paths_ignored": ignored_installer_paths,
        "resource_catalog": resource_summary,
        "required_plugins": required_plugins,
        "license_files": len(license_inventory),
        "separate_resource_asset": False,
    }


def _write_report(path: Path, report: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for command in ("create", "verify"):
        subparser = subparsers.add_parser(command)
        subparser.add_argument("--package-root", type=Path, required=True)
        subparser.add_argument("--platform", choices=("macos", "windows", "linux"), required=True)
        subparser.add_argument("--architecture", required=True)
        subparser.add_argument("--version", required=True)
        subparser.add_argument("--qt-version", required=True)
        if command == "verify":
            subparser.add_argument("--report", type=Path)
            subparser.add_argument(
                "--allow-inno-uninstaller",
                action="store_true",
                help="Allow only Inno Setup's generated unins000 metadata files in an installed Windows tree.",
            )
    args = parser.parse_args(argv)

    try:
        if args.command == "create":
            manifest = create_package_manifest(
                args.package_root,
                args.platform,
                args.architecture,
                args.version,
                args.qt_version,
            )
            print(
                f"Created {MANIFEST_FORMAT} for {args.platform}/{args.architecture}: "
                f"{len(manifest['entries'])} package entries, "
                f"{len(manifest['license_inventory'])} license inventory records."
            )
            return 0

        report = verify_package(
            args.package_root,
            args.platform,
            args.architecture,
            args.version,
            args.qt_version,
            args.allow_inno_uninstaller,
        )
        if args.report:
            _write_report(args.report, report)
        print(
            f"Verified Next {args.platform}/{args.architecture} package: "
            f"{report['manifest_entries']} paths, {report['sha256_checked']} SHA-256 hashes, "
            f"{report['resource_catalog']['hash_checked']} direct resource hashes, "
            f"{report['license_files']} license/SBOM files."
        )
        return 0
    except (PackageError, OSError, json.JSONDecodeError, ValueError) as error:
        print(f"Next package verification failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
