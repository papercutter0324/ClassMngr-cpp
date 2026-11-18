#!/usr/bin/env python3
"""Validate a representative-only P9-08 Sub Prep retention trace.

This diagnostic validates route, package, process, lifecycle, and cadence
evidence. It deliberately does not evaluate the P9 normal-use memory budget.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import statistics
import sqlite3
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SUMMARY_SCHEMA = "classmngr-phase9-p908-retention-diagnostic-summary-v1"
THRESHOLD_BYTES = 262_144_000
ROUTE_RE = re.compile(
    r"^ClassMngrNext navigation event=page-route outcome=completed cause=selection "
    r"from=(?P<from>[a-z_]+) to=(?P<to>[a-z_]+) "
    r"catalog_document_count=(?P<documents>\d+) loaded_pdf_body_count=(?P<bodies>\d+)$"
)
CHECKPOINT_PREFIX = "ClassMngrNext page lifecycle checkpoint="


class EvidenceError(ValueError):
    pass


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise EvidenceError(message)


def _read_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as error:
        raise EvidenceError(f"cannot read {path.name}: {error}") from error


def _read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8-sig") as stream:
            for line_number, line in enumerate(stream, 1):
                _require(bool(line.strip()), f"{path.name} has an empty row at {line_number}")
                try:
                    row = json.loads(line)
                except json.JSONDecodeError as error:
                    raise EvidenceError(f"{path.name} row {line_number} is malformed JSON") from error
                _require(isinstance(row, dict), f"{path.name} row {line_number} is not an object")
                rows.append(row)
    except OSError as error:
        raise EvidenceError(f"cannot read {path.name}: {error}") from error
    return rows


def _canonical_inventory_hash(rows: list[dict[str, Any]], label: str) -> str:
    normalized: list[tuple[str, int, str]] = []
    seen: set[str] = set()
    for row in rows:
        path = str(row.get("path", ""))
        size = row.get("sizeBytes")
        digest = str(row.get("sha256", ""))
        _require(path and isinstance(size, int) and size >= 0, f"{label} has an invalid path or size")
        _require(re.fullmatch(r"[0-9a-f]{64}", digest) is not None, f"{label} has an invalid SHA-256")
        key = path.casefold()
        _require(key not in seen, f"{label} duplicates path {path}")
        seen.add(key)
        normalized.append((path, size, digest))
    normalized.sort(key=lambda item: item[0].casefold())
    canonical = "".join(f"{path}\t{size}\t{digest}\n" for path, size, digest in normalized)
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def _verify_inventory_files(rows: list[dict[str, Any]], root: Path, label: str) -> None:
    canonical_root = root.resolve()
    for row in rows:
        target = root.joinpath(*str(row["path"]).replace("\\", "/").split("/"))
        try:
            target.resolve().relative_to(canonical_root)
        except (ValueError, OSError) as error:
            raise EvidenceError(f"{label} path escapes its root: {row['path']}") from error
        _require(target.is_file(), f"{label} file is missing: {row['path']}")
        _require(target.stat().st_size == int(row["sizeBytes"]), f"{label} size changed: {row['path']}")
        actual = hashlib.sha256(target.read_bytes()).hexdigest()
        _require(actual == row["sha256"], f"{label} SHA-256 changed: {row['path']}")


def _run_artifact_path(value: Any, run_directory: Path, label: str) -> Path:
    path = Path(str(value)).resolve()
    try:
        path.relative_to(run_directory.resolve())
    except ValueError as error:
        raise EvidenceError(f"{label} is outside the unique run directory") from error
    _require(path.is_file(), f"{label} is missing")
    return path


def _verify_fixture_hashes(
    fixture: dict[str, Any],
    fixture_copy: dict[str, Any],
    process: dict[str, Any],
    run_directory: Path,
) -> dict[str, str]:
    """Verify the original, transformed pre-launch, and post-exit fixture files.

    The app persists settings into its working fixture. Therefore the working
    file is expected to differ from the pre-launch snapshot after exit; each
    physical file is checked against the hash recorded for its own lifecycle
    point.
    """
    master_path = _run_artifact_path(fixture.get("masterPath"), run_directory, "master representative fixture")
    copy_path = _run_artifact_path(fixture_copy.get("path"), run_directory, "per-process transformed fixture")
    snapshot_path = _run_artifact_path(
        fixture_copy.get("preLaunchSnapshotPath"), run_directory, "pre-launch transformed fixture snapshot"
    )
    _require(copy_path != snapshot_path, "pre-launch snapshot must be separate from the app working fixture")

    master_sha = hashlib.sha256(master_path.read_bytes()).hexdigest()
    _require(master_sha == fixture.get("sha256"), "master representative fixture hash differs from its inventory")
    _require(fixture_copy.get("sourceFixtureSha256") == master_sha, "transformed fixture source hash differs from the master")
    transform = fixture_copy.get("outputPreparation", {})
    _require(isinstance(transform, dict), "transformed fixture metadata is malformed")
    _require(transform.get("sourceSha256") == master_sha, "transform source hash differs from the master")

    prelaunch_sha = hashlib.sha256(snapshot_path.read_bytes()).hexdigest()
    expected_prelaunch_sha = fixture_copy.get("preLaunchSnapshotSha256")
    _require(
        prelaunch_sha
        == expected_prelaunch_sha
        == fixture_copy.get("sha256BeforeLaunch")
        == transform.get("preparedSha256"),
        "physical pre-launch transformed fixture SHA-256 differs from its recorded hash",
    )
    _require(process.get("fixturePathPassedToApp") == str(copy_path), "app fixture path differs from the transformed fixture")
    _require(
        process.get("fixturePreLaunchSnapshotPath") == str(snapshot_path)
        and process.get("fixturePreLaunchSnapshotSha256") == prelaunch_sha
        and process.get("fixtureSha256PassedToApp") == prelaunch_sha,
        "process pre-launch fixture identity differs from its physical snapshot",
    )

    exit_sha = hashlib.sha256(copy_path.read_bytes()).hexdigest()
    _require(exit_sha == process.get("fixtureHashAtExit"), "physical post-exit fixture SHA-256 differs from its recorded hash")
    return {"master": master_sha, "preLaunch": prelaunch_sha, "postExit": exit_sha}


def _timestamp(value: Any, label: str) -> datetime:
    _require(isinstance(value, str) and value, f"{label} timestamp is missing")
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
    except ValueError as error:
        raise EvidenceError(f"{label} timestamp is malformed") from error
    _require(parsed.tzinfo is not None, f"{label} timestamp has no timezone")
    return parsed.astimezone(timezone.utc)


def _checkpoint_records(startup_rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for row in startup_rows:
        line = str(row.get("line", ""))
        if line.startswith(CHECKPOINT_PREFIX):
            try:
                record = json.loads(line[len(CHECKPOINT_PREFIX) :])
            except json.JSONDecodeError as error:
                raise EvidenceError("startup log contains malformed page checkpoint JSON") from error
            _require(isinstance(record, dict), "page checkpoint is not an object")
            records.append({"log": row, "record": record})
    return records


def _sqlite_tables(path: Path) -> dict[str, dict[str, Any]]:
    uri = path.resolve().as_uri() + "?mode=ro"
    try:
        connection = sqlite3.connect(uri, uri=True)
        names = [row[0] for row in connection.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")]
        result: dict[str, dict[str, Any]] = {}
        for name in names:
            escaped = name.replace('"', '""')
            schema = [tuple(row) for row in connection.execute(f'PRAGMA table_info("{escaped}")')]
            rows = [tuple(row) for row in connection.execute(f'SELECT * FROM "{escaped}"')]
            rows.sort(key=lambda row: json.dumps(row, ensure_ascii=False, default=str, separators=(",", ":")))
            payload = json.dumps(rows, ensure_ascii=False, default=str, separators=(",", ":")).encode("utf-8")
            result[name] = {"schema": schema, "rowCount": len(rows), "rowsSha256": hashlib.sha256(payload).hexdigest()}
        integrity = connection.execute("PRAGMA integrity_check").fetchone()[0]
        connection.close()
    except sqlite3.Error as error:
        raise EvidenceError(f"cannot inspect measured fixture SQLite contents: {error}") from error
    _require(integrity == "ok", "measured fixture SQLite integrity check failed")
    return result


def _sqlite_app_settings(path: Path) -> dict[str, str]:
    uri = path.resolve().as_uri() + "?mode=ro"
    try:
        with sqlite3.connect(uri, uri=True) as connection:
            return {str(key): str(value) for key, value in connection.execute("SELECT key,value FROM app_settings")}
    except sqlite3.Error as error:
        raise EvidenceError(f"cannot inspect app_settings table: {error}") from error


def _validate_sample_identity(sample: dict[str, Any], identity: dict[str, Any], label: str) -> None:
    _require(sample.get("processInstanceId") == "process-01", f"{label} has the wrong process instance")
    _require(int(sample.get("pid", 0)) == int(identity.get("pid", -1)), f"{label} PID differs from app identity")
    _require(sample.get("processCreatedAtUtc") == identity.get("createdAtUtc"), f"{label} process creation time differs")
    _require(sample.get("canonicalExecutablePath") == identity.get("canonicalExecutablePath"), f"{label} executable path differs")
    _require(sample.get("architecture") == "x64", f"{label} is not x64")
    _require(sample.get("sha256") == identity.get("sha256"), f"{label} executable hash differs")
    for key in ("currentWorkingSetBytes", "privateBytes", "peakWorkingSetBytes", "handles", "threads"):
        _require(isinstance(sample.get(key), int) and int(sample[key]) > 0, f"{label} has an invalid {key}")
    _timestamp(sample.get("timestampUtc"), f"{label} observation")


def validate_run_directory(run_directory: Path, expected_cycles: int) -> dict[str, Any]:
    _require(2 <= expected_cycles <= 30, "expected cycle count must be between 2 and 30")
    manifest = _read_json(run_directory / "manifest.json")
    diagnostic = _read_json(run_directory / "retention-diagnostic.json")
    source_inventory = _read_json(run_directory / "source-files.json")
    package_inventory = _read_json(run_directory / "package-files.json")
    generated_inventory = _read_json(run_directory / "generated-files.json")
    startup_rows = _read_jsonl(run_directory / "startup-log.jsonl")
    process_rows = _read_jsonl(run_directory / "process-metrics.jsonl")
    navigation_rows = _read_jsonl(run_directory / "navigation-metrics.jsonl")
    uia_rows = _read_jsonl(run_directory / "uia-actions.jsonl")

    _require(manifest.get("status") == "diagnostic-completed", "manifest is not a completed diagnostic")
    _require(manifest.get("taskId") == "P9_08_PACKAGED_NEXT_SUBPREP_LIFECYCLE", "task identity mismatch")
    _require(diagnostic.get("schema") == "classmngr-phase9-p908-retention-diagnostic-v1", "diagnostic schema mismatch")
    _require(diagnostic.get("runId") == manifest.get("runId"), "diagnostic run identity mismatch")
    _require(diagnostic.get("status") == "diagnostic-only", "diagnostic status is invalid")
    _require(diagnostic.get("budgetEvaluation") == "not-evaluated", "diagnostic must not present a budget decision")
    _require(diagnostic.get("totalSubPrepCycles") == expected_cycles, "diagnostic cycle count mismatch")
    _require(manifest.get("diagnostic", {}).get("totalCyclesIncludingGeneration") == expected_cycles, "manifest cycle contract mismatch")
    _require(manifest.get("diagnostic", {}).get("budgetEvaluation") == "not-evaluated", "manifest claims a budget evaluation")
    _require(manifest.get("candidateSource", {}).get("stableDuringRun") is True, "source changed during the trace")
    source_sha = str(manifest.get("candidateSource", {}).get("sha256", ""))
    _require(source_sha and manifest["candidateSource"].get("sha256AtRunEnd") == source_sha, "source start/end fingerprint mismatch")
    source_files = source_inventory.get("files")
    _require(isinstance(source_files, list), "source inventory is malformed")
    _require(len(source_files) == manifest.get("candidateSource", {}).get("fileCount"), "source inventory count differs from manifest")
    _require(_canonical_inventory_hash(source_files, "source inventory") == source_sha, "source inventory digest mismatch")
    repository_root = Path(str(manifest.get("repositoryRoot", "")))
    configuration = manifest.get("configuration", {})
    _require(configuration.get("cmakeCacheStableDuringRun") is True, "Release CMakeCache changed during trace")
    package = manifest.get("package", {})
    package_sha = str(package.get("sha256", ""))
    package_rows = package_inventory.get("files")
    _require(isinstance(package_rows, list), "package inventory is malformed")
    _require(len(package_rows) == package.get("fileCount"), "package inventory count differs from manifest")
    _require(_canonical_inventory_hash(package_rows, "package inventory") == package_sha, "package inventory digest mismatch")
    package_directory = Path(str(package.get("directory", "")))
    _verify_inventory_files(package_rows, package_directory, "package inventory")
    _require(package.get("candidateSourceSha256") == source_sha, "package source fingerprint differs from candidate")
    application = manifest.get("application", {})
    app_sha = str(application.get("sha256", ""))
    app_path = Path(str(application.get("canonicalPath", "")))
    _require(application.get("architecture") == "x64" and app_path.is_file(), "packaged app identity is missing")
    _require(hashlib.sha256(app_path.read_bytes()).hexdigest() == app_sha, "packaged app SHA-256 mismatch")
    _require(package.get("applicationSha256") == app_sha, "package app identity mismatch")

    fixture = manifest.get("fixture", {})
    _require((fixture.get("classCount"), fixture.get("teacherCount"), fixture.get("rosterCellCount")) == (96, 24, 7200), "fixture shape is not 96/24/7200")
    _require(fixture.get("outputPreparation", {}).get("requestedPackageClassCount") == 30, "weekday package fixture transform identity is missing")
    fixture_copies = fixture.get("perProcessCopies", [])
    _require(isinstance(fixture_copies, list) and len(fixture_copies) == 1 and fixture_copies[0].get("processInstanceId") == "process-01", "per-process fixture copy identity is missing")
    fixture_copy = fixture_copies[0]
    process_runs = manifest.get("processRuns")
    _require(isinstance(process_runs, list) and len(process_runs) == 1, "diagnostic must contain exactly one representative process")
    process = process_runs[0]
    _require(process.get("processInstanceId") == "process-01", "process instance identity mismatch")
    fixture_hashes = _verify_fixture_hashes(fixture, fixture_copy, process, run_directory)
    fixture_copy_path = Path(str(fixture_copy["path"]))
    fixture_snapshot_path = Path(str(fixture_copy["preLaunchSnapshotPath"]))
    _require(process.get("totalSubPrepCycles") == expected_cycles and process.get("retentionDiagnosticOnly") is True, "process cycle contract mismatch")
    _require(process.get("navigationTransitionCount") == 2 * expected_cycles, "process route transition count mismatch")
    _require(process.get("classSelectionCount") == expected_cycles, "process class selection count mismatch")
    _require(process.get("explicitRefreshCount") == expected_cycles, "process refresh count mismatch")
    _require(process.get("suspensionCount") == expected_cycles, "process suspension count mismatch")
    _require(process.get("processExitCode") == 0, "measured app did not exit cleanly")
    _require(process.get("normalUseBudgetPassed") is None and not process.get("budgetViolations"), "diagnostic contains a gate decision or budget violations")
    _require(process.get("idleSampleCount") == 301, "diagnostic idle trace does not contain 301 samples")
    _require(process.get("startupDocumentCatalog", {}).get("loadedPdfBodyCount") == 0, "startup document catalog loaded a PDF body")
    _require(process.get("startupReady", {}).get("checkpoint", {}).get("stage") == "startup-ready", "startup-ready evidence is missing")
    _require(process.get("workspaceOpened", {}).get("checkpoint", {}).get("stage") == "workspace-opened", "workspace-opened evidence is missing")
    _require(diagnostic.get("candidateSourceSha256") == source_sha, "diagnostic source binding mismatch")
    _require(diagnostic.get("packageSha256") == package_sha, "diagnostic package binding mismatch")
    _require(diagnostic.get("applicationSha256") == app_sha, "diagnostic application binding mismatch")
    _require(diagnostic.get("fixtureSha256") == fixture.get("sha256"), "diagnostic fixture binding mismatch")

    identity = process.get("processIdentity", {})
    _require(identity.get("canonicalExecutablePath") == str(app_path), "process and package executable paths differ")
    _require(identity.get("sha256") == app_sha and identity.get("architecture") == "x64", "process identity differs from package")
    expected_pid = int(identity.get("pid", 0))
    _require(expected_pid > 0 and identity.get("createdAtUtc"), "process PID or creation time is missing")
    _require(process.get("fixtureShapeAtExit", {}).get("integrityCheck") == "ok", "fixture database integrity check failed")
    _require(process.get("commandLineArguments") == [process.get("fixturePathPassedToApp")], "fixture path passed to the app is not recorded exactly")
    prelaunch_tables = _sqlite_tables(fixture_snapshot_path)
    exit_tables = _sqlite_tables(fixture_copy_path)
    _require(prelaunch_tables.keys() == exit_tables.keys(), "fixture table inventory changed during the run")
    for table_name in prelaunch_tables:
        if table_name == "app_settings":
            continue
        _require(prelaunch_tables[table_name] == exit_tables[table_name], f"fixture data table {table_name} changed during the run")
    prelaunch_settings = _sqlite_app_settings(fixture_snapshot_path)
    exit_settings = _sqlite_app_settings(fixture_copy_path)
    changed_settings = sorted(key for key in prelaunch_settings.keys() | exit_settings.keys() if prelaunch_settings.get(key) != exit_settings.get(key))

    selection_metric_rows = [row for row in navigation_rows if row.get("processInstanceId") == "process-01" and row.get("actionKind") == "class-selection"]
    _require(len(selection_metric_rows) == expected_cycles, "class-selection external snapshots are missing or duplicated")
    for index, row in enumerate(selection_metric_rows, 1):
        _require(row.get("selectedClassId") == "96", f"cycle {index} selected an unexpected class")
        _require(row.get("transitionIndex") is None, "class-selection metric was mislabeled as a page transition")
        _require(row.get("appEvent", {}).get("outcome") == "details-ready", f"cycle {index} class selection did not complete")
        _require(row.get("uiaAction", {}).get("verifiedAppEvent") is True, f"cycle {index} selection lacks an authentic UIA app event")
        _require(int(row.get("uiaAction", {}).get("uiaProcessId", 0)) == expected_pid and int(row.get("uiaAction", {}).get("hwndProcessId", 0)) == expected_pid, f"cycle {index} selection HWND PID differs")
        sample = row.get("sample", {})
        _validate_sample_identity(sample, identity, f"class selection cycle {index}")
        _require(0 <= float(sample.get("sampleDelayMilliseconds", -1)) <= 2_000, f"class selection cycle {index} missed its process sample correlation window")
    route_rows = [row for row in navigation_rows if row.get("processInstanceId") == "process-01" and isinstance(row.get("transitionIndex"), int)]
    _require(len(route_rows) == 2 * expected_cycles, "navigation metrics do not contain every transition")
    expected_routes = [("my_workspace", "sub_prep"), ("sub_prep", "my_workspace")] * expected_cycles
    for index, (row, expected_route) in enumerate(zip(route_rows, expected_routes, strict=True)):
        cycle = index // 2 + 1
        _require(row.get("transitionIndex") == index, "navigation transition indices are not contiguous")
        _require(row.get("cycle") == cycle, "navigation cycle annotation is missing or out of order")
        _require((row.get("fromPageId"), row.get("toPageId")) == expected_route, "navigation route order is invalid")
        _require(row.get("targetAccessibleName") == ("Sub Prep" if expected_route[1] == "sub_prep" else "My Workspace"), "navigation UIA target differs from the route")
        _require(row.get("selectionMethod") in ("uia-verified-pointer-click", "uia-verified-window-message-click"), "navigation used an unaccepted UIA method")
        action = row.get("uiaAction", {})
        _require(action.get("verifiedAppEvent") is True, "UIA action lacks the authentic app event")
        _require(int(action.get("uiaProcessId", 0)) == expected_pid and int(action.get("hwndProcessId", 0)) == expected_pid, "UIA HWND PID differs from measured process")
        _require(int(action.get("appEventLogIndex", -1)) == int(row.get("appEventLogIndex", -2)), "UIA and navigation event indices differ")
        match = ROUTE_RE.fullmatch(str(row.get("appEventLine", "")))
        _require(match is not None, "navigation event line is malformed")
        _require((match.group("from"), match.group("to")) == expected_route and int(match.group("bodies")) == 0, "navigation app event identity or PDF body count is invalid")
        sample = row.get("processSample", {})
        _validate_sample_identity(sample, identity, f"navigation transition {index}")
        _require(0 <= float(row.get("sampleDelayMilliseconds", -1)) <= 2_000, "navigation snapshot missed the app-event correlation window")
    nav_actions = [row for row in uia_rows if row.get("actionKind") == "page-navigation" and row.get("processInstanceId") == "process-01"]
    _require(len(nav_actions) == 2 * len(route_rows), "UIA click trace must retain one attempt and one verified record per transition")
    actions_by_issue: dict[str, list[dict[str, Any]]] = {}
    for action in nav_actions:
        _require(action.get("method") in ("uia-verified-pointer-click", "uia-verified-window-message-click"), "UIA click trace uses an unsupported method")
        _require(int(action.get("uiaProcessId", 0)) == expected_pid and int(action.get("hwndProcessId", 0)) == expected_pid, "UIA action HWND PID differs from measured process")
        issue_id = str(action.get("issuedAtUtc", ""))
        _require(issue_id, "UIA action is missing its issue timestamp")
        actions_by_issue.setdefault(issue_id, []).append(action)
    _require(len(actions_by_issue) == len(route_rows), "UIA click issue identifiers do not map one-to-one to routes")
    for route in route_rows:
        issue_id = str(route.get("uiaAction", {}).get("issuedAtUtc", ""))
        pair = actions_by_issue.get(issue_id, [])
        _require(len(pair) == 2 and sum(action.get("verifiedAppEvent") is True for action in pair) == 1 and sum(action.get("verifiedAppEvent") is False for action in pair) == 1, "UIA route lacks exactly one attempt and one authentic completion record")
        _require(all(action.get("fromPageId") == route.get("fromPageId") and action.get("toPageId") == route.get("toPageId") and action.get("expectedAccessibleName") == route.get("targetAccessibleName") for action in pair), "UIA action pair target differs from navigation metrics")
        verified_action = next(action for action in pair if action.get("verifiedAppEvent") is True)
        _require(int(verified_action.get("appEventLogIndex", -1)) == int(route.get("appEventLogIndex", -2)), "verified UIA completion event index differs from navigation metrics")

    routes_in_log = [row for row in startup_rows if str(row.get("line", "")).startswith("ClassMngrNext navigation event=page-route ")]
    _require(len(routes_in_log) == 2 * expected_cycles + 1, "startup log route count differs from the initial route plus diagnostic transitions")
    for row in routes_in_log:
        line = str(row.get("line", ""))
        _require("loaded_pdf_body_count=0" in line, "startup navigation trace loaded or omitted the PDF body count")
        _require(int(row.get("pid", 0)) == expected_pid, "startup route event belongs to another PID")

    checkpoints = _checkpoint_records(startup_rows)
    page_actions = [row["record"] for row in checkpoints if row["record"].get("event") == "page-action-checkpoint"]
    by_checkpoint: dict[str, list[dict[str, Any]]] = {}
    for record in page_actions:
        by_checkpoint.setdefault(str(record.get("checkpoint")), []).append(record)
    class_selection_events = by_checkpoint.get("class-selection", [])
    _require(len(class_selection_events) == 2 * expected_cycles, "class-selection app events are missing or duplicated")
    _require(all(record.get("outcome") == "details-ready" for record in class_selection_events), "a class-selection event did not complete")
    for cycle in range(expected_cycles):
        warmup = class_selection_events[cycle * 2].get("page", {}).get("sub_prep_runtime_metrics", {})
        selected = class_selection_events[cycle * 2 + 1].get("page", {}).get("sub_prep_runtime_metrics", {})
        _require(warmup.get("selected_class_id") not in (None, "", "96") and selected.get("selected_class_id") == "96", f"cycle {cycle + 1} does not prove a warmup selection followed by class 96")
    _require(len(by_checkpoint.get("explicit-refresh", [])) == expected_cycles, "explicit-refresh app events are missing or duplicated")
    _require(all(record.get("outcome") == "completed" for record in by_checkpoint["explicit-refresh"]), "an explicit refresh failed")
    for checkpoint, delay in (("page-suspended", None), ("post-page-release-snapshot", "1s"), ("post-page-release-snapshot", "5s")):
        matches = [record for record in by_checkpoint.get(checkpoint, []) if delay is None or record.get("details", {}).get("delay") == delay]
        _require(len(matches) == expected_cycles, f"{checkpoint} {delay or ''} lifecycle events are missing or duplicated")
        for record in matches:
            _require(record.get("outcome") == "completed" and record.get("sample_available") is True, f"{checkpoint} lifecycle event is invalid")
            metrics = record.get("page", {}).get("sub_prep_runtime_metrics") or {}
            _require(metrics.get("selected_details_retained") is False and metrics.get("selected_class_id") == ""
                     and metrics.get("summary_class_count") == 0 and metrics.get("teacher_summary_count") == 0
                     and metrics.get("active_grade_row_count") == 0, f"{checkpoint} did not report released Sub Prep state")
    _require(len(by_checkpoint.get("package-generation-started", [])) == 1, "package generation must be initiated exactly once")
    _require(len(by_checkpoint.get("package-generation", [])) == 1 and by_checkpoint["package-generation"][0].get("outcome") == "completed", "package generation did not complete exactly once")
    post_generation = [record for record in by_checkpoint.get("post-generation-snapshot", []) if record.get("outcome") == "completed"]
    _require(sorted(record.get("details", {}).get("delay") for record in post_generation) == ["1s", "5s", "immediate"], "post-generation checkpoints are incomplete or duplicated")

    generated_packages = generated_inventory.get("packages")
    _require(isinstance(generated_packages, list) and len(generated_packages) == 1, "generated output inventory must contain one package")
    generated_package = generated_packages[0]
    generated_documents = generated_package.get("documents", [])
    _require(len(generated_documents) == int(generated_package.get("documentCount", 0)) > 0, "generated output document count is invalid")
    _require(generated_package.get("processInstanceId") == "process-01", "generated output belongs to another process")
    output_root = Path(str(process.get("outputRoot", ""))).resolve()
    generated_root = Path(str(generated_package.get("outputRoot", ""))).resolve()
    generated_directory = Path(str(generated_package.get("outputDirectory", ""))).resolve()
    _require(generated_root == output_root, "generated package output root differs from the process-unique root")
    try:
        generated_directory.relative_to(output_root)
    except ValueError as error:
        raise EvidenceError("generated package output directory escaped its unique root") from error
    subprep_docs = [row for row in generated_documents if row.get("relativePath") == "Sub Prep.pdf"]
    _require(len(subprep_docs) == 1, "generated package must contain exactly one Sub Prep.pdf")
    for document in generated_documents:
        document_path = Path(str(document.get("path", "")))
        _require(document_path.is_file(), "generated PDF is missing")
        _require(document.get("format") == "PDF" and document_path.stat().st_size > 5 and document_path.stat().st_size == int(document.get("sizeBytes", -1)), "generated PDF format or size is invalid")
        _require(hashlib.sha256(document_path.read_bytes()).hexdigest() == document.get("sha256"), "generated PDF SHA-256 changed")
        _require(document_path.read_bytes()[:5] == b"%PDF-", "generated document does not contain a PDF header")
    generation_record = by_checkpoint["package-generation"][0]
    _require(generation_record.get("details", {}).get("documentCount") == len(generated_documents), "app generation record and output inventory disagree")
    generation_started = by_checkpoint["package-generation-started"][0]
    _require(generation_started.get("details", {}).get("requestedClassCount") == 30, "package-generation request did not use the 30-class weekday fixture")

    metrics = [row for row in process_rows if row.get("processInstanceId") == "process-01" and row.get("phase")]
    phases = {phase: [row for row in metrics if row.get("phase") == phase] for phase in {str(row.get("phase")) for row in metrics}}
    for phase, wanted in (("workspace-baseline", 1), ("post-generation-immediate", 1), ("post-generation-1s", 1), ("post-generation-5s", 1),
                          ("page-suspended", expected_cycles), ("page-release-1s", expected_cycles), ("page-release-5s", expected_cycles), ("idle", 301)):
        _require(len(phases.get(phase, [])) == wanted, f"external process metric phase {phase} has {len(phases.get(phase, []))}, expected {wanted}")
    for phase in ("page-suspended", "page-release-1s", "page-release-5s"):
        rows = phases[phase]
        _require([row.get("cycle") for row in rows] == list(range(1, expected_cycles + 1)), f"{phase} cycle checkpoints are missing or out of order")
    for phase, delay in (("post-generation-immediate", "immediate"), ("post-generation-1s", "1s"), ("post-generation-5s", "5s")):
        _validate_sample_identity(phases[phase][0], identity, phase)
        _require(phases[phase][0].get("cycle") == 1, f"{phase} is not associated with generation cycle 1")
        _require(phases[phase][0].get("appCheckpoint", {}).get("checkpoint") == "post-generation-snapshot", f"{phase} does not correlate to the generation checkpoint")
        _require(phases[phase][0].get("appCheckpoint", {}).get("details", {}).get("delay") == delay, f"{phase} delay label differs from app event")
    external_samples: list[dict[str, Any]] = []
    for phase_rows in phases.values():
        for sample in phase_rows:
            _validate_sample_identity(sample, identity, str(sample.get("phase")))
            if sample.get("phase") != "idle":
                _require(0 <= float(sample.get("sampleDelayMilliseconds", -1)) <= 2_000, f"{sample.get('phase')} sample missed the app-event correlation window")
            external_samples.append(sample)

    idle_rows = phases["idle"]
    _require([row.get("sampleIndex") for row in idle_rows] == list(range(301)), "idle sample indices are incomplete")
    idle_times = [_timestamp(row.get("timestampUtc"), "idle sample") for row in idle_rows]
    intervals = [(right - left).total_seconds() for left, right in zip(idle_times, idle_times[1:])]
    _require(all(0.75 <= interval <= 1.5 for interval in intervals), "idle sampling cadence is outside 0.75–1.5 seconds")
    _require((idle_times[-1] - idle_times[0]).total_seconds() >= 300, "idle interval is shorter than five minutes")
    idle_ws = [int(row["currentWorkingSetBytes"]) for row in idle_rows]
    idle_private = [int(row["privateBytes"]) for row in idle_rows]
    return_rows = [row for row in route_rows if row.get("toPageId") == "my_workspace"]
    cycle_rows = []
    for cycle in range(1, expected_cycles + 1):
        return_sample = next(row["processSample"] for row in return_rows if row.get("cycle") == cycle)
        release_sample = next(row for row in phases["page-suspended"] if row.get("cycle") == cycle)
        release1_sample = next(row for row in phases["page-release-1s"] if row.get("cycle") == cycle)
        release5_sample = next(row for row in phases["page-release-5s"] if row.get("cycle") == cycle)
        _require(release_sample.get("appCheckpoint", {}).get("checkpoint") == "page-suspended", f"cycle {cycle} process sample does not match page-suspended")
        _require(release1_sample.get("appCheckpoint", {}).get("checkpoint") == "post-page-release-snapshot" and release1_sample.get("appCheckpoint", {}).get("details", {}).get("delay") == "1s", f"cycle {cycle} 1s sample does not match app checkpoint")
        _require(release5_sample.get("appCheckpoint", {}).get("checkpoint") == "post-page-release-snapshot" and release5_sample.get("appCheckpoint", {}).get("details", {}).get("delay") == "5s", f"cycle {cycle} 5s sample does not match app checkpoint")
        selection_record = class_selection_events[(cycle - 1) * 2 + 1]
        selection_snapshot = selection_metric_rows[cycle - 1]["sample"]
        refresh_record = by_checkpoint["explicit-refresh"][cycle - 1]
        selection_metrics = selection_record.get("page", {}).get("sub_prep_runtime_metrics", {})
        suspended_metrics = release_sample.get("appCheckpoint", {}).get("page", {}).get("sub_prep_runtime_metrics", {})
        activation = process.get("summaryLifecycleSnapshots", [])[cycle - 1]
        cycle_rows.append({
            "cycle": cycle,
            "activationLifecycleSnapshot": activation.get("pageLifecycleSnapshot"),
            "classSelection": {
                "outcome": selection_record.get("outcome"),
                "selectedClassId": selection_metrics.get("selected_class_id"),
                "summaryClassCount": selection_metrics.get("summary_class_count"),
                "teacherSummaryCount": selection_metrics.get("teacher_summary_count"),
                "activeGradeRowCount": selection_metrics.get("active_grade_row_count"),
                "selectedDetailsRetained": selection_metrics.get("selected_details_retained"),
                "selectedDetailNoteEditorCount": selection_metrics.get("selected_detail_note_editor_count"),
                "preparationEditorCount": selection_metrics.get("preparation_editor_count"),
                "processSnapshot": {"workingSetBytes": selection_snapshot["currentWorkingSetBytes"], "privateBytes": selection_snapshot["privateBytes"], "handles": selection_snapshot["handles"], "threads": selection_snapshot["threads"]},
            },
            "refreshWidgetCount": refresh_record.get("page", {}).get("widget_count"),
            "suspendedLifecycleCounters": suspended_metrics,
            "returnToWorkspace": {"workingSetBytes": return_sample["currentWorkingSetBytes"], "privateBytes": return_sample["privateBytes"], "handles": return_sample["handles"], "threads": return_sample["threads"]},
            "pageSuspended": {"workingSetBytes": release_sample["currentWorkingSetBytes"], "privateBytes": release_sample["privateBytes"], "handles": release_sample["handles"], "threads": release_sample["threads"]},
            "postRelease1s": {"workingSetBytes": release1_sample["currentWorkingSetBytes"], "privateBytes": release1_sample["privateBytes"], "handles": release1_sample["handles"], "threads": release1_sample["threads"]},
            "postRelease5s": {"workingSetBytes": release5_sample["currentWorkingSetBytes"], "privateBytes": release5_sample["privateBytes"], "handles": release5_sample["handles"], "threads": release5_sample["threads"]},
        })
    cycle_trends: dict[str, Any] = {}
    for checkpoint_name in ("returnToWorkspace", "pageSuspended", "postRelease5s"):
        for metric_name in ("workingSetBytes", "privateBytes", "handles", "threads"):
            values = [int(row[checkpoint_name][metric_name]) for row in cycle_rows]
            late_values = values[-10:]
            cycle_trends[f"{checkpoint_name}.{metric_name}"] = {
                "cycle1": values[0],
                "cycleLast": values[-1],
                "totalDelta": values[-1] - values[0],
                "lateTenStart": late_values[0],
                "lateTenEnd": late_values[-1],
                "lateTenDelta": late_values[-1] - late_values[0],
                "lateTenRange": max(late_values) - min(late_values),
            }
    # Keep the source files physically bound to the captured inventory. This
    # check is deliberately after fixture and trace checks so a historical
    # source drift does not hide whether the preserved fixture evidence itself
    # reconciles, while still making any drift invalidate the whole trace.
    _verify_inventory_files(source_files, repository_root, "source inventory")
    summary = {
        "schema": SUMMARY_SCHEMA,
        "taskId": "P9_08_HEAP_RETENTION_DIAGNOSTIC",
        "runId": manifest["runId"],
        "validationStatus": "valid-diagnostic",
        "formalMemoryGate": "not-evaluated",
        "interpretation": "Finite trace only; it does not establish a leak or a plateau.",
        "sourceSha256": source_sha,
        "packageSha256": package_sha,
        "applicationSha256": app_sha,
        "fixtureSha256": fixture.get("sha256"),
        "fixtureSha256PassedToApp": process.get("fixtureSha256PassedToApp"),
        "fixtureSha256AtExit": process.get("fixtureHashAtExit"),
        "fixturePhysicalHashes": fixture_hashes,
        "fixtureRuntimeMutation": {"onlyAppSettingsChanged": True, "changedKeys": changed_settings, "classTeacherRosterTablesUnchanged": True},
        "pid": expected_pid,
        "architecture": "x64",
        "cycleCount": expected_cycles,
        "packageGenerationCount": 1,
        "authenticNavigationTransitionCount": len(route_rows),
        "classSelectionCount": len(selection_metric_rows),
        "classSelectionCheckpointEventCount": len(class_selection_events),
        "refreshCount": len(by_checkpoint["explicit-refresh"]),
        "suspensionCount": len(by_checkpoint["page-suspended"]),
        "idleSampleCount": len(idle_rows),
        "idleIntervalSeconds": round((idle_times[-1] - idle_times[0]).total_seconds(), 3),
        "idleWorkingSetMaximumBytes": max(idle_ws),
        "idlePrivateBytesMedian": int(statistics.median(idle_private)),
        "cycles": cycle_rows,
        "perCycleTrends": cycle_trends,
        "externalSnapshotCount": len(external_samples) + len(route_rows) + len(selection_metric_rows),
        "outputReleaseProof": "not-established; generated PDF identity is retained, but the app exposes no independent output-resource release counter",
    }
    return summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-directory", type=Path, required=True)
    parser.add_argument("--expected-cycles", type=int, default=30)
    parser.add_argument("--summary-output", type=Path, help="write a derived summary without replacing the run's summary.json")
    args = parser.parse_args(argv)
    default_summary_path = args.run_directory.resolve() / "summary.json"
    summary_path = args.summary_output.resolve() if args.summary_output else default_summary_path
    if args.summary_output and summary_path == default_summary_path:
        parser.error("--summary-output must not overwrite the original summary.json")
    if args.summary_output and summary_path.exists():
        parser.error(f"refusing to overwrite derived summary: {summary_path}")
    try:
        summary = validate_run_directory(args.run_directory, args.expected_cycles)
    except EvidenceError as error:
        summary = {
            "schema": SUMMARY_SCHEMA,
            "taskId": "P9_08_HEAP_RETENTION_DIAGNOSTIC",
            "runId": _read_json(args.run_directory / "manifest.json").get("runId"),
            "validationStatus": "invalid-diagnostic",
            "formalMemoryGate": "not-evaluated",
            "error": str(error),
        }
        summary_path.parent.mkdir(parents=True, exist_ok=True)
        summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        print(f"Invalid diagnostic evidence: {error}")
        return 1
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"Valid diagnostic trace: {summary['cycleCount']} cycles; formal memory gate not evaluated.")
    print(f"Cycle return-to-Workspace private range: {min(row['returnToWorkspace']['privateBytes'] for row in summary['cycles'])}..{max(row['returnToWorkspace']['privateBytes'] for row in summary['cycles'])} bytes.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
