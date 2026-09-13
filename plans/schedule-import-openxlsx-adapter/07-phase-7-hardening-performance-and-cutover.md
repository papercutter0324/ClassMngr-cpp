# Phase 7 — Hardening, Performance, and Cutover

**Previous:** [Phase 6](06-phase-6-winui-dialog-integration.md)

**Final phase:** [plan overview](00-START-HERE.md)

## Goal

Turn the working native import into a safe, measured, supportable release
feature and complete the cutover from the source fallback.

## Scope

- Input safety, cancellation, and error presentation.
- Representative performance and memory measurement.
- Full build/test/package verification.
- Dependency notice and release-readiness review.
- Explicit removal or isolation of temporary fallback behavior.

## Input and Failure Hardening

XLSX files are ZIP containers and should be treated as untrusted input. Apply
defenses at the earliest feasible layer without pretending OpenXLSX guarantees
limits it does not expose.

- Check extension as an affordance, not as the security decision; report invalid
  signatures/containers through the reader.
- Establish practical limits for file size, worksheet count, populated cells,
  merged ranges, string length, and parser work. Choose thresholds from real
  workbook evidence and document them.
- Reject or stop parsing structurally excessive inputs with a clear user error.
- Catch all library exceptions at the adapter boundary and convert them to
  stable reader errors; never show raw implementation stack text in the dialog.
- Keep parsing off the UI thread. When OpenXLSX cannot cancel an in-progress
  open, mark the request stale and discard its result immediately on return.
- Ensure no invalid, cancelled, or stale result can reach preview or Import.

## Performance Plan

Measure before setting release thresholds. Use representative workbooks:

- smallest supported regular and intensive files;
- a typical production schedule with multiple schedules/users;
- a large but still supported workbook;
- a deliberately excessive input to verify bounded failure.

For cold and warm runs, record at least wall-clock decode-to-model time and peak
working set. Compare against the current Qt reference where it can run the same
fixture. Record machine/build configuration with results; do not claim a
universal number from one developer machine.

Investigate only measured bottlenecks. Likely candidates are whole-sheet
materialization, repeated style lookup, repeated UTF conversion, and creating
missing cells during iteration. Any optimization must retain fixture parity.

## Cutover Checklist

1. Confirm Phase 5 fixture parity and Phase 6 end-to-end dialog tests.
2. Run all relevant engine, schedule-format, Qt compatibility, native-reader,
   and WinUI component tests.
3. Build and stage every supported Windows configuration (x64 Debug, x64
   Release, and Win32 Release if supported).
4. Perform manual smoke imports for a regular and intensive workbook through
   source dialog, review/reconcile, Back, Cancel, and Import.
5. Verify malformed/corrupt inputs never create fallback classes or alter data.
6. Confirm third-party notices and source provenance remain correct in the
   packaged build.
7. Remove production fallback code or make any retained fallback test-only and
   impossible to reach from file import.
8. Record acceptance evidence and known supported-template limitations in the
   project’s normal release/developer documentation.

## Rollback Strategy

Before general release, preserve the ability to ship the prior application
version if a critical decoder defect appears. Do not add a silent runtime switch
that imports invented data. If a feature flag is used during staged testing, it
must select only between the validated native reader and an explicit
"import unavailable" state, never the former normalized fallback.

## Release Gates

| Gate | Required evidence |
| --- | --- |
| Correctness | Native/Qt canonical parity on fixture catalogue; engine preview/apply tests pass |
| UI integrity | All dialog states, Back/Cancel, and stale-result behavior tested |
| Robustness | Corrupt/oversized/layout-invalid inputs fail safely with no state mutation |
| Performance | Representative timing and working-set results recorded and acceptable for target users |
| Packaging | Supported configurations build, launch, stage dependencies, and include notices |

## Exit Criteria

- The native reader is the only production path from XLSX selection to
  `ScheduleImportWorkbook`.
- No real file-import failure can produce synthesized fallback classes.
- Acceptance evidence covers correctness, robustness, UI responsiveness,
  performance, and packaging.
- Remaining limitations are explicit, user-safe, and documented rather than
  accidental behavioral gaps.

## Post-Cutover Maintenance

Future OpenXLSX upgrades repeat Phase 1 provenance checks, the Phase 2 build
matrix, and the Phase 5 fixture comparison before release. New spreadsheet
template variants first receive a fixture and interpreter test, then adapter
compatibility verification; they must not be supported by ad hoc WinUI logic.
