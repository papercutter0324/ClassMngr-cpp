# Initial Phase 0 baseline result

Source revision: `2c47675` (2026-08-23). These historical measurements are
retained as provisional context for the native-port work; they do not change
runtime behavior and must be re-collected for the current branch baseline.

## Windows x64 Release startup

The collector launched `build/windows-x64-release/Release/ClassMngr.exe` with
a fresh settings root for each GUI run. Raw metric/host metadata is retained in
`artifacts/phase0/windows-x64-release/run-00{1,2,3}/` (ignored build artifact).

| Run | Window constructed | Ready | Window-constructed working set | Window-constructed private bytes |
| --- | ---: | ---: | ---: | ---: |
| 001 | 1,751 ms | 3,602 ms | 713.2 MiB | 627.6 MiB |
| 002 | 1,509 ms | 3,347 ms | 713.2 MiB | 627.8 MiB |
| 003 | 1,765 ms | 3,460 ms | 713.5 MiB | 628.0 MiB |

The initial x64 budgets in
[performance-baseline.md](performance-baseline.md) are the worst sample plus
20% headroom, rounded up: 2,200 ms window construction, 4,500 ms ready,
900 MiB working set, and 800 MiB private bytes. The debug/offscreen CTest
startup scenario is deliberately not substituted for this GUI desktop baseline.

## Current Windows x64 Debug startup probe

Source revision: `3f2cf53` (2026-08-28). These samples were collected after
rebuilding the current HEAD. They are a current-branch diagnostic and do not
replace the approved Release budget above. `window-shown` is the first visible
frame checkpoint; `startup-complete` is the ready checkpoint. Memory values are
sampled at `startup-complete`.

Raw reports and host metadata are retained in
`artifacts/phase0/windows-x64-debug/run-00{7,8,9}/` (ignored build artifact).

| Run | Window shown | Startup complete | Working set | Private bytes |
| --- | ---: | ---: | ---: | ---: |
| 007 | 3,439 ms | 3,480 ms | 701.3 MiB | 598.3 MiB |
| 008 | 3,427 ms | 3,471 ms | 701.4 MiB | 598.5 MiB |
| 009 | 3,419 ms | 3,462 ms | 701.1 MiB | 598.2 MiB |

The three-run maximum peak was 703.7 MiB working set and 599.9 MiB private
bytes at 100% display scaling. No budget was changed from this diagnostic.

ARM64, navigation, resize, scrolling, first paint, PDF/report, and
device-recovery measurements remain pending. Those scenarios require the
fixture corpus and later native telemetry described in the Phase 0 plan.

## Windows ARM64 Debug compile baseline

On this x64 Windows build host, the installed Qt `6.11.1` ARM64 kit and Visual
Studio ARM64 compiler configured and built the existing Qt `ClassMngr` target:

```powershell
cmake --preset windows-arm64-debug
cmake --build build/windows-arm64-debug --config Debug --target ClassMngr --parallel 2
```

The resulting `build/windows-arm64-debug/Debug/ClassMngr.exe` is 49,259,008
bytes and has PE machine value `0xAA64` (ARM64). This establishes that the
current Qt target compiles and links for ARM64 with Windows SDK `10.0.26100.0`;
it is not a substitute for running tests, capturing UI evidence, or collecting
performance on ARM64 hardware.

## Unchanged Qt build baseline

After generating the test target's declared
`build/windows-x64-debug/test-campuses.rcc` input, `ctest --test-dir
build/windows-x64-debug -C Debug --output-on-failure` passed all 63 registered
Windows x64 Debug tests, including `ClassMngrDatabasePortFixtureTests`. The
earlier ResourcePack failure was not a product or test assertion regression:
the generated RCC file was absent from a stale build directory, so the test
could not copy its required input. The focused fixture test and the full suite
pass when that build input exists.

The schema, data-service lifecycle, imports, page manager, dialogs, print/PDF,
speaking-evaluation, startup-performance, and file-format tests passed in that
run. The same 63-test x64 Debug suite was revalidated after finalizing the
fixture corpus and Phase 0 contract tooling. This host has now compiled the
existing Qt product for ARM64, but Linux and Windows ARM64 runtime validation
remain unavailable and have not yet been performed.

## Current HEAD validation note

After rebuilding the affected targets at revision `3f2cf53`,
`ClassMngrCampusMapTests` passed. `ClassMngrScheduleImportDialogTests` remains
a reproducible existing Qt-baseline failure in
`reviewPreviewUsesSavedScheduleDisplaySettings`: the preview table has 6
columns where the test expects 8. No Phase 0 code was changed to mask this
failure; it must be resolved or explicitly dispositioned before claiming an
unchanged full-suite baseline.
