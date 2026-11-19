# Current Slice Batches

## Phase 9 — Batch 3: Additional packaged Release lifecycle evidence (Active)

Batch goal: measure the remaining large or repeated Next workflows in current-source packaged Windows x64 Release processes. Use authentic UIA-bound actions, complete lifecycle snapshots, identified fixtures, and local validation. Keep transient budgets advisory until a return-to-baseline allowance is supported by current-source measurements. Commit each slice; push once after Batch 3 closes. GitHub validation is not acceptance.

10. **P9-10 — Large Calendar Import and cleanup** (Active)
    - Acceptance: run large Calendar import success and parser-error cleanup in
      separate current-source packaged Next Release processes. Serve the
      retained fixture data from a local loopback source via
      CLASSMNGR_STARTUP_CALENDAR_IMPORT_URL. Record startup/settled Calendar
      baseline, downloaded/staged bytes, parse complete/error, typed apply or
      unchanged database, imported/skipped rows, cache refresh, staged-file
      removal, dialog/service state, process metrics, and bounded Calendar
      view/model/cache counts.
    - Success fixture: docs/qt-rewrite/visual-baseline/release/
      large-calendar-import-boundary/generated-large-calendar-import.xlsx
      (16,284 bytes; documented expectation 261 imported events and 104
      skipped weekend entries). Error fixture:
      docs/qt-rewrite/visual-baseline/release/
      large-calendar-import-error-boundary/
      malformed-calendar-import-response.bin (69 bytes; missing
      xl/workbook.xml). Verify counts against the actual Next package route and
      fixture bytes. Use separate copies of the same seeded workspace.
    - One Calendar suspension/re-entry after each result checks cleanup; P9-07
      already covers repeated normal Calendar navigation. Phase 0 lifecycle
      runs and offscreen Next tests are supporting evidence only, not packaged
      Windows acceptance. Capture actual current-process UIA and memory data.
11. **P9-11 — Class Transfer export/import lifecycle** (Queued)
    Cover export, import, cancel, and released state. Existing route:
    lifecycle-class-transfer.
12. **P9-12 — QtPdf viewer lifecycle** (Queued)
    Open, render, close, release, and reopen a PDF; record document/page and
    retained-object state. Existing route: visual-pdf-viewer.
13. **P9-13 — Speaking/report generation and preview close** (Queued)
    Verify output and cleanup, excluding Sub Prep output generation measured
    in P9-08. Existing route: lifecycle-speaking-evaluation.
14. **P9-14 — Staff Directory and AI batch review** (Queued)
    Measure staff refresh/re-entry and large AI batch review. Staff route
    exists; AI batch coverage needs an explicit route or documented mapping.
15. **P9-15 — Schedule refresh and language/theme cycles** (Queued)
    Repeat schedule refresh, language change, and theme rebuild with bounded
    retained objects. Existing route: lifecycle-schedule; locale/theme coverage
    must be made explicit.
16. **P9-16 — Repeated workspace open/close** (Queued)
    Verify matched before/after process memory and no growth across repeated
    cycles.
17. **P9-17 — Large roster editing** (Queued)
    Measure large edit operations, peak use, and cleanup; no named Phase 0
    memory route was found. Begin Batch 4 discovery when this second-to-last
    Batch 3 candidate starts.
18. **P9-18 — Campus map open/close** (Queued)
    Measure decoded map retention and release; no named Phase 0 memory route
    was found.

P9-10 is distinct from P9-07 normal Calendar navigation and P9-08 Sub Prep.
No additional candidates were found in the earlier Phase 9 and Phase F list
review; confirm when Batch 4 discovery starts. Shared acceptance for each
candidate is current-source packaged Windows x64 Release evidence, identified
fixtures, process/memory metrics, bounded widget/editor and model counts where
relevant, and a measured return-to-baseline allowance. Keep transient budgets
advisory until that allowance is defined. The Phase 9 full Debug CTest and
final packaged Release exit gates remain separate from these scenario slices.
