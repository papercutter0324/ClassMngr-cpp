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
- Last updated: 2026-10-04
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F303 is selected to batch RosterPrintDialog per-class
  selected-subtitle queries after F261, preserving its current-class-only
  branch.

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

#### Recorded batches

1. **Batch 1**
   1. F285 — Deferred before implementation: `addTeacher()` creates a blank
      teacher, but required-name validation rejects it before the profile read.
      Revisit after the blank-draft creation contract is clarified.
   2. F286 — Use the returned workspace session location for FileController's
      successful create/open current-file state.
   3. F287 — Remove ClassImportDialog's unreachable direct class-name lookup;
      preserve the existing formatted label when subtitle data is unavailable.
   4. F288 — Route the Campus Dashboard selector list through its accepted
      campus-directory query.
   5. F289 — Route roster-template printing's class-scope enumeration through
      the accepted classes-list query.
   6. F290 — Route roster-template printing's per-class roster read through the
      accepted roster query.
   7. F291 — Provide My Classes a dedicated compact class-information
      query/snapshot/port with one `ApplicationServices` adapter read; retain
      its accepted class-list, roster-count, and full teacher-profile queries.
   8. F292 — Route the sidebar's Korean birthday-directory read through the
      accepted query and adapter.
   9. F293 — Route sidebar class-teacher assignments through the accepted
      typed query, snapshot, and port with an active-session
      `ApplicationServices` adapter, preserving unassigned classes and
      existing sidebar behavior.
   10. F294 — Add an accepted latest-import-date read for teacher import.

2. **Batch 2**
   1. F295 — Pass accepted classes-list ID/name data through the roster-template
      print pipeline to remove the per-class `classroom()` lookup. F289 already
      migrates scope enumeration; the extra read remains in
      `src/features/roster/services/roster_template_print_private_service.inc:64-87`.
   2. F296 — Route roster-template printing's per-class full class-information
      read through a purpose-fit application projection. The printer also needs
      room and Zoom details beyond class-list/subtitle projections; see
      `src/features/roster/services/roster_template_print_private_service.inc:81-88`
      and `src/features/roster/services/roster_template_print_shared_data.inc:343`.
   3. F297 — Add a selected-campus detail read for Campus Dashboard. Although
      F288 migrates the selector list, `CampusDashboardPage::loadSelectedCampus()`
      directly read `m_repository.loadCampus()` for address, directions, map,
      transit, and office fields before F297; preserve
      save-before-read and silent missing-campus behavior.
   4. F298 — Deferred after review: `SidebarController::addClass()` re-reads
      the class only to reuse the ID returned by `create()`. The identity is
      mechanically available from `ClassService::create()`'s `Result<int>` and
      repository last-insert ID, but removing the read changes its dedicated
      failure warning/no-navigation behavior; `openClass()` can select another
      class or none. No test clarifies the intended post-create failure
      behavior, so defer pending a semantic decision.
   5. F299 — Add an application-facing Class Analytics dashboard read/use case
      for current-roster and historical/YTD projections. `ClassAnalyticsPage::rebuild()`
      still calls `SpeakingEvaluationService::analyticsDashboard()`
      (`src/features/classes/ui/class_analytics_page.cpp:528`), which reads
      roster/evaluation data and composes analytics in
      `src/features/classes/services/feature_services.cpp:1470`; the accepted
      single-evaluation read does not cover this dashboard composition.

No other slices were found.

3. **Batch 3**
   1. F300 accepted — Reduce repeated per-matching-class
      `SelectedClassSubtitleReadQuery` calls for destination labels in
      `ClassImportDialog`. This is query fan-out after F262's query migration,
      not a remaining direct service read.
   2. F301 accepted — Reduce per-class selected-subtitle query calls in
      `ClassExportDialog` after its classes-list load. This is query fan-out
      following F262/F266, not a remaining direct service read.
   3. F302 accepted (discovered in separate F299 audit) — Batch class-delete
      chooser subtitle-label queries following F275, reusing the accepted
      subtitle batch API.
   4. F303 selected (discovered in separate F299 audit) — Batch
      `RosterPrintDialog` per-class selected-subtitle queries after F261;
      preserve the current-class-only branch.
   5. F304 candidate (discovered in separate F299 audit) — Batch
      `RosterPrintDialog` extra-column roster reads after F264; preserve scope,
      column union, and failure fallback.
   6. F305 candidate (discovered in separate F299 audit) — Batch transfer-menu
      target metadata and target-roster reads after F259/F265; keep distinct
      from F273's transfer-time target read.

No other slices were found.

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

## Latest Progress Update - 2026-10-04 (F302 accepted; F303 selected)

F302, committed as `1ffc88a32a3b12bc0354deac2f485d61abb15ef9`, reuses
`SelectedClassSubtitleBatchReadQuery` and the active-session adapter in the
class-delete chooser. For nonempty class IDs, it batches subtitle labels when
both services are available while preserving class-list order and IDs,
class-list errors, empty-list early return, blank item and selection/cancel
behavior, and the stored-name/`Class N` fallback when either service is
unavailable. Class-data failures retain default formatting; teacher-data
failures retain class details with `No Teacher`. After selection, confirmation
still makes a fresh one-class read; an integration test changes the subtitle
after chooser population and checks the fresh confirmation text.

The two-class integration test asserts one batch class-repository call, one
metadata statement, one schedule statement, one teacher batch statement,
original labels/order, selected ID, and deletion. The empty-list UI test asserts
no batch reads and no modal. Fresh independent Windows x64 MSVC/Ninja Debug
configure passed the ownership gate at 1,270 handwritten sources. `ClassMngr`,
`ClassMngrNavigationTeacherReadTests`, the classes-list query target, and single
and batch subtitle application-query and adapter targets built; six focused
CTests passed. Independent source review confirmed one guarded batch query in
the chooser and no per-class single query; the separate confirmation read
remains. `git diff --check` passed. Logs are under
`build/f302_verify_ninja/` (`configure.log`, `build-targets.log`,
`build-targets-recheck.log`, `ctest-focused.log`). Nonfatal `vswhere.exe` and
optional Vulkan messages occurred. The full suite was not run.

Coverage limits: the fallback for class-service available/teacher-service
unavailable is source-reviewed but not directly integration-tested; current
services derive availability from the same database session, and the existing
no-session test exits before showing the chooser. No chooser-cancel or
all-item-ID enumeration test was added; the integration test verifies label
order and selects/deletes the Beta ID.

F303 is selected to batch `RosterPrintDialog` per-class selected-subtitle reads
after F261 while preserving its current-class-only branch. F298 remains
deferred pending the read-failure warning/navigation decision, and F299's
separate completeness audit remains distinct from Batch 3 discovery. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.
