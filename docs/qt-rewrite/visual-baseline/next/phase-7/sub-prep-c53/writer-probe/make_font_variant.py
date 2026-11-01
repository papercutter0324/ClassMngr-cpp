#!/usr/bin/env python3
"""Diagnostic-only SFNT cmap transformation; preserve the repository font.

Uses Python standard library only. Derive punctuation glyph IDs from a probe
manifest, rebuild Unicode cmap formats 4/12, redirect observed space aliases,
and rename the derivative's family/PostScript names to respect OFL RFNs.
The caller must keep the original copyright/OFL notices with any font copy.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def checksum(data):
    data += bytes((-len(data)) % 4)
    return sum(struct.unpack(f'>{len(data) // 4}I', data)) & 0xFFFFFFFF


def groups(mapping):
    result = []
    for cp, gid in sorted(mapping.items()):
        if not gid:
            continue
        if result and cp == result[-1][1] + 1 and gid == result[-1][2] + cp - result[-1][0]:
            result[-1][1] = cp
        else:
            result.append([cp, cp, gid])
    return result


def read_cmap12(data):
    count = struct.unpack_from('>H', data, 2)[0]
    subtables = [struct.unpack_from('>HHI', data, 4 + n * 8) for n in range(count)]
    offset = next(offset for platform, encoding, offset in subtables
                  if platform in (0, 3) and struct.unpack_from('>H', data, offset)[0] == 12)
    count = struct.unpack_from('>I', data, offset + 12)[0]
    mapping = {}
    for n in range(count):
        first, last, glyph = struct.unpack_from('>III', data, offset + 16 + n * 12)
        mapping.update((cp, glyph + cp - first) for cp in range(first, last + 1))
    return mapping


def new_cmap(mapping):
    full = groups(mapping)
    fmt12 = struct.pack('>HHIII', 12, 0, 16 + 12 * len(full), 0, len(full))
    fmt12 += b''.join(struct.pack('>III', *group) for group in full)
    bmp = groups({cp: gid for cp, gid in mapping.items() if cp < 0xFFFF})
    bmp.append([0xFFFF, 0xFFFF, 0])
    count = len(bmp)
    power = 1 << (count.bit_length() - 1)
    fmt4 = struct.pack('>7H', 4, 16 + 8 * count, 0, 2 * count,
                       2 * power, power.bit_length() - 1, 2 * count - 2 * power)
    fmt4 += b''.join(struct.pack('>H', last) for first, last, glyph in bmp) + bytes(2)
    fmt4 += b''.join(struct.pack('>H', first) for first, last, glyph in bmp)
    fmt4 += b''.join(struct.pack('>H', (glyph - first) & 65535) for first, last, glyph in bmp)
    fmt4 += bytes(2 * count)
    offset4 = 4 + 4 * 8
    offset12 = offset4 + len(fmt4)
    records = [(0, 3, offset4), (0, 4, offset12), (3, 1, offset4), (3, 10, offset12)]
    return struct.pack('>HH', 0, len(records)) + b''.join(struct.pack('>HHI', *r) for r in records) + fmt4 + fmt12


def rename(data):
    version, count, storage = struct.unpack_from('>3H', data)
    if version != 0:
        raise ValueError('Only name table version 0 is supported')
    records, strings = [], bytearray()
    for n in range(count):
        platform, encoding, language, name_id, size, offset = struct.unpack_from('>6H', data, 6 + n * 12)
        value = data[storage + offset:storage + offset + size]
        if platform == 1 and encoding == 3 and name_id >= 256 and b'Pretendard' not in value:
            records.append((platform, encoding, language, name_id, len(value), len(strings)))
            strings.extend(value)
            continue
        if platform not in (0, 3) and (platform, encoding) != (1, 0):
            raise ValueError('Unexpected name encoding')
        codec = 'mac_roman' if platform == 1 else 'utf-16-be'
        text = value.decode(codec)
        # Copyright/license attribution retains the original author/font names.
        if name_id not in (0, 13, 14):
            text = text.replace('Pretendard', 'ClassMngrPdfProbe')
        value = text.encode(codec)
        records.append((platform, encoding, language, name_id, len(value), len(strings)))
        strings.extend(value)
    return struct.pack('>3H', 0, count, 6 + 12 * count) + b''.join(struct.pack('>6H', *r) for r in records) + strings


def transform(original, probe):
    count = struct.unpack_from('>H', original, 4)[0]
    tables = {}
    for n in range(count):
        tag, stored_checksum, offset, size = struct.unpack_from('>4sIII', original, 12 + n * 16)
        tables[tag] = original[offset:offset + size]
    source_mapping = read_cmap12(tables[b'cmap'])
    mapping = dict(source_mapping)
    requested = {record['unicode']: record['shapedGlyph'] for record in probe['derivedSubstitutions']}
    if set(requested) != {0x002D, 0x2022}:
        raise ValueError('Probe must derive exactly hyphen and bullet substitutions')
    mapping.update(requested)
    # These aliases can be synthesized by CoreText rather than present in the
    # cmap. Redirect every *observed* earlier control alias of the space glyph.
    raw_cmap = {record['unicode']: record['glyph'] for record in probe['rawCmap']}
    aliases = [cp for cp, glyph in raw_cmap.items() if cp < 32 and glyph == raw_cmap[32]]
    if 9 not in aliases:
        raise ValueError('Original probe must demonstrate the TAB/space alias')
    mapping.update((cp, 1) for cp in aliases)
    tables[b'cmap'] = new_cmap(mapping)
    tables[b'name'] = rename(tables[b'name'])
    tables[b'head'] = tables[b'head'][:8] + bytes(4) + tables[b'head'][12:]
    output = bytearray(original[:12] + bytes(count * 16))
    report = []
    head_offset = None
    for n, (tag, table) in enumerate(tables.items()):
        output.extend(bytes((-len(output)) % 4))
        offset = len(output)
        output.extend(table)
        struct.pack_into('>4sIII', output, 12 + n * 16, tag, checksum(table), offset, len(table))
        if tag == b'head':
            head_offset = offset
        report.append({'tag': tag.decode(), 'sha256': hashlib.sha256(table).hexdigest()})
    output.extend(bytes((-len(output)) % 4))
    struct.pack_into('>I', output, head_offset + 8, (0xB1B0AFBA - checksum(output)) & 0xFFFFFFFF)
    assert checksum(output) == 0xB1B0AFBA
    return output, {'sourceCmapTabGlyph': source_mapping.get(9), 'requestedCmap': {f'U+{cp:04X}': glyph for cp, glyph in {**requested, **{cp: 1 for cp in aliases}}.items()},
                    'sfntChecksumValid': True, 'tablesBeforeHeadAdjustment': report,
                    'changedTables': ['cmap', 'name', 'head'], 'derivativeFamily': 'ClassMngrPdfProbe Variable'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('probe_manifest', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        raise ValueError('Refusing to overwrite source font')
    original = args.source.read_bytes()
    probe = json.loads(args.probe_manifest.read_text())
    if hashlib.sha256(original).hexdigest() != probe['fontSha256']:
        raise ValueError('Source font does not match the glyph derivation manifest')
    output, report = transform(original, probe)
    args.output.write_bytes(output)
    report.update({'sourceSha256': hashlib.sha256(original).hexdigest(),
                   'variantSha256': hashlib.sha256(output).hexdigest(), 'variantBytes': len(output)})
    args.report.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
