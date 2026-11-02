# W751 Windows output-sub-prep evidence

Task ID: W751-WINDOWS-RUN

## Source and route

- Source: detached x64 Windows checkout at `31a71e90f304400fd99b70698f986d64eb7e8a01` (40-character object ID). The runner manifest records clean initial and final Git snapshots and no source change during the run.
- Route: `output-sub-prep`; PDF capture enabled. Release build, Debug test-harness build, route, and validator all ran; `skipBuild`, `skipRun`, and `skipValidation` are false. Parallelism is 2. Per-command timeout was extended to 3600 seconds after the 900-second default timed out during Release compilation.
- Fixture SHA-256: `1c5759f9df9f20849939aeb8a3f1932c95f256485cb5eb2bfd0f384a93dab8b4` (`tests/fixtures/workspaces/large_startup.sql`). Generated test-database snapshot SHA-256: `4b190baa546508ec076158ac6f668c11a1fdff6884c77f909068247887431667`.
- Inter.ttc SHA-256: `ea4e3d9970b9b566110a1b0e13635fe235280b83dd2443dcb3faca1e2456ef`.
- PretendardVariable.ttf SHA-256: `3090ccde0442bb347aa7685d9ba8b17436a60682df6e8f92a9a670de14056e22`.
- Reference date: `2026-09-14`.

## Platform and build tools

- OS registry: Windows 10 Home, DisplayVersion 26H2, OS version `10.0.26300.9711`, x64.
- Visual Studio x64 developer environment selected Windows SDK `10.0.26100.0`. It resolved `rc.exe` and `mt.exe` from `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64`.
- CMake `4.4.2`; Ninja `1.13.2`; MSVC `19.51.36260.0` x64 (tool directory `14.51.36231`); Qt `6.12.0`; Python `3.14.7`; Git `2.55.0.windows.3`.

## Route and validator result

The Release application and Debug `ClassMngrStartupPerformanceTests` target built successfully. The focused route ran normally (not timed out) and generated both PDFs (17 pages total), PDF captures, workflow trace and metrics, generated database snapshot, route manifest, and per-page parity output. The unchanged comparator reports:

- `Sub Prep.pdf`: 12 generated and 12 historical pages, all `595 × 842` points. Normalized text matches on pages 1–10 and differs on pages 11–12.
- `Rosters - By Day.pdf`: 5 generated and 5 historical pages, all `842 × 595` points. Normalized text matches on all five pages.
- All 30 weekday markers matched their expected day/class/teacher/student on one page; Saturday markers were absent; PDF snapshot errors: 0.

The Sub Prep assignment differs at the last two pages: generated page 11 ends with Teacher 22, while generated page 12 contains Teachers 23 and 24 plus Sub Notes. The historical semantic reference places Teachers 22–24 on page 11 and only Sub Notes on page 12. The comparator marks `semanticParity: mismatch`; the test fails at its semantic-parity assertion before writing the route's expected `output/sub-prep/manifest.json`. The runner therefore records route exit code 1, and validation fails with `missing-artifact`, `abnormal-process`, and `route-failed`. This is a parity result, not a compiler or SDK failure.

The historical PDF path is `docs/qt-rewrite/visual-baseline/release/large-sub-prep-output-boundary/output-target/Teacher 01 (14 - 18 Sep 2026)`. Its manifest marks it `semanticOracleOnly: true` and `sourceBound: false`. The comparator normalizes with Qt `QString::simplified()`, so its retained per-page text collapses whitespace and does not preserve line-level wrapping/break locations. Captures include first-page images and PDF-viewer states, not page 11/12 line layout; raw generated PDFs are retained for later visual inspection.

## Mac comparison and retained artifacts

No checked-in Mac package matches this source revision. The checked-in Mac route matrix records commit `c5dd6e6b96f630d776165b9dbf61917ad743f6dd` and does not include raw route PDFs. Same-source Windows/Mac geometry and wrapping comparison remains unclosed; transfer recommendation belongs to Phase 10.

Evidence parent: `C:\Users\wfelt\AppData\Local\Temp\w751f-20261101T171718Z\e`

Run root: `C:\Users\wfelt\AppData\Local\Temp\w751f-20261101T171718Z\e\w751-31a71e90-windows-output-sub-prep`

The run root retains `run-manifest.json`, `validation-summary.json`, `output/sub-prep/route-manifest.json`, `sub-prep-output-parity.json` (full per-page normalized text and dimensions), the generated `.tps` snapshot, both raw PDFs, all route captures, workflow trace/metrics, per-step configure/build/install/route/validation logs, `runner-console.log`, and the environment launcher. The route-produced `output/sub-prep/manifest.json` is absent because the semantic assertion failed before it was written; this is recorded by the validator.

Earlier runs remain separate and unchanged: the original configure failure is under `C:\Users\wfelt\AppData\Local\Temp\ClassMngr-W751-Windows-Run-31a71e90-20261101T165451Z\evidence-parent`; the SDK-corrected 900-second build timeout is under `C:\Users\wfelt\AppData\Local\Temp\w751r-20261101T165844Z\e`.
## Raw PDF page review

The raw Windows `Sub Prep.pdf` pages 11-12 were rendered with the installed Qt 6.12 PDF module for visual inspection. Supplementary images and extracted page text are retained at `generated-subprep-page-11.png`, `generated-subprep-page-12.png`, `generated-subprep-page-11-raw-text.txt`, and `generated-subprep-page-12-raw-text.txt` in this run root. The generated page 11 shows Teacher 22 only, with the remainder of the page blank; page 12 shows Teachers 23 and 24 followed by Sub Notes. Visible text blocks on these two pages fit on single lines; the decisive observed difference is the assignment of Teachers 23-24 across the page break.

The historical semantic-oracle PDF was rendered to `inspection-historical/` for inspection only: its page 11 contains Teachers 22-24, and page 12 contains Sub Notes. This is not a same-revision Mac artifact and is not used to claim Windows/Mac geometry or wrapping parity. The route comparator retains exact per-page point dimensions and normalized text in `output/sub-prep/sub-prep-output-parity.json`; its normalized text does not retain line boxes or visual break positions. Same-source Windows/Mac geometry and wrapping comparison remains unclosed for Phase 10.
