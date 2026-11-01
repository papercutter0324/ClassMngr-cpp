#!/usr/bin/env python3
"""Audit used text codes on every page/font in the supported Qt PDF format.

This is a strict C53 evidence checker, not a general PDF parser or mutator.
Unknown xref, stream, resource, encoding, or text-show forms fail closed.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import zlib


def unique(pattern, data, message):
    matches = list(re.finditer(pattern, data, re.S))
    if len(matches) != 1:
        raise ValueError(message)
    return matches[0]


def inspect(path):
    data = path.read_bytes()
    if not data.startswith(b"%PDF-1.4\n") or len(data) >= 64 * 1024 * 1024:
        raise ValueError("Unsupported PDF version or size")
    startxref = unique(rb"startxref\s+(\d+)\s+%%EOF\s*$", data[-100:], "Invalid PDF trailer")
    xref_offset = int(startxref[1])
    if not 0 < xref_offset < len(data):
        raise ValueError("Invalid xref offset")
    tail = data[xref_offset:]
    if not tail.startswith(b"xref\n0 "):
        raise ValueError("Only the supported classic xref table is accepted")
    trailer_start = tail.find(b"trailer\n")
    if trailer_start < 0:
        raise ValueError("Trailer is missing")
    lines = tail[:trailer_start].split(b"\n")
    if len(lines) < 4 or lines[0] != b"xref":
        raise ValueError("Malformed xref table")
    first, count = map(int, lines[1].split())
    if first != 0 or count < 2 or len(lines) != count + 3:
        raise ValueError("Unsupported xref range")
    if lines[2] != b"0000000000 65535 f ":
        raise ValueError("Malformed free xref entry")
    offsets = {}
    if lines[-1] != b"":
        raise ValueError("Malformed xref table terminator")
    for object_id, line in enumerate(lines[3:-1], 1):
        match = re.fullmatch(rb"(\d{10}) 00000 n ?", line)
        if not match:
            raise ValueError("Free entries or nonzero generations are unsupported")
        offset = int(match[1])
        if not 0 < offset < xref_offset:
            raise ValueError("Object offset is outside the object area")
        offsets[object_id] = offset

    trailer_match = unique(rb"^\s*(<<[\s\S]*?>>)\s*startxref\s+\d+\s+%%EOF\s*$",
                           tail[trailer_start + 8:], "Malformed trailer dictionary")
    trailer = trailer_match[1]
    if any(token in trailer for token in (b"/Encrypt", b"/XRefStm")):
        raise ValueError("Encrypted PDFs or xref streams are unsupported")
    previous_matches = list(re.finditer(rb"/Prev\s+(\d+)\b", trailer))
    if len(previous_matches) > 1:
        raise ValueError("Ambiguous previous xref reference")
    xref_boundaries = [xref_offset]
    if previous_matches:
        previous_offset = int(previous_matches[0][1])
        if not 0 < previous_offset < xref_offset:
            raise ValueError("Invalid previous xref offset")
        previous_tail = data[previous_offset:]
        if not previous_tail.startswith(b"xref\n0 ") or b"/Prev" in previous_tail[:2048]:
            raise ValueError("Unsupported xref revision chain")
        xref_boundaries.append(previous_offset)

    ordered = sorted((offset, object_id) for object_id, offset in offsets.items() if object_id)
    if len({offset for offset, _ in ordered}) != len(ordered):
        raise ValueError("Duplicate object offsets")
    objects = {}
    for index, (offset, object_id) in enumerate(ordered):
        following = [candidate for candidate in xref_boundaries if candidate > offset]
        if index + 1 < len(ordered):
            following.append(ordered[index + 1][0])
        if not following:
            raise ValueError("Object has no xref boundary")
        end = min(following)
        raw = data[offset:end].strip()
        header = re.match(rb"%d 0 obj\n" % object_id, raw)
        if not header or not raw.endswith(b"endobj"):
            raise ValueError("Malformed object boundary")
        objects[object_id] = raw[header.end():-6].strip()

    def body(object_id):
        if object_id not in objects:
            raise ValueError("Dangling object reference")
        return objects[object_id]

    def ref(dictionary, name):
        match = unique(rb"/%s\s+(\d+)\s+0\s+R\b" % re.escape(name), dictionary,
                       "Missing or ambiguous /%s reference" % name.decode())
        return int(match[1])

    def stream(object_id):
        raw = body(object_id)
        match = unique(rb"\nstream\n", raw, "Expected one stream object")
        dictionary = raw[:match.start()]
        length = unique(rb"/Length\s+(\d+)(?:\s+(\d+)\s+R)?\b", dictionary,
                        "Missing or ambiguous stream length")
        if length[2]:
            if length[2] != b"0":
                raise ValueError("Unsupported stream length generation")
            size = int(body(int(length[1])).strip())
        else:
            size = int(length[1])
        payload = raw[match.end():match.end() + size]
        if len(payload) != size or raw[match.end() + size:].strip() != b"endstream":
            raise ValueError("Malformed stream boundary")
        if b"/DecodeParms" in dictionary or b"/DP" in dictionary:
            raise ValueError("Stream decode parameters are unsupported")
        filters = re.findall(rb"/Filter\s+(/\w+|\[[^]]+\])", dictionary)
        if filters:
            if filters != [b"/FlateDecode"]:
                raise ValueError("Unsupported stream filter")
            payload = zlib.decompress(payload)
        return payload

    root_id = ref(trailer, b"Root")
    catalog = body(root_id)
    if b"/Type /Catalog" not in catalog:
        raise ValueError("Unsupported catalog")
    pages_tree = body(ref(catalog, b"Pages"))
    if b"/Type /Pages" not in pages_tree:
        raise ValueError("Unsupported page tree")
    kids = unique(rb"/Kids\s*\[([^]]*)\]", pages_tree, "Missing or ambiguous page list")
    page_refs = list(re.finditer(rb"(\d+)\s+0\s+R\b", kids[1]))
    remainder = re.sub(rb"(\d+)\s+0\s+R\b", b"", kids[1]).strip()
    if remainder or not page_refs:
        raise ValueError("Unsupported page tree references")
    page_ids = [int(match[1]) for match in page_refs]
    page_count = int(unique(rb"/Count\s+(\d+)\b", pages_tree, "Missing page count")[1])
    if page_count != len(page_ids) or len(set(page_ids)) != len(page_ids):
        raise ValueError("Page tree count mismatch")

    def font_cmap(font_id):
        font = body(font_id)
        if b"/Subtype /Type0" not in font or b"/Encoding /Identity-H" not in font:
            raise ValueError("Unsupported text font encoding")
        cmap_id = ref(font, b"ToUnicode")
        cmap_data = stream(cmap_id)
        if b"/CMapType 2 def" not in cmap_data or b"begincmap" not in cmap_data:
            raise ValueError("Unsupported ToUnicode CMap")
        mapping = {}

        def assign(code, value):
            if code in mapping:
                raise ValueError("Duplicate or ambiguous ToUnicode source code")
            mapping[code] = value

        for block in re.findall(rb"beginbfchar(.*?)endbfchar", cmap_data, re.S):
            for source, destination in re.findall(rb"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>", block):
                if len(source) != 4 or len(destination) % 4:
                    raise ValueError("Unsupported ToUnicode code width")
                assign(int(source, 16), bytes.fromhex(destination.decode()).decode("utf-16-be", errors="strict"))
        for block in re.findall(rb"beginbfrange(.*?)endbfrange", cmap_data, re.S):
            tokens = re.findall(rb"<[0-9A-Fa-f]+>|\[|\]", block)
            cursor = 0
            while cursor < len(tokens):
                if cursor + 2 >= len(tokens):
                    raise ValueError("Malformed bfrange")
                source_start = int(tokens[cursor][1:-1], 16)
                source_end = int(tokens[cursor + 1][1:-1], 16)
                cursor += 2
                if source_end < source_start or source_end - source_start > 65535:
                    raise ValueError("Invalid bfrange interval")
                if tokens[cursor] == b"[":
                    cursor += 1
                    values = tokens[cursor:cursor + source_end - source_start + 1]
                    cursor += len(values)
                    if len(values) != source_end - source_start + 1 or cursor >= len(tokens) or tokens[cursor] != b"]":
                        raise ValueError("Malformed bfrange array")
                    cursor += 1
                else:
                    first = tokens[cursor][1:-1]
                    cursor += 1
                    values = [b"<" + format(int(first, 16) + index, "0%dx" % len(first)).encode() + b">"
                              for index in range(source_end - source_start + 1)]
                for code, value in zip(range(source_start, source_end + 1), values):
                    if len(value[1:-1]) % 4:
                        raise ValueError("Unsupported bfrange Unicode width")
                    assign(code, bytes.fromhex(value[1:-1].decode()).decode("utf-16-be", errors="strict"))
        if not mapping:
            raise ValueError("Empty ToUnicode CMap")
        return mapping

    page_reports = []
    all_valid = True
    for page_number, page_id in enumerate(page_ids, 1):
        page = body(page_id)
        if b"/Type /Page\n" not in page:
            raise ValueError("Nested or unsupported page object")
        resources = body(ref(page, b"Resources"))
        font_dict = unique(rb"/Font\s*<<([^>]*)>>", resources, "Missing or ambiguous font resources")[1]
        font_entries = re.findall(rb"/(F\d+)\s+(\d+)\s+0\s+R\b", font_dict)
        font_map = {name: int(object_id) for name, object_id in font_entries}
        if len(font_map) != len(font_entries):
            raise ValueError("Ambiguous page font resource")
        font_mappings = {object_id: font_cmap(object_id) for object_id in font_map.values()}
        content = stream(ref(page, b"Contents"))
        blocks = list(re.finditer(rb"\bBT\b(.*?)\bET\b", content, re.S))
        outside = re.sub(rb"\bBT\b.*?\bET\b", b"", content, flags=re.S)
        if re.search(rb"\b(?:BT|ET|Tf|Tj|TJ)\b", outside):
            raise ValueError("Unbalanced or unsupported text operators")
        used = {}
        for block_match in blocks:
            block = block_match[1]
            if re.search(rb"\bTJ\b|[)\]]\s*(?:Tj|'|\")", block):
                raise ValueError("Unsupported text-show operator")
            active = None
            tokens = list(re.finditer(rb"/(F\d+)\s+[\d.]+\s+Tf|<([0-9A-Fa-f]+)>\s*Tj\b", block))
            remainder = re.sub(rb"/(F\d+)\s+[\d.]+\s+Tf|<([0-9A-Fa-f]+)>\s*Tj\b", b"", block)
            if re.search(rb"\bTj\b", remainder):
                raise ValueError("Unrecognized text-show syntax")
            for token in tokens:
                if token[1]:
                    if token[1] not in font_map:
                        raise ValueError("Text uses an unknown page font")
                    active = font_map[token[1]]
                else:
                    if active is None or len(token[2]) == 0 or len(token[2]) % 4:
                        raise ValueError("Unsupported text code width or state")
                    counts = used.setdefault(active, Counter())
                    counts.update(int(token[2][offset:offset + 4], 16)
                                  for offset in range(0, len(token[2]), 4))
        if not used:
            raise ValueError("No audited text codes on page %d" % page_number)
        for font_id, counts in sorted(used.items()):
            mapping = font_mappings[font_id]
            unmapped = []
            controls = []
            for code, uses in sorted(counts.items()):
                if code not in mapping:
                    unmapped.append({"codeHex": "%04X" % code, "uses": uses})
                elif any(ord(character) < 32 or 0x7F <= ord(character) <= 0x9F for character in mapping[code]):
                    controls.append({"codeHex": "%04X" % code, "uses": uses,
                                     "unicode": ["U+%04X" % ord(character) for character in mapping[code]]})
            punctuation_uses = {
                "U+0020": sum(uses for code, uses in counts.items() if mapping.get(code) == " "),
                "U+002D": sum(uses for code, uses in counts.items() if mapping.get(code) == "-"),
                "U+2022": sum(uses for code, uses in counts.items() if mapping.get(code) == "•"),
            }
            all_valid = all_valid and not unmapped and not controls
            page_reports.append({"page": page_number, "fontObject": font_id,
                                 "baseFont": re.search(rb"/BaseFont\s+/([^\s/]+)", body(font_id))[1].decode(),
                                 "usedCodeCount": len(counts), "textCodeUses": sum(counts.values()),
                                 "unmappedUsedCodes": unmapped, "mappedControlCodes": controls,
                                 "punctuationAndSpaceUses": punctuation_uses})

    return {"path": path.as_posix(), "sha256": hashlib.sha256(data).hexdigest(),
            "pageCount": page_count, "pageFonts": page_reports,
            "allUsedCodesMappedToNonControlUnicode": all_valid}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-valid", action="store_true")
    parser.add_argument("pdfs", nargs="+", type=Path)
    args = parser.parse_args()
    reports = [inspect(path) for path in args.pdfs]
    args.output.write_text(json.dumps({"taskId": "phase7_c53_subprep_finalizer_implementation_02",
                                      "documents": reports}, indent=2) + "\n")
    if args.require_valid and not all(report["allUsedCodesMappedToNonControlUnicode"] for report in reports):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
