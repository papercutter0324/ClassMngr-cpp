# Phase 8a macOS local Debug build and tests

The Mac mini can run the existing universal Debug preset and the full CTest
suite directly. No GitHub self-hosted runner or new Actions workflow is needed.

## Checkout

Use a clean checkout of the published Qt-Rewrite branch. The current published
OS-agnostic batch is d034bb56. Windows-specific local commits on the Windows
workstation are not included in that remote commit.

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
runs the full CTest suite with failure output, and writes the baseline JSON and
JUnit report under the ignored build directory.

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

If the build and CTest suite pass, run the Phase 8a matched SQL Sub Prep
capture. The timestamp keeps repeated runs in separate output directories.

    C53_OUTPUT_DIR="$PWD/build/macos-clang-debug/artifacts/sub-prep-c53-$(date +%Y%m%d-%H%M%S)"
    c53_args=(
      --build-dir build/macos-clang-debug
      --report macos-clang-debug
      --output-dir "$C53_OUTPUT_DIR"
    )
    QT_QPA_PLATFORM=offscreen python scripts/phase8/capture_sub_prep_c53_parity.py "${c53_args[@]}"

The baseline JSON, full CTest log, and JUnit report are in
build/macos-clang-debug/artifacts and
build/macos-clang-debug/Testing. Review the Ninja output or follow the log with
tail during the build; check in at 30-minute intervals as requested.
