#!/usr/bin/env python3
"""Focused checks for the PDF lifetime probe trace validator."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import run_pdf_lifetime_probe as runner


class PdfLifetimeProbeTimingTests(unittest.TestCase):
    def make_trace(
        self,
        root: Path,
        *,
        elapsed: tuple[int, int, int, int] = (5, 20, 1020, 5020),
        close_relative: tuple[int, int, int] = (0, 1000, 5000),
        duplicate_names: bool = False,
        availability: bool = True,
        omit_availability: bool = False,
    ) -> tuple[Path, dict[str, object]]:
        application_path = root / "ClassMngr.exe"
        application_path.write_bytes(b"test identity only")
        identity: dict[str, object] = {
            "runId": "timing-self-check",
            "application": {"path": str(application_path.resolve())},
            "source": {"gitRevision": "test-revision"},
        }
        sample_names = list(runner.REQUIRED_SAMPLE_NAMES)
        if duplicate_names:
            sample_names[2] = sample_names[1]
        operations = (*close_relative,)
        samples = []
        for index, name in enumerate(sample_names):
            operation = {"closeElapsedMs": operations[index - 1]} if index > 0 else {}
            sample = {
                "name": name,
                "elapsedMs": elapsed[index],
                "operation": operation,
            }
            if not omit_availability:
                sample["available"] = availability
            samples.append(sample)
        trace = {
            "schema": "classmngr-pdf-lifetime-probe-v1",
            "status": "completed",
            "identity": identity,
            "arm": "document-only",
            "runId": identity["runId"],
            "process": {
                "applicationPath": str(application_path.resolve()),
                "gitRevision": "test-revision",
            },
            "memorySamples": samples,
            "pdf": {
                "pageCount": 38,
                "relativePath": runner.PDF_RELATIVE_PATH,
                "resolvedPath": runner.PDF_QRC_PATH,
            },
            "controls": {
                "pngFileWritingEnabled": False,
                "forcedGrabInvoked": False,
                "pdfViewCreated": False,
                "viewerVisible": False,
            },
        }
        trace_path = root / "trace.json"
        trace_path.write_text(json.dumps(trace), encoding="utf-8")
        return trace_path, identity

    def assert_rejected(self, **trace_arguments: object) -> None:
        with tempfile.TemporaryDirectory(prefix="pdf-lifetime-probe-check-") as temporary:
            trace_path, identity = self.make_trace(Path(temporary), **trace_arguments)
            with self.assertRaises(runner.ProbeError):
                runner.validate_trace(trace_path, identity, "document-only")

    def test_intended_one_and_five_second_samples_are_accepted(self) -> None:
        with tempfile.TemporaryDirectory(prefix="pdf-lifetime-probe-check-") as temporary:
            trace_path, identity = self.make_trace(Path(temporary))
            trace = runner.validate_trace(trace_path, identity, "document-only")
            self.assertEqual(trace["status"], "completed")

    def test_early_one_second_sample_is_rejected(self) -> None:
        self.assert_rejected(
            elapsed=(5, 20, 720, 5020),
            close_relative=(0, 700, 5000),
        )

    def test_late_one_second_sample_reproducer_is_rejected(self) -> None:
        # This reproduces the Tester case where the +1s label referred to +6s.
        self.assert_rejected(
            elapsed=(5, 20, 6020, 6030),
            close_relative=(0, 6000, 6010),
        )

    def test_five_second_sample_past_its_window_is_rejected(self) -> None:
        self.assert_rejected(
            elapsed=(5, 20, 1020, 6030),
            close_relative=(0, 1000, 6010),
        )

    def test_reused_timestamp_is_rejected(self) -> None:
        self.assert_rejected(
            elapsed=(5, 20, 20, 5020),
            close_relative=(0, 1000, 5000),
        )

    def test_duplicate_sample_label_is_rejected(self) -> None:
        self.assert_rejected(duplicate_names=True)

    def test_memory_sample_with_false_availability_is_rejected(self) -> None:
        self.assert_rejected(availability=False)

    def test_memory_sample_with_missing_availability_is_rejected(self) -> None:
        self.assert_rejected(omit_availability=True)


if __name__ == "__main__":
    unittest.main()
