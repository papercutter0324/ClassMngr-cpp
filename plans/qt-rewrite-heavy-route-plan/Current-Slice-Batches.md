# Current Slice Batches

## Phase 8 — Active Batch 1 (2 candidates)

1. **8.7.11 Qt Concurrent runtime staging — accepted:** Linux package build, verification, and upload passed at `a86e260be63615c075aefa4e460ea96832021fb8`; Windows x64/ARM64 and macOS universal package verification passed at `79cec8a3ed7344973f1321909a6644b31711ce63` with the same staging implementation. The optional Linux Phase 0 route remained uninvoked.
2. **8.7.13 Linux Legacy Debug link closure and Phase 5 rerun — implementation and focused target build passed; clean all-target build/CTest pending push:** the Runtime, WorkspaceStore, Sub Prep, and Calendar archives now share one `RESCAN` group. The fresh Linux GCC Debug `ClassMngr` target passed locally; the `refactoring-baseline` workflow must confirm all targets and the Phase 5 suite.

## Phase 8 — Next Batch (7 candidates discovered)

1. **8.1.10 Speaking Evaluation individual Save As chooser:** route the report PDF chooser through `FileDialogPort` and remove its direct `QFileDialog` fallback while preserving suggested name, PDF filter, extension, overwrite, and cancellation behavior.
2. **8.1.11 Speaking Evaluation batch folder and open-folder paths:** route folder selection through `FileDialogPort` and post-save folder launch through `ExternalUrlLaunchPort`; remove the dialog's direct platform fallbacks and preserve save-success ordering.
3. **8.1.12 Document Viewer Save Copy integrations:** route the export file chooser through `FileDialogPort` and opening the saved copy through `ExternalUrlLaunchPort`; preserve the existing save and open failure behavior.
4. **8.2.7 Schedule generated-output parity matrix:** extend the accepted macOS fixture raster result to the other supported platforms, retaining content checks and documented rendering tolerances.
5. **8.2.8 Roster generated-output parity matrix:** extend the four accepted Next/Legacy template cases beyond the current macOS-only result to every supported platform.
6. **8.2.9 Sub Prep generated-output parity matrix:** extend matched-SQL C53 output evidence beyond macOS and the existing Windows evidence to every supported platform.
7. **8.2.10 Speaking Evaluation report-output baseline:** establish and compare Next output with the Legacy baseline; the accepted printing adapter has no Next/Legacy output parity claim yet.
