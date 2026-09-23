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

## Current Deployment Handoff: phase2_action_registry_persistence_20260923 (paused)

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
- Phase 2 is paused here. On resume, select a new application-contract slice
  from the plan. Keep the Linux Phase 0 follow-up separate until it can run on
  a host with Xvfb and loopback access.
