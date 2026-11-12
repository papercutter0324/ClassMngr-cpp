# Phase 8a macOS local Debug build and tests

The clean universal macOS Debug candidate runs on the local Mac mini. The
GitHub Debug matrix no longer requests the billed `macos-15-large` runner or
retries it automatically. The separate macOS Release workflow uses a standard
GitHub-hosted runner, which is free for this public repository.

## Tools and Qt

Use Xcode command-line tools, CMake, Ninja, Python 3, and Qt 6.12.0 with Qt
PDF. Set `QT_MACOS_PREFIX` to the Qt `macos` installation directory. The
verified machine used:

- macOS 27.2 arm64
- Xcode 27.2, CMake 4.3.3, Ninja 1.13.2
- Qt 6.12.0 universal (`arm64` and `x86_64`)

## Fresh full candidate

Run the baseline recorder from a clean, source-stable checkout. It configures
the universal Debug preset, clean-builds all targets, runs the full CTest
suite, and attempts the matched SQL C53 capture after a successful build even
if unrelated tests fail. CTest needs normal local-socket and AppKit access;
restricted execution can deny loopback listeners and cause Cocoa `NSBundle`
errors.

```bash
export QT_MACOS_PREFIX="$HOME/Qt/6.12.0/macos"
mkdir -p build/macos-clang-debug/artifacts
set -o pipefail
export CLASSMNGR_C51_SCHEDULE_OUTPUT_DIR="$PWD/build/macos-clang-debug/artifacts/schedule-output"
export CLASSMNGR_ROSTER_OUTPUT_PARITY_DIR="$PWD/build/macos-clang-debug/artifacts/roster-output"
export CLASSMNGR_SPEAKING_EVALUATION_OUTPUT_PARITY_DIR="$PWD/build/macos-clang-debug/artifacts/speaking-evaluation-output-parity"
baseline_args=(
  --configure-preset macos-clang-debug
  --build-dir build/macos-clang-debug
  --parallel 6
  --test-timeout 120
  --expected-commit "$(git rev-parse HEAD)"
  --output build/macos-clang-debug/artifacts/refactoring-baseline.json
)
python3 scripts/record_refactoring_baseline.py "${baseline_args[@]}" 2>&1 \
  | tee build/macos-clang-debug/artifacts/refactoring-baseline.log
```

The recorder performs a clean build. On the verified Mac mini, configure took
28 seconds and the 5,607-step universal Debug build took 34m55s with six
workers. Reserve this full run for clean phase candidates. For everyday work,
reuse the configured tree and build only the affected targets, then run their
focused CTest selectors; for example:

```bash
cmake --build build/macos-clang-debug \
  --target ClassMngrNext ClassMngrNextBootstrapTests --parallel 6
ctest --test-dir build/macos-clang-debug --output-on-failure \
  -R '^(ClassMngrNextBootstrapTests)$'
```

Keep full-suite results distinct from focused reruns. The full recorder report,
CTest transcript/JUnit, C53 files, Schedule, Roster, and Speaking output are
under `build/macos-clang-debug`. Preserve the first failure logs if rerunning
under a different execution context. Do not reuse a generated Resources
directory that is non-empty and unowned by the build.

## Verified local candidate at current source-equivalent HEAD

At commit `e651b0164913478394945d8a1bc5936758bc852b`, the clean universal
Debug build passed all 5,607 steps. The initial CTest execution was sandboxed
and reported six access-related failures. A normal-access CTest rerun against
the same built targets passed **620/620**, with no failures, skips, or
timeouts, in 354.98 seconds; it required no rebuild or reconfigure. Its
[`normal-access-rerun.log`](../../../build/macos-clang-debug/artifacts/normal-access-rerun/normal-access-rerun.log),
[JUnit report](../../../build/macos-clang-debug/artifacts/normal-access-rerun/normal-access-rerun.junit.xml),
[provenance](../../../build/macos-clang-debug/artifacts/normal-access-rerun/provenance.json),
and [QWARN comparison](../../../build/macos-clang-debug/artifacts/normal-access-rerun/qwarn-delta.json)
are retained alongside the original sandbox failure logs.

Schedule Legacy and Next matched with zero changed pixels. All four Roster
layouts passed; Speaking Standard and Advanced passed expected-text checks
with zero changed pixels. C53 produced 17 pages (12 Sub Prep and 5 Rosters)
with zero changed pixels. The frozen Schedule reference differs by 5.68% on
this host because its Sans Serif fallback is unavailable; the same-host
Legacy/Next comparison is exact.

The interactive Cocoa file and print dialog smoke was not performed because
the current session lacks macOS event-posting and screen-capture permissions.
The app was not launched, no profile was accessed, and no print job was
submitted. Presenter and print-adapter tests remain the automated evidence.

## Schedule font fallback follow-up (2026-11-12)

Schedule output keeps the bundled Inter and Pretendard families first. If one
is unavailable, its final family is now Helvetica Neue on macOS and Segoe UI
on Windows; other platforms use Qt's GeneralFont system family. The focused
Schedule page test checks that the platform family is registered in
`QFontDatabase` and appears in both output font chains.

After the change, the macOS Debug targets built and both
`ClassMngrNextSchedulePageTests` and
`ClassMngrNextNormalScheduleImportIntegrationTests` passed (2/2). The output
capture is in
[`schedule-fallback-font-20261112`](../../../build/macos-clang-debug/artifacts/schedule-fallback-font-20261112/);
Legacy and Next remain pixel-identical on this host. The capture still reports
a 5.68% difference from the frozen Phase 7 image and records Inter/Pretendard
as available, so this run does not exercise the new final fallback during
rasterization. The Windows family assertion is included in the Windows test
build but was not run on this Mac.

## Current universal Release package

The local Release package build, disk-image verification, sidecar checksum,
and all 13 package-verifier tests passed at the same source revision. The DMG
and reports are under `build/macos-next-release-package`; see the [package
report](../../../build/macos-next-release-package/reports/macos-next-package.json),
[audit](../../../build/macos-next-release-package/artifacts/package-audit.json),
[provenance](../../../build/macos-next-release-package/artifacts/source-provenance.json),
and [verifier log](../../../build/macos-next-release-package/artifacts/test-verify-next-package.log).
