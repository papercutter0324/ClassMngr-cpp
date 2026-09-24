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
- Handoff: F34 selection is being checked against the Phase 2 plan and exit
  gate. The paired discovery reports differ on whether to route Sub Prep's
  all-years calendar read or Class Notes save first. No next-slice decision has
  been made yet. Workbook decoding, generic settings, remaining feature
  services, broader document work, and the Phase 2 exit gate remain open.
  Nothing was pushed; preserve user-owned commit `f5af92df` and its Start Here
  content unchanged.
