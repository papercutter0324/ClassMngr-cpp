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
Local acceptance initially used the standard macOS Debug build and focused
tests, with hosted Windows Debug and packaged Release results planned for Batch 1
closeout. The later local-only acceptance and Windows evidence are recorded in
the 2026-11-18 entry below. No Windows memory budget or exit-gate result was
claimed at batch start.

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
The accepted P9-02 slice is committed as `112eaa14`.

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

#### Batch 2 scenario-gate discovery — started 2026-11-17

Batch 2 is being shaped around packaged Windows Release gates after Batch 1
instrumentation. The current sampler waits ten seconds after a visible window,
takes five one-second samples, and sets no numeric memory pass threshold; it
does not measure startup-ready or five-minute idle. The proposed first gate is
strictly below 262,144,000 bytes at startup-ready and five-minute idle for empty
and representative workspaces, with fresh Initial Setup remaining advisory.
Complete over-budget runs must fail while preserving artifacts; incomplete or
invalid evidence must remain not evaluated.

Follow-up candidates add repeated normal navigation and idle, then the 96-class
Sub Prep route through summary, first selection, refresh, package generation,
leave, and repeated re-entry with bounded object counts and matched memory
before/after release. The documented 536,870,912-byte value remains a
diagnostic ceiling, not a hard transient budget, until current-source Windows
Release data establishes a return-to-baseline allowance. At discovery, the
package and native working-set evidence was planned for hosted Windows; the
current local-only acceptance is recorded below.

#### P9-03 — Resource/cache/image/font lifecycle trace — 2026-11-17

Production `CoreServices` now wires the existing `ResourceLoader` diagnostic
callback to a compact JSON logger. Events include request scope and owner,
catalog owner/category/kind/type/location, outcome/error/elapsed time, catalog
source bytes, decoded payload bytes, source and decoded image dimensions, and
font family names. Payload values are explicitly documented as weights, not
resident allocation sizes. `resolveFile()` now follows the same diagnostic
path and reports source bytes without decoded payload size.

Cache snapshots now include entry count and preserve cached entry kind, owner
scope, accounted payload bytes, and image bound. The app still has no production
`ResourceCache`; events mark cache data unavailable instead of reporting fake
zeros, and this instrumentation does not add a cache. The logger serializes
concurrent callbacks and catches its own failures. Catalog/font metadata
enrichment is exception-safe so diagnostics cannot change resource results.

Independent macOS re-verification passed with an incremental build in the
standard `build/macos-clang-debug` folder. Built targets were
`ClassMngrNextResourceLoaderTests`, `ClassMngrNext`, and
`ClassMngrNextBootstrapTests`. Focused CTest for the ResourceLoader and
Bootstrap targets passed 2/2; `git diff --check` passed. The tests cover
deterministic serialization, configured empty versus unavailable cache,
cache-hit ownership, image dimensions, font families, `resolveFile()`, and a
throwing diagnostic callback. Hosted Windows/Linux Debug and packaged Windows
Release checks remain pending at Batch 1 close. The accepted P9-03 slice is
committed as `e157c89c`.

The workspace signature-image upload decodes user-selected workspace data
directly in `MyWorkspacePage`, outside the resource catalog and `ResourceLoader`.
It is not included in the P9-03 catalog-resource trace; any future requirement to
trace that workspace-data path belongs with its feature-owned diagnostics.

#### P9-04 — QtPdf and output lifecycle trace — 2026-11-17

Production tracing now joins QtPdf viewer-session status, selection, load,
close, stream, document, and view lifetimes with whole-process checkpoints.
Retired documents retain their original generation and status in the trace.
The Next viewer records successful and failed export-copy outcomes and print
status; shared print rendering, roster preview rendering, and `QPdfWriter`
lifetime/finalization are also observable. Detailed page events are bounded
(viewer and preview 32, print 16, writer 32) with total and suppressed counts.
The trace records rendered pixel payload separately from process resident
memory and uses null before a render has produced an image. It does not log
document titles or filesystem paths.

Independent macOS Debug verification passed the affected-target build and
focused CTest set 5/5: PDF lifecycle diagnostics, QtPdf session release,
DocumentViewer, Bootstrap, and the startup workspace dependency boundary.
Coverage includes release/reopen and final view-document-stream destruction
order, retired generation identity, export success/failure, print status,
bounded writer/preview/print events, and 40 viewer page selections reduced to
32 details plus 8 suppressed. `git diff --check` passed. `QPdfView` exposes
selection but no internal render-completion hook at this boundary. Native
Windows counters and hosted Windows/Linux/package checks remain pending; no
Windows memory gate is claimed.

P9-04 is committed as `4a8141ff`.

#### P9-05 — Source-bound Windows Release evidence — 2026-11-17

Push-triggered `windows-release.yml` now checks out `github.sha` when no
explicit reusable-workflow `source_ref` is supplied. Before setup, build, or
packaging, it verifies that `git rev-parse HEAD` matches `GITHUB_SHA` and writes
the candidate and trigger SHAs to the Actions summary. Manual dispatch continues
to use the selected `github.ref`, and callers may still provide `source_ref`.

Independent workflow review found no issues; Ruby Psych parsed the YAML and
`git diff --check` passed. `actionlint` and PowerShell were unavailable, so
GitHub expression/schema and PowerShell execution validation are deferred to
hosted Windows. Existing Windows Release memory sampling remains advisory and
is not a Phase 9 budget pass.

#### Batch 1 hosted verification and P9-04R — 2026-11-17

Batch 1 was pushed at `e3346a57869a04b91c10fa6a1c5de8bec2f3289a`.
Windows Release run `44058668038` passed both x64 and ARM64 jobs; both package
artifacts were uploaded, and the push-triggered candidate-SHA guard passed.
Linux x64 Release run `44058668021` passed. In Refactoring baseline run
`44058668011`, Linux x64 Debug and the Windows ARM64 Debug cross-build passed,
and SQL Sub Prep parity passed. Windows x64 Debug CTest reported 619/620; only
`ClassMngrNextPlatformQtPdfSessionReleaseTests` failed with process exception
`0xc0000409` after 6.93 seconds. The job log contains no assertion, Qt fatal,
or stack trace, and does not identify the QtTest subfunction. The Windows
baseline artifacts include `LastTest.log` and `LastTestsFailed.log`; no separate
crash dump was uploaded. The first Batch 1 hosted verification therefore does
not pass.

The focused P9-04R repair keeps real single-`jump()` coverage and changes the
40-event cap test to alternating `QPdfPageNavigator::update()` calls, which
exercise all 40 page-change records without accumulating jump history. It
injects deterministic Windows-shaped snapshots so this event-cap test is not
coupled to repeated Windows process/thread sampling. Assertions still verify
40 changed pages, 32 details, and 8 suppressed events. The executor build and
independent local macOS Debug CTest both passed; the target reports 12/12 and
`git diff --check` is clean. This is an isolation repair based on the strongest
new-code hypothesis; the Windows crash cause remains unconfirmed until the
additional Windows x64 Debug verification. Batch 1 stayed open pending that
verification.

#### Initial local Windows gate and QtPdf finding (superseded) — 2026-11-18

At that point, acceptance was local Windows validation only. Batch 1 required a
passing full CTest run on Windows x64 Debug and the existing P9-05 source-bound
package evidence. Local packaged Windows Release memory measurements begin in
Batch 2 with P9-06. The overall Phase 9 exit requires both the local full-suite
result and local packaged Release memory measurements. GitHub validation is not
required. Phase 9 remains In progress; no memory budget pass is claimed.

The local full CTest run used the full preset build and reported 619/620 passed,
with zero skipped or not run. Its sole failure was test #603,
`ClassMngrNextPlatformQtPdfSessionReleaseTests`, exiting `0xc0000409`. Isolated
logs show `malformedPdfErrorIsDeferredAndCanReleaseSafely()` receives
`QPdfDocument::Status::Error` (error 4), then QtPdf reaches `Q_UNREACHABLE` at
`qpdfdocument.cpp:201` before `open()` returns and before the queued session
handler runs. The session error release path was not reached. The installed
Qt6Pdf DLL version is `6.140.0.0`.

Evidence is in `build/windows-x64-debug/p9_batch1_qtpdf_malformed_subtest.txt`,
`build/windows-x64-debug/p9_batch1_qtpdf_qtestlog.txt`, and
`build/windows-x64-debug/p9_batch1_local_windows_full_ctest_full_build.log`.
This finding led to P9-04S and is superseded by the fresh passing full-suite run
below. The earlier hosted run's artifacts did not identify a QtTest subfunction,
so this local finding does not establish that the hosted failure had the same
trigger.

#### P9-04S accepted and Batch 1 local Windows gate — 2026-11-18

P9-04S replaces the malformed-PDF case with a deterministic
`QPdfDocument::load(missingPath)` error while the session's actual ResourceLoader
stream is active. The focused test verifies the deferred error arrives after
`open()` returns, reports `FileNotFound`, releases successfully, destroys and
replaces the failed document, observes stream-release ordering, reaches final
Null state, and does not time out. Production close waiting now treats both
Null and Error as terminal before document destruction and stream release. The
prior malformed-PDF case is not covered or fixed: QtPdf reached
`Q_UNREACHABLE` at `qpdfdocument.cpp:201` before `open()` returned or the queued
session handler ran (installed Qt6Pdf DLL `6.140.0.0`).

The elevated local `cmake --build --preset windows-x64-debug` succeeded in
64.9 seconds and compiled/linked the changed session and test sources. Focused
CTest passed 1/1 in 10.999 seconds. A fresh elevated unfiltered CTest run passed
620/620, with zero skipped or not run and exit 0, in 1174.78 seconds. This fresh
run supersedes the earlier local 619/620 result. No ACL changes or cleanup were
performed; non-elevated ACL-denied attempts were diagnostic only. The cause of
the earlier hosted 619/620 failure and its relationship to the local malformed-
PDF finding remain unconfirmed because hosted artifacts did not identify a
subtest. P9-05 package evidence remains from prior hosted runs;
GitHub validation was not run for this gate.

P9-04 and P9-04S are accepted. Batch 1 local verification is complete. The
accepted P9-04S slice commit and its single batch push close Batch 1; P9-06
follows that push with the local packaged Windows Release startup-ready and
five-minute-idle gate. No Phase 9 numeric memory budget has passed.
