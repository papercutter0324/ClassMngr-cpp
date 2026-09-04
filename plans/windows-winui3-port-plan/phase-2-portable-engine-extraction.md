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

**Status (2026-09-04): Implemented; Qt integration validation pending.**
Committed as `ff9ac88`. The portable dashboard orchestration and headless
engine coverage are complete; the retained Qt/WinUI integration build remains
to be verified.

- [ ] Move the policy in
  `SpeakingEvaluationService::analyticsDashboard()` from
  `src/app/services/feature_services.cpp` into a Qt-free engine dashboard
  service/model. The engine boundary must own `All` selection, canonical
  evaluation iteration, current-roster filtering, current versus historical
  snapshot scope, cross-evaluation aggregation, latest fully-scored
  class-shape selection, and year-to-date point generation.
- [ ] Leave the Qt service and `class_analytics_page` responsible only for
  database/input conversion, localization, and rendering. Add headless tests
  for empty and partial evaluations, selected versus `All`, roster filtering,
  latest-completed selection, and historical-cohort YTD behavior.

Evidence: `src/app/services/feature_services.cpp` currently implements the
dashboard orchestration in `SpeakingEvaluationService::analyticsDashboard`,
while `src/engine/include/classmngr/engine/speaking_analytics.h` exposes only
the underlying stateless calculations.

### P2-R02 — Unify interactive roster and speaking-evaluation validation

**Status (2026-09-04): Implemented; focused engine validation passed.**
Committed as `b4ff0c8`. Shared UTF-8 name validation and duplicate-pair policy
now feed the Qt utility and both engine validators.

- [ ] Add or expose an engine-facing per-row/per-cell name-validation
  contract, then route `RosterModel`, `SpeakingEvalModel`, their delegates,
  and questionable-length confirmation through it.
- [ ] Keep Qt responsible for mapping engine issues to localized messages,
  cell/row highlights, and confirmation UI. Remove the duplicate Qt
  character, length, and duplicate-name policy from live paths.
- [ ] Add parity cases for invalid ASCII/UTF-8 input, Korean suffixes and
  lengths, duplicate pairs, and normalized names so interactive feedback and
  save-time validation cannot diverge.

Evidence: `src/core/utils/student_name_utils.cpp` implements the active Qt
validation and duplicate-pair rules used by
`src/features/roster/ui/roster_model_validation.cpp` and
`src/features/speaking_eval/ui/speaking_eval_model.cpp`, while equivalent
rules independently exist in `src/engine/roster_validator.cpp` and
`src/engine/speaking_evaluation_validator.cpp`.

### P2-R03 — Finish the teacher-import candidate policy boundary

**Status (2026-09-04): Implemented; broader import validation pending.**
Committed as `49d6e56`. Template parsing no longer rejects duplicate source
candidates, while the engine remains responsible for plan policy and matching.

- [ ] Keep workbook decoding and template-specific extraction in
  `SectionedContactListTemplate`, but pass all decoded candidates to the
  engine for cross-candidate duplicate/identity validation and import
  matching. The engine must remain authoritative for stored-record
  ambiguity and plan validity.
- [ ] Preserve source-row diagnostics and localized presentation errors in
  the Qt adapter, and add fixtures for duplicate source candidates,
  ambiguous stored matches, and normalized birthdays/names.

Evidence: `src/features/teacher/import/sectioned_contact_list_template.cpp`
still performs `normalizedBirthday`, `cleanedKoreanName`,
`cleanedStaffName`, normalized identities, and duplicate rejection, while
`src/engine/teacher_import_service.cpp` independently validates the plan and
performs matching. Format-specific extraction remains intentionally Qt-owned;
the duplicated product policy does not.

### P2-R04 — Collapse legacy application-service fallbacks

**Status (2026-09-04): Partially implemented; migration validation pending.**
Committed as `b24f35e`. ApplicationServices now owns the session and production
feature services receive it directly; the legacy DataService API remains as an
explicit shared-session adapter until remaining fallback branches are retired.

- [ ] Replace the production `ApplicationServices -> DataService ->
  DatabaseSession` composition with an engine-first session/use-case graph,
  or make the compatibility object an explicit adapter with a retirement
  owner and deadline.
- [ ] Remove the `FeatureService` `DataService*` fallback branches and narrow
  `DataService` to compatibility/test callers after equivalent engine paths
  are proven. No new UI or controller code should add another facade method.
- [ ] Keep the existing Qt-facing service API stable during the transition,
  with tests proving that open/close and all migrated feature operations use
  the same engine-backed session.

Evidence: `src/core/application_services.cpp` still creates
`DataService` and obtains every feature service through its
`DatabaseSession`; `src/app/services/feature_services.cpp` retains
`DataService` fallback branches throughout the migrated operations.

### P2-R05 — Retire the remaining Qt SQL compatibility path

**Status (2026-09-04): Calendar-cache slice implemented; wider Qt SQL retirement open.**
Committed as `36be4ef`. CalendarEventCache now reads through the engine service;
DatabaseSession, DataService, and other Qt SQL compatibility consumers remain.

- [ ] Migrate retained database consumers, including the asynchronous
  `CalendarEventCache`, to engine `SqliteDatabase`/service access, or isolate
  them behind an explicitly temporary adapter boundary.
- [ ] Remove `DatabaseSession::database()`, the direct Qt transaction in
  `DataService::save()`, and the Qt schema/transaction helpers once the
  compatibility tests have moved to the engine path. Exact `:memory:` support
  must either be engine-backed or be clearly test-only and separately owned.
- [ ] Confirm that file-backed open, migration, CRUD, rollback, and cache
  reads no longer depend on a Qt SQL connection or duplicate schema logic.

Evidence: `src/data/database/database_session.h/.cpp` still exposes and
creates `QSqlDatabase`, exact `:memory:` opens still use
`DatabaseSchemaManager`, `src/data/data_service.cpp` calls
`m_session->database().commit()`, and
`src/features/calendar/ui/calendar_event_cache.cpp` creates a worker Qt SQL
connection. The engine already owns the corresponding file-backed SQLite
pipeline.

### P2-R06 — Make platform-service contracts live at engine boundaries

**Status (2026-09-04): Implemented for calendar import and class transfer; focused rerun pending.**
Committed as `00f8dee`. Both workflows use injectable clocks with SystemClock
compatibility; the final calendar-import fixture requires a clean rerun.

- [ ] Audit each engine workflow that needs time, cancellation, resources,
  networking, signatures, logging, or process launch. Inject the appropriate
  contract where the workflow semantics depend on it, or record an explicit
  later-phase owner instead of leaving the contract test-only.
- [ ] At minimum close deterministic-time coverage for calendar import and
  class-transfer timestamps; review archive/temp-name time sources for the
  same requirement. Ensure Qt and future WinUI composition passes adapters at
  the boundary.
- [ ] Extend headless tests from testing the fake interfaces in isolation to
  testing the affected engine workflows with fixed clocks, cancellation, and
  controlled platform failures.

Evidence: `src/engine/include/classmngr/engine/platform_services.h` and
`src/core/platform/qt_platform_services.h` define the contracts/adapters,
but `tests/engine/platform_services_tests.cpp` exercises fakes directly and
does not compose them into an engine workflow. Engine code still calls
`system_clock::now()` directly in `calendar_event_import_service.cpp` and
`class_transfer_service.cpp` (with additional time sources in
`zip_archive_writer.cpp` and `file_system.cpp`).

### P2-R07 — Close the cross-platform and clean-build exit gate

**Status (2026-09-04): Automation implemented; evidence gate remains open.**
The reproducible matrix is executable through
`.github/workflows/phase2-exit-gate.yml` and
`scripts/phase2_exit_gate.py`, with the process documented in [the exit-gate
runbook](../../docs/porting/windows-winui/phase2-exit-gate-runbook.md), but
fresh x64/x86 and macOS/Linux artifacts are still required.

- [ ] Replace the focused exception in the P2-07 record with a complete,
  reproducible headless x64/x86 matrix, or explicitly repair/update the
  registered test set so an unfiltered run has no missing binaries.
- [ ] Record retained-Qt adapter results on the required Qt version and keep
  compile-only or host-blocked results separate from runtime parity. Add
  macOS/Linux Qt fixture evidence, or identify the CI job that owns that
  direction, for every migrated persistence slice.
- [ ] Include invalid input, rollback, migration, busy/locked database, and
  partial-failure assertions in the same release evidence used for the Exit
  Gate.

Evidence: `docs/porting/windows-winui/phase2-local-validation.md` records
Windows x64/x86 headless and Qt 6.12 x64 focused fixture results, but also
records nine unrelated registered engine binaries missing from the existing
build trees, a host Qt 6.11.1 versus project Qt 6.12.0 mismatch, and an
MSBuild FileTracker limitation. The plan's Validation section still requires
cross-platform fixture readability and a complete migrated-feature matrix.

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
