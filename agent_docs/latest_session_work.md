# Latest Session Work

Qt Rewrite Phase 1 has started on branch Qt-Rewrite. Phase 0 was completed on
2026-09-18: the combined Windows x64 and macOS universal exit gate passed all
24 required routes on both platforms, and the user approved the retained visual
references. The Phase 0 plan update in commit f8bb5954 is authoritative over
older Phase 0 open notes in this document's history.

## Current Deployment Handoff

- Deployment: qt1_phase1_resume_20260918, Heavy route.
- User requirement: commit each completed slice before starting the next.
- Slice 1.1 adds ClassMngrNext as a console bootstrap implemented with
  QCoreApplication. Its target links only Qt Core and has an explicit CTest
  launch probe. It has no window, legacy runtime, production resources, or
  deployment hook.
- A fresh temporary Ninja/MSVC Debug build configured with Qt 6.12 and built
  both ClassMngr and ClassMngrNext successfully (351 build steps). The focused
  ClassMngrNextLaunch CTest passed (1/1). Independent review confirmed the
  new executable links Qt Core plus MSVC/UCRT and Windows system libraries,
  without ClassMngrRuntime.
- Slice 1.2 adds six source-free next-generation layer interface targets and
  eleven source-free feature interface targets with `ClassMngrNext::` aliases.
  CMake asserts each target's exact allowed dependency list: Application →
  Domain/Persistence, Persistence → Domain, Platform → Application,
  UiShared → Application/Resources, and each feature →
  Application/Resources/UiShared. Domain and Resources have no dependencies;
  features do not depend on one another. ClassMngrNext remains linked only to
  Qt Core and does not link these placeholders.
- A fresh Ninja/MSVC Debug configure and build for slice 1.2 passed in 351
  steps for both executables; `ClassMngrNextLaunch` passed (1/1). The clean
  build directory was
  `C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.2-clean-13839b2038864e939f913007c0b33405`.
  It is local verification output, not a checked-in artifact.
- Slice 1.3 moves Qt modules out of `ClassMngrBuildSettings` and assigns the
  measured module sets to the six existing legacy production object targets.
  `ClassMngrRuntime` and the macOS `ClassMngrTestRuntime` retain the complete
  legacy module union, including QuickControls2 for the QML/resource graph.
  The Phase 1 plan records the per-target module map.
- A fresh Ninja/MSVC Debug configure and build for slice 1.3 passed for both
  executables in 351 steps; `ClassMngrNextLaunch` passed (1/1). Independent
  inspection confirmed the Domain compile command has only QtCore/QtGui
  include paths, the `ClassMngr.exe` link command retains the full legacy Qt
  set, and `ClassMngrSharedPolicyTests` built and passed (1/1). The clean build
  directory was
  `C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.3-clean-4ffa6756081f4d0bb0044b1a12758723`.
  It is local verification output, not a checked-in artifact.
- Slice 1.4 replaces recursive production source discovery with explicit
  manifests for the six legacy object targets, both executable entry points,
  and the two calendar QML files. Seven included roster `.inc` fragments are
  marked header-only and assigned to `ClassMngrFeatures`; the generated build
  info header remains a distinct `configure_file()` input. The new configure-
  time check treats source globs only as an inventory of handwritten `src`
  and active test files, and requires one target owner per file. Shared
  schedule test doubles now compile once in test-support object libraries; the
  ResourcePackManager fake is split out because ClassesPage uses the real
  manager.
- A clean Ninja/MSVC Debug configure and targeted build succeeded for
  `ClassMngr`, `ClassMngrNext`, and the five test executables using the shared
  schedule stubs. The ownership check validated 653 handwritten files for
  Windows x64, excluding the Apple-only PowerPoint notice test. The
  `ClassMngrNextLaunch` probe and all five affected tests passed CTest (6/6).
  A temporary unassigned `src/` probe was rejected by the ownership check; it
  was removed and a clean reconfigure passed again.
  The clean build directory was
  `C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.4-clean-5d33542afed54ad8bbefc3546133c8e2`.
  It is local verification output, not a checked-in artifact.
- Slice 1.5 adds formatting and static-analysis checks scoped to `src/next`,
  asserts that `ClassMngrNext` links only Qt Core, writes Qt module and
  resource-pack reports, checks runtime resource references, and adds staged
  package checks and size reports to the Windows, macOS, and Linux Release
  workflows. Startup performance CTest is labeled
  `startup;memory;performance`.
- A clean Ninja/MSVC Debug configure and full build passed for `ClassMngr` and
  `ClassMngrNext` (351 steps); `ClassMngrNextLaunch` passed (1/1). The Qt module
  report listed only `Qt6::Core` for `ClassMngrNext`; the resource checker
  passed for six generated RCC packs and seven runtime IDs/references; a
  staged-package report probe passed. Startup/memory labels were discovered,
  but the performance test was not built or run. Cross-platform CI and local
  `clang-format`/`clang-tidy` were not run. The clean build directory was
  `C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.5-clean`.
  It is local verification output, not a checked-in artifact.
- Slice 1.5 was committed as `65cd76fb` (`Phase1 - Add tooling and CI build
  reports`). Its Windows build, launch probe, resource checks, and staged
  package report passed; hosted CI and local clang tools were not run.
- Slice 1.6 updates `.github/workflows/refactoring-baseline.yml` to run on
  relevant pull requests and validate the existing Debug presets for Windows
  x64, Windows ARM64, macOS universal, and Linux. Native jobs run the baseline
  build/tests including the `ClassMngrNext` launch probe. Windows ARM64
  cross-builds `ClassMngr` and `ClassMngrNext` on an x64 runner without
  executing the target binaries. The Windows installer, macOS DMG, and Linux
  install-tree archive workflows were left unchanged as the Packaged Release
  paths.
- An independent static review passed the four-preset matrix, PR path filters,
  Qt host/target setup, and native-versus-cross execution rules. CMake preset
  listing/JSON assertions and `git diff --check` passed. The hosted jobs,
  Windows ARM64 cross-build, and workflow YAML parser/actionlint were not
  available for local execution; no hosted run result is claimed. Slice 1.6
  was committed (`Phase1 - Validate build configuration matrix`).
- Continuation validation used a clean source snapshot of commit `6f2f5fb0`.
  The local Windows x64 Debug baseline configure/build and CTest passed 66/66,
  including `ClassMngrNextLaunch` and `ClassMngrStartupPerformanceTests`. JUnit,
  `LastTest.log`, and the baseline JSON are preserved under
  `build/phase1-windows-x64-20260918-d01027708c2f4cb792b7e6bb13ce5c8a/`.
- The local Windows x64 Release preset configured and built `ClassMngr`, ran
  `windeployqt`, installed to a staged tree, and compiled the production Inno
  target. The packaged startup smoke exited 0 in 3.6 seconds with
  `finalProgress=100`. Resource/report checks passed for six RCC packs, seven
  runtime IDs, and seven references. The staged tree measured 129 files and
  207,477,952 bytes; the installer measured 93,160,351 bytes. These runs used
  Visual Studio 2026 / MSVC 19.51 with Qt 6.12, not the hosted VS2022 toolchain.
  The exact VS17 preset failed because no VS2022 instance is installed; local
  VS18 generator attempts also hit Windows FileTracker access errors before an
  elevated Ninja/MSVC run succeeded.
- After the local Qt 6.12 update completed, a fresh Windows x64 Ninja/MSVC
  Debug configure and full build passed on the working tree based on source
  commit `4dbe3ca7`. Configure-time source ownership validated 653 handwritten
  files, and the full CTest suite passed 66/66 in 179.41 seconds. The resource
  reference report passed for six RCC packs, seven runtime IDs, and seven
  references; the build report recorded a 44,797,952-byte executable and
  39,331,047 bytes across six RCC packs. Local compilation used VS
  2026/MSVC 19.51, so it is supplemental to the hosted VS2022 toolchain.
- The CTest follow-up makes Windows test executables prepend the selected Qt
  `bin` path, including `ClassMngrNextLaunch`, and gives
  `StartupVisualSettingsTests` a build-local settings root. The startup
  performance test now runs serially: its initial full-suite run overlapped a
  heavy batch-report test and measured 7.9 seconds; an isolated run measured
  about 3 seconds and passed. Focused reruns and the final full suite passed.
- At validation start, the cached `origin/Qt-Rewrite` ref matched source HEAD
  at `4dbe3ca7`. No fresh GitHub Actions or PR query was made during this
  validation, so hosted run status remains unverified. Earlier `git ls-remote`
  and browser queries could not reach GitHub, and `gh` was unavailable. WSL,
  Docker, and Podman are unavailable locally; a macOS toolchain is available
  and was used for the validation below.
- The slice 1.1 clean build directory was
  C:\Users\wfelt\AppData\Local\Temp\ClassMngr-Phase1-1.1-clean-965a9613fb8046b7bbbbd5a520b03740.
  It is local verification output, not a checked-in artifact.
- A direct clean configure with the Visual Studio 18 generator from the normal
  PowerShell environment could not identify its C++ compiler. The clean Ninja
  configure/build under VsDevCmd succeeded; use that route on this host.
- Slice 1.3 changes `cmake/sources.cmake`; slice 1.2 changes
  `cmake/next.cmake`; slice 1.1 changed `CMakeLists.txt`, `cmake/next.cmake`,
  and `src/next/main.cpp`.
- Phase 1 is not complete. No cross-platform v2 build has been verified yet.

## Local macOS Phase 1 Validation — 2026-09-18

- On macOS 27.0 arm64 with Qt 6.12.0, the Debug universal build passed for
  ClassMngr and ClassMngrNext. Source ownership passed for 654 handwritten
  files; both executables passed arm64/x86_64 and macOS 14.4 minimum-version
  checks, and ClassMngrNext remained linked only to Qt Core.
- The Release installer command completed and created the universal DMG.
  Signing, hdiutil verification, 92 bundled Mach-O architecture/minimum-version
  checks, the resource check (6 RCC packs, 7 runtime IDs, 7 references), and
  the build report passed. Qt's deployment scan printed missing dependency
  paths for optional Mimer, ODBC, and PostgreSQL SQL drivers; the deployment
  postamble removes those drivers, and the staged app contains only
  libqsqlite.dylib. Signature replacement notices and hdiutil's deprecation
  warning were nonfatal.
- The first full CTest run in the restricted Codex environment reported
  62/67. Three UI tests passed on focused reruns, and the updater test passed
  with normal loopback access. The InitialSetupWizard target then passed 4/4
  under CTest with normal macOS service access, retaining its offscreen
  platform. In the restricted run, its first test aborted at wizard.show()
  with NSInvalidArgumentException (`NSBundle initWithURL:nil`, SIGABRT 6):
  Qt's macOS QWizard background lookup asks LaunchServices for
  com.apple.KeyboardSetupAssistant, and the restricted process receives a nil
  URL. A later fresh isolated macOS universal Debug configure and 656-step
  build passed on the current working tree. The complete 67-test CTest suite
  then passed 67/67 with normal macOS service access in 67.70 seconds. The
  restricted rerun's five failures were caused by LaunchServices, display, and
  loopback restrictions; the corresponding tests passed in the normal-service
  run. The passing JUnit report and CTest log are in
  `build/phase1-macos-debug-local-20260918/Testing/normal-services.junit.xml`
  and `Testing/Temporary/LastTest.log` under that build directory. The
  executables both contain arm64/x86_64 slices and target macOS 14.4;
  `ClassMngrNext` links Qt Core only.

- The baseline runner now applies its explicit `--build-dir` to both CMake
  configure and build commands. This allowed the clean local run to use an
  isolated directory without replacing the earlier preset build's test logs.
  The hosted workflow now has one bounded macOS rerun when a failed first
  attempt publishes no baseline report artifact. Published test failures stay
  failed without a retry. The workflow change has not yet run on GitHub.

## Next Entry Point

Slices 1.1-1.6 are committed; the latest hosted baseline run
`35424488211` passed Windows x64 Debug 66/66, macOS universal Debug 67/67,
Linux x64 Debug, and the informational Windows ARM64 cross-build on commit
`0883009d`. The Phase 1 Build Quality, Dialog policy, and Release workflows
also passed. Phase 1 is closed; the next entry point is the Phase 2
application use-case contract over the new domain types.

## Current Deployment Handoff — linux_phase0_phase1_20260919 (paused)

The user requested Linux Phase 0 and Phase 1 follow-up. The original Phase 0
official exit gate remains Windows x64 plus macOS universal; this work adds a
supplemental Linux x64 baseline without changing that gate.

- Commit `749c9ba6` adds `scripts/phase0/run_phase0_evidence_linux.py`, Linux
  validator support, runner tests, and an opt-in hosted workflow. The workflow
  installs `xauth`/`xvfb` when requested, consumes the staged Release package,
  and uploads bounded evidence. Runner tests passed 9/9; validator self-tests
  passed 17/17; Python compilation, workflow YAML parsing, and diff checks
  passed.
- The local Linux x64 Release package and Debug route harness built. The
  packaged run used the staged Qt xcb plugin. This sandbox cannot create the
  root-owned `/tmp/.X11-unix` directory required by Xvfb, so the package smoke
  and all 24 route attempts exited before app startup. Validation reports
  0/24 passing, 24 failed, 0 skipped. No Linux route baseline pass is claimed.
  The hosted opt-in workflow was not run because GitHub access was unavailable.
- Commit `898cd3fc` fixes Linux process-memory snapshots. `QFile::atEnd()`
  treated procfs files reporting size zero as exhausted, so the parser did not
  read `/proc/self/status`. The implementation now reads until `readLine()`
  returns empty; tests cover an injected procfs root, conversions/fallbacks,
  unavailable RSS, and live sampling.
- Independent checks passed `ClassMngrProcessMemorySnapshotTests` and
  `ClassMngrStartupPerformanceTests` (1/1, 43.44 seconds). Full Linux CTest
  passed 66/67; `ClassMngrUpdaterTests` had 11 loopback listener failures
  because local socket creation returns `EPERM` in this sandbox. Build,
  resource checks, build report, and `ClassMngrNextLaunch` passed. The local
  `clang-format`, `clang-tidy`, and `actionlint` tools were unavailable; PyYAML
  parsed the changed workflows. These results are local, not hosted CI.
- The last repository-recorded hosted Linux Debug run was 65/66 on commit
  `57f5dff6`; it failed the same startup memory availability check. GitHub
  queries could not verify runs after the current September 19 source. The
  previous hosted Linux Release run is historical evidence only.

## Next Entry Point

Push the two code commits only with user authorization. On a host with a
working Xvfb display socket, manually run the Linux Phase 0 baseline workflow
and retain its evidence artifact. Rerun the Linux Debug CTest suite on a host
that permits local sockets, then record hosted results separately. Do not
revise the completed Windows/macOS Phase 0 gate based on the supplemental
Linux attempt.

## Current Deployment Handoff — windows_macos_recent_commit_repair_20260919

- The user reported Windows and macOS builds failing after a recent commit.
  The failing hosted run logs were unavailable: the local GitHub token was
  invalid, network access blocked run queries, and no matching artifacts were
  present locally.
- Root cause: commit `898cd3fc` added
  `tests/process_memory_snapshot_tests.cpp` to a Linux-only test target, while
  `cmake/source_ownership.cmake` inventoried all test sources on Windows and
  macOS. CMake then failed configure with that file reported unassigned.
- The fix excludes the Linux-only source from non-Linux ownership inventory;
  Linux still inventories and assigns it to `ClassMngrProcessMemorySnapshotTests`.
- Local Windows x64 Ninja/MSVC Debug configure validated 653 source owners; a
  clean build passed 649/649 steps, and CTest passed 66/66 in 112.05 seconds
  with VS18/MSVC 19.51 and Qt 6.12. Independent clean configure validation
  passed and `git diff --check` passed. The VS17/VS2022 shipping toolchain was
  unavailable locally.
- Darwin follows the same non-Linux inventory branch by source inspection,
  but no native macOS/Xcode build or post-fix hosted run was available. The
  repair is committed locally; no push was made.

## Next Entry Point

Phase 1 hosted acceptance is closed. Continue Phase 2 with the first
application use-case input/output contract; keep the paused Linux follow-up
separate on a host with Xvfb and loopback access.

## Current Deployment Handoff — phase2_domain_contract_kickoff_20260919

- The hosted `Refactoring baseline` run `35424488211` passed all matrix jobs on
  commit `0883009d`, including macOS universal Debug 67/67 and Windows x64
  Debug 66/66. The Phase 1 Build Quality run `35424488214`, Dialog policy run
  `35424488244`, and the Windows, macOS, and Linux Release runs also passed.
  Phase 1 is closed; Linux and Windows ARM64 remain informational.
- The first Phase 2 slice is implemented locally. `ClassMngrNext::Domain`
  owns `src/next/domain/domain_types.h` and
  `src/next/domain/operation_result.h`; both are standard-library-only
  headers and are explicitly recorded by source ownership.
- `ClassMngrNextDomainContractTests` covers typed IDs and structured results
  without constructing a `QApplication`. The Windows Debug target and
  `ClassMngrNextLaunch` passed after reconfiguration.
- Next entry point: add the first application use-case input/output contract
  over these domain types, then map it to a deterministic test boundary.

## Current Deployment Handoff — async_prompt_title_20260919

- Goal: repair the asynchronous prompt test-driver snapshot so
  `PromptRequest::title` is preserved in the visible `QMessageBox` without
  changing workflows or weakening the 67-test suite.
- The Executor changed only
  `src/ui/shared/dialogs/user_prompt_service.cpp`: synchronous and
  asynchronous acknowledge prompts share configuration, and the requested
  title is reapplied to the actual widget after button setup and before
  `exec()`/`open()`. The driver still snapshots `windowTitle()`.
- Independent verification passed `ClassMngrDialogServicesTests` (1/1), the
  direct async test (3/3), synchronous/shared-policy coverage, and
  `git diff --check`. A complete local CTest run passed 62/67; the five
  failures were unrelated no-screen GUI and updater local-port restrictions.
  Only non-fatal `propagateSizeHints()`/font-alias warnings remained in the
  passing dialog tests.
- Commit `0883009d` records this production fix; the hosted baseline and
  release workflows passed on that source. A later local Phase 2 contract
  slice is recorded separately above.

## Historical Handoff — phase2_action_registry_persistence_20260923

- The user requested a pause, a handoff, a commit, and a push after the
  current Phase 2 work. The three slices completed in this continuation are
  committed as c30f13e0, 7caef52e, and df8202ed.
- Theme persistence now uses the typed ThemePreferencesPort and retains the
  options/theme values Dark=0, Light=1, and SystemDefault=2. The generic
  OptionState SettingsManager fallback and settings-key constructor argument
  were removed after all eight ActionRegistry states received typed callbacks.
- File-dialog directory preferences now use a Qt-free Application port and a
  QSettings Platform adapter. The adapter retains all eight existing
  file-dialog/directories/<purpose> keys and purpose slugs. QtFileDialogService
  receives the Application port; main constructs the adapter and service
  before MainWindow. The test-service override remains available. CMake source
  ownership and the Application dependency on ClassMngrUiShared are explicit;
  UI has no Platform dependency.
- Independent verification passed ClassMngrNextPlatformSettingsManagerThemePreferencesPortTests
  and ClassMngrAiCommentOptionsTests (2/2); ClassMngrAiCommentOptionsTests
  and ClassMngrStartupVisualSettingsTests (2/2); then ClassMngrDialogServicesTests
  and ClassMngrNextPlatformQSettingsFileDialogDirectoryPreferencesAdapterTests
  (2/2). The final configure validated 840 handwritten sources and Next target
  assertions. ClassMngr and both focused test targets built; git diff --check
  passed.
- The file-dialog home-directory fallback assertion runs only when
  QStandardPaths returns an empty writable location. Verification did not
  force that condition. No hosted cross-platform run was performed.
- Two independent scans found no other live direct application preference
  persistence candidate. Unused legacy settings helpers and a read-only
  PowerPoint registry probe remain outside the migration scope.
- This handoff recorded the pause state at that time. Later user requests
  resumed Phase 2 with one commit per slice; the current continuation below
  supersedes its next-step note. Keep the Linux Phase 0 follow-up separate.

## Current Deployment Handoff — phase2_contract_slice_resume_20260923 (complete)

- Added `src/next/application/sub_prep_schedule_summary_query.h`, a Qt-free
  schedule-scope query over typed visible class IDs, weekdays, and regular or
  intensive mode. It uses an injected read port and the existing
  `ClassSummaryProjection`; request validation, empty visibility, projection
  bounds, deterministic ordering, out-of-scope rows, selected details, and
  structured read failures are explicit.
- Added app-less coverage in
  `tests/next_application_sub_prep_schedule_summary_query_tests.cpp` and
  registered `ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests`.
  CMake Release configuration validated ownership of 701 handwritten files;
  `ClassMngrNext` built, and the focused CTest passed 1/1. `git diff --check`
  passed on the test lane's owned files.
- The query contract is not connected to the live Sub Prep page and has no
  persistence adapter. No SQLite batching, output migration, UI parity, or
  memory improvement is claimed. The existing 96-class Release baseline and
  its memory failure remain the acceptance reference for later feature work.
- Next entry point: continue Phase 2 with another application-contract slice.
  Implement the Sub Prep persistence adapter under Phase 3, then connect the
  UI/model, selected details, output, and release-memory acceptance under the
  feature and memory plans. Keep the Linux Phase 0 follow-up separate.

## Current Deployment Handoff — phase2_next_contract_slice_20260923 (complete)

- Continued Phase 2 after the typed Sub Prep schedule summary query. Commit
  `389d90a6a433ae6c5c7ce7263daba02f4a27a5ce` adds the Qt-free
  `SubPrepClassDetailsQuery` and standalone validation for one bounded detail
  value. Its injected read port propagates structured errors, checks the
  returned class ID, and permits a missing teacher identity. The focused
  `ClassMngrNextApplicationSubPrepClassDetailsQueryTests` target passed 1/1.
- Commit `7959eb07` adds `SubPrepClassInformationState`, a value transition
  boundary for refresh, selection, detail application, and clear. It retains a
  class only while visible, clears details on each successful refresh or
  selection change, and matches incoming details to both the selected class
  and the teacher identity in its current summary. The focused
  `ClassMngrNextApplicationSubPrepClassInformationStateTests` target passed
  1/1. Neither target was added to the full-suite gate.
- Both targets compiled and linked in
  `build/phase1-validation-ad635ace-windows-x64-ninja` under the Visual Studio
  x64 developer environment. The three contract commits and this documentation
  handoff are recorded on `Qt-Rewrite`; the working tree is clean and nothing
  was pushed.
- Neither contract is wired to the legacy page or a persistence adapter. No
  UI fallback ordering, SQL batching, package/PDF migration, or memory
  improvement is claimed. Next entry point: define the narrow operation-scoped
  Sub Prep print-source contract, keeping roster/package output and Release
  memory acceptance as later gates. Keep the Linux Phase 0 follow-up separate.

## Current Deployment Handoff — phase2_complete_continue_20260923 (executing)

- Goal: continue Phase 2 until its application-contract exit gate is met;
  commit each finished slice and start the next. The branch is `Qt-Rewrite`.
- Commit `3802819b` adds the Qt-free `SubPrepPrintSourceQuery` contract for
  typed class/day/mode scope and an operation-owned, bounded information-sheet
  source. The query validates scope and data all-or-nothing, avoids reads for
  empty scope, preserves the port's stable order, and does not cache results.
- `ClassMngrNextApplicationSubPrepPrintSourceQueryTests` built in
  `build/phase1-validation-ad635ace-windows-x64-ninja`; focused CTest passed
  1/1. No persistence adapter, legacy page/PDF wiring, SQL batching, output
  parity, roster/package migration, source-release, or memory improvement was
  established. The Phase 2 plan, mapping, and Sub Prep memory plan now record
  this boundary and its limits.
- Commit `83163b0a` moves `ColorUtils` to the Application custom-color
  preference port and composes the Platform adapter at all seven UI callers.
  The utility no longer references `SettingsService` or the Platform adapter.
  Its new offscreen test checks all 16 load/save slots and canonical write
  values; the existing adapter test covers null-settings defaults and no-op
  writes. Both Windows x64 Ninja targets built and focused CTest passed 2/2.
- Commit `d5a5cab9` adds a Qt-free `CalendarEventImportPlan` and integrates it
  after the existing range read. It selects candidate indices in stable order,
  carries forward parser skips, and preserves the existing batch save, error,
  metric, and signal paths. The legacy six-field signature is passed as exact
  UTF-16 keys so malformed surrogate sequences retain `QString` equality.
- The new app-less planner tests cover existing and repeated candidate keys,
  stable accepted indices, skipped counts, empty input, and exact UTF-16 key
  comparison. Calendar parser tests lock all six signature fields, their
  normalization, and fields excluded from identity. The two Windows x64 Ninja
  targets built and focused CTest passed 2/2. Range retrieval and batch
  persistence remain legacy responsibilities.
- Commit `d14155c1` moves the calendar import's existing-event signature read
  behind `ApplicationServicesCalendarEventPort`. It queries the same date
  range and returns ordered UTF-16 keys using the established six-field
  signature, while leaving workbook parsing and batch save unchanged. The
  dedicated read avoids the general projection's 4,096-row cap and unrelated
  metadata checks.
- Adapter tests compare Unicode, normalization, duplicate, and ordering
  behavior against `CalendarImport::calendarEventImportSignature`; they cover
  invalid/unavailable/read failures and a successful 4,097-row range. The app
  and port test target built with Windows x64 Ninja; focused CTest passed 1/1.
  Importer network/signal integration and batch save migration remain open.
- Commit `2daae4ef` adds `ApplicationServicesSubPrepPrintSourcePort` and its
  scoped ClassService/repository read. SQL filters requested class IDs,
  weekdays, schedule mode, and usable-teacher rows before copying data. It
  applies per-class and total limit-plus-one sentinels, returns owned values
  in request order, deduplicates teacher copies, and uses validated decimal
  integer IDs so the maximum 4,096-class request stays within older SQLite
  bind limits. Missing-info, unassigned, and orphan-teacher classes are
  omitted; roster-read failures preserve the count-zero fallback.
- `ClassMngr` and the new Platform adapter test target built. The app-less
  `ClassMngrNextApplicationSubPrepPrintSourceQueryTests` and offscreen
  `ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`
  focused CTests passed 2/2; `git diff --check` passed. The test covers
  request order, mode/day/class scope, empty scopes, shared teachers,
  missing/unassigned/orphan teachers, roster failure, SQL failure, canonical
  ID aliases, per-class and aggregate overflow, the maximum class scope, and
  owning-value behavior.
- This remains a read-adapter slice. The Sub Prep page/PDF wiring, persistence
  adapter, roster/package migration, output parity, SQL batching, and Release
  memory acceptance remain open; no memory improvement is claimed. The next
  entry point after the selected-class details read is the scoped
  schedule-summary persistence read. Phase 2 remains open; nothing has been
  pushed.
- Commit `9f648b3e` adds
  `ApplicationServicesSubPrepClassDetailsPort` and its session-backed
  ClassService/repository read. It returns owning selected-class details with
  separate bounded room, WiFi, internet, Zoom, and projection values plus
  notes and preferred teacher display name. The SQL reads one class, does not
  touch schedule tables, treats absent class-info as blank details, and maps
  absent classes to `NotFound`; stale and unassigned teachers preserve empty
  teacher values. There is no `DataService` fallback.
- Windows x64 Debug builds succeeded for ClassMngr and the focused ClassSummary,
  SubPrepClassDetailsQuery, and SubPrepPrintSource Platform test targets;
  focused CTest passed 3/3. Final configure validated ownership for 855
  handwritten files and `git diff --check` passed. This read is not wired to
  the page; parity and Release memory evidence remain open. Next entry point:
  implement the scoped schedule-summary persistence read behind its existing
  Application contract. Phase 2 remains open; nothing has been pushed.
- Commit `dd429838` adds the scoped schedule-summary persistence read. The
  ClassService/Repository path filters by class IDs, weekdays, and selected
  schedule mode, returns bounded owning summary values, and gets roster counts
  in one scoped aggregate. The Platform adapter validates and projects the
  values into the existing class-summary contract, preserving request order
  and teacher deduplication. Empty scopes avoid reads; the intensive-mode test
  passes with the regular schedule table removed; and roster-query failure
  keeps the zero-count fallback. The read uses the active session and has no
  DataService fallback.
- Windows x64 Debug build succeeded for
  `ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`.
  Focused CTest passed 3/3 for
  `ClassMngrNextApplicationClassSummaryTests`,
  `ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests`, and
  `ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`.
  CMake validated 857 handwritten files; `git diff --check` passed. Nothing
  was pushed.

### Phase 2 Sub Prep model-backed navigation — 2026-09-23

- The live Sub Prep class-information view now uses the Application
  schedule-summary and selected-class details queries through their Platform
  ports. `SubPrepClassInformationListModel` supplies compact grade/level class
  rows to one `QListView`; one details card is updated from
  `SubPrepClassInformationState`.
- Schedule display-mode changes refresh the projection in place. Refresh keeps
  the selected class only while it remains in scope. Focused tests cover grade
  navigation, selection retention, invalidation when the selected class leaves
  the schedule, mode refresh, and bounded list/detail widgets.
- Windows x64 Debug Ninja built `ClassMngr`, the new list-model test target,
  `ClassMngrSubPrepPageTests`, and `ClassMngrStartupPerformanceTests`. Focused
  CTest passed the list-model and page suites 2/2. CMake validated 860
  handwritten sources; `git diff --check` passed. The 96-class packaged
  Release route was not run. The Visual Studio solution build hit a sandbox
  `FileTracker` access denial before compilation; the equivalent Ninja build
  succeeded from the Visual Studio developer environment.
- Handoff: next implement Work Package E's explicit selected-details release
  when the page deactivates, with a return-to-page lifecycle test. Output/
  package/PDF migration and parity, the 96-class Release memory gate, and the
  wider Phase 2 exit gate remain open. Keep the Linux Phase 0 follow-up
  separate.

### Phase 2 Sub Prep page-leave lifecycle — 2026-09-23

- `SubPrepPage::releaseFeatureResources()` now clears the selection state,
  compact summary projection, and selected details when `PageManager` calls
  `BasePage::deactivate()`. It marks the page stale so re-entry reloads the
  current schedule scope and selected details.
- Windows x64 Debug Ninja built `ClassMngr` and `ClassMngrSubPrepPageTests`;
  focused CTest passed 1/1. The test checks that list/details state is cleared
  on deactivation and restored with a fresh detail read on activation.
- Handoff: Work Package F connects `SubPrepPrintSourceQuery` and its
  session-backed Platform adapter to package generation. Output/PDF parity,
  cancellation/error cleanup, and the 96-class packaged Release memory gate
  remain open. Keep the Linux Phase 0 follow-up separate.

### Phase 2 Sub Prep information-sheet print-source integration - 2026-09-24

- The accepted print dialog's selected class IDs, weekdays, and current
  schedule mode now feed the operation-scoped print-source query. The result
  is mapped into the existing renderer model; the query-owned source is
  released when mapping returns.
- Added the preferred-name inputs needed to preserve the legacy teacher label
  fallback order, plus a focused mapper test for teacher facts, class details,
  schedule mode, and renderer-incompatible IDs. Removed the old information-
  sheet loader that read every class, class-info record, teacher, and roster
  count.
- Windows x64 Debug Ninja built `ClassMngr`, the page, mapper, Application
  query, Platform adapter, PDF, and package test targets. Focused CTest passed
  6/6: mapper, page, PDF, package, print-source query, and Platform adapter.
- This does not complete package migration: roster PDFs still load full
  legacy class/teacher/roster records, the renderer model is copied into its
  document value, full output parity is not established, and the 96-class
  Release memory gate remains open.
- Handoff: continue Work Package F with the remaining package/roster source
  and renderer-model lifetime boundaries, then complete parity and lifecycle
  regression evidence. Phase 2 remains open.

### Phase 2 Sub Prep renderer model lifetime - 2026-09-24

- `SubPrepDocumentModel::Document` now stores a const reference wrapper to the
  request's `classInformation` list. `saveSubPrepPdf()` keeps the request alive
  for the synchronous renderer call. The page moves the request into the
  package request, avoiding a second list copy there; the PDF test asserts the
  render document borrows the same list.
- Windows x64 Debug Ninja built `ClassMngr`, the Sub Prep PDF and package
  targets, and the page test target. CTest passed the page, PDF, and package
  suites 3/3.
- The request remains alive for the rest of package generation, so its
  information-sheet projection still overlaps roster rendering. The roster
  PDF read still uses legacy full class/teacher/roster values.
- Handoff: continue Work Package F with package-owned stage release and the
  scoped roster output read. Phase 2 and the Sub Prep Release memory gate
  remain open.

### Phase 2 Sub Prep package stage release - 2026-09-24

- `SubPrepPackageService::generate` now owns its request by value, and the
  page moves its package request into the call. `generateAt()` writes the
  information-sheet PDF before loading full roster records, then clears the
  Sub Prep document input before roster generation.
- Windows x64 Debug Ninja built `ClassMngr`, the package service test, and the
  Sub Prep page test targets. CTest passed package, page, and PDF suites 3/3.
- This shortens the main-sheet model lifetime and avoids a page/service copy.
  Roster PDFs still use legacy full class, teacher, and roster reads.
- Handoff: add the operation-scoped roster output read and parity coverage.
  Phase 2 and the 96-class Release memory gate remain open.

### Phase 2 Sub Prep roster-output Application contract - 2026-09-24

- Added the Qt-free `SubPrepRosterOutputSourceQuery` contract, scoped to
  selected class IDs, weekdays, schedule mode, and requested extra columns.
  It returns bounded owning class/teacher facts and roster values, and
  validates teacher links, meeting days, row shapes, and per-class and
  aggregate row/cell/text caps.
- CMake reconfiguration validated 865 handwritten source owners. Windows x64
  Debug built `ClassMngr` and the query test target; focused CTest passed 1/1.
  Coverage includes empty-scope no-read behavior, request/source validation,
  scope forwarding/order, maximum per-class dimensions, and aggregate limits.
- Handoff: implement the session-backed Platform read and enforce caps in the
  repository query before creating roster strings. Then replace package-service
  legacy reads and verify output parity. Phase 2 and the packaged 96-class
  Release memory gate remain open.

### Phase 2 Sub Prep bounded roster repository read - 2026-09-24

- Added `RosterRepository::loadRosterForOutput`, with DataService and
  RosterService forwarding. It selects only requested columns, checks the
  highest relevant row and row-by-column cell budget before allocating the
  row matrix, streams SQL results, and reads a bounded substring while checking
  the original cell byte length.
- `ClassMngr` and `ClassMngrDataServiceLifecycleTests` built on Windows x64
  Debug Ninja. The lifecycle suite passed 1/1, covering selected-column order,
  excluded values, row/cell/text caps, an oversized stored cell, and a sparse
  out-of-range row index. `git diff --check` passed.
- Handoff: F6 completes the bounded Platform read seam recorded below. F7 now
  wires this source into package generation and verifies renderer/package
  parity. The packaged Release memory gate and Phase 2 remain open.

### Phase 2 Sub Prep roster-output Platform read - 2026-09-24

- Added `ApplicationServicesSubPrepRosterOutputSourcePort`. It validates
  canonical class IDs before mapping to legacy IDs, reads only the requested
  class/day/mode scope, preserves unassigned classes for roster output, and
  shares teacher facts across the operation. It maps renderer-facing class
  and meeting facts, requests only English/Korean plus selected extra roster
  columns, and passes remaining row/cell/text budgets into the bounded roster
  service before converting cells to Application-owned strings.
- Added database coverage for request ordering, selected days and modes,
  unassigned teachers, roster projection, sparse-row limits, and oversized
  teacher output. `classInfosForScheduleScope` now has an explicit opt-in for
  including unassigned teachers; existing callers retain their previous
  default filter.
- CMake validated 867 handwritten source owners. Windows x64 Debug built
  `ClassMngr` and the new Platform target. Focused CTest passed 3/3 for the
  roster-output Platform adapter, the existing Sub Prep print-source adapter,
  and the roster-output Application query. `git diff --check` passed.
- Handoff: F7 wires the typed roster source into `SubPrepPackageService` and
  maps its bounded values into `RosterTemplatePrintService` inputs. Then verify
  output/package parity and cleanup behavior. The 96-class packaged Release
  memory gate and broader Phase 2 exit gate remain open; nothing has been
  pushed.

### Phase 2 Sub Prep package roster-source integration - 2026-09-24

- `SubPrepPackageService::Request` now carries typed selected class IDs and a
  borrowed roster-output read port. The service issues one scoped query for
  selected classes, weekdays, schedule mode, and requested extra columns,
  validates UTF-8 and canonical legacy IDs, then maps only the bounded source
  values into the existing roster renderer model. The UI owns the
  session-backed Platform adapter for the synchronous generation call; the
  package service has no direct `ApplicationServices` or `DataService` reads.
- Package tests cover selected scope and mode, daily and per-class output,
  selected extra columns, and read/mapping errors that must not commit a
  partial package. Windows x64 Debug built `ClassMngr` and package/page tests.
  Focused CTest passed 5/5 for package, page, PDF, Application query, and
  Platform source suites; `git diff --check` passed.
- Handoff: compare generated output against retained references and finish
  cancellation/cleanup parity. The packaged 96-class Release memory gate and
  broader Phase 2 exit work remain open. Nothing has been pushed.

### Phase 2 Sub Prep output-reference parity - 2026-09-24

- `largePackageGeneratesOutputReferenceWhenConfigured` now compares both
  generated PDFs with the committed 96-class references under
  `docs/qt-rewrite/visual-baseline/release/sub-prep-output/reference/`. It
  checks page counts, each page's point dimensions, and extracted text. On
  Windows it also compares every page's 150-DPI render exactly. The Sub Prep
  PDF matches at 19 pages and the Daily roster PDF at 16 pages; their Windows
  page rasters match exactly.
- The print-only cancellation test confirms no package directory is committed
  and temporary PDFs are removed. Roster read and mapping failures also leave
  no staging folder. Windows x64 Debug built the package test target; focused
  CTest passed 5/5 for package, page, PDF, Application query, and Platform
  source suites; `git diff --check` passed.
- Handoff: run the clean packaged Windows x64 Release 96-class Sub Prep route
  against the Phase 9 memory budget. Full UI visual-state parity and the wider
  Phase 2 exit work remain open. Nothing has been pushed.

### Phase 2 Sub Prep packaged Release measurement - 2026-09-24

- The Sub Prep output-boundary test now records the working set at one and
  five seconds after the workflow settles. The Windows x64 Debug startup test
  target built, and `capturesLargeSubPrepOutputBoundaryWhenConfigured` passed
  while driving the packaged Release application.
- Route-scoped validation passed for `output-sub-prep`: two PDFs, 17 total
  pages, 139,650 PDF bytes, normal exit, and no timeout. This was a single
  selected route, so the aggregate Phase 0 exit gate correctly remains
  incomplete. The report is under
  `%TEMP%\ClassMngr-Phase2\f9-subprep-output-5s-verified-20260924`.
- The measured peak was 315,740,160 bytes working set and 351,821,824 bytes
  private usage. Working set was 306,466,816 bytes at the one-second
  checkpoint and 306,470,912 bytes at five seconds. The route stayed below
  the temporary 512 MiB diagnostic ceiling and improved over the retained
  legacy peak of 498,176,000 working-set bytes and 480,948,224 private bytes.
  It remains above the final 250 MiB settled-memory target. The validator
  recorded 25 samples at or above 250 MiB and none at or above 512 MiB.
- Handoff: continue Phase 2 with the broader typed calendar UI/page migration.
  Generic settings persistence, the remaining feature-service migrations,
  and broader document-service migration remain open. Nothing was pushed.


### Phase 2 typed calendar import batch save - 2026-09-24

- Added a Qt-free ordered import-save request over typed event-save values.
  It validates every create request, rejects event IDs, accepts a valid empty
  batch for duplicate-only imports, and caps accepted import batches at 4,096.
- `CalendarEventImportService` now maps only the duplicate planner's accepted
  candidates into that request. The Platform adapter performs one
  `CalendarService::saveEvents()` call, preserving the legacy batch
  transaction and returning typed IDs in input order. Import metrics,
  skipped counts, failure messages, and completion signals remain connected to
  that result.
- Windows x64 Debug built `ClassMngr` and the application, Platform, and
  parser test targets. CMake validated 869 explicit source owners. Focused
  CTest passed 3/3: `ClassMngrNextApplicationCalendarEventTests`,
  `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and
  `ClassMngrCalendarImportTests`. Database coverage verifies order, all-day
  and unknown-time mapping, empty-batch behavior, invalid-input preflight, and
  rollback when the second insert fails.
- Handoff: continue the broader typed calendar UI/page migration. Calendar
  workbook parsing and campus-directory lookup remain legacy boundaries;
  generic settings, other feature-service migrations, and the wider document
  migration remain open. Nothing was pushed.


### Phase 2 typed calendar reset mutation - 2026-09-24

- Added the Qt-free `CalendarEventDeleteAllPort` and a Platform adapter that
  maps availability and delete failures to structured results. The preferences
  panel no longer stores `CalendarService`; it checks availability before
  showing the existing destructive confirmation and preserves its warning,
  success status, and refresh signal behavior.
- Windows x64 Debug built `ClassMngr` and the Application and Platform calendar
  event test targets. CMake validated 871 explicit source owners. Focused CTest
  passed 2/2; database cases cover successful reset, unavailable service, and
  a trigger-injected delete failure. `git diff --check` passed.
- Handoff: continue Phase 2 with the calendar import signature read and broader
  typed page migration. Generic settings persistence, remaining feature-service
  migrations, wider document migration, the 250 MiB settled-memory target, and
  the separate Linux Phase 0 follow-up remain open. Nothing was pushed.


### Phase 2 typed calendar availability boundary - 2026-09-24

- `ApplicationServicesCalendarEventPort` now exposes a guarded availability
  query. Calendar import start and dialog opening use it instead of retaining
  or reading a raw `CalendarService*`; existing unavailable behavior remains
  intact. A Platform contract test checks both closed and open workspace
  states.
- Windows x64 Debug built `ClassMngr`, `ClassMngrCalendarImportTests`, and the
  Platform calendar event suite. Focused CTest passed 2/2, and a source search
  found no `calendarService()` getter calls in `src/features/calendar/`.
  `git diff --check` passed.
- Handoff: continue Phase 2 with the broader calendar UI/value migration and
  remaining feature-service calls. Generic settings persistence, wider
  document migration, the 250 MiB settled-memory target, and the separate
  Linux Phase 0 follow-up remain open. Nothing was pushed.


### Phase 2 typed calendar edit-draft flow - 2026-09-24

- Calendar day activation creates a typed draft, and event activation passes
  the typed summary directly into the dialog draft. The page no longer maps
  through the legacy `CalendarEvent` record before editing; repeat/delete/save
  decisions and requests now read from that draft. Removed the unused legacy
  upcoming-event filter overloads.
- Windows x64 Debug built `ClassMngr`, `ClassMngrDialogShellTests`, and the
  Platform calendar event suite. Focused CTest passed 2/2. The dialog tests
  preserve default, timed, all-day, and unconfirmed-time interactions;
  `git diff --check` passed.
- Handoff: continue the remaining calendar UI/value migration, then generic
  settings persistence, remaining feature-service migrations, and broader
  document migration. The 250 MiB settled-memory target and Linux Phase 0
  follow-up remain open. Nothing was pushed.


### Phase 2 calendar display-preference boundary - 2026-09-24

- `CalendarPreferencesPanel` now uses its existing typed display-preferences
  port through `ApplicationServices*` and no longer stores a `SettingsService*`
  for this preference pair. Load defaults and unavailable-save no-op behavior
  remain the same; unrelated keys and atomic save behavior stay in the adapter.
- Windows x64 Debug built `ClassMngr` and the Platform display-preferences test
  target. Focused CTest passed 1/1, including open-service pointer round-trip,
  null/unavailable defaults, and save behavior. `git diff --check` passed.
- Handoff: continue the remaining calendar and generic settings migrations,
  then remaining feature-service and broader document migrations. The 250 MiB
  settled-memory target and Linux Phase 0 follow-up remain open. Nothing was
  pushed.


### Phase 2 academic calendar preference-port injection - 2026-09-24

- AcademicCalendarProvider now owns Application schedule and first-day
  preference ports instead of SettingsService. CalendarPage and
  evaluation-default selection construct the Platform adapters and pass the
  ports in. Platform schedule, first-day, and display-preference adapters no
  longer expose SettingsService-pointer constructors.
- Windows x64 Debug built ClassMngr, ClassMngrAcademicCalendarTests, and the
  schedule, first-day, and display-preference Platform suites. Focused CTest
  passed 4/4; the provider source search found no SettingsService reference,
  and git diff --check passed.
- Handoff: continue the remaining calendar upcoming-events preference callers,
  then generic settings, feature-service, and document migrations. The 250 MiB
  settled-memory target and Linux Phase 0 follow-up remain open. Nothing was
  pushed.

### Phase 2 calendar event-type color preference boundary - 2026-09-24

- Calendar event-type color reads and writes now construct the Platform
  preference adapter from ApplicationServices*. The feature no longer gates
  these operations with or passes a raw SettingsService pointer; invalid or
  unavailable stored values retain the existing default-color fallback and
  unavailable saves remain no-ops.
- Windows x64 Debug built ClassMngr and the event-type color preference test
  target. Focused CTest passed 1/1, including ApplicationServices pointer
  round-trip and null-service fallback. git diff --check passed.
- Handoff: continue the remaining upcoming-events preferences and calendar
  service boundaries, then generic settings, feature-service, and document
  migrations. The 250 MiB settled-memory target and Linux Phase 0 follow-up
  remain open. Nothing was pushed.


### Phase 2 current-campus preference availability - 2026-09-24

- CurrentCampusPreferencesPort now reports availability through a Qt-free
  Application contract. The Platform adapter supports nullable
  ApplicationServices composition; CalendarPage uses the port to gate its
  preference reads and campus projection. The raw openSettingsService helper
  and SettingsService reference were removed from the calendar feature.
- Independent Windows x64 Debug build of ClassMngr and the current-campus
  preference test target passed; focused CTest passed 1/1, covering available,
  unavailable, null ApplicationServices, and null SettingsService cases.
  git diff --check passed. No dedicated CalendarPage behavior target exists;
  the Tester accepted the port tests plus prior-code/guard comparison for
  this refactor. No code changed after the passing build/test.
- Handoff: continue the remaining generic settings, feature-service, calendar
  import workbook/campus lookup, and document migrations using the paired
  Phase 2 inventory. The 250 MiB packaged Release gate belongs to Phase 9;
  the Phase 2 plan's app-less and forbidden-dependency exit checks remain.
  Nothing was pushed.


### Phase 2 calendar import signature-query boundary - 2026-09-24

- Added a Qt-free Application request/result and availability contract for
  calendar-import signature lookup, with a dedicated Platform adapter over
  the legacy calendar service. The import workflow uses this interface; the
  old concrete `ApplicationServicesCalendarEventPort` signature method was
  removed. The Platform dependency guard remains Application plus Qt6::Core.
- Independent Windows x64 Debug verification built `ClassMngr`, the
  Application contract target, and the Platform adapter target. Focused CTest
  passed 2/2. Coverage preserves six-field QString/UTF-16 identity, order,
  unavailable/invalid/read failures, and 4,097 rows beyond the general
  projection cap. Configure validated 874 source owners; `git diff --check`
  passed. The initial plain-shell MSVC build lacked developer include paths;
  retrying inside the x64 developer environment passed. No product defect was
  found.
- Handoff: next route the Schedule and Sub Prep personal-display-name callers
  through the existing `ApplicationServices` adapter entry point, preserving
  null-service defaults, caller trimming, and read timing. Preserve Sub Prep's
  baseline preference-write ordering before the later package `mkpath` attempt.
  My Information and Initial Setup still use the adapter's
  `SettingsService*` constructor; workbook parsing/campus lookup, generic
  settings persistence, other feature services, and broader document
  migration remain open. The 250 MiB packaged Release gate belongs to Phase 9;
  nothing was pushed.


### Phase 2 personal display-name caller cutover - 2026-09-24

- Schedule output/import and Sub Prep print-dialog now construct the existing
  typed personal-display-name adapter from `ApplicationServices&`; these
  callers no longer extract `SettingsService` for `myInfo/name`. Schedule
  output reads only after dialog acceptance and preserves stored whitespace;
  Schedule import and Sub Prep trim at their existing UI boundaries. Null and
  unavailable service behavior remains empty/default or successful no-op.
- Added ScheduleWidget regression coverage for a name changed on acceptance,
  exact whitespace in the print request, unavailable settings, and null
  services. Independent Windows x64 Debug verification built `ClassMngr` and
  the ScheduleWidget, ScheduleImportDialog, and SubPrepPage targets; CTest
  passed 3/3 and ScheduleWidget passed 19/19. `git diff --check HEAD` passed.
- Sub Prep keeps its baseline event order: after folder selection and any
  replacement confirmation, it writes the name before accepting the dialog;
  package generation later attempts `QDir::mkpath`. A later filesystem
  failure does not roll back that preference write. The paired Explorer's
  earlier “after folder creation” description was inaccurate.
- Handoff: migrate the My Information and Initial Setup display-name reads to
  the existing ApplicationServices adapter while preserving their availability
  guards and aggregate personal-details save path; then remove the
  `SettingsService*` adapter constructor after its final callers and test are
  migrated. Generic settings, other feature-service, workbook/campus, and
  broader document boundaries remain open. The 250 MiB gate belongs to Phase
  9; nothing was pushed.


### Phase 2 personal display-name adapter constructor removal - 2026-09-24

- My Information and Initial Setup now create
  `ApplicationServicesPersonalDisplayNamePreferencesPort` from
  `ApplicationServices&`. The adapter's `SettingsService*` constructor and its
  constructor-only test were removed. Existing availability guards, UTF-8 and
  whitespace handling, Setup's fill-only-if-blank condition, and aggregate
  personal-details saves remain intact.
- Independent verification configured and built a fresh isolated Ninja/MSVC
  x64 tree in `build/p2-f20-independent` (322 build steps), including both
  changed production translation units and the adapter test. Initial Setup
  and adapter CTest targets passed. In `ClassMngrMyWorkspacePageTests`, the
  F20 display-name, unavailable-storage, aggregate-save, and rollback cases
  passed; three PageManager cases failed before F20 code ran because the
  required `documents` and `campuses` resource packs were unavailable. No
  baseline checkout was run. Source review found no F20 defect and
  `git diff --check` passed. There is no direct regression test that
  pre-populates the Setup name and verifies reinitialization leaves it intact;
  the existing fill-only-if-blank condition remains in place.
- Handoff: F21 removes the unreferenced `ClassNavigationPreferences` header
  and implementation, production source entry, Classes Page test source entry,
  and unused includes. Keep the active typed Application contracts and
  Platform adapters. Then continue generic settings, remaining feature
  services, calendar workbook/campus lookup, and document boundaries until the
  Phase 2 exit gate is satisfied. The Phase 9 packaged Release memory gate is
  separate. Nothing was pushed; preserve the separately staged
  `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md` user change.


### Phase 2 ClassNavigationPreferences cleanup - 2026-09-24

- Removed the unused `ClassNavigationPreferences` header and implementation,
  production source entry, Classes Page test source entry, and stale includes.
  `speaking_eval_page_p.h` now includes `class_tab_navigation_model.h`
  directly for the still-used `ClassTabNavigation`. The active Qt-free
  Application preference contracts and Platform adapters remain unchanged.
- Independent verification used fresh `build/p2-f21-independent` Ninja/MSVC
  x64 configuration and build. CMake validated 872 handwritten source owners.
  `ClassMngr`, `ClassMngrClassesPageTests`, `ClassMngrClassTabNavigationModelTests`,
  `ClassMngrEvaluationDefaultSelectionTests`, and five typed preference-port
  suites built. Focused CTest passed 8/8. No deleted API or filename references
  remain in `src`, `tests`, `cmake`, `CMakeLists.txt`, or `compile_commands.json`;
  `git diff --check` passed. No material verification gaps remain.
- Handoff: F22 moves the calendar importer's campus-code directory lookup
  behind a Qt-free Application query port and Platform adapter. Preserve the
  existing campus-name ordering, UTF-8/code values, trimming, blank removal,
  duplicate removal, and no-codes fallback for missing or empty directories.
  Leave workbook decoding, CalendarPage's separate campus lookup, generic
  settings, other feature services, and document migration for later slices.
  The formal Phase 2 exit gate remains open. Nothing was pushed; preserve the
  separately staged `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 calendar-import campus-code query - 2026-09-24

- Added the Qt-free `CalendarEventImportCampusCodeQueryPort`, returning an
  owning `std::vector<std::string>`, and the Platform
  `CalendarEventImportCampusCodeQueryAdapter` over
  `ResourcePaths::Campuses::directory()` and `CampusJsonRepository`.
  `CalendarEventImportService` now consumes the port and converts its UTF-8
  values to QString at the feature boundary; direct repository/resource-path
  lookup is removed from the importer. Workbook decoding, parser behavior, and
  CalendarPage's separate campus lookup remain unchanged.
- Preserved repository campus-name ordering, whitespace trimming, blank-code
  removal, first exact duplicate retention, and default/malformed/unreadable
  record behavior. Adapter fixtures cover ordering, Korean UTF-8, duplicates,
  blanks, default and malformed records, and empty/missing directories.
- Independent verification used a fresh Ninja/MSVC x64 Debug configure and a
  356-step build of ClassMngr and both focused test targets. CMake validated
  875 handwritten source owners; focused CTest passed 2/2 for the parser and
  campus-code adapter suites. The importer source search, dependency
  assertions, and `git diff --check` passed. No material gaps remain.
- Handoff: F23 gives CalendarPage a separate campus metadata read port and
  Platform adapter. Preserve its current-campus availability guard, repository
  ordering, ID/name/code aliases, matching, blank removal, and duplicate
  behavior. Keep that query independent from this importer-specific code list.
  Generic settings, broader feature-service, workbook decoder, and document
  migrations remain open until the Phase 2 exit gate is met. Nothing was
  pushed; preserve the separately staged
  `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 PersonalSignatureImagePort caller cutover - 2026-09-24

- Kept the reference constructor and added a nullable `ApplicationServices*`
  constructor to `ApplicationServicesPersonalSignatureImagePort`; removed the
  `SettingsService*` constructor. The five reads in Initial Setup (two), My
  Information (one), and Speaking Eval (two) now pass `setup->services()` or
  `m_services`. No unrelated settings reads/writes changed.
- Preserved the `myInfo/signatureImage` key, Base64 conversion, one-time
  `SignatureImage::prepareForEmbedding`, read-only behavior, empty results for
  missing/invalid/unavailable/corrupt values, and existing caller guards. The
  null-service adapter case now explicitly passes a null `ApplicationServices*`.
- Executor build succeeded for ClassMngr and the adapter, Initial Setup, and
  MyWorkspace targets; its focused CTest passed 3/3. Independent fresh
  Ninja/MSVC x64 configure validated 878 owners and built all requested
  targets. Independent CTest passed 2/3: adapter and Initial Setup passed;
  MyWorkspace failed three top-level page cases because `documents` and
  `campuses` packs were unavailable. The tester ran all 18 MyWorkspace
  functions individually: the F24 signature-image preview, missing/corrupt/
  unavailable image, display-name, and aggregate-save cases passed, while only
  the same three resource-dependent cases failed. `git diff --check HEAD`
  passed. Speaking Eval compiled through ClassMngr; it has no dedicated page
  integration test.
- Handoff: F25 removes the custom-color adapter's remaining
  `SettingsService*` caller path across seven picker call sites in five UI
  files. Preserve the `custom_colors` key, 16-slot palette, legacy payloads,
  defaults, unrelated settings, and load-before/save-after-dialog behavior,
  including cancel. Then continue workbook decoding, generic settings,
  remaining feature-service, and document migrations. Phase 2 remains open.
  Nothing was pushed; preserve the separately staged
  `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 Sub Prep typed settings gate removal - 2026-09-24

- Removed `openSettingsService` from the Sub Prep page helper and its four
  preference paths. Saved-content and personal-Zoom reads now construct their
  existing typed ports from `ApplicationServices&`; current-campus reads use
  the nullable ApplicationServices port and check availability before loading
  or mutating campus state. `saveSubPrepInternal()` returns before stopping
  autosave or restoring grading defaults if preferences are unavailable.
- Added page coverage that preserves saved-content, Zoom, and campus sentinels
  during unavailable loads. The unavailable save case preserves field values,
  dirty state, active autosave timer, blank grading text, and stored settings.
  The test stub's database-open flag defaults to true, so the fixture explicitly
  disables it before the unavailable checks and does not close the fake service.
- Executor and independent fresh Ninja/MSVC x64 configure/builds each
  validated 878 handwritten source owners and built ClassMngr, the Sub Prep
  page tests, and all three preference adapter test targets. Both focused CTest
  runs passed 4/4; the independent repeat build returned no work, and
  `git diff --check HEAD` passed. No material verification gaps remain.
- Handoff: F27 routes the My Information campus chooser's directory lookup
  behind an Application query and Platform adapter. Preserve repository name
  ordering, trimmed display-name fallback, original IDs stored in combo data,
  saved ID/name matching, and correction writes. Personal Details atomic-save
  callers, workbook decoding, generic settings, other feature services, and
  broader document boundaries remain open. Phase 2 remains in progress.
  Nothing was pushed; preserve the separately staged
  `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 custom-color adapter caller cutover - 2026-09-24

- Kept the adapter's `ApplicationServices&` constructor, added a nullable
  `ApplicationServices*` constructor, and removed its `SettingsService*`
  constructor. Seven picker call sites across Schedule Editor, Testing
  Classes, Schedule Import Review, shared Class Details, and Initial Setup
  now pass ApplicationServices ownership.
- Preserved the `custom_colors` key, 16 palette slots, legacy payloads,
  defaults, unrelated settings, and load-before-dialog/save-after-dialog
  behavior including cancellation. No ColorUtils logic changed.
- Executor and independent fresh Ninja/MSVC x64 builds validated 878
  handwritten source owners and built ClassMngr plus all six focused adapter,
  ColorUtils, Setup, Testing Classes, Schedule Import Dialog, and Schedule
  Widget test targets. Both CTest runs passed 6/6; the independent repeat
  build had no work, source review found the seven expected migrated callers,
  and `git diff --check HEAD` passed.
- Handoff: F26 removes Sub Prep's raw settings-service gate around its
  existing typed saved-content, personal Zoom, and current-campus preferences.
  Add page-level unavailable-settings coverage for load no-mutation and save
  no-side-effect behavior. Preserve missing-versus-present grading defaults,
  Zoom fallback/migration, campus matching/fallback, and early return before
  timer or grading changes. Keep full campus detail and the all-dates calendar
  read separate; its typed projection has a 4,096-result cap versus the
  current 1–9999-year query. Workbook, generic settings, other feature-service,
  and document migrations remain open; Phase 2 remains in progress. Nothing
  was pushed; preserve the separately staged
  `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 CalendarPage campus metadata query - 2026-09-24

- Added a Qt-free `CalendarPageCampusDirectoryQueryPort` returning owning UTF-8
  campus IDs/names and an optional code, plus a Platform adapter over
  `CampusJsonRepository` with an injected fixture directory. CalendarPage no
  longer directly reads the campus repository or `ResourcePaths`; the F22
  importer-specific query is unchanged.
- Preserved the existing availability branch and read timing, repository
  ordering, ID/name/code alias order, case-insensitive ID/name matching,
  trimmed display-name fallback, whitespace-only campus codes, and final
  removal of empty aliases and exact duplicates. Adapter fixtures cover UTF-8,
  missing/empty/whitespace codes, blank/malformed/default records, and
  empty/missing directories.
- Executor and independent tester each configured fresh Ninja/MSVC x64 builds;
  CMake validated 878 handwritten owners. Both built ClassMngr,
  CalendarEventCache, and F22/F23 adapter targets. Focused CTest passed 3/3 for
  CalendarEventCache, F22 campus-code query, and F23 campus-directory query.
  `git diff --check` passed. There is no CalendarPage-specific behavior test;
  alias semantics were checked by source comparison.
- Handoff: F24 is the PersonalSignatureImagePort caller cutover to
  `ApplicationServices*` across Initial Setup, My Information, and Speaking
  Eval. Preserve existing availability guards, `myInfo/signatureImage`, Base64
  decoding, one-time embedding preparation, and empty-result behavior. Then
  continue custom-color, workbook decoding, generic settings, remaining
  feature-service, and document migrations. Phase 2 remains open. Nothing was
  pushed; preserve the separately staged
  `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 My Information campus chooser query - 2026-09-24

- Added the Qt-free `MyInfoCampusDirectoryQueryPort` and the Platform
  `MyInfoCampusDirectoryQueryAdapter` over `CampusJsonRepository`. The adapter
  returns owning UTF-8 IDs and display names, preserves repository ordering,
  applies the existing trimmed-name/trimmed-ID fallback, filters empty display
  names, and retains each original ID value. My Information no longer reads
  the campus repository or `ResourcePaths::Campuses` directly. Saved campus
  matching, combo selection, correction write, and later save remain unchanged.
- App-less and adapter tests cover owning metadata, UTF-8, repository order,
  raw ID values, trimmed fallback, malformed/default records, and empty or
  missing directories. The existing MyWorkspace test covers case-insensitive
  saved-ID selection and correction write. `CampusJsonCodec` normalizes a
  blank ID/name record to `campus`, so the adapter's empty-label filter cannot
  be reached through a repository fixture; the filter remains in place.
- Executor and independent fresh Ninja/MSVC x64 configures each validated 882
  handwritten source owners, built ClassMngr, MyWorkspace, and both new test
  targets, and passed focused CTest 3/3. The independent repeat build returned
  no work; `git diff --check HEAD` passed. Campus resource generation succeeded
  with no test limitation.
- Handoff: F28 routes Sub Prep's full campus details—office number, Wi-Fi name
  and password, and photocopier code—through a separate Application query and
  Platform adapter. Preserve repository ordering/omission, saved-ID/name
  matching, first-campus fallback, “N/A” detail display, and availability
  timing. Keep settings and the all-years calendar query out of scope. Personal
  Details atomic-save callers, workbook decoding, generic settings, other
  feature services, and broader document boundaries remain open. Phase 2
  remains in progress. Nothing was pushed; preserve the separately staged
  `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 Sub Prep campus detail query - 2026-09-24

- Added a Qt-free `SubPrepCampusDirectoryQueryPort` with owning UTF-8 values
  for campus ID, display name, office number, Wi-Fi name/password, and
  photocopier code. The Platform adapter reads `CampusJsonRepository` and
  accepts a fixture directory. Sub Prep's page/helper files no longer access
  `CampusJsonRepository`, `ResourcePaths::Campuses`, or `CampusInfo` directly.
- Preserved repository order and omission, saved case-insensitive ID/name
  matching, first-campus fallback, raw ID selection, current-campus
  availability timing before lookup/state mutation, and `N/A` for empty office
  details. Application and Platform adapter tests cover owning metadata,
  Unicode, order, trimmed-name/ID fallback, malformed/default records, and
  empty/missing directories. Page coverage checks selected details and empty
  detail display.
- Executor and independent fresh Ninja/MSVC x64 configures validated 886
  handwritten source owners. Both built ClassMngr, the Sub Prep page suite,
  and the new Application/Platform suites. Executor focused CTest passed 3/3;
  independent focused CTest passed 6/6 including the three existing Sub Prep
  preference adapters. The independent repeat build returned no work. Campus
  resource generation passed; no resource-pack test limitation remains.
- Handoff: F29 is the Personal Details atomic-save caller cutover. Remove the
  adapter's `SettingsService*` constructor and update Initial Setup and My
  Information to pass `ApplicationServices`, retaining the My Information
  early return before autosave cancellation or field normalization and the
  adapter's single atomic `saveAll`. Workbook decoding, generic settings,
  other feature services, and broader document boundaries remain open; Phase
  2's formal exit gate is not met. Nothing was pushed; preserve the separately
  staged `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 Personal Details atomic-save caller cutover - 2026-09-24

- `ApplicationServicesPersonalDetailsSavePort` retains the reference
  constructor, adds a nullable `ApplicationServices*` constructor, and removes
  the `SettingsService*` constructor. Initial Setup and My Information now
  pass their existing `ApplicationServices` owner.
- Preserved My Information's unavailable-settings return before autosave
  cancellation and field normalization, Initial Setup's warning on failure,
  one atomic `saveAll`, all nine keys, UTF-8 conversion, signature-image
  preparation, mode/font normalization, and rollback. Added null services
  pointer coverage and an unavailable MyWorkspace save case that preserves
  whitespace Zoom fields and dirty state.
- Executor and independent fresh Ninja/MSVC x64 configures each validated 886
  handwritten source owners. Both built ClassMngr and the save adapter,
  InitialSetupWizard, and MyWorkspace targets; both focused CTest runs passed
  3/3. The independent run confirmed the two production callsites and absence
  of a `SettingsService*` constructor. `git diff --check` passed; no resource
  limitation remains.
- Handoff: F30 moves the separate read-only Personal Signature Preferences
  adapter and its two callers to `ApplicationServices`. Preserve defaults,
  mode normalization, UTF-8 signature text, availability guards, and no-write
  behavior. Workbook decoding, generic settings, other feature services, and
  broader document boundaries remain open; Phase 2's formal exit gate is not
  met. Nothing was pushed; preserve the separately staged
  `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 Personal Signature Preferences caller cutover - 2026-09-24

- `ApplicationServicesPersonalSignaturePreferencesPort` retains its reference
  constructor, adds a nullable `ApplicationServices*` constructor, and removes
  its `SettingsService*` constructor. My Information and Initial Setup now
  pass their existing `ApplicationServices` owner.
- Preserved the adapter's exact read-only keys, defaults, UTF-8 typed text,
  mode/font normalization, unavailable failure behavior, and each caller's
  existing availability guard. No writes were introduced. The null-services
  test now covers the nullable `ApplicationServices*` path.
- Executor and independent fresh Ninja/MSVC x64 configures each validated 886
  source owners. Both built ClassMngr and the adapter, InitialSetupWizard, and
  MyWorkspace suites; both focused CTest runs passed 3/3. Source review
  confirmed both callers use ApplicationServices, the adapter checks
  availability before reading, and the read performs no writes. Diff check
  passed with no resource limitation.
- Handoff: F31 removes the raw-service constructor from the current-campus
  preferences adapter and migrates the two My Information calls (read and
  correction write) plus Initial Setup's read to nullable `ApplicationServices*`.
  Preserve `myInfo/campus`, string conversion, unavailable read/write/error
  behavior, and My Information's matching/correction timing. Workbook
  decoding, generic settings, other feature services, and broader document
  boundaries remain open; Phase 2's formal exit gate is not met. Nothing was
  pushed; preserve the separately staged
  `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 current-campus preferences caller cutover - 2026-09-24

- Removed the `SettingsService*` constructor from
  `ApplicationServicesCurrentCampusPreferencesPort`. My Information's campus
  read and correction write, and Initial Setup's campus read, now pass their
  existing `ApplicationServices` owner.
- Preserved the `myInfo/campus` key, UTF-8 and `QVariant::toString()`
  conversion, unavailable empty-read/no-op-write behavior, technical error on
  failed writes, and My Information's broader availability guard and campus
  correction timing. Adapter coverage exercises null/unavailable services,
  round trips, unrelated settings, conversion, and write failure.
- Executor and independent fresh Ninja/MSVC x64 configures each validated 886
  handwritten owners. Both built ClassMngr and the adapter,
  InitialSetupWizard, and MyWorkspace suites; both focused CTest runs passed
  3/3. The independent repeat build had no work. Diff check and the three-call
  source scan passed; no resource limitation remained.
- Handoff: F32 removes the raw-service constructor from
  `ApplicationServicesSubPrepPersonalZoomPreferencesPort` and updates its My
  Information and Initial Setup callers to use `ApplicationServices*`.
  Preserve primary-over-legacy precedence, fallback and best-effort migration
  only when primary values are absent, and keep returning legacy values when
  migration writes fail. Preserve UTF-8, defaults, unavailable handling, and
  page display behavior. Workbook decoding, generic settings, other feature
  services, and broader document boundaries remain open; Phase 2's formal exit
  gate is not met. Nothing was pushed; preserve the separately staged
  `plans/qt-rewrite-heavy-route-plan/00-Start-Here.md`.


### Phase 2 Personal Zoom preferences caller cutover - 2026-09-24

- `ApplicationServicesSubPrepPersonalZoomPreferencesPort` retains its
  `ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
  constructor, and removes its `SettingsService*` constructor. My Information
  and Initial Setup now pass their existing `ApplicationServices` owner.
- Preserved primary `myInfo/zoom*` precedence and legacy
  `subPrep/personalZoom*` fallback/migration only when primary values are
  absent. Migration writes remain best-effort and migration failure still
  returns the legacy value. UTF-8, defaults, unavailable behavior, caller
  guards, and page display behavior remain unchanged. Adapter null-service
  coverage now uses the nullable ApplicationServices path.
- Executor built ClassMngr, the adapter, MyWorkspace, and InitialSetupWizard;
  all three focused CTest suites passed. Independent fresh Ninja/MSVC x64
  configure validated 886 handwritten owners, built all four targets, and
  passed focused CTest 3/3. The adapter suite covers primary precedence,
  legacy fallback/migration, UTF-8, defaults, unavailable/null services, and
  legacy-value return when migration write fails. Source scan and diff check
  passed with no resource limitation.
- Handoff: F33 routes the remaining My Information and Initial Setup
  settings-availability checks through the existing
  `ApplicationServicesCurrentCampusPreferencesPort::isAvailable()` contract.
  Its Application contract explicitly reports persistence availability, so no
  generic contract or adapter is needed. Preserve unavailable My Information
  load/save early returns, especially return before autosave cancellation and
  Zoom-field normalization, and Initial Setup's unavailable
  initialization/validation behavior. Add My Information load coverage for
  unavailable settings. Workbook decoding, generic settings, other feature
  services, and broader document boundaries remain open; Phase 2's formal exit
  gate is not met. Nothing was pushed; keep user-owned commit `f5af92df` and its
  Start Here content unchanged.


### Phase 2 Class Notes save boundary - 2026-09-24

- Added the Qt-free `ClassNotesSavePort` request/result contract and the
  `ApplicationServicesClassNotesSavePort` Platform adapter. The page's one
  Class Notes save call now uses this boundary; its other legacy reads remain
  unchanged. The contract uses `std::u16string` so the existing 10,000
  UTF-16-code-unit validation rule is preserved rather than replaced with a
  UTF-8 byte limit.
- Preserved trimming, two-field persistence, class identity, unavailable and
  write-failure behavior, manual warning, autosave timing, dirty-state rules,
  and discard/reload. Contract, adapter, page, Classes page, and DataService
  lifecycle coverage all passed. A page integration test exercises the default
  adapter through real persistence and verifies both fields and clean state.
- Independent fresh Ninja/MSVC x64 configure validated 891 handwritten source
  owners. The executable and all five focused test targets built; exact suites
  `ClassMngrNextApplicationClassNotesSavePortTests`,
  `ClassMngrNextPlatformApplicationServicesClassNotesSavePortTests`,
  `ClassMngrNextFeatureClassNotesPageTests`, `ClassMngrClassesPageTests`, and
  `ClassMngrDataServiceLifecycleTests` passed 5/5. The Qt-free contract,
  UTF-16 boundary, source callsite, and diff checks passed.
- Handoff: F35 candidate is Sub Prep's all-years calendar read. Preserve the
  years 0001–9999 query, the empty-list fallback when unavailable or failed,
  and generation continuing. The existing generic calendar projection caps
  output at 4,096 events, so define a purpose-specific query or explicit
  capacity policy before using it. Phase 2 remains open; workbook decoding,
  generic settings, remaining feature services, broader document work, and
  the formal exit gate remain open. Nothing was pushed; keep Start Here and
  the user-owned wording commit unchanged.


### Phase 2 Sub Prep calendar interval query - 2026-09-24

- Replaced the page's generic all-years CalendarService read with a
  purpose-specific typed interval query. Per user direction, its bounds are
  January 1 of the current calendar year through December 31 of the following
  year, clamped at year 9999. One captured date supplies both the query year
  and dialog reference date.
- The query returns normalized Vacation/Holiday types and complete inclusive
  start/end intervals, using only the fields the dialog needs. It avoids the
  generic 4,096-event projection cap. Unavailable/read failures still produce
  empty calendar defaults and allow generation to continue. Dialog selection
  logic and its day-28/day-29, historical-prefix, holiday-bridge, and connected
  future-tail behavior remain covered.
- Independent fresh Ninja/MSVC x64 configure validated 894 handwritten source
  owners. The executable and focused targets built; repository, Application
  query, Platform calendar adapter, Sub Prep page, print-source mapper, PDF,
  package-service, and print-source port suites passed 8/8. Tests cover 5,000
  intervals, 4,097 adapter results, the two-year bounds, year-9999 clamp,
  full overlaps, production ApplicationServices-to-repository wiring, and
  failure fallback. Source scan and `git diff --check` passed. The exceptional
  conversion fallback was source-inspected but not fault-injected.
- Handoff: after committing F35, audit the current code against the Phase 2
  plan and formal exit gate, as requested. Phase 2 remains open; workbook
  decoding, generic settings, remaining feature services, broader document
  work, and the formal exit gate remain open. Nothing was pushed; preserve
  Start Here and the user-owned wording commit.


### Phase 2 plan and exit-gate audit - 2026-09-24

- Read-only audit was performed at committed HEAD `616545cebf1de73d9f4414bcfe75cb5efbd5ad2d`; no tests were rerun. Prior focused pass records are not fresh audit results.
- Gate 1: app-less tests and dependency declarations support behavior testing without MainWindow for implemented Domain/Application contracts. The Domain layer remains partial; it currently centers on typed IDs and Result/error contracts rather than all planned records and rules.
- Gate 2: partial/unverified against baseline fixtures. Current Next tests exercise validation, conflicts, import planning, and state transitions on constructed inputs, while legacy fixture tests remain separate.
- Gate 3: WorkspaceCoordinator boundary contracts and app-less tests cover create/open/close/save/save-as/export and snapshot preservation. The production FileController still obtains dirty-page approval from MainWindow and closes the current database before replacement; end-to-end preservation after a later create/open failure is not established.
- Gate 4: static source and target-boundary review found no direct DataService, MainWindow, PageManager, or widget-pointer dependency within `src/next`. Outer ApplicationServices Platform adapters bridge to legacy services; FileController remains MainWindow-aware. Treat the core scan and the production integration boundary as separate evidence.
- Phase 2 remains open. Workbook decoding, generic settings persistence, remaining feature-service migrations, broader calendar/UI migration, and broader document-service migration remain outstanding. Next: select the next bounded slice from these gate gaps. The legacy mapping's workspace row was also found stale and is being updated from verified source facts. Nothing was pushed; preserve Start Here and the user-owned wording commit.


### Phase 2 typed settings availability guards - 2026-09-24

- My Information now gates stored-settings load and save through
  `ApplicationServicesCurrentCampusPreferencesPort::isAvailable()`. The load
  guard precedes widget reads/mutations; the save guard precedes autosave
  cancellation and Zoom normalization. Initial Setup's initialization and
  validation checks use the same typed availability query. The raw
  `settingsService()` getter and My Information helper were removed.
- Added a My Information unavailable-load sentinel regression and an Initial
  Setup unavailable-validation regression that preserves the entered name and
  signature preview, stays on the page, and shows no warning. Existing
  unavailable initialization coverage remains.
- Executor and independent fresh Ninja/MSVC x64 configure/builds validated 886
  handwritten source owners. Both built ClassMngr, the current-campus adapter,
  MyWorkspace, and InitialSetupWizard. The three focused CTest suites passed
  3/3 in both runs. Source scans and `git diff --check` passed. The independent
  verifier initially found the validation assertion missing; the executor
  added the focused test and the same verifier reran all three suites.
- Historical F34 handoff superseded by the current Phase 2 continuation entry below.

### Phase 2 Calendar import planning parity - 2026-09-24

- F36 adds a required XLSX fixture and an integration test that drives the
  production CalendarEventImportService over loopback into a temporary
  database. It covers parser output, the typed signature query and import
  planner, and the typed batch-save adapter.
- Expected outcomes are four parsed events, one parser skip, repeated-signature
  deduplication before planning, one pre-existing Red Day match, three inserted
  events, two total skipped, preserved accepted order, and the exact final
  event set. Planner-level duplicate candidates remain covered by its separate
  application suite; the production parser prevents them from reaching this
  planner end-to-end.
- Independent fresh Ninja/MSVC x64 configure/build validated 895 handwritten
  source owners and passed all five suites: CalendarImportTests,
  CalendarEventImportPlanTests, CalendarEventImportSignatureQueryPortTests,
  ApplicationServicesCalendarEventPortTests, and CalendarEventImportParityTests.
  The test requires the fixture, binds an ephemeral server to LocalHost, and
  contains no skip path or external URL.
- F36 improves Calendar import planning parity evidence, but does not close
  Gate 2: Schedule Import matching/preview, conflict detection, and state
  transitions still need production-relevant parity coverage. Phase 2 remains
  open. The path-limited F36 commit includes the test, CMake registration,
  required fixture, and plan/deployment documentation. Next: select and start
  the next Schedule Import slice. Nothing was pushed.

### Phase 2 Schedule Import apply-time state contract - 2026-09-24

- F37 moves apply-time state validation into a typed standard-C++ Application
  contract and replaces the duplicate legacy validator. The repository
  converts snapshot identities and Qt day/time values at its edge; validation
  runs after structural plan validation and database snapshot reads, before
  write loops inside the existing transaction.
- Contract tests cover stale teacher/class targets, matching identities and
  room choices, unique exact-match skips, invalid times, overlap rejection and
  adjacency, plus Normal and Intensive schedule projections. Intensive update
  includes absent classes' current hours; replacement excludes them.
- A SQLite BEFORE UPDATE trigger sentinel attempts to distinguish early
  validation from later rollback. The new regression expects the typed stale
  class error; an attempted proposed teacher update would instead abort with a
  separate trigger message. Existing repository tests for duplicate targets,
  conflict rollback, intensive modes, exact skip, and unrelated snapshot
  preservation remain enabled.
- Executor and independent fresh Ninja/MSVC x64 configure/builds validated 895
  handwritten source owners. ClassMngrScheduleImportTests and
  ClassMngrNextApplicationScheduleImportStateValidationTests passed 2/2 in both
  runs. The optional external sample test still skips if
  CLASSMNGR_SCHEDULE_IMPORT_SAMPLE is unset.
- F37 improves production apply-time validation evidence but does not close
  Gate 2 or Phase 2. Schedule Import preview/matching and checked-in workbook
  parity remain open, as do workbook decoding and broader Domain work. Next:
  commit the F37 contract, repository cutover, CMake registrations, old
  validator removal, and tests; then select the precise fixture-backed review/
  preview slice. Nothing was pushed.

## Current Deployment Handoff - phase2_complete_continue_20260923

- F37 remains committed as `7049506fb81cce611e6a7f0ab635a3600c7f960d`.
  F38's typed Schedule Import matching/preview projection is implemented,
  independently verified, and committed as
  `bc6ac011504e0a499a8cdfd4b1533b49ea3f4bcb`.
- Production preview now calls the Qt-free Application projection. The
  repository constructs Qt-simplified, case-folded grade/level/room keys and
  maps typed results back to the legacy preview model; translations remain at
  the repository edge. The former `ScheduleImportMatcher` implementation and
  target ownership were removed.
- Required fixture `tests/fixtures/imports/schedule_review.xlsx` drives
  `ScheduleImportRepository::preview` against a temporary database. It asserts
  exact and weaker candidates `[43, 42]`, suggested class 43, exact/confident
  status, regular-only inventory of two, and initially absent IDs. The seed
  stores the exact class room as ` 416 ` while the workbook imports `416`.
  The fixture test has no skip or external URL path.
- App-less projection tests cover all seven ranking buckets, stable ties,
  no-match behavior, inventory, and Normal/Intensive fallback. The contract
  header is standard C++ and includes no Qt headers.
- Executor and independent fresh Windows x64 Ninja/MSVC configure/builds each
  validated 895 handwritten source owners. The Schedule Import and matching
  projection focused suites passed 2/2 in each run. QtTest totals were 24
  passed / 0 failed / 1 skipped for Schedule Import (only the existing
  optional external-workbook sample, unset) and 5 passed / 0 failed / 0
  skipped for matching projection. `git diff --check` passed. CMake emitted
  nonfatal warnings about long generated paths for unrelated test targets.
- Gate 1 remains partial because the broader Domain models are incomplete.
  Gate 2 now has production fixture-backed Schedule preview/matching evidence,
  but remains open for full validation/conflict/import/state parity and shared
  workbook decoding. Gate 3 remains partial because FileController replacement
  still uses MainWindow dirty approval and closes the old session before the
  new session succeeds. Gate 4's audited `src/next` dependency boundary
  remains satisfied. The formal Phase 2 exit gate is not met.
- Three independent solution reviews converged 2/3 on F39: move Schedule
  Import review-time overlap/conflict projection into Qt-free Application and
  share its semantics with F37 apply-time validation, while keeping translated
  warnings and dialog behavior at the UI edge. This slice is now independently
  verified and committed; see the F39 evidence below.
- Preserve the user's Sub Prep bound: the current calendar year and following
  calendar year at most. Preserve the pre-existing modified
  `cmake/sources.cmake`; it is outside F38. Nothing was pushed.

### F39 Schedule Import conflict projection

- F39 is committed as `3121d90c2db6af8e225048f016eec6f0843c1c18`
  (`Phase2 - Share Schedule Import conflict projection`). The F38 source and
  documentation handoffs remain `bc6ac011504e0a499a8cdfd4b1533b49ea3f4bcb`
  and `e7d1aa3ccb3934e8e800e8ef4d8913125f19b857` respectively.
- `src/next/application/schedule_import_overlap_projection.h` defines the
  standard-C++ projection. UI review formats its typed conflicts as translated
  warnings; application apply validation consumes the same overlap result.
- Required `tests/fixtures/imports/schedule_overlap_conflict.xlsx` drives
  production preview/review and apply. It verifies the review warning and
  disabled import action, then verifies conflicting apply persists no teacher,
  class, or class-time rows. The existing F37 trigger sentinel still passes.
- Independent fresh Windows x64 Ninja/MSVC configure validated 896 handwritten
  source owners. The three focused targets built and CTest passed 3/3. QtTest
  totals: Schedule Import 25/0/1 (the existing optional external sample was
  unset), review dialog 21/0/0, and app-less validation 12/0/0. Total: 58
  passed, 0 failed, 1 optional skip. `git diff --check` passed.
- App-less coverage includes half-open overlap/adjacency, weekdays,
  deterministic conflict ordering, Normal/Intensive schedules, skipped classes,
  and retained intensive schedules. The new projection header uses standard
  library headers only. UI and apply validation both call it.
- Gate audit after F39: app-less Domain/Application behavior remains partial
  because broader Domain models are incomplete; baseline parity remains partial
  despite fixture-backed Calendar import, Schedule preview, and conflict
  review/apply paths; the workspace boundary remains partial because
  FileController still asks MainWindow for dirty approval and closes the old
  session before a replacement succeeds; audited `src/next` dependency
  isolation remains satisfied. Phase 2 and its formal exit gate remain open.
- Remaining work includes wider baseline parity, shared workbook decoding,
  complete Domain models, generic settings, remaining feature-service
  migrations, and broader calendar/UI and document work. Preserve the Sub Prep
  limit of current and following calendar years at most. The user-modified
  `cmake/sources.cmake` remained untouched and uncommitted; nothing was pushed.

### F40 Domain schedule-time value

- F40 is committed as `2ab23fb1796dfb1761a4c48644869a9ae6e1060d`
  (`Phase2 - Add Domain schedule time value`). It adds standard-C++
  `Domain::Weekday` and `Domain::ScheduleTime::fromMinutes`, enforcing weekdays
  Monday-Sunday and same-day minute intervals with start 0..1439, end after
  start and below 1440. Half-open overlap is owned by the Domain value.
- Untrusted Schedule Import `ScheduleImportStateTime` inputs and display labels
  remain at the Application edge. The validator maps valid values once into
  `ScheduleImportProjectedTime` (`Domain::ScheduleTime` plus labels); invalid
  inputs still return `InvalidProjectedTime` with the original class/day/start/
  end labels before conflict projection. Review and apply use the typed
  projection and preserve F39 warning, conflict order, and rejection behavior.
- Independent fresh Windows x64 Ninja/MSVC Debug configure validated 897
  handwritten source owners. Four focused targets built and CTest passed 4/4.
  QtTest totals were Domain 8/0/0, state validation 13/0/0, Schedule Import
  repository 25/0/1, and review dialog 21/0/0 (67 passed, 0 failed, one
  existing optional external-workbook skip because
  `CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset). `git diff --check` passed.
- The checked-in F39 conflict workbook still produces the expected review
  warning and disabled action; conflicting apply persists no teachers, classes,
  or class times. The F37 pre-write trigger sentinel passed. Domain tests cover
  all weekdays, minute boundaries, invalid values, value/copy behavior,
  weekday separation, overlap, and adjacency. The Domain header has no Qt
  dependency.
- Post-F40 gate audit: Gate 1 is still Partial because broader Domain models
  remain incomplete; Gate 2 is Partial with wider baseline parity open; Gate 3
  is Partial due to FileController replacement preservation; audited `src/next`
  dependency isolation remains Satisfied. Phase 2 and the formal exit gate
  remain open. Remaining items include shared workbook decoding, generic
  settings, remaining feature-service migrations, and broader calendar/UI and
  document work. Preserve the Sub Prep current-and-following-year cap; F40 did
  not change Sub Prep or `cmake/sources.cmake`.

### F41 Schedule Import review decisions

- F41 is committed as `30ec8d7512a8847a5b1d32addabf25f252b0eabb`
  (`Phase2 - Validate Schedule Import review decisions`). It adds the Qt-free
  `ScheduleImportReviewDecisionRequest` contract and structured issue codes in
  `src/next/application/schedule_import_review_decisions.h`. The dialog uses
  the shared result for readiness and duplicate-target details; the plan
  validator adapts legacy choices into the same contract. Workbook content,
  colors/meeting checks, SQL writes, and F37 current-state validation remain at
  their existing owners.
- The required `schedule_review.xlsx` path now applies explicit accepted
  choices through the production repository and asserts result counts plus
  persisted teachers/classes/colors/schedule entries, retaining unrelated
  seeded metadata. The checked-in conflict fixture still displays a warning
  and disables Import; repository apply rejects it with zero teachers, classes,
  or schedule rows written. The F37 trigger sentinel passes.
- Independent fresh Windows x64 Debug/Ninja/MSVC verification validated 899
  handwritten source owners; four focused targets built and CTest passed 4/4.
  QtTest totals: review-decision Application contract 26/0/0, Schedule Import
  repository 25/0/1, review dialog 21/0/0, and state validation 13/0/0
  (85 passed, 0 failed, one existing optional external-workbook skip because
  `CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset). `git diff --check` passed.
- Gate audit after F41: Gate 1 Domain/Application is Partial with broader
  Domain models incomplete. Gate 2 baseline parity is Partial and improves
  with fixture-backed Schedule preview, conflict rejection, and accepted apply,
  but wider parity remains. Gate 3 workspace boundary is Partial because
  FileController still requires MainWindow dirty approval and closes the old
  session before replacement success. Gate 4 audited `src/next` dependency
  isolation remains Satisfied. Phase 2 is In Progress; the exit gate remains
  Open. Remaining work includes shared workbook decoding, broader Domain
  models, generic settings, remaining feature-service migrations, broader
  calendar/UI/document work, and the workspace replacement caveat. Preserve the
  Sub Prep current-and-following-year cap; `cmake/sources.cmake` was untouched.


### F42 failure-atomic workspace replacement

- F42 is committed as `8b2eb8a2a7a0dae6a22ce8a4163b35d5d9dd24ee`
  (`Phase2 - Preserve workspace on failed replacement`). The exact source
  paths are `src/data/database/database_session.cpp/.h`,
  `src/app/controllers/file_controller.cpp`,
  `tests/data_service_lifecycle_tests.cpp`, and
  `tests/file_controller_workspace_lifecycle_tests.cpp`.
- `DatabaseSession::open` stages a candidate on a unique SQL connection and
  only replaces the active session after candidate setup succeeds.
  `FileController` no longer closes the old session before replacement has
  succeeded. Repository adapters retain references to the database object, so
  F42 stores it at a stable heap address and transfers ownership with the
  repositories. This fixed a dangling-reference crash caught by the first
  independent test run during the initial settings write.
- Failed replacement preserves profile A, the same session/repository
  identity, readable settings, recent/current-file metadata, enabled actions,
  and autosave. Invalid candidate connections are cleaned up. Successful
  replacement switches to B, updates service reads and FileController
  metadata, and removes the old connection. Same-path reopen passes at the
  DataService/session layer. Coordinator tests cover dirty rejection, failed
  open snapshot preservation, and successful selection clearing; missing-path,
  invalid-startup, New Profile, Initial Setup backup, and close lifecycle cases
  also pass.
- Fresh isolated Windows x64 Ninja/MSVC with Qt 6.12.0 reconfigured the repaired
  source and verified 899 handwritten owners, each with exactly one explicit
  owner. Four focused targets built and CTest passed 4/4. QtTest totals:
  DataService lifecycle 17/0/0, FileController lifecycle 33/0/0, Workspace
  Coordinator 25/0/0, ApplicationServices workspace port 11/0/0 (passed/fail/
  errors; no skips; 86 passed overall). `git diff --check` passed with only
  line-ending notices; the production diff adds no `src/next` or
  `ClassMngrNext::` dependency. CMake warned that `vswhere.exe` was unavailable
  and about long object paths for unrelated targets, but all four assigned
  targets built. `cmake/sources.cmake` remains the user-owned uncommitted
  change and was untouched.
- Formal exit-gate audit after F42: Gates 1 and 2 remain Partial due to
  incomplete Domain models and wider baseline parity. The workspace criterion
  as written is Satisfied by the app-less coordinator create tests: dirty
  replacement is rejected, success opens WorkspaceState and clears
  SelectionState, and gateway/invalid-session failures preserve both snapshots.
  F42 additionally fixes failed production profile replacement. Direct
  FileController snapshot comparisons for invalid SQLite and same-path reopen
  remain integration coverage gaps outside that explicit criterion. Gate 4
  dependency isolation remains Satisfied. Phase 2 and the formal exit gate
  remain Open. Sub Prep stays limited to the current and following calendar
  years at most.
- Next: use a fresh three-lane Heavy-route investigation to select the next
  bounded slice from the remaining Domain and parity gaps; update the plan and
  mapping handoff before implementation.


### F43 Class Transfer review decisions

- F43 is committed as `c0e03e55aa5f5cc1897ccf97a25901a5e119c8e5`
  (`Phase2 - Validate Class Transfer review decisions`). The changed paths are
  `src/next/application/class_transfer_projection.h`,
  `src/features/classes/ui/class_import_dialog.cpp/.h`,
  `src/data/repositories/class_transfer_repository.cpp`,
  `tests/next_application_class_transfer_tests.cpp`,
  `tests/class_transfer_tests.cpp`, and the required
  `tests/fixtures/transfers/success_source.json`.
- The Qt-free Application decision validator is now used by both dialog
  readiness and repository apply-time plan validation. It requires complete,
  unique decisions; valid action/target combinations; targets in the current
  match set; and no duplicate replacement claims. It preserves zero-match
  Create, unique-match Keep/Replace of that target, and ambiguous-match
  Create or in-set Keep/Replace semantics. Repository apply rebuilds preview
  matches from current database state before validation. SQL matching,
  schedule preflight, transaction and writes remain repository-owned.
- The success fixture now exercises production parse, preview, ready dialog
  decisions, and apply. It asserts persisted class, teacher, roster, and
  regular schedule data, including Monday 3:30 PM to 3:50 PM. The existing
  conflict fixture continues to reject schedule collisions with no partial
  teacher/class/class-info/schedule/roster writes. App-less tests cover zero,
  one, and multiple matches; missing/duplicate choices; invalid actions;
  absent, foreign, or stale targets; and duplicate replacement claims.
- Independent fresh Windows x64 Ninja/MSVC Debug configure used MSVC
  19.51.36257.0 and Qt 6.12.0, validated 899 handwritten owners, and built
  both `ClassMngrClassTransferTests` and
  `ClassMngrNextApplicationClassTransferTests`. CTest passed 2/2. QtTest
  totals: Class Transfer 19/0/0 and Application contract 15/0/0
  (passed/failed/skipped; 34 passed total). The persisted end-time delta was
  rerun by the same tester with both targets still passing. `git diff --check`
  passed; the Application contract has no Qt/legacy dependency. The pre-existing
  `cmake/sources.cmake` modification remains untouched.
- Environment notes: the existing Visual Studio generator hit FileTracker
  `E_ACCESSDENIED` before compilation. The independent fresh Ninja build
  succeeded after loading `vcvarsall.bat x64`; harmless `vswhere.exe` and
  unrelated long-object-path warnings remained.
- Gate audit after F43: Gate 1 advances but remains Partial due to broader
  Domain records. Gate 2 improves with Class Transfer fixture-backed
  success/conflict behavior but remains Partial because wider baseline parity
  is incomplete. The formal WorkspaceCoordinator create criterion and audited
  `src/next` dependency isolation remain Satisfied. Non-gating integration
  caveats remain around MainWindow dirty approval, New Profile/Initial Setup
  close-before-create ordering, and direct FileController snapshot/same-path
  coverage. Phase 2 and the exit gate remain Open. Sub Prep stays capped at the
  current and following calendar years at most.
- F43 plan/mapping audit was committed as
  `00c5cc32e35322108c7fc6757fd5c80cdc0f879c`; it selected F44 from the remaining
  Domain and baseline-parity gaps. The current continuation is recorded below.


### Phase 2 Teacher Import review decisions - 2026-09-24

- F44 is committed as
  `28170a914a4dc76dc62f66677ad8f1067dfd42bf` (`Phase2 - Validate Teacher
  Import review decisions`). The seven paths are the Qt-free Application
  review-decision contract, the Teacher Import plan model, dialog adapter,
  repository validation, two focused test files, and the required
  `tests/fixtures/teacher_import/sectioned_review.xlsx` workbook.
- The contract is shared by dialog readiness/plan creation and repository
  apply validation. The repository checks reviewed Korean teacher identities
  before starting the transaction. Existing Qt-backed parsing, localized
  errors, SQL, and transaction ownership remain at their prior edges; direct
  legacy plans without review metadata keep their compatibility path.
- The checked-in fixture is required. The test
  `checkedInWorkbookProductionDialogPlanAppliesToRepository` loads it through
  the production dialog, obtains that dialog's `importPlan()`, and passes that
  exact plan to `TeacherImportRepository`. It verifies the import summary,
  included/omitted choices, Korean/Native English/GS Team persisted records,
  and manually maintained fields. Rejected choices preserve records and
  source date. App-less tests cover invalid modes, empty/duplicate/unknown
  groups, missing decisions, and invalid/duplicate indexes.
- Independent fresh Windows x64 MSVC/Ninja Debug verification used Qt 6.12.0.
  Both focused Teacher Import targets built and CTest passed 2/2. The named
  end-to-end case and invalid-decision case each passed 3/0/0 directly
  (passed/failed/skipped); neither required fixture case skipped. The external
  sample checks remain supplemental. `git diff --check` passed and the
  Application contract has no Qt or legacy dependency. `cmake/sources.cmake`
  retained SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF` and was
  not committed. The source commit contains only the seven F44 paths.
- The first independent pass identified that fixture import and dialog review
  were tested separately; the required exact-dialog-plan-to-repository test
  was added and independently rerun before commit.
- Gate 1 and Gate 2 advance but remain Partial; broader Domain records and
  baseline parity remain open. The formal workspace criterion and audited v2
  dependency isolation remain Satisfied. Phase 2 remains In Progress and its
  exit gate remains open. Preserve the Sub Prep cap of the current and
  following calendar years at most.
- F45 is now assigned: implement a Qt-free Domain `Course` catalog/value and
  route the existing Schedule Import grade/level validation through it,
  preserving the legacy catalog and fixture behavior. After independent
  verification, commit that slice, update the Phase 2 plan/mapping and repeat
  from the remaining Domain and parity gaps. Do not touch
  `cmake/sources.cmake`.


### Phase 2 Domain Course catalog - 2026-09-24

- F45 is committed as
  `eb2167e9d39a65446265b9506d749dc0e6be0d35` (`Phase2 - Add Domain course
  catalog`). The six paths are `src/next/domain/course.h`, the single
  `cmake/next.cmake` Domain ownership registration,
  `src/features/classes/config/class_info_config.cpp`,
  `src/features/schedule/services/schedule_import_plan_validator.cpp`,
  `tests/next_domain_contract_tests.cpp`, and
  `tests/schedule_import_tests.cpp`.
- `Domain::Course` is a Qt-free value/catalog of the existing 25 supported
  grade/level pairs, preserving order and capitalization. `ClassInfoConfig`
  converts from the Domain catalog for Qt-facing lists, and Schedule Import
  validation uses `Course::fromNames`. ClassInfoConfig and validator retain
  existing behavior at their feature boundary.
- App-less tests cover every pair, ordering, value/accessor semantics, invalid
  names and cross-grade pairs. The existing `schedule_review.xlsx` production
  path still persists accepted grade/level values. The invalid-pair repository
  test now seeds teacher, class, class-info, schedule-time, and profile-setting
  state, then compares every snapshot after rejection to prove no mutation or
  added rows.
- Independent Windows x64 MSVC/Ninja Debug verification used Qt 6.12.0 and a
  fresh out-of-tree build. Both focused targets built and CTest passed 2/2.
  All three direct Domain cases, the valid fixture import, and the repaired
  invalid-apply case passed 3/0/0 each. After the invalid-case test repair, the
  same fresh build rebuilt both targets and reran focused CTest 2/2 plus the
  repaired case 3/0/0. CMake source ownership validated 900 handwritten
  files. `git diff --check` passed.
- The first independent pass found that empty affected tables could not prove
  preservation; the test was strengthened with seeded records and exact
  snapshots, then rerun by the same independent tester. The only CMake code
  change is the owner registration in `cmake/next.cmake`.
  `cmake/sources.cmake` remains unchanged with SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Gate 1 advances with a Domain record used by production Schedule Import;
  Gate 2 gains fixture-backed valid and rejected state behavior. Both remain
  Partial because broader Domain completeness and baseline parity remain.
  Workspace boundary and audited v2 dependency isolation remain Satisfied.
  Phase 2 is In Progress and the exit gate remains open. Preserve Sub Prep's
  current-and-following-calendar-year maximum.
- Three independent post-F45 investigators compared Domain, baseline-parity,
  and architecture candidates. F46 is assigned to centralize the duplicated
  Hangul-only Korean teacher identity rule as a Qt-free Domain key, used by
  both Teacher and Schedule matching. Preserve the exact five UTF-16 code-unit
  ranges and current empty-name handling; do not trim, case-fold, or normalize.
  Reuse the required Teacher and Schedule fixtures for production coverage.
  After independent verification, commit and audit the slice, then repeat
  against the remaining Phase 2 gates. Keep `cmake/sources.cmake` untouched.


### Phase 2 Korean teacher identity key - 2026-09-24

- F46 is committed as
  `bedb52e0045731bba3e4f4b7a576021bdc007b72` (`Phase2 - Centralize Korean
  teacher key`). Its seven paths are `cmake/next.cmake`,
  `src/next/domain/korean_teacher_key.h`,
  `src/next/application/schedule_import_matching_projection.h`,
  `src/features/teacher/import/teacher_import_name_utils.h`,
  `tests/next_domain_contract_tests.cpp`,
  `tests/next_application_schedule_import_matching_projection_tests.cpp`,
  and `tests/teacher_import_tests.cpp`.
- The Qt-free Domain value owns the existing five UTF-16 code-unit ranges:
  0x1100–0x11FF, 0x3130–0x318F, 0xA960–0xA97F, 0xAC00–0xD7AF, and
  0xD7B0–0xD7FF. It preserves code-unit order and does not trim or normalize.
  The Teacher QString adapter and Schedule Import matching use the shared
  rule. Empty-name behavior remains at the caller: contact parsing retains its
  same validation message, repository validation retains its required-name
  error, and Schedule matching retains its existing empty-key match behavior.
- Independent fresh Windows x64 MSVC 19.51/Ninja 1.13/CMake 4.4.2/Qt 6.12.0
  verification configured 901 handwritten source owners. The four focused
  Domain, matching, Teacher Import, and Schedule Import CTest targets passed
  4/4; Teacher Import Dialog passed 1/1. QtTest counts were respectively
  14/0/0, 6/0/0, 16/0/1, 26/0/1, and 5/0/1 (passed/failed/skipped). The three
  skips were supplemental external-workbook checks with unset sample variables.
  Required Teacher parser/review/repository and dialog-plan apply fixtures,
  Schedule matching/persisted fixture and overlap rejection passed. `git diff
  --check` passed. `cmake/sources.cmake` SHA-256 remains
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- One separate Hangul predicate remains in the sidebar marquee delegate with
  a different syllable endpoint; it is outside the Teacher/Schedule identity
  key paths in F46.
- Gate 1 and Gate 2 advance but remain Partial; broader Domain records and
  baseline parity remain. Workspace boundary and audited v2 dependency
  isolation remain Satisfied. Phase 2 remains In Progress with the exit gate
  open. Preserve the Sub Prep cap of the current and following calendar years
  at most.
- F47 completed the selected Domain Course weekly meeting-day rule; its
  implementation, independent verification, gate audit, and F48 handoff follow.


### Phase 2 Course weekly meeting-day rule - 2026-09-24

- F47 is committed as `7cba8abf952b5b32f90391844beec68eac2c3f69` (`Phase2 -
  Add Course meeting-day rule`). The commit contains only
  `src/next/domain/course.h`, `src/domain/rules/schedule_import_rules.cpp`,
  `tests/next_domain_contract_tests.cpp`, and `tests/schedule_import_tests.cpp`.
- `Domain::Course` owns typed weekly weekday patterns using
  `Domain::Weekday`. Schedule Import uses that policy for production parsing,
  partitioning, and apply validation; raw weekday text, course-name
  normalization, and localized messages remain at the feature edge. Grade
  trim+uppercase, trimmed case-insensitive Athena/Song's names, order-insensitive
  matching, duplicate/weekend rejection, and the `M3 Zeus` no-pattern-error
  behavior remain. The separate Course catalog still rejects `M3 Zeus`.
- App-less Domain tests cover paired, Athena/Song's, and single-day categories,
  allowed and rejected patterns, order, duplicates, weekends, and invalid
  enum values. The required `schedule_review.xlsx` production path persisted
  accepted rows. The fixture-derived one-day E4/Theseus pattern was rejected
  before writes; seeded teacher, class, class-info, class-time, and app-setting
  snapshots stayed unchanged. Existing invalid-course, overlap no-write, and
  Skip+prohibited-pattern (`E5`/`Zeus`, Tuesday-only) cases passed.
- Executor and independent Tester each configured fresh Windows x64 builds
  with MSVC 19.51.36257, Ninja 1.13.2, CMake 4.4.2, and Qt 6.12.0. Both built
  `ClassMngrNextDomainContractTests` and `ClassMngrScheduleImportTests`; focused
  CTest passed 2/2 in each build. Direct fixture and Domain cases passed. The
  full suite was not run. `git diff --check` passed. User-owned
  `cmake/sources.cmake` retained SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF` and was
  not included in the commit.
- After F47, Gate 1 and Gate 2 advance but remain Partial because broader Domain
  records and baseline parity are incomplete. The workspace boundary and
  audited v2 dependency isolation remain Satisfied. Phase 2 and its exit gate
  remain open. Sub Prep is capped at the current and following calendar years
  at most.
- F48 completed the selected Domain schedule-entry persistence slice; its
  verified handoff follows. Three investigators will compare the remaining
  Domain, baseline-parity, and architecture gaps to select F49.


### Phase 2 Domain schedule-entry persistence - 2026-09-25

- F48 is committed as `2055bbb5f74842e4f146a48e211df58e65908b6b` (`Phase2 -
  Add Domain schedule entry`). Its five paths are `cmake/next.cmake`,
  `src/next/domain/schedule_entry.h`,
  `src/data/repositories/schedule_import_repository.cpp`,
  `tests/next_domain_contract_tests.cpp`, and `tests/schedule_import_tests.cpp`.
- The Qt-free `Domain::ScheduleEntry` contains a typed `ClassId` and validated
  `ScheduleTime` with value/accessor semantics. Schedule Import constructs
  entries after real class IDs resolve and consumes them in the SQL persistence
  writer. The adapter keeps original weekday/time text and checks it against
  the typed value; write order and the transaction, Skip, Intensive, and
  rollback paths remain preserved. The Domain value is distinct from the
  existing UI schedule-row projection.
- App-less coverage verifies class-vs-teacher ID type distinction, equality,
  and schedule-time values. The required `schedule_review.xlsx` production
  apply compares persisted rows with typed facts after IDs resolve. The
  `schedule_overlap_conflict.xlsx` case now seeds teachers, classes,
  class_info, class_times, and app_settings and proves rejection leaves all five
  snapshots unchanged. Existing invalid-course/pattern, Skip, intensive, and
  write-failure rollback cases passed.
- Executor and independent Tester each configured fresh x64 MSVC
  19.51.36257/Ninja 1.13.2/CMake 4.4.2/Qt 6.12.0 builds and validated 902
  handwritten source owners. Both built `ClassMngrNextDomainContractTests` and
  `ClassMngrScheduleImportTests`; focused CTest passed 2/2 in each build.
  Direct QtTest totals were Domain 17/0/0 and Schedule Import 26/0/1; its single
  skip was the optional external-workbook sample. The Tester directly reran
  fixture apply, overlap rejection, Skip, and Intensive cases. `git diff
  --check` passed; the full suite was not run. User-owned `cmake/sources.cmake`
  remains at SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF` and was
  not part of the commit.
- Gate 1 and Gate 2 advance but remain Partial because broader Domain records
  and baseline parity are incomplete. The formal workspace boundary and
  audited v2 dependency isolation remain Satisfied. Phase 2 is In Progress and
  its exit gate remains Open. Sub Prep stays capped at the current and
  following calendar years at most.


### Phase 2 Calendar Import application use case - 2026-09-25

- F49 is committed as
  `6a41e958671b7fa93c301d8b25c9c4381178fd7f` (`Phase2 - Add Calendar Import
  use case`). The five paths are `cmake/next.cmake`,
  `cmake/tests/next.cmake`,
  `src/next/application/calendar_event_import_use_case.h`,
  `src/features/calendar/calendar_event_import_service.cpp`, and
  `tests/next_application_calendar_event_import_use_case_tests.cpp`.
- The Qt-free use case composes the existing Calendar Import signature-query,
  duplicate-planning, and batch-save ports. It pairs each exact UTF-16
  signature with its save request through planning, then saves accepted
  requests in order and reports imported/skipped counts. The production
  service delegates this flow while workbook parsing, network and campus
  handling, Qt signals, localized errors, and the observer-backed profiler
  timing remain at the feature edge.
- Six app-less fake-port cases cover ordered mapping, existing and in-batch
  duplicates, parser skip counts, truly empty and duplicate-only input, exact
  UTF-16 identity including an unpaired surrogate, and query/save failures.
  The required `calendar_import_parity_2026.xlsx` production test passed and
  checked persisted facts and counts. Truly empty input avoids port calls;
  duplicate-only nonempty input retains its empty batch-save call.
- Executor and independent Tester each configured fresh Windows x64 builds
  with MSVC 19.51.36257, Ninja 1.13.2, CMake 4.4.2, and Qt 6.12.0. Both built
  the app-less use-case and production parity targets; focused CTest passed
  2/2 in each build. Ownership validation covered 904 handwritten sources.
  `git diff --check` passed. The full suite was not run. The protected
  user-owned `cmake/sources.cmake` retained SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF` and was
  not included in the commit.
- Gate 1 advances but remains Partial because broader Domain and application
  contracts remain. Gate 2 gains a production Calendar Import fixture path but
  remains Partial because baseline parity is incomplete. The workspace
  criterion and audited v2 dependency isolation remain Satisfied. Phase 2 is
  In Progress and its exit gate remains Open. Sub Prep covers at most the
  current and following calendar years. Next, three investigators will compare
  remaining Domain, baseline-parity, and architecture gaps for the next slice.


### Phase 2 Calendar Import signature identity - 2026-09-25

- F50 is committed as
  `92d001db11d8c8eb973de5f238444abe855ea5c5` (`Phase2 - Add Calendar Import
  signature identity`). Its six paths are `cmake/next.cmake`,
  `cmake/tests/next.cmake`,
  `src/next/application/calendar_event_import_signature.h`,
  `src/features/calendar/academic_calendar_event_parser.cpp`,
  `src/next/platform/application_services_calendar_event_import_signature_query_port.h`,
  and `tests/next_application_calendar_event_import_signature_tests.cpp`.
- `Application::CalendarEventImportSignature` centralizes the legacy duplicate
  key used at workbook parsing and database lookup. The ordered fields are
  simplified title, normalized event type, ISO start and end dates, all-day
  `1`/`0`, and normalized time status, joined by the same delimiters. Qt
  normalization/date conversion remain at the adapters. The UTF-16 code units
  are preserved; event times, database ID, and repeat-series ID are excluded.
- The app-less test covers field order and formatting, changes to each key
  field, exact UTF-16 including placeholder-like title text, and absence of
  metadata fields. Existing `CalendarImportTests` retains coverage for
  normalization and ignored times/ID/series metadata. A Qt 6.12 probe confirmed
  the old six-argument `QString::arg` does not rescan inserted `%2` title text.
- Executor and independent Tester each configured fresh Windows x64 builds
  under MSVC 19.51.36257, Ninja 1.13.2, CMake 4.4.2, and Qt 6.12.0. Each
  configure validated 906 handwritten source owners; the signature contract,
  `ClassMngrCalendarImportTests`, and the required
  `ClassMngrCalendarEventImportParityTests` targets built, and focused CTest
  passed 3/3. The fixture `calendar_import_parity_2026.xlsx` verified the
  production parser/query path. `git diff --check` passed. The full suite was
  not run. Protected `cmake/sources.cmake` retained SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF` and was
  not part of the commit.
- Gate 1 and Gate 2 advance but remain Partial; workspace create and audited
  v2 dependency isolation remain Satisfied. Phase 2 is In Progress and the
  formal exit gate remains Open. Sub Prep stays capped at the current and
  following calendar years at most. Next, three investigators will compare
  remaining Domain, baseline-parity, and architecture gaps.

### Phase 2 typed Calendar Import signature flow - 2026-09-25

- F51 is committed as
  `e940f0c0ed8a63e740a3c2375631a22c08875f84` (`Phase2 - Carry Calendar Import
  signature through contracts`). It carries
  `Application::CalendarEventImportSignature` through parser outputs, query
  results, planner inputs, candidates, service mapping, and the application
  use case. The service no longer converts the typed value through QString and
  back to raw UTF-16 strings. The parser membership set hashes the existing
  exact UTF-16 payload; emitted candidate order remains unchanged.
- A fresh Windows x64 MSVC/Ninja configure passed the source ownership audit
  with 906 handwritten sources. The seven focused CTest cases each passed 1/1:
  `ClassMngrNextApplicationCalendarEventImportSignatureTests`,
  `ClassMngrNextApplicationCalendarEventImportSignatureQueryPortTests`,
  `ClassMngrNextApplicationCalendarEventImportPlanTests`,
  `ClassMngrNextApplicationCalendarEventImportUseCaseTests`,
  `ClassMngrCalendarImportTests`,
  `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and
  `ClassMngrCalendarEventImportParityTests`. The parity test used the checked-in
  `calendar_import_parity_2026.xlsx` fixture. After restoring detailed
  `QCOMPARE` output in all four typed signature equality checks,
  `ClassMngrCalendarImportTests` was rebuilt and independently rerun, passing
  1/1. No full suite was run.
- The plan audit left Gate 1 and baseline parity Gate 2 Partial; the formal
  workspace-create criterion and audited v2 dependency isolation remain
  Satisfied. Phase 2 remains In Progress and its exit gate Open. Sub Prep
  already queries at most the current and following calendar years; the
  dedicated Sub Prep plan now records that bound with source and test links.
- `cmake/sources.cmake` remains the sole unrelated modified path, with SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: start three independent investigations to select the next bounded
  Phase 2 slice from the remaining Domain, baseline-parity, and architecture
  gaps, then continue one source commit per slice.

### Phase 2 shared Calendar event timing - 2026-09-25

- F52 is committed as
  `9cd9a2a4469482bc803cdc172d18a072c0fb3949` (`Phase2 - Centralize Calendar
  event timing`). It adds Qt-free `Domain::CalendarEventTiming` and routes
  event-save, edit-draft, and repeat-series-edit timing validation through
  it. Application boundaries retain their feature-specific errors and
  validation order; cross-day earlier/equal clock times remain valid.
- The exact date format now requires hyphens at positions 4 and 7 and ASCII
  digits elsewhere. App-less Domain cases reject `2026006010` and
  `2026-06110`, and cover Gregorian/leap-year boundaries, paired times,
  all-day/status policy, same-day ordering, and cross-day ordering.
- A fresh independent MSVC 19.51/Ninja/Qt 6.12 configure validated 907
  handwritten source owners. The Domain contract, Application calendar-event
  contract, and Calendar Import parity targets built. Their exact CTest cases
  passed 3/3; the parity test used the checked-in
  `calendar_import_parity_2026.xlsx` fixture. The full suite was not run.
- The plan audit leaves Gate 1 and Gate 2 Partial; workspace boundary and
  audited v2 dependency isolation remain Satisfied. Phase 2 remains In
  Progress with its exit gate Open. Sub Prep remains limited to the current
  and following calendar years at most. The user-owned
  `cmake/sources.cmake` change was not part of F52.
- Next: compare three independent investigations of the remaining Domain,
  baseline-parity, and architecture gaps, then implement and verify the next
  bounded slice.

### Phase 2 Roster Score Import parity - 2026-09-25

- F53 is committed as
  `de763a0e64b3139a2c51b99bcdd610364861b920` (`Phase2 - Verify Roster score
  import parity`). It adds `tests/roster_editor_widget_import_tests.cpp` and
  registers `ClassMngrRosterEditorWidgetImportTests` in
  `cmake/tests/pages_and_output.cmake`.
- The test uses `ClassMngrRuntime` and invokes the real private import slot. It
  seeds saved evaluations through production services in a temporary database;
  this workflow reads evaluations already saved in the database and does not
  parse a workbook. Assertions cover all four grade columns, full English /
  Korean name-pair matching including collisions, preservation of unmatched,
  empty, and English-only partial rows, autosave followed by a fresh
  `RosterService` read, idempotent re-import, and missing English/Korean column
  warnings with no persistence. Mixed Winter components total 16 points across
  six scores (about 2.667), yielding the expected B+ under the existing
  `>= 0.4` rounding rule.
- A fresh independent MSVC 19.51/Ninja/Qt 6.12 configure validated 908
  handwritten source owners. The three targets
  `ClassMngrRosterEditorWidgetImportTests`, `ClassMngrRosterModelTests`, and
  `ClassMngrSpeakingEvaluationServiceTests` built and passed exact CTest 3/3.
  No full suite was run. The first Visual Studio build attempt failed before
  compilation in `ZERO_CHECK`; the independent fresh Ninja build passed.
- The plan and parity mapping now record F53 as Gate 2 evidence only. Gate 1
  and Gate 2 remain Partial; workspace boundary and audited `src/next`
  dependency isolation remain Satisfied. Phase 2 remains In Progress and the
  formal exit gate remains Open. Sub Prep stays capped at the current and
  following calendar years at most. The modified user-owned
  `cmake/sources.cmake` was excluded from the commit.
- Three independent solution reviews compared grade-band classification
  against centralizing the repeated Qt-free Calendar event/status vocabulary.
  F54 is underway on the latter, retaining per-request trimming, limits,
  validation order, and error behavior. Continue with independent verification
  and commit each bounded slice.

### Phase 2 Calendar event vocabulary - 2026-09-25

- F54 is committed as
  `3739f2aaca23587c732dc77d6e77eddb16d92d88` (`Phase2 - Centralize Calendar
  event vocabulary`). It adds Domain classifiers for the six Calendar event
  types and the `Timed`, `Unknown`, and `Unconfirmed` statuses, then reuses them
  in edit-draft, single-save, and repeat-series validators.
- The Application contracts keep raw fields and trim at their own boundaries;
  existing 64-character limits, validation order, feature-specific error
  messages, and projection behavior remain unchanged. Domain and Application
  tests cover every accepted name, unknown/case/untrimmed rejection, padded
  valid strings with raw preservation, bounds, and operation-specific errors.
- A fresh independent Ninja/MSVC 19.51/Qt 6.12 Debug configure and build
  passed `ClassMngrNextDomainContractTests` and
  `ClassMngrNextApplicationCalendarEventTests`; exact CTest passed 2/2. No full
  suite was run. F54 adds Gate 1 evidence; Gate 1 and Gate 2 remain Partial,
  workspace boundary and audited `src/next` dependency isolation remain
  Satisfied, and the formal Phase 2 exit gate remains Open. Sub Prep stays
  capped at the current and following calendar years at most.
- Two independent Explorers compared remaining Phase 2 gaps and both selected
  exposing the existing `CourseGradeBand` classifier as F55. Route it through
  Classes tab visibility, evaluation defaulting, and Schedule's testing
  suppression while preserving each caller's normalization and distinct
  policy. The lack of a direct `forClass` grade-choice test is an acceptance
  concern; F55 is underway to cover that seam if practical.

### Phase 2 Course grade-band classification - 2026-09-25

- F55 is committed as
  `87b7bfff66bf13cc5b79180cd1142101875be3cf` (`Phase2 - Share Course grade
  band classification`). It exposes `Course::gradeBandForName` and uses the
  grade-only Domain classifier in Classes tab visibility, evaluation default
  selection, and Schedule testing suppression.
- Each feature retains `trimmed().toUpper()` at its Qt edge and keeps its
  policy: Classes hides Analytics/Evaluations for M1-M3 subject to preference;
  evaluation selects Middle for M1-M3 and Elementary otherwise; Schedule
  suppresses M2/M3 and M1 only when configured. Domain coverage confirms an
  incomplete grade/level pair can still classify by grade alone.
- A fresh independent MSVC 19.51/Ninja/Qt 6.12 configure validated 908
  handwritten source owners. `ClassMngrNextDomainContractTests`,
  `ClassMngrClassesPageTests`, `ClassMngrSchedulePrintModelTests`, and
  `ClassMngrEvaluationDefaultSelectionTests` built and passed exact CTest 4/4.
  No full suite was run. The evaluation test invokes the same school-level
  policy helper used by `forClass`; it does not instantiate the full
  ApplicationServices path.
- F55 adds Gate 1 evidence; Gate 1 and Gate 2 remain Partial. Workspace
  boundary and audited `src/next` dependency isolation remain Satisfied, and
  the Phase 2 exit gate remains Open. The user-owned `cmake/sources.cmake`
  change was not part of F55. Sub Prep stays capped at the current and
  following calendar years at most.
- Next: compare the remaining Domain/Application and baseline-parity gaps for
  another bounded slice. Keep commits path-limited and independently verify
  each focused acceptance target before documenting its gate impact.

### Phase 2 Speaking Evaluation aggregate grade - 2026-09-26

- F56 is committed as
  `c73e896fe34e186a045d73b653aa8ec9dfa89e83` (`Phase2 - Centralize Speaking
  Evaluation grades`). The Qt-free Domain rule defines six criteria and
  supported grade values, parses exact labels, and calculates the overall
  grade with the existing `>= 0.4` rounding and invalid/missing outcome.
- The rule now serves `SpeakingEvalRepository::buildRosterScoreImport`, the
  report data assembler, and the live report widget. Repository parsing still
  trims labels; report paths still require exact labels. The widget import
  test saves a padded component and an incomplete evaluation, then verifies
  B+ and N/A in a fresh roster read. The original mixed 16/6 -> B+ persistence
  and idempotence checks remain. Report tests compare the mixed grade and N/A
  through the assembler and rendered output; Domain coverage exhausts all
  15,625 valid combinations.
- A fresh independent MSVC 19.51/Ninja/Qt 6.12 configure validated 909
  handwritten source owners. `ClassMngrNextDomainContractTests`,
  `ClassMngrRosterEditorWidgetImportTests`,
  `ClassMngrSpeakingEvaluationServiceTests`, and
  `ClassMngrSpeakingEvalReportWidgetTests` built and passed exact CTest 4/4.
  No full suite was run.
- The three-investigator comparison selected this named Domain gap over a
  full Evaluation Default Selection `forClass` integration test. Gate 1 and
  Gate 2 both gain evidence but remain Partial; workspace boundary and audited
  `src/next` dependency isolation remain Satisfied; the formal Phase 2 exit
  gate remains Open. Full `forClass` integration and wider parity remain open.
  Sub Prep stays capped at the current and following calendar years at most.
- Next: review the remaining Phase 2 Gate 1 and Gate 2 gaps and select the next
  bounded slice. Leave the user-owned `cmake/sources.cmake` untouched and
  commit source and documentation paths separately after each slice.

### Phase 2 Evaluation Default Selection - 2026-09-26

- F57 is committed as
  `b38b3afef0088b4c05d6d540dda15600f48c7f59` (`Phase2 - Add Evaluation
  Default Selection contract`). It adds the Qt-free `EvaluationPeriod`
  current/previous selector and adapts both `forTermSchedule` and `forClass`
  while keeping dates, persisted-service access, and exact evaluation labels
  at the feature edge.
- App-less tests cover populated/current, empty/previous for all four periods,
  Winter-to-Fall wraparound, All, and invalid period. The production
  integration target uses real `ApplicationServices` and a temporary database;
  at 2026-09-07 it verifies M2 current Fall vs previous Summer and E4 current
  Summer vs previous Speech Contest, plus All, missing schedule, and unavailable
  class information. Failed evaluation-read behavior returns no default in
  code, though the integration test does not inject that specific service
  failure.
- The executor built five focused targets and passed 5/5 CTests. Independent
  fresh Ninja/MSVC 19.51/Qt 6.12 configuration validated 912 handwritten
  source owners; the new contract, integration, existing helper, and policy
  port tests passed 4/4. The unchanged Academic Calendar schedule-preferences
  port target repeatedly failed compilation at its generated `.moc` include
  with MSVC C1083, even though the file appeared in autogen output after the
  failure; its CTest did not run independently. The executor's separate run
  passed that target. No full suite was run; baseline cause is unconfirmed.
- F57 closes F55's test gap for the full `ApplicationServices::forClass` path
  and adds evidence to Gate 1 and Gate 2, both of which remain Partial. The
  workspace boundary and audited `src/next` dependency isolation remain
  Satisfied; the formal Phase 2 exit gate remains Open. Sub Prep remains capped
  at the current and following calendar years at most. The user-owned
  `cmake/sources.cmake` file was excluded.
- Next: compare the remaining Gate 1 and Gate 2 gaps and select another
  bounded slice. Keep the current Sub Prep cap and the explicit-manifest
  boundary.

### Phase 2 Schedule Import typed matching identities - 2026-09-26

- F58 is committed as
  `9b9183818fc2163d625a8ffb088a492a4aa631a9` (`Phase2 - Type Schedule Import
  matching identities`). The Qt-free matching contract now carries
  `Domain::TeacherId` and `Domain::ClassId` through inputs and results, and
  represents an absent suggested class with `std::optional<ClassId>`. The
  repository converts between these typed values and the existing integer
  preview at the compatibility edge.
- Matching order, tie order, categories, confidence/explanations, intensive
  fallback, and empty teacher-key behavior are preserved. Teacher IDs 0 and
  -1 remain matchable; class IDs 0 and -7 cannot become matches or suggestions
  but still appear in the initially absent inventory, matching the old rules.
  Compile-time assertions confirm teacher and class IDs cannot be assigned
  across categories.
- The executor built the matching projection target and passed its focused
  CTest 1/1. Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 configuration
  validated 912 handwritten source owners; the matching target and
  `ClassMngrScheduleImportTests` built, and both CTests passed 2/2. The latter
  includes `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` with
  `schedule_review.xlsx`. No full suite was run. No direct production test
  asserts the no-suggestion `-1` adapter value; the existing preview model
  default remains `-1`.
- F58 adds Gate 1 typed-identity and Gate 2 fixture-parity evidence; Gate 1 and
  Gate 2 remain Partial. Workspace boundary and audited `src/next`
  dependency-isolation statuses remain Satisfied, and Phase 2 remains open.
  Sub Prep remains capped at the current and following calendar years at
  most. `cmake/sources.cmake` was excluded and its SHA-256 remained
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: compare remaining Gate 1 and Gate 2 gaps and select another bounded
  slice; preserve the Sub Prep cap.

### Phase 2 Schedule Import state validation identities - 2026-09-26

- F59 source is committed as
  `7769912e1a8ccec02ecc3ace2de11ff98c719327` (`Phase2 - Type Schedule Import
  state validation IDs`). The Qt-free `ScheduleImportStateValidationRequest`
  now uses `Domain::TeacherId` and `Domain::ClassId` for resolution targets,
  teacher/class snapshots, teacher links, and projected classes. Candidate
  indexes remain integers. The repository constructs typed IDs and performs
  action-aware conversion of the legacy absence/sentinel values at the
  `scheduleService()` boundary; no UI or persistence behavior was moved into
  Application.
- Numeric database ordering is preserved with an explicit numeric comparator
  for projected class IDs, including the 2-versus-10 ordering case and
  synthetic negative IDs. App-less coverage includes compile-time ID
  distinction, selected/missing/absent/stale targets, exact skip uniqueness,
  a skipped class with a mismatched teacher key, and conflict behavior.
- Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 configuration validated 912
  handwritten source owners. Both
  `ClassMngrNextApplicationScheduleImportStateValidationTests` and
  `ClassMngrScheduleImportTests` built and passed exact CTest 2/2. The focused
  Schedule Import target includes
  `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase`; the state target
  includes the mismatched teacher-key skip regression. `git diff --check`
  passed before commit. No full suite was run. The targeted review did not add
  an exhaustive direct matrix for every action-specific sentinel conversion.
- Gates 1 and 2 gain evidence and remain Partial. Workspace boundary and
  audited `src/next` dependency isolation remain Satisfied; the Phase 2 exit
  gate remains Open. Sub Prep remains capped at the current and following
  calendar years. The protected user-owned `cmake/sources.cmake` was not
  staged or committed; SHA-256 remained
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: compare remaining Gate 1/2 contracts and baseline parity for the next
  bounded slice. Keep source and documentation commits separate and preserve
  the protected CMake change.

### Phase 2 Schedule Import review-decision identities - 2026-09-26

- F60 is committed as
  `730955dd1feb24e5a46dd0bfef9f86b8ff619621` (`Phase2 - Type Schedule Import
  review decision IDs`). `ScheduleImportReviewClassResolution` and
  `ScheduleImportReviewDecisionIssue` now carry optional `Domain::ClassId`
  values. The feature PlanValidator and dialog explicitly convert legacy
  integer selections; targets <= 0 become absent. Application remains Qt-free.
- The validator preserves UpdateExisting's required-target rule, CreateNew's
  no-target rule, optional Skip targets, target-claim ordering, duplicate
  update/skip issue codes, and claimant-index details. The dialog converts
  typed target values only at the class-label boundary. Compile-time assertions
  distinguish class and teacher IDs. App-less cases assert CreateNew and Skip
  accept absent targets and preserve duplicate issue details.
- Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 configure validated 912
  handwritten source owners. Three targets built in 321 Ninja steps and
  `ClassMngrNextApplicationScheduleImportReviewDecisionsTests`,
  `ClassMngrScheduleImportDialogTests`, and `ClassMngrScheduleImportTests`
  passed exact CTest 3/3. The latter includes
  `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` loading the
  checked-in `schedule_review.xlsx` fixture. The app-less target also passed
  1/1 after the added Skip-without-target regression. `git diff --check`
  passed. No full suite was run. The duplicate-target warning test also
  asserts the resolved existing class label `E5 Athena`.
- Gates 1 and 2 remain Partial; workspace boundary and audited `src/next`
  dependency isolation remain Satisfied; the Phase 2 exit gate remains Open.
  The F59 action-specific sentinel matrix remains an uncovered parity detail.
  Sub Prep stays capped to the current and following calendar years
  (2026–2027). The protected `cmake/sources.cmake` file was excluded and its
  SHA-256 remained
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: select a bounded F61 slice from remaining Gate 1/2 gaps. Preserve the
  separate source/documentation commit sequence and protected CMake change.

### Phase 2 Evaluation Default Selection read failure - 2026-09-26

- F61 is committed as
  `5e08c2aab8c4c326463e969445757fa90e25d79c` (`Phase2 - Cover evaluation
  default read failure`). It adds
  `failedCurrentEvaluationReadReturnsNoDefault()` to
  `tests/evaluation_default_selection_integration_tests.cpp`; production code
  is unchanged.
- The test opens a unique temporary database, creates an M2 class with valid
  ClassInfo, saves the 2026 academic schedule, and sets
  `CurrentOrPreviousTerm`. Before altering storage, the successful empty Fall
  evaluation read produces Summer for 2026-09-07. It then drops
  `speaking_evaluations`, asserts the repository evaluation read is an error,
  and asserts the production `EvaluationDefaultSelection::forClass` result is
  empty. Existing missing-schedule and missing-class-info tests are retained.
- Executor verification built the target and passed
  `ClassMngrEvaluationDefaultSelectionIntegrationTests` (1/1). Independent
  fresh x64 Ninja/MSVC 19.51/Qt 6.12 configuration validated 912 source
  owners, built the integration target, and passed the exact CTest 1/1.
  `git diff --check` passed; no full suite was run.
- Gate 2 gains a distinct production-path error case; Gates 1 and 2 remain
  Partial. Workspace boundary and audited `src/next` dependency isolation
  remain Satisfied; Phase 2 exit gate remains Open. F59's action/sentinel
  matrix remains uncovered. Sub Prep
  remains capped to 2026-2027. Protected `cmake/sources.cmake` was excluded and
  SHA-256 remained
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: compare remaining Gate 1/2 gaps for F63 and preserve the separate
  source/documentation commit sequence.

### Phase 2 Schedule Import sentinel characterization - 2026-09-26

- F62 is committed as
  `691e56fbdcc536aaaf577602feeac25fc5b7227f` (`Phase2 - Characterize Schedule
  Import sentinels`). The repository-boundary matrix covers Reuse and
  UpdateRoom with teacher IDs -1/0 and matching rows present or absent; Create
  and Skip teacher actions with nonpositive and positive foreign targets; and
  class CreateNew/Skip sentinels, exact/mismatching positive Skip targets, and
  stale positive UpdateExisting targets. Rejected cases compare persisted
  database snapshots. Positive CreateNew and nonpositive UpdateExisting class
  targets are documented as F60 PlanValidator rejections.
- A fresh independent x64 Ninja/MSVC 19.51/Qt 6.12 configure built
  `ClassMngrNextApplicationScheduleImportStateValidationTests` and
  `ClassMngrScheduleImportTests`; exact CTest passed 2/2. The latter includes
  `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` with
  `tests/fixtures/imports/schedule_review.xlsx`. No full suite was run.
- Coverage limit: F60 already converts nonpositive class target IDs to absence
  before state validation, which does not inspect CreateNew targets. Therefore
  CreateNew class sentinel rows verify end-to-end apply behavior but do not
  isolate F59's adapter conversion.
- F62 adds reachable action/sentinel evidence to Gates 1 and 2; both remain
  Partial. Workspace boundary and audited `src/next` dependency isolation
  remain Satisfied; Phase 2 exit gate remains Open. Sub Prep remains capped at
  the current and following calendar years (2026-2027). The protected
  `cmake/sources.cmake` file was excluded; SHA-256 remains
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: select F63 from the remaining Gate 1/2 gaps. Keep source and audit
  commits separate and preserve the protected CMake change.

### Phase 2 Schedule Import no-suggestion preview sentinel - 2026-09-26

- F63 is committed as
  `bf4251eca530066ba65b00021f63779d185bd64e` (`Phase2 - Cover Schedule Import
  missing suggestions`). In the checked-workbook production preview test, the
  first M3/Song's candidate has empty matching IDs, legacy suggestion `-1`,
  `exactMatch == false`, and confidence `None`. This covers the repository
  adapter result when the Qt-free matching projection has no suggested class;
  the app-less projection tests already assert optional absence.
- Two fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 trees
  validated 912 handwritten source owners each, built
  `ClassMngrScheduleImportTests` and
  `ClassMngrNextApplicationScheduleImportMatchingProjectionTests`, and passed
  those exact CTests 2/2 in each tree. The first target includes the checked-in
  `tests/fixtures/imports/schedule_review.xlsx` apply fixture. No full suite
  was run; `git diff --check` passed.
- F63 closes the directly observable F58 no-suggestion sentinel gap. Gates 1
  and 2 remain Partial; workspace boundary and audited `src/next` dependency
  isolation remain Satisfied; Phase 2 exit gate remains Open. Sub Prep remains
  capped at the current and following calendar years (2026-2027). Protected
  `cmake/sources.cmake` remains excluded with SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: select F64 from remaining Gate 1/2 gaps. Continue separate source and
  audit commits; preserve the protected CMake change.

### Phase 2 student name-pair identity - 2026-09-26

- F64 is committed as
  `559b4feaa8fd67c01cd2f4d0f3ddd7dc0f166de5` (`Add typed student name pairs
  to roster score import`). Added Qt-free
  `src/next/domain/student_name_pair.h` and registered it in
  `cmake/next.cmake`. The value stores independent UTF-16 fields, rejects
  either empty component, and compares/orders the exact fields. Only the
  roster score-import join now uses this value; `QString::trimmed()` remains
  at the feature boundary and `insert_or_assign` preserves last-write-wins.
- `tests/next_domain_contract_tests.cpp` covers empty halves, exact equality,
  case/internal-whitespace distinction, UTF-16 supplementary characters,
  ordering, and formerly ambiguous U+001F-separated component pairs.
  `tests/roster_editor_widget_import_tests.cpp` retains real full-pair,
  partial-pair, persistence, and idempotence coverage, adds a one-sided trim
  regression, and verifies a later duplicate legacy row overwrites an earlier
  score. Current validation rejects duplicates; the fixture directly populates
  existing in-range row 1 to represent legacy stored data.
- Executor and independent Tester each configured or used a fresh Windows
  x64 Debug Ninja/MSVC 19.51/Qt 6.12 tree. Both focused targets built and exact
  CTests `ClassMngrNextDomainContractTests` and
  `ClassMngrRosterEditorWidgetImportTests` passed 2/2. The Tester build
  reported 913 handwritten source owners. `git diff --check` passed; no full
  suite was run.
- Gates 1 and 2 remain Partial; workspace boundary and audited `src/next`
  dependency isolation remain Satisfied; Phase 2 exit remains Open. Sub Prep
  remains capped to 2026-2027. Protected `cmake/sources.cmake` stayed excluded
  at SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: compare remaining Gate 1 and Gate 2 gaps for F65. Keep source and
  documentation commits separate and preserve the protected user change.

### Phase 2 legacy profile migration through workspace open - 2026-09-26

- F65 is committed as
  `a4fbffb91228ab1d783ac782ff572d49d3c28b65` (`Cover legacy profile migration
  through FileController`). Only
  `tests/file_controller_workspace_lifecycle_tests.cpp` changed. The new test
  materializes the checked-in `legacy_startup.sql` fixture into a temporary
  `.db` and opens it through FileController and WorkspaceCoordinator.
- The test reads teacher, class, ClassInfo, and schedule values through the
  production services and asserts the normalized active path. The legacy
  nonpositive teacher reference is repaired to SQL NULL; the service maps the
  unassigned relationship to `-1`. The active profile reaches
  `DatabaseSchemaManager::LatestSchemaVersion` (6). Migration retains
  `.pre-schema-v4-backup`, whose stored schema version is 3 and whose class
  teacher link is already repaired.
- Executor and independent Tester each configured a fresh Windows x64 Debug
  Ninja/MSVC 19.51/Qt 6.12 tree, validated 913 handwritten source owners, built
  `ClassMngrFileControllerWorkspaceLifecycleTests` and
  `ClassMngrDatabaseSchemaManagerTests`, and passed exactly those CTests 2/2.
  `git diff --check` passed; no full suite was run. Protected
  `cmake/sources.cmake` remained excluded with SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- F65 adds Gate 2 legacy `.db` production-open parity evidence; Gate 2 remains
  Partial and Gate 1 remains Partial. Workspace boundary and audited
  `src/next` dependency isolation remain Satisfied; Phase 2 exit remains Open.
  Sub Prep remains capped at 2026-2027. Next: select F66 from the remaining
  gate gaps; keep source and documentation commits separate.

### Phase 2 teacher display-name precedence - 2026-09-26

- F66 source/test commit: `9afa17f47aadb7188916cc091e370f5d0bea98bb`
  (`Extract Sub Prep teacher display name rule`). Added the Qt-free
  `src/next/domain/teacher_display_name.h` and registered it in
  `cmake/next.cmake`. It owns the selected exact UTF-16 name in legacy order:
  preferred name, English, preferred romanization, Korean.
- The Sub Prep schedule-summary and class-details adapters trim their Qt
  inputs at the platform boundary and use the shared Domain rule. Summary
  retains `N/A` for no name; class details retains an empty value. Added
  app-less precedence/empty/copy/non-ASCII tests and production adapter
  coverage for padded preferred names and both empty fallbacks. No range logic
  changed.
- Executor tree `build/phase2-f66-teacher-display-name-executor-20260926` and
  independent Tester tree
  `build/phase2-f66-teacher-display-name-tester-20260926-independent-01`
  each used a fresh Windows x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12 build,
  validated 914 handwritten source owners, built
  `ClassMngrNextDomainContractTests` and
  `ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and
  passed those exact CTests 2/2. No full suite was run. `git diff --check`
  passed.
- Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next`
  dependency isolation remain Satisfied; Phase 2 exit gate remains Open. Sub
  Prep remains capped at the current and following years (2026-2027). The
  protected user change `cmake/sources.cmake` stayed excluded with SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: select F67 from the remaining Phase 2 gaps after the three independent
  Investigator reviews. Keep source and documentation commits separate.

### Phase 2 Schedule Import CreateNew target consistency - 2026-09-26

- F67 source/test commit: `4081cc0fcbfb504766e8e10f98839f9eeffbf6ce`
  (`Reject targeted Schedule Import CreateNew state`). The application state
  validator now returns typed `CreateNewClassHasTarget` when CreateNew carries
  a class ID; targetless CreateNew still passes. The repository maps the new
  error to a translated message. Direct app-less tests cover target ID 42 and
  the valid absent target. The existing ReviewDecisions contract continues to
  reject the same combination.
- The focused production Schedule Import CTest was rerun as a regression
  guard. Normal plan validation rejects this input upstream, so F67 does not
  directly exercise the new state-validation branch through the production
  import workflow and does not add new Gate 2 parity evidence. F62's distinct
  CreateNew adapter conversion observability limit remains.
- Executor tree `build/phase2-f67-create-new-state-target-executor-20260926`
  and independent Tester tree
  `build/phase2-f67-create-new-state-target-tester-20260926-independent-01`
  each used fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds, validated
  914 handwritten source owners, built and passed exactly these CTests 3/3:
  `ClassMngrNextApplicationScheduleImportStateValidationTests`,
  `ClassMngrNextApplicationScheduleImportReviewDecisionsTests`, and
  `ClassMngrScheduleImportTests`. No full suite was run; `git diff --check`
  passed.
- Gate 1 gains direct app-less state-validation evidence and remains Partial.
  Gate 2 remains Partial with existing production evidence revalidated;
  workspace boundary and audited `src/next` dependency isolation remain
  Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at the current
  and following calendar years (2026-2027). The protected user change
  `cmake/sources.cmake` stayed excluded at SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: select F68 from the remaining gates with three independent
  Investigator reviews. Keep source and documentation commits separate.

### Phase 2 calendar campus visibility - 2026-09-26

- F68 source/test commit: `3ee0b1c6` (`Extract Qt-free calendar campus
  visibility policy`). Added the Qt-free
  `Application::CalendarEventCampusVisibilityPolicy`; the existing Qt feature
  adapter retains `QString` trimming, campus-code normalization, one-to-one
  case folding, and the legacy typed-summary and `CalendarEvent` entry points.
- App-less policy tests cover default-visible cases, exact campus identity,
  literal punctuation, token boundaries including `S2`/`S20`, and lower-case
  neighbors. Feature-adapter tests cover the Kelvin sign U+212A and dotless i
  U+0131 boundary behavior. These Unicode cases are targeted regressions, not
  an exhaustive equivalence proof for all Qt regex case folding.
- Executor tree
  `build/phase2-f68-calendar-campus-policy-executor-recheck-20260926-01` and
  independent Tester tree
  `build/phase2-f68-calendar-campus-policy-tester-20260926-independent-01`
  each used a fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 build,
  validated 915 handwritten source owners, and built and passed exactly
  `ClassMngrNextApplicationCalendarEventTests`,
  `ClassMngrCalendarEventCacheTests`, and `ClassMngrCalendarImportTests`
  (3/3). `git diff --check` passed; no full suite was run.
- Gate 1 gains direct app-less policy evidence and remains Partial. Gate 2
  remains Partial; the adapter checks do not provide additional checked-in
  baseline fixture parity. Workspace boundary and audited `src/next`
  dependency isolation remain Satisfied; Phase 2 exit gate remains Open. Sub
  Prep stays capped at the current and following calendar years (2026-2027).
  Protected user change `cmake/sources.cmake` remains excluded at SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- Next: select F69 from three independent Investigator reviews. Keep source
  and documentation commits separate.

### Phase 2 Schedule Import shared state projection - 2026-09-26 (F69)

- Source/test commit: `95aaefa4` (`Extract Schedule Import state projection`).
  The Qt-free `schedule_import_state_projection.h` owns the final set of
  schedule rows, their order, typed existing-class/candidate references, and
  whether persistence replaces or keeps existing rows. Repository apply uses
  one projection vector for overlap validation and persistence; generated
  class IDs are resolved only after inserts. Intensive UpdateExisting checks
  untouched existing schedules for overlap but leaves their persisted row IDs
  unchanged.
- Changed source/test files: `cmake/next.cmake`,
  `src/data/repositories/schedule_import_repository.cpp`,
  `src/next/application/schedule_import_state_projection.h`,
  `src/next/application/schedule_import_state_validation.h`,
  `tests/next_application_schedule_import_state_validation_tests.cpp`, and
  `tests/schedule_import_tests.cpp`.
- Executor tree `build/f69x64` and independent Tester tree
  `build/f69_projection_independent_verification` each used a fresh Windows
  x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12 build, validated 916 handwritten
  source owners, and passed exactly
  `ClassMngrNextApplicationScheduleImportStateValidationTests` and
  `ClassMngrScheduleImportTests` (2/2). These suites assert persisted
  `schedule_review.xlsx` rows, checked-in overlap rejection before writes,
  intensive replacement/preservation including untouched row identity, skip
  preservation, and rollback. `git diff --check` passed; no full suite was run.
- During verification, a `QCOMPARE` around a braced `QStringList` exposed its
  commas to the macro. The assertion now builds a local `QStringList` before
  comparing; both fresh trees passed afterward.
- Gate 1 gains app-less projection behavior and Gate 2 gains checked-in
  fixture-backed persisted-row parity; both remain Partial. Workspace
  boundary and audited `src/next` isolation remain Satisfied. Phase 2 exit
  gate remains Open. Sub Prep is still limited to the current and following
  calendar years (2026-2027). Protected `cmake/sources.cmake` stayed excluded
  at SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- F69 source/test and separate documentation commits are complete
  (`95aaefa4` and `4140b6b1`). F70's bounded work and verification are recorded
  below.

### Phase 2 Class Transfer matching policy - 2026-09-26 (F70)

- Source/test commit: `2f3d414c` (`Extract Class Transfer preview matching
  policy`). Added Qt-free `Application::matchClassTransferCandidates` in
  `src/next/application/class_transfer_matching_policy.h`. The repository
  adapter keeps Qt whitespace simplification and case folding, snapshots
  database candidates, delegates the rule, then maps typed domain IDs to the
  legacy integer preview.
- Changed source/test files: `cmake/next.cmake`,
  `src/next/application/class_transfer_matching_policy.h`,
  `src/data/repositories/class_transfer_repository.cpp`,
  `src/next/application/class_transfer_projection.h`,
  `tests/next_application_class_transfer_tests.cpp`, and
  `tests/class_transfer_tests.cpp`.
- App-less tests cover both-name and single-name teacher matching, empty-name
  rejection, class course and teacher identity, assigned-but-unloaded teacher
  placeholders, unassigned fallback, and result ordering. Production adapter
  coverage verifies whitespace and ASCII case normalization. The checked-in
  `success_source.json` and `conflict_source.json` paths assert preview IDs;
  the conflict path rejects a schedule collision without changing destination
  data. Unicode case-fold equivalence is not exhaustively tested.
- Executor tree
  `build/f70-class-transfer-preview-application-msvc-ninja-x64` and
  independent Tester tree
  `C:\Users\wfelt\AppData\Local\Temp\phase2-f70-transfer-matching-tester-20260926`
  each used a fresh Windows x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12 build,
  validated 917 handwritten source owners, and built and passed exactly
  `ClassMngrNextApplicationClassTransferTests` and
  `ClassMngrClassTransferTests` (2/2). `git diff --check` passed; no full suite
  was run. Optional Vulkan/vswhere and long-path warnings did not affect the
  independent build or tests.
- Gate 1 gains direct app-less matching-policy evidence. Gate 2 revalidates
  checked-in preview and conflict no-write behavior through the moved rule;
  both remain Partial. Workspace boundary and audited `src/next` isolation
  remain Satisfied; Phase 2 exit remains Open. Sub Prep stays capped at the
  current and following calendar years (2026-2027). Protected
  `cmake/sources.cmake` stayed excluded at SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- F70 source/test and separate plan/mapping/deployment documentation commits
  are complete (`2f3d414c` and `91d01551`). F71's implementation and evidence
  follow below.

### Phase 2 Class Transfer typed review identities and replacement parity - 2026-09-26 (F71)

- Source/test commit: `9b090cb5` (`Type Class Transfer review decision
  identities`). `ClassTransferReviewDecisionRequest` now stores candidate IDs
  and issue targets as typed `Domain::ClassId`/`Domain::TeacherId`; selected
  targets use `std::optional` rather than an integer sentinel. The Application
  validator keeps action, membership, duplicate replacement, and missing-choice
  rules app-less. Dialog and repository adapters retain legacy integer UI and
  plan APIs, convert positive IDs, map exactly `-1` to absence, and reject
  other nonpositive IDs with the existing action-specific messages and error
  precedence.
- Changed source/test files: `src/next/application/class_transfer_projection.h`,
  `src/features/classes/ui/class_import_dialog.cpp`,
  `src/data/repositories/class_transfer_repository.cpp`,
  `tests/next_application_class_transfer_tests.cpp`, and
  `tests/class_transfer_tests.cpp`.
- App-less tests assert typed categories, absence, typed issue identity, and
  existing validation rules. The checked-in `success_source.json` path seeds
  matching destination teacher/class rows and applies dialog-selected Replace.
  It asserts the retained destination ClassId and teacher profile, replaced
  class details/schedule/roster, and cleared old evaluation rows. The existing
  checked-in Create fixture and `conflict_source.json` no-write behavior remain.
  Direct dialog checks cover class targets 0/-2 and teacher targets 0/-2 for
  both available actions; repository checks cover class Create/Replace and
  teacher Create/Keep with 0/-2 plus invalid-action precedence.
- Executor tree `build/phase2-f71-typed-class-review-msvc-ninja-x64` and
  independent Tester tree
  `C:\Users\wfelt\AppData\Local\Temp\phase2-f71-typed-review-independent-20260926-v3`
  used separate fresh Windows x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12 builds.
  Each validated 917 handwritten source owners and passed exactly
  `ClassMngrNextApplicationClassTransferTests` and
  `ClassMngrClassTransferTests` (2/2). The independent Tester reran after the
  final dialog assertions. `git diff --check` passed; no full suite was run.
  An initial Ninja preset invocation failed because it supplied an unsupported
  x64 platform; direct Ninja configuration under the Visual Studio developer
  environment succeeded.
- Gate 1 gains a typed app-less review-decision contract and Gate 2 gains
  fixture-backed successful replacement parity; both remain Partial. Workspace
  boundary and audited `src/next` isolation remain Satisfied; Phase 2 exit
  remains Open. Sub Prep stays capped at current and following years
  (2026-2027). Protected `cmake/sources.cmake` stayed excluded at SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- F71 source/test and separate plan/mapping/deployment documentation commits
  are complete (`9b090cb5` and `5a9fe695`). F72's test and verification follow.

### Phase 2 synthetic Intensive workbook path - 2026-09-26 (F74)

- Source/test commit: 3e0a8d64 (Verify synthetic intensive schedule import flow).
  Added source-readable authored worksheet cells at
  tests/fixtures/imports/schedule_intensive_synthetic_worksheet.xml and
  previewsAndAppliesSyntheticIntensiveWorkbookAgainstSeededDatabase in
  tests/schedule_import_tests.cpp. The test parses them as Intensive, checks
  fixed candidate and preview expectations, selects UpdateExisting, and applies.
- Assertions verify persisted target Intensive rows, retained class identity
  and no class creation, unchanged ID/value snapshots for an untouched
  Intensive row, and unchanged regular schedule rows. Existing workbook,
  direct-plan Intensive, ReplaceWithNew, conflict, and rollback coverage remain.
- Executor used build/phase2-f69-schedule-state-projection-executor-ninja-msvc-20260926
  (Windows x64 Debug Ninja/MSVC 14.51.36231/Qt 6.12.0). Independent Tester
  used a fresh tree at
  C:\Users\wfelt\AppData\Local\Temp\f74_intensive_verify_20260926
  (Windows x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12.0). CMake validated 917
  handwritten source owners; all three targets built and passed CTest 3/3:
  ClassMngrScheduleImportTests,
  ClassMngrNextApplicationScheduleImportStateValidationTests, and
  ClassMngrNextApplicationScheduleImportMatchingProjectionTests.
- Exact focused commands:
  cmake --build <build-dir> --target ClassMngrScheduleImportTests ClassMngrNextApplicationScheduleImportStateValidationTests ClassMngrNextApplicationScheduleImportMatchingProjectionTests
  and
  ctest --test-dir <build-dir> -R "ClassMngrScheduleImportTests|ClassMngrNextApplicationScheduleImportStateValidationTests|ClassMngrNextApplicationScheduleImportMatchingProjectionTests" --output-on-failure.
- git diff --check passed; no full suite was run. CMake emitted unrelated
  object-path/Vulkan-header warnings; both focused builds succeeded. F74 is
  synthetic production-flow coverage only; the repo has no historical
  Intensive workbook paired with a legacy-output oracle, so this does not close
  baseline parity. Gate 2 remains Partial; Gate 1 is unchanged. Workspace
  boundary and audited src/next isolation remain Satisfied; Phase 2 exit
  remains Open. Sub Prep remains capped at current and following years
  (2026-2027).
- The protected cmake/sources.cmake SHA-256 remains
  9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF.
  Git status marks it modified even though the Tester confirmed the worktree
  hash and index/blob match the baseline; leave it untouched and unstaged.
- F74 source/test and separate documentation commits are complete (`3e0a8d64`
  and `55581417`). F75 source/test commit `3139bdf4` extracts ordered exact
  typed duplicate-pair grouping into the existing Qt-free `StudentNamePair`
  header and adapts shared, roster, and speaking-evaluation validation.
- Executor passed four focused CTests; the independent fresh Tester passed the
  same four plus `ClassMngrRosterEditorWidgetImportTests` (5/5 total). The
  independent tree used Windows x64 Debug, CMake 4.4.2, Ninja 1.13.2, Qt 6.12.0,
  and MSVC 19.51.36257. CMake validated 917 handwritten source owners and
  `git diff --check` passed. No full suite was run.
- F75 adds Gate 1 app-less behavior evidence; Gate 1 and Gate 2 remain Partial,
  with no new historical baseline parity. Workspace boundary and audited
  `src/next` isolation remain Satisfied; Phase 2 exit remains Open. Sub Prep
  stays capped at 2026-2027. The protected `cmake/sources.cmake` remains
  excluded at SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- F76 is selected: add a populated speaking evaluation to the checked-in Class
  Transfer success fixture and assert its full persisted row through the
  fixture-driven review/apply path. This is checked-fixture regression
  coverage, not historical baseline parity.

### Phase 2 Class Transfer speaking-evaluation fixture regression - F76

- Source/test commit `7a8b80c6` adds one named 11-column Fixture Evaluation
  row to `tests/fixtures/transfers/success_source.json`. Create/apply and
  replacement paths compare all 25 persisted rows against the literal expected
  first row plus blank remainder; replacement also retains the destination-only
  evaluation-clearing assertion.
- Executor and independent fresh Windows x64 Debug builds used CMake 4.4.2,
  Ninja 1.13.2, Qt 6.12.0, MSVC 19.51.36257; CMake validated 917 source owners
  and `ClassMngrClassTransferTests` passed 1/1. `git diff --check` passed; no
  full suite. CMake reported optional Vulkan-header/object-path warnings and a
  non-blocking `vswhere.exe` setup message.
- F76 is checked-fixture regression only: the fixture has no historical export
  provenance or independent legacy-output oracle. Gate 2 gains checked-fixture
  evidence and remains Partial; Gate 1 is unchanged and remains Partial.
  Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2
  remains Open. Sub Prep remains capped at 2026-2027; protected
  `cmake/sources.cmake` stays unstaged at the recorded SHA-256.

### Phase 2 weekly Class Transfer schedule-overlap policy - F77 verified

- Source/test commit `dd3bbc01` adds `ClassTransferScheduleCandidate` and an
  ordered, Qt-free weekly overlap policy in the existing
  `class_transfer_projection.h`. The repository adapter keeps legacy weekday
  and clock parsing, class labels, localized diagnostics, and duplicate
  rendered-message suppression.
- The app-less policy tests cover half-open/touching intervals, regular versus
  intensive separation, incoming/existing order, Sunday overnight wrap, and
  equal endpoints as 24 hours. Repository tests cover parsed Sunday overnight
  and equal-endpoint conflicts with exact diagnostics and pre-write/no-write
  assertions; the checked-in conflict fixture asserts the exact combined
  regular/intensive diagnostic and unchanged destination state.
- Independent fresh verification used a temporary snapshot based on archive
  commit `4aa2d7bd` plus the current F77 files (Windows x64 Debug, CMake 4.4.2,
  Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0). Configure validated 917
  handwritten source owners.
  `ClassMngrNextApplicationClassTransferTests` and
  `ClassMngrClassTransferTests` built and passed (2/2); `git diff --check`
  passed. Configure reported nonfatal Vulkan-header, `vswhere.exe`, and long
  object-path warnings. No full suite was run.
- F77 adds app-less Domain/Application behavior evidence to Gate 1 and
  checked-fixture conflict evidence to Gate 2; both remain Partial. The
  checked fixture is regression coverage, not an independently sourced
  historical output oracle. Workspace boundary and audited `src/next`
  isolation remain Satisfied; Phase 2 remains In Progress with its exit gate
  Open. Sub Prep remains capped at 2026-2027. Protected
  `cmake/sources.cmake` remains excluded at SHA-256
  `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.

### Phase 2 Schedule Import duplicate-target fixture coverage - F78 selected

- Use permanent `schedule_large_conflict.xlsx` in the repository apply test.
  Assign both imported classes to the same existing class, assert apply rejects
  the plan before writes, and compare seeded teacher, class, class-info,
  schedule, and settings snapshots. Keep production code unchanged unless the
  fixture path exposes a defect.
- Focused targets: `ClassMngrScheduleImportTests`,
  `ClassMngrNextApplicationScheduleImportReviewDecisionsTests`, and
  `ClassMngrScheduleImportDialogTests`. This adds required checked-fixture
  regression evidence, not an independent historical-output oracle.

### Phase 2 Schedule Import duplicate-target fixture regression - F78 verified

- Source/test commit 68ab0faa replaces the synthetic empty-candidate duplicate-target test with permanent schedule_large_conflict.xlsx coverage. It loads Current-sheet Alice E4 Hercules candidates, verifies the two candidates are distinct, directs both to one seeded existing destination, asserts the exact duplicate-target error, and compares persisted teachers, classes, class information, regular and intensive schedules, and app settings before and after rejection.
- Executor verification passed ClassMngrScheduleImportTests. Independent fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; CMake validated 917 handwritten source owners and the focused Schedule Import, review-decision, and dialog targets passed (3/3). git diff --check passed; no full suite was run. The optional external workbook sample was skipped because CLASSMNGR_SCHEDULE_IMPORT_SAMPLE was unset.
- F78 is checked-fixture regression coverage, not independent historical parity. Gate 1 and Gate 2 remain Partial; workspace boundary and audited src/next isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027. Protected cmake/sources.cmake remains excluded at SHA-256 9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF.

### Phase 2 Class Transfer validated weekly interval contract - F79 selected

- Source/test commit 947edd93 makes public app-less ClassTransferScheduleCandidate construction go through a factory from category, parsed weekday index, and start/end minute-of-day. It validates category/day/clock bounds and preserves end-at-or-before-start as next-day rollover, equal endpoints as 24 hours, and Sunday endpoints beyond the weekly boundary. The repository keeps QString parsing and exact legacy diagnostics.
- Executor fresh Windows x64 Debug configure used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; configure validated 917 handwritten source owners, all 313 build actions passed, and ClassMngrNextApplicationClassTransferTests plus ClassMngrClassTransferTests passed (2/2). Independent fresh-tree verification overlaid the protected worktree manifest at its exact SHA-256, validated 917 owners, built and passed the same two targets, and git diff --check passed. No full suite was run.
- Gate 1 gains app-less validated interval behavior and remains Partial; Gate 2 remains Partial. Workspace boundary and audited src/next isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep stays capped at 2026-2027. The protected cmake/sources.cmake remains excluded at the recorded SHA-256.

### Phase 2 Korean Teacher Import sparse-update contract - F80 verified

- Source/test commit `15cd876d` adds the Qt-free Korean sparse-update policy in `src/next/application/korean_teacher_import_update.h` and adapts `TeacherImportRepository`. The policy carries typed `TeacherId` and `KoreanTeacherKey`, validates key/name consistency, canonicalizes the imported Hangul name, preserves blank room/birthday/phone values, and identifies unchanged results. Qt Unicode trimming and legacy integer SQL identity remain at the repository edge.
- App-less tests cover mixed and all-blank updates, retained ID/key, canonicalized name, and mismatched keys. The checked `sectioned_review.xlsx` repository and dialog-plan paths seed suffix-D Hong and assert update+create counts, retained database ID, imported contact fields, every seeded manual profile field, one Hong row, no omitted candidates, and source date `2026-09-01`. The repository test also proves the all-blank import fires no teacher UPDATE trigger.
- Independent verification used a fresh archive at base commit `4abfd685` with the F80 source/test overlay, Windows x64 Debug, CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. CMake source ownership validated 919 handwritten files; all three targets built in 315 actions and passed CTest 3/3. The dialog target also passed 1/1 with both Windows and offscreen QPA. `ClassMngrTeacherImportTests` reported 16 passes and one optional external-workbook skip. `git diff --check` passed; no full suite was run. Protected `cmake/sources.cmake` remained excluded at SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
- F80 advances Gate 1 and checked-fixture Gate 2 evidence; both remain Partial. The fixture is not an independently sourced historical-output oracle. Workspace boundary and audited `src/next` isolation remain Satisfied; Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

### Phase 2 Native English Teacher Import sparse update - F81 selected

- Extract the existing Native English Teacher matched-row merge into a Qt-free Application policy. Preserve adapter-owned Qt name matching/simplification and field trimming; keep the native table's integer row ID in the repository.
- Test sparse nonblank updates, blank-field preservation, name simplification, and unchanged detection app-less. Route the matching Alex row in `sectioned_review.xlsx` through the policy and assert its persisted identity and fields with no duplicate. Focused targets: the app-less policy CTest and `ClassMngrTeacherImportTests`; include the dialog target if its plan path changes.
- This is checked-fixture regression evidence, not historical-output parity. Gate 1 and Gate 2 remain Partial. Sub Prep remains capped at 2026-2027.

### Phase 2 Native English Teacher Import sparse update - F81 verified

- Source/test commit `118baceb` adds a Qt-free Native English Teacher update policy. It carries the separate native profile values as UTF-16 strings, merges the five optional fields only when incoming values normalize to nonempty, sets the imported name from the adapter-normalized value, and detects unchanged results. Qt trimming/simplification and native-table integer row identity remain in the repository adapter.
- The checked `sectioned_review.xlsx` case imports Alex with padded name/position/birthday values and verifies normalization, retained row ID `8104`, preserved blank phone/nationality/email, no duplicate, source date `2026-09-01`, and no database update on repeated import.
- Independent fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. Configure validated 921 handwritten owners; the build completed 311 actions. `ClassMngrNextApplicationNativeEnglishTeacherImportUpdateTests` and `ClassMngrTeacherImportTests` passed (2/2); `git diff --check` passed. The optional external workbook was unset; no full suite ran.
- F81 adds app-less Gate 1 evidence; Gate 1 and Gate 2 remain Partial. Fixture coverage is not historical-output parity. Workspace boundary and audited `src/next` isolation remain Satisfied. Sub Prep remains capped at 2026-2027.

### Phase 2 Schedule Import legacy differential regression - F82 verified

- Source/test commit `6d8fb296` updates only `tests/schedule_import_tests.cpp`. A fixture-driven harness ran legacy commit `48fc5c5c` and current `118baceb` on the same seeded database and `schedule_review.xlsx`; all 14 semantic records matched: parser metadata, teacher/class preview data, apply counters `(1,0,2,1,0,2,0,false)`, and normalized persisted teacher, class, and six regular-hour rows.
- The current regression now pins the missing parser/source-cell values, teacher match IDs and affected-class counts, third-candidate no-match result, apply counters, and persisted class-to-teacher names. The fixture was added later in `f5fdcc4a`, so this is common-input differential evidence, not a workbook historically present at the baseline.
- The independent Tester built the repository's normal `ClassMngrScheduleImportTests` target from a fresh archive at `118baceb` plus the test patch. CMake 3.30.5, Ninja 1.12.1, MSVC 19.51.36257, Qt 6.12.0; 921 handwritten source owners; 309 build actions; CTest passed 1/1. `git diff --check` passed. The executor's focused QtTest run reported 51 passed, 0 failed, and one optional external-workbook skip. No full suite ran.
- F82 advances Gate 2 differential evidence but leaves it Partial; Gate 1 remains Partial. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 exit remains open. Sub Prep remains capped at 2026-2027.

### Phase 2 Schedule Import overlap-conflict differential - F83 selected

- Run `schedule_overlap_conflict.xlsx` through the legacy `48fc5c5c` and current preview/apply paths using the same seeded database. Compare preview classification, overlap rejection, and the persisted-state snapshot. Reference the legacy `conflictsRollBackBeforeWrites()` test and current `previewsAndRejectsCheckedInOverlapWorkbookBeforeWrites` test; focus on `ClassMngrScheduleImportTests`.
- The conflict workbook was added after the baseline (`3121d90c`), so treat the result as common-input differential regression evidence. This is selected work, not implementation or verification evidence. Keep Gate 1 and Gate 2 Partial until broader exit criteria pass; Sub Prep stays capped at 2026-2027.

### Phase 2 Schedule Import overlap-conflict differential - F83 verified

- Source/test commit `2e8bbab2` changes only `tests/schedule_import_tests.cpp`. Legacy `48fc5c5c` and current `1236e9cb` used identical `schedule_overlap_conflict.xlsx` bytes (SHA-256 `2de93c4abdc5e82390adede250e8313501a38d4be2053e929c4adbed6d745312`) and deterministic SQLite seed. Their parser, preview, apply-error, and normalized state transcripts matched.
- Literal assertions now pin teacher key/display values 김선생 and 이선생, imported rooms 413 and 415, preview inventory and absent class 9901, unmatched candidates, and exact rejection text: `The proposed schedule overlaps: E4 Hercules conflicts with E4 Theseus on Monday.` The before/after persisted snapshot covers teachers, classes, class info, regular and intensive schedules, intensive slot state, and app settings.
- Independent Tester used a fresh archive at `1236e9cb` plus the final test patch. Windows x64 Debug used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0; CMake validated 921 source owners, the target built in 309 actions, and `ClassMngrScheduleImportTests` passed 1/1. `git diff --check` passed. Executor QtTest output was 51 passed, 0 failed, and one optional external-workbook skip. No full suite ran.
- The fixture was introduced at `3121d90c`, after legacy baseline `48fc5c5c`; this is common-input differential coverage, not historical production-workbook parity. Gate 2 gains conflict evidence but remains Partial; Gate 1 remains Partial. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 exit stays open; Sub Prep remains capped at 2026-2027.

### Phase 2 Schedule Import typed teacher key - F84 selected

- Use `Domain::KoreanTeacherKey` for the matching key in both `ScheduleImportMatchingCandidate` and `ScheduleImportMatchingTeacherProjection`. Keep teacher display names separate and perform QString conversions at the repository edge.
- Preserve the valid empty-key behavior in `preservesEmptyTeacherKeyMatchingSemantics`, existing match ordering, room aggregation, and F82/F83 repository fixture expectations. Verify `ClassMngrNextApplicationScheduleImportMatchingProjectionTests` and `ClassMngrScheduleImportTests`. This advances Gate 1 but does not complete it; it adds no new historical-parity claim. Sub Prep remains capped at 2026-2027.

### Phase 2 Schedule Import typed teacher key - F84 verified

- Source commit `5207d65a` types `ScheduleImportMatchingCandidate::teacherKey` and `ScheduleImportMatchingTeacherProjection::teacherKey` as `Domain::KoreanTeacherKey`. The repository adapter owns QString conversion; display names remain separate. Explicit initialization preserves a valid empty key, and the typed comparison keeps existing empty-key matching behavior.
- Executor and independent fresh Tester passed `ClassMngrNextApplicationScheduleImportMatchingProjectionTests` and `ClassMngrScheduleImportTests` (2/2). Fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0, 921 handwritten source owners, and 313 build actions. `git diff --check` passed; no full suite ran.
- Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep stays capped at 2026-2027.

### Phase 2 Teacher Import full-plan validation - F85 selected

- Extract the Teacher Import plan's deterministic validation into a Qt-free Application policy. Keep Qt name normalization, date interpretation, translation, SQL, and repository transaction behavior at the adapter; preserve validation precedence and exact existing diagnostics.
- Supply normalized identity keys and date validity to the policy, retain the existing Qt-free review-decision resolver, and test policy outcomes app-less plus repository message mapping and rejection-before-write behavior. Focused targets are the new policy CTest and `ClassMngrTeacherImportTests`. This adds Gate 1 evidence; it does not claim historical-output parity. Sub Prep remains capped at 2026-2027.

### Phase 2 Teacher Import full-plan validation - F85 verified

- Source/test commit `d5971ae1` adds `TeacherImportPlanValidationInput` and a stable issue enum in a Qt-free Application header. The adapter runs existing review-choice resolution first, projects the resolved ordered Korean keys into the policy input, then maps policy issues to the exact existing translated diagnostics. Qt name normalization, `QDate` interpretation, UTF-8 conversion, SQL, and transactions remain at the repository boundary.
- App-less tests cover valid plans, review count/order mismatch, review-before-date and category precedence, date validity, missing/duplicate Korean and Native English names, missing/duplicate GS Team names, and separate GS language namespaces. Repository tests pin exact messages and verify rejected plans preserve teacher/native/GS row counts and the latest source date.
- Executor and independent fresh-archive verification passed `ClassMngrNextApplicationTeacherImportPlanValidationTests` and `ClassMngrTeacherImportTests` (2/2). The independent Windows x64 Debug tree used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; CMake validated 923 owners and Ninja completed 311 actions. The initial Visual Studio generator attempt hit an MSBuild FileTracker access denial in the temporary archive; reconfiguring with Ninja succeeded. Long-path and optional Vulkan-header warnings were nonfatal. `git diff --check` passed; no full suite ran.
- F85 advances Gate 1, which remains Partial; Gate 2 remains Partial without new baseline-parity evidence. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep stays capped at 2026-2027.

### Phase 2 Schedule Import generated baseline scenario - F86 verified

- Source/test commit `c3029f14` adds `previewsAndAppliesBaselineGeneratedWorkbookAgainstSeededDatabase` to `tests/schedule_import_tests.cpp`. It pins `scheduleWorkbookData()` at 5,352 bytes with SHA-256 `24cf273fe36278747c811ad04bb5cbf4a32a2970f50fc6abed394b5809d48b49` and asserts parsed sheets, Alice's E5 Zeus candidate from B2/D2, teacher 17 preview match, no class match/suggestion, apply counters, and the normalized state of teachers, classes, class metadata, regular/intensive schedule tables, slot states, and settings.
- Legacy `48fc5c5c` and current `31057c00` used identical generated bytes and deterministic seed and produced identical semantic transcripts (SHA-256 `8A00E7A05FBA1E86980EF324A946D9BCAD81916B8B5833856537FD94F0181DA8`). Apply creates class 41, acknowledges one ignored cell, clears one schedule, retains unrelated teacher/class metadata/settings, clears the old class 40 Friday regular time, and persists the new Monday/Wednesday times. This is source-generated synthetic baseline evidence, not historical production-workbook parity.
- Executor and independent fresh-base Tester passed `ClassMngrScheduleImportTests` (1/1). The independent CMake 4.4.2/Ninja 1.13.2/MSVC 19.51.36257/Qt 6.12.0 configure validated 923 owners; the target built 309 steps and CTest passed 1/1 in 1.12 seconds. `git diff --check` passed; no full suite ran. The legacy harness used a local Qt version-metadata override only; no project configuration changed.
- Gate 2 gained one source-generated baseline comparison but remains Partial; Gate 1 remains Partial. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

### Phase 2 GS Team sparse update policy - F87 verified

- Source/test commit `b97f81be` adds `GsTeamImportProfile`, `GsTeamImportFields`, `GsTeamImportUpdate`, and `mergeGsTeamImport` in `src/next/application/gs_team_import_update.h`. The policy uses standard-library strings, retains the stored row ID, preserves blank fields, and returns a `changed` flag. The repository keeps Korean-name/English-fallback match selection, ambiguity rejection, Qt normalization, SQL, counters, diagnostics, and transactions.
- App-less tests cover mixed sparse updates, blank preservation, stable ID, and unchanged results. Repository coverage verifies Korean matching, English fallback, one create/update/unchanged count, persisted values and IDs, the existing ambiguous-match diagnostic, and an UPDATE trigger that fires only for the changed row.
- Executor and independent fresh-base Tester passed `ClassMngrNextApplicationGsTeamImportUpdateTests` and `ClassMngrTeacherImportTests` (2/2). The Tester overlaid only six F87 paths on `81f1c625`, excluding `cmake/sources.cmake`; CMake 4.4.2/Ninja 1.13.2/MSVC 19.51.36257/Qt 6.12.0 configured and validated 925 source owners. Both executables linked and focused CTest passed 2/2. `git diff --check` passed. Nonfatal warnings covered missing `vswhere.exe`, optional Vulkan headers, and long object paths. No full suite ran. The protected manifest hash remains `bf3afbe30c77e30be83df98434eda3ee466084b0`.
- F87 adds GS Team sparse merge/no-op behavior to Gate 1, which remains Partial; Gate 2 remains Partial. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

### Phase 2 Schedule Import intensive generated-baseline comparison - F88 verified

- Source/test commit `4442726b` updates only `tests/schedule_import_tests.cpp`. It uses baseline-era `singleSheetWorkbookData()` and inline intensive worksheet data from `convertsIntensiveTimesAcrossNoon()`, with deterministic seed and identical 2,359-byte workbook input on legacy `48fc5c5c` and current code (SHA-256 `228fc2ce924f2fd4ee340500c92178386868bc83231b076b74e081dd628b93b4`). Their 65-slot persisted transcript matched (SHA-256 `7fdba13a48788441556050e17b6d0b5ca52b2b9df5a6d0c357815a6458f4a5cc`): 62 empty, two essay, and one lunch states.
- The regression pins parsed intensive candidates, teacher/class preview match and suggestion, apply counters, intensive persisted schedule and slot states, regular schedules, unrelated class/teacher data, metadata, and settings. This is source-generated synthetic baseline evidence, not historical production-workbook parity.
- Executor and independent fresh-base Tester passed `ClassMngrScheduleImportTests` (1/1). The independent Windows x64 Debug build used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; CMake validated 925 source owners and CTest passed in 1.08 seconds. `git diff --check` passed. No full suite ran. The protected `cmake/sources.cmake` manifest was excluded from the fresh archive and remains at SHA-256 `bf3afbe30c77e30be83df98434eda3ee466084b0`.
- Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

### Phase 2 Teacher Import match-cardinality policy - F89 verified

- Source/test commit `98968408` adds `TeacherImportMatchCardinality` in `src/next/application/teacher_import_match_cardinality.h` and integrates it in all three repository import branches. The policy classifies counts 0, 1, and 2+; normalization, candidate scans/order, GS Team key preference, selected identity, localized diagnostics, SQL, and transaction/rejection behavior remain in the adapter.
- App-less tests cover 0, 1, 2, and 9. Repository cases pin the Korean and Native English ambiguity diagnostics and rollback/no-write behavior, and preserve the exact GS Team ambiguity diagnostic and rejection behavior.
- Independent fresh-base verification used archive `36b300d2` plus only six F89 paths, excluding the protected manifest (base blob `bf3afbe30c77e30be83df98434eda3ee466084b0`). Updated `tests/teacher_import_tests.cpp` was blob `3ae707417e22f9c16dbda48192dbb3fdd398a806`, SHA-256 `9DDA9F8283CCDB3BB865E66E085E0937F53664D41B5175D2DF9CF1B74CB240AD`. CMake 4.4.2/Ninja/MSVC 19.51.36257/Qt 6.12.0 configured Windows x64 Debug and validated 927 owners; both focused targets built and filtered CTest passed 2/2. `git diff --check` passed. Nonfatal warnings were the `vswhere.exe` message, missing optional Vulkan headers, and unrelated long paths. No full suite ran.
- F89 adds a small app-less policy and adapter regression to Gate 1, which remains Partial; Gate 2 remains Partial with no new parity claim. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep stays capped at 2026-2027.

### Phase 2 Teacher Import apply use case - F90 verified

- Source/test commit `df8ef0cb` adds `TeacherImportUseCase`, which owns review resolution, plan validation, match cardinality, Korean/Native English/GS Team matching and mutation decisions, counts, latest-source-date advancement, and transaction decisions. A single persistence port binds reads, row writes, latest-date setting, commit, and rollback. The repository retains Qt normalization, SQL, localized diagnostics, and transaction implementation.
- App-less fake-port coverage exercises orchestration and rejection/failure paths. Repository tests preserve raw Native English and GS diagnostic labels, padded-label normalization, and rollback when the latest-date write fails after writes to all three namespaces.
- Executor focused CTest passed 2/2. Independent verification overlaid only six F90 paths on `ca72c47e`, excluded the protected manifest (base blob `bf3afbe30c77e30be83df98434eda3ee466084b0`), validated 929 source owners, built the focused targets, and passed CTest 3/3 on Windows x64 Debug with CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. `git diff --check` passed; no full suite ran.
- Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep remains capped at 2026-2027.

### Phase 2 Teacher Import source-generated baseline differential - F91 verified

- Source/test commit `e814b4fc` strengthens `TeacherImportTests::importsIntoSeparateTablesAndPreservesManualFields`. It pins Korean/Native English/GS Team result counts, every non-ID field in each single-row table snapshot, Alex's preserved manual phone/birthday/nationality/email and updated `Team Leader` position, and the latest date remaining `2026-07-09` after importing older `2026-01-01`. Existing duplicate/no-write, ambiguity, and rollback coverage remains.
- Executor and independent current-tree verification passed `ClassMngrTeacherImportTests` CTest 1/1; the independent executable reported 20 passed, 0 failed, 1 optional external-sample skip. A narrow harness ran the same hand-built plan and deterministic seed against legacy `48fc5c5c` and current F90 repository code. Their semantic JSON matched at SHA-256 `755A2B7DF52B3DD8F111ADF2EC0D0127415689B2B08A68D008505079D887B2B6`. The final test patch was Git blob `786770ca816b40f00c7626461ed58ec0a3e66c22`, SHA-256 `8BB8206A59D27E09E1C82BE918445670B32220F2CB4FD8011041646CD2BA0720`.
- Because the current test file references `next/application/import_review_session.h`, absent at the old baseline, the legacy comparison used a narrow harness while retaining baseline repository/schema/application sources; no current production code was overlaid. The hand-built plan is source-generated synthetic evidence, not workbook or historical-production-workbook parity. Windows x64, MSVC 19.51, Ninja, Qt 6.12.0; `git diff --check` passed. No full suite ran.
- Two independent exit audits confirm Gate 1 and Gate 2 remain Partial; workspace formal acceptance and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

### Phase 2 Teacher Import generated-workbook parse-to-apply differential - F92 verified

- Source/test commit `3f6ef73d64c81b8e6e5f6ee665dc85e9b976408e` updates only `tests/teacher_import_tests.cpp`. The baseline-present helper `testWorkbookData()` generates 3,422 bytes (SHA-256 `9cdccb43d7fe5e5e1abb83630ede8b18e6dd2c4824dbb288dc81d60371496daa`). Validation/parsing, explicit selection of the sole Korean M1 candidate, plan creation, repository apply, and seeded-state assertions run on legacy `48fc5c5c` and current code; the semantic transcript SHA-256 is `095d595311aaee2444d67a893d45a2d0f97fe366c2a667f90bdd690205a2bdb4`.
- The generated workbook is synthetic, not historical production-workbook evidence. Independent fresh focused verification passed `ClassMngrTeacherImportTests` CTest 1/1; its executable reported 21 passed, 0 failed, and 1 skipped optional external-sample case. The selected case passed 3 assertions. `git diff --check` passed; no full suite ran.
- Two independent post-F92 audits agree that Gate 1 and Gate 2 remain Partial, the formal workspace boundary remains Satisfied, and audited `src/next` isolation remains Satisfied. Phase 2 exit remains Open.

### Phase 2 Calendar start-of-term classification - F93 verified

- Source/test commit `c73f1469` adds `CalendarEventStartOfTermPolicy` in `src/next/application/calendar_event_start_of_term_policy.h`, registers it in `cmake/next.cmake`, routes both Calendar UI consumers through it, and removes the unused legacy classifier. The import assertion now calls the Application policy. It preserves the four aliases, title case/whitespace behavior, exact known-type normalization with unknown values mapped to `Other`, and hide preference semantics.
- App-less tests cover every alias, mixed case, ASCII and Unicode whitespace including U+0085/NEL in titles and event types, known/unknown types, hybrid near-misses, and hide off/on. Independent fresh verification used base `c999a235` plus the seven F93 paths and did not overlay the protected manifest. A Qt 6.12 probe matched legacy behavior for both NEL boundaries. Windows x64 Debug Ninja with MSVC 19.51.36257, CMake 4.4.2, Ninja 1.13.2, and Qt 6.12.0 built four targets, including `ClassMngr.exe` (358 actions); focused CTest passed 3/3. `git diff --check` passed. No full suite ran.
- Two independent post-F93 audits agree Gate 1 and Gate 2 remain Partial, the written workspace criterion remains Satisfied, and audited `src/next` isolation remains Satisfied. Phase 2 exit remains Open.

### Phase 2 Teacher Import invalid-date validation parity - F94 verified

- Source/test commit `f99d155f636d273269d805531f7ef7db7be84bed` adds `rejectsGeneratedWorkbookWithInvalidDate` to `tests/teacher_import_tests.cpp`, pinning the generated input at 3,423 bytes with SHA-256 `256b29c2f27bfe787007aaa6df28e5e084dc3788863cbf4b6a09fb265f0685d0`. Assertions cover `RecognizedButInvalid`, template `sectioned-contact-list-v1`, invalid source date, discovered `M1`, and the exact A1 date diagnostic.
- The narrow legacy/current harness used each revision's own validator, registry, sectioned template, and workbook reader over the identical input. Semantic JSON matched exactly at SHA-256 `8eee9375c47b3602b86e893092441f450c74860e2bb0f3edc53642a06322cbd9`; both saved executables were independently rerun and matched. Legacy baseline: `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`.
- Focused current `ClassMngrTeacherImportTests` QTest passed 3/3, and filtered CTest passed 1/1. `git diff --check` passed; no full suite ran. This negative validation path does not establish repository no-write or rollback parity. Gate 1 and Gate 2 remain Partial; the formal workspace criterion and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

### Phase 2 Calendar repeat occurrence planning - F95 verified

- Source/test commit `7865963815efa256b797b85af27b9a33107e8f70` adds the Qt-free `CalendarEventRepeatFrequency` and `planCalendarEventRepeatOccurrences`, registers the header in `cmake/next.cmake`, and routes the Calendar UI through `ApplicationServicesCalendarEventSeriesCreatePort`. The four committed paths are the planner header, `cmake/next.cmake`, `src/features/calendar/ui/calendar_page_events.cpp`, and `tests/next_application_calendar_event_tests.cpp`.
- The planner validates seed and until dates, supports Gregorian dates in years 0001-9999, advances monthly from the previous occurrence with month-end clamping, includes occurrences through the inclusive start-date cutoff, preserves fixed day duration and save fields, clears occurrence IDs, and enforces the 366-occurrence cap. Invalid dates, ranges, frequency, and end-date overflow return structured errors.
- Independent verification rebuilt `ClassMngrNextApplicationCalendarEventTests` and `ClassMngr`; focused CTest passed 1/1 and `git diff --check` passed. Coverage includes daily/weekly, leap/non-leap monthly chains, date bounds, inclusive cutoff, duration/field/ID behavior, invalid cases, and 366/367 limits. No full suite ran.
- Gate 1 advances but remains Partial; Gate 2 remains Partial. The workspace criterion and audited `src/next` isolation remain Satisfied. Historical production-workbook evidence, repository rollback parity, and broader migrations remain open. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

The only remaining worktree change is the pre-existing `cmake/sources.cmake` status; it remains untouched and excluded from the F95 commit.

### Phase 2 Teacher Import rollback parity - F96 verified

- Baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current `7865963815efa256b797b85af27b9a33107e8f70` were archived and independently verified byte-for-byte against their Git trees. The same external CMake harness compiled each revision's own `teacher_import_repository.cpp`, `database_schema_manager.cpp`, `database_transaction.cpp`, and `sql_query_utils.cpp`; no current production source was overlaid onto baseline.
- The shared harness used a fresh in-memory SQLite schema, one synthetic plan row in each of the Korean, Native English, and GS Team teacher namespaces, and a trigger rejecting insertion of `teacher_import/latest_source_date`. Both runs failed at `latest_date_write` and left zero rows in all four affected tables/settings. Normalized JSON matched exactly at SHA-256 `b53d70bf33fa0e20451a2582eb8e1a6f904e5907196c6fad4696543b95b230f3`.
- Independent fresh Release/Ninja builds and runs passed for both archives with CMake 4.4.2, MSVC 19.51, and Qt 6.12.0. The same harness hashes were confirmed by the tester. No full suite ran. This is one synthetic database rollback case, not workbook processing, historical production-workbook parity, or coverage of other failure stages.
- Gate 2 gains one rollback-parity case but remains Partial; Gate 1 remains Partial. Workspace criterion and audited `src/next` isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

The only remaining worktree change is the pre-existing `cmake/sources.cmake` status; it remains untouched and excluded.

### Phase 2 Calendar repeat-series suffix-edit planning - F97 verified

- Source/test commit `36ebb09a960fa82f633701ec83fba35bcd7f3599` adds `src/next/application/calendar_event_series_edit_plan.h`, registers it in `cmake/next.cmake`, routes `src/next/platform/application_services_calendar_event_series_edit_port.h` through the Qt-free planner, and adds app-less plus adapter tests.
- The planner preserves ordered suffix rows and IDs, shifts every selected start by the common selected-to-edited offset, assigns each the requested duration, propagates title/type/status/all-day/times and trimmed series ID, and returns Technical errors for invalid source or shifted dates. The platform adapter retains query, Qt conversions, save, and error mapping. App-less cases cover field, ID, order, empty input, and date-boundary behavior; platform regressions cover empty suffix success and no persistence on invalid-date/overflow failures.
- Independent fresh Ninja verification built `ClassMngrNextApplicationCalendarEventTests`, `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and `ClassMngr`. Both focused CTests passed; after the adapter regressions were added, the independent platform CTest recheck passed 1/1. `git diff --check` passed. No full suite ran.
- Gate 1 advances but remains Partial; Gate 2 remains Partial. Workspace criterion and audited `src/next` isolation remain Satisfied. Phase 2 exit remains Open; Sub Prep remains capped at 2026-2027.

F97 is committed; no unrelated source-manifest change was included.

### Phase 2 Teacher Import malformed UTF-8 parity - F98 verified

- Source/test commit `30d545a8eb198042948d233e6e10110bf364ff27` updates only `tests/teacher_import_tests.cpp` and adds `rejectsGeneratedWorkbookWithInvalidUtf8SharedString`. The helper replaces the `M` in `<t>M1</t>` with raw byte `0xFF` before `storedZip` rebuilds member CRCs. The CRC-valid six-entry fixture is 3,422 bytes, SHA-256 `7386d4eae0e7f8d54467b57f05ec909cd0c1e7f392d865f9dc7e52faf3cc8165`.
- A narrow harness compiled revision-specific `calendar_workbook_reader.cpp`, `sectioned_contact_list_template.cpp`, `teacher_import_file_validator.cpp`, and `teacher_import_template_registry.cpp` against the exact same input. Baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current `36ebb09a960fa82f633701ec83fba35bcd7f3599` both returned `UnsupportedTemplate`, empty template ID/source date/discovered sections/preview records, and zero preview counts. Localized names and diagnostics were omitted.
- Executor and independent fresh Ninja builds passed the exact QTest case (3 passed, 0 failed including setup/cleanup) and focused `ClassMngrTeacherImportTests` CTest (1/1). Independent ZIP validation passed all six entries and confirmed the raw `0xFF`; `git diff --check` passed. No full suite ran.
- F98 adds one synthetic malformed-input parity case to Gate 2, which remains Partial; Gate 1 remains Partial. This is not historical production-workbook evidence. Workspace criterion and audited `src/next` isolation remain Satisfied; Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

### Phase 2 Calendar repeated-series creation orchestration - F99 verified

- Source/test commit `794ed7c0a5de8252a1687a1be8e4612c856cf00e` adds `src/next/application/calendar_event_series_create_use_case.h`, registers it in `cmake/next.cmake`, routes the new repeat-create UI branch in `src/features/calendar/ui/calendar_page_events.cpp` through it, and adds fake-port cases in `tests/next_application_calendar_event_tests.cpp`.
- `CalendarEventSeriesCreateUseCase` composes `planCalendarEventRepeatOccurrences` with the injected `CalendarEventSeriesCreatePort`. It returns planning errors before persistence and forwards valid requests once, propagating the port result. Fake-port tests pin the full planned request, one successful call, port failure, and invalid-range rejection with zero calls. The UI keeps dialog handling, frequency/date/UUID conversions, warning display, and post-success cache invalidation.
- Executor and independent fresh Ninja verification built `ClassMngrNextApplicationCalendarEventTests`, `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and `ClassMngr`; all 354 independent Ninja actions passed. Focused CTest passed 2/2; `git diff --check` passed. No full suite ran.
- F99 advances Gate 1, which remains Partial; Gate 2 remains Partial. The workspace create criterion and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

### Phase 2 Schedule Import seeded replacement rollback parity - F100 selected

- Three independent Investigator reviews compared a Gate 1 single-event save use case, Schedule Import parse parity, and rollback candidates. F100 selects Schedule Import rollback restoration for seeded state: it adds state-transition evidence in a second importer after F96 covered a late Teacher Import metadata-write failure. Gate 1 and Gate 2 remain Partial.
- Two independent Explorers confirmed `ScheduleImportRepository::apply()` updates teacher/class-info rows, clears selected regular `class_times`, and inserts replacements inside one transaction. The existing `writeFailureRollsBackEveryChange()` begins from empty state and does not establish restoration of existing rows. A trigger can reject a later replacement row after an earlier one has been inserted.
- Seed stable teacher, class, class-info, and multiple prior regular schedule rows. Apply one deterministic normal-mode plan that updates teacher/class-info, deletes prior schedule rows, inserts an earlier replacement row, and then triggers failure on a later `class_times` insert. Compare the normalized injected failure and ordered before/after snapshots for `teachers`, `classes`, `class_info`, `class_times`, `class_intensive_times`, `intensive_slot_states`, and `app_settings`, and `sqlite_sequence`.
- Use baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and F99 current commit `794ed7c0a5de8252a1687a1be8e4612c856cf00e`, compiling each revision own repository/schema/transaction/source closure against identical plan, seed, and trigger. Add or strengthen the current `ClassMngrScheduleImportTests` regression and run focused CTest plus a narrow differential harness. This remains synthetic repository-plan parity, not historical production-workbook or parse-to-apply evidence. No full suite; Sub Prep remains capped at 2026-2027.

## Latest continuation - F103 verified; F104 selected

F102 commit `ef418996` adds Qt-free request validation and a use case for repeat-series suffix deletion. Only “This and following” routes through it; ordinary single-event deletion and delete-all remain separate. The existing diagnostic, ID bytes, Qt date conversion, service/error mappings, warnings, and success-only cache invalidation are preserved.

An independent short-path archive based on `c08c0f93` plus the seven intended F102 paths built 358/358 actions; repository, Application, and Platform focused CTests passed 3/3. A max-boundary test-only assertion then rebuilt in four Ninja actions and the Application CTest passed 1/1. Protected `cmake/sources.cmake` SHA-256 matched `5E798E0A643F499D8E8F6F3AD3732E1C87BF1301429257E6E431B32B75EBE3EC`. `git diff --check` passed with line-ending warnings. No full suite or parity claim was made.

Two independent exit audits agree Gate 1 and Gate 2 remain Partial and the workspace-create criterion is Satisfied. Direct `src/next` isolation is Satisfied in the audited scope; strict transitive ApplicationServices-to-DataService reads remain unresolved. Historical production-workbook provenance is a tracked risk, not a separate literal exit criterion. Phase 2 remains In Progress with exit Open.

F103 commit 61e3d797 routes ordinary single-event deletion through CalendarEventDeleteUseCase. It forwards the opaque typed ID once; Platform retains legacy integer parsing and service/error/exception mapping. The app-less tests cover exact forwarding and structured failure propagation. The repository fixture asserts seeded sibling and unrelated snapshots exist, then checks full-row preservation, target removal, row count, and sqlite_sequence.

Independent fresh Ninja/MSVC/Qt verification built ClassMngrNextApplicationCalendarEventTests, ClassMngrNextPlatformApplicationServicesCalendarEventPortTests, ClassMngrCalendarEventRepositoryTests, and ClassMngr; the three focused CTests passed 3/3. The same QtTest fixture passed against baseline 48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99 and F102 ef418996, each using its own repository source closure. Both observations matched: target absent, sibling/unrelated rows preserved, count 2, sequence 3 before and after. This is seeded repository parity only; no UI or historical-workbook parity claim. No full suite ran.

Gate 1 and Gate 2 remain Partial. Workspace create and audited direct src/next isolation remain Satisfied; strict transitive ApplicationServices-to-DataService reads remain unresolved. Historical production-workbook provenance remains a tracked risk, not a literal exit criterion. Phase 2 exit remains Open.

F104 is selected: add CalendarEventDeleteAllUseCase around CalendarEventDeleteAllPort, route the pre-confirm availability guard and post-confirm operation through it, and preserve confirmation/cancel, warning, and success notification behavior. Add app-less port tests plus a seeded baseline/current deleteAllCalendarEvents case using each revision's own sources. The comparison checks all rows removed, count zero, and matching sequence behavior; claim repository behavior only. Focused verification covers the Application and Platform Calendar targets, repository target, and ClassMngr, with only the three focused CTests.

The user-committed Sub Prep rule uses the reference year and the following year, ending December 31 of that following year; 2026-2027 is an example only. The Phase 2 plan/log commit 4a65da25 carries this wording forward.

Only one of the two requested Explorer lanes for next-slice mapping could run; the second lane and replacement were unavailable at the agent thread limit. F104 was selected from the written open scope, the available Explorer report, and direct evidence of the Calendar reset path. Revisit if later evidence changes the candidate.

## Current continuation - 2026-09-28 (F119 verified; F120 selected)

The active deployment objective is to complete Phase 2 under the formal plan using bounded implementation slices, an independent verification for every slice, canonical documentation updates, and per-slice commits. The user selected Heavy via `00-Start-Here`; deployment ID `qt-rewrite-phase2-resume-20260928` was emitted at deployment entry and must not be emitted again.

### F117 - app-less Teacher Profile Edit contract

Source/test commit `3fd2b93f0fd87077aa59654265cda8b53b658ba9` adds checked Teacher identity and profile values, a validation-policy boundary returning normalized fields and structured field-addressable issues, and a persistence port with separate update and reload operations. It rejects invalid IDs before policy calls, blocks only error-severity issues while preserving all details, accepts warning-only results, updates with normalized values, differentiates update/reload errors, marks a reload error as following a successful write, and returns the canonical profile after reload.

An independent Tester extracted base `47844dfc087d47da9426e0aa06d948a1ab2264a9` with `git archive`, overlaid exactly the five source/test/CMake paths from F117, and verified their SHA-256 values. A forced clean Ninja/MSVC rebuild passed; `ClassMngrNextApplicationTeacherProfileEditTests` passed 1/1 with `--no-tests=error`. The two new production headers and test target have no Qt linkage or legacy production dependency. `git diff --check` passed. MSVC emitted nonfatal C4530 because the test target lacks `/EHsc`; no full suite ran.

The formal Phase 2 plan/log update is committed as `5263aebf`. Main-owned `project_progress.md`, `project_diary.md`, and `latest_session_work.md` F117 records are committed as `a9b2f2b7`. F117 does not add the production validation adapter or wire `TeacherInfoPage`; the adapter must delegate to existing `TeacherValidator` rules and normalization.

### F118 - Gate 2 Class Transfer common-input comparison

Source/test commit `b68eba6dd93c1eaa6473ec9494a4d9e7da9980ef` changes only `tests/class_transfer_tests.cpp` (SHA-256 `71FA243D9C641EA955A5B33201478C76BCEDFAE7A333D0AF7727AE8E14FFB1E8`). The case pins `tests/fixtures/transfers/conflict_source.json` (SHA-256 `BED9CBEE84A7946F51029EFC4AA2B2850BDA9784757250FBF6CE90CAB7FB173`), seeded preview matches, normalized review choices and plan, exact combined regular/intensive conflict diagnostic, unchanged snapshots of all application tables and `sqlite_sequence`, and zero `total_changes` while SQLite query-only is enabled.

Independent verification used a fresh current archive at `5263aebf8222e16e3085498af851a7f0d3041818` with only the test file overlay and a fresh baseline archive at `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` with the unchanged fixture and only the F118 helpers/includes/slot/case transplanted into its older test file. Both focused `ClassMngrClassTransferTests` CTests passed 1/1. Baseline CMake rejected installed Qt 6.12 against its 6.11.1 minimum; the Tester changed three minimum references in the temporary tree only. CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257.0, and Qt 6.12.0 were used. Diff hygiene passed. This is common-input comparison on a checked-in post-baseline fixture, not historical production-workbook parity. No full suite or separate serialized cross-version bundle was produced.

### Exit-gate status and next work

Gate 1 and Gate 2 remain Partial. Workspace-create acceptance and the audited direct `src/next` source scan remain Satisfied. F119 removes the dual-bound factory edge; strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved because Workspace still uses DataService-backed operations through ApplicationServices. Historical production-workbook provenance is a tracked risk, not a literal exit criterion. The Phase 2 exit gate remains Open.

F119 source/test commit `80fbf034` changes exactly `src/data/data_service.h`, `src/data/data_service.cpp`, `src/core/application_services.h`, `src/core/application_services.cpp`, and `tests/data_service_lifecycle_tests.cpp`. The prior handoff's five SHA-256 values were incorrect. Independent verification archived exact commit `80fbf034d96b7d04b9be19c61de20de2c44a2f9d`; each archive file matched its Git blob and the corrected exact-commit SHA-256 values are:

| File | SHA-256 |
| --- | --- |
| `src/data/data_service.h` | `E6EAF02E21693E9B5B687F8E657FD6687D3DEBC85B81AB05693C6C4179BCCFA7` |
| `src/data/data_service.cpp` | `9EA5BB3BCD4034D07F8A21A87747E81141D5D2A98A9875A1AA99FC4F4F80F8C3` |
| `src/core/application_services.h` | `B085423CEE89A53D262DF0A7E595BF5C2357E29043F945F07C87DAE99B1883F3` |
| `src/core/application_services.cpp` | `33CAFC7EC764E0BB9E97C223157AFA0BCA7E316B8DCE245C93EEA9AF647AAC8B` |
| `tests/data_service_lifecycle_tests.cpp` | `E3DAA625158F103CE4E95D9215397C09F7F66483C4B38353D3AF3BF38B180137` |

Two independent Explorers found that DataService owned the canonical session, ApplicationServices created seven feature services with both session and DataService pointers, and the Workspace port still delegates lifecycle/save/save-as/export to DataService. F119 moves canonical session ownership to ApplicationServices, keeps DataService as a borrowing compatibility facade while preserving its standalone owner constructor, and changes all seven ApplicationServices factories to session-only construction.

The independent Tester built `ClassMngr` and six focused targets from the fresh archive in 370 Ninja steps using Windows x64 Debug, CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257.0, and Qt 6.12.0. `ClassMngrDataServiceLifecycleTests`, `ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, `ClassMngrDocumentCatalogTests`, `ClassMngrSubPrepPrintPdfTests`, `ClassMngrNextPlatformApplicationServicesClassNotesSavePortTests`, and `ClassMngrNextFeatureClassNotesPageTests` passed 6/6; `git diff --check` passed. CMake reported optional missing `WrapVulkanHeaders` notices and long-path warnings for 19 unselected targets; the selected targets built without those warnings. No full suite ran. A focused follow-up established that the old handoff hashes were incorrect and not explained by CRLF conversion; the corrected fresh-archive hashes are listed above. Workspace remains open, so strict transitive isolation and the Phase 2 exit gate remain open.

The user resumed this deployment on 2026-09-28. The branch includes the user-authored `a606b592` commit linking phase plans to `00-Start-Here`, followed by the F119 verification handoff documentation commit. No push was requested; commits after the prior pushed F118 source commit remain local.

Three independent Investigator reviews agree F120 should be one gate-closing slice: a partial reroute would leave the transitive edge open or the compatibility facade unsafe after session swaps. They compared a dedicated Workspace service/port rewire with keeping the existing `ApplicationServices` boundary. The selected boundary keeps the current `ApplicationServices` API and makes its seven workspace operation implementations use `DatabaseSession` and a small DataService-independent file-copy helper; `DataService` will resolve repositories live from its owned or borrowed session instead of caching raw pointers. Do not use a refresh callback from the Workspace path. This removes the recorded operation edge while preserving FileController and port call sites that already use ApplicationServices for open-state and path.

F120 acceptance: audit the seven Workspace operations to confirm they call only `DatabaseSession` and the shared file helper, with no `m_dataService` operation call; confirm `src/next` still has no direct DataService dependency. Preserve path/error normalization, open/close postconditions, save commit behavior, save-as copy then port reopen with Workspace identity retained, and export's unchanged active path. Add lifecycle coverage for a borrowed facade after opening from closed, successful A-to-B replacement, failed replacement preserving A, close, and reopening B; keep standalone/sessionless legacy coverage. Focused verification covers `ClassMngrDataServiceLifecycleTests`, `ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and `ClassMngrFileControllerWorkspaceLifecycleTests` as needed, plus a `ClassMngr` build and diff hygiene. F120 implementation has not started. Phase 2 remains In Progress with Gate 1 and Gate 2 Partial, Workspace-create acceptance Satisfied, audited direct `src/next` isolation Satisfied, strict transitive isolation unresolved, and the exit gate Open. Continue with implementation, independent verification, documentation, a per-slice commit, and the next slice. At Phase 2 deployment closure, verify the formal exit checklist and provide the required deployment-token report through the Archivist workflow.

## Phase 2 continuation handoff - 2026-09-28 (F122 verified; stop point)

The user requested a handoff after the current task and asked work to stop. The
active deployment ID is `qt-rewrite-phase2-resume-20260928`; the branch is
`Qt-Rewrite`; F122 source commit is
`e3411e733d2ce402a03096e715150e45b4244dff` (`Phase2 - pin Schedule Import Skip
parity`). It changes only `tests/schedule_import_tests.cpp` (Git blob
`eb422208842551eb37a4783f31982b82cab674b3`, fresh-archive SHA-256
`9697009E56FE575EE0303F54413CCBA205B17EF716278E9E5C1158E7343BB066`). The
F122 archive tree ID is `ca903158a6e8ff2866987b4873e5be3f5e92f845`; TAR SHA-256
is `402DB5014595E72B84836AA131B08C482FC23551B6D3FC192C6AB6ED52895440`.

Independent verification built and ran the exact focused
`skippedExactMatchPreservesItsSchedule` case in current and pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`; each reported 3 passed, 0 failed.
Both final snapshots hash to
`08ad64ed3d853e52a1a686c1683d0d1fe8a289026e21d21081e09ee4840c5ebe`, including
row IDs and `sqlite_sequence`. The test proves Tuesday-then-Monday insertion
order survives Skip, `classesSkipped == 1`, `schedulesCleared == 0`, and the
profile behavior. Baseline used the exact F122 test/helper overlay plus only
the missing `QCryptographicHash` and `QSqlRecord` includes; the shared
checkout was untouched. Windows 11 x64, MSVC 19.51.36257.0, Ninja 1.13.2;
current Qt 6.12.0, baseline Qt 6.11.1. No full suite ran. The baseline build
had an optional Vulkan-header notice and an unrelated object-path warning.

The Phase 2 plan and Start Here handoff record F122 as verified and retain the
gates as open. `00-Start-Here.md` previously pointed to Work Package D, but the
Phase 2 progress log records D completed on 2026-09-23. Two independent
Explorers recommend F123 as a candidate to integrate F117's existing-teacher
edit use case with `TeacherInfoPage`, preserving `TeacherValidator` behavior,
session-backed update/reload, validation feedback, autosave, canonical reload,
header updates, and `teacherSaved`. The bounded Investigator solution review
was interrupted at the user's stop request; F123 is a candidate, not an
accepted implementation scope. On resume, perform the required three-lane
solution review, compare the reports, update acceptance, then implement and
independently verify F123. Phase 2 remains In Progress/Open; Gate 1 and Gate 2
are Partial, while workspace-create and active-v2 DataService isolation are
Satisfied. The deployment-token report remains for Phase 2 deployment
closure; this task handoff does not close the phase. No push was requested.

## Phase 2 continuation - 2026-09-29 (F124 verified; F125 review next)

The user requested a commit after every completed slice, then immediate work on
the next slice. F124 source/test commit
`a159591e48e312f96378302b105a3ad1276382b3` (`Phase2 - integrate co-teacher
assignment use case`) connects `ClassCoTeacherPage` to the app-less
`ClassCoTeacherAssignmentUseCase`. The use case rejects invalid IDs and maps
the legacy `-1` unassigned sentinel to a missing teacher ID. The session-bound
platform adapter reads the existing `ClassInfo`, changes only `teacherId`, and
saves through `ClassService::saveClassInfo`; all class-owned details, notes,
and schedule rows remain intact. The page preserves its manual warning,
dirty-state handling, title refresh, and `classInfoSaved` signal.

The configured `windows-x64-debug` preset built `ClassMngr`, the app-less
use-case test, the platform adapter test, and the page integration test. Their
focused CTest selection passed 3/3. Coverage checks invalid IDs, assigned and
unassigned teacher values, persisted fields and schedule preservation,
unavailable service handling, success title/signal refresh, and manual failure
warning/dirty behavior. `git diff --check` passed. Visual Studio FileTracker
first denied access under the default sandbox; the build passed after the
required access was granted. The existing `ClassMngrClassesPageTests` target
also built but failed 22 checks, including `page.openClass(42, ...)` and
widget validation assertions; its initial Details open failure occurs before
the co-teacher editor opens. That target emitted duplicate-stub `/FORCE` linker
warnings and a stale dependency warning for
`class_navigation_preferences.h`. No baseline comparison, full suite, or
independent archive verification ran.

F125's candidate is routing the existing `ClassNotesSaveRequest` through an
app-less `ClassNotesSaveUseCase` in `ClassNotesPage`. Preserve its 10,000
UTF-16 code-unit text bound, trimmed fields, session-backed platform save,
manual warning, and dirty-state behavior. The page currently calls the port
directly while the request already owns limit validation. Candidate scope is
pending review; implementation has not begun. Gate 1 and Gate 2 remain
Partial; workspace-create acceptance and active-v2 DataService isolation
remain Satisfied. Phase 2 is In Progress/Open. No push was requested.

## Phase 2 continuation - 2026-09-29 (F128 verified; F129 implementation in progress)

The user requested a commit after every completed slice, then immediate work on
the next slice. F125 source/test commit
`42bdbbea7e1d2cbc2c9eeb8c0631fd22b335a17d` (`Phase2 - integrate class notes
save use case`) adds `ClassNotesSaveUseCase` and routes `ClassNotesPage` through
it. The app-less use case enforces `ClassNotesSaveRequest`'s 10,000 UTF-16
code-unit bound before port invocation and preserves port failures. The
platform adapter retains its defensive check and session-backed
`ClassService::saveClassNotes` call. The page keeps trimming, manual warning,
and dirty-state behavior; oversized manual text is rejected before calling
the port and remains dirty.

The `windows-x64-debug` preset built `ClassMngr`,
`ClassMngrNextApplicationClassNotesSavePortTests`,
`ClassMngrNextPlatformApplicationServicesClassNotesSavePortTests`, and
`ClassMngrNextFeatureClassNotesPageTests`. The F125 focused CTest selection
passed 3/3. The combined F124/F125 selection passed 6/6. `git diff --check`
passed. No full suite or baseline comparison ran.

F126 source/test commit `ca4c1a97` (`Phase2 - integrate class details save
use case`) adds the app-less `ClassDetailsSaveUseCase`, the session-backed
platform adapter, and page integration. The Qt-free request carries a typed
class ID, editable text, and typed regular/intensive schedule values. The page
retains normalized `ClassInfoValidator` feedback and separate regular and
intensive conflict checks. The adapter reloads the existing session-backed
record, updates only requested fields, and preserves teacher assignment,
notes, and time-filler activities. Page success still clears dirty state,
updates the title, and emits `classInfoSaved`; port failure preserves warning
and dirty-state behavior.

The focused F126 CTest selection passed 3/3. The implementation build included
the production `ClassMngr` target and all focused targets. Independent
verification reran the three focused tests successfully; its build command
was up to date. A clean independent rebuild was unavailable because Ninja
could not find `rc` and the Visual Studio generator could not find a C++
compiler. `git diff --check` passed. No full suite or baseline comparison ran.

F127 source/test commit `988b7de7` (`Phase2 - integrate class details page
read query`) adds a Qt-free typed-ID query and owning snapshot for the class
details display path. The snapshot carries class fields, ordered raw schedule
rows, teacher display name, and student count with independent source outcomes.
The platform port reads through the active `ApplicationServices` session. The
page uses the query for initial/reload display and title retranslation; after
a successful save it refreshes title data while retaining the just-saved
fields and current form state. The existing fresh read for save validation
remains separate.

The app query, platform port, page display, and F126 save-page tests passed
4/4. The page title-refresh regression verifies that a changed teacher name is
used after save while the form, schedules, and student count remain loaded.
`ClassMngr` and the focused targets rebuilt successfully through the Visual
Studio developer environment; `git diff --check` passed. No full suite or
baseline comparison ran. Malformed schedule values are covered at the
app/platform boundary; malformed-value widget rendering was not changed.

F128 source/test commit `93ad5afb` (`Phase2 - integrate schedule slot state
save use case`) adds a Qt-free command with typed weekday, start minute,
selected state, and default state. The UI/view model retains the transition
rule; the session-backed adapter maps the command to the existing global
day/start-time slot-state key, preserving delete-on-default behavior. The
shared ordinary handler covers regular and intensive views. The separate
testing-assignment path remains unchanged.

The app command, platform adapter, schedule widget, and testing-classes CTest
targets passed 4/4. The regular and intensive toggle tests assert exact
weekday/time/state/default mapping. Coverage includes invalid requests,
session persistence and deletion, failed-write warning/no-reload, unavailable
service skip/reload, read-only/time-column no-op, and testing-assignment
behavior. `ClassMngr` built and `git diff --check` passed. No full suite or
baseline comparison ran.

## Phase 2 continuation - 2026-09-29 (F136 verified; F137 candidate review interrupted)

The user requested a commit after every completed slice, then immediate work
on the next slice. The active deployment ID is
`f126-class-details-20260929`; branch `Qt-Rewrite`. F129 source/test commit
`a0e3c98a` (`Phase2 - integrate roster save use case`) adds a Qt-free save
contract for one selected class's complete roster snapshot and a
session-backed platform adapter. It preserves ordered columns, widths, all 25
row positions, UTF-16 values, and the existing questionable Korean-name
confirmation flag. The widget retains validation/focus, interactive
confirmation, autosave/manual timing, warning and dirty-state behavior, and
class-selection save gating.

Focused app/platform/page CTests passed 3/3; the existing roster-import widget
regression passed, and `ClassMngr` built. The independent follow-up rebuilt
`ClassMngrRosterEditorWidgetSaveTests` and passed 1/1. It directly verifies
invalid-cell focus, silent autosave failure with dirty state retained, and
rollback of the selected testing class after a roster-save failure. Source and
test review found no remaining acceptance gap; `git diff --check` passed. No
full suite or baseline comparison ran.

F129 verification handoff is recorded in `project_progress.md`,
`project_diary.md`, and this file. The handoff documentation commit is
separate from the source commit. No push was requested.

F130 source/test commit `70f3ddc9` (`Phase2 - integrate speaking evaluation
save use case`) adds a Qt-free typed save request and a session-backed adapter
for one selected speaking evaluation. The request preserves the ordered
25-by-11 UTF-16 matrix, evaluation name, changed-cell coordinates, and
questionable Korean-name flag. Empty changed-cell lists retain the legacy
write-all behavior. The page keeps validation/focus, confirmation, save
notices, dirty-baseline updates, autosave/manual timing, and failed
class/evaluation selection rollback.

The app, platform, and page focused CTests passed 3/3; `ClassMngr` built.
Independent verification reran the platform target after adding a
non-empty-delta case and passed 1/1. The added case proves that a changed
in-memory cell outside the delta does not overwrite stored data while the
listed cell persists. `git diff --check` passed. No full suite or baseline
comparison ran.

F130's source/test commit is `70f3ddc9`; its verification handoff is recorded
in `project_progress.md`, `project_diary.md`, and this file. The handoff
documentation commit is separate from the source commit.

F131 source/test commit `d0828493` (`Phase2 - integrate speaking evaluation
read query`) adds a Qt-free typed-ID query and a session-backed adapter for one
selected evaluation. It preserves the exact evaluation-name key and ordered
UTF-16 rows. The page reads through the query, then keeps its existing model
normalization and blank 25-by-11 clean fallback on empty or failed reads.

The app, platform, and page focused CTests passed 3/3; `ClassMngr` built.
Independent verification confirmed exact-name behavior (`" Winter "` does
not match `"Winter"`), ordered Unicode rows, structured read failures, and
empty/failed page fallbacks. `git diff --check` passed. There is no direct
page-load test for partial/malformed-row normalization; the existing model
path remains in place. No full suite or baseline comparison ran.

The F131 verification handoff is recorded in `project_progress.md`,
`project_diary.md`, and this file. Its documentation commit is separate from
the source commit.

F132 source/test commit `e878906c` (`Phase2 - integrate roster read query`)
adds a shared Qt-free `RosterSnapshot`, a typed class-ID read query, and a
session-backed platform adapter. `RosterEditorWidget::loadClass` reads through
the query. Ordered columns, widths, all raw rows, and UTF-16 values pass through
the app/platform boundary without a 25-row cap; the existing model applies
column/name/width normalization and the UI row limit. Failed and empty reads
retain the blank-roster behavior, while loading keeps autosave clean and
refreshes output capabilities.

The app, platform, and page focused CTests passed 3/3; `ClassMngr` built.
Independent verification confirmed 38 raw rows at the adapter, a missing-roster
success, structured repository failure, closed-session failure, page custom
columns/widths and normalized values, 25-row presentation, blank/failed
fallback, clean state, and capability signals. `git diff --check` passed. No
single page test loads 38 rows end-to-end; the raw and displayed limits are
covered separately. No full suite or baseline comparison ran.

F132's source/test commit is `e878906c`; its verification handoff is recorded
in `project_progress.md`, `project_diary.md`, and this file. The handoff
documentation commit is separate from the source commit.

F133 source/test commit `b111b799` (`Phase2 - integrate classes navigation
snapshot`) adds a Qt-free typed-ID navigation snapshot and active-session
platform adapter. The repository loads compact class metadata and teacher
display names with one join, then reads regular and intensive schedules with
two ordered batch queries. The application snapshot preserves exact UTF-16
names and raw schedule rows. `ClassesPage` retains its existing filtering,
selection, and refresh behavior, and keeps class names with blank metadata
when the read fails.

`ClassMngr` and four focused test targets built; the app, platform, ClassesPage,
and navigation-model CTests passed 4/4. Independent verification confirmed
three-query batching across multiple classes, missing class-info and teacher
rows, exact schedule strings/order, filtering and selection, read-failure
fallback, and refresh after class-info save. The independent build used
Ninja/MSVC after the Visual Studio FileTracker reported access denied.
`git diff --check` passed. No full suite or baseline comparison ran.
F133's verification handoff is recorded in the canonical deployment documents;
their commit is separate from the source commit.

F134 source/test commit `cbb15e32` (`Phase2 - integrate schedule builder
source query`) adds a Qt-free source snapshot and active-session Platform
adapter, then keeps schedule parsing and row construction in the service-free
builder. It reuses the repository's existing ordered batch read; this is an
application-boundary improvement, not a database query-count optimization.
The snapshot preserves raw schedule strings and current parser behavior,
including blank-day defaulting, invalid-start skipping, invalid-end inclusion,
offsets, and source order. The repaired Platform test distinguishes creation
order from repository order and checks the exact intensive end time.

The independent fresh Ninja/MSVC build passed for `ClassMngr` and the four
focused targets. The app snapshot, Platform read port, builder, and ScheduleWidget
CTest targets passed 4/4; `git diff --check` passed. No full suite or baseline
comparison ran. Once `setPreviewModel()` installs a ScheduleWidget preview,
subsequent renders and refreshes bypass source, slot-state, and
testing-assignment reads. Before that, `ScheduleImportReviewDialog::buildUi()`
calls two setters that render the live schedule, and `prepare()` performs its
normal refresh to load display preferences. With services available, setup can
perform three pre-preview reads. This call order is present at the F134 parent.

F135 source/test commit `91806e5d` (`Phase2 - integrate schedule slot-state
read query`) adds a Qt-free query for intensive slot-state overrides and a
Platform adapter that reads directly from the active `DatabaseSession`
repository. The ordered raw UTF-16 day/start/state values preserve unknown
stored text. `ScheduleWidget::reloadSlotStates()` uses the query; unavailable
reads remain silent and retain state, read failures warn and retain state, and
successful reads replace all overrides, including clearing them on an empty
result. The adapter has no `ScheduleService` or `DataService` path.

Independent fresh Windows x64 Debug Ninja/MSVC verification built `ClassMngr`
and the Application, Platform, and ScheduleWidget targets. These CTest suites
passed 3/3: `ClassMngrNextApplicationScheduleSlotStateReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesScheduleSlotStateReadPortTests`, and
`ClassMngrScheduleWidgetTests`. The widget warning assertion pins the existing
`Failed to load intensive slot states:` prefix and injected error. The tester
confirmed the root production target already links `ClassMngrNext::Platform`;
the F135 widget test and Application/Platform test registrations are explicit.
`git diff --check` passed. No full suite or baseline comparison ran.

The Heavy-route candidate review compared the testing-assignment read,
`ScheduleViewProjection` production integration, and Schedule Import review
ownership. Two independent Explorers and all three independent Investigators
ranked the testing-assignment read as the best bounded next slice.

F136 source/test commit `0295543a` adds a Qt-free ordered UTF-16 assignment
snapshot, active-session Platform adapter, and one joined repository read for
assignment and optional special-class display fields. The widget now consumes
that boundary without `ScheduleService`/`DataService` fallback or per-
assignment detail reads. It preserves plain and special assignments,
unavailable clearing, warning and previous-state retention on assignment-read
failure, the missing-special-class warning and card skip, blank/default class
metadata, and preview bypass. Assignment writes, full `ScheduleViewProjection`
integration, and Schedule Import review ownership remain separate.

Independent fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 verification in
`build/f136-independent-verification` built `ClassMngr` and the Application,
Platform, and ScheduleWidget targets. The matching focused CTests passed 3/3:
`ClassMngrNextApplicationScheduleTestingAssignmentReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesScheduleTestingAssignmentReadPortTests`,
and `ClassMngrScheduleWidgetTests`. The query-count check verifies one
statement at one and 41 assignments. Independent review initially found that
the missing-special-class warning had been dropped; the snapshot/UI boundary
was corrected, and the independent rebuild and 3/3 rerun passed. `git diff
--check` passed. No full suite or baseline comparison ran.

F137 candidate review then started at `0295543a` with two independent Explorer
lanes. The user requested that work stop before either report arrived; both
lanes were interrupted. No F137 candidate was selected, and no F137 files were
changed. On continuation, resume Heavy-route Phase 2 review from commit
`0295543a`, rerun the bounded candidate discovery, then select and implement
the next slice.

Gate 1 and Gate 2 remain Partial. Formal workspace-create acceptance and
active-v2 DataService isolation remain Satisfied. Phase 2 remains In Progress
with the exit gate Open. The deployment-token report remains required at
Phase 2 closure. No push was requested.

## 2026-09-29 — Phase 2 continuation: F144 accepted

The active deployment is phase2_resume_20260929 on the Heavy route. The user
requires a commit after every accepted slice, followed by work on the next
slice; no push was requested.

F144 moves the TestingClassesPage teacher-choice read through a Qt-free typed
Application query and an active-session Platform adapter backed by
teacherRepository()->getAllTeachers(). It preserves repository order and typed
IDs; the page trims labels and rooms, filters blank Korean labels, restores
selection by ID, retains None, stays silent when the session is unavailable,
and keeps the existing load warning on other failures. The original partial
implementation is checkpointed at 56c76f412b246230fcfe00c249b195dcc6ccd95f.
The acceptance test update is committed as
89fbbaa250ddf98fae2ab1d80385fb99164ac055 (Phase2 - accept F144 teacher
choices).

Fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 verification passed:

- build/phase2-f144-independent-ninja-x64-20260929 built and passed the F144
  Application query, Platform adapter, and TestingClassesPage CTests (3/3);
  source ownership validated 1,025 handwritten files.
- build/p2-f142f143 built and passed the F142/F143 Application and Platform
  regression CTests (4/4) in a fresh short-path tree.
- All seven distinct required CTests passed. git diff --check passed. No full
  suite or full ClassMngr application build ran.

An earlier F142/F143 Platform build attempt reported MSVC C1083 before tests
ran. The fresh short-path build then passed; the cause of the first failure is
unknown. The adapter's defensive null-teacherRepository branch is not
independently injectable while an open DatabaseSession exists; null services,
unopened sessions, and closed sessions cover the observable unavailable path.
No production defect was observed.

The earlier note that F145 was unselected is superseded by the F145 acceptance
record below. F144 owns the teacher-choice read; the detailed Phase 2 plan and
handoff resolve the earlier Start Here ambiguity.

## 2026-09-30 — macOS Qt checksum retrieval

Deployment ID: `macos-aqt-checksum-20260930`.

The user reported the macOS Qt 6.12.0 install failing in aqt 3.3.0 while retrieving the qtbase archive checksum from `download.qt.io`. Independent upstream source review established that this is a checksum-sidecar retrieval failure, not a checksum mismatch; the exact response status and URL were not available, and the `MacOS_26-X86_64-ARM64` archive name alone does not prove aqt parser incompatibility.

Updated `.github/workflows/macos-release.yml` and `.github/workflows/refactoring-baseline.yml`: both macOS Qt install commands now use a 30-second timeout, retry the complete install up to three times, and wait 10 seconds after each of the first two failures. They preserve Qt 6.12.0, `clang_64`, qtpdf, output paths, and aqt 3.3.0. Final failure remains nonzero and checksum verification remains enabled.

`git diff --check`, Ruby YAML parsing on both full workflows, and `bash -n` on both changed run blocks passed. The independent Tester reviewed the retry paths and retained settings with no findings. `actionlint` was unavailable. No tests, live Qt installation, or GitHub Actions run were performed. If the error recurs after retries, capture the exact sidecar URL and HTTP response to distinguish transient availability from a persistent Qt repository publication issue. The user subsequently requested a commit; no push was requested.

## 2026-09-30 — Phase 2 continuation: F145 accepted

Deployment: `phase2_resume_20260929`, Heavy route. The user requires each
accepted slice to be committed before work starts on the next one; no push was
requested.

F145 routes existing Testing Class details updates through the Qt-free
`TestingClassDetailsUpdateUseCase` and an active-session Platform adapter to
`TestingClassRepository::updateTestingClass()`. It validates canonical
positive class and optional teacher IDs and has no legacy fallback. The page
preserves the create and delete service paths and the roster-first ordering.
If a later detail update fails, a previously successful roster save remains
persisted and clean while the details draft stays dirty.

Production commit `26a916df9994217ffd3f12f45148207e9cf5e0c2` and test commit
`ba1b7cdec15f6f163bb1620897fb4c2e2b3baccb` are accepted. Fresh Windows x64
Debug Ninja/MSVC verification passed all ten requested CTests. The build also
validated source ownership and built the new standalone targets plus the page
target. Passed CTests:

- `ClassMngrNextApplicationTestingClassDetailsUpdateUseCaseTests`
- `ClassMngrNextPlatformApplicationServicesTestingClassDetailsUpdatePortTests`
- `ClassMngrTestingClassesPageF145UpdateTests`
- `ClassMngrTestingClassesPageTests`
- `ClassMngrNextApplicationTestingTeacherChoicesReadQueryTests`
- `ClassMngrNextPlatformApplicationServicesTestingTeacherChoicesReadPortTests`
- `ClassMngrNextApplicationScheduleTestingClassChoicesReadQueryTests`
- `ClassMngrNextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests`
- `ClassMngrNextApplicationTestingClassDetailsReadQueryTests`
- `ClassMngrNextPlatformApplicationServicesTestingClassDetailsReadPortTests`

The run log is `build/f145-verification-ninja-x64-debug/Testing/Temporary/LastTest.log`.
`git diff --check` passed. No
full suite or full `ClassMngr` application build ran. Configure emitted
existing object-path length warnings for long test targets; all requested
targets built. No product defect remains open.

F146 was selected and accepted as a new-class creation slice, including the
existing optional pending weekday/start-time assignment. Its acceptance and
the next F147 handoff are recorded below.

## 2026-09-30 — Phase 2 continuation: F146 accepted

Deployment: `phase2_resume_20260929`, Heavy route. Each accepted slice is
committed before work starts on the next one; no push was requested.

F146 adds `TestingClassCreateUseCase` with a typed `ClassId` result and an
active-session Platform adapter to
`TestingClassRepository::createTestingClass()`. The page passes an optional
pending weekday and start time into that same repository operation, so the
repository transaction still covers class, details, room, and assignment.
The existing-class F145 update and delete paths remain unchanged. No new UI
flow was added.

Production commit `d7acb516cd9261e2199f742b46f90de48aa907d1` and acceptance
test commit `14d2d124a3d8b0548d241f1d2dcea136dbee55d9` are accepted. Fresh x64
Debug Ninja verification with Visual Studio 2026 tools and Qt 6.12.0 passed
14/14 focused CTests. Coverage includes Application validation/result
propagation, active-session persistence with and without a slot, slot
normalization, unavailable-session behavior, rollback across `classes`,
`class_info`, `testing_classes`, and `schedule_testing_blocks`, page pending
slot and draft behavior, and the F145/F142–F144 regressions. `git diff
--check` passed. No full suite or full `ClassMngr` application build ran.

Passed CTests:

- `ClassMngrTestingClassRepositoryTests`
- `ClassMngrTestingClassesPageTests`
- `ClassMngrTestingClassesPageF145UpdateTests`
- `ClassMngrTestingClassesPageF146CreateTests`
- `ClassMngrNextApplicationScheduleTestingClassChoicesReadQueryTests`
- `ClassMngrNextApplicationTestingClassDetailsReadQueryTests`
- `ClassMngrNextApplicationTestingClassDetailsUpdateUseCaseTests`
- `ClassMngrNextApplicationTestingClassCreateUseCaseTests`
- `ClassMngrNextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests`
- `ClassMngrNextApplicationTestingTeacherChoicesReadQueryTests`
- `ClassMngrNextPlatformApplicationServicesTestingTeacherChoicesReadPortTests`
- `ClassMngrNextPlatformApplicationServicesTestingClassDetailsReadPortTests`
- `ClassMngrNextPlatformApplicationServicesTestingClassDetailsUpdatePortTests`
- `ClassMngrNextPlatformApplicationServicesTestingClassCreatePortTests`

The run log is `build/f146-verification-ninja-x64-debug/Testing/Temporary/LastTest.log`.
Initial test-fixture issues were corrected in test assets; all 14 final tests
passed. The next step is a Heavy-route candidate review for F147. Gate 1 and
Gate 2 remain Partial, Phase 2 is In Progress/Open, and the worktree was clean
after the test commit.

F147 was selected and accepted as the Testing Class delete boundary; its
acceptance evidence and F148 handoff follow.

## 2026-09-30 — Phase 2 continuation: F147 accepted

Deployment: `phase2_resume_20260929`, Heavy route. Each accepted slice is
committed before work starts on the next one; no push was requested.

F147 routes `TestingClassesPage::deleteCurrentClass()` through the Qt-free
`TestingClassDeleteUseCase` and an active-session Platform adapter to the
existing repository cascade. The adapter has no legacy fallback. The prompt
now names the class, roster, notes, speaking evaluations, regular and
intensive class times, and every schedule assignment. Repository transaction
semantics were not changed.

The first independent page run exposed a crash after successful deletion when
the deleted class had dirty editor/roster state and a sibling was selected: the
selection callback tried to save the stale draft through the new-class path.
Commit `315b3ff33b7e2ab42b43d52cd168ce21a92158c9` resets the deleted class
state before list rebuild/reselection. The repaired regression confirms the
sibling loads cleanly, with zero accidental create/update calls and exactly
one `testingDataChanged` signal. Delete failure still warns and retains the
current draft.

Production commit `b037b4216b71c55c7793df5f7bbbfc4a00690065`, page transition
fix commit above, and acceptance test commit
`397376e439f4b5955c82948ab0c225aaf776d679` are accepted. Fresh Windows x64
Debug Ninja/MSVC verification with Qt 6.12.0 passed 17/17 focused CTests.
Repository tests verify all cascade categories, sibling preservation, and
transaction rollback after a trigger fails the final class deletion. Page
tests cover no selection, cancel, prompt contents/controls, failure, and
success with and without siblings. `git diff --check` passed. No full 220-test
suite or full `ClassMngr` application build ran. The configure emitted
object-path length warnings for unrelated targets; `VsDevCmd` reported
missing `vswhere.exe`, but configure and requested builds succeeded. One
duplicate intensive-time fixture key was corrected before the final run.

Passed CTests:

- `ClassMngrTestingClassRepositoryTests`
- `ClassMngrTestingClassesPageTests`
- `ClassMngrTestingClassesPageF145UpdateTests`
- `ClassMngrTestingClassesPageF146CreateTests`
- `ClassMngrTestingClassesPageF147DeleteTests`
- `ClassMngrNextApplicationScheduleTestingClassChoicesReadQueryTests`
- `ClassMngrNextApplicationTestingClassDetailsReadQueryTests`
- `ClassMngrNextApplicationTestingClassDetailsUpdateUseCaseTests`
- `ClassMngrNextApplicationTestingClassCreateUseCaseTests`
- `ClassMngrNextApplicationTestingClassDeleteUseCaseTests`
- `ClassMngrNextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests`
- `ClassMngrNextApplicationTestingTeacherChoicesReadQueryTests`
- `ClassMngrNextPlatformApplicationServicesTestingTeacherChoicesReadPortTests`
- `ClassMngrNextPlatformApplicationServicesTestingClassDetailsReadPortTests`
- `ClassMngrNextPlatformApplicationServicesTestingClassDetailsUpdatePortTests`
- `ClassMngrNextPlatformApplicationServicesTestingClassCreatePortTests`
- `ClassMngrNextPlatformApplicationServicesTestingClassDeletePortTests`

The run log is `build/f147-verification-ninja-x64-debug/Testing/Temporary/LastTest.log`.
The next step is a Heavy-route candidate review for F148. Gate 1 and Gate 2
remain Partial, Phase 2 is In Progress/Open, and the worktree was clean after
the test commit.

## 2026-09-30 — Phase 2 continuation: F148 and F149 accepted; F150 selected

Deployment: `phase2_resume_20260929`, Heavy route. F148's test/CMake commit is
`6c7211d6427b6dbcddd4d109d9d09f9eeff14f28` (`Phase2 - add class details save
parity test`). The test target is
`ClassMngrClassDetailsPageSaveParityTests`; its sole case,
`successfulUiSaveMatchesSeededCommonInputState`, creates the same teacher and
class seed, edits fields through a live `ClassDetailsPage` with real
`ApplicationServices`, and saves without a fake port. It asserts persisted
teacher/details/notes/activities, exact ordered regular and intensive times,
one `classInfoSaved` signal with the class ID, and a clean dirty state.

Independent fresh Windows x64 builds used CMake 4.4.2, Ninja 1.13.2, MSVC
19.51.36257.0, and Qt 6.12.0. The case passed CTest 1/1 on both current code
(archive source commit `2956df18f3f0c0c53623ad0945b2d75d90d725f4`, 310 Ninja
actions) and pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` (313 actions). Both used test
source overlay SHA-256
`7D73E3F8425022BFA4006B9667D9D7E7CDA0B8BC0AA4CB0DD2ECCAFD15EF8F26` and
the same target registration. The temporary baseline overlay changed only
the three Qt minimum versions from 6.11.1 to the installed 6.12.0 and added
the test registration; production sources were not overlaid. Exact command:
`ctest --test-dir "<build>" -R "^ClassMngrClassDetailsPageSaveParityTests$" --output-on-failure --no-tests=error`.
The F148 case adds one common-input successful-save comparison to Gate 2;
validation and conflict parity remain open. No full suite ran. The CMake
warnings were nonfatal and affected missing `vswhere`, optional Vulkan headers,
or object-path lengths for unrelated targets.

Review of latest commits `2956df18` and `397376e4` found no blocker. The
independent review noted two low-priority test gaps: closed/unavailable-session
cases do not themselves distinguish a direct active-session adapter from a
same-session fallback, and the delete cancel/failure cases keep the roster
clean (the success regression covers dirty roster state). Production code uses
the active repository directly; the focused F147 17/17 result remains the
recorded run, not a rerun during review.

Three independent F149 Investigators agreed on a narrow typed regular/intensive
schedule-conflict query behind the Class Details page and an active-session
Platform adapter to `ClassInfoRepository::getClassTimeConflicts()`. The page
will keep validation-first ordering, Regular-before-Intensive short-circuiting,
warning/silent-autosave behavior, and the existing ClassService save-time
validation/conflict recheck. The adapter will not call `ClassService` or
`DataService`. F149 does not move the repository overlap algorithm into
Application, and will preserve the current same-display-name warning behavior.
The selected acceptance includes app-less contract tests, active-session
Platform tests, page behavior tests, and common-input regular/intensive
conflict parity against baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`.

F149 production/test commit `2e7d8866` (`Phase2 - route class details
conflict checks through typed query`) is accepted. Fresh current verification
used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. The short-path
build at `C:\Users\wflet\AppData\Local\Temp\p2f149\current` completed
successfully. The focused current selector
`^(ClassMngrNextApplicationClassDetailsScheduleConflictQueryTests|ClassMngrNextPlatformApplicationServicesClassDetailsScheduleConflictPortTests|ClassMngrClassDetailsSavePageTests|ClassMngrClassDetailsPageSaveParityTests)$`
passed 4/4. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity CTest passed 1/1, covering
both F148 successful save and F149 regular/intensive conflicts.

The baseline overlay added the same parity test and registration, and changed
only three Qt minimum versions from 6.11.1 to installed 6.12.0; no production
source was overlaid. Because the baseline dialog API predates the current
prompt driver, its temporary parity harness uses
`setUserPromptServiceForTesting` and a recording `IUserPromptService`; it keeps
the same seed, save action, warning title/body, no-write, and dirty/header
assertions. Current source SHA-256 is
`FF7C5A35C33D5F38E290E4DCFCC943BE5A38DC6E1E98DC659788DCACBA98E74F`; adapted
baseline harness SHA-256 is
`41AA9B1F7C5D26DF50645FDD8D6FCC2A5EB17BF3E7D9350033DA5FFFBA5D3F41`.
The first current build hit MSVC C1083 because an object path was 265
characters; the fresh short-path build reduced it to 233 and passed. No full
suite or full application build ran.

Three independent F150 reviews and the main review selected Class Details
pre-save normalization/validation as the next bounded slice. It will use a
Qt-free typed Domain/Application policy and preserve current page feedback,
save-blocking, and exact validation order. Keep the ClassService save-time
validation guard and F149 conflict query unchanged. Preserve book-catalog
rules, raw malformed schedule diagnostics, duplicate-slot semantics, hidden
field checks, and stable issue ordering. Use deterministic first-seen order
for duplicate-slot groups because the legacy QHash group order is unspecified.
The main implementation risk is avoiding a second book catalog while
representing raw schedule text in the typed input.
F150 implementation/test commit `854f9849` (`Phase2 - move class details
validation into typed policy`) is accepted. The policy takes typed fields,
raw regular/intensive schedule rows, and a catalog snapshot sourced from the
existing `ClassInfoConfig`. The page maps structured issues to its existing
validation binder, blocks invalid saves before F149 conflict queries, and
converts normalized values back to the save model. The `ClassService`
save-time validation guard and F149 conflict order/warnings remain unchanged.

Independent fresh Windows x64 Debug verification used CMake 4.4.2, Ninja
1.13.2, MSVC 19.51.36257.0, and Qt 6.12.0. Configure validated 1,047
handwritten source owners. `ClassMngrNextApplicationClassDetailsValidationPolicyTests`,
`ClassMngrClassDetailsSavePageTests`, `ClassMngrClassDetailsPageSaveParityTests`,
and `ClassMngrSharedPolicyTests` passed 4/4. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity target passed 1/1. Its
overlay included only parity source/registration and Qt minimum changes from
6.11.1 to installed 6.12.0; no production source was overlaid. Common inputs
cover successful save, regular/intensive conflicts, and invalid save. The
hidden notes/activity whitespace regression passed on both revisions. The
page-level mapping tests are representative rather than exhaustive for every
field/focus pair. No full suite or full application build ran. A first
baseline scratch attempt failed during incomplete extraction; the clean
re-extracted baseline run passed. The production fix after review ensured
normalized hidden fields are applied during save conversion.

F151 parity-test commit `f232301e` (`Phase2 - add class details validation
parity cases`) is accepted. It adds four common-input live-page cases for
malformed regular input, malformed intensive input, end-before-start, and two
duplicate groups with a unique row. They check issue feedback and locations,
dirty state, no visible conflict warning, no saved signal, and unchanged
target/source records. Duplicate membership and row-specific feedback are
asserted without comparing cross-group order.

Independent fresh Windows x64 Debug verification used CMake 4.4.2, Ninja
1.13.2, MSVC 19.51.36257.0, and Qt 6.12.0. Current CTests passed 4/4:
`ClassMngrNextApplicationClassDetailsValidationPolicyTests` (8 QtTest cases),
`ClassMngrClassDetailsSavePageTests` (12),
`ClassMngrClassDetailsPageSaveParityTests` (6), and
`ClassMngrSharedPolicyTests` (83). Configure validated 1,047 handwritten
source owners. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity target passed 1/1 CTest
(6/6 QtTest cases); baseline production source matched its pinned Git blob.
The overlay was limited to parity test source/registration and three Qt
minimum changes to installed 6.12.0. The parity cases use a seeded conflict as
a warning trap; they do not count repository queries. Current page tests
assert zero conflict-port requests. No full suite or full app build ran.

F152 implementation/test commit `f70e3e23` (`Phase2 - add typed class details
validation context read`) is accepted. The Qt-free query uses typed `ClassId`,
checks returned identity, and preserves raw signed teacher ID and exact UTF-16
notes/activity. Its Platform adapter reads through the active session's
`ClassInfoRepository::loadClassInfo()` with no `ClassService` or `DataService`
fallback. `ClassDetailsPage` queries on every save immediately before
validation; it does not reuse the load-time display snapshot. Query failure
uses `ClassInfo{}` values and continues through validation, F149 conflict
checks, and save. The context is not in the save request; the existing save
adapter reread and `ClassService` guard remain.

Independent Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257.0, and Qt 6.12.0. Configure validated 1,051 handwritten
source owners. The new Application query, Platform adapter, F150 policy,
ClassDetails page save/display, parity, and shared-policy targets passed 7/7.
The pinned baseline `f232301e48f1e198d301acdfa3d8f704569f7ddc` parity CTest
passed 1/1 with only parity-test source overlaid; its production page source
matches the pinned Git blob. Parity verifies persisted hidden fields changed
after page load and valid visible save preservation. No full suite or app
build ran. Initial verification found a missing `QSqlError` include and
unseeded `class_info` test rows; both fixtures were repaired and the fresh
recheck passed with `CL` cleared and embedded debug info.

F153 parity test commit `477ed151` (`Phase2 - add teacher ID validation parity
case`) is accepted. Independent Windows x64 Debug verification passed current
parity and page-save targets 2/2; page-save includes the F152 direct
no-conflict-query regression. The original pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` passed the focused F153 parity
harness 1/1. Baseline overlays were limited to adapted parity test source,
registration, and three Qt minimum bumps; production page source matched blob
`cdc48da8e3bab73dd0e064cf8364899f67ad1021`. Toolchain: CMake 4.4.2, Ninja
1.13.2, MSVC 19.51.36257.0 x64, Qt 6.12.0; `CL` cleared, embedded debug info.
The baseline run was a focused F153 harness, not the full expanded parity
suite. Parity uses a seeded conflict warning trap and does not count repository
queries; current-only tests assert no conflict requests. No full suite or app
build ran. Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open. No
push was requested.

F154 implementation commit `8bcbf136` (`Phase2 - add Class Notes typed read
boundary`) is accepted. It adds a typed Qt-free page read query/port and
active-session Platform adapter with independent class/teacher results; the
page uses it for load/discard while preserving text trimming, subtitle
formatting/fallback, and the existing save port. No `DataService` fallback is
used, and refresh/save add no read.

Independent Windows x64 Debug verification passed six current focused CTest
targets 6/6, including both save-port targets. It verified query identity and
error behavior, mapping and independent source failures, load/discard counts,
and no reads on refresh/save. The original baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity harness passed 1/1 for
initial text/subtitle and discard reload. Baseline overlays contained only
parity source/registration and Qt minimum bumps; no production overlay. Pinned
baseline page/header match blobs `bbc9bc24a053aca83434eba6efac1e4ad5801bc2`
and `5c825327f1393791d7101ab33c10999768ff639a`. Toolchain was CMake 4.4.2,
Ninja 1.13.2, MSVC 19.51.36257.0 x64, Qt 6.12.0; `CL` cleared with embedded
debug info. Current tree configure validated 1,058 handwritten source owners.
The F154 current-source test ran at prior HEAD `a8c909dd` with the uncommitted
F154 patch, then that same source was committed unchanged as `8bcbf136`. Logs:
`%TEMP%\p2f152\testsc-f154.log` and
`%TEMP%\p2f153\test-baseline-notes-parity.log`. No full suite/app build
ran. Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open. F155
is selected for a dedicated Class Co-Teacher page read query and active-session
Platform adapter. Return selected teacher ID and the class/teacher display
fields used by `SidebarNodeNaming`, with independent read outcomes. Use the
query on load/discard and after successful assignment; preserve missing/read-
error fallbacks. Keep the existing teacher-choice catalogue and assignment
use case/adapter out of scope. Current tests should verify call timing and
failures; original-pinned-baseline parity should cover selected value/title,
discard reload, and post-save title updates. Gates 1 and 2 remain Partial;
Phase 2 remains In Progress/Open. No push was requested.

An older F123 candidate handoff predates commit `9f7e736b`, which contains
TeacherInfoPage use-case integration and page tests. That production integration
is present in the current checkout; this continuation did not re-verify its
CTest target or baseline parity and makes no new acceptance claim for it.

F155 commit `2d810d0e21f85233575b700e927ec3a36c907f21` adds a dedicated typed
Co-Teacher page read query and active-session Platform adapter for selected
teacher ID and class/teacher title inputs. The page uses it on load/discard
and after successful assignment. The teacher-choice catalogue and assignment
save boundary remain separate; read failures preserve existing fallback
behavior.

Windows x64 Debug verification passed six current focused CTest targets 6/6
(read query, Platform adapter, page, parity, and existing Application and
Platform assignment tests) using CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, and
Qt 6.12.0. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity harness passed 1/1 with only
test source/registration and Qt minimum overlays; its production page/header
blobs are `d25263eda8d464b2a3b17a35f44d6376ee5db588` and
`bba856ebb2ed907072d266a38bf3abe4e939d95e`. Both revisions cover initial
selection/title, discard after an external change, and post-save selection,
title, and persistence. No full suite/app build or baseline query-count claim.

F156 is selected for a separate typed Application teacher-catalogue read and
active-session Platform adapter for Co-Teacher. Replace the page's
`TeacherService::teachers()` read with a projection limited to fields consumed
by `TeacherInfoSection`; preserve bilingual ordering, selection, displayed
details, and load-failure warning/clear behavior. Keep F155's selected-class
snapshot and assignment save unchanged. Three independent reviews recommend
this boundary. Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open.
No push was requested.

F156 commit `3581078bdca61cfe76489ff19c5d518d8b3145bb` adds a Qt-free teacher-
choice snapshot/query/port and active-session Platform adapter. It returns
typed teacher IDs, Korean/English names, room, internet type, Wi-Fi fields,
projection type, and Zoom fields. The feature boundary reconstructs the UI
`Teacher`; `TeacherInfoSection` retains sorting and formatting. The page reads
choices on load/discard, preserves the load-failure warning/clear path, and
keeps F155's selected-class snapshot and the existing save port/use case
separate. The adapter uses `TeacherRepository::getAllTeachers()` directly
without `DataService` or `TeacherService` fallback.

Fresh Windows x64 Debug configure validated 1,071 handwritten source owners.
The serial focused build and combined CTest run passed 8/8 in 1.91 seconds
with CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, and Qt 6.12.0. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity harness passed 1/1 using
only adapted parity-test database calls/registration and Qt minimum changes;
no production overlay. Baseline ClassCoTeacherPage page/header blobs match
`d25263eda8d464b2a3b17a35f44d6376ee5db588` /
`bba856ebb2ed907072d266a38bf3abe4e939d95e`. Both parity runs assert crossed
Korean/English ordering, None first, selected details and title on load and
discard. The current tests also cover save/post-save behavior. No full suite or
app build; baseline query counts are not compared. Three independent reviews
recommended distinct next candidates. F157 is selected to replace
`TeacherInfoPage`'s page-local `TeacherServiceProfileEditPort` with an
active-session Platform adapter to `TeacherRepository`, retaining the existing
edit use case and validation policy. Acceptance covers adapter field/error
mapping with no fallback, valid common-input save and canonical reload parity,
invalid-write blocking, dirty state and save signal on current and baseline.
## F157 accepted; F158 selected - 2026-09-30

F157 commit `cb6199f3369420c0e2e6c77d11f853d2799d9f92` replaces the page-local
Teacher Profile `TeacherService` persistence port with an active-session
Platform adapter using `TeacherRepository` for update and canonical reload.
All profile fields map in both directions. The edit use case and validation
policy remain in place, as do warning, dirty-state, and save-signal behavior.

Fresh Windows x64 Debug configure validated 1,074 source owners. The serial
focused build and CTest passed 4/4: Teacher Profile Application use case,
Platform persistence port, TeacherInfoPage, and public-page parity. Toolchain:
CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, Qt 6.12. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity test passed 1/1 with only
the parity source and single CMake registration overlaid; no production
overlay. Baseline page/header blobs are
`49e9ee45226ddb2f456d73c369901ad459a6eb27` and
`e279133ab8c601d5e514b3531b9e2e25a62604f1`. The parity case covers valid
normalized save/reload and invalid-write blocking. No full suite/app build or
baseline query-count claim. The current page target and baseline page parity
reverify the tested F123 save/use-case behavior; no broader F123 acceptance is
claimed.

Three independent reviews compared a Native English Staff Directory read with
the adjacent Teacher Profile navigation read. F158 is selected for a typed
Application read query and active-session Platform adapter at
`NavigationController::handleTeacher`, backed directly by
`TeacherRepository::getTeacher()`. Keep it distinct from F157's edit port and
preserve lookup/failure/leave-confirmation/page-load behavior. Acceptance will
cover typed IDs, field mapping, session and repository errors, no fallback,
current route behavior, and pinned-baseline visible parity. No query-count
comparison on the baseline. Gates 1 and 2 remain Partial; Phase 2 remains In
Progress/Open. No push was requested.

## F158 and F159 accepted; F160 selected - 2026-09-30

F158 source commit `4d099893071d4271ea8873d2219dfc7642de1e5c` adds a typed
Teacher Profile read query and active-session Platform adapter backed by
`TeacherRepository::getTeacher()`. `NavigationController::handleTeacher()`
retains lookup-before-confirmation order and silently leaves the current page
unchanged when lookup fails. Focused current build and CTest passed 8/8,
including F157 edit regressions, the read query/adapter, route behavior, and
visible parity.

Pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` navigation parity
passed 1/1. Baseline production blobs remained pinned. Because the installed
Qt is 6.12.0 while baseline CMake requests 6.11.1, configure used
`QT_NO_PACKAGE_VERSION_CHECK=TRUE` and a temporary three-line Qt version
metadata shim before `qt_standard_project_setup()` in the scratch checkout;
the shim was removed afterward and the CMake file hash restored to
`cc8a061dfa64977926805167cc10418ca15d83d8`. The copied scratch parity test
used the baseline's three-argument `NavigationController` constructor; no
repository test or production source was changed for that adaptation. No full
suite/app build or baseline query-count claim.

F159 source commit `1849ed23327538e2d21b05dfe0cebcc97e99d78c` adds a typed
Qt-free Application read query and active-session Platform adapter for only
the Native English branch of `StaffDirectoryPage::loadDirectory()`, backed by
`NativeEnglishTeacherRepository::getAll()`. It preserves six displayed fields,
row ID role, repository order, navigation confirmation-before-read, and
silent unavailable-session versus warned repository-error behavior. The
dedicated `NativeEnglishTeacherId` remains separate from Korean teacher IDs.

The focused current build and CTest passed 5/5 targets: Application query,
Platform adapter, Native English page behavior, pinned-baseline parity, and
`StaffDirectoryPage` regression. Pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` public-page parity passed 1/1. The
scratch parity setup used the existing verified baseline cache, a temporary
Qt 6.12 metadata shim (removed afterward), and test-only API/registration
adaptations; production files and pinned baseline blobs were not overlaid. No
full suite/app build or baseline query-count claim.

## 2026-09-30 — Phase 2 continuation: F161-F163 accepted; F164 selected

At continuation start, reviewed commits `314f4d5b` and `0c5d5566`. No blocking
issue was found. One non-blocking review note: `314f4d5b` changed a PDF text
assertion to use `.simplified()`, which collapses internal whitespace and no
longer checks exact spacing. F160 was accepted in source commit
`b703d3260a01b783594ffe6b87d10d9f7b02d193` and acceptance-doc commit
`58494d91406cb24f41e89292f1d5174a2918fcd5`.

F161 source commit `ac173977d8d517d4af3236ee7794368bbe9a6bdc` adds a typed
Qt-free Application save/validation operation and active-session Platform
adapter for only the Native English branch of
`StaffDirectoryPage::saveDirectory()`, backed by
`NativeEnglishTeacherRepository::saveDirectory()`. Application owns empty and
duplicate comparison-key validation plus valid-or-blank birthday decisions;
the feature edge supplies Qt-normalized keys and date facts, retaining current
Unicode/date semantics and localized messages. Typed row/deleted IDs represent
existing, added, and removed rows. GS Team remains on its own save path.

Independent Tester evidence: focused current CTest passed 12/12, covering the
app-less policy/use case, direct active-session mapping and rollback, page
update/insert/delete, persisted state, reload/signal, warning and quiet
autosave, invalid dates/duplicate Unicode keys, and F159/F160/page regressions.
Pinned-baseline save parity passed 1/1 on
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, with only the parity test and
registration overlaid and no production overlay. The scratch test adapted the
baseline database-session API; temporary Qt 6.12 metadata setup was restored,
and root CMake returned to pinned hash
`cc8a061dfa64977926805167cc10418ca15d83d8`. No full suite/application build or
baseline query-count claim.

Two independent Explorers mapped remaining paths; three independent
Investigators compared cohesion, feasibility, and parity.

F162 source commit `00a56324f1435475e3a7479fec99bc2e01653495` adds a
GS Team-specific typed Application save operation and active-session Platform
adapter backed by `GsTeamRepository::saveDirectory()`. It uses
`GsTeamMemberId`; requires at least one English or Korean name; enforces key
uniqueness within each language namespace while allowing cross-namespace
matches; and accepts valid or blank birthdays. The page preserves typed row
and deleted IDs, transaction behavior, warning and quiet autosave, dirty state
on failure, reload, and `directorySaved`.

Independent Tester evidence: focused current CTest passed 16/16, covering the
F159-F162 policy, adapter, persistence, and page regressions. Pinned-baseline
GS Team save parity passed 1/1 on
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, with only the parity test and
registration overlaid and no production overlay. Current C: linking exhausted
disk; a serial focused build and test passed from D: scratch. Baseline setup
adapted three database-session calls and used a temporary Qt metadata shim;
root CMake was restored to pinned hash
`cc8a061dfa64977926805167cc10418ca15d83d8`. No full suite/application build or
baseline query-count claim.

F163 source commit `a556c0441dcebbb3b6a7baecef6e293b5644b149` adds a typed
class ID/name list query and active-session Platform adapter backed directly by
`ClassRepository::getClasses()`. `ClassesPage::openClass()` and the
post-ClassInfo-save handler use the typed read; the separate navigation
metadata read and other legacy class calls remain unchanged. The adapter
preserves repository order, names, and IDs and has no DataService fallback.
The page preserves selection and existing open/reload failure behavior.

Focused current Application and Platform CTests passed 2/2. Direct ClassesPage
slots for active-repository reads, post-save visible refresh, ClassInfo
navigation refresh, and navigation metadata failure passed. Current and pinned
baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` both passed visible
all-grade list parity and the post-save refresh slot. The post-save fixture
changes visible class order from `[43 Athena, 42 Hercules]` to
`[42 Hercules, 43 Zulu]` while retaining class 42. The baseline was extracted
from the exact commit; only `tests/classes_page_tests.cpp` and
`tests/schedule_widget_test_stubs.cpp` were overlaid. Production hashes
matched the pin. Its Debug/Ninja build used Qt 6.12.0 with
`QT_NO_CONFIG_VERSION_OVERRIDE_FILES=ON` for the older pinned Qt requirement.
Current and baseline builds ran from D: scratch because C: had about 30 MB
free. The full `ClassMngrClassesPageTests` CTest stalled in the existing
`classDetailsAndCoTeacherTabsSeparateTheirSectionCards()` slot after
`nestedEditorsAreDeferredUntilTheirSectionIsOpened()` passed. No full suite or
application build and no query-count claim.

Three independent Investigators compared remaining candidates. F164 is selected
for the single selected-class grade read in `ClassesPage::rebuildSectionTabs()`.
Add a typed class ID/grade Application read with an active-session Platform
adapter backed by `ClassInfoRepository::loadClassInfo()`. Preserve the
middle-school grade rule, preference override, section selection, and current
quiet fallback that shows Analytics/Evaluations when the grade read is
unavailable or fails. Keep the subtitle’s ClassInfo/Teacher reads and
navigation metadata read separate; this keeps their existing failure paths
independent. Acceptance covers typed mapping, session/repository errors without
fallback, tab visibility and preference behavior, and pinned-baseline visible
tab parity. Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open. A
separate worker owns workflow repair; this work does not modify
`.github/workflows/refactoring-baseline.yml`. No push was requested.
