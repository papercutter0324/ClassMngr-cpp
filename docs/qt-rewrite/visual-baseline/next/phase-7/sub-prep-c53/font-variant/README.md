# C53 diagnostic cmap variant: exact painting, incomplete text

Task ID: `phase7_c53_subprep_output_implementation_01`.

**The cmap-only approach fails the complete requirement.** It fixes the numeric
hyphen, bullet, and space mappings while preserving every raster pixel, but
leaves 144 ordinary-hyphen uses unmapped. No production font/service/test or
existing reference was changed, and the generated font exists only in `/tmp`.

## Transformation and validation

`../writer-probe/make_font_variant.py` uses only Python's standard library. It
reads glyph substitutions and control aliases from the original Qt probe,
rebuilds Unicode cmap formats 4/12, renames the derivative, and recalculates
SFNT checksums. The original font is never overwritten. Only `cmap`, `name`,
and `head` change; outlines, metrics, variations, GSUB, and GPOS remain intact.

The repository font SHA-256 remains
`3090ccde0442bb347aa7685d9ba8b17436a60682df6e8f92a9a670de14056e22`.
The variant hash and table hashes are in `transformation-manifest.json`.
Resolved family: `ClassMngrPdfProbe Variable`; Qt 6.12.0/macOS/offscreen;
Regular, explicit `wght=400`. Qt accepts the font and `QRawFont::isValid()` is
true. `manifest.json` records complete raw cmap lookups and source-indexed
shaped glyph IDs/positions for mixed contexts, plus both PDF hashes.

Mappings are derived from the original font/probe, not retained PDF CIDs:

| Source | Original Qt lookup | Variant lookup |
| --- | ---: | ---: |
| U+002D | 13241 | 13252 |
| U+2022 | 13155 | 13207 |
| U+0009 / U+000A / U+000D | 2 | 1 |
| U+0020 | 2 | 2 |

The source font cmap contains no TAB/LF/CR entries; these aliases are observed
through the resolved Qt/CoreText lookup. Redirecting only TAB moves the earliest
space alias to LF. The final experiment redirects all three observed aliases
and verifies that emitted spaces map to U+0020.

## Result and precise blocker

The diagnostic emits 2,400 distinct `Student {1..96}-{1..25}` values, 96
source-derived class headers containing `Zoom: zoom.N • PW zoom-N`, and 16 mixed
context lines. These are standalone synthetic probe documents, **not** the
production C44 layout or SQL-backed Legacy/Next evidence.

All 16 original/variant pages are 595 × 842 pt. All-page 150-DPI comparison at
1240 × 1754 is **zero changed pixels on every page**, tolerance zero. The
mixed-context shaped glyph IDs and positions are exactly equal. Each comparison
holds only the current pair of page images.

The variant's independent expected-source checks find all 2,400 exact student
names and all 112 expected bullets (96 headers plus 16 context lines). Its
ToUnicode maps contain 3,296 U+0020 uses, 2,432 U+002D uses, and 112 U+2022 uses,
with no mapped control codes. However, 144 uses of glyph 13241 remain unmapped
(CID `0012` in this diagnostic). All 96 complete expected headers and all 16
complete mixed-context lines therefore fail exact extraction. The strict
used-code auditor exits **1**, as expected.

Shaping selects **both** glyph 13252 and glyph 13241 for the same source U+002D,
depending on context. Remapping U+002D to 13252 makes Qt's reverse lookup find
that glyph, but removes the Unicode lookup for 13241. The retained GSUB still
selects 13241 for ordinary hyphens (`zoom-1`, `wifi-2`, and ` - normal`), so the
missing mapping moves between contexts. A single cmap Unicode key cannot
reverse-map both glyphs to the same character. Adding a PUA alias would extract
a different Unicode character; changing GSUB would change established painting.

## Reproduce without retaining a modified font

From the repository root, with the standalone CMake project already configured
as described in `../README.md`:

```sh
probe=docs/qt-rewrite/visual-baseline/next/phase-7/sub-prep-c53/writer-probe
font="$PWD/resources/assets/fonts/PretendardVariable.ttf"
cmake --build /private/tmp/c53-probe-build
QT_QPA_PLATFORM=offscreen /private/tmp/c53-probe-build/ClassMngrC53FontVariantProbe \
  "$font" /private/tmp/c53-font-baseline
python3 "$probe/make_font_variant.py" "$font" \
  /private/tmp/c53-font-baseline/manifest.json /private/tmp/c53-font-variant.ttf \
  --report /private/tmp/c53-font-transformation.json
QT_QPA_PLATFORM=offscreen /private/tmp/c53-probe-build/ClassMngrC53FontVariantProbe \
  "$font" /private/tmp/c53-font-pair /private/tmp/c53-font-variant.ttf
python3 "$probe/check_used_codes.py" --require-valid \
  --output /private/tmp/c53-font-code-audit.json /private/tmp/c53-font-pair/variant.pdf
```

Build and probe execution passed. Manifest assertions confirmed all raster and
source-text outcomes above. The final audit command was run and returned 1.
`git diff --check` passed. No Windows, Bold, or SQL-backed acceptance is claimed.

## License, asset implications, and recommendation

The repository's `licenses/fonts/pretendard/LICENSE.txt` specifies OFL 1.1 and
the Reserved Font Name Pretendard. The [upstream license](https://github.com/orioncactus/pretendard/blob/main/LICENSE)
and [official OFL text](https://openfontlicense.org/open-font-license-official-text/)
permit modification, require preservation of copyright/license notices for
font redistribution, and restrict reserved names for modified versions.
Accordingly, primary family/PostScript/instance names are renamed; copyright
and license records remain intact. No derivative TTF is checked in. Shipping a
derivative would need deterministic generation, original/derived hashes,
notices, a distinct internal family, resource-catalog/font-selection updates,
and platform validation. This failed experiment does not justify that asset.

Recommend a **maintained, source-aware PDF finalization step** over maintaining
a private Qt fork. The correction must retain the original text-to-glyph
associations, handle multiple shaped glyphs for one Unicode character, and
modify mappings without changing painting. The exact library/API and mapping
capture design still need a main decision and validation; no dependency has
been selected. An upstream-supported Qt build with a verified source-aware
mapping fix would also be preferable to a private fork if available, but none
was established here. The font experiment does not remove the matched SQL and
hosted Windows gates recorded in the parent README.
