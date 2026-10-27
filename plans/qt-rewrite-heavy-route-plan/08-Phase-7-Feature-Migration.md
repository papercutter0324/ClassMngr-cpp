# Phase 7 — Feature Migration

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
- Depends on: Phases 2 through 6
- Blocks: Final parity, memory hardening, and cutover
- Owner: Codex (Heavy route)
- Last updated: 2026-10-26
- Current note: 7A.1–7A.3 are functionally verified and committed; 7A.3 is
  `01aaf3d7`. Candidate #4, setup teacher import, is active. The 7A screenshot
  comparison and memory measurement remain open, as do the 7A and Phase 7 exit
  gates. Batch 1 remains active and unpushed. See the progress log for 7A.3
  evidence and limits.

### Slice batches

Use the shared [current slice-batch tracker](Current-Slice-Batches.md) for live work when Phase 7 is active. It contains only the active
batch and the next batch once discovery starts; completed batches are
not archived there.

## Progress log

Use this section only when a decision, acceptance result, verification
finding, blocker, or exit-gate detail needs a durable record. Routine
slice chronology and closed batch history do not need to be archived;
keep current status in this plan and live batches in the shared tracker.

### 7A.1 — Profile-bootstrap stage accepted (2026-10-26)

- Commit: `d49f34fe` (`Phase7 - Add v2 setup profile bootstrap`). Git was
  verified clean; the branch is one commit ahead of origin.
- Acceptance: the user-visible v2 profile-bootstrap stage creates a real `.tps`
  through `WorkspaceStoreGateway`, supports cancel/failure and explicit restart
  recovery, and keeps `InitialSetupLifecycle` pending across later
  setup-screen slices. Keep the original-profile backup; candidate #13 owns
  recent-history insertion after the full setup workflow completes.
- Verification passed on macOS Debug using `build/macos-clang-debug`: build of
  `ClassMngrNext`, `ClassMngrNextPageHostTests`, `ClassMngrNextBootstrapTests`,
  `ClassMngrNextApplicationInitialSetupLifecycleTests`, the legacy platform
  lifecycle test, and the new store-backed setup lifecycle test; focused CTest
  passed 5/5. The fixture-backed platform test covers real SQLite seed/reopen
  and recovery. `createActionUsesSelectedPathAndDoesNotRecordPendingProfile`
  clicks the visible action with a fake dialog, verifies the selected path, one
  recent-history read, and zero saves. English/Korean × light/dark appearance
  passed 4/4 offscreen. `python3
  scripts/phase4/generate_resource_inventory.py --check` and five generator
  tests passed. Two focused verifiers independently confirmed these results.
- Limits and next work: the Windows preset is unavailable on this macOS host;
  no full suite, Windows build, cross-platform run, package, or memory gate ran.
  Full wizard/setup, recent-history completion, and remaining 7A flows remain
  open.

### 7A.2 — Personal details and signature functionally verified (2026-10-26)

- Commit: `1f9b30bf` (`Phase7 - Add setup personal details and signature`). At
  handoff the worktree is clean and the branch is two commits ahead of origin.
- Result: independent Tester recheck found no production defect. The personal
  details/signature screen is functionally verified. The Phase 7 screenshot
  comparison requirement remains open for 7A: no retained Phase 0/7 screenshot
  matches the legacy setup wizard or the new screen. Existing label, theme, and
  geometry assertions are not screenshot comparisons.
- Verification passed on macOS: six assigned targets built; focused CTest
  passed 5/5. `imageSignatureSaveReopensAndResumesFromFixture` verifies image
  mode persistence and preview after reopening/resuming a real `.tps`.
  `failedPersonalDetailsSavePreservesReplacementBackup` verifies setup remains
  pending, journal/backup hash stay unchanged, and seeded records survive a
  forced write failure. Both new QtTests passed three cases each. The appearance
  test passed six QtTest cases, including setup/cleanup, across English/Korean
  and light/dark. Resource inventory `--check` and `git diff --check` passed.
- Limits and next work: Windows, full suite, and package checks remain unrun;
  Phase 7 and 7A exit gates remain open. The screenshot/reference comparison
  remains an open 7A parity item. At the 7A.2 handoff, 7A.3 teacher entry was
  next.

### 7A.3 — Teacher-entry setup accepted and functionally verified (2026-10-26)

- Commit: `01aaf3d7`.
- Verification: independent focused suite passed 15/15; Bootstrap CTest passed
  1/1. The restored 7A.2 personal-details backup test passed directly 3/3 and
  was unchanged from HEAD. UI-to-WorkspaceStore integration passed 3/3,
  covering `.tps`, pending journal/state, and zero recent-history saves. Store
  failure/retry/reopen/resume passed 3/3 with backup hash and journal unchanged.
  English/Korean × light/dark appearance passed 6/6. Next startup dependency
  boundary passed; `ClassMngrSpeakingAnalyticsTests` passed 2/2. The
  `ClassMngrNext` target linked successfully; source ownership validated 1,705
  sources. Resource inventory `--check` and `git diff --check` passed.
- Limits and next work: the legacy QWizard parity test aborts on macOS at
  `NSBundle initWithURL:nil` before assertions. Tester-forced CMake regeneration
  stalled, although the executor's configure and production target builds
  succeeded. No full suite, Windows, package, or memory test ran. No matching
  legacy setup/personal-details screenshot exists, so screenshot comparison and
  memory measurement remain open 7A items. Candidate #4, setup teacher import,
  is active; 7A and Phase 7 exit gates remain open.

## Objective

Move every feature from domain behavior through persistence, application use cases, presentation, resources, output, localization, and tests.

## Vertical-slice rule

A feature is not migrated when its page compiles. It is migrated only when:

- Domain behavior is implemented.
- Persistence behavior is implemented.
- Application use cases are used.
- The new page matches the baseline.
- Resource loading is scoped and observable.
- Output behavior matches.
- English and Korean are covered.
- Light and dark themes are covered.
- Existing files and imports work.
- Memory behavior is measured.
- Old call paths are removed.

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) is the cross-cutting memory
acceptance companion for these vertical slices. Apply its compact-projection,
model/view, operation-scope, and release rules within the owning feature; do
not defer all memory work to Phase 9.

## Migration order

### 7A — Setup and workspace/file workflows

Migrate:

- Initial setup.
- New/open/save/save-as.
- Recent files.
- Backups and recovery.
- Legacy database import.
- .tps handling.
- Import and export errors.

Break the large setup wizard into small screens driven by application commands and explicit state.

Acceptance:

- New users can complete setup.
- Existing supported workspaces open.
- Failed setup and failed migration preserve recoverability.

### 7B — My Workspace and personal information

Migrate:

- Personal details.
- Signature image.
- Typed signature rendering.
- My Classes.
- Workspace calendar.
- Upcoming events.
- Birthdays.
- Workspace navigation.

Load only the initial workspace data required for the visible page. Calendar and schedule detail must not be fully materialized just because the workspace exists.

My Classes must use compact class summaries and one reusable selected-class
detail view rather than one rich widget tree per class. Workspace child pages
must be created on activation and released on suspension according to the
[memory hotspot remediation plan](memory-hotspot-remediation-plan.md).

### 7C — Teachers and staff

Migrate:

- Teacher information.
- Teacher imports.
- Native-English teacher view.
- GS team.
- Staff directory.
- Birthday views.

Use compact models and shared records. Do not keep duplicate teacher datasets in repository, service, page, and table layers.

The Staff Directory slice must replace per-cell table-item allocation with a
model/view table and active-cell editors, as specified in the [memory hotspot
remediation plan](memory-hotspot-remediation-plan.md).

### 7D — Classes

Migrate:

- Class list.
- Class details.
- Co-teachers.
- Notes.
- Analytics.
- Testing classes and blocks.
- Speaking-evaluation defaults.
- Class-transfer behavior.

Split the current large class page into section presenters sharing explicit class application state.

Class navigation must remain lightweight: create detail and editor content for
the selected class or active section, and release it when the class context or
page lifecycle requires it. Do not materialize one full detail page per class.
See the [memory hotspot remediation plan](memory-hotspot-remediation-plan.md).

### 7E — Schedule and import workflows

Migrate:

- Schedule display.
- Schedule editing.
- Workbook parsing.
- Teacher/class matching.
- Conflict planning.
- Import review.
- Testing assignments.
- Schedule output.
- Schedule printing.

Use streaming or bounded workbook processing. Release parser workbooks, import plans, review data, and temporary render data as soon as their stage completes.

This is a priority memory feature. The schedule view must not recreate a cell-widget-per-entry architecture.

The [memory hotspot remediation plan](memory-hotspot-remediation-plan.md)
also owns the import-review boundary: stage workbook data, build matching
indexes once, use model-backed review rows, and release raw workbook and
temporary preview data as soon as each stage completes.

### 7F — Calendar

Migrate:

- Calendar repository and model.
- Workbook import.
- Event filtering.
- Event editing.
- Preferences.
- Month-grid display.

Keep the current QML month grid behind an adapter initially if required for visual parity. Measure its memory cost before choosing a QWidget replacement.

Calendar import must not retain raw, decoded, compatibility-cell, and derived
event representations unnecessarily. Construct and release the calendar view
through the shared lifecycle defined in the [memory hotspot remediation
plan](memory-hotspot-remediation-plan.md).

### 7G — Rosters

Migrate:

- Student model.
- Roster editor.
- Headers and delegates.
- Imports.
- Transfers.
- Template selection.
- Printing.

Instantiate editors only for active rows or cells. Load roster designs and print assets on demand.

### 7H — Speaking evaluations

Migrate:

- Evaluation persistence.
- Evaluation grid.
- Private notes.
- Comment editing.
- Batch workflows.
- Analytics.
- Reports.
- AI-assisted comment workflow.
- PDF output.
- PowerPoint automation.
- Export jobs.

Preserve the current behavior of opening the configured external AI website. Do not introduce a hosted AI API as part of this rewrite.

Report images, PowerPoint objects, PDF documents, and temporary workspaces require explicit lifetime and cleanup tests.

Speaking evaluation navigation and batch review must use lightweight summary
navigation, model/view tables, bounded report batches, and operation-scoped
output data. Apply the [memory hotspot remediation
plan](memory-hotspot-remediation-plan.md) before accepting the 7H slice.

### 7I — Campus and documents

Migrate:

- Campus information.
- Directions.
- Address.
- Housing.
- Maps.
- Campus staff.
- Document catalog.
- Document viewer.

Load catalog metadata and localized names initially. Do not load PDF bodies or
render QtPdf pages while building the catalog or starting the application.
When the user opens a document, load only the selected PDF into the active
viewer session; close and release it when the document is replaced, the viewer
is closed, or the page is left, suspended, or released. Reopening may load the
document again. Load campus maps and campus JSON only when opened, and decode
images at a bounded display resolution.

### 7J — Substitute preparation and output

Migrate:

- Substitute-preparation page.
- Class-information model.
- Document package generation.
- PDF rendering.
- Printing.
- Output-folder handling.

Stream or stage large outputs instead of retaining source documents, rendered pages, and final buffers simultaneously.

#### 7J.1 - Memory-bounded class information and output pipeline

This is the owning implementation slice for the large-workspace Sub Prep
failure recorded in Phase 0. Implement the complete vertical path using the
[Sub Prep Class Information and Output Memory
Plan](sub-prep-class-information-memory-plan.md):

- compact class-summary and selected-detail application contracts;
- bounded, visible-ID-filtered persistence queries;
- model/view navigation with a reusable selected-class detail panel;
- explicit page suspension, invalidation, and release behavior;
- operation-scoped package/PDF input and bounded output rendering;
- large-fixture memory, lifecycle, visual, and output acceptance.

The bounded fixture may prove route semantics quickly, but the 96-class
large-workspace fixture remains the required scalability test. Phase 9 owns the
packaged Release memory gate; it does not replace this Phase 7J migration.

The broader [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) coordinates 7J with the other
feature slices and requires the same distinction between compact resident
page state and operation-scoped package/PDF state.

## Per-feature deliverables

Every subphase must produce:

- Application use cases.
- Persistence integration.
- New presenter/view model.
- New Qt view.
- Resource policy.
- On-demand document-load and viewer-release trace for document features.
- Output adapter usage.
- Localization updates.
- Unit and integration tests.
- Visual parity screenshots.
- Memory measurement.
- Old call-path removal list.

## Phase-level exit gate

Every feature in the preservation matrix works in v2, and no migrated feature requires DataService, old page ownership, resource-pack leases, or direct filesystem path guessing.

## Heavy-route requirements

- For every Phase 7 feature slice, use the heavy route: migrate the complete
  vertical path from domain and persistence through application, UI, resources,
  output, and tests before accepting the slice.
- Migrate complete vertical slices.
- Do not leave old and new code paths permanently interleaved.
- Do not accept a feature based only on a happy-path screenshot.
- Test large datasets, imports, errors, printing, localization, and repeated navigation.
- Keep the current user workflow unless a deliberate product change is separately approved.
