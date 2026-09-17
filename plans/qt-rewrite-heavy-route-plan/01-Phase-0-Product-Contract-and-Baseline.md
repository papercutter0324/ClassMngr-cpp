# Phase 0 — Product Contract, Source Archaeology, and Baseline

## Status

- Status: Complete
- Default route: Heavy
- Depends on: None
- Blocks: None; Phase 1 is unblocked
- Owner: Unassigned
- Last updated: 2026-09-18
- Current note: Phase 0 was declared complete by the user on 2026-09-18. Fresh
  packaged Windows x64 and macOS universal matrices pass all 24 routes each;
  combined `--require-exit-gate` validation using the Windows run root and both
  retained macOS archives passed. The user confirmed the retained visual
  references look correct. Phase 1 is unblocked; historical evidence and
  environment limitations remain recorded in the baseline log.

### Phase 0 platform and route gate

- Required packaged Release evidence: Windows x64 and macOS universal.
- Windows ARM64 and Linux are unofficial ports deferred to later work; they do
  not block Phase 0.
- The exit gate requires all 24 route IDs on each required platform. A valid
  subset per-run result is not gate completion; use `--require-exit-gate` with
  the Windows evidence root and `--macos-evidence-root` to enforce complete
  coverage. See the [runner and validator guide](../../scripts/phase0/README.md).

## Product decision update - 2026-09-16

- Documents displayed through the QtPdf viewer are on-demand content, not
  startup resources.
- Startup may load the document catalog, localized names, and validated asset
  references, but it must not load PDF bodies, render PDF pages, or call
  `QPdfDocument::load()` for catalog entries before the user requests a
  document.
- A requested document is owned by the active viewer session. Closing the
  viewer, replacing the document, leaving the viewer, or releasing the page
  must close the QtPdf document and release its document resource. Reopening a
  document may load it again.
- Generated and print-output PDFs remain separate operation-scoped resources;
  this decision does not require changing their output contract.
- The Windows x64 route matrix and packaged macOS universal baseline now record
  the startup negative case, on-demand document open, and release after viewer
  close. The macOS 24-route matrix remains 0/24 and blocks the combined gate;
  see the [baseline evidence](../../docs/qt-rewrite/phase-0-baseline.md).

## Progress update - 2026-09-15

- What changed: added the initial Phase 0 evidence set in
  `docs/qt-rewrite/` covering source ownership, features, file formats,
  resources, baseline commands, and risks.
- What remains: capture screenshots and generated-output references, create
  representative fixtures, run clean Windows x64 and macOS universal Release
  baselines, and record startup/resource traces.
- Evidence: source snapshot `75755460`; raw resource inventory is 226 files
  totaling 65,729,685 bytes; source/test inventory and the current CTest
  enumeration are recorded in the evidence documents.
- Risk: existing build directories include stale configuration from an earlier
  tree, so they are excluded from acceptance evidence until reconfigured.

## Progress update - 2026-09-15 (fixture and clean build)

- What changed: replaced the deleted historical `Testing-copy.tps` dependency
  with the reproducible fixture `tests/fixtures/workspaces/representative_startup.sql`;
  the startup test now materializes it through `DatabaseSchemaManager`.
- Evidence: clean Windows x64 Ninja Debug configuration and a 352-step focused
  target build passed; `ClassMngrStartupPerformanceTests -v1` passed both tests
  with exit code `0`.
- Preliminary measurement: the representative run reported a peak working set
  of `202,162,176` bytes at startup-complete and `221,011,968` bytes at the
  five-second settled checkpoint in a Debug/offscreen process. The exact byte
  values and scenario are recorded in `docs/qt-rewrite/phase-0-baseline.md`;
  this is not packaged Release acceptance evidence.
- What remains: add the other Phase 0 fixtures, capture visual and generated
  output references, trace startup/resource ownership, and run packaged Release
  baselines on Windows x64 and macOS universal.
- Risk: Qt 6.12 QML import scanning makes clean configuration slow, and the
  older Visual Studio build directory still loops during regeneration.

## Progress update - 2026-09-16 (visual capture harness)

- What changed: added the in-process `--startup-visual-capture-output` mode.
  It captures the visible main window at `startup-complete` and, when a settle
  interval is requested, at `settled-final`; the startup test now validates
  both PNG outputs for the representative fixture.
- Why: native desktop automation is unavailable in the current environment, and
  this keeps visual evidence on the real Qt startup path without changing the
  normal application flow.
- Follow-up: the harness accepts explicit English/Korean and light/dark
  overrides, the focused startup test exercises all four combinations, and a
  permanent empty-workspace matrix is now in `docs/qt-rewrite/visual-baseline`.
- What remains: capture populated, editing, read-only, dialog, loading, error,
  and generated-output references; packaged Release and cross-platform evidence
  remain open.

## Progress update - 2026-09-16 (large workspace fixture)

- What changed: added the generated `large_startup.sql` workspace fixture and
  generalized the startup fixture materializer so tests can build named SQL
  fixtures into temporary `.tps` files.
- Evidence: the focused `ClassMngrStartupPerformanceTests` run passed all four
  tests, including integrity and exact row-count checks for the large fixture.
- Fixture contract: 24 teachers, 96 classes, 768 regular schedule rows, 24
  intensive rows, 5 intensive slot states, 7,200 roster cells, 20 speaking
  evaluations with 600 data rows, 3 campuses, and 180 calendar events.
- What remains: add legacy/corrupt/locked compatibility fixtures and use the
  large workspace in packaged Release workflow and memory measurements.

## Progress update - 2026-09-16 (legacy compatibility fixture)

- What changed: added `legacy_startup.sql`, a permanent partial schema-version
  zero `.db` source, and an end-to-end migration check in the focused startup
  fixture suite.
- Evidence: the migrated database reaches the latest schema, passes SQLite
  integrity and foreign-key checks, preserves its class and schedule, repairs
  the invalid teacher reference, and writes the pre-schema-v4 backup.
- What remains: add generated corrupt and locked-file scenarios, then run the
  large and legacy workspaces through packaged Release startup and workflow
  measurements.

## Progress update - 2026-09-16 (corrupt and locked workspace checks)

- What changed: added transient corrupt-file and exclusive-lock scenarios to
  the focused fixture suite; no lock artifact is checked in.
- Evidence: malformed input is rejected without changing its bytes; locked
  legacy migration is rejected, and the same file reaches the latest schema
  after the lock is released.
- What remains: package the Release executable, measure the large and legacy
  workspaces through startup and workflows, and add generated-output fixtures.

## Progress update - 2026-09-16 (representative visual matrix)

- What changed: extended the visual capture suite to launch the reproducible
  representative workspace in all four English/Korean and light/dark variants.
  `CLASSMNGR_VISUAL_BASELINE_OUTPUT_DIR` can retain those generated PNGs for
  reviewable evidence; default tests continue to use temporary output.
- Evidence: the focused startup suite remains green after adding the four
  populated launches, each producing a valid startup-complete image; the
  retained matrix is under `docs/qt-rewrite/visual-baseline/representative`.
- What remains: cover editing, read-only, dialog, loading/error, print-preview,
  and generated-output states.

## Progress update - 2026-09-16 (packaged Windows x64 Release baseline)

- What changed: built and installed a fresh Windows x64 Ninja Release tree,
  then ran the installed executable through empty and representative startup
  scenarios with five-second settled captures. The representative fixture can
  now be retained from the test for repeatable packaged runs.
- Evidence: the Release build completed all `347` steps, installation
  succeeded, and both packaged startup runs exited `0`. Release JSON traces
  and `startup-complete`/`settled-final` PNGs are retained under
  `docs/qt-rewrite/visual-baseline/release/`.
- Measurements: empty startup-complete/settled-5s were `2,974`/`8,028 ms`
  with `138,178,560` bytes peak working set; representative startup-complete/
  settled-5s were `3,037`/`8,103 ms` with `169,287,680` bytes peak working set
  and `150,745,088` bytes peak private usage.
- What remains: collect packaged per-workflow memory traces, cross-platform
  Release baselines, conflict/import-review coverage, and generated-output
  references.

## Progress update - 2026-09-16 (large workspace Release baseline)

- What changed: retained the generated `large_startup.sql` fixture as a
  repeatable `.tps` input and ran it through the installed Windows x64 Release
  executable with the same startup and settled checkpoints as the normal
  representative run.
- Evidence: the `622,592`-byte large fixture launched successfully and exited
  `0`; its Release JSON trace and startup/settled PNGs are retained under
  `docs/qt-rewrite/visual-baseline/release/large/`.
- Measurement: `database-opened`/`startup-complete`/`settled-5s` were
  `748`/`2,942`/`8,000 ms`; peak working set was `175,370,240` bytes and peak
  private usage was `156,618,752` bytes. The startup-complete snapshot had 231
  widgets and 49 schedule cell widgets, with a 118 ms full schedule render.
- What remains: run the legacy compatibility workspace through packaged
  Release startup, then cover large-feature entry/exit and generated outputs.

## Progress update - 2026-09-16 (legacy workspace Release baseline)

- What changed: retained the migrated schema-version-zero fixture and ran the
  resulting `.db` through the installed Windows x64 Release startup path. The
  compatibility test still verifies the latest schema, foreign keys,
  integrity, repaired teacher reference, and migration backup sidecar.
- Evidence: the legacy Release run exited `0`; its JSON trace and
  startup/settled PNGs are retained under
  `docs/qt-rewrite/visual-baseline/release/legacy/`.
- Measurement: `database-opened`/`startup-complete`/`settled-5s` were
  `616`/`2,942`/`7,998 ms`; peak working set was `161,640,448` bytes and peak
  private usage was `141,246,464` bytes. The startup-complete snapshot had 212
  widgets and 30 schedule cell widgets, with a 5 ms full schedule render.
- What remains: add conflict/import-review coverage, measure large-feature
  entry/exit and idle retention, and capture generated-output references.

## Progress update - 2026-09-16 (conflict/import-review fixture)

- What changed: added the permanent `conflict_source.json` class-transfer
  package and an offscreen `ClassImportDialog` capture path. The new test
  loads the package, verifies matching class/teacher review choices, captures
  the review dialog when requested, and confirms a colliding schedule is
  rejected atomically.
- Evidence: `ClassMngrClassTransferTests` passed all 16 tests; the readable
  dialog reference is retained at
  `docs/qt-rewrite/visual-baseline/conflict/class-import-review.png`.
- What remains: add schedule-workbook/import-review fixtures, then measure
  large-feature entry/exit and idle retention and capture generated outputs.

## Progress update - 2026-09-16 (schedule workbook staged-review fixture)

- What changed: added the permanent `schedule_review.xlsx` workbook and
  deterministic retention hooks for the existing schedule-import dialog test.
  The test now exercises the normal workbook/source-selection path, builds the
  staged review, and can retain a readable offscreen dialog reference.
- Evidence: `ClassMngrScheduleImportDialogTests` passed all 17 test functions;
  the workbook and review image are retained at
  `tests/fixtures/imports/schedule_review.xlsx` and
  `docs/qt-rewrite/visual-baseline/schedule/schedule-import-review.png`.
  Review preparation also now loads saved schedule display preferences before
  rendering the preview, covering the existing saved-settings assertion.
- What remains: add a large schedule-workbook/conflict fixture, measure
  large-feature entry/exit and idle retention, and capture generated outputs.

## Progress update - 2026-09-16 (larger schedule-workbook conflict fixture)

- What changed: added a permanent multi-class schedule workbook with two
  imported classes that resolve to the same existing class, plus a test-only
  output hook and offscreen conflict-state capture.
- Evidence: the normal file-selection path reaches review, presents the
  duplicate-target warning, disables Import, and the test passes after the
  warning is acknowledged. The fixture and reference are retained at
  `tests/fixtures/imports/schedule_large_conflict.xlsx` and
  `docs/qt-rewrite/visual-baseline/schedule/schedule-conflict-review.png`.
- What remains: measure large-feature entry/exit and idle retention, then
  capture generated outputs and the remaining cross-platform/packaged visual
  references.

## Evidence files

- [Source archaeology](../../docs/qt-rewrite/phase-0-source-archaeology.md)
- [Feature preservation matrix](../../docs/qt-rewrite/phase-0-feature-preservation-matrix.md)
- [File compatibility matrix](../../docs/qt-rewrite/phase-0-file-compatibility-matrix.md)
- [Resource and ownership inventory](../../docs/qt-rewrite/phase-0-resource-and-ownership-inventory.md)
- [Baseline and evidence log](../../docs/qt-rewrite/phase-0-baseline.md)
- [Risk register](../../docs/qt-rewrite/phase-0-risk-register.md)

## Objective

Create a reliable description of current behavior, visual appearance, file compatibility, startup work, resource usage, and memory usage.

## Work packages

### 0.1 Feature inventory

Inventory the current source, menus, actions, sidebar, setup wizard, dialogs, services, controllers, and tests.

Record:

- Every top-level page.
- Every nested class, calendar, workspace, roster, and setup page.
- Every menu and action.
- Every keyboard shortcut.
- Every context menu.
- Every enabled-state and permission rule.
- Every admin and developer path.
- Every import, export, print, report, and external-application workflow.
- Every user-facing setting.

### 0.2 Visual contract

Capture reference images for:

- English and Korean.
- Light and dark themes.
- Empty and populated pages.
- Editing and read-only pages.
- Loading, error, warning, and confirmation states.
- Import review and conflict resolution.
- Initial setup.
- Print previews.
- Document-catalog, document-loading, document-ready, document-error, and
  document-closed/reopened viewer states.
- Generated documents, reports, and rosters.

Record window geometry, splitter positions, table column widths, font sizes, icon sizes, and important spacing values where they are user-configurable.

### 0.3 Data and file contract

Create fixture workspaces for:

- Empty data.
- Small normal data.
- Large realistic data.
- Legacy database import.
- Large schedules.
- Large rosters.
- Large speaking-evaluation datasets.
- Multiple campuses and document catalogs with representative PDF/PPTX assets.

Document behavior for:

- .tps creation and opening.
- Legacy .db import.
- Save, save-as, export, and backup.
- Class transfer files.
- Schedule, teacher, calendar, and roster imports.
- Partial and failed imports.
- Corrupt or locked databases.
- Document catalog metadata versus on-demand QtPdf content loading and release.

### 0.4 Packaged startup baseline

Build packaged Release artifacts for the required Phase 0 platforms:

- Windows x64.
- macOS universal.

Windows ARM64 and Linux are unofficial ports deferred to later work and are not
Phase 0 baseline requirements.

Do not use a debugger, development Qt installation, or profiler for the primary memory report.

Measure:

1. Process creation.
2. QApplication creation.
3. Resource initialization.
4. Settings and language load.
5. Theme and font application.
6. Main window construction.
7. Main window shown.
8. Initial page rendered.
9. Startup-ready, including confirmation that no catalog PDF is loaded.
10. Five minutes idle with catalog metadata still resident only.
11. Each large feature entered.
12. A representative PDF opened, rendered, navigated, and closed/released.
13. Each large feature left, including the document viewer.
14. Heavy output completed.

On Windows x64 record working set, private bytes, commit, peak working set,
handle count, and thread count. Record equivalent resident and private metrics
on macOS universal. Windows ARM64 and Linux remain deferred ports.

### 0.5 Startup and resource trace

Trace:

- Page construction.
- Service construction.
- Database connections and query sizes.
- Resource-pack initialization.
- Font loading.
- Translation loading.
- Stylesheet loading.
- Image decoding.
- PDF construction, `QPdfDocument::load()`, page rendering, close, and release.
- Table item and cell-widget creation.
- Document catalog creation and metadata-only startup behavior.
- Document viewer content-open count, active-document count, and retained
  bytes across open/close/reopen.
- Update checks.

Catalogue resources by:

- Installed size.
- Decoded or resident size.
- Startup necessity.
- Owning feature.
- Expected lifetime.
- Whether the resource can be streamed.

### 0.6 Compatibility and risk register

Create a risk register for:

- Database migration.
- File-format compatibility.
- Print and PDF differences.
- PDF viewer on-demand loading and release.
- PowerPoint automation.
- Korean input and localization.
- Font metrics.
- QML calendar behavior.
- Application update packaging.
- Existing installed resource-pack content.
- Windows-only memory behavior.

### 0.7 Cross-cutting memory hotspot baseline

Use the [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) to complete the before-state audit
for every identified large-data or widget-heavy workflow. In addition to the
startup baseline, capture the 96-class Sub Prep route, My Classes and Classes
navigation, schedule and calendar imports, schedule-table refresh, speaking
batch review, class transfer, staff directory, PDF/report output, and
workspace page entry/leave.

Each scenario must record its fixture, lifecycle checkpoints, working set,
private bytes, peak values, page/model/widget counts, and process outcome.
Keep the large fixtures as required stress inputs and record normal resident
memory separately from transient operation peaks.

## Deliverables

- Feature preservation matrix.
- Visual reference set.
- File-format compatibility matrix.
- Startup timeline.
- Per-platform memory report.
- Resource inventory.
- Ownership and cache inventory.
- Document viewer loading and release contract with runtime evidence.
- Compatibility-path inventory.
- Risk register.
- Baseline test commands and expected results.

## Exit gate

The team can answer what every user-facing feature does, which resources it needs, which files it reads and writes, and how much memory each major workflow consumes.

The evidence must also prove that opening the application does not load a PDF
into QtPdf, while an explicitly requested document loads successfully and is
released when its viewer session ends.

No v2 feature work begins without a fixture and an acceptance check.

## Heavy-route requirements

- For every Phase 0 slice, use the heavy route: make the evidence slice
  explicit, end to end, and acceptance-ready; keep legacy behavior only as a
  comparison oracle or an explicitly temporary bridge with a removal point.
- Use real packaged Release builds.
- Capture the worst representative workspace, not only an empty database.
- Measure actual Windows Task Manager-visible memory and private allocation.
- Keep screenshots and output files as permanent parity artifacts.
- Treat PDF viewer content as session-scoped; do not preload or retain full
  document bodies merely to populate the catalog.
- Do not rely on prior plan documents or stale build-tree test inventories.

## Progress update - 2026-09-16 (in-process page lifecycle workflow)

- What changed: added the explicit `--startup-performance-workflow` harness
  and page-enter/page-leave profiler events. The workflow drives all 11
  registered top-level routes, opens the deferred Calendar child once,
  returns to My Workspace, and then records settled memory checkpoints.
- Evidence: the focused startup test passes the representative full-route
  process workflow. A packaged Windows x64 Release run is retained under
  `docs/qt-rewrite/visual-baseline/release/workflow/` with its trace, JSON
  report, startup frame, and settled frame.
- Measurement: startup-complete/workflow-complete/settled-1s/settled-5s were
  `2,926`/`6,249`/`7,283`/`11,267 ms`. The workflow settled at 2,530 widgets,
  11/11 instantiated pages, three live ScheduleWidgets, and a
  `244,961,280`-byte peak working set (`236,060,672` bytes private).
- Heavy-route finding: the existing 96-class large fixture cannot complete the
  Sub Prep route because its 768 schedule entries expand hundreds of class
  information cards and terminate before a stable profile is written. The
  bounded representative workflow proves route semantics; the large fixture
  remains the stress input for the v2 resource/virtualization slice. The
  implementation slice is tracked in [Sub Prep Class Information and Output
  Memory Plan](sub-prep-class-information-memory-plan.md) and is owned by
  Phase 7J, with Phase 9 providing the final memory gate.

## Progress update - 2026-09-16 (QtPdf session lifecycle trace)

- What changed: the same all-route workflow now uses the existing Documents
  resource-pack lease to open and render the 38-page Lesson Planning Guide,
  closes/releases it, reopens and renders it, and closes/releases it again.
  `StartupProfiler` records PDF load, render, and release events plus live
  document counts; the harness can retain the two PDF frames through
  `CLASSMNGR_STARTUP_PDF_CAPTURE_OUTPUT_DIR`.
- Evidence: the focused Debug workflow test and the full focused Phase 0 test
  set pass. The Release artifact is retained under
  `docs/qt-rewrite/visual-baseline/release/workflow/` with
  `pdf-opened.png`, `pdf-reopened.png`, `workflow-trace.txt`, and the updated
  metrics report.
- Measurement: startup has zero loaded PDF documents; `pdf-opened` reports
  one live document/one render, `pdf-released` reports zero live/one release,
  `pdf-reopened` reports one live/two renders, and the final release reports
  zero live/two releases. The route completed at `7,596 ms`; the five-second
  sample was `12,638 ms`; peak working set was `259,440,640` bytes and peak
  private usage was `298,278,912` bytes.
- Heavy-route implication: the PDF body remains session-scoped and the
  resource lease is released between opens. The added peak is now a measured
  regression boundary for the v2 viewer/resource owner; future implementation
  slices must carry this full route and preserve the explicit release gap.

## Progress update - 2026-09-16 (route retention and five-minute idle gate)

- What changed: the full-route profiler now records a `workflow-page-left`
  checkpoint for every transitioned top-level page and supports a
  `settled-5m` checkpoint when the Release runner requests a five-minute idle
  window. This preserves the route-level memory history without forcing
  deferred deletion or using event processing as a memory workaround.
- Evidence: the Release five-minute workflow is retained under
  `docs/qt-rewrite/visual-baseline/release/workflow-five-minute/`; the shorter
  `release/workflow/` artifact was refreshed to the same source. Both include
  route/PDF captures and the full page-transition trace.
- Measurement: all 11 leave checkpoints completed in order. The five-minute
  sample reached `308,890 ms` with 2,530 widgets, 11/11 pages, zero live PDF
  documents, and 2 loads/2 renders/2 releases. Working set was
  `250,036,224` bytes at five minutes, with a `259,510,272`-byte peak and
  `298,201,088`-byte peak private usage.
- Heavy-route implication: this is the retained-memory oracle for the future
  v2 PageHost/feature-release owners. It confirms that the representative
  route is idle-stable while the large 96-class Sub Prep fixture remains the
  required scalability failure boundary; the next implementation slice must
  address that boundary through the parallel v2 ownership model.
- What remains: feature-specific retained-memory measurements beyond the
  route-level checkpoints, generated-output references, and the remaining
  packaged/cross-platform visual baselines. Future implementation slices must
  follow the heavy route by introducing the parallel v2 ownership and resource
  boundaries rather than extending the legacy composition root.

## Progress update - 2026-09-16 (Sub Prep sub-prep update evaluation)

- What changed: the recent Sub Prep update adds a named Phase 7J heavy slice
  for memory-bounded class information and operation-scoped output. It now
  defines summary/detail/print-source contracts, model/delegate ownership,
  selected-class lifetime, invalidation, output cleanup, and the Phase 9
  large-fixture memory gate.
- Evaluation: this is a substantive sequencing and ownership change, not a
  documentation-only refinement. It confirms that the current page-wide rich
  `TeacherGroup`/class-card graph is the failure boundary and that the v2 work
  must move data, application, UI, output, and lifecycle ownership together.
  The new plan does not close Phase 0 or authorize a model-only v2 patch.
- Follow-on plan impact: the newer cross-cutting Memory Hotspot Remediation
  Plan is consistent with this decision. It assigns Phase 0 the before-state
  measurements, Phases 2–8 the compact data/presentation/output boundaries,
  Phase 9 the packaged Release memory gate, and later phases the permanent
  regression/removal work. It broadens the same requirement beyond Sub Prep;
  it does not move ownership of this Phase 0 evidence or make the Sub Prep
  migration smaller.
- New Phase 0 work required by the update: retain a failure-boundary artifact
  from the actual `large_startup.sql` Sub Prep workflow, including the last
  reached checkpoint and process outcome; add Sub Prep-specific class-info
  widget/editor/model-row/query-result measurements; capture the selected,
  changed-selection, empty, both-language/theme, and generated-output visual
  states; and state the exact packaged Release memory budget and pass/fail
  thresholds that Phase 9 will enforce. Remaining generated-output and
  cross-platform references also stay open.
- Evidence added in this slice: a packaged Release output oracle uses a
  synthetic service payload matching the large fixture's 24 teachers, 96
  classes, and 7,200 roster cells. It generates and reopens a 19-page Sub
  Prep PDF and a 16-page Daily roster PDF, retaining first-page PNGs and a
  manifest under
  `docs/qt-rewrite/visual-baseline/release/sub-prep-output/reference/`.
  This closes only the corresponding output reference; it is not evidence that
  the actual large route is stable.
- Decision and next heavy slice: remain in Phase 0, keep the legacy Sub Prep
  path as the comparison oracle, and do not begin the Phase 7J production
  migration yet. The next slice will instrument and retain the actual
  large-fixture Sub Prep failure boundary so the eventual 7J vertical slice
  has measurable acceptance inputs.

## Progress update - 2026-09-16 (large Sub Prep boundary trace)

- What changed: the startup profiler now exposes Sub Prep class-information
  widget, text-editor, logical-navigation-row, source-class, visible-class,
  teacher-group, rebuild, and per-class lookup counts. Lifecycle events flush
  to the existing workflow trace so the boundary remains reviewable even when
  a child process cannot write its final JSON report. An opt-in harness runs
  the actual `large_startup.sql` fixture through the packaged Release route
  and retains the process result, stdout/stderr, workflow trace, and profile.
- Evidence: the fresh Windows x64 Release child run completed the full route
  and exited normally. `workflow-complete` was `9,553 ms` and `settled-1s`
  was `10,587 ms`; peak working set was `410,468,352` bytes and peak private
  usage was `452,853,760` bytes. The retained artifact is
  `docs/qt-rewrite/visual-baseline/release/large-sub-prep-boundary/`.
- At the one-second settled checkpoint the process retained `401,285,120`
  bytes working set and `425,164,800` bytes private usage, with the same
  2,948 class-information descendants and 192 text editors.
- Sub Prep boundary: one rebuild loaded 96 source classes, produced 96
  visible navigation rows in 24 teacher groups, performed 96 class-info, 96
  teacher, and 96 roster lookups, and retained 2,948 class-information
  descendants including 192 text editors. Total process widgets reached
  9,677. This is a completed legacy route with an unacceptable memory shape,
  rather than evidence that the v2 bounded-memory requirement is met.
- Evaluation impact: the recent Sub Prep update is validated as the correct
  7J ownership change. Phase 0's failure-boundary requirement is now replaced
  by a measured high-memory boundary for this source/toolchain snapshot, but
  refresh/re-entry/release measurements, actual SQL query/result sizes,
  Sub Prep visual states, final Release thresholds, remaining output references,
  and cross-platform baselines remain open.
- Decision and next heavy slice: keep Phase 0 open and use these measured
  counts as the 7J before-state oracle. The next slice will exercise refresh,
  selection, leave, and repeated re-entry against the same large fixture so
  deferred deletion and retained detail state are measured before the v2
  vertical migration begins.

## Progress update - 2026-09-16 (large Sub Prep lifecycle retention)

- What changed: the heavy-route harness now drives the actual packaged Release
  `large_startup.sql` workspace through class selection, two refreshes, two
  leaves, and two re-entries. It records lifecycle checkpoints, process memory,
  class-information graph counts, and the database result shape. The harness
  remains opt-in because this is an intentionally high-memory legacy boundary.
- Evidence: the Release child completed normally with
  `workflow-complete` at `12,161 ms` and `settled-1s` at `13,197 ms`.
  Peak working set was `447,959,040` bytes and peak private usage was
  `477,900,800` bytes. At the final lifecycle checkpoint the process retained
  `447,959,040` working-set bytes and `445,661,184` private-usage bytes; the
  later settled checkpoint retained `415,027,200` working-set bytes and
  `450,199,552` private-usage bytes. The retained artifact is
  `docs/qt-rewrite/visual-baseline/release/large-sub-prep-boundary/`.
- Retention finding: the first load created 2,948 class-information
  descendants and 192 text editors. Refresh one increased those counts to
  5,896 and 384; refresh two increased them to 8,844 and 576. Total process
  widgets at those checkpoints grew from 5,387 to 8,335 to 11,283. Both leave
  checkpoints and both re-entry checkpoints preserved the post-refresh graph
  without a release boundary. This is direct before-state evidence for the
  memory plan's deferred-deletion and page-leave requirements.
- Query boundary: each rebuild performed one class query returning 96 rows,
  96 class-information queries returning 96 rows and 792 schedule rows,
  96 teacher queries returning 96 rows, and 96 roster queries. The flushed
  roster trace across the three loads records 288 queries, 7,200 result rows,
  and 21,600 cells. The fixture uses a `Student` roster column while the
  current count API recognizes only `English`/`Korean`, so its returned-student
  count is zero; this is recorded as a data-contract compatibility finding,
  not treated as a failed load.
- Evaluation impact: this closes the Sub Prep refresh/re-entry/query-size
  portion of the Phase 0 before-state audit and confirms that the eventual v2
  slice must release or reuse the class-information tree on refresh and page
  leave. It does not set the v2 budget or prove remediation. Phase 0 still
  needs the other memory-hotspot workflows, Sub Prep visual states, explicit
  Release thresholds, generated-output references, and cross-platform
  evidence. The next heavy slice should measure another large workflow while
  retaining this Sub Prep artifact as its comparison oracle.

## Progress update - 2026-09-16 (large Classes lifecycle retention)

- What changed: the heavy-route harness now drives the actual packaged Release
  `large_startup.sql` workspace through the Classes page with class selection,
  two refreshes, two leaves, and two re-entries. It records grouped and
  all-classes navigation counts, query/result sizes, editor instantiation,
  lifecycle checkpoints, and process memory. The retained artifact is
  `docs/qt-rewrite/visual-baseline/release/large-classes-boundary/`.
- Evidence: the Release child completed normally with
  `workflow-complete` at `12,197 ms` and `settled-1s` at `13,233 ms`.
  Peak working set was `410,247,168` bytes and peak private usage was
  `453,857,280` bytes. The final settled checkpoint retained
  `402,075,648` working-set bytes and `426,168,320` private-usage bytes.
- Classes boundary: the page loaded 96 source and visible classes in four
  grade groups, rendering 192 class-tab placeholders across grouped and All
  navigation and 473 navigation descendants. Across the initial load and two
  refreshes it performed three class queries returning 288 rows, 288
  class-information queries returning 288 rows and 2,376 schedule rows, and
  288 teacher queries returning 288 rows. One reusable editor and one loaded
  editor-class state remained instantiated throughout the lifecycle.
- Retention finding: unlike the Sub Prep page, the Classes navigation tree
  stayed bounded at 473 descendants after both refreshes and re-entries. The
  process working set still rose from `231,948,288` bytes at entry to
  `273,227,776` bytes at lifecycle completion, so the result is a measured
  current-product boundary rather than proof of a v2 budget. The v2 work still
  needs the compact model/view contract and explicit page-release policy, but
  it should not assume that Classes has the same deferred-delete growth as
  Sub Prep.
- Evaluation impact: this closes the large Classes entry/refresh/re-entry
  before-state measurement and separates its bounded navigation behavior from
  the Sub Prep retention defect. Phase 0 remains open for schedule/calendar
  imports, speaking batches, transfer, staff directory, PDF/report operations,
  Sub Prep visuals, explicit Release thresholds, generated outputs, and
  cross-platform evidence. The next heavy slice should target the remaining
  large-workflow matrix while retaining both the Sub Prep and Classes artifacts.

## Progress update - 2026-09-16 (large Schedule lifecycle retention)

- What changed: the heavy-route harness now drives the standalone Schedule
  page in the packaged Release `large_startup.sql` workspace through two
  refreshes, two leaves, and two re-entries. It records the current schedule
  model/table shape alongside the existing renderer creation, removal, and
  deferred-deletion counters. The retained artifact is
  `docs/qt-rewrite/visual-baseline/release/large-schedule-boundary/`.
- Evidence: the Release child completed normally with
  `workflow-complete` at `10,109 ms` and `settled-1s` at `11,144 ms`.
  The schedule lifecycle entry was `201,711,616` working-set bytes and
  `187,531,264` private-usage bytes; lifecycle completion was
  `203,198,464` working-set bytes and `189,116,416` private-usage bytes.
  The full route peak, which also includes later pages, was
  `410,066,944` working-set bytes and `453,238,784` private-usage bytes.
- Schedule boundary: the current standalone page holds seven model/table
  rows, 49 model cells, 768 schedule entries, eight table columns, seven time
  column items, 49 cell widgets, and 96 visible classes at every lifecycle
  checkpoint. The process-level schedule counters show two live/created
  ScheduleWidgets (the standalone and workspace schedules), 98 created cell
  widgets, and zero cumulative removals or deferred deletions during the
  repeated refreshes. The page's current table shape is therefore bounded,
  while the legacy presentation still uses per-cell widgets and needs the
  Phase 7E model/delegate migration.
- Evaluation impact: this closes the repeated standalone Schedule refresh,
  leave, re-entry, and current-render-shape before-state measurement. It does
  not close large workbook review or calendar import, and it does not set the
  v2 operation budget. Phase 0 still needs those operation-scoped workflows,
  speaking/transfer/staff/PDF measurements, Sub Prep visuals, explicit
  Release thresholds, generated outputs, and cross-platform evidence. The
  next heavy slice should target a large import/review operation rather than
  treating the bounded schedule refresh result as remediation.

## Progress update - 2026-09-16 (large Schedule Import lifecycle retention)

- What changed: the heavy-route harness now runs the actual packaged Windows
  x64 Release executable against the 96-class workspace and a deterministic
  two-sheet workbook containing five user blocks and 96 parsed class
  candidates. Startup profiling records raw-workbook, parsed-workbook, staged
  review, review-release, cancellation, and operation-release boundaries, with
  process traces and retained source/review captures.
- Evidence: the child completed normally. Parse/review/post-review-release/
  post-release checkpoints were `3,684`/`4,489`/`4,563`/`4,597 ms`; the review
  held 20 preview entries with five teacher controls and 20 class controls.
  Raw workbook bytes were not retained after parsing, the parsed workbook was
  retained through review, review controls were released on cancellation, and
  the workbook was released after the source dialog closed. The full route
  reached `workflow-complete` at `10,760 ms` and `settled-1s` at `11,795 ms`,
  with route-wide peak working set/private usage of `418,152,448`/
  `461,545,472` bytes. The retained artifact is
  `docs/qt-rewrite/visual-baseline/release/large-schedule-import-boundary/`.
- Evaluation impact: this closes the large Schedule Import parse/review/
  cancel/cleanup before-state required by the Memory Hotspot Remediation Plan.
  It does not measure apply/commit cleanup, large Calendar import, or establish
  a v2 operation budget. Phase 0 remains open for those workflows, speaking/
  transfer/staff/PDF measurements, Sub Prep visuals, explicit Release
  thresholds, generated outputs, and cross-platform evidence.
- Decision and next heavy slice: retain this generated workbook and lifecycle
  report as the Schedule Import before-state oracle. Continue Phase 0 with a
  large Calendar import/open-close boundary, then return to the remaining
  apply/commit and batch-operation gaps before any v2 remediation slice.

## Progress update - 2026-09-16 (large Calendar Import lifecycle retention)

- What changed: the heavy-route harness now opens the real Calendar child tab,
  triggers the Preferences dialog through the in-process action path, serves a
  deterministic two-sheet workbook through a local HTTP response, applies the
  parsed events to the 96-class workspace, closes Preferences, and waits for
  Calendar cache refresh to settle. The profiler records response bytes,
  workbook cells/merged ranges/styles, parsed/skipped events, existing-event
  query size, save/apply counts, representation lifetimes, Calendar cache
  ranges, and page/widget/model counts.
- Evidence: the packaged Windows x64 Release child completed normally. The
  16,284-byte workbook contained 2 sheets, 382 cells, 12 merged ranges, and 4
  styles. The parser produced 261 events and skipped 104 weekend entries; the
  import queried 180 existing events and saved 261 new events. Response bytes
  were no longer retained at workbook parse, the workbook and event list were
  retained through apply, and raw bytes/workbook/events/operation were all
  released at the operation-release checkpoint. Calendar then settled at 22
  cached events, 22 date buckets, one loaded range, and one retained range.
- Timing and memory: response/parse/events/save/apply/release/page-refresh
  checkpoints were `3,354`/`3,386`/`3,479`/`3,543`/`3,582`/`3,638`/
  `3,813 ms`; `workflow-complete` was `11,227 ms` and `settled-1s` was
  `12,263 ms`. Route-wide peak working set/private usage were
  `416,530,432`/`457,003,008` bytes. The retained evidence is
  `docs/qt-rewrite/visual-baseline/release/large-calendar-import-boundary/`.
- Evaluation impact: this closes the Calendar import/open/close/apply/cache
  refresh before-state required by the Memory Hotspot Remediation Plan. It
  confirms that the current service holds a raw response, parsed workbook,
  parsed event list, existing-event result, and save list across distinct
  stages, while the page cache owns a separate post-apply refresh. This does
  not set the v2 memory budget or prove that the eventual importer can share
  or stream those representations.
- Decision and next heavy slice: retain this workbook and report as the
  Calendar Import before-state oracle. Phase 0 remains open for Schedule
  Import apply/commit confirmation, speaking batch, class transfer, staff
  directory, PDF/report operation measurements, Sub Prep visuals, explicit
  Release thresholds, generated-output coverage, and cross-platform evidence.
  Continue with the next heavy operation while retaining the Sub Prep,
  Classes, Schedule, Schedule Import, and Calendar artifacts as comparison
  inputs; do not begin v2 remediation yet.

## Progress update - 2026-09-16 (large Schedule Import apply lifecycle retention)

- What changed: the heavy-route harness now runs a separate apply-mode variant
  of the deterministic large Schedule Import workbook. It reaches the actual
  review Import action, auto-confirms the product prompt in-process, commits
  the repository transaction, releases the review/source ownership graph, and
  refreshes the visible Schedule page. The profiler records existing teacher,
  class, and class-information result sizes, final class/schedule-row counts,
  resolution counts, committed summary counts, representation release, and
  post-commit Schedule metrics.
- Evidence: the packaged Windows x64 Release child completed normally. The
  workbook contained 96 class candidates and produced 20 preview entries with
  five teacher controls and 20 class controls. Apply loaded 24 existing
  teachers, 96 existing classes, and 96 class-information records, then
  committed five teachers, 20 classes, and 20 schedule rows while clearing 96
  prior schedules. The review and workbook retention flags were false after
  the committed operation, and the refreshed Schedule page showed 20 visible
  classes. The retained artifact is
  `docs/qt-rewrite/visual-baseline/release/large-schedule-import-apply-boundary/`.
- Timing and memory: apply/release/refresh checkpoints were
  `4,568`/`4,635`/`4,681 ms`; `workflow-complete` was `9,218 ms` and
  `settled-1s` was `10,253 ms`. Route-wide peak working set/private usage
  were `361,115,648`/`403,337,216` bytes.
- Evaluation impact: this closes the Schedule Import transaction/apply,
  cleanup, and post-commit refresh before-state required by the Memory Hotspot
  Remediation Plan. It also confirms that the current path retains the source
  workbook and review graph through resolution, then loads full existing
  teacher/class/class-information results and a derived final schedule map
  before commit. The measurement still does not define the v2 memory budget
  or justify replacing these representations before the Phase 7E design work.
- Decision and next heavy slice: retain the cancel and apply workbooks/reports
  as separate Schedule Import before-state oracles. Phase 0 remains open for
  speaking batch, class transfer, staff directory, PDF/report operation
  measurements, Sub Prep visuals, explicit Release thresholds,
  generated-output coverage, and cross-platform evidence. Continue with the
  next heavy operation while retaining all prior artifacts; do not begin v2
  remediation yet.

## Progress update - 2026-09-16 (large Class Transfer lifecycle retention)

- What changed: the heavy-route harness now runs the packaged Windows x64
  Release executable against the 96-class workspace and a generated
  1,716,291-byte multi-class transfer package. It reaches JSON load, preview,
  the actual Class Import review dialog, transaction apply, dialog cleanup,
  operation release, and Classes refresh while recording package counts,
  matching/query sizes, resolution counts, destination before/after counts,
  memory, and representation lifetimes. The route also exposed and fixed a
  real file-import boundary defect: transfer JSON intentionally omits local
  class IDs, so the application service now validates a decoded `-1` identity
  with a non-persisted placeholder before the repository assigns the new
  destination ID.
- Evidence: the child completed normally. The package contains 12 teachers,
  48 classes, 288 roster columns, 1,440 roster rows, 8,640 roster cells, 96
  speaking evaluations, 2,400 evaluation rows, 26,400 evaluation cells, and
  48 schedule rows. Review prepared 12 teacher controls and 48 class controls
  with zero inferred class or teacher matches. Apply created 12 teachers and
  48 classes, moving the destination from 24/96 teachers/classes to 36/144;
  the refreshed Classes page showed 144 visible classes. Raw JSON bytes and
  the decoded JSON document were released after package load; package,
  preview, dialog, and operation ownership were all false at post-release.
- Timing and memory: transfer operation start/review/post-release/page-refresh
  checkpoints were `4,723`/`5,500`/`6,431`/`9,618 ms`. The operation rose from
  `231,911,424` working/`220,925,952` private bytes at start to a
  `250,601,472`/`237,604,864` review peak, then measured
  `242,741,248`/`234,180,608` at post-release and
  `262,430,720`/`251,817,984` after the Classes refresh. The full heavy route
  reached `workflow-complete` at `15,423 ms` and `settled-1s` at `16,458 ms`,
  with route-wide peak working/private usage of
  `476,622,848`/`519,741,440` bytes. The retained artifact is
  `docs/qt-rewrite/visual-baseline/release/large-class-transfer-boundary/`.
- Evaluation impact: this closes the multi-class Class Transfer
  review/commit/release/refresh before-state required by the Memory Hotspot
  Remediation Plan and proves that the real file path can complete with the
  current ownership boundaries. It does not establish a v2 operation budget
  or prove that the package, dialog copy, matching results, and transaction
  staging are bounded in the eventual streaming/compact design.
- Decision and next heavy slice: retain this package and lifecycle report as
  the Class Transfer before-state oracle. Phase 0 remains open for the large
  speaking-evaluation batch, staff directory, PDF/report operation
  measurements, Sub Prep visual states, explicit Release thresholds,
  generated-output coverage, and cross-platform evidence. Continue with the
  next heavy operation while retaining all prior artifacts; do not begin v2
  remediation yet.

## Progress update - 2026-09-16 (large Speaking Evaluation batch lifecycle retention)

- What changed: the heavy-route harness now augments a generated copy of the
  96-class workspace with 96 canonical `Winter` speaking evaluations and 2,400
  evaluation rows. It runs the actual packaged Windows x64 Release page,
  selected evaluation model, report review dialog, batch export dialog, AI
  prompt/review dialog, accepted-comment apply, cleanup, and page refresh.
  Profiler fields cover class-tab/model shape, report text, AI prompt and
  response sizes, review-table item counts, PDF/archive output, operation
  retention, and release checkpoints.
- Evidence: the child exited normally. The page exposed 96 visible class tabs
  across four navigation widgets, four evaluation tabs, and a 25-by-11 model
  (275 cells). The selected batch produced 25 report records, 25 PDFs, and a
  51,634,180-byte archive. AI review parsed 25 rows across five columns, with
  125 review items and 25 accepted comments. The report/AI/export operation
  retention flags were all false after cleanup and the refreshed page. The
  retained artifact is
  `docs/qt-rewrite/visual-baseline/release/large-speaking-evaluation-boundary/`.
- Timing and memory: operation start/page/report/export-complete/AI-response/
  release/page-refresh checkpoints were
  `4,558`/`5,597`/`5,637`/`9,646`/`9,736`/`9,941`/`9,993 ms`. The speaking
  operation reached `384,593,920` working-set bytes and
  `292,061,184` private-usage bytes before release, then measured
  `307,388,416`/`289,484,800` at operation release. The full route reached
  `workflow-complete` at `14,893 ms` and `settled-1s` at `15,923 ms`, with
  route-wide peak working/private usage of
  `471,343,104`/`525,725,696` bytes.
- Evaluation impact: this closes the large speaking-batch review/export/
  cleanup before-state required by the Memory Hotspot Remediation Plan's
  report and AI workflow entry. It proves the current route's cardinalities,
  output contract, and release checkpoints; it does not prove a bounded v2
  model/view, one-record-at-a-time report loader, chunked output pipeline, or
  a Phase 9 memory budget. The new Phase 0 evidence therefore supports the
  later Phase 7H/Phase 8 ownership work without starting that remediation
  early.
- Decision and next heavy slice: retain the augmented fixture and output
  package as the Speaking Evaluation before-state oracle. Phase 0 remains
  open for staff-directory and PDF/report measurements, Sub Prep visual states,
  explicit Release thresholds, remaining generated-output references, and
  cross-platform evidence. Continue with the next heavy operation while
  retaining all prior artifacts; do not begin v2 remediation yet.

## Progress update - 2026-09-16 (large Staff Directory lifecycle retention)

- What changed: the heavy-route harness now adds deterministic 96-row Native
  English Teacher and GS Team directory data to a generated copy of the large
  workspace. It drives both actual packaged Windows x64 Release pages through
  directory load, two refreshes, leave to My Workspace, re-entry, and operation
  release while recording table shape, text payload size, page-widget count,
  lifecycle events, process memory, and the flushed workflow trace.
- Evidence: Native English Teachers exposes a 96-by-6 table with 576 cell
  items, 7,314 bytes of cell text, and 85 page-widget descendants. GS Team
  exposes a 96-by-5 table with 480 cell items, 4,940 bytes of text, and the
  same 85 descendants. Both pages completed two refreshes, one leave, and one
  re-entry; the child exited normally. The retained artifact is
  `docs/qt-rewrite/visual-baseline/release/large-staff-directory-boundary/`.
- Timing and memory: Native operation start/page-prepared/refresh-2/left/
  re-entered/released checkpoints were `5,314`/`5,344`/`5,425`/`5,459`/
  `5,490`/`5,519 ms`; GS checkpoints were `5,784`/`5,815`/`5,889`/`5,923`/
  `5,955`/`5,984 ms`. The staff operation peaked at `242,855,936` working-set
  bytes and `235,036,672` private-usage bytes. The full heavy route reached
  `workflow-complete` at `9,782 ms` and `settled-1s` at `10,814 ms`, with
  route-wide peak working/private usage of `410,624,000`/`452,751,360` bytes.
- Memory-hotspot finding: after operation release, both operation-retention
  flags are false but both table-retention flags remain true through the final
  checkpoint. This is direct current-product before-state evidence that the
  Staff Directory page keeps its table graph under PageManager retention; it
  identifies the later v2 release/reuse work but does not implement it or set
  its budget. The Staff Directory before-state portion of the Memory Hotspot
  Remediation Plan is now closed.
- Decision and next heavy slice: retain the generated fixture, directory
  captures, lifecycle trace, and report as the Staff Directory before-state
  oracle. Phase 0 remains open for PDF/report operation measurements, Sub Prep
  visual states, explicit Release thresholds, remaining generated-output
  references, per-resource decoded/resident lifecycle traces, and
  cross-platform evidence. Continue with the next heavy operation while
  retaining all prior artifacts; do not begin v2 remediation yet.

## Progress update - 2026-09-16 (large Sub Prep output lifecycle retention)

- What changed: the heavy-route harness now composes the actual packaged
  Windows x64 Release Sub Prep page lifecycle with its real generation dialog,
  package service, PDF outputs, first-page decoding, and release boundary. The
  in-process Qt controller configures and captures the modal dialog and watches
  for product warning dialogs, so this slice remains runnable without native
  desktop automation.
- Fixture/evidence: the test retains the 96-class, 7,200-cell workspace while
  using a valid By Day output schedule with 30 selected classes distributed
  one-per-slot across five weekdays and six supported times. Roster columns
  are normalized to the service's English/Korean output contract so the
  generated reference is populated. The child exited normally and retained
  the dialog capture, two generated PDFs, two decoded first-page PNGs, the
  generated `.tps` fixture, manifest, trace, and profiler report under
  `docs/qt-rewrite/visual-baseline/release/large-sub-prep-output-boundary/`.
- Evidence: the real route generated the Sub Prep document and By Day roster
  PDFs, totaling two documents, 17 pages, and `139,650` bytes. First-page
  decoding totaled `17,399,680` bytes. Output start/generated/release
  checkpoints were `7,965`/`9,024`/`9,060 ms`; the full route reached
  `workflow-complete` at `11,442 ms` and `settled-1s` at `12,472 ms`. The
  output boundary measured `489,000,960` working-set bytes and
  `480,849,920` private-usage bytes at generation; output failure and
  operation-retention flags were false after release, with
  `livePdfDocumentCount=0`.
- Evaluation impact: this closes the actual packaged Sub Prep generated-output
  before-state and the corresponding PDF decode/release evidence required by
  the Memory Hotspot Remediation Plan. It does not set a v2 budget or prove
  remediation. The valid By Day fixture is deliberate: the unmodified
  96-class schedule correctly rejects overlapping By Day slots, so the output
  oracle preserves that product validation instead of bypassing it.
- Decision and next heavy slice: keep Phase 0 open for Sub Prep selected/
  changed/empty/language/theme visual states, explicit Release thresholds,
  remaining report/substitute/PowerPoint output references, per-resource
  decoded/resident lifecycle traces, and cross-platform Release evidence.
  Continue with the next heavy Phase 0 slice; do not begin v2 remediation or
  Phase 1 yet.

## Progress update - 2026-09-16 (large Sub Prep visual-state references)

- What changed: the heavy-route harness now has a dedicated in-process Qt
  visual-state mode. It enters the actual packaged Windows x64 Release Sub Prep
  page, scrolls the class-information content into view, captures the initial
  selected class, changes selection, and captures the resulting state. A
  second 96-class database with regular and intensive meeting times cleared
  supplies the real empty class-information state; no native desktop
  automation is required.
- Evidence: the populated Release route ran English/Korean × light/dark, with
  96 visible classes and a selected-class transition from `25` to `1` in each
  variant. The four routes completed between `9,773` and `10,179 ms`; peak
  working sets ranged from `408,436,736` to `410,849,280` bytes. The empty
  route reported zero visible classes and selected class `-1`, completed at
  `7,643 ms`, and retained a `292,823,040`-byte peak working set. Screenshots,
  generated fixtures, metrics, traces, and the manifest are retained under
  `docs/qt-rewrite/visual-baseline/release/large-sub-prep-visual-states/`.
- Evaluation impact: this closes the selected, changed-selection, empty, and
  both-language/theme Sub Prep visual-reference requirement from the updated
  Phase 0 audit. The page-scroll diagnostic hook is test-only behavior and
  does not alter normal navigation. This is still current-product before-state
  evidence; it does not set the v2 Release memory budget or implement the 7J
  remediation.
- Decision and next heavy slice: Phase 0 remains open for editing/read-only
  and dialog/loading/error visual states, explicit Release thresholds,
  remaining report/substitute/PowerPoint output references, per-resource
  decoded/resident lifecycle traces, and macOS universal Release evidence.
  Continue the next heavy Phase 0 slice; do not begin v2
  remediation or Phase 1 yet.

## Progress update - 2026-09-16 (packaged Release memory threshold contract)

- What changed: Phase 0 now records the exact primary-memory contract required
  by the Memory Hotspot Remediation update. The end-of-rewrite normal
  resident/idle/post-release target is strictly below `262,144,000` bytes
  (`250 MiB`) Windows working set. Phase 0 records the current legacy distance
  to that target rather than requiring the legacy widget graph to pass it. The
  separate bounded transient-operation ceiling is a temporary diagnostic
  ceiling, strictly below `536,870,912` bytes (`512 MiB`), with private bytes,
  commit, peak, handles, and threads retained as secondary diagnostics. The
  contract is documented in `docs/qt-rewrite/phase-0-memory-thresholds.md`.
- Heavy-route enforcement: the packaged Release Sub Prep visual-state probe
  now evaluates both comparisons and writes the target/ceiling classification
  into its retained manifest. All four populated visual routes and the empty
  route currently remain above the final target at their retained peak
  checkpoints while staying below the temporary transient ceiling; this is an
  intentional current-product finding, not a Phase 0 failure or a v2 pass.
  The highest retained visual-route peak was `410,849,280` bytes, and the
  previously retained heavy Sub Prep output peak was `489,000,960` bytes, both
  below the temporary transient ceiling.
- Evaluation impact: the Phase 0 budget requirement is now explicit and
  machine-recorded on a real packaged Release heavy route. The contract does
  not authorize accepting the current full widget graph, and it does not move
  remediation work into Phase 0. Each later phase must avoid regressions and
  should lower the affected heavy-route measurements. Phase 0 remains open
  for the remaining output/parity references, per-resource decoded/resident
  traces, and macOS universal Release evidence.
- Decision and next heavy slice: retain the threshold-classified visual
  artifacts as the before-state budget oracle and continue with the next
  heavy Phase 0 evidence gap; do not begin v2 remediation or Phase 1 yet.

## Progress update - 2026-09-16 (packaged Release resource ownership trace)

- What changed: the packaged startup harness now has an explicit
  `--startup-performance-resource-trace` mode. It runs after the full
  large-workspace navigation/PDF workflow, enumerates each required RCC pack
  and embedded asset, records logical installed payload bytes, image dimensions
  and potential decoded bytes, classifies startup versus on-demand ownership,
  and records whether each resource-pack lease returns to its pre-trace mount
  state. PDF and PPTX bodies are catalogued without loading them into a
  decoder.
- Heavy-route evidence: the packaged Windows x64 Release route completed
  normally. It retained 189 entries totaling `62,781,401` logical installed
  bytes, 34 decoded-image candidates totaling `147,851,916` potential decoded
  bytes, 83 on-demand entries, and zero decoded bytes for catalog PDF/PPTX
  entries. The route peak was `482,676,736` working-set bytes and
  `452,280,320` private-usage bytes; the trace completed at `9,646 ms`, with
  the route settled at `10,676 ms`. Evidence is under
  `docs/qt-rewrite/visual-baseline/release/large-resource-trace-boundary/`.
- Finding: the six required RCC packs acquired and returned to their prior
  mount state. `roster-designs` is declared by the current resource-pack
  manager but has no source directory or packaged `.rcc` in this checkout; the
  trace records it explicitly as `required=false` and unavailable. Preserve
  this as a Phase 4 packaging decision rather than treating the missing asset
  as a successful load.
- Evaluation impact: the Phase 0 per-resource payload/decoded/deferred-load
  and pack-lease evidence gap is closed for the Windows x64 packaged heavy
  route. The per-resource decoded values are potential per-asset sizes, not a
  claim that all images are simultaneously resident; process working set
  remains the actual memory authority. Phase 0 remains open for the remaining
  visual/output references, feature page/object gaps, and cross-platform
  Release evidence.
- Decision and next heavy slice: retain this resource trace as the ownership
  oracle and continue with the next heavy Phase 0 evidence gap; do not begin
  v2 remediation or Phase 1 yet.

## Progress update - 2026-09-16 (large packaged PDF viewer visual states)

- What changed: the existing in-process PDF lifecycle route now captures and
  checkpoints the empty catalog-ready viewer before any document load, the
  post-release closed viewer, an expected missing-file error state, and the
  reopened document in addition to the existing open/render/release path. The
  invalid load is deliberately cleared before reopen, so the error state cannot
  retain a document body or resource lease.
- Heavy-route evidence: the real packaged Windows x64 Release executable ran
  against the 96-class workspace and retained five captures under
  `docs/qt-rewrite/visual-baseline/release/large-pdf-viewer-visual-states/`.
  The route reached catalog-ready/open/closed/error/reopened checkpoints at
  `7,976`/`8,238`/`8,485`/`8,526`/`8,761 ms`, completed at `8,990 ms`, and
  settled at `10,448 ms`. Peak memory was `410,120,192` working-set bytes and
  `452,141,056` private-usage bytes. The expected error capture visibly says
  the file was not found, while the error and closed checkpoints both report
  zero active PDF documents.
- Evaluation impact: this closes the large packaged document-viewer
  catalog-ready, loading, ready, closed/released, error, and reopened visual
  reference gap without native desktop automation. The route still uses
  QtPdf's real load/render/close path and remains before-state evidence, not a
  v2 memory acceptance result. Phase 0 remains open for the remaining
  report/substitute/PowerPoint output references, feature visual states, and
  cross-platform Release evidence.
- Decision and next heavy slice: retain the five viewer captures and lifecycle
  metrics as the document-viewer parity oracle and continue with the next
  heavy Phase 0 evidence gap; do not begin v2 remediation or Phase 1 yet.

## Progress update - 2026-09-16 (large Speaking Evaluation PowerPoint renderer reference)

- What changed: the heavy-route harness now exposes the real Speaking
  Evaluation export dialog's PowerPoint renderer option through an object name
  and captures the selected renderer state in-process. The route records a
  ready checkpoint with `externalAutomation=not-run`; it deliberately does not
  invoke external Office automation from the Windows offscreen test process.
- Heavy-route evidence: the packaged Windows x64 Release route ran the full
  96-class Speaking Evaluation workflow, retained the renderer-selection
  screenshot alongside the existing PDF/archive and report/AI captures, and
  completed normally. The retained manifest records
  `powerPointRendererVisualReference=true` and
  `powerPointAutomationExecuted=false`.
- Evaluation impact: the renderer choice and fallback-warning UI are now part
  of the current-product output oracle, while the platform-specific external
  automation path remains an explicit evidence gap. This slice does not set a
  v2 output budget or implement the PowerPoint rewrite path.
- Decision and next heavy slice: keep Phase 0 open for platform-specific
  Office automation/cross-platform evidence, remaining generated-output and
  feature-state references, and the final memory trend record. Continue with
  the next heavy Phase 0 evidence gap; do not begin v2 remediation or Phase 1
  yet.

## Progress update - 2026-09-16 (large Sub Prep output-dialog validation reference)

- What changed: the existing heavy-route Sub Prep generation controller now
  drives the real modal dialog into its validation-error state by clearing both
  output options. It captures the resulting message and disabled OK action,
  records a checkpoint/trace entry, restores valid controls, and completes the
  normal package/PDF lifecycle. This remains in-process Qt automation and does
  not require native desktop control.
- Heavy-route evidence: the packaged Windows x64 Release 96-class route passed
  with validation/generated/release checkpoints at `8,747`/`9,296`/`9,337 ms`.
  The retained validation capture visibly shows
  `Select Create Folder and/or Print Paper Copies to continue.` with OK
  disabled. Normal generation still produced two PDFs over 17 pages and the
  post-release route retained zero live PDF documents; the output peak was
  `498,176,000` working-set bytes, below the temporary `512 MiB` diagnostic
  ceiling and still observational before-state evidence.
- Evaluation impact: the Sub Prep output-dialog validation/error reference is
  now retained. Editing/read-only/loading parity, other feature error states,
  cross-platform Release evidence, and v2 ownership/memory work remain open.
- Decision and next heavy slice: continue Phase 0 with the next large-route
  evidence gap; do not begin v2 remediation or Phase 1 yet.

## Progress update - 2026-09-16 (large Sub Prep editing/read-only reference)

- What changed: the packaged heavy-route Sub Prep visual probe now captures
  the top-of-page settings state and asserts the current mixed editability
  contract in-process. Four named text editors remain editable while the four
  campus fields and two Zoom fields remain read-only; the existing schedule and
  class-information states are unchanged.
- Heavy-route evidence: the Windows x64 Release 96-class route retained
  `sub-prep-editing-read-only.png` for each English/Korean and light/dark
  populated variant. The four populated routes completed between `7,079` and
  `7,364 ms`, with peak working sets from `408,637,440` to `410,324,992` bytes;
  the empty route remained covered and completed at `5,085 ms` with a
  `292,331,520`-byte peak. Each populated checkpoint records
  `editableTextEdits=4`, `readOnlyLineEdits=6`, `contractValid=true`, and
  `captured=true`.
- Evaluation impact: the Sub Prep editing/read-only visual and runtime
  contract is now retained as current-product evidence. Loading and other
  feature error states, remaining generated-output references, cross-platform
  Release evidence, and v2 ownership/memory work remain open.
- Decision and next heavy slice: continue Phase 0 with the next packaged
  Windows x64 Release heavy-route evidence gap; do not begin v2 remediation or
  Phase 1 yet.

## Progress update - 2026-09-16 (large Classes visual-state references)

- What changed: the existing packaged Classes lifecycle now has an opt-in
  in-process visual capture path. It captures the real populated entry state,
  selected class `96`, and selected class `1` after re-entry, while retaining
  the existing selection, refresh, leave, and re-entry checks. The capture is
  enabled only when `CLASSMNGR_STARTUP_CLASSES_VISUAL_OUTPUT_DIR` is set, so
  normal startup and lifecycle runs remain unchanged.
- Heavy-route evidence: the Windows x64 Release 96-class route completed in
  all four English/Korean and light/dark variants. Each variant retained the
  three Classes frames plus startup-complete, reported 96 visible classes and
  473 navigation descendants, and recorded `captured=true` at each visual
  checkpoint. Workflow completion ranged from `11,367` to `11,415 ms`, with
  peak working sets from `409,907,200` to `412,454,912` bytes. The retained
  artifact is under
  `docs/qt-rewrite/visual-baseline/release/large-classes-visual-states/`.
- Evaluation impact: populated Classes visual evidence is now retained for
  both language/theme axes and real selection/re-entry states. This remains
  current-product before-state evidence; the final `<250 MiB` target and the
  Classes v2 model/view ownership work are still future-phase obligations.
  Empty/dialog/error states, remaining feature visuals, cross-platform Release
  evidence, and generated-output gaps remain open.
- Decision and next heavy slice: continue Phase 0 with the next packaged
  Windows x64 Release heavy-route evidence gap; do not begin v2 remediation or
  Phase 1 yet.

## Progress update - 2026-09-16 (large Schedule Import loading reference)

- What changed: the existing in-process Schedule Import Heavy-route controller
  now captures the real asynchronous loading state immediately after the Load
  action, with the existing event-loop poll retained as a fallback. It records
  the loading status, visible progress bar, disabled source/load controls, and
  capture result. No production dialog behavior or new fixture was added.
- Heavy-route evidence: both packaged Windows x64 Release 96-class routes
  completed normally: the conflict/cancel route and the conflict-free
  transaction/apply route each retained `schedule-import-loading.png`. Both
  checkpoints record `status=Loading workbook...`,
  `progressVisible=true`, `controlsDisabled=true`, and `captured=true`. The
  cancel/apply route peaks were `418,488,320` and `361,316,352` working-set
  bytes respectively. The retained files are under the existing
  `large-schedule-import-boundary/` and
  `large-schedule-import-apply-boundary/` directories.
- Evaluation impact: the packaged Schedule Import loading visual is now part
  of the current-product import oracle alongside parse, review, conflict,
  cancel, apply, and release evidence. This remains before-state evidence; the
  final memory target, other feature loading/error states, cross-platform
  Release evidence, and remaining generated-output gaps remain open.
- Decision and next heavy slice: continue Phase 0 with the next packaged
  Windows x64 Release Heavy-route evidence gap; do not begin v2 remediation or
  Phase 1 yet.

## Progress update - 2026-09-16 (large Schedule Import conflict-warning reference)

- What changed: the existing in-process Schedule Import Heavy-route controller
  now waits for the real conflict-warning `QMessageBox` created by the review
  dialog, captures it by its stable object name, records its visibility/text/
  disabled-Import state, and acknowledges it before the existing cancel path
  continues. The wait is enabled only for the opt-in evidence output and the
  existing event-loop controller; normal production behavior is unchanged.
- Heavy-route evidence: the cancel fixture was adjusted so four-class teacher
  groups intentionally share projected day/time slots, which makes the real
  warning path appear. The packaged Windows x64 Release cancel route retained
  `schedule-import-conflict-warning.png` and recorded
  `visible=true`, warning text beginning `Review these schedule conflicts
  before importing:`, `importEnabled=false`, and `captured=true`. The
  conflict-free apply variant still commits normally and records no warning
  visual reference. Cancel/apply peak working sets were `418,058,240` /
  `360,960,000` bytes; peak private usage was `461,225,984` /
  `402,731,008` bytes.
- Evaluation impact: the large Schedule Import current-product oracle now
  includes asynchronous loading, staged review, a real conflict-warning modal,
  cancellation, conflict-free transaction apply, cleanup, and schedule
  refresh. The warning image, generated cancel workbook, manifests, traces,
  and profiler reports are under the two existing large Schedule Import
  boundary directories. This remains before-state evidence; v2 memory and
  ownership work, generated-output gaps, cross-platform Release evidence, and
  other feature states remain open.
- Decision and next heavy slice: Phase 0 remains open. Resume with the next
  packaged Windows x64 Release Heavy-route evidence gap after this session;
  do not begin v2 remediation or Phase 1 yet.

## Progress update - 2026-09-16 (large Calendar Import loading reference)

- What changed: the existing in-process Calendar Import Heavy-route controller
  now captures the real Preferences loading state immediately after the Import
  Events action. It records the `Importing events...` status, disabled import
  control, and capture result while leaving the production path unchanged. The
  evidence probe temporarily scrolls the existing Calendar preferences tab to
  the Import section so the retained frame shows the state, then restores the
  prior scroll position.
- Heavy-route evidence: the packaged Windows x64 Release 96-class route passed
  the deterministic local HTTP workbook workflow and retained
  `calendar-import-loading.png` alongside the Preferences and Calendar frames.
  The loading/parse/page-refresh checkpoints were `3,207`/`3,262`/`3,656 ms`,
  workflow completion was `10,112 ms`, and one-second settle was `11,138 ms`.
  Peak working set/private usage was `416,694,272`/`457,310,208` bytes. The
  checkpoint records `status=Importing events...`, `controlsDisabled=true`,
  and `captured=true`.
- Evaluation impact: Calendar Import transient loading evidence is now part of
  the current-product import oracle, while cross-platform Release capture,
  remaining feature visual/error states, generated-output gaps, and v2
  ownership/memory work remain open.
- Decision and next heavy slice: continue Phase 0 with the next packaged
  Windows x64 Release Heavy-route evidence gap; do not begin v2 remediation or
  Phase 1 yet.

## Progress update - 2026-09-17 (Calendar Import parser-failure boundary)

- The Calendar Import parser-failure boundary is accepted for Phase 0 after a
  fresh Ninja Release build at `build/qt-rewrite-calendar-verify-ninja` using
  Qt `6.12.0`/x64 MSVC, with both required binaries linked. The focused route
  passed with the expected-failure opt-in and a deterministic 69-byte malformed
  local HTTP response; trace/metrics show the real
  `Import failed: The downloaded spreadsheet is missing xl/workbook.xml.`
  error, enabled Import Events, unchanged events `0 -> 0`, operation release
  before failure observation, no forbidden success checkpoints, workflow
  completion, and normal exit. It completed at `11,191 ms`/`12,228 ms`, with
  `410,251,264` working-set and `452,120,576` private-usage peak bytes.
- Evidence is retained under
  `docs/qt-rewrite/visual-baseline/release/large-calendar-import-error-boundary/`;
  the manifest references `large-calendar-import-workflow.json`. Manual
  screenshot inspection and independent Tester validation passed; the
  unchanged success route and full startup suite (Calendar opt-ins cleared)
  also passed with exit `0`. Phase 0 remains In progress; remaining
  cross-platform, feature-state, and generated-output evidence is still open,
  and no Phase 1 or v2 memory acceptance follows.

## Progress update - 2026-09-18 (combined platform route gate)

- The combined Phase 0 validator was run with `--require-exit-gate`, the full
  Windows x64 run root, and macOS evidence staged separately from the `.tar`
  and `.tar.gz` archives under `E:\MacOS-evidence\`.
- Both archive runs passed the combined gate: all 24 required route IDs on
  Windows x64 and macOS universal. Each archive matched the retained macOS
  inventory for 643 files / 462,599,685 bytes and all 156 symlink targets.
  Archive SHA-256 values are `0c0a16e5240e1b56a63c89b06a4a8a3b9395f2613284d484207ee9e21798b4b7`
  (`.tar`) and `9a84f894893d8e6c4fc0fb5f88e7c3eea410765f3d5f4dbb948442cc70d11b18`
  (`.tar.gz`).
- The route gate is complete. Overall Phase 0 remains In progress pending
  generated-output semantic review and closure of the documented remaining
  feature-state/output gaps. The user confirmed on 2026-09-18 that the retained
  visual references look correct. The legacy 250 MiB memory trend warning is
  non-failing; ARM64/Linux remain deferred ports.

## Phase 0 completion - 2026-09-18

- The user declared Phase 0 complete. This supersedes the earlier In Progress
  status and unblocks Phase 1.
- The combined 24-route gate passed for Windows x64 and macOS universal, and
  the user confirmed that the retained visual references look correct.
- Earlier evidence-gap and environment notes remain as historical context;
  they are not current Phase 0 blockers under this closure decision.
