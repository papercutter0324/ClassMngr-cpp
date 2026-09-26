# Phase 2 — Domain Model and Application Contracts

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-27
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Current note: Qt-free Domain/Application contracts now cover workspace and
  selection state, imports (including Teacher Import apply), report jobs,
  document sessions/catalogs, calendar projections and mutations, and feature
  preference boundaries. Broader calendar UI, generic settings persistence,
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

## Verified F92 Teacher Import generated-workbook baseline parity - commit `3f6ef73d`

Commit `3f6ef73d64c81b8e6e5f6ee665dc85e9b976408e` changes only
`tests/teacher_import_tests.cpp`. F92 pins baseline-present helper
`testWorkbookData()` output: a source-generated XLSX of 3,422 bytes with SHA-256
`9cdccb43d7fe5e5e1abb83630ede8b18e6dd2c4824dbb288dc81d60371496daa`. The test
validates and parses those bytes, explicitly selects the sole Korean M1
candidate, creates and applies an import plan, then checks the seeded results
and preserved manual and unrelated values. The baseline/current semantic
transcript SHA-256 matches at
`095d595311aaee2444d67a893d45a2d0f97fe366c2a667f90bdd690205a2bdb4`.

Fresh focused CTest passed 1/1. The test executable reported 21 passed, 0
failed, 1 optional external-sample skip; the selected scenario's 3 assertions
passed. This is source-generated synthetic workbook evidence, not parity with
a historical production workbook. Invalid-date validation parity and
repository rollback parity remain open.

### Cumulative exit-gate status after F92

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F92 changes tests only and adds no app-less contract behavior; broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F92 compares a valid baseline-present source-generated workbook flow through validation, parsing, review selection, plan creation, and apply. It does not establish historical production-workbook parity; invalid-date validation and repository rollback parity remain open. |
| Workspace boundary | Satisfied | The formal documented WorkspaceGateway/WorkspaceCoordinator acceptance remains satisfied; F92 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Two post-F92 audits find audited `src/next` sources free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F92 changes tests only. |

Gate 1 and Gate 2 remain Partial; the workspace boundary and audited
`src/next` isolation remain Satisfied. Phase 2 remains In Progress with its exit
gate Open. Broader calendar UI/contracts, generic settings persistence,
remaining feature-service migrations, document-service migration,
invalid-UTF-8 coverage, and live MainWindow projection-failure/retranslation
integration remain open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F93 extracts the duplicated
start-of-term calendar-event classification used by `CalendarEventModel` and
the `CalendarPage` upcoming-event filter into an app-less policy. Preserve the
current rule: simplify and lowercase the title; normalize the event type with
unknown values mapped to `Other`; classify only `Other` events whose normalized
title is exactly one of `new semester`, `start of term`, `term start`, and
`term starts`; hide matching events only when the hide preference is true.
Acceptance: one policy owns classification; app-less tests cover title and type
normalization, all four exact titles, nonmatches, and both hide-preference
states, while both production call sites suppress matches only when hiding is
enabled. Use existing `NextApplicationCalendarEventTests` in
`tests/next_application_calendar_event_tests.cpp`. This adds bounded Gate 1
evidence but leaves Gate 1 Partial and does not itself complete Phase 2. This
is selected work, not implementation or test evidence. Sub Prep remains capped
at 2026-2027.

After F93, the next Gate 2 candidate is baseline-present invalid-date synthetic
Teacher Import workbook rejection parity: compare status and diagnostic on
baseline/current inputs and prove repository state is unchanged. F92's valid
workbook flow leaves that validation and rollback evidence open.
