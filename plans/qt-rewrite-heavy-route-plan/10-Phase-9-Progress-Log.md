# Phase 9 — Progress Log

Read the [Phase 9 plan](10-Phase-9-Windows-Memory-Hardening.md) for current
status, requirements, and contracts. This file is the historical progress
record for Phase 9; do not put dated progress updates in the plan.

### 2026-11-17 — Phase 9 started; Batch 1 defined

Phase 8 and 8a are Complete, satisfying the Phase 9 dependency on Phases 4–8.
Phase 9 is In progress on macOS under the required Heavy route. The worktree was
clean at `b5ab7975`, matching `origin/Qt-Rewrite`.

Batch 1 is the shared lifecycle measurement instrumentation sequence: P9-01
adds named Next startup process-memory checkpoints; P9-02 adds page/model
lifecycle metrics; P9-03 wires resource/cache/image/font diagnostics; and
P9-04 adds QtPdf/output lifecycle events. P9-01 acceptance requires structured
stage/outcome records, explicit platform and availability fields, all available
snapshot counters with units, unavailable values that are not misreported as
zero, and a deterministic injected-provider test. Startup behavior/order must
not change.

The project already has a platform process snapshot provider and stage-level
Next startup diagnostics, but no common production trace joins them. The
existing Windows x64 Release evidence workflow has a useful process sampler,
but its memory step is advisory and does not enforce the Phase 9 threshold.
Local acceptance will use the standard macOS Debug build and focused tests;
Batch 1 closeout requires hosted Windows Debug and packaged Release results.
No Windows memory budget or exit-gate result is claimed at batch start.

#### P9-01 target-boundary decision

The process snapshot implementation currently belongs to the broad legacy Core
object target, while Next intentionally has no dependency on that target. P9-01
will extract the existing implementation into a QtCore-level shared target and
reuse it from both the legacy runtime and Next. This preserves one native
provider implementation and the narrow Next dependency boundary. A fresh
configure/build will use the standard `build/macos-clang-debug` folder because
the source/target graph changes; existing generated artifacts must be inventoried
and preserved before the folder is emptied.

#### P9-01 independent review — 2026-11-17

The independent macOS review passed the initial affected-target incremental
build, focused CTest 3/3, direct startup-sink QtTest 6 passed (including
setup/cleanup), direct bootstrap QtTest 90 passed, the Next dependency-boundary
check, and `git diff --check`. It confirmed one production provider owner and
no broad legacy Core dependency from Next. Acceptance stayed open because an
available process sample could have a failed individual handle/thread count
serialized as zero, and Linux could fall back from missing PSS to anonymous RSS
while labelling it private usage. Those findings drove the repair below.

#### P9-01 repair and independent recheck — 2026-11-17

The provider now tracks each metric's availability separately on Windows,
macOS, and Linux. The startup JSON writes `null` for supported-but-unavailable
counters and leaves platform-unsupported counters out of the record. Linux PSS
stays distinct from anonymous resident memory; the RssAnon fallback was
removed. Linux fixtures now distinguish a missing value from an available
numeric zero. The cross-platform sink suite now samples the native provider.

Independent recheck passed on macOS arm64: affected production/test build;
focused CTest 3/3; direct sink QtTest 10 passed including setup/cleanup; direct
bootstrap QtTest 90 passed; native-provider smoke passed; dependency-boundary
check passed; and `git diff --check` passed. The build was incremental in the
freshly configured standard `build/macos-clang-debug` folder. Linux procfs
fixture tests and Windows native execution remain pending hosted CI. The P9-01
implementation is locally accepted; no hosted verification or numeric memory
gate is claimed yet.

#### Historical Sub Prep memory evidence audit

The F622/F624 Windows x64 packaged Release route is useful prior art, not
current Phase 9 acceptance. Its three 96-class runs recorded current working
set below 250 MiB and two leave/re-entry cycles, but the measured source was
F621 (`b5fdecd`), an ancestor separated from current `b5ab7975` by 2,373
commits and substantial Sub Prep changes. Raw binaries and checkpoint
manifests are absent from this checkout. The traces also lack five-minute idle
and matched return-to-baseline evidence; settled working set remained above
operation-start after export. Phase 9 must rerun the route on a source-bound
packaged Release and measure matched cleanup, repeated-cycle growth, and five-
minute idle. Preserve the old route sequence and output-parity checks as
historical comparison context only.

#### P9-02 — Next page and model lifecycle trace — 2026-11-17

The production recorder now writes structured PageHost lifecycle records for
Created, Activated, Suspended, and Released states. It joins each event to the
platform process snapshot, counter availability, page instance availability,
model names/types/root-row counts, table-item and index-widget counts, and
editor-control counts. Page data is `null` before host storage and after
release; a separate `page-instance-destroyed` event records actual QObject
destruction, including shell shutdown, while marking page-object metrics
unavailable. The recorder lives through shell teardown and does not alter
PageHost lifecycle behavior.

Page traversal is capped at 100,000 QObject nodes, view-cell inspection at
65,536 cells, and detailed model records at 256 entries with explicit
truncation flags. The existing 96-class Classes Navigator capture now records
the full lifecycle trace through suspension, explicit release, recreation,
rebuilding 96 class rows, and release of the recreated instance. It verifies
that the shared editor widget remains bounded while its loaded class and
schedule rows clear on suspension. Eight locale/theme/fixture cases pass.

Independent macOS re-verification passed using the incremental standard
`build/macos-clang-debug` folder. The affected targets were
`ClassMngrNextPageHostTests`, `ClassMngrNextClassesNavigatorVisualCapture`,
`ClassMngrNext`, and `ClassMngrNextBootstrapTests`. Focused CTest for
`ClassMngrNextPageHostTests`, `ClassMngrNextClassesNavigatorVisualCapture`, and
`ClassMngrNextBootstrapTests` passed 3/3; `git diff --check` passed. The latest
capture manifest is generated under
`build/macos-clang-debug/classes-navigator-visual-capture/manifest.json`.
Hosted Windows/Linux Debug and packaged Windows Release verification remain
pending at Batch 1 close; no memory threshold result is claimed.

The hosted Release workflow currently checks out a mutable branch ref for
push-triggered builds and does not compare the checked-out commit with the push
SHA. A separate P9-05 slice was added to bind package evidence to the triggering
commit and report the candidate SHA before the Batch 1 push.
