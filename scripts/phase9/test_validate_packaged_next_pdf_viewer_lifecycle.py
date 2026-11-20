from __future__ import annotations

import unittest
import struct
import tempfile
import hashlib
from pathlib import Path

from validate_packaged_next_pdf_viewer_lifecycle import (
    ValidationFailure,
    pdf_state,
    validate_pdf_state,
    validate_qpdf_sequence,
    _validate_screenshots,
    validate_uia_action_binding,
    validate_working_set_metrics,
)


def ready_state(generation: int = 1) -> dict:
    return {
        "status": 2,
        "error": 0,
        "pageCount": 38,
        "pageModelRowCount": 38,
        "pageIndex": 0,
        "pageWidthPoints": 612.0,
        "pageHeightPoints": 792.0,
        "activeStream": True,
        "documentPresent": True,
        "viewPresent": True,
        "borrowCount": 0,
        "generation": generation,
    }


def wrapper_state() -> dict:
    return {
        "status": 0,
        "error": 0,
        "pageCount": 0,
        "pageModelRowCount": 0,
        "pageIndex": 0,
        "pageWidthPoints": None,
        "pageHeightPoints": None,
        "activeStream": False,
        "documentPresent": True,
        "viewPresent": True,
        "borrowCount": 0,
        "generation": 2,
    }


def _record(event: str, details: dict, *, phase: str = "after", instance_id: int = 7) -> dict:
    defaults = {
        "phase": phase,
        "scope_kind": "viewer_session",
        "status": 0,
        "error": 0,
        "page_count": 0,
        "active_stream": False,
        "document_present": True,
        "view_present": True,
        "borrow_count": 0,
        "generation": 1,
        "page_selection_count": 0,
        "page_index": 0,
        "page_width_points": 612.0,
        "page_height_points": 792.0,
        "retired_document_present": False,
    }
    return {"owner": "document_viewer", "event": event, "instance_id": instance_id, "details": {**defaults, **details}}


def valid_qpdf_sequence() -> tuple[list[dict], dict, dict]:
    records = [_record("qpdf-session-create", {"page_count": 0, "document_present": True, "view_present": True})]
    for _ in range(3):
        records.extend(
            [
                _record("qpdf-document-load", {"active_stream": True, "status": 2, "page_count": 38}),
                _record("qpdf-open", {"active_stream": True}),
                _record("primary-stream-open", {"active_stream": True}),
                _record("qpdf-document-close", {"active_stream": True}, phase="after"),
                _record("primary-stream-release", {"active_stream": False}),
            ]
        )
    records.extend(
        [
            _record("qpdf-document-destroy", {"retired_document_present": False}),
            _record("qpdf-view-destroy", {"view_present": False}),
            _record("qpdf-session-release", {"status": 0, "page_count": 0, "active_stream": False, "document_present": False, "view_present": False}),
        ]
    )
    states = {
        "firstOpen": ready_state(1),
        "closed": wrapper_state(),
        "reopen": ready_state(2),
        "suspended": wrapper_state(),
        "postSuspendReopen": ready_state(3),
    }
    checkpoints = {name: {"pdfState": state, "documentId": "document_guides_lesson_planning"} for name, state in states.items()}
    page_host = {
        "catalogReady": {"viewerPageInstanceCount": 0},
        "firstOpen": {"viewerPageInstanceCount": 1, "viewerPageVisibleCount": 1, "viewerPageOffscreenCount": 0},
        "closed": {"viewerPageInstanceCount": 1, "viewerPageVisibleCount": 1, "viewerPageOffscreenCount": 0},
        "reopen": {"viewerPageInstanceCount": 1, "viewerPageVisibleCount": 1, "viewerPageOffscreenCount": 0},
        "suspended": {"viewerPageInstanceCount": 1, "viewerPageVisibleCount": 0, "viewerPageOffscreenCount": 1},
        "postSuspendReopen": {"viewerPageInstanceCount": 1, "viewerPageVisibleCount": 1, "viewerPageOffscreenCount": 0},
    }
    return records, checkpoints, page_host


class PdfViewerLifecycleValidatorTests(unittest.TestCase):
    def test_ready_document_requires_stream_and_page_model(self) -> None:
        validate_pdf_state(ready_state(), "ready")
        state = ready_state()
        state["activeStream"] = False
        with self.assertRaisesRegex(ValidationFailure, "stream"):
            validate_pdf_state(state, "ready")
        state = ready_state()
        state["pageModelRowCount"] = 37
        with self.assertRaisesRegex(ValidationFailure, "page model"):
            validate_pdf_state(state, "ready")

    def test_close_state_keeps_empty_qpdf_wrappers_but_releases_content(self) -> None:
        validate_pdf_state(wrapper_state(), "empty-wrapper")
        state = wrapper_state()
        state["activeStream"] = True
        with self.assertRaisesRegex(ValidationFailure, "stream"):
            validate_pdf_state(state, "empty-wrapper")
        state = wrapper_state()
        state["documentPresent"] = False
        with self.assertRaisesRegex(ValidationFailure, "empty QPdfDocument/QPdfView wrappers"):
            validate_pdf_state(state, "empty-wrapper")

    def test_released_qpdf_record_may_omit_page_dimensions(self) -> None:
        details = {
            "status": 0,
            "error": 0,
            "page_count": 0,
            "active_stream": False,
            "document_present": True,
            "view_present": True,
            "borrow_count": 0,
            "generation": 2,
        }
        state = pdf_state(details)
        self.assertIsNone(state["pageWidthPoints"])
        self.assertIsNone(state["pageHeightPoints"])
        validate_pdf_state(state, "empty-wrapper")

        ready = ready_state()
        ready["pageWidthPoints"] = None
        with self.assertRaisesRegex(ValidationFailure, "page dimensions"):
            validate_pdf_state(ready, "ready")

    def test_full_open_close_suspend_reopen_sequence_passes(self) -> None:
        records, checkpoints, page_host = valid_qpdf_sequence()
        counts = validate_qpdf_sequence(records, checkpoints, page_host)
        self.assertEqual(counts["documentLoads"], 3)
        self.assertEqual(counts["streamReleases"], 3)

    def test_missing_stream_release_fails_closed(self) -> None:
        records, checkpoints, page_host = valid_qpdf_sequence()
        removed = False
        filtered = []
        release_index = 0
        for row in records:
            if row["event"] == "primary-stream-release":
                release_index += 1
                if release_index == 2 and not removed:
                    removed = True
                    continue
            filtered.append(row)
        records = filtered
        with self.assertRaisesRegex(ValidationFailure, "release"):
            validate_qpdf_sequence(records, checkpoints, page_host)

    def test_strict_working_set_equality_fails(self) -> None:
        points = ("startup-ready", "catalog-ready", "first-open", "closed", "reopen", "suspended", "post-suspend-reopen", "pre-shutdown")
        metrics = [{"point": point, "pid": 1234, "currentWorkingSetBytes": 100, "peakWorkingSetBytes": 200} for point in points]
        peak, count = validate_working_set_metrics(metrics, 1234)
        self.assertEqual((peak, count), (200, 8))
        metrics[4]["currentWorkingSetBytes"] = 262_144_000
        with self.assertRaisesRegex(ValidationFailure, "strictly below"):
            validate_working_set_metrics(metrics, 1234)

    def test_memory_sample_must_bind_to_measured_pid(self) -> None:
        points = ("startup-ready", "catalog-ready", "first-open", "closed", "reopen", "suspended", "post-suspend-reopen", "pre-shutdown")
        metrics = [{"point": point, "pid": 1234, "currentWorkingSetBytes": 100, "peakWorkingSetBytes": 200} for point in points]
        metrics[-1]["pid"] = 5678
        with self.assertRaisesRegex(ValidationFailure, "measured process"):
            validate_working_set_metrics(metrics, 1234)

    def test_uia_targeted_tree_click_is_bound_to_pid_hwnd_and_element(self) -> None:
        action = {
            "processId": 1234,
            "windowProcessId": 1234,
            "windowHandle": 5678,
            "method": "UIA-targeted window click",
            "controlType": "ControlType.TreeItem",
            "automationId": "navigationTree",
            "target": {"processId": 1234, "automationId": "navigationTree", "rectangle": {"x": 100, "y": 200, "width": 50, "height": 30}},
            "pointerAction": {"kind": "window-targeted WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP at same verified UIA target", "screenX": 125, "screenY": 215, "targetProcessId": 1234, "windowHandle": 5678},
        }
        validate_uia_action_binding(action, 1234, 5678, "open")
        action["pointerAction"]["targetProcessId"] = 9999
        with self.assertRaisesRegex(ValidationFailure, "pointer action"):
            validate_uia_action_binding(action, 1234, 5678, "open")

    def test_rendered_states_require_content_but_closed_and_suspended_do_not(self) -> None:
        kinds = ("firstOpen", "closed", "reopen", "suspended", "postSuspendReopen")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            screenshots = []
            for name in kinds:
                data = b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", 10, 10) + b"\x08\x02\x00\x00\x00"
                path = root / f"{name}.png"
                path.write_bytes(data)
                screenshots.append({
                    "checkpoint": name,
                    "relativePath": path.name,
                    "sha256": hashlib.sha256(data).hexdigest(),
                    "width": 10,
                    "height": 10,
                    "processId": 1234,
                    "windowHandle": 5678,
                    "windowProcessId": 1234,
                    "pdfViewportRectangle": {"width": 4, "height": 4},
                    "pdfViewportStats": {"uniqueSampledColors": 8, "nonWhiteSampledPixels": 1000} if name in {"firstOpen", "reopen", "postSuspendReopen"} else {"uniqueSampledColors": 0, "nonWhiteSampledPixels": 0},
                })
            _validate_screenshots(root, screenshots, 1234, 5678)
            screenshots[0]["pdfViewportStats"]["nonWhiteSampledPixels"] = 0
            with self.assertRaisesRegex(ValidationFailure, "lacks visible rendered page content"):
                _validate_screenshots(root, screenshots, 1234, 5678)
            screenshots[0]["processId"] = 9999
            with self.assertRaisesRegex(ValidationFailure, "not bound to the measured process"):
                _validate_screenshots(root, screenshots, 1234, 5678)


if __name__ == "__main__":
    unittest.main()
