from __future__ import annotations

import unittest
from datetime import datetime, timedelta, timezone

from validate_next_memory_evidence import (
    EXPECTED_SCENARIOS,
    EvidenceValidationError,
    validate_documents,
)


def timestamp(value: datetime) -> str:
    return value.isoformat().replace("+00:00", "Z")


def valid_documents() -> tuple[dict, list[dict]]:
    start = datetime(2026, 10, 28, 15, 0, tzinfo=timezone.utc)
    executable = r"C:\runner-temp\phase7\bin\ClassMngrNext.exe"
    image_sha = "a" * 64
    manifest = {
        "schema": "classmngr-phase7-next-memory-v1",
        "taskId": "P7A-NEXT-MEMORY-IMPLEMENT",
        "status": "completed",
        "revision": "b" * 40,
        "startedAtUtc": timestamp(start),
        "completedAtUtc": timestamp(start + timedelta(seconds=100)),
        "configuration": {
            "preset": "windows-x64-release",
            "buildType": "Release",
            "architecture": "x64",
        },
        "os": {"platform": "Windows", "architecture": "x64"},
        "application": {
            "canonicalPath": executable,
            "sha256": image_sha,
            "architecture": "x64",
        },
        "scenarios": [],
    }
    fixture_specs = (
        ("fresh-no-profile", "c", False, None),
        ("schema-only-v6-empty", "d", True, 0),
        ("synthetic-large-startup-96-classes", "e", True, 96),
    )
    metrics: list[dict] = []
    for scenario_index, scenario_id in enumerate(EXPECTED_SCENARIOS):
        scenario_start = start + timedelta(seconds=1 + scenario_index * 30)
        created = scenario_start + timedelta(milliseconds=10)
        ready = created + timedelta(seconds=5)
        first_sample = ready + timedelta(seconds=10)
        sample_times = [first_sample + timedelta(seconds=index) for index in range(5)]
        exit_requested = sample_times[-1] + timedelta(milliseconds=50)
        exited = exit_requested + timedelta(milliseconds=100)
        fixture_id, sha_character, argument_supplied, expected_class_count = fixture_specs[
            scenario_index
        ]
        fixture_sha = sha_character * 64
        manifest["scenarios"].append(
            {
                "id": scenario_id,
                "outcome": "completed",
                "startedAtUtc": timestamp(scenario_start),
                "readiness": {
                    "readyAtUtc": timestamp(ready),
                    "windowVisible": True,
                    "stablePolls": 3,
                    "mainWindowHandle": 1234 + scenario_index,
                    "mainWindowTitle": "ClassMngr Next",
                },
                "fixture": {
                    "id": fixture_id,
                    "sha256": fixture_sha,
                    "profilePathArgumentSupplied": argument_supplied,
                    "classCount": expected_class_count,
                },
                "process": {
                    "pid": 4000 + scenario_index,
                    "createdAtUtc": timestamp(created),
                    "canonicalExecutablePath": executable,
                    "architecture": "x64",
                    "sha256": image_sha,
                    "mainWindowHandle": 1234 + scenario_index,
                    "mainWindowTitle": "ClassMngr Next",
                },
                "settleSeconds": 10,
                "closeRequestedAtUtc": timestamp(exit_requested),
                "exitedAtUtc": timestamp(exited),
                "normalExit": True,
                "exitCode": 0,
            }
        )
        for sample_index, sampled_at in enumerate(sample_times, start=1):
            metrics.append(
                {
                    "scenarioId": scenario_id,
                    "sampleIndex": sample_index,
                    "timestampUtc": timestamp(sampled_at),
                    "pid": 4000 + scenario_index,
                    "processCreatedAtUtc": timestamp(created),
                    "canonicalExecutablePath": executable,
                    "architecture": "x64",
                    "sha256": image_sha,
                    "currentWorkingSetBytes": 50_000_000 + sample_index,
                    "privateBytes": 80_000_000 + sample_index,
                    "peakWorkingSetBytes": 60_000_000 + sample_index,
                    "handles": 100 + sample_index,
                    "threads": 12 + sample_index,
                }
            )
    return manifest, metrics


class NextMemoryEvidenceValidatorTests(unittest.TestCase):
    def test_valid_run_summarizes_observations_without_memory_threshold(self) -> None:
        manifest, metrics = valid_documents()

        summary = validate_documents(manifest, metrics)

        self.assertEqual(summary["validation"], "valid")
        self.assertIsNone(summary["numericMemoryPassThreshold"])
        self.assertEqual(summary["scenarioCount"], 3)
        self.assertEqual(summary["sampleCount"], 15)
        self.assertEqual(
            summary["scenarios"][0]["currentWorkingSetBytes"]["median"],
            50_000_003,
        )

    def test_application_filename_comparison_is_case_insensitive(self) -> None:
        manifest, metrics = valid_documents()
        lowercase_path = manifest["application"]["canonicalPath"].replace(
            "ClassMngrNext.exe", "classmngrnext.exe"
        )
        manifest["application"]["canonicalPath"] = lowercase_path
        for scenario in manifest["scenarios"]:
            scenario["process"]["canonicalExecutablePath"] = lowercase_path
        for sample in metrics:
            sample["canonicalExecutablePath"] = lowercase_path

        summary = validate_documents(manifest, metrics)

        self.assertEqual(summary["validation"], "valid")

    def test_invalid_process_image_identity_is_rejected(self) -> None:
        manifest, metrics = valid_documents()
        metrics[0]["sha256"] = "f" * 64

        with self.assertRaisesRegex(EvidenceValidationError, "image hash differs"):
            validate_documents(manifest, metrics)

    def test_missing_or_out_of_order_samples_are_rejected(self) -> None:
        manifest, metrics = valid_documents()
        metrics[1]["sampleIndex"] = 4

        with self.assertRaisesRegex(EvidenceValidationError, "sample indices"):
            validate_documents(manifest, metrics)

    def test_abnormal_process_exit_is_rejected(self) -> None:
        manifest, metrics = valid_documents()
        manifest["scenarios"][1]["exitCode"] = 1

        with self.assertRaisesRegex(EvidenceValidationError, "did not exit normally"):
            validate_documents(manifest, metrics)

    def test_process_created_after_visible_window_readiness_is_rejected(self) -> None:
        manifest, metrics = valid_documents()
        scenario = manifest["scenarios"][0]
        ready_at = datetime.fromisoformat(
            scenario["readiness"]["readyAtUtc"].replace("Z", "+00:00")
        )
        created_after_ready = timestamp(
            ready_at + timedelta(seconds=1)
        )
        scenario["process"]["createdAtUtc"] = created_after_ready
        for sample in metrics:
            if sample["scenarioId"] == scenario["id"]:
                sample["processCreatedAtUtc"] = created_after_ready

        with self.assertRaisesRegex(
            EvidenceValidationError, "created after visible-window readiness"
        ):
            validate_documents(manifest, metrics)


if __name__ == "__main__":
    unittest.main()
