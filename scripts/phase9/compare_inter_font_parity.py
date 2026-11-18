#!/usr/bin/env python3
"""Compare six static Inter faces with matching faces inside a TTC.

This is a focused offline diagnostic for P9-06U. It checks OpenType identity,
Unicode cmap mappings, glyph outlines, horizontal metrics, and selected
shaping/layout tables. It does not install fonts or modify its inputs.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import struct
import sys
from typing import Any


EXPECTED_FACES = (
    ("regular", "Inter-Regular.ttf", "Regular", 400, False),
    ("medium", "Inter-Medium.ttf", "Medium", 500, False),
    ("semibold", "Inter-SemiBold.ttf", "SemiBold", 600, False),
    ("bold", "Inter-Bold.ttf", "Bold", 700, False),
    ("bold-italic", "Inter-BoldItalic.ttf", "Bold Italic", 700, True),
    ("black", "Inter-Black.ttf", "Black", 900, False),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def i16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">h", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def parse_ttc(data: bytes) -> list[int]:
    if data[:4] != b"ttcf" or len(data) < 12:
        raise ValueError("The package Inter resource is not a TrueType Collection.")
    version = u32(data, 4)
    count = u32(data, 8)
    if version not in (0x00010000, 0x00020000) or count == 0:
        raise ValueError(f"Unsupported or empty TTC header: version=0x{version:08x}, faces={count}")
    if 12 + count * 4 > len(data):
        raise ValueError("The TTC face-offset array is truncated.")
    offsets = [u32(data, 12 + index * 4) for index in range(count)]
    if any(offset + 12 > len(data) for offset in offsets):
        raise ValueError("The TTC contains an out-of-range face offset.")
    return offsets


def parse_tables(data: bytes, face_offset: int) -> dict[str, tuple[int, int]]:
    if face_offset + 12 > len(data):
        raise ValueError("The sfnt header is truncated.")
    count = u16(data, face_offset + 4)
    if face_offset + 12 + count * 16 > len(data):
        raise ValueError("The sfnt table directory is truncated.")
    tables: dict[str, tuple[int, int]] = {}
    for index in range(count):
        entry = face_offset + 12 + index * 16
        tag = data[entry : entry + 4].decode("latin-1")
        offset = u32(data, entry + 8)
        length = u32(data, entry + 12)
        if offset + length > len(data):
            raise ValueError(f"The {tag!r} table exceeds the font file bounds.")
        tables[tag] = (offset, length)
    return tables


def names_for(data: bytes, table: tuple[int, int]) -> dict[int, list[tuple[int, int, str]]]:
    offset, length = table
    if length < 6:
        raise ValueError("The OpenType name table is too short.")
    count = u16(data, offset + 2)
    string_offset = u16(data, offset + 4)
    if 6 + count * 12 > length:
        raise ValueError("The OpenType name records are truncated.")
    names: dict[int, list[tuple[int, int, str]]] = {}
    for index in range(count):
        record = offset + 6 + index * 12
        platform = u16(data, record)
        language = u16(data, record + 4)
        name_id = u16(data, record + 6)
        byte_length = u16(data, record + 8)
        relative = u16(data, record + 10)
        if platform not in (0, 3):
            continue
        start = offset + string_offset + relative
        end = start + byte_length
        if end > offset + length:
            raise ValueError("An OpenType name string is outside its table.")
        value = data[start:end].decode("utf-16-be", errors="replace").rstrip("\0")
        names.setdefault(name_id, []).append((platform, language, value))
    return names


def name_value(names: dict[int, list[tuple[int, int, str]]], preferred: int, fallback: int) -> str:
    records = names.get(preferred) or names.get(fallback) or []
    records = sorted(
        records,
        key=lambda item: (0 if item[0] == 3 and item[1] == 1033 else 1 if item[0] == 3 else 2),
    )
    return records[0][2] if records else ""


def parse_font(data: bytes, path: str, face_offset: int = 0) -> dict[str, Any]:
    tables = parse_tables(data, face_offset)
    for required in ("name", "OS/2", "head", "maxp", "cmap", "hhea", "hmtx"):
        if required not in tables:
            raise ValueError(f"Font {path!r} is missing required table {required!r}.")
    names = names_for(data, tables["name"])
    os2_offset, os2_length = tables["OS/2"]
    if os2_length < 74:
        raise ValueError(f"Font {path!r} has a short OS/2 table.")
    head_offset, _ = tables["head"]
    maxp_offset, _ = tables["maxp"]
    hhea_offset, _ = tables["hhea"]
    return {
        "path": path,
        "faceOffset": face_offset,
        "sha256": sha256(data),
        "familyName": name_value(names, 16, 1),
        "subfamilyName": name_value(names, 17, 2),
        "fullName": name_value(names, 4, 4),
        "version": name_value(names, 5, 5),
        "postScriptName": name_value(names, 6, 6),
        "weight": u16(data, os2_offset + 4),
        "italic": bool(u16(data, os2_offset + 62) & 1),
        "unitsPerEm": u16(data, head_offset + 18),
        "locaFormat": i16(data, head_offset + 50),
        "numGlyphs": u16(data, maxp_offset + 4),
        "hheaMetricsCount": u16(data, hhea_offset + 34),
        "hheaAscender": i16(data, hhea_offset + 4),
        "hheaDescender": i16(data, hhea_offset + 6),
        "hheaLineGap": i16(data, hhea_offset + 8),
        "os2TypographicAscender": i16(data, os2_offset + 68),
        "os2TypographicDescender": i16(data, os2_offset + 70),
        "os2TypographicLineGap": i16(data, os2_offset + 72),
        "tableHashes": {
            tag: sha256(data[offset : offset + length])
            for tag, (offset, length) in sorted(tables.items())
        },
        "tables": tables,
        "names": names,
        "data": data,
    }


def cmap_from_font(font: dict[str, Any]) -> dict[int, int]:
    data: bytes = font["data"]
    table_offset, table_length = font["tables"]["cmap"]
    if table_length < 4:
        raise ValueError("The cmap table is too short.")
    table_count = u16(data, table_offset + 2)
    records: list[tuple[int, int, int, int]] = []
    for index in range(table_count):
        record = table_offset + 4 + index * 8
        platform = u16(data, record)
        encoding = u16(data, record + 2)
        subtable_offset = table_offset + u32(data, record + 4)
        if subtable_offset + 2 > table_offset + table_length:
            continue
        fmt = u16(data, subtable_offset)
        priority = (0 if fmt in (12, 13) else 1 if fmt in (4, 6, 10) else 9)
        if platform == 3 and encoding == 10:
            priority -= 4
        elif platform == 0:
            priority -= 3
        elif platform == 3 and encoding == 1:
            priority -= 2
        records.append((priority, fmt, subtable_offset, table_offset + table_length))
    for _, fmt, offset, table_end in sorted(records):
        if fmt == 12 or fmt == 13:
            if offset + 16 > table_end:
                continue
            length = u32(data, offset + 4)
            group_count = u32(data, offset + 12)
            if offset + length > table_end or offset + 16 + group_count * 12 > offset + length:
                continue
            mapping: dict[int, int] = {}
            for group_index in range(group_count):
                group = offset + 16 + group_index * 12
                first = u32(data, group)
                last = u32(data, group + 4)
                start_glyph = u32(data, group + 8)
                if last > 0x10FFFF or first > last:
                    raise ValueError("The cmap format 12/13 group has invalid codepoints.")
                for codepoint in range(first, last + 1):
                    glyph = start_glyph if fmt == 13 else start_glyph + codepoint - first
                    if glyph:
                        mapping[codepoint] = glyph
            return mapping
        if fmt == 4:
            if offset + 14 > table_end:
                continue
            length = u16(data, offset + 2)
            segment_count = u16(data, offset + 6) // 2
            if segment_count == 0 or offset + length > table_end:
                continue
            end_base = offset + 14
            start_base = end_base + segment_count * 2 + 2
            delta_base = start_base + segment_count * 2
            range_base = delta_base + segment_count * 2
            if range_base + segment_count * 2 > offset + length:
                continue
            mapping: dict[int, int] = {}
            for segment in range(segment_count):
                end_code = u16(data, end_base + segment * 2)
                start_code = u16(data, start_base + segment * 2)
                delta = i16(data, delta_base + segment * 2)
                range_offset = u16(data, range_base + segment * 2)
                if start_code > end_code:
                    continue
                for codepoint in range(start_code, end_code + 1):
                    if range_offset == 0:
                        glyph = (codepoint + delta) & 0xFFFF
                    else:
                        glyph_address = range_base + segment * 2 + range_offset + (codepoint - start_code) * 2
                        if glyph_address + 2 > offset + length:
                            continue
                        glyph = u16(data, glyph_address)
                        if glyph:
                            glyph = (glyph + delta) & 0xFFFF
                    if glyph and codepoint != 0xFFFF:
                        mapping[codepoint] = glyph
            return mapping
        if fmt == 6:
            if offset + 10 > table_end:
                continue
            first = u16(data, offset + 6)
            count = u16(data, offset + 8)
            if offset + 10 + count * 2 > table_end:
                continue
            return {
                first + index: u16(data, offset + 10 + index * 2)
                for index in range(count)
                if u16(data, offset + 10 + index * 2)
            }
        if fmt == 10:
            if offset + 20 > table_end:
                continue
            first = u32(data, offset + 12)
            count = u32(data, offset + 16)
            if offset + 20 + count * 2 > table_end:
                continue
            return {
                first + index: u16(data, offset + 20 + index * 2)
                for index in range(count)
                if u16(data, offset + 20 + index * 2)
            }
    raise ValueError("No supported Unicode cmap subtable was found.")


def outline_records(font: dict[str, Any]) -> list[bytes]:
    data: bytes = font["data"]
    glyph_count = font["numGlyphs"]
    loca_offset, loca_length = font["tables"]["loca"]
    glyf_offset, glyf_length = font["tables"]["glyf"]
    if font["locaFormat"] == 0:
        if loca_length < (glyph_count + 1) * 2:
            raise ValueError("The short loca table is truncated.")
        locations = [u16(data, loca_offset + index * 2) * 2 for index in range(glyph_count + 1)]
    elif font["locaFormat"] == 1:
        if loca_length < (glyph_count + 1) * 4:
            raise ValueError("The long loca table is truncated.")
        locations = [u32(data, loca_offset + index * 4) for index in range(glyph_count + 1)]
    else:
        raise ValueError(f"Unsupported indexToLocFormat {font['locaFormat']}.")
    if locations[-1] > glyf_length or any(a > b for a, b in zip(locations, locations[1:])):
        raise ValueError("The loca table contains invalid glyph offsets.")
    return [
        data[glyf_offset + locations[index] : glyf_offset + locations[index + 1]]
        for index in range(glyph_count)
    ]


def horizontal_metrics(font: dict[str, Any]) -> list[tuple[int, int]]:
    data: bytes = font["data"]
    offset, length = font["tables"]["hmtx"]
    count = font["hheaMetricsCount"]
    glyph_count = font["numGlyphs"]
    if count < 1 or count > glyph_count or length < count * 4 + (glyph_count - count) * 2:
        raise ValueError("The hmtx table does not match its hhea/maxp counts.")
    advances: list[int] = []
    bearings: list[int] = []
    for index in range(count):
        advances.append(u16(data, offset + index * 4))
        bearings.append(i16(data, offset + index * 4 + 2))
    last_advance = advances[-1]
    for index in range(count, glyph_count):
        advances.append(last_advance)
        bearings.append(i16(data, offset + count * 4 + (index - count) * 2))
    return list(zip(advances, bearings))


def canonical_digest(rows: list[bytes]) -> str:
    digest = hashlib.sha256()
    for index, row in enumerate(rows):
        digest.update(struct.pack(">II", index, len(row)))
        digest.update(row)
    return digest.hexdigest()


def mapping_digest(mapping: dict[int, int]) -> str:
    digest = hashlib.sha256()
    for codepoint, glyph in sorted(mapping.items()):
        digest.update(struct.pack(">II", codepoint, glyph))
    return digest.hexdigest()


def metric_digest(metrics: list[tuple[int, int]]) -> str:
    digest = hashlib.sha256()
    for glyph, (advance, bearing) in enumerate(metrics):
        digest.update(struct.pack(">IHi", glyph, advance, bearing))
    return digest.hexdigest()


def compare_one(
    ttc_data: bytes,
    ttc_faces: list[tuple[int, dict[str, Any]]],
    static_path: pathlib.Path,
    key: str,
    style: str,
    weight: int,
    italic: bool,
) -> dict[str, Any]:
    static_data = static_path.read_bytes()
    static = parse_font(static_data, str(static_path))
    matches = [
        (index, face)
        for index, face in ttc_faces
        if face["familyName"] == "Inter" and face["weight"] == weight and face["italic"] is italic
    ]
    style_matches = [(index, face) for index, face in matches if face["subfamilyName"] == style]
    if len(style_matches) == 1:
        ttc_index, ttc = style_matches[0]
    elif len(matches) == 1:
        ttc_index, ttc = matches[0]
    else:
        raise ValueError(
            f"Expected one TTC Inter face for {style}/{weight}/{italic}; found {len(matches)}."
        )

    static_cmap = cmap_from_font(static)
    ttc_cmap = cmap_from_font(ttc)
    static_outlines = outline_records(static)
    ttc_outlines = outline_records(ttc)
    static_metrics = horizontal_metrics(static)
    ttc_metrics = horizontal_metrics(ttc)
    glyph_count = min(len(static_outlines), len(ttc_outlines))
    differing_glyphs = [
        glyph for glyph in range(glyph_count) if static_outlines[glyph] != ttc_outlines[glyph]
    ]
    if len(static_outlines) != len(ttc_outlines):
        differing_glyphs.extend(range(glyph_count, max(len(static_outlines), len(ttc_outlines))))
    differing_metrics = [
        glyph
        for glyph in range(min(len(static_metrics), len(ttc_metrics)))
        if static_metrics[glyph] != ttc_metrics[glyph]
    ]
    if len(static_metrics) != len(ttc_metrics):
        differing_metrics.extend(range(min(len(static_metrics), len(ttc_metrics)), max(len(static_metrics), len(ttc_metrics))))
    codepoint_differences = sorted(set(static_cmap) ^ set(ttc_cmap))
    codepoint_differences.extend(
        cp for cp in set(static_cmap) & set(ttc_cmap) if static_cmap[cp] != ttc_cmap[cp]
    )
    codepoint_differences = sorted(set(codepoint_differences))
    tags = set(static["tables"]) & set(ttc["tables"])
    selected_tags = ("cmap", "glyf", "loca", "hmtx", "hhea", "maxp", "OS/2", "GDEF", "GPOS", "GSUB", "kern")
    compared_table_hashes = {
        tag: {
            "identical": static["tableHashes"].get(tag) == ttc["tableHashes"].get(tag),
            "staticSha256": static["tableHashes"].get(tag),
            "ttcSha256": ttc["tableHashes"].get(tag),
        }
        for tag in selected_tags
        if tag in tags
    }
    metrics_fields = (
        "unitsPerEm",
        "numGlyphs",
        "hheaAscender",
        "hheaDescender",
        "hheaLineGap",
        "os2TypographicAscender",
        "os2TypographicDescender",
        "os2TypographicLineGap",
    )
    static_metrics_summary = {name: static[name] for name in metrics_fields}
    ttc_metrics_summary = {name: ttc[name] for name in metrics_fields}
    return {
        "key": key,
        "expectedStyle": style,
        "expectedWeight": weight,
        "expectedItalic": italic,
        "ttcFaceIndex": ttc_index,
        "staticFont": {name: static[name] for name in ("path", "sha256", "familyName", "subfamilyName", "fullName", "version", "postScriptName", "weight", "italic")},
        "ttcFace": {name: ttc[name] for name in ("familyName", "subfamilyName", "fullName", "version", "postScriptName", "weight", "italic")},
        "identityMatches": all(
            static[name] == ttc[name]
            for name in ("familyName", "subfamilyName", "weight", "italic", "unitsPerEm", "numGlyphs")
        ),
        "sourceVersionMatchesTtc": static["version"] == ttc["version"],
        "metrics": {
            "static": static_metrics_summary,
            "ttc": ttc_metrics_summary,
            "summaryFieldsIdentical": static_metrics_summary == ttc_metrics_summary,
            "horizontalMetricCount": len(static_metrics),
            "horizontalMetricsIdentical": not differing_metrics,
            "staticHorizontalMetricsSha256": metric_digest(static_metrics),
            "ttcHorizontalMetricsSha256": metric_digest(ttc_metrics),
            "differingGlyphIds": differing_metrics[:32],
            "differingGlyphCount": len(differing_metrics),
        },
        "glyphCoverage": {
            "staticMappedCodepointCount": len(static_cmap),
            "ttcMappedCodepointCount": len(ttc_cmap),
            "identicalMappings": not codepoint_differences,
            "staticMappingSha256": mapping_digest(static_cmap),
            "ttcMappingSha256": mapping_digest(ttc_cmap),
            "differingCodepoints": [f"U+{cp:04X}" for cp in codepoint_differences[:32]],
            "differingCodepointCount": len(codepoint_differences),
        },
        "glyphOutlines": {
            "staticGlyphCount": len(static_outlines),
            "ttcGlyphCount": len(ttc_outlines),
            "identicalGlyphDataByGlyphId": not differing_glyphs,
            "staticOutlineSha256": canonical_digest(static_outlines),
            "ttcOutlineSha256": canonical_digest(ttc_outlines),
            "differingGlyphIds": differing_glyphs[:32],
            "differingGlyphCount": len(differing_glyphs),
        },
        "selectedTableHashes": compared_table_hashes,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ttc", required=True, type=pathlib.Path)
    parser.add_argument("--static-dir", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    ttc_path = args.ttc.resolve(strict=True)
    static_dir = args.static_dir.resolve(strict=True)
    output_path = args.output.resolve()
    ttc_data = ttc_path.read_bytes()
    offsets = parse_ttc(ttc_data)
    ttc_faces = [
        (index, parse_font(ttc_data, str(ttc_path), offset))
        for index, offset in enumerate(offsets)
    ]
    faces = [
        compare_one(
            ttc_data,
            ttc_faces,
            static_dir / filename,
            key,
            style,
            weight,
            italic,
        )
        for key, filename, style, weight, italic in EXPECTED_FACES
    ]
    result = {
        "schema": "classmngr-phase9-inter-static-ttc-parity-v1",
        "ttc": {
            "path": str(ttc_path),
            "sha256": sha256(ttc_data),
            "sizeBytes": len(ttc_data),
            "faceCount": len(offsets),
            "interFaceCount": sum(1 for _, face in ttc_faces if face["familyName"] == "Inter"),
        },
        "staticFaceDirectory": str(static_dir),
        "coverageMethod": "OpenType Unicode cmap mappings (format 12/13 preferred, format 4 fallback); variation-selector format 14 is not included.",
        "outlineMethod": "Per-glyph loca/glyf byte comparison by glyph ID; glyph padding is compared as stored.",
        "metricsMethod": "Per-glyph hmtx advance width and left side bearing, plus hhea/OS2 design metrics.",
        "allSourceVersionsMatchTtc": all(face["sourceVersionMatchesTtc"] for face in faces),
        "allFamilyStyleIdentitiesMatch": all(face["identityMatches"] for face in faces),
        "allGlyphCoverageMatches": all(face["glyphCoverage"]["identicalMappings"] for face in faces),
        "allGlyphOutlinesMatchExactly": all(face["glyphOutlines"]["identicalGlyphDataByGlyphId"] for face in faces),
        "allHorizontalMetricsMatchExactly": all(face["metrics"]["horizontalMetricsIdentical"] for face in faces),
        "faces": faces,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_path.exists():
        raise FileExistsError(f"Refusing to overwrite parity output: {output_path}")
    output_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "output": str(output_path),
        "ttcFaces": len(offsets),
        "sourceVersionsMatch": result["allSourceVersionsMatchTtc"],
        "glyphCoverageMatches": result["allGlyphCoverageMatches"],
        "glyphOutlinesMatch": result["allGlyphOutlinesMatchExactly"],
        "horizontalMetricsMatch": result["allHorizontalMetricsMatchExactly"],
        "styleIdentitiesMatch": result["allFamilyStyleIdentitiesMatch"],
    }, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"font parity comparison failed: {error}", file=sys.stderr)
        raise SystemExit(1)
