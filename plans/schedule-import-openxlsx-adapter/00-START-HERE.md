# Schedule Import: OpenXLSX Adapter Plan

## Purpose

Replace the current WinUI schedule-import source fallback with a real, native
`.xlsx` reader built on OpenXLSX, without coupling `ClassMngrEngine` to either
OpenXLSX or Qt. The existing Qt importer remains the behavioral reference until
fixture parity is demonstrated.

This plan intentionally follows the import flow already implemented in the UI:

1. choose an XLSX file;
2. load and validate its workbook;
3. choose import kind and schedule worksheet/user;
4. build a preview and reconcile conflicts;
5. apply the reviewed import atomically.

The source-selection dialog must not navigate to a page. It continues to be a
modal dialog; only the data behind its states changes.

## Current State

- The WinUI dialog state machine is present, including source selection,
  schedule selection, review/reconcile, Back, Cancel, and Import.
- `loadScheduleImportSource()` currently checks that an `.xlsx` path is
  readable, then supplies normalized fallback import data rather than decoding
  the workbook.
- Qt contains the established workbook and schedule-format behavior in
  `src/features/schedule/import/schedule_workbook_parser.cpp`, backed by
  `src/features/calendar/calendar_workbook_reader.cpp`.
- The local OpenXLSX source is available at `C:\Git\openxlsx`. Its visible
  CMake metadata says 0.5.2 while its local `vcpkg.json` says 0.5.1, and the
  directory has no Git metadata. It is therefore an evaluation source, not yet
  a reproducible dependency source.

## Target Architecture

```text
WinUI import source dialog
        |
        v
ScheduleWorkbookReader (platform adapter; no public OpenXLSX types)
        |
        v
ScheduleWorkbookLayout (codec-neutral raw workbook representation)
        |
        v
Qt-free schedule-format interpreter
        |
        v
engine::ScheduleImportWorkbook
        |
        v
ScheduleImportService -> preview / validate / atomic apply

Qt importer
        |
        +--> existing Qt workbook decoder -> ScheduleWorkbookLayout -> same interpreter
```

The engine owns import domain models and import decisions. Codecs own file
decoding. The format interpreter owns spreadsheet-layout semantics. WinUI owns
dialog state, asynchronous work, and presentation of diagnostics.

## Phase Order

| Phase | Deliverable | Depends on |
| --- | --- | --- |
| [1](01-phase-1-dependency-control-and-provenance.md) | Reproducible, licensed OpenXLSX source policy | none |
| [2](02-phase-2-winui-build-integration-spike.md) | WinUI/MSBuild can build and link the codec predictably | 1 |
| [3](03-phase-3-reader-contract-and-format-boundary.md) | Stable codec-neutral reader/layout contract | 1 |
| [4](04-phase-4-qt-free-schedule-format-interpreter.md) | Shared Qt-free interpretation behavior and regression tests | 3 |
| [5](05-phase-5-openxlsx-reader-and-fixture-parity.md) | OpenXLSX adapter with true workbook fixture parity | 2, 4 |
| [6](06-phase-6-winui-dialog-integration.md) | Real workbook-backed modal source dialog | 5 |
| [7](07-phase-7-hardening-performance-and-cutover.md) | Hardened, measured, releasable native import | 6 |

## Invariants for Every Phase

- Keep external library types out of public engine and dialog headers.
- Do not alter a workbook while reading it; an import is read-only until the
  user explicitly confirms the final Import action.
- Preserve the Qt format semantics before intentionally changing them.
- Errors must describe the failed user action and file safely; raw library
  exceptions must not escape UI event handlers.
- Do not retain or import fallback classes if workbook decoding fails.
- Keep lengthy parsing off the UI thread and discard stale results after a new
  file is chosen or the dialog closes.
- Build and stage the actual supported WinUI configurations, rather than only
  the root CMake target.

## Decision Gates

1. **Dependency gate:** choose a pinned source and transitive-dependency model
   before adding OpenXLSX includes to product code.
2. **Build gate:** prove the actual `.vcxproj`/PowerShell/MSBuild route works
   for all release architectures before parser work depends on it.
3. **Parity gate:** make native fixture outputs match the Qt reference before
   changing the WinUI source dialog.
4. **Release gate:** complete robustness and performance evidence before
   removing the temporary source fallback from supported paths.

## Definition of Done

The shipped WinUI importer reads supported regular and intensive workbooks,
shows only decoded selectable schedules/users, drives the existing review and
reconciliation dialogs with real import data, reports invalid workbooks without
inventing data, and applies only the user-reviewed plan. Qt and native adapter
fixtures agree on the canonical `ScheduleImportWorkbook` result and diagnostic
classification.
