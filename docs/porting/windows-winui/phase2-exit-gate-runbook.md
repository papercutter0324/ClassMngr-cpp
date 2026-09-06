# Phase 2 exit-gate runbook

Status: Phase 2 seven-lane portable-engine exit gate complete (2026-09-06).
This is an unofficial port, so Linux remains supporting retained-platform
evidence rather than a Windows-port blocker. The automated full-gate path
remains available for future changes and official promotion.

## Automated entry point

Dispatch [`.github/workflows/phase2-exit-gate.yml`](../../../.github/workflows/phase2-exit-gate.yml)
from GitHub Actions. The workflow runs the four Qt-free Windows engine lanes,
the retained Qt 6.12.0 Windows x64 fixture, and the retained Qt Linux and
macOS fixtures. Each lane uploads its JSON report, CTest JUnit result, CTest
inventory, and separate configure/build/CTest logs. The aggregate job requires
the exact seven-lane set and rejects missing registered engine executables,
failed tests, missing artifacts, incorrect Qt metadata, compile-only or
blocked fixtures, and incomplete category evidence.

The required lane set is intentionally split as follows:

| lane group | required lanes | evidence allowed for aggregate |
| --- | --- | --- |
| Qt-free WinUI engine | x64 Debug, x64 Release, x86 Debug, x86 Release | runtime-tested only |
| retained Qt adapter | Windows Qt 6.12 x64, Linux Qt 6.12 x64, macOS Qt 6.12 universal | exact Qt 6.12.0 runtime-tested only |

The JSON report records `evidence_class` as `runtime-tested`, `compile-only`,
`host-blocked`, or `failed`. A compile-only report is an honest build result,
but it is not an exit-gate pass. A retained-Qt report is `host-blocked` when
the exact Qt prefix/version cannot be established; it must include the
blocking reasons. The four engine lanes remain separate from the three
retained-Qt lanes, including their x64/x86 results.

## Windows milestone profile

When Linux is intentionally deferred until the remaining Windows porting work
is complete, validate the Windows milestone with the five current Windows
lanes:

```powershell
python scripts/phase2_exit_gate.py validate-windows `
  --reports-dir artifacts/phase2 `
  --output artifacts/phase2/windows-aggregate-report.json
```

This profile requires the four Qt-free Windows x64/x86 Debug/Release engine
lanes and the retained Windows Qt 6.12.0 x64 fixture. A `PASS` is a Windows
milestone result only; it does not replace the full `validate` command or
claim Linux and macOS cross-platform completion. The profile deliberately
ignores deferred lane reports when they are present in the same reports
directory.

For a local lane, use the dependency-free runner from the repository root. A
Windows engine invocation is:

```powershell
$env:TrackFileAccess = 'false'
python scripts/phase2_exit_gate.py run `
  --lane-id windows-x64-winui-debug `
  --lane-type engine `
  --configure-preset windows-x64-winui-debug `
  --build-dir build/windows-x64-winui-debug `
  --configuration Debug `
  --output artifacts/phase2/windows-x64-winui-debug/windows-x64-winui-debug.json `
  --logs-dir artifacts/phase2/windows-x64-winui-debug/logs
```

The retained Windows Qt fixture additionally requires the exact Qt prefix:

```powershell
$env:TrackFileAccess = 'false'
$env:QT_MSVC_X64_PREFIX = 'C:\Qt\6.12.0\msvc2022_64'
python scripts/phase2_exit_gate.py run `
  --lane-id windows-qt-6.12-x64 --lane-type retained-qt `
  --configure-preset windows-x64-debug --build-dir build/windows-x64-debug `
  --configuration Debug --qt-version 6.12.0 --qt-architecture x64 `
  --qt-prefix-env QT_MSVC_X64_PREFIX `
  --output artifacts/phase2/windows-qt-6.12-x64/windows-qt-6.12-x64.json `
  --logs-dir artifacts/phase2/windows-qt-6.12-x64/logs
```

After collecting lane directories, validate the complete matrix with:

```powershell
python scripts/phase2_exit_gate.py validate `
  --reports-dir artifacts/phase2/reports `
  --output artifacts/phase2/aggregate-report.json
```

## Windows headless engine matrix

Configure each test-enabled preset from a fresh build directory:

```powershell
cmake --fresh --preset windows-x64-winui-debug
cmake --fresh --preset windows-x64-winui-release
cmake --fresh --preset windows-x86-winui-debug
cmake --fresh --preset windows-x86-winui-release
```

Build every registered target rather than using the matching build preset,
which builds only `ClassMngrWindowsWinUI`:

```powershell
cmake --build build/windows-x64-winui-debug --config Debug --clean-first --parallel 2 -- /p:TrackFileAccess=false
ctest --test-dir build/windows-x64-winui-debug -C Debug -R "^ClassMngrEngine" --output-on-failure
```

Repeat the build and CTest command with the matching build directory and
configuration for x64 Release, x86 Debug, and x86 Release. Archive the CTest
output with the build logs. The matrix must include invalid-input, rollback,
migration, busy/locked database, and partial-failure coverage from the
registered engine tests. The required unfiltered CTest inventory is collected
after the build and before the filtered runtime command. Every registered
`ClassMngrEngine*` test must have a resolved executable and a matching JUnit
runtime entry; missing or unexecuted binaries are failures, not omitted tests.
The current explicit category evidence test is
`ClassMngrEngineDatabaseFixtureRoundTripTests`, whose fixture suite exercises
all five categories. The report only marks a category covered when that test
is registered and actually executed.

## Retained Qt adapter fixture

Use the Qt 6.12 x64 desktop route (via `QT_MSVC_X64_PREFIX`):

```powershell
cmake --fresh --preset windows-x64-debug
cmake --build --preset windows-x64-debug --clean-first --parallel 1 --target ClassMngrDatabasePortFixtureGenerator -- /p:TrackFileAccess=false
ctest --test-dir build/windows-x64-debug -C Debug -R "^ClassMngrDatabasePortFixtureTests$" --output-on-failure
```

The CTest entry is named `ClassMngrDatabasePortFixtureTests`, but intentionally
executes the `ClassMngrDatabasePortFixtureGenerator` target. The automated
validator treats those as the test name and executable name respectively.

The Linux x64 retained lane uses the same fixture command with the exact Qt
6.12.0 prefix:

```bash
export QT_LINUX_PREFIX=/path/to/Qt/6.12.0/gcc_64
export QT_QPA_PLATFORM=offscreen
CC=/usr/bin/gcc CXX=/usr/bin/g++ \
python scripts/phase2_exit_gate.py run \
  --lane-id linux-qt-6.12-x64 --lane-type retained-qt \
  --configure-preset linux-gcc-debug --build-dir build/linux-gcc-debug \
  --qt-version 6.12.0 --qt-architecture x64 \
  --qt-prefix-env QT_LINUX_PREFIX \
  --output artifacts/phase2/linux-qt-6.12-x64/linux-qt-6.12-x64.json \
  --logs-dir artifacts/phase2/linux-qt-6.12-x64/logs
```

On hosts whose C++ launcher resolves to an unavailable or read-only `ccache`,
set `CC` and `CXX` to the direct GCC paths as shown. This is a host-toolchain
workaround, not a product configuration change.

Keep this result separate from the Qt-free engine matrix. Record the exact Qt
version and whether the test was runtime-tested, compile-only, or blocked.
To record a build without making a runtime claim, add `--compile-only` to the
`run` command. This remains a failed aggregate lane until runtime evidence is
collected.

## macOS and Linux fixture ownership

The automated `phase2-exit-gate.yml` workflow now runs and archives these
retained-Qt directions as part of its final gate. The manually dispatched
`refactoring-baseline.yml` workflow remains the historical owner of the same
fixture directions:

- `baseline` / `Linux x64 Debug` (`linux-gcc-debug`)
- `baseline` / `macOS universal Debug` (`macos-clang-debug`)

That workflow archives JSON, JUnit, and CTest logs. Release workflows are not
substitutes because they set `BUILD_TESTING=OFF`.

Automation proves the declared build, inventory, test, artifact, and metadata
contracts. Human review remains required for interpreting fixture semantics,
confirming that the required migration slices and failure categories are still
meaningful, reviewing platform-specific failures or environment issues, and
deciding whether the documented Phase 2 intent is satisfied.

## P2-R07 closure record — 2026-09-06

The Windows milestone and the exact retained-Qt Linux and macOS fixture lanes
have runtime-tested PASS reports. The downloaded report set was regenerated
into `artifacts/phase2/aggregate-report.json`; the validator returned `PASS`
with the exact seven required lanes, one commit across all lane reports, and
no missing-artifact, `host-blocked`, or failed evidence.

- All four Windows engine lanes and the retained Windows Qt lane have complete
  runtime-tested evidence with the required logs.
- The three retained-Qt lanes publish fixture evidence for the migrated
  persistence slices, including invalid input, rollback, migration,
  busy/locked database, and partial-failure outcomes where the lane owns that
  fixture.
- The `ApplicationServices::dataService()` facade and borrowed-session
  compatibility path are retired; migrated Qt callers use narrow services,
  while direct standalone `DataService` fixtures remain explicitly scoped.
