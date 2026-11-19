#!/usr/bin/env python3
"""Fail-closed validator for packaged Next Calendar Import evidence (P9-10)."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import zipfile
from collections import Counter
from datetime import date, timedelta, datetime
from pathlib import Path, PurePosixPath
from typing import Any
from xml.etree import ElementTree as ET


MANIFEST_SCHEMA = "classmngr-phase9-packaged-next-calendar-import-v1"
SUMMARY_SCHEMA = "classmngr-phase9-packaged-next-calendar-import-summary-v1"
SNAPSHOT_SCHEMA = "classmngr-phase9-calendar-import-workspace-snapshot-v1"
WORKING_SET_LIMIT = 262_144_000
OPERATION_PEAK_LIMIT = 536_870_912
SUCCESS_FIXTURE_NAME = "generated-large-calendar-import.xlsx"
SUCCESS_FIXTURE_SHA256 = "25a5bee84b02f97d6ec72f5c21ba0909a5e6f68edc1e10607ade75b2435c73fb"
ERROR_FIXTURE_NAME = "malformed-calendar-import-response.bin"
ERROR_FIXTURE_SHA256 = "35cda6bf051eaf8f5d36a39b6a632a3af34b8b697316db6192aa70ca2b1eabaa"
MONTHS = (
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
)
EXPECTED_SUCCESS_SOURCE = (
    "docs/qt-rewrite/visual-baseline/release/large-calendar-import-boundary/"
    "generated-large-calendar-import.xlsx"
)
EXPECTED_ERROR_SOURCE = (
    "docs/qt-rewrite/visual-baseline/release/large-calendar-import-error-boundary/"
    "malformed-calendar-import-response.bin"
)
APPLICATION_SHUTDOWN_MILESTONES = (
    "application.exec.enter",
    "application_shell.close_event.enter",
    "application_shell.close_handler.started",
    "application_bootstrap.request_window_close.enter",
    "application_bootstrap.request_window_close.coordinator_request_started",
    "application_bootstrap.shutdown_sequence.started",
    "application_bootstrap.shutdown_sequence.settings_flush_completed",
    "application_bootstrap.shutdown_sequence.completed",
    "application_bootstrap.request_window_close.coordinator_request_completed",
    "application_bootstrap.finalization.scheduled",
    "application_bootstrap.request_window_close.accepted",
    "application_shell.close_handler.returned",
    "application_shell.close_event.acceptance",
    "application_bootstrap.finalization.callback_started",
    "application_bootstrap.finalization.shell_release_started",
    "application_bootstrap.finalization.shell_release_completed",
    "application_bootstrap.finalization.theme_release_started",
    "application_bootstrap.finalization.theme_release_completed",
    "application_bootstrap.finalization.page_lifecycle_diagnostics_release_started",
    "application_bootstrap.finalization.page_lifecycle_diagnostics_release_completed",
    "application_bootstrap.finalization.workspace_release_started",
    "application_bootstrap.finalization.workspace_release_completed",
    "application_bootstrap.finalization.core_services_release_started",
    "application_bootstrap.finalization.core_services_release_completed",
    "application_bootstrap.finalization.startup_diagnostics_release_started",
    "application_bootstrap.finalization.startup_diagnostics_release_completed",
    "application_bootstrap.finalization.application_quit_started",
    "application.about_to_quit",
    "application_bootstrap.finalization.application_quit_returned",
    "application.exec.returned",
    "application_bootstrap.shutdown.after_exec",
)
APPLICATION_FINALIZATION_STARTED = "application_bootstrap.finalization.started"
POST_MEASUREMENT_VALIDATOR_REVISION_PATHS = {
    "scripts/phase9/validate_packaged_next_calendar_import.py",
    "scripts/phase9/test_validate_packaged_next_calendar_import.py",
}


class EvidenceError(RuntimeError):
    """Raised when an evidence record is incomplete or internally inconsistent."""


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise EvidenceError(message)


def _read_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise EvidenceError(f"could not read JSON {path}: {exc}") from exc


def _read_jsonl(path: Path) -> list[dict[str, Any]]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise EvidenceError(f"could not read JSONL {path}: {exc}") from exc
    result: list[dict[str, Any]] = []
    for line_number, line in enumerate(lines, 1):
        if not line.strip():
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError as exc:
            raise EvidenceError(f"invalid JSONL at {path}:{line_number}: {exc}") from exc
        _require(isinstance(value, dict), f"JSONL row at {path}:{line_number} is not an object")
        result.append(value)
    return result


def _validate_ordered_shutdown_milestones(events: list[str], route_id: str) -> None:
    positions: dict[str, int] = {}
    cursor = -1
    for milestone in APPLICATION_SHUTDOWN_MILESTONES:
        matches = [index for index, event in enumerate(events) if event == milestone]
        _require(bool(matches), f"{route_id} application shutdown trace lacks milestone {milestone}")
        _require(len(matches) == 1, f"{route_id} application shutdown trace duplicates milestone {milestone}")
        position = matches[0]
        _require(position > cursor, f"{route_id} application shutdown trace misorders milestone {milestone}")
        positions[milestone] = position
        cursor = position

    start_positions = [index for index, event in enumerate(events) if event == APPLICATION_FINALIZATION_STARTED]
    _require(bool(start_positions), f"{route_id} shutdown trace lacks finalization start")
    callback_position = positions["application_bootstrap.finalization.callback_started"]
    shell_release_position = positions["application_bootstrap.finalization.shell_release_started"]
    _require(
        callback_position < start_positions[0] < shell_release_position,
        f"{route_id} shutdown trace misorders finalization start",
    )
    if len(start_positions) > 1:
        _require(
            len(start_positions) == 2
            and start_positions[1] > positions["application.exec.returned"],
            f"{route_id} shutdown trace duplicates finalization start outside the post-exec idempotency check",
        )


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _canonical(value: Any) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")


def _artifact_path(value: Any, run_directory: Path, label: str, *, must_exist: bool = True) -> Path:
    _require(isinstance(value, str) and value.strip(), f"{label} path is missing")
    path = Path(value).resolve(strict=must_exist)
    if must_exist:
        try:
            path.relative_to(run_directory.resolve(strict=True))
        except ValueError as exc:
            raise EvidenceError(f"{label} escaped the evidence directory") from exc
    return path


def _timestamp(value: Any, label: str) -> datetime:
    _require(isinstance(value, str) and value.strip(), f"{label} timestamp is missing")
    try:
        result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise EvidenceError(f"{label} timestamp is invalid: {value}") from exc
    _require(result.tzinfo is not None, f"{label} timestamp has no timezone")
    return result


def _inventory_hash(rows: list[dict[str, Any]], label: str) -> str:
    paths: list[str] = []
    records: list[str] = []
    for row in rows:
        relative = PurePosixPath(str(row.get("path", "")))
        _require(not relative.is_absolute() and ".." not in relative.parts, f"{label} contains an unsafe relative path")
        path = str(relative)
        size = row.get("sizeBytes")
        digest = str(row.get("sha256", ""))
        _require(isinstance(size, int) and size >= 0, f"{label} has an invalid size for {path}")
        _require(re.fullmatch(r"[0-9a-f]{64}", digest) is not None, f"{label} has an invalid SHA-256 for {path}")
        paths.append(path)
        records.append(f"{path}\t{size}\t{digest}")
    _require(len(paths) == len(set(paths)), f"{label} has duplicate paths")
    _require(paths == sorted(paths, key=str.casefold), f"{label} paths are not casefold-sorted")
    return hashlib.sha256(("\n".join(records) + "\n").encode("utf-8")).hexdigest()


def _verify_inventory_files(rows: list[dict[str, Any]], root: Path, label: str) -> None:
    canonical_root = root.resolve(strict=True)
    for row in rows:
        relative = PurePosixPath(str(row["path"]))
        candidate = canonical_root.joinpath(*relative.parts).resolve(strict=True)
        try:
            candidate.relative_to(canonical_root)
        except ValueError as exc:
            raise EvidenceError(f"{label} inventory path escaped its root: {relative}") from exc
        _require(candidate.is_file(), f"{label} inventory entry is not a file: {relative}")
        _require(candidate.stat().st_size == int(row["sizeBytes"]), f"{label} size mismatch: {relative}")
        _require(_sha256(candidate) == row["sha256"], f"{label} SHA-256 mismatch: {relative}")


def _verify_source_inventory_with_validator_revision(
    rows: list[dict[str, Any]],
    root: Path,
    source_sha256: str,
    revision: Any,
) -> None:
    if revision is None:
        _verify_inventory_files(rows, root, "candidate source")
        return

    _require(isinstance(revision, dict), "post-measurement validator revision record is malformed")
    _require(
        revision.get("schema") == "classmngr-phase9-post-measurement-validator-revision-v1",
        "post-measurement validator revision schema is invalid",
    )
    _require(
        revision.get("measuredSourceSha256") == source_sha256,
        "post-measurement validator revision changed the measured source fingerprint",
    )
    _require(
        revision.get("reason") == "validator chronology correction after route measurement; route/process evidence unchanged",
        "post-measurement validator revision reason is invalid",
    )
    revision_files = revision.get("files")
    _require(isinstance(revision_files, list), "post-measurement validator revision files are malformed")
    revision_by_path = {str(item.get("path", "")): item for item in revision_files if isinstance(item, dict)}
    _require(
        len(revision_by_path) == len(revision_files)
        and set(revision_by_path) == POST_MEASUREMENT_VALIDATOR_REVISION_PATHS,
        "post-measurement validator revision escaped its validator/test-only scope",
    )

    measured_by_path = {str(row.get("path", "")): row for row in rows}
    canonical_root = root.resolve(strict=True)
    for relative_path in sorted(POST_MEASUREMENT_VALIDATOR_REVISION_PATHS):
        record = revision_by_path[relative_path]
        measured = measured_by_path.get(relative_path)
        _require(isinstance(measured, dict), f"measured source inventory lacks validator revision path {relative_path}")
        _require(
            record.get("measuredSha256") == measured.get("sha256")
            and record.get("measuredSizeBytes") == measured.get("sizeBytes"),
            f"validator revision does not preserve measured source row {relative_path}",
        )
        current_sha = record.get("currentSha256")
        _require(isinstance(current_sha, str) and re.fullmatch(r"[0-9a-f]{64}", current_sha) is not None,
                 f"validator revision current SHA-256 is invalid for {relative_path}")
        candidate = canonical_root.joinpath(*PurePosixPath(relative_path).parts).resolve(strict=True)
        try:
            candidate.relative_to(canonical_root)
        except ValueError as exc:
            raise EvidenceError(f"validator revision path escaped source root: {relative_path}") from exc
        _require(candidate.is_file(), f"validator revision entry is not a file: {relative_path}")
        _require(candidate.stat().st_size == int(record.get("currentSizeBytes", -1)),
                 f"validator revision size mismatch: {relative_path}")
        _require(_sha256(candidate) == current_sha, f"validator revision SHA-256 mismatch: {relative_path}")

    _verify_inventory_files(
        [row for row in rows if str(row.get("path", "")) not in POST_MEASUREMENT_VALIDATOR_REVISION_PATHS],
        root,
        "candidate source",
    )


def _validate_evidence_relocation(manifest: dict[str, Any], run_directory: Path) -> dict[str, str] | None:
    derived_from = manifest.get("derivedFrom")
    relocation = manifest.get("relocation")
    if derived_from is None and relocation is None:
        return None
    _require(isinstance(derived_from, dict) and isinstance(relocation, dict), "derived evidence lacks its origin or relocation declaration")
    _require(
        relocation.get("schema") == "classmngr-phase9-evidence-relocation-v1",
        "derived evidence relocation schema is invalid",
    )
    original_root_text = derived_from.get("root")
    original_manifest_sha256 = derived_from.get("manifestSha256")
    _require(isinstance(original_root_text, str) and original_root_text.strip(), "derived evidence origin root is missing")
    _require(
        isinstance(original_manifest_sha256, str)
        and re.fullmatch(r"[0-9a-f]{64}", original_manifest_sha256) is not None,
        "derived evidence origin manifest hash is invalid",
    )
    original_root = Path(original_root_text).resolve(strict=True)
    corrected_root = run_directory.resolve(strict=True)
    _require(
        Path(str(relocation.get("originalRoot", ""))).resolve(strict=True) == original_root,
        "derived evidence relocation original root differs from its origin",
    )
    _require(
        Path(str(relocation.get("correctedRoot", ""))).resolve(strict=True) == corrected_root,
        "derived evidence relocation corrected root differs from the validated directory",
    )
    original_manifest_path = original_root / "manifest.json"
    preserved_manifest_path = corrected_root / "manifest-original-invalid.json"
    _require(
        _sha256(original_manifest_path) == original_manifest_sha256,
        "derived evidence original manifest changed after the run",
    )
    _require(
        _sha256(preserved_manifest_path) == original_manifest_sha256,
        "derived evidence does not preserve the original manifest bytes",
    )
    return {"originalRoot": str(original_root), "correctedRoot": str(corrected_root)}


def _relocate_artifact_reference(
    observed_path: Any,
    manifest_path: Any,
    run_directory: Path,
    relocation: dict[str, str] | None,
    label: str,
) -> Path:
    _require(isinstance(observed_path, str) and observed_path.strip(), f"{label} observed path is missing")
    _require(isinstance(manifest_path, str) and manifest_path.strip(), f"{label} manifest path is missing")
    if relocation is None:
        _require(manifest_path == observed_path, f"{label} path differs between observation and manifest")
        return Path(manifest_path).resolve(strict=False)
    original_root = Path(relocation["originalRoot"]).resolve(strict=True)
    corrected_root = Path(relocation["correctedRoot"]).resolve(strict=True)
    _require(corrected_root == run_directory.resolve(strict=True), f"{label} relocation targets another evidence directory")
    source = Path(observed_path).resolve(strict=False)
    try:
        relative = source.relative_to(original_root)
    except ValueError as exc:
        raise EvidenceError(f"{label} observed path is outside the declared original root") from exc
    expected = (corrected_root / relative).resolve(strict=False)
    actual = Path(manifest_path).resolve(strict=False)
    _require(actual == expected, f"{label} manifest path does not preserve the original relative path")
    return actual


def _relocate_shutdown_dump_link(
    observation: dict[str, Any],
    manifest_link: dict[str, Any],
    run_directory: Path,
    relocation: dict[str, str] | None,
    route_id: str,
) -> dict[str, Any]:
    if relocation is None:
        _require(
            manifest_link == observation,
            f"{route_id} linked shutdown dump evidence differs from the process observation",
        )
        return observation

    corrected_path = _relocate_artifact_reference(
        observation.get("path"),
        manifest_link.get("path"),
        run_directory,
        relocation,
        f"{route_id} shutdown dump",
    )
    observed_without_path = {key: value for key, value in observation.items() if key != "path"}
    linked_without_path = {key: value for key, value in manifest_link.items() if key != "path"}
    _require(
        linked_without_path == observed_without_path,
        f"{route_id} shutdown dump identity differs from the process observation",
    )
    normalized = dict(observation)
    normalized["path"] = str(corrected_path)
    return normalized


def _verify_shutdown_dump_file(dump: dict[str, Any], run_directory: Path, route_id: str) -> Path | None:
    status = dump.get("status")
    if status == "captured":
        _require(
            Path(str(dump.get("path", ""))).is_file(),
            f"{route_id} shutdown minidump file is missing",
        )
        dump_path = _artifact_path(dump.get("path"), run_directory, f"{route_id} shutdown minidump")
        _require(_sha256(dump_path) == dump.get("sha256"), f"{route_id} shutdown dump artifact identity mismatch")
        _require(dump_path.stat().st_size == int(dump.get("sizeBytes", -1)) and dump_path.stat().st_size > 4, f"{route_id} shutdown dump file size differs")
        _require(dump_path.read_bytes()[:4] == b"MDMP" and dump.get("signature") == "MDMP", f"{route_id} shutdown dump signature is invalid")
        return dump_path
    if status == "process-exited-before-trigger":
        _require(
            not Path(str(dump.get("path", ""))).exists()
            and int(dump.get("sizeBytes", -1)) == 0
            and dump.get("sha256") is None
            and dump.get("signature") is None,
            f"{route_id} records an unexpected or incomplete dump after exiting before its trigger",
        )
        return None
    raise EvidenceError(f"{route_id} shutdown minidump capture failed or has an unknown state")


def _row_as_dict(columns: dict[str, int], row: list[Any]) -> dict[str, Any]:
    return {name: row[index] for name, index in columns.items()}


def _table(snapshot: dict[str, Any], table_name: str) -> tuple[dict[str, int], list[list[Any]]]:
    tables = snapshot.get("tables")
    _require(isinstance(tables, dict) and table_name in tables, f"workspace snapshot lacks {table_name}")
    table = tables[table_name]
    schema = table.get("schema")
    rows = table.get("rows")
    _require(isinstance(schema, list) and isinstance(rows, list), f"workspace table {table_name} is malformed")
    columns: dict[str, int] = {}
    for index, info in enumerate(schema):
        _require(isinstance(info, list) and len(info) >= 2, f"workspace schema is malformed for {table_name}")
        columns[str(info[1])] = index
    return columns, rows


def _verify_snapshot(snapshot: dict[str, Any], label: str) -> None:
    _require(snapshot.get("schema") == SNAPSHOT_SCHEMA, f"{label} has an unknown schema")
    tables = snapshot.get("tables")
    _require(isinstance(tables, dict), f"{label} has no table inventory")
    _require(snapshot.get("integrityCheck") == "ok", f"{label} SQLite integrity check failed")
    _require(int(snapshot.get("tableCount", -1)) == len(tables), f"{label} table count is inconsistent")
    aggregate: dict[str, Any] = {}
    for name, table in tables.items():
        schema, rows = table.get("schema"), table.get("rows")
        _require(isinstance(schema, list) and isinstance(rows, list), f"{label} table {name} is malformed")
        _require(int(table.get("rowCount", -1)) == len(rows), f"{label} row count mismatch for {name}")
        contents = {"schema": schema, "rows": rows}
        _require(hashlib.sha256(_canonical(contents)).hexdigest() == table.get("contentSha256"), f"{label} content hash mismatch for {name}")
        aggregate[name] = contents
    _require(hashlib.sha256(_canonical(aggregate)).hexdigest() == snapshot.get("tableContentSha256"), f"{label} full table-content hash mismatch")


def _cell_value(cell: ET.Element) -> str:
    inline = cell.find("{*}is/{*}t")
    if inline is not None and inline.text is not None:
        return inline.text
    value = cell.find("{*}v")
    return "" if value is None or value.text is None else value.text


def _column_number(reference: str) -> int:
    match = re.fullmatch(r"([A-Z]+)[0-9]+", reference)
    _require(match is not None, f"invalid cell reference in fixture: {reference}")
    result = 0
    for character in match.group(1):
        result = result * 26 + ord(character) - ord("A") + 1
    return result


def parse_success_fixture(path: Path) -> dict[str, Any]:
    """Read this deterministic calendar grid fixture directly from OOXML bytes."""
    _require(_sha256(path) == SUCCESS_FIXTURE_SHA256, "success fixture SHA-256 differs from the accepted bytes")
    _require(path.stat().st_size == 16_284, "success fixture byte count differs from the accepted fixture")
    with zipfile.ZipFile(path) as archive:
        names = set(archive.namelist())
        required = {
            "xl/workbook.xml", "xl/_rels/workbook.xml.rels", "xl/styles.xml",
            "xl/worksheets/sheet1.xml", "xl/worksheets/sheet2.xml",
        }
        _require(names == required, "success workbook package entries differ from the accepted fixture")
        workbook = ET.fromstring(archive.read("xl/workbook.xml"))
        relationships = ET.fromstring(archive.read("xl/_rels/workbook.xml.rels"))
        rel_targets = {
            relation.attrib["Id"]: relation.attrib["Target"]
            for relation in relationships.findall("{*}Relationship")
        }
        sheet = next((item for item in workbook.findall("{*}sheets/{*}sheet") if item.attrib.get("name") == "Calendar 2026"), None)
        _require(sheet is not None, "success fixture lacks Calendar 2026 worksheet")
        rel_id = sheet.attrib.get("{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id")
        target = rel_targets.get(str(rel_id), "")
        worksheet_path = str(PurePosixPath("xl") / target)
        worksheet = ET.fromstring(archive.read(worksheet_path))
        worksheet_cell_counts: dict[str, int] = {}
        for name in sorted(names):
            if name.startswith("xl/worksheets/"):
                worksheet_cell_counts[name] = len(
                    ET.fromstring(archive.read(name)).findall(".//{*}c")
                )
        all_workbook_cells = sum(worksheet_cell_counts.values())
        _require(worksheet_cell_counts.get("xl/worksheets/sheet1.xml") == 381, "success fixture Calendar worksheet cell count differs from the accepted bytes")
        _require(worksheet_cell_counts.get("xl/worksheets/sheet2.xml") == 1, "success fixture Archive worksheet cell count differs from the accepted bytes")
        style_document = ET.fromstring(archive.read("xl/styles.xml"))
        xfs = style_document.find("{*}cellXfs")
        _require(xfs is not None and len(xfs) == 4, "success fixture cell styles differ from the accepted workbook")
        style_fills = [int(item.attrib.get("fillId", "0")) for item in xfs]
        fills = style_document.find("{*}fills")
        _require(fills is not None, "success fixture style fills are missing")
        fill_colors: list[str] = []
        for fill in fills:
            color = fill.find("{*}patternFill/{*}fgColor")
            fill_colors.append("" if color is None else color.attrib.get("rgb", "").upper())

    cells: dict[str, ET.Element] = {}
    row_cells: dict[int, dict[int, ET.Element]] = {}
    for cell in worksheet.findall(".//{*}c"):
        reference = cell.attrib.get("r", "")
        cells[reference] = cell
        row_number = int(re.search(r"[0-9]+$", reference).group(0))  # type: ignore[union-attr]
        row_cells.setdefault(row_number, {})[_column_number(reference)] = cell

    legend: dict[int, str] = {}
    for reference, title in (("Z20", "DYB Workshop"), ("Z21", "Red Day"), ("Z22", "Weekend")):
        cell = cells.get(reference)
        _require(cell is not None and _cell_value(cell) == title, f"success fixture legend cell {reference} is invalid")
        style = int(cell.attrib.get("s", "0"))
        legend[style] = title
    style_to_event = {style: title for style, title in legend.items()}
    _require(style_to_event == {1: "DYB Workshop", 2: "Red Day", 3: "Weekend"}, "success fixture legend styles differ from the accepted workbook")
    _require(style_fills[1] == 2 and style_fills[2] == 3 and style_fills[3] == 4, "success fixture legend style indexes changed")
    _require(fill_colors[2:] == ["FFFFF2CC", "FFFFCCCC", "FFD9EAD3"], "success fixture legend fill colors changed")

    header_rows: dict[int, tuple[int, int]] = {}
    workbook_years: set[int] = set()
    for row_number, row in row_cells.items():
        month_cell = row.get(1)
        if month_cell is None:
            continue
        month_name = _cell_value(month_cell)
        if month_name in MONTHS:
            year_cell = row.get(2)
            if year_cell is not None:
                _require(_cell_value(year_cell) == "2026", f"success fixture year mismatch in row {row_number}")
                workbook_years.add(int(_cell_value(year_cell)))
            header_rows[row_number] = (MONTHS.index(month_name) + 1, 2026)
    _require(len(header_rows) == 12, "success fixture does not contain all 12 month headers")
    _require(workbook_years == {2026}, "success fixture does not declare one 2026 workbook year")
    events: list[list[str]] = []
    skipped = 0
    observed_dates: set[str] = set()
    _require(all_workbook_cells == 382, "success fixture total worksheet-cell count differs from the accepted bytes")

    for header_row, (month, year) in sorted(header_rows.items()):
        first_of_month = date(year, month, 1)
        first_grid_date = first_of_month - timedelta(days=first_of_month.weekday())
        start_row = header_row + 2
        for row_offset in range(6):
            for column in range(1, 8):
                cell = row_cells.get(start_row + row_offset, {}).get(column)
                if cell is None:
                    continue
                try:
                    displayed_day = int(float(_cell_value(cell)))
                except ValueError:
                    continue
                event_date = first_grid_date + timedelta(days=row_offset * 7 + column - 1)
                if event_date.month != month or event_date.year != year or displayed_day != event_date.day:
                    continue
                category = style_to_event.get(int(cell.attrib.get("s", "0")))
                _require(category is not None, f"date {event_date.isoformat()} has an unknown fixture style")
                if category == "Weekend":
                    _require(event_date.weekday() >= 5, f"weekend style was assigned to weekday {event_date.isoformat()}")
                    skipped += 1
                    continue
                _require(event_date.weekday() < 5, f"event style was assigned to weekend {event_date.isoformat()}")
                title, event_type, all_day, time_status = (
                    ("DYB Workshop", "Workshop", "0", "Unknown")
                    if category == "DYB Workshop"
                    else ("Red Day", "Holiday", "1", "Timed")
                )
                iso_date = event_date.isoformat()
                _require(iso_date not in observed_dates, f"success fixture repeats event date {iso_date}")
                observed_dates.add(iso_date)
                events.append([title, event_type, time_status, "", all_day, iso_date, "", iso_date, ""])

    _require(len(events) == 261 and skipped == 104, "success fixture source bytes do not produce 261 weekday events and 104 skipped weekend rows")
    _require(sum(row[1] == "Workshop" for row in events) == 234, "success fixture does not produce 234 DYB Workshop weekdays")
    _require(sum(row[1] == "Holiday" for row in events) == 27, "success fixture does not produce 27 Red Day weekdays")
    return {
        "events": events,
        "skippedCount": skipped,
        "worksheetCellCount": all_workbook_cells,
        "calendarWorksheetCellCount": worksheet_cell_counts["xl/worksheets/sheet1.xml"],
        "archiveWorksheetCellCount": worksheet_cell_counts["xl/worksheets/sheet2.xml"],
        "dateCount": len(observed_dates) + skipped,
        "eventTypeCounts": {"Workshop": 234, "Holiday": 27},
    }


def _candidate_hash(rows: list[list[str]]) -> str:
    serialized = sorted(
        json.dumps(row, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        for row in rows
    )
    canonical = b"".join(row + b"\n" for row in serialized)
    return hashlib.sha256(canonical).hexdigest()


def _normalized_event_row(columns: dict[str, int], row: list[Any]) -> list[str]:
    value = _row_as_dict(columns, row)
    required_columns = (
        "id", "title", "event_type", "time_status", "repeat_series_id",
        "all_day", "start_date", "start_time", "end_date", "end_time",
    )
    _require(all(name in value for name in required_columns), "calendar_events table schema lacks required event columns")
    return [
        "" if value["title"] is None else str(value["title"]),
        "" if value["event_type"] is None else str(value["event_type"]),
        "" if value["time_status"] is None else str(value["time_status"]),
        "" if value["repeat_series_id"] is None else str(value["repeat_series_id"]),
        str(int(value["all_day"] or 0)),
        "" if value["start_date"] is None else str(value["start_date"]),
        "" if value["start_time"] is None else str(value["start_time"]),
        "" if value["end_date"] is None else str(value["end_date"]),
        "" if value["end_time"] is None else str(value["end_time"]),
    ]


def _validate_workspace_pair(
    route: dict[str, Any],
    run_directory: Path,
    expected_events: list[list[str]],
    *,
    successful: bool,
    relocation: dict[str, str] | None = None,
) -> dict[str, Any]:
    workspace = route.get("workspace", {})
    before = _read_json(_artifact_path(workspace.get("beforeSnapshot"), run_directory, f"{route['routeId']} before snapshot"))
    after = _read_json(_artifact_path(workspace.get("afterSnapshot"), run_directory, f"{route['routeId']} after snapshot"))
    _verify_snapshot(before, f"{route['routeId']} before workspace")
    _verify_snapshot(after, f"{route['routeId']} after workspace")
    _require(before.get("workspacePath") == after.get("workspacePath"), f"{route['routeId']} before/after workspace paths differ")
    workspace_path = _relocate_artifact_reference(
        before.get("workspacePath"),
        workspace.get("path"),
        run_directory,
        relocation,
        f"{route['routeId']} workspace",
    )
    _require(workspace_path.is_file(), f"{route['routeId']} relocated workspace file is missing")
    _require(
        _sha256(workspace_path) == workspace.get("sha256AtExit"),
        f"{route['routeId']} relocated workspace file differs from its process exit hash",
    )
    before_tables, after_tables = before["tables"], after["tables"]
    _require(set(before_tables) == set(after_tables), f"{route['routeId']} table inventory changed")
    before_columns, before_events = _table(before, "calendar_events")
    after_columns, after_events = _table(after, "calendar_events")
    _require(before_columns == after_columns, f"{route['routeId']} calendar event schema changed")

    if not successful:
        _require(before.get("tableContentSha256") == after.get("tableContentSha256"), f"{route['routeId']} parser error changed workspace row content")
        _require(before_tables == after_tables, f"{route['routeId']} parser error changed full table rows")
        _require(len(before_events) == len(after_events) == 180, "parser error workspace does not retain exactly 180 existing events")
        _require(workspace.get("rowLevelContentsUnchanged") is True, "parser error row-level workspace equality was not recorded")
        return {"beforeEventCount": len(before_events), "afterEventCount": len(after_events), "rowLevelContentsUnchanged": True}

    for name in before_tables:
        if name == "calendar_events":
            continue
        if name != "sqlite_sequence":
            _require(before_tables[name] == after_tables[name], f"success import changed non-calendar workspace rows in {name}")
            continue
        sequence_columns_before, sequence_rows_before = _table(before, name)
        sequence_columns_after, sequence_rows_after = _table(after, name)
        _require(sequence_columns_before == sequence_columns_after, "success import changed SQLite sequence schema")
        sequence_name_column = sequence_columns_before.get("name")
        sequence_value_column = sequence_columns_before.get("seq")
        _require(sequence_name_column is not None and sequence_value_column is not None, "success import SQLite sequence schema is incomplete")
        before_sequences = {str(row[sequence_name_column]): row for row in sequence_rows_before}
        after_sequences = {str(row[sequence_name_column]): row for row in sequence_rows_after}
        _require(len(before_sequences) == len(sequence_rows_before) and len(after_sequences) == len(sequence_rows_after), "success import SQLite sequence names are duplicated")
        _require(
            {key: row for key, row in before_sequences.items() if key != "calendar_events"}
            == {key: row for key, row in after_sequences.items() if key != "calendar_events"},
            "success import changed an unrelated SQLite sequence row",
        )
        if "calendar_events" in before_sequences or "calendar_events" in after_sequences:
            _require("calendar_events" in before_sequences and "calendar_events" in after_sequences, "success import removed the Calendar event sequence")
    before_by_id = {str(row[before_columns["id"]]): row for row in before_events}
    after_by_id = {str(row[after_columns["id"]]): row for row in after_events}
    _require(len(before_by_id) == len(before_events), "success baseline Calendar event IDs are not unique")
    _require(len(after_by_id) == len(after_events), "success persisted Calendar event IDs are not unique")
    _require(all(after_by_id.get(identifier) == row for identifier, row in before_by_id.items()), "success import altered an existing Calendar event row")
    new_rows = [row for identifier, row in after_by_id.items() if identifier not in before_by_id]
    normalized_new = [_normalized_event_row(after_columns, row) for row in new_rows]
    _require(Counter(map(tuple, normalized_new)) == Counter(map(tuple, expected_events)), "persisted Calendar event row contents differ from the 261 parsed fixture events")
    _require(len(new_rows) == len(expected_events) == 261, "success import added a duplicate or missing Calendar event row")
    _require(len(after_events) == 441, "success workspace does not contain exactly 441 total Calendar events")
    _require(len(set(map(tuple, normalized_new))) == len(normalized_new), "success import persisted duplicate Calendar event content")
    if "sqlite_sequence" in after_tables:
        sequence_columns, sequence_rows = _table(after, "sqlite_sequence")
        sequence_name_column = sequence_columns.get("name")
        sequence_value_column = sequence_columns.get("seq")
        _require(sequence_name_column is not None and sequence_value_column is not None, "success import SQLite sequence schema is incomplete")
        calendar_sequence = [
            row for row in sequence_rows
            if str(row[sequence_name_column]) == "calendar_events"
        ]
        _require(len(calendar_sequence) == 1, "success import has no unique Calendar event sequence row")
        maximum_event_id = max(int(row[after_columns["id"]]) for row in after_events)
        _require(int(calendar_sequence[0][sequence_value_column]) == maximum_event_id, "success import Calendar sequence does not match its persisted maximum event ID")
    result = {
        "beforeEventCount": len(before_events),
        "afterEventCount": len(after_events),
        "rowLevelContentsUnchanged": False,
        "importedEventCount": len(new_rows),
        "persistedEventRowsSha256": _candidate_hash(normalized_new),
        "eventTypeCounts": {
            "Workshop": sum(row[1] == "Workshop" for row in normalized_new),
            "Holiday": sum(row[1] == "Holiday" for row in normalized_new),
        },
        "allDatesIn2026": all(row[5].startswith("2026-") and row[7].startswith("2026-") for row in normalized_new),
    }
    _require(result["allDatesIn2026"] is True, "success import contains a date outside 2026")
    _require(result["eventTypeCounts"] == {"Workshop": 234, "Holiday": 27}, "persisted event type distribution differs from fixture expectations")
    return result


def _validate_memory(route: dict[str, Any]) -> dict[str, Any]:
    memory = route.get("memory", {})
    normal = memory.get("normalUseSamples")
    operation = memory.get("operationSamples")
    _require(isinstance(normal, list) and isinstance(operation, list), f"{route['routeId']} memory evidence is malformed")
    points = {str(item.get("point")): item for item in normal}
    required = {"startup-ready-checkpoint", "startup-ready-external", "startup-settled", "post-cleanup"}
    _require(required.issubset(points), f"{route['routeId']} lacks startup or post-cleanup memory samples")
    tail = [item for item in normal if item.get("point") == "post-cleanup-idle-tail"]
    _require(len(tail) == 10 and sorted(int(item.get("sampleIndex", -1)) for item in tail) == list(range(10)), f"{route['routeId']} lacks ten ordered post-cleanup tail samples")
    violations: list[dict[str, Any]] = []
    for item in normal:
        for key in ("workingSetBytes", "privateBytes", "commitBytes", "peakWorkingSetBytes", "handles", "threads"):
            _require(isinstance(item.get(key), int) and int(item[key]) > 0, f"{route['routeId']} {item.get('point')} lacks {key}")
        if int(item["workingSetBytes"]) >= WORKING_SET_LIMIT:
            violations.append({"point": item.get("point"), "sampleIndex": item.get("sampleIndex"), "workingSetBytes": item["workingSetBytes"]})
    _require(any(item.get("point") == "parse-completed" for item in operation), f"{route['routeId']} lacks an operation-peak sample")
    peaks = [
        {
            "point": item.get("point"),
            "peakWorkingSetBytes": int(item["peakWorkingSetBytes"]),
            "underAdvisoryCeiling": int(item["peakWorkingSetBytes"]) < OPERATION_PEAK_LIMIT,
        }
        for item in operation
    ]
    return {
        "normalUseWorkingSetExclusiveBytes": WORKING_SET_LIMIT,
        "passed": not violations,
        "violations": violations,
        "operationPeakAdvisoryExclusiveBytes": OPERATION_PEAK_LIMIT,
        "operationPeaks": peaks,
        "privateBytesReturnAllowance": None,
    }


def _build_final_route_reporting_rows(summary: dict[str, Any]) -> list[dict[str, Any]]:
    routes = summary.get("routes")
    expected_ids = {"route-a-large-calendar-success", "route-b-malformed-calendar-error"}
    _require(isinstance(routes, dict) and set(routes) == expected_ids, "final report route inventory differs from the validated run")
    rows: list[dict[str, Any]] = []
    for route_id in sorted(expected_ids):
        route = routes[route_id]
        workspace = route.get("rowLevelWorkspace", {})
        parsed = int(route.get("parsedEventCount", -1))
        imported = int(route.get("importedEventCount", -1))
        skipped = int(route.get("skippedEventCount", -1))
        before_count = int(workspace.get("beforeEventCount", -1))
        after_count = int(workspace.get("afterEventCount", -1))
        unchanged = workspace.get("rowLevelContentsUnchanged")
        if route_id == "route-a-large-calendar-success":
            _require(parsed == imported == 261 and skipped == 104, "success final report does not match its validated import counts")
            _require(int(workspace.get("importedEventCount", -1)) == imported and before_count == 180 and after_count == 441,
                     "success final report does not match its validated workspace delta")
            _require(unchanged is False, "success final report incorrectly claims an unchanged workspace")
        else:
            _require(parsed == imported == skipped == 0, "parser-error final report must record zero parsed, applied, and skipped events")
            _require(before_count == after_count == 180 and unchanged is True,
                     "parser-error final report lacks the full unchanged-workspace comparison")
        rows.append({
            "routeId": route_id,
            "pid": int(route.get("pid", 0)),
            "outcome": str(route.get("outcome", "")),
            "beforeEventCount": before_count,
            "afterEventCount": after_count,
            "parsedEventCount": parsed,
            "appliedEventCount": imported,
            "importedCount": imported,
            "skippedCount": skipped,
            "rowLevelContentsUnchanged": unchanged,
            "stagedWorkbookRemoved": bool(route.get("stagedWorkbookRemoved")),
            "workingSetGatePassed": bool(route.get("memory", {}).get("passed")),
            "naturalExitCode": int(route.get("shutdown", {}).get("processExitCode", -1)),
        })
    return rows


def _validate_revalidation_reporting(
    manifest: dict[str, Any],
    run_directory: Path,
    current_result: dict[str, Any],
) -> None:
    history = manifest.get("validationHistory")
    _require(isinstance(history, list) and len(history) == 1,
             "corrected evidence must preserve its single original invalid validation result")
    original_copy = _artifact_path(
        str(run_directory / "manifest-original-invalid.json"),
        run_directory,
        "preserved original manifest",
    )
    original_hash = _sha256(original_copy)
    derived_from = manifest.get("derivedFrom", {})
    _require(original_hash == derived_from.get("manifestSha256"), "validation history is not bound to the preserved original manifest")
    original_manifest = _read_json(original_copy)
    initial_validation = original_manifest.get("validation", {})
    initial = history[0]
    _require(initial.get("stage") == "original-attempt-initial-validation", "validation history does not identify the initial run result")
    _require(initial.get("status") == "invalid-evidence" and int(initial.get("exitCode", -1)) == 1,
             "validation history does not retain the original invalid result")
    _require(initial.get("originalManifestSha256") == original_hash,
             "initial validation history points at another original manifest")
    _require(initial.get("validatorSha256") == initial_validation.get("validatorSha256"),
             "initial validation history changed the original validator identity")
    _require(initial.get("result") == initial_validation.get("result"),
             "initial validation history changed the original invalid result")
    initial_log = _artifact_path(initial.get("outputPath"), run_directory, "initial validator output")
    _require(_sha256(initial_log) == initial.get("outputSha256") == initial_validation.get("logSha256"),
             "initial validation history output identity differs from the original result")
    try:
        initial_log_result = json.loads(initial_log.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise EvidenceError(f"could not parse initial validator output: {exc}") from exc
    _require(initial_log_result == initial.get("result"), "initial validator output differs from its preserved result")

    validation = manifest.get("validation", {})
    validator_identity = manifest.get("validatorIdentity", {})
    _require(validation.get("status") == "passed" and int(validation.get("exitCode", -1)) == 0,
             "corrected manifest does not report a passing revalidation")
    _require(validation.get("validatorPath") == validator_identity.get("path")
             and validation.get("validatorSha256") == validator_identity.get("sha256"),
             "corrected validation report differs from the corrected validator identity")
    output_path = _artifact_path(validation.get("outputPath"), run_directory, "final validator output")
    _require(_sha256(output_path) == validation.get("outputSha256"), "final validator output hash differs")
    try:
        output = json.loads(output_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise EvidenceError(f"could not parse final validator output: {exc}") from exc
    summary_path = _artifact_path(validation.get("summaryPath"), run_directory, "final validator summary")
    _require(output.get("status") == "passed" and int(output.get("routeCount", -1)) == 2,
             "final validator output does not report both passing routes")
    _require(Path(str(output.get("summaryPath", ""))).resolve(strict=False) == summary_path,
             "final validator output points to a different summary")
    _require(_sha256(summary_path) == validation.get("summarySha256"), "final summary hash differs from the manifest link")
    existing_summary = _read_json(summary_path)
    _require(existing_summary.get("status") == "passed" and int(existing_summary.get("routeCount", -1)) == 2,
             "final linked summary does not report both passing routes")
    _require(existing_summary.get("sourceFingerprintSha256") == manifest.get("source", {}).get("sha256")
             and existing_summary.get("packageSha256") == manifest.get("package", {}).get("sha256")
             and existing_summary.get("applicationSha256") == manifest.get("application", {}).get("sha256"),
             "final linked summary changed measured source or package identity")
    _require(current_result.get("status") == "passed" and current_result.get("routeCount") == 2,
             "current validator pass does not confirm the linked revalidation")

    metadata_tool = manifest.get("metadataToolRevision", {})
    _require(metadata_tool.get("measuredSourceSha256") == manifest.get("source", {}).get("sha256"),
             "metadata tool revision is not separated from the measured source fingerprint")
    metadata_tool_path = _artifact_path(metadata_tool.get("path"), run_directory, "metadata finalization helper")
    _require(_sha256(metadata_tool_path) == metadata_tool.get("sha256"), "metadata finalization helper hash differs")

    final_checks_link = manifest.get("finalChecks", {})
    final_checks_path = _artifact_path(final_checks_link.get("path"), run_directory, "final-checks report")
    _require(_sha256(final_checks_path) == final_checks_link.get("sha256"), "final-checks report hash differs")
    final_checks = _read_json(final_checks_path)
    _require(final_checks.get("finalStatus") == "passed"
             and final_checks.get("measuredSourceSha256") == manifest.get("source", {}).get("sha256"),
             "final-checks report does not preserve the passing measured run")
    _require(final_checks.get("summarySha256") == validation.get("summarySha256"),
             "final-checks summary hash differs from the manifest link")
    _require(final_checks.get("metadataToolRevision") == metadata_tool,
             "final-checks metadata tool revision differs from the manifest")
    expected_route_rows = _build_final_route_reporting_rows(current_result)
    _require(final_checks.get("routeResults") == expected_route_rows,
             "final-checks route reporting differs from validated route metrics and workspace comparisons")


def _validate_route_lifecycle(route: dict[str, Any], successful: bool) -> dict[str, Any]:
    lifecycle = route.get("lifecycle", {})
    activated = lifecycle.get("calendarActivated", {})
    suspended = lifecycle.get("calendarSuspended", {})
    reentered = lifecycle.get("calendarReentered", {})
    for label, record, state in (
        ("initial Calendar activation", activated, "activated"),
        ("Calendar suspension", suspended, "suspended"),
        ("Calendar re-entry", reentered, "activated"),
    ):
        _require(isinstance(record, dict) and record, f"{route['routeId']} lacks {label} record")
        _require(record.get("event") == "page-lifecycle" and record.get("page_key") == "calendar" and record.get("state") == state, f"{route['routeId']} {label} identity is invalid")
        _require(record.get("processId") == int(route["processIdentity"]["pid"]), f"{route['routeId']} {label} process identity is invalid")
        _require(record.get("mainWindowHandle") == int(route["processIdentity"]["mainWindowHandle"]), f"{route['routeId']} {label} window identity is invalid")
    _require(suspended.get("state") == "suspended" and reentered.get("state") == "activated", f"{route['routeId']} did not suspend and re-enter Calendar once")
    _require(int(lifecycle.get("suspendCount", -1)) == 1 and int(lifecycle.get("reentryCount", -1)) == 1, f"{route['routeId']} performed more than one Calendar lifecycle cycle")
    activated_metrics = activated.get("page", {}).get("calendar_runtime_metrics", {})
    suspended_metrics = suspended.get("page", {}).get("calendar_runtime_metrics", {})
    reentered_metrics = reentered.get("page", {}).get("calendar_runtime_metrics", {})
    _require(isinstance(activated_metrics, dict) and isinstance(suspended_metrics, dict) and isinstance(reentered_metrics, dict), f"{route['routeId']} lifecycle Calendar metrics are missing")
    import_metrics = suspended_metrics.get("calendar_import", {})
    _require(isinstance(import_metrics, dict), f"{route['routeId']} import lifecycle metrics are missing")
    _require(int(suspended_metrics.get("calendar_import_cache_refresh_count", -1)) == (1 if successful else 0), f"{route['routeId']} Calendar cache refresh count is invalid")
    _require(int(reentered_metrics.get("calendar_import_cache_refresh_count", -1)) == (1 if successful else 0), f"{route['routeId']} cache refresh count did not survive re-entry")
    calendar_load_wait = route.get("calendarLoadWait", {})
    _require(isinstance(calendar_load_wait, dict) and calendar_load_wait.get("completed") is True, f"{route['routeId']} Calendar load did not complete after re-entry")
    reentry_settled = calendar_load_wait.get("cacheSettledCheckpoint", {})
    _require(isinstance(reentry_settled, dict), f"{route['routeId']} post-reentry cache settled checkpoint is missing")
    _require(reentry_settled.get("event") == "calendar-import-checkpoint" and reentry_settled.get("observation") == "cache-settled" and reentry_settled.get("page_key") == "calendar", f"{route['routeId']} post-reentry cache checkpoint identity is invalid")
    _require(reentry_settled.get("processId") == int(route["processIdentity"]["pid"]) and reentry_settled.get("mainWindowHandle") == int(route["processIdentity"]["mainWindowHandle"]), f"{route['routeId']} post-reentry cache checkpoint belongs to another process/window")
    reentry_time = _timestamp(reentered.get("captured_at"), f"{route['routeId']} Calendar re-entry")
    reentry_settled_time = _timestamp(reentry_settled.get("captured_at"), f"{route['routeId']} post-reentry cache checkpoint")
    load_complete_time = _timestamp(calendar_load_wait.get("waitedAtUtc"), f"{route['routeId']} post-reentry Calendar load")
    _require(reentry_time <= reentry_settled_time <= load_complete_time, f"{route['routeId']} post-reentry cache/load checks were out of order")
    reentry_cache_metrics = reentry_settled.get("calendar_runtime_metrics", {})
    _require(reentry_cache_metrics.get("cache_loading") is False and reentry_cache_metrics.get("upcoming_range_loaded") is True, f"{route['routeId']} post-reentry cache was not settled")
    _require(int(reentry_cache_metrics.get("calendar_import_cache_refresh_count", -1)) == (1 if successful else 0), f"{route['routeId']} post-reentry cache refresh count is invalid")
    return {"initial": activated_metrics, "suspended": suspended_metrics, "reentered": reentered_metrics, "import": import_metrics}


def _validate_shutdown_observability(
    route: dict[str, Any],
    run_directory: Path,
    relocation: dict[str, str] | None = None,
) -> dict[str, Any]:
    link = route.get("diagnostics", {}).get("shutdownObservability", {})
    path = _artifact_path(link.get("path"), run_directory, f"{route['routeId']} shutdown observability")
    _require(_sha256(path) == link.get("sha256"), f"{route['routeId']} shutdown observability artifact identity mismatch")
    document = _read_json(path)
    _require(document.get("schema") == "classmngr-phase9-process-shutdown-observability-v1", f"{route['routeId']} shutdown observability schema is invalid")
    _require(document.get("routeId") == route["routeId"] and int(document.get("processId", 0)) == int(route["processIdentity"]["pid"]), f"{route['routeId']} shutdown observability is bound to another process")
    _require(document.get("closeMainWindowReturned") is True, f"{route['routeId']} shutdown did not record a successful close request")
    _require(document.get("gracefulExitCompleted") is True and document.get("gracefulExitTimedOut") is False, f"{route['routeId']} did not complete a normal process exit")
    _require(int(document.get("processExitCode", -1)) == 0 and int(route.get("processExitCode", -1)) == 0, f"{route['routeId']} shutdown process exit code is not zero")
    snapshots = document.get("snapshots")
    _require(isinstance(snapshots, list) and len(snapshots) >= 1, f"{route['routeId']} shutdown lacks a starting window snapshot")
    _require(snapshots[0].get("stage") == "before-close-request", f"{route['routeId']} shutdown does not begin with the pre-close window snapshot")
    _require(snapshots[-1].get("stage") != "graceful-close-timeout", f"{route['routeId']} recorded a timeout snapshot despite passing normal exit")
    for snapshot in snapshots:
        state = snapshot.get("state", {})
        _require(int(state.get("processId", 0)) == int(route["processIdentity"]["pid"]), f"{route['routeId']} shutdown window snapshot belongs to another process")
        windows = state.get("topLevelWindows")
        _require(isinstance(windows, list) and int(state.get("topLevelWindowCount", -1)) == len(windows), f"{route['routeId']} shutdown top-level window inventory is malformed")
        _require(all(int(window.get("processId", 0)) == int(route["processIdentity"]["pid"]) for window in windows), f"{route['routeId']} shutdown inventory contains a foreign PID window")
        _require(int(state.get("uiaProbeTimeoutMillisecondsPerWindow", 0)) == 350, f"{route['routeId']} shutdown UIA window probe is not bounded")
        for window in windows:
            _require(window.get("uiaProbeStatus") in {"completed", "timed-out", "error"}, f"{route['routeId']} shutdown window lacks UIA probe status")
            for field in ("nativeTitle", "nativeClassName", "uiaTitle", "uiaControlType", "uiaAutomationId", "uiaClassName"):
                _require(isinstance(window.get(field), str), f"{route['routeId']} shutdown window lacks {field}")
    status_samples = document.get("exitStatusSamples")
    _require(isinstance(status_samples, list) and len(status_samples) >= 3, f"{route['routeId']} shutdown lacks a per-second process status series")
    _require(status_samples[0].get("stage") == "before-close-request" and status_samples[1].get("stage") == "after-close-request" and status_samples[-1].get("stage") == "process-exit-observed", f"{route['routeId']} shutdown process series does not bracket close and exit")
    elapsed = [float(sample.get("elapsedSeconds", -1)) for sample in status_samples[1:]]
    _require(all(left <= right and right - left <= 1.5 for left, right in zip(elapsed, elapsed[1:])), f"{route['routeId']} shutdown process status sampling is not approximately once per second")
    for sample in status_samples:
        _require(int(sample.get("processId", 0)) == int(route["processIdentity"]["pid"]), f"{route['routeId']} shutdown status sample belongs to another process")
        for field in ("cpuTimeMilliseconds", "workingSetBytes", "privateBytes", "handles", "threads"):
            _require(isinstance(sample.get(field), (int, float)) and sample[field] >= 0, f"{route['routeId']} shutdown status sample lacks {field}")
    _require(status_samples[-1].get("processHasExited") is True and int(status_samples[-1].get("processExitCode", -1)) == 0, f"{route['routeId']} shutdown status series does not end at natural zero exit")
    application_trace = _validate_application_shutdown_trace(route, run_directory)
    observed_dump = document.get("shutdownMiniDump", {})
    dump_link = route.get("diagnostics", {}).get("shutdownMiniDump", {})
    _require(isinstance(observed_dump, dict) and isinstance(dump_link, dict), f"{route['routeId']} shutdown MiniDumpWriteDump evidence is missing")
    dump = _relocate_shutdown_dump_link(
        observed_dump,
        dump_link,
        run_directory,
        relocation,
        str(route["routeId"]),
    )
    _require(dump.get("schema") == "classmngr-phase9-shutdown-minidump-v1", f"{route['routeId']} shutdown minidump schema is invalid")
    _require(int(dump.get("processId", 0)) == int(route["processIdentity"]["pid"]), f"{route['routeId']} shutdown minidump belongs to another process")
    _require(int(dump.get("triggerElapsedSeconds", -1)) == 12, f"{route['routeId']} shutdown minidump trigger is not the bounded 12-second checkpoint")
    _require(dump.get("dumpKind") == "MiniDumpNormal | MiniDumpWithThreadInfo", f"{route['routeId']} shutdown dump kind is not normal plus thread-info")
    _require(dump.get("status") in {"captured", "process-exited-before-trigger"}, f"{route['routeId']} shutdown minidump capture failed or has an unknown state")
    if dump.get("status") == "captured":
        _verify_shutdown_dump_file(dump, run_directory, str(route["routeId"]))
        _require(isinstance(dump.get("captureElapsedMilliseconds"), (int, float)) and dump["captureElapsedMilliseconds"] >= 0, f"{route['routeId']} shutdown dump capture duration is missing")
        _require(isinstance(dump.get("captureElapsedSeconds"), (int, float)) and 12 <= dump["captureElapsedSeconds"] <= 15, f"{route['routeId']} shutdown dump was not captured at the bounded 12-to-15-second checkpoint")
        _require(isinstance(dump.get("captureStartedAtUtc"), str) and isinstance(dump.get("captureCompletedAtUtc"), str), f"{route['routeId']} shutdown dump timestamps are missing")
    else:
        _require(route.get("processExitCode") == 0 and document.get("gracefulExitCompleted") is True, f"{route['routeId']} skipped its dump without exiting naturally before the trigger")
        _verify_shutdown_dump_file(dump, run_directory, str(route["routeId"]))
    return {
        "closeMainWindowReturned": document.get("closeMainWindowReturned"),
        "gracefulExitCompleted": document.get("gracefulExitCompleted"),
        "gracefulExitTimedOut": document.get("gracefulExitTimedOut"),
        "processExitCode": document.get("processExitCode"),
        "snapshotCount": len(snapshots),
        "perSecondStatusSampleCount": len(status_samples),
        "applicationTrace": application_trace,
        "shutdownMiniDump": {
            "status": dump.get("status"),
            "path": dump.get("path"),
            "sizeBytes": dump.get("sizeBytes"),
            "sha256": dump.get("sha256"),
            "dumpKind": dump.get("dumpKind"),
        },
    }


def _validate_application_shutdown_trace(route: dict[str, Any], run_directory: Path) -> dict[str, Any]:
    link = route.get("diagnostics", {}).get("applicationShutdownTrace", {})
    path = _artifact_path(link.get("path"), run_directory, f"{route['routeId']} application shutdown trace")
    _require(link.get("schema") == "classmngr-phase9-application-shutdown-trace-link-v1", f"{route['routeId']} application shutdown trace link schema is invalid")
    _require(link.get("status") == "captured", f"{route['routeId']} application shutdown trace was not fully captured")
    _require(_sha256(path) == link.get("sha256"), f"{route['routeId']} application shutdown trace artifact identity mismatch")
    _require(path.stat().st_size == int(link.get("sizeBytes", -1)) and path.stat().st_size > 0, f"{route['routeId']} application shutdown trace size differs")
    rows = _read_jsonl(path)
    process_id = int(route["processIdentity"]["pid"])
    _require(len(rows) == int(link.get("eventCount", -1)) and len(rows) > 0, f"{route['routeId']} application shutdown trace event count differs")
    _require(all(int(row.get("process_id", 0)) == process_id for row in rows), f"{route['routeId']} application shutdown trace contains a foreign PID")
    sequences = [int(row.get("sequence", -1)) for row in rows]
    _require(sequences == list(range(len(rows))), f"{route['routeId']} application shutdown trace sequence is incomplete or unordered")
    events = [str(row.get("event", "")) for row in rows]
    _require(link.get("events") == events, f"{route['routeId']} shutdown trace event index differs from the raw trace")

    _validate_ordered_shutdown_milestones(events, str(route["routeId"]))

    _require(any(event in events for event in (
        "application_bootstrap.shutdown_sequence.update_cancel_completed",
        "application_bootstrap.shutdown_sequence.update_cancel_skipped",
    )), f"{route['routeId']} shutdown trace lacks the startup update cancellation boundary")
    _require(any(event in events for event in (
        "application_bootstrap.shutdown_sequence.workspace_close_completed",
        "application_bootstrap.shutdown_sequence.workspace_close_skipped",
    )), f"{route['routeId']} shutdown trace lacks the preparation workspace boundary")
    _require(int(link.get("processId", 0)) == process_id, f"{route['routeId']} shutdown trace link belongs to another process")
    return {
        "eventCount": len(events),
        "firstSequence": sequences[0],
        "lastSequence": sequences[-1],
        "milestones": list(APPLICATION_SHUTDOWN_MILESTONES),
    }


def _validate_uia_actions(actions: list[dict[str, Any]], route_by_id: dict[str, dict[str, Any]]) -> dict[str, int]:
    counts: Counter[str] = Counter()
    required = {
        "navigate-to-calendar", "open-calendar-preferences", "start-calendar-import",
        "close-calendar-preferences", "suspend-calendar-to-my-workspace", "reenter-calendar",
    }
    for route_id, route in route_by_id.items():
        route_actions = [item for item in actions if item.get("routeId") == route_id]
        observed = {str(item.get("actionKind")) for item in route_actions}
        _require(required.issubset(observed), f"{route_id} is missing one or more authentic Calendar UIA actions")
        identity = route["processIdentity"]
        for action in route_actions:
            counts[f"{route_id}:{action.get('actionKind')}"] += 1
            _require(action.get("processInstanceId") == route_id, f"{route_id} UIA action has the wrong process instance")
            _require(int(action.get("applicationProcessId", -1)) == int(identity["pid"]), f"{route_id} UIA action has the wrong application PID")
            _require(int(action.get("elementProcessId", -1)) == int(identity["pid"]), f"{route_id} UIA target belongs to another process")
            _require(int(action.get("windowProcessId", -1)) == int(identity["pid"]), f"{route_id} UIA target window belongs to another process")
            _require(int(action.get("windowHandle", 0)) > 0 and int(action.get("mainWindowHandle", 0)) == int(identity["mainWindowHandle"]), f"{route_id} UIA action lacks measured HWND correlation")
            _require(str(action.get("method", "")).startswith("uia-"), f"{route_id} UIA action was not issued through UI Automation")
            _timestamp(action.get("issuedAtUtc"), f"{route_id} UIA {action.get('actionKind')}")
        import_result = route.get("import", {})
        terminal_time = _timestamp(
            import_result.get("serviceTerminalCheckpoint", {}).get("captured_at"),
            f"{route_id} terminal checkpoint",
        )
        settled_time = _timestamp(
            import_result.get("cacheSettledCheckpoint", {}).get("captured_at"),
            f"{route_id} cache settled checkpoint",
        )
        action_times = {
            str(action.get("actionKind")): _timestamp(
                action.get("issuedAtUtc"), f"{route_id} {action.get('actionKind')}"
            )
            for action in route_actions
        }
        _require(
            terminal_time <= settled_time <= action_times["close-calendar-preferences"],
            f"{route_id} preferences closed before the terminal import and settled cache checkpoints",
        )
        _require(
            settled_time <= action_times["suspend-calendar-to-my-workspace"],
            f"{route_id} Calendar suspended before the settled cache checkpoint",
        )
    _require(len([item for item in actions if item.get("routeId") in route_by_id]) == len(actions), "UIA action log contains a foreign route")
    return dict(counts)


def _validate_import_metrics(
    route: dict[str, Any],
    fixture: dict[str, Any],
    expected: dict[str, Any],
    successful: bool,
    run_directory: Path,
) -> dict[str, Any]:
    result = route.get("import", {})
    metrics = result.get("runtimeMetrics", {})
    _require(isinstance(metrics, dict), f"{route['routeId']} service metrics are missing")
    terminal_checkpoint = result.get("serviceTerminalCheckpoint", {})
    _require(isinstance(terminal_checkpoint, dict), f"{route['routeId']} immediate terminal service checkpoint is missing")
    _require(terminal_checkpoint.get("event") == "calendar-import-checkpoint" and terminal_checkpoint.get("observation") == "terminal" and terminal_checkpoint.get("page_key") == "calendar", f"{route['routeId']} immediate terminal checkpoint identity is invalid")
    _require(terminal_checkpoint.get("processId") == int(route["processIdentity"]["pid"]), f"{route['routeId']} terminal service checkpoint belongs to another process")
    _require(terminal_checkpoint.get("mainWindowHandle") == int(route["processIdentity"]["mainWindowHandle"]), f"{route['routeId']} terminal service checkpoint belongs to another window")
    terminal_runtime_metrics = terminal_checkpoint.get("calendar_runtime_metrics", {})
    terminal_metrics = terminal_runtime_metrics.get("calendar_import", {}) if isinstance(terminal_runtime_metrics, dict) else {}
    _require(isinstance(terminal_metrics, dict), f"{route['routeId']} immediate service counters are missing")
    _require(terminal_metrics == metrics, f"{route['routeId']} terminal and suspended service metrics differ")
    terminal_time = _timestamp(terminal_checkpoint.get("captured_at"), f"{route['routeId']} terminal service checkpoint")
    settled_checkpoint = result.get("cacheSettledCheckpoint", {})
    _require(isinstance(settled_checkpoint, dict), f"{route['routeId']} post-import cache settled checkpoint is missing")
    _require(settled_checkpoint.get("event") == "calendar-import-checkpoint" and settled_checkpoint.get("observation") == "cache-settled" and settled_checkpoint.get("page_key") == "calendar", f"{route['routeId']} cache settled checkpoint identity is invalid")
    _require(settled_checkpoint.get("processId") == int(route["processIdentity"]["pid"]), f"{route['routeId']} cache settled checkpoint belongs to another process")
    _require(settled_checkpoint.get("mainWindowHandle") == int(route["processIdentity"]["mainWindowHandle"]), f"{route['routeId']} cache settled checkpoint belongs to another window")
    settled_time = _timestamp(settled_checkpoint.get("captured_at"), f"{route['routeId']} cache settled checkpoint")
    _require(terminal_time <= settled_time, f"{route['routeId']} cache settled checkpoint preceded its service terminal result")
    settled_runtime_metrics = settled_checkpoint.get("calendar_runtime_metrics", {})
    _require(isinstance(settled_runtime_metrics, dict), f"{route['routeId']} cache settled runtime metrics are missing")
    _require(settled_runtime_metrics.get("cache_loading") is False and settled_runtime_metrics.get("upcoming_range_loaded") is True, f"{route['routeId']} Calendar cache was not loaded and settled before teardown")
    expected_refresh_count = 1 if successful else 0
    _require(int(terminal_runtime_metrics.get("calendar_import_cache_refresh_count", -1)) == expected_refresh_count, f"{route['routeId']} terminal Calendar cache refresh count is invalid")
    _require(int(settled_runtime_metrics.get("calendar_import_cache_refresh_count", -1)) == expected_refresh_count, f"{route['routeId']} settled Calendar cache refresh count is invalid")
    _require(metrics.get("terminal_status") == ("completed" if successful else "failed"), f"{route['routeId']} terminal service status is invalid")
    _require(metrics.get("phase") == ("completed" if successful else "failed"), f"{route['routeId']} service phase is invalid")
    _require(int(metrics.get("downloaded_bytes", -1)) == int(fixture["sizeBytes"]), f"{route['routeId']} downloaded byte count differs from the served fixture")
    _require(int(metrics.get("transfer_chunk_count", 0)) > 0 and int(metrics.get("largest_transfer_chunk_bytes", 0)) <= 65_536, f"{route['routeId']} transfer chunk metrics are invalid")
    _require(metrics.get("used_device_parser") is True, f"{route['routeId']} did not use the device workbook parser")
    _require(metrics.get("staged_workbook_removed_at_terminal") is True, f"{route['routeId']} did not report terminal staged-file removal")
    _require(metrics.get("staged_workbook_removed_on_cancel") is False, f"{route['routeId']} unexpectedly canceled the import")
    _require(result.get("tempFilesAfterResult") == [], f"{route['routeId']} isolated TEMP was not empty after staged-file cleanup")
    staging_directory = _artifact_path(
        result.get("stagingDirectory"), run_directory, f"{route['routeId']} isolated staging directory"
    )
    expected_staging_directory = Path(str(route.get("workspace", {}).get("runtimeDirectory", ""))).resolve(strict=True) / "temp"
    _require(staging_directory == expected_staging_directory.resolve(strict=True), f"{route['routeId']} staging directory is not isolated to its process")
    _require(not any(staging_directory.iterdir()), f"{route['routeId']} staging directory retained a file or directory after the terminal result")

    ui = result.get("ui", {})
    _require(ui.get("processId") == int(route["processIdentity"]["pid"]), f"{route['routeId']} import status UIA is not process-bound")
    _require(ui.get("dialogProcessId") == int(route["processIdentity"]["pid"]), f"{route['routeId']} import preferences dialog belongs to another process")
    _require(int(ui.get("dialogWindowHandle", 0)) > 0 and int(ui.get("dialogWindowProcessId", -1)) == int(route["processIdentity"]["pid"]), f"{route['routeId']} import dialog HWND is not process-bound")
    _require(ui.get("importButtonEnabledAfterResult") is True, f"{route['routeId']} import control was not re-enabled after the result")
    if successful:
        _require(int(metrics.get("streamed_worksheet_cell_count", -1)) == expected["worksheetCellCount"] == 382, "success Next parser cell count differs from fixture source cells")
        _require(int(metrics.get("materialized_worksheet_cell_count", -1)) == 0, "success Next parser materialized worksheet cells")
        _require(int(metrics.get("parsed_event_count", -1)) == len(expected["events"]) == 261, "success Next parser event count differs from fixture-derived events")
        _require(int(metrics.get("typed_candidate_count_at_apply", -1)) == 261, "success Next typed candidate count differs from parsed events")
        _require(int(metrics.get("legacy_event_count_at_apply", -1)) == 0, "success Next import retained legacy event objects at apply")
        _require(int(metrics.get("imported_event_count", -1)) == 261 and int(metrics.get("skipped_event_count", -1)) == expected["skippedCount"] == 104, "success Next imported/skipped counts differ from the fixture-derived counts")
        _require(metrics.get("candidate_rows_sha256") == _candidate_hash(expected["events"]), "success Next parser candidate row hash differs from fixture bytes")
        _require(metrics.get("staged_workbook_released_before_apply") is True, "success import did not release its staged workbook before typed apply")
        _require(re.search(r"Imported 261 event\(s\).*Skipped 104", str(ui.get("statusText", "")), re.IGNORECASE) is not None, "success UIA did not show the exact imported/skipped result")
        _require(ui.get("failureText") in (None, ""), "success import exposed an error message")
    else:
        _require(int(metrics.get("parsed_event_count", -1)) == 0 and int(metrics.get("typed_candidate_count_at_apply", -1)) == 0, "parser-error route parsed or applied Calendar events")
        _require(int(metrics.get("legacy_event_count_at_apply", -1)) == 0 and int(metrics.get("imported_event_count", -1)) == 0 and int(metrics.get("skipped_event_count", -1)) == 0, "parser-error route reported applied or skipped event counts")
        _require(metrics.get("candidate_rows_sha256") in (None, ""), "parser-error route recorded candidate rows")
        _require(metrics.get("staged_workbook_released_before_apply") is True, "parser-error route did not release the staged workbook before typed apply")
        error_text = str(ui.get("statusText", ""))
        _require("xl/workbook.xml" in error_text.lower(), "parser-error UI did not name missing xl/workbook.xml")
        _require(str(ui.get("failureText", "")).strip() and str(ui.get("failureText", "")) in error_text, "parser-error service/UI failure text differs")
    return metrics


def validate(evidence_directory: Path) -> dict[str, Any]:
    run_directory = evidence_directory.resolve(strict=True)
    manifest_path = run_directory / "manifest.json"
    manifest = _read_json(manifest_path)
    evidence_relocation = _validate_evidence_relocation(manifest, run_directory)
    _require(manifest.get("schema") == MANIFEST_SCHEMA, "manifest schema is invalid")
    _require(manifest.get("taskId") == "P9_10_PACKAGED_CALENDAR_IMPORT_ACCEPTANCE", "manifest task ID is invalid")
    _require(manifest.get("status") == "evidence-collected", "runner did not complete evidence collection")
    _require(manifest.get("routePlan") == "two-route-acceptance", "manifest is not a complete two-route acceptance attempt")
    _require(manifest.get("routeMode") == "current-source-packaged-next-on-local-windows", "evidence is not a packaged Next Windows run")
    _require(manifest.get("networkPolicy") == "loopback-only; startup updater disabled in the test package", "run did not enforce the offline packaged route")
    thresholds = manifest.get("thresholds", {})
    _require(int(thresholds.get("normalUseWorkingSetBytesExclusive", -1)) == WORKING_SET_LIMIT and thresholds.get("strictAtOrAboveFails") is True, "strict normal-use working-set limit differs")
    _require(int(thresholds.get("operationPeakWorkingSetBytesAdvisoryExclusive", -1)) == OPERATION_PEAK_LIMIT and thresholds.get("operationPeakEvaluation") == "advisory-only; no private-byte return allowance is defined", "operation peak must remain advisory")
    _require(thresholds.get("privateBytesReturnAllowance") is None, "private-byte return allowance must remain undefined")

    runner_identity = manifest.get("runnerIdentity", {})
    validator_identity = manifest.get("validatorIdentity", {})
    for label, identity in (("runner", runner_identity), ("validator", validator_identity)):
        path = Path(str(identity.get("path", ""))).resolve(strict=True)
        _require(_sha256(path) == identity.get("sha256"), f"{label} script identity hash mismatch")
    _require(Path(str(validator_identity.get("path"))).resolve() == Path(__file__).resolve(), "manifest validator identity points to a different validator")

    source_document = _read_json(_artifact_path(manifest.get("sourceInventoryPath"), run_directory, "source inventory"))
    package_document = _read_json(_artifact_path(manifest.get("packageInventoryPath"), run_directory, "package inventory"))
    source_rows, package_rows = source_document.get("files"), package_document.get("files")
    _require(isinstance(source_rows, list) and isinstance(package_rows, list), "source/package inventories are malformed")
    source_root = Path(str(manifest.get("source", {}).get("root", ""))).resolve(strict=True)
    _require(source_root == Path(str(manifest.get("repositoryRoot", ""))).resolve(strict=True), "source root differs from repository root")
    _require(_inventory_hash(source_rows, "source inventory") == manifest.get("source", {}).get("sha256"), "source inventory fingerprint mismatch")
    source_fingerprint = str(manifest.get("source", {}).get("sha256", ""))
    validator_revision = manifest.get("validationRevision")
    _verify_source_inventory_with_validator_revision(source_rows, source_root, source_fingerprint, validator_revision)
    if validator_revision is not None:
        revised_validator = next(
            item for item in validator_revision["files"]
            if item.get("path") == "scripts/phase9/validate_packaged_next_calendar_import.py"
        )
        _require(
            validator_identity.get("sha256") == revised_validator.get("currentSha256"),
            "manifest validator identity differs from its separately recorded post-measurement revision",
        )
    package = manifest.get("package", {})
    package_root = Path(str(package.get("directory", ""))).resolve(strict=True)
    _require(_inventory_hash(package_rows, "package inventory") == package.get("sha256"), "package inventory fingerprint mismatch")
    _verify_inventory_files(package_rows, package_root, "packaged Next application")
    app_path = Path(str(manifest.get("application", {}).get("canonicalPath", ""))).resolve(strict=True)
    _require(app_path == (package_root / "ClassMngrNext.exe").resolve(strict=True), "application path is not the staged package executable")
    _require(_sha256(app_path) == manifest.get("application", {}).get("sha256") == package.get("applicationSha256"), "staged app hash does not match package identity")
    _require(manifest.get("application", {}).get("architecture") == "x64", "staged application is not x64")
    _require(manifest.get("build", {}).get("succeeded") is True and int(manifest["build"].get("exitCode", -1)) == 0, "current-source Release build failed")
    _require(manifest.get("build", {}).get("startupUpdateCheck") == "OFF", "current-source package did not disable external startup update checks")
    build_log = _artifact_path(manifest.get("build", {}).get("logPath"), run_directory, "Release build log")
    _require(_sha256(build_log) == manifest.get("build", {}).get("logSha256"), "Release build log identity mismatch")
    cache_path = Path(str(manifest.get("build", {}).get("cachePath", ""))).resolve(strict=True)
    _require(_sha256(cache_path) == manifest.get("build", {}).get("cacheSha256After"), "Release CMake cache hash changed after packaging")
    build = manifest["build"]
    if build.get("reusedWithoutRebuild") is True:
        allowed_script_delta = {
            "scripts/phase9/run_packaged_next_calendar_import.ps1",
            "scripts/phase9/validate_packaged_next_calendar_import.py",
            "scripts/phase9/test_packaged_calendar_import_harness.ps1",
        }
        derived_receipt_path = _artifact_path(build.get("prebuiltBuildReceiptPath"), run_directory, "derived Release build receipt")
        raw_receipt_path = _artifact_path(build.get("rawPrebuiltBuildReceiptPath"), run_directory, "raw Release build receipt copy")
        delta_path = _artifact_path(build.get("postBuildSourceDeltaEvidencePath"), run_directory, "post-build source delta")
        current_inventory_path = _artifact_path(build.get("postBuildCurrentInventoryPath"), run_directory, "post-build source inventory")
        raw_inventory_path = _artifact_path(build.get("rawBuildSourceInventoryPath"), run_directory, "raw build source inventory")
        _require(_sha256(derived_receipt_path) == build.get("prebuiltBuildReceiptSha256"), "derived Release receipt identity mismatch")
        _require(_sha256(raw_receipt_path) == build.get("rawPrebuiltBuildReceiptSha256") == build.get("rawPrebuiltBuildReceiptOriginalSha256"), "raw Release receipt identity mismatch")
        _require(_sha256(delta_path) == build.get("postBuildSourceDeltaEvidenceSha256"), "post-build source delta identity mismatch")
        _require(_sha256(current_inventory_path) == build.get("postBuildCurrentInventorySha256"), "post-build source inventory identity mismatch")
        _require(_sha256(raw_inventory_path) == build.get("rawBuildSourceInventorySha256"), "raw build source inventory identity mismatch")
        derived_receipt = _read_json(derived_receipt_path)
        raw_receipt = _read_json(raw_receipt_path)
        delta_document = _read_json(delta_path)
        current_inventory = _read_json(current_inventory_path)
        raw_inventory = _read_json(raw_inventory_path)
        raw_source_sha = str(build.get("buildSourceSha256", ""))
        current_source_sha = str(manifest.get("source", {}).get("sha256", ""))
        _require(raw_source_sha and raw_source_sha == raw_receipt.get("sourceSha256") == derived_receipt.get("sourceSha256") == derived_receipt.get("buildSourceSha256"), "raw build fingerprint changed across the derived receipt")
        _require(current_source_sha == derived_receipt.get("postBuildSourceSha256") == derived_receipt.get("currentAcceptanceSourceSha256"), "derived receipt does not bind the current acceptance source")
        _require(derived_receipt.get("reusedWithoutRebuild") is True, "derived receipt does not state reuse without rebuild")
        _require(raw_receipt.get("releaseExecutableSha256") == derived_receipt.get("releaseExecutableSha256") == manifest.get("application", {}).get("releaseExecutableSha256"), "derived receipt changed the Release executable identity")
        _require(raw_receipt.get("cacheSha256After") == build.get("cacheSha256After"), "raw Release receipt cache identity differs from the packaged run")
        _require(delta_document.get("rawBuildReceiptSha256") == build.get("rawPrebuiltBuildReceiptOriginalSha256"), "source delta is not bound to the raw Release receipt")
        _require(delta_document.get("rawBuildSourceSha256") == raw_source_sha and delta_document.get("currentSourceSha256") == current_source_sha, "source delta fingerprints differ from the build and current source")
        _require(delta_document.get("productBuildInputsUnchanged") is True and delta_document.get("changedFilesAreAcceptanceHarnessOnly") is True, "source delta does not attest an unchanged product build surface")
        raw_rows = raw_inventory.get("files")
        current_rows = current_inventory.get("files")
        _require(isinstance(raw_rows, list) and isinstance(current_rows, list), "raw or current source inventory is malformed")
        _require(_inventory_hash(raw_rows, "raw build source inventory") == raw_source_sha == raw_inventory.get("sha256"), "raw build source inventory fingerprint mismatch")
        _require(_inventory_hash(current_rows, "current acceptance source inventory") == current_source_sha == current_inventory.get("sha256"), "current acceptance source inventory fingerprint mismatch")
        _require(current_rows == source_rows, "post-build inventory rows differ from the run's current source inventory")
        raw_by_path = {str(row["path"]): row for row in raw_rows}
        current_by_path = {str(row["path"]): row for row in current_rows}
        actual_changed = {
            path for path in set(raw_by_path) | set(current_by_path)
            if raw_by_path.get(path) != current_by_path.get(path)
        }
        delta_rows = delta_document.get("changedPaths")
        _require(isinstance(delta_rows, list), "source delta changed-path evidence is malformed")
        recorded_changed = {str(row.get("path", "")) for row in delta_rows}
        _require(actual_changed == recorded_changed and actual_changed, "source delta changed paths do not match raw/current inventory rows")
        _require(actual_changed <= allowed_script_delta, f"post-build changes escaped the runner/validator/harness: {sorted(actual_changed - allowed_script_delta)}")
        for row in delta_rows:
            path = str(row.get("path", ""))
            _require(row.get("before") == raw_by_path.get(path) and row.get("after") == current_by_path.get(path), f"source delta row differs from inventories: {path}")
        _require(derived_receipt.get("postBuildSourceDeltaPaths") == sorted(actual_changed), "derived receipt changed-path list differs from verified inventories")
        _require(derived_receipt.get("rawBuildReceiptPath") == str(raw_receipt_path), "derived receipt does not point at the local raw receipt copy")
        _require(derived_receipt.get("postBuildSourceDeltaEvidencePath") == str(delta_path), "derived receipt does not point at the local source-delta copy")
        _require(derived_receipt.get("postBuildCurrentInventoryPath") == str(current_inventory_path), "derived receipt does not point at the local current inventory copy")
        _require(derived_receipt.get("rawBuildSourceInventoryPath") == str(raw_inventory_path), "derived receipt does not point at the local raw inventory copy")
        generated = derived_receipt.get("generatedResources", {})
        staged = generated.get("stagedResourceInputs", {})
        staged_root = Path(str(build.get("verifiedResourceInputsRoot", ""))).resolve(strict=True)
        staged_rows = staged.get("files")
        _require(isinstance(staged_rows, list) and staged.get("root") == str(staged_root), "verified Release resource stage receipt is malformed")
        _require(int(staged.get("fileCount", -1)) == len(staged_rows) == int(build.get("verifiedResourceInputsFileCount", -2)), "verified Release resource stage count differs")
        _require(_inventory_hash(staged_rows, "verified Release resource stage") == staged.get("sha256") == build.get("verifiedResourceInputsInventorySha256"), "verified Release resource stage fingerprint differs")
        _verify_inventory_files(staged_rows, staged_root, "verified Release resource stage")
        _require(generated.get("currentUserSid") == build.get("verifiedResourceInputsUserSid"), "verified Release resource stage SID differs from the run")
        package_by_path = {str(row["path"]): row for row in package_rows}
        for row in staged_rows:
            _require(package_by_path.get(str(row["path"])) == row, f"packaged resources differ from the verified Release stage: {row['path']}")
    else:
        _require(build.get("reusedWithoutRebuild") is False, "build reuse mode is absent or invalid")

    fixtures = manifest.get("fixtures")
    _require(isinstance(fixtures, list) and len(fixtures) == 2, "manifest does not contain both exact Calendar response fixtures")
    by_name = {str(item.get("name")): item for item in fixtures}
    _require(set(by_name) == {SUCCESS_FIXTURE_NAME, ERROR_FIXTURE_NAME}, "fixture names differ from the P9-10 accepted pair")
    for name, expected_hash, expected_relative in (
        (SUCCESS_FIXTURE_NAME, SUCCESS_FIXTURE_SHA256, EXPECTED_SUCCESS_SOURCE),
        (ERROR_FIXTURE_NAME, ERROR_FIXTURE_SHA256, EXPECTED_ERROR_SOURCE),
    ):
        item = by_name[name]
        source_path = Path(str(item.get("sourcePath", ""))).resolve(strict=True)
        evidence_path = _artifact_path(item.get("evidencePath"), run_directory, f"{name} evidence copy")
        _require(source_path == (source_root / Path(expected_relative)).resolve(strict=True), f"{name} source path differs from the accepted fixture")
        _require(_sha256(source_path) == _sha256(evidence_path) == item.get("sha256") == expected_hash, f"{name} response bytes differ from accepted source bytes")
        _require(evidence_path.stat().st_size == int(item.get("sizeBytes", -1)), f"{name} fixture size differs from its recorded size")
    expected_source = parse_success_fixture(Path(by_name[SUCCESS_FIXTURE_NAME]["evidencePath"]))
    error_fixture = Path(by_name[ERROR_FIXTURE_NAME]["evidencePath"])
    _require(error_fixture.stat().st_size == 69, "parser-error fixture is not the accepted 69-byte response")

    routes = manifest.get("routes")
    _require(isinstance(routes, list) and len(routes) == 2, "manifest must contain exactly two fresh packaged processes")
    route_by_id = {str(route.get("routeId")): route for route in routes}
    _require(set(route_by_id) == {"route-a-large-calendar-success", "route-b-malformed-calendar-error"}, "route IDs or route count differ from P9-10")
    identities = [route.get("processIdentity", {}) for route in routes]
    pids = [int(identity.get("pid", 0)) for identity in identities]
    _require(all(pid > 0 for pid in pids) and len(set(pids)) == 2, "success and error did not use fresh distinct PIDs")
    workspace_hashes = [str(route.get("workspace", {}).get("sha256AtLaunch")) for route in routes]
    workspace_paths = [str(Path(str(route.get("workspace", {}).get("path", ""))).resolve()) for route in routes]
    _require(len(set(workspace_paths)) == 2 and len(set(workspace_hashes)) == 1, "success/error did not use identical seeded workspace copies")
    seed = manifest.get("workspaceSeed", {})
    _require(int(seed.get("calendarEventCount", -1)) == 180 and all(item == seed.get("sha256") for item in workspace_hashes), "route workspaces do not match the 180-event seeded source copy")
    seed_sql = Path(str(seed.get("seedSqlPath", ""))).resolve(strict=True)
    _require(seed_sql == (source_root / "tests/fixtures/workspaces/large_startup.sql").resolve(strict=True) and _sha256(seed_sql) == seed.get("seedSqlSha256"), "seeded workspace source identity mismatch")

    actions = _read_jsonl(_artifact_path(manifest.get("uiaActionsPath"), run_directory, "UIA action log"))
    action_counts = _validate_uia_actions(actions, route_by_id)
    server_rows = _read_jsonl(_artifact_path(manifest.get("serverLogPath"), run_directory, "loopback server log"))
    process_metrics = _read_jsonl(_artifact_path(manifest.get("processMetricsPath"), run_directory, "process metric log"))
    memory_results: list[dict[str, Any]] = []
    summary_routes: dict[str, Any] = {}
    baseline_hashes: list[str] = []
    for route_id, route in route_by_id.items():
        successful = route_id == "route-a-large-calendar-success"
        fixture_name = SUCCESS_FIXTURE_NAME if successful else ERROR_FIXTURE_NAME
        fixture = route.get("fixture", {})
        _require(fixture.get("sha256") == by_name[fixture_name].get("sha256"), f"{route_id} fixture identity mismatch")
        identity = route.get("processIdentity", {})
        _require(int(identity.get("architectureBits", 0)) == 64 and int(identity.get("pid", 0)) == pids[0 if successful else 1], f"{route_id} process identity is invalid")
        _require(Path(str(identity.get("canonicalExecutablePath", ""))).resolve() == app_path and identity.get("sha256") == _sha256(app_path), f"{route_id} process did not execute the measured current-source package")
        _require(int(identity.get("mainWindowHandle", 0)) > 0 and int(identity.get("mainWindowProcessId", -1)) == int(identity["pid"]), f"{route_id} main HWND does not belong to the measured PID")
        shutdown_summary = _validate_shutdown_observability(route, run_directory, evidence_relocation)
        _require(int(route.get("processExitCode", -1)) == 0, f"{route_id} app did not exit cleanly")
        _timestamp(identity.get("createdAtUtc"), f"{route_id} process creation")
        _require(route.get("startupReady", {}).get("record", {}).get("sample_available") is True, f"{route_id} startup checkpoint is not an available process sample")
        _require(route.get("startupReady", {}).get("record", {}).get("platform") == "windows", f"{route_id} did not use a Windows package run")
        _require(route.get("startupReady", {}).get("record", {}).get("outcome") == "completed", f"{route_id} startup-ready checkpoint failed")
        _require(route.get("calendarBaseline", {}).get("pageAutomationId", "").endswith("nextCalendarPage"), f"{route_id} Calendar baseline UIA identity is missing")
        _require(route.get("calendarBaseline", {}).get("processId") == int(identity["pid"]) and route.get("calendarBaseline", {}).get("windowProcessId") == int(identity["pid"]), f"{route_id} Calendar baseline UIA is not bound to its process")

        server = route.get("server", {})
        _require(server.get("implementation") == "in-process loopback TCP HTTP fixture server", f"{route_id} response was not served locally")
        _require(server.get("host") == "127.0.0.1" and str(server.get("url", "")).startswith("http://127.0.0.1:"), f"{route_id} server is not loopback-only")
        _require(int(server.get("requestCount", -1)) == 1 and server.get("requestPath") == "/calendar-import", f"{route_id} made unexpected local server requests")
        _require(server.get("remoteAddress") == "127.0.0.1", f"{route_id} fixture request came from outside loopback")
        _require(int(server.get("responseBytesSent", -1)) == int(fixture["sizeBytes"]) and server.get("responseSha256") == fixture.get("sha256"), f"{route_id} loopback response bytes differ from its fixture")
        _require(server.get("responseMode") == "full-fixture-response-with-no-staging-visibility-gate", f"{route_id} used an unexpected response mode")
        _require(server.get("responseCompleted") is True and server.get("aborted") is False, f"{route_id} did not complete the full loopback response")
        matching_servers = [row for row in server_rows if row.get("routeId") == route_id]
        _require(
            len(matching_servers) == 1
            and matching_servers[0].get("responseSha256") == fixture.get("sha256")
            and int(matching_servers[0].get("responseBytesSent", -1)) == int(fixture["sizeBytes"])
            and matching_servers[0].get("responseCompleted") is True
            and matching_servers[0].get("aborted") is False,
            f"{route_id} raw server evidence is missing or differs",
        )

        lifecycle = _validate_route_lifecycle(route, successful)
        import_metrics = _validate_import_metrics(route, fixture, expected_source, successful, run_directory)
        _require(route.get("calendarLoadWait", {}).get("completed") is True, f"{route_id} did not wait for Calendar cache/UI load completion after re-entry")
        if successful:
            _require(int(route["calendarLoadWait"].get("upcomingItemCount", 0)) > 0, "success re-entry did not render imported Calendar events")
        _require(route.get("calendarLoadWait", {}).get("loadingIndicatorsRemaining") == 0, f"{route_id} retained a Calendar loading indicator after the re-entry wait")

        snapshots = _validate_workspace_pair(
            route,
            run_directory,
            expected_source["events"],
            successful=successful,
            relocation=evidence_relocation,
        )
        baseline_hashes.append(str(route["workspace"].get("beforeTableContentSha256")))
        _require(route.get("workspace", {}).get("beforeCalendarEventCount") == 180, f"{route_id} baseline snapshot did not contain 180 events")
        if successful:
            _require(snapshots.get("persistedEventRowsSha256") == import_metrics.get("candidate_rows_sha256"), "persisted event row content hash differs from the typed Next candidates")
            _require(route.get("workspace", {}).get("afterCalendarEventCount") == 441, "success persisted event row count is not 441")
        else:
            _require(route.get("workspace", {}).get("afterCalendarEventCount") == 180, "parser error changed the existing Calendar event count")

        lifecycle_summary = {
            "suspendCount": int(route["lifecycle"]["suspendCount"]),
            "reentryCount": int(route["lifecycle"]["reentryCount"]),
            "cacheRefreshCount": int(lifecycle["reentered"].get("calendar_import_cache_refresh_count", -1)),
            "cacheEventCountAtReentry": int(lifecycle["reentered"].get("cache_event_count", -1)),
            "calendarLoadWait": route["calendarLoadWait"],
        }
        memory = _validate_memory(route)
        memory_results.append({"routeId": route_id, **memory})
        summary_routes[route_id] = {
            "outcome": "large-calendar-import-applied" if successful else "malformed-workbook-rejected-with-no-writes",
            "pid": int(identity["pid"]),
            "mainWindowHandle": int(identity["mainWindowHandle"]),
            "fixture": {"name": fixture["name"], "sizeBytes": int(fixture["sizeBytes"]), "sha256": fixture["sha256"]},
            "downloadedBytes": int(import_metrics["downloaded_bytes"]),
            "parsedEventCount": int(import_metrics["parsed_event_count"]),
            "importedEventCount": int(import_metrics["imported_event_count"]),
            "skippedEventCount": int(import_metrics["skipped_event_count"]),
            "stagedWorkbookRemoved": bool(import_metrics["staged_workbook_removed_at_terminal"]),
            "rowLevelWorkspace": snapshots,
            "lifecycle": lifecycle_summary,
            "memory": memory,
            "shutdown": shutdown_summary,
        }

    _require(baseline_hashes[0] == baseline_hashes[1], "success and error route baseline workspace rows differ")
    route_ids = set(route_by_id)
    _require(all(row.get("routeId") in route_ids and row.get("pid") == route_by_id[row["routeId"]]["processIdentity"]["pid"] for row in process_metrics), "process metric log contains a foreign PID")
    for row in process_metrics:
        for key in ("workingSetBytes", "privateBytes", "commitBytes", "peakWorkingSetBytes", "handles", "threads"):
            _require(isinstance(row.get(key), int) and int(row[key]) > 0, f"process metric {row.get('point')} lacks {key}")

    normal_violations = [violation for result in memory_results for violation in result["violations"]]
    peak_results = [peak for result in memory_results for peak in result["operationPeaks"]]
    budget_passed = not normal_violations
    result = {
        "schema": SUMMARY_SCHEMA,
        "taskId": "P9_10_PACKAGED_CALENDAR_IMPORT_ACCEPTANCE",
        "status": "passed" if budget_passed else "failed-budget",
        "validatedAtUtc": datetime.now().astimezone().isoformat(),
        "evidenceDirectory": str(run_directory),
        "sourceFingerprintSha256": manifest["source"]["sha256"],
        "packageSha256": package["sha256"],
        "applicationSha256": manifest["application"]["sha256"],
        "fixtureDerivedExpectations": {
            "successFixtureSha256": SUCCESS_FIXTURE_SHA256,
            "worksheetCellCount": expected_source["worksheetCellCount"],
            "calendarWorksheetCellCount": expected_source["calendarWorksheetCellCount"],
            "archiveWorksheetCellCount": expected_source["archiveWorksheetCellCount"],
            "weekdayImportedEventCount": len(expected_source["events"]),
            "weekendSkippedCount": expected_source["skippedCount"],
            "eventTypeCounts": expected_source["eventTypeCounts"],
        },
        "routeCount": len(routes),
        "routes": summary_routes,
        "uiaActionCounts": action_counts,
        "memoryBudget": {
            "normalUseWorkingSetExclusiveBytes": WORKING_SET_LIMIT,
            "passed": budget_passed,
            "violations": normal_violations,
            "operationPeakAdvisoryExclusiveBytes": OPERATION_PEAK_LIMIT,
            "operationPeaks": peak_results,
            "privateBytesReturnAllowance": None,
        },
    }
    if manifest.get("validationHistory") is not None:
        _validate_revalidation_reporting(manifest, run_directory, result)
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-dir", required=True, type=Path)
    options = parser.parse_args(argv)
    try:
        result = validate(options.evidence_dir)
        summary_path = options.evidence_dir.resolve(strict=True) / "summary.json"
        summary_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"status": result["status"], "summaryPath": str(summary_path), "routeCount": result["routeCount"]}, ensure_ascii=False, sort_keys=True))
        return 0 if result["status"] == "passed" else 2
    except EvidenceError as exc:
        print(json.dumps({"status": "invalid-evidence", "error": str(exc)}, ensure_ascii=False, sort_keys=True))
        return 1
    except (OSError, ValueError, KeyError, TypeError, zipfile.BadZipFile, ET.ParseError) as exc:
        print(json.dumps({"status": "invalid-evidence", "error": f"unexpected validation error: {exc}"}, ensure_ascii=False, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
