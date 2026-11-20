#!/usr/bin/env python3
"""Validate P9-13 current Next speaking-evaluation packaged lifecycle evidence."""

from __future__ import annotations

import argparse
import configparser
import ctypes
import gc
import hashlib
import json
import math
import os
import re
import sqlite3
import struct
import sys
import zipfile
import zlib
from pathlib import Path
from typing import Any, Iterable


TASK_ID = "P9_13_NEXT_SPEAKING_REPORT_PACKAGED_LIFECYCLE"
SCHEMA = "classmngr-phase9-packaged-next-speaking-evaluation-lifecycle-v3"
VALIDATION_SCHEMA = "classmngr-phase9-packaged-next-speaking-evaluation-lifecycle-validation-v3"
WORKSPACE_SCHEMA = "classmngr-phase9-speaking-evaluation-workspace-preparation-v2"
P912_TASK_ID = "P9_12_NEXT_QTPDF_PACKAGED_LIFECYCLE"
WORKING_SET_LIMIT = 262_144_000
RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT = 805_306_368
RUNNER_QUERY_BURST_WORKING_SET_LIMIT = 805_306_368
EXPECTED_WORKSPACE_STUDENT_COUNT = 30
EXPECTED_REPORT_COUNT = 25
DEFAULT_MAXIMUM_ROSTER_ROWS = 25
EXPECTED_NEW_SCRIPTS = {
    "scripts/phase9/prepare_speaking_evaluation_workspace.py",
    "scripts/phase9/run_packaged_next_speaking_evaluation_lifecycle.ps1",
    "scripts/phase9/validate_packaged_next_speaking_evaluation_lifecycle.py",
    "scripts/phase9/test_validate_packaged_next_speaking_evaluation_lifecycle.py",
}
REQUIRED_UIA_SNAPSHOTS = {
    "startup-ready",
    "speaking-evaluation-page-ready",
    "direct-preview-open",
    "direct-preview-closed",
    "batch-dialog-ready",
    "batch-preview-open",
    "batch-preview-closed",
    "export-complete",
    "navigated-away",
}
REQUIRED_NORMAL_POINTS = {
    "startup-ready",
    "speaking-evaluation-page-ready",
    "direct-preview-closed",
    "batch-preview-closed",
    "export-complete",
    "navigated-away",
}


class ValidationFailure(Exception):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationFailure(message)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_sha256(value: Any) -> str:
    payload = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return sha256_bytes(payload.encode("utf-8"))


def selected_export_report_names(report_names: list[str]) -> list[str]:
    require(
        len(report_names) == EXPECTED_WORKSPACE_STUDENT_COUNT,
        "workspace must retain all 30 selected evaluation rows",
    )
    return list(report_names[:EXPECTED_REPORT_COUNT])


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except Exception as error:
        raise ValidationFailure(f"cannot read JSON evidence {path}: {error}") from error
    require(isinstance(value, dict), f"JSON evidence root is not an object: {path}")
    return value


def inventory_fingerprint(rows: Iterable[dict[str, Any]]) -> str:
    ordered = sorted(
        rows,
        key=lambda row: str(row["path"]).lower(),
    )
    canonical = "".join(
        f"{row['path']}\t{int(row['sizeBytes'])}\t{str(row['sha256']).lower()}\n"
        for row in ordered
    ) or "\n"
    return sha256_bytes(canonical.encode("utf-8"))


def table_rows_hash(connection: sqlite3.Connection, table: str, order_by: str) -> str:
    escaped_table = table.replace('"', '""')
    escaped_order = order_by.replace('"', '""')
    columns = [
        str(row[1])
        for row in connection.execute(f'PRAGMA table_info("{escaped_table}")')
    ]
    rows = connection.execute(
        f'SELECT * FROM "{escaped_table}" ORDER BY {escaped_order}'
    ).fetchall()
    payload = json.dumps(
        {"columns": columns, "rows": rows},
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return sha256_bytes(payload)


def _decode_pdf_literal(data: bytes) -> str:
    output = bytearray()
    index = 0
    while index < len(data):
        byte = data[index]
        if byte != 0x5C:
            output.append(byte)
            index += 1
            continue
        index += 1
        if index >= len(data):
            break
        escaped = data[index]
        index += 1
        simple = {
            ord("n"): 0x0A,
            ord("r"): 0x0D,
            ord("t"): 0x09,
            ord("b"): 0x08,
            ord("f"): 0x0C,
            ord("("): 0x28,
            ord(")"): 0x29,
            ord("\\"): 0x5C,
        }
        if escaped in simple:
            output.append(simple[escaped])
        elif escaped in (0x0A, 0x0D):
            if escaped == 0x0D and index < len(data) and data[index] == 0x0A:
                index += 1
        elif 0x30 <= escaped <= 0x37:
            digits = bytearray([escaped])
            for _ in range(2):
                if index < len(data) and 0x30 <= data[index] <= 0x37:
                    digits.append(data[index])
                    index += 1
                else:
                    break
            output.append(int(digits.decode("ascii"), 8) & 0xFF)
        else:
            output.append(escaped)
    raw = bytes(output)
    if raw.startswith(b"\xfe\xff"):
        return raw[2:].decode("utf-16-be", errors="replace")
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("cp1252", errors="replace")


def _parse_pdf_streams(data: bytes) -> tuple[dict[int, bytes], dict[int, bytes]]:
    objects: dict[int, bytes] = {}
    streams: dict[int, bytes] = {}
    object_bodies: dict[tuple[int, int], bytes] = {}
    object_matches = list(
        re.finditer(rb"(?ms)(\d+)\s+(\d+)\s+obj\b(.*?)endobj", data)
    )
    for match in object_matches:
        object_id = int(match.group(1))
        generation = int(match.group(2))
        body = match.group(3)
        objects[object_id] = body
        object_bodies[(object_id, generation)] = body

    for match in object_matches:
        object_id = int(match.group(1))
        body = match.group(3)
        stream_marker = re.search(rb"\bstream\r?\n", body)
        if stream_marker is None:
            continue
        stream_start = stream_marker.end()
        dictionary = body[: stream_marker.start()]
        length_match = re.search(rb"/Length\s+(\d+)\s+(\d+)\s+R\b", dictionary)
        if length_match:
            length_object_id = int(length_match.group(1))
            length_generation = int(length_match.group(2))
            length_body = object_bodies.get((length_object_id, length_generation))
            if length_body is None:
                raise ValidationFailure(
                    f"PDF object {object_id} references missing stream-length object "
                    f"{length_object_id} {length_generation}"
                )
            indirect_length = re.fullmatch(rb"\s*(\d+)\s*", length_body)
            if indirect_length is None:
                raise ValidationFailure(
                    f"PDF stream-length object {length_object_id} "
                    f"{length_generation} is not a direct integer"
                )
            length = int(indirect_length.group(1))
            raw = body[stream_start : stream_start + length]
            if len(raw) != length:
                raise ValidationFailure(
                    f"PDF object {object_id} stream is shorter than its indirect /Length"
                )
        else:
            length_match = re.search(rb"/Length\s+(\d+)\b", dictionary)
            if length_match:
                length = int(length_match.group(1))
                raw = body[stream_start : stream_start + length]
                if len(raw) != length:
                    raise ValidationFailure(
                        f"PDF object {object_id} stream is shorter than its /Length"
                    )
            else:
                end = body.find(b"endstream", stream_start)
                if end < 0:
                    continue
                raw = body[stream_start:end].rstrip(b"\r\n")
        if b"/FlateDecode" in dictionary:
            try:
                raw = zlib.decompress(raw)
            except zlib.error as error:
                raise ValidationFailure(
                    f"PDF object {object_id} has an unreadable FlateDecode stream: {error}"
                ) from error
        streams[object_id] = raw
    return objects, streams


def _unicode_cmap(streams: dict[int, bytes]) -> dict[bytes, str]:
    mapping: dict[bytes, str] = {}
    for stream in streams.values():
        for section in re.finditer(
            rb"beginbfchar(.*?)endbfchar", stream, flags=re.DOTALL
        ):
            for pair in re.finditer(rb"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>", section.group(1)):
                source = bytes.fromhex(pair.group(1).decode("ascii"))
                destination = bytes.fromhex(pair.group(2).decode("ascii"))
                try:
                    mapping[source] = destination.decode("utf-16-be")
                except UnicodeDecodeError:
                    continue
        for section in re.finditer(
            rb"beginbfrange(.*?)endbfrange", stream, flags=re.DOTALL
        ):
            body = section.group(1)
            for row in re.finditer(
                rb"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*(<([0-9A-Fa-f]+)>|\[(.*?)\])",
                body,
                flags=re.DOTALL,
            ):
                first = int(row.group(1), 16)
                last = int(row.group(2), 16)
                source_width = len(row.group(1)) // 2
                if row.group(4) is not None:
                    destination = int(row.group(4), 16)
                    destination_width = len(row.group(4)) // 2
                    for offset, code in enumerate(range(first, min(last, first + 4095) + 1)):
                        source = code.to_bytes(source_width, "big")
                        target = (destination + offset).to_bytes(destination_width, "big")
                        try:
                            mapping[source] = target.decode("utf-16-be")
                        except UnicodeDecodeError:
                            continue
                else:
                    destinations = re.findall(rb"<([0-9A-Fa-f]+)>", row.group(5) or b"")
                    for offset, destination_hex in enumerate(destinations):
                        code = first + offset
                        if code > last:
                            break
                        source = code.to_bytes(source_width, "big")
                        try:
                            mapping[source] = bytes.fromhex(destination_hex.decode("ascii")).decode("utf-16-be")
                        except UnicodeDecodeError:
                            continue
    return mapping


def _decode_pdf_hex(data: bytes, cmap: dict[bytes, str]) -> str:
    if cmap:
        widths = sorted({len(key) for key in cmap}, reverse=True)
        output: list[str] = []
        offset = 0
        while offset < len(data):
            found = False
            for width in widths:
                key = data[offset : offset + width]
                if key in cmap:
                    output.append(cmap[key])
                    offset += width
                    found = True
                    break
            if not found:
                offset += max(1, min(widths) if widths else 1)
        decoded = "".join(output)
        if decoded:
            return decoded
    if data.startswith(b"\xfe\xff"):
        return data[2:].decode("utf-16-be", errors="replace")
    if len(data) % 2 == 0:
        try:
            decoded = data.decode("utf-16-be")
            if decoded and all(char.isprintable() or char.isspace() for char in decoded):
                return decoded
        except UnicodeDecodeError:
            pass
    return data.decode("cp1252", errors="replace")


def _pdf_text_strings(
    stream: bytes,
    cmap: dict[bytes, str],
    font_cmaps: dict[str, dict[bytes, str]] | None = None,
) -> list[str]:
    result: list[str] = []
    active_font: str | None = None
    font_cmaps = font_cmaps or {}
    index = 0
    while index < len(stream):
        font_match = re.match(
            rb"/([A-Za-z0-9]+)\s+[-+]?(?:\d+(?:\.\d*)?|\.\d+)\s+Tf\b",
            stream[index:],
        )
        if font_match is not None:
            active_font = font_match.group(1).decode("ascii", errors="replace")
            index += font_match.end()
            continue
        if stream[index] == 0x28:  # literal string
            index += 1
            depth = 1
            raw = bytearray()
            while index < len(stream) and depth:
                byte = stream[index]
                if byte == 0x5C:
                    raw.append(byte)
                    index += 1
                    if index < len(stream):
                        raw.append(stream[index])
                        index += 1
                    continue
                if byte == 0x28:
                    depth += 1
                elif byte == 0x29:
                    depth -= 1
                    if depth == 0:
                        index += 1
                        break
                raw.append(byte)
                index += 1
            if depth == 0:
                result.append(_decode_pdf_literal(bytes(raw)))
            continue
        if stream[index] == 0x3C and (index + 1 >= len(stream) or stream[index + 1] != 0x3C):
            end = stream.find(b">", index + 1)
            if end > index:
                payload = re.sub(rb"\s+", b"", stream[index + 1 : end])
                if payload and re.fullmatch(rb"[0-9A-Fa-f]+", payload):
                    if len(payload) % 2:
                        payload += b"0"
                    selected_cmap = font_cmaps.get(active_font or "", cmap)
                    result.append(
                        _decode_pdf_hex(bytes.fromhex(payload.decode("ascii")), selected_cmap)
                    )
                index = end + 1
                continue
        index += 1
    return result


def _page_font_cmaps(
    page_body: bytes, objects: dict[int, bytes], streams: dict[int, bytes]
) -> dict[str, dict[bytes, str]]:
    resources_ref = re.search(rb"/Resources\s+(\d+)\s+\d+\s+R", page_body)
    if resources_ref is None:
        return {}
    resources = objects.get(int(resources_ref.group(1)))
    if resources is None:
        return {}
    font_dictionary = re.search(rb"/Font\s*<<(.*?)>>", resources, flags=re.DOTALL)
    if font_dictionary is None:
        return {}
    result: dict[str, dict[bytes, str]] = {}
    for reference in re.finditer(
        rb"/([A-Za-z0-9]+)\s+(\d+)\s+\d+\s+R", font_dictionary.group(1)
    ):
        font_name = reference.group(1).decode("ascii", errors="replace")
        font_body = objects.get(int(reference.group(2)))
        if font_body is None:
            continue
        unicode_ref = re.search(rb"/ToUnicode\s+(\d+)\s+\d+\s+R", font_body)
        if unicode_ref is None:
            continue
        stream_id = int(unicode_ref.group(1))
        cmap_stream = streams.get(stream_id)
        if cmap_stream is not None:
            result[font_name] = _unicode_cmap({stream_id: cmap_stream})
    return result


def _normal_text(value: str) -> str:
    return "".join(character.casefold() for character in value if character.isalnum())


def _score_from_report_name(report_name: str) -> str | None:
    match = re.search(r"\(Score (\d{2})\)$", report_name)
    if match is None:
        return None
    return f"Score {match.group(1)}"


def inspect_pdf(data: bytes) -> dict[str, Any]:
    require(data.startswith(b"%PDF-"), "archive member does not begin with a PDF header")
    require(b"%%EOF" in data[-2048:], "PDF EOF marker is missing from the file tail")
    objects, streams = _parse_pdf_streams(data)
    page_object_ids = [
        int(match.group(1))
        for match in re.finditer(rb"(?ms)(\d+)\s+\d+\s+obj\b(.*?)endobj", data)
        if re.search(rb"/Type\s*/Page\b", match.group(2))
    ]
    require(page_object_ids, "PDF has no readable /Type /Page objects")
    cmap = _unicode_cmap(streams)
    page_stream_ids: list[int] = []
    text_segments: list[str] = []
    decompressed_bytes = 0
    for object_id in page_object_ids:
        body = objects[object_id]
        font_cmaps = _page_font_cmaps(body, objects, streams)
        contents = re.search(rb"/Contents\s*(?:\[(.*?)\]|(\d+\s+\d+\s+R))", body, flags=re.DOTALL)
        require(contents is not None, f"PDF page object {object_id} has no contents reference")
        refs = re.findall(rb"(\d+)\s+\d+\s+R", contents.group(1) or contents.group(2) or b"")
        require(refs, f"PDF page object {object_id} has no content stream reference")
        for ref in refs:
            stream_id = int(ref)
            require(stream_id in streams, f"PDF page {object_id} content stream {stream_id} did not load")
            page_stream_ids.append(stream_id)
            stream = streams[stream_id]
            decompressed_bytes += len(stream)
            text_segments.extend(_pdf_text_strings(stream, cmap, font_cmaps))
    require(decompressed_bytes > 0, "PDF pages have no decompressed page content")
    joined_text = " ".join(text_segments)
    return {
        "pageCount": len(page_object_ids),
        "pageObjectIds": page_object_ids,
        "contentStreamObjectIds": page_stream_ids,
        "decompressedPageContentBytes": decompressed_bytes,
        "textStrings": text_segments,
        "textNormalized": _normal_text(joined_text),
    }


def inspect_png(data: bytes) -> dict[str, Any]:
    require(data.startswith(b"\x89PNG\r\n\x1a\n"), "screenshot has an invalid PNG signature")
    offset = 8
    chunks: list[tuple[bytes, bytes]] = []
    idat = bytearray()
    width = height = bit_depth = color_type = 0
    while offset + 12 <= len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        tag = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + length]
        crc = struct.unpack(">I", data[offset + 8 + length : offset + 12 + length])[0]
        require(zlib.crc32(tag + payload) & 0xFFFFFFFF == crc, "PNG chunk CRC mismatch")
        chunks.append((tag, payload))
        if tag == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack(">IIBB", payload[:10])[:4]
        elif tag == b"IDAT":
            idat.extend(payload)
        elif tag == b"IEND":
            offset += 12 + length
            break
        offset += 12 + length
    require(width > 0 and height > 0 and bit_depth in {8, 16}, "PNG dimensions or bit depth are invalid")
    require(color_type in {2, 6}, f"unsupported screenshot PNG color type {color_type}")
    decoded = zlib.decompress(bytes(idat))
    channels = 3 if color_type == 2 else 4
    bytes_per_sample = bit_depth // 8
    bytes_per_pixel = channels * bytes_per_sample
    stride = width * bytes_per_pixel
    expected_bytes = height * (1 + stride)
    require(len(decoded) == expected_bytes, "PNG image data did not fully inflate to the declared dimensions")
    require(offset == len(data), "PNG has trailing or unparsed bytes after IEND")

    # Decode PNG row filters so preview-content checks examine the captured
    # pixels rather than merely trusting the runner's metadata.
    preview_left = int(width * 0.10)
    preview_right = min(width, math.ceil(width * 0.90))
    preview_top = int(height * 0.35)
    preview_bottom = min(height, math.ceil(height * 0.82))
    sample_step = 4
    unique_colors: set[tuple[int, int, int]] = set()
    non_white_samples = 0
    dark_samples = 0
    sampled_pixels = 0
    previous = bytearray(stride)
    offset = 0
    for y in range(height):
        filter_type = decoded[offset]
        encoded = decoded[offset + 1 : offset + 1 + stride]
        offset += 1 + stride
        row = bytearray(stride)
        require(filter_type in {0, 1, 2, 3, 4}, f"unsupported PNG row filter {filter_type}")
        for index, value in enumerate(encoded):
            left = row[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
            up = previous[index]
            upper_left = previous[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = up
            elif filter_type == 3:
                predictor = (left + up) // 2
            else:
                estimate = left + up - upper_left
                distances = (abs(estimate - left), abs(estimate - up), abs(estimate - upper_left))
                predictor = left if distances[0] <= distances[1] and distances[0] <= distances[2] else (
                    up if distances[1] <= distances[2] else upper_left
                )
            row[index] = (value + predictor) & 0xFF

        if preview_top <= y < preview_bottom and (y - preview_top) % sample_step == 0:
            for x in range(preview_left, preview_right, sample_step):
                pixel = x * bytes_per_pixel
                red = row[pixel]
                green = row[pixel + bytes_per_sample]
                blue = row[pixel + 2 * bytes_per_sample]
                color = (red, green, blue)
                unique_colors.add(color)
                sampled_pixels += 1
                if red + green + blue < 735:
                    non_white_samples += 1
                if red < 96 and green < 96 and blue < 96:
                    dark_samples += 1
        previous = row

    preview_content = {
        "region": "central-lower-preview-area",
        "x": preview_left,
        "y": preview_top,
        "width": preview_right - preview_left,
        "height": preview_bottom - preview_top,
        "sampledPixels": sampled_pixels,
        "uniqueSampledColors": len(unique_colors),
        "nonWhiteSampledPixels": non_white_samples,
        "darkSampledPixels": dark_samples,
    }
    return {
        "width": width,
        "height": height,
        "colorType": color_type,
        "bitDepth": bit_depth,
        "decodedBytes": len(decoded),
        "chunkCount": len(chunks),
        "previewContentStats": preview_content,
    }


def _source_rows(source_inventory: dict[str, Any]) -> dict[str, dict[str, Any]]:
    rows = source_inventory.get("files")
    require(isinstance(rows, list) and rows, "source inventory contains no files")
    mapping = {str(row["path"]): row for row in rows}
    require(len(mapping) == len(rows), "source inventory contains duplicate paths")
    return mapping


def validate_source_and_package(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    source = manifest.get("source") or {}
    source_path = root / str(source.get("inventoryPath", ""))
    source_inventory = read_json(source_path)
    rows = source_inventory.get("files") or []
    require(inventory_fingerprint(rows) == source.get("sha256"), "current source inventory fingerprint differs from manifest")
    require(inventory_fingerprint(rows) == source_inventory.get("sha256"), "source inventory JSON misstates its fingerprint")
    require(sha256_file(source_path) == source.get("inventoryFileSha256"), "source inventory file hash differs from manifest")
    current_map = _source_rows(source_inventory)
    repository_root = Path(str(source.get("repositoryRoot", ""))).resolve(strict=True)
    for path, row in current_map.items():
        live_path = (repository_root / path).resolve(strict=True)
        require(live_path.is_file(), f"measured source file is missing: {path}")
        require(live_path.stat().st_size == int(row["sizeBytes"]), f"measured source size changed: {path}")
        require(sha256_file(live_path) == str(row["sha256"]).lower(), f"measured source hash changed: {path}")

    derivation_path = root / str(source.get("derivedReceiptPath", ""))
    derivation = read_json(derivation_path)
    require(sha256_file(derivation_path) == source.get("derivedReceiptSha256"), "source derivation receipt hash differs from manifest")
    require(derivation.get("status") == "verified-script-only-derivation", "P9-13 derivation receipt is not verified")
    require(derivation.get("taskId") == TASK_ID, "source derivation receipt task ID mismatch")
    require(derivation.get("source", {}).get("productSourceEquivalent") is True, "receipt does not establish product-source equivalence")
    require(derivation.get("source", {}).get("freshExactTreeRebuild") is False, "receipt overstates provenance as a fresh exact-tree rebuild")

    prior_manifest_path = Path(str(derivation.get("priorRunManifestPath", ""))).resolve(strict=True)
    require(sha256_file(prior_manifest_path) == derivation.get("priorRunManifestSha256"), "P9-12 prior manifest hash differs from derivation receipt")
    prior_root = prior_manifest_path.parent
    prior = read_json(prior_manifest_path)
    require(prior.get("taskId") == P912_TASK_ID and prior.get("status") == "passed", "source-equivalence base is not the accepted P9-12 run")
    prior_inventory_path = prior_root / str(prior.get("source", {}).get("inventoryPath", ""))
    prior_inventory = read_json(prior_inventory_path)
    prior_map = _source_rows(prior_inventory)
    for path, old_row in prior_map.items():
        require(path in current_map and current_map[path] == old_row, f"P9-12 inventoried source changed or disappeared: {path}")
    added_paths = set(current_map) - set(prior_map)
    require(EXPECTED_NEW_SCRIPTS <= added_paths, "source inventory is missing one or more assigned P9-13 harness scripts")
    require(added_paths == set(derivation.get("source", {}).get("capturedHarnessAdditions", [])), "captured harness-addition set differs from receipt")
    require(added_paths <= set(derivation.get("source", {}).get("allowedPaths", [])), "a newly captured harness file was not declared in allowedPaths")
    require(
        all(path.startswith("scripts/phase9/") and Path(path).suffix.lower() in {".ps1", ".py"} for path in added_paths),
        "source delta includes a file outside the Phase 9 harness surface",
    )
    prior_receipt_path = prior_root / str(prior.get("source", {}).get("derivedReceiptPath", ""))
    prior_receipt = read_json(prior_receipt_path)
    require(sha256_file(prior_receipt_path) == derivation.get("priorDerivedReceiptSha256"), "P9-12 source receipt hash differs from the P9-13 derivation")
    require(prior_receipt.get("status") == "verified-script-only-derivation", "P9-12 source receipt is not verified")
    allowed = set(prior_receipt.get("source", {}).get("allowedPaths", [])) | added_paths
    product_rows = [row for path, row in current_map.items() if path not in allowed]
    product_hash = inventory_fingerprint(product_rows)
    require(product_hash == derivation.get("source", {}).get("productSourceSha256"), "product-source inventory differs from P9-12 receipt")
    require(product_hash == prior_receipt.get("source", {}).get("productSourceSha256"), "product-source fingerprint differs from the accepted P9-12 package")
    require(product_hash == source.get("productSourceSha256"), "manifest product-source fingerprint mismatch")

    package = manifest.get("package") or {}
    package_inventory_path = root / str(package.get("inventoryPath", ""))
    package_inventory = read_json(package_inventory_path)
    package_rows = package_inventory.get("files") or []
    package_fingerprint = inventory_fingerprint(package_rows)
    require(package_fingerprint == package.get("sha256"), "package inventory fingerprint differs from manifest")
    require(package_fingerprint == package_inventory.get("sha256"), "package inventory JSON misstates its fingerprint")
    require(sha256_file(package_inventory_path) == package.get("inventoryFileSha256"), "package inventory file hash differs from manifest")
    package_root = Path(str(package.get("root", ""))).resolve(strict=True)
    package_map = {str(row["path"]): row for row in package_rows}
    require(len(package_map) == len(package_rows), "package inventory contains duplicate paths")
    for relative, row in package_map.items():
        path = package_root / relative.replace("/", "\\")
        require(path.is_file(), f"packaged file is missing: {relative}")
        require(path.stat().st_size == int(row["sizeBytes"]), f"packaged file size changed: {relative}")
        require(sha256_file(path) == str(row["sha256"]).lower(), f"packaged file hash changed: {relative}")
    unlisted = [path for path in package_root.rglob("*") if path.is_file() and path.relative_to(package_root).as_posix() not in package_map]
    require(not unlisted, "reconstructed package contains files not present in its byte-bound inventory")

    resource = read_json(root / str(package.get("resourceInventoryPath", "")))
    resource_rows = resource.get("files") or []
    resource_fingerprint = inventory_fingerprint(resource_rows)
    require(resource_fingerprint == package.get("resourceInventorySha256"), "staged-resource inventory fingerprint differs from package manifest")
    require(sha256_file(root / str(package.get("resourceInventoryPath", ""))) == package.get("resourceInventoryFileSha256"), "staged-resource inventory file hash mismatch")
    resource_root = Path(str(package.get("resourceRoot", ""))).resolve(strict=True)
    for row in resource_rows:
        path = resource_root / str(row["path"]).replace("/", "\\")
        require(path.is_file() and path.stat().st_size == int(row["sizeBytes"]), f"staged resource missing or changed: {row['path']}")
        require(sha256_file(path) == str(row["sha256"]).lower(), f"staged resource hash changed: {row['path']}")

    app = manifest.get("application") or {}
    app_path = Path(str(app.get("path", ""))).resolve(strict=True)
    require(app_path == package_root / "ClassMngrNext.exe", "application path is not the measured reconstructed package executable")
    require(sha256_file(app_path) == app.get("sha256"), "application hash differs from manifest")
    require(app.get("sha256") == prior.get("application", {}).get("sha256"), "application hash differs from accepted P9-12 executable")
    release = read_json(Path(str(manifest.get("releaseBuildReceipt", {}).get("path", ""))).resolve(strict=True))
    require(sha256_file(Path(str(manifest["releaseBuildReceipt"]["path"]))) == manifest["releaseBuildReceipt"].get("sha256"), "Release receipt file hash mismatch")
    require(release.get("status") == "succeeded" and release.get("configuration") == "Release" and release.get("platform") == "x64", "preserved Release receipt is not a successful x64 build")
    require(release.get("releaseExecutableSha256") == app.get("sha256"), "Release receipt executable hash differs from package")
    require(release.get("generatedResources", {}).get("stagedResourceInputs", {}).get("sha256") == resource_fingerprint, "Release receipt resource fingerprint differs from staged inputs")
    cache_path = Path(str(manifest.get("buildProvenance", {}).get("cachePath", ""))).resolve(strict=True)
    require(sha256_file(cache_path) == manifest.get("buildProvenance", {}).get("cacheSha256Current"), "standard Release cache hash changed")
    require(manifest.get("buildProvenance", {}).get("freshExactTreeRebuild") is False, "manifest overstates build as fresh exact-tree rebuild")

    return {
        "priorTaskId": prior.get("taskId"),
        "priorRunId": prior.get("runId"),
        "currentSourceSha256": source.get("sha256"),
        "productSourceSha256": product_hash,
        "capturedHarnessAdditions": sorted(added_paths),
        "packageSha256": package_fingerprint,
        "packageFileCount": len(package_rows),
        "applicationSha256": app.get("sha256"),
        "resourceSha256": resource_fingerprint,
        "resourceFileCount": len(resource_rows),
        "freshExactTreeRebuild": False,
    }


def validate_workspace(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    fixture = manifest.get("fixture") or {}
    workspace = manifest.get("workspace") or {}
    receipt_path = root / str(workspace.get("preparationReceiptPath", ""))
    receipt = read_json(receipt_path)
    require(receipt.get("schema") == WORKSPACE_SCHEMA and receipt.get("status") == "passed", "workspace receipt identity or status mismatch")
    require(sha256_file(receipt_path) == workspace.get("preparationReceiptSha256"), "workspace preparation receipt hash mismatch")
    seed = Path(str(receipt.get("sourceSeedSql", ""))).resolve(strict=True)
    base = Path(str(receipt.get("sourceBlankWorkspace", ""))).resolve(strict=True)
    require(sha256_file(seed) == fixture.get("seedSqlSha256") == receipt.get("sourceSeedSqlSha256"), "large_startup.sql fixture hash mismatch")
    require(sha256_file(base) == fixture.get("baseWorkspaceSha256") == receipt.get("sourceBlankWorkspaceSha256"), "blank workspace base hash mismatch")
    require(receipt.get("counts") == {"classes": 96, "roster_columns": 288, "roster_data": 7200, "speaking_evaluations": 20, "speaking_eval_data": 600}, "seeded workspace counts mismatch")
    require(receipt.get("integrityCheck") == "ok" and receipt.get("foreignKeyErrors") == [], "seeded workspace integrity/foreign-key checks failed")
    selected = receipt.get("selectedEvaluation") or {}
    mapping = receipt.get("evaluationNameUiMapping") or {}
    expected_names = [f"Heavy Eval 01 (Score {index:02d})" for index in range(1, 31)]
    expected_labels = [f"Student {index:03d}" for index in range(1, 31)]
    require(selected.get("evaluationId") == 1 and selected.get("classId") == 1, "selected fixture evaluation identity mismatch")
    require(selected.get("evaluationName") == "Winter" and selected.get("studentCount") == EXPECTED_WORKSPACE_STUDENT_COUNT, "selected fixture evaluation count/name mismatch")
    require(fixture.get("selectedEvaluationName") == "Winter" and fixture.get("seedEvaluationNameBeforeUiMapping") == "Heavy Term 01", "manifest does not bind the approved UI evaluation-name mapping")
    require(int(fixture.get("expectedStudentCount", 0)) == EXPECTED_WORKSPACE_STUDENT_COUNT, "manifest does not retain the full 30-row source evaluation")
    require(int(fixture.get("expectedReportCount", 0)) == EXPECTED_REPORT_COUNT, "manifest does not bind the 25-row default UI export scope")
    require(fixture.get("expectedWorkspaceReportNames") == expected_names, "manifest workspace report-name list does not retain all 30 source rows")
    require(fixture.get("expectedReportNames") == expected_names[:EXPECTED_REPORT_COUNT], "manifest export report-name list is not the first 25 source rows")
    require(selected.get("reportNames") == expected_names, "selected report display names differ from the fixture rows")
    require(selected.get("sourceRowLabels") == expected_labels, "selected evaluation source row labels differ from the fixture")
    path = Path(str(workspace.get("path", ""))).resolve(strict=True)
    require(path.stat().st_size == int(workspace.get("bytes", -1)) == int(receipt.get("workspaceBytes", -2)), "prepared workspace size mismatch")
    require(sha256_file(path) == workspace.get("sha256") == receipt.get("workspaceSha256"), "prepared workspace hash mismatch")
    require(path == Path(str(manifest.get("process", {}).get("workspacePath", ""))).resolve(strict=True), "measured process did not open the prepared workspace")

    connection = sqlite3.connect(f"file:{path.as_posix()}?mode=ro", uri=True)
    try:
        integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
        foreign_keys = [list(row) for row in connection.execute("PRAGMA foreign_key_check")]
        counts = {
            table: int(connection.execute(f'SELECT COUNT(*) FROM "{table}"').fetchone()[0])
            for table in ("classes", "roster_columns", "roster_data", "speaking_evaluations", "speaking_eval_data")
        }
        rows = [
            [str(cell or "") for cell in row]
            for row in connection.execute(
                "SELECT col_0, col_1, col_2 FROM speaking_eval_data WHERE evaluation_id = 1 ORDER BY row_index"
            )
        ]
        hashes = {
            "speaking_evaluations": table_rows_hash(connection, "speaking_evaluations", "id"),
            "speaking_eval_data": table_rows_hash(connection, "speaking_eval_data", "evaluation_id, row_index"),
        }
        evaluation_columns = [
            str(item[1])
            for item in connection.execute('PRAGMA table_info("speaking_evaluations")')
        ]
        evaluation_rows = [
            list(row)
            for row in connection.execute(
                "SELECT * FROM speaking_evaluations ORDER BY id"
            )
        ]
    finally:
        connection.close()
    require(integrity == "ok" and not foreign_keys, "live prepared workspace fails independent SQLite integrity checks")
    require(counts == receipt.get("counts"), "live prepared workspace table counts differ from receipt")
    require([f"{row[1]} ({row[2]})" for row in rows] == expected_names, "live report rows differ from the expected ZIP/PDF names")
    require(hashes == receipt.get("speakingTableSha256") == workspace.get("speakingTableSha256"), "speaking-evaluation source table hashes changed")
    workspace_sha256 = sha256_file(path)
    mapping_result = _validate_evaluation_name_ui_mapping(
        mapping,
        evaluation_columns,
        evaluation_rows,
        hashes["speaking_evaluations"],
        workspace_sha256,
    )
    return {"workspaceSha256": workspace_sha256, "counts": counts, "selectedEvaluationId": 1, "studentCount": len(rows), "sourceRowLabels": expected_labels, "reportNames": expected_names, "exportReportNames": selected_export_report_names(expected_names), "evaluationNameUiMapping": mapping_result}


def _validate_evaluation_name_ui_mapping(
    mapping: dict[str, Any],
    columns: list[str],
    rows_after: list[list[Any]],
    table_sha256_after: str,
    workspace_sha256_after: str,
) -> dict[str, Any]:
    require(columns == ["id", "class_id", "evaluation_name"], "speaking-evaluation table schema differs from the approved fixture mapping")
    expected_before = [
        [evaluation_id, evaluation_id, f"Heavy Term {evaluation_id:02d}"]
        for evaluation_id in range(1, 21)
    ]
    expected_after = [list(row) for row in expected_before]
    expected_after[0][2] = "Winter"
    require(rows_after == expected_after, "prepared workspace differs from the seed by more than the approved evaluation-name cell")
    expected_before_hash = canonical_sha256({"columns": columns, "rows": expected_before})
    expected_after_hash = canonical_sha256({"columns": columns, "rows": expected_after})
    require(mapping.get("table") == "speaking_evaluations" and mapping.get("column") == "evaluation_name", "workspace evaluation-name mapping targets an unexpected field")
    require(mapping.get("evaluationId") == 1 and mapping.get("classId") == 1, "workspace evaluation-name mapping targets an unexpected row")
    require(mapping.get("beforeValue") == "Heavy Term 01" and mapping.get("afterValue") == "Winter", "workspace evaluation-name mapping values differ from the approved change")
    require(mapping.get("updateRowCount") == 1 and mapping.get("onlySelectedEvaluationNameCellChanged") is True, "workspace mapping did not prove one-cell scope")
    require(mapping.get("seededTableSha256BeforeMapping") == expected_before_hash, "seeded evaluation table hash before mapping is incorrect")
    require(mapping.get("preparedTableSha256AfterMapping") == expected_after_hash == table_sha256_after, "prepared evaluation table hash after mapping is incorrect")
    before_workspace_hash = str(mapping.get("workspaceSha256BeforeMapping", ""))
    after_workspace_hash = str(mapping.get("workspaceSha256AfterMapping", ""))
    require(re.fullmatch(r"[0-9a-f]{64}", before_workspace_hash) is not None, "pre-mapping workspace SHA-256 is missing or malformed")
    require(re.fullmatch(r"[0-9a-f]{64}", after_workspace_hash) is not None, "post-mapping workspace SHA-256 is missing or malformed")
    require(before_workspace_hash != after_workspace_hash == workspace_sha256_after, "workspace file hashes do not show the approved mapping result")
    return {
        "evaluationId": 1,
        "classId": 1,
        "beforeValue": "Heavy Term 01",
        "afterValue": "Winter",
        "workspaceSha256BeforeMapping": before_workspace_hash,
        "workspaceSha256AfterMapping": after_workspace_hash,
        "seededEvaluationTableSha256BeforeMapping": expected_before_hash,
        "preparedEvaluationTableSha256AfterMapping": expected_after_hash,
        "onlySelectedEvaluationNameCellChanged": True,
    }


def validate_runtime_settings(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    provenance = manifest.get("runtimeSettingsProvenance") or {}
    require(provenance.get("rootRelativePath") == "runtime/settings", "runtime settings root is not the isolated evidence-local root")
    require(provenance.get("maximumRosterRowsKey") == "options/maximumRosterRows", "runtime settings maximum-row key differs from the supported product preference")
    require(provenance.get("configuredMaximumRosterRows") is None, "runner explicitly configured maximum roster rows")
    require(int(provenance.get("effectiveDefaultMaximumRosterRows", 0)) == DEFAULT_MAXIMUM_ROSTER_ROWS, "effective default roster row limit is not 25")
    require(provenance.get("defaultPolicySource") == "src/next/application/roster_row_display_policy.h", "default roster policy source path is missing")

    settings_root = (root / str(provenance["rootRelativePath"])).resolve(strict=True)
    before_path = root / str(provenance.get("beforeLaunchInventoryPath", ""))
    after_path = root / str(provenance.get("afterExitInventoryPath", ""))
    require(before_path.is_file() and after_path.is_file(), "runtime settings before/after inventory is missing")
    require(sha256_file(before_path) == provenance.get("beforeLaunchInventorySha256"), "before-launch settings inventory hash differs from manifest")
    require(sha256_file(after_path) == provenance.get("afterExitInventorySha256"), "after-exit settings inventory hash differs from manifest")
    before = read_json(before_path)
    after = read_json(after_path)
    require(Path(str(before.get("root", ""))).resolve() == settings_root, "before-launch inventory covers a different settings root")
    require(Path(str(after.get("root", ""))).resolve() == settings_root, "after-exit inventory covers a different settings root")
    require(before.get("files") == [] and int(before.get("fileCount", -1)) == 0, "isolated settings root was not empty before packaged app startup")
    require(int(provenance.get("beforeLaunchFileCount", -1)) == 0, "manifest reports pre-existing settings files")
    require(before.get("sha256") == provenance.get("beforeLaunchRootSha256") == inventory_fingerprint([]), "empty before-launch settings root fingerprint is invalid")

    actual_files = []
    for path in sorted(
        settings_root.rglob("*"),
        key=lambda item: item.relative_to(settings_root).as_posix().lower(),
    ):
        if path.is_file():
            actual_files.append({
                "path": path.relative_to(settings_root).as_posix(),
                "sizeBytes": path.stat().st_size,
                "sha256": sha256_file(path),
            })
    recorded_files = after.get("files")
    require(isinstance(recorded_files, list), "after-exit settings inventory has no file list")
    require(actual_files == recorded_files, "after-exit settings root files differ from the captured inventory")
    actual_fingerprint = inventory_fingerprint(actual_files)
    require(actual_fingerprint == after.get("sha256") == provenance.get("afterExitRootSha256"), "after-exit settings root fingerprint differs from disk")
    require(len(actual_files) == int(after.get("fileCount", -1)) == int(provenance.get("afterExitFileCount", -2)), "after-exit settings file count differs from manifest")

    repository_root = Path(str(manifest.get("repositoryRoot", ""))).resolve(strict=True)
    policy_path = repository_root / str(provenance["defaultPolicySource"])
    policy_text = policy_path.read_text(encoding="utf-8")
    require(re.search(r"DefaultMaximumRosterRows\s*=\s*25\s*;", policy_text) is not None, "current source does not define the 25-row isolated default")
    for options_file in settings_root.rglob("*.ini"):
        parser = configparser.ConfigParser(interpolation=None)
        try:
            parser.read(options_file, encoding="utf-8")
        except configparser.Error as error:
            raise ValidationFailure(f"runtime QSettings INI cannot be parsed: {error}") from error
        configured = parser["options"] if parser.has_section("options") else {}
        require("maximumrosterrows" not in configured, "runtime profile contains an explicit maximumRosterRows override")
        require("rosterrowdisplaymode" not in configured, "runtime profile contains an explicit roster display-mode override")
    return {
        "rootRelativePath": provenance["rootRelativePath"],
        "beforeLaunchFileCount": len(before["files"]),
        "beforeLaunchRootSha256": before["sha256"],
        "afterExitFileCount": len(actual_files),
        "afterExitRootSha256": actual_fingerprint,
        "configuredMaximumRosterRows": None,
        "effectiveDefaultMaximumRosterRows": DEFAULT_MAXIMUM_ROSTER_ROWS,
    }


def _uia_matches(snapshot: dict[str, Any], automation_id: str) -> list[dict[str, Any]]:
    return [
        row
        for row in snapshot.get("elements", [])
        if str(row.get("automationId", "")) == automation_id
        or str(row.get("automationId", "")).endswith("." + automation_id)
    ]


def _label(row: dict[str, Any]) -> str:
    return str(row.get("selectedItem") or row.get("value") or row.get("name") or "")


def _validate_window_targeted_navigation_click(action: dict[str, Any], process_id: int) -> None:
    targets = {
        "navigate-to-speaking-evaluation": ("Speaking Evaluation", "speaking_evaluation"),
        "navigate-away-after-report-close": ("My Workspace", "my_workspace"),
        "navigate-to-my-workspace-probe": ("My Workspace", "my_workspace"),
    }
    action_kind = str(action.get("actionKind", ""))
    require(action_kind in targets, "window-targeted click is not bound to an approved Next navigation action")
    expected_name, expected_page = targets[action_kind]
    require(action.get("expectedName") == action.get("observedName") == expected_name, "navigation click UIA name differs from the target")
    require(action.get("targetPageId") == expected_page, "navigation click target page does not match its registered UIA target")
    target = action.get("target") or {}
    require(target.get("controlType") == "ControlType.TreeItem", "navigation click target is not a UIA TreeItem")
    require(str(target.get("automationId", "")).endswith(".classMngrNextNavigationTree"), "navigation click target is not part of the Next navigation tree")
    require(str(action.get("method", "")) == "UIA-targeted window click", "navigation did not use the authorized UIA-derived window click")

    pointer = action.get("pointerAction") or {}
    require(pointer.get("actionType") == "WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP", "navigation click message sequence is incomplete")
    require(pointer.get("kind") == "window-targeted WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_LBUTTONUP at center of the same verified UIA target", "navigation click description is unexpected")
    require(pointer.get("sourceTarget") == {key: target.get(key) for key in ("name", "automationId", "controlType", "rectangle")}, "navigation click source UIA identity or rectangle differs from its action target")
    require(int(pointer.get("targetProcessId", 0)) == process_id, "navigation click target PID differs from the measured app")
    require(int(pointer.get("windowProcessId", 0)) == process_id == int(action.get("windowProcessId", 0)), "navigation click HWND PID differs from the measured app")
    require(int(pointer.get("windowHandle", 0)) == int(action.get("windowHandle", 0)) > 0, "navigation click HWND differs from its action record")

    rectangle = target.get("rectangle") or {}
    bounds = pointer.get("measuredMainWindowBounds") or {}
    rect_x = float(rectangle.get("x", -1))
    rect_y = float(rectangle.get("y", -1))
    rect_width = float(rectangle.get("width", 0))
    rect_height = float(rectangle.get("height", 0))
    window_x = int(bounds.get("x", 0))
    window_y = int(bounds.get("y", 0))
    window_width = int(bounds.get("width", 0))
    window_height = int(bounds.get("height", 0))
    require(rect_width > 0 and rect_height > 0 and window_width > 0 and window_height > 0, "navigation click bounds are empty")
    require(
        rect_x >= window_x
        and rect_y >= window_y
        and rect_x + rect_width <= window_x + window_width
        and rect_y + rect_height <= window_y + window_height,
        "navigation UIA rectangle is not fully inside the measured main window",
    )
    expected_x = int(round(rect_x + rect_width / 2))
    expected_y = int(round(rect_y + rect_height / 2))
    click_x = int(pointer.get("screenX", -1))
    click_y = int(pointer.get("screenY", -1))
    require((click_x, click_y) == (expected_x, expected_y), "navigation click point differs from the UIA rectangle center")
    require(window_x <= click_x < window_x + window_width and window_y <= click_y < window_y + window_height, "navigation click point is outside the measured main window")

    require(action.get("verifiedByFollowingPageRoute") is True, "navigation click lacks the required observed page-route confirmation")
    arrival_before = int(action.get("logArrivalIndexBeforeAction", -1))
    route_event = action.get("pageRouteEvent") or {}
    require(route_event.get("observed") is True and route_event.get("targetPageId") == expected_page, "navigation page-route evidence does not confirm the target page")
    require(int(route_event.get("arrivalIndex", 0)) > arrival_before >= 0, "page-route event did not arrive after the navigation click")
    require(int(route_event.get("processId", 0)) == process_id == int(route_event.get("windowProcessId", 0)), "page-route evidence is not bound to the measured PID")
    require(int(route_event.get("windowHandle", 0)) == int(action.get("windowHandle", 0)), "page-route evidence HWND differs from the navigation action")
    line = str(route_event.get("line", ""))
    match = re.match(r"^ClassMngrNext navigation event=page-route outcome=completed cause=\S+ from=\S+ to=(\S+)(?:\s|$)", line)
    require(match is not None and match.group(1) == expected_page, "application log does not contain the required completed navigation page-route event")


def validate_uia(root: Path, manifest: dict[str, Any], expected_first: str, output_directory: str) -> dict[str, Any]:
    snapshots = manifest.get("uiaSnapshots") or {}
    require(REQUIRED_UIA_SNAPSHOTS <= set(snapshots), "one or more named UIA snapshots are missing")
    loaded: dict[str, dict[str, Any]] = {}
    process_id = int(manifest.get("process", {}).get("processId", 0))
    for name, info in snapshots.items():
        path = root / str(info.get("relativePath", ""))
        require(path.is_file() and sha256_file(path) == info.get("sha256"), f"UIA snapshot hash mismatch: {name}")
        snapshot = read_json(path)
        require(int(snapshot.get("processId", 0)) == process_id, f"UIA snapshot is not bound to measured PID: {name}")
        require(snapshot.get("coverage") == "bounded named UIA controls only", f"UIA snapshot is not bounded to named controls: {name}")
        require(int(snapshot.get("mainWindowHandle", 0)) == int((manifest.get("process") or {}).get("mainWindowHandle", 0)), f"UIA snapshot main HWND differs from measured process: {name}")
        loaded[name] = snapshot
    startup = loaded["startup-ready"]
    require(not _uia_matches(startup, "nextSpeakingEvaluationReports"), "startup had a speaking report dialog loaded")
    require(not _uia_matches(startup, "nextReportStudent"), "startup had a speaking report student preview loaded")
    route = loaded["speaking-evaluation-page-ready"]
    for automation_id in ("speakingEvaluationClassList", "speakingEvaluationChoice", "speakingEvaluationReports", "speakingEvaluationSaveReports"):
        require(len(_uia_matches(route, automation_id)) == 1, f"Next speaking-evaluation page control is missing or duplicated: {automation_id}")
    evaluation_choices = _uia_matches(route, "speakingEvaluationChoice")
    require(len(evaluation_choices) == 1 and _label(evaluation_choices[0]) == "Winter", "Next speaking-evaluation selector did not use the approved UI-valid Winter entry")
    direct = loaded["direct-preview-open"]
    require(len(_uia_matches(direct, "nextSpeakingEvaluationReports")) == 1, "direct preview is not UIA-visible")
    student_controls = _uia_matches(direct, "nextReportStudent")
    require(len(student_controls) == 1 and _label(student_controls[0]) == expected_first, "direct preview UIA selected report name differs from fixture")
    require(len(_uia_matches(direct, "nextReportClose")) == 1 and len(_uia_matches(direct, "nextReportSavePdf")) == 1, "direct preview Close/Save controls are missing")
    for name in ("direct-preview-closed", "batch-preview-closed", "export-complete", "navigated-away"):
        snapshot = loaded[name]
        for automation_id in ("nextSpeakingEvaluationReports", "nextReportStudent", "nextReportClose", "nextReportSavePdf"):
            require(not _uia_matches(snapshot, automation_id), f"report preview control {automation_id} remained exposed at {name}")
    batch = loaded["batch-dialog-ready"]
    require(len(_uia_matches(batch, "nextSpeakingEvaluationBatchReports")) == 1, "batch save dialog was not UIA-visible")
    scope = _uia_matches(batch, "nextReportScope")
    renderer = _uia_matches(batch, "nextReportRenderer")
    directory = _uia_matches(batch, "nextReportDirectory")
    require(len(scope) == len(renderer) == len(directory) == 1, "batch dialog scope, renderer, or output directory control is missing")
    require(_label(scope[0]) == "All Students (25)", "batch scope is not All Students (25) under isolated default settings")
    require("Internal" in _label(renderer[0]), "batch renderer is not the internal Qt renderer")
    require(str(directory[0].get("value", "")) == output_directory, "batch output directory ValuePattern value mismatch")
    preview = loaded["batch-preview-open"]
    require(len(_uia_matches(preview, "nextSpeakingEvaluationReports")) == 1, "nested batch preview is not UIA-visible")
    nested_student = _uia_matches(preview, "nextReportStudent")
    require(len(nested_student) == 1 and _label(nested_student[0]) == expected_first, "batch preview selected report differs from fixture")
    require(len(_uia_matches(loaded["batch-preview-closed"], "nextSpeakingEvaluationBatchReports")) == 1, "closing batch preview also closed its parent batch dialog")
    require(not _uia_matches(loaded["navigated-away"], "speakingEvaluationReports"), "speaking-evaluation page report action remained current after navigation away")
    navigated_checkpoint = (manifest.get("checkpoints") or {}).get("navigated-away") or {}
    require(navigated_checkpoint.get("targetPage") == "my_workspace", "navigated-away checkpoint does not identify the UIA target page")

    actions_path = root / str(manifest.get("files", {}).get("uiaActions", "uia-actions.jsonl"))
    require(actions_path.is_file(), "UIA action log is missing")
    actions: list[dict[str, Any]] = []
    for line_number, line in enumerate(actions_path.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip():
            continue
        try:
            actions.append(json.loads(line))
        except json.JSONDecodeError as error:
            raise ValidationFailure(f"invalid UIA action JSON at line {line_number}: {error}") from error
    required_actions = {
        "navigateToSpeakingEvaluation",
        "selectFixtureClass",
        "openDirectPreview",
        "closeDirectPreview",
        "openBatchDialog",
        "setBatchOutputDirectory",
        "openBatchPreview",
        "closeBatchPreview",
        "exportBatch",
        "navigateAway",
    }
    action_map = manifest.get("uiaActions") or {}
    require(required_actions <= set(action_map), "one or more named UIA actions are missing")
    for name in required_actions:
        action = action_map[name]
        require(int(action.get("processId", 0)) == process_id and int(action.get("windowProcessId", 0)) == process_id, f"UIA action {name} is not bound to measured PID")
        require(int(action.get("windowHandle", 0)) > 0, f"UIA action {name} lacks an owning HWND")
        require(int(action.get("windowProcessId", 0)) == process_id, f"UIA action {name} owning HWND PID mismatch")
        method = str(action.get("method", ""))
        if name in {"navigateToSpeakingEvaluation", "navigateAway"}:
            _validate_window_targeted_navigation_click(action, process_id)
        else:
            require(method in {"SelectionItemPattern.Select", "ValuePattern.SetValue", "InvokePattern.Invoke"}, f"UIA action {name} used an unsupported method")
        target = action.get("target") or {}
        require(int(target.get("processId", 0)) == process_id and target.get("isEnabled") is True and target.get("isOffscreen") is False, f"UIA action {name} target was not enabled/visible in measured PID")
        require(int(action.get("windowProcessId", 0)) == int(target.get("processId", 0)), f"UIA action {name} owner window and target do not share the measured PID")
        require(int(action.get("windowHandle", 0)) > 0, f"UIA action {name} has no native owner-window identity")
    route_observations = [row for row in actions if row.get("recordType") == "page-route-observation" and row.get("associatedActionKind") == "navigate-to-speaking-evaluation"]
    require(len(route_observations) == 1, "UIA action log must contain one page-route observation for Speaking Evaluation navigation")
    require(route_observations[0].get("observedPageRouteEvent") == action_map["navigateToSpeakingEvaluation"].get("pageRouteEvent"), "UIA action log page-route observation differs from the navigation manifest")
    away_action = action_map["navigateAway"]
    away_target = away_action.get("target") or {}
    require(away_action.get("actionKind") == "navigate-away-after-report-close" and away_action.get("expectedName") == away_target.get("name") == "My Workspace", "navigate-away action is not bound to the visible My Workspace TreeItem")
    require(away_target.get("controlType") == "ControlType.TreeItem" and str(away_target.get("automationId", "")).endswith(".classMngrNextNavigationTree"), "navigate-away target is not the Next navigation TreeItem")
    _validate_window_targeted_navigation_click(away_action, process_id)
    away_route = away_action.get("pageRouteEvent") or {}
    require(away_action.get("verifiedByFollowingPageRoute") is True and away_route.get("observed") is True and away_route.get("targetPageId") == "my_workspace", "navigate-away has no completed My Workspace page-route event")
    require(int(away_route.get("arrivalIndex", 0)) > int(away_action.get("logArrivalIndexBeforeAction", -1)), "My Workspace page-route event did not follow the UIA selection")
    require(int(away_route.get("processId", 0)) == process_id == int(away_route.get("windowProcessId", 0)), "My Workspace route event is not bound to the measured PID")
    require(int(away_route.get("windowHandle", 0)) == int(away_action.get("windowHandle", 0)), "My Workspace route event HWND differs from the UIA action")
    away_line = str(away_route.get("line", ""))
    away_match = re.match(r"^ClassMngrNext navigation event=page-route outcome=completed cause=\S+ from=\S+ to=(\S+)(?:\s|$)", away_line)
    require(away_match is not None and away_match.group(1) == "my_workspace", "application log does not contain the completed My Workspace page-route event")
    away_observations = [row for row in actions if row.get("recordType") == "page-route-observation" and row.get("associatedActionKind") == "navigate-away-after-report-close"]
    require(len(away_observations) == 1 and away_observations[0].get("observedPageRouteEvent") == away_route, "UIA action log My Workspace route observation differs from the navigation manifest")
    require(navigated_checkpoint.get("pageRouteEvent") == away_route, "navigated-away checkpoint differs from the verified UIA route event")
    times = [str(action_map[name].get("issuedAtUtc", "")) for name in ("navigateToSpeakingEvaluation", "selectFixtureClass", "openDirectPreview", "closeDirectPreview", "openBatchDialog", "openBatchPreview", "closeBatchPreview", "exportBatch", "navigateAway")]
    require(times == sorted(times), "speaking report UIA action order is not chronological")
    return {"snapshotCount": len(loaded), "actionCount": len(actions), "selectedFirstReport": expected_first, "batchScope": _label(scope[0]), "processId": process_id}


def validate_preview_content_stats(
    captured_stats: dict[str, Any], decoded_stats: dict[str, Any], checkpoint: str
) -> None:
    for key in ("region", "x", "y", "width", "height", "sampledPixels", "nonWhiteSampledPixels", "darkSampledPixels"):
        require(captured_stats.get(key) == decoded_stats.get(key), f"screenshot preview-content metric {key} differs from decoded pixels: {checkpoint}")
    require(
        int(captured_stats.get("uniqueSampledColors", 0)) >= 5
        and int(decoded_stats.get("uniqueSampledColors", 0)) >= 5,
        f"screenshot has insufficient color variation in captured or decoded pixels: {checkpoint}",
    )
    if checkpoint in {"direct-report-preview", "batch-report-preview"}:
        require(
            int(captured_stats.get("darkSampledPixels", 0)) >= 10
            and int(decoded_stats.get("darkSampledPixels", 0)) >= 10,
            f"report preview screenshot does not contain rendered report content: {checkpoint}",
        )


def validate_screenshots(root: Path, manifest: dict[str, Any]) -> list[dict[str, Any]]:
    required = {"direct-report-preview", "batch-report-preview", "navigated-away"}
    screenshots = manifest.get("screenshots") or []
    by_name = {str(row.get("checkpoint")): row for row in screenshots}
    require(required <= set(by_name), "required speaking report screenshots are missing")
    verified = []
    process_id = int(manifest.get("process", {}).get("processId", 0))
    for name in required:
        item = by_name[name]
        path = root / str(item.get("relativePath", ""))
        require(path.is_file() and sha256_file(path) == item.get("sha256"), f"screenshot bytes/hash mismatch: {name}")
        require(int(item.get("sizeBytes", 0)) > 0, f"screenshot is empty: {name}")
        require(int(item.get("processId", 0)) == process_id == int(item.get("windowProcessId", 0)), f"screenshot window is not bound to the measured PID: {name}")
        details = inspect_png(path.read_bytes())
        require(details["width"] == int(item.get("width", 0)) and details["height"] == int(item.get("height", 0)), f"screenshot dimensions differ from capture metadata: {name}")
        captured_stats = item.get("previewContentStats") or {}
        decoded_stats = details["previewContentStats"]
        validate_preview_content_stats(captured_stats, decoded_stats, name)
        verified.append({"checkpoint": name, "sha256": item["sha256"], "capturedPreviewContentStats": captured_stats, **details})
    return sorted(verified, key=lambda item: item["checkpoint"])


def validate_output(root: Path, manifest: dict[str, Any], expected_names: list[str]) -> dict[str, Any]:
    output = manifest.get("output") or {}
    archive = Path(str(output.get("archivePath", ""))).resolve(strict=True)
    require(archive.name == archive.parent.name + ".zip", "archive filename does not follow selected output-directory naming policy")
    require(archive.parent == Path(str(output.get("directory", ""))).resolve(strict=True), "archive is outside the unique selected output directory")
    require(archive.stat().st_size == int(output.get("archiveBytes", -1)) > 0, "archive size is empty or differs from manifest")
    require(sha256_file(archive) == output.get("archiveSha256"), "archive SHA-256 differs from manifest")
    output_files = [path for path in archive.parent.iterdir() if path.is_file()]
    require(len(output_files) == 1 and output_files[0] == archive, "output directory contains files other than the batch ZIP")
    require(output.get("keepIndividualPdfFiles") is False and output.get("openedFolder") is False, "batch output settings do not match ZIP-only lifecycle scope")
    require(output.get("expectedScope") == "All Students (25)" and int(output.get("expectedPdfCount", 0)) == EXPECTED_REPORT_COUNT, "batch output scope/count differs from intended default-scale workload")
    require(output.get("expectedReportNames") == expected_names, "manifest expected report names differ from seeded evaluation rows")
    expected_excluded_rows = [f"Student {index:03d}" for index in range(EXPECTED_REPORT_COUNT + 1, EXPECTED_WORKSPACE_STUDENT_COUNT + 1)]
    require(int(output.get("expectedSourceStudentCount", 0)) == EXPECTED_WORKSPACE_STUDENT_COUNT, "export manifest does not bind all 30 workspace source rows")
    require(output.get("excludedSourceRowLabels") == expected_excluded_rows, "export did not identify the final five source rows outside the default 25-row scope")
    selected = (manifest.get("workspace") or {}).get("selectedEvaluation") or {}
    expected_source_rows = [f"Student {index:03d}" for index in range(1, EXPECTED_WORKSPACE_STUDENT_COUNT + 1)]
    require(int(selected.get("studentCount", 0)) == EXPECTED_WORKSPACE_STUDENT_COUNT and selected.get("sourceRowLabels") == expected_source_rows, "prepared workspace does not preserve all 30 source rows")

    archive_result = inspect_output_archive(archive, expected_names)
    return {
        **archive_result,
        "archivePath": str(archive),
        "archiveBytes": archive.stat().st_size,
        "archiveSha256": sha256_file(archive),
    }


def _process_working_set_sample() -> dict[str, int | None]:
    if os.name == "nt":
        class ProcessMemoryCountersEx(ctypes.Structure):
            _fields_ = [
                ("cb", ctypes.c_ulong),
                ("pageFaultCount", ctypes.c_ulong),
                ("peakWorkingSetSize", ctypes.c_size_t),
                ("workingSetSize", ctypes.c_size_t),
                ("quotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("quotaPagedPoolUsage", ctypes.c_size_t),
                ("quotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("quotaNonPagedPoolUsage", ctypes.c_size_t),
                ("pagefileUsage", ctypes.c_size_t),
                ("peakPagefileUsage", ctypes.c_size_t),
                ("privateUsage", ctypes.c_size_t),
            ]

        counters = ProcessMemoryCountersEx()
        counters.cb = ctypes.sizeof(counters)
        get_info = ctypes.windll.psapi.GetProcessMemoryInfo
        get_info.argtypes = [ctypes.c_void_p, ctypes.POINTER(ProcessMemoryCountersEx), ctypes.c_ulong]
        get_info.restype = ctypes.c_int
        if get_info(ctypes.windll.kernel32.GetCurrentProcess(), ctypes.byref(counters), counters.cb):
            return {
                "workingSetBytes": int(counters.workingSetSize),
                "peakWorkingSetBytes": int(counters.peakWorkingSetSize),
            }
    return {"workingSetBytes": None, "peakWorkingSetBytes": None}


def inspect_output_archive(archive: Path, expected_names: list[str]) -> dict[str, Any]:
    reports: list[dict[str, Any]] = []
    memory_start = _process_working_set_sample()
    try:
        with zipfile.ZipFile(archive, "r") as bundle:
            corrupt = bundle.testzip()
            require(corrupt is None, f"ZIP CRC test failed for member {corrupt}")
            names = [item.filename for item in bundle.infolist() if not item.is_dir()]
            require(len(names) == len(set(names)), "ZIP archive contains duplicate members")
            expected_members = {f"{name}.pdf" for name in expected_names}
            require(set(names) == expected_members and len(names) == EXPECTED_REPORT_COUNT, "ZIP member list does not equal the first 25 selected report PDFs")
            for info in bundle.infolist():
                if info.is_dir():
                    continue
                require(".." not in Path(info.filename).parts and not info.filename.startswith(("/", "\\")), f"unsafe ZIP member path: {info.filename}")
                with bundle.open(info, "r") as member:
                    payload = member.read()
                require(len(payload) == info.file_size and len(payload) > 0, f"PDF member is empty or has a size mismatch: {info.filename}")
                pdf = inspect_pdf(payload)
                expected_name = Path(info.filename).stem
                english = "Heavy Eval 01"
                score = _score_from_report_name(expected_name)
                require(score is not None, f"PDF filename has no expected score marker: {info.filename}")
                require(_normal_text(english) in pdf["textNormalized"], f"PDF page text does not contain expected English report content: {info.filename}")
                require(_normal_text(score) in pdf["textNormalized"], f"PDF page text does not contain expected student score content: {info.filename}")
                reports.append({
                    "member": info.filename,
                    "bytes": len(payload),
                    "sha256": sha256_bytes(payload),
                    "crc32": f"{info.CRC:08x}",
                    "pageCount": pdf["pageCount"],
                    "contentStreamObjectIds": pdf["contentStreamObjectIds"],
                    "decompressedPageContentBytes": pdf["decompressedPageContentBytes"],
                    "expectedTextChecks": [english, score],
                    "textSample": " ".join(pdf["textStrings"])[:180],
                    "validatorWorkingSetAfterBytes": _process_working_set_sample().get("workingSetBytes"),
                })
                del payload, pdf
                gc.collect()
    except zipfile.BadZipFile as error:
        raise ValidationFailure(f"batch archive is not a readable ZIP: {error}") from error
    require(len(reports) == EXPECTED_REPORT_COUNT, "not all 25 selected report PDFs were page-loaded and content-checked")
    require(all(int(report["pageCount"]) >= 1 for report in reports), "one or more generated report PDFs have no loadable pages")
    reports.sort(key=lambda item: item["member"].lower())
    memory_end = _process_working_set_sample()
    peak_working_set = memory_end.get("peakWorkingSetBytes")
    return {
        "zipIntegrity": "passed",
        "pdfCount": len(reports),
        "pdfs": reports,
        "validatorMemory": {
            "workingSetStartBytes": memory_start.get("workingSetBytes"),
            "workingSetEndBytes": memory_end.get("workingSetBytes"),
            "peakWorkingSetBytes": peak_working_set,
            "membersProcessed": len(reports),
            "membersProcessedOneAtATime": True,
        },
    }


def validate_output_archive_only(evidence_directory: Path, manifest_file: Path | None = None) -> dict[str, Any]:
    root = evidence_directory.resolve(strict=True)
    manifest_path = manifest_file.resolve(strict=True) if manifest_file else root / "manifest.json"
    manifest = read_json(manifest_path)
    workspace = validate_workspace(root, manifest)
    output = manifest.get("output") or {}
    requested_archive = str(output.get("archivePath", ""))
    archive = Path(requested_archive).resolve() if requested_archive else root / "output" / "output.zip"
    require(archive.resolve(strict=True).parent == (root / "output").resolve(strict=True), "output-only archive is outside the run output directory")
    require(archive.name == archive.parent.name + ".zip", "output-only archive filename differs from its output-directory name")
    output_files = [path for path in archive.parent.iterdir() if path.is_file()]
    require(len(output_files) == 1 and output_files[0] == archive, "output-only directory contains files other than the batch ZIP")
    expected_names = list(workspace["exportReportNames"])
    archive_result = inspect_output_archive(archive, expected_names)
    return {
        "schema": VALIDATION_SCHEMA,
        "taskId": TASK_ID,
        "status": "output-archive-passed",
        "evidenceDirectory": str(root),
        "manifestStatus": manifest.get("status"),
        "workspaceSha256": workspace["workspaceSha256"],
        "sourceStudentCount": workspace["studentCount"],
        "expectedReportCount": len(expected_names),
        "archivePath": str(archive),
        "archiveBytes": archive.stat().st_size,
        "archiveSha256": sha256_file(archive),
        **archive_result,
    }


def _parse_lifecycle_lines(root: Path) -> list[dict[str, Any]]:
    path = root / "application-output.jsonl"
    require(path.is_file(), "combined application output log is missing")
    records: list[dict[str, Any]] = []
    pattern = re.compile(r"^ClassMngr lifecycle checkpoint=(\{.*\})$")
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        try:
            outer = json.loads(line)
        except json.JSONDecodeError as error:
            raise ValidationFailure(f"invalid combined application output JSON at line {line_number}: {error}") from error
        match = pattern.match(str(outer.get("line", "")))
        if match:
            try:
                record = json.loads(match.group(1))
            except json.JSONDecodeError as error:
                raise ValidationFailure(f"invalid lifecycle record at line {line_number}: {error}") from error
            record["sourceStream"] = outer.get("stream")
            record["sourceLine"] = line_number
            records.append(record)
    return records


def validate_writer_and_staging(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    records = _parse_lifecycle_lines(root)
    writers = [row for row in records if row.get("owner") == "pdf_generation"]
    creates = [row for row in writers if row.get("event") == "pdf-writer-create"]
    before = [row for row in writers if row.get("event") == "pdf-writer-release-before"]
    after = [row for row in writers if row.get("event") == "pdf-writer-release-after"]
    require(len(creates) == len(before) == len(after) == EXPECTED_REPORT_COUNT, "expected one create/before-release/after-release lifecycle for each of 25 PDFs")
    ids = {int(row.get("instance_id", 0)) for row in creates}
    require(len(ids) == EXPECTED_REPORT_COUNT and ids == {int(row.get("instance_id", 0)) for row in before} == {int(row.get("instance_id", 0)) for row in after}, "PDF writer instances are duplicated or lack paired release events")
    by_id = {writer_id: {} for writer_id in ids}
    for row in writers:
        by_id[int(row.get("instance_id", 0))].setdefault(str(row.get("event")), []).append(row)
    for writer_id, events in by_id.items():
        require(len(events.get("pdf-writer-create", [])) == 1, f"writer {writer_id} has no unique create event")
        require(len(events.get("pdf-writer-release-before", [])) == 1, f"writer {writer_id} has no unique release-before event")
        require(len(events.get("pdf-writer-release-after", [])) == 1, f"writer {writer_id} has no unique release-after event")
        create_line = int(events["pdf-writer-create"][0].get("sourceLine", 0))
        before_line = int(events["pdf-writer-release-before"][0].get("sourceLine", 0))
        after_line = int(events["pdf-writer-release-after"][0].get("sourceLine", 0))
        require(create_line < before_line < after_line, f"writer {writer_id} lifecycle events are out of order")
        require(events["pdf-writer-create"][0].get("details", {}).get("writer_present") is True, f"writer {writer_id} was absent at creation")
        require(events["pdf-writer-release-before"][0].get("details", {}).get("writer_present") is True, f"writer {writer_id} was absent before release")
        require(events["pdf-writer-release-after"][0].get("details", {}).get("writer_present") is False, f"writer {writer_id} remained present after release")

    staging = manifest.get("temporaryStaging") or {}
    require(staging.get("absenceVerifiedAfterCompletion") is True and staging.get("after") == [], "speaking-evaluation QTemporaryDir absence was not verified after export")
    require(not staging.get("before"), "speaking-evaluation staging directory existed before batch export")
    events_path = root / str(staging.get("watcherPath", ""))
    require(events_path.is_file() and sha256_file(events_path) == staging.get("watcherSha256"), "QTemporaryDir watcher log is missing or changed")
    observed = []
    for line in events_path.read_text(encoding="utf-8").splitlines():
        fields = line.split("\t")
        if len(fields) == 3 and fields[0] in {"created", "deleted"}:
            observed.append({"kind": fields[0], "path": fields[1], "capturedAtUtc": fields[2]})
    create_paths = [row["path"] for row in observed if row["kind"] == "created"]
    delete_paths = [row["path"] for row in observed if row["kind"] == "deleted"]
    require(len(create_paths) == len(delete_paths) == 1 and create_paths[0] == delete_paths[0], "QTemporaryDir create/delete watcher did not observe one paired staging folder")
    temp_root = Path(str(staging.get("environmentTempRoot", ""))).resolve(strict=True)
    stage_path = Path(create_paths[0]).resolve()
    require(stage_path.parent == temp_root and stage_path.name.startswith("ClassMngr-speaking-evaluations-"), "observed staging path is outside the isolated application TEMP root")
    require(not stage_path.exists(), "observed speaking-evaluation staging directory still exists")
    return {"writerCount": len(creates), "writerIds": sorted(ids), "stagingPath": str(stage_path), "stagingCreatedAndDeleted": True, "stagingAbsentAfterCompletion": True}


def validate_metrics_and_exit(root: Path, manifest: dict[str, Any]) -> dict[str, Any]:
    path = root / str(manifest.get("files", {}).get("processMetrics", "process-metrics.jsonl"))
    require(path.is_file(), "process metrics JSONL is missing")
    samples = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        try:
            samples.append(json.loads(line))
        except json.JSONDecodeError as error:
            raise ValidationFailure(f"invalid process metrics JSON at line {line_number}: {error}") from error
    pid = int(manifest.get("process", {}).get("processId", 0))
    require(pid > 0 and samples, "process metrics contain no measured process")
    require(all(int(row.get("pid", 0)) == pid for row in samples), "process metric contains a different PID")
    normal = [row for row in samples if row.get("category") == "normal-use"]
    operation = [row for row in samples if row.get("category") == "operation"]
    normal_by_point = {str(row.get("point")): row for row in normal}
    require(REQUIRED_NORMAL_POINTS <= set(normal_by_point), "stable working-set checkpoints are incomplete")
    for point, row in normal_by_point.items():
        require(int(row.get("workingSetBytes", 0)) < WORKING_SET_LIMIT, f"normal-use working set is not strictly below the limit at {point}")
        require(int(row.get("handles", 0)) > 0 and int(row.get("threadCount", 0)) > 0, f"process handle/thread diagnostics are invalid at {point}")
    peak = max((int(row.get("workingSetBytes", 0)) for row in operation), default=0)
    require(int(manifest.get("operationPeakWorkingSetBytes", -1)) == peak, "reported operation working-set peak differs from captured samples")
    require(manifest.get("thresholds", {}).get("privateBytesEvaluation", "").startswith("diagnostic only"), "manifest assigns an unapproved private-bytes return allowance")
    exit_record = manifest.get("processExit") or manifest.get("processExitEvidence") or {}
    require(exit_record.get("normalExit") is True and int(exit_record.get("exitCode", -1)) == 0, "packaged Next did not exit naturally with code zero")
    require(
        exit_record.get("forcedTerminationForCleanup") is False
        and exit_record.get("closeMethod") == "WindowPattern.Close"
        and exit_record.get("windowPatternCloseReturned") is True
        and int(exit_record.get("closeWindowHandle", 0)) == int((manifest.get("process") or {}).get("mainWindowHandle", 0))
        and exit_record.get("waitCompleted") is True,
        "packaged Next exit was not a bounded verified WindowPattern.Close",
    )
    process = manifest.get("process") or {}
    require(int(process.get("windowProcessId", 0)) == pid and int(process.get("windowHandleProcessId", 0)) == pid, "main window is not bound to measured PID")
    require(int(process.get("mainWindowHandle", 0)) > 0, "measured main window HWND is missing")
    return {
        "normalUseCheckpointCount": len(normal_by_point),
        "normalUseMaxWorkingSetBytes": max(int(row["workingSetBytes"]) for row in normal_by_point.values()),
        "operationSampleCount": len(operation),
        "operationPeakWorkingSetBytes": peak,
        "privateBytesDiagnosticOnly": True,
        "naturalExitCode": 0,
        "pid": pid,
        "mainWindowHandle": int(process["mainWindowHandle"]),
    }


def validate_runner_diagnostics(manifest: dict[str, Any]) -> dict[str, Any]:
    process = manifest.get("process") or {}
    app_pid = int(process.get("processId", 0))
    main_handle = int(process.get("mainWindowHandle", 0))
    runner_samples = manifest.get("runnerMetrics") or []
    require(runner_samples, "runner working-set diagnostics are missing")
    runner_pids: set[int] = set()
    runner_working_sets: list[int] = []
    for sample in runner_samples:
        runner_pid = int(sample.get("runnerProcessId", 0))
        working_set = int(sample.get("runnerWorkingSetBytes", 0))
        require(runner_pid > 0 and runner_pid != app_pid, "runner diagnostics are not separate from the measured app process")
        require(int(sample.get("measuredAppProcessId", 0)) == app_pid, "runner metric is bound to a different app PID")
        require(working_set > 0 and working_set < RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT, "PowerShell harness working set reached the 1 GiB diagnostic ceiling")
        require(int(sample.get("runnerHandles", 0)) > 0 and int(sample.get("runnerThreadCount", 0)) > 0, "runner handle/thread diagnostics are invalid")
        runner_pids.add(runner_pid)
        runner_working_sets.append(working_set)
    require(len(runner_pids) == 1, "runner diagnostics changed PowerShell process identity during the route")

    query_samples = manifest.get("uiaQueryMetrics") or []
    require(query_samples, "bounded UIA query diagnostics are missing")
    elapsed: list[float] = []
    query_runner_working_sets: list[int] = []
    exact_query_method = "Exact root identity or FindFirst with combined AutomationId + ControlType + Name conditions"
    blank_name_query_method = "FindFirst with combined AutomationId + ControlType conditions; post-query Name/PID/owner/enabled check"
    child_query_method = "FindFirst immediate child with combined AutomationId + ControlType + Name conditions; post-query PID/owner/enabled check"
    missing_container_method = "Top-level HWND enumeration filtered by measured PID and exact caption"
    allowed_containers = {
        "main",
        "QApplication.nextSpeakingEvaluationReports",
        "QApplication.nextSpeakingEvaluationBatchReports",
        "QApplication.QMessageBox",
    }
    for sample in query_samples:
        require(int(sample.get("processId", 0)) == app_pid, "UIA query diagnostic belongs to a different app PID")
        query_runner_pid = int(sample.get("runnerProcessId", 0))
        query_runner_working_set = int(sample.get("runnerWorkingSetBytes", 0))
        require(
            query_runner_pid in runner_pids and query_runner_pid != app_pid,
            "UIA query is missing separate PowerShell harness memory diagnostics",
        )
        require(
            0 < query_runner_working_set < RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT
            and int(sample.get("runnerWorkingSetGuardBytesExclusive", 0)) == RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT,
            "PowerShell harness working set reached the 768 MiB guard during UIA query diagnostics",
        )
        query_runner_working_sets.append(query_runner_working_set)
        require(str(sample.get("container", "")) in allowed_containers, "UIA query used an unapproved container")
        require(bool(sample.get("selector")), "UIA query has no named selector")
        require(int(sample.get("materializedTreeElementCount", -1)) == 0, "UIA query materialized a tree of elements")
        attempts = int(sample.get("attemptCount", -1))
        retries = int(sample.get("retryCount", -1))
        result_count = int(sample.get("resultCount", -1))
        handle = int(sample.get("rootWindowHandle", 0))
        root_pid = int(sample.get("rootWindowProcessId", 0))
        require(0 <= attempts <= 4 and retries == max(0, attempts - 1), "UIA query retry accounting is outside the bounded policy")
        require(result_count in {0, 1}, "UIA FindFirst returned more than one matching control")
        if handle == 0:
            require(result_count == 0 and attempts == 0 and str(sample.get("method", "")) == missing_container_method, "missing dialog root was not handled as a bounded top-level HWND lookup")
            require(sample.get("postQueryIdentityCheck") is False, "missing dialog root claims a post-query identity check")
        else:
            require(root_pid == app_pid, "UIA query root HWND belongs to a different PID")
            method = str(sample.get("method", ""))
            if sample.get("container") == "main":
                require(handle == main_handle, "main-window UIA query escaped the measured main HWND")
            require(1 <= attempts <= 4, "UIA query did not use the bounded attempt limit")
            require(int(sample.get("ancestorWindowHandle", 0)) == handle and int(sample.get("ancestorWindowProcessId", 0)) == app_pid, "UIA target ancestry is not bound to the verified query HWND/PID")
            name_applied = sample.get("nameConditionApplied")
            condition_properties = sample.get("conditionProperties")
            require(isinstance(name_applied, bool), "UIA query does not record whether Name was part of its condition")
            require(isinstance(condition_properties, list), "UIA query does not record its combined condition properties")
            if sample.get("queryScope") == "Children":
                require(method == child_query_method, "immediate child UIA lookup used an unexpected query method")
                require(condition_properties == ["AutomationId", "ControlType", "Name"] and name_applied is True, "immediate child lookup did not include all three exact conditions")
                expected_parent = {
                    "speakingEvaluationClassItem": "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationContentSplitter.speakingEvaluationClassList",
                    "speakingEvaluationNavigationItem": "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree",
                    "myWorkspaceNavigationItem": "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextNavigationPanel.classMngrNextNavigationTree",
                }.get(str(sample.get("selector", "")))
                require(expected_parent is not None and sample.get("parentAutomationId") == expected_parent, "immediate child lookup used an unexpected selector or parent")
            elif name_applied is False:
                require(method == blank_name_query_method, "blank-Name UIA lookup did not use the approved two-property query and post-check")
                require(condition_properties == ["AutomationId", "ControlType"], "blank-Name lookup included unexpected or missing conditions")
                require(sample.get("expectedName") == "", "blank-Name lookup expected a nonblank Name")
            else:
                require(method == exact_query_method, "named UIA query did not use an exact root or combined three-property selector")
                require(condition_properties == ["AutomationId", "ControlType", "Name"], "named UIA query does not record all three exact conditions")
            if result_count == 1:
                require(sample.get("postQueryIdentityCheck") is True, "found UIA element lacks post-query identity validation")
                require(sample.get("observedName") == sample.get("expectedName"), "found UIA element Name differs from its exact post-query expectation")
                if not name_applied or sample.get("queryScope") == "Children":
                    require(bool(sample.get("targetEnabled")) is True, "blank-Name/immediate-child UIA element was not confirmed enabled")
            else:
                require(sample.get("postQueryIdentityCheck") is False, "missing UIA element claims a post-query identity check")
        duration = float(sample.get("elapsedMilliseconds", -1))
        require(math.isfinite(duration) and duration >= 0, "UIA query elapsed time is invalid")
        elapsed.append(duration)
    return {
        "runnerProcessId": next(iter(runner_pids)),
        "runnerMetricCount": len(runner_samples),
        "runnerMaxWorkingSetBytes": max(runner_working_sets),
        "runnerWorkingSetDiagnosticCeilingBytes": RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT,
        "uiaQueryCount": len(query_samples),
        "uiaQueryMaxElapsedMilliseconds": max(elapsed),
        "uiaQueryRunnerMaxWorkingSetBytes": max(query_runner_working_sets),
        "uiaQueryMaterializedTreeElementCount": 0,
    }


def validate_uia_wait_metrics(manifest: dict[str, Any], expected_selectors: set[str] | None = None) -> dict[str, Any]:
    process = manifest.get("process") or {}
    app_pid = int(process.get("processId", 0))
    main_handle = int(process.get("mainWindowHandle", 0))
    records = manifest.get("uiaWaitMetrics") or []
    require(records, "bounded post-route UIA wait diagnostics are missing")
    by_selector: dict[str, dict[str, Any]] = {}
    for record in records:
        selector = str(record.get("selector", ""))
        require(bool(selector) and selector not in by_selector, "post-route UIA wait selector is missing or duplicated")
        require(int(record.get("processId", 0)) == app_pid, "post-route UIA wait belongs to a different app PID")
        require(int(record.get("mainWindowHandle", 0)) == main_handle and int(record.get("mainWindowProcessId", 0)) == app_pid, "post-route UIA wait is not bound to the measured main HWND/PID")
        require(record.get("status") == "found" and record.get("found") is True, f"post-route UIA control was not found: {selector}")
        attempts = int(record.get("attemptCount", 0))
        require(1 <= attempts <= 60 and int(record.get("maximumAttempts", 0)) == 60, f"post-route UIA polling exceeded its 60-attempt cap: {selector}")
        require(int(record.get("intervalMilliseconds", 0)) == 250, f"post-route UIA polling interval differs from 250 ms: {selector}")
        elapsed = float(record.get("elapsedMilliseconds", -1))
        require(math.isfinite(elapsed) and elapsed >= 0, f"post-route UIA wait elapsed time is invalid: {selector}")
        require(int(record.get("queryCountEnd", 0)) - int(record.get("queryCountStart", 0)) >= attempts, f"post-route UIA wait did not record every query attempt: {selector}")
        require(int(record.get("materializedTreeElementCount", -1)) == 0, f"post-route UIA wait materialized a tree: {selector}")
        require(0 < int(record.get("appWorkingSetStartBytes", 0)) < WORKING_SET_LIMIT, f"post-route app working set at wait start exceeded the strict cap: {selector}")
        require(0 < int(record.get("appWorkingSetEndBytes", 0)) < WORKING_SET_LIMIT, f"post-route app working set at wait end exceeded the strict cap: {selector}")
        require(0 < int(record.get("appWorkingSetMaxBytes", 0)) < WORKING_SET_LIMIT, f"post-route app working set during wait exceeded the strict cap: {selector}")
        require(0 < int(record.get("runnerWorkingSetStartBytes", 0)) < RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT, f"runner working set at wait start reached its diagnostic ceiling: {selector}")
        require(0 < int(record.get("runnerWorkingSetEndBytes", 0)) < RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT, f"runner working set at wait end reached its diagnostic ceiling: {selector}")
        require(0 < int(record.get("runnerWorkingSetMaxBytes", 0)) < RUNNER_WORKING_SET_DIAGNOSTIC_LIMIT, f"runner working set during wait reached its diagnostic ceiling: {selector}")
        require(int(record.get("runnerProcessId", 0)) > 0 and int(record.get("runnerProcessId", 0)) != app_pid, "UIA wait does not separate the harness PID from the app PID")
        by_selector[selector] = record
    if expected_selectors is not None:
        require(expected_selectors <= set(by_selector), "required bounded post-route UIA wait records are missing")
    return {"waitCount": len(records), "selectors": sorted(by_selector), "maximumAttemptCount": max(int(row["attemptCount"]) for row in records), "maximumElapsedMilliseconds": max(float(row["elapsedMilliseconds"]) for row in records)}


def validate_uia_query_burst_diagnostic(manifest: dict[str, Any]) -> dict[str, Any]:
    process = manifest.get("process") or {}
    app_pid = int(process.get("processId", 0))
    record = manifest.get("uiaQueryBurstDiagnostic") or {}
    selectors = ["speakingEvaluationClassList", "speakingEvaluationChoice", "speakingEvaluationSaveReports"]
    require(record.get("status") == "passed" and record.get("failureMessage") is None, "bounded UIA memory diagnostic did not pass")
    require(int(record.get("appProcessId", 0)) == app_pid, "UIA memory diagnostic belongs to a different app PID")
    require(int(record.get("iterations", 0)) == 300 and int(record.get("completedIterations", 0)) == 300, "UIA memory diagnostic did not complete 300 bounded iterations")
    require(int(record.get("intervalMilliseconds", 0)) == 100 and record.get("selectors") == selectors and int(record.get("queriesPerIteration", 0)) == 3, "UIA memory diagnostic selector or cadence differs from the fixed scope")
    expected_query_count = 900
    require(int(record.get("expectedQueryCount", 0)) == expected_query_count and int(record.get("observedQueryCount", 0)) == expected_query_count, "UIA memory diagnostic query count differs from 900")
    require(int(record.get("queryCountEnd", 0)) - int(record.get("queryCountStart", 0)) == expected_query_count, "UIA memory diagnostic range does not contain exactly 900 FindFirst queries")
    require(int(record.get("materializedTreeElementCount", -1)) == 0, "UIA memory diagnostic materialized a tree")
    app_samples = [int(record.get(key, 0)) for key in ("appWorkingSetStartBytes", "appWorkingSetMaxBytes", "appWorkingSetEndBytes")]
    require(all(0 < value < WORKING_SET_LIMIT for value in app_samples), "app working set during UIA memory diagnostic exceeded the strict cap")
    runner_samples = [int(record.get(key, 0)) for key in ("runnerWorkingSetStartBytes", "runnerWorkingSetMaxBytes", "runnerWorkingSetEndBytes")]
    require(all(0 < value < RUNNER_QUERY_BURST_WORKING_SET_LIMIT for value in runner_samples), "harness working set during UIA memory diagnostic reached the 768 MiB guard")
    require(int(record.get("runnerWorkingSetGuardBytesExclusive", 0)) == RUNNER_QUERY_BURST_WORKING_SET_LIMIT, "UIA diagnostic memory guard differs from the approved value")
    require(int(record.get("runnerProcessId", 0)) > 0 and int(record.get("runnerProcessId", 0)) != app_pid, "UIA memory diagnostic does not separate app and harness processes")
    elapsed = float(record.get("elapsedMilliseconds", -1))
    require(math.isfinite(elapsed) and 20_000 <= elapsed <= 60_000, "UIA memory diagnostic duration was outside the bounded 20–60 second window")
    route = ((manifest.get("uiaActions") or {}).get("navigateToSpeakingEvaluation") or {}).get("pageRouteEvent") or {}
    require(route.get("observed") is True and route.get("targetPageId") == "speaking_evaluation" and str(record.get("startedAtUtc", "")) >= str(route.get("observedAtUtc", "")), "UIA memory diagnostic did not run after the verified Speaking Evaluation route")
    return {"iterations": 300, "findFirstQueryCount": expected_query_count, "elapsedMilliseconds": elapsed, "runnerWorkingSetMaxBytes": max(runner_samples), "runnerWorkingSetGuardBytesExclusive": RUNNER_QUERY_BURST_WORKING_SET_LIMIT, "appWorkingSetMaxBytes": max(app_samples)}


def validate_export_completion_uia_probe(manifest: dict[str, Any]) -> dict[str, Any]:
    process = manifest.get("process") or {}
    app_pid = int(process.get("processId", 0))
    main_handle = int(process.get("mainWindowHandle", 0))
    record = manifest.get("exportCompletionUiaProbe") or {}
    require(record.get("status") in {"reports-ready-notice-found", "batch-dialog-closed"}, "single bounded export-completion UIA probe did not observe a completed route state")
    require(int(record.get("attemptCount", 0)) == int(record.get("maximumAttempts", 0)) == 1, "export-completion UIA probe exceeded one attempt")
    require(int(record.get("archiveStableCheckCount", 0)) == 3, "export-completion UIA probe did not follow three stable archive samples")
    require(int(record.get("processId", 0)) == app_pid and int(record.get("mainWindowHandle", 0)) == main_handle and int(record.get("mainWindowProcessId", 0)) == app_pid, "export-completion UIA probe is not bound to the measured app HWND/PID")
    require(int(record.get("materializedTreeElementCount", -1)) == 0, "export-completion UIA probe materialized an element tree")
    require(int(record.get("runnerProcessId", 0)) > 0 and int(record.get("runnerProcessId", 0)) != app_pid, "export-completion UIA probe does not separate app and harness processes")
    for key in ("runnerWorkingSetBeforeBytes", "runnerWorkingSetAfterBytes"):
        require(0 < int(record.get(key, 0)) < RUNNER_QUERY_BURST_WORKING_SET_LIMIT, f"runner working set reached the 768 MiB guard during export completion: {key}")
    for key in ("appWorkingSetBeforeBytes", "appWorkingSetAfterBytes"):
        require(int(record.get(key, 0)) > 0, f"export-completion app working-set sample is invalid: {key}")
    notice_found = record.get("noticeWindowFound") is True
    batch_found = record.get("batchDialogFound") is True
    if record.get("status") == "reports-ready-notice-found":
        require(notice_found and not batch_found, "Reports Ready UIA state is internally inconsistent")
        expected_query_count = 1
    else:
        require(not notice_found and not batch_found, "batch-dialog-closed UIA state is internally inconsistent")
        expected_query_count = 2
    require(int(record.get("queryCountEnd", 0)) - int(record.get("queryCountStart", 0)) == expected_query_count, "export-completion probe did not preserve its bounded query count")
    elapsed = float(record.get("elapsedMilliseconds", -1))
    require(math.isfinite(elapsed) and 0 <= elapsed <= 60_000, "export-completion UIA probe elapsed time is invalid")
    return {"status": str(record["status"]), "attemptCount": 1, "uiaQueryCount": expected_query_count, "elapsedMilliseconds": elapsed, "runnerWorkingSetAfterBytes": int(record["runnerWorkingSetAfterBytes"])}


def validate_uia_absence_waits(manifest: dict[str, Any]) -> dict[str, Any]:
    process = manifest.get("process") or {}
    app_pid = int(process.get("processId", 0))
    main_handle = int(process.get("mainWindowHandle", 0))
    records = manifest.get("uiaAbsenceWaitMetrics") or []
    expected = [
        "nextSpeakingEvaluationReports",
        "nextSpeakingEvaluationReports",
        "qt_msgbox_buttonbox",
        "nextSpeakingEvaluationBatchReports",
    ]
    require(len(records) == len(expected), "bounded UIA absence wait records are missing or duplicated")
    selectors: list[str] = []
    for record in records:
        selector = str(record.get("selector", ""))
        selectors.append(selector)
        require(record.get("status") in {"not-found", "offscreen"} and record.get("absenceObserved") is True, f"UIA control was still exposed after its close action: {selector}")
        require(int(record.get("processId", 0)) == app_pid and int(record.get("mainWindowHandle", 0)) == main_handle and int(record.get("mainWindowProcessId", 0)) == app_pid, f"UIA absence wait is not bound to measured app PID/HWND: {selector}")
        require(int(record.get("materializedTreeElementCount", -1)) == 0, f"UIA absence wait materialized a tree: {selector}")
        attempts = int(record.get("attemptCount", 0))
        require(1 <= attempts <= 60 and int(record.get("maximumAttempts", 0)) <= 60 and int(record.get("intervalMilliseconds", 0)) == 100, f"UIA absence wait exceeded its bound: {selector}")
        require(int(record.get("queryCountEnd", 0)) - int(record.get("queryCountStart", 0)) >= attempts, f"UIA absence wait did not preserve its query count: {selector}")
        elapsed = float(record.get("elapsedMilliseconds", -1))
        require(math.isfinite(elapsed) and elapsed >= 0, f"UIA absence wait elapsed time is invalid: {selector}")
        for key in ("runnerWorkingSetStartBytes", "runnerWorkingSetMaxBytes", "runnerWorkingSetEndBytes"):
            require(0 < int(record.get(key, 0)) < RUNNER_QUERY_BURST_WORKING_SET_LIMIT, f"runner working set reached the guard during UIA absence wait: {selector}/{key}")
        require(int(record.get("runnerProcessId", 0)) > 0 and int(record.get("runnerProcessId", 0)) != app_pid, "UIA absence wait does not distinguish runner and app PIDs")
    require(selectors == expected, "UIA absence waits did not cover direct preview, nested preview, success notice, and batch dialog in order")
    return {"waitCount": len(records), "selectors": selectors, "maximumAttemptCount": max(int(row["attemptCount"]) for row in records), "runnerWorkingSetMaxBytes": max(int(row["runnerWorkingSetMaxBytes"]) for row in records)}


def validate_probe_manifest(
    root: Path,
    manifest: dict[str, Any],
    provenance: dict[str, Any],
    workspace: dict[str, Any],
    settings: dict[str, Any],
) -> dict[str, Any]:
    require(manifest.get("probeOnly") is True, "probe manifest does not identify a probe-only run")
    require(manifest.get("status") in {"probe-captured-awaiting-validation", "probe-passed"}, "probe did not finish its bounded route and natural close")
    process = manifest.get("process") or {}
    process_id = int(process.get("processId", 0))
    main_handle = int(process.get("mainWindowHandle", 0))
    require(process_id > 0 and main_handle > 0, "probe process or measured main HWND is missing")
    require(int(process.get("windowProcessId", 0)) == process_id == int(process.get("windowHandleProcessId", 0)), "probe main HWND is not bound to the measured PID")
    require(process.get("architecture") == "x64", "probe app is not x64")
    require(Path(str(process.get("processImagePath", ""))).resolve(strict=True) == Path(str(manifest.get("application", {}).get("path", ""))).resolve(strict=True), "probe process image differs from the packaged app")
    require(process.get("processImageSha256") == manifest.get("application", {}).get("sha256"), "probe process image hash differs from the measured package")
    require(manifest.get("output") is None and not (manifest.get("screenshots") or []), "probe unexpectedly captured report output or preview artifacts")

    navigation_probe = manifest.get("navigationItemProbe") or {}
    navigation_target = navigation_probe.get("target") or {}
    navigation_query = navigation_probe.get("query") or {}
    require(
        navigation_target.get("name") == "My Workspace"
        and navigation_target.get("controlType") == "ControlType.TreeItem"
        and str(navigation_target.get("automationId", "")).endswith(".classMngrNextNavigationTree")
        and navigation_target.get("isEnabled") is True
        and navigation_target.get("isOffscreen") is False,
        "probe did not find the enabled visible My Workspace navigation item",
    )
    require(
        int(navigation_probe.get("processId", 0)) == process_id
        and int(navigation_probe.get("windowProcessId", 0)) == process_id
        and int(navigation_probe.get("windowHandle", 0)) == main_handle,
        "probe My Workspace navigation item is not bound to the measured main HWND/PID",
    )
    require(
        navigation_query.get("selector") == "myWorkspaceNavigationItem"
        and navigation_query.get("expectedName") == "My Workspace"
        and navigation_query.get("observedName") == "My Workspace"
        and navigation_query.get("queryScope") == "Children"
        and navigation_query.get("parentAutomationId") == navigation_target.get("automationId")
        and int(navigation_query.get("resultCount", 0)) == 1
        and navigation_query.get("postQueryIdentityCheck") is True,
        "probe My Workspace navigation item lacks its exact process-bound FindFirst record",
    )

    snapshots = manifest.get("uiaSnapshots") or {}
    required_snapshots = {
        "startup-ui-tree-before-navigation",
        "startup-ready",
        "speaking-evaluation-page-ui-tree-before-controls",
        "speaking-evaluation-page-ready",
        "probe-my-workspace-after-navigation",
    }
    require(required_snapshots <= set(snapshots), "bounded probe UIA snapshots are incomplete")
    loaded: dict[str, dict[str, Any]] = {}
    for name, info in snapshots.items():
        path = root / str(info.get("relativePath", ""))
        require(path.is_file() and sha256_file(path) == info.get("sha256"), f"probe UIA snapshot hash mismatch: {name}")
        snapshot = read_json(path)
        require(snapshot.get("coverage") == "bounded named UIA controls only", f"probe UIA snapshot is not bounded: {name}")
        require(int(snapshot.get("processId", 0)) == process_id, f"probe UIA snapshot belongs to a different process: {name}")
        require(int(snapshot.get("mainWindowHandle", 0)) == main_handle, f"probe UIA snapshot main HWND differs: {name}")
        loaded[name] = snapshot
    startup = loaded["startup-ready"]
    require(manifest.get("startupReady", {}).get("reportsLoaded") is False, "probe startup did not assert that reports were unloaded")
    require(not _uia_matches(startup, "nextSpeakingEvaluationReports") and not _uia_matches(startup, "nextReportStudent"), "probe startup already exposed a report preview")
    startup_counts = ((manifest.get("startupReady") or {}).get("ui") or {}).get("controls") or {}
    require(int(startup_counts.get("nextSpeakingEvaluationReports", -1)) == 0 and int(startup_counts.get("nextSpeakingEvaluationBatchReports", -1)) == 0 and int(startup_counts.get("nextReportStudent", -1)) == 0, "probe startup control counts show reports already loaded")

    page = loaded["speaking-evaluation-page-ready"]
    for automation_id in ("speakingEvaluationTitle", "speakingEvaluationClassList", "speakingEvaluationChoice", "speakingEvaluationReports", "speakingEvaluationSaveReports"):
        require(len(_uia_matches(page, automation_id)) == 1, f"probe page-ready snapshot lacks a unique {automation_id} control")
    choice = _uia_matches(page, "speakingEvaluationChoice")[0]
    require(_label(choice) == "Winter", "probe did not select the authorized Winter evaluation")
    page_checkpoint = (manifest.get("checkpoints") or {}).get("speaking-evaluation-page-ready") or {}
    require(page_checkpoint.get("selectedClass") == "Heavy Fixture Class 001" and page_checkpoint.get("selectedEvaluation") == "Winter", "probe page-ready checkpoint differs from the selected fixture")
    require(int(page_checkpoint.get("sourceStudentCount", 0)) == EXPECTED_WORKSPACE_STUDENT_COUNT == int(page_checkpoint.get("expectedSourceStudentCount", 0)), "probe page-ready checkpoint does not retain all 30 workspace rows")
    require(int(page_checkpoint.get("expectedExportReportCount", 0)) == EXPECTED_REPORT_COUNT, "probe page-ready checkpoint changed the 25-report export scope")
    workspace_page = loaded["probe-my-workspace-after-navigation"]
    workspace_title = [
        row
        for row in workspace_page.get("elements", [])
        if row.get("automationId") == "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.pageTitle"
        and row.get("controlType") == "ControlType.Text"
        and row.get("name") == "My Workspace"
    ]
    require(len(workspace_title) == 1, "probe did not capture the My Workspace page after its route event")
    require(
        not _uia_matches(workspace_page, "speakingEvaluationReports")
        and not _uia_matches(workspace_page, "nextSpeakingEvaluationReports")
        and not _uia_matches(workspace_page, "nextReportStudent"),
        "no-export navigation probe retained speaking-report controls",
    )
    workspace_route_checkpoint = (manifest.get("checkpoints") or {}).get("probe-my-workspace-route") or {}
    require(workspace_route_checkpoint.get("targetPage") == "my_workspace" and workspace_route_checkpoint.get("reportControlsRetained") is False, "probe My Workspace checkpoint does not prove report-control release")
    wait_summary = validate_uia_wait_metrics(manifest, {"speakingEvaluationClassList", "speakingEvaluationChoice", "myWorkspaceTitle"})
    burst_summary = validate_uia_query_burst_diagnostic(manifest)
    waits = {str(row.get("selector")): row for row in manifest.get("uiaWaitMetrics", [])}
    for selector, automation_id, control_type in (
        ("speakingEvaluationClassList", "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationContentSplitter.speakingEvaluationClassList", "ControlType.List"),
        ("speakingEvaluationChoice", "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationChoice", "ControlType.ComboBox"),
    ):
        wait = waits[selector]
        require(wait.get("automationId") == automation_id and wait.get("controlType") == control_type and wait.get("expectedName") == "", f"post-route UIA wait selector identity differs: {selector}")

    action_map = manifest.get("uiaActions") or {}
    require({"navigateToSpeakingEvaluation", "selectFixtureClass", "navigateAwayProbe"} <= set(action_map), "probe is missing required navigation or fixture selection action")
    navigation = action_map["navigateToSpeakingEvaluation"]
    _validate_window_targeted_navigation_click(navigation, process_id)
    navigate_away = action_map["navigateAwayProbe"]
    _validate_window_targeted_navigation_click(navigate_away, process_id)
    require(navigate_away.get("pageRouteEvent") == workspace_route_checkpoint.get("pageRouteEvent"), "probe My Workspace route action differs from its page checkpoint")
    require(int(navigate_away.get("windowHandle", 0)) == main_handle, "probe My Workspace click did not target the measured main HWND")
    select = action_map["selectFixtureClass"]
    require(select.get("actionKind") == "select-fixture-class" and select.get("expectedName") == select.get("observedName") == "Heavy Fixture Class 001", "probe class selection target is not the seeded class")
    require(select.get("method") == "SelectionItemPattern.Select", "probe class selection used an unexpected UIA method")
    require(int(select.get("processId", 0)) == process_id == int(select.get("windowProcessId", 0)), "probe class selection is not PID/window bound")
    require(int(select.get("windowHandle", 0)) == main_handle, "probe class selection did not target the measured main window")
    select_target = select.get("target") or {}
    require(select_target.get("name") == "Heavy Fixture Class 001" and select_target.get("automationId") == "QApplication.classMngrNextApplicationShell.QWidget.classMngrNextPageHost.QStackedWidget.QWidget.speakingEvaluationContentSplitter.speakingEvaluationClassList" and select_target.get("controlType") == "ControlType.ListItem" and select_target.get("isEnabled") is True and select_target.get("isOffscreen") is False, "probe class selection target is not the visible enabled immediate class-list ListItem")
    selection_check = select.get("selectionVerification") or {}
    require(selection_check.get("itemIsSelected") is True and int(selection_check.get("selectedChildCount", 0)) == 1 and selection_check.get("selectedChildNames") == ["Heavy Fixture Class 001"] and selection_check.get("parentAutomationId") == select_target.get("automationId"), "probe does not prove the immediate class-list selection state")
    actions_path = root / str(manifest.get("files", {}).get("uiaActions", "uia-actions.jsonl"))
    require(actions_path.is_file(), "probe UIA action log is missing")
    rows = [json.loads(line) for line in actions_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    route_observations = [row for row in rows if row.get("recordType") == "page-route-observation" and row.get("associatedActionKind") == "navigate-to-speaking-evaluation"]
    require(len(route_observations) == 1 and route_observations[0].get("observedPageRouteEvent") == navigation.get("pageRouteEvent"), "probe action log does not independently preserve the page-route event")
    away_route_observations = [row for row in rows if row.get("recordType") == "page-route-observation" and row.get("associatedActionKind") == "navigate-to-my-workspace-probe"]
    require(len(away_route_observations) == 1 and away_route_observations[0].get("observedPageRouteEvent") == navigate_away.get("pageRouteEvent"), "probe action log does not independently preserve the My Workspace page-route event")
    require(
        str(navigation.get("issuedAtUtc", "")) <= str(select.get("issuedAtUtc", "")) <= str(page_checkpoint.get("capturedAtUtc", "")) <= str(navigate_away.get("issuedAtUtc", "")) <= str(workspace_route_checkpoint.get("capturedAtUtc", "")),
        "probe UIA actions and page-ready checkpoint are out of order",
    )

    metric_path = root / str(manifest.get("files", {}).get("processMetrics", "process-metrics.jsonl"))
    require(metric_path.is_file(), "probe app working-set metrics are missing")
    app_samples = [json.loads(line) for line in metric_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    require(app_samples and all(int(sample.get("pid", 0)) == process_id for sample in app_samples), "probe app working-set samples contain a different PID")
    stable = {str(row.get("point")): row for row in app_samples if row.get("category") == "normal-use"}
    required_points = {"startup-ready", "speaking-evaluation-page-ready"}
    require(required_points <= set(stable), "probe is missing stable app working-set checkpoints")
    for point in required_points:
        row = stable[point]
        require(int(row.get("workingSetBytes", 0)) < WORKING_SET_LIMIT, f"probe app working set exceeded the strict cap at {point}")
        require(int(row.get("handles", 0)) > 0 and int(row.get("threadCount", 0)) > 0, f"probe app process diagnostics are invalid at {point}")
    exit_record = manifest.get("processExit") or manifest.get("processExitEvidence") or {}
    require(exit_record.get("normalExit") is True and int(exit_record.get("exitCode", -1)) == 0, "probe app did not exit naturally with code zero")
    require(exit_record.get("forcedTerminationForCleanup") is False and exit_record.get("closeMethod") == "WindowPattern.Close" and exit_record.get("windowPatternCloseReturned") is True, "probe app was not closed through a verified WindowPattern.Close")
    require(int(exit_record.get("closeWindowHandle", 0)) == main_handle and exit_record.get("waitCompleted") is True, "probe natural close is not bound to the measured main HWND")
    runner_diagnostics = validate_runner_diagnostics(manifest)
    return {
        "schema": VALIDATION_SCHEMA,
        "taskId": TASK_ID,
        "status": "passed",
        "evidenceDirectory": str(root),
        "validatedAtUtc": __import__("datetime").datetime.now(__import__("datetime").timezone.utc).isoformat(),
        "probeOnly": True,
        "provenance": provenance,
        "workspace": workspace,
        "runtimeSettings": settings,
        "uia": {"snapshotCount": len(loaded), "navigationAndSelectionActions": 2, "selectedEvaluation": "Winter", "pageRoute": navigation["pageRouteEvent"], "boundedWaits": wait_summary, "queryMemoryDiagnostic": burst_summary},
        "metricsAndExit": {
            "stableAppCheckpointCount": len(stable),
            "stableAppMaxWorkingSetBytes": max(int(row["workingSetBytes"]) for row in stable.values()),
            "strictAppWorkingSetLimitBytesExclusive": WORKING_SET_LIMIT,
            "naturalExitCode": 0,
            "pid": process_id,
            "mainWindowHandle": main_handle,
            "runnerDiagnostics": runner_diagnostics,
        },
    }


def validate_manifest(evidence_directory: Path, manifest_file: Path | None = None) -> dict[str, Any]:
    root = evidence_directory.resolve(strict=True)
    manifest_path = manifest_file.resolve(strict=True) if manifest_file else root / "manifest.json"
    manifest = read_json(manifest_path)
    require(manifest.get("schema") == SCHEMA and manifest.get("taskId") == TASK_ID, "P9-13 manifest identity mismatch")
    require(manifest.get("status") in {"probe-captured-awaiting-validation", "probe-passed", "captured-awaiting-validation", "passed"}, "runner did not complete a clean evidence capture")
    require(manifest.get("routeMode") == "current-next-speaking-evaluation-ui-packaged-x64-release-on-local-windows", "route mode is not the authentic current Next packaged Windows route")
    provenance = validate_source_and_package(root, manifest)
    workspace = validate_workspace(root, manifest)
    settings = validate_runtime_settings(root, manifest)
    process = manifest.get("process") or {}
    require(int(process.get("processId", 0)) > 0, "measured packaged process PID is missing")
    require(Path(str(process.get("processImagePath", ""))).resolve(strict=True) == Path(str(manifest.get("application", {}).get("path", ""))).resolve(strict=True), "process image path differs from packaged application")
    require(process.get("processImageSha256") == manifest.get("application", {}).get("sha256"), "live process image hash differs from package")
    require(process.get("architecture") == "x64", "measured packaged process is not x64")
    if manifest.get("probeOnly") is True:
        return validate_probe_manifest(root, manifest, provenance, workspace, settings)
    require(manifest.get("probeOnly") is False, "full lifecycle manifest has an invalid probeOnly flag")
    expected_names = selected_export_report_names(list(workspace["reportNames"]))
    expected_first = expected_names[0]
    uia = validate_uia(root, manifest, expected_first, str(manifest.get("output", {}).get("directory", "")))
    screenshots = validate_screenshots(root, manifest)
    output = validate_output(root, manifest, expected_names)
    lifecycle = validate_writer_and_staging(root, manifest)
    metrics = validate_metrics_and_exit(root, manifest)
    wait_summary = validate_uia_wait_metrics(manifest, {"speakingEvaluationClassList", "speakingEvaluationChoice"})
    completion_probe = validate_export_completion_uia_probe(manifest)
    absence_waits = validate_uia_absence_waits(manifest)
    require(len(manifest.get("lifecycleEvents") or []) >= 75, "manifest did not preserve the 75 PDF writer lifecycle records")
    return {
        "schema": VALIDATION_SCHEMA,
        "taskId": TASK_ID,
        "status": "passed",
        "evidenceDirectory": str(root),
        "validatedAtUtc": __import__("datetime").datetime.now(__import__("datetime").timezone.utc).isoformat(),
        "provenance": provenance,
        "workspace": workspace,
        "runtimeSettings": settings,
        "uia": uia,
        "screenshots": screenshots,
        "output": output,
        "lifecycle": lifecycle,
        "metricsAndExit": metrics,
        "runnerDiagnostics": validate_runner_diagnostics(manifest),
        "uiaWaitDiagnostics": wait_summary,
        "exportCompletionUiaProbe": completion_probe,
        "uiaAbsenceWaitDiagnostics": absence_waits,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-directory", type=Path, required=True)
    parser.add_argument("--manifest-file", type=Path, help="Optional reconciled manifest path; artifact paths remain rooted at --evidence-directory.")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--output-archive-only", action="store_true")
    parser.add_argument("--quiet", action="store_true")
    options = parser.parse_args()
    try:
        result = (
            validate_output_archive_only(options.evidence_directory, options.manifest_file)
            if options.output_archive_only
            else validate_manifest(options.evidence_directory, options.manifest_file)
        )
        payload = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
        if options.output:
            options.output.parent.mkdir(parents=True, exist_ok=True)
            options.output.write_text(payload, encoding="utf-8")
        if not options.quiet:
            sys.stdout.write(payload)
        return 0
    except Exception as error:
        result = {
            "schema": VALIDATION_SCHEMA,
            "taskId": TASK_ID,
            "status": "failed",
            "evidenceDirectory": str(options.evidence_directory),
            "error": str(error),
        }
        if options.output:
            options.output.parent.mkdir(parents=True, exist_ok=True)
            options.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        sys.stderr.write(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
