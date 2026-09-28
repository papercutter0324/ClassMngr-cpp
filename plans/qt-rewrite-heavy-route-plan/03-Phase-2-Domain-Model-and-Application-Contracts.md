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
- Current note: F119 is independently verified at commit `80fbf034`; all seven
  feature-service factories use the session only. F120's solution review
  selected session-backed `ApplicationServices` operations and live DataService
  repository resolution to remove the remaining Workspace operation edge while
  preserving facade safety. Implementation and independent verification remain
  pending. Gates 1 and 2 remain Partial; workspace create and the audited direct
  `src/next` scan remain Satisfied. Historical workbook provenance is a tracked
  risk, not a literal exit criterion. Sub Prep remains January 1 of the
  reference date's year through December 31 of the following year at most;
  2026-2027 is illustrative.

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

## Latest verified progress (F119)

Commit `80fbf034d96b7d04b9be19c61de20de2c44a2f9d` changes five files:

- `src/data/data_service.h` — SHA-256 `E6EAF02E21693E9B5B687F8E657FD6687D3DEBC85B81AB05693C6C4179BCCFA7`
- `src/data/data_service.cpp` — SHA-256 `9EA5BB3BCD4034D07F8A21A87747E81141D5D2A98A9875A1AA99FC4F4F80F8C3`
- `src/core/application_services.h` — SHA-256 `B085423CEE89A53D262DF0A7E595BF5C2357E29043F945F07C87DAE99B1883F3`
- `src/core/application_services.cpp` — SHA-256 `33CAFC7EC764E0BB9E97C223157AFA0BCA7E316B8DCE245C93EEA9AF647AAC8B`
- `tests/data_service_lifecycle_tests.cpp` — SHA-256 `E3DAA625158F103CE4E95D9215397C09F7F66483C4B38353D3AF3BF38B180137`

F119 moves canonical `DatabaseSession` ownership to `ApplicationServices`,
retains `DataService` as a borrowing compatibility facade while preserving
standalone ownership, and constructs all seven feature services with the
session only. The prior latest-session handoff SHA-256 values were incorrect;
the values above were recalculated from an archive of this exact commit, and
the corresponding Git blob IDs matched the archived files.

Independent verification used a fresh `git archive` of `80fbf034`. On Windows
x64 Debug with Ninja, MSVC, and Qt 6.12, `ClassMngr` and six target executables
built in 370 Ninja steps. These CTests passed 6/6:
`ClassMngrDataServiceLifecycleTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`,
`ClassMngrDocumentCatalogTests`, `ClassMngrSubPrepPrintPdfTests`,
`ClassMngrNextPlatformApplicationServicesClassNotesSavePortTests`, and
`ClassMngrNextFeatureClassNotesPageTests`. `git diff --check` passed. Optional
missing WrapVulkanHeaders notices and long-path warnings were confined to 19
unselected test targets; none of the six selected targets showed them. No full
suite ran.

F119 removes the dual-bound factory edge but leaves Workspace's seven
operations delegating through `ApplicationServices` to `DataService`. Gate 1
and Gate 2 remain Partial; F118 remains common-input evidence on a checked-in
post-baseline fixture, not historical production-workbook parity.

### Cumulative exit-gate status after F119

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | Coverage includes workspace lifecycle; Class Transfer schedule-candidate validation (F79); Teacher Import (F80/F81/F85/F87/F89/F90); Schedule Import match-key typing (F84); Calendar policies/use cases (F93/F95/F97/F99/F101-F105/F107/F110); and Teacher profile edit (F117). Broader class/schedule/roster/evaluation editing, backup/recovery, and legacy database import remain planned. |
| Baseline parity (Gate 2) | Partial | Bounded records include F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions, and F118 common-input Class Transfer conflict behavior. They do not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remains unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Partial transitive progress | The direct `src/next` source scan remains Satisfied; F111-F116 cover selected bound-session Settings, Calendar, Sub Prep, and ClassNotes paths. F119 makes all seven feature-service factories session-only. Workspace still reaches DataService through ApplicationServices for open, close, open-state, path, save, save-as, and export; F120 targets that remaining edge. |

Phase 2 remains In Progress with its exit gate Open. Gate 1 and Gate 2 remain
Partial; F118 adds only common-input conflict evidence. Historical
production-workbook provenance remains a tracked risk, not a literal exit
criterion. Sub Prep remains bounded to January 1 of the reference date's year
through December 31 of the following year, at most; 2026-2027 is illustrative.

### Next selected slice (F120; implementation pending)

Keep the public `ApplicationServices` Workspace API and make all seven
operation implementations session-backed: use `DatabaseSession` for open,
close, open-state, path, and save commit; use a small DataService-independent
file-operation helper shared with `DataService` for save-as/export copy
behavior. Change `DataService` repository access from cached raw pointers to
live resolution through its owned or borrowed session. Workspace calls must
not notify or refresh the facade. Keep
`ApplicationServicesWorkspacePort`/`FileController` composition stable.

Acceptance:

1. A call-graph/source audit confirms Workspace's seven operations route from
   `ApplicationServices` directly to `DatabaseSession` or the file helper;
   those methods do not call `m_dataService`, which remains only for
   compatibility construction/access. `src/next` remains free of direct
   `DataService` usage.
2. Preserve operation behavior, including error and path normalization,
   open/close postconditions, save commit, save-as copy followed by the
   Workspace-port reopen with identity retained, and export leaving the active
   path unchanged.
3. Prove a borrowed facade remains valid after open-from-closed, successful
   A-to-B replacement, failed replacement preserving A, and close/reopen to B;
   it can read and write the current repository. Retain coverage for standalone
   and sessionless legacy usage.
4. Build `ClassMngr`; run focused acceptance targets
   `ClassMngrDataServiceLifecycleTests`,
   `ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
   `ClassMngrFileControllerWorkspaceLifecycleTests` as needed; check diff
   hygiene.

The three Investigator reviews converged on session-backed routing plus facade
safety. This selected seam keeps the existing adapter/controller composition
and avoids an extra Workspace port dependency while removing the
`ApplicationServices` to `DataService` operation edge. Strict transitive
isolation remains open until implementation and independent verification pass.

### Independent open track: DataService isolation

Strict transitive isolation remains open. F119 moved canonical session
ownership to `ApplicationServices` and made feature-service factories
session-only. Although the audited `src/next` scan and workspace-create
acceptance are satisfied, they do not satisfy the literal transitive-isolation
criterion while Workspace operations still delegate to `DataService`.
