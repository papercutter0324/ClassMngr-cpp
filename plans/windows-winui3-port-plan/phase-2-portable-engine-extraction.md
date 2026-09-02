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

- [ ] Define engine-facing contracts for paths, byte streams, temporary files,
  directory creation, atomic replacement, file copy, and output existence.
- [ ] Route `DataService::saveAs()` and `exportAs()`, report output commits,
  ZIP/document output, and sub-prep package staging through those contracts.
- [ ] Return typed errors for invalid paths, missing resources, partial output,
  and failed atomic commits.

Done when engine workflows do not depend on `QFile`, `QDir`, `QSaveFile`, or
`QTemporaryDir`, and adapters can provide equivalent behavior on Windows,
macOS, and Linux.

### P2-03 — Finish retained Qt database and application-service adapters

- [ ] Audit every retained repository and make it a thin Qt-to-engine adapter,
  with no product rules duplicated in the Qt layer.
- [ ] Make `DatabaseSession` an explicit compatibility boundary: engine owns
  file-backed open/migration semantics, while Qt owns only its temporary SQL
  session until that adapter is retired.
- [ ] Route `ApplicationServices` and `DataService` workflows through engine
  use cases rather than repository/fallback paths.
- [ ] Remove or document compatibility branches once the engine path has
  equivalent coverage.

Representative code: `src/data/database/database_session.cpp`,
`src/data/data_service.cpp`, and `src/core/application_services.cpp`.

### P2-04 — Close import and file-codec boundaries

- [ ] Schedule workbook import: keep XLSX/OOXML parsing in the Qt adapter,
  but make conversion, typed errors, validation, cancellation, and apply
  semantics flow through engine services.
- [ ] Calendar workbook import: keep download and workbook parsing in Qt,
  while engine owns normalized events, rules, recurrence, and persistence
  decisions.
- [ ] Teacher import: finish the boundary between Qt file/template codecs and
  `TeacherImportService`; remove duplicated validation or matching rules.
- [ ] Class-transfer and campus JSON: isolate JSON/filesystem/resource access
  from the engine contracts and add representative codec fixtures.
- [ ] Audit `ScheduleImportMatcher`, `ScheduleImportPlanValidator`, and
  `ScheduleImportStateValidator`; route any live behavior through the engine,
  or remove/quarantine these apparently superseded duplicate implementations.

### P2-05 — Extract resource-pack and catalog policy

- [ ] Move resource-pack manifest parsing, precedence/fallback, integrity, and
  signature policy behind portable engine contracts.
- [ ] Keep `QResource`/RCC mounting, downloads, staging, and installation in
  Qt or another platform adapter.
- [ ] Put document-catalog and campus resource metadata/path checks behind the
  same portable boundary where they affect engine workflows.

Representative code: `src/core/resource_packs/resource_pack_manager.cpp`,
`src/core/resource_packs/resource_pack_manifest.h`,
`src/core/resource_packs/resource_pack_update_service.h`, and
`src/features/documents/document_catalog.cpp`.

### P2-06 — Introduce the remaining platform-service interfaces

- [ ] Define injectable interfaces for settings, networking, signature
  verification, process launch, clock, logging, resources, and cancellation.
- [ ] Keep Qt implementations as adapters and add WinUI implementations only
  for platform behavior.
- [ ] Ensure engine services can be tested headlessly with deterministic clock,
  in-memory or fixture-backed resources, and controlled cancellation/errors.

### P2-07 — Expand per-slice interoperability evidence

- [ ] For each migrated persistence slice, test open/migrate, CRUD or import,
  write, close, and reopen using the shared fixture corpus.
- [ ] Add explicit Qt-to-engine and engine-to-Qt round trips, including invalid
  input, rollback, migration, busy handling, and partial-failure cases.
- [ ] Run the headless engine matrix on x64 and x86 and record retained-Qt
  adapter results separately from compile-only or environment-limited checks.

Done when the fixture evidence covers the migrated slices individually rather
than only proving that the aggregate corpus can be opened.

### P2-08 — Retained-adapter and legacy-rule cleanup

- [ ] Search for Qt-side business rules that now have engine equivalents and
  either delete them, delegate them, or mark them as presentation-only.
- [ ] Resolve stale migration TODOs and compatibility comments in
  `DataService` after confirming the corresponding engine service is covered.
- [ ] Keep a short list of intentionally retained Qt responsibilities:
  rendering, printing, workbook/JSON codecs, resource mounting, filesystem
  commits, and Office/process automation.

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
