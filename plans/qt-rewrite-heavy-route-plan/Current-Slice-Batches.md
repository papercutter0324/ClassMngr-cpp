# Current Slice Batches

## Phase 8 — Active Batch 2 (7 candidates)

1. **8.1.10 Speaking Evaluation individual Save As chooser — accepted:** route the report PDF chooser through `FileDialogPort`; retain the suggested name, PDF filter, `pdf` suffix, overwrite confirmation, cancellation behavior, and error reporting. Focused Linux CTest passed Bootstrap (89 cases) and Speaking Evaluation (40 cases).
2. **8.1.11 Speaking Evaluation batch folder and open-folder paths — accepted:** route folder selection through `FileDialogPort` and post-save folder launch through `ExternalUrlLaunchPort`; remove the dialog's direct platform fallbacks and preserve save-success ordering. Focused Linux CTest passed Bootstrap (89 cases) and Speaking Evaluation (40 cases).
3. **8.1.12 Document Viewer Save Copy integrations — active:** route the export file chooser through `FileDialogPort` and opening the saved copy through `ExternalUrlLaunchPort`; preserve the existing save and open failure behavior.
4. **8.2.7 Schedule generated-output parity matrix:** extend the accepted macOS fixture raster result to the other supported platforms, retaining content checks and documented rendering tolerances.
5. **8.2.8 Roster generated-output parity matrix:** extend the four accepted Next/Legacy template cases beyond the current macOS-only result to every supported platform.
6. **8.2.9 Sub Prep generated-output parity matrix:** extend matched-SQL C53 output evidence beyond macOS and the existing Windows evidence to every supported platform.
7. **8.2.10 Speaking Evaluation report-output baseline:** establish and compare Next output with the Legacy baseline; the accepted printing adapter has no Next/Legacy output parity claim yet.
