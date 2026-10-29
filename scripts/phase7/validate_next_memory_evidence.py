#!/usr/bin/env python3
"""Validate externally sampled Windows x64 Release ClassMngr Next evidence.

This validates collection identity, readiness, sample cadence, and normal
completion. It deliberately defines no numeric memory pass threshold.
"""

from __future__ import annotations

import argparse
import json
import ntpath
import re
import statistics
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


TASK_ID = "P7A-NEXT-MEMORY-IMPLEMENT"
MANIFEST_SCHEMA = "classmngr-phase7-next-memory-v1"
SUMMARY_SCHEMA = "classmngr-phase7-next-memory-summary-v1"
EXPECTED_SCENARIOS = (
    "fresh-initial-setup",
    "schema-only-blank-workspace",
    "representative-96-class-workspace",
)
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
REVISION_RE = re.compile(r"^(?:[0-9a-f]{40}|[0-9a-f]{64})$")
SAMPLE_INTERVAL_MIN_SECONDS = 0.75
SAMPLE_INTERVAL_MAX_SECONDS = 1.5


class EvidenceValidationError(ValueError):
    """Raised when evidence cannot be treated as a valid complete run."""


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


def _read_metrics(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8") as stream:
            for line_number, line in enumerate(stream, start=1):
                if not line.strip():
                    raise EvidenceValidationError(
                        f"metrics trace has an empty line at {line_number}"
                    )
                try:
                    row = json.loads(line)
                except json.JSONDecodeError as error:
                    raise EvidenceValidationError(
                        f"metrics trace line {line_number} is invalid JSON"
                    ) from error
                rows.append(_require_object(row, f"metrics trace line {line_number}"))
    except OSError as error:
        raise EvidenceValidationError(f"cannot read metrics trace: {error}") from error
    return rows


def validate_documents(
    manifest: dict[str, Any], metrics: list[dict[str, Any]]
) -> dict[str, Any]:
    """Return the concise observed-data summary or raise on invalid evidence."""
    if manifest.get("schema") != MANIFEST_SCHEMA:
        raise EvidenceValidationError("unexpected manifest schema")
    if manifest.get("taskId") != TASK_ID:
        raise EvidenceValidationError("unexpected taskId")
    if manifest.get("status") != "completed":
        raise EvidenceValidationError("run status is not completed")

    revision = manifest.get("revision")
    if not isinstance(revision, str) or not REVISION_RE.fullmatch(revision):
        raise EvidenceValidationError("revision must be a full Git object ID")
    _parse_timestamp(manifest.get("startedAtUtc"), "startedAtUtc")
    run_completed = _parse_timestamp(manifest.get("completedAtUtc"), "completedAtUtc")
    run_started = _parse_timestamp(manifest.get("startedAtUtc"), "startedAtUtc")
    if run_completed < run_started:
        raise EvidenceValidationError("completedAtUtc precedes startedAtUtc")

    configuration = _require_object(manifest.get("configuration"), "configuration")
    if configuration.get("preset") != "windows-x64-release":
        raise EvidenceValidationError("configuration preset is not windows-x64-release")
    if configuration.get("buildType") != "Release":
        raise EvidenceValidationError("configuration buildType is not Release")
    if configuration.get("architecture") != "x64":
        raise EvidenceValidationError("configuration architecture is not x64")

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
        raise EvidenceValidationError("application path is not ClassMngrNext.exe")
    if application.get("architecture") != "x64":
        raise EvidenceValidationError("staged application architecture is not x64")
    application_sha = _require_sha(application.get("sha256"), "application sha256")

    scenarios = manifest.get("scenarios")
    if not isinstance(scenarios, list) or len(scenarios) != len(EXPECTED_SCENARIOS):
        raise EvidenceValidationError("manifest must contain exactly three scenarios")
    actual_ids = [scenario.get("id") if isinstance(scenario, dict) else None for scenario in scenarios]
    if tuple(actual_ids) != EXPECTED_SCENARIOS:
        raise EvidenceValidationError("scenario IDs are missing, duplicated, or out of order")

    scenario_summaries: list[dict[str, Any]] = []
    expected_fixture_shapes = {
        EXPECTED_SCENARIOS[0]: ("fresh-no-profile", False, None),
        EXPECTED_SCENARIOS[1]: ("schema-only-v6-empty", True, 0),
        EXPECTED_SCENARIOS[2]: ("synthetic-large-startup-96-classes", True, 96),
    }
    consumed_rows: set[int] = set()
    previous_exit: datetime | None = None

    for scenario in scenarios:
        scenario = _require_object(scenario, "scenario")
        scenario_id = scenario["id"]
        if scenario.get("outcome") != "completed":
            raise EvidenceValidationError(f"{scenario_id} outcome is not completed")
        scenario_started = _parse_timestamp(
            scenario.get("startedAtUtc"), f"{scenario_id}.startedAtUtc"
        )
        readiness = _require_object(scenario.get("readiness"), f"{scenario_id}.readiness")
        ready_at = _parse_timestamp(
            readiness.get("readyAtUtc"), f"{scenario_id}.readiness.readyAtUtc"
        )
        if readiness.get("windowVisible") is not True:
            raise EvidenceValidationError(f"{scenario_id} did not prove a visible window")
        if readiness.get("stablePolls") != 3:
            raise EvidenceValidationError(f"{scenario_id} did not use three stable readiness polls")
        ready_handle = _require_positive_int(
            readiness.get("mainWindowHandle"), f"{scenario_id}.mainWindowHandle"
        )
        title = readiness.get("mainWindowTitle")
        if not isinstance(title, str) or "ClassMngr Next" not in title:
            raise EvidenceValidationError(f"{scenario_id} has an unexpected main window title")

        fixture = _require_object(scenario.get("fixture"), f"{scenario_id}.fixture")
        expected_fixture_id, argument_supplied, class_count = expected_fixture_shapes[
            scenario_id
        ]
        if fixture.get("id") != expected_fixture_id:
            raise EvidenceValidationError(f"{scenario_id} fixture identity is unexpected")
        _require_sha(fixture.get("sha256"), f"{scenario_id}.fixture.sha256")
        if fixture.get("profilePathArgumentSupplied") is not argument_supplied:
            raise EvidenceValidationError(f"{scenario_id} fixture argument state is incorrect")
        if fixture.get("classCount") != class_count:
            raise EvidenceValidationError(f"{scenario_id} fixture classCount is incorrect")

        process = _require_object(scenario.get("process"), f"{scenario_id}.process")
        pid = _require_positive_int(process.get("pid"), f"{scenario_id}.process.pid")
        created_at_text = process.get("createdAtUtc")
        created_at = _parse_timestamp(created_at_text, f"{scenario_id}.process.createdAtUtc")
        if created_at > ready_at:
            raise EvidenceValidationError(
                f"{scenario_id} process was created after visible-window readiness"
            )
        process_path = process.get("canonicalExecutablePath")
        if not _same_windows_path(process_path, executable_path):
            raise EvidenceValidationError(f"{scenario_id} process image path differs from staged image")
        if process.get("architecture") != "x64":
            raise EvidenceValidationError(f"{scenario_id} process architecture is not x64")
        if _require_sha(process.get("sha256"), f"{scenario_id}.process.sha256") != application_sha:
            raise EvidenceValidationError(f"{scenario_id} process image hash differs from staged image")
        if process.get("mainWindowHandle") != ready_handle:
            raise EvidenceValidationError(f"{scenario_id} process and readiness window handles differ")
        if process.get("mainWindowTitle") != title:
            raise EvidenceValidationError(f"{scenario_id} process and readiness window titles differ")

        settle_seconds = scenario.get("settleSeconds")
        if isinstance(settle_seconds, bool) or not isinstance(settle_seconds, int) or settle_seconds < 1:
            raise EvidenceValidationError(f"{scenario_id} settleSeconds must be positive")
        samples = [
            (index, row)
            for index, row in enumerate(metrics)
            if row.get("scenarioId") == scenario_id
        ]
        if len(samples) != 5:
            raise EvidenceValidationError(f"{scenario_id} must contain exactly five metric samples")

        sample_times: list[datetime] = []
        sample_metrics: dict[str, list[int]] = {
            "currentWorkingSetBytes": [],
            "privateBytes": [],
            "peakWorkingSetBytes": [],
            "handles": [],
            "threads": [],
        }
        for expected_index, (row_index, sample) in enumerate(samples, start=1):
            if row_index in consumed_rows:
                raise EvidenceValidationError("a metric row was assigned to multiple scenarios")
            consumed_rows.add(row_index)
            if sample.get("sampleIndex") != expected_index:
                raise EvidenceValidationError(f"{scenario_id} sample indices are missing or out of order")
            if sample.get("pid") != pid:
                raise EvidenceValidationError(f"{scenario_id} sample PID differs from its process")
            if sample.get("processCreatedAtUtc") != created_at_text:
                raise EvidenceValidationError(f"{scenario_id} sample creation time differs from its process")
            if not _same_windows_path(sample.get("canonicalExecutablePath"), executable_path):
                raise EvidenceValidationError(f"{scenario_id} sample path differs from staged image")
            if sample.get("architecture") != "x64":
                raise EvidenceValidationError(f"{scenario_id} sample architecture is not x64")
            if _require_sha(sample.get("sha256"), f"{scenario_id} sample sha256") != application_sha:
                raise EvidenceValidationError(f"{scenario_id} sample image hash differs from staged image")
            sample_times.append(
                _parse_timestamp(sample.get("timestampUtc"), f"{scenario_id}.sample.timestampUtc")
            )
            for metric_name, values in sample_metrics.items():
                value = _require_nonnegative_int(
                    sample.get(metric_name), f"{scenario_id}.{metric_name}"
                )
                if metric_name in ("currentWorkingSetBytes", "privateBytes", "peakWorkingSetBytes") and value == 0:
                    raise EvidenceValidationError(f"{scenario_id}.{metric_name} must be positive")
                values.append(value)

        if any(right <= left for left, right in zip(sample_times, sample_times[1:])):
            raise EvidenceValidationError(f"{scenario_id} sample timestamps are not strictly ordered")
        for left, right in zip(sample_times, sample_times[1:]):
            interval = (right - left).total_seconds()
            if not SAMPLE_INTERVAL_MIN_SECONDS <= interval <= SAMPLE_INTERVAL_MAX_SECONDS:
                raise EvidenceValidationError(
                    f"{scenario_id} sample interval {interval:.3f}s is outside the one-second cadence"
                )
        if sample_times[0] < ready_at.replace(microsecond=ready_at.microsecond) or (
            sample_times[0] - ready_at
        ).total_seconds() < settle_seconds:
            raise EvidenceValidationError(f"{scenario_id} was sampled before its settle period ended")
        if scenario_started > ready_at or created_at > sample_times[0]:
            raise EvidenceValidationError(f"{scenario_id} timestamp order is invalid")

        close_requested = _parse_timestamp(
            scenario.get("closeRequestedAtUtc"), f"{scenario_id}.closeRequestedAtUtc"
        )
        exited_at = _parse_timestamp(scenario.get("exitedAtUtc"), f"{scenario_id}.exitedAtUtc")
        if close_requested < sample_times[-1] or exited_at < close_requested:
            raise EvidenceValidationError(f"{scenario_id} process completion timestamps are invalid")
        if scenario.get("normalExit") is not True or scenario.get("exitCode") != 0:
            raise EvidenceValidationError(f"{scenario_id} did not exit normally with code 0")
        if exited_at > run_completed:
            raise EvidenceValidationError(f"{scenario_id} exit occurred after run completion")
        if previous_exit is not None and scenario_started < previous_exit:
            raise EvidenceValidationError("scenario processes were not isolated sequentially")
        previous_exit = exited_at

        def metric_summary(values: list[int]) -> dict[str, int | float]:
            return {
                "min": min(values),
                "median": statistics.median(values),
                "max": max(values),
            }

        scenario_summaries.append(
            {
                "id": scenario_id,
                "outcome": "completed",
                "fixtureId": fixture["id"],
                "fixtureSha256": fixture["sha256"],
                "sampleCount": 5,
                "sampleStartUtc": sample_times[0].isoformat().replace("+00:00", "Z"),
                "sampleEndUtc": sample_times[-1].isoformat().replace("+00:00", "Z"),
                "currentWorkingSetBytes": metric_summary(sample_metrics["currentWorkingSetBytes"]),
                "privateBytes": metric_summary(sample_metrics["privateBytes"]),
                "peakWorkingSetBytes": metric_summary(sample_metrics["peakWorkingSetBytes"]),
                "handles": metric_summary(sample_metrics["handles"]),
                "threads": metric_summary(sample_metrics["threads"]),
            }
        )

    if len(consumed_rows) != len(metrics):
        raise EvidenceValidationError("metrics trace contains rows for an unknown scenario")

    return {
        "schema": SUMMARY_SCHEMA,
        "taskId": TASK_ID,
        "validation": "valid",
        "runStatus": "completed",
        "revision": revision,
        "configuration": configuration,
        "os": operating_system,
        "application": application,
        "scenarioCount": len(scenario_summaries),
        "sampleCount": len(metrics),
        "numericMemoryPassThreshold": None,
        "interpretation": "Observed external process memory data; Phase 7 defines no numeric memory pass threshold.",
        "scenarios": scenario_summaries,
        "errors": [],
    }


def _load_manifest(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise EvidenceValidationError(f"cannot read manifest: {error}") from error
    return _require_object(value, "manifest")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--metrics", required=True, type=Path)
    parser.add_argument("--summary", required=True, type=Path)
    arguments = parser.parse_args(argv)

    try:
        summary = validate_documents(
            _load_manifest(arguments.manifest), _read_metrics(arguments.metrics)
        )
        exit_code = 0
    except EvidenceValidationError as error:
        summary = {
            "schema": SUMMARY_SCHEMA,
            "taskId": TASK_ID,
            "validation": "invalid",
            "numericMemoryPassThreshold": None,
            "interpretation": "Evidence collection is incomplete or invalid; no memory threshold was evaluated.",
            "errors": [str(error)],
        }
        exit_code = 1

    arguments.summary.parent.mkdir(parents=True, exist_ok=True)
    arguments.summary.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps({"validation": summary["validation"], "errors": summary["errors"]}))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
