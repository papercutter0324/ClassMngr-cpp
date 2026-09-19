#!/usr/bin/env python3
"""Build, package, audit, and run the macOS universal Phase 0 matrix."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


TASK_ID = "QT0-EVIDENCE-AUTOMATION"
RUNNER_TASK_ID = "QT0-AUTO-RUNNER"
RUN_SCHEMA = "classmngr-phase0-evidence-run-v1"
ROUTE_SCHEMA = "classmngr-phase0-route-v1"
SUPPORTED_PLATFORMS = ("windows-x64", "macos-universal")
DEFERRED_PLATFORMS = ("windows-arm64", "linux")
EXPECTED_ARCHITECTURES = ("arm64", "x86_64")
DEPLOYMENT_TARGET = "14.4"
DEFAULT_TIMEOUT_SECONDS = 900
DEFAULT_PARALLEL = 4
MINIMUM_FREE_BYTES = 18 * 1024**3
LEGACY_RESIDENT_TARGET_BYTES = 250 * 1024 * 1024
TEMPORARY_DIAGNOSTIC_CEILING_BYTES = 512 * 1024 * 1024


@dataclass(frozen=True)
class RouteDefinition:
    route_id: str
    category: str
    artifact_path: str
    scenario: str
    fixture: str


ROUTES = (
    RouteDefinition("visual-empty-english-light", "visual", "visual/empty/english-light", "empty packaged startup, English, light", "empty workspace"),
    RouteDefinition("visual-empty-english-dark", "visual", "visual/empty/english-dark", "empty packaged startup, English, dark", "empty workspace"),
    RouteDefinition("visual-empty-korean-light", "visual", "visual/empty/korean-light", "empty packaged startup, Korean, light", "empty workspace"),
    RouteDefinition("visual-empty-korean-dark", "visual", "visual/empty/korean-dark", "empty packaged startup, Korean, dark", "empty workspace"),
    RouteDefinition("fixture-representative", "fixture", "fixtures/representative", "deterministic representative startup fixture", "representative_startup.sql"),
    RouteDefinition("visual-representative", "visual", "visual/representative", "representative packaged startup language/theme matrix", "representative startup fixture"),
    RouteDefinition("visual-classes", "visual", "visual/classes", "large Classes entry and selection language/theme states", "large_startup.sql"),
    RouteDefinition("visual-sub-prep", "visual", "visual/sub-prep", "large Sub Prep populated, changed, read-only, and empty states", "large_startup.sql"),
    RouteDefinition("visual-pdf-viewer", "visual", "visual/pdf-viewer", "PDF catalog, open, close, error, and reopen states", "large PDF viewer fixture"),
    RouteDefinition("startup-empty", "memory", "startup/empty", "empty packaged Release startup metrics", "empty workspace"),
    RouteDefinition("startup-representative", "memory", "startup/representative", "representative packaged Release startup metrics", "representative startup fixture"),
    RouteDefinition("workflow-representative", "workflow", "workflow/representative", "representative packaged Release full navigation and PDF lifecycle", "representative startup fixture"),
    RouteDefinition("lifecycle-sub-prep", "memory", "memory/sub-prep", "large Sub Prep refresh, leave, and repeated re-entry lifecycle", "large_startup.sql"),
    RouteDefinition("lifecycle-classes", "memory", "memory/classes", "large Classes selection, refresh, leave, and repeated re-entry lifecycle", "large_startup.sql"),
    RouteDefinition("lifecycle-schedule", "memory", "memory/schedule", "large Schedule refresh, leave, and repeated re-entry lifecycle", "large_startup.sql"),
    RouteDefinition("lifecycle-schedule-import", "output", "memory/schedule-import", "large Schedule Import parse, conflict review, cancel, and release lifecycle", "large_startup.sql"),
    RouteDefinition("lifecycle-schedule-import-apply", "output", "memory/schedule-import-apply", "large Schedule Import parse, review, apply, and release lifecycle", "large_startup.sql"),
    RouteDefinition("lifecycle-calendar-import", "output", "memory/calendar-import", "large Calendar Import parse, apply, refresh, and release lifecycle", "large_startup.sql"),
    RouteDefinition("lifecycle-calendar-import-error", "output", "memory/calendar-import-error", "large Calendar Import parser failure and release lifecycle", "large_startup.sql"),
    RouteDefinition("lifecycle-class-transfer", "memory", "memory/class-transfer", "large Class Transfer review, apply, and release lifecycle", "large_startup.sql"),
    RouteDefinition("lifecycle-speaking-evaluation", "output", "output/speaking-evaluation", "large Speaking Evaluation report, export, and release lifecycle", "large_startup.sql"),
    RouteDefinition("lifecycle-staff-directory", "memory", "memory/staff-directory", "large Staff Directory refresh, re-entry, and release lifecycle", "large_startup.sql"),
    RouteDefinition("output-sub-prep", "output", "output/sub-prep", "large Sub Prep generated PDF output and validation-error evidence", "large_startup.sql"),
    RouteDefinition("resource-trace", "memory", "memory/resource-trace", "large packaged resource payload and lease lifecycle trace", "large resource-trace fixture"),
)
ROUTE_BY_ID = {route.route_id: route for route in ROUTES}
ALL_ROUTE_IDS = tuple(route.route_id for route in ROUTES)
FOCUSED_CALENDAR_ROUTE_IDS = (
    "lifecycle-calendar-import",
    "lifecycle-calendar-import-error",
)

REQUIRED_OPT_IN_ENVIRONMENT_VARIABLES = (
    "QT_QPA_PLATFORM",
    "CLASSMNGR_TEST_APP_PATH",
    "CLASSMNGR_SETTINGS_ROOT",
    "CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH",
    "CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR",
    "CLASSMNGR_VISUAL_BASELINE_OUTPUT_DIR",
    "CLASSMNGR_LARGE_CLASSES_VISUAL_REFERENCE_DIR",
    "CLASSMNGR_LARGE_SUB_PREP_VISUAL_REFERENCE_DIR",
    "CLASSMNGR_LARGE_PDF_VIEWER_VISUAL_REFERENCE_DIR",
    "CLASSMNGR_LARGE_SUB_PREP_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_LARGE_CLASSES_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_LARGE_SCHEDULE_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_LARGE_SCHEDULE_IMPORT_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_LARGE_SCHEDULE_IMPORT_APPLY_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_LARGE_CALENDAR_IMPORT_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_LARGE_CALENDAR_IMPORT_ERROR_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_LARGE_CLASS_TRANSFER_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_LARGE_SPEAKING_EVALUATION_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_LARGE_STAFF_DIRECTORY_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_LARGE_SUB_PREP_OUTPUT_BOUNDARY_REFERENCE_DIR",
    "CLASSMNGR_STARTUP_RESOURCE_TRACE_PATH",
)


def _run_manifest_platform_metadata() -> dict[str, Any]:
    return {
        "supportedPlatform": "macos-universal",
        "supportedPlatforms": list(SUPPORTED_PLATFORMS),
        "deferredPlatforms": list(DEFERRED_PLATFORMS),
        "platformCoverage": [
            {
                "name": "windows-x64",
                "supported": True,
                "status": "supported-but-not-run-on-this-host",
            },
            {
                "name": "macos-universal",
                "supported": True,
                "status": "runnable-on-this-host",
            },
        ],
        "platform": {
            "name": "macos-universal",
            "supported": True,
            "acceptanceScope": (
                "Packaged Release macOS universal routes on Apple Silicon; "
                "x86_64 is verified as a packaged universal slice."
            ),
        },
    }


class RunnerError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def _path_text(path: Path) -> str:
    return str(path.expanduser().absolute())


def _route_paths(root: Path, artifact_path: str) -> Path:
    return root.joinpath(*artifact_path.split("/"))


def route_invocation(
    route_id: str,
    evidence_root: Path,
    application_path: Path,
    test_executable: Path,
    repository_root: Path | None = None,
) -> dict[str, Any]:
    definition = ROUTE_BY_ID[route_id]
    root = evidence_root.absolute()
    route_root = _route_paths(root, definition.artifact_path)
    app = str(application_path.absolute())
    test = str(test_executable.absolute())
    arguments: list[str]
    # Keep the platform selector explicit in every route's environment record,
    # rather than relying only on the isolated-environment default.
    overrides: dict[str, str] = {"QT_QPA_PLATFORM": "offscreen"}
    file_path = test

    if route_id == "fixture-representative":
        arguments = ["representativeStartupFixtureIsCompleteAndDeterministic"]
        overrides["CLASSMNGR_STARTUP_FIXTURE_OUTPUT_PATH"] = str(
            route_root / "representative-startup.tps"
        )
    elif route_id.startswith("visual-empty-"):
        language, theme = route_id.removeprefix("visual-empty-").split("-")
        settings_root = root / "runtime" / f"settings-{language}-{theme}"
        overrides["CLASSMNGR_SETTINGS_ROOT"] = str(settings_root)
        arguments = [
            "--startup-performance-test",
            "--startup-performance-scenario",
            "minimal",
            "--startup-performance-settle-ms",
            "1000",
            "--startup-performance-output",
            str(route_root / "startup-metrics.json"),
            "--startup-visual-capture-output",
            str(route_root),
            "--startup-visual-capture-language",
            language,
            "--startup-visual-capture-theme",
            theme,
        ]
        file_path = app
    elif route_id == "visual-representative":
        overrides["CLASSMNGR_VISUAL_BASELINE_OUTPUT_DIR"] = str(route_root)
        arguments = ["capturesRepresentativeVisualVariants"]
    elif route_id == "visual-classes":
        overrides["CLASSMNGR_LARGE_CLASSES_VISUAL_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeClassesVisualStatesWhenConfigured"]
    elif route_id == "visual-sub-prep":
        overrides["CLASSMNGR_LARGE_SUB_PREP_VISUAL_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeSubPrepVisualStatesWhenConfigured"]
    elif route_id == "visual-pdf-viewer":
        overrides["CLASSMNGR_LARGE_PDF_VIEWER_VISUAL_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargePdfViewerVisualStatesWhenConfigured"]
    elif route_id == "startup-empty":
        settings_root = root / "runtime" / "settings-empty"
        overrides["CLASSMNGR_SETTINGS_ROOT"] = str(settings_root)
        arguments = [
            "--startup-performance-test",
            "--startup-performance-scenario",
            "minimal",
            "--startup-performance-settle-ms",
            "1000",
            "--startup-performance-output",
            str(route_root / "startup-metrics.json"),
        ]
        file_path = app
    elif route_id == "startup-representative":
        fixture = root / "fixtures" / "representative" / "representative-startup.tps"
        overrides["CLASSMNGR_SETTINGS_ROOT"] = str(root / "runtime" / "representative-settings")
        arguments = [
            "--startup-performance-test",
            "--startup-performance-scenario",
            "representative",
            "--startup-performance-settle-ms",
            "5000",
            "--startup-performance-output",
            str(route_root / "startup-metrics.json"),
            "--startup-visual-capture-output",
            str(route_root),
            "--startup-visual-capture-language",
            "english",
            "--startup-visual-capture-theme",
            "light",
            str(fixture),
        ]
        file_path = app
    elif route_id == "workflow-representative":
        fixture = root / "fixtures" / "representative" / "representative-startup.tps"
        overrides["CLASSMNGR_SETTINGS_ROOT"] = str(root / "runtime" / "representative-settings")
        overrides["CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH"] = str(route_root / "workflow-trace.txt")
        overrides["CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR"] = str(route_root / "pdf-captures")
        arguments = [
            "--startup-performance-test",
            "--startup-performance-workflow",
            "--startup-performance-scenario",
            "representative",
            "--startup-performance-settle-ms",
            "1000",
            "--startup-performance-output",
            str(route_root / "startup-metrics.json"),
            "--startup-visual-capture-output",
            str(route_root / "captures"),
            "--startup-visual-capture-language",
            "english",
            "--startup-visual-capture-theme",
            "light",
            str(fixture),
        ]
        file_path = app
    elif route_id == "lifecycle-sub-prep":
        overrides["CLASSMNGR_LARGE_SUB_PREP_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeSubPrepBoundaryWhenConfigured"]
    elif route_id == "lifecycle-classes":
        overrides["CLASSMNGR_LARGE_CLASSES_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeClassesBoundaryWhenConfigured"]
    elif route_id == "lifecycle-schedule":
        overrides["CLASSMNGR_LARGE_SCHEDULE_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeScheduleBoundaryWhenConfigured"]
    elif route_id == "lifecycle-schedule-import":
        overrides["CLASSMNGR_LARGE_SCHEDULE_IMPORT_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeScheduleImportBoundaryWhenConfigured"]
    elif route_id == "lifecycle-schedule-import-apply":
        overrides["CLASSMNGR_LARGE_SCHEDULE_IMPORT_APPLY_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeScheduleImportApplyBoundaryWhenConfigured"]
    elif route_id == "lifecycle-calendar-import":
        overrides["CLASSMNGR_LARGE_CALENDAR_IMPORT_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeCalendarImportBoundaryWhenConfigured"]
    elif route_id == "lifecycle-calendar-import-error":
        overrides["CLASSMNGR_LARGE_CALENDAR_IMPORT_ERROR_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        overrides["CLASSMNGR_STARTUP_CALENDAR_IMPORT_EXPECTED_FAILURE"] = "1"
        arguments = ["capturesLargeCalendarImportBoundaryWhenConfigured"]
    elif route_id == "lifecycle-class-transfer":
        overrides["CLASSMNGR_LARGE_CLASS_TRANSFER_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeClassTransferBoundaryWhenConfigured"]
    elif route_id == "lifecycle-speaking-evaluation":
        overrides["CLASSMNGR_LARGE_SPEAKING_EVALUATION_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeSpeakingEvaluationBoundaryWhenConfigured"]
    elif route_id == "lifecycle-staff-directory":
        overrides["CLASSMNGR_LARGE_STAFF_DIRECTORY_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeStaffDirectoryBoundaryWhenConfigured"]
    elif route_id == "output-sub-prep":
        overrides["CLASSMNGR_LARGE_SUB_PREP_OUTPUT_BOUNDARY_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeSubPrepOutputBoundaryWhenConfigured"]
    elif route_id == "resource-trace":
        overrides["CLASSMNGR_LARGE_RESOURCE_TRACE_REFERENCE_DIR"] = str(route_root)
        arguments = ["capturesLargeResourceTraceWhenConfigured"]
    else:
        raise RunnerError(f"No macOS invocation mapping exists for route {route_id!r}.")

    return {
        "filePath": file_path,
        "arguments": arguments,
        "environmentOverrides": overrides,
        "workingDirectory": str(
            repository_root.absolute()
            if repository_root is not None
            else Path(__file__).resolve().parents[2]
        ),
    }


def _append_path(parts: list[str], value: str | None) -> None:
    if value and value not in parts:
        parts.append(value)


def _isolated_environment(
    evidence_root: Path,
    application_path: Path,
    test_executable: Path,
    qt_prefix: Path,
    overrides: dict[str, str] | None = None,
) -> dict[str, str]:
    root = evidence_root.absolute()
    home = root / "runtime" / "home"
    temporary = root / "tmp"
    env = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith(("CLASSMNGR_", "QT_", "DYLD_"))
        and key not in {"HOME", "TMP", "TEMP", "TMPDIR", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"}
    }
    path_parts: list[str] = []
    _append_path(path_parts, str(application_path.absolute().parent))
    _append_path(path_parts, str(test_executable.absolute().parent))
    _append_path(path_parts, str(qt_prefix.absolute() / "bin"))
    for part in os.environ.get("PATH", "").split(os.pathsep):
        _append_path(path_parts, part)
    env.update(
        {
            "PATH": os.pathsep.join(path_parts),
            "HOME": str(home),
            "TMPDIR": str(temporary),
            "TMP": str(temporary),
            "TEMP": str(temporary),
            "XDG_CONFIG_HOME": str(root / "runtime" / "xdg-config"),
            "XDG_DATA_HOME": str(root / "runtime" / "xdg-data"),
            "XDG_CACHE_HOME": str(root / "runtime" / "xdg-cache"),
            "XDG_STATE_HOME": str(root / "runtime" / "xdg-state"),
            "QT_QPA_PLATFORM": "offscreen",
            "CLASSMNGR_TEST_APP_PATH": str(application_path.absolute()),
            "PYTHONDONTWRITEBYTECODE": "1",
        }
    )
    if overrides:
        env.update(overrides)
    return env


def _recorded_environment(env: dict[str, str]) -> dict[str, str]:
    return {
        key: env[key]
        for key in sorted(env)
        if key in {
            "PATH",
            "SDKROOT",
            "HOME",
            "TMPDIR",
            "TMP",
            "TEMP",
            "XDG_CONFIG_HOME",
            "XDG_DATA_HOME",
            "XDG_CACHE_HOME",
            "XDG_STATE_HOME",
            "QT_QPA_PLATFORM",
            "QT_MACOS_PREFIX",
            "DEVELOPER_DIR",
            "PYTHONDONTWRITEBYTECODE",
        }
        or key.startswith(("QT_", "CLASSMNGR_"))
    }


def _build_environment(
    evidence_root: Path,
    qt_prefix: Path,
    macos_sdk_path: str,
) -> dict[str, str]:
    root = evidence_root.absolute()
    env = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith(("CLASSMNGR_", "QT_", "DYLD_"))
        and key not in {"HOME", "TMP", "TEMP", "TMPDIR", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME", "CMAKE_PREFIX_PATH"}
    }
    path_parts: list[str] = []
    _append_path(path_parts, str(qt_prefix.absolute() / "bin"))
    for part in os.environ.get("PATH", "").split(os.pathsep):
        _append_path(path_parts, part)
    temporary = root / "tmp"
    env.update(
        {
            "PATH": os.pathsep.join(path_parts),
            "SDKROOT": macos_sdk_path,
            "HOME": str(root / "runtime" / "home"),
            "TMPDIR": str(temporary),
            "TMP": str(temporary),
            "TEMP": str(temporary),
            "XDG_CONFIG_HOME": str(root / "runtime" / "xdg-config"),
            "XDG_DATA_HOME": str(root / "runtime" / "xdg-data"),
            "XDG_CACHE_HOME": str(root / "runtime" / "xdg-cache"),
            "XDG_STATE_HOME": str(root / "runtime" / "xdg-state"),
            "QT_MACOS_PREFIX": str(qt_prefix.absolute()),
            "PYTHONDONTWRITEBYTECODE": "1",
        }
    )
    return env


def _record_environment(env: dict[str, str]) -> dict[str, str]:
    return {
        key: env[key]
        for key in sorted(env)
        if key in {
            "PATH",
            "SDKROOT",
            "HOME",
            "TMPDIR",
            "TMP",
            "TEMP",
            "XDG_CONFIG_HOME",
            "XDG_DATA_HOME",
            "XDG_CACHE_HOME",
            "XDG_STATE_HOME",
            "QT_MACOS_PREFIX",
            "DEVELOPER_DIR",
            "PYTHONDONTWRITEBYTECODE",
        }
    }


def _cmake_configure_command(
    cmake: str,
    repository_root: Path,
    build_directory: Path,
    qt_prefix: Path,
    install_prefix: Path,
    installer_output: Path,
    build_type: str,
    build_testing: bool,
    macos_sdk_path: str,
) -> list[str]:
    return [
        cmake,
        "--fresh",
        "-S",
        str(repository_root),
        "-B",
        str(build_directory),
        "-G",
        "Ninja",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DCMAKE_PREFIX_PATH={qt_prefix}",
        f"-DCMAKE_INSTALL_PREFIX={install_prefix}",
        f"-DCMAKE_OSX_ARCHITECTURES={';'.join(EXPECTED_ARCHITECTURES)}",
        f"-DCMAKE_OSX_DEPLOYMENT_TARGET={DEPLOYMENT_TARGET}",
        f"-DCMAKE_OSX_SYSROOT={macos_sdk_path}",
        f"-DBUILD_TESTING={'ON' if build_testing else 'OFF'}",
        "-DCMAKE_CXX_STANDARD=23",
        "-DCMAKE_CXX_STANDARD_REQUIRED=ON",
        "-DCMAKE_CXX_EXTENSIONS=OFF",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        f"-DCLASSMNGR_INSTALLER_OUTPUT_DIR={installer_output}",
    ]


def build_plan(
    repository_root: Path,
    evidence_root: Path,
    qt_prefix: Path,
    cmake: str,
    python: str,
    parallel: int,
    timeout_seconds: int,
    macos_sdk_path: str,
) -> dict[str, Any]:
    repo = repository_root.absolute()
    root = evidence_root.absolute()
    qt = qt_prefix.absolute()
    release_build = root / "build" / "release"
    test_build = root / "build" / "debug"
    install_prefix = root / "package"
    test_install_prefix = root / "test-install"
    installer_output = root / "installer-output"
    release_app = install_prefix / "ClassMngr.app"
    application = release_app / "Contents" / "MacOS" / "ClassMngr"
    test_executable = test_build / "ClassMngrStartupPerformanceTests"
    qoffscreen_source = qt / "plugins" / "platforms" / "libqoffscreen.dylib"
    qoffscreen_destination = release_app / "Contents" / "PlugIns" / "platforms" / "libqoffscreen.dylib"
    validator = repo / "scripts" / "phase0" / "validate_phase0_evidence.py"
    cmake_configure_release = _cmake_configure_command(
        cmake,
        repo,
        release_build,
        qt,
        install_prefix,
        installer_output,
        "Release",
        False,
        macos_sdk_path,
    )
    cmake_configure_debug = _cmake_configure_command(
        cmake,
        repo,
        test_build,
        qt,
        test_install_prefix,
        installer_output,
        "Debug",
        True,
        macos_sdk_path,
    )
    commands: list[dict[str, Any]] = [
        {"id": "configure-release", "kind": "configure", "command": cmake_configure_release},
        {
            "id": "build-release",
            "kind": "build",
            "command": [cmake, "--build", str(release_build), "--target", "ClassMngr", "--parallel", str(parallel)],
        },
        {"id": "install-release-package", "kind": "package", "command": [cmake, "--install", str(release_build), "--prefix", str(install_prefix)]},
        {"id": "copy-offscreen-platform-plugin", "kind": "package", "command": [cmake, "-E", "copy_if_different", str(qoffscreen_source), str(qoffscreen_destination)]},
        {"id": "sign-offscreen-package", "kind": "package", "command": ["codesign", "--force", "--deep", "--sign", "-", str(release_app)]},
        {"id": "verify-package-signature", "kind": "package-check", "command": ["codesign", "--verify", "--deep", "--strict", str(release_app)]},
        {"id": "configure-debug-tests", "kind": "configure", "command": cmake_configure_debug},
        {
            "id": "build-debug-startup-harness",
            "kind": "build",
            "command": [cmake, "--build", str(test_build), "--target", "ClassMngrStartupPerformanceTests", "--parallel", str(parallel)],
        },
    ]
    for route in ROUTES:
        invocation = route_invocation(
            route.route_id,
            root,
            application,
            test_executable,
            repo,
        )
        route_env = _isolated_environment(
            root,
            application,
            test_executable,
            qt,
            invocation["environmentOverrides"],
        )
        commands.append(
            {
                "id": route.route_id,
                "kind": "route",
                "command": [invocation["filePath"], *invocation["arguments"]],
                "workingDirectory": invocation["workingDirectory"],
                "environment": _recorded_environment(route_env),
            }
        )
    commands.append(
        {
            "id": "validate-macos-universal-evidence",
            "kind": "validation",
            "command": [
                python,
                str(validator),
                "--evidence-root",
                str(root),
                "--expected-platform",
                "macos-universal",
                "--json-out",
                str(root / "validation-summary.json"),
            ],
        }
    )
    return {
        "evidenceRoot": str(root),
        "repositoryRoot": str(repo),
        "qtPrefix": str(qt),
        "releaseBuildDirectory": str(release_build),
        "testBuildDirectory": str(test_build),
        "installDirectory": str(install_prefix),
        "applicationPath": str(application),
        "testExecutable": str(test_executable),
        "targetArchitectures": list(EXPECTED_ARCHITECTURES),
        "targetMinimumVersion": DEPLOYMENT_TARGET,
        "timeoutSeconds": timeout_seconds,
        "commands": commands,
        "requestedRoutes": list(ALL_ROUTE_IDS),
    }


def _inside(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def _validate_evidence_root(root: Path, repository_root: Path, *, create: bool) -> Path:
    candidate = root.expanduser().absolute()
    if candidate == Path(candidate.anchor):
        raise RunnerError("Evidence root cannot be a filesystem root.")
    if _inside(candidate, repository_root.resolve()):
        raise RunnerError(f"Evidence root must be outside the repository: {candidate}")
    for protected in ("build", "dist", ".git", "agent_docs", "src", "tests", "cmake"):
        protected_path = repository_root.resolve() / protected
        if candidate == protected_path or _inside(candidate, protected_path):
            raise RunnerError(f"Evidence root selects protected project data: {candidate}")
    if not candidate.parent.is_dir():
        raise RunnerError(f"Evidence root parent does not exist: {candidate.parent}")
    cursor = candidate.parent
    while True:
        if cursor.is_symlink():
            raise RunnerError(f"Evidence path traverses a symlink: {cursor}")
        if cursor.parent == cursor:
            break
        cursor = cursor.parent
    if candidate.exists() or candidate.is_symlink():
        raise RunnerError(f"Evidence root already exists and will not be reused: {candidate}")
    if create:
        try:
            candidate.mkdir()
        except FileExistsError as error:
            raise RunnerError(f"Evidence root already exists and will not be reused: {candidate}") from error
    return candidate


def _git_snapshot(repository_root: Path) -> dict[str, Any]:
    commit = subprocess.run(
        ["git", "-C", str(repository_root), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    status = subprocess.run(
        ["git", "-C", str(repository_root), "status", "--porcelain", "--untracked-files=all"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()
    outside_allowed = [
        line for line in status
        if not line[3:].strip().replace("\\", "/").startswith("scripts/phase0/")
    ]
    if outside_allowed:
        raise RunnerError(
            "Working-tree edits outside scripts/phase0 are present; refusing to build an ambiguous snapshot: "
            + "; ".join(outside_allowed)
        )
    return {
        "commit": commit,
        "statusPorcelain": status,
        "statusCleanExceptRunnerFiles": not outside_allowed,
        "runnerPathsAllowed": all(
            line[3:].strip().replace("\\", "/").startswith("scripts/phase0/")
            for line in status
        ),
    }


def _write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def _hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _process_record(
    label: str,
    command: list[str],
    cwd: Path,
    env: dict[str, str],
    log_directory: Path,
    timeout_seconds: int,
) -> dict[str, Any]:
    log_directory.mkdir(parents=True, exist_ok=True)
    stdout_path = log_directory / "runner-stdout.txt"
    stderr_path = log_directory / "runner-stderr.txt"
    command = [str(item) for item in command]
    record: dict[str, Any] = {
        "label": label,
        "command": command,
        "commandLine": shlex.join(command),
        "workingDirectory": str(cwd),
        "environment": _recorded_environment(env),
        "startedAtUtc": utc_now(),
        "stdoutPath": str(stdout_path),
        "stderrPath": str(stderr_path),
        "processFinished": False,
        "exitStatus": "not-started",
        "exitCode": None,
        "timedOut": False,
        "durationSeconds": 0.0,
    }
    visible = not label.startswith(("file-", "lipo-", "vtool-"))
    if visible:
        print(f"[{label}] start: {record['commandLine']}", flush=True)
    start = time.monotonic()
    process: subprocess.Popen[bytes] | None = None
    try:
        with stdout_path.open("wb") as stdout_stream, stderr_path.open("wb") as stderr_stream:
            process = subprocess.Popen(
                command,
                cwd=str(cwd),
                env=env,
                stdin=subprocess.DEVNULL,
                stdout=stdout_stream,
                stderr=stderr_stream,
                start_new_session=True,
            )
            record["processId"] = process.pid
            try:
                process.wait(timeout=timeout_seconds)
            except subprocess.TimeoutExpired:
                record["timedOut"] = True
                record["terminationSignal"] = "SIGTERM"
                try:
                    os.killpg(process.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    record["terminationSignal"] = "SIGKILL"
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    process.wait()
        record["processFinished"] = process.poll() is not None
        record["exitCode"] = process.returncode
        record["exitStatus"] = "timeout" if record["timedOut"] else "normal"
    except Exception as error:  # preserve start/runner errors in the evidence root
        record["exitStatus"] = "failed-to-start"
        record["error"] = f"{type(error).__name__}: {error}"
        stdout_path.touch(exist_ok=True)
        with stderr_path.open("a", encoding="utf-8") as stream:
            stream.write(record["error"] + "\n")
    finally:
        if process is not None and process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
        record["durationSeconds"] = round(time.monotonic() - start, 3)
        record["finishedAtUtc"] = utc_now()
        if visible:
            print(
                f"[{label}] finished: exitCode={record['exitCode']} "
                f"timedOut={record['timedOut']} durationSeconds={record['durationSeconds']}",
                flush=True,
            )
    return record


def _is_success(record: dict[str, Any]) -> bool:
    return (
        record.get("processFinished") is True
        and record.get("exitStatus") == "normal"
        and record.get("exitCode") == 0
        and record.get("timedOut") is False
    )


def _invoke(
    manifest: dict[str, Any],
    command_id: str,
    kind: str,
    command: list[str],
    cwd: Path,
    env: dict[str, str],
    log_directory: Path,
    evidence_root: Path,
    timeout_seconds: int,
) -> dict[str, Any]:
    record = _process_record(command_id, command, cwd, env, log_directory, timeout_seconds)
    record["id"] = command_id
    record["kind"] = kind
    record["status"] = "completed" if _is_success(record) else "failed"
    manifest["commands"].append(record)
    _write_json(evidence_root / "run-manifest.json", manifest)
    return record


def _require_success(record: dict[str, Any]) -> None:
    if not _is_success(record):
        raise RunnerError(
            f"{record.get('id', record.get('label'))} failed "
            f"(exitCode={record.get('exitCode')}, timedOut={record.get('timedOut')}). "
            f"See {record.get('stderrPath')}."
        )


def _toolchain_versions(
    manifest: dict[str, Any],
    repository_root: Path,
    evidence_root: Path,
    env: dict[str, str],
    timeout_seconds: int,
) -> dict[str, str]:
    commands = (
        ("cmake", [shutil.which("cmake") or "cmake", "--version"]),
        ("ninja", [shutil.which("ninja") or "ninja", "--version"]),
        ("python", [sys.executable, "--version"]),
        ("xcodebuild", ["xcodebuild", "-version"]),
        ("macosSdk", ["xcrun", "--sdk", "macosx", "--show-sdk-version"]),
        ("macosSdkPath", ["xcrun", "--sdk", "macosx", "--show-sdk-path"]),
    )
    versions: dict[str, str] = {}
    for name, command in commands:
        record = _invoke(
            manifest,
            f"tool-version-{name}",
            "tool-version",
            command,
            repository_root,
            env,
            evidence_root / "logs" / f"tool-version-{name}",
            evidence_root,
            timeout_seconds,
        )
        _require_success(record)
        output = Path(record["stdoutPath"]).read_text(encoding="utf-8", errors="replace").strip()
        if not output:
            output = Path(record["stderrPath"]).read_text(encoding="utf-8", errors="replace").strip()
        versions[name] = output
    return versions


def _write_deterministic_settings(settings_root: Path) -> None:
    settings_directory = settings_root / "PaperCloud"
    settings_directory.mkdir(parents=True, exist_ok=True)
    settings_path = settings_directory / "ClassMngr.ini"
    contents = (
        "[options]\n"
        "theme=1\n"
        "fontSize=2\n"
        "language=1\n"
        "saveMode=0\n"
        "documentPageSpacing=2\n"
        "documentViewerBackground=1\n"
        "sidebarTooltipsEnabled=true\n"
        "sidebarMarqueeEnabled=false\n"
        "\n"
        "[updates]\n"
        "automaticChecksEnabled=false\n"
    )
    with settings_path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(contents)


def _route_environment(
    evidence_root: Path,
    application_path: Path,
    test_executable: Path,
    qt_prefix: Path,
    overrides: dict[str, str],
) -> dict[str, str]:
    return _isolated_environment(
        evidence_root,
        application_path,
        test_executable,
        qt_prefix,
        overrides,
    )


def _execute_routes(
    manifest: dict[str, Any],
    evidence_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    application_path: Path,
    test_executable: Path,
    timeout_seconds: int,
) -> list[str]:
    home = evidence_root / "runtime" / "home"
    temporary = evidence_root / "tmp"
    home.mkdir(parents=True, exist_ok=True)
    temporary.mkdir(parents=True, exist_ok=True)
    (evidence_root / "runtime").mkdir(parents=True, exist_ok=True)
    executed: list[str] = []
    failed: list[str] = []
    records: list[dict[str, Any]] = []

    if "startup-representative" in ALL_ROUTE_IDS or "workflow-representative" in ALL_ROUTE_IDS:
        _write_deterministic_settings(evidence_root / "runtime" / "representative-settings")

    for definition in ROUTES:
        invocation_plan = route_invocation(
            definition.route_id,
            evidence_root,
            application_path,
            test_executable,
            repository_root,
        )
        route_root = _route_paths(evidence_root, definition.artifact_path)
        if route_root.exists():
            raise RunnerError(f"Route output path already exists inside new evidence root: {route_root}")
        route_root.mkdir(parents=True)
        settings_value = invocation_plan["environmentOverrides"].get("CLASSMNGR_SETTINGS_ROOT")
        if settings_value:
            settings_root = Path(settings_value)
            settings_path = settings_root / "PaperCloud" / "ClassMngr.ini"
            if not settings_path.exists():
                _write_deterministic_settings(settings_root)
        env = _route_environment(
            evidence_root,
            application_path,
            test_executable,
            qt_prefix,
            invocation_plan["environmentOverrides"],
        )
        command = [invocation_plan["filePath"], *invocation_plan["arguments"]]
        process = _process_record(
            definition.route_id,
            command,
            repository_root,
            env,
            route_root / "runner",
            timeout_seconds,
        )
        status = "completed" if _is_success(process) else "failed"
        route_manifest = {
            "schema": ROUTE_SCHEMA,
            "taskId": TASK_ID,
            "routeId": definition.route_id,
            "category": definition.category,
            "scenario": definition.scenario,
            "fixture": definition.fixture,
            "artifactPath": definition.artifact_path,
            "status": status,
            "invocation": process,
        }
        _write_json(route_root / "route-manifest.json", route_manifest)
        command_record = dict(process)
        command_record.update({"id": definition.route_id, "kind": "route", "status": status})
        manifest["commands"].append(command_record)
        records.append(route_manifest)
        executed.append(definition.route_id)
        if status != "completed":
            failed.append(definition.route_id)
            manifest["failures"].append(
                {
                    "code": "route-process-failed",
                    "routeId": definition.route_id,
                    "message": (
                        f"Route exited abnormally: exitStatus={process.get('exitStatus')}, "
                        f"exitCode={process.get('exitCode')}, timedOut={process.get('timedOut')}."
                    ),
                    "stdoutPath": process["stdoutPath"],
                    "stderrPath": process["stderrPath"],
                }
            )
        manifest["routes"] = records
        manifest["routeExecution"] = {
            "status": "completed" if len(executed) == len(ROUTES) else "running",
            "requestedRouteCount": len(ROUTES),
            "executedRouteIds": executed,
            "failedRouteIds": failed,
        }
        _write_json(evidence_root / "run-manifest.json", manifest)
    return failed


def _version_tuple(value: str) -> tuple[int, ...]:
    return tuple(int(part) for part in value.split("."))


def _check_minimum_versions(output: str, architectures: set[str], label: str) -> list[str]:
    minimums = re.findall(r"(?m)^\s*minos\s+([0-9]+(?:\.[0-9]+){1,2})\s*$", output)
    if len(minimums) != len(architectures):
        raise RunnerError(
            f"{label}: vtool returned {len(minimums)} minOS values for {len(architectures)} architectures."
        )
    too_new = [minimum for minimum in minimums if _version_tuple(minimum) > _version_tuple(DEPLOYMENT_TARGET)]
    if too_new:
        raise RunnerError(
            f"{label}: minOS exceeds {DEPLOYMENT_TARGET}: {', '.join(too_new)}."
        )
    return minimums


def _verified_reusable_artifacts(
    source_root: Path,
    repository_root: Path,
    source_commit: str,
) -> tuple[Path, Path, dict[str, Any]]:
    source_root = source_root.expanduser().absolute()
    manifest_path = source_root / "run-manifest.json"
    audit_path = source_root / "packaging-audit" / "summary.json"
    try:
        source_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        audit = json.loads(audit_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RunnerError(f"Unable to read reusable build provenance under {source_root}: {error}") from error

    if source_manifest.get("source", {}).get("source", {}).get("commit") != source_commit:
        raise RunnerError("Reusable package source commit does not match the current repository HEAD.")
    if source_manifest.get("build", {}).get("status") != "completed":
        raise RunnerError("Reusable Release/Debug build did not complete successfully.")
    if source_manifest.get("packagingAudit", {}).get("status") != "passed":
        raise RunnerError("Reusable package does not have a passed macOS universal audit.")
    if source_manifest.get("build", {}).get("deploymentTarget") != DEPLOYMENT_TARGET:
        raise RunnerError(f"Reusable build is not targeted to macOS {DEPLOYMENT_TARGET}.")
    if source_manifest.get("build", {}).get("architectures") != list(EXPECTED_ARCHITECTURES):
        raise RunnerError("Reusable build manifest does not declare both universal architectures.")
    if audit.get("deploymentTarget") != DEPLOYMENT_TARGET:
        raise RunnerError("Reusable package audit reports an unexpected deployment target.")
    if audit.get("infoPlistMinimumVersion") != DEPLOYMENT_TARGET:
        raise RunnerError("Reusable package Info.plist minimum version does not match the target.")
    if set(audit.get("expectedArchitectures", [])) != set(EXPECTED_ARCHITECTURES):
        raise RunnerError("Reusable package audit does not verify both universal architectures.")
    embedded_macho = audit.get("embeddedMachO")
    if not isinstance(embedded_macho, list) or audit.get("embeddedMachOCount") != len(embedded_macho):
        raise RunnerError("Reusable package audit has an incomplete embedded Mach-O inventory.")

    macho_records = [audit.get("application"), audit.get("testHarness"), *embedded_macho]
    for record in macho_records:
        if not isinstance(record, dict) or record.get("kind") != "Mach-O":
            raise RunnerError("Reusable package audit contains a missing or non-Mach-O binary record.")
        if set(record.get("architectures", [])) != set(EXPECTED_ARCHITECTURES):
            raise RunnerError(f"Reusable Mach-O is not universal: {record.get('relativePath', '<unknown>')}.")
        minimums = record.get("minimumVersions", [])
        if not isinstance(minimums, list) or len(minimums) != len(EXPECTED_ARCHITECTURES):
            raise RunnerError(f"Reusable Mach-O lacks per-slice minimum versions: {record.get('relativePath', '<unknown>')}.")
        if any(_version_tuple(value) > _version_tuple(DEPLOYMENT_TARGET) for value in minimums):
            raise RunnerError(f"Reusable Mach-O exceeds macOS {DEPLOYMENT_TARGET}: {record.get('relativePath', '<unknown>')}.")

    application_path = source_root / "package" / "ClassMngr.app" / "Contents" / "MacOS" / "ClassMngr"
    test_executable = source_root / "build" / "debug" / "ClassMngrStartupPerformanceTests"
    expected_application = source_manifest.get("build", {}).get("applicationPath")
    expected_test = source_manifest.get("build", {}).get("testExecutable")
    if expected_application != str(application_path) or expected_test != str(test_executable):
        raise RunnerError("Reusable build manifest paths do not resolve inside the selected source root.")
    if not application_path.is_file() or not test_executable.is_file():
        raise RunnerError("Reusable packaged app or Debug harness is missing.")
    application_sha256 = _hash_file(application_path)
    harness_sha256 = _hash_file(test_executable)
    if application_sha256 != audit.get("application", {}).get("sha256"):
        raise RunnerError("Reusable packaged app hash does not match its passed audit.")
    if harness_sha256 != audit.get("testHarness", {}).get("sha256"):
        raise RunnerError("Reusable Debug harness hash does not match its passed audit.")
    if application_sha256 != source_manifest["build"].get("applicationSha256"):
        raise RunnerError("Reusable packaged app hash does not match its run manifest.")
    if harness_sha256 != source_manifest["build"].get("testHarnessSha256"):
        raise RunnerError("Reusable Debug harness hash does not match its run manifest.")

    provenance = {
        "sourceEvidenceRoot": str(source_root),
        "sourceRunManifestPath": str(manifest_path),
        "sourceRunManifestSha256": _hash_file(manifest_path),
        "sourcePackagingAuditSummaryPath": str(audit_path),
        "sourcePackagingAuditSummarySha256": _hash_file(audit_path),
        "sourceCommit": source_commit,
        "deploymentTarget": DEPLOYMENT_TARGET,
        "architectures": list(EXPECTED_ARCHITECTURES),
        "infoPlistMinimumVersion": audit["infoPlistMinimumVersion"],
        "embeddedMachOCount": audit["embeddedMachOCount"],
        "applicationSourcePath": str(application_path),
        "applicationSha256": application_sha256,
        "testHarnessSourcePath": str(test_executable),
        "testHarnessSha256": harness_sha256,
    }
    return application_path, test_executable, provenance


def _run_focused_calendar_routes(
    evidence_root: Path,
    source_build_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    timeout_seconds: int,
) -> int:
    root = _validate_evidence_root(evidence_root, repository_root, create=False)
    if not repository_root.is_dir() or not qt_prefix.is_dir():
        raise RunnerError("Repository root and Qt prefix must both exist for focused route execution.")
    source_snapshot = _git_snapshot(repository_root)
    application_source, test_source, provenance = _verified_reusable_artifacts(
        source_build_root,
        repository_root,
        source_snapshot["commit"],
    )
    source_manifest = json.loads(Path(provenance["sourceRunManifestPath"]).read_text(encoding="utf-8"))
    sdk_path = source_manifest.get("source", {}).get("macosSdkPath")
    if not isinstance(sdk_path, str) or not Path(sdk_path).is_dir():
        raise RunnerError("Reusable build has no valid selected macOS SDK path.")

    root = _validate_evidence_root(root, repository_root, create=True)
    application_bundle = root / "package" / "ClassMngr.app"
    application_path = application_bundle / "Contents" / "MacOS" / "ClassMngr"
    test_executable = root / "build" / "debug" / "ClassMngrStartupPerformanceTests"
    manifest_path = root / "run-manifest.json"
    manifest: dict[str, Any] = {
        "schema": RUN_SCHEMA,
        "taskId": TASK_ID,
        "runnerTaskId": RUNNER_TASK_ID,
        "executionTaskId": "QT0-MACOS-ROUTE-MATRIX-EXEC-01",
        "scope": "focused-calendar-diagnostic-only; not a complete Phase 0 route matrix",
        "recordedAtUtc": utc_now(),
        "status": "running",
        "scenario": "Focused packaged Release Calendar Import diagnostics",
        "fixture": "verified packaged Release macOS universal",
        **_run_manifest_platform_metadata(),
        "source": {**source_snapshot, "macosSdkPath": sdk_path},
        "host": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "python": sys.version,
        },
        "paths": {
            "repositoryRoot": str(repository_root),
            "evidenceRoot": str(root),
            "applicationPath": str(application_path),
            "testExecutable": str(test_executable),
            "sourceBuildRoot": provenance["sourceEvidenceRoot"],
            "qtPrefix": str(qt_prefix),
        },
        "options": {
            "scope": "focused-calendar-diagnostic-only",
            "skipBuild": True,
            "skipValidation": True,
            "timeoutSeconds": timeout_seconds,
            "routes": list(FOCUSED_CALENDAR_ROUTE_IDS),
            "targetArchitectures": list(EXPECTED_ARCHITECTURES),
            "targetMinimumVersion": DEPLOYMENT_TARGET,
        },
        "requestedRoutes": list(FOCUSED_CALENDAR_ROUTE_IDS),
        "packageProvenance": provenance,
        "packagingAudit": {
            "status": "passed-and-reused",
            "sourceSummaryPath": provenance["sourcePackagingAuditSummaryPath"],
            "sourceSummarySha256": provenance["sourcePackagingAuditSummarySha256"],
        },
        "build": {
            "status": "verified-reuse",
            "applicationPath": str(application_path),
            "testExecutable": str(test_executable),
            "architectures": list(EXPECTED_ARCHITECTURES),
            "deploymentTarget": DEPLOYMENT_TARGET,
            "applicationSha256": provenance["applicationSha256"],
            "testHarnessSha256": provenance["testHarnessSha256"],
            "reusedFromEvidenceRoot": provenance["sourceEvidenceRoot"],
        },
        "routeExecution": {
            "status": "not-started",
            "requestedRouteCount": len(FOCUSED_CALENDAR_ROUTE_IDS),
            "executedRouteIds": [],
            "failedRouteIds": [],
        },
        "routes": [],
        "commands": [],
        "failures": [],
    }
    _write_json(manifest_path, manifest)
    try:
        for directory in (
            root / "runtime" / "home",
            root / "runtime" / "xdg-config",
            root / "runtime" / "xdg-data",
            root / "runtime" / "xdg-cache",
            root / "runtime" / "xdg-state",
            root / "tmp",
            root / "build" / "debug",
        ):
            directory.mkdir(parents=True, exist_ok=True)
        shutil.copytree(application_source.parents[2], application_bundle, symlinks=True)
        shutil.copy2(test_source, test_executable)
        if _hash_file(application_path) != provenance["applicationSha256"]:
            raise RunnerError("Copied packaged app executable hash differs from verified source.")
        if _hash_file(test_executable) != provenance["testHarnessSha256"]:
            raise RunnerError("Copied Debug harness hash differs from verified source.")
        provenance["applicationCopiedPath"] = str(application_path)
        provenance["testHarnessCopiedPath"] = str(test_executable)
        _write_json(root / "package-provenance.json", provenance)
        manifest["packageProvenance"] = provenance
        verify_signature = _invoke(
            manifest,
            "verify-reused-package-signature",
            "package-check",
            ["codesign", "--verify", "--deep", "--strict", str(application_bundle)],
            repository_root,
            _build_environment(root, qt_prefix, sdk_path),
            root / "logs" / "verify-reused-package-signature",
            root,
            timeout_seconds,
        )
        _require_success(verify_signature)
    except Exception as error:
        manifest["status"] = "failed"
        manifest["failures"].append(
            {"code": "focused-package-setup-failed", "message": f"{type(error).__name__}: {error}"}
        )
        _write_json(manifest_path, manifest)
        return 1

    executed: list[str] = []
    failed: list[str] = []
    for route_id in FOCUSED_CALENDAR_ROUTE_IDS:
        definition = ROUTE_BY_ID[route_id]
        invocation = route_invocation(route_id, root, application_path, test_executable, repository_root)
        route_root = _route_paths(root, definition.artifact_path)
        if route_root.exists() or route_root.is_symlink():
            raise RunnerError(f"Focused route output path already exists: {route_root}")
        route_root.mkdir(parents=True)
        env = _route_environment(root, application_path, test_executable, qt_prefix, invocation["environmentOverrides"])
        process = _process_record(
            route_id,
            [invocation["filePath"], *invocation["arguments"]],
            repository_root,
            env,
            route_root / "runner",
            timeout_seconds,
        )
        status = "completed" if _is_success(process) else "failed"
        route_manifest = {
            "schema": ROUTE_SCHEMA,
            "taskId": TASK_ID,
            "routeId": route_id,
            "category": definition.category,
            "scenario": definition.scenario,
            "fixture": definition.fixture,
            "artifactPath": definition.artifact_path,
            "status": status,
            "invocation": process,
        }
        _write_json(route_root / "route-manifest.json", route_manifest)
        command_record = dict(process)
        command_record.update({"id": route_id, "kind": "route", "status": status})
        manifest["commands"].append(command_record)
        manifest["routes"].append(route_manifest)
        executed.append(route_id)
        if status != "completed":
            failed.append(route_id)
            manifest["failures"].append(
                {
                    "code": "route-process-failed",
                    "routeId": route_id,
                    "message": f"Route exited abnormally: exitStatus={process.get('exitStatus')}, exitCode={process.get('exitCode')}, timedOut={process.get('timedOut')}.",
                    "stdoutPath": process["stdoutPath"],
                    "stderrPath": process["stderrPath"],
                }
            )
        manifest["routeExecution"] = {
            "status": "running",
            "requestedRouteCount": len(FOCUSED_CALENDAR_ROUTE_IDS),
            "executedRouteIds": executed,
            "failedRouteIds": failed,
        }
        _write_json(manifest_path, manifest)

    manifest["status"] = "completed"
    manifest["recordedAtUtc"] = utc_now()
    manifest["routeExecution"]["status"] = "completed"
    manifest["routeExecution"]["executedRouteIds"] = executed
    manifest["routeExecution"]["failedRouteIds"] = failed
    _write_json(manifest_path, manifest)
    return 0 if len(executed) == len(FOCUSED_CALENDAR_ROUTE_IDS) and not failed else 1


def _run_route_matrix_with_reused_artifacts(
    evidence_root: Path,
    source_build_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    timeout_seconds: int,
    parallel: int,
) -> int:
    root = _validate_evidence_root(evidence_root, repository_root, create=False)
    if not repository_root.is_dir() or not qt_prefix.is_dir():
        raise RunnerError("Repository root and Qt prefix must both exist for reused-artifact route execution.")
    source_snapshot = _preflight(repository_root, qt_prefix)
    application_source, test_source, provenance = _verified_reusable_artifacts(
        source_build_root,
        repository_root,
        source_snapshot["source"]["commit"],
    )
    source_manifest = json.loads(Path(provenance["sourceRunManifestPath"]).read_text(encoding="utf-8"))
    sdk_path = source_manifest.get("source", {}).get("macosSdkPath")
    if not isinstance(sdk_path, str) or not Path(sdk_path).is_dir():
        raise RunnerError("Reusable build has no valid selected macOS SDK path.")
    if source_snapshot.get("macosSdkPath") != sdk_path:
        raise RunnerError(
            "The active macOS SDK differs from the SDK recorded by the reusable universal build: "
            f"{source_snapshot.get('macosSdkPath')!r} != {sdk_path!r}."
        )

    root = _validate_evidence_root(root, repository_root, create=True)
    application_bundle = root / "package" / "ClassMngr.app"
    application_path = application_bundle / "Contents" / "MacOS" / "ClassMngr"
    test_executable = root / "build" / "debug" / "ClassMngrStartupPerformanceTests"
    manifest = _new_manifest(
        root,
        repository_root,
        qt_prefix,
        source_snapshot,
        timeout_seconds,
        parallel,
    )
    manifest["executionTaskId"] = "QT0-MACOS-ROUTE-MATRIX-EXEC-02"
    manifest["options"]["skipBuild"] = True
    manifest["options"]["reuseBuiltArtifactsRoot"] = str(provenance["sourceEvidenceRoot"])
    manifest["paths"].update(
        {
            "applicationPath": str(application_path),
            "testExecutable": str(test_executable),
            "releaseBuildDirectory": str(Path(provenance["sourceEvidenceRoot"]) / "build" / "release"),
            "testBuildDirectory": str(Path(provenance["sourceEvidenceRoot"]) / "build" / "debug"),
            "installDirectory": str(root / "package"),
            "sourceBuildRoot": provenance["sourceEvidenceRoot"],
        }
    )
    manifest["toolchain"] = source_manifest.get("toolchain", {})
    manifest["build"] = {
        "status": "verified-reuse",
        "applicationPath": str(application_path),
        "testExecutable": str(test_executable),
        "releaseBuildDirectory": str(Path(provenance["sourceEvidenceRoot"]) / "build" / "release"),
        "testBuildDirectory": str(Path(provenance["sourceEvidenceRoot"]) / "build" / "debug"),
        "installDirectory": str(root / "package"),
        "architectures": list(EXPECTED_ARCHITECTURES),
        "deploymentTarget": DEPLOYMENT_TARGET,
        "applicationSha256": provenance["applicationSha256"],
        "testHarnessSha256": provenance["testHarnessSha256"],
        "reusedFromEvidenceRoot": provenance["sourceEvidenceRoot"],
    }
    manifest["packagingAudit"] = {
        "status": "passed-and-reused",
        "sourceSummaryPath": provenance["sourcePackagingAuditSummaryPath"],
        "sourceSummarySha256": provenance["sourcePackagingAuditSummarySha256"],
        "embeddedMachOCount": provenance["embeddedMachOCount"],
        "architectures": list(EXPECTED_ARCHITECTURES),
        "deploymentTarget": DEPLOYMENT_TARGET,
        "infoPlistMinimumVersion": provenance["infoPlistMinimumVersion"],
    }
    manifest["packageProvenance"] = provenance
    for directory in (
        root / "logs",
        root / "runtime" / "home",
        root / "runtime" / "xdg-config",
        root / "runtime" / "xdg-data",
        root / "runtime" / "xdg-cache",
        root / "runtime" / "xdg-state",
        root / "tmp",
        test_executable.parent,
    ):
        directory.mkdir(parents=True, exist_ok=True)
    manifest_path = root / "run-manifest.json"
    _write_json(manifest_path, manifest)

    try:
        shutil.copytree(application_source.parents[2], application_bundle, symlinks=True)
        shutil.copy2(test_source, test_executable)
        if _hash_file(application_path) != provenance["applicationSha256"]:
            raise RunnerError("Copied packaged app executable hash differs from verified source.")
        if _hash_file(test_executable) != provenance["testHarnessSha256"]:
            raise RunnerError("Copied Debug harness hash differs from verified source.")
        provenance["applicationCopiedPath"] = str(application_path)
        provenance["testHarnessCopiedPath"] = str(test_executable)
        manifest["packageProvenance"] = provenance
        _write_json(root / "package-provenance.json", provenance)
        _write_json(manifest_path, manifest)
        signature_check = _invoke(
            manifest,
            "verify-reused-package-signature",
            "package-check",
            ["codesign", "--verify", "--deep", "--strict", str(application_bundle)],
            repository_root,
            _build_environment(root, qt_prefix, sdk_path),
            root / "logs" / "verify-reused-package-signature",
            root,
            timeout_seconds,
        )
        _require_success(signature_check)
    except Exception as error:
        manifest["status"] = "failed"
        manifest["build"]["status"] = "failed-during-artifact-reuse"
        manifest["failures"].append(
            {"code": "reused-artifact-setup-failed", "message": f"{type(error).__name__}: {error}"}
        )
        _write_json(manifest_path, manifest)
        return 1

    return _finish_built_run(
        manifest,
        root,
        repository_root,
        qt_prefix,
        application_path,
        test_executable,
        timeout_seconds,
        sdk_path,
    )


def _read_cmake_cache(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith(("//", "#")) or ":" not in line or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.split(":", 1)[0]
        values[key] = value
    return values


def _audited_command(
    manifest: dict[str, Any],
    evidence_root: Path,
    repository_root: Path,
    env: dict[str, str],
    command_id: str,
    command: list[str],
    log_directory: Path,
    timeout_seconds: int,
) -> dict[str, Any]:
    record = _invoke(
        manifest,
        command_id,
        "package-check",
        command,
        repository_root,
        env,
        log_directory,
        evidence_root,
        timeout_seconds,
    )
    _require_success(record)
    return record


def _audit_binary(
    manifest: dict[str, Any],
    evidence_root: Path,
    repository_root: Path,
    env: dict[str, str],
    binary: Path,
    relative_label: str,
    timeout_seconds: int,
) -> dict[str, Any]:
    safe_label = re.sub(r"[^A-Za-z0-9_.-]+", "_", relative_label).strip("_") or "binary"
    file_record = _audited_command(
        manifest,
        evidence_root,
        repository_root,
        env,
        f"file-{safe_label}",
        [shutil.which("file") or "file", "-b", str(binary)],
        evidence_root / "packaging-audit" / "macho" / safe_label / "file",
        timeout_seconds,
    )
    file_output = Path(file_record["stdoutPath"]).read_text(encoding="utf-8", errors="replace").strip()
    if "Mach-O" not in file_output:
        return {"path": str(binary), "kind": "non-Mach-O", "fileDescription": file_output}
    lipo_record = _audited_command(
        manifest,
        evidence_root,
        repository_root,
        env,
        f"lipo-{safe_label}",
        [shutil.which("lipo") or "lipo", "-archs", str(binary)],
        evidence_root / "packaging-audit" / "macho" / safe_label / "lipo",
        timeout_seconds,
    )
    architecture_output = Path(lipo_record["stdoutPath"]).read_text(encoding="utf-8", errors="replace").strip()
    architectures = set(architecture_output.split())
    if architectures != set(EXPECTED_ARCHITECTURES):
        raise RunnerError(
            f"{relative_label}: expected architectures {list(EXPECTED_ARCHITECTURES)}, "
            f"found {sorted(architectures)}."
        )
    vtool_record = _audited_command(
        manifest,
        evidence_root,
        repository_root,
        env,
        f"vtool-{safe_label}",
        ["xcrun", "vtool", "-show-build", str(binary)],
        evidence_root / "packaging-audit" / "macho" / safe_label / "vtool",
        timeout_seconds,
    )
    vtool_output = Path(vtool_record["stdoutPath"]).read_text(encoding="utf-8", errors="replace")
    minimums = _check_minimum_versions(vtool_output, architectures, relative_label)
    return {
        "path": str(binary),
        "relativePath": relative_label,
        "kind": "Mach-O",
        "fileDescription": file_output,
        "architectures": sorted(architectures),
        "minimumVersions": minimums,
        "fileOutputPath": file_record["stdoutPath"],
        "lipoOutputPath": lipo_record["stdoutPath"],
        "vtoolOutputPath": vtool_record["stdoutPath"],
    }


def _audit_package_and_harness(
    manifest: dict[str, Any],
    evidence_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    release_build: Path,
    test_build: Path,
    application_path: Path,
    test_executable: Path,
    timeout_seconds: int,
    macos_sdk_path: str,
) -> dict[str, Any]:
    env = _build_environment(evidence_root, qt_prefix, macos_sdk_path)
    cache_results: dict[str, dict[str, str]] = {}
    for label, build_directory, expected_type, expected_testing in (
        ("release", release_build, "Release", "OFF"),
        ("debug", test_build, "Debug", "ON"),
    ):
        cache_path = build_directory / "CMakeCache.txt"
        values = _read_cmake_cache(cache_path)
        expected_architectures = ";".join(EXPECTED_ARCHITECTURES)
        requirements = {
            "CMAKE_BUILD_TYPE": expected_type,
            "BUILD_TESTING": expected_testing,
            "CMAKE_OSX_ARCHITECTURES": expected_architectures,
            "CMAKE_OSX_DEPLOYMENT_TARGET": DEPLOYMENT_TARGET,
            "CMAKE_OSX_SYSROOT": macos_sdk_path,
            "CMAKE_PREFIX_PATH": str(qt_prefix),
        }
        mismatches = {
            key: {"expected": value, "found": values.get(key)}
            for key, value in requirements.items()
            if values.get(key) != value
        }
        if mismatches:
            raise RunnerError(f"{label} CMake cache does not match requested build contract: {mismatches}")
        cache_results[label] = {key: values[key] for key in requirements}

    if not application_path.is_file():
        raise RunnerError(f"Packaged Release executable is missing: {application_path}")
    if not test_executable.is_file():
        raise RunnerError(f"Debug QtTest harness is missing: {test_executable}")
    app_bundle = application_path.parents[2]
    info_plist = app_bundle / "Contents" / "Info.plist"
    plist_record = _audited_command(
        manifest,
        evidence_root,
        repository_root,
        env,
        "plist-minimum-version",
        [shutil.which("plutil") or "plutil", "-extract", "LSMinimumSystemVersion", "raw", str(info_plist)],
        evidence_root / "packaging-audit" / "info-plist",
        timeout_seconds,
    )
    plist_minimum = Path(plist_record["stdoutPath"]).read_text(encoding="utf-8", errors="replace").strip()
    if plist_minimum != DEPLOYMENT_TARGET:
        raise RunnerError(
            f"Packaged LSMinimumSystemVersion is {plist_minimum!r}; expected {DEPLOYMENT_TARGET!r}."
        )

    package_root = app_bundle / "Contents"
    package_files = sorted(
        (
            path for path in package_root.rglob("*")
            if path.is_file() and not path.is_symlink()
        ),
        key=lambda path: path.as_posix(),
    )
    binary_results: list[dict[str, Any]] = []
    for path in package_files:
        relative = path.relative_to(package_root).as_posix()
        result = _audit_binary(
            manifest,
            evidence_root,
            repository_root,
            env,
            path,
            f"ClassMngr.app/Contents/{relative}",
            timeout_seconds,
        )
        if result["kind"] == "Mach-O":
            binary_results.append(result)
    app_relative = "ClassMngr.app/Contents/MacOS/ClassMngr"
    app_result = next(
        (item for item in binary_results if item["relativePath"] == app_relative),
        None,
    )
    if app_result is None:
        raise RunnerError("Packaged main executable was not detected as Mach-O.")
    qoffscreen_path = package_root / "PlugIns" / "platforms" / "libqoffscreen.dylib"
    if not any(item["path"] == str(qoffscreen_path) for item in binary_results):
        raise RunnerError("The packaged offscreen platform plugin is missing or not a universal Mach-O.")
    test_result = _audit_binary(
        manifest,
        evidence_root,
        repository_root,
        env,
        test_executable,
        "debug/ClassMngrStartupPerformanceTests",
        timeout_seconds,
    )
    if test_result["kind"] != "Mach-O":
        raise RunnerError("Debug QtTest harness was not detected as Mach-O.")
    test_result["sha256"] = _hash_file(test_executable)
    app_result["sha256"] = _hash_file(application_path)
    summary = {
        "recordedAtUtc": utc_now(),
        "expectedArchitectures": list(EXPECTED_ARCHITECTURES),
        "deploymentTarget": DEPLOYMENT_TARGET,
        "cmakeSysroot": macos_sdk_path,
        "cmakeCache": cache_results,
        "applicationPath": str(application_path),
        "testExecutable": str(test_executable),
        "infoPlistPath": str(info_plist),
        "infoPlistMinimumVersion": plist_minimum,
        "infoPlistCommandOutputPath": plist_record["stdoutPath"],
        "application": app_result,
        "testHarness": test_result,
        "embeddedMachOCount": len(binary_results),
        "embeddedMachO": binary_results,
    }
    _write_json(evidence_root / "packaging-audit" / "summary.json", summary)
    manifest["packagingAudit"] = {
        "summaryPath": str(evidence_root / "packaging-audit" / "summary.json"),
        "status": "passed",
        "embeddedMachOCount": len(binary_results),
        "architectures": list(EXPECTED_ARCHITECTURES),
        "deploymentTarget": DEPLOYMENT_TARGET,
        "cmakeSysroot": macos_sdk_path,
        "infoPlistMinimumVersion": plist_minimum,
    }
    _write_json(evidence_root / "run-manifest.json", manifest)
    return summary


def _discover_macos_sdk_path() -> str:
    result = subprocess.run(
        ["xcrun", "--sdk", "macosx", "--show-sdk-path"],
        check=True,
        capture_output=True,
        text=True,
    )
    path = result.stdout.strip()
    if not path or not Path(path).is_dir():
        raise RunnerError(f"xcrun returned an invalid macOS SDK path: {path!r}")
    return path


def _preflight(repository_root: Path, qt_prefix: Path) -> dict[str, Any]:
    if platform.system() != "Darwin":
        raise RunnerError("This runner requires macOS.")
    architecture = platform.machine().lower()
    if architecture not in {"arm64", "aarch64"}:
        raise RunnerError(f"This run requires Apple Silicon; host architecture is {architecture!r}.")
    if not repository_root.is_dir() or not (repository_root / "CMakeLists.txt").is_file():
        raise RunnerError(f"Repository root is not a ClassMngr source tree: {repository_root}")
    if not (qt_prefix / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").is_file():
        raise RunnerError(f"Qt 6 CMake package not found under: {qt_prefix}")
    if not (qt_prefix / "plugins" / "platforms" / "libqoffscreen.dylib").is_file():
        raise RunnerError(f"Qt offscreen platform plugin not found under: {qt_prefix}")
    for executable in ("cmake", "ninja", "git", "file", "lipo", "xcrun", "codesign", "plutil"):
        if shutil.which(executable) is None:
            raise RunnerError(f"Required executable is not on PATH: {executable}")
    macos_sdk_path = _discover_macos_sdk_path()
    snapshot = _git_snapshot(repository_root)
    return {
        "recordedAtUtc": utc_now(),
        "platform": platform.platform(),
        "machine": platform.machine(),
        "macVersion": platform.mac_ver()[0],
        "source": snapshot,
        "qtPrefix": str(qt_prefix),
        "macosSdkPath": macos_sdk_path,
    }


def _new_manifest(
    evidence_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    source_snapshot: dict[str, Any],
    timeout_seconds: int,
    parallel: int,
) -> dict[str, Any]:
    plan = build_plan(
        repository_root,
        evidence_root,
        qt_prefix,
        shutil.which("cmake") or "cmake",
        sys.executable,
        parallel,
        timeout_seconds,
        source_snapshot["macosSdkPath"],
    )
    return {
        "schema": RUN_SCHEMA,
        "taskId": TASK_ID,
        "runnerTaskId": RUNNER_TASK_ID,
        "executionTaskId": "QT0-MACOS-ROUTE-MATRIX-EXEC-01",
        "recordedAtUtc": utc_now(),
        "status": "running",
        "scenario": "Phase 0 packaged Release evidence automation",
        "fixture": "packaged Release macOS universal",
        **_run_manifest_platform_metadata(),
        "source": source_snapshot,
        "host": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "python": sys.version,
        },
        "paths": {
            "repositoryRoot": str(repository_root),
            "applicationPath": plan["applicationPath"],
            "testExecutable": plan["testExecutable"],
            "releaseBuildDirectory": plan["releaseBuildDirectory"],
            "testBuildDirectory": plan["testBuildDirectory"],
            "installDirectory": plan["installDirectory"],
            "qtPrefix": str(qt_prefix),
            "qtBinPath": str(qt_prefix / "bin"),
            "evidenceRoot": str(evidence_root),
        },
        "options": {
            "plan": False,
            "skipBuild": False,
            "skipRun": False,
            "skipValidation": False,
            "parallel": parallel,
            "timeoutSeconds": timeout_seconds,
            "routes": ["all"],
            "targetArchitectures": list(EXPECTED_ARCHITECTURES),
            "targetMinimumVersion": DEPLOYMENT_TARGET,
            "minimumFreeBytes": MINIMUM_FREE_BYTES,
        },
        "requestedRoutes": list(ALL_ROUTE_IDS),
        "requiredOptInEnvironmentVariables": list(REQUIRED_OPT_IN_ENVIRONMENT_VARIABLES),
        "memoryContract": {
            "legacyResidentTargetBytes": LEGACY_RESIDENT_TARGET_BYTES,
            "legacyResidentTargetMiB": 250,
            "temporaryDiagnosticCeilingBytes": TEMPORARY_DIAGNOSTIC_CEILING_BYTES,
            "temporaryDiagnosticCeilingMiB": 512,
            "legacy250MiBIsPhase0Failure": False,
            "temporary512MiBIsPhase0Failure": False,
        },
        "storagePreflight": {},
        "toolchain": {},
        "build": {"status": "pending"},
        "packagingAudit": {"status": "pending"},
        "routeExecution": {
            "status": "not-started",
            "requestedRouteCount": len(ALL_ROUTE_IDS),
            "executedRouteIds": [],
        },
        "routes": [],
        "commands": [],
        "failures": [],
        "warnings": [],
    }


def _validate_preflight_paths(repository_root: Path, qt_prefix: Path, evidence_root: Path) -> None:
    if not repository_root.is_dir():
        raise RunnerError(f"Repository root does not exist: {repository_root}")
    if not qt_prefix.is_dir():
        raise RunnerError(f"Qt prefix does not exist: {qt_prefix}")
    free_bytes = shutil.disk_usage(evidence_root.parent).free
    if free_bytes < MINIMUM_FREE_BYTES:
        raise RunnerError(
            f"Only {free_bytes} bytes are available under {evidence_root.parent}; "
            f"at least {MINIMUM_FREE_BYTES} bytes are required for isolated universal builds and evidence."
        )


def _finish_built_run(
    manifest: dict[str, Any],
    root: Path,
    repository_root: Path,
    qt_prefix: Path,
    application_path: Path,
    test_executable: Path,
    timeout_seconds: int,
    macos_sdk_path: str,
) -> int:
    manifest_path = root / "run-manifest.json"
    manifest["status"] = "running"
    _write_json(manifest_path, manifest)
    self_check = _invoke(
        manifest,
        "phase0-macos-runner-self-check",
        "self-check",
        [sys.executable, str(repository_root / "scripts" / "phase0" / "test_phase0_macos_runner.py")],
        repository_root,
        _build_environment(root, qt_prefix, macos_sdk_path),
        root / "logs" / "runner-self-check",
        root,
        timeout_seconds,
    )
    if not _is_success(self_check):
        manifest["status"] = "failed"
        manifest["routeExecution"]["status"] = "not-started"
        manifest["failures"].append(
            {
                "code": "runner-self-check-failed",
                "message": f"See {self_check['stderrPath']} and {self_check['stdoutPath']}.",
            }
        )
        _write_json(manifest_path, manifest)
        return 1

    try:
        failed_routes = _execute_routes(
            manifest,
            root,
            repository_root,
            qt_prefix,
            application_path,
            test_executable,
            timeout_seconds,
        )
    except Exception as error:
        manifest["status"] = "failed"
        manifest["routeExecution"]["status"] = "failed"
        manifest["failures"].append(
            {
                "code": "route-matrix-runner-failed",
                "message": f"{type(error).__name__}: {error}",
                "executedRouteIds": list(manifest["routeExecution"].get("executedRouteIds", [])),
            }
        )
        _write_json(manifest_path, manifest)
        return 1
    manifest["status"] = "completed"
    manifest["recordedAtUtc"] = utc_now()
    manifest["routeExecution"]["status"] = "completed"
    manifest["routeExecution"]["requestedRouteCount"] = len(ALL_ROUTE_IDS)
    manifest["routeExecution"]["executedRouteIds"] = list(ALL_ROUTE_IDS)
    manifest["routeExecution"]["failedRouteIds"] = failed_routes
    _write_json(manifest_path, manifest)

    validator_path = repository_root / "scripts" / "phase0" / "validate_phase0_evidence.py"
    validation_record = _invoke(
        manifest,
        "validate-macos-universal-evidence",
        "validation",
        [
            sys.executable,
            str(validator_path),
            "--evidence-root",
            str(root),
            "--expected-platform",
            "macos-universal",
            "--json-out",
            str(root / "validation-summary.json"),
        ],
        repository_root,
        _build_environment(root, qt_prefix, macos_sdk_path),
        root / "validation",
        root,
        timeout_seconds,
    )
    manifest["validation"] = {
        "status": "passed" if _is_success(validation_record) else "failed",
        "summaryPath": str(root / "validation-summary.json"),
        "process": validation_record,
    }
    _write_json(manifest_path, manifest)
    return 0 if _is_success(validation_record) and not failed_routes else 1


def _validate_existing_evidence_root(root: Path, repository_root: Path) -> Path:
    candidate = root.expanduser().absolute()
    if not candidate.is_dir() or candidate.is_symlink():
        raise RunnerError(f"Resume evidence root must be an existing, non-symlink directory: {candidate}")
    repository = repository_root.resolve()
    if _inside(candidate, repository):
        raise RunnerError(f"Evidence root must be outside the repository: {candidate}")
    for protected in ("build", "dist", ".git", "agent_docs", "src", "tests", "cmake"):
        protected_path = repository / protected
        if candidate == protected_path or _inside(candidate, protected_path):
            raise RunnerError(f"Evidence root selects protected project data: {candidate}")
    cursor = candidate
    while True:
        if cursor.is_symlink():
            raise RunnerError(f"Evidence path traverses a symlink: {cursor}")
        if cursor.parent == cursor:
            break
        cursor = cursor.parent
    return candidate


def _validate_resumable_build(
    manifest: dict[str, Any],
    root: Path,
    repository_root: Path,
    qt_prefix: Path,
    current_commit: str,
) -> tuple[Path, Path, str]:
    if manifest.get("schema") != RUN_SCHEMA:
        raise RunnerError("Existing run-manifest.json has an unexpected schema; refusing resume.")
    if manifest.get("executionTaskId") != "QT0-MACOS-ROUTE-MATRIX-EXEC-01":
        raise RunnerError("Existing run-manifest.json belongs to a different execution task; refusing resume.")
    recorded_source = manifest.get("source", {}).get("source", {}).get("commit")
    if recorded_source != current_commit:
        raise RunnerError(
            f"Existing build source {recorded_source!r} does not match current HEAD {current_commit!r}."
        )
    if manifest.get("status") != "failed":
        raise RunnerError("Resume requires a failed, incomplete run-manifest.json.")
    if manifest.get("options", {}).get("targetArchitectures") != list(EXPECTED_ARCHITECTURES):
        raise RunnerError("Existing run-manifest.json does not record the required universal architectures.")
    if manifest.get("options", {}).get("targetMinimumVersion") != DEPLOYMENT_TARGET:
        raise RunnerError("Existing run-manifest.json does not record the required minimum OS version.")
    if manifest.get("routeExecution", {}).get("status") != "not-started":
        raise RunnerError("Resume is allowed only before any route has started.")
    if manifest.get("routeExecution", {}).get("executedRouteIds") != [] or manifest.get("routes") != []:
        raise RunnerError("Resume refuses roots containing route records.")
    if manifest.get("build", {}).get("status") != "failed":
        raise RunnerError("Resume requires the original run to have stopped after build/package auditing.")
    if (root / "validation-summary.json").exists():
        raise RunnerError("Resume refuses a root that already contains validation-summary.json.")

    paths = manifest.get("paths", {})
    expected_paths = {
        "evidenceRoot": str(root),
        "repositoryRoot": str(repository_root),
        "releaseBuildDirectory": str(root / "build" / "release"),
        "testBuildDirectory": str(root / "build" / "debug"),
        "installDirectory": str(root / "package"),
        "applicationPath": str(root / "package" / "ClassMngr.app" / "Contents" / "MacOS" / "ClassMngr"),
        "testExecutable": str(root / "build" / "debug" / "ClassMngrStartupPerformanceTests"),
        "qtPrefix": str(qt_prefix),
    }
    if any(paths.get(key) != expected for key, expected in expected_paths.items()):
        raise RunnerError("Existing run paths do not match the requested root/repository; refusing resume.")

    required_completed_commands = (
        "configure-release",
        "build-release",
        "install-release-package",
        "copy-offscreen-platform-plugin",
        "sign-offscreen-package",
        "verify-package-signature",
        "configure-debug-tests",
        "build-debug-startup-harness",
    )
    command_by_id = {
        item.get("id"): item
        for item in manifest.get("commands", [])
        if isinstance(item, dict) and isinstance(item.get("id"), str)
    }
    for command_id in required_completed_commands:
        record = command_by_id.get(command_id)
        if not record or not _is_success(record):
            raise RunnerError(f"Required build/package command {command_id!r} did not complete successfully.")

    application_path = Path(expected_paths["applicationPath"])
    test_executable = Path(expected_paths["testExecutable"])
    if not application_path.is_file() or not test_executable.is_file():
        raise RunnerError("Expected packaged Release app or Debug harness is missing; refusing resume.")
    for build_directory in (root / "build" / "release", root / "build" / "debug"):
        if not (build_directory / "CMakeCache.txt").is_file():
            raise RunnerError(f"Expected CMake cache is missing: {build_directory / 'CMakeCache.txt'}")
    if (root / "package" / "ClassMngr.app" / "Contents" / "PlugIns" / "platforms" / "libqoffscreen.dylib").is_file() is False:
        raise RunnerError("Packaged offscreen plugin is missing; refusing resume.")
    for route in ROUTES:
        if _route_paths(root, route.artifact_path).exists():
            raise RunnerError(f"Resume refuses an existing route artifact directory: {_route_paths(root, route.artifact_path)}")
    sdk_path = manifest.get("source", {}).get("macosSdkPath")
    if not isinstance(sdk_path, str) or not Path(sdk_path).is_dir():
        raise RunnerError("Existing run-manifest.json has no valid selected macOS SDK path.")
    return application_path, test_executable, sdk_path


def _archive_manifest_exclusive(source_path: Path, archive_path: Path) -> None:
    with source_path.open("rb") as source, archive_path.open("xb") as destination:
        shutil.copyfileobj(source, destination)
        destination.flush()
        os.fsync(destination.fileno())


def _resume_built_run(
    evidence_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    timeout_seconds: int,
) -> int:
    root = _validate_existing_evidence_root(evidence_root, repository_root)
    _validate_preflight_paths(repository_root, qt_prefix, root)
    source_snapshot = _preflight(repository_root, qt_prefix)
    manifest_path = root / "run-manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RunnerError(f"Unable to read existing run-manifest.json: {error}") from error
    if not isinstance(manifest, dict):
        raise RunnerError("Existing run-manifest.json root must be an object.")
    application_path, test_executable, macos_sdk_path = _validate_resumable_build(
        manifest,
        root,
        repository_root.absolute(),
        qt_prefix.absolute(),
        source_snapshot["source"]["commit"],
    )

    archive_path = root / "run-manifest.initial-failed.json"
    _archive_manifest_exclusive(manifest_path, archive_path)
    prior_failures = manifest.get("failures", [])
    manifest["recoveryHistory"] = list(manifest.get("recoveryHistory", [])) + [
        {
            "recordedAtUtc": utc_now(),
            "reason": "The prior audit orchestration passed a string instead of a Path as its log directory; no audit process or route had started.",
            "priorStatus": "failed",
            "priorFailures": prior_failures,
            "archivedManifestPath": str(archive_path),
            "reusedBuildDirectories": [
                str(root / "build" / "release"),
                str(root / "build" / "debug"),
            ],
            "rebuildSkipped": True,
            "resumeCommand": [
                sys.executable,
                str(Path(__file__).resolve()),
                "--resume-built",
                "--evidence-root",
                str(root),
                "--repository-root",
                str(repository_root.absolute()),
                "--qt-prefix",
                str(qt_prefix.absolute()),
            ],
        }
    ]
    manifest["failures"] = []
    manifest["status"] = "running"
    manifest["build"] = {"status": "audit-recovery-in-progress"}
    manifest["packagingAudit"] = {"status": "pending-recovery"}
    manifest["recordedAtUtc"] = utc_now()
    _write_json(manifest_path, manifest)

    try:
        audit = _audit_package_and_harness(
            manifest,
            root,
            repository_root,
            qt_prefix,
            root / "build" / "release",
            root / "build" / "debug",
            application_path,
            test_executable,
            timeout_seconds,
            macos_sdk_path,
        )
    except Exception as error:
        manifest["status"] = "failed"
        manifest["build"] = {"status": "failed-during-audit-recovery"}
        if manifest.get("packagingAudit", {}).get("status") != "passed":
            manifest["packagingAudit"]["status"] = "failed-during-recovery"
        manifest["failures"].append(
            {
                "code": "packaging-audit-recovery-failed",
                "message": f"{type(error).__name__}: {error}",
            }
        )
        _write_json(manifest_path, manifest)
        return 1

    manifest["build"] = {
        "status": "completed",
        "applicationPath": str(application_path),
        "testExecutable": str(test_executable),
        "releaseBuildDirectory": str(root / "build" / "release"),
        "testBuildDirectory": str(root / "build" / "debug"),
        "installDirectory": str(root / "package"),
        "architectures": list(EXPECTED_ARCHITECTURES),
        "deploymentTarget": DEPLOYMENT_TARGET,
        "applicationSha256": audit["application"]["sha256"],
        "testHarnessSha256": audit["testHarness"]["sha256"],
        "reusedFromInitialAttempt": True,
    }
    _write_json(manifest_path, manifest)
    return _finish_built_run(
        manifest,
        root,
        repository_root,
        qt_prefix,
        application_path,
        test_executable,
        timeout_seconds,
        macos_sdk_path,
    )


def _continue_recovered_run(
    evidence_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    timeout_seconds: int,
) -> int:
    root = _validate_existing_evidence_root(evidence_root, repository_root)
    _validate_preflight_paths(repository_root, qt_prefix, root)
    source_snapshot = _preflight(repository_root, qt_prefix)
    manifest_path = root / "run-manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RunnerError(f"Unable to read existing run-manifest.json: {error}") from error
    if not isinstance(manifest, dict):
        raise RunnerError("Existing run-manifest.json root must be an object.")
    if manifest.get("schema") != RUN_SCHEMA or manifest.get("executionTaskId") != "QT0-MACOS-ROUTE-MATRIX-EXEC-01":
        raise RunnerError("Existing run-manifest.json does not match this execution task.")
    if manifest.get("status") != "running":
        raise RunnerError("Continuation requires a running manifest left after the recovered audit.")
    if manifest.get("build", {}).get("status") != "completed":
        raise RunnerError("Continuation requires the completed Release/Debug build record.")
    if manifest.get("packagingAudit", {}).get("status") != "passed":
        raise RunnerError("Continuation requires a passed package and universal-slice audit.")
    if manifest.get("routeExecution", {}).get("status") != "not-started":
        raise RunnerError("Continuation is allowed only before any route has started.")
    if manifest.get("routeExecution", {}).get("executedRouteIds") != [] or manifest.get("routes") != []:
        raise RunnerError("Continuation refuses roots containing route records.")
    if (root / "validation-summary.json").exists():
        raise RunnerError("Continuation refuses a root that already contains validation-summary.json.")
    if not manifest.get("recoveryHistory"):
        raise RunnerError("Continuation requires a recorded recovery history.")
    recorded_commit = manifest.get("source", {}).get("source", {}).get("commit")
    if recorded_commit != source_snapshot["source"]["commit"]:
        raise RunnerError("The current source commit does not match the audited build.")

    paths = manifest.get("paths", {})
    application_path = root / "package" / "ClassMngr.app" / "Contents" / "MacOS" / "ClassMngr"
    test_executable = root / "build" / "debug" / "ClassMngrStartupPerformanceTests"
    sdk_path = manifest.get("source", {}).get("macosSdkPath")
    if (
        paths.get("evidenceRoot") != str(root)
        or paths.get("repositoryRoot") != str(repository_root.absolute())
        or paths.get("applicationPath") != str(application_path)
        or paths.get("testExecutable") != str(test_executable)
        or paths.get("qtPrefix") != str(qt_prefix.absolute())
        or not isinstance(sdk_path, str)
        or not Path(sdk_path).is_dir()
    ):
        raise RunnerError("Existing run paths or SDK do not match the requested continuation.")
    if not application_path.is_file() or not test_executable.is_file():
        raise RunnerError("The previously audited Release app or Debug harness is missing.")
    if _hash_file(application_path) != manifest["build"].get("applicationSha256"):
        raise RunnerError("The packaged Release app changed after the successful audit.")
    if _hash_file(test_executable) != manifest["build"].get("testHarnessSha256"):
        raise RunnerError("The Debug harness changed after the successful audit.")
    for route in ROUTES:
        route_root = _route_paths(root, route.artifact_path)
        if route_root.exists() or route_root.is_symlink():
            raise RunnerError(f"Continuation refuses an existing route artifact path: {route_root}")

    archive_path = root / "run-manifest.audit-complete-before-route.json"
    _archive_manifest_exclusive(manifest_path, archive_path)
    manifest["recoveryHistory"].append(
        {
            "recordedAtUtc": utc_now(),
            "reason": "A continuation helper recursion error occurred after package audit completion and before self-check/routes.",
            "priorStatus": "running",
            "priorFailures": list(manifest.get("failures", [])),
            "archivedManifestPath": str(archive_path),
            "routesStarted": False,
            "rebuildSkipped": True,
            "continueCommand": [
                sys.executable,
                str(Path(__file__).resolve()),
                "--continue-recovered",
                "--evidence-root",
                str(root),
                "--repository-root",
                str(repository_root.absolute()),
                "--qt-prefix",
                str(qt_prefix.absolute()),
            ],
        }
    )
    manifest["recordedAtUtc"] = utc_now()
    _write_json(manifest_path, manifest)
    return _finish_built_run(
        manifest,
        root,
        repository_root,
        qt_prefix,
        application_path,
        test_executable,
        timeout_seconds,
        sdk_path,
    )


def _run(
    evidence_root: Path,
    repository_root: Path,
    qt_prefix: Path,
    timeout_seconds: int,
    parallel: int,
) -> int:
    root = _validate_evidence_root(evidence_root, repository_root, create=False)
    _validate_preflight_paths(repository_root, qt_prefix, root)
    source_snapshot = _preflight(repository_root, qt_prefix)
    root = _validate_evidence_root(root, repository_root, create=True)
    runtime = root / "runtime"
    for directory in (
        root / "logs",
        root / "build",
        root / "package",
        root / "installer-output",
        root / "runtime" / "home",
        root / "runtime" / "xdg-config",
        root / "runtime" / "xdg-data",
        root / "runtime" / "xdg-cache",
        root / "runtime" / "xdg-state",
        root / "tmp",
        root / "validation",
    ):
        directory.mkdir(parents=True, exist_ok=True)
    del runtime

    manifest = _new_manifest(
        root,
        repository_root,
        qt_prefix,
        source_snapshot,
        timeout_seconds,
        parallel,
    )
    free_bytes = shutil.disk_usage(root.parent).free
    manifest["storagePreflight"] = {
        "filesystemPath": str(root.parent),
        "freeBytesBeforeRun": free_bytes,
        "minimumFreeBytes": MINIMUM_FREE_BYTES,
        "existingBuildSizesBytes": {
            "debug": 5_423_960 * 1024,
            "release": 1_950_888 * 1024,
            "dist": 1_709_684 * 1024,
        },
    }
    manifest_path = root / "run-manifest.json"
    _write_json(manifest_path, manifest)
    macos_sdk_path = source_snapshot["macosSdkPath"]
    build_env = _build_environment(root, qt_prefix, macos_sdk_path)
    try:
        manifest["toolchain"] = _toolchain_versions(
            manifest,
            repository_root,
            root,
            build_env,
            timeout_seconds,
        )
        if manifest["toolchain"].get("macosSdkPath") != macos_sdk_path:
            raise RunnerError(
                "The macOS SDK selected during preflight changed before build: "
                f"{macos_sdk_path!r} != {manifest['toolchain'].get('macosSdkPath')!r}."
            )
        cmake = shutil.which("cmake") or "cmake"
        build_root = root / "build"
        release_build = build_root / "release"
        test_build = build_root / "debug"
        install_prefix = root / "package"
        installer_output = root / "installer-output"
        app_bundle = install_prefix / "ClassMngr.app"
        application_path = app_bundle / "Contents" / "MacOS" / "ClassMngr"
        test_executable = test_build / "ClassMngrStartupPerformanceTests"
        qoffscreen_source = qt_prefix / "plugins" / "platforms" / "libqoffscreen.dylib"
        qoffscreen_destination = app_bundle / "Contents" / "PlugIns" / "platforms" / "libqoffscreen.dylib"

        release_config = _invoke(
            manifest,
            "configure-release",
            "configure",
            _cmake_configure_command(
                cmake,
                repository_root,
                release_build,
                qt_prefix,
                install_prefix,
                installer_output,
                "Release",
                False,
                macos_sdk_path,
            ),
            repository_root,
            build_env,
            root / "logs" / "configure-release",
            root,
            timeout_seconds,
        )
        _require_success(release_config)
        release_build_result = _invoke(
            manifest,
            "build-release",
            "build",
            [cmake, "--build", str(release_build), "--target", "ClassMngr", "--parallel", str(parallel)],
            repository_root,
            build_env,
            root / "logs" / "build-release",
            root,
            timeout_seconds,
        )
        _require_success(release_build_result)
        install_result = _invoke(
            manifest,
            "install-release-package",
            "package",
            [cmake, "--install", str(release_build), "--prefix", str(install_prefix)],
            repository_root,
            build_env,
            root / "logs" / "install-release",
            root,
            timeout_seconds,
        )
        _require_success(install_result)
        if not qoffscreen_destination.parent.is_dir():
            qoffscreen_destination.parent.mkdir(parents=True, exist_ok=True)
        copy_result = _invoke(
            manifest,
            "copy-offscreen-platform-plugin",
            "package",
            [cmake, "-E", "copy_if_different", str(qoffscreen_source), str(qoffscreen_destination)],
            repository_root,
            build_env,
            root / "logs" / "copy-offscreen-plugin",
            root,
            timeout_seconds,
        )
        _require_success(copy_result)
        sign_result = _invoke(
            manifest,
            "sign-offscreen-package",
            "package",
            ["codesign", "--force", "--deep", "--sign", "-", str(app_bundle)],
            repository_root,
            build_env,
            root / "logs" / "sign-package",
            root,
            timeout_seconds,
        )
        _require_success(sign_result)
        signature_result = _invoke(
            manifest,
            "verify-package-signature",
            "package-check",
            ["codesign", "--verify", "--deep", "--strict", str(app_bundle)],
            repository_root,
            build_env,
            root / "logs" / "verify-package-signature",
            root,
            timeout_seconds,
        )
        _require_success(signature_result)
        debug_config = _invoke(
            manifest,
            "configure-debug-tests",
            "configure",
            _cmake_configure_command(
                cmake,
                repository_root,
                test_build,
                qt_prefix,
                root / "test-install",
                installer_output,
                "Debug",
                True,
                macos_sdk_path,
            ),
            repository_root,
            build_env,
            root / "logs" / "configure-debug",
            root,
            timeout_seconds,
        )
        _require_success(debug_config)
        debug_build_result = _invoke(
            manifest,
            "build-debug-startup-harness",
            "build",
            [
                cmake,
                "--build",
                str(test_build),
                "--target",
                "ClassMngrStartupPerformanceTests",
                "--parallel",
                str(parallel),
            ],
            repository_root,
            build_env,
            root / "logs" / "build-debug-harness",
            root,
            timeout_seconds,
        )
        _require_success(debug_build_result)
        audit = _audit_package_and_harness(
            manifest,
            root,
            repository_root,
            qt_prefix,
            release_build,
            test_build,
            application_path,
            test_executable,
            timeout_seconds,
            macos_sdk_path,
        )
        manifest["build"] = {
            "status": "completed",
            "applicationPath": str(application_path),
            "testExecutable": str(test_executable),
            "releaseBuildDirectory": str(release_build),
            "testBuildDirectory": str(test_build),
            "installDirectory": str(install_prefix),
            "architectures": list(EXPECTED_ARCHITECTURES),
            "deploymentTarget": DEPLOYMENT_TARGET,
            "applicationSha256": audit["application"]["sha256"],
            "testHarnessSha256": audit["testHarness"]["sha256"],
        }
        _write_json(manifest_path, manifest)
    except Exception as error:
        manifest["status"] = "failed"
        manifest["build"]["status"] = "failed"
        manifest["failures"].append(
            {
                "code": "build-or-package-failed",
                "message": f"{type(error).__name__}: {error}",
            }
        )
        if manifest.get("packagingAudit", {}).get("status") == "pending":
            manifest["packagingAudit"]["status"] = "failed-or-not-reached"
        _write_json(manifest_path, manifest)
        return 1

    return _finish_built_run(
        manifest,
        root,
        repository_root,
        qt_prefix,
        application_path,
        test_executable,
        timeout_seconds,
        macos_sdk_path,
    )


def _default_repository_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _default_qt_prefix() -> Path:
    value = os.environ.get("QT_MACOS_PREFIX", "")
    if value:
        return Path(value).expanduser().absolute()
    return Path.home() / "Qt" / "6.12.0" / "macos"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-root", type=Path, required=True, help="evidence root; new unless --resume-built is used")
    parser.add_argument("--repository-root", type=Path, default=_default_repository_root())
    parser.add_argument("--qt-prefix", type=Path, default=_default_qt_prefix())
    parser.add_argument("--timeout-seconds", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--parallel", type=int, default=DEFAULT_PARALLEL)
    parser.add_argument("--plan", action="store_true", help="print exact build and route commands without writing or executing")
    parser.add_argument(
        "--resume-built",
        action="store_true",
        help="resume audit/routes from a failed root whose universal Release and Debug builds already completed",
    )
    parser.add_argument(
        "--continue-recovered",
        action="store_true",
        help="continue self-check/routes from a root whose recovered build and packaging audit already passed",
    )
    parser.add_argument(
        "--focused-calendar-routes",
        action="store_true",
        help="run only the two Calendar Import diagnostics using verified artifacts from --built-artifacts-root",
    )
    parser.add_argument(
        "--reuse-built-artifacts",
        action="store_true",
        help="run the full 24-route matrix in a fresh root using verified artifacts from --built-artifacts-root",
    )
    parser.add_argument(
        "--built-artifacts-root",
        type=Path,
        help="read-only source root for a focused or full route run that reuses audited build artifacts",
    )
    args = parser.parse_args(argv)
    if args.timeout_seconds < 1:
        parser.error("--timeout-seconds must be positive")
    if args.parallel < 1:
        parser.error("--parallel must be positive")
    repository_root = args.repository_root.expanduser().absolute()
    qt_prefix = args.qt_prefix.expanduser().absolute()
    evidence_root = args.evidence_root.expanduser().absolute()
    try:
        if sum((args.resume_built, args.continue_recovered, args.focused_calendar_routes, args.reuse_built_artifacts)) > 1:
            parser.error(
                "--resume-built, --continue-recovered, --focused-calendar-routes, and "
                "--reuse-built-artifacts are mutually exclusive"
            )
        if args.focused_calendar_routes or args.reuse_built_artifacts:
            if args.plan:
                parser.error("reused-artifact route modes cannot be combined with --plan")
            if args.built_artifacts_root is None:
                parser.error("reused-artifact route modes require --built-artifacts-root")
            if args.reuse_built_artifacts:
                return _run_route_matrix_with_reused_artifacts(
                    evidence_root,
                    args.built_artifacts_root,
                    repository_root,
                    qt_prefix,
                    args.timeout_seconds,
                    args.parallel,
                )
            return _run_focused_calendar_routes(
                evidence_root,
                args.built_artifacts_root,
                repository_root,
                qt_prefix,
                args.timeout_seconds,
            )
        if args.built_artifacts_root is not None:
            parser.error("--built-artifacts-root requires a reused-artifact route mode")
        if args.resume_built:
            if args.plan:
                parser.error("--resume-built cannot be combined with --plan")
            return _resume_built_run(
                evidence_root,
                repository_root,
                qt_prefix,
                args.timeout_seconds,
            )
        if args.continue_recovered:
            if args.plan:
                parser.error("--continue-recovered cannot be combined with --plan")
            return _continue_recovered_run(
                evidence_root,
                repository_root,
                qt_prefix,
                args.timeout_seconds,
            )
        _validate_evidence_root(evidence_root, repository_root, create=False)
        if args.plan:
            macos_sdk_path = _discover_macos_sdk_path()
            plan = build_plan(
                repository_root,
                evidence_root,
                qt_prefix,
                shutil.which("cmake") or "cmake",
                sys.executable,
                args.parallel,
                args.timeout_seconds,
                macos_sdk_path,
            )
            print(json.dumps(plan, indent=2, ensure_ascii=False))
            return 0
        return _run(
            evidence_root,
            repository_root,
            qt_prefix,
            args.timeout_seconds,
            args.parallel,
        )
    except (RunnerError, OSError, subprocess.SubprocessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
