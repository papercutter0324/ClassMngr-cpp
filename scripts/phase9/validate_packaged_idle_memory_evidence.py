#!/usr/bin/env python3
"""Validate source-bound Windows packaged-app idle-memory evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import ntpath
import re
import statistics
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any


TASK_ID = "P9_06_PACKAGED_STARTUP_IDLE_GATE"
MANIFEST_SCHEMA = "classmngr-phase9-packaged-idle-memory-v2"
SUMMARY_SCHEMA = "classmngr-phase9-packaged-idle-memory-summary-v2"
EXPECTED_SCENARIOS = ("empty-workspace", "representative-96-class-workspace")
EXPECTED_FIXTURES = {
    "empty-workspace": ("schema-only-v6-empty", 0),
    "representative-96-class-workspace": ("synthetic-large-startup-96-classes", 96),
}
EXPECTED_FIXTURE_GENERATORS = {
    "empty-workspace": "empty-workspace-fixture-generator",
    "representative-96-class-workspace": "representative-workspace-fixture-generator",
}
REQUIRED_PACKAGE_BUILD_TARGETS = (
    "ClassMngrNextResourcePackage",
    "ClassMngrPhase7NextBlankFixtureTests",
    "ClassMngrStartupPerformanceTests",
)
STARTUP_BOUNDARY_METRICS_SCHEMA = "classmngr-phase9-startup-boundary-metrics-v1"
STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS = 2_000
EXPECTED_STARTUP_BOUNDARIES = (
    {"eventId": "theme-loaded", "kind": "stage", "value": "theme-loaded"},
    {
        "eventId": "inter-font-resource-loaded",
        "kind": "resource",
        "value": "resource:assets/fonts/Inter.ttc",
    },
    {
        "eventId": "pretendard-font-resource-loaded",
        "kind": "resource",
        "value": "resource:assets/fonts/PretendardVariable.ttf",
    },
    {
        "eventId": "required-fonts-loaded",
        "kind": "stage",
        "value": "required-fonts-loaded",
    },
    {"eventId": "shell-created", "kind": "stage", "value": "shell-created"},
)
REQUIRED_SOURCE_FILES = {
    "CMakeLists.txt",
    "CMakePresets.json",
    "docs/qt-rewrite/phase-4-resource-inventory.json",
    "licenses/qt/LGPL-3.0-only.txt",
    "scripts/phase4/bake_resources.py",
    "scripts/phase4/generate_resource_catalog.py",
    "scripts/phase4/generate_resource_inventory.py",
    "scripts/phase9/run_packaged_idle_memory_gate.ps1",
    "scripts/phase9/test_validate_packaged_idle_memory_evidence.py",
    "scripts/phase9/validate_packaged_idle_memory_evidence.py",
    "third_party/sqlite/3.54.1/sqlite3.c",
    "third_party/sqlite/3.54.1/sqlite3.h",
}
THRESHOLD_BYTES = 262_144_000
IDLE_SECONDS = 300
SAMPLE_COUNT = IDLE_SECONDS + 1
SAMPLE_INTERVAL_MIN_SECONDS = 0.75
SAMPLE_INTERVAL_MAX_SECONDS = 1.5
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
REVISION_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
STARTUP_READY_RE = re.compile(
    r"(?:^|\s)ClassMngrNext startup stage=startup-ready outcome=completed "
    r"diagnostic=none diagnostic_stage=none checkpoint=(\{.*\})$"
)
WORKSPACE_OPENED_RE = re.compile(
    r"(?:^|\s)ClassMngrNext startup stage=workspace-opened "
    r"outcome=(?P<outcome>[a-z-]+) diagnostic=(?P<diagnostic>[a-z-]+) "
    r"diagnostic_stage=(?P<diagnosticStage>[a-z-]+) checkpoint=(?P<checkpoint>\{.*\})$"
)
STARTUP_BOUNDARY_STAGE_RE = re.compile(
    r"(?:^|\s)ClassMngrNext startup stage=(?P<stage>[a-z-]+) "
    r"outcome=(?P<outcome>[a-z-]+) diagnostic=(?P<diagnostic>[a-z-]+) "
    r"diagnostic_stage=(?P<diagnosticStage>[a-z-]+) checkpoint=(?P<checkpoint>\{.*\})$"
)
RESOURCE_CHECKPOINT_RE = re.compile(r"^ClassMngrNext resource checkpoint=(\{.*\})$")


class EvidenceValidationError(ValueError):
    """Raised when evidence is incomplete or cannot be trusted."""


def _parse_timestamp(value: Any, label: str) -> datetime:
    if not isinstance(value, str):
        raise EvidenceValidationError(f"{label} must be a UTC timestamp string")
    normalized = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError as error:
        raise EvidenceValidationError(f"{label} is not an ISO timestamp") from error
    if parsed.tzinfo is None or parsed.utcoffset() != timezone.utc.utcoffset(parsed):
        raise EvidenceValidationError(f"{label} must use UTC")
    return parsed.astimezone(timezone.utc)


def _require_object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise EvidenceValidationError(f"{label} must be an object")
    return value


def _require_sha(value: Any, label: str) -> str:
    if not isinstance(value, str) or not SHA256_RE.fullmatch(value):
        raise EvidenceValidationError(f"{label} must be a lowercase SHA-256 digest")
    return value


def _require_positive_int(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise EvidenceValidationError(f"{label} must be a positive integer")
    return value


def _require_nonnegative_int(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise EvidenceValidationError(f"{label} must be a nonnegative integer")
    return value


def _same_windows_path(left: Any, right: Any) -> bool:
    if not isinstance(left, str) or not isinstance(right, str):
        return False
    return ntpath.normcase(ntpath.normpath(left)) == ntpath.normcase(
        ntpath.normpath(right)
    )


def _read_json(path: Path, label: str) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise EvidenceValidationError(f"cannot read {label}: {error}") from error


def _read_jsonl(path: Path, label: str) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8") as stream:
            for line_number, line in enumerate(stream, start=1):
                if not line.strip():
                    raise EvidenceValidationError(
                        f"{label} has an empty line at {line_number}"
                    )
                try:
                    value = json.loads(line)
                except json.JSONDecodeError as error:
                    raise EvidenceValidationError(
                        f"{label} line {line_number} is invalid JSON"
                    ) from error
                rows.append(_require_object(value, f"{label} line {line_number}"))
    except OSError as error:
        raise EvidenceValidationError(f"cannot read {label}: {error}") from error
    return rows


def _startup_boundary_event_id(line: str) -> str | None:
    for boundary in EXPECTED_STARTUP_BOUNDARIES:
        if boundary["kind"] == "stage" and (
            f"ClassMngrNext startup stage={boundary['value']} " in line
        ):
            return str(boundary["eventId"])
    for boundary in EXPECTED_STARTUP_BOUNDARIES:
        if boundary["kind"] != "resource" or boundary["value"] not in line:
            continue
        match = RESOURCE_CHECKPOINT_RE.fullmatch(line)
        if match is None:
            raise EvidenceValidationError(
                f"startup resource boundary {boundary['eventId']} record is malformed"
            )
        try:
            checkpoint = json.loads(match.group(1))
        except json.JSONDecodeError as error:
            raise EvidenceValidationError(
                f"startup resource boundary {boundary['eventId']} JSON is malformed"
            ) from error
        if not isinstance(checkpoint, dict) or checkpoint.get("resource_id") != boundary["value"]:
            raise EvidenceValidationError(
                f"startup resource boundary {boundary['eventId']} identity is malformed"
            )
        return str(boundary["eventId"])
    return None


def _validate_msvc_release_flags(value: Any, label: str) -> dict[str, Any]:
    flags = _require_object(value, label)
    raw = flags.get("raw")
    if not isinstance(raw, str):
        raise EvidenceValidationError(f"{label}.raw must be a string")
    optimization_options = [
        match.group(0).strip()
        for match in re.finditer(r"(?i)(?:^|\s)[/-](?:O1|O2|Ox)(?=\s|$)", raw)
    ]
    if not optimization_options or re.search(r"(?i)(?:^|\s)[/-]Od(?=\s|$)", raw):
        raise EvidenceValidationError(
            f"{label} must include /O1, /O2, or /Ox and must not disable optimization"
        )
    if not re.search(r"(?i)(?:^|\s)[/-]D\s*NDEBUG(?=\s|$)", raw):
        raise EvidenceValidationError(f"{label} must define NDEBUG with /DNDEBUG")
    recorded_options = flags.get("optimizationOptions")
    if recorded_options != optimization_options or flags.get("definesNdebug") is not True:
        raise EvidenceValidationError(f"{label} verified flag summary differs from raw flags")
    return {"raw": raw, "optimizationOptions": optimization_options}


def _canonical_package_sha(files: list[dict[str, Any]]) -> str:
    entries = []
    for index, raw_entry in enumerate(files):
        entry = _require_object(raw_entry, f"package file inventory entry {index}")
        path = entry.get("path")
        digest = entry.get("sha256")
        if not isinstance(path, str) or not path or "\n" in path:
            raise EvidenceValidationError("package file path is invalid")
        normalized_path = path.replace("\\", "/")
        if normalized_path.startswith("/") or any(
            part in ("", ".", "..") for part in normalized_path.split("/")
        ):
            raise EvidenceValidationError("package file path must be relative and normalized")
        _require_sha(digest, "package file sha256")
        size = _require_nonnegative_int(entry.get("sizeBytes"), "package file sizeBytes")
        entries.append(f"{normalized_path}\t{size}\t{digest}")
    return hashlib.sha256(("\n".join(entries) + "\n").encode("utf-8")).hexdigest()


def _canonical_source_sha(files: list[dict[str, Any]], included_roots: list[str]) -> str:
    source_paths: list[str] = []
    for index, raw_entry in enumerate(files):
        entry = _require_object(raw_entry, f"source file inventory entry {index}")
        path = entry.get("path")
        if not isinstance(path, str):
            raise EvidenceValidationError("source file path is invalid")
        normalized_path = path.replace("\\", "/")
        if not any(
            normalized_path == root
            or normalized_path.startswith(root.rstrip("/") + "/")
            for root in included_roots
        ):
            raise EvidenceValidationError("source file inventory contains a path outside includedRoots")
        source_paths.append(normalized_path)
    if len(source_paths) != len(set(source_paths)):
        raise EvidenceValidationError("source file inventory contains duplicate paths")
    return _canonical_package_sha(files)


def _metric_summary(values: list[int]) -> dict[str, int | float]:
    return {"min": min(values), "median": statistics.median(values), "max": max(values)}


def validate_documents(
    manifest: dict[str, Any],
    metrics: list[dict[str, Any]],
    startup_events: list[dict[str, Any]],
    package_files: list[dict[str, Any]],
    source_files: list[dict[str, Any]],
    startup_boundary_metrics: list[dict[str, Any]] | None = None,
    build_log: bytes | None = None,
) -> dict[str, Any]:
    """Validate identity and collection completeness, then evaluate the budget."""
    if manifest.get("schema") != MANIFEST_SCHEMA:
        raise EvidenceValidationError("unexpected manifest schema")
    if manifest.get("taskId") != TASK_ID:
        raise EvidenceValidationError("unexpected taskId")
    if manifest.get("status") != "completed":
        raise EvidenceValidationError("run status is not completed")

    revision = manifest.get("revision")
    if not isinstance(revision, str) or not REVISION_RE.fullmatch(revision):
        raise EvidenceValidationError("revision must be a full Git object ID")
    started = _parse_timestamp(manifest.get("startedAtUtc"), "startedAtUtc")
    completed = _parse_timestamp(manifest.get("completedAtUtc"), "completedAtUtc")
    if completed < started:
        raise EvidenceValidationError("completedAtUtc precedes startedAtUtc")

    candidate_source = _require_object(manifest.get("candidateSource"), "candidateSource")
    source_sha = _require_sha(candidate_source.get("sha256"), "candidateSource.sha256")
    if candidate_source.get("revision") != revision:
        raise EvidenceValidationError("candidate source revision differs from run revision")
    included_roots = candidate_source.get("includedRoots")
    if included_roots != [
        "CMakeLists.txt",
        "CMakePresets.json",
        "cmake",
        "docs/qt-rewrite/phase-4-resource-inventory.json",
        "licenses",
        "resources",
        "scripts/phase4",
        "scripts/phase9",
        "src",
        "tests",
        "third_party",
    ]:
        raise EvidenceValidationError("candidate source includedRoots is missing")
    if candidate_source.get("inventoryFile") != "source-files.json":
        raise EvidenceValidationError("candidate source inventory file identity is unexpected")
    if (
        isinstance(candidate_source.get("fileCount"), bool)
        or not isinstance(candidate_source.get("fileCount"), int)
        or candidate_source.get("fileCount") != len(source_files)
        or not source_files
    ):
        raise EvidenceValidationError("candidate source file inventory is incomplete")
    if _canonical_source_sha(source_files, included_roots) != source_sha:
        raise EvidenceValidationError("candidate source file inventory does not match candidate source SHA")
    source_paths = {
        str(entry.get("path", "")).replace("\\", "/")
        for entry in source_files
        if isinstance(entry, dict)
    }
    missing_source_files = REQUIRED_SOURCE_FILES - source_paths
    if missing_source_files:
        raise EvidenceValidationError(
            "candidate source inventory omits required files: "
            + ", ".join(sorted(missing_source_files))
        )
    if candidate_source.get("sha256AtRunEnd") != source_sha or candidate_source.get("stableDuringRun") is not True:
        raise EvidenceValidationError("candidate source changed during the evidence run")

    configuration = _require_object(manifest.get("configuration"), "configuration")
    if (
        configuration.get("preset") != "windows-x64-release"
        or configuration.get("buildType") != "Release"
        or configuration.get("architecture") != "x64"
        or configuration.get("releasePackage") is not True
    ):
        raise EvidenceValidationError("configuration is not packaged Windows x64 Release")
    cache_path = configuration.get("cmakeCachePath")
    if not isinstance(cache_path, str) or not ntpath.isabs(cache_path):
        raise EvidenceValidationError("CMakeCache path must be absolute")
    if ntpath.basename(cache_path).casefold() != "cmakecache.txt":
        raise EvidenceValidationError("configuration path is not CMakeCache.txt")
    cache_sha = _require_sha(configuration.get("cmakeCacheSha256"), "configuration.cmakeCacheSha256")
    if (
        configuration.get("cmakeCacheSha256AtRunEnd") != cache_sha
        or configuration.get("cmakeCacheStableDuringRun") is not True
    ):
        raise EvidenceValidationError("CMakeCache.txt changed during the evidence run")
    generator = configuration.get("generator")
    if not isinstance(generator, str) or not generator.startswith("Visual Studio"):
        raise EvidenceValidationError("configuration generator is not MSVC Visual Studio")
    release_compiler = _require_object(configuration.get("releaseCompiler"), "configuration.releaseCompiler")
    if (
        release_compiler.get("compiler") != "MSVC"
        or release_compiler.get("generator") != generator
        or release_compiler.get("configuration") != "Release"
        or release_compiler.get("platform") != "x64"
    ):
        raise EvidenceValidationError("Release compiler configuration is not MSVC Release x64")
    c_flags = _validate_msvc_release_flags(
        release_compiler.get("cFlagsRelease"), "configuration.releaseCompiler.cFlagsRelease"
    )
    cxx_flags = _validate_msvc_release_flags(
        release_compiler.get("cxxFlagsRelease"), "configuration.releaseCompiler.cxxFlagsRelease"
    )
    project_path = release_compiler.get("projectPath")
    if (
        not isinstance(project_path, str)
        or not ntpath.isabs(project_path)
        or ntpath.basename(project_path).casefold() != "classmngrnext.vcxproj"
    ):
        raise EvidenceValidationError("generated ClassMngrNext project path is invalid")
    _require_sha(release_compiler.get("projectSha256"), "configuration.releaseCompiler.projectSha256")
    if release_compiler.get("projectOptimizationSetting") not in ("MaxSpeed", "MinSpace"):
        raise EvidenceValidationError("generated Release project does not enable optimization")

    package_build = _require_object(configuration.get("packageBuild"), "configuration.packageBuild")
    if package_build.get("targets") != list(REQUIRED_PACKAGE_BUILD_TARGETS):
        raise EvidenceValidationError("required package and fixture build targets are missing")
    cmake_path = package_build.get("cmakeExecutablePath")
    if (
        not isinstance(cmake_path, str)
        or not ntpath.isabs(cmake_path)
        or ntpath.basename(cmake_path).casefold() != "cmake.exe"
    ):
        raise EvidenceValidationError("package prerequisite CMake executable path is invalid")
    _require_sha(package_build.get("cmakeExecutableSha256"), "configuration.packageBuild.cmakeExecutableSha256")
    command = package_build.get("command")
    if (
        not isinstance(command, list)
        or len(command) != 9
        or not _same_windows_path(command[0], cmake_path)
        or command[1] != "--build"
        or command[3:6] != ["--config", "Release", "--target"]
        or command[6:] != list(REQUIRED_PACKAGE_BUILD_TARGETS)
        or not _same_windows_path(command[2], ntpath.dirname(cache_path))
    ):
        raise EvidenceValidationError("package prerequisite build command is incomplete")
    log_path = package_build.get("logPath")
    if not isinstance(log_path, str) or not ntpath.isabs(log_path) or ntpath.basename(log_path).casefold() != "cmake-build.log":
        raise EvidenceValidationError("package prerequisite build log path is invalid")
    log_sha = _require_sha(package_build.get("logSha256"), "configuration.packageBuild.logSha256")
    if build_log is not None and hashlib.sha256(build_log).hexdigest() != log_sha:
        raise EvidenceValidationError("package prerequisite build log hash does not match")
    if package_build.get("exitCode") != 0 or package_build.get("succeeded") is not True:
        raise EvidenceValidationError("package prerequisites did not build successfully")
    build_started = _parse_timestamp(package_build.get("startedAtUtc"), "packageBuild.startedAtUtc")
    build_completed = _parse_timestamp(package_build.get("completedAtUtc"), "packageBuild.completedAtUtc")
    if build_completed < build_started or build_started < started or build_completed > completed:
        raise EvidenceValidationError("package prerequisite build timestamps are invalid")
    qt_installation = _require_object(manifest.get("qtInstallation"), "qtInstallation")
    qt_prefix = qt_installation.get("prefix")
    qt6_directory = qt_installation.get("qt6Directory")
    if not isinstance(qt_prefix, str) or not ntpath.isabs(qt_prefix):
        raise EvidenceValidationError("Qt installation prefix must be absolute")
    if not isinstance(qt6_directory, str) or not ntpath.isabs(qt6_directory):
        raise EvidenceValidationError("Qt6_DIR must be absolute")
    if not _same_windows_path(
        qt6_directory, ntpath.join(qt_prefix, "lib", "cmake", "Qt6")
    ):
        raise EvidenceValidationError("Qt6_DIR and Qt installation prefix do not match")
    qt_version = qt_installation.get("version")
    if not isinstance(qt_version, str) or not re.fullmatch(r"\d+\.\d+\.\d+", qt_version):
        raise EvidenceValidationError("Qt version is missing or malformed")
    qt_installation_id = _require_sha(qt_installation.get("id"), "qtInstallation.id")
    expected_qt_installation_id = hashlib.sha256(
        f"{qt_prefix}\n{qt_version}\n{qt6_directory}\n".encode("utf-8")
    ).hexdigest()
    if qt_installation_id != expected_qt_installation_id:
        raise EvidenceValidationError("Qt installation identity does not match its prefix and version")
    qt_bin = ntpath.join(qt_prefix, "bin")
    core_dll_path = qt_installation.get("coreDllPath")
    if not _same_windows_path(core_dll_path, ntpath.join(qt_bin, "Qt6Core.dll")):
        raise EvidenceValidationError("Qt6Core.dll does not come from the recorded Qt installation")
    _require_sha(qt_installation.get("coreDllSha256"), "qtInstallation.coreDllSha256")
    windeployqt_path = qt_installation.get("windeployqtPath")
    if not _same_windows_path(windeployqt_path, ntpath.join(qt_bin, "windeployqt.exe")):
        raise EvidenceValidationError("windeployqt does not come from the recorded Qt installation")
    _require_sha(qt_installation.get("windeployqtSha256"), "qtInstallation.windeployqtSha256")
    if qt_installation.get("windeployqtVersion") != qt_version:
        raise EvidenceValidationError("windeployqt version differs from the recorded Qt installation")
    qt_fixture_generators = qt_installation.get("fixtureGenerators")
    if not isinstance(qt_fixture_generators, list) or len(qt_fixture_generators) != 2:
        raise EvidenceValidationError("Qt fixture generator runtime verification is incomplete")
    qt_generators_by_id: dict[str, dict[str, Any]] = {}
    for index, raw_generator in enumerate(qt_fixture_generators):
        generator = _require_object(raw_generator, f"qtInstallation.fixtureGenerators[{index}]")
        generator_id = generator.get("id")
        if not isinstance(generator_id, str) or generator_id in qt_generators_by_id:
            raise EvidenceValidationError("Qt fixture generator IDs are missing or duplicated")
        if generator.get("verifiedSameQtInstallation") is not True:
            raise EvidenceValidationError(f"{generator_id} runtime does not match Qt6_DIR")
        generator_path = generator.get("executablePath")
        if (
            not isinstance(generator_path, str)
            or not ntpath.isabs(generator_path)
            or ntpath.splitext(generator_path)[1].casefold() != ".exe"
        ):
            raise EvidenceValidationError(f"{generator_id} executable identity is invalid")
        if not _same_windows_path(generator.get("runtimeSearchPathPrefix"), qt_bin):
            raise EvidenceValidationError(f"{generator_id} runtime search path differs from Qt6_DIR")
        _require_sha(generator.get("executableSha256"), f"{generator_id}.executableSha256")
        colocated_dlls = generator.get("colocatedQtDlls")
        if not isinstance(colocated_dlls, list):
            raise EvidenceValidationError(f"{generator_id} colocated Qt DLL record is malformed")
        for dll_index, raw_dll in enumerate(colocated_dlls):
            dll = _require_object(raw_dll, f"{generator_id}.colocatedQtDlls[{dll_index}]")
            dll_name = dll.get("name")
            if not isinstance(dll_name, str) or ntpath.basename(dll_name) != dll_name or not dll_name.casefold().startswith("qt6"):
                raise EvidenceValidationError(f"{generator_id} has an invalid colocated Qt DLL name")
            if not isinstance(dll.get("colocatedPath"), str) or not ntpath.isabs(dll["colocatedPath"]):
                raise EvidenceValidationError(f"{generator_id} colocated Qt DLL path is invalid")
            if not _same_windows_path(dll.get("qtPrefixPath"), ntpath.join(qt_bin, dll_name)):
                raise EvidenceValidationError(f"{generator_id} Qt DLL source differs from Qt6_DIR")
            colocated_sha = _require_sha(dll.get("colocatedSha256"), f"{generator_id}.colocatedSha256")
            prefix_sha = _require_sha(dll.get("qtPrefixSha256"), f"{generator_id}.qtPrefixSha256")
            if colocated_sha != prefix_sha:
                raise EvidenceValidationError(f"{generator_id} colocated Qt DLL differs from Qt6_DIR")
        qt_generators_by_id[generator_id] = generator
    if set(qt_generators_by_id) != set(EXPECTED_FIXTURE_GENERATORS.values()):
        raise EvidenceValidationError("Qt fixture generator identity is unexpected")
    if manifest.get("thresholdBytes") != THRESHOLD_BYTES or manifest.get("comparison") != "strictly-less-than":
        raise EvidenceValidationError("manifest working-set threshold contract is incorrect")
    if manifest.get("idleSeconds") != IDLE_SECONDS or manifest.get("sampleIntervalSeconds") != 1:
        raise EvidenceValidationError("manifest idle sampling contract is incorrect")
    advisory = manifest.get("advisoryScenarios")
    if not isinstance(advisory, list) or not any(
        isinstance(item, dict)
        and item.get("id") == "fresh-initial-setup"
        and item.get("budgetScope") == "advisory"
        for item in advisory
    ):
        raise EvidenceValidationError("Fresh Initial Setup must remain advisory")
    operating_system = _require_object(manifest.get("os"), "os")
    if operating_system.get("platform") != "Windows" or operating_system.get(
        "architecture"
    ) != "x64":
        raise EvidenceValidationError("run did not execute on Windows x64")

    application = _require_object(manifest.get("application"), "application")
    executable_path = application.get("canonicalPath")
    if not isinstance(executable_path, str) or not ntpath.isabs(executable_path):
        raise EvidenceValidationError("application canonicalPath must be absolute")
    if ntpath.basename(executable_path).casefold() != "classmngrnext.exe":
        raise EvidenceValidationError("application path is not the staged ClassMngrNext.exe")
    if application.get("architecture") != "x64":
        raise EvidenceValidationError("staged application architecture is not x64")
    application_sha = _require_sha(application.get("sha256"), "application.sha256")
    if application.get("candidateSourceSha256") != source_sha:
        raise EvidenceValidationError("application is not bound to candidate source SHA")
    release_path = application.get("releaseExecutablePath")
    if not isinstance(release_path, str) or not ntpath.isabs(release_path):
        raise EvidenceValidationError("source Release executable path must be absolute")
    if ntpath.basename(release_path).casefold() != "classmngr.exe":
        raise EvidenceValidationError("source Release executable is not packaged ClassMngr.exe")
    if _require_sha(application.get("releaseExecutableSha256"), "application.releaseExecutableSha256") != application_sha:
        raise EvidenceValidationError("staged app image differs from source Release executable")

    package = _require_object(manifest.get("package"), "package")
    if package.get("id") != "classmngr-next-windows-x64-release-stage-v1":
        raise EvidenceValidationError("package identity is unexpected")
    if package.get("candidateSourceSha256") != source_sha:
        raise EvidenceValidationError("package is not bound to candidate source SHA")
    if package.get("applicationSha256") != application_sha:
        raise EvidenceValidationError("package application hash differs from process image")
    if package.get("sourceReleasePackage") is not True:
        raise EvidenceValidationError("package is not bound to the local Release package build")
    if package.get("inventoryFile") != "package-files.json":
        raise EvidenceValidationError("package inventory file identity is unexpected")
    if (
        isinstance(package.get("fileCount"), bool)
        or not isinstance(package.get("fileCount"), int)
        or package.get("fileCount") != len(package_files)
        or not package_files
    ):
        raise EvidenceValidationError("package file inventory is incomplete")
    package_sha = _require_sha(package.get("sha256"), "package.sha256")
    if _canonical_package_sha(package_files) != package_sha:
        raise EvidenceValidationError("package file inventory does not match package SHA")
    app_relative_path = package.get("applicationRelativePath")
    matching_exes = [
        entry
        for entry in package_files
        if isinstance(entry.get("path"), str)
        and entry["path"].replace("/", "\\").casefold()
        == str(app_relative_path).replace("/", "\\").casefold()
    ]
    if len(matching_exes) != 1 or matching_exes[0].get("sha256") != application_sha:
        raise EvidenceValidationError("package inventory does not contain the measured app image")
    file_paths = [entry.get("path") for entry in package_files]
    if len(file_paths) != len(set(file_paths)):
        raise EvidenceValidationError("package inventory contains duplicate paths")

    if len(startup_events) < 1:
        raise EvidenceValidationError("startup log is empty")
    for event in startup_events:
        if not isinstance(event.get("stream"), str) or event.get("stream") not in (
            "stdout",
            "stderr",
        ):
            raise EvidenceValidationError("startup log stream is invalid")
        if not isinstance(event.get("line"), str):
            raise EvidenceValidationError("startup log line is missing")
        _parse_timestamp(event.get("observedAtUtc"), "startup log observedAtUtc")

    boundary_manifest = _require_object(
        manifest.get("startupBoundaryMetrics"), "startupBoundaryMetrics"
    )
    expected_boundary_ids = [
        boundary["eventId"] for boundary in EXPECTED_STARTUP_BOUNDARIES
    ]
    if (
        boundary_manifest.get("schema") != STARTUP_BOUNDARY_METRICS_SCHEMA
        or boundary_manifest.get("inventoryFile") != "startup-boundary-metrics.jsonl"
        or boundary_manifest.get("expectedEventIds") != expected_boundary_ids
        or boundary_manifest.get("samplesPerScenario") != len(EXPECTED_STARTUP_BOUNDARIES)
        or boundary_manifest.get("maxSampleDelayMilliseconds")
        != STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS
    ):
        raise EvidenceValidationError("startup boundary metric contract is missing or malformed")
    if startup_boundary_metrics is None:
        raise EvidenceValidationError("startup boundary metric inventory is missing")
    expected_boundary_rows = [
        (scenario_id, event_id)
        for scenario_id in EXPECTED_SCENARIOS
        for event_id in expected_boundary_ids
    ]
    actual_boundary_rows = [
        (row.get("scenarioId"), row.get("eventId"))
        if isinstance(row, dict)
        else (None, None)
        for row in startup_boundary_metrics
    ]
    if actual_boundary_rows != expected_boundary_rows:
        raise EvidenceValidationError(
            "startup boundary metric rows are missing, duplicated, or out of order"
        )

    scenarios = manifest.get("scenarios")
    if not isinstance(scenarios, list) or tuple(
        scenario.get("id") if isinstance(scenario, dict) else None
        for scenario in scenarios
    ) != EXPECTED_SCENARIOS:
        raise EvidenceValidationError("manifest scenarios are missing, duplicated, or out of order")

    scenario_summaries: list[dict[str, Any]] = []
    violations: list[dict[str, Any]] = []
    consumed_rows: set[int] = set()
    previous_exit: datetime | None = None
    for scenario in scenarios:
        scenario = _require_object(scenario, "scenario")
        scenario_id = scenario["id"]
        if scenario.get("outcome") != "completed":
            raise EvidenceValidationError(f"{scenario_id} outcome is not completed")
        if scenario.get("budgetScope") != "normal-use-gate":
            raise EvidenceValidationError(f"{scenario_id} is not marked as a gated normal-use scenario")
        scenario_started = _parse_timestamp(scenario.get("startedAtUtc"), f"{scenario_id}.startedAtUtc")
        if (
            scenario.get("startupBoundarySampleCount") != len(EXPECTED_STARTUP_BOUNDARIES)
            or scenario.get("startupBoundaryEventIds") != expected_boundary_ids
        ):
            raise EvidenceValidationError(
                f"{scenario_id} manifest startup boundary checkpoints are incomplete"
            )

        fixture = _require_object(scenario.get("fixture"), f"{scenario_id}.fixture")
        fixture_id, class_count = EXPECTED_FIXTURES[scenario_id]
        if fixture.get("id") != fixture_id:
            raise EvidenceValidationError(f"{scenario_id} fixture identity is unexpected")
        _require_sha(fixture.get("sha256"), f"{scenario_id}.fixture.sha256")
        if fixture.get("classCount") != class_count or fixture.get("profilePathArgumentSupplied") is not True:
            raise EvidenceValidationError(f"{scenario_id} fixture shape is incorrect")
        fixture_argument = fixture.get("pathPassedToApp")
        if (
            not isinstance(fixture_argument, str)
            or not ntpath.isabs(fixture_argument)
            or ntpath.splitext(fixture_argument)[1].casefold() != ".tps"
        ):
            raise EvidenceValidationError(f"{scenario_id} exact .tps command-line path is missing")
        generator_id = EXPECTED_FIXTURE_GENERATORS[scenario_id]
        fixture_generator = qt_generators_by_id[generator_id]
        if not _same_windows_path(
            fixture.get("generatorExecutablePath"), fixture_generator.get("executablePath")
        ):
            raise EvidenceValidationError(f"{scenario_id} fixture generator path differs from Qt runtime evidence")
        if _require_sha(fixture.get("generatorExecutableSha256"), f"{scenario_id}.fixture.generatorExecutableSha256") != fixture_generator.get("executableSha256"):
            raise EvidenceValidationError(f"{scenario_id} fixture generator hash differs from Qt runtime evidence")
        if fixture.get("qtInstallationId") != qt_installation_id:
            raise EvidenceValidationError(f"{scenario_id} fixture generator used a different Qt installation")

        process = _require_object(scenario.get("process"), f"{scenario_id}.process")
        pid = _require_positive_int(process.get("pid"), f"{scenario_id}.process.pid")
        created_at_text = process.get("createdAtUtc")
        created_at = _parse_timestamp(created_at_text, f"{scenario_id}.process.createdAtUtc")
        process_path = process.get("canonicalExecutablePath")
        if not _same_windows_path(process_path, executable_path):
            raise EvidenceValidationError(f"{scenario_id} process image differs from staged package")
        if process.get("architecture") != "x64":
            raise EvidenceValidationError(f"{scenario_id} process architecture is not x64")
        if _require_sha(process.get("sha256"), f"{scenario_id}.process.sha256") != application_sha:
            raise EvidenceValidationError(f"{scenario_id} process image hash differs from package")
        readiness = _require_object(scenario.get("readiness"), f"{scenario_id}.readiness")
        if readiness.get("windowVisible") is not True or readiness.get("stablePolls") != 3:
            raise EvidenceValidationError(f"{scenario_id} did not prove a stable visible application window")
        window_handle = _require_positive_int(readiness.get("mainWindowHandle"), f"{scenario_id}.readiness.mainWindowHandle")
        window_title = readiness.get("mainWindowTitle")
        if not isinstance(window_title, str) or "ClassMngr Next" not in window_title:
            raise EvidenceValidationError(f"{scenario_id} has an unexpected application window title")
        if process.get("mainWindowHandle") != window_handle or process.get("mainWindowTitle") != window_title:
            raise EvidenceValidationError(f"{scenario_id} process and readiness window identity differ")

        diagnostic = _require_object(scenario.get("startupReadyDiagnostic"), f"{scenario_id}.startupReadyDiagnostic")
        diagnostic_at = _parse_timestamp(diagnostic.get("observedAtUtc"), f"{scenario_id}.startupReadyDiagnostic.observedAtUtc")
        if diagnostic_at < scenario_started or created_at > diagnostic_at:
            raise EvidenceValidationError(f"{scenario_id} startup-ready lifecycle timestamps are invalid")
        if diagnostic.get("stage") != "startup-ready" or diagnostic.get("outcome") != "completed":
            raise EvidenceValidationError(f"{scenario_id} did not capture a completed startup-ready diagnostic")
        if diagnostic.get("diagnostic") != "none" or diagnostic.get("diagnosticStage") != "none":
            raise EvidenceValidationError(f"{scenario_id} startup-ready diagnostic contains an error")
        checkpoint = _require_object(diagnostic.get("checkpoint"), f"{scenario_id}.startupReadyDiagnostic.checkpoint")
        if checkpoint.get("stage") != "startup-ready" or checkpoint.get("outcome") != "completed":
            raise EvidenceValidationError(f"{scenario_id} startup checkpoint identity is invalid")
        if checkpoint.get("sample_available") is not True or checkpoint.get("platform") != "windows":
            raise EvidenceValidationError(f"{scenario_id} startup checkpoint memory is unavailable")
        startup_memory = _require_object(checkpoint.get("metrics"), f"{scenario_id}.startup checkpoint metrics")
        startup_working_set = _require_positive_int(startup_memory.get("working_set_bytes"), f"{scenario_id}.startup working_set_bytes")
        raw_line = diagnostic.get("rawLine")
        if not isinstance(raw_line, str):
            raise EvidenceValidationError(f"{scenario_id} startup-ready raw line is missing")
        match = STARTUP_READY_RE.search(raw_line)
        if match is None:
            raise EvidenceValidationError(f"{scenario_id} raw line is not the app startup-ready diagnostic")
        try:
            parsed_checkpoint = json.loads(match.group(1))
        except json.JSONDecodeError as error:
            raise EvidenceValidationError(f"{scenario_id} startup-ready checkpoint is malformed") from error
        if parsed_checkpoint != checkpoint:
            raise EvidenceValidationError(f"{scenario_id} parsed checkpoint differs from raw diagnostic")
        if not any(
            event.get("line") == raw_line
            and event.get("scenarioId") == scenario_id
            and event.get("stream") == diagnostic.get("stream")
            and event.get("observedAtUtc") == diagnostic.get("observedAtUtc")
            for event in startup_events
        ):
            raise EvidenceValidationError(f"{scenario_id} startup-ready diagnostic is absent from startup log")

        workspace_diagnostic = _require_object(
            scenario.get("workspaceOpenedDiagnostic"),
            f"{scenario_id}.workspaceOpenedDiagnostic",
        )
        workspace_at = _parse_timestamp(
            workspace_diagnostic.get("observedAtUtc"),
            f"{scenario_id}.workspaceOpenedDiagnostic.observedAtUtc",
        )
        workspace_line = workspace_diagnostic.get("rawLine")
        if not isinstance(workspace_line, str):
            raise EvidenceValidationError(f"{scenario_id} workspace-opened raw line is missing")
        workspace_match = WORKSPACE_OPENED_RE.search(workspace_line)
        if workspace_match is None:
            raise EvidenceValidationError(f"{scenario_id} workspace-opened record is malformed")
        workspace_log_matches = [
            (index, event)
            for index, event in enumerate(startup_events)
            if event.get("scenarioId") == scenario_id
            and isinstance(event.get("line"), str)
            and "ClassMngrNext startup stage=workspace-opened " in event["line"]
        ]
        if len(workspace_log_matches) != 1:
            raise EvidenceValidationError(
                f"{scenario_id} must have exactly one workspace-opened startup record"
            )
        workspace_log_index, workspace_log_event = workspace_log_matches[0]
        if (
            workspace_log_event.get("line") != workspace_line
            or workspace_log_event.get("stream") != workspace_diagnostic.get("stream")
            or workspace_log_event.get("observedAtUtc") != workspace_diagnostic.get("observedAtUtc")
        ):
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened diagnostic differs from captured startup log"
            )
        if (
            workspace_match.group("outcome") != "completed"
            or workspace_match.group("diagnostic") != "none"
            or workspace_match.group("diagnosticStage") != "none"
            or workspace_diagnostic.get("stage") != "workspace-opened"
            or workspace_diagnostic.get("outcome") != "completed"
            or workspace_diagnostic.get("diagnostic") != "none"
            or workspace_diagnostic.get("diagnosticStage") != "none"
        ):
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened record did not complete without a diagnostic"
            )
        try:
            workspace_checkpoint = _require_object(
                json.loads(workspace_match.group("checkpoint")),
                f"{scenario_id}.workspace-opened checkpoint",
            )
        except json.JSONDecodeError as error:
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened checkpoint is malformed"
            ) from error
        if workspace_checkpoint != workspace_diagnostic.get("checkpoint"):
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened checkpoint differs from raw diagnostic"
            )
        if (
            workspace_checkpoint.get("stage") != "workspace-opened"
            or workspace_checkpoint.get("outcome") != "completed"
        ):
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened checkpoint identity is invalid"
            )
        ready_log_matches = [
            index
            for index, event in enumerate(startup_events)
            if event.get("scenarioId") == scenario_id
            and event.get("line") == raw_line
            and event.get("stream") == diagnostic.get("stream")
            and event.get("observedAtUtc") == diagnostic.get("observedAtUtc")
        ]
        if len(ready_log_matches) != 1:
            raise EvidenceValidationError(
                f"{scenario_id} must have exactly one matching startup-ready startup record"
            )
        ready_log_index = ready_log_matches[0]
        if workspace_log_index >= ready_log_index or workspace_at >= diagnostic_at:
            raise EvidenceValidationError(
                f"{scenario_id} workspace-opened record did not precede startup-ready"
            )

        scenario_boundary_rows = [
            row for row in startup_boundary_metrics if row.get("scenarioId") == scenario_id
        ]
        scenario_boundary_logs: dict[str, tuple[int, dict[str, Any]]] = {}
        for boundary_log_index, event in enumerate(startup_events):
            if event.get("scenarioId") != scenario_id:
                continue
            event_line = event.get("line")
            if not isinstance(event_line, str):
                continue
            event_id = _startup_boundary_event_id(event_line)
            if event_id is None:
                continue
            if event_id in scenario_boundary_logs:
                raise EvidenceValidationError(
                    f"{scenario_id} has duplicate {event_id} startup boundary records"
                )
            scenario_boundary_logs[event_id] = (boundary_log_index, event)

        scenario_boundary_summaries: list[dict[str, Any]] = []
        prior_boundary_log_index = -1
        prior_boundary_at: datetime | None = None
        prior_boundary_sample_at: datetime | None = None
        for boundary_spec, boundary_row in zip(
            EXPECTED_STARTUP_BOUNDARIES, scenario_boundary_rows, strict=True
        ):
            event_id = str(boundary_spec["eventId"])
            log_match = scenario_boundary_logs.get(event_id)
            if log_match is None:
                raise EvidenceValidationError(
                    f"{scenario_id} is missing the {event_id} startup boundary log record"
                )
            boundary_log_index, boundary_event = log_match
            if boundary_log_index <= prior_boundary_log_index:
                raise EvidenceValidationError(
                    f"{scenario_id} startup boundary log records are out of order"
                )
            if boundary_log_index >= ready_log_index:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} startup boundary occurred after startup-ready"
                )
            if event_id == "shell-created":
                if boundary_log_index <= workspace_log_index:
                    raise EvidenceValidationError(
                        f"{scenario_id} shell-created startup record did not follow workspace-opened"
                    )
            elif boundary_log_index >= workspace_log_index:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} startup record occurred after workspace-opened"
                )
            prior_boundary_log_index = boundary_log_index

            event_line = str(boundary_event["line"])
            boundary_at_text = boundary_event.get("observedAtUtc")
            boundary_at = _parse_timestamp(
                boundary_at_text, f"{scenario_id}.{event_id}.startupLog.observedAtUtc"
            )
            if prior_boundary_at is not None and boundary_at < prior_boundary_at:
                raise EvidenceValidationError(
                    f"{scenario_id} startup boundary event timestamps are out of order"
                )
            if event_id == "shell-created":
                if boundary_at < workspace_at:
                    raise EvidenceValidationError(
                        f"{scenario_id} shell-created startup timestamp preceded workspace-opened"
                    )
            elif boundary_at > workspace_at:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} startup timestamp followed workspace-opened"
                )
            prior_boundary_at = boundary_at
            if boundary_spec["kind"] == "stage":
                stage_match = STARTUP_BOUNDARY_STAGE_RE.search(event_line)
                if (
                    stage_match is None
                    or stage_match.group("stage") != boundary_spec["value"]
                    or stage_match.group("outcome") != "completed"
                    or stage_match.group("diagnostic") != "none"
                    or stage_match.group("diagnosticStage") != "none"
                ):
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} startup stage record is invalid"
                    )
                try:
                    event_checkpoint = json.loads(stage_match.group("checkpoint"))
                except json.JSONDecodeError as error:
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} startup stage checkpoint is malformed"
                    ) from error
                if (
                    not isinstance(event_checkpoint, dict)
                    or event_checkpoint.get("stage") != boundary_spec["value"]
                    or event_checkpoint.get("outcome") != "completed"
                    or event_checkpoint.get("sample_available") is not True
                    or event_checkpoint.get("platform") != "windows"
                ):
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} startup stage checkpoint identity is invalid"
                    )
            else:
                resource_match = RESOURCE_CHECKPOINT_RE.fullmatch(event_line)
                if resource_match is None:
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} resource checkpoint is malformed"
                    )
                try:
                    event_checkpoint = json.loads(resource_match.group(1))
                except json.JSONDecodeError as error:
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} resource checkpoint JSON is malformed"
                    ) from error
                request_scope = (
                    event_checkpoint.get("request_scope")
                    if isinstance(event_checkpoint, dict)
                    else None
                )
                if (
                    not isinstance(event_checkpoint, dict)
                    or event_checkpoint.get("event") != "resource-load"
                    or event_checkpoint.get("resource_id") != boundary_spec["value"]
                    or event_checkpoint.get("outcome") != "succeeded"
                    or not isinstance(request_scope, dict)
                    or request_scope.get("kind") != "startup"
                ):
                    raise EvidenceValidationError(
                        f"{scenario_id} {event_id} resource startup record is invalid"
                    )

            row_log_index = _require_nonnegative_int(
                boundary_row.get("startupLogIndex"), f"{scenario_id}.{event_id}.startupLogIndex"
            )
            if row_log_index != boundary_log_index:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} boundary snapshot references the wrong startup log record"
                )
            if (
                boundary_row.get("eventObservedAtUtc") != boundary_at_text
                or boundary_row.get("stream") != boundary_event.get("stream")
                or boundary_row.get("rawLine") != event_line
            ):
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} boundary snapshot differs from its startup log record"
                )
            boundary_sample_at = _parse_timestamp(
                boundary_row.get("timestampUtc"), f"{scenario_id}.{event_id}.timestampUtc"
            )
            delay_ms = (boundary_sample_at - boundary_at).total_seconds() * 1000
            recorded_delay = boundary_row.get("sampleDelayMilliseconds")
            recorded_delay_is_finite = False
            if isinstance(recorded_delay, (int, float)) and not isinstance(recorded_delay, bool):
                try:
                    recorded_delay_is_finite = math.isfinite(recorded_delay)
                except OverflowError:
                    recorded_delay_is_finite = False
            if (
                delay_ms < 0
                or delay_ms > STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS
                or isinstance(recorded_delay, bool)
                or not isinstance(recorded_delay, (int, float))
                or not recorded_delay_is_finite
                or recorded_delay < 0
                or abs(recorded_delay - delay_ms) > 1.0
            ):
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} process snapshot was stale or its delay is malformed"
                )
            if prior_boundary_sample_at is not None and boundary_sample_at < prior_boundary_sample_at:
                raise EvidenceValidationError(
                    f"{scenario_id} startup boundary snapshots are out of order"
                )
            prior_boundary_sample_at = boundary_sample_at
            if boundary_sample_at >= diagnostic_at:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} process snapshot occurred after startup-ready"
                )
            if event_id == "shell-created":
                if boundary_sample_at < workspace_at:
                    raise EvidenceValidationError(
                        f"{scenario_id} shell-created process snapshot preceded workspace-opened"
                    )
            elif boundary_sample_at > workspace_at:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} process snapshot followed workspace-opened"
                )

            if (
                boundary_row.get("eventId") != event_id
                or boundary_row.get("phase") != "startup-boundary"
                or boundary_row.get("pid") != pid
                or boundary_row.get("processCreatedAtUtc") != created_at_text
                or not _same_windows_path(boundary_row.get("canonicalExecutablePath"), executable_path)
                or boundary_row.get("architecture") != "x64"
                or _require_sha(boundary_row.get("sha256"), f"{scenario_id}.{event_id}.sha256")
                != application_sha
            ):
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} process identity differs from the packaged app"
                )
            boundary_metrics_values = {
                name: _require_positive_int(
                    boundary_row.get(name), f"{scenario_id}.{event_id}.{name}"
                )
                for name in (
                    "currentWorkingSetBytes",
                    "privateBytes",
                    "peakWorkingSetBytes",
                    "handles",
                    "threads",
                )
            }
            if boundary_metrics_values["peakWorkingSetBytes"] < boundary_metrics_values["currentWorkingSetBytes"]:
                raise EvidenceValidationError(
                    f"{scenario_id} {event_id} peak working set is below current working set"
                )
            scenario_boundary_summaries.append(
                {
                    "eventId": event_id,
                    "startupLogIndex": boundary_log_index,
                    "eventObservedAtUtc": boundary_at_text,
                    "timestampUtc": boundary_row["timestampUtc"],
                    "sampleDelayMilliseconds": recorded_delay,
                    **boundary_metrics_values,
                }
            )

        if scenario.get("idleSeconds") != IDLE_SECONDS or scenario.get("sampleIntervalSeconds") != 1:
            raise EvidenceValidationError(f"{scenario_id} idle protocol is not a five-minute one-second trace")
        if scenario.get("userInteractionDuringIdle") is not False:
            raise EvidenceValidationError(f"{scenario_id} idle period included user interaction")
        samples = [(index, row) for index, row in enumerate(metrics) if row.get("scenarioId") == scenario_id]
        if len(samples) != SAMPLE_COUNT:
            raise EvidenceValidationError(f"{scenario_id} must contain exactly {SAMPLE_COUNT} metric samples")

        sample_times: list[datetime] = []
        sample_metrics: dict[str, list[int]] = {
            "currentWorkingSetBytes": [],
            "privateBytes": [],
            "peakWorkingSetBytes": [],
            "handles": [],
            "threads": [],
        }
        for expected_index, (row_index, sample) in enumerate(samples):
            if row_index in consumed_rows:
                raise EvidenceValidationError("a metric row was assigned to multiple scenarios")
            consumed_rows.add(row_index)
            sample_index = _require_nonnegative_int(sample.get("sampleIndex"), f"{scenario_id}.sampleIndex")
            if sample_index != expected_index:
                raise EvidenceValidationError(f"{scenario_id} sample indices are missing or out of order")
            if sample.get("phase") != ("startup-ready" if expected_index == 0 else "idle"):
                raise EvidenceValidationError(f"{scenario_id} sample phase is invalid")
            if sample.get("pid") != pid or sample.get("processCreatedAtUtc") != created_at_text:
                raise EvidenceValidationError(f"{scenario_id} sample process identity differs")
            if not _same_windows_path(sample.get("canonicalExecutablePath"), executable_path):
                raise EvidenceValidationError(f"{scenario_id} sample image path differs from package")
            if sample.get("architecture") != "x64" or _require_sha(sample.get("sha256"), "sample.sha256") != application_sha:
                raise EvidenceValidationError(f"{scenario_id} sample executable identity differs")
            sample_times.append(_parse_timestamp(sample.get("timestampUtc"), f"{scenario_id}.sample.timestampUtc"))
            for metric_name, values in sample_metrics.items():
                values.append(_require_positive_int(sample.get(metric_name), f"{scenario_id}.{metric_name}"))

        if sample_times[0] < diagnostic_at or (sample_times[0] - diagnostic_at).total_seconds() > 5:
            raise EvidenceValidationError(f"{scenario_id} startup process sample is not adjacent to startup-ready")
        if sample_times[-1] - diagnostic_at < timedelta(seconds=IDLE_SECONDS):
            raise EvidenceValidationError(f"{scenario_id} idle trace is shorter than five minutes")
        for left, right in zip(sample_times, sample_times[1:]):
            interval = (right - left).total_seconds()
            if not SAMPLE_INTERVAL_MIN_SECONDS <= interval <= SAMPLE_INTERVAL_MAX_SECONDS:
                raise EvidenceValidationError(f"{scenario_id} sample interval {interval:.3f}s is outside the one-second cadence")
        if scenario_started > diagnostic_at or created_at > sample_times[0]:
            raise EvidenceValidationError(f"{scenario_id} timestamp order is invalid")

        idle_completed = _parse_timestamp(scenario.get("idleCompletedAtUtc"), f"{scenario_id}.idleCompletedAtUtc")
        if idle_completed < sample_times[-1]:
            raise EvidenceValidationError(f"{scenario_id} idle completion timestamp precedes final sample")
        close_requested = _parse_timestamp(scenario.get("closeRequestedAtUtc"), f"{scenario_id}.closeRequestedAtUtc")
        exited_at = _parse_timestamp(scenario.get("exitedAtUtc"), f"{scenario_id}.exitedAtUtc")
        if close_requested < idle_completed or exited_at < close_requested:
            raise EvidenceValidationError(f"{scenario_id} process completion timestamps are invalid")
        if scenario.get("normalExit") is not True or scenario.get("exitCode") != 0:
            raise EvidenceValidationError(f"{scenario_id} did not exit normally with code 0")
        if exited_at > completed:
            raise EvidenceValidationError(f"{scenario_id} exit occurred after run completion")
        if previous_exit is not None and scenario_started < previous_exit:
            raise EvidenceValidationError("scenario processes were not isolated sequentially")
        previous_exit = exited_at

        idle_working_set = sample_metrics["currentWorkingSetBytes"][-1]
        scenario_violations = []
        if startup_working_set >= THRESHOLD_BYTES:
            scenario_violations.append({"point": "startup-ready", "workingSetBytes": startup_working_set})
        if sample_metrics["currentWorkingSetBytes"][0] >= THRESHOLD_BYTES:
            scenario_violations.append({"point": "startup-ready-external-sampler", "workingSetBytes": sample_metrics["currentWorkingSetBytes"][0]})
        if idle_working_set >= THRESHOLD_BYTES:
            scenario_violations.append({"point": "five-minute-idle", "workingSetBytes": idle_working_set})
        for violation in scenario_violations:
            violations.append({"scenarioId": scenario_id, **violation})

        scenario_summaries.append(
            {
                "id": scenario_id,
                "outcome": "completed",
                "budgetScope": "normal-use-gate",
                "fixtureId": fixture["id"],
                "fixtureSha256": fixture["sha256"],
                "fixturePathPassedToApp": fixture_argument,
                "workspaceOpenedAtUtc": workspace_diagnostic.get("observedAtUtc"),
                "workspaceOpenedOutcome": workspace_diagnostic.get("outcome"),
                "startupBoundaryMetrics": scenario_boundary_summaries,
                "sampleCount": SAMPLE_COUNT,
                "startupReadyDiagnosticAtUtc": diagnostic.get("observedAtUtc"),
                "startupReadyWorkingSetBytes": startup_working_set,
                "startupReadyExternalWorkingSetBytes": sample_metrics["currentWorkingSetBytes"][0],
                "fiveMinuteIdleWorkingSetBytes": idle_working_set,
                "idleDurationSeconds": (sample_times[-1] - diagnostic_at).total_seconds(),
                "currentWorkingSetBytes": _metric_summary(sample_metrics["currentWorkingSetBytes"]),
                "privateBytes": _metric_summary(sample_metrics["privateBytes"]),
                "peakWorkingSetBytes": _metric_summary(sample_metrics["peakWorkingSetBytes"]),
                "handles": _metric_summary(sample_metrics["handles"]),
                "threads": _metric_summary(sample_metrics["threads"]),
                "budgetViolations": scenario_violations,
            }
        )

    if len(consumed_rows) != len(metrics):
        raise EvidenceValidationError("metrics trace contains rows for an unknown scenario")
    budget_result = "failed" if violations else "passed"
    return {
        "schema": SUMMARY_SCHEMA,
        "taskId": TASK_ID,
        "validation": "valid",
        "runStatus": "completed",
        "candidateSourceSha256": source_sha,
        "revision": revision,
        "configuration": configuration,
        "qtInstallation": qt_installation,
        "os": operating_system,
        "application": application,
        "package": {
            "id": package["id"],
            "sha256": package_sha,
            "fileCount": len(package_files),
        },
        "scenarioCount": len(scenario_summaries),
        "sampleCount": len(metrics),
        "thresholdBytes": THRESHOLD_BYTES,
        "comparison": "strictly-less-than",
        "startupBoundaryMetrics": {
            "schema": boundary_manifest["schema"],
            "inventoryFile": boundary_manifest["inventoryFile"],
            "sampleCount": len(startup_boundary_metrics),
            "samplesPerScenario": len(EXPECTED_STARTUP_BOUNDARIES),
        },
        "budgetResult": budget_result,
        "gateResult": budget_result,
        "interpretation": (
            "Valid complete evidence; all gated working-set points are strictly below threshold."
            if budget_result == "passed"
            else "Valid complete evidence; one or more gated working-set points meet or exceed threshold."
        ),
        "scenarios": scenario_summaries,
        "budgetViolations": violations,
        "errors": [],
    }


def _load_manifest(path: Path) -> dict[str, Any]:
    return _require_object(_read_json(path, "manifest"), "manifest")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--metrics", required=True, type=Path)
    parser.add_argument("--startup-log", required=True, type=Path)
    parser.add_argument("--startup-boundary-metrics", required=True, type=Path)
    parser.add_argument("--package-files", required=True, type=Path)
    parser.add_argument("--source-files", required=True, type=Path)
    parser.add_argument("--build-log", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    args = parser.parse_args(argv)

    manifest: dict[str, Any] | None = None
    try:
        manifest = _load_manifest(args.manifest)
        metrics = _read_jsonl(args.metrics, "metrics trace")
        startup_events = _read_jsonl(args.startup_log, "startup log")
        startup_boundary_metrics = _read_jsonl(
            args.startup_boundary_metrics, "startup boundary metric trace"
        )
        boundary_manifest = _require_object(
            _require_object(manifest.get("startupBoundaryMetrics"), "startupBoundaryMetrics"),
            "startupBoundaryMetrics",
        )
        if args.startup_boundary_metrics.name != boundary_manifest.get("inventoryFile"):
            raise EvidenceValidationError("startup boundary metric file identity differs from manifest")
        package_files = _require_object(_read_json(args.package_files, "package file inventory"), "package file inventory").get("files")
        if not isinstance(package_files, list):
            raise EvidenceValidationError("package file inventory files must be an array")
        source_files = _require_object(_read_json(args.source_files, "source file inventory"), "source file inventory").get("files")
        if not isinstance(source_files, list):
            raise EvidenceValidationError("source file inventory files must be an array")
        try:
            build_log = args.build_log.read_bytes()
        except OSError as error:
            raise EvidenceValidationError(f"cannot read package prerequisite build log: {error}") from error
        package_build = _require_object(
            _require_object(manifest.get("configuration"), "configuration").get("packageBuild"),
            "configuration.packageBuild",
        )
        if not _same_windows_path(str(args.build_log.resolve()), package_build.get("logPath")):
            raise EvidenceValidationError("package prerequisite build log path differs from manifest")
        summary = validate_documents(
            manifest,
            metrics,
            startup_events,
            package_files,
            source_files,
            startup_boundary_metrics,
            build_log,
        )
        exit_code = 0 if summary["budgetResult"] == "passed" else 2
    except EvidenceValidationError as error:
        errors = [str(error)]
        if isinstance(manifest, dict) and isinstance(manifest.get("failure"), str):
            errors.append(manifest["failure"])
        summary = {
            "schema": SUMMARY_SCHEMA,
            "taskId": TASK_ID,
            "validation": "invalid",
            "runStatus": manifest.get("status", "unknown") if isinstance(manifest, dict) else "unknown",
            "thresholdBytes": THRESHOLD_BYTES,
            "comparison": "strictly-less-than",
            "budgetResult": "not-evaluated",
            "gateResult": "not-evaluated",
            "interpretation": "Evidence is incomplete or invalid; memory budget was not evaluated.",
            "errors": errors,
            "budgetViolations": [],
        }
        exit_code = 1

    args.summary.parent.mkdir(parents=True, exist_ok=True)
    args.summary.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"validation": summary["validation"], "budgetResult": summary["budgetResult"], "errors": summary["errors"]}))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
