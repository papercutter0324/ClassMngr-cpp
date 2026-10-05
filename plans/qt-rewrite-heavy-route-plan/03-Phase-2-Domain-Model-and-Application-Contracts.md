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
- Last updated: 2026-10-05
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F339 is selected for a purpose-fit Schedule Testing
  class-choice projection, narrowing the current nine-field reader to class ID
  and name, grade, level, and room while removing unused teacher ID, colors,
  notes from the projection. Preserve the inner `testing_classes`-to-`classes`
  join and left `class_info` join/defaults, grade/level/name/ID order, successful empty
  results, Technical errors, and one-query bound; add a statement-metric
  assertion and missing-class-info test. Focused target:
  `ClassMngrNextPlatformApplicationServicesScheduleTestingClassChoicesReadPort`.
  F339 is selected, not implemented.
  Batch 7 is complete; Batch 8 remains active with F340-F344 queued.

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

#### Recorded batches

##### Batch 7 (complete)

1. F337 - Reuse the F332 class-details reader for Class Details validation
   context.
2. F338 - Clean up the ClassImportDialog boundary.

No other slices were found.

##### Batch 8

1. F339 - Add a purpose-fit Schedule Testing class-choice projection; drop
   unused teacher ID, colors, and notes from the projection while preserving
   the `testing_classes`-to-`classes` inner join, the `class_info` left join,
   order, defaults, and errors.
2. F340 - Narrow Sub Prep roster-output teacher-profile fields to teacher ID,
   EN/KR names, preferred name, and romanization; preserve per-ID failures,
   order, and identity.
3. F341 - Narrow Sub Prep schedule-scope rows to consumed class ID, teacher ID,
   and schedule while preserving scope, order, caps, unassigned rows, and
   missing-information behavior.
4. F342 - Narrow the My Classes assigned-teacher batch projection to consumed
   profile fields while retaining identity and per-teacher failures.
5. F343 - Narrow the Schedule Import snapshot class-info projection while
   preserving classes-then-teachers-then-schedules error precedence.
6. F344 - Remove redundant compatibility-service availability gates from
   migrated Classes/My Classes pages while preserving the no-session early
   return and query-failure warnings.

No other slices were found.

#### Active batch: Batch 8

1. F339 - Selected/current: purpose-fit Schedule Testing class-choice
   projection.
2. F340 - Queued: narrow Sub Prep roster-output teacher-profile fields.
3. F341 - Queued: narrow Sub Prep schedule-scope rows.
4. F342 - Queued: narrow My Classes assigned-teacher batch projection.
5. F343 - Queued: narrow Schedule Import snapshot class-info projection.
6. F344 - Queued: remove redundant compatibility-service availability gates
   from migrated Classes/My Classes pages.

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

## Latest Progress Update - 2026-10-05 (F338 accepted; F339 selected)

F338, committed as `d0bb41ab` (`Phase2 - Move import dialog teacher batch
read to v2 (F338)`), removes direct `DatabaseSession`/`TeacherRepository` batch
access from ClassImportDialog by routing it through a purpose-fit Qt-free
application query and ApplicationServices adapter. The query reads exactly
four UTF-16 display fields and validates canonical, requested, and output
identity, uniqueness, and order. It preserves package-backed
preview filtering, unique positive teacher IDs, duplicate choice rows,
`SidebarNodeNaming`, the exact `New Teacher` missing-profile label, partial
batch results, individual profile retries after batch failure, and no-read
cases.

Fresh independent VS2026 x64/Ninja configure validated 1,334 handwritten
files. Query, adapter, and ClassTransfer targets built; focused CTest passed
3/3. The added
`ClassTransferTests::importDialogRequestsOnlyUniquePositiveTeacherIds` test
was independently rebuilt and its focused CTest passed 1/1, asserting
nonpositive filtering, deduplication, original UI candidate IDs, and one
statement. `git diff --check` passed. No full suite ran; configure had known
nonfatal warnings.

F339 is selected for a purpose-fit Schedule Testing class-choice projection.
Narrow the reader to class ID/name, grade, level, and room, removing unused
teacher ID, colors, and notes from the projection. Preserve the inner
`testing_classes`-to-`classes` join, left `class_info` join and defaults,
grade/level/name/ID order, successful empty results, Technical errors, and a
one-query bound; add a statement-metric assertion and missing-class-info
test. Use focused target
`ClassMngrNextPlatformApplicationServicesScheduleTestingClassChoicesReadPort`.
F339 is selected, not implemented. Batch 7 is complete; Batch 8 remains active
with F340-F344 queued. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.
