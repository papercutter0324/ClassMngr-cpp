# Current Slice Batches

## Phase 8a — Batch 2, sub-batch 1 (active)

Current clean `HEAD` and `origin/Qt-Rewrite` are `f3d4badc`; the source
candidate for the local Windows run is `b799c8ab`. Commit `f3d4badc` adds the
Windows run record only. Failure attribution precedes QWARN triage. Use exact
warning bodies and test contexts; target-only observations do not establish a
case or cause. Do not suppress warnings, and assign later-phase ownership
explicitly. Remote Actions will not be awaited; prefer local Mac checks where
applicable. See the [Phase 8a plan](09.5-Phase-8a-CI-Failure-and-QWARN-Triage.md)
and [progress log](09.5-Phase-8a-Progress-Log.md) for requirements and evidence.

F01–F03 are reviewed as triage; W01–W05 are implemented or dispositioned with
local macOS evidence; W04 and W06 are handed to Phase 11 for test-support and
fixture-schema corrections; W07 is dispositioned to Phase 11 test/startup
harness work. Windows case-level attribution, historical target-only
uncertainty, and cross-host runtime for W01–W03 and W05 remain open. W08 is
complete and dispositioned with local macOS evidence; W09 is dispositioned with
bounded macOS coverage. W10 is dispositioned with bounded local macOS evidence;
W11 is next. The Phase 8a gate remains open. Continue through the ordered 18
candidates:

1. **F01** — Local Mac comparison passed 3/3 targets; Windows case-level
   attribution remains unresolved for the later native Windows slice.
2. **F02** — Reconciled; historical causes remain open. Mac/Linux snapshots
   did not reproduce the old failures; 51 target-only observations remain
   unresolved, and three Windows targets need native case-level follow-up.
3. **F03** — Recovered last-output contexts, not terminal timeout failures.
   Assign only named visual captures to Phase 10; keep timeout and Windows
   Bootstrap ownership unresolved.
4. **W01** — Implemented and locally verified on macOS: all 26 warning-bearing
   targets register keyboard light/dark and spin icons. Focused CTest passed
   29/29 with those resource warnings absent. Windows/Linux runtime was not run.
5. **W02** — Implemented and locally verified on macOS: the existing calendar
   helper registers `EventCalendar.qml` and `MonthGridDelegate.qml` in all five
   affected test executables. Focused CTest passed 5/5; Windows/Linux runtime
   was not run.
6. **W03** — Implemented and locally verified on macOS: four targets register
   both combo-arrow SVGs; the fresh log has zero light/dark references and zero
   missing-resource Qt SVG warnings. Focused CTest passed 4/4. Windows/Linux
   runtime was not run.
7. **W04** — Dispositioned to Phase 11 test-fixture/support correction. A
   representative stack traces the named prepare warning to a static default
   database handle; its 123 occurrences span 44 contexts. A separate `exec`
   warning occurs once per Mac/Linux baseline with only a static path and no
   runtime stack. Keep counts distinct and both warnings visible.
8. **W05** — Implemented and locally verified on macOS: both focused CTest
   targets passed (51/51 and 11/11); the fresh log has no connection-in-use
   warning. The three remaining unknown-field warnings map to W06; one settings
   warning maps to W08. Windows/Linux runtime was not run.
9. **W06** — Dispositioned to Phase 11 fixture-schema correction: the test-only
   `teachers` view omits `birthday`, `phone_number`, and `notes`, causing three
   QWARNs per Mac/Linux baseline (six total) in the asserted malformed-JSON
   case. No Phase 8 source change or warning suppression.
10. **W07** — Dispositioned to Phase 11 test/startup-harness correction; the
    `QTEST_APPLESS_MAIN` test creates `ApplicationServices`. Stable Mac has four
    QWARNs across three contexts; Linux rows are from a source-unstable run only.
    `QTEST_GUILESS_MAIN` is a candidate; no source change or suppression.
11. **W08** — Complete/dispositioned: 69 settings QWARNs across 14 bodies and
    15 contexts are asserted fault-path diagnostics. Local macOS CTest passed
    26/26; QtTest 344/0/1 (the skip is an unconfigured optional capture). No
    filters or suppression; current Linux/Windows runtime confirmation is absent.
12. **W09** — Dispositioned as visible offscreen-plugin diagnostics; 326
    warnings were recorded across partial macOS runs, but coverage is incomplete.
    ENOSPC left 40 expanded entries without binaries, an unrelated wizard crash
    stopped the available-entry run with 28 unrun, and no current Linux/Windows
    runtime confirmation exists. W10 has since been dispositioned; W11 is next.
13. **W10** — Dispositioned as Qt font-alias initialization/timing diagnostics;
    24 current local macOS observations across existing runs, without evidence
    of failed font loads or bad glyphs. No new tests due disk exhaustion; no
    current stable Linux/Windows confirmation. W11 is next.
14. **W11** — Startup stage, locale, and shutdown warnings.
15. **W12** — Injected schedule warning.
16. **W13** — Wide teacher ID warning; verify the existing fix is absent from
    current evidence.
17. **W14** — Invalid filename warning.
18. **W15** — Temporary-file / painter warnings.

When starting **W14** (the second-to-last candidate), search for further
candidates for the next batch. Add that batch only when its discovery starts.
