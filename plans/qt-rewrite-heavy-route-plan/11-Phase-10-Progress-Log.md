# Phase 10 — Progress Log

Read the [Phase 10 plan](11-Phase-10-Visual-Behavioral-and-Cross-Platform-Parity.md)
for current status, requirements, and contracts. This file is the historical
progress record for Phase 10; do not put dated progress updates in the plan.

### 2026-11-25 — Phase 10 start precheck passed

At the user's request, an independent local Tester reviewed the fresh
Windows x64 Release precheck at
artifacts/phase9/p9-19-exact-release-builds/p10-release-precheck-20261125-115431-f8d41043.
The receipt SHA-256 is
977cd992aeff4f85fb4226275fa61358823d2f449cd1d5873f314cf7a1254192.
Its 2,377-file source inventory SHA-256 is
4f792604d5d0cfdcf7581cf1e875291a60c810b232c7d2d083243aeaddb83f25, and
the current source matched. The verified toolchain was VS2022 x64/v143
14.44.35207 with Windows SDK 10.0.26100.0. Four focused CTests passed:
ClassMngrNextBootstrapTests, ClassMngrNextPageHostTests,
ClassMngrNextClassesNavigatorPageTests, and
ClassMngrNextClassesNavigatorVisualCapture. The executable SHA-256 is
669ee067ff9efe2995cd5f81f8071e169323b1da931742cb3ccda8e5ad7f257d; the
CTest log SHA-256 is
6a1110f231c803532b466a31de8c21360cd678e2aa605098ab53f063e95e4347.

This passing focused precheck authorizes Phase 10 to start. It is not Phase 9
final packaged Release evidence and does not complete Phase 9 or satisfy its
full Windows x64 Debug CTest. Those remaining Phase 9 items are deferred to
Phase 14 after Phase 13. Phase 10 parity work and exit requirements remain
unchanged. The repository-root build directory remains empty by user request.

The active Batch 1 discovery is in the [current slice-batch tracker](Current-Slice-Batches.md).
Its visual candidates are discovery targets, not verified current-source
defects; requalify with source-bound captures and record source, toolchain,
fixture, and raw artifacts.

### 2026-11-25 — Phase 10 paused for Linux switch

The user paused this session to switch to Linux. Phase 10 remains In Progress.
Batch 1 candidate 1, focused Linux verification of migrated Phase 7 behavior,
has not run. Resume on Linux with the standard `linux-gcc-debug` preset and
the 19 selected CTest executables listed in the canonical
[session handoff](../../agent_docs/latest_session_work.md). Do not rerun the
full Phase 7 suite. The current Windows environment had no local Linux runtime;
the temporary hosted-workflow fallback was removed, and no GitHub workflow
was triggered.

The focused Windows x64 Release precheck passed and authorizes Phase 10 start
only; it does not satisfy Phase 9's final Release gate. The Phase 9 transition
commit `95c2d4abf3ab03ff69e6871a7b10b6a93c0efd87` is pushed to
`origin/Qt-Rewrite`. Phase 9 remains In Progress and incomplete with all
remaining work deferred to Phase 14 after Phase 13 under the unchanged gates.
