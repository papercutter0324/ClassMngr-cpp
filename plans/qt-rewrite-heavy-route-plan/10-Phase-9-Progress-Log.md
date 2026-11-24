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

### 2026-11-21 — P9-16 repeated workspace open/close accepted

The [accepted evidence bundle](../../artifacts/phase9/packaged-next-p9-16-repeated-workspace/20261121T023320451Z-9b576dcbfdc84dd0996f279db5ef67e9/manifest.json)
passed independent review. One packaged Next Windows x64 Release process
performed one warm-up and ten measured same-process File > Open/Close cycles
on the unchanged 96-class/768-schedule workspace. The route recorded 12 opens,
11 closes, and a stable PID/HWND, then naturally exited 0. Before/after rows
and schema matched, and SQLite integrity passed. The maximum settled Working
Set was 235,921,408 B, below 262,144,000 B; operation peak was 237,862,912 B,
below the 536,870,912 B advisory limit.

For open checkpoints, Working Set baseline was 231,993,344 B, envelope 0,
final median 180,146,176 B, and fitted trend -63,794,828 B. For closed
checkpoints, baseline was 230,440,960 B, envelope 8,192 B, final median
178,118,656 B, and fitted trend -69,570,634 B. Private Bytes increased about
10.8 MB open and 11.2 MB closed. Separate Recent Files and tiny/large A/B runs
reproduced that secondary signal but did not identify a leak or owner; no
zero-growth or root-cause claim is made. The focused tests passed 25/25 and
the final validator independently passed.

The run reused the accepted P9-15 package, establishing product-source
equivalence only, not a fresh exact-tree build. No production C++ change was
made. P9-16 is complete; P9-17 large roster editing is active. Batch 3
continues through P9-18, with one push after it closes. The separate full
Windows x64 Debug CTest and final packaged Release exit gates remain open:
access to generated `ClassMngrNext/Resources` was denied, CTest used cached
binaries, and no ACL changes were made.

### 2026-11-21 — Initial Windows x64 Debug gate attempt (superseded)

An incremental full Windows x64 Debug build in the standard
`build/windows-x64-debug` folder succeeded under elevated execution. The
normal attempt failed at `ZERO_CHECK`/FileTracker with Access Denied. An
elevated, unfiltered CTest run reported 610/620 passed, 10 failed, and 0
timeouts; this is a failed suite, not a pass. The build and CTest logs are
[here](../../artifacts/phase9/phase9-debug-build-final-20261121.log) and
[here](../../artifacts/phase9/phase9-debug-ctest-elevated-20261121.log).
Normal execution still cannot enumerate generated `ClassMngrNext/Resources`;
no ACL changes were made.

One failure,
`ClassMngrNextSubPrepPageTests::lifecycleActionCheckpointsReportSuccessfulSummaryAndRefresh`,
was traced to test-fixture destruction order: `diagnosticEvents` was destroyed
before the host teardown callback. The member declaration order alone was
corrected in commit `415bf2a58a9a47a68e728eef1555d7d3316edc8f`; production code
and assertions were unchanged. The focused Sub Prep CTest rerun passed 1/1.
Command: `ctest --test-dir build/windows-x64-debug -C Debug -R ClassMngrNextSubPrepPageTests --output-on-failure`.
The full suite has not been rerun after this fix, and the other nine failures
from the original run remain unresolved.

Nine visual capture invocations (shared staff five registered variants,
including one duplicate; teacher two; birthdays two) exited in 0.03–0.07
seconds with empty output and no refreshed PNG or manifest. Existing outputs
are stale from 2026-11-18. Three direct reproductions outside CTest (shared
staff native en_US/light, teacher ko_KR/dark, birthdays en_US/light) each
failed with exit 1 in both normal and elevated contexts, again without output
or files. Their indexes are [elevated](../../artifacts/phase9/debug-direct-visual-repro-elevated-20261121_20261121_132807/logs/index.json)
and [normal](../../artifacts/phase9/debug-direct-visual-repro-20261121_20261121_132559/logs/index.json).
At this attempt the visual cause was unknown; the no-output failures did not
establish a CTest defect or a visual parity result. The later same-day recovery
entry below identifies the font-path cause and records the passing Debug gate.
P9-17 remains active through Batch 3/P9-18; the final packaged Release gate
remains open.

### 2026-11-21 — Local Windows x64 Debug gate passed

The earlier 610/620 CTest result was superseded after the visual capture
startup cause was identified: all three harness families requested `Inter.ttc`,
which the current resource inventory excludes as superseded. The packaged
`Inter-Regular.ttf` static face is present. A four-string substitution across
the three test harnesses now loads it while retaining UI family `Inter`;
assertions, baselines, resource files, and CMake registrations are unchanged.
The Sub Prep fixture member-order correction remains test-only and is committed
as `415bf2a58a9a47a68e728eef1555d7d3316edc8f`.

The standard `build/windows-x64-debug` tree was built incrementally under
elevated execution: the earlier full build and the affected three-target
rebuild both succeeded. Focused CTest passed 9/9 visual capture registrations;
the elevated, unfiltered local Debug CTest passed 620/620 with 0 failures and 0
timeouts. The [incremental full build](../../artifacts/phase9/phase9-debug-build-final-20261121.log),
[affected-target rebuild](../../build/windows-x64-debug/phase9-diagnostics/visual-capture-resource-alignment-20261121_135359/debug-target-build-elevated.log),
[focused CTest](../../build/windows-x64-debug/phase9-diagnostics/visual-capture-resource-alignment-20261121_135359/focused-visual-ctest-elevated.log),
and [full CTest](../../build/windows-x64-debug/phase9-diagnostics/visual-capture-resource-alignment-20261121_135359/full-debug-ctest-elevated.log)
are preserved. Independent Tester review passed. Before capture regeneration,
24 prior files were preserved and SHA-256 verified; see the [diagnostic README](../../build/windows-x64-debug/phase9-diagnostics/visual-capture-resource-alignment-20261121_135359/README.txt)
and [capture manifest](../../build/windows-x64-debug/phase9-diagnostics/visual-capture-resource-alignment-20261121_135359/capture-manifest.csv).

The full local Debug build/CTest gate is passed. Normal execution still cannot
enumerate generated `ClassMngrNext/Resources`; no ACL changes were made. The
final packaged Release gate remains open. P9-17 large roster editing remains
active through Batch 3/P9-18, with one push after Batch 3 closes.

### 2026-11-21 — P9-17 first complete large-roster route; memory gate failed

The [route bundle](../../artifacts/phase9/packaged-next-p9-17-large-roster/20261121T064739565Z-988878b92fd04acca6386b80760296cd/)
records a complete run on the reused P9-16 Windows x64 Release package. Its
normalized product-source fingerprint is
`c00a0b241c00811d03457fcd9f010139efda5054af1783ffaa636763342d5c7e`; this
establishes product-source equivalence, not a fresh exact-tree build. The
fixture contained 100 rows by 156 displayed columns (6 fixed, 150 custom) and
15,200 nonempty cells. Three deterministic edits with valid English/Korean
names were saved and read back after re-entry; other workspace values and
columns were logically preserved. All 17 tables remained present, SQLite
integrity and foreign-key checks were clean, and the process naturally exited
0. The snapshot validator now narrowly normalizes surrogate IDs in
`roster_columns.id` and uses semantic descriptors; prior-bundle database
comparisons pass.

Memory acceptance failed in the matched Class Details state. Ten baseline
Working Set samples were each 237,592,576 B (range 0); after Roster clear and
post-suspension, ten samples were each 260,894,720 B (+23,302,144 B). On
re-entry, ten samples were each 265,035,776 B, 2,891,776 B above the strict
262,144,000 B cap. Private Bytes rose 23,711,744 B after clear and 28,508,160 B
after re-entry. Operation peak was 268,402,688 B, below the 536,870,912 B
advisory limit. Route completion, natural exit, and database correctness do
not override this memory failure; P9-17 is not accepted.

Heap attribution remains unavailable: PID-scoped WPR HeapSnapshot could not
start and returned privilege `0xD0000061` even in reviewed elevated execution;
no ETL or heap stacks were produced. The WPA exporter also could not run because
a dependency is missing. No ACL or registry changes were made. Source review
shows each `RosterTableModel::setData` synchronously calls full-table
`revalidate()`; `snapshot()` converts every cell to owned strings, then
`prepareRosterSaveText` deep-copies the snapshot. This is a source-supported
allocation hotspot, not a proven heap origin. A narrow production fix is in
progress: pass `RosterSnapshot` by value and move it into preparation while
preserving validation. Acceptance remains open pending a rerun. P9-18 remains
the last slice in Batch 3; push once after Batch 3 closes.

### 2026-11-21 — Prior 300 MiB policy and P9-17 exact-source result

At that time, the Phase 9 packaged Release normal-use/startup/settled lifecycle
gate had a desired Working Set target below 268,435,456 B (256 MiB) and an
acceptable ceiling of 314,572,800 B (300 MiB). Values from 268,435,456 B up to
and including the ceiling are acceptable when explicitly reported as target
misses; values above the ceiling fail. Operation peaks remain separate under the existing Phase 9
rules. Preserve return-to-baseline and cleanup allowances, repeated-cycle
checks, source/package equivalence, and database/UI correctness. Historical
measurements and gate decisions remain under their then-current thresholds;
they are not rewritten or retroactively invalidated.

The latest exact-source P9-17 run had baseline Working Set 246,779,904 B (10
samples, range 0), post-clear median 270,020,608 B, re-entry median
274,874,368 B, and operation peak 277,614,592 B. All settled windows were at
or below the 300 MiB ceiling, but post-save, post-clear, suspended, and re-entry
missed the 256 MiB target. The post-clear result exceeded the empirical cleanup
allowance of 246,779,904 B by 23,240,704 B, so cleanup acceptance failed and
P9-17 remains open. The by-value move fix is still under acceptance; this run
does not establish a memory improvement.

The latest full local Windows x64 Debug build and elevated unfiltered CTest
passed 620/620 with 0 failures and 0 timeouts. Logs: [full CTest](../../build/windows-x64-debug/artifacts/p9-17-full-debug-gate-20261121-ctest-elevated.log)
and its sibling build log. This Debug gate does not close the packaged Release
memory gate. Batch 3 remains active through P9-18, with one push after it closes.

### 2026-11-21 — Phase 9 execution paused during P9-17 Release attempt

The current-source Windows x64 Debug full build and elevated unfiltered CTest
passed 620/620 with 0 failures or timeouts. Focused P9-17 validator tests passed
20/20. Independent review verified coordinate/final-contract binding and the
cold-cleanup gate. The by-value move optimization has no new Release memory
result; the exact-source cleanup failure and the 256 MiB target / inclusive
300 MiB ceiling remain as recorded above. Preserve the five-baseline/ten-cycle
repeated-cycle checks on resume.

A standard-token PowerShell 7.7 Release attempt was started after read-only
review found no administrator requirement in the plan, route, or validator.
The session was `FELT-DESKTOP\wfelt`, Low Mandatory Level, with Administrator
role false; it could enumerate six entries under
`build/windows-x64-release/generated/ClassMngrNext/Resources/assets`. The user
then explicitly paused execution, and the worker was interrupted before
returning its checkpoint. Whether CMake ran is unknown; no build or route
result is claimed. Administrator token is not a Phase 9 acceptance requirement;
it was only a guard in the archived exact-build helper.

The frozen pre-pause source inventory was 2,421 files, SHA-256
`fd37daebc8284ee695ad4dc1232dc7eb5f55c11fd8c41f8bb8f36fe3c3b5a7ff`. Preserve
the prior blocker bundle at
`../../artifacts/phase9/p9-17-exact-release-builds/20261121T120455136Z-00e258f92ce5444aa1396a438ebb1ae4/`
and untracked `p9-09-integration-output.txt`. The attempted run made no ACL,
policy, registry, commit, branch, or push changes. On resume, inspect the latest
run-specific artifacts under
`../../artifacts/phase9/p9-17-exact-release-builds/`, verify no process remains,
and recalculate the source inventory after this documentation handoff. P9-17
remains active/open; P9-18 is queued next, Batch 3 remains active with one push
after P9-18, and the final packaged Release exit gate remains open.

### 2026-11-22 — Prior P9-17 route under the 300 MiB policy

At that time, the packaged Release settled lifecycle gate used a strict target
below 268,435,456 B (256 MiB). Samples at or above target through inclusive
367,001,600 B (350 MiB) are acceptable target misses; values above 350 MiB
fail. Operation peaks remain under their separate budgets, and cleanup,
return-to-baseline, repeated-cycle, source/package, and database/UI checks
remain required. Historical results keep the threshold in force when measured;
accepted slices are not retroactively reevaluated.

The prior P9-17 route bundle is under
`../../artifacts/phase9/packaged-next-p9-17-large-roster/20261121T161636026Z-e4c97b8587a94fb8bbed1f7e754a01dd/`.
It ran under the then-current 300 MiB ceiling; build, package, and source
checks passed. Post-save Working Set reached 480,989,184 B, and recorded cycle
windows from cycle 4 onward exceeded the then-active 350 MiB ceiling. Nine cycles
read back edits; cycle 10 closed before final snapshots. Cold return exceeded
its allowance, and no final database/integrity validator ran. The route is
failed/incomplete and P9-17 remains open; do not claim acceptance or a leak.
The no-op vs platform-save diagnostic remains under independent review and is
not accepted evidence. P9-18 remains queued next in active Batch 3.

### 2026-11-22 — Earlier failed-geometry P9-17 observer diagnostic

The unchanged generated exact-source helper completed Windows x64 Release
preflight and build as `FELT-DESKTOP\wfelt`, Medium integrity, non-admin, in
interactive Session 1. The source-bound receipt is
[here](../../artifacts/phase9/p9-17-exact-release-builds/20261121T200415219Z-0b472b3be96c4ee7b899d4e201879569/release-build-receipt.json).
The source inventory tree SHA-256 is
`df26492c8d242786a73640935dad55d510a0294398af7136dd8e28bd1a80ae99`; the
product executable SHA-256 is
`0c90e96b3bc7cd96688aa2a203b04a76f4302a13c2c0f41f41ddbb11541a07cf`. The
local unsigned helper required process-scoped `RemoteSigned` because the
actual user's CurrentUser execution policy is `Restricted`; persistent policy
scopes stayed unchanged. Tool-level filesystem access was used; no UAC/admin
token or ACL change occurred.

The UIA observer diagnostic bundle is
[here](../../artifacts/phase9/p9-17-uia-observer-diagnostic/20261121T200712794Z-3ffc903700bc4ab4b6eb975ea2be9e9c/).
Its full arm failed before measured cycles because target and owner-window
geometry did not intersect. The reduced arm completed five diagnostic cycles:
return medians rose from 272.26 to 316.48 MiB Working Set, maximum Working Set
was 318.34 MiB, and operation peak was 318.58 MiB. No sample exceeded 350 MiB;
2,515/3,408 samples were at or above the 256 MiB target. App slopes were
+29.45 MiB/min Working Set and +29.57 MiB/min Private Bytes; client slopes
were +5.50 and +5.47 MiB/min. The arms are not a valid pair, and an earlier
full arm used different executable/package hashes. These measurements are
diagnostic only; no leak owner is identified.

At that time, the lifecycle policy was Working Set below 268,435,456 B
(256 MiB), with values through inclusive 367,001,600 B (350 MiB) accepted as
target misses; operation peaks remain separate. Independent review found the
inherited `stableCheckpointLimitBytesExclusive=262144000` field is stale
P9-14 metadata, not P9-17 policy. A harness-only reconcile/label is in
progress. Fail-closed stable UIA geometry diagnostics and owner identity
verification were being implemented; a fresh receipt and valid pair were still
required then. The later matched pair is recorded below. P9-17 remains open,
P9-18 is queued, Batch 3 remains active
with one push after P9-18 closes, and the final packaged Release exit gate
remains open.

### 2026-11-22 — Earlier five-cycle matched UIA diagnostic pair

A first standard-user Release build attempt stopped at resource-bake with
WinError 5. An unchanged retry completed successfully; only the successful
source-bound receipt below is associated with this pair. It ran as
`FELT-DESKTOP\wfelt`, Medium integrity, non-admin, interactive Session 1.
The local helper used process-scoped `RemoteSigned`; persistent policy scopes
and ACLs stayed unchanged. No UAC or admin token was used. The receipt is
[here](../../artifacts/phase9/p9-17-exact-release-builds/20261121T210644993Z-ff2ceae5b5e4443c89882ba295fcb5d9/release-build-receipt.json).
Its 2,421-file source-tree SHA-256 is
`ecbf35a6ee0be93e627d67291570a4f93e7bee12eb04a840aac76d5495949517`; product
executable SHA-256 is
`57566c720e8a6e53b1443aa1a7fee4d8997c3bfb283883c9e9162b6f3e5add0c`.

The matched diagnostic bundle is
[here](../../artifacts/phase9/p9-17-uia-observer-diagnostic/20261121T210856196Z-c5b15bd0879a401eb3ace14d79aee5ee/).
The Tester independently verified matching current source, executable,
product source, package tree, fixture, and client logs; five complete cycles
per arm; all 31/31 windows; final readbacks; app exits; and SQLite integrity
and foreign keys. Full-arm Working Set return medians (cycles 1-5): 273.71, 265.27, 277.57,
259.08, and 274.83 MiB (maximum 281.81 MiB). Reduced-arm medians: 272.57,
283.05, 297.37, 310.00, and 308.62 MiB (maximum 314.96 MiB). Every cycle median missed the strict 256 MiB target; no sample
exceeded the inclusive 350 MiB ceiling. Both cold-return cleanup allowances
failed by about 21.4 MiB.

OLS Working Set slopes were +13.179 MiB/min full and +28.504 reduced; Private
Bytes slopes were +34.302 and +29.502 MiB/min. Both arms used the same
sequential PowerShell client PID, so client slopes are not independent. The
mixed diagnostic results establish neither observer causality nor a leak
owner; they were diagnostic-only. At that time no ten-cycle acceptance run
had occurred; the later normal route is recorded below.

The P9-14 `stableCheckpointLimitBytesExclusive=262144000` field is labeled as
historical P9-14 metadata; P9-17 target/ceiling metadata is coherent at
268,435,456 B and 367,001,600 B. A read-only source/lifecycle and
cleanup-baseline comparability investigation is underway. P9-17 remains
Active/open, P9-18 is queued, Batch 3 remains active with one push after P9-18
closes, and the final packaged Release exit gate remains open.


### 2026-11-22 — Prior full-observer route before request move

The prior exact-current-source incremental Windows x64 Release build receipt was
[here](../../artifacts/phase9/p9-17-exact-release-builds/20261121T230224377Z-1216f791bf594971b78c99a17724f871/release-build-receipt.json).
This is an incremental build, not a fresh-tree build. Source tree SHA-256 is
`2531b12fe3f7ce7324962a0332f52684655dbc6bd2b49a9a583535e6dd7eca50`;
product source SHA-256 is
`1f6bbe444b509971d0617c44d1790c7495a97d85589f8f5f8eefd569d2b54f22`;
executable SHA-256 is
`2ee93301417152e0f1a42b09616d637a2c0edc6be36b707a78b6291114b2eb7a`; packaged
tree SHA-256 is
`6f737c40e545a9fc8fe6d6407efbd6fe85e2afff6488bf774e36c917b1f8e5f8`.
Validation was local Windows only under a standard non-admin account.

The prior normal full-observer route bundle is
[here](../../artifacts/phase9/packaged-next-p9-17-large-roster/20261121T230418099Z-7c29622dcaa947648dfac485dd430960/).
Ten measured cycles and 46 windows/460 samples completed without sampling
error. Every window remained below 367,001,600 B (350 MiB), but 34/46
exceeded the 268,435,456 B (256 MiB) target. Cold baseline median was
242,049,024 B (zero range); first return was 265,310,208 B, exceeding the
empirical allowance by 23,261,184 B (22.184 MiB). Five post-primer baseline
windows centered on 270,368,768 B with upper envelope 270,528,512 B; 9/10 cycle
returns exceeded that envelope. Cycle-return OLS slope was +6.878 MiB/cycle.

All ten 100-by-156 grids and 156 headers were verified; 90 readbacks and the
final three values matched. The route recorded 33 SetValue edits and 11 saves.
SQLite integrity passed with zero foreign-key failures; all 17 tables and the
schema were unchanged, with only the three intended `roster_data` changes. The
app closed naturally with exit code 0. Working Set slope was +16.612 MiB/min
with a 328.41 MiB peak; diagnostic Private Bytes slope was +36.616 MiB/min
with a 337.84 MiB peak. Separate operation peak was 344,358,912 B, below the
536,870,912 B allowance.

That route stayed below the 350 MiB ceiling and had a lower Working Set peak
and slope than the preceding full route (413.64 MiB and +34.819 MiB/min), but sequential
runs used different source, so the difference does not establish cause. Private
Bytes growth remained high. The cold-return and plateau cleanup failures meant
P9-17 failed acceptance under the then-active ceiling; no cleanup allowance
changed at measurement time. The later policy decision is recorded below. No causal
credit is assigned to any individual code change. Preserve unrelated
`p9-09-integration-output.txt` and PID 35864. P9-17 remains open; P9-18 is
queued behind it, and the Batch 3 push waits until P9-18 closes. The final
packaged Release exit gate remains open.


### 2026-11-22 — Prior P9-17 gate comparability and move-through hypothesis

Both cold and plateau return gates fail on current samples, but their evidence
is different. Cold baseline and first return use the same PID/HWND and matched-
state string. The initial baseline precedes any Roster visit; its checkpoint
lacks the explicit cleared 100x6 model used later and has no two-second settle
delay. Subsequent windows include the clear checkpoint and settle. The current cold
allowance remains median plus within-window range from one short window (zero
range here), which does not measure between-window variability. Its failure is
valid under the current formula, but the comparison is confounded by process
history/checkpoint. Preserve the gate and correct checkpoint comparability in
new evidence; do not invent a new allowance.

The plateau baseline has five separate settled windows centered on
270,368,768 B with an upper envelope of 270,528,512 B. Nine of ten return
medians exceed that envelope; the fitted nine-interval increase is 64,909,908 B
versus the 159,744 B envelope. This was a repeated-return failure under the then-active acceptance evidence,
independent of the cold comparison. No window breached the 350 MiB ceiling. At
that time, the 256 MiB target, ceiling, and cleanup allowances were unchanged.

Current-source review confirms roster data clears to a blank 100x6 model on
tab leave/page suspension and one widget/model is reused; no accumulating
row/widget owner is evident. The bounded transient-churn hypothesis is full
request copying: model snapshot to widget-owned `RosterSaveRequest`, then
callback and platform adapter const-reference use, followed by a by-value
use-case copy; legacy Roster conversion adds another full representation. A
move-through ownership implementation is in progress. This is a copy-churn
hypothesis, not leak attribution. No reviewable code change, test result, or
route result for this hypothesis exists yet.

Next: make cold baseline and return checkpoints comparable while preserving the
current gate, complete focused local validation of the move-through change, and
obtain a new independently reviewed full route before acceptance. P9-17 remains
open; P9-18 is queued behind it, and Batch 3 push waits until P9-18 closes.


### 2026-11-22 — P9-17 post-move route under prior 350 MiB ceiling

Before the route, independent current-source Windows Debug verification passed
`ClassMngrNextRosterEditorTests` and
`ClassMngrNextApplicationRosterSaveUseCaseTests` (2/2 targets; full editor
class 27 cases); Python preparer passed 4/4 and validator 37/37. All 15 exact
source pins matched.

The exact-current-source incremental Release receipt is
[here](../../artifacts/phase9/p9-17-exact-release-builds/20261122T003619020Z-a6407b5dd3bd4449b28664967b4447df/release-build-receipt.json);
it is incremental, not a fresh-tree build. The source tree has 2,421 files,
SHA-256 `fdedf1ada8c2fbc18d4bae7112aa46fcd2bc21b26530fa54e30c654ce7dc938e`;
product SHA-256 is
`9c3345b498592a5b084a655433776aa9a957877db58e3e88a9bb29fa91f8353b`, app SHA-256
`37ff83db2fbf732a56e55dbc8c9c288e3b478b0d4a946a9244d536a7c59bb998`, and package
tree SHA-256
`60841d81229690a28653d3305348512a225a6729f491b72dd67ea418063455b0`.

The normal full-observer route is
[here](../../artifacts/phase9/packaged-next-p9-17-large-roster/20261122T003844525Z-2926be181b50481ea5138ae812655cf4/).
One primer, five post-primer baseline windows, ten measured cycles, and 46
windows/460 samples completed without sampling error; the app exited naturally
with code 0 and was not force-terminated. The validator exited 1 at
cycle06-post-save Working Set 371,568,640 B, 4,567,040 B above the inclusive
367,001,600 B (350 MiB) ceiling. Final re-entry was 435,896,320 B; 370/460
samples met or exceeded the 268,435,456 B (256 MiB) target. Operation peak was
435,953,664 B, below its separate 536,870,912 B advisory budget.

Cold baseline was 242,626,560 B with zero range; first return was 264,888,320 B,
22,261,760 B above it. Plateau center was 270,389,248 B, envelope 409,600 B,
and upper bound 270,798,848 B. Sample recomputation shows all ten cycle returns
exceeded the plateau envelope; the validator stopped at the ceiling before
evaluating that gate. Both cleanup gates therefore fail on available evidence.
No threshold or allowance changed.

UI/DB/function checks passed: all 100x156 grids and 156 headers; 33 SetValue
edits and 11 saves; all 90 cycle readbacks and final three values matched;
SQLite integrity passed with zero foreign-key failures, 17 tables, unchanged
schema, and only the three intended `roster_data` values changed. The unrelated
PID 35864 was untouched. P9-17 remains unaccepted and independent route review
is pending.

Compared with the prior sequential route (328.41 MiB Working Set maximum), this
run reached 415.57 MiB. Source and harness differ, so the comparison does not
establish that the request move caused the increase or that it saved memory.
Source review confirms one full request copy was removed at the platform-to-use-
case boundary; legacy UTF-16-to-Qt Roster conversion remains. Full `revalidate()`
still scans visible cells and builds duplicate-name structures after edits;
load/re-entry converts the full Qt Roster to a snapshot and then model rows.
Clear still returns to a blank 100x6 model and reuses one editor/model, with no
accumulating page/widget evidence. These are churn leads, not leak attribution.
Heap diagnostics lack a post-save checkpoint and WPR privilege is unavailable.

At that time, next review was the repeated full revalidation and remaining
legacy conversion, followed by focused local validation and independent
full-route review. The then-active 268,435,456 B target, inclusive
367,001,600 B ceiling, operation peak budget, and cleanup gates applied; the
later policy decision is recorded below. P9-18 remains
queued behind P9-17; Batch 3 push waits until P9-18 closes. Local Windows
validation used a standard non-admin account only.


### 2026-11-22 — User-selected 450 MiB policy and P9-17 reclassification

The active packaged Release normal-use Working Set policy is now a strict
268,435,456 B (256 MiB) target, with values through 471,859,200 B (450 MiB)
inclusive accepted as target misses and values above failing. Operation peaks
remain under their separate existing budgets. The 256 MiB target, cold and
post-primer cleanup formulas/gates, operation budgets, and historical run
measurements remain unchanged; historical entries retain the threshold in
force when measured.

The latest P9-17 route ran with the prior 367,001,600 B (350 MiB) ceiling;
its validator exited at cycle 6 above that ceiling. Its
maximum settled Working Set, 435,896,320 B, is below the active 471,859,200 B
ceiling by sample-based reclassification. No route rerun or 450 MiB validator
was performed, so this is not a validated pass. 370/460 samples remain at or
above the 256 MiB target. Operation peak 435,953,664 B remains below the
separate 536,870,912 B budget.

The cold-return gate still fails: first return was 22,261,760 B above its
zero-range baseline. All ten cycle returns exceeded the independently
recomputed plateau upper envelope of 270,798,848 B (center 270,389,248 B;
envelope 409,600 B). Both cleanup gates therefore remain failed; P9-17 is not
accepted. The user changed only the normal-use ceiling: no target, cleanup
gate/allowance, or operation budget changed. P9-18 remains queued behind P9-17,
and Batch 3 push waits until P9-18 closes.


### 2026-11-22 — P9-17 full-observer attempt stopped at UIA geometry guard

After the 450 MiB policy selection and validation-neutral custom-cell fast
path, independent tests and exact source pins passed. The standard non-admin
exact-source Release build succeeded; receipt:
[here](../../artifacts/phase9/p9-17-exact-release-builds/20261122T015622000Z-independent-d7e1352175bb44de8ec5b429e8c45a32/release-build-receipt.json).
Source-tree SHA-256 is
`8cf6007d4f27464115a3f4999cdc46fdcdeb02b7236b3f6e05ee99741ed6ab9a`.

Full-observer route attempt
`20261122T015800770Z-95c59e99c12d48c08652c738585e3b49` stopped before
workspace open after eight retries at the fail-closed UIA geometry guard. The
target rectangle `(5761,1921,616,41)` (HWND 0) lay 121 px outside the measured
same-process owner `(4600,1441,1040,759)`. Owner DPI was 96; system DPI was 120.
Geometry evidence is
[here](../../artifacts/phase9/packaged-next-p9-17-large-roster/20261122T015800770Z-95c59e99c12d48c08652c738585e3b49/uia/open-teacher-profile-click-geometry-failure.json).
No workspace opened, and no roster cycles, memory windows, DB snapshots, or
memory acceptance evidence were collected. This is a route-incomplete geometry
stop, not a memory result. Fail-closed harness geometry repair is in progress.

The last completed full-observer route remains the 2026-11-22 post-move run
recorded above. Under the user's active 471,859,200 B (450 MiB) ceiling, its
maximum settled sample of 435,896,320 B passes by sample-based reclassification
only; that validator was run under the prior 367,001,600 B (350 MiB) ceiling,
and no 450 MiB rerun occurred. 370/460 samples remain target misses. Cold and
plateau cleanup gates still fail, so P9-17 remains unaccepted. The 256 MiB
target, inclusive 450 MiB ceiling, cleanup gates/allowances, and separate
operation peak budget remain unchanged since the policy selection. P9-18 stays
queued behind P9-17; Batch 3 push waits until P9-18 closes.

### 2026-11-22 — UIA DPI semantics and P9-17 geometry handoff

UI Automation `BoundingRectangle` uses physical screen coordinates ([UIA
screen-scaling guidance](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-screenscaling),
[property identifiers](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-automation-element-propids)).
`GetWindowRect` is DPI-virtualized, and DPI-sensitive API results may reflect
the calling thread's awareness context ([GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect),
[high-DPI thread guidance](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows)).
Immediately before `GetWindowRect`, record the caller thread ID and exact
context returned by [`GetThreadDpiAwarenessContext`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getthreaddpiawarenesscontext)
on that same thread; obtain the ID with
[`GetCurrentThreadId`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid).
Record the actual host HWND passed to the call; query that window's context with
[`GetWindowDpiAwarenessContext`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowdpiawarenesscontext)
and verify its creator thread/process with
[`GetWindowThreadProcessId`](https://learn.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-getwindowthreadprocessid).
Reconcile the rectangles into a verified common coordinate space using a
documented conversion appropriate to that context; never apply blind 120/96
scaling. `NativeWindowHandle` is zero when an element has no window handle;
zero alone does not prove that it is contained by the host root
([NativeWindowHandle property](https://learn.microsoft.com/en-us/dotnet/api/system.windows.automation.automationelement.automationelementinformation.nativewindowhandle?view=windowsdesktop-10.0)).
Keep geometry refusal fail-closed until both coordinate-space reconciliation
and correlation to the actual host HWND and root are verified.

These API semantics inform the geometry guard but do not change route
acceptance. The latest full-observer attempt remains stopped before workspace
open, with no memory samples, database snapshots, or memory acceptance
evidence. P9-17 remains open; the 256 MiB target, inclusive 450 MiB ceiling,
cleanup gates, and route status are unchanged.

### 2026-11-22 — P9-17 standard-token helper refused identity mismatch

The exact-source helper launch for the UIA DPI repair was refused by its
identity guard before CMake. The [refusal record](../../artifacts/phase9/p9-17-exact-release-builds/20261122T025721240Z-uia-dpi-repair/standard-token-launch-refusal.json)
and [launch output](../../artifacts/phase9/p9-17-exact-release-builds/20261122T025721240Z-uia-dpi-repair/standard-token-launch-output.txt)
show the observed token `Felt-Desktop\CodexSandboxOffline`, SID suffix
`-1003`, Medium integrity, non-admin, Session 1. The helper requires the
standard non-admin desktop identity `FELT-DESKTOP\wfelt`; prior receipts use
SID suffix `-1001`. CMake was not invoked, and no Release receipt or packaged
route was produced.

This is an identity mismatch, not an elevation blocker. Elevated relaunch is
explicitly not a valid path. Resume the documented helper route when the
expected standard-user identity is available; no alternative documented P9-17
receipt route exists. This attempt adds no build or route result and no memory
samples. P9-17 remains open; the 256 MiB target, inclusive 450 MiB ceiling,
cold/plateau cleanup gates, operation budget, and P9-18 queue position are
unchanged.

### 2026-11-22 — P9-17 v2 helper stopped at resource inventory access

The tracked `scripts/phase9/p9_17_build_current_source_release.ps1` helper
(receipt v2) correctly verified a standard non-admin, Medium-integrity,
interactive Session 1 context. Its account-name gate is resolved; the actual
identity remains `Felt-Desktop\CodexSandboxOffline`. Attempt
`20261122T042948942Z-f752a6df2de74ea98db790e95370fc9c`
([attempt artifacts](../../artifacts/phase9/p9-17-exact-release-builds/20261122T042948942Z-f752a6df2de74ea98db790e95370fc9c/))
then failed before CMake while recursively inventorying
`build/windows-x64-release/generated/ClassMngrNext/Resources/assets` with
Access Denied. Read-only diagnosis found `Resources` metadata readable, but
directory listing and DACL access were denied; `assets` was inaccessible. The
blocked DACL and owner could not be identified. Parent `ClassMngrNext` is
accessible and grants `CodexSandboxUsers` Modify.
`cmake/next_resources.cmake` fixes the resource
output at `${CMAKE_CURRENT_BINARY_DIR}/generated/ClassMngrNext/Resources`, with
no documented override in the standard build tree.

No ACL changes or elevation occurred. CMake was not invoked; no Release
receipt, package, route, or memory samples resulted. Harness evidence only:
the executor reported 49 combined tests, the independent validator passed
45/45, and AST plus 12 route cases passed. These checks do not verify the live
route or memory acceptance. Resume when a standard non-admin execution context
can read the existing Resources subtree or owner-provided standard access is
available. Do not elevate or use an undocumented alternate build tree. P9-17
remains open, P9-18 remains queued, and the 256 MiB target, inclusive 450 MiB
ceiling, cleanup gates, and operation budget are unchanged.

### 2026-11-22 — P9-17 exact build and UIA observer diagnostic

The default `FELT-DESKTOP\CodexSandboxOffline` Medium, non-admin Session 1
could not enumerate the existing Release Resources subtree. A read-only check
through tool-level workspace access under the active-console standard identity
(SID suffix `-1001`, Medium, non-admin, Session 1) could enumerate that same
subtree, confirming the access failure is context-specific. The exact-source
helper then completed an incremental x64 Release build in the standard
`build/windows-x64-release` folder from 2,422 source files (SHA-256
`5ea5a1d3289dd95c3da82e717e0b76c12d1114d969ecc6d9880b2cdd77f5ffcc`); see the
[build receipt](../../artifacts/phase9/p9-17-exact-release-builds/20261122T075749059Z-7b63add16f814305bdff113312984be3/release-build-receipt.json).
The focused suite passed 47/47.

Two five-cycle comparisons used the same app, executable, package, product
source, and fixture, with counterbalanced full/reduced UIA observer order:
[FullThenReduced](../../artifacts/phase9/p9-17-uia-observer-diagnostic/20261122T075945818Z-0421654cae824f49bfa5a69852b6b143/comparison-manifest.json)
and
[ReducedThenFull](../../artifacts/phase9/p9-17-uia-observer-diagnostic/20261122T080450007Z-5dc33824c6874fe4aade7739aae825b7/comparison-manifest.json).
Both are diagnostic-only and excluded from acceptance. All four arms remained
below the inclusive 450 MiB ceiling and 512 MiB operation budget, but each
missed the strict 256 MiB target in 25/31 windows. Cold return failed in every
arm by 21.973–22.316 MiB; every cycle return exceeded the plateau noise
envelope, with fitted return growth 40.487–64.436 MiB versus 0.039–0.164 MiB
envelopes. Full-minus-reduced Private Bytes fitted growth was +15.305 MiB
(FullThenReduced) and +13.439 MiB (ReducedThenFull), while reduced arms grew
about 52.1–52.6 MiB. This suggests possible observer contribution, not
causality or a leak. Working Set contrast changed sign by order (20.294 MiB
paired order effect), so attribution is inconclusive.

P9-17 remains open: these diagnostic routes supply no normal acceptance
result, and cold/plateau cleanup gates still fail. No acceptance, waiver,
commit, or push was made; the 256 MiB target, inclusive 450 MiB ceiling,
cleanup gates, and 512 MiB operation budget are unchanged. P9-18 remains
queued; Batch 3 push waits until it closes.

### 2026-11-22 — P9-17 client-GC reference diagnostic

The active-console standard identity completed the exact-source incremental
x64 Release build in `build/windows-x64-release`; receipt source-tree SHA-256
is `005121d59d6d4e8796a590475a43839cf806591b66596f7cfe3fce709059b816`.
The [receipt](../../artifacts/phase9/p9-17-exact-release-builds/20261122T093754300Z-0ef971a592ad454880c49d9d431eb32e/release-build-receipt.json)
confirms 2,422 files, standard non-admin Medium integrity, and interactive
Session 1. The earlier resource-inventory denial was specific to
`CodexSandboxOffline`; it is not the current build blocker.

The matched [NoGcThenGc](../../artifacts/phase9/p9-17-client-gc-reference-diagnostic/20261122T093947724Z-fafc6706ae024e309beee5800cb22317/comparison-manifest.json)
and [GcThenNoGc](../../artifacts/phase9/p9-17-client-gc-reference-diagnostic/20261122T094950781Z-378a4777b30a45178d52b5197b184c05/comparison-manifest.json)
comparisons are diagnostic-only. Independent audit confirmed matching
product, executable, package, fixture, action signatures, and hashes; all four
five-cycle arms completed. Client-GC arms each recorded five forced
collections after roster-absence proof, with generation deltas (2,2,2); client
estimated memory fell 39.75–57.02 MiB per boundary. No-GC arms recorded only
no-op events and zero collections.

Across cycles 1–5, application Working Set rose 64.273–64.824 MiB and Private
Bytes 65.777–66.559 MiB. Paired GC/no-GC differences were at most 0.609 MiB
Working Set and 0.684 MiB Private Bytes, so no application effect was
distinguishable. Working Set peaks were 345.36–346.83 MiB, below the inclusive
450 MiB ceiling; each arm had 220–230 of 310 windows missing the strict
256 MiB target.
Cold return failed by 22.04–22.36 MiB. Plateau acceptance analysis was null in
this diagnostic; the prior full/reduced observer evidence above found every
cycle return beyond its plateau gate. The app exited naturally in all arms;
no sidecars or temporary files remained.

The fixture payload audit found about 0.717 MiB per roster text copy (about
2.15 MiB across three simultaneous representations), insufficient by itself
to explain the fitted growth; container overhead and churn remain unmeasured.
No leak or allocation owner is proven. No normal 10-cycle acceptance route ran
and no gate was waived. P9-17 remains open, P9-18 queued, and Batch 3 push waits
for P9-18. The strict 256 MiB target, inclusive 450 MiB ceiling, cleanup gates,
and 512 MiB operation budget are unchanged.

### 2026-11-22 — P9-17 default-heap diagnostic and post-delete checkpoint

The exact-current-source x64 Release receipt SHA-256 is
`1651a64458eff31820b8682bfa443de03aae30897181b00bdbe984be387b1ac4`. The
four-arm diagnostic manifest is
[here](../../artifacts/p9/20261122T123519219Z-a2f5358d58e04aa6a1b56eecc6483803/comparison-manifest.json).
Each arm has 40 validated application heap events. Maximum settled Working Set
ranges from 327.7 to 349.5 MiB, below the inclusive 450 MiB ceiling; operation
peaks stay below the separate 512 MiB budget. Across cycles 1–5, default-heap
`cbAllocated` grows about 41.5 MiB in reduced arms and 54.7–54.9 MiB in full
arms, alongside larger committed/reserved growth. These arms are diagnostic
only and establish neither an allocation owner nor a leak.

The diagnostic release checkpoint was captured inside the derived
`RosterEditorWidget` destructor before `delete editor` returned. The executor
added `roster-widget-after-delete-returned-after-child-destruction` after that
call, with a focused QPointer-order test and runner checks. Independent static
review confirms the checkpoint contains no text. The complete Python P9-17
suite passed 56/56 and the PowerShell 5.1 parser check passed. C++ build/CTest
did not run on this updated source: the standard non-admin
`CodexSandboxOffline` token cannot enumerate
`build/windows-x64-debug/generated/ClassMngrNext/Resources/assets`. No
elevation or ACL change was made; the older 2/2 CTest result is not current-
source evidence. WPR HeapSnapshot requires a privilege unavailable in the
standard context.

Cleanup checks show post-primer baseline drift passes, cold return fails by
22.26 MiB, and all ten cycle returns exceed the 0.4 MiB plateau envelope.
Process exit and sidecar/temp cleanup passed. The last complete normal route's
435.9 MiB maximum is below the current ceiling only by sample reclassification;
it is not current-source 450 MiB acceptance. The user reports higher memory on
Windows than on Linux/macOS, but this remains an unquantified observation, not
matched evidence. `PrivateUsage` represents Windows PrivateUsage, macOS
`phys_footprint`, and Linux PSS; matched WS/RSS routes are needed before
cross-platform attribution. P9-17 remains open, P9-18 queued, and Batch 3 push
waits until P9-18 closes. The 256 MiB target, inclusive 450 MiB ceiling,
cleanup gates, and operation budget are unchanged.

### 2026-11-23 — P9-17 focused Debug verification

The post-delete checkpoint source built the `ClassMngrNextPageHostTests` and
`ClassMngrNextRosterEditorTests` targets in the existing
`build/windows-x64-debug` folder, Debug configuration. Focused CTest passed
2/2 under non-Administrator Medium-integrity Session 1, SID
`S-1-5-21-445605264-701173606-3743913551-1001`. The independent Tester audited
the saved logs and QPointer assertions; this was not a second test execution.
The CMake target includes `selected_class_editor.cpp` and
`selected_class_analytics_model.cpp`. The initial link failure and correction
are recorded in the historical build entry. Logs:
[focused Debug build and CTest](../../artifacts/phase9/p9-17-exact-release-builds/20261122T161434055Z-post-teardown-focused-debug-link-fix/logs/).

A current-source Release build and matched five-cycle full/reduced after-delete
heap diagnostic have completed; see the next dated entry. The focused Debug
verification does not establish Release memory acceptance. P9-17 remains open,
P9-18 queued, and Batch 3 push waits for P9-18. The strict 256 MiB Working Set
target, inclusive 450 MiB ceiling, separate 512 MiB advisory operation peak,
and cold/plateau cleanup gates are unchanged.

### 2026-11-23 — P9-17 current-source Release route and post-delete heap comparison

The current-source incremental x64 Release build succeeded in the standard
`build/windows-x64-release` folder from 2,422 files. Receipt tree SHA-256 is
`494774b15999c3930437822f644996cb677c1a128fdb4d76a5bd586bd10e0450`; the
packaged executable SHA-256 is
`c740d87cc456b3e176d168a753c1625518e44a40230777ae8c4db3ea40650ebf`. The
[build receipt](../../artifacts/phase9/p9-17-exact-release-builds/20261122T180453000Z-p9-17-ps51-indexof-fix/release-build-receipt.json)
records 192 resources and a standard non-admin Medium-integrity Session 1
token (SID ending `-1001`). The updated P9-17 suite passed 57/57; PowerShell
5.1 parsing and positive/negative ordinal-`IndexOf` runtime checks passed.

The [10-cycle packaged Release route](../../artifacts/p9a/20261122T182512489Z-ac354e0c5a4e4903a5658c033fbbbf0c/manifest.json)
completed naturally (exit 0) with 100×156 roster data, SQLite integrity/FK
checks, three target-row changes, and zero sidecar or temporary files. It
recorded 460 Working Set samples: 370 samples across 37 windows missed the
strict 268,435,456 B (256 MiB) target. The inclusive 471,859,200 B (450 MiB)
ceiling passed every sample (maximum 382,664,704 B), and the separate
536,870,912 B (512 MiB) advisory operation-peak budget passed at 388,730,880 B.
The cold cleanup gate failed: matched baseline median/range were
242,814,976/0 B, first return was 265,875,456 B, exceeding the allowance by
23,060,480 B. The validator stopped before its plateau calculation. Recomputing
from raw samples gives a plateau center of 271,589,376 B, a 270,336 B envelope,
and upper bound of 271,859,712 B; all ten cycle-return medians exceed that
bound, with a fitted trend of 87,881,541.8 B. Thus the normal route completed,
but P9-17 failed its cold and plateau cleanup gates and remains open.

The [counterbalanced four-arm default-process-heap diagnostic](../../artifacts/p9h/20261122T180803249Z-af39be17f4094fb7ac18a6084bbb394a/comparison-manifest.json)
uses the same exact source, product, executable, package, fixture/preparer,
actions/readbacks, and checkpoint coordinates in both orders; every comparison
check passed. Each five-cycle arm completed naturally with 47 validated heap
events. The result suggests about 16 MiB additional default-heap allocation
from full UIA by cycle 5; reduced-observer allocation still grows about 41 MiB
by cycle 5.
`cbAllocated` falls about 4.2 MiB between each observed inside-destructor and
after-delete checkpoint. These are default-process-heap diagnostics, not
Working Set measurements; they identify neither an allocation owner nor a
leak.

Two earlier reruns produced no application-memory evidence: one failed the
runner's classic `MAX_PATH` guard on 285–288 character fixture paths; the
shallow-root rerun then hit a PowerShell 5.1 `String.Contains` overload. The
runner now uses ordinal `IndexOf`, and the successful receipt/run above is for
that exact source. A WPR administrator preflight ended with UAC operation
cancelled; the tool remained Medium-integrity/non-admin, no trace was collected,
and no registry or ACL state changed. The prepared-SQL-reuse investigation is
recorded in the following entry. No cleanup allowance was waived and
no commit or push was made. P9-18 remains queued; push after it closes Batch 3.
The <268,435,456 B target, inclusive 471,859,200 B ceiling, separate advisory
536,870,912 B peak, and cold/plateau cleanup gates remain unchanged.

### 2026-11-23 — P9-17 SQL-reuse Release route

The [exact-source 10-cycle Release route](../../artifacts/p9a/r20261122T192719Z/20261122T192720300Z-55cda0046ba144b189be428145c79ecf/manifest.json)
used source tree SHA-256
`f72d3ba6fee1ecf823953592954df0d2625256118d7070f89f092a8d8061f458` (2,422
files) and executable SHA-256
`fcbea7749a037153fa2e0e241b311bcc152eabacb7e8208e7fdb439dda76e390`. The
[Release receipt](../../artifacts/phase9/p9-17-exact-release-builds/20261122T192116446Z-roster-write-retry/release-build-receipt.json)
records the standard Windows identity (SID ending `-1001`), Medium integrity,
non-admin Session 1. The app completed ten cycles and exited naturally; DB/UI/
readback/file cleanup checks passed.

Of 460 Working Set samples, 380 missed the strict 268,435,456 B (256 MiB)
target; no sample exceeded the inclusive 471,859,200 B (450 MiB) ceiling
(maximum 380,026,880 B). The separate advisory operation peak was 380,030,976 B,
below 536,870,912 B (512 MiB). Cold cleanup failed: first return exceeded the
baseline median by 23,412,736 B. The validator short-circuited before plateau
analysis; independent raw-sample recomputation found center 273,403,904 B,
envelope 270,336 B, upper bound 273,674,240 B, and all ten return medians beyond
that bound, with fitted cycle trend 62,506,151.6 B. Compared with the previous route, cold
excess worsened by 352,256 B and 10 more samples missed the target. Reusing the
prepared SQL INSERT showed no detectable cleanup benefit; no root cause or leak
is established.

The previous [four-arm heap diagnostic](../../artifacts/p9h/20261122T180803249Z-af39be17f4094fb7ac18a6084bbb394a/comparison-manifest.json)
remains diagnostic-only: full UIA adds about 16 MiB of default-heap allocation
by cycle 5, while reduced-observer heap growth remains. The heap metric is not
Working Set and does not identify an owner. The save-stage diagnostic is
recorded in the following entry. UAC/RunAs attempts were cancelled; no WPR
trace or registry, ACL, or machine-state change occurred. No allowance was
waived, and no commit or push was made. P9-17 remains open, P9-18 remains queued,
and the Batch 3 push waits until P9-18 closes. Policy remains the strict
268,435,456 B target, inclusive 471,859,200 B ceiling, separate advisory
536,870,912 B operation peak, and required cold/plateau cleanup gates.

### 2026-11-23 — P9-17 save-stage diagnostic and row-streaming candidate

The [four-arm save-stage diagnostic evidence](../../artifacts/phase9/p9-17-save-stage-diagnostic-retry/20261122T205732627Z-b5a5828f258942ee808f2db9bae18bda/)
was built from source SHA prefix `1ed38ce2…` and executable SHA prefix
`c24c5b20…`; the [Release receipt](../../artifacts/phase9/p9-17-exact-release-builds/20261123T070000-save-stage-diagnostic/release-build-receipt.json)
records 2,422 files and the standard SID ending `-1003`, Medium integrity,
non-admin Session 1. Toolchain was VS2026/v145 with Qt 6.12. The comparison
manifest SHA-256 is
`a4b50e33447c7f4d8bc19334c19e27fb7c0bd5a89a258410ddad784e24a4e50d`. Four
five-cycle arms passed per-arm validation with 89 events each, including 42
save-stage events. The top-level full/reduced comparison remains failed:
action signatures were 50 vs 48 because full mode includes two scroll actions.
Each within-mode pair matched and every other comparison check passed. This is
diagnostic-only, not acceptance evidence; cross-mode deltas cannot be attributed
to the UIA observer.

An independent text-free join of process metrics, UIA action timestamps, and
app checkpoints reported pooled per-save PrivateUsage medians of +1.754 MiB
snapshot, +1.027 MiB preparation, +1.555 MiB legacy conversion, and +0.762 MiB
repository save in full mode; reduced medians were similar (+1.682, +1.094,
+1.588, and +0.760 MiB). Before-snapshot PrivateUsage growth from cycles 1–5
was 65.395/64.133 MiB in the full arms and 49.719/48.949 MiB in the reduced
arms; cross-mode comparison is invalid. Full-mode phase summaries showed
roster-open +3.76/+4.06 MiB; three edit action brackets of about +0.7–1.3 MiB
each (about +2.9 MiB combined versus +6.2 MiB for the broader edit phase); an
internal save-stage increase of about +5.0 MiB versus +0.4 MiB in the save
action bracket; near-zero post-save change; and +3.2–4.0 MiB on return.
Scroll-action deltas were near zero. These are descriptive phase observations,
not allocation ownership. UIA timestamps mark invocation bounds, not
asynchronous app completion.

The next bounded production candidate is row-wise snapshot-to-repository
streaming to avoid materializing a second complete Qt `Roster`. The candidate
targets the measured legacy-conversion stage (+1.555 MiB PrivateUsage), but that
stage is not presumed to be the root cause. Row streaming was not yet
implemented when this candidate was recorded. P9-17 remains open after the
preceding SQL-
reuse route's cold and plateau failures; P9-18 remains queued and Batch 3 push
waits until it closes. No waiver, commit, or push was made. The 256 MiB target,
inclusive 450 MiB ceiling, separate 512 MiB advisory peak, and cleanup gates
remain unchanged.

### 2026-11-23 — P9-17 row-streaming implementation and focused validation

Row-wise snapshot-to-repository streaming is implemented in
`RosterRepository::WindowSaveRowSource`. The [current-source focused validation
bundle](../../artifacts/phase9/p9-17-stream-roster-save-verification/20261123T034100Z-validation/)
records a 2,422-file source inventory with SHA-256
`36786ead8015db222ac82607c892c359d5f1e4084020a985885cb132a8535a79`.
Focused CTest passed 5/5, the renamed direct Qt test passed, the P9-17 Python
suite passed 58/58, and PowerShell 5.1 parsing passed.

The matching [exact-source Release receipt](../../artifacts/phase9/p9-17-exact-release-builds/20261123T035800Z-current-367/release-build-receipt.json)
was built with VS18/v145 under the standard account SID ending -1001. This is
supplemental build evidence, not acceptance. The VS2022/v143 route remained
outstanding at that earlier handoff; later installation and remaining
receipt/build gates are recorded below.

Two UIA route attempts (p9g and p9i) could not stabilize the UIA action
"Open Teacher Profile" before roster cycles, so they provide no
memory-gate results.
P9-17 remains open; P9-18 remains queued and the Batch 3 push waits for it.
The strict 268,435,456 B (256 MiB) target, inclusive 471,859,200 B (450 MiB)
ceiling, separate 536,870,912 B (512 MiB) advisory operation peak, and cold/
plateau cleanup gates remain unchanged.

### 2026-11-23 - P9-17 UIA hit-test diagnostic and pre-install status (superseded)

The [p9g route](../../artifacts/p9g/20261123T040028898Z-3dd65beacd3442be82d66961e4fb7f86/)
and [p9i route](../../artifacts/p9i/20261123T040252820Z-4ad50a10cb1c455480d732fd5f4e1e0d/)
stopped before roster cycles. A read-only investigation traced each
WindowFromPoint result to a foreign-process HWND; the runner threw before
persisting the raw HWND/PID. The p9g target was outside app-root geometry and
the p9i target was inside. Existing geometry artifacts lack screenshots and
raw HWND/PID, so coordinate/DPI/occlusion cause remains unresolved. The prior
successful p9a action resolved to a same-process popup.

The executor added failure diagnostics that capture raw HWND/PID, visibility,
physical bounds, screen point, and app-root geometry before the existing
fail-closed guards, plus a failure-time screenshot attempt. The guards were
not changed. The [executor logs](../../artifacts/phase9/p9-17-uia-hit-test-diagnostic-checks/)
record its focused checks. An independent Tester ran 55 Python tests, parsed
the PowerShell AST, compiled the C# geometry check, and confirmed current PID,
visibility, containment, stability, and click guards remain. It could not
compare a committed baseline because the runner and tests are untracked. Live
screenshot behavior remains unverified; Tester logs were saved under `%TEMP%`
as identified in its report.

At this handoff, the official VS2022 Build Tools bootstrapper had been
downloaded, signature-checked, and invoked for a side-by-side installation
with v143 and SDK 10.0.26100. Setup was incomplete at that point; this state
was superseded by the successful installation recorded below. No v143 build
was claimed at that earlier handoff.

P9-17 remains open; P9-18 remains queued and the Batch 3 push waits for it.
These UIA attempts yielded no memory-route data. The strict 268,435,456 B
(256 MiB) Working Set target, inclusive 471,859,200 B (450 MiB) ceiling,
separate 536,870,912 B (512 MiB) advisory operation peak, and cold/plateau
cleanup gates remain unchanged.

### 2026-11-23 - Initial v143 installation and pre-repair audit (superseded)

VS2022 Build Tools 17.14.37906.3 is registered at
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`.
The installed v143 toolset is 14.44.35207, x64 `cl.exe` is 19.44.35230, and
Windows SDK 10.0.26100 is installed. Setup logs report successful installation
with no reboot required. The prior launcher's exit 5002 was not the VS Installer
outcome. This confirms toolchain installation only: there is no exact-source
VS2022/v143 receipt or acceptance route yet.

At that earlier checkpoint, the 2,422-file inventory before contract edits
had SHA-256
`fb4391a04bcbf6ac2f18c453ac6eb6a5f764e43a14131d07597d46e4224405fa`. The
initial audit found that the builder, runner, and validator accepted generic
Visual Studio generators and did not bind the compiler toolset in receipts. The
cleanup validator lacked chronology checks between windows and route lifecycle
checkpoints; fail-fast cold-cleanup handling also suppressed the plateau-gate
result. For p9a, the validator explicitly failed cold cleanup and short-
circuited before plateau; independent raw-sample analysis also indicated the
plateau gate failed.

At that checkpoint, the Executor had taken ownership of all three script
consumers and their direct tests and was preparing the receipt-binding and
cleanup changes recorded in the next entry. No exact-source receipt, acceptance
route, or memory acceptance existed then. P9-17 remained open; P9-18 was queued.
The 256 MiB Working Set target, inclusive 450 MiB ceiling, separate 512 MiB
advisory peak, and cold/plateau cleanup gates were unchanged.

### 2026-11-24 - P9-17 receipt contract repair and VS17 generator blocker (superseded)

The builder, runner, and validator now use a v3 receipt contract requiring
Visual Studio 17 2022/x64/v143, compiler 19.44.35230.0, and SDK 10.0.26100.0.
The validator binds the CMake cache, generated projects, compiler hash, and
build log; checks cleanup-window chronology; and reports cold and plateau
outcomes independently. Gate formulas and thresholds did not change. Executor
self-checks passed: validator suite 58, fixture preparation 4, Python
compilation, and PowerShell parsing.

The current source inventory contains 2,422 files with SHA-256
`a6035d39034f2ea45b0d69babe915829b55b4b960ae33d0b95e129a2909cab1e`. VS2022
Build Tools 17.14.37906.3 is registered at
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`, with
v143 14.44.35207, x64 `cl.exe` 19.44.35230, and SDK 10.0.26100. Setup completed
successfully without reboot. The prior launcher session's exit 5002 was not
the VS Installer outcome.

CMake 4.4.2 still cannot select `Visual Studio 17 2022`. `vswhere -requires
Microsoft.VisualStudio.Component.VC.Tools.x86.x64` finds only VS18, indicating
the generic VC tools component is not registered in VS17. The Executor stopped
before build: there is no receipt, EXE, or route. The standard Release folder
is empty; the prior VS18 tree is preserved. A read-only Explorer is identifying
the minimal setup modification. Exact-source v143 build and independent Tester
review are required before another route.

The latest p9a validator explicitly failed cold cleanup and short-circuited
before plateau; independent raw-sample analysis also indicates the plateau
gate failed. P9-17 remains open and P9-18 remains queued. The strict 256 MiB
Working Set target, inclusive 450 MiB ceiling, separate 512 MiB advisory peak,
and cold/plateau cleanup gates remain unchanged. No v143 acceptance is claimed.

### 2026-11-24 - P9-17 accepted v143 build and route launch

The [exact-source Release receipt](../../artifacts/phase9/p9-17-exact-release-builds/20261123T161233365Z-vs17-v143-current-source-release-retry01/release-build-receipt.json)
(schema `classmngr-phase9-p9-17-exact-release-build-v3`) was independently
accepted. It binds 2,422 files, source SHA-256
`78a38f908b8b45a9b52a7ab0cf1f66f30af628eacfbdb05bd99bfc1bc6217077`, and
executable
`build/windows-x64-release/next-package-build/Release/ClassMngr.exe`, SHA-256
`b8278b445dfbd54c6cc7dc743349537c36f5a6a9968258e122bd5219c6302de7`. The
receipt records VS2022 Build Tools 17.14.37906.3 at
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`, generator
`Visual Studio 17 2022`/x64/v143, toolset 14.44.35207, compiler 19.44.35230.0,
and SDK 10.0.26100.0. The incremental Release build succeeded for `ClassMngrNext`,
`ClassMngrNextResourcePackage`, and `ClassMngrPhase7NextBlankFixtureTests` with
zero warnings and errors; it ran in active console Session 1 at Medium
integrity as a non-admin, without elevation. See the sibling
[build log](../../artifacts/phase9/p9-17-exact-release-builds/20261123T161233365Z-vs17-v143-current-source-release-retry01/logs/current-source-release-build.log).

The P9-17 chronology validator passed 63 tests and independent Tester review.
A new local ten-cycle P9-17 route launched against this build; its outcome and
remaining acceptance issues are recorded below. P9-17 remains open.

The prior completed normal route used VS18/v145 and remains failed historical
evidence: maximum settled Working Set was 380,026,880 B, the separate operation
peak was 380,030,976 B, and 380/460 samples missed the strict 268,435,456 B
target. Cold cleanup exceeded its allowance by 23,412,736 B. Its validator
stopped before plateau analysis; raw-sample recomputation put all ten returns
above the plateau upper bound. No root cause is established, and none of these
measurements describes the new route.

The strict 268,435,456 B (256 MiB) Working Set target, inclusive 471,859,200 B
(450 MiB) ceiling, separate 536,870,912 B (512 MiB) operation advisory, and
cold/plateau cleanup gates remain unchanged. P9-18 remains queued after P9-17;
Batch 3 stays active through P9-18, with one push after it closes.

### 2026-11-24 - P9-17 ten-cycle route completed; unaccepted (superseded)

The [route manifest](../../artifacts/phase9/packaged-next-p9-17-large-roster/20261123T162707040Z-d2a0dfa31bf44023ac03b55baa8845f5/manifest.json)
records ten completed cycles, UI/database/readback checks, natural exit code 0,
and no sidecars or temporary files. The run is unaccepted. The validator exits
1 because the final `close-main-window-naturally` action lacks `completedAtUtc`
even though its finalized route record has `closeReturnedAtUtc` and successful
normal process-exit evidence. `route.workflowCompletedAtUtc` precedes that
close action intentionally; top-level `manifest.completedAtUtc` follows it.
Independent review identifies a validator schema gap, not an application
shutdown failure. The route-local six-file harness inventory also omits the
dot-sourced `scripts/phase9/p9_14_packaged_lifecycle_common.ps1`. Preserve this
run as-is; do not repair or rewrite its evidence.

Independent raw calculations show 100/460 samples missed the strict
268,435,456 B (256 MiB) target. Maximum settled Working Set was 309,858,304 B;
operation peak was 310,480,896 B, below the inclusive 471,859,200 B (450 MiB)
ceiling and separate 536,870,912 B (512 MiB) operation advisory. Cold cleanup
failed: the 240,717,824 B allowance was exceeded by the 263,516,160 B return,
an excess of 22,798,336 B. Plateau center was 153,554,944 B, with a
16,236,544 B envelope and 169,791,488 B upper allowance; cycles 2–10 exceeded
it. Fitted return change was +142,282,528.6 B and baseline drift was
−22,908,108.8 B. Both cleanup gates fail independently of the validator gap.

P9-17 remains open. The Executor is correcting canonical completion capture
and finalized JSONL and pinning the shared helper; a fresh exact-source build
and route are required after those changes. The 256/450/512 MiB policy and
cold/plateau cleanup requirements remain unchanged. P9-18 stays queued after
P9-17; Batch 3 remains active through P9-18, with one push after it closes.

### 2026-11-24 - P9-17 corrected ten-cycle route; plateau cleanup failed

The [corrected ten-cycle route](../../artifacts/phase9/packaged-next-p9-17-large-roster/20261123T172727262Z-c5489fce11ad4b8d898dcd935aa4b8b8/manifest.json)
passed source receipt review, route/readback/database checks, natural exit, and
file cleanup. The route is still unaccepted because its plateau cleanup gate
failed; P9-17 remains open. All 460 Working Set samples were below the strict
268,435,456 B (256 MiB) target, with maximum 227,966,976 B. Operation peak was
223,801,344 B, below the separate 536,870,912 B (512 MiB) advisory. Cold
cleanup passed with matched baseline median/range
227,966,976/0 B and first return 142,843,904 B.

Plateau cleanup failed: center 141,725,696 B, envelope 2,039,808 B, upper
bound 143,765,504 B. Cycle 1 returned at 158,384,128 B, exceeding the bound
by 14,618,624 B; cycles 2–10 were below it. Baseline drift was −2,598,502.4 B,
slightly outside the envelope. Diagnostic PrivateUsage increased from about
198.45 MB at cycle-1 return to 348.47 MB at cycle-10 return and 352.76 MB at
final re-entry while Working Set fell. This does not identify a leak owner.

Independent resource review found no per-cycle font/resource loads. Model-to-
snapshot copying and cell payload remain hypotheses; prior full/reduced UIA
routes showed similar growth. Investigate a diagnostic-only filled-versus-
sparse payload comparison while preserving 100x156 geometry, names, row counts,
three probe values, and identical UIA actions. No threshold or gate changes.
The verified VS17 Release tree remains separate from the VS18 Debug CMake
reopen failure. Keep the previous close-timestamp-schema route artifact
unchanged. P9-18 remains queued after P9-17; Batch 3 stays active through
P9-18, with one push after it closes.

### 2026-11-24 - P9-17 deferred; leak attribution moved to Phase 14

The latest valid normal [P9-17 route](../../artifacts/phase9/packaged-next-p9-17-large-roster/20261123T172727262Z-c5489fce11ad4b8d898dcd935aa4b8b8/manifest.json)
remains unaccepted: maximum sampled Working Set was 227,966,976 B, maximum
PrivateUsage was 352,763,904 B, and operation peak was 223,801,344 B. All
Working Set samples were below 268,435,456 B (256 MiB); cold cleanup passed,
but plateau cleanup failed. By user direction P9-17 is Deferred as an active
slice, with its cleanup requirement unresolved and unaccepted. Phase 9 remains
In progress. P9-18 is the active next slice and may proceed while that separate
cleanup gate remains unresolved; Batch 3 still pushes once after P9-18 closes.

The latest valid full-observer [filled-versus-compact-text diagnostic](../../artifacts/p9-17-payload-diagnostic/20261123T213336129Z-9d79b9ba0fe44ea3a7ffcd6df0c2c2db/comparison-manifest.json)
is diagnostic-only/unaccepted; all 10 comparison checks passed. Both arms used
15,000 custom rows, 15,200 target rows, 100 English and 100 Korean names, 768
schedules, identical 100x156 UI geometry, probes, and actions. Sparse used
exactly 14,997 one-character `x` values; text length was the only changed
variable. Return slopes were nearly equal in both arms: about 11.8 MiB/cycle
Working Set and 17.0 MiB/cycle PrivateUsage. Both peaked around 357 MiB sampled
Working Set and 341 MiB PrivateUsage at final re-entry. Text length therefore
has little measured effect; this comparison does not isolate row count or
identify a leak owner. At that point the separate full-versus-3-row harness
had not been independently reviewed and had no live-arm result.

WPR heap snapshot hooks were implemented and statically reviewed, but runtime
tracing did not start. Local `Start-Process -Verb RunAs` failed with
`0xc0000142` before a usable elevated worker; no UAC prompt was observed and no
WPR snapshots or ETL exist. A platform escalation probe still returned a
Medium non-admin token. No allocation stacks were captured. Defer leak
attribution and any confirmed fixes to [Phase 14](15-Phase-14-Windows-Memory-Leak-Attribution.md).
The 256 MiB target, inclusive 450 MiB ceiling, separate 512 MiB operation
advisory, and cold/plateau cleanup contract remain unchanged.

### 2026-11-24 - Comparator review and focused CTest follow-up

The full-versus-3-row comparator/harness contract received independent focused
review; five focused tests passed. No live full or reduced arms were run, so it
has no cardinality result and supports no leak claim. The P9-17 validator still
requires both cleanup gates.

A local current-source P9-17/P9-18 build attempt passed four available focused
CTest cases. `ClassMngrNextPlatformApplicationServicesRosterSavePortTests`
hit C1083 while opening its generated `.moc`. A read-only audit found the
Q_OBJECT declaration, include, CMake registration, and autogen information
correct, and the generated `.moc` exists. The cause was unproven and appeared
possibly transient; no source change was indicated. The planned focused retry
and its outcome are recorded below.

P9-17 remains Deferred and unaccepted because plateau cleanup failed; P9-18
remains Active and Phase 9 remains In progress. The 256 MiB target, inclusive
450 MiB ceiling, separate 512 MiB operation advisory, and both cleanup gates
remain unchanged.

### 2026-11-24 - Isolated exact-source Release build #4 accepted

Isolated exact-source Release build #4 at
`artifacts/phase9/p9-18-exact-release-builds/20261124T034500000Z-RETRY4`
succeeded under VS17/v143. Its abbreviated SHA-256 values are product source
`573d962a…efbc5a`, harness `8ac84ae3…cf3dc5`, and executable
`b8925534…931cab`. The signed redist reports Microsoft Valid. All five focused
P9-17 CTests passed (5/5), including
`ClassMngrNextPlatformApplicationServicesRosterSavePortTests`. The earlier
generated-`.moc` C1083 was transient and resolved on this fresh build with no
source or CMake edit; retain the earlier failed log as historical evidence.

An independent Tester accepted this build evidence and judged the P9-17
code/harness scope safe for a separate commit. This does not pass P9-17's
plateau cleanup gate: P9-17 remains Deferred and unaccepted. P9-18 remains
Active. The 256 MiB target, inclusive 450 MiB ceiling, separate 512 MiB
operation advisory, and both cleanup gates remain unchanged.

### 2026-11-24 - P9-18 exact-source Release build #7 complete

Fresh isolated exact-source Release build #7 at
`artifacts/phase9/p9-18-exact-release-builds/20261124T044000000Z-RETRY7/`
succeeded with VS17 2022 x64/v143. Product source SHA-256 is
`573d962afa6757f6ffb108c267c38efa3119485a10df7241641a132eeeafbc5a`, harness
SHA-256 is `380ec9dc4603fe7d3c2562ea77d52bca253969049474a09209cf0a57350a16ea`,
and executable SHA-256 is
`95250da05aa2c32ecb9129ec8213bd32a53ba794aeabefb84776ba02581ce224`.
Fourteen focused Python tests, both PowerShell parsers, and all five focused
CTest cases passed; no reserved `$PID` was found.

The runner validates the initial lifecycle order
`campus_map/created -> campus_information/suspended -> campus_map/activated`,
then exact lowercase transitions. The packaged P9-18 route has not run. The
Executor is paused before route execution while the user-requested root
`build/` cleanup waits on UAC. There are no P9-18 memory samples or gate result.
P9-17 remains Deferred and unaccepted after plateau cleanup failed; Phase 9
remains In progress. The 256 MiB target, inclusive 450 MiB ceiling, separate
512 MiB operation advisory, and cold/plateau cleanup formulas remain unchanged.

### 2026-11-24 - P9-18 route attempt #1 stopped at Created availability

The packaged [route attempt #1 manifest](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T050220198Z-6dec4e81106b4f888ea01a6370beff2d/manifest.json)
records `campus_map/created -> campus_information/suspended ->
campus_map/activated`, then stops before warm-up memory windows:
`campus_map/created` reported `page_instance_available=false`,
`sample_available=true`; the suspended and activated events were true/true.
PID 8556 exited normally (0); app-data/settings were unchanged, with no temp
files or SQLite sidecars. The cold baseline had 10 samples and maximum Working
Set 203,182,080 B. This is not a P9-18 memory route result or acceptance.

An independent Tester confirmed PageHost emits Created before storing the
instance. The state-aware runner/validator/test repair passed focused checks;
review then found a missing `page_instance_available` may be coerced to false.
Require explicit presence/type validation and regression coverage before retry.
Preserve successful exact-source Release build #8 [receipt](../../artifacts/phase9/p9-18-exact-release-builds/20261124T051100000Z-RETRY8/release-build-receipt.json)
as pre-strict-schema evidence: source SHA-256
`573d962afa6757f6ffb108c267c38efa3119485a10df7241641a132eeeafbc5a`, harness
SHA-256 `54778501a440e4ad11ad8f8148c1e090d9586177d092a076ce091e067aa3660b`.
Build #8 is not route-eligible; rerun the strict-schema build as RETRY9, then
route only from its fresh receipt. Retain root `build/`: runner/validator
receipts require `build/windows-x64-release/CMakeCache.txt` and the preserved
Release directory.

P9-17 remains Deferred and unaccepted after plateau cleanup failed; Phase 9
remains In progress.

### 2026-11-24 - P9-18 route attempts #2/#3; #4 unaccepted

Attempt #2 (`20261124T060100186Z-83f2ce3af0a3496089d63d285ce50f83`)
[manifest](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T060100186Z-83f2ce3af0a3496089d63d285ce50f83/manifest.json)
and [validation report](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T060100186Z-83f2ce3af0a3496089d63d285ce50f83/validation.json)
show lifecycle and map loads succeeded, but the runner stopped before warm-up:
it compared fixture catalog `resource_file` with runtime `package_file`.
PID 32484 exited normally; app-data stayed unchanged, with no temp files or
SQLite sidecars. This produced no P9-18 route-memory result.

An independent raw-artifact audit of attempt #3
(`20261124T063058681Z-eb8b8afa557f4f47ac68e7803afa1027`) used its
[manifest](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T063058681Z-eb8b8afa557f4f47ac68e7803afa1027/manifest.json)
and [process metrics](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T063058681Z-eb8b8afa557f4f47ac68e7803afa1027/process-metrics.jsonl)
to inspect 28 windows/280 samples. It found maximum Working Set was
226,410,496 B, no settled sample was >= 268,435,456 B, the 450 MiB ceiling and
512 MiB operation advisory passed, and operation peak was 236,736,512 B. Cold
cleanup failed: baseline 203,038,720 B, return 214,466,560 B, uplift
11,427,840 B. Plateau cleanup failed cycles 1, 2, 4, and 5 over 214,482,944 B
by 909,312 / 1,339,392 / 1,970,176 / 2,557,952 B. Stable baseline and fitted
cycle change do not erase these failures. UIA/resource/file/database/process
checks passed; PID 25728 exited normally, with no app-data/temp/sidecar changes.
The validator crashed before writing `validation.json`. Its [rerun receipt](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T063058681Z-eb8b8afa557f4f47ac68e7803afa1027/logs/validator-rerun.json)
and [log](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T063058681Z-eb8b8afa557f4f47ac68e7803afa1027/logs/validator-rerun-output.txt)
show an `AttributeError`: PowerShell serialized singleton `lifecycleEvents`
in cold/plateau confirmations as an object, then the validator iterated its
dictionary keys as events. Keep attempt #3 immutable and unaccepted.

Tester accepted the successful [RETRY12 receipt](../../artifacts/phase9/p9-18-exact-release-builds/20261124T063700000Z-RETRY12/release-build-receipt.json)
and its harness and five focused tests. For attempt #4
(`20261124T065629417Z-bad41c4e931d484b9eef363a0d1639a6`), the [manifest](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T065629417Z-bad41c4e931d484b9eef363a0d1639a6/manifest.json)
and top-level [validation report](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T065629417Z-bad41c4e931d484b9eef363a0d1639a6/validation.json)
incorrectly say `passed`, but
`settledMemoryAndCleanupGates.details.passed=false`: cold
cleanup failed by 11,087,872 B and plateau cleanup failed cycles 1, 2, and 4;
the fitted cycle trend is within the baseline envelope. This status-propagation
contradiction is under independent audit/repair. Preserve attempt #4 immutably;
make no acceptance claim. P9-18 remains Active and the memory gates are unchanged.

### 2026-11-24 - P9-18 RETRY13 route attempt #5: cleanup failed

The [attempt #5 manifest](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T072603004Z-6e550a50e1d24deaa3abae409d1e3bfe/manifest.json),
[validation report](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T072603004Z-6e550a50e1d24deaa3abae409d1e3bfe/validation.json),
[process metrics](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T072603004Z-6e550a50e1d24deaa3abae409d1e3bfe/process-metrics.jsonl),
and [RETRY13 receipt](../../artifacts/phase9/p9-18-exact-release-builds/20261124T161000000Z-RETRY13/release-build-receipt.json)
record run `20261124T072603004Z-6e550a50e1d24deaa3abae409d1e3bfe`. The
independent raw audit was accepted by the Tester; this is not P9-18 acceptance.
With the earlier validator wrapper issue fixed in RETRY13, validation correctly
reports failure: four gates passed and `settledMemoryAndCleanupGates` failed.

The route captured 28 windows/280 samples across 10 cycles. Maximum Working
Set was 226,082,816 B, with zero strict-target misses and zero 450 MiB
exceedances; operation peak was 235,950,080 B. Cold cleanup failed: baseline
203,005,952 B, range 69,632 B, allowance 203,075,584 B, return 214,204,416 B,
uplift 11,198,464 B (11,128,832 B over allowance). Plateau cleanup failed:
center 214,712,320 B, envelope 344,064 B, fitted drift +660,275.2 B outside the
envelope, upper bound 215,056,384 B; cycles 1-4 returned 215,343,104 / 215,584,768 /
216,150,016 / 216,174,592 B, all above the bound.

Raw UIA/resource/image/screenshot, app-data, natural-exit, temp, and sidecar
checks passed; PID 22668 exited 0 without forced termination. Private-usage
diagnostics: settled `privateBytes` maximum 155,717,632 B at cycle 10 open;
continuous diagnostic peak 166,854,656 B at cycle 3 activation; lifecycle
private/pagefile peak 165,548,032 B during cycle 10 activation; cold private
baseline 131,182,592 B and warm-up return 142,290,944 B. These establish
neither a leak nor its owner. P9-18 remains Active and unaccepted because both
cleanup gates fail; the memory thresholds and formulas are unchanged.

### 2026-11-24 - P9-18 retained-versus-released Maps diagnostic

The independent Tester accepted this diagnostic-only [receipt](../../artifacts/phase9/p9-18-retained-vs-released-diagnostic/20261124T085644091Z-b41ae767f09f4a2daecb45b15a54db59/diagnostic-receipt.json)
(SHA-256 `71d8cf7690fa3982c3cf5ce5654dbb23a398fa31a4f526892128d668d6a61f61`),
using `tests/next/campus_directory_visual_capture.cpp` at SHA-256
`60c73c3eac4ebfa33c37838f4523f96673006816c22cf8b0112b8f61836abace`. The
[first](../../artifacts/phase9/p9-18-retained-vs-released-diagnostic/20261124T085644091Z-b41ae767f09f4a2daecb45b15a54db59/visual-captures/campus_directory_paired_capture.json)
and [second](../../artifacts/phase9/p9-18-retained-vs-released-diagnostic/20261124T085644091Z-b41ae767f09f4a2daecb45b15a54db59/visual-captures-attempt2/campus_directory_paired_capture.json)
Release run manifests record 10/10 samples before and after release and 52
PNGs each. The same visible `campus_information` page stayed at 74 widgets;
the retained Maps instance had 32 widgets and zero image references, and was
absent after release.

Run 1 retained-to-released medians: Working Set 82,124,800 -> 82,145,280 B
(+20,480); PrivateUsage 52,273,152 -> 52,285,440 B (+12,288). Run 2:
Working Set 81,281,024 -> 81,297,408 B (+16,384); PrivateUsage 52,518,912 ->
53,149,696 B (+630,784). Releasing Maps produced no meaningful process-memory
decrease; this diagnostic does not justify a production lifecycle change or
identify a leak. P9-18 remains Active and unaccepted after cleanup failed in
[route attempt #5](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T072603004Z-6e550a50e1d24deaa3abae409d1e3bfe/manifest.json);
its thresholds and gates are unchanged.

#### 2026-11-24 - Strict-precondition rerun

The independent Tester accepted this diagnostic-only [receipt](../../artifacts/phase9/p9-18-retained-vs-released-diagnostic/20261124T092403077Z-e9f12008e8ff454998371b14083905f1/diagnostic-receipt.json)
(SHA-256 `9fb71d70e3830792c63b2fd9c7a0cf483a248d8c2999d8f484664a5bf535c0aa`),
using `tests/next/campus_directory_visual_capture.cpp` at SHA-256
`401bd200bf91e33dfeef62dbe6a6aedff2d0ec42bd85895d4ecf904534ac9391`. The
[first](../../artifacts/phase9/p9-18-retained-vs-released-diagnostic/20261124T092403077Z-e9f12008e8ff454998371b14083905f1/visual-captures/campus_directory_paired_capture.json)
and [second](../../artifacts/phase9/p9-18-retained-vs-released-diagnostic/20261124T092403077Z-e9f12008e8ff454998371b14083905f1/visual-captures-attempt2/campus_directory_paired_capture.json)
Release runs enforced the precondition before sampling: Maps suspended and
retained with 32 widgets and zero loaded resources, decoded dimensions,
image-label widgets, or pixmaps. After release it was absent while the same
visible 74-widget Information page remained. The Release build and both runs
exited 0, saved 52 PNGs each, and had 10/10 available samples in both states.

Retained-to-released medians in run 1: Working Set 82,223,104 -> 82,247,680 B
(+24,576); PrivateUsage 52,371,456 -> 52,383,744 B (+12,288). Run 2:
Working Set 81,076,224 -> 80,957,440 B (-118,784); PrivateUsage 53,202,944 ->
52,736,000 B (-466,944). The mixed deltas show no consistent process-memory
benefit. This diagnostic is not P9-18 acceptance and supports neither a
production lifecycle change nor leak attribution. The earlier diagnostic is
unchanged; P9-18 remains Active and unaccepted after [route attempt #5](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T072603004Z-6e550a50e1d24deaa3abae409d1e3bfe/manifest.json)
cleanup failure. Memory gates are unchanged.

### 2026-11-24 - P9-18 call-phase attempt #6 and targeted-UIA attempt #7

Exact-source full-observer [attempt #6](../../artifacts/phase9/p9-18-call-phase-diagnostic-current-source/20261124T104545279Z-c4023c8a7fc543328b19e81db00a7914/manifest.json)
joined and recomputed 85 call-phase records against process, window, and
evidence records. The no-action UIA calls had positive Working Set deltas in
5/5 calls (median 151,552 B) and zero screenshot delta in 5/5; deltas across
all calls were mixed. All gates except cold and plateau cleanup passed. Cold
uplift was 11,075,584 B; plateau drift was +778,240 B against a 405,504 B
envelope, with return exceedances in cycles 1–5, 7, and 8. No owner was
identified. See the [call-phase receipt](../../artifacts/phase9/p9-18-call-phase-diagnostic-current-source/20261124T104545279Z-c4023c8a7fc543328b19e81db00a7914/call-phase-diagnostic-receipt.json)
and its [validation](../../artifacts/phase9/p9-18-call-phase-diagnostic-current-source/20261124T104545279Z-c4023c8a7fc543328b19e81db00a7914/validation.json).

Independently reviewed exact-source targeted-UIA [attempt #7](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T122412350Z-d59e56771be74f66ad00b21cbaad38e5/manifest.json)
used source SHA-256 `dfd5750e269ac2cd3c66020e2a0af0e4b424878774ecb065e46e2bfd4edd407a`,
harness SHA-256 `1de37275288317eb3f85cb43ea80f524e730fc06dbfa929ce6055b94426ff809`, and packaged executable SHA-256
`5b8f4fdfb12cfc01311f22b53838984ce6c95505bc5752733a6a679d75e83ccd` (VS2022
x64/v143; [build receipt](../../artifacts/phase9/p9-18-exact-release-builds/20261124T120937000Z-TARGETED3/release-build-receipt.json)).
Its 28 UIA captures showed two visible cards/images in 11 Maps captures and
none in 17 Information captures; 85 trace records were retained. Four gates
passed and only cold cleanup failed: baseline median/max 203,001,856 B, first
return median 213,811,200 B (uplift 10,809,344 B). Plateau passed with center
213,815,296 B, 2,891,776 B envelope, 216,707,072 B upper bound, and no return
exceedances. See the [route validation](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T122412350Z-d59e56771be74f66ad00b21cbaad38e5/validation.json)
and [call-phase receipt](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T122412350Z-d59e56771be74f66ad00b21cbaad38e5/call-phase-diagnostic-receipt.json).

Read-only lifecycle review places the first-use increase during Map activation
and image setup: after both Bundang images loaded (9,867,100 B decoded payload),
the activated checkpoint was 223,444,992 B Working Set / 151,404,544 B
PrivateUsage, versus 203,001,856 / 130,965,504 B at baseline. After suspension
and clear it was 213,778,432 / 141,701,120 B; later cycles add no further
~10 MB. This supports first-use retention but identifies no owner. Attempts #6
and #7 used different executable hashes, so their difference is suggestive,
not a controlled comparison. The Executor reported 30 Python tests passed, but
no retained Python log was available for independent verification; CTest 5/5
was independently verified; the PowerShell parse log is absent although the
route ran. At the time of attempt #7, P9-18 remained Active and unaccepted
because cold cleanup failed; its gates and thresholds were unchanged.

### 2026-11-24 - User deferral decision and Phase 9 continuation

By explicit user direction, preserve the current Maps page retention and defer
the P9-18 cold-cleanup gate to Phase 14. The failed P9-17 plateau-cleanup gate
is also Deferred to Phase 14. Both remain unaccepted failures, not passing
results. Phase 9 remains In progress and may close only after Batch 4's
remaining slices and the final Windows x64 Debug CTest and packaged Release
gates pass, with exactly these two cleanup gates explicitly deferred and
unaccepted. P9-19 is next; P9-20 remains conditional on a suitable fixture and
P9-21 remains the follow-up candidate. Phase 14 remains Not started until
Phase 13 and must use fresh exact-source Release evidence to revalidate both
gates under their unchanged contracts. No memory threshold or formula changed.
See the [P9-18 attempt #6 manifest](../../artifacts/phase9/p9-18-call-phase-diagnostic-current-source/20261124T104545279Z-c4023c8a7fc543328b19e81db00a7914/manifest.json),
[attempt #7 manifest](../../artifacts/phase9/packaged-next-p9-18-campus-map/20261124T122412350Z-d59e56771be74f66ad00b21cbaad38e5/manifest.json),
and [Phase 14 plan](15-Phase-14-Windows-Memory-Leak-Attribution.md).

### 2026-11-24 - P9-18 MAPSTAGE diagnostic stopped before route

At the user's direction to defer P9-18 cold-cleanup attribution to Phase 14
and preserve current PageHost retention, the in-progress stage probe was
stopped. Its source, test, and parser changes were removed; the app and Campus
Directory test source have no diff against HEAD. No diagnostic route or stage
JSONL was produced.

The source-bound VS2022 x64/v143 Release build on the instrumented tree
succeeded, and its five prescribed focused CTests passed. The extra
`ClassMngrNextCampusDirectoryTests` target built, but its CTest failed after
19.22 seconds without output; direct verbose execution exited 1 without
output. The CTest log SHA-256 is
`73CD1860CD063EB61F9F7A7CF4D1B8F4EFCF94FED31FD95F4744CECF7B40EEAD`.
Pre-stop Python/parse checks passed on the instrumented tree but were not
rerun after rollback. The [MAPSTAGE build receipt](../../artifacts/phase9/p9-18-exact-release-builds/20261124T133334000Z-MAPSTAGE/release-build-receipt.json)
has SHA-256 `2D4825A809D266B1356EAD0DFDCE895E16945B6B9BC15D05A7974B6D456CA1EB`
and source fingerprint `44ca245fbc85de8e955025b7dd099f15b405d3750d2a4d1e129a1ccf8fef5812`;
it is stale after rollback. This attempt produced no heap or image-stage
attribution and changes no acceptance status. After Phase 13, Phase 14 needs a
fresh diagnostic implementation and build on current source before route
evidence can be collected.

### 2026-11-25 - P9-19 implementation/build verified; required base missing

The P9-19 [Classes Navigator page](../../src/next/ui/classes_navigator_page.cpp),
[visual checkpoint](../../tests/next/classes_navigator_visual_capture.cpp),
[fixture preparer](../../scripts/phase9/prepare_p9_19_large_startup_fixture.py),
[runner](../../scripts/phase9/run_packaged_next_p9_19_classes_lifecycle.ps1),
[validator](../../scripts/phase9/validate_packaged_next_p9_19_classes_lifecycle.py),
and [Release builder](../../scripts/phase9/p9_19_build_current_source_release.ps1)
are present. The harness keeps its fixed limits and formulas, cold baseline before
warm-up, five no-action baselines, ten cycles, PID/HWND-bound route actions, and
required Classes checkpoint. The independent Tester confirmed 11 Python tests,
Python syntax, and PowerShell parsing passed in the [local validation
artifacts](../../artifacts/phase9/p9-19-local-validation/20261125T150000Z-a650bb58ce44499b9fc060d2e651c0aa/);
its Python test log SHA-256 is
38148501e590a0780ef664022516807391dc782035a424211ce014e50662415d.

The independently verified [exact-source Release receipt](../../artifacts/phase9/p9-19-exact-release-builds/20261125T143000Z-c4bba9118a744672987d805fe88e79b1/release-build-receipt.json)
records VS2022/v143 x64 Release, source SHA-256
b97dd838b3831f0cb65802391f5cd15d6cf7ee57c5c7edc27bd0e4b410bf3b82, and
executable SHA-256
633e4f05c8cfca3d2e60b1ddb34a6861d8fab69388f1f5769a1ad4461fccf9e1.
The source inventory matched 2,377 files; 101 CRLF/LF checkout differences
were normalized only where LF bytes matched the pins. The receipt's focused
CTest log records Bootstrap, PageHost, ClassesNavigatorPage, and Classes visual
capture passing. The Tester verified that receipt, but its direct C++ rerun did
not complete, so no independent direct-rerun success is claimed.

The required master-blank.tps is absent from the configured P9-16 artifact
path and inspected C:/D: locations; its pinned SHA-256 remains
85c23437621fe67361b38157bb53acdb19e78573ebaa0297fad769b71ad0c9e9. It was
not substituted or rebaselined. The canonical Git fixture SQL pin remains
1b2f09be03cce809e239112b3f7b4f2af405bad83a420db0c6ffe5ef0eb597e3; the D:
working copy is CRLF under core.autocrlf, and the preparer verifies canonical-LF
bytes. No P9-19 route root exists and the route was not run. P9-19 remains
Active and unaccepted pending user direction to restore the exact base artifact
or authorize a new reproducible base hash; implementation/build verification is
not slice acceptance.

The P9-17 plateau and P9-18 cold-cleanup failures remain Deferred and unaccepted
for Phase 14; Maps retention, Phase 9 Batch 4, and final gates are unchanged.
See the [Phase 14 plan](15-Phase-14-Windows-Memory-Leak-Attribution.md).
