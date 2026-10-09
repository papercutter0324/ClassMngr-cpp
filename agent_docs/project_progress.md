# Project Progress

Active deployment plan: Qt Rewrite Phase 2 — Domain Model and Application Contracts.
Current deployment: `phase2_f417_resume_20261009`. Route: Heavy.
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

### Current state - 2026-10-09

Phase 2 remains In Progress/Open under deployment
`phase2_f417_resume_20261009`. Batches 11-18 are complete. Batch 19 resumed
after F416 at the user's request. F414 and F415 are committed, and F416's
Document Catalog MainWindow-to-viewer integration test is accepted and
committed. F417 Staff Directory rendered leaf through MainWindow is committed
as `d04d9bb0`; F418 Schedule Import through MainWindow apply and Sidebar
refresh is committed as `ea755736`. Its focused Executor and independent CTest
runs passed 1/1 each, and direct QtTest passed 7/7 including init/cleanup.
F418 verifies persisted teacher/class/time data, Sidebar refresh, visible
Schedule refresh, expected prompts, and stable page, selection, session, and
workspace path. No production change was needed. F419 MainWindow Print/Save Current Page As action capability and enabled state
is committed as e2ad222b. Its Executor and independent focused CTest runs
passed 1/1 each, and direct QtTest passed 3/3 including init/cleanup. The actual
MainWindow actions track a Ready PDF and return disabled after viewer release;
the workspace remains closed. No production change was needed. F420
Class/Schedule save signal to Sidebar action-state refresh is committed as
4c10f0d1. Executor and independent Tester focused CTest passed 1/1 each,
and direct QtTest passed 9/9 each. The first independent run caught an invalid
fixture teacher name; the repaired fixture passed both save paths. F421 Useful Links URL handoff is committed as 27c064e3; focused build passed,
independent CTest passed 1/1, and independent QtTest passed 4/4. F422 Testing Classes edits refreshing both Schedule views is committed as
08f44ac67cba677b78953da4e16435b0bf68c049; focused Executor and independent
Tester CTest passed 1/1 each and direct QtTest passed 10/10. Both views
refreshed after the real save and normal activation. F423 My Schedule display-mode handoff to Classes is committed as 34966439;
executor and independent focused CTest passed 1/1 each, direct target QtTest
passed 11/11, and independent targeted QtTest passed 3/3. The live mode
handoff updated the loaded Classes page before navigation. F424 Sidebar Add Class context-menu handler is committed as 3342963b;
executor and independent CTest passed 1/1 each, executor target QtTest
passed 4/4, and independent focused case passed 3/3. F425 Upcoming Birthdays QAction is committed as 37588279; executor and
independent CTest passed 1/1 each, executor target QtTest passed 5/5, and
independent selected-case QtTest passed 3/3. F426 Class Transfer import QAction is committed as
249d57b11ce327923a6416d72e53e034b53cda3c. Executor and independent
focused CTest passed 1/1 each; executor target QtTest passed 6/6 and
independent selected-case QtTest passed 3/3. F427 Schedule Save As/PDF output is committed as
bbdf10e85ba2c5a61066ed6fd32a7c8993c0f04e. Executor and independent focused
CTest passed 1/1 each; direct target QtTest passed 5/5 and the selected case
passed 3/3. F428 canceled database profile Save As is committed as
b82bddaa59c57d88f773370e94b3163df9055f33. Executor and independent exact
CTest passed 1/1; direct target QtTest passed 6/6 and the selected case passed
3/3. F429 Document Catalog viewer Save As is committed as
19f6024d113dda41e7e7a2b41acf27961160462b. F430 Schedule Print QAction cancellation is committed as aa7fca37. Executor
and independent focused CTest passed 1/1, direct target QtTest passed 7/7,
and the selected case passed 3/3. F431 Import Teachers QAction through
MainWindow, including the page-leave gate, is selected/current. Its two-outcome
acceptance matrix is recorded in the Phase 2 progress log before implementation.
The Cancel case preserves the dirty draft and blocks the import dialog; Discard
reaches the import dialog, which the test cancels before file selection or apply.
Executor and independent focused verification passed: CTest 1/1, target QtTest 6/6, and each new case 3/3. F431 is committed as 2ac08388. F432 Export Classes QAction through its selection dialog and JSON picker is selected/current. The QAction-to-picker-cancellation case is accepted: Executor and independent CTest passed 1/1, direct target QtTest 7/7, and the selected case 3/3. F432 is committed as 4bfe3dc2. F433 New Teacher menu QAction is selected/current; its characterization matrix is recorded before implementation. Executor and independent focused verification passed: CTest 1/1, target QtTest 8/8, and selected case 3/3. It characterizes the existing required-name warning without inserting a teacher or navigating; F285 remains deferred. F433 is committed as b60c8025. F434 Delete Teacher QAction confirmation through MainWindow is selected/current; read-only context discovery is underway. F435-F438 remain provisional.
F385 is retired as a duplicate of F369.
The seven user-reported MSVC build errors in
the Teacher Profile Edit persistence target are fixed and independently
verified in both the named target and all-target build, committed as
`0b128601`. Gates 1 and 2 remain Partial. The current slice and detailed
acceptance records are maintained in the [Phase 2 plan](../plans/qt-rewrite-heavy-route-plan/03-Phase-2-Domain-Model-and-Application-Contracts.md)
and [Phase 2 progress log](../plans/qt-rewrite-heavy-route-plan/03-Phase-2-Progress-Log.md).
### Earlier Phase 2 detail - 2026-09-26

F44 adds Qt-free Teacher Import review-decision validation shared by production
dialog readiness/plan creation and repository apply. The required checked-in
workbook test passes the exact plan returned by the dialog into repository
apply; invalid decisions leave imported records and source date unchanged.
F44 is committed as `28170a914a4dc76dc62f66677ad8f1067dfd42bf` and passed
independent fresh Windows x64 MSVC/Ninja Debug verification with Qt 6.12.0:
both focused CTest targets passed (2/2), and direct end-to-end and invalid-
decision cases each passed 3/0/0.

F45 adds a Qt-free Domain `Course` value/catalog with the existing 25 ordered
grade/level pairs. `ClassInfoConfig` adapts the catalog for UI lists, and
Schedule Import validation uses the same Domain rule. It is committed as
`eb2167e9d39a65446265b9506d749dc0e6be0d35`. Independent fresh Windows x64
MSVC/Ninja verification with Qt 6.12.0 passed both focused CTest targets
(2/2); valid fixture persistence and invalid-pair rejection passed. The
rejection test preserves seeded teacher, class, class-info, schedule-time,
and profile-setting snapshots.

F46 adds a Qt-free Domain `KoreanTeacherKey` for the existing Hangul-only
UTF-16 code-unit filter, used by Teacher and Schedule matching. It preserves
the five ranges, empty-key handling, and current errors without trimming or
normalization. F46 is committed as
`bedb52e0045731bba3e4f4b7a576021bdc007b72`. Independent fresh Windows x64
MSVC/Ninja verification with Qt 6.12.0 passed four focused CTest targets (4/4)
plus Teacher Import Dialog (1/1); CMake ownership validated 901 sources.
Fixture-backed Teacher and Schedule paths passed. Optional external-workbook
checks skipped as expected.

F47 moved weekly meeting-day policy into typed `Domain::Course` behavior and
routed Schedule Import partitioning and apply validation through that policy.
F48 adds Qt-free `Domain::ScheduleEntry` using typed `ClassId` and validated
`ScheduleTime` at the real Schedule Import persistence boundary after class IDs
resolve. It is committed as `2055bbb5f74842e4f146a48e211df58e65908b6b`.
Independent fresh Windows x64 MSVC/Ninja builds with Qt 6.12.0 passed both
focused CTest targets (2/2). `schedule_review.xlsx` persisted rows compared
against typed facts; the overlap fixture rejected apply with five seeded
database snapshots unchanged. Skip, intensive, and rollback cases passed.

F49 adds a Qt-free Calendar Import application use case that composes signature
lookup, duplicate planning, and batch saving. The feature service delegates
those operations while keeping workbook, network, campus, signal, and localized
error handling at the feature edge. It is committed as
`6a41e958671b7fa93c301d8b25c9c4381178fd7f`. Independent fresh Windows x64
MSVC/Ninja builds passed both focused CTest targets (2/2 each). App-less tests
cover ordering, duplicates, parser skips, empty inputs, exact UTF-16 identity,
and query/save failures; the required `calendar_import_parity_2026.xlsx`
production path passed with persisted facts and counts. The full suite was not
run.

F51 carries the Qt-free `Application::CalendarEventImportSignature` through
Calendar Import parser output, signature-query results, plan inputs, candidate
deduplication, and the use case. Qt normalization and ISO date formatting
remain at the adapter edges; exact UTF-16 identity and emitted order are
preserved. It is committed as
`e940f0c0ed8a63e740a3c2375631a22c08875f84`. Executor and independent Tester
configured fresh Windows x64 MSVC/Ninja builds and validated 906 handwritten
source owners. The seven focused CTest cases passed 7/7, including the
production `calendar_import_parity_2026.xlsx` fixture. After restoring detailed
`QCOMPARE` output in signature tests, `ClassMngrCalendarImportTests` was
rebuilt and rerun by both, passing 1/1. The full suite was not run.

F52 centralizes Gregorian calendar-event timing validation in the Qt-free
`Domain::CalendarEventTiming`, shared by event save, edit-draft, and
repeat-series edit contracts. It is committed as
`9cd9a2a4469482bc803cdc172d18a072c0fb3949`. Independent fresh MSVC/Ninja
verification validated 907 handwritten source owners, built the three focused
targets, and passed CTest 3/3, including the checked-in Calendar Import parity
fixture. Domain tests reject missing/replaced ISO date separators and cover
leap years, event timing policy, and the cross-day clock rule. The full suite
was not run.

F53 adds real Roster Score Import widget-path parity coverage through the
production `RosterEditorWidget::importScores` slot, autosave, and persisted
roster readback. The temporary-database fixture seeds saved evaluations through
production services; this workflow reads saved evaluations and does not parse a
workbook. It covers all four grade columns, name-pair matches and collisions,
unmatched/empty/English-only rows, autosave persistence, idempotent re-import,
and missing-name-column warnings. A mixed Winter score sums to 16/6 ~= 2.667
and imports as B+. F53 is committed as
`de763a0e64b3139a2c51b99bcdd610364861b920`. Fresh independent MSVC
19.51/Ninja/Qt 6.12 verification validated 908 handwritten source owners,
built the widget-import, roster-model, and speaking-evaluation targets, and
passed their CTests 3/3. No full suite was run.

F54 centralizes calendar event-type and time-status vocabulary recognition in
the Qt-free Domain contract and reuses it in the edit-draft, single-save, and
repeat-series validators. It preserves raw request fields, Application-edge
trimming, 64-character limits, validation order, and operation-specific
errors. F54 is committed as
`3739f2aaca23587c732dc77d6e77eddb16d92d88`. A fresh independent Ninja/MSVC
19.51/Qt 6.12 Debug configure and build passed the Domain and Application
calendar-event contract CTests 2/2. No full suite was run.

F55 exposes `Course::gradeBandForName` as a Qt-free grade-only classifier and
routes Classes tab visibility, Evaluation Default Selection, and Schedule
testing suppression through it while retaining their different policies. It
is committed as `87b7bfff66bf13cc5b79180cd1142101875be3cf`. A fresh independent
MSVC 19.51/Ninja/Qt 6.12 configure validated 908 handwritten source owners;
the four focused Domain, Classes, Schedule, and evaluation-default CTests
passed 4/4. No full suite was run. The evaluation test covers the exact
school-level helper used by `forClass`, without constructing the full
ApplicationServices path.

F56 centralizes the six-score overall grade as a Qt-free Domain rule, reused by
the repository roster importer, report assembler, and report widget. The
repository retains trimming, while the report inputs still require exact
labels. The widget-path cases verify padded grade acceptance, incomplete-score
`N/A`, the mixed 16/6 to B+ result, and persistence; report tests verify B+ and
N/A output. F56 is committed as
`c73e896fe34e186a045d73b653aa8ec9dfa89e83`. A fresh independent MSVC
19.51/Ninja/Qt 6.12 configure validated 909 handwritten source owners; four
focused Domain, roster-import, speaking-service, and report-widget CTests
passed 4/4. No full suite was run.

F57 adds the Qt-free `EvaluationPeriod` selector and wires it through
`EvaluationDefaultSelection::forClass`. It preserves the four-term
current/previous cycle and legacy evaluation labels; a temporary-database test
calls the real service path and checks grade-specific current/fallback results,
All, and missing required data. F57 is committed as
`b38b3afef0088b4c05d6d540dda15600f48c7f59`. The executor passed all five
focused CTests; fresh independent verification passed the four selection
targets, while an unchanged supporting calendar-preference target failed its
generated `.moc` compile with MSVC C1083. No full suite was run.

F58 moves Schedule Import matching identities to Qt-free `Domain::TeacherId`
and `Domain::ClassId`, represents an absent suggestion with
`std::optional<ClassId>`, and keeps the legacy integer preview conversion at
the repository boundary. It is committed as
`9b9183818fc2163d625a8ffb088a492a4aa631a9`. Executor focused CTest passed
1/1. Fresh independent MSVC 19.51/Ninja/Qt 6.12 verification validated
912 handwritten source owners, built the matching-contract and fixture-backed
Schedule Import targets, and passed 2/2 CTests, including the checked-in
`schedule_review.xlsx` path. No full suite was run. The adapter's absent
suggestion continues to use the legacy model's `-1` default, but has no direct
adapter assertion yet.

The formal gate remains open. Gate 1 and Gate 2 remain Partial because broader
Domain/application contracts and baseline parity are incomplete. F53 and F56
add Gate 2 evidence; F54, F55, F56, and F57 add Gate 1 evidence; F57 adds
Gate 2 production-path evidence; F58 adds Gate 1 typed-identity and Gate 2
Schedule Import parity evidence. The workspace criterion and audited v2
dependency isolation remain Satisfied. F57 closes F55's full `forClass`
integration gap, though broader parity remains. Three independent
Investigators selected Schedule Import matching identities for F58. Next,
compare remaining Gate 1 and Gate 2 gaps for another bounded slice. Sub Prep
remains capped at the current and following calendar years at most. The
user-owned `cmake/sources.cmake` change remains outside these commits.

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


### Phase 2 Class Transfer review decisions - 2026-09-24

F43 adds a Qt-free Class Transfer choice contract shared by dialog readiness and
repository apply validation. The repository rebuilds preview matches at apply
time; the contract preserves zero/unique/ambiguous teacher-match behavior and
rejects missing/duplicate decisions, invalid or foreign targets, and duplicate
replacement claims. A required success fixture now covers production
parse/preview/apply and persisted class, teacher, regular schedule including
end time, and roster data. The conflict fixture still rejects with no partial
writes.

F43 is committed as `c0e03e55aa5f5cc1897ccf97a25901a5e119c8e5`. Fresh Windows
x64 Ninja/MSVC verification validated 899 handwritten source owners and passed
both focused CTest targets (19 + 15 QtTest cases, 34 total, no failures or
skips). `git diff --check` passed; the Application contract has no Qt or
legacy dependencies. Gates 1 and 2 advance but remain Partial due to broader
Domain completeness and baseline parity. The formal workspace criterion and
audited dependency isolation remain Satisfied; Phase 2 and its exit gate remain
open. Preserve the Sub Prep maximum of the current and following calendar
years and the user-owned `cmake/sources.cmake` modification.

## Current Phase 2 position - 2026-09-26

F59 is committed as `7769912e1a8ccec02ecc3ace2de11ff98c719327` (`Phase2 -
Type Schedule Import state validation IDs`). Its Qt-free apply-state
validation contract now carries typed teacher and class identities through
resolutions, snapshots, links, and projected classes. The repository adapter
retains action-aware conversion of legacy nonpositive sentinels. Tests cover
typed-ID separation, missing and stale selections, skip validity and
uniqueness, mismatch rejection, numeric conflict ordering, and projected
overlap behavior.

Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 verification validated 912
handwritten source owners, built the state-validation and Schedule Import
targets, and passed `ClassMngrNextApplicationScheduleImportStateValidationTests`
and `ClassMngrScheduleImportTests` (2/2). Coverage includes the mismatched
teacher-key skip regression and `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase`.
No full suite was run. There is no exhaustive direct test matrix for every
action-specific sentinel conversion.

Gates 1 and 2 remain Partial. The workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep is bounded to the current and following calendar years. The user
change to `cmake/sources.cmake` was excluded; its SHA-256 remains
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: compare the remaining Phase 2 gaps for a bounded F60 slice and continue
the source-then-documentation commit sequence.

## Current Phase 2 position - 2026-09-26 (F60)

F60 is committed as `730955dd1feb24e5a46dd0bfef9f86b8ff619621` (`Phase2 -
Type Schedule Import review decision IDs`). The Qt-free review-decision
contract uses optional `Domain::ClassId` targets for class resolutions and
issues. The PlanValidator and review dialog translate legacy positive integer
targets at their feature boundaries; nonpositive sentinels become absence.
UpdateExisting still requires a target, CreateNew forbids one, and Skip keeps
an optional exact-match target. Duplicate-target issue details and claim order
remain covered.

Fresh independent x64 Ninja/MSVC 19.51/Qt 6.12 verification validated 912
handwritten source owners, built the three focused targets in 321 steps, and
passed `ClassMngrNextApplicationScheduleImportReviewDecisionsTests`,
`ClassMngrScheduleImportDialogTests`, and `ClassMngrScheduleImportTests`
(3/3). The Schedule Import fixture still applies `schedule_review.xlsx`; the
app-less contract separately passed 1/1 after adding explicit Skip-without-
target coverage. No full suite was run. The duplicate-target warning test also
asserts the resolved existing class label `E5 Athena`.

Gates 1 and 2 remain Partial; the workspace boundary and audited `src/next`
dependency isolation remain Satisfied. The Phase 2 exit gate remains Open.
F59's exhaustive per-action sentinel matrix remains a coverage gap. Sub Prep
remains bounded to 2026–2027. The protected user change to
`cmake/sources.cmake` remains excluded with SHA-256
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: compare the remaining gated gaps for F61 and keep source and
documentation commits separate.

## Current Phase 2 position - 2026-09-26 (F61)

F61 is committed as `5e08c2aab8c4c326463e969445757fa90e25d79c` (`Phase2 -
Cover evaluation default read failure`). The production integration test now
distinguishes a successful empty current evaluation, which selects Summer for
M2 on 2026-09-07, from an evaluation query failure after the table is removed,
which returns no default. It also confirms the repository read itself fails.

Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 verification validated 912
handwritten source owners and passed
`ClassMngrEvaluationDefaultSelectionIntegrationTests` (1/1). No full suite was
run. Gates 1 and 2 remain Partial; workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026–2027). F59's action/sentinel matrix remains a separate coverage gap. The
protected user change to
`cmake/sources.cmake` remains excluded, with SHA-256
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: select a bounded F63 slice and continue separate source and documentation
commits.

## Current Phase 2 position - 2026-09-26 (F62)

F62 is committed as `691e56fbdcc536aaaf577602feeac25fc5b7227f` (`Phase2 -
Characterize Schedule Import sentinels`). The repository apply tests now cover
the reachable action-specific teacher and class sentinel behavior, including
Reuse/UpdateRoom with teacher IDs -1/0, Create/Skip teacher targets, CreateNew
and Skip class targets, exact and mismatched Skip targets, and stale positive
UpdateExisting IDs. Rejected apply cases compare persisted database snapshots.
F60's PlanValidator rejects some class-target combinations before repository
state validation; those cases are recorded as upstream rejections.

Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 verification built
`ClassMngrNextApplicationScheduleImportStateValidationTests` and
`ClassMngrScheduleImportTests`; exact CTest passed 2/2, including the existing
`schedule_review.xlsx` apply fixture. No full suite was run. CreateNew class
sentinels cannot independently prove F59 adapter conversion because F60
normalizes nonpositive targets to absence before state validation; the cases
assert end-to-end apply behavior only.

Gates 1 and 2 remain Partial; the workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026-2027). The protected user change to `cmake/sources.cmake` remains
excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: compare remaining Gate 1 and Gate 2 gaps for F63.

## Current Phase 2 position - 2026-09-26 (F63)

F63 is committed as `bf4251eca530066ba65b00021f63779d185bd64e` (`Phase2 -
Cover Schedule Import missing suggestions`). The checked-workbook production
preview now asserts the first M3/Song's candidate has no matching class IDs,
legacy `suggestedClassId == -1`, `exactMatch == false`, and confidence
`None`. This exercises the repository adapter when the typed matching
projection has no suggestion; the existing app-less projection coverage
already verifies the optional absence.

Two fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds each
validated 912 handwritten source owners and built
`ClassMngrScheduleImportTests` plus
`ClassMngrNextApplicationScheduleImportMatchingProjectionTests`. The exact
CTest names passed 2/2 in both trees. The checked-in
`schedule_review.xlsx` apply fixture passed. No full suite was run.

F63 closes the directly observable F58 production no-suggestion sentinel gap.
Gate 1 and Gate 2 remain Partial; the workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026-2027). The protected user change to `cmake/sources.cmake` remains
excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: compare remaining Gate 1 and Gate 2 gaps for F64.

## Current Phase 2 position - 2026-09-26 (F64)

F64 is committed as `559b4feaa8fd67c01cd2f4d0f3ddd7dc0f166de5` (`Add typed
student name pairs to roster score import`). The Qt-free
`Domain::StudentNamePair` stores exact UTF-16 English and Korean components,
rejects an empty half, and provides equality and ordering. Roster score import
now uses the typed pair after trimming at the Qt boundary; empty-pair skipping
and last-write-wins behavior for duplicate saved scores are preserved. The
structured key removes delimiter ambiguity for invalid stored names containing
U+001F.

Two fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds passed
`ClassMngrNextDomainContractTests` and
`ClassMngrRosterEditorWidgetImportTests` (2/2 each). Coverage includes exact
pair identity, empty halves, ordering, UTF-16 values, one-sided trim through
the real import, persistence/idempotence, and a legacy stored duplicate pair
whose later grade wins. No full suite was run.

Gate 1 and Gate 2 remain Partial; the workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026-2027). The protected user change to `cmake/sources.cmake` remains
excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: select F65 from remaining Phase 2 gaps and keep source and documentation
commits separate.

## Current Phase 2 position - 2026-09-26 (F65)

F65 is committed as `a4fbffb91228ab1d783ac782ff572d49d3c28b65` (`Cover legacy
profile migration through FileController`). The lifecycle integration test
materializes `legacy_startup.sql` as a `.db`, opens it through FileController
and the workspace coordinator, and verifies the active path, teacher, class,
ClassInfo, and schedule data. Migration repairs the invalid teacher link to
SQL NULL while the service retains its `-1` unassigned sentinel; the active
schema reaches version 6 and the retained `.pre-schema-v4-backup` contains
schema version 3.

Fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds each
validated 913 handwritten source owners and passed
`ClassMngrFileControllerWorkspaceLifecycleTests` and
`ClassMngrDatabaseSchemaManagerTests` (2/2). No full suite was run.

Gate 2 gains production-path legacy `.db` migration evidence and remains
Partial; Gate 1 remains Partial. The workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026-2027). The protected user change to `cmake/sources.cmake` remains
excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: select F66 from the remaining Phase 2 gaps; keep source and documentation
commits separate.

## Current Phase 2 position - 2026-09-26 (F66)

F66 is committed as `9afa17f47aadb7188916cc091e370f5d0bea98bb` (`Extract Sub
Prep teacher display name rule`). The new Qt-free `Domain::TeacherDisplayName`
owns the selected UTF-16 value and applies preferred name, English name,
preferred romanization, then Korean name precedence. Both Sub Prep platform
adapters trim with Qt at their boundaries and keep their existing empty
fallbacks: summary `N/A`, class details empty. No interval logic changed.

Two fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds each validated
914 handwritten source owners, built the Domain contract and Sub Prep print
source-port test targets, and passed the exact two CTests (2/2). No full suite
was run.

Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026-2027). The protected user change to `cmake/sources.cmake` remains
excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: compare the remaining Phase 2 Gate 1 and Gate 2 gaps for F67; keep source
and documentation commits separate.

## Current Phase 2 position - 2026-09-26 (F68)

F68 is committed as `3ee0b1c6` (`Extract Qt-free calendar campus visibility
policy`). The campus-token matching rule now lives in the Qt-free Application
policy; the existing Qt feature adapter retains title trimming, campus-code
normalization, and one-to-one Unicode case folding. App-less, typed-summary,
and legacy-record coverage includes token boundaries, punctuation, `S2` versus
`S20`, Kelvin sign, and dotless i.

Executor and independent Tester used separate fresh Windows x64 Debug
Ninja/MSVC 19.51/Qt 6.12 builds, each validating 915 handwritten source owners
and passing the three focused Application Calendar Event, Calendar Event
Cache, and Calendar Import CTests (3/3). `git diff --check` passed; no full
suite was run. These are adapter regressions, not checked-in baseline fixture
parity evidence.

Gate 1 gains direct app-less behavior evidence and remains Partial. Gate 2
remains Partial; workspace boundary and audited `src/next` dependency
isolation remain Satisfied. Phase 2 and its exit gate remain Open. Sub Prep
remains bounded to the current and following calendar years (2026-2027). The
protected user change to `cmake/sources.cmake` remains excluded; its SHA-256
is `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: select F69 from three independent Investigator reviews and keep
source/documentation commits separate.

## Current Phase 2 position - 2026-09-26 (F69)

F69 source/test commit `95aaefa4` (`Extract Schedule Import state projection`)
adds a Qt-free Application projection for the final schedule rows. It uses
typed references to existing `ClassId` values or new candidate indices, with
explicit `ReplaceRows` and `KeepExistingRows` dispositions. Repository apply
computes the projection once, passes it to overlap validation, and resolves
new candidate IDs only after their class rows are inserted before persisting
from that same projection. Intensive `UpdateExisting` keeps untouched row IDs
intact; normal/replacement imports and skipped targets retain their existing
behavior.

Executor and independent Tester used separate fresh Windows x64 Debug
Ninja/MSVC 19.51.36257/Qt 6.12 trees. Each validated 916 handwritten source
owners and passed `ClassMngrNextApplicationScheduleImportStateValidationTests`
and `ClassMngrScheduleImportTests` (2/2). Coverage includes persisted
`schedule_review.xlsx` rows, `schedule_overlap_conflict.xlsx` rejection before
writes, intensive preservation/replacement, skipped exact match, and rollback
after failure. `git diff --check` passed; no full suite was run.

Gate 1 gains direct app-less schedule-state projection evidence and Gate 2
gains fixture-backed persisted-row parity; both remain Partial. Workspace
boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2
and its exit gate remain Open. Sub Prep remains capped at the current and
following calendar years (2026-2027). The protected user change to
`cmake/sources.cmake` remains excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
F69 plan and mapping documentation was committed separately as `4140b6b1`.
Next: finish F70 plan/mapping and deployment documentation in a separate
commit, then select F71 from three independent Investigator reviews.

## Current Phase 2 position - 2026-09-26 (F70)

F70 source/test commit `2f3d414c` (`Extract Class Transfer preview matching
policy`) moves candidate matching from the Qt repository implementation into
`src/next/application/class_transfer_matching_policy.h`. The adapter retains
`QString::simplified().toCaseFolded()` normalization and maps typed
`ClassId`/`TeacherId` matches to the legacy integer preview. App-less tests
cover both-name and single-name teacher rules, course and teacher identity,
assigned-but-unloaded teacher identity, the unassigned fallback, and stable
source/destination order. Production tests cover Qt whitespace and ASCII case
normalization plus the checked-in success/conflict preview IDs and conflict
no-write path. This does not claim exhaustive Unicode case-fold equivalence.

Executor and independent Tester used separate fresh Windows x64 Debug
Ninja/MSVC 19.51.36257/Qt 6.12 builds. Each validated 917 handwritten source
owners, built the two Class Transfer test targets, and passed exactly
`ClassMngrNextApplicationClassTransferTests` and `ClassMngrClassTransferTests`
(2/2). `git diff --check` passed; no full suite was run.

Gate 1 gains direct app-less matching-policy evidence. Gate 2 revalidates
fixture-backed preview and conflict no-write behavior through the moved rule;
both remain Partial. Workspace boundary and audited `src/next` dependency
isolation remain Satisfied. Phase 2 and its exit gate remain Open. Sub Prep
remains capped at the current and following calendar years (2026-2027). The
protected user change to `cmake/sources.cmake` remains excluded; its SHA-256
is `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
F70 plan/mapping and canonical deployment documentation are committed
separately as `91d01551`. F71 source/test commit `9b090cb5` types the
Class Transfer review-decision IDs and verifies a successful replacement from
the checked-in success fixture. The two focused Class Transfer CTests passed
in separate executor and independent fresh builds, each with 917 handwritten
source owners. Gate 1 and Gate 2 advance but remain Partial; the Phase 2 exit
gate remains Open. Next: finish F71 plan/mapping and deployment documentation
in a separate commit, then select F72 from three independent Investigator
reviews.

## Current Phase 2 position - 2026-09-26 (F71)

F71 source/test commit `9b090cb5` (`Type Class Transfer review decision
identities`) changes app-less match candidates and issue targets to typed
`ClassId`/`TeacherId` values and selected targets to optional typed IDs. The UI
and repository adapters retain legacy integer APIs, convert positive IDs,
translate exactly `-1` to absence, and reject other nonpositive IDs with the
existing action-specific messages and validation precedence.

App-less tests assert field categories, optional target values, typed issue
identities, and existing missing, invalid-action, match-set, and duplicate
rules. The checked-in `success_source.json` now exercises a successful Replace
with an exact class/teacher match: it retains the destination class ID and
teacher profile, replaces course details, schedule, and roster, and clears old
evaluation rows. Existing Create fixture behavior and conflict no-write
coverage remain. Adapter tests cover malformed targets in both UI and
repository paths.

Executor and independent Tester used separate fresh Windows x64 Debug
Ninja/MSVC 19.51.36257/Qt 6.12 builds. Each validated 917 handwritten source
owners and passed `ClassMngrNextApplicationClassTransferTests` and
`ClassMngrClassTransferTests` (2/2). `git diff --check` passed; no full suite
was run.

Gate 1 gains a typed app-less decision contract and Gate 2 gains
fixture-backed successful replacement parity; both remain Partial. Workspace
boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2
and its exit gate remain Open. Sub Prep remains capped at the current and
following calendar years (2026-2027). The protected user change to
`cmake/sources.cmake` remains excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
F71 plan/mapping and canonical deployment documentation are committed
separately as `5a9fe695`. F72 source/test commit `aa1af5fe` adds checked-in
fixture parity for successful `Teacher::ReplaceExisting`, including all
non-identity profile fields, retained teacher identity, and the imported
class's teacher link. Executor and independent fresh builds passed the two
focused Class Transfer CTests (2/2), each with 917 source owners. Gate 2 gains
fixture-backed teacher replacement evidence; Gate 1 receives no new evidence.
Both remain Partial and the exit gate remains Open. F73 source/test `60bbd015`
and documentation `5af94f32` are complete. F74 source/test `3e0a8d64` adds
synthetic Intensive workbook parse/apply coverage; the independent fresh build
passed three focused Schedule Import targets (3/3) with 917 source owners. The
synthetic path adds production-flow coverage but does not establish historical
baseline parity; Gate 2 remains Partial. F75 source/test commit 3139bdf4 adds a
Qt-free exact typed duplicate-pair grouping policy and adapters for shared,
roster, and speaking-evaluation validation. The fresh independent run passed
four focused targets plus the existing score-import regression target (5/5).
Gate 1 gains app-less contract evidence but remains Partial; Gate 2 remains
Partial without new historical parity. Select F76 for checked-fixture Class
Transfer evaluation persistence coverage.

## Current Phase 2 position - 2026-09-26 (F74)

F74 source/test commit 3e0a8d64 (Verify synthetic intensive schedule import flow) adds a source-readable synthetic Intensive worksheet fixture and a production parser → repository preview → explicit UpdateExisting → apply test. Literal assertions verify parsed candidate rows and preview match, persisted Intensive rows, retained target class identity with no class creation, unchanged IDs and values for an untouched Intensive row, and unchanged regular schedule rows.

Executor used build/phase2-f69-schedule-state-projection-executor-ninja-msvc-20260926 (Windows x64 Debug Ninja/MSVC 14.51.36231/Qt 6.12.0). Independent Tester used a fresh tree at C:\Users\wfelt\AppData\Local\Temp\f74_intensive_verify_20260926 (Windows x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12.0). CMake validated 917 handwritten source owners. ClassMngrScheduleImportTests, ClassMngrNextApplicationScheduleImportStateValidationTests, and ClassMngrNextApplicationScheduleImportMatchingProjectionTests built and passed (3/3). git diff --check passed; no full suite was run.

F74 is synthetic production-flow coverage only: the repository has no historical Intensive workbook paired with a legacy-output oracle. Gate 2 remains Partial; Gate 1 receives no new evidence. Workspace boundary and audited src/next dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open. Sub Prep stays capped at the current and following calendar years (2026-2027). The protected cmake/sources.cmake remains excluded at SHA-256 9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF. F75 source/test commit 3139bdf4 adds exact typed duplicate-pair grouping and current roster/speaking-evaluation adapters; F76 is selected for checked-fixture Class Transfer evaluation persistence coverage.

## Current Phase 2 position - 2026-09-26 (F75)

F75 source/test commit 3139bdf4 (`Extract typed duplicate student pair grouping`) adds the ordered `duplicateStudentNamePairGroups` policy in the existing Qt-free `StudentNamePair` header. Shared validation, `RosterModel`, and `SpeakingEvalModel` adapt trimmed complete pairs while preserving incomplete-row handling and caller-specific diagnostics. Score-import matching and last-write-wins behavior are unchanged.

Executor and independent fresh Tester builds validated 917 handwritten source owners. Windows x64 Debug Ninja builds passed `ClassMngrNextDomainContractTests`, `ClassMngrSharedPolicyTests`, `ClassMngrRosterModelTests`, and `ClassMngrSpeakingEvalBatchReportServiceTests` (4/4). Tester also built and passed `ClassMngrRosterEditorWidgetImportTests` (1/1). The independent tree used CMake 4.4.2, Ninja 1.13.2, Qt 6.12.0, and MSVC 19.51.36257. `git diff --check` passed; no full suite was run.

F75 adds Gate 1 app-less typed grouping behavior; Gate 1 remains Partial. F76 source/test commit 7a8b80c6 adds checked-fixture Class Transfer evaluation persistence; Gate 2 gains regression evidence but remains Partial without historical baseline provenance. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 and its exit gate remain Open. Sub Prep stays capped at 2026-2027. Protected `cmake/sources.cmake` remains unstaged at SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`. F77 is selected to move the weekly Class Transfer schedule-overlap decision into a Qt-free Application policy while preserving current adapter semantics.

## Current Phase 2 position - 2026-09-26 (F76)

F76 source/test commit 7a8b80c6 (`Verify Class Transfer evaluation fixture persistence`) adds a named 11-column Fixture Evaluation to the checked-in success fixture. Fixture-driven create/apply and replacement tests compare all 25 persisted rows against literal expected values; replacement still verifies the destination-only evaluation is cleared.

Executor and independent fresh Windows x64 Debug builds used CMake 4.4.2, Ninja 1.13.2, Qt 6.12.0, and MSVC 19.51.36257. CMake validated 917 handwritten source owners; `ClassMngrClassTransferTests` passed 1/1 independently. `git diff --check` passed; no full suite was run.

F76 adds checked-fixture regression evidence, not historical baseline parity. Gate 2 remains Partial; Gate 1 remains Partial and unchanged. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 and its exit gate remain Open. Sub Prep remains capped at current and following calendar years, 2026-2027. The protected `cmake/sources.cmake` remains unstaged at SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.

## Current Phase 2 position - 2026-09-26 (F77)

F77 source/test commit `dd3bbc01` (`Extract Class Transfer weekly schedule overlap policy`) moves weekly half-open interval overlap decisions into a Qt-free Application policy. The repository retains exact legacy weekday/time parsing, raw diagnostic strings, localized conflict rendering, and duplicate-message suppression. App-less tests cover category isolation and deterministic order, touching boundaries, Sunday overnight wrap, and 24-hour equal endpoints. Adapter tests assert parsed overnight/equal-endpoint conflicts, exact diagnostics, and no writes; the checked conflict fixture asserts the combined regular/intensive diagnostic and unchanged destination state.

Independent fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. CMake validated 917 handwritten source owners. `ClassMngrNextApplicationClassTransferTests` and `ClassMngrClassTransferTests` passed 2/2; `git diff --check` passed. No full suite was run. Configure warnings for Vulkan headers, `vswhere.exe`, and long object paths were nonfatal.

F77 adds app-less weekly conflict policy evidence to Gate 1 and checked-fixture regression evidence to Gate 2; Gate 1 and Gate 2 remain Partial. The checked fixture is not an independently sourced historical-output oracle. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress and its exit gate Open. Sub Prep remains capped at the current and following years, 2026-2027. Protected `cmake/sources.cmake` remains unstaged at SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.

F78 is selected to drive the permanent `schedule_large_conflict.xlsx` duplicate-class-target case through Schedule Import repository apply. The test will assign both imported classes to one existing destination, require pre-write rejection, and compare seeded database snapshots. This is required checked-fixture regression coverage; it does not claim independent historical-output parity.

F77 is selected: move Class Transfer's weekly schedule-overlap decision into a Qt-free Application policy, while keeping time conversion and localized conflict rendering at the repository adapter. Preserve half-open boundaries, end-at-or-before-start overnight behavior including Sunday-to-Monday wrap, regular/intensive separation, Skip and replacement filtering, deterministic conflict order, and duplicate-message suppression. Keep the checked-in conflict fixture as the production adapter/no-write guard. Sub Prep is not changed.

## Current Phase 2 position - 2026-09-26 (F78)

F78 source/test commit 68ab0faa (Verify Schedule Import duplicate fixture rejection) adds a repository apply regression from permanent schedule_large_conflict.xlsx. It derives two distinct Alice E4 Hercules candidates, assigns both to one seeded existing target, asserts the exact duplicate-target error before any write, and verifies the full persisted snapshot remains unchanged.

Executor and independent fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. CMake validated 917 handwritten source owners. ClassMngrScheduleImportTests, ClassMngrNextApplicationScheduleImportReviewDecisionsTests, and ClassMngrScheduleImportDialogTests passed (3/3); git diff --check passed. No full suite was run. The optional external workbook sample was skipped because CLASSMNGR_SCHEDULE_IMPORT_SAMPLE was unset.

F78 adds checked-fixture regression evidence, not historical-output parity. Gate 1 and Gate 2 remain Partial; workspace boundary and audited src/next isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep stays capped at the current and following years, 2026-2027. Protected cmake/sources.cmake remains excluded at SHA-256 9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF. F79 is selected to make Class Transfer weekly schedule intervals validated app-less values, preserving overnight/equal-endpoint and Sunday week-boundary behavior while retaining legacy parsing and diagnostics in the repository adapter.

## Current Phase 2 position - 2026-09-26 (F79)

F79 source/test commit 947edd93 (Validate Class Transfer schedule intervals) makes ClassTransferScheduleCandidate factory-created from a validated category, weekday index, and start/end minute-of-day. It preserves end-at-or-before-start next-day rollover, equal endpoints as 24 hours, Sunday endpoints beyond the weekly boundary, and the repository adapter's legacy parsing and diagnostic text.

Executor verification used a fresh Windows x64 Debug build with CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; configure validated 917 handwritten source owners, 313 build actions succeeded, and both ClassMngrNextApplicationClassTransferTests and ClassMngrClassTransferTests passed. An independent fresh-tree Tester overlaid the exact protected cmake/sources.cmake worktree version, validated the same 917 owners, built both targets, and passed both CTests (2/2). git diff --check passed; no full suite was run. Nonfatal warnings included a missing vswhere.exe locator and unavailable optional Vulkan headers.

F79 adds validated app-less interval behavior to Gate 1; Gate 1 remains Partial. Gate 2 remains Partial without new evidence. Workspace boundary and audited src/next dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep stays capped at the current and following years, 2026-2027. Protected cmake/sources.cmake remains excluded at SHA-256 9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF.

### Phase 2 Korean Teacher Import sparse-update contract - F80 verified

F80 source/test commit `15cd876d` adds a Qt-free Korean Teacher Import update policy. It carries `TeacherId` and `KoreanTeacherKey`, preserves existing Hangul-only matching and imported-name canonicalization, merges nonblank contact fields, retains blank fields, and reports no-op updates. Qt Unicode trimming and SQL identity binding remain in the repository adapter; unrelated profile columns remain outside the policy.

The checked-in `sectioned_review.xlsx` repository and dialog-plan paths now seed suffix-D Hong, update the same row, create Park, and verify persisted identity, imported and preserved values, manual fields, no duplicate, and source date `2026-09-01`. An all-blank repository import verifies unchanged counts and no UPDATE trigger.

Fresh independent Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; the build completed 315 Ninja actions. `ClassMngrNextApplicationKoreanTeacherImportUpdateTests`, `ClassMngrTeacherImportTests`, and `ClassMngrTeacherImportDialogTests` passed 3/3; the dialog target also passed with Windows and offscreen QPA. The repository test had 16 passes and one optional external-workbook skip. `git diff --check` passed; no full suite was run.

F80 advances Gate 1 with app-less merge behavior and Gate 2 with checked-fixture regression evidence, not historical-output parity; both remain Partial. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027. Protected `cmake/sources.cmake` remains excluded at SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.

F81 is selected to extract the Native English Teacher Import sparse-update rule into a Qt-free Application policy and exercise it through the checked-in `sectioned_review.xlsx` fixture path. Preserve current Qt matching, trimming, and name simplification at the adapter; retain native-table row identity in the repository. Cover nonblank replacements, blank-field preservation, unchanged detection, and persisted fixture results. This adds regression evidence, not historical-output parity.

## Current Phase 2 position - 2026-09-26 (F81-F82)

F81 source/test commit `118baceb` extracts Native English Teacher sparse updates into a Qt-free Application policy while keeping Qt normalization and the native table's integer identity at the repository edge. Independent fresh verification passed the app-less update-policy and `ClassMngrTeacherImportTests` CTests (2/2); it used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0, 921 handwritten owners, and 311 build actions. The optional external workbook was unset; no full suite ran. Gate 1 gains app-less behavior evidence and remains Partial; F81's checked-workbook regression is not historical parity.

F82 source/test commit `6d8fb296` adds literal regular Schedule Import expectations derived by running legacy `48fc5c5c` and current `118baceb` against the same seeded database and `schedule_review.xlsx`. All 14 captured semantic outputs match across parser metadata, teacher/class previews, apply counters, and normalized persisted rows. The independent normal `ClassMngrScheduleImportTests` target passed in a fresh archive (CMake 3.30.5, Ninja 1.12.1, MSVC 19.51.36257, Qt 6.12.0; 921 source owners; 309 build actions); `git diff --check` passed. No full suite ran. The workbook was added after the baseline (`f5fdcc4a`), so this is common-input differential evidence, not a historically present fixture. Gate 2 advances but remains Partial; Gate 1 remains Partial. Phase 2 remains open.

F83 is selected to compare the `schedule_overlap_conflict.xlsx` preview/apply rejection and pre-write database snapshot against legacy `48fc5c5c` using the same seed. The existing legacy rollback test and current `previewsAndRejectsCheckedInOverlapWorkbookBeforeWrites` path are the reference points. This fixture also postdates the baseline (`3121d90c`), so the slice will be documented as common-input differential coverage. Sub Prep remains capped at the current and following calendar years, 2026-2027.

## Current Phase 2 position - 2026-09-26 (F83)

F83 source/test commit `2e8bbab2` pins the Schedule Import overlap fixture's teacher keys/names and rooms, preview inventory and unmatched candidates, exact Monday rejection, and normalized no-write state across seven tables. Legacy `48fc5c5c` and current code matched on the same workbook bytes and deterministic seed. Independent fresh verification passed the normal `ClassMngrScheduleImportTests` CTest (1/1) using CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0, 921 source owners, and 309 build actions. The fixture postdates the baseline; this is common-input differential evidence, not historical workbook parity. Gate 2 remains Partial; Gate 1 remains Partial; Phase 2 exit remains Open.

F84 is selected to type Schedule Import's Qt-free candidate and teacher-projection matching keys as `Domain::KoreanTeacherKey`, keep display names separate, and convert only at the repository adapter. Preserve empty-key matching, match ordering, and room aggregation. Verify `ClassMngrNextApplicationScheduleImportMatchingProjectionTests` and `ClassMngrScheduleImportTests`. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-26 (F84)

F84 source commit `5207d65a` types both Schedule Import matching keys as `Domain::KoreanTeacherKey`; display names remain separate and QString conversion stays at the repository edge. The valid empty-key behavior and existing matching and aggregation results are preserved.

Executor and independent fresh Windows x64 Debug verification passed `ClassMngrNextApplicationScheduleImportMatchingProjectionTests` and `ClassMngrScheduleImportTests` (2/2). CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0 validated 921 handwritten owners and 313 build actions; `git diff --check` passed. No full suite ran.

F84 adds typed app-less behavior to Gate 1, which remains Partial. Gate 2 remains Partial without new parity evidence. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027. F85 is selected to extract Teacher Import plan validation into a Qt-free Application policy while retaining Qt normalization, date interpretation, translation, validation order, and exact diagnostics at the repository adapter.

## Current Phase 2 position - 2026-09-26 (F85)

F85 source/test commit `d5971ae1` extracts deterministic Teacher Import plan validation into a Qt-free Application policy. The repository resolves review choices first, projects selected Korean keys, and retains Qt normalization, date interpretation, UTF-8 conversion, and translated error mapping. The policy checks review-key count/order, source-date validity, then Korean, Native English, and GS Team identity rules in the existing order.

Executor and independent fresh-archive verification passed `ClassMngrNextApplicationTeacherImportPlanValidationTests` and `ClassMngrTeacherImportTests` (2/2). The independent Windows x64 Debug Ninja build used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; CMake validated 923 handwritten source owners and Ninja completed 311 actions. `git diff --check` passed. No full suite ran.

F85 adds app-less validation behavior to Gate 1, which remains Partial. Gate 2 remains Partial; no historical-parity claim was added. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

F86 is selected to compare a generated Schedule Import scenario whose helper exists at legacy baseline `48fc5c5c` against current code, using identical generated input bytes and a deterministic database seed. Pin parsed candidates, preview matches, apply counters, and normalized persisted schedule state. This is source-generated synthetic baseline evidence, not historical production-workbook parity.

## Current Phase 2 position - 2026-09-27 (F86)

F86 source/test commit `c3029f14` adds a baseline-era generated Schedule Import regression. Legacy `48fc5c5c` and current `31057c00` consumed the same 5,352-byte workbook (`24cf273f…d48b49`) and produced identical semantic transcripts (`8A00E7A0…F0181DA8`). The test pins parser metadata and candidate values, preview match/suggestion, apply counters, and normalized persisted state. It records Normal import's full-snapshot behavior: unrelated teacher/class metadata and settings remain, while the old regular schedule row is cleared.

Executor and independent fresh-base verification passed `ClassMngrScheduleImportTests` (1/1). The fresh CMake 4.4.2/Ninja 1.13.2/MSVC 19.51.36257/Qt 6.12.0 build validated 923 source owners and 309 build steps; `git diff --check` passed. No full suite ran. This is source-generated synthetic baseline evidence, not historical production-workbook parity.

F86 adds one baseline comparison to Gate 2, which remains Partial; Gate 1 remains Partial. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. F87 is selected to move GS Team sparse merge/no-op behavior into a Qt-free Application policy while preserving repository matching, normalization, diagnostics, and no-op SQL behavior. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F87)

F87 source/test commit `b97f81be` moves GS Team matched-record sparse merge and change detection into `src/next/application/gs_team_import_update.h`. The repository retains Korean-name/English-fallback matching, ambiguity rejection, Qt normalization, SQL, diagnostics, and transaction ownership. The policy preserves the matched ID and blank imported fields and reports whether an update is needed; unchanged rows skip UPDATE.

Executor and independent fresh-base verification passed `ClassMngrNextApplicationGsTeamImportUpdateTests` and `ClassMngrTeacherImportTests` (2/2). The independent CMake 4.4.2/Ninja 1.13.2/MSVC 19.51.36257/Qt 6.12.0 build validated 925 handwritten owners. `git diff --check` passed; no full suite ran. The repository trigger probe observed one UPDATE for the changed row and none for the unchanged match.

F87 adds GS Team sparse merge/no-op behavior to Gate 1, which remains Partial; Gate 2 remains Partial. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. F88 is selected to compare the intensive Schedule Import path using baseline-era generated `singleSheetWorkbookData()` and inline intensive worksheet data from `48fc5c5c`, with identical input bytes and seed on legacy/current. Pin parse, preview, apply, intensive slot/schedule state, and regular/unrelated data; label it source-generated synthetic baseline evidence, not historical production-workbook parity. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F88)

F88 source/test commit `4442726b` adds baseline-era intensive Schedule Import parity coverage to `tests/schedule_import_tests.cpp`. It combines the legacy `singleSheetWorkbookData()` helper with the inline intensive worksheet used by `convertsIntensiveTimesAcrossNoon()`. Legacy `48fc5c5c` and current code consumed identical deterministic workbook bytes (2,359 bytes; SHA-256 `228fc2ce924f2fd4ee340500c92178386868bc83231b076b74e081dd628b93b4`) and produced identical 65-slot state transcripts (SHA-256 `7fdba13a48788441556050e17b6d0b5ca52b2b9df5a6d0c357815a6458f4a5cc`): 62 empty, two essay, and one lunch states. The regression pins parsing, preview matching/suggestion, apply counters, intensive persisted slots/schedules, unchanged regular schedules, unrelated class/teacher rows, metadata, and settings. This is source-generated synthetic baseline evidence, not historical production-workbook parity.

Executor and independent fresh-base verification passed `ClassMngrScheduleImportTests` (1/1). The independent Windows x64 Debug build used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; it configured 925 handwritten source owners and passed CTest in 1.08 seconds. The test-only F88 change passed `git diff --check`; no full suite ran. The protected manifest was excluded from the fresh archive and remains unchanged at SHA-256 `bf3afbe30c77e30be83df98434eda3ee466084b0`.

Gate 2 gains another synthetic baseline comparison but remains Partial; Gate 1 remains Partial. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. F89 is selected to extract only Teacher Import match-count classification (zero, one, or multiple) into a Qt-free Application policy, use it for Korean, Native English, and GS Team imports, and test policy outcomes app-less plus repository ambiguity diagnostics/rejection. Matching, normalization, identity choice, localized errors, SQL, and transactions stay at the repository adapter. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F89)

F89 source/test commit `98968408` adds the Qt-free `TeacherImportMatchCardinality` classifier and uses it in the Korean, Native English, and GS Team repository import branches. The Application policy accepts only a count and classifies 0, 1, or 2+ matches; normalization, candidate order, identity/key selection, localized diagnostics, rejection, SQL, and transaction behavior remain in the repository.

The new app-less target covers counts 0, 1, 2, and 9. Repository tests pin exact ambiguity diagnostics and rollback/no-write behavior for Korean and Native English, plus the existing GS Team diagnostic and rollback behavior. Independent fresh Windows x64 Debug verification overlaid only the six F89 paths on `36b300d2`; the protected `cmake/sources.cmake` blob matched the base. CMake 4.4.2 validated 927 handwritten source owners; Ninja/MSVC 19.51.36257/Qt 6.12.0 built `ClassMngrNextApplicationTeacherImportMatchCardinalityTests` and `ClassMngrTeacherImportTests`, and focused CTest passed 2/2. Nonfatal warnings covered the missing `vswhere.exe` message, optional Vulkan headers, and unrelated long paths. `git diff --check` passed; no full suite ran.

## Current Phase 2 position - 2026-09-27 (F90)

F90 source/test commit `df8ef0cb` adds a Qt-free Teacher Import application use case. It owns review resolution, plan validation, match classification, Korean/Native English/GS Team match and update decisions, result counts, latest-source-date advancement, and transaction decisions. One persistence port keeps reads and all writes within the same transaction. Qt normalization, SQL, localized messages, and the transaction implementation remain in the repository adapter.

Executor focused CTest passed 2/2. Independent fresh-base verification overlaid only six F90 paths on `ca72c47e`, preserved the protected `cmake/sources.cmake` blob, validated 929 source owners, built the focused targets, and passed CTest 3/3 with Windows x64 Debug, CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. The repository tests include failure of the latest-date write after namespace writes and verify full rollback. `git diff --check` passed; no full suite ran.

F90 advances app-less application and orchestration evidence for Gate 1, which remains Partial. Gate 2 remains Partial without new parity evidence. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Two post-F90 audits confirm broader calendar UI, generic settings persistence, other feature-service migrations, and document-service migration remain open. F91 is selected for a bounded source-generated Teacher Import baseline differential against `48fc5c5c`; this adds synthetic plan evidence, not historical production-workbook parity. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F91)

F91 source/test commit `e814b4fc` strengthens `importsIntoSeparateTablesAndPreservesManualFields` with exact per-namespace result counts, all non-ID persisted fields for the Korean, Native English, and GS Team rows, Alex's preserved manual fields and updated position, and the latest-source-date monotonicity check. Its hand-built `TeacherImportPlan` is explicitly source-generated synthetic evidence, not workbook or historical-production-workbook parity.

Executor and independent fresh-tree verification passed `ClassMngrTeacherImportTests` (CTest 1/1). The independent current executable reported 20 passed, 0 failed, and 1 skipped optional external-sample case. An independently built narrow harness against legacy `48fc5c5c` and current F90 repository matched the same semantic output (SHA-256 `755A2B7DF52B3DD8F111ADF2EC0D0127415689B2B08A68D008505079D887B2B6`). Current test patch blob was `786770ca816b40f00c7626461ed58ec0a3e66c22` (SHA-256 `8BB8206A59D27E09E1C82BE918445670B32220F2CB4FD8011041646CD2BA0720`). Windows x64, MSVC 19.51, Ninja, and Qt 6.12.0; `git diff --check` passed. No full suite ran.

F91 adds one baseline-present synthetic plan/repository scenario to Gate 2; both Gate 1 and Gate 2 remain Partial. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open. Open scope includes broader Calendar contracts/UI, generic settings persistence, remaining feature-service migrations, document-service migration, invalid-UTF-8 coverage, and live MainWindow projection-failure/retranslation integration. F92 is selected to compare the baseline-present source-generated `testWorkbookData()` bytes through Teacher Import validation, explicit M1 review selection, plan construction, and repository apply on baseline/current implementations. This remains synthetic workbook evidence, not historical production-workbook parity. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F92)

F92 source/test commit `3f6ef73d` adds a baseline-present, source-generated Teacher Import workbook parse-to-apply comparison in `tests/teacher_import_tests.cpp`. The existing `testWorkbookData()` helper produced 3,422 bytes with SHA-256 `9cdccb43d7fe5e5e1abb83630ede8b18e6dd2c4824dbb288dc81d60371496daa`. Legacy `48fc5c5c` and current implementations matched the semantic transcript SHA-256 `095d595311aaee2444d67a893d45a2d0f97fe366c2a667f90bdd690205a2bdb4`. The scenario validates and parses the workbook, explicitly selects its sole Korean M1 candidate, creates/applies the plan, and checks import counts, seeded-row updates, preserved manual/unrelated fields, and latest-date state. This is synthetic workbook evidence, not historical production-workbook parity.

Independent fresh focused verification passed `ClassMngrTeacherImportTests` CTest 1/1. The executable reported 21 passed, 0 failed, and 1 skipped optional external-sample case; the selected F92 case passed all 3 assertions. `git diff --check` passed; no full suite ran.

Two independent post-F92 audits agree: Gate 1 and Gate 2 remain Partial; the documented workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. F93 is selected to extract Calendar start-of-term classification into a Qt-free Application policy used by the Calendar event summary and upcoming-event paths, preserving simplified/lowercased title matching, unknown-type fallback to `Other`, the hide option, and the exact four recognized phrases. Add app-less policy coverage and focused Calendar verification. Gate 2 rejection parity remains open; F94 is selected next to compare the baseline-present generated Teacher Import invalid-date workbook rejection and pin its result and diagnostic. Validation returns before repository use, so this path does not establish no-write or rollback parity. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F93)

F93 source/test commit `c73f1469` adds `CalendarEventStartOfTermPolicy` as a Qt-free Application contract, routes both `CalendarEventModel` and the Calendar upcoming-event filter through it, and removes the unused duplicate legacy Domain helper. The Calendar Import assertion now exercises the Application policy. App-less coverage pins the four exact aliases, title case/whitespace simplification, known event types and unknown-to-`Other` fallback, nonmatches, the hide switch, and U+0085/NEL behavior for both titles and event types.

Independent fresh Windows x64 Debug Ninja verification based on `c999a235` overlaid the seven F93 paths; the protected source manifest matched the base. CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0 built the two changed Calendar UI translation units, the focused targets, and `ClassMngr.exe` (358 build actions). The three focused Calendar CTests passed 3/3. A direct Qt 6.12 boundary probe matched the old behavior for NEL-separated `new semester` and `Vacation` with trailing NEL. `git diff --check` passed; no full suite ran.

Two independent post-F93 audits agree: Gate 1 and Gate 2 remain Partial; the written workspace criterion remains Satisfied, and audited `src/next` dependency isolation remains Satisfied. Phase 2 exit remains Open. F93 advances one bounded app-less Calendar behavior and does not close broader Calendar UI/contracts, generic settings persistence, remaining feature-service migrations, document-service migration, invalid-UTF-8 coverage, or live MainWindow projection-failure/retranslation integration. F94 is selected to compare the baseline-present source-generated `testWorkbookData("invalid-date")` input on legacy `48fc5c5c` and current code, pinning input bytes, `RecognizedButInvalid`, discovered M1 section, and the exact A1 date diagnostic. This adds negative validation parity only; do not claim repository no-write or rollback parity without exercising that path. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F94)

F94 source/test commit `f99d155f636d273269d805531f7ef7db7be84bed` adds `rejectsGeneratedWorkbookWithInvalidDate` to `tests/teacher_import_tests.cpp`. It pins the baseline-present generated workbook at 3,423 bytes, SHA-256 `256b29c2f27bfe787007aaa6df28e5e084dc3788863cbf4b6a09fb265f0685d0`, and asserts `RecognizedButInvalid`, template `sectioned-contact-list-v1`, invalid source date, discovered `M1`, and the exact A1 date diagnostic.

The saved narrow harness compiled each revision's own validator, registry, sectioned template, and workbook reader, using the same input bytes from legacy `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current production sources. Both executables returned identical semantic JSON (SHA-256 `8eee9375c47b3602b86e893092441f450c74860e2bb0f3edc53642a06322cbd9`); I independently reran both archived executables and confirmed equality. The focused current `ClassMngrTeacherImportTests` QTest case passed (3 passed, 0 failed including setup/cleanup); filtered CTest passed 1/1. `git diff --check` passed; no full suite ran. This negative validation path does not establish repository no-write or rollback parity.

F94 adds one negative validation case to Gate 2, which remains Partial; Gate 1 remains Partial. The formal workspace criterion and audited `src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep remains capped at 2026-2027.

F95 source/test commit `7865963815efa256b797b85af27b9a33107e8f70` moves Calendar repeat occurrence planning into a Qt-free Application policy and routes the UI through the existing series-create port. Independent verification built `ClassMngrNextApplicationCalendarEventTests` and `ClassMngr`; focused CTest passed 1/1. Coverage pins daily/weekly cadence, inclusive cutoff, chained monthly clamping, date bounds, duration and copied fields, cleared IDs, invalid input, and 366/367 occurrence bounds. Gate 1 advances but remains Partial; Gate 2 remains Partial. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

F96 adds baseline/current synthetic Teacher Import database rollback parity for commits `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and `7865963815efa256b797b85af27b9a33107e8f70`. Independent fresh builds used each revision's own repository and schema sources; both runs failed at `latest_date_write` and left zero rows in `teachers`, `native_english_teachers`, `gs_team`, and `teacher_import/latest_source_date`. Normalized outputs matched at SHA-256 `b53d70bf33fa0e20451a2582eb8e1a6f904e5907196c6fad4696543b95b230f3`. This adds one synthetic rollback case to Gate 2, which remains Partial; it is not workbook or historical production-data parity. Gate 1 remains Partial; Sub Prep remains capped at 2026-2027.

F97 commit `36ebb09a960fa82f633701ec83fba35bcd7f3599` moves Calendar repeat-series suffix-edit date and field propagation into a Qt-free Application planner and routes the platform adapter through it. App-less tests cover ordered occurrences and IDs, date offset/duration, request fields, trimmed series ID, empty input, invalid dates, and range overflow. Independent verification built `ClassMngrNextApplicationCalendarEventTests`, `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and `ClassMngr`; both focused CTests passed, including a platform-boundary recheck after adding empty-query and no-save failure cases. Gate 1 advances but remains Partial; Gate 2 remains Partial. Phase 2 exit remains Open; Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-28 (F107 verified; F108 selected)

F107 commit `dbdcd721bd68bcaa30191bc93e7576de590fb650` adds a Qt-free Calendar event-by-ID query contract and routes `CalendarPage::handleCalendarEventActivated` through its use case. Application validates bounded IDs and propagates typed results; Platform retains legacy positive-integer parsing, service lookup, and error mapping. The UI still returns before opening the dialog when lookup fails.

Independent fresh Ninja/MSVC VS 2026 x64 / Qt 6.12 Debug verification built `ClassMngrNextApplicationCalendarEventTests`, `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and `ClassMngr`; the two focused CTests passed 2/2. Application tests cover exact forwarding, success/failure results, invalid inputs with no port calls, and the exact length boundary. The application test target uses `QTEST_APPLESS_MAIN`. Platform tests cover legacy lookup and invalid/unavailable IDs. The UI failure path was confirmed by source inspection; these focused tests do not directly assert dialog state. CMake reported potential object-path-length warnings for unrelated targets, but all requested targets built. No full suite ran.

F107 advances Gate 1, which remains Partial; Gate 2 remains Partial with no new parity claim. Workspace create and audited direct `src/next` isolation remain Satisfied; strict transitive ApplicationServices-to-DataService read isolation remains unresolved. F108 is selected to strengthen repeat-series suffix-delete repository state coverage and compare the same seeded fixture against baseline and current source closures. This is repository state-transition parity only. Sub Prep remains bounded to January 1 of the reference date's year through December 31 of the following year, at most. Phase 2 remains In Progress with its exit gate Open.

## Current Position - 2026-09-28 (F109 verified; F110 in progress)

F108 suffix-delete parity passed in commit `d99b226e`. The current focused repository CTest passed 1/1, and the baseline/current harness outputs were byte-identical at SHA-256 `FEC97B681F9D52E7C623AA14E245F3D4BB70FB6315425148632C0E905CC76E08`. The comparison used each revision's own source closure and confirmed five seeded rows became three, retained rows kept all fields, and `sqlite_sequence` stayed at 5. This is synthetic repository state-transition parity only.

F109 commit `26d604fb` adds a seeded repeat-series batch-creation fixture. Independent fresh-archive verification passed the focused `ClassMngrCalendarEventRepositoryTests` CTest 1/1. A separate harness compiled each revision's own repository/schema/source closure with identical seed and explicit occurrence facts. Baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current production revision `d99b226e19917f3c855a0c49fa7891c537c4415d` returned IDs `[2,3,4]`; all persisted columns, the unrelated row, count, and `sqlite_sequence` matched. Normalized output SHA-256: `DB5EC63C24300360F3639131C501D9AC65A9867942D5A14CCCBA3BAAD7420E59`. This is synthetic repository batch parity, with no UI or historical-workbook claim.

F110 is in progress: move the Calendar per-event visibility composition (start-of-term hiding followed by campus visibility) into a Qt-free Application policy used by both the Calendar event model and upcoming-event filtering. Qt normalization and preference/directory reads stay at the feature boundary; active-type filtering remains ahead of this decision. Tests must preserve hide/show-all precedence, current/non-current/missing-campus behavior, unmatched titles, and input order.

Gate 1 and Gate 2 remain Partial; the formal workspace-create boundary and audited direct `src/next` isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` read isolation remains open. Phase 2 exit remains Open. Sub Prep remains bounded to the reference date's year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F110 verified; F111 selected)

F110 commit `2656ef9c` extracts Calendar's per-event visibility composition into a Qt-free Application policy used by the event model and upcoming-event filter. Independent fresh-archive verification built the Application Calendar tests, Calendar cache tests, and `ClassMngr`; the two focused CTests passed 2/2. Twelve app-less cases cover start-of-term precedence, `Other` and unknown event types, lazy campus evaluation, show-all, missing metadata, unmatched titles, current/non-current and mixed campus tokens. The new header has no Qt dependencies. Both callers preserve input order, the upcoming type filter stays first, and the Qt normalization adapter remains. The CalendarPage path compiled but has no direct runtime visibility assertion; no full suite ran.

F111 is selected: make non-null-session `SettingsService` operations authoritative for load/save/saveAll, while preserving DataService-only legacy construction. Verify that a closed bound session does not fall through to a separately open DataService, that the legacy-only constructor still reads/writes, and that normal ApplicationServices preferences still work. This removes the settings fallback from session-bound callers only; Calendar, Teacher, Class, Schedule, Roster, and other service-family fallbacks remain under audit.

Gate 1 and Gate 2 remain Partial; the workspace-create boundary and audited direct `src/next` isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` read isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to the reference date's year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F111 verified; F112 under mapping)

F111 commit `3152ce36` makes a bound-session `SettingsService` authoritative for load/save/saveAll and preserves the sessionless DataService-only path. Fresh independent Windows x64 Debug verification overlaid only `feature_services.cpp` and `data_service_lifecycle_tests.cpp` on base `5653bf8c`; `ClassMngrDataServiceLifecycleTests` and `ClassMngrNextPlatformApplicationServicesCurrentCampusPreferencesPortTests` passed 2/2. The test used a closed bound session and separate open DataService, verified no fallback read/write or mutation, and verified legacy-only access still works. This reduces the settings-service transitive read edge only; other service families remain.

The separate open-DataService test observes `SettingsService::isAvailable()` as true while bound-session operations fail. This reflects the existing availability check, which still considers DataService open state; it is recorded as an availability limitation, not a content-read path. No full suite or baseline parity run.

F112 is under mapping: isolate CalendarService content reads used by Next Platform adapters (by-ID, range, date intervals, repeat series), retaining legacy DataService-only behavior. Scope and focused acceptance are pending the current call-site/test-seam review.

Gate 1 and Gate 2 remain Partial; workspace create and audited direct `src/next` isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to the reference date's year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F112 verified; F113 selected)

F112 commit `677f2451` makes all six CalendarService content reads session-bound when a session is present: events-for-date, range, date intervals, upcoming events, by-ID, and repeat-series. Independent fresh-archive Windows x64 Debug verification passed the lifecycle and Next Platform Calendar adapter CTests 2/2 using CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, and Qt 6.12.0. The closed-session/separate-open-DataService test proves all six reads fail closed while legacy-only construction still reads. The adapter target first hit an MSVC generated-file error on a longer temporary path; the short-path fresh retry passed. No full suite ran, and this slice does not establish global DataService isolation. Calendar writes/deletes remain outside scope.

F113 is selected to remove bound-session DataService fallbacks from the five TeacherService, ClassService, and RosterService reads used by Sub Prep print and roster-output source ports. Preserve sessionless legacy reads, the schedule-scope class-info path that is already session-only, and the print adapter's zero student-count fallback. Focused acceptance covers the closed-session/other-open-DataService contrast and both normal Sub Prep output-port tests.

Gate 1 and Gate 2 remain Partial; workspace create and audited direct `src/next` dependency isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` read isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to January 1 of the reference year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F113 verified; F114 selected)

F113 commit `e9ef19a0` makes five Sub Prep output reads session-authoritative: TeacherService teacher lookup, ClassService classroom and class-info reads, RosterService student count, and bounded roster output. A non-null closed session now fails these reads without falling through to a separately open DataService; sessionless DataService-only construction remains functional. The lifecycle regression also confirms the dual-bound services still report available, proving the operations themselves fail closed.

Independent fresh-archive verification used base `677f24518c41ade1fa1b7769375e9b8561f1ed25` and overlaid only `feature_services.cpp` and `data_service_lifecycle_tests.cpp`. The three focused CTests passed 3/3: `ClassMngrDataServiceLifecycleTests`, `ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and `ClassMngrNextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests`, on Windows x64 Debug with Ninja, MSVC 19.51, and Qt 6.12.0. The existing zero student-count print behavior remains covered. `git diff --check` passed. CMake emitted a non-fatal `vswhere.exe` warning; the isolated build and tests succeeded. The verifier removed its scratch files. No full suite ran.

The initial F114 candidate was to isolate TeacherService catalog reads, but the dependency audit found no current `src/next` caller for those methods. F114 was redirected to active Calendar mutation paths and bound-session availability; the completed slice is recorded below. Other Class, Schedule, Roster, Speaking Evaluation, document, and broader Calendar UI/contract work remains open.

Gate 1 and Gate 2 remain Partial; workspace create and audited direct `src/next` dependency isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to January 1 of the reference year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F115 verified; exit-gate audit next)

F115 source/test commit `96c8b8a5` makes `ClassService::saveClassNotes()` authoritative to a non-null session. Direct service coverage proves a closed bound session cannot save through a separately open, seeded DataService and leaves both notes fields unchanged; a DataService-only service still updates both fields. F114's session-authoritative availability guard also makes the active ClassNotes adapter fail closed before calling the service.

Independent fresh-snapshot verification overlaid only `feature_services.cpp` and `data_service_lifecycle_tests.cpp` on `0c3fdd61`. Windows x64 Debug, CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, and Qt 6.12.0 built and passed the registered lifecycle, ClassNotes save-port, and ClassNotes page CTests 3/3. `git diff --check` passed. The broader suite was not run; the initial preset configure was incompatible with Ninja's platform setting, and the direct Ninja configure succeeded.

Next, independently re-audit the active `src/next` -> `ApplicationServices` service paths against the literal dependency gate, keeping method-level runtime isolation distinct from the retained legacy `DataService*` compatibility edge. Refresh the Gate 1 and Gate 2 evidence map at the same time. Existing bounded Gate 2 comparisons include F82/F83 Schedule Import fixtures, F86/F88 source-generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import cases, and F100/F101/F103/F104/F106/F108/F109 rollback and repository transitions. They do not cover all validation, conflict, planning, and state-transition behavior; historical production-workbook provenance remains a tracked non-gating risk.

Gate 1 and Gate 2 remain Partial; workspace create and audited direct `src/next` dependency isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to January 1 of the reference year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F114 verified; F115 selected)

F114 source/test commit `8aee10a6` makes a bound `DatabaseSession` authoritative for `FeatureService::isAvailable()` and for CalendarService `saveEvents`, `deleteEvent`, `deleteRepeatSeriesFromDate`, and `deleteAllEvents`. A closed bound session rejects each operation without changing the separately open seeded DataService; lifecycle assertions check the legacy state immediately after each rejected call. Sessionless DataService-only behavior remains covered. Singular `saveEvent()` is unchanged because no current `src/next` caller was found.

Independent fresh-snapshot verification overlaid only `feature_services.cpp` and `data_service_lifecycle_tests.cpp` on `2fe91485`. With Windows x64 Debug, Ninja 1.13.2, MSVC 19.51, and Qt 6.12.0, `ClassMngrDataServiceLifecycleTests` and `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests` passed 2/2; `git diff --check` passed. The same code snapshot also passed `ClassMngrSharedPolicyTests` 1/1 before the test-only assertion refinement. No full suite ran; optional Vulkan headers were unavailable and did not affect verification.

F115 is selected to make `ClassService::saveClassNotes()` itself session-authoritative when a non-null session is bound. Direct service coverage will prove a closed session plus a separately open seeded DataService fails without changing either notes field, and a DataService-only service still saves. F114's session-authoritative availability already makes the ClassNotes adapter reject a closed bound session before calling the service; the F115 slice closes the service method's remaining latent fallback and retains the port/page regressions.

Gate 1 and Gate 2 remain Partial; workspace create and audited direct `src/next` dependency isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to January 1 of the reference year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F116 verified; F117 selected)

F116 source commit `6636cac1` removes `ApplicationServices::hasOpenDatabase()` from the active Sub Prep roster-output source port. Its existing null and session-authoritative availability checks remain. Independent fresh-snapshot verification overlaid only `src/next/platform/application_services_sub_prep_roster_output_source_port.h` (SHA-256 `C92559472DA7F1E6758BD74341CD4EBF3439E105A4A91361EAD276A9CAC39ABD`) on `85fa1e2e`. Windows x64 Debug with CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0 built and passed `ClassMngrDataServiceLifecycleTests` and `ClassMngrNextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests` 2/2 with `--no-tests=error`; `git diff --check` passed. The lifecycle test verifies that closed-session-bound services reject reads while a separate legacy DataService is open and that legacy-only services still work. The port target has no direct split-session injection seam; no full suite ran. The verifier retried from a short temporary path after an MSVC generated-`.moc` path-length failure in its first isolated build.

Two independent Gate 1 audits found broad existing app-less coverage across typed values, workspace, Calendar, Schedule/import, Class Transfer, document, job, and Sub Prep contracts, but the planned application surface remains incomplete. Teacher-profile editing is still composed and validated in `TeacherInfoPage`; F117 is selected to move that operation behind a deterministic app-less Application contract while preserving UI feedback and dirty-state behavior. Other planned editing, backup/recovery, and legacy-import use cases remain gaps. Gate 1 stays Partial.

The isolation re-audit confirmed the audited direct `src/next` source scan is clean, while v2 service factories still construct dual-bound feature services and the Workspace adapter still calls DataService-backed lifecycle operations. F116 removes only the extra roster-output open-state call. Workspace-create acceptance remains Satisfied; strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved. Gate 2 remains Partial; its current evidence map includes F82/F83, F86/F88, F91/F92/F94/F96/F98, and F100/F101/F103/F104/F106/F108/F109, but does not cover all validation, conflict, planning, and state-transition behavior. Historical production-workbook provenance is tracked as non-gating risk.

Phase 2 exit remains Open. Continue F117 after a bounded design review, and keep the independent session ownership, Workspace, and session-only v2 service work visible as separate gate blockers. Sub Prep remains bounded to January 1 of the reference date's year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F117 verified; F118 selected)

F117 source/test commit `3fd2b93f` adds an app-less Teacher Profile Edit domain/application contract: checked teacher identity and 14 profile fields, injected semantic validation returning normalized fields and structured field-addressable issues, and separate update/reload persistence operations. It short-circuits invalid IDs, blocks only error-severity issues, passes normalized values to persistence, distinguishes update from post-write reload failure, and returns the canonical reloaded profile.

Independent fresh-archive verification used base `47844dfc087d47da9426e0aa06d948a1ab2264a9` with exactly the five committed paths and hash-checked overlays. A forced clean MSVC/Ninja build passed; `ClassMngrNextApplicationTeacherProfileEditTests` passed 1/1 with `--no-tests=error`. Static include/link inspection found no Qt or legacy production dependency in the new contracts. `git diff --check` passed. MSVC warning C4530 remains non-fatal because `/EHsc` is not set on this test target. The production policy adapter and `TeacherInfoPage` integration are future work; the adapter must use existing `TeacherValidator` semantics.

F118 is selected for common-input baseline/current comparison of the Class Transfer conflict scenario using `tests/fixtures/transfers/conflict_source.json`. This fixture was added after the legacy baseline, so evidence must be described as a checked-in post-baseline common-input comparison, not historical production-workbook parity. Gate 2 remains Partial pending this and broader coverage. Gate 1 remains Partial; F117 adds app-less coverage but does not complete the planned application surface.

Workspace create and audited direct `src/next` source isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` read isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to January 1 of the reference date's year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F119 verified; F120 selected)

F118 source/test commit `b68eba6d` adds a Class Transfer conflict case over the unchanged `conflict_source.json` fixture. It pins the fixture bytes, preview match results, review choices and plan, the exact combined regular/intensive schedule-collision diagnostic, all application-table snapshots plus `sqlite_sequence`, and a zero `total_changes()` delta under SQLite query-only.

Independent verification extracted current commit `5263aebf8222e16e3085498af851a7f0d3041818` and overlaid only the F118 test file. A separate baseline archive at `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` received the same fixture bytes and only the F118 test helpers/case. Focused `ClassMngrClassTransferTests` passed 1/1 on both source closures; `git diff --check` passed. The baseline's temporary CMake files needed three Qt minimum references adjusted from 6.11.1 to the installed 6.12.0. The fixture was added after baseline, so this is common-input evidence on a checked-in post-baseline fixture, not historical production-workbook parity. No full suite ran.

F119 source/test commit `80fbf034` implements canonical `DatabaseSession` ownership in `ApplicationServices`, a borrowing `DataService` compatibility facade with the standalone owning constructor preserved, and session-only construction in all seven ApplicationServices feature-service factories. Independent fresh-archive verification confirmed the exact five committed paths and corrected SHA-256 manifest, built `ClassMngr` plus six focused targets on Windows x64 Debug with CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257.0, and Qt 6.12.0, and passed all six selected CTests. `git diff --check` passed. No full suite ran. The explicit Workspace port lifecycle/save/save-as/export edge remains a separate follow-on.

F120 is selected as one gate-closing slice after three independent solution reviews. Keep the existing `ApplicationServices` workspace API and route its seven operation implementations to `DatabaseSession` and a shared DataService-independent file-copy helper. Replace `DataService`'s stale repository-pointer cache with live session-backed access. Preserve the Workspace API and controller composition. Acceptance covers a source audit showing no `m_dataService` operation call, all seven Workspace behaviors, and facade validity after first open, successful database replacement, failed replacement, close, and reopen. Focused tests target the lifecycle, Workspace port, and FileController lifecycle suites with a `ClassMngr` build.

Gate 1 and Gate 2 remain Partial; F118 adds one post-baseline common-input conflict comparison to Gate 2. Workspace create and audited direct `src/next` source isolation remain Satisfied. F119 removes the dual-bound feature-service factory edge, but strict transitive isolation remains unresolved at the Workspace path. Phase 2 exit remains Open.

## Current Position - 2026-09-28 (F122 verified; F123 candidate pending review)

F122 source commit `e3411e733d2ce402a03096e715150e45b4244dff` adds ordered
multi-meeting and final-state parity assertions to
`ScheduleImportTests::skippedExactMatchPreservesItsSchedule`. An independent
fresh-archive Tester verified the exact commit and pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`; both focused cases passed 3/3.
Both produced final snapshot SHA-256
`08ad64ed3d853e52a1a686c1683d0d1fe8a289026e21d21081e09ee4840c5ebe`, including
`class_times.id` and `sqlite_sequence`, and retained Tuesday-before-Monday
consumer order. Current used Windows x64, MSVC 19.51.36257.0, Ninja 1.13.2,
and Qt 6.12.0; the baseline used Qt 6.11.1. No full suite ran. This is
baseline-present hand-authored seed evidence, not historical workbook parity.

F123's candidate is to integrate the existing-teacher edit use case from F117
with the production `TeacherInfoPage` flow, preserving `TeacherValidator`
semantics, the active session-backed update/reload path, validation feedback,
and save lifecycle. Two independent Explorers agreed on this gap, but the
bounded solution review is pending; no F123 implementation has begun.

Gate 1 and Gate 2 remain Partial. The formal workspace-create boundary and
active-v2 DataService isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. The user's requested session handoff records this
continuation point in `latest_session_work.md`.

## Current Position - 2026-09-29 (F124 verified; F125 review next)

F124 source/test commit `a159591e48e312f96378302b105a3ad1276382b3` integrates
`ClassCoTeacherPage` with the app-less `ClassCoTeacherAssignmentUseCase`. The
use case rejects invalid IDs and maps the legacy `-1` sentinel to an absent
teacher ID. Its session-backed platform adapter reads the current `ClassInfo`,
changes only the teacher assignment, and saves through `ClassService`, keeping
the other class fields, notes, and schedules. The page retains manual warning,
dirty-state, title-refresh, and `classInfoSaved` behavior.

The `windows-x64-debug` preset built `ClassMngr`, the application test, the
platform adapter test, and the page integration test; their focused CTests
passed 3/3. Coverage includes invalid IDs, assigned/unassigned values,
persisted-field and schedule preservation, unavailable service handling, page
title/signal updates, and warning/dirty-state behavior. `git diff --check`
passed. The existing `ClassMngrClassesPageTests` target built but failed 22
test cases, including `page.openClass(42, ...)` and widget validation assertions;
the first Details failure occurs before the co-teacher editor opens. It also
emitted duplicate-stub `/FORCE` linker warnings and a stale dependency warning
for `class_navigation_preferences.h`. No baseline comparison or full suite ran.

F125's candidate is routing `ClassNotesPage` through an app-less
`ClassNotesSaveUseCase`, applying the request's existing text limit before
calling the session-backed save port. Preserve trimmed fields and current
warning/dirty behavior. Review and implementation have not begun. Gate 1 and
Gate 2 remain Partial; formal workspace-create acceptance and active-v2
DataService isolation remain Satisfied. Phase 2 remains In Progress/Open.

## Current Position - 2026-09-29 (F136 verified; F137 candidate review interrupted)

F126 source/test commit `ca4c1a97` (`Phase2 - integrate class details save
use case`) adds the Qt-free `ClassDetailsSaveUseCase` and session-backed
platform adapter, and routes `ClassDetailsPage` saves through that boundary.
The page retains normalized `ClassInfoValidator` feedback and separate regular
and intensive schedule-conflict checks. The adapter changes only requested
editable fields and preserves teacher assignment, notes, and time-filler
activities. The schedule conversion handles validated `h:mm AP` values,
including AM/PM, noon, and midnight.

The three focused F126 CTest targets passed 3/3. The production target and
focused targets built in the implementation run; the independent build was
up to date. A fresh independent rebuild could not be configured because Ninja
could not find `rc` and the Visual Studio generator could not find a C++
compiler. `git diff --check` passed. No full suite or baseline comparison ran.

F127 source/test commit `988b7de7` (`Phase2 - integrate class details page
read query`) adds a Qt-free typed-ID snapshot query for editor fields, raw
ordered schedule rows, teacher display data, and student count. The session
adapter retains independent source outcomes; the page preserves separate
fallbacks and refreshes the snapshot for load/reload, title retranslation, and
after save. The fresh save-validation read remains separate.

Four focused F127 CTest targets passed 4/4, including the title-refresh
regression and F126 save-page test. `ClassMngr` and the focused targets rebuilt
successfully with the Visual Studio developer environment; `git diff --check`
passed. No full suite or baseline comparison ran. Malformed schedule values
are tested as lossless through the app/platform boundary; malformed-value
widget rendering remains outside scope.

F128 source/test commit `93ad5afb` (`Phase2 - integrate schedule slot state
save use case`) adds a Qt-free typed command and session-backed adapter for
the shared schedule toggle. The UI still chooses the next/default state; the
adapter preserves override deletion when a slot returns to its default. The
ordinary toggle path remains shared by regular and intensive views, with the
testing-assignment path unchanged.

The app, platform, schedule-widget, and testing-classes focused CTest targets
passed 4/4. Coverage checks invalid requests, session persistence/default
deletion, regular and intensive weekday/time mapping, warning/reload behavior,
unavailable-service fallback, and the separate testing-assignment path.
`ClassMngr` built and `git diff --check` passed. No full suite or baseline
comparison ran.

F129 source/test commit `a0e3c98a` (`Phase2 - integrate roster save use case`)
adds a Qt-free single-class roster snapshot request and a session-backed
platform adapter. It preserves ordered columns, widths, all 25 row positions,
UTF-16 values, and the questionable Korean-name confirmation flag. The widget
keeps validation/focus, interactive confirmation, autosave/manual timing,
dirty-state and warning behavior, and class-selection save gating.

The application, platform, and page CTest selection passed 3/3; the existing
roster-import widget regression passed, and `ClassMngr` built. Independent
verification rebuilt the focused roster-save widget target and passed 1/1,
including direct assertions for invalid-cell focus, silent autosave failure,
and rollback of a testing-class selection after save failure. Whitespace
validation passed. No full suite or baseline comparison ran.

F130 source/test commit `70f3ddc9` (`Phase2 - integrate speaking evaluation
save use case`) adds a Qt-free typed save request and a session-backed adapter
for one speaking evaluation. It carries the ordered 25-by-11 UTF-16 matrix,
exact changed-cell coordinates, evaluation name, and questionable-name flag;
an empty changed-cell list retains the existing write-all meaning. The page
keeps validation/focus, confirmation, notices, dirty-baseline updates,
autosave/manual timing, and class/evaluation selection rollback.

The app, platform, and page focused CTests passed 3/3; `ClassMngr` built.
Independent verification reran the platform target after adding a discriminating
non-empty-delta case and passed 1/1. The case confirms that a changed value
outside the delta stays unwritten while the listed cell is persisted.
`git diff --check` passed. No full suite or baseline comparison ran.

F131 source/test commit `d0828493` (`Phase2 - integrate speaking evaluation
read query`) adds a Qt-free typed-ID query and a session-backed read adapter
for one selected evaluation. It preserves the exact evaluation-name key and
ordered UTF-16 rows. `SpeakingEvalPage` reads through the query while keeping
the existing model normalization and blank 25-by-11 clean state for empty or
failed reads.

The app, platform, and page focused CTests passed 3/3; `ClassMngr` built.
Independent verification confirmed exact-name behavior (`" Winter "` does
not match `"Winter"`), ordered Unicode rows, structured read failures, and
empty/failed page fallbacks. `git diff --check` passed. Partial/malformed-row
normalization at the page is source-reviewed but has no direct load-path test.
No full suite or baseline comparison ran.

F132 source/test commit `e878906c` (`Phase2 - integrate roster read query`)
adds a shared Qt-free `RosterSnapshot`, typed class-ID read query, and
session-backed adapter, then routes `RosterEditorWidget::loadClass` through
it. The app/platform boundary preserves ordered columns, widths, all raw rows,
and UTF-16 values without applying the UI's 25-row cap. The existing model
continues to normalize columns, names, widths, and displayed rows; read errors
still produce a blank, clean roster.

The app, platform, and page focused CTests passed 3/3; `ClassMngr` built.
Independent verification confirmed a 38-row raw snapshot, missing-roster and
repository-error distinction, session-only access, page normalization and
widths, 25-row display, blank/failed fallback, clean state, and capability
signals. `git diff --check` passed. There is no single page test loading 38
rows end-to-end; the raw and displayed limits are covered separately. No full
suite or baseline comparison ran.

F133 source/test commit `b111b799` (`Phase2 - integrate classes navigation
snapshot`) adds a Qt-free typed-ID navigation snapshot and active-session
adapter. The repository loads class metadata and teacher labels with one join,
then loads regular and intensive schedule rows in two ordered batch queries.
`ClassesPage` retains its tab model, filtering, selection, and refresh
behavior; a read failure retains class names with blank metadata.

`ClassMngr` and four focused test targets built; the app, platform, ClassesPage,
and navigation-model CTests passed 4/4. Independent verification confirmed
the three-query batch, missing-row fallbacks, ordered raw values, filtering,
selection, and class-info-save refresh. Verification used Ninja/MSVC after the
Visual Studio generator hit a FileTracker access-denied error.
`git diff --check` passed. No full suite or baseline comparison ran.

F134 source/test commit `cbb15e32` (`Phase2 - integrate schedule builder
source query`) adds a Qt-free schedule-source snapshot and active-session
Platform adapter, then keeps parsing and row construction in the service-free
builder. It reuses the repository's ordered batch read; this advances the
application boundary rather than reducing SQL query count. Raw day/start/end
strings and parser behavior remain intact.

The app, Platform adapter, builder, and ScheduleWidget focused CTests passed
4/4 in an independent fresh Ninja/MSVC build. The Platform fixture now inserts
classes in an order distinct from repository order and asserts the sorted
snapshot IDs and exact intensive end time. `git diff --check` passed. No full
suite or baseline comparison ran. After preview installation,
`ScheduleWidget` bypasses source, slot-state, and testing-assignment reads;
`ScheduleImportReviewDialog::buildUi()` also calls two setters that render the
live schedule before `prepare()` performs its normal preference refresh and
installs the preview. With services available, setup can perform three
pre-preview reads. This call order is present at the F134 parent; later preview
renders and refreshes bypass those reads.

F135 source/test commit `91806e5d` (`Phase2 - integrate schedule slot-state
read query`) adds a Qt-free query and active-session Platform adapter for
intensive slot-state overrides. The adapter reads the `DatabaseSession`
repository directly, without `ScheduleService` or `DataService` fallback, and
preserves ordered raw day/start/state strings. The widget retains overrides on
unavailable or failed reads, warns on failure, and replaces the full map on
success, including clearing it for an empty result.

Independent fresh Windows x64 Debug Ninja/MSVC verification built `ClassMngr`
and the app, Platform, and ScheduleWidget targets. The three focused CTests
passed 3/3, including an assertion for the existing warning prefix.
`git diff --check` passed. No full suite or baseline comparison ran.

F136 source/test commit `0295543a` moves the ScheduleWidget testing-assignment
display read through a Qt-free Application query and active-session Platform
adapter. One ordered joined repository read supplies assignments and optional
special-class display fields, replacing per-assignment service reads. The UI
preserves plain/special rendering, unavailable clearing, warning and prior
state retention on assignment-read failure, missing-special warning and skip,
blank/default class-info behavior, and preview bypass.

Independent fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 verification
built `ClassMngr` and the Application, Platform, and ScheduleWidget targets.
The three focused CTests passed 3/3:
`ClassMngrNextApplicationScheduleTestingAssignmentReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesScheduleTestingAssignmentReadPortTests`,
and `ClassMngrScheduleWidgetTests`. The Platform test confirms one recorded
statement at both one and 41 assignments. `git diff --check` passed. No full
suite or baseline comparison ran.

F137 candidate review began from this commit; two Explorer lanes were started
but interrupted at the user's request before reports arrived. No next slice
has been selected or implemented. Gate 1 and Gate 2 remain Partial; formal
workspace-create acceptance and active-v2 DataService isolation remain
Satisfied. Phase 2 remains In Progress/Open.

## Current handoff — 2026-09-29

Phase 2 F144 teacher-choice read is accepted. Its production checkpoint is
56c76f412b246230fcfe00c249b195dcc6ccd95f; the acceptance test commit is
89fbbaa250ddf98fae2ab1d80385fb99164ac055. The focused F144 Application,
Platform, and TestingClassesPage CTests passed 3/3. The F142/F143 Application
and Platform regression CTests passed 4/4 in a separate fresh short-path
Ninja build. All seven distinct required targets passed. Configure-time source
ownership validated 1,025 files. No full suite or full application build ran.

F144's teacher-choice read is accepted; the earlier Start Here ambiguity is
resolved in favor of F144. F145 has since been selected and accepted as the
existing Testing Class details update slice. F146 has since been accepted as
the Testing Class creation boundary with its optional pending schedule
assignment. The next milestone is a Heavy-route candidate review for F147. No
push was requested.

## macOS Qt checksum recovery — 2026-09-30

The macOS release and refactoring-baseline jobs both installed Qt 6.12.0 with aqt 3.3.0 and had no delayed install retry. The reported `ChecksumDownloadFailure` means aqt could not retrieve the qtbase SHA-256 sidecar; the exact HTTP response is unavailable, so a transient network or sidecar publication gap is plausible but not confirmed. The archive filename alone does not establish an aqt metadata parsing defect.

Both macOS install blocks now use a 30-second request timeout and retry the complete aqt install up to three times, with a 10-second delay between failed attempts. They preserve the existing Qt version, `clang_64` target, qtpdf module, output paths, and aqt pin. Checksum verification stays enabled, and the jobs still fail after the final unsuccessful attempt.

`git diff --check`, Ruby YAML parsing of both workflows, and `bash -n` on both edited run blocks passed; an independent review confirmed the control flow and retained settings. No live Qt install or GitHub Actions run was performed. The underlying sidecar response remains unobserved; a permanent missing-sidecar condition will still fail closed after retries.

## F145 acceptance — 2026-09-30

F145 routes existing Testing Class details updates through a Qt-free
Application use case and active-session Platform adapter. New-class creation,
pending schedule assignment, and deletion cascades remain separate. The page
preserves roster-first saves: a roster failure blocks the detail update, while
a later detail failure leaves the successful roster save persisted and clean.

Production commit `26a916df9994217ffd3f12f45148207e9cf5e0c2` and acceptance
test commit `ba1b7cdec15f6f163bb1620897fb4c2e2b3baccb` are accepted. A fresh
Windows x64 Debug Ninja/MSVC configure and focused build passed; all three
F145 CTests and seven F142–F144 regressions passed (10/10). CMake source
ownership validation ran. `git diff --check` passed. No full suite or full
application build ran. F146 is recorded below; Phase 2 exit gates remain open.

## F146 acceptance — 2026-09-30

F146 routes new Testing Class saves through a Qt-free Application use case and
active-session Platform adapter. Optional weekday/start-time assignment is
passed to the same repository create operation, preserving its transaction.
The new-class page path, F145 update path, and delete path remain distinct.

Production commit `d7acb516cd9261e2199f742b46f90de48aa907d1` and acceptance
test commit `14d2d124a3d8b0548d241f1d2dcea136dbee55d9` are accepted. A fresh
Windows x64 Debug Ninja/MSVC build passed all 14 focused F146, repository, and
F142–F145 regression CTests. The repository rollback test verifies no class,
details, room, or assignment rows remain after a conflicting slot create.
`git diff --check` passed. No full suite or full application build ran. The
next step is a Heavy-route candidate review for F147; Phase 2 gates remain
open.

## F147 acceptance — 2026-09-30

F147 routes Testing Class deletion through a Qt-free Application use case and
active-session Platform adapter while retaining the repository's transactional
cascade. The confirmation now discloses the roster, notes, speaking
evaluations, regular and intensive class times, and schedule assignments.
After successful deletion, the page clears the deleted class's dirty editor
and roster state before selecting a sibling, preventing an accidental save or
create during the selection transition.

Production commit `b037b4216b71c55c7793df5f7bbbfc4a00690065`, transition fix
`315b3ff33b7e2ab42b43d52cd168ce21a92158c9`, and test commit
`397376e439f4b5955c82948ab0c225aaf776d679` are accepted. Fresh Windows x64
Debug Ninja/MSVC verification passed all 17 F147, repository, and F142–F146
regression CTests. Cascade success preserves sibling rows; a final-delete
trigger verifies rollback. `git diff --check` passed. No full 220-test suite or
full application build ran. The next step is a Heavy-route candidate review
for F148; Phase 2 gates remain open.

## F148 class-details save parity — 2026-09-30

F148 adds a common-input success case for `ClassDetailsPage` saving through
the real page and active services. Commit `6c7211d6427b6dbcddd4d109d9d09f9eeff14f28`
adds the test and its CMake registration. The same seeded save passed on a
fresh current archive and pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`.
The assertions cover persisted detail fields, preserved teacher/notes/
activities, regular and intensive schedule order, the saved signal, and clean
page state. Gate 2 gains one successful-save comparison but remains Partial;
the case does not establish validation or conflict parity. Phase 2 remains
In Progress/Open.

## F149 Class Details schedule-conflict query — 2026-09-30

F149 adds a Qt-free typed conflict query and active-session Platform adapter
for the Class Details page. The adapter reads
`DatabaseSession::classInfoRepository()->getClassTimeConflicts()` directly;
warning rendering stays in the page, overlap/order stay in persistence, and
the existing save-time `ClassService` checks remain.

Commit `2e7d8866` (`Phase2 - route class details conflict checks through typed
query`) passed the fresh focused build and CTests 4/4; the pinned baseline
parity CTest passed 1/1 for F148 success plus F149 regular/intensive conflict
cases. No full suite or application build ran. Gate 1 and Gate 2 remain
Partial, and Phase 2 remains In Progress/Open.

## F150 selection record — 2026-09-30

F150 moves Class Details pre-save normalization and validation behind a
Qt-free typed Domain/Application policy, then maps structured issues back to
the existing page field feedback. Keep `ClassService` save-time validation
and F149 conflict preflight unchanged. Preserve book-catalog rules, malformed
schedule diagnostics, duplicate-schedule semantics, hidden-field checks, and
stable issue order. Make legacy unordered duplicate-group diagnostics
deterministic in first-seen order. See the acceptance record below; Phase 2
gates remain Partial/Open.

## F150 Class Details validation policy — 2026-09-30

Commit `854f9849` moves page pre-save normalization and validation into a
Qt-free typed policy. The page adapts the live `ClassInfoConfig` catalog,
retains malformed schedule row text, maps structured issues to existing field
feedback, blocks conflicts/saves for invalid input, and saves normalized
values. `ClassService` save-time validation and F149 conflict ordering remain.

Fresh Windows x64 Debug Ninja/MSVC verification (CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257, Qt 6.12.0) validated 1,047 handwritten source owners. The
policy, page validation, parity, and shared-policy targets passed CTest 4/4.
The pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity target
passed 1/1 with only the test source/registration and three Qt minimum bumps
overlaid; no production source was changed. Parity covers F148 successful
save, F149 regular/intensive conflicts, and F150 invalid save. A regression
also checks trimming of hidden persisted notes and activity text. Representative
page field/focus mappings are tested; mapping every field directly through the
page remains a coverage limitation. No full suite or application build ran.
Gate 1 and Gate 2 remain Partial; Phase 2 remains In Progress/Open. F151 is
selected to add live-page baseline parity for malformed regular/intensive
schedule validation, end-before-start, and duplicate rows. Compare duplicate
membership and row-specific feedback semantically because the legacy
cross-group `QHash` order is unspecified. A typed Application read for hidden
persisted validation fields remains a separate subsequent candidate.

## F151 Class Details invalid-schedule parity — 2026-09-30

Commit `f232301e` adds four live-page common-input parity cases for malformed
regular schedules, malformed intensive schedules, end-before-start, and
duplicate rows. The cases assert field/row/column feedback, dirty-state
retention, no visible conflict warning, no saved signal, and unchanged target
and source records. Duplicate groups are compared semantically without
depending on cross-group ordering.

Fresh Windows x64 Debug Ninja/MSVC verification (CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257, Qt 6.12.0) validated 1,047 handwritten source owners. The
policy, page, parity, and shared-policy CTests passed 4/4. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity target passed 1/1 (6/6
QtTest cases). The baseline overlay added only parity test source/registration
and three Qt minimum bumps; its production `class_details_page.cpp` matched
the pinned Git blob. Baseline parity proves no visible conflict warning or
write; direct query-count assertions remain in the current-only page tests.
No full suite or application build ran. Gate 2 gains these validation cases
and remains Partial. Gate 1 remains Partial; Phase 2 remains In Progress/Open.
F152 is selected: replace the page's direct `ClassService::classInfo()` read
with a fresh typed validation-context query using raw signed teacher ID and
exact UTF-16 notes/activity. Preserve the `-1` sentinel, missing-row defaults,
and read-failure fallback/order; keep the context out of the save request so
the save port rereads current values.

## F152 Class Details validation-context read — 2026-09-30

Commit `f70e3e23` replaces the page's direct `ClassService::classInfo()` read
with a fresh typed Application validation-context query and active-session
Platform adapter. The query returns raw signed teacher ID and exact UTF-16
notes/activity, checks the matched class ID, and the page uses an empty
`ClassInfo{}` context on query failure before continuing the existing
validation/conflict/save sequence. The save request is unchanged; the save
adapter still rereads current data and `ClassService` retains its final guard.

Independent Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257, Qt 6.12.0, and embedded debug information after PDB update
errors with `/Zi`. Current configure validated 1,051 handwritten source
owners. The query, Platform port, F150 policy, page save/display, parity, and
shared-policy targets passed 7/7 with `CL` cleared. The pinned baseline
`f232301e48f1e198d301acdfa3d8f704569f7ddc` parity CTest passed 1/1 with only
the parity-test source overlaid; the baseline production page matched its Git
blob. Parity covers stale hidden fields after page load and valid save
preservation. No full suite or application build ran. The first verification
attempt exposed a missing test include and unseeded `class_info` rows; both
test fixtures were repaired before the passing recheck. Gate 1 advances but
remains Partial; Gate 2 remains Partial. Phase 2 remains In Progress/Open.

## F153 teacher-ID validation parity — 2026-09-30

Commit `477ed151` adds current/baseline Class Details page parity when the
persisted `teacher_id` is changed to `0` after page load. The page reports
`class_info.teacher_id.invalid` on `teacherId` with value `0`, remains dirty,
shows no conflict warning, emits no save signal, and leaves target/source
records unchanged.

Independent Windows x64 Debug verification passed the current parity and
page-save CTest targets 2/2; the page-save target also contains the F152
direct no-conflict-query regression. The pinned legacy baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` passed the focused F153 parity
harness 1/1. Its temporary overlay contained only adapted parity test source,
test registration, and Qt minimum bumps; no production source was overlaid.
The baseline page matched blob `cdc48da8e3bab73dd0e064cf8364899f67ad1021`.
Toolchain: CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0, with
`CL` cleared and embedded debug information. No full suite or app build ran.
Parity uses a seeded conflict as a warning trap and does not count repository
queries; direct query-count evidence remains in current-only tests. Gate 2
remains Partial; Gate 1 remains Partial; Phase 2 remains In Progress/Open.

## F154 Class Notes page read boundary — 2026-09-30

Commit `8bcbf136` adds a dedicated typed Application read query and
active-session Platform adapter for Class Notes. The small projection carries
class ID, exact UTF-16 notes/activity, grade/level, regular schedule day/start,
and preferred teacher display name, with class and teacher results
independent. The page keeps trimming, subtitle formatting/fallbacks, and the
existing save boundary. Load and discard use the query; refresh/save add no
reads. The adapter does not use `DataService` fallback.

Independent Windows x64 Debug verification passed six current focused targets
6/6: Application query, Platform adapter, page, parity, and both existing save
port targets. Current CMake validated 1,058 handwritten source owners. The
original pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` passed
the focused page-parity harness 1/1 for initial text/subtitle and discard
reload. Baseline overlays were limited to parity test source/registration and
Qt minimum changes; no production source was overlaid. Baseline page and
header matched blobs `bbc9bc24a053aca83434eba6efac1e4ad5801bc2` and
`5c825327f1393791d7101ab33c10999768ff639a`. Toolchain: CMake 4.4.2, Ninja
1.13.2, MSVC 19.51.36257, Qt 6.12.0, `CL` cleared, embedded debug info. The
current run built the F154 working-tree source immediately before it was
committed unchanged as `8bcbf136`. No full suite or app build ran. Gate 1 and
Gate 2 remain Partial; Phase 2 remains In Progress/Open.

## F155 selected Class Co-Teacher read boundary

Add a dedicated typed page read query and active-session Platform adapter for
the assigned teacher ID and class/teacher fields used by
`SidebarNodeNaming`. Use it on load/discard and after successful assignment to
refresh selection and title. Preserve missing/read-error fallbacks. Leave the
teacher-choice catalogue read and existing assignment use case/adapter
unchanged; do not expand into roster, schedule, or teacher-profile work.
Acceptance should cover typed identity, independent field/teacher outcomes,
current page query behavior, load/discard/post-save title behavior, and
current-versus-pinned-baseline display parity. Gates 1 and 2 remain Partial;
Phase 2 remains In Progress/Open.

The older F123 candidate notes predate commit `9f7e736b`, which contains
`TeacherInfoPage` use-case integration and tests. The production integration
is present in the current checkout; this continuation did not re-verify its
CTest target or baseline parity.

## F155 accepted; F156 selected — 2026-09-30

F155 commit `2d810d0e21f85233575b700e927ec3a36c907f21` adds the typed
Co-Teacher selected-class/title read boundary. Six focused current CTest
targets passed 6/6. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity harness passed 1/1 with
test-only adaptation and no production overlay. Both versions assert initial
selection/title, discard after an external change, and post-save selection,
title, and persistence. No full suite or application build ran; no baseline
query-count claim is made.

F156 is selected for a separate typed Application and active-session Platform
read of the Co-Teacher teacher-choice catalogue. Preserve displayed fields,
bilingual ordering, selection, and load-failure warning/clear behavior. Gates
1 and 2 remain Partial; Phase 2 remains In Progress/Open.

## F156 accepted — 2026-09-30

F156 commit `3581078bdca61cfe76489ff19c5d518d8b3145bb` adds a separate typed
Co-Teacher teacher-choice read query and active-session Platform adapter. The
current eight focused CTest targets passed 8/8; the pinned baseline parity
harness passed 1/1 with test-only adaptation and no production overlay. Tests
cover direct repository mapping, no fallback, warning/clear behavior, crossed
Korean/English choice ordering, selected details, and load/discard behavior.
No full suite/app build or baseline query-count claim. Gates 1 and 2 remain
Partial; Phase 2 remains In Progress/Open. Three independent reviews are
complete; F157 is selected for the Teacher Profile persistence adapter.

## F157 accepted; F158 selected — 2026-09-30

F157 commit `cb6199f3369420c0e2e6c77d11f853d2799d9f92` moves Teacher Profile
save/reload persistence from the page-local `TeacherService` adapter to an
active-session Platform port backed by `TeacherRepository`. It maps every
profile field and preserves the existing edit use case, validation, canonical
reload, warnings, dirty state, and save signal. The focused current build and
CTest passed 4/4; the pinned baseline public-page parity passed 1/1 with only
the parity test source/registration overlaid and no production overlay. No full
suite/app build or baseline query-count claim. The tested page edit path also
reverifies the save/use-case and invalid-write behavior previously associated
with F123; no broader F123 claim is made.

F158 is selected for a typed Teacher Profile read query and active-session
Platform adapter at `NavigationController::handleTeacher`, using
`TeacherRepository::getTeacher()`. Keep it separate from the F157 edit port;
preserve lookup failure, confirmation, and page-load behavior. Acceptance will
cover ID/field mapping, session/repository errors, no fallback, current route
behavior, and pinned-baseline visible parity. Gates 1 and 2 remain Partial;
Phase 2 remains In Progress/Open.

## F158 and F159 accepted; F160 selected — 2026-09-30

F158 commit `4d099893071d4271ea8873d2219dfc7642de1e5c` adds the typed Teacher
Profile read query and active-session Platform adapter at
`NavigationController::handleTeacher`. The focused current build and CTest
passed 8/8; pinned-baseline navigation parity passed 1/1. The baseline run
used installed Qt 6.12.0 with a temporary scratch-only Qt metadata shim and a
three-argument constructor adaptation in the copied test. No production
overlay, full suite/app build, or baseline query-count claim. Phase 2 remains
In Progress/Open; no push was requested.

F159 source commit `1849ed23327538e2d21b05dfe0cebcc97e99d78c` adds a typed
Qt-free Application query and active-session Platform adapter for only the
Native English branch of `StaffDirectoryPage::loadDirectory()`, backed by
`NativeEnglishTeacherRepository::getAll()`. `NativeEnglishTeacherId` keeps its
table identity distinct from Korean teacher IDs. The focused current build and
CTest passed 5/5 targets. Pinned-baseline public-page parity passed 1/1 with
only parity/test-harness changes, no production overlay, and pinned production
blobs unchanged. The scratch harness used a temporary Qt 6.12 metadata shim
and baseline API adaptations, then restored the original baseline CMake hash.
No full suite/app build or baseline query-count claim.

## F160 accepted; F161 selected — 2026-09-30

F160 source commit `b703d3260a01b783594ffe6b87d10d9f7b02d193` adds a typed
Qt-free Application read query and active-session Platform adapter for the GS
Team branch of `StaffDirectoryPage::loadDirectory()`, backed by
`GsTeamRepository::getAll()`. It introduces a dedicated int-backed
`GsTeamMemberId` and preserves the five displayed fields, row ID role,
repository order, unavailable-session silent clear, repository-error warning,
and success-state behavior. The route race test closes the session during
leave confirmation; the page remains hidden with no legacy fallback.

The focused current CTest selection passed 9/9, including the F160 query,
adapter, page and route cases plus F159 and `StaffDirectoryPage` regressions.
Pinned-baseline visible parity passed 1/1 with only the F160 parity test and
registration overlaid; production blobs stayed pinned. The scratch run used a
temporary Qt metadata compatibility shim and a baseline constructor
adaptation, then restored the pinned CMake hash. No full suite/application
build or baseline query-count claim. Gates 1 and 2 remain Partial; Phase 2
remains In Progress/Open.

## F161 accepted; F162 selected — 2026-09-30

F161 source commit `ac173977d8d517d4af3236ee7794368bbe9a6bdc` adds a typed
Qt-free Application save/validation operation and active-session Platform
adapter for only the Native English branch of
`StaffDirectoryPage::saveDirectory()`, backed by
`NativeEnglishTeacherRepository::saveDirectory()`. The Application policy
owns empty/duplicate comparison-key and valid-or-blank birthday decisions;
the feature edge supplies Qt-normalized keys and date facts. The page retains
the localized warning, typed IDs, add/update/delete, transaction behavior,
dirty state on failure, quiet autosave, reload, and success signal.

Focused current CTest passed 12/12. Pinned-baseline visible save parity passed
1/1 against `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, with only a parity
test and registration overlay; production blobs remained pinned. The scratch
test adapted the baseline database-session API, and its root CMake was
restored to hash `cc8a061dfa64977926805167cc10418ca15d83d8`. No full
suite/application build or baseline query-count claim. Gates 1 and 2 remain
Partial; Phase 2 remains In Progress/Open.

F162 source commit `00a56324f1435475e3a7479fec99bc2e01653495` adds a
GS Team-specific typed Application save operation and active-session Platform
adapter backed by `GsTeamRepository::saveDirectory()`. It uses
`GsTeamMemberId`; requires at least one English or Korean name; enforces key
uniqueness within each language namespace while allowing cross-namespace
matches; and accepts valid or blank birthdays. The page preserves typed row
and deleted IDs, add/update/delete transaction behavior, warning and quiet
autosave paths, dirty-on-failure state, reload, and `directorySaved`.

Focused current CTest passed 16/16, including the F159-F162 policy, adapter,
persistence, and `StaffDirectoryPage` regressions. Pinned-baseline GS Team
save parity passed 1/1 on `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, with
only the parity test and registration overlaid and no production overlay. The
current C: build exhausted disk during link; serial focused verification passed
from D: scratch. Baseline setup adapted three database-session calls and used
a temporary Qt metadata shim; root CMake was restored to pinned hash
`cc8a061dfa64977926805167cc10418ca15d83d8`. No full suite/application build or
baseline query-count claim.

F163 source commit `a556c0441dcebbb3b6a7baecef6e293b5644b149` adds a typed
class ID/name list query and active-session Platform adapter backed by
`ClassRepository::getClasses()`, migrating only the ClassesPage list reads on
open and after ClassInfo save. It preserves repository ordering, names, IDs,
selection, and existing failure handling without a DataService fallback.

The focused Application and Platform CTests passed 2/2; the relevant direct
ClassesPage slots passed. Pinned-baseline parity passed for both visible list
opening and post-save rename/reordering on
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, with only test/stub overlays and
no production changes. The full ClassesPage CTest stalled in the existing
`classDetailsAndCoTeacherTabsSeparateTheirSectionCards()` slot. No full
suite/application build or query-count claim.

F164 source commit `85af7830708f062e34c94fde8fa0ff40310a8d9e` adds a typed
selected-class ID/grade query and active-session Platform adapter to
`ClassInfoRepository::loadClassInfo()`. Only
`ClassesPage::rebuildSectionTabs()` uses it. The query has no DataService
fallback; missing or failed grades keep Analytics and Evaluations visible.
Existing grade normalization, preference override, and section selection are
preserved.

Focused Application and Platform tests each passed 4/4 test slots; three
focused page slots passed. The existing middle-school/preference tab slot
passed against both current code and the exact pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`. Baseline production sources were
unchanged. A separate real-database parity harness timed out after 300 seconds
on both builds; no mismatch or cause was established. No full suite/application
build or query-count claim.

F165, source commit `e0b9d61213af57a79a685264c9f64fabf841bd1a`, adds a typed
Qt-free selected-class subtitle read and active-session Platform adapter.
Repository projections include only class grade/level, regular meeting
day/start time, and teacher display-name fields. Class and teacher outcomes
remain independent; formatting, “No class selected,” classroom-name/`Class N`,
and formatter fallbacks stay in the UI. The F163 list and F164 section-grade
paths remain separate.

Focused Application and Platform CTests passed. The ClassesPage slots
`selectedClassSubtitleUsesIndependentReadOutcomesAndRefreshes`,
`selectedClassSubtitleFallbackChainUsesTrimmedValues`, and
`selectedClassGradeFailureFailsOpenWithoutDataServiceFallback` passed. The
subtitle test covers class-read defaults, teacher failure retaining class
details, and a legacy-services-available/session-unavailable case without a
legacy class-info read. On exact pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, a test-only slot confirmed the
visible subtitle `E4 Hercules • Susan • Tues (4:00)`; the equivalent current
slot passed. All 584 baseline `src/` blobs matched. `git diff --check` passed.
No full suite/application build or query-count claim.

F166 source commit `577aea078a01b3a3986c07336c06631394620fcc` migrates only
`RosterEditorWidget::updateHeaderText()` to F165's typed subtitle query and
active-session adapter. Formatting stays at the UI edge. An unavailable outer
read falls back to the trimmed classroom name or `Class N`; class-detail
failure retains formatter defaults and teacher failure retains class details.
The invalid-ID message, title, embedded heading, roster load, and save paths
remain unchanged.

The current `ClassMngrClassesPageTests` target built with VS 18 2026 and Qt
6.12. Focused slots for the roster subtitle, no-session/name fallback, and
F165 independent outcomes passed (5/5 including QtTest initialization and
cleanup). The visible subtitle `E4 Hercules • Susan • Tues (4:00)` passed on
the pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` with only a
temporary test-source slot overlaid; the scratch source was restored and no
production source was overlaid. Coverage includes class-detail defaults,
teacher failure, invalid ID, trimmed name/`Class 42` fallback, and no-session
name fallback while legacy services were available without a legacy class-info
read. `git diff --check` passed. An unfiltered page-test run stalled at startup
and was stopped after 30 seconds. No full suite, application build, or
query-count claim.

F167 is selected: migrate the existing `ClassDetailsPage` display read
adapter, `ApplicationServicesClassDetailsPageReadPort`, from legacy class,
teacher, and roster services to active-session `ClassInfoRepository`,
`TeacherRepository`, and `RosterRepository` reads. Reuse the existing
`ClassDetailsPageReadSnapshot`; preserve independent class, teacher, and
student-count outcomes, raw schedule text/order, page defaults/fallbacks, and
`Teacher::preferredDisplayName()` precedence. Acceptance covers adapter
mapping/errors without DataService fallback, current display regressions, and
pinned-baseline visible fields, schedules, teacher, count, and fallback
parity. Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open. A
separate worker owns workflow repair; this work does not modify
`.github/workflows/refactoring-baseline.yml`. No push was requested.

## Archived Phase 2 position - 2026-10-01 (F178 accepted; F179 selected)

F170 source commit `68391fac` moves `ApplicationServicesRosterReadPort` from
`RosterService::roster()` to the active session's `RosterRepository`, with no
service or `DataService` fallback. The port preserves canonical class-ID
validation and complete ordered sparse snapshot mapping, including columns,
widths and UTF-16 cells; empty rosters succeed and repository failures remain
technical. Independent focused CTest passed 3/3: application query (0.02s),
Platform port (0.19s), and `RosterEditorWidgetSave` (2.08s). The two focused
roster editor read slots also exited successfully. `git diff --check` passed.
No full suite or application build was run.

F171 source commit `7cb8e5d7` migrates `ApplicationServicesRosterSavePort`
from `RosterService::saveRoster()` to the active-session
`RosterRepository::saveRoster()`, with no service or `DataService` fallback.
It preserves canonical-ID/session checks, roster normalization and validation,
the questionable Korean name-length choice, and Technical failure mapping.
Tests cover normalized snapshot persistence, complete no-write behavior for
invalid input, the allow flag, repository rollback, and the closed-session
path. Focused CTest passed 3/3: application save use case (0.02s), Platform
save port (0.24s), and `RosterEditorWidgetSave` (2.03s; 2.30s total). The
independent Tester also passed 3/3. `git diff --check` passed. No full suite or
application build was run.

F172 source commit `2daa209e` migrates
`ApplicationServicesClassCoTeacherAssignmentPort` to the active session's
`ClassInfoRepository`, with no `ClassService` or `DataService` fallback. It
preserves positive-ID behavior, typed assignment/unassignment, complete stored
class fields and schedules, and joined teacher metadata behavior. It also
retains `ClassInfoValidator` normalization/validation and regular-before-
intensive conflict checks with the existing message. Port tests cover invalid
IDs and loaded data, exact regular and intensive conflict errors and priority,
no-write preservation, injected transactional failure, and open/closed session
behavior. Independent focused CTest passed 3/3: Application use case, Platform
port, and Co-Teacher page targets. The independent Tester also passed 3/3.
`git diff --check` passed. No full suite or application build was run.

F173 source commit `a214dec4` migrates `ApplicationServicesScheduleSlotStateSavePort`
to the active session's `IntensiveSlotStateRepository`, removing the service
and `DataService` fallback. It preserves typed validation, weekday/time/state
mapping, default-state deletion, and widget behavior. Independent focused CTest
passed 3/3: application use case, Platform port, and ScheduleWidget. The
independent Tester also passed 3/3. `git diff --check` passed. No full suite or
application build was run.

F174 source commit `16188918` migrates
`ApplicationServicesSpeakingEvaluationReadPort` to the active session's
`SpeakingEvalRepository`, without service or `DataService` fallback. Both the
implementation run and independent verification passed the 3 focused targets:
application query, Platform port, and SpeakingEval page. Coverage includes
canonical IDs, exact and missing-name behavior, ordered UTF-16 matrix data,
repository failures, and closed-session behavior; page tests retain blank,
clean state after failed reads. `git diff --check` passed. No full suite or
application build was run.

F175 source commit `4009fcd5` changes `ApplicationServicesScheduleBuilderSourcePort`
from constructing `ClassService(session, nullptr)` to calling the active
session's `ClassInfoRepository::loadScheduleClassInfos()`. The implementation
run and independent verification passed all 4 focused targets: application
snapshot, Platform port, ScheduleBuilder, and ScheduleWidget. Existing coverage
checks query metrics, ordering, raw schedules, teacher/profile edge cases,
testing-class exclusion, and widget modes. `git diff --check` passed. No full
suite or application build was run.

F176 source commit `e111d5be` migrates
`ApplicationServicesSubPrepClassDetailsPort` from
`ClassService::subPrepClassDetails()` to the active session's
`ClassInfoRepository::loadSubPrepClassDetails()`. It preserves canonical
class-ID and record-identity checks, preferred-name selection, bounded UTF-8
fields, missing/stale teacher and class-info defaults, structured errors, and
the post-read closed-session recheck. Implementation and independent
verification each passed 3/3 focused targets:
`ClassMngrNextApplicationSubPrepClassDetailsQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and
`ClassMngrSubPrepPageTests`. Tests cover unavailable/closed sessions and
repository read failure. `git diff --check` passed; no full suite or application
build was run.

F177 source commit `25b9719c` migrates
`ApplicationServicesSubPrepScheduleSummaryPort` to the active session's
`ClassInfoRepository::loadSubPrepClassSummaries()`. It preserves request
validation, empty-scope success without a repository read, requested class
order and omission rules, bounded summary projection and meeting formatting,
and unavailable/read error mapping. Implementation and independent
verification each passed 3/3 focused targets:
`ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and
`ClassMngrSubPrepPageTests`. `git diff --check` passed; no full suite or
application build was run.

F178 source commit `afeab035` migrates
`ApplicationServicesSubPrepPrintSourcePort` from feature services to the
active session's class-info, teacher, and roster repositories. It preserves
selected mode/day filtering and class order, teacher caching and first-
reference order, missing/unassigned-teacher omission, bounded owning output,
class/teacher error mappings, and the zero student-count fallback only for
roster-count failures. The Platform test adds unopened/closed-session
`NotFound` coverage. Implementation and independent verification passed 4/4
focused targets: `ClassMngrNextApplicationSubPrepPrintSourceQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`,
`ClassMngrSubPrepPrintSourceMapperTests`, and `ClassMngrSubPrepPageTests`.
`git diff --check` passed. The build emitted a nonfatal `vswhere.exe` warning;
no full suite or application build was run.

F179 is selected: migrate `ApplicationServicesSubPrepRosterOutputSourcePort`
to the active session's class-info, class, teacher, and roster repositories.
Preserve request validation and empty-scope no-read, selected class order and
mode/day filtering, unassigned/missing/stale-teacher behavior, class and
schedule identity checks, extra-column deduplication, UTF-8 and aggregate text
limits, and cumulative remaining row/cell/text budgets for roster projection.
Keep existing error mappings, avoid `DataService` fallback, and return no
partial output on failure. Focused targets are
`ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests`,
`ClassMngrSubPrepPackageServiceTests`, and `ClassMngrSubPrepPageTests`. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial. Workflow repair remains
with the other worker.

## Archived Phase 2 position - 2026-10-01 (F196 accepted; F197 selected)

F179-F195 advanced session-backed Sub Prep and Calendar operations and six
preference ports; their individual commits and acceptance evidence are recorded
in the [Phase 2 progress log](../plans/qt-rewrite-heavy-route-plan/03-Phase-2-Progress-Log.md).

F196 source commit `b9f0a07d` migrates
`ApplicationServicesCurrentCampusPreferencesPort` to the active session's
`SettingsRepository`. It preserves `myInfo/campus`, verbatim UTF-8 and
`QVariant::toString()`, availability, empty read/no-op write behavior while
unavailable or closed, Technical write-error mapping, and unrelated settings.
The Executor's focused run passed all nine substantive cases; the independent
registered CTest passed 1/1 after an initialized MSVC rebuild. Source review
confirmed no `DataService`/`SettingsService` fallback, and `git diff --check`
passed. No full suite or application build ran.

F197 is selected for `ApplicationServicesMiddleSchoolAnalyticsPreferencesPort`
and its existing focused Platform target. Migrate to the active open session's
`SettingsRepository`; preserve the exact key, `QVariant::toBool()`, false
fallback and default-materialization attempt, unavailable/closed behavior, and
existing callers. Add closed-session no-fallback coverage. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

## Previous Phase 2 position - 2026-10-01 (F197 accepted; F198 selected)

F197 source commit `26c0f23b` migrates
`ApplicationServicesMiddleSchoolAnalyticsPreferencesPort` to the active open
session's `SettingsRepository`. It preserves the exact visibility key,
`QVariant::toBool()`, false fallback and materialization attempt, and
unavailable/closed behavior. The independent registered CTest passed 1/1. A
SQLite-trigger test verifies a failed explicit save stays silent and preserves
the previous value; closed-session tests verify no `DataService` fallback.
`git diff --check` passed. No full suite ran.

F198 is selected for `ApplicationServicesPersonalDetailsSavePort` and its
existing focused Platform test. Move the nine-key bundle to one active-session
`SettingsRepository::saveSettings()` transaction. Preserve exact keys and
values, UTF-8 and signature-image processing, signature normalization,
rollback, typed errors, and callers. Add closed-session no-fallback coverage.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

## Previous Phase 2 position - 2026-10-01 (F198 accepted; F199 selected)

F198 source commit `0e8361eb` migrates
`ApplicationServicesPersonalDetailsSavePort` to one active-session
`SettingsRepository::saveSettings()` batch. It preserves the nine exact keys,
UTF-8 values, prepared signature-image encoding, mode/font normalization,
Technical error mapping, and atomic rollback. The independent Tester rebuilt
the focused target in the x64 MSVC environment and passed CTest 1/1. Added
closed-session coverage confirms no `DataService` fallback and verifies all
seeded settings remain unchanged after reopen. `git diff --check` passed; no
full suite ran.

F199 is selected for `ApplicationServicesPersonalDisplayNamePreferencesPort`.
Move reads and writes for `myInfo/name` to the active session's
`SettingsRepository`; preserve exact UTF-8 and whitespace, empty reads and
successful no-op writes while unavailable, and Technical write-error mapping.
Add closed-session no-fallback coverage and verify the read port sees the name
written by F198's aggregate save. Focused target:
`NextPlatformApplicationServicesPersonalDisplayNamePreferencesPort`. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

## Previous Phase 2 position - 2026-10-01 (F199 accepted; F200 selected)

F199 source commit `dc489863` migrates
`ApplicationServicesPersonalDisplayNamePreferencesPort` to the active
session's `SettingsRepository`, with no service/facade fallback. It preserves
`myInfo/name`, exact UTF-8 and whitespace, empty reads and successful no-op
writes while unavailable, Technical write errors, and warnings on failed
reads. The independent Tester rebuilt the focused x64 MSVC target and passed
CTest 1/1. Tests prove closed-session behavior with `DataService` present,
read-error fallback and warning, and visibility of a name written by F198's
aggregate port. `git diff --check` passed; no full suite ran.

F200 is selected for `ApplicationServicesPersonalSignaturePreferencesPort`.
Move reads of `myInfo/signatureMode`, `myInfo/typedSignatureFont`, and
`myInfo/typedSignatureText` to the active session's `SettingsRepository`.
Preserve persisted-mode normalization, `QVariant::toInt()` font behavior,
UTF-8 text and whitespace, defaults without writes for missing/invalid values,
Technical unavailable failures, and warning/default behavior on read errors.
Add closed-session no-fallback coverage with `DataService` present; keep
callers and generic/sessionless `SettingsService` unchanged. Focused target:
`NextPlatformApplicationServicesPersonalSignaturePreferencesPort`. Both
Explorer lanes recommended this adapter as the next reader for F198's
aggregate bundle. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

## Previous Phase 2 position - 2026-10-01 (F200 accepted; F201 selected)

F200 source commit `ef603583` migrates
`ApplicationServicesPersonalSignaturePreferencesPort` to the active session's
`SettingsRepository`. It preserves the exact mode/font/text keys, mode
normalization, `QVariant::toInt()` font conversion, UTF-8 text, and defaults
without writes for missing/invalid values. Failed repository reads still warn
per key and return defaults; unavailable/null/closed sessions return the
existing Technical error. Independent focused CTest
`ClassMngrNextPlatformApplicationServicesPersonalSignaturePreferencesPortTests`
passed 1/1. Tests cover unchanged invalid values, repository read errors,
closed-session no-fallback with `DataService`, and values written by F198's
aggregate port. `git diff --check` passed; no full suite ran.

F201 is selected for `ApplicationServicesPersonalSignatureImagePort`. Move the
`myInfo/signatureImage` read to the active session's `SettingsRepository`.
Preserve Base64 decoding and `SignatureImage::prepareForEmbedding`, exact-key
lookup, empty outputs for missing/corrupt/unavailable/read-error cases, and
the warning on repository read error. Add closed-session no-fallback coverage
with `DataService` still present; keep the existing read interface and callers
unchanged. Focused target:
`NextPlatformApplicationServicesPersonalSignatureImagePort`. Both Explorer
lanes independently recommended this candidate because it complements F198's
aggregate writer. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

## Archived Phase 2 position - 2026-10-01 (F205 accepted; F206 selected)

F201 source commit `59133929` migrates
`ApplicationServicesPersonalSignatureImagePort` to the active open session's
`SettingsRepository`. It preserves exact-key lookup, `toString().toLatin1()`
Base64 decoding, one `SignatureImage::prepareForEmbedding()` call, and empty
results for missing, corrupt, unavailable, and read-error values while
preserving the repository-read warning. Independent focused CTest
`ClassMngrNextPlatformApplicationServicesPersonalSignatureImagePortTests`
passed 1/1. Tests cover valid image preparation/transparency, read-error
warning, closed-session no-fallback with `DataService` present, and unchanged
image/unrelated values after reopen. `git diff --check` passed; no full suite
ran.

F202 source commit `c9ff5731` migrates
`ApplicationServicesClassVisibilityPreferencesPort` to the active open
session's `SettingsRepository`. It preserves the exact key and enum/string
mapping, trimmed/lowercased `all_classes`, default persistence for missing or
invalid reads, no rewrite of valid unrecognized values, ActiveSchedule when
unavailable, and silent write failures. Independent focused CTest
`ClassMngrNextPlatformApplicationServicesClassVisibilityPreferencesPortTests`
passed 1/1. Coverage includes valid unsupported values remaining unchanged,
closed-session no-fallback with `DataService` present, a failed write that
preserves the stored value, and a silent repository read failure. No full suite
ran.

F203 source commit `4f128b01` migrates
`ApplicationServicesEvaluationDefaultPolicyPort` to the active open session's
`SettingsRepository`, with no compatibility-service fallback. It preserves
key `classes_navigation_evaluation_default_policy`, trimmed/lowercased
`current_or_previous_term`, canonical values, the All fallback, missing-key
materialization, no rewrite of valid unknown values, and silent save failures.
Independent focused CTest
`ClassMngrNextPlatformApplicationServicesEvaluationDefaultPolicyPortTests`
passed 1/1. Coverage includes valid unsupported values remaining unchanged,
closed-session no-fallback with `DataService` present, silent trigger-based
write failure, and silent read failure with a best-effort default save. No full
suite ran.

F204 source commit `3cf2ab80` migrates
`ApplicationServicesClassDayFilterResetPolicyPort` to the active open
session's `SettingsRepository`, with no compatibility-service fallback. It
preserves key `classes_navigation_day_filter_reset_policy`,
trimmed/lowercased `on_page_leave`, OnApplicationClose fallback and missing-key
materialization, no rewrite of valid unknown values, and silent save failures.
Independent focused CTest
`ClassMngrNextPlatformApplicationServicesClassDayFilterResetPolicyPortTests`
passed 1/1. Coverage includes valid unsupported values unchanged,
closed-session no-fallback with `DataService` present, silent trigger-based
write failure, and silent read failure. No full suite ran.

F205 source commit `bf9ca8a7` migrates
`ApplicationServicesClassSelectionResetPolicyPort` to the active session's
`SettingsRepository`. It preserves the exact key and canonical values,
trimmed/lowercase `on_page_leave`, the OnApplicationClose default, best-effort
materialization for missing/invalid/read-error values, no rewrite of valid
unknown values, and silent save failures. Focused CTest
`ClassMngrNextPlatformApplicationServicesClassSelectionResetPolicyPortTests`
passed 1/1. The two filtered Classes Page lifecycle slots also passed after
their fixtures were updated to open temporary databases; this confirms
page-leave clears class state while application-close retains it, with the
day-filter policy interaction preserved. No full suite ran.

F206 is selected for `ApplicationServicesCustomColorPalettePreferencesPort`.
Move persistence to the active open session's `SettingsRepository`; do not
fall back through `DataService` when closed. Preserve key `custom_colors`,
the fixed 16-color palette, defaults, QColor canonicalization, compact JSON
writes, and reads of legacy QStringList, JSON, newline, semicolon, and comma
payloads. Missing/unavailable settings return defaults without writes;
repository read failures warn and return defaults, and write failures warn
while preserving the previous value. Keep callers and the typed interface
unchanged. Add closed-session no-fallback with `DataService` present and
repository read-error coverage. Focused CTest:
`NextPlatformApplicationServicesCustomColorPalettePreferencesPort`. Both
independent Explorer lanes selected this bounded adapter migration. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

## Current Phase 2 position - 2026-10-02 (F232 accepted; F233 selected)

F218-F221 are accepted in commits `9bb936ee`, `57aefadf`, `4a87ab1e`, and
`ec65c0c6`. F222 source commit `6920e019` migrates Schedule Import resolution
choice reads to the F221 typed snapshot, preserving option order, labels,
defaults, room matching, and suggested/exact/supplemental class targets.
Independent focused app snapshot, Platform snapshot, and repository/apply
tests passed. The dialog target reported 24 passed and 3 failures; the only
failures were the documented baseline cases
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. `git diff --check` passed;
no full suite ran.

F223 source commit `4b46adc7` builds Schedule Import review matching from the
Qt-free Application projection and F221 typed state snapshot. It removes the
remaining `ScheduleService::previewImport()` read from review preparation and
adds class room number at the Platform snapshot boundary. Independent focused
verification passed the matching-projection, Platform snapshot-port, and
Schedule Import repository targets. The dialog target reported 27 passed and
the three documented baseline failures:
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. The duplicate-target warning
test verifies both initial class targets are 44 before the warning. No full
suite ran; `git diff --check` passed.

F224 source commit `3c45c74b` removes the ScheduleService availability probe
from review preparation and relies on the typed snapshot outcome. Independent
focused verification passed the Application matching-projection, Platform
snapshot-port, and Schedule Import repository targets; the dialog target
reported 28 passed with only the three documented baseline failures. The
closed-session test verifies the typed warning, no controls, and no legacy
preview call. The apply-time service write path remains unchanged; no full
suite ran and `git diff --check` passed.

F225 source commit `7d842339` adds a Qt-free Schedule Import apply request,
summary, use case, and typed write port, and routes confirmed dialog applies
through them. The Platform adapter maps the request to the existing plan and
retains `ScheduleService::importSchedule()` dispatch. A boundary check rejects
Reuse/UpdateRoom without a typed teacher ID and Create/Skip with one before
calling the port; current teacher existence and freshness remain repository
checks. The independent Application and Platform apply tests passed, as did
the review-decision, plan-eligibility, and Schedule Import repository targets.
The dialog target reported 29 passed and only the three documented baseline
failures: `acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. The independent recheck of
the target-pairing correction passed the Application apply test 1/1. `git diff
--check` passed; no full suite ran.

F226 source commit `6c6210b0` adds a Qt-free review-readiness use case that
returns typed decision issues, state-evaluation status, and an optional state
error. The dialog calls it once; it no longer calls either validator directly.
The same typed input fields and snapshot read cadence are preserved, as are
decision-conflict handling and the lower priority of state-error messages.
Independent readiness, review-decision, state-validation, and Schedule Import
repository targets passed 4/4. The dialog reported 29 passed with only the
three documented baseline failures. `git diff --check` passed; no full suite
ran.

F227 source commit `cb61f2ab` moves the proposed Schedule Import action counts
into a Qt-free Application projection consuming the existing typed review
decisions, ignored-diagnostic count, and UI-computed schedules-cleared count.
It preserves the legacy count rules, leaves localized text in the dialog, and
keeps the actual apply result separate. Projection and repository CTests
passed 2/2; the offscreen dialog run reported 30 passed and only the three
documented baseline failures. `git diff --check` passed; no full suite ran.

F228 source commit `d90f475f` moves the existing-schedules-cleared count into a
Qt-free Application projection over the typed state snapshot and review
decisions. It preserves the dialog's full-parse numeric ID matching, counts
classes with selected schedule hours that have no selected target, and returns
zero when an intensive schedule is preserved or no snapshot is available. The
dialog keeps localized summary formatting and snapshot cadence unchanged. The
new Application projection CTest passed 1/1; the dialog run reported 30 passed
and only the three documented baseline failures. A new assertion verifies the
zero count after snapshot refresh failure. Source ownership validation passed
for 1,170 handwritten files and `git diff --check` passed; no full suite ran.

F229 source commit `e8a2a5b5` reuses `projectScheduleImportStateSchedules()` to
select the Schedule Import review-preview schedules. The dialog retains its
control order, snapshot-backed Skip rows, UI enrichment, translated conflicts,
snapshot-failure fallback, and snapshot cadence; the Application projection
supplies class membership and intensive-preservation rows. Focused
state-validation and Schedule Import repository CTests passed. The Dialog
target reported 32 passed and only the three documented baselines:
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. Both new preview parity slots
passed. Source ownership validation found one owner for 1,170 handwritten
files and `git diff --check` passed; no full suite ran.

F230 source commit `547ccb5b` changes the review dialog to build and pass the
typed `ScheduleImportApplyRequest` directly to the Apply use case. It removes
the UI/Application legacy-plan conversion and keeps typed-to-legacy conversion
at the persistence adapter. Candidate and resolution mappings, typed IDs,
confirmation timing, and detailed policy-error text are covered by parity
checks. The Application Apply use case, Platform apply port, and Schedule
Import repository CTests passed. The Dialog target reported 34 passed and
only the three documented baselines:
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. All three new F230 dialog
slots passed individually; `git diff --check` passed. No full suite ran.

F231 source commit `6ebf6d33` derives `ScheduleImportReviewDecisionRequest`
from the typed `ScheduleImportApplyRequest` through one Qt-free Application
projection, shared by ApplyUseCase and dialog readiness/summary. The projection
preserves candidate and resolution ordering, UTF-16/UTF-8 conversion, trimmed
and empty-room handling, actions, and typed class targets. Five focused
Application CTests passed 1/1 each. The Dialog target reported 34 passed and
only the three documented baseline failures:
`acceptedReviewCanTearDownSourceDialog`, `mismatchedProfileRequiresConfirmation`,
and `reviewPreviewUsesSavedScheduleDisplaySettings`; the modified summary and
skip-cascade check passed. Source ownership found one owner for 1,172 files,
and `git diff --check` passed. No full suite ran.

F232 source commit `075b4335` routes the typed v2 apply port directly to the
active session repository. Its repository-bound typed adapter reuses the
existing plan-backed transaction core, preserving repository-time validation,
rollback, errors, and summary behavior while keeping the legacy service and
plan API for v1 callers. The Application ApplyUseCase, Platform ApplyPort, and
Schedule Import repository CTests passed 3/3. The focused dialog confirmation
slot passed. Typed success/persisted-summary, closed-session, legacy-route,
stale/overlap rejection, write-failure rollback, and intensive mapping
coverage passed. Source ownership found one owner for 1,172 files, and
`git diff --check` passed; no full suite ran.

## Current Phase 2 position - 2026-10-02 (F252 accepted; F253 selected)

F236 source commit `5877bba0` carries structured typed policy, teacher-target,
and fresh-state failures from Repository through Platform to the review dialog.
Fresh-state codes and context are preserved, including overlap start/end times;
the dialog uses its existing formatter and stays visible after failure. The
legacy `apply(plan)` API retains its localized text result, while SQL and
transaction failures remain message-only. Independent x64 Debug verification
built 330 actions and validated one owner for 1,174 handwritten sources.
Application and Platform CTests passed 2/2; 11 focused repository functions
and 4 dialog functions passed. The dialog test directly checks visibility after
failure. `git diff --check` passed; no full suite ran.

F237 source commit `8ef76074` adds Qt-free Speaking Evaluation content
normalization and validation used by the save use case and page feedback. It
preserves field locations, severity, focus and confirmation behavior, normalizes
before persistence, and suppresses invalid writes. A differential case checks
questionable Korean-name behavior against `SpeakingEvalValidator` for both
flag settings; declining confirmation leaves persisted data unchanged.
Independent x64 Debug verification completed 323 build steps and validated one
owner for 1,177 handwritten sources. Application, Platform SavePort, and page
save tests passed 9/9, 10/10, and 14/14; `git diff --check` passed. No full
suite ran.

F238 source commit `7b832bcc` moves initial-setup profile replacement into a
Qt-free Application lifecycle contract with a Platform file/workspace adapter.
The original profile remains recoverable through create and restore failures;
FileController retains warning and recent-file behavior. Independent x64 Debug
verification validated one owner for 1,181 handwritten sources. Three focused
CTest entries passed; the Application runner passed all 11 scenarios, Platform
tests passed 7/7, and FileController tests passed 37/37. `git diff --check`
passed; no full suite ran.

F239 source commit `d3e9cded` moves roster row-reordering rules into a Qt-free
contract over `RosterSnapshot`; `RosterModel` adapts typed failures to its
existing messages and preserves validation refresh, notifications, and dirty
behavior. The independent x64 Debug build validated one owner for 1,183
handwritten sources. The Application contract, RosterModel, and roster editor
widget CTests passed 3/3. Coverage includes both move directions, invalid and
equal indexes, blank and Unicode whitespace-only rows, complete cell/metadata
preservation, selection/current-column behavior, and autosave. `git diff
--check` passed; no full suite ran.

F240 source commit `4be18aa5` extracts roster row removal and compaction into a
Qt-free operation over `RosterSnapshot`; `RosterModel` adapts typed failures to
existing messages and retains validation refresh, notifications, and dirty
behavior. Independent fresh x64 Debug verification built the app-less removal,
RosterModel, and widget save targets; focused CTest passed 3/3. Ownership found
one owner for 1,185 handwritten files, and `git diff --check` plus new-file
whitespace checks passed. Coverage includes full-row and metadata preservation,
invalid and empty/Unicode-whitespace-only rows, final-slot clearing, validation
and signals, and widget confirmation/selection/autosave. No full suite ran.

F241 source commit `746ef0bb` extracts custom-column name admission into a
Qt-free Application policy. It preserves `QString::simplified()` whitespace,
the Autumn-to-Fall alias, Qt's case-insensitive duplicate/required checks, and
existing messages. `RosterModel` keeps insertion, notifications, row extension,
validation refresh, and dirty state. A shared Qt adapter copies UTF-16 code
units exactly; leading U+FEFF content survives the F239/F240 row policies.
Independent fresh x64 Debug verification built four focused targets; CTest
passed 4/4. Ownership found one owner for 1,188 handwritten files, and
`git diff --check` plus new-file whitespace checks passed. No full suite ran.

F242 source commit `5525cade` extracts custom-column removal eligibility into
a Qt-free Application policy. It preserves invalid-index and required-column
rejection, the Autumn-to-Fall alias, Qt comparison, and existing messages.
`RosterModel` retains mutation, notifications, validation refresh, dirty state,
width/layout work, destructive confirmation, and autosave. Fresh x64
Debug/Ninja/MSVC 19.51/Qt 6.12 verification validated one owner for 1,190
handwritten sources; five focused CTests passed. Diff and new-file whitespace
checks passed. No full suite ran.

F243 source commit `68260142` extracts Qt-free custom-column append over roster
columns and rows, reusing F241 admission. It preserves existing cells and width
metadata while the model and widget retain their current UI effects. Independent
fresh x64 Debug verification validated one owner for 1,192 handwritten sources;
five focused CTests passed and new-file whitespace checks were clean. No full
suite ran.

F244 source commit `86fe782f` extracts destination-side roster transfer
preparation into Qt-free Application. It preserves matching and normalization,
empty/full/duplicate rejection order, first-empty-slot selection, and the legacy
pair-key behavior. The model retains mutation and UI effects; the widget retains
class lookup, source removal, width handling, and atomic save. Independent fresh
x64 Debug verification validated one owner for 1,194 handwritten sources; six
focused CTests passed. Diff and new-file whitespace checks passed. No full suite
ran.

F245 source commit `99c95146` moves first-unused Korean-name suffix suggestion
into a Qt-free Application policy shared by RosterModel and SpeakingEvalModel.
The models preserve exact legacy behavior by projecting
`StudentNameUtils::baseKoreanName()` and `koreanNameSuffix()` at the Qt edge; the
policy handles trimmed, case-sensitive English grouping and A-Z selection.
Duplicate-pair grouping and the suffix choice UI remain at their current owners.
The lossless UTF-16 adapter moved from Roster UI to `src/ui/shared` to remove a
cross-feature dependency. Independent fresh x64 Debug/Ninja/MSVC 19.51/Qt 6.12
verification validated one owner for 1,196 handwritten files; the app-less
policy, RosterModel, SpeakingEval page-save, and Speaking Evaluation save-use-
case CTests passed 4/4. Differential checks cover U+3000, unpaired UTF-16,
malformed suffixes, and the page choice/apply flow. `git diff --check` and
new-file whitespace checks passed; no full suite ran.

F246 source commit `b1a86db2` extracts same-grade roster transfer-target
eligibility into a Qt-free Application policy. It rejects invalid/current
class IDs, requires a nonempty current grade, and preserves trimmed,
case-sensitive grade equality. The widget retains class/roster lookups and
failures, labels, sorting, fullness, menu actions, and transfer/save behavior.
Independent fresh x64 Debug/Ninja/MSVC 19.51/Qt 6.12 verification validated
one owner for 1,198 handwritten sources; the app-less eligibility,
RosterEditorWidgetSave, and TestingClassesPage focused CTests passed 3/3.
`git diff --check` passed. The UI tests did not reach nonempty-grade target
enumeration or assert `classInfo` lookup counts; policy decisions are covered
directly. No full suite ran.

F247 source commit `7021e657` extracts roster-name import planning into a
Qt-free Application policy. It preserves Qt-compatible trimming, complete-pair
filtering, the legacy U+001F pair key and delimiter collisions, existing and
imported duplicate filtering, source order, and blank-row assignment order.
The page retains case-insensitive first-header lookup, per-cell editability,
unchanged-cell checks, applyChanges, and UI messages. Fresh x64 Debug/Ninja/
MSVC 19.51/Qt 6.12 verification found one owner for 1,200 handwritten sources;
the policy and SpeakingEvalPageSave focused CTests passed 2/2. The page test
checks first case-insensitive header selection, applied values, dirty state,
and success/already-up-to-date messages. `git diff --check` passed; no full
suite ran. Coverage uses the shared Qt-compatible whitespace helper without
exhaustively comparing all Unicode whitespace, and the current model does not
allow a partial-editability test for its two name cells.

F248 source commit `8886b46f` is accepted. It extracts interactive duplicate-
peer row lookup into Qt-free Application shared by RosterModel and
SpeakingEvalModel, reusing F247's trimmed UTF-16 U+001F key and preserving
incomplete pairs, exact case-sensitive matching and delimiter collisions,
selected-row exclusion, and candidate order. Column selection, translated
prompts/actions, and duplicate resolution remain at their model/page owners.
Independent fresh x64 Debug verification validated one owner for 1,202
handwritten files; the new app-less lookup, RosterModel, and SpeakingEval
page-save CTests passed 3/3. `git diff --check` passed. No full suite ran. The
adapter test does not cover unpaired surrogates or exhaustively compare Qt
whitespace; RosterModel's public setup always supplies base name columns, so
the missing-column guard cannot be reached through that route.

F249 source commit `828d5014` extracts Speaking Evaluation AI-batch student
eligibility into a Qt-free Application policy. It preserves first-failure
order: missing name, unsupported grade, missing Did Well observations, then
missing Needs Improvement observations. The dialog retains translated reasons,
row enablement/check state, review, prompts, comment application, and overwrite
confirmation. Fresh Windows x64 Debug verification validated one owner for
1,204 handwritten sources; the app-less eligibility CTest passed 1/1, and the
two relevant dialog functions passed. `git diff --check` and new-file hygiene
passed; no full suite ran. Two unrelated clipboard functions reproduced
`OleSetClipboard`/`OpenClipboard Failed` in the fresh environment and are not
proven against a pre-F249 baseline.

F250 source commit `4753ce3b` extracts a shared Qt-free roster-row availability
query. It treats a row as occupied when any cell remains after Qt-compatible
trimming and returns the first empty row or `rows.size()`. RosterModel projects
its QString rows through the UTF-16 adapter and returns `-1` for a full 25-row
roster. Transfer preparation reuses the same occupancy and first-empty logic
while preserving source-empty/full/duplicate rejection order and destination.
Independent fresh x64 Debug verification validated one owner for 1,206
handwritten sources; the app-less query, RosterModel, and transfer-preparation
CTests passed 3/3. `git diff --check` and new-file hygiene passed; no full suite
ran.

F251 source commit `ed548438` extracts AI batch per-comment review quality
from `SpeakingEvalAiBatchDialog::updateReviewRow()`. Fresh independent x64
Debug verification in `build/f251_independent_x64_debug` validated one owner
for 1,208 handwritten files; the app-less quality test and three focused dialog
functions passed. The 420 preferred threshold is shared by the prompt builder
and dialog through `SpeakingEval::CommentPreferredMaxLength`; rendered prompt
wording is unchanged. The hard maximum is passed from the existing domain
constant. `git diff --check` and new-file hygiene passed. No full batch CTest
or suite ran.

F252 source commit `6f41f8a2` extracts accepted AI batch comment planning
from `SpeakingEvalAiBatchDialog::applyComments()` into Qt-free Application.
Fresh independent x64 Debug/Ninja/MSVC 19.51/CMake 4.4.2/Qt 6.12 verification
in `build/f252_independent_x64_debug` validated one owner for 1,210 handwritten
sources. The app-less planner CTest passed 1/1; dialog slots
`aiBatchDialogConfirmsAcceptedCommentOverwrites` and
`aiBatchDialogSelectsEligibleStudentsAndReviewsValidComments` passed. Coverage
checks selected/valid and report-index filtering, unchanged comments, overwrite
count, input order, UTF-16 fidelity, and confirmation reject/accept. Diff and
new-file hygiene passed; no full suite ran.

F253 is selected to extract the identical private-notes section splitter
shared by `SpeakingEvalAiBatchDialog` and `SpeakingEvalPrivateNotesEditor` into
Qt-free Application. Preserve the `[Did Well]\n` prefix, first
`\n[Needs Improvement]\n` separator, exact body whitespace/newlines, and the
legacy fallback of returning all notes as Did Well when either marker is
missing. Keep serialization, bullet-list editing/normalization, prompts, and
observation parsing with their existing owners. Paired scans differed: the
alternative was AI-batch response parsing. Choose the shared exact duplicate
as a narrower reusable boundary; defer response-parser extraction. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

## Archived Phase 2 position — 2026-10-07 (F382 accepted)

Deployment `phase2_resume_20260929` continues in Phase 2 on the Heavy route.
Batch 12 is complete at F381. F382 is accepted as the first completed slice
from the discovered Batch 13. The user requested stopping after its commit;
F383-F386 remain provisional and unselected. Phase 2 remains In Progress/Open,
with Gates 1 and 2 Partial.

The user reports F367-F371 were completed on another device, but they remain
unsynced and unaudited in this checkout; local Batch 11 evidence still ends at
F366. No remote commits or acceptance results are inferred.


## Current Position — 2026-10-08 (F402 current; Batch 18 discovery underway)

Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial. F386-F393
are accepted as recorded in the Phase 2 progress log. The seven Teacher Profile
Edit MSVC diagnostics were fixed in commit `0b128601`; the named target and
standard all-target build passed earlier, and the exact affected persistence
target rebuilt successfully with no reported errors or LNK4006 warnings.

F394 Classes landing open-session confirmation parity, F395 Campus Dashboard
typed save boundary, F396 Staff Directory open-session dirty-exit parity, and
F397 MainWindow Document Catalog retranslation are accepted. F398 same-path
workspace-open parity is accepted with focused CTest 3/3. F399 Close File
no-workspace parity is accepted and committed as `f21db8c3`; its real-window
test passed 1/1 in the focused Ninja/MSVC/Qt 6.12 fallback tree.

F400 Open File dirty-page gate parity is accepted with no production changes.
The real QAction test verifies page Cancel preserves the dirty draft and skips
the chooser; Discard reaches the chooser, and chooser cancellation preserves
the same session with the saved profile restored. The focused Ninja/MSVC/Qt
6.12 target built and filtered CTest passed 1/1. Configure reported optional
missing WrapVulkanHeaders and nonfatal object-path-length warnings; no LNK4006.
The standard Visual Studio tree remains blocked before source compilation in
ZERO_CHECK by the current shell's FileTracker CommonApplicationData path
resolution; no system settings changed. No full suite ran.

The seven Teacher Profile Edit MSVC diagnostics were rechecked against the
current source: the focused x64 MSVC/Qt 6.12 Ninja target built and its filtered
CTest passed 1/1. The diagnostics refer to pre-fix code already corrected in
commit `0b128601`; no source changes were needed for this report.

F401 Recent-workspace menu selection and missing-path pruning is accepted
and committed as `2121ce59`, with no production changes. Two new
MainWindow tests use the production Recent menu to open a different existing
workspace without a chooser and to prune a missing path while verifying the
warning, history/menu updates, and exact active session/page preservation.
The focused Ninja/MSVC/Qt 6.12 target built and filtered CTest passed 1/1
(1.02 s); incremental build with no warnings. No full suite ran; the standard
Visual Studio tree was not used for this slice.

F402 MainWindow application-exit confirmation is committed as 9cbe39c9,
with no production changes. A real visible MainWindow test exercises
QWidget::close() with a dirty Details draft and open workspace. Cancel rejects
close while preserving the window, exact session/path, page, tab, Sidebar
selection, draft, and dirty state. A second close with Discard is accepted and
restores the persisted draft. The focused Ninja/MSVC/Qt 6.12 target built and
filtered CTest passed 1/1 (0.34 s); CMake reconfigured and emitted optional
WrapVulkanHeaders and known object-path-length warnings, with no LNK4006.
No full suite ran. The standard Visual Studio tree remains blocked before
source compilation by the existing FileTracker/CommonApplicationData issue.

F403 Dynamic Teacher Sidebar leaf navigation is accepted and committed as
28e881cd, with no production changes. A real MainWindow test opens a seeded
workspace, uses the production refresh path, and clicks leaves in Co-Teachers
and Korean Teachers. For each group, it verifies target teacher ID and stable
route keys; Cancel preserves the dirty source page and exact draft, and Discard
loads the target with a clean page while leaving source persistence unchanged.
The focused Ninja/MSVC/Qt 6.12 target built and CTest passed 1/1 (0.52 s); an
independent filtered CTest also passed 1/1. No full suite ran.

F403 is committed as 28e881cd, completing Batch 17. F404 Save choice on
Open/Close File actions is accepted and ready to commit, with test-only changes.
The Open File test scripts Save then chooser cancellation and verifies one prompt
and chooser request, the same workspace/session/path/page/tab/Sidebar, visible
draft with clean page/workspace, and persisted myInfo/name. The Close File test
scripts Save and verifies one prompt/no chooser, closed session/path, Campus
Dashboard Information route, disabled database actions, and persistence after
reopening the workspace. Both focused Ninja/MSVC/Qt 6.12 targets built under the
VS 18 x64 developer environment, and the filtered CTest passed 2/2. No full
suite ran.

F405 MainWindow Save As and Export action integration is committed as
07dc5864. Its target built under VS 18 x64 with Ninja/MSVC; CTest simple-name
filter passed 1/1. Batch 18 remains active with F406 Manage Campuses QAction
transition selected/current; F407-F413 remain queued.

F406 is accepted and ready to commit. Cancel preserves page/tab, dirty draft,
Sidebar, session/path, and blocks the transition. Discard reaches Campus Dashboard
Information with matching Sidebar selection, restores the persisted draft cleanly,
and preserves session/path/actions. MainWindow now calls
`CampusDashboardPage::showInformation()` after showing the reused Dashboard,
closing the Sidebar/page mismatch. The focused `ClassMngrMainWindowManageCampusesParityTests`
target built under VS 18 x64 with Ninja/MSVC; simple-name filtered CTest passed
1/1 with both Cancel/Discard cases. F407 is queued after the F406 commit;
F408-F413 follow.

F406 is committed as 28b27998, completing the accepted Manage Campuses
transition.

F407 Schedule↔Testing Classes handoff is implementation complete and accepted,
ready to commit. Real Testing Classes/back actions cover standalone Schedule and
My Workspace → Schedule, returning to the correct source page/tab and preserving
active session/path/Sidebar. The cell-dialog Manage Classes producer forwards the
empty choice list’s natural non-positive ID plus a specific day/time; class
creation persists the requested slot. Cancel preserves dirty Testing Classes
state; Discard completes the source-dependent return. The cell-dialog route now uses `QTest::mouseClick` on the rendered table
viewport, covering the actual `QTableWidget` `cellClicked` connection and
downstream handoff. Focused target
`ClassMngrMainWindowScheduleTestingClassesHandoffParityTests` built with
Ninja/MSVC; CTest passed 1/1 (1.54 s) on 2026-10-08. F408 is queued after the
F407 commit;
F409-F413 follow.

The user-modified latest_session_work.md remains untouched.

F407 is committed as `f639fbd3`, with the MainWindow Schedule ↔ Testing Classes
handoff and focused `ClassMngrMainWindowScheduleTestingClassesHandoffParityTests`
coverage (Ninja/MSVC build, filtered CTest 1/1).

F408 Dynamic Teacher Sidebar selection/state after retranslation is
implemented and accepted. `Sidebar::selectByKeys()` now restores the exact
duplicate teacher occurrence from the saved stable key path plus teacher ID,
with the prior ID-based behavior as fallback when that occurrence is gone.
The MainWindow integration test switches English↔Korean from both Co-Teachers
and Campus Staff → Korean Teachers and verifies selection path/ID, expansion,
no route event, Teacher Info page and identity, dirty manual-save draft without
prompts, delete-action state, and active database session/path. Dynamic teacher
display remains stable while group and internet-type labels translate; the
localized combo assertion compares its stable data separately from its display
text. `ClassMngrMainWindowTeacherSidebarNavigationParityTests` built under VS
18 x64 and filtered CTest passed 1/1 (1.21 s). F409-F413 remain queued in
Batch 18; the next-batch discovery trigger is F412.

F408 is committed as `9f92b78d`. F409 My Workspace Sidebar root
producer-to-handler integration is implemented and accepted. The focused test
clicks the rendered root, passively verifies its actual `NavigationData`
payload, then asserts the My Workspace page and instance, Schedule tab, root
selection, and unchanged open database session/path. This closes a coverage
gap: prior tests selected the root programmatically or fabricated the payload
and called the controller directly. `ClassMngrMainWindowMyWorkspaceSidebarRootNavigationTests`
built under VS 18 x64; filtered CTest passed 1/1 (0.37 s). No runtime defect
was established; a possible dirty-cancel selection mismatch remains a separate
source inference. F409 is committed as `ee319df2`.

F410 Classes Sidebar root integration is implemented and accepted. The new
MainWindow test clicks the rendered root, passively verifies the actual `Page`
payload, displayed label, stable `classes` key/route, and natural class ID -1,
then asserts the Classes page, selected root, and unchanged open database
session/path. This closes the gap left by direct slot invocation and synthetic
controller-route tests. Initial CTest exposed a production teardown lifetime
defect: QObject-owned pages retained non-owning service pointers but were
destroyed after MainWindow's `ApplicationServices` member. The destructor now
deletes `m_pages` while services remain alive. `ClassMngrMainWindowClassesSidebarRootNavigationTests`
built under VS 18 x64; independent filtered CTest passed 1/1 (0.32 s) after the
fix. F410 is committed as `14723973`. F411 Sub Prep Sidebar root integration
is implemented and accepted. The new MainWindow test clicks the rendered root,
verifies the actual `Page` payload, displayed label, stable `sub_prep` key/route
and class ID -1, then asserts the Sub Prep page at Important Information, root
selection, and unchanged open database session/path. Closed-session and
dirty-page cases remain covered by the route-gate test. The focused target
rebuilt under VS 18 x64; independent CTest passed 1/1 (0.41 s), and the direct
executable exited normally. F412-F413 remain queued in Batch 18; the next-batch
discovery trigger is F412. F411 is committed as `3e5dbea4`. F412 Campus Sidebar
root plus section producer integration is selected/current. Batch 19 discovery
is complete: eight provisional candidates were reconciled from two independent
bounded reviews before F412 implementation. The only preserved unrelated
worktree entries are the user-modified `latest_session_work.md` and untracked
`%SystemDrive%/` artifact.

F412 Campus Sidebar root and section producer-to-handler integration is
implemented and independently accepted. Its real MainWindow test clicks the
rendered root and all five sections, asserts each actual `NavigationData`
payload, destination page/section and Sidebar path, and unchanged database
session/path. The VS 18 x64 Debug target rebuilt; independent filtered CTest
passed 1/1 (0.42 s), with no lingering process. No production change or runtime
defect was needed. F412 is ready to commit; F413 Initial Setup success
navigation remains queued in active Batch 18.

F412 is committed as `76661791`. F413 Initial Setup success navigation from
the empty-state button is selected/current; acceptance discovery is underway.
Batch 18 remains active, and the already completed Batch 19 discovery remains
provisional until F413 completes.

F413 Initial Setup success navigation from the empty-state button is
implemented and independently accepted. The real MainWindow test QTest-clicks
the visible no-database button from Campus Dashboard Information, verifies the
BasePage/PageManager request signals and accepted InitialSetupWizard, then
checks the created profile/session, My Workspace Schedule, `my_workspace`
Sidebar selection, and hidden empty-state banner. The VS 18 x64 Debug target
rebuilt; independent filtered CTest passed 1/1 (0.46 s), with no lingering
process. No production change or runtime defect was needed. F413 is ready to
commit; Batch 18 is complete after its commit and Batch 19 candidates are ready
to activate. F413 is committed as `5d8a941a`, completing Batch 18. Batch 19 is
now active with F414 Teacher profile save preserving the selected duplicate
Sidebar occurrence selected/current; F415-F421 follow in discovered order.

F414's MainWindow regression reproduced the occurrence shift before the fix:
saving from Campus Staff → Korean
Teachers selected the Co-Teachers occurrence. The save handler now restores
the selected teacher key path after rebuilding the Sidebar. The valid manual
Save flow verifies both updated leaves, persisted profile, clean current page,
and stable database session/path. Independent VS 18 x64 Debug target build
passed; the exact filtered CTest passed 1/1 (1.52 s), and direct QtTest passed
5/5. No LNK4006 warnings occurred. F414 is committed as `6fb39b2b`. Batch 19
continues with F415 Campus Dashboard page-tab-to-Sidebar synchronization
selected/current, accepted and ready to commit. F415 is committed as
`f4bc5282`. F416 Document Catalog rendered Sidebar leaf through MainWindow and
viewer is selected/current; bounded source discovery is underway. F417-F421
remain queued.

F415 uses the existing MainWindow/open-session fixture to QTest-click all five
Campus Dashboard tabs without a test-side signal connection. Each click emits
the expected section key, synchronizes the Sidebar path, preserves the current
page and database session/path, and emits no extra Sidebar route event. The
focused VS 18 x64 Debug target rebuilt; independent filtered CTest passed 1/1
(0.46 s). No LNK4006 warnings occurred, and no production change was needed.

F416 extends the real MainWindow retranslation test with a rendered
`document_guides_lesson_planning` Sidebar leaf click through the production
route connection. It confirms the PDF viewer was uninstantiated before the
click, one expected route and selected key path, a Ready content session with
the expected resource reference/path and print/save capabilities, and no
navigation-time modal or Qt warning. No production change was needed. The
focused target built under VS 18 x64 Debug; executor and independent CTest runs
passed 1/1. The executor build reported `LNK4075` (`/INCREMENTAL` ignored due
to `/FORCE`); the independent build did not reproduce it. No full suite ran.
F416 was accepted and committed in its slice. Work paused after F416 at
user request, then resumed through F429. Batch 20 completed with F428. F426 is
committed as 249d57b11ce327923a6416d72e53e034b53cda3c; F427 as
bbdf10e85ba2c5a61066ed6fd32a7c8993c0f04e; F428 as
b82bddaa59c57d88f773370e94b3163df9055f33; and F429 as
19f6024d113dda41e7e7a2b41acf27961160462b. Batch 21 is active with F430
Schedule Print QAction selected/current. Its acceptance matrix is recorded in
the Phase 2 progress log: trigger the real MainWindow action, inspect and
cancel the Print-mode Schedule dialog before native printer UI, then verify
workspace state remains stable. Executor and independent focused
verification passed. F430 is committed as aa7fca37. F431 Import Teachers
through MainWindow, including the page-leave gate, is selected/current. Its
Cancel and Discard cases are accepted with focused verification. F431 is committed as 2ac08388. F432 Export Classes QAction through its selection dialog and JSON picker is selected/current; the QAction-to-picker-cancellation case passed focused verification. F432 is committed as 4bfe3dc2. F433 is committed as b60c8025. F434 Delete Teacher QAction confirmation through MainWindow is accepted in this changeset and ready to commit; F435-F438 remain provisional.


## 2026-10-09 - F434 acceptance matrix recorded

F434 targets the real MainWindow Delete Teacher QAction in tests/mainwindow_teacher_sidebar_navigation_parity_tests.cpp. Seed a target and a survivor, leave MainWindow on MyWorkspace without a teacher selected, and trigger window.actions().deleteTeacher. A bounded timer automates the real sidebarRecordSelectionDialog by choosing the target ID and accepting; the fake user prompt then captures the destructive Delete Teacher confirmation and returns Cancel. Assert the requested teacher/display-name text, destructive flag, and Delete/Cancel choices; target and survivor remain persisted and represented in the sidebar; Delete Teacher remains enabled; and the page, central widget, sidebar selection, database session, and path stay stable with no extra prompt. This is QAction-to-confirmation cancellation characterization; it does not claim successful deletion. Existing direct-controller coverage already exercises the lower-level rejection and chooser paths. Focused target: ClassMngrMainWindowTeacherSidebarNavigationParityTests; CTest: ClassMngrMainWindowTeacherSidebarNavigationParityTests. Matrix recorded before implementation; no implementation yet.


## 2026-10-09 - F434 QAction cancellation accepted

The real MainWindow Delete Teacher QAction opened the teacher chooser, selected the seeded target, and presented the destructive confirmation. FakeUserPromptService rejected it. The target and survivor snapshots and sidebar entries remained unchanged; the Delete Teacher action stayed enabled; MyWorkspace page, widget, sidebar selection, database session, and path remained stable, with no additional prompt. The case claims cancellation only, not successful deletion. Executor and independent Tester each passed focused target build, exact Debug CTest 1/1, direct QtTest 6/6, selected case 3/3. A five-second watchdog rejects an active modal if chooser acceptance does not happen. Executor used Ninja Debug after VS FileTracker access-denied errors; independent VS target build passed without retry. No full suite or production/CMake change. F434 is committed as 3fb5241347d14c2d9dfa94b5a25a6742d6ba28df; branch is ahead by 19.


## 2026-10-09 - F434 committed; F435 selected

F434 committed as 3fb5241347d14c2d9dfa94b5a25a6742d6ba28df (Phase2 - Cover Delete Teacher QAction cancellation (F434)); branch is ahead by 19. F435 Empty-state Open/New Profile button handoff through Banner, PageManager, and MainWindow is accepted in this changeset and ready to commit. F436 Invalid UTF-8 document resource references is next after commit; F437-F438 remain provisional. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F435 Open/New Profile acceptance matrix recorded

F435 targets tests/mainwindow_initial_setup_empty_state_navigation_tests.cpp. Add separate success cases from the no-database CampusDashboard banner. Open case: seed a valid Teacher Profile in the test temporary directory, script its path via FakeFileDialogService, click noDatabaseOpenButton, and assert exactly one BasePage::openDatabaseRequested, PageManager::openDatabaseRequested, openFile QAction trigger, and TeacherProfile open-picker request with expected filter/parent; then assert the path/session open, MyWorkspace Schedule route and sidebar selection, and banner hidden. New case: choose a unique nonexistent .tps path in the temporary directory, click noDatabaseNewButton, assert the corresponding BasePage/PageManager newDatabaseRequested signals, newFile QAction trigger, and TeacherProfile save-picker metadata; then assert file creation, open path/session, MyWorkspace Schedule route/sidebar selection, and hidden banner. Both cases assert no initial-setup request, unexpected prompt, or opposite picker request. F413 Initial Setup coverage remains separate; New Profile must not invoke the wizard. Build target and exact CTest: ClassMngrMainWindowInitialSetupEmptyStateNavigationTests. Matrix is recorded before implementation; implementation has not started.


## 2026-10-09 - F435 banner Open/New Profile accepted

Separate real banner button cases verify BasePage → PageManager → MainWindow QAction handoff for Open and New Profile. Open uses a seeded valid temporary profile and checks TeacherProfile open-picker metadata; New uses a unique nonexistent temporary .tps path and checks save-picker metadata and created file. Both assert a live session and expected path, MyWorkspace Schedule/sidebar selection, hidden banner, and no Initial Setup request, prompt, or opposite picker. F413 remains separate; New Profile does not invoke the wizard. Executor and independent Tester passed the focused build, exact Debug CTest 1/1, target QtTest 5/5, and each new case 3/3. Executor Ninja Debug had no compiler warnings; independent VS target build passed (Qt reported missing system font directory; repository fonts loaded). No production/CMake change or full-suite run. F435 is committed as 998565115361bdd301f1d06ecc4beb2da7a519b9; branch is ahead by 20.


## 2026-10-09 - F435 committed; F436 selected

F435 committed as 998565115361bdd301f1d06ecc4beb2da7a519b9 (Phase2 - Cover Empty-state Open/New Profile button handoff (F435)); branch is ahead by 20. F436 Invalid UTF-8 document resource references is accepted in this changeset and ready to commit. F437 is next after commit; F438 remains provisional. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F436 invalid UTF-8 acceptance matrix recorded

F436 will define malformed UTF-8 in raw std::string document references as Domain::ErrorCode::InvalidInput with recoverable=false. Validate and decode strictly in DocumentContentResourcePort::relativePath before path conversion/normalization and before resource-pack acquisition, for both primary and optional export references. Keep NotFound for syntactically valid references whose resource is absent. Add focused tests using raw bytes that cannot be normalized through QString (including invalid continuation and truncated/malformed sequences); assert InvalidInput, nonrecoverable status, and documents pack remains unmounted for primary and export failures. Existing valid resolution, missing-resource NotFound, NUL, scheme, and traversal behavior must remain unchanged. Focused target and exact CTest: ClassMngrNextPlatformDocumentContentResourcePortTests. Matrix was recorded before implementation; implementation and independent verification are complete.


## 2026-10-09 - F436 invalid UTF-8 accepted

The resource port now strictly decodes raw reference path bytes with stateless QStringDecoder before normalization and before acquiring the resource pack. Invalid primary or export UTF-8 returns InvalidInput with recoverable=false. Tests cover six malformed primary sequences and a malformed export sequence, assert the error and no mounted pack at return, and retain valid resolution, valid missing-resource NotFound, NUL, scheme, and traversal coverage. Executor and independent Tester passed target build, exact CTest 1/1, direct target QtTest 9/9, and selected case 3/3. The Ninja Debug build used an initialized VS x64 environment after an initial missing-type_traits failure; final runs had no compiler warnings. No full-suite run. F436 is committed as c19e247f8066b546bfb71e9177e2413123309112; branch is ahead by 21.


## 2026-10-09 - F436 committed; F437 selected

F436 committed as c19e247f8066b546bfb71e9177e2413123309112 (Phase2 - Reject invalid UTF-8 document references (F436)); branch is ahead by 21. F437 Report worker event-post failure with a zero-capacity queue is accepted and ready to commit. F438 Optional typed occurrence IDs during repeat-series creation is selected/current; read-only context discovery is underway. Batch 21 remains active through F438; Batch 22 is discovered but inactive. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F437 report worker event-post failure matrix recorded

F437 adds focused coverage in tests/next_platform_qt_job_worker_tests.cpp using ReportJobCoordinator with a zero-capacity ReportJobEventQueue and an immediately failing report callback. The callback Failed terminal event is the first attempted post and is rejected with Conflict. Assert the worker lastResult carries the post Conflict (not the callback work error), event queue stays empty, worker finishes within the established timeout and is joined, and coordinator snapshot remains Running because pump received no event. Preserve analogous Import worker behavior and existing Report success/progress/failure cases. Build target and exact CTest: ClassMngrNextPlatformQtJobWorkerTests. Batch 22 candidate discovery is underway because F437 is the second-last Batch 21 slice. Matrix was recorded before implementation; implementation and independent verification are complete.


## 2026-10-09 - Batch 22 bounded discovery recorded while F437 is current

Two independent reviews and a reconciliation surfaced eight provisional candidates for Batch 22: accept Delete Teacher confirmation through MainWindow (ClassMngrMainWindowTeacherSidebarNavigationParityTests); complete Export Classes JSON output (ClassMngrMainWindowCloseFileParityTests); apply Import Classes through its QAction with a create-only package (same target); apply Import Teachers through its QAction (ClassMngrMainWindowManageCampusesParityTests); exercise Delete Class through MainWindow QAction (ClassMngrMainWindowScheduleTestingClassesHandoffParityTests); replace an already-open profile through successful Open File QAction selection (ClassMngrMainWindowOpenFileParityTests); trigger MainWindow Save and verify persisted data (ClassMngrMainWindowSaveAsExportParityTests); and choose Save during window close (ClassMngrMainWindowExitConfirmationParityTests). F435 already covers successful Open from the no-database banner, so a generic open-success candidate is redundant. Same-path open remains held because idempotence/replacement semantics are undefined; F285 successful New Teacher remains deferred. F437 is committed; F438 is current in active Batch 21. Batch 22 is discovered but remains inactive until F438 commits. The bounded reviews found eight candidates, not repository-wide exhaustion.


## 2026-10-09 - F437 report worker event-post failure accepted

The added case uses an immediately failing callback and a zero-capacity report queue, so the terminal Failed-event post is the first attempt. The queue returns Conflict, which becomes worker lastResult instead of the callback error. The worker exits within the timeout and is joined; the queue remains empty, pump consumes zero events, and coordinator snapshot remains Running. This mirrors the established Import worker contract without claiming coordinator recovery when no event was delivered. Executor and independent Tester verified the final source in the Ninja Debug tree: target build passed, exact CTest 1/1, target QtTest 12/12, selected case 3/3. The standard VS build tree hit FileTracker access errors; final Ninja build had no warnings. No production/CMake change or full-suite run. F437 is committed as f355a1aa65d63bbf18ddeda3e8cfa7ff76c985d7; branch is ahead by 22.


## 2026-10-09 - F437 committed; F438 selected

F437 committed as f355a1aa65d63bbf18ddeda3e8cfa7ff76c985d7 (Phase2 - Cover Report worker terminal event-post failure (F437)); branch is ahead by 22. F438 Optional typed occurrence IDs during repeat-series creation is selected/current; its acceptance matrix is recorded and implementation is pending. Batch 21 remains active through F438; Batch 22 has eight provisional candidates but remains inactive. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F438 occurrence-ID contract matrix recorded

Define repeat-series creation as create-only. Any occurrence with a present CalendarEventId is invalid: CalendarEventSaveRequest.id selects an update, the repeat-series create request represents new rows, the normal planner clears seed IDs, and create-only calendar import rejects present IDs. Add a request validation rule in src/next/application/calendar_event_series_create_port.h that rejects any occurrence ID as InvalidInput with recoverable=false before persistence. In tests/next_platform_application_services_calendar_event_port_tests.cpp, seed an existing calendar row, build a multi-occurrence create request with that row ID on a later occurrence, and assert request/port validation fails, the existing row fields and repeat_series_id remain unchanged, and row count does not increase. Existing ID-less series creation/generated IDs and persistence remain unchanged. Reusing a standalone row as the first series occurrence is outside this request contract and would need a separate operation. Build target and exact CTest: ClassMngrNextPlatformApplicationServicesCalendarEventPortTests. Matrix recorded before implementation; implementation has not started. Batch 22 remains inactive until F438 commits.


## 2026-10-09 - F438 occurrence-ID rejection accepted

Repeat-series creation is create-only. The request boundary rejects any present occurrence CalendarEventId as nonrecoverable InvalidInput before persistence, preventing an existing standalone row from being updated or a partial series from being inserted. The focused integration case seeds an existing row, supplies its typed ID on a later occurrence, checks request and adapter errors, compares the full row including repeatSeriesId, and verifies row count is unchanged; ID-less generated series creation remains valid. Executor and independent Tester passed the Ninja Debug target build, exact CTest 1/1, direct target QtTest 55/55, and selected case 3/3. No compiler warnings or full-suite run. F438 is accepted and ready to commit. Batch 21 completes with this commit; Batch 22 begins with F439 Accept Delete Teacher confirmation.


## 2026-10-09 - F438 committed; Batch 21 complete

F438 committed as 0c2ceca6136a962f41130f52975bbab7315cd465 (Phase2 - Reject existing IDs in repeat-series creation (F438)); branch is ahead by 23. The commit contains the source, focused test, and three plan docs plus the two main progress records; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits. Batch 21 is complete. Batch 22 is active with F439 Accept Delete Teacher confirmation success selected/current; bounded read-only discovery is underway. F440-F446 remain provisional.


## 2026-10-09 - F439 Delete Teacher success matrix recorded

F439 adds the successful counterpart to F434 in ClassMngrMainWindowTeacherSidebarNavigationParityTests, using the actual MainWindow deleteTeacher QAction with no teacher selected. Seed two unassigned teachers, keep MyWorkspace active, and route the real chooser through the existing QTimer selector plus five-second modal watchdog to choose the target. Script PromptChoice::Destructive and verify one correctly configured “Delete Teacher” confirmation is consumed with no warning/message prompt. Assert the target is absent from the database and teacher sidebar, the survivor remains unchanged, the no-teacher-selection state and MyWorkspace page/widget remain stable, and the open session/path are unchanged. Do not cover assigned-class cleanup or deletion from a selected TeacherInfo page. The direct-controller success test already covers lower-level deletion; F439 specifically covers QAction + chooser integration. Build target and exact CTest: ClassMngrMainWindowTeacherSidebarNavigationParityTests. Matrix recorded before implementation; no implementation has started.


## 2026-10-09 - F439 Delete Teacher QAction success accepted

The new MainWindow case uses the enabled deleteTeacher QAction, selects a target in the actual chooser, and accepts the destructive confirmation. It verifies the confirmation request was consumed with no extra warning/message, the target is absent from repository and sidebar, the unassigned survivor record/sidebar leaf remain unchanged, no teacher row is selected, and MyWorkspace/page/widget/service/session/path remain stable. Executor and independent Tester passed the Ninja Debug target build, exact CTest 1/1, direct target QtTest 7/7, and selected case 3/3. No compiler warnings; offscreen Qt/font notices only. No full-suite run or production/CMake change. F439 is accepted and ready to commit. F440 Complete Export Classes JSON output is next after this commit; Batch 22 remains active.


## 2026-10-09 - F439 committed; F440 selected

F439 committed as a0c50d2dd37de11a4bfa91cfea044822cba7269f (Phase2 - Cover Delete Teacher QAction confirmation success (F439)); branch is ahead by 24. The six-path commit contains the test-only source change and accepted progress records; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits. Batch 22 remains active. F440 Complete Export Classes JSON output is selected/current and bounded read-only discovery is underway; F441-F446 remain provisional.


## 2026-10-09 - F440 Export Classes JSON success matrix recorded

F440 adds a successful companion to F432 in MainWindowCloseFileParityTests. Reuse the real MainWindow exportClasses QAction and ClassExportDialog observer, whose 10 ms poll and five-second watchdog reject a stuck modal; assert dialog observed, selected IDs contain only the target, Export clicked, and neither timeout nor fallback rejection occurred. Seed one teacher and two classes, select only the target, and script a temporary .json save path. Assert the file exists and parses; root format is ClassMngr Classes, version is 1, exported_at_utc parses as UTC, and classes contains only the selected class with its class_grade/class_level and teacher_ref linked to the single exported teacher profile. Do not compare the variable timestamp or treat generated package keys as database IDs. Assert one successful Export Classes information prompt mentions count/path, no warning, and MyWorkspace/session/path and persisted class records remain unchanged. Existing codec tests own exhaustive payload schema coverage; F440 proves the QAction/dialog/output integration. Exact target/CTest: ClassMngrMainWindowCloseFileParityTests. Matrix recorded before implementation; no implementation has started.


## 2026-10-09 - F440 Export Classes JSON success accepted

The new MainWindow case uses the real Export Classes QAction and dialog observer, verifies both seeded classes are listed but only the target is selected, and writes through the scripted temporary JSON picker. Parsed output has format ClassMngr Classes, version 1, a valid UTC timestamp, exactly the selected class with its info, and a teacher_ref linked to the exported teacher; the unselected class is absent. The success information prompt has the expected count/path and no warning. Workspace page/session/path, class names/count/table rows, and full class-info snapshots are unchanged. Executor and independent Tester passed Ninja Debug target build, exact CTest 1/1, target QtTest 9/9, and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full suite or production/CMake change. F440 is accepted and ready to commit. F441 Import Classes QAction apply is next after this commit; Batch 22 remains active.


## 2026-10-09 - F440 committed; F441 selected

F440 committed as 01d1559caae48eddcda739f4ea6f30dba8667e24 (Phase2 - Cover Export Classes JSON output (F440)); branch is ahead by 25. The six-path commit contains the test-only source change and accepted progress records; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits. Batch 22 remains active. F441 Import Classes QAction apply with a create-only fixture is selected/current and bounded read-only discovery is underway; F442-F446 remain provisional.


## 2026-10-09 - F441 Import Classes create-only matrix recorded

F441 adds successful apply coverage beside F426 in MainWindowCloseFileParityTests. Use an empty destination database and a valid one-class JSON package with no teachers and an empty teacher_ref; this avoids teacher import and replacement semantics. Trigger the actual MainWindow importClasses QAction, script one openFile selection for FileDialogPurpose::ClassTransfer/JSON, then automate the real ClassImportDialog with a timer and five-second modal watchdog. Assert the default Create class choice and Import button, dialog acceptance, and no timeout/fallback. Verify exactly one class is created with expected class-info values, zero teachers are imported and the class remains unassigned, with summary Created: 1, replaced: 0, skipped: 0 and no warning. Assert PageType::Classes shows the imported class selected in ClassesSection::Details, sidebar route classes, and database path/session stable. Target/CTest: ClassMngrMainWindowCloseFileParityTests. Matrix recorded before implementation; no implementation has started.


## 2026-10-09 - F441 Import Classes QAction apply accepted

The test drives the real Import Classes QAction, scripted ClassTransfer JSON picker, real ClassImportDialog, explicit Create choice, and Import apply from an empty destination using a one-class package with no teachers. It verifies one new class and full expected ClassInfo, zero teacher rows with teacherId -1, exact Created: 1/replaced: 0/skipped: 0 information summary, no other prompt, Classes page with imported ID/Details active, sidebar route classes, and stable session/path. Executor and independent Tester passed Ninja Debug target build, exact CTest 1/1, target QtTest 10/10, and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full-suite run or production/CMake change. F441 is accepted and ready to commit. F442 Import Teachers QAction apply is next after this commit; Batch 22 remains active.


## 2026-10-09 - F441 committed; F442 selected

F441 committed as 75a559ae7bad175cba106d0377bcf59c506cd247 (Phase2 - Cover Import Classes QAction apply success (F441)); branch is ahead by 26. The six-path commit contains the test-only source change and accepted matrix/results; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits. Batch 22 remains active. F442 Import Teachers QAction apply is selected/current and bounded read-only discovery is underway; F443-F446 remain provisional.


## 2026-10-09 - F442 Import Teachers MainWindow success matrix recorded

Add a successful Import Teachers case to MainWindowManageCampusesParityTests, complementing F431’s page-leave gate. Use an empty temporary database and clean MyWorkspace, plus checked-in tests/fixtures/teacher_import/sectioned_review.xlsx. Browse through the real TeacherImportDialog using one OpenFileRequest for purpose ImportWorkbook and Excel Workbooks (*.xlsx). Allow asynchronous validation with a bounded 15-second watchdog; assert Valid File and Import enabled, then set M1 to Select candidate 0, M2 to None, and H1 to All before clicking Import. This yields 2 Korean teachers (Hong/Park), 1 Native English teacher (Alex), and 1 GS Team member (Taylor) with no prior records/date, avoiding update or old-date confirmation. Assert exact created/updated/unchanged summary counts, new database category records and refreshed Korean sidebar entries, one Import Teachers information prompt and no warning/confirmation; current workspace page, session, and path remain stable. Target/CTest: ClassMngrMainWindowManageCampusesParityTests. Matrix recorded before implementation; no implementation has started.


## 2026-10-09 - F442 Import Teachers MainWindow apply accepted

The new case browses the checked-in XLSX through the real MainWindow QAction and TeacherImportDialog. It waits for async validation, applies M1 candidate 0 / M2 None / H1 All from a clean workspace, and verifies 2 Korean records (Hong/Park), 1 Native English (Alex), and 1 GS Team member (Taylor), all created with no updates/unchanged. It checks category table counts, Korean sidebar IDs/labels, source date, exact success summary, no warning/date confirmation, and stable MyWorkspace/session/path. Executor and independent Tester passed Ninja Debug target build, exact CTest 1/1, target QtTest 7/7, and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full-suite run or production/CMake change. F442 is accepted and ready to commit. The user asked to pause after this commit; F443 Delete Class QAction is next on resume, but no F443 discovery or implementation has started.


## 2026-10-09 - F442 committed; resumed at F443

F442 committed as 8cde28263d3c89cc5f3f610d031a1a536ad183d4 (Phase2 - Cover Import Teachers QAction apply success (F442)); branch is ahead by 27. The six-path commit contains the test-only source change and accepted records; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside commits. The user has resumed after the requested pause. Batch 22 remains active; F443 Delete Class QAction is selected/current and bounded read-only discovery is underway. F444-F446 remain provisional.


## 2026-10-09 - F443 Delete Class MainWindow success matrix recorded

Add a MainWindow integration case to MainWindowScheduleTestingClassesHandoffParityTests for the real deleteClass QAction. Preseed two unassigned classes before opening a clean Classes details page, with the target owning ClassInfo and one schedule row and the sibling retained as fallback. Trigger the QAction, use the real sidebarRecordSelectionDialog to choose the target via the combo/button with a five-second modal watchdog, and script PromptChoice::Destructive for the actual Delete Class confirmation. Assert the exact confirmation is consumed, no warning/unsaved prompt appears, target class and its ClassInfo/schedule rows are removed, sibling class/info remain unchanged, the Classes page remains current with sibling selected in Details, sidebar key is classes, and session/path stay active. No teacher assignment behavior is in scope. Direct-controller tests already cover success/fallback; F443 adds the QAction/chooser route. Target/CTest: ClassMngrMainWindowScheduleTestingClassesHandoffParityTests. Matrix recorded before implementation; no implementation has started.


## 2026-10-09 - F443 Delete Class QAction success accepted

F443 committed as `1c03b326567cf52d808bc4c54b7a5e77021bb7bf`; branch is ahead by 28.

The new MainWindow test triggers the real deleteClass QAction, selects the target through the actual chooser, and accepts the destructive confirmation. It verifies target class/ClassInfo/schedule removal, sibling record/info unchanged and active in Classes Details, Classes sidebar route, stable page/session/path, and no warning or unsaved-change prompt. No teacher assignments or cross-page navigation are asserted. Executor and independent Tester passed the target build, exact CTest 1/1, direct target QtTest 12 passes (10 test cases plus setup/cleanup), and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full-suite run or production/CMake change. F443 is accepted and ready to commit. F444 successful Open File QAction replacement is next after this commit; Batch 22 remains active.

## 2026-10-09 - F444 Open File replacement acceptance review

F444 committed as `8a21da618870ba4308415aaf5927fa1393af6e97`; branch is ahead by 29.

Two independent read-only reviews approve F444 for implementation. Existing coverage tests Open File Cancel and Save followed by picker cancellation while a profile is open; F435 tests the empty-state banner path. F444 covers the distinct successful replacement transition.

Acceptance: Start with a clean open profile A on My Workspace Schedule, trigger the real Open File QAction, and choose a distinct seeded profile B through the Teacher Profile picker. Verify one correctly configured chooser request, profile B's path and persisted data active, My Workspace Schedule and its Sidebar selection, and no warning or unsaved-changes prompt. `DatabaseSession` is reused in place, so assert active path/data rather than pointer identity. Same-path open, dirty replacement choices, and load failures remain outside scope.

Source map: `MainWindow::connectControllers()` -> `FileController::openFile()` -> `WorkspaceCoordinator::openWorkspace()` -> `MainWindow::applyDatabaseLoadedState()`. Focused test class/target: `MainWindowOpenFileParityTests` / `ClassMngrMainWindowOpenFileParityTests`. F444 is accepted and ready to commit.

Verification: target build passed with no compiler warnings; the selected QtTest slot passed 3/3 functions (setup, test, cleanup); exact filtered CTest `ClassMngrMainWindowOpenFileParityTests` passed 1/1. The final source review approved the Schedule starting state and assertions. Independent Tester could not inspect or run commands because process creation failed with `helper_unknown_error: setup refresh had errors`. One missing Qt font-directory runtime warning; no full suite.

## 2026-10-09 - F445 Save QAction persistence acceptance review

F445 committed as `d9180f1c465976dfdd707382a1e615108aed9387`; branch is ahead by 30.

Two independent read-only reviews support F445. The local source map found no MainWindow Save QAction persistence case in `MainWindowSaveAsExportParityTests`; a separate scope review found service-level persistence coverage only, which does not test the QAction integration. The scope review used a default-branch snapshot because local process setup failed, so local target details come from the source map.

Acceptance: Start with a clean file-backed profile open on My Workspace Schedule and a nonempty current path. Begin an explicit transaction on the active DB connection, write a distinctive setting value, then trigger the real Save QAction. Verify the value persisted through a fresh connection/reopened profile, the path/page/sidebar stay stable, and no Save As picker, warning, or unsaved prompt appears. A transaction is required because an ordinary setting write auto-commits. Exclude Save As, commit-failure behavior, and claims about error reporting; the save path ignores the commit result.

Source map: MainWindow Save QAction -> `FileController::saveFile()` -> `saveDatabase()` / workspace commit path. Focused class/target: `MainWindowSaveAsExportParityTests` / `ClassMngrMainWindowSaveAsExportParityTests`. F445 is accepted and ready to commit; F446 remains provisional.

Verification: target build passed without compiler warnings; selected QtTest passed 3/3 functions, full target QtTest passed 8/8 functions, and exact filtered CTest `ClassMngrMainWindowSaveAsExportParityTests` passed 1/1. Independent source review approved the transaction, real QAction, fresh-connection readback, and UI-state assertions. `SettingsService::save()` stalled before the action, so the fixture stages the unique value with a prepared `QSqlQuery` on the same active connection inside the transaction; Save commits it and a fresh `ApplicationServices` reads it. One missing Qt font-directory runtime warning; no full suite or production/CMake changes.

## 2026-10-09 - F446 Close-confirmation Save acceptance review

F446 committed as `4b34a3b8a15a062377a245607228097fb43ee46b`; branch is ahead by 31.

Two independent reviews support F446. The local source map confirmed the existing exit test covers Cancel then Discard, while the supplied-source scope review identified Save-on-close as a distinct path from F445's File → Save QAction. The scope review could not access local files due process setup failure.

Acceptance: In a file-backed profile, persist a known baseline personal name, use Manual Save, then edit it to a distinct draft name in My Details. Script the fake unsaved-changes prompt as Save and call `window.close()` through the real `closeEvent`. Verify the Save choice was issued, close was accepted and the window is hidden, with no second prompt. Reopen through fresh `ApplicationServices` and verify the exact draft name replaced the baseline. Manual mode prevents autosave races. Keep the existing Cancel/Discard case; exclude File → Save QAction behavior, Save As, save failure, and unrelated fields.

Source map: `MainWindow::closeEvent()` -> `confirmCurrentPageCanLeave(true)` -> PageManager prompt -> current page `saveChanges()`; My Details saves through `saveMyInfoInternal()`. Focused class/target: `MainWindowExitConfirmationParityTests` / `ClassMngrMainWindowExitConfirmationParityTests`. F446 is accepted and ready to commit.

Verification: target build passed without compiler warnings; selected QtTest passed 3 functions (setup, slot, cleanup); full target QtTest passed 4 functions (two slots, setup, cleanup); exact filtered CTest passed 1/1. Independent source review approved the baseline-to-draft flow, Save choice, normal close, and fresh-service persistence check. One Qt font-directory warning; no full suite or production/CMake changes. The first wrapped CTest regex invocation found no tests because cmd.exe retained the quotes; rerunning with the regex as one direct argument passed.

## 2026-10-09 - F447 New Class QAction acceptance review

F447 committed as `d5b130bd0558146185ebf4cdeab885bde6956fec`; branch is ahead by 32.

Two independent local read-only reviews identified the top-level New Class QAction as an uncovered MainWindow route. The existing `classesRootContextMenuAddClassCreatesAndOpensClass()` case covers the separate Sidebar context-menu action, while the QAction and context menu share `SidebarController::addClass()`.

Acceptance: From a clean open file-backed profile on My Workspace, assert `window.actions().newClass` exists and is enabled, then trigger the real ActionRegistry QAction. Verify one new persisted class is selected and opened on Classes Details, Classes is selected in the Sidebar, the current database session/path remain stable, and no unexpected prompt or warning occurs. This adds MainWindow action wiring coverage around existing creation behavior.

Source map: `ActionRegistry::newClass` -> Classes menu -> `SidebarController::addClass()`; handler creates the class, opens Details, selects Classes. Focused class/target: `MainWindowClassesSidebarRootNavigationTests` / `ClassMngrMainWindowClassesSidebarRootNavigationTests`. Exclude Sidebar context-menu behavior, creation/read failures (deferred F298), dirty-page confirmation, no-database behavior, and unrelated class fields. F447 is committed as `d5b130bd0558146185ebf4cdeab885bde6956fec`.

Verification: target build passed without compiler warnings; selected QtTest passed 3 functions (setup, slot, cleanup); full target QtTest passed 5 functions (three slots, setup, cleanup); exact filtered CTest passed 1/1. Independent source review approved the action wiring, class-count/persisted-ID check, Details route, and Sidebar/session/path assertions. One missing Qt font-directory warning and offscreen `raise()`/keyboard-grab notices occurred; tests passed. No full project suite or production/CMake change.

## 2026-10-09 - F448 New File QAction open-profile acceptance review

Two independent local reviews confirmed a distinct active-profile transition gap. F435 covers New Profile from the no-database banner, and F444 covers Open File replacement; neither covers New File closing an active profile and creating a new one through the MainWindow QAction. Existing FileController lifecycle tests cover creation without an open profile, not this MainWindow handoff.

Acceptance: Start with a clean file-backed profile A open on My Workspace Schedule, select a unique nonexistent `.tps` path B through the fake Save File picker, and trigger the real `window.actions().newFile`. Verify the picker is rooted at the active profile directory, B is created and active with an open database session, A remains on disk with its seeded name and campus unchanged, My Workspace Schedule and the `my_workspace` Sidebar selection are restored, and no prompt, warning, or modal remains. Do not compare session pointer identity.

Source map: `ActionRegistry::newFile` -> `FileController::newFile()` -> `confirmUnsavedChanges()` -> Save File picker -> `closeActiveDatabase()` -> `WorkspaceCoordinator::createWorkspace()` -> `MainWindow::applyDatabaseLoadedState()`. Focused class/target: `MainWindowCloseFileParityTests` / `ClassMngrMainWindowCloseFileParityTests`. Exclude dirty-page choices, picker cancellation, existing-target overwrite, Initial Setup, creation failures, and F435/F444 behavior. Acceptance matrix recorded before implementation; Batch 24 is active.

## 2026-10-09 - F448 New File QAction success accepted

The new MainWindow case triggers the real New File QAction from a clean open profile A, creates profile B at the selected destination, and checks that B becomes active on My Workspace Schedule with the `my_workspace` Sidebar route. It verifies A remains on disk with its seeded name and campus unchanged through a fresh `ApplicationServices` readback, and that no prompt, warning, or modal appears. F435 no-database creation and F444 Open File replacement remain separate coverage.

Verification: `ClassMngrMainWindowCloseFileParityTests` target build passed; the selected QtTest slot passed 3 incidents (setup, test, cleanup) with zero failures; filtered CTest passed 1/1; `git diff --check` is clean. Independent source review approved the strengthened persistence readback and active-session/UI checks, but its test commands could not launch due `helper_unknown_error: setup refresh had errors`. The executor ran the updated verification successfully. One existing Qt font-directory warning appeared. No full project suite or production/CMake change. F448 is accepted; commit metadata and next-slice discovery are recorded below.

## 2026-10-09 - F448 committed; F449 discovery started

F448 committed as `f825a388db1897bc42cacf43c488850fa48a9de8` (`Phase2 - Cover New File QAction open-profile success (F448)`); the branch is ahead by 33. Exactly the six approved paths were committed, the diff check was clean, and `agent_docs/latest_session_work.md` plus `%SystemDrive%/` remained outside. Batch 24 is complete. Batch 25 is active; two independent read-only reviews selected the Exit QAction close-confirmation handoff as F449 (acceptance matrix follows).

## 2026-10-09 - F449 Exit QAction close-confirmation acceptance review

Two independent read-only reviews found that `ActionRegistry::exitApp` is created and added to the File menu but has no project-level signal connection. Existing close-confirmation tests exercise `window.close()` directly and do not reference `exitApp`, so the menu action does not reach the guarded close path.

Acceptance: Seed a file-backed profile with a persisted baseline personal name, open it cleanly in Manual Save mode, and change the name editor to a distinct unsaved draft. Script the close choices as Cancel then Discard, and trigger the real `window.actions().exitApp` each time. After Cancel, verify the window remains visible, the draft remains unsaved, the active session/path remain, and the close prompt was recorded. After Discard, verify the window is hidden, the second prompt was consumed, and fresh-service readback still returns the baseline name.

Source map: `ActionRegistry::exitApp` -> File menu -> `MainWindow::close()` -> `MainWindow::closeEvent()` -> `confirmCurrentPageCanLeave(true)`. Production connection belongs in MainWindow action/controller wiring. Focused class/target/CTest: `MainWindowExitConfirmationParityTests` / `ClassMngrMainWindowExitConfirmationParityTests`. Exclude F446 Save behavior, OS shutdown, and multi-window behavior. Matrix recorded before implementation. F449 acceptance is recorded; commit metadata and next-slice discovery follow.

## 2026-10-09 - F449 Exit QAction success accepted

The File menu Exit QAction now calls `MainWindow::close()`, so it follows the existing `closeEvent` unsaved-changes guard. The new test drives Cancel then Discard through the QAction: Cancel keeps the dirty draft, active session/path, page, and window; Discard hides the window, and a fresh-service readback confirms the persisted baseline remains unchanged. Existing direct close tests and F446 Save coverage remain intact.

Verification: Ninja target build passed; the exact QtTest slot passed 3 incidents (setup, test, cleanup); full target QtTest passed 5 incidents (three test slots plus setup/cleanup); filtered CTest passed 1/1; `git diff --check` is clean. An MSBuild attempt hit an environment FileTracker access-denied error, so verification used the existing Ninja build with initialized MSVC. Independent Tester reran the slot, full target, and filtered CTest successfully and approved the source scope. Qt reported the known missing-font-directory warning and LF-to-CRLF notices; no whitespace errors. No full project suite or CMake changes. F449 acceptance is recorded; commit metadata and next-slice discovery follow.

## 2026-10-09 - F449 committed; F450 discovery started

F449 committed as `4e42a5a9cbe5c261fb78b2e39a98f81c93ef0372` (`Phase2 - Cover Exit QAction close-confirmation handoff (F449)`); the branch is ahead by 34. Exactly the seven approved paths were committed and the diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 25 is complete; Batch 26 is active with bounded read-only QAction coverage discovery underway. No F450 candidate is selected yet.

## 2026-10-09 - F450 Undo QAction focused-editor acceptance review

Two independent read-only reviews found no direct MainWindow Edit QAction coverage. Existing local QUndoStack/component tests do not exercise ActionRegistry wiring or focus-based dispatch through the MainWindow. Undo is a bounded, deterministic path through `EditController` and a personal-details `QLineEdit`.

Acceptance: Seed a file-backed profile with a persisted baseline personal name, start MainWindow on My Workspace Details, focus the name editor, and change the text with keyboard events (`Ctrl+A` plus QTest key typing) so QLineEdit records undo history. Do not call `setText` for the draft. Verify the real `window.actions().undo` is enabled, trigger it, and verify the editor returns to the baseline.

Source map: `ActionRegistry::undo` -> Edit menu -> `MainWindow::connectControllers()` / `EditController::connectActions()` -> focus-based `EditController::undo()` dispatch -> `QLineEdit::undo()`. Dedicated test/target/CTest: `mainwindow_edit_action_parity_tests.cpp` / `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, registered in `cmake/tests/pages_and_output.cmake`. Exclude Redo, clipboard actions, read-only widgets, and other editor types. Matrix recorded before implementation; Batch 26 is active.

## 2026-10-09 - F450 Undo QAction success accepted

The dedicated MainWindow test seeds a file-backed baseline personal name, opens My Workspace Details, focuses the name editor, enters a draft through Ctrl+A and QTest keyboard input, confirms the real Undo QAction is enabled, triggers it, and verifies the baseline value is restored. The test covers EditController focus dispatch to QLineEdit undo; no repository write is expected from this local editor operation. Independent source review approved the test and target registration.

Verification: the Ninja target build passed (the independent rerun reported no work to do); the exact QtTest slot and full target each passed 3 incidents (setup, test, cleanup) with zero failures; filtered CTest passed 1/1 using the generated registration `ClassMngrMainWindowEditActionParityTests`; `git diff --check` is clean. Both direct test runs emitted the existing Qt missing-font-directory warning, and bundled Inter/Pretendard fonts loaded. Initial MSVC attempts needed CMake regeneration and `VsDevCmd` initialization; the final executor and independent runs passed with that environment. No full project suite or production-code changes. F450 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F450 committed; F451 discovery started

F450 committed as `e39852c8a9cef33c80d684dd0e63e18d19a6acf7` (`Phase2 - Cover Undo QAction focused-editor dispatch (F450)`); the branch is ahead by 35. Exactly the seven approved files were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 26 is complete. Batch 27 is active; two independent read-only reviews are beginning bounded MainWindow QAction coverage discovery for F451. No candidate is selected yet.

## 2026-10-09 - F451 Redo QAction focused-editor acceptance review

Two independent reviews found no direct MainWindow Redo QAction coverage. Existing update-controller tests do not exercise Edit action focus dispatch, and the committed F450 test verifies only Undo. Redo is a bounded continuation of the same focused QLineEdit path in EditController.

Acceptance: In the file-backed profile and My Workspace Details setup used by `MainWindowEditActionParityTests`, focus the personal-name editor, and create a distinct draft using keyboard input (`Ctrl+A` and QTest key typing). Trigger the real Undo QAction and verify the baseline value returns. Then verify the real Redo QAction is available, trigger it, and verify the draft value returns while the editor remains focused. Keep the operation local to the editor; do not call `setText` for the draft.

Source map: `ActionRegistry::redo` -> Edit menu -> MainWindow/EditController wiring -> focus-based dispatch -> `QLineEdit::redo()`. Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add the focused Redo behavior to `mainwindow_edit_action_parity_tests.cpp`. Exclude Cut/Copy/Paste, read-only widgets, other editor types, and update-controller behavior. Matrix recorded before implementation; Batch 27 is active.

## 2026-10-09 - F451 Redo QAction success accepted

The existing focused MainWindow edit-action case now covers the paired path: after keyboard-entering a distinct draft in the focused personal-name QLineEdit, it triggers the real Undo QAction and verifies the persisted baseline value returns, then triggers the real Redo QAction and verifies the draft returns while the editor retains focus. The test slot is `undoAndRedoActionsRestoreAndReapplyPersonalNameInFocusedLineEdit`; this extends the F450 focused-editor test without production or CMake changes.

Verification: the Ninja target build passed; the exact slot passed 3 incidents (setup, test, cleanup) with zero failures, and verbose output confirmed the slot ran; full target QtTest passed 3 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. The direct runs emitted Qt’s known missing-font-directory warning; bundled Inter and Pretendard fonts loaded. No full project suite. F451 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F451 committed; F452 discovery started

F451 committed as `3137d522796365e81d4c3f99e341aacaf83cc392` (`Phase2 - Cover Redo QAction focused-editor dispatch (F451)`); the branch is ahead by 36. Exactly the six approved files were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 27 is complete. Batch 28 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F452. No candidate is selected yet.

## 2026-10-09 - F452 Paste QAction focused-editor acceptance review

Two independent reviews found no direct MainWindow Cut/Copy/Paste QAction tests. Paste is selected for its distinct clipboard-dependent enablement path and dispatch to the focused editor.

Acceptance: In `MainWindowEditActionParityTests`, snapshot the current clipboard MIME data and restore it during cleanup, including early assertion exits. Open the file-backed profile on My Workspace Details and focus the personal-name QLineEdit. Clear clipboard content and verify the real Paste QAction is disabled; then set non-empty text, wait for the QAction to enable, select the editor content using Ctrl+A, trigger the actual Paste QAction, and verify the editor contains the clipboard text and retains focus. Avoid `setText` for the edit operation.

Source map: ActionRegistry::paste -> Edit menu -> MainWindow/EditController wiring -> clipboard-change/focus action-state refresh -> focused QLineEdit::paste(). Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add coverage in `mainwindow_edit_action_parity_tests.cpp`. Exclude Cut/Copy, read-only gating, non-text clipboard formats, and other editor types. Matrix recorded before implementation; Batch 28 is active.

## 2026-10-09 - F452 Paste QAction success accepted

Added `pasteActionReplacesSelectedPersonalNameFromClipboard()` to the existing MainWindow edit-action target. The test snapshots clipboard MIME formats and payloads in an RAII restorer, opens a file-backed profile on My Workspace Details, focuses the personal-name QLineEdit, verifies Paste is disabled with empty clipboard content, sets clipboard text and waits for the action to enable, selects the existing name with Ctrl+A, triggers the actual Paste QAction, and verifies replacement text and retained focus. The clipboard is restored on normal and assertion-failure exits. No production or CMake changes.

Verification: Ninja target build passed; the exact slot passed 3 incidents (setup, test, cleanup) with zero failures; full target QtTest passed 4 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. Independent verification captured the selected-slot output in a temp log after initializing VS 18 and Qt 6.12 paths. Qt emitted the known missing-font-directory warning; bundled Inter and Pretendard fonts loaded. No full project suite. F452 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F452 committed; F453 discovery started

F452 committed as `249dd38b8b3ee2223292c99ec740f2470e20e5e9` (`Phase2 - Cover Paste QAction focused-editor dispatch (F452)`); the branch is ahead by 37. Exactly the six approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 28 is complete. Batch 29 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F453. No candidate is selected yet.

## 2026-10-09 - F453 Cut QAction focused-editor acceptance review

Two independent reviews found no MainWindow Cut or Copy QAction coverage after the Undo/Redo/Paste cases. Select Cut as a focused text-edit action with a direct clipboard result.

Acceptance: Add a dedicated Cut slot to `MainWindowEditActionParityTests`, reusing the file-backed personal-name editor setup and `ClipboardMimeDataRestorer`. Open My Workspace Details, focus the personal-name QLineEdit, seed a distinct sentinel clipboard value, select the existing name with Ctrl+A and verify it is the selected text. Confirm the real Cut QAction is enabled, trigger it, and verify the editor is empty, clipboard text now equals the selected baseline name, focus remains in the editor, and the active workspace stays open without a prompt. Do not use `setText` for the edit.

Source map: ActionRegistry::cut -> Edit menu -> MainWindow/EditController wiring -> focused-widget dispatch with editable-text guard -> QLineEdit::cut(). Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add the focused Cut case to `mainwindow_edit_action_parity_tests.cpp`. Exclude Copy, read-only gating, other editor types, persistence assertions, and non-text clipboard formats. Matrix recorded before implementation; Batch 29 is active.

## 2026-10-09 - F453 Cut QAction success accepted

Added `cutActionCopiesSelectedPersonalNameFromFocusedEditor()` to the existing MainWindow edit-action test. It seeds a file-backed baseline, snapshots/restores clipboard MIME data, focuses the personal-name QLineEdit in My Workspace Details, puts sentinel text on the clipboard, selects the baseline using Ctrl+A, and triggers the real enabled Cut QAction. Assertions verify the editor clears, the clipboard now contains the selected name, focus remains, the workspace stays open on Details, and no modal prompt appears. Manual Save mode keeps the intentionally empty field local during the test. No `setText` editing, production or CMake changes.

Verification: Ninja target build passed; the exact Cut slot passed 3 incidents (setup, test, cleanup) with zero failures; full target QtTest passed 5 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. Independent rerun used the CTest offscreen Qt environment and captured both QtTest logs. The first direct launch without Qt runtime settings failed to start; after setting VS 18/Qt 6.12 paths and offscreen mode, the selected slot and full target passed. Qt emitted the known missing-font-directory warning; bundled Inter and Pretendard fonts loaded. No full project suite. F453 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F453 committed; F454 discovery started

F453 committed as `4e9b3d6d79aff3c43b943aec8e165a6ab16ca8a9` (`Phase2 - Cover Cut QAction focused-editor dispatch (F453)`); the branch is ahead by 38. Exactly the six approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 29 is complete. Batch 30 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F454. No candidate is selected yet.

## 2026-10-09 - F454 Copy QAction focused-editor acceptance review

Two independent reviews found no direct MainWindow Copy QAction coverage after Undo/Redo, Paste, and Cut. Copy is a bounded focused-widget action that should update the clipboard while preserving editor and workspace state.

Acceptance: Add a dedicated Copy slot to `MainWindowEditActionParityTests`, reusing the file-backed personal-name editor setup and `ClipboardMimeDataRestorer`. Open My Workspace Details, focus the personal-name QLineEdit, seed a sentinel clipboard value, select the baseline name using Ctrl+A, and verify the selection. Confirm the real Copy QAction is enabled, trigger it, then verify clipboard text equals the selected name while editor text, editor focus, active workspace/path, and Details route remain unchanged with no modal prompt. Restore clipboard MIME data on all exits. Exclude empty selection behavior, keyboard-shortcut dispatch, other editor types, and production/CMake changes.

Source map: ActionRegistry::copy -> Edit menu -> MainWindow/EditController wiring -> focused-widget dispatch -> QLineEdit::copy(). Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add the focused Copy case in `mainwindow_edit_action_parity_tests.cpp`. Matrix recorded before implementation; Batch 30 is active.

## 2026-10-09 - F454 Copy QAction success accepted

Added `copyActionCopiesSelectedPersonalNameFromFocusedEditor()` to the existing MainWindow edit-action test. It snapshots clipboard MIME formats/payloads, opens a file-backed profile on My Workspace Details, focuses the personal-name QLineEdit, seeds sentinel clipboard text, selects the baseline with Ctrl+A, and triggers the real enabled Copy QAction. Assertions verify the clipboard receives the selection, the editor value/focus and selection stay unchanged, the workspace/path and Details route remain active, and no modal prompt appears. The test explicitly restores clipboard data and verifies the original formats and payload bytes; the RAII destructor remains a fallback on early assertion returns. No production or CMake changes.

Verification: Ninja target build passed; the exact Copy slot passed 3 incidents (setup, test, cleanup) with zero failures; full target QtTest passed 6 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. Independent rerun confirmed the selection and clipboard restoration assertions. Qt emitted the known missing-font-directory warning; bundled fonts loaded. No full project suite. F454 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F454 committed; F455 discovery started

F454 committed as `dd73b6d9c536809f05d38487ecca4bc873b9ea86` (`Phase2 - Cover Copy QAction focused-editor dispatch (F454)`); the branch is ahead by 39. Exactly the six approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 30 is complete. Batch 31 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F455. No candidate is selected yet.

## 2026-10-09 - F455 About QAction modal-handoff acceptance review

Independent Help-action reviews found no About QAction test. Select a bounded modal-handoff case that verifies the actual Help QAction opens the parented AboutDialog.

Acceptance: In a dedicated offscreen `MainWindowAboutActionParityTests` target, isolate `CLASSMNGR_SETTINGS_ROOT` in a temporary directory because DialogShell persists geometry. Construct and show MainWindow without an update controller; verify the real About QAction exists and is enabled. Queue a zero-delay timer before triggering it; while the QAction’s modal `exec()` is active, capture that `QApplication::activeModalWidget()` is an AboutDialog, it is visible/modal and parented to the MainWindow, then close it so the trigger returns. Afterward verify the observations, MainWindow remains visible, and no modal widget remains.

Source map: ActionRegistry::about -> Help menu -> MainWindow action connection -> `AboutDialog dialog(this); dialog.exec()`. Test file/target/CTest: `mainwindow_about_action_parity_tests.cpp` / `MainWindowAboutActionParityTests` / `ClassMngrMainWindowAboutActionParityTests`, registered in `cmake/tests/pages_and_output.cmake` with the neighboring MainWindow Qt/resource setup. Exclude license buttons/content, external links, and Check for Updates/network/download behavior. Matrix recorded before implementation; Batch 31 is active.

## 2026-10-09 - F455 About QAction modal handoff accepted

Added `aboutActionShowsModalAboutDialogAndReturnsToMainWindow()` and the dedicated `MainWindowAboutActionParity` target/`ClassMngrMainWindowAboutActionParityTests` registration. The test isolates settings in QTemporaryDir, creates a MainWindow without an update controller, and triggers the actual enabled About QAction. A zero-delay timer captures the visible modal AboutDialog, confirms it is modal and parented to MainWindow, then closes it. A mismatch branch closes any active modal and child AboutDialog so the test reaches its assertions instead of hanging. After the trigger, assertions verify the observations, MainWindow visibility, and no remaining modal. No production changes.

Verification: Ninja target build passed; the exact QtTest slot and full target each passed 3 incidents (setup, test, cleanup) with zero failures; filtered CTest `ClassMngrMainWindowAboutActionParityTests` passed 1/1; `git diff --check` is clean. Independent rerun confirmed the exact slot and modal assertions. An initial direct shell launch hung during QApplication startup due to missing Qt runtime settings; the final run used VS 18/Qt 6.12 and offscreen settings. CMake emitted pre-existing missing-Vulkan and object-path warnings; Qt emitted the known missing-font-directory warning while bundled Inter/Pretendard fonts loaded. No full project suite. F455 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F455 committed; F456 discovery started

F455 committed as `76c67663cda5604353b4ba25e54c97160f6daf90` (`Phase2 - Cover About QAction modal handoff (F455)`); the branch is ahead by 40. Exactly the seven approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 31 is complete. Batch 32 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F456. No candidate is selected yet.

## 2026-10-09 - F456 Check for Updates QAction acceptance scope

Cover the real enabled `window.actions().checkForUpdates` QAction and its handoff to a visible, manual `UpdateDialog`. Inject an empty `UpdateConfiguration` so checking fails deterministically before network access. Assert exactly one `GitHub releases API URL is not configured.` failure, a visible MainWindow-parented dialog with `automaticUpdatePrompt == false`, the expected failure title/details and enabled “Try Again” button, and that MainWindow remains visible. Isolate startup cleanup by setting `TMP`, `TEMP`, and `TMPDIR` to a dedicated CTest build-directory root and asserting `QStandardPaths::TempLocation` resolves there before triggering the action; create that private root before constructing the settings `QTemporaryDir`. No production changes or successful online update behavior are in scope.

## 2026-10-09 - F456 Check for Updates QAction accepted

Added `mainwindow_check_for_updates_action_parity_tests.cpp` and the dedicated `MainWindowCheckForUpdatesActionParity` CTest target, including keyboard resources/translations and the isolated temp environment. The actual QAction opened the manual update dialog; the test observed one configuration failure, verified the failure UI and retry action, and confirmed MainWindow remained visible. Because the service configuration has no API URL, the action makes no network request; startup cleanup resolves only inside the test-specific temp root. No production changes.

Verification: the Ninja target build passed under Visual Studio 18 with Qt 6.12; focused CTest `ClassMngrMainWindowCheckForUpdatesActionParityTests` passed 1/1; direct full-target and selected-slot QtTest runs each passed 3/0; an independent tester reran CTest and confirmed temp isolation and action-path assertions. Qt emitted the existing missing-font-directory and offscreen size/raise warnings while bundled fonts loaded. No full project suite. F456 is accepted and ready to commit; Batch 32 remains active until commit.

## 2026-10-09 - F456 committed; F457 discovery started

F456 committed as `d90def93f7947d0f031dc6f37a8a4a491f4d3718` (`Phase2 - Cover Check for Updates QAction manual handoff (F456)`); the branch was ahead by 41. Exactly the seven approved paths were committed and the cached diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 32 is complete. Batch 33 is active with F457 selected for implementation.

## 2026-10-09 - F457 Font Size QAction accepted

Added `mainwindow_font_size_action_parity_tests.cpp` and the dedicated offscreen `MainWindowFontSizeActionParity` target. It isolates settings in QTemporaryDir, establishes an English/Normal baseline, disables recent-database loading, triggers the actual Large QAction, and verifies option/check state, persisted Large preference, FontManager offset, and QApplication point size. RAII restores the original process font and offset after MainWindow destruction, including assertion-failure exits. No production behavior changes.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; direct QtTest and independent direct rerun each passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1; `git diff --check` is clean. CMake reported pre-existing long object-path and missing optional Vulkan-header warnings; Qt reported a missing bundled `lib/fonts` directory while Inter and Pretendard loaded. No full project suite. F457 is accepted; commit transition is recorded below.

## 2026-10-09 - F457 committed; F458 discovery started

F457, `Phase2 - Cover Font Size QAction application handoff (F457)`, committed as `719efeacb6e5a8533f2dd45f6fb0fdfe152c0714` on `Qt-Rewrite` (42 commits ahead of origin). The commit contains exactly seven scoped paths and the cached diff check was clean. Batch 33 is complete; Batch 34 is active with F458 Theme QAction coverage selected for implementation. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain excluded user changes.

## 2026-10-09 - F458 Theme QAction accepted

Added `mainwindow_theme_action_parity_tests.cpp` and the dedicated offscreen `MainWindowThemeActionParity` target. It isolates settings, starts with a Light preference and injected ThemeService, disables recent-database loading, and triggers the actual Dark QAction. Assertions cover enabled/checked state, persisted preference, service theme, application Window color, and MainWindow `theme` property; the test then triggers Light and verifies restoration. The RAII guard captures the original palette/stylesheet before applying Light and restores them while MainWindow and its service remain alive, including assertion-return exits. No production changes.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1; `git diff --check` and untracked-source whitespace checks are clean. CMake reported unrelated long object-path warnings during reconfigure; Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full project suite. F458 is accepted; commit transition follows.

## 2026-10-09 - F458 committed; F459 discovery started

F458, `Phase2 - Cover Theme QAction application handoff (F458)`, committed as `2ec351a805af2064a865c34b76b46f60df80acdc` on `Qt-Rewrite` (43 commits ahead of origin). The commit contains exactly seven scoped paths and its cached diff check was clean. Batch 34 is complete; Batch 35 is active with F459 Document Viewer Background QAction coverage selected for implementation. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain excluded user changes.

## 2026-10-09 - F459 Document Viewer Background QAction accepted

Added `mainwindow_document_viewer_background_action_parity_tests.cpp` and the dedicated offscreen `MainWindowDocumentViewerBackgroundActionParity` target. It isolates settings, disables recent-database loading, and creates a blank PDF viewer through `pageManager()->ensurePdfViewerPage()` without loading a document. It triggers the actual Black and Default actions and checks enabled/check state, persisted values, viewer and viewport background properties, and palette. A scoped guard triggers Default while MainWindow and the viewer remain alive, including QtTest early-return paths. No production changes, PDF load, database, network, or autosave timer.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1; `git diff --check` and untracked-source whitespace/conflict checks are clean. CMake reported unrelated long object paths during reconfigure; Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full project suite. F459 is accepted and ready to commit; Batch 35 remains active until commit.

## 2026-10-09 - F459 committed; F460 selected

F459, `Phase2 - Cover Document Viewer Background QAction parity (F459)`, committed as `cba31503f0bb5accf68de2032131db984403deff` on `Qt-Rewrite` (44 commits ahead of origin). The commit contains exactly seven scoped paths and its cached diff check was clean. Batch 35 is complete; Batch 36 is active with F460 PDF Document Viewer Page Spacing QAction coverage selected. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain excluded user changes.

## 2026-10-09 - F460 Document Viewer Page Spacing QAction acceptance

Cover the actual MainWindow Page Spacing QAction handoff on a blank PDF viewer. In a dedicated offscreen target, isolate settings in a temporary directory, disable recent-database loading, and create the viewer with `pageManager()->ensurePdfViewerPage()` without loading a PDF. Trigger the real Large and Small actions and assert enabled/check state, persisted preference, and `QPdfView::pageSpacing()` values of 32 and 8. A scoped cleanup guard returns to Small while MainWindow and the viewer remain alive, including assertion-return paths. No PDF, database, autosave timer, network, production, or user-file behavior is in scope. F460 is selected; implementation and verification are pending.

## 2026-10-09 - F460 Document Viewer Page Spacing QAction accepted

Added `mainwindow_document_viewer_page_spacing_action_parity_tests.cpp` and the dedicated `MainWindowDocumentViewerPageSpacingActionParity` CTest target. The offscreen test isolates settings, disables recent-database loading, and creates a blank viewer without loading a document. It triggers the actual Large then Small actions and checks selected action/state, persisted preferences, and `QPdfView::pageSpacing()` values of 32 and 8. `DocumentPageSpacingRestorer` triggers Small while MainWindow remains alive, including assertion-return paths. No production changes or PDF/database/network/autosave activity.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1. Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full suite. F460 is accepted and ready to commit; Batch 36 remains active until commit.

## 2026-10-09 - F460 committed; F461 discovery started

F460, `Phase2 - Cover Document Viewer Page Spacing QAction parity (F460)`, committed as `fba46915d76b05aab53de85760a3f857dc4ed2a2` on `Qt-Rewrite` (45 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 36 is complete. Batch 37 is active; two independent read-only reviews are starting bounded QAction coverage discovery for F461. No candidate is selected yet. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

## 2026-10-09 - F461 Sidebar Overflow Tooltips QAction acceptance

Two independent reviews found the Show Sidebar Tooltips option has preference persistence coverage but no direct MainWindow QAction test. MainWindow connects `showSidebarTooltips` to `Sidebar::setOverflowTooltipsEnabled`, which applies the full text to overflowing tree items. Select this as a bounded visible handoff; Save Mode remains a wider autosave/timer path.

Acceptance: Add a dedicated offscreen MainWindow test with temporary settings and recent-database loading disabled. Insert a synthetic teacher with a very long name via `Sidebar::addTeacherNode`, constrain the Sidebar/tree width, show the window, and process events so overflow is established. Trigger the real enabled `showSidebarTooltips` QAction through false, true, then false. Assert check/option/persisted state and that the item tooltip becomes the full name when enabled and clears when disabled. Use an RAII guard to finish false while MainWindow is alive, including assertion-return paths. No database, network, autosave timer, or production change. F461 is selected for implementation; Batch 37 is active.

## 2026-10-09 - F461 Sidebar Overflow Tooltips QAction accepted

Added `mainwindow_sidebar_overflow_tooltips_action_parity_tests.cpp` and the dedicated `MainWindowSidebarOverflowTooltipsActionParity` target/`ClassMngrMainWindowSidebarOverflowTooltipsActionParityTests` registration. The offscreen test isolates settings, disables recent-database loading, adds a long synthetic teacher, constrains the Sidebar/tree, and verifies overflow by comparing measured label width with available viewport width after showing the window. It triggers the actual enabled/checkable `showSidebarTooltips` QAction through false→true→false and checks persisted state plus the item tooltip (empty/full name/empty). The RAII guard restores false while MainWindow is alive, including assertion-return paths. No production changes or DB/network/autosave activity.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1. Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full suite. F461 is accepted and ready to commit; Batch 37 remains active until commit.

## 2026-10-09 - F461 committed; F462 selected

F461, `Phase2 - Cover Sidebar Overflow Tooltips QAction parity (F461)`, committed as `e590ea773d4b6b1d415d248c9c3f5068094d5d4c` on `Qt-Rewrite` (46 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 37 is complete; Batch 38 is active with F462 Save Mode QAction parity selected. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

## 2026-10-09 - F462 Save Mode QAction acceptance

Two independent reviews found MainWindow Save Mode QAction coverage missing. Select it over Sidebar Marquee because a clean `TestingClassesPage` can be ensured without activating the page or loading database data, and its public `AutosaveCoordinator::saveMode()` gives a stable effect to assert; Marquee depends on hover and a timer.

Acceptance: Add a dedicated offscreen MainWindow test with temporary settings and recent-database loading disabled. Call `pageManager()->ensureTestingClassesPage()` without activating it, find the page's direct-child `AutosaveCoordinator`, and verify the page is clean. Trigger the actual Manual then Automatic Save Mode actions. Assert check/option/persisted preference state, coordinator mode, and that a `saveRequested` spy remains at zero while the page is clean. Use RAII to restore Automatic while MainWindow remains alive, including assertion-return paths. No database read, network, user-file access, dirty page, or autosave activity. F462 is selected for implementation; Batch 38 is active.

## 2026-10-09 - F462 Save Mode QAction accepted

Added `mainwindow_save_mode_action_parity_tests.cpp` and the dedicated `MainWindowSaveModeActionParity` target/`ClassMngrMainWindowSaveModeActionParityTests` registration. The offscreen test isolates settings, disables recent-database loading, and creates a hidden `TestingClassesPage` without activating or refreshing it. It finds the direct-child `AutosaveCoordinator`, verifies clean state, triggers the actual Manual then Automatic actions, and checks action/option/persisted values, coordinator modes, page/coordinator cleanliness, and zero `saveRequested` signals after both transitions. An RAII guard restores Automatic while MainWindow remains alive, including assertion-return paths. No production changes or database/network/user-file activity.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1. Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full suite. F462 is accepted and ready to commit; Batch 38 remains active until commit.
