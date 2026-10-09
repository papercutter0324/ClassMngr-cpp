# Sub Prep Class Information and Output Memory Plan

## Status

- Status: In progress
- Route: Heavy
- Slice type: Vertical feature and memory-hardening slice
- Related phases: Phase 0, Phase 2, Phase 3, Phase 7J, Phase 8, and Phase 9
- Depends on: v2 domain/application boundaries, persistence contracts, and the
  packaged Release measurement harness
- Blocks: Large-workspace Sub Prep acceptance and the Phase 9 memory gate
- Owner: Unassigned
- Last updated: 2026-10-09

## Current Sub Prep contract boundary - 2026-09-24

Phase 2 has Qt-free schedule-summary and selected-class details queries,
`SubPrepClassInformationState` for selection/detail lifecycle, and an
operation-scoped print-source request/read-port/query. Session-backed Platform
reads now exist for the scoped print source, one selected class's details, and
the class/day/mode-scoped schedule summary. The summary read returns bounded
owning class and teacher values, batches roster counts with a zero-count
fallback on roster-query failure, avoids reads for empty class/day scopes, and
has no `DataService` fallback. The intensive-mode test passes with
`class_times` dropped. The details value contains class notes, preferred
teacher display name, teacher notes, and separate bounded room, WiFi name,
WiFi password, internet type, Zoom ID, Zoom password, and projection type
fields. The details read uses the active `ClassService` repository session,
does not query either schedule table, and has no `DataService` fallback.
Missing class-info rows on existing classes return blank details; absent
classes return `NotFound`, and
unassigned, missing, or stale teacher references return empty teacher values.

Sub Prep calendar intervals cover at most the current and following calendar
years. [readRequestForReferenceDate](../../src/next/application/sub_prep_calendar_event_intervals_query.h#L164)
builds January 1 of the reference year through December 31 of the following
year, clamped at 9999. The production page passes the current date
([sub_prep_page.cpp](../../src/features/sub_prep/ui/sub_prep_page.cpp#L502));
[derivesCurrentAndFollowingCalendarYearWindow](../../tests/next_application_sub_prep_calendar_event_intervals_query_tests.cpp#L154)
verifies that a 2026 reference yields 2026-01-01 through 2027-12-31.

Work Package D now connects the schedule-summary and selected-class details
queries to a model-backed grade/class list and one reusable details card.
`SubPrepClassInformationListModel` filters and orders compact summaries; the
page keeps selection in `SubPrepClassInformationState`, and refresh replaces
the projection in place while retaining a class only when it remains in scope.
Changing schedule display mode also refreshes that projection. There is no
per-class page or details widget.

Windows x64 Debug Ninja builds succeeded for `ClassMngr`,
`ClassMngrSubPrepClassInformationListModelTests`,
`ClassMngrSubPrepPageTests`, and `ClassMngrStartupPerformanceTests`. Focused
CTest passed the list-model and page suites 2/2; configure validated 860
handwritten files and `git diff --check` passed. The startup-performance
target compiled, but the 96-class packaged Release route was not run. Focused
page tests cover selection retention, invalidation when a class leaves scope,
and display-mode refresh. The startup target compiles the new bounded
widget/editor assertions.

Work Package E now uses `BasePage::deactivate()`'s resource-release hook to
clear the selected details, compact projection, and selection state when
PageManager leaves Sub Prep. The page is marked stale so re-entry reloads the
current schedule scope and one selected detail. The page lifecycle test
verifies release and reload; `ClassMngr` and `ClassMngrSubPrepPageTests` build
on Windows x64 Debug Ninja and the page suite passes 1/1.

Work Package F1 now routes the Sub Prep information sheet through
`SubPrepPrintSourceQuery` after the print dialog returns its selected class
IDs, weekdays, and schedule mode. `SubPrepPrintSourceMapper` converts the
bounded operation value into the existing PDF renderer model and preserves
the teacher name fallback inputs and rendered facts. The old main-sheet path
that loaded every class, class-info record, teacher, and roster count has been
removed. The query-owned projection is released when the mapping helper
returns.

Windows x64 Debug Ninja built `ClassMngr`, the page and mapper tests, the print
source query and Platform adapter tests, and the existing PDF and package
service tests. Focused CTest passed 6/6. This verifies the query, adapter,
mapping, page compilation/lifecycle, and existing renderer/package suites; it
does not prove full package parity or a memory improvement. The separate
roster-PDF stage in `SubPrepPackageService` still loads legacy class, teacher,
and full roster values. Work Package F2 replaces the rich renderer-model copy
with a render-scoped const reference to the request's list. The PDF test
checks that the document model borrows the original list, and the PDF,
package, and page suites pass 3/3. The page moves the print request into the
package request, avoiding another `TeacherGroup` list copy at that boundary.
Work Package F3 makes package generation own the request and moves it from the
page. It renders the information sheet before loading roster records, then
clears the Sub Prep document input immediately after that PDF succeeds. The
page, PDF, and package suites pass 3/3. The full legacy class/teacher/roster
read for roster PDFs remains. Work Package F4 adds the Qt-free
`SubPrepRosterOutputSourceQuery` contract for the selected class/day/mode
scope. It returns requested roster columns and owning row values under
per-class and aggregate limits, along with only the class and teacher facts
needed by the roster templates. Its app-less tests pass 1/1. The read adapter
now has a bounded repository operation: it selects requested columns, checks
row/cell limits before matrix allocation, streams SQL rows, and checks each
cell's original byte size before conversion. Data lifecycle coverage passes
1/1. The Platform adapter passes its remaining aggregate row, cell, and
text-byte budgets into that repository read and updates those totals while
building the Application input. F524 adds repository-backed coverage that
rejects cross-class row and cell overflow and aggregate text overflow before
returning the Application projection. See the [Phase 2 contract
update](03-Phase-2-Progress-Log.md#progress-update---2026-09-24-sub-prep-bounded-roster-repository-read).

Output/package/PDF migration is wired through the operation-scoped roster
source. F8 compares the 96-class information-sheet and Daily roster PDFs with
retained references across page count, dimensions, and extracted text, and
compares all Windows 150-DPI renders exactly. Print cancellation and source
failure cleanup checks pass. Work Package F has moved the information-sheet
and roster stages to separate bounded Application projections; the package
service releases the information-sheet input before loading roster records,
and the data repository applies row/cell caps before creating a dense roster
projection. F526 completed the packaged Release `lifecycle-sub-prep` and
`output-sub-prep` evidence routes. Memory acceptance remains open: lifecycle
peak working set was 264,310,784 bytes, 2,166,784 above 262,144,000; settled
lifecycle and all output-route working-set samples were below target. The next
memory action is F530's diagnosis of the PDF-reopen peak; F529 visual mismatch
work is tracked in the Phase 2 plan and log.

This is an implementation slice, not a new rewrite phase. The existing
large-workspace fixture remains a required stress input. A bounded fixture may
be used for fast route-semantics tests, but it must not replace the large
fixture's scalability and memory gate. The F9 packaged Windows x64 Release
route completes normally and passes
route-scoped validation. It measured 315,740,160 bytes peak working set,
351,821,824 bytes peak private usage, 306,466,816 bytes at one second after
completion, and 306,470,912 bytes at five seconds. These peaks improve on the
retained legacy measurement (498,176,000 working-set bytes and 480,948,224
private bytes), and the route stays below the temporary 512 MiB diagnostic
ceiling. Both settled working-set samples remain above the final 250 MiB
target; memory acceptance and repeated entry/leave growth checks remain open.

## Objective

Make Sub Prep handle the 96-class / 8-slot workspace with bounded memory while
preserving the current workflow, ordering, navigation, appearance, localized
text, generated PDFs, package generation, and print behavior.

The target ownership model is:

```text
visible schedule scope
        |
        v
compact class-summary projection
        |
        +--> model-backed navigation/list
        |
        +--> one reusable selected-class detail view
                    |
                    +--> on-demand class/teacher details

package generation --> operation-scoped print projection --> bounded renderer
```

## Current memory boundary

The current path materializes a widget hierarchy for every visible class in
`src/features/sub_prep/ui/sub_prep_page_class_information.cpp`. Each class
gets a page, layouts, a section card, many labels, and two text editors. The
same path also copies rich `ClassInfo` and `Teacher` records through source,
group, navigation, and widget-building collections.

The data-loading path first loads all classes, then performs per-class class
information and roster lookups, and may perform repeated teacher lookups. The
rebuild helper schedules old widgets for deferred deletion before allocating
the replacement tree. During a refresh, the old and new trees can therefore
overlap temporarily.

The output path separately materializes a rich `TeacherGroup` tree and passes
it to a whole-document HTML/PDF renderer. That data must remain available for
generation, but it must not be the page's resident UI representation.

## Non-goals

- Do not reduce the 96-class fixture or remove Sub Prep behavior.
- Do not hide classes, disable package generation, or reduce output fidelity.
- Do not solve the problem with a Debug-only limit or a Release-only branch.
- Do not make the legacy composition root the permanent owner of the new
  feature.
- Do not use `processEvents()` as a general memory-management workaround.

## Invariants to preserve

- The same classes are visible for the same schedule mode and selected days.
- Grade and level ordering, selected-class retention, teacher grouping, and
  class labels match the current behavior.
- The selected class still exposes all current class and teacher information.
- Localized English/Korean text and light/dark styling remain equivalent.
- PDF, print, roster, package-folder, and error/cancel behavior remain intact.
- Refreshing the schedule does not leave stale class details visible.
- Leaving Sub Prep releases feature-owned detail state and does not grow
  memory after repeated enter/leave cycles.

## Work packages

### A. Establish the measurement and parity contract

Before implementation, add or complete deterministic evidence for the current
route:

1. Run the bounded full-route fixture to establish navigation and lifecycle
   semantics.
2. Run the 96-class large fixture through startup, Sub Prep entry, refresh,
   class selection, leave, and repeated re-entry. Retain the failure trace if
   the legacy path still terminates before completion.
3. Record working set, private bytes, peak working set, process exit status,
   page-enter/page-leave events, widget count, text-editor count, model row
   count, and database query/result sizes.
4. Capture representative Sub Prep screenshots for both languages and both
   themes, including the first class, a different grade, a different level,
   empty visibility, and the generated-output entry point.
5. Define the Release memory budget after the baseline is captured. The gate
   must include steady-state entry, peak during refresh, repeated navigation,
   and memory after leaving; it must not be judged from Debug alone.

Deliverable: a before/after trace format and a visual/output parity checklist.

### B. Define the v2 application and persistence projections

Introduce explicit Sub Prep application contracts. Names are illustrative and
may be adjusted to match the v2 naming conventions.

`SubPrepClassSummary` should contain only data needed to list and select a
class:

- class ID and teacher ID;
- grade, level, display label, and compact meeting-time text;
- student count;
- the selected schedule mode or a stable mode-independent schedule summary;
- any small color/style values required to paint the row.

`SubPrepClassDetails` should contain the data needed by the selected detail
panel, including class notes and teacher facilities/notes. It must be loaded
by ID and must not be stored once for every class in the page.

The application boundary exposes contracts for operations equivalent to:

- build summaries for a schedule scope;
- load details for one class;
- invalidate or refresh the scope;
- build an operation-scoped print source for selected days/classes.

The schedule-summary, selected-details, selection-state, print-source, and
roster-output contracts now exist in Phase 2. Their scoped Platform reads are
implemented, including the bounded roster repository projection. Package
generation now consumes the typed roster source and maps it to renderer
inputs. Focused Windows Debug coverage verifies selected scope, output
variants, and source/mapping failure cleanup; full output parity,
cancellation/cleanup parity, and packaged Release memory evidence remain
open.

The UI must not issue SQL or depend on `DataService` compatibility methods.

### C. Replace broad and repeated data loading

Implement a repository/application query that receives the visible class IDs,
selected days, and regular/intensive mode, then returns the compact summary
projection in one bounded operation.

The query or repository batch should:

- filter by visible class IDs before loading class records;
- select only summary columns;
- join or batch-load teacher summary fields once per teacher;
- aggregate roster counts instead of loading roster cells;
- avoid retaining full `ClassInfo`, `Teacher`, `Roster`, and schedule objects
  in the UI projection;
- preserve deterministic ordering in the application layer;
- return a clear error without partially replacing the current model.

If the v2 repository cannot yet provide the final query, create a temporary
adapter behind the application contract. Give that adapter an explicit removal
point; do not expose the compatibility facade to the new view.

Use compact vectors and ID/index relationships where practical. A small
summary row for each of 96 classes is acceptable; a rich object graph and a
Qt widget graph for each class is not.

### D. Build a model-backed, reusable Sub Prep view

Replace per-class `QWidget` pages with a model/view presentation:

1. Add a `QAbstractItemModel` or equivalent presenter for compact class
   summaries.
2. Use a `QListView`, `QTableView`, or a custom delegate to paint the existing
   card/list appearance. Delegates must not create child widgets per row.
3. Preserve grade/level navigation with lightweight tab-bar or selector
   controls backed by model indexes. Do not create a hidden `QWidget` page for
   every class.
4. Keep one selected-class detail card/panel and update it when the selection
   changes.
5. Use a selectable/wrapping label for read-only notes where current behavior
   permits it. If scrolling or rich text behavior is required, keep one
   reusable `QPlainTextEdit`/`QTextEdit`, not one editor per class.
6. Store the selected class ID in presenter state rather than recovering it
   through widget properties and `findChild()` calls.
7. Update/reset the model in place when schedule visibility or display mode
   changes. Do not rebuild the complete widget hierarchy for ordinary refresh.

The live widget count for class information should be constant or proportional
to the visible viewport, not proportional to the number of classes. The
resident data should be one compact summary per visible class plus the current
detail record and a deliberately bounded cache.

### E. Make detail loading and lifecycle explicit

When the selected class changes:

- load its detail record by ID;
- load or reuse one teacher detail record by teacher ID;
- replace the contents of the reusable detail panel;
- release the previous detail record unless it is inside the small explicit
  cache;
- retain the selected ID only if it remains visible after a refresh.

When the schedule, workspace, language, or theme changes, invalidate the
appropriate projection and refresh the model. Do not retain stale rich
records just to avoid a query.

When leaving or releasing the page, release the selected-detail cache and any
operation state. If a structural rebuild is temporarily unavoidable, remove
and destroy the old owned view before allocating its replacement within a
lifecycle-safe boundary; do not depend on deferred deletion overlapping two
large trees.

### F. Separate the page model from output generation

Package generation must read from the application/persistence contract, not
from the set of widgets currently instantiated on the page.

Implement an operation-scoped print projection that:

- loads only the selected days/classes;
- shares teacher data within the operation;
- avoids copying full UI models into multiple request/document structures;
- releases source records after each output stage completes.

Measure the existing PDF renderer separately. If the whole-document HTML
document is a material allocation, change it to render bounded teacher/page
chunks while preserving page-break and output parity. Do not retain source
records, generated HTML, rendered pages, and final buffers unnecessarily at
the same time.

### G. Verification and regression coverage

Add focused tests for:

- visible-ID filtering and regular/intensive schedule selection;
- summary ordering and grade/level grouping;
- missing teacher/class fallback behavior;
- one-detail-load-per-selection semantics;
- selected-class retention across refresh;
- stale-detail invalidation when a class leaves the schedule;
- model row counts and delegate rendering;
- package/PDF output parity against existing references;
- cancellation, error, and partial-output cleanup.

Add UI/lifecycle tests that:

- enter Sub Prep with the bounded fixture;
- select classes across grades and levels;
- refresh schedule mode and selected days;
- repeatedly enter and leave the page;
- assert that class-information widget/editor counts remain bounded;
- assert that memory returns toward the pre-entry range after release.

Add the large-fixture workflow as a required stress test. It must complete
Sub Prep entry, class selection, leave, and repeated-entry checkpoints after
the migration. Keep the bounded fixture as a fast semantic test, not as the
scalability acceptance case.

### H. Packaged Release acceptance

Run the final workflow in a clean packaged Windows x64 Release build, then
repeat on the other supported Release targets when their baselines are
available.

Record at minimum:

- database-opened;
- Sub Prep page-entered;
- summary-model-ready;
- first-detail-loaded;
- refresh-complete;
- package-generation-start/end;
- Sub Prep page-left;
- settled memory after one and five seconds;
- repeated enter/leave growth;
- process exit status and failure artifact.

Acceptance requires:

1. The 96-class large fixture completes the Sub Prep route without abnormal
   termination.
2. Class-information widget/editor creation is bounded and no longer scales
   as one full widget tree per class.
3. Peak and settled memory meet the Phase 9 budget, with the exact budget
   recorded in the baseline before the gate is reviewed.
4. Repeated entry, refresh, and leave do not show unbounded working-set or
   private-byte growth.
5. Generated PDFs/packages and visual references remain within parity
   tolerance.
6. The bounded fixture and the large fixture both pass their distinct roles.

## Suggested implementation order

1. Complete the baseline/instrumentation and preserve the current failure as
   evidence.
2. Add the v2 application contracts and a repository adapter with unit tests.
3. Implement the compact summary query and detail lookup.
4. Implement the model-backed navigation and reusable detail panel.
5. Add explicit release/invalidation behavior and remove full-tree refreshes.
6. Migrate package/PDF generation to the operation-scoped projection.
7. Run visual, behavioral, output, and large-fixture stress tests.
8. Verify clean packaged Release memory and remove temporary compatibility
   adapters after the slice is accepted.

## Exit evidence

- Updated Sub Prep memory trace for bounded and large fixtures.
- Widget/model/query lifetime report showing bounded class-information UI.
- English/Korean and light/dark visual parity references.
- PDF/package output comparison and cleanup evidence.
- Repeated navigation and page-release regression results.
- Packaged Release report with working set, private bytes, peak values, and
  exact fixture/scenario names.
- Removal list for temporary adapters and old per-class widget construction.

## Heavy-route rule

This slice is accepted only when the owning data, application, UI, output, and
lifecycle boundaries have moved together. A bounded fixture, a reduced class
set, a hidden feature, or a Debug-only workaround is not an implementation of
the memory fix.


## F509 packaged Release measurement - 2026-10-09

Evidence root: C:\Users\wfelt\AppData\Local\Temp\ClassMngr-F509-Evidence\subprep-20261009-01. Windows x64 Release configure/build/install, test-harness configure/build, both route processes, and selected-route validation completed successfully. This run covers two routes only; it is not the full 24-route Windows/macOS gate.

The 96-class lifecycle route completed two Sub Prep refresh/leave/re-entry cycles. At sub-prep-lifecycle-complete, working set was 226,119,680 bytes and private usage 215,777,280 bytes; route peak working set was 348,200,960 bytes and final 1s working set after later My Classes/PDF navigation was 338,866,176 bytes.

The output route generated 2 PDFs with 17 pages and 139,650 combined bytes, then recorded the output operation and documents released. At operation release, working set/private usage were 233,619,456/220,405,760 bytes and peak working set 242,655,232 bytes. The next checkpoint leaving Sub Prep for My Classes measured 300,208,128-byte working set and 6,412 widgets, versus 2,168 widgets at output release. After opening, rendering, and releasing both PDFs, settled working set was 307,519,488 bytes at 1s and 307,523,584 bytes at 5s; private usage 324,579,328 bytes; peak working set 316,862,464 bytes. The five-second sample is 45,379,584 bytes over the 262,144,000-byte 250 MiB target.

The route output is generated and release flags are clear, but visual parity was not compared with a reference. The target remains open. Keep the external evidence files for later comparison.

## F510 diagnostic result - 2026-10-09

The output route measured 2,168 widgets/233,619,456 working-set bytes at output-operation release, then 6,412 widgets/300,208,128 working-set bytes after the workflow activated My Classes. The checkpoint was emitted after showPage and app.processEvents. MyClassesPage rebuilds all class detail tab pages eagerly, and PageManager retains created pages in its stacked page map when hidden. These code paths explain the transition increase; exact byte attribution by QObject was not collected. F510 diagnosis is complete.

## F511 implementation and Release result - 2026-10-09

My Classes now keeps every grade/class tab and builds the detail fields/cards only for the active class. Class and grade selection changes synchronously clear the prior detail tree before populating the new selection. Existing summary and teacher-display parity checks now select the relevant tab before inspecting its details; the grouped-grade regression also checks that the old detail widgets are cleared on both grade and class changes.

The Windows x64 Debug Ninja build and the three focused My Classes CTests passed. The alternate Visual Studio build tree stopped before compilation because MSBuild FileTracker returned E_ACCESSDENIED during ZERO_CHECK. The packaged Windows x64 Release application and startup harness were freshly built and installed. A first single-route validation attempt exposed a runner manifest-shape issue for one requested route; the successful validated rerun requested lifecycle-sub-prep and output-sub-prep together. Evidence is under the external F511 evidence root, with the package/build in `f511-20261009-02` and route validation in `f511-20261009-03`.

The 96-class output route generated Sub Prep.pdf and Rosters - By Day.pdf (2 files, 17 pages, 139,650 combined bytes). At output-operation release it measured 2,168 widgets and 235,069,440 working-set bytes. After My Classes activation it measured 2,471 widgets and 234,868,736 working-set bytes, versus F509's 6,412 widgets and 300,208,128 bytes at the equivalent checkpoint: 3,941 fewer widgets and 65,339,392 fewer working-set bytes. The output route settled at 240,312,320/240,316,416 working-set bytes at 1s/5s, with 254,984,192 private bytes and a 249,737,216-byte peak working set.

The selected lifecycle-sub-prep route still has a 280,035,328-byte maximum at the PDF reopened/rendered workflow checkpoint; 24 samples exceeded the 262,144,000-byte target. These selected routes are only 2 of 24 Windows Phase 0 routes, macOS universal remains absent, and generated output was not visually compared with a baseline. The 250 MiB, full-route, and visual-parity gates remain open.

## F512 diagnostic result - 2026-10-09

The lifecycle route's 280,035,328-byte process peak was recorded while a PDF document was loaded and the PDF view was rendered. The document-release checkpoint returned the live-document count to zero and current working set fell from 278,790,144 to 270,606,336 bytes; the final 1-second sample was 270,618,624 bytes. The route had already exceeded 250 MiB before loading a document: the My Classes activation checkpoint was 263,041,024 bytes, with 2,808 widgets and 10 instantiated pages. At the following PDF catalog-ready checkpoint, before document load, working set was 265,170,944 bytes.

The My Classes activation added 321 widgets and 3,629,056 working-set bytes in this lifecycle route. `PageManager` caches the page; `MyClassesPage` does not override `releaseFeatureResources()`, so its selected detail tree remains live while hidden. By contrast, after document close the private working set was only 860,160 bytes above its pre-load sample, but the route metrics cannot attribute the remaining total working set to the hidden page, mapped Qt PDF pages, or allocator retention. The output route's F511 five-second sample was 240,316,416 bytes; the lifecycle route remains over target.

## F513 implementation and packaged Release result - 2026-10-09

F513 now releases the selected My Classes detail tree synchronously on page leave and rematerializes it on activation from retained class summaries. Flat and grouped re-entry coverage holds the summary-list query count at 1 and checks restored student counts (4 and 0). The earlier package under `C:\Users\wf\AppData\Local\Temp\ClassMngr-F513-Evidence\f513-20261009-01` generated outputs and passed validation for lifecycle-sub-prep and output-sub-prep, but predates the query-count instrumentation and does not represent the exact current source.

The older lifecycle route measured 2,789 widgets after leaving My Classes (F511 comparison: 2,830), a 280,281,088-byte peak, and 270,901,248 bytes at the final 1-second sample (8,757,248 over 262,144,000). The output route measured 2,450 widgets after leaving My Classes (F511 comparison: 2,493), a 250,028,032-byte peak, and 240,627,712 bytes at 5 seconds. These are route-specific measurements; do not infer working-set reduction. Only 2 of 24 Windows routes were validated, macOS universal is absent, and output visual parity was not compared. The 250 MiB, full-route, and visual-parity gates remain open.


## F513 current-source Release rerun - 2026-10-09

The Windows x64 Release package passed validation for `lifecycle-sub-prep` and `output-sub-prep`. Its evidence directory is under the current user's Local\Temp at `ClassMngr-F513-Release-Evidence\f513-release-20261009-02`; the account name was redacted in the handoff, so no absolute path is recorded. The package omits all My Classes runtime metric keys, including `classSummaryListQueryCount`, and therefore does not independently verify F513's My Classes behavior. The focused Windows x64 Debug CTest pass (3/3), recorded in the Phase 2 progress log, remains the direct verification.

Lifecycle settled-1s working set was 270,848,000 bytes and peak working set was 280,162,304 bytes. Output settled-5s working set was 239,767,552 bytes and peak was 249,077,760 bytes. Only 2 of 24 Windows routes were validated; macOS and output visual parity remain open. The lifecycle settled sample is 8,704,000 bytes above the 262,144,000-byte target, so the 250 MiB gate remains open. Do not infer memory improvement from these route-specific results. F514's route-level diagnostic is recorded in the [Phase 2 progress log](03-Phase-2-Progress-Log.md). Sub Prep memory and visual gates remain open.
