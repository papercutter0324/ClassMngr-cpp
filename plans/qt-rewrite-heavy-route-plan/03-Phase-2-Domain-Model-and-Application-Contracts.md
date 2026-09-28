# Phase 2 — Domain Model and Application Contracts

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-28
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F122 is verified as baseline-present, hand-authored Schedule
  Import Skip state parity; it does not establish historical-workbook
  provenance. F120 active-v2 DataService isolation and formal workspace-create
  acceptance remain Satisfied. Gates 1 and 2 remain Partial. F123 is a
  candidate pending bounded solution review; implementation has not begun.
  Historical workbook provenance remains a tracked risk, not a literal exit
  criterion. Sub Prep remains January 1 of the reference date's year through
  December 31 of the following year at most; 2026-2027 is illustrative.

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

## Latest verified progress (F122)

Commit `e3411e733d2ce402a03096e715150e45b4244dff` (`Phase2 - pin Schedule
Import Skip parity`) changes only `tests/schedule_import_tests.cpp` (+67/-9;
Git blob `eb422208842551eb37a4783f31982b82cab674b3`, extracted SHA-256
`9697009E56FE575EE0303F54413CCBA205B17EF716278E9E5C1158E7343BB066`). Its
fresh archive tree ID is `ca903158a6e8ff2866987b4873e5be3f5e92f845`; TAR
SHA-256 `402DB5014595E72B84836AA131B08C482FC23551B6D3FC192C6AB6ED52895440`.

Independent fresh-archive verification against pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` passed the exact focused case
`ScheduleImportTests::skippedExactMatchPreservesItsSchedule` 3/3 on both
current and baseline. The baseline archive TAR SHA-256 is
`1870A9BDE4087B061CA3C35B6EB0804B10335222D12B0C98F095343D504F7FD7`; its
overlay used the same seed and test with the exact F122 helper/method and only
the missing `QCryptographicHash`/`QSqlRecord` includes. Both runs produced
identical complete final snapshots (SHA-256
`08ad64ed3d853e52a1a686c1683d0d1fe8a289026e21d21081e09ee4840c5ebe`), including
raw `class_times.id` and `sqlite_sequence`. The test preserved Tuesday-then-
Monday order, asserted one skipped row and zero schedules cleared, and retained
profile behavior.

Current and baseline builds passed on Windows 11 x64 with MSVC 19.51.36257 and
Ninja 1.13.2; current used Qt 6.12 and baseline Qt 6.11.1. The baseline reused
its cleanly configured archive dependencies. A `QSqlRecord` compile issue was
fixed only by the two missing includes in the temporary baseline overlay; no
shared checkout files changed. No full suite ran. `git diff --check` passed.

F122 is hand-authored, baseline-present seeded evidence, not historical
workbook provenance. Gate 1 and Gate 2 remain Partial; F120 active-v2
DataService isolation and the formal workspace-create boundary remain
Satisfied. Phase 2 remains In Progress with its exit gate Open.

### Cumulative exit-gate status after F122

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | Coverage includes workspace lifecycle; Class Transfer schedule-candidate validation (F79); Teacher Import (F80/F81/F85/F87/F89/F90); Schedule Import match-key typing (F84); Calendar policies/use cases (F93/F95/F97/F99/F101-F105/F107/F110); and Teacher profile edit (F117). Broader class/schedule/roster/evaluation editing, backup/recovery, and legacy database import remain planned. |
| Baseline parity (Gate 2) | Partial | Bounded records include F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions, F118 common-input Class Transfer conflict behavior, F121 common-input successful replacement state, and F122 hand-authored seeded Schedule Import Skip state parity. They do not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remains unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for active `src/next` call paths | F120 removes the Workspace operation edge to `DataService`; the source audit confirms all seven operations route to `DatabaseSession` or the file helper. Legacy facade construction/access remains available. |

Phase 2 remains In Progress with its exit gate Open. Gate 1 and Gate 2 remain
Partial; historical production-workbook provenance remains a tracked risk, not
a literal exit criterion. Sub Prep remains bounded to January 1 of the
reference date's year through December 31 of the following year, at most;
2026-2027 is illustrative.

### Next candidate (F123; bounded solution review pending)

Explore integrating F117's app-less existing-teacher edit use case with the
production `TeacherInfoPage` flow. The candidate boundary delegates validation
to `TeacherValidator` without duplicating rules, preserves warning/error
mapping, uses the session-backed update/reload path, and retains canonical
reload plus current autosave, header, and signal behavior. Two independent
Explorers agreed on the candidate; Investigator solution review was interrupted
at the user's stop request. Scope is not accepted and implementation has not
begun. Phase 2 remains In Progress/Open; Gate 1 and Gate 2 remain Partial.
