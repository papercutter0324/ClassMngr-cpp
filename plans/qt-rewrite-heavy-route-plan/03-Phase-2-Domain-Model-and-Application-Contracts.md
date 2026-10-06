# Phase 2 — Domain Model and Application Contracts

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Build and test verification

- Use `build/windows-x64-debug` as the standard local Windows x64 Debug
  build folder for slice work. Reuse this same configured folder across
  slices and phases; do not create slice-, task-, or reviewer-specific build
  folders. Build the affected targets and their dependencies, and let the
  build system recompile changed or out-of-date inputs.
- If a fresh build is needed, empty the applicable standard build folder
  before configuring and building in it. For Windows x64 Debug, keep using
  `build/windows-x64-debug`; do not create a new folder for the fresh build.
  Apply the same rule when changing the build system, toolchain, or
  dependency configuration; when stale artifacts could explain a result;
  or when a phase gate explicitly requires clean-checkout evidence.
- A gate requiring another platform or configuration must use that CMake
  preset's standard `build/<preset-name>` folder and reuse it for that
  preset, rather than creating a slice-specific folder.
- Keep any phase-specific full builds, test suites, platform matrices, and
  packaging checks required by an exit gate. Record whether verification
  used an incremental or fresh build, which targets were built, and which
  tests ran. Describe focused results as focused; do not report them as a
  full-suite pass.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-10-07
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F369 is selected for “Schedule Editor baseline parity.” Its
  discovery found no exact acceptance matrix, so its scope is not fully pinned.
  Batch 11 remains active with F370-F371 queued.

### Slice discovery batches

Discover upcoming slices in ordered batches of up to ten (or all remaining
slices if fewer than ten remain). Record each batch as an ordered list under
`Recorded batches` below and work through those slices in order. Begin discovering
and recording the next batch when starting work on the second-last slice in the
current batch. If a discovery pass finds fewer than ten slices, add the exact
standalone line `No other slices were found.` beneath that batch.

Keep the Status `Current note` limited to the latest information relevant to the
current or next slice. Keep only the most recent slice commit in the
`Latest Progress Update` section. When writing a newer update, move the previous
one to this phase's progress log before replacing it.

#### Deferred candidates

Accepted slices are removed from active tracking; their implementation and acceptance evidence remain in the chronological [Phase 2 progress log](03-Phase-2-Progress-Log.md).

- F285 - Deferred before implementation: addTeacher() creates a blank
  teacher, but required-name validation rejects it before the profile read.
  Revisit after the blank-draft creation contract is clarified.
- F298 - Deferred after review: SidebarController::addClass() re-reads the
  class only to reuse the ID returned by create(). Removing the read changes its
  failure warning/no-navigation behavior; revisit after that behavior is
  clarified.

#### Recorded batches

##### Batch 7 (complete)

1. F337 - Reuse the F332 class-details reader for Class Details validation
   context.
2. F338 - Clean up the ClassImportDialog boundary.

No other slices were found.

##### Batch 8 (complete)

1. F339 - Schedule Testing class-choice projection (accepted).
2. F340 - Sub Prep roster-output schedule-scope projection (accepted; commit
   `f02a7778`).
3. F341 - My Classes assigned-teacher profile batch projection (accepted;
   commit `71ea53da`).
4. F342 - Sub Prep roster-output teacher-profile projection (accepted;
   commit `fe190a1a`).
5. F343 - Schedule Import snapshot class-info projection (accepted; commit
   `50dfdc80`).
6. F344 - Remove redundant compatibility-service availability gates in
   migrated Classes/My Classes (accepted; commit `1bffb008`).

No other slices were found.

##### Batch 9 (complete)

1. F345 - App-less roster-save normalization and validation policy (accepted; commit `40625856`).
2. F346 - Roster row-transfer application workflow (source removal/read/prepare/atomic save; accepted; commit `5435cd0a`).
3. F347 - Class Details save orchestration (accepted; commit `b4bfcbc4`).
4. F348 - Class Transfer typed apply validation/request (accepted; commit `1af24ebb`).
5. F349 - Testing Classes delete transition baseline parity evidence (accepted; commit `5c698aa7`).
6. F350 - Testing Classes create/update persistence baseline parity (accepted; commit `d167a541`).
7. F351 - My Classes assigned-teacher display baseline parity (accepted; commit `1ecc5b09`).
8. F352 - Class Import teacher-choice display baseline parity (accepted; commit `96a59df8`).
9. F353 - Sub Prep roster-output semantic baseline parity (accepted; commit `f2bd4e71`).

No other slices were found.

##### Batch 10 (complete)

1. F354 - Class Transfer package-build Application contract; distinct from F348 apply request and F330 repository read batching; preserve package fields/order/read failures, with no assumed selection-size bound.
2. F355 - Co-teacher assignment purpose-fit persistence boundary; avoid hydrating/rewriting full ClassInfo for teacherId while preserving validation/conflict and unrelated fields (accepted; commit `0f93cbe6`).
3. F356 - Remove Speaking Evaluation compatibility-service availability gates around existing typed roster-name/selected-subtitle reads; preserve closed-session/read-failure behavior (accepted; commit `6378c369`).
4. F357 - Remove Roster Print compatibility gates around existing typed class/teacher/subtitle/extra-column reads; preserve session/errors and keep distinct from F302-F305 batching (accepted; commit `10f7060a`).
5. F358 - Remove orphan My Classes single-class information read contract/adapter/tests (no production callers); retain the F291 batch path (accepted; commit `6bca3311`).
6. F359 - Retire the unused Schedule Import compatibility helper and stale DataService include; retain the shared review-request type (accepted; commit `25f5520e`).
7. F360 - Testing Classes cancel/failure page parity; add the missing `loadTestingTeacherChoiceRecords` test-support read and compare current/baseline warning/page outcomes for F145-F147 (accepted; commit `94431d81`).
8. F361 - Class Analytics full-page baseline parity for summary/class-shape/YTD visible mappings; keep accepted query work unchanged (accepted; commit `c1ebeb8f`).

##### Batch 11

1. F362 - Sidebar class deletion Application boundary: preserve pre-confirm read warning, confirmation and page-leave guard order, cascade, and rollback (accepted; commit `88f02d36`).
2. F363 - Sidebar regular-teacher deletion Application boundary; preserve chooser, profile-read, warning, confirmation, write, and sidebar-refresh behavior (accepted; commit `93f2c2f7`).
3. F364 - Clear Testing Layout command transition; use a stateless typed Application use case/port and direct active-session Platform adapter; preserve warning, cancel/failure no-refresh, atomic failure retention, success data preservation, and Schedule/Workspace refresh (accepted; commit `f3afb54f`).
4. F365 - Initial Setup Wizard class create/save (accepted; commit `2797e8bc`).
5. F366 - Initial Setup Wizard validated teacher create (accepted; commit `cb8bd188`).
6. F367 - Class Transfer persisted apply boundary (accepted; commit `11f0ce62`; F348 request validation is already accepted).
7. F368 - Teacher Import UI apply integration using the existing Next use case (accepted; commit `cb0622ef`).
8. F369 - Schedule Editor baseline parity (selected/current; exact acceptance matrix not established).
9. F370 - Class Export picker baseline parity.
10. F371 - Roster Transfer remaining legacy availability gates.

Two independent read-only gate scans identified at least eleven candidates
without establishing that discovery is exhausted. The lower-ranked Schedule
testing-assignment gate remains for later discovery.

#### Active batch: Batch 11

1. F354-F361 - Accepted: Batch 10 is complete through Class Analytics page mapping parity.
2. F362-F366 - Accepted in Batch 11 through both Sidebar deletion boundaries, Clear Testing Layout, and Initial Setup Wizard class create/save and validated teacher create.
3. F367 - Accepted: Class Transfer persisted apply boundary (commit `11f0ce62`); F348 request validation is already accepted.
4. F368 - Accepted: Teacher Import UI apply integration using the existing Next use case (commit `cb0622ef`).
5. F369 - Selected/current: Schedule Editor baseline parity; exact acceptance matrix not established.
6. F370-F371 - Queued in the recorded Batch 11 order.

#### F299 completeness audit checkpoint

At F299 start, perform a separate completeness audit for Phase 2 slices missed
in earlier discovery or work. Keep this distinct from the planned Batch 3
discovery at F298 start.

Audit result — 2026-10-03: Two independent read-only sweeps found no missed
direct legacy-read routes outside recorded work; `SidebarController::getTeacherById()`
has no callers. The second sweep found four genuine post-migration query
fan-out candidates, independently classified as separate batching/aggregation
opportunities: F302-F305, appended after F301 in Batch 3. This F299 audit is
distinct from Batch 3's F298-start discovery of F300-F301.

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

## Latest Progress Update - 2026-10-07 (F368 accepted; F369 selected)

F368, committed as
`cb0622efe4933808a7335c5a6739142bbce20de3` (`Phase2 - Migrate Teacher Import
UI apply integration`), is accepted.

The Tester report passed focused CTests 10/10 and confirmed byte identity
between the current and F367-baseline UI transcripts. Normalized SHA-256:
`10144DFCCCDC22716AEBBDA4FE00454513EB31EF4CBDB71A448AF07171AD1862`.

F369 is selected in recorded order for “Schedule Editor baseline parity.” Its
discovery found no exact acceptance matrix, so the scope is not fully pinned.
Batch 11 remains active with F370-F371 queued. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.
