# Phase 7 C44 Sub Prep evidence review

Task ID: `phase7_c44_evidence_review_01`

Source commit: `8c4fece5`.

## UI and scale

The offscreen captures are 800×600 and cover English/Korean × Light/Dark in
selected and empty states. They document reviewed structure and visible
behavior, not pixel parity. The selected large-fixture route uses one summary
model, one detail panel, six editors, and 96 projected summaries (24 in the
active grade). Mode/day refresh, selection, and release on leave/re-entry were
verified. The [manifest](manifest.json) records the capture and route metrics.

| Locale / theme | Selected | Empty |
| --- | --- | --- |
| English / Light | [capture](selected-english-light.png) | [capture](empty-english-light.png) |
| English / Dark | [capture](selected-english-dark.png) | [capture](empty-english-dark.png) |
| Korean / Light | [capture](selected-korean-light.png) | [capture](empty-korean-light.png) |
| Korean / Dark | [capture](selected-korean-dark.png) | [capture](empty-korean-dark.png) |

The unchanged `large_startup.sql` fixture's selected 96-class output is
rejected for a Monday 4:00 PM conflict and creates no package; see the
[conflict capture](output-conflict-en-light.png). Next Cancel passed all
three Qt cases; it creates no destination and preserves selection, detail,
and read counts. The compatible
[`representative_startup.sql`](../../../../../../tests/fixtures/workspaces/representative_startup.sql)
route produced nonempty [`Sub Prep.pdf`](generated-package-success/C44%20Next%20(26%20Oct%202026)/Sub%20Prep.pdf)
and [`Rosters - By Day.pdf`](generated-package-success/C44%20Next%20(26%20Oct%202026)/Rosters%20-%20By%20Day.pdf).
Four locale/theme capture variants and both combo-arrow resources were verified.

## C44 macOS Debug verification

Focused CTests passed 13/13, plus roster parity 1/1. QtTest counts: Package
Service 14, print/PDF 24, list model 9, roster parity 6, Next bootstrap 72,
Gateway 4, resource pack 12, and FontManager 15. Schedule Import workflow
passed 16, normal integration 14, state port 8, apply port 9; its parser
passed 99 with 1 skipped because `CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset.
Resource inventory `--check` and `git diff --check` passed. Gateway checks
covered two-workspace active-store behavior and unopened, closed, and invalid
scope/repository errors; the store remains private.

Strict page-by-page PDF comparisons against unchanged references pass: Sub
Prep, 19 pages; Rosters Daily, 16 pages. Extracted text and rendered page
dimensions match. Explicit `wght` values 400/700 embed Pretendard Regular and
Bold separately. At 150 DPI (1240×1754), macOS Qt 6.11 raster comparisons
still show 0.76–10.35% changed pixels for Sub Prep and 11.05–12.21% for the
roster. The reference is weeks old and its Windows provenance is unverified;
the Windows 150-DPI pixel gate remains unverified.

The separator has a missing ToUnicode CID in both baseline and candidate;
local extractors return U+001F. Matching extracted text does not establish
copy/search correctness. The Legacy
`SubPrepPageTests::unavailablePreferenceLoadsPreservePageValues` offscreen
hang is not a pass (38.4 seconds isolated; the full run exceeded two minutes);
targeted Legacy Cancel passed. The Phase 9 packaged Release memory threshold
was not run. Manifest memory values are process-wide samples, not per-page
allocations. These results record C44 evidence and do not assert full Phase 7
completion.
