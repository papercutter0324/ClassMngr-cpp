from __future__ import annotations

import copy
import hashlib
import json
import sqlite3
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

from validate_packaged_next_sub_prep_lifecycle import (
    ACTION_CYCLES,
    EXPECTED_INTER_FONTS,
    EXPECTED_PROCESSES,
    EXPECTED_ROOTS,
    EXPECTED_SUB_PREP_SUMMARY_DAYS,
    IDLE_SAMPLE_COUNT,
    INTER_LICENSE,
    MANIFEST_SCHEMA,
    REQUIRED_BUILD_TARGETS,
    SUMMARY_SCHEMA,
    TASK_ID,
    THRESHOLD_BYTES,
    SUB_PREP_OUTPUT_FIXTURE_HELPER,
    SUB_PREP_OUTPUT_FIXTURE_TEST,
    SUB_PREP_OUTPUT_FIXTURE_TRANSFORM,
    EXPECTED_PACKAGE_CLASS_COUNT,
    EvidenceValidationError,
    _canonical_sha,
    _compare_prepared_source_inventory,
    validate_documents,
)
from sub_prep_accessible_summary import FixtureSummaryError, derive_accessible_name


def _digest(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def _timestamp(value: datetime) -> str:
    return value.isoformat(timespec="milliseconds").replace("+00:00", "Z")


def _memory(working_set: int = 120_000_000, peak: int = 140_000_000) -> dict:
    return {
        "working_set_bytes": working_set,
        "private_usage_bytes": 80_000_000,
        "private_working_set_bytes": 60_000_000,
        "peak_working_set_bytes": peak,
        "handle_count": 90,
        "thread_count": 20,
    }


def _page_metrics(kind: str) -> dict:
    values = {
        "summary_class_count": 96,
        "teacher_summary_count": 24,
        "summary_model_count": 1,
        "active_grade_row_count": 96,
        "selected_details_retained": False,
        "selected_class_id": "",
        "selected_detail_panel_count": 0,
        "selected_detail_note_editor_count": 0,
        "preparation_editor_count": 4,
        "text_editor_count": 4,
    }
    if kind in {"class-selection", "explicit-refresh"}:
        values.update(
            selected_details_retained=True,
            selected_class_id="96",
            selected_detail_panel_count=1,
            selected_detail_note_editor_count=2,
            text_editor_count=6,
        )
    elif kind in {"page-suspended", "post-page-release-snapshot"}:
        values.update(
            summary_class_count=0,
            teacher_summary_count=0,
            active_grade_row_count=0,
            selected_class_id="",
            selected_detail_note_editor_count=0,
            text_editor_count=4,
        )
    return values


def _click(pid: int, name: str, target: str, from_page: str, to_page: str,
           event_index: int, *, verified: bool = True) -> dict:
    return {
        "actionKind": "page-navigation" if to_page != from_page else "sub-prep-class-selection",
        "method": "uia-verified-pointer-click",
        "expectedAccessibleName": name,
        "observedAccessibleName": name,
        "itemId": target,
        "fromPageId": from_page,
        "toPageId": to_page,
        "uiaProcessId": pid,
        "hwndProcessId": pid,
        "hwnd": 700_000 + pid,
        "rectangle": {"x": 300, "y": 200, "width": 100, "height": 100},
        "nativeActivation": {
            "kind": "foreground-pointer",
            "foregroundVerified": True,
            "foregroundProcessId": pid,
            "screenX": 350,
            "screenY": 255,
            "inputEventsSent": 2,
        },
        "verifiedAppEvent": verified,
        "appEventLogIndex": event_index if verified else None,
    }


def valid_documents(startup_working_set: int = 120_000_000,
                    idle_working_set: int = 120_000_000) -> dict:
    run_started = datetime(2026, 11, 18, 13, 0, tzinfo=timezone.utc)
    app_path = r"C:\evidence\p908\package\ClassMngrNext.exe"
    app_hash = _digest("packaged ClassMngrNext.exe")
    fixture_hash = _digest("96-class fixture")
    prepared_fixture_hash = _digest("prepared 96-class weekday output fixture")
    font_entries = []
    source_files = []
    package_files = [
        {"path": "ClassMngrNext.exe", "sizeBytes": 2_000_000, "sha256": app_hash},
        {"path": "resources/resource-catalog.json", "sizeBytes": 50_000, "sha256": _digest("catalog")},
    ]
    for index, resource_id in enumerate(EXPECTED_INTER_FONTS):
        source_path = f"resources/assets/fonts/{resource_id.split('/')[-1]}"
        size = 120_000 + index
        digest = _digest(resource_id)
        font_entries.append({"resourceId": resource_id, "sourcePath": source_path,
                             "packageRelativePath": f"assets/fonts/{resource_id.split('/')[-1]}",
                             "sizeBytes": size, "sha256": digest})
        source_files.append({"path": source_path, "sizeBytes": size, "sha256": digest})
        package_files.append({"path": f"resources/{resource_id.removeprefix('resource:')}",
                              "sizeBytes": size, "sha256": digest})
    license_bytes = b"SPDX-License-Identifier: OFL-1.1\nInter Open Font License fixture\n"
    license_hash = hashlib.sha256(license_bytes).hexdigest()
    source_files.append({"path": INTER_LICENSE, "sizeBytes": len(license_bytes), "sha256": license_hash})
    package_files.append({"path": INTER_LICENSE, "sizeBytes": len(license_bytes), "sha256": license_hash})

    qt_prefix = r"C:\Qt\6.12.0\msvc2022_64"
    qt_paths = ["Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll",
                "platforms/qwindows.dll", "sqldrivers/qsqlite.dll"]
    runtime_files = []
    for name in qt_paths:
        digest = _digest("qt:" + name)
        size = 1_000_000 + len(name)
        if "/" in name:
            prefix_rel = "plugins/" + name
        else:
            prefix_rel = "bin/" + name
        runtime_files.append({"packagePath": name,
                              "qtPrefixPath": qt_prefix + "\\" + prefix_rel.replace("/", "\\"),
                              "sizeBytes": size, "sha256": digest})
        package_files.append({"path": name, "sizeBytes": size, "sha256": digest})
    package_files.sort(key=lambda row: row["path"].casefold())

    required_source_paths = {
        "CMakeLists.txt", "CMakePresets.json", "cmake/CMakePresets.json",
        "docs/qt-rewrite/phase-4-resource-inventory.json", "licenses/LICENSE.txt",
        "scripts/phase4/bake_resources.py", "scripts/phase9/run_packaged_idle_memory_gate.ps1",
        "scripts/phase9/run_packaged_next_sub_prep_lifecycle.ps1",
        "scripts/phase9/sub_prep_accessible_summary.py",
        SUB_PREP_OUTPUT_FIXTURE_HELPER,
        SUB_PREP_OUTPUT_FIXTURE_TEST,
        "scripts/phase9/validate_packaged_next_sub_prep_lifecycle.py",
        "scripts/phase9/test_validate_packaged_next_sub_prep_lifecycle.py",
        "src/next/bootstrap/application_bootstrap.cpp",
        "src/next/bootstrap/qt_page_lifecycle_diagnostics.cpp",
        "src/next/ui/sub_prep_page.cpp", "tests/next/page_host_tests.cpp",
        "tests/next/sub_prep_page_tests.cpp", "third_party/sqlite/sqlite3.c",
        INTER_LICENSE,
    }
    source_map = {row["path"]: row for row in source_files}
    for path in required_source_paths:
        source_map.setdefault(path, {"path": path, "sizeBytes": 100 + len(path), "sha256": _digest(path)})
    source_files = sorted(source_map.values(), key=lambda row: row["path"].casefold())
    source_file_map = {row["path"]: row for row in source_files}
    label_helper_sha = source_file_map["scripts/phase9/sub_prep_accessible_summary.py"]["sha256"]
    fixture_helper_sha = source_file_map[SUB_PREP_OUTPUT_FIXTURE_HELPER]["sha256"]
    source_sha = _canonical_sha(source_files)
    package_sha = _canonical_sha(package_files)
    cache_sha = _digest("CMakeCache.txt")
    project_sha = _digest("ClassMngrNext.vcxproj")
    build_log_sha = _digest("successful Release build log")
    qt_core_hash = next(row["sha256"] for row in runtime_files if row["packagePath"] == "Qt6Core.dll")
    windeploy_hash = _digest("windeployqt.exe")
    generator_hash = _digest("ClassMngrStartupPerformanceTests.exe")
    generator_path = r"C:\Git\ClassMngr-cpp\build\windows-x64-release\Release\ClassMngrStartupPerformanceTests.exe"
    fixture_path_root = r"C:\evidence\p908\fixtures"
    output_path_root = r"C:\evidence\p908\outputs"
    copy_rows = []
    processes = []
    generated_packages = []
    startup_log = []
    process_metrics = []
    navigation_metrics = []
    uia_actions = []

    for process_index, process_id in enumerate(EXPECTED_PROCESSES):
        pid = 71_000 + process_index
        created = run_started + timedelta(seconds=process_index * 600 + 1)
        event_time = created + timedelta(seconds=1)
        fixture_path = f"{fixture_path_root}\\{process_id}\\representative-96-class.tps"
        output_root = f"{output_path_root}\\{process_id}"
        output_directory = output_root + f"\\P908 Run {process_index + 1:02d}"
        copy_rows.append({"processInstanceId": process_id, "path": fixture_path,
                          "sourceFixtureSha256": fixture_hash,
                          "sha256BeforeLaunch": prepared_fixture_hash,
                          "preLaunchSnapshotPath": fixture_path + ".pre-launch.tps",
                          "preLaunchSnapshotSha256": prepared_fixture_hash,
                          "outputPreparation": {
                              "transformId": SUB_PREP_OUTPUT_FIXTURE_TRANSFORM,
                              "fixturePath": fixture_path,
                              "sourceSha256": fixture_hash,
                              "preparedSha256": prepared_fixture_hash,
                              "classCount": 96,
                              "teacherCount": 24,
                              "rosterCellCount": 7200,
                              "classTimeCount": 96,
                              "weekdayClassCount": 30,
                              "saturdayClassCount": 66,
                              "dayCounts": {"Monday": 6, "Tuesday": 6, "Wednesday": 6,
                                            "Thursday": 6, "Friday": 6, "Saturday": 66},
                              "selectedPackageDays": ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday"],
                              "requestedPackageClassCount": EXPECTED_PACKAGE_CLASS_COUNT,
                              "weekdayTimeConflictCount": 0,
                          }})
        identity = {"pid": pid, "createdAtUtc": _timestamp(created),
                    "canonicalExecutablePath": app_path, "architecture": "x64", "sha256": app_hash}
        process = {
            "processInstanceId": process_id,
            "fixturePathPassedToApp": fixture_path,
            "fixtureSha256PassedToApp": prepared_fixture_hash,
            "fixturePreLaunchSnapshotPath": fixture_path + ".pre-launch.tps",
            "fixturePreLaunchSnapshotSha256": prepared_fixture_hash,
            "fixtureHashAtExit": prepared_fixture_hash,
            "fixtureShapeAtExit": {"classCount": 96, "teacherCount": 24,
                                    "rosterCellCount": 7200, "integrityCheck": "ok"},
            "commandLineArguments": [fixture_path],
            "processIdentity": identity,
            "outputRoot": output_root,
            "outputDirectory": output_directory,
            "userName": f"P908 Run {process_index + 1:02d}",
            "startupDocumentCatalog": {"loadedPdfBodyCount": 0},
            "interFontResourceEvents": [{"resourceId": item} for item in EXPECTED_INTER_FONTS],
            "startupReadyWorkingSetBytes": startup_working_set,
            "navigationTransitionCount": 22,
            "classSelectionCount": ACTION_CYCLES,
            "explicitRefreshCount": ACTION_CYCLES,
            "suspensionCount": ACTION_CYCLES,
            "returnWorkingSetBytes": [],
            "returnWorkingSetMaximumBytes": 0,
            "returnWorkingSetAboveNormalThresholdCount": 0,
            "idleSampleCount": IDLE_SAMPLE_COUNT,
            "idleWorkingSetMaximumBytes": idle_working_set,
            "idlePrivateBytesMedian": 80_000_000,
            "normalUseBudgetPassed": True,
            "budgetViolations": [],
            "generation": {},
            "processExitCode": 0,
            "summaryLifecycleSnapshots": [],
        }
        processes.append(process)
        per_process_log: list[dict] = []

        def add_line(line: str, at: datetime) -> int:
            index = len(per_process_log)
            row = {"processInstanceId": process_id, "pid": pid,
                   "stream": "stderr", "observedAtUtc": _timestamp(at), "line": line}
            startup_log.append(row)
            per_process_log.append(row)
            return index

        def app_record(checkpoint: str, outcome: str, details: dict, at: datetime,
                       memory: dict | None = None, metrics_kind: str | None = None) -> dict:
            page = {"widget_count": 200,
                    "sub_prep_runtime_metrics": _page_metrics(metrics_kind or checkpoint)}
            return {
                "event": "page-action-checkpoint", "page_key": "sub_prep",
                "checkpoint": checkpoint, "outcome": outcome, "details": details,
                "captured_at": _timestamp(at), "platform": "windows", "sample_available": True,
                "memory": memory or _memory(), "page_instance_available": True, "page": page,
            }

        def add_app_record(record: dict, at: datetime) -> int:
            return add_line("ClassMngrNext page lifecycle checkpoint=" +
                            json.dumps(record, separators=(",", ":")), at)

        def external_sample(phase: str, sample_index: int, at: datetime,
                            app_checkpoint: dict | None = None) -> dict:
            sample = {
                "processInstanceId": process_id, "phase": phase, "sampleIndex": sample_index,
                "timestampUtc": _timestamp(at), "pid": pid, "processCreatedAtUtc": _timestamp(created),
                "canonicalExecutablePath": app_path, "architecture": "x64", "sha256": app_hash,
                "currentWorkingSetBytes": 120_000_000, "privateBytes": 80_000_000,
                "peakWorkingSetBytes": 710_000_000, "handles": 90, "threads": 20,
            }
            if app_checkpoint is not None:
                sample["appCheckpoint"] = app_checkpoint
            return sample

        def add_route(from_page: str, to_page: str, transition: int, at: datetime) -> dict:
            target_name = "Sub Prep" if to_page == "sub_prep" else "My Workspace"
            line = (f"ClassMngrNext navigation event=page-route outcome=completed cause=selection "
                    f"from={from_page} to={to_page} catalog_document_count=12 loaded_pdf_body_count=0")
            event_index = add_line(line, at)
            click = _click(pid, target_name, to_page, from_page, to_page, event_index)
            uia_actions.append({"processInstanceId": process_id, **click})
            sample_at = at + timedelta(milliseconds=50)
            nav = {
                "processInstanceId": process_id, "transitionIndex": transition,
                "actionStartLogIndex": max(0, event_index - 1), "fromPageId": from_page,
                "toPageId": to_page, "targetAccessibleName": target_name,
                "selectionMethod": click["method"], "uiaAction": copy.deepcopy(click),
                "appEventLogIndex": event_index, "appEventObservedAtUtc": _timestamp(at),
                "sampleDelayMilliseconds": 50.0, "processSample": external_sample("navigation", transition, sample_at),
                "appEventLine": line, "observedAtUtc": _timestamp(sample_at),
            }
            navigation_metrics.append(nav)
            return nav

        # Package fixture open, startup catalog, required fonts and startup-ready.
        workspace_at = event_time
        workspace_metrics = {
            "working_set_bytes": startup_working_set, "private_usage_bytes": 80_000_000,
            "private_working_set_bytes": 60_000_000, "peak_working_set_bytes": 710_000_000,
            "handle_count": 90, "thread_count": 20,
        }
        workspace_checkpoint = {"stage": "workspace-opened", "outcome": "completed",
                                "diagnostic": "none", "diagnostic_stage": "none",
                                "sample_available": True, "platform": "windows", "metrics": workspace_metrics}
        workspace_line = ("ClassMngrNext startup stage=workspace-opened outcome=completed "
                          "diagnostic=none diagnostic_stage=none checkpoint=" +
                          json.dumps(workspace_checkpoint, separators=(",", ":")))
        add_line(workspace_line, workspace_at)
        add_line("ClassMngrNext document-catalog event=projected outcome=completed document_count=12 loaded_pdf_body_count=0",
                 workspace_at + timedelta(milliseconds=20))
        font_events = []
        ready_at = workspace_at + timedelta(milliseconds=400)
        for index, resource_id in enumerate(EXPECTED_INTER_FONTS):
            font = {"resource_id": resource_id, "event": "resource-load", "outcome": "succeeded",
                    "request_scope": {"kind": "startup"}}
            observed = workspace_at + timedelta(milliseconds=40 + 40 * index)
            line = "ClassMngrNext resource checkpoint=" + json.dumps(font, separators=(",", ":"))
            font_index = add_line(line, observed)
            font_events.append({"resourceId": resource_id, "logIndex": font_index,
                                "observedAtUtc": _timestamp(observed), "rawLine": line})
        ready_checkpoint = {"stage": "startup-ready", "outcome": "completed", "sample_available": True,
                            "platform": "windows", "metrics": {**workspace_metrics,
                            "working_set_bytes": startup_working_set}}
        ready_line = ("ClassMngrNext startup stage=startup-ready outcome=completed diagnostic=none "
                      "diagnostic_stage=none checkpoint=" + json.dumps(ready_checkpoint, separators=(",", ":")))
        add_line(ready_line, ready_at)
        process["startupReady"] = {"observedAtUtc": _timestamp(ready_at), "rawLine": ready_line,
                                   "checkpoint": ready_checkpoint}
        process["workspaceOpened"] = {"observedAtUtc": _timestamp(workspace_at),
                                      "rawLine": workspace_line, "checkpoint": workspace_checkpoint}
        process["startupDocumentCatalog"] = {"loadedPdfBodyCount": 0, "documentCount": 12}
        process["interFontResourceEvents"] = font_events
        initial_route = ("ClassMngrNext navigation event=page-route outcome=completed cause=initial "
                         "from=none to=my_workspace catalog_document_count=12 loaded_pdf_body_count=0")
        add_line(initial_route, ready_at + timedelta(milliseconds=50))
        workspace_lifecycle = {
            "event": "page-lifecycle", "page_key": "my_workspace", "state": "activated",
            "captured_at": _timestamp(ready_at + timedelta(milliseconds=60)), "platform": "windows",
            "sample_available": True, "memory": _memory(startup_working_set, 710_000_000),
            "page_instance_available": True, "page": {"widget_count": 50},
        }
        baseline_log_index = add_app_record(workspace_lifecycle, ready_at + timedelta(milliseconds=60))
        baseline = external_sample("workspace-baseline", 0, ready_at + timedelta(milliseconds=100))
        baseline["sampleDelayMilliseconds"] = 40.0
        baseline["appLifecycleSnapshot"] = workspace_lifecycle
        process["baselineWorkspace"] = {"processSample": baseline,
                                         "appLifecycleSnapshot": workspace_lifecycle,
                                         "startupLogIndex": baseline_log_index}

        transition_index = 0
        selection_count = 0
        refresh_count = 0
        suspension_count = 0
        return_values = []
        release_sequence = 0
        now = ready_at + timedelta(milliseconds=200)
        generated_documents = []
        generation_record = None
        for cycle in range(ACTION_CYCLES):
            in_time = now + timedelta(milliseconds=100)
            summary_time = in_time + timedelta(milliseconds=30)
            summary_record = app_record("summary-data-ready", "completed", {}, summary_time,
                                        metrics_kind="summary-data-ready")
            summary_record["page_instance_available"] = False
            summary_record["page"] = None
            summary_index = add_app_record(summary_record, summary_time)
            activation_time = in_time + timedelta(milliseconds=50)
            activation_metrics = _page_metrics("summary-data-ready")
            if cycle == 0:
                activation_metrics.update(summary_class_count=30, active_grade_row_count=30)
            activation_record = {
                "event": "page-lifecycle", "page_key": "sub_prep", "state": "activated",
                "captured_at": _timestamp(activation_time), "platform": "windows",
                "sample_available": True, "memory": _memory(), "page_instance_available": True,
                "page": {"widget_count": 200, "sub_prep_runtime_metrics": activation_metrics},
            }
            activation_index = add_app_record(activation_record, activation_time)
            entry_route = add_route("my_workspace", "sub_prep", transition_index,
                                    in_time + timedelta(milliseconds=70))
            transition_index += 1
            processes[-1]["summaryLifecycleSnapshots"].append({
                "summaryActionLogIndex": summary_index,
                "activationSnapshotLogIndex": activation_index,
                "pageLifecycleSnapshot": copy.deepcopy(activation_record),
            })

            class_time = in_time + timedelta(milliseconds=80)
            class_record = app_record("class-selection", "details-ready", {}, class_time,
                                      metrics_kind="class-selection")
            class_index = add_app_record(class_record, class_time)
            if cycle == 0:
                uia_actions.append({
                    "processInstanceId": process_id,
                    "actionKind": "sub-prep-summary-day-filter",
                    "method": "uia-toggle-pattern",
                    "automationId": "ClassMngrNext.subPrepDay_5",
                    "accessibleName": "Saturday",
                    "uiaProcessId": pid,
                    "mainWindowHandle": 700_000 + pid,
                    "hwndProcessId": pid,
                    "controlType": "ControlType.CheckBox",
                    "isOffscreen": False,
                    "isEnabled": True,
                    "toggleStateBefore": "Off",
                    "toggleStateAfter": "On",
                    "actionStartLogIndex": entry_route["appEventLogIndex"] + 1,
                    "issuedAtUtc": _timestamp(class_time - timedelta(milliseconds=10)),
                    "verifiedAppEvent": True,
                    "verifiedByAppEventLogIndex": class_index,
                    "verifiedByAppEvent": class_record,
                    "summaryClassCountAfter": 96,
                })
            class_name = "Level 01 \u2022 Sat 4:00 \u2022 Teacher 24 #96"
            class_action = _click(pid, class_name, "96", "sub_prep", "sub_prep", class_index)
            class_action["actionKind"] = "sub-prep-class-selection"
            target_rectangle = {"x": 120.0, "y": 240.0, "width": 480.0, "height": 30.0}
            visible_row = {
                "accessibleName": class_name, "helpText": "",
                "automationId": "ClassMngrNext.subPrepClassSummaryList",
                "controlType": "ControlType.ListItem", "processId": pid,
                "isOffscreen": False, "rectangle": target_rectangle,
            }
            class_target = {
                "method": "fixture-derived-exact-accessible-name", "classId": "96", "grade": "M2",
                "scrollMethod": "uia-scrollpattern",
                "accessibleName": class_name, "fixturePath": fixture_path,
                "fixtureSha256": prepared_fixture_hash,
                "helperPath": r"C:\Git\ClassMngr-cpp\scripts\phase9\sub_prep_accessible_summary.py",
                "helperSha256": label_helper_sha,
                "selectedDays": list(EXPECTED_SUB_PREP_SUMMARY_DAYS),
                "visibleClassCount": 24, "baseLabelDuplicateCount": 9,
                "listAutomationId": "ClassMngrNext.subPrepClassSummaryList", "listProcessId": pid,
                "listControlType": "ControlType.List",
                "mainWindowHandle": class_action["hwnd"], "mainWindowProcessId": pid,
                "listRectangle": {"x": 100.0, "y": 100.0, "width": 400.0, "height": 300.0},
                "rowAutomationId": "ClassMngrNext.subPrepClassSummaryList", "rowProcessId": pid,
                "rowControlType": "ControlType.ListItem", "rowRectangle": target_rectangle,
                "exactVisibleMatchCount": 1, "scrollSteps": 0,
                "scrollTrace": [{"step": 0, "scrollMethod": "uia-scrollpattern",
                                 "verticalScrollPercent": 0.0, "visibleRows": [visible_row]}],
            }
            class_action["fixtureDerivedTarget"] = class_target
            uia_actions.append({"processInstanceId": process_id, **copy.deepcopy(class_action)})
            uia_actions.append({"processInstanceId": process_id, "actionKind": "class-row-resolution",
                                "outcome": "target-found", "actionIndex": cycle,
                                "resolution": copy.deepcopy(class_target)})
            selection_sample = external_sample("class-selection", transition_index, class_time + timedelta(milliseconds=50))
            selection_sample["sampleDelayMilliseconds"] = 50.0
            navigation_metrics.append({
                "processInstanceId": process_id, "transitionIndex": None, "actionKind": "class-selection",
                "selectedClassId": "96", "selectedAccessibleName": class_name,
                "uiaAction": copy.deepcopy(class_action), "appEventLogIndex": class_index,
                "appEventObservedAtUtc": _timestamp(class_time), "appEvent": class_record,
                "sample": selection_sample,
            })
            selection_count += 1

            refresh_time = class_time + timedelta(milliseconds=100)
            refresh_record = app_record("explicit-refresh", "completed", {}, refresh_time,
                                        metrics_kind="explicit-refresh")
            refresh_index = add_app_record(refresh_record, refresh_time)
            refresh_sample = external_sample("explicit-refresh", refresh_index,
                                             refresh_time + timedelta(milliseconds=50), refresh_record)
            refresh_sample["sampleDelayMilliseconds"] = 50.0
            process_metrics.append(refresh_sample)
            refresh_count += 1

            if cycle == 0:
                output_path = output_directory + r"\Sub Prep.pdf"
                generated_documents = [{
                    "processInstanceId": process_id, "path": output_path,
                    "relativePath": "Sub Prep.pdf", "sizeBytes": 25_000,
                    "sha256": _digest("generated PDF " + process_id), "format": "PDF",
                }]
                gen_start_time = refresh_time + timedelta(milliseconds=100)
                gen_start = app_record("package-generation-started", "started", {
                    "requestedClassCount": EXPECTED_PACKAGE_CLASS_COUNT, "targetRoot": output_root,
                    "createFolder": True, "openFolderAfterGeneration": False,
                }, gen_start_time, metrics_kind="package-generation-started")
                gen_start_index = add_app_record(gen_start, gen_start_time)
                # UIA drove the production page's actual Generate and Accept controls.
                generation_dialog_click = _click(
                    pid, "Generate", "subPrepGenerateButton", "sub_prep", "sub_prep",
                    gen_start_index)
                generation_dialog_click.update({
                    "processInstanceId": process_id,
                    "actionKind": "open-generation-dialog",
                    "automationId": "subPrepGenerateButton",
                    "dialogProcessId": pid,
                    "dialogHandle": 800_000 + pid,
                    "dialogObservedAtUtc": _timestamp(gen_start_time),
                })
                uia_actions.append(generation_dialog_click)
                uia_actions.append({"processInstanceId": process_id, "actionKind": "package-output-control",
                                    "automationId": "subPrepOutputFolder", "value": output_root})
                uia_actions.append({"processInstanceId": process_id, "actionKind": "package-output-control",
                                    "automationId": "subPrepOutputUserName", "value": process["userName"]})
                for automation_id, state in (
                    ("subPrepCreateFolderCheckBox", "On"), ("subPrepPrintPaperCopies", "Off"),
                    ("subPrepOpenFolderAfterGeneration", "Off"), ("subPrepNextVacationCheckBox", "Off"),
                ):
                    uia_actions.append({"processInstanceId": process_id, "actionKind": "package-output-control",
                                        "automationId": automation_id, "value": state})
                generation_submit_click = _click(
                    pid, "Generate package", "subPrepOutputAccept", "sub_prep", "sub_prep",
                    gen_start_index)
                generation_submit_click.update({
                    "processInstanceId": process_id,
                    "actionKind": "package-generation-submit",
                    "automationId": "subPrepOutputAccept",
                    "hwnd": 800_000 + pid,
                    "hwndProcessId": pid,
                    "dialogHandle": 800_000 + pid,
                    "appEventLogIndex": gen_start_index,
                    "appEvent": gen_start,
                })
                generation_submit_click["nativeActivation"]["foregroundWindowHandle"] = 800_000 + pid
                uia_actions.append(generation_submit_click)
                gen_complete_time = gen_start_time + timedelta(milliseconds=100)
                gen_memory = _memory(600_000_000, 710_000_000)
                gen_done = app_record("package-generation", "completed", {
                    "outputDirectory": output_directory, "documentPaths": [output_path],
                    "folderCreated": True, "selectedClassCount": EXPECTED_PACKAGE_CLASS_COUNT, "documentCount": 1,
                }, gen_complete_time, memory=gen_memory, metrics_kind="package-generation")
                gen_done_index = add_app_record(gen_done, gen_complete_time)
                gen_sample = external_sample("generation-complete", gen_done_index,
                                             gen_complete_time + timedelta(milliseconds=50), gen_done)
                gen_sample["currentWorkingSetBytes"] = 600_000_000
                gen_sample["sampleDelayMilliseconds"] = 50.0
                process_metrics.append(gen_sample)
                post_events = {}
                for delay_name, delta in (("immediate", 0.1), ("1s", 1.0), ("5s", 5.0)):
                    post_time = gen_complete_time + timedelta(seconds=delta)
                    post_memory = _memory(600_000_000 - int(delta * 10_000_000), 710_000_000)
                    post_record = app_record("post-generation-snapshot", "completed", {
                        "delay": delay_name, "outputDirectory": output_directory, "documentCount": 1,
                    }, post_time, memory=post_memory, metrics_kind="post-generation-snapshot")
                    post_index = add_app_record(post_record, post_time)
                    post_sample = external_sample("post-generation-" + delay_name, post_index,
                                                  post_time + timedelta(milliseconds=50), post_record)
                    post_sample["currentWorkingSetBytes"] = post_memory["working_set_bytes"]
                    post_sample["sampleDelayMilliseconds"] = 50.0
                    process_metrics.append(post_sample)
                    post_events[delay_name] = {"record": post_record, "logIndex": post_index,
                                               "observedAtUtc": _timestamp(post_time)}
                generation_record = {
                    "started": gen_start, "completed": gen_done,
                    "processPeakWorkingSetBytesAtCompletion": 710_000_000,
                    "exceedsAdvisoryCeiling": True,
                    "outputDirectory": output_directory,
                    "documentCount": 1,
                    "postGenerationSnapshots": {key: post_events[key] for key in post_events},
                }
                suspension_time = gen_complete_time + timedelta(seconds=5, milliseconds=100)
            else:
                suspension_time = refresh_time + timedelta(milliseconds=100)

            release_sequence += 1
            suspended_record = app_record("page-suspended", "completed", {
                "release_sequence": release_sequence,
            }, suspension_time, metrics_kind="page-suspended")
            suspended_index = add_app_record(suspended_record, suspension_time)
            lifecycle = {
                "event": "page-lifecycle", "page_key": "sub_prep", "state": "suspended",
                "captured_at": _timestamp(suspension_time + timedelta(milliseconds=10)),
                "platform": "windows", "sample_available": True, "memory": _memory(),
                "page_instance_available": True,
                "page": {"widget_count": 200, "sub_prep_runtime_metrics": _page_metrics("page-suspended")},
            }
            add_app_record(lifecycle, suspension_time + timedelta(milliseconds=10))
            suspend_sample = external_sample("page-suspended", suspended_index,
                                             suspension_time + timedelta(milliseconds=50), suspended_record)
            suspend_sample["sampleDelayMilliseconds"] = 50.0
            process_metrics.append(suspend_sample)
            route_out_time = suspension_time + timedelta(milliseconds=100)
            route_out = add_route("sub_prep", "my_workspace", transition_index, route_out_time)
            transition_index += 1
            return_values.append(route_out["processSample"]["currentWorkingSetBytes"])

            for delay_name, delta in (("1s", 1.0), ("5s", 5.0)):
                release_time = suspension_time + timedelta(seconds=delta)
                release_record = app_record("post-page-release-snapshot", "completed", {
                    "release_sequence": release_sequence, "delay": delay_name,
                }, release_time, metrics_kind="post-page-release-snapshot")
                release_index = add_app_record(release_record, release_time)
                release_sample = external_sample("page-release-" + delay_name, release_index,
                                                 release_time + timedelta(milliseconds=50), release_record)
                release_sample["sampleDelayMilliseconds"] = 50.0
                process_metrics.append(release_sample)
            suspension_count += 1
            now = suspension_time + timedelta(seconds=5, milliseconds=100)

        process["returnWorkingSetBytes"] = return_values
        process["returnWorkingSetMaximumBytes"] = max(return_values)
        process["returnWorkingSetAboveNormalThresholdCount"] = sum(value >= THRESHOLD_BYTES for value in return_values)
        process["navigationTransitionCount"] = transition_index
        process["classSelectionCount"] = selection_count
        process["explicitRefreshCount"] = refresh_count
        process["suspensionCount"] = suspension_count
        process["generation"] = generation_record
        process["idleStartedAtUtc"] = _timestamp(now)
        idle_rows = []
        for index in range(IDLE_SAMPLE_COUNT):
            idle_at = now + timedelta(seconds=index)
            sample = external_sample("idle", index, idle_at)
            sample["currentWorkingSetBytes"] = idle_working_set
            sample["privateBytes"] = 80_000_000
            sample["peakWorkingSetBytes"] = 710_000_000
            idle_rows.append(sample)
            process_metrics.append(sample)
        process["idleSamples"] = idle_rows
        process["idleCompletedAtUtc"] = _timestamp(now + timedelta(seconds=IDLE_SAMPLE_COUNT - 1))
        process["idleWorkingSetMaximumBytes"] = max(row["currentWorkingSetBytes"] for row in idle_rows)
        process["idlePrivateBytesMedian"] = 80_000_000
        process["normalUseBudgetPassed"] = startup_working_set < THRESHOLD_BYTES and idle_working_set < THRESHOLD_BYTES
        violations = []
        if startup_working_set >= THRESHOLD_BYTES:
            violations.append({"phase": "startup-ready", "sampleIndex": 0,
                               "workingSetBytes": startup_working_set, "thresholdBytes": THRESHOLD_BYTES,
                               "metricSource": "synchronous-app-checkpoint"})
        for index, sample in enumerate(idle_rows):
            if sample["currentWorkingSetBytes"] >= THRESHOLD_BYTES:
                violations.append({"phase": "idle", "sampleIndex": index,
                                   "workingSetBytes": sample["currentWorkingSetBytes"], "thresholdBytes": THRESHOLD_BYTES,
                                   "metricSource": "external-process-snapshot"})
        process["budgetViolations"] = violations
        process["startupReadyWorkingSetBytes"] = startup_working_set
        process["processExitCode"] = 0
        process["completedAtUtc"] = _timestamp(now + timedelta(seconds=300, milliseconds=100))
        process["startupDocumentCatalog"] = {"documentCount": 12, "loadedPdfBodyCount": 0}
        process["interFontResourceEvents"] = font_events
        process["startupReady"] = {"logIndex": 8, "observedAtUtc": _timestamp(ready_at),
                                    "checkpoint": ready_checkpoint, "rawLine": ready_line}
        process["workspaceOpened"] = {"logIndex": 0, "observedAtUtc": _timestamp(workspace_at),
                                       "checkpoint": workspace_checkpoint, "rawLine": workspace_line}
        process["outputDirectory"] = output_directory
        generated_packages.append({
            "processInstanceId": process_id, "outputRoot": output_root,
            "outputDirectory": output_directory, "documentCount": 1,
            "documents": generated_documents,
        })

    source_inventory = {"files": source_files}
    package_inventory = {"files": package_files}
    manifest = {
        "schema": MANIFEST_SCHEMA, "taskId": TASK_ID, "status": "completed",
        "revision": "a" * 40, "repositoryRoot": r"C:\Git\ClassMngr-cpp",
        "candidateSource": {"includedRoots": list(EXPECTED_ROOTS), "stableDuringRun": True,
                            "sha256": source_sha, "sha256AtRunEnd": source_sha},
        "thresholds": {"normalUseWorkingSetBytesExclusive": THRESHOLD_BYTES,
                       "generationPeakWorkingSetBytesAdvisory": 536_870_912,
                       "generationPeakMetric": "process-lifetime peak working set at package-generation completion",
                       "idleSeconds": 300, "idleSampleCount": 301,
                       "sampleIntervalSeconds": 1, "strictAtOrAboveFails": True},
        "repeatedSubPrepCycles": 10,
        "configuration": {
            "cmakeCacheSha256": _digest("cache"), "cmakeCacheSha256AtRunEnd": _digest("cache"),
            "cmakeCacheStableDuringRun": True,
            "releaseCompiler": {"compiler": "MSVC", "configuration": "Release", "platform": "x64",
                                "generator": "Visual Studio 18 2026",
                                "cFlagsRelease": {"raw": "/O2 /Ob2 /DNDEBUG"},
                                "cxxFlagsRelease": {"raw": "/O2 /Ob2 /DNDEBUG"},
                                "projectOptimizationSetting": "MaxSpeed"},
            "packageBuild": {"succeeded": True, "exitCode": 0,
                             "targets": list(REQUIRED_BUILD_TARGETS)},
        },
        "qtInstallation": {"version": "6.12.0", "prefix": qt_prefix,
                           "coreDllSha256": qt_core_hash, "windeployqtSha256": windeploy_hash,
                           "qt6Directory": qt_prefix + r"\lib\cmake\Qt6",
                           "runtimeFiles": runtime_files,
                           "fixtureGenerators": [{"id": "representative-96-class-fixture-generator",
                               "executablePath": generator_path, "executableSha256": generator_hash,
                               "runtimeSearchPathPrefix": qt_prefix + r"\bin",
                               "colocatedQtDlls": [], "verifiedSameQtInstallation": True}]},
        "package": {"candidateSourceSha256": source_sha, "sha256": package_sha,
                    "fileCount": len(package_files), "applicationSha256": app_hash,
                    "interFontLicense": {"sourcePath": INTER_LICENSE, "packagePath": INTER_LICENSE,
                                         "sizeBytes": len(license_bytes), "sha256": license_hash},
                    "interFontCatalogEntries": font_entries},
        "application": {"canonicalPath": app_path, "sha256": app_hash, "architecture": "x64",
                        "candidateSourceSha256": source_sha},
        "fixture": {"id": "synthetic-large-startup-96-classes", "classCount": 96,
                    "teacherCount": 24, "rosterCellCount": 7200, "sha256": fixture_hash,
                    "generatorPath": generator_path, "generatorSha256": generator_hash,
                    "outputPreparation": {
                        "transformId": SUB_PREP_OUTPUT_FIXTURE_TRANSFORM,
                        "helperPath": str(Path(r"C:\Git\ClassMngr-cpp") / SUB_PREP_OUTPUT_FIXTURE_HELPER),
                        "helperSha256": fixture_helper_sha,
                        "selectedPackageDays": ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday"],
                        "weekdayClassCount": 30, "saturdayClassCount": 66,
                        "requestedPackageClassCount": EXPECTED_PACKAGE_CLASS_COUNT,
                        "conflictsInSelectedDays": 0,
                    },
                    "perProcessCopies": copy_rows},
        "processRuns": processes,
    }
    generated = {"packages": generated_packages}
    return {
        "manifest": manifest, "sourceInventory": source_inventory,
        "packageInventory": package_inventory, "startupLog": startup_log,
        "processMetrics": process_metrics, "navigationMetrics": navigation_metrics,
        "uiaActions": uia_actions, "generated": generated,
    }


def validate(docs: dict) -> dict:
    return validate_documents(
        docs["manifest"], docs["sourceInventory"], docs["packageInventory"],
        docs["startupLog"], docs["processMetrics"], docs["navigationMetrics"],
        docs["uiaActions"], docs["generated"],
    )


class FixtureAccessibleNameTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.fixture_path = Path(self.temporary.name) / "fixture.tps"
        connection = sqlite3.connect(self.fixture_path)
        connection.executescript(
            """
            create table classes(id integer primary key, name text);
            create table class_info(class_id integer primary key, teacher_id integer,
                class_grade text, class_level text);
            create table teachers(id integer primary key, teacher_kr text, teacher_en text);
            create table class_times(id integer primary key autoincrement, class_id integer,
                day text, start_time text);
            create table testing_classes(class_id integer primary key);
            """
        )
        connection.executemany("insert into teachers values(?,?,?)", [
            (16, "Teacher 16 KR", "Teacher 16"),
            (24, "Teacher 24 KR", "Teacher 24"),
        ])
        connection.executemany("insert into classes values(?,?)", [
            (16, "Class 016"), (96, "Class 096"), (120, "Other grade"),
        ])
        connection.executemany("insert into class_info values(?,?,?,?)", [
            (16, 16, "M2", "Level 01"),
            (96, 24, "M2", "Level 01"),
            (120, 24, "M1", "Level 01"),
        ])
        meetings = [
            ("Monday", "4:00 PM"), ("Tuesday", "5:00 PM"),
            ("Wednesday", "6:00 PM"), ("Thursday", "3:00 PM"),
            ("Friday", "4:00 PM"), ("Monday", "5:00 PM"),
            ("Tuesday", "6:00 PM"), ("Wednesday", "3:00 PM"),
        ]
        for class_id in (16, 96):
            connection.executemany(
                "insert into class_times(class_id,day,start_time) values(?,?,?)",
                [(class_id, day, start) for day, start in meetings],
            )
        connection.commit()
        connection.close()

    def test_accessible_name_matches_schedule_and_teacher_projection(self) -> None:
        result = derive_accessible_name(self.fixture_path, 96)
        self.assertEqual(result["grade"], "M2")
        self.assertEqual(result["visibleClassCount"], 2)
        self.assertEqual(result["baseLabelDuplicateCount"], 2)
        self.assertEqual(
            result["accessibleName"],
            "Level 01 \u2022 M/F 4:00; M/T 5:00; T/W 6:00; W/Th 3:00 \u2022 Teacher 24",
        )

    def test_accessible_name_can_cover_the_full_six_day_sub_prep_summary(self) -> None:
        result = derive_accessible_name(
            self.fixture_path, 96, selected_days=EXPECTED_SUB_PREP_SUMMARY_DAYS
        )
        self.assertEqual(result["selectedDays"], list(EXPECTED_SUB_PREP_SUMMARY_DAYS))
        self.assertEqual(result["visibleClassCount"], 2)
        self.assertEqual(result["classId"], "96")

    def test_duplicate_teacher_and_schedule_labels_include_class_id(self) -> None:
        connection = sqlite3.connect(self.fixture_path)
        connection.execute("update class_info set teacher_id=24 where class_id=16")
        connection.commit()
        connection.close()
        result = derive_accessible_name(self.fixture_path, 96)
        self.assertTrue(result["accessibleName"].endswith("Teacher 24 #96"))

    def test_target_must_be_visible_in_the_requested_grade_and_weekdays(self) -> None:
        with self.assertRaisesRegex(FixtureSummaryError, "not exactly once"):
            derive_accessible_name(self.fixture_path, 96, "M1")
        with self.assertRaisesRegex(FixtureSummaryError, "not exactly once"):
            derive_accessible_name(self.fixture_path, 96, selected_days=("Saturday", "Sunday"))


class PackagedNextSubPrepLifecycleValidatorTests(unittest.TestCase):
    def _make_verified_reuse(self) -> dict:
        docs = valid_documents()
        current = docs["sourceInventory"]["files"]
        prepared = copy.deepcopy(current)
        runner = next(row for row in prepared if row["path"] == "scripts/phase9/run_packaged_next_sub_prep_lifecycle.ps1")
        runner["sizeBytes"] += 1
        runner["sha256"] = _digest("prepared older P9 runner")
        prepared.sort(key=lambda row: row["path"].casefold())
        prepared_sha = _canonical_sha(prepared)
        differences = _compare_prepared_source_inventory(current, prepared)
        docs["sourceInventory"]["preparedFiles"] = prepared
        manifest = docs["manifest"]
        manifest["candidateSource"]["preparedSourceFingerprint"] = prepared_sha
        manifest["candidateSource"]["reuseDifferences"] = differences
        manifest["configuration"]["runnerIntegrityLevel"] = "standard"
        build = manifest["configuration"]["packageBuild"]
        build.update(mode="verified-prebuilt-reuse", integrityLevel="elevated",
                     runnerIntegrityLevel="standard", integrityLevelsDifferent=True)
        build["reuse"] = {
            "preparedEvidenceDirectory": r"C:\evidence\p908\prepared",
            "preparedManifestPath": r"C:\evidence\p908\run\prepared-release.json",
            "preparedManifestSha256": _digest("prepared manifest"),
            "integrityLevelEvidence": {"level": "elevated", "kind": "main-agent-confirmed-elevated-build",
                                       "reference": "prior elevated Release build evidence"},
            "preparedSourceSha256": prepared_sha,
            "preparedPackageSha256": manifest["package"]["sha256"],
            "preparedApplicationSha256": manifest["application"]["sha256"],
            "preparedFixtureGeneratorSha256": manifest["qtInstallation"]["fixtureGenerators"][0]["executableSha256"],
            "preparedFixtureSha256": manifest["fixture"]["sha256"],
            "preparedCMakeCacheSha256": manifest["configuration"]["cmakeCacheSha256"],
            "phase9OnlySourceDifferences": differences,
        }
        manifest["package"]["sourceIdentityMode"] = "verified-prebuilt-non-phase9-equivalence"
        manifest["package"]["sourceFingerprintAtBuild"] = prepared_sha
        return docs

    @staticmethod
    def _message_click(action: dict) -> dict:
        pid = action["uiaProcessId"]
        hwnd = action["hwnd"]
        action["method"] = "uia-verified-window-message-click"
        action["pointerAttempt"] = {
            "method": "uia-verified-pointer-click", "fallbackReason": "foreground-not-verified",
            "windowHandle": hwnd, "expectedProcessId": pid, "windowProcessId": pid,
            "foregroundVerified": False, "foregroundWindowHandle": 0,
            "foregroundProcessId": 0, "setForegroundWindowReturned": False,
            "foregroundThreadInputAttachErrorCode": 0, "inputEventsSent": 0,
        }
        action["nativeActivation"] = {
            "kind": "verified-window-message",
            "sequence": ["WaitForInputIdle", "ScreenToClient", "GetClientRect",
                         "WM_MOUSEMOVE", "WM_LBUTTONDOWN", "WM_LBUTTONUP"],
            "windowHandle": hwnd, "expectedProcessId": pid, "windowProcessId": pid,
            "screenX": 350, "screenY": 255, "clientX": 350, "clientY": 255,
            "clientBounds": {"left": 0, "top": 0, "right": 1000, "bottom": 600},
            "waitForInputIdleResult": 0, "waitForInputIdleSucceeded": True,
            "mouseMoveSent": True, "leftButtonDownSent": True,
            "leftButtonUpSent": True, "messagesSent": 3,
        }
        return action

    def test_process_image_query_uses_unicode_marshalling_and_self_check(self) -> None:
        runner = Path(__file__).with_name("run_packaged_next_sub_prep_lifecycle.ps1")
        source = runner.read_text(encoding="utf-8")
        self.assertRegex(
            source,
            r'\[DllImport\("kernel32\.dll", SetLastError = true, CharSet = CharSet\.Unicode\)\]\s+'
            r'private static extern bool QueryFullProcessImageNameW',
        )
        self.assertIn("Native process image path query returned", source)

    def test_process_launch_forces_qt_stderr_and_isolates_user_state(self) -> None:
        runner = Path(__file__).with_name("run_packaged_next_sub_prep_lifecycle.ps1")
        source = runner.read_text(encoding="utf-8")
        for environment_key in (
            "CLASSMNGR_SETTINGS_ROOT",
            "LOCALAPPDATA",
            "APPDATA",
            "TEMP",
            "TMP",
        ):
            self.assertIn(f'$startInfo.Environment["{environment_key}"]', source)
        self.assertIn('$startInfo.Environment["QT_FORCE_STDERR_LOGGING"] = "1"', source)

    def test_page_lifecycle_records_allow_missing_optional_checkpoint_fields(self) -> None:
        runner = Path(__file__).with_name("run_packaged_next_sub_prep_lifecycle.ps1")
        source = runner.read_text(encoding="utf-8")
        self.assertIn("function Get-OptionalObjectProperty", source)
        self.assertIn('checkpoint = (Get-OptionalObjectProperty $parsed "checkpoint")', source)
        self.assertIn("$capturedValue.Kind -ne [DateTimeKind]::Utc", source)
        self.assertIn("$capturedValue.ToUniversalTime().ToString(\"o\")", source)

    def test_single_wait_results_are_wrapped_before_count_checks(self) -> None:
        runner = Path(__file__).with_name("run_packaged_next_sub_prep_lifecycle.ps1")
        source = runner.read_text(encoding="utf-8")
        self.assertIn('$initialRoutes = @(Wait-ForCondition $process', source)
        self.assertIn('$baselineEvents = @(Wait-ForCondition $process', source)

    def test_page_release_snapshots_are_sampled_before_waiting_for_later_delays(self) -> None:
        runner = Path(__file__).with_name("run_packaged_next_sub_prep_lifecycle.ps1")
        source = runner.read_text(encoding="utf-8")
        first_suspend = source.index('$release = Wait-PageAction')
        first_suspend_sample = source.index('$releaseSample = Add-PageReleaseSample', first_suspend)
        first_1s = source.index('$release1 = Wait-PageAction', first_suspend)
        first_1s_sample = source.index('$release1Sample = Add-PageReleaseSample', first_1s)
        first_5s = source.index('$release5 = Wait-PageAction', first_1s)
        first_5s_sample = source.index('$release5Sample = Add-PageReleaseSample', first_5s)
        self.assertLess(first_suspend, first_suspend_sample)
        self.assertLess(first_suspend_sample, first_1s)
        self.assertLess(first_1s, first_1s_sample)
        self.assertLess(first_1s_sample, first_5s)
        self.assertLess(first_5s, first_5s_sample)
        self.assertIn('$sampleDelay -gt $MaxEventSampleDelayMilliseconds', source)

    def test_paired_checkpoint_timestamp_allows_equivalent_json_fraction_precision(self) -> None:
        documents = valid_documents()
        sample = next(row for row in documents["processMetrics"]
                      if row.get("phase") == "explicit-refresh")
        original = sample["appCheckpoint"]["captured_at"]
        base, fraction = original[:-1].split(".")
        sample["appCheckpoint"]["captured_at"] = base + "." + fraction.rstrip("0") + "Z"
        self.assertNotEqual(sample["appCheckpoint"]["captured_at"], original)
        self.assertEqual(validate(documents)["validationStatus"], "valid")

        sample["appCheckpoint"]["memory"]["working_set_bytes"] += 1
        with self.assertRaisesRegex(EvidenceValidationError, "not paired to its app event"):
            validate(documents)

    def test_repeat_cycle_widget_count_uses_enclosing_page_snapshot(self) -> None:
        runner = Path(__file__).with_name("run_packaged_next_sub_prep_lifecycle.ps1")
        source = runner.read_text(encoding="utf-8")
        self.assertIn('$refreshEvent.record.page.widget_count -ne [int]$select96.record.page.widget_count', source)
        self.assertNotIn('$postRefreshMetrics.widget_count', source)

    def test_foreground_pointer_path_records_foreground_and_input_identity(self) -> None:
        runner = Path(__file__).with_name("run_packaged_next_sub_prep_lifecycle.ps1")
        source = runner.read_text(encoding="utf-8")
        self.assertIn("public uint foregroundThreadInputAttachErrorCode", source)
        self.assertIn("foregroundThreadInputAttachErrorCode = [Int64]$pointer.foregroundThreadInputAttachErrorCode", source)
        self.assertIn('sequence = @("AllowSetForegroundWindow(pid)"', source)
        self.assertIn("result.setActiveWindowReturnedNonZero = SetActiveWindow(window)", source)
        self.assertIn("result.setFocusReturnedNonZero = SetFocus(window)", source)
        self.assertIn("foreground == window && result.foregroundProcessId == expectedProcessId", source)
        self.assertIn("if (!result.foregroundVerified)", source)
        self.assertIn("if (result.inputEventsSent != 2)", source)
        self.assertNotIn("Could not attach UIA caller to foreground thread.", source)

    @staticmethod
    def _first_class_selection(documents: dict) -> tuple[dict, dict]:
        row = next(item for item in documents["navigationMetrics"]
                   if item.get("actionKind") == "class-selection")
        return row, row["uiaAction"]["fixtureDerivedTarget"]

    def test_class_row_target_is_bound_to_fixture_helper_and_exact_accessible_name(self) -> None:
        documents = valid_documents()
        summary = validate(documents)
        self.assertEqual(summary["validationStatus"], "valid")
        row, target = self._first_class_selection(documents)
        self.assertEqual(row["selectedAccessibleName"], target["accessibleName"])
        self.assertEqual(target["classId"], "96")
        self.assertEqual(target["grade"], "M2")
        self.assertEqual(target["exactVisibleMatchCount"], 1)

    def test_class_row_target_rejects_fixture_hash_mismatch(self) -> None:
        documents = valid_documents()
        _, target = self._first_class_selection(documents)
        target["fixtureSha256"] = _digest("different fixture")
        with self.assertRaisesRegex(EvidenceValidationError, "exact launched fixture"):
            validate(documents)

    def test_class_row_target_rejects_wrong_accessible_name(self) -> None:
        documents = valid_documents()
        _, target = self._first_class_selection(documents)
        target["accessibleName"] += " wrong"
        with self.assertRaisesRegex(EvidenceValidationError, "accessible name"):
            validate(documents)

    def test_class_row_target_rejects_wrong_uia_pid(self) -> None:
        documents = valid_documents()
        _, target = self._first_class_selection(documents)
        target["rowProcessId"] += 1
        with self.assertRaisesRegex(EvidenceValidationError, "class row UIA identity"):
            validate(documents)

    def test_class_row_target_rejects_missing_exact_visible_row_and_bad_scroll_order(self) -> None:
        documents = valid_documents()
        _, target = self._first_class_selection(documents)
        target["scrollTrace"][0]["visibleRows"][0]["isOffscreen"] = True
        with self.assertRaisesRegex(EvidenceValidationError, "final scroll position"):
            validate(documents)

        documents = valid_documents()
        _, target = self._first_class_selection(documents)
        target["scrollSteps"] = 1
        with self.assertRaisesRegex(EvidenceValidationError, "scroll trace is incomplete"):
            validate(documents)

    def test_class_row_target_accepts_verified_window_wheel_with_adjacent_visible_rows(self) -> None:
        documents = valid_documents()
        _, target = self._first_class_selection(documents)
        expected_name = target["accessibleName"]
        pid = target["rowProcessId"]
        other_name = "Level 01 • earlier visible class"
        other_row = {
            "accessibleName": other_name, "helpText": "",
            "automationId": "ClassMngrNext.subPrepClassSummaryList",
            "controlType": "ControlType.ListItem", "processId": pid,
            "isOffscreen": False,
            "rectangle": {"x": 120.0, "y": 240.0, "width": 480.0, "height": 30.0},
        }
        target["scrollMethod"] = "verified-window-message-wheel"
        target["scrollSteps"] = 1
        target["scrollTrace"] = [
            {
                "step": 0, "scrollMethod": "verified-window-message-wheel",
                "verticalScrollPercent": None, "visibleRows": [other_row],
                "scrollInput": {
                    "method": "verified-window-message-wheel",
                    "sequence": ["WaitForInputIdle", "ScreenToClient", "GetClientRect", "WM_MOUSEWHEEL"],
                    "windowHandle": target["mainWindowHandle"],
                    "expectedProcessId": pid, "windowProcessId": pid,
                    "screenX": 300, "screenY": 250, "clientX": 300, "clientY": 250,
                    "clientBounds": {"left": 0, "top": 0, "right": 1000, "bottom": 800},
                    "listBounds": {"left": 100, "top": 100, "right": 500, "bottom": 400},
                    "wheelDelta": -120, "waitForInputIdleSucceeded": True,
                    "messageSent": True,
                    "beforeVisibleAccessibleNames": [other_name],
                    "afterVisibleAccessibleNames": [expected_name],
                    "visibleRowsChanged": True,
                },
            },
            {
                "step": 1, "scrollMethod": "verified-window-message-wheel",
                "verticalScrollPercent": None,
                "visibleRows": target["scrollTrace"][0]["visibleRows"],
            },
        ]
        trace_action = next(item for item in documents["uiaActions"]
                            if item.get("processInstanceId") == documents["manifest"]["processRuns"][0]["processInstanceId"]
                            and item.get("actionKind") == "sub-prep-class-selection")
        trace_action["fixtureDerivedTarget"] = copy.deepcopy(target)
        resolution_action = next(item for item in documents["uiaActions"]
                                 if item.get("processInstanceId") == trace_action["processInstanceId"]
                                 and item.get("actionKind") == "class-row-resolution"
                                 and item.get("actionIndex") == 0)
        resolution_action["resolution"] = copy.deepcopy(target)
        self.assertEqual(validate(documents)["validationStatus"], "valid")

    def test_class_row_target_rejects_unverified_wheel_scroll(self) -> None:
        documents = valid_documents()
        _, target = self._first_class_selection(documents)
        target["scrollMethod"] = "verified-window-message-wheel"
        target["scrollSteps"] = 1
        target["scrollTrace"] = [
            {"step": 0, "scrollMethod": "verified-window-message-wheel",
             "verticalScrollPercent": None, "visibleRows": [],
             "scrollInput": {"method": "verified-window-message-wheel",
                             "sequence": ["WaitForInputIdle", "ScreenToClient", "GetClientRect", "WM_MOUSEWHEEL"],
                             "windowHandle": target["mainWindowHandle"],
                             "expectedProcessId": target["rowProcessId"],
                             "windowProcessId": target["rowProcessId"],
                             "waitForInputIdleSucceeded": True, "messageSent": True,
                             "wheelDelta": -120, "visibleRowsChanged": False}},
            {"step": 1, "scrollMethod": "verified-window-message-wheel",
             "verticalScrollPercent": None, "visibleRows": target["scrollTrace"][0]["visibleRows"]},
        ]
        with self.assertRaisesRegex(EvidenceValidationError, "verified scroll"):
            validate(documents)

    def test_class_row_resolution_requires_one_ordered_source_bound_record_per_action(self) -> None:
        documents = valid_documents()
        resolution = next(item for item in documents["uiaActions"]
                          if item.get("actionKind") == "class-row-resolution")
        documents["uiaActions"].remove(resolution)
        with self.assertRaisesRegex(EvidenceValidationError, "one fixture-derived class-row resolution"):
            validate(documents)

        documents = valid_documents()
        resolution = next(item for item in documents["uiaActions"]
                          if item.get("actionKind") == "class-row-resolution")
        resolution["actionIndex"] = 1
        with self.assertRaisesRegex(EvidenceValidationError, "class-row resolution order"):
            validate(documents)

    def test_complete_three_process_evidence_passes_with_generation_advisory(self) -> None:
        documents = valid_documents()
        summary = validate(documents)
        self.assertEqual(summary["schema"], SUMMARY_SCHEMA)
        self.assertEqual(summary["validationStatus"], "valid")
        self.assertEqual(summary["gateResult"], "passed")
        self.assertEqual(summary["processCount"], 3)
        self.assertTrue(summary["processSummaries"][0]["generationPeakExceedsAdvisoryCeiling"])

    def test_verified_prebuilt_reuse_passes_with_phase9_only_source_delta(self) -> None:
        documents = self._make_verified_reuse()
        summary = validate(documents)
        self.assertEqual(summary["validationStatus"], "valid")
        self.assertEqual(summary["gateResult"], "passed")
        self.assertTrue(documents["manifest"]["configuration"]["packageBuild"]["integrityLevelsDifferent"])

    def test_prebuilt_reuse_rejects_non_phase9_source_mismatch(self) -> None:
        documents = self._make_verified_reuse()
        prepared = documents["sourceInventory"]["preparedFiles"]
        row = next(item for item in prepared if item["path"] == "src/next/ui/sub_prep_page.cpp")
        row["sha256"] = _digest("changed product source")
        row["sizeBytes"] += 1
        prepared_sha = _canonical_sha(prepared)
        reuse = documents["manifest"]["configuration"]["packageBuild"]["reuse"]
        reuse["preparedSourceSha256"] = prepared_sha
        documents["manifest"]["candidateSource"]["preparedSourceFingerprint"] = prepared_sha
        with self.assertRaisesRegex(EvidenceValidationError, "outside scripts/phase9"):
            validate(documents)

    def test_prebuilt_reuse_rejects_same_integrity_level(self) -> None:
        documents = self._make_verified_reuse()
        documents["manifest"]["configuration"]["packageBuild"]["runnerIntegrityLevel"] = "elevated"
        documents["manifest"]["configuration"]["runnerIntegrityLevel"] = "elevated"
        with self.assertRaisesRegex(EvidenceValidationError, "separate the elevated Release build"):
            validate(documents)

    def test_prebuilt_reuse_rejects_package_identity_mismatch(self) -> None:
        documents = self._make_verified_reuse()
        documents["manifest"]["configuration"]["packageBuild"]["reuse"]["preparedPackageSha256"] = _digest("different package")
        with self.assertRaisesRegex(EvidenceValidationError, "reused package identity"):
            validate(documents)

    def test_prebuilt_reuse_rejects_cmake_cache_mismatch(self) -> None:
        documents = self._make_verified_reuse()
        documents["manifest"]["configuration"]["packageBuild"]["reuse"]["preparedCMakeCacheSha256"] = _digest("other CMake cache")
        with self.assertRaisesRegex(EvidenceValidationError, "prebuilt Release CMakeCache"):
            validate(documents)

    def test_prebuilt_reuse_rejects_unproven_elevated_integrity(self) -> None:
        documents = self._make_verified_reuse()
        documents["manifest"]["configuration"]["packageBuild"]["reuse"]["integrityLevelEvidence"] = {
            "level": "elevated", "kind": "unrecognized", "reference": "unknown"
        }
        with self.assertRaisesRegex(EvidenceValidationError, "integrity evidence is missing"):
            validate(documents)

    def test_runner_keeps_build_reuse_opt_in_and_smoke_route_fail_closed(self) -> None:
        runner = Path(__file__).with_name("run_packaged_next_sub_prep_lifecycle.ps1")
        source = runner.read_text(encoding="utf-8")
        self.assertIn('[string]$PreparedReleaseEvidenceDirectory = ""', source)
        self.assertIn('[switch]$RouteSmokeOnly', source)
        self.assertIn('if ($PreparedReleaseEvidenceDirectory)', source)
        self.assertIn('if ($buildExit -ne 0) { throw "Required Release package/fixture targets failed;', source)
        self.assertIn('"uia-verified-window-message-click"', source)
        self.assertIn('$smokeActivation.messagesSent -eq 3', source)
        self.assertIn('SendMessageTimeoutW', source)
        self.assertIn('$processResult.smokeGeneratedPackage.generationOutcome -cne "completed"', source)
        self.assertIn('Wait-SubPrepSummaryReady', source)
        self.assertIn('Enable-SubPrepSaturdaySummaryFilter', source)
        self.assertIn('Get-UiaElementById $window "subPrepDay_5" "ControlType.CheckBox"', source)
        self.assertIn('actionKind = "sub-prep-summary-day-filter"', source)
        self.assertIn('controlType = [string]$current.ControlType.ProgrammaticName', source)
        self.assertIn('$controlType = [string]$Element.Current.ControlType.ProgrammaticName', source)
        self.assertIn('listControlType = [string]$list.Current.ControlType.ProgrammaticName', source)
        self.assertIn('rowControlType = [string]$item.Current.ControlType.ProgrammaticName', source)
        self.assertIn('controlType = [string]$element.Current.ControlType.ProgrammaticName', source)
        self.assertIn('actionKind = if ($ActionKind -ceq "package-generation-submit") { "package-generation-submit-attempt" } else { $ActionKind }', source)
        self.assertIn('actionKind = "package-generation-submit-request"', source)
        self.assertIn('$submitClick.actionKind = "package-generation-submit"', source)
        self.assertIn('--selected-days Monday Tuesday Wednesday Thursday Friday Saturday', source)

    def test_app_checkpoint_json_preserves_iso_timestamp_strings(self) -> None:
        runner = Path(__file__).with_name("run_packaged_next_sub_prep_lifecycle.ps1")
        source = runner.read_text(encoding="utf-8")
        self.assertIn('ConvertFrom-Json -DateKind String -ErrorAction Stop', source)

    def test_summary_action_without_page_metrics_pairs_to_real_activation_snapshot(self) -> None:
        documents = valid_documents()
        for row in documents["startupLog"]:
            if row["line"].startswith("ClassMngrNext page lifecycle checkpoint="):
                record = json.loads(row["line"].partition("=")[2])
                if record.get("event") == "page-action-checkpoint" and record.get("checkpoint") == "summary-data-ready":
                    self.assertFalse(record["page_instance_available"])
                    self.assertIsNone(record["page"])
        summary = validate(documents)
        self.assertEqual(summary["validationStatus"], "valid")

    def test_summary_action_requires_activated_page_snapshot(self) -> None:
        documents = valid_documents()
        activation_index = next(index for index, row in enumerate(documents["startupLog"])
                                if row["line"].startswith("ClassMngrNext page lifecycle checkpoint=")
                                and (record := json.loads(row["line"].partition("=")[2])).get("event") == "page-lifecycle"
                                and record.get("page_key") == "sub_prep" and record.get("state") == "activated")
        row = documents["startupLog"][activation_index]
        record = json.loads(row["line"].partition("=")[2])
        record["state"] = "suspended"
        row["line"] = "ClassMngrNext page lifecycle checkpoint=" + json.dumps(record, separators=(",", ":"))
        with self.assertRaisesRegex(EvidenceValidationError, "pair all 11 summary-ready actions"):
            validate(documents)

    def test_summary_activation_must_follow_action_before_route(self) -> None:
        documents = valid_documents()
        summary_index = next(index for index, row in enumerate(documents["startupLog"])
                             if row["line"].startswith("ClassMngrNext page lifecycle checkpoint=")
                             and (record := json.loads(row["line"].partition("=")[2])).get("checkpoint") == "summary-data-ready")
        activation_index = next(index for index, row in enumerate(documents["startupLog"])
                                if row["line"].startswith("ClassMngrNext page lifecycle checkpoint=")
                                and (record := json.loads(row["line"].partition("=")[2])).get("event") == "page-lifecycle"
                                and record.get("page_key") == "sub_prep" and record.get("state") == "activated")
        documents["startupLog"][summary_index], documents["startupLog"][activation_index] = (
            documents["startupLog"][activation_index], documents["startupLog"][summary_index])
        with self.assertRaisesRegex(EvidenceValidationError, "summary action, page activation snapshot"):
            validate(documents)

    def test_summary_activation_rejects_corrupt_class_counts(self) -> None:
        documents = valid_documents()
        for row in documents["startupLog"]:
            if row["line"].startswith("ClassMngrNext page lifecycle checkpoint="):
                record = json.loads(row["line"].partition("=")[2])
                if record.get("event") == "page-lifecycle" and record.get("page_key") == "sub_prep" and record.get("state") == "activated":
                    record["page"]["sub_prep_runtime_metrics"]["summary_class_count"] = 95
                    row["line"] = "ClassMngrNext page lifecycle checkpoint=" + json.dumps(record, separators=(",", ":"))
                    break
        with self.assertRaisesRegex(EvidenceValidationError, "summary counts do not match"):
            validate(documents)

    def test_saturday_filter_is_required_and_correlated_to_full_summary_selection(self) -> None:
        documents = valid_documents()
        filter_action = next(row for row in documents["uiaActions"]
                             if row.get("actionKind") == "sub-prep-summary-day-filter")
        documents["uiaActions"].remove(filter_action)
        with self.assertRaisesRegex(EvidenceValidationError, "enable Saturday exactly once"):
            validate(documents)

        mutations = (
            ("accessibleName", "Sunday"),
            ("uiaProcessId", 99_999),
            ("toggleStateAfter", "Off"),
            ("summaryClassCountAfter", 30),
            ("verifiedByAppEventLogIndex", 999_999),
        )
        for field, value in mutations:
            with self.subTest(field=field):
                documents = valid_documents()
                action = next(row for row in documents["uiaActions"]
                              if row.get("actionKind") == "sub-prep-summary-day-filter")
                action[field] = value
                with self.assertRaises(EvidenceValidationError):
                    validate(documents)

    def test_verified_window_message_click_passes_with_exact_pid_bounds_and_authentic_events(self) -> None:
        documents = valid_documents()
        for action in documents["uiaActions"]:
            if action.get("method") == "uia-verified-pointer-click":
                self._message_click(action)
        for row in documents["navigationMetrics"]:
            action = row.get("uiaAction")
            if isinstance(action, dict) and action.get("method") == "uia-verified-pointer-click":
                self._message_click(action)
        summary = validate(documents)
        self.assertEqual(summary["validationStatus"], "valid")
        self.assertEqual(summary["gateResult"], "passed")

    def test_verified_window_message_click_rejects_wrong_hwnd_or_pid(self) -> None:
        documents = valid_documents()
        action = next(row for row in documents["navigationMetrics"] if row.get("transitionIndex") is not None)["uiaAction"]
        self._message_click(action)
        action["nativeActivation"]["windowProcessId"] += 1
        with self.assertRaisesRegex(EvidenceValidationError, "window-message HWND/PID identity mismatch"):
            validate(documents)

    def test_verified_window_message_click_rejects_missing_messages(self) -> None:
        documents = valid_documents()
        action = next(row for row in documents["navigationMetrics"] if row.get("transitionIndex") is not None)["uiaAction"]
        self._message_click(action)
        action["nativeActivation"]["messagesSent"] = 2
        with self.assertRaisesRegex(EvidenceValidationError, "did not deliver all three"):
            validate(documents)

    def test_verified_window_message_click_requires_failed_foreground_attempt(self) -> None:
        documents = valid_documents()
        action = next(row for row in documents["navigationMetrics"] if row.get("transitionIndex") is not None)["uiaAction"]
        self._message_click(action)
        action["pointerAttempt"]["foregroundVerified"] = True
        with self.assertRaisesRegex(EvidenceValidationError, "did not follow a failed foreground-only"):
            validate(documents)

    def test_threshold_equality_is_a_valid_failed_gate(self) -> None:
        documents = valid_documents(idle_working_set=THRESHOLD_BYTES)
        summary = validate(documents)
        self.assertEqual(summary["validationStatus"], "valid")
        self.assertEqual(summary["gateResult"], "failed")
        self.assertFalse(summary["strictNormalUseBudgetPassed"])
        self.assertEqual(summary["processSummaries"][0]["idleWorkingSetMaximumBytes"], THRESHOLD_BYTES)

    def test_missing_idle_sample_is_invalid(self) -> None:
        documents = valid_documents()
        documents["processMetrics"] = [
            row for row in documents["processMetrics"]
            if not (row.get("processInstanceId") == "process-01" and row.get("phase") == "idle"
                    and row.get("sampleIndex") == 120)
        ]
        with self.assertRaisesRegex(EvidenceValidationError, "301 samples"):
            validate(documents)

    def test_failed_navigation_route_is_invalid(self) -> None:
        documents = valid_documents()
        route = next(row for row in documents["startupLog"]
                     if row["processInstanceId"] == "process-01" and "cause=selection from=my_workspace" in row["line"])
        route["line"] = route["line"].replace("outcome=completed", "outcome=failed")
        with self.assertRaises(EvidenceValidationError):
            validate(documents)

    def test_duplicate_navigation_route_is_invalid(self) -> None:
        documents = valid_documents()
        route = next(row for row in documents["startupLog"]
                     if row["processInstanceId"] == "process-01" and "cause=selection from=my_workspace" in row["line"])
        documents["startupLog"].append(copy.deepcopy(route))
        with self.assertRaisesRegex(EvidenceValidationError, "22 authentic"):
            validate(documents)

    def test_nonzero_pdf_body_count_is_invalid(self) -> None:
        documents = valid_documents()
        route = next(row for row in documents["startupLog"]
                     if row["processInstanceId"] == "process-02" and "cause=selection from=my_workspace" in row["line"])
        route["line"] = route["line"].replace("loaded_pdf_body_count=0", "loaded_pdf_body_count=1")
        with self.assertRaises(EvidenceValidationError):
            validate(documents)

    def test_wrong_uia_pid_is_invalid(self) -> None:
        documents = valid_documents()
        route = next(row for row in documents["navigationMetrics"]
                     if row.get("processInstanceId") == "process-01" and row.get("transitionIndex") == 0)
        route["uiaAction"]["uiaProcessId"] += 1
        with self.assertRaisesRegex(EvidenceValidationError, "PID mismatch"):
            validate(documents)

    def test_missing_generation_submit_trace_is_invalid(self) -> None:
        documents = valid_documents()
        documents["uiaActions"] = [row for row in documents["uiaActions"]
                                   if not (row.get("processInstanceId") == "process-03"
                                           and row.get("actionKind") == "package-generation-submit")]
        with self.assertRaisesRegex(EvidenceValidationError, "generation dialog and submit"):
            validate(documents)

    def test_generation_dialog_and_submit_accept_verified_window_messages(self) -> None:
        documents = valid_documents()
        process = next(row for row in documents["manifest"]["processRuns"]
                       if row["processInstanceId"] == "process-01")
        pid = process["processIdentity"]["pid"]
        for row in documents["uiaActions"]:
            if row.get("processInstanceId") != "process-01" or row.get("actionKind") not in {
                "open-generation-dialog", "package-generation-submit"
            }:
                continue
            window_handle = row["dialogHandle"] if row["actionKind"] == "package-generation-submit" else row["hwnd"]
            if row["actionKind"] == "package-generation-submit":
                row["hwnd"] = window_handle
            row["method"] = "uia-verified-window-message-click"
            row["nativeActivation"] = {
                "kind": "verified-window-message",
                "sequence": ["WaitForInputIdle", "ScreenToClient", "GetClientRect",
                             "WM_MOUSEMOVE", "WM_LBUTTONDOWN", "WM_LBUTTONUP"],
                "windowHandle": window_handle,
                "expectedProcessId": pid,
                "windowProcessId": pid,
                "screenX": 350,
                "screenY": 255,
                "clientX": 50,
                "clientY": 55,
                "clientBounds": {"left": 0, "top": 0, "right": 800, "bottom": 600},
                "waitForInputIdleSucceeded": True,
                "mouseMoveSent": True,
                "leftButtonDownSent": True,
                "leftButtonUpSent": True,
                "messagesSent": 3,
            }
            row["pointerAttempt"] = {
                "method": "uia-verified-pointer-click",
                "fallbackReason": "foreground-not-verified",
                "windowHandle": window_handle,
                "expectedProcessId": pid,
                "windowProcessId": pid,
                "foregroundVerified": False,
                "inputEventsSent": 0,
            }
        validate(documents)

    def test_generation_dialog_rejects_unverified_invoke(self) -> None:
        documents = valid_documents()
        dialog = next(row for row in documents["uiaActions"]
                      if row.get("processInstanceId") == "process-01"
                      and row.get("actionKind") == "open-generation-dialog")
        dialog["method"] = "uia-verified-invoke"
        with self.assertRaisesRegex(EvidenceValidationError, "unaccepted UIA method"):
            validate(documents)

    def test_generation_submit_rejects_wrong_dialog_hwnd(self) -> None:
        documents = valid_documents()
        submit = next(row for row in documents["uiaActions"]
                      if row.get("processInstanceId") == "process-01"
                      and row.get("actionKind") == "package-generation-submit")
        submit["hwnd"] += 1
        with self.assertRaisesRegex(EvidenceValidationError, "generation submit UIA identity"):
            validate(documents)

    def test_click_point_must_stay_inside_exact_uia_control(self) -> None:
        documents = valid_documents()
        submit = next(row for row in documents["uiaActions"]
                      if row.get("processInstanceId") == "process-01"
                      and row.get("actionKind") == "package-generation-submit")
        submit["nativeActivation"]["screenX"] = submit["rectangle"]["x"] + submit["rectangle"]["width"]
        with self.assertRaisesRegex(EvidenceValidationError, "outside the exact UIA-selected control"):
            validate(documents)

    def test_qt_runtime_identity_mismatch_is_invalid(self) -> None:
        documents = valid_documents()
        documents["manifest"]["qtInstallation"]["runtimeFiles"][0]["sha256"] = "0" * 64
        with self.assertRaises(EvidenceValidationError):
            validate(documents)

    def test_failed_or_unsequenced_page_release_is_invalid(self) -> None:
        documents = valid_documents()
        release = next(row for row in documents["startupLog"]
                       if row["processInstanceId"] == "process-01"
                       and row["line"].find('"checkpoint":"page-suspended"') >= 0)
        release["line"] = release["line"].replace('"release_sequence":1', '"release_sequence":2')
        with self.assertRaises(EvidenceValidationError):
            validate(documents)


if __name__ == "__main__":
    unittest.main()
