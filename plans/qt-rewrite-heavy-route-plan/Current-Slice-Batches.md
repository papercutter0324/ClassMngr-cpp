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

Work through these 18 candidates in order; none is marked complete:

1. **F01** — Local Mac comparison passed 3/3 targets; Windows case-level
   attribution remains unresolved for the later native Windows slice.
2. **F02** — Reconcile the historical 72 failures with later local stable
   full-suite passes and the current Windows run.
3. **F03** — Recover test contexts from dirty artifact logs; assign explicit
   later-phase owners or record remaining uncertainty.
4. **W01** — Missing keyboard light icons; warning count and target-set
   discrepancy pending. Resource-registration work has not begun; the keyboard
   executor is paused without edits.
5. **W02** — `EventCalendar.qml` missing test resource.
6. **W03** — Combo-arrow resources.
7. **W04** — `QSqlQuery::prepare` database-not-open warning; SQL investigation
   is in progress.
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
