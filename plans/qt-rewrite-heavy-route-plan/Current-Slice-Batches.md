# Current Slice Batches

## Phase 8a — Batch 2 complete; Batch 3 Windows x64 local validation complete

Batch 2’s 18-slice sub-batch and final one-candidate W16 sub-batch are
complete. The first sub-batch was pushed at `cbb0939c`. W16 has local macOS
verification and is included in the Windows full-suite run below. No other
OS-agnostic slices were found.

Windows x64 Batch 3 local validation completed on 2026-11-12: a fresh Ninja /
MSVC Debug build succeeded, all 618 CTest targets passed with a 240-second
per-target timeout, the four Roster output cases passed, Schedule and Speaking
output evidence passed, and C53 matched all 12 Sub Prep and 5 Roster pages with
zero changed pixels. The test run was source-stable, but its working tree was
not clean: two test files had local Windows fixes. This is local evidence, not
the clean candidate snapshot required by the Phase 8a acceptance gate. The
repeatable `windows-x64-ninja-debug` preset was added and configure-validated;
the Windows baseline recorder now defaults to a 240-second test timeout
because StartupPerformance takes about 181 seconds here.

Phase 8a remains open because the clean candidate/source provenance and shared
Mac/Linux platform gates remain unresolved. Earlier stable platform runs use
different revisions (Mac `3dc37340`, Linux `7f82f427`, Windows `b799c8ab`).
See the [Phase 8a plan](09.5-Phase-8a-CI-Failure-and-QWARN-Triage.md) and
[progress log](09.5-Phase-8a-Progress-Log.md) for detailed evidence and limits.
