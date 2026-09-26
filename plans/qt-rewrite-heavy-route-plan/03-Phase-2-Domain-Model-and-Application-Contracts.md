# Phase 2 — Domain Model and Application Contracts

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-26
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Current note: Typed Domain/Application contracts now cover workspace lifecycle,
  selection state, import/report jobs, document sessions/catalogs, calendar
  projections and mutations, and feature preference boundaries. The Application
  layer remains Qt-free. Broader calendar UI, generic settings persistence,
  remaining feature-service migrations, and document-service migration remain
  open. Invalid-UTF-8 boundary coverage and live UI integration are non-blocking
  gaps; MainWindow projection-failure/retranslation has no integration test.
  Sub Prep remains capped at 2026-2027.

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

## Verified F84 Schedule Import matching-key typing - commit 5207d65a

The candidate and teacher projection now store `Domain::KoreanTeacherKey`;
the empty key is explicitly initialized as a valid value, and the display name
remains separate. `schedule_import_repository.cpp` owns conversion to and from
the legacy representation. Matching behavior, including empty-key matching,
remains covered.

Executor and independent fresh Tester verification passed
`ClassMngrNextApplicationScheduleImportMatchingProjectionTests` and
`ClassMngrScheduleImportTests` (2/2). The Windows x64 Debug verification used
CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; CMake validated
921 handwritten source owners and the build completed 313 actions. `git diff
--check` passed. No full suite was run.

### Cumulative exit-gate status after F84

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Teacher Import update policies; F84 adds typed Schedule Import matching keys. Broader behavior remains incomplete. |
| Baseline parity | Partial | F82 and F83 compare legacy/current behavior on checked inputs and identical seeds, but both fixtures postdate the legacy baseline. They add common-input regression, not historical-output parity. Earlier fixture regressions remain; broader parity is incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F84 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F84's policy remains Qt-free. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress with its
exit gate Open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice: F85 extracts Teacher Import full-plan validation
from `teacher_import_repository.cpp` into a Qt-free Application policy. Keep
Qt normalization and date interpretation, review/validation order, translation,
and exact diagnostics at the repository adapter; pass normalized identity keys
and date validity into the policy. Preserve Korean, Native English, and GS Team
name rules and rejection-before-write behavior. Verify the app-less policy and
focused Teacher Import repository tests. This is selected work, not
implementation evidence. Sub Prep remains capped at 2026-2027.
