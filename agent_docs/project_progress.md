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
and deduplication. No dedicated CalendarPage behavior target exists. F24 and
F25 are now complete. The next bounded slice is the Sub Prep typed-preference
caller cutover, followed by workbook, generic-settings, other feature-service,
and document boundaries. The Phase 2 exit gate remains open.

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

### Phase 2 PersonalSignatureImagePort caller cutover - 2026-09-24

Initial Setup, My Information, and Speaking Eval now pass `ApplicationServices*`
to the read-only signature-image adapter; its `SettingsService*` constructor
was removed. The exact key, Base64 decoding, one-time image preparation, empty
results, and caller guards remain unchanged. Executor build and focused CTest
passed 3/3. Independent fresh Ninja/MSVC x64 configure validated 878 owners and
all targets built; adapter and Setup suites passed. Three MyWorkspace top-level
cases failed because `documents` and `campuses` packs were unavailable. All 18
MyWorkspace functions were run individually: F24 signature-image, missing,
corrupt, unavailable, display-name, and aggregate-save cases passed; only the
same three resource-pack-dependent cases failed. Next: remove the custom-color
adapter's remaining `SettingsService*` caller path while preserving stored
palette behavior. Workbook, generic settings, remaining feature services, and
document boundaries remain open; the Phase 2 exit gate is not met.

### Phase 2 custom-color adapter constructor cleanup - 2026-09-24

All seven custom-color picker callers across Schedule Editor, Testing Classes,
Schedule Import Review, shared Class Details, and Initial Setup now pass
`ApplicationServices*`. The adapter retains its reference constructor, adds a
nullable application-services constructor, and removes the settings-service
constructor. `custom_colors`, all 16 slots, legacy payloads, defaults,
unrelated settings, and load-before-dialog/save-after-dialog behavior
including cancel remain unchanged. Executor and independent fresh Ninja/MSVC
x64 builds validated 878 handwritten owners; both built ClassMngr and all six
focused adapter, ColorUtils, Setup, Testing Classes, Schedule Import Dialog,
and Schedule Widget suites. Both CTest runs passed 6/6; the independent repeat
build returned no work, and `git diff --check HEAD` passed. Next: remove
Sub Prep's raw settings-service gate around its typed saved-content, Zoom, and
current-campus preferences while preserving unavailable-service no-op
behavior. Keep Sub Prep's full campus directory and all-dates calendar reads
separate. Workbook, generic settings, other feature-service, and document
boundaries remain open; Phase 2 remains In progress.

### Phase 2 Sub Prep typed settings gate removal - 2026-09-24

Removed Sub Prep's raw `openSettingsService` helper. Saved-content and Zoom
paths now use their existing ApplicationServices-backed preference ports;
current-campus reads check the nullable typed port before changing campus
state. Saving returns before stopping autosave or restoring grading defaults
when settings are unavailable. New page tests preserve all preference fields
on unavailable loads and verify unavailable saves preserve dirty state, the
active timer, blank grading text, and stored settings. Executor and independent
fresh Ninja/MSVC x64 builds validated 878 handwritten owners, built ClassMngr,
the page suite, and all three preference adapter suites, and passed CTest 4/4.
Independent repeat build returned no work; `git diff --check HEAD` passed.
The test stub's database-open flag must be set false explicitly to exercise an
unavailable service. Next: move My Information's campus chooser directory
lookup behind an Application query and Platform adapter. Keep Sub Prep's full
office/Wi-Fi campus details and all-years calendar read separate. Workbook,
generic settings, personal-details atomic save, other feature services, and
broader document work remain open; Phase 2 remains In progress.

### Phase 2 My Information campus chooser query - 2026-09-24

My Information now reads its campus chooser through a Qt-free
`MyInfoCampusDirectoryQueryPort` and a Platform adapter. The page no longer
accesses `CampusJsonRepository` or `ResourcePaths::Campuses` directly. The
adapter preserves repository ordering, owning UTF-8 IDs, trimmed display names
with trimmed-ID fallback, and the raw ID values used as combo data. Saved
ID/name matching and correction writes remain unchanged. Executor and
independent fresh Ninja/MSVC x64 builds validated 882 handwritten owners and
built ClassMngr, MyWorkspace, and both new suites; focused CTest passed 3/3 in
both runs. The existing MyWorkspace test covers selection correction. The
codec normalizes blank IDs/names to `campus`, so the empty-label defensive
filter cannot be reached through repository fixtures. Next: route Sub Prep's
full campus office/Wi-Fi detail read through a separate Application query and
Platform adapter. Keep the all-years calendar query separate. Personal Details
atomic save, workbook, generic settings, other feature services, and broader
document boundaries remain open; Phase 2 remains In progress.

### Phase 2 Sub Prep campus detail query - 2026-09-24

Sub Prep now loads campus ID, display name, office number, Wi-Fi name/password,
and photocopier code through a Qt-free Application query and Platform adapter.
The page no longer accesses `CampusJsonRepository`, `ResourcePaths::Campuses`,
or `CampusInfo` directly. Repository order and omission, saved trimmed-ID or
display-name matching, first-campus fallback, raw ID selection, current-campus
availability timing, and `N/A` for empty detail fields are preserved. Executor
and independent fresh Ninja/MSVC x64 configures each validated 886 handwritten
owners. Executor focused CTest passed 3/3; independent CTest passed 6/6,
including all three Sub Prep preference adapters, and its repeat build had no
work. Campus resource generation succeeded. Next: cut the Personal Details
atomic-save adapter's remaining `SettingsService*` constructor and both UI
callers over to `ApplicationServices`, retaining the early availability guard
and atomic `saveAll` behavior. Workbook, generic settings, other feature
services, and broader document boundaries remain open; Phase 2 remains In
progress.

### Phase 2 Personal Details atomic-save caller cutover - 2026-09-24

The Personal Details save adapter now takes `ApplicationServices&` or nullable
`ApplicationServices*`; its `SettingsService*` constructor is removed. Initial
Setup and My Information pass their existing service owner. The save still
uses one atomic `saveAll` for all nine keys, preserving UTF-8 conversion,
signature-image preparation, normalization, rollback, and failure behavior.
My Information still returns before autosave cancellation or field
normalization when settings are unavailable. A page regression test preserves
whitespace Zoom values and dirty state in that case. Executor and independent
fresh Ninja/MSVC x64 configures each validated 886 handwritten owners; both
built ClassMngr and the adapter, InitialSetupWizard, and MyWorkspace suites,
and focused CTest passed 3/3. Next: move the separate read-only Personal
Signature Preferences adapter and its two callers to `ApplicationServices`,
preserving defaulting, normalization, UTF-8 text, and no-write behavior.
Workbook, generic settings, other feature services, broader document work, and
the Phase 2 exit gate remain open.

### Phase 2 Personal Signature Preferences caller cutover - 2026-09-24

The Personal Signature Preferences adapter now takes `ApplicationServices&`
or nullable `ApplicationServices*`; its `SettingsService*` constructor is
removed. My Information and Initial Setup pass their existing service owner.
Read-only keys, defaults, UTF-8 typed text, mode/font normalization,
unavailable failure behavior, and surrounding availability guards are
preserved. Executor and independent fresh Ninja/MSVC x64 configures each
validated 886 source owners; both built ClassMngr and the adapter,
InitialSetupWizard, and MyWorkspace suites, and both focused CTest runs passed
3/3.
Next: move the current-campus preferences adapter and three remaining My
Information/Initial Setup callers to `ApplicationServices*`, preserving
matching, correction timing, and unavailable read/write semantics. Workbook,
generic settings, other feature services, broader document work, and the Phase
2 exit gate remain open.

### Phase 2 current-campus preferences caller cutover - 2026-09-24

The current-campus preferences adapter no longer accepts `SettingsService*`.
My Information's campus read and correction write, and Initial Setup's campus
read, now pass their existing `ApplicationServices` owner. The `myInfo/campus`
key, UTF-8/`QVariant::toString()` conversion, unavailable empty-read and
no-op-write behavior, write-failure mapping, and My Information guard and
correction timing are preserved. Executor and independent fresh Ninja/MSVC x64
configures each validated 886 handwritten owners; both built ClassMngr and the
adapter, InitialSetupWizard, and MyWorkspace suites, and both focused CTest runs
passed 3/3. Next: cut the Personal Zoom preferences adapter and its My
Information/Initial Setup callers over to `ApplicationServices*`, preserving
primary-key precedence and best-effort legacy migration. Workbook, generic
settings, other feature services, broader document work, and the Phase 2 exit
gate remain open.

### Phase 2 Personal Zoom preferences caller cutover - 2026-09-24

The Personal Zoom preferences adapter now accepts `ApplicationServices&` or
nullable `ApplicationServices*`; its `SettingsService*` constructor is
removed. My Information and Initial Setup pass their existing service owner.
Primary `myInfo/zoom*` values still take precedence; legacy
`subPrep/personalZoom*` values are read and migrated best-effort only when
primary values are absent, and the legacy value is still returned if migration
fails. UTF-8 conversion, defaults, unavailable behavior, and page display are
unchanged. Executor and independent fresh Ninja/MSVC x64 CTest runs passed
3/3; the independent configure validated 886 handwritten owners and built
ClassMngr, the adapter, MyWorkspace, and InitialSetupWizard. Next: use the
existing `ApplicationServicesCurrentCampusPreferencesPort::isAvailable()`
contract to route My Information and Initial Setup's remaining settings
availability checks. Preserve early-return behavior and add unavailable My
Information load coverage. Workbook, generic settings, other feature services,
broader document work, and the Phase 2 exit gate remain open.

### Phase 2 typed settings availability guards - 2026-09-24

My Information and Initial Setup now use the existing typed current-campus
preferences availability query instead of exposing raw `SettingsService`
getters. Unavailable My Information loading returns before reading or changing
widgets; saving returns before autosave cancellation or Zoom normalization.
Initial Setup retains unavailable initialization and validation early returns.
Regression coverage checks My Information's sentinel fields and Initial
Setup's populated name/signature preview on unavailable validation. Executor
and independent fresh Ninja/MSVC x64 runs built the page, adapter, and wizard
targets; all three focused CTest suites passed 3/3. The independent configure
validated 886 handwritten source owners; source and diff checks passed.
Phase 2 remains open. Next: define a typed Sub Prep calendar projection that
preserves the all-years query behavior without silently applying the generic
4,096-event limit. Workbook decoding, generic settings, remaining feature
services, broader document work, and the formal exit gate remain open.

### Phase 2 Class Notes save boundary - 2026-09-24

Class Notes saves now pass through a Qt-free `ClassNotesSavePort` contract and
Platform adapter. The page no longer calls `ClassService::saveClassNotes()`;
its other reads remain unchanged. The contract uses UTF-16 text to preserve
the existing 10,000-code-unit validation rule. Trimming, the two-field
upsert, warning behavior, autosave timing, and dirty-state handling remain
covered. A page test exercises the default adapter through real persistence.
Executor and independent fresh Ninja/MSVC x64 runs validated 891 handwritten
source owners and built the ClassMngr executable, new boundary/page targets,
ClassMngrClassesPageTests, and DataServiceLifecycle. The five focused CTest
suites passed 5/5; source and diff checks passed. Phase 2 remains open. Next:
audit the current code against the Phase 2 plan and formal exit gate, then
continue with the remaining gaps. Workbook decoding, generic settings,
remaining feature services, broader document work, and the formal exit gate
remain open.

### Phase 2 Sub Prep calendar interval query - 2026-09-24

Sub Prep now requests only the current and following calendar years, using
January 1 of the current year through December 31 of the next year. The query
returns only normalized Vacation/Holiday types and complete event intervals;
it avoids the generic 4,096-event projection cap. The page uses one captured
date for query bounds and dialog defaults. Unavailable or failed reads still
open the dialog with an empty calendar. Executor and independent fresh Ninja/
MSVC x64 runs validated 894 handwritten source owners; all eight focused
repository, Application, Platform, Sub Prep page, and output tests passed.
The production ApplicationServices path and 4,097-event behavior are covered.
Phase 2 remains open. The requested current-state audit follows; its findings
and next step are recorded below.

### Phase 2 plan and exit-gate audit - 2026-09-24

Read-only review at committed F35 `616545ce` found app-less tests and target
dependency boundaries for implemented Domain/Application contracts, but the
Domain model and baseline-fixture parity are partial. WorkspaceCoordinator
create/open/close/save/save-as/export snapshot behavior passes its stated
contract tests; the production FileController still relies on MainWindow for
dirty-page approval and closes the old database before replacement succeeds.
The `src/next` source boundary has no direct DataService, MainWindow,
PageManager, or widget-pointer dependencies, while outer ApplicationServices
adapters bridge to legacy services. Phase 2 remains open. Next: select a slice
from the remaining gate gaps. Workbook decoding, generic settings, remaining
feature services, broader calendar and document work, and the formal exit
gate remain open.

### Phase 2 Calendar import planning parity - 2026-09-24

F36 adds a required checked-in workbook and loopback integration test around the
production CalendarEventImportService, exercising workbook parsing, the typed
signature query and planner, and batch save into a temporary database. The test
checks four parsed events, one parser skip, repeated-signature parser
deduplication, a pre-existing matching Red Day, the exact three-imported / two-
skipped result, order, and persisted event set. An independent fresh x64
Ninja/MSVC configure validated 895 handwritten source owners; all five focused
calendar parser, planner, signature-query, ApplicationServices port, and parity
suites passed. The test requires its fixture, serves it over loopback, and has
no skip path or external Google URL. Parser dedup occurs before planning, so
planner duplicate candidates are not covered end-to-end; the planner suite
continues to cover planner behavior separately. This adds production-path
Calendar import parity evidence, while Gate 2 remains partial and Phase 2 stays
open. Next: choose between Schedule Import matching/preview and conflict/state
projection for the next bounded parity slice. Workbook decoding, generic
settings, remaining feature services, broader calendar and document work, and
the formal exit gate remain open.

### Phase 2 Schedule Import apply-time state contract - 2026-09-24

F37 moves the live Schedule Import apply-time state checks into a typed,
standard-C++ Application contract. The repository converts the current database
snapshot and Qt teacher/class/day/time values at its boundary, then invokes the
contract after plan validation and snapshot reads, before the first write in the
existing transaction. The duplicate legacy state validator was removed. The
contract covers stale teacher/class targets, identity and room selection,
unique exact-match skips, invalid projected times, overlap/adjacency, and normal
and intensive schedule projection. A SQLite BEFORE UPDATE trigger sentinel
proves stale-state validation occurs before a proposed teacher write.
Independent fresh Ninja/MSVC x64 configure validated 895 handwritten source
owners; the Schedule Import regression and app-less contract suites passed 2/2.
Gate 2 remains open: Schedule Import matching/preview and checked-in fixture
parity, workbook decoding, and wider baseline-parity evidence remain. Phase 2
remains in progress. Next: choose the exact fixture-backed Schedule Import
review/preview boundary, then continue the remaining exit-gate gaps.

### Phase 2 Schedule Import matching and preview - 2026-09-24

F38 moves Schedule Import candidate matching and preview ranking into a
standard-C++ Application projection. `ScheduleImportRepository::preview`
adapts Qt/SQLite records into the contract and converts results back at the
repository edge. The old competing matcher was removed. A required checked-in
`schedule_review.xlsx` test calls the production preview path and asserts
candidate ordering, exact/confident suggestion, inventory, and initially
absent classes. Its seeded exact-match room includes surrounding whitespace,
verifying Qt normalization at the adapter boundary. App-less tests cover all
seven ranking categories, stable ties, no match, inventory, and Normal/
Intensive fallback.

Executor and independent fresh Windows x64 Ninja/MSVC builds validated 895
handwritten source owners. Both Schedule Import and Application projection
CTest suites passed 2/2 in both runs. The matching suite passed 5/5; the
Schedule Import suite passed 24 tests with only its existing optional external
workbook test skipped because `CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset.
The required checked-in fixture test passed. `git diff --check` passed, and a
source scan found no references to the removed matcher. Gate 2 has improved
fixture-backed preview evidence but remains incomplete; Gate 1 and Gate 3 are
partial, and Gate 4's audited `src/next` dependency boundary remains satisfied.
Phase 2 remains open.

### Phase 2 Schedule Import conflict projection - 2026-09-24

F39 adds one Qt-free Application overlap projection used by both Schedule
Import review presentation and apply-time validation. The UI retains translated
warning formatting. The checked-in `schedule_overlap_conflict.xlsx` fixture
drives production preview/review, displays the expected warning and disabled
import action, and proves apply rejection leaves teachers, classes, and class
times unwritten. App-less tests cover half-open overlap and adjacency, matching
days, deterministic order, Normal/Intensive schedules, skipped classes, and
retained intensive schedules.

Independent fresh Windows x64 Ninja/MSVC verification validated 896 handwritten
source owners. The three focused CTest targets passed 3/3; QtTest totals were
58 passed, 0 failed, and 1 existing optional external-workbook skip. The F37
pre-write trigger sentinel also passed. F39 is committed as
`3121d90c2db6af8e225048f016eec6f0843c1c18`.

The Phase 2 gate remains open: app-less Domain/Application behavior is partial;
fixture parity is partial despite the F36/F38/F39 fixture paths; workspace
replacement is partial because FileController still depends on MainWindow dirty
approval and closes the old session before replacement succeeds; audited
`src/next` dependency isolation is satisfied. Wider baseline parity, shared
workbook decoding, complete Domain models, generic settings, remaining
feature-service migrations, and broader calendar/UI and document work remain.
Preserve the Sub Prep bound of the current and following calendar years at most.
Preserve the pre-existing modified `cmake/sources.cmake`; it was not part of
F39. Nothing was pushed.

### Phase 2 Domain schedule-time value - 2026-09-24

F40 adds `Domain::Weekday` and a validated, Qt-free `Domain::ScheduleTime`
value with the existing same-day minute bounds and half-open overlap rule. The
Schedule Import validator converts untrusted time inputs once, keeps labels at
the Application edge for `InvalidProjectedTime`, and carries typed values into
the shared F39 conflict projection. Review and apply retain the same conflicts
and ordering.

Independent fresh Windows x64 Ninja/MSVC verification validated 897
handwritten source owners. Domain, Application state-validation, Schedule
Import repository, and review-dialog targets passed CTest 4/4: 67 passed, 0
failed, and one existing optional external-workbook skip. The F39 conflict
fixture review/disabled-import and zero-write rejection plus the F37 pre-write
trigger sentinel passed. F40 is committed as
`2ab23fb1796dfb1761a4c48644869a9ae6e1060d`.

Gate 1 advances but remains partial because broader Domain models are
incomplete. Gate 2 remains partial with F36/F38/F39 fixture parity and broader
baseline parity open; Gate 3 remains partial at FileController replacement;
the audited `src/next` dependency boundary remains satisfied. Phase 2 remains
open. Preserve the Sub Prep current-and-following-year limit and the unrelated
user modification to `cmake/sources.cmake`; neither was changed in F40.

### Phase 2 Schedule Import review decisions - 2026-09-24

F41 adds a Qt-free Application contract for teacher/class review choices and
uses it from both the dialog readiness path and legacy plan validator adapter.
The checked-in `schedule_review.xlsx` now follows production parse, preview,
explicit accepted choices, and repository apply, with assertions for summary,
persisted teachers/classes/colors/schedule times, and unrelated seeded state.
F39 conflict review/disabled-import/zero-write behavior and the F37 pre-write
trigger sentinel remain passing.

Independent fresh Windows x64 Ninja/MSVC verification validated 899 handwritten
source owners and passed the four focused CTest targets 4/4. QtTest totals were
85 passed, 0 failed, and one existing optional external-workbook skip. F41 is
committed as `30ec8d7512a8847a5b1d32addabf25f252b0eabb`.

Gates 1 and 2 advance but remain partial: broader Domain models and baseline
parity are incomplete. Gate 3 remains partial at FileController replacement;
the audited `src/next` dependency boundary remains satisfied. Phase 2 remains
open. Keep Sub Prep within the current and following calendar years at most;
the unrelated user change to `cmake/sources.cmake` was preserved.


### Phase 2 failure-atomic workspace replacement - 2026-09-24

F42 stages replacement databases and preserves the active workspace when a
replacement is invalid. It is committed as
`8b2eb8a2a7a0dae6a22ce8a4163b35d5d9dd24ee`. Independent fresh Windows x64
Ninja/MSVC verification reconfigured 899 handwritten sources with exactly one
owner each. Four focused CTest targets passed (17, 33, 25, and 11 QtTest cases;
86 total, no failures/errors/skips). `git diff --check` passed and no new
`src/next` dependency was introduced.

The initial verification exposed dangling `QSqlDatabase` references after
moving a candidate connection. F42 now keeps the database object at a stable
heap address and transfers its ownership with the repository adapters. Failed
candidate connections are cleaned up; successful and same-path replacements
retain readable settings and correct service references. The formal workspace
exit criterion is satisfied by the passing app-less coordinator create tests:
dirty replacement is rejected, success opens WorkspaceState and clears
SelectionState, and gateway/invalid-session failures preserve both snapshots.
F42 also fixes production profile replacement. Direct FileController snapshot
comparisons for invalid SQLite and same-path reopen remain integration coverage
gaps. Gates 1 and 2 remain partial for broader Domain models and baseline
parity; Gate 4 remains satisfied. Phase 2 and its exit gate remain open.
Preserve the Sub Prep bound of the current and following calendar years at
most and the user-owned `cmake/sources.cmake` modification.
