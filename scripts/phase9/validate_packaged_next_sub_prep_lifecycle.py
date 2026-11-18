#!/usr/bin/env python3
"""Validate P9-08 packaged Next Sub Prep lifecycle evidence."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import ntpath
import re
import statistics
import sys
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


TASK_ID = "P9_08_PACKAGED_NEXT_SUBPREP_LIFECYCLE"
MANIFEST_SCHEMA = "classmngr-phase9-packaged-next-sub-prep-lifecycle-v1"
SUMMARY_SCHEMA = "classmngr-phase9-packaged-next-sub-prep-lifecycle-summary-v1"
EXPECTED_PROCESSES = ("process-01", "process-02", "process-03")
REPEATED_CYCLES = 10
ACTION_CYCLES = REPEATED_CYCLES + 1  # initial generation entry plus ten repeats
TRANSITIONS_PER_RUN = ACTION_CYCLES * 2
THRESHOLD_BYTES = 262_144_000
IDLE_SECONDS = 300
IDLE_SAMPLE_COUNT = IDLE_SECONDS + 1
MIN_INTERVAL_SECONDS = 0.75
MAX_INTERVAL_SECONDS = 1.5
MAX_EVENT_SAMPLE_DELAY_MS = 2_000
EXPECTED_INTER_FONTS = (
    "resource:assets/fonts/Inter-Regular.ttf",
    "resource:assets/fonts/Inter-Medium.ttf",
    "resource:assets/fonts/Inter-SemiBold.ttf",
    "resource:assets/fonts/Inter-Bold.ttf",
    "resource:assets/fonts/Inter-BoldItalic.ttf",
    "resource:assets/fonts/Inter-Black.ttf",
)
EXPECTED_SUB_PREP_WEEKDAYS = ("Monday", "Tuesday", "Wednesday", "Thursday", "Friday")
EXPECTED_SUB_PREP_SUMMARY_DAYS = EXPECTED_SUB_PREP_WEEKDAYS + ("Saturday",)
EXPECTED_SUB_PREP_VISIBLE_GRADE_COUNT = 24
SUB_PREP_LABEL_HELPER = "scripts/phase9/sub_prep_accessible_summary.py"
SUB_PREP_OUTPUT_FIXTURE_HELPER = "scripts/phase9/prepare_sub_prep_output_fixture.py"
SUB_PREP_OUTPUT_FIXTURE_TEST = "scripts/phase9/test_prepare_sub_prep_output_fixture.py"
SUB_PREP_OUTPUT_FIXTURE_TRANSFORM = "large-sub-prep-output-weekday-fixture-v1"
EXPECTED_PACKAGE_CLASS_COUNT = 30
EXPECTED_ROOTS = (
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
)
REQUIRED_BUILD_TARGETS = (
    "ClassMngrNextResourcePackage",
    "ClassMngrPhase7NextBlankFixtureTests",
    "ClassMngrStartupPerformanceTests",
)
INTER_LICENSE = "licenses/fonts/inter/LICENSE.txt"
SHA_RE = re.compile(r"^[0-9a-f]{64}$")
REVISION_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
STARTUP_READY_RE = re.compile(
    r"^ClassMngrNext startup stage=startup-ready outcome=(?P<outcome>[a-z-]+) "
    r"diagnostic=(?P<diagnostic>[a-z-]+) diagnostic_stage=(?P<diagnostic_stage>[a-z-]+) "
    r"checkpoint=(?P<checkpoint>\{.*\})$"
)
WORKSPACE_OPENED_RE = re.compile(
    r"^ClassMngrNext startup stage=workspace-opened outcome=(?P<outcome>[a-z-]+) "
    r"diagnostic=(?P<diagnostic>[a-z-]+) diagnostic_stage=(?P<diagnostic_stage>[a-z-]+) "
    r"checkpoint=(?P<checkpoint>\{.*\})$"
)
DOC_CATALOG_RE = re.compile(
    r"^ClassMngrNext document-catalog event=projected outcome=(?P<outcome>[a-z-]+) "
    r"document_count=(?P<documents>\d+) loaded_pdf_body_count=(?P<bodies>\d+)$"
)
FONT_RE = re.compile(r"^ClassMngrNext resource checkpoint=(\{.*\})$")
ROUTE_RE = re.compile(
    r"^ClassMngrNext navigation event=page-route outcome=(?P<outcome>[a-z-]+) "
    r"cause=(?P<cause>[a-z-]+) from=(?P<from>[a-z0-9_-]+) to=(?P<to>[a-z0-9_-]+) "
    r"catalog_document_count=(?P<documents>-?\d+) loaded_pdf_body_count=(?P<bodies>\d+)$"
)


class EvidenceValidationError(ValueError):
    """Raised when evidence cannot support a valid gate decision."""


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise EvidenceValidationError(message)


def _obj(value: Any, label: str) -> dict[str, Any]:
    _require(isinstance(value, dict), f"{label} must be an object")
    return value


def _int(value: Any, label: str, minimum: int = 0) -> int:
    _require(isinstance(value, int) and not isinstance(value, bool) and value >= minimum,
             f"{label} must be an integer >= {minimum}")
    return value


def _sha(value: Any, label: str) -> str:
    _require(isinstance(value, str) and SHA_RE.fullmatch(value) is not None,
             f"{label} must be a lowercase SHA-256")
    return value


def _timestamp(value: Any, label: str) -> datetime:
    _require(isinstance(value, str), f"{label} must be a timestamp string")
    normalized = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError as error:
        raise EvidenceValidationError(f"{label} is not an ISO timestamp") from error
    _require(parsed.tzinfo is not None and parsed.utcoffset() == timezone.utc.utcoffset(parsed),
             f"{label} must use UTC")
    return parsed.astimezone(timezone.utc)


def _same_checkpoint_value(left: Any, right: Any, key: str = "") -> bool:
    """Compare captured checkpoints exactly, treating ISO timestamp precision lexically."""
    if key == "captured_at" and isinstance(left, str) and isinstance(right, str):
        try:
            return _timestamp(left, "checkpoint.captured_at") == _timestamp(right, "checkpoint.captured_at")
        except EvidenceValidationError:
            return False
    if isinstance(left, dict) and isinstance(right, dict):
        return left.keys() == right.keys() and all(
            _same_checkpoint_value(left[item], right[item], item) for item in left
        )
    if isinstance(left, list) and isinstance(right, list):
        return len(left) == len(right) and all(
            _same_checkpoint_value(a, b) for a, b in zip(left, right)
        )
    return left == right


def _canonical_sha(files: list[dict[str, Any]]) -> str:
    ordered = sorted(files, key=lambda row: str(row["path"]).casefold())
    canonical = "".join(
        f"{row['path']}\t{row['sizeBytes']}\t{row['sha256']}\n" for row in ordered
    )
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def _read_json(path: Path, label: str) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise EvidenceValidationError(f"cannot read {label}: {error}") from error


def _read_jsonl(path: Path, label: str) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8") as stream:
            for line_no, line in enumerate(stream, start=1):
                _require(bool(line.strip()), f"{label} has empty line {line_no}")
                try:
                    value = json.loads(line)
                except json.JSONDecodeError as error:
                    raise EvidenceValidationError(
                        f"{label} line {line_no} is invalid JSON"
                    ) from error
                rows.append(_obj(value, f"{label} line {line_no}"))
    except OSError as error:
        raise EvidenceValidationError(f"cannot read {label}: {error}") from error
    return rows


def _validate_inventory_files(
    files: Any, label: str, expected_root: Path | None = None
) -> tuple[list[dict[str, Any]], str]:
    _require(isinstance(files, list) and files, f"{label} must be a nonempty file list")
    seen: set[str] = set()
    normalized: list[dict[str, Any]] = []
    for index, value in enumerate(files):
        row = _obj(value, f"{label}[{index}]")
        path = row.get("path")
        size = _int(row.get("sizeBytes"), f"{label}[{index}].sizeBytes")
        digest = _sha(row.get("sha256"), f"{label}[{index}].sha256")
        _require(isinstance(path, str) and path and "\\" not in path and not path.startswith("/"),
                 f"{label}[{index}].path must be a relative slash path")
        key = path.casefold()
        _require(key not in seen, f"{label} contains duplicate path {path}")
        seen.add(key)
        if expected_root is not None:
            target = expected_root.joinpath(*path.split("/"))
            try:
                target.resolve().relative_to(expected_root.resolve())
            except ValueError as error:
                raise EvidenceValidationError(f"{label} path escapes its root: {path}") from error
            _require(target.is_file(), f"{label} file is missing: {path}")
            _require(target.stat().st_size == size, f"{label} size mismatch: {path}")
            actual = hashlib.sha256(target.read_bytes()).hexdigest()
            _require(actual == digest, f"{label} SHA-256 mismatch: {path}")
        normalized.append({"path": path, "sizeBytes": size, "sha256": digest})
    return normalized, _canonical_sha(normalized)


def _compare_prepared_source_inventory(
    current_files: list[dict[str, Any]], prepared_files: list[dict[str, Any]]
) -> list[dict[str, str]]:
    current_by_path = {row["path"].casefold(): row for row in current_files}
    prepared_by_path = {row["path"].casefold(): row for row in prepared_files}
    differences: list[dict[str, str]] = []
    for key in sorted(current_by_path.keys() | prepared_by_path.keys()):
        current = current_by_path.get(key)
        prepared = prepared_by_path.get(key)
        if current is None:
            assert prepared is not None
            difference = {"path": prepared["path"], "change": "removed"}
        elif prepared is None:
            difference = {"path": current["path"], "change": "added"}
        elif (current["sizeBytes"], current["sha256"]) != (prepared["sizeBytes"], prepared["sha256"]):
            difference = {"path": current["path"], "change": "changed"}
        else:
            continue
        _require(difference["path"].casefold().startswith("scripts/phase9/"),
                 f"prepared Release source differs outside scripts/phase9: {difference['path']}")
        differences.append(difference)
    return differences


def _source_inventory_from_tree(root: Path) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    for relative_root in EXPECTED_ROOTS:
        target = root.joinpath(*relative_root.split("/"))
        _require(target.exists(), f"source root is missing: {relative_root}")
        paths = [target] if target.is_file() else list(target.rglob("*"))
        for path in paths:
            if not path.is_file() or any(
                part in {"__pycache__", ".pytest_cache", ".mypy_cache"}
                for part in path.parts
            ) or path.suffix in {".pyc", ".pyo"}:
                continue
            relative = path.relative_to(root).as_posix()
            results.append(
                {
                    "path": relative,
                    "sizeBytes": path.stat().st_size,
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                }
            )
    return sorted(results, key=lambda row: row["path"].casefold())


def _page_records(startup_log: list[dict[str, Any]], process_id: str) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    process_log = [entry for entry in startup_log if entry.get("processInstanceId") == process_id]
    for index, entry in enumerate(process_log):
        line = entry.get("line")
        if not isinstance(line, str) or not line.startswith(
            "ClassMngrNext page lifecycle checkpoint="
        ):
            continue
        raw = line.partition("=")[2]
        try:
            record = json.loads(raw)
        except json.JSONDecodeError as error:
            raise EvidenceValidationError(f"malformed page checkpoint for {process_id} at {index}") from error
        records.append(
            {
                "logIndex": index,
                "observedAtUtc": entry.get("observedAtUtc"),
                "record": _obj(record, "page checkpoint"),
            }
        )
    return records


def _metric_for_process(
    metrics: list[dict[str, Any]], process_id: str, phase: str
) -> list[dict[str, Any]]:
    return [row for row in metrics if row.get("processInstanceId") == process_id and row.get("phase") == phase]


def _validate_app_snapshot(record: dict[str, Any], label: str) -> dict[str, Any]:
    _require(record.get("sample_available") is True and record.get("platform") == "windows",
             f"{label} lacks a Windows process snapshot")
    memory = _obj(record.get("memory"), f"{label}.memory")
    ws = _int(memory.get("working_set_bytes"), f"{label}.working_set_bytes", 1)
    private = _int(memory.get("private_usage_bytes"), f"{label}.private_usage_bytes", 1)
    private_ws = _int(memory.get("private_working_set_bytes"), f"{label}.private_working_set_bytes", 1)
    peak_ws = _int(memory.get("peak_working_set_bytes"), f"{label}.peak_working_set_bytes", 1)
    handles = _int(memory.get("handle_count"), f"{label}.handle_count", 1)
    threads = _int(memory.get("thread_count"), f"{label}.thread_count", 1)
    _timestamp(record.get("captured_at"), f"{label}.captured_at")
    return {"workingSetBytes": ws, "privateBytes": private, "privateWorkingSetBytes": private_ws,
            "peakWorkingSetBytes": peak_ws, "handles": handles, "threads": threads}


def _validate_external_sample(
    sample: dict[str, Any], process: dict[str, Any], label: str
) -> None:
    identity = _obj(process.get("processIdentity"), f"{label}.processIdentity")
    _require(sample.get("processInstanceId") == process.get("processInstanceId"),
             f"{label} is bound to the wrong process instance")
    _require(_int(sample.get("pid"), f"{label}.pid", 1) == identity.get("pid"),
             f"{label} PID differs from the packaged process")
    _require(ntpath.normcase(ntpath.normpath(str(sample.get("canonicalExecutablePath"))))
             == ntpath.normcase(ntpath.normpath(str(identity.get("canonicalExecutablePath")))),
             f"{label} executable path differs from the package")
    _require(sample.get("architecture") == "x64" and sample.get("sha256") == identity.get("sha256"),
             f"{label} executable identity differs from the package")
    _int(sample.get("currentWorkingSetBytes"), f"{label}.currentWorkingSetBytes", 1)
    _int(sample.get("privateBytes"), f"{label}.privateBytes", 1)
    _int(sample.get("peakWorkingSetBytes"), f"{label}.peakWorkingSetBytes", 1)
    _int(sample.get("handles"), f"{label}.handles", 0)
    _int(sample.get("threads"), f"{label}.threads", 1)
    _timestamp(sample.get("timestampUtc"), f"{label}.timestampUtc")


def _validate_uia_action(
    action: Any,
    process: dict[str, Any],
    expected_name: str,
    expected_target: str,
    expected_from: str,
    expected_to: str,
    label: str,
) -> None:
    row = _obj(action, f"{label}.uiaAction")
    identity = _obj(process.get("processIdentity"), f"{label}.processIdentity")
    _require(row.get("method") in {"uia-verified-pointer-click", "uia-verified-window-message-click"},
             f"{label} used an unaccepted UIA method")
    _require(row.get("verifiedAppEvent") is True, f"{label} lacks a verified app event")
    _require(row.get("expectedAccessibleName") == expected_name and row.get("observedAccessibleName") == expected_name,
             f"{label} accessible item name mismatch")
    _require(row.get("itemId") == expected_target, f"{label} intended target ID mismatch")
    _require(row.get("fromPageId") == expected_from and row.get("toPageId") == expected_to,
             f"{label} intended route mismatch")
    _require(_int(row.get("uiaProcessId"), f"{label}.uiaProcessId", 1) == identity.get("pid")
             and _int(row.get("hwndProcessId"), f"{label}.hwndProcessId", 1) == identity.get("pid"),
             f"{label} UIA control/HWND PID mismatch")
    _require(_int(row.get("hwnd"), f"{label}.hwnd", 1) > 0, f"{label} has no HWND")
    rectangle = _obj(row.get("rectangle"), f"{label}.rectangle")
    rect_x = rectangle.get("x")
    rect_y = rectangle.get("y")
    rect_width = rectangle.get("width")
    rect_height = rectangle.get("height")
    _require(all(isinstance(value, (int, float)) and not isinstance(value, bool)
                 for value in (rect_x, rect_y, rect_width, rect_height))
             and rect_width > 0 and rect_height > 0,
             f"{label} UIA selected control rectangle is malformed")
    activation = _obj(row.get("nativeActivation"), f"{label}.nativeActivation")
    screen_x = _int(activation.get("screenX"), f"{label}.screenX")
    screen_y = _int(activation.get("screenY"), f"{label}.screenY")
    _require(rect_x <= screen_x < rect_x + rect_width and rect_y <= screen_y < rect_y + rect_height,
             f"{label} click point is outside the exact UIA-selected control")
    if row.get("method") == "uia-verified-pointer-click":
        _require(activation.get("kind") == "foreground-pointer"
                 and activation.get("foregroundVerified") is True
                 and activation.get("foregroundProcessId") == identity.get("pid")
                 and activation.get("inputEventsSent") == 2,
                 f"{label} pointer click lacks exact foreground identity or both SendInput events")
    else:
        _require(activation.get("kind") == "verified-window-message"
                 and tuple(activation.get("sequence", ())) == (
                     "WaitForInputIdle", "ScreenToClient", "GetClientRect",
                     "WM_MOUSEMOVE", "WM_LBUTTONDOWN", "WM_LBUTTONUP"),
                 f"{label} window-message sequence is invalid")
        _require(_int(activation.get("windowHandle"), f"{label}.message.windowHandle", 1) == row.get("hwnd")
                 and _int(activation.get("expectedProcessId"), f"{label}.message.expectedProcessId", 1) == identity.get("pid")
                 and _int(activation.get("windowProcessId"), f"{label}.message.windowProcessId", 1) == identity.get("pid"),
                 f"{label} window-message HWND/PID identity mismatch")
        _require(activation.get("waitForInputIdleSucceeded") is True,
                 f"{label} window-message click did not wait for the app to reach input-idle")
        bounds = _obj(activation.get("clientBounds"), f"{label}.message.clientBounds")
        left = _int(bounds.get("left"), f"{label}.message.clientBounds.left")
        top = _int(bounds.get("top"), f"{label}.message.clientBounds.top")
        right = _int(bounds.get("right"), f"{label}.message.clientBounds.right", 1)
        bottom = _int(bounds.get("bottom"), f"{label}.message.clientBounds.bottom", 1)
        client_x = _int(activation.get("clientX"), f"{label}.message.clientX")
        client_y = _int(activation.get("clientY"), f"{label}.message.clientY")
        _require(right > left and bottom > top and left <= client_x < right and top <= client_y < bottom,
                 f"{label} window-message click point is outside client bounds")
        _require(activation.get("mouseMoveSent") is True
                 and activation.get("leftButtonDownSent") is True
                 and activation.get("leftButtonUpSent") is True
                 and activation.get("messagesSent") == 3,
                 f"{label} did not deliver all three window click messages")
        pointer_attempt = _obj(row.get("pointerAttempt"), f"{label}.pointerAttempt")
        _require(pointer_attempt.get("method") == "uia-verified-pointer-click"
                 and pointer_attempt.get("fallbackReason") == "foreground-not-verified"
                 and pointer_attempt.get("foregroundVerified") is False
                 and pointer_attempt.get("inputEventsSent") == 0
                 and _int(pointer_attempt.get("windowHandle"), f"{label}.pointerAttempt.windowHandle", 1) == row.get("hwnd")
                 and _int(pointer_attempt.get("expectedProcessId"), f"{label}.pointerAttempt.expectedProcessId", 1) == identity.get("pid")
                 and _int(pointer_attempt.get("windowProcessId"), f"{label}.pointerAttempt.windowProcessId", 1) == identity.get("pid"),
                 f"{label} window-message click did not follow a failed foreground-only pointer attempt")


def _validate_fixture_derived_target(
    action: Any,
    process: dict[str, Any],
    source_file_map: dict[str, dict[str, Any]],
    repository_root: str,
    expected_name: str,
    label: str,
) -> dict[str, Any]:
    row = _obj(action, f"{label}.uiaAction")
    target = _obj(row.get("fixtureDerivedTarget"), f"{label}.fixtureDerivedTarget")
    identity = _obj(process.get("processIdentity"), f"{label}.processIdentity")
    expected_fixture = str(process.get("fixturePathPassedToApp", ""))
    expected_fixture_sha = _sha(process.get("fixtureSha256PassedToApp"), f"{label}.fixtureSha256PassedToApp")
    # The runner records canonical absolute helper paths; the run manifest owns the source root.
    expected_helper = ntpath.normcase(ntpath.normpath(
        ntpath.join(repository_root, SUB_PREP_LABEL_HELPER.replace("/", "\\"))
    ))
    helper_row = source_file_map.get(SUB_PREP_LABEL_HELPER)
    _require(helper_row is not None, f"{label} source inventory omits the fixture-derived accessibility helper")
    helper_sha = _sha(helper_row.get("sha256"), f"{label}.helper source SHA-256")

    _require(target.get("method") == "fixture-derived-exact-accessible-name"
             and target.get("classId") == "96" and target.get("grade") == "M2",
             f"{label} fixture-derived class target is not the required M2 class 96")
    _require(target.get("accessibleName") == expected_name,
             f"{label} fixture-derived accessible name does not match the selected UIA row")
    if "observedAccessibleName" in row or "expectedAccessibleName" in row:
        _require(row.get("observedAccessibleName") == expected_name
                 and row.get("expectedAccessibleName") == expected_name,
             f"{label} fixture-derived accessible name does not match the selected UIA row")
    fixture_path = str(target.get("fixturePath", ""))
    _require(ntpath.normcase(ntpath.normpath(fixture_path))
             == ntpath.normcase(ntpath.normpath(expected_fixture))
             and target.get("fixtureSha256") == expected_fixture_sha,
             f"{label} fixture-derived label is not bound to the exact launched fixture")
    helper_path = str(target.get("helperPath", ""))
    _require(ntpath.normcase(ntpath.normpath(helper_path)) == expected_helper
             and target.get("helperSha256") == helper_sha,
             f"{label} fixture-derived label helper identity differs from the source inventory")
    selected_days = target.get("selectedDays")
    _require(isinstance(selected_days, list) and tuple(selected_days) == EXPECTED_SUB_PREP_SUMMARY_DAYS,
             f"{label} fixture-derived label does not use the complete six-day Sub Prep summary scope")
    _require(_int(target.get("visibleClassCount"), f"{label}.visibleClassCount", 1)
             == EXPECTED_SUB_PREP_VISIBLE_GRADE_COUNT,
             f"{label} fixture-derived label does not resolve against the 24 visible M2 rows in the full summary")
    _int(target.get("baseLabelDuplicateCount"), f"{label}.baseLabelDuplicateCount", 1)

    _require(str(target.get("listAutomationId", "")).endswith("subPrepClassSummaryList")
             and target.get("listControlType") == "ControlType.List"
             and _int(target.get("listProcessId"), f"{label}.listProcessId", 1) == identity.get("pid"),
             f"{label} summary list UIA identity is invalid")
    _require(str(target.get("rowAutomationId", "")).endswith("subPrepClassSummaryList")
             and target.get("rowControlType") == "ControlType.ListItem"
             and _int(target.get("rowProcessId"), f"{label}.rowProcessId", 1) == identity.get("pid"),
             f"{label} class row UIA identity is invalid")
    main_window_handle = _int(target.get("mainWindowHandle"), f"{label}.mainWindowHandle", 1)
    _require(_int(target.get("mainWindowProcessId"), f"{label}.mainWindowProcessId", 1) == identity.get("pid"),
             f"{label} main-window PID differs from the measured process")
    if row.get("hwnd") is not None:
        _require(main_window_handle == _int(row.get("hwnd"), f"{label}.uiaAction.hwnd", 1),
                 f"{label} class-list main HWND differs from the clicked app window")
    list_rectangle = _obj(target.get("listRectangle"), f"{label}.listRectangle")
    for dimension in ("x", "y", "width", "height"):
        value = list_rectangle.get(dimension)
        _require(isinstance(value, (int, float)) and not isinstance(value, bool),
                 f"{label}.listRectangle.{dimension} is malformed")
    _require(list_rectangle["width"] > 0 and list_rectangle["height"] > 0,
             f"{label} summary list has an empty UIA rectangle")
    rectangle = _obj(target.get("rowRectangle"), f"{label}.rowRectangle")
    activation_value = row.get("nativeActivation")
    if isinstance(activation_value, dict) and ("screenX" in activation_value or "screenY" in activation_value):
        screen_x = _int(activation_value.get("screenX"), f"{label}.click.screenX")
        screen_y = _int(activation_value.get("screenY"), f"{label}.click.screenY")
        _require(rectangle["x"] <= screen_x < rectangle["x"] + rectangle["width"]
                 and rectangle["y"] <= screen_y < rectangle["y"] + rectangle["height"]
                 and list_rectangle["x"] <= screen_x < list_rectangle["x"] + list_rectangle["width"]
                 and list_rectangle["y"] <= screen_y < list_rectangle["y"] + list_rectangle["height"],
                 f"{label} class-row click point is outside the exact row/list intersection")
    _require(_int(target.get("exactVisibleMatchCount"), f"{label}.exactVisibleMatchCount", 1) == 1,
             f"{label} did not resolve exactly one visible class row")
    scroll_steps = _int(target.get("scrollSteps"), f"{label}.scrollSteps")
    _require(scroll_steps <= 40, f"{label} exceeded the bounded UIA list scroll search")
    trace = target.get("scrollTrace")
    _require(isinstance(trace, list) and len(trace) == scroll_steps + 1,
             f"{label} list scroll trace is incomplete")
    scroll_method = target.get("scrollMethod")
    _require(scroll_method in {"uia-scrollpattern", "verified-window-message-wheel"},
             f"{label} used an unaccepted list scrolling method")
    final_matches: list[dict[str, Any]] = []
    for index, value in enumerate(trace):
        step = _obj(value, f"{label}.scrollTrace[{index}]")
        _require(_int(step.get("step"), f"{label}.scrollTrace[{index}].step") == index,
                 f"{label} list scroll trace is out of order")
        _require(step.get("scrollMethod") == scroll_method,
                 f"{label} list scroll trace changed methods mid-search")
        percent = step.get("verticalScrollPercent")
        if scroll_method == "uia-scrollpattern":
            _require(isinstance(percent, (int, float)) and not isinstance(percent, bool)
                     and 0 <= percent <= 100 and step.get("scrollInput") is None,
                     f"{label} UIA list scroll position is invalid")
        else:
            _require(percent is None, f"{label} wheel-driven list scroll contains an invented scroll percent")
            if index < scroll_steps:
                scroll_input = _obj(step.get("scrollInput"), f"{label}.scrollTrace[{index}].scrollInput")
                _require(scroll_input.get("method") == "verified-window-message-wheel"
                         and tuple(scroll_input.get("sequence", ())) == (
                             "WaitForInputIdle", "ScreenToClient", "GetClientRect", "WM_MOUSEWHEEL"),
                         f"{label} class-list wheel message sequence is invalid")
                _require(_int(scroll_input.get("windowHandle"), f"{label}.scrollInput.windowHandle", 1) == main_window_handle
                         and _int(scroll_input.get("expectedProcessId"), f"{label}.scrollInput.expectedProcessId", 1) == identity.get("pid")
                         and _int(scroll_input.get("windowProcessId"), f"{label}.scrollInput.windowProcessId", 1) == identity.get("pid"),
                         f"{label} class-list wheel message HWND/PID identity is invalid")
                _require(scroll_input.get("waitForInputIdleSucceeded") is True
                         and scroll_input.get("messageSent") is True
                         and scroll_input.get("wheelDelta") == -120
                         and scroll_input.get("visibleRowsChanged") is True,
                         f"{label} class-list wheel message did not cause a verified scroll")
                bounds = _obj(scroll_input.get("clientBounds"), f"{label}.scrollInput.clientBounds")
                client_left = _int(bounds.get("left"), f"{label}.scrollInput.clientBounds.left")
                client_top = _int(bounds.get("top"), f"{label}.scrollInput.clientBounds.top")
                client_right = _int(bounds.get("right"), f"{label}.scrollInput.clientBounds.right", 1)
                client_bottom = _int(bounds.get("bottom"), f"{label}.scrollInput.clientBounds.bottom", 1)
                client_x = _int(scroll_input.get("clientX"), f"{label}.scrollInput.clientX")
                client_y = _int(scroll_input.get("clientY"), f"{label}.scrollInput.clientY")
                _require(client_right > client_left and client_bottom > client_top
                         and client_left <= client_x < client_right and client_top <= client_y < client_bottom,
                         f"{label} class-list wheel point is outside the main-window client bounds")
                list_bounds = _obj(scroll_input.get("listBounds"), f"{label}.scrollInput.listBounds")
                list_left = _int(list_bounds.get("left"), f"{label}.scrollInput.listBounds.left")
                list_top = _int(list_bounds.get("top"), f"{label}.scrollInput.listBounds.top")
                list_right = _int(list_bounds.get("right"), f"{label}.scrollInput.listBounds.right", 1)
                list_bottom = _int(list_bounds.get("bottom"), f"{label}.scrollInput.listBounds.bottom", 1)
                _require(list_right > list_left and list_bottom > list_top
                         and list_left <= _int(scroll_input.get("screenX"), f"{label}.scrollInput.screenX") < list_right
                         and list_top <= _int(scroll_input.get("screenY"), f"{label}.scrollInput.screenY") < list_bottom,
                         f"{label} class-list wheel point is outside the UIA list bounds")
                expected_bounds = {
                    "left": int(list_rectangle["x"] // 1),
                    "top": int(list_rectangle["y"] // 1),
                    "right": int(-(- (list_rectangle["x"] + list_rectangle["width"]) // 1)),
                    "bottom": int(-(- (list_rectangle["y"] + list_rectangle["height"]) // 1)),
                }
                _require((list_left, list_top, list_right, list_bottom) == (
                    expected_bounds["left"], expected_bounds["top"],
                    expected_bounds["right"], expected_bounds["bottom"]),
                    f"{label} class-list wheel bounds differ from the UIA list rectangle")
                before_names = scroll_input.get("beforeVisibleAccessibleNames")
                after_names = scroll_input.get("afterVisibleAccessibleNames")
                _require(isinstance(before_names, list) and isinstance(after_names, list)
                         and before_names != after_names,
                         f"{label} wheel scroll lacks a verified before/after visible-row change")
            else:
                _require(step.get("scrollInput") is None,
                         f"{label} final class row was found but another wheel message was recorded")
        visible_rows = step.get("visibleRows")
        _require(isinstance(visible_rows, list), f"{label} visible row evidence is malformed")
        visible_matches = 0
        for row_index, raw_candidate in enumerate(visible_rows):
            candidate = _obj(raw_candidate, f"{label}.scrollTrace[{index}].visibleRows[{row_index}]")
            _require(candidate.get("controlType") == "ControlType.ListItem"
                     and str(candidate.get("automationId", "")).endswith("subPrepClassSummaryList")
                     and _int(candidate.get("processId"), f"{label}.visibleRow.processId", 1) == identity.get("pid"),
                     f"{label} visible row identity is invalid")
            _require(isinstance(candidate.get("accessibleName"), str)
                     and isinstance(candidate.get("isOffscreen"), bool),
                     f"{label} visible row label/offscreen evidence is malformed")
            if candidate.get("accessibleName") == expected_name and candidate.get("isOffscreen") is False:
                visible_matches += 1
                if index == scroll_steps:
                    final_matches.append(candidate)
        if index < scroll_steps:
            _require(visible_matches == 0, f"{label} scroll trace skipped an earlier exact visible match")
            if scroll_method == "verified-window-message-wheel":
                names = [candidate.get("accessibleName") for candidate in visible_rows
                         if candidate.get("isOffscreen") is False]
                scroll_input = step["scrollInput"]
                next_step = _obj(trace[index + 1], f"{label}.scrollTrace[{index + 1}]")
                next_rows = next_step.get("visibleRows")
                _require(names == scroll_input.get("beforeVisibleAccessibleNames")
                         and [candidate.get("accessibleName") for candidate in next_rows
                              if candidate.get("isOffscreen") is False]
                         == scroll_input.get("afterVisibleAccessibleNames"),
                         f"{label} wheel scroll visible-row trace does not match adjacent UIA snapshots")
        else:
            _require(visible_matches == 1, f"{label} final scroll position lacks one exact visible class row")
    width = rectangle.get("width")
    height = rectangle.get("height")
    _require(isinstance(width, (int, float)) and not isinstance(width, bool) and width > 0
             and isinstance(height, (int, float)) and not isinstance(height, bool) and height > 0,
             f"{label} target UIA row has an empty bounding rectangle")
    _require(len(final_matches) == 1 and final_matches[0].get("rectangle") == rectangle,
             f"{label} final visible target row does not match its recorded rectangle")
    return target


def _validate_uia_trace(
    traces: list[dict[str, Any]], process: dict[str, Any], event_index: int,
    expected_name: str, target: str, from_page: str, to_page: str, label: str,
) -> dict[str, Any]:
    matches = [row for row in traces
               if row.get("processInstanceId") == process.get("processInstanceId")
               and row.get("appEventLogIndex") == event_index
               and row.get("verifiedAppEvent") is True]
    _require(len(matches) == 1, f"{label} lacks exactly one verified UIA trace row")
    _validate_uia_action(matches[0], process, expected_name, target, from_page, to_page, label + ".trace")
    return matches[0]


def validate_documents(
    manifest: dict[str, Any],
    source_inventory: dict[str, Any],
    package_inventory: dict[str, Any],
    startup_log: list[dict[str, Any]],
    process_metrics: list[dict[str, Any]],
    navigation_metrics: list[dict[str, Any]],
    uia_actions: list[dict[str, Any]],
    generated_files: dict[str, Any],
) -> dict[str, Any]:
    _require(manifest.get("schema") == MANIFEST_SCHEMA, "manifest schema mismatch")
    _require(manifest.get("taskId") == TASK_ID and manifest.get("status") == "completed",
             "manifest is incomplete or belongs to another task")
    thresholds = _obj(manifest.get("thresholds"), "thresholds")
    _require(thresholds.get("normalUseWorkingSetBytesExclusive") == THRESHOLD_BYTES
             and thresholds.get("strictAtOrAboveFails") is True
             and thresholds.get("idleSeconds") == IDLE_SECONDS
             and thresholds.get("idleSampleCount") == IDLE_SAMPLE_COUNT
             and thresholds.get("sampleIntervalSeconds") == 1
             and thresholds.get("generationPeakWorkingSetBytesAdvisory") == 536_870_912
             and thresholds.get("generationPeakMetric") == "process-lifetime peak working set at package-generation completion",
             "P9-08 strict normal-use or advisory generation limits changed")
    _require(manifest.get("repeatedSubPrepCycles") == REPEATED_CYCLES,
             "manifest does not require ten repeated Sub Prep cycles")
    _require(REVISION_RE.fullmatch(str(manifest.get("revision"))) is not None, "revision is invalid")
    candidate = _obj(manifest.get("candidateSource"), "candidateSource")
    _require(tuple(candidate.get("includedRoots", ())) == EXPECTED_ROOTS, "candidate source roots differ from the canonical input inventory")
    _require(candidate.get("stableDuringRun") is True, "candidate source changed during measurement")
    source_files, source_sha = _validate_inventory_files(source_inventory.get("files"), "source inventory")
    _require(source_sha == candidate.get("sha256") == candidate.get("sha256AtRunEnd"), "source inventory digest is not source-bound")
    required_source_files = {
        "scripts/phase9/run_packaged_next_sub_prep_lifecycle.ps1",
        "scripts/phase9/validate_packaged_next_sub_prep_lifecycle.py",
        "scripts/phase9/test_validate_packaged_next_sub_prep_lifecycle.py",
        SUB_PREP_LABEL_HELPER,
        SUB_PREP_OUTPUT_FIXTURE_HELPER,
        SUB_PREP_OUTPUT_FIXTURE_TEST,
        "scripts/phase9/run_packaged_idle_memory_gate.ps1",
        "src/next/bootstrap/application_bootstrap.cpp",
        "src/next/bootstrap/qt_page_lifecycle_diagnostics.cpp",
        "src/next/ui/sub_prep_page.cpp",
        "tests/next/page_host_tests.cpp",
        "tests/next/sub_prep_page_tests.cpp",
        INTER_LICENSE,
    }
    source_paths = {row["path"] for row in source_files}
    _require(required_source_files <= source_paths, "source inventory omits a required P9-08 input")
    source_file_map = {row["path"]: row for row in source_files}
    repository_root = str(manifest.get("repositoryRoot", ""))
    _require(ntpath.isabs(repository_root), "manifest repository root is not an absolute path")

    configuration = _obj(manifest.get("configuration"), "configuration")
    _sha(configuration.get("cmakeCacheSha256"), "CMakeCache SHA-256")
    _require(configuration.get("cmakeCacheStableDuringRun") is True
             and configuration.get("cmakeCacheSha256AtRunEnd") == configuration.get("cmakeCacheSha256"),
             "Release CMakeCache changed during the run")
    compiler = _obj(configuration.get("releaseCompiler"), "releaseCompiler")
    _require(compiler.get("compiler") == "MSVC" and compiler.get("configuration") == "Release"
             and compiler.get("platform") == "x64" and str(compiler.get("generator", "")).startswith("Visual Studio"),
             "Release compiler identity is invalid")
    for key in ("cFlagsRelease", "cxxFlagsRelease"):
        flags = _obj(compiler.get(key), key)
        raw = str(flags.get("raw", ""))
        _require(re.search(r"(?i)(?:^|\s)/O(?:1|2|x)(?:\s|$)", raw) is not None
                 and re.search(r"(?i)(?:^|\s)/D\s*NDEBUG(?:\s|$)", raw) is not None,
                 f"{key} does not include optimization and NDEBUG")
    _require(compiler.get("projectOptimizationSetting") in {"MaxSpeed", "MinSpace"},
             "generated Release project does not enable optimization")
    build = _obj(configuration.get("packageBuild"), "packageBuild")
    _require(build.get("succeeded") is True and build.get("exitCode") == 0
             and tuple(build.get("targets", ())) == REQUIRED_BUILD_TARGETS,
             "Release package/fixture prerequisite build is incomplete")
    reuse_attestation: dict[str, Any] | None = None
    prepared_source_digest: str | None = None
    if build.get("mode") == "verified-prebuilt-reuse":
        _require(build.get("integrityLevel") == "elevated"
                 and build.get("runnerIntegrityLevel") == "standard"
                 and build.get("integrityLevelsDifferent") is True
                 and configuration.get("runnerIntegrityLevel") == "standard",
                 "prebuilt reuse did not separate the elevated Release build from the standard-integrity UIA launch")
        reuse_attestation = _obj(build.get("reuse"), "prebuilt reuse attestation")
        prepared_source_digest = _sha(reuse_attestation.get("preparedSourceSha256"), "prepared source SHA-256")
        _require(candidate.get("preparedSourceFingerprint") == prepared_source_digest,
                 "prepared source fingerprint is not bound to the candidate record")
        prepared_files, actual_prepared_digest = _validate_inventory_files(
            source_inventory.get("preparedFiles"), "prepared source inventory")
        _require(actual_prepared_digest == prepared_source_digest,
                 "prepared source inventory digest differs from the elevated attestation")
        source_differences = _compare_prepared_source_inventory(source_files, prepared_files)
        _require(source_differences == reuse_attestation.get("phase9OnlySourceDifferences")
                 == candidate.get("reuseDifferences"),
                 "prebuilt reuse source differences are incomplete or misreported")
        _require(all(row.get("path", "").casefold().startswith("scripts/phase9/") for row in source_differences),
                 "prebuilt reuse has a non-Phase-9 source difference")
        _require(isinstance(reuse_attestation.get("preparedEvidenceDirectory"), str)
                 and isinstance(reuse_attestation.get("preparedManifestPath"), str),
                 "prebuilt reuse evidence paths are missing")
        integrity_evidence = _obj(reuse_attestation.get("integrityLevelEvidence"), "prepared build integrity evidence")
        _require(integrity_evidence.get("level") == "elevated"
                 and integrity_evidence.get("kind") in {"windows-token-observed", "main-agent-confirmed-elevated-build"}
                 and isinstance(integrity_evidence.get("reference"), str) and integrity_evidence.get("reference"),
                 "elevated Release build integrity evidence is missing")
        for digest_name in ("preparedManifestSha256", "preparedPackageSha256", "preparedApplicationSha256",
                            "preparedFixtureGeneratorSha256", "preparedFixtureSha256", "preparedCMakeCacheSha256"):
            _sha(reuse_attestation.get(digest_name), f"prebuilt reuse {digest_name}")
        _require(reuse_attestation.get("preparedCMakeCacheSha256") == configuration.get("cmakeCacheSha256"),
                 "prebuilt Release CMakeCache differs from the current standard tree")
    else:
        _require(not source_inventory.get("preparedFiles"),
                 "prepared source inventory is present without verified prebuilt reuse mode")

    qt = _obj(manifest.get("qtInstallation"), "qtInstallation")
    _require(isinstance(qt.get("version"), str) and re.fullmatch(r"\d+\.\d+\.\d+", qt["version"]), "Qt version is invalid")
    _require(isinstance(qt.get("prefix"), str) and qt["prefix"], "Qt prefix is missing")
    _sha(qt.get("coreDllSha256"), "QtCore SHA-256")
    _sha(qt.get("windeployqtSha256"), "windeployqt SHA-256")

    package = _obj(manifest.get("package"), "package")
    _require(package.get("candidateSourceSha256") == candidate.get("sha256"), "package was built from a different source fingerprint")
    if reuse_attestation is not None:
        _require(package.get("sourceIdentityMode") == "verified-prebuilt-non-phase9-equivalence"
                 and package.get("sourceFingerprintAtBuild") == prepared_source_digest
                 and reuse_attestation.get("preparedPackageSha256") == package.get("sha256"),
                 "reused package identity is not bound to the verified prepared source/package")
    package_files, package_sha = _validate_inventory_files(package_inventory.get("files"), "package inventory")
    _require(package_sha == package.get("sha256"), "package inventory digest mismatch")
    _require(package.get("fileCount") == len(package_files), "package file count mismatch")
    app = _obj(manifest.get("application"), "application")
    _require(app.get("sha256") == package.get("applicationSha256") and app.get("architecture") == "x64"
             and app.get("candidateSourceSha256") == candidate.get("sha256"), "packaged application identity mismatch")
    _sha(app.get("sha256"), "application SHA-256")
    if reuse_attestation is not None:
        _require(reuse_attestation.get("preparedApplicationSha256") == app.get("sha256"),
                 "reused application hash differs from elevated Release evidence")
    package_map = {row["path"]: row for row in package_files}
    _require("ClassMngrNext.exe" in package_map and package_map["ClassMngrNext.exe"]["sha256"] == app["sha256"],
             "package inventory does not contain the exact app executable")
    license_info = _obj(package.get("interFontLicense"), "Inter license")
    _require(license_info.get("sourcePath") == INTER_LICENSE and license_info.get("packagePath") == INTER_LICENSE,
             "Inter OFL license path is invalid")
    license_row = package_map.get(INTER_LICENSE)
    _require(license_row is not None and license_row["sha256"] == license_info.get("sha256")
             and license_row["sizeBytes"] == license_info.get("sizeBytes"), "Inter OFL license is absent or has the wrong hash")
    font_entries = package.get("interFontCatalogEntries")
    _require(isinstance(font_entries, list) and len(font_entries) == len(EXPECTED_INTER_FONTS), "package must include six Inter static faces")
    for index, expected in enumerate(EXPECTED_INTER_FONTS):
        entry = _obj(font_entries[index], "Inter font entry")
        _require(entry.get("resourceId") == expected, "Inter font catalog order or identity mismatch")
        _sha(entry.get("sha256"), f"{expected} hash")
        package_path = "resources/" + expected.removeprefix("resource:")
        package_row = package_map.get(package_path)
        _require(package_row is not None and package_row["sha256"] == entry["sha256"]
                 and package_row["sizeBytes"] == entry.get("sizeBytes"), f"package font identity mismatch: {expected}")

    runtime_files = qt.get("runtimeFiles")
    _require(isinstance(runtime_files, list) and len(runtime_files) >= 6,
             "Qt runtime DLL/plugin identity inventory is missing")
    runtime_by_path: dict[str, dict[str, Any]] = {}
    for runtime in runtime_files:
        runtime = _obj(runtime, "Qt runtime file")
        package_path = runtime.get("packagePath")
        _require(isinstance(package_path, str) and package_path not in runtime_by_path,
                 "Qt runtime file list contains a duplicate or invalid package path")
        _sha(runtime.get("sha256"), f"{package_path} SHA-256")
        _int(runtime.get("sizeBytes"), f"{package_path} size", 1)
        _require(isinstance(runtime.get("qtPrefixPath"), str)
                 and ntpath.normcase(ntpath.normpath(runtime["qtPrefixPath"])).startswith(
                     ntpath.normcase(ntpath.normpath(qt["prefix"]) + "\\")),
                 f"{package_path} was not bound to the configured Qt prefix")
        package_row = package_map.get(package_path)
        _require(package_row is not None and package_row["sha256"] == runtime["sha256"]
                 and package_row["sizeBytes"] == runtime["sizeBytes"],
                 f"packaged runtime identity mismatch: {package_path}")
        runtime_by_path[package_path] = runtime
    for required in ("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll",
                     "platforms/qwindows.dll", "sqldrivers/qsqlite.dll"):
        _require(required in runtime_by_path, f"Qt runtime identity omits {required}")
    _require(runtime_by_path["Qt6Core.dll"]["sha256"] == qt["coreDllSha256"],
             "QtCore runtime hash differs from the configured Qt installation")
    if reuse_attestation is not None:
        _require(reuse_attestation.get("preparedFixtureGeneratorSha256")
                 == (qt.get("fixtureGenerators") or [{}])[0].get("executableSha256"),
                 "reused fixture generator differs from elevated Release evidence")

    fixture = _obj(manifest.get("fixture"), "fixture")
    _require(fixture.get("id") == "synthetic-large-startup-96-classes"
             and fixture.get("classCount") == 96 and fixture.get("teacherCount") == 24
             and fixture.get("rosterCellCount") == 7200, "fixture shape is not 96 classes / 24 teachers / 7200 roster cells")
    fixture_sha = _sha(fixture.get("sha256"), "fixture SHA-256")
    _sha(fixture.get("generatorSha256"), "fixture generator SHA-256")
    if reuse_attestation is not None:
        _require(reuse_attestation.get("preparedFixtureSha256") == fixture_sha,
                 "reused representative fixture differs from elevated Release evidence")
    fixture_generators = qt.get("fixtureGenerators")
    _require(isinstance(fixture_generators, list) and len(fixture_generators) == 1,
             "representative fixture generator Qt identity is missing")
    fixture_generator = _obj(fixture_generators[0], "fixture generator Qt identity")
    _require(fixture_generator.get("id") == "representative-96-class-fixture-generator"
             and fixture_generator.get("verifiedSameQtInstallation") is True
             and fixture_generator.get("executablePath") == fixture.get("generatorPath")
             and fixture_generator.get("executableSha256") == fixture.get("generatorSha256")
             and ntpath.normcase(ntpath.normpath(str(fixture_generator.get("runtimeSearchPathPrefix"))))
             == ntpath.normcase(ntpath.normpath(ntpath.join(qt["prefix"], "bin"))),
             "fixture generator executable or runtime search path is not bound to the configured Qt")
    colocated_qt = fixture_generator.get("colocatedQtDlls")
    _require(isinstance(colocated_qt, list), "fixture generator colocated Qt runtime inventory is missing")
    for dll in colocated_qt:
        dll = _obj(dll, "fixture generator colocated Qt DLL")
        _sha(dll.get("sha256"), "fixture generator Qt DLL SHA-256")
        _int(dll.get("sizeBytes"), "fixture generator Qt DLL size", 1)
        configured = runtime_by_path.get(str(dll.get("fileName")))
        _require(configured is not None and configured["sha256"] == dll.get("sha256")
                 and configured["sizeBytes"] == dll.get("sizeBytes"),
                 "fixture generator colocated Qt DLL differs from the package Qt installation")
    copies = fixture.get("perProcessCopies")
    preparation = _obj(fixture.get("outputPreparation"), "fixture.outputPreparation")
    helper_path = str(preparation.get("helperPath", ""))
    helper_record = source_file_map.get(SUB_PREP_OUTPUT_FIXTURE_HELPER)
    _require(preparation.get("transformId") == SUB_PREP_OUTPUT_FIXTURE_TRANSFORM
             and helper_path.replace("\\", "/").endswith(SUB_PREP_OUTPUT_FIXTURE_HELPER)
             and helper_record is not None and preparation.get("helperSha256") == helper_record.get("sha256")
             and preparation.get("selectedPackageDays") == list(EXPECTED_SUB_PREP_WEEKDAYS)
             and preparation.get("weekdayClassCount") == 30 and preparation.get("saturdayClassCount") == 66
             and preparation.get("requestedPackageClassCount") == EXPECTED_PACKAGE_CLASS_COUNT
             and preparation.get("conflictsInSelectedDays") == 0,
             "output-ready fixture transform identity or weekday package shape is invalid")
    _sha(preparation.get("helperSha256"), "fixture preparation helper SHA-256")
    _require(isinstance(copies, list) and len(copies) == EXPECTED_PROCESSES.__len__(), "fixture must have one isolated launch copy per process")
    copy_map = {row.get("processInstanceId"): row for row in copies if isinstance(row, dict)}
    _require(set(copy_map) == set(EXPECTED_PROCESSES), "per-process fixture copy IDs mismatch")
    expected_prepared_day_counts = {
        "Monday": 6, "Tuesday": 6, "Wednesday": 6, "Thursday": 6,
        "Friday": 6, "Saturday": 66,
    }
    for process_id, row in copy_map.items():
        output_preparation = _obj(row.get("outputPreparation"), f"{process_id}.fixture.outputPreparation")
        prepared_sha = _sha(row.get("sha256BeforeLaunch"), f"{process_id}.prepared fixture SHA-256")
        prelaunch_sha = _sha(row.get("preLaunchSnapshotSha256"), f"{process_id}.pre-launch fixture snapshot SHA-256")
        _require(row.get("sourceFixtureSha256") == fixture_sha
                 and output_preparation.get("fixturePath") == row.get("path")
                 and row.get("preLaunchSnapshotPath")
                 and prelaunch_sha == prepared_sha
                 and output_preparation.get("transformId") == SUB_PREP_OUTPUT_FIXTURE_TRANSFORM
                 and output_preparation.get("sourceSha256") == fixture_sha
                 and output_preparation.get("preparedSha256") == prepared_sha
                 and output_preparation.get("classCount") == 96
                 and output_preparation.get("teacherCount") == 24
                 and output_preparation.get("rosterCellCount") == 7200
                 and output_preparation.get("classTimeCount") == 96
                 and output_preparation.get("weekdayClassCount") == 30
                 and output_preparation.get("saturdayClassCount") == 66
                 and output_preparation.get("dayCounts") == expected_prepared_day_counts
                 and output_preparation.get("selectedPackageDays") == list(EXPECTED_SUB_PREP_WEEKDAYS)
                 and output_preparation.get("requestedPackageClassCount") == EXPECTED_PACKAGE_CLASS_COUNT
                 and output_preparation.get("weekdayTimeConflictCount") == 0,
                 f"{process_id} fixture transform does not preserve the 96/24/7200 route contract")

    processes = manifest.get("processRuns")
    _require(isinstance(processes, list) and tuple(row.get("processInstanceId") for row in processes) == EXPECTED_PROCESSES,
             "exactly three ordered fresh process runs are required")
    pids: set[int] = set()
    output_roots: set[str] = set()
    summaries: list[dict[str, Any]] = []
    overall_budget_passed = True
    all_metric_rows = process_metrics
    all_navigation_rows = navigation_metrics
    all_generated = generated_files.get("packages") if isinstance(generated_files, dict) else None
    _require(isinstance(all_generated, list) and len(all_generated) == len(processes), "generated package file map is incomplete")
    generated_map = {row.get("processInstanceId"): row for row in all_generated if isinstance(row, dict)}
    _require(set(generated_map) == set(EXPECTED_PROCESSES), "generated package process IDs mismatch")

    for process_id, process in zip(EXPECTED_PROCESSES, processes):
        process = _obj(process, process_id)
        _require(process.get("processInstanceId") == process_id, "process order or identity mismatch")
        identity = _obj(process.get("processIdentity"), f"{process_id}.processIdentity")
        pid = _int(identity.get("pid"), f"{process_id}.pid", 1)
        _require(pid not in pids, "fresh process PIDs are not unique")
        pids.add(pid)
        _require(identity.get("architecture") == "x64" and identity.get("sha256") == app.get("sha256")
                 and ntpath.normcase(ntpath.normpath(str(identity.get("canonicalExecutablePath"))))
                 == ntpath.normcase(ntpath.normpath(str(app.get("canonicalPath")))),
                 f"{process_id} sampled a different app/package identity")
        _timestamp(identity.get("createdAtUtc"), f"{process_id}.createdAtUtc")
        _require(process.get("fixturePathPassedToApp") == copy_map[process_id].get("path")
                 and process.get("fixtureSha256PassedToApp") == copy_map[process_id].get("sha256BeforeLaunch")
                 and process.get("fixturePreLaunchSnapshotPath") == copy_map[process_id].get("preLaunchSnapshotPath")
                 and process.get("fixturePreLaunchSnapshotSha256") == copy_map[process_id].get("preLaunchSnapshotSha256"),
                 f"{process_id} command line fixture path/hash mismatch")
        exit_shape = _obj(process.get("fixtureShapeAtExit"), f"{process_id}.fixtureShapeAtExit")
        _sha(process.get("fixtureHashAtExit"), f"{process_id}.fixtureHashAtExit")
        _require(exit_shape.get("classCount") == 96 and exit_shape.get("teacherCount") == 24
                 and exit_shape.get("rosterCellCount") == 7200 and exit_shape.get("integrityCheck") == "ok",
                 f"{process_id} post-exit fixture hash/shape evidence is invalid")
        _require(process.get("commandLineArguments") == [process.get("fixturePathPassedToApp")],
                 f"{process_id} did not pass exactly the recorded fixture path to the app")
        output_root = ntpath.normcase(ntpath.normpath(str(process.get("outputRoot"))))
        _require(output_root and output_root not in output_roots, "process output roots are not unique")
        output_roots.add(output_root)
        _require(process.get("normalUseBudgetPassed") in (True, False), f"{process_id} budget result is missing")
        if not process.get("normalUseBudgetPassed"):
            overall_budget_passed = False

        log = [row for row in startup_log if row.get("processInstanceId") == process_id]
        _require(log and all(row.get("pid") == pid for row in log), f"{process_id} startup log is not process-bound")
        lines = [str(row.get("line", "")) for row in log]
        ready = [(i, STARTUP_READY_RE.fullmatch(line)) for i, line in enumerate(lines)]
        ready = [(i, match) for i, match in ready if match]
        _require(len(ready) == 1, f"{process_id} must have exactly one startup-ready event")
        ready_index, ready_match = ready[0]
        ready_checkpoint = json.loads(ready_match.group("checkpoint"))
        _require(ready_match.group("outcome") == "completed" and ready_match.group("diagnostic") == "none"
                 and ready_match.group("diagnostic_stage") == "none" and ready_checkpoint.get("outcome") == "completed"
                 and ready_checkpoint.get("stage") == "startup-ready", f"{process_id} startup-ready failed")
        ready_metrics = _obj(ready_checkpoint.get("metrics"), f"{process_id}.startup-ready.metrics")
        ready_memory = {
            "workingSetBytes": _int(ready_metrics.get("working_set_bytes"), f"{process_id}.startup-ready working set", 1),
            "privateBytes": _int(ready_metrics.get("private_usage_bytes"), f"{process_id}.startup-ready private bytes", 1),
            "privateWorkingSetBytes": _int(ready_metrics.get("private_working_set_bytes"), f"{process_id}.startup-ready private working set", 1),
            "peakWorkingSetBytes": _int(ready_metrics.get("peak_working_set_bytes"), f"{process_id}.startup-ready peak working set", 1),
            "handles": _int(ready_metrics.get("handle_count"), f"{process_id}.startup-ready handles", 1),
            "threads": _int(ready_metrics.get("thread_count"), f"{process_id}.startup-ready threads", 1),
        }
        _timestamp(log[ready_index].get("observedAtUtc"), f"{process_id}.startup-ready observedAtUtc")
        _require(ready_memory["workingSetBytes"] == process.get("startupReadyWorkingSetBytes"), f"{process_id} startup-ready metric mismatch")
        workspace_events = [(i, WORKSPACE_OPENED_RE.fullmatch(line)) for i, line in enumerate(lines)]
        workspace_events = [(i, match) for i, match in workspace_events if match]
        _require(len(workspace_events) == 1, f"{process_id} requires exactly one workspace-opened event")
        workspace_index, workspace_match = workspace_events[0]
        workspace_checkpoint = json.loads(workspace_match.group("checkpoint"))
        _require(workspace_index < ready_index and workspace_match.group("outcome") == "completed"
                 and workspace_match.group("diagnostic") == "none"
                 and workspace_match.group("diagnostic_stage") == "none"
                 and workspace_checkpoint.get("outcome") == "completed", f"{process_id} workspace-opened was not a successful pre-ready event")
        catalog_events = [(i, DOC_CATALOG_RE.fullmatch(line)) for i, line in enumerate(lines)]
        catalog_events = [(i, match) for i, match in catalog_events if match]
        _require(len(catalog_events) == 1, f"{process_id} must project the startup document catalog once")
        catalog_index, catalog_match = catalog_events[0]
        _require(catalog_index < ready_index and catalog_match.group("outcome") == "completed"
                 and int(catalog_match.group("documents")) > 0 and catalog_match.group("bodies") == "0",
                 f"{process_id} startup catalog loaded a body or was incomplete")
        catalog_document_count = int(catalog_match.group("documents"))
        _require(process.get("startupDocumentCatalog", {}).get("loadedPdfBodyCount") == 0,
                 f"{process_id} manifest reports a loaded PDF body")

        fonts: list[tuple[int, str]] = []
        for index, line in enumerate(lines):
            if "resource:assets/fonts/Inter" not in line:
                continue
            match = FONT_RE.fullmatch(line)
            _require(match is not None, f"{process_id} has malformed Inter resource evidence")
            resource = json.loads(match.group(1))
            fonts.append((index, str(resource.get("resource_id"))))
            _require(resource.get("event") == "resource-load" and resource.get("outcome") == "succeeded"
                     and resource.get("request_scope", {}).get("kind") == "startup",
                     f"{process_id} Inter resource did not load successfully at startup")
        _require(tuple(resource for _, resource in fonts) == EXPECTED_INTER_FONTS
                 and len(fonts) == 6 and all(index < ready_index for index, _ in fonts),
                 f"{process_id} six Inter static-face events are missing, unordered, or late")
        _require(tuple(event.get("resourceId") for event in process.get("interFontResourceEvents", [])) == EXPECTED_INTER_FONTS,
                 f"{process_id} manifest Inter face event summary mismatch")

        route_lines = [(index, ROUTE_RE.fullmatch(line)) for index, line in enumerate(lines)]
        route_lines = [(index, match) for index, match in route_lines if match]
        _require(not any(re.search(r"(?i)(?:pdf[_ -]body[_ -]loaded|pdf body loaded|document body loaded)", line)
                         for line in lines), f"{process_id} startup trace contains a PDF body load event")
        initial = [match for _, match in route_lines if match.group("cause") == "initial"]
        _require(len(initial) == 1 and initial[0].group("from") == "none" and initial[0].group("to") == "my_workspace"
                 and initial[0].group("bodies") == "0", f"{process_id} initial Workspace route is invalid")
        measured_routes = [(index, match) for index, match in route_lines if match.group("cause") == "selection"]
        expected_routes = [("my_workspace", "sub_prep"), ("sub_prep", "my_workspace")] * ACTION_CYCLES
        _require(len(measured_routes) == TRANSITIONS_PER_RUN, f"{process_id} expected 22 authentic Sub Prep navigation transitions")
        nav_rows = sorted(
            [row for row in all_navigation_rows if row.get("processInstanceId") == process_id and isinstance(row.get("transitionIndex"), int)],
            key=lambda row: row.get("transitionIndex", -1),
        )
        _require(len(nav_rows) == TRANSITIONS_PER_RUN, f"{process_id} navigation metrics have missing transitions")
        for transition_index, (nav_row, (from_page, to_page), (log_index, match)) in enumerate(zip(nav_rows, expected_routes, measured_routes)):
            _require(nav_row.get("transitionIndex") == transition_index and nav_row.get("fromPageId") == from_page
                     and nav_row.get("toPageId") == to_page, f"{process_id} transition {transition_index} is out of order")
            _require(match.group("outcome") == "completed" and match.group("from") == from_page
                     and match.group("to") == to_page and match.group("bodies") == "0"
                     and int(match.group("documents")) == catalog_document_count,
                     f"{process_id} route event {transition_index} failed or changed the zero-body document catalog")
            _require(nav_row.get("appEventLogIndex") == log_index and nav_row.get("appEventLine") == lines[log_index],
                     f"{process_id} route metric does not correlate to the captured app log")
            expected_name = "Sub Prep" if to_page == "sub_prep" else "My Workspace"
            _validate_uia_action(nav_row.get("uiaAction"), process, expected_name, to_page, from_page, to_page,
                                 f"{process_id}.route[{transition_index}]")
            _validate_uia_trace(uia_actions, process, log_index, expected_name, to_page,
                                from_page, to_page, f"{process_id}.route[{transition_index}]")
            sample = _obj(nav_row.get("processSample"), "route process sample")
            _validate_external_sample(sample, process, f"{process_id}.route[{transition_index}]")
            delay = nav_row.get("sampleDelayMilliseconds")
            _require(isinstance(delay, (int, float)) and 0 <= delay <= MAX_EVENT_SAMPLE_DELAY_MS,
                     f"{process_id} route snapshot was not promptly correlated")

        page_records = _page_records(startup_log, process_id)
        actions = [row for row in page_records if row["record"].get("event") == "page-action-checkpoint"]
        subprep_actions = [row for row in actions if row["record"].get("page_key") == "sub_prep"]
        def action_rows(checkpoint: str) -> list[dict[str, Any]]:
            return [row for row in subprep_actions if row["record"].get("checkpoint") == checkpoint]
        summaries_rows = action_rows("summary-data-ready")
        summary_activation_rows = [row for row in page_records
                                   if row["record"].get("event") == "page-lifecycle"
                                   and row["record"].get("page_key") == "sub_prep"
                                   and row["record"].get("state") == "activated"]
        refresh_rows = action_rows("explicit-refresh")
        all_selection_rows = action_rows("class-selection")
        selected_rows = [row for row in action_rows("class-selection") if row["record"].get("outcome") == "details-ready" and row["record"].get("page", {}).get("sub_prep_runtime_metrics", {}).get("selected_class_id") == "96"]
        generation_started = action_rows("package-generation-started")
        generation_rows = action_rows("package-generation")
        post_generation = action_rows("post-generation-snapshot")
        suspended_rows = action_rows("page-suspended")
        released_rows = action_rows("post-page-release-snapshot")
        _require(len(summaries_rows) == ACTION_CYCLES, f"{process_id} must report 11 summary-ready checkpoints")
        summary_lifecycle_links = process.get("summaryLifecycleSnapshots")
        _require(isinstance(summary_lifecycle_links, list),
                 f"{process_id}.summaryLifecycleSnapshots must be a list")
        _require(len(summary_activation_rows) == ACTION_CYCLES
                 and len(summary_lifecycle_links) == ACTION_CYCLES,
                 f"{process_id} must pair all 11 summary-ready actions with activated page snapshots")
        _require(len(refresh_rows) == ACTION_CYCLES and all(row["record"].get("outcome") == "completed" for row in refresh_rows),
                 f"{process_id} must report 11 successful explicit refreshes")
        _require(all_selection_rows and all(row["record"].get("outcome") == "details-ready"
                 and str(row["record"].get("page", {}).get("sub_prep_runtime_metrics", {}).get("selected_class_id", "")).isdigit()
                 and 1 <= int(row["record"].get("page", {}).get("sub_prep_runtime_metrics", {}).get("selected_class_id", 0)) <= 96
                 for row in all_selection_rows), f"{process_id} contains a failed or invalid class-selection event")
        _require(len(selected_rows) == ACTION_CYCLES, f"{process_id} must report 11 successful class-96 detail selections")
        _require(len(generation_started) == 1 and generation_started[0]["record"].get("outcome") == "started"
                 and len(generation_rows) == 1 and generation_rows[0]["record"].get("outcome") == "completed",
                 f"{process_id} must generate exactly one successful package")
        _require(len(post_generation) == 3 and {row["record"].get("details", {}).get("delay") for row in post_generation} == {"immediate", "1s", "5s"},
                 f"{process_id} needs immediate, 1-second, and 5-second post-generation snapshots")
        _require(len(suspended_rows) == ACTION_CYCLES and len(released_rows) == ACTION_CYCLES * 2,
                 f"{process_id} page suspension/delayed release snapshots are incomplete")
        for sequence, suspended in enumerate(suspended_rows, start=1):
            _require(suspended["record"].get("details", {}).get("release_sequence") == sequence,
                     f"{process_id} suspension release sequence is missing, duplicated, or out of order")
        for sequence in range(1, ACTION_CYCLES + 1):
            sequence_rows = [row for row in released_rows if row["record"].get("details", {}).get("release_sequence") == sequence]
            _require(len(sequence_rows) == 2 and {row["record"].get("details", {}).get("delay") for row in sequence_rows} == {"1s", "5s"},
                     f"{process_id} release sequence {sequence} needs exact 1-second and 5-second checkpoints")
            suspended_time = _timestamp(suspended_rows[sequence - 1]["observedAtUtc"], "page-suspended time")
            for release_row in sequence_rows:
                release_time = _timestamp(release_row["observedAtUtc"], "page-release time")
                delay = (release_time - suspended_time).total_seconds()
                expected_delay = release_row["record"].get("details", {}).get("delay")
                if expected_delay == "1s":
                    _require(0.5 <= delay <= 2.5, f"{process_id} release sequence {sequence} 1-second snapshot was mistimed")
                else:
                    _require(4.0 <= delay <= 7.5, f"{process_id} release sequence {sequence} 5-second snapshot was mistimed")
        for row in post_generation:
            _require(row["record"].get("outcome") == "completed", f"{process_id} has a failed post-generation checkpoint")
        post_generation_by_delay = {row["record"].get("details", {}).get("delay"): row for row in post_generation}
        generated_at = _timestamp(generation_rows[0]["observedAtUtc"], "generation completion time")
        for delay_name, lower, upper in (("immediate", 0.0, 2.0), ("1s", 0.5, 2.5), ("5s", 4.0, 7.5)):
            elapsed = (_timestamp(post_generation_by_delay[delay_name]["observedAtUtc"], "post-generation time") - generated_at).total_seconds()
            _require(lower <= elapsed <= upper, f"{process_id} post-generation {delay_name} snapshot was mistimed")
        _require(process.get("navigationTransitionCount") == TRANSITIONS_PER_RUN
                 and process.get("classSelectionCount") == ACTION_CYCLES
                 and process.get("explicitRefreshCount") == ACTION_CYCLES
                 and process.get("suspensionCount") == ACTION_CYCLES,
                 f"{process_id} manifest lifecycle counts mismatch")

        metric_rows = [row for row in process_metrics if row.get("processInstanceId") == process_id]
        baseline = _obj(process.get("baselineWorkspace"), f"{process_id}.baselineWorkspace")
        baseline_sample = _obj(baseline.get("processSample"), f"{process_id}.baselineWorkspace.processSample")
        _validate_external_sample(baseline_sample, process, f"{process_id}.workspace-baseline")
        _require(baseline.get("startupLogIndex") is not None
                 and baseline.get("appLifecycleSnapshot", {}).get("event") == "page-lifecycle"
                 and baseline.get("appLifecycleSnapshot", {}).get("page_key") == "my_workspace"
                 and baseline.get("appLifecycleSnapshot", {}).get("state") == "activated"
                 and baseline_sample.get("appLifecycleSnapshot") == baseline.get("appLifecycleSnapshot"),
                 f"{process_id} lacks a matched Workspace activation baseline")
        _validate_app_snapshot(baseline["appLifecycleSnapshot"], f"{process_id}.workspace-baseline")
        _require(isinstance(baseline_sample.get("sampleDelayMilliseconds"), (int, float))
                 and 0 <= baseline_sample["sampleDelayMilliseconds"] <= MAX_EVENT_SAMPLE_DELAY_MS,
                 f"{process_id} Workspace baseline snapshot was not promptly correlated")

        def validate_paired_metric_samples(phase: str, events: list[dict[str, Any]], label: str) -> list[dict[str, Any]]:
            paired = [row for row in metric_rows if row.get("phase") == phase]
            _require(len(paired) == len(events), f"{process_id} {label} process snapshot count mismatch")
            by_index = {row.get("sampleIndex"): row for row in paired}
            _require(len(by_index) == len(paired), f"{process_id} {label} snapshots have duplicate event indexes")
            for event in events:
                sample = by_index.get(event["logIndex"])
                _require(sample is not None and _same_checkpoint_value(sample.get("appCheckpoint"), event["record"]),
                         f"{process_id} {label} process snapshot is not paired to its app event")
                _validate_external_sample(sample, process, f"{process_id}.{label}")
                _require(isinstance(sample.get("sampleDelayMilliseconds"), (int, float))
                         and 0 <= sample["sampleDelayMilliseconds"] <= MAX_EVENT_SAMPLE_DELAY_MS,
                         f"{process_id} {label} snapshot was not promptly correlated")
            return paired

        validate_paired_metric_samples("explicit-refresh", refresh_rows, "explicit-refresh")
        generation_complete_samples = validate_paired_metric_samples("generation-complete", generation_rows, "generation-complete")
        post_generation_samples: list[dict[str, Any]] = []
        for delay_name in ("immediate", "1s", "5s"):
            event = post_generation_by_delay[delay_name]
            post_generation_samples.extend(validate_paired_metric_samples("post-generation-" + delay_name, [event], "post-generation-" + delay_name))

        suspended_metric_samples = validate_paired_metric_samples("page-suspended", suspended_rows, "page-suspended")
        release_1s_rows = [row for row in released_rows if row["record"].get("details", {}).get("delay") == "1s"]
        release_5s_rows = [row for row in released_rows if row["record"].get("details", {}).get("delay") == "5s"]
        release_metric_samples = validate_paired_metric_samples("page-release-1s", release_1s_rows, "page-release-1s")
        release_metric_samples += validate_paired_metric_samples("page-release-5s", release_5s_rows, "page-release-5s")

        process_uia = [row for row in uia_actions if row.get("processInstanceId") == process_id]
        selection_nav_rows = sorted(
            [row for row in all_navigation_rows if row.get("processInstanceId") == process_id
             and row.get("actionKind") == "class-selection"],
            key=lambda row: row.get("appEventLogIndex", -1),
        )
        _require(len(selection_nav_rows) == ACTION_CYCLES,
                 f"{process_id} needs 11 UIA class-selection records")
        day_filter_rows = [row for row in process_uia if row.get("actionKind") == "sub-prep-summary-day-filter"]
        _require(len(day_filter_rows) == 1,
                 f"{process_id} must enable Saturday exactly once to expose the full summary fixture")
        day_filter = day_filter_rows[0]
        _require(day_filter.get("method") == "uia-toggle-pattern"
                 and str(day_filter.get("automationId", "")).endswith("subPrepDay_5")
                 and day_filter.get("accessibleName") == "Saturday"
                 and day_filter.get("controlType") == "ControlType.CheckBox"
                 and day_filter.get("uiaProcessId") == pid
                 and _int(day_filter.get("mainWindowHandle"), f"{process_id}.SaturdayFilter.mainWindowHandle", 1) > 0
                 and day_filter.get("hwndProcessId") == pid
                 and day_filter.get("isOffscreen") is False
                 and day_filter.get("isEnabled") is True
                 and day_filter.get("toggleStateBefore") == "Off"
                 and day_filter.get("toggleStateAfter") == "On"
                 and day_filter.get("verifiedAppEvent") is True
                 and day_filter.get("summaryClassCountAfter") == 96,
                 f"{process_id} Saturday summary filter UIA identity or toggle state is invalid")
        filter_start = _int(day_filter.get("actionStartLogIndex"), f"{process_id}.SaturdayFilter.actionStartLogIndex")
        first_subprep_route = nav_rows[0].get("appEventLogIndex", -1)
        first_selection = selection_nav_rows[0]
        first_selection_event = _obj(first_selection.get("appEvent"), f"{process_id}.firstClassSelection.appEvent")
        _require(first_subprep_route < filter_start <= first_selection.get("appEventLogIndex", -1)
                 and day_filter.get("verifiedByAppEventLogIndex") == first_selection.get("appEventLogIndex")
                 and day_filter.get("verifiedByAppEvent") == first_selection_event
                 and _timestamp(day_filter.get("issuedAtUtc"), f"{process_id}.SaturdayFilter.issuedAtUtc")
                 < _timestamp(first_selection.get("appEventObservedAtUtc"), f"{process_id}.firstClassSelection.observedAtUtc")
                 and first_selection_event.get("outcome") == "details-ready"
                 and first_selection_event.get("page", {}).get("sub_prep_runtime_metrics", {}).get("summary_class_count") == 96,
                 f"{process_id} Saturday filter is not ordered and correlated to the authentic 96-class selection event")
        for cycle_index, row in enumerate(selection_nav_rows):
            _require(row.get("selectedClassId") == "96"
                     and row.get("appEventLogIndex") == selected_rows[cycle_index]["logIndex"],
                     f"{process_id} selection {cycle_index + 1} is not correlated to its class-96 checkpoint")
            selection_action = _obj(row.get("uiaAction"), f"{process_id}.class-selection[{cycle_index}].uiaAction")
            expected_class_name = str(row.get("selectedAccessibleName", ""))
            _validate_uia_action(
                selection_action, process,
                expected_class_name,
                "96", "sub_prep", "sub_prep",
                f"{process_id}.class-selection[{cycle_index}]")
            target_evidence = _validate_fixture_derived_target(
                selection_action, process, source_file_map, repository_root,
                expected_class_name, f"{process_id}.class-selection[{cycle_index}]")
            trace_action = _validate_uia_trace(uia_actions, process, int(row["appEventLogIndex"]),
                                expected_class_name,
                                "96", "sub_prep", "sub_prep",
                                f"{process_id}.class-selection[{cycle_index}]")
            trace_target = _validate_fixture_derived_target(
                trace_action, process, source_file_map, repository_root,
                expected_class_name, f"{process_id}.class-selection[{cycle_index}].trace")
            _require(trace_target == target_evidence,
                     f"{process_id} class-selection trace target evidence differs from the navigation record")
            app_event = _obj(row.get("appEvent"), "class-selection app event")
            _require(app_event.get("outcome") == "details-ready"
                     and app_event.get("page", {}).get("sub_prep_runtime_metrics", {}).get("selected_class_id") == "96",
                     f"{process_id} class-selection UIA click lacks the matching app event")
            _validate_external_sample(_obj(row.get("sample"), "class selection process sample"),
                                      process, f"{process_id}.class-selection[{cycle_index}]")

        resolutions = [row for row in process_uia if row.get("actionKind") == "class-row-resolution"]
        _require(len(resolutions) == ACTION_CYCLES,
                 f"{process_id} requires one fixture-derived class-row resolution per selection")
        for action_index, resolution in enumerate(resolutions):
            _require(resolution.get("outcome") == "target-found"
                     and _int(resolution.get("actionIndex"), f"{process_id}.class-row-resolution.actionIndex") == action_index,
                     f"{process_id} class-row resolution order is invalid")
            expected_name = str(selection_nav_rows[action_index].get("selectedAccessibleName", ""))
            resolution_target = _validate_fixture_derived_target(
                {"fixtureDerivedTarget": resolution.get("resolution")}, process,
                source_file_map, repository_root, expected_name,
                f"{process_id}.class-row-resolution[{action_index}]")
            click_target = _obj(selection_nav_rows[action_index].get("uiaAction"), "class selection UIA action").get("fixtureDerivedTarget")
            _require(resolution_target == click_target,
                     f"{process_id} class-row resolution differs from its authentic class selection")
        generation_dialogs = [row for row in process_uia if row.get("actionKind") == "open-generation-dialog"]
        generation_submits = [row for row in process_uia if row.get("actionKind") == "package-generation-submit"]
        _require(len(generation_dialogs) == 1 and len(generation_submits) == 1,
                 f"{process_id} did not use exactly one UIA production generation dialog and submit action")
        dialog = generation_dialogs[0]
        _validate_uia_action(dialog, process, str(dialog.get("expectedAccessibleName", "")),
                             "subPrepGenerateButton", "sub_prep", "sub_prep",
                             f"{process_id}.generation-dialog")
        dialog_handle = _int(dialog.get("dialogHandle"), "generation dialog HWND", 1)
        _require(str(dialog.get("automationId", "")).endswith("subPrepGenerateButton")
                 and dialog.get("dialogProcessId") == pid
                 and _timestamp(dialog.get("dialogObservedAtUtc"), "generation dialog observation timestamp")
                 and dialog_handle != _int(dialog.get("hwnd"), "Generate button window HWND", 1),
                 f"{process_id} generation dialog UIA identity is invalid")
        submit = generation_submits[0]
        _validate_uia_action(submit, process, str(submit.get("expectedAccessibleName", "")),
                             "subPrepOutputAccept", "sub_prep", "sub_prep",
                             f"{process_id}.generation-submit")
        _require(str(submit.get("automationId", "")).endswith("subPrepOutputAccept")
                 and _int(submit.get("hwnd"), "generation submit HWND", 1) == dialog_handle
                 and _int(submit.get("dialogHandle"), "generation submit dialog HWND", 1) == dialog_handle,
                 f"{process_id} generation submit UIA identity is invalid")
        submitted_event = _obj(submit.get("appEvent"), "generation submit event")
        _require(submit.get("appEventLogIndex") == generation_started[0]["logIndex"]
                 and submitted_event.get("outcome") == "started"
                 and submitted_event.get("details", {}).get("requestedClassCount") == EXPECTED_PACKAGE_CLASS_COUNT
                 and ntpath.normcase(ntpath.normpath(str(submitted_event.get("details", {}).get("targetRoot")))) == output_root,
                 f"{process_id} UIA generation submit does not match the authentic 30-class weekday app event")
        output_controls = [row for row in process_uia if row.get("actionKind") == "package-output-control"]
        control_values = {str(row.get("automationId", "")).split(".")[-1]: row.get("value") for row in output_controls}
        required_control_values = {
            "subPrepOutputFolder": process.get("outputRoot"),
            "subPrepOutputUserName": process.get("userName"),
            "subPrepCreateFolderCheckBox": "On",
            "subPrepPrintPaperCopies": "Off",
            "subPrepOpenFolderAfterGeneration": "Off",
            "subPrepNextVacationCheckBox": "Off",
        }
        _require(len(output_controls) == len(required_control_values)
                 and all(control_values.get(key) == value for key, value in required_control_values.items()),
                 f"{process_id} output dialog values do not match the intended unique package request")
        for cycle_index in range(ACTION_CYCLES):
            route_in = nav_rows[cycle_index * 2]
            route_out = nav_rows[cycle_index * 2 + 1]
            selection = selection_nav_rows[cycle_index]
            refresh = refresh_rows[cycle_index]
            suspension = suspended_rows[cycle_index]
            _require(route_in.get("appEventLogIndex", -1) < selection.get("appEventLogIndex", -1)
                     < refresh.get("logIndex", -1) < suspension.get("logIndex", -1)
                     < route_out.get("appEventLogIndex", -1),
                     f"{process_id} route/select/refresh/suspension order is invalid in cycle {cycle_index + 1}")

        for cycle_index, row in enumerate(summaries_rows):
            record = row["record"]
            _require(record.get("outcome") == "completed", f"{process_id} has failed summary data")
            _validate_app_snapshot(record, f"{process_id}.summary-action[{cycle_index + 1}]")
            activation = summary_activation_rows[cycle_index]
            route_in = nav_rows[cycle_index * 2]
            activation_record = activation["record"]
            _require(row["logIndex"] < activation["logIndex"]
                     < route_in.get("appEventLogIndex", -1),
                     f"{process_id} summary action, page activation snapshot, and route event order is invalid")
            _validate_app_snapshot(activation_record, f"{process_id}.summary-activation[{cycle_index + 1}]")
            _require(activation_record.get("page_instance_available") is True,
                     f"{process_id} summary activation lacks its page instance")
            metrics = activation_record.get("page", {}).get("sub_prep_runtime_metrics", {})
            expected_summary_count = 30 if cycle_index == 0 else 96
            _require(metrics.get("summary_class_count") == expected_summary_count and metrics.get("teacher_summary_count") == 24
                     and metrics.get("summary_model_count") == 1
                     and 0 < metrics.get("active_grade_row_count", 0) <= expected_summary_count,
                     f"{process_id} summary counts do not match the pre-filter/full representative fixture bounds")
            link = _obj(summary_lifecycle_links[cycle_index],
                        f"{process_id}.summaryLifecycleSnapshots[{cycle_index}]")
            _require(link.get("summaryActionLogIndex") == row["logIndex"]
                     and link.get("activationSnapshotLogIndex") == activation["logIndex"]
                     and link.get("pageLifecycleSnapshot") == activation_record,
                     f"{process_id} summary lifecycle link differs from its captured startup trace")
        for row in selected_rows:
            record = row["record"]
            metrics = record.get("page", {}).get("sub_prep_runtime_metrics", {})
            _require(metrics.get("selected_details_retained") is True and metrics.get("selected_class_id") == "96"
                     and metrics.get("selected_detail_panel_count") == 1
                     and metrics.get("selected_detail_note_editor_count") == 2
                     and metrics.get("preparation_editor_count") == 4
                     and metrics.get("text_editor_count") == 6
                     and metrics.get("summary_class_count") == 96 and metrics.get("teacher_summary_count") == 24
                     and 0 < metrics.get("active_grade_row_count", 0) <= 96,
                     f"{process_id} class 96 detail/editor/model bounds are invalid")
            _validate_app_snapshot(record, f"{process_id}.class-96")
        for row in refresh_rows:
            _validate_app_snapshot(row["record"], f"{process_id}.refresh")
            metrics = row["record"].get("page", {}).get("sub_prep_runtime_metrics", {})
            _require(metrics.get("summary_class_count") == 96 and metrics.get("teacher_summary_count") == 24
                     and metrics.get("selected_detail_note_editor_count") == 2
                     and metrics.get("preparation_editor_count") == 4,
                     f"{process_id} refresh metrics exceed Sub Prep bounds")

        # Pair every clicked class-96 details-ready event with the following refresh;
        # the persistent page widget tree must remain stable across both actions.
        for selection in selected_rows:
            following_refresh = [row for row in refresh_rows if row["logIndex"] > selection["logIndex"]]
            _require(following_refresh, f"{process_id} class selection has no following refresh")
            refresh = min(following_refresh, key=lambda row: row["logIndex"])
            selected_widget_count = selection["record"].get("page", {}).get("widget_count")
            refreshed_widget_count = refresh["record"].get("page", {}).get("widget_count")
            _require(selected_widget_count == refreshed_widget_count, f"{process_id} Sub Prep widget count changed across selection/refresh")

        for index, row in enumerate(suspended_rows, start=1):
            record = row["record"]
            metrics = record.get("page", {}).get("sub_prep_runtime_metrics", {})
            _require(record.get("outcome") == "completed" and metrics.get("selected_details_retained") is False
                     and metrics.get("selected_class_id") == "" and metrics.get("summary_class_count") == 0
                     and metrics.get("teacher_summary_count") == 0 and metrics.get("active_grade_row_count") == 0,
                     f"{process_id} suspension {index} retained page data")
            _validate_app_snapshot(record, f"{process_id}.suspension[{index}]")
        for index, row in enumerate(released_rows, start=1):
            record = row["record"]
            metrics = record.get("page", {}).get("sub_prep_runtime_metrics", {})
            _require(record.get("outcome") == "completed" and metrics.get("selected_details_retained") is False
                     and metrics.get("selected_class_id") == "" and metrics.get("summary_class_count") == 0,
                     f"{process_id} post-release checkpoint {index} retained page data")
            _validate_app_snapshot(record, f"{process_id}.post-release[{index}]")
        lifecycle_suspended = [row for row in page_records if row["record"].get("event") == "page-lifecycle"
                               and row["record"].get("page_key") == "sub_prep" and row["record"].get("state") == "suspended"]
        _require(len(lifecycle_suspended) == ACTION_CYCLES, f"{process_id} lifecycle recorder missed a suspension state")

        gen_start = generation_started[0]["record"]
        gen_done = generation_rows[0]["record"]
        _require(gen_start.get("details", {}).get("requestedClassCount") == EXPECTED_PACKAGE_CLASS_COUNT
                 and gen_start.get("details", {}).get("createFolder") is True
                 and gen_start.get("details", {}).get("openFolderAfterGeneration") is False
                 and ntpath.normcase(ntpath.normpath(str(gen_start.get("details", {}).get("targetRoot")))) == output_root,
                 f"{process_id} package request settings are invalid")
        _require(gen_done.get("details", {}).get("selectedClassCount") == EXPECTED_PACKAGE_CLASS_COUNT,
                 f"{process_id} weekday package did not include the established 30 conflict-free classes")
        generation_memory = _validate_app_snapshot(gen_done, f"{process_id}.generation")
        _require(process.get("generation", {}).get("processPeakWorkingSetBytesAtCompletion") == generation_memory["peakWorkingSetBytes"]
                 and process.get("generation", {}).get("exceedsAdvisoryCeiling")
                 is (generation_memory["peakWorkingSetBytes"] > 536_870_912),
                 f"{process_id} generation peak advisory summary mismatch")
        generation_sample = generation_complete_samples[0]
        _require(_same_checkpoint_value(generation_sample.get("appCheckpoint"), gen_done)
                 and generation_sample.get("phase") == "generation-complete",
                 f"{process_id} generation completion lacks its paired external snapshot")
        gen_output = generated_map[process_id]
        _require(gen_output.get("outputRoot") == process.get("outputRoot")
                 and gen_output.get("outputDirectory") == process.get("outputDirectory")
                 and process.get("generation", {}).get("outputDirectory") == process.get("outputDirectory")
                 and gen_output.get("documentCount") == gen_done.get("details", {}).get("documentCount")
                 and gen_output.get("documentCount", 0) > 0,
                 f"{process_id} generated output manifest does not match app completion event")
        docs = gen_output.get("documents")
        _require(isinstance(docs, list) and len(docs) == gen_output.get("documentCount"),
                 f"{process_id} generated document inventory count mismatch")
        names: set[str] = set()
        for doc in docs:
            doc = _obj(doc, f"{process_id}.generated document")
            _require(doc.get("processInstanceId") == process_id and doc.get("format") == "PDF"
                     and str(doc.get("relativePath", "")).lower().endswith(".pdf"),
                     f"{process_id} generated document identity is invalid")
            _int(doc.get("sizeBytes"), f"{process_id}.generated size", 6)
            _sha(doc.get("sha256"), f"{process_id}.generated SHA-256")
            names.add(str(doc.get("relativePath")))
        _require("Sub Prep.pdf" in names,
                 f"{process_id} generated package lacks Sub Prep.pdf")

        idle = sorted(_metric_for_process(all_metric_rows, process_id, "idle"), key=lambda row: row.get("sampleIndex", -1))
        _require(len(idle) == IDLE_SAMPLE_COUNT, f"{process_id} idle interval must include exactly 301 samples")
        idle_ws: list[int] = []
        idle_private: list[int] = []
        idle_times: list[datetime] = []
        for index, sample in enumerate(idle):
            _require(sample.get("sampleIndex") == index, f"{process_id} idle sample indices are incomplete")
            _validate_external_sample(sample, process, f"{process_id}.idle[{index}]")
            idle_ws.append(_int(sample.get("currentWorkingSetBytes"), "idle working set", 1))
            idle_private.append(_int(sample.get("privateBytes"), "idle private bytes", 1))
            idle_times.append(_timestamp(sample.get("timestampUtc"), "idle timestamp"))
            if idle_ws[-1] >= THRESHOLD_BYTES:
                overall_budget_passed = False
        intervals = [(right - left).total_seconds() for left, right in zip(idle_times, idle_times[1:])]
        _require(all(MIN_INTERVAL_SECONDS <= interval <= MAX_INTERVAL_SECONDS for interval in intervals),
                 f"{process_id} idle sample cadence is outside 0.75–1.5 seconds")
        _require(process.get("idleSampleCount") == IDLE_SAMPLE_COUNT
                 and process.get("idleWorkingSetMaximumBytes") == max(idle_ws)
                 and process.get("idlePrivateBytesMedian") == int(statistics.median(idle_private)),
                 f"{process_id} idle summary differs from raw samples")
        _require(len(process.get("returnWorkingSetBytes", [])) == ACTION_CYCLES,
                 f"{process_id} did not retain every return-to-Workspace working-set value")
        return_values = [_int(value, f"{process_id}.returnWorkingSetBytes", 1) for value in process["returnWorkingSetBytes"]]
        if ready_memory["workingSetBytes"] >= THRESHOLD_BYTES:
            overall_budget_passed = False
        _require(return_values == [row["processSample"]["currentWorkingSetBytes"] for row in nav_rows[1::2]],
                 f"{process_id} return-to-Workspace summary differs from authentic route samples")
        _require(process.get("returnWorkingSetMaximumBytes") == max(return_values)
                 and process.get("returnWorkingSetAboveNormalThresholdCount") == sum(value >= THRESHOLD_BYTES for value in return_values),
                 f"{process_id} advisory return snapshot summary differs from raw values")
        expected_violations = []
        if ready_memory["workingSetBytes"] >= THRESHOLD_BYTES:
            expected_violations.append(("startup-ready", 0, ready_memory["workingSetBytes"], "synchronous-app-checkpoint"))
        for index, sample in enumerate(idle):
            value = int(sample["currentWorkingSetBytes"])
            if value >= THRESHOLD_BYTES:
                expected_violations.append(("idle", index, value, "external-process-snapshot"))
        actual_violations = []
        violations = process.get("budgetViolations")
        _require(isinstance(violations, list), f"{process_id} budget violation inventory is malformed")
        for violation in violations:
            violation = _obj(violation, f"{process_id}.budgetViolation")
            _require(violation.get("thresholdBytes") == THRESHOLD_BYTES
                     and _int(violation.get("workingSetBytes"), "budget violation working set", THRESHOLD_BYTES) >= THRESHOLD_BYTES,
                     f"{process_id} contains an invalid threshold violation")
            actual_violations.append((violation.get("phase"), violation.get("sampleIndex"),
                                      violation.get("workingSetBytes"), violation.get("metricSource")))
        _require(Counter(actual_violations) == Counter(expected_violations),
                 f"{process_id} strict startup/idle budget violation inventory differs from raw samples")
        calculated_process_pass = not expected_violations
        _require(process.get("normalUseBudgetPassed") is calculated_process_pass,
                 f"{process_id} strict startup/idle gate classification is inconsistent")
        if not calculated_process_pass:
            overall_budget_passed = False

        summaries.append(
            {
                "processInstanceId": process_id,
                "pid": pid,
                "startupReadyWorkingSetBytes": ready_memory["workingSetBytes"],
                "generationWorkingSetBytes": generation_memory["workingSetBytes"],
                "processPeakWorkingSetBytesAtGenerationCompletion": generation_memory["peakWorkingSetBytes"],
                "generationPeakExceedsAdvisoryCeiling": generation_memory["peakWorkingSetBytes"] > 536_870_912,
                "postGenerationWorkingSetBytes": {
                    row["record"].get("details", {}).get("delay"): _validate_app_snapshot(row["record"], "post-generation")["workingSetBytes"]
                    for row in post_generation
                },
                "returnWorkingSetBytes": return_values,
                "returnWorkingSetMaximumBytes": max(return_values),
                "returnWorkingSetAboveNormalThresholdCount": sum(value >= THRESHOLD_BYTES for value in return_values),
                "pageSuspendedWorkingSetMaximumBytes": max(sample["currentWorkingSetBytes"] for sample in suspended_metric_samples),
                "pageReleaseWorkingSetMaximumBytes": max(sample["currentWorkingSetBytes"] for sample in release_metric_samples),
                "idleWorkingSetMaximumBytes": max(idle_ws),
                "idlePrivateBytesMedian": int(statistics.median(idle_private)),
                "idleHandlesMedian": int(statistics.median([row["handles"] for row in idle])),
                "idleThreadsMedian": int(statistics.median([row["threads"] for row in idle])),
                "budgetPassed": process.get("normalUseBudgetPassed"),
                "budgetViolationCount": len(process.get("budgetViolations", [])),
                "generatedDocumentCount": len(docs),
            }
        )

    def spread(key: str) -> dict[str, int]:
        values = [int(row[key]) for row in summaries]
        return {"min": min(values), "median": int(statistics.median(values)), "max": max(values), "range": max(values) - min(values)}

    return {
        "schema": SUMMARY_SCHEMA,
        "taskId": TASK_ID,
        "validationStatus": "valid",
        "gateResult": "passed" if overall_budget_passed else "failed",
        "workingSetThresholdBytesExclusive": THRESHOLD_BYTES,
        "strictNormalUseBudgetPassed": overall_budget_passed,
        "processCount": len(summaries),
        "repeatedSubPrepCyclesPerProcess": REPEATED_CYCLES,
        "idleSampleCountPerProcess": IDLE_SAMPLE_COUNT,
        "processSummaries": summaries,
        "runToRunSpread": {
            "startupReadyWorkingSetBytes": spread("startupReadyWorkingSetBytes"),
            "generationWorkingSetBytes": spread("generationWorkingSetBytes"),
            "processPeakWorkingSetBytesAtGenerationCompletion": spread("processPeakWorkingSetBytesAtGenerationCompletion"),
            "idleWorkingSetMaximumBytes": spread("idleWorkingSetMaximumBytes"),
            "idlePrivateBytesMedian": spread("idlePrivateBytesMedian"),
        },
    }


def validate_run_directory(run_directory: Path) -> dict[str, Any]:
    manifest = _obj(_read_json(run_directory / "manifest.json", "manifest"), "manifest")
    if manifest.get("status") != "completed":
        raise EvidenceValidationError("run did not complete all required measurement arms")
    source_inventory = _obj(_read_json(run_directory / "source-files.json", "source inventory"), "source inventory")
    package_inventory = _obj(_read_json(run_directory / "package-files.json", "package inventory"), "package inventory")
    root = Path(str(manifest.get("repositoryRoot", ""))).resolve()
    _require(root.is_dir(), "manifest repositoryRoot is missing")
    source_files, source_sha = _validate_inventory_files(source_inventory.get("files"), "source inventory", root)
    expected_sources = _source_inventory_from_tree(root)
    _require(source_files == expected_sources, "source inventory does not cover the complete canonical source roots")
    package_directory = Path(str(manifest.get("package", {}).get("directory", ""))).resolve()
    _require(package_directory.is_dir(), "preserved package directory is missing")
    package_files, package_sha = _validate_inventory_files(package_inventory.get("files"), "package inventory", package_directory)
    expected_package = []
    for path in package_directory.rglob("*"):
        if path.is_file():
            expected_package.append({"path": path.relative_to(package_directory).as_posix(), "sizeBytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    expected_package.sort(key=lambda row: row["path"].casefold())
    _require(package_files == expected_package, "package inventory does not cover every staged package file")
    configuration = _obj(manifest.get("configuration"), "configuration")
    cache_path = Path(str(configuration.get("cmakeCachePath", ""))).resolve()
    _require(cache_path.is_file(), "Release CMakeCache.txt is missing from its recorded path")
    cache_digest = hashlib.sha256(cache_path.read_bytes()).hexdigest()
    _require(cache_digest == configuration.get("cmakeCacheSha256")
             == configuration.get("cmakeCacheSha256AtRunEnd"), "Release CMakeCache hash changed or was misreported")
    cache_values: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith(("//", "#")) or ":" not in line or "=" not in line:
            continue
        name, value = line.split("=", 1)
        cache_values[name.split(":", 1)[0]] = value
    _require(Path(cache_values.get("CMAKE_HOME_DIRECTORY", "")).resolve() == root,
             "Release CMakeCache source directory is not the recorded repository")
    _require(cache_values.get("CLASSMNGR_NEXT_RELEASE_PACKAGE") == "ON",
             "Release package option is disabled in the actual CMakeCache")
    compiler = _obj(configuration.get("releaseCompiler"), "releaseCompiler")
    _require(cache_values.get("CMAKE_C_FLAGS_RELEASE") == compiler.get("cFlagsRelease", {}).get("raw")
             and cache_values.get("CMAKE_CXX_FLAGS_RELEASE") == compiler.get("cxxFlagsRelease", {}).get("raw"),
             "Release compiler flags differ from the actual CMakeCache")
    qt = _obj(manifest.get("qtInstallation"), "Qt installation")
    cache_qt_dir = Path(cache_values.get("Qt6_DIR", "")).resolve()
    configured_prefix = Path(str(qt.get("prefix", ""))).resolve()
    _require(cache_values.get("CMAKE_GENERATOR") == compiler.get("generator")
             and cache_qt_dir == Path(str(qt.get("qt6Directory", ""))).resolve()
             and cache_qt_dir.parents[2] == configured_prefix,
             "Release CMakeCache Qt/generator identity differs from the recorded build")
    project_path = Path(str(compiler.get("projectPath", ""))).resolve()
    _require(project_path.is_file() and hashlib.sha256(project_path.read_bytes()).hexdigest() == compiler.get("projectSha256"),
             "generated Release project file is missing or changed")
    try:
        project = ET.parse(project_path).getroot()
    except (ET.ParseError, OSError) as error:
        raise EvidenceValidationError(f"cannot parse generated Release project: {error}") from error
    ns = {"msbuild": "http://schemas.microsoft.com/developer/msbuild/2003"}
    release_optimization = [node.text.strip() for group in project.findall(".//msbuild:ItemDefinitionGroup", ns)
                            if re.search(r"'\$\(Configuration\)\|\$\(Platform\)'\s*==\s*'Release\|x64'", group.get("Condition", ""))
                            for node in group.findall("./msbuild:ClCompile/msbuild:Optimization", ns) if node.text]
    _require(release_optimization == [compiler.get("projectOptimizationSetting")]
             and release_optimization[0] in {"MaxSpeed", "MinSpace"},
             "generated Release|x64 project does not map to an enabled optimization setting")
    build_log_path = Path(str(configuration.get("packageBuild", {}).get("logPath", ""))).resolve()
    _require(build_log_path.is_file()
             and hashlib.sha256(build_log_path.read_bytes()).hexdigest() == configuration.get("packageBuild", {}).get("logSha256"),
             "Release package prerequisite build log is missing or changed")
    if configuration.get("packageBuild", {}).get("mode") == "verified-prebuilt-reuse":
        reuse = _obj(configuration["packageBuild"].get("reuse"), "prebuilt reuse attestation")
        prepared_manifest_path = Path(str(reuse.get("preparedManifestPath", ""))).resolve()
        _require(prepared_manifest_path == run_directory.resolve() / "prepared-release.json"
                 and prepared_manifest_path.is_file()
                 and hashlib.sha256(prepared_manifest_path.read_bytes()).hexdigest() == reuse.get("preparedManifestSha256"),
                 "copied elevated prepared Release manifest is missing or changed")
        prepared = _obj(_read_json(prepared_manifest_path, "prepared Release manifest"), "prepared Release manifest")
        _require(prepared.get("schema") == "classmngr-phase9-p908-prepared-release-v1"
                 and prepared.get("integrityLevel") == "elevated"
                 and prepared.get("integrityLevelEvidence") == reuse.get("integrityLevelEvidence")
                 and prepared.get("source", {}).get("sha256") == reuse.get("preparedSourceSha256")
                 and prepared.get("cache", {}).get("sha256") == reuse.get("preparedCMakeCacheSha256")
                 and prepared.get("package", {}).get("sha256") == reuse.get("preparedPackageSha256")
                 and prepared.get("application", {}).get("sha256") == reuse.get("preparedApplicationSha256")
                 and prepared.get("fixtureGenerator", {}).get("sha256") == reuse.get("preparedFixtureGeneratorSha256")
                 and prepared.get("fixture", {}).get("sha256") == reuse.get("preparedFixtureSha256"),
                 "prepared Release manifest identities differ from the standard-integrity run attestation")
        _require(prepared.get("build", {}).get("succeeded") is True
                 and prepared.get("build", {}).get("exitCode") == 0
                 and tuple(prepared.get("build", {}).get("targets", ())) == REQUIRED_BUILD_TARGETS
                 and prepared.get("build", {}).get("logSha256") == configuration.get("packageBuild", {}).get("logSha256"),
                 "elevated prepared build target/log evidence is invalid")

    package = _obj(manifest.get("package"), "package")
    app = _obj(manifest.get("application"), "application")
    app_path = Path(str(app.get("canonicalPath", ""))).resolve()
    _require(app_path == package_directory / "ClassMngrNext.exe" and app_path.is_file()
             and hashlib.sha256(app_path.read_bytes()).hexdigest() == app.get("sha256"),
             "packaged Next executable is missing or changed")
    release_executable = Path(str(app.get("releaseExecutablePath", ""))).resolve()
    _require(release_executable.is_file()
             and hashlib.sha256(release_executable.read_bytes()).hexdigest() == app.get("releaseExecutableSha256") == app.get("sha256"),
             "packaged executable differs from the Release build output")
    windeploy = Path(str(qt.get("windeployqtPath", ""))).resolve()
    _require(windeploy.is_file() and hashlib.sha256(windeploy.read_bytes()).hexdigest() == qt.get("windeployqtSha256"),
             "CMakeCache-bound windeployqt executable is missing or changed")
    for runtime in qt.get("runtimeFiles", []):
        runtime = _obj(runtime, "Qt runtime file")
        source = Path(str(runtime.get("qtPrefixPath", ""))).resolve()
        _require(source.is_file() and source.stat().st_size == runtime.get("sizeBytes")
                 and hashlib.sha256(source.read_bytes()).hexdigest() == runtime.get("sha256"),
                 f"configured Qt runtime changed: {runtime.get('packagePath')}")
    for executable in qt.get("fixtureGenerators", []):
        executable = _obj(executable, "fixture generator Qt identity")
        path = Path(str(executable.get("executablePath", ""))).resolve()
        _require(path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == executable.get("executableSha256"),
                 "representative fixture generator is missing or changed")
        for dll in executable.get("colocatedQtDlls", []):
            dll = _obj(dll, "fixture generator Qt DLL")
            colocated = path.parent / str(dll.get("fileName", ""))
            _require(colocated.is_file() and colocated.stat().st_size == dll.get("sizeBytes")
                     and hashlib.sha256(colocated.read_bytes()).hexdigest() == dll.get("sha256"),
                     "fixture generator colocated Qt DLL changed")

    fixture = _obj(manifest.get("fixture"), "fixture")
    master_path = Path(str(fixture.get("masterPath", ""))).resolve()
    try:
        master_path.relative_to(run_directory.resolve())
    except ValueError as error:
        raise EvidenceValidationError("fixture master is outside the unique run directory") from error
    _require(master_path.is_file() and hashlib.sha256(master_path.read_bytes()).hexdigest() == fixture.get("sha256"),
             "representative fixture master is missing or changed")
    fixture_copies = fixture.get("perProcessCopies", [])
    _require(len(fixture_copies) == len(EXPECTED_PROCESSES), "per-process fixture copy list is incomplete")
    seen_fixture_paths: set[Path] = set()
    for fixture_copy in fixture_copies:
        fixture_copy = _obj(fixture_copy, "per-process fixture copy")
        copy_path = Path(str(fixture_copy.get("path", ""))).resolve()
        try:
            copy_path.relative_to(run_directory.resolve())
        except ValueError as error:
            raise EvidenceValidationError("per-process fixture copy is outside the unique run directory") from error
        _require(copy_path not in seen_fixture_paths and copy_path.is_file(),
                 "per-process fixture copy is missing or duplicated")
        seen_fixture_paths.add(copy_path)
        snapshot_path = Path(str(fixture_copy.get("preLaunchSnapshotPath", ""))).resolve()
        try:
            snapshot_path.relative_to(run_directory.resolve())
        except ValueError as error:
            raise EvidenceValidationError("pre-launch fixture snapshot is outside the unique run directory") from error
        _require(snapshot_path != copy_path and snapshot_path.is_file()
                 and hashlib.sha256(snapshot_path.read_bytes()).hexdigest() == fixture_copy.get("preLaunchSnapshotSha256")
                 == fixture_copy.get("sha256BeforeLaunch"),
                 "pre-launch fixture snapshot is missing or differs from the recorded transform output")
    for process in manifest.get("processRuns", []):
        process = _obj(process, "process run")
        output_root = Path(str(process.get("outputRoot", ""))).resolve()
        try:
            output_root.relative_to(run_directory.resolve())
        except ValueError as error:
            raise EvidenceValidationError("generated output root is outside the unique run directory") from error
        _require(output_root.is_dir(), "unique per-process output root is missing")
        output_directory = Path(str(process.get("outputDirectory", ""))).resolve()
        try:
            output_directory.relative_to(output_root)
        except ValueError as error:
            raise EvidenceValidationError("generated output directory escaped its unique process root") from error
        fixture_copy = next((row for row in fixture_copies if row.get("processInstanceId") == process.get("processInstanceId")), None)
        _require(fixture_copy is not None and process.get("fixturePathPassedToApp") == fixture_copy.get("path")
                 and process.get("fixturePreLaunchSnapshotPath") == fixture_copy.get("preLaunchSnapshotPath")
                 and process.get("fixturePreLaunchSnapshotSha256") == fixture_copy.get("preLaunchSnapshotSha256"),
                 "measured process fixture path/pre-launch hash differs from the preserved fixture copy")
        copy_path = Path(str(fixture_copy.get("path", ""))).resolve()
        _require(copy_path.is_file()
                 and hashlib.sha256(copy_path.read_bytes()).hexdigest() == process.get("fixtureHashAtExit"),
                 "post-exit fixture file does not match its recorded SHA-256")
    startup = _read_jsonl(run_directory / "startup-log.jsonl", "startup log")
    metrics = _read_jsonl(run_directory / "process-metrics.jsonl", "process metrics")
    navigation = _read_jsonl(run_directory / "navigation-metrics.jsonl", "navigation metrics")
    uia = _read_jsonl(run_directory / "uia-actions.jsonl", "UIA actions")
    generated = _obj(_read_json(run_directory / "generated-files.json", "generated file inventory"), "generated file inventory")
    summary = validate_documents(manifest, source_inventory, package_inventory, startup, metrics, navigation, uia, generated)
    for package in generated.get("packages", []):
        output_directory = Path(str(package.get("outputDirectory", ""))).resolve()
        _require(output_directory.is_dir(), f"generated output directory is missing: {output_directory}")
        for row in package.get("documents", []):
            path = Path(str(row.get("path", ""))).resolve()
            try:
                path.relative_to(output_directory)
            except ValueError as error:
                raise EvidenceValidationError("generated PDF escaped its unique output directory") from error
            _require(path.is_file() and path.stat().st_size == row.get("sizeBytes"), f"generated PDF is missing or has changed: {path}")
            _require(hashlib.sha256(path.read_bytes()).hexdigest() == row.get("sha256"), f"generated PDF hash mismatch: {path}")
            _require(path.read_bytes()[:5] == b"%PDF-", f"generated output is not a PDF: {path}")
    return summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-directory", required=True, type=Path)
    args = parser.parse_args(argv)
    run_directory = args.run_directory.resolve()
    summary_path = run_directory / "summary.json"
    output_path = run_directory / "validator-output.txt"
    try:
        summary = validate_run_directory(run_directory)
        message = json.dumps(summary, indent=2, sort_keys=True)
        summary_path.write_text(message + "\n", encoding="utf-8")
        output_path.write_text("Evidence valid; gate " + summary["gateResult"] + ".\n", encoding="utf-8")
        print(f"P9-08 evidence valid; gate {summary['gateResult']}.")
        return 0 if summary["gateResult"] == "passed" else 1
    except EvidenceValidationError as error:
        failed = {
            "schema": SUMMARY_SCHEMA,
            "taskId": TASK_ID,
            "validationStatus": "not-evaluated",
            "gateResult": "not-evaluated",
            "error": str(error),
        }
        try:
            summary_path.write_text(json.dumps(failed, indent=2) + "\n", encoding="utf-8")
            output_path.write_text("Evidence not evaluated: " + str(error) + "\n", encoding="utf-8")
        except OSError:
            pass
        print(f"P9-08 evidence not evaluated: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
