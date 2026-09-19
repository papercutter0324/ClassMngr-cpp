# Qt Rewrite Memory Hotspot Remediation Plan

## Status

- Status: Phase A closed; Phases B-H open
- Route: Heavy
- Plan type: Cross-cutting execution plan
- Related phases: 0, 2, 3, 5, 6, 7, 8, 9, 11, 12, and 13
- Depends on: Phase 0 measurements and the v2 ownership boundaries
- Blocks: The Phase 9 Windows memory gate and final cutover
- Owner: Unassigned
- Last updated: 2026-09-20
- Current note: Phase 2 gateway work continues; this memory plan remains open.

This document turns the Phase 0 memory audit into an ordered implementation
plan. It does not create a new numbered rewrite phase or replace the existing
Sub Prep plan. Each fix remains owned by the phase that owns its data,
presentation, output, or lifecycle boundary.

## Objective

Remove the application's repeated pattern of retaining a complete data graph
and a heavyweight Qt widget graph for every record. The target is bounded
memory that grows with the visible viewport or the active operation, not with
the total number of records or the number of pages visited.

The plan must preserve:

- the current user workflow, ordering, appearance, localization, and output;
- the 96-class and other large fixtures as required stress inputs;
- file-format and import/export compatibility;
- the distinction between normal resident memory and operation-scoped peaks;
- the legacy implementation as a comparison oracle until the affected slice
  is accepted.

The bounded fixture may prove fast lifecycle semantics. It must never replace
the large fixture as the scalability and memory acceptance case.

## Design rules

1. Keep a compact application projection for lists and navigation. Do not pass
   rich domain records or widget-owned objects through every layer.
2. Load details by stable ID when selected or otherwise needed. Retain only a
   small, explicitly budgeted cache.
3. Use model/view and delegates for repeated data. Editors and child widgets
   exist only for the active item or visible interaction.
4. Make page suspension and release real ownership boundaries. A page that is
   hidden must not automatically retain every large model, editor, preview,
   or document it ever created.
5. Treat import, export, report, PDF, and transfer data as operation-scoped.
   Release each representation as soon as the next stage owns what it needs.
6. Prefer one indexed representation over repeated copies. If a compatibility
   representation is temporarily required, give it an explicit removal point.
7. Preserve data fidelity and visual parity. A smaller fixture, hidden
   feature, disabled output path, or Debug-only limit is not a memory fix.
8. Instrument before and after each slice. Memory numbers without lifecycle,
   row-count, widget-count, and ownership evidence are insufficient.

## Hotspot ownership matrix

| Audited hotspot | Primary implementation owner | Supporting phases | Required end state |
|---|---|---|---|
| My Classes creates a complete page tree for every class | Phase 7B | 2, 3, 6, 9, 11 | Compact class model, one reusable selected-class detail view, no per-class page tree |
| Schedule import review retains workbook, preview, and all resolution controls | Phase 7E | 2, 3, 6, 8, 9, 11 | Staged workbook pipeline, indexed matching data, model-backed review, bounded preview |
| Calendar/workbook parsing overlaps raw, decoded, cell, and derived representations | Phases 3 and 7F | 2, 8, 9, 11 | One-pass or staged parser with explicit representation release and no compatibility cell duplication in v2 |
| Schedule table allocates widgets per cell and defers cleanup | Phase 7E | 6, 9, 11 | QTableView/model/delegate presentation with editors only for active interaction |
| Classes and Speaking Evaluation materialize all class tabs | Phases 7D and 7H | 3, 6, 9, 11 | Lightweight ID/summary navigation and lazy active content |
| Sub Prep PDF rendering overlaps rich source, HTML, document, and output data | Phase 7J and Phase 8.2 | 2, 3, 9, 11 | Operation-scoped projection and bounded/chunked rendering |
| Class transfer retains full package, JSON document, bytes, and dialog copy | Phases 3.4 and 8 | 2, 7D, 9, 11 | Staged or streaming transfer with one owned package representation |
| Staff Directory and AI batch dialogs use per-cell table items | Phase 6.7 and feature slices | 7C, 7H, 9, 11 | Model/view tables and bounded batch/report state |
| My Workspace eagerly constructs hidden child pages | Phases 5.4 and 6.2 | 7B, 9, 11 | Visible-page-only construction and release/recreation on suspension |
| PageManager retains instantiated pages indefinitely | Phase 6.2 | 5, 7, 9, 11, 12 | Explicit Created/Activated/Suspended/Released lifecycle with bounded retention policy |

## Phase A — Baseline, budgets, and ownership trace

**Existing phase:** Phase 0.

Before changing allocation boundaries, complete the audit for every hotspot.
The current Sub Prep trace remains valid evidence of the legacy shape; it is
not a reason to weaken the heavy route.

### Work

- Record startup-ready, first-render, settled-idle, page-enter, page-leave,
  refresh, operation-start, operation-end, and post-release checkpoints.
- Add or standardize deterministic large inputs for:
  - the 96-class / 8-slot workspace;
  - a large schedule workbook and review;
  - a large calendar workbook;
  - a multi-class transfer package;
  - a large speaking-evaluation batch;
  - repeated class navigation and repeated page entry/leave.
- Capture working set, private bytes, commit, peak values, handle count, and
  thread count, plus process outcome.
- Capture feature-specific counts: page instances, model rows, table items,
  cell widgets, editors, loaded documents, decoded image bytes, workbook
  cells, and retained package/report records.
- Record query/result sizes and the lifetime of raw bytes, decoded data, and
  derived projections where the current instrumentation permits it.
- Establish separate budgets for normal resident memory and operation peaks.
  The exact values are recorded in the Phase 0 baseline and enforced by
  Phase 9; they must not be invented after an implementation fails.
- Retain failure or high-memory artifacts from the current implementation,
  including the large Sub Prep route, as before-state evidence.

### Exit evidence

- Every hotspot has a named scenario, fixture, checkpoint sequence, and
  before-state measurement.
- The large fixture remains the required stress input.
- The measurement report distinguishes a real bounded implementation from a
  route that merely terminates successfully.

### Phase A / existing Phase 0 closure — 2026-09-18

Phase A is closed as the existing Phase 0 baseline and ownership step. The user
closed Phase 0 after the combined packaged Release gate passed all 24 routes on
Windows x64 and macOS universal and the retained visual references were
reviewed and confirmed. See the [Phase 0 baseline and evidence
log](../../docs/qt-rewrite/phase-0-baseline.md) and [Phase 0 memory
thresholds](../../docs/qt-rewrite/phase-0-memory-thresholds.md).

The retained [Phase 0 route records](../../docs/qt-rewrite/phase-0-evidence/)
and [packaged Release artifacts](../../docs/qt-rewrite/visual-baseline/release/)
cover the ten hotspot rows and the large fixtures/routes. The final
normal-resident target is strictly `<262,144,000` bytes (`250 MiB`); the
transient diagnostic ceiling is strictly `<536,870,912` bytes (`512 MiB`).
Legacy high-memory results remain before-state trend evidence, and non-blocking
deferred platform/state gaps are historical and non-gating under the closure
decision. Only Phase A is closed: Phases B-H remain open, and Phase 2
continues.

## Phase B — Compact contracts and lifetime boundaries

**Existing phases:** Phases 2 and 3.

Define the data contracts before replacing views. The UI must consume
application projections rather than broad compatibility-service results.

### Shared contracts

Introduce v2 contracts equivalent to the following; names may follow the
established v2 naming conventions:

- ClassSummary: ID, teacher ID, grade, level, display label, meeting text,
  student count, and small presentation values.
- ClassDetails: selected-class notes, teacher facilities/notes, and the
  currently required detail values.
- TeacherSummaryIndex: one compact record per referenced teacher, addressable
  by ID.
- ScheduleViewProjection: compact visible rows/cells suitable for a model,
  with no child-widget ownership.
- ImportReviewSession: a bounded review plan, matching indexes, diagnostics,
  and compact user decisions rather than the original workbook plus all
  derived objects.
- TransferReader/TransferWriter: operation-scoped records that can be
  processed incrementally or in bounded stages.
- ReportJob/PdfRenderSource: selected data needed for output, independent of
  the page's widget tree.

### Rules for the contracts

- Return only columns and relationships needed by the current use case.
- Use IDs and indexes to share repeated teachers, classes, and strings.
- Aggregate roster counts when a screen needs counts, rather than loading
  roster cells.
- Make ownership and release behavior explicit in each operation contract.
- Do not expose DataService compatibility methods as the v2 API.
- Keep Qt Widgets and page pointers out of domain and application contracts.

### Exit evidence

- Contract tests run without constructing the main window.
- A 96-class summary is compact and does not contain one rich record graph per
  class.
- Import, transfer, report, and PDF contracts specify when each large
  representation is released.

## Phase C — Shared lifecycle and presentation primitives

**Existing phases:** Phases 5 and 6.

Build the mechanisms that all feature slices will use. The purpose is to make
the efficient ownership model the default rather than a feature-specific
exception.

### Page lifecycle

- Make page registration factory-only; registering a page must not construct
  it.
- Implement Created, Activated, Suspended, and Released states in PageHost.
- On suspension, release transient models, editors, previews, loaded
  documents, and feature caches according to the page policy.
- Preserve persistent selection and preferences in application state or
  persistence, not only in widgets.
- Ensure a released page can be recreated with the same behavior.
- Replace indefinite PageManager retention with a documented retention policy.

### Reusable presentation primitives

- Add shared model/view helpers for compact rows, delegates, selection,
  loading, empty, error, and asynchronous refresh states.
- Add a reusable selected-record detail host for features that currently
  create one detail page per record.
- Add lazy navigation support that creates a page or editor only when selected,
  and can release it when suspended.
- Add active-cell or active-row editor factories for large tables.
- Make lifecycle diagnostics report creation, release, and model row counts.

### Exit evidence

- Navigating through placeholder pages does not construct unrelated pages.
- A representative model/view surface demonstrates constant or
  viewport-bounded child-widget creation.
- Pages can be suspended, released, and recreated without stale state.

## Phase D — Feature vertical slices

**Existing phase:** Phase 7.

Implement the following in the existing feature migration order. Each item is
a complete vertical slice through application, persistence, UI, resources,
output, localization, and tests. The slices may be developed in parallel only
after their shared contracts and primitives are accepted.

### D1 — My Workspace and My Classes

**Addresses:** My Classes and eager workspace children.

- Build a compact class summary model for navigation and grouping.
- Replace one QWidget page per class with a list/table/delegate or equivalent
  card renderer.
- Preserve grade/level navigation with lightweight model indexes.
- Keep one reusable selected-class detail panel and at most a small,
  byte-bounded detail cache.
- Replace the three-per-class text-editor pattern with reusable read-only
  presentation or one active editor where editing is required.
- Load Personal Details and Schedule only when their tabs are activated.
- Do not construct a real CalendarPage merely to populate a workspace tab.
- Release workspace child pages and My Classes detail state on suspension,
  theme/language rebuild, and page leave.

**Acceptance:** 96 classes produce one compact summary per visible class, not
96 rich widget trees; repeated refresh and re-entry do not retain generations
of old content.

### D2 — Teachers and Staff

**Addresses:** Staff Directory per-cell items.

- Replace the staff QTableWidget with a QAbstractItemModel and QTableView.
- Use delegates for display and create editors only for the active cell.
- Share compact teacher/staff records between native and GS views.
- Refresh the model in place instead of clearing and rebuilding every item.

**Acceptance:** row counts and sorting/filtering match the baseline, while
table item and cell-widget counts remain independent of the number of cells.

### D3 — Classes

**Addresses:** Classes page's all-class tab and retained section editors.

- Build a lightweight class navigation model from IDs and summary records.
- Keep only the selected class's detail/editor content active.
- Make section editors lazy and release them on suspension or when the class
  context changes, unless a deliberate bounded retention policy is documented.
- Preserve class tabs, labels, ordering, permissions, and unsaved-change
  prompts through the new navigation model.
- Remove deferred full-tree rebuilds as the normal refresh mechanism.

**Acceptance:** 96 classes do not create 96 detail-page trees; visiting a
different class does not monotonically grow the editor stack.

### D4 — Schedule display and import

**Addresses:** schedule table, schedule import review, and workbook parsing.

- Use a compact schedule model with delegates for the main schedule table.
- Create cell editors and entry widgets only for active interaction.
- Avoid returning a full schedule model by value across UI/render layers;
  define one owner or an immutable shared projection with clear lifetime.
- Parse workbooks in a staged worker pipeline. Keep only the current worksheet
  chunk or compact import records required by the next stage.
- Build class and teacher matching indexes once per import operation.
- Materialize review rows through a model; create resolution controls only for
  visible/selected rows or the current bounded review window.
- Keep one compact preview projection instead of retaining the raw workbook,
  a duplicate preview model, and all derived UI records.
- Release raw file bytes after the parser owns the necessary data, release
  worksheet buffers after each sheet, and release the import workbook when the
  review plan is complete.
- If undo is required, retain compact decisions or diffs rather than a second
  complete workbook.
- Keep diagnostics bounded or paginated rather than joining an unbounded
  diagnostic list into one large label.

**Acceptance:** import memory is measured separately for parse, review, apply,
and cleanup; review controls do not grow as imported-classes multiplied by
existing-classes; schedule refresh does not show deferred-widget accumulation.

### D5 — Calendar import and calendar presentation

**Addresses:** raw/decoded/workbook/cell duplication and hidden calendar
construction.

- Reuse the staged workbook reader boundary where formats overlap.
- Process worksheet XML and event rows incrementally or in bounded chunks.
- Remove the v2 compatibility cells view unless a consumer demonstrably
  requires it; migrate that consumer to sheet/row projections.
- Keep only the compact event model required by the active calendar range.
- Construct QML calendar objects on calendar entry and release them according
  to the PageHost lifecycle.

**Acceptance:** a large calendar workbook has no simultaneous unnecessary raw,
decoded, cell, and derived copies; calendar entry does not affect startup
memory.

### D6 — Speaking evaluations and batch workflows

**Addresses:** all-class speaking tabs, AI batch tables, and report data
copies.

- Replace all-class tab-page construction with summary navigation and one
  selected evaluation view.
- Use model/view for batch selection and review tables.
- Keep report records addressable by ID and load only the selected/current
  batch details.
- Process large report batches in bounded chunks; release parsed AI responses
  after the corresponding report state is stored.
- Do not copy the full report list into both the dialog and a second review
  object unless the ownership contract requires it.
- Keep PowerPoint, image, PDF, and temporary workspace objects inside the
  report operation scope.

**Acceptance:** batch selection and review retain behavior and output parity
without per-cell item growth or a second complete report collection.

### D7 — Substitute Preparation

**Addresses:** the existing Sub Prep UI and output boundary.

Implement the dedicated [Sub Prep Class Information and Output Memory
Plan](sub-prep-class-information-memory-plan.md) as the owning 7J slice:

- compact summaries and selected details;
- visible-ID-filtered queries;
- one reusable detail panel;
- explicit refresh, leave, and release behavior;
- operation-scoped package and PDF projections;
- bounded output rendering.

The 96-class fixture remains the required scalability test, and the current
high-memory Release trace remains the before-state oracle.

**Acceptance:** the large route completes within the Phase 9 budget, class
information widget/editor counts are bounded, and generated output remains
visually and behaviorally equivalent.

## Phase E — Output and transfer operations

**Existing phase:** Phase 8.

Make large outputs independent of resident page trees and avoid holding every
representation simultaneously.

### PDF and printing

- Let output services consume PdfRenderSource or equivalent application
  projections, never widgets.
- Render Sub Prep and reports in bounded teacher/page chunks when whole-document
  HTML or QTextDocument allocation exceeds the operation budget.
- Release each chunk's source records after it is rendered or written.
- Avoid retaining source data, full HTML, rendered pages, and final output
  buffers simultaneously unless the output contract requires it.
- Keep generated and print-output PDFs distinct from the on-demand viewer
  document lifecycle.

### Class transfer

- Write transfer JSON to a QIODevice incrementally or in bounded package
  stages without first building an unnecessary full JSON byte array.
- Read and validate records incrementally where the format permits; otherwise
  enforce one owned parsed package and release raw bytes/document objects before
  opening the preview.
- Preview compact class summaries first and load full roster/evaluation data
  only for the selected class or during the actual import.
- Make the caller transfer ownership into the operation/dialog rather than
  retaining the same complete package in multiple owners.
- Preserve the existing transfer format, validation, cancellation, and
  failure recovery.

**Acceptance:** large export/import operations have a named peak budget and
return toward the pre-operation memory range after completion or cancellation.

## Phase F — Memory hardening and release qualification

**Existing phase:** Phase 9.

After the feature slices are complete, run the common instrumentation against
the full route. This phase owns the final budget and rejects partial fixes.

### Required scenarios

- empty and representative startup;
- 96-class Sub Prep entry, selection, refresh, output, leave, and re-entry;
- large schedule parse, review, apply, and cancellation;
- large calendar import and calendar open/close;
- repeated schedule refresh and language/theme rebuild;
- repeated Classes, My Classes, and Speaking Evaluation navigation;
- staff directory and large AI batch review;
- class transfer export/import/cancel;
- PDF/report generation and preview close;
- five-minute idle and repeated workspace open/close.

### Required assertions

- normal startup-ready and idle memory stay below the established Windows
  packaged Release target;
- feature widget/editor counts are constant or viewport-bounded;
- model row counts match the scenario and do not duplicate full datasets;
- raw, decoded, parsed, and output representations have non-overlapping
  lifetimes where overlap is not required;
- page leave/release returns memory toward the pre-entry range;
- repeated operations do not show unbounded working-set, private-byte, handle,
  or thread growth;
- transient operation peaks stay within their documented budgets;
- no gate is passed by reducing fixture size, disabling a route, or changing
  output fidelity.

## Phase G — Permanent regression gates

**Existing phase:** Phase 11.

Convert every accepted scenario into deterministic tests and packaged Release
evidence.

### Test groups

- Projection tests: filtering, ordering, ID sharing, bounded query columns,
  and detail invalidation.
- Lifecycle tests: page creation, activation, suspension, release, recreation,
  and repeated navigation.
- Presentation tests: delegate rendering, selection, active editors, model
  row counts, and no per-record page construction.
- Import tests: large workbook staging, matching indexes, diagnostics,
  cancellation, cleanup, and no duplicate retained workbook.
- Output tests: PDF/package/report parity, chunk cleanup, cancellation, and
  failure recovery.
- Memory tests: checkpoint measurements, peak budgets, post-operation return,
  and repeated-run growth.
- Visual tests: both languages, both themes, selected/empty/loading/error
  states, and output references.

The large fixtures are mandatory for memory tests. Smaller fixtures remain
useful for fast semantic tests and failure-path coverage.

## Phase H — Compatibility removal and maintenance

**Existing phases:** Phases 12 and 13.

Only after parity and memory gates pass:

- remove per-class widget-tree builders and old tab-page construction;
- remove QTableWidget-based priority paths and cell-widget renderers;
- remove compatibility workbook cell duplication and broad import copies;
- remove temporary repository/application adapters;
- remove duplicate PageManager ownership and obsolete release shims;
- keep memory, import-size, repeated-navigation, and output-budget tests in
  continuous maintenance;
- require a documented owner, scope, and byte budget for every new cache or
  large operation.

## Recommended execution order

1. Phase 0: finish measurements and retain the current high-memory traces.
2. Phases 2–3: define projections, query boundaries, parser stages, and
   operation ownership.
3. Phases 5–6: implement PageHost lifecycle, lazy navigation, shared
   model/view primitives, and reusable detail/active-editor hosts.
4. Phase 7B and 7D: migrate workspace, My Classes, and Classes so the
   per-class page pattern is removed early.
5. Phase 7E and 7F: migrate schedule/calendar tables and import pipelines.
6. Phase 7C and 7H: migrate staff, speaking navigation, batch review, and
   report data lifetimes.
7. Phase 7J: complete the existing Sub Prep memory/output vertical slice.
8. Phase 8: move PDF, printing, report, and transfer operations behind bounded
   adapters.
9. Phase 9: run the full packaged Release hardening matrix and set final
   pass/fail evidence.
10. Phase 11: enforce the gates in the permanent test suite.
11. Phases 12–13: remove temporary paths and monitor for regressions.

## Definition of done

This plan is complete only when:

- every audited hotspot has a migrated owner and a passing regression test;
- large data is represented compactly and loaded by scope, selection, or
  operation;
- repeated records no longer imply repeated heavyweight widget trees;
- raw, parsed, decoded, and output data lifetimes are explicit;
- normal and transient memory budgets pass in packaged Release;
- leaving a feature releases its feature-owned resources;
- all required behavior, visuals, files, imports, exports, and outputs remain
  compatible;
- legacy allocation paths and temporary adapters have been removed.

The measured legacy high-memory route remains documented after completion as a
before-state comparison, but it is not an accepted v2 behavior.
