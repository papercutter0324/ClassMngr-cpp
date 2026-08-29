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

## Validation

- Configure-time and source audits reject Qt, WinUI, WinRT, and Win32 UI
  dependencies in engine public or private code.
- Headless engine tests cover rules, migrations, rollback, imports, and report
  models without loading a UI framework.
- Windows writes are readable by macOS/Linux Qt, and Qt writes are readable by
  Windows, across every supported fixture.
- Migrated Qt features pass existing tests through adapters before duplicate
  implementations are removed.

## Exit Gate

The engine can open and migrate databases, execute representative CRUD and
import workflows, and produce report models without Qt or Windows UI code.
Both presentation stacks consume the same implementation for migrated slices.
