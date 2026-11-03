# W7.5.2 Windows current-head Debug requalification

## Result

The requalification run and Phase 5 comparison are complete. The Debug gate is
**not qualified**: the post-fix all-target build passed, but unfiltered serial
CTest exited 8 with 39 failures among 614 tests (575 passed) after 3,528.81 s.

The run started from clean source SHA
`4b5f2de9491f5d6e7064bf41edaa0fedd66ade1f`. The initial two-job clean build
timed out at 7,200.052 s. An incremental continuation found two unresolved
`DialogServices::confirm` references in the Classes Navigator page test and
visual-capture targets. The local fix adds their missing private
`ClassMngrRuntime` dependency in `cmake/tests/next.cmake`. After CMake
regenerated the Visual Studio solution, the all-target Debug build passed.
`ClassMngrNextClassesNavigatorPageTests` passed; the visual-capture executable
linked but its CTest case failed at runtime.

The saved Phase 5 artifact lists 14 failures among 512 tests. Ten are also in
this run; four historical failures now pass. This run has 29 failures that do
not appear in that list. One explicitly reports the Windows runner could not
create a genuine symlink (`CreateSymbolicLinkW` error 1314). The other 28
additional outcomes remain unclassified by the captured diagnostics and need
follow-up. Those 28 outcomes are transferred to Phase 10 for diagnosis; the
symlink-privilege limitation is transferred to Phase 11 to provide a Windows
runner with the required capability. Because the source revision, toolchain,
and test count differ from the Phase 5 artifact, these additional outcomes are
not labeled proven regressions. The separate accepted Phase 7 baseline still
timed out during compilation and supplied no CTest result.

The final build and tests used source SHA `4b5f2de` plus the uncommitted CMake
link-dependency fix recorded in `source-manifest.json`; this is not a pristine
HEAD qualification. No existing assertions or comparators were changed. CTest
rewrote ten tracked SubPrep baseline images during visual-capture side effects;
they were restored after the run.

## Evidence files

- `prior-clean-run.json` and `initial-clean-build.stdout.log`: initial clean
  configure/build record and two-hour timeout.
- `continued-build-before-fix.stdout.log`: incremental build and the two link
  failures.
- `build-after-link-fix.stdout.log`: regenerated all-target Debug build exit 0.
- `ctest.stdout.log`, `ctest.junit.xml`, `ctest-last-test.log`, and
  `ctest-failed-tests.log`: full serial CTest result and raw test records.
- `historical-phase5-comparison.json`: exact failure overlap, cleared historic
  failures, additional current failures, and outcome counts.
- `toolchain.json`, `run-policy.json`, and `source-manifest.json`: environment,
  commands, source identity, and local correction.

The earlier incomplete W7.5.2 attempt remains preserved at
[`../w7.5.2-current-debug/README.md`](../w7.5.2-current-debug/README.md).
This bundle's [SHA-256 manifest](SHA256SUMS.txt) covers every included file and
this README, but not the manifest itself.
