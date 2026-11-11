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

F01–F03 are reviewed as triage; W01–W03 are implemented and locally verified
on macOS only. Windows case-level attribution, historical target-only
uncertainty, and cross-host runtime for W01–W03 remain open. W04 is next;
continue through the ordered 18 candidates:

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
7. **W04** — `QSqlQuery::prepare` database-not-open warning remains unresolved;
   needs a focused debugger/backtrace check.
8. **W05** — `removeDatabase` connection-still-in-use warning.
9. **W06** — Unknown teacher fields.
10. **W07** — `QCoreApplication` / `applicationDirPath` startup warnings.
11. **W08** — Settings failures.
12. **W09** — Offscreen QPA / dialog warnings.
13. **W10** — Font fallback warnings.
14. **W11** — Startup stage, locale, and shutdown warnings.
15. **W12** — Injected schedule warning.
16. **W13** — Wide teacher ID warning; verify the existing fix is absent from
    current evidence.
17. **W14** — Invalid filename warning.
18. **W15** — Temporary-file / painter warnings.

When starting **W14** (the second-to-last candidate), search for further
candidates for the next batch. Add that batch only when its discovery starts.
