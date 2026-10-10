#!/usr/bin/env python3
"""Run isolated packaged PDF lifetime probe processes.

The identity sidecar is prepared and hashed here, before each measured
ClassMngr process starts. The application only validates and records it.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sqlite3
import subprocess
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


PDF_RELATIVE_PATH = "Guides/DYB Lesson Planning Guide.pdf"
PDF_QRC_PATH = f":/resource-packs/documents/{PDF_RELATIVE_PATH}"
PDF_SHA256 = "295ACCAB548B41F0B4F56E6AB89958AD5C3347646904C155B16A56EBE926CAEC"
DOCUMENTS_RCC_SHA256 = "A3EB570294B55A616EA05797222FC1FEDD93620ABB25360B724ED6843BEE924F"
ARMS = ("document-only", "viewer-no-grab", "viewer-grab")
REQUIRED_SAMPLE_NAMES = (
    "pdf-lifetime-loaded-ready",
    "pdf-lifetime-after-close",
    "pdf-lifetime-after-close-1s",
    "pdf-lifetime-after-close-5s",
)
# The delayed Qt timers can be a little early or late under GUI scheduling.
# These windows allow 20% early / 1s late at +1s and 500ms early / 1s late
# at +5s, while keeping the two labels well separated.
ONE_SECOND_SAMPLE_WINDOW_MS = (800, 2000)
FIVE_SECOND_SAMPLE_WINDOW_MS = (4500, 6000)
SOURCE_IDENTITY_FILES = (
    "src/main.cpp",
    "src/app/startup_database_path.cpp",
    "src/core/startup_profiler.cpp",
    "src/core/resource_packs/resource_pack_manager.cpp",
    "src/ui/shared/pages/pdf_viewer_page.cpp",
    "src/ui/shared/pages/pdf_viewer_page_navigation.cpp",
    "src/ui/shared/pages/pdf_viewer_page_ui.cpp",
    "scripts/phase0/run_pdf_lifetime_probe.py",
)


class ProbeError(RuntimeError):
    pass


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def canonical_path(path: Path) -> str:
    return str(path.resolve(strict=True))


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = path.with_suffix(path.suffix + ".tmp")
    temporary_path.write_text(
        json.dumps(value, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    temporary_path.replace(path)


def read_only_class_count(fixture_path: Path) -> int:
    if fixture_path.suffix.lower() != ".tps":
        raise ProbeError("The 96-class probe fixture must be a .tps workspace file.")
    uri = fixture_path.resolve().as_uri() + "?mode=ro"
    connection = sqlite3.connect(uri, uri=True)
    try:
        integrity = connection.execute("PRAGMA integrity_check").fetchone()
        if not integrity or integrity[0] != "ok":
            raise ProbeError(f"Fixture integrity check failed: {fixture_path}")
        count = connection.execute("SELECT COUNT(*) FROM classes").fetchone()
        if not count:
            raise ProbeError(f"Fixture has no classes table: {fixture_path}")
        return int(count[0])
    except sqlite3.Error as error:
        raise ProbeError(f"Unable to validate the SQLite fixture: {error}") from error
    finally:
        connection.close()


def find_documents_rcc(application_path: Path) -> Path:
    application_directory = application_path.parent
    candidates = (
        application_directory / "resources" / "resource-packs" / "documents.rcc",
        application_directory / "resource-packs" / "documents.rcc",
        application_directory.parent / "Resources" / "resource-packs" / "documents.rcc",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise ProbeError(
        "Could not resolve the packaged documents.rcc beside the application. "
        "Pass an application from a built or installed package."
    )


def git_output(repository_root: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=repository_root,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        return "unknown"
    return result.stdout.strip()


def source_identity(repository_root: Path) -> dict[str, Any]:
    source_files: dict[str, str] = {}
    for relative_path in SOURCE_IDENTITY_FILES:
        source_path = repository_root / relative_path
        if not source_path.is_file():
            raise ProbeError(f"Source identity input is missing: {relative_path}")
        source_files[relative_path] = sha256_file(source_path)

    tracked_changes = git_output(
        repository_root,
        "status",
        "--porcelain",
        "--untracked-files=no",
    ).splitlines()
    return {
        "gitRevision": git_output(repository_root, "rev-parse", "--short", "HEAD"),
        "trackedChanges": tracked_changes,
        "sourceFilesSha256": source_files,
    }


def create_identity(
    repository_root: Path,
    application_path: Path,
    fixture_path: Path,
    documents_rcc_path: Path,
    run_id: str,
    session_id: str,
    arm: str,
    output_path: Path,
) -> dict[str, Any]:
    pdf_source_path = (
        repository_root
        / "resources"
        / "assets"
        / "documents"
        / Path(PDF_RELATIVE_PATH)
    )
    if not pdf_source_path.is_file():
        raise ProbeError(f"Source PDF is missing: {pdf_source_path}")

    pdf_sha256 = sha256_file(pdf_source_path)
    documents_rcc_sha256 = sha256_file(documents_rcc_path)
    if pdf_sha256 != PDF_SHA256:
        raise ProbeError(
            "The source PDF differs from the F536/F539 fixture identity: "
            f"expected {PDF_SHA256}, found {pdf_sha256}."
        )
    if documents_rcc_sha256 != DOCUMENTS_RCC_SHA256:
        raise ProbeError(
            "The packaged documents.rcc differs from the F536/F539 identity: "
            f"expected {DOCUMENTS_RCC_SHA256}, found {documents_rcc_sha256}."
        )

    class_count = read_only_class_count(fixture_path)
    if class_count != 96:
        raise ProbeError(
            f"Expected the existing 96-class fixture; found {class_count} classes."
        )

    return {
        "schema": "classmngr-pdf-lifetime-probe-identity-v1",
        "sessionId": session_id,
        "runId": run_id,
        "arm": arm,
        "preparedAtUtc": datetime.now(timezone.utc).isoformat(),
        "application": {
            "path": canonical_path(application_path),
            "sha256": sha256_file(application_path),
        },
        "source": source_identity(repository_root),
        "fixture": {
            "path": canonical_path(fixture_path),
            "sha256": sha256_file(fixture_path),
            "classCount": class_count,
        },
        "resourcePack": {
            "id": "documents",
            "version": "1.0.0",
            "path": canonical_path(documents_rcc_path),
            "sha256": documents_rcc_sha256,
        },
        "pdf": {
            "catalogId": "document_guides_lesson_planning",
            "relativePath": PDF_RELATIVE_PATH,
            "resolvedPath": PDF_QRC_PATH,
            "sha256": pdf_sha256,
        },
        "controls": {
            "arm": arm,
            "forcedGrab": arm == "viewer-grab",
            "pngFileWritingEnabled": False,
        },
        "outputPath": str(output_path.resolve(strict=False)),
    }


def process_environment(run_directory: Path) -> dict[str, str]:
    environment = os.environ.copy()
    for variable in (
        "QT_QPA_PLATFORM",
        "QT_PLUGIN_PATH",
        "QT_QPA_PLATFORM_PLUGIN_PATH",
        "QML2_IMPORT_PATH",
        "QML_IMPORT_PATH",
        "CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR",
        "CLASSMNGR_STARTUP_WORKFLOW_TRACE_PATH",
    ):
        environment.pop(variable, None)

    settings_root = run_directory / "settings"
    app_data_root = run_directory / "app-data"
    environment["CLASSMNGR_SETTINGS_ROOT"] = str(settings_root)
    environment["APPDATA"] = str(app_data_root / "roaming")
    environment["LOCALAPPDATA"] = str(app_data_root / "local")
    environment["XDG_DATA_HOME"] = str(app_data_root / "xdg")
    return environment


def close_relative_elapsed_ms(sample: dict[str, Any], trace_path: Path) -> int:
    operation = sample.get("operation")
    value = operation.get("closeElapsedMs") if isinstance(operation, dict) else None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ProbeError(
            f"Process sample '{sample.get('name')}' has no close-relative timestamp: {trace_path}"
        )
    if not float(value).is_integer():
        raise ProbeError(
            f"Process sample '{sample.get('name')}' has a fractional close-relative timestamp: {trace_path}"
        )
    return int(value)


def validate_trace(
    trace_path: Path,
    identity: dict[str, Any],
    arm: str,
) -> dict[str, Any]:
    if not trace_path.is_file():
        raise ProbeError(f"Application did not write the probe trace: {trace_path}")
    trace = json.loads(trace_path.read_text(encoding="utf-8"))
    if trace.get("schema") != "classmngr-pdf-lifetime-probe-v1":
        raise ProbeError(f"Unexpected probe trace schema: {trace_path}")
    if trace.get("status") != "completed":
        raise ProbeError(
            f"Application marked the probe run failed: {trace.get('error') or trace_path}"
        )
    if trace.get("identity") != identity:
        raise ProbeError(f"Trace identity does not match its prepared sidecar: {trace_path}")
    if trace.get("arm") != arm or trace.get("runId") != identity["runId"]:
        raise ProbeError(f"Trace arm/run identity mismatch: {trace_path}")
    process = trace.get("process", {})
    if os.path.normcase(os.path.realpath(process.get("applicationPath", ""))) != os.path.normcase(
        identity["application"]["path"]
    ):
        raise ProbeError(f"Measured executable path differs from the prepared identity: {trace_path}")
    if process.get("gitRevision") != identity["source"].get("gitRevision"):
        raise ProbeError(f"Measured binary source revision differs from the sidecar: {trace_path}")

    samples = trace.get("memorySamples")
    if not isinstance(samples, list):
        raise ProbeError(f"Trace has no process memory sample list: {trace_path}")
    by_name: dict[str, dict[str, Any]] = {}
    for sample in samples:
        name = sample.get("name")
        if name in by_name:
            raise ProbeError(f"Duplicate process sample '{name}' in {trace_path}")
        by_name[name] = sample
    for name in REQUIRED_SAMPLE_NAMES:
        if name not in by_name:
            raise ProbeError(f"Required process sample '{name}' is missing from {trace_path}")
        if by_name[name].get("available") is not True:
            raise ProbeError(f"Process memory is unavailable at '{name}' in {trace_path}")

    elapsed = [int(by_name[name].get("elapsedMs", -1)) for name in REQUIRED_SAMPLE_NAMES]
    if any(value < 0 for value in elapsed) or any(
        current <= previous for previous, current in zip(elapsed, elapsed[1:])
    ):
        raise ProbeError(
            f"Process sample times are missing, duplicated, or not strictly ordered: {trace_path}"
        )

    close_elapsed = close_relative_elapsed_ms(by_name[REQUIRED_SAMPLE_NAMES[1]], trace_path)
    one_second_elapsed = close_relative_elapsed_ms(by_name[REQUIRED_SAMPLE_NAMES[2]], trace_path)
    five_second_elapsed = close_relative_elapsed_ms(by_name[REQUIRED_SAMPLE_NAMES[3]], trace_path)
    if close_elapsed != 0:
        raise ProbeError(f"The immediate post-close sample has a nonzero close-relative time: {trace_path}")
    if not ONE_SECOND_SAMPLE_WINDOW_MS[0] <= one_second_elapsed <= ONE_SECOND_SAMPLE_WINDOW_MS[1]:
        raise ProbeError(
            "The 1s post-close sample was outside its timing window "
            f"[{ONE_SECOND_SAMPLE_WINDOW_MS[0]}, {ONE_SECOND_SAMPLE_WINDOW_MS[1]}] ms: {trace_path}"
        )
    if not FIVE_SECOND_SAMPLE_WINDOW_MS[0] <= five_second_elapsed <= FIVE_SECOND_SAMPLE_WINDOW_MS[1]:
        raise ProbeError(
            "The 5s post-close sample was outside its timing window "
            f"[{FIVE_SECOND_SAMPLE_WINDOW_MS[0]}, {FIVE_SECOND_SAMPLE_WINDOW_MS[1]}] ms: {trace_path}"
        )

    one_second_process_delta = elapsed[2] - elapsed[1]
    five_second_process_delta = elapsed[3] - elapsed[1]
    if (
        not ONE_SECOND_SAMPLE_WINDOW_MS[0] <= one_second_process_delta <= ONE_SECOND_SAMPLE_WINDOW_MS[1]
        or not FIVE_SECOND_SAMPLE_WINDOW_MS[0] <= five_second_process_delta <= FIVE_SECOND_SAMPLE_WINDOW_MS[1]
    ):
        raise ProbeError(
            f"Process sample timestamps do not match their close-relative labels: {trace_path}"
        )

    controls = trace.get("controls", {})
    pdf = trace.get("pdf", {})
    if (
        pdf.get("pageCount") != 38
        or pdf.get("relativePath") != PDF_RELATIVE_PATH
        or pdf.get("resolvedPath") != PDF_QRC_PATH
    ):
        raise ProbeError(f"PDF fixture identity differs from the expected 38-page guide: {trace_path}")
    if controls.get("pngFileWritingEnabled") is not False:
        raise ProbeError(f"PNG file writing was not disabled: {trace_path}")
    if bool(controls.get("forcedGrabInvoked")) != (arm == "viewer-grab"):
        raise ProbeError(f"Forced grab does not match arm '{arm}': {trace_path}")
    if arm == "document-only" and (
        controls.get("pdfViewCreated") is not False
        or controls.get("viewerVisible") is not False
    ):
        raise ProbeError(f"Document-only arm created or showed a PDF view: {trace_path}")
    grab_samples = {
        "pdf-lifetime-grab-start",
        "pdf-lifetime-grab-complete",
    }
    if arm == "viewer-grab" and not grab_samples.issubset(by_name):
        raise ProbeError(f"Viewer-grab arm missed the existing forced grab path: {trace_path}")
    if arm != "viewer-grab" and grab_samples.intersection(by_name):
        raise ProbeError(f"Forced grab occurred outside the viewer-grab arm: {trace_path}")
    if arm != "document-only" and (
        controls.get("pdfViewCreated") is not True
        or controls.get("viewerVisible") is not True
        or controls.get("pageMode") != "MultiPage"
        or int(controls.get("viewWidth", 0)) <= 0
        or int(controls.get("viewHeight", 0)) <= 0
    ):
        raise ProbeError(f"Viewer arm was not visible in MultiPage mode: {trace_path}")

    return trace


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    repository_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(
        description=(
            "Run fresh-process PDF lifetime probe arms against one packaged "
            "application and the existing 96-class fixture."
        )
    )
    parser.add_argument("--application", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=repository_root)
    parser.add_argument("--documents-rcc", type=Path)
    parser.add_argument("--session-id")
    parser.add_argument("--arms", nargs="+", choices=ARMS, default=list(ARMS))
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--timeout-seconds", type=int, default=120)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    arguments = parse_arguments(argv)
    repository_root = arguments.source_root.resolve(strict=True)
    application_path = arguments.application.resolve(strict=True)
    fixture_path = arguments.fixture.resolve(strict=True)
    output_root = arguments.output_root.resolve(strict=False)
    documents_rcc_path = (
        arguments.documents_rcc.resolve(strict=True)
        if arguments.documents_rcc
        else find_documents_rcc(application_path).resolve(strict=True)
    )

    if not application_path.is_file():
        raise ProbeError(f"Application is not a file: {application_path}")
    if not fixture_path.is_file():
        raise ProbeError(f"Fixture is not a file: {fixture_path}")
    if arguments.repeats < 1:
        raise ProbeError("--repeats must be at least 1.")
    if arguments.timeout_seconds < 1:
        raise ProbeError("--timeout-seconds must be at least 1.")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,79}", arguments.session_id or "probe"):
        raise ProbeError("--session-id must contain only letters, digits, dot, underscore, or hyphen.")

    if output_root.exists():
        if not output_root.is_dir() or any(output_root.iterdir()):
            raise ProbeError(f"Output root must be new or empty: {output_root}")
    else:
        output_root.mkdir(parents=True)

    session_id = arguments.session_id or (
        datetime.now(timezone.utc).strftime("pdfprobe-%Y%m%dT%H%M%SZ")
        + "-"
        + uuid.uuid4().hex[:8]
    )
    documents_rcc_baseline = canonical_path(documents_rcc_path)
    application_baseline = canonical_path(application_path)
    fixture_baseline = canonical_path(fixture_path)
    fixture_sha256_baseline = sha256_file(fixture_path)
    application_sha256_baseline = sha256_file(application_path)
    documents_rcc_sha256_baseline = sha256_file(documents_rcc_path)
    source_baseline = source_identity(repository_root)

    manifest: dict[str, Any] = {
        "schema": "classmngr-pdf-lifetime-probe-run-v1",
        "sessionId": session_id,
        "status": "running",
        "startedAtUtc": datetime.now(timezone.utc).isoformat(),
        "applicationPath": application_baseline,
        "fixturePath": fixture_baseline,
        "documentsRccPath": documents_rcc_baseline,
        "arms": list(arguments.arms),
        "repeats": arguments.repeats,
        "runs": [],
        "error": None,
    }
    manifest_path = output_root / "probe-manifest.json"
    write_json(manifest_path, manifest)

    traces: list[dict[str, Any]] = []
    seen_pids: set[int] = set()
    try:
        for repetition in range(1, arguments.repeats + 1):
            for arm in arguments.arms:
                run_id = f"{session_id}-{arm}-r{repetition:02d}"
                run_directory = output_root / run_id
                run_directory.mkdir()
                sidecar_path = run_directory / "identity.json"
                trace_path = run_directory / "trace.json"
                stdout_path = run_directory / "stdout.log"
                stderr_path = run_directory / "stderr.log"

                identity = create_identity(
                    repository_root,
                    application_path,
                    fixture_path,
                    documents_rcc_path,
                    run_id,
                    session_id,
                    arm,
                    trace_path,
                )
                if (
                    identity["application"]["path"] != application_baseline
                    or identity["application"]["sha256"] != application_sha256_baseline
                    or identity["fixture"]["path"] != fixture_baseline
                    or identity["fixture"]["sha256"] != fixture_sha256_baseline
                    or identity["resourcePack"]["path"] != documents_rcc_baseline
                    or identity["resourcePack"]["sha256"] != documents_rcc_sha256_baseline
                    or identity["source"] != source_baseline
                ):
                    raise ProbeError("Application, fixture, resource pack, or source changed during the run.")
                write_json(sidecar_path, identity)

                command = [
                    str(application_path),
                    "--startup-performance-pdf-lifetime-probe",
                    arm,
                    "--startup-performance-pdf-lifetime-run-id",
                    run_id,
                    "--startup-performance-pdf-lifetime-sidecar",
                    str(sidecar_path),
                    "--startup-performance-pdf-lifetime-output",
                    str(trace_path),
                    "--startup-performance-scenario",
                    "representative",
                    "--startup-performance-settle-ms",
                    "0",
                    str(fixture_path),
                ]
                write_json(
                    run_directory / "invocation.json",
                    {
                        "command": command,
                        "workingDirectory": str(repository_root),
                        "sidecarPath": str(sidecar_path),
                        "tracePath": str(trace_path),
                    },
                )
                manifest["runs"].append(
                    {
                        "runId": run_id,
                        "arm": arm,
                        "repetition": repetition,
                        "sidecar": str(sidecar_path),
                        "trace": str(trace_path),
                        "status": "running",
                    }
                )
                write_json(manifest_path, manifest)

                try:
                    result = subprocess.run(
                        command,
                        cwd=repository_root,
                        env=process_environment(run_directory),
                        check=False,
                        capture_output=True,
                        text=True,
                        encoding="utf-8",
                        errors="replace",
                        timeout=arguments.timeout_seconds,
                    )
                except subprocess.TimeoutExpired as error:
                    stdout_path.write_text(str(error.stdout or ""), encoding="utf-8")
                    stderr_path.write_text(str(error.stderr or ""), encoding="utf-8")
                    raise ProbeError(f"Probe process timed out for {run_id}.") from error

                stdout_path.write_text(result.stdout, encoding="utf-8")
                stderr_path.write_text(result.stderr, encoding="utf-8")
                if result.returncode != 0:
                    raise ProbeError(
                        f"Probe process {run_id} exited with {result.returncode}; "
                        f"see {stderr_path} and {stdout_path}."
                    )

                trace = validate_trace(trace_path, identity, arm)
                process_id = int(trace.get("process", {}).get("processId", 0))
                if process_id <= 0 or process_id in seen_pids:
                    raise ProbeError(f"Measured process identity was reused or missing: {run_id}")
                seen_pids.add(process_id)
                traces.append(trace)
                manifest["runs"][-1].update(
                    {
                        "status": "completed",
                        "processId": process_id,
                        "qtVersion": trace.get("process", {}).get("qtVersion"),
                        "screen": trace.get("display", {}),
                    }
                )
                write_json(manifest_path, manifest)

        first_trace = traces[0]
        reference_process = first_trace.get("process", {})
        reference_display = first_trace.get("display", {})
        reference_view_size = None
        for trace in traces:
            controls = trace.get("controls", {})
            if not controls.get("pdfViewCreated"):
                continue
            view_size = (controls.get("viewWidth"), controls.get("viewHeight"))
            if reference_view_size is None:
                reference_view_size = view_size
            elif view_size != reference_view_size:
                raise ProbeError("Visible PDF view size changed between viewer arms.")
        for trace in traces[1:]:
            process = trace.get("process", {})
            for identity_field in ("version", "gitRevision", "buildTimestamp", "qtVersion"):
                if process.get(identity_field) != reference_process.get(identity_field):
                    raise ProbeError(
                        f"Measured binary identity field '{identity_field}' changed between processes."
                    )
            if trace.get("display") != reference_display:
                raise ProbeError("Display geometry or scale changed between measured processes.")

        if any(output_root.rglob("*.png")):
            raise ProbeError("A probe run wrote PNG files despite capture being disabled.")

        manifest["status"] = "completed"
        manifest["completedAtUtc"] = datetime.now(timezone.utc).isoformat()
        manifest["processIds"] = sorted(seen_pids)
        manifest["qtVersion"] = reference_process.get("qtVersion")
        manifest["display"] = reference_display
        manifest["expectedPdfSha256"] = PDF_SHA256
        manifest["expectedDocumentsRccSha256"] = DOCUMENTS_RCC_SHA256
        write_json(manifest_path, manifest)
        print(f"PDF lifetime probe completed: {manifest_path}")
        return 0
    except Exception as error:
        manifest["status"] = "failed"
        manifest["error"] = str(error)
        manifest["failedAtUtc"] = datetime.now(timezone.utc).isoformat()
        write_json(manifest_path, manifest)
        raise


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except ProbeError as error:
        print(f"PDF lifetime probe failed: {error}", file=sys.stderr)
        raise SystemExit(2)
