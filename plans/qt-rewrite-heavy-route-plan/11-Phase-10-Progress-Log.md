# Phase 10 — Progress Log

Read the [Phase 10 plan](11-Phase-10-Visual-Behavioral-and-Cross-Platform-Parity.md)
for current status, requirements, and contracts. This file is the historical
progress record for Phase 10; do not put dated progress updates in the plan.

### 2026-11-27 — Candidate 39 macOS AI-assisted comments follow-up

Accepted focused current-source macOS evidence at pushed HEAD
`89c9abad70253ad028fab93225cf64a8e0dd43fa`. Five C39 policy CTests
(#296, #297, #298, #301, #302), three provider/voice parity CTests
(#125, #126, #127), and 11 named page-test slots passed. Nine targets have
universal arm64 and x86_64 slices; execution was arm64 only. The final build
command succeeded with all targets already up to date, so it performed no
compile or link.

The 16 current-source captures passed name, dimension, and checksum checks.
Their hashes all differ from the historical C39 README images, which explicitly
make no pixel-parity claim. Independent review established that the Reports
row is later C40 UI and the Korean 25-candidate selection copy reflects the
current C39 cap; record both as expected content evolution and retain the README
images as historical. No AI network or browser was used. The independent report
audit verified 2,354 input hashes and restoration of all 29 shared build
outputs.

The result is synthetic offscreen macOS evidence only; x86_64 runtime and
Windows x64 acceptance remain open. The only available CI route was a full
`refactoring-baseline.yml` matrix (up to 300 minutes) that does not retain the
C39 PNG directory; GitHub MCP has no manual-dispatch operation and `gh` is not
installed. No Windows workflow was triggered. Keep C39 queued for Windows x64
and do not start C40. See the [verification report](../../artifacts/phase10/p10-macos-c39-ai-comments-20261127/VERIFICATION-REPORT.md).

### 2026-11-27 — macOS requalification of Candidates 34–38

Accepted focused current-source macOS evidence at HEAD
`ebb3950b1d45f8957a54c3d46cc955f63bd12aed`. The standard
`macos-clang-debug` build completed 152 Ninja steps for 18 targets. All built
executables contained arm64 and x86_64 slices; tests ran on arm64 only.

- C34: six registrations passed; the Calendar page test had one disclosed
  diagnostic-only probe skip.
- C35: 19 selected roster editor/model slots passed.
- C36: 10 selected import/transfer slots and six support CTests passed.
- C37: roster output CTest and five support CTests passed.
- C38: 11 selected Speaking Evaluation slots and startup-boundary CTest
  passed.

Independent review accepted 80 decoded PNGs and eight C37 PDFs. Each PDF was
one page with expected fixture text; its raster matched the corresponding
preview crop exactly. All 135 artifact checksums passed. Source hashes matched
before and after, no code changed, and pre-existing build outputs were
restored. The archived package does not contain paired rerun image hash lists
for independently reconstructing the C35/C36/C38 rerun-hash comparison; this
does not affect the test results or verified captures.

This is synthetic offscreen evidence only. It does not establish x86_64
runtime, native AppKit behavior, physical printing, cross-host pixel parity,
full-suite health, or Phase 10 exit acceptance. See the [run report](../../artifacts/phase10/p10-macos-c34-c38-followup-20261127/run-report.md),
[commands](../../artifacts/phase10/p10-macos-c34-c38-followup-20261127/commands.txt),
[capture manifest](../../artifacts/phase10/p10-macos-c34-c38-followup-20261127/capture-output-manifest.json),
and [checksums](../../artifacts/phase10/p10-macos-c34-c38-followup-20261127/SHA256SUMS).

### 2026-11-27 - Candidate 37 Windows x64 Roster templates/printing acceptance

Accepted at source commit `8c86505d83e9364afe30386d46737698eb81c246`
(`Phase10 - Verify Testing roster Extra Info output`), changing only
`tests/next/roster_output_tests.cpp`. The capture selects `FIXTURE EXTRA` in
the Testing Classes dialog, checks the output request, and verifies the saved
PDF contains `FIXTURE TEST`, `FIXTURE EXTRA`, and `FIXTURE VALUE`. Both regular
and Testing previews require one populated page whose saved screenshot crop
matches its independently rendered PDF raster.

The incremental Debug build of the five focused targets succeeded in the
standard `build/windows-x64-debug` directory after the ordinary MSBuild
FileTracker `E_ACCESSDENIED` and reviewed-access retry. Five focused registered
CTests passed: `ClassMngrNextRosterOutputTests` (#623, 20.59 s),
`ClassMngrSubPrepRosterOutputParityTests` (#164, 2.12 s),
`ClassMngrRosterTemplatePrintServiceTests` (#169, 4.71 s),
`ClassMngrNextApplicationRosterTemplatePrintSourceReadQueryTests` (#279,
0.02 s), and
`ClassMngrNextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests`
(#520, 0.33 s). The separate `NextStartupWorkspaceDependencyBoundary` check
(#261, 0.02 s) passed. The Phase 7 history does not name its original five
focused tests; this is the current C37 selection, not a reconstruction of that
historical command. This is focused Windows evidence, not a full-suite pass.

The fresh evidence set contains 16 English/Korean light/dark dialog and preview
PNGs, eight generated PDFs, and eight rendered-page PNGs. The Testing previews
show the selected Extra Info header and value; all eight screenshot page crops
match their PDF rasters. Each Testing page has 25,760 non-white pixels and each
regular page has 99,580. The independent review accepted all 16 captures.
Source-input hashes remained stable across build and tests; all 182 prior root
captures and 59 Calendar captures were unchanged, with only 33 new C37 outputs.
Prior app-directory visual and PDF/page outputs were restored. This is synthetic
Windows evidence only; no physical-printer or cross-platform screenshot-pixel
parity claim is made. See the [C37 execution report](../../artifacts/phase10/p10-win-c37-roster-templates-printing-20261127/run-report.md),
[visual review](../../artifacts/phase10/p10-win-c37-roster-templates-printing-20261127/visual-review.md),
and [capture set](../../captures/next-c37-roster-templates-printing-20261127/).

### 2026-11-27 — Candidate 36 Windows x64 Roster score import/student transfer acceptance

Accepted at source commit `f60f3b5a028088770da97f484798ef2bb215e27b`
(`Phase10 - Verify roster score import and transfer captures`), changing only
`tests/next/roster_editor_tests.cpp`. The standard incremental Debug build of
`ClassMngrNextRosterEditorTests` succeeded after the ordinary MSBuild
FileTracker `E_ACCESSDENIED` and reviewed-access retry. Registered CTest #335
passed its filtered selection (1/1, 2.34 s) with these ten slots:
`transferActionTracksCurrentRowContent`,
`scoreImportMapsTrimmedPairsAndIsIdempotent`,
`scoreImportRequiresNameColumnsAndUsesAvailableGradeColumns`,
`scoreImportReadFailureDoesNotMutateAndAutosaveFailureRetries`,
`transferCancelAndFailuresPreserveDirtySelectionAndTimer`,
`transferMenuFiltersOtherGradesAndDisablesFullTargets`,
`transferSuccessRemovesSourceOnlyAfterPairSave`,
`pathRosterWorkflowActionsStayScopedAndPairSaveIsAtomic`,
`transferTargetNamesMatchLegacyFormattingAndFallbacks`, and
`capturesImportAndTransferStatesAcrossLocalesAndThemes`.

Five supporting functional CTests also passed individually without forwarded
arguments: `ClassMngrNextApplicationSpeakingEvaluationRosterScoreImportTests`,
`ClassMngrNextApplicationSpeakingEvaluationRosterScoreRowAssignmentsTests`,
`ClassMngrNextApplicationRosterRowTransferPreparationTests`,
`ClassMngrNextApplicationRosterRowTransferUseCaseTests`, and
`ClassMngrNextApplicationRosterTransferTargetEligibilityTests`. The separate
`NextStartupWorkspaceDependencyBoundary` check passed. This is focused C36
coverage, not an unfiltered #335 run or a project-suite pass. Candidate35
editor/model slots, P9-17 diagnostics, and C37 output/printing tests were
excluded.

The capture slot produced eight readable 1024x768 English/Korean light/dark
images: four scores visible after import and four transfer states showing
Student A moved while Student B became the source roster's first row. The
manifest and pre-grab assertions cover seven packaged fonts, locale-specific
font resolution, Latin/Hangul coverage, and the 25-row/six-column model. The
viewport shows about 14 rows at once with the remaining rows available by
scrolling. Visual review found no defect; the captures make no pixel-parity
claim.

The 106-input record found no changes after the final build/test; the intended
`tests/next/roster_editor_tests.cpp` working-tree edit was the only input that
differed from the pre-build HEAD snapshot, while the other 105 inputs matched
HEAD and the pre-build hashes. The generated Korean `.qm` was unchanged. All
173 pre-existing root captures and all 59 shared Calendar captures remained
byte-identical; the nine C36 PNG/manifest files are the only new root captures.
The nine prior Debug app-directory outputs were restored, and C35 capture files
remained unchanged. See the [C36 execution report](../../artifacts/phase10/p10-win-c36-roster-score-transfer-20261127/run-report.md)
and [fresh capture set](../../captures/next-c36-roster-score-transfer-20261127/).

### 2026-11-27 — Candidate 35 Windows x64 Roster editor/student model acceptance

Accepted at source commit `0e15866f6daff47b71b7303835b47dc1c40596ef`
(`Phase10 - Load packaged fonts in roster captures`), changing only
`cmake/tests/next.cmake` and `tests/next/roster_editor_tests.cpp`. The capture
fixture registers the six packaged Inter faces and Pretendard Variable through
`ResourceLoader`, applies the production locale family order, checks resolved
application/editor/table families and Latin `A`/Hangul U+AC00 glyph coverage,
preserves the application font size, and restores font, palette, locale, and
translator state.

The standard incremental Debug build of
`ClassMngrNextRosterEditorTests` succeeded. Registered CTest #335 passed the
filtered C35 selection (1/1, 5.76 s; 19 named slots): 15 editor/model behavior
slots — `editorLoadsOnDemandAndUsesOnlyActiveCellEditors`,
`modelDisplayModeShowsExtraRowsAndExtendsOnEdit`,
`safeCustomCellEditSkipsFullValidationAndMatchesCanonicalIssues`,
`customCellLengthBoundaryCrossingsRevalidate`,
`emptyRowTransitionsAndImportedGradesRevalidate`,
`rowHasDataMatchesTrimmedCellContent`,
`emptyRosterAndReadFailureHaveDifferentStatesAndNoFailureWrite`,
`automaticManualAndFailedSaveRetryRetainLatestDraft`,
`discardReloadsVisibleWindowAndClearsDraft`,
`structuredCellIssuesLocalizeAndQuestionableKoreanRequiresConsent`,
`exactWorkspacePathsPersistWidthsAndKeepSparseTailIndexes`,
`customColumnTailDataRequiresTwoStageConfirmation`,
`normalizedHeaderTailDataRequiresCancelOrExplicitDeletion`,
`ambiguousNormalizedDuplicateHeadersFailClosed`, and
`pathActionsRejectMalformedIdsBeforeOpeningWorkspace`; three editor
diagnostics —
`defaultHeapDiagnosticEventsCoverLoadedCellSaveAndSuccessfulLeave`,
`saveStageDiagnosticsBracketSnapshotAndFollowSaveBoundaries`, and
`defaultHeapAfterDeleteCheckpointFollowsWidgetAndChildDestruction`; and
`capturesEnglishAndKoreanLightAndDarkRosterStates`. C36 score import/transfer,
P9 diagnostics, C37 output/printing, and the unfiltered CTest were excluded.
This is focused acceptance evidence, not a project-suite pass.

The four fresh 1024x768 English/Korean light/dark captures show readable text
and the expected theme states. The manifest records the 25-row, 156-column
model and locale-specific font resolution. This is presentation evidence, not
pixel-parity evidence. The 100-input source set remained unchanged during
capture and verification; of 173 root capture files, only the five C35 outputs
changed and the other 168 remained byte-identical. All 59 shared Calendar
captures were unchanged, and the prior Debug app-directory outputs were
restored.

The first pre-font captures contained square glyphs. The first run after adding
font registration opened an MSVC assertion because `QVERIFY2` eagerly evaluated
a description that called `.error()` on a successful `std::expected`; it was
manually stopped after 209.03 s, while CTest's configured timeout was 1500 s.
The error access now occurs only in failure branches. A later default-sandbox
capture could not read the generated font package; the focused capture and
final filtered run passed using reviewed access. See the [execution report](../../artifacts/phase10/p10-win-c35-roster-editor-20261127/run-report.md),
[visual review](../../artifacts/phase10/p10-win-c35-roster-editor-20261127/visual-review.md),
[nested verification report](../../artifacts/phase10/p10-win-c35-roster-editor-20261127/font-fixture-fix-20261127/verification-report.md),
[exact command](../../artifacts/phase10/p10-win-c35-roster-editor-20261127/font-fixture-fix-20261127/ctest-final-19-c35-slots-command.txt),
and [fresh capture set](../../captures/next-c35-roster-editor-20261127/).

### 2026-11-27 — Candidate 33 Windows x64 Calendar acceptance

Accepted at source commit `0ff229f52479481a66e4e624f1e88620928d7329`, which
changes only `tests/next/calendar_page_tests.cpp`. The first build and captures
used nonstandard `build/windows-x64-debug-p10-c33` and did not qualify the
slice. The later incremental `ClassMngrNextCalendarPageTests` build succeeded
in standard `build/windows-x64-debug` after ordinary MSBuild FileTracker
`E_ACCESSDENIED` and a reviewed-escalation retry; prior commit
`d9f04bc9` contains the safe generated-resource recovery and exact-manifest
no-op baker change that enabled standard-folder work. Focused CTest #621 with
explicit `-C Debug` passed independently (1/1, 32.054 s); QtTest reported 32
passed, 0 failed, 1 skipped. This is focused evidence, not a project-suite
pass.

An intermediate run showed stale parent-QWidget captures while direct QML was
current. Capture readiness now brackets QWidget grabs with fresh `QQuickWidget`
frames, synchronizes via `QQuickWidget::grab()`, and crops the actual Quick
scene rather than the taller QWidget. A native-DPR probe found QWidget
geometry 1240×535 versus the 1240×500 Quick scene. All four English/Korean
light/dark page and Preferences captures were reviewed, with Campus meeting,
Term planning, and School holiday visible and no loading overlay. At DPR 1, the
unclipped, unscaled 1240×500 scene/root/window/direct/page comparisons had
zero differing pixels out of 620,000 per context, below the unchanged 0.5%
threshold, after four attempts per context. An RAII guard restores incoming
locale and palette for subsequent grid tests. The shared
`build/windows-x64-debug/calendar-captures` tree retained all 59/59 hashes;
`git diff --check` passed. See the [execution and capture report](../../artifacts/phase10/p10-win-c33-calendar-20261126/readiness-fix-20261127/report-restore-global-state-20261127.md),
[scene-bounds evidence](../../artifacts/phase10/p10-win-c33-calendar-20261126/readiness-fix-20261127/scene-bounds-bracket-report-20261127.md),
[independent focused CTest transcript](../../artifacts/phase10/p10-win-c33-calendar-20261126/readiness-fix-20261127/independent-verification-20261127/ctest-post-palette-fix.stdout.log),
[parity review](../../artifacts/phase10/p10-win-c33-calendar-20261126/readiness-fix-20261127/independent-verification-20261127/capture-review-summary.txt),
and the [fresh capture set](../../captures/next-independent-post-palette-fix-20261127/).

### 2026-11-27 — Candidate 34 Windows x64 Calendar import acceptance

Accepted at source commit `3d4f333b798cfd979d62ed4bbdbe7f9f2713ee85`
(`Phase10 - Show full Calendar import error details`). The incremental build
in `build/windows-x64-debug` emitted five executable targets; after an
ordinary MSBuild FileTracker `E_ACCESSDENIED`, the same command succeeded on
reviewed-escalation retry. One focused Windows x64 Debug CTest run passed all
six registrations: #41 parser, #261 startup dependency boundary, #434 import
use case, #552 campus-code adapter, #621 Next Calendar page/import, and #622
WorkspaceStore actions under reviewed escalation. The independent post-layout
run exited 0 in 32.525 s (CTest total 32.22 s); #621 reported 32 passed,
0 failed, 1 skipped. This is focused evidence, not a project-suite pass.

Early page-test runs failed without a useful LastTest diagnostic and emitted
no post-change images. Pre-fix images showed the full English error but clipped
Korean text after “The downloaded spreadsheet is missing”. The status label
now sizes to `heightForWidth()`; Korean light/dark captures show the full
`xl/workbook.xml.` detail, and the regression assertion covers both themes.
Import/parser text is unchanged. C33 images were not reused because their
manifest did not cover matching Candidate34 inputs. The fresh set contains 24
PNGs plus `calendar-captures.json`; page, Preferences, and all four import
outcomes were reviewed across English/Korean light/dark. The three page chips
are present with no loading overlay. Parity is complete with zero differing
pixels per context.

The refreshed 40-input SHA-256 manifest matches the committed blobs at the
Candidate34 commit; source snapshots before and after capture match, and the
capture manifest hash is
`0fecda98b905de7d52ae8280b2baabce4f25f1b91d8b7bcdec44f3d14e2fafd9`. The
shared `build/windows-x64-debug/calendar-captures` tree remained 59/59 hashes.
See the [post-fix CTest transcript](../../artifacts/phase10/p10-win-c34-calendar-import-20261127/independent-verification-layout-fix-20261127/ctest-six-layout-fix.stdout.log),
[run context](../../artifacts/phase10/p10-win-c34-calendar-import-20261127/independent-verification-layout-fix-20261127/ctest-six-layout-fix.run.txt),
[40-input manifest](../../artifacts/phase10/p10-win-c34-calendar-import-20261127/source-inputs.sha256),
and [source refresh report](../../artifacts/phase10/p10-win-c34-calendar-import-20261127/source-refresh-summary.md).
The [capture review](../../artifacts/phase10/p10-win-c34-calendar-import-20261127/independent-verification-layout-fix-20261127/capture-review-summary.txt),
[59-file shared-tree comparison](../../artifacts/phase10/p10-win-c34-calendar-import-20261127/independent-verification-layout-fix-20261127/shared-captures-comparison.txt),
[fresh capture set](../../captures/next-independent-c34-layout-fix-20261127/),
and [capture manifest](../../captures/next-independent-c34-layout-fix-20261127/calendar-captures.json)
hold the visual and hash evidence.

### 2026-11-27 — Phase 10 next-batch discovery (Candidates 34–53)

Read-only discovery at source HEAD
`2e26e6b09d0d75e4cff46efef79903d1edbd78e2` identified 20 Phase 7 entries,
Candidates 34–53. Candidate 41 (Campus Directory) is omitted: P10 C24 has 52
captures and focused directory CTest/admin-route evidence, and all 9 source
inputs match. Candidate 42 (Document Catalog navigation) is omitted because
P10 C22 has 13 current inputs for that navigation scope; PDF viewing is
explicitly outside the scope and remains with Candidate 43. The ordered next
batch is 18 slices:

1. [Candidate 34 — Calendar import](08-Phase-7-Progress-Log.md#L950): focused
   import-use-case, campus-code query, Next Calendar import-state tests and
   captures; reuse C33 import visuals only if the manifest proves fresh,
   matching inputs.
2. [Candidate 35 — Roster editor/student model](08-Phase-7-Progress-Log.md#L992):
   `ClassMngrNextRosterEditorTests` and English/Korean light/dark captures;
   no matching Windows package.
3. [Candidate 36 — Roster score import/student transfer](08-Phase-7-Progress-Log.md#L1010):
   editor import/transfer captures and WorkspaceStore actions; P10 C25 is
   class transfer, not student transfer.
4. [Candidate 37 — Roster templates/printing](08-Phase-7-Progress-Log.md#L1034):
   Next roster output and template-print tests. P10 C5's current-source
   comparison matches 13/15 inputs and covers By Day, Daily, and Per Class
   PDFs; confirm target definitions and cover Testing Classes and Extra Info.
5. [Candidate 38 — Speaking Evaluation core](08-Phase-7-Progress-Log.md#L1067):
   Next page tests and English/Korean light/dark captures; no Windows page
   workflow evidence.
6. [Candidate 39 — AI-assisted comments/batch workflow](08-Phase-7-Progress-Log.md#L1107):
   eligibility, prompt, selection, review, and apply cases; no Windows
   AI-comment evidence.
7. [Candidate 40 — Reports/PDF/PowerPoint/export jobs](08-Phase-7-Progress-Log.md#L1133):
   report widget, batch report, widgetless PDF, and UI/export states; reuse
   applicable P10 C5/C7 outputs and add current report UI/export cases. Make
   no live Office automation claim.
8. [Candidate 43 — Document viewer lifecycle/actions](08-Phase-7-Progress-Log.md#L1354):
   Next viewer tests/captures, including Legacy viewer; no current Windows
   viewer artifact, and C22 did not open a PDF.
9. [Candidate 44 — Sub Prep class information/output](08-Phase-7-Progress-Log.md#L1412):
   class model/page, captures, and print tests; rebuild and capture current
   source because P10 C4/C53 renderer inputs changed and the prior
   page-grouping comparison remains open.
10. [Candidate 45 — App-wide Preferences](08-Phase-7-Progress-Log.md#L1456):
    model/snapshot adapters and the production Preferences route in Next
    bootstrap; no matching Windows route package.
11. [Candidate 46 — Admin/permission gates](08-Phase-7-Progress-Log.md#L1496):
    operation-level gate tests. P10 C24 has current captures but only directory
    CTest and shell-route evidence.
12. [Candidate 47 — User-initiated updates](08-Phase-7-Progress-Log.md#L1526):
    coordinator/shared presentation/manual Help route and 36-state capture
    matrix. Existing updater package and tests match except two CMake files;
    current route/presentation registrations remain to verify.
13. [Candidate 48 — Useful Links external navigation](08-Phase-7-Progress-Log.md#L1565):
    selection behavior, locale labels, and four captures. Existing launch
    package is partial (11/13), and bootstrap tests/CMake differ; rerun focused
    tests/captures.
14. [Candidate 49 — Workspace Calendar](08-Phase-7-Progress-Log.md#L1601):
    My Workspace page/calendar tests and capture. P10 C19 has 21 captures and
    focused tests but an incomplete source inventory; recheck inputs and
    refresh if needed.
15. [Candidate 50 — Classes parity/transfer boundary](08-Phase-7-Progress-Log.md#L1630):
    navigator/full-window captures and transfer workflow. P10 C20 page source
    differs, while C25 transfer inputs match; rerun the navigator and refresh
    captures if product inputs differ. C26–29 cover adjacent sections.
16. [Candidate 51 — Schedule import/output follow-up](08-Phase-7-Progress-Log.md#L1669):
    normal schedule integration, Schedule page/output, and five-block
    comparisons. P10 C30–32 are adjacent evidence; C5's integration source
    differs. Run the current integration target and check inputs before reuse.
17. [Candidate 52 — Document Catalog live-locale behavior](08-Phase-7-Progress-Log.md#L1701):
    bootstrap live-locale switch plus catalog/viewer tests and captures; C22
    covers navigation, not PDF/viewer-session behavior.
18. [Candidate 53 — Sub Prep PDF text mapping/output parity](08-Phase-7-Progress-Log.md#L1726):
    PDF finalizer/package tests and current C53 SQL parity capture. Hosted
    Phase 7 and local P10 C4 evidence are older; renderer and test CMake inputs
    differ, so rerun focused tests/current capture and retain the prior grouping
    observation as open.

Candidates 55–56 are reserved for the next discovery point, not this batch.
No build, test, or capture ran during discovery. The live order and statuses
are in the [current slice-batch tracker](Current-Slice-Batches.md).

### 2026-11-26 - Candidate 33 Windows x64 Calendar discovery

Read-only discovery confirmed four relevant CTest registrations: the Legacy
Calendar event-mutation parity suite, the Next startup dependency boundary,
the Next Calendar page suite, and the Next Calendar WorkspaceStore actions
suite. Only the Next page executable currently exists in the shared Debug
build. The Phase 7 Candidate #33 package has 12 macOS-origin reference images;
Candidate 19's Windows Workspace Calendar captures cover a separate page.
The Next capture slot writes to a populated fixed build directory containing
59 files, so execution will use an isolated x64 Debug binary directory. No
tests or captures ran during discovery. See
`artifacts/phase10/p10-win-c33-calendar-discovery-20261126/discovery-report.md`.

### 2026-11-26 - Candidate 32 Windows x64 Schedule output/printing discovery

Read-only discovery found adjacent Windows x64 C51 PDF and output-adapter
evidence, with current PDF/adapter executable hashes matching the prior
package. It does not include Candidate32's Testing Assignment editor, output
options, and representative PDF capture states. The current Schedule capture
target provides a `--c32` path. CTest lists five print-model, PDF, and adapter
registrations; the print-model executable is absent. The bounded next slice
will build the model/PDF/adapter tests and capture target, run the five focused
tests, and capture to a fresh `--c32` directory. `ctest -N` only was used.
No build, test, or capture ran. Evidence:
`artifacts/phase10/p10-win-c32-schedule-output-discovery-20261126/`.

### 2026-11-26 - Candidate 32 Windows x64 Schedule output/printing verification

At source HEAD `2a45dd9e37dc4bc041aa9faecf010e4b23ead5f0`, the Windows x64
Debug build succeeded for the print-model, print-PDF, Next output-adapter, and
visual-capture targets. The five focused CTests passed 5/5 under the Windows
platform renderer. A separate offscreen run failed only the PDF target; its
pixel/text diagnostics are preserved, and the PDF test passed under Windows
QPA. The fresh `--c32` Windows QPA capture exited 0 and produced 12 dialog
images plus a one-page PDF preview. The manifest references eight pending-slot
manager images reused from Candidate 29. All 19 source input hashes match;
every reference resolves and the reviewed Korean labels are readable. No
pixel-parity or native interaction claim is made. See
`artifacts/phase10/p10-win-c32-schedule-output-20261126/execution-report.md`
and `visual-review.md`.

### 2026-11-26 - Candidate 31 Windows x64 Schedule Import discovery

The existing P10-WIN-34 Windows x64 package already covers Candidate #31:
three focused CTests passed, followed by Legacy and Next capture CTests; its
72 PNG files and 94 SHA-256 entries verify. Current import test sources and
all three test executables match the package. The normal-import CMake block is
unchanged, and no product `src/` path changed since the package's build
baseline. `ctest -N` confirmed all three registrations without running them.
No duplicate build, test, or capture is proposed. This discovery ran no build,
test, or capture. Report and current input hashes:
`artifacts/phase10/p10-win-c31-schedule-import-discovery-20261126/`.

### 2026-11-26 - Candidate 30 Windows x64 Schedule verification

At source HEAD `f1bba551432781ae8b98b78718de23540a8761ad`, the
`ClassMngrNextScheduleWorkspaceStoreActionsTests` Windows x64 Debug build
succeeded in 17.84 seconds and its focused CTest passed 1/1 in 11.27 seconds.
The target declares three cases for path isolation, transactional testing
assignment behavior, and errors. Combined with the source-matched page/capture
executables and 38-record Windows package, Candidate 30 now has scoped
WorkspaceStore, page, and visual-capture evidence. All 11 direct input hashes
and artifact hashes match. No new capture was needed; no product or test source
changed. Details are in
`artifacts/phase10/p10-win-c30-schedule-20261126/`.

### 2026-11-26 - Candidate 30 Windows x64 Schedule discovery

Read-only discovery at current HEAD `d11ed895a77aa652c4ac5618fb99d8de41fcb688`
found an existing Windows x64 Schedule package from source HEAD
`6208ebe92b87d04b1beaa436396e166db1c095a5`. Its focused page and capture
CTest passed 2/2 and its manifest has 38 Regular/Intensive/Testing records.
Current-source comparison found 27/28 tracked inputs identical; the only file
hash difference is global `cmake/tests/next.cmake`. Both Schedule target blocks
are unchanged and both current executables match their recorded hashes.

The registered `ClassMngrNextScheduleWorkspaceStoreActionsTests` CTest has no
current x64 executable; the existing package did not cover it. The next bounded
slice will build that target and run its single CTest. `ctest -N` confirmed the
registration without running it. No build, test, or capture ran during this
discovery. Report and input inventory:
`artifacts/phase10/p10-win-c30-schedule-discovery-20261126/`.

### 2026-11-26 - Candidate 29 Windows x64 Testing Classes requalification

At source HEAD `1cb3fa3a8fc02c3c20934240ca1cb1066c994660`, the Visual Studio 18
2026 x64 Debug build succeeded for the manager-test and capture targets. The
focused CTest run passed 2/2 in 82.11 seconds, including all 16 declared
manager slots and the capture registration. A separate direct offscreen run
produced 32 decoded 1024x768 PNGs for Details, Notes, Roster, and Pending slot
across en_US/ko_KR and light/dark. The manifest uses class 3 from the 96-class
fixture; pending-slot state is Thursday at 10:00. All 25 source-input hashes
and 46 artifact inventory hashes match.

English and Korean labels are readable with no missing glyphs. The current
four-state source output exceeds the historical three-state README/manifest
and CMake comment; this evidence records current output without changing the
historical package or making a separate Roster acceptance decision. No product
or test source changed. Logs, captures, manifest, verification, and review are
in `artifacts/phase10/p10-win-c29-testing-classes-20261126/`. The bounded
offscreen result makes no pixel-parity or native-window claim.

### 2026-11-26 - Candidate 29 Windows x64 Testing Classes discovery

Read-only discovery at source HEAD `59ffb201ca17da1812e4ff2de1cf1ef975a5b44f`
confirmed current CMake/CTest registrations for
`ClassMngrNextTestingClassesManagerTests` and
`ClassMngrNextTestingClassesVisualCapture`; no matching Phase 10 Windows x64
capture was found. The manager test contains 16 slots. Historical Phase 7
materials specify 12 paired states (24 PNGs), but current capture source
iterates Details, Notes, Roster, and Pending slot, predicting 16 pairs (32
PNGs). The CMake comment and historical materials omit Roster, so execution
will verify and report the actual manifest and file set.

The bounded next slice will build the two Windows x64 Debug targets, run only
their registered CTests, and capture to fresh artifact storage. It will
validate manifest counts, fixture/selection, image dimensions and decoding,
and representative English/light and Korean/dark images across each emitted
state. This discovery ran no build, test, or capture. Report and source hashes:
`artifacts/phase10/p10-win-c29-testing-classes-discovery-20261126/`.

### 2026-11-26 — Candidate 28 Windows x64 Classes Details/Co-Teacher requalification

At source HEAD c6797a3a4f4e3e7bc3478735034fbbc8d137d09d, the Windows x64
Debug build exited 0 for ClassMngrNextClassesNavigatorPageTests,
ClassMngrNextClassesNavigatorWorkspaceStoreTests, and
ClassMngrNextClassesDetailsCoTeacherVisualCapture. The focused page and
WorkspaceStore CTests passed 2/2 in 56.94 seconds. Five selected Details and
Co-Teacher page cases and the path-actions transaction case each exited 0.
The paired offscreen capture exited 0 and produced 16 decoded 1024x768 PNGs
across eight section/locale/theme entries. All 17 source-input hashes match.

English and Korean labels are readable. The Legacy Details selector is blank
while Next shows Level 02, confirming the known Phase 7 fixture/catalog
limitation. Details schedule content extends below the crop; Co-Teacher data
is visible. These are descriptive observations with no pixel-parity or defect
conclusion. No product or test source changed. Logs, manifest, captures,
review, and verification are in
artifacts/phase10/p10-win-c28-classes-details-coteacher-20261126/.

### 2026-11-26 — Candidate 28 Windows x64 Classes Details/Co-Teacher discovery

Read-only discovery at source HEAD b67ee6ea922ceb5b7ac840ca0576a49495c8eb83
selected the paired Details/Times and Co-Teacher capture as the next bounded
Windows x64 follow-up. CMake and CTest inventory confirmed the current capture
target plus focused Navigator page and WorkspaceStore tests. No current-source
Phase 10 Windows x64 capture was found. The historical Phase 7 package has 16
images across four locale/theme pairs but is not Windows evidence.

Proposed acceptance: build the page-test, WorkspaceStore-test, and capture
targets as Windows x64 Debug; run only the two focused CTests and five page
cases plus the path-actions transaction case; capture directly into fresh
artifact storage. Validate the eight section/locale/theme entries and 16
1024x768 images, class 1, the tracked 96-class fixture, and readable English
and Korean labels. The known Level 02 Legacy selector/catalog mismatch is a
fixture observation, not a new defect finding. The differing shells prevent a
pixel-parity conclusion. This discovery ran no build, test, or capture. Its
report and 17 source hashes are in
artifacts/phase10/p10-win-c28-classes-details-coteacher-discovery-20261126/.

### 2026-11-26 — Candidate 27 Windows x64 Classes Notes requalification

At source HEAD 602e33f2fcc2617234eee5154efcd7fad0e14bfd, the Windows x64
Debug build exited 0 for ClassMngrNextClassesNavigatorPageTests,
ClassMngrNextClassesNavigatorWorkspaceStoreTests, and
ClassMngrNextClassesNotesVisualCapture. The focused page and WorkspaceStore
CTest selection passed 2/2 in 46.85 seconds. Five Notes page cases and the
classNotesPathActionsBoundTextAndPreserveDetailsTransactionally case each
exited 0. The paired offscreen capture exited 0 and produced eight decoded
1024x768 PNGs across four locale/theme pairs. All 17 source-input hashes match.

English and Korean labels are readable. The Legacy Save widget was found but
hidden outside the viewport in all four entries: the page measured 1024x843
for English with Save at (35,816), and 1024x841 for Korean with Save at
(35,814), both 100x30. Next's Save Changes control is visible in its route.
The cropped viewport and differing shells are observations only; no
pixel-parity or defect conclusion is made. No product or test source changed.
The manifest, captures, review, logs, and verification are in
artifacts/phase10/p10-win-c27-classes-notes-20261126/.

### 2026-11-26 — Candidate 27 Windows x64 Classes Notes discovery

Read-only discovery at source HEAD c1f4cc5f24f00f090a7c3292ded3b0f26c6929e2
selected the paired Classes Notes and Time Filler capture as the next bounded
Windows x64 follow-up. The current CMake registers the Notes capture, Classes
Navigator page tests, and Navigator WorkspaceStore tests; CTest -N confirmed
their names without running them. No current-source Phase 10 Windows x64 Notes
capture was found. The existing Phase 7 package covers eight Legacy/Next
images across four locale/theme pairs, but its historical Candidate 26 result
is not Windows evidence.

Proposed acceptance: build the page-test, WorkspaceStore-test, and Notes
capture targets as Windows x64 Debug; run only the two focused CTests plus the
five Notes page cases and one path-actions transaction case; run the capture
directly into fresh artifact storage. Validate eight 1024x768 images, class 1,
the four locale/theme entries, Notes/Time Filler context, and the actual
Legacy Save-widget geometry/visibility recorded in the manifest. Review
readable English and Korean text without a pixel-parity claim. The capture
harness does not explicitly select a font. This discovery ran no build, test,
or capture. Its report and 17 source hashes are in
artifacts/phase10/p10-win-c27-classes-notes-discovery-20261126/.

### 2026-11-26 — Candidate 26 Windows x64 Classes Analytics requalification

At source HEAD 121852f289bd9c1db2c0ebd189cfdd535a0d5939, the Windows x64
Debug build exited 0 for ClassMngrNextClassesNavigatorPageTests and
ClassMngrNextClassesAnalyticsVisualCapture. The focused page CTest passed 1/1
in 9.26 seconds, and both selected Analytics QtTest cases exited 0. The final
offscreen paired capture exited 0 and produced 16 readable Legacy/Next images
at 1024x768 across four locale/theme pairs, each covering All and Winter for
selected class 1. All 16 source-input hashes match.

English and Korean labels are readable; no missing-glyph squares were seen.
The Legacy and Next shells differ, some Korean Legacy controls remain in
English, and the fixed viewport crops lower content. These are descriptive
observations only, with no pixel-parity or defect conclusion. The initial
FileTracker build attempt and default-QPA capture attempt are preserved as
environment diagnostics; the build retry passed with artifact-local temp,
and CTest/capture passed with isolated temp/profile paths and the registered
offscreen settings. No product or test source changed. Logs,
manifests, captures, review, and verification are in
artifacts/phase10/p10-win-c26-classes-analytics-20261126/.

### 2026-11-26 — Candidate 26 Windows x64 Classes Analytics discovery

Read-only discovery at source HEAD 53e5fb9f89f38ea3bb7be7ea41c650a454d60d87
selected the current paired Classes Analytics capture as the next bounded
Windows x64 follow-up. CMake registers
ClassMngrNextClassesAnalyticsVisualCapture and
ClassMngrNextClassesNavigatorPageTests. The tracked Phase 7 package covers
Legacy/Next All and Winter states for class 1 across four locale/theme pairs;
its different Legacy and Next shells do not support a pixel-parity claim. No
prior Phase 10 Windows x64 populated Analytics capture was found.

Proposed acceptance: build both targets as Windows x64 Debug, run only the
Navigator page CTest and review its two Analytics-specific cases, then run the
capture executable directly into fresh artifact storage. Validate the 16
1024x768 images, four locale/theme entries, selected class, All/Winter rows,
readable English and Korean labels, matching route context, and source hashes.
The current capture harness does not explicitly select or record an
application font, so glyph readability needs direct review. This is a
discovery only; no build, test, or capture ran. The report and 16 source hashes
are in artifacts/phase10/p10-win-c26-classes-analytics-discovery-20261126/.

### 2026-11-26 — Candidate 25 Windows x64 Class Transfer requalification

At source HEAD ad46ba419bc4b83c084ce6bdb83dcced2fabb66f, the Windows x64
Debug build exited 0 for ClassMngrNextClassTransferWorkflowTests and
ClassMngrNextClassTransferVisualCapture. The focused Next workflow CTest passed
1/1 in 46.35 seconds. The direct offscreen capture exited 0 and produced the
24 Legacy/Next images for export selection, import review, and conflict
feedback across en_US/ko_KR and light/dark. All images decode at 1024x768; all
ten source-input hashes match.

The final captures use packaged Inter at 11 pt for English and Pretendard
Variable at 11 pt for Korean. The initial missing-glyph run is preserved as a
diagnostic and superseded by the test-only font-registration correction in
ad46ba41. Review recorded readable labels, visible conflict feedback, clipped
Legacy export rows at the fixed viewport, and an elided Next Matches value.
These are observations only: no pixel-parity or native Windows UI claim is
made. The broad Legacy aggregate and full repository suite were not run; no
product source changed. Evidence is in
artifacts/phase10/p10-win-c25-class-transfer-20261126/.

### 2026-11-26 — Candidate 25 Windows x64 capture font harness correction

At source HEAD 0b4fd7c521d2bb93678970565c1cf3057dd132d1, the first Windows x64
offscreen Class Transfer capture target exited 0 and wrote 24 PNGs, but every
English and Korean label rendered as missing-glyph squares. The capture
harness had not registered the packaged fonts. This is a capture-environment
result, not a product defect; those images are preserved as diagnostics.

The test-only harness now registers packaged Inter-Regular and
PretendardVariable, selects the corresponding 11 pt application font by
locale, and records font family and size in each manifest locale/theme entry.
The capture target rebuilt successfully and a fresh validation capture exited
0. All 24 images decoded at 1024x768; visual review of English/light and
Korean/dark Legacy/Next export, review, and conflict states found readable
text. The workflow CTest passed 1/1 earlier in this run, before the capture-
only correction; its target/source was unchanged. The font-validation result
and superseded images are retained in
artifacts/phase10/p10-win-c25-class-transfer-20261126/.

This correction changes no product source and makes no pixel-parity claim.
Candidate 25 still needs a final current-source Windows x64 requalification
after the harness source is committed.

### 2026-11-26 — Candidate 25 Windows x64 next-slice discovery

Read-only discovery at source HEAD 2ad30f2cbf0aaf06d3ea325f3dd4da0489fa67c5
found no current-source Windows x64 Class Transfer capture artifact. The
historical Phase 7 package records 12 Legacy/Next state pairs (24 PNGs) for
export selection, import review, and schedule-conflict feedback across four
locale/theme pairs, but its README claims no Windows result and explicitly
makes no visual-parity claim.

The current source registers ClassMngrNextClassTransferWorkflowTests and
ClassMngrNextClassTransferVisualCapture. CTest lists both; the capture target
accepts a caller-provided output directory, so a follow-up can avoid its
registered fixed build-tree path. The capture uses the tracked 96-class
workspace fixture. The historical README also records one Legacy aggregate
failure whose origin is unknown; the proposed slice is limited to the Next
workflow test and paired capture.

Proposed bounded follow-up: build both current-source targets for Windows x64
Debug, run only the Next workflow CTest, capture into fresh artifact storage,
and review the 24 images, manifest, states, and hashes. No build, test, or
capture was run during discovery. The source inventory and discovery report
are in artifacts/phase10/p10-win-c25-class-transfer-discovery-20261126/.
Candidate 11's installed geometry/font task remains separate.

### 2026-11-26 — Candidate 24 Windows x64 Campus Directory and C46 states

At source HEAD 9c01f4e40dd37ee3579f205891bd4c852918c5ed, the three Debug x64
targets ClassMngrNextCampusDirectoryTests,
ClassMngrNextCampusDirectoryVisualCapture, and ClassMngrNextNavigationTests
built successfully. A direct confirmation build recorded exit code 0 using
Visual Studio 18 2026, MSBuild 18.12.3, CMake 4.4.2, and Qt 6.12.0 MSVC x64
Debug.

The focused ClassMngrNextCampusDirectoryTests CTest passed 1/1 with -C Debug.
The exact navigation slot
shellAdminMenuIsCapabilityGatedAndManageCampusesUsesNavigationGuard passed
with 3 passed, 0 failed, and 0 skipped. Its QtTest log includes a Qt font
directory warning from the installed Qt package.

The offscreen capture target exited 0 and produced 40 C41 route captures and
12 C46 Admin/authorized/read-only captures across en_US/ko_KR and light/dark.
All 52 PNGs decoded: 48 route images are 1024x768 and four Admin-menu strips
are 1024x49 or 1024x50. Review found the Manage Campuses menu item, the
authorized Save control, the read-only notice with editing controls hidden,
localized Korean labels, and the Bundang map/building images. These are
observations; Legacy keeps a dashboard while Next uses individual routes, so
no pixel-parity claim is made.

All nine source-input hashes match. User profile, temporary files, and
settings were isolated in the artifact directory. No installed app, native
Windows window, physical input, UIA, or native clipboard was exercised. The
first multi-config CTest invocation omitted -C Debug and was not run; the
configuration-specific rerun passed. A hidden build wrapper stayed open after
the target outputs; after confirming no active build/compiler processes, only
that wrapper was stopped, and a direct rebuild captured exit code 0. The full
report, manifests, logs, and hash inventory are in
artifacts/phase10/p10-win-c24-campus-directory-20261126/.

No product or test source changed. Candidate 24's scoped offscreen evidence is
recorded; native Windows interaction and pixel parity remain open.

### 2026-11-26 — Candidate 2 Windows x64 Edit/clipboard logic follow-up

At source HEAD `23f1976973ba899f8f9a4c4863e9f43b87d4b65f`, the Debug x64
`ClassMngrNextNavigationTests` target built successfully with Visual Studio 18
2026, MSBuild 18.12.3, CMake 4.4.2, and Qt 6.12.0. The focused
`shellEditActionsFollowFocusDispatchAndClipboard` slot passed with exit code 0
(3 QtTest phases passed, 0 failed, 0 skipped; 196 ms). The slot covers Edit
menu enabled states and shortcuts, action dispatch, copy/cut/paste, undo/redo,
and blocking cut/paste in read-only editors.

Qt loaded `qoffscreend.dll`; `qwindowsd.dll` was not loaded. The clipboard
assertions therefore used Qt's offscreen clipboard backend, and the Windows
user clipboard was not read or modified. This is focused action logic, not
native Windows clipboard, physical focus, UIA, or window evidence. The build
reconfigured its existing tree and regenerated 199 runtime resource records.
The accepted run, source hashes, toolchain, and logs are in the
[execution report](../../artifacts/phase10/p10-win-c2-edit-clipboard-offscreen-20261126/execution-report.md).
An initial wrapper attempt captured a passing QtTest transcript but not a
reliable child exit code; it is retained as diagnostic evidence, and the
accepted rerun records both exit code 0 and the passing totals. No product or
test source changed. Candidate 2 and Phase 10 remain open.

### 2026-11-26 — Windows x64 next-batch discovery

Read-only discovery at source HEAD `fe48d3484be80fbdc69ce9e299a1154192ed0fc6`
found that Candidate 6 already supplies the current-source-equivalent Windows
x64 Document Viewer state matrix: 16 Next and 16 Legacy captures, all four
locale/theme pairs, and Ready/error/closed/reopened states. Its C6 manifest
contains 35 source inputs; 34 C++/header files still match, and the sole
difference is unrelated edits elsewhere in `cmake/tests/next.cmake`. No
duplicate Candidate 23 capture run is needed for this Windows x64 scope.

The next visual candidate is Candidate 24, current-source Campus Directory
plus the C46 Admin menu, authorized, and read-only Information states. The
existing capture target writes both sets to a supplied output directory and
uses packaged Bundang data with a target-local Legacy resource alias. No
Campus Directory Windows x64 capture artifact was found. The selected follow-
up will build `ClassMngrNextCampusDirectoryTests` and
`ClassMngrNextCampusDirectoryVisualCapture`, run the focused CTest, and capture
40 C41 paired-route PNGs plus 12 C46 state PNGs into fresh artifact storage.
No builds or tests were run during discovery. Full details and source hashes
are in the [discovery report](../../artifacts/phase10/p10-win-next-batch-discovery-20261126/discovery-report.md).

Candidate 11's installed geometry/font comparison remains a separate open
Windows x64 task. Native interaction, Windows ARM64, macOS, and cross-host
Phase 10 requirements remain open.

### 2026-11-25 — Phase 10 start precheck passed

At the user's request, an independent local Tester reviewed the fresh
Windows x64 Release precheck at
artifacts/phase9/p9-19-exact-release-builds/p10-release-precheck-20261125-115431-f8d41043.
The receipt SHA-256 is
977cd992aeff4f85fb4226275fa61358823d2f449cd1d5873f314cf7a1254192.
Its 2,377-file source inventory SHA-256 is
4f792604d5d0cfdcf7581cf1e875291a60c810b232c7d2d083243aeaddb83f25, and
the current source matched. The verified toolchain was VS2022 x64/v143
14.44.35207 with Windows SDK 10.0.26100.0. Four focused CTests passed:
ClassMngrNextBootstrapTests, ClassMngrNextPageHostTests,
ClassMngrNextClassesNavigatorPageTests, and
ClassMngrNextClassesNavigatorVisualCapture. The executable SHA-256 is
669ee067ff9efe2995cd5f81f8071e169323b1da931742cb3ccda8e5ad7f257d; the
CTest log SHA-256 is
6a1110f231c803532b466a31de8c21360cd678e2aa605098ab53f063e95e4347.

This passing focused precheck authorizes Phase 10 to start. It is not Phase 9
final packaged Release evidence and does not complete Phase 9 or satisfy its
full Windows x64 Debug CTest. Those remaining Phase 9 items are deferred to
Phase 14 after Phase 13. Phase 10 parity work and exit requirements remain
unchanged. The repository-root build directory remains empty by user request.

The active Batch 1 discovery is in the [current slice-batch tracker](Current-Slice-Batches.md).
Its visual candidates are discovery targets, not verified current-source
defects; requalify with source-bound captures and record source, toolchain,
fixture, and raw artifacts.

### 2026-11-25 — Phase 10 paused for Linux switch

The user paused this session to switch to Linux. Phase 10 remains In Progress.
Batch 1 candidate 1, focused Linux verification of migrated Phase 7 behavior,
has not run. Resume on Linux with the standard `linux-gcc-debug` preset and
the 19 selected CTest executables listed in the canonical
[session handoff](../../agent_docs/latest_session_work.md). Do not rerun the
full Phase 7 suite. The current Windows environment had no local Linux runtime;
the temporary hosted-workflow fallback was removed, and no GitHub workflow
was triggered.

The focused Windows x64 Release precheck passed and authorizes Phase 10 start
only; it does not satisfy Phase 9's final Release gate. The Phase 9 transition
commit `95c2d4abf3ab03ff69e6871a7b10b6a93c0efd87` is pushed to
`origin/Qt-Rewrite`. Phase 9 remains In Progress and incomplete with all
remaining work deferred to Phase 14 after Phase 13 under the unchanged gates.

### 2026-11-25 — Linux continuation paused after Candidate 14

The user directed work to stop after the current candidate and asked for
candidate completion states. The [Batch 1 tracker](Current-Slice-Batches.md)
now marks Candidates 1–14 as having bounded Linux evidence, with Candidate 4
disposition still open and Candidate 11 partial; Candidates 15–20 are not
started. Phase 10 remains In Progress and its cross-platform exit gate remains
open. Candidate 15 is the next entry point on resume. No commit, push, or
remote workflow occurred.

- **Candidate 12 — My Classes:** Fresh source-bound Linux captures used the
  production fonts in four locale/theme pairs. Next grade tabs no longer elide
  Korean `E4`; the empty status label is hidden while empty and error messages
  remain visible; the combined header/content gap matches Legacy at 24 px. The
  selected tab begins at y=129 in all pairs. Focused page/capture checks passed
  and the independent review verified the captures. Existing typography,
  selector-density, schedule-text, and Korean room-label differences remain
  accepted; empty-state captures, forced tab overflow, and other hosts remain
  open. See the [capture report](../../artifacts/phase10/p10-b1-c12-linux-my-classes-20261125T104422Z-header-gap-fb5828ab/manifest.json)
  and [independent review](../../artifacts/phase10/p10-b1-c12-header-gap-final-independent-review-20261125T104803Z-fb5828ab/report.md).
- **Candidate 13 — Schedule display:** Fresh Linux captures and independent
  review covered the toolbar, all display modes, overflow, and Intensive text.
  Next now sizes the plain Testing assignment row from translated wrapped text
  and available width, and uses the catalog's translated Korean room prefix.
  Focused page/capture CTests passed 2/2. Extreme 256-character Regular and
  Testing labels clip in both routes; Next exposes their full text through
  display, accessibility, and tooltip data, and the compact-row behavior is
  accepted. Other hosts remain unreviewed. See the [final capture report](../../artifacts/phase10/p10-b1-c13-korean-room-prefix-20261125T121321Z-fb5828ab/report.md),
  [independent review](../../artifacts/phase10/p10-b1-c13-korean-room-prefix-independent-review-20261125T123000Z-fb5828ab/report.md),
  and [long-label review](../../artifacts/phase10/p10-b1-c13-wrap-stress-independent-review-20261125T124500Z-fb5828ab/report.md).
- **Candidate 14 — Schedule Import preview:** Fresh Linux Legacy/Next captures
  verified the compact Next preview still clips Meeting times at the initial
  horizontal position; full strings are now exposed through tooltip and
  accessibility roles. The deterministic first typed overlap now appears in
  localized status with both classes and time ranges; Apply remains disabled.
  The five locale catalogs and their Phase 4 inventory records were updated.
  App-state validation, Legacy import dialog, and Next integration CTests each
  passed 1/1, and an independent review verified the captures and inventory
  delta. Windows/macOS and live screen-reader/tooltip interaction remain open.
  See the [implementation report](../../artifacts/phase10/p10-b1-c14-preview-access-conflict-20261125T133418Z-fb5828ab/report.md)
  and [independent review](../../artifacts/phase10/p10-b1-c14-preview-access-conflict-independent-review-20261125T135300Z-fb5828ab/report.md).

Candidates 5–10 also have focused Linux evidence, with their platform-specific
follow-ups still open; Candidate 11 remains partial because a full installed
Next runtime was not staged. Candidate 4's page-break/product disposition was
unresolved at that point; its later user decision and Windows x64 evidence are
recorded below. At the time of that pause, Candidate 15 was the resume point;
Candidate 19 still triggers next-batch discovery.

### 2026-11-26 — Windows x64 follow-up through Candidate 5

Phase 10 remains In Progress. Candidates 1–5 now have Windows x64 evidence;
resume with Candidate 6 Windows x64. Candidates 6–10 and 12–14 have no
Windows x64 follow-up yet; Candidate 11 remains partial Linux evidence.

- **Candidate 1:** The exact selected 19 migrated-feature CTests passed 19/19
  in Windows x64 Debug (265.79s). Toolchain: VS18 2026, MSVC 19.52.37029,
  SDK 10.0.28000, Qt 6.12.0. A one-line `ClassMngrAboutPresentation` test link dependency was added at
  `cmake/tests/next.cmake:782`; no C2/C3 product implementation changed. See
  [CTest log](../../artifacts/phase10/p10-win-c1-c3-filetracker-20261125T153339Z-8dadae2b/ctest-c1-exact19-followup5.log)
  and [commands](../../artifacts/phase10/p10-win-c1-c3-filetracker-20261125T153339Z-8dadae2b/p10-win-c1-c3-commands-followup5.txt).
- **Candidates 2–3:** Windows x64 offscreen `ClassMngrNextNavigationTests`
  and `ClassMngrNextSpeakingEvaluationPageTests` passed 1/1 each and were
  independently accepted for tested behavior. Native interactive Windows
  focus, clipboard, and window behavior remain open. See the
  [Candidate 2 log](../../artifacts/phase10/p10-win-c1-c3-filetracker-20261125T153339Z-8dadae2b/ctest-c2-navigation-followup5.log)
  and [Candidate 3 log](../../artifacts/phase10/p10-win-c1-c3-filetracker-20261125T153339Z-8dadae2b/ctest-c3-speaking-redo-followup5.log).
- **Candidate 4:** The user selected the historical grouping as the product
  contract: Teachers 22–24 stay together on page 11 and Sub Notes starts on
  page 12. The shared pagination renderer and focused test were updated;
  `ClassMngrSubPrepPrintPdfTests` passed 1/1, and independent review accepted
  Windows x64. Refreshed C53 has 12 Sub Prep and 5 roster pages; all 17
  Legacy/Next rasters match at 150 DPI, the SQL oracle reports 750 students,
  30 class headers, and 0 bullets, and extracted Legacy/Next text matches
  (11,119 chars) with 30 class notes and 24 teacher notes. The older historical
  reference is `sourceBound=false`. The strict Next Sub Prep PDF diagnostic
  found 903 U+0009 TAB mappings and zero unmapped; that diagnostic is not a
  pass. The formal strict C53 audit passes for the Next roster PDF. Windows
  ARM64 and macOS remain open. See the [independent report](../../artifacts/phase10/p10-win-13-subprep-c53-repair-20261126T023203/report.md)
  and [test log](../../artifacts/phase10/p10-win-13-subprep-c53-repair-20261126T023203/subprep-test-final.log).
- **Candidate 5:** Schedule, Roster, and Speaking targets passed 7/7 with
  page, text, geometry, and raster checks; independent review accepted this
  Windows x64 matrix. Candidate 4's refreshed C53 is the Sub Prep row. Windows
  ARM64/macOS remain open. Cross-host comparison values and an LNK4075 warning
  are documented; two capture environment assignments were not fully
  recorded. See the [matrix report](../../artifacts/phase10/p10-win-c5-output-parity-20261125T170351Z/report.md).

For the Windows run, resource input byte drift was corrected only for guarded
HEAD/inventory-identical CRLF variants; all 238 Phase 4 inventory records pass.
A process-only TEMP/TMP override avoided MSBuild FileTracker denial; the Phase
4 verification evidence is [here](../../artifacts/phase10/p10-win-c1-c3-filetracker-20261125T153339Z-8dadae2b/phase4-verify-238-followup5.json).

### 2026-11-26 — Windows x64 follow-up through Candidate 18; Candidate 19 started

Phase 10 remains In Progress. Candidates 6–10 and 12–18 now have bounded
Windows x64 follow-up evidence; Candidate 11 remains partial because a full
installed Next runtime was not staged. These Windows captures and tests do not
close native interactive, Windows ARM64, or macOS requirements unless stated.

- **Candidate 6 — Native printing:** Independent review accepted bounded
  Windows x64 headless viewer and print-route evidence: two capture CTests and
  focused injected print slots passed. No native dialog, printer, print job,
  or physical output was exercised. See the [report](../../artifacts/phase10/p10-win-36-c6-headless-print-20261126/report.md).
- **Candidates 7–10 — PowerPoint, updater, file workflows, external launches:**
  Focused Windows x64 service/fallback tests passed 2/2 for Candidate 7, updater
  tests passed 2/2 for Candidate 8, and file-workflow tests passed 10/10 for
  Candidate 9. Candidate 10 verified injected URL/folder routes and a fake
  local-file opener; registered OS handlers, Office COM/PowerPoint rendering,
  native installer/folder UI, native pickers, and real browser/Explorer launches
  remain open. Evidence: [C7](../../artifacts/phase10/p10-win-c7-windows-x64-debug/report.md),
  [C8](../../artifacts/phase10/p10-win-22-updater-windows-x64-debug/report.md),
  [C9](../../artifacts/phase10/p10-win-25-file-workflow-followup/report.md),
  [C10](../../artifacts/phase10/p10-win-27-external-launch-windows-x64-debug/report.md)
  and its [local-file follow-up](../../artifacts/phase10/p10-win-27-external-launch-windows-x64-debug/follow-up.md).
- **Candidate 12 — My Classes:** Windows x64 populated, empty, and summary-read
  error capture CTests passed 3/3; the accepted populated captures stayed
  byte-identical to the prior Windows run. Forced grade-tab overflow remains
  open because unsupported grade labels collapse to “Other.” See the
  [state follow-up](../../artifacts/phase10/p10-win-29-my-classes-state-followup-20261126/report.md)
  and [populated capture report](../../artifacts/phase10/p10-win-29-my-classes-visual-20261126/report.md).
- **Candidate 13 — Schedule display:** A test-only precision correction to
  `plainTextEvidence` preserved all fit and containment assertions. The focused
  page and capture CTests passed 2/2; 38 Legacy/Next records were captured.
  Testing rows grow in Next to fit wrapped room text, while long Intensive text
  remains available to accessibility/tooltip roles. Pixel parity is not
  claimed. Independent review found the manifest's same-data field is
  hard-coded and a screenshot-order concern inconclusive; current source order
  did not establish a product defect. See the [report](../../artifacts/phase10/p10-win-13-schedule-harness-fix-20261126T065642/report.md).
- **Candidate 14 — Schedule Import preview:** Focused model/dialog/integration
  checks and both Legacy/Next capture CTests passed. The artifact set has 72
  PNGs across four locale/theme pairs; live tooltip and screen-reader behavior
  remain open. See the [report](../../artifacts/phase10/p10-b1-c14-preview-access-conflict-windows-x64-20261126T065044/report.md).
- **Candidate 15 — Teacher Profile:** The capture CTest and focused birthday
  clear/save/reopen slot passed; four page images were readable. Native popup,
  pointer, keyboard/IME, and accessibility behavior remain open. See the
  [report](../../artifacts/phase10/p10-win-15-teacher-profile-followup-20261126/report.md).
- **Candidate 16 — My Details and Signature:** The four-locale/theme capture
  CTest and three focused signature slots passed; all 16 images were reviewed
  and readable. Native picker, keyboard/IME, and screen-reader behavior remain
  open. See the [report](../../artifacts/phase10/p10-win-16-my-details-signature-20261126/report.md).
- **Candidate 17 — Staff Directory:** Independent review accepted the Windows
  x64 offscreen evidence: the unit CTest passed 1/1, four registered capture
  CTests passed, and four direct cases exited 0. All 16 PNGs were readable and
  all 67 hashes verified. Eight page-relative manifests measure Legacy table
  bottom Y=670 and Next Y=676 (+6 px), matching the historical direction; no
  pixel-parity threshold was applied. Native DPI/window behavior, ARM64, and
  macOS remain open. See the [review summary](../../artifacts/phase10/p10-win-37-staff-directory-20261126/review-summary.json)
  and [hash inventory](../../artifacts/phase10/p10-win-37-staff-directory-20261126/SHA256SUMS.json).
- **Candidate 18 — Upcoming Birthdays:** Independent review accepted the final
  focused CTest run (7/7), 16 readable populated/empty captures, four manifests,
  and 20 matching hashes. Legacy and Next PNGs are byte-identical per pair;
  Inter/Pretendard Variable resolution and Hangul support are recorded. Native
  display behavior, ARM64, and macOS remain open. See the [report](../../artifacts/phase10/p10-win-18-upcoming-birthdays-20261126T0852KST/report.md).

Candidate 19's next-batch discovery is complete, and its Windows x64 visual
follow-up is now in progress. The current harness
(`tests/next/my_workspace_visual_capture.cpp`) covers the initial embedded
Calendar view in all four locale/theme pairs but does not scroll to the lower
Upcoming section. Requalification will capture upper and lower Legacy/Next
views and record actual shell labels, including the reported Korean difference.
The focused CTests are `ClassMngrNextMyWorkspaceVisualCapture`,
`ClassMngrNextMyWorkspacePageTests`, and `ClassMngrNextCalendarPageTests`; no
Candidate 19 result is available yet.
Candidate 20's source mapping has also begun as read-only exploration; no
Windows Candidate 20 implementation or verification has started.

The beyond-20 scan found two further discovery targets, not confirmed defects:
the historical Initial Setup Wizard package has 14 Legacy/Next stage pairs but
omits English/dark and Korean/light ([README](../../docs/qt-rewrite/visual-baseline/next/phase-7/initial-setup-wizard/README.md));
the historical Document Catalog package contains Next states without a Legacy
pair ([README](../../docs/qt-rewrite/visual-baseline/next/phase-7/document-catalog-c42/README.md)).
Document Viewer and Campus Directory already have historical four-pair
Legacy/Next packages. Requalify all historical findings against current source.

### 2026-11-26 — Windows x64 follow-up through Candidate 21; Candidate 22 started

Phase 10 remains In Progress. Candidates 19–21 now have bounded, independently
accepted Windows x64 evidence. Candidate 22 is the next entry point. These
offscreen runs do not close Windows ARM64, macOS, or native-interaction gates.

- **Candidate 19 — My Workspace Calendar:** The focused page and Calendar page
  CTests passed in an earlier valid run; the capture CTest then failed because
  the Legacy scrollbar range changed during layout. After bounded settling and
  an 18 px heading alignment, the focused capture CTest passed 1/1. Independent
  review accepted all 21 PNGs and 134 hashes. The lower Upcoming heading and an
  event row are visible in all eight Legacy/Next locale/theme captures. Korean
  Legacy shell labels remain English while Next uses Korean labels; actual
  strings are recorded without a defect conclusion. See the
  [report](../../artifacts/phase10/p10-win-c19-calendar-20261126/report.md).
- **Candidate 20 — Classes states/localization:** Independent review accepted
  all 24 PNGs, 12 manifest entries across four locale/theme pairs, 37 hashes,
  and both focused CTests. The initial build failed in MSBuild FileTracker
  with E_ACCESSDENIED when TEMP/TMP pointed to the user temp directory; the
  artifact-local retry built both targets and passed both tests. The Next
  read-error diagnostic continues below the 800×600 capture edge while the
  localized status and Retry control remain visible. See the
  [review notes](../../artifacts/phase10/p10-win-c20-classes-20261126/review-notes.md).
- **Candidate 21 — Initial Setup Wizard locale/theme coverage:** Independent
  review accepted 60 final PNGs (28 Legacy/Next stage pairs and four extra Next
  personal-details captures), eight font records, 325 hashes, and five focused
  capture CTests. Review found the first Legacy captures had missing glyphs
  because the test bypassed production FontManager initialization. The
  capture-only harness now links, loads, and applies bundled Inter/Pretendard
  fonts per locale, asserts a resolved family and U+AC00 support, and restores
  the QApplication font. The final Korean text renders; pre-fix tofu images
  remain preserved as diagnostics, not accepted captures. See the
  [visual review](../../artifacts/phase10/p10-win-c21-initial-setup-wizard-20261126/visual-review.md).
- **Candidate 22 — Document Catalog paired visual review:** Current-source
  Windows follow-up is underway. The Legacy `Sidebar` accepts the same typed
  `DocumentCatalogProjection` used by Next; a sidebar-region pair is feasible.
  The historical package contains Next shell captures without a Legacy pair.
  Full-window Legacy parity is outside this mapped scope, and no result or
  product-defect conclusion is available yet. See the
  [historical package](../../docs/qt-rewrite/visual-baseline/next/phase-7/document-catalog-c42/README.md).

### 2026-11-26 — Candidate 22 accepted; supplemental Windows follow-ups closed

Candidate 22's current-source Windows x64 sidebar-region review was independently
accepted. Two focused CTests passed. The artifact set contains 12 PNGs and 24
verified hashes: four Next full-shell captures plus eight matched Legacy/Next
sidebar-tree images, 254×630, across en_US/ko_KR and light/dark. The manifest
records 7 folders and 30 documents, the same selected stable ID
`document_guides_lesson_planning`, and no loaded PDF body; the capture target
records zero `QPdfDocument` instances and zero document-body diagnostics.
Korean labels and the selected row are visible in both trees. At the fixed
254-pixel Next tree width, some long sibling labels elide, while the selected
label remains readable. This is a sidebar-region comparison and makes no
full-window Legacy parity claim. See the
[review notes](../../artifacts/phase10/p10-win-c22-document-catalog-20261126/review-notes.md).

With Candidate 22 accepted, the supplemental Windows x64 follow-ups for
Candidates 19–22 are closed. Phase 10 remains In Progress: Candidate 11 is
partial Linux evidence, and Windows ARM64, macOS, native-interaction, and
cross-host requirements remain open.

### 2026-11-26 — Candidate 11 Windows x64 installed-runtime follow-up started

Candidate 11's existing Linux evidence remains partial; its Windows x64
installed-runtime follow-up has started, and no runtime result is available
yet. The bounded plan uses a separate Release build tree, an artifact-local
install prefix, explicit components, and isolated settings. It excludes
Program Files installs and the installer target. Candidates 19–22 remain
independently accepted. Phase 10 stays In Progress with Windows ARM64, macOS,
native-interaction, and cross-host requirements open.

### 2026-11-26 — Candidate 11 Windows x64 limited pass; ARM64 cross-build started

Independent review accepted the bounded Windows x64 Release staging result.
The package manifest covered 406 paths; 338 package hashes and 192 direct
resource hashes were verified, and the catalog matched its fixture.
`ClassMngrNextResourceRootResolverTests` and
`ClassMngrNextPlatformSettingsManagerWindowGeometryPortTests` passed, and the
layout probe resolved the staged `resources` directory.
An isolated 15-second offscreen startup recorded a flushed
`application.exec.enter` event before the harness stopped that exact PID. The
368-entry artifact inventory is at
[report](../../artifacts/phase10/p10-win-c11-installed-runtime-20261126/report.md).

This is not full installed-runtime acceptance: the geometry CTest covers only
the settings adapter/key round trip; installed-app geometry across relaunches
and visual font/translation rendering were not tested. No native GUI, installer,
Program Files install, ARM64 runtime, or macOS was exercised. Candidate 11
remains partial overall because Linux evidence is partial. Its separate Windows
ARM64 Debug cross-build follow-up is now in progress; no build result is
available yet. Phase 10 remains In Progress with cross-host and native gates
open.

### 2026-11-26 — Candidate 11 ARM64 cross-build accepted; supplemental checks started

Independent review accepted Candidate 11's Windows ARM64 Debug cross-build as
cross-build-only. The standard `build/windows-arm64-debug` tree configured and
built `ClassMngrNext`, `ClassMngrNextResourceRootResolverTests`, and
`ClassMngrNextPlatformSettingsManagerWindowGeometryPortTests`. The x64 `dumpbin`
verifier confirmed all three executable headers as AA64/ARM64, and all 10
artifact checksums matched. No ARM64 CTests, application launch, or install was
performed. The qmake metadata probe was rejected by Windows before execution;
this was a host/tool execution rejection, not a source or test failure. See the
[cross-build report](../../artifacts/phase10/p10-win-arm64-crossbuild-20261126/report.md).

Candidate 9's supplemental Windows-QPA presenter cancellation test is now in
progress, with no result. Candidate 6's safe print-dialog feasibility map is
also in progress, with no result. The user confirmed that no native ARM64 or
macOS host is currently available. Candidate 11 remains partial overall, and
Phase 10's runtime, native-interaction, macOS, and cross-host gates remain open.

### 2026-11-26 — Candidate 9 Windows-QPA cancellation and Candidate 6 print feasibility

The selected existing Candidate 9 presenter cancellation slot passed under
`QT_QPA_PLATFORM=windows`: PID 6632 exited 0 in 307 ms, and the test asserted
cancellation with no selected path and no preference write; all 46 artifact
checksum entries were verified. Its focused build
attempt failed in ZERO_CHECK with MSB4018/FileTracker access denied before
compilation, so the pre-existing x64 Debug executable was used. This verifies
in-process Escape cancellation for the Qt widget dialog only; it does not cover
UI Automation, navigation, visual parity, or the Windows common-dialog backend.
See the [supplemental report](../../artifacts/phase10/p10-win-qt-dialog-qpa-20261126/report.md).

Read-only Candidate 6 mapping found that the existing cancellation slot returns
`QDialog::Rejected` directly and never opens `QPrintDialog`. The product
Windows “Use Windows Print Dialog” route enumerates/uses the system default
printer before showing `QPrintDialog`. No dialog or printer was touched. Actual
print dialogs, printers, jobs, and physical output remain unverified.

Candidate 11 remains partial overall: its Windows x64 installed-runtime result
is limited and its ARM64 result is cross-build-only. No native ARM64/macOS host
is currently available; runtime, native-interaction, macOS, and cross-host
Phase 10 gates remain open. Continue with remaining Windows-native interaction
and cross-host requirements.

### 2026-11-26 — Windows native follow-up handoff

Candidate 2's isolated `shellExitActionUsesCloseApprovalAndGeometryPersistence`
slot built and passed under `QT_QPA_PLATFORM=windows`. Independent review
confirmed `qwindowsd.dll`, the exact PID and slot, 12 matching compilation-unit
hashes, and the artifact inventory. The window sample came from a different
process, so it is not shell-HWND or UIA evidence; the clipboard slot was not
run. This is limited platform-backend evidence. See the
[report](../../artifacts/phase10/p10-win-c2-shell-exit-native-20261126/execution-report.md).

Candidate 11's staged Windows x64 Release package and ARM64 cross-build retain
their limited acceptance. Three artifact-isolated Windows UI attempts did not
complete the app-level geometry/font/translation checks. The first rejected
the provider-prefixed root AutomationId; source and the existing P9 helper
confirm the exact identity is `QApplication.classMngrNextApplicationShell`.
The corrected attempt confirmed that root and moved the window, but foreground
activation failed, so it produced no screenshot, UIA close, or saved geometry.
The final retry stopped before UIA enumeration when PowerShell rejected a
`pid` parameter due to its case-insensitive collision with automatic `$PID`.
It terminated only the exact launch PID. No geometry persistence or Korean
visual result is accepted. Evidence is under
`artifacts/phase10/p10-win-c11-installed-runtime-20261126/interactive-followup-20261126/`,
`interactive-followup-retry1-20261126/`, and
`interactive-followup-retry2-20261126/`.

Read-only maps found no safe current-host real OS action for Candidates 6–8
and 10: the Windows print route enumerates the user's printer, Office COM
activation does not isolate the Office profile, the Windows updater starts
the selected package detached, and external URL/folder routes lack exact
process identity for cleanup. Candidate 9's Windows product dialog policy
uses Qt widgets; its forced Native test backend is not the normal Windows
product route. No printer, Office app, installer, browser, or Explorer action
was performed. At the time of this Windows follow-up, the user had no native
ARM64 or macOS host available. Phase 10
remains In Progress; this Windows follow-up is handed off paused, with the
native-interaction and cross-host gates open.

### 2026-11-26 — Candidate 1 macOS focused subset verified

Independent verification accepted bounded macOS evidence for the exact
selected 19 CTests: 19/19 passed in 22.74 seconds on macOS 27.2.1 arm64 using
the standard universal `macos-clang-debug` preset (`arm64;x86_64`, Debug,
`BUILD_TESTING=ON`). CTest ran with the offscreen Qt platform. The test
executables and app were universal; x86_64 was compile-time verified only, and
the runtime execution was arm64. Full Phase 7 was not run. Non-fatal linker
warnings and CTest `QWARN` output remain.

The first build attempt stopped at the generated-resource manifest guard
because the existing generated tree was stale. Before retrying, the prior
190-file tree was preserved and all its hashes verified; the retry rebuilt
resources successfully and the selected tests passed. See the [macOS report](../../artifacts/phase10/p10-macos-c1-exact19-20261126T061233Z-6981b584/report.md).

This gives Candidate 1 bounded macOS evidence, not Phase 10 acceptance. Phase
10 remains In Progress and its cross-platform and native-interaction gates
remain open.

### 2026-11-26 — Candidate 2 macOS shell-command follow-up

A subsequent independent verification confirmed bounded current-source
offscreen evidence at `363f595cbb983abb2e4c05920fdd1aec7508808e`. Four
focused CTests passed:
Navigation 21/21, legacy About 3/3, legacy Exit 5/5, and legacy Edit 6/6.
The separate Next About modal/parent/nested-license slot passed 3/3. The
existing standard `macos-clang-debug` tree and universal (`arm64;x86_64`)
binaries were used (Debug, `BUILD_TESTING=ON`); runtime execution was arm64
only. No code or tests changed.

The isolated Cocoa Exit/geometry slot exited 134 (SIGABRT) before its
assertions because Qt reported “no screens available.” Independent review
classified this as an environment observation; it establishes no product
defect, and native Cocoa behavior remains open. See the [macOS artifact report](../../artifacts/phase10/p10-macos-c2-shell-commands-20261126T065213Z-363f595c/report.md).

Candidate 2 remains unaccepted, and Phase 10's cross-platform and
native-interaction gates remain open.

### 2026-11-26 — Candidate 3 macOS Speaking Evaluation redo

Independent verification accepted bounded current-source evidence at
`6d4e3c15c5f5580324afa83fdb93b91de691a153`. The standard
`macos-clang-debug` universal tree and executable (`arm64;x86_64`) were reused.
The incremental build regenerated resources but needed no compile or link
steps. The single offscreen `ClassMngrNextSpeakingEvaluationPageTests` CTest
passed all 41 QtTest slots, including
`matrixIsFixedSizeTracksExactDeltaValidationAndUndo`; QtTest simulated
`QKeySequence::Redo` and `Undo` dispatch and page-level undo/redo. Runtime was
arm64 only. Physical keyboard delivery, native Cocoa focus, and x86_64 runtime
remain unverified. Font/offscreen warnings remain; no production or test code
changed. See the [macOS report](../../artifacts/phase10/p10-macos-c3-speaking-redo-20261126T072332Z-6d4e3c15/report.md).

This is bounded offscreen evidence, not native parity or Phase 10 acceptance.
Phase 10 remains In Progress with cross-platform and native-interaction gates
open.

### 2026-11-26 — Candidate 4 macOS C53 current-source review

Subsequent independent review and source analysis accepted the macOS result
for the user-selected page grouping, visible 17-page parity, and SQL-backed
roster content at source HEAD
`9dcba15b414ad4ee22d8fc09678f04f0c6c68f1b`. The standard
`macos-clang-debug` tree was reused; all four built executables were universal
(`arm64;x86_64`), but runtime was arm64 only. No code or tests changed; no
printer, dialog, job, or physical output was used.

- **Tests and visible output:** `ClassMngrSubPrepPrintPdfTests` passed 25/25
  QtTest slots; the C53 startup capture passed 3/3. All 12 Sub Prep and 5
  roster Legacy/Next pages matched at 150 DPI with zero changed pixels.
  Page 11 keeps Teachers 22–24 and their class/teacher notes together; page 12
  starts Sub Notes. PDFKit text for those selected pages matches. This
  supports the selected grouping and visible 17-page result.
- **Roster data:** The SQL oracle reports 750 student names, 30 class headers,
  and 0 bullets. The Next roster strict used-code audit passes with no
  unmapped codes or control-code uses; generation left the prepared database
  unchanged.
- **Text-layer limits:** The checked-in runner sets
  `pdfToPdfTextComparison=false`; it does not assert full PDF text equality.
  Raw roster extraction differs: Legacy emits PUA punctuation while Next emits
  ASCII. A supplemental three-codepoint mapping outside the runner makes those
  full extracts match. Source analysis attributes the punctuation map to the
  Legacy roster finalizer, corrected by Next; the Sub Prep page 11 opening `(`
  remains U+E081 in both because the Next sheet path does not use that
  finalizer. The selected-page PDFKit match does not establish full text-layer
  or copy fidelity. Legacy audits include mapped tabs (903 Sub Prep, 1,640
  roster), so they are not strict non-control passes; this bundle has no
  strict Next Sub Prep audit. Candidate 5 did not close this text/copy gap; a
  source-aware text-layer follow-up remains needed before Phase 10 exit.

This is bounded macOS evidence, not full Candidate 5 output-matrix closure or
Phase 10 acceptance. Windows ARM64, x86_64 runtime, physical output, and the
broader cross-platform gates remain open. See the [macOS C53 report](../../artifacts/phase10/p10-macos-c4-c53-20261126T073111Z-9dcba15b/report.md).

### 2026-11-26 — Candidate 5 macOS output matrix

Independent verification accepted the scoped macOS output evidence at source
HEAD `75fd7d20739fad823bec7772abf72d0c81e0af59`. The standard universal
`macos-clang-debug` tree was reused; built binaries include `arm64;x86_64`,
but captured runtime evidence is arm64 only. No source or test changes were
made. No print dialog, printer, job, or physical output was used; Office and
automation were not launched.

- **Schedule C51:** `ClassMngrNextNormalScheduleImportIntegrationTests` passed
  18/18 QtTest slots. Legacy and Next each produced one landscape A4 page with
  profile `Alice` and courses E6-Helios, E6-Poseidon, E6-Gaia, and E6-Hera.
  At 150 DPI the same-host pair had zero changed pixels, within the existing
  tolerance. The archived Phase 7 Schedule image differs and requests an
  unavailable font;
  it is not a cross-host gate.
- **Roster:** `ClassMngrNextRosterOutputTests` passed 20/20 slots. By Day and
  Daily each produced one page; Per Class produced two portrait and two
  landscape pages. All six Legacy/Next page pairs matched in text, geometry,
  orientation, synthetic content, and raster.
- **Speaking:** The focused
  `legacyAndNextOutputPathsMatchForStandardAndAdvanced` slot passed 3/3,
  including setup/cleanup. Standard and Advanced each produced one 540×780 pt
  page with expected synthetic content, matching extracted text, and zero
  changed pixels at 150 DPI. Both adapter paths use a shared renderer, so this
  is not independent-renderer parity.

Candidate 4's C53 row was carried forward, not rerun: the selected grouping,
17-page raster, and SQL result remain accepted, while raw PDF text-layer/copy
fidelity remains open. Candidate 5 does not normalize that output or close the
full text matrix; see the [Candidate 4 review](../../artifacts/phase10/p10-macos-c4-c53-20261126T073111Z-9dcba15b/report.md).

This scoped macOS evidence is accepted; the full output matrix, Windows ARM64,
x86_64 runtime, and Phase 10 cross-host gates remain open. See the [macOS
report](../../artifacts/phase10/p10-macos-c5-output-matrix-20261126T075719Z-75fd7d20/report.md).

### 2026-11-26 — Candidate 6 macOS safe print-route checks

Independent verification accepted three bounded individual offscreen slots on
the standard `macos-clang-debug` tree at source HEAD
`eaf3b1415e27fdd578a232b9ab4d4c27cc8757b0`. The incremental build regenerated
resources but had no C++ compile or link steps. The executable is universal
(`arm64;x86_64`); runtime was arm64 only. The full registered CTest target was
inventoried but not run.

- `batchPrintValidatesBeforeDialogAndReleasesOnCancellation`
- `selectedDocumentPrintUsesInjectedBorrowedOperation`
- `zoomEntryButtonsAndFitModesUpdateThePdfView`

Each selected slot passed with zero failures, errors, or skips. The first
checks invalid-input rejection before the injected dialog hook and release
before cancellation; the second uses a recording fake print operation; the
third checks PDF-view zoom, fit, navigation, close, and reopen offscreen.

This does not verify the native print panel. The cancellation route constructs
`QPrinter::NativeFormat` and `QPrintDialog`, but the injected hook returns
`Rejected` without calling `exec()`. No explicit printer enumeration, panel
display, selection, or job submission was triggered; lower-level Qt
printer-service initialization or queries are not ruled out. Native dialog
geometry persistence after return, including cancel, printer selection,
system preview, real jobs, and physical output remain unverified. A prior
Cocoa window attempt stopped before window creation because no screen was
available; this run did not retry it. The preliminary `-help` probe without
offscreen QPA logged PasteBoard errors, while the three test processes used
offscreen QPA and had clean console output. No source, test, or documentation
changes were made.

Candidate 6 and Phase 10 remain open. See the [corrected macOS report](../../artifacts/phase10/p10-macos-c6-safe-print-route-20261126T081814Z-eaf3b141/report.md);
the bundle manifest SHA-256 is
`2835f16729ad93085dc1c225dfc61495310a5588e9d4ab42065b0b756d0932d4`.

### 2026-11-26 — Candidate 7 macOS safe PowerPoint fallback checks

Independent verification accepted bounded fake-only macOS evidence at source
HEAD `c8350ef74ac3a0bc3ac48513daf45ba6e9b4d994`, using the standard universal
`macos-clang-debug` tree. Built test binaries include `arm64;x86_64`; runtime
was arm64 only. The two registered protected-data notice CTests passed:
`ClassMngrPowerPointDataAccessNoticeTests` and
`ClassMngrNextPlatformSettingsManagerPowerPointDataAccessNoticePortTests`.
Three direct offscreen slots passed 13 synthetic data cases with
`CLASSMNGR_ENABLE_POWERPOINT_INTEGRATION_TESTS` unset:

- `SpeakingEvaluationPageTests::nextPowerPointUsesInjectedAutomationAndApplicationActions` (6 cases)
- `SpeakingEvaluationPageTests::nextPowerPointRequestFailsWhenAutomationPortIsMissing` (1 case)
- `SpeakingEvalBatchReportServiceTests::powerPointPortRejectsUnavailableAndIncompleteOutput` (6 cases)

The full Speaking Evaluation CTest target was not run. The real availability
slot was excluded because it checks `/Applications/Microsoft PowerPoint.app`
and locates `osascript`; neither path nor executable availability was
queried. No Office app, AppleScript, template, user Office state, or Automation
permission was touched. Fake cases use synthetic data and temporary paths;
`ClassMngrPowerPointProcessFixture` was built as a dependency but not executed.
Real PowerPoint installation/availability, activation, rendering, export, and
fallback against an actual unavailable installation remain open. No source,
test, or documentation files changed.

This does not establish Candidate 7 or Phase 10 acceptance. See the [macOS
report](../../artifacts/phase10/p10-macos-c7-powerpoint-safe-fallback-20261126T083606Z-c8350ef7/report.md);
the bundle manifest SHA-256 is
`7c3c6563500b5f04fa227c30b422f727dfe35ad30731a3e79cf5a09d085800ad`.

### 2026-11-26 — Candidate 8 macOS updater/coordinator checks

Independent review accepted bounded macOS evidence at source HEAD
`71f97551e792447349235ec7548501b18c0597b2`, using the standard universal
`macos-clang-debug` tree. Both test binaries have `arm64;x86_64` slices; all
runtime execution was arm64. In the initial restricted run,
`ClassMngrNextUpdateCoordinatorTests` passed but `ClassMngrUpdaterTests` failed
16 assertions when local `QTcpServer::listen()` returned false. The original
bundle preserves this result. The same two CTests were rerun with approved
escalation and unchanged binaries, without rebuilding; both passed in 0.94
seconds.

Independent comparison strongly resolves the first failure as
environment-limited. The artifacts do not prove a kernel-level privilege
boundary restricted to loopback. No package, Finder folder, external URL, or
macOS `open` action was invoked. Real macOS `open` and Finder reveal behavior
remain unverified. No x86_64 runtime was run; no source or test changes were
made.

This is bounded updater/coordinator evidence, not Candidate 8 or Phase 10
acceptance. See the [initial restricted run](../../artifacts/phase10/p10-macos-c8-updater-fake-actions-20261126T085306Z-71f97551/report.md)
and [approved retry](../../artifacts/phase10/p10-macos-c8-updater-loopback-retry-20261126T090057Z-71f97551/report.md).

### 2026-11-26 — Candidate 9 macOS file-dialog contract checks

Independent review accepted the hashes and provenance and verified the bounded
macOS policy inspection at source HEAD
`4a8c0ccf9d449cd9202b509add58de2aa78cf0dd`. Both targets were built
incrementally in the standard `macos-clang-debug` tree. The focused
`ClassMngrNextFileDialogContractTests` CTest passed 1/1; only
`forcedQtAndPlatformDefaultPolicyAreExplicit` and
`openOneUsesEscapeForCancellationWithoutWritingPreferences` were run, each
with zero failures. The two universal executables ran on arm64 only.

The policy inspection confirms the macOS default maps to the native-dialog
policy, but the selected presenter runs forced Qt mode; no native Cocoa panel
or user path was opened. This does not verify platform-native picker behavior.
The full presenter CTest and `ProfileFileWorkflowVisualCapture`, native
Cocoa behavior, x86_64 runtime, interactive navigation, and visual parity
remain open. No source, test, or documentation files changed. See the
[macOS report](../../artifacts/phase10/p10-macos-c9-file-dialog-20261126T090735Z-4a8c0ccf/report.md).

This is bounded current-source evidence, not Candidate 9 or Phase 10
acceptance.

### 2026-11-26 — Candidate 10 macOS injected external-launch checks

Independent review accepted the hashes, provenance, and exact test scope at
source HEAD `3b7d3446619f75619c021ddac6148a507d10bd70`. Five targets were built
incrementally in the standard universal `macos-clang-debug` tree. The
`ClassMngrNextUsefulLinksSelectionBehavior` CTest passed 1/1; it checks a
completed shell-startup stage, not external URL dispatch. Five selected
offscreen direct slots also passed with zero failures, errors, and skips:

- Bootstrap AI composition used a capturing HTTPS handler.
- Campus map links used an injected URL launcher.
- Document print used an injected borrowed operation.
- Speaking reports used a synthetic unsaved projection and batch-output paths.
- Sub Prep folder open used an injected callback.

Settings, home/XDG paths, temporary paths, workspaces, and generated files
were isolated. All five test executables are universal (`arm64;x86_64`), but
runtime was arm64 only. No default `QDesktopServices` dispatch, browser/Finder
process, folder reveal, native file/print panel, real user path, or persistent
product output was used. Actual macOS LaunchServices/browser/Finder behavior,
x86_64 runtime, full suites, and visual captures remain open. No source, test,
or tracked documentation changed.

This is bounded launch-seam evidence, not Candidate 10 or Phase 10 acceptance.
See the [macOS report](../../artifacts/phase10/p10-macos-c10-external-launches-20261126T092129Z-3b7d3446/report.md).

### 2026-11-26 — Candidate 11 macOS staged Release follow-up

Independent review accepted the focused checks and staged-package evidence at
source HEAD `4f4a3ee07c46e090970a1cc4f2f91832a763d763`. The two selected Debug
CTests, `ClassMngrNextResourceRootResolverTests` and
`ClassMngrNextPlatformSettingsManagerWindowGeometryPortTests`, passed, as did
the Bootstrap slot
`loadsOnlySelectedLanguageThemeAndRequiredFontsBeforeShell`.

The supported Release script produced a fresh staged, universal app and the
package verifier accepted 919 paths: 498 SHA-256 files, 192 direct resource
hashes, and 22 license/SBOM files. Strict deep code-signature verification
passed; minimum deployment target is 14.4. Historical
`build/macos-clang-release` and `dist/ClassMngr-macos` were preserved. The
release script then failed DMG creation with `hdiutil: Device not configured`;
no DMG or checksum was produced.

Direct staged-app smoke was attempted with offscreen QPA but aborted before
startup because the packaged Release app has Cocoa and no offscreen plugin.
No startup trace or runtime metrics were produced; the controller sent no
signal and did not force termination. This host/plugin limitation establishes
no product defect. The staged package was not installed into `/Applications`
or opened with Finder/open. No x86_64 runtime was executed. Candidate 11
remains open for DMG creation, successful app runtime, close/relaunch geometry,
window persistence, and native rendering; no geometry persistence or visual
result is accepted.

See the [macOS report](../../artifacts/phase10/p10-macos-c11-installed-runtime-20261126T095442Z-4f4a3ee0/report.md); its SHA-256 is
`bec76603fbcf52ed8f61e10510713b9bb2a0931b8373bbc387b26d7cfc2b0625`.

### 2026-11-26 — Candidate 12 macOS My Classes captures

Independent Tester verified the current-source evidence at source HEAD
`d27bfd043acc96a12ad4f28d2472ed322ec3a123`: all 53 hashes and provenance,
five selected page slots, three direct offscreen capture invocations, and 28
PNGs. The standard universal Debug targets were rebuilt/current; executables
contain `arm64;x86_64`, but runtime was arm64 only. The page slots and capture
commands all passed. The existing fixed capture directory was preserved with
its nine files and hashes unchanged.

The 28 images cover populated (8), empty (8), and summary-error (12) states
across four English/Korean and light/dark pairs; all are 1024×768 except the
Legacy warning-dialog captures. Observations include Next navigation buttons
in empty/error images where the corresponding Legacy page images lack them,
and Legacy modal warning versus Next inline error presentation. There is no
image comparator, so these observations establish neither parity nor a defect
or acceptance conclusion. Full details are in the [visual observations](../../artifacts/phase10/p10-macos-c12-my-classes-20261126T102031Z-d27bfd04/visual-observations.md).

Forced grade-tab overflow remains open; this run used four supported grades.
Native Cocoa rendering and x86_64 runtime remain open. No source or test files
changed. See the [macOS report](../../artifacts/phase10/p10-macos-c12-my-classes-20261126T102031Z-d27bfd04/report.md).

### 2026-11-26 — Candidate 13 macOS Schedule Display captures

Independent Tester verified the evidence, hashes, and provenance at source
HEAD `e670bdc0f509f6210f6071a55037e26ebe70ca30`. The standard universal Debug
page/capture targets built successfully; two focused page slots passed. Direct
offscreen capture produced 19 default Legacy/Next pairs and two stress pairs.
The existing fixed capture directory's 33 files and hashes were unchanged.
Built executables contain `arm64;x86_64`; runtime was arm64 only.

No pixel or content parity is claimed. The manifest's
`sameModeAndWorkspaceData=true` field is hard-coded, not a data comparator. Four
256-character Regular/Testing stress entries report
`documentFitsCellContent=false` in 60 px cells, with no fit assertion. This is
an unresolved overflow finding, not a passing fit check or product-defect
conclusion. The Windows screenshot-order question remains open; this macOS
bundle contains no Windows evidence. Native Cocoa/interactive behavior and
x86_64 runtime remain open. No source or test files changed. See the [report](../../artifacts/phase10/p10-macos-c13-schedule-display-20261126T103458Z-e670bdc0/report.md)
and [visual observations](../../artifacts/phase10/p10-macos-c13-schedule-display-20261126T103458Z-e670bdc0/visual-observations.md).

This is bounded macOS capture and focused-page evidence, not a Candidate 13
or Phase 10 acceptance conclusion.

### 2026-11-26 — Candidate 14 macOS Schedule Import preview

Independent Tester verified the current-source evidence at HEAD
`07d78ccc43fb7750bdb78e2188b7d2e6fa9b72c3` and tree
`8df141480d428490a44f45bd2ddf972eb11aa379`, including isolated inputs and all
captured hashes. Two targets built; the conflict slot and both Legacy/Next
capture slots passed offscreen. Captures contain 16 Legacy and 40 Next scenes
across English/Korean and light/dark. Universal binaries ran on arm64 only.

In the English/light Next small-review preview, the Meeting-times cell is
553 px wide but only 48 px intersects the 254 px viewport; 40 px remains for
text after padding. `DisplayRole`, `AccessibleTextRole`, and `ToolTipRole` hold
the full meeting-time strings. This is a geometry/model-role measurement, not
a fit assertion, parity result, defect finding, or acceptance conclusion.
There is no native picker, real hover, VoiceOver traversal, or user path in
this evidence. The two tracked large XLSX fixture hashes were omitted from
`source-fixtures.sha256`; HEAD, tree, and clean status are recorded. Live
tooltip/accessibility and native Cocoa behavior remain open. See the [report](../../artifacts/phase10/p10-macos-c14-schedule-import-20261126T105423Z-07d78ccc/report.md)
and [preview measurements](../../artifacts/phase10/p10-macos-c14-schedule-import-20261126T105423Z-07d78ccc/preview-measurements.md).

This remains bounded offscreen state/content evidence, not Candidate 14 or
Phase 10 acceptance. x86_64 runtime and native accessibility behavior remain
open.

### 2026-11-26 — Candidate 15 macOS Teacher Profile evidence

At source HEAD `3f3faf6af00bb9665ce80e6d11951f2381561aac`, the selected
`teacherProfileUsesTypedOpenAndStaticPageHistory` Bootstrap slot passed. The
capture target and dependencies built; `ClassMngrNextBootstrapTests` was
already current and was not recompiled. Both universal executables contain
`arm64;x86_64`; tests ran on arm64 only. The standalone capture produced four
1024×768 English/light and Korean/dark Legacy/Next images with a verified
manifest. The pre-existing fixed capture directory's hashes were unchanged.

The fixture shows March 14. Next displays a clear glyph; it is not
distinguishable in Legacy English/light and is subtle in Legacy Korean/dark.
This is a visual observation, not a parity or defect conclusion. The selected
test simulated Qt calendar selection and a clear-button click, then verified
save/reopen. The static capture did not open the birthday popup. Native Cocoa
popup placement, physical input, IME, accessibility, and Windows ARM64 remain
open. The fixed-path capture CTest was not run. No source, test, or tracked
documentation changed during the evidence run. See the [macOS report](../../artifacts/phase10/p10-macos-c15-teacher-profile-20261126T111138Z-3f3faf6a/report.md).

This is bounded macOS test/capture evidence, not a Candidate 15 or Phase 10
acceptance conclusion.

### 2026-11-26 — Candidate 16 macOS focused tests; capture matrix open

At source HEAD `5c79c863319ec3f53ae8016da5ab6fc97d8ee3c1`, three focused
signature slots passed on the universal macOS Debug executable under arm64:
`typedAndImageSignaturesPersistAndCanBeRemoved`,
`typedSignaturePreviewRendersTheSelectedTextAndFont`, and
`signaturePreviewIsReleasedOnSuspendAndReloadedOnActivation`. The standalone
capture target built, but two normal runs exited 139 after producing only the
four en_US/light images; neither wrote a manifest. The requested 16-image
matrix is incomplete. The fixed CTest output directory and hashes remained
unchanged.

Diagnostic crash reports localize the faulting caller to
`QApplication::setStyleSheet` at line 641 during the transition to en_US/dark;
they show arm64 `EXC_BAD_ACCESS` at `0x30`. The underlying cause is unknown
(the relevant Qt framework frames lack symbols). An escalated LLDB attempt
produced no backtrace and was stopped; no repair or source change was made, and
the crash is not a product-defect conclusion. A Rosetta probe returned “Bad
CPU type” before app launch, so no x86_64 runtime was obtained.

Candidate 16 and the visual matrix remain open. See the [main report](../../artifacts/phase10/p10-macos-c16-my-details-signature-20261126T112448Z-5c79c863/report.md),
[crash diagnostic](../../artifacts/phase10/p10-macos-c16-crash-diagnostic-20261126T114253Z-5c79c863/report.md),
and [Rosetta probe](../../artifacts/phase10/p10-macos-c16-rosetta-20261126T115517Z-5c79c863/report.md).
Their bundle manifest SHA-256 values are respectively
`4a3dc6e2d900ff1174df267df229b64a4d1da17fac6a4eaf03e458f9cdb64a06`,
`71579bf376628937bab50cca746962a17ed9750845166d41198ed92af21e9178`, and
`48394b106b7714cc3e4c9afa896fd63a50f6581de4791fbca224212d9f5b0e67`.

### 2026-11-26 — Candidate 17 macOS Shared Staff Directory captures

At source HEAD `870f2356f117b82f1c3a32eae44190d8151c4892`, the focused
`ClassMngrNextSharedStaffDirectoryTests` CTest passed 1/1. Four offscreen
Native-teacher/GS-team captures across English/light and Korean/dark produced
eight 1024×768 PNGs and four manifests. Each manifest records 120 fixture rows
and matched repository order. The standard universal executables contain
`arm64;x86_64`; build and captures ran on arm64 only.

All four scenes record a Legacy page-relative table bottom of 670 and Next
bottom of 676, with 38 px headers and representative 42 px rows; viewport and
scroll states are also recorded. These are descriptive measurements only:
there is no geometry or pixel threshold and no parity claim. Native DPI and
window behavior remain open. The existing fixed capture output was not
targeted, but no before/after hash inventory was made, so no byte-hash
preservation claim is recorded. No x86_64 runtime or Windows ARM64 result is
included; Candidate 17 and broader Phase 10 acceptance remain open. No source,
test, or tracked documentation changed. See the [macOS report](../../artifacts/phase10/p10-macos-c17-shared-staff-directory-20261126T115929Z-870f2356/report.md).

### 2026-11-26 — Candidate 18 macOS Upcoming Birthdays

At source HEAD `ded5b8e8c9fc9c1765f0eb0189a48df6a87e3b04`, the focused
`ClassMngrNextUpcomingBirthdaysFlowTests` CTest passed all seven flow slots.
The visual-capture target was rebuilt; the test target was already current.
Fresh offscreen en_US/dark and ko_KR/light runs produced eight 520×560 PNGs
and two manifests. Both executables are universal (`arm64;x86_64`); runtime
was arm64 only.

Populated manifests record one birthday today, one this week, and two next
week. Empty captures show localized messages, an unchecked dismissal checkbox,
and Close. Independent review accepted this scoped evidence and found all four
Legacy/Next image pairs byte-identical for these cases. It supplies no general
pixel comparator or parity claim. The existing fixed output and history were not
targeted, and no before/after hash inventory was made. No native Cocoa window
or popup was used; native DPI/window behavior remains open. No source or test
files changed.

This evidence does not accept Candidate 18 or Phase 10. See the [macOS
report](../../artifacts/phase10/p10-macos-c18-upcoming-birthdays-20261126T121421Z-ded5b8e8/report.md).

### 2026-11-26 — Candidate 19 macOS My Workspace Calendar

Independent review accepted the bounded current-source evidence at HEAD
`15c60e12aae50a4156a0e9a86153b13d9205a38e`. The focused
`ClassMngrNextMyWorkspacePageTests` CTest passed 1/1. The direct offscreen
capture produced 21 images across 13 scenarios; in all eight lower-Upcoming
records the heading and a representative event row are visible. The universal
executables ran on arm64 only.

Before/after hashes matched across 81 preserved build-output and historical
baseline files. In the Korean captures, Next shell labels are localized while
Legacy shell labels remain English; both routes show the heading
`예정된 일정`. These are captured strings only, with no fixed-localization,
defect, or acceptance conclusion. No native Cocoa window, pointer, or popup
was used. Native window/DPI behavior and x86_64 runtime remain open. No source,
test, or tracked documentation changed. See the [macOS report](../../artifacts/phase10/p10-macos-c19-my-workspace-calendar-20261126T122757Z-15c60e12/report.md).

### 2026-11-26 — Candidate 20 macOS Classes full-window states

At source HEAD `b77be502ee5f009e603465a6109fa2e30f1ce660`, the focused
`ClassMngrNextClassesNavigatorPageTests` CTest passed 1/1. A direct offscreen
capture produced 24 verified 800×600 PNGs in 12 Legacy/Next pairs across
empty, delete-confirmation, and list-read-error states, two locales, and two
themes. The independent review observed the Next Retry control and trailing
diagnostic clipped at the right edge; Legacy shows a read warning over My
Workspace/Schedule, while Next shows the Classes backdrop. These are
observations only: no pixel-parity, Retry-functionality, native AppKit,
defect, or acceptance conclusion.

Fixtures were temporary. Translation destination files were absent before
and after capture, with source catalog hashes preserved. The existing fixed
13-file CTest output inventory matched before and after. Universal binaries
ran on arm64 only. Native AppKit and runtime behavior remain unverified. No
source, test, or tracked documentation changed. See the [macOS report](../../artifacts/phase10/p10-macos-c20-classes-full-window-state-20261126T124035Z-b77be502/report.md)
and [capture images/manifest](../../artifacts/phase10/p10-macos-c20-classes-full-window-state-20261126T124035Z-b77be502/captures/matched-full-window-state-captures.json).

### 2026-11-27 — Candidate 38 Windows x64 Speaking Evaluation core

Candidate 38 is accepted for the bounded supplemental Windows x64 slice at
source commit `0cdfe7a0f251f037e6651f74de7234ce3164c6ef`. The standard
`build/windows-x64-debug` Debug target build succeeded. Registered CTest #332
`ClassMngrNextSpeakingEvaluationPageTests` passed these 11 selected slots, and
CTest #261 `NextStartupWorkspaceDependencyBoundary` passed separately:

- `matrixIsFixedSizeTracksExactDeltaValidationAndUndo`
- `rowPreferencesDisplayExtrasExtendAndRetainTailRows`
- `saveReopenPersistsUnicodeAndOnlyChangedCells`
- `largeNavigationUsesOneMatrixAndReleasesItOnSuspension`
- `splitterStateSavesOnSuspensionWithoutResettingRetainedPage`
- `selectionSaveDiscardAndFailureRetryPreserveDrafts`
- `saveOnSwitchPersistsBeforeLoadingAndFailureKeepsDraft`
- `automaticModeDebouncesAndSavesOnce`
- `rosterImportNotesAndValidationUseTypedRules`
- `rosterNameImportUsesMaximumBeyondExtraDisplayRows`
- `capturesEnglishKoreanLightAndDarkPresentation`

This was focused verification, not a full-suite run.

The capture fixture now registers packaged Inter and Pretendard fonts, applies
the production locale-specific font family ordering through
`FontSizePresenter`, checks Latin and Hangul glyph coverage, and restores
application font, locale, palette, stylesheet, and translator state. This was
a fixture-only correction; production code did not change. The first default
sandbox run could not access packaged generated font resources (`E_ACCESSDENIED`);
the reviewed-access build and tests passed after removing temporary tracing.
The final 1,769-input source/test hash bracket had no changes.

Independent review verified the four English/Korean light/dark PNG hashes and
legibility. They are synthetic Windows x64 offscreen presentation evidence,
not native-window, physical-printer, or pixel-parity evidence. All 29 shared
C39/C40 output files matched their saved baseline hashes after the capture.
See the [committed C38 captures](../../captures/next-c38-speaking-evaluation-core-20261127/visual-review.md)
and [run report](../../artifacts/phase10/p10-win-c38-speaking-evaluation-core-20261127/run-report.md)
for the selected slot names, commands, and detailed inventories. By user
direction, stop after Candidate 38; Candidate 39 is next when work resumes.
