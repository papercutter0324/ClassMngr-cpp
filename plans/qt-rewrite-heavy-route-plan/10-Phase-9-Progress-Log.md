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

#### P9-06 optimized packaged startup/idle gate — 2026-11-18

The first packaged run was Release-configured but not optimized: its C and C++
Release flags were empty and the generated app project/compiler trace lacked
MSVC optimization. It is preserved at
`artifacts/phase9/packaged-idle-memory/20261117T173949789Z-0700b5e42c684780a2bfd432c067cda4/`
as diagnostic only and is not a Release baseline. Before rebuilding the
standard `build/windows-x64-release` folder, the original cache, app project,
and compile trace were copied to the run's `preoptimized-build-state` folder.
No prior artifact or build tree was deleted.

The standard Windows x64 Release tree was reconfigured with Visual Studio 18
2026, Qt 6.12.0, `BUILD_TESTING=ON`,
`CLASSMNGR_NEXT_RELEASE_PACKAGE=ON`, and `/MD /O2 /Ob2 /DNDEBUG` for C and C++.
The actual `ClassMngrNext` compile trace contains 69 commands with `/O2` and
`NDEBUG`, none with `/Od`; trace SHA-256 is
`180f02f2e04075f2b57d755b89e22c1d0f37e00027cfab417b741355d86a67ce`. The
prerequisite package and fixture targets built successfully. A local portable
PowerShell 7.7.0 runtime was installed under ignored `build/tools`; its ZIP was
verified against the official release SHA-256. No hosted validation was used.

The complete valid run used for the P9-06 baseline is preserved at
`artifacts/phase9/packaged-idle-memory/20261117T190410131Z-43f692fcd33b4a2a87cf12518dbf1386/`.
Its source fingerprint is `a0c24aa2acb7ab5205b016fff454e4bf9d2ad3fe4928be1eb5af7cae5851a03c`
(2,371 files, stable), CMakeCache SHA-256 is
`57058dfc0307cd99eafbd7a351be45447ddaf1c6474d07dc578a2f661b9a8c2f` (stable),
and package SHA-256 is
`1e2e56d4778ad63358f6046e1c7ab46e94d9595e108e7deeb1ea4e7ec8b3e50f`
(1,575 files). The Qt installation is 6.12.0. Both packaged fixtures opened,
emitted successful `workspace-opened` before `startup-ready`, completed 301
samples over about 300 seconds, and exited normally. The validator reported
`validation=valid`, `runStatus=completed`, `budgetResult=failed`, with no
validation errors.

| Scenario | Startup-ready WS / private | External startup WS / private | Five-minute idle WS / private |
|---|---:|---:|---:|
| Empty workspace | 679,813,120 / 612,536,320 B | 686,862,336 / 614,686,720 B | 686,362,624 / 613,658,624 B |
| Representative 96-class workspace | 680,648,704 / 614,563,840 B | 687,775,744 / 616,292,352 B | 686,927,872 / 615,014,400 B |

All six gated working-set points exceed the strict 262,144,000-byte limit.
Median five-minute idle working sets are 686,030,848 bytes (empty) and
686,592,000 bytes (96-class). This is a valid over-budget result, not a Phase 9
pass. In-process startup records show about 559 MB growth between
`theme-loaded` and `required-fonts-loaded`; `workspace-opened` adds roughly
2 MB, while `shell-created` is about 70 MB above the font-loaded stage. These
checkpoints establish the combined font-loading stage as the primary startup
boundary to reduce, but do not isolate each individual font.

Two later runs are preserved but not evaluated. The first, at
`20261117T185429628Z-d0504dc837744171849432246619a071`, completed the empty
scenario but the 96-class process exited normally at sample 104. The next run,
at `20261117T194226492Z-42c8d910718d481da9ae9186dcf3c268`, completed both
scenarios and captured all ten supplemental external startup-boundary rows,
but its representative trace had a 1.997074-second gap followed by 0.347319-
and 0.633089-second catch-up intervals. The validator correctly reports
`not-evaluated`; its cadence limit remains unchanged. The shell-created record
also occurs after workspace-opened, as the application actually emits it; the
validator contract now reflects that ordering and 35 focused tests plus the
PowerShell 7.7 parser/marker checks pass.

The supplemental per-font samples are approximate, not causal checkpoints:
the app can continue startup while the external runner receives a log line.
In the 96-class trace, the in-process `theme-loaded` sample is about 29.7 MB
working set while the external sample received 17 ms later is about 596 MB.
Do not attribute the jump to one font from these asynchronous readings. The
next slice tests file-backed Qt registration of the same catalog-verified
`Inter.ttc`, keeping Pretendard, the asset, and font family behavior unchanged.
The final decision requires another complete local Release gate; P9-07 and
P9-08 remain open.

#### P9-06R — Same-asset Inter file-backed registration — 2026-11-18

`ApplicationBootstrap` now registers only `Inter.ttc` through a new
`ResourceLoader::loadFileBackedFont()` path. The loader requires a package-file
catalog entry with an expected byte count and SHA-256, resolves its canonical
path, verifies the exact file, checks the existing 128 MiB font bound against
both catalog-declared and current file sizes, and passes that path to
`QFontDatabase::addApplicationFont()`. The `RegisteredFont` scope owns the Qt
registration and retains its source path through release. Pretendard remains
on the existing data-backed route. No font, catalog, license, family name, or
glyph asset changed.

The focused ResourceLoader test compares file-backed and data-backed Inter
family lists; verifies normal and italic 400/600/700 faces; checks diagnostic
identity, catalog byte/hash rejection, scope release, and the oversized
catalog guard without allocating a large test font. The standard local Windows
x64 Debug tree `build/windows-x64-debug` rebuilt the ResourceLoader and
Bootstrap targets with elevated access after sandbox FileTracker denials.
`ClassMngrNextResourceLoaderTests` passed 1/1 (0.90s) and
`ClassMngrNextBootstrapTests` passed 1/1 (80.20s). The full Debug suite has not
been rerun for this slice. The optimized Release gate result is recorded in the P9-06R measurement subsection below; it showed no meaningful reduction, so this is not accepted as a memory fix.

#### P9-06R optimized Release measurement - 2026-11-18

The source-bound local Windows x64 Release run on commit
`e70ae0553dff97a6dd6689d707b76e8347675d84` is preserved at
`artifacts/phase9/packaged-idle-memory/20261117T210831970Z-bbf3d6cbbdb04455a541aba7a2175375/`.
The source fingerprint covered 2,371 files and was stable; the CMake cache
hash and 1,575-file package hash were also stable. The actual app compile trace
had 69 commands with `/O2` and `NDEBUG`, none with `/Od`; the app executable
hash matched the packaged executable. Qt was 6.12.0, both fixture hashes
matched, and the x64 app exited normally for each scenario.

Both empty and 96-class scenarios completed 301 samples over about 300 seconds
with cadence gaps between 0.975 and 1.030 seconds. Evidence validation returned
`validation=valid`, `budgetResult=failed`, `errors=[]`. Every gated point
exceeded the strict 262,144,000-byte limit:

| Scenario | Startup-ready WS / private | External startup WS / private | Five-minute idle WS / private |
|---|---:|---:|---:|
| Empty | 680,534,016 / 613,031,936 B | 687,480,832 / 614,899,712 B | 687,075,328 / 613,617,664 B |
| 96-class | 681,529,344 / 614,031,360 B | 688,103,424 / 615,546,880 B | 687,566,848 / 614,154,240 B |

These points are within 0.3–0.9 MB of the prior valid baseline and do not show
a meaningful reduction from file-backed registration of the exact Inter TTC.
Internal process snapshots show the combined `theme-loaded` to
`required-fonts-loaded` increase remained about 558 MB WS and 564 MB private;
the stage includes Inter, Pretendard, and applying the startup font. External
per-font snapshots are asynchronous and cannot assign that increase to one
operation. Qt 6.12's Windows DirectWrite font database reads file-backed font
data into memory and retains it for font-face creation, consistent with the
lack of savings from the path change. P9-06R is therefore not accepted as a
memory fix. P9-06S now measures each operation synchronously in fresh Release
processes; see its verified results below. The strict budget and cadence remain
unchanged.

#### P9-06S fresh-process font allocation probe

The diagnostic probe is in the local Windows x64 Release tree and uses the
packaged Qt 6.12.0 qwindows plugin and catalog-verified Inter and Pretendard
files. The optimized probe target compiled with /MD /O2 /Ob2 /DNDEBUG, with
the actual compiler trace naming font_allocation_probe.cpp; it did not alter
application startup or the P9-06 gate.

The first complete run is preserved at
artifacts/phase9/font-allocation-probe/20261117T220225631Z-9ff383b0a0944d9d8d36b6f38d689520/.
Independent Tester verification reran seven fresh processes in each of five
scenarios and preserved the result at
artifacts/phase9/font-allocation-probe/20261117T221147310Z-66cd20c5fc88415a852a17a5d7f15c7d/.
Both runs used source fingerprint
29ed73b6b077b410c0fd3ff590a5aceeadd08b4216fd6bc6770c1a6f2d9ee2f8,
Qt 6.12.0, and packaged Inter (13,172,948 bytes,
ea4e3d9970b9b6b566110a1b0e13635fe235280b83dd2443dcb3faca1e2456ef) and
Pretendard (6,739,336 bytes,
3090ccde0442bb347aa7685d9ba8b17436a60682df6e8f92a9a670de14056e22) hashes.
The second run completed 35/35 processes on 35 unique PIDs. The independent
Tester checked all raw records for event order, sequence, PID, architecture,
executable, Qt version, metrics, and completion; it recomputed 203 operation
deltas and matched the process summary.

| Scenario / operation | Median WS delta | Median private-usage delta |
|---|---:|---:|
| No-font process, final | 25.5 MB WS | 6.0 MB private |
| Inter only, registration | 489,877,504 B | 493,740,032 B |
| Pretendard only, data registration | 62,431,232 B | 61,841,408 B |
| Inter then Pretendard, Inter registration | 489,885,696 B | 493,740,032 B |
| Inter then Pretendard, Pretendard registration | about 61–62 MB | about 61–62 MB |
| Pretendard then Inter, Inter registration | 488,849,408 B | 493,371,392 B |
| Family enumeration / startup-font application | no material immediate median delta | no material immediate median delta |
| Full system-family query | about 5–6 MB WS | negligible private delta |

The dominant measured increase is synchronous with
QFontDatabase::addApplicationFont(file) for the packaged Inter TTC and is
stable in both font orders. It does not identify Qt/DirectWrite's internal
allocator or prove the full application's steady-state budget; the probe does
not construct the full shell or idle.

Source usage requests six exact styles: Normal 400, Medium 500, DemiBold 600,
Bold 700, BoldItalic 700, and Black 900 upright. P9-06T compares these six
unmodified, version-matched static TTFs from Inter v4.0 under DirectWrite
with the bundled TTC under Qt GDI. The strict P9-06 gate remains unchanged; no
normal-use memory pass is claimed.
