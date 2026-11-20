from __future__ import annotations

import importlib.util
import hashlib
import json
import struct
import tempfile
import unittest
import zipfile
import zlib
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("validate_packaged_next_speaking_evaluation_lifecycle.py")
SPEC = importlib.util.spec_from_file_location("p913_validator", MODULE_PATH)
assert SPEC and SPEC.loader
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)


def make_pdf(content_stream: bytes) -> bytes:
    compressed = zlib.compress(content_stream)
    chunks = [
        b"%PDF-1.4\n",
        b"1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n",
        b"2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n",
        b"3 0 obj\n<< /Type /Page /Parent 2 0 R /Contents 4 0 R >>\nendobj\n",
        b"4 0 obj\n<< /Length " + str(len(compressed)).encode("ascii") + b" /Filter /FlateDecode >>\nstream\n",
        compressed,
        b"\nendstream\nendobj\n",
        b"5 0 obj\n<< /Producer (unit test) >>\nendobj\n",
        b"trailer\n<< /Root 1 0 R >>\nstartxref\n0\n%%EOF\n",
    ]
    return b"".join(chunks)


def make_pdf_with_indirect_stream_length(content_stream: bytes) -> bytes:
    compressed = zlib.compress(content_stream)
    chunks = [
        b"%PDF-1.4\n",
        b"1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n",
        b"2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n",
        b"3 0 obj\n<< /Type /Page /Parent 2 0 R /Contents 4 0 R >>\nendobj\n",
        b"4 0 obj\n<< /Length 5 0 R /Filter /FlateDecode >>\nstream\n",
        compressed,
        b"\nendstream\nendobj\n",
        b"5 0 obj\n" + str(len(compressed)).encode("ascii") + b"\nendobj\n",
        b"trailer\n<< /Root 1 0 R >>\nstartxref\n0\n%%EOF\n",
    ]
    return b"".join(chunks)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


class ValidatorUnitTests(unittest.TestCase):
    @staticmethod
    def window_targeted_navigation_action(
        target_name: str = "Speaking Evaluation", target_page_id: str = "speaking_evaluation"
    ) -> dict[str, object]:
        rectangle = {"x": 150.0, "y": 250.0, "width": 100.0, "height": 50.0}
        target = {
            "name": target_name,
            "automationId": "QApplication.classMngrNextApplicationShell.classMngrNextNavigationTree",
            "controlType": "ControlType.TreeItem",
            "processId": 99,
            "isEnabled": True,
            "isOffscreen": False,
            "rectangle": rectangle,
        }
        from_page = "my_workspace" if target_page_id == "speaking_evaluation" else "speaking_evaluation"
        event = {
            "observed": True,
            "targetPageId": target_page_id,
            "arrivalIndex": 10,
            "processId": 99,
            "windowHandle": 1234,
            "windowProcessId": 99,
            "line": f"ClassMngrNext navigation event=page-route outcome=completed cause=selection from={from_page} to={target_page_id} catalog_document_count=30",
        }
        return {
            "actionKind": "navigate-to-speaking-evaluation" if target_page_id == "speaking_evaluation" else "navigate-away-after-report-close",
            "expectedName": target_name,
            "observedName": target_name,
            "method": "UIA-targeted window click",
            "processId": 99,
            "windowHandle": 1234,
            "windowProcessId": 99,
            "targetPageId": target_page_id,
            "target": target,
            "pointerAction": {
                "kind": "window-targeted WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP at center of the same verified UIA target",
                "actionType": "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP",
                "screenX": 200,
                "screenY": 275,
                "sourceTarget": {key: target[key] for key in ("name", "automationId", "controlType", "rectangle")},
                "targetProcessId": 99,
                "windowHandle": 1234,
                "windowProcessId": 99,
                "measuredMainWindowBounds": {"x": 100, "y": 200, "width": 300, "height": 400},
            },
            "logArrivalIndexBeforeAction": 9,
            "verifiedByFollowingPageRoute": True,
            "pageRouteEvent": event,
        }

    def test_inventory_fingerprint_is_order_independent_and_binds_bytes(self) -> None:
        self.assertEqual(VALIDATOR.inventory_fingerprint([]), VALIDATOR.sha256_bytes(b"\n"))
        first = [
            {"path": "b.py", "sizeBytes": 2, "sha256": "B"},
            {"path": "a.py", "sizeBytes": 1, "sha256": "A"},
        ]
        self.assertEqual(
            VALIDATOR.inventory_fingerprint(first),
            VALIDATOR.inventory_fingerprint(list(reversed(first))),
        )
        changed = [dict(row) for row in first]
        changed[0]["sizeBytes"] = 3
        self.assertNotEqual(
            VALIDATOR.inventory_fingerprint(first),
            VALIDATOR.inventory_fingerprint(changed),
        )

    def test_pdf_page_content_inflates_and_checks_selected_text(self) -> None:
        pdf = make_pdf(b"BT /F1 12 Tf (Heavy Eval 01) Tj (Score 01) Tj ET")
        result = VALIDATOR.inspect_pdf(pdf)
        self.assertEqual(result["pageCount"], 1)
        self.assertGreater(result["decompressedPageContentBytes"], 0)
        self.assertIn("heavyeval01", result["textNormalized"])
        self.assertIn("score01", result["textNormalized"])

    def test_report_filename_score_parser_accepts_parenthesized_score(self) -> None:
        self.assertEqual(
            VALIDATOR._score_from_report_name("Heavy Eval 01 (Score 25)"),
            "Score 25",
        )
        self.assertIsNone(VALIDATOR._score_from_report_name("Heavy Eval 01"))

    def test_pdf_stream_resolves_indirect_length_object(self) -> None:
        pdf = make_pdf_with_indirect_stream_length(
            b"BT /F1 12 Tf (Heavy Eval 01) Tj (Score 01) Tj ET"
        )
        result = VALIDATOR.inspect_pdf(pdf)
        self.assertEqual(result["pageCount"], 1)
        self.assertGreater(result["decompressedPageContentBytes"], 0)
        self.assertIn("heavyeval01", result["textNormalized"])

    def test_pdf_text_uses_the_active_font_to_unicode_map(self) -> None:
        stream = b"/F1 12 Tf <0001> Tj /F2 12 Tf <0001> Tj"
        font_cmaps = {
            "F1": {b"\x00\x01": "S"},
            "F2": {b"\x00\x01": "H"},
        }
        self.assertEqual(
            VALIDATOR._pdf_text_strings(stream, {}, font_cmaps),
            ["S", "H"],
        )

    def test_pdf_rejects_missing_page_content(self) -> None:
        pdf = b"%PDF-1.4\n1 0 obj << /Type /Catalog >> endobj\n%%EOF"
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.inspect_pdf(pdf)

    def test_output_archive_checks_one_pdf_member_at_a_time(self) -> None:
        report_names = [f"Heavy Eval 01 (Score {index:02d})" for index in range(1, 26)]
        with tempfile.TemporaryDirectory() as temporary_directory:
            archive = Path(temporary_directory) / "output.zip"
            with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
                for index, report_name in enumerate(report_names, start=1):
                    content = f"BT /F1 12 Tf (Heavy Eval 01) Tj (Score {index:02d}) Tj ET".encode("ascii")
                    bundle.writestr(f"{report_name}.pdf", make_pdf(content))

            result = VALIDATOR.inspect_output_archive(archive, report_names)

        self.assertEqual(result["pdfCount"], 25)
        self.assertTrue(result["validatorMemory"]["membersProcessedOneAtATime"])
        self.assertEqual(result["validatorMemory"]["membersProcessed"], 25)
        self.assertEqual(result["pdfs"][0]["expectedTextChecks"], ["Heavy Eval 01", "Score 01"])

    def test_unicode_cmap_decodes_two_byte_text(self) -> None:
        cmap_stream = (
            b"1 beginbfchar\n<0001> <0041>\nendbfchar\n"
            b"1 beginbfrange\n<0002> <0003> <0042>\nendbfrange"
        )
        cmap = VALIDATOR._unicode_cmap({7: cmap_stream})
        self.assertEqual(VALIDATOR._decode_pdf_hex(b"\x00\x01\x00\x02\x00\x03", cmap), "ABC")

    def test_png_validates_crc_and_inflated_dimensions(self) -> None:
        header = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
        payload = (
            b"\x89PNG\r\n\x1a\n"
            + png_chunk(b"IHDR", header)
            + png_chunk(b"IDAT", zlib.compress(b"\x00\x10\x20\x30"))
            + png_chunk(b"IEND", b"")
        )
        result = VALIDATOR.inspect_png(payload)
        self.assertEqual((result["width"], result["height"]), (1, 1))
        self.assertEqual(result["decodedBytes"], 4)

    def test_png_preview_content_metrics_come_from_decoded_pixels(self) -> None:
        width = height = 40
        raw = bytearray()
        for y in range(height):
            raw.append(0)
            for x in range(width):
                if 8 <= y < 32 and 4 <= x < 36:
                    value = (x * 7 + y * 3) % 256
                    if (x + y) % 7 == 0:
                        raw.extend((0, 0, 0))
                    else:
                        raw.extend((value, (value + y * 2) % 256, (value + x) % 256))
                else:
                    raw.extend((255, 255, 255))
        header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
        payload = (
            b"\x89PNG\r\n\x1a\n"
            + png_chunk(b"IHDR", header)
            + png_chunk(b"IDAT", zlib.compress(bytes(raw)))
            + png_chunk(b"IEND", b"")
        )

        result = VALIDATOR.inspect_png(payload)["previewContentStats"]
        self.assertGreater(result["sampledPixels"], 0)
        self.assertGreater(result["uniqueSampledColors"], 5)
        self.assertGreater(result["nonWhiteSampledPixels"], 0)
        self.assertGreater(result["darkSampledPixels"], 0)

    def test_screenshot_content_checks_keep_pixel_counts_exact_with_color_count_variance(self) -> None:
        captured = {
            "region": "central-lower-preview-area",
            "x": 10,
            "y": 20,
            "width": 100,
            "height": 80,
            "sampledPixels": 100,
            "uniqueSampledColors": 246,
            "nonWhiteSampledPixels": 70,
            "darkSampledPixels": 25,
        }
        decoded = dict(captured, uniqueSampledColors=556)
        VALIDATOR.validate_preview_content_stats(captured, decoded, "direct-report-preview")

        decoded["darkSampledPixels"] = 0
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_preview_content_stats(captured, decoded, "direct-report-preview")

    def test_uiatargeted_navigation_click_requires_measured_rect_and_page_route(self) -> None:
        action = self.window_targeted_navigation_action()
        VALIDATOR._validate_window_targeted_navigation_click(action, 99)

        action["pointerAction"]["screenX"] = 201  # type: ignore[index]
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR._validate_window_targeted_navigation_click(action, 99)

    def test_uiatargeted_navigation_click_rejects_missing_route_confirmation(self) -> None:
        action = self.window_targeted_navigation_action()
        action["verifiedByFollowingPageRoute"] = False
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR._validate_window_targeted_navigation_click(action, 99)

    def test_uiatargeted_my_workspace_click_requires_measured_rect_and_completed_route(self) -> None:
        action = self.window_targeted_navigation_action("My Workspace", "my_workspace")
        VALIDATOR._validate_window_targeted_navigation_click(action, 99)

        action["pageRouteEvent"]["targetPageId"] = "speaking_evaluation"  # type: ignore[index]
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR._validate_window_targeted_navigation_click(action, 99)

    def test_runner_diagnostics_keep_app_and_harness_processes_separate_and_queries_bounded(self) -> None:
        manifest = {
            "process": {"processId": 99, "mainWindowHandle": 1234},
            "runnerMetrics": [{
                "runnerProcessId": 100,
                "measuredAppProcessId": 99,
                "runnerWorkingSetBytes": 180_000_000,
                "runnerHandles": 300,
                "runnerThreadCount": 20,
            }],
            "uiaQueryMetrics": [{
                "processId": 99,
                "selector": "speakingEvaluationReports",
                "container": "main",
                "rootWindowHandle": 1234,
                "rootWindowProcessId": 99,
                "method": "Exact root identity or FindFirst with combined AutomationId + ControlType + Name conditions",
                "queryScope": "Descendants",
                "parentAutomationId": "",
                "conditionProperties": ["AutomationId", "ControlType", "Name"],
                "nameConditionApplied": True,
                "expectedName": "Reports",
                "observedName": "Reports",
                "postQueryIdentityCheck": True,
                "targetEnabled": False,
                "ancestorWindowHandle": 1234,
                "ancestorWindowProcessId": 99,
                "attemptCount": 1,
                "retryCount": 0,
                "resultCount": 1,
                "materializedTreeElementCount": 0,
                "elapsedMilliseconds": 4.25,
                "runnerProcessId": 100,
                "runnerWorkingSetBytes": 180_000_000,
                "runnerWorkingSetGuardBytesExclusive": VALIDATOR.RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT,
            }],
        }
        result = VALIDATOR.validate_runner_diagnostics(manifest)
        self.assertEqual(result["runnerProcessId"], 100)
        self.assertEqual(result["uiaQueryMaterializedTreeElementCount"], 0)
        self.assertEqual(result["uiaQueryRunnerMaxWorkingSetBytes"], 180_000_000)

        manifest["uiaQueryMetrics"][0]["materializedTreeElementCount"] = 25  # type: ignore[index]
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_runner_diagnostics(manifest)

        manifest["uiaQueryMetrics"][0]["materializedTreeElementCount"] = 0  # type: ignore[index]
        manifest["uiaQueryMetrics"][0]["runnerWorkingSetBytes"] = VALIDATOR.RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT  # type: ignore[index]
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_runner_diagnostics(manifest)

        manifest["uiaQueryMetrics"][0]["runnerWorkingSetBytes"] = 180_000_000  # type: ignore[index]
        manifest["uiaQueryMetrics"][0]["runnerProcessId"] = 101  # type: ignore[index]
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_runner_diagnostics(manifest)

        manifest["uiaQueryMetrics"][0]["runnerProcessId"] = 100  # type: ignore[index]
        manifest["runnerMetrics"][0]["runnerWorkingSetBytes"] = VALIDATOR.RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT  # type: ignore[index]
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_runner_diagnostics(manifest)

    def test_runner_diagnostics_accept_blank_name_two_property_query_only_with_postcheck(self) -> None:
        manifest = {
            "process": {"processId": 99, "mainWindowHandle": 1234},
            "runnerMetrics": [{
                "runnerProcessId": 100,
                "measuredAppProcessId": 99,
                "runnerWorkingSetBytes": 180_000_000,
                "runnerHandles": 300,
                "runnerThreadCount": 20,
            }],
            "uiaQueryMetrics": [{
                "processId": 99,
                "selector": "speakingEvaluationChoice",
                "container": "main",
                "rootWindowHandle": 1234,
                "rootWindowProcessId": 99,
                "method": "FindFirst with combined AutomationId + ControlType conditions; post-query Name/PID/owner/enabled check",
                "queryScope": "Descendants",
                "parentAutomationId": "",
                "conditionProperties": ["AutomationId", "ControlType"],
                "nameConditionApplied": False,
                "expectedName": "",
                "observedName": "",
                "postQueryIdentityCheck": True,
                "targetEnabled": True,
                "ancestorWindowHandle": 1234,
                "ancestorWindowProcessId": 99,
                "attemptCount": 1,
                "retryCount": 0,
                "resultCount": 1,
                "materializedTreeElementCount": 0,
                "elapsedMilliseconds": 4.25,
                "runnerProcessId": 100,
                "runnerWorkingSetBytes": 180_000_000,
                "runnerWorkingSetGuardBytesExclusive": VALIDATOR.RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT,
            }],
        }
        self.assertEqual(VALIDATOR.validate_runner_diagnostics(manifest)["uiaQueryCount"], 1)

        manifest["uiaQueryMetrics"][0]["observedName"] = "unexpected"  # type: ignore[index]
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_runner_diagnostics(manifest)

        manifest["uiaQueryMetrics"][0]["observedName"] = ""  # type: ignore[index]
        manifest["uiaQueryMetrics"][0]["targetEnabled"] = False  # type: ignore[index]
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_runner_diagnostics(manifest)

        manifest["uiaQueryMetrics"][0]["targetEnabled"] = True  # type: ignore[index]
        manifest["uiaQueryMetrics"][0]["postQueryIdentityCheck"] = False  # type: ignore[index]
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_runner_diagnostics(manifest)

        child_query = manifest["uiaQueryMetrics"][0]
        child_query.update({
            "selector": "speakingEvaluationClassItem",
            "method": "FindFirst immediate child with combined AutomationId + ControlType + Name conditions; post-query PID/owner/enabled check",
            "queryScope": "Children",
            "parentAutomationId": "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationContentSplitter.speakingEvaluationClassList",
            "conditionProperties": ["AutomationId", "ControlType", "Name"],
            "nameConditionApplied": True,
            "expectedName": "Heavy Fixture Class 001",
            "observedName": "Heavy Fixture Class 001",
            "postQueryIdentityCheck": True,
            "targetEnabled": True,
        })
        self.assertEqual(VALIDATOR.validate_runner_diagnostics(manifest)["uiaQueryCount"], 1)
        child_query["parentAutomationId"] = "unverified-parent"
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_runner_diagnostics(manifest)

        child_query.update({
            "selector": "speakingEvaluationNavigationItem",
            "parentAutomationId": "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree",
            "expectedName": "Speaking Evaluation",
            "observedName": "Speaking Evaluation",
        })
        self.assertEqual(VALIDATOR.validate_runner_diagnostics(manifest)["uiaQueryCount"], 1)
        child_query["parentAutomationId"] = "unverified-parent"
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_runner_diagnostics(manifest)

        child_query.update({
            "selector": "myWorkspaceNavigationItem",
            "parentAutomationId": "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree",
            "expectedName": "My Workspace",
            "observedName": "My Workspace",
        })
        self.assertEqual(VALIDATOR.validate_runner_diagnostics(manifest)["uiaQueryCount"], 1)
        child_query["parentAutomationId"] = "unverified-parent"
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_runner_diagnostics(manifest)

    def test_post_route_uia_wait_enforces_found_control_and_hard_attempt_cap(self) -> None:
        record = {
            "selector": "speakingEvaluationClassList",
            "processId": 99,
            "mainWindowHandle": 1234,
            "mainWindowProcessId": 99,
            "status": "found",
            "found": True,
            "attemptCount": 1,
            "maximumAttempts": 60,
            "intervalMilliseconds": 250,
            "elapsedMilliseconds": 8.5,
            "queryCountStart": 0,
            "queryCountEnd": 1,
            "materializedTreeElementCount": 0,
            "appWorkingSetStartBytes": 200_000_000,
            "appWorkingSetEndBytes": 201_000_000,
            "appWorkingSetMaxBytes": 201_000_000,
            "runnerWorkingSetStartBytes": 100_000_000,
            "runnerWorkingSetEndBytes": 101_000_000,
            "runnerWorkingSetMaxBytes": 101_000_000,
            "runnerProcessId": 100,
        }
        manifest = {"process": {"processId": 99, "mainWindowHandle": 1234}, "uiaWaitMetrics": [record]}
        self.assertEqual(VALIDATOR.validate_uia_wait_metrics(manifest, {"speakingEvaluationClassList"})["maximumAttemptCount"], 1)

        record["attemptCount"] = 61
        record["queryCountEnd"] = 61
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_uia_wait_metrics(manifest, {"speakingEvaluationClassList"})

    def test_uia_query_memory_diagnostic_binds_900_queries_and_runner_memory(self) -> None:
        diagnostic = {
            "status": "passed",
            "failureMessage": None,
            "appProcessId": 99,
            "iterations": 300,
            "completedIterations": 300,
            "intervalMilliseconds": 100,
            "selectors": ["speakingEvaluationClassList", "speakingEvaluationChoice", "speakingEvaluationSaveReports"],
            "queriesPerIteration": 3,
            "expectedQueryCount": 900,
            "observedQueryCount": 900,
            "queryCountStart": 10,
            "queryCountEnd": 910,
            "materializedTreeElementCount": 0,
            "appWorkingSetStartBytes": 200_000_000,
            "appWorkingSetMaxBytes": 201_000_000,
            "appWorkingSetEndBytes": 200_000_000,
            "runnerProcessId": 100,
            "runnerWorkingSetStartBytes": 100_000_000,
            "runnerWorkingSetMaxBytes": 120_000_000,
            "runnerWorkingSetEndBytes": 110_000_000,
            "runnerWorkingSetGuardBytesExclusive": VALIDATOR.RUNNER_QUERY_BURST_WORKING_SET_LIMIT,
            "elapsedMilliseconds": 30_100,
            "startedAtUtc": "2026-11-20T10:00:05.000Z",
        }
        manifest = {
            "process": {"processId": 99},
            "uiaQueryBurstDiagnostic": diagnostic,
            "uiaActions": {"navigateToSpeakingEvaluation": {"pageRouteEvent": {
                "observed": True,
                "targetPageId": "speaking_evaluation",
                "observedAtUtc": "2026-11-20T10:00:00.000Z",
            }}},
        }
        self.assertEqual(VALIDATOR.validate_uia_query_burst_diagnostic(manifest)["findFirstQueryCount"], 900)

        diagnostic["runnerWorkingSetMaxBytes"] = VALIDATOR.RUNNER_QUERY_BURST_WORKING_SET_LIMIT
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_uia_query_burst_diagnostic(manifest)

    def test_export_completion_ui_probe_is_single_attempt_and_window_bound(self) -> None:
        manifest = {
            "process": {"processId": 99, "mainWindowHandle": 1234},
            "exportCompletionUiaProbe": {
                "status": "reports-ready-notice-found",
                "attemptCount": 1,
                "maximumAttempts": 1,
                "archiveStableCheckCount": 3,
                "processId": 99,
                "mainWindowHandle": 1234,
                "mainWindowProcessId": 99,
                "materializedTreeElementCount": 0,
                "runnerProcessId": 100,
                "runnerWorkingSetBeforeBytes": 200_000_000,
                "runnerWorkingSetAfterBytes": 201_000_000,
                "appWorkingSetBeforeBytes": 210_000_000,
                "appWorkingSetAfterBytes": 211_000_000,
                "noticeWindowFound": True,
                "batchDialogFound": False,
                "queryCountStart": 99,
                "queryCountEnd": 100,
                "elapsedMilliseconds": 8.25,
            },
        }
        self.assertEqual(VALIDATOR.validate_export_completion_uia_probe(manifest)["uiaQueryCount"], 1)

        manifest["exportCompletionUiaProbe"]["attemptCount"] = 2  # type: ignore[index]
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_export_completion_uia_probe(manifest)

    def test_uia_absence_waits_are_bounded_and_distinguish_hidden_controls(self) -> None:
        selectors = [
            "nextSpeakingEvaluationReports",
            "nextSpeakingEvaluationReports",
            "qt_msgbox_buttonbox",
            "nextSpeakingEvaluationBatchReports",
        ]
        records = []
        for index, selector in enumerate(selectors):
            records.append({
                "selector": selector,
                "processId": 99,
                "mainWindowHandle": 1234,
                "mainWindowProcessId": 99,
                "status": "offscreen" if index == 2 else "not-found",
                "absenceObserved": True,
                "observedOffscreen": index == 2,
                "attemptCount": 1,
                "maximumAttempts": 60,
                "intervalMilliseconds": 100,
                "elapsedMilliseconds": 5,
                "queryCountStart": index,
                "queryCountEnd": index + 1,
                "materializedTreeElementCount": 0,
                "runnerProcessId": 100,
                "runnerWorkingSetStartBytes": 100_000_000,
                "runnerWorkingSetMaxBytes": 101_000_000,
                "runnerWorkingSetEndBytes": 100_000_000,
            })
        manifest = {
            "process": {"processId": 99, "mainWindowHandle": 1234},
            "uiaAbsenceWaitMetrics": records,
        }
        self.assertEqual(VALIDATOR.validate_uia_absence_waits(manifest)["waitCount"], 4)

        records[2]["status"] = "timeout"
        records[2]["absenceObserved"] = False
        records[2]["attemptCount"] = 61
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR.validate_uia_absence_waits(manifest)

    def test_evaluation_name_mapping_allows_only_the_selected_cell(self) -> None:
        columns = ["id", "class_id", "evaluation_name"]
        rows_before = [
            [evaluation_id, evaluation_id, f"Heavy Term {evaluation_id:02d}"]
            for evaluation_id in range(1, 21)
        ]
        rows_after = [list(row) for row in rows_before]
        rows_after[0][2] = "Winter"
        before_hash = VALIDATOR.canonical_sha256({"columns": columns, "rows": rows_before})
        after_hash = VALIDATOR.canonical_sha256({"columns": columns, "rows": rows_after})
        before_workspace_hash = "a" * 64
        after_workspace_hash = "b" * 64
        mapping = {
            "table": "speaking_evaluations",
            "column": "evaluation_name",
            "evaluationId": 1,
            "classId": 1,
            "beforeValue": "Heavy Term 01",
            "afterValue": "Winter",
            "updateRowCount": 1,
            "onlySelectedEvaluationNameCellChanged": True,
            "seededTableSha256BeforeMapping": before_hash,
            "preparedTableSha256AfterMapping": after_hash,
            "workspaceSha256BeforeMapping": before_workspace_hash,
            "workspaceSha256AfterMapping": after_workspace_hash,
        }

        result = VALIDATOR._validate_evaluation_name_ui_mapping(
            mapping, columns, rows_after, after_hash, after_workspace_hash
        )
        self.assertEqual(result["beforeValue"], "Heavy Term 01")
        self.assertEqual(result["afterValue"], "Winter")
        self.assertTrue(result["onlySelectedEvaluationNameCellChanged"])

        rows_after[1][2] = "Unexpected"
        with self.assertRaises(VALIDATOR.ValidationFailure):
            VALIDATOR._validate_evaluation_name_ui_mapping(
                mapping, columns, rows_after, after_hash, after_workspace_hash
            )

    def test_export_scope_is_first_25_of_all_30_source_rows(self) -> None:
        source_names = [f"Heavy Eval 01 (Score {index:02d})" for index in range(1, 31)]
        selected = VALIDATOR.selected_export_report_names(source_names)
        self.assertEqual(len(source_names), 30)
        self.assertEqual(len(selected), 25)
        self.assertEqual(selected, source_names[:25])
        self.assertEqual(source_names[25:], [
            "Heavy Eval 01 (Score 26)",
            "Heavy Eval 01 (Score 27)",
            "Heavy Eval 01 (Score 28)",
            "Heavy Eval 01 (Score 29)",
            "Heavy Eval 01 (Score 30)",
        ])

    def test_runtime_settings_proves_empty_default_profile_and_rejects_override(self) -> None:
        with tempfile.TemporaryDirectory() as temp_name:
            evidence_root = Path(temp_name)
            settings_root = evidence_root / "runtime" / "settings"
            settings_root.mkdir(parents=True)
            policy = evidence_root / "src" / "next" / "application" / "roster_row_display_policy.h"
            policy.parent.mkdir(parents=True)
            policy.write_text("inline constexpr int DefaultMaximumRosterRows = 25;\n", encoding="utf-8")
            before_payload = {
                "root": str(settings_root.resolve()),
                "sha256": VALIDATOR.inventory_fingerprint([]),
                "fileCount": 0,
                "files": [],
            }
            settings_file = settings_root / "PaperCloud" / "ClassMngr.ini"
            settings_file.parent.mkdir(parents=True)
            settings_file.write_text("[files]\nlast=C:/workspace.tps\n", encoding="utf-8")
            after_files = [{
                "path": "PaperCloud/ClassMngr.ini",
                "sizeBytes": settings_file.stat().st_size,
                "sha256": VALIDATOR.sha256_file(settings_file),
            }]
            after_payload = {
                "root": str(settings_root.resolve()),
                "sha256": VALIDATOR.inventory_fingerprint(after_files),
                "fileCount": 1,
                "files": after_files,
            }
            before_path = evidence_root / "runtime" / "settings-before-launch-inventory.json"
            after_path = evidence_root / "runtime" / "settings-after-exit-inventory.json"
            before_path.write_text(json.dumps(before_payload), encoding="utf-8")
            after_path.write_text(json.dumps(after_payload), encoding="utf-8")
            manifest = {
                "repositoryRoot": str(evidence_root),
                "runtimeSettingsProvenance": {
                    "rootRelativePath": "runtime/settings",
                    "maximumRosterRowsKey": "options/maximumRosterRows",
                    "configuredMaximumRosterRows": None,
                    "effectiveDefaultMaximumRosterRows": 25,
                    "defaultPolicySource": "src/next/application/roster_row_display_policy.h",
                    "beforeLaunchInventoryPath": "runtime/settings-before-launch-inventory.json",
                    "beforeLaunchInventorySha256": VALIDATOR.sha256_file(before_path),
                    "beforeLaunchFileCount": 0,
                    "beforeLaunchRootSha256": before_payload["sha256"],
                    "afterExitInventoryPath": "runtime/settings-after-exit-inventory.json",
                    "afterExitInventorySha256": VALIDATOR.sha256_file(after_path),
                    "afterExitFileCount": 1,
                    "afterExitRootSha256": after_payload["sha256"],
                },
            }
            result = VALIDATOR.validate_runtime_settings(evidence_root, manifest)
            self.assertEqual(result["effectiveDefaultMaximumRosterRows"], 25)
            self.assertEqual(result["beforeLaunchFileCount"], 0)

            settings_file.write_text("[options]\nmaximumRosterRows=30\n", encoding="utf-8")
            after_files[0]["sizeBytes"] = settings_file.stat().st_size
            after_files[0]["sha256"] = VALIDATOR.sha256_file(settings_file)
            after_payload["sha256"] = VALIDATOR.inventory_fingerprint(after_files)
            after_path.write_text(json.dumps(after_payload), encoding="utf-8")
            manifest["runtimeSettingsProvenance"]["afterExitInventorySha256"] = VALIDATOR.sha256_file(after_path)
            manifest["runtimeSettingsProvenance"]["afterExitRootSha256"] = after_payload["sha256"]
            with self.assertRaises(VALIDATOR.ValidationFailure):
                VALIDATOR.validate_runtime_settings(evidence_root, manifest)


if __name__ == "__main__":
    unittest.main()
