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
Bold 700, BoldItalic 700, and Black 900 upright. P9-06T comparison results
are recorded in the next subsection. The strict P9-06 gate remains unchanged;
no normal-use memory pass is claimed.
#### P9-06T Inter representation and Qt backend probe

The three-arm diagnostic used the standard local Windows x64 Release tree,
packaged Qt 6.12.0, and seven fresh processes per arm. The first result is at
artifacts/phase9/font-allocation-probe/20261117T230733178Z-8a835931bfce462197075f20075a0034/.
The independent seven-process-per-arm rerun is at
artifacts/phase9/font-allocation-probe/20261117T231413177Z-97221786522349e5a3ae58cbb7297183/.
After removing PowerShell 7.7's obsolete Export-Csv -NoTypeInformation
argument, the clean final run is at
artifacts/phase9/font-allocation-probe/20261117T232318887Z-7bb3c8eb1e274cb1991c64bdb2b6437f/.
The warning was removed by deleting the obsolete parameter; no warning was
suppressed. The final CSV header is unchanged and contains 57 data rows.

The final run completed 21 fresh processes on 21 unique PIDs with a stable
source fingerprint, an optimized VS 18 2026 x64 Release probe
(/MD /O2 /Ob2 /DNDEBUG), and Qt 6.12.0. The standard Release cache hash is
f71bc0c48c5bc5b5fab72c089ef2f4b7640a42e305e49161dace835523979e47; the probe
SHA-256 is ec1e2a5a077094a6c5020ffa74ee970cb3dd666e00457468d5fffb7a037bedc4.
The independent Tester verified raw stream identity, QPA option, backend,
style resolution, archive/license hashes, and per-process deltas. The QRawFont
engine type was DirectWrite for the first two arms and Win for the GDI arm.

| Arm | Median registration WS / private usage |
|---|---:|
| Packaged Inter TTC, DirectWrite | 489,861,120 / 493,707,264 B |
| Six Inter v4.0 static TTFs, DirectWrite, cumulative after six | 6,287,360 / 6,213,632 B |
| Packaged Inter TTC, windows:nodirectwrite / Qt Win | 405,504 / 0 B |

Each of the six source-requested styles resolved in all arms: Regular 400,
Medium 500, SemiBold 600, Bold 700, BoldItalic 700, and Black 900 upright.
Application-font enumeration added only about 8–12 KB WS and zero median
private usage. Subsequent style resolution added about 6.3 MB WS / 5.7 MB
private for DirectWrite and 1.6 MB / 1.3 MB for Qt Win. This probe does not
construct the full application or establish a P9-06 budget result.

The official Inter v4.0 archive was 27,885,132 bytes, SHA-256
ff970a5d4561a04f102a7cb781adbd6ac4e9b6c460914c7a101f15acb7f7d1a4. Its six
static files match the archive members and local OFL license, but their name
tables report version 4.000; all faces in the bundled TTC report 4.001. The
version difference prevents treating these static TTFs as exact-version
replacements without parity review. Official Inter v4.1 release metadata
reports 4.001; P9-06U will verify and measure the six static files under
DirectWrite. See the official [Inter v4.1 release](https://github.com/rsms/inter/releases/tag/v4.1)
and [font metadata](https://github.com/rsms/inter/blob/master/docs/_data/fontinfo.json).

The GDI arm retained the packaged TTC and selected Qt's Win engine for each
requested style, with near-zero registration delta. It preserves the asset
version but changes Qt's text rendering backend broadly; keep it as a fallback
if the exact-version DirectWrite static-face candidate is unsuitable. Neither
diagnostic alters product files or replaces the strict packaged Release gate.

#### P9-06U exact-version Inter static-face parity and registration probe

P9-06U compared the six original static Inter v4.1 faces with the catalogued
Inter TTC under Qt 6.12 DirectWrite. The official archive was 33,707,794 bytes
with SHA-256
9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e. Its six
static faces report version 4.001, matching the bundled TTC. The independent
Tester re-ran the parity comparator and verified family/style/weight,
Unicode cmap, glyph outlines by glyph ID, horizontal metrics, and selected
OpenType tables. The only reported table differences were the `head` table's
checksum adjustment and modified timestamp; neither source had cmap format
14 records. The check does not compare rasterized pixels.

The first seven-process-per-arm run is
artifacts/phase9/font-allocation-probe/20261117T234749224Z-f7bad8bc995f4e78904aaa7509c30ae4/.
The independent verification is
artifacts/phase9/font-allocation-probe/20261117T235550694Z-3b664a639b0547628f542eb49c962036/.
It completed 14 fresh x64 optimized Release processes, seven per arm, with
QtRawFont reporting DirectWrite for all six faces. The Tester recomputed all
48 CSV metric rows from raw snapshots and verified the process identities,
Qt/runtime/package hashes, source fingerprint, and optimized `/MD /O2 /Ob2
/DNDEBUG` compile settings. The parity report SHA-256 is
8b1016171eb4b1ff3a711ce1681f7d3ca0e8da47b9cb1298600cb2bc3feda1b6.

| Arm | Median cumulative registration WS / private usage |
|---|---:|
| Bundled Inter TTC, DirectWrite | 489,906,176 / 493,772,800 B |
| Six exact-version Inter v4.1 static faces, DirectWrite | 6,328,320 / 6,172,672 B |

This diagnostic does not alter the package and is not a normal-use memory
result. P9-06V is the separate production migration slice: package the six
verified faces, preserve required registration semantics and cleanup, then
run the unchanged full P9-06 Release gate. The strict 262,144,000-byte limit
and 301-sample scenarios remain unchanged.

#### P9-06V exact-version Inter production migration and packaged memory pass

P9-06V replaced the bundled Inter TTC in the application resource inventory
with the six verified Inter v4.1 static TTFs. All six preserve the product's
required Inter family/style/weight identity and exact-version parity recorded
under P9-06U. The source-face hashes are:

| Font | Bytes | SHA-256 |
|---|---:|---|
| Inter-Regular.ttf | 411,640 | `40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82` |
| Inter-Medium.ttf | 417,300 | `97ad806f526e41546d46365bb3a393145f75b7b1568913db74549ad8b8dba872` |
| Inter-SemiBold.ttf | 419,744 | `78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3` |
| Inter-Bold.ttf | 420,428 | `288316099b1e0a47a4716d159098005eef7c0066921f34e3200393dbdb01947f` |
| Inter-BoldItalic.ttf | 425,296 | `948405a16cdc62701da5f4005ed068ca5f4d27061d98f7974ccfc37831d9581d` |
| Inter-Black.ttf | 419,968 | `6342d3ea6dc088b43867f615e807d898adf100c93edb978b8e52c5eb71a264da` |

The source archive is official Inter v4.1, 33,707,794 bytes, SHA-256
`9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e`. The
Inter OFL license is included exactly once in the generated package at
`licenses/fonts/inter/LICENSE.txt` (4,472 bytes; SHA-256
`b3195af0fb14368d1b3b10fb9d3fe503b7163ea083859d2ee553bc74da07c320`). The
package has the six TTFs, no Inter TTC or TTC alias, and unchanged Pretendard
(6,739,336 bytes; SHA-256
`3090ccde0442bb347aa7685d9ba8b17436a60682df6e8f92a9a670de14056e22`). The
generated catalog contains the six Inter entries plus Pretendard and has
SHA-256 `4ce39047d72866696f9c56cbcf6079b30d9c3cb4c895db8dfea19d19e60b88c2`.

The P9-06 runner and evidence validator now require each of the six catalogued
Inter resource events exactly once, successfully, and in order before
`required-fonts-loaded`, `workspace-opened`, and `startup-ready`. The events
are bound to source/catalog/package identities, and a TTC alias is rejected.
Pretendard's event, five startup boundary snapshots, fixtures, 301-sample
cadence, and strict threshold were preserved. The focused validator suite
passed 44/44. Phase 4 inventory tests passed 34 with one expected skip because
the Windows account could not create the symlink required by that assertion.

The first passing full Release run, before the OFL file was installed, remains
preserved at
artifacts/phase9/packaged-idle-memory/20261118T005314031Z-87d21774f4fd48ffa8ccc50b76a2dbf6/.
It is not the accepted package because it lacked the license. The final,
license-inclusive run is
artifacts/phase9/packaged-idle-memory/20261118T014537306Z-212490675a364c8fb30d334495d2f5f3/.
Independent Tester recomputed all 602 raw metric rows and verified both
ordered startup logs, process identities, cadence, compile/package identity,
license identity, and six strict working-set decisions. Evidence validated
`valid` with `budgetResult=passed`, zero errors and zero violations.

| Scenario | Startup-ready WS | External-startup WS | Five-minute WS | Max idle WS | Max idle private bytes |
|---|---:|---:|---:|---:|---:|
| Empty workspace | 199,716,864 | 206,819,328 | 206,180,352 | 206,839,808 | 126,337,024 |
| Representative 96-class workspace | 200,421,376 | 207,167,488 | 206,553,088 | 207,187,968 | 126,910,464 |

All six gated WS checkpoints are strictly below 262,144,000 bytes. The maximum
idle WS was 207,187,968 bytes. Each scenario used 301 ordered samples over
about 300 seconds, a unique x64 PID, normal exit, and sample gaps no longer
than 1.036222 seconds. The empty and 96-class workspaces opened successfully.

The final Release candidate used local Windows x64, VS 18 2026, Qt 6.12.0,
and `/MD /O2 /Ob2 /DNDEBUG`. All 69 application compile records had `/MD`,
`/O2`, `/Ob2`, and `/D NDEBUG`, with no `/Od`. Source fingerprint:
`e717f47b15f8de5404ba55eac46ad37cdf88a367e147b355e9e98b3940d1ddb8`;
cache SHA-256:
`f71bc0c48c5bc5b5fab72c089ef2f4b7640a42e305e49161dace835523979e47`;
package SHA-256:
`e0898b2e6517d8e26989982dad6daffc977f7ecbc65a7bc8ea4b9587c6aecc16`.
The independent Tester matched all 2,381 source-inventory entries, 1,581
package-inventory entries, and the aggregate package hash.

Five affected Debug CTests passed: `ClassMngrFontManagerTests`,
`ClassMngrNextResourceLoaderTests`, `ClassMngrNextBootstrapTests`,
`ClassMngrNextInitialSetupTeacherEntryBootstrapIntegrationTests`, and
`ClassMngrNextNormalScheduleImportIntegrationTests`. The independent Tester
re-ran the latter four migration/integration tests. The ResourceLoader
symlink-escape assertion was skipped because Windows symlink creation was
unavailable. An ordinary (non-elevated) Debug attempt also hit an access-denied
failure in the generated Resources tree and one Normal Schedule run; the same
target/test passed after an elevated build.
The prior generated resource trees were retained under unique `Resources.pre-p9-06V-*`
paths. No full Debug CTest suite has yet run on the final Phase 9 source; it
remains required before the phase exit decision. No GitHub validation was run.

#### P9-07 repeated navigation and idle — valid over-budget result; remediation in progress

The first complete P9-07 Release route is
artifacts/phase9/packaged-idle-memory/20261118T030653547Z-22b2f496a13b4ab7a80e90e3819731e0/.
Its evidence is valid and source-bound, but the strict budget failed. Each
empty and representative 96-class fixture completed 12 successful page
transitions across three cycles of Schedule → Calendar → My Classes → My
Workspace, followed by 301 ordered idle samples over 300 seconds. The runtime
document catalog reported 30 metadata entries and zero loaded PDF bodies; no
PDF body load event occurred. UIA selected the exact named page row and each
event matched the measured app PID. Independent Tester recomputed all 26
over-budget points and matched the validator's valid/budget-failed result.

| Fixture | Startup-ready WS | First Calendar WS | Post-navigation WS | Five-minute WS | Max idle WS |
|---|---:|---:|---:|---:|---:|
| Empty workspace | 201,129,984 | 272,875,520 | 279,908,352 | 275,947,520 | 279,908,352 |
| Representative 96-class workspace | 201,965,568 | 274,464,768 | 280,420,352 | 276,447,232 | 280,424,448 |

The 262,144,000-byte limit is unchanged. Each scenario has 13 strict-limit
violations among 16 gated checkpoints: route steps 2–12, post-navigation,
and the five-minute endpoint. Startup-ready, external-startup, and the first
Schedule transition are below the limit. Return-to-Workspace WS rises across
cycles, but three cycles do not establish a linear leak.

The independent code/telemetry investigation compared P9-07 with the accepted
P9-06V memory run. Immediately before Calendar, the empty-fixture active
Schedule page recorded 210,407,424 WS / 126,431,232 private bytes. Calendar's
created checkpoint was 211,107,840 / 126,754,816; its activated checkpoint
was 251,031,552 / 202,334,208 with threads increasing 17→33. At suspension,
the widget count fell 32→8 and the Qt item-model count 2→0. The run did not
record Calendar QML object counts or `CalendarPageRuntimeMetrics`, so this
does not identify the allocation stack. The `QQuickWidget`/QML activation path
is a hypothesis, not a proven cause; the 91 campus JSON resource loads total
434,441 source bytes and 206,661 decoded bytes and cannot explain the measured
increase by payload size. Source review finds `CalendarPage::onSuspending()`
destroys the view before its borrowed model/provider and releases the cache,
but a diagnostic A/B remains necessary.

The candidate source fingerprint is
`7de09bf8c013202c005bf9362ad78e9540956f18680ebb8f6bc22b641a1e615e`; the
Release cache hash remains
`f71bc0c48c5bc5b5fab72c089ef2f4b7640a42e305e49161dace835523979e47`, and the
package hash is
`5795c4beab6212b13c364cba60781c22e737d43c1792dd1a6156ba42c64dfa2a`. The app
was x64 MSVC Release with `/MD /O2 /Ob2 /DNDEBUG`, Qt 6.12.0; source/cache
hashes were stable. The incomplete build/UIA attempts are preserved separately
as invalid/not-evaluated runs and are not counted as budget results.

At the time of this initial failure report, P9-07 remained active. Subsequent
diagnosis, remediation, and accepted results are recorded below.

#### P9-07 accepted navigation gate and Calendar view remediation — 2026-11-18

The QML activation path was measured with a production-equivalent A/B through
`CalendarViewFactory`. The full QML grid added about 34 MB WS / 37 MB private
relative to the minimal QWidget baseline, while a product-equivalent QWidget
grid added about 11 MB WS / 7 MB private. A lazy-event-list experiment and a
`Label` to `Text` QML experiment did not meaningfully reduce memory and were
discarded. The implementation migrated the production Calendar view to a
QWidget, preserving the 35/42-day layout, academic-week columns, event colors,
activation, navigation, keyboard access, and accessibility behavior. The QML
factory remains available for comparison.

The final accepted packaged Windows x64 Release run is
`artifacts/phase9/packaged-idle-memory/20261118T092055381Z-e63554e873664d1eaface390ddc4ec9e/`.
Both fixtures completed ten ordered Schedule → Calendar → My Classes → My
Workspace cycles (40 successful selections each), then 301 process samples
with one-second cadence through the five-minute idle endpoint. Events were
bound to the measured process and exact UIA-selected page rows. Both document
catalogs reported 30 metadata entries and zero loaded PDF bodies; no body-load
event occurred. Processes exited normally.

| Fixture | Startup-ready WS | External startup WS | Route max WS | Post-navigation WS | Idle max WS | Five-minute WS |
|---|---:|---:|---:|---:|---:|---:|
| Empty | 201,785,344 | 208,629,760 | 219,725,824 | 219,725,824 | 220,250,112 | 202,289,152 |
| Representative, 96 classes | 201,842,688 | 208,969,728 | 223,997,952 | 223,997,952 | 224,305,152 | 205,213,696 |

All gated checkpoints are strictly below the unchanged 262,144,000-byte limit.
Independent Tester recomputed all 602 samples, 80 routes, event identities,
cadence, PDF-body counts, compiler flags, source inventory, package inventory,
and gate decisions. The accepted run has source fingerprint
`13ad0c35356f964b85590caefb2a9c915b05ac012e61a46a4752646e44b92096`, cache
SHA-256 `f71bc0c48c5bc5b5fab72c089ef2f4b7640a42e305e49161dace835523979e47`,
package SHA-256 `da00e1a403bf31018a6c9f6d19df852d1d65b659830891fa5271585bad4384a0`,
and Release executable SHA-256
`c8fc43d556140de08bbe31d9077b35cea85498d530723da2bfb3de35ef500c7e`.
The package used VS 18 2026, MSVC x64 Release, Qt 6.12.0, and
`/MD /O2 /Ob2 /DNDEBUG` with MaxSpeed optimization.

Two later attempts against the same package are preserved as invalid and not
evaluated because `SendMessageTimeout` could not deliver a UIA-bound click
before the route and idle sampling completed. They did not supersede the
passing run. A current-source comparison against the valid gate found exactly
one later file change: test-only `tests/next/calendar_page_tests.cpp`. The
Release executable, cache, package inventory, and all production/runtime
inputs are unchanged; the independent Tester confirmed the passing gate is
reusable for P9-07 runtime-memory acceptance. The current test file compiles
only into `ClassMngrNextCalendarPageTests`.

Visual review initially found that capture fixtures lacked production font
registration and that the title-fit manifest could disagree with pixels. The
final capture test now registers the exact packaged Inter and Pretendard assets
through the production ResourceLoader path, checks English and Korean glyphs,
records build/source/font/fixture provenance and separate RGB/alpha metrics,
and compares actual title pixels against an independently specified full-width
localized reference. Forty suffix-removal negative controls all fail as
expected. The final 16-image capture set is
`artifacts/phase9/calendar-view-parity/20261118T105331504Z-280befaa-f966-4e3a-8ebc-d870d9906349/`;
its manifest SHA-256 is
`e72e7db988c6e68829e7c4db6100d1001322992dfb6960fe6344a962849da22c`.
Independent visual inspection confirmed all eight QML/Widget pairs have
complete English (`MS: Fall Wk 7`) or Korean titles, readable text, matching
event dates/labels, and no title suffix mismatch. The focused Debug build and
`ClassMngrNextCalendarPageTests` CTest passed 1/1.

Separate 20- and 30-cycle representative-only navigation diagnostics are
preserved under `artifacts/phase9/packaged-idle-memory/`. The corrected 20-cycle
review validates 80 selection transitions; the original summary retains its
initial-route counting error, while `summary-review.json` records the
corrected, valid diagnostic. The 30-cycle run
`20261118T083123865Z-3fb2daa3c9e546d6bb24daf99f6ab49a/` validates 120
transitions with no idle phase or budget evaluation. Return-to-Workspace memory
still rises, but the measured private-byte trend declines from about 269
KB/cycle over cycles 11–20 to about 139 KB/cycle over cycles 21–30. Calendar
view/root/session/cache counts return to zero on every suspension; other page
widget/model counts remain stable. These observations do not identify the
allocation owner, prove a leak, or establish a long-term plateau. The formal
ten-cycle strict gate passes; no threshold, cadence, or PDF-body rule changed.

P9-07 is complete. P9-08, the 96-class Sub Prep lifecycle and matched-cleanup
slice, is next. Batch 2 remains active until P9-08 closes; push once after the
batch. The full Phase 9 Debug CTest and final packaged Release exit gates are
still required.

### 2026-11-19 — P9-08 Sub Prep lifecycle accepted; Batch 2 closed

P9-08 completed under the Heavy route. Its final locally packaged Windows x64
Release run is
artifacts/phase9/packaged-next-sub-prep-lifecycle/20261118T183011729Z-2b8b67f6d9a1450393d97632a0ecadeb/.
The current-source fingerprint was
81c8021a8812eade50293ac67fe4832317efe7bd5217b08312d6d1a80d047055; the
Release package hash was
25f2248303ab5199ca40280124f3151941a401cfbbc6f6cfca29e85cb7784958; app
3e8de4508b098aa6f1193ae8d8f7bf6d2b7034a3cb78fc18913d1053ab78357a; cache
f71bc0c48c5bc5b5fab72c089ef2f4b7640a42e305e49161dace835523979e47.
The package was x64 MSVC Release, VS 18 2026, Qt 6.12.0, /MD /O2 /Ob2
/DNDEBUG. The route reused a Release package whose non-Phase-9 product inputs
matched; all Phase-9 source differences were scripts and diagnostics.

Three fresh processes each exercised 11 Sub Prep entry/selection/refresh/
suspension cycles, including one package generation, then 301 one-second idle
samples over 308.703–308.826 seconds. Each process recorded 22 authentic UIA
navigation transitions, 11 class-96 selections, 11 refreshes, 11 suspensions,
48 complete lifecycle snapshots, and zero loaded catalog PDF bodies. Each
produced Sub Prep.pdf and Rosters - By Day.pdf. Parsed output contained 12
Sub Prep pages for 30 weekday classes and five roster pages with six classes
per weekday. The 96-class fixture contained 24 teachers and 7,200 roster
cells; the output-ready copy retained all 66 Saturday classes. The diagnostic
fixture and its transform hashes are bound in the per-run manifest.

| Process | Startup-ready WS | Maximum page-release WS | Maximum idle WS | Idle private median |
|---|---:|---:|---:|---:|
| 01 | 191,496,192 | 227,352,576 | 217,669,632 | 149,852,160 |
| 02 | 190,537,728 | 224,489,472 | 207,986,688 | 149,921,792 |
| 03 | 191,660,032 | 226,119,680 | 223,875,072 | 150,413,312 |

All strict startup, page-suspension/release, and idle working-set checkpoints
were below the exclusive 262,144,000-byte limit. Package-generation working
sets were 222,228,480–223,535,104 bytes; process peak at generation completion
was 222,404,608–224,194,560 bytes, below the separate advisory
536,870,912-byte ceiling. The independent Tester verified the raw metrics,
UIA process/window bindings, fixture integrity, output PDFs, and gate status.

After generation the page returned to its fixed shell: 103 widgets, two
view-attached models, and seven editor controls. Suspended state cleared
selected details, classes, teachers, active rows, and Sub Prep list rows.
Preparation editors and the cached page shell remain present by design.
Page suspension is not PageHost destruction; the gate measured cached-page
suspension and reactivation. Focused Windows Debug CTest passed 4/4:
ClassMngrSubPrepPackageServiceTests, ClassMngrNextBootstrapTests,
ClassMngrNextPageHostTests, and ClassMngrNextSubPrepPageTests. The focused
Phase-9 Python validator suite passed 53 tests; the repaired retention
validator's focused tests passed 3/3 and Python compilation passed.
PowerShell 7.7 parsing and embedded C# checks passed.

Return-to-baseline allowance remains undefined, so no transient memory budget
is claimed. The three-process run shows positive private-byte residuals across
the repeated Sub Prep route; idle private bytes remain stable for five minutes,
but this does not calibrate the per-cycle increase. A separate raw 30-cycle
trace showed private bytes rising 31,141,888 bytes from cycle 1 to 30 and
10,944,512 bytes over cycles 21–30. Independent raw review found its route,
fixtures, package, and measurements internally consistent, but the artifact is
invalid for current-source acceptance: its diagnostic validator changed after
measurement. The corrected fail-closed validator preserves the original
artifacts and leaves the formal memory gate unevaluated. No allocator cause,
leak, or plateau is claimed. Windows Performance Recorder heap capture failed
with Access Denied (0x80070005) under the available token; no ETW heap trace
was produced.

One later fresh diagnostic attempt emitted “Unable to commit the Sub Prep
package” and produced no files. A single current-source packaged RouteSmokeOnly
reproduction using a fresh output root succeeded and generated both PDFs
(Sub Prep.pdf 62,440 bytes; Rosters - By Day.pdf 80,930 bytes). The initial
failure did not reproduce; its direct cause is unknown. There is no independent
output-resource release counter, so output object release is not claimed from
that diagnostic. The accepted gate's completed output and page-content checks
remain valid.

Batch 2 is closed after the P9-08 commit; push the Batch 2 commits together. Batch 3
is active with P9-09 Large Schedule Import lifecycle. The Next route is distinct
from P9-07 normal navigation and P9-08 Sub Prep; the legacy Phase 0 routes
cannot supply packaged Next evidence. Use the separate 96-candidate conflict
workbook for parse/review/cancel and the 20-candidate applicable workbook for
the apply path. No 96-candidate apply claim is made. The full Windows Debug
CTest and final packaged Release exit gates remain open.
### 2026-11-19 — P9-09 packaged Next Schedule Import accepted

The final locally packaged Windows x64 Release run is
artifacts/phase9/packaged-next-schedule-import/20261118T235451002Z-5c2547d52abb4e708fffc9e8af4b7942/.
Its source SHA-256 is
fc36448ac756d46d68a5e9fb87c47fe44ab94ad1a59610ae71b5b30888fbc5ce; package
8e1683b248d5dfb1690f2a06062ad67cdd10ffc9db127ebe6713addb9c365e16; app
26f5f9d7b7cd89932eda9f173beff6c3663b9767d34458bbf2721c244d8e4ac6. The
Release cache stayed at
f71bc0c48c5bc5b5fab72c089ef2f4b7640a42e305e49161dace835523979e47.
The independent Tester recomputed the inventories, validated the two-route
manifest, checked UIA-to-process/window identities, and visually inspected the
Schedule screenshot.

Route A used the 96-candidate, five-block conflict workbook. Its selected
block showed 20 class rows and 20 preview rows, a conflict warning, and
disabled Apply; UIA Cancel produced no apply request. All 18 SQLite table
snapshots and the workspace-file hash were identical before and after, with
table-content SHA-256
b873a73892852d95edab937ac474cec70c04419a5ce5acc3f5939eecacbc6eef2.
Route B used a distinct process and fresh workspace. Its request imported 20
classes, five teachers, 20 class_info rows, and 30 class_times rows. The
persisted class/teacher/day/time association multiset matched exactly with
SHA-256 8d5e5090ef59fb0167dc42a98c4fa0887ea303357f9df6e2cfaeea20fcd08e6b.
The manifest’s Korean strings matched the raw UTF-8 apply trace. The refreshed
Schedule UIA snapshot bound to the measured PID/HWND and recorded 54 visible
items; its inspected screenshot showed imported schedule data, not wallpaper.

| Route | Startup-ready WS | Settled WS | Post-cleanup WS | Idle-tail maximum WS |
|---|---:|---:|---:|---:|
| A | 199,532,544 | 206,471,168 | 244,277,248 | 244,281,344 |
| B | 196,943,872 | 203,583,488 | 242,491,392 | 242,495,488 |

Every startup-ready and post-cleanup working-set checkpoint was strictly below
262,144,000 bytes. Both routes had ten idle samples at 1.000–1.011-second
cadence. The highest operation peak was 244,658,176 bytes, below the advisory
536,870,912-byte ceiling. Post-cleanup private bytes were 144,785,408 (A) and
145,022,976 (B); no numerical private-byte return allowance was defined.
Workspaces and settings files were removed. The prior invalid attempt with
path-separator and Unicode manifest issues and the wallpaper screenshot remains
preserved; only the final rerun is accepted.

The focused Python suite passed 8/8, Python compilation and PowerShell parsing
passed, Release packaging succeeded, and the affected Windows Debug CTests
passed 2/2 before the script-only follow-up. The final current-source evidence
validator passed both routes. The full Debug CTest and final packaged Release
exit gates remain open. No 96-candidate apply claim or private return allowance
is made.

Batch 3 continues with P9-10 Large Calendar Import. Discovery confirms that
P9-07 already covers normal Calendar navigation, so this slice measures import
success, parser-error cleanup, staged-file removal, and one post-result
suspension/re-entry rather than repeating the navigation loop. Use the local
URL override and retained large/malformed responses. No Phase 0 or offscreen
Next route is packaged acceptance.

### 2026-11-19 — P9-10 Large Calendar Import accepted

The final corrected evidence bundle is
[packaged-next-calendar-import-shutdown-dump](../../artifacts/phase9/packaged-next-calendar-import-shutdown-dump/20261119T080427892Z-9ac95b83de184232b1c63824a66e3e65-corrected-evidence/).
Its manifest, summary, and final-checks SHA-256 prefixes are `9d2bc95f`,
`401f851d`, and `74d2c1d0`; validator revision `ced16a2f` and finalization
helper `7e9a3302` are recorded. The measured source fingerprint `2fd4ca5c`
covers 2,398 rows; resource inventory `5d5c7731`, package fingerprint
`2c678cd`, and executable SHA-256 `3f73505c` identify the packaged inputs.
The independent Tester passed. The corrected bundle changes declared metadata
only; raw route logs and process measurements are unchanged, and the original
evidence remains preserved. The original run manifest omitted the source
inventory SHA and expected `about_to_quit` ordering; corrected validation
passed without changing route or process measurements.

Two separate fresh packaged local Windows x64 Release processes naturally
exited 0. The 16,284-byte success fixture (SHA-256 prefix `25a5bee8`) imported
261 dated events (234 Workshop and 27 Holiday in 2026), skipped 104, grew the
Calendar from 180 to 441 events, and preserved exact existing rows. Staging
was released before apply and removed at terminal state; the route performed
one cache refresh, cache settle, and suspension/re-entry. The 69-byte error
fixture (SHA-256 prefix `35cda6bf`) visibly reported missing
`xl/workbook.xml`, parsed/applied/imported/skipped zero rows, preserved the
180-event workspace, removed staging, performed no cache refresh, and had one
suspension/re-entry.

Strict working-set maxima were 223,293,440 and 215,097,344 bytes, below
262,144,000; operation peaks were below the advisory 536,870,912-byte ceiling.
No private-byte return allowance is claimed. Focused Debug Bootstrap and
ThemePresenter CTests passed 2/2, focused Python validator tests passed 10/10,
and the harness passed. A crash was traced to ThemePresenter stylesheet
restoration while shell widgets were alive. The finalizer now destroys the
shell before ThemePresenter; final traces confirm shell release precedes theme
release. This verifies the teardown-order fix without identifying an invalid
widget as the crash origin. The full Windows x64 Debug CTest and final packaged
Release exit gates remain open.

P9-10 is complete; P9-11 Class Transfer export/import lifecycle is active.
Batch 3 remains active through P9-18; push after Batch 3 closes.

### 2026-11-20 — P9-11 Class Transfer lifecycle accepted

The final evidence bundle is
[packaged-next-class-transfer](../../artifacts/phase9/packaged-next-class-transfer/20261119T160942280Z-4b1cf3bb7da340e1920cb4bcbc7c4ccc/manifest.json);
its manifest passed. Independent review confirmed three fresh UIA-bound
packaged Windows x64 Release processes, each naturally exiting 0, for export,
apply, and explicit review-cancel. Route 1 exported 48 classes and 24 teachers
without changing the source workspace. The 1,149,692-byte JSON has SHA-256
`85d4aa69a9345765214176a61c69e6be208b11667b4f8adca3085a153801cdd8`.

Apply and Cancel consumed that same hash. Apply reviewed all 48 classes and 24
teachers with Apply enabled, then imported once: 48 class rows, 24 teacher
rows, 48 class-info relations, 16 regular and 24 intensive schedule rows,
3,600 roster cells, and 20 evaluations with 6,600 nonempty values and 2,000
fixed-schema evaluation rows. Unrelated rows were preserved; validation
accounts for three Classes navigator defaults materialized during navigation.
The only `TeacherValidator` normalization was `Teacher-A..X` to
`Teacher-a..x`. Cancel from the same review-ready state changed status to
`Package review cancelled`, counted one cancellation, disabled Apply, cleared
the package and preview, and preserved all workspace table contents.

Route maximum working sets were Export 234,950,656 B, Apply 241,602,560 B, and
Cancel 233,230,336 B, all below the strict `<262,144,000 B` cap; highest
process peak was 242,483,200 B. Private bytes remain diagnostic and no return
allowance is defined. Preparation changed only five teacher profile columns
for IDs 1–24 and three `class_info` detail columns for class IDs 1–48 in cloned
workspaces; checks preserved IDs, relations, schedule/roster/evaluation rows,
and unrelated columns. Production validation behavior did not change.
Preparation Python tests passed 6/6, validator tests 21/21, PowerShell parsing
and Python compilation passed, and focused Windows Debug Class Transfer CTests
passed 18/18. The [saved final validator report](../../artifacts/phase9/packaged-next-class-transfer/20261119T160942280Z-4b1cf3bb7da340e1920cb4bcbc7c4ccc/logs/validator-output.txt)
is in the evidence bundle.

The preserved successful x64 Release executable SHA-256 is
`e9e26536a61d510412e7b8976417fea649ad3655996cce7e75a08a94f786ff86`; staged
resource inventory SHA-256 begins `5d5c7731`. The [derivation receipt](../../artifacts/phase9/packaged-next-class-transfer/20261119T160942280Z-4b1cf3bb7da340e1920cb4bcbc7c4ccc/build-derivation/derived-receipt.json)
shows product sources match the current tree after excluding five allowed
harness-only scripts. This establishes script-only product-source equivalence,
not a successful build from the exact full current tree: the fresh build failed
because access to the generated `Resources` directory was denied. Earlier UIA
assumptions, invalid fixture profiles, a natural-shutdown timeout, and a
validator mismatch remain in the preserved attempt evidence; this accepted run
resolves the route checks. The independent review did not require a
conflict-error route.

P9-11 is complete. P9-12 QtPdf open/render/close/release/reopen lifecycle is
active; Batch 3 continues through P9-18, with one push after Batch 3 closes.
The full Windows x64 Debug CTest and final packaged Release exit gates remain
open.

### 2026-11-20 — P9-12 QtPdf viewer lifecycle accepted

The [accepted evidence bundle](../../artifacts/phase9/packaged-next-pdf-viewer/20261120T064939281Z-be6b922092554021b6bcc80ecb008d6b/manifest.json)
passed independent review. One fresh packaged Windows x64 Release process
opened, closed, reopened, suspended, and re-entered the Next PDF viewer; it
naturally exited 0 without forced termination. At catalog-ready, all 30
documents were metadata-only, with no QtPdf events or viewer page. Opening
`document_guides_lesson_planning` loaded the 3,533,707-byte packaged PDF
(SHA-256 `295accab548b41f0b4f56e6ab89958ad5c3347646904c155b16a56ebe926caec`)
in Ready/status 2 with error 0: 38 pages, index 0, 540×780 pt, and an active
stream.

Close released the stream and reset page count/rows to zero; the expected
empty `QPdfDocument`/`QPdfView` wrapper pair and cached page remained. Reopen
restored the 38-page document. Navigating to My Workspace suspended the viewer
and released the stream while retaining the same session; re-entry and reopen
restored the PDF. The trace records three loads, stream opens, and releases,
12 retired-document destruction records, and one final view destruction. The
internal `QPdfView` render-completion hook is unavailable. Five screenshots
and UIA snapshots bound to PID 5452/HWND 10292150, with viewport samples, show
the PDF at first open/reopen/re-entry, a blank closed viewer, and My Workspace
during suspension.

All eight working-set checkpoints were below the strict `<262,144,000 B` cap;
maximum working set was 222,203,904 B and process peak was 223,891,456 B.
Focused Python tests passed 9/9, PowerShell parsing passed, and the [final
evidence validator](../../artifacts/phase9/packaged-next-pdf-viewer/20261120T064939281Z-be6b922092554021b6bcc80ecb008d6b/logs/validator-output.json)
passed. P9-12 reused the accepted P9-11 executable
(`e9e26536…f786ff86`), package inventory (`043312b7…16da0b`), and resources
(`5d5c7731…94b35f`). Its product-source fingerprint (`fcf0d919…d41fe8`)
matches P9-11 under the same exclusion of five allowed harness-only scripts.
This establishes script-only product-source equivalence, not a fresh build
from the exact full tree.

The separate full Windows Debug gate remains open. The targeted rebuild in
`build/windows-x64-debug` stopped at `ZERO_CHECK` with MSB4018/
`UnauthorizedAccessException` because access to generated
`ClassMngrNext/Resources` was denied. CTest then used cached binaries and all
four targets failed; the bounded ACL reset was also denied and changed no
permissions. This is not a P9-12 route defect. See the [Debug recheck evidence](../../artifacts/phase9/packaged-next-pdf-viewer/20261120T070658533Z-p9-12-access-ctest-recheck-3f55e798/result.txt).

P9-12 is complete; P9-13 Speaking/report generation and preview close is
active. Batch 3 continues through P9-18, with one push after Batch 3 closes.
The full Windows x64 Debug CTest and final packaged Release exit gates remain
open.

### 2026-11-20 — P9-13 Speaking/report lifecycle accepted

The [accepted evidence bundle](../../artifacts/phase9/packaged-next-speaking-evaluation/20261120T133615659Z-4a085b90bf3842319599ca4f1c24bff5/manifest.json)
passed independent review. The [normal validator report](../../artifacts/phase9/p913-final-validation/20261120T133615659Z-4a085b90bf3842319599ca4f1c24bff5/normal-validator.json)
and [pre-run source freeze](../../artifacts/phase9/p913-final-run-freeze/freeze-20261120T133608274Z-5f4058f6a7524f1f8e6194c9de994e14/source-freeze.json)
are preserved separately. One packaged Windows x64 Release process (PID
38292, main HWND 11863712) naturally exited 0 without forced termination.
UIA-bound navigation visited Speaking Evaluation and My Workspace. Direct and
batch report previews each opened and closed; screenshots show custom report
preview content. The prepared workspace preserved 30 evaluation rows. Default
All Students (25) generated 25 one-page PDFs with expected text in a
50,740,224-byte ZIP (SHA-256
`222980970850b6be54dd5c2f00d530d3f48aadb62365482bfcf59498fea85a14`).

The trace records 25 writer creates and release-before/release-after events;
the staging directory was created, deleted, and absent after completion. The
six stable normal-use checkpoints peaked at 214,372,352 B, below the strict
262,144,000 B cap; the separate operation peak was 368,545,792 B. Focused
Python validator tests passed 23/23, PowerShell AST parsing and Python
compilation passed, and the normal validator passed. Packaged visibility does
not expose internal report asset-session counters; cleanup evidence is limited
to writer, staging, preview, and page lifecycle signals. No production C++
change was made.

The run reused the P9-12 executable/package and establishes product-source
equivalence only, not a fresh exact-tree build. The separate full Windows x64
Debug CTest gate remains open because generated
`build/windows-x64-debug/generated/ClassMngrNext/Resources` access was denied
and CTest used cached binaries. The final packaged Release exit gate also
remains open. P9-13 is complete; P9-14 Staff Directory refresh/re-entry and
AI batch review is active, with an explicit AI route or mapping still needed.
Batch 3 continues through P9-18, with one push after it closes.

### 2026-11-21 — P9-14 Staff Directory and AI batch review accepted

The [Staff Directory bundle](../../artifacts/phase9/packaged-next-p9-14-staff-directory/20261120T210244662Z-e03ad18018b742949cd997ccdc9ef8c1/manifest.json)
and [AI batch review bundle](../../artifacts/phase9/packaged-next-p9-14-ai-batch-review/20261120T210342158Z-6cbd34b02cc7460faee0e67361fefd6c/manifest.json)
passed independent review. Both ran packaged Next Windows x64 Release
processes and naturally exited 0. They bind the same 2,407-entry harness
inventory (`343ff4c1…27834b`) and current-source SHA
`664fbc71…3656c6c`. Both reuse the P9-12/P9-13 package (SHA
`043312b7…16da0b`, executable SHA `e9e26536…f786ff86`), establishing
product-source equivalence only, not a fresh exact-tree build.

The pinned Staff Directory workspace (`d6017672…e8ee7363`) retained exactly
96 rows in each table. Verification covered the 6/5-column layouts, headers,
identities and names, activation/re-entry lifecycle events, and PID-bound
screenshots. Its maximum stable working set was 208,056,320 B and operation
peak 208,056,320 B. The AI run used isolated `maximumRosterRows=100` settings
without changing the global default. Of 100 candidates, 25 valid responses
were parsed and applied; only `col_9` for rows 0–24 changed, preserving the
other 75 rows and all other cells. Its stable working set and operation peak
were both 214,089,728 B, below the strict 262,144,000 B cap. Focused
validator tests passed 16/16, both bundle validators passed, and the
independent Tester passed both bundles. No C++ production change was made; no
return-to-baseline allowance is defined.

P9-14 is complete. P9-15 Schedule refresh and language/theme cycles is active.
Batch 3 continues through P9-18, with one push after it closes. The separate
full Windows x64 Debug CTest and final packaged Release exit gates remain open.

### 2026-11-21 — P9-15 Schedule refresh and language/theme accepted

The [accepted packaged evidence bundle](../../artifacts/phase9/packaged-next-p9-15-schedule-locale-theme/20261120T225845019Z-f75caa8036d44ba18c2c41ce06dbc621/manifest.json)
passed independent review. One packaged Next Windows x64 Release process
(PID 32152, main HWND 4129970) reused the accepted P9-14 package and naturally
exited 0 without forced termination. Its source fingerprint is
`fcf0d919d105b44d52eaca319eef5a42e222c3721e8b022ea970127d41d41fe8`; this
establishes product-source equivalence only, not a fresh exact-tree build.

Using isolated settings, the route traversed English/Light → Korean/Light →
Korean/Dark → English/Dark → English/Light, then left and re-entered Schedule
for two refreshes. Seven UIA and screenshot states show each rendered 7×8 grid
(56 cells, 8 headers) matching all 768 schedule entries in the 96-class
fixture. Before/after snapshots preserved all 17 user tables; SQLite integrity
and foreign-key checks passed.

The stable working-set maximum was 221,945,856 B, below 262,144,000 B; the
separate operation peak was 223,498,240 B, below 536,870,912 B. Focused
validator tests passed 8/8, the normal validator and independent review passed,
and PowerShell parsing passed. Generic lifecycle counters are not claimed as
Schedule metrics, and no return-to-baseline allowance is defined. No
production C++ change was made.

P9-15 is complete; P9-16 repeated workspace open/close is active. Batch 3
continues through P9-18, with one push after it closes. The separate full
Windows x64 Debug CTest and final packaged Release exit gates remain open.
