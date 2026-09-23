# Project Progress

Active deployment plan: Qt Rewrite Phase 2 — Domain Model and Application Contracts.
Current deployment: phase2_complete_continue_20260923 (executing). Route: Heavy.
Phase 1 hosted acceptance is closed on commit `0883009d`; the local branch adds
continued Phase 2 domain and application-contract work on top of that verified
baseline.

## Goal

Define explicit, Qt-free ClassMngrNext application contracts with structured
results, bounded projections, and deterministic tests while keeping the legacy
application available as a compatibility oracle until its later cutover.

## Overall Progress

Phase 0 completed on 2026-09-18. The combined route gate passed all 24 required
routes on Windows x64 and macOS universal, and the user confirmed the retained
visual references. The Phase 0 plan update in commit f8bb5954 records this
closure; earlier Phase 0 open notes are superseded.

Phase 1 slice 1.1 adds a Qt Core-only ClassMngrNext console bootstrap and a
focused CTest launch probe. It has no window and does not link the legacy
runtime.

Slice 1.2 establishes explicit next-generation layer and feature interface
targets in `cmake/next.cmake`. Configure-time assertions enforce the planned
dependency edges. The targets are intentionally source-free at this stage;
`ClassMngrNext` remains independently linked only to Qt Core.

Slice 1.3 measured Qt/Zlib header dependencies per legacy production object
target and moved those modules out of the shared build-settings target. The
legacy runtime keeps the complete module union required to link the current
application and tests.

Slice 1.4 replaces recursive production source discovery with explicit lists
for all six legacy object targets, the two executable entry points, and the
calendar QML files. Included roster `.inc` fragments are explicit header-only
inputs. A configure-time ownership check compares the handwritten `src` and
active test inventories with their targets; shared schedule test doubles now
have one object-library owner each.

Slice 1.5 adds scoped formatting/static-analysis CI for `src/next`, compile
database validation, a configure-time Qt Core-only link assertion for
`ClassMngrNext`, generated Qt-module and resource-pack reports, resource
reference checks, staged-package build reports in the Release workflows, and
startup/memory/performance CTest labels. A clean Windows Ninja/MSVC Debug
configure and full build passed (351 steps); `ClassMngrNextLaunch` passed
(1/1). The module report lists only `Qt6::Core` for `ClassMngrNext`; the
resource check passed for six generated RCC packs and seven runtime IDs, and
the staged-package report probe passed. Cross-platform CI and local
`clang-format`/`clang-tidy` were not run.

## Current Position

### Windows/macOS source ownership regression — 2026-09-19

Commit `898cd3fc` added a Linux-only process-memory test, but the cross-platform
source-ownership inventory included its file on Windows and macOS without an
owner. `cmake/source_ownership.cmake` now excludes that source from non-Linux
inventory while retaining Linux ownership validation. Windows x64 Debug
configuration passed with 653 source owners; a clean build passed 649/649
steps and CTest passed 66/66 using VS18/MSVC 19.51 and Qt 6.12. An independent
fresh configure also passed. This is supplemental to the VS2022 hosted gate;
macOS and post-fix hosted checks remain unverified.

Slice 1.6 makes the existing Debug baseline workflow run for relevant pull
requests and covers Windows x64, Windows ARM64, macOS universal, and Linux.
Native jobs run the baseline build/tests including the `ClassMngrNext` launch
probe; the ARM64 job cross-builds `ClassMngr` and `ClassMngrNext` without
executing them on its x64 runner. Release package workflows remain unchanged
and use the production install/deployment paths. Slice 1.6 is committed.
Independent static review and preset checks passed.

On clean source snapshot `6f2f5fb0`, local Windows x64 Debug baseline configure,
build, and CTest passed 66/66, including `ClassMngrNextLaunch` and the startup
performance test. A Release configure/build, CMake install, and Inno Setup
installer target also passed under Visual Studio 2026/MSVC 19.51 with Qt 6.12.
The staged installer launched with exit 0 and `finalProgress=100`; six RCC
packs, seven runtime IDs, and seven references passed the resource/report
checks. This is supplemental local evidence: the Windows 2022/VS17 generator
could not find a VS2022 instance, so the local run does not prove the hosted
shipping toolchain.

After the local Qt 6.12 update completed, a fresh Windows x64 Ninja/MSVC Debug
configure and full build passed on the working tree based on source commit
`4dbe3ca7`. Configure-time ownership validated 653 handwritten source files;
CTest passed 66/66 in 179.41 seconds. The resource-reference check passed for
six RCC packs, seven runtime IDs, and seven references, and the build report
was generated. These local results use VS 2026/MSVC 19.51 and remain
supplemental to the VS2022 hosted toolchain.

A local macOS 27.0 arm64 / Qt 6.12 Debug universal validation passed for
ClassMngr and ClassMngrNext; the source ownership check passed for 654
handwritten files, both executables passed arm64/x86_64 and macOS 14.4 minimum
checks, and ClassMngrNext links only Qt Core. Release installer/DMG creation,
signature and hdiutil verification, 92 bundled Mach-O compatibility checks,
the resource check (6 RCC packs, 7 runtime IDs, 7 references), and build report
passed. A fresh isolated Debug configure and 656-step build also passed. The
first full CTest run under restricted Codex execution reported 62/67 because
LaunchServices, display, and loopback services were unavailable. Rerunning the
same 67-test suite with normal macOS service access passed 67/67 in 67.70
seconds, including `ClassMngrNextLaunch`, `InitialSetupWizard`, and the updater
tests. Both Debug executables are universal and target macOS 14.4; `ClassMngrNext`
links only Qt Core. The passing JUnit report and CTest log are preserved under
`build/phase1-macos-debug-local-20260918/Testing/`.

Phase 1's official acceptance targets were Windows x64 and macOS universal.
The hosted `Refactoring baseline` run `35424488211` passed Windows x64 Debug
66/66 and macOS universal Debug 67/67 on commit `0883009d`; the informational
Linux x64 and Windows ARM64 jobs also completed successfully. The hosted Phase
1 Build Quality run `35424488214`, Dialog policy run `35424488244`, and
Windows, macOS, and Linux Release runs `35424488203`, `35424488198`, and
`35424488209` all passed. This closes the Phase 1 hosted gate; Linux and
Windows ARM64 remain informational for this phase.

## Linux Phase 0/1 follow-up — 2026-09-19

The user requested Linux follow-up while preserving Phase 0's completed
Windows x64 and macOS universal official gate. Commit `749c9ba6` adds a Linux
x64 packaged Release route runner, validator support, runner tests, and an
opt-in hosted workflow. Python runner tests passed 9/9; validator self-tests
passed 17/17; Python compilation and workflow YAML parsing passed.

The local packaged run built the Release package and harness, then attempted
all 24 routes. Xvfb could not create its display socket because the sandbox
does not permit the required root-owned `/tmp/.X11-unix` directory. No route
passed; the validator correctly records the supplemental Linux baseline as
failed. The hosted workflow installs Xvfb and uses the staged package's xcb
plugin, but no hosted run was available from this environment. This attempt
does not revise the original Phase 0 official gate.

Commit `898cd3fc` fixes Linux process-memory sampling. Qt's `QFile::atEnd()`
treated zero-sized procfs pseudo-files as exhausted, so `/proc/self/status`
was not read. The provider now reads until `readLine()` returns no data and
has Linux tests for injected procfs input and live sampling. The snapshot and
startup-performance tests passed. Full local CTest passed 66/67; the updater
listener tests failed because this sandbox denies socket creation with
`EPERM`. Build/resource reports and `ClassMngrNextLaunch` passed. Local
format/tidy/actionlint tools were unavailable; YAML parsing passed. The
hosted Linux rerun remains unverified.

## Next Milestone

Phase 2 Work Packages D and E and Sub Prep output packages F1-F8 are complete.
F9 adds one- and five-second working-set checkpoints to the Sub Prep output
workflow test and records the packaged Windows x64 Release route. The selected
route passed validation and generated two PDFs. Peak working set was
315,740,160 bytes and peak private usage was 351,821,824 bytes, both below the
temporary 512 MiB diagnostic ceiling and lower than the retained legacy
baseline. Settled working set was 306,466,816 bytes at one second and
306,470,912 bytes at five seconds, so the final 250 MiB target remains open.

The calendar importer now persists accepted candidates through a typed,
ordered batch port while preserving one transaction and duplicate-only no-op
behavior. Continue the broader Phase 2 exit work with the typed calendar
UI/page migration. The calendar preferences reset mutation now uses a typed
delete-all port; its availability guard, confirmation, warning, status, and
refresh behavior are preserved. Import-start and dialog-opening availability
guards now query the typed Platform calendar boundary; the calendar feature no
longer calls `ApplicationServices::calendarService()` directly. Generic
settings persistence, remaining feature-service migrations, and broader
document-service migration also remain open. The calendar page now passes
typed edit drafts from activation or new-event creation through the dialog and
into typed save, repeat, and delete requests without a legacy event-object
round trip. The preferences panel now passes `ApplicationServices*` to its
typed event-display preferences adapter and no longer retains
`SettingsService*` for that preference pair. Keep the Linux Phase 0 follow-up
separate until it can run on a host with Xvfb and loopback access.

AcademicCalendarProvider now owns injected Application schedule and first-day
preference ports. CalendarPage and evaluation-default selection construct the
Platform adapters at their boundaries, so the provider no longer references
SettingsService. Windows x64 Debug built ClassMngr and the academic-calendar
and three preference-port suites; focused CTest passed 4/4. Continue the
broader Phase 2 work with generic settings, feature-service, and document
migrations.

Calendar event-type color reads and writes now use the Application preference
port through a Platform adapter constructed from ApplicationServices*. The
adapter preserves the existing default-color fallback and unavailable-save
no-op. Its focused Platform suite passed 1/1 after the Windows Debug app and
test targets built.

The current-campus Application preference port now exposes a Qt-free
availability query. CalendarPage uses it to gate existing display-preference
reads and campus projection, removing the final SettingsService reference
from the calendar feature. Independent Windows x64 Debug build and focused
CTest passed; there is no dedicated CalendarPage behavior test target.

### Phase 2 CalendarPage campus metadata query - 2026-09-24

CalendarPage's campus-directory lookup now uses a Qt-free Application query
port and Platform adapter over CampusJsonRepository. CalendarPage no longer
reads CampusJsonRepository or ResourcePaths directly; F22's importer query
remains separate. Executor and independent fresh Ninja/MSVC x64 builds
validated 878 handwritten owners and built ClassMngr, CalendarEventCache, and
the F22/F23 adapter suites. Focused CTest passed 3/3 in each build. Source
comparison confirmed availability timing, alias order, matching, trimmed
display fallback, whitespace-only code preservation, and final empty removal
and deduplication. No dedicated CalendarPage behavior target exists. Next:
cut over remaining PersonalSignatureImagePort callers to ApplicationServices*,
then continue custom-color, workbook, generic-settings, feature-service, and
document boundaries. The Phase 2 exit gate remains open.

### Phase 2 kickoff — 2026-09-19

The v2 Domain boundary now contains header-only, Qt-free typed identifiers and
structured operation results in `src/next/domain/`. CMake records both headers
under `ClassMngrNextDomain`, and `ClassMngrNextDomainContractTests` verifies
empty-ID rejection, type separation, value/error results, and void success
results without constructing a `QApplication`. Local Windows Debug configure,
build, the new contract test, and `ClassMngrNextLaunch` passed.

Phase 1 hosted acceptance is closed; no Phase 1 production implementation is
being reopened while Phase 2 contracts begin.

### Async prompt title repair — 2026-09-19

The asynchronous `QMessageBox` prompt title regression is fixed in
`src/ui/shared/dialogs/user_prompt_service.cpp`. Synchronous and asynchronous
acknowledge prompts now share the complete configuration path, and the
requested title is preserved on the actual widget before `exec()` or `open()`.
Independent verification passed `ClassMngrDialogServicesTests` (1/1), the
direct async test (3/3), synchronous/shared-policy coverage, and
`git diff --check`. A local full CTest run was 62/67 because five unrelated
GUI/loopback tests require unavailable screen or port services; no dialog
service test failed. No workflow files were changed.

### Phase 2 ActionRegistry persistence pause — 2026-09-23

The latest committed slices are theme persistence (c30f13e0), removal of the
generic OptionState SettingsManager fallback (7caef52e), and typed file-dialog
directory preferences (df8202ed). Theme keeps the existing options/theme
values Dark=0, Light=1, and SystemDefault=2. All eight ActionRegistry option
states now use typed persistence callbacks; OptionState no longer accepts a
settings key or saves through SettingsManager.

The file-dialog preference contract is Qt-free and owned by Application. Its
QSettings adapter preserves all eight file-dialog/directories/<purpose> keys
and existing purpose slugs. QtFileDialogService consumes the Application port;
main constructs the adapter and service before MainWindow, while the existing
test-service override retains priority. CMake records the new source ownership
and the Application dependency on legacy ClassMngrUiShared without adding a
UI-to-Platform dependency.

Verification passed the theme port and AI comment options tests (2/2), the
AI comment options and startup visual settings tests (2/2), and the dialog
services and QSettings adapter tests (2/2). The final configure validated
840 handwritten sources and the ClassMngrNext dependency assertions. ClassMngr
and both focused test targets built, and git diff --check passed. The home
directory fallback assertion is conditional on the platform returning an
empty writable location; that condition was not forced during verification.
No hosted cross-platform run was performed.

Historical note (superseded): Phase 2 was paused at the user's request when
this update was written. Later work resumed the plan. The earlier preference
scan found no other live direct application preference persistence candidate
at that time; unused legacy settings helpers and the read-only PowerPoint
registry probe remain outside the migration scope. Keep the supplemental
Linux Phase 0 follow-up separate until a host with Xvfb and loopback access is
available.

### Phase 2 calendar import signature-query boundary — 2026-09-24

The import workflow now reads existing duplicate signatures and checks
availability through a Qt-free Application query port. A dedicated Platform
adapter owns the legacy calendar-service access; the former concrete signature
method was removed. The six-field UTF-16 identity, order, errors, and uncapped
range behavior are preserved. Independent Windows x64 Debug verification
built `ClassMngr` and both focused targets; CTest passed 2/2, CMake validated
874 source owners, and `git diff --check` passed. Workbook parsing and
campus-directory lookup remain open. Next: continue typed preference caller
migrations, then the remaining feature-service and document boundaries.

### Phase 2 personal display-name caller cutover — 2026-09-24

Schedule output/import and Sub Prep print-dialog now consume the existing typed
personal-display-name adapter through `ApplicationServices&`, preserving the
caller-specific trimming, acceptance timing, unavailable defaults, and Sub
Prep's baseline preference-write order. The added ScheduleWidget cases verify
accepted-dialog reads, whitespace, unavailable settings, and null services.
Independent Windows x64 Debug verification passed three focused targets
(3/3); the ScheduleWidget suite passed 19/19. My Information and Initial Setup
remain the last pointer-constructor consumers before that overload can be
removed. Phase 2 continues with those preference reads, generic settings,
remaining feature services, and broader document boundaries.

### Phase 2 personal display-name adapter constructor removal — 2026-09-24

My Information and Initial Setup now construct the display-name adapter from
`ApplicationServices&`; the adapter's `SettingsService*` constructor and its
constructor-only test were removed. Existing availability guards, exact UTF-8
and whitespace behavior, Setup's fill-only-when-blank rule, and aggregate
personal-details saves remain intact. A fresh isolated Ninja/MSVC x64 build
completed 322 steps. Initial Setup and adapter CTest targets passed. The
MyWorkspace target's F20 display-name, availability, aggregate-save, and
rollback cases passed, while three PageManager cases failed because the
`documents` and `campuses` resource packs were unavailable before F20 code ran.
The independent Tester found no F20 defect; `git diff --check` passed.
Next: remove the unused `ClassNavigationPreferences` helper and stale build
references, then continue the open generic-settings, feature-service, calendar
workbook/campus, and document migrations against the Phase 2 exit gate.

### Phase 2 ClassNavigationPreferences cleanup — 2026-09-24

Removed the unused `ClassNavigationPreferences` API and implementation, both
CMake source-owner entries, and stale includes. Speaking Eval now includes
`class_tab_navigation_model.h` directly for `ClassTabNavigation`; active typed
Application contracts and Platform adapters remain. CMake validated 872
handwritten source owners. Independent fresh Ninja/MSVC x64 build passed for
ClassMngr, ClassesPage, ClassTabNavigation, evaluation-default selection, and
five typed preference targets; focused CTest passed 8/8. Source/CMake/build
metadata searches and `git diff --check` passed. Next: route the calendar
importer's campus-code directory lookup through Application and Platform,
preserving its ordered, trimmed, blank-filtered, duplicate-free result and
empty-directory behavior. Generic settings, other feature-service, workbook,
calendar-page campus lookup, and document migrations remain open.

### Phase 2 calendar-import campus-code query — 2026-09-24

Added the Qt-free `CalendarEventImportCampusCodeQueryPort` and a Platform
adapter over the campus resource directory and repository. The importer now
uses the port and converts the owning UTF-8 values to QString at its feature
boundary. Repository ordering, code trimming, blank removal, exact duplicate
handling, malformed/default record skipping, and the empty/missing-directory
fallback remain intact. CMake validated 875 owners; independent fresh
Ninja/MSVC x64 build passed for ClassMngr and both importer tests; CTest passed
2/2, including Korean UTF-8 fixtures. Source search and `git diff --check`
passed. Next: give CalendarPage a separate campus metadata query while keeping
its matching and alias behavior in the UI. Workbook decoding, generic settings,
other feature-service, and broader document migrations remain open.
