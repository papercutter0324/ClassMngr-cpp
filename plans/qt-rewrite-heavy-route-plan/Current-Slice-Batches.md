# Current Slice Batches

## Phase 8 — Active Batch 2 (17 candidates)

1. **8.1.5 Dialog geometry — complete:** use a binary-safe callback boundary,
   keep Legacy storage behavior, and require callbacks for Next Roster output.
2. **8.1.6 Main-window geometry — complete:** restore through a dedicated
   Application port and SettingsManager adapter; persist only on accepted close.
3. **8.1.7 Splitter state — complete:** persist four retained Next page
   splitters with isolated settings keys, one-time restore, and suspension save;
   the transient Schedule Import review splitter remains unchanged.
4. **8.1.8 Application icon — complete:** runtime uses the packaged PNG; the
   Next macOS bundle carries ICNS metadata and the Windows target has a minimal
   ICO resource. Windows RC compilation awaits a Windows host.
5. **8.1.9 Font-family lookup — complete:** Navigation and Teacher Profile use
   the injected current-family list while retaining their distinct match and
   fallback rules; the resource loader's font registration is unchanged.
6. **8.2.1 QtPdf viewer session — complete:** the selected-document
   load/status/render/close and borrowed-stream release behind a session-scoped
   platform owner. `DocumentViewerPage` retains application state and controls;
   generated/print PDFs stay out of this session boundary.
7. **8.2.2 PDF/print operation adapter — complete:** add separate borrowed-
   document and ordered-batch print operations; the viewer uses the injected
   port, and batch PDF lifetimes are serial while preserving one print job.
8. **8.2.3 Schedule output — complete:** Next renders through the extracted
   `SchedulePdfRenderer` and uses `PdfPrintOperation` for print; its target no
   longer links `ClassMngrSchedulePrint`. Frozen C51 raster matched exactly.
9. **8.2.4 Roster output — complete:** Save As uses the file dialog adapter,
   Print uses `PdfPrintOperation`, and Preview owns its generated PDF session;
   four Next/Legacy template parity cases pass.
10. **8.2.5 Sub Prep output — complete:** Next uses the bounded projection and
    injected output operations; packaged SQL PDF parity passed separately from
    the still-open memory lifecycle gate.
11. **8.2.6 Speaking Evaluation reports — complete:** Next printing uses the
    ordered staged PDFs through `PdfPrintOperation`; Legacy keeps its fallback.
12. **8.3.1 PowerPoint automation — complete:** isolate availability, launch,
    failure, cleanup, and cancellation behind an application port.
13. **8.4.1 Update installation — complete:** move download/resume/cancel/
    launch/reveal behavior behind Next application/platform adapters.
14. **8.4.2 Next application package — complete:** replace package contents
    under existing updater assets; verify the manifest, Qt runtime/QML, direct
    resources, notices, and final archive/image or installer payloads.
15. **8.5.1 Calendar view — complete:** put lazy QML creation/release behind a calendar
    view interface with the compact event model.
16. **8.6.1 Transfer codec — complete:** stage or stream JSON read/write while preserving
    the 16 MiB compatibility and validation contract.
17. **8.6.2 Transfer workflow ownership — complete:** share one immutable
    package, preserve retry state on apply errors, and release it on success,
    cancel, invalid load, and workspace change.

Large-package peak-and-return measurement remains under the Phase 9/11 memory
and release-test gates; do not duplicate those gates in this batch.

## Phase 9 — Next Batch (20 candidates; gated on Phase 8 Batch 2 package CI)

1. **9.1 Measurement contract and artifact schema:** define named checkpoints,
   metrics, fixture/build provenance, and failure artifacts.
2. **9.2 Lifecycle instrumentation:** cover page, model, editor, and database
   result counts and ownership gaps found in the existing diagnostics.
3. **9.3 Resource instrumentation:** cover image/font/resource-cache ownership
   and PDF/export lifetime gaps found in the existing diagnostics.
4. **9.4 Packaged Windows Release runner:** validate installed x64 Release
   executable, architecture, Qt/CRT configuration, and deployed contents.
5. **9.5 Empty-workspace startup:** measure cold launch through first render.
6. **9.6 Representative-workspace startup:** measure startup-ready and first
   render with a fixed representative fixture.
7. **9.7 Idle and normal navigation:** capture five-minute idle and settled
   navigation separately from operation peaks.
8. **9.8 Repeated workspace/page lifecycle:** verify memory, handles, threads,
   and return after release.
9. **9.9 Theme and language rebuild:** verify settled memory across repeated
   palette and translation changes.
10. **9.10 Sub Prep 96-class lifecycle:** cover entry, summary, detail, refresh,
    output, leave, re-entry, widget/editor bounds, and memory return.
11. **9.11 Large schedule import:** measure parse, review, apply/cancel, and
    cleanup checkpoints.
12. **9.12 Large roster editing:** bound item/editor counts and measure release.
13. **9.13 Campus map lifecycle:** open, close, reopen, and measure decoded
    dimensions/bytes and release.
14. **9.14 Document catalog startup:** prove catalog metadata loads without PDF
    bodies or rendered pages.
15. **9.15 QtPdf and PDF preview lifecycle:** open, render, close, release,
    reopen, and measure preview cleanup.
16. **9.16 Calendar import and presentation:** measure the large import and
    retain the Phase 8 lazy-view lifecycle.
17. **9.17 My Classes, Classes, and Speaking navigation:** bound retained page,
    detail, and editor counts across repeated navigation.
18. **9.18 Staff Directory and AI batch review:** measure large review routes
    and bound table/editor counts.
19. **9.19 Speaking report generation:** measure operation peak, output release,
    and return to the pre-operation range.
20. **9.20 Class transfer export/import/cancel:** measure package/preview
    ownership, cancellation cleanup, peak, and return.

Phase 9 implementation and packaged measurements start only after the Phase 8
Batch 2 Windows, macOS, and Linux package workflows pass. The Phase 9 exit gate
follows candidate 20.
