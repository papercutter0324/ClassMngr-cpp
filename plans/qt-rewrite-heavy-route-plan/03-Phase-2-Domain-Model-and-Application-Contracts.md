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
- Current note: F117 is verified at commit `3fd2b93f`; it adds an app-less
  Teacher profile edit contract. Gates 1 and 2 remain Partial; workspace create
  and the audited direct `src/next` scan remain Satisfied. Strict transitive
  isolation remains unresolved: AppServices factories retain dual-bound
  services and Workspace still reaches DataService. F118 is selected for a
  common-input Class Transfer conflict comparison using a post-baseline fixture,
  not historical production-workbook parity. Historical workbook provenance is
  a tracked risk, not a literal exit criterion. The separate DataService-
  isolation track remains open. Sub Prep remains January 1 of the reference
  date's year through December 31 of the following year at most; 2026-2027 is
  illustrative.

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

## Latest verified progress (F117)

Commit `3fd2b93f0fd87077aa59654265cda8b53b658ba9` adds the app-less
`TeacherId`, 14-field `TeacherProfileFields`/`TeacherProfile`, and
`TeacherProfileEditUseCase`. Its injected policy returns normalized fields and
structured issues (code, field, warning/error severity, bounded arguments).
Invalid IDs short-circuit; validation errors preserve issues and block writes;
warnings may continue. The use case updates normalized fields, reloads, and
returns the canonical saved profile. Update and reload failures are distinct;
reload failure records that the write succeeded.

An independent clean archive of base
`47844dfc087d47da9426e0aa06d948a1ab2264a9` overlaid exactly five paths:
`cmake/next.cmake`, `cmake/tests/next.cmake`,
`src/next/application/teacher_profile_edit_use_case.h`,
`src/next/domain/teacher_profile.h`, and
`tests/next_application_teacher_profile_edit_tests.cpp`. Their SHA-256 hashes
in that order are `30F5584639C7DBECD36409E655AF1F87A1176DAAFF130CD5DF511481DF9CE7EC`,
`38ADAD66CC58C4E0F0F225E0DFE1377195E35789D5BD6CC6226617BC11EC4E55`,
`1D073F3413A5E6B80C9A46595D432B630340F873326AF2EA76066E59704023FA`,
`DC096F4BAF78227225DC54C49007AC991C0013E6CD711B2B864241B28977530D`, and
`F90B8CD5ECBD5D22B27526C09F009FE71FCFE7CC0DFB3FBA6659EB3C726377D1`.
The forced target rebuild succeeded; focused CTest
`ClassMngrNextApplicationTeacherProfileEditTests` passed 1/1 with
`--no-tests=error`, and `git diff --check` passed. The new contracts have no
Qt or legacy production dependency. MSVC C4530 was non-blocking.

No production validation-policy adapter or `TeacherInfoPage` integration is
included. A future adapter must delegate to the existing `TeacherValidator`
and map its normalized values and issues without duplicating validation
semantics.

### Cumulative exit-gate status after F117

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | Coverage includes workspace lifecycle; Class Transfer schedule-candidate validation (F79); Teacher Import (F80/F81/F85/F87/F89/F90); Schedule Import match-key typing (F84); Calendar policies/use cases (F93/F95/F97/F99/F101-F105/F107/F110); and Teacher profile edit (F117). Broader class/schedule/roster/evaluation editing, backup/recovery, and legacy database import remain planned. |
| Baseline parity (Gate 2) | Partial | Bounded records include F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, and F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions. They do not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remains unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Partial transitive progress | The direct `src/next` source scan remains Satisfied; F111-F116 cover selected bound-session Settings, Calendar, Sub Prep, and ClassNotes paths. AppServices factories retain dual-bound services and Workspace reaches DataService; strict transitive isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Gate 1 remains Partial;
F117 advances its app-less coverage, but does not integrate the production
validation adapter or UI. Gate 2 remains Partial; bounded comparisons do not
cover every validation, conflict, planning, or state-transition behavior.
Historical production-workbook provenance remains a tracked risk, not a literal
exit criterion. Sub Prep remains bounded to January 1 of the reference date's
year through December 31 of the following year, at most; 2026-2027 is
illustrative.

### Next selected slice (F118)

Compare the baseline and current Class Transfer behavior for the same checked-in
`tests/fixtures/transfers/conflict_source.json`. Compare normalized preview /
review results, exact conflict diagnostic, complete database snapshots, and
zero writes. Label this common-input comparison as post-baseline fixture
evidence; it is not historical production-workbook parity. Gate 2 remains
Partial until all required behavior is covered.

### Independent open track: DataService isolation

Keep the strict isolation review open. The audited direct `src/next` scan and
formal workspace-create criterion remain Satisfied, but these scopes do not
resolve dual-bound AppServices factories or Workspace's DataService edge.
F116 removed the roster-output port's `hasOpenDatabase()` call; F117 does not
change isolation. Do not claim strict transitive isolation.
