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
7. **8.2.2 PDF/print operation adapter — in progress:** isolate layout,
   fonts/images, resources, printer selection, preview, file output, and cleanup
   from viewer session ownership.
8. **8.2.3 Schedule output:** remove the legacy print-service forwarding path
   and verify schedule output parity.
9. **8.2.4 Roster output:** route preview/print/save through the operation
   adapter and preserve existing Next/Legacy output comparisons.
10. **8.2.5 Sub Prep output:** use the bounded projection for the packaged SQL
    route and record output/memory evidence separately.
11. **8.2.6 Speaking Evaluation reports:** route report generation through the
    operation boundary with staged cleanup and bounded rendering.
12. **8.3.1 PowerPoint automation:** isolate availability, launch, failure,
    cleanup, and cancellation behind an application port.
13. **8.4.1 Update installation:** move download/resume/cancel/launch/reveal
    behavior behind Next application/platform adapters.
14. **8.4.2 Next application package:** produce one update artifact with the
    Next executable, Qt runtime/plugins, resources, QML, licenses, and metadata.
15. **8.5.1 Calendar view:** put lazy QML creation/release behind a calendar
    view interface with the compact event model.
16. **8.6.1 Transfer codec:** stage or stream JSON read/write while preserving
    the 16 MiB compatibility and validation contract.
17. **8.6.2 Transfer workflow ownership:** retain compact preview state, avoid
    full-package copies, and release operation data on success/failure/cancel.

Large-package peak-and-return measurement remains under the Phase 9/11 memory
and release-test gates; do not duplicate those gates in this batch.
