#!/usr/bin/env python3
"""Write executable, RCC resource-pack, and linked Qt-module size metrics."""

from __future__ import annotations

import argparse
import json
import platform
import sys
from pathlib import Path
from typing import Any


EXECUTABLE_CANDIDATES = (
    Path("ClassMngr.exe"),
    Path("ClassMngr"),
    Path("bin/ClassMngr"),
    Path("ClassMngr.app/Contents/MacOS/ClassMngr"),
    Path("Contents/MacOS/ClassMngr"),
)
PACK_DIRECTORY_CANDIDATES = (
    Path("resource-packs"),
    Path("resources/resource-packs"),
    Path("bin/resources/resource-packs"),
    Path("Contents/Resources/resource-packs"),
    Path("ClassMngr.app/Contents/Resources/resource-packs"),
)


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object in {path}")
    return value


def relative_to(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return str(path)


def locate_executable(package_root: Path) -> Path | None:
    for relative_path in EXECUTABLE_CANDIDATES:
        candidate = package_root / relative_path
        if candidate.is_file():
            return candidate
    return None


def locate_pack_directory(package_root: Path) -> Path | None:
    for relative_path in PACK_DIRECTORY_CANDIDATES:
        candidate = package_root / relative_path
        if candidate.is_dir():
            return candidate
    return None


def create_report(package_root: Path, build_dir: Path) -> dict[str, Any]:
    executable = locate_executable(package_root)
    if executable is None:
        raise ValueError(f"No ClassMngr executable was found under {package_root}")
    if executable.stat().st_size == 0:
        raise ValueError(f"ClassMngr executable is empty: {executable}")

    pack_manifest = load_json(build_dir / "reports/resource-pack-manifest.json")
    module_report = load_json(build_dir / "reports/qt-module-links.json")
    packs = pack_manifest.get("packs", [])
    targets = module_report.get("targets", {})
    if not isinstance(packs, list) or not isinstance(targets, dict):
        raise ValueError("CMake resource or Qt module report has an invalid shape")

    pack_directory = locate_pack_directory(package_root)
    if pack_directory is None:
        raise ValueError(f"No resource-packs directory was found under {package_root}")

    pack_sizes: list[dict[str, Any]] = []
    expected_pack_ids: set[str] = set()
    for row in packs:
        if not isinstance(row, dict) or not isinstance(row.get("id"), str):
            raise ValueError("CMake resource-pack manifest contains an invalid entry")
        pack_id = row["id"]
        if pack_id in expected_pack_ids:
            raise ValueError(f"Resource-pack manifest repeats '{pack_id}'")
        expected_pack_ids.add(pack_id)
        pack_path = pack_directory / f"{pack_id}.rcc"
        if not pack_path.is_file() or pack_path.stat().st_size == 0:
            raise ValueError(f"Packaged resource pack is missing or empty: {pack_path}")
        pack_sizes.append(
            {
                "id": pack_id,
                "path": relative_to(pack_path, package_root),
                "size_bytes": pack_path.stat().st_size,
            }
        )

    actual_pack_ids = {
        path.stem for path in pack_directory.glob("*.rcc") if path.is_file()
    }
    unexpected_pack_ids = actual_pack_ids - expected_pack_ids
    if unexpected_pack_ids:
        raise ValueError(
            "Package contains undeclared resource packs: "
            + ", ".join(sorted(unexpected_pack_ids))
        )

    classmngr_modules = targets.get("ClassMngr")
    next_modules = targets.get("ClassMngrNext")
    if not isinstance(classmngr_modules, list) or not isinstance(next_modules, list):
        raise ValueError("Qt module report is missing ClassMngr or ClassMngrNext")

    return {
        "schema_version": 1,
        "recorded_platform": platform.platform(),
        "package_root": str(package_root),
        "executable": {
            "path": relative_to(executable, package_root),
            "size_bytes": executable.stat().st_size,
        },
        "resource_packs": pack_sizes,
        "resource_packs_total_size_bytes": sum(
            row["size_bytes"] for row in pack_sizes
        ),
        "linked_qt_modules": {
            "ClassMngr": sorted(set(classmngr_modules)),
            "ClassMngrNext": sorted(set(next_modules)),
            "production_targets": targets,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-root", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    package_root = args.package_root.resolve()
    build_dir = args.build_dir.resolve()
    try:
        report = create_report(package_root, build_dir)
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print(f"Build report could not be written: {error}", file=sys.stderr)
        return 1

    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        f"Wrote build report {output}: executable "
        f"{report['executable']['size_bytes']} bytes, "
        f"{report['resource_packs_total_size_bytes']} bytes across "
        f"{len(report['resource_packs'])} RCC packs."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
