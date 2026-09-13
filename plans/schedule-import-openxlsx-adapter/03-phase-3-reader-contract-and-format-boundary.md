# Phase 3 — Reader Contract and Format Boundary

**Previous:** [Phase 1](01-phase-1-dependency-control-and-provenance.md)

**Next:** [Phase 4](04-phase-4-qt-free-schedule-format-interpreter.md)

## Goal

Define a stable, testable boundary between workbook decoding and schedule
interpretation so that OpenXLSX and Qt are interchangeable adapters rather
than leaking into import domain code.

## Status

Implemented in Phase 3. The Qt-free engine now publishes a value-owned layout
model and a `ScheduleWorkbookReader` interface returning the existing native
`ScheduleImportWorkbook`/`Result` types. A contract test exercises fake-reader
substitution, cancellation, worksheet visibility/layout values, and the
standard-library-only boundary.

## Scope

- Specify `ScheduleWorkbookReader` and its result/error/cancellation behavior.
- Specify the codec-neutral raw workbook model that retains the information the
  schedule layout rules require.
- Identify ownership boundaries among platform adapters, shared format logic,
  engine import services, and WinUI dialog state.
- Define test seams for deterministic UI and parser tests.

## Non-Goals

- Full Qt parser extraction (Phase 4).
- Reading an actual XLSX file with OpenXLSX (Phase 5).
- Rewriting reconciliation or atomic import apply behavior.

## Proposed Public Contract

The exact result/error types should follow project conventions, but the public
surface should be equivalent to:

```cpp
class ScheduleWorkbookReader {
public:
    Result<engine::ScheduleImportWorkbook> read(
        const std::filesystem::path& file,
        engine::ScheduleImportKind importKind,
        Cancellation cancellation) const;
};
```

Rules:

- The public header uses standard and project types only; it exposes no
  `OpenXLSX`, Qt, ZIP, XML, or WinRT type.
- `read()` never mutates the source workbook and never persists imported data.
- Errors distinguish at least: inaccessible path, unsupported file/signature,
  unreadable/corrupt workbook, no compatible worksheet, malformed schedule
  layout, cancellation, and unexpected internal failure.
- File paths and user-visible messages are sanitized at the platform boundary.
- Cancellation is cooperative. If library document open cannot be interrupted,
  its completed result is discarded when cancellation is observed.

## Codec-Neutral Layout Model

Before schedule interpretation, codecs normalize into a value model such as
`ScheduleWorkbookLayout`:

- workbook sheets with name and visibility state;
- non-empty cells with 1-based row/column coordinates, string value, optional
  note, and a normalized style reference/value;
- merged-cell ranges;
- normalized style information needed by existing rules: fill color, font
  color, filled state, and bold state;
- workbook-level diagnostics for recoverable decoding omissions.

The model must capture semantic facts, not library object lifetimes. It must be
cheap to construct, movable, and safe to use after its decoder document closes.

## Boundary Ownership

| Layer | Owns | Must not own |
| --- | --- | --- |
| Qt/OpenXLSX adapter | Opening files and mapping workbook APIs to raw layout | Import persistence or UI controls |
| Format interpreter | Worksheet/layout rules and diagnostic classification | ZIP/XML library APIs |
| `ClassMngrEngine` | Import domain model, preview, conflict resolution, atomic apply | XLSX decoding library |
| WinUI | Dialog state, background work, selected sheet/user, message rendering | Workbook layout heuristics |

## Work Breakdown

1. Trace the data expected by `ScheduleImportService` and the current WinUI
   source/review dialog fields.
2. Catalogue every Qt parser dependency on workbook cell, style, merge, and
   worksheet metadata.
3. Draft the raw layout model and reader result/error types in the Qt-free
   engine include boundary selected for the project.
4. Decide whether the reader returns a final `ScheduleImportWorkbook` directly
   or an internal layout plus an interpreter. The preferred public result is the
   final engine model; raw layout stays implementation/internal-test scope.
5. Add a fake/in-memory reader implementation usable by WinUI dialog tests.
6. Document error-to-dialog-state mapping: failed Load retains source controls
   and disables Next; a successful compatible workbook enables schedule
   selection; only a selected compatible schedule enables Next.

## Acceptance Tests to Design Now

- A fake reader can produce a workbook with multiple visible sheets and users;
  UI code does not know the workbook codec.
- A cancellation result cannot populate UI fields after the dialog is closed or
  a later file has been chosen.
- An invalid workbook result contains no importable schedule data.
- A reader implementation can be replaced without recompiling engine import
  logic.

## Exit Criteria

The team can write the shared interpreter and an OpenXLSX adapter against an
explicit contract, and the WinUI dialog can depend on a fake reader in tests.
No later phase needs to decide where third-party types are allowed.

Phase 3 evidence: `classmngr/engine/schedule_workbook_layout.h` and
`classmngr/engine/schedule_workbook_reader.h` contain no Qt/OpenXLSX/WinRT
types; `ClassMngrEngineScheduleWorkbookReaderContractTests` builds as a
Qt-free engine test when the host compiler is available. The configured MSVC
test build was attempted, but the host's existing `Microsoft.Build.Utilities`
`FileTracker` access-denied failure stopped `ClassMngrEngine` before compiling
the test.
