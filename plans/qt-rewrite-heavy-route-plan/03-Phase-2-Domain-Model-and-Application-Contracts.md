# Phase 2 — Domain Model and Application Contracts

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-10-02
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F145 is acceptance-complete as the existing Testing Class
  details update slice; F146 is accepted as new-class creation, including the
  optional pending weekday/start-time assignment in the atomic repository
  operation. F145 preserves roster-first page behavior. F147 is accepted for
  delete/cascade and its page transition fix. F148 adds common-input successful
  save parity; F149's typed conflict query and F150's typed validation policy
  are accepted. F126 routes `ClassDetailsPage` saves through the Qt-free use
  case reused by F139 in `ScheduleEditorDialog`. F151 is accepted for
  current/baseline live-page invalid-schedule parity (malformed regular/intensive,
  end-before-start, duplicate rows); Gate 2 advances but remains Partial. F152
  is accepted: its fresh-at-save typed query and active-session adapter return
  raw signed teacher ID and exact UTF-16 notes/activity; read-error behavior,
  save request, and `ClassService` guard remain as before. F153 is accepted for
  current/baseline page parity when persisted teacher ID zero changes after
  load. F154's typed Class Notes page-read query and F155's Class Co-Teacher
  selected-class/title read and F156's ClassCoTeacherPage teacher-catalogue
  read, F157's TeacherInfoPage profile-save port, and F158's selected-teacher
  navigation read, F159's Native English Staff Directory read, F160's GS
  Team Staff Directory read, F161's Native English Staff Directory save,
  F162's GS Team Staff Directory save, F163's ClassesPage class-list read,
  F164's selected-class grade read, F165's selected-class subtitle read, and
  F166's RosterEditorWidget class subtitle read, and F167's ClassDetailsPage
  display read, F168's ClassDetailsPage save port, and F169's ClassNotesPage
  save port, F170's roster read port, and F171's roster save port are accepted.
  F172's Class Co-Teacher assignment save port, F173's ScheduleWidget
  slot-state save port, and F174's speaking-evaluation read port are accepted.
  F175's ScheduleBuilder source port, F176's Sub Prep class-details read port,
  F177's Sub Prep schedule-summary port, F178's Sub Prep print source port,
  F179's Sub Prep roster-output source port, F180's Sub Prep calendar-event
  interval port, and F181's Calendar Event Import signature query port are
  accepted. F182's active Calendar event read/by-ID adapter, F183's single
  Calendar Event save port, F184's Calendar Event delete port, F185's
  Calendar Event delete-all port, F186's repeat-series suffix-delete port,
  F187's Calendar Event Import save port, F188's repeat-series creation port,
  and F189's repeat-series edit port are accepted. F190-F197's preference
  ports, F198's Personal Details save, F199's Personal Display Name, F200's
  Personal Signature preferences, F201's Personal Signature Image, F202's
  Class Visibility, F203's Evaluation Default Policy, F204's Class Day Filter
  Reset Policy, F205's Class Selection Reset Policy, F206's Custom Color
  Palette preferences, F207's Sub Prep Personal Zoom preferences, and F208's
  Sub Prep saved-content preferences and F209's speaking-evaluation save are
  accepted; F210's recent-workspace history policy, F211's Speaking
  Evaluation read-port extraction, and F212's upcoming-birthday schedule
  policy, F213's default evaluation selection, and F214's class day-filter
  matching policy, F215's automatic-update startup eligibility, F216's
  skipped-update-version policy, and F217's roster-score import are accepted.
  F218's Speaking Evaluation class-tab integration, F219's live-state
  validation, F220's plan eligibility policy, F221's current-state snapshot,
  F222's resolution choices, F223's matching projection, F224's typed-snapshot
  prepare path, F225's apply contract, and F226's readiness orchestration are
  accepted. F227's proposed-summary projection, F228's cleared-schedule count,
  F229's typed preview projection, F230's UI-built typed apply request, F231's
  apply-request decision projection, and F232's session-bound typed apply are
  accepted. F233 is accepted: the typed apply request is the shared repository
  core input and the validated v1 plan is an edge adapter. F234 is accepted:
  the UseCase and direct typed repository entry share Qt-free validation in
  which plan eligibility runs first, followed by the intensive-mode check and
  teacher target check. Localized formatting stays at the edges, and the legacy
  plan validator is unchanged. F235 is accepted: repository state validation
  preserves exact typed teacher and class target IDs, rejecting aliases such as
  `01` before writes. F236 is accepted: structured typed policy, teacher-target,
  and fresh-state errors carry through Repository -> Platform -> dialog, while
  legacy `apply(plan)` stays localized and SQL/transaction failures remain
  message-only. F237 is accepted: Qt-free Speaking Evaluation validation and
  normalization is shared by save and page feedback; Platform persists
  normalized input and invalid data is blocked. F238 is selected to add an
  initial-setup recovery contract advancing the named backup/recovery Gate 1
  gap.
  The prior F123 candidate wording is historical;
  current Teacher Profile integration status is recorded in the progress log.
  Gates 1 and 2
  remain Partial; F120 active-v2 DataService isolation and formal
  workspace-create acceptance remain Satisfied.
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

Earlier verified slices and cumulative exit-gate snapshots are archived in the [Phase 2 progress log](03-Phase-2-Progress-Log.md).

## Latest progress update - 2026-10-02 (F237 accepted; F238 selected)

F237, source commit `8ef76074`, shares Qt-free Speaking Evaluation
normalization/content validation between the save use case and page feedback
adapter; Platform persists normalized input and invalid data is blocked. Fresh
x64 Debug verification completed 323 steps; source ownership found 1 owner for
1,177 handwritten sources. Focused Application, Platform, and page-save tests
passed 9/9, 10/10 (after parity follow-up), and 14/14; CTest passed 3/3 before
follow-up, and Korean-name differential/page-decline slots passed 3/3 each.
Both questionable-name settings match legacy issue code/location/severity and
error outcome, with expected port suppression/forwarding; declining confirmation
leaves stored state unchanged. `git diff --check` passed; no full suite ran.

F238 is selected: add a Qt-free Application contract for initial-setup profile
replacement that preserves the original during setup, finalizes on success, and
restores on cancellation, with file operations in Platform. Preserve warnings,
messages, recent-file updates, no-overwrite behavior, and original-profile
recovery after create or restore failure. Acceptance covers app-less operation
and failure ordering; adapter create/restore failures with original survival;
and FileController/MainWindow finish, cancel, failed-create, and recent-file
regressions. The historical cumulative Gate 1 map ends at F143 and still names
backup/recovery as planned ([map](03-Phase-2-Progress-Log.md#L7570)). Current
flow: [FileController create](../../src/app/controllers/file_controller.cpp#L359),
[finish](../../src/app/controllers/file_controller.cpp#L431), and
[cancel](../../src/app/controllers/file_controller.cpp#L460); lifecycle
regressions cover [failed-close initiation](../../tests/file_controller_workspace_lifecycle_tests.cpp#L639),
[finish/recent state](../../tests/file_controller_workspace_lifecycle_tests.cpp#L695),
and [cancel/recent state](../../tests/file_controller_workspace_lifecycle_tests.cpp#L741).
Two Explorer candidates disagreed; initial-setup recovery was chosen for the
explicit gap, while Teacher Import typed apply is deferred because the recovery
gap is directly named; its transaction path remains intact. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain
Partial.
