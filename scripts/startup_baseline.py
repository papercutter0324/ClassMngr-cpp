#!/usr/bin/env python3
"""Run and aggregate repeatable ClassMngr startup baseline captures.

This dependency-free script works on Windows, macOS, and Linux. It launches
the application's existing --startup-performance-test mode, creates an
isolated settings profile for each run, validates the resulting report, and
writes raw run artifacts plus aggregate JSON and Markdown summaries.

Typical representative capture:

    python scripts/startup_baseline.py \
        --app-path build/macos-clang-debug/ClassMngr \
        --output-dir artifacts/startup-baseline/macos-run \
        --runs 3

Use --qpa-platform offscreen for a headless or CI capture. Omit it when the
baseline should use the platform's normal Qt platform plugin.
"""

from __future__ import annotations

import argparse
import datetime as datetime_module
import hashlib
import json
import math
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any


PROFILE_FORMAT = "classmngr-startup-profile-v2"
SUMMARY_FORMAT = "classmngr-startup-baseline-v1"
RUN_METADATA_FORMAT = "classmngr-startup-run-v1"

BASE_CHECKPOINTS = (
    "process-start",
    "qapplication-created",
    "preferences-resolved",
    "locale-applied",
    "font-applied",
    "theme-applied",
    "resource-system-initialized",
    "splash-shown",
    "main-window-shell-created",
    "services-created",
    "page-manager-initialized",
    "controllers-connected",
    "database-opened",
    "navigation-data-loaded",
    "startup-page-created",
    "startup-page-loaded",
    "window-shown",
    "startup-complete",
)

SETTLED_CHECKPOINTS = (
    (1000, "settled-1s"),
    (5000, "settled-5s"),
    (30000, "settled-30s"),
)

MEMORY_FIELDS = (
    "workingSetBytes",
    "peakWorkingSetBytes",
    "privateUsageBytes",
    "privateWorkingSetBytes",
    "privateDirtyBytes",
    "pagefileUsageBytes",
    "handleCount",
    "threadCount",
)

APPLICATION_FIELDS = (
    "widgetCount",
    "instantiatedPageCount",
    "registeredPageCount",
    "liveScheduleWidgetCount",
    "scheduleWidgetsCreated",
    "scheduleRenderCount",
    "scheduleTableItemsCreated",
    "scheduleCellWidgetsCreated",
    "scheduleCellWidgetsRemoved",
    "scheduleCellWidgetsQueuedForDeletion",
)


class ValidationError(RuntimeError):
    """Raised when an application startup report is incomplete or invalid."""


def utc_now() -> str:
    return (
        datetime_module.datetime.now(datetime_module.timezone.utc)
        .isoformat(timespec="milliseconds")
        .replace("+00:00", "Z")
    )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def positive_int(value: str) -> int:
    try:
        parsed = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be an integer") from error
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return parsed


def nonnegative_int(value: str) -> int:
    try:
        parsed = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be an integer") from error
    if parsed < 0:
        raise argparse.ArgumentTypeError("must not be negative")
    return parsed


def json_number(value: Any, description: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValidationError(f"{description} is not numeric")
    if not math.isfinite(float(value)):
        raise ValidationError(f"{description} is not finite")
    return float(value)


def required_object(
    parent: dict[str, Any],
    key: str,
    description: str,
) -> dict[str, Any]:
    value = parent.get(key)
    if not isinstance(value, dict):
        raise ValidationError(f"{description}.{key} is missing or not an object")
    return value


def required_string(
    parent: dict[str, Any],
    key: str,
    description: str,
) -> str:
    value = parent.get(key)
    if not isinstance(value, str) or not value:
        raise ValidationError(f"{description}.{key} is missing or empty")
    return value


def checkpoint_map(
    report: dict[str, Any],
) -> tuple[list[dict[str, Any]], dict[str, dict[str, Any]]]:
    raw_checkpoints = report.get("checkpoints")
    if not isinstance(raw_checkpoints, list) or not raw_checkpoints:
        raise ValidationError("report.checkpoints is missing or empty")

    checkpoints: list[dict[str, Any]] = []
    names: dict[str, dict[str, Any]] = {}
    previous_elapsed = -1.0

    for expected_sequence, raw_checkpoint in enumerate(raw_checkpoints, start=1):
        if not isinstance(raw_checkpoint, dict):
            raise ValidationError("a checkpoint is not an object")

        sequence = raw_checkpoint.get("sequence")
        if sequence != expected_sequence:
            raise ValidationError(
                "checkpoint sequence is not contiguous "
                f"(expected {expected_sequence}, got {sequence!r})"
            )

        name = required_string(raw_checkpoint, "name", "checkpoint")
        if name in names:
            raise ValidationError(f"checkpoint '{name}' occurs more than once")

        elapsed = json_number(
            raw_checkpoint.get("elapsedMs"),
            f"checkpoint '{name}'.elapsedMs",
        )
        if elapsed < previous_elapsed:
            raise ValidationError(
                f"checkpoint '{name}' moves backwards in elapsed time"
            )
        previous_elapsed = elapsed

        required_object(raw_checkpoint, "memory", f"checkpoint '{name}'")
        required_object(raw_checkpoint, "metrics", f"checkpoint '{name}'")
        checkpoints.append(raw_checkpoint)
        names[name] = raw_checkpoint

    return checkpoints, names


def validate_report(
    report: dict[str, Any],
    scenario: str,
    settle_milliseconds: int,
) -> dict[str, Any]:
    if report.get("format") != PROFILE_FORMAT:
        raise ValidationError(
            f"report.format must be {PROFILE_FORMAT!r}, "
            f"got {report.get('format')!r}"
        )

    report_scenario = required_object(report, "scenario", "report")
    expected_scenario_name = f"{scenario}-startup"
    if report_scenario.get("name") != expected_scenario_name:
        raise ValidationError(
            f"report scenario must be {expected_scenario_name!r}, "
            f"got {report_scenario.get('name')!r}"
        )
    if report_scenario.get("settleMilliseconds") != settle_milliseconds:
        raise ValidationError(
            "report scenario settleMilliseconds does not match the requested "
            f"value ({settle_milliseconds})"
        )

    checkpoints, by_name = checkpoint_map(report)
    missing = [name for name in BASE_CHECKPOINTS if name not in by_name]
    if missing:
        raise ValidationError(f"missing required checkpoints: {', '.join(missing)}")

    positions = [
        next(
            index
            for index, checkpoint in enumerate(checkpoints)
            if checkpoint["name"] == name
        )
        for name in BASE_CHECKPOINTS
    ]
    if positions != sorted(positions):
        raise ValidationError("required startup checkpoints are out of order")

    expected_settled = {
        name
        for threshold, name in SETTLED_CHECKPOINTS
        if settle_milliseconds >= threshold
    }
    if (
        settle_milliseconds > 0
        and settle_milliseconds not in {1000, 5000, 30000}
    ):
        expected_settled.add("settled-final")

    actual_settled = {name for name in by_name if name.startswith("settled-")}
    missing_settled = expected_settled - actual_settled
    if missing_settled:
        raise ValidationError(
            "missing requested settled checkpoints: "
            + ", ".join(sorted(missing_settled))
        )
    unexpected_settled = actual_settled - expected_settled
    if unexpected_settled:
        raise ValidationError(
            "report contains settled checkpoints outside the requested interval: "
            + ", ".join(sorted(unexpected_settled))
        )

    startup_complete = by_name["startup-complete"]
    startup_metrics = required_object(startup_complete, "metrics", "startup-complete")
    startup_memory = required_object(startup_complete, "memory", "startup-complete")

    if startup_memory.get("available") is not True:
        raise ValidationError("startup-complete memory sample is unavailable")
    required_string(startup_memory, "platform", "startup-complete.memory")
    for field in ("workingSetBytes", "privateUsageBytes", "peakWorkingSetBytes"):
        value = json_number(
            startup_memory.get(field),
            f"startup-complete.memory.{field}",
        )
        if value <= 0:
            raise ValidationError(f"startup-complete.memory.{field} must be positive")
    if json_number(
        startup_memory["peakWorkingSetBytes"],
        "startup-complete.memory.peakWorkingSetBytes",
    ) < json_number(
        startup_memory["workingSetBytes"],
        "startup-complete.memory.workingSetBytes",
    ):
        raise ValidationError(
            "startup-complete peakWorkingSetBytes is below workingSetBytes"
        )

    for field in ("widgetCount", "registeredPageCount", "instantiatedPageCount"):
        if (
            json_number(
                startup_metrics.get(field),
                f"startup-complete.metrics.{field}",
            )
            <= 0
        ):
            raise ValidationError(f"startup-complete.metrics.{field} must be positive")

    expected_counts = {
        "registeredPageCount": 11,
        "instantiatedPageCount": 1,
        "liveScheduleWidgetCount": 1,
        "scheduleWidgetsCreated": 1,
        "scheduleRenderCount": 1 if scenario == "representative" else 0,
    }
    for field, expected in expected_counts.items():
        actual = json_number(
            startup_metrics.get(field),
            f"startup-complete.metrics.{field}",
        )
        if actual != expected:
            raise ValidationError(
                f"startup-complete.metrics.{field} must be {expected}, got {actual:g}"
            )

    events = report.get("events")
    if not isinstance(events, list):
        raise ValidationError("report.events is missing or not an array")

    startup_complete_elapsed = json_number(
        startup_complete.get("elapsedMs"),
        "startup-complete.elapsedMs",
    )
    event_names: list[str] = []
    page_identifiers: list[str] = []
    for raw_event in events:
        if not isinstance(raw_event, dict):
            raise ValidationError("an event is not an object")
        event_name = required_string(raw_event, "name", "event")
        event_names.append(event_name)
        event_elapsed = json_number(
            raw_event.get("elapsedMs"),
            f"event '{event_name}'",
        )

        if event_name in {"page-created", "schedule-render-start"}:
            if event_elapsed > startup_complete_elapsed:
                raise ValidationError(
                    f"{event_name} occurred after startup-complete"
                )
        if event_name == "page-created":
            page_identifiers.append(
                required_string(raw_event, "detail", "page-created")
            )

    if page_identifiers != ["my-workspace"]:
        raise ValidationError(
            "startup page construction must be exactly ['my-workspace'], "
            f"got {page_identifiers!r}"
        )

    if "page-created" not in event_names:
        raise ValidationError("report has no page-created event")
    if "schedule-widget-created" not in event_names:
        raise ValidationError("report has no schedule-widget-created event")

    if scenario == "representative":
        if (
            "schedule-render-start" not in event_names
            or "schedule-render-end" not in event_names
        ):
            raise ValidationError("representative report has no complete schedule render")
        if "schedule-widget-startup-diagnostic" not in event_names:
            raise ValidationError(
                "representative report has no schedule startup diagnostic"
            )
    elif any(
        name in {"schedule-render-start", "schedule-render-end"}
        for name in event_names
    ):
        raise ValidationError("minimal startup unexpectedly rendered a schedule")

    final_progress = json_number(report.get("finalProgress"), "report.finalProgress")
    if final_progress != 100:
        raise ValidationError(f"report.finalProgress must be 100, got {final_progress:g}")

    peak_memory = required_object(report, "peakMemory", "report")
    if peak_memory.get("available") is not True:
        raise ValidationError("report.peakMemory is unavailable")
    if peak_memory.get("checkpointSampleCount") != len(checkpoints):
        raise ValidationError(
            "report.peakMemory.checkpointSampleCount does not match checkpoint count"
        )
    if json_number(
        peak_memory.get("workingSetBytes"),
        "report.peakMemory.workingSetBytes",
    ) < max(
        json_number(
            required_object(
                checkpoint,
                "memory",
                f"checkpoint '{checkpoint['name']}'",
            ).get("workingSetBytes"),
            f"checkpoint '{checkpoint['name']}'.memory.workingSetBytes",
        )
        for checkpoint in checkpoints
    ):
        raise ValidationError("report.peakMemory is below a checkpoint working set")

    stable_names = ["window-shown", "startup-complete"] + [
        name for _, name in SETTLED_CHECKPOINTS if name in by_name
    ]
    if "settled-final" in by_name:
        stable_names.append("settled-final")
    baseline_metrics = required_object(
        by_name["startup-complete"],
        "metrics",
        "startup-complete",
    )
    stable_metric_fields = (
        "instantiatedPageCount",
        "registeredPageCount",
        "liveScheduleWidgetCount",
        "scheduleWidgetsCreated",
        "scheduleRenderCount",
    )
    for name in stable_names:
        metrics = required_object(by_name[name], "metrics", name)
        for field in stable_metric_fields:
            if metrics.get(field) != baseline_metrics.get(field):
                raise ValidationError(
                    f"{name}.metrics.{field} changed after startup-complete"
                )

    return {
        "checkpointNames": [checkpoint["name"] for checkpoint in checkpoints],
        "pageIdentifiers": page_identifiers,
        "eventNames": event_names,
        "startupCompleteMs": json_number(
            startup_complete["elapsedMs"],
            "startup-complete.elapsedMs",
        ),
        "windowShownMs": json_number(
            by_name["window-shown"]["elapsedMs"],
            "window-shown.elapsedMs",
        ),
        "stableCheckpointNames": stable_names,
    }


def write_text(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8", newline="\n")


def write_json(path: Path, value: Any) -> None:
    write_text(path, json.dumps(value, indent=2, sort_keys=True) + "\n")


def write_representative_settings(settings_root: Path) -> Path:
    settings_file = settings_root / "PaperCloud" / "ClassMngr.ini"
    settings_file.parent.mkdir(parents=True, exist_ok=True)
    write_text(
        settings_file,
        """[options]
theme=1
fontSize=2
language=1
saveMode=0
documentPageSpacing=2
documentViewerBackground=1
sidebarTooltipsEnabled=true
sidebarMarqueeEnabled=false

[updates]
automaticChecksEnabled=false
""",
    )
    return settings_file


def git_revision(project_root: Path) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(project_root), "rev-parse", "HEAD"],
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
    except (OSError, subprocess.TimeoutExpired):
        return "unknown"
    revision = result.stdout.strip()
    return revision if result.returncode == 0 and revision else "unknown"


def physical_memory_bytes() -> int | None:
    if hasattr(os, "sysconf"):
        try:
            pages = os.sysconf("SC_PHYS_PAGES")
            page_size = os.sysconf("SC_PAGE_SIZE")
            if pages > 0 and page_size > 0:
                return int(pages * page_size)
        except (OSError, ValueError):
            pass
    return None


def host_metadata(
    qt_version: str,
    build_configuration: str,
    display_scale_percent: int | None,
) -> dict[str, Any]:
    uname = platform.uname()
    return {
        "operatingSystem": {
            "system": uname.system,
            "release": uname.release,
            "version": uname.version,
        },
        "architecture": platform.machine(),
        "processor": platform.processor(),
        "logicalProcessors": os.cpu_count(),
        "physicalMemoryBytes": physical_memory_bytes(),
        "pythonVersion": platform.python_version(),
        "qtVersion": qt_version or "unknown",
        "buildConfiguration": build_configuration or "unknown",
        "displayScalePercent": display_scale_percent,
    }


def percentile(values: list[float], percentage: float) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("cannot calculate a percentile without values")
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * percentage / 100.0
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def statistics_for(values: list[float]) -> dict[str, float | int]:
    if not values:
        raise ValueError("cannot calculate statistics without values")
    return {
        "count": len(values),
        "minimum": min(values),
        "median": percentile(values, 50),
        "p95": percentile(values, 95),
        "maximum": max(values),
    }


def report_checkpoint(report: dict[str, Any], name: str) -> dict[str, Any]:
    for checkpoint in report["checkpoints"]:
        if checkpoint["name"] == name:
            return checkpoint
    raise KeyError(name)


def collect_checkpoint_measurements(
    reports: list[dict[str, Any]],
    checkpoint_names: list[str],
) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for checkpoint_name in checkpoint_names:
        checkpoint_values = [
            report_checkpoint(report, checkpoint_name) for report in reports
        ]
        elapsed_values = [
            json_number(
                checkpoint["elapsedMs"],
                f"{checkpoint_name}.elapsedMs",
            )
            for checkpoint in checkpoint_values
        ]
        memory: dict[str, Any] = {}
        metrics: dict[str, Any] = {}
        for field in MEMORY_FIELDS:
            memory[field] = statistics_for(
                [
                    json_number(
                        required_object(
                            checkpoint,
                            "memory",
                            checkpoint_name,
                        ).get(field),
                        f"{checkpoint_name}.memory.{field}",
                    )
                    for checkpoint in checkpoint_values
                ]
            )
        for field in APPLICATION_FIELDS:
            metrics[field] = statistics_for(
                [
                    json_number(
                        required_object(
                            checkpoint,
                            "metrics",
                            checkpoint_name,
                        ).get(field),
                        f"{checkpoint_name}.metrics.{field}",
                    )
                    for checkpoint in checkpoint_values
                ]
            )
        result[checkpoint_name] = {
            "elapsedMs": statistics_for(elapsed_values),
            "memory": memory,
            "metrics": metrics,
        }
    return result


def largest_positive_memory_increases(
    reports: list[dict[str, Any]],
    run_numbers: list[int],
) -> dict[str, Any]:
    largest: dict[str, dict[str, Any] | None] = {
        "workingSetBytes": None,
        "privateUsageBytes": None,
    }

    for report, run_number in zip(reports, run_numbers):
        checkpoints = report["checkpoints"]
        for previous, current in zip(checkpoints, checkpoints[1:]):
            previous_memory = previous["memory"]
            current_memory = current["memory"]
            for field in largest:
                delta = json_number(
                    current_memory[field],
                    f"{current['name']}.memory.{field}",
                ) - json_number(
                    previous_memory[field],
                    f"{previous['name']}.memory.{field}",
                )
                candidate = {
                    "run": run_number,
                    "from": previous["name"],
                    "to": current["name"],
                    "bytes": delta,
                }
                current_largest = largest[field]
                if delta > 0 and (
                    current_largest is None or delta > current_largest["bytes"]
                ):
                    largest[field] = candidate

    return largest


def relative_path(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return str(path)


def run_capture(
    *,
    app_path: Path,
    project_root: Path,
    output_dir: Path,
    run_number: int,
    scenario: str,
    fixture: Path | None,
    fixture_sha256: str | None,
    settle_milliseconds: int,
    timeout_milliseconds: int,
    qpa_platform: str | None,
    runtime_dir: Path | None,
    source_revision: str,
    host: dict[str, Any],
) -> tuple[dict[str, Any], dict[str, Any]]:
    run_dir = output_dir / f"run-{run_number:03d}"
    run_dir.mkdir()

    settings_root = run_dir / "settings"
    settings_root.mkdir()
    settings_file: Path | None = None
    if scenario == "representative":
        settings_file = write_representative_settings(settings_root)

    copied_fixture: Path | None = None
    if fixture is not None:
        copied_fixture = run_dir / fixture.name
        shutil.copy2(fixture, copied_fixture)

    metrics_path = run_dir / "startup-metrics.json"
    stdout_path = run_dir / "stdout.log"
    stderr_path = run_dir / "stderr.log"
    metadata_path = run_dir / "run-metadata.json"

    command = [
        str(app_path),
        "--startup-performance-test",
        "--startup-performance-scenario",
        scenario,
        "--startup-performance-settle-ms",
        str(settle_milliseconds),
        "--startup-performance-output",
        str(metrics_path),
    ]
    if copied_fixture is not None:
        command.append(str(copied_fixture))

    environment = os.environ.copy()
    environment["CLASSMNGR_SETTINGS_ROOT"] = str(settings_root)
    if qpa_platform:
        environment["QT_QPA_PLATFORM"] = qpa_platform
    if runtime_dir is not None:
        environment["PATH"] = (
            str(runtime_dir)
            + os.pathsep
            + environment.get("PATH", "")
        )

    metadata: dict[str, Any] = {
        "format": RUN_METADATA_FORMAT,
        "capturedAtUtc": utc_now(),
        "run": run_number,
        "scenario": scenario,
        "settleMilliseconds": settle_milliseconds,
        "timeoutMilliseconds": timeout_milliseconds,
        "sourceRevision": source_revision,
        "qpaPlatform": qpa_platform or os.environ.get(
            "QT_QPA_PLATFORM",
            "inherited",
        ),
        "application": {
            "path": str(app_path),
            "name": app_path.name,
        },
        "host": host,
        "fixture": (
            {
                "source": str(fixture),
                "copiedFile": copied_fixture.name if copied_fixture else None,
                "sha256": fixture_sha256,
            }
            if fixture is not None
            else None
        ),
        "settingsFile": str(settings_file) if settings_file is not None else None,
        "command": command,
        "status": "started",
    }

    started = time.perf_counter()
    error_message: str | None = None
    try:
        completed = subprocess.run(
            command,
            cwd=project_root,
            env=environment,
            capture_output=True,
            text=True,
            errors="replace",
            timeout=timeout_milliseconds / 1000.0,
            check=False,
        )
        write_text(stdout_path, completed.stdout)
        write_text(stderr_path, completed.stderr)
        metadata["exitCode"] = completed.returncode
        metadata["processElapsedMs"] = round(
            (time.perf_counter() - started) * 1000,
            3,
        )
        if completed.returncode != 0:
            error_message = (
                f"application exited with code {completed.returncode}; "
                f"see {stdout_path} and {stderr_path}"
            )
    except subprocess.TimeoutExpired as error:
        stdout = error.stdout or ""
        stderr = error.stderr or ""
        write_text(
            stdout_path,
            stdout
            if isinstance(stdout, str)
            else stdout.decode(errors="replace"),
        )
        write_text(
            stderr_path,
            stderr
            if isinstance(stderr, str)
            else stderr.decode(errors="replace"),
        )
        metadata["processElapsedMs"] = round(
            (time.perf_counter() - started) * 1000,
            3,
        )
        metadata["timedOut"] = True
        error_message = (
            f"application timed out after {timeout_milliseconds} ms; "
            f"see {stdout_path} and {stderr_path}"
        )
    except OSError as error:
        metadata["processElapsedMs"] = round(
            (time.perf_counter() - started) * 1000,
            3,
        )
        error_message = f"unable to launch application: {error}"

    if error_message is None and not metrics_path.is_file():
        error_message = f"application did not write {metrics_path}"

    report: dict[str, Any] | None = None
    derived: dict[str, Any] | None = None
    if error_message is None:
        try:
            report_value = json.loads(metrics_path.read_text(encoding="utf-8"))
            if not isinstance(report_value, dict):
                raise ValidationError("startup metrics JSON is not an object")
            report = report_value
            derived = validate_report(report, scenario, settle_milliseconds)
        except (OSError, json.JSONDecodeError, ValidationError) as error:
            error_message = f"invalid startup report: {error}"

    if error_message is not None:
        metadata["status"] = "failed"
        metadata["error"] = error_message
        write_json(metadata_path, metadata)
        raise ValidationError(f"run {run_number}: {error_message}")

    assert report is not None
    assert derived is not None
    metadata["status"] = "passed"
    metadata["report"] = {
        "file": metrics_path.name,
        "checkpointCount": len(report["checkpoints"]),
        "startupCompleteMs": derived["startupCompleteMs"],
        "windowShownMs": derived["windowShownMs"],
    }
    metadata["stdoutFile"] = stdout_path.name
    metadata["stderrFile"] = stderr_path.name
    write_json(metadata_path, metadata)
    return report, {
        "run": run_number,
        "reportFile": relative_path(metrics_path, output_dir),
        "metadataFile": relative_path(metadata_path, output_dir),
        "derived": derived,
    }


def aggregate_reports(
    *,
    reports: list[dict[str, Any]],
    run_records: list[dict[str, Any]],
    scenario: str,
    settle_milliseconds: int,
    source_revision: str,
    host: dict[str, Any],
    fixture: Path | None,
    fixture_sha256: str | None,
    output_dir: Path,
) -> dict[str, Any]:
    checkpoint_names = [
        checkpoint["name"] for checkpoint in reports[0]["checkpoints"]
    ]
    for report in reports[1:]:
        names = [checkpoint["name"] for checkpoint in report["checkpoints"]]
        if names != checkpoint_names:
            raise ValidationError(
                "startup runs produced different checkpoint sequences"
            )
    measurements = collect_checkpoint_measurements(reports, checkpoint_names)
    process_elapsed = [
        float(record["processElapsedMs"])
        for record in run_records
        if record.get("processElapsedMs") is not None
    ]

    for record, report in zip(run_records, reports):
        record["startupCompleteMs"] = report_checkpoint(
            report,
            "startup-complete",
        )["elapsedMs"]

    summary: dict[str, Any] = {
        "format": SUMMARY_FORMAT,
        "generatedAtUtc": utc_now(),
        "sourceRevision": source_revision,
        "scenario": {
            "name": f"{scenario}-startup",
            "requestedRuns": len(reports),
            "settleMilliseconds": settle_milliseconds,
            "qpaPlatform": "inherited",
        },
        "host": host,
        "fixture": (
            {
                "source": str(fixture),
                "sha256": fixture_sha256,
            }
            if fixture is not None
            else None
        ),
        "runs": run_records,
        "measurements": {
            "processElapsedMs": statistics_for(process_elapsed)
            if process_elapsed
            else None,
            "checkpoints": measurements,
        },
        "largestPositiveMemoryIncreases": largest_positive_memory_increases(
            reports,
            [int(record["run"]) for record in run_records],
        ),
        "artifacts": {
            "rawRunDirectories": [
                relative_path(
                    output_dir / f"run-{int(record['run']):03d}",
                    output_dir,
                )
                for record in run_records
            ],
            "summaryJson": "summary.json",
            "summaryMarkdown": "summary.md",
        },
    }
    return summary


def format_stat(statistics: dict[str, Any] | None, suffix: str = "") -> str:
    if statistics is None:
        return "n/a"
    return (
        f"median={statistics['median']:.1f}, "
        f"p95={statistics['p95']:.1f}, "
        f"max={statistics['maximum']:.1f}{suffix}"
    )


def bytes_to_mib(statistics: dict[str, Any]) -> dict[str, Any]:
    return {
        key: value / (1024 * 1024) if key != "count" else value
        for key, value in statistics.items()
    }


def summary_markdown(summary: dict[str, Any]) -> str:
    lines = [
        "# ClassMngr startup baseline",
        "",
        f"- Source revision: {summary['sourceRevision']}",
        f"- Scenario: {summary['scenario']['name']}",
        f"- Runs: {summary['scenario']['requestedRuns']}",
        f"- Settle interval: {summary['scenario']['settleMilliseconds']} ms",
        (
            "- Host: "
            f"{summary['host']['operatingSystem']['system']} "
            f"{summary['host']['operatingSystem']['release']} "
            f"({summary['host']['architecture']})"
        ),
        "",
        "## Checkpoint summary",
        "",
        (
            "| Checkpoint | Elapsed (ms) | Working set (MiB) | "
            "Private/PSS (MiB) | Widgets | Pages | Schedule renders |"
        ),
        "| --- | --- | --- | --- | ---: | ---: | ---: |",
    ]
    checkpoint_measurements = summary["measurements"]["checkpoints"]
    for name, measurement in checkpoint_measurements.items():
        memory = measurement["memory"]
        metrics = measurement["metrics"]
        lines.append(
            (
                f"| {name} | "
                f"{format_stat(measurement['elapsedMs'])} | "
                f"{format_stat(bytes_to_mib(memory['workingSetBytes']), ' MiB')} | "
                f"{format_stat(bytes_to_mib(memory['privateUsageBytes']), ' MiB')} | "
                f"{metrics['widgetCount']['median']:.1f} | "
                f"{metrics['instantiatedPageCount']['median']:.1f} | "
                f"{metrics['scheduleRenderCount']['median']:.1f} |"
            )
        )

    largest = summary["largestPositiveMemoryIncreases"]
    lines.extend(["", "## Largest positive memory increases", ""])
    for field, item in largest.items():
        if item is None:
            lines.append(f"- {field}: no positive increase")
        else:
            lines.append(
                f"- {field}: {item['bytes'] / (1024 * 1024):.1f} MiB "
                f"from {item['from']} to {item['to']} (run {item['run']})"
            )

    lines.extend(
        [
            "",
            "Raw reports and stdout/stderr logs are in the per-run directories.",
            "",
        ]
    )
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture and aggregate repeatable ClassMngr startup metrics."
    )
    parser.add_argument(
        "--app-path",
        required=True,
        type=Path,
        help="Path to the ClassMngr executable.",
    )
    parser.add_argument(
        "--output-dir",
        required=True,
        type=Path,
        help="New or empty directory in which to store the captures.",
    )
    parser.add_argument(
        "--scenario",
        choices=("minimal", "representative"),
        default="representative",
        help="Startup scenario to execute (default: representative).",
    )
    parser.add_argument(
        "--fixture",
        type=Path,
        help="Representative .tps fixture; defaults to checked-in Testing-copy.tps.",
    )
    parser.add_argument(
        "--runs",
        type=positive_int,
        default=3,
        help="Number of independent captures (default: 3).",
    )
    parser.add_argument(
        "--settle-ms",
        type=nonnegative_int,
        help="Settling interval; representative defaults to 30000 and minimal to 0.",
    )
    parser.add_argument(
        "--timeout-ms",
        type=positive_int,
        default=120000,
        help="Per-run process timeout (default: 120000).",
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        help="Repository root; defaults to the parent of scripts/.",
    )
    parser.add_argument(
        "--runtime-dir",
        type=Path,
        help="Optional Qt/runtime directory to prepend to PATH.",
    )
    parser.add_argument(
        "--qpa-platform",
        help="Optional value for QT_QPA_PLATFORM, e.g. offscreen.",
    )
    parser.add_argument(
        "--qt-version",
        default=os.environ.get("QT_VERSION", "unknown"),
        help="Qt version to record in metadata (default: QT_VERSION or unknown).",
    )
    parser.add_argument(
        "--build-configuration",
        default=os.environ.get("CLASSMNGR_BUILD_CONFIGURATION", "unknown"),
        help="Build configuration to record (default: CLASSMNGR_BUILD_CONFIGURATION or unknown).",
    )
    parser.add_argument(
        "--display-scale-percent",
        type=nonnegative_int,
        help="Display scale to record, when known (for example, 100 or 200).",
    )
    return parser


def main(argv: list[str]) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    script_root = Path(__file__).resolve().parent
    project_root = (
        args.project_root.resolve()
        if args.project_root is not None
        else script_root.parent.resolve()
    )
    app_path = args.app_path.resolve()
    output_dir = args.output_dir.resolve()
    runtime_dir = args.runtime_dir.resolve() if args.runtime_dir is not None else None

    if not app_path.is_file():
        parser.error(f"application does not exist: {app_path}")
    if not project_root.is_dir():
        parser.error(f"project root does not exist: {project_root}")
    if runtime_dir is not None and not runtime_dir.is_dir():
        parser.error(f"runtime directory does not exist: {runtime_dir}")
    if output_dir == project_root:
        parser.error("refusing to use the repository root as --output-dir")
    if output_dir.exists() and any(output_dir.iterdir()):
        parser.error(f"output directory must be empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    settle_milliseconds = args.settle_ms
    if settle_milliseconds is None:
        settle_milliseconds = 30000 if args.scenario == "representative" else 0

    fixture: Path | None = None
    if args.scenario == "representative":
        fixture = (
            args.fixture.resolve()
            if args.fixture is not None
            else project_root
            / "plans"
            / "startup-sequence-optimization-plan"
            / "Testing-copy.tps"
        )
        if not fixture.is_file():
            parser.error(f"representative fixture does not exist: {fixture}")
    elif args.fixture is not None:
        parser.error("--fixture is only valid with --scenario representative")

    source_revision = git_revision(project_root)
    host = host_metadata(
        args.qt_version,
        args.build_configuration,
        args.display_scale_percent,
    )
    fixture_sha256 = sha256_file(fixture) if fixture is not None else None

    print(
        f"Capturing {args.runs} {args.scenario} startup runs "
        f"(settle={settle_milliseconds} ms)"
    )
    print(f"Output: {output_dir}")

    reports: list[dict[str, Any]] = []
    run_records: list[dict[str, Any]] = []
    for run_number in range(1, args.runs + 1):
        print(f"Run {run_number}/{args.runs}...", flush=True)
        try:
            report, record = run_capture(
                app_path=app_path,
                project_root=project_root,
                output_dir=output_dir,
                run_number=run_number,
                scenario=args.scenario,
                fixture=fixture,
                fixture_sha256=fixture_sha256,
                settle_milliseconds=settle_milliseconds,
                timeout_milliseconds=args.timeout_ms,
                qpa_platform=args.qpa_platform,
                runtime_dir=runtime_dir,
                source_revision=source_revision,
                host=host,
            )
        except ValidationError as error:
            print(f"ERROR: {error}", file=sys.stderr)
            return 1

        metadata_path = output_dir / f"run-{run_number:03d}" / "run-metadata.json"
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        record["processElapsedMs"] = metadata.get("processElapsedMs")
        reports.append(report)
        run_records.append(record)
        print(
            f"  startup-complete={record['derived']['startupCompleteMs']:.0f} ms, "
            f"process={metadata.get('processElapsedMs', 0):.0f} ms"
        )

    summary = aggregate_reports(
        reports=reports,
        run_records=run_records,
        scenario=args.scenario,
        settle_milliseconds=settle_milliseconds,
        source_revision=source_revision,
        host=host,
        fixture=fixture,
        fixture_sha256=fixture_sha256,
        output_dir=output_dir,
    )
    summary["scenario"]["qpaPlatform"] = args.qpa_platform or os.environ.get(
        "QT_QPA_PLATFORM",
        "inherited",
    )

    summary_json = output_dir / "summary.json"
    summary_markdown_path = output_dir / "summary.md"
    write_json(summary_json, summary)
    write_text(summary_markdown_path, summary_markdown(summary))

    print(f"Wrote {summary_json}")
    print(f"Wrote {summary_markdown_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
