from __future__ import annotations

import unittest
from datetime import datetime, timedelta, timezone

from validate_initial_setup_lifecycle_evidence import (
    EXPECTED_CHECKPOINTS,
    EvidenceValidationError,
    validate_document,
)


def timestamp(value: datetime) -> str:
    return value.isoformat().replace("+00:00", "Z")


def valid_document() -> dict:
    start = datetime(2026, 11, 1, 15, 0, tzinfo=timezone.utc)
    ids = EXPECTED_CHECKPOINTS
    states = [
        {
            "setupPending": False,
            "recoveryAvailable": False,
            "workspaceSessionActive": False,
            "profileExists": False,
            "recoveryJournalExists": False,
            "originalBackupExists": False,
            "classDetailsDraftAvailable": False,
            "databaseRows": {},
        },
        {
            "setupPending": True,
            "recoveryAvailable": False,
            "workspaceSessionActive": True,
            "profileExists": True,
            "recoveryJournalExists": True,
            "originalBackupExists": False,
            "classDetailsDraftAvailable": False,
            "databaseRows": {},
        },
        {
            "setupPending": True,
            "recoveryAvailable": False,
            "workspaceSessionActive": True,
            "profileExists": True,
            "recoveryJournalExists": True,
            "originalBackupExists": False,
            "classDetailsDraftAvailable": True,
            "databaseRows": {"classes": 0},
        },
        {
            "setupPending": False,
            "recoveryAvailable": False,
            "workspaceSessionActive": True,
            "profileExists": True,
            "recoveryJournalExists": False,
            "originalBackupExists": False,
            "classDetailsDraftAvailable": False,
            "databaseRows": {
                "teachers": 1,
                "classes": 1,
                "classInfo": 1,
                "regularTimes": 1,
                "intensiveTimes": 0,
            },
        },
        {
            "setupPending": False,
            "recoveryAvailable": False,
            "workspaceSessionActive": False,
            "profileExists": True,
            "recoveryJournalExists": False,
            "originalBackupExists": False,
            "classDetailsDraftAvailable": False,
            "databaseRows": {},
        },
        {
            "setupPending": True,
            "recoveryAvailable": False,
            "workspaceSessionActive": True,
            "profileExists": True,
            "recoveryJournalExists": True,
            "originalBackupExists": True,
            "classDetailsDraftAvailable": False,
            "databaseRows": {},
        },
        {
            "setupPending": False,
            "recoveryAvailable": False,
            "workspaceSessionActive": False,
            "profileExists": True,
            "recoveryJournalExists": False,
            "originalBackupExists": False,
            "classDetailsDraftAvailable": False,
            "databaseRows": {},
            "originalProfileBytesPreserved": True,
        },
    ]
    checkpoints = []
    for index, (checkpoint_id, state) in enumerate(zip(ids, states)):
        sampled = start + timedelta(seconds=index + 1)
        sections = {
            name: {
                "present": True,
                "visible": name == "classTimes" and index == 2,
                "widgetCount": 10,
                "visibleWidgetCount": 2 if name == "classTimes" and index == 2 else 0,
            }
            for name in ("personalDetails", "teacherEntry", "classDetails", "classTimes")
        }
        checkpoints.append(
            {
                "id": checkpoint_id,
                "elapsedMs": (index + 1) * 1000,
                "state": state,
                "featureCounts": {
                    "pageWidgetCount": 100,
                    "visiblePageWidgetCount": 65,
                    "visibleActionCount": 2,
                    "sectionWidgets": sections,
                    "scheduleRows": {"regular": 1, "intensive": 0},
                },
                "processMemory": {
                    "scope": "whole-process",
                    "platform": "windows",
                    "isAvailable": True,
                    "capturedAtUtc": timestamp(sampled),
                    "workingSetBytes": 100_000_000 + index,
                    "peakWorkingSetBytes": 110_000_000 + index,
                    "privateUsageBytes": 90_000_000 + index,
                    "pagefileUsageBytes": 95_000_000 + index,
                    "privateWorkingSetBytes": 80_000_000 + index,
                    "privateDirtyBytes": 0,
                    "handleCount": 200,
                    "threadCount": 20,
                },
            }
        )
    return {
        "schema": "classmngr-phase7-initial-setup-lifecycle-v1",
        "taskId": "P7A-C54-INITIAL-SETUP-LIFECYCLE",
        "status": "completed",
        "feature": "Initial Setup",
        "startedAtUtc": timestamp(start),
        "completedAtUtc": timestamp(start + timedelta(seconds=20)),
        "fixture": {
            "id": "c54-synthetic-initial-setup-v1",
            "syntheticDataOnly": True,
            "screenshotCaptured": False,
            "successfulProfileCount": 1,
            "replacementCancelCount": 1,
        },
        "measurementScope": {
            "featureCounts": "Initial Setup UI and persisted-row counts",
            "processMemory": (
                "Whole-process ProcessMemorySnapshot observations; "
                "not page-attributed and not a per-page memory ceiling"
            ),
        },
        "numericMemoryPassThreshold": None,
        "checkpointCount": len(checkpoints),
        "checkpoints": checkpoints,
    }


class InitialSetupLifecycleEvidenceValidatorTests(unittest.TestCase):
    def test_valid_lifecycle_has_no_numeric_memory_threshold(self) -> None:
        summary = validate_document(valid_document())

        self.assertEqual(summary["validation"], "valid")
        self.assertEqual(summary["checkpointCount"], 7)
        self.assertIsNone(summary["numericMemoryPassThreshold"])

    def test_numeric_memory_threshold_is_rejected(self) -> None:
        document = valid_document()
        document["numericMemoryPassThreshold"] = 262_144_000

        with self.assertRaisesRegex(EvidenceValidationError, "threshold must be null"):
            validate_document(document)

    def test_unavailable_process_memory_is_rejected(self) -> None:
        document = valid_document()
        document["checkpoints"][0]["processMemory"]["isAvailable"] = False

        with self.assertRaisesRegex(EvidenceValidationError, "snapshot is unavailable"):
            validate_document(document)

    def test_cancel_must_restore_original_profile_bytes(self) -> None:
        document = valid_document()
        document["checkpoints"][-1]["state"]["originalProfileBytesPreserved"] = False

        with self.assertRaisesRegex(EvidenceValidationError, "did not restore"):
            validate_document(document)

    def test_section_feature_counts_are_required(self) -> None:
        document = valid_document()
        document["checkpoints"][0]["featureCounts"]["sectionWidgets"].pop(
            "teacherEntry"
        )

        with self.assertRaisesRegex(EvidenceValidationError, "section counts are incomplete"):
            validate_document(document)

    def test_process_memory_scope_must_disclaim_page_ceiling(self) -> None:
        document = valid_document()
        document["measurementScope"]["processMemory"] = (
            "Whole-process observations; not page-attributed"
        )

        with self.assertRaisesRegex(EvidenceValidationError, "disclaim a per-page ceiling"):
            validate_document(document)

    def test_missing_checkpoint_is_rejected(self) -> None:
        document = valid_document()
        document["checkpoints"].pop(2)
        document["checkpointCount"] -= 1

        with self.assertRaisesRegex(EvidenceValidationError, "exactly seven"):
            validate_document(document)


if __name__ == "__main__":
    unittest.main()
