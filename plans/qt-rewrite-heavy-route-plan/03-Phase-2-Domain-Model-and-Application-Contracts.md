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
- Last updated: 2026-10-09
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F438, “Phase2 - Reject existing IDs in repeat-series creation
  (F438),” is committed as 0c2ceca6; Batch 21 is complete. F439 Delete Teacher
  QAction confirmation success is committed as a0c50d2d (branch ahead 24). Batch
  F440, “Phase2 - Cover Export Classes JSON output (F440),” is committed as
  01d1559c (branch ahead 25). F441, Import Classes QAction apply success, is
  committed as 75a559ae (branch ahead 26). F442, Import Teachers QAction apply
  success, is committed as 8cde2826 (branch ahead 27). The requested pause after
  the F442 commit was observed; the user has resumed. F443 is committed as
  1c03b326 (branch ahead 28). F444 is committed as
  8a21da618870ba4308415aaf5927fa1393af6e97 (branch ahead 29). F445 is committed as
  d9180f1c465976dfdd707382a1e615108aed9387 (branch ahead 30). F446 is committed as
  4b34a3b8a15a062377a245607228097fb43ee46b (branch ahead 31). F447 is committed as
  d5b130bd0558146185ebf4cdeab885bde6956fec (branch ahead 32); Batch 23 is complete. F448,
  “Phase2 - Cover New File QAction open-profile success (F448),” is committed as
  f825a388db1897bc42cacf43c488850fa48a9de8 (branch ahead 33); Batch 24 is complete. F449,
  “Phase2 - Cover Exit QAction close-confirmation handoff (F449),” is committed as
  4e42a5a9cbe5c261fb78b2e39a98f81c93ef0372 (branch ahead 34); Batch 25 is complete. F450,
  “Phase2 - Cover Undo QAction focused-editor dispatch (F450),” is committed as
  e39852c8a9cef33c80d684dd0e63e18d19a6acf7 (branch ahead 35); Batch 26 is complete. Its commit
  includes exactly seven approved paths and has a clean commit diff check. Batch 27 is active with
  F451, “Phase2 - Cover Redo QAction focused-editor dispatch (F451),” is committed as
  3137d522796365e81d4c3f99e341aacaf83cc392 (branch ahead 36); Batch 27 is complete. Its commit
  includes exactly six approved paths and has a clean commit diff check. Batch 28 is active with
  F452, “Phase2 - Cover Paste QAction focused-editor dispatch (F452),” is committed as
  249dd38b8b3ee2223292c99ec740f2470e20e5e9 (branch ahead 37); Batch 28 is complete. Its commit
  includes exactly six approved paths and has a clean commit diff check. F453, “Phase2 - Cover Cut QAction focused-editor dispatch (F453),” is committed as
  4e9b3d6d79aff3c43b943aec8e165a6ab16ca8a9 (branch ahead 38); Batch 29 is complete. Its commit
  includes exactly six approved paths and has a clean commit diff check. F454, “Phase2 - Cover Copy QAction focused-editor dispatch (F454),” is committed as
  dd73b6d9c536809f05d38487ecca4bc873b9ea86 (branch ahead 39); Batch 30 is complete. Its commit
  includes exactly six approved paths and has a clean commit diff check. F455, “Phase2 - Cover About QAction modal handoff (F455),” is committed as
  76c67663cda5604353b4ba25e54c97160f6daf90 (branch ahead 40); Batch 31 is complete. Its commit
  includes exactly seven approved paths and has a clean commit diff check. F456, “Phase2 - Cover Check for Updates QAction manual handoff (F456),” is committed as
  d90def93f7947d0f031dc6f37a8a4a491f4d3718 (branch ahead 41); Batch 32 is complete. The commit
  contains exactly seven scoped paths and its cached diff check was clean. F457, “Phase2 - Cover Font Size QAction application handoff (F457),” is committed as
  719efeacb6e5a8533f2dd45f6fb0fdfe152c0714 on Qt-Rewrite, 42 commits ahead of origin; Batch 33 is
  complete. Its commit contains exactly seven scoped paths and the cached diff check was clean. Batch 34 is complete. F458, “Phase2 - Cover Theme QAction application handoff (F458),” is
  committed as 2ec351a805af2064a865c34b76b46f60df80acdc on Qt-Rewrite, 43 commits ahead of origin.
  F459 Document Viewer Background QAction parity is committed as cba31503f0bb5accf68de2032131db984403deff;
  Batch 35 is complete. F460, “Phase2 - Cover Document Viewer Page Spacing QAction parity (F460),” is committed
  as fba46915d76b05aab53de85760a3f857dc4ed2a2 on Qt-Rewrite, 45 commits ahead of origin; Batch 36 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded
  latest_session_work.md and %SystemDrive%/. F461, “Phase2 - Cover Sidebar Overflow Tooltips QAction parity (F461),” was
  committed as e590ea773d4b6b1d415d248c9c3f5068094d5d4c on Qt-Rewrite, 46 commits ahead of origin; Batch 37 is
  complete. Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F462, “Phase2 - Cover Save Mode QAction application handoff (F462),” was committed
  as ad0d4de70aa5d984dd50eb0da13c1e3506e2be70 on Qt-Rewrite, 47 commits ahead of origin; Batch 38 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F463, “Phase2 - Cover Automatic Update Preference QAction parity (F463),” was
  committed as e0771d5a3b87f75f6385bff23dd869e24e237242 on Qt-Rewrite, 48 commits ahead of origin; Batch 39 is
  complete. Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F464, “Phase2 - Cover AI Comment Voice QAction prompt handoff (F464),” was committed as
  27d30bbd8452f0d350e968ef462af83ae340cdd3 on Qt-Rewrite, 49 commits ahead of origin; Batch 40 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F465, “Phase2 - Cover AI Comment Provider QAction dialog handoff (F465),” was committed as
  599916bada7a2a59ae041dc80d59cba2187cf3be on Qt-Rewrite, 50 commits ahead of origin; Batch 41 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F466, “Phase2 - Cover Custom Website provider QAction modal handoff (F466),” was committed as
  0abbaf9238a25407f3c8a6c88a07adb4e93aa238 on Qt-Rewrite, 51 commits ahead of origin; Batch 42 is complete.
  Its commit contains exactly seven scoped paths and the cached diff check was clean. Post-commit status was clean except for
  excluded pre-existing latest_session_work.md and %SystemDrive%/. F467, “Phase2 - Cover Sidebar Marquee QAction parity (F467),” was committed as
  0384c3768c18009689f918f456ae932c0c1d5a89 on Qt-Rewrite, 52 commits ahead of origin; Batch 43 is complete.
  Its commit contains exactly seven paths and the cached diff check was clean. Post-commit status was clean except for excluded pre-existing
  agent_docs/latest_session_work.md and %SystemDrive%/. F468, “Phase2 - Cover Document Catalog Language Preference QAction parity (F468),” was committed as
  219ca8fb64b50247c24a722f7a091d75a2a6d86e on Qt-Rewrite, 53 commits ahead of origin; Batch 44 is complete. Its commit
  contains six scoped paths and the cached diff check was clean. Batch 45 is active with F469 Document Viewer Page Spacing
  None/Medium QAction parity accepted in this changeset and ready to commit. F469 acceptance and focused verification are
  recorded in the progress log; F470 discovery begins only after F469 commits.
  F457 acceptance and verification remain in the progress log. F456
  acceptance and focused verification remain in the progress log. F454
  acceptance and focused verification remain in the progress log. F453
  acceptance and focused verification remain in the progress log.
  F452 acceptance remains in the progress log. F451 acceptance remains recorded there.
  F451 acceptance remains in the progress log. F450 acceptance remains recorded there. F449/F448
  acceptance and verification remain in the progress log. F435 covers
  no-database New Profile creation and picker metadata; F444 covers Open File replacement.
  F285 stays deferred. See the
  progress log. Gates 1 and 2 remain Partial.

### Slice discovery batches

Discover upcoming slices in ordered batches of up to ten (or all remaining
slices if fewer than ten remain). Keep only the active batch below. Begin
discovering and recording the next batch when starting work on the second-last
slice in the current batch. If a discovery pass finds fewer than ten slices,
add the exact standalone line `Batch 20 read-only reviews have surfaced provisional candidates after F421; see the Phase 2 progress log.` beneath that batch.

Keep the Status `Current note` limited to the latest information relevant to the
current or next slice. Remove accepted slices from this plan; keep their
implementation and acceptance evidence in the chronological [Phase 2 progress
log](03-Phase-2-Progress-Log.md).

#### Deferred candidates

- F285 - Deferred before implementation: addTeacher() creates a blank
  teacher, but required-name validation rejects it before the profile read.
  Revisit after the blank-draft creation contract is clarified.
- F298 - Deferred after review: SidebarController::addClass() re-reads the
  class only to reuse the ID returned by create(). Removing the read changes its
  failure warning/no-navigation behavior; revisit after that behavior is
  clarified.

#### Active batch: Batch 45

F469 Document Viewer Page Spacing None/Medium QAction parity is accepted in this changeset and ready to commit. The
existing F460 MainWindow test now triggers the actual None and Medium actions on its blank PDF viewer, verifies OptionState,
exclusive action checks, persisted values 0/2, and QPdfView pageSpacing values 0/16 px, while retaining Small/Large
coverage. The scoped Small restorer syncs on assertion exits.

Focused target build and CTest passed 1/1; executor direct QtTest passed 3/0/0. Independent CTest passed 1/1 and
git diff --check passed. The independent direct invocation exited 0 without a QtTest summary, so no independent case
count is claimed. Qt reported an optional missing fonts-directory warning; bundled Inter/Pretendard fonts loaded. No full
suite was run. Batch 45 remains active until F469 commits; F470 discovery begins after that commit.

Batch 21 began with ten candidates after F428 and is complete; F429-F438 are
committed. F438, “Phase2 - Reject existing IDs in repeat-series creation (F438),”
is commit 0c2ceca6. F439, “Phase2 - Cover Delete Teacher QAction confirmation
success (F439),” is commit a0c50d2d. F440, “Phase2 - Cover Export Classes JSON
output (F440),” is commit 01d1559c. F441, “Phase2 - Cover Import Classes QAction
apply success (F441),” is commit 75a559ae. F442, “Phase2 - Cover Import Teachers
QAction apply success (F442),” is commit 8cde2826 on Qt-Rewrite (branch ahead
27). The requested pause after F442 was observed and the user has resumed. F443 is
committed as 1c03b326567cf52d808bc4c54b7a5e77021bb7bf on Qt-Rewrite (branch ahead
28); its acceptance evidence remains recorded in the progress log. Batch 22 is
F444 is committed as 8a21da618870ba4308415aaf5927fa1393af6e97 on Qt-Rewrite (branch ahead 29); its acceptance
and focused verification remain recorded in the progress log. F445 is committed as d9180f1c465976dfdd707382a1e615108aed9387 on Qt-Rewrite (branch ahead 30); its acceptance
and focused verification remain recorded in the progress log. F446 is committed as 4b34a3b8a15a062377a245607228097fb43ee46b on Qt-Rewrite (branch ahead 31); acceptance and
verification remain recorded in the progress log. Batch 22 is complete. F447 is committed as d5b130bd0558146185ebf4cdeab885bde6956fec on Qt-Rewrite (branch ahead 32); its acceptance and
verification remain recorded in the progress log. Batch 23 is complete. F448 is committed as
f825a388db1897bc42cacf43c488850fa48a9de8 on Qt-Rewrite (branch ahead 33); its acceptance and
focused verification remain recorded in the progress log. F449 is committed as
4e42a5a9cbe5c261fb78b2e39a98f81c93ef0372 on Qt-Rewrite (branch ahead 34); Batch 25 is complete.
F450 is committed as e39852c8a9cef33c80d684dd0e63e18d19a6acf7 on Qt-Rewrite (branch ahead 35);
Batch 26 is complete. Its commit includes exactly seven approved paths and has a clean diff check.
F451 is committed as 3137d522796365e81d4c3f99e341aacaf83cc392 on Qt-Rewrite (branch ahead 36);
Batch 27 is complete. Its commit includes exactly six approved paths and has a clean diff check.
F452 is committed as 249dd38b8b3ee2223292c99ec740f2470e20e5e9 on Qt-Rewrite (branch ahead 37);
Batch 28 is complete. Its commit includes exactly six approved paths and has a clean diff check.
F453, “Phase2 - Cover Cut QAction focused-editor dispatch (F453),” is committed as
4e9b3d6d79aff3c43b943aec8e165a6ab16ca8a9 on Qt-Rewrite (branch ahead 38); Batch 29 is complete.
Its commit includes exactly six approved paths and has a clean diff check. F455, “Phase2 - Cover About QAction modal handoff (F455),” is committed as
76c67663cda5604353b4ba25e54c97160f6daf90 on Qt-Rewrite (branch ahead 40); Batch 31 is complete.
The commit includes exactly seven approved paths and has a clean diff check. F456, “Phase2 - Cover Check for Updates QAction manual handoff (F456),” is committed as
d90def93f7947d0f031dc6f37a8a4a491f4d3718 on Qt-Rewrite (branch ahead 41); Batch 32 is complete.
The commit contains exactly seven scoped paths and its cached diff check was clean. Batch 33 is complete. F457, “Phase2 - Cover Font Size QAction application handoff (F457),” is
committed as 719efeacb6e5a8533f2dd45f6fb0fdfe152c0714 on Qt-Rewrite, 42 commits ahead of origin.
Its commit contains exactly seven scoped paths and the cached diff check was clean. Batch 34 is complete. F458, “Phase2 - Cover Theme QAction application handoff (F458),” is
committed as 2ec351a805af2064a865c34b76b46f60df80acdc on Qt-Rewrite, 43 commits ahead of origin.
Its commit contains exactly seven scoped paths and the cached diff check was clean. F459 Document Viewer
Background QAction parity is committed as cba31503f0bb5accf68de2032131db984403deff; Batch 35 is complete.
F460, “Phase2 - Cover Document Viewer Page Spacing QAction parity (F460),” is committed as
fba46915d76b05aab53de85760a3f857dc4ed2a2 on Qt-Rewrite, 45 commits ahead of origin; Batch 36 is complete.
The commit contains exactly seven scoped paths. Post-commit status was clean except for excluded
  latest_session_work.md and %SystemDrive%/. F461, “Phase2 - Cover Sidebar Overflow Tooltips QAction parity (F461),” was
  committed as e590ea773d4b6b1d415d248c9c3f5068094d5d4c on Qt-Rewrite, 46 commits ahead of origin; Batch 37 is
  complete. Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F462, “Phase2 - Cover Save Mode QAction application handoff (F462),” was committed
  as ad0d4de70aa5d984dd50eb0da13c1e3506e2be70 on Qt-Rewrite, 47 commits ahead of origin; Batch 38 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F463, “Phase2 - Cover Automatic Update Preference QAction parity (F463),” was
  committed as e0771d5a3b87f75f6385bff23dd869e24e237242 on Qt-Rewrite, 48 commits ahead of origin; Batch 39 is
  complete. Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F464, “Phase2 - Cover AI Comment Voice QAction prompt handoff (F464),” was committed as
  27d30bbd8452f0d350e968ef462af83ae340cdd3 on Qt-Rewrite, 49 commits ahead of origin; Batch 40 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F465, “Phase2 - Cover AI Comment Provider QAction dialog handoff (F465),” was committed as
  599916bada7a2a59ae041dc80d59cba2187cf3be on Qt-Rewrite, 50 commits ahead of origin; Batch 41 is complete.
  Its commit contains exactly seven scoped paths. Post-commit status was clean except for excluded pre-existing
  latest_session_work.md and %SystemDrive%/. F466, “Phase2 - Cover Custom Website provider QAction modal handoff (F466),” was committed as
  0abbaf9238a25407f3c8a6c88a07adb4e93aa238 on Qt-Rewrite, 51 commits ahead of origin; Batch 42 is complete.
  Its commit contains exactly seven scoped paths and the cached diff check was clean. Post-commit status was clean except for
  excluded pre-existing latest_session_work.md and %SystemDrive%/. F467, “Phase2 - Cover Sidebar Marquee QAction parity (F467),” was committed as
  0384c3768c18009689f918f456ae932c0c1d5a89 on Qt-Rewrite, 52 commits ahead of origin; Batch 43 is complete.
  Its commit contains exactly seven paths and the cached diff check was clean. Post-commit status was clean except for excluded pre-existing
  agent_docs/latest_session_work.md and %SystemDrive%/. F468, “Phase2 - Cover Document Catalog Language Preference QAction parity (F468),” was committed as
  219ca8fb64b50247c24a722f7a091d75a2a6d86e on Qt-Rewrite, 53 commits ahead of origin; Batch 44 is complete. Its commit
  contains six scoped paths and the cached diff check was clean. Batch 45 is active with F469 Document Viewer Page Spacing
  None/Medium QAction parity accepted in this changeset and ready to commit. F469 acceptance and focused verification are
  recorded in the progress log; F470 discovery begins only after F469 commits.
remain in the progress log. F456 acceptance remains recorded there; F455/F454/F453/F452/F451 evidence remains there.
F435 covers opening from the no-database banner.
F444 covers replacing a distinct open profile and excludes same-path opening,
dirty replacement choices, and
load-failure behavior. F285 remains deferred. This bounded discovery does not establish
repository-wide exhaustion.
Batch 20 completed when F428 committed as b82bddaa. F429 committed as 19f6024d. Batch 21 was
discovered while F427 was the second-last known Batch 20 candidate and is now active.
Candidate evidence and limits are in the Phase 2 progress log; this bounded
discovery does not establish repository-wide exhaustion.

Batch 18 was discovered at F402 start from two independent bounded reviews and
activated after F403 committed as 28e881cd; it completed when F413 committed
as 5d8a941a. Batch 19 was discovered at F412 and activated after Batch 18
completed.

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
