# Phase 2 — Portable Engine Extraction

> Progress is tracked in [00-START-HERE.md](00-START-HERE.md).

## Goal

Make `ClassMngrEngine` the sole implementation of product rules, persistence,
and platform-neutral workflows consumed by both the WinUI and Qt products.

## Implementation Sequence

Extract vertical slices instead of mechanically converting every Qt type.

1. Define engine public contracts with UTF-8 strings, `std::chrono`, standard
   containers, explicit optionals, and typed result/error values.
2. Move domain models, validators, scheduling rules, class transfers, calendar
   recurrence, speaking-evaluation calculations, and deterministic formatting.
3. Introduce use cases such as `OpenDatabase`, `SaveClass`, `ImportSchedule`,
   `LoadCalendarRange`, and `GenerateReportModel`. UIs call use cases rather
   than repositories directly.
4. Replace Qt SQL in the portable data layer with pinned SQLite C APIs,
   prepared statements, transactions, migrations, busy handling, and typed row
   mapping.
5. Extract interfaces for files, paths, settings, networking, signature
   verification, process launch, clock, logging, resources, and cancellation.
6. Move resource-pack precedence and signature policy behind portable
   byte-stream/filesystem contracts.
7. Separate report content and pagination decisions from drawing. Emit
   renderer-neutral page, table, text-run, and asset descriptions.
8. Keep thin Qt adapters operational after every slice and add WinUI adapters
   only for platform behavior, never product rules.
9. Run cross-platform database round trips for each migrated persistence slice.

## Specific Phase 2 Targets

These targets turn the remaining extraction work into independently actionable
slices. Keep the Qt implementation as an adapter until the corresponding
engine contract, tests, and round-trip evidence are complete. `[ ]` means the
target remains open.

### P2-01 — Complete report and export adapter boundaries

- [x] Schedule reports: make `ScheduleReportModel` the sole source of report
  content for print/export adapters. Keep `QPainter`, `QPdfWriter`, page
  geometry, fonts, and colors in Qt.
- [x] Roster reports: make engine-produced cell values and template policy the
  sole source of roster content. Keep PDF geometry, drawing, and printing in
  Qt.
- [x] Speaking-evaluation reports: finish the typed handoff from report and
  batch-job services to PDF, ZIP, and PowerPoint adapters. Isolate resource
  mapping, JSON transport, Office automation, progress, and output commits.
- [x] Sub-prep reports: consume engine document/package plans from the output
  adapter and keep rendering, printing, and desktop integration in Qt.

Representative code: `src/features/schedule/services/schedule_print_service.cpp`,
`src/features/roster/services/roster_template_print_service.cpp`,
`src/features/speaking_eval/services/speaking_eval_batch_report_service.cpp`,
and `src/features/sub_prep/services/sub_prep_package_service.cpp`.

Done when report content, ordering, pagination decisions, and output policy
come from `ClassMngrEngine`, while Qt owns only presentation and platform I/O.

### P2-02 — Define portable file and output contracts

- [x] Define engine-facing contracts for paths, byte streams, temporary files,
  directory creation, atomic replacement, file copy, and output existence.
- [x] Route `DataService::saveAs()` and `exportAs()`, report output commits,
  ZIP/document output, and sub-prep package staging through those contracts.
- [x] Return typed errors for invalid paths, missing resources, partial output,
  and failed atomic commits.

Done when engine workflows do not depend on `QFile`, `QDir`, `QSaveFile`, or
`QTemporaryDir`, and adapters can provide equivalent behavior on Windows,
macOS, and Linux.

### P2-03 — Finish retained Qt database and application-service adapters

- [x] Audit every retained repository and make it a thin Qt-to-engine adapter,
  with no product rules duplicated in the Qt layer.
- [x] Make `DatabaseSession` an explicit compatibility boundary: engine owns
  file-backed open/migration semantics, while Qt owns only its temporary SQL
  session until that adapter is retired.
- [x] Route `ApplicationServices` and `DataService` workflows through engine
  use cases rather than repository/fallback paths.
- [x] Remove or document compatibility branches once the engine path has
  equivalent coverage.

Representative code: `src/data/database/database_session.cpp`,
`src/data/data_service.cpp`, and `src/core/application_services.cpp`.

### P2-04 — Close import and file-codec boundaries

- [x] Schedule workbook import: keep XLSX/OOXML parsing in the Qt adapter,
  but make conversion, typed errors, validation, cancellation, and apply
  semantics flow through engine services.
- [x] Calendar workbook import: keep download and workbook parsing in Qt,
  while engine owns normalized events, rules, recurrence, and persistence
  decisions.
- [x] Teacher import: finish the boundary between Qt file/template codecs and
  `TeacherImportService`; remove duplicated validation or matching rules.
- [x] Class-transfer and campus JSON: isolate JSON/filesystem/resource access
  from the engine contracts and add representative codec fixtures.
- [x] Audit `ScheduleImportMatcher`, `ScheduleImportPlanValidator`, and
  `ScheduleImportStateValidator`; route any live behavior through the engine,
  or remove/quarantine these apparently superseded duplicate implementations.

### P2-05 — Extract resource-pack and catalog policy

- [x] Move resource-pack manifest parsing, precedence/fallback, integrity, and
  signature policy behind portable engine contracts.
- [x] Keep `QResource`/RCC mounting, downloads, staging, and installation in
  Qt or another platform adapter.
- [x] Put document-catalog and campus resource metadata/path checks behind the
  same portable boundary where they affect engine workflows.

Representative code: `src/core/resource_packs/resource_pack_manager.cpp`,
`src/core/resource_packs/resource_pack_manifest.h`,
`src/core/resource_packs/resource_pack_update_service.h`, and
`src/features/documents/document_catalog.cpp`.

### P2-06 — Introduce the remaining platform-service interfaces

- [x] Define injectable interfaces for settings, networking, signature
  verification, process launch, clock, logging, resources, and cancellation.
- [x] Keep Qt implementations as adapters and add WinUI implementations only
  for platform behavior.
- [x] Ensure engine services can be tested headlessly with deterministic clock,
  in-memory or fixture-backed resources, and controlled cancellation/errors.

### P2-07 — Expand per-slice interoperability evidence

- [x] For each migrated persistence slice, test open/migrate, CRUD or import,
  write, close, and reopen using the shared fixture corpus.
- [x] Add explicit Qt-to-engine and engine-to-Qt round trips, including invalid
  input, rollback, migration, busy handling, and partial-failure cases.
- [x] Run the headless engine matrix on x64 and x86 and record retained-Qt
  adapter results separately from compile-only or environment-limited checks.

Done: the shared corpus now has individual per-slice engine assertions, typed
invalid/rollback/busy coverage, and explicit retained-Qt adapter round trips in
both directions, with x64/x86 headless results recorded separately from the
Qt 6.12 retained-adapter result.

### P2-08 — Retained-adapter and legacy-rule cleanup

- [x] Search for Qt-side business rules that now have engine equivalents and
  either delete them, delegate them, or mark them as presentation-only.
- [x] Resolve stale migration TODOs and compatibility comments in
  `DataService` after confirming the corresponding engine service is covered.
- [x] Keep a short list of intentionally retained Qt responsibilities:
  rendering, printing, workbook/JSON codecs, resource mounting, filesystem
  commits, and Office/process automation.

Done: the retained Qt audit removed duplicate directory-name validation from
the presentation page, documented the remaining Qt-only input and presentation
responsibilities, and replaced stale `DataService` migration markers with
engine-service ownership comments. The retained responsibility list and
validation evidence are recorded in the [Phase 2 local validation
record](../../docs/porting/windows-winui/phase2-local-validation.md#p2-08--retained-adapter-and-legacy-rule-cleanup--2026-09-03).

## Remaining Phase 2 slices — code audit (2026-09-04)

The P2-01–P2-08 checklists above describe the extraction packages already
recorded as complete. A source audit against the Goal and Exit Gate found the
following open follow-up slices. They are ordered by how directly they affect
the portable-engine boundary. `[ ]` means open; completion requires the
acceptance evidence listed for that slice.

### P2-R01 — Extract speaking-evaluation dashboard orchestration

**Status (2026-09-05): Complete; headless and retained-Qt integration
validation passed.** The portable dashboard orchestration and headless engine
coverage are complete, and the retained Qt service/page boundary now has
serial x64 Debug build and test evidence.

- [x] Move the policy in
  `SpeakingEvaluationService::analyticsDashboard()` from
  `src/app/services/feature_services.cpp` into a Qt-free engine dashboard
  service/model. The engine boundary must own `All` selection, canonical
  evaluation iteration, current-roster filtering, current versus historical
  snapshot scope, cross-evaluation aggregation, latest fully-scored
  class-shape selection, and year-to-date point generation.
- [x] Verify that the Qt service and `class_analytics_page` remain responsible
  only for database/input conversion, localization, and rendering, then pass
  the retained Qt/WinUI integration build. The headless cases are implemented;
  preserve coverage for empty and partial evaluations, selected versus `All`,
  roster filtering, latest-completed selection, and historical-cohort YTD.

Evidence: `src/engine/include/classmngr/engine/speaking_analytics.h` and
`src/engine/speaking_analytics.cpp` expose and implement the dashboard
orchestration. `src/features/classes/services/speaking_analytics.cpp` and
`src/app/services/feature_services.cpp` convert Qt inputs and map the engine
result. `src/features/classes/ui/class_analytics_page.cpp` selects the
evaluation, requests the service result, and renders/localizes the returned
snapshots without reimplementing dashboard policy. The current x64 Debug
`ClassMngrEngineSpeakingAnalyticsTests`, `ClassMngrSpeakingAnalyticsTests`,
and `ClassMngrClassAnalyticsRankingModelTests` all pass.

### P2-R02 — Unify interactive roster and speaking-evaluation validation

**Status (2026-09-05): Implemented; focused engine and retained-Qt parity passed.**
Committed as `b4ff0c8`. Shared UTF-8 name validation and duplicate-pair policy
now feed the Qt utility and both engine validators.

- [x] Add or expose an engine-facing per-row/per-cell name-validation
  contract, then route `RosterModel`, `SpeakingEvalModel`, their delegates,
  and questionable-length confirmation through it.
- [x] Keep Qt responsible for mapping engine issues to localized messages,
  cell/row highlights, and confirmation UI. Remove the duplicate Qt
  character, length, and duplicate-name policy from live paths.
- [x] Add parity cases for invalid ASCII/UTF-8 input, Korean suffixes and
  lengths, duplicate pairs, and normalized names so interactive feedback and
  save-time validation cannot diverge.

Evidence: `src/core/utils/student_name_utils.cpp` delegates the active Qt
name normalization/validation to the engine, and the roster and speaking
evaluation UI maps engine issues to localized messages and highlights. The
focused parity tests now cover malformed UTF-8, invalid ASCII, Korean suffixes,
unusual/over-limit lengths, normalized duplicate pairs, and save-time issue
mapping in both interactive models and their engine validators. The current
x64 Debug retained-Qt/engine selection passes 6/6.

### P2-R03 — Finish the teacher-import candidate policy boundary

**Status (2026-09-05): Complete; typed import policy and retained-adapter
validation passed.** Template parsing remains responsible for workbook and
source-row extraction, while the engine now owns candidate identity, stored
record ambiguity, and latest-source-date policy.

- [x] Keep workbook decoding and template-specific extraction in
  `SectionedContactListTemplate`, but pass all decoded candidates to the
  engine for cross-candidate duplicate/identity validation and import
  matching. The engine must remain authoritative for stored-record
  ambiguity and plan validity.
- [x] Expose the latest source-date comparison as a typed engine import or
  preview decision. Keep the older/equal-date confirmation prompt in Qt/WinUI,
  but ensure the comparison and persisted-date update use one engine policy.
- [x] Preserve source-row diagnostics and localized presentation errors in
  the Qt adapter, and add fixtures for duplicate source candidates,
  ambiguous stored matches, and normalized birthdays/names.

Evidence: `src/features/teacher/import/sectioned_contact_list_template.cpp`
continues to perform `normalizedBirthday`, `cleanedKoreanName`,
`cleanedStaffName`, and source-row identity normalization, while all decoded
candidates cross the repository boundary into
`src/engine/teacher_import_service.cpp` for plan validation, duplicate
candidates, stored-record ambiguity, matching, and persistence. The engine
now exposes typed `NoPreviousDate`, `Newer`, `Equal`, and `Older` decisions,
rejecting invalid candidate or persisted dates for the read-only comparison;
the monotonic persistence path shares the same date comparator. The Qt
service/controller maps that decision to the existing localized confirmation
prompt without reading or comparing the raw setting directly. The focused
engine and retained-Qt teacher-import tests pass.

### P2-R04 — Collapse legacy application-service fallbacks

**Status (2026-09-05): Implemented for production composition; facade retirement
is tracked for Phase 3.** ApplicationServices owns the session and production
feature services receive it directly. The legacy DataService API remains an
explicit shared-session adapter with a named owner and a deadline before the
Phase 3 application-foundation exit gate.

- [x] Keep migrated production workflows on the engine-first session/use-case
  graph and make the remaining compatibility object an explicit adapter with
  a named retirement owner and deadline.
- [x] Remove the `FeatureService` `DataService*` fallback branches and narrow
  `DataService` to compatibility/test callers after equivalent engine paths
  are proven. No new UI or controller code should add another facade method.
- [x] Keep the existing Qt-facing service API stable during the transition,
  with tests proving that open/close and all migrated feature operations use
  the same engine-backed session.

Evidence: `src/core/application_services.h/.cpp` owns the shared
`DatabaseSession` and constructs production feature services from it. The
`FeatureService` `DataService*` and mixed-session fallback constructors are
removed, and the source audit found no production caller of the compatibility
facade. `ClassMngrDataServiceLifecycleTests` now covers shared-session service
construction, open/close availability, and the compatibility adapter. The
broad `DataService` facade remains only as a source-compatible adapter until
the Phase 3 deadline.

### P2-R05 — Retire the remaining Qt SQL compatibility path

**Status (2026-09-05): Implemented for production composition; Qt SQL helpers
are test-only compatibility support.** CalendarEventCache reads through the
engine service, DatabaseSession file-backed opens are fully engine-owned, and
exact `:memory:` ownership is explicit.

- [x] Migrate retained database consumers, including the asynchronous
  `CalendarEventCache`, to engine `SqliteDatabase`/service access, or isolate
  them behind an explicitly temporary adapter boundary.
- [x] Retire `DatabaseSession::compatibilityDatabase()` and quarantine the
  remaining Qt schema/transaction/query helpers in a test-only compatibility
  target. `DataService::save()` and `ApplicationServices::saveDatabase()` are
  currently no-ops; confirm that no production workflow relies on them.
- [x] Define exact `:memory:` ownership: make it a shared engine-backed test
  mode, or document and isolate it as a compatibility-only path. It must not
  mix one Qt in-memory database with independent repository databases.
- [x] Confirm that file-backed open, migration, CRUD, rollback, and cache
  reads no longer depend on a Qt SQL connection or duplicate schema logic.

Evidence: `DatabaseSession` no longer owns a `QSqlDatabase` or Qt schema
manager. File-backed opens call `OpenDatabase`, then construct repositories
against the normalized engine path. Exact `:memory:` opens are rejected by the
session with an explicit instruction to use
`OpenDatabase::execute(":memory:")` for headless engine ownership. The former
Qt schema, transaction, and query helpers are compiled only into
`ClassMngrQtSqlTestSupport`, and lifecycle tests use explicit test-owned Qt
connections for compatibility fixtures. The calendar cache reads through the
engine service; the remaining Qt repository constructors are compatibility
adapters and are not part of production composition.

### P2-R06 — Make platform-service contracts live at engine boundaries

**Status (2026-09-05): Complete for the audited workflows and adapters.**
Calendar import, class transfer, and ZIP output now compose the clock contract
at their retained Qt boundaries, while archive finalization uses the same
injected clock as archive temporary-name generation. Existing headless
workflow tests cover fixed-time behavior, cancellation, rollback, and
controlled platform failures; the focused clock-composition rerun passed.

- [x] Audit each engine workflow that needs time, cancellation, resources,
  networking, signatures, logging, or process launch. Inject the appropriate
  contract where the workflow semantics depend on it, or record an explicit
  later-phase owner instead of leaving the contract test-only.
- [x] At minimum close deterministic-time coverage for calendar import and
  class-transfer timestamps; review archive/temp-name time sources for the
  same requirement. Ensure Qt and future WinUI composition passes adapters at
  the boundary.
- [x] Extend headless tests from testing the fake interfaces in isolation to
  testing the affected engine workflows with fixed clocks, cancellation, and
  controlled platform failures.

Evidence: `src/engine/include/classmngr/engine/platform_services.h` and
`src/core/platform/qt_platform_services.h` define the contracts/adapters.
Calendar import and class transfer now receive `QtClock` at their retained
adapter boundaries. The Qt ZIP adapter also passes `QtClock`, and
`ZipArchiveWriter` constructs `StandardFileSystem` with the injected clock for
atomic finalization. Source-file modification times remain source metadata;
the clock controls fallback timestamps, temporary names, and replacement
operations. `ClassMngrEngineCalendarEventImportServiceTests`,
`ClassMngrEngineClassTransferServiceTests`, and
`ClassMngrEngineZipArchiveWriterTests` passed in the focused rerun.

### P2-R07 — Close the cross-platform and clean-build exit gate

**Status (2026-09-05): Deferred; Windows local matrix complete, cross-platform
retained-Qt evidence pending.**
The reproducible matrix is executable through
`.github/workflows/phase2-exit-gate.yml` and
`scripts/phase2_exit_gate.py`, with the process documented in [the exit-gate
runbook](../../docs/porting/windows-winui/phase2-exit-gate-runbook.md). A fresh
local Windows run now has runtime-tested PASS reports for all four Qt-free
x64/x86 Debug/Release engine lanes and the retained Windows Qt 6.12.0 x64
lane. The Linux and macOS retained-Qt lanes are explicitly host-blocked on
this Windows device because their exact Qt prefixes are unavailable, so the
aggregate remains red. Since this is currently an unofficial port, the
evidence gate remains deferred rather than treated as a current release
blocker.

- [x] Replace the focused exception in the P2-07 record with a complete,
  reproducible headless x64/x86 matrix, or explicitly repair/update the
  registered test set so an unfiltered run has no missing binaries. The fresh
  runner reports register and execute 56 `ClassMngrEngine*` tests in each
  Windows lane with no missing or unexecuted engine binaries.
- [ ] Record retained-Qt adapter results on the required Qt version and keep
  compile-only or host-blocked results separate from runtime parity. Add
  macOS/Linux Qt fixture evidence, or identify the CI job that owns that
  direction, for every migrated persistence slice. The Windows retained-Qt
  report is now exact Qt 6.12.0 runtime-tested; the Linux and macOS reports
  remain host-blocked with explicit missing-prefix reasons and require CI
  evidence.
- [ ] Include invalid input, rollback, migration, busy/locked database, and
  partial-failure assertions in the same release evidence used for the Exit
  Gate. The four Windows engine reports record all five coverage categories as
  covered by `ClassMngrEngineDatabaseFixtureRoundTripTests`; the complete
  cross-platform release evidence remains pending with the blocked retained-
  Qt lanes.

Evidence: `docs/porting/windows-winui/phase2-local-validation.md` records
the exact 2026-09-05 Windows matrix and retained-Qt report paths, the
host-blocked Linux/macOS results, the aggregate validator outcome, and the
MSVC FileTracker host limitation. The plan's Validation section still requires
cross-platform fixture readability and a complete migrated-feature matrix;
P2-R07 is not complete until the CI-owned retained-Qt lanes produce
runtime-tested reports and the aggregate is non-red.

### P2-R08 — Remove or quarantine superseded Qt validation helpers

**Status (2026-09-04): Production quarantine implemented; compatibility test validation pending.**
Committed as `6a9ec28`. The helpers are no longer part of production domain
linkage and are retained only for the legacy shared-policy test target.

- [ ] Confirm the source audit remains clean for the apparently unused
  `SharedValidation` and `ScheduleValueParser` APIs. Remove them or mark them
  compatibility-only after verifying no generated or external build target
  depends on them.
- [ ] Keep only Qt validation helpers that translate engine issues or own
  presentation-only policy; do not leave a second live schedule/name rule
  implementation in the Qt domain layer.

Evidence: `src/domain/validation/shared_validation.cpp` is the only caller of
`src/domain/rules/schedule_value_parser.cpp`, and the current source search
finds no production call site for `SharedValidation`. The live engine
equivalents are `ClassTimeValidator`, `RosterValidator`, and
`SpeakingEvaluationValidator`.

### P2-R09 — Extract calendar recurrence workflows

**Status (2026-09-05): Implemented; engine and retained-Qt recurrence
validation passed.**

The recurrence workflow is now owned by `CalendarEventService`; the calendar
page retains only dialog/prompt, localization, and UUID responsibilities.

- [x] Add Qt-free engine operations for creating/expanding a repeat series and
  updating a series from a selected date. The contract must own recurrence
  frequency/interval semantics, occurrence limits and date bounds, duration
  propagation, month-end behavior, and persistence/transaction decisions.
- [x] Keep dialog choices, localized labels, presentation prompts, and any
  adapter-only identity/UUID translation outside the engine. Make identity
  ownership explicit in the engine contract so Qt and WinUI cannot generate
  different product behavior.
- [x] Add headless tests for daily, weekly, and monthly series; month-end
  clamping/termination; invalid bounds; duration propagation; series edits;
  rollback; and representative Qt-to-engine round trips.

Evidence: `src/engine/calendar_event_service.cpp` owns expansion, month-end
clamping, duration propagation, and transactional create/update operations.
`src/data/repositories/calendar_event_repository.cpp` and
`src/app/services/feature_services.cpp` provide retained-Qt adapters, while
`src/features/calendar/ui/calendar_page_events.cpp` no longer implements
recurrence mutation. `ClassMngrEngineCalendarEventServiceTests` covers the
headless cases and `ClassMngrCalendarEventRepositoryTests` covers the Qt
round trip; both pass.

### P2-R10 — Canonicalize shared policy catalogs and roster projections

**Status (2026-09-05): Implemented for the current catalog/projection slice;
focused engine and retained-Qt validation passed.**

- [x] Expose engine-owned catalogs/constants for teacher display-name choices,
  testing-class grades/levels, roster base columns, calendar event types and
  time statuses, and speaking-evaluation names, dimensions, score values, and
  comment limits. Route the corresponding Qt model helpers through those
  contracts and add parity tests.
- [x] Add an engine roster projection/count operation with an explicit
  definition of a populated student row. Align class details, My Classes, sub
  prep, roster validation, and speaking-dashboard counts with that definition.
- [x] Consolidate duplicate engine ordering helpers, including the teacher
  display comparator used by sub-prep and class naming, or document why a
  distinction is intentional.
- [x] Keep colors, headers, widths, row highlights, and other rendering-only
  metadata in Qt/WinUI adapters.

Evidence: engine catalogs in `src/engine/include/classmngr/engine/` now feed
the Qt model helpers, and `RosterService::studentCount()` defines populated
rows as those with non-blank English or Korean names. Class details, My
Classes, sub-prep, roster, and speaking-dashboard paths use the shared count;
teacher ordering is shared by class naming and sub-prep. The full Qt-free
engine suite (57/57) and the four focused retained-Qt model/repository tests
pass.

### P2-R11 — Extract database initial-setup and recovery workflow

**Status (2026-09-05): Implemented; failure-injection and adapter compilation
validation passed.**

- [x] Define a portable database lifecycle workflow for creating a database,
  backing up an existing profile, restoring after cancellation or failure,
  and cleaning up incomplete files. Use the engine filesystem/atomic-replace
  contracts and typed results; extend those contracts if rename/move semantics
  are required.
- [x] Add failure-injection tests for backup, setup completion, cancellation,
  restore, and cleanup, including preserving the original database on failure.
- [x] Leave standard-directory discovery, path selection, dialogs, and user
  confirmation in the Qt/WinUI adapters.

Evidence: `DatabaseLifecycleService` and the non-overwriting
`FileSystem::moveFile()` contract own the portable transitions and typed
failures. `FileController` supplies selected paths and UI, then delegates
backup, completion, cancellation, restore, and incomplete-file cleanup. The
engine lifecycle test covers injected backup, completion, cancellation,
restore, and cleanup failures; it passes together with the full engine suite.

## Recommended completion order

Complete each step with its focused engine tests and retained-Qt adapter
round trip before moving to the next; P2-R07 is the final aggregate gate.

1. **P2-R04 — Establish engine-first composition.** Finish the shared-session
   service graph, remove or quarantine production `DataService` fallbacks, and
   assign a retirement owner for the compatibility facade.
2. **P2-R05 — Finish database ownership cleanup.** Resolve the duplicate
   path-backed SQLite connections, define the `:memory:` mode, and retire the
   remaining Qt SQL/schema path.
3. **P2-R11 — Make database recovery portable.** Build on the now-defined
   filesystem/session boundary and cover setup rollback and restore behavior.
4. **P2-R06 — Close platform-service composition.** Complete fixed-clock,
   cancellation, and controlled-failure workflow tests, including archive and
   temporary-file metadata decisions.
5. **P2-R09 — Move recurrence workflows into the engine.** Implement and test
   repeat-series expansion and update semantics against the engine database
   boundary.
6. **P2-R03 — Finish teacher-import policy.** Move all candidate/identity/date
   decisions into the engine while retaining source-row diagnostics in Qt.
7. **P2-R10 — Canonicalize shared policy and projections.** Centralize the
   catalogs, then define one roster-population/count semantic for all clients.
8. **P2-R01 and P2-R02 — Complete integration and parity validation.** Verify
   the dashboard, interactive validation, localized issue mapping, and
   selected/All or normalized-name cases through the retained adapters.
9. **P2-R08 — Remove or quarantine superseded helpers.** Do this after the
   replacement contracts and parity cases are green, so cleanup cannot hide a
   missing rule.
10. **P2-R07 — Run the complete exit gate.** Obtain CI/device-owned Linux and
    macOS retained-Qt evidence, include invalid/rollback/migration/busy and
    partial-failure coverage, and regenerate a non-red aggregate report.

## Audit classifications intentionally outside Phase 2

The audit did not add the following to the open-slice list because the plan
already assigns them to an adapter or later phase: campus JSON and workbook/
JSON/XML/ZIP decoding, `QResource` mounting and resource staging, PDF/print/
PowerPoint drawing and Office automation, updater/network implementation, and
WinUI screen/control migration. Those boundaries should remain thin and
documented; moving their implementation is not required to close the
portable-engine extraction gate.

## Validation

- Configure-time and source audits reject Qt, WinUI, WinRT, and Win32 UI
  dependencies in engine public or private code.
- Headless x64 and x86 engine tests cover rules, migrations, rollback, imports,
  and report models without loading a UI framework.
- Windows writes are readable by macOS/Linux Qt, and Qt writes are readable by
  Windows, across every supported fixture.
- Migrated Qt features pass existing tests through adapters before duplicate
  implementations are removed.

## Exit Gate

The engine can open and migrate databases, execute representative CRUD and
import workflows, and produce report models without Qt or Windows UI code.
Both presentation stacks consume the same implementation for migrated slices.

The Hangul composer and WinUI screen/control migration are not Phase 2
extraction targets; they remain shared-UX or later presentation work. Full
replacement of Qt PDF/printing and OS updater packaging is likewise deferred
until the later platform/presentation phases, although their Phase 2 engine
contracts may be defined here.
