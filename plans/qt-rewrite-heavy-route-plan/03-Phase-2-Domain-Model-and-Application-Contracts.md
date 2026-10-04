# Phase 2 — Domain Model and Application Contracts

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-10-04
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F309 is accepted. F310 is selected to batch ClassImportDialog
  matched-teacher alternative display-name reads, preserving choice order and
  duplicates, formatting, and the current “New Teacher” fallback for an
  unreadable profile. F311-F315 remain in Batch 4; at F314 start, discover
  Batch 5 and reconsider the fixed up-to-four-evaluation roster score-import
  read.

### Slice discovery batches

Discover upcoming slices in ordered batches of up to ten (or all remaining
slices if fewer than ten remain). Record each batch as an ordered list under
`Recorded batches` below and work through those slices in order. Begin discovering
and recording the next batch when starting work on the second-last slice in the
current batch. If a discovery pass finds fewer than ten slices, add the exact
standalone line `No other slices were found.` beneath that batch.

Keep the Status `Current note` limited to the latest information relevant to the
current or next slice. Keep only the most recent slice commit in the
`Latest Progress Update` section. When writing a newer update, move the previous
one to this phase's progress log before replacing it.

#### Deferred candidates

Accepted slices are removed from active tracking; their implementation and acceptance evidence remain in the chronological [Phase 2 progress log](03-Phase-2-Progress-Log.md).

- F285 - Deferred before implementation: addTeacher() creates a blank
  teacher, but required-name validation rejects it before the profile read.
  Revisit after the blank-draft creation contract is clarified.
- F298 - Deferred after review: SidebarController::addClass() re-reads the
  class only to reuse the ID returned by create(). Removing the read changes its
  failure warning/no-navigation behavior; revisit after that behavior is
  clarified.

#### Active batch: Batch 4

1. F310 - Batch ClassImportDialog matched-teacher alternative display-name
   reads; separate from F300 class subtitles; preserve choice
   order/duplicates, formatting, and the current “New Teacher” fallback when a
   profile cannot be read.
2. F311 - Batch Sub Prep roster-output per-class class-name and compact
   metadata reads; preserve schedule order, omission of classes without
   meetings in the selected scope, identity checks,
   failure-before-partial-output, and aggregate output bounds.
3. F312 - Purpose-fit projection for initial-setup teacher choices; preserve
   ID/name/preferred display fields, repository order, validation, and error
   behavior.
4. F313 - Purpose-fit projection for testing-teacher choices; preserve
   ID/Korean-name/room, repository order, blank-name filtering and selection
   behavior, and current recoverability semantics.
5. F314 - Purpose-fit projection for co-teacher choices; preserve exact
   profile/network fields, repository order, ID validation, and error
   behavior.
6. F315 - Purpose-fit Korean teacher birthday-directory projection;
   preserve birthday/name/preferred-display fields, raw values, repository
   order, and downstream filtering.

At F314 start, discover Batch 5 and reconsider the fixed up-to-four-evaluation
roster score-import read. F305's transfer-menu capacity/roster loop is already
accepted and must not be duplicated.
#### F299 completeness audit checkpoint

At F299 start, perform a separate completeness audit for Phase 2 slices missed
in earlier discovery or work. Keep this distinct from the planned Batch 3
discovery at F298 start.

Audit result — 2026-10-03: Two independent read-only sweeps found no missed
direct legacy-read routes outside recorded work; `SidebarController::getTeacherById()`
has no callers. The second sweep found four genuine post-migration query
fan-out candidates, independently classified as separate batching/aggregation
opportunities: F302-F305, appended after F301 in Batch 3. This F299 audit is
distinct from Batch 3's F298-start discovery of F300-F301.

## Objective

Create a stable, testable application core that is independent of widget construction, page visibility, and the legacy data facade.

## Work packages

### 2.1 Domain value types

Create explicit value types for:

- Workspace identifiers.
- Teacher and staff records.
- Class and course records.
- Class times and schedule entries.
- Students and rosters.
- Campuses and locations.
- Calendar events.
- Speaking evaluations and criteria.
- Document catalog entries, document-content references, and templates.
- Import matches and conflicts.
- User preferences.

Use typed identifiers and enums instead of unrelated strings that happen to contain IDs or state values.

### 2.2 Structured results

Replace loosely typed return values with structured results:

- Success values.
- Recoverable warnings.
- User-facing errors.
- Technical errors.
- Validation failures.
- Import conflicts.
- Cancellation state.

An import result, for example, should separately expose imported records, warnings, unmatched values, and conflicts.

### 2.3 Application use cases

Create use cases for:

- Creating, opening, closing, saving, and exporting a workspace.
- Importing a legacy database.
- Importing teachers, schedules, calendars, rosters, and class transfers.
- Editing teachers, classes, schedules, calendar events, rosters, and evaluations.
- Generating reports and substitute documents.
- Listing campus and document metadata.
- Opening document content on demand for a viewer or output operation.
- Performing backups and recovery.
- Checking for application updates after startup.

The Sub Prep slice also requires explicit summary, selected-detail, and
operation-scoped print-source contracts. Their feature-level implementation
plan is tracked in [Sub Prep Class Information and Output Memory
Plan](sub-prep-class-information-memory-plan.md).

Each use case must have:

- Explicit input.
- Explicit output.
- Structured errors.
- No widget references.
- No hidden singleton state.
- A deterministic test boundary.

### 2.4 Validation and business rules

Move validation rules into domain or application services.

The UI may display validation results and choose when to validate, but it must not own the business rule implementation.

Preserve current validation messages and behavior until visual and behavioral parity is accepted.

### 2.5 State and concurrency

Define:

- Workspace session state.
- Unsaved-change state.
- Current selection state.
- Import-job state.
- Report/export-job state.
- Document-content session state: requested, loading, ready, failed, and released.
- Cancellation behavior.
- Thread ownership.

Background work must return results through application interfaces. Worker code must not mutate widgets directly.

### 2.6 Memory-safe projections and operation contracts

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) defines the compact contracts needed
by the large-data slices. Add application-facing projections equivalent to:

- compact class and teacher summaries plus selected class details;
- a compact schedule view projection;
- an import review session containing matching indexes and compact decisions,
  not the original workbook and every derived UI object;
- staged transfer-reader and transfer-writer records;
- operation-scoped report and PDF render sources.

Every contract must state which layer owns the data, when raw or derived
representations may overlap, and when they are released. Contracts must not
return widget trees, page pointers, or broad compatibility-service snapshots.

## Deliverables

- Domain model library.
- Application use-case library.
- Structured error and warning types.
- Validation services.
- Application-state definitions.
- Domain tests that run without a QApplication.
- Mapping document from old service calls to new use cases.

## Exit gate

Domain and application behavior can be tested without constructing the main window.

Validation, conflict detection, import planning, and state transitions match the baseline fixtures.

For the workspace boundary, acceptance includes the current
`WorkspaceGateway::createWorkspace` contract and `WorkspaceCoordinator` create
behavior: dirty replacement is rejected before the gateway, a successful
session opens `WorkspaceState` and clears `SelectionState`, and gateway or
invalid-session failures preserve both snapshots. The existing app-less
`NextApplicationContractTests` and `NextApplicationWorkspaceCoordinatorTests`
cover these create paths alongside open, close, save, save-as, and export.

No new v2 production path depends on DataService, MainWindow, PageManager, or a widget pointer.

## Heavy-route requirements

- For every Phase 2 slice, use the heavy route: implement the contract across
  its intended v2 boundary, verify it with the owning layers, and remove any
  temporary compatibility wrapper when the slice is accepted.
- Convert core contracts rather than wrapping every old Qt type indefinitely.
- Keep Qt conversion at the UI, filesystem, or platform boundary.
- Prefer explicit immutable snapshots for read models.
- Keep document-content contracts independent of QtPdf; the viewer/platform
  adapter owns the active document session and its release boundary.
- Do not hide business rules inside presenters or delegates.
- Do not allow compatibility methods to become the permanent v2 API.

Earlier verified slices and cumulative exit-gate snapshots are archived in the [Phase 2 progress log](03-Phase-2-Progress-Log.md).

## Latest Progress Update - 2026-10-04 (F309 accepted; F310 selected)

F309, committed as
`a87edb3147577cf52d92e35e5d8587792df7883a`, batches Sub Prep information-sheet
roster count reads after schedule and teacher filtering. The print-source port
requests counts only for included classes, preserves source order, and leaves
each class in place with a zero count when its roster read fails. The
repository reads roster columns and sparse cells in set-based statements,
preserving exact first English/Korean selection, QString trimming, and one
count per row.

Successful class counts emit the legacy `sub-prep-roster-query` metrics with
the original class ID, column count, materialized row count, cell count, and
returned student count. Missing name headers still emit a zero-count event;
failed per-class reads emit no event. Event failures stay isolated from the
class output.

Executor self-check and fresh independent Windows x64 Debug/Ninja verification
passed. The independent configure validated 1,301 handwritten source files;
ClassMngr and all seven focused test targets built. Focused CTest passed 7/7:
the My Classes student-count page/Application/Platform regression tests, both
Sub Prep print-source Application/Platform tests, the Sub Prep print-source
mapper test, and the DataService lifecycle test. `git diff --check` passed.
Independent logs are under build/f309v/ (configure.log, build.log,
focused_ctest.log, diff_check.log); executor logs are under build/.
The full suite was not run.

Read-only discovery for F310 confirmed that an unreadable matched teacher
profile currently displays “New Teacher,” and F311 discovery confirmed that
classes without a meeting in the selected scope are omitted from roster output.
The active batch text now records those existing behaviors. F310 is selected
to batch matched-teacher alternative display-name reads, separate from F300
class subtitles. F311-F315 remain in Batch 4; at F314 start, discover Batch 5
and reconsider the fixed up-to-four-evaluation roster score-import read. F298
remains deferred pending its warning/navigation decision. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.
