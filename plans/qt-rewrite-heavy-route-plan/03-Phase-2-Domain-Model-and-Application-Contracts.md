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

## Verified F90 Teacher Import apply use case - commit `df8ef0cb`

Commit `df8ef0cb Phase2 - Add Qt-free Teacher Import apply use case (F90)`
adds a Qt-free `TeacherImportUseCase` that owns review resolution, full-plan
validation, match cardinality, Korean/Native English/GS Team match-update-
create-no-op decisions and counts, latest-source-date advancement, and
transaction decisions. One persistence port binds reads, writes, date-setting,
commit, and rollback to the same transaction. The repository retains Qt
normalization, SQL, localized diagnostics, and transaction implementation.

Fake-port app-less coverage is paired with repository failure injection for a
latest-date write after three namespace writes, proving transaction rollback.
Coverage also pins padded-label handling, raw Native English and GS Team
diagnostics, and a secondary rollback warning that preserves the primary
diagnostic. Executor focused CTest passed 2/2. An independent fresh-base Tester
used `ca72c47e` plus only the six F90 paths and preserved protected manifest
blob `bf3afbe30c77e30be83df98434eda3ee466084b0`. Windows x64 Debug used CMake
4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; 929 source owners were
validated, focused targets built, and CTest passed 3/3 (`ClassMngrTeacherImportTests`
and the F89/F90 app-less targets). No full suite was run.

### Cumulative exit-gate status after F90

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F79, F80, F81, F84, F85, F87, and F89 add bounded contracts; F90 adds the Qt-free apply use case and atomic persistence port. Broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F82/F83 compare checked inputs; F86/F88 compare baseline-era source-generated inputs. F90 adds no parity evidence; historical production-workbook parity and broader coverage remain incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F90 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Post-F90 audits find audited `src/next` sources free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; the F90 use case is Qt-free. |

Both post-F90 Explorer audits leave Gate 1 and Gate 2 Partial, and find the
workspace boundary and audited `src/next` isolation satisfied. Phase 2 remains
In Progress with its exit gate Open. Broader calendar UI, generic settings
persistence, other feature-service migrations, and document-service migration
remain open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F91 adds a bounded source-generated
Teacher Import baseline differential using the existing
`TeacherImportTests::importsIntoSeparateTablesAndPreservesManualFields`
scenario, present at baseline `48fc5c5c` and current `df8ef0cb`. Hand-build a
`TeacherImportPlan` in memory and label the result source-generated synthetic
plan evidence, not workbook or historical-production-workbook evidence. Seed
deterministic in-memory SQLite with existing Native English Alex and manual
phone/nationality/email, Korean 홍길동 in room 413, Native English Alex as Team
Leader, GS Team 김하늘 as Branch Manager with phone/birthday, and source date
`2026-07-09`; existing latest date `2026-01-01` must not move backward.
Strengthen literal and normalized snapshots for all three tables and latest
date, preserve manual-field/update/count checks, and exclude generated IDs.
Compare current updated assertions against the legacy baseline using the same
test source if it compiles there (the current test calls the legacy public
`TeacherImportRepository` API); otherwise build a narrow compatible harness.
Do not overlay current repository on baseline. Focus `ClassMngrTeacherImportTests`
and report baseline/current outcome parity. This is one bounded Gate 2 scenario,
which remains Partial; Gate 1 remains Partial. Re-audit, then select F92. This
is selected work, not implementation or test evidence. Sub Prep remains capped
at 2026-2027.
