#!/usr/bin/env python3
"""Fail-closed validator for the packaged P9-12 Next PDF viewer route."""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import re
import struct
import sys
from pathlib import Path
from typing import Any


TASK_ID = "P9_12_NEXT_QTPDF_PACKAGED_LIFECYCLE"
SCHEMA = "classmngr-phase9-packaged-next-pdf-viewer-lifecycle-v1"
WORKING_SET_LIMIT = 262_144_000
OPERATION_PEAK_ADVISORY = 536_870_912
EXPECTED_DOCUMENT_ID = "document_guides_lesson_planning"
EXPECTED_DOCUMENT_NAME = "Lesson Planning"
EXPECTED_SUSPENSION_PAGE_ID = "my_workspace"
EXPECTED_SUSPENSION_PAGE_NAME = "My Workspace"
EXPECTED_PDF_RELATIVE_PATH = "resources/assets/documents/Guides/DYB Lesson Planning Guide.pdf"
EXPECTED_PDF_SIZE = 3_533_707
EXPECTED_PDF_SHA256 = "295accab548b41f0b4f56e6ab89958ad5c3347646904c155b16a56ebe926caec"
EXPECTED_NEW_SCRIPT_PATHS = {
    "scripts/phase9/run_packaged_next_pdf_viewer_lifecycle.ps1",
    "scripts/phase9/validate_packaged_next_pdf_viewer_lifecycle.py",
    "scripts/phase9/test_validate_packaged_next_pdf_viewer_lifecycle.py",
}


class ValidationFailure(RuntimeError):
    pass


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationFailure(message)


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _file_sha256(path: Path) -> str:
    return _sha256(path.read_bytes())


def _read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    _require(isinstance(value, dict), f"JSON object expected at {path}")
    return value


def inventory_fingerprint(rows: list[dict[str, Any]]) -> str:
    normalized = [
        (str(row["path"]), int(row["sizeBytes"]), str(row["sha256"]).lower())
        for row in rows
    ]
    normalized.sort(key=lambda row: row[0].lower())
    canonical = "\n".join(f"{path}\t{size}\t{digest}" for path, size, digest in normalized) + "\n"
    return _sha256(canonical.encode("utf-8"))


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    _require(path.is_file(), f"required JSONL evidence is missing: {path}")
    values: list[dict[str, Any]] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        if not line.strip():
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError as error:
            raise ValidationFailure(f"invalid JSONL at {path}:{line_number}: {error}") from error
        _require(isinstance(value, dict), f"JSONL object expected at {path}:{line_number}")
        values.append(value)
    return values


def lifecycle_records_from_logs(stdout_path: Path, stderr_path: Path) -> list[dict[str, Any]]:
    pattern = re.compile(r"^ClassMngr lifecycle checkpoint=(\{.*\})$")
    records: list[dict[str, Any]] = []
    for stream, path in (("stdout", stdout_path), ("stderr", stderr_path)):
        _require(path.is_file(), f"process {stream} log is missing: {path}")
        for line_number, line in enumerate(path.read_text(encoding="utf-8-sig", errors="replace").splitlines(), 1):
            match = pattern.match(line)
            if not match:
                continue
            try:
                record = json.loads(match.group(1))
            except json.JSONDecodeError as error:
                raise ValidationFailure(f"invalid QtPdf lifecycle record at {path}:{line_number}: {error}") from error
            record["sourceStream"] = stream
            record["sourceLine"] = line_number
            records.append(record)
    records.sort(key=lambda row: (str(row.get("captured_at", "")), int(row.get("sourceLine", 0))))
    return records


def pdf_state(details: dict[str, Any]) -> dict[str, Any]:
    page_count = int(details.get("page_count", -1))
    return {
        "status": int(details.get("status", -1)),
        "error": int(details.get("error", -1)),
        "pageCount": page_count,
        "pageModelRowCount": page_count,
        "pageIndex": details.get("page_index"),
        "pageWidthPoints": details.get("page_width_points"),
        "pageHeightPoints": details.get("page_height_points"),
        "activeStream": bool(details.get("active_stream", False)),
        "documentPresent": bool(details.get("document_present", False)),
        "viewPresent": bool(details.get("view_present", False)),
        "borrowCount": int(details.get("borrow_count", -1)),
        "generation": int(details.get("generation", -1)),
    }


def validate_pdf_state(state: dict[str, Any], expected: str) -> None:
    if expected == "ready":
        _require(state["status"] == 2, "QPdfDocument is not Ready (QtPdf Status::Ready is 2)")
        _require(state["error"] == 0, "QPdfDocument reported an error while Ready")
        _require(state["pageCount"] > 0 and state["pageModelRowCount"] == state["pageCount"], "ready PDF page model is empty or inconsistent")
        _require(state["pageIndex"] == 0, "ready PDF did not start on its first zero-based page")
        _require(float(state["pageWidthPoints"] or 0) > 0 and float(state["pageHeightPoints"] or 0) > 0, "ready PDF page dimensions are missing")
        _require(state["activeStream"] and state["documentPresent"] and state["viewPresent"], "ready PDF is missing its stream, document wrapper, or view")
        _require(state["borrowCount"] == 0, "PDF viewer has an outstanding borrowed document")
    elif expected == "empty-wrapper":
        _require(state["status"] == 0, "closed PDF wrapper is not in Null status")
        _require(state["error"] == 0 and state["pageCount"] == 0 and state["pageModelRowCount"] == 0, "closed PDF wrapper retained pages or an error")
        _require(not state["activeStream"], "closed PDF retained its primary stream")
        _require(state["documentPresent"] and state["viewPresent"], "close did not retain the documented empty QPdfDocument/QPdfView wrappers")
        _require(state["borrowCount"] == 0, "closed PDF viewer retained an outstanding borrow")
    elif expected == "destroyed":
        _require(state["status"] == 0 and state["pageCount"] == 0 and state["pageModelRowCount"] == 0, "final PDF teardown retained page model state")
        _require(not state["activeStream"] and not state["documentPresent"] and not state["viewPresent"], "final PDF teardown retained a stream, document, or view")
    else:
        raise ValidationFailure(f"unknown expected PDF state {expected!r}")


def validate_qpdf_sequence(
    records: list[dict[str, Any]],
    checkpoints: dict[str, dict[str, Any]],
    page_host: dict[str, dict[str, Any]],
) -> dict[str, int]:
    qpdf = [row for row in records if str(row.get("owner", "")) == "document_viewer"]
    _require(qpdf, "no structured QtPdf viewer diagnostics were captured")
    sessions = [row for row in qpdf if row.get("event") == "qpdf-session-create" and row.get("details", {}).get("phase") == "after"]
    _require(len(sessions) == 1, f"expected one retained viewer session, found {len(sessions)}")
    session_id = int(sessions[0].get("instance_id", -1))
    _require(session_id > 0, "QtPdf session has no diagnostic identity")
    _require(all(int(row.get("instance_id", -2)) == session_id for row in qpdf), "QtPdf lifecycle records changed session identity across close/reopen")

    loads = [row for row in qpdf if row.get("event") == "qpdf-document-load" and row.get("details", {}).get("phase") == "after"]
    opens = [row for row in qpdf if row.get("event") == "qpdf-open" and row.get("details", {}).get("phase") == "after"]
    stream_opens = [row for row in qpdf if row.get("event") == "primary-stream-open" and row.get("details", {}).get("phase") == "after"]
    stream_releases = [row for row in qpdf if row.get("event") == "primary-stream-release" and row.get("details", {}).get("phase") == "after"]
    document_closes = [row for row in qpdf if row.get("event") == "qpdf-document-close" and row.get("details", {}).get("phase") == "after"]
    _require(len(loads) == len(opens) == len(stream_opens) == 3, "the same PDF was not opened and loaded exactly three times")
    _require(len(stream_releases) == 3, f"expected release after explicit close, suspension, and final teardown; found {len(stream_releases)}")
    _require(len(document_closes) == 3, f"expected three terminal QPdfDocument closes, found {len(document_closes)}")
    stream_sequence = [str(row.get("event")) for row in qpdf if row.get("event") in {"primary-stream-open", "primary-stream-release"} and row.get("details", {}).get("phase") == "after"]
    _require(stream_sequence == ["primary-stream-open", "primary-stream-release"] * 3, "PDF streams were not opened and released in three ordered lifecycle generations")
    _require(all(pdf_state(row["details"])["activeStream"] for row in stream_opens), "a primary stream was not active after open")
    _require(all(not pdf_state(row["details"])["activeStream"] for row in stream_releases), "a released primary stream remained active")

    for name in ("firstOpen", "reopen", "postSuspendReopen"):
        _require(name in checkpoints, f"missing lifecycle checkpoint {name}")
        validate_pdf_state(checkpoints[name]["pdfState"], "ready")
        _require(checkpoints[name]["documentId"] == EXPECTED_DOCUMENT_ID, f"{name} did not use the same typed catalog DocumentId")
    for name in ("closed", "suspended"):
        _require(name in checkpoints, f"missing lifecycle checkpoint {name}")
        validate_pdf_state(checkpoints[name]["pdfState"], "empty-wrapper")

    _require(checkpoints["firstOpen"]["pdfState"]["generation"] < checkpoints["reopen"]["pdfState"]["generation"] < checkpoints["postSuspendReopen"]["pdfState"]["generation"], "close/reopen did not advance the viewer generation")
    _require(checkpoints["firstOpen"]["pdfState"]["pageCount"] == checkpoints["reopen"]["pdfState"]["pageCount"] == checkpoints["postSuspendReopen"]["pdfState"]["pageCount"], "reopen changed the PDF page count")
    _require(page_host["catalogReady"]["viewerPageInstanceCount"] == 0, "viewer page was instantiated before a document selection")
    for name in ("firstOpen", "closed", "reopen", "postSuspendReopen"):
        _require(page_host[name]["viewerPageInstanceCount"] == 1, f"UIA did not expose exactly one viewer page instance at {name}")
    suspended_page = page_host["suspended"]
    _require(suspended_page["viewerPageVisibleCount"] == 0, "viewer page remained visible after PageHost suspension")
    if suspended_page["viewerPageInstanceCount"] == 1:
        _require(suspended_page["viewerPageOffscreenCount"] == 1, "suspended viewer page was not marked offscreen")
    else:
        _require(suspended_page["viewerPageInstanceCount"] == 0 and suspended_page.get("viewerPageUiaExposure") == "not-present-in-control-view", "suspended viewer PageHost instance was neither retained offscreen nor explicitly absent from UIA control view")
        _require(int(suspended_page.get("retainedViewerSessionId", -1)) == session_id, "suspended UIA-hidden page lacks the same retained QtPdf session identity")
    _require(page_host["postSuspendReopen"]["viewerPageVisibleCount"] == 1, "reentry did not make the retained viewer page visible")

    final = [row for row in qpdf if row.get("event") == "qpdf-session-release" and row.get("details", {}).get("phase") == "after"]
    _require(len(final) == 1, "final process teardown did not release the one QtPdf viewer session")
    validate_pdf_state(pdf_state(final[0]["details"]), "destroyed")
    view_destroy = [row for row in qpdf if row.get("event") == "qpdf-view-destroy" and row.get("details", {}).get("phase") == "after"]
    _require(len(view_destroy) == 1 and not view_destroy[0]["details"].get("view_present", True), "final teardown did not destroy the QPdfView")
    retired = [row for row in qpdf if row.get("event") == "qpdf-document-destroy" and row.get("details", {}).get("phase") == "after"]
    _require(retired and all(not row["details"].get("retired_document_present", True) for row in retired), "a retired QPdfDocument remained live after destruction")
    return {"viewerSessionCount": 1, "documentLoads": len(loads), "streamOpens": len(stream_opens), "streamReleases": len(stream_releases), "retiredDocumentDestructions": len(retired), "finalViewDestructions": len(view_destroy)}


def validate_working_set_metrics(metrics: list[dict[str, Any]], process_id: int) -> tuple[int, int]:
    by_point = {str(row.get("point")): row for row in metrics}
    required_points = ("startup-ready", "catalog-ready", "first-open", "closed", "reopen", "suspended", "post-suspend-reopen", "pre-shutdown")
    _require(all(point in by_point for point in required_points), "one or more required stable lifecycle memory checkpoints are missing")
    for point in required_points:
        row = by_point[point]
        _require(int(row.get("currentWorkingSetBytes", WORKING_SET_LIMIT)) < WORKING_SET_LIMIT, f"{point} working set is not strictly below {WORKING_SET_LIMIT}")
        _require(int(row.get("pid", 0)) == process_id, f"{point} memory sample is not bound to the measured process")
    operation_peak = max((int(row.get("peakWorkingSetBytes", 0)) for row in metrics), default=0)
    return operation_peak, len(required_points)


def _validate_source_equivalence(root: Path, manifest: dict[str, Any]) -> None:
    source = manifest.get("source") or {}
    repository_root = Path(str(source.get("repositoryRoot", ""))).resolve()
    _require(repository_root.is_dir(), "source repository root is missing")
    source_path = root / str(source.get("inventoryPath", ""))
    source_inventory = _read_json(source_path)
    rows = source_inventory.get("files")
    _require(isinstance(rows, list) and rows, "current source inventory is empty")
    current_sha = inventory_fingerprint(rows)
    _require(current_sha == source.get("sha256"), "current source inventory fingerprint differs from manifest")
    _require(current_sha == source_inventory.get("sha256"), "source inventory JSON omits or misstates its top-level fingerprint")
    _require(_file_sha256(source_path) == source.get("inventoryFileSha256"), "source inventory file hash differs from manifest")
    _require(source_inventory.get("includedRoots") == source.get("includedRoots"), "source inventory roots differ from manifest")
    for row in rows:
        live_path = repository_root.joinpath(*str(row["path"]).split("/"))
        _require(live_path.is_file(), f"measured current-source file is missing: {row['path']}")
        _require(live_path.stat().st_size == int(row["sizeBytes"]), f"measured current-source size changed: {row['path']}")
        _require(_file_sha256(live_path) == str(row["sha256"]).lower(), f"measured current-source SHA changed: {row['path']}")
    derivation = _read_json(root / str(source.get("derivedReceiptPath", "")))
    _require(_file_sha256(root / str(source.get("derivedReceiptPath", ""))) == source.get("derivedReceiptSha256"), "current source derivation receipt hash differs from manifest")
    prior = _read_json(Path(str(derivation.get("priorRunManifestPath", ""))))
    prior_receipt_path = Path(str(derivation.get("priorDerivedReceiptPath", "")))
    prior_receipt = _read_json(prior_receipt_path)
    prior_root = Path(str(derivation.get("priorRunManifestPath", ""))).resolve().parent
    _require(prior.get("status") == "passed", "source-equivalence base is not a passing P9-11 package")
    _require(prior_receipt.get("status") == "verified-script-only-derivation", "source-equivalence base receipt is not validated")
    _require(derivation.get("source", {}).get("productSourceSha256") == prior_receipt.get("source", {}).get("productSourceSha256"), "product-source equivalence differs from the accepted P9-11 receipt")
    _require(derivation.get("source", {}).get("currentSourceSha256") == current_sha, "derived receipt does not bind the current source inventory")
    _require(set(derivation.get("source", {}).get("addedP912Scripts", [])) == EXPECTED_NEW_SCRIPT_PATHS, "source delta contains unexpected or missing P9-12 harness scripts")
    _require(derivation.get("source", {}).get("productSourceEquivalent") is True and derivation.get("source", {}).get("freshExactTreeRebuild") is False, "receipt overstates build provenance")
    _require(_file_sha256(Path(str(derivation["priorRunManifestPath"]))) == derivation.get("priorRunManifestSha256"), "P9-11 base manifest hash differs from the derived receipt")
    _require(_file_sha256(prior_receipt_path) == derivation.get("priorDerivedReceiptSha256"), "P9-11 source derivation hash differs from the derived receipt")

    prior_inventory = _read_json(prior_root / str(prior["source"]["inventoryPath"]))
    prior_rows = prior_inventory.get("files")
    _require(isinstance(prior_rows, list) and inventory_fingerprint(prior_rows) == prior["source"]["sha256"], "P9-11 accepted source inventory is invalid")
    current_by_path = {str(row["path"]): row for row in rows}
    prior_by_path = {str(row["path"]): row for row in prior_rows}
    _require(set(current_by_path) - set(prior_by_path) == EXPECTED_NEW_SCRIPT_PATHS, "current source has an unexpected added or missing file")
    _require(not (set(prior_by_path) - set(current_by_path)), "current source is missing a P9-11 measured source file")
    for path, old_row in prior_by_path.items():
        _require(current_by_path[path] == old_row, f"a previously measured source file changed after the P9-11 receipt: {path}")

    allowed = set(prior_receipt["source"]["allowedPaths"]) | EXPECTED_NEW_SCRIPT_PATHS
    product_rows = [row for row in rows if str(row["path"]) not in allowed]
    _require(inventory_fingerprint(product_rows) == prior_receipt["source"]["productSourceSha256"], "current product-source inventory is not equivalent to the accepted P9-11 source")

    release_receipt = manifest.get("releaseBuildReceipt") or {}
    release_receipt_path = Path(str(release_receipt.get("path", "")))
    _require(release_receipt_path.is_file() and _file_sha256(release_receipt_path) == release_receipt.get("sha256"), "original P9-11 Release build receipt is missing or changed")
    release = _read_json(release_receipt_path)
    _require(release.get("status") == "succeeded" and release.get("configuration") == "Release" and release.get("platform") == "x64" and int(release.get("build", {}).get("exitCode", -1)) == 0, "bound P9-11 Release receipt is not a successful x64 Release build")
    _require(release.get("releaseExecutableSha256") == manifest.get("application", {}).get("sha256"), "preserved build executable hash differs from the packaged app")
    generated_resources_sha = str((release.get("generatedResources") or {}).get("stagedResourceInputs", {}).get("sha256", ""))
    _require(generated_resources_sha == (manifest.get("package") or {}).get("priorResourceSha256"), "staged resource inventory differs from the original Release receipt")
    build = manifest.get("buildProvenance") or {}
    cache_path = Path(str(build.get("cachePath", "")))
    _require(cache_path.is_file() and _file_sha256(cache_path) == build.get("cacheSha256Current") == build.get("cacheSha256After"), "current standard Release CMake cache differs from the P9-11 build identity")
    _require(build.get("configuration") == "Release" and build.get("platform") == "x64" and build.get("freshExactTreeRebuild") is False, "build provenance overstates or misidentifies the reused Release package")


def _validate_package(root: Path, manifest: dict[str, Any]) -> None:
    package = manifest.get("package") or {}
    inventory = _read_json(root / str(package.get("inventoryPath", "")))
    rows = inventory.get("files")
    _require(isinstance(rows, list) and rows, "package inventory is empty")
    _require(inventory_fingerprint(rows) == package.get("sha256"), "package inventory fingerprint differs from manifest")
    _require(_file_sha256(root / str(package.get("inventoryPath", ""))) == package.get("inventoryFileSha256"), "package inventory file hash differs")
    package_root = Path(str(package.get("root", "")))
    _require(package_root.is_dir(), "verified packaged Release root is missing")
    actual_rows = []
    for file_path in package_root.rglob("*"):
        if file_path.is_file():
            actual_rows.append({"path": file_path.relative_to(package_root).as_posix(), "sizeBytes": file_path.stat().st_size, "sha256": _file_sha256(file_path)})
    _require(len(actual_rows) == int(package.get("fileCount", -1)) and inventory_fingerprint(actual_rows) == package.get("sha256"), "actual package root contains an unlisted, missing, or changed file")
    for row in rows:
        file_path = package_root / str(row["path"])
        _require(file_path.is_file(), f"packaged file missing: {row['path']}")
        _require(file_path.stat().st_size == int(row["sizeBytes"]), f"packaged file size differs: {row['path']}")
        _require(_file_sha256(file_path) == str(row["sha256"]).lower(), f"packaged file SHA differs: {row['path']}")
    app = manifest.get("application") or {}
    executable = Path(str(app.get("path", "")))
    _require(executable.is_file() and _file_sha256(executable) == app.get("sha256"), "packaged ClassMngrNext.exe identity mismatch")
    _require(app.get("architecture") == "x64", "packaged application is not x64")
    _require(executable.resolve() == (package_root / "ClassMngrNext.exe").resolve(), "measured executable is not the bound package-root executable")
    _require(package.get("sha256") == package.get("priorPackageSha256") and package.get("fileCount") == package.get("priorPackageFileCount"), "reused package differs from the passing P9-11 inventory")
    _require(package.get("reconstructedFromRecordedInventory") is True, "package reconstruction provenance is not explicit")
    source_package_root = Path(str(package.get("originalPackagePath", "")))
    unlisted = package.get("originalPackageUnlistedOutput") or {}
    unlisted_path = source_package_root / str(unlisted.get("relativePath", ""))
    _require(unlisted_path.is_file(), "preserved P9-11 package-root output is missing")
    _require(unlisted_path.stat().st_size == int(unlisted.get("sizeBytes", -1)) and _file_sha256(unlisted_path) == unlisted.get("sha256"), "preserved P9-11 package-root output identity differs")
    resource_inventory = _read_json(root / str(package.get("resourceInventoryPath", "")))
    resource_rows = resource_inventory.get("files")
    _require(isinstance(resource_rows, list) and inventory_fingerprint(resource_rows) == package.get("resourceInventorySha256"), "staged resource inventory fingerprint differs from manifest")
    _require(_file_sha256(root / str(package.get("resourceInventoryPath", ""))) == package.get("resourceInventoryFileSha256"), "staged resource inventory file hash differs")
    resource_root = Path(str(package.get("resourceRoot", "")))
    _require(resource_root.is_dir(), "verified staged resource tree is missing")
    _require(package.get("resourceInventorySha256") == package.get("priorResourceSha256"), "staged resource inventory differs from the original Release receipt")
    for row in resource_rows:
        file_path = resource_root / str(row["path"])
        _require(file_path.is_file(), f"staged resource is missing: {row['path']}")
        _require(file_path.stat().st_size == int(row["sizeBytes"]) and _file_sha256(file_path) == str(row["sha256"]).lower(), f"staged resource identity differs: {row['path']}")
    actual_resource_rows = []
    for file_path in resource_root.rglob("*"):
        if file_path.is_file():
            actual_resource_rows.append({"path": file_path.relative_to(resource_root).as_posix(), "sizeBytes": file_path.stat().st_size, "sha256": _file_sha256(file_path)})
    _require(len(actual_resource_rows) == len(resource_rows) and inventory_fingerprint(actual_resource_rows) == package.get("resourceInventorySha256"), "actual staged resource tree contains an unlisted, missing, or changed file")


def _validate_fixture(root: Path, manifest: dict[str, Any]) -> None:
    fixture = manifest.get("fixture") or {}
    pdf_path = Path(str(fixture.get("packagedPdfPath", "")))
    _require(pdf_path.is_file(), "selected PDF body is absent from the packaged resource tree")
    package_root = Path(str((manifest.get("package") or {}).get("root", "")))
    _require(pdf_path.resolve() == (package_root / EXPECTED_PDF_RELATIVE_PATH).resolve(), "selected PDF is outside the byte-bound package root")
    _require(pdf_path.stat().st_size == EXPECTED_PDF_SIZE and _file_sha256(pdf_path) == EXPECTED_PDF_SHA256, "selected PDF bytes differ from the complete known fixture")
    _require(fixture.get("documentId") == EXPECTED_DOCUMENT_ID and fixture.get("displayName") == EXPECTED_DOCUMENT_NAME, "catalog selection identity differs from the known typed document")
    _require(fixture.get("resourceRelativePath") == EXPECTED_PDF_RELATIVE_PATH, "catalog document path differs from the PDF fixture path")
    _require(int(fixture.get("pdfBytes", -1)) == EXPECTED_PDF_SIZE and fixture.get("pdfSha256") == EXPECTED_PDF_SHA256, "manifest fixture identity does not bind the complete PDF bytes")
    _require(fixture.get("packageCatalogSha256") == fixture.get("sourceCatalogSha256"), "packaged catalog bytes differ from the current source catalog")
    workspace = manifest.get("workspace") or {}
    workspace_path = Path(str(workspace.get("copyPath", "")))
    workspace_source = Path(str(workspace.get("sourcePath", "")))
    _require(workspace_path.is_file() and workspace_source.is_file(), "large viewer workspace fixture or copied workspace is missing")
    _require(workspace_path.stat().st_size == workspace_source.stat().st_size == int(workspace.get("sourceSizeBytes", -1)), "workspace fixture size changed")
    _require(_file_sha256(workspace_path) == _file_sha256(workspace_source) == workspace.get("sourceSha256") == workspace.get("copySha256"), "workspace fixture copy changed the input bytes")


def _validate_screenshots(root: Path, screenshots: list[dict[str, Any]], process_id: int, window_handle: int) -> None:
    _require(len(screenshots) == 5, "exactly five lifecycle screenshots are required")
    kinds = {str(row.get("checkpoint")) for row in screenshots}
    _require({"firstOpen", "closed", "reopen", "suspended", "postSuspendReopen"} <= kinds, "lifecycle screenshot set is incomplete")
    _require(len(kinds) == 5, "lifecycle screenshots contain duplicate checkpoints")
    for row in screenshots:
        _require(int(row.get("processId", 0)) == process_id and int(row.get("windowProcessId", 0)) == process_id, f"screenshot {row.get('checkpoint')} is not bound to the measured process")
        _require(int(row.get("windowHandle", 0)) == window_handle, f"screenshot {row.get('checkpoint')} used a different main-window HWND")
        path = root / str(row.get("relativePath", ""))
        _require(path.is_file(), f"screenshot is missing: {path}")
        data = path.read_bytes()
        _require(data.startswith(b"\x89PNG\r\n\x1a\n"), f"screenshot is not PNG: {path}")
        _require(_sha256(data) == row.get("sha256"), f"screenshot hash differs: {path}")
        width, height = struct.unpack(">II", data[16:24])
        _require(width == int(row.get("width", 0)) and height == int(row.get("height", 0)) and width > 0 and height > 0, f"screenshot dimensions are invalid: {path}")
        stats = row.get("pdfViewportStats") or {}
        if row.get("checkpoint") in {"firstOpen", "reopen", "postSuspendReopen"}:
            _require(int(stats.get("uniqueSampledColors", 0)) >= 8 and int(stats.get("nonWhiteSampledPixels", 0)) >= 1000, f"PDF viewport screenshot lacks visible rendered page content: {path}")
            rect = row.get("pdfViewportRectangle") or {}
            _require(float(rect.get("width", 0)) > 0 and float(rect.get("height", 0)) > 0, f"rendered screenshot has no measured PDF viewport rectangle: {path}")


def _validate_uia_snapshots(root: Path, manifest: dict[str, Any]) -> None:
    process = manifest.get("process") or {}
    pid = int(process.get("processId", 0))
    hwnd = int(process.get("mainWindowHandle", 0))
    _require(pid > 0 and hwnd > 0, "UIA evidence has no measured process or main-window handle")
    references: list[dict[str, Any]] = []
    references.append((manifest.get("catalogReady") or {}).get("uiaEvidence") or {})
    for checkpoint in (manifest.get("checkpoints") or {}).values():
        references.append(checkpoint.get("uiaEvidence") or {})
    _require(len(references) == 6, "catalog and lifecycle UIA snapshots are incomplete")
    for evidence in references:
        path = root / str(evidence.get("relativePath", ""))
        _require(path.is_file(), f"raw UIA snapshot is missing: {path}")
        _require(_file_sha256(path) == evidence.get("sha256"), f"raw UIA snapshot SHA differs: {path}")
        snapshot = _read_json(path)
        _require(int(snapshot.get("processId", 0)) == pid and int(snapshot.get("windowProcessId", 0)) == pid, f"UIA snapshot is not bound to measured PID {pid}")
        _require(int(snapshot.get("windowHandle", 0)) == hwnd, "UIA snapshot HWND differs from the measured main window")
        elements = snapshot.get("elements")
        _require(isinstance(elements, list), "raw UIA snapshot has no element inventory")
        identified = [int(row.get("processId", 0)) for row in elements if row.get("name") or row.get("automationId")]
        _require(all(element_pid == pid for element_pid in identified), "UIA snapshot contains a named/identified element from another process")

    actions = manifest.get("uiaActions") or {}
    required = ("firstOpen", "close", "closedNavigateAway", "reopen", "suspend", "postSuspendReopen")
    _require(all(name in actions for name in required), "one or more required authentic UIA actions are missing")
    for name in required:
        validate_uia_action_binding(actions[name], pid, hwnd, name)
    for name in ("firstOpen", "reopen", "postSuspendReopen"):
        action = actions[name]
        _require(action.get("actionKind") in {"select-catalog-document", "reenter-and-reopen-catalog-document"} and action.get("documentId") == EXPECTED_DOCUMENT_ID and action.get("observedName") == EXPECTED_DOCUMENT_NAME, f"UIA action {name} did not select the typed Lesson Planning catalog entry")
    files = manifest.get("files") or {}
    action_log = root / str(files.get("uiaActions", "uia-actions.jsonl"))
    _require(action_log.is_file() and manifest.get("uiaActionsLogSha256"), "raw UIA action log or SHA is missing")
    _require(_file_sha256(action_log) == manifest.get("uiaActionsLogSha256"), "raw UIA action log SHA differs")
    rows = read_jsonl(action_log)
    _require(len(rows) == len(required), "raw UIA action log count differs from the manifest")
    _require([str(row.get("actionKind")) for row in rows] == [str(actions[name].get("actionKind")) for name in required], "raw UIA action log ordering differs from the manifest")


def validate_uia_action_binding(action: dict[str, Any], pid: int, hwnd: int, name: str) -> None:
    _require(int(action.get("processId", 0)) == pid and int(action.get("windowProcessId", 0)) == pid, f"UIA action {name} is not bound to the measured PID")
    _require(int(action.get("windowHandle", 0)) == hwnd, f"UIA action {name} used a different main-window HWND")
    method = action.get("method")
    _require(method in {"SelectionItemPattern.Select", "InvokePattern.Invoke", "UIA-targeted window click"}, f"UIA action {name} did not use an authentic UIA-targeted action")
    if method == "UIA-targeted window click":
        pointer = action.get("pointerAction") or {}
        target = action.get("target") or {}
        rectangle = target.get("rectangle") or {}
        x = int(pointer.get("screenX", -1))
        y = int(pointer.get("screenY", -1))
        left = float(rectangle.get("x", 0))
        top = float(rectangle.get("y", 0))
        right = left + float(rectangle.get("width", 0))
        bottom = top + float(rectangle.get("height", 0))
        _require(action.get("controlType") == "ControlType.TreeItem", f"UIA action {name} clicked a non-tree UIA element")
        _require(pointer.get("kind") == "window-targeted WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP at same verified UIA target", f"UIA action {name} lacks a recorded targeted mouse action")
        _require(int(pointer.get("targetProcessId", 0)) == pid and int(pointer.get("windowHandle", 0)) == hwnd, f"UIA action {name} pointer action is not bound to the measured process/window")
        _require(target.get("automationId") == action.get("automationId") and int(target.get("processId", 0)) == pid, f"UIA action {name} click target differs from its measured UIA element")
        _require(right > left and bottom > top and left <= x <= right and top <= y <= bottom, f"UIA action {name} click point falls outside its verified UIA element")


def validate_evidence(evidence_dir: Path) -> dict[str, Any]:
    root = evidence_dir.resolve()
    manifest = _read_json(root / "manifest.json")
    _require(manifest.get("schema") == SCHEMA and manifest.get("taskId") == TASK_ID, "P9-12 manifest identity mismatch")
    _require(manifest.get("status") in {"captured-awaiting-validation", "passed"}, "runner did not complete a clean evidence capture")
    _validate_source_equivalence(root, manifest)
    _validate_package(root, manifest)
    _validate_fixture(root, manifest)

    process = manifest.get("process") or {}
    _require(process.get("processId") and process.get("windowProcessId") == process.get("processId"), "measured process/window identity is not bound")
    _require(process.get("mainWindowHandle") and process.get("windowHandleProcessId") == process.get("processId"), "main window HWND is not bound to measured PID")
    exit_evidence = manifest.get("processExit") or {}
    _require(exit_evidence.get("normalExit") is True and exit_evidence.get("exitCode") == 0, "packaged Next process did not exit naturally with code 0")
    _require(exit_evidence.get("forcedTerminationForCleanup") is False and exit_evidence.get("closeMainWindowReturned") is True and exit_evidence.get("waitCompleted") is True, "process cleanup was not a bounded natural close")

    logs = manifest.get("logs") or {}
    stdout = root / str(logs.get("stdout", ""))
    stderr = root / str(logs.get("stderr", ""))
    for stream, path in (("stdout", stdout), ("stderr", stderr)):
        _require(path.is_file() and _file_sha256(path) == logs.get(f"{stream}Sha256"), f"raw process {stream} log is missing or changed")
    records = lifecycle_records_from_logs(stdout, stderr)
    catalog_lines = [line for path in (stdout, stderr) for line in path.read_text(encoding="utf-8-sig", errors="replace").splitlines() if "document-catalog event=projected outcome=completed" in line]
    _require(len(catalog_lines) == 1 and "loaded_pdf_body_count=0" in catalog_lines[0], "startup did not record metadata-only catalog readiness with zero PDF bodies")
    catalog_checkpoint = manifest.get("catalogReady") or {}
    _require(int(catalog_checkpoint.get("loadedPdfBodyCount", -1)) == 0, "catalog-ready UI checkpoint does not report zero loaded PDF bodies")
    _require(int(catalog_checkpoint.get("qpdfEventCount", -1)) == 0, "QtPdf viewer state existed before the first catalog document selection")
    _require(int(catalog_checkpoint.get("viewerPageInstanceCount", -1)) == 0, "viewer PageHost instance existed before the first catalog document selection")
    first_selection = manifest.get("uiaActions", {}).get("firstOpen", {})
    _require(first_selection.get("actionKind") == "select-catalog-document" and first_selection.get("documentId") == EXPECTED_DOCUMENT_ID, "first PDF selection was not bound to the typed catalog document")
    actions = manifest.get("uiaActions") or {}
    _require(actions.get("close", {}).get("actionKind") == "close-pdf", "explicit PDF close control was not invoked")
    _require(actions.get("closedNavigateAway", {}).get("actionKind") == "navigate-away-after-explicit-close", "intermediate catalog deselection route is missing")
    _require(actions.get("suspend", {}).get("actionKind") == "suspend-document-viewer", "active PDF suspension route is missing")
    _require(actions.get("suspend", {}).get("expectedName") == EXPECTED_SUSPENSION_PAGE_NAME and actions.get("suspend", {}).get("targetPageId") == EXPECTED_SUSPENSION_PAGE_ID, "active PDF suspension did not leave through the measured My Workspace PageHost route")
    _require(actions.get("postSuspendReopen", {}).get("actionKind") == "reenter-and-reopen-catalog-document", "post-suspension PDF recreation route is missing")
    intermediate = manifest.get("intermediateNavigation") or {}
    _require(intermediate.get("toPage") == "calendar" and intermediate.get("activePdfStreamAtTransition") is False and "to=calendar" in str(intermediate.get("appEventLine", "")), "closed-document selection reset did not use a measured Calendar route")
    _require(f"to={EXPECTED_SUSPENSION_PAGE_ID}" in str(intermediate.get("activeContentSuspensionAppLine", "")), "active PDF suspension lacks its completed My Workspace page-route event")
    _require(str(actions["firstOpen"].get("issuedAtUtc", "")) < str(actions["close"].get("issuedAtUtc", "")) < str(actions["reopen"].get("issuedAtUtc", "")) < str(actions["suspend"].get("issuedAtUtc", "")) < str(actions["postSuspendReopen"].get("issuedAtUtc", "")), "UIA open/close/suspend/reopen actions are not chronologically ordered")
    first_selection_time = str(first_selection.get("issuedAtUtc", ""))
    qpdf_first_time = min((str(row.get("captured_at", "")) for row in records), default="")
    _require(first_selection_time and qpdf_first_time and qpdf_first_time >= first_selection_time, "QtPdf session existed before the catalog selection action")

    qpdf_stats = validate_qpdf_sequence(records, manifest.get("checkpoints") or {}, manifest.get("pageHostSnapshots") or {})
    _validate_screenshots(root, manifest.get("screenshots") or [], int(process["processId"]), int(process["mainWindowHandle"]))
    _validate_uia_snapshots(root, manifest)

    metrics = read_jsonl(root / str(manifest.get("processMetricsPath", "process-metrics.jsonl")))
    operation_peak, required_memory_points = validate_working_set_metrics(metrics, int(process["processId"]))

    shutdown = manifest.get("shutdownTrace") or {}
    trace_path = root / str(shutdown.get("relativePath", ""))
    _require(trace_path.is_file() and _file_sha256(trace_path) == shutdown.get("sha256"), "shutdown trace is missing or changed")
    trace_lines = [json.loads(line) for line in trace_path.read_text(encoding="utf-8-sig").splitlines() if line.strip()]
    events = [str(row.get("event", "")) for row in trace_lines]
    _require(any(event.endswith("application.exec.returned") or event == "application.exec.returned" for event in events), "shutdown trace did not record return from QApplication::exec")

    return {
        "schema": "classmngr-phase9-packaged-next-pdf-viewer-lifecycle-validation-v1",
        "taskId": TASK_ID,
        "status": "passed",
        "evidenceDirectory": str(root),
        "processId": int(process["processId"]),
        "pdfBytes": EXPECTED_PDF_SIZE,
        "pdfSha256": EXPECTED_PDF_SHA256,
        "documentId": EXPECTED_DOCUMENT_ID,
        "pageCount": int(manifest["checkpoints"]["firstOpen"]["pdfState"]["pageCount"]),
        "qpdfLifecycle": qpdf_stats,
        "naturalExitCode": 0,
        "forcedTerminationForCleanup": False,
        "strictWorkingSetLimitExclusive": WORKING_SET_LIMIT,
        "requiredMemoryCheckpointCount": required_memory_points,
        "maximumReportedWorkingSetPeakBytes": operation_peak,
        "operationPeakAdvisoryLimitExclusive": OPERATION_PEAK_ADVISORY,
        "operationPeakWithinAdvisory": operation_peak < OPERATION_PEAK_ADVISORY,
        "sourceProductEquivalence": manifest["source"]["productSourceSha256"],
        "freshExactTreeRebuild": False,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-dir", required=True)
    args = parser.parse_args(argv)
    root = Path(args.evidence_dir).resolve()
    output_path = root / "logs" / "validator-output.json"
    try:
        result = validate_evidence(root)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except Exception as error:
        failure = {"schema": "classmngr-phase9-packaged-next-pdf-viewer-lifecycle-validation-v1", "taskId": TASK_ID, "status": "failed", "evidenceDirectory": str(root), "error": str(error)}
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(failure, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(failure, ensure_ascii=False, indent=2))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
