# Normal Workspace Schedule Import Evidence

Task ID: phase7-c31-schedule-workbook-import-20261028

## Capture set

Next captures cover English and Korean in light and dark palettes. Legacy captures cover the matched small source, selection, review, and overlap-conflict states in the same four locale/theme combinations. The root manifest links the paired Legacy images with a legacy- prefix; legacy-captures/manifest.json records the original Legacy capture measurements.

| State | Next image stem | Legacy image stem |
| --- | --- | --- |
| Source selection | small-source | small-source |
| Worksheet and block selection | small-selection | small-selection |
| Review | small-review | small-review |
| Overlap conflict | matched-conflict | overlap-conflict |
| Apply failure with retry enabled | small-failure-retry | Next only |
| 96-candidate parser stress conflict | large-conflict | Next only |
| Successful representative large review | large-applicable-review | Next only |
| Applied import and refreshed Schedule handoff | small-success-handoff | Next only |

Each dialog stem has en-light, en-dark, ko-light, and ko-dark PNGs. The dialog captures use QDialog::grab() at QT_SCALE_FACTOR=1 with QT_QPA_PLATFORM=offscreen; they show the dialog window, not a desktop crop. The success handoff is a Schedule PageHost capture at 1024×768 after the import summary is shown and the schedule rows have refreshed. It is Next-only because the legacy workflow does not use this page handoff.

Both implementations use 436×520 for source and selection, and 800×800 for review and conflict. Next uses model-backed review tables with scrolling; Legacy presents review cards. Next's narrow candidate preview clips part of a meeting time until the user scrolls horizontally. On overlap, Next reports a generic conflict status and does not name the conflicting pair; Legacy shows the conflicting class details. The dimensions and workflow states are comparable, while the controls and layout remain different. **This package makes no visual parity claim.**

## Data and bounds

generated-large-schedule-import.xlsx parses 96 class candidates across five schedule blocks. Its selected conflict block contains 20 review rows; the preview viewport shows no more than six at once. The test checks fewer than 200 dialog widgets and verifies that cancel clears class, teacher, preview, worksheet, block, and source-path data.

generated-large-schedule-import-applicable.xlsx is a separate non-overlapping 20-candidate block. The real normal-workspace flow applies it to a temporary .tps store and verifies 20 persisted classes and 30 schedule rows. This is a bounded representative apply alongside the parser-wide 96-candidate evidence; no 96-row apply is claimed.

The small-failure-retry capture shows a transactional write failure with **Apply schedule** still enabled. That same button retries the transaction after the cause is resolved; the capture test removes the injected database failure and clicks **Apply schedule** again. The small-success-handoff captures the refreshed Schedule page and its completion status after apply. Korean UI strings are translated, while the raw SQLite failure detail remains English. Legacy Korean also retains some English fallback strings.

Memory samples in manifest.json cover the whole macOS test process, including Qt, the page host, workflow objects, and runtime caches. They do not attribute memory to the parser or establish a process memory budget. The workflow tests verify owned review/source data is cleared on cancel and apply, and that late parse results are discarded; they do not claim that process RSS falls by a specific amount.

## Reproduction

From the repository root, run:

    env CLASSMNGR_SCHEDULE_IMPORT_LEGACY_CAPTURE_DIR=docs/qt-rewrite/visual-baseline/release/normal-schedule-import-next/legacy-captures QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 build/macos-clang-debug/ClassMngrScheduleImportDialogTests captureScheduleImportVisualEvidenceWhenConfigured

    env CLASSMNGR_NEXT_NORMAL_IMPORT_CAPTURE_DIR=docs/qt-rewrite/visual-baseline/release/normal-schedule-import-next QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 build/macos-clang-debug/ClassMngrNextNormalScheduleImportIntegrationTests captureImportVisualEvidenceWhenConfigured
