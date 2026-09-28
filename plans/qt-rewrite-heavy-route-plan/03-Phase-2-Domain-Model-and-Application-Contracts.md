# Phase 2 — Domain Model and Application Contracts

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-28
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F118 is verified at commit `b68eba6d`; its Class Transfer
  comparison is common-input evidence on a checked-in post-baseline fixture, not
  historical production-workbook parity. Gates 1 and 2 remain Partial;
  workspace create and the audited direct `src/next` scan remain Satisfied.
  Strict transitive isolation remains unresolved: F119 targets the seven
  dual-bound service factories, while Workspace still reaches DataService.
  Historical workbook provenance is a tracked risk, not a literal exit
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

## Latest verified progress (F118)

Commit `b68eba6dd93c1eaa6473ec9494a4d9e7da9980ef` changes only
`tests/class_transfer_tests.cpp` (SHA-256
`71FA243D9C641EA955A5B33201478C76BCEDFAE7A333D0AF7727AE8E14FFB1E8`). Its
focused test uses `tests/fixtures/transfers/conflict_source.json` (SHA-256
`BED9CBEE84A7946F51029EFC4AA2B2850BDA9784757250FBF6CE90CAB7FB173`) and pins
seeded preview matches, normalized review choices and plan, the exact combined
regular/intensive schedule-collision diagnostic, unchanged snapshots of all
application tables including `sqlite_sequence`, and zero `total_changes` under
`query_only`.

The independent Tester used a fresh archive of current base
`5263aebf8222e16e3085498af851a7f0d3041818` with only this test overlay. The
legacy baseline was `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, with the fixture
unchanged and only the F118 helper/includes/slot/case transplanted. Both
focused CTests passed 1/1 and the exact error and persisted-state assertions
matched. Verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt
6.12.0. The temporary baseline tree needed three CMake minimum references
changed from 6.11.1 to 6.12.0; no baseline production code changed.
`git diff --check` passed. No full suite ran.

F118 is common-input comparison evidence on a checked-in post-baseline fixture;
it is not historical production-workbook parity. Gate 2 remains Partial.

### Cumulative exit-gate status after F118

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | Coverage includes workspace lifecycle; Class Transfer schedule-candidate validation (F79); Teacher Import (F80/F81/F85/F87/F89/F90); Schedule Import match-key typing (F84); Calendar policies/use cases (F93/F95/F97/F99/F101-F105/F107/F110); and Teacher profile edit (F117). Broader class/schedule/roster/evaluation editing, backup/recovery, and legacy database import remain planned. |
| Baseline parity (Gate 2) | Partial | Bounded records include F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions, and F118 common-input Class Transfer conflict behavior. They do not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remains unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Partial transitive progress | The direct `src/next` source scan remains Satisfied; F111-F116 cover selected bound-session Settings, Calendar, Sub Prep, and ClassNotes paths. All seven feature-service factories remain dual-bound and Workspace reaches DataService; F119 targets the factory edge but strict transitive isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Gate 1 and Gate 2 remain
Partial; F118 adds only common-input conflict evidence. Historical
production-workbook provenance remains a tracked risk, not a literal exit
criterion. Sub Prep remains bounded to January 1 of the reference date's year
through December 31 of the following year, at most; 2026-2027 is illustrative.

### Next selected slice (F119)

Move canonical `DatabaseSession` ownership from `DataService` to
`ApplicationServices`, keep `DataService` as a borrowing compatibility facade
while preserving standalone `DataService` ownership, and construct all seven
feature services with the session only. Limit implementation to
`data_service.h/.cpp`, `application_services.h/.cpp`, and lifecycle tests. This
removes the dual-bound service-factory edge; it does not close the Workspace
path, which still delegates through `ApplicationServices` to `DataService`, or
pass strict transitive isolation.

### Independent open track: DataService isolation

Keep strict isolation open. DataService currently owns the canonical session;
ApplicationServices creates it through DataService, and all seven
feature-service factories pass both the session and DataService. Workspace
operations still delegate through ApplicationServices to DataService. F119
targets session ownership and the factory edge but leaves the Workspace edge;
direct `src/next` isolation and workspace-create acceptance do not satisfy the
literal transitive-isolation criterion.
