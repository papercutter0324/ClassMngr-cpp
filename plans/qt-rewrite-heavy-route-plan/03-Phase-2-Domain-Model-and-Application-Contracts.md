# Phase 2 — Domain Model and Application Contracts

Read [00-Start-Here.md](00-Start-Here.md) first for the overall plan, workflow, and phase sequence.

## Mandatory route for new and resumed work

- Use the Heavy route for every task, slice, follow-up, and reopened gate in this phase. `Default route: Heavy` is a binding instruction.
- At each start or resume, explicitly select Heavy and reread this phase plan and [00-Start-Here.md](00-Start-Here.md). Follow the active `AGENTS.md` Heavy-route instructions and `~/.codex/codex_workflow/heavy_route.md`, including their delegation, verification, and deployment-state requirements.
- A new session, handoff, or context reset does not change the route. Do not continue under Light or Medium. If a required Heavy-route step blocks progress, report the blocker before implementation instead of silently switching routes.

## Status

- Status: In progress
- Default route: Heavy
- Depends on: Phase 1
- Blocks: Persistence, bootstrap, shared UI, and feature migration
- Owner: Unassigned
- Last updated: 2026-09-30
- Historical progress log: [03-Phase-2-Progress-Log.md](03-Phase-2-Progress-Log.md)
- Exit gate: Open
- Current note: F145 is acceptance-complete as the existing Testing Class
  details update slice; F146 is accepted as new-class creation, including the
  optional pending weekday/start-time assignment in the atomic repository
  operation. F145 preserves roster-first page behavior. F147 is accepted for
  delete/cascade and its page transition fix. F148 adds common-input successful
  save parity; F149's typed conflict query and F150's typed validation policy
  are accepted. F126 routes `ClassDetailsPage` saves through the Qt-free use
  case reused by F139 in `ScheduleEditorDialog`. F151 is accepted for
  current/baseline live-page invalid-schedule parity (malformed regular/intensive,
  end-before-start, duplicate rows); Gate 2 advances but remains Partial. F152
  is accepted: its fresh-at-save typed query and active-session adapter return
  raw signed teacher ID and exact UTF-16 notes/activity; read-error behavior,
  save request, and `ClassService` guard remain as before. F153 is accepted for
  current/baseline page parity when persisted teacher ID zero changes after
  load. F154's typed Class Notes page-read query and F155's Class Co-Teacher
  selected-class/title read and F156's ClassCoTeacherPage teacher-catalogue
  read, F157's TeacherInfoPage profile-save port, and F158's selected-teacher
  navigation read, F159's Native English Staff Directory read, F160's GS
  Team Staff Directory read, F161's Native English Staff Directory save,
  F162's GS Team Staff Directory save, F163's ClassesPage class-list read,
  F164's selected-class grade read, and F165's selected-class subtitle read
  are accepted. F166 is selected for the
  roster editor's class subtitle read.
  The prior F123 candidate wording is historical;
  current Teacher Profile integration status is recorded in the progress log.
  Gates 1 and 2
  remain Partial; F120 active-v2 DataService isolation and formal
  workspace-create acceptance remain Satisfied.
  Historical workbook provenance remains a tracked risk, not a literal exit
  criterion. Sub Prep remains January 1 of the reference date's year through
  December 31 of the following year at most; 2026-2027 is illustrative.

## Objective

Create a stable, testable application core that is independent of widget construction, page visibility, and the legacy data facade.

## Work packages

### 2.1 Domain value types

Create explicit value types for:

- Workspace identifiers.
- Teacher and staff records.
- Class and course records.
- Class times and schedule entries.
- Students and rosters.
- Campuses and locations.
- Calendar events.
- Speaking evaluations and criteria.
- Document catalog entries, document-content references, and templates.
- Import matches and conflicts.
- User preferences.

Use typed identifiers and enums instead of unrelated strings that happen to contain IDs or state values.

### 2.2 Structured results

Replace loosely typed return values with structured results:

- Success values.
- Recoverable warnings.
- User-facing errors.
- Technical errors.
- Validation failures.
- Import conflicts.
- Cancellation state.

An import result, for example, should separately expose imported records, warnings, unmatched values, and conflicts.

### 2.3 Application use cases

Create use cases for:

- Creating, opening, closing, saving, and exporting a workspace.
- Importing a legacy database.
- Importing teachers, schedules, calendars, rosters, and class transfers.
- Editing teachers, classes, schedules, calendar events, rosters, and evaluations.
- Generating reports and substitute documents.
- Listing campus and document metadata.
- Opening document content on demand for a viewer or output operation.
- Performing backups and recovery.
- Checking for application updates after startup.

The Sub Prep slice also requires explicit summary, selected-detail, and
operation-scoped print-source contracts. Their feature-level implementation
plan is tracked in [Sub Prep Class Information and Output Memory
Plan](sub-prep-class-information-memory-plan.md).

Each use case must have:

- Explicit input.
- Explicit output.
- Structured errors.
- No widget references.
- No hidden singleton state.
- A deterministic test boundary.

### 2.4 Validation and business rules

Move validation rules into domain or application services.

The UI may display validation results and choose when to validate, but it must not own the business rule implementation.

Preserve current validation messages and behavior until visual and behavioral parity is accepted.

### 2.5 State and concurrency

Define:

- Workspace session state.
- Unsaved-change state.
- Current selection state.
- Import-job state.
- Report/export-job state.
- Document-content session state: requested, loading, ready, failed, and released.
- Cancellation behavior.
- Thread ownership.

Background work must return results through application interfaces. Worker code must not mutate widgets directly.

### 2.6 Memory-safe projections and operation contracts

The [Qt Rewrite Memory Hotspot Remediation
Plan](memory-hotspot-remediation-plan.md) defines the compact contracts needed
by the large-data slices. Add application-facing projections equivalent to:

- compact class and teacher summaries plus selected class details;
- a compact schedule view projection;
- an import review session containing matching indexes and compact decisions,
  not the original workbook and every derived UI object;
- staged transfer-reader and transfer-writer records;
- operation-scoped report and PDF render sources.

Every contract must state which layer owns the data, when raw or derived
representations may overlap, and when they are released. Contracts must not
return widget trees, page pointers, or broad compatibility-service snapshots.

## Deliverables

- Domain model library.
- Application use-case library.
- Structured error and warning types.
- Validation services.
- Application-state definitions.
- Domain tests that run without a QApplication.
- Mapping document from old service calls to new use cases.

## Exit gate

Domain and application behavior can be tested without constructing the main window.

Validation, conflict detection, import planning, and state transitions match the baseline fixtures.

For the workspace boundary, acceptance includes the current
`WorkspaceGateway::createWorkspace` contract and `WorkspaceCoordinator` create
behavior: dirty replacement is rejected before the gateway, a successful
session opens `WorkspaceState` and clears `SelectionState`, and gateway or
invalid-session failures preserve both snapshots. The existing app-less
`NextApplicationContractTests` and `NextApplicationWorkspaceCoordinatorTests`
cover these create paths alongside open, close, save, save-as, and export.

No new v2 production path depends on DataService, MainWindow, PageManager, or a widget pointer.

## Heavy-route requirements

- For every Phase 2 slice, use the heavy route: implement the contract across
  its intended v2 boundary, verify it with the owning layers, and remove any
  temporary compatibility wrapper when the slice is accepted.
- Convert core contracts rather than wrapping every old Qt type indefinitely.
- Keep Qt conversion at the UI, filesystem, or platform boundary.
- Prefer explicit immutable snapshots for read models.
- Keep document-content contracts independent of QtPdf; the viewer/platform
  adapter owns the active document session and its release boundary.
- Do not hide business rules inside presenters or delegates.
- Do not allow compatibility methods to become the permanent v2 API.

## Verified F143 progress record (commit `e2a3811cdba71b58ff2289f2c756d9bf12349bf5`)

F143 adds the Qt-free `TestingClassDetailsReadQuery`, typed snapshot, and
handler. Its Platform adapter reads the selected record directly through the
active `DatabaseSession`'s
`TestingClassRepository::loadTestingClass()`. `TestingClassesPage::loadClass()`
uses this boundary instead of a `ScheduleService` detail read. The result keeps
class ID, name, grade, level, room, teacher ID, class/font colors, and notes;
roster loading, success state, and warning behavior remain intact. An
unavailable session returns silent `NotFound`; missing/read errors remain
warnings. Nonpositive teacher IDs map to absence, preserving the editor's
“None” row.

Independent fresh Windows x64 Debug Ninja/MSVC/Qt 6.12 verification in
`C:\Users\wfelt\AppData\Local\Temp\codex_f143_testing_class_details_20260929`
validated 1,021 handwritten source owners. The build succeeded for
`ClassMngrNextApplicationTestingClassDetailsReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesTestingClassDetailsReadPortTests`,
and `ClassMngrTestingClassesPageTests`; the exact focused CTest selection passed
3/3 (0.10s, 0.89s, 1.64s). App-less tests cover typed-ID/result propagation;
Platform tests cover field mapping, absent/zero teacher, unavailable/missing/
read-error behavior, and no fallback; the page test covers fields, roster,
success, silence/warnings, the zero-ID “None” row, and the F142 list regression.
No full suite or baseline comparison ran. Source/test commit
`e2a3811cdba71b58ff2289f2c756d9bf12349bf5`.

F143 adds Gate 1 application behavior evidence and no Gate 2 baseline-parity
evidence. F120 active-v2 DataService isolation and the formal workspace-create
boundary remain Satisfied. Phase 2 remains In Progress with its exit gate Open.
Historical production-workbook provenance remains a tracked risk, not a
literal exit criterion. Sub Prep remains bounded to January 1 of the reference
date's year through December 31 of the following year at most; 2026-2027 is
illustrative.

### Cumulative exit-gate status after F143

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | Coverage includes workspace lifecycle; Class Transfer schedule-candidate validation (F79); Teacher Import (F80/F81/F85/F87/F89/F90); Schedule Import match-key typing (F84); Calendar policies/use cases (F93/F95/F97/F99/F101-F105/F107/F110); teacher-profile editing (F117/F123); co-teacher assignment (F124); class-notes save (F125); Classes navigation snapshot (F133); ScheduleBuilder source snapshot (F134); slot-state and testing-assignment reads (F135/F136); testing-class choices and TestingClasses list reads (F138/F142); ScheduleEditor save and class-info read (F139/F140); and TestingClasses selected-detail read (F143). Broader class-detail, schedule, roster, and evaluation editing, backup/recovery, and legacy database import remain planned. |
| Baseline parity (Gate 2) | Partial | Bounded records include F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions, F118 common-input Class Transfer conflict behavior, F121 common-input successful replacement state, and F122 hand-authored seeded Schedule Import Skip state parity. They do not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remains unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for active `src/next` call paths | F120 removes the Workspace operation edge to `DataService`; the source audit confirms all seven operations route to `DatabaseSession` or the file helper. Legacy facade construction/access remains available. |

Phase 2 remains In Progress with its exit gate Open. Gate 1 and Gate 2 remain
Partial; historical production-workbook provenance remains a tracked risk, not
a literal exit criterion. Sub Prep remains bounded to January 1 of the
reference date's year through December 31 of the following year, at most;
2026-2027 is illustrative.

## F144 acceptance record

F144 moves `TestingClassesPage::populateTeachers()` from
`TeacherService::teachers()` to a Qt-free typed Application query and a
Platform adapter that reads
`DatabaseSession::teacherRepository()->getAllTeachers()`. It preserves
repository order, typed teacher IDs, trimmed Korean labels and rooms, blank
label filtering, ID-based selection restoration, the None row, silent
unavailable-session behavior, and the existing failure warning.

The partial production/test implementation is checkpointed at
`56c76f412b246230fcfe00c249b195dcc6ccd95f`; the final page-test update is in
acceptance test commit
`89fbbaa250ddf98fae2ab1d80385fb99164ac055`.

Each fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 configure, with
`BUILD_TESTING=ON`, validated 1,025 handwritten source owners. The F144
Application query, Platform adapter, and `ClassMngrTestingClassesPageTests`
targets built and passed CTest 3/3 in
`build/phase2-f144-independent-ninja-x64-20260929`. The F142/F143 Application
and Platform regression targets built and passed CTest 4/4 in the separate
short-path tree `build/p2-f142f143`. All seven focused CTest cases passed.
`git diff --check` passed. No full suite or full `ClassMngr`
application build ran.

An earlier F142/F143 Platform build attempt failed before test execution with
MSVC C1083; the separate fresh short-path build passed without reproducing it.
The cause is unknown. The null-`teacherRepository()` defensive branch has no
direct test seam while an open `DatabaseSession` exists; null services,
unopened sessions, and closed sessions cover the observable unavailable case.
No production defect was observed.

F144 is the preceding accepted slice. F145's accepted existing-class details
update is recorded below. F146's accepted new-class create, including the
optional pending weekday/start-time assignment, is recorded after it. F147's
accepted delete/cascade migration is recorded below; F148-F159 acceptance and
F160 selection are recorded below.

## F145 acceptance record

F145 adds existing-class detail updates through the Qt-free
`TestingClassDetailsUpdateUseCase` and the active-session Platform adapter to
`TestingClassRepository::updateTestingClass()`. The Testing Classes page keeps
its roster-first save order: if the roster save succeeds and the details update
fails, the roster remains persisted and clean. F146 handles new-class creation
and its optional pending assignment below; delete/cascade remains separate.

F145's production and acceptance-test commits and verification limits are
recorded in the [Phase 2 progress log](03-Phase-2-Progress-Log.md). Gate 1 and
Gate 2 remain Partial; Phase 2 remains In Progress with its exit gate Open.

## F146 acceptance record

F146 routes new Testing Class creation through the Qt-free
`TestingClassCreateUseCase` and the active-session Platform adapter to
`TestingClassRepository::createTestingClass()`. The existing optional pending
weekday/start-time assignment travels with creation because that repository
operation owns the atomic class, details, room, and assignment transaction.
F145 remains the separate existing-class details update slice; F147 handles
delete/cascade below.

Production and acceptance-test commits and verification limits are recorded in
the [Phase 2 progress log](03-Phase-2-Progress-Log.md). Gate 1 and Gate 2 remain
Partial; Phase 2 remains In Progress with its exit gate Open.

## F147 acceptance record

F147 routes Testing Class deletion through the Qt-free
`TestingClassDeleteUseCase` and an active-session Platform adapter to
`TestingClassRepository::deleteTestingClass()`. The confirmation discloses the
roster, notes, speaking evaluations, regular and intensive class times, and all
schedule assignments affected by deletion. The page clears the deleted editor
and roster before rebuilding and selecting a sibling, preventing the old dirty
draft from being saved over that sibling. F145 details update and F146 class
creation remain separate accepted slices.

Production, page-transition-fix, and acceptance-test commits and the focused
verification evidence and limits are recorded in the [Phase 2 progress
log](03-Phase-2-Progress-Log.md). Gate 1 and Gate 2 remain Partial; Phase 2
remains In Progress with its exit gate Open. F148 adds one successful-save
comparison; F149 adds a typed conflict query and a baseline parity case. F150
is selected and implementation is starting. The latest [Phase 2 progress
entry](03-Phase-2-Progress-Log.md) records the acceptance boundaries and
verification evidence.

## F148 acceptance record

F148 adds a live `ClassDetailsPage` successful-save comparison against the
pinned legacy baseline using the same seeded teacher, class, and edits. The
case verifies persisted fields and untouched values, regular/intensive time
ordering, the `classInfoSaved` signal, and clean dirty state. This adds one
common-input Gate 2 successful-save case; it provides no validation or conflict
evidence. F149 conflict behavior is recorded below. Commit,
test, toolchain, and verification limits are recorded in the [Phase 2 progress
log](03-Phase-2-Progress-Log.md). Gate 1 and Gate 2 remain Partial; Phase 2
remains In Progress with its exit gate Open.

## F149 acceptance record

F149 routes the Class Details page's regular/intensive pre-save conflict lookup
through a Qt-free typed query and an active-session Platform adapter to the
existing `ClassInfoRepository` operation. Warning presentation stays in the
page; overlap calculation/order stay in persistence; `ClassService` save-time
guards remain. The page behavior test
`regularConflictShortCircuitsIntensiveAndKeepsSameNameWording` covers the
same-display-name warning edge case. Baseline parity compares regular/intensive
conflicts with distinct conflicting class names. Focused current and baseline
evidence is recorded in the [Phase 2 progress log](03-Phase-2-Progress-Log.md).
F150 acceptance is recorded below. Gates 1 and 2
remain Partial; the Phase 2 exit gate remains Open.

## F150 acceptance record

F150 moves `ClassDetailsPage` pre-save validation and normalization into a
typed Qt-free Domain/Application policy and maps structured issues through
`FormValidationBinder`. It preserves `ClassService` save-time validation,
F149 conflict ordering/warnings, exact duplicate-schedule behavior, validation
order, Qt-dependent book-catalog rules, raw malformed schedule input, and
hidden-field behavior without duplicating catalog rules. Current and baseline
verification, including representative-only page field/focus mapping coverage,
is recorded in the [Phase 2 progress log](03-Phase-2-Progress-Log.md). Gates 1
and 2 remain Partial; Phase 2 remains In Progress with its exit gate Open.

## F151 validation-parity slice

F151 compares live-page current and baseline validation for malformed regular
and intensive schedule inputs, end-before-start, and duplicate rows. Invalid
cases must remain dirty, produce no visible conflict warning, save, or signal,
and leave persisted data unchanged. Current-only page tests assert no conflict
query directly; baseline parity uses a seeded conflict trap. Compare duplicate
membership and row feedback semantically; do not compare cross-group issue
order because legacy `QHash` ordering is unspecified. F152 implements the
separate typed Application query for fresh hidden persisted
teacher/notes/activity fields.

## F151 acceptance record

F151, commit `f232301e`, is accepted with current/baseline live-page validation
parity. The current focused run passed 4/4; baseline parity passed CTest 1/1
and six QtTest cases. Four input cases cover malformed regular, malformed
intensive, end-before-start, and two duplicate groups with a unique row. Issue
mapping, dirty state, no visible warning/save signal, and unchanged data are
asserted. Current page tests count conflict-query calls directly; the baseline
parity uses a conflict trap. Toolchain, overlay, and verification limits are in
the [Phase 2 progress log](03-Phase-2-Progress-Log.md). Gate 2 advances but
remains Partial; Gate 1 remains Partial and Phase 2 stays In Progress/Open.

## F152 hidden validation-context read

F152 loads the validation context fresh at save time through a typed
Application query and active-session Platform adapter, returning the raw signed
teacher ID and exact UTF-16 notes/activity; do not reuse the display snapshot.
The page queries on each save; on read error it uses `ClassInfo{}` and
continues validation, conflict, and save. Preserve `-1` as the unassigned
sentinel, treat zero as invalid, retain missing-row defaults, and keep the
validation/conflict/save order. The save request, adapter reread, and
`ClassService` guard remain separate. F153-F159 acceptance and F160 selection
are recorded below.

## F152 acceptance record

F152 is accepted at commit `f70e3e23`. The current fresh Windows x64 Debug
focused run passed 7/7 across the Application query, Platform port, F150 policy,
page save/display, parity, and SharedPolicy targets. The baseline parity CTest
passed 1/1 on `f232301e48f1e198d301acdfa3d8f704569f7ddc`, using only the parity
test source overlay; the baseline production page matches its pinned blob.
Toolchain, fixture repairs, PDB workaround, and limits are in the [Phase 2
progress log](03-Phase-2-Progress-Log.md). Gates 1 and 2 remain Partial; Phase 2
remains In Progress with its exit gate Open.

## F153 accepted teacher-ID validation parity

F153, commit `477ed151`, compares the live page and baseline when persisted
`teacherId=0` changes after page load. The accepted assertions cover the legacy
teacher issue, dirty state, no save signal or visible conflict warning, and an
unchanged target row. The conflict check uses a warning trap, not a query-count
comparison. Current parity and page-save targets passed 2/2; the page-save
target retains F152's separate no-conflict-query regression. The original pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` focused parity harness passed 1/1.
The baseline overlay changed only the parity harness/registration and Qt
minimums; no production source was overlaid, and its page source matches blob
`cdc48da8e3bab73dd0e064cf8364899f67ad1021`. Toolchain and verification limits
are in the [Phase 2 progress log](03-Phase-2-Progress-Log.md). Gates 1 and 2
remain Partial; Phase 2 remains In Progress/Open.

## F154 accepted Class Notes page read

F154, commit `8bcbf136`, adds a typed Class Notes query/port and active-session
Platform adapter, separate from the existing save port. Its projection carries
class ID, exact UTF-16 notes/time-filler activities, grade/level, regular
schedule day/start, and preferred teacher display name; class and teacher
outcomes are independent. The UI retains trimming and subtitle formatting/
fallbacks. Failed reads preserve defaults; there is no `DataService` fallback.
Load/discard use the query; refresh/save add no reads.

Six current focused CTest targets passed 6/6 across the query, adapter, feature
page, parity, and existing Application/Platform save ports; configure validated
1,058 source owners. The original pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` focused parity harness passed 1/1
for initial text/subtitle and discard reload. Its overlay changed only the
parity test source/registration and Qt minimums; no production source was
overlaid. Baseline page/header match blobs
`bbc9bc24a053aca83434eba6efac1e4ad5801bc2` /
`5c825327f1393791d7101ab33c10999768ff639a`. Toolchain and build provenance are
recorded in the [Phase 2 progress log](03-Phase-2-Progress-Log.md). No full
suite or application build ran. F155 remains selected, not implemented or
accepted. Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open.

## F155 accepted Class Co-Teacher selected-class/title read

F155, commit `2d810d0e21f85233575b700e927ec3a36c907f21`, adds the typed
selected-class/title read and active-session Platform adapter. The query returns
selected teacher ID, grade/level/regular-schedule inputs for
`SidebarNodeNaming`, and assigned teacher display name with independent class
and teacher outcomes. It runs on load, discard, and after successful assignment;
existing fallbacks, selection/title, dirty/save/signal behavior, teacher-choice
catalogue, and assignment use case/adapter remain intact. The slice does not
cover schedule, roster, or Teacher Profile reads.

Six focused current CTest targets passed 6/6 on Windows x64 Debug with CMake
4.4.2, Ninja 1.13.2, MSVC 19.51, and Qt 6.12: query, Platform adapter, page,
current parity, and existing Application/Platform assignment tests. The pinned
baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity passed 1/1 using the
baseline cache and only adapted parity source/registration/Qt minimum overlays;
no production source was overlaid. Baseline page/header match blobs
`d25263eda8d464b2a3b17a35f44d6376ee5db588` /
`bba856ebb2ed907072d266a38bf3abe4e939d95e`. Visible load, external-change
discard, and post-save selection/title/persistence passed on both. Keep current-
only read-count claims separate; do not claim baseline query-count parity. Gates
1 and 2 remain Partial; Phase 2 remains In Progress/Open.

## F156 accepted Class Co-Teacher teacher-catalogue read

F156, commit `3581078bdca61cfe76489ff19c5d518d8b3145bb`, replaces
`TeacherService::teachers()` for `ClassCoTeacherPage` with a separate Qt-free
Application catalogue projection and active-session Platform adapter. It
includes only fields consumed by `TeacherInfoSection` and preserves teacher
IDs, bilingual ordering/details, selection, and load-failure warning/clear
behavior. It uses no `DataService` or `TeacherService` fallback. F155's snapshot
and assignment save remain unchanged.

Eight focused current CTest targets passed 8/8; configure validated 1,071
owners. Verification used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, and Qt 6.12.
Coverage includes direct active-repository mapping/no-fallback, warning/clear,
crossed bilingual order (Korean one-to-two, English two-to-one, None first),
selection/details/title on load and discard, and current save/post-save.
Original pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity
passed 1/1 with test-only DB API adaptation, registration, and Qt minimums; no
production overlay. Baseline page/header match blobs
`d25263eda8d464b2a3b17a35f44d6376ee5db588` /
`bba856ebb2ed907072d266a38bf3abe4e939d95e`. No baseline query-count claim; no
full suite or application build. Gates 1 and 2 remain Partial; Phase 2 remains
In Progress/Open.

## F157 accepted TeacherInfoPage profile-save port

F157, commit `cb6199f3369420c0e2e6c77d11f853d2799d9f92`, replaces the
page-local `TeacherServiceProfileEditPort` with an active-session
`TeacherRepository` update/reload adapter. It maps all `TeacherProfile` fields
and preserves `TeacherProfileEditUseCase`, `TeacherInfoValidationPolicy`,
canonical reload, and visible validation/warning/dirty/save/signal semantics.
The slice excludes Teacher Profile load and navigation reads.

The current focused build and CTest passed 4/4 (Application use case, Platform
port, TeacherInfoPage, and page parity) after configure validated 1,074 owners.
Toolchain: Windows x64 Debug, CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, Qt 6.12.
Pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` public-page parity
passed 1/1 with only parity source/registration overlaid; no production source
was overlaid. Baseline page/header hashes are
`49e9ee45226ddb2f456d73c369901ad459a6eb27` /
`e279133ab8c601d5e514b3531b9e2e25a62604f1`. Parity covers valid normalized
save/reload and invalid no-write. No full suite/application build or baseline
query-count claim.

The F157 focused tests also reverified TeacherInfoPage/use-case save/reload and
invalid-write behavior previously attributed to F123. This evidence is limited
to the tested page-edit path; it does not establish broader F123 acceptance.
Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open.

## F158 accepted TeacherInfoPage teacher-navigation read

F158, commit `4d099893071d4271ea8873d2219dfc7642de1e5c`, moves only the
selected-teacher lookup in `NavigationController::handleTeacher` to a Qt-free
typed Application query and active-session Platform `TeacherRepository`
adapter. It preserves lookup-before-leave-confirmation order and silent lookup
failures; F157's edit port, the existing page-load path, and other teacher
reads remain separate. Focused verification passed 8/8 on current and 1/1 on
the pinned baseline; details and limitations are in the progress log. Gates 1
and 2 remain Partial; Phase 2 remains In Progress/Open.

## F159 accepted Native English Staff Directory read

F159, source commit `1849ed23327538e2d21b05dfe0cebcc97e99d78c`, migrates only
the Native English branch of `StaffDirectoryPage::loadDirectory()` through a
typed Qt-free Application query and active-session Platform adapter using
`NativeEnglishTeacherRepository::getAll()`. It preserves the six displayed
fields, ID role, repository ordering, confirmation-before-read and
show-only-on-success behavior, unavailable-state clear, repository-error
warning, and current state semantics. The GS Team branch, both save paths, and
Phase 7 model/view conversion remain separate.

Five focused current CTest targets passed 5/5: Application query, Platform
adapter, Native English page behavior, pinned-baseline parity, and
`StaffDirectoryPage` regression. Public-page parity passed 1/1 on pinned
baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` with only test/harness and
registration changes and no production overlay. The scratch baseline run used
a temporary Qt 6.12 metadata compatibility shim, removed after configure with
the original CMake hash restored, plus a parity-test API adaptation. No full
suite/application build or baseline query-count claim. Detailed evidence is in
the [Phase 2 progress log](03-Phase-2-Progress-Log.md).

## F160 accepted GS Team Staff Directory read

F160, source commit `b703d3260a01b783594ffe6b87d10d9f7b02d193`, migrates only
the GS Team branch of `StaffDirectoryPage::loadDirectory()` through a typed
Qt-free Application query and active-session Platform adapter to
`GsTeamRepository::getAll()`, using int-backed `Domain::GsTeamMemberId`. It
preserves the five displayed fields, ID role, repository order, unavailable-
session silent clear, repository-error warning, and confirmation-before-read/
show-only-on-success behavior. The Native English branch, both save paths, and
Phase 7 model/view conversion remain separate. Focused verification and limits
are recorded in the [Phase 2 progress log](03-Phase-2-Progress-Log.md).

## F161 accepted Native English Staff Directory save

F161, source commit `ac173977d8d517d4af3236ee7794368bbe9a6bdc`, migrates only
the Native English branch of `StaffDirectoryPage::saveDirectory()` through a
typed Qt-free Application save/validation operation and active-session
Platform adapter to `NativeEnglishTeacherRepository::saveDirectory()`. The
Application decides empty/duplicate comparison-key and valid-or-blank birthday
rules; the Qt page supplies simplified, case-folded keys and current `QDate`
facts while retaining localized warnings. Typed row IDs, CRUD and transaction
behavior, failure dirty state, quiet autosave, successful reload, and
`directorySaved` are preserved. The GS Team writer remains separate. Focused
verification and limits are in the [Phase 2 progress
log](03-Phase-2-Progress-Log.md).

## F162 accepted GS Team Staff Directory save

F162, source commit `00a56324f1435475e3a7479fec99bc2e01653495`, migrates only
the GS Team branch of `StaffDirectoryPage::saveDirectory()` through a
GS Team-specific typed Application save operation/policy and active-session
Platform adapter backed directly by `GsTeamRepository::saveDirectory()`, using
`GsTeamMemberId`. It preserves the five fields, typed existing/deleted IDs,
add/update/delete transaction, warning versus quiet autosave, dirty-on-failure,
reload, and signal behavior. The Native English writer and model/view
conversion remain separate.

Focused CTest passed 16/16. Pinned-baseline GS Team save parity passed 1/1 on
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`. No full suite, application build,
or query-count comparison ran. Verification details and limits are in the
[Phase 2 progress log](03-Phase-2-Progress-Log.md).

## F163 accepted ClassesPage class-list read

F163, source commit `a556c0441dcebbb3b6a7baecef6e293b5644b149`, adds the typed
`ClassesListReadQuery`/port and active-session Platform adapter to
`ClassRepository::getClasses()`. Only the ClassesPage open and post-ClassInfo-
save class-list reads use it. Ordering, IDs/names, selection, and empty/error
behavior remain intact; navigation metadata and other legacy class calls stay
outside the slice.

Focused Application/Platform CTests passed 2/2, and the direct current-page
slots passed. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` passed visible list parity on open
and after save; its overlay changed only `classes_page_tests.cpp` and
`schedule_widget_test_stubs.cpp`, with production hashes matching. The full
ClassesPage CTest stalled in existing
`classDetailsAndCoTeacherTabsSeparateTheirSectionCards()` after
`nestedEditorsAreDeferredUntilTheirSectionIsOpened()`. No full suite,
application build, or query-count comparison ran. See the [Phase 2 progress
log](03-Phase-2-Progress-Log.md) for evidence and limits.

## F164 accepted ClassesPage selected-class grade read

F164, source commit `85af7830708f062e34c94fde8fa0ff40310a8d9e`, adds
`SelectedClassGradeReadQuery`/port and an active-session adapter to
`ClassInfoRepository::loadClassInfo()`, used only in
`ClassesPage::rebuildSectionTabs()`. It preserves grade normalization, the
middle-school rule, preference override, current selection, and fail-open
behavior; missing or failed grade reads leave Analytics and Evaluations visible.
There is no `DataService` fallback. Subtitle and navigation metadata reads
remain separate.

Application and Platform focused tests passed 4/4 each; three focused page
slots passed. `middleSchoolAnalyticsAndEvaluationsTabsFollowPreference` passed
on current and pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, and baseline production blobs
matched. Stub-backed visible tab parity passed. The separate real-database
parity harness timed out after 300 seconds on both revisions without a mismatch
or cause identified. No full ClassesPage suite, application build, or
query-count comparison is claimed. Evidence and limits are in the [Phase 2
progress log](03-Phase-2-Progress-Log.md).

## F165 accepted ClassesPage selected-class subtitle read

F165, source commit `e0b9d61213af57a79a685264c9f64fabf841bd1a`, adds the
Qt-free `SelectedClassSubtitle` read query and active-session repository
adapter. Its narrow projection carries class grade, level, and regular schedule
plus assigned-teacher display fields, with independent class and teacher
outcomes. The UI retains subtitle formatting, fallback, and refresh behavior;
there is no `DataService` fallback.

Current Application and Platform CTests passed. The focused page slots
`selectedClassSubtitleUsesIndependentReadOutcomesAndRefreshes`,
`selectedClassSubtitleFallbackChainUsesTrimmedValues`, and
`selectedClassGradeFailureFailsOpenWithoutDataServiceFallback` passed. On the
pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, the visible
subtitle `E4 Hercules • Susan • Tues (4:00)` passed with only a test-source
slot overlaid; all 584 baseline `src/` blobs matched. `git diff --check` passed.
No full suite, application build, or query-count comparison is claimed. See the
[Phase 2 progress log](03-Phase-2-Progress-Log.md) for evidence and limits.

## F166 selected RosterEditorWidget class subtitle read

Migrate `RosterEditorWidget::updateHeaderText()` and
`sidebarClassDisplayName()` in
[`roster_editor_widget_ui.cpp`](../../src/features/roster/ui/roster_editor_widget_ui.cpp)
from `ClassService::classInfo()` and `TeacherService::teacher()` to F165's
typed selected-class subtitle read. Keep `SidebarNodeNaming::formatClassDisplayName()`
in the UI. Preserve invalid-ID "No class selected"; for valid IDs retain the
classroom-name/`Class N` fallback when the read is unavailable and retain class
details when the teacher read fails. Keep the title, embedded heading, and
roster load/save behavior unchanged. Acceptance covers exact subtitle output,
teacher failure, fallback/no-session behavior, and pinned-baseline parity.
Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open.
