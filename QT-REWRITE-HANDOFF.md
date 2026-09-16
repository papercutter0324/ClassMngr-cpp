# Qt Rewrite Handoff

Paused on 2026-09-16 (Asia/Seoul) at the user's request. Continue on the
`Qt-Rewrite` branch from the commit containing this file. The Calendar Import
parser-failure boundary is not accepted yet; do not begin Phase 1.

## State at handoff

- Prior commits:
  - `ded8b5dc089110e5348cb9b9511d17ca770a795a` — `Phase0 - Add Calendar Import parser-failure boundary`
  - `9fa2ba4a54371a5597895061d52e8ec4eae3e163` — `Phase0 - Record Calendar Import verification handoff`
- Handoff commit: `a8cccb35d09a7888488f781be3c6c2dfafebf888` — `Phase0 - Save Calendar Import device handoff`
- The current uncommitted test change guards the two normal-path screenshot
  assertions in `tests/startup_performance_tests.cpp` when the configured
  Calendar Import route is expected to fail.
- The generated directory
  `docs/qt-rewrite/visual-baseline/release/large-calendar-import-error-boundary/`
  contains the current error/loading screenshots, generated workbook,
  malformed response, manifest, process logs, and workflow trace. It is
  preserved as diagnostic evidence, not acceptance evidence. The latest set is
  incomplete: the focused route stopped because the required
  `large-calendar-import-workflow.json`/completed metrics evidence was not
  available.
- No further build or test was run after the stop request.

Read these first:

```powershell
Get-Content -Raw .\QT-REWRITE-HANDOFF.md
Get-Content -Raw .\agent_docs\latest_session_work.md
Get-Content -Raw .\plans\qt-rewrite-heavy-route-plan\00-Start-Here.md
git log -3 --oneline
```

## Next-device verification

Open an x64 Visual Studio developer PowerShell, or initialize one with:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -NoLogo
```

If script execution policy prevents that command, use the corresponding x64
Developer Command Prompt or invoke the script with an appropriate
`ExecutionPolicy Bypass` option. Confirm Ninja is available:

```powershell
where.exe ninja
```

From the repository root, rebuild the two required targets:

```powershell
ninja -C build/qt-rewrite-calendar-verify-ninja -j 2 ClassMngr ClassMngrStartupPerformanceTests
```

Run the focused expected-failure route with a fresh settings/output root:

```powershell
$verifyRoot = Join-Path (Get-Location) "artifacts\qt0-calendar-next"
New-Item -ItemType Directory -Force -Path $verifyRoot | Out-Null
$env:CLASSMNGR_TEST_APP_PATH = (Resolve-Path ".\build\qt-rewrite-calendar-verify-ninja\ClassMngr.exe").Path
$env:CLASSMNGR_SETTINGS_ROOT = Join-Path $verifyRoot "settings"
New-Item -ItemType Directory -Force -Path $env:CLASSMNGR_SETTINGS_ROOT | Out-Null
$env:CLASSMNGR_STARTUP_CALENDAR_IMPORT_EXPECTED_FAILURE = "1"
$env:CLASSMNGR_LARGE_CALENDAR_IMPORT_ERROR_BOUNDARY_REFERENCE_DIR = (Resolve-Path ".\docs\qt-rewrite\visual-baseline\release\large-calendar-import-error-boundary").Path
& ".\build\qt-rewrite-calendar-verify-ninja\ClassMngrStartupPerformanceTests.exe" capturesLargeCalendarImportBoundaryWhenConfigured
```

The expected-failure run must produce usable metrics/manifest/trace evidence,
show the real import error, re-enable controls, preserve the unchanged-event
invariants, omit forbidden success checkpoints, and pass the release-ordering
and memory assertions. If it passes, clear the expected-failure opt-in and run
the normal success route and then the full startup-performance suite. Manually
inspect `calendar-import-error.png` before treating the Phase 0 evidence as
accepted. Then update the Phase 0 handoff/verification documents and make the
final evidence commit.

Do not make v2 memory claims from this work. The remaining manual check is the
visual inspection of the error screenshot; all other listed checks are
intended to be automated once the focused route has complete inputs and output
artifacts.
