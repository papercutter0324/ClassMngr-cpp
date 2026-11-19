#!/usr/bin/env python3
"""Fail-closed P9-09 packaged Next Schedule Import evidence validator."""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import re
import struct
import sys
from datetime import datetime
from pathlib import Path
from typing import Any


MANIFEST_SCHEMA = "classmngr-phase9-packaged-next-schedule-import-v1"
SUMMARY_SCHEMA = "classmngr-phase9-packaged-next-schedule-import-summary-v1"
TRACE_SCHEMA = "classmngr-schedule-import-diagnostic-v1"
SNAPSHOT_SCHEMA = "classmngr-phase9-schedule-import-workspace-snapshot-v1"
WORKING_SET_LIMIT = 262_144_000
PEAK_ADVISORY_LIMIT = 536_870_912
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


class EvidenceError(ValueError):
    """Raised when evidence is missing, inconsistent, or out of contract."""


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise EvidenceError(message)


def _read_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as error:
        raise EvidenceError(f"cannot read {path.name}: {error}") from error


def _read_jsonl(path: Path) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8-sig") as stream:
            for line_number, line in enumerate(stream, 1):
                _require(bool(line.strip()), f"{path.name} has an empty row at {line_number}")
                try:
                    value = json.loads(line)
                except json.JSONDecodeError as error:
                    raise EvidenceError(f"{path.name} row {line_number} is malformed JSON") from error
                _require(isinstance(value, dict), f"{path.name} row {line_number} is not an object")
                result.append(value)
    except OSError as error:
        raise EvidenceError(f"cannot read {path.name}: {error}") from error
    return result


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    except OSError as error:
        raise EvidenceError(f"cannot hash {path}: {error}") from error
    return digest.hexdigest()


def _canonical(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def _inventory_hash(rows: list[dict[str, Any]], label: str) -> str:
    normalized: list[tuple[str, int, str]] = []
    seen: set[str] = set()
    for row in rows:
        relative = str(row.get("path", ""))
        size = row.get("sizeBytes")
        digest = str(row.get("sha256", ""))
        _require(relative and isinstance(size, int) and size >= 0, f"{label} contains an invalid path/size")
        _require(SHA256_RE.fullmatch(digest) is not None, f"{label} contains an invalid SHA-256")
        key = relative.casefold()
        _require(key not in seen, f"{label} duplicates path {relative}")
        seen.add(key)
        normalized.append((relative, size, digest))
    normalized.sort(key=lambda item: item[0].casefold())
    canonical = "".join(f"{path}\t{size}\t{digest}\n" for path, size, digest in normalized)
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def _verify_inventory_files(rows: list[dict[str, Any]], root: Path, label: str) -> None:
    canonical_root = root.resolve(strict=True)
    for row in rows:
        relative = str(row["path"]).replace("\\", "/")
        target = root.joinpath(*relative.split("/"))
        try:
            target.resolve(strict=True).relative_to(canonical_root)
        except (ValueError, OSError) as error:
            raise EvidenceError(f"{label} path escapes or is missing under its root: {relative}") from error
        _require(target.is_file(), f"{label} file is missing: {relative}")
        _require(target.stat().st_size == int(row["sizeBytes"]), f"{label} size changed: {relative}")
        _require(_sha256(target) == row["sha256"], f"{label} SHA-256 changed: {relative}")


def _artifact_path(value: Any, run_directory: Path, label: str, *, must_exist: bool = True) -> Path:
    path = Path(str(value)).resolve()
    try:
        path.relative_to(run_directory.resolve())
    except ValueError as error:
        raise EvidenceError(f"{label} is outside the unique evidence directory") from error
    if must_exist:
        _require(path.is_file(), f"{label} is missing: {path}")
    return path


def _timestamp(value: Any, label: str) -> datetime:
    _require(isinstance(value, str) and value, f"{label} timestamp is missing")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise EvidenceError(f"{label} timestamp is malformed") from error
    _require(parsed.tzinfo is not None, f"{label} timestamp has no timezone")
    return parsed


def _same_path(left: Any, right: Any) -> bool:
    try:
        return Path(str(left)).resolve() == Path(str(right)).resolve()
    except (OSError, RuntimeError, ValueError):
        return False


def _table(snapshot: dict[str, Any], table_name: str) -> tuple[dict[str, int], list[list[Any]]]:
    tables = snapshot.get("tables")
    _require(isinstance(tables, dict) and table_name in tables, f"workspace snapshot omits table {table_name}")
    value = tables[table_name]
    schema = value.get("schema")
    rows = value.get("rows")
    _require(isinstance(schema, list) and isinstance(rows, list), f"workspace table {table_name} snapshot is malformed")
    names: dict[str, int] = {}
    for column in schema:
        _require(isinstance(column, list) and len(column) >= 2, f"workspace table {table_name} has malformed schema metadata")
        names[str(column[1])] = int(column[0])
    _require(int(value.get("rowCount", -1)) == len(rows), f"workspace table {table_name} row count disagrees with its row data")
    return names, rows


def _row_as_dict(columns: dict[str, int], row: list[Any]) -> dict[str, Any]:
    return {name: row[index] for name, index in columns.items() if index < len(row)}


def _verify_snapshot(snapshot: dict[str, Any], label: str) -> None:
    _require(snapshot.get("schema") == SNAPSHOT_SCHEMA, f"{label} has the wrong snapshot schema")
    _require(snapshot.get("integrityCheck") == "ok", f"{label} failed SQLite integrity_check")
    tables = snapshot.get("tables")
    _require(isinstance(tables, dict) and int(snapshot.get("tableCount", -1)) == len(tables), f"{label} table inventory is malformed")
    normalized: dict[str, dict[str, Any]] = {}
    for name, value in tables.items():
        schema = value.get("schema")
        rows = value.get("rows")
        _require(isinstance(schema, list) and isinstance(rows, list), f"{label} table {name} is malformed")
        _require(int(value.get("rowCount", -1)) == len(rows), f"{label} table {name} row count mismatch")
        contents = {"schema": schema, "rows": rows}
        _require(hashlib.sha256(_canonical(contents)).hexdigest() == value.get("contentSha256"), f"{label} table {name} content hash mismatch")
        normalized[name] = contents
    digest = hashlib.sha256(_canonical(normalized)).hexdigest()
    _require(digest == snapshot.get("tableContentSha256"), f"{label} full table-content hash mismatch")
    file_digest = str(snapshot.get("workspaceFileSha256", ""))
    _require(SHA256_RE.fullmatch(file_digest) is not None, f"{label} workspace file SHA-256 is missing")
    _require(int(snapshot.get("workspaceFileSizeBytes", 0)) > 0, f"{label} workspace file size is missing")


def _trace_event(route: dict[str, Any], name: str, *, required: bool = True) -> dict[str, Any] | None:
    events = route.get("dialogTrace", [])
    _require(isinstance(events, list), f"{route.get('routeId')} dialog trace is malformed")
    matches = [event for event in events if event.get("event") == name]
    if not required and not matches:
        return None
    _require(len(matches) == 1, f"{route.get('routeId')} expected one {name} trace event; found {len(matches)}")
    event = matches[0]
    _require(event.get("schema") == TRACE_SCHEMA, f"{route.get('routeId')} {name} trace schema mismatch")
    _timestamp(event.get("timestampUtc"), f"{route.get('routeId')} {name}")
    return event


def _validate_trace_roundtrip(route: dict[str, Any], run_directory: Path) -> list[dict[str, Any]]:
    trace_path = _artifact_path(route.get("tracePath"), run_directory, f"{route.get('routeId')} raw UTF-8 dialog trace")
    _require(_sha256(trace_path) == route.get("traceSha256"), f"{route.get('routeId')} raw dialog trace SHA-256 mismatch")
    raw_events = _read_jsonl(trace_path)
    manifest_events = route.get("dialogTrace")
    _require(isinstance(manifest_events, list) and manifest_events == raw_events, f"{route.get('routeId')} manifest dialog trace differs from the raw UTF-8 trace")
    process_id = int(route.get("processIdentity", {}).get("pid", -1))
    _require(process_id > 0 and all(int(event.get("processId", -1)) == process_id for event in raw_events), f"{route.get('routeId')} raw dialog trace contains a foreign process identity")
    return raw_events


def _validate_manifest_request_matches_raw_trace(manifest_request: dict[str, Any], raw_events: list[dict[str, Any]]) -> None:
    apply_requests = [event for event in raw_events if event.get("event") == "apply-request"]
    _require(len(apply_requests) == 1, f"expected exactly one raw apply-request trace; found {len(apply_requests)}")
    _require(manifest_request == apply_requests[0], "manifest apply request differs from the raw UTF-8 apply trace")


def _validate_released_data(route: dict[str, Any], expected_outcome: str, fixture_path: str) -> None:
    released = _trace_event(route, "retained-data-released")
    _require(released is not None and released.get("outcome") == expected_outcome, f"{route['routeId']} retained-data outcome is wrong")
    before = released.get("before", {})
    after = released.get("after", {})
    _require(isinstance(before, dict) and isinstance(after, dict), f"{route['routeId']} retained-data snapshots are malformed")
    _require(_same_path(before.get("sourceSelection"), fixture_path) and _same_path(before.get("sourceFilePath"), fixture_path), f"{route['routeId']} source selection was not captured before cleanup")
    _require(before.get("selectedSheet") and before.get("selectedBlock"), f"{route['routeId']} sheet/block selection was missing before cleanup")
    _require(
        (int(before.get("candidateRows", -1)), int(before.get("teacherRows", -1)), int(before.get("previewRows", -1))) == (20, 5, 20),
        f"{route['routeId']} retained review models did not contain 20/5/20 rows before cleanup",
    )
    _require(int(before.get("sheetCount", 0)) > 0 and int(before.get("blockCount", 0)) > 0, f"{route['routeId']} selection choices were not retained before cleanup")
    _require(
        after.get("sourceSelection") == "" and after.get("sourceFilePath") == ""
        and int(after.get("sheetCount", -1)) == 0 and int(after.get("blockCount", -1)) == 0
        and after.get("selectedSheet") == "" and after.get("selectedBlock") == ""
        and (int(after.get("candidateRows", -1)), int(after.get("teacherRows", -1)), int(after.get("previewRows", -1))) == (0, 0, 0),
        f"{route['routeId']} retained source, selection, or review model data after close",
    )
    _require(int(before.get("reviewWidgetCount", 0)) < 200, f"{route['routeId']} review widget count is not bounded")


def _validate_class_associations(snapshot: dict[str, Any], request: dict[str, Any]) -> dict[str, Any]:
    candidates = request.get("candidates")
    teachers = request.get("teacherDecisions")
    classes = request.get("classDecisions")
    _require(isinstance(candidates, list) and isinstance(teachers, list) and isinstance(classes, list), "apply request candidate/decision rows are malformed")
    _require(len(candidates) == 20 and len(classes) == 20 and len(teachers) == 5, "apply request must contain 20 candidates, 20 class decisions, and five teachers")
    _require(int(request.get("candidateCount", -1)) == 20 and int(request.get("createdTeacherCount", -1)) == 5 and int(request.get("createdClassCount", -1)) == 20, "apply request creation totals differ from 20/5/20")
    _require(all(row.get("actionName") == "create" for row in teachers), "not every distinct imported teacher was set to Create")
    _require(all(row.get("actionName") == "create-new" for row in classes), "not every imported class was set to Create new")
    _require(len({str(row.get("teacherKey", "")) for row in teachers}) == 5, "teacher decisions do not contain five unique teacher keys")
    _require({str(row.get("teacherKey", "")) for row in teachers} == {str(row.get("teacherKey", "")) for row in candidates}, "teacher decisions do not cover every candidate teacher key")
    _require(sorted(int(row.get("candidateIndex", -1)) for row in classes) == list(range(20)), "class decisions do not cover each candidate exactly once")

    expected: collections.Counter[tuple[str, str, str, str, str, str]] = collections.Counter()
    expected_teachers: set[str] = set()
    for candidate in candidates:
        teacher = str(candidate.get("teacherName", ""))
        grade = str(candidate.get("grade", ""))
        level = str(candidate.get("level", ""))
        times = candidate.get("times")
        _require(teacher and grade and level and isinstance(times, list) and times, "apply request candidate has an empty identity or schedule")
        expected_teachers.add(teacher)
        for time in times:
            expected[(teacher, grade, level, str(time.get("day", "")), str(time.get("startTime", "")), str(time.get("endTime", "")))] += 1
    _require(len(expected_teachers) == 5, "20 apply candidates do not map to five distinct imported teacher names")
    _require(sum(expected.values()) == 30, "request candidate schedules do not total 30 class-time rows")

    class_columns, class_rows = _table(snapshot, "classes")
    teacher_columns, teacher_rows = _table(snapshot, "teachers")
    info_columns, info_rows = _table(snapshot, "class_info")
    time_columns, time_rows = _table(snapshot, "class_times")
    _require((len(class_rows), len(teacher_rows), len(info_rows), len(time_rows)) == (20, 5, 20, 30), "persisted table totals differ from 20 classes, 5 teachers, 20 class_info, and 30 class_times rows")
    classes_by_id = {str(_row_as_dict(class_columns, row).get("id")): _row_as_dict(class_columns, row) for row in class_rows}
    teachers_by_id = {str(_row_as_dict(teacher_columns, row).get("id")): _row_as_dict(teacher_columns, row) for row in teacher_rows}
    times_by_class: dict[str, list[dict[str, Any]]] = collections.defaultdict(list)
    for row in time_rows:
        item = _row_as_dict(time_columns, row)
        times_by_class[str(item.get("class_id"))].append(item)
    actual: collections.Counter[tuple[str, str, str, str, str, str]] = collections.Counter()
    persisted_class_identities: list[dict[str, str]] = []
    for row in info_rows:
        info = _row_as_dict(info_columns, row)
        class_id = str(info.get("class_id"))
        class_row = classes_by_id.get(class_id)
        teacher_row = teachers_by_id.get(str(info.get("teacher_id")))
        _require(class_row is not None and teacher_row is not None, "persisted class_info row refers to a missing class or teacher")
        teacher_name = str(teacher_row.get("teacher_kr", ""))
        grade = str(info.get("class_grade", ""))
        level = str(info.get("class_level", ""))
        persisted_class_identities.append({"classId": class_id, "className": str(class_row.get("name", "")), "teacherName": teacher_name, "grade": grade, "level": level})
        rows = times_by_class.get(class_id, [])
        _require(rows, f"persisted imported class {class_id} has no schedule rows")
        for time in rows:
            actual[(teacher_name, grade, level, str(time.get("day", "")), str(time.get("start_time", "")), str(time.get("end_time", "")))] += 1
    _require(set(teachers_by_id.keys()) == {str(_row_as_dict(teacher_columns, row).get("id")) for row in teacher_rows}, "teacher IDs are not unique")
    actual_teacher_names = {str(_row_as_dict(teacher_columns, row).get("teacher_kr", "")) for row in teacher_rows}
    _require(actual_teacher_names == expected_teachers, "persisted teacher names differ from the five imported source teachers")
    _require(actual == expected, "persisted class/teacher/day/time associations differ from the exact apply request")
    return {
        "candidateCount": len(candidates),
        "createdClassCount": len(class_rows),
        "createdTeacherCount": len(teacher_rows),
        "classInfoRowCount": len(info_rows),
        "classTimeRowCount": len(time_rows),
        "expectedAssociationCount": sum(expected.values()),
        "actualAssociationCount": sum(actual.values()),
        "associationMultisetSha256": hashlib.sha256(_canonical([[*row, count] for row, count in sorted(expected.items())])).hexdigest(),
        "persistedClassIdentities": persisted_class_identities,
    }


def _validate_route_cleanup(route: dict[str, Any], run_directory: Path) -> None:
    cleanup = route.get("cleanup", {})
    _require(cleanup.get("removed") is True and route.get("workspace", {}).get("removedAfterEvidenceCapture") is True, f"{route['routeId']} runtime/workspace cleanup was not recorded")
    runtime = Path(str(cleanup.get("runtimeDirectory", ""))).resolve()
    try:
        runtime.relative_to(run_directory.resolve() / "runtime")
    except ValueError as error:
        raise EvidenceError(f"{route['routeId']} cleanup path escaped the run runtime directory") from error
    _require(not runtime.exists(), f"{route['routeId']} temporary runtime directory still exists")
    workspace_path = Path(str(route.get("workspace", {}).get("path", ""))).resolve()
    try:
        workspace_path.relative_to(runtime)
    except ValueError as error:
        raise EvidenceError(f"{route['routeId']} workspace is outside the cleaned route directory") from error
    removed_paths = cleanup.get("removedPaths")
    _require(isinstance(removed_paths, list) and any(str(path).lower().endswith(".tps") for path in removed_paths), f"{route['routeId']} cleanup manifest omits its workspace file")
    _require(str(workspace_path).casefold() in {str(Path(path).resolve()).casefold() for path in removed_paths}, f"{route['routeId']} cleanup manifest omits its workspace path")
    for path in removed_paths:
        removed = Path(str(path)).resolve()
        try:
            removed.relative_to(runtime)
        except ValueError as error:
            raise EvidenceError(f"{route['routeId']} cleanup list escapes its runtime directory") from error
    _require(cleanup.get("workbookEvidenceCopyPreserved") is True, f"{route['routeId']} fixture evidence copy was not preserved")


def _validate_uia_actions(actions: list[dict[str, Any]], routes: dict[str, dict[str, Any]]) -> dict[str, int]:
    counts: dict[str, int] = collections.Counter()
    for row in actions:
        route_id = str(row.get("routeId", ""))
        _require(route_id in routes, "UIA action refers to an unknown route")
        route = routes[route_id]
        pid = int(route["processIdentity"]["pid"])
        _require(row.get("processInstanceId") == route_id, f"UIA action has wrong process instance: {row.get('actionKind')}")
        _require(int(row.get("applicationProcessId", row.get("elementProcessId", -1))) == pid, f"UIA action application PID mismatch: {row.get('actionKind')}")
        method = str(row.get("method", ""))
        _require(method.startswith("uia-"), f"UIA action did not use UIA-bound interaction: {row.get('actionKind')}")
        if row.get("actionKind") == "choose-workbook-file":
            _require(int(row.get("ownerProcessId", -1)) == pid, "file picker owner HWND is not bound to the measured app PID")
            _require(int(row.get("pickerWindowHandle", 0)) > 0 and int(row.get("ownerWindowHandle", 0)) > 0, "file picker window/owner HWND is missing")
        elif row.get("actionKind") == "verify-workbook-selection":
            _require(int(row.get("elementProcessId", -1)) == pid, "workbook source selection element belongs to another process")
            _require(int(row.get("applicationProcessId", -1)) == pid, "workbook source selection action application PID mismatch")
            _require(Path(str(row.get("expectedPath", ""))).resolve() == Path(str(row.get("parseTraceSourcePath", ""))).resolve(), "UIA-selected workbook path differs from the workflow parse trace")
        elif row.get("actionKind") in {"set-review-choice", "verify-review-choice"}:
            _require(int(row.get("elementProcessId", -1)) == pid, "review choice action element belongs to another process")
            if row.get("actionKind") == "verify-review-choice":
                _require(row.get("observedChoice") == row.get("choice") and bool(row.get("choice")), "UIA observed review choice differs from the required selection")
                _require(int(row.get("windowProcessId", -1)) == pid and int(row.get("windowHandle", 0)) > 0, "review choice verification is not bound to the measured app window")
        else:
            _require(int(row.get("elementProcessId", -1)) == pid, f"UIA target belongs to another process: {row.get('actionKind')}")
            _require(int(row.get("windowProcessId", -1)) == pid, f"UIA action HWND belongs to another process: {row.get('actionKind')}")
            _require(int(row.get("windowHandle", 0)) > 0, f"UIA action lacks HWND: {row.get('actionKind')}")
        _timestamp(row.get("issuedAtUtc"), f"UIA action {row.get('actionKind')}")
        counts[f"{route_id}:{row.get('actionKind')}"] += 1
    return dict(counts)


def _validate_memory(route: dict[str, Any]) -> dict[str, Any]:
    points = route.get("memory", {}).get("normalUseSamples", [])
    _require(isinstance(points, list) and len(points) >= 4, f"{route['routeId']} is missing startup, settled, post-cleanup, and idle-tail memory points")
    violations: list[dict[str, Any]] = []
    point_names = {str(row.get("point")) for row in points}
    for required in ("startup-ready-checkpoint", "startup-ready-external", "startup-settled", "post-cleanup", "post-cleanup-idle-tail"):
        _require(required in point_names, f"{route['routeId']} is missing normal-use memory point {required}")
    for sample in points:
        value = sample.get("workingSetBytes")
        _require(isinstance(value, int) and value > 0, f"{route['routeId']} memory point has invalid working set")
        for key in ("privateBytes", "commitBytes"):
            _require(isinstance(sample.get(key), int) and int(sample[key]) > 0, f"{route['routeId']} memory point has invalid {key}")
        _timestamp(sample.get("timestampUtc"), f"{route['routeId']} {sample.get('point')} memory")
        if value >= WORKING_SET_LIMIT:
            violations.append({"point": sample.get("point"), "sampleIndex": sample.get("sampleIndex", 0), "workingSetBytes": value, "exclusiveThresholdBytes": WORKING_SET_LIMIT})
    operation_samples = route.get("memory", {}).get("operationSamples", [])
    if not isinstance(operation_samples, list):
        operation_samples = []
    peak_rows = []
    for sample in operation_samples:
        peak = sample.get("peakWorkingSetBytes")
        _require(isinstance(peak, int) and peak > 0, f"{route['routeId']} operation sample has no peak working set")
        peak_rows.append({"point": sample.get("point"), "workingSetBytes": sample.get("currentWorkingSetBytes"), "peakWorkingSetBytes": peak, "underAdvisoryCeiling": peak < PEAK_ADVISORY_LIMIT})
    return {"normalUseSamples": len(points), "strictWorkingSetViolations": violations, "operationPeaks": peak_rows, "budgetPassed": not violations}


def validate(evidence_directory: Path) -> dict[str, Any]:
    run_directory = evidence_directory.resolve(strict=True)
    manifest = _read_json(run_directory / "manifest.json")
    _require(manifest.get("schema") == MANIFEST_SCHEMA and manifest.get("taskId") == "P9_09_PACKAGED_SCHEDULE_IMPORT", "manifest schema/task identity mismatch")
    _require(manifest.get("status") in {"evidence-collected", "passed"}, "manifest is not a complete evidence-collected run")
    _require(manifest.get("budgetEvaluation") in {"pending-independent-validation", "evaluated-by-independent-validator"}, "invalid attempt cannot be treated as a memory budget outcome")
    source_document = _read_json(run_directory / "source-files.json")
    package_document = _read_json(run_directory / "package-files.json")
    source_rows = source_document.get("files"); package_rows = package_document.get("files")
    _require(isinstance(source_rows, list) and isinstance(package_rows, list), "source/package inventory is malformed")
    source_root_value = manifest.get("source", {}).get("root") or manifest.get("repositoryRoot")
    _require(bool(source_root_value), "candidate source root is missing")
    source_root = Path(str(source_root_value))
    _require(source_root.resolve(strict=True) == Path(str(manifest.get("repositoryRoot", ""))).resolve(strict=True), "source root differs from the recorded repository root")
    _require(_inventory_hash(source_rows, "source inventory") == manifest.get("source", {}).get("sha256"), "source inventory canonical hash differs from manifest")
    _verify_inventory_files(source_rows, source_root, "candidate source")
    _require(_inventory_hash(package_rows, "package inventory") == manifest.get("package", {}).get("sha256"), "package inventory canonical hash differs from manifest")
    package_root = Path(str(manifest.get("package", {}).get("directory", "")))
    _verify_inventory_files(package_rows, package_root, "packaged application")
    app_path = Path(str(manifest.get("application", {}).get("canonicalPath", ""))).resolve(strict=True)
    _require(app_path == (package_root / "ClassMngrNext.exe").resolve(strict=True), "application path is not the staged Next package executable")
    _require(_sha256(app_path) == manifest.get("application", {}).get("sha256") == manifest.get("package", {}).get("applicationSha256"), "application SHA-256 differs from package identities")
    _require(manifest.get("application", {}).get("architecture") == "x64" and _pe_architecture(app_path) == "x64", "staged app is not a native Windows x64 executable")
    build = manifest.get("build", {})
    _require(build.get("succeeded") is True and int(build.get("exitCode", -1)) == 0, "current-source Release build did not succeed")
    _require(set(build.get("targets", [])) == {"ClassMngrNext", "ClassMngrNextNormalScheduleImportIntegrationTests", "ClassMngrPhase7NextBlankFixtureTests"}, "build target list is incomplete")
    build_log = Path(str(build.get("logPath", ""))).resolve(strict=True)
    _require(_sha256(build_log) == build.get("logSha256"), "Release build log hash mismatch")
    _require(_sha256(Path(str(build.get("cachePath", ""))).resolve(strict=True)) == build.get("cacheSha256After"), "Release CMakeCache changed after its recorded hash")

    fixtures = manifest.get("fixtures")
    _require(isinstance(fixtures, list) and len(fixtures) == 2, "manifest does not contain both exact schedule workbook fixtures")
    fixture_by_name = {str(item.get("name")): item for item in fixtures}
    _require(set(fixture_by_name) == {"generated-large-schedule-import.xlsx", "generated-large-schedule-import-applicable.xlsx"}, "schedule fixture names differ from the accepted pair")
    for item in fixtures:
        source_path = Path(str(item.get("sourcePath", ""))).resolve(strict=True)
        evidence_path = _artifact_path(item.get("evidencePath"), run_directory, f"{item.get('name')} evidence workbook")
        _require(_sha256(source_path) == item.get("sourceSha256") == _sha256(evidence_path) == item.get("sha256"), f"workbook source/copy hash mismatch: {item.get('name')}")
        _require(evidence_path.stat().st_size == int(item.get("sizeBytes", -1)), f"workbook evidence size mismatch: {item.get('name')}")

    routes = manifest.get("routes")
    _require(isinstance(routes, list) and len(routes) == 2, "manifest must contain exactly two packaged routes")
    route_by_id = {str(route.get("routeId")): route for route in routes}
    _require(set(route_by_id) == {"route-a-conflict-cancel", "route-b-applicable-apply"}, "route IDs or fresh process count are wrong")
    pids = [int(route["processIdentity"]["pid"]) for route in routes]
    _require(len(set(pids)) == 2, "Route B did not use a fresh process")
    workspace_paths = [str(Path(str(route.get("workspace", {}).get("path", ""))).resolve()) for route in routes]
    _require(len(set(workspace_paths)) == 2, "Route B did not use a fresh workspace")
    action_rows = _read_jsonl(run_directory / "uia-actions.jsonl")
    action_counts = _validate_uia_actions(action_rows, route_by_id)
    summary_routes: dict[str, Any] = {}
    memory_results: list[dict[str, Any]] = []
    for route_id, route in route_by_id.items():
        fixture = route.get("fixture", {})
        fixture_path = str(Path(str(fixture.get("evidencePath", ""))).resolve())
        expected_fixture = fixture_by_name["generated-large-schedule-import.xlsx" if route_id.startswith("route-a") else "generated-large-schedule-import-applicable.xlsx"]
        _require(fixture.get("sha256") == expected_fixture.get("sha256") and fixture_path == str(Path(str(expected_fixture.get("evidencePath"))).resolve()), f"{route_id} workbook identity differs from expected fixture")
        identity = route.get("processIdentity", {})
        _require(int(identity.get("pid", 0)) > 0 and identity.get("architecture") == "x64", f"{route_id} process identity is incomplete")
        _require(Path(str(identity.get("canonicalExecutablePath", ""))).resolve() == app_path and identity.get("sha256") == _sha256(app_path), f"{route_id} process did not use the current staged app")
        _require(int(identity.get("mainWindowHandle", 0)) > 0 and "ClassMngr Next" in str(identity.get("mainWindowTitle", "")), f"{route_id} main window identity is incomplete")
        _require(int(route.get("processExitCode", -1)) == 0, f"{route_id} process did not exit cleanly")
        _timestamp(identity.get("createdAtUtc"), f"{route_id} process creation")
        raw_trace_events = _validate_trace_roundtrip(route, run_directory)
        parse = route.get("parse", {})
        _timestamp(parse.get("requestedAtUtc"), f"{route_id} parse start")
        _timestamp(parse.get("completedAtUtc"), f"{route_id} parse completion")
        _require(int(parse.get("elapsedMilliseconds", -1)) >= 0 and _same_path(parse.get("sourcePath"), fixture_path) and parse.get("workbookSha256") == fixture.get("sha256"), f"{route_id} parse timing/source identity is invalid")
        if route_id.startswith("route-a"):
            _require((int(parse.get("candidateCount", -1)), int(parse.get("scheduleBlockCount", -1))) == (96, 5), "Route A must parse 96 candidates across five blocks")
            review = route.get("review", {})
            _require((int(review.get("classRows", -1)), int(review.get("previewRows", -1))) == (20, 20), "Route A conflict block must show 20 class and preview rows")
            _require(int(review.get("teacherRows", -1)) == 5 and int(review.get("reviewWidgetCount", 10**9)) < 200, "Route A review model/widget counts differ from bounded acceptance")
            _require(review.get("applyEnabled") is False and review.get("uiApplyEnabled") is False, "Route A Apply was not disabled while conflicts remained")
            _require(re.search(r"overlap|conflict|겹", str(review.get("conflictStatus", "")), re.IGNORECASE) is not None, "Route A did not show a conflict warning")
            _require(re.search(r"overlap|conflict|겹", str(review.get("uiStatus", "")), re.IGNORECASE) is not None, "Route A UIA did not observe the conflict warning")
            _validate_released_data(route, "cancelled", fixture_path)
            events = route.get("dialogTrace", [])
            _require(sum(event.get("event") == "apply-request" for event in events) == 0, "Route A emitted an apply request")
            before = _read_json(_artifact_path(route.get("workspace", {}).get("beforeSnapshot"), run_directory, "Route A pre-cancel workspace snapshot"))
            after = _read_json(_artifact_path(route.get("workspace", {}).get("afterSnapshot"), run_directory, "Route A post-cancel workspace snapshot"))
            _verify_snapshot(before, "Route A pre-cancel workspace")
            _verify_snapshot(after, "Route A post-cancel workspace")
            _require(before.get("workspacePath") == after.get("workspacePath") == route.get("workspace", {}).get("path"), "Route A workspace snapshot path changed")
            _require(before.get("tableContentSha256") == after.get("tableContentSha256") and before.get("tables") == after.get("tables"), "Route A cancellation changed row-level workspace contents")
            _require(route.get("workspace", {}).get("rowLevelContentsUnchanged") is True, "Route A row-level workspace equality was not recorded")
            _require(any(row.get("actionKind") == "cancel-conflict-review" and row.get("targetAccessibleName") == "Cancel" for row in action_rows if row.get("routeId") == route_id), "Route A did not use the authentic UIA Cancel action")
            _require(any(row.get("actionKind") == "read-workbook" for row in action_rows if row.get("routeId") == route_id), "Route A did not parse through the UIA workflow")
            _require(any(row.get("actionKind") == "verify-workbook-selection" and Path(str(row.get("parseTraceSourcePath", ""))).resolve() == Path(fixture_path).resolve() for row in action_rows if row.get("routeId") == route_id), "Route A UIA source path did not match the parsed fixture")
            route_result = {"routeId": route_id, "outcome": "conflict-review-cancelled", "candidateCount": int(parse["candidateCount"]), "scheduleBlockCount": int(parse["scheduleBlockCount"]), "reviewRows": int(review["classRows"]), "previewRows": int(review["previewRows"]), "applyRequestCount": 0, "workspaceRowsUnchanged": True, "widgetCount": int(review["reviewWidgetCount"])}
        else:
            _require((int(parse.get("candidateCount", -1)), int(parse.get("scheduleBlockCount", -1))) == (20, 1), "Route B must parse the 20-candidate applicable fixture")
            review = route.get("review", {})
            _require((int(review.get("classRows", -1)), int(review.get("previewRows", -1))) == (20, 20), "Route B review must show 20 class and preview rows")
            _require(int(review.get("reviewWidgetCount", 10**9)) < 200, "Route B review widget count is not bounded")
            _require(any(row.get("actionKind") == "verify-workbook-selection" and Path(str(row.get("parseTraceSourcePath", ""))).resolve() == Path(fixture_path).resolve() for row in action_rows if row.get("routeId") == route_id), "Route B UIA source path did not match the parsed fixture")
            _validate_released_data(route, "applied", fixture_path)
            apply = route.get("apply", {})
            request = apply.get("request", {})
            _validate_manifest_request_matches_raw_trace(request, raw_trace_events)
            _timestamp(apply.get("requestedAtUtc"), "Route B apply start")
            _timestamp(apply.get("completedAtUtc"), "Route B apply completion")
            _require(int(apply.get("elapsedMilliseconds", -1)) >= 0, "Route B apply timing is invalid")
            _require((int(request.get("candidateCount", -1)), int(request.get("teacherDecisionCount", -1)), int(request.get("classDecisionCount", -1))) == (20, 5, 20), "Route B apply request counts differ from 20 candidates / 5 teachers / 20 classes")
            _require((int(request.get("createdTeacherCount", -1)), int(request.get("createdClassCount", -1))) == (5, 20), "Route B creation decisions differ from five teachers / 20 classes")
            _require((int(apply.get("result", {}).get("teachersCreated", -1)), int(apply.get("result", {}).get("classesCreated", -1))) == (5, 20), "Route B transaction summary differs from five teachers / 20 classes")
            workspace_after = _read_json(_artifact_path(route.get("workspace", {}).get("afterSnapshot"), run_directory, "Route B persisted workspace snapshot"))
            _verify_snapshot(workspace_after, "Route B persisted workspace")
            _require(workspace_after.get("workspacePath") == route.get("workspace", {}).get("path"), "Route B persisted workspace identity mismatch")
            associations = _validate_class_associations(workspace_after, request)
            refreshed = route.get("refreshedSchedule", {})
            _require("Weekly Class Schedule" in str(refreshed.get("pageAccessibleName", "")) or refreshed.get("pageAutomationId", "").endswith("nextSchedulePage"), "Route B did not reach the current Schedule page")
            status = str(refreshed.get("status", ""))
            _require("Schedule import completed" in status and "20 classes created" in status, "Route B refreshed Schedule UI did not show the successful 20-class result")
            uia_evidence = refreshed.get("uiaSnapshot", {})
            uia_path = _artifact_path(uia_evidence.get("path"), run_directory, "Route B refreshed Schedule UIA snapshot")
            _require(_sha256(uia_path) == uia_evidence.get("sha256"), "Route B refreshed Schedule UIA snapshot SHA-256 mismatch")
            uia_document = _read_json(uia_path)
            _require(uia_document.get("processId") == int(identity["pid"]) == uia_document.get("windowProcessId"), "Route B refreshed Schedule UIA snapshot process identity mismatch")
            _require(int(uia_document.get("mainWindowHandle", 0)) == int(identity["mainWindowHandle"]), "Route B refreshed Schedule UIA snapshot window handle mismatch")
            _require(str(uia_document.get("page", {}).get("automationId", "")).endswith("nextSchedulePage"), "Route B refreshed Schedule UIA snapshot is not the Schedule page")
            _require("Schedule import completed" in str(uia_document.get("status", {}).get("name", "")), "Route B refreshed Schedule UIA snapshot lacks the applied status")
            _require(str(uia_document.get("tableAutomationId", "")).endswith("nextScheduleTable"), "Route B refreshed Schedule UIA snapshot does not identify the Schedule table")
            ui_items = uia_document.get("visibleDataItems")
            _require(isinstance(ui_items, list) and len(ui_items) > 0 and len(ui_items) == int(uia_document.get("visibleDataItemCount", -1)), "Route B refreshed Schedule UIA snapshot has no rendered table items")
            all_table_items = uia_document.get("tableDataItems")
            _require(isinstance(all_table_items, list) and len(all_table_items) == int(uia_document.get("tableDataItemCount", -1)) and len(all_table_items) >= len(ui_items), "Route B refreshed Schedule UIA snapshot table item inventory is malformed")
            _require(all(item.get("controlType") == "ControlType.DataItem" and int(item.get("processId", -1)) == int(identity["pid"]) and str(item.get("automationId", "")).endswith("nextScheduleTable") for item in all_table_items), "Route B refreshed Schedule table items are not bound to its UIA table/process")
            _require(all(item.get("isOffscreen") is False for item in ui_items) and any(str(item.get("name", "")).strip() for item in ui_items), "Route B refreshed Schedule UIA evidence has no visible schedule data")
            screenshot = refreshed.get("screenshot", {})
            screenshot_path = _artifact_path(screenshot.get("path"), run_directory, "Route B refreshed Schedule screenshot")
            _require(_sha256(screenshot_path) == screenshot.get("sha256"), "Route B refreshed Schedule screenshot SHA-256 mismatch")
            png = screenshot_path.read_bytes()[:24]
            _require(png[:8] == b"\x89PNG\r\n\x1a\n", "Route B refreshed Schedule screenshot is not PNG")
            width, height = struct.unpack(">II", png[16:24])
            _require(width > 0 and height > 0 and int(screenshot.get("width", 0)) == width and int(screenshot.get("height", 0)) == height, "Route B screenshot dimensions are invalid")
            _require(int(screenshot.get("windowProcessId", -1)) == int(identity["pid"]) and int(screenshot.get("windowHandle", 0)) == int(identity["mainWindowHandle"]), "Route B screenshot is not bound to the measured app window")
            _require(screenshot.get("captureMethod") == "print-window-render-full-content" and screenshot.get("printWindowReturnedContent") is True, "Route B screenshot did not render pixels from the measured app window")
            route_result = {"routeId": route_id, "outcome": "applicable-workbook-transactionally-applied", "candidateCount": 20, "classCount": associations["createdClassCount"], "teacherCount": associations["createdTeacherCount"], "scheduleRowCount": associations["classTimeRowCount"], "associationMultisetSha256": associations["associationMultisetSha256"], "refreshedScheduleScreenshot": screenshot, "refreshedScheduleUiaSnapshot": uia_evidence, "widgetCount": int(review["reviewWidgetCount"])}
            route_result["persistedAssociations"] = associations
        _validate_route_cleanup(route, run_directory)
        memory_result = _validate_memory(route)
        memory_results.append({"routeId": route_id, **memory_result})
        route_result["memory"] = memory_result
        summary_routes[route_id] = route_result

    source_log = _read_jsonl(run_directory / "startup-log.jsonl")
    _require(source_log, "startup log is empty")
    _require(all(row.get("processInstanceId") in route_by_id and int(row.get("pid", -1)) == int(route_by_id[row["processInstanceId"]]["processIdentity"]["pid"]) for row in source_log), "startup log contains a foreign process identity")
    process_metrics = _read_jsonl(run_directory / "process-metrics.jsonl")
    _require(len(process_metrics) >= 10, "process metric log lacks lifecycle, cleanup, or idle-tail samples")
    for row in process_metrics:
        route_id = str(row.get("routeId", ""))
        _require(route_id in route_by_id and int(row.get("pid", -1)) == int(route_by_id[route_id]["processIdentity"]["pid"]), "process metric row is bound to the wrong PID")
        for key in ("currentWorkingSetBytes", "privateBytes", "commitBytes", "peakWorkingSetBytes", "peakCommitBytes", "handles", "threads"):
            _require(isinstance(row.get(key), int) and int(row[key]) > 0, f"process metric {row.get('point')} has invalid {key}")
    for relative in ("validator-output.txt",):
        _require((run_directory / relative).exists() or relative == "validator-output.txt", f"missing validator output {relative}")

    normal_violations = [violation for result in memory_results for violation in result["strictWorkingSetViolations"]]
    peak_summary = [peak for result in memory_results for peak in result["operationPeaks"]]
    budget_passed = not normal_violations
    return {
        "schema": SUMMARY_SCHEMA,
        "taskId": "P9_09_PACKAGED_SCHEDULE_IMPORT",
        "status": "passed" if budget_passed else "failed-budget",
        "validatedAtUtc": datetime.now().astimezone().isoformat(),
        "evidenceDirectory": str(run_directory),
        "sourceFingerprintSha256": manifest["source"]["sha256"],
        "packageSha256": manifest["package"]["sha256"],
        "applicationSha256": manifest["application"]["sha256"],
        "routeCount": len(routes),
        "routes": summary_routes,
        "uiaActionCounts": action_counts,
        "memoryBudget": {
            "normalUseWorkingSetExclusiveBytes": WORKING_SET_LIMIT,
            "passed": budget_passed,
            "violations": normal_violations,
            "operationPeakAdvisoryExclusiveBytes": PEAK_ADVISORY_LIMIT,
            "operationPeaks": peak_summary,
            "privateBytesReturnAllowance": None,
        },
    }


def _pe_architecture(path: Path) -> str:
    try:
        with path.open("rb") as stream:
            _require(stream.read(2) == b"MZ", "application has no MZ header")
            stream.seek(0x3C)
            offset = struct.unpack("<I", stream.read(4))[0]
            stream.seek(offset)
            _require(stream.read(4) == b"PE\0\0", "application has no PE header")
            machine = struct.unpack("<H", stream.read(2))[0]
    except OSError as error:
        raise EvidenceError(f"cannot read application PE header: {error}") from error
    return {0x8664: "x64", 0xAA64: "arm64"}.get(machine, "unknown")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-dir", required=True, type=Path)
    options = parser.parse_args(argv)
    evidence = options.evidence_dir.resolve()
    summary_path = evidence / "summary.json"
    try:
        result = validate(evidence)
        summary_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"status": result["status"], "routeCount": result["routeCount"], "summary": str(summary_path)}, ensure_ascii=False))
        return 0 if result["status"] == "passed" else 1
    except (EvidenceError, OSError, KeyError, TypeError, ValueError) as error:
        failed = {
            "schema": SUMMARY_SCHEMA,
            "taskId": "P9_09_PACKAGED_SCHEDULE_IMPORT",
            "status": "failed",
            "evidenceDirectory": str(evidence),
            "error": str(error),
        }
        try:
            summary_path.write_text(json.dumps(failed, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        except OSError:
            pass
        print(f"P9-09 validation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
