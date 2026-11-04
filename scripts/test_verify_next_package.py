#!/usr/bin/env python3
"""Focused tests for the host-independent Next package verifier."""

from __future__ import annotations

import hashlib
import json
import os
import plistlib
import stat
import tempfile
import unittest
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator
from unittest.mock import patch

from verify_next_package import PackageError, create_package_manifest, verify_package


VERSION = "0.19.0"
QT_VERSION = "6.12.0"


def _write(path: Path, contents: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(contents)


class _StatModeOverlay:
    """Expose fixture mode bits that Windows filesystems do not represent."""

    def __init__(
        self,
        result: os.stat_result,
        mode: int,
        file_attributes: int | None = None,
    ) -> None:
        self._result = result
        self.st_mode = mode
        self.st_file_attributes = (
            file_attributes
            if file_attributes is not None
            else getattr(result, "st_file_attributes", 0)
        )

    def __getattr__(self, name: str) -> object:
        return getattr(self._result, name)


@contextmanager
def _windows_posix_package_semantics(
    root: Path,
    executable_relative_path: str,
    symlink_targets: dict[str, str] | None = None,
    executable_is_marked_executable: bool = True,
) -> Iterator[None]:
    """Model POSIX executable bits and symlinks for package fixtures on Windows."""
    if os.name != "nt":
        yield
        return

    executable = root / executable_relative_path
    symlinks = {root / relative: target for relative, target in (symlink_targets or {}).items()}
    original_stat = Path.stat
    original_lstat = os.lstat
    original_readlink = os.readlink
    original_islink = os.path.islink

    def overlay_mode(result: os.stat_result, mode_bits: int, file_type: int | None = None) -> _StatModeOverlay:
        mode = result.st_mode | mode_bits
        file_attributes = getattr(result, "st_file_attributes", 0)
        if file_type is not None:
            mode = (mode & ~0o170000) | file_type
            if file_type == stat.S_IFLNK:
                file_attributes |= stat.FILE_ATTRIBUTE_REPARSE_POINT
        return _StatModeOverlay(result, mode, file_attributes)

    def stat_with_posix_mode(path: Path, *args: object, **kwargs: object) -> os.stat_result:
        result = original_stat(path, *args, **kwargs)
        if path == executable:
            if executable_is_marked_executable:
                return overlay_mode(result, stat.S_IXUSR)
            mode = result.st_mode & ~(stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
            return _StatModeOverlay(result, mode)
        return result

    def lstat_with_posix_mode(
        path: str | bytes | os.PathLike[str] | os.PathLike[bytes],
        *args: object,
        **kwargs: object,
    ) -> os.stat_result:
        result = original_lstat(path, *args, **kwargs)
        fixture_path = Path(path)
        if fixture_path == executable:
            if executable_is_marked_executable:
                return overlay_mode(result, stat.S_IXUSR)
            mode = result.st_mode & ~(stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
            return _StatModeOverlay(result, mode)
        if fixture_path in symlinks:
            return overlay_mode(result, 0, stat.S_IFLNK)
        return result

    def readlink_with_posix_target(
        path: str | bytes | os.PathLike[str] | os.PathLike[bytes],
        *args: object,
        **kwargs: object,
    ) -> str:
        fixture_path = Path(path)
        if fixture_path in symlinks:
            return symlinks[fixture_path]
        return original_readlink(path, *args, **kwargs)

    def islink_with_posix_target(path: str | bytes | os.PathLike[str] | os.PathLike[bytes]) -> bool:
        if Path(path) in symlinks:
            return True
        return original_islink(path)

    with (
        patch.object(Path, "stat", new=stat_with_posix_mode),
        patch("os.lstat", new=lstat_with_posix_mode),
        patch("os.readlink", new=readlink_with_posix_target),
        patch("os.path.islink", new=islink_with_posix_target),
    ):
        yield


def _make_resource_stage(root: Path) -> None:
    direct_path = "assets/fonts/sample.ttf"
    direct_contents = b"fixture font payload"
    direct_record = {
        "logical_id": "resource:assets/fonts/sample.ttf",
        "package_relative_path": direct_path,
        "location": "resource_file",
        "provenance": {
            "baked": {
                "bytes": len(direct_contents),
                "sha256": hashlib.sha256(direct_contents).hexdigest(),
            }
        },
    }
    records = [direct_record]
    _write(root / direct_path, direct_contents)
    for locale in ("en_US", "ko_KR", "ja_JP", "zh_CN", "zh_TW"):
        relative = f"assets/translations/{locale}.qm"
        _write(root / relative, f"translation:{locale}".encode("ascii"))
        records.append(
            {
                "logical_id": f"translation:{locale}",
                "package_relative_path": relative,
                "location": "resource_file",
                "expected_type": "qm",
                "provenance": None,
            }
        )
    for name in ("EventCalendar.qml", "MonthGridDelegate.qml"):
        relative = f"qt/qml/ClassMngr/Calendar/{name}"
        records.append(
            {
                "logical_id": f"calendar.qml:{name}",
                "package_relative_path": relative,
                "location": "qt_resource",
                "runtime_url": f"qrc:/{relative}",
            }
        )
    catalog = {
        "format": "ClassMngrNext.ResourceCatalog",
        "schema_version": 1,
        "package_path_base": "ClassMngrNext::Resources",
        "records": records,
    }
    _write(
        root / "resource-catalog.json",
        (json.dumps(catalog, sort_keys=True, indent=2) + "\n").encode("utf-8"),
    )


def _make_linux_package(root: Path) -> None:
    executable = root / "bin/ClassMngr"
    _write(executable, b"ClassMngr fake executable")
    executable.chmod(0o755)

    qt_modules = (
        "Core",
        "Gui",
        "Widgets",
        "Sql",
        "Network",
        "Concurrent",
        "Pdf",
        "PdfWidgets",
        "Qml",
        "Quick",
        "QuickControls2",
    )
    for module in qt_modules:
        _write(root / f"lib/libQt6{module}.so.6.12.0", f"{module} library".encode("ascii"))

    _write(root / "lib/plugins/platforms/libqxcb.so", b"platform plugin")
    _write(root / "lib/plugins/sqldrivers/libqsqlite.so", b"sqlite plugin")
    _write(root / "lib/qml/QtQuick/Controls/qmldir", b"module QtQuick.Controls\n")
    _write(root / "lib/qml/QtQuick/Layouts/qmldir", b"module QtQuick.Layouts\n")
    _make_resource_stage(root / "share/ClassMngr")

    license_files = {
        "licenses/fonts/inter/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "licenses/fonts/pretendard/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "licenses/fonts/just-another-hand/LICENSE.txt": "Apache License",
        "licenses/fonts/caveat/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "licenses/fonts/dancing-script/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "licenses/fonts/great-vibes/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "licenses/qt-eventcalendar/BSD-3-Clause.txt": "BSD 3-Clause License",
        "licenses/qt/Qt-LICENSE.txt": "GNU LESSER GENERAL PUBLIC LICENSE Version 3",
        "licenses/qt/Qt-GPL3-LICENSE.txt": "GNU GENERAL PUBLIC LICENSE Version 3",
        "licenses/qt/Qt-GPL3-EXCEPTION.txt": "Qt Company GPL Exception 1.0",
    }
    for relative, contents in license_files.items():
        _write(root / relative, contents.encode("utf-8"))
    for module in ("qtbase", "qtdeclarative", "qtpdf"):
        _write(
            root / f"licenses/qt/sbom/{module}-6.12.0.spdx",
            b"SPDXVersion: SPDX-2.3\nDocumentName: package fixture\n",
        )


def _make_macos_package(root: Path) -> None:
    app = root / "ClassMngr.app"
    executable = app / "Contents/MacOS/ClassMngr"
    _write(executable, b"ClassMngr universal fake executable")
    executable.chmod(0o755)
    info = {
        "CFBundleExecutable": "ClassMngr",
        "CFBundleName": "ClassMngr",
        "CFBundleIdentifier": "com.PaperCloud.ClassMngr",
        "LSMinimumSystemVersion": "14.4",
    }
    plist_path = app / "Contents/Info.plist"
    plist_path.parent.mkdir(parents=True, exist_ok=True)
    plist_path.write_bytes(plistlib.dumps(info))
    _write(app / "Contents/_CodeSignature/CodeResources", b"ad-hoc signature seal")

    qt_modules = (
        "Core",
        "Gui",
        "Widgets",
        "Sql",
        "Network",
        "Concurrent",
        "Pdf",
        "PdfWidgets",
        "Qml",
        "Quick",
        "QuickControls2",
    )
    for module in qt_modules:
        _write(app / f"Contents/Frameworks/Qt{module}.framework/Versions/A/Qt{module}", f"{module} library".encode("ascii"))
    _write(app / "Contents/PlugIns/platforms/libqcocoa.dylib", b"platform plugin")
    _write(app / "Contents/PlugIns/sqldrivers/libqsqlite.dylib", b"sqlite plugin")
    _write(app / "Contents/Resources/qml/QtQuick/Controls/qmldir", b"module QtQuick.Controls\n")
    _write(app / "Contents/Resources/qml/QtQuick/Layouts/qmldir", b"module QtQuick.Layouts\n")
    resource_root = app / "Contents/Resources/ClassMngr"
    _make_resource_stage(resource_root)
    license_root = app / "Contents/Resources/licenses"

    license_files = {
        "fonts/inter/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "fonts/pretendard/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "fonts/just-another-hand/LICENSE.txt": "Apache License",
        "fonts/caveat/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "fonts/dancing-script/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "fonts/great-vibes/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "qt-eventcalendar/BSD-3-Clause.txt": "BSD 3-Clause License",
        "qt/Qt-LICENSE.txt": "GNU LESSER GENERAL PUBLIC LICENSE Version 3",
        "qt/Qt-GPL3-LICENSE.txt": "GNU GENERAL PUBLIC LICENSE Version 3",
        "qt/Qt-GPL3-EXCEPTION.txt": "Qt Company GPL Exception 1.0",
    }
    for relative, contents in license_files.items():
        _write(license_root / relative, contents.encode("utf-8"))
    for module in ("qtbase", "qtdeclarative", "qtpdf"):
        _write(
            license_root / f"qt/sbom/{module}-6.12.0.spdx",
            b"SPDXVersion: SPDX-2.3\nDocumentName: package fixture\n",
        )

    applications_link = root / "Applications"
    if os.name == "nt":
        _write(applications_link, b"macOS Applications symlink fixture")
    else:
        applications_link.symlink_to("/Applications")


def _make_windows_package(root: Path) -> None:
    _write(root / "ClassMngr.exe", b"ClassMngr x64 fake executable")

    qt_modules = (
        "Core",
        "Gui",
        "Widgets",
        "Sql",
        "Network",
        "Concurrent",
        "Pdf",
        "PdfWidgets",
        "Qml",
        "Quick",
        "QuickControls2",
    )
    for module in qt_modules:
        _write(root / f"Qt6{module}.dll", f"{module} library".encode("ascii"))

    _write(root / "plugins/platforms/qwindows.dll", b"platform plugin")
    _write(root / "plugins/sqldrivers/qsqlite.dll", b"sqlite plugin")
    _write(root / "qml/QtQuick/Controls/qmldir", b"module QtQuick.Controls\n")
    _write(root / "qml/QtQuick/Layouts/qmldir", b"module QtQuick.Layouts\n")
    _make_resource_stage(root / "resources")

    license_files = {
        "fonts/inter/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "fonts/pretendard/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "fonts/just-another-hand/LICENSE.txt": "Apache License",
        "fonts/caveat/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "fonts/dancing-script/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "fonts/great-vibes/LICENSE.txt": "SIL OPEN FONT LICENSE Version 1.1",
        "qt-eventcalendar/BSD-3-Clause.txt": "BSD 3-Clause License",
        "qt/Qt-LICENSE.txt": "GNU LESSER GENERAL PUBLIC LICENSE Version 3",
        "qt/Qt-GPL3-LICENSE.txt": "GNU GENERAL PUBLIC LICENSE Version 3",
        "qt/Qt-GPL3-EXCEPTION.txt": "Qt Company GPL Exception 1.0",
    }
    for relative, contents in license_files.items():
        _write(root / f"licenses/{relative}", contents.encode("utf-8"))
    for module in ("qtbase", "qtdeclarative", "qtpdf"):
        _write(
            root / f"licenses/qt/sbom/{module}-6.12.0.spdx",
            b"SPDXVersion: SPDX-2.3\nDocumentName: package fixture\n",
        )


class VerifyNextPackageTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name) / "ClassMngr-linux-x86_64"
        self.root.mkdir()
        _make_linux_package(self.root)
        self.enterContext(
            _windows_posix_package_semantics(self.root, "bin/ClassMngr")
        )

    def _create_manifest(self) -> None:
        create_package_manifest(
            self.root,
            "linux",
            "x86_64",
            VERSION,
            QT_VERSION,
        )

    def test_accepts_complete_linux_next_package(self) -> None:
        manifest = create_package_manifest(
            self.root,
            "linux",
            "x86_64",
            VERSION,
            QT_VERSION,
        )
        result = verify_package(self.root, "linux", "x86_64", VERSION, QT_VERSION)

        self.assertEqual(result["product"], "ClassMngr")
        self.assertEqual(result["updater_assets"], ["ClassMngr-linux-x86_64.tar.gz"])
        self.assertGreater(result["sha256_checked"], 10)
        self.assertEqual(result["resource_catalog"]["hash_checked"], 1)
        self.assertEqual(result["resource_catalog"]["generated_qm"], 5)
        self.assertEqual(result["license_files"], 13)
        qt_license_metadata = {
            entry["path"]: (entry["component"], entry["license"])
            for entry in manifest["license_inventory"]
            if entry["path"].startswith("licenses/qt/Qt-")
        }
        self.assertEqual(
            qt_license_metadata,
            {
                "licenses/qt/Qt-LICENSE.txt": (
                    "Qt runtime and modules (LGPL option)",
                    "LGPL-3.0-only",
                ),
                "licenses/qt/Qt-GPL3-LICENSE.txt": (
                    "Qt modules (GPL option)",
                    "GPL-3.0-only",
                ),
                "licenses/qt/Qt-GPL3-EXCEPTION.txt": (
                    "Qt modules under the GPL option with the Qt GPL exception",
                    "GPL-3.0-only WITH Qt-GPL-exception-1.0",
                ),
            },
        )
        self.assertFalse(result["separate_resource_asset"])

    def test_rejects_non_executable_posix_program(self) -> None:
        executable = self.root / "bin/ClassMngr"
        executable.chmod(0o644)
        with _windows_posix_package_semantics(
            self.root,
            "bin/ClassMngr",
            executable_is_marked_executable=False,
        ):
            self._create_manifest()
            with self.assertRaisesRegex(
                PackageError,
                "ClassMngr executable is not marked executable",
            ):
                verify_package(self.root, "linux", "x86_64", VERSION, QT_VERSION)

    def test_rejects_missing_qt_sqlite_plugin(self) -> None:
        self._create_manifest()
        (self.root / "lib/plugins/sqldrivers/libqsqlite.so").unlink()

        with self.assertRaisesRegex(PackageError, "package paths differ from manifest"):
            verify_package(self.root, "linux", "x86_64", VERSION, QT_VERSION)

    def test_rejects_changed_direct_resource_hash(self) -> None:
        _write(self.root / "share/ClassMngr/assets/fonts/sample.ttf", b"changed direct payload")
        self._create_manifest()

        with self.assertRaisesRegex(PackageError, "resource catalog or direct resource verification"):
            verify_package(self.root, "linux", "x86_64", VERSION, QT_VERSION)

    def test_rejects_unmanifested_extra_path(self) -> None:
        self._create_manifest()
        _write(self.root / "bin/unexpected-helper", b"extra")

        with self.assertRaisesRegex(PackageError, "package paths differ from manifest"):
            verify_package(self.root, "linux", "x86_64", VERSION, QT_VERSION)

    def test_rejects_wrong_expected_qt_version(self) -> None:
        self._create_manifest()

        with self.assertRaisesRegex(PackageError, "qt_version"):
            verify_package(self.root, "linux", "x86_64", VERSION, "6.11.0")

    def test_rejects_separate_legacy_resource_pack(self) -> None:
        _write(self.root / "bin/legacy-resources.rcc", b"legacy pack")
        self._create_manifest()

        with self.assertRaisesRegex(PackageError, "Legacy resource-pack payloads"):
            verify_package(self.root, "linux", "x86_64", VERSION, QT_VERSION)

    def test_accepts_complete_macos_universal_package_with_signed_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir) / "macos-package"
            root.mkdir()
            with _windows_posix_package_semantics(
                root,
                "ClassMngr.app/Contents/MacOS/ClassMngr",
                {"Applications": "/Applications"},
            ):
                _make_macos_package(root)
                manifest = create_package_manifest(
                    root,
                    "macos",
                    "universal",
                    VERSION,
                    QT_VERSION,
                )
                result = verify_package(root, "macos", "universal", VERSION, QT_VERSION)

        self.assertEqual(manifest["manifest_path"], "ClassMngr.package-manifest.json")
        self.assertEqual(manifest["executable"], "ClassMngr.app/Contents/MacOS/ClassMngr")
        self.assertEqual(result["license_files"], 13)
        self.assertGreater(result["sha256_checked"], 10)

    def test_rejects_unapproved_macos_absolute_symlink(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir) / "macos-package"
            root.mkdir()
            with _windows_posix_package_semantics(
                root,
                "ClassMngr.app/Contents/MacOS/ClassMngr",
                {"Applications": "/tmp/Applications"},
            ):
                _make_macos_package(root)
                with self.assertRaisesRegex(
                    PackageError,
                    "package symlink must have a relative target: Applications",
                ):
                    create_package_manifest(
                        root,
                        "macos",
                        "universal",
                        VERSION,
                        QT_VERSION,
                    )

    def test_rejects_unused_macos_sql_driver(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir) / "macos-package"
            root.mkdir()
            with _windows_posix_package_semantics(
                root,
                "ClassMngr.app/Contents/MacOS/ClassMngr",
                {"Applications": "/Applications"},
            ):
                _make_macos_package(root)
                _write(
                    root / "ClassMngr.app/Contents/PlugIns/sqldrivers/libqsqlodbc.dylib",
                    b"unused SQL driver",
                )
                create_package_manifest(root, "macos", "universal", VERSION, QT_VERSION)

                with self.assertRaisesRegex(PackageError, "unused Qt SQL drivers"):
                    verify_package(root, "macos", "universal", VERSION, QT_VERSION)

    def test_accepts_complete_windows_x64_package(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir) / "windows-package"
            root.mkdir()
            _make_windows_package(root)
            manifest = create_package_manifest(root, "windows", "x64", VERSION, QT_VERSION)
            result = verify_package(root, "windows", "x64", VERSION, QT_VERSION)

        self.assertEqual(manifest["executable"], "ClassMngr.exe")
        self.assertEqual(manifest["resource_root"], "resources")
        self.assertEqual(result["updater_assets"], ["ClassMngr-0.19.0-win-x64.exe"])
        self.assertEqual(result["license_files"], 13)
        self.assertGreater(result["sha256_checked"], 10)

    def test_rejects_missing_windows_platform_plugin(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir) / "windows-package"
            root.mkdir()
            _make_windows_package(root)
            (root / "plugins/platforms/qwindows.dll").unlink()
            create_package_manifest(root, "windows", "x64", VERSION, QT_VERSION)

            with self.assertRaisesRegex(PackageError, "Qt platform plugin is missing: qwindows.dll"):
                verify_package(root, "windows", "x64", VERSION, QT_VERSION)

    def test_allows_only_known_inno_uninstaller_files_in_installed_tree_mode(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir) / "windows-package"
            root.mkdir()
            _make_windows_package(root)
            create_package_manifest(root, "windows", "x64", VERSION, QT_VERSION)
            _write(root / "unins000.exe", b"Inno uninstaller")
            _write(root / "unins000.dat", b"Inno uninstall metadata")
            _write(root / "unins000.msg", b"Inno localized uninstaller messages")

            with self.assertRaisesRegex(PackageError, "package paths differ from manifest"):
                verify_package(root, "windows", "x64", VERSION, QT_VERSION)

            result = verify_package(
                root,
                "windows",
                "x64",
                VERSION,
                QT_VERSION,
                allow_inno_uninstaller=True,
            )

        self.assertEqual(
            result["inno_uninstaller_paths_ignored"],
            ["unins000.dat", "unins000.exe", "unins000.msg"],
        )


if __name__ == "__main__":
    unittest.main()
