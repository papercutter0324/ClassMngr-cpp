# Phase 2 — Domain Model and Application Contracts

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-27
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: Gate 1 and Gate 2 remain Partial. F93 adds a Qt-free
  start-of-term calendar policy used by both Calendar consumers. Broader
  calendar UI/contracts, generic settings persistence, remaining feature-service
  migrations, document-service migration, invalid-UTF-8 coverage, and live
  MainWindow projection-failure/retranslation integration remain open. The
  formal workspace criterion and audited src/next dependency isolation are
  Satisfied; a broader FileController integration gap remains outside the
  written workspace criterion. Sub Prep remains capped at 2026-2027.

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

## Verified F93 Calendar start-of-term policy - commit `c73f1469`

Commit `c73f1469` (`Phase2 - Extract Calendar start-of-term application policy
(F93)`) changes seven paths. The new Qt-free `CalendarEventStartOfTermPolicy`
is registered in `cmake/next.cmake`; both `CalendarEventModel` and
`CalendarPage` consumers use it. The redundant legacy Domain helper is removed
and the Calendar Import assertion is migrated. App-less tests cover all four
aliases, title case and space simplification, known and unknown type fallback,
hybrid nonmatches, the hide switch, and U+0085/NEL whitespace for title and
type.

Independent fresh Windows x64 Debug verification from base `c999a235` plus
only the seven paths built `ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrCalendarEventCacheTests`, `ClassMngrCalendarImportTests`, and
`ClassMngr` (358 actions; MSVC 19.51.36257, CMake 4.4.2, Ninja 1.13.2, Qt
6.12.0). Focused CTest passed 3/3. A direct Qt 6.12 probe confirmed legacy and
policy agreement for NEL-separated `new semester` and trailing NEL after
`Vacation`. No full suite was run; `git diff --check` passed.

### Cumulative exit-gate status after F93

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F93 adds a Qt-free start-of-term policy and routes both Calendar consumers through it; broader Calendar UI and contract coverage remain incomplete. |
| Baseline parity (Gate 2) | Partial | F92 compares a baseline-present, source-generated valid workbook flow through validation, parsing, M1 review selection, plan creation, and apply. Historical production-workbook evidence remains missing; F94 is limited to invalid-date validation parity. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator criterion remains satisfied. Audits separately note a broader FileController integration gap outside that written criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Two independent post-F93 audits find audited `src/next` sources free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies. |

Gate 1 and Gate 2 remain Partial; the formal workspace criterion and audited
`src/next` isolation remain Satisfied. Phase 2 remains In Progress with its
exit gate Open. Open scope includes broader Calendar UI/contracts, generic
settings persistence, remaining feature-service migrations, document-service
migration, invalid-UTF-8 coverage, and live MainWindow projection-failure/
retranslation integration. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F94 compares baseline-present
synthetic invalid-date Teacher Import workbook validation. Use identical bytes
from `testWorkbookData("invalid-date")` through each revision's own validator;
pin the byte hash if stable, `RecognizedButInvalid`, the discovered M1 section,
and the exact A1 diagnostic `Cell A1 must contain a version date such as
26.07.09ver.`. This is negative validation parity only: the current validator
makes no repository calls, and the UI disables import for an invalid result.
Do not claim repository snapshot/no-write or transactional rollback parity.
Focus current `ClassMngrTeacherImportTests`; because the current test file
references `next/application/import_review_session.h`, which is absent at
baseline, a narrow baseline harness may be needed. No full suite. Gate 2 remains
Partial and historical production-workbook evidence remains missing. This is
selected work, not implementation or test evidence. Sub Prep remains capped at
2026-2027.
