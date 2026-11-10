# Phase 8a macOS local Debug build and tests

The Mac mini can run the existing universal Debug preset and the full CTest
suite directly. No GitHub self-hosted runner or new Actions workflow is needed.

## Checkout

Use a clean checkout of the published Qt-Rewrite branch. The current published
candidate is 3dc37340. Windows-specific local commits on the Windows
workstation are not included in this macOS candidate.

    git clone https://github.com/papercutter0324/ClassMngr-cpp.git
    cd ClassMngr-cpp
    git switch Qt-Rewrite
    git pull --ff-only origin Qt-Rewrite
    git status --short

## One-time tools and Qt setup

Install Xcode command-line tools, CMake, and Ninja:

    xcode-select --install
    brew install cmake ninja

Create a Python virtual environment and install the pinned Qt installer:

    mkdir -p "$HOME/.venvs"
    python3 -m venv "$HOME/.venvs/classmngr-qt"
    source "$HOME/.venvs/classmngr-qt/bin/activate"
    python -m pip install aqtinstall==3.3.0

Install Qt 6.12.0 with Qt PDF. This can be skipped on later runs if the same
Qt installation is still available.

    mkdir -p "$HOME/Qt"
    python -m aqt install-qt mac desktop 6.12.0 clang_64 --outputdir "$HOME/Qt" --modules qtpdf
    export QT_MACOS_PREFIX="$HOME/Qt/6.12.0/macos"

## Build and run every test

From the repository root, run the existing baseline recorder. It performs a
fresh CMake configure with the macOS universal Debug preset, builds all targets,
runs the full CTest suite with failure output, and then attempts the matched SQL
C53 capture whenever the build succeeds. A CTest failure does not suppress the
capture. The JSON report records build, CTest, provenance, and C53 outcomes
separately; the C53 files and JUnit report are written under the ignored build
directory.

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
    python scripts/record_refactoring_baseline.py "${baseline_args[@]}" 2>&1 | tee build/macos-clang-debug/artifacts/refactoring-baseline.log

The baseline JSON, full CTest log, JUnit report, and timestamped C53 capture
directory are in `build/macos-clang-debug/artifacts` and
`build/macos-clang-debug/Testing`. Review the recorder's JSON even when CTest
fails so the independent C53 result is retained. Review the Ninja output or
follow the log with tail during the build; check in at 30-minute intervals as
requested.

Run this command where the tests have normal local-socket access and macOS
AppKit framework access. A restricted runner sandbox can deny `QTcpServer`
loopback binds and cause QWizard/AppKit `NSBundle` exceptions; those are
environment failures for these tests, not evidence of an application defect.
For repeat runs, prefer a fresh build directory. The resource baker protects
non-empty unowned `generated/ClassMngrNext/Resources` output from replacement;
archive that generated directory before reusing an existing build tree.

## Verified local macOS result

The source-stable baseline at commit 3dc37340 completed on macOS 27.2 arm64:
the clean Debug build passed all 5,572 steps, all 620 CTests passed with no
failures or timeouts, and C53 passed with 17 pages and zero changed pixels.
The run report is
[`refactoring-baseline-unsandboxed-retry-1.json`](../../../build/phase8a-macos-debug-3dc37340/artifacts/refactoring-baseline-unsandboxed-retry-1.json).
The corresponding full CTest transcript is in
[`LastTest.log`](../../../build/phase8a-macos-debug-3dc37340/Testing/Temporary/LastTest.log).
This closes the local macOS slice; Windows and Linux still need their own
source-stable baseline and capture verification.
