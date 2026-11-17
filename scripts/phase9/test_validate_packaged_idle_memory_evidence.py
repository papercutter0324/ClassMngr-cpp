from __future__ import annotations

import json
import hashlib
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

from validate_packaged_idle_memory_evidence import (
    EXPECTED_STARTUP_BOUNDARIES,
    EXPECTED_SCENARIOS,
    STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS,
    STARTUP_BOUNDARY_METRICS_SCHEMA,
    THRESHOLD_BYTES,
    _canonical_package_sha,
    main,
    validate_documents,
)

BUILD_LOG_BYTES = b"Successful CMake Release prerequisite build fixture.\n"


def timestamp(value: datetime) -> str:
    return value.isoformat(timespec="milliseconds").replace("+00:00", "Z")


def valid_documents(startup_bytes: int = 90_000_000, idle_bytes: int = 95_000_000):
    run_started = datetime(2026, 11, 10, 9, 0, tzinfo=timezone.utc)
    executable = r"C:\runner-temp\phase9\package\ClassMngrNext.exe"
    image_sha = "a" * 64
    source_files = [
        {"path": "CMakeLists.txt", "sizeBytes": 100, "sha256": "b" * 64},
        {"path": "CMakePresets.json", "sizeBytes": 150, "sha256": "1" * 64},
        {
            "path": "docs/qt-rewrite/phase-4-resource-inventory.json",
            "sizeBytes": 12_000,
            "sha256": "5" * 64,
        },
        {"path": "licenses/qt/LGPL-3.0-only.txt", "sizeBytes": 20_000, "sha256": "6" * 64},
        {"path": "scripts/phase4/bake_resources.py", "sizeBytes": 28_000, "sha256": "7" * 64},
        {
            "path": "scripts/phase4/generate_resource_catalog.py",
            "sizeBytes": 35_000,
            "sha256": "8" * 64,
        },
        {
            "path": "scripts/phase4/generate_resource_inventory.py",
            "sizeBytes": 40_000,
            "sha256": "9" * 64,
        },
        {
            "path": "scripts/phase9/run_packaged_idle_memory_gate.ps1",
            "sizeBytes": 37_000,
            "sha256": "2" * 64,
        },
        {
            "path": "scripts/phase9/test_validate_packaged_idle_memory_evidence.py",
            "sizeBytes": 18_000,
            "sha256": "3" * 64,
        },
        {
            "path": "scripts/phase9/validate_packaged_idle_memory_evidence.py",
            "sizeBytes": 31_000,
            "sha256": "4" * 64,
        },
        {
            "path": "third_party/sqlite/3.54.1/sqlite3.c",
            "sizeBytes": 9_000_000,
            "sha256": "a" * 64,
        },
        {
            "path": "third_party/sqlite/3.54.1/sqlite3.h",
            "sizeBytes": 650_000,
            "sha256": "b" * 64,
        },
        {"path": "src/main.cpp", "sizeBytes": 200, "sha256": "c" * 64},
    ]
    source_files.sort(key=lambda item: item["path"].casefold())
    source_sha = _canonical_package_sha(source_files)
    package_files = [
        {"path": "ClassMngrNext.exe", "sizeBytes": 1_000_000, "sha256": image_sha},
        {"path": "Qt6Core.dll", "sizeBytes": 5_000_000, "sha256": "c" * 64},
    ]
    package_files.sort(key=lambda item: item["path"].casefold())
    package_sha = _canonical_package_sha(package_files)
    qt_prefix = r"C:\Qt\6.14.2\msvc2022_64"
    qt_bin = qt_prefix + r"\bin"
    qt6_directory = qt_prefix + r"\lib\cmake\Qt6"
    qt_installation_id = hashlib.sha256(
        f"{qt_prefix}\n6.14.2\n{qt6_directory}\n".encode("utf-8")
    ).hexdigest()
    fixture_generator_specs = (
        (
            "empty-workspace-fixture-generator",
            r"C:\Git\ClassMngr-cpp\build\windows-x64-release\Release\ClassMngrPhase7NextBlankFixtureTests.exe",
            "8" * 64,
        ),
        (
            "representative-workspace-fixture-generator",
            r"C:\Git\ClassMngr-cpp\build\windows-x64-release\Release\ClassMngrStartupPerformanceTests.exe",
            "9" * 64,
        ),
    )
    qt_fixture_generators = [
        {
            "id": generator_id,
            "executablePath": generator_path,
            "executableSha256": generator_sha,
            "runtimeSearchPathPrefix": qt_bin,
            "colocatedQtDlls": [],
            "verifiedSameQtInstallation": True,
        }
        for generator_id, generator_path, generator_sha in fixture_generator_specs
    ]
    manifest = {
        "schema": "classmngr-phase9-packaged-idle-memory-v2",
        "taskId": "P9_06_PACKAGED_STARTUP_IDLE_GATE",
        "status": "completed",
        "revision": "d" * 40,
        "startedAtUtc": timestamp(run_started),
        "completedAtUtc": timestamp(run_started + timedelta(minutes=11)),
        "thresholdBytes": THRESHOLD_BYTES,
        "comparison": "strictly-less-than",
        "idleSeconds": 300,
        "sampleIntervalSeconds": 1,
        "startupBoundaryMetrics": {
            "schema": STARTUP_BOUNDARY_METRICS_SCHEMA,
            "inventoryFile": "startup-boundary-metrics.jsonl",
            "expectedEventIds": [boundary["eventId"] for boundary in EXPECTED_STARTUP_BOUNDARIES],
            "samplesPerScenario": len(EXPECTED_STARTUP_BOUNDARIES),
            "maxSampleDelayMilliseconds": STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS,
        },
        "advisoryScenarios": [
            {"id": "fresh-initial-setup", "budgetScope": "advisory"}
        ],
        "candidateSource": {
            "revision": "d" * 40,
            "sha256": source_sha,
            "includedRoots": [
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
            ],
            "inventoryFile": "source-files.json",
            "fileCount": len(source_files),
            "sha256AtRunEnd": source_sha,
            "stableDuringRun": True,
        },
        "configuration": {
            "preset": "windows-x64-release",
            "buildType": "Release",
            "architecture": "x64",
            "releasePackage": True,
            "generator": "Visual Studio 18 2026",
            "cmakeCachePath": r"C:\Git\ClassMngr-cpp\build\windows-x64-release\CMakeCache.txt",
            "cmakeCacheSha256": "9" * 64,
            "cmakeCacheSha256AtRunEnd": "9" * 64,
            "cmakeCacheStableDuringRun": True,
            "releaseCompiler": {
                "compiler": "MSVC",
                "generator": "Visual Studio 18 2026",
                "configuration": "Release",
                "platform": "x64",
                "cFlagsRelease": {
                    "raw": "/O2 /Ob2 /DNDEBUG",
                    "optimizationOptions": ["/O2"],
                    "definesNdebug": True,
                },
                "cxxFlagsRelease": {
                    "raw": "/O2 /Ob2 /DNDEBUG",
                    "optimizationOptions": ["/O2"],
                    "definesNdebug": True,
                },
                "projectPath": r"C:\Git\ClassMngr-cpp\build\windows-x64-release\cmake\next_app\ClassMngrNext.vcxproj",
                "projectSha256": "7" * 64,
                "projectOptimizationSetting": "MaxSpeed",
            },
            "packageBuild": {
                "targets": [
                    "ClassMngrNextResourcePackage",
                    "ClassMngrPhase7NextBlankFixtureTests",
                    "ClassMngrStartupPerformanceTests",
                ],
                "command": [
                    r"C:\Program Files\CMake\bin\cmake.exe",
                    "--build",
                    r"C:\Git\ClassMngr-cpp\build\windows-x64-release",
                    "--config",
                    "Release",
                    "--target",
                    "ClassMngrNextResourcePackage",
                    "ClassMngrPhase7NextBlankFixtureTests",
                    "ClassMngrStartupPerformanceTests",
                ],
                "cmakeExecutablePath": r"C:\Program Files\CMake\bin\cmake.exe",
                "cmakeExecutableSha256": "5" * 64,
                "startedAtUtc": timestamp(run_started),
                "completedAtUtc": timestamp(run_started + timedelta(seconds=2)),
                "exitCode": 0,
                "succeeded": True,
                "logPath": r"C:\runner-temp\phase9\cmake-build.log",
                "logSha256": hashlib.sha256(BUILD_LOG_BYTES).hexdigest(),
            },
        },
        "qtInstallation": {
            "id": qt_installation_id,
            "version": "6.14.2",
            "prefix": qt_prefix,
            "qt6Directory": qt6_directory,
            "coreDllPath": qt_bin + r"\Qt6Core.dll",
            "coreDllSha256": "a" * 64,
            "windeployqtPath": qt_bin + r"\windeployqt.exe",
            "windeployqtSha256": "b" * 64,
            "windeployqtVersion": "6.14.2",
            "fixtureGenerators": qt_fixture_generators,
        },
        "os": {"platform": "Windows", "architecture": "x64"},
        "application": {
            "canonicalPath": executable,
            "sha256": image_sha,
            "architecture": "x64",
            "candidateSourceSha256": source_sha,
            "releaseExecutablePath": r"C:\Git\ClassMngr-cpp\build\next-package-build\Release\ClassMngr.exe",
            "releaseExecutableSha256": image_sha,
        },
        "package": {
            "id": "classmngr-next-windows-x64-release-stage-v1",
            "sourceReleasePackage": True,
            "inventoryFile": "package-files.json",
            "sha256": package_sha,
            "fileCount": len(package_files),
            "applicationRelativePath": "ClassMngrNext.exe",
            "applicationSha256": image_sha,
            "candidateSourceSha256": source_sha,
        },
        "scenarios": [],
    }
    metrics: list[dict] = []
    startup_events: list[dict] = []
    startup_boundary_metrics: list[dict] = []
    fixture_specs = (
        ("schema-only-v6-empty", 0, "e", 0),
        ("synthetic-large-startup-96-classes", 96, "f", 1),
    )
    next_start = run_started + timedelta(seconds=1)
    for scenario_index, scenario_id in enumerate(EXPECTED_SCENARIOS):
        started_at = next_start
        created_at = started_at + timedelta(milliseconds=10)
        ready_at = created_at + timedelta(seconds=5)
        observed_at = ready_at + timedelta(milliseconds=10)
        workspace_opened_at = ready_at - timedelta(milliseconds=100)
        fixture_id, class_count, fixture_sha_character, generator_index = fixture_specs[scenario_index]
        generator_id, generator_path, generator_sha = fixture_generator_specs[generator_index]
        fixture_path = rf"C:\runner-temp\phase9\fixtures\{scenario_id}.tps"
        pid = 6000 + scenario_index
        memory = {
            "working_set_bytes": startup_bytes,
            "peak_working_set_bytes": max(startup_bytes, idle_bytes),
            "private_usage_bytes": 120_000_000,
            "pagefile_usage_bytes": 120_000_000,
            "handle_count": 90,
            "thread_count": 16,
            "private_working_set_bytes": 110_000_000,
        }
        checkpoint = {
            "stage": "startup-ready",
            "outcome": "completed",
            "sample_available": True,
            "platform": "windows",
            "metrics": memory,
        }
        workspace_checkpoint = {
            "stage": "workspace-opened",
            "outcome": "completed",
            "sample_available": True,
            "platform": "windows",
            "metrics": memory,
        }
        workspace_line = (
            "ClassMngrNext startup stage=workspace-opened outcome=completed "
            "diagnostic=none diagnostic_stage=none checkpoint="
            + json.dumps(workspace_checkpoint, separators=(",", ":"))
        )
        workspace_diagnostic = {
            "stage": "workspace-opened",
            "outcome": "completed",
            "diagnostic": "none",
            "diagnosticStage": "none",
            "stream": "stderr",
            "observedAtUtc": timestamp(workspace_opened_at),
            "checkpoint": workspace_checkpoint,
            "rawLine": workspace_line,
        }
        boundary_observation_times = (
            created_at + timedelta(seconds=1),
            created_at + timedelta(seconds=1, milliseconds=100),
            created_at + timedelta(seconds=1, milliseconds=200),
            created_at + timedelta(seconds=1, milliseconds=300),
            workspace_opened_at + timedelta(milliseconds=50),
        )
        boundary_working_sets = (
            31_000_000,
            310_000_000,
            580_000_000,
            589_000_000,
            660_000_000,
        )
        for boundary_index, (boundary, event_at, working_set) in enumerate(
            zip(EXPECTED_STARTUP_BOUNDARIES, boundary_observation_times, boundary_working_sets, strict=True)
        ):
            if boundary_index == len(EXPECTED_STARTUP_BOUNDARIES) - 1:
                startup_events.append(
                    {
                        "scenarioId": scenario_id,
                        "stream": "stderr",
                        "observedAtUtc": timestamp(workspace_opened_at),
                        "line": workspace_line,
                    }
                )
            if boundary["kind"] == "stage":
                boundary_checkpoint = {
                    "stage": boundary["value"],
                    "outcome": "completed",
                    "sample_available": True,
                    "platform": "windows",
                    "metrics": memory,
                }
                boundary_line = (
                    f"ClassMngrNext startup stage={boundary['value']} outcome=completed "
                    "diagnostic=none diagnostic_stage=none checkpoint="
                    + json.dumps(boundary_checkpoint, separators=(",", ":"))
                )
            else:
                boundary_checkpoint = {
                    "event": "resource-load",
                    "resource_id": boundary["value"],
                    "outcome": "succeeded",
                    "request_scope": {"kind": "startup"},
                }
                boundary_line = "ClassMngrNext resource checkpoint=" + json.dumps(
                    boundary_checkpoint, separators=(",", ":")
                )
            startup_log_index = len(startup_events)
            event_text = timestamp(event_at)
            startup_events.append(
                {
                    "scenarioId": scenario_id,
                    "stream": "stderr",
                    "observedAtUtc": event_text,
                    "line": boundary_line,
                }
            )
            startup_boundary_metrics.append(
                {
                    "scenarioId": scenario_id,
                    "eventId": boundary["eventId"],
                    "startupLogIndex": startup_log_index,
                    "eventObservedAtUtc": event_text,
                    "timestampUtc": timestamp(event_at + timedelta(milliseconds=25)),
                    "sampleDelayMilliseconds": 25.0,
                    "phase": "startup-boundary",
                    "pid": pid,
                    "processCreatedAtUtc": timestamp(created_at),
                    "canonicalExecutablePath": executable,
                    "architecture": "x64",
                    "sha256": image_sha,
                    "currentWorkingSetBytes": working_set,
                    "privateBytes": max(1, working_set - 7_000_000),
                    "peakWorkingSetBytes": working_set + 10_000_000,
                    "handles": 1035 if boundary["eventId"] == "shell-created" else 277,
                    "threads": 6,
                    "stream": "stderr",
                    "rawLine": boundary_line,
                }
            )
        raw_line = (
            "ClassMngrNext startup stage=startup-ready outcome=completed "
            "diagnostic=none diagnostic_stage=none checkpoint="
            + json.dumps(checkpoint, separators=(",", ":"))
        )
        diagnostic = {
            "stage": "startup-ready",
            "outcome": "completed",
            "diagnostic": "none",
            "diagnosticStage": "none",
            "stream": "stderr",
            "observedAtUtc": timestamp(observed_at),
            "checkpoint": checkpoint,
            "rawLine": raw_line,
        }
        startup_events.append(
            {
                "scenarioId": scenario_id,
                "stream": "stderr",
                "observedAtUtc": timestamp(observed_at),
                "line": raw_line,
            }
        )
        scenario_metrics = []
        for sample_index in range(301):
            sampled_at = observed_at + timedelta(milliseconds=100) + timedelta(seconds=sample_index)
            ws = startup_bytes if sample_index == 0 else (
                idle_bytes if sample_index == 300 else min(startup_bytes, idle_bytes) + sample_index
            )
            row = {
                "scenarioId": scenario_id,
                "sampleIndex": sample_index,
                "phase": "startup-ready" if sample_index == 0 else "idle",
                "timestampUtc": timestamp(sampled_at),
                "pid": pid,
                "processCreatedAtUtc": timestamp(created_at),
                "canonicalExecutablePath": executable,
                "architecture": "x64",
                "sha256": image_sha,
                "currentWorkingSetBytes": ws,
                "privateBytes": 120_000_000 + sample_index,
                "peakWorkingSetBytes": max(startup_bytes, idle_bytes),
                "handles": 90,
                "threads": 16,
            }
            metrics.append(row)
            scenario_metrics.append(row)
        idle_completed = datetime.fromisoformat(
            scenario_metrics[-1]["timestampUtc"].replace("Z", "+00:00")
        ) + timedelta(milliseconds=20)
        close_requested = idle_completed + timedelta(milliseconds=20)
        exited_at = close_requested + timedelta(milliseconds=100)
        manifest["scenarios"].append(
            {
                "id": scenario_id,
                "outcome": "completed",
                "budgetScope": "normal-use-gate",
                "startedAtUtc": timestamp(started_at),
                "fixture": {
                    "id": fixture_id,
                    "sha256": fixture_sha_character * 64,
                    "classCount": class_count,
                    "profilePathArgumentSupplied": True,
                    "pathPassedToApp": fixture_path,
                    "generatorExecutablePath": generator_path,
                    "generatorExecutableSha256": generator_sha,
                    "qtInstallationId": qt_installation_id,
                },
                "workspaceOpenedDiagnostic": workspace_diagnostic,
                "process": {
                    "pid": pid,
                    "createdAtUtc": timestamp(created_at),
                    "canonicalExecutablePath": executable,
                    "architecture": "x64",
                    "sha256": image_sha,
                    "mainWindowHandle": 1234 + scenario_index,
                    "mainWindowTitle": "ClassMngr Next",
                },
                "readiness": {
                    "windowVisible": True,
                    "stablePolls": 3,
                    "mainWindowHandle": 1234 + scenario_index,
                    "mainWindowTitle": "ClassMngr Next",
                },
                "startupReadyDiagnostic": diagnostic,
                "startupBoundarySampleCount": len(EXPECTED_STARTUP_BOUNDARIES),
                "startupBoundaryEventIds": [
                    boundary["eventId"] for boundary in EXPECTED_STARTUP_BOUNDARIES
                ],
                "idleSeconds": 300,
                "sampleIntervalSeconds": 1,
                "userInteractionDuringIdle": False,
                "idleCompletedAtUtc": timestamp(idle_completed),
                "closeRequestedAtUtc": timestamp(close_requested),
                "exitedAtUtc": timestamp(exited_at),
                "normalExit": True,
                "exitCode": 0,
            }
        )
        next_start = exited_at + timedelta(seconds=1)
    manifest["completedAtUtc"] = timestamp(next_start + timedelta(seconds=1))
    return manifest, metrics, startup_events, package_files, source_files, startup_boundary_metrics


class PackagedIdleMemoryEvidenceTests(unittest.TestCase):
    def test_valid_under_budget_evidence_passes(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()

        summary = validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

        self.assertEqual(summary["validation"], "valid")
        self.assertEqual(summary["budgetResult"], "passed")
        self.assertEqual(summary["sampleCount"], 602)
        self.assertEqual(summary["thresholdBytes"], THRESHOLD_BYTES)
        self.assertEqual(summary["scenarios"][1]["sampleCount"], 301)
        self.assertEqual(summary["startupBoundaryMetrics"]["sampleCount"], 10)

    def test_valid_startup_boundary_samples_are_diagnostic_only(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()

        summary = validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

        boundaries = summary["scenarios"][0]["startupBoundaryMetrics"]
        self.assertEqual([item["eventId"] for item in boundaries], [
            boundary["eventId"] for boundary in EXPECTED_STARTUP_BOUNDARIES
        ])
        self.assertEqual(len(boundaries), 5)
        self.assertGreater(boundaries[-1]["currentWorkingSetBytes"], THRESHOLD_BYTES)
        self.assertEqual(summary["budgetResult"], "passed")
        scenario_lines = [
            event["line"] for event in events if event["scenarioId"] == "empty-workspace"
        ]
        positions = {
            marker: next(index for index, line in enumerate(scenario_lines) if marker in line)
            for marker in (
                "stage=required-fonts-loaded ",
                "stage=workspace-opened ",
                "stage=shell-created ",
                "stage=startup-ready ",
            )
        }
        for boundary in EXPECTED_STARTUP_BOUNDARIES[:-1]:
            marker = (
                f"stage={boundary['value']} "
                if boundary["kind"] == "stage"
                else str(boundary["value"])
            )
            boundary_position = next(
                index for index, line in enumerate(scenario_lines) if marker in line
            )
            self.assertLess(boundary_position, positions["stage=workspace-opened "])
        self.assertLess(positions["stage=workspace-opened "], positions["stage=shell-created "])
        self.assertLess(positions["stage=shell-created "], positions["stage=startup-ready "])

    def test_required_fonts_boundary_after_shell_is_rejected(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        required = next(
            index for index, event in enumerate(events)
            if event["scenarioId"] == "empty-workspace"
            and "stage=required-fonts-loaded " in event["line"]
        )
        required_event = events.pop(required)
        shell = next(
            index for index, event in enumerate(events)
            if event["scenarioId"] == "empty-workspace"
            and "stage=shell-created " in event["line"]
        )
        events.insert(shell + 1, required_event)
        lines = [event["line"] for event in events if event["scenarioId"] == "empty-workspace"]
        self.assertLess(
            next(index for index, line in enumerate(lines) if "stage=shell-created " in line),
            next(index for index, line in enumerate(lines) if "stage=required-fonts-loaded " in line),
        )

        with self.assertRaisesRegex(ValueError, "required-fonts-loaded startup record occurred after workspace-opened"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_shell_boundary_after_startup_ready_is_rejected(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        shell = next(
            index for index, event in enumerate(events)
            if event["scenarioId"] == "empty-workspace"
            and "stage=shell-created " in event["line"]
        )
        shell_event = events.pop(shell)
        ready = next(
            index for index, event in enumerate(events)
            if event["scenarioId"] == "empty-workspace"
            and "stage=startup-ready " in event["line"]
        )
        events.insert(ready + 1, shell_event)

        with self.assertRaisesRegex(ValueError, "shell-created startup boundary.*after startup-ready"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_missing_startup_boundary_sample_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        boundary_metrics.pop(0)

        with self.assertRaisesRegex(ValueError, "startup boundary metric rows are missing"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_duplicate_startup_boundary_sample_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        boundary_metrics.insert(1, dict(boundary_metrics[0]))

        with self.assertRaisesRegex(ValueError, "startup boundary metric rows are missing"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_startup_boundary_samples_must_preserve_event_order(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        boundary_metrics[1], boundary_metrics[2] = boundary_metrics[2], boundary_metrics[1]

        with self.assertRaisesRegex(ValueError, "startup boundary metric rows are missing"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_duplicate_startup_boundary_log_record_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        original = next(
            event
            for event in events
            if event["scenarioId"] == "empty-workspace"
            and "stage=theme-loaded " in event["line"]
        )
        events.insert(events.index(original) + 1, dict(original))

        with self.assertRaisesRegex(ValueError, "duplicate theme-loaded startup boundary"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_unsuccessful_font_resource_boundary_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        event = next(
            item
            for item in events
            if item["scenarioId"] == "empty-workspace"
            and "resource:assets/fonts/Inter.ttc" in item["line"]
        )
        checkpoint = json.loads(event["line"].split("=", 1)[1])
        checkpoint["outcome"] = "failed"
        event["line"] = "ClassMngrNext resource checkpoint=" + json.dumps(
            checkpoint, separators=(",", ":")
        )
        matching_sample = next(
            item
            for item in boundary_metrics
            if item["scenarioId"] == "empty-workspace"
            and item["eventId"] == "inter-font-resource-loaded"
        )
        matching_sample["rawLine"] = event["line"]

        with self.assertRaisesRegex(ValueError, "resource startup record is invalid"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_startup_boundary_process_identity_must_match(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        boundary_metrics[0]["pid"] += 1

        with self.assertRaisesRegex(ValueError, "process identity differs"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_startup_boundary_memory_counters_must_be_positive(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        boundary_metrics[0]["privateBytes"] = 0

        with self.assertRaisesRegex(ValueError, "positive integer"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_stale_startup_boundary_snapshot_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        boundary_metrics[0]["timestampUtc"] = timestamp(
            datetime.fromisoformat(
                boundary_metrics[0]["eventObservedAtUtc"].replace("Z", "+00:00")
            )
            + timedelta(seconds=3)
        )

        with self.assertRaisesRegex(ValueError, "snapshot was stale"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_nonfinite_startup_boundary_delay_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        boundary_metrics[0]["sampleDelayMilliseconds"] = float("nan")

        with self.assertRaisesRegex(ValueError, "snapshot was stale or its delay is malformed"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_equal_to_threshold_fails_strict_budget(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents(startup_bytes=THRESHOLD_BYTES)

        summary = validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

        self.assertEqual(summary["validation"], "valid")
        self.assertEqual(summary["budgetResult"], "failed")
        self.assertTrue(any(item["point"] == "startup-ready" for item in summary["budgetViolations"]))

    def test_over_budget_five_minute_idle_fails_gate(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents(idle_bytes=THRESHOLD_BYTES + 1)

        summary = validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

        self.assertEqual(summary["validation"], "valid")
        self.assertEqual(summary["budgetResult"], "failed")
        self.assertTrue(any(item["point"] == "five-minute-idle" for item in summary["budgetViolations"]))

    def test_incomplete_idle_trace_is_invalid_and_never_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        metrics.pop()

        with self.assertRaisesRegex(ValueError, "exactly 301 metric samples"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_malformed_startup_ready_record_is_rejected(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        manifest["scenarios"][0]["startupReadyDiagnostic"]["rawLine"] = "not an app diagnostic"

        with self.assertRaisesRegex(ValueError, "not the app startup-ready diagnostic"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_missing_workspace_opened_record_is_invalid(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        events[:] = [
            event
            for event in events
            if not (
                event["scenarioId"] == "empty-workspace"
                and "stage=workspace-opened" in event["line"]
            )
        ]

        with self.assertRaisesRegex(ValueError, "exactly one workspace-opened"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_warning_workspace_opened_record_is_invalid(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        scenario_id = "representative-96-class-workspace"
        scenario = next(item for item in manifest["scenarios"] if item["id"] == scenario_id)
        diagnostic = scenario["workspaceOpenedDiagnostic"]
        warning_checkpoint = dict(diagnostic["checkpoint"])
        warning_checkpoint["outcome"] = "warning"
        warning_line = (
            "ClassMngrNext startup stage=workspace-opened outcome=warning "
            "diagnostic=stage-warning diagnostic_stage=workspace-opened checkpoint="
            + json.dumps(warning_checkpoint, separators=(",", ":"))
        )
        diagnostic["outcome"] = "warning"
        diagnostic["diagnostic"] = "stage-warning"
        diagnostic["diagnosticStage"] = "workspace-opened"
        diagnostic["checkpoint"] = warning_checkpoint
        diagnostic["rawLine"] = warning_line
        for event in events:
            if event["scenarioId"] == scenario_id and "stage=workspace-opened" in event["line"]:
                event["line"] = warning_line

        with self.assertRaisesRegex(ValueError, "did not complete without a diagnostic"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_failed_workspace_opened_record_is_invalid(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        scenario_id = "representative-96-class-workspace"
        scenario = next(item for item in manifest["scenarios"] if item["id"] == scenario_id)
        diagnostic = scenario["workspaceOpenedDiagnostic"]
        failed_checkpoint = dict(diagnostic["checkpoint"])
        failed_checkpoint["outcome"] = "failed"
        failed_line = (
            "ClassMngrNext startup stage=workspace-opened outcome=failed "
            "diagnostic=stage-failed diagnostic_stage=workspace-opened checkpoint="
            + json.dumps(failed_checkpoint, separators=(",", ":"))
        )
        diagnostic["outcome"] = "failed"
        diagnostic["diagnostic"] = "stage-failed"
        diagnostic["diagnosticStage"] = "workspace-opened"
        diagnostic["checkpoint"] = failed_checkpoint
        diagnostic["rawLine"] = failed_line
        for event in events:
            if event["scenarioId"] == scenario_id and "stage=workspace-opened" in event["line"]:
                event["line"] = failed_line

        with self.assertRaisesRegex(ValueError, "did not complete without a diagnostic"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_workspace_fixture_path_passed_to_app_is_required(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        manifest["scenarios"][0]["fixture"]["pathPassedToApp"] = None

        with self.assertRaisesRegex(ValueError, "exact .tps command-line path"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_qt_prefix_must_match_release_build_qt6_dir(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        manifest["qtInstallation"]["qt6Directory"] = r"C:\Qt\6.12.0\msvc2022_64\lib\cmake\Qt6"

        with self.assertRaisesRegex(ValueError, "Qt6_DIR and Qt installation prefix do not match"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_fixture_generator_runtime_must_match_qt_installation(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        manifest["qtInstallation"]["fixtureGenerators"][0]["verifiedSameQtInstallation"] = False

        with self.assertRaisesRegex(ValueError, "runtime does not match Qt6_DIR"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_over_budget_cli_preserves_evidence_and_classifies_failure(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents(idle_bytes=THRESHOLD_BYTES)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest_path = root / "manifest.json"
            metrics_path = root / "metrics.jsonl"
            log_path = root / "startup-log.jsonl"
            boundary_path = root / "startup-boundary-metrics.jsonl"
            package_path = root / "package-files.json"
            source_path = root / "source-files.json"
            build_path = root / "cmake-build.log"
            summary_path = root / "summary.json"
            manifest["configuration"]["packageBuild"]["logPath"] = str(build_path)
            manifest_text = json.dumps(manifest)
            metrics_text = "".join(json.dumps(row) + "\n" for row in metrics)
            boundary_text = "".join(json.dumps(row) + "\n" for row in boundary_metrics)
            manifest_path.write_text(manifest_text, encoding="utf-8")
            metrics_path.write_text(metrics_text, encoding="utf-8")
            log_path.write_text("".join(json.dumps(row) + "\n" for row in events), encoding="utf-8")
            boundary_path.write_text(boundary_text, encoding="utf-8")
            package_path.write_text(json.dumps({"files": files}), encoding="utf-8")
            source_path.write_text(json.dumps({"files": source_files}), encoding="utf-8")
            build_path.write_bytes(BUILD_LOG_BYTES)

            exit_code = main(
                [
                    "--manifest", str(manifest_path),
                    "--metrics", str(metrics_path),
                    "--startup-log", str(log_path),
                    "--startup-boundary-metrics", str(boundary_path),
                    "--package-files", str(package_path),
                    "--source-files", str(source_path),
                    "--build-log", str(build_path),
                    "--summary", str(summary_path),
                ]
            )

            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            self.assertEqual(exit_code, 2)
            self.assertEqual(summary["validation"], "valid")
            self.assertEqual(summary["budgetResult"], "failed")
            self.assertEqual(manifest_path.read_text(encoding="utf-8"), manifest_text)
            self.assertEqual(metrics_path.read_text(encoding="utf-8"), metrics_text)
            self.assertEqual(boundary_path.read_text(encoding="utf-8"), boundary_text)

    def test_incomplete_cli_is_not_evaluated_and_preserves_trace(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        metrics.pop()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest_path = root / "manifest.json"
            metrics_path = root / "metrics.jsonl"
            log_path = root / "startup-log.jsonl"
            boundary_path = root / "startup-boundary-metrics.jsonl"
            package_path = root / "package-files.json"
            source_path = root / "source-files.json"
            build_path = root / "cmake-build.log"
            summary_path = root / "summary.json"
            manifest["configuration"]["packageBuild"]["logPath"] = str(build_path)
            manifest_text = json.dumps(manifest)
            metrics_text = "".join(json.dumps(row) + "\n" for row in metrics)
            manifest_path.write_text(manifest_text, encoding="utf-8")
            metrics_path.write_text(metrics_text, encoding="utf-8")
            log_path.write_text("".join(json.dumps(row) + "\n" for row in events), encoding="utf-8")
            boundary_path.write_text("".join(json.dumps(row) + "\n" for row in boundary_metrics), encoding="utf-8")
            package_path.write_text(json.dumps({"files": files}), encoding="utf-8")
            source_path.write_text(json.dumps({"files": source_files}), encoding="utf-8")
            build_path.write_bytes(BUILD_LOG_BYTES)

            exit_code = main(
                [
                    "--manifest", str(manifest_path),
                    "--metrics", str(metrics_path),
                    "--startup-log", str(log_path),
                    "--startup-boundary-metrics", str(boundary_path),
                    "--package-files", str(package_path),
                    "--source-files", str(source_path),
                    "--build-log", str(build_path),
                    "--summary", str(summary_path),
                ]
            )

            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            self.assertEqual(exit_code, 1)
            self.assertEqual(summary["validation"], "invalid")
            self.assertEqual(summary["budgetResult"], "not-evaluated")
            self.assertEqual(metrics_path.read_text(encoding="utf-8"), metrics_text)

    def test_incomplete_boundary_cli_is_not_evaluated_and_preserves_trace(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        boundary_metrics.pop()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest_path = root / "manifest.json"
            metrics_path = root / "metrics.jsonl"
            log_path = root / "startup-log.jsonl"
            boundary_path = root / "startup-boundary-metrics.jsonl"
            package_path = root / "package-files.json"
            source_path = root / "source-files.json"
            build_path = root / "cmake-build.log"
            summary_path = root / "summary.json"
            manifest["configuration"]["packageBuild"]["logPath"] = str(build_path)
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            metrics_path.write_text("".join(json.dumps(row) + "\n" for row in metrics), encoding="utf-8")
            log_path.write_text("".join(json.dumps(row) + "\n" for row in events), encoding="utf-8")
            boundary_text = "".join(json.dumps(row) + "\n" for row in boundary_metrics)
            boundary_path.write_text(boundary_text, encoding="utf-8")
            package_path.write_text(json.dumps({"files": files}), encoding="utf-8")
            source_path.write_text(json.dumps({"files": source_files}), encoding="utf-8")
            build_path.write_bytes(BUILD_LOG_BYTES)

            exit_code = main(
                [
                    "--manifest", str(manifest_path),
                    "--metrics", str(metrics_path),
                    "--startup-log", str(log_path),
                    "--startup-boundary-metrics", str(boundary_path),
                    "--package-files", str(package_path),
                    "--source-files", str(source_path),
                    "--build-log", str(build_path),
                    "--summary", str(summary_path),
                ]
            )

            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            self.assertEqual(exit_code, 1)
            self.assertEqual(summary["validation"], "invalid")
            self.assertEqual(summary["budgetResult"], "not-evaluated")
            self.assertEqual(boundary_path.read_text(encoding="utf-8"), boundary_text)

    def test_malformed_cli_evidence_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        events[0]["line"] = "malformed"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest_path = root / "manifest.json"
            metrics_path = root / "metrics.jsonl"
            log_path = root / "startup-log.jsonl"
            boundary_path = root / "startup-boundary-metrics.jsonl"
            package_path = root / "package-files.json"
            source_path = root / "source-files.json"
            build_path = root / "cmake-build.log"
            summary_path = root / "summary.json"
            manifest["configuration"]["packageBuild"]["logPath"] = str(build_path)
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            metrics_path.write_text("".join(json.dumps(row) + "\n" for row in metrics), encoding="utf-8")
            log_path.write_text("".join(json.dumps(row) + "\n" for row in events), encoding="utf-8")
            boundary_path.write_text("".join(json.dumps(row) + "\n" for row in boundary_metrics), encoding="utf-8")
            package_path.write_text(json.dumps({"files": files}), encoding="utf-8")
            source_path.write_text(json.dumps({"files": source_files}), encoding="utf-8")
            build_path.write_bytes(BUILD_LOG_BYTES)

            exit_code = main(
                [
                    "--manifest", str(manifest_path),
                    "--metrics", str(metrics_path),
                    "--startup-log", str(log_path),
                    "--startup-boundary-metrics", str(boundary_path),
                    "--package-files", str(package_path),
                    "--source-files", str(source_path),
                    "--build-log", str(build_path),
                    "--summary", str(summary_path),
                ]
            )

            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            self.assertEqual(exit_code, 1)
            self.assertEqual(summary["validation"], "invalid")
            self.assertEqual(summary["budgetResult"], "not-evaluated")

    def test_malformed_package_inventory_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        files[0]["sha256"] = "not-a-sha"

        with self.assertRaisesRegex(ValueError, "lowercase SHA-256"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_candidate_source_inventory_must_match_source_sha(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        source_files[0]["sha256"] = "0" * 64

        with self.assertRaisesRegex(ValueError, "does not match candidate source SHA"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_candidate_source_start_end_digest_detects_gate_config_change(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        manifest["candidateSource"]["sha256AtRunEnd"] = "0" * 64
        manifest["candidateSource"]["stableDuringRun"] = False

        with self.assertRaisesRegex(ValueError, "candidate source changed during"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_candidate_source_inventory_requires_gate_and_preset_files(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        source_files = [
            entry
            for entry in source_files
            if entry["path"] != "CMakePresets.json"
            and entry["path"] != "scripts/phase9/run_packaged_idle_memory_gate.ps1"
        ]
        new_source_sha = _canonical_package_sha(source_files)
        manifest["candidateSource"]["sha256"] = new_source_sha
        manifest["candidateSource"]["sha256AtRunEnd"] = new_source_sha
        manifest["candidateSource"]["fileCount"] = len(source_files)
        manifest["application"]["candidateSourceSha256"] = new_source_sha
        manifest["package"]["candidateSourceSha256"] = new_source_sha

        with self.assertRaisesRegex(ValueError, "omits required files"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_cmake_cache_start_end_digest_detects_configuration_change(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        manifest["configuration"]["cmakeCacheSha256AtRunEnd"] = "0" * 64
        manifest["configuration"]["cmakeCacheStableDuringRun"] = False

        with self.assertRaisesRegex(ValueError, "CMakeCache.txt changed during"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_unoptimized_release_flags_are_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        manifest["configuration"]["releaseCompiler"]["cxxFlagsRelease"]["raw"] = "/DNDEBUG"

        with self.assertRaisesRegex(ValueError, "cxxFlagsRelease must include"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_missing_package_prerequisite_target_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        manifest["configuration"]["packageBuild"]["targets"].remove(
            "ClassMngrNextResourcePackage"
        )

        with self.assertRaisesRegex(ValueError, "required package and fixture build targets are missing"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_package_build_log_hash_must_match_retained_log(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()

        with self.assertRaisesRegex(ValueError, "build log hash does not match"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics, b"different log")


if __name__ == "__main__":
    unittest.main()
