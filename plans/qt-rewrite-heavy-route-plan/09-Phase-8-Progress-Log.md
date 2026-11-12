# Phase 8 — Progress Log

Read the [Phase 8 plan](09-Phase-8-Platform-and-Output-Adapters.md) for current status, contracts, work packages, and exit gate.

## Update policy

Append historical Phase 8 implementation and verification results, decisions,
blockers, acceptance findings, and exit-gate changes to this log. Do not put
historical progress updates in the [Phase 8 plan](09-Phase-8-Platform-and-Output-Adapters.md).
Keep current status, objectives, requirements, and contracts in that plan;
keep live batches in the shared tracker.

### 8.1.1 — External URL launch boundary (accepted)

Added the `ExternalUrlLaunchPort` application interface and its Qt
`QDesktopServices` adapter. Bootstrap now supplies it to shell navigation,
Speaking Evaluation, and Campus Directory. Those callers retain their current
URLs, HTTPS checks, localized failure behavior, and shell navigation behavior;
the platform adapter is the only direct `QDesktopServices` caller in the
assigned paths.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed for `ClassMngrNext`, `ClassMngrNextBootstrapTests`,
`ClassMngrNextSpeakingEvaluationPageTests`, and
`ClassMngrNextCampusDirectoryTests`. Focused tests passed for Campus Map URL
handoff/failure, AI URL handoff/failure, Bootstrap AI composition, and shell
navigation. Independent recheck confirmed Naver failure and Kakao success
handoffs; the Campus Map startup fixture has no URL, so those link assertions
use the injected action seam. No full suite was run.

### 8.1.2 — Speaking Evaluation clipboard adapter (accepted)

Added the write-only `ClipboardWritePort` and Qt clipboard adapter, injected
through Bootstrap into the Speaking Evaluation page and both AI dialogs. The
page and dialogs no longer write to the clipboard directly. Focused coverage
preserves copy-only dialog state, copy-before-open ordering, failure behavior,
batch copying of the displayed prompt, and Bootstrap adapter wiring.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build for `ClassMngrNextSpeakingEvaluationPageTests` and
`ClassMngrNextBootstrapTests` passed. Independent CTest verification passed
both targets (2/2); no full suite was run. `QClipboard::setText` has no result
value, so this port does not expose an operating-system clipboard-write
failure result.

### 8.1.3 — Schedule Import file-dialog adapter (accepted)

Bootstrap now supplies the existing `FileDialogPort` for Schedule Import
workbook selection, and the dialog no longer has a direct `QFileDialog`
fallback. The request retains the `ImportWorkbook` purpose, “Import Schedule”
title, `*.xlsx` filter, and remembered-directory behavior. Cancellation
preserves the current workbook selection.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed for `ClassMngrNextSchedulePageTests` and
`ClassMngrNextNormalScheduleImportIntegrationTests`. Independent focused runs
passed both filters (3/3 each), including the Bootstrap-selected workbook and
Gateway import, row-count checks, and unavailable-Gateway failure. The normal
headless verification uses Qt's offscreen platform. No full suite was run.

### 8.1.4 — Sub Prep output directory chooser (accepted)

Sub Prep now requests its output directory through the existing `FileDialogPort`
with `SubPrepPackage` purpose, “Select Folder” title, and the current field
value as the initial directory. A selection updates the field; cancellation
preserves it. Canceling the output dialog does not invoke package generation.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build and independent focused tests passed for `ClassMngrNextSubPrepPageTests`
and `ClassMngrNextBootstrapTests` (3/3 each). The page no longer calls
`QFileDialog`; the platform presenter was inspected but the real operating
system chooser was not opened. No full suite was run.

### 8.1.5 — Dialog geometry (accepted)

`DialogShell` now accepts a UI-owned, binary-safe geometry callback bundle.
Next composition supplies callbacks backed by the existing Application port
and SettingsManager platform adapter for Update, DocumentViewer printing,
Roster output, and both custom and native print dialogs. The shared shell no
longer includes Next Application or Platform types. Empty callbacks retain the
existing Legacy settings path; Next's RosterPrintDialog requires a complete
read/write pair and rejects missing or partial pairs, preventing accidental
fallback to Legacy storage. Stable dialog keys, first-show restore, close and
destruction persistence, and screen clamping are preserved.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental affected-
target Debug build passed in `build/macos-clang-debug`. Independent CTest
verification passed all five focused suites: DialogShell, Next DocumentViewer,
Next Roster Output, Next Update Coordinator, and the Next SettingsManager
geometry adapter (5/5). Coverage includes binary-safe callback round-trip,
Legacy geometry persistence, Next callback composition, and rejection of
missing or one-sided Roster callbacks. The native macOS print dialog was not
opened interactively; its callback path compiled and was inspected. No full
suite was run.

### 8.1.6 — Next main-window geometry (accepted)

Added a dedicated `WindowGeometryPort` and SettingsManager adapter using the
existing `ui/windowGeometry` key. Bootstrap supplies the port to
`ApplicationShell`, which restores the binary-safe Qt geometry after applying
its default and minimum size and before the first show. It saves only after
the close handler leaves the close event accepted; rejected closes do not
write. Missing or invalid geometry preserves the default bounds. The shell
contains no SettingsManager or Platform dependency.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed in `build/macos-clang-debug` for the affected production,
navigation, bootstrap, and geometry-adapter targets. Independent CTest
verification passed `ClassMngrNextNavigationTests`,
`ClassMngrNextBootstrapTests`, and
`ClassMngrNextPlatformSettingsManagerWindowGeometryPortTests` (3/3). Tests
cover pre-show restore, accepted-close persistence, rejected-close no-write,
empty/invalid payload defaults, and exact-key binary round-trip. The bootstrap
suite also ran its embedded visual-capture slots; dedicated visual-capture
targets compiled but were not run or visually reviewed. No full suite was run.

### 8.1.7 — Next splitter state (accepted)

Added a dedicated `SplitterStatePort` and SettingsManager adapter with four
isolated `ui/next/splitters/...` keys for the retained Classes Navigator,
Testing Classes Manager, Speaking Evaluation, and Class Transfer page
splitters. The transient Schedule Import review splitter remains unchanged.
Each page requires the port, restores its configured splitter state once, and
saves on suspension. Page reactivation retains the live splitter without
reapplying stored state. Missing or invalid payloads preserve the current
defaults; the adapter syncs writes so the final active-page save during
PageHost destruction is flushed.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed in `build/macos-clang-debug` for `ClassMngrNext`, the four focused
page targets, the settings-adapter target, and affected visual-capture and
Schedule targets. Independent CTest verification passed the four page suites
and `ClassMngrNextPlatformSettingsManagerSplitterStatePortTests` (5/5).
Coverage includes isolated keys, binary round-trip, absent/invalid defaults,
one-time restoration, suspension persistence, and retained-page reactivation.
The full suite was not run; visual captures were compiled but not visually
reviewed.

### 8.1.8 — Next application icon (accepted)

Next startup now loads the existing 256×256 PNG through `ResourceLoader` and
sets both the QApplication default icon and the main shell icon. The Next
macOS target builds as a bundle with the existing ICNS and matching
`MACOSX_BUNDLE_ICON_FILE` metadata. The Windows target receives a minimal RC
icon resource using the existing ICO, without Legacy version metadata or a
subsystem change. Installer and update-package production remain in 8.4.2.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed for `ClassMngrNext` and `ClassMngrNextBootstrapTests`; focused
CTest passed `ClassMngrNextApplicationIconTests` (1/1). The test compares
application and shell icons against the packaged PNG. The built app bundle's
Info.plist names `app_icon.icns`, and the bundled ICNS matches the source asset
byte-for-byte. Windows RC target wiring was inspected but not compiled on this
macOS host. No installer/package build or full suite was run.

The Bootstrap fixture's exact startup-resource expectation now includes the
required icon PNG. Its existing equality assertion remains unchanged; the
focused `generatedCatalogStartsWithOnlySelectedStartupPayloads` test passed.

### 8.1.9 — Navigation and Teacher Profile font-family lookup (accepted)

Added a Qt-free Application port for the ordered available font-family list
and a Qt Platform adapter that queries `QFontDatabase` on demand. Bootstrap
supplies the required port to Navigation and Teacher Profile. Matching policy
remains in the UI: Navigation uses the first exact case-insensitive Inter match
and preserves inherited fallback ordering; Teacher Profile uses the first
case-insensitive substring match for Inter/Pretendard and retains inherited
families when no match exists. Font sizes, styles, and widget assignments are
preserved. ResourceLoader's font registration is unchanged.

On macOS 27 ARM64 with Qt 6.12.0 and Apple Clang 21, the incremental Debug
build passed for `ClassMngrNext`, Navigation tests, Bootstrap tests, and the
Teacher Profile visual-capture target. Independent CTest verification passed
Navigation, Document Catalog Navigation, Document Viewer, Shared Staff
Directory, Application Icon, and the full `ClassMngrNextBootstrapTests`
target (6/6 targets; 89 Bootstrap cases passed). Deterministic coverage checks
exact versus substring matching, list order, deduplication, missing-family
fallback, Unicode conversion, all four Navigation sizes/styles, and the
intended Teacher Profile widgets. Offscreen English/light and Korean/dark
captures passed runtime assertions; two Next captures were visually reviewed.
The normal capture launch required offscreen mode because no display was
available. No repository-wide suite was run.

### 8.1.10 — Speaking Evaluation individual report Save As chooser (accepted)

The individual report Save As action now submits a `GeneratedPdf`
`SaveFileDialogRequest` through `FileDialogPort`. Bootstrap supplies the Qt
presenter and remembers the generated-PDF directory; tests can inject the same
port boundary. The request retains the safe suggested filename, PDF filter,
`pdf` suffix, and overwrite confirmation. Cancellation remains a no-op, the UI
still adds `.pdf` when a selected path has no suffix, and chooser errors or
directory-preference warnings are surfaced. The individual action no longer
falls back to direct `QFileDialog` use; the batch folder chooser remains for
8.1.11.

On the Linux x86_64 GCC Debug preset with Qt 6.12.0, the incremental build
passed for `ClassMngrNextSpeakingEvaluationPageTests` and
`ClassMngrNextBootstrapTests`. Focused CTest passed both registrations (89
Bootstrap cases and 40 Speaking Evaluation cases). Coverage verifies the real
Bootstrap composition request and cancellation, plus the dialog request
contract and extension behavior using a suffixless selected path. The clean
all-target Linux baseline remains pending in workflow `42146144532`; this is
focused evidence, not a full-suite result.

### 8.1.11 — Speaking Evaluation batch folder and open-folder paths (accepted)

The report batch folder chooser now sends a `GeneratedPdf`
`SelectDirectoryDialogRequest` through the same `FileDialogPort` seam used by
individual Save As. Cancellation leaves the folder unchanged, and selection
or preference errors remain visible in the dialog status. After a successful
Save As run, the optional “Open Output Folder” action now calls Bootstrap's
`ExternalUrlLaunchPort` adapter with a local-file URL. The callback remains
after successful output and before the dialog accepts; failure and cancellation
do not launch the folder. Direct `QFileDialog` and `QDesktopServices` fallbacks
have been removed from the Speaking Evaluation report dialog.

On the Linux x86_64 GCC Debug preset with Qt 6.12.0, the incremental build
passed for `ClassMngrNextSpeakingEvaluationPageTests` and
`ClassMngrNextBootstrapTests`. Focused CTest passed both registrations (89
Bootstrap cases and 40 Speaking Evaluation cases). Coverage verifies the
Bootstrap directory request, folder selection through the port, and
post-success open-folder callback ordering. The clean all-target Linux
baseline remains pending in workflow `42146144532`; this is focused evidence,
not a full-suite result.

### 8.1.12 — Document Viewer Save Copy chooser and open action (accepted)

Document Viewer Save Copy now sends a `General` `SaveFileDialogRequest`
through Bootstrap's `FileDialogPort`, preserving the document-derived
suggested filename, initial Documents directory, type filter, default suffix,
overwrite confirmation, and “Open after saving” option. Cancellation remains
a no-op, and a selected path without a suffix receives the document's suffix
before the atomic copy. Opening a saved copy now calls Bootstrap's
`ExternalUrlLaunchPort`; copy failures and open failures retain their existing
warning behavior. Direct `QFileDialog` and `QDesktopServices` fallbacks are
removed from the viewer.

On the Linux x86_64 GCC Debug preset with Qt 6.12.0, the incremental build
passed for `ClassMngrNextDocumentViewerTests` and
`ClassMngrNextBootstrapTests`. Focused CTest passed both registrations (17
Document Viewer cases and 89 Bootstrap cases). Coverage verifies the real
Bootstrap request and cancellation, typed request details, suffix application,
optional launch, and the open-failure warning. The broader clean Linux
baseline workflow `42146144532` is being requalified after its prior pushed
head failed on stale calendar test accessors; this is focused evidence, not a
full-suite result.

### 8.2.1 — Selected-document QtPdf session (accepted)

`QtPdfViewerSession` now owns the active `QPdfDocument`, `QPdfView`, and
borrowed source resource. `DocumentViewerPage` retains application session
state and controls. Replacement, close, suspension, page release, malformed
PDF handling, stale status suppression, close-timeout teardown, and reopen
are covered. Generated and print-output PDFs remain outside this viewer
session and continue through their operation paths.

On Darwin 27 arm64 in the Qt offscreen Debug configuration, the incremental
build in `build/macos-clang-debug` passed for five targets:
`ClassMngrNextDocumentCatalogNavigationTests`,
`ClassMngrNextDocumentViewerTests`,
`ClassMngrNextDocumentViewerVisualCapture`,
`ClassMngrNextPageHostTests`, and
`ClassMngrNextPlatformQtPdfSessionReleaseTests`. Independent CTest
verification passed all six focused checks: Document Viewer, QtPdf session
owner, visual capture, startup/workspace dependency boundary, PageHost, and
catalog navigation. The requested `ClassMngrNextDependencyBoundaryTests`
name is not registered; `NextStartupWorkspaceDependencyBoundary` was verified
with `ctest -N -V` and used. Ninja reported `premature end of file; recovering`
and completed with exit code 0 without compile or link failures. Visual
captures were generated and English/light Ready and Korean/dark Closed states
were reviewed; no pixel-parity claim against the older baseline was made.

The synchronous print borrow rejects reentrant replacement or release; its
owner/page must remain alive until the callback returns. The production path
uses a modal print dialog, so destruction from inside that callback is outside
the supported contract. The native macOS print dialog was not opened, and no
repository-wide suite was run.

### 8.2.2 — PDF/print operation adapter (accepted)

Added the Qt Platform `PdfPrintOperation` with separate borrowed-ready-document
and ordered local-batch operations. `DocumentViewerPage` invokes it inside the
viewer session's synchronous borrow; the viewer remains sole owner of its active
document and stream. The adapter delegates to shared Qt printing mechanics.
Shared batch printing validates and releases each document before the dialog,
stores page counts, determines orientation from the selected range, then
reopens and renders documents serially into one print job. Schedule, Roster,
Sub Prep, and Speaking Evaluation migrations remain in 8.2.3–8.2.6.

Focused verification passed the incremental build
`cmake --build build/macos-clang-debug --target ClassMngrNext ClassMngrQtPdfPrint ClassMngrNextDocumentViewerTests -j 8`, despite Ninja reporting
`premature end of file; recovering` (exit 0, no compile or link failures).
`ctest --test-dir build/macos-clang-debug -R '^(ClassMngrNextDocumentViewerTests|ClassMngrNextPlatformQtPdfSessionReleaseTests|NextStartupWorkspaceDependencyBoundary)$' --output-on-failure`
passed 3/3; `QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1
./build/macos-clang-debug/ClassMngrNextDocumentViewerTests -v1` passed 17/17.
`git diff --check` passed. Coverage verifies source order, global page ranges,
orientation, release before dialog, at most one ready document, PDF output
reload, pre-dialog missing/corrupt errors, and cancellation cleanup. No full
suite was run.

The production success path of the new Qt adapter was source-reviewed but not
runtime-driven because it opens printer UI. No native macOS print dialog,
physical print, or Linux/Windows runtime was exercised. Page-image lifetime was
code-inspected, not instrumented. Batch inputs are reopened after the modal
dialog, so changed contents with the same page count could print, and deleted
or inaccessible inputs can fail after acceptance; current callers use
operation-scoped temporary outputs.

### 8.2.3 — Schedule output (accepted)

Extracted the existing schedule PDF layout and writer into
`SchedulePdfRenderer`. Legacy `SchedulePrintService` remains the compatibility
wrapper with its existing print flow; Next Schedule output calls the renderer
directly and uses 8.2.2 `PdfPrintOperation` for printing. The Next target no
longer links `ClassMngrSchedulePrint`, and the dependency-boundary CTest passed.
Request, projection, dialogs, style/theme/font/orientation/path semantics are
preserved. Save As does not invoke a printer. Print creates a temporary PDF
and synchronously borrows a stack-owned ready document to the print operation,
releasing it when the operation returns.

The independent Tester incrementally built `ClassMngrSchedulePdfRenderer`,
`ClassMngrSchedulePrintPdfTests`, `ClassMngrNext`,
`ClassMngrNextScheduleOutputAdapterTests`,
`ClassMngrNextNormalScheduleImportIntegrationTests`, and
`ClassMngrNextSchedulePageTests` in `build/macos-clang-debug` with `-j4`
(exit 0). Focused CTest passed 5/5 for those four test suites and
`NextStartupWorkspaceDependencyBoundary`. QtTest counts were Legacy PDF 10,
Schedule page 13, adapter 5, and integration 18. `git diff --check` passed;
no full suite was run.

On the controlled macOS/Qt 6.12 fallback-font host, production Next Save As
matched the frozen `legacy-page-1.png` raster exactly at 1754×1240: zero
changed pixels and zero mean absolute channel difference (SHA-256
`eff64821b32270a3d9803f79ef2db869f6a63d3729f080ae2fd6406d6a53c175`). Fresh
PDFs each loaded as one A4 landscape page (842×595 points) and contained Alice
and E6-Helios/Poseidon/Gaia/Hera; their bytes differ from the archived PDF.
This establishes fixture raster parity, not PDF-byte identity or all-platform
parity. `Sans Serif` was unavailable and the host fallback font was used. No
native print dialog, physical print, Windows/Linux runtime, or full repository
suite was exercised.

### 8.2.4 — Roster output (accepted)

Roster Save As now sends a `GeneratedPdf` request through the existing
`FileDialogPort`-shaped boundary and writes directly through the atomic
`savePdf` adapter. Print uses the injected `PdfPrintOperation` with a nonempty
Ready borrowed PDF, preserving the title, options, and orientation. The
temporary PDF and document session are local to Print and retained only through
Preview; Preview Save and Print reuse that same PDF. Direct `QFileDialog` and
`PdfPrintService` fallbacks are absent. The existing truncation policy and all
four Next/Legacy template parity cases remain.

Verification used a fresh configure in the standard
`build/macos-clang-debug` folder on macOS with AppleClang 21, Ninja, Qt 6.12,
Debug, arm64+x86_64, deployment target 14.4, and `BUILD_TESTING=ON`.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextRosterOutputTests --parallel 4
```

The build exited 0. Focused CTest passed 2/2:

```sh
ctest --test-dir build/macos-clang-debug -R 'ClassMngrNextRosterOutputTests|NextStartupWorkspaceDependencyBoundary' --output-on-failure
```

The verbose Roster suite passed 18 tests with none failed or skipped, and all
four template parity rows passed. Offscreen Qt emitted the expected size-hint
and `Sans Serif` fallback warnings. Native printer UI and physical printing
were not exercised.

### 8.2.5 — Sub Prep output (accepted)

The page query result now lives only through mapping, and the mapped request
is retained through Sub Prep PDF rendering. Next supplies a neutral package
callback that Bootstrap maps to ordered `PdfPrintOperation` and
`ExternalUrlLaunchPort` operations. The shared service no longer references
Next Platform; Legacy direct output behavior remains.

The standard `build/macos-clang-debug` build passed for
`ClassMngrSubPrepPackageServiceTests`,
`ClassMngrNextSubPrepPageTests`, `ClassMngrStartupPerformanceTests`, and
`ClassMngrNextSubPrepSqlParityCapture` (exit 0; duplicate-library linker
warnings only). Focused CTest passed 3/3. Legacy
`capturesMatchedSqlSubPrepForC53` passed 3/3 in 11.573s against the SQL oracle
of 750 students and 30 headers. The Next matched-SQL capture passed for 12
Sub Prep and 5 roster pages (17 total), with matching dimensions, zero changed
pixels, and SQL text/font/hash gates passing. The strict Next audit covered 5
pages and 15 page-font records with zero unmapped or control codes; the Legacy
audit diagnostic showed expected control-code mappings. C53 artifacts were
written under dedicated `/private/tmp`; no PDF byte-identity claim is made.

No memory lifecycle verification was run. Separate Sub Prep memory acceptance
remains open under the memory plan.

### 8.2.6 — Speaking Evaluation output (accepted)

The shared print callback now accepts the parent widget, ordered staged-file
paths, and a singular/plural title. An empty callback preserves Legacy's
`PdfPrintService` path. Next installs a `PdfPrintOperation` callback, maps
Sent/Canceled/Failed results, and uses portrait, A4, and fit-to-page settings.
Staged files remain available through synchronous printing and are then
cleaned up. The bounded 100-report route is preserved.

In the standard macOS Debug configuration, builds passed for
`ClassMngrSpeakingEvalBatchReportServiceTests`,
`ClassMngrNextSpeakingEvaluationPageTests`, and
`ClassMngrNextBootstrapTests` (exit 0). Focused CTest, including
`NextStartupWorkspaceDependencyBoundary`, passed 4/4. QtTest results were 84
service cases passed with 5 gated PowerPoint skips, 33 Next page cases passed,
and 89 Bootstrap cases passed. Standard and advanced PDF-widget raster
comparisons and the maximum-row test passed. Duplicate-library, offscreen, and
font warnings were the only reported warnings. No native printer UI or
physical printing was exercised, and no Next/Legacy report-output parity
claim was measured.

The Save As chooser, output-folder chooser, and desktop folder-open paths
remain unchanged as separate dialog/URL integration work for later discovery.

### 8.2.7 — Schedule generated-output parity matrix (implementation ready; platform CI pending)

The C51 Schedule output evidence capture is enabled in the Refactoring
baseline CTest run on Linux x64, Windows x64, and macOS universal.
Each runner captures the production Legacy and Next PDFs, expected text,
landscape A4 geometry, 150-DPI first-page renders, and host/Qt metadata. The
archived Phase 7 macOS raster remains a measured reference; it is not compared
with a cross-platform pixel threshold because font fallback and QtPdf
rasterization vary by host.

The same-host Legacy/Next raster check allows at most 0.1% changed pixels and
0.5 mean absolute RGB channel difference on the 0–255 scale. The focused Linux
x86_64 GCC Debug build of `ClassMngrNextNormalScheduleImportIntegrationTests`
passed, and focused CTest passed 1/1 (4.66 seconds) on Fedora Linux 44 with
Qt 6.12.0. Both PDFs contained Alice and all four E6 course labels, had one
842×595-point landscape page, and rendered to 1754×1240 pixels with zero
changed pixels and zero mean channel difference. Against the frozen macOS
image, Linux recorded 140,929 changed pixels and 9.4227 mean channel
difference; these values are captured for diagnosis, not gated.

The Windows x64 and macOS universal captures will run after publication of the
active batch. Their artifacts are retained in the Refactoring baseline
workflow. See [Schedule output parity matrix](../../docs/qt-rewrite/visual-baseline/next/phase-8/schedule-output-parity/README.md)
for reproduction and tolerance details.

### 8.2.8 — Roster generated-output parity matrix (implementation ready; platform CI pending)

The existing four-template `templateParity` cases now retain paired Legacy and
Next PDFs, per-page rasters, and JSON metadata whenever the Refactoring
baseline workflow sets `CLASSMNGR_ROSTER_OUTPUT_PARITY_DIR`. It captures By
Day landscape, Daily portrait, and Per Class portrait/landscape cases on Linux
x64, Windows x64, and macOS universal. Each case verifies exact page text,
orientation, point geometry, and 1000×1000 QtPdf raster equality; requested
font families, host/Qt versions, PDF hashes, and rendered page text are
recorded. Raster images are compared within each host, not between operating
systems.

The Linux x86_64 GCC Debug build of `ClassMngrNextRosterOutputTests` passed
incrementally after using the `ClassMngrLegacyRuntimeClosure` and rescanning
the test target's Legacy class-transfer/domain archive cycle. Focused CTest
passed 1/1 (3.87 seconds) on Fedora Linux 44 with Qt 6.12.0. All four cases
passed and emitted six paired page rasters; every page's Legacy and Next
images had equal hashes, text, and geometry. The Windows x64 and macOS
universal captures will be collected after the active batch push. See
[Roster output parity matrix](../../docs/qt-rewrite/visual-baseline/next/phase-8/roster-output-parity/README.md)
for evidence structure and limitations.

### 8.2.9 — Sub Prep generated-output parity matrix (Linux evidence complete; platform matrix pending)

The Refactoring baseline workflow now runs the matched-SQL C53 Legacy/Next
capture, independent SQL oracle, and used-code audits on Linux x64, Windows
x64, and macOS universal. It retains the prepared database, paired package
PDFs, extracted page text, 150-DPI page rasters, comparison manifests, and
font/code audits in each platform artifact. All 12 Sub Prep pages and 5 By Day
roster pages must match Legacy page counts, dimensions, and pixels on the same
host. Raster equality is not compared across operating systems.

The required Linux Debug targets built in `build/linux-gcc-debug`, and the
candidate-2 capture passed all 17 pages with zero changed pixels. The SQL
oracle verified 750 unique weekday roster names and 30 class headers; the
strict Next used-code audit passed 5 pages and 15 page-font records. Qt Core
6.12 with QtPdf 6.14 is the valid Qt 6.12 release composition for this Linux
evidence. Windows x64 and
macOS universal captures, plus hosted exact-toolchain results, remain pending.
See [Sub Prep C53 output parity by platform]
(../../docs/qt-rewrite/visual-baseline/next/phase-8/sub-prep-c53-parity/README.md)
for artifact contents and reproduction.

### 8.2.10 — Speaking Evaluation report-output baseline (Linux evidence complete; platform matrix pending)

The Refactoring baseline workflow opts the existing Speaking Evaluation
report-service test into a matched-data capture on Linux x64, Windows x64, and
macOS universal. Standard and Advanced synthetic reports pass through the
Legacy `SpeakingEvalBatchReportService::exportReports()` API and the Next
`Output::runSpeakingEvaluationReports()` adapter. Each path must save one PDF
with the expected extracted text and 540 × 780-point page geometry; the two
same-host 150-DPI renders must be pixel-identical. The PDFs, PNGs, and JSON
manifest are uploaded per platform. This demonstrates adapter-path behavior
for matched inputs: both paths use `exportResolvedReports()`,
`SpeakingEvalInternalPdfRenderer`, and `SpeakingEvalReportPainter`, so it does
not claim independent renderer or layout parity. PDF byte identity and
cross-host raster identity are not gates.

The Linux Debug `ClassMngrSpeakingEvalBatchReportServiceTests` target built
successfully. Independent verification passed the focused Standard/Advanced
capture (3 passed, 0 failed, 0 skipped), confirmed equal extracted text,
540 × 780-point geometry, and pixel-identical 1125 × 1625 rasters for both
templates, and verified that an unset capture variable skips without writing a
manifest and a nonempty output directory fails without replacing its contents.
The manifest must be fully written and flushed before the test passes. Qt Core
6.12 with QtPdf 6.14 is the valid Qt 6.12 release composition for this Linux
evidence. Windows x64, macOS universal, and hosted exact-toolchain captures
remain pending. See [Speaking Evaluation
report output by adapter path](../../docs/qt-rewrite/visual-baseline/next/phase-8/speaking-evaluation-output-parity/README.md)
for capture details and reproduction.

### 8.3.1 — PowerPoint automation adapter (accepted)

The shared report engine now depends on a neutral PowerPoint automation port.
The Qt host adapter owns availability checks, process/workspace handling,
script execution, and cleanup; Legacy's wrapper and Next Bootstrap compose it,
and Next injects the port. macOS build repairs moved the workspace constructor
out of line for incomplete `QTemporaryDir` and made pure script generators
compile on all hosts without changing script contents.

The focused build passed for `ClassMngrSpeakingEvalBatchReportServiceTests`,
`ClassMngrNextSpeakingEvaluationPageTests`,
`ClassMngrNextBootstrapTests`, and `ClassMngrPowerPointProcessFixture` using
`-j4`. Focused CTest passed 5/5 in 38.69 seconds: service 109 passed, 0
failed, 5 skipped; mixed-template 3/0/0; Bootstrap 89/0/0; Next page 40/0/0;
dependency boundary 1/1. Four Office integration cases and one generated-assets
case were skipped because the integration gate was unset. The standalone
process fixture was up to date; runner behavior cases ran through the Legacy
service suite. Native PowerPoint automation was not exercised.

### 8.4.1 — Update operations ports (accepted)

Added Qt-free Next Application contracts for download events and cleanup,
package actions, and the application artifact value. Qt adapters wrap the
existing downloader and installer, preserving hash/size validation,
resume/reuse, cleanup, and process actions. The shared dialog no longer
references `UpdateDownloader`, `UpdateInstaller`, or `Q_OS_LINUX`; Next
`UpdateCoordinator` and Legacy `UpdateController` inject the ports, including
cleanup policy and current artifact. `ClassMngrUpdateOperations` owns
downloader/installer sources, `ClassMngrNextUpdateAdapters` links it directly,
and `ClassMngrUpdatePresentation` owns only dialog sources.

The final build passed for `ClassMngrUpdateOperations`,
`ClassMngrNextUpdateAdapters`, `ClassMngrUpdatePresentation`, `ClassMngr`,
`ClassMngrNext`, `ClassMngrNextBootstrapTests`,
`ClassMngrNextUpdateCoordinatorTests`, `ClassMngrUpdaterTests`, and
`ClassMngrUpdateControllerAutomaticStartupTests`. Focused CTest passed 4/4 in
24.01s: Bootstrap, startup/workspace dependency boundary, Next coordinator,
and shared update presentation. The boundary report confirms the direct
Update Operations dependency and unique source ownership. The download-failure
fake-port recheck passed 3 QtTest cases and coordinator CTest 1/1. Offline
selected updater cases passed 15/15; selected Legacy controller port/data
cases passed 11/11; unconfigured-startup cases passed 3/3.

The full updater and Legacy controller suites were not run because their HTTP
fixtures fail at `QTcpServer::listen()` before reaching tested behavior. Real
installer/folder actions were not invoked, and the Linux branch was not
runtime-tested on macOS.

### 8.4.2 — Next application packaging (accepted)

The Next app package replaces contents under the existing app/update asset
identities; it does not add a separate resource update asset. Independent
package verifier tests passed 11/11, covering manifest tree/hash integrity,
resource catalog and direct hashes, Qt plugin/QML runtime, licenses/SBOMs, and
absence of Legacy `.rcc` payloads. The macOS staged package verifier covered
885 paths, 466 SHA-256 checks, 187 direct resource hashes, and 21 license/SBOM
files. Its universal executable contains arm64+x86_64 with minimum macOS 14.4;
ad-hoc `codesign --verify --deep --strict` passed. The
`ClassMngr-0.19.0-macos-universal.dmg` in
`/private/tmp/classmngr-phase842-build/dmg-retry/` was created and verified
with `hdiutil` and its checksum sidecar (SHA-256
`a5e34bfc3c8d6d7f36706fb9e3cf2254f7b5608a098210246118f78f6396ee8c`). The
package executor mounted it read-only, verified the mounted tree, and detached.
An independent mount was not repeated: `hdiutil` attach failed with “Device
not configured”; the tester confirmed counts from the executor's prior report.

Windows release wiring stages the matching VS 2022 v143 redist outside the app
package, validates Authenticode and PE architecture, passes it to Inno, and
checks the embedded hash. The Inno helper checks x64 and ARM64 payloads, x64
installed Next startup stages, artifacts, and checksums. Eight synthetic CMake
ON/OFF x64/ARM64 cases passed; actual Windows PowerShell, Inno, and runtime
execution remain CI-only. Linux workflow extracts and verifies the final
tarball; YAML and extracted shell syntax passed, while package execution remains
CI-only.

Publisher tag/version, asset filenames, and the eight-entry/eight-checksum
contract are unchanged. The macOS signature is ad hoc; no Developer ID signing
or notarization was performed.

### 8.5.1 — Calendar view adapter (accepted)

Extracted an injectable `CalendarView` interface and QML factory. `CalendarPage`
still creates its model and providers lazily on entry, releases the view before
its borrowed providers, and discards partial state after a failed load so a
later activation can retry. The QML component and compact `CalendarEventModel`
contract are unchanged.

Incremental macOS builds passed for `ClassMngr`,
`ClassMngrNextCalendarPageTests`, and `ClassMngrNextMyWorkspacePageTests`.
Focused CTest execution passed both affected suites:
`ctest --test-dir build/macos-clang-debug --output-on-failure -R 'ClassMngrNext(CalendarPage|MyWorkspacePage)Tests'`.
English and Korean calendar captures in light and dark palettes were generated
and visually reviewed. No full test suite was run.

### 8.6.1 — Transfer export codec (accepted)

The mapped/staged reader was audited and left intact. Both `saveFile` overloads
now share bounded incremental JSON output with an 8 KiB buffer and Qt-escaped
1,024 UTF-16-unit chunks. A count-only preflight enforces the inclusive 16 MiB
cap before writing, preserving validation error order and the single timestamp.

Incremental macOS builds passed for `ClassMngrClassTransferSupport` and
`ClassMngrClassTransferExportCodecParityTests`; focused CTest passed 1/1:
`ctest --test-dir build/macos-clang-debug --output-on-failure -R '^ClassMngrClassTransferExportCodecParityTests$'`.
Coverage includes exact Qt-indented byte parity for Legacy and portable output
with escaped, control, and Unicode strings; exact-cap acceptance; over-cap
destination preservation; and mapped-reader behavior. No full suite or memory
measurement was run; Phase 9 retains the memory gate.

### 8.6.2 — Transfer apply workflow ownership (accepted)

The page and `ClassTransferApplyCommand` now share one immutable Portable
package handle, removing the deep command copy; Legacy conversion takes a const
reference. Retryable apply errors retain the same handle. Success, cancellation,
invalid replacement, and workspace change release it, and cleanup frees review
row-vector capacity.

Incremental macOS production and test builds passed in the standard folder:
`cmake --build build/macos-clang-debug --target ClassMngr ClassMngrNext --parallel 4`
and `cmake --build build/macos-clang-debug --target ClassMngrNextClassTransferWorkflowTests ClassMngrNextApplicationClassTransferApplyUseCaseTests ClassMngrClassTransferApplyPackageAdapterTests ClassMngrNextPlatformApplicationServicesClassTransferApplyPortTests --parallel 4`.
Focused CTest passed 4/4 in 6.47 seconds, including weak-handle identity,
retry/success/cancel release, and null-handle validation coverage. The initial
platform suite exposed a stale assertion expecting 25 rows; commit `4b5f2de9`
updated storage to `MaximumRowCount=100`, and the assertion now checks 100 rows
with bounds 0–99 while retaining wide-ID and retained-marker checks. No full
suite or memory measurement was run; Phase 9 owns the memory gate.

### 8.7.1 — Windows redistributable staging

Package run `41794662707` failed on both x64 and ARM64 at Visual Studio 2022
discovery, before package configure or build. The same-SHA rerun (attempt 2)
failed at the same point. The repair records stdout, stderr, and exit status,
and documents a narrow VS 2022 Enterprise fallback requiring the v143 tree
and requested architecture payload. Existing Authenticode, signer, PE
architecture, and staged-package checks are preserved.

Static review accepted the candidate for commit; this does not establish a
runtime pass. Ruby YAML parsing and `git diff --check` passed; PowerShell and
actionlint were unavailable. The full Windows package workflow remains pending
after the batch push. This entry does not establish the Phase 8 exit gate or
Phase 9 readiness.

### 8.7.2 — macOS/Linux package triggers

The macOS and Linux release workflows now include `Qt-Rewrite` alongside
`main` under `push.branches`. Path filters, `workflow_dispatch`,
`workflow_call`, runners, package steps, and permissions are unchanged.

Ruby Psych parsed both YAML files; removing the added branch line from each
makes it byte-for-byte identical to `HEAD`. `git diff --check` passed;
`actionlint` was unavailable. Package CI results remain pending after the
batch push, and Phase 9 remains gated.

### 8.7.3 — Windows redistributable architecture selection

At pushed SHA `c6fcae8e`, Windows run `41799924542` failed on x64 and ARM64
after `vswhere` succeeded; its logs omitted candidate path and machine. The
workflow now logs each matching candidate's path, version, and PE machine,
then selects the highest-version candidate only after successful `dumpbin`
inspection confirms the target machine. No-match diagnostics are included.
Authenticode validity and Microsoft-signer checks, staged signature/hash
checks, and installer/package checks remain in place.

Static Tester accepted the change. Ruby YAML parsing and `git diff --check`
passed; PowerShell was unavailable, and Windows runtime CI is pending. The
Phase 8 package gate remains blocked by missing Qt license notices on Linux
and macOS (runs `41799924350` and `41799924549`); Phase 9 remains gated.

### 8.7.4 — Qt license staging

CMake now stages three checked-in, verbatim Qt 6.12.0 license texts from
official `qt/qtbase` tag `v6.12.0`: `LICENSES/LGPL-3.0-only.txt` (blob
`65c5ca88a67c30becee01c5a8816d964b03862f9`),
`LICENSES/GPL-3.0-only.txt` (blob
`94a9ed024d3859793618152ea559a168bbcbb5e2`), and
`LICENSES/Qt-GPL-exception-1.0.txt` (blob
`d0322bf0e4e1d484d35cc6a1d96fa2b65b9d579b`). Provenance is recorded in
`licenses/qt/UPSTREAM.md`; the package inventory lists LGPL, GPL, and the Qt
exception separately. Installed Qt SBOM staging and validation remain.

The focused `python3 scripts/test_verify_next_package.py` verifier passed
11/11, and the static Tester accepted the contract. The upstream exception
text retains its one blank line at EOF to preserve the source hash;
`git diff --check` reports that line when checking the untracked text file,
while tracked code changes are clean. Full Windows, macOS, and Linux package
CI remains pending, and Phase 9 remains gated.

### 8.7.5 — Phase 8 Batch 4 package matrix

At SHA `7de5aa471c75bd274e1102464b8f2f02c4f23e5b`, Windows run `41806105651`
failed on x64 and ARM64 because the selected architecture-named
`vc_redist.*.exe` files have x86 outer PE images; installer and package checks
did not run. Linux run `41806105416` failed during CMake configure/generate
with a strong dependency cycle involving
`RESCAN:{ClassMngrRuntime,ClassMngrWorkspaceStore}`, `ClassMngrSubPrepShared`,
and `ClassMngrNextCalendarUi`; no build or package verifier ran.

macOS run `41806105824` passed image verification and the mounted package
verifier (853 paths, 451 hashes, 187 direct-resource hashes, 22 license/SBOM
files, and 82 universal binaries). The combined disk-image step then exited 16
before focused verifier tests and artifact upload. `hdiutil detach` is the
likely failure point: it is the script command after the last successful
verifier output, but the logs do not prove this.

The Phase 8 package gate did not pass, and no artifact was uploaded for any
workflow. Phase 9 remains gated.

### 8.7.6 — Windows redistributable package validation

Windows run `41806105651` reported PE machine `x86` for both architecture-named
v143 redistributable installers (`14.44.35211.0`). This identifies the outer
installer stub and does not establish the runtime payload architecture. Source
review confirmed Microsoft's architecture-specific package naming, the
`ClassMngr.iss` mapping, and the verifier's exact embedded filename/hash. The
workflow now accepts only the matrix-derived filename under the VS2022 v143
tree and selects the newest valid version. Authenticode, Microsoft signer, and
staged-hash checks remain, as does the separate `ClassMngr.exe` PE check.

Static Tester accepted the change. Ruby YAML parsing and `git diff --check`
passed; PowerShell was unavailable, and Windows package CI is pending. The
Linux CMake cycle and macOS disk-image exit remain open, so Phase 9 stays gated.

### 8.7.7 — Linux CMake link-group cycle

The Linux dependency cycle is addressed by keeping the Runtime/WorkspaceStore
`RESCAN` group and moving WorkspaceStore, Sub Prep, and Calendar UI edges out of
`ClassMngrRuntime`'s public interface into the legacy runtime closure. The
no-RESCAN fallback and Apple test runtime retain their required dependencies.
Static review and `git diff --check` passed. This Darwin host has no Linux
release preset or `QT_LINUX_PREFIX`, so local Linux configure was unavailable;
Linux package configure/build CI remains pending. Phase 9 remains gated.

### 8.7.8 — macOS disk-image cleanup

After mounted-package verification succeeds, the release script retries
`hdiutil detach` status 16 up to three times and reports failures. If detach or
temporary-directory removal still fails, it warns and continues to checksum
generation; the EXIT trap retains one final best-effort detach attempt.
Verification failures remain fatal and preserve their exit status.

`bash -n` and `git diff --check` passed. A temporary harness covered retry
success, persistent detach failures, other detach errors, verification failure,
and directory-removal failure; checksum generation continued after cleanup
failures, and verification failure retained its original status. It did not
exercise a real macOS mount. The prior package run's exit 16 is still inferred
to be a busy detach, and macOS package CI remains pending. Phase 9 remains
gated.

### 8.7.9 — Linux package domain-model link closure

At pushed SHA `3808d5da9d4fa4c76a7df5e9d14d31a8234b45ed`, Linux run
`41813392564` configured successfully but failed linking `ClassMngrNext`:
`ClassMngrNextWorkspaceDomain` was scanned before Sub Prep and WorkspaceStore,
leaving `Classroom` and `Roster::BaseColumns` undefined. The correction puts the
existing Domain objects in one `ClassMngrDomainArchive`, shared privately by
WorkspaceStore, Runtime, and Apple TestRuntime; NextWorkspaceDomain now uses
that archive, and its exact dependency assertion is updated. Static review
accepted the target graph and source ownership. No local Linux build was run
on this Darwin host; Linux package CI must confirm link order and symbols. The
prior Windows and macOS runs were still in progress at the last check. Phase 9
remains gated.

### 8.7.10 — Linux package color-validation dependency

At SHA `76bba91f0bc65be0f4f09392ff8ec52c650178c5`, Linux rerun `41816907048`
resolved the Classroom/Roster symbols but exposed an undefined
`ColorUtils::canonicalHexColor` reference, whose implementation was owned by
the earlier ClassMngrClassTransferSupport archive. The function was extracted
to a separately owned `ClassMngrColorValidation` leaf target, privately linked
by ClassMngrDomainArchive and ClassMngrClassTransferSupport. Source ownership
and exact dependency-boundary assertions were updated; static review passed.
No local build or test was run. Package CI is pending, so Phase 9 remains
gated.

### 8.7.11 — Qt Concurrent package runtime artifact (accepted)

At SHA `5cad12853814d8cd348b6f22ad3c9a788d5c3a2a`, Linux run `41820638403`
built and linked, but strict package verification failed because the package
did not contain Qt Concurrent. Windows run `41820638202` failed the same
verifier check for x64 and ARM64. The exact reason Qt's automatic deployment
omitted the library is unproven.

`cmake/next_resources.cmake` now installs the imported runtime artifact
`Qt6::Concurrent` in the Runtime component before the generated Qt deployment
script: Windows DLLs go to `${CMAKE_INSTALL_BINDIR}`, Linux libraries to
`${CMAKE_INSTALL_LIBDIR}`, and macOS dylibs/frameworks to
`ClassMngr.app/Contents/Frameworks`. Static review accepted the rule. No local
build or test was run. At the same SHA, macOS run `41820638665` passed the
disk-image build and package verification. Its checksum reported VALID; the
focused verifier checked 853 paths, 451 hashes, 187 direct-resource hashes,
and 22 license/SBOM files, then uploaded `ClassMngr-macos-universal` (DMG,
checksum, and JSON report). The package step passed and no cleanup failure
surfaced, though the log does not expose the explicit detach command or result.

Fresh package verification now passes with the explicit staging rule. At
`79cec8a3ed7344973f1321909a6644b31711ce63`, Windows Release run `42138310757`
passed installer verification and artifact upload for x64 and ARM64, and
macOS universal run `42138310884` passed disk-image verification, focused
package verifier tests, and artifact upload. At the pushed Phase 7.5 batch
head `a86e260be63615c075aefa4e460ea96832021fb8`, Linux Release run
`42142955221` passed build/install, architecture/package verification, and
artifact upload. Its optional Phase 0 route remained uninvoked. The diff from
the Windows/macOS evidence revision changes only the Linux workflow and plan
documents; the Qt Concurrent staging implementation is identical across the
three package results. This accepts 8.7.11; the broader Phase 8 gate remains
open.

### 8.7.12 — Windows Release startup verifier and package gate

At `53aaeed2a4b1f1123a12a4f365696968c2b5cc80`, Windows Release run
`42111769908` passed final installer verification (step 22) for x64 and
ARM64. The x64 log confirms all ten installed startup stages in order:
`process-created`, `core-services-created`, `settings-loaded`,
`language-loaded`, `theme-loaded`, `required-fonts-loaded`,
`workspace-opened`, `shell-created`, `initial-page-ready`, and
`startup-ready`. Both architecture-specific
`WINDOWS_PACKAGE_VERIFICATION_SUCCESS` sentinels appeared. Final verification
logs (step 23) and release artifacts (step 25) uploaded for both architectures.
The verifier fix reads startup output asynchronously and makes the workflow
fail on verifier errors or a missing success sentinel. Windows Release package
verification is therefore accepted on both architectures.

Linux Release run `42104507218` passed its package-verification step (step 9)
and uploaded its artifact at ancestor SHA
`44e89448a1b463a1428de1d89f43a7cd445fdb27`; the workflow later failed at step
11 in a supplemental Phase 0 route-contract test because
`CLASSMNGR_STARTUP_SUB_PREP_OUTPUT_REFERENCE_DATE='2026-09-14'` was treated as
a confined path. macOS Release run `42104507137` passed at that same ancestor
SHA. These are not Linux/macOS reruns at the Windows verifier fix SHA. Separate
Phase 4 resource-install-layout run `42111769886` passed on Windows x64, Linux
x64, and macOS.

The local verifier invocation previously shown stopped at
`verify_next_package.ps1:32` because
`dist/checksums-windows-x64.txt` was absent; it did not reach extraction. The
CI results verify the uploaded Windows packages, but do not establish the
Phase 8 exit gate: all v2 external integrations must work without platform
code leaking into feature logic, and generated output must match the baseline
on every supported platform. Phase 7.5 is Complete and does not block
Phase 8. The Linux supplemental route-contract failure is a separate follow-up.

### 8.7.13 — Linux Legacy Debug link closure and Phase 5 rerun

The clean Linux x64 Debug baseline at `79cec8a3ed7344973f1321909a6644b31711ce63`
failed while linking Legacy `ClassMngr`, before CTest. The generated link line
rescanned Runtime and WorkspaceStore, then placed shared Sub Prep and Calendar
archives outside the group even though those archives both reference Runtime.

The closure now assembles after `ClassMngrNextCalendarUi` is declared. On
linkers supporting CMake's `RESCAN` group, Runtime, WorkspaceStore,
`ClassMngrSubPrepShared`, and `ClassMngrNextCalendarUi` are rescanned together.
The unsupported-linker path retains the direct dependency order.

After clearing the standard `build/linux-gcc-debug` folder, a fresh configure
with Qt 6.12.0 and GCC 16.2.1 completed with `CCACHE_DISABLE=1`. The clean
all-target build passed 947/947; the post-closure incremental build passed
2204/2204. The closure links DomainArchive with Runtime, WorkspaceStore,
SubPrepShared, and NextCalendarUi; the emitted link command confirms the
complete archive set is inside `--start-group` and `--end-group`, and seven
newly closure-linked targets passed independently.

The direct Phase 5 CTest run completed 619 tests: 602 passed, 14 failed, and
3 timed out (exit 8). The timed-out tests were `ClassMngrPageManagerTests`,
`ClassMngrClassDetailsSavePageTests`, and `ClassMngrSubPrepPageTests`. The
suite is therefore not green; Windows x64, macOS universal, hosted
exact-toolchain captures, and the package/resource matrix remain pending.

### Phase 8a — Windows x64 Batch 3 clean candidate

On 2026-11-12, the clean Windows x64 Debug candidate passed at
`bb37cf9aaa4562b4fdf9c9b592df2012928bf5c6`: full build, 618/618 CTest targets,
Schedule, Speaking, all four Roster layouts, and C53 (12 Sub Prep and 5 Roster
pages, zero changed pixels). Evidence, hashes, QWARN dispositions, and remaining
limits are in the [Phase 8a progress log](09.5-Phase-8a-Progress-Log.md#2026-11-12---batch-3-windows-x64-clean-candidate).
This closes Windows Batch 3 only; Phase 8 remains open for shared provenance/
capture work and macOS/Linux candidates.

### 8.7.14 — Windows x64 Release package verification (2026-11-12; accepted)

Official Windows Release Build run [43156278197](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/43156278197), a push on `Qt-Rewrite` at exact HEAD `1d4cb7d1d0b4c762715ef887ced6e81f97a9d3d1`, passed x64 job `130976853399`. Installer build, architecture/checksum checks, staged-package verification, final payload extraction, silent install, installed-tree verification, and startup gate all passed. The verifier reported 394 paths, 327 SHA hashes, 187 direct-resource hashes, and 22 license/SBOM files. The VS 2022 v143 redistributable `14.44.35211.0` had a valid Authenticode signature from Microsoft Corporation and SHA-256 `CC0FF0EB1DC3F5188AE6300FAEF32BF5BEEBA4BDD6E8E445A9184072096B713B`. All ten startup stages passed through `startup-ready`; the x64 success sentinel was present. Independent Tester accepted the run.

The [ClassMngr-windows-x64 artifact](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/43156278197/artifacts/13916749851) (ID `13916749851`, 108294767 bytes) has archive SHA-256 `fbcbdd24a53dbaf99efb2b11955ed2c58aa6f8bae07af79e581e26f21484e7d0`; the [final verifier log artifact](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/43156278197/artifacts/13917627426) (ID `13917627426`, 5320 bytes) has SHA-256 `bfa392fbeb246f0782b9a185cdd0b074ea37146fadeb901f56e6ba139ec69630`. Both were unexpired at retrieval and expire 2027-02-10. This resolves the Windows x64 Release package gap at this HEAD. The separate local retry is incomplete and is not package evidence: it used VS18/v145 intermediates and did not run the final verifier. The earlier hosted HTTP 504 attempt remains a historical failure without an artifact. Phase 8/8a remain open for native macOS work and final cross-host review.
