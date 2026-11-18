from __future__ import annotations

import json
import hashlib
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

from validate_packaged_idle_memory_evidence import (
    EXPECTED_INTER_FONT_CATALOG_ENTRIES,
    EXPECTED_INTER_FONT_RESOURCE_IDS,
    EXPECTED_STARTUP_BOUNDARIES,
    EXPECTED_SCENARIOS,
    NAVIGATION_CYCLES,
    NAVIGATION_METRICS_SCHEMA,
    NAVIGATION_PLATEAU_CYCLES,
    NAVIGATION_PLATEAU_DIAGNOSTIC_SCHEMA,
    SUPPORTED_NAVIGATION_PLATEAU_CYCLE_COUNTS,
    NAVIGATION_ROUTE,
    NAVIGATION_SELECTION_METHODS,
    EXPECTED_NAVIGATION_TRANSITIONS,
    INTER_FONT_LICENSE_PACKAGE_PATH,
    INTER_FONT_LICENSE_SOURCE_PATH,
    P9_07_MANIFEST_SCHEMA,
    P9_07_TASK_ID,
    STARTUP_READY_METRICS_SCHEMA,
    STARTUP_BOUNDARY_MAX_DELAY_MILLISECONDS,
    STARTUP_BOUNDARY_METRICS_SCHEMA,
    THRESHOLD_BYTES,
    _canonical_package_sha,
    main,
    validate_documents,
    validate_navigation_plateau_documents,
)

BUILD_LOG_BYTES = b"Successful CMake Release prerequisite build fixture.\n"


def timestamp(value: datetime) -> str:
    return value.isoformat(timespec="milliseconds").replace("+00:00", "Z")


def valid_documents(startup_bytes: int = 90_000_000, idle_bytes: int = 95_000_000):
    run_started = datetime(2026, 11, 10, 9, 0, tzinfo=timezone.utc)
    executable = r"C:\runner-temp\phase9\package\ClassMngrNext.exe"
    image_sha = "a" * 64
    inter_font_catalog_entries = []
    inter_font_source_entries = []
    inter_font_package_entries = []
    font_hash_characters = "123456"
    for index, expected in enumerate(EXPECTED_INTER_FONT_CATALOG_ENTRIES):
        size_bytes = 110_000 + index
        font_sha = font_hash_characters[index] * 64
        entry = {**expected, "sizeBytes": size_bytes, "sha256": font_sha}
        inter_font_catalog_entries.append(entry)
        inter_font_source_entries.append(
            {"path": expected["sourcePath"], "sizeBytes": size_bytes, "sha256": font_sha}
        )
        inter_font_package_entries.append(
            {
                "path": f"resources/{expected['packageRelativePath']}",
                "sizeBytes": size_bytes,
                "sha256": font_sha,
            }
        )
    license_fixture = b"Inter Open Font License fixture\n"
    inter_license_entry = {
        "sourcePath": INTER_FONT_LICENSE_SOURCE_PATH,
        "packagePath": INTER_FONT_LICENSE_PACKAGE_PATH,
        "sizeBytes": len(license_fixture),
        "sha256": hashlib.sha256(license_fixture).hexdigest(),
    }
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
            "path": "src/next/bootstrap/application_bootstrap.cpp",
            "sizeBytes": 500_000,
            "sha256": "c" * 64,
        },
        {
            "path": "src/next/ui/application_shell.cpp",
            "sizeBytes": 120_000,
            "sha256": "d" * 64,
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
        {
            "path": INTER_FONT_LICENSE_SOURCE_PATH,
            "sizeBytes": inter_license_entry["sizeBytes"],
            "sha256": inter_license_entry["sha256"],
        },
    ]
    source_files.extend(inter_font_source_entries)
    source_files.sort(key=lambda item: item["path"].casefold())
    source_sha = _canonical_package_sha(source_files)
    package_files = [
        {"path": "ClassMngrNext.exe", "sizeBytes": 1_000_000, "sha256": image_sha},
        {"path": "Qt6Core.dll", "sizeBytes": 5_000_000, "sha256": "c" * 64},
        {"path": "resources/resource-catalog.json", "sizeBytes": 20_000, "sha256": "7" * 64},
        {
            "path": INTER_FONT_LICENSE_PACKAGE_PATH,
            "sizeBytes": inter_license_entry["sizeBytes"],
            "sha256": inter_license_entry["sha256"],
        },
    ]
    package_files.extend(inter_font_package_entries)
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
        "schema": "classmngr-phase9-packaged-idle-memory-v3",
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
        "requiredInterFontResourceIds": list(EXPECTED_INTER_FONT_RESOURCE_IDS),
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
            "resourceCatalogPath": "resources/resource-catalog.json",
            "resourceCatalogSha256": "7" * 64,
            "interFontCatalogEntries": inter_font_catalog_entries,
            "interFontLicense": inter_license_entry,
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
            if boundary["eventId"] == "inter-font-resource-loaded":
                for font_index, resource_id in enumerate(EXPECTED_INTER_FONT_RESOURCE_IDS[1:], start=1):
                    font_checkpoint = {
                        "event": "resource-load",
                        "resource_id": resource_id,
                        "outcome": "succeeded",
                        "request_scope": {"kind": "startup"},
                    }
                    startup_events.append(
                        {
                            "scenarioId": scenario_id,
                            "stream": "stderr",
                            "observedAtUtc": timestamp(
                                event_at + timedelta(milliseconds=font_index * 10)
                            ),
                            "line": "ClassMngrNext resource checkpoint="
                            + json.dumps(font_checkpoint, separators=(",", ":")),
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
        inter_font_resource_events = []
        for startup_log_index, event in enumerate(startup_events):
            if event.get("scenarioId") != scenario_id or "resource:assets/fonts/Inter" not in event["line"]:
                continue
            font_checkpoint = json.loads(event["line"].split("=", 1)[1])
            inter_font_resource_events.append(
                {
                    "resourceId": font_checkpoint["resource_id"],
                    "outcome": font_checkpoint["outcome"],
                    "requestScopeKind": font_checkpoint["request_scope"]["kind"],
                    "startupLogIndex": startup_log_index,
                    "stream": event["stream"],
                    "observedAtUtc": event["observedAtUtc"],
                    "rawLine": event["line"],
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
                "interFontResourceEvents": inter_font_resource_events,
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


def valid_navigation_documents(
    route_working_set: int = 96_000_000,
    post_navigation_working_set: int = 100_000_000,
    idle_working_set: int = 105_000_000,
):
    manifest, old_metrics, old_events, package_files, source_files, boundary_metrics = valid_documents()
    manifest["schema"] = P9_07_MANIFEST_SCHEMA
    manifest["taskId"] = P9_07_TASK_ID
    route_manifest = [
        {"fromPageId": from_page, "toPageId": to_page}
        for from_page, to_page in NAVIGATION_ROUTE
    ]
    manifest["navigation"] = {
        "schema": NAVIGATION_METRICS_SCHEMA,
        "cycles": NAVIGATION_CYCLES,
        "transitionCountPerScenario": EXPECTED_NAVIGATION_TRANSITIONS,
        "route": route_manifest,
        "selectionMethods": list(NAVIGATION_SELECTION_METHODS),
        "metricsFile": "navigation-metrics.jsonl",
        "startupReadyMetricsFile": "startup-ready-metrics.jsonl",
        "returnToNormalPageId": "my_workspace",
        "catalogBodyCountRequired": 0,
    }
    manifest["startupReadyMetrics"] = {
        "schema": STARTUP_READY_METRICS_SCHEMA,
        "inventoryFile": "startup-ready-metrics.jsonl",
        "samplesPerScenario": 1,
    }
    startup_ready_metrics = []
    navigation_metrics = []
    new_events = []
    new_metrics = []
    next_scenario_start = datetime.fromisoformat(
        manifest["scenarios"][0]["startedAtUtc"].replace("Z", "+00:00")
    )

    for scenario_index, scenario in enumerate(manifest["scenarios"]):
        scenario_id = scenario["id"]
        old_start = datetime.fromisoformat(scenario["startedAtUtc"].replace("Z", "+00:00"))
        offset = next_scenario_start - old_start
        scenario["startedAtUtc"] = timestamp(next_scenario_start)
        process = scenario["process"]
        process_created = datetime.fromisoformat(process["createdAtUtc"].replace("Z", "+00:00")) + offset
        process["createdAtUtc"] = timestamp(process_created)
        scenario["startupReadyDiagnostic"]["observedAtUtc"] = timestamp(
            datetime.fromisoformat(
                scenario["startupReadyDiagnostic"]["observedAtUtc"].replace("Z", "+00:00")
            ) + offset
        )
        ready_at = datetime.fromisoformat(
            scenario["startupReadyDiagnostic"]["observedAtUtc"].replace("Z", "+00:00")
        )
        scenario["workspaceOpenedDiagnostic"]["observedAtUtc"] = timestamp(
            datetime.fromisoformat(
                scenario["workspaceOpenedDiagnostic"]["observedAtUtc"].replace("Z", "+00:00")
            ) + offset
        )

        scenario_events = [
            {**event, "observedAtUtc": timestamp(
                datetime.fromisoformat(event["observedAtUtc"].replace("Z", "+00:00")) + offset
            )}
            for event in old_events
            if event["scenarioId"] == scenario_id
        ]
        expanded_events = []
        for event in scenario_events:
            expanded_events.append(event)
            if "ClassMngrNext startup stage=workspace-opened " in event["line"]:
                catalog_at = datetime.fromisoformat(event["observedAtUtc"].replace("Z", "+00:00")) + timedelta(milliseconds=25)
                expanded_events.append(
                    {
                        "scenarioId": scenario_id,
                        "stream": "stderr",
                        "observedAtUtc": timestamp(catalog_at),
                        "line": "ClassMngrNext document-catalog event=projected outcome=completed document_count=5 loaded_pdf_body_count=0",
                    }
                )
            if "ClassMngrNext startup stage=startup-ready " in event["line"]:
                route_event_records = []
                for route_index in range(EXPECTED_NAVIGATION_TRANSITIONS):
                    cycle = route_index // len(NAVIGATION_ROUTE) + 1
                    step = route_index % len(NAVIGATION_ROUTE) + 1
                    from_page, to_page = NAVIGATION_ROUTE[route_index % len(NAVIGATION_ROUTE)]
                    route_at = ready_at + timedelta(milliseconds=300 + route_index * 250)
                    route_line = (
                        "ClassMngrNext navigation event=page-route outcome=completed cause=selection "
                        f"from={from_page} to={to_page} catalog_document_count=5 loaded_pdf_body_count=0"
                    )
                    route_event = {
                        "scenarioId": scenario_id,
                        "stream": "stderr",
                        "observedAtUtc": timestamp(route_at),
                        "line": route_line,
                    }
                    expanded_events.append(route_event)
                    route_event_records.append((route_event, route_index, cycle, step, from_page, to_page))
        new_events.extend(expanded_events)

        original_scenario_metrics = [
            row for row in old_metrics if row["scenarioId"] == scenario_id
        ]
        startup_external = {**original_scenario_metrics[0], "timestampUtc": timestamp(ready_at + timedelta(milliseconds=100))}
        startup_external["processCreatedAtUtc"] = timestamp(process_created)
        startup_ready_metrics.append(startup_external)
        scenario["startupReadyExternalSample"] = startup_external

        scenario_route_events = [
            event for event in expanded_events
            if event["line"].startswith("ClassMngrNext navigation event=page-route ")
        ]
        for route_index, route_event in enumerate(scenario_route_events):
            cycle = route_index // len(NAVIGATION_ROUTE) + 1
            step = route_index % len(NAVIGATION_ROUTE) + 1
            from_page, to_page = NAVIGATION_ROUTE[route_index % len(NAVIGATION_ROUTE)]
            event_at = datetime.fromisoformat(route_event["observedAtUtc"].replace("Z", "+00:00"))
            sample_at = event_at + timedelta(milliseconds=25)
            item_names = {
                "schedule": "Schedule",
                "calendar": "Calendar",
                "my_classes": "My Classes",
                "my_workspace": "My Workspace",
            }
            route_row = {
                **startup_external,
                "scenarioId": scenario_id,
                "sampleIndex": route_index,
                "phase": "navigation",
                "routeIndex": route_index,
                "cycle": cycle,
                "routeStep": step,
                "fromPageId": from_page,
                "toPageId": to_page,
                "targetPageId": to_page,
                "timestampUtc": timestamp(sample_at),
                "currentWorkingSetBytes": route_working_set + route_index * 100_000,
                "privateBytes": 86_000_000 + route_index * 100_000,
                "peakWorkingSetBytes": route_working_set + route_index * 100_000 + 1_000_000,
                "eventLogIndex": None,
                "eventObservedAtUtc": route_event["observedAtUtc"],
                "sampleDelayMilliseconds": 25.0,
                "stream": "stderr",
                "rawLine": route_event["line"],
                "selectionMethod": NAVIGATION_SELECTION_METHODS[0],
                "uiaPatternName": "PointerClick",
                "selectionVerified": True,
                "uiaWindowHandle": 2099604,
                "uiaWindowProcessId": startup_external["pid"],
                "uiaWindowTitle": "ClassMngr Next",
                "uiaControlAutomationId": "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree",
                "uiaControlName": "ClassMngr Next navigation",
                "uiaItemName": item_names[to_page],
                "uiaItemRectangle": {"x": 100.0, "y": 50.0, "width": 100.0, "height": 100.0},
                "windowMessageActivation": None,
                "milestone": "return-to-normal" if to_page == "my_workspace" else None,
            }
            navigation_metrics.append(route_row)

        final_route_sample_at = datetime.fromisoformat(
            navigation_metrics[-1]["timestampUtc"].replace("Z", "+00:00")
        )
        post_navigation_at = final_route_sample_at + timedelta(milliseconds=100)
        scenario["postNavigationCheckpoint"] = {
            **startup_external,
            "sampleIndex": 0,
            "phase": "post-navigation",
            "timestampUtc": timestamp(post_navigation_at),
            "currentWorkingSetBytes": post_navigation_working_set,
            "privateBytes": 90_000_000,
            "peakWorkingSetBytes": post_navigation_working_set + 1_000_000,
        }
        for sample_index in range(301):
            sampled_at = post_navigation_at + timedelta(seconds=sample_index)
            working_set = post_navigation_working_set if sample_index == 0 else (
                idle_working_set if sample_index == 300 else post_navigation_working_set + sample_index
            )
            new_metrics.append(
                {
                    **startup_external,
                    "scenarioId": scenario_id,
                    "sampleIndex": sample_index,
                    "phase": "post-navigation" if sample_index == 0 else "idle",
                    "timestampUtc": timestamp(sampled_at),
                    "currentWorkingSetBytes": working_set,
                    "privateBytes": 90_000_000 + sample_index,
                    "peakWorkingSetBytes": max(post_navigation_working_set, idle_working_set) + 1_000_000,
                    "handles": 90,
                    "threads": 16,
                }
            )
        idle_completed = post_navigation_at + timedelta(seconds=300, milliseconds=20)
        close_requested = idle_completed + timedelta(milliseconds=20)
        exited_at = close_requested + timedelta(milliseconds=100)
        scenario["idleCompletedAtUtc"] = timestamp(idle_completed)
        scenario["closeRequestedAtUtc"] = timestamp(close_requested)
        scenario["exitedAtUtc"] = timestamp(exited_at)
        scenario["navigationTransitionCount"] = EXPECTED_NAVIGATION_TRANSITIONS
        scenario["navigationCycles"] = NAVIGATION_CYCLES
        scenario["navigationCompletedAtUtc"] = timestamp(final_route_sample_at)
        scenario["returnToNormalMilestoneCount"] = NAVIGATION_CYCLES
        scenario["returnToNormalMilestones"] = []
        next_scenario_start = exited_at + timedelta(seconds=1)

    for scenario in manifest["scenarios"]:
        scenario_id = scenario["id"]
        scenario_process_created = scenario["process"]["createdAtUtc"]
        scenario_events = [event for event in new_events if event["scenarioId"] == scenario_id]
        for row in boundary_metrics:
            if row["scenarioId"] != scenario_id:
                continue
            match = next(event for event in scenario_events if event["line"] == row["rawLine"])
            row["startupLogIndex"] = new_events.index(match)
            row["eventObservedAtUtc"] = match["observedAtUtc"]
            row["processCreatedAtUtc"] = scenario_process_created
            row["timestampUtc"] = timestamp(
                datetime.fromisoformat(match["observedAtUtc"].replace("Z", "+00:00"))
                + timedelta(milliseconds=25)
            )
        for record in scenario["interFontResourceEvents"]:
            match = next(
                event for event in scenario_events
                if event["line"] == record["rawLine"]
            )
            record["startupLogIndex"] = new_events.index(match)
            record["observedAtUtc"] = match["observedAtUtc"]
        route_rows = [row for row in navigation_metrics if row["scenarioId"] == scenario_id]
        for row in route_rows:
            match = next(
                event for event in scenario_events
                if event["line"] == row["rawLine"]
                and event["observedAtUtc"] == row["eventObservedAtUtc"]
            )
            row["eventLogIndex"] = new_events.index(match)
            if row["toPageId"] == "my_workspace":
                milestone = {
                    "cycle": row["cycle"],
                    "pageId": row["toPageId"],
                    "eventLogIndex": row["eventLogIndex"],
                    "timestampUtc": row["timestampUtc"],
                    "currentWorkingSetBytes": row["currentWorkingSetBytes"],
                }
                scenario["returnToNormalMilestones"].append(milestone)

    manifest["scenarios"][0]["normalExit"] = True
    manifest["scenarios"][0]["exitCode"] = 0
    manifest["scenarios"][1]["normalExit"] = True
    manifest["scenarios"][1]["exitCode"] = 0
    manifest["completedAtUtc"] = timestamp(next_scenario_start + timedelta(seconds=1))
    return (
        manifest,
        new_metrics,
        new_events,
        package_files,
        source_files,
        boundary_metrics,
        startup_ready_metrics,
        navigation_metrics,
    )


def valid_navigation_plateau_documents(cycles: int = NAVIGATION_PLATEAU_CYCLES):
    (
        manifest,
        _,
        events,
        package_files,
        source_files,
        _,
        startup_ready_metrics,
        _,
    ) = valid_navigation_documents()
    representative_id = EXPECTED_SCENARIOS[1]
    scenario = json.loads(json.dumps(manifest["scenarios"][1]))
    manifest["scenarios"] = [scenario]
    manifest["status"] = "completed"
    manifest["runId"] = "plateau-test-run"
    manifest["navigation"].update(
        {
            "schema": NAVIGATION_PLATEAU_DIAGNOSTIC_SCHEMA,
            "executionMode": "diagnostic-navigation-plateau",
            "diagnosticOnly": True,
            "cycles": cycles,
            "transitionCountPerScenario": cycles * len(NAVIGATION_ROUTE),
        }
    )
    scenario["outcome"] = "diagnostic-completed"
    scenario["budgetScope"] = "diagnostic-only"
    scenario["navigationCycles"] = cycles
    scenario["navigationTransitionCount"] = cycles * len(NAVIGATION_ROUTE)
    scenario["returnToNormalMilestoneCount"] = cycles
    scenario["idleSeconds"] = 0
    scenario["normalExit"] = True
    scenario["exitCode"] = 0
    scenario["returnToNormalMilestones"] = []

    startup_events = [
        event for event in events
        if event["scenarioId"] == representative_id
        and not event["line"].startswith("ClassMngrNext navigation event=page-route ")
    ]
    ready_at = datetime.fromisoformat(
        startup_ready_metrics[1]["timestampUtc"].replace("Z", "+00:00")
    )
    navigation_rows = []
    item_names = {
        "schedule": "Schedule",
        "calendar": "Calendar",
        "my_classes": "My Classes",
        "my_workspace": "My Workspace",
    }
    initial_at = ready_at + timedelta(milliseconds=100)
    startup_events.append(
        {
            "scenarioId": representative_id,
            "stream": "stderr",
            "observedAtUtc": timestamp(initial_at),
            "line": (
                "ClassMngrNext navigation event=page-route outcome=completed cause=initial "
                "from=none to=my_workspace catalog_document_count=5 loaded_pdf_body_count=0"
            ),
        }
    )
    for route_index in range(cycles * len(NAVIGATION_ROUTE)):
        cycle = route_index // len(NAVIGATION_ROUTE) + 1
        step = route_index % len(NAVIGATION_ROUTE) + 1
        from_page, to_page = NAVIGATION_ROUTE[route_index % len(NAVIGATION_ROUTE)]
        event_at = ready_at + timedelta(milliseconds=300 + route_index * 250)
        route_line = (
            "ClassMngrNext navigation event=page-route outcome=completed cause=selection "
            f"from={from_page} to={to_page} catalog_document_count=5 loaded_pdf_body_count=0"
        )
        event_index = len(startup_events)
        event = {
            "scenarioId": representative_id,
            "stream": "stderr",
            "observedAtUtc": timestamp(event_at),
            "line": route_line,
        }
        startup_events.append(event)
        row = {
            **startup_ready_metrics[1],
            "scenarioId": representative_id,
            "sampleIndex": route_index,
            "phase": "navigation",
            "routeIndex": route_index,
            "cycle": cycle,
            "routeStep": step,
            "fromPageId": from_page,
            "toPageId": to_page,
            "targetPageId": to_page,
            "timestampUtc": timestamp(event_at + timedelta(milliseconds=25)),
            "currentWorkingSetBytes": 220_000_000 + route_index * 100_000,
            "privateBytes": 135_000_000 + route_index * 200_000,
            "peakWorkingSetBytes": 230_000_000 + route_index * 100_000,
            "handles": 1200 + route_index // 4,
            "threads": 20,
            "eventLogIndex": event_index,
            "eventObservedAtUtc": timestamp(event_at),
            "sampleDelayMilliseconds": 25.0,
            "stream": "stderr",
            "rawLine": route_line,
            "selectionMethod": NAVIGATION_SELECTION_METHODS[0],
            "selectionVerified": True,
            "uiaWindowHandle": 2099604,
            "uiaWindowProcessId": startup_ready_metrics[1]["pid"],
            "uiaWindowTitle": "ClassMngr Next",
            "uiaControlAutomationId": "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree",
            "uiaControlName": "ClassMngr Next navigation",
            "uiaItemName": item_names[to_page],
            "uiaItemRectangle": {"x": 100.0, "y": 50.0, "width": 100.0, "height": 100.0},
            "windowMessageActivation": None,
            "milestone": "return-to-normal" if to_page == "my_workspace" else None,
        }
        navigation_rows.append(row)
        if to_page == "my_workspace":
            scenario["returnToNormalMilestones"].append(
                {
                    "cycle": cycle,
                    "pageId": "my_workspace",
                    "eventLogIndex": event_index,
                    "timestampUtc": row["timestampUtc"],
                    "currentWorkingSetBytes": row["currentWorkingSetBytes"],
                }
            )

    for cycle in range(1, cycles + 1):
        for state, observation, view_count in (
            ("activated", "content-ready", 1),
            ("suspended", "post-suspension", 0),
        ):
            lifecycle = {
                "captured_at": timestamp(ready_at + timedelta(seconds=cycle * 30, milliseconds=state == "suspended")),
                "event": "page-lifecycle",
                "page_key": "calendar",
                "state": state,
                "page": {
                    "calendar_runtime_metrics": {
                        "observation": observation,
                        "calendar_view_count": view_count,
                        "calendar_root_object_count": view_count,
                        "live_heavy_session_count": view_count,
                        "cache_loaded_range_count": view_count,
                        "cache_retained_range_count": view_count,
                    }
                },
            }
            startup_events.append(
                {
                    "scenarioId": representative_id,
                    "stream": "stderr",
                    "observedAtUtc": lifecycle["captured_at"],
                    "line": "ClassMngrNext page lifecycle checkpoint=" + json.dumps(lifecycle, separators=(",", ":")),
                }
            )

    final_return = navigation_rows[-1]
    post_time = datetime.fromisoformat(final_return["timestampUtc"].replace("Z", "+00:00")) + timedelta(seconds=2)
    post_suspension = {
        **startup_ready_metrics[1],
        "sampleIndex": 1,
        "phase": "diagnostic-post-suspension",
        "timestampUtc": timestamp(post_time),
        "currentWorkingSetBytes": 230_000_000,
        "privateBytes": 145_000_000,
        "peakWorkingSetBytes": 240_000_000,
        "handles": 1220,
        "threads": 20,
    }
    scenario["postSuspensionCheckpoint"] = post_suspension
    scenario["diagnosticNavigation"] = {
        "diagnosticType": "navigation-plateau",
        "cycles": cycles,
        "transitionCount": cycles * len(NAVIGATION_ROUTE),
        "returnToNormalMilestones": scenario["returnToNormalMilestones"],
        "delayedPostSuspensionSample": post_suspension,
    }
    scenario["navigationCompletedAtUtc"] = final_return["timestampUtc"]
    scenario["exitedAtUtc"] = timestamp(post_time + timedelta(seconds=1))
    scenario["closeRequestedAtUtc"] = timestamp(post_time + timedelta(milliseconds=100))
    scenario["idleCompletedAtUtc"] = None
    manifest["completedAtUtc"] = scenario["exitedAtUtc"]
    return (
        manifest,
        [post_suspension],
        startup_events,
        [startup_ready_metrics[1]],
        navigation_rows,
        package_files,
        source_files,
    )


class PackagedIdleMemoryEvidenceTests(unittest.TestCase):
    def test_valid_twenty_cycle_plateau_diagnostic_passes_without_gate_acceptance(self) -> None:
        manifest, metrics, events, startup_ready, navigation, package_files, source_files = (
            valid_navigation_plateau_documents()
        )

        summary = validate_navigation_plateau_documents(
            manifest, metrics, events, startup_ready, navigation, package_files, source_files
        )

        self.assertEqual(summary["status"], "valid-diagnostic")
        self.assertEqual(summary["validation"], "not-evaluated")
        self.assertEqual(summary["budgetResult"], "not-evaluated")
        self.assertFalse(summary["gateAcceptanceClaimed"])
        self.assertEqual(summary["cycles"], 20)
        self.assertEqual(summary["transitions"], 80)
        self.assertEqual(len(summary["returnSnapshots"]), 20)
        self.assertEqual(summary["calendarLifecycle"]["postSuspensionCount"], 20)
        self.assertEqual(summary["postSuspensionSnapshot"]["threads"], 20)

    def test_valid_thirty_cycle_plateau_diagnostic_preserves_exact_route_contract(self) -> None:
        manifest, metrics, events, startup_ready, navigation, package_files, source_files = (
            valid_navigation_plateau_documents(cycles=30)
        )

        summary = validate_navigation_plateau_documents(
            manifest, metrics, events, startup_ready, navigation, package_files, source_files
        )

        self.assertEqual(summary["status"], "valid-diagnostic")
        self.assertEqual(summary["validation"], "not-evaluated")
        self.assertEqual(summary["budgetResult"], "not-evaluated")
        self.assertFalse(summary["gateAcceptanceClaimed"])
        self.assertEqual(summary["cycles"], 30)
        self.assertEqual(summary["transitions"], 120)
        self.assertEqual(len(summary["returnSnapshots"]), 30)
        self.assertEqual(summary["calendarLifecycle"]["contentReadyCount"], 30)
        self.assertEqual(summary["calendarLifecycle"]["postSuspensionCount"], 30)

    def test_plateau_diagnostic_rejects_unsupported_cycle_count(self) -> None:
        self.assertEqual(SUPPORTED_NAVIGATION_PLATEAU_CYCLE_COUNTS, (20, 30))
        manifest, metrics, events, startup_ready, navigation, package_files, source_files = (
            valid_navigation_plateau_documents(cycles=25)
        )
        with self.assertRaisesRegex(ValueError, "cycle count is unsupported"):
            validate_navigation_plateau_documents(
                manifest, metrics, events, startup_ready, navigation, package_files, source_files
            )

    def test_plateau_diagnostic_rejects_missing_or_out_of_order_routes(self) -> None:
        manifest, metrics, events, startup_ready, navigation, package_files, source_files = (
            valid_navigation_plateau_documents()
        )
        navigation.pop()
        with self.assertRaisesRegex(ValueError, "exactly 80 transitions"):
            validate_navigation_plateau_documents(
                manifest, metrics, events, startup_ready, navigation, package_files, source_files
            )

        manifest, metrics, events, startup_ready, navigation, package_files, source_files = (
            valid_navigation_plateau_documents()
        )
        navigation[40]["routeIndex"] = 39
        with self.assertRaisesRegex(ValueError, "out of order or unverified at transition 40"):
            validate_navigation_plateau_documents(
                manifest, metrics, events, startup_ready, navigation, package_files, source_files
            )

    def test_plateau_diagnostic_rejects_process_identity_and_post_suspension_gaps(self) -> None:
        manifest, metrics, events, startup_ready, navigation, package_files, source_files = (
            valid_navigation_plateau_documents()
        )
        navigation[5]["pid"] += 1
        with self.assertRaisesRegex(ValueError, "process identity differs"):
            validate_navigation_plateau_documents(
                manifest, metrics, events, startup_ready, navigation, package_files, source_files
            )

        manifest, metrics, events, startup_ready, navigation, package_files, source_files = (
            valid_navigation_plateau_documents()
        )
        metrics[0]["timestampUtc"] = navigation[-1]["timestampUtc"]
        with self.assertRaisesRegex(ValueError, "not delayed after the final return"):
            validate_navigation_plateau_documents(
                manifest, metrics, events, startup_ready, navigation, package_files, source_files
            )

    def test_plateau_diagnostic_rejects_retained_calendar_objects_and_idle_rows(self) -> None:
        manifest, metrics, events, startup_ready, navigation, package_files, source_files = (
            valid_navigation_plateau_documents()
        )
        lifecycle_index = next(
            index for index, event in enumerate(events)
            if '"observation":"post-suspension"' in event["line"]
        )
        events[lifecycle_index]["line"] = events[lifecycle_index]["line"].replace(
            '"calendar_view_count":0', '"calendar_view_count":1', 1
        )
        with self.assertRaisesRegex(ValueError, "retained calendar_view_count"):
            validate_navigation_plateau_documents(
                manifest, metrics, events, startup_ready, navigation, package_files, source_files
            )

        manifest, metrics, events, startup_ready, navigation, package_files, source_files = (
            valid_navigation_plateau_documents()
        )
        metrics.append({**metrics[0], "phase": "idle"})
        with self.assertRaisesRegex(ValueError, "no idle trace"):
            validate_navigation_plateau_documents(
                manifest, metrics, events, startup_ready, navigation, package_files, source_files
            )

    def test_plateau_diagnostic_cli_writes_not_evaluated_summary(self) -> None:
        manifest, metrics, events, startup_ready, navigation, package_files, source_files = (
            valid_navigation_plateau_documents()
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            manifest_path = root / "manifest.json"
            metrics_path = root / "metrics.jsonl"
            startup_path = root / "startup-log.jsonl"
            boundary_path = root / "startup-boundary-metrics.jsonl"
            startup_ready_path = root / "startup-ready-metrics.jsonl"
            navigation_path = root / "navigation-metrics.jsonl"
            package_path = root / "package-files.json"
            source_path = root / "source-files.json"
            summary_path = root / "summary.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            metrics_path.write_text("\n".join(json.dumps(row) for row in metrics) + "\n", encoding="utf-8")
            startup_path.write_text("\n".join(json.dumps(row) for row in events) + "\n", encoding="utf-8")
            boundary_path.write_text("{}\n", encoding="utf-8")
            startup_ready_path.write_text("\n".join(json.dumps(row) for row in startup_ready) + "\n", encoding="utf-8")
            navigation_path.write_text("\n".join(json.dumps(row) for row in navigation) + "\n", encoding="utf-8")
            package_path.write_text(json.dumps({"files": package_files}), encoding="utf-8")
            source_path.write_text(json.dumps({"files": source_files}), encoding="utf-8")

            exit_code = main([
                "--manifest", str(manifest_path),
                "--metrics", str(metrics_path),
                "--startup-log", str(startup_path),
                "--startup-boundary-metrics", str(boundary_path),
                "--startup-ready-metrics", str(startup_ready_path),
                "--navigation-metrics", str(navigation_path),
                "--package-files", str(package_path),
                "--source-files", str(source_path),
                "--build-log", str(root / "unused-build.log"),
                "--summary", str(summary_path),
                "--navigation-plateau-diagnostic",
            ])

            self.assertEqual(exit_code, 0)
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            self.assertEqual(summary["status"], "valid-diagnostic")
            self.assertEqual(summary["budgetResult"], "not-evaluated")

    def test_valid_under_budget_evidence_passes(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()

        summary = validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

        self.assertEqual(summary["validation"], "valid")
        self.assertEqual(summary["budgetResult"], "passed")
        self.assertEqual(summary["sampleCount"], 602)
        self.assertEqual(summary["thresholdBytes"], THRESHOLD_BYTES)
        self.assertEqual(summary["scenarios"][1]["sampleCount"], 301)
        self.assertEqual(summary["startupBoundaryMetrics"]["sampleCount"], 10)
        self.assertEqual(
            [item["resourceId"] for item in summary["scenarios"][0]["interFontResourceEvents"]],
            list(EXPECTED_INTER_FONT_RESOURCE_IDS),
        )

    def test_valid_ten_cycle_navigation_catalog_and_idle_evidence_passes(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()

        summary = validate_documents(
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            None,
            startup_ready_metrics,
            navigation_metrics,
        )

        self.assertEqual(summary["validation"], "valid")
        self.assertEqual(summary["budgetResult"], "passed")
        self.assertEqual(summary["thresholdBytes"], THRESHOLD_BYTES)
        self.assertEqual(manifest["navigation"]["cycles"], 10)
        self.assertEqual(manifest["navigation"]["transitionCountPerScenario"], 40)
        self.assertEqual(summary["navigation"]["sampleCount"], 80)
        self.assertEqual(summary["navigation"]["startupReadySampleCount"], 2)
        self.assertEqual(
            summary["navigation"]["selectionMethods"],
            [NAVIGATION_SELECTION_METHODS[0]],
        )
        self.assertEqual(summary["scenarios"][0]["documentCatalogDocumentCount"], 5)
        self.assertEqual(len(summary["scenarios"][0]["navigationTransitions"]), 40)
        self.assertEqual(len(summary["scenarios"][0]["returnToNormalMilestones"]), 10)
        self.assertEqual(
            summary["scenarios"][0]["returnToNormalWorkingSetBytes"],
            [route["currentWorkingSetBytes"] for route in navigation_metrics if route["scenarioId"] == EXPECTED_SCENARIOS[0] and route["toPageId"] == "my_workspace"],
        )

    def test_ten_cycle_route_order_and_all_return_snapshots_are_explicit(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()

        summary = validate_documents(
            manifest, metrics, events, files, source_files, boundary_metrics,
            None, startup_ready_metrics, navigation_metrics,
        )

        for scenario in summary["scenarios"]:
            transitions = scenario["navigationTransitions"]
            expected = [
                (cycle, step, from_page, to_page)
                for cycle in range(1, 11)
                for step, (from_page, to_page) in enumerate(NAVIGATION_ROUTE, start=1)
            ]
            self.assertEqual(
                [(row["cycle"], row["routeStep"], row["fromPageId"], row["toPageId"])
                 for row in transitions],
                expected,
            )
            self.assertEqual(
                [row["cycle"] for row in scenario["returnToNormalMilestones"]],
                list(range(1, 11)),
            )
            self.assertEqual(
                len(scenario["returnToNormalWorkingSetBytes"]),
                10,
            )

    def test_navigation_rejects_out_of_order_route_in_cycle_ten(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        later_schedule_events = [
            event
            for event in events
            if event["scenarioId"] == EXPECTED_SCENARIOS[0]
            and "navigation event=page-route" in event["line"]
            and "cause=selection" in event["line"]
            and "to=schedule " in event["line"]
        ]
        self.assertEqual(len(later_schedule_events), 10)
        later_schedule_events[-1]["line"] = later_schedule_events[-1]["line"].replace(
            "to=schedule ", "to=calendar ", 1
        )

        with self.assertRaisesRegex(ValueError, "out of order at transition 36"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_navigation_ui_automation_name_supports_qt_without_automation_id(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        for row in navigation_metrics:
            row["uiaControlAutomationId"] = ""

        summary = validate_documents(
            manifest, metrics, events, files, source_files, boundary_metrics,
            None, startup_ready_metrics, navigation_metrics,
        )

        self.assertEqual(summary["validation"], "valid")

    def test_navigation_rejects_wrong_ui_automation_control_name(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        navigation_metrics[0]["uiaControlName"] = "Unrelated control"

        with self.assertRaisesRegex(ValueError, "not verified through the packaged sidebar selection"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_navigation_ui_window_must_belong_to_sampled_app_process(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        navigation_metrics[0]["uiaWindowProcessId"] += 1

        with self.assertRaisesRegex(ValueError, "not verified through the packaged sidebar selection"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_navigation_accepts_exact_uia_selection_or_invoke_method(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        for row in navigation_metrics:
            row["selectionMethod"] = NAVIGATION_SELECTION_METHODS[1]
            row["uiaPatternName"] = "InvokePattern"

        summary = validate_documents(
            manifest, metrics, events, files, source_files, boundary_metrics,
            None, startup_ready_metrics, navigation_metrics,
        )

        self.assertEqual(summary["validation"], "valid")
        self.assertEqual(
            summary["navigation"]["selectionMethods"],
            [NAVIGATION_SELECTION_METHODS[1]],
        )

    def test_navigation_accepts_uia_selection_item_pattern(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        for row in navigation_metrics:
            row["selectionMethod"] = NAVIGATION_SELECTION_METHODS[1]
            row["uiaPatternName"] = "SelectionItemPattern"

        summary = validate_documents(
            manifest, metrics, events, files, source_files, boundary_metrics,
            None, startup_ready_metrics, navigation_metrics,
        )

        self.assertEqual(summary["validation"], "valid")

    def test_navigation_accepts_exact_uia_window_message_click(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        sequence = [
            "WaitForInputIdle",
            "ScreenToClient",
            "GetClientRect",
            "WM_MOUSEMOVE",
            "WM_LBUTTONDOWN",
            "WM_LBUTTONUP",
        ]
        for row in navigation_metrics:
            row["selectionMethod"] = NAVIGATION_SELECTION_METHODS[2]
            row["uiaPatternName"] = "WindowMessageClick"
            row["windowMessageActivation"] = {
                "sequence": list(sequence),
                "windowHandle": row["uiaWindowHandle"],
                "expectedProcessId": row["uiaWindowProcessId"],
                "windowProcessId": row["uiaWindowProcessId"],
                "screenX": 150,
                "screenY": 100,
                "clientX": 50,
                "clientY": 50,
                "waitForInputIdleSucceeded": True,
                "mouseMoveSent": True,
                "leftButtonDownSent": True,
                "leftButtonUpSent": True,
                "messagesSent": 3,
            }

        summary = validate_documents(
            manifest, metrics, events, files, source_files, boundary_metrics,
            None, startup_ready_metrics, navigation_metrics,
        )

        self.assertEqual(summary["validation"], "valid")
        self.assertEqual(
            summary["navigation"]["selectionMethods"],
            [NAVIGATION_SELECTION_METHODS[2]],
        )

    def test_navigation_rejects_window_message_click_with_wrong_pid(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        for row in navigation_metrics:
            row["selectionMethod"] = NAVIGATION_SELECTION_METHODS[2]
            row["uiaPatternName"] = "WindowMessageClick"
            row["windowMessageActivation"] = {
                "sequence": [
                    "WaitForInputIdle",
                    "ScreenToClient",
                    "GetClientRect",
                    "WM_MOUSEMOVE",
                    "WM_LBUTTONDOWN",
                    "WM_LBUTTONUP",
                ],
                "windowHandle": row["uiaWindowHandle"],
                "expectedProcessId": row["uiaWindowProcessId"] + 1,
                "windowProcessId": row["uiaWindowProcessId"],
                "screenX": 150,
                "screenY": 100,
                "clientX": 50,
                "clientY": 50,
                "waitForInputIdleSucceeded": True,
                "mouseMoveSent": True,
                "leftButtonDownSent": True,
                "leftButtonUpSent": True,
                "messagesSent": 3,
            }

        with self.assertRaisesRegex(ValueError, "window-message navigation activation identity"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_navigation_rejects_window_message_click_outside_exact_row(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        for row in navigation_metrics:
            row["selectionMethod"] = NAVIGATION_SELECTION_METHODS[2]
            row["uiaPatternName"] = "WindowMessageClick"
            row["windowMessageActivation"] = {
                "sequence": [
                    "WaitForInputIdle",
                    "ScreenToClient",
                    "GetClientRect",
                    "WM_MOUSEMOVE",
                    "WM_LBUTTONDOWN",
                    "WM_LBUTTONUP",
                ],
                "windowHandle": row["uiaWindowHandle"],
                "expectedProcessId": row["uiaWindowProcessId"],
                "windowProcessId": row["uiaWindowProcessId"],
                "screenX": 250,
                "screenY": 100,
                "clientX": 50,
                "clientY": 50,
                "waitForInputIdleSucceeded": True,
                "mouseMoveSent": True,
                "leftButtonDownSent": True,
                "leftButtonUpSent": True,
                "messagesSent": 3,
            }

        with self.assertRaisesRegex(ValueError, "point did not fall inside the exact UIA row"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_navigation_rejects_selection_method_pattern_mismatch(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        navigation_metrics[0]["uiaPatternName"] = "InvokePattern"

        with self.assertRaisesRegex(ValueError, "not verified through the packaged sidebar selection"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_navigation_rejects_unknown_selection_method(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        navigation_metrics[0]["selectionMethod"] = "uia-unknown-activation"

        with self.assertRaisesRegex(ValueError, "not verified through the packaged sidebar selection"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_navigation_rejects_wrong_accessible_sidebar_item(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        navigation_metrics[0]["uiaItemName"] = "Calendar"

        with self.assertRaisesRegex(ValueError, "not verified through the packaged sidebar selection"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_missing_navigation_event_is_not_evaluated(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        scenario_id = EXPECTED_SCENARIOS[0]
        events[:] = [
            event for event in events
            if not (
                event["scenarioId"] == scenario_id
                and "navigation event=page-route" in event["line"]
                and "to=calendar " in event["line"]
            )
        ]

        with self.assertRaisesRegex(ValueError, "exactly 40 normal navigation transitions"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_duplicate_navigation_sample_is_not_evaluated(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        navigation_metrics[1] = dict(navigation_metrics[0])

        with self.assertRaisesRegex(ValueError, "navigation process metric rows are missing, duplicated, or out of order"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_duplicate_navigation_route_event_is_not_evaluated(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        target = next(
            event for event in events
            if event["scenarioId"] == EXPECTED_SCENARIOS[0]
            and "navigation event=page-route" in event["line"]
        )
        duplicate = dict(target)
        duplicate["observedAtUtc"] = timestamp(
            datetime.fromisoformat(target["observedAtUtc"].replace("Z", "+00:00"))
            + timedelta(milliseconds=5)
        )
        events.insert(events.index(target) + 1, duplicate)

        with self.assertRaisesRegex(ValueError, "exactly 40 normal navigation transitions"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_failed_navigation_route_is_not_evaluated(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        target = next(
            event for event in events
            if event["scenarioId"] == EXPECTED_SCENARIOS[0]
            and "navigation event=page-route" in event["line"]
        )
        target["line"] = target["line"].replace("outcome=completed", "outcome=failed", 1)

        with self.assertRaisesRegex(ValueError, "failed or is out of order"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_out_of_order_navigation_route_is_not_evaluated(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        scenario_id = EXPECTED_SCENARIOS[0]
        route_positions = [
            index for index, event in enumerate(events)
            if event["scenarioId"] == scenario_id
            and "navigation event=page-route" in event["line"]
        ]
        events[route_positions[0]], events[route_positions[1]] = (
            events[route_positions[1]], events[route_positions[0]]
        )

        with self.assertRaisesRegex(ValueError, "failed or is out of order"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_catalog_must_report_zero_pdf_bodies_at_startup(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        catalog = next(
            event for event in events
            if event["scenarioId"] == EXPECTED_SCENARIOS[0]
            and event["line"].startswith("ClassMngrNext document-catalog ")
        )
        catalog["line"] = catalog["line"].replace("loaded_pdf_body_count=0", "loaded_pdf_body_count=1")

        with self.assertRaisesRegex(ValueError, "zero loaded PDF bodies"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_navigation_rejects_pdf_body_resource_load(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents()
        events.append(
            {
                "scenarioId": EXPECTED_SCENARIOS[0],
                "stream": "stderr",
                "observedAtUtc": events[-1]["observedAtUtc"],
                "line": "ClassMngrNext resource checkpoint=" + json.dumps(
                    {
                        "event": "resource-load",
                        "resource_id": "document.body:Guides/sample.pdf",
                        "catalog_resource": {"kind": "document_body"},
                        "outcome": "succeeded",
                    },
                    separators=(",", ":"),
                ),
            }
        )

        with self.assertRaisesRegex(ValueError, "loaded or requested a PDF body"):
            validate_documents(
                manifest, metrics, events, files, source_files, boundary_metrics,
                None, startup_ready_metrics, navigation_metrics,
            )

    def test_navigation_threshold_equality_fails_strict_budget(self) -> None:
        (
            manifest,
            metrics,
            events,
            files,
            source_files,
            boundary_metrics,
            startup_ready_metrics,
            navigation_metrics,
        ) = valid_navigation_documents(route_working_set=THRESHOLD_BYTES)

        summary = validate_documents(
            manifest, metrics, events, files, source_files, boundary_metrics,
            None, startup_ready_metrics, navigation_metrics,
        )

        self.assertEqual(summary["validation"], "valid")
        self.assertEqual(summary["budgetResult"], "failed")
        self.assertTrue(
            any(
                violation["point"].startswith("navigation-cycle-")
                for violation in summary["budgetViolations"]
            )
        )

    def test_all_six_catalogued_inter_ttf_events_are_required_and_correlated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()

        summary = validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

        scenario = summary["scenarios"][0]
        self.assertEqual(
            [item["resourceId"] for item in scenario["interFontResourceEvents"]],
            list(EXPECTED_INTER_FONT_RESOURCE_IDS),
        )
        self.assertTrue(
            all(item["outcome"] == "succeeded" for item in scenario["interFontResourceEvents"])
        )

    def test_inter_font_license_is_in_package_and_matches_source(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()

        summary = validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

        license_entry = manifest["package"]["interFontLicense"]
        self.assertEqual(license_entry["sourcePath"], INTER_FONT_LICENSE_SOURCE_PATH)
        self.assertEqual(license_entry["packagePath"], INTER_FONT_LICENSE_PACKAGE_PATH)
        self.assertEqual(summary["package"]["sha256"], manifest["package"]["sha256"])

    def test_missing_inter_font_license_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        files[:] = [entry for entry in files if entry["path"] != INTER_FONT_LICENSE_PACKAGE_PATH]
        manifest["package"]["fileCount"] = len(files)
        manifest["package"]["sha256"] = _canonical_package_sha(files)

        with self.assertRaisesRegex(ValueError, "Inter OFL license is absent from source or package inventory"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_inter_font_license_package_hash_must_match_source(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        package_license = next(
            entry for entry in files if entry["path"] == INTER_FONT_LICENSE_PACKAGE_PATH
        )
        package_license["sha256"] = "0" * 64
        manifest["package"]["sha256"] = _canonical_package_sha(files)

        with self.assertRaisesRegex(ValueError, "Inter OFL license hash or size differs"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_missing_inter_ttf_event_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        target_id = EXPECTED_INTER_FONT_RESOURCE_IDS[-1]
        events[:] = [
            event
            for event in events
            if not (
                event["scenarioId"] == "empty-workspace"
                and target_id in event["line"]
            )
        ]

        with self.assertRaisesRegex(ValueError, "successful startup event for each catalogued Inter TTF"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_duplicate_inter_ttf_event_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        original = next(
            event
            for event in events
            if event["scenarioId"] == "empty-workspace"
            and EXPECTED_INTER_FONT_RESOURCE_IDS[-1] in event["line"]
        )
        events.insert(events.index(original) + 1, dict(original))

        with self.assertRaisesRegex(ValueError, "successful startup event for each catalogued Inter TTF"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_failed_inter_ttf_event_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        target_id = EXPECTED_INTER_FONT_RESOURCE_IDS[1]
        event = next(
            item
            for item in events
            if item["scenarioId"] == "empty-workspace" and target_id in item["line"]
        )
        checkpoint = json.loads(event["line"].split("=", 1)[1])
        checkpoint["outcome"] = "warning"
        event["line"] = "ClassMngrNext resource checkpoint=" + json.dumps(
            checkpoint, separators=(",", ":")
        )

        with self.assertRaisesRegex(ValueError, "did not complete successfully during startup"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_inter_ttf_events_must_be_in_registration_order(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        first_id, second_id = EXPECTED_INTER_FONT_RESOURCE_IDS[1:3]
        first = next(
            event
            for event in events
            if event["scenarioId"] == "empty-workspace" and first_id in event["line"]
        )
        second = next(
            event
            for event in events
            if event["scenarioId"] == "empty-workspace" and second_id in event["line"]
        )
        first_line, first_time = first["line"], first["observedAtUtc"]
        first["line"], first["observedAtUtc"] = second["line"], second["observedAtUtc"]
        second["line"], second["observedAtUtc"] = first_line, first_time

        with self.assertRaisesRegex(ValueError, "successful startup event for each catalogued Inter TTF"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_inter_ttc_alias_is_rejected(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        target_id = EXPECTED_INTER_FONT_RESOURCE_IDS[1]
        event = next(
            item
            for item in events
            if item["scenarioId"] == "empty-workspace" and target_id in item["line"]
        )
        checkpoint = json.loads(event["line"].split("=", 1)[1])
        checkpoint["resource_id"] = "resource:assets/fonts/Inter.ttc"
        event["line"] = "ClassMngrNext resource checkpoint=" + json.dumps(
            checkpoint, separators=(",", ":")
        )

        with self.assertRaisesRegex(ValueError, "unexpected Inter font resource ID.*TTC aliases are rejected"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

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

        with self.assertRaisesRegex(ValueError, "required-fonts-loaded record did not precede workspace-opened"):
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

        with self.assertRaisesRegex(ValueError, "Inter font manifest event differs from its startup log record"):
            validate_documents(manifest, metrics, events, files, source_files, boundary_metrics)

    def test_unsuccessful_font_resource_boundary_is_not_evaluated(self) -> None:
        manifest, metrics, events, files, source_files, boundary_metrics = valid_documents()
        event = next(
            item
            for item in events
            if item["scenarioId"] == "empty-workspace"
            and "resource:assets/fonts/Inter-Regular.ttf" in item["line"]
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

        with self.assertRaisesRegex(ValueError, "did not complete successfully during startup"):
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
