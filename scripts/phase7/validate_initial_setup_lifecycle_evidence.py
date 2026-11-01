#!/usr/bin/env python3
"""Validate the focused Phase 7 Initial Setup lifecycle evidence artifact."""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


TASK_ID = "P7A-C54-INITIAL-SETUP-LIFECYCLE"
SCHEMA = "classmngr-phase7-initial-setup-lifecycle-v1"
EXPECTED_CHECKPOINTS = (
    "entry.initial-setup",
    "setup.active",
    "save-failure.draft-retained",
    "setup.completed",
    "replacement.entry",
    "replacement.active",
    "cancel.restored-original-profile",
)
EXPECTED_SECTIONS = ("personalDetails", "teacherEntry", "classDetails", "classTimes")


class EvidenceValidationError(ValueError):
    """Raised when the Initial Setup lifecycle artifact is incomplete."""


def _object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise EvidenceValidationError(f"{label} must be an object")
    return value


def _timestamp(value: Any, label: str) -> datetime:
    if not isinstance(value, str):
        raise EvidenceValidationError(f"{label} must be an ISO timestamp")
    normalized = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError as error:
        raise EvidenceValidationError(f"{label} must be an ISO timestamp") from error
    if parsed.tzinfo is None or parsed.utcoffset() != timezone.utc.utcoffset(parsed):
        raise EvidenceValidationError(f"{label} must use UTC")
    return parsed.astimezone(timezone.utc)


def _nonnegative_int(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise EvidenceValidationError(f"{label} must be a nonnegative integer")
    return value


def validate_document(document: dict[str, Any]) -> dict[str, Any]:
    """Validate completeness and return a concise validation result."""
    if document.get("schema") != SCHEMA:
        raise EvidenceValidationError("unexpected schema")
    if document.get("taskId") != TASK_ID:
        raise EvidenceValidationError("unexpected taskId")
    if document.get("status") != "completed":
        raise EvidenceValidationError("capture status is not completed")
    if document.get("feature") != "Initial Setup":
        raise EvidenceValidationError("feature is not Initial Setup")
    started = _timestamp(document.get("startedAtUtc"), "startedAtUtc")
    completed = _timestamp(document.get("completedAtUtc"), "completedAtUtc")
    if completed < started:
        raise EvidenceValidationError("completedAtUtc precedes startedAtUtc")
    if document.get("numericMemoryPassThreshold", "missing") is not None:
        raise EvidenceValidationError("numeric memory threshold must be null")

    fixture = _object(document.get("fixture"), "fixture")
    if fixture.get("syntheticDataOnly") is not True:
        raise EvidenceValidationError("fixture must use synthetic values only")
    if fixture.get("screenshotCaptured") is not False:
        raise EvidenceValidationError("the lifecycle capture must not duplicate screenshots")

    measurement_scope = _object(document.get("measurementScope"), "measurementScope")
    memory_scope = measurement_scope.get("processMemory")
    if not isinstance(memory_scope, str) or "whole-process" not in memory_scope.casefold():
        raise EvidenceValidationError("process memory scope must be labeled whole-process")
    if "not page-attributed" not in memory_scope:
        raise EvidenceValidationError("process memory must not be presented as page-attributed")
    if "not a per-page memory ceiling" not in memory_scope:
        raise EvidenceValidationError("process memory scope must disclaim a per-page ceiling")

    checkpoints = document.get("checkpoints")
    if not isinstance(checkpoints, list) or len(checkpoints) != len(EXPECTED_CHECKPOINTS):
        raise EvidenceValidationError("artifact must contain exactly seven checkpoints")
    if document.get("checkpointCount") != len(checkpoints):
        raise EvidenceValidationError("checkpointCount does not match checkpoints")
    ids = [item.get("id") if isinstance(item, dict) else None for item in checkpoints]
    if tuple(ids) != EXPECTED_CHECKPOINTS:
        raise EvidenceValidationError("checkpoint IDs are missing, duplicated, or out of order")

    prior_elapsed = -1
    checked: dict[str, dict[str, Any]] = {}
    for index, raw_checkpoint in enumerate(checkpoints):
        checkpoint = _object(raw_checkpoint, f"checkpoints[{index}]")
        elapsed = _nonnegative_int(checkpoint.get("elapsedMs"), "elapsedMs")
        if elapsed < prior_elapsed:
            raise EvidenceValidationError("checkpoints are not ordered by elapsedMs")
        prior_elapsed = elapsed

        state = _object(checkpoint.get("state"), "checkpoint state")
        for key in (
            "setupPending",
            "recoveryAvailable",
            "workspaceSessionActive",
            "profileExists",
            "recoveryJournalExists",
            "originalBackupExists",
            "classDetailsDraftAvailable",
        ):
            if not isinstance(state.get(key), bool):
                raise EvidenceValidationError(f"checkpoint state {key} must be boolean")
        counts = _object(checkpoint.get("featureCounts"), "featureCounts")
        page_widgets = _nonnegative_int(
            counts.get("pageWidgetCount"), "featureCounts.pageWidgetCount"
        )
        visible_widgets = _nonnegative_int(
            counts.get("visiblePageWidgetCount"),
            "featureCounts.visiblePageWidgetCount",
        )
        if page_widgets <= 0 or visible_widgets > page_widgets:
            raise EvidenceValidationError("Initial Setup widget counts are inconsistent")
        visible_actions = _nonnegative_int(
            counts.get("visibleActionCount"), "visibleActionCount"
        )
        if not 0 < visible_actions <= 6:
            raise EvidenceValidationError("Initial Setup visible action count is inconsistent")
        section_widgets = _object(counts.get("sectionWidgets"), "sectionWidgets")
        if set(section_widgets) != set(EXPECTED_SECTIONS):
            raise EvidenceValidationError("Initial Setup section counts are incomplete")
        for section_name in EXPECTED_SECTIONS:
            section = _object(section_widgets.get(section_name), section_name)
            if section.get("present") is not True:
                raise EvidenceValidationError(f"Initial Setup section {section_name} is missing")
            if not isinstance(section.get("visible"), bool):
                raise EvidenceValidationError(
                    f"Initial Setup section {section_name} visibility is missing"
                )
            section_count = _nonnegative_int(
                section.get("widgetCount"), f"{section_name}.widgetCount"
            )
            visible_section_count = _nonnegative_int(
                section.get("visibleWidgetCount"), f"{section_name}.visibleWidgetCount"
            )
            if (
                section_count <= 0
                or visible_section_count > section_count
                or section["visible"] != (visible_section_count > 0)
            ):
                raise EvidenceValidationError(
                    f"Initial Setup section {section_name} counts are inconsistent"
                )
        schedule_rows = _object(counts.get("scheduleRows"), "scheduleRows")
        _nonnegative_int(schedule_rows.get("regular"), "scheduleRows.regular")
        _nonnegative_int(schedule_rows.get("intensive"), "scheduleRows.intensive")

        memory = _object(checkpoint.get("processMemory"), "processMemory")
        if memory.get("scope") != "whole-process":
            raise EvidenceValidationError("process memory checkpoint scope is not whole-process")
        if memory.get("isAvailable") is not True:
            raise EvidenceValidationError("process memory snapshot is unavailable")
        if not isinstance(memory.get("platform"), str) or not memory["platform"]:
            raise EvidenceValidationError("process memory platform is missing")
        _timestamp(memory.get("capturedAtUtc"), "processMemory.capturedAtUtc")
        for key in (
            "workingSetBytes",
            "peakWorkingSetBytes",
            "privateUsageBytes",
            "pagefileUsageBytes",
            "privateWorkingSetBytes",
            "privateDirtyBytes",
            "handleCount",
            "threadCount",
        ):
            _nonnegative_int(memory.get(key), f"processMemory.{key}")
        if memory["workingSetBytes"] == 0 or memory["peakWorkingSetBytes"] == 0:
            raise EvidenceValidationError("process working-set observations must be positive")
        checked[checkpoint["id"]] = state

    entry = checked["entry.initial-setup"]
    if entry["setupPending"] or entry["workspaceSessionActive"] or entry["profileExists"]:
        raise EvidenceValidationError("entry checkpoint is not a fresh Initial Setup state")

    active = checked["setup.active"]
    if not active["setupPending"] or not active["workspaceSessionActive"]:
        raise EvidenceValidationError("active setup checkpoint lacks an open pending workspace")

    failed = checked["save-failure.draft-retained"]
    if not failed["setupPending"] or not failed["classDetailsDraftAvailable"]:
        raise EvidenceValidationError("save failure did not retain pending setup draft state")
    failed_rows = _object(failed.get("databaseRows"), "save failure databaseRows")
    if failed_rows.get("classes") != 0:
        raise EvidenceValidationError("failed class creation left a class row")
    failed_counts = _object(
        checkpoints[2].get("featureCounts"), "save failure featureCounts"
    )
    failed_sections = _object(failed_counts.get("sectionWidgets"), "sectionWidgets")
    if not failed_sections["classTimes"]["visible"]:
        raise EvidenceValidationError("save failure checkpoint did not measure Class Times")
    if _object(failed_counts.get("scheduleRows"), "scheduleRows").get("regular", 0) <= 0:
        raise EvidenceValidationError("save failure checkpoint has no regular schedule rows")

    completed_state = checked["setup.completed"]
    if (
        completed_state["setupPending"]
        or not completed_state["workspaceSessionActive"]
        or not completed_state["profileExists"]
        or completed_state["recoveryJournalExists"]
    ):
        raise EvidenceValidationError("successful completion checkpoint is inconsistent")
    completed_rows = _object(
        completed_state.get("databaseRows"), "completed databaseRows"
    )
    if any(
        completed_rows.get(key) != expected
        for key, expected in (
            ("teachers", 1),
            ("classes", 1),
            ("classInfo", 1),
            ("regularTimes", 1),
        )
    ):
        raise EvidenceValidationError("completed setup rows do not match the synthetic fixture")

    replacement_entry = checked["replacement.entry"]
    if (
        replacement_entry["setupPending"]
        or replacement_entry["workspaceSessionActive"]
        or not replacement_entry["profileExists"]
        or replacement_entry["recoveryJournalExists"]
    ):
        raise EvidenceValidationError("replacement entry checkpoint lacks the original profile")

    replacement = checked["replacement.active"]
    if (
        not replacement["setupPending"]
        or not replacement["workspaceSessionActive"]
        or not replacement["recoveryJournalExists"]
        or not replacement["originalBackupExists"]
    ):
        raise EvidenceValidationError("replacement setup did not preserve its original profile")

    canceled = checked["cancel.restored-original-profile"]
    if (
        canceled["setupPending"]
        or canceled["workspaceSessionActive"]
        or not canceled["profileExists"]
        or canceled["recoveryJournalExists"]
        or canceled.get("originalProfileBytesPreserved") is not True
    ):
        raise EvidenceValidationError("cancel checkpoint did not restore the original profile")

    return {
        "validation": "valid",
        "taskId": TASK_ID,
        "checkpointCount": len(checkpoints),
        "numericMemoryPassThreshold": None,
        "processMemoryScope": "whole-process observations; no page attribution or threshold",
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", required=True, type=Path)
    arguments = parser.parse_args(argv)
    try:
        document = json.loads(arguments.artifact.read_text(encoding="utf-8"))
        result = validate_document(_object(document, "artifact"))
        exit_code = 0
    except (OSError, json.JSONDecodeError, EvidenceValidationError) as error:
        result = {"validation": "invalid", "errors": [str(error)]}
        exit_code = 1
    print(json.dumps(result, separators=(",", ":")))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
