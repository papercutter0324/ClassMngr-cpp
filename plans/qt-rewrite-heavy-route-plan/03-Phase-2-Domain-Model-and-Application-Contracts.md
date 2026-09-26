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

## Verified F91 Teacher Import generated-plan baseline parity - commit `e814b4fc`

Commit `e814b4fc84d5c7e455a07688e096b077e3ed0f11` (`Phase2 - Pin Teacher
Import generated baseline parity (F91)`) changes only
`tests/teacher_import_tests.cpp`. The existing
`importsIntoSeparateTablesAndPreservesManualFields` scenario labels its
hand-built `TeacherImportPlan` as source-generated synthetic evidence. It pins
apply counts (Korean 1/0/0, Native English 0/1/0, GS Team 1/0/0), every
non-ID field of each single persisted table row, Alex's manual phone, birthday,
nationality, and email plus the updated Team Leader position, and that source
date `2026-01-01` does not move the latest date backward from `2026-07-09`.
Existing duplicate, no-write, ambiguity, and rollback cases remain.

Executor focused `ClassMngrTeacherImportTests` CTest passed 1/1. An independent
Tester built from current docs HEAD `17bbc8d4` plus only the final test file
(blob `786770ca816b40f00c7626461ed58ec0a3e66c22`, SHA-256
`8BB8206A59D27E09E1C82BE918445670B32220F2CB4FD8011041646CD2BA0720`); the
executable reported 20 passed, 0 failed, 1 skipped (the optional sample needs
`CLASSMNGR_TEACHER_IMPORT_SAMPLE`), and CTest passed 1/1. Its narrow common-input
harness passed on archived baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`
and the current F90 repository, with equivalent synthetic plan and SQLite
seed; exact JSON semantic outputs matched at SHA-256
`755A2B7DF52B3DD8F111ADF2EC0D0127415689B2B08A68D008505079D887B2B6`.
Current and legacy repository blobs were `7f2604023d1c7b8f8c3132acd1fcfe8e6ed2759a`
and `058aa4d0b85fb05d5cc04a2c6d0f72d99ba9cc71`. The full current test file
does not compile on baseline because `next/application/import_review_session.h`
is absent there, so the narrow harness used baseline production sources and
schema without overlaying current production code. Protected manifest blob
`bf3afbe30c77e30be83df98434eda3ee466084b0` was neither overlaid nor consumed.
Windows x64 used MSVC 19.51 and Qt 6.12.0. `git diff --check` passed; no full
suite was run. This is source-generated synthetic plan/repository evidence,
not workbook or historical production-workbook parity.

### Cumulative exit-gate status after F91

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F91 changes tests only and adds no app-less contract behavior; broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F91 advances parity by one baseline-present, source-generated synthetic plan/repository scenario. It does not cover workbook decode/preview or a historical production workbook; broader coverage remains incomplete. |
| Workspace boundary | Satisfied | The formal documented WorkspaceGateway/WorkspaceCoordinator acceptance remains satisfied; F91 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Independent audits find audited `src/next` sources free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F91 changes tests only. |

Gate 1 and Gate 2 remain Partial; the workspace boundary and audited
`src/next` isolation remain Satisfied. Phase 2 remains In Progress with its exit
gate Open. Broader calendar UI/contracts, generic settings persistence,
remaining feature-service migrations, document-service migration,
invalid-UTF-8 coverage, and live MainWindow projection-failure/retranslation
integration remain open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F92 extends Teacher Import parity
upstream through baseline-present source-generated helper `testWorkbookData()`.
Run identical generated workbook bytes through validation/parser, explicit M1
review selection, plan creation, and `TeacherImportRepository::importTeachers`
on baseline `48fc5c5c` and current sources using equivalent deterministic
SQLite seeds. Pin the bytes/hash if stable, template/date/parsed Korean M1
candidate, apply counts, normalized persisted teacher row, latest source date,
and unrelated seeded state; exclude generated IDs. The helper creates
source-generated synthetic XLSX bytes with one Korean M1 candidate and source
date `2026-07-09`; this is synthetic workbook evidence, not historical
production-workbook parity. The checked-in `sectioned_review.xlsx` postdates
baseline. The current full test source needs a narrow legacy harness because
`next/application/import_review_session.h` is absent at baseline. Focus
`ClassMngrTeacherImportTests`; no full suite. Gate 2 advances but remains
Partial, and Gate 1 remains Partial. Calendar start-of-term classification is
a later Gate 1 candidate, not selected here. This is selected work, not
implementation or test evidence. Sub Prep remains capped at 2026-2027.
