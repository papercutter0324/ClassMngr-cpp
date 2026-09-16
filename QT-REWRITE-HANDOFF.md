# Qt Rewrite Handoff

Deployment `qt0_calendar_verify_20260917` closure is `complete`. Phase 0
remains `In progress`: the Calendar Import parser-failure boundary is accepted,
but Phase 0 is not closed and Phase 1 must not start.
Continue on branch `Qt-Rewrite` from the main agent's reviewed worktree.

## State at handoff

- Prior commits:
  - `ded8b5dc089110e5348cb9b9511d17ca770a795a` — `Phase0 - Add Calendar Import parser-failure boundary`
  - `9fa2ba4a54371a5597895061d52e8ec4eae3e163` — `Phase0 - Record Calendar Import verification handoff`
- The screenshot-scroll fix is the only source delta after `ded8b5dc`.
- The error-boundary evidence directory below contains the completed
  manifest, metrics, trace, screenshots, workbook, and malformed response; it
  is accepted Phase 0 evidence.

Read these first:

```powershell
Get-Content -Raw .\QT-REWRITE-HANDOFF.md
Get-Content -Raw .\agent_docs\latest_session_work.md
Get-Content -Raw .\plans\qt-rewrite-heavy-route-plan\00-Start-Here.md
git log -3 --oneline
```

## Accepted Calendar Import parser-failure boundary

- A fresh Ninja Release configure/build completed at
  `build/qt-rewrite-calendar-verify-ninja` with Qt `6.12.0`/x64 MSVC;
  `ClassMngr` and `ClassMngrStartupPerformanceTests` linked.
- The focused expected-failure route passed with
  `CLASSMNGR_STARTUP_CALENDAR_IMPORT_EXPECTED_FAILURE=1` and a deterministic
  69-byte malformed local HTTP response.
- Retained failure evidence is under
  `docs/qt-rewrite/visual-baseline/release/large-calendar-import-error-boundary/`.
  Its manifest references `large-calendar-import-workflow.json`; trace and
  metrics record the real error
  `Import failed: The downloaded spreadsheet is missing xl/workbook.xml.`,
  re-enabled Import Events, unchanged events `0 -> 0`, operation release before
  failure observation, absent forbidden success checkpoints, workflow complete,
  and normal exit.
- The failure route completed at `workflow` `11,191 ms` and `settled-1s`
  `12,228 ms`. Peak working set/private usage was `410,251,264`/
  `452,120,576` bytes; these are legacy before-state measurements only.
- Manual inspection of `calendar-import-error.png` found the Calendar Import
  Import section, enabled Import Events, and the real error. An independent
  Tester confirmed both PNGs valid at `1020x735` and all checks.
- The unchanged success route passed, and
  `ClassMngrStartupPerformanceTests.exe -v1` with Calendar Import opt-ins
  cleared exited `0`. Normal success evidence is under
  `docs/qt-rewrite/visual-baseline/release/large-calendar-import-boundary/`.

## Remaining Phase 0 evidence gaps

- Windows ARM64, macOS universal, and Linux Release evidence.
- Remaining packaged feature editing/read-only/loading/error/warning states
  and golden report, substitute, roster, and PowerPoint outputs; external
  Office automation remains unexecuted in the Windows offscreen baseline.
- Final cross-platform and feature memory trend evidence; do not treat the
  retained legacy measurements as final `<250 MiB` acceptance or as v2 claims.

## Git handoff

The accepted slice is committed at `c0ebb484`:
`Phase0 - Accept Calendar Import parser-failure evidence`. The worktree was
clean after the commit. Continue Phase 0 with the remaining evidence gaps.
