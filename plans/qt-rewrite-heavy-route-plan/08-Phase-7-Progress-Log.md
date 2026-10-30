# Phase 7 Progress Log

Historical progress entries moved from [08-Phase-7-Feature-Migration.md](08-Phase-7-Feature-Migration.md).
Entries are preserved in their original document order.

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

### Candidate #30 — Schedule display/editing accepted (2026-10-28)

- Commit: `894a11ec`. Next adds standalone weekly Schedule display/editing
  through path-scoped `WorkspaceStore` actions and typed app actions, using one
  `QTableView`, model, and delegate with compact resident state. Regular,
  Intensive, and Testing modes, display preferences, and empty-slot state are
  persisted. Class navigation reuses Class Details and returns a refreshed
  schedule. Failed preference, mode, or slot saves remain visible and restore
  the prior state. Candidate #31 import/review/apply and #32 testing-assignment
  mutation/output/printing remain deferred.
- Workspace isolation was verified with two `.tps` files sharing IDs and an
  identical slot key; mode, preferences, and slot state remained separate.
  Replacement, suspension, re-entry, and close clear or reload projections.
  The Next path has no Gateway, `DataService`, Legacy widget, or
  `ApplicationServices` Schedule-port dependency.
- Verification: incremental macOS build in `build/macos-clang-debug` passed
  for the application, Bootstrap, new WorkspaceStore action/page/capture
  targets, and relevant schedule/Class Details regressions. Focused CTest
  passed 25/25. Resource inventory `--check` and `git diff --check` passed.
  No full suite or Windows build is claimed; `build/windows-x64-debug` was
  unavailable.
- Evidence and limits: the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/schedule-display-editing/README.md)
  contains 32 PNGs (16 Legacy/Next pairs) at 1024×768, covering all modes in
  English/Korean × light/dark plus 96-class Regular/Intensive and wrapped-label
  Intensive states. Measured toolbar/grid geometry, row heights, rows, cells,
  and entries match across pairs. Legacy and Next each report 549 Testing
  entries; the focused pair shows the canonical Monday 17:00 assignment. Six
  Next Intensive captures keep all 15/15 grade and level lines visible;
  256-character teacher/room values are elided in its 60 px rows while
  remaining in accessible text and tooltips. Legacy wraps or clips those
  labels. `pixelParityClaim:false` remains accurate due toolbar actions
  (Preferences versus Legacy Import/Testing Classes), overflow-scrollbar, glyph,
  and compact Intensive typography differences.
  The large fixture reports Next Regular at 7 rows/49 cells/768 entries and
  Intensive at 13 rows/91 cells/15 entries, with one table view and no
  per-entry widgets. Suspension releases retained rows, cells, entries, and
  source classes to zero. Active whole-process macOS snapshots span about
  107–135 MB working set and 54–75 MB private usage; they do not measure page
  allocation or establish a Windows memory budget. The 7A screenshot/memory
  evidence and the 7A/Phase 7 exit gates remain open.

### Candidate #31 — Schedule workbook import/review/apply accepted (2026-10-28)

- Commit: `c1166d79`. Normal workspace Schedule Import owns source, worksheet,
  and block selection; asynchronous seekable-`QIODevice` parsing; model-backed
  review and diagnostic acknowledgement; explicit teacher/class decisions;
  Normal and Intensive modes; typed path-scoped `WorkspaceStore` snapshot and
  transactional apply; rollback/retry; Schedule refresh; and lifecycle cleanup.
  It does not use Setup Import recovery, finish, or profile-name semantics.
  Both profile-update flags remain false; recent history, setup journal, and
  profile information are preserved.
- Two real `.tps` files with overlapping IDs verified exact-path isolation.
  Coverage includes teacher revalidation, stale workspace/source, active parse
  cancellation/timeout and late results, second worksheet/block, unresolved
  diagnostic acknowledgement, Normal/Intensive preserve/replace/counts,
  successful apply, rollback/retry, Schedule refresh, suspension/replacement
  cleanup, and the setup boundary. A separate
  `ClassMngrNextNormalScheduleImportIntegrationTests` target covers this flow;
  setup-only `ClassMngrNextScheduleImportWorkflowTests` remains a regression.
- Verification: macOS Debug build in `build/macos-clang-debug`
  passed; focused CTest passed 23/23, and Bootstrap passed 1/1. The build also
  covered the repaired `ClassMngrNextScheduleVisualCapture` target. Resource
  inventory `--check` (194 runtime resources), `git diff --check`, and the path
  boundary scan passed. No full suite or Windows result is claimed;
  `build/windows-x64-debug` was unavailable.
- Evidence and limits: the [capture package](../../docs/qt-rewrite/visual-baseline/release/normal-schedule-import-next/README.md)
  contains 40 Next scenes, 16 Legacy references and their 16 aliases across
  English/Korean × light/dark. Dialog sizes match at 436×520 for source and
  selection and 800×800 for review and conflict, but there is no visual-parity
  claim. Next requires horizontal scrolling to see full meeting times in its
  preview and shows generic overlap status; Legacy names the conflicting pair
  and some Korean fallback strings remain English. The stress workbook has
  96 candidates across five blocks; the selected conflict block contains 20
  rows with six preview rows visible. No 96-row apply was tested. A separate
  successful 20-candidate real-store apply persisted 20 classes and 30 schedule
  rows. Whole-process memory samples do not establish a parser or process
  memory budget. The 7A screenshot/memory evidence and 7A/Phase 7 exit gates
  remain open.

### Candidate #32 — Testing assignments and Schedule output/printing accepted (2026-10-28)

- Commit: `51faa658`. Testing assignments use direct path-scoped
  `WorkspaceStore` actions; Cancel and the manager handoff do not write.
  Update/replace/remove, transaction failure/retry, stale path, successful
  reload, and visible error behavior are covered. Schedule output uses a
  shared value-only print document and renderer, with resolved fonts supplied
  at the Legacy and Next boundaries. A4, 300 DPI, half-inch content margin,
  styles/themes/Excel, orientation, and existing print controls remain intact.
- Architecture: `ClassMngrQtPdfPrint`, `ClassMngrSchedulePrint`, and
  `ClassMngrNextScheduleOutput` reuse Qt PDF/print preview. The Next allowlist
  adds only the narrow output target and excludes `ClassMngrFeatures`,
  `ClassMngrUiShared`, and `ClassMngrCore`. This keeps the slice's output
  boundary explicit; Phase 8 still owns platform and external printer/PDF
  adapter isolation. Qt PDF and printer objects remain operation-scoped.
- Verification: a fresh `build/macos-clang-debug` configure/build passed and
  validated one explicit owner for 1,867 handwritten source files. The
  original focused CTest batch passed 16/16 and the repair delta 5/5;
  `ClassMngrSchedulePrintPdfTests` passed 10/10, and the Schedule Import
  integration target passed. Source ownership/dependency boundary,
  resource inventory `--check`, and `git diff --check` passed. The PDF failure
  test returns Failed and creates no file for a missing parent directory.
- Evidence and limits: the [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/schedule-testing-output/README.md)
  and manifest reference 21 existing images across English/Korean and
  light/dark, with no clipping; they document states and do not claim pixel
  parity. PDF content checks normalize whitespace while preserving ordered
  content and punctuation. Print-dialog cancellation was tested with faked
  UI results, not physical hardware. No full suite, Windows, physical-printer,
  or memory-budget result is claimed. The 7A screenshot/memory evidence and
  7A/Phase 7 exit gates remain open.

### Candidate #33 — Calendar events, preferences, and month grid accepted (2026-10-28)

- Commit: `21f2d98c` (`Phase7 - Migrate calendar events and month grid`). The
  lazy Next Calendar route uses typed calendar actions and the
  WorkspaceStore path bridge. `ClassMngrNextCalendarUi` is the narrow target;
  the strict `NextStartupWorkspaceDependencyBoundary` passes without adding
  broad Legacy Features, UI Shared, or Core targets. Workbook import remains
  candidate #34.
- Behavior: Upcoming event-type filters no longer filter the month grid, which
  remains complete like Legacy. Persisted event colors appear as Upcoming row
  swatches. The scope dropdown and compact colored controls remain Next
  presentation differences. Repeat occurrence/suffix edit and delete,
  cancel/no-write, failure/retry, preferences, and reset are covered. The
  [paired capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/calendar/README.md)
  records its fixture, selector commands, and visual limits.
- Verification: incremental `build/macos-clang-debug` configure/build reran
  and source ownership validated one owner for 1,879 handwritten sources.
  Final direct `ClassMngrNextCalendarPageTests -v1` passed 20/20; focused CTest
  passed 3/3 (`NextStartupWorkspaceDependencyBoundary`,
  `ClassMngrNextCalendarPageTests`,
  `ClassMngrNextCalendarWorkspaceStoreActionsTests`). Bridge coverage includes
  event save/readback, repeat-series operations, display/first-day preferences,
  academic-schedule JSON round-trip, and missing-path failures. Resource
  inventory `--check` and `git diff --check` passed. Gateway edit hashes match
  their pre-candidate values.
- Evidence and limits: 12 captures cover English/Korean × Light/Dark at
  1280×920. Legacy dark grid cells remain light; Next follows the dark palette.
  Legacy uses larger badges and separate grid-top/upcoming scroll captures
  because its grid has an 840 px minimum; Next fits both in one view. No
  clipping was observed. Manifest counts record 35 visible days, one retained
  range, three cached events, Upcoming limit 10, and zero QML roots/views after
  leave. Whole-process memory values are observational only, not a page budget
  or Phase 9 acceptance. No clean build, full suite, Windows build, or Phase 9
  memory budget ran. No production changes after the commit are pending.

### Candidate #34 — Calendar import accepted (2026-10-28)

- Commit: `8645d9db` (`Phase7 - Add Calendar import to Next`). Next reuses the
  row-streaming parser through the narrow shared
  `ClassMngrCalendarImportParser` target and applies the typed import use case
  through a path-scoped `WorkspaceStore` bridge. The existing remote URL and
  environment override, direct apply counts, and narrow Next dependency
  boundary remain. There is no local-file picker, per-row review, or Cancel
  button; page suspension, service destruction, and path changes cancel with
  cleanup and no writes.
- Verification: incremental `build/macos-clang-debug` configure/build validated
  one owner for 1,883 handwritten sources. Focused CTest passed 6/6:
  `ClassMngrCalendarImportTests`,
  `ClassMngrNextApplicationCalendarEventImportUseCaseTests`,
  `ClassMngrNextCalendarPageTests`,
  `ClassMngrNextCalendarWorkspaceStoreActionsTests`,
  `ClassMngrNextPlatformCalendarEventImportCampusCodeQueryTests`, and
  `NextStartupWorkspaceDependencyBoundary`; the page suite passed 26 cases.
  The Legacy golden parser selector passed. Legacy loopback parity cases were
  not run because `QTcpServer::listen(LocalHost, 0)` is unavailable in this
  sandbox. Next integration uses injected in-process transport and covers
  duplicate counts (3 imported/2 skipped; repeat 0/5), malformed/transport/
  staging failures, cancellation and cleanup on suspension/path change and
  destruction, query failure, atomic rollback, refresh behavior, and the URL
  environment override. Resource inventory `--check` and `git diff --check`
  passed.
- Memory and visual evidence: the deterministic 246,017-byte XLSX arrives in
  four chunks (maximum 65,536 bytes). Tests assert file-backed parsing, zero
  materialized worksheet cells, staging removal before signature query/apply,
  and zero retained parser events at apply. The four-event fixture verifies
  transfer and representation release, not a high-event-count memory ceiling.
  The [import capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/calendar/import/README.md)
  contains 20 Preferences/import-state images for English/Korean × Light/Dark
  and Preferences/Loading/Success/Error/Canceled states. No clipping was
  observed; success screenshots show 4 imported/1 skipped while integration
  verifies 3/2 with a pre-existing duplicate. Korean prefixes are translated;
  detailed parser errors remain English as in Legacy. Captures are presentation
  evidence, not pixel-parity evidence. Whole-process metrics are observational,
  not a per-import budget or Phase 9 acceptance.
- No full suite, Windows build, or Phase 9 memory-budget result is claimed. The
  7A screenshot/memory evidence and the 7A/Phase 7 exit gates remain open.

### Candidate #35 — Roster editor/student model accepted (2026-10-28)

- Commit: `9da01020` (`Phase7 - Migrate roster editor and model view`). The
  shared Next roster editor uses a table model/view and a 25-row resident
  window; its capture manifest records six base plus 150 custom columns.
  WorkspaceStore roster actions serve both regular and testing classes.
  Implementation and independent verification were accepted.
- Evidence: the [roster-editor
  package](../../docs/qt-rewrite/visual-baseline/next/phase-7/roster-editor/README.md)
  contains four standalone Next captures across English/Korean and light/dark,
  plus four Legacy/Next Testing Classes roster pairs from the 96-class fixture.
  The paired captures use class ID 3; Next records 25 modeled rows, nine
  dynamic columns, and roster-editor clearing on suspension. These screenshots
  are presentation evidence and do not claim pixel parity.
- Limits: no full-suite, Windows, cross-platform, or Phase 9 memory-budget
  result is claimed. The 7A screenshot comparison and memory evidence, Windows
  packaged-Next memory gate, and 7A/Phase 7 exit gates remain open.

### Candidate #36 — Roster score import and student transfer accepted (2026-10-28)

- Commit: `b0df9112` (`Phase7 - Add roster import and student transfer`). The
  Next roster editor imports saved speaking-evaluation scores and transfers an
  individual student to a same-grade class. Full targets are disabled and a
  positive confirmation precedes the paired source/target save. Parent checks
  verify both class parents in the atomic save; changes made to either roster
  after it was read are not detected optimistically.
- Verification: independent Tester passed the focused CTest selection 6/6;
  `ClassMngrNextRosterEditorTests` reported 22 passed, 0 failed. Assertions
  cover same-grade target filtering, disabled full targets, database-path
  forwarding, and positive confirmation while retaining existing assertions.
  Direct Legacy-label comparison and path-scoped tests passed 4/4.
  `NextStartupWorkspaceDependencyBoundary`, resource inventory `--check`,
  CMake source ownership (1,891), and `git diff --check` passed. The inventory
  contains 187 direct resources and 194 catalog records.
- Captures: the [roster import/transfer
  package](../../docs/qt-rewrite/visual-baseline/next/phase-7/roster-import-transfer/README.md)
  records imported scores and student transferred for English/Korean ×
  light/dark at 1024×768. These are presentation evidence, not pixel parity.
- Limits: no full suite, Windows, cross-platform, or Phase 9 memory result is
  claimed. The 7A screenshot comparison and memory evidence, Windows
  packaged-Next memory gate, and 7A/Phase 7 exit gates remain open.

### Candidate #37 — Roster templates/printing accepted (2026-10-28)

- Commits: source `098f1de7` (`Phase7 - Add Next roster templates and
  printing`) and test/inventory follow-up `1214b569`
  (`Phase7 - Guard roster preview captures against blank pages`, 2026-10-29).
  Next supports By Day, Daily, and Per Class with Extra Info roster output;
  Testing Classes support current-class Per Class with Extra Info output. The
  shared renderer preserves legacy template row rules. WorkspaceStore print
  projections are built and released one page at a time.
- Verification: the standard incremental macOS build covered Next, roster
  output/editor, Legacy renderer, and SubPrep parity tests. Five focused CTests
  passed, as did an independent `NextStartupWorkspaceDependencyBoundary`
  recheck. After the Tester added a print-port delivery assertion,
  `ClassMngrNextRosterOutputTests` passed again (1/1). Thirty-two inventory,
  catalog, baker, and audit Python tests passed; resource inventory
  `--check`, CMake source ownership (1,902), the dependency boundary, and
  `git diff --check` passed.
- Diagnostic memory route: synthetic 120-class/3,000-student Daily output
  produced 20 pages. Peak retained projection was six classes, 150 rows, and
  156 cells; all 20 page-release callbacks ran. macOS peak RSS was 64.875 MiB.
  This is diagnostic evidence only; selected-class page metadata scales with
  the selection and this is not Windows or Phase 9 memory evidence.
- Capture follow-up: five fresh processes produced 20 regular-preview
  comparisons, including 10 Dark cases. The English/Dark and Korean/Dark
  regular previews were verified nonblank against PDF text and raster output;
  each screenshot page matched its rendered PDF page pixel-for-pixel. The
  earlier blank observation's source is unknown; no production omission was
  reproduced. The 16-image [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/roster-templates-printing/README.md)
  is synthetic presentation evidence, not strict pixel parity. No physical
  printer submission, Windows build, full-suite pass, or Phase 9 claim is
  made. The 7A screenshot comparison and memory evidence, Windows
  packaged-Next memory gate, and 7A/Phase 7 exit gates remain open.

### Candidate #38 — Speaking-evaluation core accepted (2026-10-29)

- Commit: `5b0274d9` (`Phase7 - Add speaking evaluation editor in Next`). The
  Next page uses one fixed 25×11 model for the selected class/evaluation and a
  compact class-summary list. Direct `WorkspaceStore` adapters handle
  evaluation reads/writes and roster-name import; page flows include
  validation, private notes, undo, and retry. Save on class/evaluation switch
  persists before loading the new selection; a failed save retains the draft
  and allows retry. Automatic save is debounced.
- Verification: the independent Tester recheck passed the page QtTest 10/10
  and focused CTest 1/1. Coverage includes save/reopen of changed Unicode
  cells, Save/Discard and failed-save retry, save-on-switch success/failure,
  roster import, validation, notes, undo, and one-model release on suspension
  with 96 class summaries. Next launch and
  `NextStartupWorkspaceDependencyBoundary` passed. CMake ownership verified
  one owner for 1,909 handwritten sources. Resource inventory `--check`
  passed with 194 runtime records (187 direct, 7 generated).
- Evidence and limits: the four 1440×880 English/Korean × Light/Dark
  [captures](../../docs/qt-rewrite/visual-baseline/next/phase-7/speaking-evaluation-core/README.md)
  show presentation only and do not claim pixel parity. The 96-class test
  verifies compact model residency and release on suspension, not a memory
  budget. No full suite, Windows, cross-platform, or Phase 9 memory result is
  claimed. The 7A screenshot comparison and memory evidence, Windows
  packaged-Next memory gate, and 7A/Phase 7 exit gates remain open.

### 7A visual evidence — Initial Setup personal details (2026-10-29)

- The [capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/initial-setup-personal-details/README.md)
  preserves Legacy and Next fixed-size offscreen captures across English/Korean
  and light/dark. All eight are 1024×720; packaged SHA-256 values match the
  source captures. Next labels differ by locale; Legacy retains English labels
  for `ko_KR` because this page has no Korean catalog entries.
- The independent visual Tester passed this screen's review after the Next
  dark signature-canvas contrast fix. Save/reopen and signature-processor tests
  passed, preserving transparent signature bytes. This closes only the setup
  personal-details screenshot subgate.
- Limits: offscreen widget renders do not establish native desktop-window
  behavior. Broader 7A visual evidence, memory evidence, the Windows
  packaged-Next memory gate, and the 7A/Phase 7 exit gates remain open.

### Candidate #39 — AI-assisted Speaking Evaluation comments and batch workflow accepted (2026-10-29)

- Commit: `02fbeca6` (`Phase7 - Add AI-assisted speaking evaluation comments`).
  Class analytics remain in candidate #27. Candidate #39 adds single-student
  and batch AI-assisted comments to the model/view evaluation page, with a
  25-student batch cap, typed eligibility and prompt policies, provider/voice
  preferences, and the configured external HTTPS website flow. Student names
  are redacted from the prompt and restored locally when applying responses;
  no hosted AI API is used. Review/apply uses the existing evaluation matrix,
  undo, dirty-state, and save path.
- Verification: the independent Tester validated one owner for 1,911
  handwritten sources, built the affected targets, and passed focused CTest
  9/9. Direct offscreen QtTest verified centered mouse and Space-key toggles
  for both batch-selection and review indicators, and batch apply/save/page
  recreation/reopen through a real WorkspaceStore fixture. Resource inventory
  and `git diff --check` passed. The four accepted #38 capture files remain
  byte-identical to HEAD.
- Evidence and limits: the [C39 capture
  package](../../docs/qt-rewrite/visual-baseline/next/phase-7/speaking-evaluation-ai-comments-c39/README.md)
  records 16 synthetic English/Korean × light/dark page, prompt, selection,
  and review captures with verified hashes and dimensions. They are
  presentation evidence and do not claim pixel parity. No full suite,
  Windows, hosted memory, or Phase 7 exit-gate result is claimed. Candidate
  #40, reports/PDF/PowerPoint/export jobs, is active; Batch 3 remains open
  through #40.

### Candidate #40 — Reports, PDF, PowerPoint, and export jobs accepted (2026-10-29)

- Commit: `f2b24bd7` (`Phase7 - Add speaking evaluation report exports`). The
  Next report path uses a typed WorkspaceStore context port and resolved
  catalog resources. It supports unsaved report projection, student
  navigation/editing, current/all batch output, single-report PDF/print,
  ZIP/individual PDFs, overwrite handling, and operation-scoped staging with
  cleanup on success, failure, and cancellation. The 25-student limit matches
  the existing Legacy evaluation model row count. PowerPoint was exercised via
  a deterministic adapter; #27 analytics and hosted AI APIs are not duplicated.
- Independent verification built the report widget, batch service, Next page,
  and bootstrap test targets. Focused CTest passed 4/4:
  `ClassMngrSpeakingEvalReportWidgetTests`,
  `ClassMngrSpeakingEvalBatchReportServiceTests`,
  `ClassMngrNextBootstrapTests`, and
  `ClassMngrNextSpeakingEvaluationPageTests`. Direct page interaction verified
  Save As PDF and Print, including one-page PDF output and staged print input.
  The Next report-output target closure excludes Legacy Runtime/Core/Features,
  ResourcePack, and DataService targets. Resource inventory `--check` and
  `git diff --check` passed.
- After C40 integration, `ClassMngrNext` built and
  `NextStartupWorkspaceDependencyBoundary` passed 1/1. The boundary now checks
  the Speaking Evaluation output target's exact direct closure and excludes
  Runtime/Calendar dependencies.
- The [C40 capture package](../../docs/qt-rewrite/visual-baseline/next/phase-7/speaking-evaluation-reports-c40/README.md)
  contains ten synthetic English/Korean × Light/Dark preview and batch
  captures, a one-page PDF, and verified manifest. It records presentation
  evidence only, not Legacy/Next pixel parity. Native PowerPoint automation,
  the macOS protected-area prompt, Windows-specific rendering, hosted Windows
  startup/memory, and the full suite were not verified at candidate acceptance.
  Batch 3 and its Windows evidence support were pushed at
  `2cfb1db386850987b0680e9b3f92704a8cb155e6`. The broader 7A and Phase 7 exit
  gates remain open.

### Phase 7 exit evidence — synthetic schema-v6 `.db` output check passed (2026-10-29)

- The independent `ClassMngrNextRosterOutputTests` target passed after adding
  the complete `SettingsRepository` header needed by the evidence test. The
  focused slot `syntheticRepresentativeSchemaV6DbProfileExitEvidenceParity`
  passed directly offscreen, and the registered CTest passed 1/1. The test
  checks schema version 6, foreign-key integrity, representative teacher,
  class, schedule, roster, and settings values, and a no-sibling profile path.
- The Next PDF matches the Legacy renderer for page size, extracted text,
  expected values, and rendered RGB pixels in the one-page landscape “Per
  Class with Extra Info” template.
- Scope limit: the profile is generated from the synthetic
  `representative_startup.sql` fixture, not a historical user profile. Output
  parity covers one template. This closes the synthetic schema-v6 exit-evidence
  item only; broader 7A and Phase 7 gates remain open.

### Windows Release memory collector — local support verified (2026-10-29)

- The fresh Windows x64 Release workflow is scoped to `Qt-Rewrite` and builds
  the Next package plus fixture targets before collecting three scenarios:
  initial setup, empty schema-v6 workspace, and synthetic 96-class/eight-slot
  workspace. It takes five process samples per scenario and uploads the
  manifest, metrics, summary, and validator output. This evidence collector
  has no numeric memory threshold; Phase 9 owns the separate `<250 MiB` gate.
- Static workflow review confirmed collector failures remain in logs and the
  uploaded failure manifest but do not block later x64 release checks. Artifact
  upload still runs under `always()` when files are available.
- Local verification passed six Python validator tests, the blank schema-v6
  fixture target (CTest 1/1; direct QtTest 3/3), `ClassMngrNext` build, and
  `NextStartupWorkspaceDependencyBoundary` CTest 1/1. CMake ownership validated
  one owner for 1,921 handwritten sources. The fixture's final link has one
  WorkspaceStore archive and no Legacy Runtime/Calendar closure. Hosted result:
  [run 40914914018](https://github.com/papercutter0324/ClassMngr-cpp/actions/runs/40914914018)
  completed with workflow conclusion `success` at the exact pushed SHA, but
  the Windows x64 diagnostic prerequisite build reported missing
  `WorkspaceStoreGateway::loadScheduleImportState` and
  `WorkspaceStoreGateway::applyScheduleImport` declarations/definitions before
  `ClassMngrNextResourcePackage` failed. Because the collector is
  non-gating, the workflow continued; the expected
  `phase7-next-memory-windows-x64` artifact and all four evidence files were
  absent. The collector did not run, so no hosted Phase 7 memory evidence or
  Phase 9 result is claimed.

### Candidate #41 — Campus Directory accepted (2026-10-29)

- Commit: `728c8c14` (`Phase7 - Add Campus Directory and map lifecycle`).

- Scope: the five Campus Dashboard sections—office/printer information,
  directions, address, housing, and maps. Campus Staff is a separate,
  workspace-scoped directory already owned by #17 (Korean Teachers), #18
  (Native English Teachers and GS Team), and #19 (teacher workbook import).
  The Phase 7 §7I list now records that ownership.
- Legacy evidence: normal dashboard construction mounts
  `:/resource-packs/campuses`; `CampusJsonRepository::saveCampus()` attempts
  `QSaveFile` at that read-only resource path and reports a technical failure.
  Before `b97c3d8e` removed editable campus-directory support, overrides were
  stored at the exact `<process cwd>/data/campuses` path. Do not claim current
  packaged Legacy admin edits were persisted.
- Decision: Next reads the packaged campus catalog through ResourceLoader and
  stores overrides under the platform app-data directory, with ID-based
  override precedence and atomic writes. Import only from the one known
  historical `<process cwd>/data/campuses` directory when it exists; do not
  search alternate roots. A previous directory launched from a different
  working directory cannot be discovered automatically. The override remains
  visible after save/reopen and takes precedence over the packaged campus.
- Admin boundary: presence of `--enable-admin` yields a typed campus edit
  capability; absence is read-only. Keep the capability local to the five
  campus routes and their actions. Do not change generic `PageContext`,
  `PageHost`, navigation grants, or app-wide permission behavior; candidate
  #46 owns that gate.
- Map boundary: resolve map assets as catalog resources rather than retaining
  Legacy filesystem/resource-pack paths. Load only for the maps section, bound
  decoded dimensions to the Legacy 1520-pixel maximum, and release map images
  on suspension/leave/release. Preserve campus selection fallback (saved ID,
  then first available campus) and current localized fields.
- Image-cache boundary: bounded and unbounded requests for one resource ID must
  not satisfy one another with an image at the wrong resolution. Verify both
  cache request orders with a synthetic image larger than the bound.
- Acceptance: cover all five routes, default/changed campus selection,
  read-only and admin states, packaged/override merge, historical import from
  only the explicit root, atomic-save failure and retry, reopen persistence,
  and map load/release/reopen. Also verify image-cache requests in both
  bounded/unbounded orders, English/Korean and Light/Dark captures, the narrow
  Next link boundary, resource inventory, and `git diff --check`.
- Independent verification: the standard incremental macOS Debug build
  completed for `ClassMngrNext`, `ClassMngrNextResourceLoaderTests`,
  `ClassMngrNextCampusDirectoryTests`, and
  `ClassMngrNextCampusDirectoryVisualCapture`. Focused CTest passed 3/3;
  Campus Directory direct QtTest passed 12/12. Checks include actual
  `QSaveFile` obstruction with dirty-state preservation and retry, production
  argument-helper presence/absence, migration-root bounds, selection,
  resource lifecycle, cache-resolution order, resource inventory (140 entries),
  and `git diff --check`. The generated Next link command has no Legacy
  Runtime, DataService, or ResourcePack dependency.
- Capture evidence: the [C41 package](../../docs/qt-rewrite/visual-baseline/next/phase-7/campus-directory-c41/README.md)
  contains 40 PNGs in 20 Legacy/Next pairs across five sections, English and
  Korean, and Light and Dark. The independent Tester matched all package PNGs
  to fresh capture outputs by SHA-256 and verified dimensions. The README
  records offscreen rendering and a capture-only Legacy map alias; it makes no
  pixel-parity claim.
- Memory evidence: a whole-process macOS working-set sample was 157,515,776 B
  before Maps, 158,990,336 B after Maps entry and after page release, and
  163,217,408 B after reopen. The loaded-map-resource count returns to zero on
  release, though whole-process working set does not shrink at that point.
  Two maps decoded at 1230×1150 and 1225×859, below the 1520-pixel cap. This is
  lifecycle evidence with no numeric threshold, not Phase 9 evidence.
- Limits: no full suite, other platform, native-screen rendering, or full
  application launch with the admin flag was run. The helper used by production
  bootstrap is covered for both flag states. Migration checks only the exact
  `<process cwd>/data/campuses` path; a different historical working directory
  cannot be found automatically. The shared ResourceCache retains decoded
  images until LRU eviction, but C41 constructs its map loader without that
  cache and releases its map resources on page lifecycle. Windows diagnostic
  memory evidence remains unavailable pending a fresh hosted run after the
  setup repair and Batch 4 push.

### Initial Setup schedule-import diagnostic build repair — accepted (2026-10-29)

- Commit: `80f7f1c6` (`Phase7 - Repair setup schedule import build`).
- Change: Initial Setup resolves the active profile path from `WorkspaceState`
  and uses the existing path-scoped schedule-import read/apply adapters. The
  setup callbacks no longer depend on the missing `WorkspaceStoreGateway`
  schedule-import members. The protected Gateway pair remains pre-existing
  user work and was not included in this repair.
- Regression: the still-open setup Gateway sees six imported regular schedule
  rows; an injected SQLite trigger failure preserves workspace table counts,
  settings, pending setup state, and the recovery journal and original-profile
  backup byte-for-byte. After successful apply, a fresh `WorkspaceStore`
  confirms the six rows persisted. The generic Gateway `saveWorkspace` call is
  not part of setup Finish and was removed from the regression because the
  schedule-import use case commits its own transaction.
- Independent verification: the incremental standard macOS Debug build
  completed for the setup and normal schedule-import integration targets.
  Focused CTest passed 3/3: setup integration, normal schedule-import
  integration, and `NextStartupWorkspaceDependencyBoundary`.
  `git diff --check` passed. No full suite or local Windows build ran.
- Limit: Windows Release run `40914914018` still has no diagnostic artifact;
  the corrected prerequisite must be exercised by a fresh hosted run after the
  Batch 4 push. No hosted Phase 7 memory evidence or Phase 9 result is claimed.

### Candidate #42 — Document catalog accepted (2026-10-29)

- Commit: `0d635731` (`Phase7 - Add localized document catalog navigation`).
- Boundary: the Next shell consumes the existing immutable
  `DocumentCatalogProjection` and renders a localized nested tree. Document
  leaves carry stable `DocumentId`, folder identity, order, and print/export
  metadata. Selection emits the typed ID without entering page history.
  Candidate #43 owns selected PDF loading and viewer lifecycle. Startup and
  catalog rendering do not load PDF bodies or QtPdf pages. Keep strict
  validation for the packaged metadata; do not reintroduce Legacy
  partial-catalog recovery behavior for this immutable resource.
- Independent verification: the incremental standard macOS Debug build
  completed for the C42 navigation, visual-capture, bootstrap, navigation, and
  shared-staff targets. Those five focused CTests passed, along with
  `ClassMngrNextResourceDocumentCatalogTests`,
  `ClassMngrDocumentCatalogNavigationParityTests`, and
  `NextStartupWorkspaceDependencyBoundary` (8 focused tests total). Coverage
  includes nested sibling ordering, typed selection, print/export flags,
  locale projection refresh with selection retained, startup projection
  population, metadata-missing startup failure, and metadata loading while a
  PDF body is absent. `git diff --check` passed.
- Capture evidence: the [C42 package](../../docs/qt-rewrite/visual-baseline/next/phase-7/document-catalog-c42/README.md)
  contains four 1024×720 PNGs across English/Korean and Light/Dark. The Tester
  matched each SHA-256 to fresh CTest capture output and verified PNG integrity
  and dimensions. The manifest records 7 folders, 30 documents, and zero
  non-metadata resource diagnostics. Its zero-document counters are harness
  observations corroborated by source inspection and the body-absent resource
  test. The capture executable has no QtPdf link. No pixel-parity claim is
  made.
- Memory evidence: `PlatformProcessMemorySnapshotProvider` on macOS
  `TASK_VM_INFO` measured whole-process resident/private usage at
  37,666,816/12,468,896 bytes before projection, 64,815,104/24,200,032 bytes
  after rendering, and 64,864,256/24,200,032 bytes after shell release. No
  PDF bodies were loaded. These are process-wide lifecycle observations with
  no numeric threshold; memory did not shrink after release, and no Phase 9
  result is claimed.
- Limits: the current Next app has no runtime locale-switch workflow. The
  projection-refresh API and selection preservation are tested directly; a
  future locale coordinator must call it after changing the translator. The
  missing-metadata startup failure is tested; malformed-metadata rejection is
  supported by code inspection but has no focused negative regression. The
  unrelated `ClassMngrNextTeacherDirectoryVisualCapture` target remains
  un-linkable in this worktree due to missing `ColorUtils`, `FileNameUtils`,
  `ClassInfoConfig`, and profiler-compat symbols in its narrow closure; C42's
  capture and relevant shell tests pass, so that target was not expanded.
  No full suite or Windows build ran.

### Candidate #43 — Document viewer lifecycle/actions accepted (2026-10-29)

- Commit: `3ac8fea4` (`Phase7 - Add document viewer lifecycle and actions`).
  The Next shell consumes the stable `DocumentId` selection from #42 and loads
  only the selected PDF into a file-backed viewer session. Catalog construction
  and startup remain metadata-only. The page preserves page navigation,
  incremental and fit zoom, save/copy of the optional catalog export asset,
  print capability, close, replacement, suspension, and reopen behavior.
- Lifecycle: ordinary close/suspension closes the PDF document before
  releasing its borrowed stream and session. A deterministic timeout fallback
  destroys the view/document while the stream remains alive, releases the
  session resources in order, and recreates empty wrappers for a fresh reopen.
  Tests cover timeout on close and suspension, release ordering, export
  cancellation/failure, save/open and print delegates, Korean `닫기`, and zoom
  reset after reopen. No numeric memory threshold is applied before Phase 9.
- Build and verification: a fresh configure in the standard
  `build/macos-clang-debug` completed after moving `ClassMngrNext` target
  creation to `cmake/next_app`, which scopes Qt 6.12's import scan away from the
  in-repository Release staging tree while retaining the explicit calendar
  QRC. Built `ClassMngrNext`, `ClassMngrNextDocumentViewerTests`,
  `ClassMngrNextDocumentViewerVisualCapture`,
  `ClassMngrNextLegacyDocumentViewerVisualCapture`,
  `ClassMngrNextDocumentCatalogNavigationTests`,
  `ClassMngrNextBootstrapTests`,
  `ClassMngrNextInitialSetupTeacherEntryBootstrapIntegrationTests`,
  `ClassMngrNextApplicationDocumentCatalogUseCaseTests`,
  `ClassMngrNextApplicationDocumentContentTests`,
  `ClassMngrNextPlatformDocumentContentResourcePortTests`,
  `ClassMngrNextPlatformDocumentExportResourcePortTests`,
  `ClassMngrNextPlatformQtPdfSessionReleaseTests`, and
  `ClassMngrNextPageHostTests`.
  Independent focused CTest passed 14/14: `ClassMngrNextDocumentCatalogNavigationTests`,
  `ClassMngrNextDocumentViewerTests`, `ClassMngrNextDocumentViewerVisualCapture`,
  `ClassMngrNextLegacyDocumentViewerVisualCapture`, `ClassMngrNextBootstrapTests`,
  `ClassMngrNextInitialSetupTeacherEntryBootstrapIntegrationTests`,
  `NextStartupWorkspaceDependencyBoundary`,
  `ClassMngrNextApplicationDocumentCatalogUseCaseTests`,
  `ClassMngrNextApplicationDocumentContentTests`,
  `ClassMngrNextPlatformDocumentContentResourcePortTests`,
  `ClassMngrNextPlatformDocumentExportResourcePortTests`,
  `ClassMngrNextPlatformQtPdfSessionReleaseTests`, `ClassMngrNextPageHostTests`,
  and `ClassMngrNextLaunch`.
  Paired Next/Legacy captures cover ready/error/closed/reopened in
  English/Korean ×
  Light/Dark. All 32 PNGs are 1024×720 and hash-match fresh captures; visual
  review found no material content/state mismatch. The package calls the
  comparison illustrative and makes no pixel-parity claim.
- Limits: no full suite, Windows build, packaged Release run, or Phase 9 memory
  gate ran. Capture memory is process-wide, without viewer-specific
  attribution. The timeout branch is forced through an injected waiter rather
  than a real five-second QtPdf hang. No physical printer or interactive native
  print dialog was exercised; runtime locale switching remains outside the
  #42/#43 boundary.
- Decision and next: keep viewer content and streams operation-scoped and keep
  QtPdf consumers gone before stream/session release. Batch 4 continues with
  #44, 7J.1 Sub Prep, which owns the required 96-class class-information and
  output migration.

### Candidate #44 — Sub Prep class information and output accepted (2026-10-29)

- Commit: `8c4fece5` (`Phase7 - Migrate Sub Prep class information and output`).
  The Next shell now reads Sub Prep data through typed operations on the same
  private `WorkspaceStore` owned by `WorkspaceStoreGateway`. It does not expose
  the store or open a second database. The page uses one compact summary model,
  one reusable selected-detail panel, six active editors, bounded scoped
  queries, and explicit release/re-entry behavior. Output projections are
  operation-scoped; Pretendard variable-font axes are set explicitly so PDF
  headings and body text embed the intended Bold and Regular faces separately.
- Lifecycle and output: the unchanged 96-class / 8-slot fixture projects all
  96 summaries, verifies mode/day refresh, selection and release/re-entry, and
  rejects its Monday 4:00 PM schedule conflict without creating a package. A
  compatible `representative_startup.sql` route generates nonempty Sub Prep
  and daily-roster PDFs. The Next output Cancel route verifies no destination
  is created and the selected class/detail remain intact. Four 800×600 captures
  cover English/Korean × Light/Dark, plus empty and conflict states.
- Verification: the incremental standard `build/macos-clang-debug` build
  completed for the affected Next, output, font, and resource targets. Focused
  CTest passed 13/13 plus roster parity 1/1. QtTest totals included package
  service 14, print/PDF 24, list model 9, roster parity 6, Next bootstrap 72,
  active-store Gateway 4, resource pack 12, FontManager 15, Schedule Import
  workflow 16, normal integration 14, and state/apply ports 8/9. The optional
  external Schedule Import sample check skipped once because
  `CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset (99 passed, 1 skipped). The
  focused Next Cancel/lifecycle slot passed 3/3. Resource inventory `--check`
  and `git diff --check` passed. The strict PDF comparison passed unchanged
  references for all 19 Sub Prep pages and 16 daily-roster pages, including
  page counts, dimensions, and per-page extracted text.
- Limits: Windows 150-DPI raster parity was not run. Same-renderer macOS
  Qt 6.11 comparisons changed 11.05–12.21% of daily-roster pixels and
  0.76–10.35% of Sub Prep PDF pixels; the reference is several weeks old and
  its Windows provenance is unverified, so pixel parity is not claimed. Both
  reference and candidate PDFs lack a Unicode mapping for the separator glyph;
  local extractors return U+001F, and copy/search correctness is not claimed.
  `ClassMngrSubPrepPageTests::unavailablePreferenceLoadsPreservePageValues`
  hangs in macOS offscreen modal behavior and is not counted as a pass; the
  targeted Legacy Cancel test passed. No full suite, Windows build, or Phase 9
  packaged Release numerical memory gate ran. Memory samples are process-wide.
- Decision and next: preserve the strict PDF references and keep the Windows
  pixel and Phase 9 memory gates open. Batch 4 continues with #45, General
  app-wide Preferences UI/menu; its scope must integrate existing Next
  page-owned settings rather than duplicate their controls.
