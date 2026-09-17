#!/usr/bin/env python3
"""Check that resource-pack declarations, references, and RCC files agree."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


MANAGER_ROW = re.compile(
    r'\{\s*QStringLiteral\("([a-z0-9-]+)"\)\s*,\s*'
    r'Version\s*\([^)]*\)\s*,\s*(true|false)\s*\}',
    re.DOTALL,
)
RESOURCE_PACK_REFERENCE = re.compile(
    r'\b(?:acquirePack|activePackPath)\s*\(\s*'
    r'QStringLiteral\("([a-z0-9-]+)"\)',
    re.DOTALL,
)
DIRECT_MANAGER_REFERENCE = re.compile(
    r'ResourcePackManager::instance\(\)\s*\.\s*'
    r'(?:acquire|activeRoot|currentVersion|isMounted)\s*\(\s*'
    r'QStringLiteral\("([a-z0-9-]+)"\)',
    re.DOTALL,
)
STARTUP_PACK_LIST = re.compile(
    r'resourcePackIds\s*\{(.*?)\}\s*;',
    re.DOTALL,
)
OPTIONAL_STARTUP_PACK = re.compile(
    r'optionalPack\s*=\s*packId\s*==\s*QStringLiteral\("([a-z0-9-]+)"\)'
)
STRING_LITERAL = re.compile(r'QStringLiteral\("([a-z0-9-]+)"\)')


def read_manager_definitions(source: str) -> dict[str, bool]:
    match = re.search(r"m_definitions\s*\(\s*\{(.*?)\}\s*\)", source, re.DOTALL)
    if match is None:
        raise ValueError("Could not read ResourcePackManager definitions")
    rows = MANAGER_ROW.findall(match.group(1))
    if not rows:
        raise ValueError("ResourcePackManager definitions contain no pack entries")
    definitions: dict[str, bool] = {}
    for pack_id, updateable in rows:
        if pack_id in definitions:
            raise ValueError(f"ResourcePackManager defines '{pack_id}' more than once")
        definitions[pack_id] = updateable == "true"
    return definitions


def checked_packs(
    repository: Path,
    build_dir: Path,
    packaged_directory: Path | None,
) -> tuple[dict[str, Any], list[str]]:
    manager_source = (
        repository / "src/core/resource_packs/resource_pack_manager.cpp"
    ).read_text(encoding="utf-8")
    path_source = (repository / "src/core/resource_paths.h").read_text(
        encoding="utf-8"
    )
    startup_source = (repository / "src/main.cpp").read_text(encoding="utf-8")

    errors: list[str] = []
    definitions = read_manager_definitions(manager_source)
    manager_ids = set(definitions)

    startup_match = STARTUP_PACK_LIST.search(startup_source)
    if startup_match is None:
        errors.append("Startup resource-pack list was not found in src/main.cpp")
        startup_ids: list[str] = []
    else:
        startup_ids = STRING_LITERAL.findall(startup_match.group(1))
        if len(startup_ids) != len(set(startup_ids)):
            errors.append("Startup resource-pack list contains duplicate IDs")
        if set(startup_ids) != manager_ids:
            errors.append(
                "Startup resource-pack list differs from ResourcePackManager: "
                f"startup={sorted(startup_ids)}, manager={sorted(manager_ids)}"
            )
    optional_startup_ids = set(OPTIONAL_STARTUP_PACK.findall(startup_source))

    path_references = set(RESOURCE_PACK_REFERENCE.findall(path_source))
    direct_manager_references: dict[str, list[str]] = {}
    for source_path in sorted(
        path for path in (repository / "src").rglob("*")
        if path.is_file() and path.suffix.lower() in {".cpp", ".h"}
    ):
        source_text = source_path.read_text(encoding="utf-8")
        for pack_id in DIRECT_MANAGER_REFERENCE.findall(source_text):
            direct_manager_references.setdefault(pack_id, []).append(
                source_path.relative_to(repository).as_posix()
            )

    referenced_ids = path_references | set(direct_manager_references)
    unknown_references = referenced_ids - manager_ids
    if unknown_references:
        errors.append(
            "ResourcePaths references unknown resource packs: "
            + ", ".join(sorted(unknown_references))
        )

    manifest_path = build_dir / "reports/resource-pack-manifest.json"
    if not manifest_path.is_file():
        errors.append(f"CMake resource-pack manifest is missing: {manifest_path}")
        manifest: dict[str, Any] = {"packs": [], "optional_runtime_ids": []}
    else:
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            errors.append(f"CMake resource-pack manifest is invalid JSON: {error}")
            manifest = {"packs": [], "optional_runtime_ids": []}

    pack_rows = manifest.get("packs", [])
    if not isinstance(pack_rows, list):
        errors.append("CMake resource-pack manifest 'packs' must be an array")
        pack_rows = []
    pack_ids = [row.get("id") for row in pack_rows if isinstance(row, dict)]
    if len(pack_ids) != len(set(pack_ids)):
        errors.append("CMake resource-pack manifest contains duplicate pack IDs")
    invalid_rows = [row for row in pack_rows if not isinstance(row, dict)]
    if invalid_rows:
        errors.append("CMake resource-pack manifest contains a non-object entry")

    generated_ids = set(pack_ids)
    unknown_generated = generated_ids - manager_ids
    if unknown_generated:
        errors.append(
            "CMake generates packs with no ResourcePackManager definition: "
            + ", ".join(sorted(unknown_generated))
        )

    optional_ids_value = manifest.get("optional_runtime_ids", [])
    if not isinstance(optional_ids_value, list) or any(
        not isinstance(pack_id, str) for pack_id in optional_ids_value
    ):
        errors.append("CMake optional_runtime_ids must be an array of strings")
        optional_runtime_ids: set[str] = set()
    else:
        optional_runtime_ids = set(optional_ids_value)

    unknown_optional = optional_runtime_ids - manager_ids
    if unknown_optional:
        errors.append(
            "Optional runtime packs have no ResourcePackManager definition: "
            + ", ".join(sorted(unknown_optional))
        )

    if optional_runtime_ids & generated_ids:
        errors.append(
            "Optional runtime packs must not have baseline RCC files: "
            + ", ".join(sorted(optional_runtime_ids & generated_ids))
        )
    for pack_id in optional_runtime_ids:
        if not definitions.get(pack_id, False):
            errors.append(f"Optional runtime pack '{pack_id}' must be updateable")
    if optional_runtime_ids != optional_startup_ids:
        errors.append(
            "CMake optional runtime IDs differ from startup's optional-pack "
            f"handling: cmake={sorted(optional_runtime_ids)}, "
            f"startup={sorted(optional_startup_ids)}"
        )

    uncovered = manager_ids - generated_ids - optional_runtime_ids
    if uncovered:
        errors.append(
            "ResourcePackManager entries have neither an RCC baseline nor an "
            "optional runtime declaration: "
            + ", ".join(sorted(uncovered))
        )

    build_artifacts: list[dict[str, Any]] = []
    for row in pack_rows:
        if not isinstance(row, dict):
            continue
        pack_id = row.get("id")
        relative_path = row.get("path")
        if not isinstance(pack_id, str) or not isinstance(relative_path, str):
            errors.append("CMake resource-pack entries need string id and path values")
            continue
        expected_relative_path = f"resource-packs/{pack_id}.rcc"
        if relative_path.replace("\\", "/") != expected_relative_path:
            errors.append(
                f"CMake resource pack '{pack_id}' uses unexpected output path "
                f"'{relative_path}'"
            )
        artifact_path = build_dir / relative_path
        if not artifact_path.is_file() or artifact_path.stat().st_size == 0:
            errors.append(f"Built resource pack is missing or empty: {artifact_path}")
            continue
        build_artifacts.append(
            {
                "id": pack_id,
                "path": relative_path.replace("\\", "/"),
                "size_bytes": artifact_path.stat().st_size,
            }
        )

    packaged_artifacts: list[dict[str, Any]] = []
    if packaged_directory is not None:
        for pack_id in sorted(generated_ids):
            artifact_path = packaged_directory / f"{pack_id}.rcc"
            if not artifact_path.is_file() or artifact_path.stat().st_size == 0:
                errors.append(
                    f"Packaged resource pack is missing or empty: {artifact_path}"
                )
                continue
            packaged_artifacts.append(
                {
                    "id": pack_id,
                    "path": artifact_path.name,
                    "size_bytes": artifact_path.stat().st_size,
                }
            )

    report = {
        "schema_version": 1,
        "resource_manager_pack_ids": sorted(manager_ids),
        "resource_path_references": sorted(path_references),
        "referenced_pack_ids": sorted(referenced_ids),
        "direct_manager_references": direct_manager_references,
        "startup_pack_ids": startup_ids,
        "generated_pack_ids": sorted(generated_ids),
        "optional_runtime_ids": sorted(optional_runtime_ids),
        "build_artifacts": build_artifacts,
        "packaged_artifacts": packaged_artifacts,
        "build_total_size_bytes": sum(
            entry["size_bytes"] for entry in build_artifacts
        ),
        "packaged_total_size_bytes": sum(
            entry["size_bytes"] for entry in packaged_artifacts
        ),
    }
    return report, errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--pack-directory", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    repository = args.repository.resolve()
    build_dir = args.build_dir.resolve()
    packaged_directory = (
        args.pack_directory.resolve() if args.pack_directory is not None else None
    )

    try:
        report, errors = checked_packs(repository, build_dir, packaged_directory)
    except (OSError, ValueError) as error:
        print(f"Resource-pack check could not complete: {error}", file=sys.stderr)
        return 2

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    if args.output is not None:
        output = args.output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"Wrote resource-pack reference report: {output}")

    print(
        "Validated "
        f"{len(report['generated_pack_ids'])} RCC packs, "
        f"{len(report['resource_manager_pack_ids'])} runtime IDs, and "
        f"{len(report['referenced_pack_ids'])} runtime references."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
