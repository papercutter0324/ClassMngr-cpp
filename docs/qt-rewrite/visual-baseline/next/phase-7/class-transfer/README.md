# Class Transfer

Candidate #29 was accepted at source commit `56455a1b`.

## Capture set

The [manifest](manifest.json) records 12 Legacy/Next state pairs (24 PNGs) for export selection, import review, and schedule-conflict feedback across English (`en_US`) and Korean (`ko_KR`), light and dark. Each pair uses the same 96-class fixture and selected class ID `1`, with a 1024×768 viewport. Tester inspected all three states in `en_US`/light and `ko_KR`/dark.

| Locale / theme | Export selection | Import review | Conflict feedback |
|---|---|---|---|
| en_US / light | [Legacy](en_US_light_legacy_export.png) · [Next](en_US_light_next_export.png) | [Legacy](en_US_light_legacy_review.png) · [Next](en_US_light_next_review.png) | [Legacy](en_US_light_legacy_conflict.png) · [Next](en_US_light_next_conflict.png) |
| en_US / dark | [Legacy](en_US_dark_legacy_export.png) · [Next](en_US_dark_next_export.png) | [Legacy](en_US_dark_legacy_review.png) · [Next](en_US_dark_next_review.png) | [Legacy](en_US_dark_legacy_conflict.png) · [Next](en_US_dark_next_conflict.png) |
| ko_KR / light | [Legacy](ko_KR_light_legacy_export.png) · [Next](ko_KR_light_next_export.png) | [Legacy](ko_KR_light_legacy_review.png) · [Next](ko_KR_light_next_review.png) | [Legacy](ko_KR_light_legacy_conflict.png) · [Next](ko_KR_light_next_conflict.png) |
| ko_KR / dark | [Legacy](ko_KR_dark_legacy_export.png) · [Next](ko_KR_dark_next_export.png) | [Legacy](ko_KR_dark_legacy_review.png) · [Next](ko_KR_dark_next_review.png) | [Legacy](ko_KR_dark_legacy_conflict.png) · [Next](ko_KR_dark_next_conflict.png) |

Legacy export/import use standalone dialogs; Next is shown inside `PageHost`. The paired fixture and state do not make these shell extents equivalent. The manifest explicitly sets `parityClaim: false`; no visual-parity claim is made.

## Transfer behavior and file-size contract

The Next route uses path-scoped `WorkspaceStore` actions and typed JSON v1 contracts. Transfer data includes teachers, classes, Details, Notes/Time Filler, schedules, roster, and evaluations; IDs are remapped into the destination profile. The tracked [fixture](../../../release/large-classes-visual-states/generated-large-classes-visual.tps) is copied for source and destination. Teacher 1 and class 1 use catalog-valid sample data; destination class 1 keeps its regular/intensive schedules, classes 2–96 have schedules cleared for readable conflict feedback, and the export package contains class 1.

The newly proposed file-size contract is inclusive at 16 MiB (16,777,216 bytes): that exact size is accepted, one byte over is rejected before parsing, and oversized export cancels atomically without replacing an existing file. This is a new product contract, not an existing Phase 7 numeric requirement. The historical 1,716,291-byte package passes; it contains 12 teachers, 48 classes, 8,640 roster cells, 26,400 evaluation cells, and 48 schedule rows. A 4,097-class package of 2,127,895 bytes parses, but applying all 4,097 classes was not tested. No `ClassTransferProjection` caps were reused; these results do not establish behavior for that untested apply size.

## Memory scope

The manifest's maximum macOS `peakWorkingSetBytes` is 88,457,216. The sequential process snapshots include Qt allocator retention and cumulative font/translation caches; they do not measure page allocation or establish a Windows peak-memory budget.

## Verification and remaining Legacy failure

The standard build passed for `ClassMngrNext`, `ClassMngrNextClassTransferWorkflowTests`, and `ClassMngrNextClassTransferVisualCapture`. Focused CTest passed 20/21 registrations; the 20 passing registrations cover the dependency boundary, new workflow/capture, codec parity, Next query/use-case/adapter targets, and Legacy transfer preview/apply/teacher-choice parity. The direct Next workflow QtTest passed 9/9. `git diff --check`, resource inventory `--check`, and route scan passed.

The remaining Legacy test is `ClassTransferTests::importDialogFallsBackToIndividualTeacherProfilesAfterBatchFailure` in `ClassMngrClassTransferTests`: it returned `Keep local: New Teacher` instead of `Keep local: Romanized One`. The offscreen aggregate reported 420 passed and 1 failed. Its test setup/expectation and `class_import_dialog.cpp` were unchanged; it builds an in-memory Legacy package and does not use the new WorkspaceStore route or codec. The report does not establish when this failure first appeared; no broad Legacy pass is claimed. No full-suite or Windows result is claimed.
