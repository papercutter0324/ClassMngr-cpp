#!/usr/bin/env python3
"""Validate packaged Phase 0 evidence without changing it.

The validator deliberately treats the retained 250 MiB and temporary 512 MiB
memory comparisons as trend data.  Missing files, malformed JSON, abnormal
process completion, and lifecycle-contract violations are failures; evidence
for unofficial ports is reported as deferred metadata instead.  By default,
the process exit code represents the supplied run(s), not the whole Phase 0
gate.  Use --require-exit-gate when automation must fail until every supported
platform has passing evidence.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import tempfile
import unittest
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


TASK_ID = "QT0-EVIDENCE-AUTOMATION"
LEGACY_TASK_IDS = ("QT0-AUTO-ORCHESTRATOR",)
SCHEMA = "classmngr-phase0-evidence-run-v1"
SUMMARY_SCHEMA = "classmngr-phase0-exit-gate-summary-v1"
VALIDATOR_VERSION = "1.0.0"
SUPPORTED_PLATFORM = "windows-x64"
SUPPORTED_PLATFORMS = ("windows-x64", "macos-universal")
DEFERRED_PLATFORMS = ("windows-arm64", "linux")
LEGACY_RESIDENT_TARGET_BYTES = 250 * 1024 * 1024
TEMPORARY_DIAGNOSTIC_CEILING_BYTES = 512 * 1024 * 1024


@dataclass(frozen=True)
class RouteSpec:
    route_id: str
    category: str
    artifact_path: str
    required_files: tuple[str, ...] = ()
    required_globs: tuple[str, ...] = ()
    metrics_file: str | None = None
    trace_file: str | None = None
    manifest_required: bool = False
    checkpoint_sequence: tuple[str, ...] = ()
    event_sequence: tuple[str, ...] = ()
    png_files: tuple[str, ...] = ()
    minimum_pdf_files: int = 0
    minimum_zip_files: int = 0
    minimum_png_files: int = 0
    resource_trace_file: str | None = None
    release_prefixes: tuple[str, ...] = ()
    expected_failure: bool = False


VARIANTS = ("english-light", "english-dark", "korean-light", "korean-dark")
WORKFLOW_CHECKPOINTS = (
    "startup-complete",
    "pdf-opened",
    "pdf-released",
    "pdf-reopened",
    "pdf-released-after-reopen",
    "pdf-workflow-complete",
    "workflow-complete",
    "settled-1s",
)


def _visual_empty_specs() -> list[RouteSpec]:
    return [
        RouteSpec(
            route_id=f"visual-empty-{variant}",
            category="visual",
            artifact_path=f"visual/empty/{variant}",
            required_files=("route-manifest.json", "startup-complete.png", "startup-metrics.json"),
            metrics_file="startup-metrics.json",
            png_files=("startup-complete.png",),
        )
        for variant in VARIANTS
    ]


ROUTE_SPECS: tuple[RouteSpec, ...] = tuple(
    _visual_empty_specs()
    + [
        RouteSpec(
            "fixture-representative",
            "fixture",
            "fixtures/representative",
            required_files=("route-manifest.json", "representative-startup.tps"),
        ),
        RouteSpec(
            "visual-representative",
            "visual",
            "visual/representative",
            required_files=("route-manifest.json",),
            required_globs=tuple(
                f"{variant}/startup-complete.png" for variant in VARIANTS
            ),
            png_files=tuple(
                f"{variant}/startup-complete.png" for variant in VARIANTS
            ),
        ),
        RouteSpec(
            "visual-classes",
            "visual",
            "visual/classes",
            required_files=(
                "route-manifest.json",
                "manifest.json",
                "generated-large-classes-visual.tps",
            ),
            required_globs=tuple(
                f"{variant}/classes-entry.png" for variant in VARIANTS
            )
            + tuple(
                f"{variant}/classes-selected-1.png" for variant in VARIANTS
            )
            + tuple(
                f"{variant}/classes-selected-96.png" for variant in VARIANTS
            )
            + tuple(f"{variant}/metrics.json" for variant in VARIANTS)
            + tuple(f"{variant}/workflow-trace.txt" for variant in VARIANTS),
            png_files=tuple(
                f"{variant}/{name}.png"
                for variant in VARIANTS
                for name in ("classes-entry", "classes-selected-1", "classes-selected-96")
            ),
        ),
        RouteSpec(
            "visual-sub-prep",
            "visual",
            "visual/sub-prep",
            required_files=(
                "route-manifest.json",
                "manifest.json",
                "generated-large-sub-prep-visual.tps",
                "generated-large-sub-prep-empty.tps",
            ),
            required_globs=tuple(
                f"{variant}/{name}.png"
                for variant in VARIANTS
                for name in (
                    "startup-complete",
                    "sub-prep-selected",
                    "sub-prep-changed-selection",
                    "sub-prep-editing-read-only",
                )
            )
            + tuple(
                f"{variant}/metrics.json" for variant in VARIANTS
            )
            + tuple(f"{variant}/workflow-trace.txt" for variant in VARIANTS)
            + ("empty/startup-complete.png", "empty/sub-prep-empty.png", "empty/metrics.json", "empty/workflow-trace.txt"),
            png_files=tuple(
                f"{variant}/{name}.png"
                for variant in VARIANTS
                for name in (
                    "startup-complete",
                    "sub-prep-selected",
                    "sub-prep-changed-selection",
                    "sub-prep-editing-read-only",
                )
            )
            + ("empty/startup-complete.png", "empty/sub-prep-empty.png"),
        ),
        RouteSpec(
            "visual-pdf-viewer",
            "visual",
            "visual/pdf-viewer",
            required_files=(
                "route-manifest.json",
                "manifest.json",
                "generated-large-pdf-viewer-visual.tps",
                "large-pdf-viewer-visual-workflow.json",
                "workflow-trace.txt",
            ),
            metrics_file="large-pdf-viewer-visual-workflow.json",
            trace_file="workflow-trace.txt",
            png_files=tuple(
                f"pdf-{name}.png"
                for name in ("catalog", "opened", "closed", "error", "reopened")
            ),
            checkpoint_sequence=WORKFLOW_CHECKPOINTS,
        ),
        RouteSpec(
            "startup-empty",
            "memory",
            "startup/empty",
            required_files=("route-manifest.json", "startup-metrics.json"),
            metrics_file="startup-metrics.json",
            checkpoint_sequence=("startup-complete", "settled-1s"),
        ),
        RouteSpec(
            "startup-representative",
            "memory",
            "startup/representative",
            required_files=(
                "route-manifest.json",
                "startup-metrics.json",
                "startup-complete.png",
                "settled-final.png",
            ),
            metrics_file="startup-metrics.json",
            png_files=("startup-complete.png", "settled-final.png"),
            checkpoint_sequence=("startup-complete", "settled-5s"),
        ),
        RouteSpec(
            "workflow-representative",
            "workflow",
            "workflow/representative",
            required_files=(
                "route-manifest.json",
                "startup-metrics.json",
                "workflow-trace.txt",
                "captures/startup-complete.png",
                "captures/settled-final.png",
                "pdf-captures/pdf-opened.png",
                "pdf-captures/pdf-reopened.png",
            ),
            metrics_file="startup-metrics.json",
            trace_file="workflow-trace.txt",
            png_files=(
                "captures/startup-complete.png",
                "captures/settled-final.png",
                "pdf-captures/pdf-opened.png",
                "pdf-captures/pdf-reopened.png",
            ),
            checkpoint_sequence=WORKFLOW_CHECKPOINTS,
            event_sequence=(
                "page-enter",
                "page-leave",
                "pdf-document-loaded",
                "pdf-document-released",
            ),
        ),
        RouteSpec(
            "lifecycle-sub-prep",
            "memory",
            "memory/sub-prep",
            required_files=("route-manifest.json", "manifest.json", "large-sub-prep-workflow.json", "workflow-trace.txt"),
            metrics_file="large-sub-prep-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            checkpoint_sequence=(
                "sub-prep-lifecycle-entry",
                "sub-prep-selection-96",
                "sub-prep-refresh-1-start",
                "sub-prep-refresh-1-complete",
                "sub-prep-left-1",
                "sub-prep-reentry-1",
                "sub-prep-refresh-2-start",
                "sub-prep-refresh-2-complete",
                "sub-prep-left-2",
                "sub-prep-reentry-2",
                "sub-prep-lifecycle-complete",
            ),
            release_prefixes=("subPrepOutput",),
        ),
        RouteSpec(
            "lifecycle-classes",
            "memory",
            "memory/classes",
            required_files=("route-manifest.json", "manifest.json", "large-classes-workflow.json", "workflow-trace.txt"),
            metrics_file="large-classes-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            checkpoint_sequence=(
                "classes-lifecycle-entry",
                "classes-selection-96",
                "classes-refresh-1-start",
                "classes-refresh-1-complete",
                "classes-left-1",
                "classes-reentry-1",
                "classes-refresh-2-start",
                "classes-refresh-2-complete",
                "classes-left-2",
                "classes-reentry-2",
                "classes-lifecycle-complete",
            ),
        ),
        RouteSpec(
            "lifecycle-schedule",
            "memory",
            "memory/schedule",
            required_files=("route-manifest.json", "manifest.json", "large-schedule-workflow.json", "workflow-trace.txt"),
            metrics_file="large-schedule-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            checkpoint_sequence=(
                "schedule-lifecycle-entry",
                "schedule-refresh-1-start",
                "schedule-refresh-1-complete",
                "schedule-left-1",
                "schedule-reentry-1",
                "schedule-refresh-2-start",
                "schedule-refresh-2-complete",
                "schedule-left-2",
                "schedule-reentry-2",
                "schedule-lifecycle-complete",
            ),
        ),
        RouteSpec(
            "lifecycle-schedule-import",
            "output",
            "memory/schedule-import",
            required_files=("route-manifest.json", "manifest.json", "large-schedule-import-workflow.json", "workflow-trace.txt", "generated-large-schedule-import.xlsx"),
            metrics_file="large-schedule-import-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            checkpoint_sequence=(
                "schedule-import-dialog-opened",
                "schedule-import-loading",
                "schedule-import-parse-complete",
                "schedule-import-review-ready",
                "schedule-import-cancel-start",
                "schedule-import-post-review-release",
                "schedule-import-post-release",
                "schedule-import-operation-end",
            ),
            event_sequence=(
                "schedule-import-operation-start",
                "schedule-import-operation-cancelled",
                "schedule-import-review-released",
                "schedule-import-operation-released",
            ),
            release_prefixes=("scheduleImport",),
        ),
        RouteSpec(
            "lifecycle-schedule-import-apply",
            "output",
            "memory/schedule-import-apply",
            required_files=("route-manifest.json", "manifest.json", "large-schedule-import-workflow.json", "workflow-trace.txt", "generated-large-schedule-import.xlsx"),
            metrics_file="large-schedule-import-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            checkpoint_sequence=(
                "schedule-import-dialog-opened",
                "schedule-import-loading",
                "schedule-import-parse-complete",
                "schedule-import-review-ready",
                "schedule-import-apply-start",
                "schedule-import-apply-complete",
                "schedule-import-post-review-release",
                "schedule-import-post-release",
                "schedule-import-page-refreshed",
                "schedule-import-operation-end",
            ),
            event_sequence=(
                "schedule-import-operation-start",
                "schedule-import-operation-applied",
                "schedule-import-review-released",
                "schedule-import-operation-released",
            ),
            release_prefixes=("scheduleImport",),
        ),
        RouteSpec(
            "lifecycle-calendar-import",
            "output",
            "memory/calendar-import",
            required_files=("route-manifest.json", "manifest.json", "large-calendar-import-workflow.json", "workflow-trace.txt", "generated-large-calendar-import.xlsx"),
            metrics_file="large-calendar-import-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            checkpoint_sequence=(
                "calendar-import-operation-start",
                "calendar-import-loading",
                "calendar-import-response-received",
                "calendar-import-workbook-parsed",
                "calendar-import-events-prepared",
                "calendar-import-operation-applied",
                "calendar-import-operation-released",
                "calendar-import-page-refreshed",
                "calendar-import-operation-end",
            ),
            event_sequence=(
                "calendar-import-operation-start",
                "calendar-import-workbook-parsed",
                "calendar-import-operation-applied",
                "calendar-import-operation-released",
            ),
            release_prefixes=("calendarImport",),
        ),
        RouteSpec(
            "lifecycle-calendar-import-error",
            "output",
            "memory/calendar-import-error",
            required_files=("route-manifest.json", "manifest.json", "large-calendar-import-workflow.json", "workflow-trace.txt", "generated-large-calendar-import.xlsx", "malformed-calendar-import-response.bin"),
            metrics_file="large-calendar-import-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            checkpoint_sequence=(
                "calendar-import-operation-start",
                "calendar-import-loading",
                "calendar-import-response-received",
                "calendar-import-operation-failed",
                "calendar-import-operation-released",
                "calendar-import-failure-observed",
                "calendar-import-operation-end",
            ),
            event_sequence=(
                "calendar-import-operation-start",
                "calendar-import-operation-failed",
                "calendar-import-operation-released",
            ),
            release_prefixes=("calendarImport",),
            expected_failure=True,
        ),
        RouteSpec(
            "lifecycle-class-transfer",
            "memory",
            "memory/class-transfer",
            required_files=("route-manifest.json", "manifest.json", "large-class-transfer-workflow.json", "workflow-trace.txt", "generated-large-class-transfer.json"),
            metrics_file="large-class-transfer-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            checkpoint_sequence=(
                "class-transfer-operation-start",
                "class-transfer-package-loaded",
                "class-transfer-preview-prepared",
                "class-transfer-dialog-opened",
                "class-transfer-apply-start",
                "class-transfer-dialog-released",
                "class-transfer-operation-applied",
                "class-transfer-operation-released",
                "class-transfer-post-release",
                "class-transfer-page-refreshed",
                "class-transfer-operation-end",
            ),
            event_sequence=(
                "class-transfer-operation-start",
                "class-transfer-package-loaded",
                "class-transfer-dialog-released",
                "class-transfer-operation-applied",
                "class-transfer-operation-released",
            ),
            release_prefixes=("classTransfer",),
        ),
        RouteSpec(
            "lifecycle-speaking-evaluation",
            "output",
            "output/speaking-evaluation",
            required_files=("route-manifest.json", "manifest.json", "large-speaking-evaluation-workflow.json", "workflow-trace.txt", "generated-large-speaking-evaluation.tps"),
            metrics_file="large-speaking-evaluation-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            png_files=(
                "speaking-evaluation-page.png",
                "speaking-evaluation-report.png",
                "speaking-evaluation-export-dialog.png",
                "speaking-evaluation-ai-review.png",
                "speaking-evaluation-powerpoint-renderer.png",
            ),
            minimum_pdf_files=1,
            minimum_zip_files=1,
            checkpoint_sequence=(
                "speaking-evaluation-operation-start",
                "speaking-evaluation-page-prepared",
                "speaking-evaluation-reports-prepared",
                "speaking-evaluation-report-dialog-released",
                "speaking-evaluation-export-dialog-released",
                "speaking-evaluation-export-complete",
                "speaking-evaluation-ai-dialog-released",
                "speaking-evaluation-operation-released",
                "speaking-evaluation-page-refreshed",
            ),
            event_sequence=(
                "speaking-evaluation-operation-start",
                "speaking-evaluation-reports-prepared",
                "speaking-evaluation-export-complete",
                "speaking-evaluation-operation-released",
            ),
            release_prefixes=("speakingEval",),
        ),
        RouteSpec(
            "lifecycle-staff-directory",
            "memory",
            "memory/staff-directory",
            required_files=("route-manifest.json", "manifest.json", "large-staff-directory-workflow.json", "workflow-trace.txt", "generated-large-staff-directory.tps"),
            metrics_file="large-staff-directory-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            png_files=(
                "staff-directory-native-english-teachers.png",
                "staff-directory-gs-team.png",
            ),
            checkpoint_sequence=(
                "staff-directory-native-operation-start",
                "staff-directory-native-page-prepared",
                "staff-directory-native-refresh-1",
                "staff-directory-native-refresh-2",
                "staff-directory-native-page-left",
                "staff-directory-native-page-reentered",
                "staff-directory-native-operation-released",
                "staff-directory-gs-operation-start",
                "staff-directory-gs-page-prepared",
                "staff-directory-gs-refresh-1",
                "staff-directory-gs-refresh-2",
                "staff-directory-gs-page-left",
                "staff-directory-gs-page-reentered",
                "staff-directory-gs-operation-released",
            ),
            event_sequence=(
                "staff-directory-native-operation-start",
                "staff-directory-native-operation-released",
                "staff-directory-gs-operation-start",
                "staff-directory-gs-operation-released",
            ),
            release_prefixes=("staffDirectory",),
        ),
        RouteSpec(
            "output-sub-prep",
            "output",
            "output/sub-prep",
            required_files=("route-manifest.json", "manifest.json", "large-sub-prep-output-workflow.json", "workflow-trace.txt", "generated-large-sub-prep-output.tps"),
            metrics_file="large-sub-prep-output-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            minimum_pdf_files=2,
            minimum_png_files=3,
            checkpoint_sequence=(
                "sub-prep-output-operation-start",
                "sub-prep-output-validation-error",
                "sub-prep-output-generated",
                "sub-prep-output-operation-released",
            ),
            event_sequence=(
                "sub-prep-output-operation-start",
                "sub-prep-output-generated",
                "sub-prep-output-operation-released",
            ),
            release_prefixes=("subPrepOutput",),
        ),
        RouteSpec(
            "resource-trace",
            "memory",
            "memory/resource-trace",
            required_files=("route-manifest.json", "manifest.json", "large-resource-trace-workflow.json", "workflow-trace.txt", "generated-large-resource-trace.tps", "resource-trace.json"),
            metrics_file="large-resource-trace-workflow.json",
            trace_file="workflow-trace.txt",
            manifest_required=True,
            resource_trace_file="resource-trace.json",
            checkpoint_sequence=("resource-trace-start", "resource-trace-complete"),
        ),
    ]
)

ROUTE_BY_ID = {spec.route_id: spec for spec in ROUTE_SPECS}
ALL_ROUTE_IDS = tuple(spec.route_id for spec in ROUTE_SPECS)
ROUTES_BY_CATEGORY: dict[str, tuple[str, ...]] = {
    category: tuple(spec.route_id for spec in ROUTE_SPECS if spec.category == category)
    for category in {spec.category for spec in ROUTE_SPECS}
}


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _as_bool(value: Any) -> bool | None:
    if isinstance(value, bool):
        return value
    return None


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _inside(root: Path, candidate: Path) -> bool:
    try:
        candidate.relative_to(root)
        return True
    except ValueError:
        return False


def _resolve_inside(root: Path, value: str) -> Path | None:
    raw = Path(value)
    candidate = raw.resolve() if raw.is_absolute() else (root / raw).resolve()
    return candidate if _inside(root, candidate) else None


def _append_issue(
    issues: list[dict[str, Any]],
    code: str,
    message: str,
    path: Path | None = None,
) -> None:
    issue: dict[str, Any] = {"code": code, "message": message}
    if path is not None:
        issue["path"] = str(path)
    issues.append(issue)


def _load_json(
    path: Path,
    failures: list[dict[str, Any]],
    json_errors: list[str],
) -> Any | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        detail = f"{path}: {error}"
        json_errors.append(detail)
        _append_issue(failures, "invalid-json", f"Invalid JSON: {error}", path)
        return None


def _required_files(
    route_root: Path,
    spec: RouteSpec,
    failures: list[dict[str, Any]],
) -> list[Path]:
    files: list[Path] = []
    for relative in spec.required_files + spec.required_globs:
        candidate = _resolve_inside(route_root, relative)
        if candidate is None:
            _append_issue(
                failures,
                "unsafe-artifact-path",
                f"Required artifact path escapes route root: {relative}",
                route_root,
            )
            continue
        if not candidate.is_file():
            _append_issue(
                failures,
                "missing-artifact",
                f"Required evidence file is missing: {relative}",
                candidate,
            )
        else:
            if candidate.stat().st_size <= 0:
                _append_issue(
                    failures,
                    "empty-artifact",
                    f"Required evidence file is empty: {relative}",
                    candidate,
                )
            files.append(candidate)
    return files


def _check_png(path: Path, failures: list[dict[str, Any]]) -> None:
    try:
        data = path.read_bytes()
    except OSError as error:
        _append_issue(failures, "unreadable-artifact", str(error), path)
        return
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        _append_issue(failures, "invalid-visual-artifact", "PNG signature or IHDR is invalid.", path)
        return
    width, height = struct.unpack(">II", data[16:24])
    if width <= 0 or height <= 0:
        _append_issue(failures, "invalid-visual-artifact", "PNG dimensions must be positive.", path)


def _check_pdf(path: Path, failures: list[dict[str, Any]]) -> None:
    try:
        header = path.read_bytes()[:5]
    except OSError as error:
        _append_issue(failures, "unreadable-artifact", str(error), path)
        return
    if header != b"%PDF-":
        _append_issue(failures, "invalid-output-artifact", "PDF header is missing.", path)


def _check_zip(path: Path, failures: list[dict[str, Any]]) -> None:
    try:
        header = path.read_bytes()[:2]
    except OSError as error:
        _append_issue(failures, "unreadable-artifact", str(error), path)
        return
    if header != b"PK":
        _append_issue(failures, "invalid-output-artifact", "ZIP archive header is missing.", path)


def _sequence_missing(actual: list[str], expected: Iterable[str]) -> list[str]:
    position = 0
    missing: list[str] = []
    for name in expected:
        try:
            position = actual.index(name, position) + 1
        except ValueError:
            missing.append(name)
    return missing


def _collect_metric_reports(route_root: Path) -> list[tuple[Path, dict[str, Any]]]:
    reports: list[tuple[Path, dict[str, Any]]] = []
    for path in sorted(route_root.rglob("*.json")):
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError):
            continue
        if isinstance(value, dict) and isinstance(value.get("checkpoints"), list):
            reports.append((path, value))
    return reports


def _checkpoint_names(
    report: dict[str, Any],
    failures: list[dict[str, Any]],
    path: Path,
) -> tuple[list[str], list[str], list[dict[str, Any]]]:
    checkpoints = report.get("checkpoints")
    if not isinstance(checkpoints, list) or not checkpoints:
        _append_issue(failures, "invalid-metrics", "Metrics report has no checkpoints array.", path)
        return [], [], []
    names: list[str] = []
    elapsed: list[float] = []
    normalized: list[dict[str, Any]] = []
    for index, checkpoint in enumerate(checkpoints):
        if not isinstance(checkpoint, dict):
            _append_issue(failures, "invalid-metrics", f"Checkpoint {index} is not an object.", path)
            continue
        name = checkpoint.get("name")
        if not isinstance(name, str) or not name:
            _append_issue(failures, "invalid-metrics", f"Checkpoint {index} has no name.", path)
        else:
            names.append(name)
        value = checkpoint.get("elapsedMs")
        if not _is_number(value) or value < 0:
            _append_issue(failures, "invalid-metrics", f"Checkpoint {index} has invalid elapsedMs.", path)
        else:
            elapsed.append(float(value))
        if isinstance(checkpoint, dict):
            normalized.append(checkpoint)
    if any(later < earlier for earlier, later in zip(elapsed, elapsed[1:])):
        _append_issue(failures, "invalid-metrics", "Checkpoint elapsedMs values are not ordered.", path)
    return names, [str(value.get("name")) for value in normalized if isinstance(value.get("name"), str)], normalized


def _find_terminal_metrics(
    report: dict[str, Any],
    route_manifest: dict[str, Any] | None,
) -> list[dict[str, Any]]:
    candidates: list[dict[str, Any]] = []
    checkpoints = report.get("checkpoints", [])
    if isinstance(checkpoints, list):
        for checkpoint in checkpoints:
            if not isinstance(checkpoint, dict):
                continue
            name = checkpoint.get("name", "")
            if isinstance(name, str) and (
                "release" in name or "complete" in name or name in {"settled-1s", "settled-5s"}
            ):
                metrics = checkpoint.get("metrics")
                if isinstance(metrics, dict):
                    candidates.append(metrics)
    if isinstance(route_manifest, dict):
        for key, value in route_manifest.items():
            if isinstance(value, dict) and (
                "release" in key.lower() or "lastcheckpointmetrics" in key.lower()
            ):
                candidates.append(value)
    return candidates


def _check_release_flags(
    spec: RouteSpec,
    report: dict[str, Any],
    route_manifest: dict[str, Any] | None,
    failures: list[dict[str, Any]],
    lifecycle_summary: dict[str, Any],
) -> None:
    if not spec.release_prefixes:
        return
    terminal = _find_terminal_metrics(report, route_manifest)
    retained_true: list[str] = []

    def visit(value: Any, key_path: str = "") -> None:
        if isinstance(value, dict):
            for key, child in value.items():
                child_path = f"{key_path}.{key}" if key_path else key
                if (
                    isinstance(child, bool)
                    and child
                    and key.lower().endswith(("retained", "owned"))
                    and any(key.lower().startswith(prefix.lower()) for prefix in spec.release_prefixes)
                    and "tableretained" not in key.lower()
                ):
                    retained_true.append(child_path)
                visit(child, child_path)
        elif isinstance(value, list):
            for index, child in enumerate(value):
                visit(child, f"{key_path}[{index}]")

    for candidate in terminal:
        visit(candidate)

    lifecycle_summary["terminalReleaseFlagsChecked"] = bool(terminal)
    lifecycle_summary["retainedFlagsTrue"] = sorted(set(retained_true))
    if retained_true:
        _append_issue(
            failures,
            "lifecycle-release-flag",
            "Terminal lifecycle metrics retain released operation state: "
            + ", ".join(sorted(set(retained_true))),
        )


def _validate_process_record(
    record: dict[str, Any],
    failures: list[dict[str, Any]],
    label: str,
    path: Path,
) -> bool:
    process_finished = record.get("processFinished")
    exit_status = record.get("exitStatus")
    exit_code = record.get("exitCode")
    timed_out = record.get("timedOut")
    valid = (
        process_finished is True
        and exit_status == "normal"
        and exit_code == 0
        and timed_out is False
    )
    if not valid:
        _append_issue(
            failures,
            "abnormal-process",
            f"{label} did not complete normally (processFinished={process_finished!r}, "
            f"exitStatus={exit_status!r}, exitCode={exit_code!r}, timedOut={timed_out!r}).",
            path,
        )
    return valid


def _validate_route(
    root: Path,
    record: dict[str, Any],
    spec: RouteSpec,
    failures: list[dict[str, Any]],
    warnings: list[dict[str, Any]],
    counts: dict[str, int],
    memory_samples: list[dict[str, Any]],
) -> dict[str, Any]:
    route_summary: dict[str, Any] = {
        "routeId": spec.route_id,
        "category": spec.category,
        "scenario": record.get("scenario"),
        "fixture": record.get("fixture"),
        "artifactPath": record.get("artifactPath", spec.artifact_path),
        "status": record.get("status"),
        "lifecycle": {},
    }
    artifact_value = record.get("artifactPath", spec.artifact_path)
    if not isinstance(artifact_value, str):
        _append_issue(failures, "invalid-route-record", "Route artifactPath must be a string.")
        return route_summary
    if artifact_value.replace("\\", "/").strip("/") != spec.artifact_path:
        _append_issue(
            failures,
            "route-path-mismatch",
            f"Route {spec.route_id} points at {artifact_value!r}; expected {spec.artifact_path!r}.",
        )
    route_root = _resolve_inside(root, artifact_value)
    if route_root is None:
        _append_issue(failures, "unsafe-artifact-path", f"Route path escapes evidence root: {artifact_value!r}")
        return route_summary
    if not route_root.is_dir():
        _append_issue(failures, "missing-artifact", f"Route evidence directory is missing: {artifact_value}", route_root)
        return route_summary

    required = _required_files(route_root, spec, failures)
    counts["routesChecked"] += 1
    counts["requiredFilesChecked"] += len(spec.required_files) + len(spec.required_globs)
    counts["requiredFilesPresent"] += len(required)

    route_manifest_path = route_root / "route-manifest.json"
    route_manifest: dict[str, Any] | None = None
    if route_manifest_path.is_file():
        value = _load_json(route_manifest_path, failures, counts.setdefault("jsonErrors", []))
        if isinstance(value, dict):
            route_manifest = value
            if value.get("routeId") != spec.route_id:
                _append_issue(
                    failures,
                    "invalid-route-record",
                    f"route-manifest.json routeId is {value.get('routeId')!r}, expected {spec.route_id!r}.",
                    route_manifest_path,
                )
            invocation = value.get("invocation")
            if not isinstance(invocation, dict):
                _append_issue(failures, "invalid-route-record", "route-manifest.json has no invocation object.", route_manifest_path)
            else:
                if _validate_process_record(invocation, failures, spec.route_id, route_manifest_path):
                    counts["normalProcesses"] += 1
                command = invocation.get("command")
                environment = invocation.get("environment")
                if not isinstance(command, list) or not command or not all(isinstance(item, str) for item in command):
                    _append_issue(failures, "invalid-route-record", "Invocation command is missing or not a string array.", route_manifest_path)
                if not isinstance(environment, dict):
                    _append_issue(failures, "invalid-route-record", "Invocation environment is missing.", route_manifest_path)
                elif environment.get("QT_QPA_PLATFORM") != "offscreen":
                    _append_issue(failures, "invalid-route-record", "Invocation must record QT_QPA_PLATFORM=offscreen.", route_manifest_path)
    else:
        _append_issue(failures, "missing-artifact", "Runner route-manifest.json is missing.", route_manifest_path)

    for path in sorted(route_root.rglob("*.json")):
        counts["jsonFiles"] += 1
        _load_json(path, failures, counts.setdefault("jsonErrors", []))

    if spec.manifest_required:
        manifest_path = route_root / "manifest.json"
        if manifest_path.is_file():
            value = _load_json(manifest_path, failures, counts.setdefault("jsonErrors", []))
            if isinstance(value, dict):
                if "processFinished" in value:
                    if _validate_process_record(value, failures, f"{spec.route_id} manifest", manifest_path):
                        counts["normalProcesses"] += 1
                elif spec.route_id not in {"visual-classes", "visual-sub-prep"}:
                    _append_issue(failures, "invalid-route-manifest", "Route manifest has no process completion fields.", manifest_path)
                route_summary["manifest"] = {
                    "processFinished": value.get("processFinished"),
                    "exitStatus": value.get("exitStatus"),
                    "exitCode": value.get("exitCode"),
                    "timedOut": value.get("timedOut"),
                }
                for lifecycle_flag in ("workflowCompleted", "lifecycleCompleted"):
                    if lifecycle_flag in value and value[lifecycle_flag] is not True:
                        _append_issue(
                            failures,
                            "lifecycle-incomplete",
                            f"Route manifest records {lifecycle_flag}=false.",
                            manifest_path,
                        )
                references = (
                    "metricsPath",
                    "tracePath",
                    "workflowTracePath",
                    "resourceTracePath",
                    "stdoutPath",
                    "stderrPath",
                    "fixturePath",
                    "outputDirectory",
                )
                for key in references:
                    reference = value.get(key)
                    if not isinstance(reference, str) or not reference.strip():
                        continue
                    target = _resolve_inside(route_root, reference)
                    if target is None:
                        _append_issue(failures, "unsafe-artifact-path", f"Manifest reference {key} escapes route root: {reference!r}", manifest_path)
                    elif key != "outputDirectory" and not target.exists():
                        _append_issue(failures, "missing-artifact", f"Manifest reference {key} is missing: {reference}", target)
                    elif key == "outputDirectory" and not target.is_dir():
                        _append_issue(failures, "missing-artifact", f"Manifest outputDirectory is missing: {reference}", target)

    metric_reports = _collect_metric_reports(route_root)
    report: dict[str, Any] | None = None
    report_path: Path | None = None
    if spec.metrics_file:
        metric_path = _resolve_inside(route_root, spec.metrics_file)
        if metric_path is None or not metric_path.is_file():
            _append_issue(failures, "missing-artifact", f"Metrics report is missing: {spec.metrics_file}", metric_path or route_root)
        else:
            value = _load_json(metric_path, failures, counts.setdefault("jsonErrors", []))
            if isinstance(value, dict):
                report = value
                report_path = metric_path
                counts["metricsReports"] += 1
    elif metric_reports:
        report_path, report = metric_reports[0]
        counts["metricsReports"] += 1

    if report is not None and report_path is not None:
        if report.get("format") not in {None, "classmngr-startup-profile-v2"}:
            _append_issue(failures, "invalid-metrics", f"Unexpected metrics format: {report.get('format')!r}.", report_path)
        names, _, checkpoints = _checkpoint_names(report, failures, report_path)
        lifecycle = route_summary["lifecycle"]
        lifecycle["checkpointCount"] = len(names)
        lifecycle["checkpoints"] = names
        missing = _sequence_missing(names, spec.checkpoint_sequence)
        lifecycle["missingCheckpoints"] = missing
        if missing:
            _append_issue(failures, "lifecycle-order", f"Missing or out-of-order lifecycle checkpoints: {', '.join(missing)}.", report_path)
        if spec.category in {"memory", "output"}:
            terminal_missing = _sequence_missing(names, ("workflow-complete", "settled-1s"))
            lifecycle["missingTerminalCheckpoints"] = terminal_missing
            if terminal_missing:
                _append_issue(
                    failures,
                    "lifecycle-order",
                    f"Missing or out-of-order route completion checkpoints: {', '.join(terminal_missing)}.",
                    report_path,
                )
        event_names: list[str] = []
        events = report.get("events", [])
        if isinstance(events, list):
            for event in events:
                if isinstance(event, dict) and isinstance(event.get("name"), str):
                    event_names.append(event["name"])
        lifecycle["events"] = event_names
        missing_events = _sequence_missing(event_names, spec.event_sequence)
        lifecycle["missingEvents"] = missing_events
        if missing_events:
            _append_issue(failures, "lifecycle-order", f"Missing or out-of-order lifecycle events: {', '.join(missing_events)}.", report_path)
        if spec.expected_failure:
            forbidden = {
                "calendar-import-workbook-parsed",
                "calendar-import-events-prepared",
                "calendar-import-operation-applied",
            }
            observed = sorted(forbidden.intersection(names))
            lifecycle["forbiddenSuccessCheckpoints"] = observed
            if observed:
                _append_issue(failures, "unexpected-success", f"Expected parser failure reached success checkpoints: {', '.join(observed)}.", report_path)
        _check_release_flags(spec, report, route_manifest, failures, lifecycle)
        for checkpoint in checkpoints:
            memory = checkpoint.get("memory")
            if isinstance(memory, dict):
                sample = {
                    "routeId": spec.route_id,
                    "checkpoint": checkpoint.get("name"),
                    "workingSetBytes": memory.get("workingSetBytes"),
                    "peakWorkingSetBytes": memory.get("peakWorkingSetBytes"),
                    "privateUsageBytes": memory.get("privateUsageBytes"),
                }
                if _is_number(sample["workingSetBytes"]) or _is_number(sample["peakWorkingSetBytes"]):
                    memory_samples.append(sample)
        peak = report.get("peakMemory")
        if isinstance(peak, dict):
            memory_samples.append(
                {
                    "routeId": spec.route_id,
                    "checkpoint": "peakMemory",
                    "workingSetBytes": peak.get("workingSetBytes"),
                    "peakWorkingSetBytes": peak.get("peakWorkingSetBytes"),
                    "privateUsageBytes": peak.get("privateUsageBytes"),
                }
            )
    elif spec.metrics_file:
        route_summary["lifecycle"]["missingMetrics"] = True

    if spec.trace_file:
        trace_path = _resolve_inside(route_root, spec.trace_file)
        if trace_path is not None and trace_path.is_file():
            trace = trace_path.read_text(encoding="utf-8", errors="replace")
            if not trace.strip():
                _append_issue(failures, "invalid-trace", "Workflow trace is empty.", trace_path)
            route_summary["lifecycle"]["traceLineCount"] = len([line for line in trace.splitlines() if line.strip()])
        else:
            _append_issue(failures, "missing-artifact", f"Workflow trace is missing: {spec.trace_file}", trace_path or route_root)

    for relative in spec.png_files:
        path = _resolve_inside(route_root, relative)
        if path is not None and path.is_file():
            _check_png(path, failures)
            counts["visualArtifacts"] += 1
        else:
            _append_issue(failures, "missing-artifact", f"Visual artifact is missing: {relative}", path or route_root)
    for pattern in ("**/*.png", "**/*.jpg", "**/*.jpeg"):
        counts["visualFilesDiscovered"] += len(list(route_root.glob(pattern)))

    pdfs = [path for path in route_root.rglob("*.pdf") if path.is_file()]
    zips = [path for path in route_root.rglob("*.zip") if path.is_file()]
    if len(pdfs) < spec.minimum_pdf_files:
        _append_issue(failures, "missing-output-artifact", f"Expected at least {spec.minimum_pdf_files} PDF output(s), found {len(pdfs)}.", route_root)
    if len(zips) < spec.minimum_zip_files:
        _append_issue(failures, "missing-output-artifact", f"Expected at least {spec.minimum_zip_files} ZIP archive(s), found {len(zips)}.", route_root)
    png_files = [path for path in route_root.rglob("*.png") if path.is_file()]
    if len(png_files) < spec.minimum_png_files:
        _append_issue(failures, "missing-output-artifact", f"Expected at least {spec.minimum_png_files} PNG output/capture(s), found {len(png_files)}.", route_root)
    for path in pdfs:
        _check_pdf(path, failures)
    for path in zips:
        _check_zip(path, failures)
    counts["pdfFiles"] += len(pdfs)
    counts["zipFiles"] += len(zips)
    counts["outputFiles"] += len(pdfs) + len(zips)

    if spec.resource_trace_file:
        path = _resolve_inside(route_root, spec.resource_trace_file)
        if path is None or not path.is_file():
            _append_issue(failures, "missing-artifact", f"Resource trace is missing: {spec.resource_trace_file}", path or route_root)
        else:
            value = _load_json(path, failures, counts.setdefault("jsonErrors", []))
            if not isinstance(value, (dict, list)):
                _append_issue(failures, "invalid-resource-trace", "Resource trace must be a JSON object or array.", path)

    return route_summary


def _memory_summary(memory_samples: list[dict[str, Any]], warnings: list[dict[str, Any]]) -> dict[str, Any]:
    evaluated: list[dict[str, Any]] = []
    above_legacy = 0
    above_diagnostic = 0
    maximum: int | None = None
    for sample in memory_samples:
        value = sample.get("peakWorkingSetBytes")
        if not _is_number(value):
            value = sample.get("workingSetBytes")
        if not _is_number(value):
            continue
        integer = int(value)
        maximum = integer if maximum is None else max(maximum, integer)
        item = dict(sample)
        item["comparedWorkingSetBytes"] = integer
        item["aboveLegacy250MiB"] = integer >= LEGACY_RESIDENT_TARGET_BYTES
        item["aboveTemporary512MiB"] = integer >= TEMPORARY_DIAGNOSTIC_CEILING_BYTES
        evaluated.append(item)
        if item["aboveLegacy250MiB"]:
            above_legacy += 1
        if item["aboveTemporary512MiB"]:
            above_diagnostic += 1
    if above_legacy:
        warnings.append(
            {
                "code": "legacy-memory-trend",
                "message": f"{above_legacy} memory sample(s) are at or above the legacy 250 MiB comparison; this is trend data, not a Phase 0 failure.",
            }
        )
    if above_diagnostic:
        warnings.append(
            {
                "code": "temporary-memory-diagnostic",
                "message": f"{above_diagnostic} memory sample(s) are at or above the temporary 512 MiB diagnostic ceiling; this is visible diagnostic context, not a Phase 0 failure.",
            }
        )
    return {
        "legacyResidentTargetBytes": LEGACY_RESIDENT_TARGET_BYTES,
        "legacyResidentTargetMiB": 250,
        "temporaryDiagnosticCeilingBytes": TEMPORARY_DIAGNOSTIC_CEILING_BYTES,
        "temporaryDiagnosticCeilingMiB": 512,
        "maximumComparedWorkingSetBytes": maximum,
        "sampleCount": len(evaluated),
        "aboveLegacy250MiBCount": above_legacy,
        "aboveTemporary512MiBCount": above_diagnostic,
        "isPhase0Failure": False,
        "samples": evaluated,
    }


def _manifest_references(value: Any) -> Iterable[tuple[str, str, bool]]:
    """Yield file/directory references found in retained manifests."""
    if isinstance(value, dict):
        for key, child in value.items():
            lower = key.lower()
            if lower == "capturenames" and isinstance(child, list):
                for reference in child:
                    if isinstance(reference, str):
                        yield key, reference, False
                continue
            if isinstance(child, str) and (
                lower.endswith("path")
                or lower.endswith("capture")
                or lower in {"outputdirectory", "outputtarget"}
            ):
                # A bare `path` inside resource-trace payloads names a packaged
                # Qt resource, not a retained evidence file.
                if lower != "path":
                    yield key, child, lower in {"outputdirectory", "outputtarget"}
            else:
                yield from _manifest_references(child)
    elif isinstance(value, list):
        for child in value:
            yield from _manifest_references(child)


def _terminal_release_flags(report: dict[str, Any]) -> list[str]:
    """Find operation-owned state still true at a release/end checkpoint."""
    bad: list[str] = []
    ownership_words = (
        "operationretained", "dialogretained", "workbookretained",
        "rawbytesretained", "responseretained", "eventsretained",
        "documentretained", "documentsretained", "listretained",
    )
    checkpoints = report.get("checkpoints")
    if not isinstance(checkpoints, list):
        return bad
    terminal = [
        checkpoint for checkpoint in checkpoints
        if isinstance(checkpoint, dict)
        and isinstance(checkpoint.get("name"), str)
        and any(token in checkpoint["name"] for token in (
            "operation-released", "post-release", "operation-end",
            "page-refreshed", "lifecycle-complete", "output-release",
        ))
    ]
    if terminal:
        terminal = terminal[-1:]
    for checkpoint in terminal:
        if not isinstance(checkpoint, dict):
            continue
        name = checkpoint.get("name", "")
        metrics = checkpoint.get("metrics")
        if not isinstance(metrics, dict):
            continue
        for key, child in metrics.items():
            normalized = key.lower()
            if child is True and any(word in normalized for word in ownership_words):
                bad.append(f"{name}:{key}")
    return sorted(set(bad))


def _attach_exit_gate(
    summary: dict[str, Any],
    additional: dict[str, dict[str, Any]] | None = None,
) -> dict[str, Any]:
    """Separate per-run validity from complete multi-platform route coverage."""
    required_routes = list(ALL_ROUTE_IDS)

    def platform_record(run: dict[str, Any]) -> dict[str, Any]:
        run_status = run.get("status")
        observed = {
            item.get("routeId")
            for item in run.get("routeSummary", [])
            if isinstance(item, dict) and item.get("status") == "completed"
        }
        present = [route_id for route_id in required_routes if route_id in observed]
        missing = [route_id for route_id in required_routes if route_id not in observed]
        coverage_complete = not missing
        status = (
            "failed" if run_status == "fail"
            else "passed" if run_status == "pass" and coverage_complete
            else "incomplete"
        )
        return {
            "status": status,
            "artifactPath": run.get("artifactPath"),
            "runStatus": run_status,
            "routeCoverageComplete": coverage_complete,
            "presentRouteIds": present,
            "missingRouteIds": missing,
        }

    evidence: dict[str, dict[str, Any]] = {
        platform: {
            "status": "missing", "artifactPath": None, "runStatus": None,
            "routeCoverageComplete": False, "presentRouteIds": [],
            "missingRouteIds": list(required_routes),
        }
        for platform in SUPPORTED_PLATFORMS
    }
    validated = summary.get("platformStatus", {}).get("validated")
    if validated in evidence:
        evidence[validated] = platform_record(summary)
    for platform, other in (additional or {}).items():
        if platform not in evidence:
            continue
        evidence[platform] = platform_record(other)
        evidence[platform]["failures"] = other.get("failures", [])
        evidence[platform]["warnings"] = other.get("warnings", [])
    statuses = {item["status"] for item in evidence.values()}
    gate_status = "failed" if "failed" in statuses else "passed" if statuses == {"passed"} else "incomplete"
    pending = [platform for platform, item in evidence.items() if item["status"] != "passed"]
    summary["supportedPlatforms"] = list(SUPPORTED_PLATFORMS)
    summary["deferredPlatforms"] = list(DEFERRED_PLATFORMS)
    summary["platformEvidence"] = evidence
    summary["exitGate"] = {
        "status": gate_status,
        "pass": gate_status == "passed",
        "pendingPlatforms": pending,
        "requiredRouteIds": required_routes,
        "requiredRouteCount": len(required_routes),
        "routeCoverage": {
            platform: {
                "complete": item["routeCoverageComplete"],
                "presentRouteIds": item["presentRouteIds"],
                "missingRouteIds": item["missingRouteIds"],
            }
            for platform, item in evidence.items()
        },
        "message": (
            "All supported Phase 0 platforms passed with complete route coverage."
            if gate_status == "passed"
            else "Phase 0 exit gate is incomplete pending supported-platform evidence or complete route coverage."
            if gate_status == "incomplete"
            else "Phase 0 exit gate failed because supplied supported-platform evidence failed validation."
        ),
    }
    return summary


def validate_retained_evidence(evidence_root: Path) -> dict[str, Any]:
    """Validate the historical retained-tree layout, which has no run manifest."""
    root = evidence_root.resolve()
    failures: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []
    memory_samples: list[dict[str, Any]] = []
    inventory: dict[str, Any] = {
        "files": 0, "bytes": 0, "json": 0, "png": 0, "pdf": 0, "zip": 0,
        "manifests": 0, "metricReports": 0, "workflowTraces": 0,
    }
    lifecycle_findings: list[dict[str, Any]] = []
    manifest_findings: list[dict[str, Any]] = []

    if not root.is_dir():
        _append_issue(failures, "missing-evidence-root", f"Evidence root does not exist: {root}", root)
        files: list[Path] = []
    else:
        files = sorted(path for path in root.rglob("*") if path.is_file())
    inventory["files"] = len(files)
    inventory["bytes"] = sum(path.stat().st_size for path in files)

    parsed: dict[Path, Any] = {}
    for path in files:
        suffix = path.suffix.lower()
        if suffix == ".json":
            inventory["json"] += 1
            value = _load_json(path, failures, [])
            if value is not None:
                parsed[path] = value
        elif suffix == ".png":
            inventory["png"] += 1
            _check_png(path, failures)
        elif suffix == ".pdf":
            inventory["pdf"] += 1
            _check_pdf(path, failures)
        elif suffix == ".zip":
            inventory["zip"] += 1
            _check_zip(path, failures)
        if path.name.lower() == "workflow-trace.txt":
            inventory["workflowTraces"] += 1
            if not path.read_text(encoding="utf-8", errors="replace").strip():
                _append_issue(failures, "invalid-trace", "Workflow trace is empty.", path)

    manifests = [(path, value) for path, value in parsed.items() if path.name == "manifest.json"]
    reports = [
        (path, value) for path, value in parsed.items()
        if isinstance(value, dict) and isinstance(value.get("checkpoints"), list)
    ]
    inventory["manifests"] = len(manifests)
    inventory["metricReports"] = len(reports)

    for path, manifest in manifests:
        if not isinstance(manifest, dict):
            _append_issue(failures, "invalid-manifest", "Manifest root must be an object.", path)
            continue
        finding = {
            "path": str(path.relative_to(root)), "scenario": manifest.get("scenario"),
            "fixture": manifest.get("fixture"),
        }
        has_process = any(key in manifest for key in ("processFinished", "exitStatus", "exitCode", "timedOut"))
        if has_process:
            valid = (
                manifest.get("processFinished") is True
                and manifest.get("exitStatus") == "normal"
                and manifest.get("exitCode") == 0
                and manifest.get("timedOut", False) is False
            )
            finding["normalCompletion"] = valid
            if not valid:
                _append_issue(failures, "abnormal-process", "Manifest does not record normal process completion.", path)
            if not isinstance(manifest.get("scenario"), str) or not manifest.get("scenario"):
                _append_issue(failures, "missing-route-identity", "Process manifest has no scenario identity.", path)
            if not isinstance(manifest.get("fixture"), str) or not manifest.get("fixture"):
                _append_issue(failures, "missing-fixture-identity", "Process manifest has no fixture identity.", path)
        for field, reference, is_directory in _manifest_references(manifest):
            target = _resolve_inside(path.parent, reference)
            if target is None:
                _append_issue(failures, "unsafe-artifact-path", f"Manifest reference {field} escapes its directory: {reference!r}.", path)
            elif (is_directory and not target.is_dir()) or (not is_directory and not target.is_file()):
                _append_issue(failures, "missing-manifest-reference", f"Manifest reference {field} is missing: {reference}.", target)
        manifest_findings.append(finding)

    for path, report in reports:
        names, _, checkpoints = _checkpoint_names(report, failures, path)
        events = report.get("events")
        if not isinstance(events, list):
            _append_issue(failures, "invalid-metrics", "Metrics report has no events array.", path)
        if report.get("finalProgress") not in (None, 100):
            _append_issue(failures, "incomplete-lifecycle", f"Final progress is {report.get('finalProgress')!r}, expected 100.", path)
        scenario = report.get("scenario")
        if not isinstance(scenario, dict) or not isinstance(scenario.get("name"), str) or not scenario.get("name"):
            _append_issue(failures, "missing-route-identity", "Metrics report has no scenario name.", path)
        completion = any(name in names for name in ("workflow-complete", "startup-complete"))
        if not completion:
            _append_issue(failures, "incomplete-lifecycle", "No startup-complete or workflow-complete checkpoint was recorded.", path)
        bad_flags = _terminal_release_flags(report)
        if bad_flags:
            _append_issue(failures, "lifecycle-release-flag", "Released operation state remains owned: " + ", ".join(bad_flags), path)
        lifecycle_findings.append({
            "path": str(path.relative_to(root)), "checkpointCount": len(names),
            "eventCount": len(events) if isinstance(events, list) else 0,
            "normalCompletion": completion, "retainedReleaseFlags": bad_flags,
        })
        for checkpoint in checkpoints:
            memory = checkpoint.get("memory")
            if isinstance(memory, dict):
                memory_samples.append({
                    "routeId": str(path.parent.relative_to(root)), "checkpoint": checkpoint.get("name"),
                    "workingSetBytes": memory.get("workingSetBytes"),
                    "peakWorkingSetBytes": memory.get("peakWorkingSetBytes"),
                    "privateUsageBytes": memory.get("privateUsageBytes"),
                })
        peak = report.get("peakMemory")
        if isinstance(peak, dict):
            memory_samples.append({
                "routeId": str(path.parent.relative_to(root)), "checkpoint": "peakMemory",
                "workingSetBytes": peak.get("workingSetBytes"),
                "peakWorkingSetBytes": peak.get("peakWorkingSetBytes"),
                "privateUsageBytes": peak.get("privateUsageBytes"),
            })

    categories = {
        "visualStates": inventory["png"] > 0,
        "generatedOutputs": inventory["pdf"] > 0 or inventory["zip"] > 0,
        "lifecycle": inventory["metricReports"] > 0,
        "workflowTraces": inventory["workflowTraces"] > 0,
        "memory": bool(memory_samples), "manifests": inventory["manifests"] > 0,
    }
    one_run = (root / "manifest.json").is_file() or any(path.parent == root for path, _ in reports)
    if one_run:
        process_manifest = any(
            isinstance(value, dict) and "processFinished" in value
            for _, value in manifests
        )
        output_manifest = any(
            isinstance(value, dict) and any(key in value for key in ("pdfCount", "documentCount", "outputDirectory"))
            for _, value in manifests
        )
        required_categories = {
            "manifests": (root / "manifest.json").is_file(),
            "lifecycle": True, "memory": True,
            "workflowTraces": process_manifest,
            "generatedOutputs": output_manifest,
            "visualStates": "visual" in root.name.lower() or bool(inventory["png"]),
        }
    else:
        required_categories = {category: True for category in categories}
    for category in required_categories:
        if required_categories[category] and not categories[category]:
            _append_issue(failures, "missing-evidence-category", f"Required Windows x64 evidence category is missing: {category}.", root)
    if required_categories.get("manifests") and not manifests:
        _append_issue(failures, "missing-manifest", "No manifest.json files were found.", root)

    memory = _memory_summary(memory_samples, warnings)
    invalid_json = sum(1 for item in failures if item["code"] == "invalid-json")
    summary = {
        "schema": SUMMARY_SCHEMA, "schemaVersion": 1, "validatorVersion": VALIDATOR_VERSION,
        "taskId": TASK_ID, "recordedAtUtc": _utc_now(), "status": "fail" if failures else "pass",
        "pass": not failures, "layout": "retained-tree", "artifactPath": str(root),
        "platformStatus": {
            "required": list(SUPPORTED_PLATFORMS), "validated": SUPPORTED_PLATFORM,
            "supported": True, "deferred": list(DEFERRED_PLATFORMS),
        },
        "evidenceCategories": categories, "requiredEvidenceCategories": required_categories,
        "artifactInventory": inventory,
        "jsonFindings": {"valid": inventory["json"] - invalid_json, "invalid": invalid_json},
        "manifestFindings": manifest_findings, "lifecycleFindings": lifecycle_findings,
        "memoryTrend": memory, "failures": failures, "warnings": warnings,
    }
    return _attach_exit_gate(summary)


def validate_evidence(
    evidence_root: Path,
    expected_routes: Iterable[str] | None = None,
    expected_platform: str = SUPPORTED_PLATFORM,
    run_manifest_path: Path | None = None,
) -> dict[str, Any]:
    root = evidence_root.resolve()
    failures: list[dict[str, Any]] = []
    warnings: list[dict[str, Any]] = []
    json_errors: list[str] = []
    counts: dict[str, Any] = {
        "routesChecked": 0,
        "requiredFilesChecked": 0,
        "requiredFilesPresent": 0,
        "normalProcesses": 0,
        "jsonFiles": 0,
        "metricsReports": 0,
        "visualArtifacts": 0,
        "visualFilesDiscovered": 0,
        "pdfFiles": 0,
        "zipFiles": 0,
        "outputFiles": 0,
        "jsonErrors": json_errors,
    }
    memory_samples: list[dict[str, Any]] = []
    manifest_path = run_manifest_path.resolve() if run_manifest_path is not None else root / "run-manifest.json"
    run_manifest: dict[str, Any] | None = None
    if not root.is_dir():
        _append_issue(failures, "missing-evidence-root", f"Evidence root does not exist: {root}", root)
    elif not manifest_path.is_file():
        _append_issue(
            failures,
            "missing-run-manifest",
            "run-manifest.json is required to establish supported/deferred platform scope and requested routes.",
            manifest_path,
        )
    else:
        value = _load_json(manifest_path, failures, json_errors)
        if isinstance(value, dict):
            run_manifest = value

    deferred_platforms: list[str] = []
    top_status = "unknown"
    requested: list[str] = list(expected_routes or ())
    if run_manifest is not None:
        top_status = str(run_manifest.get("status", "unknown"))
        if run_manifest.get("schema") != SCHEMA:
            _append_issue(failures, "invalid-run-manifest", f"Unexpected run manifest schema: {run_manifest.get('schema')!r}.", manifest_path)
        manifest_task_id = run_manifest.get("taskId")
        if manifest_task_id not in (TASK_ID, *LEGACY_TASK_IDS):
            _append_issue(
                failures, "invalid-run-manifest",
                f"Unexpected taskId: {manifest_task_id!r}; expected {TASK_ID!r}.", manifest_path,
            )
        elif manifest_task_id in LEGACY_TASK_IDS:
            warnings.append(
                {
                    "code": "legacy-task-id",
                    "message": f"Run manifest uses legacy taskId {manifest_task_id!r}; accepted for compatibility. New evidence should use {TASK_ID!r}.",
                    "path": str(manifest_path),
                }
            )
        supported = run_manifest.get("supportedPlatform")
        if supported != expected_platform:
            _append_issue(failures, "unsupported-platform", f"Run manifest supportedPlatform is {supported!r}; expected {expected_platform!r}.", manifest_path)
        raw_deferred = run_manifest.get("deferredPlatforms")
        if not isinstance(raw_deferred, list) or not all(isinstance(item, str) for item in raw_deferred):
            _append_issue(failures, "invalid-platform-metadata", "deferredPlatforms must be a string array.", manifest_path)
        else:
            deferred_platforms = list(raw_deferred)
            if set(deferred_platforms) != set(DEFERRED_PLATFORMS) or len(deferred_platforms) != len(DEFERRED_PLATFORMS):
                _append_issue(
                    failures, "invalid-platform-metadata",
                    f"deferredPlatforms must be exactly {list(DEFERRED_PLATFORMS)!r}; got {deferred_platforms!r}.",
                    manifest_path,
                )
            if expected_platform in deferred_platforms:
                _append_issue(failures, "invalid-platform-metadata", f"Supported platform is incorrectly listed as deferred: {expected_platform}.", manifest_path)
        platform_info = run_manifest.get("platform")
        if not isinstance(platform_info, dict):
            _append_issue(failures, "invalid-platform-metadata", "platform metadata object is missing.", manifest_path)
        else:
            if platform_info.get("name") != expected_platform:
                _append_issue(failures, "invalid-platform-metadata", f"platform.name is {platform_info.get('name')!r}; expected {expected_platform!r}.", manifest_path)
            if top_status == "completed" and platform_info.get("supported") is not True:
                _append_issue(failures, "unsupported-platform", "A completed run must record platform.supported=true.", manifest_path)
        if not requested:
            raw_requested = run_manifest.get("requestedRoutes")
            if isinstance(raw_requested, list) and all(isinstance(item, str) for item in raw_requested):
                requested = list(raw_requested)
        raw_routes = run_manifest.get("routes")
        route_records = raw_routes if isinstance(raw_routes, list) else []
        route_by_id = {
            item.get("routeId"): item
            for item in route_records
            if isinstance(item, dict) and isinstance(item.get("routeId"), str)
        }
    else:
        route_by_id = {}

    if not requested and top_status == "completed":
        _append_issue(failures, "invalid-run-manifest", "A completed run must record requestedRoutes.", manifest_path)
    unknown_routes = [route_id for route_id in requested if route_id not in ROUTE_BY_ID]
    for route_id in unknown_routes:
        _append_issue(failures, "unknown-route", f"No validator contract exists for route {route_id!r}.", manifest_path)
    route_summaries: list[dict[str, Any]] = []
    if top_status == "completed" and not unknown_routes:
        for route_id in requested:
            record = route_by_id.get(route_id)
            if record is None:
                _append_issue(failures, "missing-route", f"Run manifest has no record for requested route {route_id!r}.", manifest_path)
                continue
            spec = ROUTE_BY_ID[route_id]
            route_summaries.append(_validate_route(root, record, spec, failures, warnings, counts, memory_samples))
            if record.get("status") != "completed":
                _append_issue(failures, "route-failed", f"Route {route_id} has status {record.get('status')!r}.", manifest_path)

    if run_manifest is not None:
        raw_commands = run_manifest.get("commands")
        if top_status == "completed" and (not isinstance(raw_commands, list) or not raw_commands):
            _append_issue(failures, "invalid-run-manifest", "A completed run must record exact commands.", manifest_path)

    if top_status in {"planned", "skipped", "deferred"} and not failures:
        warnings.append(
            {
                "code": f"run-{top_status}",
                "message": f"Evidence run is {top_status}; required route evidence was not evaluated as a completed gate.",
            }
        )
    elif top_status not in {"completed", "planned", "skipped", "deferred"}:
        _append_issue(failures, "incomplete-run", f"Unsupported or incomplete run status: {top_status!r}.", manifest_path)

    memory = _memory_summary(memory_samples, warnings)
    status = "fail" if failures else top_status if top_status in {"planned", "skipped", "deferred"} else "pass"
    summary: dict[str, Any] = {
        "schema": SUMMARY_SCHEMA,
        "schemaVersion": 1,
        "validatorVersion": VALIDATOR_VERSION,
        "taskId": TASK_ID,
        "recordedAtUtc": _utc_now(),
        "status": status,
        "pass": status == "pass",
        "layout": "orchestrated-run",
        "scenario": "phase0-packaged-release-evidence",
        "fixture": "packaged-release-windows-x64",
        "artifactPath": str(root),
        "supportedPlatform": expected_platform,
        "deferredPlatforms": deferred_platforms,
        "platformStatus": {
            "required": list(SUPPORTED_PLATFORMS),
            "validated": expected_platform,
            "supported": expected_platform in SUPPORTED_PLATFORMS,
            "deferred": deferred_platforms,
        },
        "routeSummary": route_summaries,
        "metricSummary": {
            key: value
            for key, value in counts.items()
            if key != "jsonErrors"
        },
        "artifactInventory": {
            "json": counts["jsonFiles"], "png": counts["visualFilesDiscovered"],
            "pdf": counts["pdfFiles"], "zip": counts["zipFiles"],
            "requiredChecked": counts["requiredFilesChecked"],
            "requiredPresent": counts["requiredFilesPresent"],
        },
        "jsonFindings": {"invalid": len(json_errors), "errors": json_errors},
        "manifestFindings": [item.get("manifest", {}) for item in route_summaries],
        "lifecycleSummary": {
            "routesWithLifecycleContracts": sum(1 for item in route_summaries if item.get("lifecycle", {}).get("checkpointCount", 0)),
            "routes": {
                item["routeId"]: item.get("lifecycle", {})
                for item in route_summaries
                if "routeId" in item
            },
        },
        "memoryTrend": memory,
        "failures": failures,
        "warnings": warnings,
    }
    return _attach_exit_gate(summary)


def _minimal_png() -> bytes:
    return (
        b"\x89PNG\r\n\x1a\n"
        b"\x00\x00\x00\x0dIHDR"
        b"\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00"
        b"\x1f\x15\xc4\x89"
    )


def _write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def _write_summary_json(path: Path, value: dict[str, Any], force: bool = False) -> None:
    """Write a summary atomically with respect to the no-overwrite contract."""
    path.parent.mkdir(parents=True, exist_ok=True)
    mode = "w" if force else "x"
    with path.open(mode, encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2)
        stream.write("\n")


def _result_exit_code(
    summary: dict[str, Any],
    supplemental_failed: bool = False,
    require_exit_gate: bool = False,
) -> int:
    if summary.get("status") not in {"pass", "planned", "skipped", "deferred"} or supplemental_failed:
        return 1
    if require_exit_gate and summary.get("exitGate", {}).get("status") != "passed":
        return 3
    return 0


def _create_self_test_fixture(root: Path) -> None:
    route = root / "workflow/representative"
    route.mkdir(parents=True, exist_ok=True)
    for relative in (
        "captures/startup-complete.png",
        "captures/settled-final.png",
        "pdf-captures/pdf-opened.png",
        "pdf-captures/pdf-reopened.png",
    ):
        path = route / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(_minimal_png())
    checkpoints = [
        {"name": name, "elapsedMs": index * 10, "memory": {"workingSetBytes": 1000, "peakWorkingSetBytes": 1000}, "metrics": {"livePdfDocumentCount": 0}}
        for index, name in enumerate(WORKFLOW_CHECKPOINTS)
    ]
    _write_json(
        route / "startup-metrics.json",
        {
            "format": "classmngr-startup-profile-v2",
            "scenario": {"name": "representative-startup"},
            "checkpoints": checkpoints,
            "events": [
                {"name": "page-enter"},
                {"name": "page-leave"},
                {"name": "pdf-document-loaded"},
                {"name": "pdf-document-released"},
            ],
            "peakMemory": {"workingSetBytes": 1000, "peakWorkingSetBytes": 1000},
        },
    )
    (route / "workflow-trace.txt").write_text("complete\n", encoding="utf-8")
    _write_json(
        route / "route-manifest.json",
        {
            "schema": "classmngr-phase0-route-v1",
            "routeId": "workflow-representative",
            "scenario": "representative workflow",
            "fixture": "representative-startup.tps",
            "artifactPath": "workflow/representative",
            "status": "completed",
            "invocation": {
                "command": ["ClassMngr.exe", "--startup-performance-workflow"],
                "environment": {"QT_QPA_PLATFORM": "offscreen", "CLASSMNGR_TEST_APP_PATH": "ClassMngr.exe"},
                "processFinished": True,
                "exitStatus": "normal",
                "exitCode": 0,
                "timedOut": False,
            },
        },
    )
    _write_json(
        root / "run-manifest.json",
        {
            "schema": SCHEMA,
            "taskId": TASK_ID,
            "status": "completed",
            "supportedPlatform": SUPPORTED_PLATFORM,
            "deferredPlatforms": list(DEFERRED_PLATFORMS),
            "platform": {"name": SUPPORTED_PLATFORM, "supported": True},
            "requestedRoutes": ["workflow-representative"],
            "routes": [
                {
                    "routeId": "workflow-representative",
                    "category": "workflow",
                    "scenario": "representative workflow",
                    "fixture": "representative-startup.tps",
                    "artifactPath": "workflow/representative",
                    "status": "completed",
                }
            ],
            "commands": [{"id": "workflow-representative", "argv": ["ClassMngr.exe"]}],
        },
    )


def run_self_test() -> int:
    class ValidatorSelfTest(unittest.TestCase):
        def test_valid_fixture_passes(self) -> None:
            with tempfile.TemporaryDirectory(prefix="phase0-validator-") as temporary:
                root = Path(temporary)
                _create_self_test_fixture(root)
                summary = validate_evidence(root)
                self.assertEqual(summary["status"], "pass")
                self.assertFalse(summary["failures"])
                self.assertEqual(summary["exitGate"]["status"], "incomplete")
                self.assertEqual(summary["platformEvidence"]["windows-x64"]["status"], "incomplete")
                self.assertEqual(summary["platformEvidence"]["windows-x64"]["presentRouteIds"], ["workflow-representative"])
                self.assertEqual(len(summary["platformEvidence"]["windows-x64"]["missingRouteIds"]), 23)
                self.assertEqual(summary["platformEvidence"]["macos-universal"]["status"], "missing")

        def test_windows_and_macos_subsets_do_not_complete_exit_gate(self) -> None:
            with tempfile.TemporaryDirectory(prefix="phase0-validator-") as temporary:
                base = Path(temporary)
                windows_root = base / "windows"
                macos_root = base / "macos"
                _create_self_test_fixture(windows_root)
                _create_self_test_fixture(macos_root)
                manifest_path = macos_root / "run-manifest.json"
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                manifest["supportedPlatform"] = "macos-universal"
                manifest["platform"]["name"] = "macos-universal"
                _write_json(manifest_path, manifest)
                windows = validate_evidence(windows_root)
                macos = validate_evidence(macos_root, expected_platform="macos-universal")
                _attach_exit_gate(windows, {"macos-universal": macos})
                self.assertEqual(macos["status"], "pass")
                self.assertEqual(windows["exitGate"]["status"], "incomplete")
                self.assertFalse(windows["platformEvidence"]["windows-x64"]["routeCoverageComplete"])
                self.assertFalse(windows["platformEvidence"]["macos-universal"]["routeCoverageComplete"])

        def test_full_synthetic_route_coverage_completes_exit_gate(self) -> None:
            self.assertEqual(len(ALL_ROUTE_IDS), 24)

            def complete_summary(platform: str) -> dict[str, Any]:
                return {
                    "status": "pass",
                    "artifactPath": f"synthetic/{platform}",
                    "platformStatus": {"validated": platform},
                    "routeSummary": [
                        {"routeId": route_id, "status": "completed"}
                        for route_id in ALL_ROUTE_IDS
                    ],
                    "failures": [],
                    "warnings": [],
                }

            windows = complete_summary("windows-x64")
            macos = complete_summary("macos-universal")
            _attach_exit_gate(windows, {"macos-universal": macos})
            self.assertEqual(windows["exitGate"]["status"], "passed")
            self.assertEqual(windows["exitGate"]["requiredRouteCount"], 24)
            self.assertFalse(windows["exitGate"]["pendingPlatforms"])
            self.assertTrue(all(item["routeCoverageComplete"] for item in windows["platformEvidence"].values()))

        def test_deferred_platforms_are_exact(self) -> None:
            with tempfile.TemporaryDirectory(prefix="phase0-validator-") as temporary:
                root = Path(temporary)
                _create_self_test_fixture(root)
                manifest_path = root / "run-manifest.json"
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                manifest["deferredPlatforms"].append("macos-universal")
                _write_json(manifest_path, manifest)
                summary = validate_evidence(root)
                self.assertEqual(summary["status"], "fail")
                self.assertTrue(any(item["code"] == "invalid-platform-metadata" for item in summary["failures"]))

        def test_legacy_runner_task_id_remains_compatible(self) -> None:
            with tempfile.TemporaryDirectory(prefix="phase0-validator-") as temporary:
                root = Path(temporary)
                _create_self_test_fixture(root)
                manifest_path = root / "run-manifest.json"
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                manifest["taskId"] = LEGACY_TASK_IDS[0]
                _write_json(manifest_path, manifest)
                summary = validate_evidence(root)
                self.assertEqual(summary["status"], "pass")
                self.assertTrue(any(item["code"] == "legacy-task-id" for item in summary["warnings"]))

        def test_json_output_requires_force_to_overwrite(self) -> None:
            with tempfile.TemporaryDirectory(prefix="phase0-validator-") as temporary:
                output = Path(temporary) / "summary.json"
                _write_summary_json(output, {"revision": 1})
                with self.assertRaises(FileExistsError):
                    _write_summary_json(output, {"revision": 2})
                self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["revision"], 1)
                _write_summary_json(output, {"revision": 2}, force=True)
                self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["revision"], 2)

        def test_require_exit_gate_changes_only_exit_semantics(self) -> None:
            with tempfile.TemporaryDirectory(prefix="phase0-validator-") as temporary:
                root = Path(temporary)
                _create_self_test_fixture(root)
                summary = validate_evidence(root)
                self.assertEqual(summary["status"], "pass")
                self.assertEqual(summary["exitGate"]["status"], "incomplete")
                self.assertEqual(_result_exit_code(summary), 0)
                self.assertEqual(_result_exit_code(summary, require_exit_gate=True), 3)

        def test_missing_visual_is_failure_and_memory_is_not_gate(self) -> None:
            with tempfile.TemporaryDirectory(prefix="phase0-validator-") as temporary:
                root = Path(temporary)
                _create_self_test_fixture(root)
                (root / "workflow/representative/captures/settled-final.png").unlink()
                value = json.loads((root / "workflow/representative/startup-metrics.json").read_text(encoding="utf-8"))
                value["peakMemory"]["peakWorkingSetBytes"] = LEGACY_RESIDENT_TARGET_BYTES + 1
                _write_json(root / "workflow/representative/startup-metrics.json", value)
                summary = validate_evidence(root)
                self.assertEqual(summary["status"], "fail")
                self.assertTrue(any(item["code"] == "missing-artifact" for item in summary["failures"]))
                self.assertEqual(summary["memoryTrend"]["aboveLegacy250MiBCount"], 1)
                self.assertFalse(summary["memoryTrend"]["isPhase0Failure"])

        def test_bad_json_abnormal_process_and_failed_lifecycle_fail(self) -> None:
            with tempfile.TemporaryDirectory(prefix="phase0-validator-") as temporary:
                root = Path(temporary)
                _create_self_test_fixture(root)
                route = root / "workflow/representative"
                (route / "bad.json").write_text("{not json", encoding="utf-8")
                route_manifest = json.loads((route / "route-manifest.json").read_text(encoding="utf-8"))
                route_manifest["invocation"]["exitCode"] = 9
                _write_json(route / "route-manifest.json", route_manifest)
                metrics = json.loads((route / "startup-metrics.json").read_text(encoding="utf-8"))
                metrics["checkpoints"] = metrics["checkpoints"][:-2]
                _write_json(route / "startup-metrics.json", metrics)
                summary = validate_evidence(root)
                codes = {item["code"] for item in summary["failures"]}
                self.assertEqual(summary["status"], "fail")
                self.assertEqual(summary["platformEvidence"]["windows-x64"]["status"], "failed")
                self.assertTrue({"invalid-json", "abnormal-process", "lifecycle-order"}.issubset(codes))

    result = unittest.TextTestRunner(verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(ValidatorSelfTest)
    )
    return 0 if result.wasSuccessful() else 1


def _parse_routes(values: list[str] | None) -> list[str] | None:
    if not values:
        return None
    expanded: list[str] = []
    for value in values:
        for token in value.split(","):
            token = token.strip()
            if not token:
                continue
            if token == "all":
                expanded.extend(ALL_ROUTE_IDS)
            elif token in ROUTES_BY_CATEGORY:
                expanded.extend(ROUTES_BY_CATEGORY[token])
            else:
                expanded.append(token)
    return list(dict.fromkeys(expanded))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-root", type=Path, help="completed runner output directory")
    parser.add_argument("--json-out", "--summary-output", dest="json_out", type=Path, help="optional JSON summary output path")
    parser.add_argument("--force-json-out", action="store_true", help="allow --json-out to replace an existing file")
    parser.add_argument(
        "--require-exit-gate", action="store_true",
        help="return exit code 3 unless exitGate.status is passed (default exit status validates supplied runs only)",
    )
    parser.add_argument("--run-manifest", type=Path, help="explicit orchestrated run manifest (defaults to EVIDENCE_ROOT/run-manifest.json)")
    parser.add_argument("--expected-platform", choices=SUPPORTED_PLATFORMS, default=SUPPORTED_PLATFORM)
    parser.add_argument("--macos-evidence-root", type=Path, help="optional macOS universal evidence root to consolidate into the Phase 0 exit gate")
    parser.add_argument("--macos-run-manifest", type=Path, help="explicit manifest for --macos-evidence-root")
    parser.add_argument("--routes", nargs="*", help="route IDs or categories to validate")
    parser.add_argument("--self-test", action="store_true", help="run deterministic validator self-checks")
    args = parser.parse_args(argv)
    if args.self_test:
        return run_self_test()
    if args.force_json_out and args.json_out is None:
        parser.error("--force-json-out requires --json-out")
    if args.json_out is not None:
        requested_output = args.json_out.resolve()
        if requested_output.is_dir():
            parser.error(f"--json-out is a directory: {requested_output}")
        if requested_output.exists() and not args.force_json_out:
            parser.error(f"refusing to overwrite existing --json-out: {requested_output}; use --force-json-out to replace it")
    if args.evidence_root is None:
        parser.error("--evidence-root is required unless --self-test is used")
    expected_routes = _parse_routes(args.routes)
    default_manifest = args.evidence_root / "run-manifest.json"
    if args.run_manifest is not None or default_manifest.is_file():
        summary = validate_evidence(args.evidence_root, expected_routes, args.expected_platform, args.run_manifest)
    else:
        if expected_routes is not None:
            parser.error("--routes requires an orchestrated run manifest")
        if args.expected_platform != SUPPORTED_PLATFORM:
            parser.error("macos-universal evidence requires an orchestrated run manifest")
        summary = validate_retained_evidence(args.evidence_root)
    macos_summary: dict[str, Any] | None = None
    if args.macos_run_manifest is not None and args.macos_evidence_root is None:
        parser.error("--macos-run-manifest requires --macos-evidence-root")
    if args.macos_evidence_root is not None:
        if args.expected_platform == "macos-universal":
            parser.error("--macos-evidence-root is supplemental; the primary --evidence-root must be windows-x64")
        macos_summary = validate_evidence(
            args.macos_evidence_root,
            expected_platform="macos-universal",
            run_manifest_path=args.macos_run_manifest,
        )
        _attach_exit_gate(summary, {"macos-universal": macos_summary})
        summary["supplementalPlatformRuns"] = {"macos-universal": macos_summary}
    metrics = summary.get("metricSummary", summary.get("artifactInventory", {}))
    print(
        f"Phase 0 evidence: {summary['status'].upper()} | "
        f"routes checked={metrics.get('routesChecked', len(summary.get('lifecycleFindings', [])))} | "
        f"visual={metrics.get('visualArtifacts', metrics.get('png', 0))} | "
        f"output PDFs={metrics.get('pdfFiles', metrics.get('pdf', 0))} ZIPs={metrics.get('zipFiles', metrics.get('zip', 0))}"
    )
    deferred = summary.get("deferredPlatforms", summary.get("platformStatus", {}).get("deferred", []))
    print(
        "Deferred unofficial platforms: "
        + (", ".join(deferred) if deferred else "none")
        + " (not Phase 0 failures)"
    )
    gate = summary["exitGate"]
    print(
        f"Phase 0 exit gate: {gate['status'].upper()} | "
        + (f"pending={', '.join(gate['pendingPlatforms'])}" if gate["pendingPlatforms"] else "all supported platforms passed")
    )
    memory = summary["memoryTrend"]
    print(
        "Memory trend: "
        f"max working set={memory['maximumComparedWorkingSetBytes']!r} bytes; "
        f"250 MiB comparisons={memory['aboveLegacy250MiBCount']}; "
        f"512 MiB diagnostic comparisons={memory['aboveTemporary512MiBCount']}"
    )
    for issue in summary["failures"]:
        print(f"FAIL [{issue['code']}] {issue['message']}")
    for warning in summary["warnings"]:
        print(f"WARN [{warning['code']}] {warning['message']}")
    if args.json_out is not None:
        output = args.json_out.resolve()
        try:
            _write_summary_json(output, summary, force=args.force_json_out)
        except FileExistsError:
            print(
                f"ERROR: refusing to overwrite existing --json-out: {output}; "
                "use --force-json-out to replace it",
                file=sys.stderr,
            )
            return 2
        print(f"Machine-readable summary: {output}")
    else:
        print("Machine-readable summary: use --json-out <path> (no files were written).")
    supplied_platform_failed = macos_summary is not None and macos_summary["status"] == "fail"
    return _result_exit_code(summary, supplied_platform_failed, args.require_exit_gate)


if __name__ == "__main__":
    raise SystemExit(main())
