#!/usr/bin/env python3
"""Tests for the bounded macOS baseline retry decision."""

from __future__ import annotations

import unittest

from retry_macos_baseline_if_missing_report import (
    MACOS_JOB_NAME,
    MACOS_REPORT_NAME,
    failed_macos_job_without_report,
)


class FailedMacOsJobWithoutReportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.macos_job = {
            "id": 123,
            "name": MACOS_JOB_NAME,
            "conclusion": "failure",
        }

    def test_selects_failed_macos_job_when_no_report_was_published(self) -> None:
        self.assertEqual(
            failed_macos_job_without_report([self.macos_job], []), self.macos_job
        )

    def test_does_not_retry_when_report_artifact_exists(self) -> None:
        artifacts = [{"name": MACOS_REPORT_NAME, "expired": False}]

        self.assertIsNone(
            failed_macos_job_without_report([self.macos_job], artifacts)
        )

    def test_expired_report_does_not_count_as_published_evidence(self) -> None:
        artifacts = [{"name": MACOS_REPORT_NAME, "expired": True}]

        self.assertEqual(
            failed_macos_job_without_report([self.macos_job], artifacts), self.macos_job
        )

    def test_does_not_retry_a_successful_macos_job(self) -> None:
        successful_job = {**self.macos_job, "conclusion": "success"}

        self.assertIsNone(failed_macos_job_without_report([successful_job], []))

    def test_does_not_retry_an_unrelated_failed_matrix_job(self) -> None:
        linux_job = {**self.macos_job, "name": "Linux x64 Debug"}

        self.assertIsNone(failed_macos_job_without_report([linux_job], []))


if __name__ == "__main__":
    unittest.main()
