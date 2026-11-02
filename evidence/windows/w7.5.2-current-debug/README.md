# W7.5.2 Windows current Debug evidence

- Task/run identity: W752-CURRENT-DEBUG-RUN.
- Source revision: 9f59e68992b753bed1cad4546808fc708573b559; the baseline and source manifest record clean start/end state.
- Configuration: Windows x64 Debug, windows-x64-debug preset, Visual Studio 18 2026/MSVC 19.51, CMake 4.4.2, Qt 6.12.0.
- Source evidence root: C:/w752-current-debug-9f59/e.

## Recorded result and limits

Configuration succeeded (exit 0, 238.031 seconds). The clean-first all-target Debug build timed out at 7,200.108 seconds (exit 124) after 485 project outputs; no compiler diagnostic was recorded. CTest did not run (test_exit_code=-1, duration 0, no tests). This is an incomplete build, not a test pass. The records also note that CMake selected Windows SDK 10.0.28000.0 while SDK 10.0.26100.0 was separately added to PATH, and that this VS18/MSVC 19.51 environment differs from the historical Phase 5 VS17/MSVC 19.44 environment.

The historical Phase 5 comparison is recorded in historical-phase5-comparison.json; the separate Phase 7 timeout is documented in the Phase 7.5 plan.

## Included

The baseline summary and full configure/build stdout, toolchain and source manifests, run policy/status, historical comparison, and small launcher/checkout/clone logs are included. Empty stderr and checkout/clone logs are retained as recorded.

## Omitted source files

The older attempt1_temp_path subtree, build/cache/object outputs, Python bytecode, and execution helper scripts (launch.cmd, runner.ps1, sitecustomize.py) are excluded. No CTest result exists for this current run. Included raw evidence files are unchanged.

SHA256SUMS.txt covers every included raw evidence file and this README using forward-slash relative paths; it does not hash itself.

