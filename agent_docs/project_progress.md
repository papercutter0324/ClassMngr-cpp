# Project Progress

Active deployment plan: Qt Rewrite Phase 7 — Feature Migration.
Current deployment: `phase7_resume_20261029`. Route: Heavy.
Phases 0–6 are Complete; Phase 7 is in progress. F1514 added descriptor-based
lazy registration, F1515 added explicit page lifecycle states, and F1516 added
guarded placeholder navigation with history. F1517 added the typed CommandBus;
F1518 made history action state respect target visibility. F1519 added typed
semantic prompt contracts and a value-only Qt automation boundary. F1520 added
the typed file-dialog port and Qt presenter. F1521 added a transient
shell-owned notification service. F1522 added explicit read/apply/persist/
notify operations around the validated preferences state. F1523 added the
theme presenter using existing QSS and preference storage. F1524 added font
size to the shared preferences state, a complete snapshot adapter, startup
language/theme/font selection, and page access to the same model. F1525 added a
shared editable text-row model adopted by Staff Directory. F1526 accepted
Staff Directory as the representative model/view gate. F1527 captured and
verified English/Korean shell appearance under Light/Dark themes and was
pushed with Batch 1 at `a3ff4d1c`. F1528 completed the bounded review-window
model and Phase 6 exit gate; Batch 3 discovery found no other Phase 6 slices.
Phase 5 platform evidence and broad-suite limitations remain in the
[Phase 5 plan](../plans/qt-rewrite-heavy-route-plan/06-Phase-5-Startup-and-Bootstrap-Rewrite.md)
and [Start Here](../plans/qt-rewrite-heavy-route-plan/00-Start-Here.md).

## Goal

Complete Phase 7 feature migration, preserving supported workflows and
workspace compatibility, verifying the required behavior and exit gates,
committing each accepted slice, and pushing each completed batch.

## Overall Progress

Phases 0–6 are Complete. Phase 7 started on the Heavy route on 2026-10-26.
Slice 7A.1, the user-visible v2 profile-bootstrap stage, passed independent
focused macOS Debug verification and was committed as `d49f34fe`. Slice 7A.2,
personal details and signature, passed functional persistence verification and
was committed as `1f9b30bf`. Slice 7A.3, setup teacher entry, passed an
independent recheck and was committed as `01aaf3d7`. It adds manual teacher
creation through the typed teacher-create use case and a dedicated
WorkspaceStore adapter, keeps setup pending with the recovery journal and
original-profile backup intact, and does not add a pending profile to recent
history. Optional-only input is not silently skipped when another teacher is
already available. Slice 7A.4, setup teacher import, passed independent focused
verification and was committed as `8bee6dbb`. Its Next-owned asynchronous
workflow maps review choices directly into the typed teacher-import use case
and applies through the pending `WorkspaceStore` transaction. The unchanged
fixture verifies Korean group selection, automatic NET/GS inclusion, source
date persistence, rollback and retry, pending setup state, no recent-history
write, retained backup/journal, and cancellation recovery. Candidate #5,
setup class details, passed independent focused verification and was committed
as `932fdfb2`. It captures a typed in-memory draft after teacher entry without
creating class, class-details, or schedule rows. Required fields preserve the
legacy four-field rule; books remain optional. A real `.tps` integration
imports Korean teachers, selects one for the draft, then cancels and verifies
original-profile restoration. Candidate #6, setup class times, passed
independent focused verification and was committed as `97d45d3d`. It consumes the typed draft,
creates the blank class, then transactionally saves class details and ordered
regular/intensive schedules through direct WorkspaceStore adapters. Failure
and retry preserve the legacy separate create/save states. Candidate #7, setup
schedule import, passed independent focused verification and was committed as
`a83b7280`. Its optional Next-owned source/review flow uses typed decisions and
direct WorkspaceStore snapshot/apply adapters. The review exposes destructive
clear counts; import leaves setup pending, and only explicit Finish or Skip and
Finish writes recents. Source cancel restores the original profile; review
cancel returns to source selection. Candidate #8, normal `.tps` creation and
replacement, passed independent focused verification and was committed as
`95c1243a`. It uses the typed replacement lifecycle and direct WorkspaceStore
adapter to create empty schema-v6 profiles, safely replace existing targets,
and update recent history only after success. Rare recovery-name collision,
failed original restoration and failed prior-workspace reopen branches remain
untested. Candidate #9, interactive existing `.tps` open, passed independent
focused macOS Debug verification and was committed as `cdbf3ead`. The File
action opens through the Next coordinator; cancel, dirty rejection,
setup/recovery gating, and failed opens preserve the active workspace, while
success refreshes navigation and records recent history once. The fresh
standard build completed; three focused CTests and one dependency-boundary
CTest passed. OS permission-denied behavior was not tested. Candidate #10,
save, close, and dirty state, passed independent verification and was committed
as `283871fa`. File Save/Close use the Next coordinator; one shared typed flow
resolves page drafts and WorkspaceState dirtiness with Save/Discard/Cancel for
navigation, Open/New, profile close, and shutdown. Discard rolls back pending
store work. Pending setup blocks profile close without changing its journal.
The focused standard-tree CTest set passed 7/7, including the workspace
dependency boundary; configure validated 1,739 source owners. No active-profile
editor currently produces dirty state, so 7B editors must use the new Page hook.
Candidate #11, Save As, passed independent focused verification and was
committed as `86b09d94`. It uses SQLite online backup and atomic file
replacement, then switches the active store only after destination reopen;
the current session ID is preserved and history updates only after success.
The file-operation and online-backup sources moved into the shared
`ClassMngrWorkspaceStore` target, keeping one source owner and no Next-to-legacy
Data link. Candidate #12, Export, is active; Batch 1 remains un-pushed.
Verification used the standard macOS tree, validated 1,741 source owners, and
passed seven focused CTests including the Next boundary and legacy DataService
lifecycle. No production Qt dialog was displayed, no forced destination-reopen
failure was injected, and no Save As dirty-page/recovery fixture ran. The 7A
screenshot comparison, Phase 7 memory evidence, and all remaining 7A workflows
remain open. Active candidates are tracked in
`plans/qt-rewrite-heavy-route-plan/Current-Slice-Batches.md`.

Candidate #12, Export, passed independent focused verification and was
committed as `5badb2c8`. The Next File action exports a normalized `.tps`
snapshot through the shared online-backup and atomic-copy helper. It preserves
the source session/path, selection, dirty state, and recent-profile history;
setup/recovery state blocks before the dialog. Focused CTest passed 4/4,
including Export flow, bootstrap, preferences, and the Next dependency
boundary. No full suite or native dialog run occurred; invalid-path and
dialog-open-failure cases were not injected.

Candidate #13, Recent Files menu, restore, and clear, passed independent
focused verification and was committed as `b79b98e9`; its remembered-directory
parity fix and combined failure test are in `4fcab8bf`. The Next submenu reads
history when opened; restore uses the existing open flow without a file
picker, remembers the opened profile's directory, and clear removes recent
and last-file settings without deleting profiles, changing the active
workspace, or clearing the directory. Setup/recovery blocks before dirty
resolution. Only confirmed missing paths are pruned; corrupt and unsupported
profiles remain listed. Focused CTest passed 5/5 including the Next boundary;
after the follow-ups, all five acceptance CTest entries passed again, the
selected offscreen Qt cases passed, and Bootstrap CTest passed 1/1.
`git diff --check` passed. No full-suite or native-dialog run occurred; the
directory-preference write-failure warning was not injected. Candidate #14,
normal-create interrupted recovery, passed independent focused verification
and was committed as `0b5c1d6c`. It adds an explicit Next File-menu action that
scans one directory for matching hidden recovery siblings, copies a selected
source to a new non-overwriting `.tps` destination, retains the source, and
opens the copy through the shared open flow. Initial Setup backups remain
separate, and no normal-create journal or startup prompt was added. The
focused build covered Next, Bootstrap, Navigation, and recovery/open/create
flow targets; focused CTest passed 6/6 including the dependency boundary. The
follow-up added three-candidate ordering and a real menu-to-bootstrap test;
these plus the boundary passed 3/3. No full-suite or manual-dialog run
occurred, and recovery used seeded sibling artifacts rather than a process
terminated mid-create. Bootstrap open-failure integration starts with no
active session; active-session preservation is verified separately in the
recovery-flow test. Next-batch discovery completed with five ordered
Teacher/Staff proposals. Candidate #15, schema-v6 `.db` input and native `.tps`
normalization, passed independent focused verification and was committed as
`c9cdd421`. The Next open path accepts case-insensitive `.db` inputs and keeps
the selected database active at its original `.db` path; schema validation
still requires v6. Create, Save As, and Export continue to normalize outputs to
`.tps`. Focused CTest passed 5/5, including interactive open, bootstrap/startup,
Save As, Export, and the Next dependency boundary; `git diff --check` passed.
No full suite or native dialog run occurred. The v6 fixture is test-created,
and the v5 rejection fixture exercises the version gate without a populated
historical v5 schema; no real-world legacy `.db` fixture was available. Batch 1
is complete and pushed to `origin/Qt-Rewrite` at `c5af6755`; local and remote
tips match. Batch 2 starts with candidate #16, Teacher profile page and
navigation, from the five discovered Teacher/Staff proposals. The 7A
screenshot comparison, Phase 7 memory evidence, and 7A/Phase 7 exit gates
remain open.

Phase 0 completed on 2026-09-18. The combined route gate passed all 24 required
routes on Windows x64 and macOS universal, and the user confirmed the retained
visual references. The Phase 0 plan update in commit f8bb5954 records this
closure; earlier Phase 0 open notes are superseded.

Phase 1 slice 1.1 adds a Qt Core-only ClassMngrNext console bootstrap and a
focused CTest launch probe. It has no window and does not link the legacy
runtime.

Slice 1.2 establishes explicit next-generation layer and feature interface
targets in `cmake/next.cmake`. Configure-time assertions enforce the planned
dependency edges. The targets are intentionally source-free at this stage;
`ClassMngrNext` remains independently linked only to Qt Core.

Slice 1.3 measured Qt/Zlib header dependencies per legacy production object
target and moved those modules out of the shared build-settings target. The
legacy runtime keeps the complete module union required to link the current
application and tests.

Slice 1.4 replaces recursive production source discovery with explicit lists
for all six legacy object targets, the two executable entry points, and the
calendar QML files. Included roster `.inc` fragments are explicit header-only
inputs. A configure-time ownership check compares the handwritten `src` and
active test inventories with their targets; shared schedule test doubles now
have one object-library owner each.

Slice 1.5 adds scoped formatting/static-analysis CI for `src/next`, compile
database validation, a configure-time Qt Core-only link assertion for
`ClassMngrNext`, generated Qt-module and resource-pack reports, resource
reference checks, staged-package build reports in the Release workflows, and
startup/memory/performance CTest labels. A clean Windows Ninja/MSVC Debug
configure and full build passed (351 steps); `ClassMngrNextLaunch` passed
(1/1). The module report lists only `Qt6::Core` for `ClassMngrNext`; the
resource check passed for six generated RCC packs and seven runtime IDs, and
the staged-package report probe passed. Cross-platform CI and local
`clang-format`/`clang-tidy` were not run.

## Current Position

### Current checkpoint — 2026-10-29 (candidate #41 active; Batch 4 active)

Candidate #39, AI-assisted Speaking Evaluation comments and batch workflow,
is accepted in source commit `02fbeca6`. Class analytics remain in candidate
#27. The page uses typed prompt/eligibility policies, a 25-student batch cap,
configured external HTTPS website preferences, and local name restoration.
Review/apply flows through the evaluation matrix's existing undo, dirty-state,
and save path. Independent verification passed 9 focused CTests and the direct
mouse/keyboard and WorkspaceStore save/reopen cases. The 16 synthetic captures
are packaged as presentation evidence; no pixel-parity claim is made. Resource
inventory, one-owner source validation (1,911), and diff checks passed.

Candidate #40, reports/PDF/PowerPoint/export jobs, is accepted in source commit
`f2b24bd7`. Independent verification passed four focused CTests, the Next
Save-As/Print interaction checks, report lifecycle/output checks, the narrow
Next dependency boundary, resource inventory validation, and the C40 capture
integrity review. Its ten synthetic captures and one-page PDF are packaged in
the C40 visual-baseline README; they show presentation and output, not
pixel-parity. Batch 3 and its Windows evidence support were pushed at
`2cfb1db386850987b0680e9b3f92704a8cb155e6`. Batch 4 is active with candidates
#41–48. Candidate #41's Campus Dashboard boundary is accepted for
implementation: packaged read-only data, writable app-data overrides with
import from the known historical directory, and a campus-local `--enable-admin`
capability. Campus Staff remains owned by #17–19; app-wide permission parity
remains #46. Production implementation is underway; independent verification
remains pending. The generated schema-v6 `.db`
relationship/output check passed independently
using synthetic fixture data, not a historic profile, with PDF parity limited
to one landscape template. Exit evidence still open includes broader 7A visual
and memory/lifecycle evidence and hosted Windows x64 Release Next startup
collection. The 96-class Sub Prep entry/refresh/output/release route is
required in #44. Local memory-evidence support passed six Python validator
tests, blank schema-v6 fixture CTest 1/1, and the Next startup dependency
boundary CTest 1/1. The fixture link has no broad Runtime/Calendar closure and
source ownership validated 1,921 handwritten files. Windows Release run
`40914914018` completed at the pushed SHA, but the x64 diagnostic prerequisite
build failed on missing `WorkspaceStoreGateway` schedule import members; no
collector files or Phase 7 memory evidence were produced. The `<250 MiB`
threshold remains a separate Phase 9 gate. The 7A and Phase 7 exit gates are
not yet passed.

The following checkpoint records are historical; canonical current state is
in the Phase 7 plan, progress log, and slice-batch tracker.

### Historical checkpoint - 2026-10-20 (Batch488 pushed; F1169 accepted/in progress; F629 open)

Batch487 (F1144–F1153) is closed and pushed to `origin/Qt-Rewrite` at
`c3c06e2f5432933a0e8b7df629e25b2afa920e2c`. Batch488 is active: F1154–F1158
are complete; F1158's source/test commit is
`ca33b146fa9b33fb4ad0be5f2e9beced7663606d`, after acceptance commit
`b61529e38323526f254949e0c5bdc8d48a178d03`. F1159 is complete at
`813f506a5a489bcfedc2200dfdc38b7481cfa564`, following its acceptance commit
`890223b546586c4666f21bac571e95a52035bca5`. F1160 is complete at
`14d2c957cbcd8ebbf21a652408fada85ea13e924`; F1161 completes Batch488 at
source/test commit `81a1fc256fa7e7d32b589fe2bef193e1a213864e`, after acceptance
commit `8162b27b22c5ace992ab129b149849572dbeac73`. F1162 completes the first
Batch489 candidate at source/test commit `75614008eb26cf33e3c602b1cdbc99d4a497a11b`,
after acceptance commit `ddf8e74c68d5ee3f4566d0e8399d4e0ca5166639`. The
incremental macOS Debug build covered `ClassMngr` and the Application,
Platform, and Calendar Page parity targets; all three focused CTests passed,
and `git diff --check` passed. No full suite or Windows build ran. Batch488 is
closed and pushed at `138c2cf86f3bac153d948649980798fbfd066dd6`; the Phase 2
progress log has the push receipt. Batch489 is active with F1162–F1168 complete,
F1169 accepted/in progress, and F1170 discovered. Batch490 is discovered and
inactive until Batch489 closes; its ordered candidates are in the Phase 2
progress log.
F1163's acceptance commit is `c7660c884420ec2a8cdcb674b8788fc69982d336`, and
its source/test commit is `030e9752ec581b8013c27eef37a4ef5e36af6531`. It
types the shared Calendar save request while preserving raw input at the
Application boundary across single-save, import, and repeat creation. The
incremental macOS Debug build ran 40 Ninja steps; independent focused CTests
passed 4/4, and the Domain contract CTest passed 1/1 after its target was
rebuilt. `git diff --check` passed; no full suite or Windows build ran.
F1164's acceptance commit is `840919adff1b3e4f7730256b26c79f92b6339869`, and
its source/test commit is `bb4fc2a7fb9c0cadcb309f135187fc101d268c7a`. The
persisted Calendar series-edit snapshot now carries an optional typed date;
malformed persisted values retain the existing Technical failure. The
incremental macOS Debug build covered `ClassMngr` and both Calendar event test
targets; focused CTests passed 2/2 (78 Application and 73 Platform tests),
and `git diff --check` passed. No full suite or Windows build ran. There is no
dedicated integration fixture for a valid `QDate` year outside 0001–9999;
`GregorianDate::fromComponents` rejects those years, and the corrupt stored-
date path is verified end to end before persistence. F1165 completed at
source/test commit `ecf50166d8c0b510879c5a20df3d89a42a7ac2b0`, following
acceptance commit `acdec03d5abcb6d946f8e70411686c550d69e025`. Schedule Import
projected time state now carries an optional `Domain::ScheduleTime`; unknown
`-1` values, default state, and other invalid tuples still produce the labeled
`InvalidProjectedTime` result before overlap checks. The three focused
macOS Debug targets built, and independent focused CTests passed 3/3. Tests
cover typed values, invalid components, labels and precedence, plus Apply and
review diagnostics. `git diff --check` passed. No full suite or Windows build
ran. F629 still awaits source-bound Windows x64 layout evidence; the user
cannot run Windows now. F1166 completed at source/test commit
`f83b1b286edaef98dd437666f50f4ddd36641b5d`, following acceptance commit
`3df496be31fd916be780112bc9d6fcb7e17cba6b`. Upcoming Birthday schedule input
now uses optional `Domain::GregorianDate`; invalid and out-of-domain `QDate`
references still produce an empty schedule. Both focused test binaries passed
(Application 14/14, Feature 12/12), and all five focused CTests passed,
including invalid-reference, ordering, and leap-day cases. The Feature run
emitted two offscreen Qt warnings; all tests passed. `git diff --check` passed.
No full suite or Windows build ran.
F1167 completed at source/test commit
`cc652bdea3c7c18c2d006852940526ee5eaa8e74`, following acceptance commit
`0737fc598309d0f0736a754f8f27a848a1c46f61`. Application counts and totals now
use the non-converting `TeacherImportCount`; the repository explicitly unwraps
values for the unchanged legacy summary. The Application, Apply-port,
repository, dialog, and sidebar success tests passed; five focused CTests
passed. The repository and dialog binaries each had one optional external
sample skip. The full UI parity binary and its `wizardSuccessTranscript`
selector crash on macOS in `QWizard`/NSBundle initialization; the required
sidebar success selector passes. `git diff --check` passed. No full suite or
Windows build ran. F1168 completed at source/test commit
`acace84eb2d572c52c9cabfde95c047b74799236`, following acceptance commit
`ab102aaefb21fe33c7a8628c1d80aac65ee38e30`. The manual and startup
Application artifacts now share the opaque `UpdateArtifactSha256` value,
preserving exact text without value-level validation or normalization; the
legacy release parser keeps its existing input policy. The macOS Debug build
covered `ClassMngr` and four focused test targets. Independent review accepted
the change; all four focused binaries and matching CTests passed, including 24
controller tests and 30 updater/parser tests. Local loopback access was needed
for the test servers. `git diff --check` passed. No full suite or Windows build
ran. F1169 is accepted/in progress to type Sub Prep navigation weekday and
optional start-minute facts while preserving unknown-day filtering, range
guards, unknown ordering, and the earliest-day behavior. Batch490 is discovered
for after F1170, with seven ordered candidates and a recorded discovery
shortfall. Phase 0 and Phase 2 gates remain open. The Phase 2 progress log
records prior focused evidence, including outstanding ClassesPage assertions
and the ScheduleWidget CTest timeout.

F1148 completed at source/test commit
`5a1756f54173affb33853f3e5dd0b8d8b3ce793d`, after acceptance commit
`2320a316750090983dd0ec424d4eee9fd4ca0147`. The current-campus preference
and `PersonalDetailsSaveRequest::campus` now use exact opaque
`Application::CurrentCampusPreferenceValue`; missing/unavailable fallbacks,
legacy aliases, and Calendar, My Details, Sub Prep, and Initial Setup behavior
remain unchanged. Incremental macOS builds passed for all seven assigned
targets. Both port CTests passed 2/2; My Workspace passed 4 selectors (6
QtTest cases), Sub Prep 3 selectors (5 cases), and Calendar 3 selectors
(5 cases), including exact embedded-NUL persistence and unknown nonempty
Calendar behavior. `git diff --check` passed. Both Initial Setup selectors
abort before assertions in the known macOS QWizard/NSBundle harness and remain
unverified; no full suite, Windows run, fresh configure, or clean build was
performed. F1149 completed at source/test commit
`91306a8473628064974b2956a803b2b8f0b1af25`, following acceptance commit
`127df8c24e375dc432853ae53387d4a8abf35697`. The portable package now carries
`Application::ClassTransferExportedAtUtc`; the query formats exact canonical
UTC text, and the JSON serializer and legacy adapter unwrap/wrap it at their
existing boundaries. Incremental builds passed for all five assigned targets;
the four focused CTests passed 4/4. Direct QtTest results were export query
11, export source 14, apply adapter 5, and codec parity 12 passed. Exact
`1970-01-01T00:00:01.234Z` output, offset import normalization, and the
invalid/empty legacy adapter round-trip passed. `git diff --check` passed. No
full suite, Windows run, fresh configure, or clean build was performed.
F1150 completed at source/test commit
`801b4d441054acf1d2a7858e6d8d6b36cb25d047`, following acceptance commit
`a508c8f71683f4140c176ff8634c23193347e5de`. Schedule Import review decisions
now use `Application::ScheduleImportReviewRoomValue` for imported-room
membership and selected-room values, preserving adapter-owned trimming,
first-seen ordering, empty filtering, and exact case-sensitive membership.
Incremental macOS builds passed for all four targets; both focused Application
CTests passed 2/2, the review-decision binary passed 37 cases, and the dialog
selectors passed 3/3 (5 QtTest cases including setup/cleanup). A new case-only
mismatch returns `ForeignTeacherRoom`. The first build caught three raw-string
test fixture initializers, corrected before the passing retry. `git diff --check`
passed. No full suite, Windows run, fresh configure, or clean build.
F1151 completed at source/test commit
`7aa21f36c553933545829bc5e6873db263c3625a`, following acceptance commit
`106ca0cf2924992a6fa62055fd6c3f27303f85cf`. Schedule Testing read rows now use
exact owning `Application::ScheduleTestingAssignmentKey` values for the raw
UTF-16 weekday/start-time pair. Repository order, UI lookup projection,
save-time accepted forms/canonicalization, and the `-1` unassigned class ID
sentinel remain unchanged. All four assigned incremental macOS targets built;
the focused Application and Platform CTests passed 2/2, with direct QtTest
counts 9/9 and 6/6. Two of three selected Schedule widget tests passed 3/3
each. The save/reload selector timed out after 70.547 seconds after
`initTestCase`, with no assertion result; it was interrupted and remains
unverified, with no assertion failure observed. `git diff --check` passed. No
full suite, Windows run, fresh configure, or clean build was performed.
F1152 is accepted to add Qt-free `Application::ClassAnalyticsEvaluationSelection`
with distinct All and owning exact UTF-16 name cases. Blank UI item data maps
to All; nonblank item data maps to an exact name, while translated display text
is never parsed. The name remains unnormalized in the value; Application keeps
the existing trim-before-exact-case-sensitive match, successful empty result
for unknown names, and global year-to-date projection. Explicitly typed
`ExactName("All")` remains name mode, separate from the legacy text adapter's
case-insensitive All spelling. Default-selection policy and its persisted
preference remain out of scope. The focused gate is pending.

Batch487 has eight complete slices, F1152 accepted/in progress, and F1153
discovered. Batch488 has eight discovered candidates and is not yet active.
F629 remains active pending source-bound Windows layout evidence; Phase 0 and
Phase 2 gates remain open.

Batch471 is closed and pushed to `origin/Qt-Rewrite` at `0a570252`. Batch472
is now complete: F1081-F1085 are committed at `a9066ae7`, `c48d43a4`,
`e26e866b`, `14fd7ae4`, and `116719e1`. F1085 preserves Calendar preferences
when a technical read error occurs: the failed option group shows a diagnostic
and blocks its writes, while a successfully loaded group remains saveable.
Missing, invalid, and unavailable settings retain their defaults/fallbacks.
Independent verification built `ClassMngr` and four affected targets; focused
CTests passed 4/4 with 50 QtTest cases, and `git diff --check` passed. No full
suite ran.

F1086 is complete at source/test commit `aeb2a46f`. It preserves the Schedule
display mode after technical preference-read failures and prevents a failed
read from triggering legacy migration. Independent focused verification
passed the preference-port and My Workspace CTests (2/2), the two targeted
ScheduleWidget QtTest cases, and `git diff --check`. The broad ScheduleWidget
CTest hung without output and was interrupted; no full suite ran.

Batch473's five discovered candidates are F1086, F1087 Middle School Analytics
visibility, F1088 Evaluation default policy, F1089 personal signature
settings, and F1090 Sub Prep Zoom preference, all covering technical
preference-read failures. At the user's direction, F629 was resumed from
Deferred and selected immediately after F1086. The Phase 2 plan and progress
log hold the ordered list and discovery evidence.

F629's initial clean current-source macOS universal matrix ran at commit
`3ab4fa3a`.
The package audit passed for 116 Mach-O files with arm64 and x86_64 slices and
14.4 minimum deployment targets; signature checks and the runner self-check
passed. All 24 routes were attempted: 23 completed, and `output-sub-prep`
failed the existing strict per-page semantic PDF comparison. Generated page 9
contains Teachers 18-20 while the reference page 9 contains Teacher 18;
generated page 10 contains Teacher 21 while the reference page 10 contains
Teachers 19-21. A focused Mac trace confirms breaks before Teachers 18 and 21,
but supplies no Windows frame or break decisions. The checked assertion was
not weakened and no renderer change was justified by this evidence. F629 and
the combined Phase 0 exit gate remain open. Full evidence is under
`/private/tmp/classmngr-f629-macos-universal-20261019-run3`; the focused trace
is under `/private/tmp/classmngr-f629-subprep-pagination-trace-20261019`.

Follow-up evidence changes the diagnosis. Clean Windows run
`07aa1f005f100b747780ed4c11fc35bc73517014` used the same fixture as Mac run5
(`667ff54e`) and passed `output-sub-prep`: all 12 Sub Prep pages and all five
roster pages matched the historical reference. Production and test sources
are unchanged between those commits; the historical reference is still
`sourceBound=false` and `semanticOracleOnly=true`, so this proves a
platform-dependent output difference, not which pagination is the product
contract. The `QTextDocument::setUseDesignMetrics(true)` candidate built and
passed signature checks, but its focused retry still failed strict parity on
pages 9-10 with the same Mac assignments; it was removed. F629 remains open.
Next compare Windows and Mac page geometry, card wrapping, and break decisions
before selecting another renderer change. The strict comparator stays intact.

F1087 is complete at source/test commit `7ec8ba7a`. The Middle School Analytics
preference port now returns a typed `Result<bool>` and propagates technical
read errors without writing a false fallback. Missing, invalid, and unavailable
settings retain their existing false defaults and legacy QVariant coercion.
ClassesPage and the Preferences control preserve the known in-memory
Analytics/Evaluations visibility when a read fails. Independent verification
built both affected targets and passed the focused preference-port and
MainWindow failure CTests (2/2); the existing save-failure rollback/retry
coverage passed, as did `git diff --check`. The read-failure test drops
`app_settings`, so it cannot independently count write attempts during that
fault; the adapter's error path returns before any write. No full suite ran.

F1088 is complete at source/test commit `53c43e75`. The Evaluation default
policy port returns `Result<EvaluationDefaultPolicy>` and propagates technical
read errors without writing `All`. Missing, invalid, and unavailable settings
retain their defaults, and unsupported stored values remain unchanged.
Preferences shows a load diagnostic and disables the two policy radios when
the value is unknown; failed save/reload restores the last known policy. The
Class Analytics default-selection service returns no automatic selection on a
technical read error. Independent verification built the port, selection
integration, and MainWindow failure targets; focused CTests passed 3/3,
including save-failure rollback/retry. `git diff --check` passed. The SQLite
missing-table fault gives a generic diagnostic and cannot count write attempts
while the table is absent; the adapter returns before writing. No full suite
ran.

F1089 is committed at `15c5d035`. The personal signature preference port now
returns a typed technical error if any mode, typed-text, or font read fails;
missing or invalid fields keep their existing defaults. My Details preserves
the visible signature controls, marks the preference bundle unknown, and
blocks aggregate saves until a successful reload. Initial Setup shows a
retryable warning and returns before aggregate save on a technical read
failure.

Independent verification built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesPersonalSignaturePreferencesPortTests`,
`ClassMngrMyWorkspacePageTests`, and `ClassMngrInitialSetupWizardTests`. The
focused signature-port and My Details CTests passed 2/2, including technical
failure injection for each preference and save/reload recovery;
`git diff --check` passed. The new Initial Setup regression compiled but could
not reach its assertions: the macOS Debug harness aborts in Qt's QWizard
page/pixmap path with `NSBundle initWithURL: nil URL argument`. An existing
Initial Setup wizard test aborts at the same path, so the product behavior remains
unverified there; the release app-bundle attempt also failed before QtTest
started because macOS pasteboard/XPC services were unavailable. No full suite
ran.

F629 was revalidated from clean source at `667ff54e` after F1090. The fresh
universal Release/Debug build and 116-Mach-O package audit passed in
`/private/tmp/classmngr-f629-macos-universal-20261019-run4`. That first route
attempt was sandboxed: both Calendar Import routes failed immediately because
the local test server could not bind `QHostAddress::LocalHost`. A fresh
all-route run reused the verified app and harness with loopback access in
`/private/tmp/classmngr-f629-macos-universal-20261019-run5`; all 24 routes were
attempted, 23 completed, and only `output-sub-prep` failed. The generated and
reference PDFs have the same page count and dimensions. Page 9 contains
Teachers 18-20 in the generated PDF and Teacher 18 in the reference; page 10
contains Teacher 21 in the generated PDF and Teachers 19-21 in the reference.
The strict page-by-page semantic comparator remains unchanged. The Mac evidence
does not establish Windows layout decisions, so no renderer fix is claimed and
F629 remains open and unaccepted. The Mac had approximately 48 GiB free before
the build and 44 GiB afterward; 18 GiB is the runner's preflight minimum, not
observed consumption.

F629 diagnosis confirmed that the parity difference is real but does not yet
establish a renderer defect. The Mac run5 manifest ties the generated output
to source commit `667ff54e`, while its historical reference is explicitly
`sourceBound=false` and `semanticOracleOnly=true`. The renderer measures
teacher-card frame bounds and inserts breaks before groups that span a page;
the Mac trace records breaks before Teachers 18 and 21. Bundled Inter and
Pretendard fonts loaded successfully, but the run does not record effective
glyph metrics or Windows layout decisions. The checked-in Windows route summary
has no source identity and omits original route artifacts, and no source-bound
Windows output for `667ff54e` or Windows route runner is available in this Mac
workspace. No renderer change is justified by these artifacts. F629 remains
active and Batch473 remains open; the next evidence step is a source-bound
Windows run at the same commit, fixture, and font assets, or another
evidence-backed resolution, followed by the unchanged Mac parity check.

F1090 is complete at source/test commit `f78791a9`. The Sub Prep Zoom
preference port now returns `Technical` for any primary or required legacy
read failure. It does not read legacy values after a technical primary error,
and stages migrations until all three preferences have been read
successfully. Primary precedence, missing-value defaults, legacy fallback and
migration, UTF-8/whitespace, and best-effort migration-save behavior remain
unchanged. My Details blocks aggregate saves until the Zoom bundle reloads
successfully; Initial Setup warns and returns before aggregate save; Sub Prep
preserves its read-only Zoom fields and reports the diagnostic.

Independent verification built the four affected test targets. The Zoom port
CTest passed 10/10 and the full My Details CTest passed. Direct focused Sub
Prep Zoom, F1079 read-failure/reload, save-failure rollback/retry, and
unavailable-save cases passed. The Initial Setup Zoom regression passed
through the real `initializePage()`/`validatePage()` path without showing the
wizard. The full Sub Prep class hung in the existing
`unavailablePreferenceLoadsPreservePageValues` case and was interrupted after
104 seconds. The full Initial Setup class aborts in the existing macOS Qt
QWizard page/pixmap path (`NSBundle initWithURL: nil URL argument`), also
reproduced by an existing wizard test; navigation/display runtime remains
unverified. `git diff --check` passed; no full suite ran.

Phase 2 remains In Progress/Open. Gates 1 and 2 remain Partial, and F1007
remains pending policy reconciliation. Exact acceptance, verification,
commit, and discovery evidence is in
`plans/qt-rewrite-heavy-route-plan/03-Phase-2-Progress-Log.md`.

F1136 is complete at source/test commit `98ed1894`, following acceptance
`6b456bff`. The Application skipped-update preference now carries exact
stored text, while the Platform adapter keeps trimming and blank-to-missing
behavior. The numeric version parse and literal suppression comparison remain
separate; focused macOS verification passed all three policy, preferences-port,
and automatic-startup controller CTests. F1137 completed Batch485 at
`dcdc9be8`; focused macOS verification passed the Domain contract and Schedule
Import plan-validation CTests (2/2; 38 Domain cases). F1138 is complete at
source/test commit `fd3c0462`; focused macOS Debug verification passed the
Domain and Platform CTests (2/2) and the selected Calendar Page color-picker
failure/resync/retry case. `ClassMngr` and the affected targets built, and
`git diff --check` passed. F1139 completed at source/test commit `05c9de8c`;
the focused Platform adapter and Calendar Import CTests passed 2/2. F1140
completed at source/test commit `230f7a00`; its Platform CTest passed 1/1 and
both selected UI read-error cases passed. F1141 completed at source/test
commit `73b9a8e7`; all five affected targets built in the documented macOS
Debug preset, the Platform CTest passed 1/1 with 11 direct QtTest entries, and
the selected Sub Prep and My Workspace cases passed. The Initial Setup case
could not reach its assertions because the registered offscreen Qt Widgets /
QWizard test aborts with an `NSBundle initWithURL: nil URL argument` exception
in both the Phase 0 and standard preset trees; its cause is unresolved. The
Initial Setup target compiled, but behavior remains unverified. F1142 is
complete at source/test commit `67f7277a`; the focused Platform and ColorUtils
CTests passed 2/2 after the incremental macOS Debug build. F1143 completed at
`686bdc2a`; the incremental macOS Debug build passed, all nine focused CTests
passed, and `git diff --check` passed. Batch486 is complete locally; its
closure commit `d048f231` remains unpushed while exact-payload approval is
pending after automatic review rejected the push. Batch487 is active. F1144
completed at source/test commit `e523001d`, following acceptance commit
`f648d367`; the six affected targets built incrementally on macOS, three
focused CTests passed, and the My Workspace save selector passed. The two
focused Initial Setup selectors aborted before assertions with the known
offscreen QWizard `NSBundle initWithURL: nil URL argument` exception, so those
behaviors remain unverified. F1145 completed at source/test commit `119b961e`,
following acceptance commit `19a1b2d5`. The narrowed change splits Sub Prep's
raw read input from its normalized `CalendarEventType` query result; Calendar
create/edit text contracts remain unchanged. Three targets built
incrementally, and both focused CTests passed (Query 10/10, Platform 73/73).
The query suite covers padded labels and exact filtering; Platform trim and
prefilter code remained unchanged and was source-reviewed, while its fixtures
use canonical labels. F1146 completed at source/test commit
`fcbeadeeb61838d7829c0083715cd60556debf9e`, following acceptance commit
`d6e785d7c10f2938c07d93a9ee0f009cbafb764c`. Raw UI ISO-date input now converts
to typed `GregorianDate` request fields before the Application port; persisted
occurrence dates remain raw for Technical failure reporting. The incremental
build passed for `ClassMngr` and the Application, Platform, and UI parity test
targets. Both focused CTests passed (2/2; 73 QtTest cases in each), as did the
selected repeat-series suffix UI parity selector (3 entries). The first build
found three stale test-fixture string assignments; these were updated to valid
`GregorianDate` values and the same incremental build then passed.
`git diff --check` passed. No configure, clean build, full suite, or Windows
build ran. Batch487 has three complete slices (F1144–F1146), no accepted but
incomplete slices, and seven discovered slices; F1147 is next. Batch485 was
pushed to
`origin/Qt-Rewrite` at `192f676a`. F629 remains active but awaits
source-bound Windows layout evidence, which the user cannot run now; the Phase
0 and Phase 2 gates remain open. F1147 completed at source/test commit
`6a89aa0f6459fc949fe4858ac164deffe619bbe5`, following acceptance commit
`7ebab48b47599a66079edc361146869064e05982`. The shared exact-byte
`Application::CampusCode` now types both Calendar Page metadata output and
Calendar Import's campus-code query; the importer retains its existing trim,
blank filtering, first exact duplicate, and order behavior. Calendar Page
preserves its optional nonempty codes, UTF-8, whitespace, embedded NUL bytes,
and repository order. Both Calendar Page and Campus Dashboard convert the
value at their UI boundary without changing downstream behavior.

The incremental build passed for `ClassMngr`, both campus-query adapter test
targets, and `ClassMngrCalendarImportTests`. The first attempt automatically
regenerated CMake/Ninja for the new header, then found a stale raw-string
conversion in Campus Dashboard; the Executor fixed it to read `CampusCode::value()`
under the existing optional guard, and the retry passed. The three focused
CTests passed 3/3 with 32 QtTest cases total (21, 5, and 6). `git diff --check`
passed. No standalone configure, clean build, full suite, or Windows build ran.
Batch487 has four complete, no accepted/in progress, and six discovered
candidates; F1148 is next. Batch486's completed-batch push remains pending the
user's response to the exact-payload approval request.

## Next Milestone

Complete candidates #41–48 in Batch 4 and push the batch. Begin the next-batch
discovery when candidate #47 starts. Resolve the Windows diagnostic
prerequisite build without disturbing the protected Gateway work, then obtain
and review a fresh artifact. Do not
declare Phase 7 complete until the 7A and Phase 7 exit gates pass; see the
[Phase 7 plan](../plans/qt-rewrite-heavy-route-plan/08-Phase-7-Feature-Migration.md)
and [progress log](../plans/qt-rewrite-heavy-route-plan/08-Phase-7-Progress-Log.md).

## Current Phase 2 position - 2026-09-26

F59 is committed as `7769912e1a8ccec02ecc3ace2de11ff98c719327` (`Phase2 -
Type Schedule Import state validation IDs`). Its Qt-free apply-state
validation contract now carries typed teacher and class identities through
resolutions, snapshots, links, and projected classes. The repository adapter
retains action-aware conversion of legacy nonpositive sentinels. Tests cover
typed-ID separation, missing and stale selections, skip validity and
uniqueness, mismatch rejection, numeric conflict ordering, and projected
overlap behavior.

Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 verification validated 912
handwritten source owners, built the state-validation and Schedule Import
targets, and passed `ClassMngrNextApplicationScheduleImportStateValidationTests`
and `ClassMngrScheduleImportTests` (2/2). Coverage includes the mismatched
teacher-key skip regression and `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase`.
No full suite was run. There is no exhaustive direct test matrix for every
action-specific sentinel conversion.

Gates 1 and 2 remain Partial. The workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep is bounded to the current and following calendar years. The user
change to `cmake/sources.cmake` was excluded; its SHA-256 remains
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: compare the remaining Phase 2 gaps for a bounded F60 slice and continue
the source-then-documentation commit sequence.

## Current Phase 2 position - 2026-09-26 (F60)

F60 is committed as `730955dd1feb24e5a46dd0bfef9f86b8ff619621` (`Phase2 -
Type Schedule Import review decision IDs`). The Qt-free review-decision
contract uses optional `Domain::ClassId` targets for class resolutions and
issues. The PlanValidator and review dialog translate legacy positive integer
targets at their feature boundaries; nonpositive sentinels become absence.
UpdateExisting still requires a target, CreateNew forbids one, and Skip keeps
an optional exact-match target. Duplicate-target issue details and claim order
remain covered.

Fresh independent x64 Ninja/MSVC 19.51/Qt 6.12 verification validated 912
handwritten source owners, built the three focused targets in 321 steps, and
passed `ClassMngrNextApplicationScheduleImportReviewDecisionsTests`,
`ClassMngrScheduleImportDialogTests`, and `ClassMngrScheduleImportTests`
(3/3). The Schedule Import fixture still applies `schedule_review.xlsx`; the
app-less contract separately passed 1/1 after adding explicit Skip-without-
target coverage. No full suite was run. The duplicate-target warning test also
asserts the resolved existing class label `E5 Athena`.

Gates 1 and 2 remain Partial; the workspace boundary and audited `src/next`
dependency isolation remain Satisfied. The Phase 2 exit gate remains Open.
F59's exhaustive per-action sentinel matrix remains a coverage gap. Sub Prep
remains bounded to 2026–2027. The protected user change to
`cmake/sources.cmake` remains excluded with SHA-256
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: compare the remaining gated gaps for F61 and keep source and
documentation commits separate.

## Current Phase 2 position - 2026-09-26 (F61)

F61 is committed as `5e08c2aab8c4c326463e969445757fa90e25d79c` (`Phase2 -
Cover evaluation default read failure`). The production integration test now
distinguishes a successful empty current evaluation, which selects Summer for
M2 on 2026-09-07, from an evaluation query failure after the table is removed,
which returns no default. It also confirms the repository read itself fails.

Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 verification validated 912
handwritten source owners and passed
`ClassMngrEvaluationDefaultSelectionIntegrationTests` (1/1). No full suite was
run. Gates 1 and 2 remain Partial; workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026–2027). F59's action/sentinel matrix remains a separate coverage gap. The
protected user change to
`cmake/sources.cmake` remains excluded, with SHA-256
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: select a bounded F63 slice and continue separate source and documentation
commits.

## Current Phase 2 position - 2026-09-26 (F62)

F62 is committed as `691e56fbdcc536aaaf577602feeac25fc5b7227f` (`Phase2 -
Characterize Schedule Import sentinels`). The repository apply tests now cover
the reachable action-specific teacher and class sentinel behavior, including
Reuse/UpdateRoom with teacher IDs -1/0, Create/Skip teacher targets, CreateNew
and Skip class targets, exact and mismatched Skip targets, and stale positive
UpdateExisting IDs. Rejected apply cases compare persisted database snapshots.
F60's PlanValidator rejects some class-target combinations before repository
state validation; those cases are recorded as upstream rejections.

Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 verification built
`ClassMngrNextApplicationScheduleImportStateValidationTests` and
`ClassMngrScheduleImportTests`; exact CTest passed 2/2, including the existing
`schedule_review.xlsx` apply fixture. No full suite was run. CreateNew class
sentinels cannot independently prove F59 adapter conversion because F60
normalizes nonpositive targets to absence before state validation; the cases
assert end-to-end apply behavior only.

Gates 1 and 2 remain Partial; the workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026-2027). The protected user change to `cmake/sources.cmake` remains
excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: compare remaining Gate 1 and Gate 2 gaps for F63.

## Current Phase 2 position - 2026-09-26 (F63)

F63 is committed as `bf4251eca530066ba65b00021f63779d185bd64e` (`Phase2 -
Cover Schedule Import missing suggestions`). The checked-workbook production
preview now asserts the first M3/Song's candidate has no matching class IDs,
legacy `suggestedClassId == -1`, `exactMatch == false`, and confidence
`None`. This exercises the repository adapter when the typed matching
projection has no suggestion; the existing app-less projection coverage
already verifies the optional absence.

Two fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds each
validated 912 handwritten source owners and built
`ClassMngrScheduleImportTests` plus
`ClassMngrNextApplicationScheduleImportMatchingProjectionTests`. The exact
CTest names passed 2/2 in both trees. The checked-in
`schedule_review.xlsx` apply fixture passed. No full suite was run.

F63 closes the directly observable F58 production no-suggestion sentinel gap.
Gate 1 and Gate 2 remain Partial; the workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026-2027). The protected user change to `cmake/sources.cmake` remains
excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: compare remaining Gate 1 and Gate 2 gaps for F64.

## Current Phase 2 position - 2026-09-26 (F64)

F64 is committed as `559b4feaa8fd67c01cd2f4d0f3ddd7dc0f166de5` (`Add typed
student name pairs to roster score import`). The Qt-free
`Domain::StudentNamePair` stores exact UTF-16 English and Korean components,
rejects an empty half, and provides equality and ordering. Roster score import
now uses the typed pair after trimming at the Qt boundary; empty-pair skipping
and last-write-wins behavior for duplicate saved scores are preserved. The
structured key removes delimiter ambiguity for invalid stored names containing
U+001F.

Two fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds passed
`ClassMngrNextDomainContractTests` and
`ClassMngrRosterEditorWidgetImportTests` (2/2 each). Coverage includes exact
pair identity, empty halves, ordering, UTF-16 values, one-sided trim through
the real import, persistence/idempotence, and a legacy stored duplicate pair
whose later grade wins. No full suite was run.

Gate 1 and Gate 2 remain Partial; the workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026-2027). The protected user change to `cmake/sources.cmake` remains
excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: select F65 from remaining Phase 2 gaps and keep source and documentation
commits separate.

## Current Phase 2 position - 2026-09-26 (F65)

F65 is committed as `a4fbffb91228ab1d783ac782ff572d49d3c28b65` (`Cover legacy
profile migration through FileController`). The lifecycle integration test
materializes `legacy_startup.sql` as a `.db`, opens it through FileController
and the workspace coordinator, and verifies the active path, teacher, class,
ClassInfo, and schedule data. Migration repairs the invalid teacher link to
SQL NULL while the service retains its `-1` unassigned sentinel; the active
schema reaches version 6 and the retained `.pre-schema-v4-backup` contains
schema version 3.

Fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds each
validated 913 handwritten source owners and passed
`ClassMngrFileControllerWorkspaceLifecycleTests` and
`ClassMngrDatabaseSchemaManagerTests` (2/2). No full suite was run.

Gate 2 gains production-path legacy `.db` migration evidence and remains
Partial; Gate 1 remains Partial. The workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026-2027). The protected user change to `cmake/sources.cmake` remains
excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: select F66 from the remaining Phase 2 gaps; keep source and documentation
commits separate.

## Current Phase 2 position - 2026-09-26 (F66)

F66 is committed as `9afa17f47aadb7188916cc091e370f5d0bea98bb` (`Extract Sub
Prep teacher display name rule`). The new Qt-free `Domain::TeacherDisplayName`
owns the selected UTF-16 value and applies preferred name, English name,
preferred romanization, then Korean name precedence. Both Sub Prep platform
adapters trim with Qt at their boundaries and keep their existing empty
fallbacks: summary `N/A`, class details empty. No interval logic changed.

Two fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds each validated
914 handwritten source owners, built the Domain contract and Sub Prep print
source-port test targets, and passed the exact two CTests (2/2). No full suite
was run.

Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open.
Sub Prep remains bounded to the current and following calendar years
(2026-2027). The protected user change to `cmake/sources.cmake` remains
excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: compare the remaining Phase 2 Gate 1 and Gate 2 gaps for F67; keep source
and documentation commits separate.

## Current Phase 2 position - 2026-09-26 (F68)

F68 is committed as `3ee0b1c6` (`Extract Qt-free calendar campus visibility
policy`). The campus-token matching rule now lives in the Qt-free Application
policy; the existing Qt feature adapter retains title trimming, campus-code
normalization, and one-to-one Unicode case folding. App-less, typed-summary,
and legacy-record coverage includes token boundaries, punctuation, `S2` versus
`S20`, Kelvin sign, and dotless i.

Executor and independent Tester used separate fresh Windows x64 Debug
Ninja/MSVC 19.51/Qt 6.12 builds, each validating 915 handwritten source owners
and passing the three focused Application Calendar Event, Calendar Event
Cache, and Calendar Import CTests (3/3). `git diff --check` passed; no full
suite was run. These are adapter regressions, not checked-in baseline fixture
parity evidence.

Gate 1 gains direct app-less behavior evidence and remains Partial. Gate 2
remains Partial; workspace boundary and audited `src/next` dependency
isolation remain Satisfied. Phase 2 and its exit gate remain Open. Sub Prep
remains bounded to the current and following calendar years (2026-2027). The
protected user change to `cmake/sources.cmake` remains excluded; its SHA-256
is `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Next: select F69 from three independent Investigator reviews and keep
source/documentation commits separate.

## Current Phase 2 position - 2026-09-26 (F69)

F69 source/test commit `95aaefa4` (`Extract Schedule Import state projection`)
adds a Qt-free Application projection for the final schedule rows. It uses
typed references to existing `ClassId` values or new candidate indices, with
explicit `ReplaceRows` and `KeepExistingRows` dispositions. Repository apply
computes the projection once, passes it to overlap validation, and resolves
new candidate IDs only after their class rows are inserted before persisting
from that same projection. Intensive `UpdateExisting` keeps untouched row IDs
intact; normal/replacement imports and skipped targets retain their existing
behavior.

Executor and independent Tester used separate fresh Windows x64 Debug
Ninja/MSVC 19.51.36257/Qt 6.12 trees. Each validated 916 handwritten source
owners and passed `ClassMngrNextApplicationScheduleImportStateValidationTests`
and `ClassMngrScheduleImportTests` (2/2). Coverage includes persisted
`schedule_review.xlsx` rows, `schedule_overlap_conflict.xlsx` rejection before
writes, intensive preservation/replacement, skipped exact match, and rollback
after failure. `git diff --check` passed; no full suite was run.

Gate 1 gains direct app-less schedule-state projection evidence and Gate 2
gains fixture-backed persisted-row parity; both remain Partial. Workspace
boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2
and its exit gate remain Open. Sub Prep remains capped at the current and
following calendar years (2026-2027). The protected user change to
`cmake/sources.cmake` remains excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
F69 plan and mapping documentation was committed separately as `4140b6b1`.
Next: finish F70 plan/mapping and deployment documentation in a separate
commit, then select F71 from three independent Investigator reviews.

## Current Phase 2 position - 2026-09-26 (F70)

F70 source/test commit `2f3d414c` (`Extract Class Transfer preview matching
policy`) moves candidate matching from the Qt repository implementation into
`src/next/application/class_transfer_matching_policy.h`. The adapter retains
`QString::simplified().toCaseFolded()` normalization and maps typed
`ClassId`/`TeacherId` matches to the legacy integer preview. App-less tests
cover both-name and single-name teacher rules, course and teacher identity,
assigned-but-unloaded teacher identity, the unassigned fallback, and stable
source/destination order. Production tests cover Qt whitespace and ASCII case
normalization plus the checked-in success/conflict preview IDs and conflict
no-write path. This does not claim exhaustive Unicode case-fold equivalence.

Executor and independent Tester used separate fresh Windows x64 Debug
Ninja/MSVC 19.51.36257/Qt 6.12 builds. Each validated 917 handwritten source
owners, built the two Class Transfer test targets, and passed exactly
`ClassMngrNextApplicationClassTransferTests` and `ClassMngrClassTransferTests`
(2/2). `git diff --check` passed; no full suite was run.

Gate 1 gains direct app-less matching-policy evidence. Gate 2 revalidates
fixture-backed preview and conflict no-write behavior through the moved rule;
both remain Partial. Workspace boundary and audited `src/next` dependency
isolation remain Satisfied. Phase 2 and its exit gate remain Open. Sub Prep
remains capped at the current and following calendar years (2026-2027). The
protected user change to `cmake/sources.cmake` remains excluded; its SHA-256
is `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
F70 plan/mapping and canonical deployment documentation are committed
separately as `91d01551`. F71 source/test commit `9b090cb5` types the
Class Transfer review-decision IDs and verifies a successful replacement from
the checked-in success fixture. The two focused Class Transfer CTests passed
in separate executor and independent fresh builds, each with 917 handwritten
source owners. Gate 1 and Gate 2 advance but remain Partial; the Phase 2 exit
gate remains Open. Next: finish F71 plan/mapping and deployment documentation
in a separate commit, then select F72 from three independent Investigator
reviews.

## Current Phase 2 position - 2026-09-26 (F71)

F71 source/test commit `9b090cb5` (`Type Class Transfer review decision
identities`) changes app-less match candidates and issue targets to typed
`ClassId`/`TeacherId` values and selected targets to optional typed IDs. The UI
and repository adapters retain legacy integer APIs, convert positive IDs,
translate exactly `-1` to absence, and reject other nonpositive IDs with the
existing action-specific messages and validation precedence.

App-less tests assert field categories, optional target values, typed issue
identities, and existing missing, invalid-action, match-set, and duplicate
rules. The checked-in `success_source.json` now exercises a successful Replace
with an exact class/teacher match: it retains the destination class ID and
teacher profile, replaces course details, schedule, and roster, and clears old
evaluation rows. Existing Create fixture behavior and conflict no-write
coverage remain. Adapter tests cover malformed targets in both UI and
repository paths.

Executor and independent Tester used separate fresh Windows x64 Debug
Ninja/MSVC 19.51.36257/Qt 6.12 builds. Each validated 917 handwritten source
owners and passed `ClassMngrNextApplicationClassTransferTests` and
`ClassMngrClassTransferTests` (2/2). `git diff --check` passed; no full suite
was run.

Gate 1 gains a typed app-less decision contract and Gate 2 gains
fixture-backed successful replacement parity; both remain Partial. Workspace
boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2
and its exit gate remain Open. Sub Prep remains capped at the current and
following calendar years (2026-2027). The protected user change to
`cmake/sources.cmake` remains excluded; its SHA-256 is
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
F71 plan/mapping and canonical deployment documentation are committed
separately as `5a9fe695`. F72 source/test commit `aa1af5fe` adds checked-in
fixture parity for successful `Teacher::ReplaceExisting`, including all
non-identity profile fields, retained teacher identity, and the imported
class's teacher link. Executor and independent fresh builds passed the two
focused Class Transfer CTests (2/2), each with 917 source owners. Gate 2 gains
fixture-backed teacher replacement evidence; Gate 1 receives no new evidence.
Both remain Partial and the exit gate remains Open. F73 source/test `60bbd015`
and documentation `5af94f32` are complete. F74 source/test `3e0a8d64` adds
synthetic Intensive workbook parse/apply coverage; the independent fresh build
passed three focused Schedule Import targets (3/3) with 917 source owners. The
synthetic path adds production-flow coverage but does not establish historical
baseline parity; Gate 2 remains Partial. F75 source/test commit 3139bdf4 adds a
Qt-free exact typed duplicate-pair grouping policy and adapters for shared,
roster, and speaking-evaluation validation. The fresh independent run passed
four focused targets plus the existing score-import regression target (5/5).
Gate 1 gains app-less contract evidence but remains Partial; Gate 2 remains
Partial without new historical parity. Select F76 for checked-fixture Class
Transfer evaluation persistence coverage.

## Current Phase 2 position - 2026-09-26 (F74)

F74 source/test commit 3e0a8d64 (Verify synthetic intensive schedule import flow) adds a source-readable synthetic Intensive worksheet fixture and a production parser → repository preview → explicit UpdateExisting → apply test. Literal assertions verify parsed candidate rows and preview match, persisted Intensive rows, retained target class identity with no class creation, unchanged IDs and values for an untouched Intensive row, and unchanged regular schedule rows.

Executor used build/phase2-f69-schedule-state-projection-executor-ninja-msvc-20260926 (Windows x64 Debug Ninja/MSVC 14.51.36231/Qt 6.12.0). Independent Tester used a fresh tree at C:\Users\wfelt\AppData\Local\Temp\f74_intensive_verify_20260926 (Windows x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12.0). CMake validated 917 handwritten source owners. ClassMngrScheduleImportTests, ClassMngrNextApplicationScheduleImportStateValidationTests, and ClassMngrNextApplicationScheduleImportMatchingProjectionTests built and passed (3/3). git diff --check passed; no full suite was run.

F74 is synthetic production-flow coverage only: the repository has no historical Intensive workbook paired with a legacy-output oracle. Gate 2 remains Partial; Gate 1 receives no new evidence. Workspace boundary and audited src/next dependency isolation remain Satisfied. Phase 2 and its exit gate remain Open. Sub Prep stays capped at the current and following calendar years (2026-2027). The protected cmake/sources.cmake remains excluded at SHA-256 9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF. F75 source/test commit 3139bdf4 adds exact typed duplicate-pair grouping and current roster/speaking-evaluation adapters; F76 is selected for checked-fixture Class Transfer evaluation persistence coverage.

## Current Phase 2 position - 2026-09-26 (F75)

F75 source/test commit 3139bdf4 (`Extract typed duplicate student pair grouping`) adds the ordered `duplicateStudentNamePairGroups` policy in the existing Qt-free `StudentNamePair` header. Shared validation, `RosterModel`, and `SpeakingEvalModel` adapt trimmed complete pairs while preserving incomplete-row handling and caller-specific diagnostics. Score-import matching and last-write-wins behavior are unchanged.

Executor and independent fresh Tester builds validated 917 handwritten source owners. Windows x64 Debug Ninja builds passed `ClassMngrNextDomainContractTests`, `ClassMngrSharedPolicyTests`, `ClassMngrRosterModelTests`, and `ClassMngrSpeakingEvalBatchReportServiceTests` (4/4). Tester also built and passed `ClassMngrRosterEditorWidgetImportTests` (1/1). The independent tree used CMake 4.4.2, Ninja 1.13.2, Qt 6.12.0, and MSVC 19.51.36257. `git diff --check` passed; no full suite was run.

F75 adds Gate 1 app-less typed grouping behavior; Gate 1 remains Partial. F76 source/test commit 7a8b80c6 adds checked-fixture Class Transfer evaluation persistence; Gate 2 gains regression evidence but remains Partial without historical baseline provenance. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 and its exit gate remain Open. Sub Prep stays capped at 2026-2027. Protected `cmake/sources.cmake` remains unstaged at SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`. F77 is selected to move the weekly Class Transfer schedule-overlap decision into a Qt-free Application policy while preserving current adapter semantics.

## Current Phase 2 position - 2026-09-26 (F76)

F76 source/test commit 7a8b80c6 (`Verify Class Transfer evaluation fixture persistence`) adds a named 11-column Fixture Evaluation to the checked-in success fixture. Fixture-driven create/apply and replacement tests compare all 25 persisted rows against literal expected values; replacement still verifies the destination-only evaluation is cleared.

Executor and independent fresh Windows x64 Debug builds used CMake 4.4.2, Ninja 1.13.2, Qt 6.12.0, and MSVC 19.51.36257. CMake validated 917 handwritten source owners; `ClassMngrClassTransferTests` passed 1/1 independently. `git diff --check` passed; no full suite was run.

F76 adds checked-fixture regression evidence, not historical baseline parity. Gate 2 remains Partial; Gate 1 remains Partial and unchanged. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 and its exit gate remain Open. Sub Prep remains capped at current and following calendar years, 2026-2027. The protected `cmake/sources.cmake` remains unstaged at SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.

## Current Phase 2 position - 2026-09-26 (F77)

F77 source/test commit `dd3bbc01` (`Extract Class Transfer weekly schedule overlap policy`) moves weekly half-open interval overlap decisions into a Qt-free Application policy. The repository retains exact legacy weekday/time parsing, raw diagnostic strings, localized conflict rendering, and duplicate-message suppression. App-less tests cover category isolation and deterministic order, touching boundaries, Sunday overnight wrap, and 24-hour equal endpoints. Adapter tests assert parsed overnight/equal-endpoint conflicts, exact diagnostics, and no writes; the checked conflict fixture asserts the combined regular/intensive diagnostic and unchanged destination state.

Independent fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. CMake validated 917 handwritten source owners. `ClassMngrNextApplicationClassTransferTests` and `ClassMngrClassTransferTests` passed 2/2; `git diff --check` passed. No full suite was run. Configure warnings for Vulkan headers, `vswhere.exe`, and long object paths were nonfatal.

F77 adds app-less weekly conflict policy evidence to Gate 1 and checked-fixture regression evidence to Gate 2; Gate 1 and Gate 2 remain Partial. The checked fixture is not an independently sourced historical-output oracle. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress and its exit gate Open. Sub Prep remains capped at the current and following years, 2026-2027. Protected `cmake/sources.cmake` remains unstaged at SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.

F78 is selected to drive the permanent `schedule_large_conflict.xlsx` duplicate-class-target case through Schedule Import repository apply. The test will assign both imported classes to one existing destination, require pre-write rejection, and compare seeded database snapshots. This is required checked-fixture regression coverage; it does not claim independent historical-output parity.

F77 is selected: move Class Transfer's weekly schedule-overlap decision into a Qt-free Application policy, while keeping time conversion and localized conflict rendering at the repository adapter. Preserve half-open boundaries, end-at-or-before-start overnight behavior including Sunday-to-Monday wrap, regular/intensive separation, Skip and replacement filtering, deterministic conflict order, and duplicate-message suppression. Keep the checked-in conflict fixture as the production adapter/no-write guard. Sub Prep is not changed.

## Current Phase 2 position - 2026-09-26 (F78)

F78 source/test commit 68ab0faa (Verify Schedule Import duplicate fixture rejection) adds a repository apply regression from permanent schedule_large_conflict.xlsx. It derives two distinct Alice E4 Hercules candidates, assigns both to one seeded existing target, asserts the exact duplicate-target error before any write, and verifies the full persisted snapshot remains unchanged.

Executor and independent fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. CMake validated 917 handwritten source owners. ClassMngrScheduleImportTests, ClassMngrNextApplicationScheduleImportReviewDecisionsTests, and ClassMngrScheduleImportDialogTests passed (3/3); git diff --check passed. No full suite was run. The optional external workbook sample was skipped because CLASSMNGR_SCHEDULE_IMPORT_SAMPLE was unset.

F78 adds checked-fixture regression evidence, not historical-output parity. Gate 1 and Gate 2 remain Partial; workspace boundary and audited src/next isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep stays capped at the current and following years, 2026-2027. Protected cmake/sources.cmake remains excluded at SHA-256 9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF. F79 is selected to make Class Transfer weekly schedule intervals validated app-less values, preserving overnight/equal-endpoint and Sunday week-boundary behavior while retaining legacy parsing and diagnostics in the repository adapter.

## Current Phase 2 position - 2026-09-26 (F79)

F79 source/test commit 947edd93 (Validate Class Transfer schedule intervals) makes ClassTransferScheduleCandidate factory-created from a validated category, weekday index, and start/end minute-of-day. It preserves end-at-or-before-start next-day rollover, equal endpoints as 24 hours, Sunday endpoints beyond the weekly boundary, and the repository adapter's legacy parsing and diagnostic text.

Executor verification used a fresh Windows x64 Debug build with CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; configure validated 917 handwritten source owners, 313 build actions succeeded, and both ClassMngrNextApplicationClassTransferTests and ClassMngrClassTransferTests passed. An independent fresh-tree Tester overlaid the exact protected cmake/sources.cmake worktree version, validated the same 917 owners, built both targets, and passed both CTests (2/2). git diff --check passed; no full suite was run. Nonfatal warnings included a missing vswhere.exe locator and unavailable optional Vulkan headers.

F79 adds validated app-less interval behavior to Gate 1; Gate 1 remains Partial. Gate 2 remains Partial without new evidence. Workspace boundary and audited src/next dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep stays capped at the current and following years, 2026-2027. Protected cmake/sources.cmake remains excluded at SHA-256 9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF.

### Phase 2 Korean Teacher Import sparse-update contract - F80 verified

F80 source/test commit `15cd876d` adds a Qt-free Korean Teacher Import update policy. It carries `TeacherId` and `KoreanTeacherKey`, preserves existing Hangul-only matching and imported-name canonicalization, merges nonblank contact fields, retains blank fields, and reports no-op updates. Qt Unicode trimming and SQL identity binding remain in the repository adapter; unrelated profile columns remain outside the policy.

The checked-in `sectioned_review.xlsx` repository and dialog-plan paths now seed suffix-D Hong, update the same row, create Park, and verify persisted identity, imported and preserved values, manual fields, no duplicate, and source date `2026-09-01`. An all-blank repository import verifies unchanged counts and no UPDATE trigger.

Fresh independent Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; the build completed 315 Ninja actions. `ClassMngrNextApplicationKoreanTeacherImportUpdateTests`, `ClassMngrTeacherImportTests`, and `ClassMngrTeacherImportDialogTests` passed 3/3; the dialog target also passed with Windows and offscreen QPA. The repository test had 16 passes and one optional external-workbook skip. `git diff --check` passed; no full suite was run.

F80 advances Gate 1 with app-less merge behavior and Gate 2 with checked-fixture regression evidence, not historical-output parity; both remain Partial. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027. Protected `cmake/sources.cmake` remains excluded at SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.

F81 is selected to extract the Native English Teacher Import sparse-update rule into a Qt-free Application policy and exercise it through the checked-in `sectioned_review.xlsx` fixture path. Preserve current Qt matching, trimming, and name simplification at the adapter; retain native-table row identity in the repository. Cover nonblank replacements, blank-field preservation, unchanged detection, and persisted fixture results. This adds regression evidence, not historical-output parity.

## Current Phase 2 position - 2026-09-26 (F81-F82)

F81 source/test commit `118baceb` extracts Native English Teacher sparse updates into a Qt-free Application policy while keeping Qt normalization and the native table's integer identity at the repository edge. Independent fresh verification passed the app-less update-policy and `ClassMngrTeacherImportTests` CTests (2/2); it used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0, 921 handwritten owners, and 311 build actions. The optional external workbook was unset; no full suite ran. Gate 1 gains app-less behavior evidence and remains Partial; F81's checked-workbook regression is not historical parity.

F82 source/test commit `6d8fb296` adds literal regular Schedule Import expectations derived by running legacy `48fc5c5c` and current `118baceb` against the same seeded database and `schedule_review.xlsx`. All 14 captured semantic outputs match across parser metadata, teacher/class previews, apply counters, and normalized persisted rows. The independent normal `ClassMngrScheduleImportTests` target passed in a fresh archive (CMake 3.30.5, Ninja 1.12.1, MSVC 19.51.36257, Qt 6.12.0; 921 source owners; 309 build actions); `git diff --check` passed. No full suite ran. The workbook was added after the baseline (`f5fdcc4a`), so this is common-input differential evidence, not a historically present fixture. Gate 2 advances but remains Partial; Gate 1 remains Partial. Phase 2 remains open.

F83 is selected to compare the `schedule_overlap_conflict.xlsx` preview/apply rejection and pre-write database snapshot against legacy `48fc5c5c` using the same seed. The existing legacy rollback test and current `previewsAndRejectsCheckedInOverlapWorkbookBeforeWrites` path are the reference points. This fixture also postdates the baseline (`3121d90c`), so the slice will be documented as common-input differential coverage. Sub Prep remains capped at the current and following calendar years, 2026-2027.

## Current Phase 2 position - 2026-09-26 (F83)

F83 source/test commit `2e8bbab2` pins the Schedule Import overlap fixture's teacher keys/names and rooms, preview inventory and unmatched candidates, exact Monday rejection, and normalized no-write state across seven tables. Legacy `48fc5c5c` and current code matched on the same workbook bytes and deterministic seed. Independent fresh verification passed the normal `ClassMngrScheduleImportTests` CTest (1/1) using CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0, 921 source owners, and 309 build actions. The fixture postdates the baseline; this is common-input differential evidence, not historical workbook parity. Gate 2 remains Partial; Gate 1 remains Partial; Phase 2 exit remains Open.

F84 is selected to type Schedule Import's Qt-free candidate and teacher-projection matching keys as `Domain::KoreanTeacherKey`, keep display names separate, and convert only at the repository adapter. Preserve empty-key matching, match ordering, and room aggregation. Verify `ClassMngrNextApplicationScheduleImportMatchingProjectionTests` and `ClassMngrScheduleImportTests`. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-26 (F84)

F84 source commit `5207d65a` types both Schedule Import matching keys as `Domain::KoreanTeacherKey`; display names remain separate and QString conversion stays at the repository edge. The valid empty-key behavior and existing matching and aggregation results are preserved.

Executor and independent fresh Windows x64 Debug verification passed `ClassMngrNextApplicationScheduleImportMatchingProjectionTests` and `ClassMngrScheduleImportTests` (2/2). CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0 validated 921 handwritten owners and 313 build actions; `git diff --check` passed. No full suite ran.

F84 adds typed app-less behavior to Gate 1, which remains Partial. Gate 2 remains Partial without new parity evidence. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027. F85 is selected to extract Teacher Import plan validation into a Qt-free Application policy while retaining Qt normalization, date interpretation, translation, validation order, and exact diagnostics at the repository adapter.

## Current Phase 2 position - 2026-09-26 (F85)

F85 source/test commit `d5971ae1` extracts deterministic Teacher Import plan validation into a Qt-free Application policy. The repository resolves review choices first, projects selected Korean keys, and retains Qt normalization, date interpretation, UTF-8 conversion, and translated error mapping. The policy checks review-key count/order, source-date validity, then Korean, Native English, and GS Team identity rules in the existing order.

Executor and independent fresh-archive verification passed `ClassMngrNextApplicationTeacherImportPlanValidationTests` and `ClassMngrTeacherImportTests` (2/2). The independent Windows x64 Debug Ninja build used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; CMake validated 923 handwritten source owners and Ninja completed 311 actions. `git diff --check` passed. No full suite ran.

F85 adds app-less validation behavior to Gate 1, which remains Partial. Gate 2 remains Partial; no historical-parity claim was added. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

F86 is selected to compare a generated Schedule Import scenario whose helper exists at legacy baseline `48fc5c5c` against current code, using identical generated input bytes and a deterministic database seed. Pin parsed candidates, preview matches, apply counters, and normalized persisted schedule state. This is source-generated synthetic baseline evidence, not historical production-workbook parity.

## Current Phase 2 position - 2026-09-27 (F86)

F86 source/test commit `c3029f14` adds a baseline-era generated Schedule Import regression. Legacy `48fc5c5c` and current `31057c00` consumed the same 5,352-byte workbook (`24cf273f…d48b49`) and produced identical semantic transcripts (`8A00E7A0…F0181DA8`). The test pins parser metadata and candidate values, preview match/suggestion, apply counters, and normalized persisted state. It records Normal import's full-snapshot behavior: unrelated teacher/class metadata and settings remain, while the old regular schedule row is cleared.

Executor and independent fresh-base verification passed `ClassMngrScheduleImportTests` (1/1). The fresh CMake 4.4.2/Ninja 1.13.2/MSVC 19.51.36257/Qt 6.12.0 build validated 923 source owners and 309 build steps; `git diff --check` passed. No full suite ran. This is source-generated synthetic baseline evidence, not historical production-workbook parity.

F86 adds one baseline comparison to Gate 2, which remains Partial; Gate 1 remains Partial. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. F87 is selected to move GS Team sparse merge/no-op behavior into a Qt-free Application policy while preserving repository matching, normalization, diagnostics, and no-op SQL behavior. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F87)

F87 source/test commit `b97f81be` moves GS Team matched-record sparse merge and change detection into `src/next/application/gs_team_import_update.h`. The repository retains Korean-name/English-fallback matching, ambiguity rejection, Qt normalization, SQL, diagnostics, and transaction ownership. The policy preserves the matched ID and blank imported fields and reports whether an update is needed; unchanged rows skip UPDATE.

Executor and independent fresh-base verification passed `ClassMngrNextApplicationGsTeamImportUpdateTests` and `ClassMngrTeacherImportTests` (2/2). The independent CMake 4.4.2/Ninja 1.13.2/MSVC 19.51.36257/Qt 6.12.0 build validated 925 handwritten owners. `git diff --check` passed; no full suite ran. The repository trigger probe observed one UPDATE for the changed row and none for the unchanged match.

F87 adds GS Team sparse merge/no-op behavior to Gate 1, which remains Partial; Gate 2 remains Partial. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. F88 is selected to compare the intensive Schedule Import path using baseline-era generated `singleSheetWorkbookData()` and inline intensive worksheet data from `48fc5c5c`, with identical input bytes and seed on legacy/current. Pin parse, preview, apply, intensive slot/schedule state, and regular/unrelated data; label it source-generated synthetic baseline evidence, not historical production-workbook parity. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F88)

F88 source/test commit `4442726b` adds baseline-era intensive Schedule Import parity coverage to `tests/schedule_import_tests.cpp`. It combines the legacy `singleSheetWorkbookData()` helper with the inline intensive worksheet used by `convertsIntensiveTimesAcrossNoon()`. Legacy `48fc5c5c` and current code consumed identical deterministic workbook bytes (2,359 bytes; SHA-256 `228fc2ce924f2fd4ee340500c92178386868bc83231b076b74e081dd628b93b4`) and produced identical 65-slot state transcripts (SHA-256 `7fdba13a48788441556050e17b6d0b5ca52b2b9df5a6d0c357815a6458f4a5cc`): 62 empty, two essay, and one lunch states. The regression pins parsing, preview matching/suggestion, apply counters, intensive persisted slots/schedules, unchanged regular schedules, unrelated class/teacher rows, metadata, and settings. This is source-generated synthetic baseline evidence, not historical production-workbook parity.

Executor and independent fresh-base verification passed `ClassMngrScheduleImportTests` (1/1). The independent Windows x64 Debug build used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; it configured 925 handwritten source owners and passed CTest in 1.08 seconds. The test-only F88 change passed `git diff --check`; no full suite ran. The protected manifest was excluded from the fresh archive and remains unchanged at SHA-256 `bf3afbe30c77e30be83df98434eda3ee466084b0`.

Gate 2 gains another synthetic baseline comparison but remains Partial; Gate 1 remains Partial. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. F89 is selected to extract only Teacher Import match-count classification (zero, one, or multiple) into a Qt-free Application policy, use it for Korean, Native English, and GS Team imports, and test policy outcomes app-less plus repository ambiguity diagnostics/rejection. Matching, normalization, identity choice, localized errors, SQL, and transactions stay at the repository adapter. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F89)

F89 source/test commit `98968408` adds the Qt-free `TeacherImportMatchCardinality` classifier and uses it in the Korean, Native English, and GS Team repository import branches. The Application policy accepts only a count and classifies 0, 1, or 2+ matches; normalization, candidate order, identity/key selection, localized diagnostics, rejection, SQL, and transaction behavior remain in the repository.

The new app-less target covers counts 0, 1, 2, and 9. Repository tests pin exact ambiguity diagnostics and rollback/no-write behavior for Korean and Native English, plus the existing GS Team diagnostic and rollback behavior. Independent fresh Windows x64 Debug verification overlaid only the six F89 paths on `36b300d2`; the protected `cmake/sources.cmake` blob matched the base. CMake 4.4.2 validated 927 handwritten source owners; Ninja/MSVC 19.51.36257/Qt 6.12.0 built `ClassMngrNextApplicationTeacherImportMatchCardinalityTests` and `ClassMngrTeacherImportTests`, and focused CTest passed 2/2. Nonfatal warnings covered the missing `vswhere.exe` message, optional Vulkan headers, and unrelated long paths. `git diff --check` passed; no full suite ran.

## Current Phase 2 position - 2026-09-27 (F90)

F90 source/test commit `df8ef0cb` adds a Qt-free Teacher Import application use case. It owns review resolution, plan validation, match classification, Korean/Native English/GS Team match and update decisions, result counts, latest-source-date advancement, and transaction decisions. One persistence port keeps reads and all writes within the same transaction. Qt normalization, SQL, localized messages, and the transaction implementation remain in the repository adapter.

Executor focused CTest passed 2/2. Independent fresh-base verification overlaid only six F90 paths on `ca72c47e`, preserved the protected `cmake/sources.cmake` blob, validated 929 source owners, built the focused targets, and passed CTest 3/3 with Windows x64 Debug, CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. The repository tests include failure of the latest-date write after namespace writes and verify full rollback. `git diff --check` passed; no full suite ran.

F90 advances app-less application and orchestration evidence for Gate 1, which remains Partial. Gate 2 remains Partial without new parity evidence. Workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. Two post-F90 audits confirm broader calendar UI, generic settings persistence, other feature-service migrations, and document-service migration remain open. F91 is selected for a bounded source-generated Teacher Import baseline differential against `48fc5c5c`; this adds synthetic plan evidence, not historical production-workbook parity. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F91)

F91 source/test commit `e814b4fc` strengthens `importsIntoSeparateTablesAndPreservesManualFields` with exact per-namespace result counts, all non-ID persisted fields for the Korean, Native English, and GS Team rows, Alex's preserved manual fields and updated position, and the latest-source-date monotonicity check. Its hand-built `TeacherImportPlan` is explicitly source-generated synthetic evidence, not workbook or historical-production-workbook parity.

Executor and independent fresh-tree verification passed `ClassMngrTeacherImportTests` (CTest 1/1). The independent current executable reported 20 passed, 0 failed, and 1 skipped optional external-sample case. An independently built narrow harness against legacy `48fc5c5c` and current F90 repository matched the same semantic output (SHA-256 `755A2B7DF52B3DD8F111ADF2EC0D0127415689B2B08A68D008505079D887B2B6`). Current test patch blob was `786770ca816b40f00c7626461ed58ec0a3e66c22` (SHA-256 `8BB8206A59D27E09E1C82BE918445670B32220F2CB4FD8011041646CD2BA0720`). Windows x64, MSVC 19.51, Ninja, and Qt 6.12.0; `git diff --check` passed. No full suite ran.

F91 adds one baseline-present synthetic plan/repository scenario to Gate 2; both Gate 1 and Gate 2 remain Partial. Workspace boundary and audited `src/next` isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open. Open scope includes broader Calendar contracts/UI, generic settings persistence, remaining feature-service migrations, document-service migration, invalid-UTF-8 coverage, and live MainWindow projection-failure/retranslation integration. F92 is selected to compare the baseline-present source-generated `testWorkbookData()` bytes through Teacher Import validation, explicit M1 review selection, plan construction, and repository apply on baseline/current implementations. This remains synthetic workbook evidence, not historical production-workbook parity. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F92)

F92 source/test commit `3f6ef73d` adds a baseline-present, source-generated Teacher Import workbook parse-to-apply comparison in `tests/teacher_import_tests.cpp`. The existing `testWorkbookData()` helper produced 3,422 bytes with SHA-256 `9cdccb43d7fe5e5e1abb83630ede8b18e6dd2c4824dbb288dc81d60371496daa`. Legacy `48fc5c5c` and current implementations matched the semantic transcript SHA-256 `095d595311aaee2444d67a893d45a2d0f97fe366c2a667f90bdd690205a2bdb4`. The scenario validates and parses the workbook, explicitly selects its sole Korean M1 candidate, creates/applies the plan, and checks import counts, seeded-row updates, preserved manual/unrelated fields, and latest-date state. This is synthetic workbook evidence, not historical production-workbook parity.

Independent fresh focused verification passed `ClassMngrTeacherImportTests` CTest 1/1. The executable reported 21 passed, 0 failed, and 1 skipped optional external-sample case; the selected F92 case passed all 3 assertions. `git diff --check` passed; no full suite ran.

Two independent post-F92 audits agree: Gate 1 and Gate 2 remain Partial; the documented workspace boundary and audited `src/next` dependency isolation remain Satisfied. Phase 2 exit remains Open. F93 is selected to extract Calendar start-of-term classification into a Qt-free Application policy used by the Calendar event summary and upcoming-event paths, preserving simplified/lowercased title matching, unknown-type fallback to `Other`, the hide option, and the exact four recognized phrases. Add app-less policy coverage and focused Calendar verification. Gate 2 rejection parity remains open; F94 is selected next to compare the baseline-present generated Teacher Import invalid-date workbook rejection and pin its result and diagnostic. Validation returns before repository use, so this path does not establish no-write or rollback parity. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F93)

F93 source/test commit `c73f1469` adds `CalendarEventStartOfTermPolicy` as a Qt-free Application contract, routes both `CalendarEventModel` and the Calendar upcoming-event filter through it, and removes the unused duplicate legacy Domain helper. The Calendar Import assertion now exercises the Application policy. App-less coverage pins the four exact aliases, title case/whitespace simplification, known event types and unknown-to-`Other` fallback, nonmatches, the hide switch, and U+0085/NEL behavior for both titles and event types.

Independent fresh Windows x64 Debug Ninja verification based on `c999a235` overlaid the seven F93 paths; the protected source manifest matched the base. CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0 built the two changed Calendar UI translation units, the focused targets, and `ClassMngr.exe` (358 build actions). The three focused Calendar CTests passed 3/3. A direct Qt 6.12 boundary probe matched the old behavior for NEL-separated `new semester` and `Vacation` with trailing NEL. `git diff --check` passed; no full suite ran.

Two independent post-F93 audits agree: Gate 1 and Gate 2 remain Partial; the written workspace criterion remains Satisfied, and audited `src/next` dependency isolation remains Satisfied. Phase 2 exit remains Open. F93 advances one bounded app-less Calendar behavior and does not close broader Calendar UI/contracts, generic settings persistence, remaining feature-service migrations, document-service migration, invalid-UTF-8 coverage, or live MainWindow projection-failure/retranslation integration. F94 is selected to compare the baseline-present source-generated `testWorkbookData("invalid-date")` input on legacy `48fc5c5c` and current code, pinning input bytes, `RecognizedButInvalid`, discovered M1 section, and the exact A1 date diagnostic. This adds negative validation parity only; do not claim repository no-write or rollback parity without exercising that path. Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-27 (F94)

F94 source/test commit `f99d155f636d273269d805531f7ef7db7be84bed` adds `rejectsGeneratedWorkbookWithInvalidDate` to `tests/teacher_import_tests.cpp`. It pins the baseline-present generated workbook at 3,423 bytes, SHA-256 `256b29c2f27bfe787007aaa6df28e5e084dc3788863cbf4b6a09fb265f0685d0`, and asserts `RecognizedButInvalid`, template `sectioned-contact-list-v1`, invalid source date, discovered `M1`, and the exact A1 date diagnostic.

The saved narrow harness compiled each revision's own validator, registry, sectioned template, and workbook reader, using the same input bytes from legacy `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current production sources. Both executables returned identical semantic JSON (SHA-256 `8eee9375c47b3602b86e893092441f450c74860e2bb0f3edc53642a06322cbd9`); I independently reran both archived executables and confirmed equality. The focused current `ClassMngrTeacherImportTests` QTest case passed (3 passed, 0 failed including setup/cleanup); filtered CTest passed 1/1. `git diff --check` passed; no full suite ran. This negative validation path does not establish repository no-write or rollback parity.

F94 adds one negative validation case to Gate 2, which remains Partial; Gate 1 remains Partial. The formal workspace criterion and audited `src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep remains capped at 2026-2027.

F95 source/test commit `7865963815efa256b797b85af27b9a33107e8f70` moves Calendar repeat occurrence planning into a Qt-free Application policy and routes the UI through the existing series-create port. Independent verification built `ClassMngrNextApplicationCalendarEventTests` and `ClassMngr`; focused CTest passed 1/1. Coverage pins daily/weekly cadence, inclusive cutoff, chained monthly clamping, date bounds, duration and copied fields, cleared IDs, invalid input, and 366/367 occurrence bounds. Gate 1 advances but remains Partial; Gate 2 remains Partial. Phase 2 exit remains Open. Sub Prep remains capped at 2026-2027.

F96 adds baseline/current synthetic Teacher Import database rollback parity for commits `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and `7865963815efa256b797b85af27b9a33107e8f70`. Independent fresh builds used each revision's own repository and schema sources; both runs failed at `latest_date_write` and left zero rows in `teachers`, `native_english_teachers`, `gs_team`, and `teacher_import/latest_source_date`. Normalized outputs matched at SHA-256 `b53d70bf33fa0e20451a2582eb8e1a6f904e5907196c6fad4696543b95b230f3`. This adds one synthetic rollback case to Gate 2, which remains Partial; it is not workbook or historical production-data parity. Gate 1 remains Partial; Sub Prep remains capped at 2026-2027.

F97 commit `36ebb09a960fa82f633701ec83fba35bcd7f3599` moves Calendar repeat-series suffix-edit date and field propagation into a Qt-free Application planner and routes the platform adapter through it. App-less tests cover ordered occurrences and IDs, date offset/duration, request fields, trimmed series ID, empty input, invalid dates, and range overflow. Independent verification built `ClassMngrNextApplicationCalendarEventTests`, `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and `ClassMngr`; both focused CTests passed, including a platform-boundary recheck after adding empty-query and no-save failure cases. Gate 1 advances but remains Partial; Gate 2 remains Partial. Phase 2 exit remains Open; Sub Prep remains capped at 2026-2027.

## Current Phase 2 position - 2026-09-28 (F107 verified; F108 selected)

F107 commit `dbdcd721bd68bcaa30191bc93e7576de590fb650` adds a Qt-free Calendar event-by-ID query contract and routes `CalendarPage::handleCalendarEventActivated` through its use case. Application validates bounded IDs and propagates typed results; Platform retains legacy positive-integer parsing, service lookup, and error mapping. The UI still returns before opening the dialog when lookup fails.

Independent fresh Ninja/MSVC VS 2026 x64 / Qt 6.12 Debug verification built `ClassMngrNextApplicationCalendarEventTests`, `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and `ClassMngr`; the two focused CTests passed 2/2. Application tests cover exact forwarding, success/failure results, invalid inputs with no port calls, and the exact length boundary. The application test target uses `QTEST_APPLESS_MAIN`. Platform tests cover legacy lookup and invalid/unavailable IDs. The UI failure path was confirmed by source inspection; these focused tests do not directly assert dialog state. CMake reported potential object-path-length warnings for unrelated targets, but all requested targets built. No full suite ran.

F107 advances Gate 1, which remains Partial; Gate 2 remains Partial with no new parity claim. Workspace create and audited direct `src/next` isolation remain Satisfied; strict transitive ApplicationServices-to-DataService read isolation remains unresolved. F108 is selected to strengthen repeat-series suffix-delete repository state coverage and compare the same seeded fixture against baseline and current source closures. This is repository state-transition parity only. Sub Prep remains bounded to January 1 of the reference date's year through December 31 of the following year, at most. Phase 2 remains In Progress with its exit gate Open.

## Current Position - 2026-09-28 (F109 verified; F110 in progress)

F108 suffix-delete parity passed in commit `d99b226e`. The current focused repository CTest passed 1/1, and the baseline/current harness outputs were byte-identical at SHA-256 `FEC97B681F9D52E7C623AA14E245F3D4BB70FB6315425148632C0E905CC76E08`. The comparison used each revision's own source closure and confirmed five seeded rows became three, retained rows kept all fields, and `sqlite_sequence` stayed at 5. This is synthetic repository state-transition parity only.

F109 commit `26d604fb` adds a seeded repeat-series batch-creation fixture. Independent fresh-archive verification passed the focused `ClassMngrCalendarEventRepositoryTests` CTest 1/1. A separate harness compiled each revision's own repository/schema/source closure with identical seed and explicit occurrence facts. Baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current production revision `d99b226e19917f3c855a0c49fa7891c537c4415d` returned IDs `[2,3,4]`; all persisted columns, the unrelated row, count, and `sqlite_sequence` matched. Normalized output SHA-256: `DB5EC63C24300360F3639131C501D9AC65A9867942D5A14CCCBA3BAAD7420E59`. This is synthetic repository batch parity, with no UI or historical-workbook claim.

F110 is in progress: move the Calendar per-event visibility composition (start-of-term hiding followed by campus visibility) into a Qt-free Application policy used by both the Calendar event model and upcoming-event filtering. Qt normalization and preference/directory reads stay at the feature boundary; active-type filtering remains ahead of this decision. Tests must preserve hide/show-all precedence, current/non-current/missing-campus behavior, unmatched titles, and input order.

Gate 1 and Gate 2 remain Partial; the formal workspace-create boundary and audited direct `src/next` isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` read isolation remains open. Phase 2 exit remains Open. Sub Prep remains bounded to the reference date's year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F110 verified; F111 selected)

F110 commit `2656ef9c` extracts Calendar's per-event visibility composition into a Qt-free Application policy used by the event model and upcoming-event filter. Independent fresh-archive verification built the Application Calendar tests, Calendar cache tests, and `ClassMngr`; the two focused CTests passed 2/2. Twelve app-less cases cover start-of-term precedence, `Other` and unknown event types, lazy campus evaluation, show-all, missing metadata, unmatched titles, current/non-current and mixed campus tokens. The new header has no Qt dependencies. Both callers preserve input order, the upcoming type filter stays first, and the Qt normalization adapter remains. The CalendarPage path compiled but has no direct runtime visibility assertion; no full suite ran.

F111 is selected: make non-null-session `SettingsService` operations authoritative for load/save/saveAll, while preserving DataService-only legacy construction. Verify that a closed bound session does not fall through to a separately open DataService, that the legacy-only constructor still reads/writes, and that normal ApplicationServices preferences still work. This removes the settings fallback from session-bound callers only; Calendar, Teacher, Class, Schedule, Roster, and other service-family fallbacks remain under audit.

Gate 1 and Gate 2 remain Partial; the workspace-create boundary and audited direct `src/next` isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` read isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to the reference date's year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F111 verified; F112 under mapping)

F111 commit `3152ce36` makes a bound-session `SettingsService` authoritative for load/save/saveAll and preserves the sessionless DataService-only path. Fresh independent Windows x64 Debug verification overlaid only `feature_services.cpp` and `data_service_lifecycle_tests.cpp` on base `5653bf8c`; `ClassMngrDataServiceLifecycleTests` and `ClassMngrNextPlatformApplicationServicesCurrentCampusPreferencesPortTests` passed 2/2. The test used a closed bound session and separate open DataService, verified no fallback read/write or mutation, and verified legacy-only access still works. This reduces the settings-service transitive read edge only; other service families remain.

The separate open-DataService test observes `SettingsService::isAvailable()` as true while bound-session operations fail. This reflects the existing availability check, which still considers DataService open state; it is recorded as an availability limitation, not a content-read path. No full suite or baseline parity run.

F112 is under mapping: isolate CalendarService content reads used by Next Platform adapters (by-ID, range, date intervals, repeat series), retaining legacy DataService-only behavior. Scope and focused acceptance are pending the current call-site/test-seam review.

Gate 1 and Gate 2 remain Partial; workspace create and audited direct `src/next` isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to the reference date's year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F112 verified; F113 selected)

F112 commit `677f2451` makes all six CalendarService content reads session-bound when a session is present: events-for-date, range, date intervals, upcoming events, by-ID, and repeat-series. Independent fresh-archive Windows x64 Debug verification passed the lifecycle and Next Platform Calendar adapter CTests 2/2 using CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, and Qt 6.12.0. The closed-session/separate-open-DataService test proves all six reads fail closed while legacy-only construction still reads. The adapter target first hit an MSVC generated-file error on a longer temporary path; the short-path fresh retry passed. No full suite ran, and this slice does not establish global DataService isolation. Calendar writes/deletes remain outside scope.

F113 is selected to remove bound-session DataService fallbacks from the five TeacherService, ClassService, and RosterService reads used by Sub Prep print and roster-output source ports. Preserve sessionless legacy reads, the schedule-scope class-info path that is already session-only, and the print adapter's zero student-count fallback. Focused acceptance covers the closed-session/other-open-DataService contrast and both normal Sub Prep output-port tests.

Gate 1 and Gate 2 remain Partial; workspace create and audited direct `src/next` dependency isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` read isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to January 1 of the reference year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F113 verified; F114 selected)

F113 commit `e9ef19a0` makes five Sub Prep output reads session-authoritative: TeacherService teacher lookup, ClassService classroom and class-info reads, RosterService student count, and bounded roster output. A non-null closed session now fails these reads without falling through to a separately open DataService; sessionless DataService-only construction remains functional. The lifecycle regression also confirms the dual-bound services still report available, proving the operations themselves fail closed.

Independent fresh-archive verification used base `677f24518c41ade1fa1b7769375e9b8561f1ed25` and overlaid only `feature_services.cpp` and `data_service_lifecycle_tests.cpp`. The three focused CTests passed 3/3: `ClassMngrDataServiceLifecycleTests`, `ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and `ClassMngrNextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests`, on Windows x64 Debug with Ninja, MSVC 19.51, and Qt 6.12.0. The existing zero student-count print behavior remains covered. `git diff --check` passed. CMake emitted a non-fatal `vswhere.exe` warning; the isolated build and tests succeeded. The verifier removed its scratch files. No full suite ran.

The initial F114 candidate was to isolate TeacherService catalog reads, but the dependency audit found no current `src/next` caller for those methods. F114 was redirected to active Calendar mutation paths and bound-session availability; the completed slice is recorded below. Other Class, Schedule, Roster, Speaking Evaluation, document, and broader Calendar UI/contract work remains open.

Gate 1 and Gate 2 remain Partial; workspace create and audited direct `src/next` dependency isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to January 1 of the reference year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F115 verified; exit-gate audit next)

F115 source/test commit `96c8b8a5` makes `ClassService::saveClassNotes()` authoritative to a non-null session. Direct service coverage proves a closed bound session cannot save through a separately open, seeded DataService and leaves both notes fields unchanged; a DataService-only service still updates both fields. F114's session-authoritative availability guard also makes the active ClassNotes adapter fail closed before calling the service.

Independent fresh-snapshot verification overlaid only `feature_services.cpp` and `data_service_lifecycle_tests.cpp` on `0c3fdd61`. Windows x64 Debug, CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, and Qt 6.12.0 built and passed the registered lifecycle, ClassNotes save-port, and ClassNotes page CTests 3/3. `git diff --check` passed. The broader suite was not run; the initial preset configure was incompatible with Ninja's platform setting, and the direct Ninja configure succeeded.

Next, independently re-audit the active `src/next` -> `ApplicationServices` service paths against the literal dependency gate, keeping method-level runtime isolation distinct from the retained legacy `DataService*` compatibility edge. Refresh the Gate 1 and Gate 2 evidence map at the same time. Existing bounded Gate 2 comparisons include F82/F83 Schedule Import fixtures, F86/F88 source-generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import cases, and F100/F101/F103/F104/F106/F108/F109 rollback and repository transitions. They do not cover all validation, conflict, planning, and state-transition behavior; historical production-workbook provenance remains a tracked non-gating risk.

Gate 1 and Gate 2 remain Partial; workspace create and audited direct `src/next` dependency isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to January 1 of the reference year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F114 verified; F115 selected)

F114 source/test commit `8aee10a6` makes a bound `DatabaseSession` authoritative for `FeatureService::isAvailable()` and for CalendarService `saveEvents`, `deleteEvent`, `deleteRepeatSeriesFromDate`, and `deleteAllEvents`. A closed bound session rejects each operation without changing the separately open seeded DataService; lifecycle assertions check the legacy state immediately after each rejected call. Sessionless DataService-only behavior remains covered. Singular `saveEvent()` is unchanged because no current `src/next` caller was found.

Independent fresh-snapshot verification overlaid only `feature_services.cpp` and `data_service_lifecycle_tests.cpp` on `2fe91485`. With Windows x64 Debug, Ninja 1.13.2, MSVC 19.51, and Qt 6.12.0, `ClassMngrDataServiceLifecycleTests` and `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests` passed 2/2; `git diff --check` passed. The same code snapshot also passed `ClassMngrSharedPolicyTests` 1/1 before the test-only assertion refinement. No full suite ran; optional Vulkan headers were unavailable and did not affect verification.

F115 is selected to make `ClassService::saveClassNotes()` itself session-authoritative when a non-null session is bound. Direct service coverage will prove a closed session plus a separately open seeded DataService fails without changing either notes field, and a DataService-only service still saves. F114's session-authoritative availability already makes the ClassNotes adapter reject a closed bound session before calling the service; the F115 slice closes the service method's remaining latent fallback and retains the port/page regressions.

Gate 1 and Gate 2 remain Partial; workspace create and audited direct `src/next` dependency isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to January 1 of the reference year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F116 verified; F117 selected)

F116 source commit `6636cac1` removes `ApplicationServices::hasOpenDatabase()` from the active Sub Prep roster-output source port. Its existing null and session-authoritative availability checks remain. Independent fresh-snapshot verification overlaid only `src/next/platform/application_services_sub_prep_roster_output_source_port.h` (SHA-256 `C92559472DA7F1E6758BD74341CD4EBF3439E105A4A91361EAD276A9CAC39ABD`) on `85fa1e2e`. Windows x64 Debug with CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0 built and passed `ClassMngrDataServiceLifecycleTests` and `ClassMngrNextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests` 2/2 with `--no-tests=error`; `git diff --check` passed. The lifecycle test verifies that closed-session-bound services reject reads while a separate legacy DataService is open and that legacy-only services still work. The port target has no direct split-session injection seam; no full suite ran. The verifier retried from a short temporary path after an MSVC generated-`.moc` path-length failure in its first isolated build.

Two independent Gate 1 audits found broad existing app-less coverage across typed values, workspace, Calendar, Schedule/import, Class Transfer, document, job, and Sub Prep contracts, but the planned application surface remains incomplete. Teacher-profile editing is still composed and validated in `TeacherInfoPage`; F117 is selected to move that operation behind a deterministic app-less Application contract while preserving UI feedback and dirty-state behavior. Other planned editing, backup/recovery, and legacy-import use cases remain gaps. Gate 1 stays Partial.

The isolation re-audit confirmed the audited direct `src/next` source scan is clean, while v2 service factories still construct dual-bound feature services and the Workspace adapter still calls DataService-backed lifecycle operations. F116 removes only the extra roster-output open-state call. Workspace-create acceptance remains Satisfied; strict transitive `ApplicationServices`-to-`DataService` isolation remains unresolved. Gate 2 remains Partial; its current evidence map includes F82/F83, F86/F88, F91/F92/F94/F96/F98, and F100/F101/F103/F104/F106/F108/F109, but does not cover all validation, conflict, planning, and state-transition behavior. Historical production-workbook provenance is tracked as non-gating risk.

Phase 2 exit remains Open. Continue F117 after a bounded design review, and keep the independent session ownership, Workspace, and session-only v2 service work visible as separate gate blockers. Sub Prep remains bounded to January 1 of the reference date's year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F117 verified; F118 selected)

F117 source/test commit `3fd2b93f` adds an app-less Teacher Profile Edit domain/application contract: checked teacher identity and 14 profile fields, injected semantic validation returning normalized fields and structured field-addressable issues, and separate update/reload persistence operations. It short-circuits invalid IDs, blocks only error-severity issues, passes normalized values to persistence, distinguishes update from post-write reload failure, and returns the canonical reloaded profile.

Independent fresh-archive verification used base `47844dfc087d47da9426e0aa06d948a1ab2264a9` with exactly the five committed paths and hash-checked overlays. A forced clean MSVC/Ninja build passed; `ClassMngrNextApplicationTeacherProfileEditTests` passed 1/1 with `--no-tests=error`. Static include/link inspection found no Qt or legacy production dependency in the new contracts. `git diff --check` passed. MSVC warning C4530 remains non-fatal because `/EHsc` is not set on this test target. The production policy adapter and `TeacherInfoPage` integration are future work; the adapter must use existing `TeacherValidator` semantics.

F118 is selected for common-input baseline/current comparison of the Class Transfer conflict scenario using `tests/fixtures/transfers/conflict_source.json`. This fixture was added after the legacy baseline, so evidence must be described as a checked-in post-baseline common-input comparison, not historical production-workbook parity. Gate 2 remains Partial pending this and broader coverage. Gate 1 remains Partial; F117 adds app-less coverage but does not complete the planned application surface.

Workspace create and audited direct `src/next` source isolation remain Satisfied. Strict transitive `ApplicationServices`-to-`DataService` read isolation remains unresolved. Phase 2 exit remains Open. Sub Prep remains bounded to January 1 of the reference date's year through December 31 of the following year at most.

## Current Position - 2026-09-28 (F119 verified; F120 selected)

F118 source/test commit `b68eba6d` adds a Class Transfer conflict case over the unchanged `conflict_source.json` fixture. It pins the fixture bytes, preview match results, review choices and plan, the exact combined regular/intensive schedule-collision diagnostic, all application-table snapshots plus `sqlite_sequence`, and a zero `total_changes()` delta under SQLite query-only.

Independent verification extracted current commit `5263aebf8222e16e3085498af851a7f0d3041818` and overlaid only the F118 test file. A separate baseline archive at `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` received the same fixture bytes and only the F118 test helpers/case. Focused `ClassMngrClassTransferTests` passed 1/1 on both source closures; `git diff --check` passed. The baseline's temporary CMake files needed three Qt minimum references adjusted from 6.11.1 to the installed 6.12.0. The fixture was added after baseline, so this is common-input evidence on a checked-in post-baseline fixture, not historical production-workbook parity. No full suite ran.

F119 source/test commit `80fbf034` implements canonical `DatabaseSession` ownership in `ApplicationServices`, a borrowing `DataService` compatibility facade with the standalone owning constructor preserved, and session-only construction in all seven ApplicationServices feature-service factories. Independent fresh-archive verification confirmed the exact five committed paths and corrected SHA-256 manifest, built `ClassMngr` plus six focused targets on Windows x64 Debug with CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257.0, and Qt 6.12.0, and passed all six selected CTests. `git diff --check` passed. No full suite ran. The explicit Workspace port lifecycle/save/save-as/export edge remains a separate follow-on.

F120 is selected as one gate-closing slice after three independent solution reviews. Keep the existing `ApplicationServices` workspace API and route its seven operation implementations to `DatabaseSession` and a shared DataService-independent file-copy helper. Replace `DataService`'s stale repository-pointer cache with live session-backed access. Preserve the Workspace API and controller composition. Acceptance covers a source audit showing no `m_dataService` operation call, all seven Workspace behaviors, and facade validity after first open, successful database replacement, failed replacement, close, and reopen. Focused tests target the lifecycle, Workspace port, and FileController lifecycle suites with a `ClassMngr` build.

Gate 1 and Gate 2 remain Partial; F118 adds one post-baseline common-input conflict comparison to Gate 2. Workspace create and audited direct `src/next` source isolation remain Satisfied. F119 removes the dual-bound feature-service factory edge, but strict transitive isolation remains unresolved at the Workspace path. Phase 2 exit remains Open.

## Current Position - 2026-09-28 (F122 verified; F123 candidate pending review)

F122 source commit `e3411e733d2ce402a03096e715150e45b4244dff` adds ordered
multi-meeting and final-state parity assertions to
`ScheduleImportTests::skippedExactMatchPreservesItsSchedule`. An independent
fresh-archive Tester verified the exact commit and pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`; both focused cases passed 3/3.
Both produced final snapshot SHA-256
`08ad64ed3d853e52a1a686c1683d0d1fe8a289026e21d21081e09ee4840c5ebe`, including
`class_times.id` and `sqlite_sequence`, and retained Tuesday-before-Monday
consumer order. Current used Windows x64, MSVC 19.51.36257.0, Ninja 1.13.2,
and Qt 6.12.0; the baseline used Qt 6.11.1. No full suite ran. This is
baseline-present hand-authored seed evidence, not historical workbook parity.

F123's candidate is to integrate the existing-teacher edit use case from F117
with the production `TeacherInfoPage` flow, preserving `TeacherValidator`
semantics, the active session-backed update/reload path, validation feedback,
and save lifecycle. Two independent Explorers agreed on this gap, but the
bounded solution review is pending; no F123 implementation has begun.

Gate 1 and Gate 2 remain Partial. The formal workspace-create boundary and
active-v2 DataService isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. The user's requested session handoff records this
continuation point in `latest_session_work.md`.

## Current Position - 2026-09-29 (F124 verified; F125 review next)

F124 source/test commit `a159591e48e312f96378302b105a3ad1276382b3` integrates
`ClassCoTeacherPage` with the app-less `ClassCoTeacherAssignmentUseCase`. The
use case rejects invalid IDs and maps the legacy `-1` sentinel to an absent
teacher ID. Its session-backed platform adapter reads the current `ClassInfo`,
changes only the teacher assignment, and saves through `ClassService`, keeping
the other class fields, notes, and schedules. The page retains manual warning,
dirty-state, title-refresh, and `classInfoSaved` behavior.

The `windows-x64-debug` preset built `ClassMngr`, the application test, the
platform adapter test, and the page integration test; their focused CTests
passed 3/3. Coverage includes invalid IDs, assigned/unassigned values,
persisted-field and schedule preservation, unavailable service handling, page
title/signal updates, and warning/dirty-state behavior. `git diff --check`
passed. The existing `ClassMngrClassesPageTests` target built but failed 22
test cases, including `page.openClass(42, ...)` and widget validation assertions;
the first Details failure occurs before the co-teacher editor opens. It also
emitted duplicate-stub `/FORCE` linker warnings and a stale dependency warning
for `class_navigation_preferences.h`. No baseline comparison or full suite ran.

F125's candidate is routing `ClassNotesPage` through an app-less
`ClassNotesSaveUseCase`, applying the request's existing text limit before
calling the session-backed save port. Preserve trimmed fields and current
warning/dirty behavior. Review and implementation have not begun. Gate 1 and
Gate 2 remain Partial; formal workspace-create acceptance and active-v2
DataService isolation remain Satisfied. Phase 2 remains In Progress/Open.

## Current Position - 2026-09-29 (F136 verified; F137 candidate review interrupted)

F126 source/test commit `ca4c1a97` (`Phase2 - integrate class details save
use case`) adds the Qt-free `ClassDetailsSaveUseCase` and session-backed
platform adapter, and routes `ClassDetailsPage` saves through that boundary.
The page retains normalized `ClassInfoValidator` feedback and separate regular
and intensive schedule-conflict checks. The adapter changes only requested
editable fields and preserves teacher assignment, notes, and time-filler
activities. The schedule conversion handles validated `h:mm AP` values,
including AM/PM, noon, and midnight.

The three focused F126 CTest targets passed 3/3. The production target and
focused targets built in the implementation run; the independent build was
up to date. A fresh independent rebuild could not be configured because Ninja
could not find `rc` and the Visual Studio generator could not find a C++
compiler. `git diff --check` passed. No full suite or baseline comparison ran.

F127 source/test commit `988b7de7` (`Phase2 - integrate class details page
read query`) adds a Qt-free typed-ID snapshot query for editor fields, raw
ordered schedule rows, teacher display data, and student count. The session
adapter retains independent source outcomes; the page preserves separate
fallbacks and refreshes the snapshot for load/reload, title retranslation, and
after save. The fresh save-validation read remains separate.

Four focused F127 CTest targets passed 4/4, including the title-refresh
regression and F126 save-page test. `ClassMngr` and the focused targets rebuilt
successfully with the Visual Studio developer environment; `git diff --check`
passed. No full suite or baseline comparison ran. Malformed schedule values
are tested as lossless through the app/platform boundary; malformed-value
widget rendering remains outside scope.

F128 source/test commit `93ad5afb` (`Phase2 - integrate schedule slot state
save use case`) adds a Qt-free typed command and session-backed adapter for
the shared schedule toggle. The UI still chooses the next/default state; the
adapter preserves override deletion when a slot returns to its default. The
ordinary toggle path remains shared by regular and intensive views, with the
testing-assignment path unchanged.

The app, platform, schedule-widget, and testing-classes focused CTest targets
passed 4/4. Coverage checks invalid requests, session persistence/default
deletion, regular and intensive weekday/time mapping, warning/reload behavior,
unavailable-service fallback, and the separate testing-assignment path.
`ClassMngr` built and `git diff --check` passed. No full suite or baseline
comparison ran.

F129 source/test commit `a0e3c98a` (`Phase2 - integrate roster save use case`)
adds a Qt-free single-class roster snapshot request and a session-backed
platform adapter. It preserves ordered columns, widths, all 25 row positions,
UTF-16 values, and the questionable Korean-name confirmation flag. The widget
keeps validation/focus, interactive confirmation, autosave/manual timing,
dirty-state and warning behavior, and class-selection save gating.

The application, platform, and page CTest selection passed 3/3; the existing
roster-import widget regression passed, and `ClassMngr` built. Independent
verification rebuilt the focused roster-save widget target and passed 1/1,
including direct assertions for invalid-cell focus, silent autosave failure,
and rollback of a testing-class selection after save failure. Whitespace
validation passed. No full suite or baseline comparison ran.

F130 source/test commit `70f3ddc9` (`Phase2 - integrate speaking evaluation
save use case`) adds a Qt-free typed save request and a session-backed adapter
for one speaking evaluation. It carries the ordered 25-by-11 UTF-16 matrix,
exact changed-cell coordinates, evaluation name, and questionable-name flag;
an empty changed-cell list retains the existing write-all meaning. The page
keeps validation/focus, confirmation, notices, dirty-baseline updates,
autosave/manual timing, and class/evaluation selection rollback.

The app, platform, and page focused CTests passed 3/3; `ClassMngr` built.
Independent verification reran the platform target after adding a discriminating
non-empty-delta case and passed 1/1. The case confirms that a changed value
outside the delta stays unwritten while the listed cell is persisted.
`git diff --check` passed. No full suite or baseline comparison ran.

F131 source/test commit `d0828493` (`Phase2 - integrate speaking evaluation
read query`) adds a Qt-free typed-ID query and a session-backed read adapter
for one selected evaluation. It preserves the exact evaluation-name key and
ordered UTF-16 rows. `SpeakingEvalPage` reads through the query while keeping
the existing model normalization and blank 25-by-11 clean state for empty or
failed reads.

The app, platform, and page focused CTests passed 3/3; `ClassMngr` built.
Independent verification confirmed exact-name behavior (`" Winter "` does
not match `"Winter"`), ordered Unicode rows, structured read failures, and
empty/failed page fallbacks. `git diff --check` passed. Partial/malformed-row
normalization at the page is source-reviewed but has no direct load-path test.
No full suite or baseline comparison ran.

F132 source/test commit `e878906c` (`Phase2 - integrate roster read query`)
adds a shared Qt-free `RosterSnapshot`, typed class-ID read query, and
session-backed adapter, then routes `RosterEditorWidget::loadClass` through
it. The app/platform boundary preserves ordered columns, widths, all raw rows,
and UTF-16 values without applying the UI's 25-row cap. The existing model
continues to normalize columns, names, widths, and displayed rows; read errors
still produce a blank, clean roster.

The app, platform, and page focused CTests passed 3/3; `ClassMngr` built.
Independent verification confirmed a 38-row raw snapshot, missing-roster and
repository-error distinction, session-only access, page normalization and
widths, 25-row display, blank/failed fallback, clean state, and capability
signals. `git diff --check` passed. There is no single page test loading 38
rows end-to-end; the raw and displayed limits are covered separately. No full
suite or baseline comparison ran.

F133 source/test commit `b111b799` (`Phase2 - integrate classes navigation
snapshot`) adds a Qt-free typed-ID navigation snapshot and active-session
adapter. The repository loads class metadata and teacher labels with one join,
then loads regular and intensive schedule rows in two ordered batch queries.
`ClassesPage` retains its tab model, filtering, selection, and refresh
behavior; a read failure retains class names with blank metadata.

`ClassMngr` and four focused test targets built; the app, platform, ClassesPage,
and navigation-model CTests passed 4/4. Independent verification confirmed
the three-query batch, missing-row fallbacks, ordered raw values, filtering,
selection, and class-info-save refresh. Verification used Ninja/MSVC after the
Visual Studio generator hit a FileTracker access-denied error.
`git diff --check` passed. No full suite or baseline comparison ran.

F134 source/test commit `cbb15e32` (`Phase2 - integrate schedule builder
source query`) adds a Qt-free schedule-source snapshot and active-session
Platform adapter, then keeps parsing and row construction in the service-free
builder. It reuses the repository's ordered batch read; this advances the
application boundary rather than reducing SQL query count. Raw day/start/end
strings and parser behavior remain intact.

The app, Platform adapter, builder, and ScheduleWidget focused CTests passed
4/4 in an independent fresh Ninja/MSVC build. The Platform fixture now inserts
classes in an order distinct from repository order and asserts the sorted
snapshot IDs and exact intensive end time. `git diff --check` passed. No full
suite or baseline comparison ran. After preview installation,
`ScheduleWidget` bypasses source, slot-state, and testing-assignment reads;
`ScheduleImportReviewDialog::buildUi()` also calls two setters that render the
live schedule before `prepare()` performs its normal preference refresh and
installs the preview. With services available, setup can perform three
pre-preview reads. This call order is present at the F134 parent; later preview
renders and refreshes bypass those reads.

F135 source/test commit `91806e5d` (`Phase2 - integrate schedule slot-state
read query`) adds a Qt-free query and active-session Platform adapter for
intensive slot-state overrides. The adapter reads the `DatabaseSession`
repository directly, without `ScheduleService` or `DataService` fallback, and
preserves ordered raw day/start/state strings. The widget retains overrides on
unavailable or failed reads, warns on failure, and replaces the full map on
success, including clearing it for an empty result.

Independent fresh Windows x64 Debug Ninja/MSVC verification built `ClassMngr`
and the app, Platform, and ScheduleWidget targets. The three focused CTests
passed 3/3, including an assertion for the existing warning prefix.
`git diff --check` passed. No full suite or baseline comparison ran.

F136 source/test commit `0295543a` moves the ScheduleWidget testing-assignment
display read through a Qt-free Application query and active-session Platform
adapter. One ordered joined repository read supplies assignments and optional
special-class display fields, replacing per-assignment service reads. The UI
preserves plain/special rendering, unavailable clearing, warning and prior
state retention on assignment-read failure, missing-special warning and skip,
blank/default class-info behavior, and preview bypass.

Independent fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 verification
built `ClassMngr` and the Application, Platform, and ScheduleWidget targets.
The three focused CTests passed 3/3:
`ClassMngrNextApplicationScheduleTestingAssignmentReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesScheduleTestingAssignmentReadPortTests`,
and `ClassMngrScheduleWidgetTests`. The Platform test confirms one recorded
statement at both one and 41 assignments. `git diff --check` passed. No full
suite or baseline comparison ran.

F137 candidate review began from this commit; two Explorer lanes were started
but interrupted at the user's request before reports arrived. No next slice
has been selected or implemented. Gate 1 and Gate 2 remain Partial; formal
workspace-create acceptance and active-v2 DataService isolation remain
Satisfied. Phase 2 remains In Progress/Open.

## Current handoff — 2026-09-29

Phase 2 F144 teacher-choice read is accepted. Its production checkpoint is
56c76f412b246230fcfe00c249b195dcc6ccd95f; the acceptance test commit is
89fbbaa250ddf98fae2ab1d80385fb99164ac055. The focused F144 Application,
Platform, and TestingClassesPage CTests passed 3/3. The F142/F143 Application
and Platform regression CTests passed 4/4 in a separate fresh short-path
Ninja build. All seven distinct required targets passed. Configure-time source
ownership validated 1,025 files. No full suite or full application build ran.

F144's teacher-choice read is accepted; the earlier Start Here ambiguity is
resolved in favor of F144. F145 has since been selected and accepted as the
existing Testing Class details update slice. F146 has since been accepted as
the Testing Class creation boundary with its optional pending schedule
assignment. The next milestone is a Heavy-route candidate review for F147. No
push was requested.

## macOS Qt checksum recovery — 2026-09-30

The macOS release and refactoring-baseline jobs both installed Qt 6.12.0 with aqt 3.3.0 and had no delayed install retry. The reported `ChecksumDownloadFailure` means aqt could not retrieve the qtbase SHA-256 sidecar; the exact HTTP response is unavailable, so a transient network or sidecar publication gap is plausible but not confirmed. The archive filename alone does not establish an aqt metadata parsing defect.

Both macOS install blocks now use a 30-second request timeout and retry the complete aqt install up to three times, with a 10-second delay between failed attempts. They preserve the existing Qt version, `clang_64` target, qtpdf module, output paths, and aqt pin. Checksum verification stays enabled, and the jobs still fail after the final unsuccessful attempt.

`git diff --check`, Ruby YAML parsing of both workflows, and `bash -n` on both edited run blocks passed; an independent review confirmed the control flow and retained settings. No live Qt install or GitHub Actions run was performed. The underlying sidecar response remains unobserved; a permanent missing-sidecar condition will still fail closed after retries.

## F145 acceptance — 2026-09-30

F145 routes existing Testing Class details updates through a Qt-free
Application use case and active-session Platform adapter. New-class creation,
pending schedule assignment, and deletion cascades remain separate. The page
preserves roster-first saves: a roster failure blocks the detail update, while
a later detail failure leaves the successful roster save persisted and clean.

Production commit `26a916df9994217ffd3f12f45148207e9cf5e0c2` and acceptance
test commit `ba1b7cdec15f6f163bb1620897fb4c2e2b3baccb` are accepted. A fresh
Windows x64 Debug Ninja/MSVC configure and focused build passed; all three
F145 CTests and seven F142–F144 regressions passed (10/10). CMake source
ownership validation ran. `git diff --check` passed. No full suite or full
application build ran. F146 is recorded below; Phase 2 exit gates remain open.

## F146 acceptance — 2026-09-30

F146 routes new Testing Class saves through a Qt-free Application use case and
active-session Platform adapter. Optional weekday/start-time assignment is
passed to the same repository create operation, preserving its transaction.
The new-class page path, F145 update path, and delete path remain distinct.

Production commit `d7acb516cd9261e2199f742b46f90de48aa907d1` and acceptance
test commit `14d2d124a3d8b0548d241f1d2dcea136dbee55d9` are accepted. A fresh
Windows x64 Debug Ninja/MSVC build passed all 14 focused F146, repository, and
F142–F145 regression CTests. The repository rollback test verifies no class,
details, room, or assignment rows remain after a conflicting slot create.
`git diff --check` passed. No full suite or full application build ran. The
next step is a Heavy-route candidate review for F147; Phase 2 gates remain
open.

## F147 acceptance — 2026-09-30

F147 routes Testing Class deletion through a Qt-free Application use case and
active-session Platform adapter while retaining the repository's transactional
cascade. The confirmation now discloses the roster, notes, speaking
evaluations, regular and intensive class times, and schedule assignments.
After successful deletion, the page clears the deleted class's dirty editor
and roster state before selecting a sibling, preventing an accidental save or
create during the selection transition.

Production commit `b037b4216b71c55c7793df5f7bbbfc4a00690065`, transition fix
`315b3ff33b7e2ab42b43d52cd168ce21a92158c9`, and test commit
`397376e439f4b5955c82948ab0c225aaf776d679` are accepted. Fresh Windows x64
Debug Ninja/MSVC verification passed all 17 F147, repository, and F142–F146
regression CTests. Cascade success preserves sibling rows; a final-delete
trigger verifies rollback. `git diff --check` passed. No full 220-test suite or
full application build ran. The next step is a Heavy-route candidate review
for F148; Phase 2 gates remain open.

## F148 class-details save parity — 2026-09-30

F148 adds a common-input success case for `ClassDetailsPage` saving through
the real page and active services. Commit `6c7211d6427b6dbcddd4d109d9d09f9eeff14f28`
adds the test and its CMake registration. The same seeded save passed on a
fresh current archive and pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`.
The assertions cover persisted detail fields, preserved teacher/notes/
activities, regular and intensive schedule order, the saved signal, and clean
page state. Gate 2 gains one successful-save comparison but remains Partial;
the case does not establish validation or conflict parity. Phase 2 remains
In Progress/Open.

## F149 Class Details schedule-conflict query — 2026-09-30

F149 adds a Qt-free typed conflict query and active-session Platform adapter
for the Class Details page. The adapter reads
`DatabaseSession::classInfoRepository()->getClassTimeConflicts()` directly;
warning rendering stays in the page, overlap/order stay in persistence, and
the existing save-time `ClassService` checks remain.

Commit `2e7d8866` (`Phase2 - route class details conflict checks through typed
query`) passed the fresh focused build and CTests 4/4; the pinned baseline
parity CTest passed 1/1 for F148 success plus F149 regular/intensive conflict
cases. No full suite or application build ran. Gate 1 and Gate 2 remain
Partial, and Phase 2 remains In Progress/Open.

## F150 selection record — 2026-09-30

F150 moves Class Details pre-save normalization and validation behind a
Qt-free typed Domain/Application policy, then maps structured issues back to
the existing page field feedback. Keep `ClassService` save-time validation
and F149 conflict preflight unchanged. Preserve book-catalog rules, malformed
schedule diagnostics, duplicate-schedule semantics, hidden-field checks, and
stable issue order. Make legacy unordered duplicate-group diagnostics
deterministic in first-seen order. See the acceptance record below; Phase 2
gates remain Partial/Open.

## F150 Class Details validation policy — 2026-09-30

Commit `854f9849` moves page pre-save normalization and validation into a
Qt-free typed policy. The page adapts the live `ClassInfoConfig` catalog,
retains malformed schedule row text, maps structured issues to existing field
feedback, blocks conflicts/saves for invalid input, and saves normalized
values. `ClassService` save-time validation and F149 conflict ordering remain.

Fresh Windows x64 Debug Ninja/MSVC verification (CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257, Qt 6.12.0) validated 1,047 handwritten source owners. The
policy, page validation, parity, and shared-policy targets passed CTest 4/4.
The pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity target
passed 1/1 with only the test source/registration and three Qt minimum bumps
overlaid; no production source was changed. Parity covers F148 successful
save, F149 regular/intensive conflicts, and F150 invalid save. A regression
also checks trimming of hidden persisted notes and activity text. Representative
page field/focus mappings are tested; mapping every field directly through the
page remains a coverage limitation. No full suite or application build ran.
Gate 1 and Gate 2 remain Partial; Phase 2 remains In Progress/Open. F151 is
selected to add live-page baseline parity for malformed regular/intensive
schedule validation, end-before-start, and duplicate rows. Compare duplicate
membership and row-specific feedback semantically because the legacy
cross-group `QHash` order is unspecified. A typed Application read for hidden
persisted validation fields remains a separate subsequent candidate.

## F151 Class Details invalid-schedule parity — 2026-09-30

Commit `f232301e` adds four live-page common-input parity cases for malformed
regular schedules, malformed intensive schedules, end-before-start, and
duplicate rows. The cases assert field/row/column feedback, dirty-state
retention, no visible conflict warning, no saved signal, and unchanged target
and source records. Duplicate groups are compared semantically without
depending on cross-group ordering.

Fresh Windows x64 Debug Ninja/MSVC verification (CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257, Qt 6.12.0) validated 1,047 handwritten source owners. The
policy, page, parity, and shared-policy CTests passed 4/4. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity target passed 1/1 (6/6
QtTest cases). The baseline overlay added only parity test source/registration
and three Qt minimum bumps; its production `class_details_page.cpp` matched
the pinned Git blob. Baseline parity proves no visible conflict warning or
write; direct query-count assertions remain in the current-only page tests.
No full suite or application build ran. Gate 2 gains these validation cases
and remains Partial. Gate 1 remains Partial; Phase 2 remains In Progress/Open.
F152 is selected: replace the page's direct `ClassService::classInfo()` read
with a fresh typed validation-context query using raw signed teacher ID and
exact UTF-16 notes/activity. Preserve the `-1` sentinel, missing-row defaults,
and read-failure fallback/order; keep the context out of the save request so
the save port rereads current values.

## F152 Class Details validation-context read — 2026-09-30

Commit `f70e3e23` replaces the page's direct `ClassService::classInfo()` read
with a fresh typed Application validation-context query and active-session
Platform adapter. The query returns raw signed teacher ID and exact UTF-16
notes/activity, checks the matched class ID, and the page uses an empty
`ClassInfo{}` context on query failure before continuing the existing
validation/conflict/save sequence. The save request is unchanged; the save
adapter still rereads current data and `ClassService` retains its final guard.

Independent Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257, Qt 6.12.0, and embedded debug information after PDB update
errors with `/Zi`. Current configure validated 1,051 handwritten source
owners. The query, Platform port, F150 policy, page save/display, parity, and
shared-policy targets passed 7/7 with `CL` cleared. The pinned baseline
`f232301e48f1e198d301acdfa3d8f704569f7ddc` parity CTest passed 1/1 with only
the parity-test source overlaid; the baseline production page matched its Git
blob. Parity covers stale hidden fields after page load and valid save
preservation. No full suite or application build ran. The first verification
attempt exposed a missing test include and unseeded `class_info` rows; both
test fixtures were repaired before the passing recheck. Gate 1 advances but
remains Partial; Gate 2 remains Partial. Phase 2 remains In Progress/Open.

## F153 teacher-ID validation parity — 2026-09-30

Commit `477ed151` adds current/baseline Class Details page parity when the
persisted `teacher_id` is changed to `0` after page load. The page reports
`class_info.teacher_id.invalid` on `teacherId` with value `0`, remains dirty,
shows no conflict warning, emits no save signal, and leaves target/source
records unchanged.

Independent Windows x64 Debug verification passed the current parity and
page-save CTest targets 2/2; the page-save target also contains the F152
direct no-conflict-query regression. The pinned legacy baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` passed the focused F153 parity
harness 1/1. Its temporary overlay contained only adapted parity test source,
test registration, and Qt minimum bumps; no production source was overlaid.
The baseline page matched blob `cdc48da8e3bab73dd0e064cf8364899f67ad1021`.
Toolchain: CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0, with
`CL` cleared and embedded debug information. No full suite or app build ran.
Parity uses a seeded conflict as a warning trap and does not count repository
queries; direct query-count evidence remains in current-only tests. Gate 2
remains Partial; Gate 1 remains Partial; Phase 2 remains In Progress/Open.

## F154 Class Notes page read boundary — 2026-09-30

Commit `8bcbf136` adds a dedicated typed Application read query and
active-session Platform adapter for Class Notes. The small projection carries
class ID, exact UTF-16 notes/activity, grade/level, regular schedule day/start,
and preferred teacher display name, with class and teacher results
independent. The page keeps trimming, subtitle formatting/fallbacks, and the
existing save boundary. Load and discard use the query; refresh/save add no
reads. The adapter does not use `DataService` fallback.

Independent Windows x64 Debug verification passed six current focused targets
6/6: Application query, Platform adapter, page, parity, and both existing save
port targets. Current CMake validated 1,058 handwritten source owners. The
original pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` passed
the focused page-parity harness 1/1 for initial text/subtitle and discard
reload. Baseline overlays were limited to parity test source/registration and
Qt minimum changes; no production source was overlaid. Baseline page and
header matched blobs `bbc9bc24a053aca83434eba6efac1e4ad5801bc2` and
`5c825327f1393791d7101ab33c10999768ff639a`. Toolchain: CMake 4.4.2, Ninja
1.13.2, MSVC 19.51.36257, Qt 6.12.0, `CL` cleared, embedded debug info. The
current run built the F154 working-tree source immediately before it was
committed unchanged as `8bcbf136`. No full suite or app build ran. Gate 1 and
Gate 2 remain Partial; Phase 2 remains In Progress/Open.

## F155 selected Class Co-Teacher read boundary

Add a dedicated typed page read query and active-session Platform adapter for
the assigned teacher ID and class/teacher fields used by
`SidebarNodeNaming`. Use it on load/discard and after successful assignment to
refresh selection and title. Preserve missing/read-error fallbacks. Leave the
teacher-choice catalogue read and existing assignment use case/adapter
unchanged; do not expand into roster, schedule, or teacher-profile work.
Acceptance should cover typed identity, independent field/teacher outcomes,
current page query behavior, load/discard/post-save title behavior, and
current-versus-pinned-baseline display parity. Gates 1 and 2 remain Partial;
Phase 2 remains In Progress/Open.

The older F123 candidate notes predate commit `9f7e736b`, which contains
`TeacherInfoPage` use-case integration and tests. The production integration
is present in the current checkout; this continuation did not re-verify its
CTest target or baseline parity.

## F155 accepted; F156 selected — 2026-09-30

F155 commit `2d810d0e21f85233575b700e927ec3a36c907f21` adds the typed
Co-Teacher selected-class/title read boundary. Six focused current CTest
targets passed 6/6. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity harness passed 1/1 with
test-only adaptation and no production overlay. Both versions assert initial
selection/title, discard after an external change, and post-save selection,
title, and persistence. No full suite or application build ran; no baseline
query-count claim is made.

F156 is selected for a separate typed Application and active-session Platform
read of the Co-Teacher teacher-choice catalogue. Preserve displayed fields,
bilingual ordering, selection, and load-failure warning/clear behavior. Gates
1 and 2 remain Partial; Phase 2 remains In Progress/Open.

## F156 accepted — 2026-09-30

F156 commit `3581078bdca61cfe76489ff19c5d518d8b3145bb` adds a separate typed
Co-Teacher teacher-choice read query and active-session Platform adapter. The
current eight focused CTest targets passed 8/8; the pinned baseline parity
harness passed 1/1 with test-only adaptation and no production overlay. Tests
cover direct repository mapping, no fallback, warning/clear behavior, crossed
Korean/English choice ordering, selected details, and load/discard behavior.
No full suite/app build or baseline query-count claim. Gates 1 and 2 remain
Partial; Phase 2 remains In Progress/Open. Three independent reviews are
complete; F157 is selected for the Teacher Profile persistence adapter.

## F157 accepted; F158 selected — 2026-09-30

F157 commit `cb6199f3369420c0e2e6c77d11f853d2799d9f92` moves Teacher Profile
save/reload persistence from the page-local `TeacherService` adapter to an
active-session Platform port backed by `TeacherRepository`. It maps every
profile field and preserves the existing edit use case, validation, canonical
reload, warnings, dirty state, and save signal. The focused current build and
CTest passed 4/4; the pinned baseline public-page parity passed 1/1 with only
the parity test source/registration overlaid and no production overlay. No full
suite/app build or baseline query-count claim. The tested page edit path also
reverifies the save/use-case and invalid-write behavior previously associated
with F123; no broader F123 claim is made.

F158 is selected for a typed Teacher Profile read query and active-session
Platform adapter at `NavigationController::handleTeacher`, using
`TeacherRepository::getTeacher()`. Keep it separate from the F157 edit port;
preserve lookup failure, confirmation, and page-load behavior. Acceptance will
cover ID/field mapping, session/repository errors, no fallback, current route
behavior, and pinned-baseline visible parity. Gates 1 and 2 remain Partial;
Phase 2 remains In Progress/Open.

## F158 and F159 accepted; F160 selected — 2026-09-30

F158 commit `4d099893071d4271ea8873d2219dfc7642de1e5c` adds the typed Teacher
Profile read query and active-session Platform adapter at
`NavigationController::handleTeacher`. The focused current build and CTest
passed 8/8; pinned-baseline navigation parity passed 1/1. The baseline run
used installed Qt 6.12.0 with a temporary scratch-only Qt metadata shim and a
three-argument constructor adaptation in the copied test. No production
overlay, full suite/app build, or baseline query-count claim. Phase 2 remains
In Progress/Open; no push was requested.

F159 source commit `1849ed23327538e2d21b05dfe0cebcc97e99d78c` adds a typed
Qt-free Application query and active-session Platform adapter for only the
Native English branch of `StaffDirectoryPage::loadDirectory()`, backed by
`NativeEnglishTeacherRepository::getAll()`. `NativeEnglishTeacherId` keeps its
table identity distinct from Korean teacher IDs. The focused current build and
CTest passed 5/5 targets. Pinned-baseline public-page parity passed 1/1 with
only parity/test-harness changes, no production overlay, and pinned production
blobs unchanged. The scratch harness used a temporary Qt 6.12 metadata shim
and baseline API adaptations, then restored the original baseline CMake hash.
No full suite/app build or baseline query-count claim.

## F160 accepted; F161 selected — 2026-09-30

F160 source commit `b703d3260a01b783594ffe6b87d10d9f7b02d193` adds a typed
Qt-free Application read query and active-session Platform adapter for the GS
Team branch of `StaffDirectoryPage::loadDirectory()`, backed by
`GsTeamRepository::getAll()`. It introduces a dedicated int-backed
`GsTeamMemberId` and preserves the five displayed fields, row ID role,
repository order, unavailable-session silent clear, repository-error warning,
and success-state behavior. The route race test closes the session during
leave confirmation; the page remains hidden with no legacy fallback.

The focused current CTest selection passed 9/9, including the F160 query,
adapter, page and route cases plus F159 and `StaffDirectoryPage` regressions.
Pinned-baseline visible parity passed 1/1 with only the F160 parity test and
registration overlaid; production blobs stayed pinned. The scratch run used a
temporary Qt metadata compatibility shim and a baseline constructor
adaptation, then restored the pinned CMake hash. No full suite/application
build or baseline query-count claim. Gates 1 and 2 remain Partial; Phase 2
remains In Progress/Open.

## F161 accepted; F162 selected — 2026-09-30

F161 source commit `ac173977d8d517d4af3236ee7794368bbe9a6bdc` adds a typed
Qt-free Application save/validation operation and active-session Platform
adapter for only the Native English branch of
`StaffDirectoryPage::saveDirectory()`, backed by
`NativeEnglishTeacherRepository::saveDirectory()`. The Application policy
owns empty/duplicate comparison-key and valid-or-blank birthday decisions;
the feature edge supplies Qt-normalized keys and date facts. The page retains
the localized warning, typed IDs, add/update/delete, transaction behavior,
dirty state on failure, quiet autosave, reload, and success signal.

Focused current CTest passed 12/12. Pinned-baseline visible save parity passed
1/1 against `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, with only a parity
test and registration overlay; production blobs remained pinned. The scratch
test adapted the baseline database-session API, and its root CMake was
restored to hash `cc8a061dfa64977926805167cc10418ca15d83d8`. No full
suite/application build or baseline query-count claim. Gates 1 and 2 remain
Partial; Phase 2 remains In Progress/Open.

F162 source commit `00a56324f1435475e3a7479fec99bc2e01653495` adds a
GS Team-specific typed Application save operation and active-session Platform
adapter backed by `GsTeamRepository::saveDirectory()`. It uses
`GsTeamMemberId`; requires at least one English or Korean name; enforces key
uniqueness within each language namespace while allowing cross-namespace
matches; and accepts valid or blank birthdays. The page preserves typed row
and deleted IDs, add/update/delete transaction behavior, warning and quiet
autosave paths, dirty-on-failure state, reload, and `directorySaved`.

Focused current CTest passed 16/16, including the F159-F162 policy, adapter,
persistence, and `StaffDirectoryPage` regressions. Pinned-baseline GS Team
save parity passed 1/1 on `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, with
only the parity test and registration overlaid and no production overlay. The
current C: build exhausted disk during link; serial focused verification passed
from D: scratch. Baseline setup adapted three database-session calls and used
a temporary Qt metadata shim; root CMake was restored to pinned hash
`cc8a061dfa64977926805167cc10418ca15d83d8`. No full suite/application build or
baseline query-count claim.

F163 source commit `a556c0441dcebbb3b6a7baecef6e293b5644b149` adds a typed
class ID/name list query and active-session Platform adapter backed by
`ClassRepository::getClasses()`, migrating only the ClassesPage list reads on
open and after ClassInfo save. It preserves repository ordering, names, IDs,
selection, and existing failure handling without a DataService fallback.

The focused Application and Platform CTests passed 2/2; the relevant direct
ClassesPage slots passed. Pinned-baseline parity passed for both visible list
opening and post-save rename/reordering on
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, with only test/stub overlays and
no production changes. The full ClassesPage CTest stalled in the existing
`classDetailsAndCoTeacherTabsSeparateTheirSectionCards()` slot. No full
suite/application build or query-count claim.

F164 source commit `85af7830708f062e34c94fde8fa0ff40310a8d9e` adds a typed
selected-class ID/grade query and active-session Platform adapter to
`ClassInfoRepository::loadClassInfo()`. Only
`ClassesPage::rebuildSectionTabs()` uses it. The query has no DataService
fallback; missing or failed grades keep Analytics and Evaluations visible.
Existing grade normalization, preference override, and section selection are
preserved.

Focused Application and Platform tests each passed 4/4 test slots; three
focused page slots passed. The existing middle-school/preference tab slot
passed against both current code and the exact pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`. Baseline production sources were
unchanged. A separate real-database parity harness timed out after 300 seconds
on both builds; no mismatch or cause was established. No full suite/application
build or query-count claim.

F165, source commit `e0b9d61213af57a79a685264c9f64fabf841bd1a`, adds a typed
Qt-free selected-class subtitle read and active-session Platform adapter.
Repository projections include only class grade/level, regular meeting
day/start time, and teacher display-name fields. Class and teacher outcomes
remain independent; formatting, “No class selected,” classroom-name/`Class N`,
and formatter fallbacks stay in the UI. The F163 list and F164 section-grade
paths remain separate.

Focused Application and Platform CTests passed. The ClassesPage slots
`selectedClassSubtitleUsesIndependentReadOutcomesAndRefreshes`,
`selectedClassSubtitleFallbackChainUsesTrimmedValues`, and
`selectedClassGradeFailureFailsOpenWithoutDataServiceFallback` passed. The
subtitle test covers class-read defaults, teacher failure retaining class
details, and a legacy-services-available/session-unavailable case without a
legacy class-info read. On exact pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, a test-only slot confirmed the
visible subtitle `E4 Hercules • Susan • Tues (4:00)`; the equivalent current
slot passed. All 584 baseline `src/` blobs matched. `git diff --check` passed.
No full suite/application build or query-count claim.

F166 source commit `577aea078a01b3a3986c07336c06631394620fcc` migrates only
`RosterEditorWidget::updateHeaderText()` to F165's typed subtitle query and
active-session adapter. Formatting stays at the UI edge. An unavailable outer
read falls back to the trimmed classroom name or `Class N`; class-detail
failure retains formatter defaults and teacher failure retains class details.
The invalid-ID message, title, embedded heading, roster load, and save paths
remain unchanged.

The current `ClassMngrClassesPageTests` target built with VS 18 2026 and Qt
6.12. Focused slots for the roster subtitle, no-session/name fallback, and
F165 independent outcomes passed (5/5 including QtTest initialization and
cleanup). The visible subtitle `E4 Hercules • Susan • Tues (4:00)` passed on
the pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` with only a
temporary test-source slot overlaid; the scratch source was restored and no
production source was overlaid. Coverage includes class-detail defaults,
teacher failure, invalid ID, trimmed name/`Class 42` fallback, and no-session
name fallback while legacy services were available without a legacy class-info
read. `git diff --check` passed. An unfiltered page-test run stalled at startup
and was stopped after 30 seconds. No full suite, application build, or
query-count claim.

F167 is selected: migrate the existing `ClassDetailsPage` display read
adapter, `ApplicationServicesClassDetailsPageReadPort`, from legacy class,
teacher, and roster services to active-session `ClassInfoRepository`,
`TeacherRepository`, and `RosterRepository` reads. Reuse the existing
`ClassDetailsPageReadSnapshot`; preserve independent class, teacher, and
student-count outcomes, raw schedule text/order, page defaults/fallbacks, and
`Teacher::preferredDisplayName()` precedence. Acceptance covers adapter
mapping/errors without DataService fallback, current display regressions, and
pinned-baseline visible fields, schedules, teacher, count, and fallback
parity. Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open. A
separate worker owns workflow repair; this work does not modify
`.github/workflows/refactoring-baseline.yml`. No push was requested.

## Archived Phase 2 position - 2026-10-01 (F178 accepted; F179 selected)

F170 source commit `68391fac` moves `ApplicationServicesRosterReadPort` from
`RosterService::roster()` to the active session's `RosterRepository`, with no
service or `DataService` fallback. The port preserves canonical class-ID
validation and complete ordered sparse snapshot mapping, including columns,
widths and UTF-16 cells; empty rosters succeed and repository failures remain
technical. Independent focused CTest passed 3/3: application query (0.02s),
Platform port (0.19s), and `RosterEditorWidgetSave` (2.08s). The two focused
roster editor read slots also exited successfully. `git diff --check` passed.
No full suite or application build was run.

F171 source commit `7cb8e5d7` migrates `ApplicationServicesRosterSavePort`
from `RosterService::saveRoster()` to the active-session
`RosterRepository::saveRoster()`, with no service or `DataService` fallback.
It preserves canonical-ID/session checks, roster normalization and validation,
the questionable Korean name-length choice, and Technical failure mapping.
Tests cover normalized snapshot persistence, complete no-write behavior for
invalid input, the allow flag, repository rollback, and the closed-session
path. Focused CTest passed 3/3: application save use case (0.02s), Platform
save port (0.24s), and `RosterEditorWidgetSave` (2.03s; 2.30s total). The
independent Tester also passed 3/3. `git diff --check` passed. No full suite or
application build was run.

F172 source commit `2daa209e` migrates
`ApplicationServicesClassCoTeacherAssignmentPort` to the active session's
`ClassInfoRepository`, with no `ClassService` or `DataService` fallback. It
preserves positive-ID behavior, typed assignment/unassignment, complete stored
class fields and schedules, and joined teacher metadata behavior. It also
retains `ClassInfoValidator` normalization/validation and regular-before-
intensive conflict checks with the existing message. Port tests cover invalid
IDs and loaded data, exact regular and intensive conflict errors and priority,
no-write preservation, injected transactional failure, and open/closed session
behavior. Independent focused CTest passed 3/3: Application use case, Platform
port, and Co-Teacher page targets. The independent Tester also passed 3/3.
`git diff --check` passed. No full suite or application build was run.

F173 source commit `a214dec4` migrates `ApplicationServicesScheduleSlotStateSavePort`
to the active session's `IntensiveSlotStateRepository`, removing the service
and `DataService` fallback. It preserves typed validation, weekday/time/state
mapping, default-state deletion, and widget behavior. Independent focused CTest
passed 3/3: application use case, Platform port, and ScheduleWidget. The
independent Tester also passed 3/3. `git diff --check` passed. No full suite or
application build was run.

F174 source commit `16188918` migrates
`ApplicationServicesSpeakingEvaluationReadPort` to the active session's
`SpeakingEvalRepository`, without service or `DataService` fallback. Both the
implementation run and independent verification passed the 3 focused targets:
application query, Platform port, and SpeakingEval page. Coverage includes
canonical IDs, exact and missing-name behavior, ordered UTF-16 matrix data,
repository failures, and closed-session behavior; page tests retain blank,
clean state after failed reads. `git diff --check` passed. No full suite or
application build was run.

F175 source commit `4009fcd5` changes `ApplicationServicesScheduleBuilderSourcePort`
from constructing `ClassService(session, nullptr)` to calling the active
session's `ClassInfoRepository::loadScheduleClassInfos()`. The implementation
run and independent verification passed all 4 focused targets: application
snapshot, Platform port, ScheduleBuilder, and ScheduleWidget. Existing coverage
checks query metrics, ordering, raw schedules, teacher/profile edge cases,
testing-class exclusion, and widget modes. `git diff --check` passed. No full
suite or application build was run.

F176 source commit `e111d5be` migrates
`ApplicationServicesSubPrepClassDetailsPort` from
`ClassService::subPrepClassDetails()` to the active session's
`ClassInfoRepository::loadSubPrepClassDetails()`. It preserves canonical
class-ID and record-identity checks, preferred-name selection, bounded UTF-8
fields, missing/stale teacher and class-info defaults, structured errors, and
the post-read closed-session recheck. Implementation and independent
verification each passed 3/3 focused targets:
`ClassMngrNextApplicationSubPrepClassDetailsQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and
`ClassMngrSubPrepPageTests`. Tests cover unavailable/closed sessions and
repository read failure. `git diff --check` passed; no full suite or application
build was run.

F177 source commit `25b9719c` migrates
`ApplicationServicesSubPrepScheduleSummaryPort` to the active session's
`ClassInfoRepository::loadSubPrepClassSummaries()`. It preserves request
validation, empty-scope success without a repository read, requested class
order and omission rules, bounded summary projection and meeting formatting,
and unavailable/read error mapping. Implementation and independent
verification each passed 3/3 focused targets:
`ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and
`ClassMngrSubPrepPageTests`. `git diff --check` passed; no full suite or
application build was run.

F178 source commit `afeab035` migrates
`ApplicationServicesSubPrepPrintSourcePort` from feature services to the
active session's class-info, teacher, and roster repositories. It preserves
selected mode/day filtering and class order, teacher caching and first-
reference order, missing/unassigned-teacher omission, bounded owning output,
class/teacher error mappings, and the zero student-count fallback only for
roster-count failures. The Platform test adds unopened/closed-session
`NotFound` coverage. Implementation and independent verification passed 4/4
focused targets: `ClassMngrNextApplicationSubPrepPrintSourceQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`,
`ClassMngrSubPrepPrintSourceMapperTests`, and `ClassMngrSubPrepPageTests`.
`git diff --check` passed. The build emitted a nonfatal `vswhere.exe` warning;
no full suite or application build was run.

F179 is selected: migrate `ApplicationServicesSubPrepRosterOutputSourcePort`
to the active session's class-info, class, teacher, and roster repositories.
Preserve request validation and empty-scope no-read, selected class order and
mode/day filtering, unassigned/missing/stale-teacher behavior, class and
schedule identity checks, extra-column deduplication, UTF-8 and aggregate text
limits, and cumulative remaining row/cell/text budgets for roster projection.
Keep existing error mappings, avoid `DataService` fallback, and return no
partial output on failure. Focused targets are
`ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests`,
`ClassMngrSubPrepPackageServiceTests`, and `ClassMngrSubPrepPageTests`. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial. Workflow repair remains
with the other worker.

## Archived Phase 2 position - 2026-10-01 (F196 accepted; F197 selected)

F179-F195 advanced session-backed Sub Prep and Calendar operations and six
preference ports; their individual commits and acceptance evidence are recorded
in the [Phase 2 progress log](../plans/qt-rewrite-heavy-route-plan/03-Phase-2-Progress-Log.md).

F196 source commit `b9f0a07d` migrates
`ApplicationServicesCurrentCampusPreferencesPort` to the active session's
`SettingsRepository`. It preserves `myInfo/campus`, verbatim UTF-8 and
`QVariant::toString()`, availability, empty read/no-op write behavior while
unavailable or closed, Technical write-error mapping, and unrelated settings.
The Executor's focused run passed all nine substantive cases; the independent
registered CTest passed 1/1 after an initialized MSVC rebuild. Source review
confirmed no `DataService`/`SettingsService` fallback, and `git diff --check`
passed. No full suite or application build ran.

F197 is selected for `ApplicationServicesMiddleSchoolAnalyticsPreferencesPort`
and its existing focused Platform target. Migrate to the active open session's
`SettingsRepository`; preserve the exact key, `QVariant::toBool()`, false
fallback and default-materialization attempt, unavailable/closed behavior, and
existing callers. Add closed-session no-fallback coverage. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

## Previous Phase 2 position - 2026-10-01 (F197 accepted; F198 selected)

F197 source commit `26c0f23b` migrates
`ApplicationServicesMiddleSchoolAnalyticsPreferencesPort` to the active open
session's `SettingsRepository`. It preserves the exact visibility key,
`QVariant::toBool()`, false fallback and materialization attempt, and
unavailable/closed behavior. The independent registered CTest passed 1/1. A
SQLite-trigger test verifies a failed explicit save stays silent and preserves
the previous value; closed-session tests verify no `DataService` fallback.
`git diff --check` passed. No full suite ran.

F198 is selected for `ApplicationServicesPersonalDetailsSavePort` and its
existing focused Platform test. Move the nine-key bundle to one active-session
`SettingsRepository::saveSettings()` transaction. Preserve exact keys and
values, UTF-8 and signature-image processing, signature normalization,
rollback, typed errors, and callers. Add closed-session no-fallback coverage.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

## Previous Phase 2 position - 2026-10-01 (F198 accepted; F199 selected)

F198 source commit `0e8361eb` migrates
`ApplicationServicesPersonalDetailsSavePort` to one active-session
`SettingsRepository::saveSettings()` batch. It preserves the nine exact keys,
UTF-8 values, prepared signature-image encoding, mode/font normalization,
Technical error mapping, and atomic rollback. The independent Tester rebuilt
the focused target in the x64 MSVC environment and passed CTest 1/1. Added
closed-session coverage confirms no `DataService` fallback and verifies all
seeded settings remain unchanged after reopen. `git diff --check` passed; no
full suite ran.

F199 is selected for `ApplicationServicesPersonalDisplayNamePreferencesPort`.
Move reads and writes for `myInfo/name` to the active session's
`SettingsRepository`; preserve exact UTF-8 and whitespace, empty reads and
successful no-op writes while unavailable, and Technical write-error mapping.
Add closed-session no-fallback coverage and verify the read port sees the name
written by F198's aggregate save. Focused target:
`NextPlatformApplicationServicesPersonalDisplayNamePreferencesPort`. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

## Previous Phase 2 position - 2026-10-01 (F199 accepted; F200 selected)

F199 source commit `dc489863` migrates
`ApplicationServicesPersonalDisplayNamePreferencesPort` to the active
session's `SettingsRepository`, with no service/facade fallback. It preserves
`myInfo/name`, exact UTF-8 and whitespace, empty reads and successful no-op
writes while unavailable, Technical write errors, and warnings on failed
reads. The independent Tester rebuilt the focused x64 MSVC target and passed
CTest 1/1. Tests prove closed-session behavior with `DataService` present,
read-error fallback and warning, and visibility of a name written by F198's
aggregate port. `git diff --check` passed; no full suite ran.

F200 is selected for `ApplicationServicesPersonalSignaturePreferencesPort`.
Move reads of `myInfo/signatureMode`, `myInfo/typedSignatureFont`, and
`myInfo/typedSignatureText` to the active session's `SettingsRepository`.
Preserve persisted-mode normalization, `QVariant::toInt()` font behavior,
UTF-8 text and whitespace, defaults without writes for missing/invalid values,
Technical unavailable failures, and warning/default behavior on read errors.
Add closed-session no-fallback coverage with `DataService` present; keep
callers and generic/sessionless `SettingsService` unchanged. Focused target:
`NextPlatformApplicationServicesPersonalSignaturePreferencesPort`. Both
Explorer lanes recommended this adapter as the next reader for F198's
aggregate bundle. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

## Previous Phase 2 position - 2026-10-01 (F200 accepted; F201 selected)

F200 source commit `ef603583` migrates
`ApplicationServicesPersonalSignaturePreferencesPort` to the active session's
`SettingsRepository`. It preserves the exact mode/font/text keys, mode
normalization, `QVariant::toInt()` font conversion, UTF-8 text, and defaults
without writes for missing/invalid values. Failed repository reads still warn
per key and return defaults; unavailable/null/closed sessions return the
existing Technical error. Independent focused CTest
`ClassMngrNextPlatformApplicationServicesPersonalSignaturePreferencesPortTests`
passed 1/1. Tests cover unchanged invalid values, repository read errors,
closed-session no-fallback with `DataService`, and values written by F198's
aggregate port. `git diff --check` passed; no full suite ran.

F201 is selected for `ApplicationServicesPersonalSignatureImagePort`. Move the
`myInfo/signatureImage` read to the active session's `SettingsRepository`.
Preserve Base64 decoding and `SignatureImage::prepareForEmbedding`, exact-key
lookup, empty outputs for missing/corrupt/unavailable/read-error cases, and
the warning on repository read error. Add closed-session no-fallback coverage
with `DataService` still present; keep the existing read interface and callers
unchanged. Focused target:
`NextPlatformApplicationServicesPersonalSignatureImagePort`. Both Explorer
lanes independently recommended this candidate because it complements F198's
aggregate writer. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

## Archived Phase 2 position - 2026-10-01 (F205 accepted; F206 selected)

F201 source commit `59133929` migrates
`ApplicationServicesPersonalSignatureImagePort` to the active open session's
`SettingsRepository`. It preserves exact-key lookup, `toString().toLatin1()`
Base64 decoding, one `SignatureImage::prepareForEmbedding()` call, and empty
results for missing, corrupt, unavailable, and read-error values while
preserving the repository-read warning. Independent focused CTest
`ClassMngrNextPlatformApplicationServicesPersonalSignatureImagePortTests`
passed 1/1. Tests cover valid image preparation/transparency, read-error
warning, closed-session no-fallback with `DataService` present, and unchanged
image/unrelated values after reopen. `git diff --check` passed; no full suite
ran.

F202 source commit `c9ff5731` migrates
`ApplicationServicesClassVisibilityPreferencesPort` to the active open
session's `SettingsRepository`. It preserves the exact key and enum/string
mapping, trimmed/lowercased `all_classes`, default persistence for missing or
invalid reads, no rewrite of valid unrecognized values, ActiveSchedule when
unavailable, and silent write failures. Independent focused CTest
`ClassMngrNextPlatformApplicationServicesClassVisibilityPreferencesPortTests`
passed 1/1. Coverage includes valid unsupported values remaining unchanged,
closed-session no-fallback with `DataService` present, a failed write that
preserves the stored value, and a silent repository read failure. No full suite
ran.

F203 source commit `4f128b01` migrates
`ApplicationServicesEvaluationDefaultPolicyPort` to the active open session's
`SettingsRepository`, with no compatibility-service fallback. It preserves
key `classes_navigation_evaluation_default_policy`, trimmed/lowercased
`current_or_previous_term`, canonical values, the All fallback, missing-key
materialization, no rewrite of valid unknown values, and silent save failures.
Independent focused CTest
`ClassMngrNextPlatformApplicationServicesEvaluationDefaultPolicyPortTests`
passed 1/1. Coverage includes valid unsupported values remaining unchanged,
closed-session no-fallback with `DataService` present, silent trigger-based
write failure, and silent read failure with a best-effort default save. No full
suite ran.

F204 source commit `3cf2ab80` migrates
`ApplicationServicesClassDayFilterResetPolicyPort` to the active open
session's `SettingsRepository`, with no compatibility-service fallback. It
preserves key `classes_navigation_day_filter_reset_policy`,
trimmed/lowercased `on_page_leave`, OnApplicationClose fallback and missing-key
materialization, no rewrite of valid unknown values, and silent save failures.
Independent focused CTest
`ClassMngrNextPlatformApplicationServicesClassDayFilterResetPolicyPortTests`
passed 1/1. Coverage includes valid unsupported values unchanged,
closed-session no-fallback with `DataService` present, silent trigger-based
write failure, and silent read failure. No full suite ran.

F205 source commit `bf9ca8a7` migrates
`ApplicationServicesClassSelectionResetPolicyPort` to the active session's
`SettingsRepository`. It preserves the exact key and canonical values,
trimmed/lowercase `on_page_leave`, the OnApplicationClose default, best-effort
materialization for missing/invalid/read-error values, no rewrite of valid
unknown values, and silent save failures. Focused CTest
`ClassMngrNextPlatformApplicationServicesClassSelectionResetPolicyPortTests`
passed 1/1. The two filtered Classes Page lifecycle slots also passed after
their fixtures were updated to open temporary databases; this confirms
page-leave clears class state while application-close retains it, with the
day-filter policy interaction preserved. No full suite ran.

F206 is selected for `ApplicationServicesCustomColorPalettePreferencesPort`.
Move persistence to the active open session's `SettingsRepository`; do not
fall back through `DataService` when closed. Preserve key `custom_colors`,
the fixed 16-color palette, defaults, QColor canonicalization, compact JSON
writes, and reads of legacy QStringList, JSON, newline, semicolon, and comma
payloads. Missing/unavailable settings return defaults without writes;
repository read failures warn and return defaults, and write failures warn
while preserving the previous value. Keep callers and the typed interface
unchanged. Add closed-session no-fallback with `DataService` present and
repository read-error coverage. Focused CTest:
`NextPlatformApplicationServicesCustomColorPalettePreferencesPort`. Both
independent Explorer lanes selected this bounded adapter migration. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

## Current Phase 2 position - 2026-10-02 (F232 accepted; F233 selected)

F218-F221 are accepted in commits `9bb936ee`, `57aefadf`, `4a87ab1e`, and
`ec65c0c6`. F222 source commit `6920e019` migrates Schedule Import resolution
choice reads to the F221 typed snapshot, preserving option order, labels,
defaults, room matching, and suggested/exact/supplemental class targets.
Independent focused app snapshot, Platform snapshot, and repository/apply
tests passed. The dialog target reported 24 passed and 3 failures; the only
failures were the documented baseline cases
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. `git diff --check` passed;
no full suite ran.

F223 source commit `4b46adc7` builds Schedule Import review matching from the
Qt-free Application projection and F221 typed state snapshot. It removes the
remaining `ScheduleService::previewImport()` read from review preparation and
adds class room number at the Platform snapshot boundary. Independent focused
verification passed the matching-projection, Platform snapshot-port, and
Schedule Import repository targets. The dialog target reported 27 passed and
the three documented baseline failures:
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. The duplicate-target warning
test verifies both initial class targets are 44 before the warning. No full
suite ran; `git diff --check` passed.

F224 source commit `3c45c74b` removes the ScheduleService availability probe
from review preparation and relies on the typed snapshot outcome. Independent
focused verification passed the Application matching-projection, Platform
snapshot-port, and Schedule Import repository targets; the dialog target
reported 28 passed with only the three documented baseline failures. The
closed-session test verifies the typed warning, no controls, and no legacy
preview call. The apply-time service write path remains unchanged; no full
suite ran and `git diff --check` passed.

F225 source commit `7d842339` adds a Qt-free Schedule Import apply request,
summary, use case, and typed write port, and routes confirmed dialog applies
through them. The Platform adapter maps the request to the existing plan and
retains `ScheduleService::importSchedule()` dispatch. A boundary check rejects
Reuse/UpdateRoom without a typed teacher ID and Create/Skip with one before
calling the port; current teacher existence and freshness remain repository
checks. The independent Application and Platform apply tests passed, as did
the review-decision, plan-eligibility, and Schedule Import repository targets.
The dialog target reported 29 passed and only the three documented baseline
failures: `acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. The independent recheck of
the target-pairing correction passed the Application apply test 1/1. `git diff
--check` passed; no full suite ran.

F226 source commit `6c6210b0` adds a Qt-free review-readiness use case that
returns typed decision issues, state-evaluation status, and an optional state
error. The dialog calls it once; it no longer calls either validator directly.
The same typed input fields and snapshot read cadence are preserved, as are
decision-conflict handling and the lower priority of state-error messages.
Independent readiness, review-decision, state-validation, and Schedule Import
repository targets passed 4/4. The dialog reported 29 passed with only the
three documented baseline failures. `git diff --check` passed; no full suite
ran.

F227 source commit `cb61f2ab` moves the proposed Schedule Import action counts
into a Qt-free Application projection consuming the existing typed review
decisions, ignored-diagnostic count, and UI-computed schedules-cleared count.
It preserves the legacy count rules, leaves localized text in the dialog, and
keeps the actual apply result separate. Projection and repository CTests
passed 2/2; the offscreen dialog run reported 30 passed and only the three
documented baseline failures. `git diff --check` passed; no full suite ran.

F228 source commit `d90f475f` moves the existing-schedules-cleared count into a
Qt-free Application projection over the typed state snapshot and review
decisions. It preserves the dialog's full-parse numeric ID matching, counts
classes with selected schedule hours that have no selected target, and returns
zero when an intensive schedule is preserved or no snapshot is available. The
dialog keeps localized summary formatting and snapshot cadence unchanged. The
new Application projection CTest passed 1/1; the dialog run reported 30 passed
and only the three documented baseline failures. A new assertion verifies the
zero count after snapshot refresh failure. Source ownership validation passed
for 1,170 handwritten files and `git diff --check` passed; no full suite ran.

F229 source commit `e8a2a5b5` reuses `projectScheduleImportStateSchedules()` to
select the Schedule Import review-preview schedules. The dialog retains its
control order, snapshot-backed Skip rows, UI enrichment, translated conflicts,
snapshot-failure fallback, and snapshot cadence; the Application projection
supplies class membership and intensive-preservation rows. Focused
state-validation and Schedule Import repository CTests passed. The Dialog
target reported 32 passed and only the three documented baselines:
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. Both new preview parity slots
passed. Source ownership validation found one owner for 1,170 handwritten
files and `git diff --check` passed; no full suite ran.

F230 source commit `547ccb5b` changes the review dialog to build and pass the
typed `ScheduleImportApplyRequest` directly to the Apply use case. It removes
the UI/Application legacy-plan conversion and keeps typed-to-legacy conversion
at the persistence adapter. Candidate and resolution mappings, typed IDs,
confirmation timing, and detailed policy-error text are covered by parity
checks. The Application Apply use case, Platform apply port, and Schedule
Import repository CTests passed. The Dialog target reported 34 passed and
only the three documented baselines:
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. All three new F230 dialog
slots passed individually; `git diff --check` passed. No full suite ran.

F231 source commit `6ebf6d33` derives `ScheduleImportReviewDecisionRequest`
from the typed `ScheduleImportApplyRequest` through one Qt-free Application
projection, shared by ApplyUseCase and dialog readiness/summary. The projection
preserves candidate and resolution ordering, UTF-16/UTF-8 conversion, trimmed
and empty-room handling, actions, and typed class targets. Five focused
Application CTests passed 1/1 each. The Dialog target reported 34 passed and
only the three documented baseline failures:
`acceptedReviewCanTearDownSourceDialog`, `mismatchedProfileRequiresConfirmation`,
and `reviewPreviewUsesSavedScheduleDisplaySettings`; the modified summary and
skip-cascade check passed. Source ownership found one owner for 1,172 files,
and `git diff --check` passed. No full suite ran.

F232 source commit `075b4335` routes the typed v2 apply port directly to the
active session repository. Its repository-bound typed adapter reuses the
existing plan-backed transaction core, preserving repository-time validation,
rollback, errors, and summary behavior while keeping the legacy service and
plan API for v1 callers. The Application ApplyUseCase, Platform ApplyPort, and
Schedule Import repository CTests passed 3/3. The focused dialog confirmation
slot passed. Typed success/persisted-summary, closed-session, legacy-route,
stale/overlap rejection, write-failure rollback, and intensive mapping
coverage passed. Source ownership found one owner for 1,172 files, and
`git diff --check` passed; no full suite ran.

## Current Phase 2 position - 2026-10-02 (F252 accepted; F253 selected)

F236 source commit `5877bba0` carries structured typed policy, teacher-target,
and fresh-state failures from Repository through Platform to the review dialog.
Fresh-state codes and context are preserved, including overlap start/end times;
the dialog uses its existing formatter and stays visible after failure. The
legacy `apply(plan)` API retains its localized text result, while SQL and
transaction failures remain message-only. Independent x64 Debug verification
built 330 actions and validated one owner for 1,174 handwritten sources.
Application and Platform CTests passed 2/2; 11 focused repository functions
and 4 dialog functions passed. The dialog test directly checks visibility after
failure. `git diff --check` passed; no full suite ran.

F237 source commit `8ef76074` adds Qt-free Speaking Evaluation content
normalization and validation used by the save use case and page feedback. It
preserves field locations, severity, focus and confirmation behavior, normalizes
before persistence, and suppresses invalid writes. A differential case checks
questionable Korean-name behavior against `SpeakingEvalValidator` for both
flag settings; declining confirmation leaves persisted data unchanged.
Independent x64 Debug verification completed 323 build steps and validated one
owner for 1,177 handwritten sources. Application, Platform SavePort, and page
save tests passed 9/9, 10/10, and 14/14; `git diff --check` passed. No full
suite ran.

F238 source commit `7b832bcc` moves initial-setup profile replacement into a
Qt-free Application lifecycle contract with a Platform file/workspace adapter.
The original profile remains recoverable through create and restore failures;
FileController retains warning and recent-file behavior. Independent x64 Debug
verification validated one owner for 1,181 handwritten sources. Three focused
CTest entries passed; the Application runner passed all 11 scenarios, Platform
tests passed 7/7, and FileController tests passed 37/37. `git diff --check`
passed; no full suite ran.

F239 source commit `d3e9cded` moves roster row-reordering rules into a Qt-free
contract over `RosterSnapshot`; `RosterModel` adapts typed failures to its
existing messages and preserves validation refresh, notifications, and dirty
behavior. The independent x64 Debug build validated one owner for 1,183
handwritten sources. The Application contract, RosterModel, and roster editor
widget CTests passed 3/3. Coverage includes both move directions, invalid and
equal indexes, blank and Unicode whitespace-only rows, complete cell/metadata
preservation, selection/current-column behavior, and autosave. `git diff
--check` passed; no full suite ran.

F240 source commit `4be18aa5` extracts roster row removal and compaction into a
Qt-free operation over `RosterSnapshot`; `RosterModel` adapts typed failures to
existing messages and retains validation refresh, notifications, and dirty
behavior. Independent fresh x64 Debug verification built the app-less removal,
RosterModel, and widget save targets; focused CTest passed 3/3. Ownership found
one owner for 1,185 handwritten files, and `git diff --check` plus new-file
whitespace checks passed. Coverage includes full-row and metadata preservation,
invalid and empty/Unicode-whitespace-only rows, final-slot clearing, validation
and signals, and widget confirmation/selection/autosave. No full suite ran.

F241 source commit `746ef0bb` extracts custom-column name admission into a
Qt-free Application policy. It preserves `QString::simplified()` whitespace,
the Autumn-to-Fall alias, Qt's case-insensitive duplicate/required checks, and
existing messages. `RosterModel` keeps insertion, notifications, row extension,
validation refresh, and dirty state. A shared Qt adapter copies UTF-16 code
units exactly; leading U+FEFF content survives the F239/F240 row policies.
Independent fresh x64 Debug verification built four focused targets; CTest
passed 4/4. Ownership found one owner for 1,188 handwritten files, and
`git diff --check` plus new-file whitespace checks passed. No full suite ran.

F242 source commit `5525cade` extracts custom-column removal eligibility into
a Qt-free Application policy. It preserves invalid-index and required-column
rejection, the Autumn-to-Fall alias, Qt comparison, and existing messages.
`RosterModel` retains mutation, notifications, validation refresh, dirty state,
width/layout work, destructive confirmation, and autosave. Fresh x64
Debug/Ninja/MSVC 19.51/Qt 6.12 verification validated one owner for 1,190
handwritten sources; five focused CTests passed. Diff and new-file whitespace
checks passed. No full suite ran.

F243 source commit `68260142` extracts Qt-free custom-column append over roster
columns and rows, reusing F241 admission. It preserves existing cells and width
metadata while the model and widget retain their current UI effects. Independent
fresh x64 Debug verification validated one owner for 1,192 handwritten sources;
five focused CTests passed and new-file whitespace checks were clean. No full
suite ran.

F244 source commit `86fe782f` extracts destination-side roster transfer
preparation into Qt-free Application. It preserves matching and normalization,
empty/full/duplicate rejection order, first-empty-slot selection, and the legacy
pair-key behavior. The model retains mutation and UI effects; the widget retains
class lookup, source removal, width handling, and atomic save. Independent fresh
x64 Debug verification validated one owner for 1,194 handwritten sources; six
focused CTests passed. Diff and new-file whitespace checks passed. No full suite
ran.

F245 source commit `99c95146` moves first-unused Korean-name suffix suggestion
into a Qt-free Application policy shared by RosterModel and SpeakingEvalModel.
The models preserve exact legacy behavior by projecting
`StudentNameUtils::baseKoreanName()` and `koreanNameSuffix()` at the Qt edge; the
policy handles trimmed, case-sensitive English grouping and A-Z selection.
Duplicate-pair grouping and the suffix choice UI remain at their current owners.
The lossless UTF-16 adapter moved from Roster UI to `src/ui/shared` to remove a
cross-feature dependency. Independent fresh x64 Debug/Ninja/MSVC 19.51/Qt 6.12
verification validated one owner for 1,196 handwritten files; the app-less
policy, RosterModel, SpeakingEval page-save, and Speaking Evaluation save-use-
case CTests passed 4/4. Differential checks cover U+3000, unpaired UTF-16,
malformed suffixes, and the page choice/apply flow. `git diff --check` and
new-file whitespace checks passed; no full suite ran.

F246 source commit `b1a86db2` extracts same-grade roster transfer-target
eligibility into a Qt-free Application policy. It rejects invalid/current
class IDs, requires a nonempty current grade, and preserves trimmed,
case-sensitive grade equality. The widget retains class/roster lookups and
failures, labels, sorting, fullness, menu actions, and transfer/save behavior.
Independent fresh x64 Debug/Ninja/MSVC 19.51/Qt 6.12 verification validated
one owner for 1,198 handwritten sources; the app-less eligibility,
RosterEditorWidgetSave, and TestingClassesPage focused CTests passed 3/3.
`git diff --check` passed. The UI tests did not reach nonempty-grade target
enumeration or assert `classInfo` lookup counts; policy decisions are covered
directly. No full suite ran.

F247 source commit `7021e657` extracts roster-name import planning into a
Qt-free Application policy. It preserves Qt-compatible trimming, complete-pair
filtering, the legacy U+001F pair key and delimiter collisions, existing and
imported duplicate filtering, source order, and blank-row assignment order.
The page retains case-insensitive first-header lookup, per-cell editability,
unchanged-cell checks, applyChanges, and UI messages. Fresh x64 Debug/Ninja/
MSVC 19.51/Qt 6.12 verification found one owner for 1,200 handwritten sources;
the policy and SpeakingEvalPageSave focused CTests passed 2/2. The page test
checks first case-insensitive header selection, applied values, dirty state,
and success/already-up-to-date messages. `git diff --check` passed; no full
suite ran. Coverage uses the shared Qt-compatible whitespace helper without
exhaustively comparing all Unicode whitespace, and the current model does not
allow a partial-editability test for its two name cells.

F248 source commit `8886b46f` is accepted. It extracts interactive duplicate-
peer row lookup into Qt-free Application shared by RosterModel and
SpeakingEvalModel, reusing F247's trimmed UTF-16 U+001F key and preserving
incomplete pairs, exact case-sensitive matching and delimiter collisions,
selected-row exclusion, and candidate order. Column selection, translated
prompts/actions, and duplicate resolution remain at their model/page owners.
Independent fresh x64 Debug verification validated one owner for 1,202
handwritten files; the new app-less lookup, RosterModel, and SpeakingEval
page-save CTests passed 3/3. `git diff --check` passed. No full suite ran. The
adapter test does not cover unpaired surrogates or exhaustively compare Qt
whitespace; RosterModel's public setup always supplies base name columns, so
the missing-column guard cannot be reached through that route.

F249 source commit `828d5014` extracts Speaking Evaluation AI-batch student
eligibility into a Qt-free Application policy. It preserves first-failure
order: missing name, unsupported grade, missing Did Well observations, then
missing Needs Improvement observations. The dialog retains translated reasons,
row enablement/check state, review, prompts, comment application, and overwrite
confirmation. Fresh Windows x64 Debug verification validated one owner for
1,204 handwritten sources; the app-less eligibility CTest passed 1/1, and the
two relevant dialog functions passed. `git diff --check` and new-file hygiene
passed; no full suite ran. Two unrelated clipboard functions reproduced
`OleSetClipboard`/`OpenClipboard Failed` in the fresh environment and are not
proven against a pre-F249 baseline.

F250 source commit `4753ce3b` extracts a shared Qt-free roster-row availability
query. It treats a row as occupied when any cell remains after Qt-compatible
trimming and returns the first empty row or `rows.size()`. RosterModel projects
its QString rows through the UTF-16 adapter and returns `-1` for a full 25-row
roster. Transfer preparation reuses the same occupancy and first-empty logic
while preserving source-empty/full/duplicate rejection order and destination.
Independent fresh x64 Debug verification validated one owner for 1,206
handwritten sources; the app-less query, RosterModel, and transfer-preparation
CTests passed 3/3. `git diff --check` and new-file hygiene passed; no full suite
ran.

F251 source commit `ed548438` extracts AI batch per-comment review quality
from `SpeakingEvalAiBatchDialog::updateReviewRow()`. Fresh independent x64
Debug verification in `build/f251_independent_x64_debug` validated one owner
for 1,208 handwritten files; the app-less quality test and three focused dialog
functions passed. The 420 preferred threshold is shared by the prompt builder
and dialog through `SpeakingEval::CommentPreferredMaxLength`; rendered prompt
wording is unchanged. The hard maximum is passed from the existing domain
constant. `git diff --check` and new-file hygiene passed. No full batch CTest
or suite ran.

F252 source commit `6f41f8a2` extracts accepted AI batch comment planning
from `SpeakingEvalAiBatchDialog::applyComments()` into Qt-free Application.
Fresh independent x64 Debug/Ninja/MSVC 19.51/CMake 4.4.2/Qt 6.12 verification
in `build/f252_independent_x64_debug` validated one owner for 1,210 handwritten
sources. The app-less planner CTest passed 1/1; dialog slots
`aiBatchDialogConfirmsAcceptedCommentOverwrites` and
`aiBatchDialogSelectsEligibleStudentsAndReviewsValidComments` passed. Coverage
checks selected/valid and report-index filtering, unchanged comments, overwrite
count, input order, UTF-16 fidelity, and confirmation reject/accept. Diff and
new-file hygiene passed; no full suite ran.

F253 is selected to extract the identical private-notes section splitter
shared by `SpeakingEvalAiBatchDialog` and `SpeakingEvalPrivateNotesEditor` into
Qt-free Application. Preserve the `[Did Well]\n` prefix, first
`\n[Needs Improvement]\n` separator, exact body whitespace/newlines, and the
legacy fallback of returning all notes as Did Well when either marker is
missing. Keep serialization, bullet-list editing/normalization, prompts, and
observation parsing with their existing owners. Paired scans differed: the
alternative was AI-batch response parsing. Choose the shared exact duplicate
as a narrower reusable boundary; defer response-parser extraction. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

## Archived Phase 2 position — 2026-10-07 (F382 accepted)

Deployment `phase2_resume_20260929` continues in Phase 2 on the Heavy route.
Batch 12 is complete at F381. F382 is accepted as the first completed slice
from the discovered Batch 13. The user requested stopping after its commit;
F383-F386 remain provisional and unselected. Phase 2 remains In Progress/Open,
with Gates 1 and 2 Partial.

The user reports F367-F371 were completed on another device, but they remain
unsynced and unaudited in this checkout; local Batch 11 evidence still ends at
F366. No remote commits or acceptance results are inferred.


## Current Position — 2026-10-08 (F402 current; Batch 18 discovery underway)

Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial. F386-F393
are accepted as recorded in the Phase 2 progress log. The seven Teacher Profile
Edit MSVC diagnostics were fixed in commit `0b128601`; the named target and
standard all-target build passed earlier, and the exact affected persistence
target rebuilt successfully with no reported errors or LNK4006 warnings.

F394 Classes landing open-session confirmation parity, F395 Campus Dashboard
typed save boundary, F396 Staff Directory open-session dirty-exit parity, and
F397 MainWindow Document Catalog retranslation are accepted. F398 same-path
workspace-open parity is accepted with focused CTest 3/3. F399 Close File
no-workspace parity is accepted and committed as `f21db8c3`; its real-window
test passed 1/1 in the focused Ninja/MSVC/Qt 6.12 fallback tree.

F400 Open File dirty-page gate parity is accepted with no production changes.
The real QAction test verifies page Cancel preserves the dirty draft and skips
the chooser; Discard reaches the chooser, and chooser cancellation preserves
the same session with the saved profile restored. The focused Ninja/MSVC/Qt
6.12 target built and filtered CTest passed 1/1. Configure reported optional
missing WrapVulkanHeaders and nonfatal object-path-length warnings; no LNK4006.
The standard Visual Studio tree remains blocked before source compilation in
ZERO_CHECK by the current shell's FileTracker CommonApplicationData path
resolution; no system settings changed. No full suite ran.

The seven Teacher Profile Edit MSVC diagnostics were rechecked against the
current source: the focused x64 MSVC/Qt 6.12 Ninja target built and its filtered
CTest passed 1/1. The diagnostics refer to pre-fix code already corrected in
commit `0b128601`; no source changes were needed for this report.

F401 Recent-workspace menu selection and missing-path pruning is accepted
and committed as `2121ce59`, with no production changes. Two new
MainWindow tests use the production Recent menu to open a different existing
workspace without a chooser and to prune a missing path while verifying the
warning, history/menu updates, and exact active session/page preservation.
The focused Ninja/MSVC/Qt 6.12 target built and filtered CTest passed 1/1
(1.02 s); incremental build with no warnings. No full suite ran; the standard
Visual Studio tree was not used for this slice.

F402 MainWindow application-exit confirmation is committed as 9cbe39c9,
with no production changes. A real visible MainWindow test exercises
QWidget::close() with a dirty Details draft and open workspace. Cancel rejects
close while preserving the window, exact session/path, page, tab, Sidebar
selection, draft, and dirty state. A second close with Discard is accepted and
restores the persisted draft. The focused Ninja/MSVC/Qt 6.12 target built and
filtered CTest passed 1/1 (0.34 s); CMake reconfigured and emitted optional
WrapVulkanHeaders and known object-path-length warnings, with no LNK4006.
No full suite ran. The standard Visual Studio tree remains blocked before
source compilation by the existing FileTracker/CommonApplicationData issue.

F403 Dynamic Teacher Sidebar leaf navigation is accepted and committed as
28e881cd, with no production changes. A real MainWindow test opens a seeded
workspace, uses the production refresh path, and clicks leaves in Co-Teachers
and Korean Teachers. For each group, it verifies target teacher ID and stable
route keys; Cancel preserves the dirty source page and exact draft, and Discard
loads the target with a clean page while leaving source persistence unchanged.
The focused Ninja/MSVC/Qt 6.12 target built and CTest passed 1/1 (0.52 s); an
independent filtered CTest also passed 1/1. No full suite ran.

F403 is committed as 28e881cd, completing Batch 17. F404 Save choice on
Open/Close File actions is accepted and ready to commit, with test-only changes.
The Open File test scripts Save then chooser cancellation and verifies one prompt
and chooser request, the same workspace/session/path/page/tab/Sidebar, visible
draft with clean page/workspace, and persisted myInfo/name. The Close File test
scripts Save and verifies one prompt/no chooser, closed session/path, Campus
Dashboard Information route, disabled database actions, and persistence after
reopening the workspace. Both focused Ninja/MSVC/Qt 6.12 targets built under the
VS 18 x64 developer environment, and the filtered CTest passed 2/2. No full
suite ran.

F405 MainWindow Save As and Export action integration is committed as
07dc5864. Its target built under VS 18 x64 with Ninja/MSVC; CTest simple-name
filter passed 1/1. Batch 18 remains active with F406 Manage Campuses QAction
transition selected/current; F407-F413 remain queued.

F406 is accepted and ready to commit. Cancel preserves page/tab, dirty draft,
Sidebar, session/path, and blocks the transition. Discard reaches Campus Dashboard
Information with matching Sidebar selection, restores the persisted draft cleanly,
and preserves session/path/actions. MainWindow now calls
`CampusDashboardPage::showInformation()` after showing the reused Dashboard,
closing the Sidebar/page mismatch. The focused `ClassMngrMainWindowManageCampusesParityTests`
target built under VS 18 x64 with Ninja/MSVC; simple-name filtered CTest passed
1/1 with both Cancel/Discard cases. F407 is queued after the F406 commit;
F408-F413 follow.

F406 is committed as 28b27998, completing the accepted Manage Campuses
transition.

F407 Schedule↔Testing Classes handoff is implementation complete and accepted,
ready to commit. Real Testing Classes/back actions cover standalone Schedule and
My Workspace → Schedule, returning to the correct source page/tab and preserving
active session/path/Sidebar. The cell-dialog Manage Classes producer forwards the
empty choice list’s natural non-positive ID plus a specific day/time; class
creation persists the requested slot. Cancel preserves dirty Testing Classes
state; Discard completes the source-dependent return. The cell-dialog route now uses `QTest::mouseClick` on the rendered table
viewport, covering the actual `QTableWidget` `cellClicked` connection and
downstream handoff. Focused target
`ClassMngrMainWindowScheduleTestingClassesHandoffParityTests` built with
Ninja/MSVC; CTest passed 1/1 (1.54 s) on 2026-10-08. F408 is queued after the
F407 commit;
F409-F413 follow.

The user-modified latest_session_work.md remains untouched.

F407 is committed as `f639fbd3`, with the MainWindow Schedule ↔ Testing Classes
handoff and focused `ClassMngrMainWindowScheduleTestingClassesHandoffParityTests`
coverage (Ninja/MSVC build, filtered CTest 1/1).

F408 Dynamic Teacher Sidebar selection/state after retranslation is
implemented and accepted. `Sidebar::selectByKeys()` now restores the exact
duplicate teacher occurrence from the saved stable key path plus teacher ID,
with the prior ID-based behavior as fallback when that occurrence is gone.
The MainWindow integration test switches English↔Korean from both Co-Teachers
and Campus Staff → Korean Teachers and verifies selection path/ID, expansion,
no route event, Teacher Info page and identity, dirty manual-save draft without
prompts, delete-action state, and active database session/path. Dynamic teacher
display remains stable while group and internet-type labels translate; the
localized combo assertion compares its stable data separately from its display
text. `ClassMngrMainWindowTeacherSidebarNavigationParityTests` built under VS
18 x64 and filtered CTest passed 1/1 (1.21 s). F409-F413 remain queued in
Batch 18; the next-batch discovery trigger is F412.

F408 is committed as `9f92b78d`. F409 My Workspace Sidebar root
producer-to-handler integration is implemented and accepted. The focused test
clicks the rendered root, passively verifies its actual `NavigationData`
payload, then asserts the My Workspace page and instance, Schedule tab, root
selection, and unchanged open database session/path. This closes a coverage
gap: prior tests selected the root programmatically or fabricated the payload
and called the controller directly. `ClassMngrMainWindowMyWorkspaceSidebarRootNavigationTests`
built under VS 18 x64; filtered CTest passed 1/1 (0.37 s). No runtime defect
was established; a possible dirty-cancel selection mismatch remains a separate
source inference. F409 is committed as `ee319df2`.

F410 Classes Sidebar root integration is implemented and accepted. The new
MainWindow test clicks the rendered root, passively verifies the actual `Page`
payload, displayed label, stable `classes` key/route, and natural class ID -1,
then asserts the Classes page, selected root, and unchanged open database
session/path. This closes the gap left by direct slot invocation and synthetic
controller-route tests. Initial CTest exposed a production teardown lifetime
defect: QObject-owned pages retained non-owning service pointers but were
destroyed after MainWindow's `ApplicationServices` member. The destructor now
deletes `m_pages` while services remain alive. `ClassMngrMainWindowClassesSidebarRootNavigationTests`
built under VS 18 x64; independent filtered CTest passed 1/1 (0.32 s) after the
fix. F410 is committed as `14723973`. F411 Sub Prep Sidebar root integration
is implemented and accepted. The new MainWindow test clicks the rendered root,
verifies the actual `Page` payload, displayed label, stable `sub_prep` key/route
and class ID -1, then asserts the Sub Prep page at Important Information, root
selection, and unchanged open database session/path. Closed-session and
dirty-page cases remain covered by the route-gate test. The focused target
rebuilt under VS 18 x64; independent CTest passed 1/1 (0.41 s), and the direct
executable exited normally. F412-F413 remain queued in Batch 18; the next-batch
discovery trigger is F412. F411 is committed as `3e5dbea4`. F412 Campus Sidebar
root plus section producer integration is selected/current. Batch 19 discovery
is complete: eight provisional candidates were reconciled from two independent
bounded reviews before F412 implementation. The only preserved unrelated
worktree entries are the user-modified `latest_session_work.md` and untracked
`%SystemDrive%/` artifact.

F412 Campus Sidebar root and section producer-to-handler integration is
implemented and independently accepted. Its real MainWindow test clicks the
rendered root and all five sections, asserts each actual `NavigationData`
payload, destination page/section and Sidebar path, and unchanged database
session/path. The VS 18 x64 Debug target rebuilt; independent filtered CTest
passed 1/1 (0.42 s), with no lingering process. No production change or runtime
defect was needed. F412 is ready to commit; F413 Initial Setup success
navigation remains queued in active Batch 18.

F412 is committed as `76661791`. F413 Initial Setup success navigation from
the empty-state button is selected/current; acceptance discovery is underway.
Batch 18 remains active, and the already completed Batch 19 discovery remains
provisional until F413 completes.

F413 Initial Setup success navigation from the empty-state button is
implemented and independently accepted. The real MainWindow test QTest-clicks
the visible no-database button from Campus Dashboard Information, verifies the
BasePage/PageManager request signals and accepted InitialSetupWizard, then
checks the created profile/session, My Workspace Schedule, `my_workspace`
Sidebar selection, and hidden empty-state banner. The VS 18 x64 Debug target
rebuilt; independent filtered CTest passed 1/1 (0.46 s), with no lingering
process. No production change or runtime defect was needed. F413 is ready to
commit; Batch 18 is complete after its commit and Batch 19 candidates are ready
to activate. F413 is committed as `5d8a941a`, completing Batch 18. Batch 19 is
now active with F414 Teacher profile save preserving the selected duplicate
Sidebar occurrence selected/current; F415-F421 follow in discovered order.

F414's MainWindow regression reproduced the occurrence shift before the fix:
saving from Campus Staff → Korean
Teachers selected the Co-Teachers occurrence. The save handler now restores
the selected teacher key path after rebuilding the Sidebar. The valid manual
Save flow verifies both updated leaves, persisted profile, clean current page,
and stable database session/path. Independent VS 18 x64 Debug target build
passed; the exact filtered CTest passed 1/1 (1.52 s), and direct QtTest passed
5/5. No LNK4006 warnings occurred. F414 is committed as `6fb39b2b`. Batch 19
continues with F415 Campus Dashboard page-tab-to-Sidebar synchronization
selected/current, accepted and ready to commit. F415 is committed as
`f4bc5282`. F416 Document Catalog rendered Sidebar leaf through MainWindow and
viewer is selected/current; bounded source discovery is underway. F417-F421
remain queued.

F415 uses the existing MainWindow/open-session fixture to QTest-click all five
Campus Dashboard tabs without a test-side signal connection. Each click emits
the expected section key, synchronizes the Sidebar path, preserves the current
page and database session/path, and emits no extra Sidebar route event. The
focused VS 18 x64 Debug target rebuilt; independent filtered CTest passed 1/1
(0.46 s). No LNK4006 warnings occurred, and no production change was needed.

F416 extends the real MainWindow retranslation test with a rendered
`document_guides_lesson_planning` Sidebar leaf click through the production
route connection. It confirms the PDF viewer was uninstantiated before the
click, one expected route and selected key path, a Ready content session with
the expected resource reference/path and print/save capabilities, and no
navigation-time modal or Qt warning. No production change was needed. The
focused target built under VS 18 x64 Debug; executor and independent CTest runs
passed 1/1. The executor build reported `LNK4075` (`/INCREMENTAL` ignored due
to `/FORCE`); the independent build did not reproduce it. No full suite ran.
F416 was accepted and committed in its slice. Work paused after F416 at
user request, then resumed through F429. Batch 20 completed with F428. F426 is
committed as 249d57b11ce327923a6416d72e53e034b53cda3c; F427 as
bbdf10e85ba2c5a61066ed6fd32a7c8993c0f04e; F428 as
b82bddaa59c57d88f773370e94b3163df9055f33; and F429 as
19f6024d113dda41e7e7a2b41acf27961160462b. Batch 21 is active with F430
Schedule Print QAction selected/current. Its acceptance matrix is recorded in
the Phase 2 progress log: trigger the real MainWindow action, inspect and
cancel the Print-mode Schedule dialog before native printer UI, then verify
workspace state remains stable. Executor and independent focused
verification passed. F430 is committed as aa7fca37. F431 Import Teachers
through MainWindow, including the page-leave gate, is selected/current. Its
Cancel and Discard cases are accepted with focused verification. F431 is committed as 2ac08388. F432 Export Classes QAction through its selection dialog and JSON picker is selected/current; the QAction-to-picker-cancellation case passed focused verification. F432 is committed as 4bfe3dc2. F433 is committed as b60c8025. F434 Delete Teacher QAction confirmation through MainWindow is accepted in this changeset and ready to commit; F435-F438 remain provisional.


## 2026-10-09 - F434 acceptance matrix recorded

F434 targets the real MainWindow Delete Teacher QAction in tests/mainwindow_teacher_sidebar_navigation_parity_tests.cpp. Seed a target and a survivor, leave MainWindow on MyWorkspace without a teacher selected, and trigger window.actions().deleteTeacher. A bounded timer automates the real sidebarRecordSelectionDialog by choosing the target ID and accepting; the fake user prompt then captures the destructive Delete Teacher confirmation and returns Cancel. Assert the requested teacher/display-name text, destructive flag, and Delete/Cancel choices; target and survivor remain persisted and represented in the sidebar; Delete Teacher remains enabled; and the page, central widget, sidebar selection, database session, and path stay stable with no extra prompt. This is QAction-to-confirmation cancellation characterization; it does not claim successful deletion. Existing direct-controller coverage already exercises the lower-level rejection and chooser paths. Focused target: ClassMngrMainWindowTeacherSidebarNavigationParityTests; CTest: ClassMngrMainWindowTeacherSidebarNavigationParityTests. Matrix recorded before implementation; no implementation yet.


## 2026-10-09 - F434 QAction cancellation accepted

The real MainWindow Delete Teacher QAction opened the teacher chooser, selected the seeded target, and presented the destructive confirmation. FakeUserPromptService rejected it. The target and survivor snapshots and sidebar entries remained unchanged; the Delete Teacher action stayed enabled; MyWorkspace page, widget, sidebar selection, database session, and path remained stable, with no additional prompt. The case claims cancellation only, not successful deletion. Executor and independent Tester each passed focused target build, exact Debug CTest 1/1, direct QtTest 6/6, selected case 3/3. A five-second watchdog rejects an active modal if chooser acceptance does not happen. Executor used Ninja Debug after VS FileTracker access-denied errors; independent VS target build passed without retry. No full suite or production/CMake change. F434 is committed as 3fb5241347d14c2d9dfa94b5a25a6742d6ba28df; branch is ahead by 19.


## 2026-10-09 - F434 committed; F435 selected

F434 committed as 3fb5241347d14c2d9dfa94b5a25a6742d6ba28df (Phase2 - Cover Delete Teacher QAction cancellation (F434)); branch is ahead by 19. F435 Empty-state Open/New Profile button handoff through Banner, PageManager, and MainWindow is accepted in this changeset and ready to commit. F436 Invalid UTF-8 document resource references is next after commit; F437-F438 remain provisional. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F435 Open/New Profile acceptance matrix recorded

F435 targets tests/mainwindow_initial_setup_empty_state_navigation_tests.cpp. Add separate success cases from the no-database CampusDashboard banner. Open case: seed a valid Teacher Profile in the test temporary directory, script its path via FakeFileDialogService, click noDatabaseOpenButton, and assert exactly one BasePage::openDatabaseRequested, PageManager::openDatabaseRequested, openFile QAction trigger, and TeacherProfile open-picker request with expected filter/parent; then assert the path/session open, MyWorkspace Schedule route and sidebar selection, and banner hidden. New case: choose a unique nonexistent .tps path in the temporary directory, click noDatabaseNewButton, assert the corresponding BasePage/PageManager newDatabaseRequested signals, newFile QAction trigger, and TeacherProfile save-picker metadata; then assert file creation, open path/session, MyWorkspace Schedule route/sidebar selection, and hidden banner. Both cases assert no initial-setup request, unexpected prompt, or opposite picker request. F413 Initial Setup coverage remains separate; New Profile must not invoke the wizard. Build target and exact CTest: ClassMngrMainWindowInitialSetupEmptyStateNavigationTests. Matrix is recorded before implementation; implementation has not started.


## 2026-10-09 - F435 banner Open/New Profile accepted

Separate real banner button cases verify BasePage → PageManager → MainWindow QAction handoff for Open and New Profile. Open uses a seeded valid temporary profile and checks TeacherProfile open-picker metadata; New uses a unique nonexistent temporary .tps path and checks save-picker metadata and created file. Both assert a live session and expected path, MyWorkspace Schedule/sidebar selection, hidden banner, and no Initial Setup request, prompt, or opposite picker. F413 remains separate; New Profile does not invoke the wizard. Executor and independent Tester passed the focused build, exact Debug CTest 1/1, target QtTest 5/5, and each new case 3/3. Executor Ninja Debug had no compiler warnings; independent VS target build passed (Qt reported missing system font directory; repository fonts loaded). No production/CMake change or full-suite run. F435 is committed as 998565115361bdd301f1d06ecc4beb2da7a519b9; branch is ahead by 20.


## 2026-10-09 - F435 committed; F436 selected

F435 committed as 998565115361bdd301f1d06ecc4beb2da7a519b9 (Phase2 - Cover Empty-state Open/New Profile button handoff (F435)); branch is ahead by 20. F436 Invalid UTF-8 document resource references is accepted in this changeset and ready to commit. F437 is next after commit; F438 remains provisional. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F436 invalid UTF-8 acceptance matrix recorded

F436 will define malformed UTF-8 in raw std::string document references as Domain::ErrorCode::InvalidInput with recoverable=false. Validate and decode strictly in DocumentContentResourcePort::relativePath before path conversion/normalization and before resource-pack acquisition, for both primary and optional export references. Keep NotFound for syntactically valid references whose resource is absent. Add focused tests using raw bytes that cannot be normalized through QString (including invalid continuation and truncated/malformed sequences); assert InvalidInput, nonrecoverable status, and documents pack remains unmounted for primary and export failures. Existing valid resolution, missing-resource NotFound, NUL, scheme, and traversal behavior must remain unchanged. Focused target and exact CTest: ClassMngrNextPlatformDocumentContentResourcePortTests. Matrix was recorded before implementation; implementation and independent verification are complete.


## 2026-10-09 - F436 invalid UTF-8 accepted

The resource port now strictly decodes raw reference path bytes with stateless QStringDecoder before normalization and before acquiring the resource pack. Invalid primary or export UTF-8 returns InvalidInput with recoverable=false. Tests cover six malformed primary sequences and a malformed export sequence, assert the error and no mounted pack at return, and retain valid resolution, valid missing-resource NotFound, NUL, scheme, and traversal coverage. Executor and independent Tester passed target build, exact CTest 1/1, direct target QtTest 9/9, and selected case 3/3. The Ninja Debug build used an initialized VS x64 environment after an initial missing-type_traits failure; final runs had no compiler warnings. No full-suite run. F436 is committed as c19e247f8066b546bfb71e9177e2413123309112; branch is ahead by 21.


## 2026-10-09 - F436 committed; F437 selected

F436 committed as c19e247f8066b546bfb71e9177e2413123309112 (Phase2 - Reject invalid UTF-8 document references (F436)); branch is ahead by 21. F437 Report worker event-post failure with a zero-capacity queue is accepted and ready to commit. F438 Optional typed occurrence IDs during repeat-series creation is selected/current; read-only context discovery is underway. Batch 21 remains active through F438; Batch 22 is discovered but inactive. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F437 report worker event-post failure matrix recorded

F437 adds focused coverage in tests/next_platform_qt_job_worker_tests.cpp using ReportJobCoordinator with a zero-capacity ReportJobEventQueue and an immediately failing report callback. The callback Failed terminal event is the first attempted post and is rejected with Conflict. Assert the worker lastResult carries the post Conflict (not the callback work error), event queue stays empty, worker finishes within the established timeout and is joined, and coordinator snapshot remains Running because pump received no event. Preserve analogous Import worker behavior and existing Report success/progress/failure cases. Build target and exact CTest: ClassMngrNextPlatformQtJobWorkerTests. Batch 22 candidate discovery is underway because F437 is the second-last Batch 21 slice. Matrix was recorded before implementation; implementation and independent verification are complete.


## 2026-10-09 - Batch 22 bounded discovery recorded while F437 is current

Two independent reviews and a reconciliation surfaced eight provisional candidates for Batch 22: accept Delete Teacher confirmation through MainWindow (ClassMngrMainWindowTeacherSidebarNavigationParityTests); complete Export Classes JSON output (ClassMngrMainWindowCloseFileParityTests); apply Import Classes through its QAction with a create-only package (same target); apply Import Teachers through its QAction (ClassMngrMainWindowManageCampusesParityTests); exercise Delete Class through MainWindow QAction (ClassMngrMainWindowScheduleTestingClassesHandoffParityTests); replace an already-open profile through successful Open File QAction selection (ClassMngrMainWindowOpenFileParityTests); trigger MainWindow Save and verify persisted data (ClassMngrMainWindowSaveAsExportParityTests); and choose Save during window close (ClassMngrMainWindowExitConfirmationParityTests). F435 already covers successful Open from the no-database banner, so a generic open-success candidate is redundant. Same-path open remains held because idempotence/replacement semantics are undefined; F285 successful New Teacher remains deferred. F437 is committed; F438 is current in active Batch 21. Batch 22 is discovered but remains inactive until F438 commits. The bounded reviews found eight candidates, not repository-wide exhaustion.


## 2026-10-09 - F437 report worker event-post failure accepted

The added case uses an immediately failing callback and a zero-capacity report queue, so the terminal Failed-event post is the first attempt. The queue returns Conflict, which becomes worker lastResult instead of the callback error. The worker exits within the timeout and is joined; the queue remains empty, pump consumes zero events, and coordinator snapshot remains Running. This mirrors the established Import worker contract without claiming coordinator recovery when no event was delivered. Executor and independent Tester verified the final source in the Ninja Debug tree: target build passed, exact CTest 1/1, target QtTest 12/12, selected case 3/3. The standard VS build tree hit FileTracker access errors; final Ninja build had no warnings. No production/CMake change or full-suite run. F437 is committed as f355a1aa65d63bbf18ddeda3e8cfa7ff76c985d7; branch is ahead by 22.


## 2026-10-09 - F437 committed; F438 selected

F437 committed as f355a1aa65d63bbf18ddeda3e8cfa7ff76c985d7 (Phase2 - Cover Report worker terminal event-post failure (F437)); branch is ahead by 22. F438 Optional typed occurrence IDs during repeat-series creation is selected/current; its acceptance matrix is recorded and implementation is pending. Batch 21 remains active through F438; Batch 22 has eight provisional candidates but remains inactive. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits.


## 2026-10-09 - F438 occurrence-ID contract matrix recorded

Define repeat-series creation as create-only. Any occurrence with a present CalendarEventId is invalid: CalendarEventSaveRequest.id selects an update, the repeat-series create request represents new rows, the normal planner clears seed IDs, and create-only calendar import rejects present IDs. Add a request validation rule in src/next/application/calendar_event_series_create_port.h that rejects any occurrence ID as InvalidInput with recoverable=false before persistence. In tests/next_platform_application_services_calendar_event_port_tests.cpp, seed an existing calendar row, build a multi-occurrence create request with that row ID on a later occurrence, and assert request/port validation fails, the existing row fields and repeat_series_id remain unchanged, and row count does not increase. Existing ID-less series creation/generated IDs and persistence remain unchanged. Reusing a standalone row as the first series occurrence is outside this request contract and would need a separate operation. Build target and exact CTest: ClassMngrNextPlatformApplicationServicesCalendarEventPortTests. Matrix recorded before implementation; implementation has not started. Batch 22 remains inactive until F438 commits.


## 2026-10-09 - F438 occurrence-ID rejection accepted

Repeat-series creation is create-only. The request boundary rejects any present occurrence CalendarEventId as nonrecoverable InvalidInput before persistence, preventing an existing standalone row from being updated or a partial series from being inserted. The focused integration case seeds an existing row, supplies its typed ID on a later occurrence, checks request and adapter errors, compares the full row including repeatSeriesId, and verifies row count is unchanged; ID-less generated series creation remains valid. Executor and independent Tester passed the Ninja Debug target build, exact CTest 1/1, direct target QtTest 55/55, and selected case 3/3. No compiler warnings or full-suite run. F438 is accepted and ready to commit. Batch 21 completes with this commit; Batch 22 begins with F439 Accept Delete Teacher confirmation.


## 2026-10-09 - F438 committed; Batch 21 complete

F438 committed as 0c2ceca6136a962f41130f52975bbab7315cd465 (Phase2 - Reject existing IDs in repeat-series creation (F438)); branch is ahead by 23. The commit contains the source, focused test, and three plan docs plus the two main progress records; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits. Batch 21 is complete. Batch 22 is active with F439 Accept Delete Teacher confirmation success selected/current; bounded read-only discovery is underway. F440-F446 remain provisional.


## 2026-10-09 - F439 Delete Teacher success matrix recorded

F439 adds the successful counterpart to F434 in ClassMngrMainWindowTeacherSidebarNavigationParityTests, using the actual MainWindow deleteTeacher QAction with no teacher selected. Seed two unassigned teachers, keep MyWorkspace active, and route the real chooser through the existing QTimer selector plus five-second modal watchdog to choose the target. Script PromptChoice::Destructive and verify one correctly configured “Delete Teacher” confirmation is consumed with no warning/message prompt. Assert the target is absent from the database and teacher sidebar, the survivor remains unchanged, the no-teacher-selection state and MyWorkspace page/widget remain stable, and the open session/path are unchanged. Do not cover assigned-class cleanup or deletion from a selected TeacherInfo page. The direct-controller success test already covers lower-level deletion; F439 specifically covers QAction + chooser integration. Build target and exact CTest: ClassMngrMainWindowTeacherSidebarNavigationParityTests. Matrix recorded before implementation; no implementation has started.


## 2026-10-09 - F439 Delete Teacher QAction success accepted

The new MainWindow case uses the enabled deleteTeacher QAction, selects a target in the actual chooser, and accepts the destructive confirmation. It verifies the confirmation request was consumed with no extra warning/message, the target is absent from repository and sidebar, the unassigned survivor record/sidebar leaf remain unchanged, no teacher row is selected, and MyWorkspace/page/widget/service/session/path remain stable. Executor and independent Tester passed the Ninja Debug target build, exact CTest 1/1, direct target QtTest 7/7, and selected case 3/3. No compiler warnings; offscreen Qt/font notices only. No full-suite run or production/CMake change. F439 is accepted and ready to commit. F440 Complete Export Classes JSON output is next after this commit; Batch 22 remains active.


## 2026-10-09 - F439 committed; F440 selected

F439 committed as a0c50d2dd37de11a4bfa91cfea044822cba7269f (Phase2 - Cover Delete Teacher QAction confirmation success (F439)); branch is ahead by 24. The six-path commit contains the test-only source change and accepted progress records; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits. Batch 22 remains active. F440 Complete Export Classes JSON output is selected/current and bounded read-only discovery is underway; F441-F446 remain provisional.


## 2026-10-09 - F440 Export Classes JSON success matrix recorded

F440 adds a successful companion to F432 in MainWindowCloseFileParityTests. Reuse the real MainWindow exportClasses QAction and ClassExportDialog observer, whose 10 ms poll and five-second watchdog reject a stuck modal; assert dialog observed, selected IDs contain only the target, Export clicked, and neither timeout nor fallback rejection occurred. Seed one teacher and two classes, select only the target, and script a temporary .json save path. Assert the file exists and parses; root format is ClassMngr Classes, version is 1, exported_at_utc parses as UTC, and classes contains only the selected class with its class_grade/class_level and teacher_ref linked to the single exported teacher profile. Do not compare the variable timestamp or treat generated package keys as database IDs. Assert one successful Export Classes information prompt mentions count/path, no warning, and MyWorkspace/session/path and persisted class records remain unchanged. Existing codec tests own exhaustive payload schema coverage; F440 proves the QAction/dialog/output integration. Exact target/CTest: ClassMngrMainWindowCloseFileParityTests. Matrix recorded before implementation; no implementation has started.


## 2026-10-09 - F440 Export Classes JSON success accepted

The new MainWindow case uses the real Export Classes QAction and dialog observer, verifies both seeded classes are listed but only the target is selected, and writes through the scripted temporary JSON picker. Parsed output has format ClassMngr Classes, version 1, a valid UTC timestamp, exactly the selected class with its info, and a teacher_ref linked to the exported teacher; the unselected class is absent. The success information prompt has the expected count/path and no warning. Workspace page/session/path, class names/count/table rows, and full class-info snapshots are unchanged. Executor and independent Tester passed Ninja Debug target build, exact CTest 1/1, target QtTest 9/9, and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full suite or production/CMake change. F440 is accepted and ready to commit. F441 Import Classes QAction apply is next after this commit; Batch 22 remains active.


## 2026-10-09 - F440 committed; F441 selected

F440 committed as 01d1559caae48eddcda739f4ea6f30dba8667e24 (Phase2 - Cover Export Classes JSON output (F440)); branch is ahead by 25. The six-path commit contains the test-only source change and accepted progress records; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits. Batch 22 remains active. F441 Import Classes QAction apply with a create-only fixture is selected/current and bounded read-only discovery is underway; F442-F446 remain provisional.


## 2026-10-09 - F441 Import Classes create-only matrix recorded

F441 adds successful apply coverage beside F426 in MainWindowCloseFileParityTests. Use an empty destination database and a valid one-class JSON package with no teachers and an empty teacher_ref; this avoids teacher import and replacement semantics. Trigger the actual MainWindow importClasses QAction, script one openFile selection for FileDialogPurpose::ClassTransfer/JSON, then automate the real ClassImportDialog with a timer and five-second modal watchdog. Assert the default Create class choice and Import button, dialog acceptance, and no timeout/fallback. Verify exactly one class is created with expected class-info values, zero teachers are imported and the class remains unassigned, with summary Created: 1, replaced: 0, skipped: 0 and no warning. Assert PageType::Classes shows the imported class selected in ClassesSection::Details, sidebar route classes, and database path/session stable. Target/CTest: ClassMngrMainWindowCloseFileParityTests. Matrix recorded before implementation; no implementation has started.


## 2026-10-09 - F441 Import Classes QAction apply accepted

The test drives the real Import Classes QAction, scripted ClassTransfer JSON picker, real ClassImportDialog, explicit Create choice, and Import apply from an empty destination using a one-class package with no teachers. It verifies one new class and full expected ClassInfo, zero teacher rows with teacherId -1, exact Created: 1/replaced: 0/skipped: 0 information summary, no other prompt, Classes page with imported ID/Details active, sidebar route classes, and stable session/path. Executor and independent Tester passed Ninja Debug target build, exact CTest 1/1, target QtTest 10/10, and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full-suite run or production/CMake change. F441 is accepted and ready to commit. F442 Import Teachers QAction apply is next after this commit; Batch 22 remains active.


## 2026-10-09 - F441 committed; F442 selected

F441 committed as 75a559ae7bad175cba106d0377bcf59c506cd247 (Phase2 - Cover Import Classes QAction apply success (F441)); branch is ahead by 26. The six-path commit contains the test-only source change and accepted matrix/results; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside slice commits. Batch 22 remains active. F442 Import Teachers QAction apply is selected/current and bounded read-only discovery is underway; F443-F446 remain provisional.


## 2026-10-09 - F442 Import Teachers MainWindow success matrix recorded

Add a successful Import Teachers case to MainWindowManageCampusesParityTests, complementing F431’s page-leave gate. Use an empty temporary database and clean MyWorkspace, plus checked-in tests/fixtures/teacher_import/sectioned_review.xlsx. Browse through the real TeacherImportDialog using one OpenFileRequest for purpose ImportWorkbook and Excel Workbooks (*.xlsx). Allow asynchronous validation with a bounded 15-second watchdog; assert Valid File and Import enabled, then set M1 to Select candidate 0, M2 to None, and H1 to All before clicking Import. This yields 2 Korean teachers (Hong/Park), 1 Native English teacher (Alex), and 1 GS Team member (Taylor) with no prior records/date, avoiding update or old-date confirmation. Assert exact created/updated/unchanged summary counts, new database category records and refreshed Korean sidebar entries, one Import Teachers information prompt and no warning/confirmation; current workspace page, session, and path remain stable. Target/CTest: ClassMngrMainWindowManageCampusesParityTests. Matrix recorded before implementation; no implementation has started.


## 2026-10-09 - F442 Import Teachers MainWindow apply accepted

The new case browses the checked-in XLSX through the real MainWindow QAction and TeacherImportDialog. It waits for async validation, applies M1 candidate 0 / M2 None / H1 All from a clean workspace, and verifies 2 Korean records (Hong/Park), 1 Native English (Alex), and 1 GS Team member (Taylor), all created with no updates/unchanged. It checks category table counts, Korean sidebar IDs/labels, source date, exact success summary, no warning/date confirmation, and stable MyWorkspace/session/path. Executor and independent Tester passed Ninja Debug target build, exact CTest 1/1, target QtTest 7/7, and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full-suite run or production/CMake change. F442 is accepted and ready to commit. The user asked to pause after this commit; F443 Delete Class QAction is next on resume, but no F443 discovery or implementation has started.


## 2026-10-09 - F442 committed; resumed at F443

F442 committed as 8cde28263d3c89cc5f3f610d031a1a536ad183d4 (Phase2 - Cover Import Teachers QAction apply success (F442)); branch is ahead by 27. The six-path commit contains the test-only source change and accepted records; diff checks passed. latest_session_work.md and unrelated %SystemDrive%/ remain outside commits. The user has resumed after the requested pause. Batch 22 remains active; F443 Delete Class QAction is selected/current and bounded read-only discovery is underway. F444-F446 remain provisional.


## 2026-10-09 - F443 Delete Class MainWindow success matrix recorded

Add a MainWindow integration case to MainWindowScheduleTestingClassesHandoffParityTests for the real deleteClass QAction. Preseed two unassigned classes before opening a clean Classes details page, with the target owning ClassInfo and one schedule row and the sibling retained as fallback. Trigger the QAction, use the real sidebarRecordSelectionDialog to choose the target via the combo/button with a five-second modal watchdog, and script PromptChoice::Destructive for the actual Delete Class confirmation. Assert the exact confirmation is consumed, no warning/unsaved prompt appears, target class and its ClassInfo/schedule rows are removed, sibling class/info remain unchanged, the Classes page remains current with sibling selected in Details, sidebar key is classes, and session/path stay active. No teacher assignment behavior is in scope. Direct-controller tests already cover success/fallback; F443 adds the QAction/chooser route. Target/CTest: ClassMngrMainWindowScheduleTestingClassesHandoffParityTests. Matrix recorded before implementation; no implementation has started.


## 2026-10-09 - F443 Delete Class QAction success accepted

F443 committed as `1c03b326567cf52d808bc4c54b7a5e77021bb7bf`; branch is ahead by 28.

The new MainWindow test triggers the real deleteClass QAction, selects the target through the actual chooser, and accepts the destructive confirmation. It verifies target class/ClassInfo/schedule removal, sibling record/info unchanged and active in Classes Details, Classes sidebar route, stable page/session/path, and no warning or unsaved-change prompt. No teacher assignments or cross-page navigation are asserted. Executor and independent Tester passed the target build, exact CTest 1/1, direct target QtTest 12 passes (10 test cases plus setup/cleanup), and selected case 3/3. No compiler warnings; Qt font/offscreen notices only. No full-suite run or production/CMake change. F443 is accepted and ready to commit. F444 successful Open File QAction replacement is next after this commit; Batch 22 remains active.

## 2026-10-09 - F444 Open File replacement acceptance review

F444 committed as `8a21da618870ba4308415aaf5927fa1393af6e97`; branch is ahead by 29.

Two independent read-only reviews approve F444 for implementation. Existing coverage tests Open File Cancel and Save followed by picker cancellation while a profile is open; F435 tests the empty-state banner path. F444 covers the distinct successful replacement transition.

Acceptance: Start with a clean open profile A on My Workspace Schedule, trigger the real Open File QAction, and choose a distinct seeded profile B through the Teacher Profile picker. Verify one correctly configured chooser request, profile B's path and persisted data active, My Workspace Schedule and its Sidebar selection, and no warning or unsaved-changes prompt. `DatabaseSession` is reused in place, so assert active path/data rather than pointer identity. Same-path open, dirty replacement choices, and load failures remain outside scope.

Source map: `MainWindow::connectControllers()` -> `FileController::openFile()` -> `WorkspaceCoordinator::openWorkspace()` -> `MainWindow::applyDatabaseLoadedState()`. Focused test class/target: `MainWindowOpenFileParityTests` / `ClassMngrMainWindowOpenFileParityTests`. F444 is accepted and ready to commit.

Verification: target build passed with no compiler warnings; the selected QtTest slot passed 3/3 functions (setup, test, cleanup); exact filtered CTest `ClassMngrMainWindowOpenFileParityTests` passed 1/1. The final source review approved the Schedule starting state and assertions. Independent Tester could not inspect or run commands because process creation failed with `helper_unknown_error: setup refresh had errors`. One missing Qt font-directory runtime warning; no full suite.

## 2026-10-09 - F445 Save QAction persistence acceptance review

F445 committed as `d9180f1c465976dfdd707382a1e615108aed9387`; branch is ahead by 30.

Two independent read-only reviews support F445. The local source map found no MainWindow Save QAction persistence case in `MainWindowSaveAsExportParityTests`; a separate scope review found service-level persistence coverage only, which does not test the QAction integration. The scope review used a default-branch snapshot because local process setup failed, so local target details come from the source map.

Acceptance: Start with a clean file-backed profile open on My Workspace Schedule and a nonempty current path. Begin an explicit transaction on the active DB connection, write a distinctive setting value, then trigger the real Save QAction. Verify the value persisted through a fresh connection/reopened profile, the path/page/sidebar stay stable, and no Save As picker, warning, or unsaved prompt appears. A transaction is required because an ordinary setting write auto-commits. Exclude Save As, commit-failure behavior, and claims about error reporting; the save path ignores the commit result.

Source map: MainWindow Save QAction -> `FileController::saveFile()` -> `saveDatabase()` / workspace commit path. Focused class/target: `MainWindowSaveAsExportParityTests` / `ClassMngrMainWindowSaveAsExportParityTests`. F445 is accepted and ready to commit; F446 remains provisional.

Verification: target build passed without compiler warnings; selected QtTest passed 3/3 functions, full target QtTest passed 8/8 functions, and exact filtered CTest `ClassMngrMainWindowSaveAsExportParityTests` passed 1/1. Independent source review approved the transaction, real QAction, fresh-connection readback, and UI-state assertions. `SettingsService::save()` stalled before the action, so the fixture stages the unique value with a prepared `QSqlQuery` on the same active connection inside the transaction; Save commits it and a fresh `ApplicationServices` reads it. One missing Qt font-directory runtime warning; no full suite or production/CMake changes.

## 2026-10-09 - F446 Close-confirmation Save acceptance review

F446 committed as `4b34a3b8a15a062377a245607228097fb43ee46b`; branch is ahead by 31.

Two independent reviews support F446. The local source map confirmed the existing exit test covers Cancel then Discard, while the supplied-source scope review identified Save-on-close as a distinct path from F445's File → Save QAction. The scope review could not access local files due process setup failure.

Acceptance: In a file-backed profile, persist a known baseline personal name, use Manual Save, then edit it to a distinct draft name in My Details. Script the fake unsaved-changes prompt as Save and call `window.close()` through the real `closeEvent`. Verify the Save choice was issued, close was accepted and the window is hidden, with no second prompt. Reopen through fresh `ApplicationServices` and verify the exact draft name replaced the baseline. Manual mode prevents autosave races. Keep the existing Cancel/Discard case; exclude File → Save QAction behavior, Save As, save failure, and unrelated fields.

Source map: `MainWindow::closeEvent()` -> `confirmCurrentPageCanLeave(true)` -> PageManager prompt -> current page `saveChanges()`; My Details saves through `saveMyInfoInternal()`. Focused class/target: `MainWindowExitConfirmationParityTests` / `ClassMngrMainWindowExitConfirmationParityTests`. F446 is accepted and ready to commit.

Verification: target build passed without compiler warnings; selected QtTest passed 3 functions (setup, slot, cleanup); full target QtTest passed 4 functions (two slots, setup, cleanup); exact filtered CTest passed 1/1. Independent source review approved the baseline-to-draft flow, Save choice, normal close, and fresh-service persistence check. One Qt font-directory warning; no full suite or production/CMake changes. The first wrapped CTest regex invocation found no tests because cmd.exe retained the quotes; rerunning with the regex as one direct argument passed.

## 2026-10-09 - F447 New Class QAction acceptance review

F447 committed as `d5b130bd0558146185ebf4cdeab885bde6956fec`; branch is ahead by 32.

Two independent local read-only reviews identified the top-level New Class QAction as an uncovered MainWindow route. The existing `classesRootContextMenuAddClassCreatesAndOpensClass()` case covers the separate Sidebar context-menu action, while the QAction and context menu share `SidebarController::addClass()`.

Acceptance: From a clean open file-backed profile on My Workspace, assert `window.actions().newClass` exists and is enabled, then trigger the real ActionRegistry QAction. Verify one new persisted class is selected and opened on Classes Details, Classes is selected in the Sidebar, the current database session/path remain stable, and no unexpected prompt or warning occurs. This adds MainWindow action wiring coverage around existing creation behavior.

Source map: `ActionRegistry::newClass` -> Classes menu -> `SidebarController::addClass()`; handler creates the class, opens Details, selects Classes. Focused class/target: `MainWindowClassesSidebarRootNavigationTests` / `ClassMngrMainWindowClassesSidebarRootNavigationTests`. Exclude Sidebar context-menu behavior, creation/read failures (deferred F298), dirty-page confirmation, no-database behavior, and unrelated class fields. F447 is committed as `d5b130bd0558146185ebf4cdeab885bde6956fec`.

Verification: target build passed without compiler warnings; selected QtTest passed 3 functions (setup, slot, cleanup); full target QtTest passed 5 functions (three slots, setup, cleanup); exact filtered CTest passed 1/1. Independent source review approved the action wiring, class-count/persisted-ID check, Details route, and Sidebar/session/path assertions. One missing Qt font-directory warning and offscreen `raise()`/keyboard-grab notices occurred; tests passed. No full project suite or production/CMake change.

## 2026-10-09 - F448 New File QAction open-profile acceptance review

Two independent local reviews confirmed a distinct active-profile transition gap. F435 covers New Profile from the no-database banner, and F444 covers Open File replacement; neither covers New File closing an active profile and creating a new one through the MainWindow QAction. Existing FileController lifecycle tests cover creation without an open profile, not this MainWindow handoff.

Acceptance: Start with a clean file-backed profile A open on My Workspace Schedule, select a unique nonexistent `.tps` path B through the fake Save File picker, and trigger the real `window.actions().newFile`. Verify the picker is rooted at the active profile directory, B is created and active with an open database session, A remains on disk with its seeded name and campus unchanged, My Workspace Schedule and the `my_workspace` Sidebar selection are restored, and no prompt, warning, or modal remains. Do not compare session pointer identity.

Source map: `ActionRegistry::newFile` -> `FileController::newFile()` -> `confirmUnsavedChanges()` -> Save File picker -> `closeActiveDatabase()` -> `WorkspaceCoordinator::createWorkspace()` -> `MainWindow::applyDatabaseLoadedState()`. Focused class/target: `MainWindowCloseFileParityTests` / `ClassMngrMainWindowCloseFileParityTests`. Exclude dirty-page choices, picker cancellation, existing-target overwrite, Initial Setup, creation failures, and F435/F444 behavior. Acceptance matrix recorded before implementation; Batch 24 is active.

## 2026-10-09 - F448 New File QAction success accepted

The new MainWindow case triggers the real New File QAction from a clean open profile A, creates profile B at the selected destination, and checks that B becomes active on My Workspace Schedule with the `my_workspace` Sidebar route. It verifies A remains on disk with its seeded name and campus unchanged through a fresh `ApplicationServices` readback, and that no prompt, warning, or modal appears. F435 no-database creation and F444 Open File replacement remain separate coverage.

Verification: `ClassMngrMainWindowCloseFileParityTests` target build passed; the selected QtTest slot passed 3 incidents (setup, test, cleanup) with zero failures; filtered CTest passed 1/1; `git diff --check` is clean. Independent source review approved the strengthened persistence readback and active-session/UI checks, but its test commands could not launch due `helper_unknown_error: setup refresh had errors`. The executor ran the updated verification successfully. One existing Qt font-directory warning appeared. No full project suite or production/CMake change. F448 is accepted; commit metadata and next-slice discovery are recorded below.

## 2026-10-09 - F448 committed; F449 discovery started

F448 committed as `f825a388db1897bc42cacf43c488850fa48a9de8` (`Phase2 - Cover New File QAction open-profile success (F448)`); the branch is ahead by 33. Exactly the six approved paths were committed, the diff check was clean, and `agent_docs/latest_session_work.md` plus `%SystemDrive%/` remained outside. Batch 24 is complete. Batch 25 is active; two independent read-only reviews selected the Exit QAction close-confirmation handoff as F449 (acceptance matrix follows).

## 2026-10-09 - F449 Exit QAction close-confirmation acceptance review

Two independent read-only reviews found that `ActionRegistry::exitApp` is created and added to the File menu but has no project-level signal connection. Existing close-confirmation tests exercise `window.close()` directly and do not reference `exitApp`, so the menu action does not reach the guarded close path.

Acceptance: Seed a file-backed profile with a persisted baseline personal name, open it cleanly in Manual Save mode, and change the name editor to a distinct unsaved draft. Script the close choices as Cancel then Discard, and trigger the real `window.actions().exitApp` each time. After Cancel, verify the window remains visible, the draft remains unsaved, the active session/path remain, and the close prompt was recorded. After Discard, verify the window is hidden, the second prompt was consumed, and fresh-service readback still returns the baseline name.

Source map: `ActionRegistry::exitApp` -> File menu -> `MainWindow::close()` -> `MainWindow::closeEvent()` -> `confirmCurrentPageCanLeave(true)`. Production connection belongs in MainWindow action/controller wiring. Focused class/target/CTest: `MainWindowExitConfirmationParityTests` / `ClassMngrMainWindowExitConfirmationParityTests`. Exclude F446 Save behavior, OS shutdown, and multi-window behavior. Matrix recorded before implementation. F449 acceptance is recorded; commit metadata and next-slice discovery follow.

## 2026-10-09 - F449 Exit QAction success accepted

The File menu Exit QAction now calls `MainWindow::close()`, so it follows the existing `closeEvent` unsaved-changes guard. The new test drives Cancel then Discard through the QAction: Cancel keeps the dirty draft, active session/path, page, and window; Discard hides the window, and a fresh-service readback confirms the persisted baseline remains unchanged. Existing direct close tests and F446 Save coverage remain intact.

Verification: Ninja target build passed; the exact QtTest slot passed 3 incidents (setup, test, cleanup); full target QtTest passed 5 incidents (three test slots plus setup/cleanup); filtered CTest passed 1/1; `git diff --check` is clean. An MSBuild attempt hit an environment FileTracker access-denied error, so verification used the existing Ninja build with initialized MSVC. Independent Tester reran the slot, full target, and filtered CTest successfully and approved the source scope. Qt reported the known missing-font-directory warning and LF-to-CRLF notices; no whitespace errors. No full project suite or CMake changes. F449 acceptance is recorded; commit metadata and next-slice discovery follow.

## 2026-10-09 - F449 committed; F450 discovery started

F449 committed as `4e42a5a9cbe5c261fb78b2e39a98f81c93ef0372` (`Phase2 - Cover Exit QAction close-confirmation handoff (F449)`); the branch is ahead by 34. Exactly the seven approved paths were committed and the diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 25 is complete; Batch 26 is active with bounded read-only QAction coverage discovery underway. No F450 candidate is selected yet.

## 2026-10-09 - F450 Undo QAction focused-editor acceptance review

Two independent read-only reviews found no direct MainWindow Edit QAction coverage. Existing local QUndoStack/component tests do not exercise ActionRegistry wiring or focus-based dispatch through the MainWindow. Undo is a bounded, deterministic path through `EditController` and a personal-details `QLineEdit`.

Acceptance: Seed a file-backed profile with a persisted baseline personal name, start MainWindow on My Workspace Details, focus the name editor, and change the text with keyboard events (`Ctrl+A` plus QTest key typing) so QLineEdit records undo history. Do not call `setText` for the draft. Verify the real `window.actions().undo` is enabled, trigger it, and verify the editor returns to the baseline.

Source map: `ActionRegistry::undo` -> Edit menu -> `MainWindow::connectControllers()` / `EditController::connectActions()` -> focus-based `EditController::undo()` dispatch -> `QLineEdit::undo()`. Dedicated test/target/CTest: `mainwindow_edit_action_parity_tests.cpp` / `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, registered in `cmake/tests/pages_and_output.cmake`. Exclude Redo, clipboard actions, read-only widgets, and other editor types. Matrix recorded before implementation; Batch 26 is active.

## 2026-10-09 - F450 Undo QAction success accepted

The dedicated MainWindow test seeds a file-backed baseline personal name, opens My Workspace Details, focuses the name editor, enters a draft through Ctrl+A and QTest keyboard input, confirms the real Undo QAction is enabled, triggers it, and verifies the baseline value is restored. The test covers EditController focus dispatch to QLineEdit undo; no repository write is expected from this local editor operation. Independent source review approved the test and target registration.

Verification: the Ninja target build passed (the independent rerun reported no work to do); the exact QtTest slot and full target each passed 3 incidents (setup, test, cleanup) with zero failures; filtered CTest passed 1/1 using the generated registration `ClassMngrMainWindowEditActionParityTests`; `git diff --check` is clean. Both direct test runs emitted the existing Qt missing-font-directory warning, and bundled Inter/Pretendard fonts loaded. Initial MSVC attempts needed CMake regeneration and `VsDevCmd` initialization; the final executor and independent runs passed with that environment. No full project suite or production-code changes. F450 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F450 committed; F451 discovery started

F450 committed as `e39852c8a9cef33c80d684dd0e63e18d19a6acf7` (`Phase2 - Cover Undo QAction focused-editor dispatch (F450)`); the branch is ahead by 35. Exactly the seven approved files were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 26 is complete. Batch 27 is active; two independent read-only reviews are beginning bounded MainWindow QAction coverage discovery for F451. No candidate is selected yet.

## 2026-10-09 - F451 Redo QAction focused-editor acceptance review

Two independent reviews found no direct MainWindow Redo QAction coverage. Existing update-controller tests do not exercise Edit action focus dispatch, and the committed F450 test verifies only Undo. Redo is a bounded continuation of the same focused QLineEdit path in EditController.

Acceptance: In the file-backed profile and My Workspace Details setup used by `MainWindowEditActionParityTests`, focus the personal-name editor, and create a distinct draft using keyboard input (`Ctrl+A` and QTest key typing). Trigger the real Undo QAction and verify the baseline value returns. Then verify the real Redo QAction is available, trigger it, and verify the draft value returns while the editor remains focused. Keep the operation local to the editor; do not call `setText` for the draft.

Source map: `ActionRegistry::redo` -> Edit menu -> MainWindow/EditController wiring -> focus-based dispatch -> `QLineEdit::redo()`. Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add the focused Redo behavior to `mainwindow_edit_action_parity_tests.cpp`. Exclude Cut/Copy/Paste, read-only widgets, other editor types, and update-controller behavior. Matrix recorded before implementation; Batch 27 is active.

## 2026-10-09 - F451 Redo QAction success accepted

The existing focused MainWindow edit-action case now covers the paired path: after keyboard-entering a distinct draft in the focused personal-name QLineEdit, it triggers the real Undo QAction and verifies the persisted baseline value returns, then triggers the real Redo QAction and verifies the draft returns while the editor retains focus. The test slot is `undoAndRedoActionsRestoreAndReapplyPersonalNameInFocusedLineEdit`; this extends the F450 focused-editor test without production or CMake changes.

Verification: the Ninja target build passed; the exact slot passed 3 incidents (setup, test, cleanup) with zero failures, and verbose output confirmed the slot ran; full target QtTest passed 3 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. The direct runs emitted Qt’s known missing-font-directory warning; bundled Inter and Pretendard fonts loaded. No full project suite. F451 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F451 committed; F452 discovery started

F451 committed as `3137d522796365e81d4c3f99e341aacaf83cc392` (`Phase2 - Cover Redo QAction focused-editor dispatch (F451)`); the branch is ahead by 36. Exactly the six approved files were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 27 is complete. Batch 28 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F452. No candidate is selected yet.

## 2026-10-09 - F452 Paste QAction focused-editor acceptance review

Two independent reviews found no direct MainWindow Cut/Copy/Paste QAction tests. Paste is selected for its distinct clipboard-dependent enablement path and dispatch to the focused editor.

Acceptance: In `MainWindowEditActionParityTests`, snapshot the current clipboard MIME data and restore it during cleanup, including early assertion exits. Open the file-backed profile on My Workspace Details and focus the personal-name QLineEdit. Clear clipboard content and verify the real Paste QAction is disabled; then set non-empty text, wait for the QAction to enable, select the editor content using Ctrl+A, trigger the actual Paste QAction, and verify the editor contains the clipboard text and retains focus. Avoid `setText` for the edit operation.

Source map: ActionRegistry::paste -> Edit menu -> MainWindow/EditController wiring -> clipboard-change/focus action-state refresh -> focused QLineEdit::paste(). Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add coverage in `mainwindow_edit_action_parity_tests.cpp`. Exclude Cut/Copy, read-only gating, non-text clipboard formats, and other editor types. Matrix recorded before implementation; Batch 28 is active.

## 2026-10-09 - F452 Paste QAction success accepted

Added `pasteActionReplacesSelectedPersonalNameFromClipboard()` to the existing MainWindow edit-action target. The test snapshots clipboard MIME formats and payloads in an RAII restorer, opens a file-backed profile on My Workspace Details, focuses the personal-name QLineEdit, verifies Paste is disabled with empty clipboard content, sets clipboard text and waits for the action to enable, selects the existing name with Ctrl+A, triggers the actual Paste QAction, and verifies replacement text and retained focus. The clipboard is restored on normal and assertion-failure exits. No production or CMake changes.

Verification: Ninja target build passed; the exact slot passed 3 incidents (setup, test, cleanup) with zero failures; full target QtTest passed 4 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. Independent verification captured the selected-slot output in a temp log after initializing VS 18 and Qt 6.12 paths. Qt emitted the known missing-font-directory warning; bundled Inter and Pretendard fonts loaded. No full project suite. F452 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F452 committed; F453 discovery started

F452 committed as `249dd38b8b3ee2223292c99ec740f2470e20e5e9` (`Phase2 - Cover Paste QAction focused-editor dispatch (F452)`); the branch is ahead by 37. Exactly the six approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 28 is complete. Batch 29 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F453. No candidate is selected yet.

## 2026-10-09 - F453 Cut QAction focused-editor acceptance review

Two independent reviews found no MainWindow Cut or Copy QAction coverage after the Undo/Redo/Paste cases. Select Cut as a focused text-edit action with a direct clipboard result.

Acceptance: Add a dedicated Cut slot to `MainWindowEditActionParityTests`, reusing the file-backed personal-name editor setup and `ClipboardMimeDataRestorer`. Open My Workspace Details, focus the personal-name QLineEdit, seed a distinct sentinel clipboard value, select the existing name with Ctrl+A and verify it is the selected text. Confirm the real Cut QAction is enabled, trigger it, and verify the editor is empty, clipboard text now equals the selected baseline name, focus remains in the editor, and the active workspace stays open without a prompt. Do not use `setText` for the edit.

Source map: ActionRegistry::cut -> Edit menu -> MainWindow/EditController wiring -> focused-widget dispatch with editable-text guard -> QLineEdit::cut(). Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add the focused Cut case to `mainwindow_edit_action_parity_tests.cpp`. Exclude Copy, read-only gating, other editor types, persistence assertions, and non-text clipboard formats. Matrix recorded before implementation; Batch 29 is active.

## 2026-10-09 - F453 Cut QAction success accepted

Added `cutActionCopiesSelectedPersonalNameFromFocusedEditor()` to the existing MainWindow edit-action test. It seeds a file-backed baseline, snapshots/restores clipboard MIME data, focuses the personal-name QLineEdit in My Workspace Details, puts sentinel text on the clipboard, selects the baseline using Ctrl+A, and triggers the real enabled Cut QAction. Assertions verify the editor clears, the clipboard now contains the selected name, focus remains, the workspace stays open on Details, and no modal prompt appears. Manual Save mode keeps the intentionally empty field local during the test. No `setText` editing, production or CMake changes.

Verification: Ninja target build passed; the exact Cut slot passed 3 incidents (setup, test, cleanup) with zero failures; full target QtTest passed 5 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. Independent rerun used the CTest offscreen Qt environment and captured both QtTest logs. The first direct launch without Qt runtime settings failed to start; after setting VS 18/Qt 6.12 paths and offscreen mode, the selected slot and full target passed. Qt emitted the known missing-font-directory warning; bundled Inter and Pretendard fonts loaded. No full project suite. F453 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F453 committed; F454 discovery started

F453 committed as `4e9b3d6d79aff3c43b943aec8e165a6ab16ca8a9` (`Phase2 - Cover Cut QAction focused-editor dispatch (F453)`); the branch is ahead by 38. Exactly the six approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 29 is complete. Batch 30 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F454. No candidate is selected yet.

## 2026-10-09 - F454 Copy QAction focused-editor acceptance review

Two independent reviews found no direct MainWindow Copy QAction coverage after Undo/Redo, Paste, and Cut. Copy is a bounded focused-widget action that should update the clipboard while preserving editor and workspace state.

Acceptance: Add a dedicated Copy slot to `MainWindowEditActionParityTests`, reusing the file-backed personal-name editor setup and `ClipboardMimeDataRestorer`. Open My Workspace Details, focus the personal-name QLineEdit, seed a sentinel clipboard value, select the baseline name using Ctrl+A, and verify the selection. Confirm the real Copy QAction is enabled, trigger it, then verify clipboard text equals the selected name while editor text, editor focus, active workspace/path, and Details route remain unchanged with no modal prompt. Restore clipboard MIME data on all exits. Exclude empty selection behavior, keyboard-shortcut dispatch, other editor types, and production/CMake changes.

Source map: ActionRegistry::copy -> Edit menu -> MainWindow/EditController wiring -> focused-widget dispatch -> QLineEdit::copy(). Test class/target/CTest remain `MainWindowEditActionParityTests` / `ClassMngrMainWindowEditActionParityTests`, already registered in `cmake/tests/pages_and_output.cmake`; add the focused Copy case in `mainwindow_edit_action_parity_tests.cpp`. Matrix recorded before implementation; Batch 30 is active.

## 2026-10-09 - F454 Copy QAction success accepted

Added `copyActionCopiesSelectedPersonalNameFromFocusedEditor()` to the existing MainWindow edit-action test. It snapshots clipboard MIME formats/payloads, opens a file-backed profile on My Workspace Details, focuses the personal-name QLineEdit, seeds sentinel clipboard text, selects the baseline with Ctrl+A, and triggers the real enabled Copy QAction. Assertions verify the clipboard receives the selection, the editor value/focus and selection stay unchanged, the workspace/path and Details route remain active, and no modal prompt appears. The test explicitly restores clipboard data and verifies the original formats and payload bytes; the RAII destructor remains a fallback on early assertion returns. No production or CMake changes.

Verification: Ninja target build passed; the exact Copy slot passed 3 incidents (setup, test, cleanup) with zero failures; full target QtTest passed 6 incidents; filtered CTest `ClassMngrMainWindowEditActionParityTests` passed 1/1; `git diff --check` is clean. Independent rerun confirmed the selection and clipboard restoration assertions. Qt emitted the known missing-font-directory warning; bundled fonts loaded. No full project suite. F454 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F454 committed; F455 discovery started

F454 committed as `dd73b6d9c536809f05d38487ecca4bc873b9ea86` (`Phase2 - Cover Copy QAction focused-editor dispatch (F454)`); the branch is ahead by 39. Exactly the six approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 30 is complete. Batch 31 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F455. No candidate is selected yet.

## 2026-10-09 - F455 About QAction modal-handoff acceptance review

Independent Help-action reviews found no About QAction test. Select a bounded modal-handoff case that verifies the actual Help QAction opens the parented AboutDialog.

Acceptance: In a dedicated offscreen `MainWindowAboutActionParityTests` target, isolate `CLASSMNGR_SETTINGS_ROOT` in a temporary directory because DialogShell persists geometry. Construct and show MainWindow without an update controller; verify the real About QAction exists and is enabled. Queue a zero-delay timer before triggering it; while the QAction’s modal `exec()` is active, capture that `QApplication::activeModalWidget()` is an AboutDialog, it is visible/modal and parented to the MainWindow, then close it so the trigger returns. Afterward verify the observations, MainWindow remains visible, and no modal widget remains.

Source map: ActionRegistry::about -> Help menu -> MainWindow action connection -> `AboutDialog dialog(this); dialog.exec()`. Test file/target/CTest: `mainwindow_about_action_parity_tests.cpp` / `MainWindowAboutActionParityTests` / `ClassMngrMainWindowAboutActionParityTests`, registered in `cmake/tests/pages_and_output.cmake` with the neighboring MainWindow Qt/resource setup. Exclude license buttons/content, external links, and Check for Updates/network/download behavior. Matrix recorded before implementation; Batch 31 is active.

## 2026-10-09 - F455 About QAction modal handoff accepted

Added `aboutActionShowsModalAboutDialogAndReturnsToMainWindow()` and the dedicated `MainWindowAboutActionParity` target/`ClassMngrMainWindowAboutActionParityTests` registration. The test isolates settings in QTemporaryDir, creates a MainWindow without an update controller, and triggers the actual enabled About QAction. A zero-delay timer captures the visible modal AboutDialog, confirms it is modal and parented to MainWindow, then closes it. A mismatch branch closes any active modal and child AboutDialog so the test reaches its assertions instead of hanging. After the trigger, assertions verify the observations, MainWindow visibility, and no remaining modal. No production changes.

Verification: Ninja target build passed; the exact QtTest slot and full target each passed 3 incidents (setup, test, cleanup) with zero failures; filtered CTest `ClassMngrMainWindowAboutActionParityTests` passed 1/1; `git diff --check` is clean. Independent rerun confirmed the exact slot and modal assertions. An initial direct shell launch hung during QApplication startup due to missing Qt runtime settings; the final run used VS 18/Qt 6.12 and offscreen settings. CMake emitted pre-existing missing-Vulkan and object-path warnings; Qt emitted the known missing-font-directory warning while bundled Inter/Pretendard fonts loaded. No full project suite. F455 is accepted; commit metadata and next-slice discovery follow.

## 2026-10-09 - F455 committed; F456 discovery started

F455 committed as `76c67663cda5604353b4ba25e54c97160f6daf90` (`Phase2 - Cover About QAction modal handoff (F455)`); the branch is ahead by 40. Exactly the seven approved paths were committed and the commit diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 31 is complete. Batch 32 is active; two independent read-only reviews are beginning bounded QAction coverage discovery for F456. No candidate is selected yet.

## 2026-10-09 - F456 Check for Updates QAction acceptance scope

Cover the real enabled `window.actions().checkForUpdates` QAction and its handoff to a visible, manual `UpdateDialog`. Inject an empty `UpdateConfiguration` so checking fails deterministically before network access. Assert exactly one `GitHub releases API URL is not configured.` failure, a visible MainWindow-parented dialog with `automaticUpdatePrompt == false`, the expected failure title/details and enabled “Try Again” button, and that MainWindow remains visible. Isolate startup cleanup by setting `TMP`, `TEMP`, and `TMPDIR` to a dedicated CTest build-directory root and asserting `QStandardPaths::TempLocation` resolves there before triggering the action; create that private root before constructing the settings `QTemporaryDir`. No production changes or successful online update behavior are in scope.

## 2026-10-09 - F456 Check for Updates QAction accepted

Added `mainwindow_check_for_updates_action_parity_tests.cpp` and the dedicated `MainWindowCheckForUpdatesActionParity` CTest target, including keyboard resources/translations and the isolated temp environment. The actual QAction opened the manual update dialog; the test observed one configuration failure, verified the failure UI and retry action, and confirmed MainWindow remained visible. Because the service configuration has no API URL, the action makes no network request; startup cleanup resolves only inside the test-specific temp root. No production changes.

Verification: the Ninja target build passed under Visual Studio 18 with Qt 6.12; focused CTest `ClassMngrMainWindowCheckForUpdatesActionParityTests` passed 1/1; direct full-target and selected-slot QtTest runs each passed 3/0; an independent tester reran CTest and confirmed temp isolation and action-path assertions. Qt emitted the existing missing-font-directory and offscreen size/raise warnings while bundled fonts loaded. No full project suite. F456 is accepted and ready to commit; Batch 32 remains active until commit.

## 2026-10-09 - F456 committed; F457 discovery started

F456 committed as `d90def93f7947d0f031dc6f37a8a4a491f4d3718` (`Phase2 - Cover Check for Updates QAction manual handoff (F456)`); the branch was ahead by 41. Exactly the seven approved paths were committed and the cached diff check was clean. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain outside. Batch 32 is complete. Batch 33 is active with F457 selected for implementation.

## 2026-10-09 - F457 Font Size QAction accepted

Added `mainwindow_font_size_action_parity_tests.cpp` and the dedicated offscreen `MainWindowFontSizeActionParity` target. It isolates settings in QTemporaryDir, establishes an English/Normal baseline, disables recent-database loading, triggers the actual Large QAction, and verifies option/check state, persisted Large preference, FontManager offset, and QApplication point size. RAII restores the original process font and offset after MainWindow destruction, including assertion-failure exits. No production behavior changes.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; direct QtTest and independent direct rerun each passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1; `git diff --check` is clean. CMake reported pre-existing long object-path and missing optional Vulkan-header warnings; Qt reported a missing bundled `lib/fonts` directory while Inter and Pretendard loaded. No full project suite. F457 is accepted; commit transition is recorded below.

## 2026-10-09 - F457 committed; F458 discovery started

F457, `Phase2 - Cover Font Size QAction application handoff (F457)`, committed as `719efeacb6e5a8533f2dd45f6fb0fdfe152c0714` on `Qt-Rewrite` (42 commits ahead of origin). The commit contains exactly seven scoped paths and the cached diff check was clean. Batch 33 is complete; Batch 34 is active with F458 Theme QAction coverage selected for implementation. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain excluded user changes.

## 2026-10-09 - F458 Theme QAction accepted

Added `mainwindow_theme_action_parity_tests.cpp` and the dedicated offscreen `MainWindowThemeActionParity` target. It isolates settings, starts with a Light preference and injected ThemeService, disables recent-database loading, and triggers the actual Dark QAction. Assertions cover enabled/checked state, persisted preference, service theme, application Window color, and MainWindow `theme` property; the test then triggers Light and verifies restoration. The RAII guard captures the original palette/stylesheet before applying Light and restores them while MainWindow and its service remain alive, including assertion-return exits. No production changes.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1; `git diff --check` and untracked-source whitespace checks are clean. CMake reported unrelated long object-path warnings during reconfigure; Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full project suite. F458 is accepted; commit transition follows.

## 2026-10-09 - F458 committed; F459 discovery started

F458, `Phase2 - Cover Theme QAction application handoff (F458)`, committed as `2ec351a805af2064a865c34b76b46f60df80acdc` on `Qt-Rewrite` (43 commits ahead of origin). The commit contains exactly seven scoped paths and its cached diff check was clean. Batch 34 is complete; Batch 35 is active with F459 Document Viewer Background QAction coverage selected for implementation. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain excluded user changes.

## 2026-10-09 - F459 Document Viewer Background QAction accepted

Added `mainwindow_document_viewer_background_action_parity_tests.cpp` and the dedicated offscreen `MainWindowDocumentViewerBackgroundActionParity` target. It isolates settings, disables recent-database loading, and creates a blank PDF viewer through `pageManager()->ensurePdfViewerPage()` without loading a document. It triggers the actual Black and Default actions and checks enabled/check state, persisted values, viewer and viewport background properties, and palette. A scoped guard triggers Default while MainWindow and the viewer remain alive, including QtTest early-return paths. No production changes, PDF load, database, network, or autosave timer.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1; `git diff --check` and untracked-source whitespace/conflict checks are clean. CMake reported unrelated long object paths during reconfigure; Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full project suite. F459 is accepted and ready to commit; Batch 35 remains active until commit.

## 2026-10-09 - F459 committed; F460 selected

F459, `Phase2 - Cover Document Viewer Background QAction parity (F459)`, committed as `cba31503f0bb5accf68de2032131db984403deff` on `Qt-Rewrite` (44 commits ahead of origin). The commit contains exactly seven scoped paths and its cached diff check was clean. Batch 35 is complete; Batch 36 is active with F460 PDF Document Viewer Page Spacing QAction coverage selected. `agent_docs/latest_session_work.md` and `%SystemDrive%/` remain excluded user changes.

## 2026-10-09 - F460 Document Viewer Page Spacing QAction acceptance

Cover the actual MainWindow Page Spacing QAction handoff on a blank PDF viewer. In a dedicated offscreen target, isolate settings in a temporary directory, disable recent-database loading, and create the viewer with `pageManager()->ensurePdfViewerPage()` without loading a PDF. Trigger the real Large and Small actions and assert enabled/check state, persisted preference, and `QPdfView::pageSpacing()` values of 32 and 8. A scoped cleanup guard returns to Small while MainWindow and the viewer remain alive, including assertion-return paths. No PDF, database, autosave timer, network, production, or user-file behavior is in scope. F460 is selected; implementation and verification are pending.

## 2026-10-09 - F460 Document Viewer Page Spacing QAction accepted

Added `mainwindow_document_viewer_page_spacing_action_parity_tests.cpp` and the dedicated `MainWindowDocumentViewerPageSpacingActionParity` CTest target. The offscreen test isolates settings, disables recent-database loading, and creates a blank viewer without loading a document. It triggers the actual Large then Small actions and checks selected action/state, persisted preferences, and `QPdfView::pageSpacing()` values of 32 and 8. `DocumentPageSpacingRestorer` triggers Small while MainWindow remains alive, including assertion-return paths. No production changes or PDF/database/network/autosave activity.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1. Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full suite. F460 is accepted and ready to commit; Batch 36 remains active until commit.

## 2026-10-09 - F460 committed; F461 discovery started

F460, `Phase2 - Cover Document Viewer Page Spacing QAction parity (F460)`, committed as `fba46915d76b05aab53de85760a3f857dc4ed2a2` on `Qt-Rewrite` (45 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 36 is complete. Batch 37 is active; two independent read-only reviews are starting bounded QAction coverage discovery for F461. No candidate is selected yet. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

## 2026-10-09 - F461 Sidebar Overflow Tooltips QAction acceptance

Two independent reviews found the Show Sidebar Tooltips option has preference persistence coverage but no direct MainWindow QAction test. MainWindow connects `showSidebarTooltips` to `Sidebar::setOverflowTooltipsEnabled`, which applies the full text to overflowing tree items. Select this as a bounded visible handoff; Save Mode remains a wider autosave/timer path.

Acceptance: Add a dedicated offscreen MainWindow test with temporary settings and recent-database loading disabled. Insert a synthetic teacher with a very long name via `Sidebar::addTeacherNode`, constrain the Sidebar/tree width, show the window, and process events so overflow is established. Trigger the real enabled `showSidebarTooltips` QAction through false, true, then false. Assert check/option/persisted state and that the item tooltip becomes the full name when enabled and clears when disabled. Use an RAII guard to finish false while MainWindow is alive, including assertion-return paths. No database, network, autosave timer, or production change. F461 is selected for implementation; Batch 37 is active.

## 2026-10-09 - F461 Sidebar Overflow Tooltips QAction accepted

Added `mainwindow_sidebar_overflow_tooltips_action_parity_tests.cpp` and the dedicated `MainWindowSidebarOverflowTooltipsActionParity` target/`ClassMngrMainWindowSidebarOverflowTooltipsActionParityTests` registration. The offscreen test isolates settings, disables recent-database loading, adds a long synthetic teacher, constrains the Sidebar/tree, and verifies overflow by comparing measured label width with available viewport width after showing the window. It triggers the actual enabled/checkable `showSidebarTooltips` QAction through false→true→false and checks persisted state plus the item tooltip (empty/full name/empty). The RAII guard restores false while MainWindow is alive, including assertion-return paths. No production changes or DB/network/autosave activity.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1. Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full suite. F461 is accepted and ready to commit; Batch 37 remains active until commit.

## 2026-10-09 - F461 committed; F462 selected

F461, `Phase2 - Cover Sidebar Overflow Tooltips QAction parity (F461)`, committed as `e590ea773d4b6b1d415d248c9c3f5068094d5d4c` on `Qt-Rewrite` (46 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 37 is complete; Batch 38 is active with F462 Save Mode QAction parity selected. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

## 2026-10-09 - F462 Save Mode QAction acceptance

Two independent reviews found MainWindow Save Mode QAction coverage missing. Select it over Sidebar Marquee because a clean `TestingClassesPage` can be ensured without activating the page or loading database data, and its public `AutosaveCoordinator::saveMode()` gives a stable effect to assert; Marquee depends on hover and a timer.

Acceptance: Add a dedicated offscreen MainWindow test with temporary settings and recent-database loading disabled. Call `pageManager()->ensureTestingClassesPage()` without activating it, find the page's direct-child `AutosaveCoordinator`, and verify the page is clean. Trigger the actual Manual then Automatic Save Mode actions. Assert check/option/persisted preference state, coordinator mode, and that a `saveRequested` spy remains at zero while the page is clean. Use RAII to restore Automatic while MainWindow remains alive, including assertion-return paths. No database read, network, user-file access, dirty page, or autosave activity. F462 is selected for implementation; Batch 38 is active.

## 2026-10-09 - F462 Save Mode QAction accepted

Added `mainwindow_save_mode_action_parity_tests.cpp` and the dedicated `MainWindowSaveModeActionParity` target/`ClassMngrMainWindowSaveModeActionParityTests` registration. The offscreen test isolates settings, disables recent-database loading, and creates a hidden `TestingClassesPage` without activating or refreshing it. It finds the direct-child `AutosaveCoordinator`, verifies clean state, triggers the actual Manual then Automatic actions, and checks action/option/persisted values, coordinator modes, page/coordinator cleanliness, and zero `saveRequested` signals after both transitions. An RAII guard restores Automatic while MainWindow remains alive, including assertion-return paths. No production changes or database/network/user-file activity.

Verification: the VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1. Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full suite. F462 is accepted and ready to commit; Batch 38 remains active until commit.

## 2026-10-09 - F462 committed; F463 selected

F462, `Phase2 - Cover Save Mode QAction application handoff (F462)`, committed as `ad0d4de70aa5d984dd50eb0da13c1e3506e2be70` on `Qt-Rewrite` (47 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 38 is complete; Batch 39 is active with F463 Automatic Update Preference QAction coverage selected. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

## 2026-10-09 - F463 Automatic Update Preference QAction acceptance

Two independent reviews found no MainWindow QAction parity test for Automatically Check for Updates. Select this preference action over Sidebar Marquee because the SettingsManager preference port is a stable observable; Marquee has no public enabled-state getter and depends on hover and timer-driven rendering.

Acceptance: Add a dedicated offscreen MainWindow target with temporary settings, English language, recent-database loading disabled, and no UpdateController. Trigger the actual `automaticallyCheckForUpdates` QAction off then on and assert its checked state plus `SettingsManagerAutomaticUpdatePreferencesPort::read()` after each transition. Use RAII to restore the original preference while MainWindow remains alive, including assertion-return paths. Do not construct an UpdateController or trigger startup cleanup/network work; no database or user-file access. F463 is selected for implementation; Batch 39 is active.

## 2026-10-09 - F463 Automatic Update Preference QAction accepted

Added `mainwindow_automatic_update_preference_action_parity_tests.cpp` and the dedicated `MainWindowAutomaticUpdatePreferenceActionParity` target/`ClassMngrMainWindowAutomaticUpdatePreferenceActionParityTests` registration. The offscreen test uses English, temporary settings, recent-database loading disabled, and a target-local temporary environment; MainWindow receives no UpdateController. It triggers the actual enabled, checkable `automaticallyCheckForUpdates` QAction off then on and checks action state plus `SettingsManagerAutomaticUpdatePreferencesPort::read()` after each transition. An RAII guard restores the original preference while MainWindow remains alive. No update controller, startup cleanup, network, database, or user-file activity.

Verification: VS18/Qt6.12/Ninja focused build passed; CMake reconfiguration took about 244 seconds, exceeding the first 180-second wrapper timeout, and a sequential retry succeeded. Focused CTest passed 1/1; direct and independent QtTest runs each passed setup/test/cleanup (3 passed, 0 failed); independent CTest passed 1/1; `git diff --check` is clean. Qt reported its missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full suite. F463 is accepted and ready to commit; Batch 39 remains active until commit.

## 2026-10-09 - F463 committed; F464 selected

F463, `Phase2 - Cover Automatic Update Preference QAction parity (F463)`, committed as `e0771d5a3b87f75f6385bff23dd869e24e237242` on `Qt-Rewrite` (48 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 39 is complete; Batch 40 is active with F464 AI Comment Voice QAction coverage selected. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

## 2026-10-09 - F464 AI Comment Voice QAction acceptance

Two independent reviews compared Sidebar Marquee with AI Comment Voice. Select AI Comment Voice because its downstream effect can be observed in generated prompt text without hover or timer-driven rendering. Existing tests cover option persistence and prompt generation separately but do not trigger the MainWindow preference QAction.

Acceptance: Add a dedicated offscreen MainWindow test with temporary settings and recent-database loading disabled. Trigger the actual Third Person action from `aiCommentVoiceState`; assert action state and `SettingsManagerAiCommentVoicePreferencesPort::read()`. Then create a `SpeakingEvalAiBatchDialog` with one synthetic eligible grade-4 report and no existing comment, click `speakingEvalAiBatchCreatePrompt`, and assert the generated `speakingEvalAiBatchPrompt` includes parent/guardian and they/their instructions. Do not copy or open the prompt. Use RAII to restore the original voice while MainWindow remains alive, including assertion-return paths. No database, network, timer, or user-file activity. F464 is selected for implementation; Batch 40 is active.

## 2026-10-09 - F464 AI Comment Voice QAction accepted

Added `mainwindow_ai_comment_voice_action_parity_tests.cpp` and the dedicated `MainWindowAiCommentVoiceActionParity` target/`ClassMngrMainWindowAiCommentVoiceActionParityTests` registration. With temporary settings and recent-database loading disabled, the offscreen MainWindow test triggers the actual Third Person voice action and verifies action/OptionState selection plus `SettingsManagerAiCommentVoicePreferencesPort::read()`. It creates a synthetic eligible grade-4 report with no existing comment, clicks only the batch dialog's Create Prompt button, and checks that the generated prompt contains parent/guardian and they/their instructions. It does not copy or open the prompt. An RAII guard restores and syncs the original voice while MainWindow is alive, including assertion-return paths. No production changes, database, network, timer, or user-file activity.

Verification: VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1; whitespace checks are clean. CMake reported unavailable optional Vulkan headers; Qt reported the missing bundled `lib/fonts` path while packaged Inter and Pretendard loaded. No full suite. F464 is accepted and ready to commit; Batch 40 remains active until commit.

## 2026-10-09 - F464 committed; F465 selected

F464, `Phase2 - Cover AI Comment Voice QAction prompt handoff (F464)`, committed as `27d30bbd8452f0d350e968ef462af83ae340cdd3` on `Qt-Rewrite` (49 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 40 is complete; Batch 41 is active with F465 AI Comment Provider QAction coverage selected. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

## 2026-10-09 - F465 AI Comment Provider QAction acceptance

Two independent reviews agreed that a built-in provider preference offers a stable dialog-label effect. Select it over Sidebar Marquee, whose visible behavior depends on hover and timer-driven rendering, and the macOS-only PowerPoint notice.

Acceptance: Add a dedicated offscreen MainWindow test with temporary settings, English language, and recent-database loading disabled. Trigger the actual Gemini provider action through `aiCommentProviderState`; assert action selection and `SettingsManagerAiCommentProviderPreferencesPort::read()`. Construct `SpeakingEvalAiBatchDialog` and verify its `speakingEvalAiBatchCopyOpen` button label includes “Gemini.” Do not click the button or invoke clipboard/browser behavior; avoid Custom Website. Use RAII to restore and sync the original provider while MainWindow remains alive, including assertion-return paths. No database, network, timer, or user-file activity. F465 is selected for implementation; Batch 41 is active.

## 2026-10-09 - F465 AI Comment Provider QAction accepted

Added `mainwindow_ai_comment_provider_action_parity_tests.cpp` and the dedicated `MainWindowAiCommentProviderActionParity` target/`ClassMngrMainWindowAiCommentProviderActionParityTests` registration. The offscreen test clears isolated temporary settings, selects ChatGPT as the starting provider, triggers the actual Gemini QAction, and verifies selection plus the synced `SettingsManagerAiCommentProviderPreferencesPort::read()`. It constructs the AI batch dialog and checks the Copy/Open button label includes Gemini without clicking the button. An RAII restorer triggers and syncs the original provider while MainWindow is alive, including assertion-return paths. No production changes or external behavior.

Verification: VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1; `git diff --check` and source whitespace checks are clean. CMake emitted nonfatal optional Vulkan-header and long object-path warnings; Qt reported the missing bundled `lib/fonts` path while packaged Inter/Pretendard loaded. No full suite. F465 is accepted and ready to commit; Batch 41 remains active until commit.

## 2026-10-09 - F465 committed; F466 selected

F465, `Phase2 - Cover AI Comment Provider QAction dialog handoff (F465)`, committed as `599916bada7a2a59ae041dc80d59cba2187cf3be` on `Qt-Rewrite` (50 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 41 is complete; Batch 42 is active with F466 Custom Website provider QAction coverage selected. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

## 2026-10-09 - F466 Custom Website provider QAction acceptance

Independent reviews identified remaining Sidebar Marquee and macOS-only PowerPoint notice gaps; the source scan also found an uncovered Custom Website provider action with a clear modal and persisted URL result. Select Custom Website over timer-driven Marquee.

Acceptance: Add a dedicated offscreen MainWindow test with temporary settings, English language, and recent-database loading disabled. Trigger the actual Custom Website provider QAction. Queue a zero-delay timer to observe the active `QInputDialog`, enter a valid test URL, and accept it; close any unexpected active modal so an assertion failure cannot hang. Assert the modal handoff, provider becomes CustomWebsite, and `SettingsManagerAiCommentCustomWebsitePort::read()` returns the test URL. Restore the original URL/provider without opening another dialog while MainWindow remains alive, including assertion-return paths. Do not click Copy/Open or make a network request. Avoid invalid URL and Cancel paths. No database, production, or user-file behavior. F466 is selected for implementation; Batch 42 is active.

## 2026-10-09 - F466 Custom Website provider QAction accepted

Added `mainwindow_custom_website_ai_comment_provider_action_parity_tests.cpp` and the dedicated `MainWindowCustomWebsiteAiCommentProviderActionParity` target/`ClassMngrMainWindowCustomWebsiteAiCommentProviderActionParityTests` registration. With temporary settings, English, and recent-database loading disabled, the offscreen test triggers the actual Custom Website action. A zero-delay timer captures the active QInputDialog, enters `https://example.test/`, and accepts it. Assertions verify the modal was observed and closed, the provider becomes CustomWebsite, and the custom-URL port stores the entered URL. Unexpected modals are rejected or closed to avoid hangs. RAII restores the original provider and URL while MainWindow is alive. No Copy/Open, browser, network, database, or user-file activity.

Verification: VS18/Qt6.12/Ninja target build passed; focused CTest passed 1/1; independent direct QtTest passed setup/test/cleanup (3 passed, 0 failed); independent CTest rerun passed 1/1; `git diff --check` is clean. CMake reported optional Vulkan headers unavailable and used bundled zlib fallback; Qt reported missing `lib/fonts` and offscreen `propagateSizeHints` warnings while packaged Inter/Pretendard loaded. No full suite. F466 is accepted and ready to commit; Batch 42 remains active until commit.

## 2026-10-09 - F466 committed; F467 selected

F466, `Phase2 - Cover Custom Website provider QAction modal handoff (F466)`, committed as `0abbaf9238a25407f3c8a6c88a07adb4e93aa238` on `Qt-Rewrite` (51 commits ahead of origin). Exactly seven scoped paths were committed; Batch 42 is complete. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

F467 covers the remaining portable MainWindow Sidebar Marquee QAction handoff. The existing Sidebar Overflow Tooltips action parity target can be extended without CMake changes. Acceptance: expose a const read-only query for `SidebarMarqueeDelegate` enabled state, then trigger the actual `animateSidebarText` QAction and verify both transitions update delegate state and the persisted sidebar-marquee preference. Use temporary settings for a deterministic start and an RAII restorer that restores the original QAction setting while MainWindow is alive, including assertion exits. The scope is configuration handoff only; no hover, timer, rendered pixels, or animation behavior assertion. F467 is selected for implementation; Batch 43 is active.

## 2026-10-09 - F467 Sidebar Marquee QAction parity accepted

Added a const `[[nodiscard]] marqueeEnabled()` query to `SidebarMarqueeDelegate` and extended the registered Sidebar Overflow action parity test. With temporary settings and recent-database loading disabled, the test seeds the preference off, triggers the real `animateSidebarText` QAction through off→on→off, and verifies QAction state, delegate state, and persisted preference at each transition. An RAII restorer returns the original action state and syncs while MainWindow remains alive. This verifies configured handoff only; it does not exercise hover, timer progression, or rendered scrolling. No CMake changes.

Verification: focused target build passed; focused CTest passed 1/1, including independent rerun. Direct QtTest from the build tree with the offscreen environment exited 0 but emitted no summary, so no test count is claimed from that run. `git diff --check` passed; independent source review accepted the test/restorer lifecycle. No full suite. F467 is independently verified, accepted, and ready to commit; Batch 43 remains active until commit.

## 2026-10-09 - F467 committed; F468 selected

F467, `Phase2 - Cover Sidebar Marquee QAction parity (F467)`, committed as `0384c3768c18009689f918f456ae932c0c1d5a89` on `Qt-Rewrite` (52 commits ahead of origin). Exactly seven scoped paths were committed and the cached diff check was clean. Batch 43 is complete. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent discovery passes selected F468: extend the existing MainWindow document-catalog language retranslation parity test to verify language QAction preference persistence. After its real Korean and English action triggers, assert `languageState->current()`, the typed `SettingsManagerLanguagePreferencesPort::read()`, and selected QAction state alongside existing locale/retranslation assertions. Keep temporary settings and recent-database loading disabled. Add an RAII restorer that returns the English baseline through the QAction and syncs while MainWindow remains alive, including assertion-return paths. This is test-only and uses the existing registered target; no production or CMake change. F468 is selected for implementation; Batch 44 is active.

## 2026-10-09 - F468 Document Catalog Language QAction parity accepted

Extended `mainwindow_document_catalog_retranslation_parity_tests.cpp` at its existing Korean and English language QAction transitions. After each real trigger, the test checks `languageState->current()`, both actions’ checked state, and the typed `SettingsManagerLanguagePreferencesPort::read()` after syncing settings. Temporary settings start at English; recent-database loading is disabled. A scoped English restorer, declared after MainWindow, triggers English only if needed and syncs on early assertion returns. No production or CMake change.

Verification: focused target build passed; focused CTest passed 1/1; direct offscreen QtTest passed 4/0/0 skipped; `git diff --check` passed. Independent review confirmed fixture and restorer ordering and independently passed CTest 1/1 and direct QtTest 4/0/0. Qt warned that its configured fonts directory is missing; packaged Inter/Pretendard fonts loaded. No full suite. F468 is independently verified, accepted, and ready to commit; Batch 44 remains active until commit.

## 2026-10-09 - F468 committed; F469 selected

F468, `Phase2 - Cover Document Catalog Language Preference QAction parity (F468)`, committed as `219ca8fb64b50247c24a722f7a091d75a2a6d86e` on `Qt-Rewrite` (53 commits ahead of origin). Exactly six scoped paths were committed; the cached diff check was clean. Batch 44 is complete. The only post-commit worktree changes are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F469 scans identified deterministic gaps. Select the missing Document Page Spacing variants: the existing registered viewer parity test covers only Small/Large; the enum also defines None (0 px) and Medium (16 px). Acceptance: extend the existing test to trigger the actual None and Medium QActions, assert selected state/exclusive checks, persisted spacing preference, and `QPdfView::pageSpacing()` of 0 and 16 while retaining existing Small/Large checks. Keep temporary settings and recent-database loading disabled. The existing Small restorer must sync its reset on early assertion exits. The blank viewer requires no PDF load; no production or CMake change. This was selected over the locale-dependent System Default language action because viewer effects are concrete and host-independent. Implementation now triggers None and Medium in the existing test, checking OptionState, exclusive actions, persisted values 0/2, and viewer spacing 0/16 px while retaining Small/Large coverage. The restorer syncs after conditionally restoring Small. Focused target build passed; focused CTest passed 1/1; direct offscreen QtTest passed 3/0/0; `git diff --check` passed. The only runtime warning was the missing optional configured Qt fonts directory; project Inter/Pretendard fonts loaded. Independent Tester confirmed the fixture, restorer, and action coverage, and independently passed focused CTest 1/1 and `git diff --check`. Its direct offscreen invocation exited 0 but emitted no summary, so no independent direct test count is claimed. Executor direct QtTest reported 3 passed/0 failed/0 skipped. F469 is independently verified and accepted, ready to commit; Batch 45 remains active until commit.


## 2026-10-09 - F469 committed; F470 Background White selected

F469, `Phase2 - Cover Document Viewer Page Spacing None and Medium QAction parity (F469)`, committed as `4044c80a` on `Qt-Rewrite`. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 45 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F470 scans selected the omitted White action in the registered Document Viewer Background parity test. Extend the blank-viewer test to trigger White and assert `OptionState`, exclusive Default/White/Black QAction checks, persisted value `1`, viewer/viewport properties `white`, and the viewer `QPalette::Dark` color. Retain Black and Default behavior and cleanup. Use temporary settings, disable recent-database loading, and sync the Default restorer on assertion exits; no PDF load, production change, or CMake change is needed. F470 is selected for implementation; Batch 46 is active.

F470 implementation extends the registered background parity test with the White action before Black, asserting OptionState, exclusive Default/White/Black action checks, persisted value 1, viewer and viewport properties, and a white `QPalette::Dark`; existing Black/Default checks remain. The Default restorer conditionally restores and always syncs on assertion exits. VS18 focused build passed; focused CTest passed 1/1; direct offscreen QtTest passed 3/0/0; `git diff --check` passed. Qt reported a missing optional fonts directory while packaged Inter/Pretendard fonts loaded. Independent Tester confirmed the test scope and restorer, and independently passed focused CTest 1/1 and `git diff HEAD --check`. Its direct offscreen invocation exited 0 without a summary, so no direct case count is claimed from that run; the executor invocation reported 3/0/0. No full suite. F470 is independently verified and accepted, ready to commit; Batch 46 remains active until commit.


## 2026-10-09 - F470 committed; F471 Font Size matrix selected

F470, `Phase2 - Cover Document Viewer Background White QAction parity (F470)`, committed as `738f40f1` on `Qt-Rewrite`. The commit contains the six scoped test and Phase 2 tracking paths. Batch 46 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F471 scans selected the missing Font Size actions in the existing registered parity test. It currently starts at Normal and exercises Large only. Extend it to trigger Small, retain Large, trigger Extra Large, and return through the Normal action. At each transition assert `OptionState`, exclusive action checks, the typed persisted preference after sync, `FontManager::sizeOffset()`, and runtime-relative application point size (`FontManager::getPlatformFontSize() + offset`). Keep English and temporary settings, disable recent-database loading, and restore the original application font/offset plus the Normal preference on assertion exits. Use dynamic base-size assertions, not hard-coded visual sizes. No production or CMake change. F471 is selected; Batch 47 is active.

F471 implementation extends the existing Font Size parity test to trigger Small, Large, Extra Large, and Normal actions. It asserts `OptionState`, exclusive checks, typed persisted preferences after sync, offsets -2/+2/+4/0, and runtime-relative application point size after each transition. The guard restores the original application font/offset and writes/syncs Normal on assertion exits. The focused target built under VS18; CTest passed 1/1; direct offscreen QtTest exited 0 without a summary; `git diff --check` passed. Independent Tester confirmed all four transitions, typed preference checks, runtime-relative font outcomes, and restorer cleanup; independently passed focused CTest 1/1 and `git diff --check`. Its direct offscreen invocation exited 0 without output, so no direct count is claimed. No full suite. F471 is independently verified and accepted, ready to commit; Batch 47 remains active until commit.


## 2026-10-09 - F471 committed; F472 System Default Theme selected

F471, `Phase2 - Cover Small and Extra Large Font Size QAction parity (F471)`, committed as `0d972b53` on `Qt-Rewrite`. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 47 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F472 scans selected the omitted System Default Theme action in the existing MainWindow Theme parity test. Extend its Light → Dark → System Default → Light flow to assert the three exclusive actions, `OptionState`, typed persisted preference after sync, and the effective resolved theme from `ThemeService::currentTheme()` plus the window theme property. Compute the expected Light/Dark result from `QApplication::styleHints()->colorScheme()` (Unknown resolves Light); do not assume a host scheme. Restore Light while MainWindow remains alive and sync settings, retaining the existing palette/stylesheet cleanup. Keep temporary settings, English, and recent-database loading disabled; no production or CMake change. F472 is selected; Batch 48 is active.

F472 implementation extends the existing theme parity test with the System Default action in Light → Dark → System Default → Light order. It checks exclusive action group/check state, OptionState, typed preferences after sync, and the resolved theme in ThemeService, application palette, and the window property. Expected Light/Dark is derived from Qt colorScheme; Unknown maps to Light. The restorer syncs Light while the window is alive and retains palette/stylesheet cleanup. VS2026 x64 focused build passed; CTest passed 1/1; direct offscreen QtTest passed 3/0/0; `git diff --check` passed. Independent Tester confirmed the fixture, action group, preferences, dynamic resolved theme, and cleanup, and independently passed focused CTest 1/1, direct offscreen QtTest 3/0/0, and `git diff --check`. Qt warned about a missing configured fonts directory while packaged fonts loaded. No full suite. F472 is independently verified and accepted, ready to commit; Batch 48 remains active until commit.


## 2026-10-09 - F472 committed; F473 Claude Provider selected

F472, `Phase2 - Cover System Default Theme QAction parity (F472)`, committed as `88fd2088` on `Qt-Rewrite`. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 48 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F473 scans nominated System Default Language and Claude AI Provider. Select Claude in the existing MainWindow provider parity test: it already triggers Gemini and checks the local batch-dialog provider label, while Claude’s QAction, saved preference value, and consumer label have stable local behavior. System Default Language remains locale-dependent and was set aside during F469. Extend the test to trigger the actual Claude action after Gemini, assert provider state and exclusive ChatGPT/Gemini/Claude checks, typed persisted preference after sync, and a newly constructed batch dialog label identifying Claude. Retain Gemini checks. Keep temporary settings, English, and recent-database loading disabled; do not click the open button or access a browser/network. Reuse the existing restorer to return to ChatGPT and sync. No production or CMake change. F473 is selected; Batch 49 is active.

F473 implementation extends the existing provider parity test with a Claude QAction transition after Gemini. It checks ChatGPT/Gemini/Claude action state, typed persisted preference after sync, and the existing consumer dialog label for each provider. The restorer returns to ChatGPT and syncs on assertion exits. VS18 x64 focused build passed; CTest passed 1/1; direct offscreen QtTest exited 0 without a summary; `git diff --check` passed. No browser/network action was clicked; no production or CMake change. Independent Tester confirmed the fixture, action state/exclusivity, typed persistence, dialog labels, and ChatGPT restorer; independently passed focused CTest 1/1 and `git diff --check`. Its direct offscreen invocation exited 0 without output, so no direct count is claimed. No full suite or network behavior. F473 is independently verified and accepted, ready to commit; Batch 49 remains active until commit.


## 2026-10-09 - F473 committed; F474 Microsoft Copilot selected

F473, `Phase2 - Cover Claude AI Comment Provider QAction parity (F473)`, committed as `438f541a` on `Qt-Rewrite`. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 49 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F474 scans found provider action gaps: the code scan nominated Microsoft Copilot; the plan scan noted ChatGPT’s consumer label/transition is not asserted. Select Copilot because the existing provider test already verifies ChatGPT baseline/restoration and Gemini/Claude action handoffs, while Copilot has no actual selected-action or consumer-label coverage. Extend `mainwindow_ai_comment_provider_action_parity_tests.cpp` to trigger Copilot after Claude, assert state and exclusivity across ChatGPT/Gemini/Claude/Copilot/CustomWebsite, typed persisted MicrosoftCopilot preference after sync, and a new batch dialog label containing “Microsoft Copilot.” Retain Gemini and Claude checks. Use temporary settings, English, and disabled recent-database loading; do not click the open button or access browser/network. Reuse the ChatGPT restorer and sync. No production or CMake change. F474 is selected; Batch 50 is active.

F474 implementation extends the provider parity test with Microsoft Copilot after Gemini and Claude. It checks exclusive action state across ChatGPT/Gemini/Claude/MicrosoftCopilot/CustomWebsite, typed persisted Copilot preference after sync, and a new batch dialog label containing “Microsoft Copilot.” Existing Gemini/Claude labels remain. The ChatGPT restorer syncs on assertion exits; the open button is never clicked. VS18 focused build passed; CTest passed 1/1; direct offscreen QtTest exited 0 without output; `git diff --check` passed. No production or CMake change. Independent Tester confirmed all five provider states, typed persistence, Gemini/Claude/Copilot dialog labels, and the ChatGPT restorer; independently passed focused CTest 1/1 and `git diff --check`. Its direct offscreen invocation exited 0 without output, so no direct count is claimed. No full suite or network behavior. F474 is independently verified and accepted, ready to commit; Batch 50 remains active until commit.


## 2026-10-09 - F474 committed; F475 Custom Website label selected

F474, `Phase2 - Cover Microsoft Copilot AI Comment Provider QAction parity (F474)`, committed as `f102e7ad` on `Qt-Rewrite`. The commit contains the six reviewed test and Phase 2 tracking paths. Batch 50 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Two independent F475 scans found missing batch-dialog label coverage: one nominated ChatGPT at the existing default baseline, and the other nominated Custom Website after its modal action flow. Select Custom Website because the existing isolated test verifies the unique URL-entry action, saved provider, and URL; asserting its local consumer label closes the end-to-end handoff without host or network dependence. Extend `mainwindow_custom_website_ai_comment_provider_action_parity_tests.cpp` to construct a batch dialog with synthetic report data after the action saves the custom URL, then assert `speakingEvalAiBatchCopyOpen` contains “Custom AI Website.” Retain provider/URL persistence assertions and the ChatGPT/URL restorer. Do not click the button or access a browser/network. Use temporary settings, English, and disabled recent-database loading; no production or CMake change. F475 is selected; Batch 51 is active.

F475 implementation extends the Custom Website provider action test with a synthetic batch report and asserts the dialog’s `speakingEvalAiBatchCopyOpen` label contains “Custom AI Website” after the provider and URL are saved. Existing action, typed preference, URL, and restorer checks remain. The button is not clicked. VS18 focused build passed; CTest passed 1/1; direct offscreen QtTest exited 0 without a summary; `git diff --check` passed. Git emitted a line-ending normalization notice. No production or CMake change. Independent Tester confirmed modal URL entry, provider/URL persistence, Custom Website consumer label, fixture, and restorer; independently passed focused CTest 1/1 and `git diff --check`. Its direct offscreen invocation exited 0 without a summary, so no count is claimed; the executable was newer than source. No full suite or network behavior. F475 is independently verified and accepted, ready to commit; Batch 51 remains active until commit.

F475, `Phase2 - Cover Custom Website batch dialog label (F475)`, committed as `643d9674` on `Qt-Rewrite` with exactly six scoped paths. Batch 51 is complete; the only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`.

Independent F476 scans identified a missing ChatGPT batch-dialog label assertion and an untested Direct to Student voice action-to-prompt handoff. Select the voice handoff because it extends a deterministic QAction → persisted preference → local prompt consumer path. Acceptance: extend the existing voice parity test to retain Third Person coverage, then trigger Direct to Student, assert OptionState and exclusive QAction state, sync and assert the typed preference, construct a fresh synthetic-report batch dialog, and click only its local Create Prompt button. Assert the Direct instruction appears and Third Person instructions are absent. Keep English, temporary settings, and recent-database loading disabled; restore the original voice and sync on assertion exits. No production/CMake changes, browser, or network. F476 is selected; Batch 52 is active.

F476 implementation retains the Third Person action-to-prompt assertions, then triggers DirectToStudent, checks exclusive action state and typed saved preference after sync, and generates a fresh local batch prompt from the synthetic report. The prompt must contain the Direct instruction and omit both Third Person instructions. The executor's focused VS18 target build passed, focused CTest passed 1/1, and `git diff --check` passed. No production/CMake changes or browser/network behavior. Independent review is pending; Batch 52 remains active.

Independent review accepted F476. The Tester confirmed the DirectToStudent action, exclusive selection, persisted typed preference, both prompt-wording assertions, and RAII restoration. Independent focused CTest passed 1/1; direct offscreen QtTest exited 0 without a summary; `git diff --check` passed. The direct executable was newer than source, so the executor's successful focused build is the build evidence. No full suite. F476 is accepted and ready to commit; Batch 52 remains active until commit.

F476, `Phase2 - Cover Direct to Student voice action-to-prompt handoff (F476)`, committed as `9e58daae` on `Qt-Rewrite` with exactly six scoped paths. Batch 52 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F477 discovery is starting.

Independent F477 plan and code scans agree on the deferred ChatGPT provider-to-batch-dialog label handoff. The registered provider parity test covers Gemini, Claude, and Microsoft Copilot labels but never selects ChatGPT after another provider and checks a fresh dialog. Acceptance: after existing Copilot assertions, trigger the actual ChatGPT QAction; assert current/exclusive action state, sync and assert the typed ChatGPT preference, then construct a fresh synthetic-report batch dialog and assert its `speakingEvalAiBatchCopyOpen` label identifies ChatGPT. Retain all existing checks. Keep temporary English settings and recent-database loading disabled; restore ChatGPT and sync. Do not click the dialog button or access browser/network; no production/CMake changes. F477 is selected; Batch 53 is active.

F477 implementation adds a ChatGPT transition after the retained Copilot coverage: it triggers the actual QAction, checks exclusive selected state, syncs and verifies the typed preference, then checks the fresh synthetic-report dialog's exact `Copy Prompt and Open ChatGPT` label. No production/CMake change. Executor VS18 focused target build passed, focused CTest passed 1/1, direct offscreen executable exited 0 without a summary, and `git diff --check` passed. Independent review is pending; Batch 53 remains active.

Independent review accepted F477. The Tester confirmed ChatGPT action exclusivity, typed preference after sync, exact local dialog label, fixture, and RAII restoration. Independent focused CTest passed 1/1; direct offscreen QtTest exited 0 with empty output; `git diff --check` passed. The focused object and executable timestamps are later than the source edit, and the executor's focused build passed. Git emitted the expected LF-to-CRLF working-copy notice. No full suite or browser/network behavior. F477 is accepted and ready to commit; Batch 53 remains active until commit.

F477, `Phase2 - Cover ChatGPT AI Comment Provider QAction dialog label parity (F477)`, committed as `9a4c9e85` on `Qt-Rewrite` with exactly six scoped paths. Batch 53 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F478 discovery is starting.

F478 decision: the plan scan found no currently actionable deferred item; F285 and F298 still depend on unresolved contracts. The code scan found that the single-student SpeakingEvalReportDialog main Copy Prompt and Open button reads the saved provider, while current test coverage checks only Gemini there; no test covers the other provider labels. Select this local deterministic consumer gap. Acceptance: extend the existing MainWindow provider parity test to check the single-student dialog label after ChatGPT, Gemini, Claude, and Microsoft Copilot actions, and extend the existing Custom Website action parity test to check its label after the real modal action. Use fresh synthetic reports and assert the speakingEvalCopyOpenAiPromptButton identifies the selected provider. Preserve current batch-dialog checks, temporary English settings, disabled recent-database loading, and restorers; do not click open/preview controls or access browser/network. No production/CMake changes. The preview popup's modal contract is deferred. F478 is selected; Batch 54 is active.

F478 implementation adds fresh single-student report dialog label checks after ChatGPT, Gemini, Claude, and Microsoft Copilot actions; the Custom Website test adds the same consumer check after its actual modal action. Existing batch-dialog assertions and restorers remain. Both focused targets built; focused CTest registrations passed 2/2; direct offscreen QtTest passed 3/0 for each executable; `git diff --check` passed. No production/CMake changes or browser/network behavior. Independent review is pending; Batch 54 remains active.

Independent review accepted F478. The Tester confirmed each label follows the actual provider action and synced typed preference; the Custom Website path also follows its real modal URL flow. Both focused CTests passed 2/2, direct offscreen QtTests passed 3/0 each, and independent `git diff --check` passed. Focused binaries postdate their source. Qt reported optional font-directory and offscreen size-hint warnings; Git reported LF-to-CRLF working-copy notices. No full suite, browser, or network behavior. F478 is accepted and ready to commit; Batch 54 remains active until commit.

F478, `Phase2 - Cover Single-Student Report Dialog Provider Label parity (F478)`, committed as `d5a0eef5` on `Qt-Rewrite` with exactly seven scoped paths. Batch 54 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F479 discovery is starting.

F479 decision: the plan scan found no ready candidate; F285 and F298 still depend on unresolved contracts. The code scan found an uncovered provider-label consumer in SpeakingEvalAiPromptPreviewDialog: its speakingEvalAiPromptPreviewCopyOpen button is separate from the main report button. The existing aiPromptPreviewCopiesAnAnonymousPrompt test already opens this modal from eligible synthetic data and closes it with a timer. Select a five-provider label matrix there. Acceptance: for ChatGPT, Gemini, Claude, Microsoft Copilot, and Custom AI Website, write and sync the typed provider preference, open the local preview modal, assert the named Copy Prompt and Open button contains the provider name, then close the modal without clicking Copy/Open. Restore the original provider with an RAII guard on assertion exits. Preserve existing prompt/anonymization/copy coverage and temporary settings. No production/CMake changes or browser/network behavior. F479 is selected; Batch 55 is active.

F479 implementation adds a five-provider preview-popup label test using the existing synthetic report and modal harness. It reads each typed preference after sync, inspects speakingEvalAiPromptPreviewCopyOpen, rejects the popup from a timer, and restores the original provider with RAII. The existing prompt/anonymization/copy test is unchanged. Focused target build passed; CTest passed 1/1; direct offscreen slot exited 0 without captured output; `git diff --check` passed. No production/CMake changes or browser/network behavior. Independent review is pending; Batch 55 remains active.

Independent review accepted F479. The Tester confirmed all five typed provider preferences are synced and reflected in the preview Copy/Open label, each popup closes without activating Copy/Open, and the RAII guard restores/syncs the original preference. Independent focused CTest passed 1/1; the direct offscreen slot passed 3/0/0 with its summary captured to a temporary report; `git diff --check` passed. The focused executable postdates the source. Qt reported optional font-directory and offscreen size-hint warnings. No full suite, browser, or network behavior. F479 is accepted and ready to commit; Batch 55 remains active until commit.

F479, `Phase2 - Cover AI Prompt Preview Provider Label parity (F479)`, committed as `803ff239` on `Qt-Rewrite` with exactly six scoped paths. Batch 55 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F480 discovery is starting.

F480 decision: the independent scans surfaced preview Copy/Open handoff and Custom Website cancellation rollback. Select cancellation rollback as a deterministic local preference-state slice: ActionRegistry's cancel branch restores the previous provider and returns before writing the URL, and the existing isolated Custom Website test already has a safe modal timer. Acceptance: in a new test slot, seed ChatGPT and a known stored URL in temporary English settings, disable recent-database loading, trigger the actual Custom Website QAction, have the timer replace the input then reject the modal, and assert it was observed/closed, ChatGPT remains current/checked, CustomWebsite is unchecked, typed provider preference after sync is ChatGPT, and stored URL is unchanged. Reuse the existing provider/URL RAII restorer for assertion exits. Do not test invalid URL, open a browser, or access network. No production/CMake changes. The preview Copy/Open handoff remains a separate candidate. F480 is selected; Batch 56 is active.

F480 implementation adds an isolated cancellation slot to the Custom Website parity test. Its timer changes the input text and rejects the modal; assertions confirm ChatGPT current/checked, CustomWebsite unchecked, typed preference remains ChatGPT after sync, and the original URL remains. Existing provider/URL RAII restorer handles assertion exits. Focused Debug build passed, focused CTest passed 1/1, direct offscreen cancellation slot exited 0, and `git diff --check` passed. No production/CMake changes or browser/network behavior. Independent review is pending; Batch 56 remains active.

Independent review accepted F480. The Tester confirmed the rejection path leaves ChatGPT selected/persisted and the custom URL unchanged, and that the provider/URL restorer runs on assertion exits. Focused Debug CTest passed 1/1; direct offscreen cancellation slot exited 0 without a summary; independent `git diff --check` passed. The executor's focused Debug build contains the new slot. Git emitted an LF-to-CRLF notice. No full suite, invalid URL, browser, or network behavior. F480 is accepted and ready to commit; Batch 56 remains active until commit.

F480, `Phase2 - Cover Custom Website chooser cancellation rollback (F480)`, committed as `869b7331` on `Qt-Rewrite` with exactly six scoped paths. Batch 56 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F481 discovery is starting.

F481 discovery selected the speaking-evaluation AI preview Copy/Open handoff. The plan scan identified it as explicitly uncovered after F479; the code scan confirmed QDesktopServices HTTPS URL capture is already used in the test suite, so this can be isolated without a browser or network. Acceptance: add one focused slot to speaking_eval_batch_report_service_tests using synthetic eligible report data and the temporary settings root already established by the fixture. Seed ChatGPT, open the preview, capture the displayed prompt, click the actual speakingEvalAiPromptPreviewCopyOpen button, then assert the preview remains open after the ActionRole handoff, explicitly reject it, and verify closure; the clipboard equals the displayed anonymous prompt (contains STD_NAME, excludes real names), and exactly one captured URL is https://chatgpt.com/. Use scoped URL-handler cleanup and the existing provider preference restorer. Keep F479 label coverage unchanged; no production/CMake/browser/network changes. Batch 57 is active; implementation pending.

F481 implementation adds one preview Copy/Open handoff slot with scoped provider and HTTPS handler cleanup. It verifies the preview remains open after the ActionRole click, closes it explicitly, and checks exact clipboard prompt, anonymization, and one captured ChatGPT URL. Executor Ninja build succeeded in the VS Developer environment; focused CTest passed 1/1; direct slot passed 3/0/0; diff check passed. Independent review accepted the corrected diff: CTest passed 1/1, the executable inventory contains the new slot, direct offscreen invocation exited 0 without output, and independent diff check passed with an LF-to-CRLF notice. The prior Copy-only test is unchanged. No full suite, browser/network, production, or CMake changes. F481 is accepted and ready to commit; Batch 57 remains active until commit.

F481, `Phase2 - Cover AI Prompt Preview Copy/Open handoff (F481)`, committed as `ae1da379` on `Qt-Rewrite` with exactly six scoped paths. Batch 57 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F482 discovery is starting.

F482 discovery selected Custom Website preview Copy/Open destination coverage after independent plan and code scans converged. preferredAiCommentProviderUrl reads the saved custom URL; aiCommentProviderUrl returns a valid HTTPS value, and the existing scoped HTTPS handler captures it. Acceptance: add one focused slot in speaking_eval_batch_report_service_tests.cpp using its synthetic eligible-report fixture and temporary settings root. Save and sync a deterministic Custom Website URL with path/query, select/sync CustomWebsite, and install RAII restorers for both previous provider and URL. Open preview, capture the displayed anonymous prompt, click the actual Copy/Open action, verify the preview stays open after the ActionRole handoff, clipboard equals the exact prompt (STD_NAME present; real names absent), and exactly one intercepted URL equals the saved QUrl; then explicitly reject and verify closure. Reuse the committed scoped HTTPS URL handler. No invalid-URL case, production/CMake changes, browser, or network. Batch 58 is active; implementation pending.

F482 implementation adds a Custom Website preview Copy/Open slot. It captures and restores previous provider and custom URL with scoped guards, seeds/syncs a deterministic HTTPS path/query URL, intercepts the handoff, and verifies exact anonymous prompt clipboard, one exact URL, ActionRole modal persistence, and explicit close. Focused Ninja build succeeded in VS Developer environment; CTest passed 1/1; direct slot passed 3/0/0; diff check passed. Qt font/offscreen warnings only. Independent verification is pending; Batch 58 remains active.

Independent review accepted F482. The Tester confirmed provider/URL restorers precede both mutations; the preview Copy/Open action captures exactly `https://custom.example.test/f482/preview?source=phase2`; the clipboard matches the displayed anonymous prompt; the ActionRole preview stays open until explicit rejection; and the timeout dismisses unexpected modals safely. Independent focused CTest passed 1/1; direct offscreen slot exited 0 without a summary; diff check passed. The Ninja executable contains the slot and is newer than the source/object. No full suite or browser/network launch. F482 is accepted and ready to commit; Batch 58 remains active until commit.

F482, `Phase2 - Cover Custom Website preview Copy/Open destination (F482)`, committed as `82733b05` on `Qt-Rewrite` with exactly six scoped paths. Batch 58 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F483 discovery is starting.

F483 discovery selected Gemini preview Copy/Open destination coverage after independent plan and code scans. F481/F482 cover ChatGPT and Custom Website; no Copy/Open destination tests exist for Gemini, Claude, or Copilot. The production Gemini URL is https://gemini.google.com/app. Acceptance: add one focused slot to speaking_eval_batch_report_service_tests.cpp using synthetic eligible report data and the temporary settings fixture. Capture and restore the prior provider, seed/sync Gemini, and reuse the committed scoped HTTPS handler. Open preview, capture its anonymous prompt, click actual Copy/Open, assert the ActionRole preview remains open, clipboard exactly matches the preview (STD_NAME present; real student names absent), and exactly one intercepted URL equals https://gemini.google.com/app; then explicitly reject and verify closure/no active modal. Preserve F481/F482 tests. No production/CMake/browser/network changes. Batch 59 is active; implementation pending.

F483 implementation adds one Gemini preview Copy/Open destination slot using the existing scoped provider restorer and HTTPS capture handler. It verifies exact anonymous clipboard prompt, one captured https://gemini.google.com/app URL, ActionRole modal persistence, and explicit close. Focused Ninja build succeeded in VS Developer environment; CTest passed 1/1; direct slot passed 3/0/0; diff check passed. Qt font/offscreen warnings only. Independent review is pending; Batch 59 remains active.

Independent review accepted F483. The Tester confirmed the provider restorer precedes the Gemini write, the scoped HTTPS handler captures exactly one https://gemini.google.com/app URL, and clipboard content matches the anonymous preview prompt. The ActionRole preview remains active until explicit rejection; timeout handles unexpected modals. Independent CTest passed 1/1; direct offscreen slot exited 0; diff check passed with an LF-to-CRLF notice. The Ninja executable lists the new slot. F481/F482 are unchanged. No full suite or browser/network launch. F483 is accepted and ready to commit; Batch 59 remains active until commit.

F483, `Phase2 - Cover Gemini preview Copy/Open destination (F483)`, committed as `3589209f` on `Qt-Rewrite` with exactly six scoped paths. Batch 59 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F484 discovery is starting.

F484 discovery selected Claude preview Copy/Open destination coverage after independent plan and code scans. F481-F483 cover ChatGPT, Custom Website, and Gemini; tests contain no Claude preview Copy/Open destination assertion. Production maps Claude to https://claude.ai/. Acceptance: add one focused slot to speaking_eval_batch_report_service_tests.cpp using synthetic eligible report data and the temporary settings fixture. Install the provider restorer before writing/syncing Claude; reuse the committed scoped HTTPS handler. Open preview, capture its anonymous prompt, click actual Copy/Open, assert the ActionRole preview remains open, clipboard exactly matches the preview (STD_NAME present; real student names absent), and exactly one intercepted URL equals https://claude.ai/; then explicitly reject and verify closure/no active modal. Preserve F481-F483 tests. No production/CMake/browser/network changes. Batch 60 is active; implementation pending.

F484 implementation adds one Claude preview Copy/Open destination slot using the existing scoped provider restorer and HTTPS capture handler. It verifies exact anonymous clipboard prompt, one captured https://claude.ai/ URL, ActionRole modal persistence, and explicit close. Focused Ninja build succeeded in VS Developer environment; CTest passed 1/1; direct slot passed 3/0/0; diff check passed. Qt font/offscreen warnings only. Independent review is pending; Batch 60 remains active.

Independent review accepted F484. The Tester confirmed the provider restorer precedes the Claude write, the scoped handler captures exactly one https://claude.ai/ URL, and the clipboard matches the anonymous preview prompt. The ActionRole preview remains active until explicit rejection; timeout handles unexpected modals. Independent focused CTest passed 1/1 in 8.64 seconds; direct offscreen slot exited 0 without output; diff check passed. The Ninja executable lists the slot. No full suite or browser/network launch. F484 is accepted and ready to commit; Batch 60 remains active until commit.

F484, `Phase2 - Cover Claude preview Copy/Open destination (F484)`, committed as `4116c83f` on `Qt-Rewrite` with exactly six scoped paths. Batch 60 is complete. The only post-commit worktree entries are the excluded pre-existing `agent_docs/latest_session_work.md` and `%SystemDrive%/`. F485 discovery is starting.

F485 discovery selected Microsoft Copilot preview Copy/Open destination coverage after independent plan and code scans converged. F481-F484 cover ChatGPT, Custom Website, Gemini, and Claude; no Copilot preview handoff assertion exists. Production maps Copilot to https://copilot.microsoft.com/. Acceptance: add one focused slot to speaking_eval_batch_report_service_tests.cpp using synthetic eligible report data and temporary settings. Install the existing provider restorer before writing/syncing MicrosoftCopilot; reuse the scoped HTTPS handler. Open preview, capture its anonymous prompt, click actual Copy/Open, assert the ActionRole preview remains open, clipboard exactly matches the preview (STD_NAME present; real names absent), and exactly one intercepted URL equals https://copilot.microsoft.com/; explicitly reject and verify closure/no active modal. Preserve F481-F484 tests. No production/CMake/browser/network changes. Batch 61 is active; implementation pending.

F485 implementation adds one Microsoft Copilot preview Copy/Open destination slot using the existing scoped provider restorer and HTTPS capture handler. It verifies exact anonymous clipboard prompt, one captured https://copilot.microsoft.com/ URL, ActionRole modal persistence, and explicit close. Focused Ninja build succeeded in VS Developer environment; CTest passed 1/1; direct slot passed 3/0/0; diff check passed. Qt font/offscreen warnings only. Independent review is pending; Batch 61 remains active.

Independent review accepted F485. The tester confirmed provider restoration precedes the MicrosoftCopilot write, exactly one https://copilot.microsoft.com/ URL is captured through the scoped handler, clipboard exactly matches the displayed anonymous prompt (STD_NAME present; Alice and 김민지 absent), and the ActionRole preview remains open until explicit rejection. The executable lists the slot; focused CTest passed 1/1 in 8.31 sec; direct offscreen slot exited 0; scoped diff check passed. Timeout cleanup covers unexpected modals. No full suite, browser, or network. F485 is accepted and ready to commit; Batch 61 remains active.

F485, Phase2 - Cover Copilot preview Copy/Open destination (F485), committed as f8453210 on Qt-Rewrite with exactly six scoped paths. Batch 61 is complete. The only post-commit worktree entries are the excluded pre-existing agent_docs/latest_session_work.md and %SystemDrive%/. Batch 62 F486 candidate discovery is underway; selection is pending.

F486 discovery selected a focused invalid Custom Website URL rejection test in mainwindow_custom_website_ai_comment_provider_action_parity_tests.cpp. Plan scan evidence: this branch was explicitly deferred in F466, F480, and F482. Source code validates HTTPS and a non-empty host before writes; an invalid URL restores the prior provider and shows the Invalid AI Website warning. Code scan also identified batch-dialog Copy/Open as an uncovered future candidate. Acceptance: temporary English settings, RAII restore provider and saved URL, start with ChatGPT and a valid saved URL, accept an HTTP replacement through the real chooser, verify exact warning title/message and dismissal, then assert ChatGPT remains selected and persisted and the saved URL is unchanged. No production/CMake/browser/network changes. Batch 62 is active; implementation pending.

F486 implementation adds one focused invalid-HTTP chooser slot. It verifies the exact Invalid AI Website warning, dismisses it safely, then confirms ChatGPT remains current and persisted and the original HTTPS URL is unchanged. The focused Ninja build succeeded; CTest passed 1/1; direct slot passed 3/0/0; scoped diff check passed. Routine Qt font/offscreen size-hint warnings only. No production/CMake/browser/network changes. Independent review is pending; Batch 62 remains active.

Independent review accepted F486. The tester verified the real chooser accepts http://invalid.example.test/, displays the exact warning title/message, then preserves ChatGPT as current/checked/persisted and the saved https://original.example.test/ URL. The RAII restorer runs before window destruction. Focused CTest passed 1/1; direct offscreen slot passed 3/3 including setup and cleanup; the executable lists the slot; scoped diff check exited 0 (LF-to-CRLF notice only). No browser/network. F486 is accepted and ready to commit; Batch 62 remains active.

F486, Phase2 - Cover Invalid Custom Website URL rejection (F486), committed as 964da4e3 on Qt-Rewrite with exactly six scoped paths. Batch 62 is complete. The only post-commit worktree entries are the excluded pre-existing agent_docs/latest_session_work.md and %SystemDrive%/. Batch 63 F487 candidate discovery is underway; selection is pending.

F487 discovery selected the batch AI dialog Copy/Open handoff. Plan scan evidence: F249/F252 cover batch eligibility/prompt planning and F477 verifies provider labels without clicking Copy/Open; the F486 log recorded this uncovered code-scan candidate. Code scan confirms SpeakingEvalAiBatchDialog connects speakingEvalAiBatchCopyOpen to copyPrompt(true), which copies the generated prompt and opens preferredProviderUrl(); no test clicks this batch button. Acceptance: one test slot in speaking_eval_batch_report_service_tests.cpp using a scoped provider restorer (capture before seeding/syncing Gemini), synthetic eligible report data with Alice and 김민지, create the batch prompt, click the actual button, assert exact clipboard equality with the anonymous prompt (STD_NAME present, names absent), and exactly one captured HTTPS URL equals https://gemini.google.com/app. No production/CMake/browser/network changes. Batch 63 is active; implementation pending.

F487 implementation adds one batch-dialog Copy/Open slot. It verifies STD_NAME is present, Alice and 김민지 are absent, clipboard exactly matches the generated prompt, and the scoped URL handler captures exactly one https://gemini.google.com/app URL. Focused Ninja build succeeded; CTest passed 1/1; direct slot passed 3/0/0; scoped diff check passed. Routine Qt font-directory warning only. Independent review is pending; Batch 63 remains active.

Independent review accepted F487. Provider restoration precedes the Gemini write; clipboard exactly matches the generated prompt with STD_NAME and without Alice/김민지; exactly one https://gemini.google.com/app URL is captured. The executable lists the slot. Focused CTest passed 1/1; direct native slot exited 0; scoped diff check passed. The direct offscreen invocation and a simple control both timed out under the same setup, so the offscreen cause is unresolved; no failure was reproduced in CTest or native direct execution. No full suite/browser/network. F487 is accepted and ready to commit; Batch 63 remains active.

F487, Phase2 - Cover batch AI dialog Copy/Open handoff (F487), committed as efa93a42 on Qt-Rewrite with exactly six scoped paths. Batch 63 is complete. The only post-commit worktree entries are the excluded pre-existing agent_docs/latest_session_work.md and %SystemDrive%/. Batch 64 F488 candidate discovery is underway; selection is pending.

F488 discovery selected the batch-dialog Include selection reset test. The F487 log listed this candidate; both current scans found no test for changing an Include checkbox after prompt/response/review generation. Source contract: resetGeneratedContent clears selected report indexes, prompt, response, review rows, and parse summary; disables Copy Prompt, Copy/Open, Parse, and Apply; Create Prompt remains enabled for an eligible remaining student. Acceptance: one slot with two eligible reports, create and parse a valid two-student response, uncheck the second Include item, verify the complete stale-state reset and button states, then regenerate and verify the prompt contains only the remaining student ID. The response-edit reset is an alternative; selection change exercises the broader reset. No production/CMake/browser/network changes. Batch 64 is active; implementation pending.

F488 implementation adds one focused batch-dialog selection reset slot. The test parses two Ready response blocks before unchecking the second Include item, then verifies prompt/response/parse summary/review rows clear, Copy/CopyOpen/Parse/Apply disable, Create remains enabled, and regeneration contains only STUDENT_01. Focused Ninja build succeeded; CTest passed 1/1; direct slot passed 3/0/0 in 19 ms; scoped diff check passed. Routine Qt font-directory warning only. Independent review is pending; Batch 64 remains active.

Independent review accepted F488. The Tester confirmed the full Include-checkbox reset and regenerated single-student prompt. Focused CTest passed 1/1; direct QT_QPA_PLATFORM=offscreen slot passed 3/3 including setup/cleanup; the executable lists the slot and its timestamp is newer than the source; scoped diff check exited 0 with only a line-ending conversion notice. No full suite/browser/network. F488 is accepted and ready to commit; Batch 64 remains active.

F488, Phase2 - Cover batch dialog selection reset (F488), committed as f4c10bfc on Qt-Rewrite with exactly six scoped paths. Batch 64 is complete. The only post-commit worktree entries are the excluded pre-existing agent_docs/latest_session_work.md and %SystemDrive%/. Batch 65 F489 candidate discovery is underway; selection is pending.

F489 discovery selected the batch-dialog response-edit reset. Plan scan points to the alternate recorded during F488; code scan confirms the response textChanged handler clears review rows and parse summary, updates Parse enabled state, and recalculates Apply, with no post-parse edit test. Acceptance: one slot with one eligible report; create a prompt, parse a valid response into one Ready review row and enabled Apply, edit response text, assert the edited response/prompt remain, stale review/summary clear, Apply disables, Parse remains enabled, and Copy/CopyOpen stay enabled; reparse and verify the review reflects edited content. F488 covers the broader Include-selection reset. No production/CMake/browser/network changes. Batch 65 is active; implementation pending.

F489 implementation adds one focused post-parse response-edit slot. It confirms a Ready review row and enabled Apply before editing; afterward the old review and summary clear, Apply disables, Parse and Copy/CopyOpen remain enabled, and reparsing reflects the edited phrase. Focused Ninja build succeeded; CTest passed 1/1; direct slot passed 3/0/0; scoped diff check passed. Routine Qt font-directory warning only. Independent review is pending; Batch 65 remains active.

Independent review accepted F489. The test verified stale review/summary invalidation on response edit, Apply disabled with Parse/Copy/CopyOpen still enabled, and successful reparse reflecting the edit. The executable lists the slot. Focused CTest passed 1/1; direct QT_QPA_PLATFORM=offscreen slot passed 3/3 including setup/cleanup; scoped diff check exited 0 with no whitespace error and an LF-to-CRLF notice only. No full suite/browser/network. F489 is accepted and ready to commit; Batch 65 remains active.

F489, Phase2 - Cover batch dialog response edit reset (F489), committed as db95dde0 on Qt-Rewrite with exactly six scoped paths. Batch 65 is complete. The only post-commit worktree entries are the excluded pre-existing agent_docs/latest_session_work.md and %SystemDrive%/. Batch 66 F490 candidate discovery is underway; selection is pending.

F490 discovery selected clearing the AI batch response after a successful parse. Plan and code scans converged; F489 covered nonempty edits, while clearing tests the separate empty-response branch that disables Parse. Acceptance: one eligible report; create prompt and parse valid response into one Ready row; clear response; assert review/summary clear, Parse/Apply disable, prompt remains unchanged and Copy/CopyOpen stay enabled; restore response and verify Parse and review recover. Dialog-level malformed-status mapping is an alternate for later work. No production/CMake/browser/network changes. Batch 66 is active; implementation pending.

F490 implementation adds one focused empty-response boundary slot. After a Ready parse, clearing response clears review/summary and disables Parse/Apply while preserving the prompt and Copy/CopyOpen; restoring valid response recovers a Ready row. Focused Ninja build succeeded; CTest passed 1/1; direct offscreen slot passed 3/0/0; scoped diff check passed. Routine Qt font-directory warning and line-ending notice only. Independent review is pending; Batch 66 remains active.

Independent review accepted F490. The focused CTest passed 1/1; the executable lists aiBatchDialogClearingResponseClearsStaleReviewState; direct offscreen slot exited 0; timestamp indicates a fresh build; scoped diff check exited 0 with only a line-ending notice. Clearing and response restoration states match acceptance. No full suite/browser/network. F490 is accepted and ready to commit; Batch 66 remains active.

F490, Phase2 - Cover batch dialog response clear reset (F490), committed as ac2b8042 on Qt-Rewrite with exactly six scoped paths. Batch 66 is complete. The only post-commit worktree entries are the excluded pre-existing agent_docs/latest_session_work.md and %SystemDrive%/. Batch 67 F491 candidate discovery is underway; selection is pending.

F491 discovery selected batch-dialog presentation of parser classifications. Parser unit tests already cover duplicate STUDENT_01, malformed STUDENT_03, valid STUDENT_02, and unknown STUDENT_99 outputs, but dialog mapping is uncovered. Source maps those IDs to Duplicate response blocks, Ready, and Malformed response block; summary uses parsed comment count and ignored unknown IDs. Acceptance: one UI slot with three eligible reports and those response blocks; assert exact row statuses, only valid row selected for application, and summary text reports 1 of 3 selected students parsed plus STUDENT_99 ignored. No production/CMake/browser/network changes. Batch 67 is active; implementation pending.

F491 implementation adds a batch-dialog mapping test for duplicate, valid, malformed, and unknown response blocks. It asserts the exact statuses, valid-row-only Apply check, and exact unknown-ID summary. Focused Ninja build succeeded; CTest passed 1/1; direct offscreen slot passed 3/0/0; scoped diff check passed. Routine Qt font-directory warning and line-ending notice only. Independent review is pending; Batch 67 remains active.

Independent review accepted F491. Exact duplicate/valid/malformed row mappings, valid-row-only check state, Apply enabled state, and unknown-ID summary passed. The executable lists aiBatchDialogParsesDuplicateMalformedAndUnknownBlocks. Focused CTest passed 1/1 in 8.53 sec; direct offscreen invocation exited 0 with no per-slot output; scoped diff check exited 0. No full suite/browser/network. F491 is accepted and ready to commit; Batch 67 remains active.

F491, Phase2 - Cover batch dialog parser status mapping (F491), committed as d6564476 on Qt-Rewrite with exactly six scoped paths. Batch 67 is complete. The only post-commit worktree entries are the excluded pre-existing agent_docs/latest_session_work.md and %SystemDrive%/. Batch 68 F492 candidate discovery is underway; selection is pending.

F492 selection: add a focused batch-dialog prompt privacy test for an unchecked classmate mentioned in selected feedback. The UI promises student-name removal, and the dialog passes names from all reports to the prompt builder. The test uses eligible Alice/Bob reports, leaves Bob unchecked, and verifies Alice's ID only, STD_NAME/CLASSMATE substitution, and no raw English/Korean names. Existing mixed-row tests already exercise Apply. The three plan docs record acceptance before implementation; test-only scope. Batch 68 is active.

F492 implementation is complete in a focused test slot for unchecked-classmate name redaction in selected AI batch feedback. Independent review accepted the assertions for selected Alice, unchecked Bob, STD_NAME/CLASSMATE, absence of Bob's ID for unchecked content, and removal of all four raw English/Korean names. Focused build and CTest passed 1/1; direct offscreen passed 3/0/0; slot listing and scoped diff check passed. Qt font-directory warning only. Test-only; F492 is ready to commit, Batch 68 remains active until commit.

F492, Phase2 - Cover unchecked classmate name redaction (F492), committed as 31f84450 with six scoped paths. Batch 68 is complete. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 69 F493 candidate discovery is underway; selection is pending.

F493 discovery compared two gaps: a plan-log selection-change prompt reset candidate (ambiguous and potentially overlapping F488) and the code-side absence of a test for opting out of a valid generated AI comment. Selected the review-row opt-out because Phase 2 §2.3/§2.4 and F252's checked/valid filtering contract support it, and its user-visible effect is precise. Acceptance: parse two valid rows, uncheck both and verify Apply disables, recheck one, apply, then assert exactly that row is accepted. Test-only expected; acceptance recorded before implementation. Batch 69 remains active.

F493 implementation adds aiBatchDialogAppliesOnlyRecheckedReadyComment. Two Ready comments begin checked; unchecking both disables Apply without changing Ready status; rechecking Bob and applying yields exactly Bob's expected comment at sourceRow 9 with no Alice comment. Independent verification accepted. Focused Ninja build, CTest 1/1, direct offscreen invocation, slot listing, and diff check passed; LF-to-CRLF advisory only. Test-only; F493 is ready to commit, Batch 69 remains active until commit.

F493, Phase2 - Cover batch comment opt-out (F493), committed as 57da53c7 with six scoped paths. Batch 69 is complete. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 70 F494 candidate discovery is underway; selection is pending.

F494 discovery selected the overwrite-decline UI state. Plan history documents the F252 confirmation gate; source/test discovery found the existing decline/retry path but it never shows the dialog, so continued visibility and preserved review state are not asserted. Acceptance: show the dialog, decline, assert it remains visible with the Ready selected comment intact and acceptedComments empty, then confirm and assert the existing accepted result. Test-only expected; acceptance recorded before implementation. Batch 70 remains active.

F494 extends aiBatchDialogConfirmsAcceptedCommentOverwrites to show the dialog and assert the non-destructive decline leaves it visible with the Ready/checked review row and comment unchanged and acceptedComments empty; the destructive retry still accepts the expected comment. Independent verification accepted. Focused Ninja build, CTest 1/1, direct offscreen, slot listing, and diff check passed; LF-to-CRLF advisory only. Test-only; F494 is ready to commit, Batch 70 remains active until commit.

F494, Phase2 - Cover overwrite decline state (F494), committed as b89fe5d4 with six scoped paths. Batch 70 is complete. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 71 F495 candidate discovery is underway; selection is pending.

F495 discovery compared dialog cancellation and malformed-response manual repair. Selected cancellation with an applicable checked Ready row: Phase 2 §§2.2/2.5 specify cancellation state/behavior, and source/test scans found no Cancel case exercising a staged Ready comment. Acceptance: create prompt, parse one valid response to Ready/checked with Apply enabled, click Cancel, assert Rejected and empty acceptedComments. Malformed-row repair remains a later candidate. Test-only expected; acceptance recorded before implementation. Batch 71 remains active.

F495 adds aiBatchDialogCancelDiscardsReadyComment: after a checked Ready comment is parsed and Apply enabled, the actual Cancel control returns Rejected, leaves acceptedComments empty, and hides the dialog. Independent review accepted. Focused Ninja build, CTest 1/1, direct offscreen slot, function listing, and diff check passed; LF-to-CRLF advisory only. Test-only; F495 is ready to commit, Batch 71 remains active until commit.

F495, Phase2 - Cover batch review cancellation (F495), committed as eb003134 with six scoped paths. Batch 71 is complete. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 72 F496 candidate discovery is underway; selection is pending.

F496 selection: test manual repair of a malformed parsed batch comment through the review cell. The Phase 2 log listed this as a later candidate; code confirms editing can recover the row. Acceptance: parse a malformed recognized block (Malformed/unchecked/Apply disabled), edit to a valid 100–420 character comment without STD_NAME, verify status Ready — name placeholder was omitted (the original response's advisory), checked and Apply enabled, then apply exactly that comment/sourceRow. This preserves parser-origin placeholder metadata and requires no production change. It is distinct from response-editor reset and initial status mapping. Acceptance was recorded before implementation. Batch 72 remains active.

F496 implementation adds aiBatchDialogRepairsMalformedReviewComment. The test checks malformed/unchecked/Apply disabled, then a valid 100–420-character edit without STD_NAME becomes Ready — name placeholder was omitted, checked and applicable; applying accepts exactly the edited comment at sourceRow 11. The advisory preserves original parser metadata. Independent verification accepted. Focused Ninja build and CTest 1/1 passed; direct offscreen slot, function list, and diff check passed; LF-to-CRLF advisory only. Test-only; F496 is ready to commit, Batch 72 remains active until commit.

F496, Phase2 - Cover malformed review recovery (F496), committed as da5540bb with six scoped paths. Batch 72 is complete. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 73 F497 candidate discovery is underway; selection is pending.

F497 selection: cover the missing-response-block status alongside one valid block in the batch dialog. F491 covered duplicate, Ready, malformed, and unknown IDs, but not the selected student with no matching block. Acceptance: parse only one of two selected reports; assert missing row is unchecked with Missing response block, valid row is checked/Ready and Apply remains enabled; apply exactly the valid sourceRow/comment. This tests Phase 2 structured results and behavior preservation; existing valid-row edit/apply coverage made that alternative less novel. Test-only expected; acceptance recorded before implementation. Batch 73 active.

F497 implementation adds aiBatchDialogAppliesValidCommentWhenOtherBlockIsMissing. With Alice and Bob selected, only Bob's block parses; Alice remains Missing response block/unchecked, Bob is Ready/checked, and Apply is enabled. Applying accepts exactly Bob's restored comment at sourceRow 17. Independent verification accepted. Focused Ninja build, CTest 1/1, direct offscreen slot, function listing and diff check passed; LF-to-CRLF advisory only. Test-only; F497 is ready to commit, Batch 73 remains active until commit.

F497, Phase2 - Cover missing response block status (F497), committed as f5f9b561 with six scoped paths. Batch 73 is complete. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 74 F498 candidate discovery is underway; selection is pending.

F498 selection: test sparse prompt-ID stability across an ineligible middle report. Source assigns IDs from original report indexes and uses those indexes for prompt construction, response parsing, and application mapping. Acceptance: eligible reports at indexes 0/2 around an ineligible index 1 produce IDs STUDENT_01 and STUDENT_03 only; parse valid blocks for both and assert two Ready/checked rows; Apply returns each original sourceRow/comment in order. This exercises Phase 2 typed identity and evaluation-edit mapping. Test-only expected; acceptance recorded before implementation. Batch 74 active.

F498 implementation adds aiBatchDialogKeepsOriginalIdsAcrossIneligibleReport. Eligible reports at indexes 0 and 2 keep prompt IDs STUDENT_01/STUDENT_03 around a disabled grade-3 report; ID 02 is absent. Two Ready/checked rows apply in order to sourceRows 5 and 14 with expected restored comments. Independent verification accepted. Focused Ninja build, CTest 1/1, direct offscreen slot, function listing and diff check passed; LF-to-CRLF advisory only. Test-only; F498 is ready to commit, Batch 74 remains active until commit.

F498, Phase2 - Cover AI batch IDs across report gaps (F498), committed as 76df67b6 with six scoped paths. Batch 74 is complete. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 75 F499 candidate discovery is underway; selection is pending.

F499 selection: cover the dialog's 420-character preferred maximum on review-cell edits. Phase 2/F251 defines 420 preferred and 450 hard maximum; app-level tests cover 420/421, while current dialog coverage does not assert edit-driven status/check/apply projection at that edge. Acceptance: parse valid placeholder-bearing output, edit to 420 (Ready/checked), then 421 (outside-preferred warning, still checked/applicable), and apply the exact 421-character text/sourceRow. Korean-only prompt fallback remains a later candidate. Test-only expected; acceptance recorded before implementation. Batch 75 active.

F499 implementation adds aiBatchDialogProjectsPreferredCommentLengthBoundary. Editing the parsed placeholder-bearing row to 420 characters reports 420/Ready/checked; at 421 it reports Ready — outside preferred length, remains checked, and Apply stays enabled. Apply returns the exact 421-character comment at sourceRow 23. Independent verification accepted. Focused Ninja build and CTest 1/1, direct offscreen slot, function listing and diff check passed; LF-to-CRLF advisory only. Test-only; F499 is ready to commit, Batch 75 remains active until commit.

F499, Phase2 - Cover preferred comment boundary (F499), committed as b253db85 with six scoped paths. Batch 75 is complete. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 76 F500 candidate discovery is underway; selection is pending.

F500 acceptance recorded: exercise the Korean-only fallback across prompt generation, response parsing, and apply. The prompt must not reveal the Korean name; parse must restore that name into a Ready, checked review comment; Apply must return the exact comment and original sourceRow. This closes the specifically deferred Korean-only prompt fallback gap with one eligible report. No production, CMake, browser, or network change expected. Batch 76 active.

F500 implementation adds aiBatchDialogUsesKoreanNameWhenEnglishNameIsBlank. It verifies default selection, prompt anonymity, Korean-name substitution, Ready/checked review, and exact accepted comment/sourceRow 37. Executor-focused Ninja build (VsDevCmd initialized), CTest 1/1, direct offscreen slot, function listing, and scoped diff check passed; direct slot noted a nonfatal font-directory warning. Independent verification is pending. Test-only; Batch 76 remains active.

F500 independent verification accepted. The Tester confirmed prompt anonymity for the Korean-only student and exact name restoration through the Ready/checked review row to Apply at sourceRow 37. Focused Ninja build, registered CTest 1/1, direct offscreen slot, function listing, and diff check passed; LF-to-CRLF advisory only. Test-only; F500 is ready to commit, Batch 76 remains active until commit.

F500, Phase2 - Cover Korean-only name fallback (F500), committed as 0f54d965 with six scoped paths. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 76 is complete. Batch 77 F501 candidate discovery is underway; selection is pending.

F501 acceptance recorded: add one focused batch-dialog test for Copy Prompt only. Generate an anonymous prompt for one eligible student, then verify clicking Copy Prompt copies the exact prompt and does not open a provider URL. The corresponding Copy Prompt and Open action already has coverage; this fills the separately deferred Copy-only handoff gap. Test-only expected; Batch 77 active.

F501 implementation adds aiBatchDialogCopyPromptCopiesWithoutOpeningUrl. It verifies the anonymous generated prompt is copied exactly through the standalone Copy Prompt action and no HTTPS URL handler is called. Executor focused Ninja build, CTest 1/1, direct offscreen slot, function list, and scoped diff check passed; nonfatal Qt font-directory warning only. Independent verification pending; test-only; Batch 77 active.

F501 independent verification accepted. The Tester confirmed Copy Prompt-only exact clipboard handoff with the captured URL handler unused. Focused Ninja build, registered CTest 1/1, direct offscreen slot, function listing, and diff check passed; LF-to-CRLF advisory only. Test-only; F501 is ready to commit, Batch 77 remains active until commit.

F501, Phase2 - Cover batch Copy Prompt handoff (F501), committed as 030ac493 with six scoped paths. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 77 is complete. Batch 78 F502 candidate discovery is underway; selection is pending.

F502 acceptance recorded: test the batch dialog with two eligible reports. After unchecking both prompt-input Include boxes, Create Prompt must be disabled and clicking it must leave the prompt empty; rechecking one must re-enable Create Prompt. F488 keeps one report checked during reset, and F493 tests review-table opt-out, so neither asserts this input-selection gate. The implementation already derives button state from whether any row is checked. Test-only expected; Batch 78 active.

F502 implementation adds aiBatchDialogDisablesPromptWhenNoReportsAreIncluded. It verifies the no-included-report boundary disables Create Prompt and keeps the prompt empty, then re-enables the action when one report is rechecked. Executor focused Ninja build, CTest 1/1, direct slot, function listing, and diff check passed; nonfatal Qt font-directory warning only. Independent verification pending; Batch 78 active, test-only.

F502 independent verification accepted. The Tester confirmed the zero-selection input gate and recovery after rechecking one row. Focused Ninja build, registered CTest 1/1, direct offscreen slot, function list, and diff check passed; LF-to-CRLF advisory only. Test-only; F502 is ready to commit, Batch 78 remains active until commit.

F502, Phase2 - Cover empty batch prompt selection (F502), committed as 4b95a041 with six scoped paths. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 78 is complete. Batch 79 F503 candidate discovery is underway; selection is pending.

F503 acceptance recorded: extend F492’s unchecked-classmate privacy guarantee to overlapping names. With selected Alice’s notes mentioning unchecked Alice Jones, prompt generation must replace the selected name with STD_NAME and the full classmate reference with CLASSMATE before a shorter match can leave “Jones” exposed; assert raw English/Korean names and fragments are absent. Add one dialog regression test and correct redaction ordering. The plan’s selection-reset candidate overlaps F488’s existing Include-checkbox reset and is deferred. Production code and test only; no CMake/browser/network change expected. Batch 79 active.

F503 implementation changes `promptItems()` to match escaped trimmed name alternatives against the original observations longest-first and replace each match once, preserving STD_NAME/CLASSMATE roles. `aiBatchDialogRedactsOverlappingUncheckedClassmateName` proved the original code leaked the suffix, then passed after the fix. Executor focused Ninja target, CTest 1/1, direct slot 3/0/0, function list, and scoped diff check passed; only nonfatal Qt font-directory and line-ending advisories. Independent verification pending; Batch 79 active.

F503 independent verification accepted. The Tester confirmed the Alice/Alice Jones overlap is fully redacted, selected and classmate tokens are correct, and distinct-name redaction remains intact. Focused Ninja target, registered CTest 1/1, direct offscreen slot, function listing, and diff check passed; LF-to-CRLF advisory only. F503 includes the prompt-redaction fix plus test and is ready to commit; Batch 79 remains active until commit.

F503, Phase2 - Fix overlapping classmate name redaction (F503), committed as b07c09af with seven scoped paths. Post-commit worktree contains only the excluded latest_session_work.md change and %SystemDrive%/. Batch 79 is complete. Batch 80 F504 candidate discovery is underway; selection is pending.

F504 acceptance recorded for Batch 80: exercise the Generate Comments page action from report creation through accepted-comment mapping and table apply. Put an unnamed source model row between named students; accept a response for the later named student; assert the original row receives the comment, surrounding rows stay unchanged, and one undo restores the original comment. The page handoff is not covered by current dialog sourceRow and separate table undo tests. Test-only scope; Batch 80 active.

F504 executor implementation is complete in the page-save test. generateCommentsAppliesToOriginalRowAndUndoesAsOneAction() exercises the Generate Comments page action with an unnamed row between Alice and Carol, a response for STUDENT_02 only, original-row application, unaffected surrounding comments, overwrite confirmation, and one undo restoring the previous comment. ClassMngrSpeakingEvalPageSaveTests built via VsDevCmd; registered CTest 1/1, direct offscreen slot 3 passed, function listing and scoped diff check passed. Initial modal-timer ordering and overwrite confirmation were corrected; the test includes a bounded safety close. Independent verification pending; Batch 80 active.

F504 independent verification accepted. The Tester independently reviewed the page sourceRow handoff and modal failure visibility, ran the focused CTest (1/1), direct offscreen slot (exit 0), and function listing, and confirmed the edited test source was newer than the up-to-date target object. The test verifies later-student row mapping across the unnamed gap, unchanged adjacent comments, confirmed overwrite, and a single undo restoring the original comment. Scoped diff check passed with only an LF-to-CRLF advisory. F504 is ready to commit; Batch 80 remains active.

F504, Phase2 - Cover page AI comment sourceRow handoff (F504), committed as f3ced0e5 with six scoped paths. Batch 80 is complete. Batch 81 F505 candidate discovery begins; selection is pending. Post-commit status contains only excluded agent_docs/latest_session_work.md and untracked %SystemDrive%/.

F505 acceptance recorded for Batch 81: exercise the page Generate Comments action through prompt creation and parsing of a valid Ready replacement, then click the batch dialog Cancel button. The page model/comment, undo count, and dirty state must remain unchanged. F495 covers cancellation inside the dialog; F504 covers the Accepted page path; no page-level Rejected result path is tested. No production change planned; Batch 81 active, implementation pending.

F505 executor implementation is complete in `cancelingGenerateCommentsLeavesPageStateUntouched()`. The page action opens the AI batch dialog for an existing comment; the test prepares a valid Ready replacement, clicks Cancel, and verifies original model rows, comment, dirty state, and undo count remain unchanged. Focused target built; CTest passed 1/1; direct offscreen slot 3 passed; `-functions` listed both F504/F505 slots; scoped diff check passed. Bounded safety close and explicit state checks are present. Only nonfatal offscreen font/icon/size-hint warnings and LF-to-CRLF advisory; independent verification pending, Batch 81 active.

F505 acceptance refinement before commit: assert the batch dialog result is `QDialog::Rejected` after clicking Cancel, because unchanged model/dirty/undo state alone does not distinguish rejection from an Accepted empty change list. The executor is adding visible result capture; focused and independent checks will be repeated. Batch 81 remains active.

F505 implementation refinement now captures the modal `finished(int)` result and asserts `QDialog::Rejected` immediately after the Cancel button click. This makes the page rejection branch explicit, beyond unchanged model, dirty state, and undo history. Focused build, CTest 1/1, direct offscreen slot 3/0/0, function list, and scoped diff check passed again; only nonfatal offscreen environment warnings. Independent reverification pending; Batch 81 active.

F505 independent reverification accepted. The Tester reviewed the `QDialog::finished(int)` capture and confirmed the explicit `QDialog::Rejected` assertion distinguishes Cancel from Accepted-with-no-changes. Focused build was up to date; registered CTest passed 1/1; direct offscreen F505 slot exited 0; function list includes both F504/F505; scoped diff check passed with only an LF-to-CRLF advisory. The Ready review, actual Cancel click, unchanged model/dirty state, zero undo entry, and bounded safety timer are covered. F505 is ready to commit; Batch 81 active.

F505, Phase2 - Cover page AI comment cancellation (F505), committed as 735ebf64 with six scoped paths. Batch 81 is complete. Batch 82 F506 candidate discovery is underway; selection is pending. Post-commit status contains only excluded agent_docs/latest_session_work.md and untracked %SystemDrive%/.

F506 acceptance recorded before implementation: exercise two accepted AI comments through the page action for named students around an unnamed row. Assert both comments map to original source rows, the unnamed row is unchanged, QUndoStack contains one command, and one undo restores both original comments. F504 covered only one accepted row; current page action passes the full list to one `applyChanges` command. No production change expected. Single-student peer-name redaction is not selected pending a broader privacy contract. Batch 82 active.

F506 executor implementation evolves the F504 page-action slot to multiple accepted comments. Alice and Carol receive valid STUDENT_01/02 responses across the unnamed source row; both target comments update, row1 remains unchanged, QUndoStack has one command, and a single undo restores both prior comments. F505 cancellation coverage remains intact. Focused build, CTest 1/1, direct offscreen slot 3 passed, function list and scoped diff check passed; LF-to-CRLF advisory only. Independent verification pending; Batch82 active.

F506 verification refinement: independent review rejected the implementation because it replaced the F504 Carol-only acceptance rather than preserving it. Retain the committed F504 single-result test unchanged in its own slot, add the two-student/one-undo scenario as a separate slot, keep F505 cancellation coverage, and rerun focused verification including both F504/F506 direct slots. Batch82 remains active.

F506 independent verification accepted after preserving prior slices separately. Tester compared F504’s Carol-only test body with commit f3ced0e5 and F505 cancellation with commit 735ebf64; both are unchanged. The new F506 slot covers two Ready/checked results across an unnamed row, maps source rows 0/2, keeps row1 unchanged, and restores both prior comments through one undo command. Focused build up-to-date, registered CTest 1/1, direct F504/F506 slots exit 0, all three page-action slots listed, scoped diff check passed (LF-to-CRLF advisory only). F506 is ready to commit; Batch82 active.

F506, Phase2 - Cover multi-comment page apply undo (F506), committed as e6891f3f with six scoped paths. Batch 82 is complete. Batch 83 F507 candidate discovery is underway; selection is pending. Post-commit status contains only excluded agent_docs/latest_session_work.md and untracked %SystemDrive%/.

F507 acceptance recorded before implementation: preserve F380 and add a distinct multi-day Sub Prep page integration case. Select Tue/Thu for two classes meeting on different selected days, leaving a Friday-only sentinel unselected. Verify the real page forwards selected weekdays, class IDs in existing order, and regular mode to the print-source read; verify the generated PDF includes both selected classes and omits the sentinel. This follows the Sub Prep plan’s selected scope/order/filter contracts (plan §/lines 179, 328; progress lines 3125, 3208, 3306) and the existing page test currently covers one class/day. Test-only expected. Batch 83 active, F507 implementation pending.

F507 independent verification accepted. The separate page integration slot selects Tuesday and Thursday through the real print dialog and uses class IDs 100 and 43 in schedule-cell order, with a Friday-only ID 44 sentinel. It verifies the application read request is [100,43] (so ascending numeric sorting would fail), regular mode, selected-class/teacher data, generated PDF markers for both selected classes, and sentinel absence. F380 remains a separate unchanged single-day slot; the independent tester confirmed its body matches HEAD after line-ending normalization. Focused build passed; direct F380 and F507 each passed 3/3; the function list includes F507; scoped diff check passed. Registered ClassMngrSubPrepPageTests remains red at 17 passed and 5 failed in untouched existing grading, campus, Zoom, username-save, and database-state slots; F507 passes in that full run. F507 is accepted and ready to commit; Batch 83 remains active until commit.

F507, “Phase2 - Cover multi-day Sub Prep page scope (F507),” committed as a5ff8b1c with six scoped paths. Batch 83 is complete. The commit records the independently accepted Tuesday 100 / Thursday 43 regular-mode request order, selected PDF markers, Friday sentinel exclusion, and F380 preservation. Its registered target CTest remains red at 17 passed/5 failed in the untouched grading, campus, Zoom, username-save, and database-state slots; F380 and F507 each pass individually 3/3. Batch 84 discovery found an uncovered Intensive-mode page print handoff and queued the explicit packaged Release memory gate after it. Post-commit worktree entries were only excluded agent_docs/latest_session_work.md and untracked %SystemDrive%/.

F508 acceptance recorded before implementation: add a separate Sub Prep page print-action test in tests/sub_prep_page_tests.cpp, preserving F380 and committed F507. Persist ScheduleWidget’s Intensive mode, open/accept the real Generate Sub Prep dialog, select Tuesday only for the target class, and assert the print-source request carries Intensive mode and that target class ID while request.selectedClassIds excludes the Friday-only sentinel. The page-level generation tests currently assert Regular; existing Intensive coverage tests summary refresh and Application query forwarding separately, not this page action. This is a contract-coverage gap, with no production defect asserted. F509 remains the packaged Release 96-class lifecycle/memory gate; the Sub Prep plan’s prior ~306 MiB settled samples exceed its 250 MiB target. Batch 84 active, F508 implementation pending.

F508 independently verified and accepted, ready to commit. The separate page-generation slot opens the real Generate dialog with persisted Intensive mode, selects Tuesday for class 4201, excludes the Friday-only 9901 sentinel, and verifies ScheduleViewMode::Intensive plus the filtered source request before asserting the expected Load Class Information warning from the deterministic fake source failure. The temporary SQLite schema is required because Generate refreshes the persisted schedule display preference. F380 and committed F507 remain unchanged. Focused ClassMngrSubPrepPageTests build passed; direct F508 and F380 each passed 3/3; -functions lists F380/F507/F508; scoped diff check passed. Registered ClassMngrSubPrepPageTests remains red: the full target reported 18 passed and 5 failed in the same untouched grading-settings, campus-details, Zoom-credentials, username-save, and clear-database-state slots. F508 passes directly. Batch 84 remains active until commit; F509 is next as the separate Packaged Release 96-class Sub Prep memory gate.

F508, “Phase2 - Cover Intensive Sub Prep page handoff (F508),” committed as 6d6f0186 with six scoped paths. Batch 84 is complete. The committed test uses the real Generate dialog and persisted Intensive preference, selects Tuesday for target class 4201, excludes Friday sentinel 9901 from request.selectedClassIds, and verifies the source-failure warning. Direct F508/F380 each passed 3/3; the registered target remains red at 18 passed/5 failed in the same untouched slots.

F509 is committed as 37621c16. Batch 85 evidence capture is complete; the 250 MiB target and output parity remain open. F510 diagnostic tracing is complete and F511 is next for the large Sub Prep-to-My Classes widget increase.


## F509 packaged Release evidence capture - 2026-10-09

Packaged Windows x64 Release configuration/build/install and the test harness build passed in the fresh external evidence root C:\Users\wfelt\AppData\Local\Temp\ClassMngr-F509-Evidence\subprep-20261009-01. lifecycle-sub-prep and output-sub-prep each completed normally; the selected-route validator reports no failures. Scope is limited to 2/24 Phase 0 routes on Windows. macOS universal and the other Windows routes remain missing.

The 96-class Sub Prep lifecycle completed two refresh/leave/re-entry cycles. At the Sub Prep lifecycle checkpoint: 226,119,680 working-set bytes and 215,777,280 private bytes. The output route generated 2 PDFs (17 pages, 139,650 combined bytes) and released the output operation/documents. At release: 233,619,456 working-set and 220,405,760 private bytes; peak working set 242,655,232 bytes. Leaving Sub Prep for My Classes raised working set to 300,208,128 bytes and widget count from 2,168 to 6,412. After two PDF loads/renders/releases, the output route settled at 307,519,488 bytes at 1s and 307,523,584 bytes at 5s; private usage was 324,579,328 bytes and peak working set 316,862,464 bytes. The final sample exceeds the 262,144,000-byte 250 MiB target. The route does not compare generated output visually against a reference, and Phase 0 full 24-route Windows/macOS coverage remains incomplete.

Batch 85 evidence capture is complete, with memory acceptance still open. F510 diagnosis is recorded below; Batch 86 is complete and F511 is selected in Batch 87.


## F510 diagnostic result - 2026-10-09

The F509 output route records widget/memory checkpoints after calling PageManager::showPage and processing events. At sub-prep-output-operation-released: widgetCount 2,168, instantiatedPageCount 9, working set 233,619,456 bytes. After My Classes activation: widgetCount 6,412, instantiatedPageCount 10, working set 300,208,128 bytes. Delta: 4,244 widgets and 66,588,672 working-set bytes.

Code tracing confirms the construction path. MyClassesPage::refresh calls rebuildClassInformation. After loading class summaries and building navigation, both the flat-class and grade-group loops call createClassPage for every tab. Each page builds teacher and class detail cards plus multiple read-only fields/editors. The 96-class fixture therefore creates detail widgets for every class on first activation, even though only one class is selected. PageManager stores each created page in m_pages and the QStackedWidget; page leave deactivates it but does not remove the My Classes page or clear its generated tree. The route samples after showPage returns and app.processEvents, so the observed count includes this activation and construction.

This is strong causal evidence for the measured widget growth and resident increase, while the route does not measure a per-object byte attribution. F510 diagnostic slice is complete. F511 is selected in Batch 87: retain all navigation labels/order and batched summary data, build detailed UI only for the active class, clear the previously active detail tree when selection changes, and confirm selection/content parity plus the packaged Release memory route. Keep the 250 MiB gate open until measured.


## F511 My Classes detail materialization

F511 changed My Classes to build details only for the selected class while retaining the full class/grade navigation. The Windows x64 Debug Ninja build and focused CTest passed for the three My Classes test targets. Packaged Release evidence was validated for lifecycle-sub-prep and output-sub-prep: the output route's My Classes activation checkpoint fell to 2,471 widgets and 234,868,736 working-set bytes from F509's 6,412 widgets and 300,208,128 bytes. The output route later settled below 250 MiB, but the lifecycle route still peaked at 280,035,328 bytes during PDF reopen/render. Only 2 of 24 Windows Phase 0 routes are covered; macOS universal and visual output parity remain unverified. F512 is selected to trace the lifecycle peak before a remedy is chosen.
## F512 lifecycle-sub-prep peak diagnosis

In the validated F511 lifecycle route, the working set was already 263,041,024 bytes after My Classes activation (2,808 widgets, 10 pages), before loading a PDF. The PDF catalog-ready sample was 265,170,944 bytes with no live PDF document. Opening/rendering raised current working set to 278,790,144 bytes and the process high-water mark to 280,035,328 bytes; the PDF viewer then closed the document and its live count returned to zero. The route settled at 270,618,624 bytes at 1s, still 8,474,624 bytes above 250 MiB. Private working set after release was only 860,160 bytes above its pre-load value, so the route does not identify the exact owner of the residual total working set.

The lifecycle transition into My Classes added 321 widgets; MyClassesPage is cached and does not release its active details on deactivation. F513 is selected to clear that hidden detail tree and rematerialize it from retained summaries on return, preserving the selected class without a new summary read. The F511 output route's five-second working set was 240,316,416 bytes; lifecycle memory and output visual parity remain open.

## F513 My Classes hidden-detail lifecycle - 2026-10-09

F513 synchronously clears selected details on leave and rematerializes them on activation from retained summaries. Flat and grouped re-entry tests preserve one summary-list query; focused Windows x64 Debug CTest passed 3/3 across the three My Classes targets. A fresh current-source Windows x64 Release package passed validation for lifecycle-sub-prep and output-sub-prep, but its reports contain no My Classes runtime metrics, so they do not independently validate F513 behavior. Lifecycle settled-1s working set was 270,848,000 bytes (peak 280,162,304); output settled-5s was 239,767,552 bytes (peak 249,077,760). These process-level samples do not attribute bytes to My Classes; 250 MiB lifecycle acceptance, full Phase 0 coverage, and visual parity remain open. F513 was committed as 139fb17cef7e8c83e404049b968f4166c8d5f28f; F514 is complete and its evidence follows; F515 is next to release the hidden Classes navigation tree while retaining summary data and nested editor state.

## F514 Classes lifecycle diagnosis - 2026-10-09

F514 paired lifecycle-classes and lifecycle-sub-prep passed Windows x64 selected-route validation (83 total checkpoints, including 12 Classes lifecycle checkpoints; packaged app and harness both exited 0). Across two leave/re-entry cycles, the 473 navigation widgets and one instantiated/loaded editor stayed stable; selection persisted and re-entry added no rebuild or class/info queries. Each explicit refresh added one rebuild and one class-info query; separate teacher queries remained 0 while class-info and teacher result rows were 288. The stale opt-in test expectations were corrected to 3 batched class-info calls and 0 separate teacher calls, retaining all 288-row assertions. The final process-level working set was 275,021,824 bytes after later workflow pages and is not attributable to ClassesPage. Full Phase 0 coverage, the 250 MiB memory gate, and visual parity remain open. Batch 90 is complete.

F515 is accepted in Batch 91. Classes now retains value-only navigation summaries and nested editors while synchronously releasing hidden navigation widgets. Fresh return through the real sidebar route does not reread class or class-info data; stale return refreshes after successful TeacherInfo save, external Schedule save, or accepted Schedule Import. Failed/cancelled writes remain query-neutral, current-page explicit selection keeps its refresh behavior, and retranslation recomputes section eligibility without recreating released widgets. Independent focused checks passed, and the three MainWindow integration CTests passed 3/3. The full ClassesPage target remains red at 30/39 because nine unchanged tests use closed-session or mismatched preference/grade stubs; neither F515-specific case failed, and the full-target baseline was not established. Batch 91 is complete. F516 begins Batch 92 with paired packaged Release lifecycle routes for Classes and Sub Prep; the 250 MiB gate, full Phase 0 coverage, and visual parity remain open.

F516 is accepted in Batch 92. A fresh current-source Windows x64 Release package and Debug harness passed route-scoped validation for `lifecycle-classes` and `lifecycle-sub-prep` (83 checkpoints each). The Classes route recorded 473/0/473 navigation widgets across entry/leave/re-entry, one retained editor, three class and three class-info queries only across initial load plus two explicit refreshes, and five navigation builds including the two rematerializations. One-second settled working sets were 266,936,320 bytes (Classes) and 262,729,728 bytes (Sub Prep); both exceed 262,144,000 bytes. The harness provides no 5-second sample, so Batch 93/F517 will add one for both lifecycle routes. These are process-wide measurements; the full 24-route Windows/macOS gate and visual parity remain open.

F517 is accepted in Batch 93. The paired packaged Windows x64 Release `lifecycle-classes` and `lifecycle-sub-prep` routes both passed validation with 84 checkpoints and 0 app/harness exit codes. Both now emit ordered one- and five-second settled measurements. Five-second working set/private usage was 267,243,520/289,345,536 bytes for Classes and 262,823,936/283,787,264 for Sub Prep; both remain above the 262,144,000-byte target. The Classes route ended with 473/0/473 navigation widgets, one editor, three class/class-info queries each, and five navigation builds. Measurements remain process-wide. Batch 94/F518 will add per-page and selected-editor widget descendant counts to diagnose ownership across Classes leave/re-entry; the memory, full Phase 0, and visual gates remain open.

F518 is accepted in Batch 94. PageManager and the selected Classes editor now report value-only recursive descendant counts. Independent tests and the packaged Windows x64 Release paired `lifecycle-classes`/`lifecycle-sub-prep` run passed; both route harnesses exited 0 and validation passed. Per-page JSON keys matched instantiated pages at the Classes checkpoints and settled snapshots; class/editor counts were recorded across both leave/re-entry cycles. No data reads were added on fresh Classes re-entry. Five-second working sets remain over 262,144,000 bytes: Classes 267,280,384 and Sub Prep 262,733,824. The Classes subtree count changed with refresh and re-entry, so the data identifies widget-count movement but does not attribute memory. The standard Visual Studio Debug tree remains blocked before compilation by FileTracker `E_ACCESSDENIED`; focused checks passed in the existing Ninja tree. The full 24-route Windows/macOS gate and visual parity remain open. Batch 95/F519 will compare schedule-row and navigation-root descendants across refresh, event-loop return, and leave before selecting any memory remedy.

F519 is accepted in Batch 95. Focused tests and the fresh packaged Windows x64 Release paired routes passed. Current/live schedule rows showed deferred cleanup from 8/24 after refresh 1 to 9/9 at the natural event-loop return; direct navigation roots showed 1/2 after refresh, 1/1 after the normal return, 0/0 on synchronous leave, and 1/1 on re-entry. The Classes route added no class or class-info reads on re-entry. Five-second working sets remain over 262,144,000 bytes: Classes 267,591,680 and Sub Prep 263,360,512. This establishes the widget cleanup timing, not a byte attribution. The standard Visual Studio Debug tree still fails before compilation at FileTracker `E_ACCESSDENIED`; focused Ninja checks passed. The full 24-route Windows/macOS gate and visual parity remain open. Batch 96/F520 will assess whether synchronous disposal of replaced tab roots is safe and useful before any deletion policy changes.

## Phase 2 current progress - 2026-10-12

F740 Gregorian date conversion across the Calendar recurrence and series-edit
planners is accepted and committed as
`9d61d6aef53ce0893e3ffc069338f1ba1a11e3df`. In the existing Windows x64
Debug Ninja tree under the VS 2026 x64 Developer environment, `ClassMngr` and
`ClassMngrNextApplicationCalendarEventTests` built; exact CTest registration
#246 (count 1) passed 1/1 in 0.29 seconds. The scoped diff check passed; no
full suite ran.

F741, reusing `ScheduleKeyRules::parseWeekday` in
`ClassDetailsSaveWorkflow`, is accepted and committed as
`c518f0d4766538b5c584949e72a79845a7a18dfa`. In the Windows x64 Debug Ninja
tree under VS 2026 x64 Developer, `ClassMngr` and
`ClassMngrNextApplicationClassDetailsSaveUseCaseTests` built; exact CTest
registration #213 (count 1) passed 1/1 in 0.03 seconds (0.04 total). The
scoped diff check passed; no full suite ran. Only nonblocking `vswhere.exe`
and LF/CRLF warnings occurred.

F742, reusing `Domain::calendarEventTypeFromName` in Sub Prep interval
filtering after trimming, is accepted and committed as
`e8487f180550e03f62c4efa5c098baaf0ea6f473`. It preserves the exact-case
Vacation/Holiday filter and trimmed output; existing tests were reused
unchanged. In the Windows x64 Debug Ninja tree under VS 2026 x64 Developer,
`ClassMngr` and `ClassMngrNextApplicationSubPrepCalendarEventIntervalsQueryTests`
built; the single CTest registration #307 passed 1/1 in 0.05 seconds (0.06
total). The scoped diff check passed; no full suite ran. Only nonblocking
`vswhere` and LF/CRLF warnings occurred.

F743 is accepted at code commit
`893e400be3988e9bad7fbd5940e76799dc083d79` (`Phase2 - Share required field
validation across testing classes`). Under VS 18.0 x64 Developer, Ninja/Debug
in `build/windows-x64-debug-ninja` with Hostx64/x64 MSVC toolset 14.51.36231
and compiler 19.51.36257, `ClassMngr`,
`ClassMngrNextApplicationTestingClassCreateUseCaseTests`, and
`ClassMngrNextApplicationTestingClassDetailsUpdateUseCaseTests` built with
exit code 0. Exact CTest registrations #374 and #375
(count 1 each) passed via the exact CTest regex, 2/2 in 0.10 seconds. F743
reused existing tests without test edits. Automatic CMake reconfigure took
226 seconds; nonblocking warnings covered `vswhere`, missing Vulkan with Qt
zlib fallback, and long object paths. No full suite ran.

F744 is accepted at code commit
`1d8a3c4b18749d2d77e2213660d528d759048534` (`Phase2 - Share schedule mode
validity across Sub Prep`). It added `isValidScheduleViewMode` beside the
schedule projection type and reused it in Sub Prep summary, print, and roster
queries while preserving caller-specific guards and errors. The
`ClassMngr` target and all three query test targets built in the Windows x64
Debug Ninja tree under VS 18.0 x64 Developer (MSVC 14.51.36231/compiler
19.51.36257); exact CTest registrations #204, #205, and #237 (one each)
passed 3/3 in 1.12 seconds. No full suite ran.

The separately requested CMake optimization is committed as
`cbfac3c1682ef99ad3558adb7da4871a87fa0020` (`Build - Index source ownership
validation`). `cmake/source_ownership.cmake` now builds a normalized-path
owner index instead of repeatedly comparing every inventory path against
every owner record. Two ordinary configurations passed in the same Windows
Ninja tree in 40.942 and 30.165 seconds; both retained the exact 416-name
CTest inventory with SHA-256
`fd05abf7731f5d337fef767793272447b11061d6f6a01790dfd9a25ebe713f3b`. The
older 226-second configure observation was not causally attributed to this
validation loop. No build or tests ran for this CMake change.

F745 is accepted at code commit
`d813248e74a42ae138d384536d9ed5568039566f` (`Phase2 - Share Sub Prep weekday
validity`). It moved `SubPrepWeekday` into an Application header and uses one
validity helper in Sub Prep summary, print, and roster request and
returned-meeting checks while preserving caller-specific validation order and
errors. Under VS 18 x64 Developer, `ClassMngr` and all three query targets
built in `build/windows-x64-debug-ninja`; exact CTest registrations #204,
#205, and #237 (one each) passed 3/3 in 1.11 seconds (0.04, 0.32, and 0.74
seconds). No full suite ran.

F746 is accepted and committed as
`8cab6fa9e60a0a04f949fc9f28c484687507199c` (`Phase2 - Share Sub Prep source
identifier validation`). The shared blank-and-bounded typed-ID rule now
serves the print and roster queries while preserving each caller's errors and
read boundaries. Under VS 18 x64 Developer, `ClassMngr` and both query test
targets built; unique CTest registrations #205 and #237 passed 2/2 in 1.08
seconds. No full suite ran.

F747 is accepted and committed as
`892d05dd5db4cf57dc9d7da1381b2afe6ce4d8f5` (`Phase2 - Reuse source identifier
validation in schedule summary`). It removed the duplicate identifier rule
from the summary query and reused F746's shared check, retaining
length-before-whitespace scanning and the existing validation order. Under
VS 18 x64 Developer, `ClassMngr` and the summary, print, and roster query test
targets built. Unique CTest registrations #204, #205, and #237 passed 3/3 in
1.09 seconds. No full suite ran.

F748 is accepted and committed as
`f773cb0bea7867bc7e47912848317343a2d67e07` (`Phase2 - Cover Sub Prep
identifier validation precedence`). Its focused test proves two identical
whitespace-only IDs return the malformed-ID error before duplicate detection,
with nonrecoverable `InvalidInput` and zero reads. `ClassMngr` and the summary
query target built; unique CTest #204 passed 1/1 in 0.04 seconds. No full
suite ran.

F749 is accepted at commit
`318f0b3a1806e21ff4eded355f981ee17fe46112` (`Phase2 - Reuse Gregorian date
validation for Sub Prep intervals`). It reuses
`Domain::GregorianDate::fromIsoString` and preserves `std::chrono::sys_days`
range comparisons. `ClassMngr` and the interval-query test target built;
unique CTest #307 passed 1/1 in 0.05 seconds. No full suite ran.

F750 is accepted at commit
`a129057b77c08f6fceef1bdaa0d3ea00008b8cc0` (`Phase2 - Share Sub Prep
selected-day validation`). One shared typed validator now enforces the seven-
day maximum, weekday-enum validity, and duplicate rejection for the schedule-
summary, print-source, and roster-output queries. Each query retains its own
error messages and surrounding validation order. Under VS 18 x64 Developer,
`ClassMngr` and all three query test targets built. Unique CTest registrations
#204, #205, and #237 passed 3/3 in 1.11 seconds. No full suite ran. An initial
build outside the Developer environment lacked MSVC standard-library include
paths; the Developer-environment rerun passed, with only a nonfatal
`vswhere.exe` notice.

F751 is accepted and committed as
`990a5469b8a2268ddb27bc46d034ce5762ec4d31` (`Phase2 - Reuse Gregorian date
validation in Teacher import`). It replaced the duplicate ISO/Gregorian
parser with `Domain::GregorianDate::fromIsoString`, retaining the existing
validation flag and rejecting invalid dates before persistence. Under VS 18
x64 Developer, `ClassMngr` and
`ClassMngrNextApplicationTeacherImportUseCaseTests` built; unique CTest #265
passed 1/1 in 0.02 seconds. An initial shell lacked MSVC include paths; the
Developer-environment rerun passed. No full suite ran.

F752 is accepted and committed as
`1dee0b06543c053badfba56bef542417e7177efe` (`Phase2 - Reuse ordered duplicate
name grouping in speaking evaluation`). Speaking-evaluation validation now
uses `Domain::duplicateStudentNamePairGroups`, retaining Unicode trimming,
incomplete-pair exclusion, first-seen group and row order, issue metadata,
duplicate-row metadata, and the pre-persistence validation boundary. Under
the Windows x64 Ninja Debug tree and VS 2026 x64 Developer environment,
`ClassMngr` and
`ClassMngrNextApplicationSpeakingEvaluationSaveUseCaseTests` built. Unique
CTest #229 passed 1/1 in 0.05 seconds (0.08 seconds total). No full suite ran.

F753 is accepted and committed as
`ef92df3a0b881bccd0fbfb52b3f88e39ac291255` (`Phase2 - Share Qt whitespace rule
in birthday schedule`). The schedule blank-name check now reuses
`Application::isQtWhitespace`, recognizing U+0085 while preserving ASCII
birthday parsing and all other schedule behavior. Under Windows x64 Ninja
Debug with VS 2026 x64 Developer, `ClassMngr` and
`ClassMngrNextApplicationUpcomingBirthdayScheduleUseCaseTests` built. Unique
CTest #193 passed 1/1 in 0.03 seconds (0.04 seconds total). No full suite ran.

F754 is accepted and committed as
`006593ab40e4791bc9442056eea58961430e1e6c` (`Phase2 - Reuse Gregorian date
validation for birthdays`). `isValidMonthDayBirthday` now delegates calendar
validity to `Domain::GregorianDate::fromComponents(2000, month, day)` while
preserving the application-owned Unicode trimming, blank behavior, exact
two-ASCII-digit syntax, and leap-year-2000 semantics. Under VS 18 x64
Developer, `ClassMngr` and the Native English and GS Team test targets built.
Unique CTests #273 and #274 passed 2/2 (0.07 seconds total). An initial build
without the Developer environment lacked MSVC standard-header configuration;
the Developer-environment rerun passed. No full suite ran.

F755 is selected in Batch 185 to share canonical positive-decimal integer ID
validation across `ClassCoTeacherPageReadQuery`,
`ClassCoTeacherTeacherChoicesReadQuery`, `ClassByIdReadQuery`, and
`ClassesListReadQuery`. Add a small Application helper for nonempty canonical
positive decimal values that parse fully into `int` and round-trip through
`std::to_string`; register its header in `cmake/next.cmake`. Preserve each
caller’s error code/message/recoverability, validation and port-read order,
and `ClassesListReadQuery` duplicate-ID precedence. Run the existing focused
tests, which cover zero, negative, leading-zero, malformed/trailing, and
overflow IDs (plus co-teacher page plus/leading-whitespace cases). The audit
found gaps in malformed/trailing inputs and exact caller-specific assertions;
strengthen the four existing tests as follows:

- Co-teacher page: add `42x`; assert exact invalid-ID and snapshot-mismatch
  messages, nonrecoverability, and zero calls for invalid request IDs.
- Teacher choices: add `42x`; assert its exact invalid-teacher-ID message,
  nonrecoverability, and the existing one-port-call boundary.
- ClassById: assert invalid-input recoverability is `true`; assert the exact
  snapshot-mismatch message and retain the existing zero-call boundary.
- ClassesList: add `42x`; assert the exact invalid-ID message separately from
  the duplicate-ID message, retaining nonrecoverability and one port call.

Build
`ClassMngr` and the four corresponding query test targets; confirm exactly
one registration for each and run only those four exact CTests. No full
suite. F707 and F709 remain deferred. Phase 2 remains In Progress/Open with
Gates 1 and 2 Partial; F625's assessment is accepted, F626 is deferred, and
separate current-source Phase 0 evidence remains Windows x64 2/24 and macOS
universal 0/24.

## Phase 2 current position — 2026-10-13 (F755 accepted; F756 selected)

F755 is accepted and committed as
`ae93c500716e1c23552178b1b61d2e29e1f14439` (`Phase2 - Share canonical
positive decimal ID validation`). The shared Application helper now serves
the co-teacher page, teacher-choice, ClassById, and classes-list queries while
preserving their distinct error contracts and validation/read boundaries.
Under VS 18.10.1 x64 Developer with the existing Windows x64 Debug Ninja
tree, `ClassMngr` and all four focused query test targets built. Each exact
CTest name had one registration; the four focused CTests passed 4/4 in 0.13
seconds. CMake reconfigured after detecting the new helper. The build also
reported missing optional `WrapVulkanHeaders` and unrelated object-path-length
warnings; neither blocked the requested targets. No full suite ran.

F756 is selected in Batch 186 to reuse
`Application::isCanonicalPositiveDecimalId` in
`MyClassesClassInformationBatchReadQuery`,
`MyClassesStudentCountBatchReadQuery`, and
`MyClassesTeacherProfileBatchReadQuery`. Replace only their duplicate local
positive-decimal parsers; the helper is already registered in
`cmake/next.cmake`, so no CMake inventory change is expected. Preserve each
caller's error code, exact message, recoverability,
validation/duplicate precedence, and zero-port-call boundary for invalid and
duplicate request IDs. Keep successful empty requests port-free and existing
nonempty reads, entry-count/order checks, and row-level results unchanged.
The focused tests must assert these exact caller contracts:

- Class information: invalid IDs return `InvalidInput` with
  `My Classes class IDs must be canonical positive integers.`; duplicate IDs
  return `InvalidInput` with `My Classes class IDs must be unique.`. Both are
  nonrecoverable and make zero port calls.
- Student count: invalid and duplicate class IDs return the same respective
  `InvalidInput` messages as class information, remain nonrecoverable, and
  make zero port calls.
- Teacher profile: invalid IDs return `InvalidInput` with
  `My Classes teacher IDs must be canonical positive integers.`; duplicate
  IDs return `InvalidInput` with `My Classes teacher IDs must be unique.`.
  Both are nonrecoverable and make zero port calls.
- Class information validates an optional returned teacher ID only after
  one port read; a malformed ID returns `Validation` with
  `A My Classes class-information batch returned an invalid teacher identifier.`
  and is nonrecoverable.

Strengthen the three focused tests before verification: add `42x` to each
invalid request-ID set; assert the exact messages above. For class
information, also exercise a returned teacher ID of `19x` and assert its
exact post-read validation contract; preserve the existing leading-zero
returned-teacher case. Build `ClassMngr` and
`ClassMngrNextApplicationMyClassesClassInformationBatchReadQueryTests`,
`ClassMngrNextApplicationMyClassesStudentCountBatchReadQueryTests`, and
`ClassMngrNextApplicationMyClassesTeacherProfileBatchReadQueryTests`; confirm
exactly one CTest registration for each using those full names, then run only
those three exact CTest registrations. No full suite. F707 and F709 remain
deferred. Phase 2 remains In Progress/Open with Gates 1 and 2 Partial;
separate current-source Phase 0 evidence remains Windows x64 2/24 and macOS
universal 0/24.

## Phase 2 current position — 2026-10-13 (F756 accepted; F757 selected)

F756 is accepted and committed as
`1fdabd5a4b656c68baaa0426def3e8ffe5af4b2b` (`Phase2 - Reuse canonical
positive decimal ID validation in My Classes batches`). The three My Classes
batch queries now reuse the Application validator, including the class-
information returned-teacher check after the port read. Under VS 18.10.1 x64
Developer/MSVC 14.51.36231, `ClassMngr` and all three focused query targets
built. CTest registered each full `ClassMngr...Tests` name once; all three
passed 3/3 in 0.10 seconds. No full suite ran.

F757 is selected in Batch 187 to reuse
`Application::isCanonicalPositiveDecimalId` in
`TeacherProfileReadQuery` and `TeacherDisplayNameBatchReadQuery`. Replace
their local canonical-ID predicates only; the helper is already in
`cmake/next.cmake`, so no CMake inventory change is expected. Preserve these
caller contracts and read boundaries:

- Teacher profile: invalid IDs return `InvalidInput` with
  `Teacher ID must be a canonical positive integer.`, recoverable, before any
  port call. Preserve the exact mismatched-snapshot `Validation` error and
  nonrecoverability after one port read.
- Teacher display-name batch: invalid request IDs return nonrecoverable
  `InvalidInput` with `Teacher IDs must be canonical positive integers.`;
  duplicate IDs return nonrecoverable `InvalidInput` with
  `Teacher IDs must be unique.`. Both fail before the port read; empty input
  remains successful without a read. For returned rows, preserve validation
  order (canonical ID, requested membership, duplicate identity, requested
  order), partial results, and row order. A noncanonical returned ID remains
  nonrecoverable `Validation` with
  `A teacher display name batch returned a noncanonical teacher identifier.`
  after one port call.

Extend the two focused tests: add `42x` to both invalid-ID cases and assert
the exact invalid-input messages and recoverability/read boundaries. Assert
the profile mismatch's exact message. In the batch output test, retain the
leading-zero returned ID and add malformed `42x`, asserting the exact
noncanonical-output error, nonrecoverability, and one call for each failed
read; retain unexpected, duplicate, misordered, empty, and partial-result
coverage. Build `ClassMngr` and
`ClassMngrNextApplicationTeacherProfileReadQueryTests` plus
`ClassMngrNextApplicationTeacherDisplayNameBatchReadQueryTests`; confirm one
CTest registration for each exact full name and run only those two CTests.
No full suite. F707/F709 remain deferred. Phase 2 remains In Progress/Open
with Gates 1 and 2 Partial; separate current-source Phase 0 evidence remains
Windows x64 2/24 and macOS universal 0/24.

## Phase 2 current position — 2026-10-13 (F774 accepted; F775 selected)

F774 is accepted at
`e0481c1564a77fa0e85ae1edd3c0c9dfb5e5f8fa`. Roster Save, Roster Row
Transfer, and Speaking Evaluation Save now use the shared canonical-positive-
int validator while preserving distinct errors, precedence, and port behavior.
Their exact CTests each registered once and passed 3/3 after the bounded
Visual Studio 18 x64 Debug build succeeded outside the sandbox following
FileTracker `E_ACCESSDENIED`. No full suite ran.

F775 is selected to pin exact Class Details Page query failure contracts in
its existing focused test. Cover blank/oversized request IDs with the exact
nonrecoverable InvalidInput message and zero reads, and mismatched returned
identity with its exact nonrecoverable Validation message and one read. Keep
opaque bounded IDs, overall port-error forwarding, and independent partial-
field results unchanged. No source or CMake change is needed. F707/F709 remain
deferred. Phase 2 remains In Progress/Open with Gates 1 and 2 Partial; separate
current-source Phase 0 evidence remains Windows x64 2/24 and macOS universal
0/24.

## Phase 2 current position — 2026-10-13 (F775 accepted; F776 selected)

F775 is accepted at `5ac1107fe87a6b1eba266c995467c61bbcd899a2`. Its exact
Class Details Page query CTest registered once (#232) and passed 1/1 after
`ClassMngr` and the focused target built in the existing Visual Studio 18 x64
Debug tree. The sandbox build hit FileTracker `E_ACCESSDENIED`; the same
bounded build succeeded outside the sandbox. No full suite ran.

F776 adds an application-level identity check to Teacher Profile Edit reload:
a successful reload for a different teacher must fail as `ReloadFailed`,
preserve `writeSucceeded=true`, and report nonrecoverable `Validation`. The
acceptance matrix is recorded in the Phase 2 progress log. F707/F709 remain
deferred. Phase 2 remains In Progress/Open with Gates 1 and 2 Partial; the
formal exit gate remains Open.

## Phase 2 current position — 2026-10-13 (F776 accepted; F777 selected)

F776 is accepted at `a9fc10566d3c419a1f456670be3a70748d157d5c`. The Teacher
Profile Edit target built in the existing Visual Studio 18 x64 Debug tree;
its exact CTest registered once (#267) and passed 1/1 after the same bounded
build succeeded outside the sandbox following FileTracker `E_ACCESSDENIED`.
No full suite ran.

F777 reuses the canonical positive-decimal helper for the optional Teacher ID
in Class Details Save while retaining its separate permissive Class ID policy
and the `00042` forwarding contract. The acceptance matrix is in the Phase 2
progress log. F707/F709 remain deferred; Gates 1/2 are Partial and the formal
Phase 2 exit gate remains Open.

## Phase 2 current position — 2026-10-13 (F777 accepted; F778 selected)

F777 is accepted at `dc6d98d7da07e9d08b2f17166d7cb2c8ccbcc3e7`. The
Class Details Save target built in the existing Visual Studio 18 x64 Debug
tree; its exact target-named CTest registered once (#213) and passed 1/1 after
the bounded build succeeded outside the sandbox following FileTracker
`E_ACCESSDENIED`. No full suite ran.

F778 extracts shared canonical positive-decimal parsing with an `int` result
and uses it in Class Transfer Apply, preserving the current malformed-target
precedence. Its acceptance matrix is recorded in the Phase 2 progress log.
F707/F709 remain deferred; Gates 1/2 are Partial and the formal exit gate is
Open.

## Phase 2 current position — 2026-10-13 (F781 accepted; F782 selected)

F781 is committed as `6a067c32b9a70819252d69541f5de5c31fa946b6`. In the
existing Visual Studio 18 Windows x64 Debug tree, `ClassMngr` and the focused
Teacher Profile Edit target built after the bounded retry outside the sandbox
for FileTracker `E_ACCESSDENIED`. Its exact CTest registered once and passed
1/1; no full suite ran. The use case now rejects missing and present
noncanonical Teacher IDs before policy or persistence while preserving the
existing `InvalidTeacherId` result.

F782 will align Speaking Evaluation validation's class-ID grammar with its
save boundary: reject padded positive IDs such as `01` while preserving the
existing structured validation issue. Its pre-implementation matrix is in the
Phase 2 progress log. F707/F709 remain deferred; Gates 1/2 are Partial and the
formal exit gate remains Open.

## Phase 2 current position — 2026-10-14 (F873 accepted; F874 selected)

F870 through F873 are accepted; source/test commits and focused evidence are
recorded in the Phase 2 progress log. F873 validates persisted schedule class
IDs before narrowing; its pre-fix SQLite baseline reproduced a wide ID
aliasing to ID 1, schedule misassociation, and a live duplicate-ID failure.
The executor and independent tester each passed the focused schedule-source
CTest 1/1 after sequential Debug builds. No full suite ran.

The post-F872 review checked all 13 Deferred candidates and found no new
trigger. Batch 288 now selects F874, the public legacy testing-class list ID
read; F875, the conflict-label fallback, is next. Each requires its own
pre-fix baseline. Batch 289 candidate discovery begins with F874. Gates 1 and
2 remain Partial and the formal Phase 2 exit gate remains Open.

## Phase 2 current position — 2026-10-14 (F874 accepted; F875 selected)

F874 is accepted at
`3e74c455fc20f1675651ae772e8266382454b5e7`. Its pre-fix SQLite baseline
reproduced distinct testing-class IDs 1 and 4294967297 both loading as ID 1
while SQLite retained separate INTEGER rows. Persisted IDs are now checked as
positive qint64 values no greater than INT_MAX before narrowing. IDs 1 and
INT_MAX retain their fields and order; zero and wide IDs fail without a partial
public result or database changes. The independent F874 Debug builds passed
sequentially and exact CTest `ClassMngrTestingClassRepositoryTests` passed
1/1. No full suite ran.

The post-F874 audit rechecked all 13 Phase 2 Deferred entries; none became
actionable and all remain unchanged. Their platform, matched-measurement,
caller, validation-seam, or product-policy blockers still apply. F875 is the
next Batch 288 slice. Batch 289 discovery found one separate upcoming public
list candidate, F876 for `TeacherRepository::getAllTeachers()` persisted-ID
narrowing, pending its own SQLite alias baseline after F875. The existing
Sidebar Teacher Create/Delete ID-width deferral is unaffected. Gates 1 and 2
remain Partial and the formal Phase 2 exit gate remains Open.

## Phase 2 current position — 2026-10-14 (F875 accepted; F876 selected)

F875 is accepted at
`bd874b5bb111e607875cd87d5672101b0c69c530`. Its real-SQLite baseline showed
the Class Details conflict port narrowing persisted ID 4294967297 to ID 1 and
returning `Class 1`, aliasing the requested class. The conflict repository now
keeps the ID as qint64 through fallback formatting and returns `Class
4294967297`. The ordinary blank-name fallback `Class 42`, named regular and
intensive conflicts, and persisted wide INTEGER row are covered. Sequential
Debug builds of `ClassMngr` and
`ClassMngrNextPlatformApplicationServicesClassDetailsScheduleConflictPortTests`
passed, followed by exact CTest 1/1; the target was rebuilt and CTest repeated
after a post-call database assertion was added. No full suite ran.

The post-F874 review found none of the 13 Deferred candidates newly
actionable; their blockers remain unchanged. Batch 288 is complete. F876 is
selected in Batch 289, pending its required SQLite baseline for
`TeacherRepository::getAllTeachers()` persisted-ID narrowing. This distinct
public-list candidate does not resolve the Sidebar Teacher Create/Delete
int-based ID deferral. Gates 1 and 2 remain Partial and the formal Phase 2 exit
gate remains Open.

F1023 completed at source commit `5cb82bf04591608a6f9f8db5cff88dbbbcaf1baf`.
The new immutable `Domain::BirthdayMonthDay` expresses valid month/day values
including February 29 in leap year 2000. Save validation and upcoming-birthday
scheduling use the type after retaining their own Qt-compatible Unicode and
ASCII trimming rules. Raw stored, read, and draft birthday strings remain
unchanged. The independent Tester built and linked `ClassMngr` and the GS Team
save target; Domain Contract, Upcoming Birthday, Native English save, and GS
Team save CTests passed 4/4. No full suite ran. Two independent post-commit
audits found all 13 official Deferred entries unchanged.

F1024, typed upcoming-birthday occurrence dates, is accepted and implementation
is pending. Change only the output date on `UpcomingBirthdayOccurrence` to
`Domain::GregorianDate`; keep the raw reference date input so invalid values
still produce an empty schedule. Preserve candidate ASCII trimming, ordering,
week buckets, years 1..9999, and February 29 mapping to February 28 in
non-leap years, including the 9999 boundary. The feature adapter must retain
the same `QDate` and display behavior. Check the chrono-to-domain optional and
assert/fail closed if the valid-generated-date invariant breaks. Focused
verification covers `NextApplicationUpcomingBirthdayScheduleUseCaseOrdering`,
`UpcomingBirthdaysLocaleAwareOrderingTests`, and
`UpcomingBirthdaysLeapDayFallbackBehavior` only.
Batch391 begins with F1024. All 13 official Deferred entries remain unchanged;
Gates 1/2 are Partial and the formal Phase 2 exit gate remains Open.

## Phase 2 current position — 2026-10-15 (F886 accepted; F887 selected)

F876 is accepted at
`cdbe51da607637286c948a44813f361fe64724de`. Its SQLite baseline showed
`DataService::getAllTeachers()` returning distinct teachers stored as IDs 1 and
4294967297 with both IDs narrowed to 1. The list reader now validates persisted
IDs in `1..INT_MAX` before narrowing or appending, while the shared
`teacherFromQuery()` mapper remains unchanged. Tests cover every Teacher field
and order for IDs 1 and INT_MAX; wide and zero IDs fail with full-table SQLite
snapshots unchanged. Independent sequential Debug builds and exact CTest
`ClassMngrDataServiceLifecycleTests` passed 1/1. No full suite ran.

F877 is accepted at `c8178ddf52e94abb02fba1db7392f82aa2fea16f`. Its
helper-level baseline reproduced deletion of an existing destination when a
copy from a missing source failed. The shared save/export copy helper now
stages through `QSaveFile`; the focused DataService lifecycle CTest passed
1/1 after sequential Debug builds. The baseline is not claimed through an
open-session ApplicationServices path, and no NTFS ACL guarantee is claimed.
No full suite ran.

F878 is accepted at `12df019fa58ba87611bc9bef021346b94d6ea2d2`. The
conditional UPSERT guard makes the write authoritative: it preserves a
positive class assignment unless replacement is explicitly allowed, and a
zero-row result returns the existing confirmation error. The focused test
asserts that rejection preserves both room and class ID, and that explicit
replacement succeeds. Independent sequential Debug builds of `ClassMngr` and
`ClassMngrTestingBlockRepositoryTests` passed; exact CTest passed 1/1. Both
build targets required approved elevated retries after Visual Studio
FileTracker `E_ACCESSDENIED`; no full suite ran. The baseline remains
helper-level evidence of the stale SQL sequence, not a full-method or public
ApplicationServices race reproduction.

F879 is accepted at `18202c7908ce36fa85a27872257358c75d92f1b4`. The public
pre-fix RosterReadPort baseline stored SQLite INTEGER `4294967297` and returned
snapshot width `1`. The implementation checks both `RosterRepository` width
read paths, covers RosterReadPort, Class Transfer export, and Template Print,
preserves NULL as zero and all signed-int-representable values, and rejects
only out-of-range SQLite INTEGER values. Non-INTEGER storage keeps the prior
`toInt()` behavior. Tests cover NULL, INT_MIN/INT_MAX, a nonnumeric TEXT width,
and the wide INTEGER through all three public routes. The independent Tester
sequentially built `ClassMngr` and all three test targets; each exact CTest
passed 1/1. The RosterReadPort target was rebuilt and rerun after adding the
TEXT compatibility test. Builds needed elevated retries after Visual Studio
FileTracker `E_ACCESSDENIED`; no full suite ran.

Two independent post-F879 audits reviewed all 13 Deferred entries and found
none newly actionable. Batch 295 application-contract and repository scans
found no new candidate. Batch 296 validation/business-rule review also found
no gap. Its document-navigation feedback lead was excluded because F393
explicitly accepts no warning for a missing resource; the report-job queue
lead has no live application callsite. F880 is accepted at
`b8e56c40106d35e7ce0176f404abf854c5a47d02`: only an explicit nonempty
schedule-overlap result maps to recoverable `Conflict` on the typed
Class Transfer ApplyPort. The exact conflict message and no-write behavior
remain intact, malformed persisted schedules and other repository failures
remain `Technical`, and the legacy QString API keeps its behavior.

The independent Tester sequentially built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesClassTransferApplyPortTests`, and
`ClassMngrClassTransferTests`. The typed ApplyPort CTest passed 1/1. The legacy
full-target CTest failed in
`importDialogFallsBackToIndividualTeacherProfilesAfterBatchFailure()`:
`Keep local: New Teacher` was returned instead of `Keep local: Romanized One`.
The unchanged test reads teacher display names and does not call the modified
import methods. The same full-target CTest was red on the pre-F880 source
commit `18202c79`, while the isolated
`importedClassesConflictAtomically` legacy-conflict test passed. No full suite
ran.

F881 is accepted at `3aa8f54b6e3d1650bdd0a94984dd0fd9664e328b`. An immediate
transaction serializes the roster save with the parent-class existence check;
only the explicit repository-owned missing-parent failure maps to typed
`NotFound`. SQL, trigger, and transaction failures remain Technical with
rollback. Existing closed-session/service-unavailable `NotFound` behavior,
the legacy QString API, and batch behavior are preserved. Tests cover populated
and valid zero-row snapshots after parent deletion, trigger-failure rollback,
and full-snapshot success. The independent Tester sequentially built
`ClassMngr` and `ClassMngrNextPlatformApplicationServicesRosterSavePortTests`;
the exact CTest passed 1/1 after elevated FileTracker retries. No full suite
ran.

F882 is accepted at `334783d2b341125f659e03c94c94de57c713ade4`. The detailed
repository read result tags a missing class only when the class query succeeds
with no row and no SQL error. The existing QString-backed result remains a
wrapper with the same `class not found` diagnostic. The typed co-teacher port
maps only that tag to `NotFound`; invalid input and unavailable-session
behavior remain unchanged, and later schedule-query, validation, conflict,
write, and transaction errors remain Technical. The missing-class test checks
the diagnostic and unchanged persisted fields. A second test forces the
regular-schedule query to fail and verifies Technical with class/teacher state
unchanged. The independent Tester sequentially built `ClassMngr` and
`ClassMngrNextPlatformApplicationServicesClassCoTeacherAssignmentPortTests`;
exact CTest passed 1/1. The initial class-row query failure is source-inspected;
the forced later schedule query covers the Technical read-error path. Builds
required elevated retries after FileTracker `E_ACCESSDENIED`; no full suite
ran.

F883 is accepted at `3c3ba64a9a1a90a426477152fe97231d6c7bbe32`. The typed
class-notes save uses a detailed repository result with explicit
`ParentClassMissing` provenance. The parent check and notes UPSERT run in one
Immediate transaction, and only that result maps to `NotFound`. The legacy
`saveClassNotes()` Status API keeps its original SQLite FK diagnostic.
Regression coverage checks typed `NotFound` with no class/class_info row,
exact legacy diagnostic preservation, successful save, and trigger failure as
Technical with stored notes unchanged. Independent sequential Debug builds of
`ClassMngr` and
`ClassMngrNextPlatformApplicationServicesClassNotesSavePortTests` passed; the
exact matching CTest passed 1/1. No full suite ran.

F884 is accepted at `8db6f256c5019eecab75ae2abdeda1113ef19e5d`. The typed
Speaking Evaluation save uses an explicit repository `ParentClassMissing`
result. Its parent lookup and all persistence run inside an Immediate
transaction; only the explicit missing-parent tag maps to `NotFound`. The
legacy `saveSpeakingEval()` Status/QString API skips the precheck and retains
the original SQLite foreign-key diagnostic. Regression tests verify typed
NotFound with no evaluation header or matrix-data row, exact legacy diagnostic
preservation, and the existing successful save, Technical write-failure
rollback, and closed-session behavior. Independent sequential Debug builds of
`ClassMngr` and
`ClassMngrNextPlatformApplicationServicesSpeakingEvaluationSavePortTests`
passed; the exact Debug CTest passed 1/1. No full suite ran.

F885 is accepted at `c119c6f56099931beae32ec2c3c6c26b98b1be73`. The direct
typed Class Details port now returns explicit repository `ParentClassMissing`
provenance before its own detail/conflict reads. Its detailed save rechecks
the parent and writes details/schedules within one Immediate transaction.
Only the missing-parent tag maps to `NotFound`; legacy `saveClassDetails()`
retains the Status/QString FK diagnostic. Tests cover missing parents with
empty and nonempty schedules and no stored rows, exact legacy diagnostic,
successful saves, trigger rollback, and unavailable-session behavior.
Independent sequential Debug builds of `ClassMngr` and
`ClassMngrNextPlatformApplicationServicesClassDetailsSavePortTests` passed;
the exact Debug CTest passed 1/1. No full suite ran.

Two independent post-F885 audits reviewed all 13 official Deferred entries;
none became actionable and all 13 remain unchanged. Batch 299 is complete.
Batch 300 selected F886 for deleted-parent classification during direct typed
Testing Class details update. The temporary public baseline created a real
Testing Class, deleted it through `ClassService::remove()`, then called the
direct update use case. The result was `Technical` (ErrorCode value 5) with
exact message `The testing class no longer exists.`; `classes`, `class_info`,
and `testing_classes` rows remained absent. The existing ordinary-class case
also remains Technical and passed. The temporary test was removed; the target
and exact CTest passed 1/1, and the target rebuilt after cleanup. The initial
target build reported a nonfatal missing dependency warning for
`teacher_import_plan_mapper.h`, which did not recur during cleanup rebuild.

F886 scope is the direct `TestingClassDetailsUpdateUseCase` to
`ApplicationServicesTestingClassDetailsUpdatePort` path. Add explicit
repository provenance only when a successful query confirms the parent
`classes` row is absent; check and update under one Immediate transaction.
Map only that tag to typed `NotFound`. Preserve the legacy
`updateTestingClass()` diagnostic, existing ordinary-class `Technical`
behavior, InvalidInput/unavailable-session behavior, and Technical rollback
for query/write/commit failures. Cover a genuinely deleted class with no
mutation, exact legacy diagnostic, ordinary class, successful update, and a
trigger failure after an earlier write that proves rollback. Sequentially
build `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesTestingClassDetailsUpdatePortTests`,
and `ClassMngrTestingClassRepositoryTests` in `build/windows-x64-debug`; run
only the two matching Debug CTests. No full suite. Gates 1/2 remain Partial;
the formal Phase 2 exit gate is Open.

F886 is accepted at source/test commit
`e4f2d089e1001dea6c980f46962938586b8a30b8`. The detailed update route tags
only an absent parent `classes` row inside its Immediate transaction; existing
ordinary classes without a Testing Class row remain Technical. The legacy
diagnostic is preserved, and trigger coverage verifies rollback of earlier
field writes. Independent Debug builds of `ClassMngr` and the two planned
focused targets passed; the exact two CTests passed 2/2. An initial un-elevated
build hit FileTracker access denied at `ZERO_CHECK`; the approved retry passed.
No full suite ran.

Two independent post-F886 audits found none of the 13 official Deferred
entries newly actionable. Batch 300 is complete. Batch 301 selects F887 for
deleted-parent classification on the typed Testing Class details-read route.
The pre-fix public baseline returned Technical with exact message
`The testing class was not found.` after a real Testing Class parent was
deleted; all three related row counts were zero. The existing parent-without-
Testing-Class regression remains the classification boundary. Focused target
cleanup verification passed 1/1 after elevated FileTracker retries, and the
temporary test slot was removed before F887 implementation. Gates 1/2 remain
Partial and the formal exit gate is Open.

## Phase 2 current position - 2026-10-16 (F947 complete; F948 selected)

F947 is complete at acceptance documentation commit `c2d41335` and source/test
commit `ec6eae8f`. The typed Co-Teacher page port classifies malformed
persisted teacher IDs as nonrecoverable Validation while retaining the early
caller ClassId InvalidInput check. Its regression verifies the exact repository
diagnostic and the independent teacher-display NotFound result. Independent
sequential Debug builds of `ClassMngr` and
`ClassMngrNextPlatformApplicationServicesClassCoTeacherPageReadPortTests`
passed; exact CTest #334 passed 1/1. No full suite ran.

Post-F947 review found no newly actionable, resolved, superseded, or changed
item among the 13 official Deferred candidates. Batch313 discovery found no
distinct candidate. Batch314 selected F948, a local parity fix for malformed
persisted `class_info.teacher_id` in the single and batch Selected Class
Subtitle reads, which currently classify the same repository failure as
InvalidInput and Technical. Acceptance and focused verification are recorded
in the Phase 2 progress log. Class Notes and Testing Class remain outside this
local slice because their routes retain separate accepted behavior; no
project-wide malformed-data classification rule is claimed. Gates 1/2 remain
Partial and the formal Phase 2 exit gate remains Open.

F948 completed at source/test commit `236cea99`, following acceptance docs
commit `271c35d0`. Both Selected Class Subtitle read paths now return
nonrecoverable Validation for malformed persisted teacher IDs, retain their
exact repository diagnostics, and keep the outer snapshots and independent
teacher components successful. The batch regression also checks requested ID
order and confirms teacher lookup is not attempted after the repository
failure. Sequential Debug builds of `ClassMngr` and both subtitle read-port
test targets passed with `/p:TrackFileAccess=false`; exact CTests #329 and
#330 passed 1/1 each. No full suite ran.

Two independent Batch315 scans found no distinct next candidate and confirmed
the exact 13 official Deferred entries remain unchanged. The Phase 2 progress
log records the inspected surfaces and rejected duplicate leads. Gates 1/2
remain Partial and the formal exit gate remains Open.

Batch316 source discovery is underway in separate Qt-free domain/application
contracts and feature port/repository boundaries.

Batch316 found no distinct candidate. The nearest domain lead was an invalid
CalendarEventTimeStatus cast that has no production construction path and was
already rejected in history; the other inspected read ports are covered by
explicit parent provenance or prior fixes. Batch317 source discovery is now
underway in persisted identity conversions and typed write contracts.

## Phase 2 current position — 2026-10-16 (F949 complete; Batch318 discovery)

F949 was accepted at documentation commit `af31c6da` and completed at
source/test commit `28cd2d38`. Calendar Event Save now validates a present
legacy update ID after date validation and before checking session
availability. The request contract remains adapter-neutral and continues to
allow opaque bounded IDs. Invalid ID `0` returns InvalidInput with open,
unopened, and closed sessions; valid update and create requests still return
NotFound when the session is unavailable. Repository and transaction behavior
remain unchanged.

The focused Debug target built with `/p:TrackFileAccess=false`; the independent
Tester rebuilt it and passed CTest #356 (1/1). `git diff --check` passed. The
source preserves date-before-ID validation order, though no regression combines
an invalid date and invalid ID. No full suite ran. Two independent post-F949
audits reviewed all 13 official Deferred candidates; none became actionable
and their listed blockers remain. Batch318 source discovery is underway. Gates
1/2 remain Partial and the formal Phase 2 exit gate remains Open.

### F954 complete; F955 accepted as active Batch323 slice - 2026-10-16

F954 is complete at source/test commit `140bb639`, following acceptance docs
commit `f9bfec14`. The Qt-free single `SpeakingEvaluationQuery` now rejects
Qt-whitespace-only names as recoverable `InvalidInput` with its existing
diagnostic before the read port. Empty-name and invalid-ClassId recoverability,
validation order, exact forwarding of padded nonblank names, and the separate
batch-query fallback remain unchanged.

The independent Tester sequentially built
`ClassMngrNextApplicationSpeakingEvaluationQueryTests` and
`ClassMngrNextPlatformApplicationServicesSpeakingEvaluationReadPortTests` in
the standard Debug folder with `/p:TrackFileAccess=false`; exact CTests #232
and #324 passed 1/1 each. `git diff --check` passed. The Executor saw an
MSBuild warning for the missing generated dependency
`teacher_import_plan_mapper.h`, while the targets linked and the final
independent builds exited successfully. No full suite ran.

Two independent post-F954 audits reviewed all 13 official Deferred entries;
all remain unchanged and none is actionable, resolved, or superseded. The
Dense Speaking Evaluation row-limit item remains a separate unresolved product
policy. Batch323 read-only scans found two leads. F955 is selected for the
Campus Dashboard selected-campus read: replace the query's byte-wise blank
check with shared `Application::isBlankUtf8Text()` so Unicode whitespace-only
IDs are rejected as the existing nonrecoverable `InvalidInput` before the
port. Cover U+00A0 and U+3000 in the app-less test. Preserve exact forwarding
for nonblank IDs and do not change the adapter, repository, UI, or global
campus-ID policy. The Class Details save adapter lead was not selected because
normal callers validate `TeacherId` in the use case and its discrepancy is
limited to direct adapter error precedence. Gates 1/2 remain Partial and the
formal Phase 2 exit gate remains Open.

### F955 complete; F956 accepted as active Batch324 slice - 2026-10-16

F955 is complete at source/test commit `0df727c2`, following acceptance-docs
commit `bc020f3f`. The Qt-free selected-campus query now uses the shared
`isBlankUtf8Text()` helper before constructing `CampusId`; Qt-compatible
Unicode whitespace-only IDs fail with the existing nonrecoverable
`InvalidInput` before the port call. Nonblank IDs are still forwarded exactly,
and adapter, repository, UI, and global campus-ID policy are unchanged.

The independent Tester sequentially rebuilt
`ClassMngrNextApplicationCampusDashboardSelectedCampusReadQueryTests` in the
standard Debug folder with `/p:TrackFileAccess=false`; exact CTest #160 passed
1/1. `git diff --check` passed, with Git only reporting LF-to-CRLF
normalization warnings. No full suite ran. Two independent post-F955 audits
reviewed all 13 official Deferred entries; none became actionable, resolved,
or superseded. Batch324's application-contract scan selected F956:
`ScheduleViewProjection` currently applies byte-wise blank checks to display
text, while a separate platform-read scan found no actionable candidate. The
independent review found no production `create`/`validate` callsite, so F956 is
accepted as API-level application-contract hardening without a current UI
defect claim. Use shared `isBlankUtf8Text()` only for the four required and two
nonempty optional display-text fields; preserve identifiers, empty optional
text, exact nonblank values, byte bounds, and malformed-UTF-8 behavior. Test
U+00A0 and U+3000 across all six fields. Gates 1/2 remain Partial and the
formal Phase 2 exit gate remains Open.

### F956 complete; Batch325 discovery underway - 2026-10-16

F956 is complete at source/test commit `2d3bda62`, following acceptance docs
commit `b9d3516c` and active-row correction `009dc213`. Required and nonempty
optional Schedule View display text now uses shared
`Application::isBlankUtf8Text()`; local byte-wise blank handling for typed
identifiers remains unchanged. The contract still allows empty optional text,
preserves byte limits, exact nonblank values, malformed-UTF-8 behavior, and
existing diagnostics. No current production `create`/`validate` callsite was
found, so this is API-level hardening, not a current UI defect claim.

The independent Tester rebuilt `ClassMngrNextApplicationScheduleViewTests` in
the standard Debug folder with `/p:TrackFileAccess=false`; the exact CTest
passed 1/1. `git diff --check` passed with Git line-ending normalization
notices only. No full suite ran. Two independent post-F956 audits reviewed all
13 official Deferred entries; none became actionable, resolved, or
superseded. Batch325's read-only scans later found no actionable candidate.
Gates 1/2 remain Partial and the formal Phase 2 exit gate remains Open.

## Phase 2 current position — 2026-10-16 (F949 complete; Batch319 discovery)

F949 remains complete at acceptance commit `af31c6da` and implementation/test
commit `28cd2d38`; the post-slice closeout and Deferred audit are recorded in
`d9980e06`. Its focused Debug target and CTest #356 passed, and the exact 13
official Deferred entries remain unchanged with no item newly actionable.

Batch318 ended without a selected slice. The use-case scan found no distinct
contract gap. The Class Details Page malformed teacher-ID lead was rejected
because F889's accepted contract preserves Technical for malformed
class-detail data; F947/F948 remain route-specific. Batch319 discovery is
underway across Qt-free domain invariants and non-class feature projections.
Gates 1/2 remain Partial and the formal Phase 2 exit gate remains Open.

## Phase 2 current position — 2026-10-16 (F949 complete; Batch320 discovery)

Batch319 closed without a selected slice. The domain scan found no invariant
gap across the inspected value objects; the projection scan found no uncovered
ordering, bounds, or optional-value mismatch in its inspected features. No
source changed, so the post-F949 audit remains the current review of all 13
official Deferred entries. Batch320 discovery is active in additional non-class,
non-calendar Next read/write adapters. Gates 1/2 remain Partial and the formal
Phase 2 exit gate remains Open.

## Phase 2 current position - 2026-10-16 (F951 complete; Batch321 discovery)

F951 completed at source/test commit `1294d32d`, following acceptance-docs
commit `8dc6f3b5`. Campus Dashboard saves now carry explicit Create/Update
intent. A Create whose normalized JSON path is already occupied returns
Conflict without replacing the existing file; updates continue under the
retained ID, including display-name changes. A failed Create keeps the draft
selected and dirty.

The executor sequentially built the three focused Debug targets for the save
port, repository adapter, and Campus Dashboard page. CTests #161, #367, and
#152 passed 1/1; after adding direct Create-path I/O-failure coverage, the
repository-adapter target and CTest #367 passed again. `git diff --check`
passed, and no full suite ran. Two independent post-F951 audits found all 13
official Deferred entries unchanged and none actionable. Batch321 discovery is
underway across typed application ports, repository/feature projections, and
domain values. Gates 1/2 remain Partial and the formal Phase 2 exit gate
remains Open.

### F953 complete; F954 active - 2026-10-16

F952 is complete at source/test commit `55d510fe`. Calendar repeat-series
Create and Edit request validation now uses the existing UTF-8-aware blank
helper, so Qt-compatible Unicode-whitespace-only IDs fail as InvalidInput
before the use-case port or platform repository path. Byte limits, diagnostics,
validation order, padded-ID trimming, and malformed-UTF-8 behavior remain
unchanged.

The independent Tester sequentially built
`ClassMngrNextApplicationCalendarEventTests` and
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests` in the
standard Windows x64 Debug folder with `/p:TrackFileAccess=false`; exact CTests
with those names passed 1/1 each. `git diff --check` passed. No full suite ran.
Two independent post-F952 audits found all 13 official Deferred entries
unchanged, including F709's unavailable Calendar projection seam.

F953 is complete at source/test commit `5ce0e0f9`, after acceptance docs
commit `9d6cf5c8`. Both application policies now reject present nonpositive
row IDs, duplicate positive row IDs, and nonpositive deleted IDs before the
port call. Missing IDs remain valid inserts, duplicate positive deletions
remain allowed, and the existing `saved=false`/validation result is retained.
The legacy adapter sentinel and repository/UI behavior are unchanged.

The independent Tester sequentially rebuilt
`ClassMngrNextApplicationGsTeamDirectorySaveTests` and
`ClassMngrNextApplicationNativeEnglishTeacherDirectorySaveTests` in the
standard Debug folder with `/p:TrackFileAccess=false`. Exact CTests #278 and
#277 passed 1/1 each; `git diff --check` passed. The Executor observed an
MSB8064 stale dependency warning for absent `teacher_import_plan_mapper.h`,
but compilation and linking succeeded; the final independent rebuilds also
passed. No full suite ran.

Two independent post-F953 audits reviewed all 13 official Deferred entries;
all remain unchanged and none became actionable, resolved, or superseded.
Batch321 is closed. Batch322 selected F954, a separate candidate rather than
one of the official Deferred entries. F954 is now active: reject blank-only
names on the single Speaking Evaluation query while preserving recoverable
InvalidInput semantics, exact padded-name lookup, and batch fallback. Gates
1/2 remain Partial and the formal Phase 2 exit gate remains Open.

### F958 complete; Batch329 closed; F959 active - 2026-10-16

F957 is complete at source/test commit `a14de63f`, following acceptance docs
commit `683bdacc`; its focused Debug CTest #170 passed 1/1, and two audits
found all 13 official Deferred entries unchanged. Batch326 closed; Batch327
found no defensible candidate. Batch328 had no typed read/result or non-class
command candidate; its class/teacher/schedule scan selected F958 after finding
a reachable stale schedule-conflict precheck across separate writable sessions.

F958 completed at source/test commit `305a0553`, following acceptance docs
commit `8f935055`. In the typed Class Details save, regular and intensive
conflicts are rechecked inside `BEGIN IMMEDIATE` after the parent check and
before the first write. Conflict/query errors stay Technical with the existing
`Class schedule conflict: ...` format; port preflight and legacy save paths
are unchanged. The independent Tester sequentially rebuilt `ClassMngr` and
`ClassMngrNextPlatformApplicationServicesClassDetailsSavePortTests` in standard
Debug with `/p:TrackFileAccess=false`; exact CTest #315 passed 1/1, and
`git diff --check` passed. The initial fixture had invalid grade/book values;
after retaining valid grade, level, and book values, the final run passed. No
full suite ran.

Two fresh independent post-commit audits reviewed all 13 official Deferred
entries. All remain unchanged; none is actionable, resolved, or superseded by
F958. Batch328 is closed.

Batch329 closed after scans of Next teacher-profile/import contracts, Testing
Class lifecycle, and Schedule Testing use cases. The Schedule Testing scan
found no uncovered contract. The Testing Class deletion race lead was rejected:
the ID-scoped transactional cascade completes the requested final state if
another session already deleted the class, no stable concurrent-delete result
is specified, and deterministic synchronization would require a new seam.
The teacher-import scan selected F959 as Qt-free Application-boundary
hardening. Plan validation checks adapter-supplied Korean validation keys,
while matching derives identity from `teacherKr`; the production adapter
currently supplies the same Hangul-only key to both paths. F959 checks the
derived domain identity for missing or duplicate Korean teacher names before
persistence, preserving shape/review/date precedence and the current adapter
mapping. Its focused regression uses two distinct mirrored validation keys
for sources with the same `KoreanTeacherKey` and requires
`DuplicateKoreanTeacherName` before any persistence call. This strengthens
direct use-case callers; the current UI adapter already produces canonical
keys.

F959 is the sole active slice. No source changes have been made for it yet.
Gates 1/2 remain Partial; the formal Phase 2 exit gate remains Open.

## Current Position - 2026-10-16 (F1014 complete; F1015 accepted)

F1011, academic-year winter continuity, is complete at source/test commit
`7acb5c5e5da5a88f29510a6315bc722e16d765d9`. Its Qt-free policy accepts the
full 64-bit resolved day span, checks the 53-week limit before narrowing, and
is used by the calendar schedule and Preferences paths. Both profile spans
are validated before schedule mutation. Four exact focused selectors passed;
ordinary MSBuild attempts hit FileTracker denial and passed with
`/p:TrackFileAccess=false`. The Preferences target had a nonfatal MSB8064
warning for missing generated dependency
`src/next/application/teacher_import_plan_mapper.h`. No full suite ran.

F1012, Sub Prep selected-day summary grouping, is complete at source/test
commit `e57a3c816ca3b88fcd31a6401ba3f9dd0649b5ae`. The Qt-free Application
policy groups resolved canonical weekdays by exact seconds-of-day; the
platform adapter retains raw parsing, translated labels, compact formatting,
legacy feature behavior, and the distinct navigation path. The independent
Tester validated ownership for 1,508 handwritten files, built `ClassMngr`,
the app-less policy target, and the platform target serially, passed the
app-less CTest 1/1 after a contradictory fixture was corrected, and passed
the two named adapter slots. Normal MSBuild hit FileTracker denial and
elevated retries with `/p:TrackFileAccess=false` passed. Configure reported an
optional missing `Vulkan_INCLUDE_DIR`; generation succeeded. No full suite ran.

F1013, Sub Prep regular-before-intensive navigation metadata, is complete at
source commit `fa63e499ab87bdb15763369d46b650ed6e972d44`. The Qt-free
Application policy selects one complete metadata value; the Platform adapter
retains schedule reads, conditional Intensive reads, parsing, formatting,
selected-day `meetingText`, and selected-mode `navigationTeacherLabel`.
Serial Debug builds passed for `ClassMngr`, the app-less policy target, and
the platform target. The exact app-less CTest passed 1/1; the named platform
precedence and existing Intensive fallback slots each passed 3/3. The
independent Tester retried builds with `/p:TrackFileAccess=false` after
FileTracker denied ordinary access. Configure completed with an optional
missing Vulkan include warning. `git diff --check` passed; no full suite ran.

F1014, Sub Prep class-information selection fallback, is complete at source
commit `d9ea2c9ac16c306eb86676b478aacd94b1dc3f05`. Its Qt-free policy retains
the selected class when it remains among caller-supplied visible IDs;
otherwise it chooses the first ID in current model order or no class for an
empty list. The page continues to own filtering, sorting, grade choice, and
labels. Independent serial Debug builds passed for `ClassMngr`, the policy
target, the existing state target, and `ClassMngrSubPrepPageTests`. The exact
app-less CTest passed 1/1, and five named page slots each passed 3/3. FileTracker
denied ordinary access and `/p:TrackFileAccess=false` retries passed. Warnings
included optional missing Vulkan headers, an MSB8064 generated-dependency
notice, and duplicate-definition/LNK4088 notices on the page-test link. No
full suite ran.

F1015, Sub Prep navigation schedule-order reduction, is complete at source
commit `10d46c5fa15ac3589a42595415a767b8bb29710`. The Qt-free Application
policy reduces resolved canonical weekday/minute facts to the earliest day
and earliest valid minute on that day, using sentinel 1000 when unknown. The
Platform adapter retains exact weekday recognition, raw time parsing,
seconds-to-minutes truncation, labels, formatting, and selected-day
`meetingText`. The independent Tester serially built `ClassMngr`, the new
app-less policy target, and the platform print-source port target. The exact
app-less CTest passed 1/1 after a final numeric-guard assertion was added and
the target rebuilt. Three named platform slots each passed 3/3. FileTracker
denied ordinary access; escalated serial retries with
`/p:TrackFileAccess=false` passed. Configure reported an optional missing
Vulkan include. `git diff --check` passed with LF-to-CRLF notices. No full
suite ran.

Two independent post-F1015 audits reviewed all 13 official Deferred entries;
none became actionable, resolved, or superseded. F626 now has evidence for
Sub Prep feature-resource release and refresh on a retained page, but still
lacks matched owner-specific packaged Release retained-byte attribution and
a safe PageManager eviction/recreation contract. Keep it deferred and record
that distinction in the plan.

Batch389 discovery used two Explorers and two Investigators. F1016, Initial
Setup teacher identity completeness, is complete at source commit
`40bb34603a57471e891b64052005dc73966e41f8`. The Qt-free Application policy
accepts trimmed presence facts and returns the two-of-three completeness,
additional-field count, and missing-field membership. The wizard retains
trimming, translated prompt construction, fixed missing-field order, and the
gate before setup/database preflight and `TeacherCreateUseCase`. The app-less
test passed 1/1 across all eight presence combinations; the exact Initial
Setup parity test passed 1/1. Serial Debug builds passed for `ClassMngr` and
both test targets after FileTracker required an elevated
`/p:TrackFileAccess=false` retry. Configure reported optional missing Vulkan
headers, and the build reported MSB8064 for an existing generated dependency
on `teacher_import_plan_mapper.h`. No full suite ran.

F1017, Sub Prep visible-row ordering, is complete at source commit
`bd94f54422fa7053a94bf5799471aef6e6f203db`. Application now orders prepared
facts by feature-computed level rank, projected navigation day/minute,
feature-built base label, and class ID through an injected Qt locale
comparator. Numeric unknown sentinels remain literal (1000 sorts before 1020).
The model keeps rank calculation, label construction, locale comparison, and
filtering; the page still retains a visible selection or selects/loads the
first sorted row after it disappears. Serial Debug builds passed for
`ClassMngr`, the policy target, and the model/page targets. The exact three
focused CTest selectors passed 3/3. FileTracker required elevated workspace
access after `/p:TrackFileAccess=false` retries. Configure reported optional
missing Vulkan headers; build output included MSB8064 for an existing
generated dependency and duplicate-symbol/`/FORCE` notices on the page-test
link. No full suite ran.

F1018, Teacher Info preferred-name load/edit precedence, is complete at source
commit `a6a69b4fcc420acab7160da1cf9c28112c457513`. The Qt-free policy selects
an exact case-sensitive choice index: load prefers stored then first; edit
prefers current, then stored, then first; empty choices return no index.
Choice construction, trimming, deduplication/order, combo state, and prompt
dialog behavior remain at their previous boundaries. Serial Debug builds
passed for `ClassMngr`, the policy target, and `ClassMngrTeacherInfoPageTests`;
the two exact focused CTest selectors passed 2/2. FileTracker required an
elevated retry; configure reported optional missing Vulkan headers, and build
output reported an existing generated dependency and forced duplicate-link
notices. No full suite ran.

Two independent post-F1018 audits reviewed all 13 official Deferred entries;
none became actionable, resolved, superseded, or needed rationale changes.
F1020, Testing Classes list ordering, is complete at source commit
`6aa20a7ecfbcbd87b8e9d408a93b5b03a1b38848`. Its app-less CTest passed 1/1 and
the named page QtTest passed 3/3. F1019, shared Class Tab navigation ordering,
is complete at source commit `85849c85574b1478c15ab8b3b8c9e281e93401a`. The
Qt-free policy compares prepared facts; the shared model retains schedule
parsing, ordering-source selection, labels, locale comparison, and page
selection behavior. All 13 focused model methods passed (QtTest 15/15 including
setup/cleanup), policy CTest passed 1/1, and two page selectors passed (QtTest
4/4 including setup/cleanup). Two independent post-commit audits found all 13
official Deferred entries unchanged. F1021, Class Analytics average and grade
rounding, is complete across source commit
`4d13f0e04903d751168936376d65d0258c44d937` and range-contract comment commit
`5a86ee39d7b8b8b7f3e35b3c7968e3fff989a7a4`. Its shared Qt-free policy rounds
finite [0,5] averages to three decimals and derives grade from integer
thousandths, fixing the exact 2.400 >=0.4 threshold. The focused analytics and
page parity CTest registrations passed 3/3; Ninja reported targets up to date,
so the retry did not recompile. Two independent audits found all 13 official
Deferred entries unchanged, including the Class Analytics allocation-cap and
Dense Speaking Evaluation row-limit items. F1022, Sub Prep print grouping and
order, is accepted next: order prepared teacher/class facts through a Qt-free
index policy while the feature retains filters, parsing, labels, meeting
formatting, and PDF rendering. Gates 1/2 remain Partial and the formal Phase 2
exit gate remains Open.

F1022 completed at source commit `fd793132da5860714be6801c956ea167f168c20d`.
The policy orders prepared Sub Prep teacher/class facts while existing feature
filters, labels, schedule parsing, meeting formatting, and PDF rendering stay
at their boundaries. The independent Tester built the affected model and
mapper targets; the five focused policy, model, mapper, PDF, and source-query
CTest registrations passed 5/5. No full suite ran. Two independent post-commit
audits reviewed the 13 official Deferred entries; none became actionable,
resolved, or superseded. F614 remains deferred because F1022's temporary
ordering buffers do not supply owner-specific memory measurements; F707 remains
deferred because the Sub Prep locale comparator does not establish a live
Sidebar helper callsite. Keep all 13 entries and rationales unchanged.

F1023, shared `BirthdayMonthDay` domain value, is accepted and implementation
is pending. Add an immutable Qt-free month/day value valid against leap year
2000 and use it in both save validation and upcoming-birthday scheduling after
each Application parser applies its existing input rules. Save validation
retains Qt-compatible Unicode trimming and exact ASCII `MM-dd`; the schedule
retains ASCII-only trimming, invalid-date omission, and February 29 mapping to
February 28 in non-leap years. Keep stored, draft, and read birthday strings
unchanged; do not migrate `TeacherProfileFields` or persistence. Focused
verification covers the domain contract, schedule use case, and Native English
and GS Team save policies. Batch390 begins with F1023. All 13 official Deferred
entries remain unchanged; Gates 1/2 are Partial and the formal Phase 2 exit
gate remains Open.

## Current continuation - 2026-10-17 (F1028 complete; Batch402 discovering)

F1028 completed at source commit `40cfa3f7`; its acceptance and documentation
are committed at `851574d9`. The Campus Dashboard rejects reserved `default`
IDs or `Default` display names before persistence. The independent Tester
reviewed the committed behavior and reported all three focused Debug CTests
passing. The targets linked incrementally; the relevant C++ objects were
already current, and no full suite ran.

Two independent post-commit audits found all 13 official Deferred candidates
unchanged. Batch401 found no eligible slice in direct legacy facade/repository
UI paths or the remaining Application import use cases. Batch402 discovery is
underway. Gates 1/2 remain Partial, F1007 remains pending policy
reconciliation, and the Phase 2 exit gate remains Open.

## Current continuation - 2026-10-17 (F1033 accepted; Batch410 active)

Batch402–404 found no eligible source slice. Batch405 found an inconsistent
`FeatureService` contract: some operations fall back to a separate legacy
`DataService` even when a bound session makes `isAvailable()` false. Two
independent reviews confirmed the public API path and supported accepting one
session-authority policy without claiming a current UI misrouting bug.

F1029 completed at source commit `c49c0739`. A non-null bound session is
authoritative across descendant operations; unavailable repositories fail
without legacy fallback, while DataService-only construction remains intact.
The focused lifecycle CTest passed 1/1 in the standard Debug tree; no full
suite ran. Two independent audits found all 13 official Deferred entries
unchanged.

F1030 completed at source commit `e2c5e65e`. `DocumentContentSession` now uses
the shared Qt-compatible UTF-8 blankness helper. Its focused content and
catalog CTests passed 2/2; coverage includes NBSP/U+3000 rejection with no
snapshot mutation, malformed-byte preservation, and the exact length limit.
No full suite ran. Two independent audits again found all 13 official
Deferred entries unchanged. Batch407 found no production slice: the stateful
Application transitions and read projections preserve their contracts with
existing focused coverage. No files or tests changed.

F1031 completed at source commit `d5bcff97`. Testing Classes details update
now marks unavailable-session `NotFound` recoverable while preserving the
typed deleted-parent result as nonrecoverable with its detailed-repository
diagnostic. The independent Debug build and focused CTest passed 1/1 after an
elevated Visual Studio FileTracker retry; no full suite ran. Two independent
post-commit audits checked the exact 13 official Deferred entries; all remain
unchanged and unresolved.

Batch409 identified F1032: Class Details Save, Class Notes Save, and
Co-Teacher Assignment classified unavailable-session `NotFound` as
nonrecoverable while sibling class mutation ports marked it recoverable. This
public result-metadata issue did not establish a current UI defect. F1032 is
complete at source commit `74229928`; only those three unavailable-session
results changed, and missing-entity behavior and the Details Validation
Context fallback remain intact. The independent Debug build and three focused
CTests passed 3/3 after an elevated Visual Studio FileTracker retry; no full
suite ran. Two independent post-commit audits found all 13 official Deferred
entries unchanged and unresolved. Batch410's Schedule Testing Assignment ID
scan confirmed F780 intentionally accepts leading-zero IDs and the live UI
supplies canonical IDs; no additional platform candidate was found. The
multi-step Application scan found a retained-file disclosure gap in Initial
Setup cancellation. If restoration and moving the incomplete profile back
both fail, the lifecycle retains both files but the controller warning omits
the incomplete profile's UUID-named recovery path. F1033 was accepted and its
acceptance record was committed at `c48b380b`. The source slice is complete at
`725a654f`: `incompleteRecovery` is empty when the incomplete profile is not
retained at its recovery path, and the restore-failure warning includes both
labeled paths only when both files remain. The successful-restore
cleanup-failure `Canceled` path is unchanged.

The Executor built `ClassMngr`,
`ClassMngrNextApplicationInitialSetupLifecycleTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests`; the matching two focused
CTests passed 2/2. The independent Tester repeated the build and focused tests
successfully. Ordinary and `/p:TrackFileAccess=false` builds hit Visual Studio
FileTracker `E_ACCESSDENIED`; the elevated build retry passed. No full suite
ran.

Two independent post-commit audits reviewed the exact 13 official Deferred
entries. All remain unchanged and unresolved; none became actionable, resolved,
or superseded. F579/F598 still require a WPR capture under an enabled
Administrator token; this session has only a Medium token. F629 still lacks a
suitable runner exposed in this Windows x64 session, without establishing
global runner unavailability. The remaining policy, measurement, callsite, and
projection-injection triggers remain unmet.

Batch411 discovery found F1034: the Speaking Evaluation save adapter returns
`NotFound` with `recoverable=false` when its database session is unavailable,
while sibling read adapters report that condition as recoverable. F1034 is
accepted as a public result-metadata correction, without claiming a current UI
defect. Mark only the null or closed session result `NotFound,
recoverable=true`; preserve invalid-input, technical, and missing-parent
outcomes. Add direct null-service and closed-session assertions, including the
error code, message, and recoverability, and retain existing technical failure
coverage. This is separate from the deferred Dense Speaking Evaluation
row-limit policy. Gates 1/2 remain Partial, F1007 remains pending policy
reconciliation, and the Phase 2 exit gate remains Open.

F1034 is complete at source commit `86200e26`. The Speaking Evaluation save
adapter now marks only a null or closed session `NotFound, recoverable=true`;
the existing unavailable message and other error mappings remain intact. Tests
assert the null and closed session results through the save use case, the exact
typed missing-parent diagnostic, and noncanonical-ID `InvalidInput` with
`recoverable=false`. `ClassMngr` and the focused adapter target built, and its
CTest passed 1/1 after an elevated Visual Studio FileTracker retry. No full
suite ran.

Two independent post-F1034 audits checked the exact 13 official Deferred
entries. All remain unchanged and unresolved; none became actionable, resolved,
or superseded. WPR capture still requires an enabled Administrator token, and
the macOS runner is not exposed in this Windows x64 session. The listed
product-policy, measurement, callsite, and injection triggers remain unmet.

Batch412 found F1035: the Campus Dashboard adapter parses its directions JSON
strings and housing JSON string without checking parse errors or root types,
then can persist empty objects or arrays on Update. F1035 is accepted as a
public save-contract data-loss guard, not as a current UI defect. Validate both
directions fields as JSON objects and housing as a JSON array before Create or
Update writes; return nonrecoverable `InvalidInput` with field-specific
diagnostics without mutating files. Preserve valid `{}` and `[]`, existing
recognized nested direction forms, housing nested/custom fields, and current
direction normalization behavior. Add tests for malformed and wrong-root
fields, unchanged Update bytes, no Create file, and valid empty roots. Gates
1/2 remain Partial, F1007 remains pending policy reconciliation, and the Phase
2 exit gate remains Open.

F1035 is complete at source commit `a501998f`. The adapter validates both
directions strings as JSON objects and housing as a JSON array after reserved-
identity validation and before writes. Malformed or wrong-root data returns
field-specific nonrecoverable `InvalidInput`; invalid Create leaves no file,
and invalid Update leaves its existing bytes unchanged. Tests preserve empty
roots against raw-repository normalization baselines, verify reserved-
identity precedence, and retain nested/custom round-trip coverage. The
independent Tester built `ClassMngr` and
`ClassMngrNextPlatformCampusDashboardCampusRepositoryAdapterTests`; focused
CTest passed 1/1 after an elevated Visual Studio FileTracker retry. No full
suite ran.

Two independent post-F1035 audits reviewed exactly the 13 official Deferred
entries. All remain unchanged and unresolved; none became actionable,
resolved, or superseded. WPR still requires an enabled Administrator token,
and the macOS runner is not exposed in this Windows x64 session. The listed
policy, measurement, callsite, and injection triggers remain unmet.

Batch413 found F1036: Calendar single-event Save reports an unavailable
database session as `NotFound, recoverable=false`, although the session can
be restored and the operation retried. Two independent reviews confirmed the
adapter/use-case path and recommended changing only unavailable-session save
results to `NotFound, recoverable=true`, preserving the message and all other
failure mappings. Test those outcomes through the use case; retain non-
recoverability for invalid input, missing events, and technical failures.
Other Calendar write adapters and dialog draft retention remain outside this
slice. Gates 1/2 remain Partial, F1007 remains pending policy reconciliation,
and the Phase 2 exit gate remains Open.

F1036 is complete at source commit `f3124488`. The single-event Calendar Save
adapter now reports unavailable or closed-session failures as
`NotFound, recoverable=true` with the existing message, both before the save
and when a failed repository write detects a closed session. The generic
failure path remains nonrecoverable for invalid input, missing events, and
technical failures. The independent Tester built `ClassMngr` and
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`; its focused
CTest passed 1/1 after an elevated Visual Studio FileTracker retry. No full
suite ran. The test covers unavailable sessions before the save call; it does
not deterministically close a session during a failed repository write, but
source review confirms that branch uses the same dedicated result helper.

Two independent post-F1036 audits reviewed exactly the 13 official Deferred
entries. All remain unchanged and unresolved; none became actionable,
resolved, or superseded. F1036 changes only the single-event Calendar Save
result contract and does not resolve the Calendar projection injection item.
The dialog still dismisses after a failed save and does not retain its draft.
Gates 1/2 remain Partial, F1007 remains pending policy reconciliation, and
the Phase 2 exit gate remains Open.

Batch414 scans found no supported candidate in the inspected class/roster,
workspace/setup, class-notes, directory, and preference boundaries. Batch415
reviews found that the remaining Calendar mutation adapters still report
unavailable-session failures as nonrecoverable, matching their prior tests;
the F1031/F1032 acceptance history supports the same recoverable metadata for
unavailable-session mutations. A second review confirmed that Calendar import
loads signatures before saving and propagates query failures unchanged, so
the import signature-query adapter must be included with import save for an
unavailable-session error to reach the caller as recoverable.

Accept F1037: in the remaining Calendar series create/edit/delete, single-event
delete, delete-all, and import-save adapters, and the import signature-query
adapter, mark only absent/closed-session failures `NotFound, recoverable=true`
while preserving each existing operation message. Keep invalid input,
missing entities, open-session repository failures, and technical errors
unchanged and nonrecoverable. The import UI availability preflight and all UI
draft behavior remain unchanged. Extend the existing platform adapter tests
for each unavailable-session case and retain other failure classifications.
Add an import use-case assertion that the recoverable signature-query failure
reaches the caller without saving. Build `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and
`ClassMngrNextApplicationCalendarEventImportUseCaseTests`; run only the two
matching CTests. F1036 at `f3124488` is the latest source commit. Post-F1036
audits are current: all 13 official Deferred entries remain unchanged; repeat
both audits after F1037 implementation. Gates 1/2 remain Partial, F1007
remains pending policy reconciliation, and the Phase 2 exit gate remains
Open. F1037 is accepted and pending implementation.

F1037 is complete at source commit `eb4da0c9`. Seven Calendar adapters now
return `NotFound, recoverable=true` only for unavailable/closed-session
failures, preserving each existing message. Generic failure helpers remain
nonrecoverable for invalid input, missing entities/signatures, open-session
repository failures, and technical errors. Platform tests assert exact
unavailable-session metadata; an import use-case test verifies a recoverable
signature-query error propagates without calling save.

The independent Tester built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and
`ClassMngrNextApplicationCalendarEventImportUseCaseTests`; the two focused
CTests passed 2/2 after an elevated Visual Studio FileTracker retry. No full
suite ran. The executor's elevated build also logged nonfatal `MSB8064` for a
missing dependency path `src/next/application/teacher_import_plan_mapper.h`;
the independent build and CTests passed. Tests do not deterministically close
a session during a repository call, but source review confirmed those
post-failure branches use the same dedicated helper.

Two independent post-F1037 audits reviewed exactly the 13 official Deferred
entries. All remain unchanged and unresolved; none became actionable,
resolved, or superseded. F1037 does not establish a Calendar projection
injection seam. Gates 1/2 remain Partial, F1007 remains pending policy
reconciliation, and the Phase 2 exit gate remains Open.

Batch417 rejected an apparent My Classes assigned-teacher presentation issue:
F913 and `failedTeacherProfileKeepsClassAndUsesSilentUnassignedFallback()`
explicitly preserve the silent Unassigned fallback when the profile cannot be
loaded. Batch418's three independent reviews found no supported slice; F285
and F707 remain behind their recorded triggers, and the Speaking Evaluation
blank-grid fallback remains explicitly tested pending policy. All 13 official
Deferred entries remain unchanged.

F1038 is complete at source commit `586a03f3`. The Roster Save adapter now
returns `NotFound,recoverable=true` only for an unavailable session, retaining
the exact existing message. The shared failure helper and separate Row
Transfer path are unchanged. Tests assert closed-session recoverability and
missing-parent nonrecoverability.

The executor built `ClassMngr` and
`ClassMngrNextPlatformApplicationServicesRosterSavePortTests`; focused CTest
passed 1/1. The independent Tester repeated the incremental Debug build and
CTest, also passing 1/1 after the ordinary MSBuild FileTracker attempt failed
with `E_ACCESSDENIED` and an elevated retry succeeded. No full suite ran.

Two independent post-F1038 audits checked exactly all 13 official Deferred
entries. All remain unchanged and unresolved, with none actionable, resolved,
or superseded. Batch419 is closed with no other supported slice found.
Batch420 discovery is in progress. Gates 1/2 remain Partial, F1007 awaits
policy reconciliation, and Phase 2 remains open.

Batch421 rejected the Workspace Save commit-result lead: no production
transaction spans Save, and the legacy `commit()` result cannot distinguish an
ordinary no-transaction save from a failed pending commit. This repeats the
Batch371 conclusion; no official Deferred entry changed.

F1039 is complete at source commit `2a66e3f2`. `LanguageService` stages a
separate translator and retains the prior active translator and service state
when locale load or installation fails. Language QAction flow stages the
locale, writes the preference, applies the prepared translator, and then
commits the checked action and `OptionState`. Install failure restores the
exact prior raw setting, including absence or an invalid variant; failure of
that rollback is surfaced with the apply error in one diagnostic. Programmatic
`OptionState::set()` retains its existing commit and callback behavior.

The implementation added deterministic load/install failure, exact restore,
rollback-failure, presentation preservation, and retry coverage to the
existing LanguageService, LanguagePreferencePort, and MainWindow document
catalog parity targets. The executor built `ClassMngr` and all three targets;
the exact three CTests passed 3/3. Independent verification repeated the
focused build under Debug Ninja/MSVC and passed 3/3. `git diff --check` passed;
no full suite ran.

An independent post-F1039 audit checked the exact 13 official Deferred
entries. None is newly actionable, resolved, or superseded. F579 and F598
remain blocked on elevated capture access; future captures should use a build
including F1039's translator lifetime changes. Batch421 is closed and Batch422
discovery is active. Gates 1/2 remain Partial, F1007 awaits policy
reconciliation, and the Phase 2 exit gate remains Open.

Batch422 used three independent scans and found no eligible slice. The
Schedule Testing Assignment recoverability mismatch is explicitly locked by
closed-session save tests; other slot-state mappings agree. Non-language
preference actions already persist before state or restore selection, while
the PowerPoint notice remains a documented macOS legacy void write. The
application-use-case scan found failure propagation or explicit tested
contracts, including F971's limit on ImportJob pump error reporting. Batch422
is closed as a discovery shortfall; Batch423 discovery is active. Gates 1/2
remain Partial, F1007 awaits policy reconciliation, and Phase 2 remains open.

Batch423 also found no eligible slice. The read/query scan confirmed that
Teacher Import malformed dates, Class Details partial snapshots, My Classes
batch completeness, and Campus Dashboard malformed-record handling all match
explicit tests. The Qt-free boundary scan found no Qt/UI/legacy UI dependency
in `src/next/domain` or `src/next/application`; existing CMake dependency
assertions and app-less contract tests enforce the gate. No official Deferred
item changed. Batch423 is closed as a discovery shortfall; Batch424 discovery
is active. Gates 1/2 remain Partial, F1007 awaits policy reconciliation, and
Phase 2 remains open.

F1040 is complete at source commit `6035349b`. `StaffDirectoryPage::loadDirectory()`
now stages both Native English and GS Team rows and replaces the table only
after a successful read. Technical read failures retain the displayed model,
IDs, local edits, pending deletions, and dirty state while preserving the
warning. NotFound/unavailable-session clearing remains unchanged. The executor
and independent Tester built `ClassMngr` and both existing directory read
targets; their focused CTests passed 2/2. No full suite ran.

Two independent post-F1040 audits reviewed all 13 official Deferred entries;
none is actionable, resolved, or superseded. F579's review trigger is
satisfied, but F579/F598 captures require an elevated Administrator token;
F629 still needs a suitable macOS universal Phase 0 runner. The remaining
measurement, source-seam, and policy prerequisites are unchanged. Batch424 is
closed after accepting F1040; Batch425 discovery is active. Gates 1/2 remain
Partial, F1007 remains pending policy reconciliation, and Phase 2 remains
Open. Preserve unrelated user work.


A temporary environment-gated pagination trace is now present in the dirty
renderer source. It leaves layout behavior unchanged and records per-pass card
bounds, spans, break sets, and final assignments. The fresh Mac universal
Release build/install and signature check passed; the focused route reached
the strict parity assertion. Trace shows pass 1 inserts a break before Teacher
21 after its card spans pages 8-9; final Mac assignments still differ on
pages 9-10. Trace SHA-256 is
`38ce3cf811c7624bc630d32b8b1c3aaa3e79ed5edfcd3b30bc488d8736476773`.
The user-provided Windows run contains no per-card trace, so the matching
one-route Windows rerun is pending. No functional fix or acceptance is claimed.

## Phase 2 update - F629 complete; F1091 selected - 2026-10-19

F629 source commit `31a71e90` fixes Sub Prep page assignment using a generic
card-boundary rule and makes startup-profiler settle timers precise. Fresh
macOS universal run8 passed Release/Debug builds, package signature and the
93-file universal audit, plus all 24/24 routes. Strict semantic output parity
matches 12/12 Sub Prep pages and 5/5 roster pages. The combined Phase 0 gate
remains open because Windows x64 full-matrix evidence is missing; Phase 2 also
remains open. Batch473 is closed. Batch474 is active, with F1091 selected to
propagate signature-image technical read failures and prevent aggregate saves
from replacing the stored image; F1092-F1094 remain provisional. Details and
evidence paths are in the Phase 2 plan and progress log.

## Phase 2 update - F1091 complete; F1092 selected - 2026-10-19

F1091 source commit `cd2e7a68` propagates signature-image technical read
failures and blocks aggregate personal-details saves in Initial Setup and My
Details until the image reloads. Missing, corrupt, or unavailable images retain
the existing empty fallback, including Speaking Evaluation reports. The
focused port and My Workspace CTests passed; the isolated Initial Setup F1091
case passed. The full Initial Setup class still aborts before that case in a
pre-existing macOS Qt wizard path. Batch474 continues at F1092, which is under
read-error reproduction before implementation acceptance.

## Phase 2 update - F1092 complete; F1093 selected - 2026-10-19

F1092 source commit `ec07d16e` returns a technical error for Sidebar Display
settings read failures and prevents an unknown pair of defaults from being
written. A real macOS QSettings access error reproduced the data-loss path; the
recovery path now reloads both stored values, merges only the toggled action,
and keeps the companion action and live Sidebar behavior aligned. `ClassMngr`
and both selected test targets built, and both focused CTests passed. Batch474
continues at F1093. A read-only Batch475 discovery recheck found no additional
eligible candidates or newly met Deferred triggers.

## Phase 2 update - F1093 complete; F1094 selected - 2026-10-19

F1093 source/test commit `45c9aba9` makes Class Visibility preference reads
status-aware. Technical repository failures no longer become defaults or
overwrite the active Classes page projection. ClassesPage and SpeakingEvalPage
retain the applied scope during failed reads, while a recovered value is
applied after storage recovers. Navigation Preferences restores its last known
radio selection if both an explicit save and its follow-up read fail.

`ClassMngr` and the existing port, ClassesPage, SpeakingEvalPage, and MainWindow
class-visibility test targets built incrementally. The port and MainWindow
focused CTests passed 2/2; the focused ClassesPage and SpeakingEvalPage cases
passed. The full ClassesPage CTest had 40 passes and 8 failures in other page
behaviors; F1093's test passed. No full application suite ran. Batch474
continues at F1094, selected to reproduce and define the two Classes page reset
preference read-error behaviors. The combined Phase 0 and Phase 2 exit gates
remain open.

## Phase 2 update - F1094 complete; Batch475 selected - 2026-10-19

F1094 source/test commit `5c4201ae` makes both Classes page reset-policy reads
return structured results. Repository errors remain recoverable Technical
failures and do not materialize an `OnApplicationClose` default. `ClassesPage`
applies each hide-time reset only after its own successful read, and Navigation
Preferences restores the last known radio selection after a save and follow-up
read both fail. Batch474 is complete; its documentation closeout is being
committed before the batch push.

Incremental `macos-clang-debug` built `ClassMngr` and the two port, ClassesPage,
and MainWindow failure targets. The focused port CTests passed 2/2; the
ClassesPage regression and both MainWindow reset-policy cases passed. The
MainWindow tests cover a successful policy switch followed by a save and
reload failure. `git diff --check` passed. This was focused macOS arm64
verification; Windows x64 execution is unavailable, and no full suite ran.

Batch474 was pushed to `origin/Qt-Rewrite` at `fbcbb6c9`. Batch475 has one
accepted slice, F1095: carry the existing Calendar event time-status enum
through Application contracts while preserving canonical legacy text and
normalization behavior. Its acceptance is recorded before implementation.
No other slices were found in the reviewed work-package and settings-read
surfaces. The Phase 0 and Phase 2 exit gates remain open.

## Phase 2 update - F1098 complete; Batch477 closeout pending - 2026-10-19

F1098 is complete at source/test commit `fb92cdd`; its acceptance and
clarification are recorded in `478d1799` and `6f713932`. Calendar repeat-series
identity now remains typed through the Application contracts and relevant
boundaries. The focused macOS arm64 verification built `ClassMngr` and five
existing Calendar targets; all five focused CTests passed, and
`git diff --check` passed. No full suite ran.

Batch477 is complete, but its documentation closeout and push are pending. The
Git handoff could not stage the documentation because the automatic approval
reviewer twice reported that its selected model was at capacity; no Git action
ran. Batch478 has five discovered candidates: F1099 is selected next, pending
Batch477 closeout; F1100 is a typed `ClassId` ordering follow-up; F1101 is a
lower-confidence `CampusId` snapshot decision; F1102 is a Class Details
validation identity follow-up; F1103 remains deferred pending parity/policy
reconciliation. No newly actionable Deferred/Gate-derived item was found.

The combined Phase 0 gate remains open because Windows x64 evidence is
unavailable and the user cannot run Windows at present. The Phase 2 exit gate
remains open, Gates 1/2 remain Partial, and F1007 remains pending policy
reconciliation.

## Phase 2 update - F1099 complete; F1100 selected - 2026-10-19

F1099 is complete at source/test commit `a942daab`, after pre-implementation
acceptance commit `2721941c`. The Calendar event-by-ID request now carries an
optional `CalendarEventId`; the UI constructs it at the QString boundary and
the Application use case forwards it unchanged. Existing validation, exact
identifier text, Platform decimal parsing, and returned-ID mismatch behavior
remain intact.

The incremental `macos-clang-debug` build passed for `ClassMngr` and all three
requested test targets. The focused Application Calendar Event, Platform
Application Services Calendar Event Port, and Calendar Page mutation parity
CTests passed 3/3. `git diff --check` passed. This is local macOS arm64/Qt 6.12
verification; no fresh configure or full suite ran. Batch478 remains active;
F1100 is selected next. F1101 remains lower-confidence, F1102 awaits
reassessment after earlier entries, and F1103 remains deferred.

The combined Phase 0 gate remains open because Windows x64 evidence is
unavailable. The Phase 2 exit gate remains open, Gates 1/2 remain Partial, and
F1007 remains pending policy reconciliation.

## Phase 2 update - F1100 accepted - 2026-10-19

F1100 is accepted as the next Batch478 slice. Keep `Domain::ClassId` typed
through the Sub Prep visible-class ordering fact. `TypedId`'s default ordering
compares the underlying string, preserving the existing lexical class-ID
tie-break. Preserve the F1017 sort tuple, literal numeric sentinels, stable
exact ties, model-owned rank/label/filtering, and sorted-first selection
fallback. Focused verification will build the application, ordering policy,
visible-class model, and Sub Prep page targets and run their exact CTests only.

Batch478 remains active; F1099 is complete and F1100 is accepted before
implementation. F1101 remains lower-confidence, F1102 remains to be
reassessed, and F1103 remains deferred. The combined Phase 0 and Phase 2 exit
gates remain open; Gates 1/2 remain Partial, and F1007 awaits policy
reconciliation.

## Phase 2 update - F1100 complete; F1101 reassessment started - 2026-10-19

F1100 completed at source/test commit `820f54a2`, after acceptance commit
`a49536c4`. `Domain::ClassId` now remains typed through the Sub Prep visible
class ordering fact; its existing lexical comparison and the F1017 ordering
and sorted-first fallback behavior are preserved.

The incremental `build/macos-clang-debug` build succeeded for `ClassMngr` and
the ordering policy, visible-class model, and Sub Prep page targets. The
policy CTest passed 1/1; the model CTest passed 1/1 (QtTest 7/7). The page
selector did not finish cleanly: the F1017 sorted-first fallback case passed,
but the page target had one warning-count assertion failure and one hanging
case. Both observations reproduced with pre-F1100 production files, so they
are existing page-target limitations. No full suite ran; `git diff --check`
passed. Verification was incremental macOS arm64/x86_64 with Qt 6.12.

Batch478 remains active; F1099 and F1100 are complete. F1101 is selected for
reassessment and remains lower-confidence; F1102 remains discovered and
F1103 remains deferred. The combined Phase 0 and Phase 2 exit gates remain
open, Gates 1/2 remain Partial, and F1007 awaits policy reconciliation.

## Phase 2 update - F1101 accepted before implementation - 2026-10-19

F1101 is accepted to replace the owning string `id` fields in the MyInfo,
Sub Prep, and Calendar Page campus metadata contracts with `Domain::CampusId`.
This value type preserves the exact string and performs no normalization;
feature consumers must continue using their existing trimmed,
case-insensitive preference matching, display-name fallback, and Calendar
alias behavior. The campus codec generates a nonempty ID for blank input,
while padded IDs remain unchanged. F23 applies specifically to CalendarPage;
MyInfo and Sub Prep have separate UTF-8 snapshot history.

F1101 is accepted, not implemented. Its focused scope covers the existing
Application contracts, three Platform adapters, and relevant MyInfo, Sub
Prep, and Calendar Page UI paths. Batch478 remains active; F1099/F1100 are
complete, F1102 remains discovered, and F1103 remains deferred. The combined
Phase 0 and Phase 2 exit gates remain open.

Before implementation, a bounded use-site scan found Campus Dashboard also
consumes `CalendarPageCampusMetadata`. F1101's acceptance scope now includes
its exact ID equality check during dirty refresh. Existing behavior saves and
reloads the dirty draft when that ID remains present; a missing-ID refresh
preserves the dirty draft. A separate focused regression covers the
present-ID save path, while the existing missing-ID regression stays intact.

## Phase 2 update - F1101 complete; F1102 next - 2026-10-19

F1101 is complete at source/test commit `cd64b46becee89c10faec37194482615fe1876d5`.
The MyInfo, Sub Prep, and Calendar Page metadata contracts now carry
`Domain::CampusId`; their Platform adapters preserve exact UTF-8 ID bytes, and
UI consumers unwrap only at existing text boundaries. Existing preference
matching, Calendar aliases, optional campus-code handling, and Campus Dashboard
dirty-refresh behavior remain intact.

The incremental `build/macos-clang-debug` build succeeded for `ClassMngr` and
nine affected test targets. The five Application/Platform CTest selectors
passed 5/5. Focused MyInfo and Sub Prep selection cases, the refined Calendar
ID-alias case, and both Campus Dashboard present-ID and missing-ID dirty-refresh
cases passed. `git diff --check` passed. Verification was local macOS arm64 with
Qt 6.12; no fresh configure or full suite ran.

Batch478 remains active and unpushed: F1099-F1101 are complete, F1102 is next
for acceptance reassessment while preserving the unassigned-teacher sentinel
and wide teacher IDs, and F1103 remains deferred pending parity/policy
reconciliation. Starting F1102 also triggers the next-batch discovery required
by the Phase 2 plan. The combined Phase 0 gate remains open while Windows x64
evidence is unavailable; the Phase 2 exit gate remains open.

## Phase 2 update - F1102 accepted; Batch479 discovered - 2026-10-19

F1102 is accepted before implementation. Type both IDs in
`ClassDetailsValidationInput` and its normalized result as `Domain::ClassId`
and `Domain::TeacherId`; keep the teacher `-1` sentinel and 64-bit range,
invalid numeric diagnostics, and policy issue ordering. Keep the validation
context snapshot's teacher field as `std::int64_t`; context-read fallback
policy remains deferred under F1103. No F1102 source or test changes have
started.

The required next-batch discovery is complete. Batch479 has five ordered
candidates and is queued behind Batch478. Batch478 remains active and
unpushed: F1099-F1101 are complete, F1102 is accepted, and F1103 remains
deferred. The combined Phase 0 gate remains open while Windows x64 evidence is
unavailable; the Phase 2 exit gate remains open.

## Phase 2 update - F1102 complete; Batch478 closeout - 2026-10-19

F1102 is complete at source/test commit
`b5e5a1628b7e0fc7606a4b314b90e03719750171`. The validation input and normalized
result now carry `Domain::ClassId` and `Domain::TeacherId`; the policy keeps
numeric validation and diagnostics, the `-1` teacher sentinel, wide teacher
IDs, and existing issue ordering. The validation-context snapshot remains
numeric, and F1103's fallback behavior is unchanged.

Incremental macOS arm64/Qt 6.12 verification built `ClassMngr` and four
existing test targets. Both Application CTests passed 2/2. Named cases for
invalid save blocking, fresh invalid context, wide teacher UI save, and
`workflowContextFailureAndMismatchFallBack` passed. `git diff --check` passed;
no full suite ran.

All Batch478 candidates are resolved: F1099-F1102 are complete and F1103
remains deferred. Batch478's closeout was pushed to `origin/Qt-Rewrite` at
`5877b296c0c9a032adeeeb9cbfb807e19765e40a`. Batch479 is active, with F1104
selected for acceptance reassessment. The
combined Phase 0 gate remains open while Windows x64 evidence is unavailable;
the Phase 2 exit gate remains open.

## Phase 2 update - F1104 accepted; Batch479 active - 2026-10-19

F1104 is accepted for implementation; no implementation has started. Type
`ScheduleTestingAssignmentReadRow.classId` as `Domain::ClassId`, adapting the
existing repository `int` in the Platform port and converting back at the
Schedule widget's legacy UI boundary. Keep the empty read query, repository
records, and legacy view models unchanged. Preserve row order, display fields,
special-class metadata behavior, read failures, and unassigned rendering.

The handler must continue accepting only exact `-1` or positive IDs in the
existing `int` range, rejecting zero, other negatives, malformed/trailing
text, and overflow with the same nonrecoverable Validation error. This
validation remains necessary because `TypedId::fromString` accepts any
nonempty string. F1105-F1108 remain discovered. Batch478's closeout push is
recorded at `5877b296c0c9a032adeeeb9cbfb807e19765e40a`; the combined Phase 0
gate and Phase 2 exit gate remain open.

## Phase 2 update - F1104 complete; Batch479 active - 2026-10-20

F1104 is complete at source/test commit
`ddcb8bbb4110f14e7f601be9845b159122c11fee`, following acceptance commit
`859ffef265bd640a68f81b76110e4db7f0bacc54`. The Schedule testing-assignment
read row carries `Domain::ClassId`; the Platform adapter and Schedule widget
convert at the boundaries while repository and legacy UI models remain
integer-based. The handler accepts only literal `-1` or positive int-range
IDs, rejects zero, other negative spellings, malformed/trailing text, and
overflow with the existing nonrecoverable Validation error, and preserves
ordering, metadata, display fields, read failures, and unassigned rendering.

Focused verification on Darwin arm64 / Qt 6.12.0 built the Application,
Platform, and Schedule widget test targets. Application and Platform read
tests passed 8/8 and 6/6. The unassigned and assigned widget selectors passed
3/3 each; independent `git diff --check` passed. A broader widget interaction
run stalled in a modal case and was interrupted; no full widget suite result
is claimed. Batch479 remains active, with F1105 next for acceptance
reassessment and F1106-F1108 discovered. Batch479 has not been pushed; the
Phase 0 gate and Phase 2 exit gate remain open.

## Phase 2 update - F1105 accepted; Batch479 active - 2026-10-20

F1105 is accepted for implementation; no implementation has started. Type
`CampusDashboardSelectedCampusReadRequest.campusId` as `Domain::CampusId`,
keeping the existing typed port, adapter, result, and snapshot. The Campus
Dashboard page keeps its empty-selection early return and explicitly encodes
the selected value through `QString::toUtf8()` before constructing the typed
ID.

The query continues rejecting Qt-compatible whitespace-only IDs and values
over 256 UTF-8 bytes; exactly 256 bytes remains valid. Empty values cannot be
represented in the typed request, so the existing page guard preserves that
behavior. Preserve exact returned-ID matching and the nonrecoverable Technical
mismatch error, port failures, successful absence, and current handling of
malformed UTF-8. Do not trim, normalize, or case-fold. F1106-F1108 remain
discovered; Batch479 is unpushed and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1105 complete; Batch479 active - 2026-10-20

F1105 is complete at source/test commit
`1d86cc57f1884915880dce544fd453743aadf4bb`, following acceptance commit
`c4e933fd8d8f1b5e456cedbcdddadddc3190ff8c`. The selected-campus read request
now carries `Domain::CampusId`; the page preserves its empty-selection early
return and encodes the selected `QString` via `toUtf8()`. The query validates
the typed value's UTF-8 bytes against the same Qt-compatible whitespace rule
and 256-byte limit, and preserves exact result-ID matching and existing error
behavior. It does not trim, normalize, case-fold, or reject malformed UTF-8
that previously passed through.

Focused independent verification built the Application query and Campus
Dashboard page targets. The Application query executable passed; the
non-ASCII UTF-8 and empty-list page selectors passed 4 QTest cases total.
`git diff --check` passed. No full suite ran. Batch479 remains active and
unpushed, with F1104-F1105 complete, F1106 next for acceptance reassessment,
and F1107-F1108 discovered. The Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1106 accepted; Batch479 active - 2026-10-20

F1106 is accepted for implementation; no implementation has started. Change
`ClassCoTeacherAssignmentUseCase::execute` to accept `Domain::ClassId` and
`Domain::TeacherId`, converting the page's legacy numeric values to decimal
strings at the caller boundary. Preserve literal `-1` as the unassigned
teacher sentinel and map it to `std::nullopt` only in the typed port request.
The use case retains full decimal parsing, positive `int` class-ID and
positive `int64_t` teacher-ID ranges, rejection of zero, other negatives,
malformed/trailing text and overflow, current recoverable `InvalidInput`
messages, and wide teacher IDs. Platform, persistence, and legacy view
models stay unchanged.

Batch479 remains active and unpushed: F1104-F1105 are complete, F1106 is
accepted, and F1107-F1108 are discovered. The combined Phase 0 gate and
Phase 2 exit gate remain open; Windows x64 evidence remains unavailable.

## Phase 2 update - F1106 complete; Batch479 active - 2026-10-20

F1106 is complete at source/test commit
`8a6e8174233b502815f22c4ff3df55029986cc40`, following acceptance commit
`fdc95c7c1aaef206c6650150405fb5d63a09c781`. The Co-Teacher assignment
use-case entry now accepts `Domain::ClassId` and `Domain::TeacherId`; the page
converts its legacy numeric values at the UI boundary. The use case fully
validates positive `int`/`int64_t` values, preserves exact `-1` as the
unassigned sentinel, and forwards wide IDs. Existing integration test callers
were adapted; no Platform production code changed.

Focused independent verification on Darwin arm64, Ninja Debug, and Qt 6.12.0
passed the Application assignment, Platform assignment integration,
persistence parity, and wide-identity page CTests 4/4. Application maximum
integer-boundary tests passed; Platform integration passed 16 cases, parity
passed 3, and the wide page selector passed 3. The default-port page save and
manual-failure selectors also passed. `git diff --check` passed. No full suite
ran.

Batch479 remains active and unpushed: F1104-F1106 are complete, F1107 is next
for acceptance reassessment, and F1108 remains discovered. The Phase 0 and
Phase 2 gates remain open; Windows x64 evidence is unavailable.

## Phase 2 update - F1107 accepted; Batch479 active - 2026-10-20

F1107 is accepted for implementation; no implementation has started. Type
the teacher and class identity fields in the Sub Prep print-order facts and
adapt their existing positive-int feature-model boundary. Preserve grouping,
name/key priorities, and numeric teacher-ID and class-ID tie-breaks because
typed IDs compare lexically by default. Keep source filtering, renderer
models, and persistence unchanged. Batch479 remains active and unpushed:
F1104-F1106 are complete, F1107 is accepted, and F1108 is discovered.
Windows x64 evidence remains unavailable; the Phase 0 and Phase 2 gates
remain open.

## Phase 2 update - F1107 complete; F1108 next - 2026-10-20

F1107 is complete at source/test commit
`600c119cc912dd0c713c95e8acbd68639a4a85f5`, following acceptance commit
`dd1a650b9e3aa58266f16bee21dce6da80538a86`. The Sub Prep print-order facts
now carry typed teacher/class IDs; the feature model constructs them at its
existing positive-int boundary. The policy preserves grouping and numeric
teacher/class tie-breaks, including deterministic nonthrowing fallback for
unexpected typed text.

The focused Application policy and class-information model CTests passed
2/2 on macOS arm64. Regression coverage includes 2-versus-10 ordering for
both ID tie-breaks, malformed typed IDs without throwing, and existing
grouping/name/key/sentinel/input-order behavior. `git diff --check` passed;
no full suite ran. Batch479 remains active and unpushed: F1104-F1107 are
complete, and F1108 is next for acceptance reassessment. Windows x64 evidence
remains unavailable; Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1108 accepted; Batch479 active - 2026-10-20

F1107 closeout documentation is committed at
`114cc86cdc7cd2262dbb5869187cc0a6fa58ff34`. F1108 is accepted for
implementation; no implementation or tests have started. Type the
Sub Prep selected-class schedule fact and Application policy result with
`Domain::ClassId`, retaining `QList<int>` at the existing feature/UI
boundary. The policy will parse full decimal `int` values, retain only
positive IDs, deduplicate numerically, and emit canonical typed IDs. The
adapter bridges legacy integer IDs without changing exact day matching,
schedule order, or inclusion of both entry kinds. Batch479 remains active
and unpushed; Windows x64 evidence is unavailable and the Phase 0 and Phase 2
gates remain open.

## Phase 2 update - F1108 complete; Batch479 closed - 2026-10-20

F1108 completed at source/test commit
`ef431808732e3d5e385a7a509574a70224083379`, following acceptance commit
`98b22b7fc3379d454010ac031fd4c115a3a3237b`. The selected-class schedule
fact and Application policy result now carry `Domain::ClassId`; the existing
service and UI `QList<int>` API remains the legacy edge. Full base-10 int
parsing preserves positive-only filtering and numeric deduplication, with
canonical typed results. Malformed, trailing, overflow, zero, and negative
IDs are skipped without throwing. Exact day matching, schedule order, and
both entry kinds remain unchanged.

On macOS arm64 with Apple Clang 21, Qt 6.12.0, and Ninja Debug, the incremental
`ClassMngrFeatures` build succeeded. Focused Application policy and package
service projection CTests passed 2/2; `git diff --check` passed. No full suite,
fresh configure, or clean build ran. Batch479 is complete. Batch480 has two
ordered candidates, with F1109 next for acceptance reassessment and F1110
discovered. Batch479 closeout commit
`8a2f11318b54425c58bfadc83fa1526af1d69091` was pushed to
`origin/Qt-Rewrite`; the remote matched and the worktree is clean. F629 and the
combined Phase 0 gate remain open pending Windows x64 evidence; Phase 2 also
remains open.

## Phase 2 update - F1109 accepted; Batch480 active - 2026-10-20

F1109 is accepted for implementation; no implementation or tests have
started. Type only `ClassTabNavigationOrderingFact.classId` as
`Domain::ClassId` and adapt the existing feature boundary from its integer
model. Preserve the signed numeric `int32_t` final tie-break, the prior tuple
fields and injected label comparison, typed `-1` default, and exact-tie
behavior. Parse full numeric strings and provide deterministic ordering for
malformed/out-of-range typed values; keep UI and persistence IDs unchanged.
Batch480 remains active with F1109 accepted and F1110 discovered. Batch481
discovery is underway.

## Phase 2 update - F1109 complete; Batch480 active - 2026-10-20

F1109 completed at source/test commit `d39a2cb1`, following acceptance commit
`1fb1ef6d`. The Class Tab Application ordering fact now carries
`Domain::ClassId`; the feature adapter canonicalizes the existing integer ID,
and output/UI/persistence IDs remain integers. Numeric signed-int32 tie-break,
tuple priority, injected label comparison, exact ties, and typed `-1` default
are preserved. Full int32 parses compare numerically, equal numeric spellings
compare lexically, and malformed or out-of-range typed text receives a
deterministic lexical order.

The independent Tester built the focused policy and model targets in the
existing `build/macos-clang-debug` tree; both focused CTests passed 2/2.
Coverage includes signed boundaries, aliases, malformed/partial/out-of-range
IDs, strict ordering across representative facts, and a reversed-input 2/10
model case with integer output preserved. `git diff --check` passed. No full
suite or Windows x64 run occurred. Batch480 remains active with F1110 next for
acceptance reassessment. Batch481 discovery identified F1111, an Application
value type for Teacher Import's transient review-group correlation key; it
remains discovered, not accepted. F629 and the Phase 0 and Phase 2 gates
remain open pending Windows x64 evidence.

## Phase 2 update - F1110 accepted; Batch480 active - 2026-10-20

F1109 is complete at source/test commit `d39a2cb1`, following acceptance
commit `1fb1ef6d`; completion documentation is committed at `2565f323`.
F1110 is accepted, with implementation and tests not started. Type the
roster-transfer eligibility IDs as `Domain::ClassId` and preserve typed
class-list IDs through the pre-read guard and subtitle batch query. Apply
canonical positive decimal int validation because the wrapper itself accepts
arbitrary nonempty strings. Keep invalid/current-ID no-read behavior,
Qt-compatible whitespace trimming, case-sensitive grade equality, candidate
order and duplicates, and int-based menu/action IDs. Focused targets are the
Application eligibility tests and Roster Transfer Menu tests. Batch481's
F1111 Teacher Import correlation-key value remains discovered, not accepted.
F629 and the Phase 0 and Phase 2 gates remain open pending Windows x64 evidence.

## Phase 2 update - F1110 complete; Batch480 closeout pending - 2026-10-20

F1110 completed at source/test commit `0a7bb774`, following acceptance commit
`4b79475d`. The roster-transfer Application eligibility helpers now accept
`Domain::ClassId` and enforce canonical positive decimal int values before
eligibility or target metadata reads. The feature retains typed IDs from the
class-list snapshot through the pre-read guard and subtitle batch request;
menu/action IDs remain integers. Qt-compatible whitespace trimming,
case-sensitive trimmed-grade equality, candidate order/duplicates, menu
sorting, and existing labels remain unchanged.

The incremental `ClassMngr` build succeeded in `build/macos-clang-debug`.
Focused eligibility and Roster Transfer Menu CTests passed 2/2. Coverage
includes invalid/noncanonical/overflow IDs, current and invalid no-read cases,
grade matching, order/duplicates, and unchanged integer transfer behavior.
`git diff --check` passed. No full suite or Windows x64 run occurred. Batch480
is complete; its closeout documentation is being committed before the
authorized push. Batch481 has one discovered candidate, F1111, a transient
Teacher Import review-group correlation-key value. F629 and the Phase 0 and
Phase 2 gates remain open pending Windows x64 evidence.

## Phase 2 update - Batch480 pushed; Batch481 active - 2026-10-20

Batch480 closeout commit `7acd5fac0646b9868c7b66659f617e5cf300926d` was
pushed without force to `origin/Qt-Rewrite`. The remote SHA matched, the
worktree was clean, and the branch was up to date. Batch481 is active with
F1111 discovered and next for acceptance reassessment. F629 still needs
source-bound Windows x64 layout evidence; the Phase 0 and Phase 2 gates remain
open.

## Phase 2 update - F1111 accepted; Batch481 active - 2026-10-20

F1111 is accepted; implementation and tests have not started. Add an
Application `TeacherImportGroupKey` value type for the candidate-group,
decision, and use-case review keys. Preserve exact string bytes and matching,
including default-empty values, and leave empty, duplicate, unknown, and
missing-key validation with the existing F44 resolver. Keep ordering, UTF-8
adapter boundaries, and legacy UI/persistence representations unchanged; do
not normalize or use `Domain::TypedId`. Batch482 discovery is underway.
F629 still needs source-bound Windows x64 layout evidence; the Phase 0 and
Phase 2 gates remain open.

## Phase 2 update - F1111 complete; Batch481 closeout pending - 2026-10-20

F1111 completed at source/test commit `4a9dfd44`, following acceptance commit
`5515158a`. The Application `TeacherImportGroupKey` now carries the three
Teacher Import review-group keys with exact byte equality and default-empty
support; F44 retains empty, duplicate, unknown, missing, and index validation.
The UI and repository adapters keep their UTF-8 conversion, and legacy UI and
persistence representations are unchanged. Focused macOS build and tests
passed 3/3. Arbitrary UTF-8 is verified through the resolver, use case, and
repository adapter; UI dialog coverage exercises its fixed supported ASCII
levels because the parser cannot emit arbitrary non-ASCII group levels. No
full suite or Windows x64 run occurred. Batch481 closeout documentation is
pending; Batch482 has F1112-F1114 discovered, with F1112 next for acceptance.
F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - Batch481 pushed; Batch482 active - 2026-10-20

Batch481 closeout commit `b79aa10d657ccfdfaa09d1458a5d1c6b2d13034d` was
pushed without force to `origin/Qt-Rewrite`. The remote SHA matched, and the
worktree was clean and up to date. Batch482 is active with F1112 next for
acceptance reassessment; F1113 and F1114 remain discovered, not accepted.
F629 still needs source-bound Windows x64 layout evidence; the Phase 0 and
Phase 2 gates remain open.

## Phase 2 update - F1112 accepted; Batch482 active - 2026-10-20

F1112 is accepted before implementation. Add Application
`ScheduleImportReviewTeacherKey` to the three Schedule Import review-decision
key fields and preserve exact `std::string` bytes, empty/default values,
grouping, validation, and issue order. Update only the review validator and
feature/apply-review construction adapters; leave apply-request and state
validation keys unchanged. F1113 and F1114 remain discovered, not accepted.
F629 still needs source-bound Windows x64 layout evidence, which is not
available now; the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1112 complete; Batch482 active - 2026-10-20

F1112 completed at source/test commit `9110a8c8`, following acceptance commit
`17aadcb1`. The Application review-decision candidate, teacher-resolution,
and issue keys now use `ScheduleImportReviewTeacherKey`, preserving exact
bytes, empty values, validation, grouping, issue order, and room aggregation.
The validator and feature/apply-review adapters retain the existing UTF
conversions; apply-request and state-validation contracts remain unchanged.
Focused review-decision, apply-projection, and plan-validation CTests passed
3/3; the Schedule Import dialog adapter selector also passed. No full suite or
Windows run occurred. F1113 is next for acceptance review; F1114 remains
discovered. F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1113 accepted; Batch482 active - 2026-10-20

F1113 is accepted before implementation. Use distinct exact Application key
types for Class Transfer package class and teacher correlation keys, carrying
them through projection, preview matching, and review/apply records and
lookups. Keep construction permissive and preserve path-specific blank,
256-byte, uniqueness, exact-reference, fallback, and issue-order rules.
Preserve the repository-versus-dialog blank-key normalization boundary and
legacy UTF-8 conversions; leave portable-package wire fields, JSON, legacy
models, and persistence maps unchanged. F1114 remains discovered. F629 and
the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1113 complete; Batch482 active - 2026-10-20

F1113 completed at source/test commit `d2e81f6c`, following acceptance commit
`67cd2da0`. Separate exact Application `ClassTransferTeacherKey` and
`ClassTransferClassKey` types now cover projection, preview matching, review,
apply, and typed projection lookup paths. Existing projection validation,
preview fallback/order, review issues, UTF-8 adapters, and the dialog versus
repository blank-key behavior remain intact. Focused verification built three
targets; the preview CTest passed 1/1 and selected Application and feature
QTest cases passed. No full suite or Windows run occurred. F1114 is next for
acceptance review; F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1114 accepted; Batch482 active - 2026-10-20

F1114 is accepted before implementation. Replace the generic import-review
source-key alias with an opaque `ImportSourceKey` value type across match
indexes, decisions, and diagnostics. Preserve exact bytes, default-empty and
explicit construction, equality/order, and the existing empty/all-whitespace
and 256-byte validation rules. Keep category independent from the key and
preserve issue/error behavior, collection order, and existing typed entity
IDs. F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1114 complete; Batch482 closed locally - 2026-10-20

F1114 completed at source/test commit `124d6b82`, following acceptance commit
`824ad280`. Generic import-review `ImportSourceKey` now types the match-index,
decision, and diagnostic keys while preserving exact bytes, default-empty,
equality/order, error behavior, category separation, and collection order.
The focused macOS build passed and
`ClassMngrNextApplicationImportReviewTests` passed 1/1. No full suite or
Windows run occurred. Batch482 was pushed without force to
`origin/Qt-Rewrite`; remote SHA `d973d82f3a93f4fde94b0a10fc3d5943439f591f`
matched local HEAD, and the worktree was clean and up to date. Batch483 is
active with F1115 next for acceptance review. F629 and the Phase 0 and Phase 2
gates remain open.

## Phase 2 update - F1115 accepted; Batch483 active - 2026-10-20

F1115 is accepted before implementation. Replace the Next Application
personal-signature font integers with a Qt-free closed choice type matching
the existing persisted values 0–3. Preserve the setting key, defaults,
invalid-value fallback to the first choice, exact save values, and existing
My Workspace and Initial Setup behavior. Keep the renderer enum, legacy
repository integer, and storage schema unchanged. F1116 and F1117 remain
discovered. F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1115 complete; Batch483 active - 2026-10-20

F1115 completed at source/test commit `a455d24c`, following acceptance commit
`e10d9e11`. The Qt-free `PersonalSignatureFontChoice` now types both Next
Application fields and maps the four existing values through preference and
save adapters. Three focused CTests passed 3/3, and the new Initial Setup
four-value selector passed. The full Initial Setup target aborted in its
existing keyboard-availability test with a Qt/macOS `NSBundle initWithURL:`
exception; no full suite or Windows run occurred. F1116 is next for acceptance,
F1117 remains discovered, and Batch484 discovery has started. F629 and the
Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1116 accepted; Batch483 active - 2026-10-20

F1116 is accepted before implementation. Replace the finite set of 14
Speaking Evaluation Application validation-code strings with a closed typed
code enum. Preserve each outward UI string key, issue ordering, shared length
code behavior, field/location/severity/arguments, and all validation results.
Cover every emitted code in the Application tests, including row and cell
overflow, and test the explicit UI adapter mapping and payload preservation.
F1115 is complete, F1117 remains discovered, and Batch484 discovery is
underway. F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1116 active; Batch484 discovery recorded - 2026-10-20

F1116 was accepted at `f744a03b`; production and focused test work are
underway. Two independent early-discovery scans identified ten provisional
Batch484 candidates, now recorded in the Phase 2 progress log. Batch484 stays
provisional until Batch483 closes. F1115 is complete, F1117 remains
discovered, and F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1116 complete; F1117 next - 2026-10-20

F1116 completed at source/test commit `edd09fdf`, following acceptance
`f744a03b`. The Application now uses a closed typed code for all 14 Speaking
Evaluation validation issues; the UI adapter preserves exact legacy keys and
payload order. Both focused targets built and both CTests passed 2/2. The
malformed-matrix use-case assertions preserve its existing `InvalidInput`
result while direct validator checks cover row and cell overflow. No full
suite or Windows run occurred. F1117 is next for acceptance. Batch484's ten
provisional candidates are recorded; F629 and the Phase 0 and Phase 2 gates
remain open.

## Phase 2 update - F1117 accepted; Batch483 active - 2026-10-20

F1117 is accepted before implementation: type the Calendar event query path
while preserving exact UTF-8 bytes through both query requests. F1115 and
F1116 are complete; Batch484's ten candidates remain provisional and
inactive. F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - Batch483 complete; Batch484 active - 2026-10-20

F1117 completed at source/test commit `98018322`, following acceptance
`3addf82e`. Both Calendar query request fields now use the Application
`CalendarEventDatabasePath` value, and focused tests preserve exact UTF-8
bytes through range and next-event requests. Both focused targets built and
both CTests passed 2/2. The test fixture now constructs the existing typed
series ID with `CalendarEventSeriesId::fromString`; no full suite or Windows
run occurred. Batch483 is complete and its closeout is ready to push.
Batch484 is active with ten discovered candidates; F1118 is next for
acceptance. F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - Batch483 pushed; Batch484 active - 2026-10-20

Batch483 closeout commit `0f47ad32` is pushed to `origin/Qt-Rewrite` and
verified at `0f47ad3241c727002c2cdb68a81c38145141dc03`; the worktree was
clean after the push. Batch484 is active with ten discovered candidates and
F1118 next for acceptance. F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1118 accepted; Batch484 active - 2026-10-20

F1118 is accepted to type only the generated repeat-series occurrence update
dates as `Domain::GregorianDate`, retaining typed valid dates through the
Application plan and converting to Qt at the platform adapter. Keep request
dates as raw strings for unchanged `InvalidInput` behavior and persisted
snapshot dates raw so invalid database dates still return `Technical` before
persistence. Preserve inclusive suffix selection, occurrence order, date
offset/duration, supported range, and projection behavior. Implementation and
tests have not started. F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1118 complete; Batch484 active - 2026-10-20

F1118 completed at source/test commit `f3f0b46e`, following acceptance
`3039bf25`. Planner-generated repeat-series update dates now remain
`GregorianDate` values through the Application contract and convert to `QDate`
in the platform adapter; request and snapshot input strings remain unchanged.
The Application calendar event and platform edit-port targets built, and both
focused CTests passed (2/2). The full suite, UI page parity target, and Windows
verification were not run. F1119 is next for acceptance; F629 and both phase
gates remain open.

## Phase 2 update - F1119 accepted; Batch484 active - 2026-10-20

F1119 is accepted with distinct typed representations for Schedule Import
correlation values: reuse the exact UTF-8 `ScheduleImportReviewTeacherKey`
for the three state-projection keys, and add an opaque UTF-16 apply-key value
for apply candidate, resolution, and target-issue keys. Preserve original
bytes/code units, state exact-match behavior, Hangul-only validation at its
current edges, conversion behavior, ordering, and diagnostic payloads. No
implementation or tests have started. F629 and the Phase 0 and Phase 2 gates
remain open.

## Phase 2 update - F1119 complete; Batch484 active - 2026-10-20

F1119 completed at source/test commit `4f19554d`, following acceptance
`011d807e`. State correlation records now use the existing exact UTF-8
`ScheduleImportReviewTeacherKey`; apply records use a separate opaque exact
UTF-16 `ScheduleImportApplyTeacherKey`. The focused state validation, plan
validation, review decision, apply projection/use-case, platform port, data
repository, dialog, and `ClassMngr` targets built; 9/9 selected CTests passed.
The full suite was not run. F1120 is next for acceptance; F629 and the Phase 0
and Phase 2 gates remain open.

## Phase 2 update - F1120 accepted; Batch484 active - 2026-10-20

F1120 is accepted to type the UTF-8 source-validation key fields in the
Teacher Import plan validator, use-case validation input, Korean and GS source
records, and review candidate lists with the existing opaque exact-byte
`ImportSourceKey`. Preserve empty/malformed values until existing validation,
normalization at adapter edges, exact per-index correspondence, source order,
duplicate rules, GS namespace separation, and error precedence. The native
English name-policy result remains a UTF-16 internal value outside scope. No
implementation or tests have started. F629 and both phase gates remain open.

## Phase 2 update - F1120 complete; Batch484 active - 2026-10-20

F1120 completed at source/test commit `1e9263a3`, following acceptance
`fd5f8d63`. Teacher Import's accepted UTF-8 source-validation fields now use
the existing exact-byte `ImportSourceKey`; adapter normalization, key/source
alignment, ordering, duplicate checks, and namespace behavior remain intact.
`ClassMngr` and the five focused Application/platform/parser/dialog targets
built; 6/6 selected CTests passed. The dialog ran offscreen, and its optional
external-workbook case was skipped because no sample path was set. The full
suite was not run. F1121 is next for acceptance; F629 and both phase gates
remain open.

## Phase 2 update - F1121 accepted; Batch484 active - 2026-10-20

F1121 is accepted to type only `CampusSummary::key` as an opaque,
byte-preserving `CampusDirectoryKey`, while allowing empty and arbitrary-byte
values to reach existing projection validation. Preserve its 64-byte limit,
bytewise blank check, exact uniqueness and lookups, projection order,
first-failure order, and existing `InvalidInput` behavior. Keep `code()` and
`shortCode()` as aliases for the same typed key. Add focused multibyte UTF-8
boundary coverage for 64 bytes accepted and 65 bytes rejected. Other campus
identifiers, metadata, adapters, and similarly named code fields remain out
of scope. Implementation has not started; the focused target is
`ClassMngrNextApplicationCampusDirectoryTests`. F629 and both phase gates
remain open.

## Phase 2 update - F1121 complete; Batch484 active - 2026-10-20

F1121 completed at source/test commit `dbef700b`, following acceptance
`57533c95`. `CampusSummary::key` now uses the opaque, byte-preserving
`CampusDirectoryKey`; current projection validation and exact comparison
behavior remain in place. The focused Application target built and its CTest
passed 1/1, including 64/65-byte UTF-8 boundaries and raw malformed-byte plus
embedded-NUL representation. Windows verification was unavailable and not
run; the full suite was not run. F1122 is next for acceptance. F629 and both
phase gates remain open.

## Phase 2 update - F1122 accepted; Batch484 active - 2026-10-20

F1122 is accepted to type the Teacher Profile validation issue code, field,
and argument key as distinct opaque byte-preserving values:
`TeacherProfileValidationCode`, `TeacherProfileValidationField`, and
`TeacherProfileValidationArgumentKey`. Preserve empty, unknown, duplicate,
and arbitrary-byte representations; issue and argument order; severity and
failure payload behavior; the eight-argument, 64-byte key, and 256 UTF-16
code-unit limits; and current Qt adapter conversion/mapping. Keep legacy
validation logic, profile IDs/fields, and persistence out of scope. Focused
Application, Teacher Info Page, and persistence parity targets are recorded
in the Phase 2 plan. Implementation has not started. F629 and both phase
gates remain open.

## Phase 2 update - F1122 complete; Batch484 active - 2026-10-20

F1122 completed at source/test commit `93f38196`, following acceptance
`4f9df35d`. Teacher Profile validation codes, field identifiers, and
argument keys now use distinct opaque byte-preserving values; current payload
ordering, adapter conversion, and argument limits remain intact. The focused
Application, Teacher Info Page, and persistence parity targets built and
their CTests passed 3/3. Windows and full-suite verification were not run.
F1123 is next for acceptance. F629 and both phase gates remain open.

## Phase 2 update - F1123 accepted; Batch484 active - 2026-10-20

F1123 is accepted to type only the Document Catalog metadata path fields:
folder `path`, folder `parentPath`, and document `path`, using one opaque
byte-preserving `DocumentCatalogPath`. Preserve unvalidated/un-normalized
construction, existing blankness and 4,096-byte checks, empty root parent
paths, exact uniqueness and parent matching, missing-parent/cycle behavior,
validation order, and vector order. Keep filesystem parsing and normalization,
document-content references, and resource paths outside scope. Focused
Application, platform port, sidebar, and navigation parity targets are
recorded in the Phase 2 plan. Implementation has not started. F629 and both
phase gates remain open.

## Phase 2 update - F1123 complete; Batch484 active - 2026-10-20

F1123 completed at source/test commit `e21fb087`, following acceptance
`6f147f8d`. `DocumentCatalogPath` now types the three accepted catalog
metadata path fields, with projection validation and exact hierarchy behavior
preserved. The Application, platform port, sidebar, and navigation parity
targets built; focused CTest passed 4/4. Windows and full-suite verification
were not run. F1124 is next for acceptance. F629 and both phase gates remain
open.

## Phase 2 update - F1124 accepted; Batch484 active - 2026-10-20

F1124 is accepted to replace only `RosterSaveIssue.code` with a separate
closed `RosterSaveIssueCode` enum covering the 15 current codes, with an
explicit mapping to their unchanged dotted keys. Preserve issue order,
severity, serialized error text, and the UI `.required` suffix and `duplicate`
substring behavior. Type-check the Korean length-code comparisons in the
Application use cases. Keep this enum separate from F1116's Speaking
Evaluation codes and leave other issue fields, arguments, IDs, and generic Qt
validation codes unchanged. Focused Application, platform, model, editor,
and row-transfer targets are recorded in the Phase 2 plan. Implementation
has not started. F629 and both phase gates remain open.

## Phase 2 update - F1124 complete; Batch484 active - 2026-10-20

F1124 completed at source/test commit `812692c6`, following acceptance
`241af2eb`. Roster Save codes now use a separate closed enum with explicit
mappings to the same 15 dotted keys; issue order, severity, UI matching, and
error serialization remain intact. The Application, platform, model, editor,
and row-transfer targets built; focused CTest passed 5/5. The Application
target was rerun after a test-only cleanup and passed 1/1. Windows and
full-suite verification were not run. F1125 is next for acceptance. F629 and
both phase gates remain open.

## Phase 2 update - F1125 accepted; Batch484 active - 2026-10-20

F1125 is accepted to type only `UpcomingBirthdayCandidate::birthdayMonthDay`
as a permissive opaque `UpcomingBirthdayMonthDayInput` over its current UTF-8
string representation. Invalid or blank text must remain representable so
the existing parser continues to skip it. Preserve ASCII `MM-dd` parsing and
trimming, valid `BirthdayMonthDay` behavior, leap-day fallback, occurrence
window/bucketing, and ordering. Keep `UpcomingBirthdayDate`, invalid-reference
empty-schedule behavior, directory snapshot/read-port strings, and Qt
trimming unchanged. Focused Application and feature targets are recorded in
the Phase 2 plan. Implementation has not started. F629 and both phase gates
remain open.

## Phase 2 update - F1125 complete; Batch484 active - 2026-10-20

F1125 completed at source/test commit `7775df86`, following acceptance
`5c427479`. `UpcomingBirthdayCandidate::birthdayMonthDay` now uses permissive
opaque `UpcomingBirthdayMonthDayInput`; empty and malformed bytes remain
representable and the existing parser skips them. Parsing, trimming,
post-parse `BirthdayMonthDay`, leap fallback, schedule window, buckets, and
ordering remain unchanged. Both focused build targets passed; the three
focused CTests passed 3/3, as did direct selectors for invalid skipping,
trimming, leap fallback, invalid reference dates, buckets, year rollover,
range cap, and feature mapping. Windows and the full suite were not run. F1126
is next for acceptance. F629 and both phase gates remain open.

## Phase 2 update - F1126 accepted; Batch484 active - 2026-10-20

F1126 is accepted to add one closed `ExcelImportTimeoutChoice` for 30, 60,
120, and 300 seconds, with explicit conversions to/from integer seconds. Use
it in `ExcelImportTimeoutPreferences` and in `UserPreferencesSnapshot` while
preserving the legacy numeric getters and validated integer setter. Keep the
120-second default/fallback for unsupported stored values; reject invalid
Application inputs without changing state; preserve platform write errors,
synchronized-write rollback, the integer QSettings key
`imports/excelTimeoutSeconds`, and menu save-failure recovery. UI timer
conversion and other preference settings stay unchanged. Focused Application,
platform port, menu, and legacy settings targets are recorded in the Phase 2
plan. Implementation has not started. F629 and both phase gates remain open.

## Phase 2 update - F1126 complete; Batch484 active - 2026-10-20

F1126 completed at source/test commit `a87bb612`, following acceptance
`a281a774`. Closed `ExcelImportTimeoutChoice` now types
`ExcelImportTimeoutPreferences` and stored user-preference state. Explicit
conversion keeps the 30/60/120/300-second values at integer settings and UI
boundaries; invalid integer inputs, unknown enum writes, 120-second fallback,
the existing setting key, and save-failure behavior remain intact. `ClassMngr`
and all four focused test targets built; focused CTest passed 4/4, including
invalid combo item-data restoration/warning and no-write behavior. The import
dialog timer call sites were compiled and still multiply the selected seconds
by 1,000. Windows and full-suite verification were not run. F1127 is next for
acceptance. F629 and both phase gates remain open.

## Phase 2 update - F1127 accepted; Batch484 active - 2026-10-20

F1127 is accepted to type only `platformKey` in the Application startup and
manual update artifacts as permissive opaque UTF-8 `UpdateArtifactPlatformKey`.
Keep bytes and exact case unchanged; unknown keys and architecture-derived
strings remain representable so existing asset matching and failures continue.
Wrap at the two platform service ports and convert to QString at the existing
controller/dialog boundary via `.value()`. Keep OS/architecture detection,
archive matching, downloader JSON persistence, and other artifact fields
unchanged. Focused Application and platform mapping targets are recorded in
the Phase 2 plan. Implementation has not started. F629 and both phase gates
remain open.

## Phase 2 update - Batch484 complete; Batch485 active - 2026-10-20

F1127 completed at source/test commit `6021d04a`, following acceptance
`a080d89a`. `UpdateArtifactPlatformKey` now types only the Application startup
and manual artifact keys, retaining exact bytes, case, aliases, and unknown
values through existing adapters. The `ClassMngr` app build and startup/manual
Application and controller tests passed; focused CTest passed 3/3. The
`ClassMngrUpdaterTests` target built and passed 1/1 after updating its stale
typed-key fixture. Controller tests pin exact case, unknown-key errors, and
Linux aliases. Windows and the full suite were not run. Batch484 is complete;
Batch485 has ten discovered candidates F1128–F1137, with F1128 next for
acceptance. F629 and both phase gates remain open.

## Phase 2 update - F1128 complete - 2026-10-20

F1128 completed at source/test commit `63fff147`, following acceptance
`2a95f68e`. `CourseGradeBand` now types the Speaking Evaluation report result
and AI batch eligibility input. Existing permissive report-grade parsing,
trimmed-original template selection, legacy integer mapping, prompt wording,
and eligibility failure order remain intact. `Other` and non-elementary bands
remain unsupported for AI eligibility; the report adapter maps them to legacy
sentinel 0.

In the existing macOS arm64 Debug preset tree, a clean reconfiguration using
Qt 6.12.0 restored the `ClassMngr` link after the generated runtime archive
proved inconsistent. `ClassMngr` and four affected test targets built. The
three focused Application CTests passed 3/3; report-projection adapter CTest
passed 1/1; both offscreen UI selectors passed, 3/0 each. `git diff --check`
passed. Windows and the full suite were not run. Batch485 has one complete
slice and nine discovered; F1129 is next for acceptance. F629 and both phase
gates remain open.

## Phase 2 update - F1129 accepted; Batch485 active - 2026-10-20

F1129 is accepted/in progress. Add permissive Application value
`ClassDetailsValidationContextTeacherId` around the raw `std::int64_t` used by
the Class Details validation context and snapshot, preserving `-1` fallback,
explicit `-2`, and `2147483648`. Keep the later `TeacherId` conversion and
downstream validation unchanged. Implementation and tests have not started.
Batch485 has one complete slice, one accepted/in progress, and eight discovered;
F1130 is next. F629 and both phase gates remain open.

## Phase 2 update - F1129 complete - 2026-10-20

F1129 completed at source/test commit `596ed1f6`, following acceptance
`53115d66`. The new permissive `ClassDetailsValidationContextTeacherId`
types both context identity fields while preserving raw `-1`, `-2`, and wide
int64 values. The platform adapter wraps `qint64`; the save workflow unwraps
at the existing `TeacherId` conversion. `ClassMngr` and all three focused
targets built; the Application query, platform adapter, and save-use-case
CTests passed 3/3. Windows and full-suite verification were not run. Batch485
has two complete slices and eight discovered; F1130 is next for acceptance.
F629 and both phase gates remain open.

## Phase 2 update - F1130 accepted; Batch485 active - 2026-10-20

F1130 is accepted for a dedicated Application `ScheduleSlotStartMinute`
wrapper on `ScheduleSlotStateSaveRequest::startMinute`. Keep construction
permissive so the existing request validator retains the `[0,1440)` check,
weekday/minute/state failure order, exact error, and no-port-call behavior.
Adapt the strict UI time parser and platform `HH:mm` formatter at their
existing boundaries. `Domain::ScheduleTime` is unsuitable because it models
a weekday interval with start and end. Implementation and verification are
pending; F1131 is next for acceptance. Batch485 remains unpushed. F629 and
both phase gates remain open.

## Phase 2 update - F1130 complete - 2026-10-20

F1130 completed at source/test commit
`08d614eed4e385e9a2f2007d115b04bd1f3d2262`, following acceptance
`849dd0fb`. `ScheduleSlotStartMinute` now types the Application save request;
the existing validator keeps its weekday/minute/state order, `[0,1440)`
bounds, exact error, and no-port-call behavior. The UI parser retains its
strict input rules, and the Platform adapter preserves zero-padded `HH:mm`.
`ClassMngr` and both focused test targets built; both focused CTests passed
2/2. Coverage includes typed value/equality/default, 0 and 1439, invalid -1
and 1440, validation order, and persisted `00:00`, `09:05`, and `23:59`.
`git diff --check` passed. Windows and full-suite verification were not run.
Batch485 has three complete slices and seven discovered; F1131 is next for
acceptance. F629 and both phase gates remain open.

## Phase 2 update - F1131 accepted; Batch485 active - 2026-10-20

F1131 is accepted for a permissive Application `SubPrepMeetingSecondsOfDay`
wrapper backed by `std::int32_t`, used on both meeting facts and grouping
results. Keep invalid raw values representable so the grouping policy retains
its current skip behavior; preserve exact-second grouping/order, selected-day
filtering, canonical weekday/group ordering, and cross-day deduplication. The
Platform adapter continues compact labels that omit seconds, while the
separate navigation path continues minute-based ordering. Implementation and
verification are pending; F1132 is next for acceptance. Batch485 remains
unpushed. F629 and both phase gates remain open.

## Phase 2 update - F1131 complete - 2026-10-20

F1131 completed at source/test commit
`4976bc2e121fd70ede48e24424dac1e3b2f68f50`, following acceptance
`1b2e1d99`. The permissive `SubPrepMeetingSecondsOfDay` now types both
Application meeting facts and grouping results. Policy-level invalid skipping,
exact-second grouping/order, selected-day filtering, cross-day grouping,
weekday de-duplication, and canonical ordering remain intact. The Platform
adapter unwraps only for `QTime` conversion; compact labels still omit seconds,
and navigation remains minute-based. `ClassMngr` and both focused test targets
built; both focused CTests passed 2/2. Coverage includes 0/86399 retention,
-1/86400 skipping, exact-second ordering, compact `Mon 9:30am`, and navigation
order 570. `git diff --check` passed. Windows and full-suite verification were
not run. Batch485 has four complete slices and six discovered; F1132 is next
for acceptance. F629 and both phase gates remain open.

## Phase 2 update - F1132 accepted; Batch485 active - 2026-10-20

F1132 is accepted to reuse `Domain::GregorianDate` for typed latest-date
results and persistence snapshots. Keep the read-port string so the query
still rejects malformed present port output; the real adapter's missing or
malformed stored values remain absent. Type the use-case snapshot date, but
retain raw `sourceDateIso` and `sourceDateValid` at the request boundary so
invalid-request behavior and precedence remain unchanged. Compare validated
dates by day number for save-only-if-later, and convert to/from Qt date values
only at existing UI/Platform edges. Implementation and verification are
pending; F1133 is next for acceptance. Batch485 remains unpushed. F629 and both
phase gates remain open.

## Phase 2 update - F1132 complete - 2026-10-20

F1132 completed at source/test commit
`ab07b25a93358448408c0cac4c39fd76ef23279a`, following acceptance `32a23569`.
The latest-date query now returns `optional<GregorianDate>`, and the
transactional snapshot stores a typed date. The raw read port still reports
malformed present values through the existing Validation error, and the import
request retains raw ISO text and its supplied validation flag. Save-only-if-
later compares typed day numbers; the existing settings key, canonical ISO
write, transaction order, and UI prompt behavior remain intact.

`ClassMngr` and the four focused targets built in the existing macOS build
tree; the four focused CTests passed 4/4. Coverage includes typed query output,
exact malformed-port error, invalid requests before persistence, malformed
settings as absence, and equal/older/newer date behavior. `git diff --check`
passed. The full suite and Windows verification were not run. Batch485 has
five complete slices and five discovered; F1133 is next for acceptance. The
batch remains unpushed. F629 and both phase gates remain open.

## Phase 2 update - F1133 accepted; Batch485 active - 2026-10-20

F1133 is accepted for a permissive Application `DirectoryPreferencePath` used
by both remembered-directory ports. It owns the exact UTF-8 bytes and retains
an empty default; construction must not trim, validate, or normalize paths.
Keep conversions in the Platform adapters and preserve the existing empty
fallback, canonicalization, and absolute-path rules in `QtFileDialogService`
and `FileController`. Do not repeat or alter F688/F689 checked persistence,
keys, rollback, or retry behavior. Implementation and verification are
pending; F1134 is next for acceptance. Batch485 remains unpushed. F629 and both
phase gates remain open.

## Phase 2 update - F1133 complete - 2026-10-20

F1133 completed at source/test commit `7800f471`, following acceptance
`693f9494`. `DirectoryPreferencePath` now carries the exact UTF-8 bytes for
both remembered-directory ports, including the empty default; Qt conversion
remains at the Platform and UI boundaries. Existing fallback, path
canonicalization, checked persistence, rollback, and retry behavior remain
covered and unchanged.

`ClassMngr` and all four focused targets built in the existing macOS debug
tree; the four focused CTests passed 4/4. Coverage includes default empty and
exact unnormalized UTF-8 values, all eight dialog preference keys, fallback
and precedence, canonical path handling, persistence failure rollback/retry,
and FileController startup/fallback behavior. `git diff --check` passed.
The full suite and Windows verification were not run. Batch485 has six
complete slices and four discovered; F1134 is next for acceptance. The batch
remains unpushed. F629 and both phase gates remain open.

## Phase 2 update - F1134 accepted; Batch485 active - 2026-10-20

F1134 is accepted for dedicated export-only UTF-16 class and teacher
correlation-key types used in the portable package. Keep them distinct from
the UTF-16 payload type and from existing UTF-8 Class Transfer apply keys.
Preserve exact code units, generated `class-N` selection order, first-seen
teacher-ID `teacher-N` reuse and references, source identities, JSON schema,
and legacy package mapping. Do not normalize, validate, trim, or transcode
keys. Focused query, platform export-source, and legacy package adapter tests
are expected; implementation and verification are pending. F1135 is next for
acceptance. Batch485 remains unpushed; F629 and both phase gates remain open.

## Phase 2 update - F1134 complete - 2026-10-20

F1134 completed at source/test commit
`d0d51ffcda20b55340acd2792a040bc929787083`, following acceptance `edc9bb9b`.
The export package now uses exact-value UTF-16 class and teacher key types,
separate from payload text and UTF-8 apply keys. Class-key selection order,
typed teacher-identity reuse, references, JSON fields, legacy conversion, and
apply semantics remain unchanged.

The existing macOS Debug tree built `ClassMngr` and the three focused targets
for export query, platform export source, and legacy package adapter. Their
three focused CTests passed 3/3. Coverage includes type separation, exact
UTF-16 code units, generated key order/reuse, Unicode source data, and existing
JSON/legacy mappings. `git diff --check` passed. The full suite and Windows
verification were not run. Batch485 has seven complete slices and three
discovered; F1135 is next for acceptance. The batch remains unpushed. F629 and
both phase gates remain open.

## Phase 2 update - F1135 accepted; Batch485 active - 2026-10-20

F1135 is accepted for `ClassAnalyticsAverageThousandths`, an Application value
backed by integer thousandths, with a zero default, an explicit
thousandths construction, accessors, equality, and no validation or clamping
in the value type. Use it for rounded criterion, student, class, and year-to-
date average outputs. Preserve the existing raw calculations and weighting,
positive half-up rounding, the 400-thousandths grade threshold, rank tie-breaks,
criterion extrema ties/order, locale formatting, and grade/chart clamps. The
focused query, ranking, extrema, and UI analytics tests are expected;
implementation and verification are pending. F1136 is next for acceptance.
Batch485 remains unpushed; F629 and both phase gates remain open.

## Phase 2 update - F1135 complete - 2026-10-20

F1135 completed at source/test commit
`f88316206a36891e911ede1ff403ec79a6817a13`, following acceptance
`305c8c413cfd7f55191d748734f93e486728d28f`. All four Application average
outputs now use `ClassAnalyticsAverageThousandths`; raw score accumulation and
per-student rounding remain unchanged. Class and YTD means of rounded student
values now use integer thousandths with positive half-up rounding, removing a
floating representation error at the 2399/2400 midpoint without changing
weighting or eligibility.

The macOS Debug tree built `ClassMngr` and five focused targets for dashboard
query, ranking order, criterion extrema, SpeakingAnalytics, and page parity.
All five focused CTests passed 5/5. Coverage includes type and scale, partial
scoring and YTD eligibility, half-up grade boundaries, rank and extrema ties,
and German-locale one-decimal display. `git diff --check` passed. The full
suite and Windows verification were not run. Batch485 has eight complete
slices and two discovered; F1136 is next for acceptance. The batch remains
unpushed. F629 and both phase gates remain open.

## Phase 2 update - F1136 accepted; Batch485 active - 2026-10-20

F1136 is accepted for `SkippedUpdateVersionText`, an exact-byte Application
value with empty default, explicit construction, `.value()`, `.empty()`, and
equality. Use `optional<SkippedUpdateVersionText>` in the existing preferences
contract and the policy's stored-text input. The value does not parse, trim,
validate, canonicalize, or normalize text. Preserve the Platform adapter's
existing trimming and blank-to-missing behavior.

Keep numeric parsing and raw-text equality as separate policy checks:
`Version::parse` may accept leading zeros, but suppression still requires a
literal match, so `01.2.3` does not suppress latest `1.2.3`. Preserve missing,
invalid, stale-clear, exact-match, and prompt-suppression outcomes, the separate
controller suppression flag, write/clear failure recovery, and canonical text
written by a user Skip action. F1127 `UpdateArtifactPlatformKey` remains
unrelated. Focused policy, platform port, and automatic-startup controller
tests are expected; implementation and verification are pending. F1137 is
next for acceptance. Batch485 remains unpushed; F629 and both phase gates
remain open.

## Phase 2 update - F1136 complete - 2026-10-20

F1136 completed at source/test commit
`98ed18944ad2f3f82b7c83c03c5f48f475129efb`, following acceptance
`6b456bff16c940d924f7baea94dea591d8c49347`. `SkippedUpdateVersionText` now
types exact stored version text in the Application preferences and policy
contracts. The settings adapter still trims and maps blank text to missing;
numeric version parsing remains separate from literal suppression comparison.
Controller clear/write recovery, retry, canonical Skip writes, and prompt
behavior remain preserved.

`ClassMngr` and the three focused macOS targets built; the Application policy,
Platform preferences-port, and automatic-startup controller CTests passed 3/3.
The first sandboxed controller test could not bind its local HTTP server; the
same requested CTest command passed outside the sandbox. `git diff --check`
passed. Full-suite and Windows verification were not run. Batch485 has nine
complete slices and one discovered candidate, with F1137 next for acceptance.
The batch remains unpushed; F629 and both phase gates remain open.

## Phase 2 update - F1137 accepted; Batch485 active - 2026-10-20

F1137 is accepted to add a closed Domain `CourseCatalogIdentity` for the 25
official grade/level pairs and store that typed identity in `Course`. Keep
`CourseGradeBand` separate as a grade classification, including `Other`.
Preserve exact, unnormalized `fromNames()` matching, canonical grade and level
text and ordering, distinct identities for repeated level names in different
grades, equality, and every existing weekly meeting-day rule. Keep Schedule
Import's raw string candidate boundary and validation/error behavior intact.
Focused Domain contract and Schedule Import plan-validation tests will verify
these boundaries. Implementation and verification are pending. Batch485 has
nine complete slices and one accepted/in progress; Batch486's six-candidate
discovery is provisional. The batch remains unpushed; F629 and both phase gates
remain open.

## Phase 2 update - F1137 complete; Batch486 active - 2026-10-20

F1137 completed Batch485 at source/test commit
`dcdc9be831b7c4b19e9c6fcbfea0d501c0ab91d4`, following acceptance
`aca313bd7973fcfa1bc8fd392eca33e5461d81e0`. `Course` now stores and exposes
the closed `CourseCatalogIdentity` for all 25 official grade/level pairs.
Exact matching, canonical names/order, equality, same-level distinctions across
grades, weekly-day rules, and Schedule Import's raw-string validation remain.

The two focused macOS arm64 Qt 6.12 Debug CTests passed 2/2 for Domain
contracts and Schedule Import plan validation; the Domain target reported 38
cases. `git diff --check` passed. The full suite and Windows verification were
not run. Batch485 was pushed to `origin/Qt-Rewrite` at
`192f676ae6736553c6fd4bed4788b71fdc5e5d96`; Batch486 is active with accepted
F1138 and five discovered candidates. F629 and both phase gates remain open.

## Phase 2 update - F1138 accepted; Batch486 active - 2026-10-20

F1138 is accepted to type the Application calendar color-preference port's
event identity as existing Domain `CalendarEventType`, leaving color text
opaque. Preserve Qt normalization at the UI boundary (trim, exact six labels,
unknown to `Other`) and convert normalized names to the enum there. Platform
must map enum values explicitly to the existing six canonical setting-key
suffixes; do not use enum ordinals. An invalid enum value created by cast also
maps to the `Other` key so the adapter never constructs an arbitrary key.
Preserve the key prefix, missing/unavailable read as empty, invalid stored-color
pass-through, UI `QColor` fallback, invalid-picker no-save, write error
handling, and warning/retry behavior.

Focused verification is planned for the Domain contract, Platform preferences
port including all six exact keys, and the existing Calendar Page color picker
failure/resync/retry case, with `ClassMngr` and affected targets built. Full
suite and Windows verification are not expected. Implementation and
verification are pending. Batch486 has six candidates with F1138 accepted and
five discovered; F629 and both phase gates remain open.

## Phase 2 update - F1138 complete; Batch486 active - 2026-10-20

F1138 completed at source/test commit
`fd3c0462b2843b9ed7633f9e6e24fcff919dec57`. The Application color-preference
port now takes `Domain::CalendarEventType`; the Platform adapter maps the six
typed values to their unchanged canonical setting-key suffixes and maps an
invalid enum cast to `Other`. UI normalization and opaque color text remain at
their prior boundaries, as do missing/unavailable reads, invalid stored-color
pass-through, QColor fallback, picker cancellation, save warnings, and retry
behavior.

The macOS Debug tree built `ClassMngr` and the three affected targets. The
focused Domain and Platform CTests passed 2/2, and the selected Calendar Page
color-picker failure/resync/retry QtTest case passed. Coverage includes all six
exact setting keys, invalid-enum fallback, UI normalization, and the existing
save failure flow. `git diff --check` passed. The full suite and Windows
verification were not run. Batch486 has one complete slice and five discovered;
F1139 is next. F629 and both phase gates remain open.

## Phase 2 update - F1139 accepted; Batch486 active - 2026-10-20

F1139 is accepted to replace the calendar-import campus-code query's raw
`std::string` elements with a distinct owning Application value carrying the
same UTF-8 bytes. Do not reuse `Application::CalendarEventCampusCode`: that
visibility-policy input carries both normalized and case-folded forms, while
this importer needs only trimmed code text. Preserve repository order,
Platform-side Qt trimming, blank filtering, UTF-8 conversion, and first-seen
case-sensitive duplicate removal. Preserve the query's empty-list behavior for
missing/unavailable/malformed/default campus records, the service's UTF-8 to
QString conversion, and parser normalization, matching, and suffix behavior.

Focused verification will build `ClassMngr`,
`NextPlatformCalendarEventImportCampusCodeQuery`, and
`ClassMngrCalendarImportTests`; run the two focused CTests and
`git diff --check`. Full-suite and Windows verification are not expected.
Batch486 has one complete slice, one accepted/in-progress F1139, and four
discovered candidates; F1140 is next for acceptance. F629 and both phase gates
remain open.

## Phase 2 update - F1139 complete; Batch486 active - 2026-10-20

F1139 completed at source/test commit
`05c9de8cccc4929a90b72362d6a3112f7e1f8a00`. The calendar-import query now
returns an owning Application campus-code value containing the same UTF-8
bytes. The Platform adapter retains repository order, trimming, blank
filtering, exact case-sensitive duplicate removal and missing/malformed/default
record behavior. The service still converts the bytes to `QString` before
calling the unchanged parser.

The macOS Debug build of `ClassMngr`,
`ClassMngrNextPlatformCalendarEventImportCampusCodeQueryTests`, and
`ClassMngrCalendarImportTests` passed. The two focused CTests passed 2/2, and
`git diff --check` passed. Full-suite and Windows verification were not run.
Batch486 now has two complete slices and four discovered; F1140 is next for
acceptance. F629 and both phase gates remain open.

## Phase 2 update - F1140 accepted; Batch486 active - 2026-10-20

F1140 is accepted to introduce a Qt-free Domain value for prepared personal-
signature image bytes, preserving the exact byte sequence with no decoding,
validation, or normalization in the value. Keep the existing result wrapper
so empty bytes remain a successful absence and a repository read failure
remains a nonrecoverable `Technical` error. Base64 decoding and image
preparation stay in Platform, and the exact settings key and successful empty
behavior for missing, unavailable, closed, invalid, corrupt, or unsupported
images remain. Preserve Initial Setup's save-time retry/blocking behavior, My
Workspace's image preservation and save blocking on read errors, and Speaking
Evaluation's no-signature fallback. The separate personal-details write
request remains outside this slice.

Focused verification will build `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesPersonalSignatureImagePortTests`,
`ClassMngrInitialSetupWizardTests`, and `ClassMngrMyWorkspacePageTests`. Run the
focused adapter CTest and the two selected UI read-error cases, then
`git diff --check`. Full-suite and Windows verification are not expected.
Batch486 has two complete slices, one accepted/in-progress F1140, and three
discovered candidates; F1141 is next for acceptance. F629 and both phase gates
remain open.

## Phase 2 update - F1140 complete; Batch486 active - 2026-10-20

F1140 completed at source/test commit
`230f7a0052679864856eaff4b2eea47b4a5554e3`. A Qt-free Domain
`PreparedPersonalSignatureImageBytes` now owns the exact prepared bytes; the
read result retains `Domain::Result` so an empty value remains successful and
repository read failures remain nonrecoverable `Technical` errors. Base64 and
image preparation remain in Platform, and all callers retain their existing
retry, preserve, and no-signature behavior. The separate write-side field was
not changed.

The macOS Debug build passed for `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesPersonalSignatureImagePortTests`,
`ClassMngrInitialSetupWizardTests`, and `ClassMngrMyWorkspacePageTests`. The
focused Platform CTest passed 1/1, and the two selected UI read-error cases
passed. `git diff --check` passed. Full-suite and Windows verification were not
run. Batch486 has three complete slices and three discovered; F1141 is next for
acceptance. F629 and both phase gates remain open.

## Phase 2 update - F1141 complete; Batch486 active - 2026-10-20

F1141 is accepted to distinguish the Sub Prep Zoom login ID and password with
two Qt-free owning Application values. Preserve exact permissive UTF-8 bytes,
including spaces and embedded NULs; apply no validation, trimming, or case
folding in the values, and treat `N/A` as ordinary text. Keep the existing
defaults (`N/A`, `N/A`, and `unavailable=true`) and boolean availability field.

Platform retains primary and legacy keys, valid-primary precedence, fallback
only for missing or invalid primary values, length-aware UTF-8 conversion, and
best-effort migration only after all three settings read successfully. Ignore
migration-write failures as before; repository read failures remain
nonrecoverable `Technical` errors and do not trigger fallback or migration.
Sub Prep and My Info continue to decode by explicit length and trim only for
presentation; Sub Prep hides values when unavailable, while My Info retains
them and disables the fields. Initial Setup forwards the exact payloads to its
existing save request. Preserve read-error warning and field-retention behavior.

The incremental build in the existing documented `macos-clang-debug` tree
passed for `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests`,
`ClassMngrSubPrepPageTests`, `ClassMngrMyWorkspacePageTests`, and
`ClassMngrInitialSetupWizardTests`. Platform CTest passed 1/1 and the direct
Platform run passed all 11 entries. Selected cases
`zoomUnavailableHidesStoredCredentials`,
`zoomReadFailurePreservesReadOnlyFieldsAndReportsDiagnostic`,
and `storedZoomCredentialsPrefillAndRespectUnavailableState` passed; the latter
asserted exact UTF-8 display text with surrounding spaces and unavailable
field behavior. `aggregateSaveComposesTypedReadsWithoutDataLoss` could not
reach its assertions: the selected launch and registered offscreen CTest abort
in Qt Widgets / QWizard with `NSInvalidArgumentException` from
`NSBundle initWithURL: nil URL argument`, also in the documented preset tree.
The cause is unresolved and no behavior conclusion is available for this case.
`git diff --check` passed. Full suite and Windows were not run. Batch486 has
four complete slices and two discovered candidates; F1142 is next. F629 and
both phase gates remain open. Batch487 is discovered with ten ordered
candidates (F1144–F1153).

## Phase 2 update - F1142 complete; Batch486 active - 2026-10-20

F1142 is accepted to add a Qt-free Application `CustomColorPaletteEntry` that
owns exact UTF-8 text, with no validation, trimming, or canonicalization in the
value. Store all 16 entries in the existing palette array and preserve the 16
defaults and equality. The Platform adapter continues to own legacy decoding,
read-time trim/validation/HexRgb canonicalization, invalid fallback, padding,
truncation, and write serialization. Writes preserve exactly all 16 supplied
strings in compact JSON under `custom_colors`; do not normalize caller input.
Keep current unavailable/missing/error and UI read/write behavior.

F1142 completed at source/test commit
`67f7277a20d5950ad85f9e6fc655f80f118bb2d2`, following acceptance
`977ca6c4f6f6aeffb39e872b3f775afa1fd3ce66`. A focused incremental build in
the existing `macos-clang-debug` tree passed for `ClassMngr`, the Platform
custom-color-palette preferences port tests, and the ColorUtils palette tests.
Both focused CTests passed (Platform 12/12 QtTest entries; ColorUtils 11/11).
The Platform regression stores `  #abcdef  ` exactly in compact JSON, then
confirms that the next read applies existing normalization. `git diff --check`
passed. Full-suite and Windows verification were not run. Batch486 has five
complete slices and one discovered candidate; F1143 is next. Batch487 has ten
discovered candidates and is not active. F629 and both phase gates remain open.

## Phase 2 update - F1143 complete; Batch486 closed locally - 2026-10-20

F1143 added Qt-free Application `AcademicCalendarScheduleJsonText` for exact
JSON text. The value owns its text and does not parse, validate, trim, or
canonicalize it; Platform retains the existing settings key and length-aware
UTF-8 conversions, while `AcademicCalendarProvider` retains compact
serialization and parsing. Successful empty reads, invalid/schema fallback,
Technical read errors and provider state retention, and write-error
propagation and provider rollback remain covered. The Platform regression
round-trips Korean valid UTF-8 JSON text and confirms its stored UTF-8 bytes.

The incremental build in the existing `macos-clang-debug` tree passed for
`ClassMngr` and four affected targets; no configure or clean was run. All nine
focused registered CTests passed, including the Calendar Preferences parent
target and its two selectors; `git diff --check` passed. The first build
attempt caught two test-edit errors, which were corrected before the passing
retry. Full-suite and Windows verification were not run. F1143 completed at
source/test commit `686bdc2a5000709c6b5ab6cd6b632552d62577d6`, following
acceptance `c6b6eadb8a2c6901e85bc26961b8b1332c02cbb6`. Batch486 has six
complete candidates and is closed locally; Batch487 is active with ten
discovered candidates, and F1144 is next for acceptance. Its closure commit is
awaiting push. F629 and both phase gates remain open.

## Phase 2 update - F1152 complete; Batch487 active - 2026-10-20

F1152 completed at source/test commit
`d2ed97f8547ba1d67cda65361d6a4443531b9d47`, following acceptance commit
`69755eb15f6e0bc9e919d1f2dd73b2ea6db707cd`. Class Analytics now represents
evaluation selection as an explicit All or owning exact-name Application
value. The UI uses combo item data, and the query keeps existing trimmed,
case-sensitive matching, unknown-name empty snapshots, aggregate order, and
global year-to-date behavior.

The incremental macOS Debug build passed for `ClassMngr` and the Application,
Platform, and Class Analytics Page parity targets. Application and Platform
CTests passed 2/2; the UI selector passed under Qt offscreen after the native
launch could not find a screen. `git diff --check` passed. No full, clean, or
Windows build ran. Batch487 has nine complete slices and F1153 accepted/in
progress; Batch488 is discovered but inactive. F629 and both phase gates
remain open.

## Phase 2 update - Batch487 complete locally; Batch488 active - 2026-10-20

F1153 completed at source/test commit
`f95e04ec0e51d9922dcf1d7488ab3a62d8e54bdc`, following acceptance commit
`07ecc351012bf94ee8f0911492e350884d1dc692`. Class Analytics now uses
`Domain::SpeakingEvaluationGrade` for pure grade-valued Application outputs;
the feature adapter converts them back to the existing QString DTOs. Optional
snapshot and criterion grades retain blank empty/partial states. Raw score
text, composite criterion labels, numeric distributions, and calculations
remain unchanged. The unknown numeric label fallback to C and
`numberToGrade()` clamp behavior are preserved; unknown raw score text remains
skipped.

In the existing `build/macos-clang-debug` tree, the incremental build passed
for `ClassMngr`, the Application Class Analytics query tests, and the Class
Analytics Page parity tests. Application CTest passed 1/1, and the visible
summary/YTD plus no-scored-data UI selectors passed under Qt offscreen. Qt
reported font-alias and offscreen size-hint warnings without failing either
selector. `git diff --check` passed. No full suite or Windows build ran.

Batch487 is complete locally; its push follows the closeout commit, with no
push receipt yet. Batch488 is now active with eight candidates and F1154 next.
F629 and both phase gates remain open.

## Phase 2 update - F1169 complete; Batch489 active - 2026-10-20

F1169 completed at source/test commit `63ec8f89`, following acceptance commit
`2139a070`. Sub Prep navigation facts now use `Domain::Weekday` and an optional
typed start minute; the reducer preserves the 1000 no-time sentinel while
correctly retaining valid minutes through 1439. Unknown/noncanonical weekday
filtering, invalid-time absence, second truncation, and presentation remain
unchanged.

The incremental macOS Debug build passed for `ClassMngr` and both focused test
targets. The focused binaries passed, including 40 Platform test entries, and
both matching CTests passed 2/2 offscreen. `git diff --check` passed. No full
suite or Windows build ran. Batch489 has eight complete slices and F1170 next;
the batch remains unpushed until F1170 closes. Batch490 is discovered and
inactive until Batch489 closes. F629 and both phase gates remain open.

## Phase 2 update - F1170 accepted; Batch489 active - 2026-10-20

F1170 is accepted as the final Batch489 slice. It will replace only the
Sub Prep print class-order weekday and start-minute integers with distinct
non-converting Application value types. Each retains the exact integer,
including the max-int unknown sentinel, and the policy compares those values
in their existing order. An unknown weekday with a valid minute therefore
continues to sort before a class with no valid meeting.

Preserve the producer's visible-day filtering, canonical weekday recognition,
invalid-time omission, minute conversion, lexicographic policy order, and
printed class sequence. Batch489 remains unpushed until F1170 closes. Batch490
remains inactive until then; F629 and both phase gates remain open.
Implementation and verification have not started.

## Phase 2 update - Batch489 complete locally; Batch490 active - 2026-10-20

F1170 completed at source/test commit `9f0116cb`, following acceptance commit
`286b2851`. Sub Prep print-order weekday and start-minute keys now use
distinct, non-converting Application wrappers that preserve exact integers
and the `INT_MAX` unknown sentinels. The comparator and feature producer keep
the existing key order and print sequence; an unknown weekday with a valid
minute still precedes a class with no meeting.

The incremental macOS Debug build passed for `ClassMngr` and all three focused
test targets; all three focused CTests passed 3/3 offscreen. The independent
Tester accepted the change, and `git diff --check` passed. No full suite or
Windows build ran. Batch489 was pushed to origin at `e329f92b`; the push
receipt is in the [Phase 2 progress log](../plans/qt-rewrite-heavy-route-plan/03-Phase-2-Progress-Log.md#batch489-push-receipt-2026-10-20).
Batch490 is active with seven candidates and F1171 next. F629 and both phase
gates remain open.

## Phase 2 update - F1171 accepted; Batch490 active - 2026-10-20

F1171 is accepted to replace `ManualUpdateArtifact::sizeBytes` and
`StartupUpdateArtifact::sizeBytes` with a shared Qt-free
`Application::UpdateArtifactSizeBytes`. The value retains exact signed
`std::int64_t` values, defaults to zero, and performs no validation. Platform
and legacy UI/controller boundaries must unwrap explicitly; the GitHub parser
and downloader retain their existing nonpositive-size policy.

Acceptance covers the value type, both Application results and use cases, both
Platform adapters, and the legacy consumer boundaries. Focused verification
targets are the two updater use-case tests, automatic-startup/controller tests,
and updater tests. At acceptance, implementation and verification had not
started. Batch489 is pushed; F1171 is complete and F1172 is next in Batch490.
F629 and both phase gates remain open.

## Phase 2 update - F1171 complete; Batch490 active - 2026-10-20

F1171 completed at source/test commit `6ef86387`, following acceptance commit
`ef32efbb`. The shared Qt-free `Application::UpdateArtifactSizeBytes` now
types manual and startup artifact sizes while preserving exact signed
`std::int64_t` values and zero defaults. Both Platform adapters and both
legacy consumer boundaries explicitly convert. Parser/downloader positivity
policy, SHA-256, timestamps, and other artifact fields remain unchanged.

The incremental macOS Debug build passed for `ClassMngr` and four focused
test targets. The Application, controller, and updater binaries passed; the
controller suite reported 25/25 and updater suite 31/31. Four focused CTests
passed 4/4 offscreen after a loopback-enabled retry. `git diff --check` passed.
No full suite or Windows build ran. Batch490 has one complete slice and F1172
next; Batch489 is pushed. F629 and both phase gates remain open.

## Phase 2 update - F1172 complete; Batch490 active - 2026-10-20

F1172 completed at source/test commit `319c7a8b`, following acceptance commit
`a06421a3`. The Qt-free `Application::UpdateCheckUtcMilliseconds` types
manual/startup check timestamps with exact signed values and zero defaults.
Existing service freshness, timestamp sourcing, and `QDateTime` boundaries
remain; the UI reconstructs UTC explicitly. Focused macOS build and four
offscreen CTests passed, and the independent Tester accepted the change.
Coverage uses signed extremes through the Qt-free use cases and representative
Qt-representable values at adapters/UI; it does not claim the full int64 range
through `QDateTime`. No full suite or Windows build ran. Batch490 has F1171–F1172
complete and F1173 next; it remains unpushed until F1177 closes. F629 and both
phase gates remain open.

## Phase 2 update - F1173 complete; Batch490 active - 2026-10-20

F1173 completed at source/test commit `a3160a51`, following acceptance commit
`8284393b`. The new `Application::CalendarEventImportCount` types the six
semantic counters with exact signed `int`, a zero default, explicit
construction, `rawValue()`, equality, and prefix increment. Parser and service
signals remain `int`; profiler and UI boundaries unwrap explicitly. Collection
cardinalities and indices remain `std::size_t`, and the existing 4096 save
limit is unchanged.

The incremental macOS Debug build passed for `ClassMngr` and three focused
test targets. Three focused offscreen CTests passed 3/3, including service
parity of 3 imported/2 skipped after a localhost-enabled retry. The
independent Tester accepted the change and `git diff --check` passed. No full
suite or Windows build ran. Batch490 has F1171–F1173 complete and F1174 next;
it remains unpushed until F1177 closes. F629 and both phase gates remain open.

## Phase 2 update - F1174 complete; Batch490 active - 2026-10-20

F1174 completed at source/test commit `d5450e3c`, following acceptance commit
`a678d01c`. `Application::ClassTransferExportUtcMilliseconds` now types the
export source's exact signed UTC epoch-millisecond value with a zero default.
The repository still sources UTC `QDateTime`; the Platform port converts that
value with `toMSecsSinceEpoch()`. The query unwraps at its chrono formatter
boundary and keeps `YYYY-MM-DDTHH:MM:SS.mmmZ` output in the existing JSON
string field.

Wrapper tests cover signed extremes, default zero, equality, and
non-convertibility. Formatter tests pin zero, 1,234 ms, and -1 ms; they do not
claim every `int64_t` can be formatted. The Platform test brackets its real
current UTC timestamp with before/after clock reads rather than injecting a
fixed clock. The incremental macOS Debug build passed for `ClassMngr` and
three focused test targets; three offscreen CTests passed 3/3. Codec parity
also passed, independent Tester accepted the slice, and `git diff --check`
passed. No full suite or Windows build ran. Batch490 has F1171–F1174 complete
and F1175 next; it remains unpushed until F1177 closes. F629 and both phase
gates remain open.

## Phase 2 update - F1175 complete; Batch490 active - 2026-10-20

F1175 completed at source/test commit `176df257`, following acceptance commit
`1e8b70cc`. The Application Class Transfer package version now uses exact
signed-int `ClassTransferPackageVersion` with default 1; adapter boundaries
unwrap explicitly and JSON output remains an integer. The legacy Domain field,
version-1 parser policy, and exact diagnostics are unchanged. The Apply Use Case
test fixture now uses the existing typed class-key API so its accepted target
builds.

The incremental macOS Debug build passed for `ClassMngr` and all five accepted
test targets. Four focused offscreen CTests passed, as did the direct parser
diagnostic case and `git diff --check`. The broader
`ClassMngrClassTransferTests` CTest had 63 passes and one teacher-fallback label
failure outside the package-version path; no clean baseline was run, so it is
not claimed as proven pre-existing. No full suite or Windows build ran.
Batch490 has F1171–F1175 complete and F1176 next; it remains unpushed until
F1177 closes. F629 and both phase gates remain open.

## Phase 2 update - F1176 complete; Batch490 active - 2026-10-20

F1176 completed at source/test commit `d3935c2f`, following acceptance commit
`ff39e21a`. The optional Application schedule policy hour and feature helper
now use Qt-free `ScheduleSlotCanonicalHour` with exact signed-int storage,
explicit construction, `.value()`, equality, and no range policy. The
15/16 cutoff, absent value, weekend/intensive precedence, toggling, parser,
and canonical-label check remain unchanged.

The independent Tester rebuilt `ClassMngr`, the policy tests, and schedule
print-model tests from a cleaned affected-target graph in the existing
`build/macos-clang-debug` folder. Four focused offscreen CTests passed 4/4,
including registered print-model parity cases; `git diff --check` passed. No
full suite or Windows build ran. Batch490 has F1171–F1176 complete and F1177
next; it remains unpushed until F1177 closes. Batch491 has eight provisional
candidates and remains inactive until Batch490 closes. F629 and both phase
gates remain open.

## Phase 2 update - F1177 accepted; Batch490 active - 2026-10-20

F1177 is accepted to add Qt-free `Application::AcademicTermWeekCount` with
exact signed-int storage, zero default, explicit construction, `.value()`,
equality, and no range policy. It types only the Application policy boundary;
the Feature model keeps its four raw integers, and JSON v1 parsing, UI
behavior, four-term cardinality, 1–53 validation, and date-distance rules stay
unchanged. `AcademicTermPosition::week` remains the separate one-based position.
Implementation and verification have not started. Batch490 has F1171–F1176
complete and F1177 accepted/in progress as its final slice; it remains
unpushed until F1177 closes. Batch491 has eight provisional candidates and
remains inactive until Batch490 closes. F629 and both phase gates remain open.

## Phase 2 update - F1177 complete; Batch490 locally complete - 2026-10-20

F1177 completed at source/test commit `d3e8ca01`, after acceptance commit
`914b5316` and CTest-name correction `80ea5513`. Qt-free
`Application::AcademicTermWeekCount` now types the Application policy boundary
while preserving Feature storage, JSON v1 parsing, UI behavior, cardinality,
range checks, and date-distance rules. The executor's four-target build
passed; the independent Tester confirmed eight focused CTests passed 8/8 using
the actual registered restore-defaults name
`ClassMngrCalendarPreferencesRestoreDefaultsTests`. Its build reported no work
because targets were current, and `git diff --check` passed. No full suite or
Windows build ran. Batch490 is complete and pushed at `69be43a7`; see the
[push receipt](../plans/qt-rewrite-heavy-route-plan/03-Phase-2-Progress-Log.md#batch490-push-receipt-2026-10-20).
Batch491 is active with eight provisional candidates and F1178 next. F1175's one
teacher-fallback label failure remains unresolved and is not
claimed to be pre-existing. F629 and the Phase 0 and Phase 2 gates remain open.

## Phase 2 update - F1178 accepted; Batch491 active - 2026-10-20

F1178 is accepted to add Qt-free `Application::ScheduleImportReviewCount` and
type the eight fields of `ScheduleImportReviewSummary` plus its two count
inputs. Preserve exact signed `int` values, zero default, explicit construction,
`rawValue()`, pre-increment, equality, and the absence of conversion or range
policy. Preserve action-to-count mappings, non-counting actions, and existing
review text. The Feature boundary wraps diagnostics and schedule-clear counts
and unwraps summary values for display. The applied-result
`ScheduleImportApplySummary` and collection positions remain separate.
Implementation and verification have not started. Batch490 is pushed at
`69be43a7`; Batch491 has seven other discovered candidates with F1179 next.
F629 and both phase gates remain open.

## Phase 2 update - F1178 complete; Batch491 active - 2026-10-20

F1178 completed at source/test commit `bf8d7fb7`, following acceptance commit
`2460b996`. `Application::ScheduleImportReviewCount` now types all eight
review-summary fields and the projection's two count inputs, preserving exact
signed values, zero defaults, explicit construction, pre-increment, equality,
action mappings, non-counting actions, and the existing review text. The
applied-result `ScheduleImportApplySummary` and positional indices remain
unchanged.

The four requested targets built successfully; the independent Tester rebuilt
the schedules-cleared projection target while the other three were up to date.
Both standalone Application projection CTests passed 2/2. The dialog selector
`reviewSummaryUsesApplicationProjection` passed 3/3 with a font-alias warning
only. `git diff --check` passed. No full suite or Windows run occurred. Batch491
has F1178 complete and F1179 next, with six discovered candidates remaining.
Batch490 is pushed; F629 and both phase gates remain open.

## Phase 2 update - F1179 accepted; Batch491 active - 2026-10-20

F1179 is accepted to reuse existing `Domain::CourseGradeBand` in both
Application Speaking Evaluation AI prompt request grade fields, with default
`Other`. Preserve 4–6 eligibility, rejection of `Other`, exact prompt text,
and the existing integer-to-band mapper at Feature boundaries. Keep the
`speakingEvaluationElementaryGrade` parser returning `int` with its current
behavior and tests unchanged, and retain report/legacy integer storage.
Implementation and verification have not started. Batch491 remains unpushed;
F1180–F1185 remain discovered. Batch490 is pushed at `69be43a7`; F629 and
both phase gates remain open.

## Phase 2 update - F1179 complete; Batch491 active - 2026-10-20

F1179 completed at source/test commit `26751644`, following acceptance commit
`3a46ce21`. Both Application AI prompt grade fields now use
`Domain::CourseGradeBand` with default `Other`; prompt eligibility and ordinal
formatting consume the typed band, while both Feature request boundaries map
the existing report integers through `speakingEvaluationCourseGradeBandFromInteger`.
Exact prompt text, the integer grade parser, report/legacy integer storage,
and unsupported-grade behavior remain unchanged.

The independent Tester cleaned the affected target outputs and rebuilt
`ClassMngr`, the prompt and eligibility test targets,
`ClassMngrSpeakingEvalBatchReportServiceTests`, and the affected Platform text
match port test target. Four focused CTests passed 4/4; the eligibility UI
selector passed with 3 QtTest cases. `git diff --check` passed. The UI checks
reported a font-alias warning without failures. No full suite or Windows build
ran. Batch491 remains unpushed with F1180 next and F1181–F1185 discovered.
Batch490 is pushed at `69be43a7`; F629 and both phase gates remain open.

## Phase 2 update - F1180 accepted; Batch491 active - 2026-10-20

F1180 is accepted to add Qt-free `Application::ClassTransferSkippedClassCount`
with exact signed-int storage, zero default, explicit construction,
`rawValue()`, equality, and no conversion or range policy. Type only
`ClassTransferApplySummary::skippedClassCount`; preserve use-case forwarding,
explicit wrapping of the legacy count at the Platform port, and the existing
Feature display text. Keep the legacy repository summary, created/replaced
IDs, and package-class index unchanged. Acceptance checks zero default,
explicit construction, `int` minimum/maximum round-trip and equality, and no
implicit conversions; it also checks positive use-case propagation, zero
Platform result, and a positive Skip-action copy at the Platform boundary.
The existing Sidebar parity test covers the unchanged `skipped: 1` display.
Build `ClassMngr`, the Application use-case and Platform apply-port tests, and
the Sidebar parity test target; run their focused CTests. Implementation and
verification have not started. Batch491 remains unpushed with F1181 next and
F1182–F1185 discovered. Batch490 is pushed at `69be43a7`; F629 and both phase
gates remain open.

## Phase 2 update - F1180 complete; Batch491 active - 2026-10-20

F1180 completed at source/test commit `20975320`, after acceptance commit
`c841fe2c`. `Application::ClassTransferSkippedClassCount` now types only
`ClassTransferApplySummary::skippedClassCount`, with signed-int storage,
zero default, explicit construction, `rawValue()`, equality, and no implicit
conversions or range policy. The use case forwards the value; the Platform
port wraps the repository's exact count; and the sidebar unwraps it while
keeping its existing display text. The legacy repository summary, IDs, and
package-class index remain unchanged.

The independent Tester cleaned affected generated outputs and rebuilt
`ClassMngr`, `ClassMngrNextApplicationClassTransferApplyUseCaseTests`,
`ClassMngrNextPlatformApplicationServicesClassTransferApplyPortTests`, and
`ClassMngrSidebarClassTransferApplyParityTests`. All three focused CTests
passed; wrapper checks cover zero, signed `int` extremes, equality, and both
conversion directions. The Platform test verified both zero and a
repository-backed Skip count of 1. `git diff --check` passed. No full suite or
Windows build ran. Batch491 remains unpushed with F1181 next and F1182–F1185
discovered. Batch490 is pushed at `69be43a7`; F629 and both phase gates remain
open.

## Phase 2 update - F1181 accepted; Batch491 active - 2026-10-20

F1181 is accepted to add Qt-free `Application::ClassTransferPackageClassIndex`
with exact signed-int storage, default `-1`, explicit construction,
`rawValue()`, equality, and no implicit conversion or range policy. Type only
the Application review candidate, resolution, and decision-issue index fields.
Keep matching-policy `std::size_t` positions and legacy/UI integer fields
unchanged. Preserve `-1`, values such as `99` (which remain
`UnknownClassResolution`), and all validation issue order. Convert explicitly
at Application and Platform adapter edges; keep preview and apply mappings.

Acceptance covers type semantics, sentinel and out-of-range validation,
preview order, issue order, and adapter/UI parity. Build `ClassMngr`,
`ClassMngrNextApplicationClassTransferTests`,
`ClassMngrNextApplicationClassTransferPreviewQueryTests`, and
`ClassMngrClassTransferTests`; run the first two CTests and
`ClassTransferPreviewAdapterRegressionTests`, plus the focused selector
`typedDialogRequestMatchesCompatibilityPlanAndGating`. Implementation and
verification have not started. Batch491 remains unpushed with F1182 next and
F1183–F1185 discovered. Batch490 is pushed at `69be43a7`; F629 and both phase
gates remain open.
