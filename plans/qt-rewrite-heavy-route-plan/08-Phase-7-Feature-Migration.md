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
- Last updated: 2026-10-28
- Current note: 7A.1–7A.15 and candidates #23–29 remain accepted. Batch 3
  candidate #30, Schedule display/editing, is active. The 7A screenshot
  comparison and memory evidence remain open, as do the 7A and Phase 7 exit
  gates. See the
  progress log for slice evidence and limits.

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
  remains an open 7A parity item.

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
  memory measurement remain open 7A items. The 7A and Phase 7 exit gates remain
  open.

### 7A.4 — Setup teacher import accepted (2026-10-26)

- Commit: `8bee6dbb` (`Phase7 - Add setup teacher import`). The Next-owned
  workflow validates workbooks asynchronously using the Next Excel import
  timeout preference, handles retry and stale results, and maps review
  directly to the typed `TeacherImportUseCaseRequest` applied through the
  pending `WorkspaceStore` transaction. It does not use the legacy
  teacher-import UI or `ApplicationServices` apply port. Setup rejection and
  cancellation use existing recovery.
- The unchanged fixture
  `tests/fixtures/teacher_import/sectioned_review.xlsx` parses five section
  markers: M1 (two Korean teachers), M2 (one), H1 (one), NET Alex, and GS
  Taylor; its source date is 2026-09-01. Integration selects one M1 teacher,
  skips M2, imports H1, and includes Alex and Taylor automatically. A forced GS
  insertion failure rolls back Korean and NET writes and the source date;
  retry succeeds. Tests verify setup stays pending, recent history is not
  written, backup and journal survive import, and cancel after an invalid
  workbook restores the original profile.
- Workbook parser sources now use a narrow Qt Core+ZLIB target shared with
  legacy consumers; `Qt6::Concurrent` is an explicit Next dependency. The
  resource inventory was regenerated and its check passed. An independent
  Tester built focused targets and ran CTest 6/6. QtTest totals were 107
  passed, 0 failed, 1 skipped (the optional external sample path was unset),
  across legacy import (30), Bootstrap (50), setup integration (3), page host
  (15), and workflow (9), plus the dependency boundary. Coverage includes
  English/Korean × Light/Dark and asynchronous timeout, retry, and stale-result
  behavior. Resource check, `git diff --check`, and source ownership check
  passed; ownership has one explicit owner for 1,710 handwritten sources.
- Limits and next work: no full suite, Windows, package, peak-RSS measurement,
  or screenshot parity was verified. 7A and Phase 7 exit gates remain open;
  screenshot comparison and memory measurement remain open 7A items.

### 7A.5 — Setup class details accepted (2026-10-27)

- Commit: `932fdfb2` (`Phase7 - Add setup class details`). After teacher entry,
  setup captures a typed in-memory class-details draft in `ApplicationBootstrap`;
  it creates no class, class-details, or schedule rows. Candidate #6 owns
  persistence once schedule rows are ready.
- Requirements: a Korean teacher, chosen color, Grade, and Level are required;
  Reading and Essay Books are optional. A single teacher is selected
  automatically; multiple choices require explicit selection, and teacher-read
  failures remain visible and retryable. Color selection preserves the 16-slot
  palette through a direct `WorkspaceStore` adapter. The shared catalog now
  lives in `src/domain/config/class_details_catalog.h` and serves both legacy
  Course/ClassInfoConfig and Next. Invalid grade/level fallback matches
  committed legacy behavior (15 Reading Smart choices; N/A essay choices for
  E4/E5/E6/M1/M2/M3); valid M1/M2 GraVoca choices remain.
- The real `.tps` integration imports Korean teachers through
  `WorkspaceStore`, selects an imported teacher, captures the draft, then
  cancels. It verifies no class/details/schedule writes, zero recent-history
  saves, original-profile restoration, and backup/journal cleanup. An
  independent Tester verified exact fallback parity and the combined
  import/draft/cancel path.
- Verification: focused CTest passed 12/12. Nine QtTest executables reported
  151 passed, 0 failed, 0 skipped, plus three standalone checks. The
  resource-baked build produced 194 runtime resource records; source ownership
  validated 1,717 files. Dependency-boundary, resource-inventory, and
  `git diff --check` checks passed.
- Limits and next work: full suite, Windows, package, screenshots, QWizard
  parity, and peak-memory were not checked. Screenshot comparison and memory
  measurement remain open 7A items; 7A and Phase 7 exit gates remain open.

### 7A.6 — Setup class times accepted (2026-10-27)

- Commit: `97d45d3d` (`Phase7 - Add setup class times`). The screen consumes
  the typed 7A.5 draft, creates a blank class, and adds order-preserving
  regular/intensive schedule rows using the shared catalog. It starts with one
  regular Monday 4:00–4:55 PM row and validates that both modes have rows
  before writing. Class details and both schedule tables save transactionally
  through direct `WorkspaceStore` adapters. Setup remains pending and recent
  history is not saved. Create failure leaves no class ID; details-save failure
  rolls back details and schedule rows while retaining the blank class ID for
  retry, matching legacy behavior.
- The real pending-`.tps` integration imports a Korean teacher and checks the
  draft, details, books, color, ordered regular/intensive rows, create and
  intensive-insert failures, successful retry, backup/journal preservation,
  no recent-history write, and cancel restoration of the original profile.
  It directly checks all five end-duration values at representative starts in
  both modes, the 9:55 PM cap, absence of over-cap options, and typed weekday
  and time values.
- Verification: independent Tester passed six focused CTests, including the
  setup class-times requirement and Next dependency boundary. Integration
  passed 8/8 with no skips or failures. CMake ownership validated 1,719 files;
  the resource-baked build contained 194 runtime resources. Resource inventory
  and `git diff --check` passed.
- Limits and next work: full suite, Windows, package, screenshots, and
  peak-memory checks were not run. The 7A screenshot comparison and memory
  measurement, plus the 7A and Phase 7 exit gates, remain open.

### 7A.7 — Setup schedule import accepted (2026-10-27)

- Commit: `a83b7280` (`Phase7 - Add setup schedule import`). The optional
  Import Schedule and Skip/Finish choices follow setup class times. Next owns
  the source/review flow, typed review/apply use cases, and direct
  `WorkspaceStore` apply adapter. Import uses a coherent store read snapshot
  and transactional repository; the shared workbook-reader/schedule-parser
  CMake boundary stays narrow. Next does not use the legacy
  `ScheduleImportDialog`/Review UI or `ApplicationServices`.
- Normal import may replace regular schedules. Intensive `UpdateExisting`
  preserves absent classes; `ReplaceWithNew` clears them. Review and applied
  status show clear counts, including an integration case that reports one
  cleared schedule after omitting a 7A.6 manual class and a zero-clear case.
  Teacher/class resolutions, room/color data, diagnostic acknowledgement, and
  profile-name choices map into the typed request.
- Import apply leaves setup pending. Source-dialog cancel performs setup
  recovery; review cancel returns to the source. Explicit Finish or Skip and
  Finish finalizes setup; that explicit finish action records recent history
  and cleans the backup/journal.
- Verification: independent Tester passed focused CTest 19/19; QtTests passed
  legacy schedule dialog 60/60, Next schedule workflow 16/16, and `.tps` setup
  integration 9/9. The legacy parser passed 99 tests with one optional
  external-workbook check skipped because `CLASSMNGR_SCHEDULE_IMPORT_SAMPLE`
  was unset. Coverage included cancellation, rollback/retry, normal replacement,
  intensive preserve/replace, profile-name consent, clear counts, finish/skip,
  and control-to-request mapping. `git diff --check`, resource inventory
  (194 runtime records: 187 direct, 7 generated; 5 translations, 2 QML), CMake
  ownership (1,727), 1,889 compiled Korean translations, and the dependency
  boundary passed.
- Limits and next work: no full suite, package, Windows, screenshot, QWizard
  parity, or peak-memory check ran. The 7A screenshot comparison and memory
  measurement, plus the 7A and Phase 7 exit gates, remain open.

### 7A.8 — Normal workspace creation and replacement accepted (2026-10-27)

- Commit: `95c1243a` (`Phase7 - Add normal workspace creation`). The File menu
  action and New shortcut create an empty schema-v6 `.tps` with no setup-pending
  state. A typed `WorkspaceCreationLifecycle` policy uses a direct
  `WorkspaceStore` adapter; this flow does not use `ApplicationServices`,
  `FileController`, or `InitialSetupLifecycle`. It guards pending setup/recovery
  and confirms page leave before opening the dialog.
- Path policy preserves `.tps` case, normalizes trailing `.db` to `.tps`, and
  appends `.tps` to other suffixes. Existing targets move to a recovery sibling
  before replacement; failures restore originals when safe. Recent history is
  written once, only after success.
- Independent Tester: CTest passed 7/7 and the new-flow QtTest passed 14/14.
  Coverage includes same-path replacement, failed-create restore/reopen,
  partial-workspace close, close and cleanup failures, retained recovery warning,
  recent-history write failure, cancellation, and suffix behavior. Source
  ownership validated 1,732 files; `git diff --check` passed. Resource inventory
  `--check` passed after a metadata-only regeneration; manifest matches the tree
  and all 187/187 direct content hashes.
- Limits and next work: recovery-name collision, failed partial-file cleanup or
  move, failed original restoration, and failed prior-workspace reopen remain
  untested; no product defect was reproduced. No full suite, Windows, package,
  screenshot, or peak-memory check ran. The 7A screenshot comparison and memory
  measurement, plus the 7A and Phase 7 exit gates, remain open.

### 7A.9 — Existing teacher profiles open in Next accepted (2026-10-27)

- Commit: `cdbf3ead` (`Phase7 - Open existing teacher profiles in Next`). The
  Next File menu adds Open Teacher Profile… with the platform Open shortcut and
  `.tps` filter, using the application flow and `WorkspaceStore` candidate-open
  path. Cancel has no side effects. Pending setup/recovery or unreadable state
  blocks interactive opening; the dirty-workspace replacement guard remains.
  Missing, corrupt, and unsupported-schema profiles fail without closing the
  active workspace and can be retried. Success refreshes navigation and records
  history once; a history-save failure does not undo the open.
- Independent verification: fresh `build/macos-clang-debug`; focused CTest
  passed 3/3 (`ClassMngrNextBootstrapTests`, `ClassMngrNextNavigationTests`,
  `ClassMngrNextWorkspaceOpenFlowTests`) and
  `NextStartupWorkspaceDependencyBoundary` passed 1/1. Configure validated one
  owner for 1,735 handwritten sources; resource bake contained 187 direct and
  7 generated entries; `git diff --check` passed. Coverage includes success,
  cancel, missing/corrupt/unsupported-schema failure and retry, session and
  selection preservation, dirty rejection, setup/recovery blocking,
  success-only history, history-save failure, and navigation refresh.
- Limits and next work: no separate OS permission-denied fixture, full suite,
  Windows, packaging, screenshot, or memory gate ran. The 7A screenshot
  comparison and memory evidence, plus the 7A and Phase 7 exit gates, remain
  open.

### 7A.10 — Save, close, and dirty state accepted (2026-10-27)

- Commit: `283871fa` (`Phase7 - Add workspace save close and dirty handling`).
  File > Save and Save and Close Teacher Profile use Next workspace contracts;
  clean Save is a no-op. Typed Save/Discard/Cancel handles Page drafts
  separately from workspace/database dirtiness and guards navigation, Open,
  New/replacement, File Close, and shutdown. Failed save/discard/close retains
  the active profile safely; successful Close returns to no-profile setup.
  File Close is blocked if setup is pending or recovery is present/unreadable;
  the active session and journal remain unchanged. Window shutdown may close
  the store and preserve recovery for restart.
- Verification: standard `build/macos-clang-debug`; configure found one owner
  for 1,739 handwritten sources; focused CTest passed 7/7, including
  `NextStartupWorkspaceDependencyBoundary`. The Save QAction test covers dirty
  commit success, injected failure with open/dirty state retained, and retry.
  `git diff --check` passed. Unrelated pre-existing schedule-import edits are
  uncommitted and excluded from this commit.
- Limits and next work: clean Save is tested as a no-op because an empty
  `WorkspaceStore` commit reports failure. Page dirty tracking is an extension
  point; active-profile feature pages are placeholders and no production editor
  marks a draft dirty. 7B editors must wire the hook before exposing edits. No
  full suite, Windows build, package, screenshot, or memory gate ran; the 7A
  screenshot comparison and memory measurement and the 7A/Phase 7 exit gates
  remain open.

### 7A.11 — Save As accepted (2026-10-27)

- Commit: `86b09d94` (`Phase7 - Add Save As for existing profiles`). File >
  Save As… uses the platform shortcut and Teacher Profile dialog with `.tps`
  default suffix, overwrite confirmation, current profile directory, and
  suggested filename. The shared path policy normalizes extensionless and
  `.db` target names to `.tps`; schema-v6 `.db` input was deferred to candidate
  #15.
- Save As creates a SQLite online-backup snapshot and atomically replaces its
  target. The active store switches only after copy succeeds; the session ID
  is preserved, the source remains, and recent history is recorded after
  success. It shares dirty-resolution and pending setup/recovery guards with
  7A.10. The online-backup implementation and file operation have one explicit
  owner in `CLASSMNGR_WORKSPACE_STORE_SOURCES`, without a Next-to-ClassMngrData
  link. A borrowed Qt settings adapter lifetime bug in the default dialog
  presenter was fixed.
- Verification: standard `build/macos-clang-debug`; CMake configure validated
  one explicit owner for 1,741 handwritten sources; focused CTest passed 7/7:
  Save As flow, bootstrap, DataService lifecycle, Next boundary, workspace
  creation/open/coordinator. `git diff --check` passed. Coverage includes
  persisted marker data, location/session preservation, close/reopen, retained
  source, alias no-op, cancel, pending setup guard, source/target preservation
  on copy failure, `.db` output normalization, and history success/failure.
- Limits: bootstrap coverage uses a dialog seam, not the production Qt modal.
  Forced destination-reopen failure and explicit Save As dirty-page or
  recovery-pending cases were not tested. Full suite, Windows, packaging,
  screenshot, and memory gates remain unrun; screenshot comparison and memory
  evidence and the 7A/Phase 7 exit gates remain open.

### 7A.12 — Export accepted (2026-10-27)

- Commit: `5badb2c8` (`Phase7 - Add Next Teacher Profile Export`). Next File >
  Export is a distinct action enabled for an open profile, with a separate
  `ExportTeacherProfile` save-dialog preference and normalized `.tps` target.
  It uses the shared `DatabaseFileOperations::copyDatabaseFile` online-backup
  and atomic-destination helper without reopening or switching the active
  store. Export preserves the source session/path, selection, dirty Page and
  workspace state, and recent-profile history; it does not resolve ordinary
  dirty state or include page-only drafts. Pending setup or any available or
  unreadable recovery blocks before the dialog. Cancel is a no-op, same-file
  aliases are safe, and copy failure preserves an existing destination.
- Verification: standard `build/macos-clang-debug`; focused builds of
  `ClassMngrNextWorkspaceExportFlowTests`, `ClassMngrNextBootstrapTests`, and
  `ClassMngrNextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests`
  passed. Focused CTest passed 4/4, including
  `NextStartupWorkspaceDependencyBoundary`; `git diff --check` passed.
- Limits: no full suite or native-dialog run. Invalid-path and dialog-open
  failure injection were not covered; destination-failure coverage checks
  existing destination bytes, not source-content byte equality. Screenshot
  comparison and memory evidence, plus the 7A and Phase 7 exit gates, remain
  open.

### 7A.13 — Recent Files menu, restore, and clear accepted (2026-10-27)

- Commits: `b79b98e9` (`Phase7 - Add Next Recent Files history`) and parity
  follow-up `4fcab8bf` (`Phase7 - Remember directory after recent restore`).
  The Next Recent Files submenu refreshes from persisted paths when opened and
  shows ordered, numbered entries with escaped paths and tooltip/action data,
  an empty disabled placeholder, and Clear. Selecting an entry uses the shared
  Next open flow without showing a file picker. Successful open remembers the
  normalized profile directory as the `TeacherProfile` preference.
- Clear removes recent-history and last-file values without deleting profiles,
  changing active state, or clearing that directory preference. Setup or
  recovery blocks before dirty prompts. Only confirmed missing paths are
  pruned; corrupt and unsupported profiles remain.
- Verification: built `ClassMngrNextWorkspaceOpenFlowTests`,
  `ClassMngrNextBootstrapTests`,
  `ClassMngrNextPlatformSettingsManagerRecentWorkspaceHistoryPortTests`, and
  `ClassMngrNextApplicationRecentWorkspaceHistoryUseCaseTests`. Focused CTest
  passed 5/5, including `NextStartupWorkspaceDependencyBoundary`. After the
  parity follow-up, selected offscreen restore tests passed, the final five
  acceptance CTests passed 5/5, Bootstrap CTest passed 1/1, and
  `git diff --check` passed. No acceptance gate is pending.
- Limits and remaining gates: no full suite or native-dialog run; injected
  directory-preference write-warning coverage is absent. Screenshot comparison
  and memory evidence, plus the 7A and Phase 7 exit gates, remain open.

### 7A.14 — Interrupted normal-create recovery accepted (2026-10-27)

- Commit: `0b5c1d6c`. The manual Next File-menu recovery action scans one
  active, remembered, or default directory nonrecursively for exact hidden
  `.<target>.workspace-create-UUID.recovery` regular files. It rejects
  lookalikes and symlinks and excludes Initial Setup backups. After the user
  selects a candidate, the flow resolves dirty state, copies it with
  no-replace semantics to a newly selected unused `.tps` destination, retains
  the source, and opens the copy through the shared Next open flow. Setup or
  recovery blocks before scanning or prompting. No normal-create journal or
  startup prompt was added.
- Copy/open failure preserves the source and recovery artifacts and existing
  active state/history; successful open updates history/navigation. Verification
  used standard `build/macos-clang-debug` and built `ClassMngrNext`, Bootstrap,
  Navigation, InterruptedRecoveryFlow, OpenFlow, and CreationFlow targets.
  Focused CTest passed 6/6, including the Next dependency boundary. Follow-up
  sort/order assertions and real File-menu-to-bootstrap cancel, failure, and
  success coverage passed Bootstrap, recovery, and boundary CTest 3/3.
  `git diff --check` passed. No acceptance gate remains.
- Limits: no full suite or native-dialog run; recovery artifacts were seeded
  rather than created by terminating a process mid-create. The Bootstrap
  failed-open test starts closed; the flow test separately verifies preservation
  of an existing active session. The 7A screenshot comparison, memory evidence,
  and 7A/Phase 7 exit gates remain open.

### 7A.15 — Schema-v6 `.db` input accepted (2026-10-27)

- Commit: `c9cdd421`. Next interactive Open, recent restore, and command-line
  startup accept case-insensitive `.db` inputs and preserve the absolute,
  canonical `.db` path. Schema-v6 opens in place without a sibling `.tps`, and
  ordinary Save writes to the active `.db`. Save As and Export still normalize
  `.db` destinations to `.tps`; the existing schema manager rejects v0–5 and
  future versions, with no migration added.
- Verification: standard `build/macos-clang-debug`; focused builds of OpenFlow,
  Bootstrap, SaveAsFlow, and ExportFlow passed. Five focused CTests passed for
  open, bootstrap, Save As, Export, and the dependency boundary. Changed-path
  coverage includes uppercase `.DB` interactive open and Save, recent restore,
  CLI startup/history, and v5/future rejection preserving session, selection,
  history, and files; existing Save As/Export tests cover `.db` destination
  normalization.
  `git diff --check` passed. No acceptance gate remains for this slice.
- Limits and remaining gates: no full suite or native-dialog run. The v6 profile
  is generated by tests; v5 rejection uses `user_version=5` rather than a
  populated historic schema, and no real-world legacy `.db` fixture is
  available. Screenshot comparison and memory evidence, plus the 7A and Phase 7
  exit gates, remain open.

### Candidate #16 — Teacher profile page and navigation accepted (2026-10-27)

- Commit: `24c14f5e` (`Phase7 - Add teacher profile page and interactions`).
  Next now has a static typed Teacher Profile route with open-by-ID navigation,
  direct `WorkspaceStore` reads and edits, validation, dirty/save handling,
  Korean/English on-screen keyboard input, and a calendar-backed birthday field
  that displays a localized month/day and stores `MM-dd`.
- Visual evidence: matched 1024×768 Fusion/offscreen captures cover English/light
  and Korean/dark. The page restores the legacy header, two-row details and
  connectivity cards, and Notes card. Remaining deltas accepted in review are
  roughly 2–6 px in card vertical positions, a few pixels at field edges, and
  idle visibility of the birthday clear affordance. See the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/teacher-profile/README.md).
- Focused verification in `build/macos-clang-debug`: `ClassMngr`,
  `ClassMngrNext`, `ClassMngrNextBootstrapTests`,
  `ClassMngrOnScreenKeyboardTests`, and `ClassMngrTeacherProfileVisualCapture`
  built; focused CTest passed 4/4. `NextStartupWorkspaceDependencyBoundary`
  passed 1/1; source ownership passed for 1,751 handwritten sources; resource
  inventory `--check` and `git diff --check` passed. Build freshness (fresh vs.
  incremental) was not stated in the verification handoff.
- Limits: these profile captures do not close the 7A screenshot comparison or
  memory evidence; the 7A and Phase 7 exit gates remain open.

### Candidate #17 — Korean teacher sidebar directory accepted (2026-10-27)

- Commit: `d6574628` (`Phase7 - Add Korean teacher sidebar directory`). The
  sidebar shows 24 Korean teachers under Campus Staff. Matched legacy/Next
  captures show 28 items and 12 widgets in that subtree. Both use Inter 14 pt,
  12 px indentation, and 30 px rows, aligned with legacy.
- Visual review: English/light and Korean/dark pairs use Fusion/offscreen at
  1024×768. Main reviewed all four captures; the only expected difference is
  localized “My Workspace” text. The [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/korean-teacher-directory/README.md)
  includes both manifests.
- Verification: focused CTest passed 13/13 and visual captures 2/2. Required
  targets built; `NextStartupWorkspaceDependencyBoundary`, ownership count
  (1,760), resource inventory `--check`, and `git diff --check` passed.
  Independent Tester verification covered #17; #16 was not independently
  re-verified.
- Memory: with both surfaces alive, whole-process snapshots were English/light:
  resident 74.11 MiB, peak 74.16 MiB, physical footprint 30.38 MiB, internal
  resident 29.75 MiB; Korean/dark: resident 79.38 MiB, peak 79.42 MiB,
  physical footprint 32.45 MiB, internal resident 31.83 MiB. These are not
  per-list deltas and do not
  establish a budget or close the 7A or Phase 9 memory gates. The 7A screenshot
  comparison and 7A/Phase 7 exit gates also remain open.

### Candidate #18 — Shared Staff Directory accepted (2026-10-27)

- Commit: `aab11f67` (`Phase7 - Add shared staff directory`). Native English
  and GS Team use a shared model/view directory with in-place refresh,
  active-cell editors, and separate typed `WorkspaceStore` branches. The
  `refreshFromStore()` seam is for candidate #19; normal teacher workbook
  import is not part of #18.
- Verification: incremental build and CMake reconfiguration in standard
  `build/macos-clang-debug`; CMake validated one owner for 1,769 handwritten
  sources. Focused CTest passed 19/19. The filtered offscreen Bootstrap route
  check passed 3/3 with missing-icon and unsupported-operation warnings.
  Resource inventory `--check` and `git diff --check` passed. The Executor
  added assertions for header order, direct-store row order, row/column counts,
  sorting state, zero table items, and zero cell widgets. Independent Tester
  rechecked the slice.
- Visual review: Main and Tester reviewed the four legacy/Next pairs across
  Native English and GS Team in English/light and Korean/dark. Content and
  layout align closely; the Next table bottom edge is about 6 px lower. See the
  [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/shared-staff-directory/README.md)
  for the fixture, assertions, memory evidence, and limits.
- Limits: snapshots are sequential whole-process macOS measurements, not
  per-page allocation deltas or a budget. No handle-count/private-dirty metric,
  full suite, Windows run, native-window verification, or phase exit gate was
  verified. The 7A screenshot comparison and memory evidence and the 7A/Phase 7
  exit gates remain open. Candidate #19, normal teacher workbook import in
  Next separate from Initial Setup, is active.

### Candidate #19 — Normal teacher workbook import accepted (2026-10-27)

- Commit: `6dedd431` (`Phase7 - Add normal teacher workbook import`). The Next
  normal-workspace action is separate from Initial Setup. It opens an Excel
  picker, validates asynchronously, builds a typed review request with Korean
  M1/M2/H1 choices, and imports Native English and GS counts automatically.
  Before apply it reads the latest imported source date and asks for
  confirmation when the selected date is same or older (Cancel is default).
  Successful apply uses the `WorkspaceStoreGateway`, refreshes the Korean list
  and loaded staff pages, and reports import counts; a refresh failure leaves
  the committed import intact and requires the affected directory to reload.
- Fixture and visuals: `sectioned_review_large.xlsx` has source date
  `2026-09-01` (current version `2026-10-01`), 122 Korean candidates with 120
  checked, plus 40 automatic Native English and 40 GS Team rows in the review.
  English/light and Korean/dark legacy/Next selection, review, and version
  prompt captures verify nonblank dates in both locales. Accepted text and
  visible-row deltas are documented in the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/normal-teacher-import/README.md).
- Verification: the independent Tester used the standard
  `build/macos-clang-debug` build and passed the focused Next CTest suite 16/16;
  earlier direct Bootstrap cases passed. Resource inventory and
  `git diff --check` passed. The Tester recommended acceptance.
- Memory and limits: manifest snapshots show whole-process resident memory near
  35 MiB before parse, 37 MiB after parse, and 75–76 MiB after review/apply;
  recorded peaks were 75.73 MiB English/light and 75.56 MiB Korean/dark. They
  include Qt, SQLite, parser, and fixture state, so they are not per-import
  allocation deltas or a budget. No full suite, Windows run, or native-dialog
  verification was recorded. The 7A screenshot comparison and memory evidence
  and 7A/Phase 7 exit gates remain open. Candidate #20, upcoming birthdays and
  daily dismissal in Next, is active.

### Candidate #20 — Upcoming birthdays and daily dismissal accepted (2026-10-27)

- Commit: `a36a1fb5`. The Next dialog matches the legacy 520×560 layout and
  content in English/light and Korean/dark. Main reviewed all four captures;
  the independent Tester confirmed content, layout, and empty states. Capture
  manifests include populated birthday sections and empty-schedule assertions.
  See the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/upcoming-birthdays/README.md).
- Dismissal behavior: checking “Don't show again today” writes today's local
  ISO date on close. Legacy does not automatically trigger the dialog or read
  that setting; Next adds neither behavior.
- Verification: standard incremental `build/macos-clang-debug`; CMake
  ownership check passed for 1,778 handwritten sources; focused CTest passed
  7/7. The Next dependency boundary, resource inventory `--check`, and
  `git diff --check` passed.
- Limits: no dirty-editor-page scenario or separate GS-only failure test was
  run. Whole-process macOS snapshots showed working set about 44.30→66.19 MiB
  and private usage 11.99→18.99 MiB in English/light; Korean/dark was
  44.45→69.83 MiB and 12.14→21.45 MiB. Memory remains after dialog cleanup;
  these are not feature-only deltas or a budget. No full suite, Windows, or
  phase exit gate was verified. The 7A screenshot comparison and memory
  evidence and 7A/Phase 7 exit gates remain open. Candidate #21, My Details and
  Signature, is active.

### Candidate #21 — My Details and Signature accepted (2026-10-27)

- Commit: `060aca5c`. The normal-workspace page supports My Details and saved
  Signature modes (Image and Type), separate from Initial Setup. Normal-workspace
  empty-name behavior matches legacy; Initial Setup validation is unchanged.
  Image and typed previews render, and saved signatures remain compatible
  through roundtrip and removal. Dirty navigation, automatic/manual save,
  full-bundle save, failure/retry, report readback, and workspace replacement
  are covered.
- Visual review: Main and Tester reviewed eight matched 1024×768
  Fusion/offscreen captures across English/light and Korean/dark in both
  signature modes. Next has no persistent footer or page Save button in normal
  states. Accepted deltas are controls becoming visible about 10–20 px earlier
  and additional Korean shell/tab localization. See the [capture
  package](../../docs/qt-rewrite/visual-baseline/next/phase-7/my-details-signature/README.md).
- Verification: after the footer repair, focused CTest passed 5/5, page tests
  14/14, and two targeted Bootstrap scenarios 4/4. Source ownership validated
  1,782 handwritten sources; resource inventory and `git diff --check` passed.
  Earlier focused functional recheck passed CTest 8/8, page tests 13/13, and
  selected Bootstrap cases 7/7, including full-bundle save, failures/retry,
  dirty navigation, replacement, and signature roundtrips.
- Memory and limits: whole-process macOS snapshots across captures span
  70.9–109.8 MB working set (67.6–104.7 MiB) and reach 46.6 MB private usage
  (44.5 MiB), including Qt, legacy runtime, resources, and both surfaces. They
  are not page-specific allocations or a budget. No EXIF-orientation fixture,
  exact debounce timing assertion, full suite, or Windows run was verified.
  The 7A screenshot comparison and memory evidence and 7A/Phase 7 exit gates
  remain open. Candidate #22, My Classes, is active.

### Candidate #22 — My Classes accepted (2026-10-27)

- Commit: `f7a3d81c`. The page uses one compact summary model and one selected
  class detail panel. On the tracked 96-class fixture, the Next capture reports
  96 summary rows, one list view/model, one selected detail panel, and one
  requested class in each class, class-info, and roster detail read; suspension
  releases details and clears the summary rows. The manifest records 69 active
  Next page widgets
  and 321 legacy page widgets, including 96 legacy class widget shells.
- Visual review: Main and Tester accepted the matched 1024×768 Fusion/offscreen
  captures in English/Korean and light/dark. Grade/class navigation and the
  full-width teacher-first stacked details match the legacy composition.
  Visible differences are Next's bold/plain field presentation versus legacy
  outlined read-only controls, tighter selector spacing, and more schedule text
  visible in Next because the legacy one-line control clips it. Korean Next
  localizes the room label to “교실.” See the
  [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/my-classes/README.md).
- Verification: build command was
  `cmake --build build/macos-clang-debug --target ClassMngrNextMyClassesPageTests ClassMngrNextMyClassesVisualCapture ClassMngrNextBootstrapTests --parallel 8`.
  Five focused CTests passed 5/5: `ClassMngrMyClassesPageTests`,
  `ClassMngrMyClassesPageTeacherDisplayParityTests`,
  `ClassMngrMyClassesPageSummaryParityTests`,
  `ClassMngrNextMyClassesPageTests`, and
  `ClassMngrNextMyClassesVisualCapture`. The targeted real-store A-to-B
  replacement passed in `ClassMngrNextBootstrapTests`, slot
  `myClassesReloadsRealWorkspaceDataWhenProfileIsReplacedWhileActive`;
  `NextStartupWorkspaceDependencyBoundary` passed 1/1. Resource inventory
  `--check` and `git diff --check` passed.
- Limits: the full `ClassMngrNextBootstrapTests` run had 66 passes and one
  failure at the status-message assertion in
  `recentRestoreRemembersDirectoryWhenHistorySaveFails`. It uses a recording
  gateway; this candidate does not change that path, and its baseline is
  unverified. English/light private-working-set samples were 9.56 → 27.69 →
  31.05 MiB for legacy and 31.14 → 33.67 → 33.75 MiB for Next across sequential
  before/active/suspended captures. These are process-wide samples, not page
  allocations or a memory budget. The 7A screenshot comparison and memory
  remediation and the 7A/Phase 7 exit gates remain open. Candidate #23, My
  Schedule and workspace event summary, is active.

### Candidate #23 — My Schedule and workspace event summary accepted (2026-10-27)

- Result: Next My Schedule is a read-only regular-schedule table; Calendar is a
  read-only Next 10 upcoming-event table with date, time, title, and type. My
  Schedule remains initially selected. Details, Schedule, and Events load only
  on their tabs. Leaving a tab, suspending the page, or replacing/closing the
  workspace clears the projections; re-entry reloads current data. Candidate
  #23 does not add birthday or daily-dismissal behavior from #20.
- Visual evidence: 13 captures from the standard
  `build/macos-clang-debug/my-workspace-visual-capture/` output use Fusion at
  1024×768: four legacy schedule surfaces, four Next schedule surfaces, four
  Next upcoming-event surfaces across English/Korean and light/dark, plus one
  96-class Next schedule. Legacy schedule is a weekly grid with Regular,
  Intensive, Testing, and Import controls; Next shows a read-only table. The
  Korean Next schedule displays “월요일” while the canonical store value stays
  `Monday`. The legacy Calendar surface was not captured because constructing
  its Upcoming Events panel constructs the deferred `QQuickWidget` month grid;
  the Next upcoming-event capture is surface-only, so Calendar parity remains
  open. See the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/my-schedule-workspace-events/README.md).
- Model/lifecycle evidence: Next owns two table model/views and 125 widgets.
  Captures show six representative regular-schedule rows, 10 upcoming events,
  and 96 rows in the scale scenario. After suspension, schedule and event rows
  are zero; Personal Details read count is zero in schedule/event captures.
- Verification: the independent Tester built with
  `cmake --build build/macos-clang-debug --target ClassMngrNextMyWorkspacePageTests ClassMngrNextMyWorkspaceVisualCapture ClassMngrNext -j 6`.
  The CTest selection
  `ClassMngrNextMyWorkspacePageTests|ClassMngrNextMyWorkspaceVisualCapture|ClassMngrMyWorkspacePageTests|NextStartupWorkspaceDependencyBoundary|ClassMngrCalendarEventRepositoryTests`
  passed 5/5. Resource inventory `--check` and `git diff --check` passed.
- Limits: manifest memory readings are process-wide snapshots accumulated in
  one process, not per-page allocations or a budget. Full schedule editing,
  intensive/testing schedules, import, and output remain for #30–32; Calendar
  editing, filters, preferences, month grid, and full visual parity remain for
  #33–34. No full suite, Windows run, or final Phase 7 exit gate was verified.
  The 7A screenshot comparison and memory evidence and 7A/Phase 7 exit gates
  remain open.

### Candidate #24 — Classes list/navigation accepted (2026-10-28)

- Commit: `368fcee8`. Next Classes now has a page-scoped model/view navigator
  with always-visible grade groups and All, legacy ordering and labels,
  regular/intensive schedule source, weekday/weekend filtering, and All Classes
  / Active Schedule visibility. Selection uses typed ClassId values and is
  remembered independently for each grade group and All. Reads go through a
  path-scoped WorkspaceStore action and bounded class and class-info repository
  APIs. Reads cap classes at 4,096, schedule rows at 14 per source/class, and
  navigation text/tokens; over-limit results fail instead of truncating.
  Testing classes are excluded. My Classes remains a separate route.
- Lifecycle and scope: replacing or closing the active workspace clears IDs
  and rows; replacement and re-entry load current data, while close/re-entry
  shows the no-workspace state. No student counts, class details/editors,
  create/delete, co-teachers, notes, analytics, roster, evaluations, testing
  workflows, or section deep links are included; later candidates own them.
- Capture package: eight synthetic Next-only page captures cover English/Korean,
  light/dark, and 6/96 classes. The manifest records one page, model, and view;
  41 page widgets; zero per-class widgets or detail editors; and cleared model
  rows on suspension. Whole-process observations span 57,622,528–65,159,168
  working-set bytes and 20,087,648–24,970,104 private-usage bytes. They are not
  page allocations or a budget. Callback counts are injected action counts, not
  SQL query counts. The captures use synthetic data without the application
  shell and do not match the linked legacy reference states; full Classes and
  Phase 7 visual parity remain open. See the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/classes-navigation/README.md)
  and the legacy [English/light](../../docs/qt-rewrite/visual-baseline/release/large-classes-visual-states/english-light/classes-entry.png),
  [English/dark](../../docs/qt-rewrite/visual-baseline/release/large-classes-visual-states/english-dark/classes-entry.png),
  [Korean/light](../../docs/qt-rewrite/visual-baseline/release/large-classes-visual-states/korean-light/classes-entry.png),
  and [Korean/dark](../../docs/qt-rewrite/visual-baseline/release/large-classes-visual-states/korean-dark/classes-entry.png)
  references.
- Verification: independent repaired-tree incremental build passed for
  `ClassMngrData`, `ClassMngrNext`, bootstrap, navigator page/Store/capture,
  My Classes, PageHost, ordering, repository, route, translation, and resource
  targets. The focused CTest selection passed 15/15 in 20.20 seconds; both
  active-workspace lifecycle selectors also passed directly with
  `QT_QPA_PLATFORM=offscreen` (four QtTest results including setup/cleanup).
  Resource inventory passed with 187 direct resources and 7 generated runtime
  inputs; `git diff --check` passed.
- Limits: before repair, the broad legacy
  `ClassMngrClassesPageTests::navigationReleaseCachesSnapshotAndEditorsAcrossReentry`
  timed out after 300,010 ms with 19 passes; no cause or baseline was
  established, and it was not repeated. The optional adjacent
  `ClassMngrNextPlatformApplicationServicesClassesListReadPortTests` target
  failed to link on unresolved `FeatureService`, `ClassService::classes`, and
  `DocumentCatalog` symbols; it did not run and was not rerun. The focused suite
  uses the standalone Next WorkspaceStore path. No full suite or Windows result
  is claimed. The 7A screenshot/memory evidence and 7A/Phase 7 exit gates remain
  open.

### Candidate #25 — Class details, create/edit, and co-teachers accepted (2026-10-28)

- Commit: `c0cb9c93`. Independent acceptance confirmed the Details/Times and
  Co-Teacher surfaces, with create/edit behavior and lifecycle coverage.
- Verification: affected `ClassMngrNext`, Bootstrap, PageHost, Classes
  navigator UI/WorkspaceStore, and related application-contract targets built.
  The corrected focused CTest selection passed 21/21. The Manual save test
  passed three QtTest results including setup/cleanup and confirmed manual and
  automatic save behavior. The route boundary scan,
  `NextStartupWorkspaceDependencyBoundary`, resource inventory `--check` (194
  runtime records), and `git diff --check` passed.
- Evidence and limits: the [paired capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/class-details-coteacher/README.md)
  records the 96-class fixture, English/Korean × light/dark matrix, page and
  lifecycle counts, and sequential whole-process memory scope. Legacy and
  Next have substantially different layout/shell scope, so these captures do
  not establish full visual parity. The `Level 02` value shown only by Next
  reflects a fixture/catalog limitation for the selected E4 class. The 7A
  screenshot comparison and memory evidence and the 7A/Phase 7 exit gates
  remain open.

### Candidate #26 — Class notes and time filler accepted (2026-10-28)

- Commit: `833e9074`. Independent acceptance covered Notes/Time Filler read,
  save, class/workspace lifecycle, and preservation of related class data.
- Verification: affected targets built and the corrected focused CTest
  selection passed 13/13. The real `.tps`
  `classNotesPathActionsBoundTextAndPreserveDetailsTransactionally` test passed
  three QtTest results including setup/cleanup. It covers both fields' exact
  length boundaries, persisted over-limit read failure, supplementary
  UTF-16-unit boundaries, trim semantics, transactional preservation, trigger
  rollback/retry, and Details saves preserving Notes. The dependency boundary,
  `git diff --check`, resource inventory `--check` (194 runtime records), and
  bounded source scan passed.
- Evidence and limits: the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/class-notes/README.md)
  documents the paired 96-class matrix, 600 Legacy and 230 Next widgets,
  lifecycle counts, process-wide memory observations, and Legacy Save geometry.
  The hidden Legacy Save button is below the 768px crop in both locales;
  Legacy and Next layouts differ, so no visual-parity result is claimed. The
  7A screenshot/memory evidence and 7A/Phase 7 exit gates remain open. No full
  suite, Windows, or memory-budget result is claimed.

### Candidate #27 — Class Analytics and speaking-evaluation defaults accepted (2026-10-28)

- Commit: `8f38ae44`. Next adds the Class Analytics dashboard through the
  path-scoped `WorkspaceStore` and typed
  `ClassAnalyticsDashboardReadQuery`; the shared
  `classes_navigation_evaluation_default_policy` semantics remain intact.
  Legacy Speaking Evaluation already consumes that preference and stays in
  candidate #38 scope.
- Term mapping is explicit and the 2200 cutoff is removed. A local parser is
  retained because no shared Next parser exists and a Calendar dependency would
  cross module boundaries. This duplicates Calendar interpretation and
  requires parity coverage; future Next consumers should extract a shared
  pure parser.
- Verification: affected targets built; focused CTest passed 28/28 and the
  WorkspaceStore Qt suite passed 23/23. Legacy-comparison tests cover custom
  term boundaries, elementary/middle profiles, dates through 2201, missing
  profiles, and invalid week counts/weekdays. Existing path tests cover
  populated/empty defaults, previous-term fallback, Spring-to-Speech Contest,
  explicit retention, and malformed JSON. `git diff --check`, resource
  inventory `--check`, and source/dependency boundary scans passed.
- Evidence and limits: the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/class-analytics/README.md)
  documents the All/Winter matrix, content, lifecycle, counts, and whole-process
  memory scope. Legacy and Next shells differ, so the captures do not establish
  visual parity; memory snapshots do not establish a budget. The 7A evidence
  and 7A/Phase 7 exit gates remain open. No full-suite or Windows result is
  claimed.

### Candidate #28 — Testing classes and blocks accepted (2026-10-28)

- Commit: `9bdbe3aa`. The accepted manager boundary is the Next testing-class
  list/model, one selected Details/Notes editor, create/update/delete, full
  cascade cleanup, and typed pending-slot create. It does not include Roster
  UI (#35) or the weekly block/assignment editor (#30/#32).
- Verification after the translation-context fix: affected Next manager tests
  passed 12/12; the visual capture target passed with all Korean assertions.
  Separate Legacy persistence, delete, cancel, and schedule-handoff targets
  passed. `NextStartupWorkspaceDependencyBoundary`, `git diff --check`,
  resource inventory `--check`, and the Next route scan passed. The scanned
  Next UI/App/Platform path contains no `ApplicationServices`, `DataService`,
  SQL, Gateway, or direct database reference.
- A wider 34-registration focused selection before the translation-only fix
  passed 33/34. Its sole failure was the unchanged Legacy aggregate
  `ClassMngrTestingClassesPageTests`, independently reproduced at 48 passed
  and 4 failed: `rosterEditorOmitsRemoveButtonAndKeepsContextAction`,
  `rosterKeyboardTriggerOpensInAppPalette`,
  `rosterKeyboardWritesToKoreanNameCell`, and
  `outputAvailabilityFollowsRosterTabAndLoadedClass`. The failures reported
  `QSqlQuery::prepare: database not open`. Tester found these cases instantiate
  `ApplicationServices`/`DataService` without opening a database. The Legacy
  page, tests, and CMake target were unchanged by #28; the report does not
  establish when these failures first appeared.
- Evidence and limits: the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/testing-classes/README.md)
  records the 96-testing-class fixture, Details/Notes/pending-slot matrix,
  Korean localization, one-editor lifecycle, and process-wide memory scope.
  Legacy holds the pending weekday/start in page state without a visible label;
  Next displays the pending tuple. Their captures are not a parity claim. No
  full-suite, Windows, or memory-budget result is claimed. The 7A evidence and
  7A/Phase 7 exit gates remain open.
- Capture metadata follow-up: `c2ec78e9` after source commit `9bdbe3aa` records
  the typed Thursday 10:00 tuple and localized display text in pending Next
  states; non-pending Next states have null slot fields. The capture target
  passed 1/1 after this metadata-only correction.

### Candidate #29 — Class Transfer accepted (2026-10-28)

- Commit: `56455a1b`. Next uses path-scoped `WorkspaceStore` actions and typed
  JSON v1 contracts for teacher/class transfer data (Details, Notes/Time Filler,
  schedules, roster, and evaluations); destination IDs are remapped. No
  `ClassTransferProjection` caps were reused.
- The new proposed file-size contract is inclusive at 16 MiB: exactly
  16,777,216 bytes is accepted; one byte over is rejected before parse; an
  oversized export cancels atomically without replacing an existing file.
  This is a new product contract, not an existing Phase 7 numeric requirement.
  The historical 1,716,291-byte package passed. A 4,097-class package of
  2,127,895 bytes parsed, but applying all 4,097 classes was not tested.
- Verification: the standard build passed for `ClassMngrNext`,
  `ClassMngrNextClassTransferWorkflowTests`, and
  `ClassMngrNextClassTransferVisualCapture`. Focused CTest passed 20/21
  registrations; direct Next workflow QtTest passed 9/9. The one Legacy
  failure is `ClassTransferTests::importDialogFallsBackToIndividualTeacherProfilesAfterBatchFailure`
  (`Keep local: New Teacher` vs `Keep local: Romanized One`; offscreen aggregate
  420 passed/1 failed). Its exact evidence and limitation are in the [capture
  package](../../docs/qt-rewrite/visual-baseline/next/phase-7/class-transfer/README.md).
  The Legacy test setup/expectation and `class_import_dialog.cpp` were unchanged;
  it uses an in-memory Legacy package, not the new WorkspaceStore route/codec.
  No evidence establishes when the failure first appeared.
- Evidence and limits: the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/class-transfer/README.md)
  records export/review/conflict states, `parityClaim: false`, and the process
  memory scope. Legacy standalone dialogs and Next `PageHost` have different
  shell extents; process snapshots do not establish a page allocation or
  Windows memory budget. `git diff --check`, inventory `--check`, and route
  scan passed. No full-suite or Windows result is claimed; the 7A evidence and
  7A/Phase 7 exit gates remain open.

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
