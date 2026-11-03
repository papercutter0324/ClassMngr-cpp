# Current Slice Batches

## Phase 8 — Active Batch 1

1. **8.1.1 External URL launching — complete:** committed as `838f2b8c`.
2. **8.1.2 Clipboard — complete:** committed as `a7d27adb`.
3. **8.1.3 Schedule Import chooser — complete:** committed as `c14920b3`.
4. **8.1.4 Sub Prep output directory — complete, slice commit pending:** use
   the existing `FileDialogPort` for output-folder selection.

Batch 1 is ready to close after candidate 4 is committed and pushed.

## Phase 8 — Next Batch 2 (discovered; pending activation)

1. **8.1.5 Dialog geometry:** inject the existing preferences port into
   `DialogShell` rather than constructing its platform adapter there.
2. **8.1.6 Main-window geometry:** persist/restore Next window state through a
   narrow settings adapter.
3. **8.1.7 Splitter state:** persist in-scope Next splitter sizes with isolated
   keys and lifecycle-safe restoration.
4. **8.1.8 Application icon:** apply the icon to the Next window and native
   Windows/macOS application targets.
5. **8.1.9 Font-family lookup:** remove direct `QFontDatabase` policy lookups
   from Navigation and Teacher Profile.
6. **8.2.1 QtPdf viewer session:** move document load/status/render/close and
   borrowed-stream release behind a session-scoped platform owner.
7. **8.2.2 PDF/print operation adapter:** isolate layout, fonts/images,
   resources, printer selection, preview, file output, and cleanup from viewer
   session ownership.
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
