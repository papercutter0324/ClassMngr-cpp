#!/usr/bin/env python3
"""Focused checks for the PDF lifetime probe trace validator."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from typing import Callable

import run_pdf_lifetime_probe as runner


class PdfLifetimeProbeTimingTests(unittest.TestCase):
    def make_trace(
        self,
        root: Path,
        *,
        arm: str = "document-only",
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
        if arm == "viewer-grab":
            samples[1:1] = [
                {
                    "name": "pdf-lifetime-grab-start",
                    "elapsedMs": 10,
                    "available": availability,
                    "operation": {},
                },
                {
                    "name": "pdf-lifetime-grab-complete",
                    "elapsedMs": 15,
                    "available": availability,
                    "operation": {},
                },
            ]
        is_viewer_arm = arm != "document-only"
        trace = {
            "schema": "classmngr-pdf-lifetime-probe-v1",
            "status": "completed",
            "identity": identity,
            "arm": arm,
            "runId": identity["runId"],
            "process": {
                "applicationPath": str(application_path.resolve()),
                "gitRevision": "test-revision",
            },
            "measurementLabels": list(runner.required_measurement_labels(arm)),
            "memorySamples": samples,
            "pdf": {
                "pageCount": 38,
                "relativePath": runner.PDF_RELATIVE_PATH,
                "resolvedPath": runner.PDF_QRC_PATH,
            },
            "controls": {
                "pngFileWritingEnabled": False,
                "forcedGrabInvoked": arm == "viewer-grab",
                "pdfViewCreated": is_viewer_arm,
                "viewerVisible": is_viewer_arm,
                "pageMode": "MultiPage" if is_viewer_arm else "",
                "viewWidth": 1200 if is_viewer_arm else 0,
                "viewHeight": 800 if is_viewer_arm else 0,
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


class PageManagerPdfLifetimeProbeTests(unittest.TestCase):
    def make_page_manager_trace(
        self,
        root: Path,
        arm: str,
        *,
        alter: Callable[[dict[str, object]], None] | None = None,
        process_age_offset_ms: int = 0,
    ) -> tuple[Path, dict[str, object], dict[str, object]]:
        application_path = root / "ClassMngr.exe"
        application_path.write_bytes(b"test identity only")
        repetition = "01"
        run_id = f"page-manager-{arm}-r{repetition}"
        identity: dict[str, object] = {
            "runId": run_id,
            "application": {"path": str(application_path.resolve())},
            "source": {"gitRevision": "test-revision"},
        }
        identities = {
            "pageIdentity": "0x101",
            "viewIdentity": "0x202",
            "documentIdentity": "0x303",
        }

        def state(*, loaded: bool) -> dict[str, object]:
            return {
                "documentCreated": True,
                "documentStatus": "Ready" if loaded else "Null",
                "pageCount": 38 if loaded else 0,
                "pdfViewCreated": True,
                "viewerVisible": loaded,
                "pageMode": "MultiPage",
                "viewWidth": 1200,
                "viewHeight": 800,
                "forcedGrabInvoked": False,
                "pngFileWritingEnabled": False,
                "currentPage": "pdf-viewer" if loaded else "my-workspace",
                "documentsPackMounted": loaded,
                **identities,
            }

        sample_rows: list[tuple[str, int, dict[str, object], dict[str, object]]] = []
        sample_rows.append((
            "pdf-lifetime-loaded-ready",
            2500,
            state(loaded=True),
            {"event": "initial-load-ready", "probeElapsedMs": 100},
        ))
        sample_rows.extend((
            ("pdf-lifetime-after-close", 2520, state(loaded=False), {"event": "first-page-leave-close", "closeElapsedMs": 0}),
            ("pdf-lifetime-after-close-1s", 3520, state(loaded=False), {"event": "close-plus-one-second", "closeElapsedMs": 1000}),
            ("pdf-lifetime-after-close-5s", 7520, state(loaded=False), {"event": "close-plus-five-seconds", "closeElapsedMs": 5000}),
        ))

        if arm == "page-manager-cycle":
            sample_rows.append((
                "pdf-lifetime-cycle-end",
                13000 + process_age_offset_ms,
                state(loaded=False),
                {"event": "matched-process-age-control-end", "probeElapsedMs": 10500, "matchedProcessAgeTargetMs": 10500},
            ))
            event_specs = (
                ("initial-load-ready", "pdf-lifetime-loaded-ready", 100, True),
                ("first-page-leave-close", "pdf-lifetime-after-close", 120, False),
                ("cycle-control-end", "pdf-lifetime-cycle-end", 10500, False),
            )
        else:
            sample_rows.extend((
                ("pdf-lifetime-reopened-ready", 7700, state(loaded=True), {"event": "reopened-load-ready", "firstCloseElapsedMs": 5000}),
                ("pdf-lifetime-after-reopen-close", 7720, state(loaded=False), {"event": "second-page-leave-close", "closeElapsedMs": 0}),
                ("pdf-lifetime-after-reopen-close-1s", 8720, state(loaded=False), {"event": "close-plus-one-second", "closeElapsedMs": 1000}),
                ("pdf-lifetime-after-reopen-close-5s", 13000 + process_age_offset_ms, state(loaded=False), {"event": "close-plus-five-seconds", "closeElapsedMs": 5000}),
            ))
            event_specs = (
                ("initial-load-ready", "pdf-lifetime-loaded-ready", 100, True),
                ("first-page-leave-close", "pdf-lifetime-after-close", 120, False),
                ("reopened-load-ready", "pdf-lifetime-reopened-ready", 5300, True),
                ("second-page-leave-close", "pdf-lifetime-after-reopen-close", 5320, False),
            )

        samples = [
            {
                "name": name,
                "elapsedMs": elapsed,
                "available": True,
                "viewerState": viewer_state,
                "operation": operation,
            }
            for name, elapsed, viewer_state, operation in sample_rows
        ]
        lifecycle = {
            "enabled": True,
            "navigationRoute": "NavigationController.handleNavigation",
            "documentId": "document_guides_lesson_planning",
            "matchedProcessAgeTargetMs": 10500,
            **identities,
            "events": [
                {
                    "name": event_name,
                    "memorySampleName": sample_name,
                    "probeElapsedMs": event_elapsed,
                    **state(loaded=loaded),
                }
                for event_name, sample_name, event_elapsed, loaded in event_specs
            ],
        }
        trace: dict[str, object] = {
            "schema": "classmngr-pdf-lifetime-probe-v1",
            "status": "completed",
            "identity": identity,
            "arm": arm,
            "runId": run_id,
            "process": {
                "applicationPath": str(application_path.resolve()),
                "gitRevision": "test-revision",
            },
            "measurementLabels": list(runner.required_measurement_labels(arm)),
            "memorySamples": samples,
            "pdf": {
                "pageCount": 38,
                "relativePath": runner.PDF_RELATIVE_PATH,
                "resolvedPath": runner.PDF_QRC_PATH,
            },
            "controls": {
                "pngFileWritingEnabled": False,
                "forcedGrabInvoked": False,
                "pdfViewCreated": True,
                "viewerVisible": True,
                "pageMode": "MultiPage",
                "viewWidth": 1200,
                "viewHeight": 800,
            },
            "pageManagerLifecycle": lifecycle,
        }
        if alter is not None:
            alter(trace)
        trace_path = root / "trace.json"
        trace_path.write_text(json.dumps(trace), encoding="utf-8")
        return trace_path, identity, trace

    def test_page_manager_cycle_schema_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory(prefix="page-manager-probe-check-") as temporary:
            path, identity, _ = self.make_page_manager_trace(
                Path(temporary), "page-manager-cycle"
            )
            trace = runner.validate_trace(path, identity, "page-manager-cycle")
            self.assertEqual(trace["pageManagerLifecycle"]["enabled"], True)

    def test_page_manager_reopen_schema_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory(prefix="page-manager-probe-check-") as temporary:
            path, identity, _ = self.make_page_manager_trace(
                Path(temporary), "page-manager-reopen"
            )
            trace = runner.validate_trace(path, identity, "page-manager-reopen")
            self.assertEqual(
                trace["pageManagerLifecycle"]["events"][2]["name"],
                "reopened-load-ready",
            )

    def test_measurement_labels_match_actual_samples_for_every_arm(self) -> None:
        for arm in runner.ARMS:
            with self.subTest(arm=arm):
                with tempfile.TemporaryDirectory(prefix="pdf-lifetime-label-check-") as temporary:
                    root = Path(temporary)
                    if arm in ("page-manager-cycle", "page-manager-reopen"):
                        path, identity, trace = self.make_page_manager_trace(root, arm)
                    else:
                        path, identity = PdfLifetimeProbeTimingTests().make_trace(
                            root, arm=arm
                        )
                        trace = json.loads(path.read_text(encoding="utf-8"))

                    labels = trace["measurementLabels"]
                    sample_names = [
                        sample["name"] for sample in trace["memorySamples"]
                    ]
                    label_set = set(labels)
                    actual_required_sample_order = [
                        name for name in sample_names if name in label_set
                    ]
                    self.assertEqual(actual_required_sample_order, labels)
                    if arm in runner.DEFAULT_ARMS:
                        self.assertEqual(labels, list(runner.REQUIRED_SAMPLE_NAMES))
                    else:
                        self.assertEqual(
                            labels,
                            list(runner.required_measurement_labels(arm)),
                        )
                    validated = runner.validate_trace(path, identity, arm)
                    self.assertEqual(validated["measurementLabels"], labels)

    def assert_lifecycle_rejected(
        self,
        arm: str,
        alter: Callable[[dict[str, object]], None],
    ) -> None:
        with tempfile.TemporaryDirectory(prefix="page-manager-probe-check-") as temporary:
            path, identity, _ = self.make_page_manager_trace(
                Path(temporary), arm, alter=alter
            )
            with self.assertRaises(runner.ProbeError):
                runner.validate_trace(path, identity, arm)

    def test_reopen_identity_change_is_rejected(self) -> None:
        def change_identity(trace: dict[str, object]) -> None:
            lifecycle = trace["pageManagerLifecycle"]
            lifecycle["events"][2]["documentIdentity"] = "0x404"

        self.assert_lifecycle_rejected("page-manager-reopen", change_identity)

    def test_reopen_load_must_be_ready_and_mounted(self) -> None:
        def clear_mount(trace: dict[str, object]) -> None:
            trace["memorySamples"][4]["viewerState"]["documentsPackMounted"] = False

        self.assert_lifecycle_rejected("page-manager-reopen", clear_mount)

    def test_close_must_be_null_and_unmounted(self) -> None:
        def leave_mounted(trace: dict[str, object]) -> None:
            trace["memorySamples"][1]["viewerState"]["documentsPackMounted"] = True

        self.assert_lifecycle_rejected("page-manager-cycle", leave_mounted)

    def test_lifecycle_sample_reordering_is_rejected(self) -> None:
        def reorder(trace: dict[str, object]) -> None:
            samples = trace["memorySamples"]
            samples[2], samples[3] = samples[3], samples[2]

        self.assert_lifecycle_rejected("page-manager-cycle", reorder)

    def test_forced_grab_is_rejected(self) -> None:
        def force_grab(trace: dict[str, object]) -> None:
            trace["memorySamples"][0]["viewerState"]["forcedGrabInvoked"] = True

        self.assert_lifecycle_rejected("page-manager-cycle", force_grab)

    def test_matched_process_age_is_checked_when_both_arms_run(self) -> None:
        with tempfile.TemporaryDirectory(prefix="page-manager-probe-check-") as temporary:
            root = Path(temporary)
            cycle_root = root / "cycle"
            reopen_root = root / "reopen"
            cycle_root.mkdir()
            reopen_root.mkdir()
            cycle_path, cycle_identity, cycle = self.make_page_manager_trace(
                cycle_root, "page-manager-cycle"
            )
            reopen_path, reopen_identity, reopen = self.make_page_manager_trace(
                reopen_root, "page-manager-reopen", process_age_offset_ms=2000
            )
            runner.validate_trace(cycle_path, cycle_identity, "page-manager-cycle")
            runner.validate_trace(reopen_path, reopen_identity, "page-manager-reopen")
            with self.assertRaises(runner.ProbeError):
                runner.validate_matched_page_manager_process_age([cycle, reopen])


if __name__ == "__main__":
    unittest.main()
