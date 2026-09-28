# Phase 2 — Domain Model and Application Contracts

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-29
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F122 is verified as baseline-present, hand-authored Schedule
  Import Skip state parity; it does not establish historical-workbook
  provenance. F123 integrates the app-less Teacher Profile Edit use case with
  `TeacherInfoPage`; F124 integrates typed co-teacher assignment with
  `ClassCoTeacherPage`. Both passed their focused tests. F120 active-v2
  DataService isolation and formal workspace-create acceptance remain
  Satisfied. Gates 1 and 2 remain Partial. F125's `ClassNotesPage` use-case
  integration is the next bounded candidate. Historical workbook provenance
  remains a tracked risk, not a literal exit criterion. Sub Prep remains
  January 1 of the reference date's year through December 31 of the following
  year at most; 2026-2027 is illustrative.

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

## Latest verified progress (F124)

Source/test commit
`a159591e48e312f96378302b105a3ad1276382b3` (`Phase2 - integrate co-teacher
assignment use case`) connects `ClassCoTeacherPage` to the app-less
`ClassCoTeacherAssignmentUseCase`. The use case validates positive IDs and
maps the UI's `-1` unassigned sentinel to a missing teacher ID. The
session-backed platform adapter reloads the current `ClassInfo`, changes only
the teacher assignment, and calls `ClassService::saveClassInfo`, preserving
class details, notes, and schedules. The page retains its manual warning,
dirty-state, title-refresh, and `classInfoSaved` behavior.

The `windows-x64-debug` preset built `ClassMngr`, the app-less use-case test,
the platform-adapter test, and the page integration test. The three focused
CTest targets passed 3/3, covering invalid IDs, assigned/unassigned states,
persisted-field and schedule preservation, unavailable service handling, page
title/signal updates, and warning/dirty-state behavior. `git diff --check`
passed. The existing `ClassMngrClassesPageTests` target also built, but its
CTest run failed 22 test cases; failures include `page.openClass(42, ...)`
returning false and widget validation assertions. Its initial Details open
failure occurs before the co-teacher editor opens. That target emitted
duplicate-stub `/FORCE` linker warnings and a stale build dependency warning
for `class_navigation_preferences.h`. No baseline comparison or full suite ran.

Gate 1 and Gate 2 remain Partial. F124 completes production-page integration
for co-teacher assignment but does not close the remaining class-detail,
schedule, roster, evaluation, backup/recovery, or legacy database-import gaps.
F120 active-v2 DataService isolation and the formal workspace-create boundary
remain Satisfied. Phase 2 remains In Progress with its exit gate Open.

### Cumulative exit-gate status after F124

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | Coverage includes workspace lifecycle; Class Transfer schedule-candidate validation (F79); Teacher Import (F80/F81/F85/F87/F89/F90); Schedule Import match-key typing (F84); Calendar policies/use cases (F93/F95/F97/F99/F101-F105/F107/F110); teacher-profile editing (F117/F123); and co-teacher assignment (F124). Broader class-detail/schedule/roster/evaluation editing, backup/recovery, and legacy database import remain planned. |
| Baseline parity (Gate 2) | Partial | Bounded records include F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions, F118 common-input Class Transfer conflict behavior, F121 common-input successful replacement state, and F122 hand-authored seeded Schedule Import Skip state parity. They do not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remains unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for active `src/next` call paths | F120 removes the Workspace operation edge to `DataService`; the source audit confirms all seven operations route to `DatabaseSession` or the file helper. Legacy facade construction/access remains available. |

Phase 2 remains In Progress with its exit gate Open. Gate 1 and Gate 2 remain
Partial; historical production-workbook provenance remains a tracked risk, not
a literal exit criterion. Sub Prep remains bounded to January 1 of the
reference date's year through December 31 of the following year, at most;
2026-2027 is illustrative.

### Next candidate (F125; bounded solution review pending)

Integrate the existing `ClassNotesSaveRequest` with an app-less
`ClassNotesSaveUseCase` and route `ClassNotesPage` through it. Preserve the
10,000 UTF-16 code-unit limit, trimmed values, session-backed platform save,
manual warning, and dirty-state behavior. The page currently calls its
application port directly; the request already owns text-limit validation and
the platform adapter defensively repeats it. Review validation and port
ordering, with a no-write oversized-input case and the existing page/adapter
regressions. Candidate scope is pending acceptance; no F125 implementation
has begun. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.
