# Phase 2 Progress Log

Historical progress entries moved from [03-Phase-2-Domain-Model-and-Application-Contracts.md](03-Phase-2-Domain-Model-and-Application-Contracts.md).
Entries are preserved in their original document order.

## Progress update - 2026-09-19 (initial domain-contract slice)

`ClassMngrNext::Domain` now owns header-only, Qt-free contracts for typed
workspace, teacher, class, campus, and calendar-event identifiers, together
with `Result<T>`, `Result<void>`, `OperationError`, and explicit error codes.
The contracts reject empty identifiers, keep identifier categories distinct at
compile time, and separate successful values from recoverable or technical
operation failures.

`ClassMngrNextDomainContractTests` exercises these rules without constructing a
`QApplication`; CMake source ownership records the new headers explicitly.
The Windows Debug target and `ClassMngrNextLaunch` both passed locally. The
next slice is to define the first application use-case input/output contract
and map it to the legacy service boundary without adding widget or singleton
dependencies.
## Progress update - 2026-09-19 (workspace application-contract slice)

`ClassMngrNext::Application` now owns a Qt-free, header-only workspace
create/open/close contract. Requests carry an adapter-neutral workspace
location, successful create/open operations return a copyable session
projection containing the typed `Domain::WorkspaceId` and location, and close
accepts that caller-owned session explicitly. `WorkspaceUseCase` validates
empty locations before invoking an injected `WorkspaceGateway`, which keeps
the boundary stateless and makes gateway call counts deterministic in
`NextApplicationContractTests`.

The adapter-facing legacy mapping is reserved for a later outer adapter:
v2 `openWorkspace` maps to `ApplicationServices::openDatabase(QString)` and
v2 `closeWorkspace` maps to `ApplicationServices::closeDatabase()`; QString
conversion is deferred to that adapter, and legacy QString errors are mapped
to structured `Domain::OperationError`. No Qt/DataService adapter is part of
this slice.
## Progress update - 2026-09-19 (workspace persistence application-contract slice)

The stateless workspace contract now exposes explicit save, save-as, and export
requests, with validation of the caller-owned session and destination before
the gateway is called. The outer adapter mapping is concise and exact:

- v2 `saveWorkspace` -> `ApplicationServices::saveDatabase` (outer adapter
  turns legacy void/postcondition into a structured result)
- v2 `saveWorkspaceAs` -> `saveDatabaseAs(QString)`
- v2 `exportWorkspace` -> `exportDatabaseAs(QString)`
## Progress update - 2026-09-19 (workspace application-state slice)

`ClassMngrNext::Application` now owns a Qt-free `WorkspaceStateSnapshot` and
`WorkspaceState`. A snapshot is a copyable value containing an optional
caller-visible `WorkspaceSession`; its lifecycle is closed or open based on
that optional session, and its unsaved state is clean or dirty. `WorkspaceState`
owns its snapshot and returns copies, so no caller, widget, page, or singleton
retains hidden current-workspace state.

The transition policy is deterministic: opening a valid session from closed or
replacing a clean session succeeds and resets the new session to clean;
invalid session input returns `InvalidInput`; dirty replacement and dirty
close return recoverable `Conflict`; dirty/saved/close operations without an
open session return `NotFound`. Marking dirty or saved is idempotent while a
session is open, and a successful close resets the snapshot to closed/clean.
`NextApplicationStateTests` covers the value and transition contract without
constructing a `QApplication`. Legacy service adapters and migration remain a
separate outer-boundary slice.
## Progress update - 2026-09-19 (current-selection application-state slice)

`ClassMngrNext::Application` now owns a Qt-free `SelectionStateSnapshot` and
`SelectionState`. A snapshot is a copyable value backed by a variant of no
selection, `TeacherId`, `ClassId`, `CampusId`, or `CalendarEventId`. Its
`SelectionKind` and typed accessors are derived from the same variant, so one
selection cannot carry a category/value mismatch. Typed domain identifiers
remain the only accepted identifier inputs; empty raw identifiers cannot enter
this contract.

`SelectionState` is the sole owner of the current application selection and
returns copies from `snapshot()`. `setSelection` replaces the owned value and
`clear` releases it immediately. Selection is not persisted with a workspace;
the later application coordinator must clear it at workspace close or
replacement boundaries so an identifier from one workspace cannot leak into
another. This owner remains independent from workspace state and has no hidden
external lifetime or object identity dependency. `NextApplicationSelectionTests`
covers no selection, all supported categories, replacement, clearing, value
copy/equality, and compile-time category distinction without a `QApplication`.
## Progress update - 2026-09-19 (import-job state and cancellation slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ImportJobSnapshot`/`ImportJobState` contract. The snapshot exposes the compact
`Idle`, `Running`, `Completed`, `Failed`, and `Canceled` lifecycle, total and
completed item counts, a cancellation-requested flag, and an optional
structured failure. `ImportJobState` accepts a new job from any non-running
phase, rejects duplicate starts with recoverable `Conflict`, rejects progress
above the total with `InvalidInput`, and only completes after all items are
reported. Terminal snapshots do not change in response to late worker events;
`start` is the explicit reset boundary for a later job.

Cancellation requests are idempotent while running. A worker cancellation
acknowledgement (or the equivalent `cancel()` event) transitions the running
job to `Canceled` and preserves its counts; a completion event wins if final
progress arrives before cancellation is acknowledged. One application owner
serializes worker events and owns this state; worker code emits events and does
not mutate the state object directly. `NextApplicationImportJobTests` covers
the lifecycle, structured failure, cancellation, terminal immutability, and
copy/equality behavior without constructing a `QApplication`.
## Progress update - 2026-09-19 (Phase 2.6 import-review projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ImportReviewSession` projection. It contains typed teacher/class matching
indexes, category-specific decisions with explicit `Unresolved`, `Create`,
`UpdateOrReplace`, and `Skip` actions, and separate warning, unmatched-value,
and conflict collections. Teacher decisions can target only
`Domain::TeacherId`; class decisions can target only `Domain::ClassId`.
Decision and session factories reject empty or oversized source text and
contradictory action/target combinations with `Domain::ErrorCode::InvalidInput`;
explicit compact caps of 4,096 entries per index/decision/diagnostic
collection and 256 candidates per match index preserve the large-import
scenarios without allowing an unbounded projection. Adapters must paginate or
stage inputs above those caps before creating a review session.

The session is an operation-scoped value snapshot. The import/parser owner
releases raw workbook bytes, decoded worksheet buffers, and other broad
compatibility representations before or when this projection becomes
authoritative. UI code materializes only bounded review rows from the
snapshot, and releases its session copy when apply or cancel completes. The
projection retains no workbook, cells, Qt types, widgets, repositories, or
page pointers. `NextApplicationImportReviewTests` verifies these boundaries,
typed categories, readiness/conflict behavior, invalid decisions, and
copy/equality semantics without constructing a `QApplication`.
## Progress update - 2026-09-19 (report/export-job state and output contract slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ReportJobSnapshot`/`ReportJobState` contract for report, PDF, and export
operations. The lifecycle is explicit (`Idle`, `Running`, `Completed`,
`Failed`, and `Canceled`) with bounded total/completed unit counts,
cancellation-requested state, optional structured failure, and a bounded
output reference populated only by successful completion. Duplicate starts,
invalid progress, incomplete completion, and blank or oversized output
references return structured errors without mutating the snapshot. Terminal
snapshots remain immutable until an explicit restart.

The state owner releases the operation-scoped render source and output
buffers after completion, cancellation, or failure; the snapshot retains only
the bounded successful output reference. Workers emit lifecycle events to the
owner instead of mutating state. The actual PDF renderer and its platform or
filesystem adapter remain outside this contract. `NextApplicationReportJobTests`
covers the lifecycle, validation, cancellation, output bound, terminal
immutability, copy/equality, and worker-boundary behavior without constructing
a `QApplication`.
## Progress update - 2026-09-20 (report-job event bridge/coordinator slice)

`ClassMngrNext::Application` now owns a Qt-free `ReportJobCoordinator` with a
generation-tagged, bounded thread-safe FIFO sink and adapter-neutral worker
port. The coordinator alone applies progress, output-bearing completion,
cancellation acknowledgement, and failure events to `ReportJobState`; stale
generations, invalid/incomplete completions, and late terminal events cannot
mutate the active snapshot. Worker start/cancellation failures and exceptions
remain structured, while renderer and legacy adapters stay outside the slice.
`NextApplicationReportJobCoordinatorTests` covers the sink boundary, ordering,
overflow, restart isolation, cancellation races, output validation, and
terminal immutability without constructing a `QApplication`.
## Progress update - 2026-09-19 (document-content session contract slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`DocumentContentReference`, `DocumentContentSnapshot`, and
`DocumentContentSession` contract. References contain only bounded token, path,
or URI text; the snapshot retains the explicit `Idle`, `Requested`, `Loading`,
`Ready`, `Failed`, and `Released` phases, optional bounded reference metadata,
and an optional structured failure. Blank and oversized references are rejected
without mutating the current snapshot.

Requests replace only an idle, failed, or released session. Replacement while
requested, loading, or ready returns a recoverable conflict until the caller
releases the active session. Loading, ready, and failure events are accepted
only from their exact predecessor phases. Failure retains only the bounded
reference metadata and structured error. `release()` is the explicit boundary
at which the viewer/platform adapter must release its document object and
content bytes; the application projection then clears its reference and error.
Requests after release are supported, while late events after released or
other terminal phases are rejected without snapshot mutation.

`NextApplicationDocumentContentTests` covers lifecycle transitions, validation,
structured failure, release/re-request, late-event immutability, copy/equality,
and the no-content-bytes/no-QtPdf boundary without constructing a
`QApplication`. The remaining Phase 2 work is the application-to-legacy mapping
and coordinator integration for worker-thread ownership plus end-to-end
cancellation request/acknowledgement; the current job contracts define those
event boundaries but do not yet provide the runtime thread bridge.
## Progress update - 2026-09-19 (import-job event bridge/coordinator slice)

`ClassMngrNext::Application` now owns a synchronous, Qt-free
`ImportJobCoordinator` with an adapter-neutral worker port and a bounded,
thread-safe FIFO event sink. Generation-tagged progress, completion,
failure, and cancellation-acknowledgement events are applied only when pumped;
stale generations and late terminal events cannot mutate a restarted/current
snapshot. Queue overflow and worker-start failures retain structured errors,
and focused app-less tests cover FIFO ordering and completion-vs-cancellation
semantics. Actual worker implementations and legacy adapters remain outside
this slice.
## Progress update - 2026-09-19 (workspace lifecycle coordinator slice)

`ClassMngrNext::Application` now owns a synchronous, Qt-free
`WorkspaceCoordinator` that composes the workspace use case, workspace state,
and current-selection state. Create/open calls guard dirty replacement before
the gateway, commit only valid gateway sessions, and clear selection after a
successful transition. Close uses the current session, guards dirty or closed
state before the gateway, and clears selection only after a successful close;
gateway failures and invalid returned sessions leave both snapshots unchanged.
`NextApplicationWorkspaceCoordinatorTests` covers these transitions with a fake
gateway and `QTEST_APPLESS_MAIN`, without legacy service references or a
`QApplication`.
## Progress update - 2026-09-19 (workspace persistence coordinator slice)

`WorkspaceCoordinator` now exposes synchronous save, save-as, and export
operations. Each operation requires an open session and returns structured
`NotFound` without calling the use case when the workspace is closed. Save
commits the clean state only after a successful gateway result. Save-as passes
validation through `WorkspaceUseCase`, then atomically replaces only the
current session's location and marks it clean after a valid gateway-returned
location; gateway failures and invalid returned locations leave the workspace
and selection snapshots unchanged. Export propagates the use-case result and
does not mutate either snapshot.

`WorkspaceState::markSavedAs` validates before assignment, preserving the
workspace id and keeping the replacement/clean transition atomic. The
app-less coordinator and state tests cover success, failure, closed/no-call,
validation, invalid returned locations, selection preservation, and the
existing lifecycle contract. No Qt, widget, singleton, legacy, thread, or
adapter code is part of this slice.

Windows x64 Debug verification passed with `cmake --preset
windows-x64-debug`; configure-time source ownership validated 673 handwritten
files, the focused coordinator/state CTest run passed 2/2, and the full
`ClassMngrNext*` selection passed 10/10. The resource manifest checker also
passed for six RCC packs, seven runtime IDs, and seven runtime references;
the generated Qt-link manifest keeps `ClassMngrNext` limited to `Qt6::Core`.
## Progress update - 2026-09-20 (legacy application mapping slice)

[`phase2-legacy-application-mapping.md`](phase2-legacy-application-mapping.md)
records the verified `ApplicationServices`/`FileController` boundary, maps
workspace lifecycle and persistence to the existing v2 gateway/use-case,
coordinator, workspace-state, and selection-state contracts, and assigns the
outer-adapter duties and reversible migration gates. No legacy adapter,
FileController cutover, runtime Qt worker bridge, or feature-service migration
was added. Remaining Phase 2 work is to implement and verify those outer
boundaries, then migrate feature services as separate future slices.
## Progress update - 2026-09-20 (document-catalog metadata projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`DocumentCatalogProjection` containing bounded folder metadata and document
entries. Folder and document identifiers are distinct domain types; the
factory rejects blank or oversized metadata and references, invalid typed ids,
unknown folder links, duplicate ids, collection overflow, negative ordering,
and inconsistent exportability. Optional export references preserve absent vs
present state and use the existing `DocumentContentReference` metadata
boundary; no content bytes, viewer objects, or legacy ownership cross it.

The projection owns copied strings, identifiers, flags, and references only.
Adapters may release parser/catalog source after construction, UI consumers may
copy and release snapshots, and the adapter remains responsible for document
and export bytes plus viewer release through the content-session boundary.
`NextApplicationDocumentCatalogTests` covers valid metadata, category
distinction, validation/duplicates, optional export state, copy/equality, and
the adapter-neutral surface without constructing a `QApplication`.
## Progress update - 2026-09-20 (document-catalog use-case slice)

`ClassMngrNext::Application` now owns a Qt-free `DocumentCatalogUseCase` that
composes a caller-owned const `DocumentCatalogProjection` with a mutable
`DocumentContentSession`. It returns copied metadata or structured `NotFound`,
delegates primary and optional-export requests to the existing content-session
contract, and returns its generation token without retaining content bytes,
viewer objects, Qt types, or mutable projection state. App-less tests cover
lookup/copy, no-mutation not-found paths, exact reference selection, token and
conflict/release behavior, error propagation, and copyable metadata results.
## Progress update - 2026-09-20 (legacy workspace gateway seam slice)

`ClassMngrNext::Platform` now owns an injectable, Qt-free
`LegacyWorkspacePort` and stateless `LegacyWorkspaceGateway`. The gateway maps
create/open/close/save/save-as/export requests and legacy text/status results
to the existing `Application::WorkspaceGateway`, validates returned handle and
location postconditions, and preserves typed workspace identity without
retaining mutable sessions or service pointers. The explicit create-or-open
hook documents that concrete outer adapters own `QString`/`QFile` conversion
and file preparation/removal. App-less fake-port tests cover the mapping,
failure/no-mutation behavior, and the platform dependency boundary; concrete
legacy Qt conversion and FileController cutover remain later work.

Windows x64 Debug verification passed: configure-time ownership/dependency
checks validated 695 handwritten sources with `ClassMngrNextPlatform` limited
to `ClassMngrNext::Application`; the focused gateway test passed 1/1 and the
full `ClassMngrNext*` plus launch selection passed 21/21. The resource-pack
manifest check passed for six RCC packs, seven runtime IDs, and seven runtime
references; the generated Qt-link report keeps `ClassMngrNext` at
`Qt6::Core`.
## Exit-gate status after F48 - 2026-09-25 (commit `2055bbb5f74842e4f146a48e211df58e65908b6b`)

This audit applies the formal criteria above to the verified F36/F38/F39/F40/F41/F42/F43/F44/F45/F46/F47/F48 evidence. F48's independent fresh-build and focused-test evidence is recorded below; no tests were rerun for this documentation update.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F40 adds app-less `Domain::Weekday`/`Domain::ScheduleTime`; F41 and F43 add Schedule and Class Transfer review-decision behavior, F44 adds Teacher Import review-decision validation, F45 adds `Domain::Course`, F46 adds `Domain::KoreanTeacherKey`, F47 adds typed `Domain::Course::WeeklyMeetingDayRule`, and F48 adds typed `Domain::ScheduleEntry` with distinct ClassId/TeacherId types and validated ScheduleTime. Broader Domain records remain incomplete. |
| Baseline parity | Partial | Required fixtures cover F36 Calendar import, F38 Schedule preview, F39 conflict review/apply, F41 Schedule review through persisted apply, F43 Class Transfer persistence/conflict, F44 Teacher Import review through persisted apply, F45 valid Schedule apply/invalid-course rejection, F46 Teacher Import and Schedule matching/persistence plus overlap rejection, F47 accepted Schedule persistence plus prohibited-pattern rejection before writes, and F48 persistence facts checked against resolved typed entries. F37's pre-write sentinel and F39/F43/F44/F45/F46/F47/F48 no-write cases add state evidence; F48 also verifies overlap rejection preserves seeded teacher, class, class_info, class_times, and app_settings snapshots. Wider baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal criterion is the `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` behavior stated above: dirty replacement is rejected before the gateway, success opens `WorkspaceState` and clears `SelectionState`, and gateway/invalid-session failures preserve both snapshots. The focused app-less workspace CTests verify these cases. F42 additionally verifies failed production replacement-open preservation and abort-before-target-preparation if close fails; F43/F44/F45/F46/F47/F48 do not change this criterion. FileController integration limitations remain separate and non-gating. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The `src/next` source scan and target dependencies found no direct `DataService`, `MainWindow`, `PageManager`, or widget-pointer dependency. F43's Class Transfer, F44's Teacher Import, F45/F47's `Domain::Course`, F46's `Domain::KoreanTeacherKey`, and F48's `Domain::ScheduleEntry` contracts have no Qt or legacy Application dependencies; outer adapters bridge legacy services, and `FileController` remains MainWindow-aware. |

Gate 1 (app-less Domain/Application behavior) and Gate 2 (baseline parity)
advance with F48 but remain Partial. F48 was independently verified using
fresh x64 MSVC 19.51.36257/Ninja 1.13.2/CMake 4.4.2/Qt 6.12.0 builds; both
focused CTests passed 2/2. Direct QtTest results were Domain 17/0/0 and Schedule
Import 26/0/1, with only the optional external-workbook sample skipped. The
required `schedule_review.xlsx` path checked persisted SQL values against
resolved `ScheduleEntry` values, and the expanded overlap-rejection test
confirmed all five seeded snapshots remained unchanged. CMake ownership covered
902 handwritten files; `git diff --check` passed and the user-owned
`cmake/sources.cmake` SHA-256 remained unchanged. Remaining work includes wider
baseline parity, shared workbook decoding, broader Domain records, generic
settings persistence, remaining feature-service migrations, and broader
calendar/UI and document work. The Sub Prep interval query remains capped at
the current and following calendar years at most.
Non-gating FileController integration gaps remain: dirty-page approval still
comes from `MainWindow`; New Profile and Initial Setup replacement can close the
active session before target preparation and coordinator create have succeeded;
and same-path replacement plus the complete live MainWindow snapshot lack direct
coverage. Phase 2 remains In Progress and the formal exit gate remains open.
## Exit-gate status after F49 - 2026-09-25 (commit `6a41e958671b7fa93c301d8b25c9c4381178fd7f`)

This audit adds the independently verified F49 Calendar Import use-case
evidence to the F48 audit above; no tests were rerun for this documentation
update.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F49 adds a Qt-free use case that composes the existing signature-query port, duplicate planner, and batch-save port. Six fake-port cases cover ordering, duplicate handling, parser skips, empty and duplicate-only candidates, exact UTF-16 signatures, and query/save failures. Broader Domain records remain incomplete. |
| Baseline parity | Partial | The required `calendar_import_parity_2026.xlsx` fixture exercises the modified production service and verifies persisted rows and counts. This advances Calendar Import parity; wider baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` criterion and its prior focused app-less coverage are unchanged by F49. FileController integration limitations remain separate and non-gating. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The F49 use case remains Qt-free and composes adapter-neutral ports; workbook/network/campus handling and localized errors stay at the feature edge. Existing audited `src/next` isolation remains satisfied. |

F49 was independently checked in fresh Executor and Tester builds; both
focused CTests passed 2/2, including the production fixture path. No full suite
was run. Wider baseline parity, shared workbook decoding, broader Domain
records, generic settings persistence, remaining feature migrations, and
broader calendar/UI and document work remain open. Phase 2 remains In Progress
and the formal exit gate remains open. Sub Prep remains capped at the current
and following calendar years at most.
## Exit-gate status after F50 - 2026-09-25 (commit `92d001db11d8c8eb973de5f238444abe855ea5c5`)

This audit adds the independently verified F50 signature-identity and
Calendar Import fixture evidence to the F49 audit above; no tests were rerun
for this documentation update.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F50 adds a Qt-free six-field signature value shared by the parser and query port. App-less cases verify formatting, all fields, exact UTF-16 code units, `%2` title text, and type members without metadata. This advances a narrow Calendar Import behavior contract; broader Domain records remain incomplete. |
| Baseline parity | Partial | Required `calendar_import_parity_2026.xlsx` production parity passed through the Calendar Import path. This advances fixture-backed import parity; wider baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal workspace create criterion and its prior coverage are unchanged by F50. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The shared signature value and query contract remain Qt-free; string normalization and date conversion remain at the Qt adapters. Existing audited `src/next` isolation remains satisfied. |

Executor and independent Tester each freshly configured Windows x64 MSVC/Ninja,
validated 906 source owners, built three focused targets, and passed CTest 3/3,
including the required fixture path. No full suite was run. Wider baseline
parity, shared workbook decoding, broader Domain records, generic settings,
remaining feature migrations, and broader calendar/UI and document work remain
open. Phase 2 remains In Progress with its exit gate Open. Sub Prep remains
capped at the current and following calendar years at most.
## Exit-gate status after F56 - 2026-09-26 (commit `c73e896fe34e186a045d73b653aa8ec9dfa89e83`)

This audit adds F56 speaking-evaluation grade-contract and production-path
evidence to the F50 audit above; no tests were rerun for this documentation
update.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | `Domain::SpeakingEvaluationGrade` represents six criteria and C/B/B+/A/A+ values, parses exact labels, and centralizes legacy aggregation, rounding, invalid/missing outcomes, and clamping. Exhaustive valid-combination and boundary coverage adds evidence; broader Domain/Application behavior remains incomplete. |
| Baseline parity | Partial | Roster widget import verifies a padded saved label, incomplete evaluation to N/A, retained 16/6-to-B+ result, persistence, and idempotence. The report widget path verifies B+ and N/A through assembly/rendering. Wider baseline parity remains incomplete. |
| Workspace boundary | Satisfied | F56 does not change the formal workspace create criterion or its focused app-less coverage. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The new grade contract is Qt-free; F56 does not change the audited `src/next` dependency-isolation finding. |

Fresh independent MSVC 19.51/Ninja/Qt 6.12 verification validated 909
handwritten source owners, built `ClassMngrNextDomainContractTests`,
`ClassMngrRosterEditorWidgetImportTests`,
`ClassMngrSpeakingEvaluationServiceTests`, and
`ClassMngrSpeakingEvalReportWidgetTests`, and passed exact CTest 4/4. No full
suite was run. Gate 1 and Gate 2 remain Partial; Phase 2 remains In Progress
with its exit gate Open. Sub Prep remains capped at the current and following
calendar years at most.
## Cumulative exit-gate status after F58 - 2026-09-26 (commit `9b9183818fc2163d625a8ffb088a492a4aa631a9`)

This audit carries forward the verified F48-F57 findings and adds F58; no
checks were rerun for this documentation update.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F48 adds typed schedule entries; F54-F57 add shared event, course-grade, speaking-grade, and evaluation-period rules; F58 adds typed teacher/class matching identities and an optional class suggestion. Focused contracts exercise these rules, but broader Domain/Application behavior remains incomplete. |
| Baseline parity | Partial | Fixture-backed evidence includes typed Schedule Import persistence/no-write checks, roster-score persistence, and F58's checked-in `schedule_review.xlsx` preview/apply path against a seeded database. F58 preserves matching order, ranks, confidence, explanations, fallback and legacy edge behavior; broader baseline parity remains incomplete. |
| Workspace boundary | Satisfied | F58 does not change the formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` criterion or its focused app-less coverage. FileController integration caveats remain non-gating. |
| v2 dependency isolation | Satisfied in the audited v2 scope | F58's matching contract uses typed Domain identifiers and an optional suggestion; the legacy repository adapter owns conversion to integer preview IDs. The audited `src/next` isolation finding remains unchanged. |

Fresh independent x64 Ninja/MSVC 19.51/Qt 6.12 verification validated 912
handwritten source owners, built the matching projection and
`ClassMngrScheduleImportTests`, and passed exact CTest 2/2, including
`previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` with the checked-in
fixture. Executor focused CTest passed 1/1. No full suite was run. At F58,
there was no direct production assertion for the adapter's no-suggestion `-1`
sentinel; F63 closes this specific gap. The data model retained that default
and app-less contract tests asserted an absent optional suggestion. Gate 1 and Gate 2 remain Partial; Phase 2 remains
In Progress and its exit gate Open. Sub Prep remains capped at the current and
following calendar years at most.
### Progress update - 2026-09-20 (class-summary projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ClassSummaryProjection` with bounded `TeacherSummary` records, compact
`ClassSummary` navigation rows, and at most one `SelectedClassDetails` value.
The factory rejects blank or oversized required text and typed identifiers,
duplicate IDs, unknown teacher references, invalid selected-class identity,
collection overflow, and student-count overflow. An absent class teacher is
an explicit supported fallback; present references must resolve in the
`TeacherSummaryIndex`. Lookups return value copies, while the projection
retains no rosters, repositories, widgets, page pointers, or rich record
graph. The query/adapter owner may release rich source data after creation;
app-less focused tests cover the 96-class scale, metadata, validation,
copy/equality, missing-teacher behavior, and the ownership boundary.
### Progress update - 2026-09-20 (schedule-view projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ScheduleViewProjection` made of bounded visible `ScheduleViewRow` and
`ScheduleViewCell` values. The factory validates nonnegative ordering/day/slot
values, unique row/cell identities, bounded labels and display text, optional
typed class/teacher references, regular/intensive mode, and row/cell caps.
Empty snapshots, rows, and time-slot cells are explicit valid fallbacks;
lookups return value copies. Query owners must paginate or stage larger
visible scopes and may release rich classes, rosters, repositories, and raw
workbook data after projection creation. App-less tests cover the 96-row /
768-cell scale, ordering, missing references, invalid input, caps, and the
no-rich-record contract boundary.
### Progress update - 2026-09-20 (staged class-transfer projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`ClassTransferProjection` with bounded flat teacher/class records, source-key
matching, optional missing-teacher references, and separate regular/intensive
time collections. The deterministic factory rejects blank or oversized text,
duplicate or unknown keys, negative ordering, and teacher/class/time/package
collection overflow with structured `InvalidInput`; lookups return value
copies. Reader adapters stage only this compact package, release raw source
representations before projection handoff, and must paginate larger inputs.
App-less tests cover the boundary, caps, categories, fallback, equality, and
no external-owner/raw-source accessors; legacy transfer models and codecs are
outside this slice.
### Progress update - 2026-09-20 (campus directory metadata projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`CampusDirectoryProjection` of bounded `CampusSummary` navigation metadata:
typed `Domain::CampusId`, display name, short key, address, optional notes,
nonnegative order, and active state. Deterministic create/validate rejects
blank or oversized required fields, invalid optional notes, duplicate IDs or
keys, negative order, and entry overflow with structured `InvalidInput`;
empty directories are valid and lookup misses return `nullopt`. ID/key
lookups return value copies. Query/adapters must paginate or stage above the
cap and may release rich campus/location records after projection creation;
the projection has no Qt, repository, widget/page, pointer, or raw-byte state.
The app-less test covers 96 entries, exact-cap acceptance, validation,
copy/equality, metadata retention, and the ownership boundary.
### Progress update - 2026-09-20 (calendar-event projection slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`CalendarEventProjection` of bounded `CalendarEventSummary` event-list
metadata. The contract retains typed calendar-event IDs plus optional typed
class/campus references, bounded title/date/time/location/notes/`eventType`/
`timeStatus` text, an optional bounded `repeatSeriesId`, nonnegative
ordering, and an explicit all-day policy: all-day events omit times, while
timed events may omit both unknown times but not a partial range. Dates and
times remain opaque adapter-neutral text.

Deterministic create/validate rejects blank or oversized identifiers and
required fields, invalid optional references or time combinations, duplicate
IDs, negative ordering, and collection overflow with structured
`InvalidInput`; empty projections and value-copy ID lookups are explicit.
Query/adapter owners release rich calendar records, recurrence state, and
service data after projection creation and stage or paginate above the cap.
`NextApplicationCalendarEventTests` covers 96-scale metadata, typed
references, temporal policy, validation, caps, copies/equality, safe lookups,
and the no-rich-record boundary without constructing a `QApplication`.
### Progress update - 2026-09-20 (user-preferences state contract slice)

`ClassMngrNext::Application` now owns a Qt-free, copyable
`UserPreferencesSnapshot`/`UserPreferencesState` for explicit theme and
language preferences, sidebar display flags, teacher and PowerPoint notice
flags, automatic update checks, supported Excel timeout values, and an
optional validated typed `Domain::CampusId` last selection. Defaults match
the verified legacy defaults; invalid enum, timeout, and campus updates return
structured `InvalidInput` errors without mutating the snapshot, and campus
selection is explicitly clearable. The contract intentionally does not retain
the separate legacy JSON campus key: the later outer adapter owns any mapping
between storage keys and the typed campus identifier. App-less tests cover
defaults, all values, validation, copies/equality, value-copy access, and the
absence of persistence, UI, singleton, and legacy-service dependencies.
### Progress update - 2026-09-20 (ApplicationServices workspace-port slice)

`src/next/platform/application_services_workspace_port.h` now implements
`ApplicationServicesWorkspacePort` behind `LegacyWorkspacePort`. UTF-8/path
normalization, explicit non-destructive `createOrOpen`, close/open
postconditions, the void-save postcondition, save-as through the legacy call
plus reopen while preserving identity, export non-mutation, and legacy error
mapping are covered by real `ApplicationServices` + `QTemporaryDir` tests in
`tests/next_platform_application_services_workspace_port_tests.cpp`.

Configure/ownership checks validated 697 files; the `ClassMngrNextPlatform`
dependency remains limited to `ClassMngrNext::Application`. The focused
build/CTest passed 1/1 with all 9 slots; at that point `FileController`
remained untouched. Invalid UTF-8 and stale/closed save-as/export boundaries
are non-blocking but untested. FileController integration, the runtime
bridge, and feature-service migration remained for subsequent slices.
### Progress update - 2026-09-20 (FileController workspace lifecycle slice)

`FileController` now owns the concrete
`ApplicationServicesWorkspacePort` -> `LegacyWorkspaceGateway` ->
`WorkspaceUseCase` -> `WorkspaceCoordinator` composition when services are
available, with an out-of-line destructor that permits incomplete service
types. `loadDatabase` closes the current coordinator session before opening
the replacement and returns `false` when that close fails. `closeFile` now
preserves the current file and UI state when coordinator close fails. The
structured open error is converted with explicit UTF-8 decoding. A null
`ApplicationServices` pointer remains safe, and create/initial-setup opens
retain the legacy fallback path.

`FileControllerWorkspaceLifecycleTests` is an offscreen QTest using public
`loadDatabaseOnStartup`, real `ApplicationServices`, and valid
`QTemporaryDir` workspaces. It asserts normalized recent/last-file settings,
missing-file warning capture, legacy open error text, non-empty null-service
safety, and sequential coordinator/legacy fallback. There is no direct
injected FileController close-failure/legacy-operation integration test;
lower-layer tests and source inspection cover that behavior.

The final independent bounded Debug CTest selection passed 9/9: FileController
lifecycle, startup visual settings, data service lifecycle, startup
performance, legacy workspace gateway, ApplicationServices workspace port,
and the workspace coordinator-related targets. Focused build, configure,
source-ownership, and dependency checks passed, and `git diff --check` passed.

Save, save-as, and export remain legacy. The runtime bridge and feature-service
slices remain open.
### Progress update - 2026-09-20 (FileController create/initial-setup coordinator slice)

`FileController` now keeps explicit ownership of normal-create replacement
(`QFile::remove`) and initial-setup backup rename/cleanup, checks
`closeActiveDatabase()` before either destructive preparation, and routes the
successful prepared path through `WorkspaceCoordinator::createWorkspace` with
an explicit UTF-8 `WorkspaceLocation`. Structured domain errors are decoded
with `QString::fromUtf8` before the existing warning title/message policy is
used. Initial-setup cancellation now also leaves its path and UI state intact
when close fails. Save, save-as, and export calls remain on
`ApplicationServices`.

`FileControllerWorkspaceLifecycleTests` now uses `FakeFileDialogService`, fake
prompts, real `ApplicationServices`, and `QTemporaryDir` to cover normal
creation/recent updates, existing-target replacement, close-failure
non-destructive behavior, initial-setup backup/open/finish and cancel restore,
and structured create-error propagation while retaining the prior open/close,
recent, warning, and null-service coverage. Source inspection confirms the
legacy save/save-as/export call sites remain unchanged.

`cmake --preset windows-x64-debug` passed and validated one explicit target
owner for 698 handwritten source files; the CMake dependency guard and
generated Qt-link report keep `ClassMngrNext` at `Qt6::Core`. The focused
Debug target build passed, the focused lifecycle CTest passed 1/1, and the
bounded regression selection passed 9/9:
`ClassMngrStartupVisualSettingsTests`, `ClassMngrDataServiceLifecycleTests`,
`ClassMngrStartupPerformanceTests`, `ClassMngrNextApplicationContractTests`,
`ClassMngrNextApplicationStateTests`,
`ClassMngrNextApplicationWorkspaceCoordinatorTests`,
`ClassMngrNextPlatformLegacyWorkspaceGatewayTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests`. The resource-pack check
passed for six RCC packs, seven runtime IDs, and seven runtime references;
`git diff --check` passed.

The remaining gates are the legacy FileController save, save-as, and export
paths, including their stale/closed and invalid-UTF-8 boundary coverage, plus
the runtime worker-thread/cancellation bridge and later feature-service
migration slices.
### Progress update - 2026-09-20 (FileController save/autosave coordinator slice)

`FileController::saveDatabase` now preserves the existing no-service and
no-open early return, dispatches a coordinator-owned workspace session through
`WorkspaceCoordinator::saveWorkspace`, and reports structured failures with
the existing warning service/title policy (`Save Teacher Profile`). The shared
UTF-8 decoder is used for domain error text. A failed coordinator save does
not alter workspace state, current-file/UI state, or invoke a false clean
transition. When the v2 state is closed but `ApplicationServices` is already
open through the compatibility path, the historical void
`ApplicationServices::saveDatabase` fallback remains active. Save-as and
export remain on their legacy service calls.

`FileControllerWorkspaceLifecycleTests` retains the prior lifecycle/create
coverage and adds real `ApplicationServices`/`QTemporaryDir`/fake-prompt
coverage for coordinator save success, stale-session structured failure with
warning and repeated state preservation, and closed-v2 compatibility fallback.
Source assertions keep save-as/export on the legacy calls and reject v2
save-as/export dispatch in this slice.

`cmake --preset windows-x64-debug` passed, validating one explicit owner for
698 handwritten source files; the CMake dependency guard passed, and the
generated Qt-link report keeps `ClassMngrNext` at `Qt6::Core`. The focused
Debug target build passed, the focused lifecycle CTest passed 1/1, and the
bounded regression selection passed 9/9:
`ClassMngrStartupVisualSettingsTests`, `ClassMngrDataServiceLifecycleTests`,
`ClassMngrStartupPerformanceTests`,
`ClassMngrNextApplicationContractTests`,
`ClassMngrNextApplicationStateTests`,
`ClassMngrNextApplicationWorkspaceCoordinatorTests`,
`ClassMngrNextPlatformLegacyWorkspaceGatewayTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests`. `git diff --check` passed
(with only the existing LF-to-CRLF warnings from Git).

The remaining Phase 2 gates are FileController save-as/export migration and
their stale/closed/invalid-UTF-8 coverage, the runtime worker-thread and
cancellation bridge, and later feature-service migration. No feature gate or
runtime bridge was changed here; this slice is ready for its separate commit.
### Progress update - 2026-09-20 (FileController save-as coordinator slice)

`FileController::saveDatabaseAs` now preserves the existing normalization,
no-service/no-open guard, dialog request, and `Save Teacher Profile` warning
policy. When `WorkspaceState` owns a session it passes the normalized path as
an UTF-8 `WorkspaceLocation` to `WorkspaceCoordinator::saveWorkspaceAs`,
decodes structured UTF-8 failures without changing workspace, selection,
current-file, recent-file, or loaded UI state, and commits the returned
normalized location directly to `m_currentFile`, recent/last-file settings,
and loaded UI state. It does not reopen through `loadDatabase`. A closed v2
state with an already-open compatibility service retains legacy
`saveDatabaseAs` followed by `loadDatabase`; export remains on the legacy
service call.

`FileControllerWorkspaceLifecycleTests` retains the previous create/open/
close/save coverage and now deterministically exercises v2 save-as success and
location/recent/UI updates, stale-session structured failure and recovery,
closed-v2 legacy fallback, dialog policy, and source assertions that export
has not migrated.

Verification passed: `cmake --preset windows-x64-debug` validated one explicit
owner for 698 handwritten sources and the CMake dependency guards; the
generated Qt-link report keeps `ClassMngrNext` at `Qt6::Core`. The focused
Debug target build passed, focused CTest passed 1/1, and the bounded regression
selection passed 9/9 (`ClassMngrStartupVisualSettingsTests`,
`ClassMngrDataServiceLifecycleTests`, `ClassMngrStartupPerformanceTests`,
`ClassMngrNextApplicationContractTests`, `ClassMngrNextApplicationStateTests`,
`ClassMngrNextApplicationWorkspaceCoordinatorTests`,
`ClassMngrNextPlatformLegacyWorkspaceGatewayTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests`). `git diff --check` passed
with only the existing LF-to-CRLF warnings.

The remaining gates are FileController export migration and its boundary
coverage, the runtime worker-thread/cancellation bridge, and later
feature-service migration. No export implementation, platform adapter, v2
contract, memory document, or unrelated code changed in this slice; this slice
is ready for a separate commit.
### Progress update - 2026-09-20 (FileController export coordinator slice)

`FileController::exportDatabaseAs` now preserves native-output normalization,
the no-service/no-open guard, the existing export dialog, the `Export Teacher
Profile` warning title, and directory remembering. When `WorkspaceState` owns
a session it passes an explicit UTF-8 `WorkspaceLocation` to
`WorkspaceCoordinator::exportWorkspace`; success remembers only the returned
normalized destination directory. It does not change `m_currentFile`,
workspace or selection state, recent/last-file settings, or loaded UI state.
Structured failures decode their UTF-8 message before warning. When v2 state is
closed while `ApplicationServices` is already open, the legacy export call
remains the compatibility fallback.

`FileControllerWorkspaceLifecycleTests` retains all prior lifecycle, create,
save, and save-as coverage and adds FakeFileDialogService/real
ApplicationServices/fake-prompt coverage for v2 export success and
non-mutation, repeated stale-session failure and warning preservation, legacy
compatibility fallback, dialog policy, and the no-open guard. Source checks
continue to assert that save and save-as remain on their migrated paths and
that the legacy export call remains available only as the fallback branch.

Verification passed: `cmake --preset windows-x64-debug` validated one
explicit owner for 698 handwritten source files and the CMake dependency
guards; `build/windows-x64-debug/reports/qt-module-links.json` keeps
`ClassMngrNext` at `Qt6::Core`. The focused Debug target
`ClassMngrFileControllerWorkspaceLifecycleTests` built successfully, focused
CTest passed 1/1, and the bounded regression selection passed 9/9
(`ClassMngrStartupVisualSettingsTests`,
`ClassMngrDataServiceLifecycleTests`, `ClassMngrStartupPerformanceTests`,
`ClassMngrNextApplicationContractTests`, `ClassMngrNextApplicationStateTests`,
`ClassMngrNextApplicationWorkspaceCoordinatorTests`,
`ClassMngrNextPlatformLegacyWorkspaceGatewayTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests`). The resource-pack check
validated 6 RCC packs, 7 runtime IDs, and 7 runtime references. `git diff
--check` passed with only the existing LF-to-CRLF warnings.

The remaining Phase 2 gate is later feature-service migration. No platform
adapter, v2 contract, memory document, or unrelated feature changed in this
slice; the worktree remains uncommitted.
### Progress update - 2026-09-20 (Qt runtime worker/cancellation bridge slice)

`ClassMngrNext::Platform` now has QtCore-only `QtJobWorkerLifetime` plus typed
`QtImportJobWorker` and `QtReportJobWorker` adapters over the existing Qt-free
`Application` worker ports. Work runs on joined, non-detached `QThread`s;
cancellation is cooperative and asynchronous; generation-tagged events are
posted to bounded queues; owner coordinators remain the only state mutators;
task, exception, and post failures are structured and observable; and
destruction joins before releasing task/sink captures.

Focused deterministic QtTest coverage for both adapters includes off-thread
execution, completion, cancellation, failure/exception, overflow visibility,
restart, late-cancel, and destructor join. Configure/ownership checks passed
for 701 sources with dependency guards; `ClassMngrNextPlatform` depends on
`ClassMngrNext::Application` and `Qt6::Core`, while `Application` remains
Qt-free. The generated `ClassMngrNextPlatformQtJobWorkerTests` CTest target
passed 1/1, the exact 9-target regression passed 9/9, and the resource report
validated 6 RCC packs, 7 runtime IDs, and 7 references. `git diff --check`
passed. An initial unsandboxed MSBuild FileTracker `E_ACCESSDENIED` required
an elevated retry and then passed. Stress/TSAN coverage and a direct report
queue-post-failure test remain non-blocking gaps.

The remaining Phase 2 gate is later feature-service migration; the worktree
remains uncommitted.
### Progress update - 2026-09-20 (document-catalog adapter and document-route slice)

`ClassMngrNext::Platform` now provides an
`ApplicationServicesDocumentCatalogPort` that maps legacy `DocumentCatalog`
metadata into bounded, typed `DocumentCatalogProjection` values.
`NavigationController` resolves only its document route through that port and
`DocumentCatalogUseCase`, while preserving confirm-leave behavior, the
`ResourcePaths` document lease, `PdfViewerDocumentDescriptor`, viewer loading,
and page navigation. `MainWindow`/`Sidebar` catalog ownership and document
content-byte/session loading remain legacy and open; this is not a full
document-service migration or Phase 2 completion.

Verification passed: configure/ownership checks validated 703 sources with
dependency guards for `Platform -> Application + Qt6::Core` and
`ClassMngrNext -> Qt6::Core`; focused CTest passed 1/1; the exact 9-target
regression passed 9/9; the resource report validated 6 RCC packs, 7 runtime
IDs, and 7 references; and `git diff --check` passed. A fresh MSBuild
FileTracker access-denied was an environment/toolchain issue; the isolated
build/regression passed. Malformed-catalog fixture injection remains a
non-blocking gap. The worktree remains uncommitted.
### Progress update - 2026-09-20 (document-content session runtime integration slice)

`DocumentContentSession` is integrated into the `PdfViewerPage` lifecycle for
descriptors carrying a content reference. `NavigationController` propagates
the projected reference. Request and `beginLoading` precede `QPdfDocument`
loading; Qt `Ready`/`Error` map to session state; and release follows
`QPdfDocument::close()` on replacement, navigation, and destruction. Direct
no-reference descriptors remain compatible with direct loading.

Tests cover Ready, failure, release, replacement, and new-generation paths.
Ownership/dependency configure passed at 703 sources; focused
PageManager/content/catalog checks passed 6/6; the exact nine-target
production regression passed 9/9; resource checks passed for 6 RCC packs, 7
runtime IDs, and 7 references; and `git diff --check` passed. An initial
MSBuild FileTracker `E_ACCESSDENIED` required an elevated rerun; the elevated
build passed. Remaining work includes resource/platform adapter completion,
Sidebar/MainWindow ownership, and other legacy service migration. Phase 2
remains in progress.
### Progress update - 2026-09-20 (bounded resource/platform document resolver slice)

Against baseline commit `e031317c`, the bounded resolver slice is complete.
`ClassMngrNext::Platform` now provides `DocumentContentResourcePort`, which
accepts `ResourcePackManager&`, validates `resource://documents/` references
including malformed, empty, and traversal rejection, acquires one
documents-pack lease, resolves primary and optional export paths, and returns
a move-only lease/path value. `NavigationController` uses this port instead of
directly acquiring or parsing `ResourcePaths::Documents`. `PdfViewerPage`
retains ownership of closing the PDF before releasing the lease, and
descriptors without a reference retain direct-loading compatibility. The
application layer remains Qt-free.

Configure/source-ownership/dependency checks passed at 705 sources; focused
resolver/navigation CTest passed 2/2; the exact nine-target CTest passed 9/9;
resource validation passed for 6 RCC packs, 7 runtime IDs, and 7 references;
and `git diff --check` passed with LF-to-CRLF warnings only. An initial MSBuild
FileTracker `E_ACCESSDENIED` required an elevated retry; the focused build/link
passed. Invalid UTF-8 and live UI integration lack direct coverage;
close-before-release is source-order verified. Sidebar/MainWindow catalog
ownership and other feature migrations remain open. Phase 2 remains in
progress.
### Progress update - 2026-09-20 (document-folder hierarchy metadata prerequisite slice)

After baseline commit `fd695fd`, `DocumentFolderMetadata` carries bounded
`parentPath` metadata, empty for roots, and projection validation handles it.
`ApplicationServicesDocumentCatalogPort` copies legacy
`DocumentFolderDefinition::parentPath`. This preserves nested hierarchy for
the upcoming `Sidebar`/`MainWindow` projection cutover while keeping the
application layer Qt-free and aggregate initialization compatible.

Configure/ownership/dependency checks passed at 705 handwritten files;
focused projection/adapter CTest passed 2/2; Qt-free application and
standalone syntax checks passed; and `git diff --check` passed. The embedded
fixture contains root folders only, so nested adapter transfer lacks runtime
coverage; nested projection behavior is covered. Phase 2 remains in progress;
at this prerequisite handoff, UI cutover was still pending. The subsequent
typed `Sidebar`/`MainWindow` cutover is recorded below; other feature migration
remains open.
### Progress update - 2026-09-20 (Sidebar/MainWindow typed catalog cutover)

After baseline commit `662e5f2`, the typed document-catalog projection is now
cut over at the Sidebar/MainWindow boundary. `Sidebar` owns a copied or
move-assigned `Application::DocumentCatalogProjection`; it has no legacy
`DocumentCatalog` pointer, include, or dependency. Its tree maps typed
`parentPath`, folder IDs, keys, and display names while preserving nested
hierarchy, order, localized labels, and empty-projection behavior.

`MainWindow::initializeSidebar` and `MainWindow::retranslateUi` request
locale-specific projections through
`Platform::ApplicationServicesDocumentCatalogPort` and pass them to Sidebar
by value. Projection failure supplies an empty projection. Configure,
ownership, and dependency checks covered 705 handwritten sources; Sidebar CTest
passed 1/1; adjacent catalog/projection/port tests passed 4/4; the exact
nine-target regression passed 9/9; and resource validation covered 6 RCC
packs, 7 runtime IDs, and 7 references. Application Qt-free and projection
standalone syntax checks passed. An elevated MSBuild retry was required after
`E_ACCESSDENIED`; the retry passed, and `git diff --check` passed.

There is no live MainWindow projection-failure/retranslation integration test;
static and production-compilation coverage is present, so this remains a
non-blocking gap. Phase 2 remains open for the remaining feature-service
migrations and is not complete.
### Progress update - 2026-09-20 (theme preference bridge slice)

After baseline commit `9f0d86d`, `ClassMngrNext::Platform::ThemePreferencePort`
explicitly maps typed `Application::ThemePreference`
(`SystemDefault`, `Light`, or `Dark`) to the legacy `ThemeService`.
`ThemeController` owns typed `UserPreferencesState`, synchronizes the
persisted `ActionRegistry` theme without reapplying it during action
connection, and applies valid changes through the port. Invalid input and
state updates remain atomic; valid changes preserve persistence, icon refresh,
and live palette behavior. `MainWindow` now passes an explicit `ThemeService`
reference. `schedule_output_controller.cpp` still read the legacy theme at
that baseline; the follow-on direct-theme handoff is recorded below.

Configure/ownership/dependency checks passed at 706 sources. Focused
`StartupVisualSettings` passed 1/1; the next preferences/launch targets passed
2/2; the exact nine-target CTest passed 9/9; and resource validation covered
6 RCC packs, 7 runtime IDs, and 7 references. Qt-free application checks
passed, and `git diff --check` passed with CRLF warnings. The Ninja/MSVC
fallback build passed after the environment/FileTracker issue. No dedicated
icon-pixel assertion exists; this is a non-blocking gap. Phase 2 remains in
progress.
### Progress update - 2026-09-20 (language preference bridge slice)

After baseline commit `3ad3ef1`, `ClassMngrNext::Platform::LanguagePreferencePort`
explicitly maps typed `Application::LanguagePreference`
(`SystemDefault`, `English`, or `Korean`) to the legacy `LanguageService`.
`LanguageController` owns typed `UserPreferencesState`, synchronizes the
persisted `ActionRegistry` language without reapplying it during action
connection, and applies valid changes through the port while preserving font
refresh, retranslation, and persistence behavior. `MainWindow` now passes an
explicit `LanguageService` reference. Generic settings persistence and other
feature-service migrations remain open.

Configure/ownership/dependency checks passed at 708 sources; focused
language/controller CTest passed 3/3; the exact nine-target CTest passed 9/9;
`LanguageService`/startup visual tests passed; and resource validation covered
6 RCC packs, 7 runtime IDs, and 7 references. Qt-free/raw-pointer checks and
`git diff --check` passed, and an elevated FileTracker retry passed. No live
`MainWindow::retranslateUi` assertion exists, failure rollback is not
deterministically exercised, and the nullable legacy `MainWindow`
`LanguageService` pointer has no null-construction coverage. Phase 2 remains
in progress.
### Progress update - 2026-09-20 (schedule output explicit-theme slice)

Against baseline commit `7f185ca`, `ScheduleOutputController` now receives
the resolved `Theme` explicitly and no longer includes or accesses
`ThemeService` or `currentTheme`. `ScheduleWidget` resolves the current theme
at the caller boundary and preserves the legacy Dark fallback when no theme
service is available. Existing settings-service username behavior,
print/save action selection, style/orientation selection, and show-English-
names behavior remain intact.

Focused `ScheduleWidget` and `SchedulePrintPdf` CTest passed 2/2. Tests cover
widget theme propagation/fallback and PDF `CurrentAppearance` Light/Dark
behavior while preserving explicit Light/Dark/Excel styles. The exact
nine-target regression passed 9/9; resource validation covered 6 RCC packs,
7 runtime IDs, and 7 references; `git diff --check` passed with CRLF warnings
only; and static controller review passed. Fresh configure could not find a
compiler in the verifier shell, but existing configured VS Debug artifacts
were current and passed. The schedule output direct-theme accessor is closed;
generic settings/application-services seams and other feature migrations
remain open. Phase 2 remains in progress and is not complete.
### Progress update - 2026-09-20 (calendar read-projection adapter slice)

Against baseline commit `08b86215`,
`src/next/platform/application_services_calendar_event_port.h` maps
`CalendarService::eventsInRange` into an owned, typed
`Application::CalendarEventProjection`. It copies bounded metadata and typed
IDs, and validates ordered valid ranges, unavailable service, technical
failures, invalid IDs/metadata, partial timed ranges, and projection capacity.
All-day and unknown-time policy remains explicit. No legacy pointers escape,
and the Application layer remains Qt-free. The malformed partial-time fixture
is inserted directly with `QSqlQuery` at the persistence boundary because
`CalendarService::saveEvent` correctly rejects malformed input.

The production header is registered in `cmake/next.cmake` and the focused test
is registered in `cmake/tests/next.cmake`. Focused adapter CTest passed 1/1;
existing `ClassMngrCalendarEventCacheTests` passed 1/1; the workspace control
test passed 1/1; and the exact existing nine-target regression passed 9/9 in
58.92s. Configure/ownership/dependency checks passed at 710 sources;
resource validation passed for 6 RCC packs, 7 runtime IDs, and 7 references;
strict UTF-8/encoding review passed; and `git diff --check` passed with only
LF-to-CRLF warnings. The focused build passed after an environmental
FileTracker `E_ACCESSDENIED` retry.

The calendar read-projection boundary is complete. The narrow typed
cache/model boundary is recorded below; broader typed calendar UI/page
migration remains future. The later worker-boundary handoff separates
database-query and worker ownership while leaving the legacy compatibility
conversions available. The metadata enrichment and its verification are
recorded below.
Generic settings and other feature migrations remain open. Phase 2 remains in
progress and is not complete.
### Progress update - 2026-09-20 (calendar-event projection enrichment slice)

Against baseline commit `695d1065`, `CalendarEventSummary` now owns bounded
`eventType` and `timeStatus` strings plus an optional bounded `repeatSeriesId`.
The projection remains Qt-free, copyable, bounded, typed-ID based, ordered,
and pointer-free; existing all-day/unknown-time behavior, order, ID,
capacity, and legacy mapped-field byte semantics remain intact.

`ApplicationServicesCalendarEventPort` maps and validates the new fields,
trims only `repeatSeriesId` for normalization, preserves surrounding
whitespace for existing title/date/time and other mapped fields, and returns
structured failures for blank, over-bounds, or malformed values. Application
and adapter tests cover bounds, validation, copy/equality/lookups, legacy
value/repeat-series preservation, malformed persisted input, and title
whitespace compatibility. CMake registrations remain unchanged; the narrow
typed cache/model boundary is recorded below, while broader typed calendar
UI/page migration remains future.

The elevated VS Debug build passed after an environmental FileTracker
`UnauthorizedAccessException` retry. Focused application, adapter,
calendar-cache, and workspace-control tests passed 1/1; the exact nine-target
regression passed 9/9; configure/ownership/dependency checks passed with 710
handwritten sources and one explicit owner; resource validation passed 6 RCC
packs, 7 runtime IDs, and 7 references; `git diff --check` passed with
LF/CRLF warnings only; and the static Qt-free/pointer review passed.

Phase 2 remains open. The typed projection now feeds the completed narrow
cache/model boundary, and the later worker-boundary handoff separates calendar
database-query/worker ownership while legacy compatibility conversions remain
for unchanged callers. Broader typed calendar UI/page migration, generic
settings, and other migrations remain open.
### Progress update - 2026-09-20 (typed calendar query-port slice)

Against baseline commit `0859cd82`, added
`src/next/application/calendar_event_query_port.h`, a Qt-free, value-only
application port for copied range/next-event requests and typed projection,
date, and error results. Adapted
`src/features/calendar/calendar_event_projection_query.h/.cpp` and
`CalendarEventCache` h/cpp to use a clear per-worker port/factory lifetime
while retaining unique SQLite ownership in the worker adapter and the existing
cache scheduling/public API.
Added `tests/next_application_calendar_event_query_port_tests.cpp` and
extended `tests/calendar_event_cache_tests.cpp`; the new application/test
sources are registered in `cmake/next.cmake` and `cmake/tests/next.cmake`,
while production-source and `data_and_imports` registrations remain unchanged.
UI/model/page call sites and unrelated modules remain untouched.

No Qt, `QObject`, service, repository, SQLite/QSql object, or legacy pointer
crosses the application port. Query parity remains covered for repeat-series,
multi-day membership, dedupe, retention, ordering/filtering, next-event
lookup, cancellation-by-invalidation, generation/stale-result rejection, and
errors. Legacy `eventsForDate`/`eventsInRange` compatibility conversions
remain for unchanged callers; the narrow typed cache/model boundary is
recorded below.

The elevated VS Debug query-port/cache build passed after an environmental
FileTracker `E_ACCESSDENIED` retry. Focused query-port/cache tests passed 2/2;
relevant calendar tests passed 4/4; the exact nine-target regression passed
9/9 in 58.91s; configure/ownership/dependency passed with 714 handwritten
sources and one owner each; resource validation passed 6 RCC packs, 7 runtime
IDs, and 7 references; `git diff --check` passed with LF/CRLF warnings only;
and static Qt-free/worker review passed. Failure-path SQLite cleanup is
statically verified but not runtime fault-injected.

Phase 2 remains open. The typed worker query-port and narrow cache/model
boundaries are complete while legacy page/upcoming compatibility callers
remain; broader typed calendar UI/page migration, generic settings, and other
feature migrations remain open.
### Progress update - 2026-09-20 (typed calendar cache/model cutover)

Against baseline commit `0b5b8eb6`, `CalendarEventCache` retains typed
`CalendarEventSummary` values and exposes the date-scoped
`eventProjectionForDate` accessor. `eventsForDate` and `eventsInRange` remain
legacy compatibility conversions. `CalendarEventModel` consumes the typed
projection and `CalendarEventSummary` for QML rows, converting dates, times,
and `QVariant` only at the UI boundary. Calendar pages and upcoming-event
callers remain unchanged.

Campus filtering adds the typed-summary path while retaining the necessary
legacy `CalendarEvent` compatibility wrapper for unchanged upcoming-page and
import-test callers; filtering semantics remain preserved. Tests cover typed
projection/model parity, ordering/filtering, all-day and unknown-time cases,
repeat metadata, legacy compatibility, generation/stale-result behavior, and
relevant calendar paths. No CMake changes were made.

The elevated VS Debug build passed after the `constFind` fix. Focused and
relevant tests passed 7/7; the exact nine-target regression passed 9/9 in
57.83s, including startup performance; configure/ownership/dependency checks
passed with 714 handwritten sources; resource validation passed for 6 RCC
packs, 7 runtime IDs, and 7 references; `git diff --check` passed with CRLF
warnings only; and static review passed for typed model/filter usage. The
non-blocking warnings are missing Vulkan headers and existing MSBuild
custom-build dependency warnings.

This closes the narrow typed cache/model boundary. Phase 2 remains open:
legacy page/upcoming callers remain, while broader typed calendar UI/page
migration, generic settings, and other feature migrations remain future work.
### Progress update - 2026-09-20 (typed upcoming-events read cutover)

Against baseline commit `88bd88dd`, `CalendarEventCache` adds a range-scoped
typed projection accessor while `eventsInRange` and `ensureNextTenEvents`
compatibility remain intact. `calendar_page_upcoming_events.cpp` migrates only
upcoming-event retrieval, filtering, date-time formatting, and row rendering
to `CalendarEventSummary`. `calendar_page_events.cpp`, edit dialogs, service
calls outside this path, and integer-ID activation remain unchanged.

Scope loading, retention, generation invalidation/stale cancellation,
dedupe, ordering, active-type/campus/start-of-term filtering, the ten-event
limit, display text, row IDs, edit navigation, all-day/unknown-time handling,
repeat metadata, and legacy parity remain preserved. The projection boundary
uses `events()`, `pop_back()`, and `empty()` correctly. Tests cover range
projection ordering/filtering inputs, metadata, typed/legacy parity, and the
relevant calendar/page paths; no CMake changes were made.

The elevated current-source VS Debug build passed; focused calendar tests
passed 3/3 and page tests 2/2; the exact nine-target regression passed 9/9 in
58.77s; configure/ownership/dependency passed with 714 sources; resource
validation passed 6 RCC packs, 7 IDs, and 7 references; `git diff --check`
passed with CRLF warnings only; and static review passed. No dedicated live
upcoming-page UI test exists.

This closes the narrow upcoming-events typed read path. Phase 2 remains open:
`calendar_page_events.cpp`, edit dialogs, other legacy callers, and broader
typed page migration remain future work, as do generic settings and other
migrations.
### Progress update - 2026-09-20 (final next-ten-events read cutover)

Against baseline commit `4de5c8c2`, the only implementation change was
`src/features/calendar/ui/calendar_page_events.cpp`.
`CalendarPage::ensureNextTenEvents` now uses
`eventProjectionInRange(...).events()` with the typed
`filterUpcomingEvents` overload; no `eventsInRange()` remains in that file.
`requestNextEventMonth`, loading guards, retention/invalidation, the ten-event
prefetch threshold, and surrounding page behavior remain unchanged. Legacy
`eventsInRange` APIs remain for other callers; edit/activation remains
integer-ID legacy.

The cutover changed no tests, CMake, or documentation; existing cache/page
coverage is reused. Elevated current-source VS Debug builds passed; focused
calendar/page tests passed 5/5; the exact nine-target regression passed 9/9 in
58.63s; configure/ownership/dependency passed for 714 sources; resource
validation passed 6 RCC packs, 7 IDs, and 7 references; `git diff --check`
passed with CRLF warnings only; static review passed; and exactly one file was
dirty. No dedicated live upcoming-row UI test exists; this is non-blocking.

The typed read-only calendar paths are now covered through upcoming retrieval,
next-ten prefetch, and the activation-read seam. Dialog/mutation/repeat
operations and other legacy callers remain future work, as do generic
settings and other migrations. Phase 2 remains open.
### Progress update - 2026-09-20 (typed calendar activation-read boundary)

Against baseline commit `a7498732`,
`src/next/platform/application_services_calendar_event_port.h` now exposes
typed `projectionById(int)`. It preserves ID, title, event type, status,
repeat-series, all-day, unknown-time, date, and time fields, and returns
structured unavailable, missing, invalid, partial-time, overflow, and
malformed-repeat failures. `calendar_page_events.cpp` reads activation data
through the adapter and converts the typed result to legacy `CalendarEvent`
only at the existing UI boundary before opening the unchanged dialog.

`tests/next_platform_application_services_calendar_event_port_tests.cpp`
covers valid by-ID field preservation, missing/unavailable/malformed cases,
and boundary checks. No CMake changes were made. CalendarEventDialog,
save/delete/repeat mutation operations, schedule settings, integer-ID
semantics, and other callers remain unchanged; invalid reads cause no
mutation.

The elevated current-source VS Debug build passed; focused tests passed
11/11; the exact nine-target regression passed 9/9 (57.90s startup, 60.68s
total); configure/ownership passed with 714 sources; dependency assertions
passed (9 production targets, `ClassMngrNext -> Qt6::Core`); resource
validation passed 6 RCC packs and 7 IDs/references; diff/static checks
passed; and exactly three scoped files were dirty. Non-blocking warnings were
missing Vulkan headers and existing MSVC `/FORCE`/duplicate-stub linker
warnings.

The typed activation-read seam is closed. Dialog/mutation/repeat operations,
other legacy callers, generic settings, and broader migrations remain future
work; Phase 2 remains open.
### Progress update - 2026-09-20 (typed single-event delete boundary)

Against baseline commit `0fe9ca9d`, added
`src/next/application/calendar_event_delete_port.h`, a Qt-free typed
`Result<void>` contract for `CalendarEventId`, and
`src/next/platform/application_services_calendar_event_delete_port.h`, the
legacy `CalendarService::deleteEvent` adapter. The platform header is
registered in `cmake/next.cmake`; the existing platform-port test target is
reused.

Only the non-repeat delete branch in `calendar_page_events.cpp` changed: it
converts the event ID at the UI boundary and calls the typed port. Save,
repeat-series deletion, `CalendarEventDialog`, schedule settings,
integer-ID semantics, warnings/errors, close/return behavior, and generation
invalidation remain unchanged. The platform-port tests now cover valid delete,
invalid ID, unavailable service, and injected failure; fixtures persist valid
09:00–10:00 events.

The elevated current-source Debug build passed; focused tests passed 11/11;
the exact nine-target regression passed 9/9 (63.22s; startup 60.38s);
configure/ownership passed with 716 sources; dependency assertions passed
(9 production targets, `ClassMngrNext -> Qt6::Core`); resource checks passed
6 RCC packs, 7 IDs, and 7 references; and diff/cached-diff/static checks
passed. The static checker recognized the `CalendarEventDeleteResult` alias;
no source issue was found.

The non-repeat single-event delete seam is closed; the repeat-series
suffix-delete boundary is recorded below. Save, repeat-series edit/save,
dialog ownership, generic settings, and other migrations remain future work;
Phase 2 remains open.
### Progress update - 2026-09-20 (typed repeat-series suffix-delete boundary)

Against baseline commit `d4c186be`, added
`src/next/application/calendar_event_series_delete_port.h` with a Qt-free
bounded request carrying `repeatSeriesId` and an ISO start date plus
`Result<void>`, and
`src/next/platform/application_services_calendar_event_series_delete_port.h`
over `CalendarService::deleteRepeatSeriesFromDate`. The platform header is
registered in `cmake/next.cmake`; the existing platform-port test target is
reused.

Only the `thisAndFollowing` branch in `calendar_page_events.cpp` changed.
Exact series/date semantics remain intact; successful deletion invalidates and
refreshes, while failures preserve the existing state and warnings. Save,
single-event delete, repeat-edit/save, dialog, schedule, and all other branches
remain unchanged. Tests cover valid suffix deletion, blank or overlong IDs,
invalid ISO dates, unavailable service, and injected legacy failure; the valid
fixture forwards the exact series ID and `2026-12-08` and preserves the
pre-cutoff event.

The elevated Debug build passed; focused port/calendar/page tests passed
11/11; the exact nine-target regression passed 9/9 in 59.69s (57.05s startup);
configure/ownership/dependency passed with 718 sources and 9 production
targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6 RCC packs,
7 IDs, and 7 references; diff, cached-diff, trailing-whitespace, and static
checks passed. Verification reported exactly five scoped implementation files
dirty. Existing Vulkan, Qt-zlib, and MSVC notices are non-blocking.

The typed non-repeat delete and typed repeat-series suffix-delete seams are
closed. Save, repeat-series edit/save, dialog ownership, generic settings, and
other migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-20 (typed non-repeat calendar-event save boundary)

Against baseline commit `e197891f`, added
`src/next/application/calendar_event_save_port.h`, a Qt-free typed
`CalendarEventSaveRequest` returning `Result<CalendarEventId>`, and
`src/next/platform/application_services_calendar_event_save_port.h`, which
maps through `CalendarService::saveEvents({event})`. The platform header is
registered in `cmake/next.cmake`; the existing platform-port test target is
reused.

Only the non-repeat save branch in `calendar_page_events.cpp` changed. The
repeat edit/save path and dialog behavior remain on their existing legacy
path; single-event delete, repeat-series suffix deletion, schedule behavior,
and other branches remain unchanged.

The executor and independent tester both reported a passing Debug build,
focused port tests 11/11, calendar tests 7/7, the exact nine-target regression
9/9, and launch 1/1. Configure/ownership passed with 718 sources; dependency
checks passed for 9 production targets (`ClassMngrNext -> Qt6::Core`);
resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime references;
diff, cached-diff, trailing-whitespace, and static checks passed. Existing
Vulkan, Qt-zlib, and MSVC notices are non-blocking.

The typed non-repeat save seam is closed. Repeat-series edit/save, dialog
ownership, generic settings, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-20 (typed this-and-following repeat-series edit/save boundary)

Against baseline commit `03c1fecc`, added
`src/next/application/calendar_event_series_edit_port.h`, a Qt-free
`CalendarEventSeriesEditRequest` with `CalendarEventSeriesEditResult` as
`Result<void>`, and
`src/next/platform/application_services_calendar_event_series_edit_port.h`.
The platform adapter loads the suffix through
`repeatSeriesFromDate(repeatSeriesId, startDate)`, computes the start-date
offset and edited duration, propagates the edited fields to each selected
occurrence, and persists the updated list through `saveEvents(updatedEvents)`.

Only the `thisAndFollowing` branch in `calendar_page_events.cpp` migrated to
the typed port. Recurrence selection, dialog behavior, single-event delete,
non-repeat save, and other calendar paths remain preserved; the adapter keeps
the legacy recurrence/persistence boundary outside the Qt-free request.

The executor and independent tester reported a passing Debug build, focused
port tests 11/11, calendar tests 7/7, the exact nine-target regression 9/9
including startup verification, and launch 1/1. Configure/ownership passed
with 722 sources; dependency checks passed for 9 production targets
(`ClassMngrNext -> Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime
IDs, and 7 runtime references; diff, cached-diff, trailing-whitespace, and
static checks passed. Existing Vulkan, Qt-zlib, and MSVC notices are
non-blocking.

The typed this-and-following repeat-series edit/save seam is closed. Dialog
ownership, generic settings, remaining repeat paths, and broader page,
document, and feature migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-20 (typed this-event-only repeat-occurrence save boundary)

Against baseline commit `963d4578`, only the
`repeatSeriesEvent && !thisAndFollowing` branch in
`calendar_page_events.cpp` now routes through the existing typed
`CalendarEventSavePort`. It clears `repeatSeriesId` before mapping the
occurrence ID and edited fields into the existing save request. The existing
warning and early-return behavior remains intact: failures return before
invalidation, while success preserves the existing refresh. The
this-and-following, new-repeat, delete, dialog, non-repeat, and other paths
remain unchanged.

The executor and independent tester reported a passing Debug build, focused
tests 11/11, the exact nine-target regression 9/9 including startup
verification, and launch 1/1. Configure/ownership passed with 722 sources;
dependency checks passed for 9 production targets (`ClassMngrNext ->
Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime
references; static, diff, cached-diff, and trailing-whitespace checks passed.
Existing Vulkan, Qt-zlib, and MSVC notices are non-blocking.

The typed this-event-only repeat-occurrence save seam is closed. Dialog
ownership, generic settings, remaining repeat paths, and broader page,
document, and feature migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-20 (typed new-repeat series creation/batch save boundary)

Against baseline commit `f94f78fa`, added
`src/next/application/calendar_event_series_create_port.h`, a Qt-free
`CalendarEventSeriesCreateRequest`/port over typed occurrence save requests,
returning typed event IDs, and
`src/next/platform/application_services_calendar_event_series_create_port.h`.
The platform adapter copies the bounded series ID and occurrence fields into
one ordered legacy batch and makes one atomic `saveEvents(events)` call; the
typed result returns one `CalendarEventId` per saved occurrence.

Only the `dialog.repeatEnabled()` branch in `calendar_page_events.cpp` now
uses the series-create port. Existing `repeatedCalendarEvents` generation and
all typed edit/save/delete/dialog paths remain preserved. Daily, weekly, and
monthly recurrence parity is covered, and injected batch failure rolls back
without partial rows.

The executor and independent tester reported a passing Debug build, focused
tests 12/12, the exact nine-target regression 9/9 including startup
verification, and launch 1/1. Configure/ownership passed with 724 sources;
dependency checks passed for 9 production targets (`ClassMngrNext ->
Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime
references; static, diff, cached-diff, and trailing-whitespace checks passed.
Existing Vulkan, Qt-zlib, and MSVC notices are non-blocking.

The typed new-repeat series-create/batch-save seam is closed. Dialog
ownership, generic settings, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-20 (typed calendar-dialog edit-draft boundary)

Against baseline commit `2e2cc65c`, added the Qt-free bounded
`CalendarEventEditDraft`. `CalendarEventDialog::eventData()` now returns the
draft, while the legacy `CalendarEvent` conversion is private to the dialog as
`legacyEventData()`. `calendar_page_events.cpp` consumes the draft for all
existing typed save, series-create, and series-edit requests; no legacy Qt
value conversion is required at those application request boundaries.

Dialog defaults, validation, inline errors, warnings, repeat controls, delete
and mutation behavior, and page invalidation/refresh behavior remain
preserved. The draft boundary is additive; full dialog ownership migration
remains open.

The executor and independent tester reported a passing Debug build, focused
tests 12/12, the exact nine-target regression 9/9 including startup
verification, and launch 1/1. Configure/ownership passed with 725 sources;
dependency checks passed for 9 production targets (`ClassMngrNext ->
Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime
references; static, diff, cached-diff, and trailing-whitespace checks passed.
A non-blocking PTY/CreateProcess retry was required; existing Vulkan, Qt-zlib,
and MSVC notices are also non-blocking.

The typed calendar-dialog edit-draft boundary is closed. Dialog ownership,
generic settings, and broader page, document, and feature migrations remain
future work; Phase 2 remains open.
### Progress update - 2026-09-20 (typed calendar-dialog constructor/input ownership boundary)

Against baseline commit `d687bf69`, `CalendarEventDialog` now accepts and
stores the existing Qt-free `CalendarEventEditDraft` by value. The page maps
legacy activation and new-event values into a draft before construction;
legacy conversion helpers remain private to the dialog implementation, and
public `eventData()` continues to return the draft.

Defaults, validation, inline errors, warnings, delete and repeat controls,
`schedule_use_24h`, routing, and invalidation/refresh behavior remain
preserved. The typed draft continues through the existing save, series-create,
and series-edit paths without expanding legacy ownership. Verification covered
the four-file scope: `calendar_event_dialog.cpp`,
`calendar_event_dialog.h`, `calendar_page_events.cpp`, and
`tests/dialog_shell_tests.cpp`.

The executor and independent tester reported a passing Debug build, focused
tests 12/12, the exact nine-target regression 9/9 including startup
verification, and launch 1/1. Configure/ownership passed with 725 sources;
dependency checks passed for 9 production targets (`ClassMngrNext ->
Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime
references; static and diff checks passed. A static-checker correction was
required and then passed; existing Vulkan, Qt-zlib, and MSVC notices remain
non-blocking.

The typed calendar-dialog constructor/input ownership seam is closed. Full
dialog ownership, generic settings, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-20 (typed read-only schedule_use_24h settings bridge)

Against baseline commit `2d785250`, added the Qt-free
`ScheduleDisplayPreferences` value and read-only
`ScheduleDisplayPreferencesPort`, plus the Qt-boundary
`ApplicationServicesScheduleDisplayPreferencesPort`. The adapter preserves
the legacy `SettingsService` `schedule_use_24h` true/false behavior, including
the missing or unavailable-settings false fallback and legacy `QVariant`
coercion. Only the calendar and upcoming-event callers now use the bridge;
there are no writes or other settings changes, and date/time formatting is
unchanged.

The executor and independent tester reported a passing Debug build, focused
tests 14/14, the exact nine-target regression 9/9 including startup
verification, and launch 1/1. Configure/ownership passed with 728 sources;
dependency checks passed for 9 production targets (`ClassMngrNext ->
Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime IDs, and 7 runtime
references; direct-read, read-only, static, and diff checks passed.
`clang-format` and `clang-tidy` were unavailable; normal Vulkan, Qt-zlib, and
MSVC warnings remain non-blocking.

The typed read-only schedule display-preferences seam is closed. Residual
`SettingsService` callers, generic settings persistence, and broader page,
document, and feature migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-20 (typed five-key schedule-preferences persistence boundary)

Against baseline commit `2ebac724`, extended the existing Qt-free schedule
display-preferences contract and adapter to all five booleans:
`use24HourTime`, `showEnglishNames`, `showWeekends`,
`showAllIntensiveHours`, and `testingAffectsM1`. The port now exposes an
atomic typed save returning `Result<void>`; the adapter maps one bundle to the
legacy settings store and preserves rollback on failure.

Only `src/app/menu_builder.cpp` and
`src/features/schedule/ui/schedule_widget.cpp` use the five-key persistence
boundary. The legacy `ScheduleSettingsPreferences` compatibility files and
other callers remain intact; menu/widget rendering and controls, the existing
calendar `use24h` read, and all formatting behavior are preserved.

Serial full and focused builds passed. A transient parallel MSVC `LNK1163`
was non-blocking and cleared on the serial rerun. Focused tests passed 10/10;
the exact nine-target regression passed 9/9; launch passed 1/1.
Configure/ownership passed with 728 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Five-key, atomic-rollback,
call-site, Qt-free, and diff checks passed. `clang-format` and `clang-tidy`
were unavailable; normal Vulkan, Qt-zlib, and MSVC warnings remain
non-blocking.

The typed five-key schedule-preferences persistence seam is closed. Residual
settings callers, generic settings persistence, and broader page, document,
and feature migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-20 (typed excelImportTimeoutSeconds boundary)

Against baseline commit `b4fca5b1`, added the Qt-free bounded
`ExcelImportTimeoutPreferences` value and read/write
`ExcelImportTimeoutPreferencesPort`, with the
`SettingsManagerExcelImportTimeoutPort` adapter. The policy preserves default
120 seconds, supports only 30/60/120/300, normalizes invalid values to 120,
and round-trips the legacy `imports/excelTimeoutSeconds` key. Shared timeout
policy was narrowly centralized in `user_preferences_state.h` while the
existing Excel-specific and import-timeout APIs remain available.

Only `src/app/menu_builder.cpp`,
`src/features/teacher/ui/teacher_import_dialog.cpp`, and
`src/features/schedule/ui/schedule_import_dialog.cpp` use the typed boundary.
No worker or dialog workflow changed; existing import behavior and other
settings callers remain intact.

The executor and independent tester reported a passing Debug build, focused
tests 8/8, the exact nine-target regression 9/9, and a passing launch check.
Configure/ownership passed with 731 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Caller-scope, Qt-free,
normalization/round-trip, and diff checks passed. Normal warnings remained
non-blocking; `clang-format` and `clang-tidy` were unavailable.

The typed Excel import-timeout seam is closed. Residual settings callers,
generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-21 (typed sidebar display preferences boundary)

Against baseline commit `ea10176b`, added the Qt-free paired
`SidebarDisplayPreferences` value and `SidebarDisplayPreferencesPort`, plus
the `SettingsManagerSidebarDisplayPreferencesPort` adapter. It maps the exact
legacy keys `options/sidebarTooltipsEnabled` and
`options/sidebarMarqueeEnabled`, preserves enabled defaults for missing or
invalid values, retains legacy `QVariant` boolean coercion, and round-trips
both values. The legacy writes are void, so persistence failure is not
observable and save failure is not applicable at this boundary.

Only `src/ui/shared/actions/action_registry.cpp` is cut over. Checked states,
toggle persistence, unrelated actions, and existing sidebar rendering/control
behavior remain preserved; other settings callers are unchanged.

The executor and independent tester reported a passing Debug build, focused
tests 5/5, the exact nine-target regression 9/9, and launch 1/1.
Configure/ownership passed with 734 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Call-site, Qt-free,
static, and diff checks passed. `clang-format` and `clang-tidy` were
unavailable; normal warnings remain non-blocking.

The typed sidebar display-preferences seam is closed. Residual settings
callers, generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-21 (typed PowerPoint data-access notice boundary)

Against baseline commit `f462a162`, added the Qt-free
`PowerPointDataAccessNoticePreferences` value and
`PowerPointDataAccessNoticePreferencesPort`, plus the
`SettingsManagerPowerPointDataAccessNoticePort` adapter. It preserves the
exact `options/showPowerPointDataAccessNotice` key, an enabled default for
missing or unavailable values, legacy `QVariant` boolean coercion, and
round-trip behavior. The legacy write is void, so persistence failure is not
observable and save failure is not applicable at this boundary.

Only `src/ui/shared/actions/action_registry.cpp` and
`src/features/speaking_eval/ui/speaking_eval_batch_export_dialog.cpp` use the
typed preference. The `Q_OS_MACOS` confirmation/bypass before PowerPoint
export remains preserved, as does export behavior and non-Apple behavior.

The executor and independent tester reported a passing Debug build, focused
tests 9/9, the exact nine-target regression 9/9, and a passing launch check.
Configure/ownership passed with 740 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Qt-free, call-site,
static, and diff checks passed. The Apple-only runtime test was unavailable on
Windows; non-Apple guards passed. Normal warnings remained non-blocking;
`clang-format` and `clang-tidy` were unavailable.

The typed PowerPoint data-access notice seam is closed. Residual settings
callers, generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-21 (typed skipped-update-version persistence boundary)

Against baseline commit `7fdc315f`, added the Qt-free optional
`SkippedUpdateVersionPreferences` value and typed read/write/clear
`SkippedUpdateVersionPreferencesPort`, plus the
`SettingsManagerSkippedUpdateVersionPort` adapter. It maps the exact
`updates/skippedVersion` key, represents missing or empty storage as
`std::nullopt`, trims values on read and write, round-trips non-empty values,
and clears the setting for an empty optional.

Only `src/app/controllers/update_controller.cpp` uses the typed boundary.
`Version::parse` still validates and normalizes skipped versions before write;
skip/unskip, reconciliation clearing, prompt suppression, dialog display, and
the `QString` conversion boundary remain preserved.

The executor and independent tester reported a passing Debug build after a
transient MSVC `LNK1163` retry, focused tests 5/5, the exact nine-target
regression 9/9, and a passing launch check. Configure/ownership passed with
743 sources; dependency checks passed for 9 production targets
(`ClassMngrNext -> Qt6::Core`); resource checks passed 6 RCC packs, 7 runtime
IDs, and 7 runtime references. Qt-free, call-site, static, and diff checks
passed. Normal warnings remained non-blocking; `clang-format` and
`clang-tidy` were unavailable.

The typed skipped-update-version seam is closed. Residual settings callers,
generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-21 (typed recent-workspace history boundary)

Against baseline commit `3fc8ec4a`, added the Qt-free
`RecentWorkspaceHistory` value and `RecentWorkspaceHistoryPort`, plus the
`SettingsManagerRecentWorkspaceHistoryPort` adapter for the recent-file list
and last-file value only. UTF-8/Unicode and path normalization are preserved;
raw and normalized paths deduplicate, entries remain newest-first with a
ten-entry cap, and prune/clear behavior is retained. Missing or unavailable
storage remains empty, while `mostRecentDatabasePath()` prefers the list and
falls back to the last file.

`FileController` now routes recent/last-file reads and writes through the
typed boundary, preserving recent-menu and startup behavior. Last-directory
persistence and the workspace lifecycle remain unchanged; no direct raw
recent/last-file access remains in `FileController`.

The executor and independent tester reported focused tests 7/7 including
launch/startup verification, and the exact nine-target regression 9/9.
Configure/ownership passed with 746 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Qt-free, direct-call,
static, and diff checks passed. Normal warnings remained non-blocking;
`clang-format` and `clang-tidy` were unavailable.

The typed recent-workspace history seam is closed. Residual settings callers,
generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-21 (typed evaluation-default-policy boundary)

Against baseline commit `9fe09c65`, added the Qt-free
`EvaluationDefaultPolicy` contract with only `All` and
`CurrentOrPreviousTerm`, plus the
`ApplicationServicesEvaluationDefaultPolicyPort` adapter. It preserves the
exact `classes_navigation_evaluation_default_policy` key, stores the two
legacy values, and falls back to `All` for missing, invalid, or unavailable
settings.

Only `src/app/menu_builder.cpp` and
`src/features/classes/evaluation_default_selection_service.cpp` use the typed
boundary. Menu persistence and radio selection behavior remain preserved; the
other four class-navigation preference keys are untouched.

The executor and independent tester reported a passing Debug build, focused
tests 8/8, the exact nine-target regression 9/9, and a passing launch check.
Configure/ownership passed with 749 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Qt-free, call-site,
static, and diff checks passed. Normal warnings remained non-blocking;
`clang-format` and `clang-tidy` were unavailable.

The typed evaluation-default-policy seam is closed. Residual settings callers,
generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-21 (typed automatic-update preference boundary)

Against baseline commit `374461a0`, added the Qt-free
`AutomaticUpdatePreferences` value and `AutomaticUpdatePreferencesPort`, plus
the `SettingsManagerAutomaticUpdatePreferencesPort` adapter. It preserves the
exact `updates/automaticChecksEnabled` key, enabled defaults for missing or
invalid values, legacy `QVariant` boolean coercion, and round-trip behavior.
The legacy write is void, so persistence failure is not observable and save
failure is not applicable at this boundary.

Only `src/ui/shared/actions/action_registry.cpp` and
`src/app/controllers/update_controller.cpp` use the typed preference. Checked
state and toggle persistence remain intact. Automatic checks retain
`checkOnStartup` gating, while forced/manual checks, skipped-version and
prompt suppression, and update-dialog behavior remain preserved.

The executor and independent tester reported a passing Debug build, focused
tests 6/6, the exact nine-target regression 9/9, and a passing launch check.
Configure/ownership passed with 737 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Call-site, Qt-free,
static, and diff checks passed. Normal warnings remained non-blocking;
`clang-format` and `clang-tidy` were unavailable.

The typed automatic-update preference seam is closed. Residual settings
callers, generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-21 (typed middle-school analytics preference boundary)

Against baseline commit `1c8122bb`, added the Qt-free boolean
`MiddleSchoolAnalyticsPreferencesPort` and its
`ApplicationServicesMiddleSchoolAnalyticsPreferencesPort` adapter. The adapter
preserves the exact
`classes_navigation_show_middle_school_analytics_and_evaluations` key, false
default for missing or unavailable settings, legacy `QVariant` boolean
coercion, and round-trip persistence.

Only `src/app/menu_builder.cpp` and
`src/features/classes/ui/classes_page.cpp` use the typed preference. M1-M3
Analytics and Evaluations tab behavior remains preserved; visibility and reset
preference keys are untouched.

The executor and independent tester reported a passing Debug build, focused
tests 9/9 including launch, and the exact nine-target regression 9/9.
Configure/ownership passed with 752 sources; dependency checks passed for 9
production targets (`ClassMngrNext -> Qt6::Core`); resource checks passed 6
RCC packs, 7 runtime IDs, and 7 runtime references. Qt-free, call-site,
static, and diff checks passed. Normal warnings remained non-blocking; a
transient CMake regeneration issue was resolved.

The typed middle-school analytics preference seam is closed. Residual settings
callers, generic settings persistence, and broader page, document, and feature
migrations remain future work; Phase 2 remains open.
### Progress update - 2026-09-21 (typed class day-filter reset-policy boundary)

Against baseline commit `4be0af93`, added the Qt-free
`ClassDayFilterResetPolicy`/`ClassDayFilterResetPolicyPort` contract and its
platform adapter for the exact
`classes_navigation_day_filter_reset_policy` key. It maps
`OnApplicationClose` and `OnPageLeave`, preserves round-trip persistence, and
falls back to `OnApplicationClose` for missing, invalid, or unavailable
settings.

Only the day-filter radio controls in `menu_builder.cpp` and the day-filter
branch of `ClassesPage::hideEvent` use the typed port. Menu radio persistence
is preserved; page leave clears only day-filter state when configured for
`OnPageLeave`. Class-selection reset and visibility policies remain unchanged,
and neither caller makes a direct legacy day-policy call.

The executor reported focused tests 7/7. The independent tester reported PASS
with configure/ownership, a final Debug build, focused tests 9/9, the exact
nine-target regression 9/9, resource/dependency/Qt-free/static/diff checks,
and the exact eight-file scope. Known nonblocking warnings included a
transient LNK1163 resolved by retry, existing linker warnings, unavailable
`clang-format`/`clang-tidy`, and LF/CRLF normalization warnings.

The typed class day-filter reset-policy seam is closed. Phase 2 remains open;
the next slice is not yet selected.
### Progress update - 2026-09-21 (typed class-selection reset-policy boundary)

Against baseline commit `ebc6a3f9`, added the Qt-free
`ClassSelectionResetPolicy`/`ClassSelectionResetPolicyPort` contract and its
platform adapter for the exact
`classes_navigation_class_selection_reset_policy` key. It maps
`OnApplicationClose` and `OnPageLeave`, trims and case-normalizes stored values,
preserves round-trip persistence, and falls back to `OnApplicationClose` for
missing, invalid, or unavailable settings.

The class-selection menu radio persistence is typed independently from the
completed day-filter policy. On `ClassesPage::hideEvent`, `OnPageLeave` clears
only selected/current-class state; day-filter and visibility policies remain
independent, and neither `menu_builder.cpp` nor `classes_page.cpp` directly
calls the legacy class-selection policy functions.

Verification passed configure/ownership with 758 sources; focused tests 10/10
(new adapter, ClassesPage, day-filter, launch, and navigation/analytics/
evaluation/startup visual checks); the exact nine-target regression 9/9;
resources 6 RCC packs/7 runtime IDs/7 runtime references; and dependency,
Qt-free, call-site, static, and diff checks. The exact dirty scope was eight
files. An unrelated full-solution build-directory file-lock failure occurred
after modified targets compiled; it was nonblocking, as were existing linker
warnings. `clang-format`/`clang-tidy` remained unavailable.

The typed class-selection reset-policy seam is closed. Phase 2 remains open;
the next slice is not yet selected.
### Progress update - 2026-09-21 (typed class-navigation visibility-scope boundary)

Against baseline commit `8372fd9d`, added the Qt-free
`ClassVisibilityScope`/`ClassVisibilityPreferencesPort` contract and its
platform adapter for the exact `classes_navigation_visibility_scope` key.
Stored `active_schedule` and `all_classes` map to the corresponding typed
values; missing, invalid, and unavailable settings fall back to
`ActiveSchedule`, with missing-key initialization and trimmed,
case-normalized round-trip persistence preserved.

Typed menu persistence now feeds the initial and refresh loads of
`ClassesPage` and `SpeakingEvalPage`. Existing class-tab and day-filter
semantics remain unchanged, other navigation keys are untouched, and none of
the three callers (`menu_builder.cpp`, `classes_page.cpp`, and
`speaking_eval_page.cpp`) directly accesses `ClassNavigationPreferences`.

Verification recorded a clean Debug build PASS and configure/ownership with
761 sources. Focused visibility/page/navigation checks were 11/12 PASS; the
single unrelated `ClassMngrSpeakingEvalBatchReportServiceTests`
`aiPromptPreviewCopiesAnAnonymousPrompt` case is environment-sensitive, passes
with `QT_QPA_PLATFORM=offscreen`, and its full rerun was interrupted.
Additional navigation checks passed 2/2; the exact nine-target regression
passed 9/9; resources passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff
checks passed. The exact dirty scope was nine files. Expected Vulkan/zlib and
existing linker warnings remained nonblocking.

The typed class-navigation visibility-scope seam is closed. Phase 2 remains
open; the next slice is not yet selected.
### Progress update - 2026-09-21 (typed last-selected-campus persistence boundary)

Against baseline commit `31db0c2d`, added the Qt-free
`LastSelectedCampusPort` and `SettingsManagerLastSelectedCampusPort` adapter
using `std::optional<Domain::CampusId>`. The canonical legacy key is exactly
`campus/lastSelectedJsonId` (`SettingsManager::Keys::LAST_CAMPUS_JSON_ID`);
blank, invalid, missing, and unavailable values read as no selection without
rewriting, while valid IDs preserve exact text and support set/clear round-trip
behavior.

Only `src/features/campus/ui/campus_dashboard_page.cpp` and
`src/features/campus/ui/campus_dashboard_page_data.cpp` use the typed port.
The explicit current-campus value takes precedence over the persisted fallback.
`UserPreferencesState`, `CampusDirectoryProjection`, and resource surfaces are
unchanged, and the separate legacy integer campus key remains untouched.

Verification passed configure/ownership with 764 sources, a clean Debug build,
focused tests 9/9, and the exact nine-target regression 9/9. Resource checks
passed 6 RCC packs/7 runtime IDs/7 runtime references; dependency
(`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff checks
passed. The hardened canonical-literal adapter assertion passed 1/1, and the
exact dirty scope was eight files. Only existing linker and LF/CRLF warnings
were noted.

The typed last-selected-campus persistence seam is closed. Phase 2 remains
open; the next slice is not yet selected.
### Progress update - 2026-09-21 (typed last-database-directory persistence boundary)

Against baseline commit `89068539`, added the Qt-free UTF-8 typed
`LastDatabaseDirectoryPort` and its SettingsManager adapter. The canonical
key is exactly `SettingsManager::Keys::LAST_DATABASE_DIRECTORY`,
`files/lastDirectory`. Missing or empty values read as empty, Unicode and
path values round-trip, and writes retain the legacy synchronized persistence
behavior.

`FileController::databaseDialogDirectory()` prefers the active/current
database directory, then the typed persisted directory, then the default
directory. Remembered writes use the absolute parent directory and return
early for blank paths; create, save-as, and export flows update the typed
boundary. `mostRecentDatabasePath()` remains recent-history-only, recent-file
semantics are unchanged, and `file_controller.cpp` has no direct legacy key or
getter/setter calls for this boundary.

Verification passed configure/ownership with 767 sources, a clean Debug build,
focused tests 9/9, and the exact nine-target regression 9/9. Resource checks
passed 6 RCC packs/7 runtime IDs/7 runtime references; dependency
(`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff checks
passed. The exact dirty scope was seven files. Expected Vulkan/zlib, existing
linker, and LF/CRLF warnings were the only noted nonblocking warnings.

The typed last-database-directory persistence seam is closed. Phase 2 remains
open; the next slice is not yet selected.
### Progress update - 2026-09-21 (typed AI custom-website persistence boundary)

Against baseline commit `8579ee1f`, added the Qt-free UTF-8/empty
`AiCommentCustomWebsitePort` and its SettingsManager adapter. The exact
canonical key is `OptionKeys::AiCommentCustomWebsiteUrl`,
`options/aiCommentCustomWebsiteUrl`, with typed read/write/clear behavior.

ActionRegistry, the menu presentation, and both speaking-evaluation dialogs
now use the typed boundary. Existing HTTPS trimming and validation remain
unchanged; invalid stored URLs fall back to ChatGPT, cancel leaves the prior
provider and URL untouched, valid custom URLs still open, and provider/voice
settings remain unchanged.

Verification passed configure/ownership with 770 sources and a clean Debug
build. The offscreen focused suite passed 8/8, covering the adapter, AI
options, both speaking dialog/report paths, dialog shell, launch, resources,
and startup; the additional speaking service check passed. The exact
nine-target regression passed 9/9; resource checks passed 6 RCC packs/7
runtime IDs/7 runtime references; dependency (`ClassMngrNext -> Qt6::Core`),
Qt-free, static, call-site, and diff checks passed. The exact dirty scope was
ten files. Expected Vulkan/zlib and LF/CRLF warnings remained nonblocking; the
default headless dialog mode required `QT_QPA_PLATFORM=offscreen`.

The typed AI custom-website persistence seam is closed. Phase 2 remains open;
the next slice is not yet selected.
### Progress update - 2026-09-21 (typed AI-comment voice read bridge)

Against baseline commit `626501b5`, added the Qt-free typed
`AiCommentVoicePreferencesPort` and its read-only SettingsManager adapter for
the exact canonical key `OptionKeys::AiCommentVoice`,
`options/aiCommentVoice`. Stored `0` maps to `DirectToStudent`, `1` to
`ThirdPerson`, and missing, unknown, or unavailable values map to
`DirectToStudent`.

Only `src/features/speaking_eval/ui/speaking_eval_ai_batch_dialog.cpp` and
`src/features/speaking_eval/ui/speaking_eval_report_dialog.cpp` use the typed
read bridge. ActionRegistry remains the compatibility writer and the menu
remains compatible with it; provider and custom-URL paths and existing prompt
behavior are unchanged.

Verification passed configure/ownership with 773 sources and a clean Debug
build. The offscreen focused suite passed 9/9, covering the adapter, AI
options, both dialogs, speaking service, dialog shell, launch, resources, and
startup. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff
checks passed. The exact dirty scope was seven files. Expected Vulkan/zlib,
linker, and LF/CRLF warnings remained nonblocking.

The typed AI-comment voice read seam is closed. Phase 2 remains open; the next
slice is not yet selected.
### Progress update - 2026-09-21 (typed AI-comment provider read bridge)

Against baseline commit `6a03386d`, added the Qt-free typed
`AiCommentProviderPreferencesPort` and its read-only SettingsManager adapter
for the exact canonical key `OptionKeys::AiCommentProvider`,
`options/aiCommentProvider`. Stored values map `0` to `ChatGPT`, `1` to
`Gemini`, `2` to `Claude`, `3` to `Microsoft Copilot`, and `4` to
`CustomWebsite`; missing, unknown, and unavailable values map to `ChatGPT`.

Only `src/features/speaking_eval/ui/speaking_eval_ai_batch_dialog.cpp` and
`src/features/speaking_eval/ui/speaking_eval_report_dialog.cpp` use the typed
read bridge. ActionRegistry remains the compatibility writer and the menu
remains compatible with it; voice, custom-URL, prompt, and browser behavior
are unchanged.

Verification passed configure/ownership with 776 sources and a clean Debug
build. The voice/AI/dialog suite passed 9/9, and the provider adapter was
explicitly built and passed standalone 1/1. Resource checks passed 6 RCC
packs/7 runtime IDs/7 runtime references; dependency (`ClassMngrNext ->
Qt6::Core`), Qt-free, static, call-site, and diff checks passed. The exact
dirty scope was seven files. The initial aggregate CTest omitted the provider
target, but the standalone provider test passed; expected Vulkan/zlib, linker,
and LF/CRLF warnings remained nonblocking.

The typed AI-comment provider read seam is closed. Phase 2 remains open; the
next slice is not yet selected.
### Progress update - 2026-09-21 (typed font-size startup read bridge)

Against baseline commit `988db2af`, added the Qt-free typed
`FontSizePreferencesPort` and its read-only SettingsManager adapter for the
exact canonical key `OptionKeys::FontSize`, `options/fontSize`. Stored values
map `-2` to `Small`, `0` to `Normal`, `2` to `Large`, and `4` to `ExtraLarge`;
missing, unknown, and unavailable values map to `Normal`.

`main.cpp` now performs the typed startup read. Existing
`FontManager::setSizeOffset` offsets and visual-capture precedence remain
unchanged, as does the `OptionState<FontSize>` menu compatibility
reader/writer.

Verification passed configure/ownership with 779 sources and a clean Debug
build. The offscreen focused suite passed 6/6, covering the font adapter,
FontManager, startup visual behavior, AI options, launch, and resources.
Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff
checks passed. The exact dirty scope was six files. The adapter test required a
configure refresh; expected warnings remained nonblocking.

The typed font-size startup read seam is closed. Phase 2 remains open; the next
slice is not yet selected.
### Progress update - 2026-09-21 (typed theme startup read bridge)

Against baseline commit `6e67978c`, added the Qt-free typed theme startup read
bridge for the canonical `OptionKeys::Theme == "options/theme"`; this is not
the legacy `SettingsManager::Keys::THEME == "ui/theme"` key. Stored values map
`0` to `Dark`, `1` to `Light`, and `2` to `SystemDefault`; missing, unknown,
and unavailable values map to `SystemDefault`.

`main.cpp` now performs the typed read. Visual-capture precedence and the
existing `ThemeService` values remain unchanged, as do the
`ActionRegistry`/`ThemeController`/`ThemePreferencePort` and menu compatibility
owners.

Verification passed configure/ownership with 782 sources and a clean Debug
build. The offscreen focused suite passed 5/5, covering the theme adapter,
startup visual behavior, AI options, launch, and resources. Resource checks
passed 6 RCC packs/7 runtime IDs/7 runtime references; dependency
(`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff checks
passed. The exact dirty scope was six files. The corrected build-target
invocation was used; expected warnings remained nonblocking.

The typed theme startup read seam is closed. Phase 2 remains open; the next
slice is not yet selected.
### Progress update - 2026-09-21 (typed DialogShell geometry persistence)

Against baseline commit `05750cda`, added the Qt-free dialog-geometry
read/write contract and its SettingsManager adapter. The dynamic canonical key
is `ui/dialogs/<normalizedDialogKey>/geometry`; dialog-key trimming and
character normalization are preserved. Missing or empty reads remain empty,
binary geometry round-trips exactly, each dialog is isolated, and an empty key
performs no write.

`DialogShell` now keeps the restore/persist lifecycle guards without singleton
access. Derived-dialog behavior remains unchanged.

Verification passed configure/ownership and a clean Debug build. The offscreen
focused suite passed 7/7, covering the geometry adapter, DialogShell lifecycle
and saved-size behavior, three next-application tests, launch, and resources.
Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff
checks passed. The exact dirty scope was six files. The adapter target
inclusion correction was applied; expected warnings remained nonblocking.

The typed DialogShell geometry persistence seam is closed. Phase 2 remains
open; the next slice is not yet selected.
### Progress update - 2026-09-21 (typed language-preference persistence/migration bridge)

Against baseline commit `e961016d`, added the Qt-free typed language
preference persistence/migration bridge for the canonical
`OptionKeys::Language == "options/language"`. Stored values map `0` to
`SystemDefault`, `1` to `English`, and `5` to `Korean`; legacy values `2`, `3`,
and `4` read as `English` and are rewritten as `1`. Unknown, malformed,
missing, and unavailable values map to `SystemDefault`.

`main.cpp` now performs the typed read. `LanguageService::savedLanguage`
persistence access and stale call sites were removed. Existing
`LanguagePreferencePort` apply behavior and the dominant visual-language
override remain unchanged.

Verification passed configure/ownership with 788 sources and an elevated clean
Debug build. The offscreen focused suite passed 8/8, covering the language
adapter, LanguageService, LanguagePreferencePort, startup visual/performance,
launch, resources, and user preferences. Resource checks passed 6 RCC
packs/7 runtime IDs/7 runtime references; dependency
(`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff checks
passed. The exact dirty scope was eight files. The non-elevated build required
a FileTracker retry before the elevated pass; expected warnings remained
nonblocking.

The typed language-preference persistence/migration seam is closed. Phase 2
remains open; the next slice is not yet selected.
### Progress update - 2026-09-21 (typed upcoming-birthday dismissal write port)

Against baseline commit `958b471a`, added the Qt-free write-only typed date
contract for `SettingsManager::Keys::UPCOMING_BIRTHDAYS_DISMISSED_DATE ==
"notifications/upcomingBirthdaysDismissedDate"`. The adapter performs strict
ISO `CalendarEventDate` to legacy `QDate` conversion: valid dates are stored,
while empty or malformed dates do not overwrite the existing value.

Only the sidebar writer was cut over, and it writes only from
`dismissForToday()`. Reminder and visibility behavior remain unchanged; the
obsolete header dependency was removed safely.

Verification passed configure/ownership and elevated clean/focused builds. The
focused suite passed 4/4, covering the adapter, next application contract,
UpcomingBirthdays, and sidebar. Resource checks passed 6 RCC packs/7 runtime
IDs/7 runtime references; dependency (`ClassMngrNext -> Qt6::Core`), Qt-free,
static, call-site, and diff checks passed. The exact dirty scope was seven
files. Expected Vulkan/zlib and LF/CRLF warnings remained nonblocking; stale
MSBuild children were stopped.

The typed upcoming-birthday dismissal write seam is closed. Phase 2 remains
open; the next slice is not yet selected.
### Progress update - 2026-09-21 (typed document-viewer-background read bridge)

Against baseline commit `e5bd3bce`, added the Qt-free typed read bridge for the
canonical `OptionKeys::DocumentViewerBackground ==
"options/documentViewerBackground"`. Stored values map `0` to `Default`, `1`
to `White`, and `2` to `Black`; missing, invalid, unknown, and unavailable
values map to `Default`.

`ActionRegistry` now uses the typed load. The existing `OptionState` remains
the compatibility writer and menu-persistence owner. `PageManager`/
`PdfViewer` live and theme-derived behavior remains unchanged.

Verification passed configure/ownership with 794 sources and an elevated clean
Debug build. The offscreen focused suite passed 5/5, covering the adapter, next
application contract, PageManager, startup visual behavior, and startup
performance/PDF. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was six files.
Expected warnings remained nonblocking; stale processes were stopped.

The typed document-viewer-background read seam is closed. Phase 2 remains open;
the next slice is not yet selected.
### Progress update - 2026-09-21 (typed document-page-spacing read bridge)

Against baseline commit `b73ffa1b`, added the Qt-free typed read bridge for the
canonical `OptionKeys::DocumentPageSpacing ==
"options/documentPageSpacing"`. Stored values map `0` to `None`, `1` to
`Small`, `2` to `Medium`, and `3` to `Large`; missing, unavailable, and
unknown numeric values map to `Small`. Malformed or non-numeric values retain
legacy parity through unchecked `QVariant::toInt()`, yielding `0` (`None`).

`ActionRegistry` now uses the typed load. The existing `OptionState` remains
the compatibility writer and menu-persistence owner, and `PageManager`/
`PdfViewer` behavior remains unchanged.

Verification passed configure/ownership with 797 sources and an elevated clean
Debug build. The offscreen focused suite passed 5/5, covering the adapter,
application contract, PageManager, startup visual behavior, and startup
performance/PDF. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was six files.
Expected warnings remained nonblocking; stale processes were stopped.

The typed document-page-spacing read seam is closed. Phase 2 remains open; the
next slice is not yet selected.
### Progress update - 2026-09-21 (typed SaveMode preference read bridge)

Against baseline commit `294c7f08`, added the Qt-free typed read bridge for the
canonical `OptionKeys::SaveMode == "options/saveMode"`; this is not the stale
`SettingsManager::Keys::SAVE_MODE == "app/saveMode"` key. Stored values map
`0` to `Automatic` and `1` to `Manual`; missing, malformed, unknown, and
unavailable values map to `Automatic`.

`ActionRegistry` now uses the typed load. The existing `OptionState` remains
the compatibility writer and menu-persistence owner. `MainWindow`,
`PageManager`, and `FileController` behavior remains unchanged.

Verification passed configure/ownership with 800 sources and an elevated clean
Debug build. The offscreen focused suite passed 5/5, covering the adapter, next
application contract, PageManager, startup visual behavior, and startup
performance. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was six files.
Expected warnings remained nonblocking; stale processes were stopped.

The typed SaveMode read seam is closed. Phase 2 remains open; the next slice is
not yet selected.
### Progress update - 2026-09-21 (ActionRegistry typed AI-comment-voice read cutover)

Against baseline commit `9db2848f`, completed the remaining caller cutover in
`src/ui/shared/actions/action_registry.cpp` to the existing typed voice port
for the exact key `options/aiCommentVoice`. Values map `0` to
`DirectToStudent` and `1` to `ThirdPerson`; missing, unknown, and unavailable
values fall back to `DirectToStudent`. The caller has no direct raw settings
load.

The existing `OptionState` remains the compatibility writer and menu owner.
Provider, custom-URL, prompt, and dialog behavior remain unchanged.

Verification passed configure/ownership with 800 sources and a clean targeted
Debug build. The offscreen focused suite passed 6/6, covering the voice
adapter, stored-voice AI options, report widget, DialogShell, and startup
visual/performance. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was one file. An
optional broad `ALL_BUILD` was stopped; no code failure was indicated.

The typed AI-comment-voice read cutover is closed. Phase 2 remains open; the
next slice is not yet selected.
### Progress update - 2026-09-21 (ActionRegistry typed AI-comment-provider read cutover)

Against baseline commit `610e53e6`, completed the remaining ActionRegistry
caller cutover to the existing typed provider port for the exact key
`OptionKeys::AiCommentProvider == "options/aiCommentProvider"`. Values map `0`
to `ChatGPT`, `1` to `Gemini`, `2` to `Claude`, `3` to `Microsoft Copilot`,
and `4` to `CustomWebsite`; missing, unknown, and unavailable values fall back
to `ChatGPT`. There is no direct raw provider load.

The existing `OptionState` remains the compatibility writer and menu owner.
Custom URL, provider URL, prompt, voice, and dialog behavior remain unchanged.

Verification passed configure/ownership with 800 sources and a clean focused
Debug build. The offscreen focused suite passed 7/7, covering the provider
adapter, custom-URL adapter, AI-options provider/custom-URL cases, report
widget, DialogShell, and startup visual/performance. Resource checks passed 6
RCC packs/7 runtime IDs/7 runtime references; dependency
(`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff checks
passed. The exact dirty scope was one file. Expected warnings remained
nonblocking; no clipboard failure occurred.

The typed AI-comment-provider read cutover is closed. Phase 2 remains open; the
next slice is not yet selected.
### Progress update - 2026-09-21 (ActionRegistry typed font-size read cutover)

Against baseline commit `bc5c90b6`, completed the remaining ActionRegistry
caller cutover to the existing typed font-size port for the exact key
`OptionKeys::FontSize == "options/fontSize"`. Values map `-2` to `Small`, `0`
to `Normal`, `2` to `Large`, and `4` to `ExtraLarge`; missing, unknown, and
unavailable values fall back to `Normal`. The caller has no direct raw load.

The existing `OptionState` remains the compatibility writer and menu owner.
`FontManager` offsets, visual-capture precedence, and startup behavior remain
unchanged.

Verification passed configure/ownership with 800 sources and a clean focused
Debug build. The offscreen focused suite passed 5/5, covering the font
adapter, FontManager, startup visual behavior, startup performance, and AI
options. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static, call-site, and diff
checks passed. The exact dirty scope was one file. Expected warnings remained
nonblocking.

The typed ActionRegistry font-size read cutover is closed. Phase 2 remains
open; the next slice is not yet selected.
### Progress update - 2026-09-21 (ActionRegistry typed theme read cutover)

Against baseline commit `50de1ce4`, completed the remaining ActionRegistry
caller cutover to the existing typed theme port for canonical
`OptionKeys::Theme == "options/theme"`, distinct from legacy
`SettingsManager::Keys::THEME == "ui/theme"`. Values map `0` to `Dark`, `1`
to `Light`, and `2` to `SystemDefault`; missing, unknown, and unavailable
values fall back to `SystemDefault`. The caller has no direct raw load.

The existing `OptionState` remains the compatibility writer and menu owner.
ThemeController synchronization, palette/icon refresh, visual-capture
precedence, and user writes remain unchanged.

Verification passed configure/ownership with 800 sources and a clean focused
Debug build. The offscreen focused suite passed 4/4, covering the theme
adapter, startup visual behavior, startup performance, and AI options.
Resource, dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was one file.
Expected warnings remained nonblocking.

The typed ActionRegistry theme read cutover is closed. Phase 2 remains open;
the next slice is not yet selected.
### Progress update - 2026-09-21 (ActionRegistry typed language read cutover)

Against baseline commit `46ef66f1`, completed the remaining ActionRegistry
caller cutover to the existing migration-aware language port for canonical
`OptionKeys::Language == "options/language"`. Values map `0` to
`SystemDefault`, `1` to `English`, and `5` to `Korean`; legacy values `2`, `3`,
and `4` read as `English` and are written back as `1`. Unknown, malformed,
missing, and unavailable values fall back to `SystemDefault`. The caller has no
direct raw load.

The existing `OptionState` remains the compatibility writer and menu owner.
Retranslation, font refresh, visual override, controller synchronization, and
user writes remain unchanged.

Verification passed configure/ownership with 800 sources and a clean focused
Debug build. The offscreen focused suite passed 6/6, covering the language
adapter, LanguagePreferencePort, LanguageService, startup visual/performance,
and AI options. Resource, dependency (`ClassMngrNext -> Qt6::Core`), Qt-free,
static, call-site, and diff checks passed. The exact dirty scope was one file.
Expected warnings remained nonblocking.

The typed ActionRegistry language read cutover is closed. Phase 2 remains open;
the next slice is not yet selected.
### Progress update - 2026-09-21 (typed ScheduleWidget display-mode persistence cutover)

Against baseline commit `41859a5f`, added the Qt-free display-mode contract for
`Regular`, `Intensive`, and `Testing`. The canonical key is
`schedule_display_mode`; first load falls back to legacy
`schedule_show_intensive` when the canonical value is absent, then migrates
that result to the canonical key. Missing or invalid values fall back to
`Regular`.

Unavailable reads return `Regular` and unavailable saves are no-ops.
`ScheduleWidget` alone now uses the typed load/save boundary. Existing
`ClassesPage` and `SpeakingEval` compatibility callers/helper remain
unchanged. Button refresh, reload, and `displayModeChanged` ordering remain
unchanged.

Verification passed configure/ownership with 803 sources and a clean focused
Debug build. The offscreen focused suite passed 6/6, covering the adapter,
ScheduleWidget, builder, print model/PDF, and existing schedule-display
adapter. Resource, dependency (`ClassMngrNext -> Qt6::Core`), Qt-free, static,
call-site, and diff checks passed. The exact dirty scope was six files.
Expected warnings remained nonblocking.

The typed ScheduleWidget display-mode persistence seam is closed. Phase 2
remains open; the next slice is not yet selected.
### Progress update - 2026-09-21 (ClassesPage typed schedule-display-mode caller cutover)

Against baseline commit `83cca837`, completed the ClassesPage caller cutover
to the typed schedule-display-mode boundary. The canonical key is
`schedule_display_mode`, with legacy fallback key `schedule_show_intensive`.
Values are trimmed and case-normalized; invalid modern values do not overwrite
the canonical setting, while a missing modern key performs the legacy-to-
canonical migration. An unavailable service resolves to `Regular`.

`Testing` preserves the existing `Regular` schedule-source behavior.
`SpeakingEvalPage` remains the compatibility caller. The exact current
production/test-target scope is `src/features/classes/ui/classes_page.cpp` and
`cmake/tests/pages_and_output.cmake`.

Verification passed configure/build and ownership validation with 803 sources;
the focused suite passed 7/7. Resource, Qt-free, static, call-site, and diff
checks passed. Known warnings were missing Vulkan headers, existing
`/FORCE`/duplicate-stub linker warnings, and LF-to-CRLF normalization.

This ClassesPage caller-cutover slice is closed. Phase 2 remains open; the next
boundary is not yet selected.
### Progress update - 2026-09-21 (final SpeakingEvalPage schedule-display-mode seam)

Against baseline commit `01092f57`, completed the final schedule-display-mode
compatibility caller cutover. `SpeakingEvalPage` now uses the existing typed
`ApplicationServicesScheduleDisplayModePreferencesPort`, mapping typed
`Regular`/`Intensive`/`Testing` to the legacy UI enum while preserving page
lifecycle, evaluation loading, visibility, and `scheduleSourceForMode`
behavior. `ScheduleWidget` remains the write owner.

The canonical key is `schedule_display_mode`, with legacy fallback/migration
from `schedule_show_intensive`. Reads trim and case-normalize; an invalid
modern value does not overwrite the canonical setting, a missing modern key
migrates the legacy value, and an unavailable service resolves to `Regular`.
No legacy callers or source-list references remain. The exact seven-file
implementation scope is:

- `cmake/production_sources.cmake`
- `cmake/tests/features.cmake`
- `cmake/tests/pages_and_output.cmake`
- `src/features/speaking_eval/ui/speaking_eval_page.cpp`
- `src/features/speaking_eval/ui/speaking_eval_page_p.h`
- deleted `src/features/schedule/schedule_display_mode_preferences.cpp`
- deleted `src/features/schedule/schedule_display_mode_preferences.h`

Verification passed ownership validation with 801 handwritten sources, a clean
Debug rebuild, focused CTest 8/8, and an offscreen launch smoke test. Resource
checks passed 6 RCC packs/7 runtime IDs/7 runtime references; dependency and
Qt-free, static, and call-site checks passed; `git diff --check` passed.
Warnings were limited to the nonfatal MSB8064 generated-autogen notice for
stale deleted-helper paths, existing `/FORCE`/duplicate-stub linker warnings,
missing Vulkan headers, and LF-to-CRLF normalization.

This final schedule-display-mode seam is closed. Phase 2 remains open; the
next boundary is not yet selected.
### Progress update - 2026-09-21 (typed two-key calendar event-display preferences boundary)

Against baseline commit `db8c7cf1`, completed the Qt-free typed boundary for
`calendar/showEventsAtAllCampuses` and `calendar/hideStartOfTermEvents`.
Missing and unavailable values default to `false`; exact-key round trips,
legacy `QVariant` boolean coercion, atomic `saveAll` failure/rollback, and
unrelated-setting preservation are covered.

`CalendarPreferencesPanel` now uses the typed port for load/save while
preserving warning behavior. `CalendarPageUpcomingEvents` uses the typed port
for display reads while preserving filtering. New sources are registered once,
the contract remains Qt-free, and existing production ownership is clean.

The exact seven-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/calendar/ui/calendar_page_upcoming_events.cpp`
- `src/features/calendar/ui/calendar_preferences_panel.cpp`
- `src/next/application/calendar_event_display_preferences.h`
- `src/next/platform/application_services_calendar_event_display_preferences_port.h`
- `tests/next_platform_application_services_calendar_event_display_preferences_port_tests.cpp`

Verification passed ownership validation with 804 handwritten sources and a
clean Debug rebuild. Focused tests passed 7/7, including adapter,
calendar/page/ScheduleWidget coverage; the offscreen launch smoke passed.
Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency, Qt-free, static, call-site, and `git diff --check` checks passed.
There is no standalone CalendarPreferencesPanel or UpcomingEvents test target;
coverage is through compilation, smoke/surrounding tests, and static review.
Warnings were missing Vulkan headers, the Qt bundled-zlib fallback, existing
`/FORCE`/duplicate-symbol linker warnings, and LF-to-CRLF normalization.

This calendar event-display preferences boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed AcademicCalendarProvider first-day-of-week preferences boundary)

Against baseline commit `6a33df6d`, completed the typed first-day-of-week
preferences boundary owned by `AcademicCalendarProvider` for the exact key
`calendar/firstDayOfWeek`. Persisted values `0..6` read directly; missing or
invalid values use the locale fallback, while an unavailable service uses the
same fallback and makes saves no-ops. Exact-key round trips are preserved.

`setFirstDayOfWeek` continues to normalize to Sunday/Monday (`0`/`1`). Provider
revision and signal ordering and save-warning behavior remain unchanged.
`AcademicCalendarProvider` has no direct raw key access. The
`calendar/academicSchedule/v1` JSON, QML, panel, and compatibility key owners
remain unchanged.

The exact six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/calendar/ui/academic_calendar_provider.cpp`
- `src/next/application/calendar_first_day_of_week_preferences.h`
- `src/next/platform/application_services_calendar_first_day_of_week_preferences_port.h`
- `tests/next_platform_application_services_calendar_first_day_of_week_preferences_port_tests.cpp`

Verification passed ownership validation with 807 handwritten sources and a
clean Debug rebuild. Focused tests passed 8/8, including the new adapter,
`ClassMngrAcademicCalendarTests`, and calendar/page/ScheduleWidget coverage;
the offscreen launch smoke passed. Resource checks passed 6 RCC packs/7
runtime IDs/7 runtime references; dependency, Qt-free, static, call-site, and
diff checks passed. Warnings were missing Vulkan headers, the Qt bundled-zlib
fallback, existing `/FORCE`/duplicate-stub linker warnings, and LF-to-CRLF
normalization.

This narrow calendar first-day-of-week boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed AcademicCalendarProvider schedule-persistence boundary)

Against baseline commit `7a50b3e9`, completed the typed persistence boundary
for the exact key `calendar/academicSchedule/v1`. The Qt-free contract carries
opaque `std::string` JSON, with the adapter preserving exact-key UTF-8/
`QVariant` round trips. Missing or unavailable services return an empty read
and make writes no-ops; payload and unrelated settings are preserved, and save
failure retains warning behavior.

`AcademicCalendarProvider::reload()` and `persist()` now use only the typed
port. `AcademicCalendarSchedule::toJson()`/`fromJson()`, schema/version 1,
malformed-load clearing, defaults, provider API, revision/signal ordering,
panel, QML, and production ownership remain unchanged.

The exact six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/calendar/ui/academic_calendar_provider.cpp`
- `src/next/application/academic_calendar_schedule_preferences.h`
- `src/next/platform/application_services_academic_calendar_schedule_preferences_port.h`
- `tests/next_platform_application_services_academic_calendar_schedule_preferences_port_tests.cpp`

Verification passed ownership validation with 810 handwritten sources and a
clean Debug rebuild. Focused tests passed 8/8, including the new adapter,
`ClassMngrAcademicCalendarTests` JSON round-trip/malformed fallback coverage,
and calendar/page/ScheduleWidget coverage; the offscreen launch smoke passed.
Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency, Qt-free, static, call-site, and diff checks passed. Warnings were
missing Vulkan headers, the Qt bundled-zlib fallback, existing
`/FORCE`/duplicate-stub linker warnings, and LF-to-CRLF normalization.

This remaining narrow calendar persistence boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed CalendarPage event-type color persistence boundary)

Against baseline commit `c74b8a9c`, completed the typed CalendarPage
event-type color persistence boundary. Dynamic keys are
`calendar/eventTypeColor/<normalized-event-type>`, with the exact normalized
event types `Vacation`, `Holiday`, `Workshop`, `CM`, `Meeting`, and `Other`.
Event-type normalization and `QColor` conversion remain in the UI; the adapter
owns only typed key construction and persistence.

Valid `QColor::HexRgb` values round-trip and store exactly. Missing or
unavailable values use the UI default; invalid stored colors pass through the
adapter and fall back in the UI. Invalid colors are no-op saves, save failures
retain their warning, and unrelated settings are preserved. `myInfo`, campus,
`custom_colors`, and generic settings remain separate.

`CalendarPage` no longer raw-accesses dynamic color keys; defaults, warning
paths, and repaint/filter-refresh ordering remain unchanged. The exact
six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/calendar/ui/calendar_page_upcoming_events.cpp`
- `src/next/application/calendar_event_type_color_preferences.h`
- `src/next/platform/application_services_calendar_event_type_color_preferences_port.h`
- `tests/next_platform_application_services_calendar_event_type_color_preferences_port_tests.cpp`

Verification passed ownership validation with 813 handwritten sources and an
elevated Debug focused build. Focused tests passed 7/7, including the adapter
and calendar/page-adjacent coverage; the offscreen launch smoke passed.
Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency, Qt-free, static, dynamic-key call-site, and diff checks passed.
The initial stale CTest listing was resolved by target regeneration. Existing
`/FORCE`/duplicate-symbol linker warnings, missing Vulkan headers, and
LF-to-CRLF normalization remained nonblocking; idle MSBuild nodes were also
nonblocking.

This event-type color boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.
### Progress update - 2026-09-21 (typed CalendarPage current-campus read boundary)

Against baseline commit `9413fc1d`, completed the typed CalendarPage
current-campus read boundary for the exact key `myInfo/campus`. The adapter is
read-only and performs no writes: it reads the exact key, returns an empty
value when the setting is missing or unavailable, preserves verbatim
`QVariant::toString()` behavior, and leaves unrelated settings untouched.

CalendarPage preserves empty handling, current/all-campus code construction,
case-insensitive matching, duplicate removal, filtering, and refresh behavior.
This boundary is separate from `campus/lastSelectedJsonId`, and
`PersonalDetailsRepository::saveCampus` remains the writer.

The exact six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/calendar/ui/calendar_page_upcoming_events.cpp`
- `src/next/application/current_campus_preferences.h`
- `src/next/platform/application_services_current_campus_preferences_port.h`
- `tests/next_platform_application_services_current_campus_preferences_port_tests.cpp`

Verification passed ownership validation with 816 handwritten sources and an
elevated Debug build. Focused tests passed 7/7; the adapter passed 4/4, and
last-selected-campus plus CampusDashboard regressions passed 2/2. The
offscreen launch smoke passed. Resource checks passed 6 RCC packs/7 runtime
IDs/7 runtime references; dependency, Qt-free, static, key-separation,
call-site, and diff checks passed. Warnings were missing Vulkan headers, the
Qt bundled-zlib fallback, existing `/FORCE`/duplicate-symbol linker warnings,
and LF-to-CRLF normalization.

This current-campus boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.
### Progress update - 2026-09-21 (typed custom-color palette persistence boundary)

Against baseline commit `84e5cf70`, completed the typed custom-color palette
persistence boundary for the exact key `custom_colors`. The typed port owns a
fixed 16-entry palette with canonical uppercase defaults, missing/unavailable
fallbacks, compact JSON round-trip, legacy `QStringList`/JSON/separator
payload compatibility, invalid-entry fallback, fixed 16-entry normalization,
canonicalization, save-failure warning, and unrelated-setting preservation.
`QColorDialog` behavior remains unchanged.

The exact six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/core/utils/colorutils.cpp`
- `src/next/application/custom_color_palette_preferences.h`
- `src/next/platform/application_services_custom_color_palette_preferences_port.h`
- `tests/next_platform_application_services_custom_color_palette_preferences_port_tests.cpp`

`ColorUtils` now uses the typed port with no raw settings calls. `myInfo/name`,
Sub Prep, `OptionState`, and unrelated settings remain separate. Compatibility
diagnosis confirmed that valid stored colors are Qt-canonicalized lowercase on
read, while default fallback entries remain uppercase on write. SQLite coerces
`QStringList` settings to empty `TEXT`, so the `QStringList` parser path is
covered directly through the adapter helper.

Verification passed ownership validation with 819 handwritten sources and an
elevated Debug build. Focused tests passed 5/5 (adapter, TestingClassesPage,
ClassesPage, ScheduleImportDialog, and SubPrepPage); adapter slots passed 6/6,
and the offscreen launch smoke passed. Resource checks passed 6 RCC packs/7
runtime IDs/7 runtime references; dependency, Qt-free, static, call-site, and
diff checks passed. Warnings were missing Vulkan headers/Qt zlib fallback,
existing `/FORCE`/duplicate-symbol linker warnings, and LF-to-CRLF
normalization; no standalone ColorUtils target exists.

This custom-color boundary is closed. Phase 2 remains open; the next boundary
is not yet selected.
### Progress update - 2026-09-21 (typed personal display-name read bridge)

Against baseline commit `4c0f891d`, completed the typed, read-only personal
display-name bridge for the exact key `myInfo/name`. The adapter performs an
exact-key read, returns an empty value when the setting is missing or
unavailable, preserves UTF-8 and whitespace round-trip behavior, and leaves
unrelated settings untouched. It performs no writes, exposes no direct reader
key access to callers, and does not use `SettingsManager`.

Schedule output consumes the raw value; ScheduleImportDialog and
SubPrepPage retain their existing trimmed-value policies. `PersonalDetailsRepository`
and other writers remain unchanged, as does the direct schedule-import SQL.
The exact eight-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/schedule/services/schedule_output_controller.cpp`
- `src/features/schedule/ui/schedule_import_dialog.cpp`
- `src/features/sub_prep/ui/sub_prep_print_dialog.cpp`
- `src/next/application/personal_display_name_preferences.h`
- `src/next/platform/application_services_personal_display_name_preferences_port.h`
- `tests/next_platform_application_services_personal_display_name_preferences_port_tests.cpp`

Verification passed ownership validation with 822 handwritten sources and a
Debug build. Focused tests passed 3/3 (adapter, ScheduleImportDialog, and
SubPrepPage); adapter slots passed 3/3, and schedule-output coverage passed
3/3 for ScheduleWidget, PrintModel, and PrintPdf. The offscreen launch smoke
passed. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency, Qt-free, static, call-site, writer-preservation, and diff checks
passed. Warnings were Vulkan/zlib configure notices, existing forced-link and
duplicate-symbol warnings, and LF-to-CRLF normalization.

This personal display-name boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.
### Progress update - 2026-09-21 (typed Sub Prep saved-content settings bundle)

Against baseline commit `d4dc18e0`, completed the typed Sub Prep saved-content
settings boundary for the exact keys `subPrep/classMaterials`,
`subPrep/bookReportGrading`, `subPrep/bookReportSpecialInstructions`, and
`subPrep/subComments`. Typed load/save replaces only these raw settings calls,
with one atomic `saveAll`, missing-versus-present grading and special-instruction
defaults, unavailable-service no-op load and failed-save behavior, unrelated
setting preservation, and existing dirty/autosave/failure behavior retained.

The exact seven-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/sub_prep/ui/sub_prep_page_p.h`
- `src/features/sub_prep/ui/sub_prep_page_settings.cpp`
- `src/next/application/sub_prep_preferences.h`
- `src/next/platform/application_services_sub_prep_preferences_port.h`
- `tests/next_platform_application_services_sub_prep_preferences_port_tests.cpp`

Zoom, campus, name/PersonalDetails, `OptionState`, and the broader Sub Prep
migration remain separate. A compatibility correction is recorded: missing
`loadSetting` returns a successful result containing an invalid `QVariant`; the
adapter test asserts both `has_value` and `!value.isValid()`.

Verification passed ownership validation with 825 handwritten sources and a
Debug build. Focused tests passed 2/2; adapter slots passed 5/5, including the
required SubPrep regressions
`freshAndExistingGradingSettingsResolveWithoutDataLoss` and
`clearDatabaseStateStopsAutosaveAndRemovesLoadedContent`. The offscreen launch
smoke passed. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency, Qt-free, static, call-site, and diff checks passed.
The initial non-elevated FileTracker access denial was resolved by elevated
retry. Other warnings were Vulkan/zlib notices, existing `/FORCE`/duplicate-
symbol warnings, LF-to-CRLF normalization, and resident MSBuild nodes.

This Sub Prep saved-content boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.
### Progress update - 2026-09-21 (typed Sub Prep current-campus read cutover)

Against baseline commit `ffd1f769`, completed the Sub Prep current-campus read
cutover by reusing the existing `CurrentCampusPreferencesPort`. The exact
three-file implementation scope is:

- `src/features/sub_prep/ui/sub_prep_page_p.h`
- `src/features/sub_prep/ui/sub_prep_page_settings.cpp`
- `tests/sub_prep_page_tests.cpp`

The cutover removes `SettingsKeys::MyInfoCampus`, retains the settings
availability gate, and leaves no raw `myInfo/campus` read in Sub Prep. Existing
UTF-8 conversion, trimming, case-insensitive ID/display-name matching,
first-campus fallback, campus-field population, and unavailable-service no-op
behavior are preserved. `PersonalDetailsRepository::saveCampus` remains the
writer, and other Sub Prep settings remain untouched.

Verification passed ownership validation with 825 handwritten sources and a
Debug build. CTest passed 2/2; current-campus adapter slots passed 4/4, and
the individual Sub Prep tests passed 3/3:
`savedCampusSelectionUsesTypedRead`,
`freshAndExistingGradingSettingsResolveWithoutDataLoss`, and
`clearDatabaseStateStopsAutosaveAndRemovesLoadedContent`. The offscreen launch
smoke passed. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency, Qt-free, static, exact-key, call-site,
writer-preservation, and diff checks passed. Warnings were known Vulkan/zlib
notices, existing `/FORCE`/duplicate-symbol warnings, and LF-to-CRLF
normalization; initial guessed adapter slot names were corrected from the
executable listing.

This Sub Prep current-campus boundary is closed. Phase 2 remains open; the
next boundary is not yet selected.
### Progress update - 2026-09-21 (typed Sub Prep personal-Zoom read/migration boundary)

Against baseline commit `2ae382db`, completed the typed Sub Prep personal-Zoom
read/migration boundary. Primary keys are `myInfo/zoomLoginId`,
`myInfo/zoomPassword`, and `myInfo/zoomNotAvailable`; legacy fallbacks are
`subPrep/personalZoomEmail`, `subPrep/personalZoomPassword`, and
`subPrep/personalZoomNotAvailable`.

Primary values take precedence. Legacy reads are best-effort migrated to the
primary keys; if migration save fails, the legacy values remain readable.
Missing values retain N/A credentials and the default unavailable state, and
unavailable-service behavior is preserved. QVariant/UTF-8 coercion,
credential hiding, and `valueOrNa` display behavior remain unchanged. There
are no writer changes to `PersonalDetailsRepository`, personal-details
aggregation, `OptionState`, or unrelated Sub Prep settings.

The exact seven-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/sub_prep/ui/sub_prep_page_p.h`
- `src/features/sub_prep/ui/sub_prep_page_settings.cpp`
- `src/next/application/sub_prep_personal_zoom_preferences.h`
- `src/next/platform/application_services_sub_prep_personal_zoom_preferences_port.h`
- `tests/next_platform_application_services_sub_prep_personal_zoom_preferences_port_tests.cpp`

Verification passed ownership validation with 828 handwritten sources and a
Debug build. CTest passed 2/2; adapter slots passed 5/5, and Sub Prep
regressions passed, including `zoomUnavailableHidesStoredCredentials`, the
grading regression, and the database-state/autosave regression. The offscreen
launch smoke passed. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime
references; dependency, Qt-free, static, key-ownership, call-site, and diff
checks passed. Warnings were Vulkan/zlib notices, existing
`/FORCE`/duplicate-symbol warnings, and LF-to-CRLF normalization.

This personal-Zoom boundary is closed. Phase 2 remains open; the next boundary
is not yet selected.
### Progress update - 2026-09-21 (typed personal-display-name writer extension)

Against baseline commit `f4480483`, extended the existing typed `myInfo/name`
bridge with a typed save. Successful saves persist the trimmed name; existing
names are not overwritten, disabled folder creation performs no write,
unavailable writes are silent no-ops, and save failures map to technical
errors while retaining the warning. UTF-8/whitespace behavior and unrelated
settings remain preserved.

The exact four-file implementation scope is:

- `src/next/application/personal_display_name_preferences.h`
- `src/next/platform/application_services_personal_display_name_preferences_port.h`
- `src/features/sub_prep/ui/sub_prep_print_dialog.cpp`
- `tests/next_platform_application_services_personal_display_name_preferences_port_tests.cpp`

`SubPrepPrintDialog` has no raw key or direct settings access.
`PersonalDetailsRepository::save` remains the aggregate writer; no
`OptionState` or full personal-details migration is included.

Verification passed ownership validation with 828 handwritten sources and a
Debug build. CTest passed 2/2; adapter slots passed 5/5, including
`printDialogRequiresAndSavesMissingUserName`. The offscreen launch smoke
passed. Resource checks passed 6 RCC packs/7 runtime IDs/7 runtime references;
dependency, Qt-free, static, call-site, writer-preservation, and diff checks
passed. Warnings were Vulkan/zlib notices, existing `/FORCE`/duplicate-symbol
warnings, and LF-to-CRLF normalization.

This personal-display-name writer boundary is closed. Phase 2 remains open; the
next boundary is not yet selected.
### Progress update - 2026-09-21 (typed personal signature-image read port)

Against baseline commit `fa8b5193`, completed the typed personal signature-image
read boundary for the exact key `myInfo/signatureImage`. The port Base64-decodes
the stored value, invokes the existing `SignatureImage::prepareForEmbedding`
exactly once, and returns opaque bytes. Missing, invalid, unavailable, or
corrupt-Base64 values become empty; the port performs no writes.

The two page-actions `PersonalDetailsRepository` calls were removed while
report generation, PowerPoint output, and class-service guards remain
unchanged. The exact six-file implementation scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/speaking_eval/ui/speaking_eval_page_actions.cpp`
- `src/next/application/personal_signature_image.h`
- `src/next/platform/application_services_personal_signature_image_port.h`
- `tests/next_platform_application_services_personal_signature_image_port_tests.cpp`

Verification passed ownership validation with 831 handwritten sources and a
clean-first full Debug build. CTest passed 3/3; adapter slots passed 4/4, and
the offscreen launch smoke passed. Resource, Qt-free, static, exact-key,
one-preparation-call, call-site, writer-preservation, dependency, and diff
checks all passed. Expected notices were missing Vulkan headers, the bundled
zlib fallback, MSVC/build warnings, and LF-to-CRLF normalization.

This personal signature-image boundary is closed. Phase 2 remains open; the
next boundary is not yet selected.
### Progress update - 2026-09-21 (typed InitialSetupWizard signature-image reads)

Against baseline commit `039a618c`, completed the InitialSetupWizard slice:
both remaining personal signature-image reads now use the existing typed
`PersonalSignatureImagePort` and
`ApplicationServicesPersonalSignatureImagePort`. The adapter retains the
exact-key, Base64-decoding, exactly-once preparation, opaque-byte, and
read-only semantics; no new contract, adapter, or CMake change was needed.

The exact implementation/test scope is:

- `src/features/setup/ui/initial_setup_wizard.cpp`
- `tests/initial_setup_wizard_tests.cpp`

PersonalDetailsRepository aggregate loading for the name and other fields,
aggregate save, existing image file-selection replacement/write behavior,
preview behavior, and guards remain unchanged. Temporary-database coverage
verified valid image use, missing/unavailable/corrupt values as empty, and
preservation of the existing image when no replacement is selected.

Verification passed ownership validation with 831 handwritten sources and a
clean-first serial Debug build. CTest passed 4/4; wizard slots passed 2/2 and
signature-port slots passed 4/4. Resource, dependency, static, source,
offscreen, and diff checks passed. Expected notices were missing Vulkan
headers, the Qt bundled-zlib fallback, and LF-to-CRLF normalization. Four
leftover MSBuild processes were stopped cleanly; no command remains active.

This InitialSetupWizard boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.
### Progress update - 2026-09-21 (typed InitialSetupWizard display-name prefill)

Against baseline commit `bd1e0fec`, completed the remaining InitialSetupWizard
`myInfo/name` prefill cutover using the existing typed
`PersonalDisplayNamePreferencesPort` and
`ApplicationServicesPersonalDisplayNamePreferencesPort`. UTF-8 and whitespace
behavior matches legacy `QVariant::toString()` semantics; missing or
unavailable values are empty, and the read path performs no writes.

The exact implementation/test scope is:

- `src/features/setup/ui/initial_setup_wizard.cpp`
- `tests/initial_setup_wizard_tests.cpp`

PersonalDetailsRepository aggregate loading remains for `validatePage` aggregate
save and other fields. Trimming/validation, aggregate save, the typed
signature-image path, and guards remain preserved; no new contract, adapter,
CMake change, or unrelated migration was introduced.

Verification passed ownership validation with 831 handwritten sources and a
clean-first Debug build. CTest passed 5/5; new wizard name slots passed 2/2,
the display-name adapter passed 5/5, and the signature adapter passed 4/4.
Resource, dependency, offscreen, static, source, and diff checks passed. A
transient stale-AUTOGEN omission was resolved by forced regeneration/rebuild.
Expected notices were missing Vulkan headers, the bundled-zlib fallback, and
LF-to-CRLF normalization. Three leftover MSBuild processes were stopped; no
command remains active.

This InitialSetupWizard display-name boundary is closed. Phase 2 remains open;
the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed current-campus writer boundary)

Against baseline commit `b3a90622`, extended the existing current-campus port
with the Qt-free `write(std::string) -> Domain::Result<void>` contract. The
adapter writes the exact key `myInfo/campus` with UTF-8 conversion and maps
unavailable services and save failures. The PersonalDetailsPage corrective
write now uses this port.

The exact implementation/test scope is:

- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `src/next/application/current_campus_preferences.h`
- `src/next/platform/application_services_current_campus_preferences_port.h`
- `tests/next_platform_application_services_current_campus_preferences_port_tests.cpp`

Aggregate `PersonalDetailsRepository::save()`, current-campus read behavior,
matching/filtering/fallback/refresh/guards, other callers, and CMake remain
unchanged. Tests covered exact-key UTF-8 write, unrelated-setting
preservation, unavailable no-op, QVariant coercion, and save failure.

Verification passed ownership validation with 831 handwritten sources and a
clean-first serial Debug build. Focused tests passed 4/4, including
current-campus, MyWorkspace, CampusDashboard, and last-selected-campus;
adapter slots passed 6/6. Resource, dependency, offscreen, static, source,
and diff checks passed. Three leftover MSBuild processes were stopped; no
command remains active. Expected notices were missing Vulkan headers, the Qt
bundled-zlib fallback, and LF-to-CRLF normalization.

This current-campus writer boundary is closed. Phase 2 remains open; the next
boundary is not yet selected.
### Progress update - 2026-09-21 (typed PersonalDetailsPage signature-image read)

Against baseline commit `f434d5b3`, completed the remaining
PersonalDetailsPage `myInfo/signatureImage` read using the existing typed
`PersonalSignatureImagePort` and
`ApplicationServicesPersonalSignatureImagePort`. Prepared opaque bytes are
converted at the Qt boundary.

The exact implementation/test scope is:

- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/my_workspace_page_tests.cpp`

Aggregate `PersonalDetailsRepository` load/save, image replacement/removal
write behavior, no-write load semantics, name/campus/Zoom/typed signature
fields, and guards remain preserved. No new contract, adapter, CMake change,
or unrelated migration was introduced. Valid prepared previews work; missing,
corrupt, or unavailable values retain the default behavior. Existing adapter
coverage verifies exact Base64 decoding, preparation, and no-write semantics.

Verification passed ownership validation with 831 handwritten sources and a
clean-first Debug build. Focused tests passed 4/4 (MyWorkspace, signature
adapter, current-campus adapter, and CampusDashboard); new MyWorkspace slots
passed 2/2 and signature-adapter slots passed 4/4. Resource, dependency,
offscreen, static, source, and diff checks passed. The page test checks a
non-null preview; exact prepared bytes remain covered by adapter tests.
Expected notices were Vulkan/zlib and LF-to-CRLF normalization. Three leftover
MSBuild processes were stopped; no command remains active.

This PersonalDetailsPage signature-image boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed PersonalDetailsPage current-campus read)

Against baseline commit `9cec8483`, completed the remaining
`PersonalDetailsPage::loadStoredSettings()` current-campus read using the
existing typed `CurrentCampusPreferencesPort` and
`ApplicationServicesCurrentCampusPreferencesPort`. Exact UTF-8/verbatim
semantics are preserved. Repository loading remains for name, Zoom, and typed
signature fields.

The exact implementation/test scope is:

- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/my_workspace_page_tests.cpp`

Missing/unavailable behavior, combo matching, first-campus fallback,
case-insensitive comparison, corrective typed write, refresh, guards, and
aggregate save remain preserved. No new contract, adapter, CMake change, or
unrelated migration was introduced.

Verification passed ownership validation with 831 handwritten sources and a
clean-first serial Debug build. Focused tests passed 4/4 (MyWorkspace,
current-campus adapter, CampusDashboard, and last-selected-campus); MyWorkspace
slots passed 3/3 and current-campus adapter slots passed 6/6. Resource,
dependency, offscreen, static, source, and diff checks passed. Page
missing/unavailable cases are covered through adapter tests and source review;
stored-campus matching and correction are directly tested. Expected notices
were Vulkan/zlib and LF-to-CRLF normalization. Three leftover MSBuild
processes were stopped; no command remains active.

This PersonalDetailsPage current-campus boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed PersonalDetailsPage display-name read)

Against baseline commit `2b58a9c0`, completed the remaining
`PersonalDetailsPage::loadStoredSettings()` display-name read using the
existing typed `PersonalDisplayNamePreferencesPort` and
`ApplicationServicesPersonalDisplayNamePreferencesPort`. UTF-8 is converted
to `QString` with legacy `QVariant::toString()` and whitespace behavior;
missing or unavailable values are empty, and the read path performs no writes.

The exact implementation/test scope is:

- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/my_workspace_page_tests.cpp`

Repository loading remains for Zoom and typed-signature fields, with aggregate
save ownership preserved. Save trimming/validation, campus/signature UI state,
guards, and unrelated writers remain unchanged. No new contract, adapter, CMake
change, or unrelated migration was introduced.

Verification passed ownership validation with 831 handwritten sources and a
clean-first serial Debug build. CTest passed 4/4; 17/17 individual slots
passed. Application, MyWorkspace, display-name, campus, and signature targets
were built. Static, source, resource, dependency, offscreen, and diff checks
all passed. Expected notices were missing Vulkan headers/zlib fallback and
LF-to-CRLF normalization; no concrete failures or active commands remained.

This PersonalDetailsPage display-name boundary is closed. Phase 2 remains open;
the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed PersonalDetailsPage Zoom-read reuse)

Against baseline commit `da4f8c74`, completed the three PersonalDetailsPage
reads for `myInfo/zoomLoginId`, `myInfo/zoomPassword`, and
`myInfo/zoomNotAvailable` using the existing typed
`SubPrepPersonalZoomPreferencesPort` and
`ApplicationServicesSubPrepPersonalZoomPreferencesPort`. Qt-free values,
primary-key precedence, legacy fallback/migration, typed defaults, UTF-8
credentials, N/A masking, unavailable state/field enablement, and
save/normalization behavior remain preserved.

The exact implementation/test scope is:

- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/my_workspace_page_tests.cpp`

Aggregate loading remains for typed-signature fields and aggregate save remains
the owner; unrelated behavior is unchanged. No Zoom writer, new contract,
adapter, CMake change, or unrelated migration was introduced.

Verification passed with a serial clean-first Debug build. MyWorkspace passed
17/17; the typed Zoom adapter passed 7/7 and the SubPrep page passed 14/14.
Resource checks passed 6 RCC packs/7 runtime references; the dependency JSON
was valid at 1042 bytes, and diff checks passed for the exact two-file scope.
No commands remain active. The umbrella focused CTest/offscreen runner was
stopped after stalling; bounded individual gates passed. Expected notices were
offscreen `propagateSizeHints`, pre-existing `/FORCE`/duplicate-symbol linker
warnings, and Vulkan/zlib/LF-to-CRLF notices.

This PersonalDetailsPage Zoom-read boundary is closed. Phase 2 remains open;
the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed personal-signature-preferences bundle)

Against baseline commit `554e106b`, added the Qt-free read-only personal
signature-preferences bundle for exact keys `myInfo/signatureMode`,
`myInfo/typedSignatureText`, and `myInfo/typedSignatureFont`. Defaults are
`Image`, empty text, and font `0`; invalid QVariant mode/font values normalize
to those defaults, text preserves UTF-8/whitespace, unavailable settings return
a failure, and the bundle performs no writes.

The exact seven-file scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/next/application/personal_signature_preferences.h`
- `src/next/platform/application_services_personal_signature_preferences_port.h`
- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/next_platform_application_services_personal_signature_preferences_port_tests.cpp`
- `tests/my_workspace_page_tests.cpp`

PersonalDetailsPage maps the typed mode, text, and font to the existing UI
types and removes only the aggregate load from `loadStoredSettings()`. Aggregate
save and all name/campus/Zoom/signature-image/write/guard behavior remain
preserved. CMake registration is included; no unrelated migration was made.

Verification passed ownership validation with 834 handwritten sources and a
serial clean-first Debug build. The new adapter passed 7/7, MyWorkspace passed
18/18, the signature-image adapter passed 6/6, the current-campus adapter
passed 8/8, and the display-name and Zoom adapters passed 7/7 each. Resource
checks passed 6 RCC packs/7 runtime references; dependency JSON was valid at
1042 bytes and the final diff passed. No TIMEOUT or FAIL occurred and no
commands remain active. Executables ran offscreen, so a separate offscreen
launch step was skipped. Expected notices were Qt/Vulkan/zlib and LF-to-CRLF;
stub-test linker notices were also nonblocking.

This personal-signature-preferences boundary is closed. Phase 2 remains open;
the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed PersonalDetailsPage aggregate atomic writer)

Against baseline commit `36773c0b`, added the Qt-free
`PersonalDetailsSaveRequest`/`Result<void>` contract and Qt adapter for exactly
nine keys: `myInfo/name`, `myInfo/campus`, `myInfo/zoomLoginId`,
`myInfo/zoomPassword`, `myInfo/zoomNotAvailable`, `myInfo/signatureImage`,
`myInfo/signatureMode`, `myInfo/typedSignatureText`, and
`myInfo/typedSignatureFont`. The boundary preserves exact UTF-8/whitespace,
image preparation/Base64 encoding, mode/font normalization, one atomic
`saveAll`, unrelated-setting preservation, unavailable-service handling, and
rollback with no partial writes.

The exact seven-file implementation/test scope is:

- `cmake/next.cmake`
- `cmake/tests/next.cmake`
- `src/features/my_info/ui/personal_details_page_sections.cpp`
- `tests/my_workspace_page_tests.cpp`
- `src/next/application/personal_details_save.h`
- `src/next/platform/application_services_personal_details_save_port.h`
- `tests/next_platform_application_services_personal_details_save_port_tests.cpp`

PersonalDetailsPage replaces only the aggregate save. InitialSetupWizard and
repository compatibility ownership, all typed read callers and writers, and
guards remain unchanged. CMake registration is included; no unrelated
migration was made.

Verification recorded the executor focused Debug build and an independent
incremental serial Debug build exiting 0. The save adapter passed 6/6,
MyWorkspace 20/20, signature-image 6/6, current-campus 8/8, display-name 7/7,
and Zoom 7/7. Ownership validation counted 837 handwritten sources; resource
validation passed 6 RCC packs/7 IDs/7 references, dependency JSON was valid at
1042 bytes, and diff checks passed. The independent serial clean-first build
was stopped while compiling without compiler failure; incremental build/tests
passed, and no commands remain active. Expected notices were Vulkan/zlib,
LF-to-CRLF, and stub-linker warnings; no unrelated changes were found.

This PersonalDetailsPage aggregate-writer boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed InitialSetupWizard aggregate-writer cutover)

Against baseline commit `d4dc9e52`,
`InitialSetupWizard::PersonalDetailsWizardPage::validatePage()` now uses the
existing typed `PersonalDetailsSavePort` for all nine keys and values in one
atomic `saveAll`. Repository loading remains for legacy Zoom fallback/migration
and untouched-field preservation.

The exact implementation/test scope is:

- `src/features/setup/ui/initial_setup_wizard.cpp`
- `tests/initial_setup_wizard_tests.cpp`

Trimmed-name validation and warnings, selected/retained signature image,
campus and Zoom fallback/availability, mode/text/font, unrelated settings, and
the existing guards remain preserved. The wizard has no direct settings access;
no new contract, adapter, CMake change, or unrelated migration was introduced.

Configure/ownership checks passed. The executor Debug build passed, and bounded
independent gates passed: InitialSetupWizard 10/10, MyWorkspace 20/20,
PersonalDetailsSavePort 6/6, signature-image 6/6, current-campus 8/8,
display-name 7/7, and Zoom 7/7. Resource validation passed 6 RCC packs/7 IDs/7
references; dependency JSON was valid at 1042 bytes, diff checks passed, and no
test failure or timeout occurred. No commands remain active. Clean-first
encountered environmental MSBuild FileTracker `E_ACCESSDENIED`; bounded gates
passed and no unrelated changes were found. Expected notices were offscreen
Qt/font/plugin, Vulkan/zlib, LF-to-CRLF, and FileTracker notices.

This InitialSetupWizard aggregate-writer boundary is closed. Phase 2 remains
open; the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed InitialSetupWizard read composition)

Against baseline commit `5cf3d9e1`, InitialSetupWizard no longer references
`PersonalDetailsRepository` or accesses settings directly. It composes the
existing typed campus, SubPrep Zoom, signature-image, and
signature-preferences reads into the existing nine-key
`PersonalDetailsSaveRequest` and atomic writer. The name remains trimmed and
UI-owned.

The exact implementation/test scope is:

- `src/features/setup/ui/initial_setup_wizard.cpp`
- `tests/initial_setup_wizard_tests.cpp`

Zoom primary/legacy fallback, migration, and defaults; campus; signature-image
retention/replacement; mode/text/font; unavailable behavior; failure warning;
and unrelated-setting preservation remain unchanged. No new contract, adapter,
CMake change, or unrelated migration was introduced.

Verification passed with an incremental elevated Debug build exiting 0.
InitialSetupWizard passed 11/11, MyWorkspace 20/20, PersonalDetailsSavePort
6/6, campus 8/8, Zoom 7/7, signature-image 6/6, signature-preferences 7/7,
and display-name 7/7. Ownership validation counted 837 handwritten sources;
resource validation passed 6 RCC packs/7 IDs/7 references, dependency JSON was
valid at 1042 bytes, formal Qt-free and call-site checks passed, and the final
diff passed. No test failure, timeout, or active command remained. Expected
notices were `/FORCE`/duplicate-symbol, Vulkan/zlib, and LF-to-CRLF; the
FileTracker environmental qualification remained nonblocking.

This InitialSetupWizard typed-read composition boundary is closed. Phase 2
remains open; the next boundary is not yet selected.
### Progress update - 2026-09-21 (typed language persistence cutover)

Against baseline commit `641f7936` (`Phase2 - Remove InitialSetupWizard
repository reads`), completed the typed language persistence cutover. The
current uncommitted scope is:

- `src/ui/shared/actions/action_registry.cpp`
- `src/ui/shared/state/option_state.h`
- `tests/language_preference_port_tests.cpp`

`OptionState` now exposes the `onPersist` seam. `ActionRegistry` maps legacy
Language `SystemDefault`/`English`/`Korean` to typed
`LanguagePreference` through `SettingsManagerLanguagePreferencesPort`.
Canonical persisted values remain 0/1/5; reads migrate legacy 2/3/4 to
English. Startup selection synchronizes without reapplying presentation
language, and other `OptionState` writers retain compatibility behavior.

Acceptance passed after rebuilding runtime/test targets: an isolated run with a
unique `CLASSMNGR_SETTINGS_ROOT` passed the full `LanguagePreferencePortTests`
8/8, each of the three focused cases passed 3/3, typed adapter,
`LanguageService`, and AI-options regressions passed, and static Qt-free and
call-site checks passed. Earlier shared/alternate-harness 0-vs-1/5/2 failures
were environment/order-dependent; the isolated rerun passed.
### Progress update - 2026-09-21 (typed SaveMode persistence cutover)

Against baseline commit `9ad0fddb` (`Phase2 - Cut ActionRegistry language
persistence over`), completed the typed SaveMode persistence cutover. The
current scope is:

- `src/next/application/save_mode_preferences.h`
- `src/next/platform/settings_manager_save_mode_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_save_mode_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

The `SaveModePreferencesPort` write contract uses canonical `options/saveMode`
values 0=`Automatic` and 1=`Manual`. `ActionRegistry` installs its `onPersist`
cutover before startup selection, preserving `onChanged`/autosave behavior;
malformed or unknown reads fall back to `Automatic`, and other `OptionState`
writers remain unaffected.

An independent elevated Debug rebuild passed after a non-elevated VS FileTracker
access-denied retry. Configure/ownership passed with 837 handwritten sources;
the adapter passed 6/6, AI/ActionRegistry 10/10, and language 8/8. Static
Qt-free, call-site, ownership, and diff checks passed, with each test using a
unique temporary `CLASSMNGR_SETTINGS_ROOT`.
### Progress update - 2026-09-21 (typed AI-comment voice persistence slice)

Against baseline commit `fe29d1cb` (`Phase2 - Cut ActionRegistry SaveMode
persistence over`), added the typed AI-comment voice write contract. The
current five-file scope is:

- `src/next/application/ai_comment_voice_preferences.h`
- `src/next/platform/settings_manager_ai_comment_voice_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_ai_comment_voice_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

Canonical `options/aiCommentVoice` values are 0=`DirectToStudent` and
1=`ThirdPerson`. `ActionRegistry` installs `onPersist` before startup
selection; unknown or missing reads fall back to `DirectToStudent`, while
reload and direct persistence retain canonical values. Other option writers and
UI behavior remain unchanged; no CMake changes were made.

An independent elevated Debug rebuild passed. The voice adapter passed 7/7,
AI/ActionRegistry 10/10, language adapter and regression 8/8 each, and
SaveMode adapter 6/6. Static Qt-free, call-site, ownership, and diff checks
passed; every test used a fresh temporary `CLASSMNGR_SETTINGS_ROOT`.
### Progress update - 2026-09-21 (typed AI-comment provider persistence slice)

Against baseline commit `c0281217` (`Phase2 - Cut ActionRegistry AI voice
persistence over`), added the typed AI-comment provider write contract. The
current five-file scope is:

- `src/next/application/ai_comment_provider_preferences.h`
- `src/next/platform/settings_manager_ai_comment_provider_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_ai_comment_provider_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

Canonical `options/aiCommentProvider` values are 0=`ChatGPT`, 1=`Gemini`,
2=`Claude`, 3=`MicrosoftCopilot`, and 4=`CustomWebsite`. `ActionRegistry`
installs `onPersist` before startup selection; unknown or missing reads fall
back to `ChatGPT`, while reload and direct persistence retain canonical values.
Custom URL behavior and other option writers remain unchanged; no CMake changes
were made.

An independent elevated Debug rebuild passed. The provider adapter passed 7/7,
AI/ActionRegistry 10/10, voice 7/7, language adapter and regression 8/8 each,
and SaveMode 6/6. Static Qt-free, call-site, ownership, and diff checks passed;
every test used a fresh temporary `CLASSMNGR_SETTINGS_ROOT`.
### Progress update - 2026-09-23 (typed document-viewer background persistence)

Against baseline commit `2d7d4a29` (`Phase2 - Cut ActionRegistry AI provider
persistence over`), added the typed document-viewer background write contract.
The five-file scope is:

- `src/next/application/document_viewer_background_preferences.h`
- `src/next/platform/settings_manager_document_viewer_background_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_document_viewer_background_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

Canonical `options/documentViewerBackground` values are 0=`Default`,
1=`White`, and 2=`Black`. `ActionRegistry` installs `onPersist` before startup
selection; malformed or unknown reads fall back to `Default`. The existing
MainWindow `onChanged` propagation and PDF viewer behavior remain intact; no
CMake changes were needed.

The focused Debug rebuild passed after retrying a Visual Studio FileTracker
access-denied failure with elevated access. A serial CTest run passed all 9
selected targets, including the background adapter, AI/ActionRegistry,
PageManager, StartupVisualSettings, and provider, voice, language, and SaveMode
regressions. CMake ownership validation passed with 837 handwritten sources;
Qt-free, call-site, source-path, and diff checks passed. Tests used an isolated
`CLASSMNGR_SETTINGS_ROOT`.
### Progress update - 2026-09-23 (typed document-page-spacing persistence)

Against baseline commit `790082c4` (`Phase2 - Cut ActionRegistry viewer
background persistence over`), completed the typed document-page-spacing
persistence cutover. The Qt-free `DocumentPageSpacingPreferencesPort` now
exposes `write()`, and the SettingsManager adapter persists valid `None`,
`Small`, `Medium`, and `Large` values as `0`, `1`, `2`, and `3`. Invalid typed
values are rejected without modifying storage. Existing reads remain
compatible: missing or unknown values map to `Small`, while malformed text
continues through unchecked `QVariant::toInt()` and maps to `None`.

`ActionRegistry` installs typed persistence before initial state selection;
MainWindow/PageManager update propagation remains unchanged. The expected
implementation/test scope is:

- `src/next/application/document_page_spacing_preferences.h`
- `src/next/platform/settings_manager_document_page_spacing_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_document_page_spacing_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

No CMake changes were made.

Independent review, build, and serial CTest passed for
`ClassMngrNextPlatformSettingsManagerDocumentPageSpacingPreferencesPortTests`,
`ClassMngrAiCommentOptionsTests`, `ClassMngrPageManagerTests`, and
`ClassMngrStartupVisualSettingsTests`; `git diff --check` was clean. Phase 2
remains open; this document-page-spacing persistence boundary is closed.
### Progress update - 2026-09-23 (typed font-size persistence cutover)

Completed the typed FontSize writer cutover in the current Phase 2 working
tree. The Qt-free `FontSizePreferencesPort` now exposes `write()`, and the
SettingsManager adapter writes only canonical offsets `Small=-2`, `Normal=0`,
`Large=2`, and `ExtraLarge=4`; invalid typed values are ignored without
changing storage. Missing, unknown, and malformed reads continue to fall back
to `Normal`.

`ActionRegistry` installs typed `onPersist` before startup selection and no
longer performs the direct raw write. `FontSizeController` `onChanged`
behavior remains unchanged. The exact five-file scope is:

- `src/next/application/font_size_preferences.h`
- `src/next/platform/settings_manager_font_size_preferences_port.h`
- `src/ui/shared/actions/action_registry.cpp`
- `tests/next_platform_settings_manager_font_size_preferences_port_tests.cpp`
- `tests/ai_comment_options_tests.cpp`

Independent source review passed. The targeted Debug rebuild succeeded after
the normal MSBuild FileTracker `E_ACCESSDENIED` retry with elevated access;
registered CTest targets passed: `ClassMngrNextPlatformSettingsManagerFontSizePreferencesPortTests`,
`ClassMngrAiCommentOptionsTests`, `ClassMngrStartupVisualSettingsTests`, and
`ClassMngrFontManagerTests`. No CMake changes were made; `git diff --check`
was clean. Phase 2 remains open.
### Progress update - 2026-09-23 (Sub Prep schedule-summary query contract)

`src/next/application/sub_prep_schedule_summary_query.h` adds a Qt-free
Application query for typed visible class IDs, Application-owned weekdays,
and typed regular/intensive schedule mode. It uses an injected read port and
reuses `ClassSummaryProjection`; projection validation is all-or-nothing and
ordering is deterministic. Empty visible-class or selected-day scopes return
a successful empty projection without reading.

The app-less test is
`tests/next_application_sub_prep_schedule_summary_query_tests.cpp`, registered
in `cmake/tests/next.cmake`. Release configuration validated ownership of 701
handwritten sources, `ClassMngrNext` built, and
`ctest -R ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests
--output-on-failure` passed 1/1. The query is not connected to the legacy page
and has no persistence adapter; batching, output/UI migration, feature parity,
and memory improvement remain unclaimed. Phase 2 remains In progress.
### Progress update - 2026-09-23 (Sub Prep selected-details and selection-state contracts)

Commit `389d90a6a433ae6c5c7ce7263daba02f4a27a5ce` adds the Qt-free
`SubPrepClassDetailsQuery` for one selected class. Its injected read port
propagates structured failures, verifies the returned class ID, and permits a
missing teacher identity. `ClassMngrNextApplicationSubPrepClassDetailsQueryTests`
passed 1/1.

Commit `7959eb07` adds `SubPrepClassInformationState` for scope refresh,
selection, detail application, and clear. It retains only a selection within
the visible scope, clears details on each successful refresh or selection
change, and accepts details only when class and teacher identity match. The
focused `ClassMngrNextApplicationSubPrepClassInformationStateTests` passed 1/1. Both
app-less targets compiled and linked in the Windows x64 developer environment;
neither contract is connected to legacy UI or persistence. No batching,
package/PDF migration, or memory reduction is claimed. Package/roster/PDF and
Release memory gates remain later work. Phase 2 remains In progress.
### Progress update - 2026-09-23 (Sub Prep operation-scoped print-source contract)

`SubPrepPrintSourceRequest`, `SubPrepPrintSourceReadPort`, and
`SubPrepPrintSourceQuery` in
[`sub_prep_print_source_query.h`](../../src/next/application/sub_prep_print_source_query.h)
define a Qt-free boundary for selected class IDs, weekdays, and regular or
intensive schedule mode. The query validates the full request before reading,
returns an empty source without I/O for an empty class or day scope, propagates
structured read errors, and validates the complete bounded result before
returning an owned source. Class rows refer to teacher IDs; teacher values are
stored once, and missing teachers or a subset of requested classes are
permitted.

[`ClassMngrNextApplicationSubPrepPrintSourceQueryTests`](../../tests/next_application_sub_prep_print_source_query_tests.cpp)
is an app-less test registered in [`next.cmake`](../../cmake/tests/next.cmake).
The focused target build passed and CTest passed 1/1. No Sub Prep adapter,
page/UI, or PDF wiring, SQL batching, release/memory improvement, roster/package
migration, or output parity was established. Phase 2 remains In Progress.
### Progress update - 2026-09-23 (custom-color palette caller boundary)

Commit `83163b0a` completed the custom-color palette caller seam. [`ColorUtils`](../../src/core/utils/colorutils.h)
now accepts the existing [`CustomColorPalettePreferencesPort`](../../src/next/application/custom_color_palette_preferences.h)
and has no `SettingsService` or Platform dependency. All seven UI picker call
sites pass the existing Platform adapter. Picker behavior and the stored
palette format were preserved; no other UI-service boundary is covered by
this slice.

The offscreen QtTest [`ColorUtilsCustomColorPaletteTests`](../../tests/colorutils_custom_color_palette_tests.cpp)
covers load/save across all 16 `QColorDialog` custom-color slots, canonical
writes, and slot restoration. The
[`NextPlatformApplicationServicesCustomColorPalettePreferencesPortTests`](../../tests/next_platform_application_services_custom_color_palette_preferences_port_tests.cpp)
also covers null `SettingsService` defaults and no-op writes.
Both Windows x64 Ninja targets built and focused CTest passed 2/2; diff checks
passed. Generic settings persistence remains open. This is not the Phase 3
persistence rewrite or a Phase 7 feature migration; Phase 2 remains In
Progress.
### Progress update - 2026-09-23 (calendar import planning contract)

Commit `d5a5cab9` adds the Qt-free
[`CalendarEventImportPlan`](../../src/next/application/calendar_event_import_plan.h)
and uses it from
[`CalendarEventImportService::handleFinished`](../../src/features/calendar/calendar_event_import_service.cpp).
After the legacy range read, the service supplies exact UTF-16 signature keys
for existing and candidate events. The planner returns accepted candidate
indices in input order and carries forward parser skips, preserving duplicate
behavior including exact comparison of malformed surrogate sequences. The
batch save, error, metric, and signal paths remain in the legacy service.

[`NextApplicationCalendarEventImportPlanTests`](../../tests/next_application_calendar_event_import_plan_tests.cpp)
and [`CalendarImportTests`](../../tests/calendar_import_tests.cpp) cover
planning order/counts, exact six-field identity behavior, and UTF-16 equality.
Both Windows x64 Ninja targets built and focused CTest passed 2/2. Range
retrieval and batch persistence remain legacy responsibilities; this contract
does not complete calendar import migration or Phase 2. The existing-event
signature-key read is completed in commit `d14155c1` below; candidate parsing
and batch save remain unchanged. The general event projection has a 4,096-row
cap and stricter metadata validation, so the import uses a dedicated key read
to preserve legacy range behavior.
### Progress update - 2026-09-23 (calendar import existing-signature read cutover)

Commit `d14155c1` adds
[`ApplicationServicesCalendarEventPort::importSignatureKeysInRange`](../../src/next/platform/application_services_calendar_event_port.h)
and routes duplicate planning through it. The port reads the same requested
date range, preserves legacy result order, and returns UTF-16 keys matching
`CalendarImport::calendarEventImportSignature`. It bypasses the general
4,096-row event projection and its stricter metadata validation. Candidate
parsing and batch save remain in the legacy import service.

`ClassMngr` and the focused calendar-event port target built on Windows x64
Ninja; focused CTest passed 1/1 and `git diff --check` passed. Phase 2 remains
open. The next slice integrates the Sub Prep print-source contract with a
production read adapter; the contract still has no adapter, page, or PDF
wiring, and no output-parity or memory acceptance is claimed.
### Progress update - 2026-09-23 (Sub Prep print-source Platform read adapter)

Commit `2daae4ef` adds
[`ApplicationServicesSubPrepPrintSourcePort`](../../src/next/platform/application_services_sub_prep_print_source_port.h)
and a scoped `ClassService::classInfosForScheduleScope` /
`ClassInfoRepository::loadClassInfosForScheduleScope` SQL read. The query
filters requested class IDs, selected weekdays, and the selected regular or
intensive schedule table before materializing class information. Positive
typed IDs are validated and rendered as decimal integer literals, keeping the
4,096-class scope within SQLite's bind limit. Per-class and aggregate meeting
limits read at most one sentinel row beyond each bound and report overflow.

The adapter returns owning values, preserves requested-class order, copies
each referenced teacher once, and omits classes without selected meetings or
with missing class information, an unassigned teacher, or a missing teacher.
Roster lookup failures retain the legacy zero-count fallback. The class-scope
read requires the active repository session and has no `DataService` fallback.

`ClassMngr`, the Platform adapter test target, and the app-less print-source
query target built on Windows x64 Ninja. Focused CTest passed
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests` and
`ClassMngrNextApplicationSubPrepPrintSourceQueryTests`; `git diff --check`
passed. This closes the selected-scope source-read seam only. Page/PDF wiring,
output parity, teacher/roster batching, Release memory acceptance, and full
Sub Prep completion remain open; Phase 2 remains In Progress. The next bounded
slice adds the selected-class details Platform read adapter, initially
unconnected to the page.
### Progress update - 2026-09-23 (Sub Prep selected-class details Platform read)

The selected-class Application details value now exposes separate bounded
fields for room, WiFi name, WiFi password, internet type, Zoom ID, Zoom
password, and projection type. The session-backed
`ApplicationServicesSubPrepClassDetailsPort` reads one class through
`ClassService` and the active repository session; it copies only class notes,
preferred teacher display name, those seven fields, and teacher notes. It does
not query either schedule table and has no `DataService` fallback. An existing
class without a `class_info` row returns blank details; absent classes return
`NotFound`. Unassigned, missing, or stale teacher references produce empty
teacher values.

The Windows x64 Debug build succeeded. Focused `NextApplicationClassSummary`,
`NextApplicationSubPrepClassDetailsQuery`, and
`NextPlatformApplicationServicesSubPrepPrintSourcePort` suites passed. This
establishes the bounded read seam only: no page wiring, full legacy-output
parity, memory improvement, or broader Sub Prep/Phase 2 acceptance is claimed.
Page/output integration and parity plus the large-workspace Release memory
evidence remain open. The next Phase 2-bounded Sub Prep continuation is the
scoped schedule-summary persistence read behind its existing Application
contract. Phase 2 remains In Progress.
### Progress update - 2026-09-23 (Sub Prep scoped schedule-summary Platform read)

Commit `dd429838` adds
[`ApplicationServicesSubPrepScheduleSummaryPort`](../../src/next/platform/application_services_sub_prep_schedule_summary_port.h)
for the existing scoped schedule-summary query. It reads only the requested
class/day/mode scope through the active `ClassService` repository session and
returns bounded owning class and teacher summary values. Roster counts are
batched; a roster read failure preserves the zero-count fallback. Empty class
or day scopes return without reads, and the adapter has no `DataService`
fallback. The intensive-mode test succeeds with `class_times` dropped,
proving the mode-specific read does not require the regular schedule table.

The Windows Debug build of
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`
succeeded. Focused CTest passed
`ClassMngrNextApplicationClassSummaryTests`,
`ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests`, and
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests` 3/3 in
13.39 seconds. Configure validated 857 handwritten files, and
`git diff --check` passed. This closes the scoped schedule-summary read seam
only. Phase 2 remains In Progress; page/output wiring, behavior parity, and
96-class Release memory evidence remain open. Work Package D is now complete;
the following progress update records the model-backed list/navigation and
reusable selected-class details view.
### Progress update - 2026-09-23 (Sub Prep model-backed navigation and reusable details view)

Work Package D wires the schedule-summary and selected-class details queries
to the live Sub Prep class-information view. A Qt list model filters and orders
the compact `ClassSummaryProjection` by grade and configured level; one
`QListView` replaces per-class pages, and one details card is reused as the
selection changes. `SubPrepClassInformationState` owns the selected typed
class ID and current bounded details. Refresh keeps the selected class only
while it remains in the schedule scope, and the schedule display-mode signal
refreshes the projection in place.

Windows x64 Debug Ninja built `ClassMngr`,
`ClassMngrSubPrepClassInformationListModelTests`,
`ClassMngrSubPrepPageTests`, and `ClassMngrStartupPerformanceTests`. Focused
CTest passed the list-model and page suites 2/2; configure validated 860
handwritten files and `git diff --check` passed. The startup-performance
target compiled, but this slice did not run the packaged 96-class Release
route. This closes the model-backed view integration only. Page-leave resource
release, output/package/PDF migration, parity, and Release memory acceptance
remain open; Phase 2 remains In Progress. Work Package E is next.
### Progress update - 2026-09-23 (Sub Prep page-leave lifecycle release)

Work Package E uses the existing `BasePage::deactivate()` resource-release
hook. When PageManager leaves Sub Prep, the page clears the selected details,
summary projection, and selection state, then marks itself stale. On the next
activation, Sub Prep reloads the current schedule scope and one selected-class
detail instead of retaining the prior operation state.

Windows x64 Debug Ninja built `ClassMngr` and `ClassMngrSubPrepPageTests`;
focused CTest passed the page suite 1/1. The lifecycle test verifies the list
and details are cleared at deactivation and restored on activation, including
a new details read. This closes page-leave release only. Package/PDF output
migration, parity, and 96-class Release memory acceptance remain open; Phase 2
remains In Progress. Work Package F is next: connect the operation-scoped
print-source query to package generation.
### Progress update - 2026-09-24 (Sub Prep information-sheet print-source integration)

Work Package F1 connects the Sub Prep page's output request to
`SubPrepPrintSourceQuery`. After dialog acceptance, the page sends the
selected class IDs, selected weekdays, and current regular/intensive mode to
the query. The bounded owning result is mapped into the existing
`SubPrepClassInformation::TeacherGroup` renderer model. The Application value
now carries English name, Korean name, preferred name, and preferred
romanization so the mapper preserves `Teacher::preferredDisplayName()` and
the existing teacher facts.

The previous output path loaded all classes and then looked up each class's
information, roster count, and teacher. That path is removed for the main
information sheet. The query source is released when the mapper returns. The
separate roster-PDF package stage still reads legacy class, teacher, and full
roster values; `SubPrepDocumentModel` still copies the renderer model. No full
package output parity or memory improvement is claimed.

Windows x64 Debug Ninja built `ClassMngr`, the page, mapper, Application
query, Platform adapter, PDF, and package targets. Focused CTest passed 6/6 for
the mapper, page, print-source query and Platform adapter, PDF renderer, and
package service. This closes only the main information-sheet read integration.
Work Package F continues with the remaining output-source and renderer-model
lifetime boundaries; parity, cancellation/error cleanup, the 96-class
packaged Release memory gate, and Phase 2 remain open.
### Progress update - 2026-09-24 (Sub Prep information-sheet render model lifetime)

Work Package F2 removes the second rich `TeacherGroup` list created when
`SubPrepDocumentModel::build()` copied `SubPrepPrintService::Request` into its
renderer value. `Document::classInformation` now holds an explicit const
reference wrapper to the request's list. `saveSubPrepPdf()` keeps the source
request alive in a named local through the synchronous `SubPrepPdfRenderer`
call; no renderer-retained pointer is introduced. The page moves its print
request into the package request so this handoff does not clone the
`TeacherGroup` list.

The PDF test verifies that the render document refers to the exact request
list. Windows x64 Debug Ninja built `ClassMngr`, the PDF, package, and page
targets; focused CTest passed those suites 3/3. This removes one overlapping
rich model allocation only. The package request still owns those values after
the main sheet finishes, and roster PDF generation still reads full legacy
class, teacher, and roster values. Work Package F continues with stage release
and the roster-source contract; output parity, 96-class packaged Release
memory acceptance, and Phase 2 remain open.
### Progress update - 2026-09-24 (Sub Prep package stage release)

Work Package F3 makes `SubPrepPackageService::generate()` take ownership of
its operation request by value. `SubPrepPage` moves its completed package
request into the call. `generateAt()` now writes the information-sheet PDF
before loading full class, teacher, and roster values; after that PDF succeeds,
it clears `request.subPrep` before beginning the roster-output stage. This
releases the schedule and rich information-sheet model instead of overlapping
them with the roster projection.

Windows x64 Debug Ninja built `ClassMngr`,
`ClassMngrSubPrepPackageServiceTests`, and `ClassMngrSubPrepPageTests`.
Focused CTest passed the page, PDF, and package suites 3/3. Existing package
tree, document order, and status behavior tests pass. The roster stage still
uses legacy class, teacher, and full roster reads; the next Work Package F
slice adds an operation-scoped roster source. Output parity, the 96-class
packaged Release memory gate, and Phase 2 remain open.
### Progress update - 2026-09-24 (Sub Prep roster-output Application contract)

Work Package F4 adds the Qt-free
`SubPrepRosterOutputSourceRequest`, read port, bounded source value, and query.
The request carries selected class IDs, weekdays, schedule mode, and requested
extra roster columns. The source holds only renderer-facing class/teacher
names, selected schedule facts, room/network/Zoom values, requested roster
columns, and owning cell strings. Query validation enforces unique in-scope
IDs and teacher links, selected weekdays, row shape, per-class and aggregate
row/cell/text limits, and no I/O for empty class/day scopes.

The app-less query test covers request and source rejection, no-read behavior,
read failure propagation, stable port order, maximum per-class dimensions, and
aggregate overflow. CMake source ownership validated 865 handwritten files;
Windows x64 Debug built `ClassMngr` and the query target, and focused CTest
passed 1/1. Query-side caps do not replace adapter-side limits: the next slice
must enforce bounds while reading the active database session, before
materializing roster rows. Package integration, PDF/package parity, the
96-class packaged Release memory gate, and Phase 2 remain open.
### Progress update - 2026-09-24 (Sub Prep bounded roster repository read)

Work Package F5 adds `RosterRepository::loadRosterForOutput` and forwards it
through `DataService` and `RosterService`. The query resolves only requested
columns, checks `MAX(row_index)` on those physical columns, and validates the
dense row/cell budget before allocating the projected matrix. SQL reads are
forward-only; selected values use a bounded substring and the original byte
length is checked before conversion. Unrequested column values are never read.

`ClassMngr` and `ClassMngrDataServiceLifecycleTests` built on Windows x64
Debug Ninja; the lifecycle suite passed 1/1. It covers selected-column order,
row/cell/text budgets, an oversized stored cell, and a sparse out-of-range row
index. The repository method enforces aggregate budgets supplied by its
caller; the next Platform adapter must decrement those budgets across the
operation while mapping bounded values. Package integration, output parity,
the 96-class packaged Release memory gate, and Phase 2 remain open.
### Progress update - 2026-09-24 (Sub Prep roster-output Platform read)

Work Package F6 adds
[`ApplicationServicesSubPrepRosterOutputSourcePort`](../../src/next/platform/application_services_sub_prep_roster_output_source_port.h).
It maps canonical typed IDs to legacy IDs, reads the requested class/day/mode
scope through the active services, retains unassigned classes for roster
output, shares teacher facts, and projects only baseline and requested roster
columns. Remaining operation row/cell/text budgets are passed to the bounded
roster service before values become the Application-owned source. The scoped
schedule read now accepts an explicit include-unassigned option; existing
callers retain their prior default behavior.

The new database-backed Platform test covers request ordering, day and mode
selection, unassigned teachers, roster column projection, sparse-row overflow,
and oversized teacher output. CMake validated 867 handwritten source owners.
Windows x64 Debug built `ClassMngr`; focused CTest passed the new Platform
suite, the existing Sub Prep print-source Platform suite, and the roster-output
Application query suite (3/3). `git diff --check` passed. This closes the
bounded roster-source adapter only. Package-service wiring, renderer mapping,
output parity/cleanup, 96-class packaged Release memory evidence, and Phase 2
acceptance remain open. Work Package F7 wires the source into package
generation.
### Progress update - 2026-09-24 (Sub Prep package roster-source integration)

Work Package F7 replaces the package service's direct legacy class, teacher,
and roster reads with `SubPrepRosterOutputSourceQuery`. The request carries
typed selected class IDs, dates, schedule mode, and extra columns. The package
service maps bounded Application values into the existing renderer model,
checks canonical integer IDs and UTF-8 round trips, and preserves the current
class ordering, folder naming, package tree, and per-class/daily/by-day output
selection. `SubPrepPage` owns the session-backed Platform adapter for the
synchronous generation call; the package service has no `ApplicationServices`
or `DataService` dependency.

Windows x64 Debug built `ClassMngr`, the package service tests, and the page
tests. Focused CTest passed 5/5 for package, page, PDF, Application query, and
Platform source suites. Tests cover scoped class/day/mode/column forwarding,
daily and per-class output, and source/mapping failure cleanup before a final
package is committed. Full PDF/package parity and the 96-class packaged
Release memory gate have not been established. Work Package F8 compares output
with retained references and closes cancellation/error/cleanup parity before
the Release acceptance run; the broader Phase 2 exit gate remains open.
### Progress update - 2026-09-24 (Sub Prep output-reference parity)

Work Package F8 extends the 96-class package probe to compare the generated
`Sub Prep.pdf` and Daily roster PDF with the committed references under
`docs/qt-rewrite/visual-baseline/release/sub-prep-output/reference/`. It checks
page counts, every page's point dimensions, and extracted text. On Windows it
also renders every page at 150 DPI and compares the images exactly. The
information-sheet and Daily roster PDFs match the references at 19 and 16
pages. PDF bytes are regenerated and are not used as the parity oracle.

Cancellation and failure cleanup coverage confirms that print-only temporary
PDFs are removed and no package directory is committed on cancellation, roster
read failure, or invalid text mapping. Windows x64 Debug built the package
suite; focused CTest passed 5/5 for package, page, PDF, Application query, and
Platform source suites. Next is the packaged 96-class Windows x64 Release
memory gate; UI visual-state and the broader Phase 2 exit gates remain open.
### Progress update - 2026-09-24 (Sub Prep packaged Release measurement)

Work Package F9 extends the Sub Prep output-boundary workflow test to capture
working set at both one and five seconds after completion. The updated Windows
x64 Debug startup test target built, and the route test passed while driving
the packaged Release application. Route-scoped validation passed for
`output-sub-prep`, with two PDFs, 17 pages, normal exit, and no timeout. This
single-route run leaves the aggregate Phase 0 exit gate incomplete, as
expected.

The report measured a 315,740,160-byte peak working set and 351,821,824-byte
peak private usage, compared with retained legacy peaks of 498,176,000 and
480,948,224 bytes. Settled working set was 306,466,816 bytes at one second and
306,470,912 bytes at five seconds. The route is below the temporary 512 MiB
diagnostic ceiling but remains above the final 250 MiB target. Package output
parity and this route result do not close the broader Phase 2 exit gate; the
typed calendar UI/page migration, generic settings persistence, remaining
feature-service migrations, and broader document-service migration remain.
### Progress update - 2026-09-24 (typed calendar import batch-save boundary)

The calendar import service now maps only the duplicate planner's accepted
candidate indices into `CalendarEventImportSaveRequest`. The Qt-free request
validates every typed create value, rejects update IDs, supports the empty
batch used when all candidates are duplicates, and bounds one batch at 4,096
events. The Platform adapter converts the ordered batch and calls
`CalendarService::saveEvents()` once, keeping the repository transaction over
the whole import; it returns typed event IDs in the same order. Import metrics,
skipped counts, failure reporting, and completion signals continue to use the
same operation result.

Windows x64 Debug built `ClassMngr` and the Application, Platform, and parser
test targets. CMake validated 869 explicit source owners. Focused CTest passed
3/3 for `ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and
`ClassMngrCalendarImportTests`. Database tests cover ordered IDs and event
fields, duplicate-only no-op, invalid-batch preflight, unavailable service,
and rollback after a later insert fails. Workbook parsing and campus-directory
lookup remain legacy responsibilities; the calendar import migration is not
complete, and broader Phase 2 work remains open.
### Progress update - 2026-09-24 (typed calendar availability boundary)

The calendar feature no longer calls `ApplicationServices::calendarService()`
directly. Import-start and dialog-opening availability guards now use
`ApplicationServicesCalendarEventPort::isAvailable()`, which contains the
legacy service check and treats exceptions as unavailable. This preserves the
existing early-return behavior while keeping raw service ownership in the
Platform boundary. Windows x64 Debug built `ClassMngr`, the calendar import
test target, and the Platform calendar event suite; focused CTest passed 2/2.
The broader calendar UI/value migration and other feature-service migrations
remain open.
### Progress update - 2026-09-24 (typed calendar reset mutation)

The calendar preferences panel now routes its confirmed reset through
`CalendarEventDeleteAllPort` and
`ApplicationServicesCalendarEventDeleteAllPort`. The adapter exposes service
availability separately so the UI preserves the existing behavior of checking
availability before displaying the destructive confirmation. The panel keeps
the same prompt, warning, success status, and refresh notification and no
longer stores `CalendarService`.

Windows x64 Debug built `ClassMngr` and the Application and Platform calendar
event test targets. CMake validated 871 explicit source owners. Focused CTest
passed 2/2; database cases cover successful reset, unavailable service, and a
trigger-injected delete failure that leaves its event intact. The import
signature read, generic settings persistence, remaining feature-service
migrations, and broader document migration remain open.
### Progress update - 2026-09-24 (typed calendar edit-draft flow)

Calendar day activation now creates a `CalendarEventEditDraft`, and event
activation maps the already typed `CalendarEventSummary` directly to a draft.
The page passes that value into the dialog and bases repeat, delete, and save
requests on it. The old projection-to-`CalendarEvent`-to-draft conversion is
removed, as are the unused legacy `QList<CalendarEvent>` upcoming filter and
its visibility overload.

Windows x64 Debug built `ClassMngr`, `ClassMngrDialogShellTests`, and the
Platform calendar event suite. Focused CTest passed 2/2. Broader calendar
visual-state and page integration coverage remains part of the Phase 2 exit
work.
### Progress update - 2026-09-24 (calendar display-preference boundary)

`CalendarPreferencesPanel` now passes its `ApplicationServices*` to
`ApplicationServicesCalendarEventDisplayPreferencesPort` rather than retaining
a `SettingsService*` solely to construct the adapter. The adapter continues to
own the same setting keys, default-false reads, unavailable-save no-op, and
atomic two-key save. Windows x64 Debug built `ClassMngr` and the Platform
display-preferences suite; focused CTest passed 1/1, including pointer-based
round-trip and null/unavailable service behavior.
### Progress update - 2026-09-24 (academic calendar preference-port injection)

AcademicCalendarProvider now owns the Application schedule and first-day
preference ports. CalendarPage and evaluation-default selection construct
the Platform adapters and inject those ports, removing SettingsService from
the provider. The schedule, first-day, and display-preference Platform
adapters no longer expose SettingsService-pointer constructors.

Windows x64 Debug built ClassMngr, ClassMngrAcademicCalendarTests, and the
three Platform preference suites. Focused CTest passed 4/4; CMake validated
871 source owners, a provider source search found no SettingsService
references, and git diff --check passed. The remaining calendar
upcoming-events preference callers and broader Phase 2 migrations remain open.
### Progress update - 2026-09-24 (calendar event-type color preference boundary)

Calendar event-type color reads and writes now construct
`ApplicationServicesCalendarEventTypeColorPreferencesPort` from
`ApplicationServices*`. The feature no longer gates these operations with or
passes a raw `SettingsService*`; unavailable reads retain the default-color
fallback and unavailable saves remain no-ops.

Windows x64 Debug built `ClassMngr` and the Platform color-preference suite;
focused CTest passed 1/1, including ApplicationServices-pointer round-trip
and null-service fallback. `git diff --check` passed. Other upcoming-events
preference access and broader Phase 2 migrations remain open.
### Progress update - 2026-09-24 (current-campus availability and options boundary)

`Application::CurrentCampusPreferencesPort` exposes Qt-free `isAvailable()`;
`Platform::ApplicationServicesCurrentCampusPreferencesPort` maps availability
to the legacy service. CalendarPage upcoming-event options use the port before
preference reads and campus projection, preserving the old availability gate.
The feature file no longer retains or queries `SettingsService`.

Windows x64 Debug built `ClassMngr` and the CurrentCampus preference test
target. Focused CTest passed 1/1 for available, unavailable, null
`ApplicationServices`, and null `SettingsService`; `git diff --check` passed.
No dedicated CalendarPage test exists; the Tester judged the port tests plus
guard/source comparison reasonable for this refactor. Other calendar
input/lookup work and broader Phase 2 migrations remain open.
### Progress update - 2026-09-24 (calendar import signature-query contract)

The exact existing-signature read now crosses the Qt-free Application
`CalendarEventImportSignatureQueryPort`, with a dedicated Platform
`ApplicationServicesCalendarEventImportSignatureQueryPort`. The import
workflow uses the port for availability and date-range signatures; the former
`ApplicationServicesCalendarEventPort::importSignatureKeysInRange` method has
been removed. Platform's enforced dependencies remain Application and
Qt6::Core.

Windows x64 Debug built `ClassMngr` and both query-port targets. Independent
focused CTest passed 2/2; CMake validated 874 source owners, and
`git diff --check` passed. Coverage preserves the six-field QString/UTF-16
signature, ordering, invalid/unavailable/read failures, and 4,097-row results
beyond the general projection cap. Workbook parsing and campus-directory
lookup remain legacy; broader calendar import, generic settings, other
feature-service migrations, and Phase 2 remain open.
### Progress update - 2026-09-24 (personal display-name caller migration)

Schedule output, schedule import, and the Sub Prep print dialog now construct
`ApplicationServicesPersonalDisplayNamePreferencesPort` from
`ApplicationServices&`, removing their `SettingsService*` adapter callers.
Unavailable reads remain empty and writes remain silent no-ops; null services
retain empty/default names. Schedule output reads only after dialog acceptance
and preserves stored whitespace, while import and Sub Prep continue to trim.

The ScheduleWidget tests cover a preference value changed by the accepted
signal, including exact whitespace in the print request, unavailable
preferences, and null services. Independent verification built
`ClassMngrScheduleWidgetTests`, `ClassMngrScheduleImportDialogTests`, and
`ClassMngrSubPrepPageTests`; CTest passed 3/3 and ScheduleWidget passed 19/19.
`git diff --check HEAD` passed with line-ending conversion notices. My
Information and Initial Setup remain display-name adapter callers; generic
settings persistence, other feature-service migrations, and Phase 2 remain
open.
### Progress update - 2026-09-24 (F20 My Information and Initial Setup migration)

My Information and Initial Setup now construct
`ApplicationServicesPersonalDisplayNamePreferencesPort` from
`ApplicationServices&`. The adapter's `SettingsService*` constructor and its
constructor-only test were removed. Existing availability guards remain;
My Information preserves stored UTF-8/whitespace, Initial Setup fills only a
blank name field, and both retain aggregate personal-details saves.

An independent fresh Windows x64 Ninja/MSVC build completed 322 steps and
compiled the changed production and test translation units. Initial Setup and
adapter test targets passed. In My Workspace, F20 display-name, availability,
aggregate-save, and rollback cases passed; three PageManager cases failed
before reaching F20 code because required documents/campuses resource packs
were unavailable, and no baseline checkout was available. The independent
review found no F20 defect and `git diff --check` passed. Setup's prefilled-name
reinitialization behavior still lacks a direct assertion; its fill-only-if-blank
source condition remains. Generic settings persistence, other feature-service
migrations, and Phase 2 remain open.
### Progress update - 2026-09-24 (F21 unused class-navigation preferences cleanup)

Removed the unused `ClassNavigationPreferences` header and implementation,
their production and Classes Page test source entries, and stale includes.
`speaking_eval_page_p.h` now includes `class_tab_navigation_model.h` directly
for `ClassTabNavigation`. The active typed Application contracts, Platform
adapters, and preference tests remain.

Independent fresh Windows x64 Ninja/MSVC verification configured and built
376+8 steps; CTest passed 8/8 across ClassesPage, ClassTabNavigation, evaluation
defaults, and the five typed preference suites. CMake validated 872 source
owners. Searches found no deleted API/file references in `src`, `tests`,
`cmake`, or `CMakeLists.txt`; `git diff --check` passed. Phase 2 remains open.
At the close of F21, the next planned slice was calendar-import campus-code
lookup; F22 completes it below. Workbook decoding and other campus lookups
remain legacy.
### Progress update - 2026-09-24 (F22 calendar-import campus-code query)

Calendar import now obtains campus codes through the Qt-free
`Application::CalendarEventImportCampusCodeQueryPort` and the Platform
`CalendarEventImportCampusCodeQueryAdapter`. The adapter owns `ResourcePaths`
and `CampusJsonRepository` access and accepts an injected directory for tests;
`CalendarEventImportService` no longer performs direct resource or repository
lookup. The query returns `std::vector<std::string>`.

The adapter preserves repository campus-name ordering, trims codes, removes
blank values, and retains only the first exact duplicate. Default,
malformed, and unreadable records are skipped; missing or empty directories
produce no codes. Workbook parsing, parser behavior, and CalendarPage behavior
are unchanged. A fixture includes actual Korean UTF-8. Independent fresh
Windows x64 Ninja/MSVC Debug configure and full 356-step build passed; CMake
validated 875 source owners, focused parser and adapter CTest passed 2/2, and
source/dependency and `git diff --check` reviews passed. Phase 2 remains open.
The next planned slice was CalendarPage's distinct campus metadata read; F23
completes it without reusing or widening the importer's campus-code-list port.
### Progress update - 2026-09-24 (F23 CalendarPage campus-directory query)

CalendarPage campus metadata now crosses the Qt-free
`Application::CalendarPageCampusDirectoryQueryPort`; its Platform adapter
reads through `CampusJsonRepository` and accepts an injected fixture directory.
The query returns owning UTF-8 IDs and names with an optional code. CalendarPage
no longer reads `CampusJsonRepository` or `ResourcePaths` directly, and the
F22 importer code-list port remains separate.

Source comparison confirmed the existing availability branch and timing,
repository order, alias order and matching, trimmed display-name fallback,
whitespace-only-code behavior, and exact-empty removal/deduplication. Adapter
coverage includes Unicode and missing, empty, whitespace, blank, malformed,
default, and missing-directory cases. Executor and independent fresh Windows
x64 Ninja/MSVC configure/builds validated 878 handwritten owners; both focused
CTest runs passed 3/3 for the F23 adapter, F22 adapter, and CalendarEventCache.
No dedicated CalendarPage test exists. This closes the campus-directory
migration seam only; workbook parsing and broader calendar migration remain
open. The next slice is the personal-signature-image caller cutover.
### Progress update - 2026-09-24 (F24 personal signature-image caller cutover)

`ApplicationServicesPersonalSignatureImagePort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes the `SettingsService*` constructor. All five reads in
Initial Setup, My Information, and Speaking Eval now use the Application
adapter. The `myInfo/signatureImage` key, Base64 conversion, one-time
`SignatureImage::prepareForEmbedding`, read-only behavior, empty results, and
existing availability guards remain unchanged.

Executor verification reused the Ninja/MSVC x64 configure with 878 handwritten
owners, built `ClassMngr` and the adapter, Initial Setup, and MyWorkspace
targets, and passed focused CTest 3/3; source scan and `git diff --check` were
clean. Independent fresh configure/build also validated 878 owners and compiled
all targets. Focused CTest passed 2/3: adapter and Initial Setup passed; three
MyWorkspace PageManager cases failed because the `documents` and `campuses`
resource packs were unavailable. Running all 18 MyWorkspace functions
individually confirmed the F24 image preview, missing/corrupt/unavailable
image, display-name, and aggregate-save cases passed; only those same three
resource-dependent cases failed. This is a fresh-tree resource limitation, not
an F24 repair. Phase 2 remains open. The next planned slice was F25
custom-color adapter constructor cleanup; F25 completes it below.
### Progress update - 2026-09-24 (F25 custom-color adapter constructor cleanup)

The custom-color adapter retains its `ApplicationServices&` constructor, adds a
nullable `ApplicationServices*` constructor, and removes `SettingsService*`.
Seven callers in five UI files now pass their `ApplicationServices*`: Schedule
Editor (two), Testing Classes (two), Schedule Import Review, Class Details, and
Initial Setup. The `custom_colors` key, all 16 palette slots, payload formats,
defaults, unrelated settings, and getColor load-before/save-after/cancel
behavior remain unchanged. No ColorUtils logic changed.

Executor fresh Ninja/MSVC x64 configure validated 878 handwritten owners; the
382-step build covered `ClassMngr`, adapter and ColorUtils tests, Initial Setup,
Testing Classes, Schedule Import Review, and ScheduleWidget targets. Focused
CTest passed 6/6. Independent fresh configure also validated 878 owners; the
repeat Ninja build returned success with no work, and the same focused suites
passed 6/6. Source scan found exactly seven callers and no `SettingsService*`
constructor or use; `git diff --check HEAD` passed. Schedule Editor and Class
Details have no picker-specific tests, though their translation units compiled
through `ClassMngr`. Phase 2 remains open. The next candidate was F26 Sub Prep
typed settings-gate removal; it is completed below.
### Progress update - 2026-09-24 (F26 Sub Prep typed settings-gate removal)

Sub Prep no longer exposes the raw `openSettingsService` helper. Saved-content
and Zoom preference paths use their existing typed ports with
`ApplicationServices`; the nullable current-campus port checks availability
before campus loading or mutation. When settings are unavailable, save returns
before stopping autosave or restoring grading. The four existing preference
paths and keys, atomic save behavior, grading default, Zoom primary/legacy
fallback and best-effort migration, and campus matching/fallback are preserved.
The full campus-detail lookup and all-years calendar read remain untouched.

Page tests cover unavailable loading with sentinel fields and unavailable save
with page values, dirty state, timer, blank grading, and stored settings
preserved. The test stub defaults to database-open; each unavailable fixture
sets it false before page construction and does not close its fake service.
Executor and independent fresh Ninja/MSVC x64 configure runs each validated
878 handwritten owners and built `ClassMngr`, the page, and three adapter
targets. Focused CTest passed 4/4 in both runs; the independent repeat build
returned exit 0 with no work. `git diff --check` was clean. No verification gaps
remain. Phase 2 remains open. The next candidate was F27 My Information campus
chooser directory query; F27 completes it below.
### Progress update - 2026-09-24 (F27 My Information campus-directory query)

My Information now reads campus chooser metadata through the Qt-free
`MyInfoCampusDirectoryQueryPort`, implemented by a Platform adapter over
`CampusJsonRepository`. The query returns owning UTF-8 IDs and display names;
the adapter preserves repository order, trimmed name/ID fallback, and the raw
ID. The page no longer reads `CampusJsonRepository` or `ResourcePaths`
directly. Stored ID/name matching and correction writes remain unchanged.

Two new Application/Platform query suites and the existing MyWorkspace behavior
test cover the cutover. Executor and independent fresh Ninja/MSVC x64
configures validated 882 handwritten owners, built `ClassMngr`, MyWorkspace,
and both new suites, and passed focused CTest 3/3. The independent repeat build
returned exit 0 with no work; diff check passed. `CampusJsonCodec` normalizes a
blank ID/name to `campus`, so the empty-display filter cannot be exercised from
a repository fixture; the filter remains in place. Phase 2 remains open. Next
### Progress update - 2026-09-24 (F28 Sub Prep campus-detail directory query)

Sub Prep campus details now cross the Qt-free
`SubPrepCampusDirectoryQueryPort`; its Platform adapter wraps
`CampusJsonRepository` and accepts an injected fixture directory. The page no
longer references `CampusJsonRepository`, `ResourcePaths::Campuses`, or
`CampusInfo`. It preserves repository order and omission, trimmed
case-insensitive saved ID/name matching, first-campus fallback, availability
checks before lookup or state mutation, raw selected IDs, and `N/A` for empty
detail fields.

New Application and Platform suites cover ordering, UTF-8 fields, fallback,
malformed/default records, and missing or empty directories; the page suite
checks selected details and `N/A`. Executor and independent fresh Ninja/MSVC
x64 configures validated 886 handwritten owners and built `ClassMngr`, the
SubPrepPage suite, and both new suites. Executor CTest passed 3/3; independent
CTest passed 6/6, including three existing preference suites. Resource
generation passed, and the independent repeat build had no work. Phase 2
remains open. F29 completes the Personal Details atomic-save caller cutover;
workbook decoding, generic settings, other feature services, broader document
work, and the formal Phase 2 exit gate remain open.
### Progress update - 2026-09-24 (F29 Personal Details atomic-save caller cutover)

`ApplicationServicesPersonalDetailsSavePort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes the `SettingsService*` constructor. Initial Setup and
My Information now pass their existing `ApplicationServices*`. The adapter
still checks availability before saving, prepares UTF-8 values and signature
image data, normalizes the request, and performs one atomic `saveAll` for all
nine keys. Initial Setup's failure warning, My Information's early return
before autosave cancellation or field normalization, and rollback behavior
remain intact.

Adapter tests cover the nullable constructor; a MyWorkspace regression verifies
that an unavailable save preserves whitespace Zoom fields and dirty state.
Executor and independent fresh Ninja/MSVC x64 configs each validated 886
handwritten owners, built `ClassMngr`, the adapter, InitialSetupWizard, and
MyWorkspace targets, and passed focused CTest 3/3. Diff check and caller/
constructor scan passed; no resource limitation occurred. Phase 2 remains
open. The next slice is F30: cut the personal-signature-preferences callers in
My Information and Initial Setup over to nullable `ApplicationServices*`,
preserving defaulting, value normalization, UTF-8 text, availability, and
read-only/no-write behavior. F30 completes below.
### Progress update - 2026-09-24 (F30 personal-signature-preferences caller cutover)

`ApplicationServicesPersonalSignaturePreferencesPort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes `SettingsService*`. My Information and Initial Setup
now pass their existing `ApplicationServices` owners. Availability guards,
exact read-only keys and defaults, UTF-8 typed text, mode/font normalization,
unavailable failure behavior, and the no-write contract remain unchanged.

Executor and independent fresh Ninja/MSVC x64 configs each validated 886
handwritten owners, built `ClassMngr` and the adapter, InitialSetupWizard, and
MyWorkspace targets, and passed focused CTest 3/3. Diff and source scans passed;
there was no resource limitation. Phase 2 remains open. The next slice is F31:
cut the current-campus-preferences callers over to nullable
`ApplicationServices*` while preserving caller guards and read/correction
semantics. Workbook decoding, generic settings, other feature services,
broader document work, and the formal Phase 2 exit gate remain open.
### Progress update - 2026-09-24 (F31 current-campus-preferences caller cutover)

`ApplicationServicesCurrentCampusPreferencesPort` removes its raw
`SettingsService*` constructor while retaining its `ApplicationServices&` and
nullable `ApplicationServices*` constructors. My Information's campus read
and correction writes, plus Initial Setup's campus read, pass their existing
`ApplicationServices` owners. The `myInfo/campus` key, UTF-8/QVariant string
conversion, unavailable empty-read/no-op-write behavior, mapped save failure,
My Information availability guard, and saved-campus correction timing remain
preserved.

Executor and independent fresh Ninja/MSVC x64 configures each validated 886
handwritten owners, built `ClassMngr`, the adapter, InitialSetupWizard, and
MyWorkspace, and passed focused CTest 3/3. Adapter/test/source scans and diff
check passed; there was no resource limitation. Phase 2 remains open. F32 is
the Sub Prep personal-Zoom preference caller cutover: remove the raw
`SettingsService*` constructor from
`ApplicationServicesSubPrepPersonalZoomPreferencesPort` and update My
Information and Initial Setup to use `ApplicationServices*`, preserving
primary-over-legacy precedence, best-effort migration only when primary
values are absent, legacy-value return on migration failure, UTF-8/defaults,
unavailable handling, and UI behavior. Workbook decoding, generic settings,
other feature services, broader document work, and the formal Phase 2 exit
gate remain open.
### Progress update - 2026-09-24 (F32 Sub Prep personal-Zoom preference caller cutover)

`ApplicationServicesSubPrepPersonalZoomPreferencesPort` retains its
`ApplicationServices&` constructor, adds a nullable `ApplicationServices*`
constructor, and removes the `SettingsService*` constructor. My Information and
Initial Setup now pass their existing services. Primary `myInfo/zoom*` values
take precedence; legacy `subPrep/personalZoom*` values are fallback inputs and
are best-effort migrated only when the primary key is absent. Migration failure
still returns the legacy values. UTF-8 conversion, defaults, unavailable
behavior, and page display remain unchanged. Null-constructor coverage was
updated.

The executor built `ClassMngr`, the adapter, MyWorkspace, and InitialSetupWizard;
focused CTest passed 1/1. An independent fresh Ninja/MSVC x64 configure
validated 886 handwritten owners, built all four targets, and passed focused
CTest 3/3. Adapter/source audits and diff check passed; there was no resource
limitation. Phase 2 remains open. F33 completes the typed settings-availability
migration for My Information and Initial Setup. Workbook decoding, generic
settings, other feature services, broader document work, and the formal Phase 2
exit gate remain open.
### Progress update - 2026-09-24 (F33 My Information and Initial Setup typed availability boundary)

My Information and Initial Setup now use
`ApplicationServicesCurrentCampusPreferencesPort::isAvailable()` instead of
direct settings-availability access. My Information's raw availability helper
and Initial Setup's raw getter were removed. Guards preserve My Information's
load no-mutation and save-before-autosave-cancel/Zoom-normalization behavior,
and Initial Setup's early returns. Regressions verify that unavailable My
Information loading preserves sentinel fields and unavailable Initial Setup
validation preserves the entered name and signature preview, stays on the page,
and shows no warning; the unavailable-initialization test remains.

Executor and independent fresh Ninja/MSVC x64 configure/builds validated 886
handwritten owners and built `ClassMngr`, the CurrentCampus adapter,
MyWorkspace, and InitialSetupWizard. After a test-only coverage repair, the
independent rerun passed the exact focused CTest suites
`ClassMngrInitialSetupWizardTests`, `ClassMngrMyWorkspacePageTests`, and
`ClassMngrNextPlatformApplicationServicesCurrentCampusPreferencesPortTests`
(3/3). Source scans and diff check passed. Phase 2 remains in progress.

F34 Class Notes save is recorded above. F35's Sub Prep calendar interval query
is recorded below. Next, audit current code against this plan and the formal
Phase 2 exit gate. Phase 2 remains in progress and the exit gate remains open.
### Progress update - 2026-09-24 (F34 Class Notes save cutover)

Only the Class Notes page save path now uses the Qt-free
`Application::ClassNotesSavePort`; its `std::u16string` fields preserve the
existing exact 10,000 UTF-16-code-unit semantics. The Platform
`ApplicationServicesClassNotesSavePort` maps to the existing
`ClassService::saveClassNotes`. The page's other reads remain unchanged.

The cutover preserves trimming, the single two-field upsert, warning and
autosave behavior, dirty state on failure, and clean state on success. New
contract, adapter, and feature-page suites plus the existing Classes page and
DataServiceLifecycle suites passed 5/5 on a fresh x64 Ninja/MSVC configure;
all targets built with 891 handwritten owners. The added
`defaultPortSavesBothFieldsToPersistence()` case verifies persistence of both
fields and clean page state through the real default adapter. The Qt-free
contract/source audit and diff check passed.
### Progress update - 2026-09-24 (F35 Sub Prep calendar interval query)

The typed `SubPrepCalendarEventIntervalsQuery` and Platform
`ApplicationServicesSubPrepCalendarEventIntervalsPort` replace Sub Prep's
direct generic calendar range read. One captured date sets the inclusive range
from January 1 of its calendar year through December 31 of the following year
(the current and following calendar years at most), clamped at year 9999, and
is also passed to the dialog. The repository selects
only normalized Vacation/Holiday events overlapping the full range; invalid
or reversed intervals are ignored. This purpose-specific read does not apply
the generic 4,096-event projection cap.

Unavailable services, query failures, and conversion exceptions still produce
empty intervals while generation continues. Independent fresh Ninja/MSVC x64
verification validated 894 source owners, built the executable and focused
targets, and passed the eight focused CTests 8/8. Coverage includes 4,097
events, the production ApplicationServices-to-repository path, the two-year
boundary and year-9999 clamp, interval overlaps, day-28/day-29 cases, and
historical/future holiday bridges. `git diff --check` passed. The exceptional
conversion fallback was source-inspected but not fault-injected.

The read-only exit-gate audit is recorded above. Phase 2 remains in progress
and its formal exit gate remains open; next, select a slice from the remaining
gate gaps.
### Progress update - 2026-09-24 (F36 Calendar import parity)

The production `CalendarEventImportService` now has deterministic offline
coverage using a required checked-in XLSX fixture, loopback transport, and a
temporary database. The test exercises workbook parsing, typed existing-event
signature lookup, planner execution, and ordered batch save; it asserts exactly
three inserted events, two skipped rows, and the persisted event set.

Independent fresh Windows x64 configure/build and all five focused suites
passed. Parser-level signature deduplication and planner duplicate-candidate
handling are covered by their separate planner suite, not end-to-end in this
service test. This closes only the exercised Calendar import-planning parity
path; Domain completeness and broader validation, conflict, import, and state
parity remain open. Phase 2 remains in progress and its formal exit gate is
open.
### Progress update - 2026-09-24 (F37 Schedule import state validation contract)

`ClassMngr::Next::Application` now owns the Qt-free
`validateScheduleImportState` contract. The legacy repository adapts its
structurally checked plan and existing teacher/class snapshots to the request
after snapshot reads and before applying writes in the current transaction;
the duplicate legacy state validator was removed. The request covers
Normal/Intensive projections, teacher reuse/update-room/create/skip
resolution, class update/create/skip resolution, selected-target availability
and identity, the unique exact-match rule for skipped classes, valid projected
day/time ranges, and overlaps. It preserves skipped classes in the projected
normal schedule and absent intensive classes only in intensive update mode.

The repository test installs a SQLite `BEFORE UPDATE` trigger that aborts if a
teacher write is reached before validation. A stale selected class is rejected
by the new contract, and persisted teacher, class, schedule-time, and settings
sentinels remain unchanged. This proves pre-write rejection for that path; it
does not prove every validation rule through the database integration.

The Application contract is Qt-free; this does not establish that the whole
`src/next` tree is Qt-free. Executor and independent Tester each completed a
fresh Windows x64 configure with 895 handwritten source owners, and both
focused suites passed 2/2. Schedule matching/preview, required workbook parity,
the workbook decoder, and broader Domain completeness remain open. Gate 2 is
partial; Phase 2 remains in progress and its formal exit gate remains open.
### Progress update - 2026-09-24 (F38 Schedule Import matching and preview)

Schedule Import now uses the Qt-free
[`ScheduleImportMatchingProjection`](../../src/next/application/schedule_import_matching_projection.h)
contract, and [`ScheduleImportRepository::preview`](../../src/data/repositories/schedule_import_repository.cpp)
is wired through it. The legacy `ScheduleImportMatcher` was removed. At the Qt
edge, grade, level, and room matching keys are simplified and case-folded; raw
room text remains available for display. The app-less
[`matching-projection suite`](../../tests/next_application_schedule_import_matching_projection_tests.cpp)
covers all seven ranking buckets, stable ties, no-match and inventory results,
and Normal/Intensive fallback.

The required checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
drives production preview with no skip or external-workbook path. The database
fixture in [`schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp)
seeds room as `' 416 '` against workbook room `416` and asserts exact/weaker
candidate IDs `[43,42]`, suggestion `43`, exact/confident status, two regular
and no intensive inventory candidates, and initially absent IDs. Executor and
independent fresh x64 Ninja/MSVC configure/build runs each validated 895
source owners, built both focused targets, and passed CTest 2/2. QtTest
reported Schedule 24 passed, 0 failed, 1 skipped (only the optional external
sample was unset), and matching 5 passed, 0 failed, 0 skipped.

F38 improved Gate 2 but left review-time conflict projection open; F39 now
provides that shared Application projection and fixture-backed review/apply
coverage. Wider baseline parity, shared workbook decoding, and broader Domain
completeness remain open. Phase 2 remains in progress and the formal exit gate
remains open. F38 is committed as
`bc6ac011504e0a499a8cdfd4b1533b49ea3f4bcb`.
### Progress update - 2026-09-24 (F39 Schedule Import conflict projection)

The Qt-free standard-C++
[`schedule_import_overlap_projection.h`](../../src/next/application/schedule_import_overlap_projection.h)
is now shared by Schedule Import review presentation and apply-time state
validation. It owns half-open overlap versus adjacency, matching days,
deterministic conflict order, Normal/Intensive projection, skipped classes,
and retained intensive schedules. UI translation remains at the edge.

The required checked-in
[`schedule_overlap_conflict.xlsx`](../../tests/fixtures/imports/schedule_overlap_conflict.xlsx)
drives production preview and review: it shows the expected conflict warning
and disables import in [`schedule_import_dialog_tests.cpp`](../../tests/schedule_import_dialog_tests.cpp).
Apply rejects the conflicting state, and database assertions in
[`schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp) verify
that no teachers, classes, or `class_times` rows persist; the F37 pre-write
trigger sentinel also passes. App-less rule coverage is in
[`next_application_schedule_import_state_validation_tests.cpp`](../../tests/next_application_schedule_import_state_validation_tests.cpp).
Independent Tester
`PH2-F39-INDEPENDENT-VERIFY` configured a fresh Windows x64 Ninja/MSVC build
with 896 handwritten source owners, built three focused targets, and passed
CTest 3/3. QtTest passed 25 ScheduleImport, 21 ScheduleImportDialog, and 12
Application state-validation tests (58 passed, 0 failed), with one existing
optional external-workbook skip because `CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was
unset. `git diff --check` passed.

F39 is committed as `3121d90c2db6af8e225048f016eec6f0843c1c18`. It improves
Gate 2 but does not close Phase 2: wider baseline parity and the remaining
work listed in the gate audit remain open.
### Progress update - 2026-09-24 (F40 Domain schedule-time value)

Standard-C++ [`Domain::Weekday` and `Domain::ScheduleTime`](../../src/next/domain/schedule_time.h)
now provide a validated schedule value with weekday and minute bounds,
half-open overlap behavior, and value comparison. Raw Application inputs are
retained for error reporting. After validation,
`ScheduleImportProjectedTime` carries the Domain value plus Application-owned
labels through the overlap projection; apply validation and F39 UI review both
consume it. Invalid raw values preserve `InvalidProjectedTime` labels.

The independent `PH2-F40-SCHEDULE-TIME-VERIFY` recheck configured a fresh
Windows x64 Ninja/MSVC Debug build with 897 handwritten sources, each with one
explicit owner, and passed four focused CTest suites (4/4). QtTest passed
[`Domain`](../../tests/next_domain_contract_tests.cpp) 8,
[`state validation`](../../tests/next_application_schedule_import_state_validation_tests.cpp)
13, [`Schedule Import repository`](../../tests/schedule_import_tests.cpp) 25,
and [`review dialog`](../../tests/schedule_import_dialog_tests.cpp) 21 cases
(67 passed, 0 failed), with one existing optional external-workbook skip because
`CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset. The F39 fixture, conflict warning,
disabled review action, zero-write rejection, and F37 pre-write trigger
sentinel passed; `git diff --check` passed.

F40 is committed as `2ab23fb1796dfb1761a4c48644869a9ae6e1060d`. It adds Domain
coverage but no baseline-parity scope; Phase 2 remains in progress and the
formal exit gate remains open.
### Progress update - 2026-09-24 (F41 Schedule Import review decisions)

The Qt-free
[`schedule_import_review_decisions.h`](../../src/next/application/schedule_import_review_decisions.h)
is the shared authority for teacher/class choice validation in dialog
readiness and the PlanValidator adapter. It checks complete, unique
resolutions; valid actions and targets; duplicate targets; required or foreign
rooms; and skipped teacher/class consistency. Workbook content, colors, and
meeting validation remain at the feature edge; SQL and stale/current-state
checks remain in the repository and F37 validator.

Required [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
now exercises production parse, preview, explicit review choices, and
repository apply. Assertions cover the result summary, persisted teacher,
class, color, and time state, and retained unrelated class metadata. The F39
conflict fixture still verifies the review warning, disabled Import action,
and zero-write apply rejection; the F37 pre-write trigger sentinel passes.

Independent verification `PH2-F41-SCHEDULE-IMPORT-VERIFY` configured fresh
Windows x64 Debug/Ninja/MSVC with 899 handwritten source owners; four focused
CTest suites passed 4/4. QtTest passed the
[`decision contract`](../../tests/next_application_schedule_import_review_decisions_tests.cpp)
26/0/0, [`Schedule Import repository`](../../tests/schedule_import_tests.cpp)
25/0/1, [`review dialog`](../../tests/schedule_import_dialog_tests.cpp) 21/0/0,
and [`F37 state validation`](../../tests/next_application_schedule_import_state_validation_tests.cpp)
13/0/0 (85 passed, 0 failed, one existing optional skip because
`CLASSMNGR_SCHEDULE_IMPORT_SAMPLE` was unset). `git diff --check` passed.

F41 is committed as `30ec8d7512a8847a5b1d32addabf25f252b0eabb`. This expands
fixture-backed Schedule review/apply parity but does not complete Gate 2 or
Phase 2; wider baseline parity and the remaining gate-audit items remain open.
### Progress update - 2026-09-24 (F42 workspace replacement failure handling)

[`FileController` workspace integration tests](../../tests/file_controller_workspace_lifecycle_tests.cpp)
verify that a failed production profile replacement-open preserves the active
database, settings sentinel, recent/last-file entries, and UI action
availability. If the coordinator reports close failure, interactive create
stops before preparing or replacing the selected target. No new `src/next`
production file was added.

Independent fresh x64 Ninja/MSVC verification validated 899 handwritten
owners. Four focused CTest targets passed 17/17, 33/33, 25/25, and 11/11; all
passed. `git diff --check` was clean. The formal workspace criterion is
satisfied by the verified `WorkspaceGateway::createWorkspace` and
`WorkspaceCoordinator` dirty-rejection, successful-state-transition, and
failure-atomicity cases. This does not close Phase 2.

Non-gating FileController gaps remain: dirty-page approval still comes from
`MainWindow`; normal new-profile and initial-setup flows close the current
session before target preparation and coordinator create succeed; and
same-path replacement plus the complete live MainWindow snapshot lack direct
coverage. F42 is committed as
`8b2eb8a2a7a0dae6a22ce8a4163b35d5d9dd24ee`. Phase 2 remains In Progress and
the formal exit gate remains open.
### Progress update - 2026-09-24 (F43 Class Transfer review decisions)

The Qt-free [`class_transfer_projection.h`](../../src/next/application/class_transfer_projection.h)
provides shared Class Transfer review-decision validation to dialog readiness
and repository apply validation. Required checked-in
[`success_source.json`](../../tests/fixtures/transfers/success_source.json)
drives fixture-backed apply and verifies persisted class, teacher, schedule
(including end time), and roster state. The checked-in
[`conflict_source.json`](../../tests/fixtures/transfers/conflict_source.json)
verifies a conflicting transfer performs no partial writes. The application
contract remains free of Qt and legacy Application dependencies.

Independent fresh Windows x64 Ninja/MSVC verification validated 899 handwritten
source owners; both focused CTest targets passed 2/2, and the app-less and
repository QtTest suites passed 19 and 15 cases respectively. `git diff --check`
was clean. See the [app-less contract tests](../../tests/next_application_class_transfer_tests.cpp)
and [repository integration tests](../../tests/class_transfer_tests.cpp).
F43 is committed as `c0e03e55aa5f5cc1897ccf97a25901a5e119c8e5`. It advances
Domain/Application behavior and fixture-backed parity but leaves both gates
partial; the formal WorkspaceCoordinator create criterion and audited
dependency-isolation criterion remain satisfied. Phase 2 remains In Progress
with its exit gate open. Sub Prep remains limited to the current and following
calendar years at most.
## Verified F44 Teacher Import review decisions - commit `28170a914a4dc76dc62f66677ad8f1067dfd42bf`

Qt-free [`import_review_session.h`](../../src/next/application/import_review_session.h)
provides Teacher Import review-decision validation shared by production dialog
readiness/plan creation and repository apply. The repository validates that
reviewed candidate identities match the plan before opening its transaction;
the contract has no Qt or legacy Application includes.

Required checked-in
[`sectioned_review.xlsx`](../../tests/fixtures/teacher_import/sectioned_review.xlsx)
drives the production dialog to produce the plan passed directly to apply in
[`teacher_import_dialog_tests.cpp`](../../tests/teacher_import_dialog_tests.cpp).
It verifies selected and omitted candidates and persisted Korean, Native English,
and GS Team records (3/0/0 pass/fail/skip). The repository suite also verifies
deterministic summary values, manually maintained-field preservation, and that
invalid reviewed plans neither write records nor advance the source date.
Direct invalid-decision coverage in
[`teacher_import_tests.cpp`](../../tests/teacher_import_tests.cpp) passed 3/0/0,
including an empty group ID.

Independent fresh out-of-tree Windows x64 MSVC/Ninja Debug verification with
Qt 6.12.0 built both focused Teacher Import targets and passed CTest 2/2. The
required fixture case did not skip; optional external-sample tests skipped
because `CLASSMNGR_TEACHER_IMPORT_SAMPLE` was unset. `git diff --check` passed.
Gate 1 and Gate 2 advance but remain Partial; Workspace boundary and dependency
isolation remain Satisfied. Wider baseline parity, shared workbook decoding,
broader Domain records, generic settings, remaining feature migrations, and
broader calendar/UI and document work remain open. Phase 2 remains In Progress
with its exit gate Open. Sub Prep remains capped at the current and following
calendar years at most.
## Verified F45 Domain Course catalog - commit `eb2167e9d39a65446265b9506d749dc0e6be0d35`

Qt-free [`Domain::Course`](../../src/next/domain/course.h) owns the exact
ordered 25 supported grade/level pairs. [`ClassInfoConfig`](../../src/features/classes/config/class_info_config.cpp)
adapts that catalog to the existing Qt lists, and
[`ScheduleImportPlanValidator`](../../src/features/schedule/services/schedule_import_plan_validator.cpp)
validates imported pairs through `Domain::Course`. This is a course-value and
validation boundary; broader Domain records and other class/catalog consumers
remain open.

The required checked-in
[`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
still applies its valid pairs successfully (3/0/0 pass/fail/skip). The three
direct Course cases each reported 3/0/0; valid-fixture apply and repaired
invalid-course apply also each passed 3/0/0. The invalid case in
[`schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp) seeds
`teacher`, `class`, `class_info`, `class_times`, and profile-setting snapshots and
confirms all remain unchanged after rejection. After the independent review
found empty-table counts insufficient to prove preservation, the test was
strengthened and the same tester reran the target build and rejection case.

Independent fresh Windows x64 MSVC 19.51/Ninja Debug verification with Qt 6.12.0
built `ClassMngrNextDomainContractTests` and `ClassMngrScheduleImportTests`;
focused CTest passed 2/2. Source ownership validated 900 handwritten sources,
with `course.h` assigned once under `CLASSMNGR_NEXT_DOMAIN_SOURCES`.
`git diff --check` passed; the full suite was not run. Gate 1 and Gate 2 advance
but remain Partial; workspace boundary and audited v2 dependency isolation
remain Satisfied. Wider baseline parity, shared workbook decoding, broader
Domain records, generic settings, remaining feature migrations, and broader
calendar/UI and document work remain open. Non-gating FileController caveats
remain recorded above. Phase 2 remains In Progress with its exit gate Open.
Sub Prep remains capped at the current and following calendar years at most.
## Verified F46 Korean teacher identity key - commit `bedb52e0045731bba3e4f4b7a576021bdc007b72`

Qt-free [`Domain::KoreanTeacherKey`](../../src/next/domain/korean_teacher_key.h)
owns the shared UTF-16 Hangul code-unit ranges U+1100–U+11FF, U+3130–U+318F,
U+A960–U+A97F, U+AC00–U+D7AF, and U+D7B0–U+D7FF. It filters other code units while
preserving order, does not trim or normalize, and permits an empty key.
[`TeacherImportNameUtils`](../../src/features/teacher/import/teacher_import_name_utils.h)
converts QString values at the edge; Schedule matching uses the same Domain key
through [`ScheduleImportMatchingProjection`](../../src/next/application/schedule_import_matching_projection.h),
removing its duplicate range implementation. Contract tests cover range
boundaries, excluded code units, empty/equality/accessor semantics, and
composed, decomposed, and compatibility forms. Existing empty-key behavior
remains: the contact parser keeps its validation, the repository keeps its
required-name error and leaves the row count unchanged, and Schedule matching
keeps its empty-key match.

Required fixture coverage remains green for Teacher Import parsing, review,
persistence, and the production dialog-plan-to-repository path using
[`sectioned_review.xlsx`](../../tests/fixtures/teacher_import/sectioned_review.xlsx);
Schedule matching/persistence using
[`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx);
and overlap rejection-before-write using
[`schedule_overlap_conflict.xlsx`](../../tests/fixtures/imports/schedule_overlap_conflict.xlsx).
Evidence suites are [`Domain contracts`](../../tests/next_domain_contract_tests.cpp),
[`Schedule matching projection`](../../tests/next_application_schedule_import_matching_projection_tests.cpp),
[`Teacher Import`](../../tests/teacher_import_tests.cpp),
[`Teacher Import dialog`](../../tests/teacher_import_dialog_tests.cpp),
[`Schedule Import`](../../tests/schedule_import_tests.cpp), and
[`Schedule Import dialog`](../../tests/schedule_import_dialog_tests.cpp).

Independent fresh Windows x64 MSVC 19.51/Ninja 1.13/CMake 4.4.2/Qt 6.12.0
verification passed four focused CTests 4/4 and Teacher Import dialog CTest
1/1. QtTest counts: Domain 14/0/0, matching projection 6/0/0, Teacher Import
16/0/1, Schedule Import 26/0/1, and dialog 5/0/1; skips were optional
external-sample cases. Source ownership validated 901 handwritten files.
`git diff --check` passed; the full suite was not run. Gate 1 and Gate 2 remain
Partial; Workspace boundary and audited v2 dependency isolation remain
Satisfied. Wider baseline parity, shared workbook decoding, broader Domain
records, generic settings, remaining feature migrations, and broader
calendar/UI and document work remain open. Non-gating FileController caveats
remain recorded above. Phase 2 remains In Progress with its exit gate Open.
Sub Prep remains capped at the current and following calendar years at most.
## Verified F47 Course weekly meeting-day rule - commit `7cba8abf952b5b32f90391844beec68eac2c3f69`

Qt-free [`Domain::Course::WeeklyMeetingDayRule`](../../src/next/domain/course.h)
owns typed weekly meeting-day policy over `Domain::Weekday` patterns, rejecting
invalid, out-of-range, and duplicate weekdays and matching patterns without
depending on their order. The Schedule Import production parser/partitioning
and plan/apply validation use the Course rule; QString parsing and translated
diagnostics remain at the feature edge. Legacy grade trim/uppercase and
trimmed, case-insensitive Athena/Song's handling are preserved, as are the
allowed/forbidden categories and E5/Zeus Tuesday-only Skip behavior. Unsupported
`M3 Zeus` still has no meeting-pattern error and is rejected by separate Course
validation.

The required [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
persists accepted rows. A fixture-derived one-day E4/Theseus pattern is rejected
before writes, preserving seeded teachers, classes, class_info, class_times,
and app_settings snapshots. Existing invalid-course, overlap no-write, and
Skip-with-prohibited-pattern coverage remains green.

Executor and independent fresh x64 MSVC 19.51/Ninja 1.13/CMake 4.4/Qt 6.12
builds each passed focused CTest 2/2 for `ClassMngrNextDomainContractTests` and
`ClassMngrScheduleImportTests`; direct fixture persistence, rejection, overlap,
and Domain policy cases passed. `git diff --check` passed; the full suite was
not run. Gate 1 and Gate 2 advance but remain Partial. Workspace boundary and
audited `src/next` dependency isolation remain Satisfied. Phase 2 remains In
Progress with its exit gate Open. Sub Prep remains capped at the current and
following calendar years at most.

### Progress update - 2026-09-25 (F49 Calendar Import use case)

Qt-free [`CalendarEventImportUseCase`](../../src/next/application/calendar_event_import_use_case.h)
composes the existing signature-query port, `planCalendarEventImport`, and
batch-save port. The production `CalendarEventImportService` delegates query,
deduplication, ordered signature/request pairing, saving, and imported/skipped
counts to the use case. Workbook/network/campus handling, signals, localized
errors remain at the feature edge; an injected observer preserves profiler
timing at the prior boundaries.

Six app-less fake-port cases cover ordered mapping, existing and in-batch
duplicates, parser skip counts, empty and duplicate-only inputs, exact UTF-16
signature identity (including a lone surrogate), and query/save failures. The
required [`calendar_import_parity_2026.xlsx`](../../tests/fixtures/imports/calendar_import_parity_2026.xlsx)
production path verifies persisted rows and counts. Executor and independent
Tester fresh builds each passed the focused CTest 2/2; no full suite was run.
F49 is committed as `6a41e958671b7fa93c301d8b25c9c4381178fd7f`. Gate 1 and Gate
2 advance but remain Partial; workspace create and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate
Open. Sub Prep remains limited to the current and following calendar years at
most.

### Progress update - 2026-09-25 (F50 Calendar Import signature identity)

Qt-free [`Application::CalendarEventImportSignature`](../../src/next/application/calendar_event_import_signature.h)
is the shared six-field key value used by
[`academic_calendar_event_parser.cpp`](../../src/features/calendar/academic_calendar_event_parser.cpp)
and the signature-query port. The Qt adapters retain title simplification,
type/time-status normalization, and ISO date conversion; the value preserves
field order, delimiters, exact UTF-16 code units, and the all-day `1/0` flag,
and excludes times, database ID, and repeat-series ID. App-less coverage checks
the format, every field, UTF-16 code units, `%2` title text, and type members
without metadata. Existing CalendarImportTests covers normalization and
excluded metadata; a Qt 6.12 probe confirmed inserted `%2` text is not rescanned
by the legacy six-argument `QString::arg` call.

Executor and independent Tester each freshly configured Windows x64
MSVC/Ninja, validated 906 source owners, built three focused targets, and
passed CTest 3/3, including required `calendar_import_parity_2026.xlsx`
production parity. No full suite was run. F50 is committed as
`92d001db11d8c8eb973de5f238444abe855ea5c5`. Gate 1 and Gate 2 remain Partial;
Workspace boundary and audited `src/next` dependency isolation remain
Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep stays
capped at the current and following calendar years at most.

### Progress update - 2026-09-25 (F51 typed Calendar Import signature flow)

F51 carries the Qt-free Application::CalendarEventImportSignature value
through signature-query results, plan inputs, candidate deduplication, and the
Calendar Import use case; the parser and Platform query adapter produce the
same typed value. This removes raw UTF-16 string conversions between those
Application boundaries; Qt normalization and date conversion remain at the
adapters. A fresh Windows x64 MSVC/Ninja configure validated 906 source owners.
Executor and independent Tester each verified seven focused CTest targets,
each passing 1/1:
ClassMngrNextApplicationCalendarEventImportSignatureTests,
ClassMngrNextApplicationCalendarEventImportSignatureQueryPortTests,
ClassMngrNextApplicationCalendarEventImportPlanTests,
ClassMngrNextApplicationCalendarEventImportUseCaseTests,
ClassMngrCalendarImportTests,
ClassMngrNextPlatformApplicationServicesCalendarEventPortTests, and
ClassMngrCalendarEventImportParityTests. The production path used the required
calendar_import_parity_2026.xlsx fixture. With QCOMPARE diagnostics restored,
ClassMngrCalendarImportTests was rebuilt and rerun, passing 1/1. No full suite
was run. F51 is committed as e940f0c0ed8a63e740a3c2375631a22c08875f84. Gate 1
improves but remains Partial because broader Domain and Application behavior
is outstanding; baseline parity Gate 2 remains Partial because parity coverage
is incomplete. Workspace create and audited v2 dependency isolation remain
Satisfied. Phase 2 remains In Progress with its exit gate Open.

### Progress update - 2026-09-25 (F52 centralized Calendar event timing)

F52 adds Qt-free [`Domain::CalendarEventTiming`](../../src/next/domain/calendar_event_timing.h)
and uses it in `CalendarEventEditDraft`, `CalendarEventSavePort`, and
`CalendarEventSeriesEditPort`. Canonical dates require hyphens at positions 4
and 7 and digits elsewhere; regression coverage rejects `2026006010` and
`2026-06110`. The feature boundaries retain their specific errors and
validation order, including the existing cross-day clock rule.

Fresh MSVC 19.51/Ninja/Qt 6.12 configure validated 907 source owners; three
focused targets built, exact CTest passed 3/3, and the checked-in Calendar
Import parity fixture passed. No full suite was run. F52 is committed as
`9cd9a2a4469482bc803cdc172d18a072c0fb3949`. Gate 1 and Gate 2 remain Partial;
Workspace boundary and audited `src/next` dependency isolation remain
Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep stays
capped at the current and following calendar years at most.

### Progress update - 2026-09-25 (F53 Roster Score Import parity)

[`roster_editor_widget_import_tests.cpp`](../../tests/roster_editor_widget_import_tests.cpp)
exercises the real `RosterEditorWidget::importScores` slot through
`ClassMngrRuntime`. It seeds saved evaluations through production services in
a temporary database; this workflow reads saved evaluations and does not parse
a workbook. Coverage verifies all four grade columns, English/Korean name-pair
matching including collisions, preservation of unmatched, empty, and
English-only partial rows, autosave persistence via a fresh roster-service
read, idempotent re-import, and missing-English/Korean-column warnings with no
persisted changes. A mixed Winter score totaling 16/6 is asserted as B+.

Fresh independent Ninja/MSVC 19.51/Qt 6.12 configure validated 908 source
owners; `ClassMngrRosterEditorWidgetImportTests`,
`ClassMngrRosterModelTests`, and `ClassMngrSpeakingEvaluationServiceTests`
built and exact CTest passed 3/3. No full suite was run. F53 expands Gate 2
parity evidence only: Gate 1 and Gate 2 remain Partial; Workspace boundary
and audited `src/next` dependency isolation remain Satisfied. Phase 2 remains
In Progress with its exit gate Open. Sub Prep remains capped at the current
and following calendar years at most.

### Progress update - 2026-09-25 (F54 Calendar event vocabulary)

F54 adds Qt-free Domain classifiers for the six Calendar event type names and
three time statuses in `calendar_event_timing.h`. The calendar edit-draft,
single-save, and repeat-series-edit validators reuse these classifiers while
preserving request strings and raw fields, trimming at each Application
boundary, 64-character limits, validation order, feature-specific errors, and
projection behavior. Domain and Application tests cover known and unknown
values, casing/whitespace, raw-field preservation, length boundaries, and
request-specific errors.

Fresh independent Ninja/MSVC 19.51/Qt 6.12 configure/build ran
`ClassMngrNextDomainContractTests` and
`ClassMngrNextApplicationCalendarEventTests`; exact CTest passed 2/2. No full
suite was run. F54 adds Gate 1 evidence only; Gate 1 and Gate 2 remain Partial,
Workspace boundary and audited `src/next` dependency isolation remain
Satisfied, and the Phase 2 exit gate remains Open. Sub Prep remains capped at
the current and following calendar years at most.

### Progress update - 2026-09-25 (F55 shared Course grade-band classification)

`Domain::Course::gradeBandForName` provides Qt-free classification from the
grade name alone. Classes page visibility, Evaluation Default Selection, and
Schedule testing-score suppression share this classifier while retaining each
caller’s existing `trimmed().toUpper()` normalization and its own policy:
Classes hides Analytics/Evaluations for M1–M3 subject to the preference;
Evaluation selects Middle only for M1–M3 and Elementary otherwise; Schedule
suppresses M2/M3, and suppresses M1 only when `testingAffectsM1` is true.
Classification remains independent of whether a grade/level pair is valid.

Coverage includes all six bands plus Other, invalid, case, whitespace, and an
invalid level-pair classification; class preference behavior; the grade/school-
level helper used by Evaluation Default Selection; and Schedule suppression
without assignments, including the M1 toggle. The Evaluation test calls the
same private policy helper used by `forClass`, but does not exercise the full
`ApplicationServices` integration.

Independent fresh MSVC 19.51/Ninja/Qt 6.12 verification validated 908
handwritten owners, built `ClassMngrNextDomainContractTests`,
`ClassMngrClassesPageTests`, `ClassMngrSchedulePrintModelTests`, and
`ClassMngrEvaluationDefaultSelectionTests`, and passed exact CTest 4/4. No
full suite was run. F55 adds Gate 1 evidence only: Gate 1 and Gate 2 remain
Partial; Workspace boundary and audited `src/next` dependency isolation remain
Satisfied; the Phase 2 exit gate remains Open. Sub Prep remains capped at the
current and following calendar years at most.

### Progress update - 2026-09-26 (F56 Speaking Evaluation grade contract)

Qt-free [`Domain::SpeakingEvaluationGrade`](../../src/next/domain/speaking_evaluation_grade.h)
represents the six evaluation criteria and C/B/B+/A/A+ values, parses exact
labels, and centralizes legacy aggregation, >=0.4 rounding, invalid/missing
outcomes, and clamping. It replaces duplicated calculation in roster import,
the report data assembler, and the report widget. Repository import continues
to trim input; report paths continue to require exact labels.

Domain coverage exhausts all 15,625 valid combinations and checks invalid or
missing values, labels, and rounding. The real roster widget-import test adds a
padded saved label and incomplete evaluation (N/A), retaining F53's mixed
16/6-to-B+ result, persistence, and idempotence coverage. Report-widget tests
check B+ and N/A through assembly and rendering. Independent fresh
MSVC 19.51/Ninja/Qt 6.12 verification validated 909 handwritten source owners,
built `ClassMngrNextDomainContractTests`,
`ClassMngrRosterEditorWidgetImportTests`,
`ClassMngrSpeakingEvaluationServiceTests`, and
`ClassMngrSpeakingEvalReportWidgetTests`, and passed exact CTest 4/4. No full
suite was run. F56 is committed as
`c73e896fe34e186a045d73b653aa8ec9dfa89e83`. Gate 1 and Gate 2 advance but
remain Partial; Workspace boundary and audited `src/next` dependency isolation
remain Satisfied; the Phase 2 exit gate remains Open. The Evaluation Default
Selection test still exercises its policy helper rather than full
`ApplicationServices::forClass` integration. Sub Prep remains capped at the
current and following calendar years at most.

### Progress update - 2026-09-26 (F57 Evaluation Default Selection contract, commit `b38b3afef0088b4c05d6d540dda15600f48c7f59`)

Qt-free `Application::EvaluationPeriod` now selects Winter, Speech Contest,
Summer, Fall, or no evaluation for All, supporting current/previous period
selection and the Winter-to-Fall cycle. The feature adapter preserves exact
legacy labels and schedule/service boundaries. App-less tests cover populated
and empty current/previous cycles, All, and invalid input. Temporary-database
integration invokes production `EvaluationDefaultSelection::forClass`: at
2026-09-07 it selects current Fall for M2 with Summer fallback, and current
Summer for E4 with Speech Contest fallback; it also covers All, missing saved
schedule, and missing ClassInfo. Failed class-info or evaluation reads return
no default; the integration test does not explicitly simulate an evaluation
read failure.

Executor verification built `ClassMngrEvaluationDefaultSelectionTests`,
`ClassMngrEvaluationDefaultSelectionIntegrationTests`,
`ClassMngrNextApplicationEvaluationDefaultSelectionTests`,
`ClassMngrNextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests`,
and `ClassMngrNextPlatformApplicationServicesEvaluationDefaultPolicyPortTests`;
exact CTest passed 5/5. Fresh independent Ninja/MSVC 19.51/Qt 6.12 verification
validated 912 handwritten owners, built the selection, integration, app-less
contract, and Evaluation Default Policy port targets, and passed exact CTest
4/4. The unchanged
`ClassMngrNextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests`
target failed MSVC build with C1083 for a generated `.moc` include, despite
the file appearing after failure; its CTest was not run and the baseline cause
was not established. No full suite was run. F57 closes F55's helper-only
integration limitation and adds Gate 1 and Gate 2 evidence; both remain
Partial. Workspace boundary and audited `src/next` dependency isolation remain
Satisfied, and the Phase 2 exit gate remains Open. Sub Prep remains capped at
the current and following calendar years at most.

### Progress update - 2026-09-26 (F58 typed Schedule Import matching identities, commit `9b9183818fc2163d625a8ffb088a492a4aa631a9`)

[`ScheduleImportMatchingProjection`](../../src/next/application/schedule_import_matching_projection.h)
now uses `Domain::TeacherId`/`Domain::ClassId` in its app-less matching
contract and represents the suggested class as optional. The legacy repository
adapter converts resolved numeric IDs back to the existing integer preview.
Parity retains ordering, ranks, confidence and explanations, intensive
fallback, empty teacher-key behavior, unfiltered teacher IDs including 0/-1,
and nonpositive class IDs excluded from match/suggestion but retained in
`initiallyAbsentClassIds`.

Executor focused CTest passed 1/1. Independent fresh x64 Ninja/MSVC
19.51.36257/Qt 6.12 verification in
`build/phase2-f58-schedule-matching-tester-20260926` validated 912 handwritten
source owners, built the matching projection and
`ClassMngrScheduleImportTests`, and passed exact CTest 2/2, including the
checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
path `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase`. No full suite
was run. At F58, the adapter's no-suggestion `-1` sentinel lacked a direct
production assertion; F63 closes this specific gap. The data model default
was `-1` and app-less contract tests already asserted absent optional
suggestions. Gate 1 and Gate 2 remain Partial; Workspace
boundary and audited `src/next` dependency isolation remain Satisfied; the
Phase 2 exit gate remains Open. Sub Prep remains capped at the current and
following calendar years at most.

### Progress update - 2026-09-26 (F59 typed Schedule Import state-validation identities, commit `7769912e1a8ccec02ecc3ace2de11ff98c719327`)

`Application::validateScheduleImportState` now carries teacher and class
targets, snapshots, and class-to-teacher links as `Domain::TeacherId` and
`Domain::ClassId`. The `scheduleService()` repository adapter performs
action-aware conversion from legacy numeric IDs and sentinels. Compile-time
checks keep the identity categories distinct. Regression coverage preserves
state-validation ordering, skip target exactness and uniqueness, conflict
ordering for numeric IDs and synthetic classes, and
`rejectsSkippedClassWithMismatchedTeacherKey`.

Fresh independent MSVC 19.51/Ninja/Qt 6.12 verification passed exact CTest
2/2: `ClassMngrNextApplicationScheduleImportStateValidationTests` and
`ClassMngrScheduleImportTests`. The latter includes the checked-in
[`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx)
apply fixture. No full suite was run. Direct sentinel coverage does not yet
exhaustively enumerate every action/sentinel combination.

### Exit-gate status after F59

This cumulative audit applies the formal exit criteria above through F59. The
F59 focused tests were freshly and independently built; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | Typed Schedule Import matching and state-validation identities add compile-time separation and deterministic ordering/conflict rules; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F59 preserves the Schedule Import workbook apply path and covers mismatched teacher-key Skip rejection; wider baseline fixture parity remains incomplete, and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F59 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F59's contract remains app-less and the repository conversion stays at the legacy edge. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. Sub Prep remains limited to the current and following
calendar years, 2026-2027.

### Progress update - 2026-09-26 (F60 typed Schedule Import review-decision targets, commit `730955dd1feb24e5a46dd0bfef9f86b8ff619621`)

Schedule Import review-decision requests and issues now carry optional
`Domain::ClassId` targets. `PlanValidator` and the dialog feature edge convert
legacy integer targets, representing nonpositive values as absent. App-less
validation preserves required targets for UpdateExisting, target-free CreateNew,
and optional-target Skip; duplicate-claimant and issue details/order remain
stable. Dialog coverage preserves translated issue text and existing class-label
behavior. Compile-time checks establish typed fields and keep `ClassId` distinct
from `TeacherId`; explicit CreateNew-without-target and Skip-without-target
acceptance cases are covered.

Independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 verification validated 912
handwritten source owners, built three targets in 321 steps, and passed exact
CTest 3/3: `ClassMngrNextApplicationScheduleImportReviewDecisionsTests`,
`ClassMngrScheduleImportDialogTests`, and `ClassMngrScheduleImportTests`. The
latter includes `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` with
the checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx).
After adding Skip-without-target acceptance, the executor reran the app-less
suite successfully (1/1); `git diff --check` passed. No full suite was run.
The remaining coverage gap is the F59 action/sentinel matrix.
`reviewWarnsForDuplicateClassTargets` directly asserts that the warning
includes the resolved existing class label `E5 Athena`.

### Exit-gate status after F60

This cumulative audit applies the formal exit criteria above through F60. The
focused F60 tests were freshly and independently built; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | Typed Schedule Import matching, apply-state validation, and review-decision targets strengthen identity and decision contracts; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F60 preserves review-decision details/order, dialog labels/translations, and the workbook apply path; wider baseline fixture parity remains incomplete and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F60 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F60 keeps legacy conversion at the feature edges. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. Sub Prep remains limited to the current and following
calendar years, 2026-2027.

### Progress update - 2026-09-26 (F61 Evaluation Default Selection read-failure coverage, commit `5e08c2aab8c4c326463e969445757fa90e25d79c`)

`failedCurrentEvaluationReadReturnsNoDefault()` adds temporary-database
integration coverage through production `ApplicationServices` and
`EvaluationDefaultSelection::forClass`; no production code changed. The test
creates a valid M2 class and ClassInfo with a 2026 saved schedule. With an
empty Fall evaluation, `CurrentOrPreviousTerm` selects Summer on 2026-09-07.
After the `speaking_evaluations` table is dropped, a direct Fall evaluation
service read reports an error and `forClass` returns no default.

Executor and independent fresh x64 Ninja/MSVC 19.51/Qt 6.12 configure/build
validated 912 handwritten source owners and each passed exact CTest 1/1:
`ClassMngrEvaluationDefaultSelectionIntegrationTests`. No full suite was run.

### Exit-gate status after F61

This cumulative audit applies the formal exit criteria above through F61. The
focused F61 integration test was freshly and independently built; no full
suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | The typed Schedule Import matching, state-validation, and review-decision contracts remain covered; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F61 verifies the production Evaluation Default Selection read-failure path, while wider baseline fixture parity remains incomplete and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F61 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F61 adds integration coverage without changing production dependencies. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. F59's exhaustive action/sentinel matrix remains a
follow-up. Sub Prep remains limited to the current and following calendar years, 2026-2027.

### Progress update - 2026-09-26 (F62 Schedule Import apply-boundary sentinel characterization, commit `691e56fbdcc536aaaf577602feeac25fc5b7227f`)

F62 adds data-driven production apply tests for the action/sentinel behavior that
can reach `ScheduleImportRepository::scheduleService()`. Reuse/UpdateRoom
teacher targets -1/0 are checked with matching snapshot rows present and
absent; Create/Skip teacher targets cover -1/0 and positive foreign IDs. Class
cases cover CreateNew sentinels, targetless Skip sentinels, exact and mismatched
positive Skip targets, and stale positive UpdateExisting targets. Rejected
applies compare persisted snapshots and confirm no changes. Positive CreateNew
and nonpositive UpdateExisting class targets are rejected upstream by the F60
PlanValidator.

The CreateNew class sentinel conversion itself is not isolated: F60
PlanValidator canonicalizes nonpositive IDs to absence and state validation does
not inspect CreateNew targets. Those test rows establish overall apply behavior,
not the repository conversion. The F62 matrix therefore adds bounded
characterization without claiming exhaustive adapter-branch coverage.

Fresh independent x64 Ninja/MSVC 19.51/Qt 6.12 verification built
`ClassMngrNextApplicationScheduleImportStateValidationTests` and
`ClassMngrScheduleImportTests` and passed the exact CTest filter 2/2. The latter
includes `previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` with the
checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx).
No full suite was run.

### Exit-gate status after F62

This cumulative audit applies the formal exit criteria above through F62. The
focused F62 targets were freshly and independently built; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | Typed Schedule Import matching, apply-state validation, and review-decision contracts remain covered; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F62 adds apply-boundary action/sentinel characterization and preserves the checked workbook apply path; wider baseline fixture parity remains incomplete and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F62 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F62 changes tests only. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. F62 narrows but does not exhaust the F59 action/sentinel
matrix, with the CreateNew class conversion limitation above. Next slice:
select F63. Sub Prep remains capped at 2026-2027, the current and following
calendar years.

### Progress update - 2026-09-26 (F63 Schedule Import no-suggestion sentinel assertion, commit `bf4251eca530066ba65b00021f63779d185bd64e`)

`previewsAndAppliesCheckedInWorkbookAgainstSeededDatabase` now asserts that
the first fixture candidate, M3/Song, has no matching IDs, the legacy
`suggestedClassId == -1`, `exactMatch == false`, and confidence None. The
seeded class set (E4/Hercules and M2/Atlas) makes the no-match production
reachable; the existing positive-suggestion case remains. This closes the F58
production-adapter assertion gap for the no-suggestion `-1` sentinel. The
app-less projection already covered absence of a suggested class.

Fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12.0 verification
validated 912 handwritten source owners. Executor and Tester each passed the
exact `ClassMngrScheduleImportTests` and
`ClassMngrNextApplicationScheduleImportMatchingProjectionTests` CTests (2/2)
in separate fresh trees. No full suite was run.

### Exit-gate status after F63

This cumulative audit applies the formal exit criteria above through F63. The
focused F63 targets were freshly and independently built; no full suite was
run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | Typed Schedule Import matching, apply-state validation, and review-decision contracts remain covered; broader Domain and Application behavior remains incomplete. F63 adds no new Gate 1 behavior; app-less absence coverage was already present. |
| Baseline parity | Partial | F63 adds Gate 2 production-adapter evidence for the no-suggestion sentinel and retains the positive-suggestion case through the checked-in workbook; broader baseline fixture parity remains incomplete and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F63 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F63 changes tests only. |

Gate 1 remains Partial and unchanged by F63; Gate 2 remains Partial with new
adapter evidence. Workspace boundary and audited `src/next` dependency
isolation remain Satisfied. Phase 2 remains In Progress and its exit gate
remains Open. F59's action/sentinel coverage remains bounded; F62's
CreateNew class conversion limitation still applies. Next slice: select F64.
Sub Prep remains capped at 2026-2027, the current and following calendar years.

### Progress update - 2026-09-26 (F64 StudentNamePair Domain value and roster score-import join, commit `559b4feaa8fd67c01cd2f4d0f3ddd7dc0f166de5`)

Qt-free `Domain::StudentNamePair` stores exact English and Korean UTF-16
components separately, rejects either empty half, and provides equality and
ordering. `RosterEditorWidget::importScores` trims both names at the Qt
boundary, skips incomplete pairs, and joins using the typed pair while
preserving last-write-wins behavior for duplicate imported score pairs.
Separate components remove delimiter ambiguity for invalid stored names
containing U+001F.

Domain coverage includes empty components; exact, case, internal-whitespace,
UTF-16, and ordering behavior; and separator-containing pairs. The real widget
fixture verifies one-sided outer trimming and later-grade persistence for a
duplicate pair in the second saved speaking-evaluation row; current validation
rejects new duplicates. Fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds
each passed exact CTest 2/2: `ClassMngrNextDomainContractTests` and
`ClassMngrRosterEditorWidgetImportTests`. No full suite was run.

### Exit-gate status after F64

This cumulative audit applies the formal exit criteria through F64. Executor
and independent Tester freshly built the two focused F64 targets; no full
suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F64 adds a Qt-free, directly tested student name-pair value; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F64 adds a real widget-import check for one-sided trim and persisted duplicate-row behavior; broader baseline parity remains incomplete and the full suite has not run. |
| Workspace boundary | Satisfied | The `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less tests remain satisfied; F64 does not change this boundary. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F64's new Domain value remains Qt-free. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. F59's action/sentinel coverage remains bounded; F62's
CreateNew class conversion limitation still applies. F65 selection is pending.
Sub Prep remains capped at the current and following calendar years,
2026-2027.

### Progress update - 2026-09-26 (F65 legacy profile startup migration coverage, commit `a4fbffb91228ab1d783ac782ff572d49d3c28b65`)

[`FileControllerWorkspaceLifecycleTests`](../../tests/file_controller_workspace_lifecycle_tests.cpp)
now materializes the checked-in
[`legacy_startup.sql`](../../tests/fixtures/workspaces/legacy_startup.sql)
fixture as a `.db` and opens it through `FileController` into
`ApplicationServices`/`WorkspaceCoordinator`. The test checks the normalized
active path; migrated teacher, class, ClassInfo, and schedule values; schema
version 6; the service projection of the unassigned teacher as `-1` while the
database stores `NULL`; and the retained `.pre-schema-v4-backup` at schema
version 3.

Fresh Executor and independent Tester Windows x64 Debug Ninja/MSVC 19.51/Qt
6.12 builds each validated 913 handwritten source owners, built
`ClassMngrFileControllerWorkspaceLifecycleTests` and
`ClassMngrDatabaseSchemaManagerTests`, and passed their exact CTests 2/2. No
full suite was run.

### Exit-gate status after F65

This cumulative audit applies the formal exit criteria through F65. The two
focused F65 targets were freshly and independently built; no full suite was
run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F64 adds a directly tested Qt-free student name-pair value; the broader Domain and Application behavior remains incomplete. F65 adds no Gate 1 behavior. |
| Baseline parity | Partial | F65 adds production-path legacy `.db` startup/open and schema-migration evidence, including migrated service values and retained pre-v4 backup. Wider fixture-backed baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F65 adds startup lifecycle integration evidence without changing that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F65 changes tests only. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its
exit gate remains Open. F59's action/sentinel coverage remains bounded; F62's
CreateNew class conversion limitation still applies. F66 selection is pending.
Sub Prep remains capped at the current and following calendar years,
2026-2027.

### Progress update - 2026-09-26 (F66 Sub Prep teacher display-name rule, commit `9afa17f47aadb7188916cc091e370f5d0bea98bb`)

Qt-free [`Domain::TeacherDisplayName`](../../src/next/domain/teacher_display_name.h)
owns the selected UTF-16 display value using the existing precedence: preferred
name, English, preferred romanization, then Korean. Both Sub Prep platform
adapters trim the source `QString` fields at the Qt boundary before calling the
Domain rule. The schedule-summary adapter retains its `N/A` empty fallback;
class details retains an empty value.

Domain tests cover each precedence branch, empty and copied values, and
non-ASCII text. Production adapter tests cover padded preferred-name input and
both all-empty fallbacks. Fresh Executor and independent Tester Windows x64
Debug Ninja/MSVC 19.51.36257/Qt 6.12 builds each validated 914 handwritten
owners and passed exact CTests 2/2:
`ClassMngrNextDomainContractTests` and
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`. No full
suite was run. F66 does not change Sub Prep range logic; the query remains
capped at the current and following calendar years, 2026-2027.

### Exit-gate status after F66

This cumulative audit applies the formal exit criteria through F66. The two
focused F66 targets were freshly and independently built; no full suite was
run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F64 adds the typed student-name pair; F66 adds the Qt-free teacher display-name precedence rule with direct tests. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F65 adds production-path legacy profile startup/migration coverage; F66 adds Sub Prep adapter parity for boundary trimming and the two existing empty fallbacks. Wider fixture-backed baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F65 startup integration and F66 do not change that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies. F66 keeps the selection rule Qt-free and normalization in the platform adapters. |

Gate 1 and Gate 2 advance but remain Partial; Workspace boundary and audited
`src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. F59's action/sentinel coverage remains bounded; F62's
CreateNew class conversion limitation remains distinct from F67's directly
tested application-state rule: `PlanValidator` rejects the targeted input
upstream, so the production regression suite does not reach that branch. Next
entry: select F68. Sub Prep remains capped at the current and following
calendar years, 2026-2027.

### Progress update - 2026-09-26 (F67 targeted Schedule Import CreateNew state, commit `4081cc0fcbfb504766e8e10f98839f9eeffbf6ce`)

`Application::validateScheduleImportState` now returns the distinct
`CreateNewClassHasTarget` error when a CreateNew class decision carries a
target, and continues to accept a targetless CreateNew decision. The repository
maps the error to its user-facing text. Direct app-less tests cover target ID
42 rejected and no target accepted; the existing review-decision contract also
rejects the combination and its focused test was rerun.

Independent fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds each
validated 914 handwritten source owners and passed the exact state-validation,
review-decision, and production Schedule Import CTests 3/3. `git diff --check`
was clean; no full suite was run. The production suite is a regression guard:
normal `PlanValidator` rejects this input upstream, so it does not exercise the
new state-validation branch end-to-end and adds no new Gate 2 evidence.

### Exit-gate status after F67

This cumulative audit applies the formal exit criteria through F67. The three
focused targets were freshly and independently built; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F67 adds a directly tested application-state invariant for targeted CreateNew class decisions. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | Existing F63/F65/F66 production-path parity evidence remains; F67's production suite revalidates behavior but does not reach the new branch, so it adds no new parity evidence. Wider baseline parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F67 does not change the criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F67 keeps the validation rule in the app-less Application contract. |

Gate 1 remains Partial with new app-less state-validation evidence. Gate 2
remains Partial with prior evidence revalidated but no F67 parity gain. Workspace
boundary and audited `src/next` dependency isolation remain Satisfied; Phase 2
remains In Progress and its exit gate Open. F62's upstream-unreachable
CreateNew sentinel-conversion observability limitation remains distinct from
F67's newly tested application-state rejection. Next entry: select F68. Sub
Prep remains capped at the current and following calendar years, 2026-2027.

### Progress update - 2026-09-26 (F68 Qt-free calendar campus visibility policy, commit `3ee0b1c6`)

`Application::CalendarEventCampusVisibilityPolicy` now owns literal campus-token
matching without Qt dependencies. The Qt feature adapter retains QString
trimming, campus-code normalization, and one-to-one case-fold preprocessing,
then delegates matching. App-less policy tests and typed-summary/legacy adapter
regressions cover show-all and empty defaults, punctuation, S2/S20 and token
boundaries, Kelvin U+212A, and dotless i U+0131. These Unicode cases are targeted
regressions, not an exhaustive equivalence proof.

Fresh independent Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 builds each
validated 915 handwritten source owners and passed the exact
`ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrCalendarEventCacheTests`, and `ClassMngrCalendarImportTests` CTests
3/3. `git diff --check` passed; no full suite was run.

### Exit-gate status after F68

This cumulative audit applies the formal exit criteria through F68. The three
focused F68 targets were freshly and independently built; no full suite was
run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F68 adds directly tested Qt-free calendar campus-visibility behavior. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F68 adds typed-summary and legacy adapter regression checks for campus visibility; these are not checked-in baseline fixture parity. Wider fixture-backed parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F68 does not change that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F68 keeps campus-token matching in a Qt-free Application policy. |

Gate 1 gains direct app-less behavior evidence and Gate 2 gains adapter
regression evidence; both remain Partial. Workspace boundary and audited
`src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. F62's CreateNew sentinel-conversion observability
limitation and F67's upstream-unreachable production branch remain distinct
and unresolved. Next entry: select F69. Sub Prep remains capped at the current
and following calendar years, 2026-2027.
## Verified F69 Schedule Import state projection - commit `95aaefa4`

Qt-free [`Application::projectScheduleImportStateSchedules`](../../src/next/application/schedule_import_state_projection.h)
projects the final normal or intensive schedule rows using typed
`Domain::ClassId | ScheduleImportStateCandidateIndex` references. Each row also
carries an explicit `ReplaceRows` or `KeepExistingRows` persistence disposition.
The repository computes this projection once, passes the same rows to overlap
validation, and resolves candidate indexes to generated class IDs only after
inserting new classes. Intensive `UpdateExisting` keeps untouched existing rows
and their identities; normal import, `ReplaceWithNew`, and Skip retain replacement
and preservation behavior.

App-less tests cover typed references, source meeting order, overlap, intensive
modes, and skip dispositions. Production tests assert persisted-row parity for
checked-in [`schedule_review.xlsx`](../../tests/fixtures/imports/schedule_review.xlsx),
intensive replacement and untouched-row identity; the checked-in overlap fixture
and existing skipped exact-match, pre-write, and rollback cases remain covered.
Two independent fresh Windows x64 Debug Ninja/MSVC 19.51.36257/Qt 6.12 trees
validated 916 handwritten source owners and passed
`ClassMngrNextApplicationScheduleImportStateValidationTests` and
`ClassMngrScheduleImportTests` (2/2). `git diff --check` passed; no full suite was
run.

### Exit-gate status after F69

This cumulative audit applies the formal exit criteria through F69. Both focused
targets were independently built and passed; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F69 directly tests the Qt-free final schedule-row projection, typed class/candidate references, source meeting order, overlap, intensive modes, and skip dispositions. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F69 adds persisted-row parity for the checked-in `schedule_review.xlsx` fixture. Separate production assertions cover intensive row replacement and untouched-row identity; checked-in `schedule_overlap_conflict.xlsx` verifies pre-write overlap rejection. Broader baseline fixture parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F69 does not change that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F69 keeps the projection Qt-free and resolves generated IDs at the repository boundary. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress with its exit
gate Open. F62's CreateNew sentinel-conversion observability limitation and
F67's upstream-unreachable production validation branch remain distinct. Next
entry: select F70 from the remaining Phase 2 gaps. Sub Prep remains limited to
the current and following calendar years, 2026-2027.
## Verified F70 Class Transfer preview matching policy - commit `2f3d414c`

The Qt-free Application matching policy
([`matchClassTransferCandidates`](../../src/next/application/class_transfer_matching_policy.h))
now owns preview matching over normalized source values and ordered
destination snapshots. The repository adapter retains Qt simplified/case-fold
normalization and converts typed teacher/class identities to the legacy integer
preview representation. App-less coverage checks teacher-match rules, course
and teacher identity, assigned-but-unloaded teacher handling versus unassigned
fallback, and destination order. Production coverage preserves normalized
matching, the checked-in success/conflict fixture preview IDs, and conflict
no-write behavior. Fixture parity is covered; exhaustive Unicode case-fold
equivalence is not claimed.

Independent and executor fresh Windows x64 Debug Ninja/MSVC 19.51.36257/Qt
6.12 builds each validated 917 handwritten source owners and passed
`ClassMngrNextApplicationClassTransferTests` and
`ClassMngrClassTransferTests` (2/2). No full suite was run.

### Exit-gate status after F70

This cumulative audit applies the formal exit criteria through F70. Both focused
targets were independently built and passed; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F70 adds direct Qt-free Class Transfer matching rules and ordering tests. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F70 rechecks checked-in success/conflict preview IDs and conflict no-write behavior after moving matching into Application. Broader baseline fixture parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F70 does not change that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | The audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; the new matching policy is Qt-free and repository-owned normalization/presentation remain at the adapter. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress with its exit
gate Open. F70 verifies existing fixture parity without claiming exhaustive
Unicode case-fold coverage. Next entry: select F71 from the remaining Phase 2
gaps. Sub Prep remains limited to the current and following calendar years,
2026-2027.
## Verified F71 typed Class Transfer review decision identities - commit `9b090cb5`

[`class_transfer_projection.h`](../../src/next/application/class_transfer_projection.h)
now uses typed `Domain::ClassId`/`TeacherId` match and issue identities, with
optional typed resolution targets. Both app-less validators preserve action,
membership, duplicate, and missing-target rules. UI and repository adapters retain
legacy integer APIs: positive values become typed IDs, exactly `-1` means absent,
and `0`/`-2` retain existing action-specific errors and invalid-action precedence.

App-less tests assert field categories, no implicit integer conversion, optional
targets, and typed issue identity. UI tests cover class and teacher `0`/`-2` targets
for both available actions. Repository tests cover class Create/Replace and teacher
Create/Keep cases with `0`/`-2`, invalid-action precedence, and no writes. The checked-in
`success_source.json` now exercises a nonempty exact teacher/class match and
dialog-selected Replace: it asserts the destination class ID and teacher profile
are retained, class details/schedule/roster are replaced, and old evaluation is
cleared. The existing Create fixture and checked-in conflict/no-write path remain.

Executor and independent Tester each used a fresh Windows x64 Debug Ninja/MSVC
19.51.36257/Qt 6.12 tree, validated 917 handwritten source owners, and passed
`ClassMngrNextApplicationClassTransferTests` and `ClassMngrClassTransferTests` (2/2).
`git diff --check` passed; no full suite was run.

### Cumulative exit-gate status after F71

This audit applies the formal exit criteria through F71. Both focused targets passed
independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F71 adds typed Class Transfer match/issue identities and optional typed targets while preserving validator rules. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F71 exercises exact teacher/class matching and successful dialog-selected Replace through the checked-in success fixture, retaining the Create fixture and conflict/no-write coverage; sentinel and invalid-action matrices add adapter evidence. Broader baseline fixture parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F71 does not change that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies. F71's review contract remains Qt-free; legacy integer conversion stays in UI/repository adapters. |

Gate 1 and Gate 2 remain Partial; Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress and its exit
gate Open. Sub Prep remains limited to the current and following calendar years,
2026-2027.
## Verified F72 Class Transfer teacher replacement parity - commit `aa1af5fe`

The production test uses checked-in `success_source.json` and a seeded matching
teacher whose non-identity profile fields all differ from the fixture. It selects
Teacher ReplaceExisting through `ClassImportDialog` action/target item data and
applies with `DataService::importClasses`. Assertions verify the existing
`TeacherId` is retained, every teacher profile field receives the fixture value,
no duplicate teacher is created, and the imported class references the retained
teacher. F71 class replacement remains covered; Create and checked-in
`conflict_source.json` no-write paths are unchanged. This commit changes tests only.

Executor and independent Tester used separate fresh Windows x64 Debug Ninja/MSVC
19.51.36257/Qt 6.12 trees, each validated 917 handwritten source owners and
passed `ClassMngrNextApplicationClassTransferTests` and
`ClassMngrClassTransferTests` (2/2). `git diff --check` passed; no full suite was
run. F72 adds Gate 2 teacher-replacement fixture parity and no Gate 1 evidence;
both gates remain Partial. Workspace boundary and audited v2 dependency isolation
remain Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep
remains limited to the current and following calendar years, 2026-2027.

### Cumulative exit-gate status after F72

This audit applies the formal exit criteria through F72. Both focused targets
passed independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F72 adds no app-less behavior evidence; F71's typed Class Transfer contract evidence remains. Broader Domain and Application behavior is incomplete. |
| Baseline parity | Partial | F72 adds checked-in fixture parity for Teacher ReplaceExisting, verifying retained identity, replaced profile fields, no duplicate teacher, and class linkage; existing class replacement, Create, and conflict/no-write paths remain. Broader baseline fixture parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F72 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F72 changes tests only. |

Gate 1 remains Partial and unchanged; Gate 2 remains Partial with added teacher
replacement fixture evidence. Workspace boundary and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.
## Verified F73 Class Transfer Skip action fixture parity - commit `60bbd015`

`tests/class_transfer_tests.cpp` extends
`permanentConflictFixturePresentsReviewAndRejectsScheduleCollision` using the
checked-in [`conflict_source.json`](../../tests/fixtures/transfers/conflict_source.json).
It retains default schedule-collision rejection/no-write coverage and exercises
class Skip with teacher ReplaceExisting selected through production dialog plan
metadata. The import succeeds with one skipped class and no created/replaced
class; teacher profile, class details and ClassInfo schedules, roster, speaking
evaluation, and record counts remain unchanged. The source commit changes only
this test file.

Executor and independent fresh Tester trees used Windows x64 Debug, Ninja 1.13.2,
MSVC 19.51.36257, and Qt 6.12.0; each validated 917 source owners and passed
`ClassMngrNextApplicationClassTransferTests` and
`ClassMngrClassTransferTests` (2/2). Tester also ran the fixture test directly.
`git diff --check` passed; no full suite was run. F73 adds Gate 2 fixture-backed
Skip-action evidence; Gate 1 is unchanged. Both gates remain Partial, workspace
boundary and audited v2 dependency isolation remain Satisfied, and Phase 2's
exit gate remains Open. Sub Prep remains capped at the current and following
calendar years, 2026-2027.

### Cumulative exit-gate status after F73

This audit applies the formal exit criteria through F73. Both focused targets
passed independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F73 is test-only and adds no app-less behavior evidence; F71's typed Class Transfer contract evidence remains. Broader Domain and Application behavior is incomplete. |
| Baseline parity | Partial | F73 adds checked-in conflict-fixture coverage for dialog-selected class Skip alongside teacher ReplaceExisting metadata, while preserving collision rejection/no-write and asserting unchanged teacher/class-related records. F72 teacher replacement and F71 class replacement fixture parity remain; broader baseline fixture parity is incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F73 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F73 changes tests only. |

Gate 1 remains Partial and unchanged; Gate 2 remains Partial with added Skip
action fixture evidence. Workspace boundary and audited v2 dependency isolation
remain Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep
remains capped at the current and following calendar years, 2026-2027.
## Verified F74 synthetic Intensive Schedule Import production flow - commit `3e0a8d64`

[`tests/schedule_import_tests.cpp`](../../tests/schedule_import_tests.cpp) adds
`previewsAndAppliesSyntheticIntensiveWorkbookAgainstSeededDatabase`, using the
source-readable authored worksheet
[`schedule_intensive_synthetic_worksheet.xml`](../../tests/fixtures/imports/schedule_intensive_synthetic_worksheet.xml).
The test parses it as Intensive, asserts fixed candidate and preview values,
explicitly applies UpdateExisting, and verifies persisted target Intensive rows
and class identity with no class creation. An untouched Intensive row retains
its ID and value, and regular schedule rows remain unchanged. This is synthetic
production-flow coverage only: the repository has no historical Intensive
workbook or legacy-output oracle, so historical baseline parity is not
established.

Executor and independent fresh Tester trees used Windows x64 Debug Ninja and
Qt 6.12 (Executor MSVC 14.51.36231; Tester MSVC 19.51.36257). CMake validated
917 handwritten source owners; `ClassMngrScheduleImportTests`,
`ClassMngrNextApplicationScheduleImportStateValidationTests`, and
`ClassMngrNextApplicationScheduleImportMatchingProjectionTests` passed 3/3
independently. `git diff --check` passed; no full suite was run. Protected
`cmake/sources.cmake` remains at SHA-256
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
F74 adds synthetic production-flow evidence but no historical baseline parity;
Gate 1 is unchanged and Gate 2 remains Partial. Workspace boundary and audited
v2 dependency isolation remain Satisfied. Phase 2's exit gate remains Open.
Sub Prep remains capped at the current and following calendar years, 2026-2027.

### Cumulative exit-gate status after F74

This audit applies the formal exit criteria through F74. The three focused
CTests passed independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F74 is test-only and adds no app-less behavior evidence; F71's typed Class Transfer contract evidence remains. Broader Domain and Application behavior is incomplete. |
| Baseline parity | Partial | F74 verifies a synthetic Intensive parse/preview/apply path and persisted-state invariants, but no historical Intensive workbook or legacy-output oracle exists; it does not establish historical baseline parity. F71-F73 Class Transfer fixture checks remain, and broader baseline parity is incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F74 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F74 changes tests only. |

Gate 1 remains Partial and unchanged. Gate 2 remains Partial: F74 adds synthetic
production-flow evidence but no historical parity evidence. Workspace boundary
and audited v2 dependency isolation remain Satisfied. Phase 2 remains In
Progress with its exit gate Open. Next selected bounded slice: F75 implements a
Qt-free typed student-name-pair duplicate-grouping policy for roster and
speaking-evaluation duplicate validation, adapted at current callers while
preserving caller-side trimming, incomplete-row handling, and caller-specific
diagnostics. This selection is not implementation evidence; score-import
last-write-wins behavior remains distinct. Sub Prep remains capped at the
current and following calendar years, 2026-2027.
## Verified F75 typed duplicate student-pair grouping - commit `3139bdf4`

[`student_name_pair.h`](../../src/next/domain/student_name_pair.h) adds the
header-only, Qt-free `duplicateStudentNamePairGroups` policy. It accepts one
`optional<StudentNamePair>` per row, skips incomplete (`nullopt`) rows, groups
by exact equality of both UTF-16 name parts, and returns only duplicate groups.
Groups follow each pair's first input occurrence; row indexes follow input
order. It performs no trimming, case folding, or other name normalization.

Shared validation, roster validation, and speaking-evaluation validation adapt
their rows at the existing callers: they trim both names, represent incomplete
pairs as absent, then retain their existing field/row locations, duplicate-row
arguments, and caller-specific cell messages. Score-import last-write-wins
lookup behavior is unchanged. App-less tests cover exact pair identity,
delimiter-part separation, case sensitivity, skipped rows, group and row order,
and deterministic repeated calls; adapter tests cover trimmed matching,
incomplete rows, and preserved diagnostics.

Independent fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257, and Qt 6.12.0, validated 917 handwritten source owners, and
passed the four focused Domain/shared-policy/roster/speaking-evaluation CTests
(4/4) plus `ClassMngrRosterEditorWidgetImportTests` (1/1). `git diff --check`
passed; no full suite was run. Protected `cmake/sources.cmake` remains at
SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Gate 1 gains app-less policy evidence but remains Partial. F75 adds no historical
baseline parity, so Gate 2 remains Partial. Workspace boundary and audited v2
dependency isolation remain Satisfied; Phase 2 remains In Progress with its
exit gate Open. Sub Prep remains capped at the current and following calendar
years, 2026-2027.

### Cumulative exit-gate status after F75

This audit applies the formal exit criteria through F75. The four focused CTests
and additional roster import CTest passed independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F75 adds exact typed duplicate-pair grouping and deterministic group/order tests in the Qt-free Domain policy. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F75 adds no historical baseline parity. F71-F73 checked-fixture Class Transfer evidence and F74 synthetic Intensive production-flow evidence remain; broader baseline coverage is incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F75 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F75's Domain grouping policy remains Qt-free. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.
Next selected bounded slice: F76 adds a nonempty speaking evaluation to
`tests/fixtures/transfers/success_source.json` and asserts the full persisted
evaluation row through fixture-driven Class Transfer review/apply create and
replacement. This is checked-fixture regression coverage, not evidence of
historical baseline parity. Sub Prep remains capped at the current and following
calendar years, 2026-2027.
## Verified F76 Class Transfer speaking-evaluation fixture regression - commit `7a8b80c6`

The checked-in
[`success_source.json`](../../tests/fixtures/transfers/success_source.json)
now contains a named 11-column Fixture Evaluation. Fixture-driven Class Transfer
create and replacement tests compare all 25 persisted speaking-evaluation rows
against literal expected values; replacement also preserves the assertion that
the destination-only evaluation is cleared. This verifies checked-fixture
regression behavior, not historical baseline parity.

Executor and independent fresh Windows x64 Debug builds used CMake 4.4.2,
Ninja 1.13.2, Qt 6.12.0, and MSVC 19.51.36257; CMake validated 917 handwritten
source owners, and `ClassMngrClassTransferTests` passed 1/1. `git diff --check`
passed; no full suite was run. Protected `cmake/sources.cmake` remains at
SHA-256 `9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.
Gate 2 gains checked-fixture evidence but remains Partial; Gate 1 is unchanged
and remains Partial. Workspace boundary and audited v2 dependency isolation
remain Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep
remains capped at the current and following calendar years, 2026-2027.

### Cumulative exit-gate status after F76

This audit applies the formal exit criteria through F76. The focused Class
Transfer CTest passed independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F76 adds production adapter regression coverage but no new app-less behavior; F75's exact typed Domain grouping policy and prior contracts remain. Broader Domain and Application behavior is incomplete. |
| Baseline parity | Partial | F76 verifies every persisted row of the named Fixture Evaluation on checked-fixture create and replacement, while retaining the cleared destination-only evaluation check. This is checked-fixture coverage, not historical baseline parity; broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F76 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F76 changes fixture and production tests only. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.
Next selected bounded slice: F77 extracts Class Transfer weekly schedule
overlap into a Qt-free Application policy, feasible per two independent context
reviews. Preserve half-open intervals; end-at-or-before-start overnight
intervals, including Sunday-to-Monday wrap; regular/intensive separation;
Skip/replacement filtering; deterministic conflict ordering; existing
repository error rendering and deduplication; and the checked-in conflict-fixture
adapter guard. This is a selected slice, not implementation evidence. Sub Prep
remains capped at the current and following calendar years, 2026-2027.
## Verified F77 Class Transfer weekly schedule-overlap policy - commit `dd3bbc01`

[`class_transfer_projection.h`](../../src/next/application/class_transfer_projection.h)
now contains a Qt-free weekly overlap policy. The repository adapter retains
legacy weekday/time parsing, localized diagnostic rendering, and duplicate
message suppression. The policy preserves half-open intervals, touching-time
nonconflicts, overnight intervals including Sunday-to-Monday week wrap, equal
endpoints as 24-hour intervals, regular/intensive category separation, and
deterministic incoming/existing conflict order.

App-less and production adapter tests cover those boundaries, exact diagnostics,
and conflict rejection before writes. The checked-in conflict fixture asserts
the exact combined diagnostic. Independent fresh Windows x64 Debug verification
used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; CMake validated
917 handwritten source owners, and `ClassMngrNextApplicationClassTransferTests`
and `ClassMngrClassTransferTests` passed (2/2). `git diff --check` passed; no
full suite was run. Protected `cmake/sources.cmake` remained excluded at SHA-256
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.

### Cumulative exit-gate status after F77

This audit applies the formal exit criteria through F77. Both focused CTests
passed independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F77 adds Qt-free weekly overlap policy evidence with ordering and interval-boundary tests. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F77 verifies exact checked-fixture conflict diagnostics and no-write behavior, adding checked-fixture regression evidence but no historical baseline parity. Broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal `WorkspaceGateway::createWorkspace`/`WorkspaceCoordinator` acceptance and focused app-less coverage remain satisfied; F77 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct `DataService`, `MainWindow`, `PageManager`, and widget-pointer dependencies; F77's overlap policy remains Qt-free. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.
Next selected bounded slice: F78 tests Schedule Import apply rejection when both
classes from the permanent [`schedule_large_conflict.xlsx`](../../tests/fixtures/imports/schedule_large_conflict.xlsx)
fixture are assigned to the same existing class. Assert rejection before writes
by comparing seeded database snapshots. Keep the slice to focused test changes
unless it exposes a production defect. Verify with
`ClassMngrScheduleImportTests`,
`ClassMngrNextApplicationScheduleImportReviewDecisionsTests`, and
`ClassMngrScheduleImportDialogTests`. This is a selected slice, not implementation
evidence. Sub Prep remains capped at the current and following calendar years,
2026-2027.
## Verified F78 Schedule Import duplicate-target fixture regression - commit 68ab0faa

The repository apply test derives two distinct Alice E4 Hercules candidates
from the permanent schedule_large_conflict.xlsx workbook, assigns both to one
seeded existing destination, and asserts the exact duplicate-target rejection
before writes. The persisted-state snapshot covering teachers, classes, class
information, regular and intensive schedules, and app settings is identical
before and after rejection.

Executor verification passed ClassMngrScheduleImportTests. Independent fresh
Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257, and Qt 6.12.0; CMake validated 917 handwritten source
owners. ClassMngrScheduleImportTests,
ClassMngrNextApplicationScheduleImportReviewDecisionsTests, and
ClassMngrScheduleImportDialogTests passed (3/3). git diff --check passed; no
full suite was run. The optional external workbook sample was skipped because
CLASSMNGR_SCHEDULE_IMPORT_SAMPLE was unset.

F78 adds checked-fixture regression coverage, not an independently sourced
historical-output oracle. Gate 1 remains Partial; Gate 2 gains fixture-backed
duplicate-target rejection evidence and remains Partial. Workspace boundary
and audited src/next dependency isolation remain Satisfied. Phase 2 remains In
Progress with its exit gate Open. Sub Prep remains capped at 2026-2027.
Protected cmake/sources.cmake remains excluded at SHA-256
9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF.

### Cumulative exit-gate status after F78

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F77 adds the Qt-free weekly Class Transfer overlap policy. F78 adds no app-less behavior; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F78 verifies fixture-derived duplicate-target rejection and unchanged persisted state. This is checked-fixture regression coverage, not historical baseline parity; broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F78 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited src/next sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F78 changes tests only. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.
Next selected bounded slice: F79 makes Class Transfer weekly schedule intervals
validated app-less values through a factory from parsed weekday and
minute-of-day inputs. Preserve end-at-or-before-start overnight rollover,
equal endpoints as 24 hours, and Sunday endpoints beyond the weekly boundary.
Keep legacy QString parsing and diagnostics in the repository adapter. Verify
with ClassMngrNextApplicationClassTransferTests and
ClassMngrClassTransferTests. This is selected work, not implementation
evidence. Sub Prep remains capped at 2026-2027.
## Verified F79 Class Transfer schedule-candidate validation - commit 947edd93

`ClassTransferScheduleCandidate` is now created through a factory that accepts
the schedule category, parsed weekday, and minute-of-day bounds. It rejects
invalid categories, weekdays, and clock values before producing the weekly
interval. End-at-or-before-start still rolls into the next day, equal endpoints
represent 24 hours, and Sunday rollover can extend beyond the weekly endpoint.
The repository adapter retains legacy Qt weekday parsing and the exact
malformed-day diagnostic.

App-less tests cover invalid category/day/clock values, week-start behavior,
equal endpoints, and Sunday rollover; the adapter test asserts the legacy
malformed-day message. Executor and independent fresh Tester Windows x64 Debug
builds used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. Each
validated 917 handwritten source owners and passed
`ClassMngrNextApplicationClassTransferTests` and `ClassMngrClassTransferTests`
(2/2); Tester also passed `git diff --check`. No full suite was run. Protected
`cmake/sources.cmake` remained excluded at SHA-256
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.

F79 adds app-less interval-validation evidence to Gate 1, which remains Partial.
Gate 2 remains Partial; F78's checked-fixture duplicate-target regression is
not historical-output parity. Workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress with its
exit gate Open. Sub Prep remains capped at 2026-2027.

### Cumulative exit-gate status after F79

This audit applies the formal exit criteria through F79. Both focused CTests
passed independently; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds a validated Qt-free Class Transfer interval value and boundary tests. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F78 adds fixture-derived duplicate-target rejection with unchanged persisted state; F79 verifies the legacy malformed-day adapter diagnostic and interval behavior. These are checked-regression results, not independently sourced historical-output parity. Broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F79 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F79's policy remains Qt-free. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.
Next selected bounded slice: F80 extracts the sparse update rule for Korean
Teacher Import into a Qt-free app-less policy and exercises it through the
`sectioned_review.xlsx` fixture dialog-plan/repository path. Preserve the matched
TeacherId and KoreanTeacherKey; merge nonempty trimmed room, birthday, and phone
values; preserve stored values for blank inputs; report unchanged when the
result equals the existing values; and leave unrelated profile fields alone.
Seed a matching Korean teacher with suffix D, then assert one update and one
create, retained identity, merged and manually maintained fields, no duplicate,
and the source date. Verify the app-less policy and focused
`ClassMngrTeacherImportTests` and `ClassMngrTeacherImportDialogTests`. This is
selected work, not implementation evidence; checked-fixture regression is not
historical-output parity. Sub Prep remains capped at 2026-2027.
## Verified F80 Korean Teacher Import sparse-update policy - commit 15cd876d

The Qt-free Korean Teacher Import update policy preserves the matched
`TeacherId` and `KoreanTeacherKey`, merges nonempty trimmed room, birthday, and
phone values, retains stored values for blanks, reports unchanged when the
result equals the existing profile, and leaves unrelated profile fields alone.
The repository/dialog path keeps the existing matching and fixture-driven
behavior: a checked `sectioned_review.xlsx` case updates the seeded Korean
teacher with suffix D and creates one other teacher, retaining the updated row's
identity, applying source date and nonblank values, preserving manually
maintained fields, and avoiding a duplicate.

Independent fresh Windows x64 Debug verification used an isolated archive at
`4abfd685` with the F80 source/test overlay, CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257, and Qt 6.12.0. Fresh configuration validated 919 handwritten
source owners; the build completed 315 actions. The Korean update policy,
teacher import, and teacher import dialog CTests passed (3/3). The dialog also
passed separately with Windows and offscreen QPA; the teacher import target
reported 16 passing cases and one optional external-workbook skip. No full
suite was run. `git diff --check` passed, including separate checks for the two
new files. Protected `cmake/sources.cmake` remained excluded at SHA-256
`9B15C799FCD0637A4486C54CCAF5D313072A92396F35E575639AD85824347CFF`.

F80 adds app-less policy evidence to Gate 1 and checked-fixture regression to
Gate 2; both remain Partial. The fixture regression is not historical-output
parity. Workspace boundary and audited `src/next` dependency isolation remain
Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep stays
capped at 2026-2027.

### Cumulative exit-gate status after F80

This audit applies the formal exit criteria through F80. The three focused
CTests passed; no full suite was run.

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer interval values; F80 adds a Qt-free Korean Teacher Import sparse-update policy with merge, blank-preservation, unchanged, and identity cases. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F78 and F80 add checked-workbook regression paths, including duplicate-target rejection and Korean teacher update/create behavior; F79 adds interval-boundary and adapter-diagnostic regression coverage. These checks are not independently sourced historical-output parity. Broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F80 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F80's policy remains Qt-free. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.
Next selected bounded slice: F81 extracts Native English Teacher Import's
sparse-update behavior from `teacher_import_repository.cpp` into a Qt-free
Application policy. Keep current Qt matching, trimming, and canonicalization at
the adapter, and retain native-table integer row identity in the repository.
Exercise the existing matching Alex update from the checked
`sectioned_review.xlsx` fixture; test app-less merging of nonempty position,
phone, birthday, nationality, and email, blank-field preservation, name
simplification, and unchanged detection. Extend fixture tests to verify retained
row identity, no duplicate, and source date. Verify the policy and
`ClassMngrTeacherImportTests`; include `ClassMngrTeacherImportDialogTests` only
if the dialog path changes. This is selected regression work, not historical
output parity. Sub Prep remains capped at 2026-2027.
## Verified F81 Native English Teacher Import sparse-update policy - commit 118baceb

The Qt-free Application policy models the Native English teacher profile with
`std::u16string` fields while retaining the native table's raw integer row
identity in the repository. It merges each nonempty normalized incoming
position, phone, birthday, nationality, and email value; preserves stored
values for blanks; takes the name from the adapter-simplified value; and reports
unchanged when all six strings match. The repository retains Qt
simplified/trimmed matching and normalization and the SQL identity boundary.

The checked `sectioned_review.xlsx` case updates the matching Alex row seeded
with ID 8104, verifies adapter padding, no duplicate, source date 2026-09-01,
blank-field preservation, and that a repeated import issues no UPDATE. An
independent fresh Windows x64 Debug tree based on archive `e49aaa15` plus the
F81 overlay used CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0;
it validated 921 handwritten source owners, completed 311 build actions, and
passed `ClassMngrTeacherImportTests` and
`ClassMngrNextApplicationNativeEnglishTeacherImportUpdateTests` (2/2).
Tracked and new-file diff checks passed. The optional external workbook was
unset; no full suite was run.

F81 adds app-less policy evidence to Gate 1, which remains Partial. Gate 2 gains
checked-fixture regression, not historical-output parity, and remains Partial.
Workspace boundary and audited `src/next` dependency isolation remain
Satisfied. Phase 2 remains In Progress with its exit gate Open. Sub Prep
remains capped at 2026-2027.

### Cumulative exit-gate status after F81

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Korean and Native English Teacher Import sparse-update policies. Broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F78, F80, and F81 add checked-fixture regressions; F79 adds interval and adapter-diagnostic regression coverage. These slices add no historical-output oracle, and broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F81 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F81's policy remains Qt-free. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.
Next selected bounded slice: F82 adds a Schedule Import differential test
against legacy commit `48fc5c5c`, running the checked `schedule_review.xlsx`
input against legacy and current code with the same seeded SQLite database.
Capture and compare parser metadata, teacher/class previews, apply counters,
teachers, classes, and regular schedule values. The fixture was added at
`f5fdcc4a`, after the baseline; treat this as common-input differential
regression evidence, not a historically present workbook oracle. Verify with
`ClassMngrScheduleImportTests`; no full suite. This is selected work, not
implementation evidence. Sub Prep remains capped at 2026-2027.
## Verified F82 Schedule Import common-input differential regression - commit 6d8fb296

Only `tests/schedule_import_tests.cpp` changed. A temporary harness ran legacy
commit `48fc5c5c` and current code at `118baceb` on the checked
`schedule_review.xlsx` input with the same seeded SQLite database. All 14
captured semantic outputs matched, covering parser metadata, teacher/class
previews, apply counters `(1,0,2,1,0,2,0,false)`, persisted teachers/classes,
and six regular-hour values. Literal assertions also cover source cells,
teacher matching arrays and affected classes, the unmatched third candidate,
zero ignored cells, no profile-name update, and persisted class-to-teacher
names.

The independent fresh archive build used CMake 3.30.5, Ninja 1.12.1,
MSVC 19.51.36257, and Qt 6.12.0; configuration validated 921 handwritten
source owners and planned 309 build actions. `ClassMngrScheduleImportTests`
passed (1/1), and `git diff --check` passed. The executor's focused QtTest run
reported 51 passed, 0 failed, and one optional external-workbook skip. No full
suite was run. Because `schedule_review.xlsx` was added at `f5fdcc4a` after
baseline `48fc5c5c`, F82 establishes common-input differential regression,
not historical-output parity.

F82 advances Gate 2 with differential evidence but it remains Partial; Gate 1
remains Partial. Workspace boundary and audited `src/next` dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate
Open. Sub Prep remains capped at 2026-2027.

### Cumulative exit-gate status after F82

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Teacher Import update policies. Broader Domain and Application behavior remains incomplete; F82 changes tests only. |
| Baseline parity | Partial | F82 compares 14 semantic outputs for the same checked input and seed against legacy/current code. The fixture postdates the legacy baseline, so this is common-input differential regression, not historical-output parity. F78/F80/F81 checked-fixture regressions remain; broader parity is incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F82 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F82 changes tests only. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited v2 dependency
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.
Selected F83 scope, verified in the latest progress entry in the [Phase 2 plan](03-Phase-2-Domain-Model-and-Application-Contracts.md): compare the checked
`schedule_overlap_conflict.xlsx` input against legacy commit `48fc5c5c` and
current code with the same seed, covering preview values, overlap rejection,
and persisted state before and after apply. The fixture postdates the baseline,
so the result is common-input regression, not historical-output parity. Sub
Prep remains capped at 2026-2027.

## Verified F83 Schedule Import overlap-conflict differential regression - commit 2e8bbab2

Only `tests/schedule_import_tests.cpp` changed. Legacy `48fc5c5c` and current
code `1236e9cb` ran identical fixture bytes and a deterministic SQLite seed
through parser, preview, and apply harnesses. The
`schedule_overlap_conflict.xlsx` fixture has SHA-256
`2de93c4abdc5e82390adede250e8313501a38d4be2053e929c4adbed6d745312` and was
introduced at `3121d90c`, after the legacy baseline.

Semantic transcripts matched for teacher keys and display names 김선생/이선생,
rooms 413/415, and preview inventory `classCount=1`, `regular=true`, and
`intensive=false`. Class ID 9901 was initially absent; both candidate classes
were unmatched, with no suggestion and `None` confidence. Both paths rejected
with the exact message: `The
proposed schedule overlaps: E4 Hercules conflicts with E4 Theseus on Monday.`
Normalized state was unchanged across teachers, classes, class_info, regular
times, intensive times, intensive slot states, and app_settings. The test pins
those preview fields, the exact message, and the seven-table snapshot.

Independent fresh Windows x64 Debug verification used archive `1236e9cb` plus
the final test patch, CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt
6.12.0. CMake validated 921 handwritten source owners; the build completed
309 actions; `ClassMngrScheduleImportTests` passed (1/1), and `git diff
--check` passed. Executor QtTest reported 51 passed, 0 failed, and one optional
external-workbook skip. No full suite was run. Since the fixture was added
after baseline `48fc5c5c`, F83 is common-input differential regression, not
historical workbook parity.

F83 advances Gate 2 with checked common-input differential evidence, but Gate
2 remains Partial. Gate 1 remains Partial; workspace boundary and audited
`src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. Sub Prep remains capped at 2026-2027.

#### Cumulative exit-gate status after F83

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Teacher Import update policies. F82 and F83 change tests only; broader Domain and Application behavior remains incomplete. |
| Baseline parity | Partial | F82 and F83 compare legacy/current semantic behavior on checked inputs and identical seeds, but both fixtures postdate the legacy baseline. They add common-input differential regression, not historical-output parity. Earlier fixture regressions remain; broader parity is incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F83 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F83 changes tests only. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress with its
exit gate Open. Next selected bounded slice: F84 types Schedule Import matching
teacher keys. Represent
`ScheduleImportMatchingCandidate::teacherKey` and
`ScheduleImportMatchingTeacherProjection::teacherKey` as
`Domain::KoreanTeacherKey`; preserve a valid empty key with explicit default
member initialization or the existing factory, without adding a Domain
constructor unless justified. Keep `teacherName` separate and convert to/from
the legacy representation only at `schedule_import_repository.cpp`. Preserve
ordering, room aggregation, match results, and especially
`preservesEmptyTeacherKeyMatchingSemantics`. Verify with
`ClassMngrNextApplicationScheduleImportMatchingProjectionTests` and
`ClassMngrScheduleImportTests`; no new target is expected. This adds Gate 1
evidence but does not close it and adds no historical parity. Broader Teacher
Import plan-validation extraction remains a separate candidate.
This is selected work, not implementation evidence. Sub Prep remains capped at
2026-2027.

## Verified F84 Schedule Import matching-key typing - commit 5207d65a

The candidate and teacher projection now store `Domain::KoreanTeacherKey`;
the empty key is explicitly initialized as a valid value, and the display name
remains separate. `schedule_import_repository.cpp` owns conversion to and from
the legacy representation. Matching behavior, including empty-key matching,
remains covered.

Executor and independent fresh Tester verification passed
`ClassMngrNextApplicationScheduleImportMatchingProjectionTests` and
`ClassMngrScheduleImportTests` (2/2). The Windows x64 Debug verification used
CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; CMake validated
921 handwritten source owners and the build completed 313 actions. `git diff
--check` passed. No full suite was run.

### Cumulative exit-gate status after F84

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Teacher Import update policies; F84 adds typed Schedule Import matching keys. Broader behavior remains incomplete. |
| Baseline parity | Partial | F82 and F83 compare legacy/current behavior on checked inputs and identical seeds, but both fixtures postdate the legacy baseline. They add common-input regression, not historical-output parity. Earlier fixture regressions remain; broader parity is incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F84 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F84's policy remains Qt-free. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress with its
exit gate Open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice: F85 extracts Teacher Import full-plan validation
from `teacher_import_repository.cpp` into a Qt-free Application policy. Keep
Qt normalization and date interpretation, review/validation order, translation,
and exact diagnostics at the repository adapter; pass normalized identity keys
and date validity into the policy. Preserve Korean, Native English, and GS Team
name rules and rejection-before-write behavior. Verify the app-less policy and
focused Teacher Import repository tests. This is selected work, not
implementation evidence. Sub Prep remains capped at 2026-2027.

## Verified F85 Teacher Import full-plan validation - commit d5971ae1

A Qt-free `TeacherImportPlanValidationInput` and issue policy now validate in
established order: optional reviewed ordered Korean-key correspondence,
source-date validity, Korean keys, Native English keys, and GS Team keys. The
repository adapter resolves review choices first and retains Qt normalization,
date interpretation, UTF-8 conversion, and `QObject::tr` diagnostic mapping.
App-less and repository tests preserve exact existing messages, allow
cross-language GS Team key collisions while rejecting duplicates within each
namespace, and verify a rejected plan leaves imported rows and the latest
source date unchanged.

Executor and independent fresh archive Tester passed
`ClassMngrNextApplicationTeacherImportPlanValidationTests` and
`ClassMngrTeacherImportTests` (2/2). The Tester used archive `8e2ee2d6` plus
only the six F85 paths. Windows x64 Debug verification used CMake 4.4.2,
Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; configuration validated 923
handwritten source owners and the build completed 311 actions. `git diff
--check` passed. No full suite was run.

### Cumulative exit-gate status after F85

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Teacher Import update policies; F84 types Schedule Import matching keys; F85 adds Qt-free Teacher Import plan validation. Broader behavior remains incomplete. |
| Baseline parity | Partial | F82 and F83 compare legacy/current behavior on checked inputs and identical seeds, but both fixtures postdate the legacy baseline. They add common-input differential regression, not historical-output parity. F85 adds no parity claim; broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F85 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F85's policy remains Qt-free. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress with its
exit gate Open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F86 adds a baseline-era generated
Schedule Import differential using `scheduleWorkbookData()` from baseline
`48fc5c5c`. Run identical generated input bytes and a seeded
database through legacy and current parser, preview,
and apply paths, then pin a semantic transcript as literals: parse metadata,
matches and suggestions, apply counters, and normalized persisted schedule
state. Label this source-generated synthetic baseline comparison, not
historical production-data parity. Verify with
`ClassMngrScheduleImportTests`; no full suite. This is selected work, not
implementation evidence. Sub Prep remains capped at 2026-2027.

## Verified F86 Schedule Import generated-baseline differential - commit c3029f14

`tests/schedule_import_tests.cpp` adds
`previewsAndAppliesBaselineGeneratedWorkbookAgainstSeededDatabase`. The test
pins `scheduleWorkbookData()` at 5,352 bytes with SHA-256
`24cf273fe36278747c811ad04bb5cbf4a32a2970f50fc6abed394b5809d48b49`. The
helper existed at baseline `48fc5c5c`. Legacy and current paths received
identical generated bytes and deterministic database seeds; their semantic
transcript SHA-256 matched:
`8A00E7A05FBA1E86980EF324A946D9BCAD81916B8B5833856537FD94F0181DA8`.

The transcript covers parsing Alice’s E5 Zeus candidate from B2,D2, preview
matching teacher 17 with no class match, and apply creating class 41 with
`ignoredCells=1` and `schedulesCleared=1`. Unrelated teacher, class,
class_info, and settings state remains. The Normal full-snapshot import clears
class 40’s seeded Friday time and creates Monday/Wednesday rows for the new
class. This is a source-generated synthetic baseline comparison, not
historical production-workbook parity.

Executor and independent fresh-base Tester passed
`ClassMngrScheduleImportTests` (1/1). The Tester used CMake 4.4.2, Ninja
1.13.2, MSVC 19.51.36257, and Qt 6.12.0; fresh configuration validated 923
handwritten source owners, the build completed 309 steps, and CTest passed in
1.12 seconds. `git diff --check` passed. No full suite was run.

### Cumulative exit-gate status after F86

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Teacher Import update policies; F84 types Schedule Import matching keys; F85 adds Qt-free Teacher Import plan validation. F86 changes tests only; broader behavior remains incomplete. |
| Baseline parity | Partial | F82 and F83 compare checked inputs added after baseline and provide common-input differential regression. F86 compares identical input generated by a helper present at baseline, with the same seed and matching semantic transcript; it is synthetic source-generated evidence, not historical production-workbook parity. Broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F86 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F86 changes tests only. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited
`src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F87 extracts the GS Team
matched-record sparse-merge/no-op policy into a Qt-free Application contract,
following the Korean and Native English teacher update policies. Keep
repository matching, the Korean-name-else-English match choice, and
ambiguous-match rejection in the adapter. Preserve retained identity,
blank-field preservation and normalization, changed/no-op counts, and zero
UPDATE on no-op. Verify with the app-less policy target and
`ClassMngrTeacherImportTests`; no full suite. This is selected work, not
implementation evidence. Sub Prep remains capped at 2026-2027.

## Verified F87 GS Team Teacher Import sparse-merge policy - commit b97f81be

The Qt-free Application contract adds `GsTeamImportProfile`,
`GsTeamImportFields`, `GsTeamImportUpdate`, and `mergeGsTeamImport` in
`src/next/application/gs_team_import_update.h`. It preserves the matched ID,
retains stored fields when incoming values are blank, and reports whether any
field changed. `TeacherImportRepository` retains match-key selection,
ambiguity rejection, Qt `QString` normalization, SQL, counters, diagnostics,
and transaction handling; unchanged matched rows skip UPDATE.

App-less tests cover mixed sparse updates, blank-field preservation, stable
identity, and unchanged results. Repository tests cover Korean matching,
English fallback, create/update/unchanged counts, persisted values and IDs,
zero UPDATEs for unchanged rows, and the existing ambiguity diagnostic. CMake
registration is in `cmake/next.cmake` and `cmake/tests/next.cmake`.

Executor and independent fresh-base Tester passed
`ClassMngrNextApplicationGsTeamImportUpdateTests` and
`ClassMngrTeacherImportTests` (2/2). The Tester used archive `81f1c625` plus
only the six F87 paths, excluding the protected manifest. CMake 4.4.2, Ninja
1.13.2, MSVC 19.51.36257, and Qt 6.12.0 configured; fresh configuration
validated 925 handwritten source owners, both executables built, and CTest
passed 2/2. `git diff --check` passed. Nonfatal warnings reported missing
`vswhere.exe`, optional Vulkan headers, and long object paths. No full suite
was run.

### Cumulative exit-gate status after F87

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Teacher Import update policies; F84 types Schedule Import matching keys; F85 adds Qt-free Teacher Import plan validation; F87 adds the Qt-free GS Team sparse-merge/no-op policy. Broader behavior remains incomplete. |
| Baseline parity | Partial | F82 and F83 compare checked inputs added after baseline and provide common-input differential regression. F86 compares identical input generated by a helper present at baseline, with the same seed and matching semantic transcript; it is synthetic source-generated evidence, not historical production-workbook parity. Broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F87 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F87's policy remains Qt-free. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited
`src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F88 adds an intensive Schedule
Import source-generated baseline comparison using only baseline-era
`singleSheetWorkbookData()` and the inline intensive worksheet data present at
baseline `48fc5c5c`. Do not use the checked-in
`schedule_intensive_synthetic_worksheet.xml` as historical baseline evidence;
it postdates the baseline. Compare identical generated bytes and a
deterministic database seed against legacy and current paths. Pin parse,
preview, and apply counters; intensive slot and schedule state; and preservation
of regular hours and unrelated classes. Label the result as source-generated
synthetic baseline evidence, not historical production-workbook parity. Verify
with `ClassMngrScheduleImportTests`; no full suite. This is selected work, not
implementation evidence. Sub Prep remains capped at 2026-2027.

## Verified F88 Schedule Import intensive generated-baseline differential - commit 4442726b

`tests/schedule_import_tests.cpp` adds an intensive Schedule Import baseline
comparison using `singleSheetWorkbookData()` and inline worksheet data that
are identical to baseline `48fc5c5c`. The source-generated workbook is 2,359
bytes with SHA-256
`228fc2ce924f2fd4ee340500c92178386868bc83231b076b74e081dd628b93b4`.
The legacy snapshot parser, repository, model, rules, schema manager, helper,
and worksheet were verified against baseline. Legacy and current paths used
the same pinned bytes and seeded database.

Both parsed and persisted intensive-slot transcripts contain 65 rows (62
empty, 2 essay, 1 lunch) and match at SHA-256
`7fdba13a48788441556050e17b6d0b5ca52b2b9df5a6d0c357815a6458f4a5cc`. The
test pins parse and preview matching/suggestion behavior, apply counters,
intensive schedule state, and preservation of regular hours and unrelated
class, teacher, and settings data. This is source-generated synthetic
baseline evidence, not historical production-workbook parity.

Executor and independent fresh-base Tester passed
`ClassMngrScheduleImportTests` (1/1). The focused Windows x64 Debug build used
CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36231, and Qt 6.12.0. The independent
fresh-base build used MSVC 19.51.36257, validated 925 handwritten source
owners, and also passed CTest 1/1. `git diff --check` passed. Nonfatal
warnings included missing `vswhere.exe`, optional Vulkan headers, zlib
fallback, and long object paths. No full suite was run.

### Cumulative exit-gate status after F88

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Teacher Import update policies; F84 types Schedule Import matching keys; F85 adds Qt-free Teacher Import plan validation; F87 adds the Qt-free GS Team sparse-merge/no-op policy. F88 changes tests only; broader behavior remains incomplete. |
| Baseline parity | Partial | F82 and F83 compare checked inputs added after baseline and provide common-input differential regression. F86 and F88 compare identical inputs generated by helpers/data present at baseline, with deterministic seeds and matching semantic transcripts. These are source-generated synthetic comparisons, not historical production-workbook parity. Broader parity remains incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F88 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F88 changes tests only. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited
`src/next` dependency isolation remain Satisfied. Phase 2 remains In Progress
with its exit gate Open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F89 adds Qt-free
`teacher_import_match_cardinality.h` to classify match counts as zero, one, or
multiple. Integrate it into all three Teacher Import loops and its app-less
target/tests. Keep `QString` normalization, matching scan/order, GS Team
Korean preference, exact localized errors and rejection, SQL, counters,
transactions, and rollback in the repository. Add repository coverage for
Korean-key ambiguity, which current tests do not explicitly cover; preserve
existing Native English and GS Team ambiguity coverage. Verify the new
app-less classifier target and `ClassMngrTeacherImportTests`. This is selected
work, not implementation evidence. Sub Prep remains capped at 2026-2027.

## Verified F89 Teacher Import match-cardinality policy - commit 98968408

The Qt-free `TeacherImportMatchCardinality` policy classifies candidate counts
as zero, one, or multiple and is used by the Korean, Native English, and GS
Team import loops. The repository retains normalization, candidate scan and
ordering, GS Team key preference, identity selection, localized diagnostics
and rejection, SQL, counters, and transaction behavior. App-less tests cover
counts 0, 1, 2, and 9. Repository tests pin Korean, Native English, and GS Team
ambiguity diagnostics and rejection without writes, including the exact Native
English message `More than one stored Native English Teacher matches JAMIE.`

Executor and independent fresh-archive verification passed both focused
application and `ClassMngrTeacherImportTests` targets (2/2). The independent
Windows x64 Debug build used archive `36b300d2` with only the six F89 paths
overlaid, including final `tests/teacher_import_tests.cpp` blob
`3ae707417e22f9c16dbda48192dbb3fdd398a806` (SHA-256
`9DDA9F8283CCDB3BB865E66E085E0937F53664D41B5175D2DF9CF1B74CB240AD`). CMake
4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0 configured; CMake
validated 927 handwritten source owners, both targets built, and CTest passed
2/2. `git diff --check` passed. Nonfatal warnings reported missing
`vswhere.exe`, optional Vulkan headers, and unrelated long paths. No full suite
was run.

### Cumulative exit-gate status after F89

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior | Partial | F79 adds validated Class Transfer intervals; F80 and F81 add Qt-free Teacher Import update policies; F84 types Schedule Import matching keys; F85 adds Qt-free Teacher Import plan validation; F87 adds the GS Team sparse-merge/no-op policy; F89 adds match-cardinality classification and adapter evidence. Broader behavior remains incomplete. |
| Baseline parity | Partial | F82 and F83 compare post-baseline checked inputs; F86 and F88 compare baseline-era source-generated inputs. F89 adds no parity claim; historical production-workbook parity and broader coverage remain incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F89 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F89's policy remains Qt-free. |

Gate 1 and Gate 2 remain Partial; workspace boundary and audited `src/next`
dependency isolation remain Satisfied. Phase 2 remains In Progress with its
exit gate Open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F90 builds a Qt-free Teacher Import
apply use case for an already parsed and reviewed plan. Compose review
resolution, plan validation, match cardinality, and the three existing update
policies behind explicit Qt-free request, result, error, and atomic-persistence
ports. Keep workbook parsing and dialogs, Qt normalization, SQL schema,
localization, and transaction implementation at the adapter edge; preserve
atomicity across the Korean, Native English, and GS Team namespaces and the
latest-source-date setting. Add an app-less fake-port target and retain or
extend repository and dialog tests as appropriate. Do not claim baseline parity
without separate evidence. This is selected work, not implementation evidence.
Sub Prep remains capped at 2026-2027.

## Verified F90 Teacher Import apply use case - commit `df8ef0cb`

Commit `df8ef0cb Phase2 - Add Qt-free Teacher Import apply use case (F90)`
adds a Qt-free `TeacherImportUseCase` that owns review resolution, full-plan
validation, match cardinality, Korean/Native English/GS Team match-update-
create-no-op decisions and counts, latest-source-date advancement, and
transaction decisions. One persistence port binds reads, writes, date-setting,
commit, and rollback to the same transaction. The repository retains Qt
normalization, SQL, localized diagnostics, and transaction implementation.

Fake-port app-less coverage is paired with repository failure injection for a
latest-date write after three namespace writes, proving transaction rollback.
Coverage also pins padded-label handling, raw Native English and GS Team
diagnostics, and a secondary rollback warning that preserves the primary
diagnostic. Executor focused CTest passed 2/2. An independent fresh-base Tester
used `ca72c47e` plus only the six F90 paths and preserved protected manifest
blob `bf3afbe30c77e30be83df98434eda3ee466084b0`. Windows x64 Debug used CMake
4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0; 929 source owners were
validated, focused targets built, and CTest passed 3/3 (`ClassMngrTeacherImportTests`
and the F89/F90 app-less targets). No full suite was run.

### Cumulative exit-gate status after F90

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F79, F80, F81, F84, F85, F87, and F89 add bounded contracts; F90 adds the Qt-free apply use case and atomic persistence port. Broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F82/F83 compare checked inputs; F86/F88 compare baseline-era source-generated inputs. F90 adds no parity evidence; historical production-workbook parity and broader coverage remain incomplete. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator acceptance and focused app-less coverage remain satisfied; F90 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Post-F90 audits find audited `src/next` sources free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; the F90 use case is Qt-free. |

Both post-F90 Explorer audits leave Gate 1 and Gate 2 Partial, and find the
workspace boundary and audited `src/next` isolation satisfied. Phase 2 remains
In Progress with its exit gate Open. Broader calendar UI, generic settings
persistence, other feature-service migrations, and document-service migration
remain open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F91 adds a bounded source-generated
Teacher Import baseline differential using the existing
`TeacherImportTests::importsIntoSeparateTablesAndPreservesManualFields`
scenario, present at baseline `48fc5c5c` and current `df8ef0cb`. Hand-build a
`TeacherImportPlan` in memory and label the result source-generated synthetic
plan evidence, not workbook or historical-production-workbook evidence. Seed
deterministic in-memory SQLite with existing Native English Alex and manual
phone/nationality/email, Korean 홍길동 in room 413, Native English Alex as Team
Leader, GS Team 김하늘 as Branch Manager with phone/birthday, and source date
`2026-07-09`; existing latest date `2026-01-01` must not move backward.
Strengthen literal and normalized snapshots for all three tables and latest
date, preserve manual-field/update/count checks, and exclude generated IDs.
Compare current updated assertions against the legacy baseline using the same
test source if it compiles there (the current test calls the legacy public
`TeacherImportRepository` API); otherwise build a narrow compatible harness.
Do not overlay current repository on baseline. Focus `ClassMngrTeacherImportTests`
and report baseline/current outcome parity. This is one bounded Gate 2 scenario,
which remains Partial; Gate 1 remains Partial. Re-audit, then select F92. This
is selected work, not implementation or test evidence. Sub Prep remains capped
at 2026-2027.

## Verified F91 Teacher Import generated-plan baseline parity - commit `e814b4fc`

Commit `e814b4fc84d5c7e455a07688e096b077e3ed0f11` (`Phase2 - Pin Teacher
Import generated baseline parity (F91)`) changes only
`tests/teacher_import_tests.cpp`. The existing
`importsIntoSeparateTablesAndPreservesManualFields` scenario labels its
hand-built `TeacherImportPlan` as source-generated synthetic evidence. It pins
apply counts (Korean 1/0/0, Native English 0/1/0, GS Team 1/0/0), every
non-ID field of each single persisted table row, Alex's manual phone, birthday,
nationality, and email plus the updated Team Leader position, and that source
date `2026-01-01` does not move the latest date backward from `2026-07-09`.
Existing duplicate, no-write, ambiguity, and rollback cases remain.

Executor focused `ClassMngrTeacherImportTests` CTest passed 1/1. An independent
Tester built from current docs HEAD `17bbc8d4` plus only the final test file
(blob `786770ca816b40f00c7626461ed58ec0a3e66c22`, SHA-256
`8BB8206A59D27E09E1C82BE918445670B32220F2CB4FD8011041646CD2BA0720`); the
executable reported 20 passed, 0 failed, 1 skipped (the optional sample needs
`CLASSMNGR_TEACHER_IMPORT_SAMPLE`), and CTest passed 1/1. Its narrow common-input
harness passed on archived baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`
and the current F90 repository, with equivalent synthetic plan and SQLite
seed; exact JSON semantic outputs matched at SHA-256
`755A2B7DF52B3DD8F111ADF2EC0D0127415689B2B08A68D008505079D887B2B6`.
Current and legacy repository blobs were `7f2604023d1c7b8f8c3132acd1fcfe8e6ed2759a`
and `058aa4d0b85fb05d5cc04a2c6d0f72d99ba9cc71`. The full current test file
does not compile on baseline because `next/application/import_review_session.h`
is absent there, so the narrow harness used baseline production sources and
schema without overlaying current production code. Protected manifest blob
`bf3afbe30c77e30be83df98434eda3ee466084b0` was neither overlaid nor consumed.
Windows x64 used MSVC 19.51 and Qt 6.12.0. `git diff --check` passed; no full
suite was run. This is source-generated synthetic plan/repository evidence,
not workbook or historical production-workbook parity.

### Cumulative exit-gate status after F91

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F91 changes tests only and adds no app-less contract behavior; broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F91 advances parity by one baseline-present, source-generated synthetic plan/repository scenario. It does not cover workbook decode/preview or a historical production workbook; broader coverage remains incomplete. |
| Workspace boundary | Satisfied | The formal documented WorkspaceGateway/WorkspaceCoordinator acceptance remains satisfied; F91 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Independent audits find audited `src/next` sources free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F91 changes tests only. |

Gate 1 and Gate 2 remain Partial; the workspace boundary and audited
`src/next` isolation remain Satisfied. Phase 2 remains In Progress with its exit
gate Open. Broader calendar UI/contracts, generic settings persistence,
remaining feature-service migrations, document-service migration,
invalid-UTF-8 coverage, and live MainWindow projection-failure/retranslation
integration remain open. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F92 extends Teacher Import parity
upstream through baseline-present source-generated helper `testWorkbookData()`.
Run identical generated workbook bytes through validation/parser, explicit M1
review selection, plan creation, and `TeacherImportRepository::importTeachers`
on baseline `48fc5c5c` and current sources using equivalent deterministic
SQLite seeds. Pin the bytes/hash if stable, template/date/parsed Korean M1
candidate, apply counts, normalized persisted teacher row, latest source date,
and unrelated seeded state; exclude generated IDs. The helper creates
source-generated synthetic XLSX bytes with one Korean M1 candidate and source
date `2026-07-09`; this is synthetic workbook evidence, not historical
production-workbook parity. The checked-in `sectioned_review.xlsx` postdates
baseline. The current full test source needs a narrow legacy harness because
`next/application/import_review_session.h` is absent at baseline. Focus
`ClassMngrTeacherImportTests`; no full suite. Gate 2 advances but remains
Partial, and Gate 1 remains Partial. Calendar start-of-term classification is
a later Gate 1 candidate, not selected here. This is selected work, not
implementation or test evidence. Sub Prep remains capped at 2026-2027.

## Verified F92 Teacher Import generated-workbook baseline parity - commit `3f6ef73d`

Commit `3f6ef73d64c81b8e6e5f6ee665dc85e9b976408e` changes only
`tests/teacher_import_tests.cpp`. F92 pins baseline-present helper
`testWorkbookData()` output: a source-generated XLSX of 3,422 bytes with SHA-256
`9cdccb43d7fe5e5e1abb83630ede8b18e6dd2c4824dbb288dc81d60371496daa`. The test
validates and parses those bytes, explicitly selects the sole Korean M1
candidate, creates and applies an import plan, then checks the seeded results
and preserved manual and unrelated values. The baseline/current semantic
transcript SHA-256 matches at
`095d595311aaee2444d67a893d45a2d0f97fe366c2a667f90bdd690205a2bdb4`.

Fresh focused CTest passed 1/1. The test executable reported 21 passed, 0
failed, 1 optional external-sample skip; the selected scenario's 3 assertions
passed. This is source-generated synthetic workbook evidence, not parity with
a historical production workbook. Invalid-date validation parity and
repository rollback parity remained open.

### Cumulative exit-gate status after F92

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F92 changes tests only and adds no app-less contract behavior; broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F92 compares a valid baseline-present source-generated workbook flow through validation, parsing, review selection, plan creation, and apply. It does not establish historical production-workbook parity; invalid-date validation and repository rollback parity remain open. |
| Workspace boundary | Satisfied | The formal documented WorkspaceGateway/WorkspaceCoordinator acceptance remains satisfied; F92 changes no workspace behavior. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Two post-F92 audits find audited `src/next` sources free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F92 changes tests only. |

Gate 1 and Gate 2 remained Partial; the workspace boundary and audited
`src/next` isolation remained Satisfied. Phase 2 remained In Progress with its
exit gate Open. Broader calendar UI/contracts, generic settings persistence,
remaining feature-service migrations, document-service migration,
invalid-UTF-8 coverage, and live MainWindow projection-failure/retranslation
integration remained open. Sub Prep remained capped at 2026-2027.

Next selected bounded slice (2026-09-27): F93 extracts the duplicated
start-of-term calendar-event classification used by `CalendarEventModel` and
the `CalendarPage` upcoming-event filter into an app-less policy. Preserve the
current rule: simplify and lowercase the title; normalize event type with
unknown values mapped to `Other`; classify only `Other` events whose normalized
title is exactly one of `new semester`, `start of term`, `term start`, and
`term starts`; hide matches only when the hide preference is true. Acceptance:
one policy owns classification; app-less tests cover normalization, all four
exact titles, nonmatches, and both preference states, while both production
consumers suppress matches only when hiding is enabled. Use
`NextApplicationCalendarEventTests` in
`tests/next_application_calendar_event_tests.cpp`. This adds bounded Gate 1
evidence but leaves Gate 1 Partial and does not complete Phase 2. This is
selected work, not implementation or test evidence. Sub Prep remains capped at
2026-2027.

## Verified F93 Calendar start-of-term policy - commit `c73f1469`

Commit `c73f1469` (`Phase2 - Extract Calendar start-of-term application policy
(F93)`) changes seven paths. The new Qt-free `CalendarEventStartOfTermPolicy`
is registered in `cmake/next.cmake`; both `CalendarEventModel` and
`CalendarPage` consumers use it. The redundant legacy Domain helper is removed
and the Calendar Import assertion is migrated. App-less tests cover all four
aliases, title case and space simplification, known and unknown type fallback,
hybrid nonmatches, the hide switch, and U+0085/NEL whitespace for title and
type.

Independent fresh Windows x64 Debug verification from base `c999a235` plus
only the seven paths built `ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrCalendarEventCacheTests`, `ClassMngrCalendarImportTests`, and
`ClassMngr` (358 actions; MSVC 19.51.36257, CMake 4.4.2, Ninja 1.13.2, Qt
6.12.0). Focused CTest passed 3/3. A direct Qt 6.12 probe confirmed legacy and
policy agreement for NEL-separated `new semester` and trailing NEL after
`Vacation`. No full suite was run; `git diff --check` passed.

### Cumulative exit-gate status after F93

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F93 adds a Qt-free start-of-term policy and routes both Calendar consumers through it; broader Calendar UI and contract coverage remain incomplete. |
| Baseline parity (Gate 2) | Partial | F92 compares a baseline-present, source-generated valid workbook flow through validation, parsing, M1 review selection, plan creation, and apply. Historical production-workbook evidence remains missing; F94 is limited to invalid-date validation parity. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator criterion remains satisfied. Audits separately note a broader FileController integration gap outside that written criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Two independent post-F93 audits find audited `src/next` sources free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies. |

Gate 1 and Gate 2 remain Partial; the formal workspace criterion and audited
`src/next` isolation remain Satisfied. Phase 2 remains In Progress with its
exit gate Open. Open scope includes broader Calendar UI/contracts, generic
settings persistence, remaining feature-service migrations, document-service
migration, invalid-UTF-8 coverage, and live MainWindow projection-failure/
retranslation integration. Sub Prep remains capped at 2026-2027.

Next selected bounded slice (2026-09-27): F94 compares baseline-present
synthetic invalid-date Teacher Import workbook validation. Use identical bytes
from `testWorkbookData("invalid-date")` through each revision's own validator;
pin the byte hash if stable, `RecognizedButInvalid`, the discovered M1 section,
and the exact A1 diagnostic `Cell A1 must contain a version date such as
26.07.09ver.`. This is negative validation parity only: the current validator
makes no repository calls, and the UI disables import for an invalid result.
Do not claim repository snapshot/no-write or transactional rollback parity.
Focus current `ClassMngrTeacherImportTests`; because the current test file
references `next/application/import_review_session.h`, which is absent at
baseline, a narrow baseline harness may be needed. No full suite. Gate 2 remains
Partial and historical production-workbook evidence remains missing. This is
selected work, not implementation or test evidence. Sub Prep remains capped at
2026-2027.

## Verified F94 Teacher Import invalid-date validation parity - commit `f99d155f`

Commit `f99d155f636d273269d805531f7ef7db7be84bed` changes only
`tests/teacher_import_tests.cpp`. `rejectsGeneratedWorkbookWithInvalidDate`
pins `testWorkbookData("invalid-date")` at 3,423 bytes and SHA-256
`256b29c2f27bfe787007aaa6df28e5e084dc3788863cbf4b6a09fb265f0685d0`. It
asserts `RecognizedButInvalid`, template `sectioned-contact-list-v1`, an
invalid source date, section M1, and the exact A1 diagnostic `Cell A1 must
contain a version date such as 26.07.09ver.`

A narrow harness compiled each revision's own validator, registry, sectioned
template, and workbook reader. Baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`
and current sources returned identical semantic JSON at SHA-256
`8eee9375c47b3602b86e893092441f450c74860e2bb0f3edc53642a06322cbd9`; root
independently reran both archived executables and confirmed the match. The
focused current QTest passed 3/3 including setup and cleanup; filtered CTest
passed 1/1. `git diff --check` passed; no full suite was run. This is negative
validation parity only and establishes no repository no-write or rollback
parity.

### Cumulative exit-gate status after F94

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F93 adds a Qt-free start-of-term policy used by both Calendar consumers; F94 changes tests only. Broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F92 covers a baseline-present source-generated valid workbook flow; F94 adds same-byte invalid-date validation parity. Historical production-workbook evidence and broader parity remain open. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator criterion remains satisfied; the broader FileController integration gap is outside that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies; F94 changes tests only. |

Phase 2 remains In Progress with its exit gate Open. Open scope includes
historical production-workbook evidence, repository rollback parity, broader
Calendar UI/contracts, generic settings persistence, remaining feature-service
migrations, document-service migration, invalid-UTF-8 coverage, and live
MainWindow projection-failure/retranslation integration. Sub Prep remains capped at 2026-2027.

## Verified F95 Calendar repeat occurrence planner - commit `78659638`

Three independent Gate 1 feasibility lanes found no narrower seam and selected
Calendar repeat occurrence planning. Commit
`7865963815efa256b797b85af27b9a33107e8f70` adds the Qt-free
`CalendarEventRepeatOccurrencePlan`, registers it in `cmake/next.cmake`, and
routes Calendar series creation through the existing
`ApplicationServicesCalendarEventSeriesCreatePort`. It preserves daily and
weekly cadence, chained month-end clamping, inclusive until dates, fixed event
duration, copied fields, cleared occurrence IDs, and the 366-occurrence limit.
Malformed dates, ranges, and frequencies return structured errors. The legacy
Domain estimator remains separate.

App-less Calendar tests cover daily/weekly/monthly recurrence, January 31 to
February 28/29 to March 28/29, cutoff, duration, fields and IDs, invalid inputs,
and 366/367 boundaries. Independent verification built
`ClassMngrNextApplicationCalendarEventTests` and `ClassMngr`; focused CTest
passed 1/1 and `git diff --check` passed.

### Cumulative exit-gate status after F95

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F93 adds the Calendar start-of-term policy and F95 adds repeat occurrence planning; broader application behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F92 covers a baseline-present valid generated workbook flow and F94 adds invalid-date validation parity. F95 adds no parity evidence; historical production-workbook evidence and repository rollback parity remain open. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator criterion remains satisfied; the broader FileController integration gap remains outside that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies. |

Phase 2 remains In Progress with its exit gate Open. Broader Calendar UI and
contracts, generic settings persistence, remaining feature-service migrations,
document-service migration, invalid-UTF-8 coverage, and live MainWindow
projection-failure/retranslation integration remain open. Sub Prep remains
capped at 2026-2027.

## Verified F96 Teacher Import database rollback parity

F96 selection compared the fixed synthetic rollback scenario in current test
`rollsBackAllTeacherWritesWhenLatestDateSaveFails`; the baseline test lacks
this case. The plan contains one Korean teacher, one Native English teacher,
and one GS Team member, with failure injected during
`teacher_import/latest_source_date` insertion.

F96 compared baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` with
current `7865963815efa256b797b85af27b9a33107e8f70` using the same external
harness, which compiled each tree's own Teacher Import repository, schema
manager, transaction, and SQL utilities. Independent fresh Release/Ninja builds
and runs passed, and the archived source trees were verified against Git.

Both runs failed at `latest_date_write` and left `teachers`,
`native_english_teachers`, `gs_team`, and the latest-date setting empty. Their
normalized JSON outputs matched at SHA-256
`b53d70bf33fa0e20451a2582eb8e1a6f904e5907196c6fad4696543b95b230f3`. This is
synthetic database rollback evidence only; it does not establish workbook,
historical production-data, or other failure-stage parity.

### Cumulative exit-gate status after F96

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F95 adds Qt-free Calendar repeat occurrence planning; broader application behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F92 and F94 cover generated workbook paths; F96 adds one synthetic database rollback case. Historical production-workbook evidence and broader parity remain open. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator criterion remains satisfied; the broader FileController integration gap remains outside that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook evidence, other rollback failure stages, broader Calendar
UI/contracts, generic settings persistence, remaining feature-service
migrations, document-service migration, invalid-UTF-8 coverage, and live
MainWindow projection-failure/retranslation integration remain open. Sub Prep
remains capped at 2026-2027.

## Verified F97 Calendar repeat-series edit planner - commit `36ebb09a`

F97 was selected to move repeat-series suffix transformation from the platform
port into an app-less Application planner after the platform query. Commit
`36ebb09a960fa82f633701ec83fba35bcd7f3599` adds
`src/next/application/calendar_event_series_edit_plan.h`, registers it in
`cmake/next.cmake`, routes
`src/next/platform/application_services_calendar_event_series_edit_port.h`
through the planner, and adds app-less and platform tests. It preserves query
order and IDs, common start-date offset and requested duration, request fields,
trimmed series ID, empty input success, and Technical failures for invalid
source or shifted dates.

Independent fresh Ninja builds produced
`ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and
`ClassMngr`. Both focused CTests passed. An independent platform recheck passed
for empty suffix and no-save failure cases. `git diff --check` passed; no full
suite was run.

### Cumulative exit-gate status after F97

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F95 adds repeat occurrence planning and F97 adds repeat-series edit planning; broader application behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F92 and F94 compare source-generated workbook paths; F96 adds one synthetic database rollback case. Historical production-workbook evidence and broader parity remain open; F97 adds no parity evidence. |
| Workspace boundary | Satisfied | The formal WorkspaceGateway/WorkspaceCoordinator criterion remains satisfied; the broader FileController integration gap remains outside that criterion. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook evidence, other rollback failure stages, broader Calendar
UI/contracts, generic settings persistence, remaining feature-service
migrations, document-service migration, and live MainWindow
projection-failure/retranslation integration remain open. Sub Prep remains
capped at 2026-2027.

## Verified F98 malformed-UTF-8 Teacher Import workbook parity - commit `30d545a8`

Commit `30d545a8eb198042948d233e6e10110bf364ff27` changes only
`tests/teacher_import_tests.cpp`. The generated malformed-workbook input is
3,422 bytes with SHA-256
`7386d4eae0e7f8d54467b57f05ec909cd0c1e7f392d865f9dc7e52faf3cc8165`. The
input replaces the `M` in the second `<t>M1</t>` marker at
`xl/sharedStrings.xml` member offset 223 with raw `0xFF`; the rebuilt ZIP has
valid CRCs for all six entries.

A baseline/current harness ran identical bytes through each revision's own
reader, validator, registry, template, and name helper. Both returned
`UnsupportedTemplate` with empty metadata and records and zero counts. An
independent exact-case run reported 3 passed, 0 failed; focused CTest passed
1/1. This is source-generated malformed-workbook parity only; historical
production-workbook evidence remains open. Gate 2 advances but remains Partial;
Gate 1 remains Partial. The formal workspace criterion and audited v2 isolation
remain Satisfied. Phase 2 exit remains Open. Sub Prep remains capped at
2026-2027.

## Verified F99 Calendar repeat-series creation use case - commit `794ed7c0`

Two Explorer lanes confirmed the Calendar page owned plan/save composition.
Three Investigator lanes compared Gate 2 rollback, Schedule Import
meeting-pattern, and Calendar use-case candidates; Calendar was selected to
return to Gate 1 and reuse the existing occurrence planner and series-create
port.

Commit `794ed7c0a5de8252a1687a1be8e4612c856cf00e` adds a Qt-free Calendar
repeat-series creation use case, routes the page through it, and adds fake-port
tests. Qt dialog mapping, UUID generation, warnings, and cache invalidation
remain in the UI. The fake-port contract verifies successful planned-request
forwarding with one call, port-error propagation, and no call for an invalid
plan.

Independent fresh Debug Ninja/MSVC/Qt 6.12 verification built the Application
Calendar tests, Platform series-create tests, and `ClassMngr` (354 actions).
Focused CTest passed 2/2; `git diff --check` passed.

### Cumulative exit-gate status after F99

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F99 adds the repeat-series create use case; broader application behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F98 adds source-generated malformed-workbook parity; historical production-workbook evidence and broader parity remain open. F99 adds no parity evidence. |
| Workspace boundary | Satisfied | The formal workspace create criterion remains satisfied. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook evidence, broader Calendar UI/contracts, generic settings
persistence, remaining feature-service migrations, document-service migration,
and live MainWindow projection-failure/retranslation integration remain open.
Sub Prep remains capped at 2026-2027.

### Next selected bounded slice (F100)

Add seeded Schedule Import replacement rollback parity comparing baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` with current F99
`794ed7c0a5de8252a1687a1be8e4612c856cf00e`. Run the same fixed seed, plan, and
failure trigger through each revision's own repository, schema, and transaction
closure. Update teacher and `class_info`, clear the target's regular schedule
rows, insert an earlier replacement row, then fail a later `class_times` row
(for example, Wednesday).

For each run, require the injected failure and assert the post-failure state
matches its pre-operation snapshot for `teachers`, `classes`, `class_info`,
`class_times`, `class_intensive_times`, `intensive_slot_states`,
`app_settings`, and `sqlite_sequence`; compare normalized baseline/current
snapshots as well. Add or strengthen the current focused
`ClassMngrScheduleImportTests` case and use a narrow baseline/current harness.
Two independent explorers verified transaction order and found no blocker.
Gate 2 remains Partial; Gate 1 remains Partial. This is selected work, not
implementation or parity evidence. Phase 2 exit remains Open. Sub Prep remains
capped at 2026-2027.

## Verified F100 seeded Schedule Import replacement rollback parity - commit `2738216a`

Commit `2738216a026ffdb935c1327aff70c0f95cc2af6f` changes only
`tests/schedule_import_tests.cpp`. A fresh MSVC 19.51, Qt 6.12, Ninja build
passed; the exact selected Qt Test slot passed 3/3 and focused CTest passed
1/1.

Independent baseline/current harness builds compiled each revision's own
source closure (17/17 and 15/15 build actions). Baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current F99
`794ed7c0a5de8252a1687a1be8e4612c856cf00e` both reached the injected
`class_times.wednesday_insert.injected` failure after earlier schedule and
teacher/class-info writes. Each returned to its pre-operation snapshot,
including `sqlite_sequence` and unrelated seeded state. Their normalized
outputs match at SHA-256
`B9B52658A6E5CA61A0C0173351CBC966BE5DD9A4FBB1F1D9EE4E4162672CE062`.

### Cumulative exit-gate status after F100

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F99 adds the Calendar repeat-series create use case; broader application behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F100 adds seeded Schedule Import transaction rollback parity; historical production-workbook evidence and broader parity remain open. |
| Workspace boundary | Satisfied | The formal workspace create criterion remains satisfied. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook evidence, broader Calendar UI/contracts, generic settings
persistence, remaining feature-service migrations, document-service migration,
and live MainWindow projection-failure/retranslation integration remain open.
Sub Prep remains capped at 2026-2027.

### Selected next slice (F101)

Add a Qt-free `CalendarEventSaveUseCase` around
`CalendarEventSaveRequest`/`CalendarEventSavePort`, route normal save and
“this occurrence only” through it, preserve repeat-ID detachment, and leave
“this and following” on the series-edit path. Test exact forwarding, one port
call, result ID, port error, and invalid-request rejection without a call.
Pair the Gate 1 work with seeded create/update parity between baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and F100
`2738216a026ffdb935c1327aff70c0f95cc2af6f`, compiling each revision's own
repository/model/schema/transaction sources and comparing returned IDs and
normalized event state. No workbook provenance is implied.

## Verified F101 Calendar event save use case and repository parity - commit `6502a96e`

F101 added a Qt-free `CalendarEventSaveUseCase` around the existing
`CalendarEventSaveRequest` and `CalendarEventSavePort`, routed the normal-save
and one-occurrence Calendar UI branches through it, and retained repeat-series
detachment for the selected occurrence. The separate “this and following”
series-edit path remains unchanged. App-less fake-port tests cover request
forwarding, result ID, port failure, and invalid-request rejection without a
port call.

A fresh independent Ninja build compiled `ClassMngr` and three focused test
targets in 358/358 steps; focused CTest passed 3/3. Seeded baseline/F100
Calendar Event create/update runs compiled each revision's own repository
source closure and produced matching normalized snapshots at SHA-256
`F479B00CBD1F2DDFC5D960A6CA92CC3BC32527D22597A512ACB2FC0FF222E23D`.
This establishes seeded repository parity for those create/update cases only;
it does not establish historical production-workbook provenance.

### Cumulative exit-gate status after F101

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F101 adds the Calendar single-event save use case; broader application behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F101 adds seeded Calendar Event create/update parity; historical production-workbook evidence and broader parity remain open. |
| Workspace boundary | Satisfied | The formal workspace create criterion remains satisfied. |
| v2 dependency isolation | Satisfied in the audited v2 scope | Audited `src/next` sources remain free of direct DataService, MainWindow, PageManager, and widget-pointer dependencies. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook evidence, broader Calendar UI/contracts, generic settings
persistence, remaining feature-service migrations, document-service migration,
and live MainWindow projection-failure/retranslation integration remain open.
Sub Prep remains capped at 2026-2027.

### Selected next slice (F102)

Move repeat-series suffix-delete ID/date validation from the Qt adapter into a
Qt-free Application request/use case. Route only “This and following” through
it; preserve the current diagnostic, validation-before-service order, series
ID bytes, Qt date conversion, warning behavior, and success-only invalidation.
Keep ordinary deletion separate. Test request forwarding, one call, invalid
input with no port call, and port-error propagation. This validation-only slice
adds no new parity claim.

## Verified F102 Calendar suffix-delete validation - commit `ef418996`

F102 adds Qt-free request validation and a use case for repeat-series suffix
deletion. Only “This and following” routes through it; ordinary deletion and
delete-all remain separate. Existing diagnostics, ID bytes, date conversion,
service/error mappings, warning behavior, and success-only cache invalidation
are preserved.

An independent short-path archive based on `c08c0f93` plus the seven intended
F102 paths built 358/358 actions; focused repository, Application, and Platform
CTest passed 3/3. A max-boundary assertion rebuilt in four Ninja actions and
the Application CTest passed 1/1. Protected `cmake/sources.cmake` SHA-256
matched `5E798E0A643F499D8E8F6F3AD3732E1C87BF1301429257E6E431B32B75EBE3EC`.
`git diff --check` passed with line-ending warnings. No full suite or parity
claim was made.

### Cumulative exit-gate status after F102

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F102 adds repeat-series suffix-delete validation to Application; broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F102 adds no persistence parity; earlier seeded cases remain bounded evidence. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for audited direct `src/next` references | Strict transitive ApplicationServices-to-DataService read isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a separately
written exit criterion. Broader Calendar UI/contracts, generic settings
persistence, remaining feature-service migrations, document-service migration,
and live MainWindow projection-failure/retranslation integration remain open.
Sub Prep remains capped at 2026-2027.

### Selected next slice (F103)

Add a Qt-free `CalendarEventDeleteUseCase` around `CalendarEventDeletePort` and
route only ordinary single-event deletion through it. Keep the F102 suffix
delete and delete-all paths separate. Preserve exact typed-ID forwarding and
port-result propagation; leave legacy positive-integer parsing, its exact
diagnostic, and service/error/exception mapping in Platform. Add app-less
fake-port tests for exact ID, one call, success, and error propagation.

Pair it with seeded single-event repository deletion parity using baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current F102 `ef418996`, each
compiled from its own source closure. Assert target removal, unchanged sibling
and unrelated rows, and unchanged sequence. This is repository behavior, not
UI or historical-workbook parity. Focused targets are
`ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrCalendarEventRepositoryTests`, and `ClassMngr`; run focused CTest for
the three test targets. F103 should advance Gate 1 and Gate 2, though both
remain Partial. Sub Prep's calendar window is derived from its reference date: January 1
of that date's year through December 31 of the following year. 2026-2027 is an example, not a
fixed range.

## Verified F103 Calendar single-event deletion use case and repository parity - commit `61e3d797`

F103 commit `61e3d7973edf59e28d542fdc2f64a2ad1da94540`
(`Phase2 - route Calendar single-event delete through Application`) adds the Qt-free
`CalendarEventDeleteUseCase`, registers it, routes only ordinary single-event
delete through it, and adds app-less and repository coverage. The five paths
are `cmake/next.cmake`, `src/next/application/calendar_event_delete_use_case.h`,
`src/features/calendar/ui/calendar_page_events.cpp`,
`tests/next_application_calendar_event_tests.cpp`, and
`tests/calendar_event_repository_tests.cpp`.

An independent fresh Ninja/MSVC/Qt build compiled
`ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrCalendarEventRepositoryTests`, and `ClassMngr`; focused CTest passed
3/3. `git diff --check` passed; no full suite ran. The same seeded deletion fixture ran against
separate temporary source copies of baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and F102 `ef418996`; each compiled
its own repository source closure and passed the QtTest case. Both observations
matched: the target was absent, the same-series sibling and unrelated rows
were fully preserved, row count was 2, and `sqlite_sequence` remained 3. This
is seeded repository behavior only, with no UI or historical-workbook parity
claim.

### Cumulative exit-gate status after F103

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F103 adds the single-event Calendar delete use case; broader Application behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F103 adds seeded single-event repository deletion parity; broader parity remains open. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for audited direct `src/next` references | Strict transitive ApplicationServices-to-DataService read isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a separate literal
exit criterion. Broader Calendar UI/contracts, generic settings persistence,
remaining feature-service migrations, document-service migration, and live
MainWindow projection-failure/retranslation integration remain open.

### Selected next slice (F104)

Add an app-less `CalendarEventDeleteAllUseCase` around the existing
`CalendarEventDeleteAllPort`. Let Application own the availability query via
`isAvailable(port)`. Route `CalendarPreferencesPanel::resetCalendarEvents()`
through the use case for both its pre-confirm availability guard and its
post-confirm delete-all operation; return before prompting when unavailable.
Preserve confirmation and cancel
behavior, the failure warning, and on success the status-label update plus
`calendarPreferencesChanged(true)`. Keep Platform service/error mapping in
Platform. Add fake-port coverage for availability forwarding, exactly one
delete call, success, and structured-error propagation.

Pair Gate 1 with a seeded baseline/current repository
`deleteAllCalendarEvents` scenario, compiling each revision's own source
closure: baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current F103
`61e3d7973edf59e28d542fdc2f64a2ad1da94540`. Assert all seeded event rows are
removed, the final count is zero, and sequence behavior matches. This is
fixture-scoped repository parity only. Focused targets are
`ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrCalendarEventRepositoryTests`, and `ClassMngr`; run only the three
focused CTests and no full suite. F104 should advance Gate 1 and Gate 2, though
both remain Partial. Workspace create and direct `src/next` isolation remain
Satisfied; strict transitive ApplicationServices-to-DataService read isolation
remains unresolved. Phase 2 exit remains Open. Sub Prep's calendar window is
derived from its reference date: January 1 of that date's year through
December 31 of the following year. 2026-2027 is an example, not a fixed range.

## Verified F104 Calendar delete-all use case and repository parity - commit `6057bc9e`

F104 commit `6057bc9e` (`Phase2 - route Calendar reset through Application`)
adds the Qt-free `CalendarEventDeleteAllUseCase`, routes the Calendar
preferences availability guard and confirmed delete-all operation through it,
and adds app-less and seeded repository coverage. The UI retains confirmation,
cancel behavior, warning presentation, and success status/signal behavior;
Platform retains service and error mapping.

Independent fresh Ninja/MSVC/Qt verification built
`ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrCalendarEventRepositoryTests`, and `ClassMngr`; the three focused
CTests passed 3/3. A local focused rerun also passed 3/3. No full suite ran.
The seeded delete-all fixture passed against baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current F103
`61e3d7973edf59e28d542fdc2f64a2ad1da94540`, each using its own repository
source closure. Both removed all three seeded rows, returned count zero, and
preserved `sqlite_sequence` at 3. This is fixture-scoped repository parity,
not UI or historical-workbook parity.

### Cumulative exit-gate status after F104

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F104 adds the Calendar delete-all operation and availability use case; broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F104 adds seeded delete-all repository parity; validation, conflict, import-planning, and state-transition parity remains broader work. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for audited direct `src/next` references | Strict transitive ApplicationServices-to-DataService read isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a separate written
exit criterion. Broader Calendar UI/contracts, generic settings persistence,
remaining feature-service migrations, document-service migration, and live
MainWindow projection-failure/retranslation integration remain open. Sub Prep
is bounded to January 1 of the reference year through December 31 of the
following year; 2026-2027 is illustrative, not fixed.

## Verified F105 Calendar repeat-series edit use case - commit `d45fb405`

F105 commit `d45fb405` (`Phase2 - route repeat-series edit through
Application`) adds the Qt-free `CalendarEventSeriesEditUseCase`, validates the
typed request before invoking the port, and routes only the Calendar “This and
following” edit branch through Application. The Platform adapter retains its
direct-caller validation guard, service/query/save orchestration, and Qt
conversion. The UI retains warning presentation and success-only cache
invalidation.

A clean Ninja/MSVC 19.51/Qt 6.12 Debug configure validated 938 handwritten
source owners. The Application and Platform Calendar Event test targets and
`ClassMngr` built in 353 actions. The two focused CTests passed 2/2. App-less
tests pin exact request forwarding, one port call, validation rejection with
zero port calls, and structured port-error propagation. `git diff --check`
passed; no full suite ran. F105 adds no baseline parity claim.

### Cumulative exit-gate status after F105

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F105 adds request validation and execution through an app-less use case; broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F105 adds no parity evidence; broader validation, conflict, import-planning, and state-transition parity remains open. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for audited direct `src/next` references | Strict transitive ApplicationServices-to-DataService read isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a separate written
exit criterion. Sub Prep's interval remains January 1 of the reference year
through December 31 of the following year, at most; 2026-2027 is illustrative.

## Verified F106 Calendar repeat-series repository parity - commit `bcc2e85e`

F106 commit `bcc2e85e` adds a seeded repository fixture to
`tests/calendar_event_repository_tests.cpp`. It pins five complete rows before
the edit, loads the suffix from the selected occurrence, and verifies ordered
IDs for the selected and following occurrences. It applies deterministic
title, date, and time changes to those rows in one `saveCalendarEvents` batch,
then pins all five persisted rows again. The earlier occurrence, other series,
and unrelated event remain unchanged; selected/following IDs and order,
row count, and `sqlite_sequence` remain stable.

The current `ClassMngrCalendarEventRepositoryTests` target built in the
independent Ninja/MSVC 19.51/Qt 6.12 Debug tree and its focused CTest passed
1/1. A temporary baseline harness compiled the same fixture against the
repository, schema, transaction, and SQL helper sources archived from baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`; each archived source blob was
verified against Git. The baseline test source omitted only the later-added
`loadCalendarEventDateIntervalsInRange` test and used its matching MOC include;
the F106 fixture itself was unchanged. Baseline CTest passed 1/1. This is
repository query/update parity only; no UI or planner-transformation or
historical-workbook parity is claimed. `git diff --check` passed; no full suite
ran.

### Cumulative exit-gate status after F106

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F105 adds repeat-series request validation and execution through Application; broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F106 adds one repeat-series query/update persisted-state fixture; broader validation, conflict, import-planning, and state-transition parity remains open. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for audited direct `src/next` references | Strict transitive ApplicationServices-to-DataService read isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a separate written
exit criterion. Sub Prep remains bounded to January 1 of the reference year
through December 31 of the following year; 2026-2027 is illustrative. F107 is
selected to move Calendar event-summary lookup by ID behind an app-less query
contract and the existing UI event-activation path.

## Verified F107 Calendar event lookup through Application - commit `dbdcd721bd68bcaa30191bc93e7576de590fb650`

The Calendar activation path now uses the Qt-free
`CalendarEventByIdQueryUseCase` and typed `CalendarEventByIdQueryPort`. The
use case rejects blank, whitespace-only, and over-limit IDs without calling
the port; valid IDs are forwarded once and success or structured failure is
returned. `ApplicationServicesCalendarEventPort::loadEventById` converts the
typed ID to the legacy positive integer and retains existing service lookup
and error mapping. `CalendarPage::handleCalendarEventActivated` returns on
lookup failure before opening the dialog.
F107 changes no Calendar range-worker/display behavior or Sub Prep
implementation, and adds no parity claim.

App-less tests cover exact ID forwarding, success and structured failure,
invalid requests with zero calls, and acceptance at the exact length limit.
Fresh Ninja/MSVC VS 2026 x64 / Qt 6.12 Debug verification built
`ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and
`ClassMngr`. The two focused CTests passed 2/2; no full suite ran. The focused
tests do not assert the UI-level failure/dialog-closed behavior; Tester
verified the return-before-open path by source inspection. `git diff --check`
passed. Configure emitted unrelated object-path length warnings, but all
three requested targets built.

### Cumulative exit-gate status after F107

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F107 adds the Calendar event-by-ID query; broader behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F107 adds no parity evidence; broader repository and behavior parity remains open. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for audited direct `src/next` references | Strict transitive ApplicationServices-to-DataService read isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a separate written
exit criterion. F108 is selected for seeded repeat-series suffix-delete
repository state-transition parity: full rows before/after; preservation of
earlier same-series, overlapping other-series, and standalone rows; selected
and following removals; final count; and unchanged `sqlite_sequence`. The same
fixture will run against baseline and current revisions using each revision's
own repository source closure. This is repository parity only, with no UI or
historical-workbook claim; F108 advances Gate 2 while it remains Partial. Sub
Prep remains bounded to January 1 of the reference date's year through
December 31 of the following year at most; 2026-2027 is illustrative.

## Verified F108 Calendar repeat-series suffix-delete repository parity - commit `d99b226e19917f3c855a0c49fa7891c537c4415d`

F108 commit `d99b226e19917f3c855a0c49fa7891c537c4415d` (tree
`e69e6b09fdc46e5be34bcbe4140645b2763001d1`) changes only
`tests/calendar_event_repository_tests.cpp`. It verifies repeat-series suffix
deletion against baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` (tree
`03338fb728365d20502adddcab161b27a7933948`).

Fresh isolated Git archives ran the focused
`ClassMngrCalendarEventRepositoryTests` CTest successfully (1/1) on current.
The Debug, `BUILD_TESTING=ON` environment used CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51 x64, and Qt 6.12.0. The narrow harness compiled each revision's own
`calendar_event_repository.cpp`, `sql_query_utils.cpp`,
`database_transaction.cpp`, and headers against an identical seeded five-row
database and request. Normalized outputs were byte-identical, with SHA-256
`FEC97B681F9D52E7C623AA14E245F3D4BB70FB6315425148632C0E905CC76E08`.

Both revisions removed the selected and following series-1 rows and preserved
the earlier occurrence, other series, and standalone event in all fields. The
row count changed from five to three; `sqlite_sequence` remained five. This is
synthetic repository state-transition parity only. No full baseline suite ran;
no UI or historical-workbook parity is established.

### Cumulative exit-gate status after F108

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F107 adds the Calendar event-by-ID query; broader Application behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F108 adds synthetic repeat-series suffix-delete state-transition parity; broader parity remains open. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for audited direct `src/next` references | Strict transitive ApplicationServices-to-DataService read isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a literal exit
criterion. Broader Calendar UI/contracts, generic settings persistence,
remaining feature-service migrations, document-service migration, and live
MainWindow projection-failure/retranslation integration remain open. Sub Prep
is bounded to January 1 of the reference date's year through December 31 of
the following year, at most; 2026-2027 is illustrative.

### Next selected bounded slice (F109)

Add deterministic seeded repeat-series batch-creation parity in
`tests/calendar_event_repository_tests.cpp`. Use the same explicit occurrence
facts against baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` and current
HEAD, compiling each revision's own repository/schema/source closure. Compare
returned IDs in occurrence order, full persisted event rows and order across
all fields, and the repeat-series ID; preserve a seeded unrelated row and
compare the final count and `sqlite_sequence`. Label this synthetic repository
batch parity. Existing Application planner and Platform tests cover current
planning and adapter behavior; this slice establishes no UI or
historical-workbook evidence.

## Verified F109 Calendar repeat-series creation parity - commit `26d604fb`

Commit `26d604fb` (`Phase2 - add repeat-series creation parity fixture`) adds
146 lines only to `tests/calendar_event_repository_tests.cpp`. It compares
current production revision `d99b226e19917f3c855a0c49fa7891c537c4415d` with
baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`.

Tester archived the current production tree and overlaid only the changed test
file; the focused CTest passed 1/1 and the direct test passed. A separate
harness compiled each revision's own repository, schema manager, transaction,
SQL helpers, headers, and CalendarEvent model with the same seed and three
explicit events. Returned IDs `[2,3,4]`, all persisted columns, the unrelated
event, final count, and `sqlite_sequence` value 4 matched. Normalized output
SHA-256: `DB5EC63C24300360F3639131C501D9AC65A9867942D5A14CCCBA3BAAD7420E59`.
This establishes synthetic repository state-transition parity only; no UI or
historical-workbook parity is claimed.

### Cumulative exit-gate status after F109

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F109 adds no Application behavior; broader app-less behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F109 adds synthetic repeat-series creation state-transition parity; broader parity remains open. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for audited direct `src/next` references | Strict transitive ApplicationServices-to-DataService read isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a literal exit
criterion. Broader Calendar UI/contracts, generic settings persistence,
remaining feature-service migrations, document-service migration, and live
MainWindow projection-failure/retranslation integration remain open. Sub Prep
is bounded to January 1 of the reference date's year through December 31 of
the following year, at most; 2026-2027 is illustrative.

### Next selected bounded slice (F110)

Move per-event Calendar visibility composition into a Qt-free Application
predicate used by both `CalendarEventModel` and the upcoming-events filter.
Keep Qt text/campus normalization and preference/directory loading at the
feature boundary; the upcoming-events active-event-type filter remains before
the predicate. Preserve start-term hiding before show-all, and hide only
start-term aliases whose effective type is `Other`. Show-all bypasses only the
campus check. Missing current/known campus metadata or no recognized campus
token remains visible; a known non-current token hides, while a current token
allows the event, including mixed current/other tokens. Preserve event order;
`event.campusId` remains unused. Keep existing component policies and tests.
Add an app-less composed matrix in
`tests/next_application_calendar_event_tests.cpp`, register the Application
header in `cmake/next.cmake`, and build `CalendarEventModel` and `ClassMngr` to
verify both callers.

## Verified F110 Calendar event visibility policy - commit `2656ef9c`

Commit `2656ef9c` (`Phase2 - add Calendar event visibility policy`) changes
exactly five paths: new
`src/next/application/calendar_event_visibility_policy.h`,
`cmake/next.cmake`, `src/features/calendar/ui/calendar_event_model.cpp`,
`src/features/calendar/ui/calendar_page_upcoming_events.cpp`, and
`tests/next_application_calendar_event_tests.cpp`. An independent fresh
archive of base `d59f0be4` overlaid only those five files.

The build compiled `ClassMngrNextApplicationCalendarEventTests`,
`ClassMngrCalendarEventCacheTests`, and `ClassMngr`; focused CTest passed 2/2
on Windows x64 Debug with Ninja 1.13.2, MSVC 19.51.36257, Qt 6.12.0, and CMake
4.4.2. A Qt-free app-less 12-case composition matrix pins start-term hide
precedence (only aliases with effective type `Other` hide, and show-all does
not bypass that rule), lazy campus checks, show-all bypassing campus filtering
only, visibility with missing/unmatched metadata or no recognized campus
token, hiding non-current-only tokens, and allowing current tokens including
mixed current/other tokens. Both callers preserve input order; the upcoming-
events active-type gate remains first, and Qt title/campus normalization
remains at the feature boundary. The page translation unit compiled, but no
dedicated CalendarPage visibility CTest asserted runtime page output. No full
suite ran.

### Cumulative exit-gate status after F110

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F110 adds a composed Calendar visibility predicate; broader app-less behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F109 adds synthetic repeat-series creation state-transition parity; broader parity remains open. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for audited direct `src/next` references | Strict transitive ApplicationServices-to-DataService read isolation remains unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a literal exit
criterion. Broader Calendar UI/contracts, generic settings persistence,
remaining feature-service migrations, document-service migration, and live
MainWindow projection-failure/retranslation integration remain open. Sub Prep
is bounded to January 1 of the reference date's year through December 31 of
the following year, at most; 2026-2027 is illustrative.

### Next selected bounded slice (F111)

Make a non-null-session `SettingsService` authoritative for `load`, `save`, and
`saveAll`, with no DataService fallback, while preserving the legacy
DataService-only constructor and behavior. Cover a non-null closed session
alongside a separately open DataService (no fallback reads or writes), legacy
DataService-only reads/writes, normal open `ApplicationServices` session
preferences, and defaults/errors. Other feature-service fallback families
remain open. This is partial strict-isolation progress, not a global audit
pass.

## Verified F111 session-bound SettingsService reads - commit `3152ce36`

Commit `3152ce36` (`Phase2 - isolate session-bound settings reads`) changes
only `src/app/services/feature_services.cpp` and
`tests/data_service_lifecycle_tests.cpp`. An independent fresh archive of
base `5653bf8c032ed553fc60324444428135e5f1b8dc` overlaid only those two paths.

`ClassMngrDataServiceLifecycleTests` and
`ClassMngrNextPlatformApplicationServicesCurrentCampusPreferencesPortTests`
passed focused CTest 2/2 on Windows x64 Debug with CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51, and Qt 6.12. A non-null bound `SettingsService` session now fails
`load`, `save`, and `saveAll` without a repository instead of falling back to
DataService; the sessionless DataService-only path retains read/write behavior.
`loadOrDefault` still routes through `load`. The closed-session/separate-open-
DataService test confirms no content mutation, and the normal preferences
adapter test passed.

`SettingsService::isAvailable()` can still return true when the separate
DataService is open, even though bound-session operations fail; availability
does not establish read isolation. This slice covers SettingsService only;
Calendar, Teacher, Class, Schedule, Roster, and Sub Prep fallbacks remain. No
full suite or baseline parity run.

### Cumulative exit-gate status after F111

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | F110 adds the composed Calendar visibility predicate; broader app-less behavior remains incomplete. |
| Baseline parity (Gate 2) | Partial | F109 adds synthetic repeat-series creation state-transition parity; broader parity remains open. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Partial transitive progress | Direct audited `src/next` isolation remains Satisfied; F111 isolates SettingsService operations for non-null sessions, but other service fallbacks and the strict transitive edge remain unresolved. |

Phase 2 remains In Progress with its exit gate Open. Historical
production-workbook provenance remains a tracked risk, not a literal exit
criterion. Broader Calendar UI/contracts, CalendarService isolation,
remaining feature-service migrations, document-service migration, and live
MainWindow projection-failure/retranslation integration remain open. Sub Prep
is bounded to January 1 of the reference date's year through December 31 of
the following year, at most; 2026-2027 is illustrative.

### Next selected bounded slice (F112)

In `src/app/services/feature_services.cpp`, make these six `CalendarService`
content reads authoritative to a non-null session: `eventsForDate`,
`eventsInRange`, `eventDateIntervalsInRange`, `upcomingEvents`, `event`, and
`repeatSeriesFromDate`. If that session lacks the calendar repository, fail or
report unavailable without reading DataService. Preserve all six DataService-
only behaviors for sessionless legacy construction; leave writes and deletes
unchanged.

Add one lifecycle regression with a closed bound `DatabaseSession` and a
separately open, seeded DataService: none of the six dual-bound reads may
expose its event data, while legacy-only `CalendarService` reads still do.
Retain the focused normal ApplicationServices Calendar adapter CTest. This
slice does not close other service-family fallbacks or establish global
isolation. Broader Calendar UI/contracts remain open; F112 isolates only these
CalendarService reads.

## Verified F112 session-bound CalendarService reads - commit `677f2451`

F112 was verified against base `0e05a3d1`. In a short-path retry after C1083,
`ClassMngrDataServiceLifecycleTests` and
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests` passed
focused CTest 2/2.
The lifecycle regression confirms the six session-bound CalendarService reads
do not fall back to a separately open, seeded DataService, while sessionless
legacy reads remain. No full suite ran.

## Verified F113 Sub Prep output-read isolation - commit `e9ef19a0`

An independent archive of base `677f2451` overlaid only
`src/app/services/feature_services.cpp` and
`tests/data_service_lifecycle_tests.cpp`. The exact three CTests passed 3/3:
`ClassMngrDataServiceLifecycleTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and
`ClassMngrNextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests`.
Verification used Windows x64 Debug, Ninja, MSVC 19.51, and Qt 6.12. A
non-fatal `vswhere` warning was emitted. `git diff --check` passed. No full
suite ran.

For a non-null session, Sub Prep print-source and roster-output reads now fail
or report unavailable without falling back to DataService; sessionless
DataService-only behavior remains. Broader Calendar UI/contracts and other
feature-service migrations remain open. Historical production-workbook
provenance remains a tracked, non-gating risk. Sub Prep remains bounded to
January 1 of the reference date's year through December 31 of the following
year, at most; 2026-2027 is illustrative.

## Verified F114 Calendar live-mutation isolation - commit `8aee10a6a4704535e49d3c2621f03479b8b8580a`

The commit changes only `src/app/services/feature_services.cpp` and
`tests/data_service_lifecycle_tests.cpp`. Bound-session
`FeatureService::isAvailable()` now follows the non-null session; DataService-
only construction retains legacy availability. `CalendarService::saveEvents`,
`deleteEvent`, `deleteRepeatSeriesFromDate`, and `deleteAllEvents` fail closed
when a bound session is closed, without falling through to a separately open,
seeded DataService. Lifecycle tests assert unchanged legacy state immediately
after each rejected call and retain sessionless legacy operations. `saveEvent()`
is unchanged because the audit found no current `src/next` caller.

The initial independent verification found an assertion-granularity gap. After
per-operation state assertions were added, the same independent Tester passed
the exact two-file snapshot on base
`2fe914856851581efb637be44cd762b3dec7b1db`; SHA-256 was
`21E5048D50FD636BE954504444061E804F93D746668C1B9D272418793319719B` for
`feature_services.cpp` and
`F377B89DC93A880680CB301065BFC27512BF6E809DED9F9D609DDE7EE1007583` for the
lifecycle test. Fresh Windows x64 Debug verification with Ninja 1.13.2, MSVC
19.51.36257, and Qt 6.12.0 passed
`ClassMngrDataServiceLifecycleTests` and
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests` (2/2), and
`git diff --check` passed. The same snapshot also passed
`ClassMngrSharedPolicyTests` (1/1), including open-session `saveEvents`. No
full suite ran. Optional Vulkan headers were unavailable and non-blocking.

F114 resolves live Calendar mutation fallbacks and the service-availability
mismatch only; it does not establish global or strict object-graph DataService
isolation. Direct `src/next` sources remain free of DataService, MainWindow,
PageManager, and widget-pointer references, but strict transitive isolation is
unresolved. Gate 1 and Gate 2 remain Partial; workspace boundary remains
Satisfied. Historical production-workbook provenance remains a tracked risk,
not a literal exit criterion. Existing Schedule Import F82/F83 comparisons
remain common-fixture evidence and do not close Gate 2.

F114 supersedes the earlier TeacherService catalog-read selection: an
independent call-site audit found no current `src/next` caller for those reads.

## Verified F115 session-bound ClassNotes save isolation - commit `96c8b8a5812ceb8280fafd8fa3c9b6f99a8d409c`

The commit changes only `src/app/services/feature_services.cpp` and
`tests/data_service_lifecycle_tests.cpp`, based on
`0c3fdd61171ef3a7ee9a1cac1460b256f7b03313`. SHA-256 is
`E57108C8810E40D9FBFA92FA51FFE6AD2FD08D2CC9BD0FA67661BA745B3AA024` for
`feature_services.cpp` and
`EA9D5C745CEE98D59222B70EC4C58EBFE570F7F5274526E77A717ECED81AFA11` for the
lifecycle tests. `ClassService::saveClassNotes()` now fails closed when a
non-null bound session has no repository instead of using a separately open
DataService. Direct lifecycle assertions confirm both notes fields stay
unchanged on rejection; a DataService-only service still updates both fields.
F114's session-authoritative `isAvailable()` already rejects the active
ClassNotes adapter before it calls this method. F115 closes a latent direct
service fallback, not an observed live port leak.

Independent fresh-snapshot Windows x64 Debug verification used CMake 4.4.2,
Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. The exact registered CTests
`ClassMngrDataServiceLifecycleTests`,
`ClassMngrNextPlatformApplicationServicesClassNotesSavePortTests`, and
`ClassMngrNextFeatureClassNotesPageTests` passed 3/3 with `--no-tests=error`;
`git diff --check` passed. A preset configure first failed because its x64
platform setting is incompatible with Ninja; direct Ninja configure succeeded.
Optional Vulkan-header and `vswhere` notices were nonfatal. No full suite ran.

After F111-F115, selected active Settings, Calendar read/mutation, Sub Prep
output, and ClassNotes service paths have session-authoritative entry points.
This does not resolve the retained `DataService*` field or wider
`ApplicationServices` usage. Keep isolation findings distinct: the audited
direct `src/next` scan remains Satisfied; method-level runtime isolation applies
to the selected bound-session paths; the legacy compatibility edge and broader
service usage require re-audit; documented Workspace and document-catalog
routes remain outer-adapter boundaries. Strict transitive isolation remains
unresolved. Gate 1 and Gate 2 remain Partial; workspace boundary remains
Satisfied and Phase 2 exit remains Open.

Gate 2 remains Partial. Existing bounded comparisons include F82/F83
post-baseline Schedule Import inputs, F86/F88 baseline-era source-generated
Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure
cases, and F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and
Calendar repository transitions. These records do not cover all validation,
conflict, planning, or state-transition behavior. Historical
production-workbook provenance remains unverified and is a tracked non-gating
risk; the post-baseline Schedule Import fixtures do not establish it.

## Verified F116 Sub Prep roster-output port check removal - commit `6636cac1a26528089dbda49e1bc9f93ff1d6346a`

The commit changes only
`src/next/platform/application_services_sub_prep_roster_output_source_port.h`,
removing its `ApplicationServices::hasOpenDatabase()` call. Independent
fresh-snapshot verification overlaid exactly this file; SHA-256:
`C92559472DA7F1E6758BD74341CD4EBF3439E105A4A91361EAD276A9CAC39ABD`.
`ClassMngrDataServiceLifecycleTests` and
`ClassMngrNextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests`
passed 2/2 with `--no-tests=error`; `git diff --check` passed. The lifecycle
test proves a closed bound service rejects while a separate legacy DataService
remains open. The focused port test has no direct split-session injection
assertion. The Workspace adapter is unchanged.

Gate 1 remains Partial. Existing app-less coverage includes workspace lifecycle
contracts; Class Transfer schedule-candidate validation (F79); Teacher Import
sparse-update/matching/full-plan policies and apply (F80/F81/F85/F87/F89/F90);
Schedule Import match-key typing (F84); and Calendar start-of-term/repeat
planning, create/save/delete, lookup, and visibility policies/use cases
(F93/F95/F97/F99/F101-F105/F107/F110). Missing planned use cases include
teacher-profile edit, broader class, schedule, roster/evaluation editing,
backup/recovery, and legacy database import.

Gate 2 remains Partial. Existing bounded comparisons include F82/F83
post-baseline Schedule Import inputs, F86/F88 baseline-era source-generated
Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure
cases, and F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and
Calendar repository transitions. These do not cover every validation,
conflict, planning, or state-transition behavior. Historical production-
workbook provenance remains a tracked non-gating risk.

Direct `src/next` isolation and the formal workspace-create boundary remain
Satisfied. F111-F115 provide method-level isolation for selected bound-session
Settings, Calendar, Sub Prep, and ClassNotes paths; F116 removes the roster-
output port's `hasOpenDatabase()` call, but the focused port test lacks direct
split-session injection coverage. AppServices factories retain dual-bound
services, and Workspace itself reaches DataService. Strict transitive
isolation remains unresolved; keep this independent track open.

Phase 2 remains In Progress with its exit gate Open.

### Next selected slice (F117; pending bounded solution review)

Add an app-less teacher-profile edit use case at the `TeacherInfoPage`
application boundary after bounded solution review establishes the contract
and acceptance. Broader class, schedule, roster/evaluation editing,
backup/recovery, and legacy database import remain planned work.

### Independent open track: DataService isolation

Continue the separate isolation review of active `src/next` to
`ApplicationServices` calls, the retained dual-bound services, and Workspace's
DataService edge. The direct source scan and workspace-create criterion remain
Satisfied, but strict transitive isolation is unresolved.

## Verified F117 app-less Teacher profile edit contract - commit `3fd2b93f0fd87077aa59654265cda8b53b658ba9`

F117 adds app-less `TeacherId`, the 14-field `TeacherProfileFields` and
`TeacherProfile`, and `TeacherProfileEditUseCase`. Its injected validation
policy returns normalized fields and structured issues containing code, field,
warning/error severity, and bounded arguments. Invalid IDs short-circuit;
validation errors preserve issues and block writes, while warning-only results
may continue. Persistence receives normalized fields, then reloads and returns
the canonical saved profile. Update and reload failures are distinct; a reload
failure records that the write succeeded.

Independent verification used a clean archive of base
`47844dfc087d47da9426e0aa06d948a1ab2264a9` with exactly five overlays:
`cmake/next.cmake` (`30F5584639C7DBECD36409E655AF1F87A1176DAAFF130CD5DF511481DF9CE7EC`),
`cmake/tests/next.cmake` (`38ADAD66CC58C4E0F0F225E0DFE1377195E35789D5BD6CC6226617BC11EC4E55`),
`src/next/application/teacher_profile_edit_use_case.h`
(`1D073F3413A5E6B80C9A46595D432B630340F873326AF2EA76066E59704023FA`),
`src/next/domain/teacher_profile.h`
(`DC096F4BAF78227225DC54C49007AC991C0013E6CD711B2B864241B28977530D`), and
`tests/next_application_teacher_profile_edit_tests.cpp`
(`F90B8CD5ECBD5D22B27526C09F009FE71FCFE7CC0DFB3FBA6659EB3C726377D1`). The
forced target rebuild succeeded; CTest
`ClassMngrNextApplicationTeacherProfileEditTests` passed 1/1 with
`--no-tests=error`, and `git diff --check` passed. The new contracts have no
Qt or legacy production dependency; MSVC C4530 was non-blocking.

No production validation-policy adapter or `TeacherInfoPage` integration is
included. A future adapter must delegate to the existing `TeacherValidator`
and map normalized values and issues without duplicating validation semantics.
F117 advances Gate 1 app-less coverage, which remains Partial. Gate 2 remains
Partial, the workspace-create boundary remains Satisfied, and strict
transitive DataService isolation remains unresolved. Phase 2 remains In
Progress with its exit gate Open. Historical production-workbook provenance is
a tracked non-gating risk.

### Next selected slice (F118)

Compare baseline and current Class Transfer behavior on the same checked-in
`tests/fixtures/transfers/conflict_source.json`: normalized preview/review
results, exact conflict diagnostic, complete database snapshots, and zero
writes. This is common-input evidence using a post-baseline fixture, not
historical production-workbook parity. Gate 2 remains Partial until required
behavior is covered.

### Independent open track: DataService isolation

Continue auditing active `src/next` to `ApplicationServices` calls, dual-bound
services, and Workspace's DataService edge. F117 does not change isolation; the
direct source scan and workspace-create criterion remain Satisfied, while
strict transitive isolation remains unresolved.

## Verified F118 Class Transfer common-input conflict comparison - commit `b68eba6dd93c1eaa6473ec9494a4d9e7da9980ef`

F118 changes only `tests/class_transfer_tests.cpp` (SHA-256
`71FA243D9C641EA955A5B33201478C76BCEDFAE7A333D0AF7727AE8E14FFB1E8`). The
focused test uses checked-in fixture
`tests/fixtures/transfers/conflict_source.json` (SHA-256
`BED9CBEE84A7946F51029EFC4AA2B2850BDA9784757250FBF6CE90CAB7FB173`). It pins
seeded preview matches, normalized review choices and plan, the exact combined
regular/intensive schedule-collision diagnostic, unchanged snapshots of all
application tables including `sqlite_sequence`, and zero `total_changes` under
`query_only`.

Independent verification used a fresh current archive at
`5263aebf8222e16e3085498af851a7f0d3041818` with only the test overlay. Baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` retained the same fixture and
received only the F118 helper/includes/slot/case transplant. Both focused CTests
passed 1/1; exact error and persisted-state assertions matched. The toolchain
was CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. The temporary
baseline tree required three CMake minimum references to change from 6.11.1 to
6.12.0; baseline production code was unchanged. `git diff --check` passed. No
full suite ran.

This is common-input evidence on a checked-in post-baseline fixture, not
historical production-workbook parity. Gate 2 remains Partial. Gate 1 remains
Partial; the workspace-create boundary and audited direct `src/next` scan
remain Satisfied. Strict transitive isolation remains unresolved, and Phase 2
remains In Progress with its exit gate Open.

### Next selected slice (F119)

Move canonical `DatabaseSession` ownership to `ApplicationServices`, retain
`DataService` as a borrowing compatibility facade while preserving standalone
`DataService` ownership, and construct all seven feature-service factories
with the session only. Scope the implementation to `data_service.h/.cpp`,
`application_services.h/.cpp`, and lifecycle tests. This removes the
dual-bound factory edge, not Workspace's continued delegation through
`ApplicationServices` to `DataService` or the strict transitive isolation gate.

### Independent open track: DataService isolation

At selection, DataService owns the canonical session; ApplicationServices
creates it through DataService, and all seven feature-service factories pass
both session and DataService. Workspace operations also delegate through
ApplicationServices to DataService. F119 addresses ownership and the factory
edge only; Workspace's edge remains open.

## Verified F119 ApplicationServices session ownership and factory handoff - commit `80fbf034d96b7d04b9be19c61de20de2c44a2f9d`

F119 changes five files:

- `src/data/data_service.h` — SHA-256 `E6EAF02E21693E9B5B687F8E657FD6687D3DEBC85B81AB05693C6C4179BCCFA7`
- `src/data/data_service.cpp` — SHA-256 `9EA5BB3BCD4034D07F8A21A87747E81141D5D2A98A9875A1AA99FC4F4F80F8C3`
- `src/core/application_services.h` — SHA-256 `B085423CEE89A53D262DF0A7E595BF5C2357E29043F945F07C87DAE99B1883F3`
- `src/core/application_services.cpp` — SHA-256 `33CAFC7EC764E0BB9E97C223157AFA0BCA7E316B8DCE245C93EEA9AF647AAC8B`
- `tests/data_service_lifecycle_tests.cpp` — SHA-256 `E3DAA625158F103CE4E95D9215397C09F7F66483C4B38353D3AF3BF38B180137`

Canonical `DatabaseSession` ownership now resides in `ApplicationServices`.
`DataService` remains a borrowing compatibility facade while retaining its
standalone owning constructor, and all seven feature-service factories receive
the session only. Earlier latest-session handoff hashes for these paths were
incorrect; these exact-commit archive hashes replace them, with matching Git
blob IDs.

Independent verification used a fresh archive of `80fbf034`. `ClassMngr` and
six target executables built on Windows x64 Debug with Ninja, MSVC, and Qt 6.12
in 370 Ninja steps. CTest passed 6/6:
`ClassMngrDataServiceLifecycleTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`,
`ClassMngrDocumentCatalogTests`, `ClassMngrSubPrepPrintPdfTests`,
`ClassMngrNextPlatformApplicationServicesClassNotesSavePortTests`, and
`ClassMngrNextFeatureClassNotesPageTests`. `git diff --check` passed. Optional
missing WrapVulkanHeaders notices and long-path warnings applied only to 19
unselected test targets; none appeared for the six selected targets. No full
suite ran.

The Workspace adapter still reaches `DataService` through
`ApplicationServices` for open, close, open-state, path, save, save-as, and
export. Thus F119 removes the dual-bound factory edge but does not pass strict
transitive-isolation acceptance. Gate 1 and Gate 2 remain Partial; the formal
workspace-create boundary and audited direct `src/next` scan remain Satisfied.
F118 is common-input evidence on a checked-in post-baseline fixture, not
historical production-workbook parity. Phase 2 remains In Progress with its
exit gate Open.

### F120 solution review and selected implementation boundary

Three independent Investigator reviews agreed this should be one gate-closing
slice: partial routing would leave either the Workspace-to-DataService edge or
the borrowed facade unsafe. The reviews converged on session-backed operations
and facade safety, while differing on whether to add a dedicated Workspace
service/port. The selected seam retains the existing `ApplicationServices`
Workspace API and adapter/controller composition. Its seven operations will
route directly to `DatabaseSession` or a small DataService-independent
file-operation helper shared with `DataService`; facade repository access will
resolve live through its owned or borrowed session instead of cached raw
pointers. Workspace calls will not notify or refresh the facade.

The rationale is that `ApplicationServices` is allowed by the formal gate and
already supplies FileController's open-state/path reads. A new Workspace
service/port would add an unnecessary dependency without improving the
transitive boundary. Acceptance required a source audit confirming the seven
operations avoid `m_dataService` and `src/next` remains DataService-free;
preservation of path/error normalization, lifecycle postconditions, save,
save-as copy-and-reopen identity, and export behavior; borrowed-facade reads and
writes through open, replacement, failure, close, and reopen; and standalone
and sessionless compatibility. Build `ClassMngr` and run the lifecycle,
WorkspacePort, and FileController workspace targets with diff hygiene checks.

At the solution-review handoff, F120 implementation and independent
verification were pending. Phase 2 remained In Progress with its exit gate
Open; Gate 1 and Gate 2 remained Partial, the workspace-create boundary was
Satisfied, and strict transitive isolation remained unresolved.

## Verified F120 Workspace DataService isolation and same-file copy repair - commits `b1288b96166a3beaa5885555e3fe05ab83d59107` and `09201aa5282973044a83b1471c6c8f676a7cb716`

F120 made all seven `ApplicationServices` Workspace operations session-backed
and made `DataService` resolve repository access live through its owned or
borrowed session. The initial implementation's exact-commit fresh-archive
verification passed the focused build and three required CTests, then exposed a
same-file Windows path-alias data-loss edge in file-copy handling. The Executor
fixed that edge in `09201aa5`; the same independent Tester verified a fresh
archive of the repair. `ClassMngr` and all three F120 targets built, and these
CTest targets passed 3/3:
`ClassMngrDataServiceLifecycleTests`,
`ClassMngrNextPlatformApplicationServicesWorkspacePortTests`, and
`ClassMngrFileControllerWorkspaceLifecycleTests`. A case-variant probe
reported `operationSucceeded=1`, `sourceExists=1`, and `contentPreserved=1`.
The toolchain was CMake 4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0.
Missing optional Vulkan headers and one object-path warning affected an
unselected target. No full suite ran.

The exact-archive repair delta is:

- `src/data/database/database_file_operations.cpp` — SHA-256 `57B07272C59D4BD7B09B40D78EE1E69112497AB92DE6A2E38E2B5F4C9F9E0DF6`; Git blob `7f20ea653900f661e68e625ec8a9951b89e7c906`.
- `tests/data_service_lifecycle_tests.cpp` — SHA-256 `0A8674C31F015BF0AD10D58B827152FB427AA6E7EADA642F0EA93D268008F182`; Git blob `825e530379931eccb0efd68c32738484c9d2b0d0`.

Source audit confirms the seven Workspace operations route to
`DatabaseSession` or the DataService-independent file helper; they do not call
`m_dataService`, which remains only for facade construction/access. There are
no direct `DataService` references under `src/next`. The Workspace operation
edge and active `src/next` DataService isolation are now Satisfied; the formal
workspace-create boundary remains Satisfied. Gate 1 and Gate 2 remain Partial,
and Phase 2 remains In Progress with its exit gate Open.

### Next selected slice (F121)

Add a test-only successful Class Transfer replacement common-input comparison
using identical bytes from `tests/fixtures/transfers/success_source.json` in a
clean current tree and baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`.
Pin fixture SHA-256
`A40CB4079865EB5C48800208A3648360C08B0CEC2F3ED1E3FA383E91BD4050E8`. The
deterministic seeded test must compare normalized preview, review, and plan;
replacement identity/result; and persisted class details, schedule, roster,
and evaluation, including `sqlite_sequence` if supported. Run focused
`ClassMngrClassTransferTests` in both trees. This is post-baseline common-input
evidence, not historical production-workbook parity. At selection, the
Executor had begun the test change; its current/baseline verification is
recorded in the following entry. Gate 2 remained Partial.

## Verified F121 Class Transfer successful replacement common-input comparison - commit `dd8d22c0732dc3b85f9c531d7991c639524e6a52`

F121 changes only `tests/class_transfer_tests.cpp` (SHA-256
`9E02465B02721946362D90305C1A6968607939432561BEFE96561261BF8CA7B2`, Git blob
`9a58d3c35cef404c0d9f4a7542206ed49afc32c0`). Fixture
`tests/fixtures/transfers/success_source.json` is post-baseline (SHA-256
`A40CB4079865EB5C48800208A3648360C08B0CEC2F3ED1E3FA383E91BD4050E8`, Git blob
`74d7a69a99d8c0aa6424cba8ac5a14e3fcb7d3ab`).

Independent exact-commit fresh-archive verification passed
`successFixtureClassReplacementMatchesCommonInputState` on current and pinned
baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, with the same two-file
baseline overlay (test and fixture). Both runs produced persisted snapshot
SHA-256 `ae65cb0a14393a9da0c9a546320f233531e324a4ef0bbfeee7f1a8306701ee6b`,
including `sqlite_sequence`. Current build and
`ClassMngrClassTransferTests` CTest passed 1/1. Baseline build passed and the
direct parity function passed with setup/test/cleanup (3 QtTest cases). The
full baseline overlaid CTest target was attempted and failed four unrelated
tests: two expect behavior absent from the baseline and two require unrelated
fixtures outside the overlay; it did not pass as a full target. Current used Qt
6.12.0 and baseline Qt 6.11.1; both used CMake 4.4.2, Ninja 1.13.2, and MSVC
19.51.36257. Both focused runs had an unrelated missing Qt font-directory
warning.

F121 is post-baseline common-input evidence, not historical-workbook parity.
Gate 1 and Gate 2 remain Partial; F120 active-v2 DataService isolation and the
formal workspace-create boundary remain Satisfied. Phase 2 remains In Progress
with its exit gate Open.

### Next selected slice (F122)

Use current/baseline final-state parity for
`ScheduleImportTests::skippedExactMatchPreservesItsSchedule`. Seed at least two
schedule rows in insertion order different from day/time sort; compare their
`(day, start, end)` values ordered by `class_times.id` before and after Skip,
requiring the same relative order. Pin the full post-apply hash from
`persistedScheduleImportSnapshot(database, true)` on baseline, including IDs
and `sqlite_sequence`, then require that exact snapshot on current. Keep the
one-skipped/zero-cleared summary, schedule/profile-name assertions, and later
explicit name update.

Both baseline and current Normal-import flows copy times in ID order, recreate
the schedule table, and reinsert rows. Baseline re-materialized the same Monday
`class_id`, day, start/end, and profile-name values while `class_times.id` and
its sequence advanced 1→2. This is expected storage behavior; retain IDs
because ClassInfo and Sub Prep use them for ordering. Preserve relative meeting
order; do not claim pre/post storage identity or zero writes. Run the same seed
and plan in current and pinned
baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`. This is baseline-present,
hand-authored seeded evidence, not workbook parsing or historical-workbook
provenance. F122 implementation is underway and current/baseline verification
is pending at selection; results follow below. Gate 1 and Gate 2 remain Partial
and Phase 2 remains In Progress/Open.

## Verified F122 Schedule Import Skip final-state parity - commit `e3411e733d2ce402a03096e715150e45b4244dff`

F122 changes only `tests/schedule_import_tests.cpp` (+67/-9; Git blob
`eb422208842551eb37a4783f31982b82cab674b3`, extracted SHA-256
`9697009E56FE575EE0303F54413CCBA205B17EF716278E9E5C1158E7343BB066`). The
fresh archive tree ID is `ca903158a6e8ff2866987b4873e5be3f5e92f845`; TAR
SHA-256 `402DB5014595E72B84836AA131B08C482FC23551B6D3FC192C6AB6ED52895440`.

Independent fresh-archive current and pinned-baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` builds both passed, and the exact
case `ScheduleImportTests::skippedExactMatchPreservesItsSchedule` passed 3/3 on
each tree (3 passed, 0 failed). The baseline archive TAR SHA-256 is
`1870A9BDE4087B061CA3C35B6EB0804B10335222D12B0C98F095343D504F7FD7`; its
overlay used the same seed/test, the exact F122 helper/method, and only the
missing `QCryptographicHash`/`QSqlRecord` includes. Both final snapshots had
SHA-256 `08ad64ed3d853e52a1a686c1683d0d1fe8a289026e21d21081e09ee4840c5ebe`,
including raw `class_times.id` and `sqlite_sequence`. Tuesday-then-Monday order
was preserved; assertions retained one skipped row, zero schedules cleared,
and profile behavior.

The environment was Windows 11 x64, MSVC 19.51.36257, Ninja 1.13.2, current
Qt 6.12, and baseline Qt 6.11.1. Baseline reused its cleanly configured
archive dependencies; the `QSqlRecord` compile issue was resolved only in the
temporary baseline overlay, with no shared-checkout edits. No full suite ran.
`git diff --check` passed. F122 is hand-authored, baseline-present seeded
evidence, not historical-workbook provenance. Gate 1 and Gate 2 remain Partial;
workspace and active-v2 DataService isolation remain Satisfied; Phase 2 remains
In Progress/Open.

### Next candidate (F123; bounded solution review pending)

Consider integrating F117's app-less existing-teacher edit use case into
production `TeacherInfoPage`. The candidate preserves `TeacherValidator` as the
validation owner, warning/error mapping, session-backed update/reload, canonical
reload, and current autosave/header/signal behavior. Two Explorers agreed on
this candidate; the Investigator solution review was interrupted at the user's
stop request, so scope is not accepted and implementation has not begun.

## Latest verified progress (F125)

Source/test commit `42bdbbea7e1d2cbc2c9eeb8c0631fd22b335a17d` (`Phase2 -
integrate class notes save use case`) adds the app-less
`ClassNotesSaveUseCase` and routes `ClassNotesPage` through it. The use case
enforces the request's existing 10,000 UTF-16 code-unit limit before calling
the application port and returns port failures unchanged. The platform
adapter keeps its defensive validation and session-backed `ClassService` save.
Page trimming, manual warning, and dirty-state behavior remain intact.

The `windows-x64-debug` preset built `ClassMngr`, the application contract
test, the platform adapter test, and the page integration test. The three
F125 CTest targets passed 3/3; the combined F124/F125 focused set passed 6/6.
Coverage includes exact-limit acceptance, oversized no-write rejection,
failure propagation, trimmed page saves, persistence, warning/dirty handling,
and the page's oversized-input warning. `git diff --check` passed. No full
suite or baseline comparison ran.

Gate 1 and Gate 2 remain Partial. F125 completes the notes save action but does
not close the remaining class-detail, schedule, roster, evaluation,
backup/recovery, or legacy database-import gaps. F120 active-v2 DataService
isolation and the formal workspace-create boundary remain Satisfied. Phase 2
remains In Progress with its exit gate Open.

### Cumulative exit-gate status after F125

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | Coverage includes workspace lifecycle; Class Transfer schedule-candidate validation (F79); Teacher Import (F80/F81/F85/F87/F89/F90); Schedule Import match-key typing (F84); Calendar policies/use cases (F93/F95/F97/F99/F101-F105/F107/F110); teacher-profile editing (F117/F123); co-teacher assignment (F124); and class-notes save (F125). Broader class-detail/schedule/roster/evaluation editing, backup/recovery, and legacy database import remain planned. |
| Baseline parity (Gate 2) | Partial | Bounded records include F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions, F118 common-input Class Transfer conflict behavior, F121 common-input successful replacement state, and F122 hand-authored seeded Schedule Import Skip state parity. They do not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remains unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remains satisfied. |
| v2 dependency isolation | Satisfied for active `src/next` call paths | F120 removes the Workspace operation edge to `DataService`; the source audit confirms all seven operations route to `DatabaseSession` or the file helper. Legacy facade construction/access remains available. |

Phase 2 remains In Progress with its exit gate Open. Gate 1 and Gate 2 remain
Partial; historical production-workbook provenance remains a tracked risk, not
a literal exit criterion. Sub Prep remains bounded to January 1 of the
reference date's year through December 31 of the following year, at most;
2026-2027 is illustrative.

### Next candidate (F126; bounded solution review pending)

Review an app-less save boundary for the class details edited by
`ClassDetailsPage`. Preserve the existing `ClassInfoValidator` feedback and
normalization, regular and intensive schedule-conflict checks, untouched
teacher/notes/activity fields, session-backed `ClassService` persistence, and
the current warning, dirty-state, title, and `classInfoSaved` behavior. The
page currently assembles a Qt `ClassInfo` and calls `saveClassInfo` directly;
the application contract should carry explicit Qt-free class fields and
schedule values. Candidate scope and validation/persistence port split are
pending review; no F126 implementation has begun. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Superseding status note - F126 and F139 (2026-09-30)

The candidate status above is historical and is superseded by the verified
implementation record: commit
`ca4c1a9701bbeee7a1ce27789808311a1760ef68` already routes
`ClassDetailsPage` save through the Qt-free `ClassDetailsSaveUseCase` and
Platform adapter. F139, commit
`f1c70a166943b9daac74cdadfdc0a515cea89efa`, reuses that contract for
`ScheduleEditorDialog`. The Platform adapter still persists through
`ClassService`; page-level validation and schedule-conflict behavior have not
been migrated. See the [legacy application mapping](phase2-legacy-application-mapping.md)
for the current boundary. This correction does not change Gate 1 or Gate 2.

## Verified F134 ScheduleBuilder source query - commit `cbb15e32`

F134 adds a compact Qt-free schedule-source snapshot, an active-session
Platform adapter, and ScheduleBuilder/Widget integration. The adapter reads
through the existing ordered, batched repository query; this work does not
claim query-count or database optimization. Raw UTF-16 day/start/end values
and their order remain available to the existing parser.

Independent fresh Windows x64 Debug Ninja/MSVC verification built `ClassMngr`
and the focused application, platform, builder, and widget targets. The four
focused CTest suites passed 4/4, and `git diff --check` passed. The repaired
platform test distinguishes fixture insertion order from repository order and
asserts the exact intensive `endTime`. No full suite or baseline comparison
ran.

Preview behavior is unchanged. `buildUi()` sets `setCompactPreview(true)` and
`setMaximumVisibleRows(6)`; each triggers `loadSchedule()` before
`ScheduleImportReviewDialog::prepare` calls `refreshSchedule()` and installs
the preview model. With services available, those steps can perform three
pre-model schedule reads, all present in the F134 parent. After
`setPreviewModel()` runs, subsequent preview renders and refreshes bypass
source, slot-state, and testing-assignment reads.

Gate 1 and Gate 2 remain Partial. F120 active-v2 DataService isolation and the
formal workspace-create boundary remain Satisfied. Phase 2 remains In
Progress/Open.

## Verified F135 slot-state read - source/test commit `91806e5d`

F135 adds a Qt-free ordered raw UTF-16 slot-state query and a Platform adapter
that reads `IntensiveSlotStateRepository` directly from the active session,
without `ScheduleService` or `DataService` fallback. `ScheduleWidget::reloadSlotStates`
uses the read while preserving behavior: an unavailable service is silent and
retains current state; a read failure warns and retains current state; a
successful read replaces state, including clearing old state for an empty
result.

The documentation handoff for F135 is commit `68ff760e`.

Independent fresh Windows x64 Debug Ninja/MSVC verification built `ClassMngr`
and the application, Platform, and widget targets. The three focused CTests
passed 3/3, including the strengthened warning-prefix assertion. The explicit
`ClassMngrFeatures` to `ClassMngrNext::Platform` dependency exists in the root
`CMakeLists.txt`, and F135 test targets are registered. `git diff --check`
passed. No full suite or baseline comparison ran.

Gate 1 and Gate 2 remain Partial. The formal workspace-create boundary and
active-v2 DataService isolation remain Satisfied. Phase 2 remains In
Progress/Open.

## Verified F136 testing-assignment display read - commit `0295543a`

F136 adds a Qt-free ordered UTF-16 assignment snapshot, an active-session
Platform adapter, and one joined repository read for assignments and optional
special-class display fields. `ScheduleWidget::reloadTestingBlocks()` consumes
the new boundary without `ScheduleService`/`DataService` fallback or per-
assignment detail reads. It preserves plain/special rendering, unavailable
clearing, warning and prior-state retention on assignment-read failure,
missing-special warning and skip, blank/default class-info behavior, and
installed-preview bypass.

Independent fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 verification in
`build/f136-independent-verification` built `ClassMngr` and the Application,
Platform, and ScheduleWidget targets. The three focused CTests passed 3/3:
`ClassMngrNextApplicationScheduleTestingAssignmentReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesScheduleTestingAssignmentReadPortTests`,
and `ClassMngrScheduleWidgetTests`. The Platform test confirms one recorded
statement for reads of one and 41 assignments. Independent review found the
missing-special-class warning had been dropped; after restoring it at the UI
boundary, the fresh build and focused 3/3 rerun passed. `git diff --check`
passed. No full suite or baseline comparison ran.

Gate 1 and Gate 2 remain Partial. The formal workspace-create boundary and
active-v2 DataService isolation remain Satisfied. Phase 2 remains In
Progress/Open.

### Cumulative exit-gate status after F136 (historical snapshot)

| Exit-gate area | Audit status | Finding |
| --- | --- | --- |
| App-less Domain/Application behavior (Gate 1) | Partial | Coverage includes workspace lifecycle; Class Transfer schedule-candidate validation (F79); Teacher Import (F80/F81/F85/F87/F89/F90); Schedule Import match-key typing (F84); Calendar policies/use cases (F93/F95/F97/F99/F101-F105/F107/F110); teacher-profile editing (F117/F123); co-teacher assignment (F124); class-notes save (F125); Classes navigation snapshot (F133); ScheduleBuilder source snapshot (F134); slot-state read snapshot (F135); and testing-assignment display snapshot (F136). Broader class-detail/schedule/roster/evaluation editing, backup/recovery, and legacy database import remained planned. |
| Baseline parity (Gate 2) | Partial | Bounded records included F82/F83 post-baseline Schedule Import inputs, F86/F88 baseline-era generated Schedule Import inputs, F91/F92/F94/F96/F98 Teacher Import flows and failure cases, F100/F101/F103/F104/F106/F108/F109 Schedule Import rollback and Calendar repository transitions, F118 common-input Class Transfer conflict behavior, F121 common-input successful replacement state, and F122 hand-authored seeded Schedule Import Skip state parity. They did not cover all validation, conflict, planning, or state-transition behavior; historical production-workbook provenance remained unverified. |
| Workspace boundary | Satisfied | The formal workspace-create criterion remained satisfied. |
| v2 dependency isolation | Satisfied for active `src/next` call paths | F120 removed the Workspace operation edge to `DataService`; the source audit confirmed all seven operations route to `DatabaseSession` or the file helper. Legacy facade construction/access remained available. |

Phase 2 was In Progress with its exit gate Open. Sub Prep remained bounded to
January 1 of the reference date's year through December 31 of the following
year at most; 2026-2027 was illustrative.

### F137 candidate review interrupted (historical)

Candidate discovery began from F136 commit `0295543a` with two independent
Explorer lanes. The user requested that work stop before either report
arrived, and both lanes were interrupted. No F137 candidate was selected or
implemented at that time. F143 was selected later; its scope and acceptance
are recorded in the current Phase 2 plan.

## Verified F138 testing-class choice query - commit `cc15eced`

F138 adds the Qt-free `ScheduleTestingClassChoicesReadQuery` and typed choice
snapshot. Its Platform adapter reads `TestingClassRepository` through the
active `DatabaseSession`; `TestingAssignmentDialog` consumes the result instead
of reading choices from `ScheduleService`. Unavailable-session `NotFound`
remains silent, while other failures warn. The commit adds app-less,
Platform-adapter, and UI coverage.

## Verified F139 ScheduleEditor save through Application - commit `f1c70a16`

F139 routes `ScheduleEditorDialog::saveChanges()` through the existing
`ClassDetailsSaveUseCase` and Platform port. Optional regular/intensive time
fields distinguish a class-detail save that leaves schedules unchanged from
an explicit schedule update. Grade, level, books, and colors continue through
the established persistence boundary. Application, Platform, and dialog tests
were updated.

## Verified F140 ScheduleEditor class-info read - commit `7f26bcd4`

F140 adds a typed `ScheduleEditorClassInfoQuery` snapshot and active-session
Platform adapter. `ScheduleEditorDialog::loadData()` uses it to load grade,
level, books, class/font colors, teacher Korean name, and room instead of
reading `classInfo` through `ClassService`. The app-less, Platform, and dialog
test targets were extended.

## Verified F141 ScheduleWidget unavailable-source behavior - commit `912e62f6`

F141 removes the legacy `ClassService` availability precheck before the
ScheduleBuilder source query. An unavailable source result produces the
days-only model without a warning; other read failures still warn. Schedule
widget tests cover both unavailable and failure behavior.

## Verified F142 TestingClasses list read - commit `a53791f4`

F142 routes `TestingClassesPage` list loading through the typed choice query
introduced in F138 and removes its `ScheduleService::testingClasses()` read.
The page retains its ordering and action behavior, handles unavailable
sessions silently, and warns on other read failures. The Testing Classes page
tests cover the list boundary and its behavior.

## Verified F143 TestingClasses selected-detail read - commit `e2a3811cdba71b58ff2289f2c756d9bf12349bf5`

F143 adds the Qt-free `TestingClassDetailsReadQuery`, typed snapshot, and
handler. The Platform adapter reads one detail record through the active
`DatabaseSession`'s `TestingClassRepository::loadTestingClass()`;
`TestingClassesPage::loadClass()` no longer reads details through
`ScheduleService`. It preserves class ID, name, grade, level, room, teacher ID,
class/font colors, notes, roster loading, success, and warning behavior. An
unavailable session is silent `NotFound`; missing/read failures warn.
Nonpositive teacher IDs map to absence so the editor keeps its “None” row.

Independent fresh Windows x64 Debug Ninja/MSVC/Qt 6.12 verification in
`C:\Users\wfelt\AppData\Local\Temp\codex_f143_testing_class_details_20260929`
validated 1,021 handwritten source owners. The build succeeded for
`ClassMngrNextApplicationTestingClassDetailsReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesTestingClassDetailsReadPortTests`,
and `ClassMngrTestingClassesPageTests`. The exact focused CTest selection
passed 3/3 in 0.10s, 0.89s, and 1.64s. Coverage includes typed-ID/result
propagation; Platform field mapping, absent/zero teacher, unavailable/missing/
read-error behavior, and no fallback; and page fields, roster, success,
silence/warnings, zero-ID “None” behavior, and the F142 list regression. No
full suite or baseline comparison ran. Source/test commit:
`e2a3811cdba71b58ff2289f2c756d9bf12349bf5`.

F143 adds Gate 1 application evidence but no Gate 2 baseline-parity evidence.
Gate 1 and Gate 2 remain Partial; workspace boundary and active-v2 DataService
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate
Open.

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

### Progress update - 2026-09-29 (F144 accepted)

F144 routes `TestingClassesPage::populateTeachers()` through a Qt-free typed
Application query and an active-session Platform adapter backed by
`DatabaseSession::teacherRepository()->getAllTeachers()`. It preserves
repository order and typed IDs; the page trims labels/rooms, filters blank
Korean labels, restores selection by ID, retains None, stays silent for an
unavailable session, and preserves the existing failure warning. The partial
implementation is checkpointed at
`56c76f412b246230fcfe00c249b195dcc6ccd95f`; the final page-test update is
committed as `89fbbaa250ddf98fae2ab1d80385fb99164ac055`.

Each fresh Windows x64 Debug Ninja/MSVC 19.51/Qt 6.12 configure validated
1,025 handwritten source owners. The F144 Application query, Platform adapter,
and TestingClassesPage CTests passed 3/3 in
`build/phase2-f144-independent-ninja-x64-20260929`. The F142/F143 Application
and Platform regression CTests passed 4/4 in the separate fresh short-path
tree `build/p2-f142f143`. All seven focused CTest cases passed;
`git diff --check` passed. An earlier F142/F143 Platform build attempt failed
before test execution with MSVC C1083; the fresh short-path build passed
without reproducing it, and its cause is unknown. No full suite or full
`ClassMngr` application build ran. The adapter's defensive
null-teacherRepository branch has no direct test seam while an open session
exists; null services, unopened sessions, and closed sessions cover the
observable unavailable behavior.

F144 adds Gate 1 evidence and no Gate 2 baseline-parity evidence. Gate 1 and
Gate 2 remain Partial; workspace boundary and active-v2 DataService isolation
remain Satisfied.

### Progress update - 2026-09-30 (F145 accepted)

F145 adds the existing Testing Class details update through the Qt-free
`TestingClassDetailsUpdateUseCase` and an active-session Platform adapter to
`TestingClassRepository::updateTestingClass()`. Existing-page roster-first
behavior is preserved: when roster save succeeds but details update fails, the
roster remains persisted and clean. New-class creation, pending assignment,
and delete/cascade were outside F145's existing-class update boundary.

Production commit: `26a916df9994217ffd3f12f45148207e9cf5e0c2`;
acceptance-test commit: `ba1b7cdec15f6f163bb1620897fb4c2e2b3baccb`. Fresh
Windows x64 Debug Ninja/MSVC configure/build passed. Three F145 CTests and
seven F142-F144 regression CTests passed (10/10); source ownership validation
and `git diff --check` passed. Object-path length warnings occurred, but all
requested targets built. No full suite or full application build ran.

Gate 1 and Gate 2 remain Partial; workspace boundary and active-v2 DataService
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.

### Progress update - 2026-09-30 (F146 accepted)

F146 routes new Testing Class creation through the Qt-free
`TestingClassCreateUseCase` and active-session Platform adapter to
`TestingClassRepository::createTestingClass()`. It includes the optional
pending weekday/start-time assignment because the repository operation owns
the atomic class/details/room/assignment transaction. F145 remains the
existing-class details update slice; delete/cascade remains separate.

Production commit: `d7acb516cd9261e2199f742b46f90de48aa907d1`;
acceptance-test commit: `14d2d124a3d8b0548d241f1d2dcea136dbee55d9`. A fresh
Windows x64 Debug Ninja/MSVC configure/build with Qt 6.12 passed 14 focused
CTests covering F146, repository behavior, F145, and F142-F144. Rollback
assertions cover `classes`, `class_info`, `testing_classes`, and
`schedule_testing_blocks`. Fixture issues were corrected in test assets before
the final pass. `git diff --check` passed. No full suite or full application
build ran.

Gate 1 and Gate 2 remain Partial; workspace boundary and active-v2 DataService
isolation remain Satisfied. Phase 2 remains In Progress with its exit gate Open.

### Progress update - 2026-09-30 (F147 accepted)

F147 routes Testing Class deletion through the Qt-free
`TestingClassDeleteUseCase` and an active-session Platform adapter to
`TestingClassRepository::deleteTestingClass()`. The page's destructive
confirmation names the roster, notes, speaking evaluations, regular and
intensive class times, and every schedule assignment. After a successful
delete, the page clears the deleted editor and roster before rebuilding the
list and selecting a sibling.

Production commit: `b037b4216b71c55c7793df5f7bbbfc4a00690065`; page
transition fix: `315b3ff33b7e2ab42b43d52cd168ce21a92158c9`; acceptance-test
commit: `397376e439f4b5955c82948ab0c225aaf776d679`. A fresh Windows x64 Debug
Ninja/MSVC/Qt 6.12 configure/build passed all 17 focused CTests covering
repository behavior, the F145/F146/F147 page slices, and F142-F146 regressions.
Coverage includes the typed use case, active-session adapter and no fallback,
full cascade with sibling preservation, rollback on a final-delete trigger
failure, prompt disclosure, cancel/failure/success behavior, and a repaired
page regression asserting clean sibling load, zero create/update calls, and
exactly one signal. An initial page run exposed a crash when deleting a dirty
selected class and then selecting a sibling; the transition fix clears the
deleted editor and roster before list rebuild/reselection.

CMake reported unrelated object-path length warnings and a missing-vswhere
notice; configure and build succeeded. A duplicate intensive-time fixture key
was corrected before the final pass. `git diff --check` passed. No full 220-test
suite or full application build ran. Gate 1 and Gate 2 remain Partial; workspace
boundary and active-v2 DataService isolation remain Satisfied. Phase 2 remains
In Progress with its exit gate Open.

### Progress update - 2026-09-30 (F148 accepted; F149 selected)

F148 adds the live-page case
`successfulUiSaveMatchesSeededCommonInputState` in target
`ClassMngrClassDetailsPageSaveParityTests`. It saves the same seeded teacher,
class, and edits through `ClassDetailsPage` with real `ApplicationServices`,
then checks persisted edited and untouched values, exact regular/intensive
schedule order, one `classInfoSaved` signal with the class ID, and clean dirty
state. The test/CMake commit is
`6c7211d6427b6dbcddd4d109d9d09f9eeff14f28`.

Independent fresh Windows x64 builds used CMake 4.4.2, Ninja 1.13.2, MSVC
19.51.36257.0, and Qt 6.12.0. The focused CTest passed 1/1 on current code
(source archive `2956df18f3f0c0c53623ad0945b2d75d90d725f4`, 310 Ninja actions)
and 1/1 on pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` (313 actions). Both used identical
test source overlay SHA-256
`7D73E3F8425022BFA4006B9667D9D7E7CDA0B8BC0AA4CB0DD2ECCAFD15EF8F26` and
target registration. The baseline temporary overlay changed only three Qt
minimum versions from 6.11.1 to installed 6.12.0 and added the test
registration; production sources were not overlaid. Command:
`ctest --test-dir "<build>" -R "^ClassMngrClassDetailsPageSaveParityTests$" --output-on-failure --no-tests=error`.
Nonfatal CMake warnings concerned missing `vswhere`, optional Vulkan headers,
and unrelated object-path lengths. No full suite ran. This adds one common-input
successful-save comparison to Gate 2 only; it does not establish validation or
conflict parity. Gate 1 and Gate 2 remain Partial; the Phase 2 exit gate stays
Open.

### Progress update - 2026-09-30 (F149 accepted; F150 selected)

F149, commit `2e7d8866` (`Phase2 - route class details conflict checks through
typed query`), routes `ClassDetailsPage` regular/intensive pre-save conflict
lookups through a Qt-free typed query and Platform adapter to the active
`DatabaseSession`'s existing `ClassInfoRepository` operation. UI warning
rendering stays in the page; overlap calculation and ordering stay in
persistence; `ClassService` save-time guards remain. The same-display-name
warning edge case is covered by the page behavior test
`regularConflictShortCircuitsIntensiveAndKeepsSameNameWording`. Current-vs-
baseline parity compares regular/intensive conflicts with distinct conflicting
class names.

The independent Tester built a fresh short-path Windows x64 tree with CMake
4.4.2, Ninja 1.13.2, MSVC 19.51.36257, and Qt 6.12.0. The exact focused CTest
selection passed 4/4:
`^(ClassMngrNextApplicationClassDetailsScheduleConflictQueryTests|ClassMngrNextPlatformApplicationServicesClassDetailsScheduleConflictPortTests|ClassMngrClassDetailsSavePageTests|ClassMngrClassDetailsPageSaveParityTests)$`.
The first long-path attempt failed with MSVC C1083 at a 265-character path;
retrying at 233 characters succeeded, with no remaining C++ diagnostic.

`ClassMngrClassDetailsPageSaveParityTests` passed 1/1 on pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`. Current parity source SHA-256:
`FF7C5A35C33D5F38E290E4DCFCC943BE5A38DC6E1E98DC659788DCACBA98E74F`.
The baseline-only observer adaptation to
`setUserPromptServiceForTesting`/recording `IUserPromptService` has SHA-256
`41AA9B1F7C5D26DF50645FDD8D6FCC2A5EB17BF3E7D9350033DA5FFFBA5D3F41`; it
preserves input, title/body, no-write, dirty-state, and header assertions. The
baseline overlay also adds the parity source and target registration and bumps
only three Qt minimum versions. Production sources were not overlaid. F149
adds regular and intensive common-input conflict comparisons to Gate 2, plus
app-less query evidence to Gate 1; both gates remain Partial and Phase 2's exit
gate remains Open.

F150 was selected after three independent read-only reviews and main review;
implementation is starting. Move `ClassDetailsPage` pre-save validation and
normalization into a Qt-free typed Domain/Application policy and map structured
issues to `FormValidationBinder`. Preserve `ClassService` save-time validation,
F149 conflict order and warnings, exact duplicate-schedule behavior, and
validation order. Keep Qt-dependent book-catalog rules, raw malformed schedule
input, and hidden-field behavior intact without duplicating catalog rules.

### Progress update - 2026-09-30 (F150 accepted; F151 selected)

F150, commit `854f9849` (`Phase2 - move class details validation into typed
policy`), moves `ClassDetailsPage` pre-save validation and normalization into a
typed Qt-free policy and maps structured issues to `FormValidationBinder`.
`ClassService` save-time validation remains. The policy preserves F149 conflict
ordering/warnings, exact duplicate-schedule behavior and validation order,
Qt-dependent book-catalog rules, raw malformed schedule input, and hidden-field
behavior without duplicating catalog rules.

A fresh Windows x64 Debug configure with CMake 4.4.2, Ninja 1.13.2, MSVC
19.51.36257, and Qt 6.12.0 validated 1,047 handwritten source owners. Current
policy, page, parity, and shared-policy focused CTests passed 4/4. The pinned
baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity CTest passed 1/1.
Its overlay added only parity test source/registration and three Qt minimum
bumps from 6.11.1 to 6.12.0; no production source was overlaid. Parity covers
F148 successful save, F149 regular/intensive conflicts, F150 missing-level
invalid save, and hidden persisted notes/activity whitespace trimming on both
revisions. Page field/focus mapping cases are representative, not exhaustive.
No full suite or application build ran. Gate 1 and Gate 2 remain Partial; Phase
2 remains In Progress with its exit gate Open.

F151 was selected for live-page current/baseline validation parity covering
malformed regular and intensive schedule inputs, end-before-start, and
duplicate rows. Invalid cases must remain dirty, show no conflict warning,
save, or signal, and leave persisted data unchanged. Current page tests assert
no conflict query directly; the baseline parity uses a seeded conflict trap.
Compare duplicate membership and row-level feedback semantically; do not
compare cross-group issue order because legacy `QHash` order is unspecified. A
typed Application query for fresh hidden persisted teacher/notes/activity
fields follows F151 as a separate slice.

### Progress update - 2026-09-30 (F151 accepted; F152 selected)

F151, commit `f232301e`, adds live-page current/baseline validation parity for
malformed regular input, malformed intensive input, end-before-start, and two
duplicate groups with a unique row. Assertions cover issue mapping, retained
dirty state, no visible warning or save signal, and unchanged persisted data.
Current page tests assert zero conflict-query calls directly; baseline parity
uses a conflict trap. Duplicate membership and row feedback compare
semantically; cross-group issue order is excluded because legacy `QHash` order
is unspecified.

Fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC
19.51.36257, and Qt 6.12.0. Configure validated 1,047 handwritten source
owners, and the focused current CTests passed 4/4. The pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity CTest passed 1/1 with six
QtTest cases. Its overlay added only the parity test source/registration and
three Qt minimum bumps from 6.11.1 to 6.12.0; baseline production page source
matches the pinned blob. No full suite or application build ran. Gate 2
advances but remains Partial; Gate 1 remains Partial and Phase 2 remains In
Progress/Open.

F152 was selected for a fresh-at-save typed Application validation-context
query and active-session Platform adapter returning the raw signed teacher ID
and exact UTF-16 notes/activity. Do not reuse the display snapshot. Preserve
`-1` as the unassigned sentinel, zero as invalid, missing-row defaults, and
legacy read-failure fallback plus validation-to-conflict-to-save order. Keep
the save port and `ClassService` guard separate.

### Progress update - 2026-09-30 (F152 accepted; F153 selected)

F152, commit `f70e3e23`, adds a fresh-at-save typed Application query and
active-session Platform adapter for the raw signed teacher ID and exact UTF-16
notes/activity. The page queries per save; on read error it uses `ClassInfo{}`
then continues validation, conflict, and save. The save request, adapter
reread, and `ClassService` guard remain unchanged.

Fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC
19.51.36257, and Qt 6.12.0. Configure validated 1,051 handwritten source
owners; seven focused CTest targets passed 7/7 across the Application query,
Platform port, F150 policy, page save/display, parity, and SharedPolicy. The
pinned baseline `f232301e48f1e198d301acdfa3d8f704569f7ddc` parity CTest passed
1/1 with only the parity-test source overlay; baseline production page source
matches its pinned blob. Embedded debug info replaced `/Zi` after MSVC PDB
errors, and `CL` was cleared. Missing `QSqlError` inclusion and unseeded
`class_info` fixture rows were corrected before the passing recheck. No full
suite or application build ran. Gates 1 and 2 remain Partial; Phase 2 stays
In Progress/Open.

F153 is selected for live-page current/baseline parity when persisted
`teacherId=0` changes after page load. Assert the legacy teacher issue, dirty
state, no save signal or visible conflict warning, and unchanged target row.
Keep current direct query-count evidence distinct from baseline observer
evidence; do not claim query-count parity. After F153, review another planned
feature. Class Notes read is a candidate, not selected.

### Progress update - 2026-09-30 (F153 accepted; F154 selected)

F153, commit `477ed151`, accepts current/baseline live-page parity when
persisted `teacherId=0` changes after load. Current parity and page-save
targets passed 2/2; page-save retains F152's separate no-conflict-query
regression. The F153 conflict check uses a warning trap, not a query-count
comparison.
The focused harness on original pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` passed CTest 1/1. Its overlay
contains only the adapted parity source, test registration, and Qt minimum
bumps; no production source was overlaid, and baseline page source matches blob
`cdc48da8e3bab73dd0e064cf8364899f67ad1021`.

Fresh Windows x64 Debug verification used CMake 4.4.2, Ninja 1.13.2, MSVC
19.51.36257, and Qt 6.12.0. `CL` was cleared and embedded debug information was
used. The warning trap is not a query-count assertion. This was not the
expanded parity suite; no full suite or application build ran. Gates 1
and 2 remain Partial; Phase 2 remains In Progress/Open.

F154 is selected, after three independent solution reviews, as a separate
typed Class Notes page-read query and active-session Platform port. Its
projection includes class ID, exact UTF-16 notes/time-filler activity,
grade/level, regular-schedule day/start, and teacher display name; class and
teacher outcomes remain independent. Keep trimming and `SidebarNodeNaming`
title formatting/fallbacks in UI, preserve failed-read defaults, and avoid
`DataService` fallback. Load/discard use the query; refresh/save add no reads.
Verify identity/errors, independent source failures, mapping, load/discard, and
current-versus-original-pinned-baseline display parity. F154 is selected, not
implemented or accepted.

### Progress update - 2026-09-30 (F154 accepted; F155 under review)

F154, commit `8bcbf136`, adds the typed Class Notes query/port and active-session
Platform adapter. The screen projection contains class ID, exact UTF-16
notes/time-filler activities, grade/level, regular schedule day/start, and
preferred teacher display name; class and teacher outcomes are independent.
The UI retains trimming and subtitle formatting/fallbacks. Failed reads keep
defaults, with no `DataService` fallback. Load/discard use the query;
refresh/save add no reads.

Fresh Windows x64 Debug configure validated 1,058 source owners; six focused
CTest targets passed 6/6 across query, adapter, feature page, parity, and
existing Application/Platform save ports. Toolchain: CMake 4.4.2, Ninja 1.13.2,
MSVC 19.51.36257, Qt 6.12.0; `CL` was cleared and embedded debug info used.
The F154 working tree was built on previous HEAD `a8c909dd` and committed
unchanged as `8bcbf136`.

The original pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` focused parity harness passed 1/1
for initial text/subtitle and discard reload. Its overlay changed only parity
test source/registration and Qt minimums; no production source was overlaid.
Baseline page/header match blobs
`bbc9bc24a053aca83434eba6efac1e4ad5801bc2` /
`5c825327f1393791d7101ab33c10999768ff639a`. No full suite or application build
ran. F155's candidate selection is under review; it is not selected. Gates 1
and 2 remain Partial; Phase 2 remains In Progress/Open.

### Progress update - 2026-09-30 (F155 selected)

F155 is selected after three independent solution reviews for a separate Class
Co-Teacher selected-class/title read. Add a typed Application query/port and
active-session Platform adapter that returns the selected teacher ID plus the
grade, level, and regular-schedule inputs for `SidebarNodeNaming`, and assigned
teacher display name. Class and teacher outcomes remain independent. Use it on
load, discard, and after successful assignment. Preserve read/error fallbacks,
selection/title, dirty/save/signal behavior; leave the teacher-choice catalogue
and assignment use case/adapter unchanged. Do not expand into schedule, roster,
or Teacher Profile reads.

Acceptance requires typed identity/error behavior, adapter mapping with no
`DataService` fallback, read timing and source independence, load/discard/
post-save title and selected-value behavior, and current-versus-original-pinned-
baseline parity. F155 is selected, not implemented or accepted. Gates 1 and 2
remain Partial; Phase 2 remains In Progress/Open.

### Dated status correction - F123 Teacher Profile integration

The earlier F123 candidate/pending-review note above is historical. Commit
`9f7e736b2e525f18c9b135352579138860fad5b6` (`Phase2 - integrate teacher profile
edit use case`) now changes `src/features/teacher/ui/teacher_info_page.cpp` and
`tests/teacher_info_page_tests.cpp`: current page source invokes
`TeacherProfileEditUseCase`, and its tests cover save/reload and invalid-write
blocking. At that point the TeacherInfoPage target and pinned-baseline parity
had not been independently rerun; this note made no F123 acceptance claim.

### Progress update - 2026-09-30 (F155 accepted; F156 selected)

F155, commit `2d810d0e21f85233575b700e927ec3a36c907f21` (`Phase2 - add
Co-Teacher page read boundary`), adds the typed Class Co-Teacher selected-class/
title read and active-session Platform adapter. Visible load, external-change
discard, and post-save selected-value/title/persistence assertions passed on
current and baseline. Current focused CTests passed 6/6 on Windows x64 Debug
(query, Platform adapter, page, current parity, existing Application and
Platform assignment tests), using CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, and
Qt 6.12.

Pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity passed 1/1 by
reusing its cache and overlaying only adapted parity test source/registration
and Qt minimums; no production source was overlaid. Baseline page/header match
blobs `d25263eda8d464b2a3b17a35f44d6376ee5db588` /
`bba856ebb2ed907072d266a38bf3abe4e939d95e`. Keep current-only read-count
claims separate; no baseline query-count claim. Gates 1 and 2 remain Partial;
Phase 2 remains In Progress/Open.

F156 is selected after three independent investigations: replace
`TeacherService::teachers()` in `ClassCoTeacherPage` with a separate Qt-free
Application teacher-catalogue projection and active-session Platform adapter.
Include only `TeacherInfoSection` fields; preserve IDs, bilingual
ordering/details, selection, and load-failure warning/clear behavior. Add no
`DataService` or `TeacherService` fallback, and leave F155's snapshot and
assignment save unchanged. Acceptance requires mapping/no-fallback,
warning/error/clear, ordering/selection/display, current query timing, and
current-versus-original-baseline visible parity. Do not compare baseline query
counts. F156 is selected, not implemented or accepted. Gates 1 and 2 remain
Partial; Phase 2 remains In Progress/Open.

### Progress update - 2026-09-30 (F156 accepted; F157 selected)

F156, commit `3581078bdca61cfe76489ff19c5d518d8b3145bb`, replaces the
ClassCoTeacherPage `TeacherService::teachers()` read with a separate Qt-free
Application catalogue projection and active-session Platform adapter. Eight
focused current CTests passed 8/8; configure validated 1,071 owners. Toolchain:
Windows x64 Debug, CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, Qt 6.12. Coverage
includes direct active-repository mapping/no-fallback, warning/clear, crossed
bilingual order (Korean one-to-two, English two-to-one, None first),
selection/details/title on load/discard, and current save/post-save.

Original pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` parity
passed 1/1 with test-only DB API adaptation, registration, and Qt minimums; no
production overlay. Baseline page/header match blobs
`d25263eda8d464b2a3b17a35f44d6376ee5db588` /
`bba856ebb2ed907072d266a38bf3abe4e939d95e`. Do not claim baseline query-count
parity. No full suite or app build ran. Gates 1 and 2 remain Partial; Phase 2
remains In Progress/Open.

F157 is selected: replace the page-local `TeacherServiceProfileEditPort` with
an active-session Platform adapter to `TeacherRepository`, retaining
`TeacherProfileEditUseCase`, `TeacherInfoValidationPolicy`, canonical reload,
and visible validation/warning/dirty/save/signal behavior. Keep the scope away
from Teacher Profile load/navigation reads. Acceptance requires current
port/page/use-case tests, no `DataService`/`TeacherService` fallback,
repository mapping and session/repository errors, valid save/reload,
invalid-write blocking, and pinned-baseline public-page parity. Existing F123
use-case integration is not newly accepted: its page target and baseline parity
were not independently rerun. F157 is selected, not implemented or accepted.
Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open.

### Progress update - 2026-09-30 (F157 accepted; F158 selected)

F157, commit `cb6199f3369420c0e2e6c77d11f853d2799d9f92`, replaces
`TeacherInfoPage`'s page-local `TeacherServiceProfileEditPort` with an
active-session `TeacherRepository` update/reload adapter. It maps all
`TeacherProfile` fields and preserves the existing edit use case, validation
policy, canonical reload, and visible validation/warning/dirty/save/signal
behavior.

Current focused build and CTest passed 4/4 (Application use case, Platform
port, TeacherInfoPage, and page parity); configure validated 1,074 owners.
Toolchain: Windows x64 Debug, CMake 4.4.2, Ninja 1.13.2, MSVC 19.51, Qt 6.12.
Pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` public-page parity
passed 1/1 with only parity source/registration overlaid; no production source
was overlaid. Baseline page/header hashes are
`49e9ee45226ddb2f456d73c369901ad459a6eb27` /
`e279133ab8c601d5e514b3531b9e2e25a62604f1`. Parity covers valid normalized
save/reload and invalid no-write. No full suite/application build or baseline
query-count claim.

This focused run reverified TeacherInfoPage/use-case save/reload and
invalid-write behavior previously attributed to F123. It establishes only the
tested page-edit path, not broader F123 acceptance.

F158 is selected after three investigations: move only the selected-teacher
navigation read in `NavigationController::handleTeacher` to a Qt-free typed
Application query and active-session Platform `TeacherRepository::getTeacher()`
adapter, separate from F157's edit port. Preserve lookup/failure/confirmation/
load order and visible behavior. Leave existing `TeacherInfoPage` save/load
APIs and handlers unchanged; navigation still invokes its page-load path. Do
not migrate sidebar teacher operations or other reads. Acceptance requires
typed mapping, session/repository failure and no-fallback coverage,
lookup-before-leave-confirmation order, current navigation, and pinned-baseline
visible parity. F158 is selected, not implemented or accepted.
Gates 1 and 2 remain Partial; Phase 2 remains In Progress/Open.

### Progress update - 2026-09-30 (F158 accepted; F159 selected)

F158, source commit `4d099893071d4271ea8873d2219dfc7642de1e5c`, moves only the
selected-teacher lookup in `NavigationController::handleTeacher` through a
typed Qt-free Application query and active-session Platform
`TeacherRepository::getTeacher()` adapter. Lookup remains before leave
confirmation; lookup failures remain silent. F157's edit port, the page-load
path, and other teacher reads remain separate.

The focused current build and CTest passed 8/8. Navigation parity passed 1/1
on pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`; pinned
baseline production blobs remained unchanged. The baseline run used installed
Qt 6.12.0 while its CMake requires 6.11.1: a temporary three-line Qt version
metadata shim before `qt_standard_project_setup()` enabled configure, then was
removed and the original CMake hash
`cc8a061dfa64977926805167cc10418ca15d83d8` restored. The copied scratch parity
test was adapted only for the pinned three-argument `NavigationController`
API; no repository production files were overlaid. No full suite/application
build or baseline query-count claim.

### Progress update - 2026-09-30 (F159 accepted; F160 selected)

F159, source commit `1849ed23327538e2d21b05dfe0cebcc97e99d78c`, migrates only
the Native English branch of `StaffDirectoryPage::loadDirectory()` through a
typed Qt-free Application query and active-session Platform adapter using
`NativeEnglishTeacherRepository::getAll()`. It preserves the six displayed
fields, ID role, repository ordering, confirmation-before-read and
show-only-on-success behavior, unavailable-session clear, repository-error
warning, and current state semantics. GS Team and both save paths remain
outside F159.

The focused current build and CTest passed 5/5 targets: Application query,
Platform adapter, Native English page behavior, pinned-baseline parity, and
`StaffDirectoryPage` regression. Public-page parity passed 1/1 on pinned
baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` with only test/harness and
registration changes and no production overlay. The scratch baseline run used
a temporary Qt 6.12 metadata compatibility shim, removed after configure with
the original CMake hash restored, plus a parity-test API adaptation. No full
suite/application build or baseline query-count claim.

F160 is selected after three independent Investigator reviews (unanimous) and
two Explorer confirmations. Migrate only the GS Team branch of
`StaffDirectoryPage::loadDirectory()` through a typed Qt-free Application read
query and active-session Platform adapter to `GsTeamRepository::getAll()`,
using a dedicated int-backed `Domain::GsTeamMemberId`. Preserve its five
displayed fields, row ID role, repository order, unavailable-session silent
clear, repository-error warning, success-state semantics, and
confirmation-before-read/show-only-on-success behavior. Keep F159's Native
English read, both save paths, and Phase 7 model/view conversion out of scope.
Acceptance covers query/adapter mapping, errors and no-fallback; page/route
values, ordering, IDs, and failures; F159 and save regressions; and
pinned-baseline visible parity. Test the route race where
`TeacherService::isAvailable()` passes while the session is open, then the
session closes during leave confirmation and the active-session read returns
`NotFound`: do not fall back through `TeacherService` or `DataService`, or show
the page after confirmation. Test unavailable-session silent clear separately
from the repository-error warning. Make no baseline query-count claim. F160 is
selected; implementation has not started. Gates 1 and 2 remain Partial; Phase 2
remains In Progress/Open.

### Progress update - 2026-09-30 (F160 accepted; F161 selected)

F160, source commit `b703d3260a01b783594ffe6b87d10d9f7b02d193`, adds the
int-backed `GsTeamMemberId`, a typed Qt-free Application read query, and an
active-session Platform adapter to `GsTeamRepository::getAll()`. It migrates
only the GS Team branch of `StaffDirectoryPage::loadDirectory()`. The page
retains its five display fields, ID role, repository order, unavailable-session
silent clear, repository-error warning, success state, and
confirmation-before-read/show-only-on-success behavior. A route regression
closes the session during leave confirmation: the typed read returns
`NotFound`, the page stays hidden, and no `TeacherService`/`DataService`
fallback or warning occurs. Unavailable-session clear and repository-error
warning are tested separately.

Independent Tester evidence: focused current build/CTest passed 9/9 across the
F160 query, adapter, page/parity, F159, and StaffDirectoryPage regression
targets. Pinned-baseline visible parity passed 1/1 with test/registration
overlay only and no production overlay. Baseline Qt 6.11.1 versus installed
6.12.0 required a temporary scratch-only metadata shim and constructor
adaptation; the shim was removed and the pinned root CMake file restored to
hash `cc8a061dfa64977926805167cc10418ca15d83d8`. No full suite/application
build or baseline query-count claim.

F161 was selected after two Explorer lanes and three independent Investigator
lanes. It migrates only the Native English branch of
`StaffDirectoryPage::saveDirectory()` through a typed Application save
operation and active-session Platform adapter backed by
`NativeEnglishTeacherRepository::saveDirectory()`. Preserve unique-name and
optional-birthday validation, typed row IDs, add/update/delete, the repository
transaction, warning versus quiet autosave, dirty state on failure, and
successful reload plus `directorySaved`. Keep the GS Team writer and
directory model/view conversion separate. Acceptance covers typed mapping;
active-session/repository errors and no fallback; page validation/save state;
persistence, reload, and signal behavior; regressions; and pinned-baseline
visible parity. No baseline query-count claim. Implementation has not started;
F161 was selected as the narrower first writer path with one name-key invariant
and an existing typed ID.

Gate 1 and Gate 2 remain Partial; Phase 2 remains In Progress and its exit gate
remains Open.

### Progress update - 2026-09-30 (F161 accepted; F162 selected)

F161, source commit `ac173977d8d517d4af3236ee7794368bbe9a6bdc`, migrates only
the Native English branch of `StaffDirectoryPage::saveDirectory()` through a
typed Qt-free Application save/validation operation and active-session
Platform adapter calling `NativeEnglishTeacherRepository::saveDirectory()`.
Application owns empty/duplicate comparison-key and valid-or-blank birthday
decisions. The page supplies `QString::simplified().toCaseFolded()` keys and
current `QDate` facts while retaining localized warnings. Typed existing and
deleted IDs, add/update/delete, repository transaction, dirty state on failure,
quiet autosave, successful reload, and `directorySaved` are preserved. The GS
Team writer remains separate.

Focused Windows x64 Debug CTest passed 12/12 with MSVC 14.51 and Qt 6.12.0.
Coverage includes the app-less policy/use case, direct active-session adapter
and rollback, page CRUD/reload/signal/validation, unavailable-session silence,
repository warning and dirty state, quiet autosave, and F159/F160/page
regressions. Pinned-baseline public-page save parity passed 1/1 on
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`. Only parity test and registration
were overlaid; no production source was overlaid. The baseline adapter changed
nine `databaseSession()` calls to `dataService()->databaseSession()` and added
the DataService include (adapted test hash
`3508be3660d926602a29bfa21d1a8ea888eea10f`; registration hash
`80d1b5bf9366339b9df49fc0e14232107c972f2f`). The temporary Qt 6.12 metadata
shim was removed and pinned root CMake restored to hash
`cc8a061dfa64977926805167cc10418ca15d83d8`. No full suite/app build or baseline
query-count claim. Under Qt 6.12, `Straße` and `STRASSE` produce distinct
keys, while `Ä` and `ä` collide; current page semantics are preserved and the
test asserts the latter equality explicitly.

F162 was selected after two Explorer lanes and three independent Investigator
reviews. It migrates only the GS Team branch of
`StaffDirectoryPage::saveDirectory()` through a GS Team-specific typed
Application save operation/policy and active-session Platform adapter backed
directly by `GsTeamRepository::saveDirectory()`, using `GsTeamMemberId`.
Preserve the five fields, typed existing/deleted IDs, add/update/delete
transaction, warning versus quiet autosave, dirty-on-failure, reload, and
signal. Require at least one English or Korean name; normalized keys must be
unique within each language namespace, while cross-namespace matches remain
allowed; birthdays must be blank or valid. Keep the Native English writer and
model/view conversion separate. Acceptance covers app-less rules including a
cross-namespace match, mapping/session/repository failure and no fallback,
page/persistence behavior, F159-F161 regressions, and pinned-baseline visible
parity. No baseline query-count claim. F162 implementation has not started.

Gate 1 and Gate 2 remain Partial; Phase 2 remains In Progress and its exit gate
remains Open.

### Progress update - 2026-09-30 (F162 accepted; F163 selected)

F162, source commit `00a56324f1435475e3a7479fec99bc2e01653495`, migrates the
GS Team branch of `StaffDirectoryPage::saveDirectory()` through the typed
Application save operation/policy and active-session Platform adapter to
`GsTeamRepository::saveDirectory()`. Focused CTest passed 16/16, and pinned-
baseline GS Team save parity passed 1/1 on
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`. No full suite, application build,
or query-count comparison ran.

F163 is selected: add a typed class ID/name list query and active-session
Platform adapter backed by `ClassRepository::getClasses()`, replacing only the
`ClassesPage` list reads on open and after successful ClassInfo save. Preserve
order, IDs/names, selection, and empty/error behavior. Existing navigation
metadata reads and other legacy class calls stay outside the slice. Acceptance
covers query/adapter mapping, session/repository errors without fallback,
open/post-save list contents, order and selection, and pinned-baseline visible
parity. Implementation has not started. Gate 1 and Gate 2 remain Partial;
Phase 2 remains In Progress and its exit gate remains Open.

### Progress update - 2026-09-30 (F163 accepted; F164 selected)

F163, source commit `a556c0441dcebbb3b6a7baecef6e293b5644b149`, adds typed
`ClassesListReadQuery`/port and an active-session Platform adapter to
`ClassRepository::getClasses()`. It replaces only the ClassesPage open and
post-ClassInfo-save class-list reads, preserving order, IDs/names, selection,
and empty/error behavior. Navigation metadata and other legacy class calls
remain outside the slice.

Focused Application/Platform CTests passed 2/2, and direct current-page slots
passed. Pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` passed
visible list parity on open and post-save with test/stub-only overlays to
`classes_page_tests.cpp` and `schedule_widget_test_stubs.cpp`; production hashes
matched. The full ClassesPage CTest stalled in existing
`classDetailsAndCoTeacherTabsSeparateTheirSectionCards()` after
`nestedEditorsAreDeferredUntilTheirSectionIsOpened()`. No full suite,
application build, or query-count comparison ran.

F164 is selected: add a typed selected-class ID/grade read for only
`ClassesPage::rebuildSectionTabs()` and an active-session Platform adapter
backed by `ClassInfoRepository::loadClassInfo()`. Preserve the middle-school
rule, preference override, current selection, and fail-open behavior: missing
or failed grade reads leave Analytics/Evaluations visible. Keep subtitle and
navigation metadata reads outside scope. Acceptance covers typed mapping,
session/repository failures without fallback, tab visibility and
preference/current-page behavior, and pinned-baseline visible tab parity.
Implementation has not started. Gate 1 and Gate 2 remain Partial; Phase 2
remains In Progress and its exit gate remains Open.

### Progress update - 2026-09-30 (F164 accepted; F165 selected)

F164, source commit `85af7830708f062e34c94fde8fa0ff40310a8d9e`, adds
`SelectedClassGradeReadQuery`/port and an active-session adapter to
`ClassInfoRepository::loadClassInfo()`, used only in
`ClassesPage::rebuildSectionTabs()`. Grade normalization, the middle-school
rule, preference override, current selection, and fail-open behavior remain;
missing or failed grade reads leave Analytics and Evaluations visible. There is
no `DataService` fallback.

Current Application and Platform tests passed 4/4 test slots each; three
focused page slots passed. The existing
`middleSchoolAnalyticsAndEvaluationsTabsFollowPreference` test passed on both
current and pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`; baseline production blobs matched.
Stub-backed visible tab parity passed. Separate real-database parity harnesses
timed out after 300 seconds on both revisions, with no mismatch or cause
identified. No full ClassesPage suite, application build, or query-count
comparison is claimed.

F165 is selected: replace only selected-class subtitle reads in
`ClassesPage::updateHeaderText()` with a typed read of class fields used by the
existing formatter and optional assigned-teacher display data, backed by
active-session repositories without `DataService` fallback. Keep formatting in
the UI. Preserve invalid selection/unavailable services as “No class
selected”; missing/failed class info uses defaults, while teacher lookup
failure must not erase class details. Preserve class-name/`Class N` and
formatter fallbacks and refresh points, with independent class/teacher
outcomes. Acceptance covers typed mapping, session/repository failures without
fallback, independent class/teacher failure cases, and pinned-baseline visible
subtitle/fallback parity. Keep F163 list reads and F164 tab behavior separate.
Implementation has not started. Gate 1 and Gate 2 remain Partial; Phase 2
remains In Progress and its exit gate remains Open.

### Progress update - 2026-09-30 (F165 accepted; F166 selected)

F165, source commit `e0b9d61213af57a79a685264c9f64fabf841bd1a`, adds the
Qt-free `SelectedClassSubtitle` read query and active-session repository
adapter, projecting only class grade, level, regular schedule, and teacher
display fields. Class and teacher failures remain independent; subtitle
formatting, fallback, and refresh behavior stay in the UI, with no
`DataService` fallback.

Current Application and Platform CTests passed. The page slots
`selectedClassSubtitleUsesIndependentReadOutcomesAndRefreshes`,
`selectedClassSubtitleFallbackChainUsesTrimmedValues`, and
`selectedClassGradeFailureFailsOpenWithoutDataServiceFallback` passed. On the
pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, visible subtitle
`E4 Hercules • Susan • Tues (4:00)` passed with only a test-source slot
overlaid; all 584 baseline `src/` blobs matched. `git diff --check` passed.
No full suite, application build, or query-count comparison is claimed.

F166 is selected: migrate `RosterEditorWidget::updateHeaderText()` and
`sidebarClassDisplayName()` in `src/features/roster/ui/roster_editor_widget_ui.cpp`
from `ClassService::classInfo()` and `TeacherService::teacher()` to reuse F165's
typed subtitle read. Keep `SidebarNodeNaming::formatClassDisplayName()` in the
UI. Preserve invalid-ID "No class selected"; for valid IDs retain the
classroom-name/`Class N` fallback if the read is unavailable and retain class
details on teacher failure. Preserve the title, embedded heading, and roster
load/save behavior. Acceptance covers exact subtitle, teacher failure,
fallback/no-session behavior, and pinned-baseline parity. Gate 1 and Gate 2
remain Partial; Phase 2 remains In Progress and its exit gate remains Open.

### Progress update - 2026-09-30 (F166 accepted; F167 selected)

F166, source commit `577aea078a01b3a3986c07336c06631394620fcc`, routes
`RosterEditorWidget::updateHeaderText()` and `sidebarClassDisplayName()` through
F165's typed selected-class subtitle read. UI formatting, title, embedded
heading, fallbacks, and roster load/save behavior remain intact.

`ClassMngrClassesPageTests` built using the existing VS 18 2026/Qt 6.12 Debug
cache. Focused slots `rosterEditorSubtitleUsesSelectedClassSubtitleRead`,
`rosterEditorSubtitleKeepsNameFallbackWhenReadIsUnavailable`, and
`selectedClassSubtitleUsesIndependentReadOutcomesAndRefreshes` passed 5/5
including init/cleanup. Exact visible text `E4 Hercules • Susan • Tues (4:00)`
passed on current and
pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`; baseline used a
temporary test-source-only slot and no production overlay. Coverage includes
teacher failure retaining class/schedule details, class-detail failure retaining
formatter defaults, invalid ID, trimmed-name/`Class 42` fallback, and
unavailable-session name fallback with legacy services available and no legacy
class-info read. `git diff --check` passed. The unfiltered ClassesPage run
stalled at startup and was stopped after 30 seconds. No full suite, application
build, or query-count comparison is claimed.

F167 is selected: replace the Platform source in
`ApplicationServicesClassDetailsPageReadPort` with active-session reads through
`ClassInfoRepository::loadClassInfo()`,
`TeacherRepository::loadTeacherDisplayNameFields()`, and
`RosterRepository::getRosterStudentCount()`, reusing
`ClassDetailsPageReadSnapshot`. Preserve independent class, teacher, and count
results; raw schedule fields/order; defaults/fallbacks; and
`Teacher::preferredDisplayName()` precedence. Acceptance covers adapter mapping,
missing/source failures without `DataService` fallback, existing
ClassDetailsPage display regressions, and pinned-baseline visible
fields/schedule/teacher/count/fallback parity. F167 implementation has not
started. Gate 1 and Gate 2 remain Partial; Phase 2 remains In Progress and its
exit gate remains Open.

### Progress update - 2026-10-01 (F207 accepted; F208 selected)

F167, source commit `97efac8b`, updates
`ApplicationServicesClassDetailsPageReadPort` to use active-session
`ClassInfoRepository::loadClassInfo()`,
`TeacherRepository::loadTeacherDisplayNameFields()`, and
`RosterRepository::getRosterStudentCount()`. It reuses
`ClassDetailsPageReadSnapshot` and preserves independent class, teacher, and
count outcomes, raw schedule fields/order, defaults/fallbacks, and
`Teacher::preferredDisplayName()` precedence.

Current focused tests passed 8/8 adapter, 5/5 existing display, and 4/4 live
parity. The same live parity source passed 4/4 on pinned baseline
`48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99` with test/CMake overlays only and no
production overlay, covering visible fields, raw schedule order, preferred
teacher, count, and missing-teacher fallback while retaining class/count. An
exploratory missing-class-info-row test exposed a pre-existing UI fallback
difference: current shows the selected class name; baseline shows
`Unknown Class • No Teacher`. F167 changed no UI code and retained current
behavior; this case is excluded from parity claims.

F168, source commit `07493cab`, migrates
`ApplicationServicesClassDetailsSavePort` to the active-session
`ClassInfoRepository`, preserving validation, hidden fields/schedules, and
independent typed failures without `DataService` fallback. Focused CTest targets
passed 4/4: save port 9, page display 5, page save 14, and live parity 9. On
pinned baseline `48fc5c5cc7dee78d82f8bf5f1bf8b51725575b99`, the visible-save
parity case passed with only the parity test and target registration overlaid;
no production files were overlaid. `git diff --check` passed. No full suite or
application build is claimed.

F169, source commit `56dec15a`, migrates
`ApplicationServicesClassNotesSavePort` to the open session's
`ClassInfoRepository`, trims both fields, and uses
`ClassInfoValidator::validateNotes` without `ClassService` or `DataService`
fallback. It preserves invalid/unavailable/technical result mapping and the
UTF-16 request limit. Independent Tester review and focused CTest passed 2/2:
save-port target in 0.18s and ClassNotesPage target in 2.91s.
`git diff --check` passed. No full suite or application build ran.

F170, source commit `68391fac`, migrates `ApplicationServicesRosterReadPort`
to the open session's `RosterRepository::loadRoster()` with no service or
`DataService` fallback. Tester confirmed canonical-ID validation, complete
ordered sparse snapshot conversion, empty-roster success, technical repository
errors, and NotFound on a closed session. Independent CTest command:
`ctest --test-dir build/f168 -R "ClassMngrNextApplicationRosterReadQueryTests|ClassMngrNextPlatformApplicationServicesRosterReadPortTests|ClassMngrRosterEditorWidgetSaveTests" --output-on-failure`;
3/3 passed (query 0.02s, port 0.19s, widget 2.08s; 2.29s total). The two named
widget read slots also exited 0 individually. `git diff --check` passed. No
full suite or application build ran.

F171, source commit `7cb8e5d7`, migrates `ApplicationServicesRosterSavePort`
from `RosterService` to active-session `RosterRepository::saveRoster`, with no
service or `DataService` fallback. It preserves canonical ID/open-session
checks, `RosterValidator::normalized()` and `validate()` with the questionable
Korean name-length flag, and Technical error mapping. Tests cover normalized
full-snapshot persistence, exact stored-snapshot preservation on invalid input,
the allow flag, injected-DB-failure rollback, and closed session with
`DataService` present. Independent CTest command:
`ctest --test-dir build/f168 -R "ClassMngrNextApplicationRosterSaveUseCaseTests|ClassMngrNextPlatformApplicationServicesRosterSavePortTests|ClassMngrRosterEditorWidgetSaveTests" --output-on-failure`;
3/3 passed (0.02s, 0.24s, 2.03s; 2.30s total). The independent Tester also
passed 3/3. `git diff --check` passed. No full suite or application build ran.

F172, source commit `2daa209e`, migrates
`ApplicationServicesClassCoTeacherAssignmentPort` to active-session
`ClassInfoRepository`, preserving positive-ID behavior, typed assignment and
unassignment, stored class fields/schedules, joined teacher metadata, full
`ClassInfoValidator` normalization, and regular-then-intensive conflict checks
with exact current messages. It has no `ClassService` or `DataService`
fallback. Port tests cover invalid IDs/loaded data, exact regular/intensive
conflict messages and precedence, complete no-write state, injected
transactional rollback, and open/closed sessions. Worker and independent Tester
each built and passed 3/3:
`ClassMngrNextApplicationClassCoTeacherAssignmentTests`,
`ClassMngrNextPlatformApplicationServicesClassCoTeacherAssignmentPortTests`,
and `ClassMngrNextFeatureClassCoTeacherPageTests`. `git diff --check` passed.
No full suite or application build ran.

F173, source commit `a214dec4`, migrates
`ApplicationServicesScheduleSlotStateSavePort` to the active session's
`IntensiveSlotStateRepository`. Independent verification passed 3/3 focused
targets: `ClassMngrNextApplicationScheduleSlotStateSaveTests`,
`ClassMngrNextPlatformApplicationServicesScheduleSlotStateSavePortTests`, and
`ClassMngrScheduleWidgetTests`. `git diff --check` passed. No full suite or
application build ran.

F174, source commit `16188918`, migrates
`ApplicationServicesSpeakingEvaluationReadPort` to the active-session
`SpeakingEvalRepository`, with no feature-service or `DataService` fallback.
Independent Tester and implementer each reported all three targets passed:
`ClassMngrNextApplicationSpeakingEvaluationQueryTests`,
`ClassMngrNextPlatformApplicationServicesSpeakingEvaluationReadPortTests`, and
`ClassMngrSpeakingEvalPageSaveTests`. Coverage includes canonical IDs,
exact/missing name behavior, 25x11 UTF-16 Unicode order, whitespace-only names
and SQL errors mapped to Technical, closed-session NotFound, and a blank clean
grid on page read failure. `git diff --check` passed. No full suite or app build
ran.

F175, source commit `4009fcd5`, migrates
`ApplicationServicesScheduleBuilderSourcePort` from constructing
`ClassService(session, nullptr)` to active-session
`DatabaseSession::classInfoRepository()->loadScheduleClassInfos()`, preserving
session checks, NotFound/Technical mappings, snapshot fields/order/raw
schedules, Testing Class exclusion, teacher missing/stale behavior, and widget
modes. The implementation and independent Tester each passed all four focused targets:
`ClassMngrNextApplicationScheduleBuilderSourceSnapshotTests`,
`ClassMngrNextPlatformApplicationServicesScheduleBuilderSourcePortTests`,
`ClassMngrScheduleBuilderTests`, and `ClassMngrScheduleWidgetTests`.
`git diff --check` passed. No full suite or application build ran.

F176, source commit `e111d5be`, migrates
`ApplicationServicesSubPrepClassDetailsPort` to the active-session
`ClassInfoRepository`. Independent implementation and Tester each passed 3/3:
`ClassMngrNextApplicationSubPrepClassDetailsQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and
`ClassMngrSubPrepPageTests`. The direct read preserves canonical ID and record
identity, preferred-name selection, bounded UTF-8 conversion, teacher
fallbacks, structured errors, and the post-read closed-session recheck. No full
suite or application build ran.

F177, source commit `25b9719c`, migrates
`ApplicationServicesSubPrepScheduleSummaryPort` to active-session
`ClassInfoRepository::loadSubPrepClassSummaries()`, preserving scope
validation, empty-scope no-read behavior, ordering/omission, bounded projection,
meeting formatting, and unavailable/read-error mapping. Implementation and
independent Tester each passed all three targets:
`ClassMngrNextApplicationSubPrepScheduleSummaryQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`, and
`ClassMngrSubPrepPageTests`. `git diff --check` passed. No full suite or
application build ran.

F178, source commit `afeab035`, migrates
`ApplicationServicesSubPrepPrintSourcePort` to active-session class-info,
teacher, and roster repositories. Implementation and independent Tester each
passed all four targets:
`ClassMngrNextApplicationSubPrepPrintSourceQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests`,
`ClassMngrSubPrepPrintSourceMapperTests`, and `ClassMngrSubPrepPageTests`. Added
coverage confirms unopened/closed sessions return NotFound. No full suite or
application build ran.

F179, source commit `82931424`, migrates
`ApplicationServicesSubPrepRosterOutputSourcePort` from
`ClassService`, `TeacherService`, and `RosterService` to direct active-session
`ClassInfoRepository::loadClassInfosForScheduleScope()` and `loadClassInfo()`,
`ClassRepository::getClassById()`, `TeacherRepository::getTeacher()`, and
`RosterRepository::loadRosterForOutput()`. Preserve request validation and
return success without reads when the selected class or day scope is empty.
Preserve class order, mode/day filtering, unassigned/missing/stale
teacher behavior, class/schedule identity checks, extra-column normalization
and deduplication, UTF-8 and aggregate text limits, cumulative remaining
row/cell/text budgets, existing errors, no `DataService` fallback, and no
partial output/package on failure. Verify
`ClassMngrNextApplicationSubPrepRosterOutputSourceQueryTests`,
`ClassMngrNextPlatformApplicationServicesSubPrepRosterOutputSourcePortTests`,
`ClassMngrSubPrepPackageServiceTests`, and `ClassMngrSubPrepPageTests`.
Implementation and independent Tester each passed all four targets. Added
coverage exercises unavailable sessions, stale-teacher failure without partial
output, technical repository failure, and empty-day no-read. `git diff --check`
passed. `build/f168` contains all four targets and is retained. Gate 1 and Gate
2 remain Partial; Phase 2 remains In Progress and its exit gate remains Open.

F180, source commit `0f3ebf51`, migrates
`ApplicationServicesSubPrepCalendarEventIntervalsPort` in
`src/next/platform/application_services_sub_prep_calendar_event_intervals_port.h`
to open-session `CalendarEventRepository::loadCalendarEventDateIntervalsInRange()`.
Preserve query/window validation, the inclusive current- and following-year
range with a year-9999 clamp, unlimited purpose-specific results, Vacation and
Holiday filtering, crossing-interval order, quiet read failure with the page's
empty-calendar fallback, and the injectable `IntervalRangeReader` with its
4,097-event no-projection-cap test. Implementation and independent Tester each
passed all three targets:
`ClassMngrNextApplicationSubPrepCalendarEventIntervalsQueryTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and
`ClassMngrSubPrepPageTests`. Added tests cover unopened/closed-session NotFound
and active-repository Technical failure. `git diff --check` passed.

F181, source commit `fcb68738`, migrates
`ApplicationServicesCalendarEventImportSignatureQueryPort` in
`src/next/platform/application_services_calendar_event_import_signature_query_port.h`
to active-session `CalendarEventRepository::loadCalendarEventsInRange()`,
including `isAvailable()`, with no `CalendarService` or `DataService` fallback.
Preserve canonical ordered ISO range validation, all rows without projection
cap, repository order and duplicates, and the six-field UTF-16 signature
normalization: simplified title, normalized event type and time status, ISO
dates, and `allDay`. Preserve typed InvalidInput, NotFound, and Technical
errors, with no `CalendarService` or `DataService` fallback. Implementation
and independent Tester each passed all four targets:
`ClassMngrNextApplicationCalendarEventImportSignatureQueryPortTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventImportUseCaseTests`, and
`ClassMngrCalendarEventImportParityTests`. Unavailable-session and repository
errors now use the message “The calendar event repository is unavailable.”
`git diff --check` passed.

F182, source commit `1d4d9eda`, moves
`ApplicationServicesCalendarEventPort` to the open active session's
`CalendarEventRepository` for availability, by-ID, and range reads, with no
`CalendarService` or `DataService` fallback. The implementer and independent
Tester each passed `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarEventRepositoryTests`. Coverage includes unavailable/closed
sessions and repository Technical failures, missing-ID mapping, and the 4,096
projection cap (4,097 fails); the import signature query remains uncapped.
`git diff --check` passed. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

F183, source commit `3c13a5fb`, migrates
`ApplicationServicesCalendarEventSavePort` to the open session's
`CalendarEventRepository::saveCalendarEvents({event})`, with no
`CalendarService` or `DataService` fallback. It retains
`CalendarEventValidator::normalized()` followed by `validateSeries()`, the
one-event transaction path, typed create/update IDs, request/date/time
conversion, `repeatSeriesId` clearing, and error mapping. Implementation and
independent Tester each passed
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarEventRepositoryTests`. Coverage includes unopened/closed
sessions with `DataService` present, create/update IDs, normalized title,
invalid requests, and repository failure. `git diff --check` passed. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

F184, source commit `adf60cbf`, migrates
`ApplicationServicesCalendarEventDeletePort` to the active session's
`CalendarEventRepository::deleteCalendarEvent(int)`, with no
`CalendarService` or `DataService` fallback. It preserves positive typed-ID
validation, NotFound for unavailable/closed sessions or a missing repository,
Technical for repository failures while open, and success for a valid positive
ID with no matching row. Implementation and independent Tester each passed
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarEventRepositoryTests`. Coverage includes repository
create/delete, missing-ID success, invalid IDs, unopened/closed sessions with
`DataService` present, and injected SQL failure. The null repository case has
no direct fixture because normal open sessions provide the repository; this is
nonblocking. `git diff --check` passed. Phase 2 remains In Progress/Open; Gates
1 and 2 remain Partial.

F185, source commit `a4bd5914`, migrates
`ApplicationServicesCalendarEventDeleteAllPort` availability and deletion to
the active session's `CalendarEventRepository::deleteAllCalendarEvents()`,
with no `CalendarService` or `DataService` fallback. It preserves
unavailable/closed NotFound, open-session repository Technical failures, and
success, while leaving `CalendarPreferencesPanel` confirmation/cancel,
warning, and success behavior unchanged. Implementation and independent
Tester each passed
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarEventRepositoryTests`. Tests cover seeded regular and
repeat-series rows, open/unopened/closed availability with `DataService`
present, and SQL failure; the repository test verifies F104 `sqlite_sequence`
parity. The null-repository guard has no direct fixture because open sessions
normally provide it; this is nonblocking. `git diff --check` passed. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

F186, source commit `192dcc8c`, migrates
`ApplicationServicesCalendarEventSeriesDeletePort` to the active session's
`CalendarEventRepository::deleteCalendarEventsForRepeatSeriesFromDate()`,
with no `CalendarService` or `DataService` fallback. It preserves request
validation before session lookup, exact diagnostics, date conversion and
series ID handling, selected-and-later suffix scope while retaining earlier
and unrelated events, NotFound for unavailable/closed sessions, Technical
repository failures, and success when no rows match. Implementation and
independent Tester each passed
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarEventRepositoryTests`. Coverage includes padded IDs,
no-match success, invalid diagnostics, open/closed sessions with `DataService`,
SQL failure, and F108 repository suffix/sequence parity. `git diff --check`
passed. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

F187, source commit `e7396c77`, migrates
`ApplicationServicesCalendarEventImportSavePort` to exactly one active open
session `CalendarEventRepository::saveCalendarEvents(normalizedEvents)` call,
with no `CalendarService` or `DataService` fallback. It preserves
`request.validate()` creation-only behavior and the 4,096-event maximum,
canonical date/time
and all-day/unknown-field conversion, event order, normalization then
`validateSeries()`, blank `repeatSeriesId`, ordered typed-ID cardinality, batch
transaction/rollback, empty-batch success while open, unavailable/closed
NotFound, and open-session repository Technical failures. Import use-case,
query, and UI wiring remain unchanged. Implementation and independent Tester
each passed
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventImportUseCaseTests`,
`ClassMngrCalendarEventImportParityTests`, and
`ClassMngrCalendarEventRepositoryTests`; build and `git diff --check` passed.
Source has one visible repository save call; tests verify result order and
rollback rather than instrumenting invocation count. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

F188, source commit `6ad0dc6f`, migrates
`CalendarEventSeriesCreatePort` to the active session's
`CalendarEventRepository::saveCalendarEvents(normalizedEvents)` in one call,
with no `CalendarService` or `DataService` fallback. It preserves request
validation before session access; daily/weekly/monthly occurrence order;
trimmed series ID; canonical date/time, `allDay`, and unknown-field conversion;
normalization then `validateSeries()`; ordered positive typed IDs; and
NotFound/Technical mapping. Implementation and independent Tester passed
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarEventRepositoryTests`; build and `git diff --check` passed.
Coverage includes normalized fields, series ID, ordered IDs, validation before
session lookup, unavailable/open/closed sessions with `DataService` present,
and transaction rollback. Optional typed occurrence-ID conversion remains in
code without direct test coverage; no defect was found. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

F189, source commit `99f41d0e`, migrates
`ApplicationServicesCalendarEventSeriesEditPort` to active-session
`CalendarEventRepository::loadCalendarEventsForRepeatSeriesFromDate()` and one
normalized, validated, ordered `saveCalendarEvents()` batch, without
`CalendarService` or `DataService` fallback. It preserves request validation
before session lookup, suffix order/identity, planner offsets, durations and
field propagation, earlier and unrelated rows, empty-suffix success,
NotFound/Technical mapping with repository wording, and atomic updates.
Implementation and independent Tester passed
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarEventRepositoryTests`; build and `git diff --check` passed.
Coverage includes repository seed/read parity, suffix order/identity, prefix/unrelated-row
retention, empty suffix, invalid source/overflow, all-day/unknown-time fields,
unavailable/closed sessions with `DataService`, read failure, and rollback on a
second update. Source has one suffix load and one batch save; tests do not
instrument call count.

F190, source commit `7d1c6cdd`, migrates
`ApplicationServicesCalendarEventDisplayPreferencesPort` to the active open
session's `SettingsRepository`. It preserves the exact keys, default-false
reads, QVariant boolean coercion, read-error fallback to false, unavailable
and null no-op behavior, Technical save-error mapping, atomic two-key save,
and unrelated settings. The focused build succeeded; the independent Tester
passed `ClassMngrNextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests`
1/1 and `git diff --check`. Closed/unavailable-session coverage includes
`DataService`; generic `SettingsService` behavior and UI/callers are unchanged.
The stale `saveAll` comment was corrected before the source commit.

F191, source commit `d170c5f3`, migrates
`ApplicationServicesCalendarEventTypeColorPreferencesPort` to use only the
active open session's `SettingsRepository`. It preserves the exact dynamic
key, caller-normalized event type, UTF-8 bytes, invalid stored-color passthrough,
unavailable/null no-op behavior, save-failure warning, and previous value on
save failure. Tests cover closed-session no-fallback with `DataService`,
read-error empty fallback, round-trip, unrelated setting, and existing
behavior. The build succeeded; the independent Tester passed registered CTest
`ClassMngrNextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests`
1/1 and `git diff --check`. Generic/sessionless `SettingsService` behavior and
caller/UI are unchanged.

F192, source commit `f77b6d1c`, migrates
`ApplicationServicesCalendarFirstDayOfWeekPreferencesPort` to use only the
active open session's `SettingsRepository`. It preserves
`calendar/firstDayOfWeek`, recalculated `QLocale` fallback for missing,
unavailable, null, closed, read-error, and invalid values, all values `0..6`,
unavailable/closed no-op saves, save-warning behavior, provider normalization,
revision and signal ordering, and generic `SettingsService` behavior. The
worker build and independent registered CTest
`ClassMngrNextPlatformApplicationServicesCalendarFirstDayOfWeekPreferencesPortTests`
passed 1/1; `git diff --check` passed.

F193, source commit `e0042081`, migrates
`ApplicationServicesAcademicCalendarSchedulePreferencesPort` to use only the
active open session's `SettingsRepository`. It preserves exact key
`calendar/academicSchedule/v1`, opaque UTF-8 payload round-trip without parsing
or rewriting, empty reads/no-op writes for missing/unavailable/null/closed/
read-error cases, save warning and prior value on failure, and unrelated
settings. The worker build succeeded; independent registered CTest
`ClassMngrNextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests`
passed 1/1, and `git diff --check` passed. The historical 2026-09-26 generated-
MOC build failure did not recur. Provider, callers, and generic
`SettingsService` are unchanged.

F194, source commit `7db4cfbb`, migrates
`ApplicationServicesScheduleDisplayPreferencesPort` to the active open
session's `SettingsRepository`. It preserves the five exact keys, false
defaults, QVariant coercion, one atomic `saveSettings` call, Technical error
mapping, successful no-op for unavailable/closed sessions, and unrelated
settings. Tests cover read-error defaults, closed-session no-fallback with
`DataService`, keys/coercion, unrelated settings, and rollback. The worker
build succeeded; independent registered CTest
`ClassMngrNextPlatformApplicationServicesScheduleDisplayPreferencesPortTests`
passed 1/1, and `git diff --check` passed. Callers and generic
`SettingsService` are unchanged.

F195, source commit `eb73707b`, migrates
`ApplicationServicesScheduleDisplayModePreferencesPort` to use only the active
open session's `SettingsRepository`. It preserves canonical
`schedule_display_mode` values regular/intensive/testing, legacy
`schedule_show_intensive` fallback, migration writes only when the canonical
QVariant is absent/invalid, and leaves an invalid-but-present canonical value
untouched while retaining legacy fallback interpretation. Reads use Regular
when unavailable/closed; saves are no-ops in those states. Legacy-read and
save failures retain their warnings. The worker build and independent CTest
`ClassMngrNextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests`
passed 1/1; `git diff --check` passed. Callers and generic `SettingsService`
are unchanged. Two Explorer lanes differed, with one suggesting Current
Campus; Schedule display mode was selected after comparing scans for
continuity with F194 and existing focused migration coverage.

F196, source commit `b9f0a07d`, migrates
`ApplicationServicesCurrentCampusPreferencesPort` to direct
`DatabaseSession`/`SettingsRepository` access, with no
`DataService`/`SettingsService` fallback. It
preserves key `myInfo/campus`, verbatim UTF-8 and `QVariant::toString()`
conversion, unavailable/closed behavior, read-error empty results, and
Technical write-error mapping. The executor self-check passed 9 test slots;
the independent Tester rebuilt the focused target and CTest passed 1/1.
`git diff --check` passed; no full suite ran. Callers and the Personal Details
aggregate writer remain unchanged.

F197, source commit `26c0f23b`, migrates
`ApplicationServicesMiddleSchoolAnalyticsPreferencesPort` to the active open
session's `SettingsRepository` only. It preserves exact key
`classes_navigation_show_middle_school_analytics_and_evaluations`,
`QVariant::toBool()`, best-effort false materialization for missing, invalid,
or read-error values, unavailable/closed false/no-op behavior, and silent write
failures. The Executor and independent Tester passed the focused registered
CTest `ClassMngrNextPlatformApplicationServicesMiddleSchoolAnalyticsPreferencesPortTests`
1/1. Tester verified SQLite-trigger write failure is silent and preserves
stored `true`, closed-session no-fallback with `DataService`, and repository-
only adapter references. `git diff --check` passed; generic/sessionless
`SettingsService` and callers are unchanged. No full suite ran.

F198, source commit `0e8361eb`, migrates
`ApplicationServicesPersonalDetailsSavePort` to one atomic active-session
`SettingsRepository::saveSettings()` batch for the nine-key bundle. It
preserves exact keys/values, UTF-8 and prepared signature-image encoding,
mode/font normalization, typed Technical failures, and rollback. After an x64
MSVC rebuild, independent focused CTest
`ClassMngrNextPlatformApplicationServicesPersonalDetailsSavePortTests` passed
1/1. Closed-session coverage confirms no `DataService` fallback and no bundle
changes after reopen. `git diff --check` passed; no full suite ran. UI callers
and generic/sessionless `SettingsService` are unchanged.

F199, source commit `dc489863`, migrates
`ApplicationServicesPersonalDisplayNamePreferencesPort` to the active-session
`SettingsRepository` for `myInfo/name`, with no facade fallback. It preserves
UTF-8 and whitespace, empty reads/no-op writes when unavailable, Technical
write failures, and warning/default behavior on read error. Coverage includes
closed-session no-fallback with `DataService`, read-failure warning, and
integration with F198's aggregate writer. Independent x64 MSVC CTest
`ClassMngrNextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests`
passed 1/1. `git diff --check` passed; no full suite ran.

F200, source commit `ef603583`, migrates
`ApplicationServicesPersonalSignaturePreferencesPort` to use only the active
session's `SettingsRepository` for mode, font, and text. It preserves keys,
mode `1`/other-value Type/Image mapping, font `toInt()`, typed-text UTF-8 and
whitespace, and defaults; warns and defaults on repository read failure;
returns the existing Technical error when unavailable, null, or closed; and
does not write missing or invalid values. Focused x64 MSVC CTest
`ClassMngrNextPlatformApplicationServicesPersonalSignaturePreferencesPortTests`
passed 1/1. Coverage includes read errors/warnings, unchanged invalid values,
closed-session no-fallback with `DataService` and preserved database values,
and values written by F198's aggregate port. `git diff --check` passed; no full
suite ran.

F201, source commit `59133929`, moves
`ApplicationServicesPersonalSignatureImagePort` to use only the active open
session's `SettingsRepository` for exact key `myInfo/signatureImage`. It
preserves `QVariant::toString().toLatin1()`, Base64 decoding, one
`SignatureImage::prepareForEmbedding`, empty output for
missing/corrupt/unavailable/read-error values, and warning on read error.
Closed-session coverage with `DataService` present returns empty and confirms
image and unrelated settings remain unchanged after reopen. Independent x64
MSVC CTest
`ClassMngrNextPlatformApplicationServicesPersonalSignatureImagePortTests`
passed 1/1. `git diff --check` passed; no full suite ran.

F202, source commit `c9ff5731`, migrates
`ApplicationServicesClassVisibilityPreferencesPort` to the active open
session's `SettingsRepository`, with no compatibility-service fallback. It
preserves key `classes_navigation_visibility_scope`, trimmed/lowercased
`all_classes` mapping, ActiveSchedule defaults and missing-key materialization,
no rewrite of valid unsupported values, and silent write failures. Closed-
session coverage retains `DataService`, returns the default, and confirms
stored and unrelated values after reopen; trigger-based failed write preserves
the stored value silently. Repository read failure is also silent. Independent
focused CTest
`ClassMngrNextPlatformApplicationServicesClassVisibilityPreferencesPortTests`
passed 1/1; `git diff --check` passed. No full suite ran.

F203, source commit `4f128b01`, migrates
`ApplicationServicesEvaluationDefaultPolicyPort` to the active session's
`SettingsRepository`. It preserves key
`classes_navigation_evaluation_default_policy`, trimmed/lowercased
`current_or_previous_term` and canonical `all`/`current_or_previous_term`
values, `All` fallback, default materialization for missing/invalid/read-error
values, and no rewrite of valid unrecognized values. Closed sessions have no
facade fallback; trigger-based failed saves and repository read errors remain
silent. Independent registered CTest
`ClassMngrNextPlatformApplicationServicesEvaluationDefaultPolicyPortTests`
passed 1/1. No full suite ran.

F204, source commit `3cf2ab80`, migrates
`ApplicationServicesClassDayFilterResetPolicyPort` to the active session's
`SettingsRepository`, with no facade fallback. It preserves exact key
`classes_navigation_day_filter_reset_policy`, trimmed/lowercase
`on_page_leave` mapping to OnPageLeave, OnApplicationClose default,
best-effort materialization for missing/invalid/read-error values, and no
rewrite of valid unknown values. Closed-session coverage includes `DataService`;
read-error and trigger-based failed-write behavior are silent. Independent
registered CTest
`ClassMngrNextPlatformApplicationServicesClassDayFilterResetPolicyPortTests`
passed 1/1. No full suite ran.

F205, source commit `bf9ca8a7`, migrates
`ApplicationServicesClassSelectionResetPolicyPort` to the active session's
`SettingsRepository`, with no closed-session fallback. Independent registered
CTest `ClassMngrNextPlatformApplicationServicesClassSelectionResetPolicyPortTests`
passed 1/1. After fixing two closed-database fixtures, both filtered
`ClassesPage` slots passed. `git diff --check` passed; no full suite ran.

F206, source commit `7d0291d3`, migrates
`ApplicationServicesCustomColorPalettePreferencesPort` to the active
session's `SettingsRepository`. The focused adapter CTest passed 1/1 in the
executor and independent run; independent
`ClassMngrColorUtilsCustomColorPaletteTests` passed 1/1. The source owner check
validated 1,138 owners. `git diff --check` passed; no full suite ran.

F207, source commit `12cb021a`, migrates
`ApplicationServicesSubPrepPersonalZoomPreferencesPort` to the active open
session's `SettingsRepository`. It preserves the primary keys
`myInfo/zoomLoginId`, `myInfo/zoomPassword`, and `myInfo/zoomNotAvailable`,
legacy keys `subPrep/personalZoomEmail`, `subPrep/personalZoomPassword`, and
`subPrep/personalZoomNotAvailable`, primary precedence, legacy fallback and
best-effort migration writes, UTF-8, `QVariant::toBool()`, `N/A`/true defaults,
the typed Technical unavailable error, successful legacy results when
migration saves fail, and no closed-session `DataService` fallback. Focused
CTest passed 1/1 in executor and independent runs. Source ownership validated
1,139 owners. `git diff --check` passed; no full suite ran.

### Progress update - 2026-10-01 (F208 accepted)

F208, source commit `d17ddd25`, migrates
`ApplicationServicesSubPrepPreferencesPort` to the active open session's
`SettingsRepository`. It preserves the exact four keys, UTF-8 and whitespace,
empty required fields versus absent or present-empty optional fields, and
missing reads without materialization (verified by an explicit zero-row query
after load). Four-key saves are atomic, roll back on failure, and preserve
unrelated settings. Unavailable/save failures retain typed Technical results;
repository read failures retain warnings and defaults. Closed-session coverage
confirms no fallback with `DataService` present. The focused x64 build passed;
source ownership validated 1,140 handwritten files; independent CTest
`ClassMngrNextPlatformApplicationServicesSubPrepPreferencesPortTests` passed
1/1 in `build/f168`; `git diff --check` passed. No full suite ran.

### Progress update - 2026-10-01 (F209 accepted; F210 selected)

F209, source commit `7b2f8226`, moves
`ApplicationServicesSpeakingEvaluationSavePort` to the active session's
`SpeakingEvalRepository`, moves implementation into `.cpp`, registers it under
`ClassMngrFeatures`, and removes service/database/repository/validator details
from the shared header. It preserves canonical ID checks, open-session
NotFound, trimmed evaluation name, `SpeakingEvalValidator` normalization and
validation (including the preserved, questionable Korean-name-length flag
behavior), the 25x11 matrix, original changed-cell delta, error mapping, and no
`DataService` fallback. Tests cover
matrix/delta, name and score-alias normalization, invalid create/update
rejection, flag behavior, closed-session behavior with `DataService` present,
and trigger-forced repository save failure with the persisted matrix
unchanged. The focused x64 build passed; CMake ownership validated 1,141
handwritten sources; independent CTest
`ClassMngrNextPlatformApplicationServicesSpeakingEvaluationSavePortTests`
passed 1/1 in `build/f168`. No full suite ran.

At this update, F210 was selected: move deterministic recent-workspace history
mutation from `FileController` to a Qt-free Application use case using existing
`RecentWorkspaceHistory` and `RecentWorkspaceHistoryPort`, in planned target
`NextApplicationRecentWorkspaceHistoryUseCase` (app-less). The planned policy
preserved
raw/normalized alias removal, normalized-path prepend, the 10-entry cap,
`lastPath` recording, pruning both aliases, clearing `lastPath` only on a
match, and unrelated fallback state. Qt path normalization, encoding
conversion, last-database-directory updates, and menu work were to remain in
`FileController`; `FileControllerWorkspaceLifecycle` integration coverage
would be retained. Two fresh independent scans compared candidate scopes and
agreed that no direct `DataService` or `SettingsService` references remained
under `src/next`; the policy was controller-owned, so its move was expected to
advance Gate 1. Speaking Evaluation read-port `.cpp` extraction was a later
structural cleanup; the last-selected-campus length concern was lower priority
and not a confirmed defect. This selection snapshot is superseded by the
following F210 acceptance and F211 selection. Phase 2 remained In Progress/Open;
Gates 1 and 2 remained Partial.

### Progress update - 2026-10-01 (F210 accepted; F211 selected)

F210, source commit `22cec99b`, moves recent-workspace history mutation from
`FileController` into the Qt-free Application use case
`NextApplicationRecentWorkspaceHistoryUseCase`. It preserves raw/normalized
alias removal, normalized newest-first insertion, the ten-entry cap, setting
`lastPath` when recording, pruning both aliases, clearing `lastPath` only on a
match, and unrelated fallback state.
`FileController` retains Qt path work, persistence, database-directory
updates, and menu work. Independent focused CTest passed 2/2 in `build/f168`:
`ClassMngrNextApplicationRecentWorkspaceHistoryUseCaseTests` and
`ClassMngrFileControllerWorkspaceLifecycleTests`.

F211 is selected after two independent Explorer scans agreed on extracting
`ApplicationServicesSpeakingEvaluationReadPort` implementation from its header
into a `.cpp`, distinct from F209's accepted save-port migration. Preserve
canonical positive class IDs and query-identity validation, exact UTF-16 names
and row order, active open-session/repository availability, Technical
repository/exception errors, and the page's blank-grid behavior on read
failure. The existing focused target is
`ClassMngrNextPlatformApplicationServicesSpeakingEvaluationReadPortTests`.
This structural cleanup identifies no new behavioral Gate 1 gap. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-01 (F211 accepted; F212 selected)

F211, source commit `2347739c`, extracts
`ApplicationServicesSpeakingEvaluationReadPort` implementation into `.cpp`,
preserving its typed port/header boundary and semantics; this read-port
extraction is distinct from F209's accepted save-port migration. The build
passed all three focused targets; CMake source ownership validated 1,144
handwritten sources; independent focused CTest passed 3/3 in `build/f168`:
`ClassMngrNextApplicationSpeakingEvaluationQueryTests`,
`ClassMngrNextPlatformApplicationServicesSpeakingEvaluationReadPortTests`, and
`ClassMngrSpeakingEvalPageSaveTests`. Coverage includes canonical IDs,
exact-name misses, ordered Unicode rows, repository errors, closed sessions,
query identity, and page blank-grid behavior. Exception mapping was
source-reviewed; tests did not directly throw an exception. No full suite ran.

F212 is selected: move upcoming-birthday date parsing, occurrence generation,
and bucket policy from `src/features/teacher/upcoming_birthday_schedule.cpp` into a
Qt-free Application use case with an app-less test target. Keep Qt `QDate` and
`QString` conversion, locale-aware sorting, and dialog presentation at the
feature/UI edges. Preserve an empty result for an invalid reference date,
trimmed `MM-dd`, omission of invalid dates and blank display names, buckets for
today, this week through Sunday, and next week through Sunday, year rollover,
February 29 mapping to February 28 in non-leap years, all three staff groups,
and display-name fallback rules. Keep existing dialog/action coverage in
`UpcomingBirthdaysTests`, expanding only schedule checks as needed.
Two Explorer scans compared candidate scopes: workspace-port `.cpp` extraction
is structural cleanup, while birthday bucketing moves Qt feature policy into
Application and advances Gate 1. Phase 2 remains In Progress/Open; Gates 1 and
2 remain Partial.

### Progress update - 2026-10-01 (F212 accepted; F213 selected)

F212, source commit `bd6d044f`, moves upcoming-birthday date parsing,
occurrence generation, and week bucketing from the feature wrapper into a
Qt-free Application use case. CMake ownership validated 1,146 handwritten
sources. The focused Application and feature CTests passed 2/2; after a Unicode
display-name regression assertion was added, the focused Application target
passed 1/1 and the independent Tester recheck passed 1/1. No full suite ran.

F213 is selected: move default evaluation selection's populated-row semantics
into Qt-free Application and route the feature wrapper through the existing
typed `SelectedClassGradeReadPort` and `SpeakingEvaluationReadPort` contracts
and Application cycle selector. Keep calendar schedule/date/term calculation
and display labels in the feature. Preserve `All` as empty; empty results for
no data, schedule, or read error; grade-level mapping; any non-whitespace cell,
including Unicode, making the current evaluation populated; and four-period
previous-cycle fallback, including Winter/Fall wrap. Two independent Explorer
scans disagreed on the candidate; the selected path closes a concrete v1 read
path through existing typed contracts. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-01 (F213 accepted; F214 selected)

F213, source commit `dc2b3ed9`, routes `forClass` through
`SelectedClassGradeReadPort`/`SelectedClassGradeReadQuery` and
`SpeakingEvaluationReadPort`/`SpeakingEvaluationQuery`; the production path no
longer calls legacy `ClassService` or `SpeakingEvaluationService`. Qt-free
Application owns the UTF-16 row-content policy, including U+0085 parity. The
focused target and `ClassMngrFeatures` built successfully; CTest passed 3/3,
and CMake ownership validated 1,146 files. The independent Tester caught a
U+0085 mismatch; it passed after repair. `git diff --check` passed; no full
suite ran.

F214 is selected: move class day-filter matching into Qt-free Application,
including selected-day matching, trimmed case-fold normalization,
`weekend`/`wkend` expansion, Regular/Intensive source selection, and
ActiveSchedule hide-empty behavior. Keep grouping, ordering, time formatting,
and translated labels in the feature. Acceptance covers app-less policy tests
and existing `ClassTabNavigationModelTests`. Two independent Explorer scans
disagreed on automatic-update startup eligibility; class day-filter is selected
as a bounded UI-owned rule with existing production-path tests. Update
eligibility remains a later candidate. Phase 2 remains In Progress/Open; Gates
1 and 2 remain Partial.

### Progress update - 2026-10-01 (F214 accepted; F215 selected)

F214, source commit `9e03e668`, adds Qt-free `ClassDayFilterPolicy` for
normalized-key matching, `weekend`/`wkend` aliases, OR matching,
Regular/Intensive source selection, and AllClasses/ActiveSchedule empty-filter
behavior. Qt trim/case-fold/UTF-8 conversion and UI grouping, ordering, time
formatting, and labels remain in the feature. CMake ownership validated 1,148
handwritten sources. Focused builds passed for
`ClassMngrNextApplicationClassDayFilterPolicyTests`,
`ClassMngrClassTabNavigationModelTests`, and `ClassMngrFeatures`; focused CTest
passed 2/2 in executor and independent Tester runs. `git diff --check` passed;
no full suite ran.

F215 is selected: move automatic-update startup eligibility into Qt-free
Application policy using configuration `checkOnStartup`, the typed automatic-
check preference, and release API URL availability. Preserve lifecycle,
service, and one-shot guards; run startup cleanup once before preference/URL
gates; keep disabled or unconfigured attempts retryable; and set started then
request `CheckPolicy::Force` only for an enabled, configured attempt. Reread
the preference when processing results for prompt suppression. Manual checks,
networking, downloader cleanup, dialogs, and skipped-version behavior remain
with their current owners. Acceptance requires an app-less decision matrix and
focused `UpdateController` integration coverage for startup, one-shot behavior,
retry after disabled or unconfigured attempts, and forced service dispatch. Two
independent Explorer scans agreed on moving startup eligibility into
Application. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-01 (F215 accepted; F216 selected)

F215, source commit `68ef5962`, adds the Qt-free
`automaticUpdateStartupCheckIsEligible` policy. Startup guards and maintenance
order remain intact; cleanup runs once before preference/URL gates, and only
eligible attempts set one-shot state and dispatch `CheckPolicy::Force`. The
separate preference reread for prompt suppression remains in place. Focused
targets `ClassMngrNextApplicationAutomaticUpdateStartupEligibilityTests` and
`ClassMngrUpdateControllerAutomaticStartupTests` built; CTest passed 2/2 in the
independent run. Coverage includes the eight-case decision matrix, startup
completion, Force despite a fresh result, one-shot behavior, disabled-to-
enabled retry, missing-URL non-dispatch, and cleanup order/once. `UpdateService`
configuration is immutable: later URL eligibility is covered by the policy
matrix, while controller tests verify no dispatch for a missing URL; no same-
controller URL-recovery test is claimed. No full suite ran.

F216 is selected: move skipped-update-version reconciliation and matching
policy from `UpdateController` into Qt-free Application, preserving strict
`x.x.x` parsing and comparison plus the existing clear/keep cases. Keep
settings persistence and dialog synchronization in the controller. Acceptance
requires focused app-less policy tests and controller integration tests for
preference and dialog behavior. Two independent Explorer scans agreed on this
slice, which advances Phase 2's Application-use-case alignment. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-01 (F216 accepted; F217 selected)

F216, source commit `23dc6c2b`, adds the Qt-free
`decideSkippedUpdateVersion` policy. The controller retains `Version::parse`,
UTF-8 conversion, preference writes and clears, and dialog synchronization.
The app-less eight-case matrix includes textual `01.2.3` versus `1.2.3`.
Controller tests cover stale-version clearing, prompt and open-dialog
synchronization, exact skip persistence, and suppression. The Executor's
focused build passed and CTest passed 4/4; the independent Tester build was
already current and CTest passed 2/2. `git diff --check` passed; no full suite
ran.

F217 is selected: add a Qt-free roster-score import use case and integrate it
into `RosterEditorWidget::importScores()`, replacing that operation's direct
`SpeakingEvaluationService::rosterScoreImport` dependency with the existing
`SpeakingEvaluationReadQuery`/`SpeakingEvaluationReadPort`. Preserve trimmed
complete English/Korean pair matching, exact pair semantics and last-duplicate
wins; parse six score labels using existing Domain grade conversion and
six-component overall-grade rounding; return `N/A` for incomplete or unknown
grades. Keep evaluation-column selection, model updates, no-match/error
behavior, missing-column handling, and idempotence in the UI. Acceptance
requires app-less use-case tests and the existing registered
`RosterEditorWidgetImport` UI test target. Two independent Explorer scans
disagreed on the top candidate; Explorer A identified this production path and
its existing typed read contract and UI coverage. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-01 (F217 accepted; F218 selected)

F217, source commit `b587a6d5`, adds Qt-free
`SpeakingEvaluationRosterScoreImportUseCase` using the existing typed speaking-
evaluation query and read port. `qt_compatible_text.h` extracts the exact
QString-compatible whitespace rule, and `ClassDetailsValidationPolicy`
delegates to it. The feature no longer calls
`SpeakingEvaluationService::rosterScoreImport`; the legacy compatibility API
remains. App-less tests cover trimming, source order, grade labels, rounding
below/above 0.4, malformed/short/missing-name rows, unknown/incomplete grades
as `N/A`, read failure, identity mismatch, and empty reads. UI tests preserve
duplicate last-wins, pair matching, persistence, no-data/error behavior,
idempotence, and required-column handling; they add
`missingOptionalEvaluationColumnDoesNotBlockAvailableImports`. The Executor's
focused build and CTest passed 5/5 with source ownership validation. The
independent Tester passed 4/4, plus the class-details whitespace regression
1/1 and optional-column recheck 1/1. `git diff --check` passed; no full suite
ran.

F218 is selected: route `SpeakingEvalPage::loadEvaluations()` class-list reads
through `ClassesListReadQuery`/`ApplicationServicesClassesListReadPort`, and
`rebuildClassTabs()` metadata/teacher reads through
`ClassesNavigationSnapshotQueryHandler`/`ApplicationServicesClassesNavigationReadPort`.
Keep `ClassTabNavigation` construction/presentation, preference handling,
evaluation grid, and Qt conversion at the feature edge. Preserve list order,
tab labels/metadata, selection fallback/retention, schedule and visibility
filters, disabled/unavailable behavior, class-list warning/clearing, and
name-only tabs when per-class metadata/teacher reads fail. Extend
`SpeakingEvalPageSave` with assertions for tab order/selection and list or
snapshot failure/fallback. Existing focused query, list-port, and navigation-
port targets remain acceptance; add the app-page integration coverage. Two
Explorers and three Investigators reviewed candidates: two recommendations
favored broader Schedule Import validation for gate value, while the narrower
Speaking Evaluation cutover is selected for complete typed contracts, existing
focused page tests, and bounded scope. Schedule Import remains a later
gate-focused follow-up; F218 does not close a gate. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-01 (F218 accepted; F219 selected)

F218, source commit `9bb936ee`, moves Speaking Evaluation class-list,
per-class navigation metadata, and selected-class subtitle reads to existing
Application queries and Platform ports. It preserves tab order, labels, and
metadata; selected-class fallback/retention; Regular/Intensive schedule and
visibility filtering; absent/closed session behavior; warnings on open-session
list-read failure; name-only fallback on failed navigation metadata; and
rendered subtitle parity. Independent focused verification passed all eight
CTest targets: `ClassMngrSpeakingEvalPageSaveTests`,
`ClassMngrNextApplicationClassesListReadQueryTests`,
`ClassMngrNextApplicationClassesNavigationSnapshotTests`,
`ClassMngrNextApplicationSelectedClassSubtitleReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesClassesListReadPortTests`,
`ClassMngrNextPlatformApplicationServicesClassesNavigationReadPortTests`,
`ClassMngrNextPlatformApplicationServicesSelectedClassSubtitleReadPortTests`,
and `ClassMngrClassTabNavigationModelTests`. The filter-integration coverage
gap was closed before acceptance. `git diff --check` passed; no full suite ran.

F219 is selected: add `validateScheduleImportState()` as a live-state preflight
in `ScheduleImportReviewDialog::updateReviewState()` after review-decision
checks. Preserve status priority, form and duplicate-decision validation, full
overlap-warning details, targetless Skip, intensive-preservation behavior, and
the repository's final pre-write check. Reject ambiguous positive-target Skip
before the dialog appears ready. Acceptance targets are
`NextApplicationScheduleImportStateValidation`,
`ClassMngrNextApplicationScheduleImportReviewDecisionsTests`,
`ClassMngrScheduleImportDialogTests`, and `ClassMngrScheduleImportTests`.
Two Explorers recommended this gate-focused integration; three independent
Investigators agreed on the minimal live-state preflight after decision
validation while retaining dialog warnings and repository apply validation.
F219 advances Gate 1; Gate 2 remains Partial. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-02 (F219 accepted; F220 selected)

F219, source commit `57aefadf`, wires Qt-free
`validateScheduleImportState()` as a live review-readiness preflight after
review decisions. It preserves status priority, detailed overlap warnings,
targetless Skip, intensive preservation, and the repository's final apply-time
check. Dialog tests cover rejecting ambiguous positive-target Skip and
accepting unique exact Skip and targetless Skip. Independent Application
state-validation, review-decision, and `ClassMngrScheduleImportTests` passed;
`git diff --check` passed. No full suite ran.

`ClassMngrScheduleImportDialogTests` has three confirmed pre-existing failures:
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. F219 produced 21 pass/3 fail;
a clean archived F218 baseline at `9bb936ee` produced 18 pass/3 fail. These
are a known gate issue, not F219 regressions; the dialog target is not claimed
as passing.

F220 is selected: move intrinsic `ScheduleImportPlanValidator` eligibility
rules into a Qt-free Application contract, retaining the feature validator as
a Qt adapter that translates values and errors. Preserve exact first-error
order: intensive mode, diagnostics acknowledgment, review decisions,
all-candidate basics, then per-candidate meeting-pattern and colors; Skip
exempts only pattern and color. Keep parsing, presentation, and repository
apply guards with their current owners. Add app-less target
`NextApplicationScheduleImportPlanValidation` and retain
`ClassMngrScheduleImportTests` integration coverage. A typed current-state
snapshot-read contract remains later work. F219 advances Gate 1; Gate 2
remains Partial. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### Progress update - 2026-10-02 (F220 accepted; F221 selected)

F220, source commit `4a87ab1e`, moves Schedule Import plan eligibility into a
Qt-free Application contract with a feature adapter, preserving validation
order, localized errors, and Skip exemptions. Independent focused build/CTest
passed 2/2: `ClassMngrNextApplicationScheduleImportPlanValidationTests` and
`ClassMngrScheduleImportTests`. `git diff --check` passed; no full suite ran.
F220 advances Gate 1; Gate 2 remains Partial.

F221 is selected: add a typed Application query and Platform adapter for the
live Schedule Import current-state snapshots used by F219 review readiness.
Move only the snapshot source in
`ScheduleImportReviewDialog::updateReviewState()` to the active-session typed
read boundary. Keep resolution-control/presentation lookups, matching, and
repository apply-time validation in their current owners. Preserve snapshot
identity/data semantics and validator/status priority. Use structured
unavailable/read failures without legacy-service fallback for absent/closed
sessions or repository read failures. Acceptance requires app-less
success/error tests; Platform mapping, read-error, unavailable, and
closed-session tests; dialog integration for fresh preflight snapshots and
error priority; and continued `ClassMngrScheduleImportTests` apply-time
coverage. `ClassMngrScheduleImportDialogTests` has the same three baseline
failures confirmed at F218 and F219 (18/3 and 21/3); don't claim it passes and
compare baseline if used. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-02 (F221 accepted; F222 selected)

F221, source commit `ec65c0c6`, adds the Qt-free Application current-state
snapshot query and active-session Platform adapter. Schedule Import review
state now uses the typed snapshot for validation/readiness, conflict labels,
schedule preservation, preview projection, and cleared counts. Repository
class/teacher order is preserved, with no legacy fallback. Focused Application
snapshot, Platform snapshot, and `ClassMngrScheduleImportTests` CTests passed.
`ClassMngrScheduleImportDialogTests` passed 23 and failed the same three
baseline cases: `acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`; both new dialog tests passed.
`git diff --check` passed; no full suite ran. F221 advances Gate 1; Gate 2
remains Partial.

F222 is selected: populate Schedule Import resolution controls from the F221
typed snapshot, removing `ClassService` and `TeacherService` reads from choice
construction while retaining widget creation and presentation in the feature
UI. Preserve choice order, labels, room matching, suggested/exact and
supplemental eligible class choices, and action data/defaults. Keep
`scheduleImportClassOptionIsEligible()`, `ScheduleService::previewImport()`,
matching semantics, and repository apply validation with their current
owners. Snapshot failure must be explicit, with no legacy fallback or partial
controls. Acceptance covers option contents/order/defaults and failure cases;
reruns the Application and Platform snapshot targets plus
`ClassMngrScheduleImportTests`; and compares dialog results with the documented
baseline without claiming a pass if the same three failures remain. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-02 (F222 accepted; F223 selected)

F222, source commit `6920e019`, populates Schedule Import resolution controls
from the F221 typed snapshot and removes legacy class/teacher service reads
from choice construction. Independent review passed. Application state
snapshot, Platform state snapshot, and `ClassMngrScheduleImportTests` passed.
`ClassMngrScheduleImportDialogTests` passed 24 and failed the same documented
baseline cases: `acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. `git diff --check` passed;
no full suite ran. No direct test covers a successful snapshot with a missing
preview reference, a narrow remaining coverage gap.

F223 is selected: migrate Schedule Import review matching to the existing
Qt-free `projectScheduleImportMatching` projection using the F221 typed
snapshot. Add `roomNumber` to the snapshot, mapped from Platform `ClassInfo`,
because F221's projection omits it. Preserve Qt
`simplified().toCaseFolded()` normalization, conversion ordering, suggestions,
confidence, and explanation. Leave workbook parsing, apply-time validation,
and matching policy unchanged. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-02 (F223 accepted; F224 selected)

F223, source commit `4b46adc7`, routes Schedule Import review matching through
the Qt-free `projectScheduleImportMatching` projection and F221 typed snapshot.
The independent Tester built all four focused targets: Application matching
projection and Platform snapshot-port tests passed; Schedule Import repository
tests passed 54 with one sample-gated skip; and the dialog target passed 27
with only the three established baseline failures
(`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`). The duplicate-target test
verified both initial targets were 44 before the modal warning. `git diff --check`
passed; no full suite ran.

F224 is selected: remove the legacy `openScheduleImportService()` availability
guard from `ScheduleImportReviewDialog::prepare()`. Review readiness and
failures now come from the F221 typed state snapshot. Preserve the
apply-time `ScheduleService::importSchedule()` write path, workbook parsing,
matching and eligibility, validation, and repository-read failure handling.
Add focused regression coverage showing that a missing active session follows
the typed snapshot failure path, builds no controls, and does not call
`previewImport()`. Acceptance reruns the Application matching-projection,
Platform snapshot-port, Schedule Import repository, and Dialog focused targets;
only the three named Dialog baseline failures are expected, and
`git diff --check` must pass. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-02 (F226 accepted; F227 selected)

F226, source commit `6c6210b0`, adds a Qt-free Schedule Import review-readiness
use case that validates review decisions before optional state validation. The
dialog uses it once, preserves duplicate-conflict/message priority, and makes
no additional snapshot read. Independent readiness, decision, state-validation,
and Schedule Import repository CTests passed 4/4. The Dialog target had 29
passed with only the three documented baseline failures
(`acceptedReviewCanTearDownSourceDialog`, `mismatchedProfileRequiresConfirmation`,
and `reviewPreviewUsesSavedScheduleDisplaySettings`). `git diff --check` passed;
no full suite ran.

F227 is selected: move teacher/class action counts, ignored-diagnostic count,
and schedules-cleared count from legacy `ScheduleImportReviewSummaryBuilder`
into a Qt-free Application proposed-summary projection, using typed
`ScheduleImportApplyRequest` plus the UI-derived schedules-cleared count.
Preserve count semantics and localized formatting in UI; leave preview
projection, snapshot cadence, and repository apply validation unchanged. Add
app-less summary tests and dialog summary parity; run summary, dialog, and
repository targets, expecting only the three named dialog baselines, plus
`git diff --check`. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### Progress update - 2026-10-02 (F227 accepted; F228 selected)

F227, source commit `cb61f2ab`, moves the proposed Schedule Import summary to
a Qt-free Application projection over `ScheduleImportReviewDecisionRequest`,
ignored-diagnostic count, and the UI-computed `schedulesCleared` count. It
counts teacher Create, UpdateRoom, and Skip plus class CreateNew,
UpdateExisting, and Skip; Reuse, Unselected, and Invalid actions are ignored.
The dialog retains localized summary formatting and the separate apply-result
message. Independent projection and repository CTests passed 2/2. The direct
offscreen Dialog run passed 30 and failed only the three recorded baselines:
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. `git diff --check` passed;
no full suite ran.

F228 is selected: move only the existing-schedules-cleared review count into
a Qt-free Application projection over the typed current-state snapshot and
selected class targets. Count classes with hours in the selected schedule
that have no selected target; preserve zero for absent intensive classes in
preserve mode and zero when no snapshot is available. Keep localized
formatting in the dialog; leave snapshot cadence, preview construction, and
actual apply behavior unchanged. Acceptance covers normal/intensive
selection, target exclusion, empty schedules, intensive preserve mode, and
dialog summary parity. Rerun the focused projection and dialog targets, with
only the three established Dialog baseline failures expected; verify source
ownership and `git diff --check`. No full suite is planned.

### Progress update - 2026-10-02 (F228 accepted; F229 selected)

F228, source commit `d90f475f`, adds the Qt-free
`projectScheduleImportSchedulesCleared()` projection over the typed state
snapshot and review decisions. It preserves full-parse numeric ID matching,
counts classes with selected-kind hours that have no selected target, and
returns zero in intensive preserve mode. The dialog calls it only with an
available snapshot, retaining zero when no snapshot is available. The
Application projection CTest passed 1/1. Dialog reported 30 passed and only
the three established baselines:
`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`. Both the nonzero summary
parity assertion and no-snapshot zero-count assertion passed. Independent
source ownership validation found one owner for 1,170 handwritten files;
`git diff --check` passed. No full suite ran.

F229 was selected after two independent Explorer lanes. Reuse the existing
`projectScheduleImportStateSchedules()` projection to drive class-schedule
selection for the review preview. Keep widget and Qt preview-row construction
and conversion, teacher-room and color enrichment, displayed order, and
translated conflict messages in feature UI. Preserve skipped-target schedule
retention, incomplete-resolution behavior, intensive preservation of
untargeted classes, snapshot-failure fallback, and read cadence. Acceptance
compares preview rows and conflict ordering against existing dialog behavior
and adds focused parity coverage. Run
`NextApplicationScheduleImportStateValidation`,
`NextApplicationScheduleImportStateSnapshot` if affected,
`ClassMngrScheduleImportTests`, and `ClassMngrScheduleImportDialogTests`; allow
only the three named Dialog baselines. Verify source ownership and
`git diff --check`; no full suite.

### Progress update - 2026-10-02 (F229 accepted; F230 selected)

F229, source commit `e8a2a5b5`, reuses
`projectScheduleImportStateSchedules()` for review-preview membership and
schedule preservation. The dialog retains control order, snapshot Skip times,
and Qt row construction, enrichment, and conflict formatting. Focused
Application state-validation and Schedule Import repository targets passed.
The Dialog target reported 32 passed and only the three established baseline
failures (`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`); both new parity slots
passed. Source ownership found one owner for 1,170 handwritten files, and
`git diff --check` passed. No full suite ran.

F230 is selected based on two independent Explorer reports: have
`ScheduleImportReviewDialog::applyImport()` build and pass the typed
`ScheduleImportApplyRequest` directly to the Application use case. Remove the
legacy `ScheduleImportPlan` construction and plan-to-request conversion at the
UI/Application boundary; retain typed-to-legacy conversion inside the existing
service persistence boundary. Preserve the complete field mapping and typed
IDs, confirmation-before-write timing, current detailed policy-error text from
the legacy validator, and repository apply validation. Verify request mapping
and confirmation through
`applyUsesConfirmationAndReportsServiceOutcome`, the Application apply use
case, Platform apply port, Schedule Import repository, and Dialog targets;
only the three established Dialog baselines are expected. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-02 (F230 accepted; F231 selected)

F230, source commit `547ccb5b`, has the dialog build and pass the typed
`ScheduleImportApplyRequest` directly to the Application use case, removing
legacy-plan construction at the UI/Application boundary while retaining
typed-to-legacy conversion at the service persistence boundary. Focused
Application ApplyUseCase, Platform apply-port, and Schedule Import repository
CTests passed. The Dialog target reported 34 passed and only the three
established baseline failures (`acceptedReviewCanTearDownSourceDialog`,
`mismatchedProfileRequiresConfirmation`, and
`reviewPreviewUsesSavedScheduleDisplaySettings`). New confirmation/mapping,
typed request-model mapping, and legacy policy-text parity slots passed;
`git diff --check` passed.

F231 is selected based on two independent Explorer reports: add an Application
projection from `ScheduleImportApplyRequest` to
`ScheduleImportReviewDecisionRequest`, then reuse it in ApplyUseCase and dialog
review readiness/summary. Preserve candidate and resolution order,
UTF-16/UTF-8 conversion, imported-room normalization and empty omission, typed
class IDs, skipped-teacher auto-skip behavior, summary/presentation,
confirmation timing, and snapshot cadence. Keep state-validation request
assembly in the UI; defer the broader repository migration. Add app-less tests
for non-ASCII data, typed targets, action/order, and empty rooms. Run the
projection, apply-use-case, review-decision/readiness/summary, and Dialog
targets; allow only the three named Dialog baselines. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-02 (F231 accepted; F232 selected)

F231, source commit `6ebf6d33`, adds the Qt-free apply-request decision
projection used by ApplyUseCase and dialog readiness/summary. Its five focused
Application CTests passed 1/1 each. Dialog reported 34 passed and the same
three established baseline failures; the changed summary/teacher-skip slot,
confirmation outcome, and snapshot checks passed. Source ownership found one
owner for 1,172 files; `git diff --check` passed. No full suite ran.

### Progress update - 2026-10-02 (F232 accepted; F233 selected)

F232, source commit `075b4335`, routes the typed v2 Apply port directly to the
active session's Schedule Import repository. `applyTyped()` maps the typed
request into the existing plan-based transactional core. The v2 route no
longer calls `ScheduleService` or the legacy `DataService` fallback; the v1
`ScheduleImportPlan` service/repository path remains. Focused CTests passed
3/3: Application ApplyUseCase, Platform ApplyPort, and
`ClassMngrScheduleImportTests`. The focused dialog confirmation slot
`applyUsesConfirmationAndReportsServiceOutcome` passed. Coverage includes typed
success and persisted summary, missing/closed session, retained legacy service
route, stale-target and overlap rejection before writes, forced write-failure
rollback, and intensive-mode and slot-state mapping. Configure/source-ownership
validation found one owner for 1,172 handwritten files; `git diff --check`
passed. No full suite or aggregate Dialog target ran.

F233 is selected: use the existing typed `ScheduleImportApplyRequest` as the
shared repository-core input and keep `ScheduleImportPlan` as a validated v1
edge adapter into that core. Remove the typed-v2 round-trip through the plan;
preserve legacy API behavior and validation messages/order. The typed core
must explicitly validate direct calls, preserve repository-time fresh-state
checks and rejection before writes, use one transaction/rollback path, and
retain summaries plus normal/intensive behavior. Add parity for typed and
legacy entrypoints, including malformed typed actions/modes so enum defaults
cannot silently change. ApplyRequest already carries the v2 write data; the
narrow current-state validation request is insufficient, while a second
persistence command would duplicate translation. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-02 (F236 accepted; F237 selected)

F236, source commit `5877bba0`, carries structured typed policy,
teacher-target, and fresh-state errors through Repository -> Platform ->
dialog, including fresh-state context and overlap start/end times. The dialog
uses the established formatter and a direct test confirms errors remain visible;
legacy `apply(plan)` keeps localized `QString` behavior and SQL/transaction
failures remain message-only. Windows x64 Debug verification completed 330
actions and found 1 owner for 1,174 handwritten sources. Application and
Platform CTests passed 2/2; 11 selected repository functions and 4 selected
dialog functions passed, including the visibility assertion. `git diff --check`
passed. No full suite ran.

F237 is selected: move Speaking Evaluation normalization and content validation
into a Qt-free contract shared by the save use case and page feedback, advancing
the broader evaluation-editing Gate 1 gap. Acceptance preserves baseline score
aliases, name normalization, issue codes/locations/messages, empty-row,
duplicate, name, score, comment, and note behavior, the Korean-name-length
option, and changed-cell behavior. Invalid requests must not reach the port;
active-session persistence, rollback, and representative UI/save behavior must
remain intact. Compare representative cases with the legacy baseline and run
focused Application, Platform, and page-save tests. Two Explorer candidates
disagreed; this seam was selected because broader evaluation editing is named
as a Gate 1 gap. Class Transfer typed apply is deferred given its wider
transaction surface and lack of a more directly named current gap. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-02 (F237 accepted; F238 selected)

F237, source commit `8ef76074`, shares Qt-free Speaking Evaluation
normalization/content validation between the save use case and page feedback
adapter; Platform persists normalized input and invalid data is blocked. Fresh
x64 Debug verification completed 323 steps; source ownership found 1 owner for
1,177 handwritten sources. Focused Application, Platform, and page-save tests
passed 9/9, 10/10 after parity follow-up, and 14/14; CTest passed 3/3 before the
follow-up, and Korean-name differential/page-decline slots passed 3/3 each.
False and true questionable-name settings match legacy code/location/severity
and error outcome, with port suppression/forwarding; declining confirmation
leaves stored state unchanged. `git diff --check` passed. No full suite ran.

F238 is selected: add a Qt-free Application contract for initial-setup profile
replacement that preserves the original during setup, finalizes on success,
and restores on cancellation, with file operations in Platform. Preserve
warnings/messages, recent-file updates, no-overwrite behavior, and recoverable
original profiles after create/restore failure. Acceptance: app-less operation
and failure-ordering tests; adapter create/restore failure with original
survival; and FileController/MainWindow regressions for finish, cancel,
failed creation, and recent-file state. Current implementation is in
[FileController](../../src/app/controllers/file_controller.cpp#L359); lifecycle
coverage includes [failed-close precondition](../../tests/file_controller_workspace_lifecycle_tests.cpp#L639),
[finish](../../tests/file_controller_workspace_lifecycle_tests.cpp#L695), and
[cancel](../../tests/file_controller_workspace_lifecycle_tests.cpp#L741).
The historical cumulative Gate 1 map ends at F143 and still names backup/
recovery as planned ([map](03-Phase-2-Progress-Log.md#L7570)). Two Explorer
candidates disagreed; main chose the directly named recovery gap, deferring
Teacher Import typed apply while keeping its transaction path intact. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-02 (F238 accepted; F239 selected)

F238, source commit `7b832bcc`, adds a Qt-free Application initial-setup
lifecycle with a Platform file/workspace adapter. FileController warnings,
recent-file behavior, and original-profile recovery remain intact. Independent
x64 Debug source ownership found 1 owner for 1,181 handwritten sources. CTest
passed 3/3; the Application custom runner passed 11 scenarios, Platform passed
7/7, and FileController passed 37/37. Coverage includes no-original finish,
failed creation, cancel/restore, failed remove/rename paths, recent-file timing,
and warning/recovery assertions. `git diff --check` passed. No full suite ran.

F239, source commit `d3e9cded`, extracts roster row reordering into an app-less
contract over existing `RosterSnapshot` rows. Focused x64 Debug CTest passed
3/3; source ownership found one owner for 1,183 handwritten sources, and
`git diff --check` passed. No full suite ran.

F240, source commit `4be18aa5`, extracts roster row removal into an app-less
operation over `RosterSnapshot`, reused by `RosterModel`. Independent fresh x64
Debug verification used CMake 4.4.2, MSVC 19.51, and Qt 6.12. The app-less
row-removal, RosterModel, and roster-widget CTests passed 3/3; source ownership
found one owner for 1,185 handwritten files. `git diff --check` and new-file
whitespace checks passed. No full suite ran.

F241, source commit `746ef0bb`, extracts custom roster-column name admission
into Qt-free Application policy. It preserves whitespace normalization,
Autumn-to-Fall aliasing, and empty/duplicate/required decision order, with Qt
comparison injected through an adapter. A lossless UTF-16 adapter and leading
U+FEFF regression cover row move/removal as well. Fresh x64 Debug/Ninja/MSVC
19.51/Qt 6.12 verification completed 323 actions; source ownership found one
owner for 1,188 handwritten files. Four focused CTests passed: column policy,
RosterModel, row removal, and roster-widget save. `git diff --check` and all
new-file whitespace checks passed; no full suite ran.

F242, source commit `5525cade`, factors invalid-index and required-column
removal eligibility into Qt-free Application policy, preserving Autumn-to-Fall
and Qt comparison semantics. Mutation, model notifications, validation/dirty
state, layout/width, confirmation, and autosave remain at their current owners.
Independent fresh x64 Debug/Ninja/MSVC 19.51.36257/Qt 6.12 verification used
CMake 4.4.2, reached build action 324/325, and validated one explicit owner for
1,190 handwritten sources. Five focused CTests passed: custom-column removal
policy, RosterModel, roster-editor widget save, row removal, and custom-column
name policy. `git diff --check` passed; both new files passed LF, final-newline,
and trailing-whitespace checks. No full suite ran.

F243, source commit `68260142`, adds a Qt-free custom-column append operation
over `RosterSnapshot`. It reuses F241 admission/normalization, appends the
normalized name and one empty cell per existing row, preserves rows and width
metadata, and leaves notifications, validation, dirty state, widget layout,
selection, autosave, and persistence at their current owners. Fresh Windows
x64 Debug/Ninja verification used CMake 4.4.2, MSVC 19.51.36257, and Qt 6.12;
source ownership found one owner for 1,192 handwritten sources. Five focused
CTests passed: append policy, name policy, RosterModel, roster-editor widget
save, and row removal. `git diff --check` and both new-file whitespace checks
passed; no full suite ran.

F244, source commit `86fe782f`, adds Qt-free destination-side roster transfer
preparation returning a typed rejection or compact target-row/mapped-row result.
It preserves column matching, cell normalization, empty-row/full-target/
duplicate-pair rejection order, first-empty-slot choice, incomplete-pair
allowance, and legacy U+001F duplicate-key behavior. RosterModel retains errors,
mutation, validation, signals, and dirty state; widget lookup, source removal,
width normalization, and atomic save remain at their owners. Fresh x64
Debug/Ninja/MSVC 19.51/Qt 6.12 verification found one owner for 1,194 handwritten
sources and passed six focused CTests: transfer preparation, RosterModel,
roster widget save, custom-column name, append, and row removal.
`git diff --check` and both new-file whitespace checks passed; no full suite
ran.

F245, source commit `99c95146`, moves trimmed case-sensitive English grouping
and first-unused A-Z Korean suffix selection into Qt-free Application policy.
RosterModel and SpeakingEvalModel project legacy
`StudentNameUtils::baseKoreanName()` and
`StudentNameUtils::koreanNameSuffix()` values; the lossless
UTF-16 Qt adapter now lives at the neutral UI boundary in
[`qt_text_adapter.h`](../../src/ui/shared/qt_text_adapter.h). Fresh x64
Debug/Ninja/MSVC 19.51/Qt 6.12 verification in
`build/f245_independent_x64_debug` found one owner for 1,196 handwritten files.
Four focused CTests passed: suffix policy, RosterModel, SpeakingEval page save,
and Speaking Evaluation save use case. Differential coverage includes U+3000
and unpaired-surrogate behavior, plus page-level suffix choose/apply.
`git diff --check` and new-file whitespace checks passed; no full suite ran.

F246, source commit `b1a86db2`, extracts same-grade roster transfer-target
eligibility. Fresh Windows x64 Debug/Ninja/MSVC 19.51/Qt 6.12 verification
found one owner for 1,198 handwritten sources; three focused CTests passed:
app-less eligibility, RosterEditorWidgetSave, and TestingClassesPage.
`git diff --check` passed. Coverage limitation: no focused UI assertion reaches
nonempty-grade target enumeration or checks `classInfo` call counts; policy
tests cover the logic and the widget source compiled. No full suite ran.

F247, source commit `7021e657`, extracts `SpeakingEvalPage::nameImportChanges`
into a Qt-free Application planner. It preserves Qt-compatible trimming,
U+001F pair-key/collision behavior, duplicate filtering and roster order, and
blank rows. Fresh Windows x64 Debug/Ninja/MSVC 19.51/Qt 6.12 verification found
one owner for 1,200 handwritten sources. Both focused CTests passed: the
standalone planner and SpeakingEvalPageSave. `git diff --check` passed. The page
regression checks case-insensitive first-header lookup, applied values, dirty
state, and success/already-up-to-date notices. No full suite ran. Coverage
limits: no exhaustive Qt whitespace-code-point comparison; partial editability
cannot occur in the current SpeakingEvalModel because both name cells are
editable, though per-cell checks remain at the UI boundary.

F248, source commit `8886b46f`, extracts duplicate peer-row lookup into Qt-free
Application. Fresh Windows x64 Debug/Ninja/MSVC 19.51/Qt 6.12 verification in
`build/f248_independent_x64_debug` used a CMake ownership audit: one explicit
owner for 1,202 handwritten sources. All three focused CTests passed:
`ClassMngrNextApplicationStudentNamePairLookupTests`,
`ClassMngrRosterModelTests`, and `ClassMngrSpeakingEvalPageSaveTests`.
`git diff --check` passed. No full suite ran. Coverage limits: no adapter test
for unpaired surrogates or exhaustive Qt-whitespace comparison; the missing
RosterModel name-column guard cannot be reached through public `setRoster`.

F249, source commit `828d5014`, extracts AI batch student eligibility into
Qt-free Application policy. Fresh Windows x64 Debug/Ninja/MSVC 19.51/Qt 6.12
verification in `build/f249_independent_x64_debug` used CMake ownership
validation, which found one owner for 1,204 handwritten sources. The app-less
eligibility CTest passed 1/1; dialog slots
`aiBatchDialogDisplaysEligibilityReasonsAndCheckState` and
`aiBatchDialogSelectsEligibleStudentsAndReviewsValidComments` passed
individually. `git diff --check` and new-file hygiene passed. Coverage includes
first-failure order (no name, unsupported grade, missing Did Well, then missing
Needs Improvement), either trimmed name including Korean-only, grade 4–6, both
observation lists nonempty, eligible/ineligible check and enable state,
existing-comment defaults, and page success/already-up-to-date behavior. No
full suite ran. Clipboard slots `aiPromptPreviewCopiesAnAnonymousPrompt` and
`pastedAiCommentsReplaceStudentPlaceholder` reproduced
`OleSetClipboard/OpenClipboard Failed` in the fresh environment; those paths
are outside F249, but no pre-F249 baseline or full batch CTest was run. Treat
this as a reproduced environment limitation, not a proven baseline.

F250, source commit `4753ce3b`, extracts shared first-empty roster-row
availability. Fresh `build/f250_independent_x64_debug` x64 Debug/Ninja/MSVC
19.51/Qt 6.12 verification and CMake ownership validation found one owner for
1,206 handwritten sources. Three focused CTests passed:
`ClassMngrNextApplicationRosterRowAvailabilityTests`,
`ClassMngrRosterModelTests`, and
`ClassMngrNextApplicationRosterRowTransferPreparationTests`. `git diff --check`
and new-file hygiene passed; no full suite ran. The policy trims all cells and
returns the lowest empty index or `rows.size()` sentinel; RosterModel maps a
full 25-row roster to -1. Transfer preserves source/full/duplicate rejection
order and first-destination selection.

F251, source commit `ed548438`, extracts deterministic per-comment AI review
quality. Fresh/reconfigured `build/f251_independent_x64_debug` Windows 11 x64
Debug/Ninja/MSVC 19.51/CMake 4.4.2/Qt 6.12 verification rebuilt policy/UI
targets and found one owner for 1,208 handwritten sources. The app-less
comment-quality CTest passed 1/1; UI slots
`aiBatchDialogAssessesCommentQualityAndPreservesStatuses`,
`aiBatchDialogSelectsEligibleStudentsAndReviewsValidComments`, and
`aiPromptBuilderUsesObservationsAndSelectedVoice` passed. Prompt output still
contains `between 100 and 420 characters, including spaces`. The production
420 threshold is single-sourced as `SpeakingEval::CommentPreferredMaxLength`
for prompt builder and dialog; the dialog passes existing 100/450 min/max. An
initial independent pass flagged the prompt literal; F251 centralized it, and
the final independent pass passed. The unrelated Next validation constant
`SpeakingEvaluationMaximumCommentLength=450` predates F251. `git diff --check`
and new-file hygiene passed; no full batch CTest or suite ran.

F252, source commit `6f41f8a2`, extracts Qt-free accepted AI batch comment
planning. Final independent fresh x64 Debug/Ninja verification at
`build/f252_independent_x64_debug` used MSVC 19.51, CMake 4.4.2, and Qt 6.12.
CMake ownership validation found one explicit owner for 1,210 handwritten
sources and both focused targets were built. The app-less
`ClassMngrNextApplicationSpeakingEvaluationAiBatchAcceptedCommentPlanTests`
passed 1/1; Qt slots `aiBatchDialogConfirmsAcceptedCommentOverwrites` and
`aiBatchDialogSelectsEligibleStudentsAndReviewsValidComments` passed
individually. `git diff --check` and new-file hygiene passed; no full suite ran.
The plan preserves checked/valid filtering, bounded report indexes, exact-pair
no-op skipping, report order/text, Qt-trimmed old-comment counting, typed
assignments, overwrite count, and the UI confirmation gate.

F253 is selected: extract the verbatim `splitPrivateNotes` rule shared by
`SpeakingEvalAiBatchDialog` and `SpeakingEvalPrivateNotesEditor` into a UTF-16
Qt-free Application policy. Preserve `[Did Well]\n` at the start and the first
`\n[Needs Improvement]\n` separator, body whitespace/newlines, whole-input
fallback to didWell plus empty needsImprovement when either marker is missing,
and repeated-separator behavior. Preserve exact body whitespace/newlines. Keep
`joinPrivateNotes`, bullet editing and
normalization, prompts/redaction, and observation parsing in current owners.
Add policy header and app-less tests/CMake for formatted, empty, legacy, missing,
repeated, and whitespace-exact cases; adapt both UI consumers and add focused
editor/dialog regressions. Two independent scans differed: the alternative was
AI batch response parsing. Select the narrower duplicate splitter required by
both consumers; defer parser extraction. The cumulative Gate 1 map is
historical and ends at F143
([map](03-Phase-2-Progress-Log.md#L7570)); Gates 1 and 2 remain Partial. Phase
2 remains In Progress/Open.

### Progress update - 2026-10-02 (F253 accepted; F254 selected)

F253, source commit `8975e825`, extracts the shared private-notes split rule
into a UTF-16 Qt-free Application policy used by both
`SpeakingEvalAiBatchDialog` and `SpeakingEvalPrivateNotesEditor`. It preserves
the exact opening and first separator markers, whole-input fallback when either
marker is missing, repeated later separators, and body whitespace/newlines.
Independent Windows x64 Debug verification built both focused targets; the
app-less policy CTest passed 1/1, and five existing dialog/editor UI slots
passed individually. CMake ownership is explicit, and
`git diff HEAD^ HEAD --check` passed. Exact whitespace/Unicode edge cases are
asserted at the policy layer; UI slots cover ordinary formatted and legacy
notes. The full suite did not run.

F254 is selected to move roster score-to-row matching and changed-assignment
planning from `RosterEditorWidget::importScores()` into a Qt-free Application
policy. Preserve trimmed exact English/Korean pair matching, last imported
duplicate wins, skipping incomplete and unmatched pairs, skipping unchanged
grades, roster-row order, and existing import/use-case results. Keep evaluation
column lookup, Qt model access and writes, autosave, and localized messages in
the widget. The app-less policy tests will cover duplicate, incomplete,
unmatched, unchanged, and ordered assignments; retain the focused roster score
import UI tests. AI response parsing remains deferred. Gates 1 and 2 remain
Partial, and Phase 2 remains In Progress/Open.

### Progress update - 2026-10-03 (F257 accepted; F258 selected)

F255 routes `SpeakingEvalReportDialog::aiPromptUnavailableReason()` through
the accepted Qt-free `speakingEvaluationAiBatchEligibilityReason` policy. The
dialog retains its no-selected-report guard and exact localized tooltip
messages; name, grade, Did Well, and Needs Improvement remain the ordered
first-failure checks. The focused UI test covers Korean-only names and missing-
name priority when multiple inputs are invalid. Fresh independent Windows x64
Debug/Ninja verification validated one owner for 1,214 handwritten sources,
built `ClassMngrFeatures` and
`ClassMngrSpeakingEvalBatchReportServiceTests`, and passed the focused slot
(3 passed, 0 failed including setup/cleanup). `git diff --check` passed. The
Visual Studio preset attempt hit FileTracker access failure; the independent
Ninja/MSVC build succeeded. No full suite ran.

F256 routes `SpeakingEvalPage::importNames()` and
`unmatchedRosterNamePairs()` through the existing `RosterReadUseCase` and
`ApplicationServicesRosterReadPort`, removing their direct roster-data reads.
Preserve service/class guards, empty/error fallback behavior, missing-column
warning and no-op result, F247 name-import planning, and duplicate-resolution
candidate order/filtering. Keep QString conversion and localized messaging at
the feature edge. Focused coverage includes a two-row ordered import, empty and
unavailable reads, missing columns, suffix resolution, and duplicate-location
behavior. Fresh independent Windows x64 Debug/Ninja verification with MSVC
19.51 and Qt 6.12 validated one owner for 1,214 handwritten sources, built
`ClassMngrFeatures` and `ClassMngrSpeakingEvalPageSaveTests`, and passed the six
focused QtTest slots (3 passed each including setup/cleanup); the focused page
CTest target passed 1/1. `git diff --check` passed with line-ending notices.
The build emitted non-fatal object-path warnings; the UI run logged non-fatal
resource, font, keyboard-SVG, and offscreen-size warnings. No full suite ran.
No direct nonempty candidate-list assertion was added; the candidate algorithm
is unchanged, and the ordered roster-import test exercises the same read and
conversion path. The independent tester assessed this as non-blocking. Gates 1
and 2 remain Partial.

F257 routes `SpeakingEvalPage::showReports()`,
`generateClassAiComments()`, and `outputReports()` through the existing
selected-class subtitle read contract. Preserve the service guards and default
`ClassInfo` fallback on failed class reads; keep successfully read class fields
when the optional teacher read fails; retain signature-image reads and their
guards. The existing grade, level, schedule, and teacher projection covers
these consumers; its scope comment now names report context. Fresh independent
Windows x64 Debug/Ninja verification built `ClassMngrFeatures` and
`ClassMngrSpeakingEvalPageSaveTests`; the page target passed 26/26, and the
selected CTest set passed 7/7. The batch report service reported 38 passed and
5 skipped for PowerPoint integration. `git diff --check` passed with line-ending
notices. Signature-port, signature-processor, and report-widget tests passed;
source review confirms both page call sites retain and forward signature bytes.
No new page-level signature-render assertion was retained. Non-fatal CTest
environment warnings included missing optional documents/font/keyboard
resources and offscreen size hints. No full suite ran.

F258 is selected to move the ordered stored evaluation-name vocabulary and
unknown/empty-to-Winter fallback into the existing Qt-free Application
evaluation-selection contract. Preserve the order Winter, Speech Contest,
Summer, Fall and the Spring-to-Speech-Contest mapping. Keep localized labels
and calendar `AcademicTerm` conversion at the feature edge. Extend the existing
app-less Application tests for ordered names, period mapping, and fallback;
retain the Classes UI label/order assertions. Phase 2 remains In Progress/Open.

### Progress update - 2026-10-03 (F254 accepted; F255 selected)

F254 adds the Qt-free `speakingEvaluationRosterScoreRowAssignments` policy and
routes `RosterEditorWidget::importScores()` through it. The policy trims
QString-compatible names, matches complete exact English/Korean pairs, keeps
the last imported duplicate, skips incomplete/unmatched pairs and unchanged
grades, and returns assignments in roster-row order. The widget retains column
lookup, Qt model reads/writes, successful-write counting, autosave, action
refresh, and localized messages. A fresh independent Windows x64 Debug/Ninja
build validated one explicit owner for 1,214 handwritten sources; the policy,
F217 use-case, and roster-widget import CTests passed 3/3. `git diff --check`
passed. No full suite ran.

F255 is selected to reuse the accepted
`speakingEvaluationAiBatchEligibilityReason` policy in
`SpeakingEvalReportDialog::aiPromptUnavailableReason()`. Keep the no-selected-
report guard and translated dialog messages at the UI edge; preserve name,
grade, Did Well, then Needs Improvement first-failure order, including the
current missing-editor behavior. Extend the report-dialog test with missing
name and first-failure assertions, retaining existing tooltip checks. AI
response grammar and observation parsing remain deferred. Gates 1 and 2 remain
Partial, and Phase 2 remains In Progress/Open.

### Progress update - 2026-10-03 (F258 accepted; F259 selected)

F258 centralizes the ordered stored evaluation names and exact Winter fallback
in the Qt-free Application evaluation-selection contract. AcademicTerm mapping
and localized labels remain at the feature edge. Fresh Windows x64
Debug/Ninja verification built ClassMngrFeatures and the app-less evaluation,
Classes page, and Speaking Evaluation page test targets. The app-less contract
test passed 1/1; the focused Classes UI name/order test, Speaking Evaluation
switch test, and existing combined Classes slot all passed. The combined slot's
student-row fixture now seeds SpeakingEvalRepository, and its existing
assertions remain unchanged. git diff --check passed. Qt emitted non-fatal
resource/font/offscreen warnings; no full suite ran.

F259 is selected to route roster-transfer source/target metadata reads in
RosterEditorWidget::showRosterContextMenu() through the accepted
SelectedClassSubtitleReadQuery. Preserve same-grade eligibility, empty-grade
exclusion, menu labels and ordering, roster-full checks, class-list loading,
and transfer writes. Add focused UI coverage for same/different grade,
target labels/order, and read-failure fallback while retaining F246 app-less
eligibility and subtitle-read tests. Defer typed transfer apply. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F259 accepted; F260 selected)

F259 routes roster-transfer source and target metadata reads through the
accepted SelectedClassSubtitleReadQuery. Class-list loading, roster-full checks,
same-grade eligibility, sorting, menu text, fallback names, and transfer writes
remain at their existing owners. Fresh Windows x64 Debug/Ninja verification
built ClassMngrFeatures, the new transfer-menu UI target, and the existing
eligibility/subtitle query/adapter targets. The three focused UI cases and the
selected CTest set passed 4/4, including sorted same-grade labels, different
and empty-grade exclusion, teacher-read failure retaining class fields with
the No Teacher label, and class-fields read failure. git diff --check passed.
Non-fatal configure and offscreen/resource warnings occurred; no full suite ran.
The focused UI tests cover menu contents and metadata failures, not activating
a transfer action.

F260 is selected to centralize Qt-free roster evaluation-column
classification. Reuse the canonical stored names with the legacy Autumn alias;
preserve case-insensitive exact matching without trimming and keep Qt
comparison at the Roster UI edge. Extend the app-less evaluation contract tests
for the four canonical names, Autumn, case variants, unknowns, and
whitespace-padded values. Add a RosterModel editability regression for
read-only evaluation columns and editable custom columns, retaining the
existing Autumn header/removal behavior and custom-column normalization. Phase
2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F260 accepted; F261 selected)

F260 centralizes roster evaluation-column classification in a Qt-free
Application policy that reuses F258's canonical names and recognizes the
legacy Autumn alias. The caller supplies case-insensitive comparison semantics;
matching remains exact and does not trim. Roster grouping, width, and
editability use the shared policy, while Autumn-to-Fall normalization and
header behavior remain unchanged. Fresh Windows x64 Debug/Ninja verification
built ClassMngrFeatures and the app-less evaluation and RosterModel targets.
Both focused CTests passed; the two model slots passed, and git diff --check
passed. Configure emitted non-fatal vswhere, Vulkan-header, and object-path
warnings. No full suite ran.

F261 is selected to route RosterPrintDialog::loadClasses() class and teacher
metadata reads through the accepted SelectedClassSubtitleReadQuery. Preserve
the service-availability guards, class-list order, formatted labels,
current-class selection and checked state, and the currentClassOnly
TestingClass path. Preserve default-class formatting on class-field read
failure and class details with the default No Teacher label when the optional
teacher read fails. Add focused dialog UI coverage for labels/order/IDs/check
state and both metadata failure outcomes, retaining the selected-class query
and adapter tests. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### Progress update - 2026-10-03 (F261 accepted; F262 selected)

F261 routes RosterPrintDialog normal class-label metadata reads through the
accepted SelectedClassSubtitleReadQuery. The dialog retains its service
availability guards, list order, item IDs and checked state, current-class
display projection, and currentClassOnly TestingClass path. Fresh Windows x64
Debug/Ninja verification built ClassMngrFeatures, the new dialog UI target, and
the selected-class subtitle query/adapter targets. Four focused UI cases and
the selected CTest set passed 3/3. Coverage includes class read failure using
the former default ClassInfo/Teacher formatting and teacher read failure
preserving class fields with the No Teacher label. git diff --check passed.
Non-fatal configure/offscreen/resource warnings occurred; no full suite ran.
PDF generation was not exercised.

F262 is selected to route only the assigned-class label reads in
ClassImportDialog and ClassExportDialog through the accepted
SelectedClassSubtitleReadQuery. Pass ApplicationServices to the dialogs for
the query adapter while retaining feature-service uses for class lists and
other existing operations. Preserve SidebarNodeNaming output, classroom-name
and Class-N fallbacks, export sorting/IDs/check state, import replacement
choices, teacher matching, and transfer planning. Keep the separate arbitrary
destination-teacher lookup and typed transfer behavior unchanged. Extend the
existing class-transfer UI tests for labels, ordering, IDs/checks, and read
failures while retaining selected-class query and adapter tests. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F262 accepted; F263 candidate discovery)

F262 routes assigned-class label metadata reads in ClassImportDialog and
ClassExportDialog through the accepted SelectedClassSubtitleReadQuery.
ApplicationServices is an explicit dialog input; class-list loading, import
teacher matching, transfer planning, and the arbitrary destination-teacher
lookup remain at their prior owners. SidebarNodeNaming output, fallbacks,
export sorting/IDs/check state, and import replacement choices are preserved.
Fresh Windows x64 Debug/Ninja verification built ClassMngrFeatures, the class
transfer UI target, and the selected-class subtitle query/adapter targets.
Nine focused QtTest slots passed (11 passes including init/cleanup), the
selected query/adapter CTests passed 2/2, and git diff --check passed. Added
coverage checks visible labels and metadata-read failures while retaining
existing transfer assertions. Non-fatal configure/resource/font warnings
occurred; no full suite ran.

F263 candidate discovery is underway; Phase 2 remains In Progress/Open and
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F262 accepted; F263 selected)

F263 is selected to route only ClassImportDialog's matched-teacher display
reads through the accepted TeacherProfileReadQuery and its ApplicationServices
adapter. Preserve SidebarNodeNaming formatting and the current New Teacher /
Teacher-ID fallback behavior, Keep local and Replace local choices and IDs,
teacher matching, transfer planning, and F262's class-label read. Add visible
label assertions for single and ambiguous matches plus a profile-read failure
case; retain query and adapter tests. Phase 2 remains In Progress/Open; Gates
1 and 2 remain Partial.

### Progress update - 2026-10-03 (F263 accepted; F264 candidate discovery)

F263 routes only ClassImportDialog's matched-teacher display reads through the
accepted TeacherProfileReadQuery and ApplicationServices adapter. It maps the
four display-name fields into the existing formatter and preserves current
New Teacher / Teacher-ID fallback behavior, Keep local and Replace local
choices and IDs, teacher matching, transfer planning, and F262's class-label
read. Fresh Windows x64 Debug/Ninja verification built ClassMngrFeatures, the
ClassTransfer UI test target, and TeacherProfileReadQuery plus adapter test
targets. Ten focused ClassTransfer slots passed (12 QtTest passes including
setup/cleanup), the query/adapter CTests passed 2/2, and global/test-file
git diff --check passed. Coverage checks single and ambiguous labels, choice
IDs, and a profile-read failure while retaining existing plan assertions. The
unique build tree was removed after logs were preserved under
`build/p2_f263_verify_logs/`. Non-fatal vswhere, Vulkan-header, object-path,
resource, and font warnings occurred; no full suite ran.

F264 candidate discovery is underway; Phase 2 remains In Progress/Open and
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F263 accepted; F264 selected)

F264 is selected to route only the optional per-class roster reads in
RosterPrintDialog::updateExtraInfoColumns() through RosterReadUseCase and the
accepted ApplicationServicesRosterReadPort. Keep the existing class/scope
resolution, service-availability guards, extra-column order, checked-column
restoration, and empty-roster behavior on read failure. Project only the
snapshot columns needed by availablePerClassExtraInfoColumns(), not roster
rows or widths. Extend the existing UI target for discovered columns,
checkbox restoration, and a failed-read case; retain roster query and adapter
tests. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F264 accepted; F265 candidate discovery)

F264 routes only the optional per-class roster reads in
RosterPrintDialog::updateExtraInfoColumns() through RosterReadUseCase and the
accepted ApplicationServicesRosterReadPort. It projects only snapshot columns
needed by availablePerClassExtraInfoColumns(), not roster rows or widths. Class
resolution/list failures, service-availability and layout guards, extra-column
order, checked-column restoration, preview behavior, and empty-roster fallback
remain intact. Fresh Windows x64 Debug/Ninja verification built
ClassMngrFeatures, the RosterPrintDialog UI test target, and roster read-use
case/adapter test targets. Six focused UI slots passed (8 QtTest passes
including setup/cleanup), the roster query/adapter CTests passed 2/2, and
git diff --check passed; a direct whitespace check covered the untracked UI
test file. The unique build tree was removed after logs were preserved under
`build/p2_f264_verify_logs/`. Non-fatal vswhere, Vulkan-header, object-path,
resource, and font warnings occurred; no full suite ran.

F265 candidate discovery is underway; Phase 2 remains In Progress/Open and
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F264 accepted; F265 selected)

F265 is selected to route only the target-roster read used for fullness in
RosterEditorWidget::showRosterContextMenu() through RosterReadUseCase and the
accepted ApplicationServicesRosterReadPort. Preserve F259's metadata path,
same-grade filtering, labels, sort order, and failed-read empty-roster
fallback. Continue to use RosterModel::setRoster() and firstEmptyRow() so the
25-row UI padding determines fullness; project snapshot columns and rows into
the UI Roster value and leave widths at their prior default. Keep the separate
transferRosterRow() read/write path unchanged. Add focused menu coverage for a
full disabled target and a failed target read; retain app-less query and
adapter tests. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F265 accepted; F266 candidate discovery)

F265 routes only the target-roster read used for fullness in
RosterEditorWidget::showRosterContextMenu() through RosterReadUseCase and the
accepted ApplicationServicesRosterReadPort. The UI projection preserves
snapshot columns and rows while leaving column widths at their prior default;
RosterModel::setRoster() still pads to 25 rows before firstEmptyRow() decides
fullness. F259 metadata, same-grade filtering, labels/order, and the separate
transferRosterRow() read/write flow remain unchanged. Fresh Windows x64
Debug/Ninja verification built ClassMngrFeatures, the transfer-menu UI target,
and roster query/adapter targets. All five focused menu slots passed; the menu
CTest passed 1/1 and the roster query/adapter CTests passed 2/2. git diff
--check and direct whitespace inspection passed. The unique build tree was
removed after logs were preserved under `build/p2_f265_verify_logs/`.
Non-fatal vswhere, Vulkan-header, object-path, resource, font, and platform
plugin warnings occurred; no full suite ran.

F266 is selected to route only the class-list read in ClassExportDialog
through the accepted ClassesListReadQuery and
ApplicationServicesClassesListReadPort. Preserve the current service-presence
guard, F262's selected-class label query and fallbacks, QCollator sorting,
IDs, unchecked initial items, export-button behavior, and export flow. On list
read failure, keep the existing warning, empty list, and disabled Export
button; the list itself remains enabled. Add focused failure coverage and
retain the class-transfer sorting/selection tests plus list query/adapter
tests. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F266 accepted)

F266 routes only the class-list read in ClassExportDialog through the accepted
ClassesListReadQuery and ApplicationServicesClassesListReadPort. Class IDs and
UTF-16 names are projected at the UI edge. The service-presence guard, F262
selected-class label query and fallbacks, QCollator ordering, IDs, unchecked
initial items, Export enablement, and export flow remain unchanged. On a read
failure, the existing warning is preserved with structured error details; the
list stays empty and enabled, and Export stays disabled. A focused test drops
the classes table and captures the warning and dialog state. Fresh Windows x64
Debug/Ninja verification built ClassMngrFeatures, ClassMngrClassTransferTests,
and the classes-list query and adapter test targets. Four focused export list
slots passed (6 QtTest passes including setup and cleanup); the query and
adapter CTests passed 2/2. git diff --check passed. The unique build tree was
removed after logs were preserved under `build/p2_f266_verify_logs/`. Non-fatal
Vulkan, zlib fallback, vswhere, resource/font, and offscreen size-hint warnings
occurred; the full suite was not run. Phase 2 remains In Progress/Open; Gates 1
and 2 remain Partial.

### Progress update - 2026-10-03 (F267 accepted)

F267 routes only the normal class-list read in
RosterPrintDialog::loadClasses() through the accepted ClassesListReadQuery and
ApplicationServicesClassesListReadPort. ApplicationServices creates the
feature services and the list adapter from the same DatabaseSession, so the
current service-availability guards retain their active-session invariant.
The current service-availability guards, repository order, class IDs, checked
state, selected-class label query, warning and empty-list failure behavior,
and current-class-only TestingClass branch are preserved. The new UI test
drops the classes table, captures the warning details, and verifies the class
list is empty and enabled. Fresh Windows x64 Debug/Ninja verification built
ClassMngrFeatures, ClassMngrRosterPrintDialogTests, and the list query/adapter
test targets. The
focused RosterPrintDialog CTest and both supporting CTests passed 3/3; direct
execution of the new QtTest slot passed 3/3 including setup and cleanup.
git diff --check and direct trailing-whitespace inspection passed. The unique
build tree was removed after logs were preserved under
`build/p2_f267_verify_logs/`. Non-fatal Vulkan, zlib fallback, vswhere, and
offscreen Qt resource/font/size-hint warnings occurred; the full suite and
other platforms were not run. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-03 (F268 accepted)

F268 routes only the class-list read in MyClassesPage::rebuildClassInformation()
through the accepted ClassesListReadQuery and
ApplicationServicesClassesListReadPort. Typed class IDs and UTF-16 names are
projected into the existing Classroom inputs. The service guards,
clear-before-read order, failure warning and return, empty-list state,
per-class detail/count/teacher reads, class order, visible titles, navigation,
and selected-class restoration remain intact. A new focused page test target
covers ordering and labels, selection restoration, empty data, and a failed
list read after existing content was rendered. Fresh Windows x64 Debug/Ninja
verification passed CMake source-ownership validation and built
ClassMngrFeatures, ClassMngrMyClassesPageTests, and the list query/adapter test
targets. The MyClassesPage CTest and both supporting CTests passed 3/3; direct
execution of all three UI cases passed 5/5 including setup and cleanup.
git diff --check and direct test-file whitespace inspection passed. The unique
build tree was removed after logs were preserved under
`build/p2_f268_verify_logs/`. Non-fatal vswhere, Vulkan, zlib fallback, and
offscreen Qt resource/font/size-hint warnings occurred; the full suite and
other platforms were not run. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-03 (F268 accepted; F269 selected)

F269 is selected to route only the teacher-choice list read in
ClassDetailsWizardPage::initializePage() through a new Qt-free
InitialSetupTeacherChoicesReadQuery and active-session Platform adapter. Its
snapshot will carry typed teacher IDs and the UTF-16 name fields used by
Teacher::preferredDisplayName(). Preserve repository order, display labels,
single-teacher auto-selection, the existing `Load Teachers` warning, and the
empty combo on failure. FileController creates the initial setup database
before launching the wizard, so the active-session adapter matches this path;
keep the existing teacher-service availability guard and leave the separate
personal-details and teacher-entry empty-list guards unchanged. Add app-less
query and Platform adapter tests plus focused wizard tests for ordered labels,
single-teacher selection, and read failure. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F269 accepted)

F269 routes only the teacher-choice list read in
ClassDetailsWizardPage::initializePage() through the new Qt-free
InitialSetupTeacherChoicesReadQuery and active-session Platform adapter. Its
snapshot carries typed teacher IDs and the UTF-16 fields used by
Teacher::preferredDisplayName(). FileController creates the initial setup
database before launching the wizard. The service guard, repository order,
display labels, multiple-teacher placeholder, sole-teacher auto-selection,
warning, and empty-combo failure behavior remain intact; the other wizard
teacher reads are unchanged. The app-less query, adapter, and focused UI tests
cover field mapping, ordering, validation, selection, and failure behavior.
Fresh Windows x64 Debug/Ninja verification built ClassMngrFeatures,
ClassMngrInitialSetupWizardTests, and both new boundary test targets. All three
CTests passed; the two focused wizard slots passed 4/4 including setup and
cleanup. `git diff --check` and direct whitespace inspection passed. The unique
build tree was removed after logs were preserved under
`build/p2_f269_verify_logs/`. Non-fatal vswhere, Vulkan, zlib fallback, and
offscreen Qt resource/font warnings occurred; the full suite was not run.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F269 accepted; F270 selected)

F270 is selected to route only the assigned-teacher read in
MyClassesPage::rebuildClassInformation() through the accepted
TeacherProfileReadQuery and ApplicationServicesTeacherProfileReadPort. Preserve
the zero-ID skip, silent Teacher{} fallback on read failure, class navigation
labels, and teacher-card fields by projecting the returned profile at the UI
edge and setting Teacher.id only after a successful read. Keep the existing
teacher-service availability guard, F268 class-list query, class-info and
roster-count reads unchanged. Add focused page tests for complete teacher
profile projection and the failed-read fallback; retain the teacher-profile
query and adapter CTests. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-03 (F270 accepted)

F270 routes only the assigned-teacher read in
MyClassesPage::rebuildClassInformation() through the accepted
TeacherProfileReadQuery and ApplicationServicesTeacherProfileReadPort. The
zero-ID skip, silent Teacher{} fallback on read failure, class navigation
labels, and teacher-card fields remain intact; returned profile values are
projected at the UI edge, and Teacher.id is set only after a successful,
identity-matched read. The service-availability guard, F268 class-list query,
class-info, and roster-count reads are unchanged. Focused UI tests cover the
consumed profile fields, including UTF-16 text, and a missing assigned teacher
with retained class content and no warning. Fresh Windows x64 Debug/Ninja
verification built ClassMngrFeatures, ClassMngrMyClassesPageTests, and the
teacher-profile query and adapter test targets. The MyClassesPage and both
supporting CTests passed 3/3; both new UI slots passed directly.
`git diff --check` and direct test-file whitespace inspection passed. The
unique build tree was removed after logs were preserved under
`build/p2_f270_verify_logs/`. Non-fatal vswhere, optional Vulkan, Qt resource
and font, and offscreen size-hint warnings occurred; the full suite and other
platforms were not run. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### Progress update - 2026-10-03 (F270 accepted; F271 selected)

F271 is selected to route only SidebarController::classDisplayName()'s
selected-class and optional assigned-teacher reads through the accepted
SelectedClassSubtitleReadQuery and
ApplicationServicesSelectedClassSubtitleReadPort. Preserve the service
availability fallback, SidebarNodeNaming formatting, trimmed class-name and
`Class N` fallbacks, the `No Teacher` fallback, and delete choice/confirmation
flow. Extend the existing NavigationTeacherRead target to cover the chooser
label and confirmation message plus separate class-field and teacher-field
read failures. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F271 accepted)

F271 routes only SidebarController::classDisplayName()'s selected-class and
optional assigned-teacher reads through the accepted
SelectedClassSubtitleReadQuery and
ApplicationServicesSelectedClassSubtitleReadPort. The service guards,
SidebarNodeNaming formatting, trimmed classroom-name and `Class N` fallback
chain, and delete choice/confirmation flow remain intact. Focused tests exercise
the real record-selection dialog, selected label and confirmation message,
class-fields failure formatting, assigned-teacher failure retaining class
fields with the `No Teacher` fallback, and the no-active-session guard. On the
valid-ID public chooser path, the formatter always supplies a nonempty default
subtitle, so the final `Class N` fallback cannot be reached. Fresh Windows x64
MSVC 19.51 Debug/Ninja verification built ClassMngrFeatures,
ClassMngrNavigationTeacherReadTests, and the selected-subtitle query and adapter
test targets. All three CTests and all four new UI slots passed. `git diff
--check` found no whitespace errors; direct test-file whitespace inspection
passed. The unique build tree was removed after logs were preserved under
`build/p2_f271_verify_logs/`. Non-fatal vswhere, optional Vulkan, Qt resource and
font, and offscreen size-hint warnings occurred; the full suite and other
platforms were not run. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### Progress update - 2026-10-03 (F271 accepted; F272 selected)

F272 is selected to route only the current-class-only testing-class read in
RosterPrintDialog::loadClasses() through the accepted
TestingClassDetailsReadQueryHandler and
ApplicationServicesTestingClassDetailsReadPort. Preserve the service guards,
display name/grade/level formatting, checked item, class ID, and silent empty
list on read failure. Extend the existing RosterPrintDialog tests with a
current-class-only failure case and retain the testing-class query and adapter
CTests. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F272 accepted)

F272 routes only the current-class-only testing-class read in
RosterPrintDialog::loadClasses() through the accepted
TestingClassDetailsReadQueryHandler and
ApplicationServicesTestingClassDetailsReadPort. The service guards,
display-name/grade/level formatting, checked item, class ID, branch return, and
silent empty-list behavior on invalid ID or read failure remain intact. A new
UI test removes the testing-class details row while the session and services
remain available; it verifies the list and selected IDs are empty and no prompt
appears. The existing current-class-only success test remains covered. Fresh
Windows x64 Debug/Ninja verification built ClassMngrFeatures,
ClassMngrRosterPrintDialogTests, and the testing-class query and adapter test
targets. The new UI slot passed directly (3 QtTest passes including setup and
cleanup); all three selected CTests passed. `git diff --check` and direct
test-file whitespace inspection passed. The unique build tree was removed
after logs were preserved under `build/p2_f272_verify_logs/`. Non-fatal
vswhere, Qt resource, and font-directory warnings occurred; the full suite and
other platforms were not run. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-03 (F272 accepted; F273 selected)

F273 is selected to route only the fresh target-roster read in
RosterEditorWidget::transferRosterRow() through the accepted
RosterReadUseCase and ApplicationServicesRosterReadPort. Keep the read after
transfer validation and at transfer time. Preserve the empty-Roster fallback
on failure, custom columns and widths, row mapping, paired source/target saves,
and current warning/source-row behavior. Add an integration test that triggers
the actual transfer and checks source removal plus target data and widths;
retain the roster query and adapter CTests. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F273 accepted)

F273 routes only the fresh target-roster read in
RosterEditorWidget::transferRosterRow() through the accepted RosterReadUseCase
and ApplicationServicesRosterReadPort. The read stays after transfer
validation and at transfer time. The empty-Roster fallback on failure, custom
columns and widths, row mapping, paired source/target saves, and current
warning/source-row behavior remain intact. The integration test opens the real
menu, changes the target roster after menu construction, then selects the
target action; it verifies source-row removal and preservation of target rows,
custom columns, and widths. Fresh Windows x64 Debug/Ninja verification built
ClassMngrFeatures, ClassMngrRosterTransferMenuTests, and the roster query and
adapter test targets. The new UI slot passed directly (3 QtTest passes
including setup and cleanup); all three selected CTests passed.
`git diff --check` and direct test-file whitespace inspection passed. The
unique build tree was removed after logs were preserved under
`build/p2_f273_verify_logs/`. Non-fatal missing-vswhere, optional Vulkan,
bundled-zlib fallback, and offscreen Qt resource/font/window warnings occurred;
the full suite was not run. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-03 (F273 accepted; F274 selected)

F274 is selected to route only the class-list read inside
RosterPrintDialog::updateExtraInfoColumns() through the accepted
ClassesListReadQuery and ApplicationServicesClassesListReadPort. Preserve
class-ID resolution, the existing warning and preview update on failure,
column ordering and checked-state restoration. Leave the accepted F264 roster
reads untouched. Extend the existing RosterPrintDialog tests with a failure
after existing extra-info controls have been rendered, checking the warning
and preserved controls; retain the classes-list query and adapter CTests. Phase
2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F274 accepted)

F274 routes only the class-list read inside
RosterPrintDialog::updateExtraInfoColumns() through the accepted
ClassesListReadQuery and ApplicationServicesClassesListReadPort. Class-ID
resolution, the existing warning and preview update on failure, column order,
and checked-state restoration remain intact; accepted F264 roster reads are
unchanged. The UI failure test drops the classes table after rendering and
checking extra-info controls, then confirms the warning and preserved control
state. Fresh Windows x64 Debug/Ninja verification built ClassMngrFeatures,
ClassMngrRosterPrintDialogTests, and the classes-list query and adapter test
targets. The new UI slot passed directly (3 QtTest passes including setup and
cleanup); all three selected CTests passed. `git diff --check` and direct
test-file whitespace inspection passed. The unique build tree was removed
after logs were preserved under `build/p2_f274_verify_logs/`. Non-fatal
vswhere, optional Vulkan, Qt bundled-zlib, resource-pack and font-directory
warnings occurred; the full suite was not run. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F275 accepted; F276 selected)

F275, committed as `59b3887e`, routes only the class-list read in
SidebarController::promptForClassToDelete() through the accepted
ClassesListReadQuery and ApplicationServicesClassesListReadPort. The
service-availability return, existing `Delete Class` warning on read failure,
positive unique IDs, list order, F271 labels, and selected-ID flow remain
intact. The shared query rejects corrupt invalid or duplicate IDs so they reach
the existing warning instead of being silently skipped. The new
NavigationTeacherRead case confirms that a list-read failure shows the warning
without opening a chooser or confirmation. Fresh Windows x64 Debug/Ninja
verification built ClassMngrFeatures and the NavigationTeacherRead, classes
list query, and classes-list adapter test targets. The focused UI case passed
3/3 including setup and cleanup; the three CTests passed 3/3. `git diff --check`
and `git show --check` passed. Logs are preserved under
`build/p2_f275_verify_logs/`; the temporary build was removed. The full suite
was not run. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F276 accepted; F277 selected)

F276, committed as `2572d29d`, routes the Native English and GS birthday-
directory reads in `SidebarController::loadUpcomingBirthdaySchedule()` through
the existing directory-list queries and active-session Platform adapters. The
full Korean-teacher read remains direct and the order is preserved: full
teachers, Native English directory, then GS team directory. Snapshot fields
are projected into the existing schedule builder; the date range, schedule
builder, `Birthdays could not be loaded.` warning, and Native English
diagnostic precedence when both directory reads fail remain unchanged. Three
caller-level cases verify entries from all staff directories, the warning
without a dialog for a GS read failure, and Native English diagnostic
precedence when both directory reads fail. Fresh Windows x64 Debug/Ninja
verification built `ClassMngrNavigationTeacherReadTests`; the three added
slots passed directly and the focused CTest target passed 1/1. `git diff --check`
and `git show --check` passed. Logs are preserved under
`build/p2_f276_verify_logs/`. Non-fatal missing-documents-resource-pack and
offscreen Qt sizing warnings occurred; the full suite was not run.

### Progress update - 2026-10-03 (F277 accepted; F278 selected)

F277, committed as `54ec8a66`, routes only the direct class-list read in
`RosterEditorWidget::showRosterContextMenu()` through the accepted
`ClassesListReadQuery` and `ApplicationServicesClassesListReadPort`. The read
remains inside the existing eligibility branch; target eligibility, sorting,
label fallback, disabled menu behavior, and the existing warning are preserved.
The adjacent roster read and metadata behavior remain unchanged. The new caller
case preserves usable class metadata while making the class-list query fail,
then checks the warning and disabled `No same-grade classes` action. The shared
query's invalid/duplicate-ID rejection is a stricter failure condition than the
former direct service read. Fresh Windows x64 Debug/Ninja verification built
the `RosterTransferMenu`, class-list query, and adapter test targets. The added
slot passed directly (3 QtTest passes including setup and cleanup); all three
selected CTests passed. `git diff --check` and `git show --check` passed. Logs
are preserved under `build/p2_f277_verify_logs/`. Non-fatal document-resource,
font, SVG, and offscreen-plugin warnings occurred; the full suite was not run.

### Progress update - 2026-10-03 (F278 accepted; F279 selected)

F278, committed as `11ae1b59`, routes the teacher-existence reads in
`PersonalDetailsWizardPage::nextId()` and
`TeacherEntryWizardPage::validatePage()` through the accepted
`InitialSetupTeacherChoicesReadQuery` and
`ApplicationServicesInitialSetupTeacherChoicesReadPort`. Schedule-import
routing, page selection, blank-entry skipping, and the failure-as-empty
fallback remain intact; no warnings were added to either check. Teacher
creation and Class Details teacher-choice population remain unchanged. Four
caller cases cover routing for populated and empty directories, skipping a
blank teacher entry when teachers exist, and treating failed reads as empty
without warnings. Active-session requirements and invalid/duplicate-ID query
validation are stricter than the legacy service call; those failures use the
same empty-list fallback. Fresh Windows x64 Debug/Ninja verification built
only the wizard, query, and adapter test targets. All four added slots passed
directly, and the three selected CTests passed. Logs are preserved under
`build/p2_f278_verify_logs/`. Non-fatal resource/font warnings occurred; the
full suite was not run.

### Progress update - 2026-10-03 (F279 accepted; F280 selected)

F279, committed as `a1701f54`, routes only the selected-teacher profile read
inside `SidebarController::deleteTeacher()` through the accepted
`TeacherProfileReadQuery` and `ApplicationServicesTeacherProfileReadPort`.
The service guard, confirmation text, cancel behavior, delete flow, and
`Delete Teacher` / `The teacher could not be loaded.` warning remain intact. The
teacher-list read in `promptForTeacherToDelete()` and post-create read in
`addTeacher()` are unchanged. Caller tests confirm that a stale sidebar label
does not replace the profile display name, cancellation leaves the teacher in
place, and a failed profile read shows the warning without a confirmation.
Fresh Windows x64 Debug/Ninja verification built the navigation, profile query,
and profile adapter test targets. Both new slots passed directly, and all three
selected CTests passed. Logs are preserved under `build/p2_f279_verify_logs/`;
non-fatal resource/font warnings occurred and the full suite was not run. The
profile adapter requires an active session whereas the legacy service can fall
back to DataService.

### Progress update - 2026-10-03 (F280 accepted; F281 selected)

F280, committed as `84daa9eb`, routes the teacher-list read in
`SidebarController::promptForTeacherToDelete()` through the accepted
`InitialSetupTeacherChoicesReadQuery` and
`ApplicationServicesInitialSetupTeacherChoicesReadPort`. Repository order,
formatted labels, positive selected IDs, the existing warning, and the early
return when no usable records remain are preserved. Caller tests verify the
expected label and selected ID on success, then cancel; on read failure, the
existing warning appears without a chooser or confirmation. Fresh Windows x64
Debug/Ninja verification built the NavigationTeacherRead, teacher-choice query,
and adapter test targets. Both new slots passed directly and the three selected
CTests passed. Logs are preserved under `build/p2_f280_verify_logs/`; non-fatal
resource/font and offscreen size-hint warnings occurred, and the full suite was
not run. The query rejects a list containing invalid or duplicate IDs, where
the old chooser skipped nonpositive IDs, and its adapter requires an active
session unlike the legacy DataService fallback.

### Progress update - 2026-10-03 (F281 accepted; F282 selected)

F281, committed as `0287bc62`, routes the teacher-list read and projection in
`SidebarController::refreshTeacherSidebar()` through the accepted
`InitialSetupTeacherChoicesReadQuery` and
`ApplicationServicesInitialSetupTeacherChoicesReadPort`. Service guards, read
order (teacher list before class-teacher assignments), evaluation of both
reads before failure handling, teacher-read diagnostic precedence, sidebar
clearing, action-state update, assignment matching, sorting, labels, and
assigned/unassigned grouping remain intact. Caller tests cover assigned and
unassigned names/IDs and a teacher-list failure that warns, clears teacher
groups, and updates action states. Fresh Windows x64 Debug/Ninja verification
built the NavigationTeacherRead, teacher-choice query, and adapter targets.
Both added slots passed directly; the three selected CTests passed. Logs are
preserved under `build/p2_f281_verify_logs/`; non-fatal resource/font warnings
occurred and the full suite was not run. The query rejects the full list when
any teacher ID is invalid or duplicated, and the adapter requires an active
session unlike the legacy DataService fallback.

### Progress update - 2026-10-03 (F282 accepted; F283 selected)

F282, committed as `63ef995c`, routes the class and teacher list reads in the
no-argument `SidebarController::updateActionStates()` through the accepted
class-list and teacher-choice queries and active-session adapters. The
no-actions early return, service-availability guard, classes-then-teachers
order, and behavior that failed or empty reads disable only their respective
data-dependent actions without a warning remain intact. The caller failure
case confirms that class-list failure disables class actions while the teacher
list remains usable; F281's test retains the complementary teacher-read
failure case. Fresh Windows x64 Debug/Ninja verification built
NavigationTeacherRead plus both query and adapter test pairs; all five CTests
passed. Logs are preserved under `build/p2_f282_verify_logs/`. The isolated
slot invocation exited successfully but produced no console output, so the
full CTest target is the verification evidence. No full suite ran. Active-
session requirements and invalid/duplicate-ID rejection can disable actions
where legacy reads could have found records.

F283 is selected to migrate only the per-class roster read used for student
counts in `MyClassesPage::refresh()` through the accepted `RosterReadUseCase`
and `ApplicationServicesRosterReadPort`. Preserve count semantics: count a row
when the trimmed English or Korean cell is nonblank, return zero when neither
column exists, and keep failure-as-zero. Keep the existing `# of Students`
display and class-list/read behavior unchanged. Extend the existing
`MyClassesPage` caller tests to cover populated and blank roster rows plus the
zero fallback on read failure; retain the roster query and adapter CTests. The
count projection must match the legacy QString trimming and row/column bounds
behavior. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F283 accepted; F284 selected)

F283, committed as `9c884c97`, routes MyClassesPage per-class roster reads
through the accepted RosterReadUseCase and active-session adapter. Its row
count matches the legacy behavior: either trimmed English or Korean name
counts once; missing name columns and read failure display zero. The
`# of Students` label and class-list behavior remain unchanged. The focused
build succeeded and the MyClassesPage, roster-query, and roster-adapter CTests
passed 3/3. Logs are under `build/p2_f283_verify_logs/`; no full suite ran.

F284 is selected to replace the post-selection `classroom(classId)` read in
`SidebarController::deleteClass()` with the accepted classes-list query and
adapter. Recheck that the chosen ID is still present before confirmation. On
query failure or a missing selected ID, preserve the existing Delete Class
warning and stop without confirmation or deletion. Keep the delete service
guard, confirmation content, and removal flow unchanged. Add caller coverage
for a selected-class reload failure after the chooser has accepted a choice;
retain the NavigationTeacherRead, classes-list query, and adapter CTests.
The full-list query can reject unrelated invalid or duplicate IDs, and its
adapter requires an active database session where the legacy class service
could fall back to DataService. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-03 (F284 accepted; F285 selected)

F284, committed as `d1a7e04b`, routes `SidebarController::deleteClass()`'s
post-selection class reload through the accepted classes-list query and
adapter. Query failure or a missing selected ID preserves the existing warning
and returns before confirmation or deletion. The service guard and successful
delete flow remain unchanged. Caller tests cover a failed list reload and a
selected class removed after chooser selection. The focused build succeeded
and the NavigationTeacherRead, classes-list query, and adapter CTests passed
3/3. Logs are under `build/p2_f284_verify_logs/`; no full suite ran. The
full-list query can reject unrelated invalid or duplicate IDs, and its adapter
requires an active database session where the legacy service could fall back
to DataService.

F285 was deferred before implementation. `SidebarController::addTeacher()`
creates a default blank `Teacher`, but `TeacherService::create()` applies
`TeacherValidator`, which rejects a blank Korean name, English name, and
preferred romanization with `teacher.name.required`. This validation was added
in commit `399fd1ae`, after the blank-draft flow was introduced in `1edfe38c`.
The controller therefore returns before the profile read. Changing creation
validation or adding a name-entry step would change a product/domain contract;
a test seam that pretends blank creation succeeded would not verify production
behavior. Revisit this migration after the blank-draft contract is clarified.

F286 is selected to use the successful `WorkspaceSession` location returned by
the coordinator when FileController sets its current-file state after create
or open, removing the redundant service-path read. Preserve path normalization,
recent-workspace updates, and open-path handling. Retain the
FileControllerWorkspaceLifecycle and workspace-port CTests, including Unicode
path coverage if the existing assertions do not exercise it. Compare returned
session locations with the previous normalized service path before accepting.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F286 accepted; F287 selected)

F286, committed as `8c5d3a01`, sets FileController's current-file path after
successful workspace create/open from the returned `WorkspaceSession`
location, converted from UTF-8 at the controller boundary. Create still
updates recent history from `m_currentFile`; open still passes the original
`filePath` to recent-history handling. Unicode create/open caller cases cover
normalized current-file state, open-history deduplication, and directory
behavior. Fresh focused verification built the FileController lifecycle and
workspace-port targets; their CTests passed 2/2, and both Unicode slots passed
directly (4/4 QtTest entries including setup and cleanup). Logs are under
`build/p2_f286_verify_logs/`; non-fatal missing-documents-resource warnings
occurred and the full suite was not run. The lifecycle cases use concrete
services, so they do not force session and service paths to diverge; source
review confirms FileController now selects the returned session location.

F287 is selected to remove the direct `classroom(classId)` lookup from
`ClassImportDialog::destinationClassDisplayName()`. The formatter always
returns a nonempty label, including `Unknown Class • No Teacher` when subtitle
fields are unavailable, so the class-name and `Class N` branches cannot
contribute to current output. Preserve the committed fallback label and retain
the `ClassMngrClassTransferTests` caller case. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F287 accepted; F288 selected)

F287, committed as `16b77b79`, removes the unreachable direct class lookup from
`ClassImportDialog::destinationClassDisplayName()`. The formatter always
returns a nonempty label, so the prior `classroom(classId)` name fallback could
not affect current output. The existing caller test still expects
`Unknown Class • No Teacher` when subtitle fields cannot load. Fresh Windows
x64 Debug/Ninja verification built the class-transfer target in 315 steps; the
`ClassMngrClassTransferTests` CTest passed 1/1 and the focused caller case
passed 3/3 QtTest entries. Logs are under `build/p2_f287_verify_logs/`. The
documents-resource warning was non-fatal; the configure wrapper reported exit
1 despite successful generation, target build, and tests. The full suite was
not run.

### Progress update - 2026-10-03 (F288 accepted; F289 selected)

F288, committed as `b2c807b1`, routes `CampusDashboardPage::loadCampuses()`
through `CalendarPageCampusDirectoryQueryAdapter`, preserving campus codes,
repository order, selector labels, and current/stored-selection fallback. The
caller tests cover all 13 bundled campuses, ordering, role-specific labels, and
selection behavior; missing and whitespace optional codes remain covered at
the adapter layer. The independent `ClassMngrCampusDashboardPageTests` and
`ClassMngrNextPlatformCalendarPageCampusDirectoryQueryTests` CTests passed 2/2.
Logs are under `build/p2_f288_verify_ninja_*` and
`build/p2_f288_verify_logs/`. The initial Visual Studio generator attempt was
replaced by a successful Ninja configure/build/CTest run; the optional Vulkan
header warning was non-fatal. The full suite was not run.

F289 is selected to route roster-template print class-scope enumeration
through the accepted classes-list query. Inject `ClassesListReadQuery` at the
print-service boundary; the production caller must provide the real
`ApplicationServicesClassesListReadPort` adapter, and the service test must
exercise the real query over a fake `ClassesListReadPort`. Do not pass the
adapter an unconstructed `ApplicationServices` test object. Preserve
all/current/selected scope resolution and class order, then verify the real
adapter independently.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F289 accepted; F290 selected)

F289, committed as `4b2dcc9f`, routes roster-template class-scope enumeration
through the accepted `ClassesListReadQuery`. Both the editor save/print caller
and live-preview caller construct the production
`ApplicationServicesClassesListReadPort`; class-detail and roster reads remain
unchanged. The focused `ClassMngrFeatures` build compiled the changed callers.
The roster print service, classes-list query, and production classes-list
adapter CTests passed 3/3:
`ClassMngrRosterTemplatePrintServiceTests`,
`ClassMngrNextApplicationClassesListReadQueryTests`, and
`ClassMngrNextPlatformApplicationServicesClassesListReadPortTests`. Logs are
under `build/p2_f289_verify_logs/`. An initial Visual Studio environment
attempt reported missing `vswhere.exe`; adding the Visual Studio Installer
directory resolved it. The successful final build had no compiler warnings.
The service test uses a fake read port, while the platform-port test exercises
the real adapter; UI caller wiring was compile-verified, not runtime
integration-tested. The full suite was not run.

F290 is selected to route roster-template printing's per-class roster read
through the accepted roster query. Preserve class-scope resolution and order,
class-detail reads, missing-roster handling, and printed output. Retain the
roster print service caller tests and verify the real roster adapter
independently. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F290 accepted; F291 selected)

F290, committed as `2e8f3cb8`, routes roster-template per-class roster reads
through `RosterReadUseCase` and the accepted roster port while preserving
class-detail reads and per-class order. Both editor save/print and live-preview
callers construct the production roster adapter. The complete roster snapshot
conversion preserves columns, widths, rows, sparse cells, and UTF-16 text; a
rendered PDF comparison covers Unicode and sparse-row output. The focused
`ClassMngrFeatures` and print-service builds passed. The print-dialog, print
service, roster query, and production roster-adapter CTests passed 4/4:
`ClassMngrRosterPrintDialogTests`,
`ClassMngrRosterTemplatePrintServiceTests`,
`ClassMngrNextApplicationRosterReadQueryTests`, and
`ClassMngrNextPlatformApplicationServicesRosterReadPortTests`. After adding a
multi-class early-stop assertion, the print-service CTest passed again 1/1.
Logs are under `build/p2_f290_verify_logs/`. A non-fatal `vswhere.exe`
environment warning was resolved by adding the Visual Studio Installer
directory to `PATH`. The full suite was not run.

F291 is selected to replace My Classes' per-class full-info read by composing
the accepted class-details and class-notes projections, subject to
field-parity review. Preserve the displayed fields and teacher association;
record any legacy fields not represented by the accepted projections before
cutting over. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F291 accepted; F292 selected)

F291, committed as `31f92c73`, adds a dedicated compact My Classes
class-information query, snapshot, and port with one `ApplicationServices`
adapter read; the accepted class-list, roster-count, and full teacher-profile
queries remain. The evidence-based re-scope followed field-parity review:
class-details and class-notes projections both omit `teacherId`, and each
adapter loads full `ClassInfo`, so composing them would lose the teacher
association and duplicate repository reads. An isolated Windows x64
Debug/Ninja build compiled all three targets; source ownership validated 1,230
handwritten sources. `ClassMngrNextApplicationMyClassesClassInformationReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesMyClassesClassInformationReadPortTests`,
and `ClassMngrMyClassesPageTests` passed 3/3. Toolchain: MSVC 19.51.36257.0 and
Qt 6.12.0. `git diff --check` passed. Logs are under
`build/p2_f291_independent_verify/`. Plain PowerShell could not locate `cl.exe`;
configuration succeeded from the Visual Studio developer shell. Non-fatal
`vswhere.exe` and long-path warnings occurred. The full suite was not run.

F292 is selected to add a Korean-teacher birthday-directory query and adapter
for the sidebar birthday schedule. Phase 2 remains In Progress/Open; Gates 1
and 2 remain Partial.

### Progress update - 2026-10-03 (F292 accepted; F293 selected)

F292, committed as `1617d71f3df12717391562a37200c5b70da9c215`, adds a Qt-free
Korean birthday-directory query, snapshot, and port with an
`ApplicationServices` adapter, and removes the sidebar's Korean birthday read
dependency on `TeacherService`. The compact projection carries raw birthday
and preferred-name fallback fields. The sidebar preserves silent returns for
typed NotFound or unavailable session; technical read failures retain the
Korean-first warning and early stop, ahead of later Native English and GS
warning handling.

Independent fresh Windows x64 Ninja/MSVC 19.51.36257 and Qt 6.12 verification
passed the CMake ownership gate (1,236 handwritten sources), built `ClassMngr`
and six focused targets, and passed these six CTests: `ClassMngrNextApplicationKoreanTeacherBirthdayDirectoryReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesKoreanTeacherBirthdayDirectoryReadPortTests`,
`ClassMngrNavigationTeacherReadTests`,
`ClassMngrNavigationTeacherReadParityTests`,
`ClassMngrNextApplicationUpcomingBirthdayScheduleUseCaseTests`, and
`ClassMngrUpcomingBirthdaysTests`. `git diff --check` passed. Logs are under
`build/p2_f292_independent_verify_ninja/`. The adapter test does not runtime-spy
repository call count; source inspection confirms one `getAllTeachers()` call.
The full suite was not run.

When F293 work began, its second-last-in-Batch-1 discovery trigger was applied;
Batch 2 is recorded in the phase plan (commit `67376ac7`). Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F293 accepted; F294 selected)

F293, committed as `51e7da7a9ad0f660e07a1f0cb5319a9dc66ae306`, adds a Qt-free
assignment snapshot, read port, and query with an active-session
`ApplicationServices` adapter over
`ClassInfoRepository::loadClassTeacherAssignments()`. Sidebar refresh uses the
query; the assignment-read and availability paths no longer depend on
`ClassService` or `TeacherService`. Action-state availability now checks the
active database session directly. The snapshot has one row per regular
class, including unassigned rows; only positive teacher assignments carry
a typed teacher ID. Sidebar refresh still clears
nodes, attempts both teacher-choice and assignment reads, preserves
teacher-choice error precedence, sorting and deduplication, unassigned-row
action availability, warning/action updates, and silent no-session behavior.

Fresh independent Windows x64 verification used Ninja/MSVC 19.51.36257 and Qt
6.12.0. The CMake ownership gate reported 1,242 handwritten files; `ClassMngr`
and six focused targets built in 376 steps. These six CTests passed 6/6:
`ClassMngrNextApplicationClassTeacherAssignmentsReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesClassTeacherAssignmentsReadPortTests`,
`ClassMngrNavigationTeacherReadTests`,
`ClassMngrTestingClassRepositoryTests`,
`ClassMngrNavigationTeacherReadParityTests`, and
`ClassMngrNextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests`.
After a test-only repair added direct exact sidebar-order assertions,
`ClassMngrNavigationTeacherReadTests` rebuilt and passed 1/1. `git diff --check`
passed. Logs are under
`build/p2_f293_independent_verify_ninja_20261003/` (configure, focused build,
focused CTest, and navigation-recheck logs). Nonfatal environment warnings:
VSDevCmd could not find `vswhere.exe`; 27 object-path-length warnings.
Repository call count and malformed database teacher IDs were source-inspected,
not runtime-spied; sidebar order is directly asserted. The full suite was not
run.

Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F294 accepted; F295 selected)

F294, committed as `d16614817d5e7512881f046bd37ee0398db45577`, adds a Qt-free
latest teacher-import source-date query returning an optional canonical ISO
date. Its active-session `ApplicationServices` adapter reads
`teacher_import/latest_source_date` once. Missing, empty, or malformed dates
produce a successful absent value; unavailable persistence or read failure
produces a failed result. Sidebar pre-apply comparison uses the query while
the legacy `TeacherService` remains for import apply. Newer or missing dates
bypass confirmation; equal or older dates retain confirmation, and
cancellation, read-failure warning/stop, and silent no-session behavior remain.
Unused latest-date compatibility reads were removed from `TeacherService` and
`DataService`.

Fresh Windows x64 Debug/Ninja/MSVC verification used Qt 6.12 and passed the
CMake ownership gate at 1,247 handwritten files. The navigation controller
and two new query/adapter targets built; three focused CTests passed. After a
test repair, the navigation target rebuilt and its CTest passed 1/1. Five
import-related QtTest slots passed; the reported three QtTest entries included
setup and cleanup. Accepted-confirmation integration asserted confirmation,
completion, saved source date, and imported rows in Native-English and GS
tables only; read-failure coverage asserted warning, no completion, and empty
rows in all three tables. `git diff --check` passed. Logs are under
`build/f294v/`; nonfatal environment messages covered missing `vswhere.exe`,
optional Vulkan headers, the documents resource pack, and Qt offscreen/font
warnings. The full 309-test suite was not run.

F295 is selected to pass accepted classes-list ID/name data through the
roster-template print pipeline and remove its per-class `classroom()` lookup.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F295 accepted; F296 selected)

F295, committed as `1d0e318aa26ec1ed0148749dbf781440b769d5af`, reuses names
from the accepted classes-list projection in the roster-template loader and
removes its per-class `classroom()` read. Each resolved class still gets one
class-information read and the accepted roster query. `RosterClassData` keeps
the label for the existing class-grade/level then name fallback. The editor
passes the raw known current-class name as scalar `Request.currentClassName`
through final print/save; dialog testing-class details pass it to live preview.
This preserves an out-of-list current/testing class without a duplicate full
class-list snapshot in the request.

Tests cover the classes-list fallback label, all/selected ordering and read
order, out-of-list current-only printing with PDF comparison, and invalid
non-current selected IDs absent from the list/name failing before class-info
or roster reads. Fresh independent Ninja/MSVC x64 Debug/Qt 6.12 configure and
build compiled `ClassMngr`, `ClassMngrRosterTemplatePrintServiceTests`, and
`ClassMngrRosterPrintDialogTests`. The CMake ownership gate passed for 1,247
handwritten sources; both focused CTests and `git diff --check` passed. Logs
are under `build/f295v/`. Nonfatal messages covered missing `vswhere.exe`,
unavailable optional Vulkan headers, and line-ending conversion warnings. The
full suite was not run.

A scoped internal-request limitation remains: an invalid non-current selected
ID absent from the authoritative list/current-name input fails before reads
with the generic "Roster data is not available." error; previously,
`classroom()` could load a valid unlisted ID or return a repository-specific
missing-class error. Production dialog selections come from the classes list,
and the excluded current/testing-class case remains supported explicitly.

F296 is selected to route roster-template printing's per-class full
class-information read through a purpose-fit application projection because
the printer needs room and Zoom details beyond class-list/subtitle
projections. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F296 accepted; F297 selected)

F296, committed as `6d98d602d673b532fb57e37d6ce76a669160df5f`, adds a Qt-free
roster-print class-information query, snapshot, and active-session adapter.
The repository reads only the template's class grade/level, teacher names,
room, Wi-Fi and Zoom fields, and regular schedule; it skips intensive times.
If the metadata row is absent, the read keeps blank metadata and returns any
regular schedule. The query validates canonical IDs, propagates errors, and
rejects mismatched result IDs. Roster printing uses the compact projection,
and the Sub Prep consumer was adapted to cache its existing display name
after `RosterClassData` became purpose-fit.

Fresh independent Windows x64 Debug/Ninja verification passed the CMake
ownership gate at 1,253 handwritten sources. Five focused CTests passed:
`ClassMngrNextApplicationRosterPrintClassInfoReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesRosterPrintClassInfoReadPortTests`,
`ClassMngrRosterTemplatePrintServiceTests`,
`ClassMngrSubPrepPackageServiceTests`, and
`ClassMngrRosterPrintDialogTests`; the targeted roster service recheck passed
1/1. Verification artifacts are under `build/f296v_verify_20261003/`; the full
suite was not run.

F297 is selected to route Campus Dashboard's selected-campus detail read
through an application query. Preserve save-before-read behavior and the
silent return when the selected campus is missing. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-03 (F297 accepted; F298 deferred; F299 selected)

F297, committed as `2e2ce2c4c109a7ba55f0d2365ef2c6bc7cf41aac`, routes Campus
Dashboard's selected-campus detail read through an application query while
preserving save-before-read and silent missing-campus behavior. Fresh Windows
x64 Debug/Ninja verification in `build/f297_verify_ninja` passed the CMake
ownership gate at 1,259 handwritten sources and built `ClassMngr` plus the
query, adapter, and dashboard test targets. Three focused CTests passed; after
the final dashboard-test-only updates, an independent dashboard CTest rerun
passed 1/1. `git diff --check` passed; the full suite was not run.

F298 remains deferred after review. `ClassService::create()` returns
`Result<int>` and repository last-insert ID, so the identity is mechanically
available, but read-failure warning/no-navigation behavior was deliberately
added in `cd60f95b`, and `openClass()` may select another class or none. No test
clarifies the intended post-create failure behavior, so defer pending a
semantic decision. F299 is selected for the Class Analytics dashboard
read/use case. Its start includes a separate Phase 2 missed-slice completeness
audit; this is distinct from the Batch 3 discovery recorded at F298 start.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Audit update - 2026-10-03 (F299-start completeness audit)

Two independent read-only sweeps found no missed direct legacy-read routes
outside recorded work; `SidebarController::getTeacherById()` has no callers.
The second sweep identified four genuine post-migration query fan-out
opportunities, independently classified as separate batching/aggregation
work: F302-F305, added after F301 in Batch 3. This audit is distinct from the
F298-start Batch 3 discovery of F300-F301. F299 remains selected/current.

### Progress update - 2026-10-04 (F299 accepted; F300 selected)

F299, committed as `250fc6d2addd77559f376d0aae872f4aab849a60`, adds a Qt-free
Class Analytics query/calculator and compact roster/evaluation DTOs and ports.
Its active-session SQL adapter reads only roster-name columns and six evaluation
scores. Platform name semantics preserve `StudentNameUtils` normalization and
`QLocale` ordering. `ClassAnalyticsPage` routes through the query and preserves
its empty state on no-data or read failure.

App-less tests cover canonical evaluation-read order, current-roster filtering,
All/named/unknown selection, historical global YTD, duplicate consolidation,
partial scores, name identity behavior, and structured failure. SQL tests cover
compact mapping, missing evaluation as successful empty input,
punctuation/Korean suffix matching, and query failure. Page tests cover
no-data/error and successful ranking-model name/average/grade mapping.

Fresh independent Windows x64 Debug/Ninja configure in
`build/f299_verify_ninja` passed the ownership gate for 1,265 handwritten
sources, built six targets (`ClassMngr`, query, adapter, page, and two legacy
analytics fixtures), and passed five focused CTests. After a success-path page
test delta, an independent rebuild and focused page CTest passed 1/1.
`git diff --check` passed. Logs:
`build/f299_verify_ninja_{configure,build,ctest_focused,page_rebuild,page_recheck_ctest}.log`.
The full suite was not run. Configure had nonfatal optional
`WrapVulkanHeaders`/`Vulkan_INCLUDE_DIR` and `vswhere.exe` messages; MSVC was
found.

The page success test checks selected ranking mappings only; there is no
runtime side-by-side full-dashboard comparison against the legacy service or
direct UI assertion for summary, class-shape, and YTD mapping. This limitation
was accepted for the slice; it does not establish full parity or Phase 2
completion. The F299-start completeness audit remains distinct from the earlier
F298-start Batch 3 discovery: independent sweeps found no additional direct
legacy-read paths, while F302-F305 remain separate query fan-out candidates
after F301. F298 remains deferred pending a semantic decision on read-failure
warning/no-navigation behavior. F300 is selected next: reduce repeated
per-matching-class `SelectedClassSubtitleReadQuery` calls for ClassImportDialog
destination labels after F262; F301-F305 remain ordered candidates. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F300 accepted; F301 selected)

F300, committed as `885adbf6f301be03dc437bfc7c774e4e5f913f47`, adds
`SelectedClassSubtitleBatchReadQuery` and `SelectedClassSubtitleBatchReadPort`,
with an active-session `ApplicationServicesSelectedClassSubtitleBatchReadPort`.
ClassImportDialog collects canonical destination IDs across valid preview
rows, deduplicates them for one ordered batch query, then renders choices from
the original matching-ID lists so their order and duplicates remain visible.
When batch data is unavailable, the existing formatted destination-label
fallback remains in use.

The application query validates canonical positive, unique IDs before port
access, skips the port for empty input, and checks returned count and ID order.
The adapter makes one class batch repository call, covering one metadata and
one regular-schedule statement; assigned teachers are deduplicated and use
one batch repository call/statement when present. Read failures remain scoped
to their class or assigned-teacher snapshot fields.

Independent fresh Windows x64 Debug/Ninja configure passed the CMake ownership
gate at 1,270 handwritten sources. `ClassMngr` and the query, adapter, dialog,
and single-read test targets built. These five focused CTests passed:
`ClassMngrClassTransferTests`,
`ClassMngrNextApplicationSelectedClassSubtitleBatchReadQueryTests`,
`ClassMngrNextApplicationSelectedClassSubtitleReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests`,
and `ClassMngrNextPlatformApplicationServicesSelectedClassSubtitleReadPortTests`.
`git diff --check` passed; the full suite was not run, and no further F300
limitation was identified. Logs are under
`build/f300_verify_ninja/verification_logs/`.

F301 is selected to batch ClassExportDialog's per-class selected-subtitle reads
after its classes-list load. The F299 completeness audit, F298 deferral, and
ordered Batch 3 candidates remain unchanged. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F301 accepted; F302 selected)

F301, committed as `77411a5474e178ab278efec519c5aee3b8de64bb`, reuses the
accepted `SelectedClassSubtitleBatchReadQuery` and port in ClassExportDialog.
After the accepted class-list read, the dialog gathers valid IDs, runs one
batch query when the list is nonempty, maps snapshots by ID, and formats the
subtitle labels. It preserves the exact formatter behavior, failed
class/teacher fallback, class-list warning, case-insensitive/numeric sort then
ID, user-role ID, unchecked initial state, and selection behavior.

The three-class UI test asserts one batch call for three requested IDs, one
metadata SQL statement, one regular-schedule statement, and one teacher batch
statement. A new empty-list UI test asserts zero batch reads. Fresh independent
Windows x64 MSVC/Ninja Debug configure passed the CMake ownership gate at
1,270 handwritten sources; `ClassMngr`, `ClassMngrClassTransferTests`, and the
batch application-query and platform-adapter targets built. These three
focused CTests passed: `ClassMngrClassTransferTests`,
`ClassMngrNextApplicationSelectedClassSubtitleBatchReadQueryTests`, and
`ClassMngrNextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests`.
Independent source review found one guarded batch query and no per-class
single query. `git diff --check` passed; the full suite was not run. Logs are
under `build/f301_verify_ninja/verification_logs/`.

F302 is selected to batch class-delete chooser subtitle-label queries
following F275. F299's separate completeness audit and F298's semantic
deferral remain unchanged. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-04 (F302 accepted; F303 selected)

F302, committed as `1ffc88a32a3b12bc0354deac2f485d61abb15ef9`, reuses
`SelectedClassSubtitleBatchReadQuery` and the active-session adapter in the
class-delete chooser. For nonempty class IDs, it batches subtitle labels when
both services are available while preserving class-list order and IDs,
class-list errors, empty-list early return, blank item and selection/cancel
behavior, and the stored-name/`Class N` fallback when either service is
unavailable. Class-data failures retain default formatting; teacher-data
failures retain class details with `No Teacher`. After selection, confirmation
still makes a fresh one-class read; an integration test changes the subtitle
after chooser population and checks the fresh confirmation text.

The two-class integration test asserts one batch class-repository call, one
metadata statement, one schedule statement, one teacher batch statement,
original labels/order, selected ID, and deletion. The empty-list UI test asserts
no batch reads and no modal. Fresh independent Windows x64 MSVC/Ninja Debug
configure passed the ownership gate at 1,270 handwritten sources. `ClassMngr`,
`ClassMngrNavigationTeacherReadTests`, the classes-list query target, and single
and batch subtitle application-query and adapter targets built; six focused
CTests passed. Independent source review confirmed one guarded batch query in
the chooser and no per-class single query; the separate confirmation read
remains. `git diff --check` passed. Logs are under
`build/f302_verify_ninja/` (`configure.log`, `build-targets.log`,
`build-targets-recheck.log`, `ctest-focused.log`). Nonfatal `vswhere.exe` and
optional Vulkan messages occurred. The full suite was not run.

Coverage limits: the fallback for class-service available/teacher-service
unavailable is source-reviewed but not directly integration-tested; current
services derive availability from the same database session, and the existing
no-session test exits before showing the chooser. No chooser-cancel or
all-item-ID enumeration test was added; the integration test verifies label
order and selects/deletes the Beta ID.

F303 is selected to batch `RosterPrintDialog` per-class selected-subtitle reads
after F261 while preserving its current-class-only branch. F298 remains
deferred pending the read-failure warning/navigation decision, and F299's
separate completeness audit remains distinct from Batch 3 discovery. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F303 accepted; F304 selected)

F303, committed as `27914312e1a7539ba55d4059847ca583c5cda9a6`, batches
`RosterPrintDialog` subtitle reads in the normal class-list path. It preserves
class-list order and IDs, label formatting, default class/teacher failure
behavior, checked state, selected IDs, and the current-class display label.
The current-class-only path remains on its separate
`TestingClassDetailsReadQuery` branch and returns before normal list/batch
reads.

The three-class integration test asserts exact labels, order, IDs, and checked
state; three requested IDs; one batch call; one metadata and one schedule SQL
statement; and one teacher batch call and statement. The current-class-only
test retains testing-class display, ID, and checked assertions and verifies
zero batch metrics. Fresh independent Windows x64 MSVC/Ninja Debug configure
passed the CMake ownership gate at 1,270 handwritten sources. `ClassMngr`,
`ClassMngrRosterPrintDialogTests`, and the batch application-query and platform
adapter targets built. These three focused CTests passed:
`ClassMngrRosterPrintDialogTests`,
`ClassMngrNextApplicationSelectedClassSubtitleBatchReadQueryTests`, and
`ClassMngrNextPlatformApplicationServicesSelectedClassSubtitleBatchReadPortTests`.
`git diff --check` passed. Logs are under `build/f303_verify_ninja/`
(`configure.log`, `build-targets.log`, `ctest-focused.log`, and
`diff-check.log`). Optional `vswhere.exe`, pthread-probe, Vulkan, and line-ending
messages were nonfatal. The full suite was not run.

F304 is selected to batch `RosterPrintDialog` extra-column roster reads after
F264, preserving scope, column union, and failure fallback. F298 remains
deferred pending the read-failure warning/navigation decision, and F299's
separate completeness audit remains distinct from Batch 3 discovery. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F304 accepted; F305 selected)

F304, committed as `2852290e7c51709e8c64d3361ff5ec1bad2d9366`, adds a Qt-free
typed batch roster-extra-info Application contract and active-session Platform
adapter. `RosterPrintDialog` passes resolved IDs in their existing order to one
query, then maps name-only results back to the existing roster slots. The
existing union/filter/selection flow remains. The repository prepares and
executes one query selecting only `roster_columns` class IDs and names, ordered
by request order, column position, and ID. Empty or missing-column classes
return successful empty slots. No roster rows, widths, or output-only limit are
included. Query failure remains silent with no extra columns; the class-list
error path remains intact.

Independent fresh Windows x64 Debug/Ninja verification passed the source-
ownership configure gate at 1,276 handwritten files (1,278 workspace inventory
after platform filtering). `ClassMngr` and six focused targets built; six
focused CTests passed for the dialog, print service, batch Application
contract, existing roster read, batch Platform adapter, and roster-output
source. `git diff --check` passed. Logs are under
`build/f304_verify_ninja/` (`configure3.log`, `build.log`, and
`focused-ctest.log`). The full suite was not run. No runtime SQL trace or
counter asserts statement count; accepted evidence is the one prepared
repository query and one batch use-case call visible in source. Keep this fresh
verification tree for continued Phase 2 work.

F305 is selected to batch transfer-menu target metadata, capacity, and roster
reads after F259/F265; its loop is the existing Batch 3 item and is
distinct from F273's transfer-time target read. Batch 4 records F306-F315. The
fixed up-to-four-evaluation roster score-import read remains a separate
candidate for Batch 5 discovery at F314 start; it has no F number yet. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F305 accepted; F306 selected)

F305, committed as `c4720ba67ff6d5f35e26ee9da017f2d0c369293e`, batches
transfer-menu candidate subtitle metadata with
`SelectedClassSubtitleBatchReadQuery` after source metadata/class-list flow.
Eligible targets use one typed batch capacity use case and active-session
Platform call. Its Application contract returns only class IDs and first-empty-
row indices. A Qt-free roster-column projection is shared by `RosterModel` and
the capacity adapter; the adapter receives existing `Roster::BaseColumns`, and
the shared 25-row limit applies to both UI and capacity. The repository reuses
the ordered-column-name batch query and adds a forward-only batched sparse-cell
stream for row indices 0-24, processing capacity incrementally without
returning full roster snapshots or cell text to the menu. F273's fresh
transfer-time target roster read is unchanged.

Menu labels/order and class-field/teacher-field failure distinctions remain.
Capacity failure stays silent and fail-open to the first slot; class-field
metadata failures exclude a target, teacher failure keeps class fields with
`No Teacher`, and the class-list warning path remains. No runtime query-count
or memory instrumentation was added; batching and bounded streaming are
supported by source inspection.

Executor verification configured `build/f304_verify_ninja`, passed source
ownership at 1,283 handwritten files, built affected production/test targets,
and passed focused CTest 8/8 plus `git diff --check` (line-ending conversion
warnings only). An independent Tester reconfigured the same tree, rebuilt the
touched dependency graph in 327 Ninja steps, built
`ClassMngrRosterModelTests`, `ClassMngrRosterTransferMenuTests`,
`ClassMngrNextApplicationRosterAvailabilityBatchReadQueryTests`, and
`ClassMngrNextPlatformApplicationServicesRosterReadPortTests`, then passed
focused CTest 4/4 and `git diff --check`. The tester log is
`build/f304_verify_ninja/Testing/Temporary/LastTest.log`. These are separate
focused passes; the full suite was not run.

F306 is selected to batch My Classes compact class-information reads after
F291, preserving class-info/default/failure outcomes separately from roster
and teacher inputs. Batch 4's remaining F307-F315 order is unchanged; at F314
start, discover Batch 5 and reconsider the fixed up-to-four-evaluation roster
score-import read. F298 remains deferred pending the read-failure
warning/navigation decision, and F299's separate completeness audit remains
distinct from Batch 3 discovery. Phase 2 remains In Progress/Open; Gates 1 and
2 remain Partial.

### Progress update - 2026-10-04 (F306 accepted; F307 selected)

F306, committed as d5b8710a7ab6c457a6ad4394e98c4ee69ef5e2db, adds a typed batch
Application query/port and an active-session Platform adapter for My Classes
compact class information. ClassInfoRepository reads metadata, regular
schedules, and intensive schedules in one batch call using three set-based
statements, preserving class-list order and schedule row order. Each class
keeps its own success or failure result. Missing metadata remains a successful default;
batch statement failure retries class reads individually. The page preserves
its silent default fallback, and roster counts and full teacher profiles
remain separate inputs.

Executor self-check passed. Fresh independent Windows x64 Debug/Ninja configure
validated 1,289 handwritten source files; ClassMngr and the affected page,
Application, and Platform targets built. Focused CTest passed 3/3:
ClassMngrMyClassesPageTests,
ClassMngrNextApplicationMyClassesClassInformationBatchReadQueryTests, and
ClassMngrNextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests.
git diff --check passed. The full suite and other platforms were not run.
Independent logs are under build/f306v/ (configure.log, build.log, ctest.log);
executor logs are under build/p2_f291_impl_ninja/ and build/.

F307 is selected to batch My Classes assigned-teacher profile reads after F270,
preserving class-to-teacher association, class order, and profile-failure
behavior. F308-F315 remain ordered in Batch 4. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F307 accepted; F308 selected)

F307, committed as
`4c4c1e0d20aa097308465eb2ad50bd279d9e29c7`, batches My Classes full assigned-
teacher profiles after F270. Only IDs from successful F306 class-information
results are requested. The page deduplicates them in first-seen class order,
then reuses each profile for all associated classes while preserving class
order. The batch projection includes all 14 profile fields. The successful
path uses one set-based SQL statement; batch SQL failure falls back to
individual reads. Per-teacher failures stay isolated and keep the silent
Unassigned fallback; roster reads remain separate.

Executor self-check and fresh independent Windows x64 Debug/Ninja verification
passed. The independent configure validated 1,295 handwritten source files;
ClassMngr and all five focused test targets built. Focused CTest passed 5/5:
ClassMngrMyClassesPageTests, both My Classes teacher-profile Application and
Platform tests, and both F306 class-information Application and Platform
regression tests. `git diff --check` passed. The full suite and other platform
builds were not run. Independent logs are under build/f307v/ (configure.log,
build.log, focused_ctest.log); executor logs are under build/p2_f291_impl_ninja/
and build/.

F308 is selected to batch My Classes roster-backed student-count reads after
F283, preserving exact English/Korean selection, QString trimming, and
zero-on-failure behavior. F309-F315 remain ordered in Batch 4. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F308 accepted; F309 selected)

F308, committed as
`cff4c0dcfb0893cb43e473096247cc8a28c51be8`, batches My Classes roster-backed
student-count reads after F283. A narrow Application query and active-session
Platform port carry per-class count results; the repository uses set-based
column and sparse-cell reads, preserving the first exact English/Korean
columns, QString trimming, one count per row, and zero-on-failure behavior.
Query failures retry per class, and counts remain separate from class
information and teacher profiles.

Executor self-check and fresh independent Windows x64 Debug/Ninja verification
passed. The independent configure validated 1,301 handwritten source files;
ClassMngr and all eight focused targets built. Focused CTest passed 7/7:
ClassMngrMyClassesPageTests, both F308 student-count Application/Platform
tests, both F306 class-information Application/Platform regressions, and both
F307 teacher-profile Application/Platform regressions. `git diff --check`
passed. Independent logs are under build/f308v/ (configure.log, build.log,
focused_ctest.log); executor logs are under build/p2_f291_impl_ninja/ and
build/. The full suite and other platform builds were not run.

F309 is selected to batch Sub Prep information-sheet per-class roster counts
after F179, preserving schedule order, zero fallback, and equivalent read
metrics. F310-F315 remain ordered in Batch 4. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F309 accepted; F310 selected)

F309, committed as
`a87edb3147577cf52d92e35e5d8587792df7883a`, batches Sub Prep information-sheet
roster count reads after schedule and teacher filtering. The print-source port
requests counts only for included classes, preserves source order, and leaves
each class in place with a zero count when its roster read fails. The
repository reads roster columns and sparse cells in set-based statements,
preserving exact first English/Korean selection, QString trimming, and one
count per row.

Successful class counts emit the legacy `sub-prep-roster-query` metrics with
the original class ID, column count, materialized row count, cell count, and
returned student count. Missing name headers still emit a zero-count event;
failed per-class reads emit no event. Event failures stay isolated from the
class output.

Executor self-check and fresh independent Windows x64 Debug/Ninja verification
passed. The independent configure validated 1,301 handwritten source files;
ClassMngr and all seven focused test targets built. Focused CTest passed 7/7:
the My Classes student-count page/Application/Platform regression tests, both
Sub Prep print-source Application/Platform tests, the Sub Prep print-source
mapper test, and the DataService lifecycle test. `git diff --check` passed.
Independent logs are under build/f309v/ (configure.log, build.log,
focused_ctest.log, diff_check.log); executor logs are under build/.
The full suite was not run.

Read-only discovery for F310 confirmed that an unreadable matched teacher
profile currently displays “New Teacher,” and F311 discovery confirmed that
classes without a meeting in the selected scope are omitted from roster output.
The active batch text now records those existing behaviors. F310 is selected
to batch matched-teacher alternative display-name reads, separate from F300
class subtitles. F311-F315 remain ordered in Batch 4. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F310 accepted; F311 selected)

F310, committed as
`825e5b898990b918daad2f3cbb530704264eb7d0`, batches matched-teacher
alternative display-name reads in ClassImportDialog, independently from F300
class-subtitle reads. It reads unique positive teacher IDs for package-backed
preview rows once, then maps names over the original match lists, preserving
choice order and duplicate choices. Missing profiles retain the existing
“New Teacher” label; a batch failure falls back to individual profile reads
so successful siblings remain available. Empty and skipped rows perform no
batch read.

Executor self-check and fresh independent Windows x64 Debug/Ninja verification
passed. The independent configure validated 1,301 handwritten source files;
ClassMngr and all three focused test targets built. Focused CTest passed 3/3:
ClassMngrClassTransferTests and the F300 class-subtitle Application/Platform
regression tests. `git diff --check` passed. Independent logs are under
build/f310v/ (configure.log, build.log, focused_ctest.log, diff_check.log);
executor logs are under build/. The full suite was not run.

F311 is selected to batch Sub Prep roster-output per-class class-name and
compact metadata reads. Read-only discovery confirmed that classes without a
meeting in the selected scope are omitted, class and metadata identities are
validated, and any source read failure aborts before package output begins.
F312-F315 remain ordered in Batch 4. Phase 2 remains In Progress/Open; Gates 1
and 2 remain Partial.

### Progress update - 2026-10-04 (F311 accepted; F312 selected)

F311, committed as
`167bb72f8d96fe56dc24e2206f1ca5a4761dfc74`, batches Sub Prep roster-output
class-name and compact metadata reads for selected classes with meetings in
the selected schedule scope. Names and class information are fetched in one
ordered batch each. Classes without selected meetings remain omitted and
schedule order is preserved. Missing class-information rows and query errors
fail instead of producing blank metadata. Class, class-information, teacher
assignment, and teacher identity checks remain. Teacher-profile and bounded
roster reads remain separate; output bounds are unchanged.

Executor self-check and fresh independent Windows x64 Debug/Ninja/MSVC
verification passed. The independent configure validated 1,301 handwritten
source files; ClassMngr and all three focused test targets built. Focused
CTest passed 3/3: the Sub Prep package-service, Application source-query, and
Platform source-port tests. They cover reduced class/metadata read fan-out,
ordering, scope omission, query failures, missing metadata, bounds, and no
final package left after a source-read failure. `git diff --check` passed.
Independent logs are under build/f311v2/ (configure.log, build.log,
focused_ctest.log, diff_check.log); executor logs are under build/. The full
suite was not run. Temporary package staging may be created before the source
read and is cleaned on failure; no final package is committed.

F312 is selected to use a purpose-fit projection for initial-setup teacher
choices, preserving ID, Korean and English names, preferred romanization/name,
repository order, validation, and error behavior. Read-only discovery
confirmed the current read selects all teacher columns in one query; the
choice projection needs only the ID and four raw display fields. F313-F315
remain ordered in Batch 4; at F314 start, discover Batch 5 and reconsider the
fixed up-to-four-evaluation roster score-import read. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F312 accepted; F313 selected)

F312, committed as
`334920b61237ffa733c29a2445c44079b576ffcc`, adds a purpose-fit repository
projection for initial-setup teacher choices. It selects only teacher ID,
Korean name, English name, preferred romanization, and preferred name,
ordered by `teacher_en`. The Platform port maps raw fields into the existing
snapshot; `getAllTeachers()` and its other consumers remain unchanged.

Executor self-check and fresh independent Windows x64 Debug/Ninja/MSVC
verification passed. The independent configure validated 1,301 handwritten
source files; ClassMngr and all three focused test targets built. Focused
CTest passed 3/3: the initial-setup Platform port, Application query, and
wizard tests. Coverage checks field mapping, raw strings, repository order,
empty results, session/query failures, and wizard presentation behavior.
`git diff --check` passed. Independent logs are under build/f312v/
(configure.log, build.log, focused_ctest.log, diff_check.log); executor logs
are under build/. The full suite was not run.

F313 is selected to use a purpose-fit projection for testing-teacher
choices, preserving teacher ID, Korean name, room, repository order, and raw
values for the page to filter and display. Read-only discovery confirmed
that the page removes blank Korean names, restores selection by ID where
possible, and otherwise selects the built-in None choice. Unavailable sessions
remain nonrecoverable NotFound errors; repository errors remain recoverable
Technical errors. F314-F315 remain ordered in Batch 4; at F314 start,
discover Batch 5 and reconsider the fixed up-to-four-evaluation roster
score-import read. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### Progress update - 2026-10-04 (F313 accepted; F314 selected)

F313, committed as
`443f6bb4d44829c2cbc5e6480db935e96a22a58b`, adds a purpose-fit repository
projection for testing-teacher choices. It selects only teacher ID, Korean
name, and room, ordered by `teacher_en`; the Platform port preserves raw
values. The page continues to filter blank Korean names, trim display/room
values, preserve repository order, and restore selection by ID or fall back
to None. Unavailable sessions remain nonrecoverable NotFound results;
repository errors remain recoverable Technical results.

Executor self-check and fresh independent Windows x64 Debug/Ninja/MSVC
verification passed. The independent configure validated 1,301 handwritten
source files; ClassMngr and the Platform, Application, and isolated page
targets built. Focused CTest passed 3/3, including the four F313 page cases
for ordering/selection, empty choices, silent NotFound handling, and
warning behavior on query failure. `git diff --check` passed. Independent
logs are under build/f313v/ (configure.log, build.log, ctest.log,
page_focused_verbose.log, diff_check.log); executor logs are under build/.
The broad ClassMngrTestingClassesPageTests suite was not run: the executor
previously hit a 300-second stall in `outputAvailabilityFollowsRosterTabAndLoadedClass`,
before the F313 cases. The focused F313 registration passed independently.

F314 is selected for a purpose-fit projection for co-teacher choices,
preserving exact profile/network fields, repository order, ID validation,
and error behavior. At F314 start, discover Batch 5 and reconsider the fixed
up-to-four-evaluation roster score-import read. F315 remains in Batch 4;
F298 remains deferred pending its warning/navigation decision. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F314 accepted; F315 selected)

F314, committed as
`15ffd504d2e5778488eddb529b88fcf4a101d923`, adds a distinct purpose-fit
repository projection for co-teacher choices. It selects teacher ID, Korean
and English names, room, internet type, Wi-Fi name/password, projection
type, and Zoom ID/password, ordered by `teacher_en`. The Platform port
preserves raw values; the broader `getAllTeachers()` and F312/F313 paths
remain unchanged.

Executor self-check and fresh independent Windows x64 Debug/Ninja/MSVC
verification passed. The independent configure validated 1,301 handwritten
source files; ClassMngr and all three focused targets built. Focused CTest
passed 3/3: the co-teacher Application query, Platform port, and feature
page tests. Coverage checks ordered/raw projection, empty results, invalid
IDs, recoverability, and page warn/clear-state behavior. `git diff --check`
passed. Independent logs are under build/f314v/ (configure.log, build.log,
ctest_corrected.log, diff_check_final.log); executor logs are under build/.
The full suite was not run; missing-repository and exception branches were
source-reviewed but not explicitly injected.

At F314 start, two independent read-only audits recorded Batch 5 (F316-F322).
The roster score-import read remains distinct from F299 analytics; six
additional fan-out/projection candidates were recorded, and no other strong
candidates surfaced. F315 is selected for a purpose-fit Korean teacher
birthday-directory projection, preserving birthday/name/preferred-display
fields, raw values, repository order, and downstream filtering. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F315 accepted; F316 selected)

F315, committed as
`82bad469d008b30e44006707faaecf5aab2feaa5`, adds a purpose-fit repository
projection for the Korean teacher birthday directory. It selects birthday,
Korean and English names, preferred romanization, and preferred name, ordered
by `teacher_en`. The Platform port preserves raw values and repository order;
`getAllTeachers()` and Native English/GS birthday reads remain unchanged.

Executor self-check and fresh independent Windows x64 Debug/Ninja/MSVC
verification passed. The independent configure validated 1,301 handwritten
source files; the Application query, Platform port, and navigation targets
built. Focused CTest passed 3/3. Tests cover the exact projection and order,
raw values, empty success, unavailable-session and repository-failure
mapping, and Korean-source warning behavior. `git diff --check` passed.
Independent logs are under build/f315v/; executor logs are under build/.
The full suite was not run; exception mapping was source-reviewed but not
explicitly injected.

F316 is selected from the previously recorded Batch 5: batch roster score-
import evaluation reads while preserving fixed column order, absent-column
skips, per-evaluation failure isolation, and score assignment. F298 remains
deferred pending its warning/navigation decision. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F316 accepted; F317 selected)

F316, committed as
`e9a4705c6bf31235b84da1ae60947b0b80103ae7`, batches the roster score-import
read for present Winter, Speech Contest, Summer, and Fall destinations. The
normal path uses one ordered set-based read for evaluation identities and all
11 raw score cells; empty requests do no work, and missing or empty evaluations
remain successful empty results. A failed or invalid batch result falls back
to the existing per-evaluation read in destination order, preserving error
isolation. The existing score parser and roster assignment behavior remain.
The shared single-read contract and F299 Class Analytics path are unchanged.

Executor self-check and fresh independent Windows x64 Debug/Ninja/MSVC
verification passed. The independent configure validated 1,305 handwritten
source files; the Application, Platform, and roster-editor targets built.
Focused CTest passed 3/3. Coverage checks batch ordering and row mapping,
empty input, missing/empty evaluations, fixed destination behavior, and
failure fallback where an earlier retry fails and a later destination imports.
`git diff --check` passed. Independent logs are under build/f316v/;
executor logs are under build/. The full suite was not run; malformed-response
fallback was reviewed by source control flow but was not tested in combination
with a rejected batch response.

F317 is selected from Batch 5 to batch Sub Prep roster-output reads for
distinct assigned teacher profiles, preserving selected-scope filtering,
first-seen association, teacher identity validation, and source-error
behavior. F298 remains deferred pending its warning/navigation decision.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F317 accepted; F318 selected)

F317, committed as
`c17bfcc9d25030984b485c44c7c21483c575ff9a`, removes per-class teacher-profile
reads from the Sub Prep roster-output source port. It gathers distinct
positive teacher IDs after schedule-scope filtering and in selected-class
order, then uses the existing batched profile repository read once. The port
preserves first-seen association, profile fields, identity checks, errors,
and all-or-failure source behavior.

Executor self-check and fresh independent Windows x64 Debug/Ninja/MSVC
verification passed. The independent configure validated 1,305 handwritten
source files; the Sub Prep Platform, Application, and package-service targets
built. Focused CTest passed 3/3. Tests cover batch count, first-seen ordering
and deduplication, empty teacher assignments, stale-profile failure, and no
partial source. An independent review found the out-of-scope teacher was not
distinct in the scope fixture; that fixture was strengthened with a distinct
teacher and independently rechecked, passing the Platform target 1/1.
`git diff --check` passed. Logs are under build/f317v/ and
build/p2_f317_coverage_*; the full suite was not run.

F318 is selected from Batch 5 to batch roster-template print per-class
class-information and roster reads while preserving class order, current-class
name fallback, printed fields, and abort-on-read-failure behavior. F298 remains
deferred pending its warning/navigation decision. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F318 accepted; F319 selected)

F318, committed as
`76b8b85fedc4e78c4a8e5990024e9469ff10c67a`, adds a print-specific batch
source query for full class-information/schedule and roster data. The normal
path uses two ordered class-info reads, a roster columns/widths read, and a
cell read only when at least one requested roster has columns. It preserves
class order, missing-class-info blank defaults with schedule data, empty
rosters, sparse row padding, printed fields, and the current-class name
fallback. Failed or invalid batch results use the original per-class
class-info-then-roster path, preserving failure order and no-partial-output.

Executor self-check and fresh independent Windows x64 Debug/Ninja/MSVC
verification passed. The independent configure validated 1,311 handwritten
source files; the new Application/Platform batch targets, print service, and
existing class-info/roster query and adapter targets built. Focused CTest
passed 7/7, and `git diff --check` passed. Logs are under build/f318v/;
executor logs are under build/. The full suite was not run; malformed batch
identity fallback was source-reviewed and the error/fallback behavior was
covered separately, but not in a combined service test.

F319 is selected from Batch 5 to batch Class Analytics typed evaluation reads,
preserving canonical evaluation order, missing-evaluation-as-empty behavior,
roster filtering, YTD cohorts, and whole-dashboard failure behavior. F298
remains deferred pending its warning/navigation decision. Phase 2 remains
In Progress/Open; Gates 1 and 2 remain Partial.

### Progress update - 2026-10-04 (F319 accepted; F320 selected)

F319, committed as
`7a4dad745be6709f04fcebbe1ca717cf7ed3adab`, batches Class Analytics' four
canonical evaluation reads into one typed port call and one class-scoped SQL
query. The projection contains only English/Korean names and six score fields
(`col_1`–`col_8`); comments and notes remain outside the Analytics contract.
Missing and present-but-empty evaluations remain empty slots, and any batch
read failure fails the whole dashboard. The roster-first order, filtering,
selection behavior, YTD cohorts, and other calculations remain unchanged.

Executor self-check and fresh independent Windows x64 Debug/Ninja/MSVC
verification passed. The independent configure validated 1,311 handwritten
source files; the Application, Platform, and page targets built. Focused CTest
passed 3/3, and `git diff --check` passed. Logs are under build/f319v/;
executor logs are under build/. The full suite was not run; one-query behavior
was confirmed by source review, not an instrumented statement-count test.

F320 is selected from Batch 5 for a purpose-fit Speaking Evaluation
roster-name projection, preserving English/Korean column selection, row order,
trimming, and name-pair matching. F298 remains deferred pending its
warning/navigation decision. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### Progress update - 2026-10-04 (F320 accepted; F321 selected)

F320, committed as
`6d7532d380c79fb42cf8a173dbe4d0586a184e3a`, adds a purpose-fit roster-name
projection for Speaking Evaluation. Both Import Names and duplicate-name
resolution now receive ordered raw English/Korean pairs, first matching
case-insensitive headers, and legacy row-existence information without loading
full roster cells. Sparse gaps, unrelated blank data rows, no-column behavior,
warning order, and existing trimming/matching logic are preserved.

Executor self-check and fresh independent Windows x64 Debug/Ninja/MSVC
verification passed. The independent configure validated 1,317 handwritten
source files; the new Application/Platform targets, name-import plan, and
Speaking Evaluation page targets built. Focused CTest passed 4/4, and
`git diff --check` passed. Logs are under build/f320v/; executor logs are
under build/. The full suite was not run. The review found no defect; tied
duplicate-header positions and NULL selected name cells were source-reviewed
but not directly tested.

F321 is selected from Batch 5 for purpose-fit Native English and GS Team
birthday projections, preserving schedule fields, source order, and warning
behavior. F298 remains deferred pending its warning/navigation decision.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Slice discovery update - 2026-10-04 (Batch 6 recorded at F321 start)

Two independent read-only sweeps were compared at the second-last slice of
Batch 5. Batch 6 records ten ordered candidates, F323-F332, across Schedule
Import, Class Transfer, Sub Prep, Class Notes, and Co-Teacher paths. The
Class Transfer package-export work is separated into evaluation rows, assigned
teacher profiles, and class-information/full-roster reads so each slice can
retain its own field mapping and failure order. The candidates are distinct
from accepted F316-F320 and active F321-F322. Deferred F285/F298 remain
unchanged.

### F321 accepted / F322 selected - 2026-10-04

F321, committed as
`0cf920978c8958c6596e31af1eaa254b5cea1357`, adds purpose-fit birthday
projections for Native English and GS Team. Native English reads raw name,
position, and birthday; GS Team also reads Korean name for display fallback.
Both projections preserve repository ordering, including GS's exact-empty-name
fallback, and retain blank/null-mapped rows for existing downstream handling.
The shared full-directory readers remain unchanged for staff-directory use.
The Sidebar still attempts both reads and keeps its existing warning,
Native-error-precedence, and dialog behavior.

Fresh independent Windows x64 Debug/Ninja/MSVC configure and build passed; the
configure validated 1,329 handwritten source files. Focused CTest passed 9/9,
including new Application and Platform projection coverage, four
full-directory regressions, and `NavigationTeacherRead`. `git diff --check`
passed. Logs are under `build/f321v/`. One query per source was verified by
source review rather than SQL-count instrumentation.

F322 is selected from Batch 5 to reduce Class Analytics roster-name fixed SQL
reads while preserving sparse-row sizing and the compact English/Korean
projection. Batch 6 (F323-F332), recorded at F321 start, remains queued for
after Batch 5. F298 remains deferred; Phase 2 remains In Progress/Open, with
Gates 1 and 2 Partial.

### F322 accepted / F323 selected - 2026-10-05

F322, committed as
`76861fcb78df99be4c394bf2cf63864e14984dca`, changes `readRosterNames()` to
use the existing zero-column guard and one combined SQL query for header
indices, maximum valid materialized row sizing, and selected name cells (at
most two SQL reads, down from up to four). It preserves exact `English` and
`Korean` header selection by `(position,id)`, duplicate first-header choice,
all-column sparse-row sizing, empty holes, compact name values, and the
no-column short-circuit.

The Qt-free Application contract, dashboard read order, and four-evaluation
batch are unchanged. Tests cover sparse sizing from unrelated Notes cells,
blank holes, exact and case-mismatched headers, duplicate and absent headers,
compact projection, empty-roster success without `roster_data`, and structured
Technical failure when columns exist but `roster_data` does not.

Fresh independent Windows x64 Debug/Ninja/MSVC configure validated 1,329
handwritten source files; the target built, focused CTest passed 1/1, and
`git diff --check` passed. Evidence is in
`build/p2_f322_independent_configure.log`,
`build/p2_f322_independent_build.log`,
`build/p2_f322_independent_ctest.log`, and
`build/p2_f322_independent_diff_check.log`. The query-count improvement was
verified by source review; no runtime SQL instrumentation was added. F323 is
selected from Batch 6 to batch Schedule Import apply-time class-information
reads while preserving class order, per-class defaults, schedule fields, and
transaction error behavior. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### F323 accepted / F324 selected - 2026-10-05

F323, committed as
`e6bdd44d05f2d4c8de8a906e6def051f87d3e3b6`, batches apply-time class
information reads in `ScheduleImportRepository::applyCore`. After transaction
start and the existing teacher/class reads, apply loads class information once
with `ClassInfoRepository::loadScheduleClassInfos()` before validation or
writes, indexes the results by ID, and retains `existingClasses` iteration
order for downstream validation. The separate snapshot path and preview reads
are unchanged.

Tests cover reversed class/batch order, defaults, regular and intensive
schedules, repository batch metrics, and a missing intensive-table batch-read
failure before writes; persisted state remains unchanged and the connection
can begin a later transaction. Fresh independent Windows x64 Debug/Ninja/MSVC
configure validated 1,329 handwritten source files; the target built in 316
steps, focused CTest passed 1/1, and `git diff --check` passed. Evidence is
under `C:\Users\wfelt\AppData\Local\Temp\f323_verify_20261005_72b3b7e8\`.
The batch-call count was confirmed by source review; applyCore has no local
metrics seam. F324 is selected from Batch 6 to batch Class Transfer
package-export evaluation-row reads while preserving class and evaluation
order, sparse row indexes, full cells, and abort behavior. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### F324 accepted / F325 selected - 2026-10-05

F324, committed as
`1368077f9477e88b05bb4094cb30d3d3f33eb7a7`, keeps the per-class ordered
evaluation-metadata query and its no-row-query short circuit when metadata is
empty. Otherwise, one class-scoped `speaking_eval_data` query replaces one
query per evaluation. Results map by evaluation ID, select all 11 cells, keep
25 row slots and sparse indexes, skip out-of-range indexes, and map NULL to
empty.

Class order, transactional package construction, query-error propagation, and
controller build-before-save behavior remain unchanged. Tests cover class and
evaluation order, sparse holes, all cells, NULL-to-empty, out-of-range rows,
no evaluations when the row table is absent, and row-read failure with an
evaluation present. Fresh independent Windows x64 Debug/Ninja/MSVC configure
validated 1,329 handwritten sources; the class-transfer target built, focused
CTest passed 1/1, and `git diff --check` passed. Logs are
`build/p2_f324_independent_configure.log`,
`build/p2_f324_independent_build.log`,
`build/p2_f324_independent_build_final.log`, and
`build/p2_f324_independent_ctest_retry.log`. The query bound was confirmed by
source review; no runtime SQL instrumentation or full-suite run was used.
F325 is selected to batch Class Transfer plan-validation schedule reads while
preserving destination order, replaced-class skips, schedule conflict
results, and failure behavior. Phase 2 remains In Progress/Open; Gates 1 and
2 remain Partial.

### F325 accepted / F326 selected - 2026-10-05

F325, committed as
`af6e50e6c489c5b6bee265f9acc3c584c76e60f0`, batches Class Transfer schedule
preflight reads. It excludes replaced destinations, loads navigation records
for eligible destination IDs in one batch, and validates those records in
destination order, preserving regular and intensive schedule conflict order.
The read includes schedule rows when a destination has no `class_info` record;
batch-read failures still abort before writes.

Tests cover schedules without class information, destination order and
replaced/skipped-class exclusions, and read failure before writes. Fresh
Windows x64 Debug/Ninja/MSVC verification ran the focused
`ClassMngrClassTransferTests` CTest successfully (1/1); committed-patch
whitespace check passed. The test log is under
`build/f325_fresh_ninja/Testing/Temporary/LastTest.log`. No full suite ran.
F326 is selected to batch Sub Prep information-sheet assigned-teacher profile
reads while preserving selected scope, first-seen association, missing-profile
behavior, and read failures. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### F326 accepted / F327 selected - 2026-10-05

F326, committed as
`b6851ceac872752038e39b08a112b5ed63430946`, batches Sub Prep information-sheet
assigned-teacher profile reads. It deduplicates positive teacher IDs in first
reference order, loads profiles once, validates the returned count and order,
and reuses profiles for repeated assignments. Unassigned classes cause no
teacher-profile read; selected class order is preserved.

Tests verify first-reference order, repeated-teacher deduplication, the
unassigned no-read path, and batch metrics. Fresh Windows x64 Debug/Ninja/MSVC
verification passed the focused
`ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests` CTest
(1/1); committed-patch whitespace check passed. The CTest log is under
`build/f326_fresh_ninja/Testing/Temporary/LastTest.log`. Limitation: direct
technical profile-read error classification is not exercised through this
port because the earlier `loadClassInfosForScheduleScope` query filters orphan
teachers; repository batch-fallback tests exist. No full suite ran. F327 is
selected to batch Schedule Import teacher reads across current-state snapshot
and apply validation while preserving teacher order, Korean-name matching,
room data, and failure behavior. Exclude stale preview unless an active caller
is found. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F327 accepted / F328 selected - 2026-10-05

F327, committed as
`e4409e917f9cdaadfeefe14ef0cdfa80129be569`, adds a purpose-fit teacher read
for Schedule Import containing only ID, Korean name, and room number, ordered
by English name. The current-state snapshot port and apply validation use it
instead of loading full teacher records, preserving repository order,
Korean-name matching, room data, and failure behavior; preview reads remain
unchanged.

The snapshot regression preserves repository order and raw Korean-name and
room values, and checks one query. The apply regression verifies a teacher-read
failure rolls back before writes. Fresh independent Windows x64 Debug/Ninja/
MSVC configure validated one owner for 1,329 handwritten sources; both the
snapshot-port and `ClassMngrScheduleImportTests` targets built, focused CTest
passed 2/2, and `git diff --check` passed. Evidence is under
`build/f327_independent_20261005_ninja/`. No full suite ran. F328 is selected
to batch Class Transfer preview destination-matching inputs while preserving
class order, conditional teacher-name reads, matching behavior, and errors.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F328 accepted / F329 selected - 2026-10-05

F328, committed as
`940d4711eb872321df6047b2e66b3c23503e13e0`, batches Class Transfer preview
destination class information through `loadClassesNavigationRecords()`. For a
nonempty destination set, this ordered batch uses one metadata statement and
two schedule statements, independent of destination count; it is skipped when
there are no destinations. The navigation metadata batch and initial
`getAllTeachers()` query still read teacher names. The additional per-destination
`getTeacher()` profile lookup remains conditional on a positive assigned
teacher ID and a matching source-course grade/level. Destination order,
matching, and error propagation are preserved.

Tests cover destination order, conditional per-destination profile lookup,
batch-read failure, and the no-destination short circuit. Fresh independent
Windows x64 Debug/Ninja/MSVC configure validated one owner for 1,329
handwritten sources; `ClassMngrClassTransferTests` built in 316 steps and
focused CTest passed 1/1.
`git diff --check` passed. The CTest log is under
`build/f328_tester_20261005_ninja/Testing/Temporary/LastTest.log`. No full
suite ran. F329 is selected to batch Class Transfer package-export assigned-
teacher profiles while preserving first-seen teacher keys, profile identity,
and transaction errors. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### F329 accepted / F330 selected - 2026-10-05

F329, committed as
`95f96cea27d685100422f5a7cdd7b5f66e3db893`, batches distinct positive teacher
profiles in first-seen order during Class Transfer package export. It preserves
teacher keys, repeated assignments, complete teacher identity and profile
fields, and the returned error precedence. The batch query bound was confirmed
by source inspection because `buildPackage()`'s local repository has no metrics.

Tests verify teacher keys and repeated assignments, full profile identity and
fields, and teacher/evaluation/roster error precedence. Class-info precedence
and transaction rollback have no dedicated assertions; source and RAII paths
were reviewed. Staging may read later classes in the processed prefix before
replaying an earlier teacher error, but the read-only work stays in the
existing transaction and returned precedence is preserved. Fresh VS2026/Ninja
configure validated ownership for 1,329 handwritten sources; the final
incremental `ClassMngrClassTransferTests` build succeeded and focused CTest
passed 1/1. `git diff --check` passed. No full suite ran. F330 is selected to
batch Class Transfer package-export class-information and full-roster reads
while preserving class order, full output fields, sparse rows, and
first-failure behavior. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### F330 accepted / F331 selected - 2026-10-05

F330, committed as
`a8d3004323d9b399fcf8f83bf845362c43a807bb`, batches Class Transfer package-
export class information and full rosters. The class-info batch preserves all
fields/defaults and ordered regular/intensive schedules; the roster batch
preserves columns, widths, and sparse rows. `buildPackage()` falls back to
ordered scalar reads after global or malformed batch results and replays
failures in info → roster → teacher → evaluation order, with selection and
class-lookup failures deferred.

Fresh VS2026 x64/Ninja configure validated ownership for 1,329 handwritten
sources; `ClassMngrClassTransferTests` and
`ClassMngrRosterTemplatePrintServiceTests` built, focused CTest passed 2/2,
and `git diff --check` passed. No full suite ran. Successful-path query bounds
were source-inspected because there are no integration metrics: three
class-info statements and one roster-column statement plus an optional cell
statement. No synthetic misordered batch-result injection, direct query-count
metric, or export selection-size bound for the SQL `VALUES` inputs was
established. F331 is selected for purpose-fit selected-teacher display reads
for Class Notes and Co-Teacher pages, preserving preferred-name fallback and
per-page errors. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### F331 accepted / F332 selected - 2026-10-05

F331, committed as
`95ba0e1bd62182ba9d1e5310a35c9e40860f73d2`, narrows the Class Notes and
Co-Teacher teacher display reads to the shared
`TeacherRepository::loadTeacherDisplayNameFields(int)` helper. For a positive
assigned ID, each port uses one statement to load four display fields instead
of `SELECT *`; nonpositive IDs skip that read.
Both ports map the fields into `Teacher` and reuse `preferredDisplayName`:
trimmed preferred name, English name, romanization, then Korean name. This
narrowed projection does not reduce round trips; its query bound was
source-inspected because no direct scalar query-count metric exists.

Regressions cover fallback and isolated SQL projection failures while
preserving class fields; Co-Teacher assertions retain exact selected IDs.
Earlier cases cover preferred names, missing teachers, nonpositive IDs, and
independent errors. Final VS2026 x64/Ninja verification rebuilt the Co-Teacher
target after the exact-ID assertion adjustment; the combined
`NextPlatformApplicationServicesClass(Notes|CoTeacher)PageReadPort` CTest
passed 2/2. `git diff --check` was clean, with only LF-to-CRLF warnings. No
full suite ran. F332 is selected for purpose-fit class-detail reads for Class
Notes and Co-Teacher pages, preserving consumed class/schedule fields and
per-page read behavior. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### Slice discovery update - 2026-10-05 (Batch 7 recorded)

Two independent read-only sweeps found six distinct bounded candidates for
Batch 7, ranked as follows:

1. F333 - Reuse Class Transfer preview's initially loaded teacher profiles by
   ID.
2. F334 - Add a purpose-fit Class Details projection.
3. F335 - Combine Schedule Editor projection reads into one statement.
4. F336 - Narrow Selected Class Grade reads to consumed scalar fields.
5. F337 - Reuse the F332 reader for Class Details validation context.
6. F338 - Clean up the ClassImportDialog boundary.

No other slices were found.

### F332 accepted / F333 selected - 2026-10-05

F332, committed as
`760559a1dbb7fd663bb27fc9bb0d5a45b8a911d1`, adds purpose-fit class-detail
reads for Class Notes and Co-Teacher pages, preserving the class and schedule
fields they consume and each page's read behavior. Independent VS2026
x64/Ninja verification passed the focused Notes and Co-Teacher CTest 2/2.
`git diff --check` was clean except for line-ending notices. No full suite ran.

Batch 6 is complete. F333 is selected from Batch 7 to reuse teacher profiles
already loaded by Class Transfer preview's initial `getAllTeachers()` call,
keyed by ID, instead of making a `getTeacher()` profile read for each
qualifying destination class. Preserve the conditional positive assigned ID
and matching source course, first failure and returned error, destination
order, and EN/KR normalization. Retain the `getTeacher()` fallback only when
the positive conditional ID is unexpectedly absent from the loaded profiles,
preserving the existing missing-teacher error. The source-inspected bound is no
per-qualifying-class full-profile SELECT for present profiles; a rare missing-ID
fallback remains. F333 is selected, not implemented or verified. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### F333 accepted / F334 selected - 2026-10-05

F333, committed as
`062f109ea58c5e2e0e3b1bd19f69e620d1fc693d`, indexes normalized English and
Korean names from Class Transfer preview's initial `getAllTeachers()` result
by ID and reuses them for qualifying class assignments. `getTeacher()` remains
only as a fallback when the conditional positive ID is absent from that list,
preserving the existing missing-teacher error. The query bound is
source-inspected, not instrumented: present profiles cause no per-class
full-profile SELECT, while the rare missing-ID fallback remains.

Independent VS2026 x64/Ninja Debug configure validated 1,329 handwritten
source files; `ClassMngrClassTransferTests` built, focused CTest
`^ClassMngrClassTransferTests$` passed 1/1, and `git diff --check` exited 0.
No full suite ran. F334 is selected for a purpose-fit Class Details projection.
Preserve requested/canonical class IDs; grade/level, books, class/font colors;
ordered regular/intensive schedule contents; missing class-info defaults; the
same class-field error mapping; and independent teacher-display/roster results.
The target is two class-info statements (metadata plus a combined tagged
`UNION ALL` schedule read ordered by source-tag/id), compared with the three
statements in `loadClassInfo()`. F334 is selected, not implemented. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### F334 accepted / F335 selected - 2026-10-05

F334, committed as
`812c61a05835eb5c1bc1fdae5c8820aaf3b78866`, adds
`ClassDetailsPageReadRecord` with two class-info statements: metadata plus a
tagged `UNION ALL` schedule read ordered by source/id. Only the Class Details
page port is redirected. The reader preserves independent teacher/roster
results, missing-metadata defaults while retaining schedules, and the prior
regular/intensive visible error labels.

Tests cover the two-statement metrics, defaults with schedule rows,
field/order mapping, and regular/intensive SQL failures with Technical
classification and an independent roster outcome. Fresh VS2026 x64/Ninja
Debug configure passed the 1,329-file ownership audit; the focused
`ClassMngrNextPlatformApplicationServicesClassDetailsPageReadPortTests` target
built and CTest passed 1/1. `git diff --check` was clean; no full suite ran.
Configure had nonfatal Visual Studio and long-path notices. Error-label
selection reads SQLite/Qt error text for the table name; this was verified on
the current SQLite driver.

F335 is selected for a one-statement Schedule Editor class-info projection
using `LEFT JOIN`. Preserve requested/canonical and selected IDs, grade/level,
books, colors, teacher Korean name, and room; white/black color defaults;
baseline blank teacher fields for missing teacher/class-info; repository error
mapping; and non-class-info outcomes. This slice is selected, not implemented.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F335 accepted / F336 selected - 2026-10-05

F335, committed as
`65696eabe100591a9fd4985e30e7dbd1ab124667`, adds
`loadScheduleEditorClassInfoRecord()` with one purpose-fit metadata `LEFT JOIN`
for Schedule Editor. Only the consuming page port uses it, and schedules are
not read. The projection preserves the requested ID, default colors and blank
fields, including blank values for missing or dangling left-joined teachers,
and Technical failure classification.

Tests cover all fields, one-statement metrics, absent-`class_info` defaults,
dangling teachers, and a dropped metadata table. Fresh VS2026 x64/Ninja Debug
configure passed the 1,329-file ownership audit; the focused
`ClassMngrNextPlatformApplicationServicesScheduleEditorClassInfoReadPortTests`
target built and CTest passed 1/1. `git diff --check` exited 0; no full suite
ran. Configure had recurring nonfatal `vswhere` and long-path warnings.

F336 is selected to narrow the Selected Class Grade port from
`loadClassInfo()` to a one-column, one-query read. Preserve requested/canonical
class ID, grade value, blank success when metadata is missing or empty, and
Technical/error classification. F336 is selected, not implemented. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### F336 accepted / F337 selected - 2026-10-05

F336, committed as
`59bf62feb6d9a708e91dd2daaf1f460c744f8e0b`, adds a purpose-fit repository
reader that performs one `SELECT class_grade`, returns the requested ID, and
returns a blank grade when the row is missing. It preserves whitespace and
uses the existing action's Technical mapping; only the Selected Class Grade
port changed.

Tests cover the exact value ` M2 `, requested IDs, missing and empty rows, one
statement per call, and a dropped `class_info` table producing a Technical
error. Fresh VS2026 x64/Ninja Debug configure passed the 1,329-file ownership
audit; `ClassMngrNextPlatformApplicationServicesSelectedClassGradeReadPortTests`
built in 316/316 steps and focused CTest passed 1/1. `git diff --check` exited
0; no full suite ran. Configure had nonfatal `vswhere` and long-path warnings.

F337 is selected to switch the Class Details validation-context port from
`loadClassInfo()` to the F332 `loadClassPageDetails()` reader. Preserve
matched/request IDs, the teacher-ID sentinel, exact notes/filler text,
missing-row defaults, and Technical errors. The target is two statements
(metadata and regular schedule) instead of three; the planned read includes
unused regular schedules and no intensive schedules. F337 is selected, not
implemented. It is the second-last Batch 7 slice, and Batch 8 discovery is
underway. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Slice discovery update - 2026-10-05 (Batch 8 recorded at F337 start)

Two independent read-only sweeps found six bounded candidates for Batch 8, in
this order:

1. F339 - Schedule Testing class-choice projection (accepted).
2. F340 - Sub Prep roster-output schedule-scope projection (accepted; commit
   `f02a7778`).
3. F341 - My Classes assigned-teacher profile batch projection (accepted;
   commit `71ea53da`).
4. F342 - Sub Prep roster-output teacher-profile projection (selected/current).
5. F343 - Schedule Import snapshot class-info projection (queued).
6. F344 - Remove redundant compatibility-service availability gates in
   migrated Classes/My Classes (queued).

No other slices were found.

### F337 accepted / F338 selected - 2026-10-05

F337, committed as `97dfdf81` (`Phase2 - Reuse class details reader for
validation (F337)`), switches the Class Details validation-context port from
`loadClassInfo()` to F332's `loadClassPageDetails()`. It preserves mapped class
ID, teacher ID and sentinels 0 and -2, exact notes/time-filler text, and
missing-row defaults. The reader performs two metadata and two regular
schedule statements across two reads; a missing row uses one plus one. No
intensive schedule is read.

Fresh independent VS2026 x64/Ninja Debug configure validated 1,329 handwritten
sources; `ClassMngrNextPlatformApplicationServicesClassDetailsValidationContextPortTests`
built 316/316; focused CTest passed 1/1; and `git diff --check` exited 0. No
full suite ran. Configure emitted only nonfatal existing `vswhere` and
long-path warnings.

F338 is selected to move ClassImportDialog's direct
`DatabaseSession`/`TeacherRepository::loadTeacherDisplayNameRecords` batch read
behind a purpose-fit v2 application query and ApplicationServices platform
adapter. Preserve package-backed preview filtering, unique positive teacher
IDs, current display labels and fallback, and per-teacher profile fallback if
the batch operation fails. F338 is selected, not implemented. Batch 7 remains
active with only F338 remaining; Batch 8 (F339-F344) discovery is queued. Phase
2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F338 accepted / F339 selected - 2026-10-05

F338, committed as `d0bb41ab` (`Phase2 - Move import dialog teacher batch
read to v2 (F338)`), removes direct `DatabaseSession`/`TeacherRepository` batch
access from ClassImportDialog by routing it through a purpose-fit Qt-free
application query and ApplicationServices adapter. The query reads exactly
four UTF-16 display fields and validates canonical, requested, and output
identity, uniqueness, and order. It preserves package-backed
preview filtering, unique positive teacher IDs, duplicate choice rows,
`SidebarNodeNaming`, the exact `New Teacher` missing-profile label, partial
batch results, individual profile retries after batch failure, and no-read
cases.

Fresh independent VS2026 x64/Ninja configure validated 1,334 handwritten
files. Query, adapter, and ClassTransfer targets built; focused CTest passed
3/3. The added
`ClassTransferTests::importDialogRequestsOnlyUniquePositiveTeacherIds` test
was independently rebuilt and its focused CTest passed 1/1, asserting
nonpositive filtering, deduplication, original UI candidate IDs, and one
statement. `git diff --check` passed. No full suite ran; configure had known
nonfatal warnings.

F339 is selected for a purpose-fit Schedule Testing class-choice projection.
Narrow the reader to class ID/name, grade, level, and room, removing unused
teacher ID, colors, and notes from the projection. Preserve the inner
`testing_classes`-to-`classes` join, left `class_info` join and defaults,
grade/level/name/ID order, successful empty results, Technical errors, and a
one-query bound; add a statement-metric assertion and missing-class-info
test. Use focused target
`ClassMngrNextPlatformApplicationServicesScheduleTestingClassChoicesReadPort`.
F339 is selected, not implemented. Batch 7 is complete; Batch 8 is active with
F340-F344 queued. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### F339 accepted / F340 selected - 2026-10-05

F339, committed as `1d2b6810` (`Phase2 - Narrow Schedule Testing class
choices (F339)`), adds a purpose-fit repository record and read selecting class
ID, name, grade, level, and room. Only the existing v2 port consumes it;
generic `loadTestingClasses()` remains unchanged. The read preserves the
`testing_classes`-to-`classes` inner join, `class_info` left join, grade/level/
name/ID order, blank grade/level defaults when class info is missing, room,
successful empty results, Technical repository failures, and one statement.

Tests cover missing-class-info behavior and the one-statement metric. Fresh
independent VS2026 x64/Ninja Debug configure validated 1,334 files; repository
and port targets built; focused CTest passed 2/2. `git diff --check` exited 0.
No full suite ran; configure had known nonfatal warnings.

F340 is selected to narrow Sub Prep roster-output schedule-scope class reads
from `loadClassInfosForScheduleScope(..., includeUnassignedTeachers=true)` to
class ID, assigned teacher ID, and selected-scope schedule records, removing
grade/level/colors/notes repeated per meeting. Preserve selected-day/type
filtering, schedule order, output caps, unassigned-class inclusion, INNER JOIN omission/empty-success for absent
`class_info`, the separate metadata-reader error if called or if data changes
after the schedule read, identity checks, and source failure behavior. Keep the
one-query bound and add or retain an explicit one-statement assertion when
feasible. Focused target:
`NextPlatformApplicationServicesSubPrepRosterOutputSourcePort`.
F340 is selected, not implemented. Batch 7 is complete; Batch 8 remains active
with F341-F344 queued. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### Batch 8 tracking correction - 2026-10-05

During active selection, review found the F340/F341 descriptions reversed in
tracking. The recorded candidates now assign F340 to the accepted Sub Prep
roster-output schedule-scope read and F341 to the next teacher-profile
projection, aligning slice IDs with source commit `f02a7778`. No Git history
changed.

### F340 accepted / F341 selected - 2026-10-05

F340, committed as `f02a7778` (`Phase2 - Narrow Sub Prep roster schedule
reads (F340)`), adds a purpose-fit repository scope read containing only class
ID, teacher ID, and selected meetings. Sub Prep roster-output uses it; generic
`loadClassInfosForScheduleScope` and Sub Prep print remain unchanged. The
reader preserves selected day/type filtering, selected-meeting order,
unassigned-teacher inclusion, class/teacher identity, per-class and aggregate
meeting caps, and one statement with metrics.

Both old and new schedule-scope queries use an INNER JOIN to `class_info`, so
a selected schedule with no class-info row is omitted, so the port succeeds
with no output for that class. The separate metadata batch reader reports a missing-record
error if called directly or if metadata disappears after the schedule read.

Tests cover missing-class-info/empty-scope behavior, repository query errors,
unassigned teachers, filtering/order, per-class overflow, and aggregate
overflow. Fresh independent VS2026 x64/Ninja configure exited 0 and the source
ownership audit validated 1,334 handwritten sources. The focused
`NextPlatformApplicationServicesSubPrepRosterOutputSourcePort` target rebuilt
and CTest passed 1/1. The aggregate regression adds 16,385 meetings across 257
classes (each at or below 64) and asserts Validation plus one statement.
`git diff --check` exited 0. No full suite ran; configure had known nonfatal `vswhere` and line-ending warnings.

F341 is selected for the Sub Prep roster-output teacher-profile batch
projection. The consumer uses teacher ID, EN/KR names, preferred name, and
preferred romanization. Preserve package semantics, per-ID failures, order,
identity, and batch errors. F341 is selected, not implemented or verified.
Batch 8 remains active with F342-F344 queued. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Batch 8 source-alignment correction - 2026-10-05

Final source review found two active-label mismatches: F340's committed
schedule-scope implementation had been described as F341 in the original
candidate list, and F341's source commit is the My Classes candidate previously
recorded as F342. The recorded order and current selection now match the source
commit IDs. No commit history was rewritten.

### F341 accepted / F342 selected - 2026-10-05

F341, committed as `71ea53da` (`Phase2 - Narrow My Classes teacher profile
batch (F341)`), adds a Qt-free 12-field My Classes snapshot and a dedicated
repository batch read with metrics. Teacher ID remains separate. The snapshot
covers the page's consumed fields, including preferred name/romanization and
room; it omits only birthday and phone. Generic `loadTeacherProfileRecords()`
remains unchanged. Results preserve batch order and identity, per-teacher errors,
visible page values, and missing-profile behavior, including the `Unassigned`
and `N/A` fallbacks.

Fresh independent VS2026 x64/Ninja configure validated 1,334 files; three
targets built in 324 steps; focused CTest passed 3/3. `git diff --check` exited
0. No full suite ran; no material gaps were reported.

F342 is selected to narrow the Sub Prep roster-output teacher-profile batch to
teacher ID and EN/KR names, preferred name, and preferred romanization.
Preserve per-ID failure entries, returned order and identity, output fallback
labels, and one statement. Focused target:
`NextPlatformApplicationServicesSubPrepRosterOutputSourcePort`. F342 remains
selected, not implemented or verified. F343-F344 remain queued. Batch 8 is
active; Batch 7 is complete. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### F342 accepted / F343 selected - 2026-10-05

F342, committed as `fe190a1a`, narrows the Sub Prep roster-output
teacher-profile batch to teacher ID and the four consumed display fields:
EN/KR names, preferred name, and preferred romanization. It preserves per-ID
failure entries, returned order and identity, output fallback labels, and the
one-statement bound.

Fresh independent VS2026 x64/Ninja configure audited 1,334 files; the focused
snapshot/source-port target built and CTest passed 1/1. `git diff --check` was
clean. No full suite ran.

F343 is selected for the Schedule Import snapshot class-info projection,
preserving classes-then-teachers-then-schedules error precedence. F343 remains
selected, not implemented or verified; F344 remains queued. Batch 8 is active;
Batch 7 is complete. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### F343 accepted / F344 selected - 2026-10-05

F343, committed as `50dfdc80`, adds a dedicated Schedule Import snapshot
projection that omits `font_color` and class-info teacher display names. It
preserves the three-statement pattern and source precedence; generic
`loadScheduleClassInfos` remains unchanged.

Fresh independent VS2026 x64/Ninja Debug configure audited 1,334 files. The
focused `ScheduleImportStateSnapshotPort` target built and CTest passed 1/1.
Precedence cases are tested separately; independent review confirmed code order
remains Classes -> Teachers -> ClassSchedules and found no behavior regression.
`git diff --check` passed. No full suite ran.

F344 is selected to remove redundant compatibility-service availability gates
from migrated Classes/My Classes pages. Preserve the no-session early return
and query-failure warnings. F344 is selected, not implemented. Batch 8 remains
active with F344 current. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### F344 accepted - 2026-10-05

F344, committed as `1bffb008`, replaces service-level availability gates in
migrated Classes/My Classes pages with null-guarded
`ApplicationServices::hasOpenDatabase()` checks and preserves quiet behavior
when the session is closed. My Classes CTest passed 1/1 and both new
ClassesPage slots passed. The full ClassesPage CTest stalled in
`classDetailsAndCoTeacherTabsSeparateTheirSectionCards`; the stall reproduced
with the old gates restored, so it is not evidence of an F344 regression.

Fresh VS2026 x64/Ninja configure audited 1,334 sources; both targets built and
`git diff --check` was clean. No full suite ran.

### Slice discovery update - 2026-10-05 (Batch 9 recorded)

Two independent exit-gate sweeps identified these bounded follow-up candidates
in order:

1. F345 - App-less roster-save normalization and validation policy.
2. F346 - Roster row-transfer application workflow (source removal/read/prepare/atomic save).
3. F347 - Class Details save orchestration.
4. F348 - Class Transfer typed apply validation/request.
5. F349 - Testing Classes delete transition baseline parity evidence.
6. F350 - Testing Classes create/update persistence baseline parity.
7. F351 - My Classes assigned-teacher display baseline parity.
8. F352 - Class Import teacher-choice display baseline parity.
9. F353 - Sub Prep roster-output semantic baseline parity.

No other slices were found.

Gate 2 parity candidates are evidence gaps; the sweeps did not establish
functional defects.

### F344 accepted / F345 selected - 2026-10-05

F344 acceptance and verification are recorded above. F344 closes Batch 8.
F345 is selected for an app-less roster-save normalization/validation contract
over existing `RosterSnapshot` plus the Korean-length flag. This explicitly
revisits F171's earlier choice to keep raw-snapshot validation in Platform:
the bounded scope shares a Qt-free policy across use case and widget while
preserving exact Qt normalization, issue order, UI focus/messages, and port
suppression. F345 is selected, not implemented or verified. F346-F353 remain
queued in order. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### F345 accepted / F346 selected - 2026-10-05

F345, committed as
`40625856d5af8f41fbf951f156300d191658591c`, adds a Qt-free roster-save
normalization/validation policy over `RosterSnapshot` and the Korean-length
flag. Invalid snapshots are rejected in Application before the save-port or
session check; valid snapshots with a closed session still receive the existing
Platform NotFound. Prepared logical text crosses the port, avoiding two Qt
decodes. The policy preserves exact Qt normalization, issue order, UI
focus/messages, and port suppression. Qt 6.12 behavior parity was differentially
checked for Unicode whitespace/case, malformed UTF-16, structural/cell limits,
duplicate pairs, Korean flags, and BOM storage.

Fresh VS2026 x64/Ninja Debug configure audited 1,336 handwritten sources;
three focused targets built and CTest passed 3/3. The standalone Qt-free
policy compile passed and `git diff --check` was clean. No full suite ran.

F346 is selected for the roster row-transfer application workflow (source
removal, read, preparation, and atomic save). F346 is selected, not implemented
or verified. F347-F353 remain queued in order. Batch 8 is complete; Batch 9 is
active. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F346 accepted / F347 selected - 2026-10-05

F346, committed as
`5435cd0a1beb0faebde9e7ad74e19c6555ecc6a7`, adds a typed roster row-transfer
use case. It checks canonical/distinct IDs and source-row removal before a
fresh target read, reproduces the 25-row RosterModel projection (base/custom
columns, cell/header normalization, width mapping including Autumn-to-Fall and
raw-header custom widths, and BOM decoding), prepares insertion, then calls
one typed save port. The Platform adapter invokes `RosterService::saveRosters`
once, preserving batch validation and transaction behavior.

UI validation and confirmation remain in place; live model/autosave/selection
updates occur only after success. Apply-path target-read errors report and
abort instead of attempting a blank-target transfer; menu-time availability
behavior is unchanged. Read/save failure integration cases preserve source
data, model, dirty/autosave/timer/selection state, including rollback on target
write failure.

Independent verification: fresh VS2026 x64/Ninja Debug configure exited 0 and
audited one owner for 1,339 handwritten sources. The focused use-case and
roster-transfer-menu targets built; CTest passed 2/2. Standalone Application
compilation without Qt include/lib paths passed. `git diff --check` and the
new-file trailing-whitespace scan were clean. No full suite ran.

F347 is selected for Class Details save orchestration. F347 is selected, not
implemented or verified. F348-F353 remain queued in order. Batch 8 is complete;
Batch 9 is active. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### F347 accepted / F348 selected - 2026-10-05

F347, committed as
`b4bfcbc4607daeb825a13bad2b849e57139c05fc`, implements Class Details save
orchestration. Independent verification used a fresh source-ownership audit
covering 1,340 handwritten sources, focused CTest 4/4, a Qt-free compile, and
`git diff --check`. Evidence logs are under
`build/p2_f347_independent_*`. These are focused results; no full-suite pass is
claimed.

F348 is selected for Class Transfer typed apply validation/request. F348 is
selected, not implemented or verified. F349-F353 remain queued in order. Batch
8 is complete; Batch 9 is active. Phase 2 remains In Progress/Open; Gates 1 and
2 remain Partial.

### F348 accepted / F349 selected - 2026-10-05

F348, committed as
`1af24ebb1c77087fe29a55217131c3ea9a2e43a0`, implements Class Transfer typed
apply validation/request. Independent verification used a fresh ownership
audit of 1,342 handwritten sources, a 320-step focused build, CTest 2/2, a
standalone Qt-free compile, and clean diff/new-header whitespace checks.
Evidence logs are under `build/p2_f348_independent_verify3_*`. These are
focused results; no full-suite pass is claimed.

F349 is selected for Testing Classes delete transition baseline parity
evidence. F349 is selected, not implemented or verified. F350-F353 remain
queued in order. Batch 8 is complete; Batch 9 is active. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### F349 accepted / F350 selected - 2026-10-05

F349, committed as
`5c698aa71dc25d20d978ce0dc3ad16d6644b3141`, records Testing Classes
clean-delete parity. Current and pinned-baseline paths matched all eight
fields. A fresh current-source ownership audit covered 1,343 handwritten
sources; independent fresh build and focused parity, repository, Application,
and Platform checks passed.

The independent review also surfaced the pre-existing F147 page-target issue:
cancel/failure slots show an extra `Load Teachers` warning because
`ScheduleWidgetTestSupport` omits `loadTestingTeacherChoiceRecords`. This is
outside F349; its target and source remain unchanged. Retain this issue as a
follow-up.

F350 is selected for Testing Classes create/update persistence baseline parity.
F350 is selected, not implemented or verified. F351-F353 remain queued in
order. Batch 8 is complete; Batch 9 is active. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### F350 accepted / F351 selected - 2026-10-05

F350, committed as
`d167a541bb934c4cfce6115942661dbc73ebc4a6`, records Testing Classes
create/update persistence baseline parity. Three current and pinned-baseline
semantic transcripts matched exactly. The fresh owner audit covered 1,344
handwritten sources. The baseline overlay was restricted to identical
test/registration changes plus three minimum Qt substitutions.

The independent fresh build and focused semantic parity, repository,
Application, and Platform checks passed. The current focused CTest passed 6/8;
F145 and F146 timed out after 30 seconds because of the same teacher-query
support gap noted in the F147 page-target follow-up, not an F350 regression.
Cancel/failure slots have an extra `Load Teachers` warning because
`ScheduleWidgetTestSupport` omits `loadTestingTeacherChoiceRecords`. This
remains a follow-up; F349/F350 source and targets were unchanged.

F351 is selected for My Classes assigned-teacher display baseline parity.
F351 is selected, not implemented or verified. F352-F353 remain queued in
order. Batch 8 is complete; Batch 9 is active. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### F351 accepted / F352 selected - 2026-10-05

F351, committed as
`1ecc5b09344099b6bbfb9ee148ce76ec93bfd55a`, records My Classes assigned-teacher
display baseline parity. Three current and pinned-baseline semantic transcripts,
including Unicode profile fields, parsed and matched. The current owner audit
covered 1,345 handwritten sources. Current focused CTest passed 4/4 and
baseline parity passed 1/1. Baseline overlay verification covered 969 files;
test/registration changes were identical, with only three minimum Qt
substitutions.

The existing F145/F146 teacher-query setup gap remains a follow-up; its details
are recorded in the F350 entry above. These are focused results; no full-suite
pass is claimed.

F352 is selected for Class Import teacher-choice display baseline parity. F352
is selected, not implemented or verified. F353 remains queued. Batch 8 is
complete; Batch 9 is active. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### F352 accepted / F353 selected - 2026-10-05

F352, committed as
`96a59df84b2a12b6f1d2c197e183c09b56485511`, records Class Import teacher-choice
display baseline parity. Three scenarios and four data executions produced
ASCII JSON transcripts that parsed and matched. The current owner audit covered
1,346 handwritten sources. Current focused CTest passed 4/4 and baseline parity
passed 1/1. Baseline overlay verification covered 969 files, with identical
test/registration changes and three minimum Qt substitutions.

The F145/F146 teacher-query setup gap remains a follow-up; its details are
recorded in the F350 entry above. These are focused results; no full-suite pass
is claimed.

F353 is selected for Sub Prep roster-output semantic baseline parity. F353 is
selected, not implemented or verified. Batch 9 has no remaining queued slice.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F353 accepted / Batch 10 recorded; F354 selected - 2026-10-05

F353, committed as
`f2bd4e710d23e5a6b4354626f42adbcbe4c9e340`, adds the parity test and CMake
registration. Fresh VS2026 x64/Ninja Debug configure audited 1,347 handwritten
sources. Focused current CTest passed 5/5 across parity, package, renderer,
Application query, and Platform port targets; pinned-baseline parity CTest
passed 1/1. Three ASCII JSON transcripts parsed and matched; the LF transcript
SHA-256 is `78f34cf4b268841226673db6ebd151b3a8f3e4c0bcefef61f5c0c23c64df9ba1`.
The 969-file baseline overlay audit passed with only identical source/registration
changes and three minimum Qt substitutions. Focused evidence only; no full
suite ran.

Batch 10 candidates are recorded in this order:

1. F354 - Class Transfer package-build Application contract; distinct from F348 apply request and F330 repository read batching; preserve package fields/order/read failures, with no assumed selection-size bound.
2. F355 - Co-teacher assignment purpose-fit persistence boundary; avoid hydrating/rewriting full ClassInfo for teacherId while preserving validation/conflict and unrelated fields.
3. F356 - Remove Speaking Evaluation compatibility-service availability gates around existing typed roster-name/selected-subtitle reads; preserve closed-session/read-failure behavior.
4. F357 - Remove Roster Print compatibility gates around existing typed class/teacher/subtitle/extra-column reads; preserve session/errors and keep distinct from F302-F305 batching.
5. F358 - Remove orphan My Classes single-class information read contract/adapter/tests (no production callers); retain the F291 batch path.
6. F359 - Retire the unused Schedule Import compatibility helper and stale DataService include; retain the shared review-request type.
7. F360 - Testing Classes cancel/failure page parity; add the missing `loadTestingTeacherChoiceRecords` test-support read and compare current/baseline warning/page outcomes for F145-F147.
8. F361 - Class Analytics full-page baseline parity for summary/class-shape/YTD visible mappings; keep accepted query work unchanged.

No other slices were found.

F354 is selected as the next slice. Preserve package fields/order/read failures
and assume no selection-size bound. Batch 9 is complete; Batch 10 is active
with F355-F361 queued. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### F354 accepted / F355 selected - 2026-10-06

F354, committed as
`2ad7f7b7db9a65c240f90e3d4c0efc8670fda7d0` (`Phase2 - Add Class Transfer
package-build Application contract (F354)`), adds the Qt-free package-build
Application contract while preserving the full export payload and order,
staged error behavior, and unbounded selections. Fresh current configure and
ownership audit covered 1,352 handwritten sources. Focused current CTest passed
4/4; pinned-baseline parity passed 1/1. The independent tester repeated both.
Nine ASCII JSON transcripts matched byte-for-byte after timestamp-only
normalization; SHA-256:
`9f27c2f9af4a262a77f8ebe95b9f3e76b38d715742d059e061d0b278dfe6896e`.
Thirteen source hashes were frozen. The unavailable warning preserves the
corrected text `No Teacher Profile service is available.` UI flow was
static-checked, not controller-interaction tested. Focused evidence only; no
full suite ran.

F355 is selected for a Co-teacher assignment purpose-fit persistence boundary:
avoid hydrating and rewriting full ClassInfo just to change teacherId while
preserving validation, conflict handling, and unrelated fields. F355 is
selected, not implemented or verified. Batch 9 is complete; Batch 10 remains
active with F356-F361 queued. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### F355 accepted / F356 selected - 2026-10-06

F355, committed as
`0f93cbe661908999603ffd5c45b9bdcccaad1bb3` (`Phase2 - Add purpose-fit
co-teacher assignment persistence (F355)`), uses a purpose-fit validation
snapshot and prepared `teacher_id`-only upsert. SQL `NULL` unassigns, and a
missing `class_info` row receives compatible defaults. Validation, conflict,
and failure behavior remain; unrelated metadata and schedules are preserved,
both schedule-table write-audit triggers remain silent, and full-ClassInfo
read count does not increase. Fresh current ownership audit covered 1,353
handwritten sources; seven focused CTests passed. Baseline parity passed 1/1
with only the parity test and CMake registration overlaid. Five transcript
rows matched; LF-normalized SHA-256:
`336FDC58CD6D92C784A7EC60873D5E2861C41AD3D7EE5EB5F2027189FC605784`.
Focused evidence only; no full suite ran.

F356 is selected to remove Speaking Evaluation compatibility-service
availability gates around existing typed roster-name and selected-subtitle
reads while preserving closed-session and read-failure behavior. F356 is
selected, not implemented or verified. F357-F361 remain queued. Batch 9 is
complete; Batch 10 is active. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### F356 accepted / F357 selected - 2026-10-06

F356, committed as
`6378c369ac8738741ad64581c02e9cdc6f688caf` (`Phase2 - Remove Speaking
Evaluation compatibility read gates (F356)`), removes redundant
`rosterService()` and `classService()` availability gates around Speaking
Evaluation's typed roster-name and selected-subtitle reads. It retains the
`m_services`, class-ID, and page-model guards, query contracts, closed-session
and read-failure behavior, and signature reads; a direct closed-session roster
NotFound test was added.

Fresh current and pinned-F355-baseline builds passed the same 8/8 focused
CTests. Both ownership audits covered 1,353 handwritten sources. The pinned
baseline was `c3f9f314`; its overlay added only the roster test source, with no
production overlays. No full suite ran.

F357 is selected to remove Roster Print compatibility gates around existing
typed class/teacher/subtitle/extra-column reads. Preserve session and errors,
and keep this distinct from F302-F305 batching. F357 is selected, not
implemented or verified. Batch 9 is complete; Batch 10 is active with F358-F361
queued. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F357 accepted / F358 selected - 2026-10-06

F357, committed as
`10f7060a4bc65757063962abc72e84da78794314` (`Phase2 - Remove Roster Print
compatibility gates (F357)`), replaces compatibility `FeatureService::isAvailable()`
gates with direct `ApplicationServices::hasOpenDatabase()` checks at the two
UI edges. Typed class-list, subtitle, and roster-column queries and F302-F304
batching remain; closed-session behavior stays quiet. Two closed-session UI
tests cover the empty/silent class list and preservation of extra-column
controls and selections without a prompt.

Independent fresh current and pinned-F356-baseline builds passed all seven
focused CTests; both ownership audits covered 1,353 handwritten sources. The
baseline used only a test-source overlay for the new closed-session cases, with
no production overlay. No full suite ran.

F358 is selected to retire the orphan My Classes single-class information read
contract, adapter, and tests, which have no production callers; retain the F291
batch path. F358 is selected, not implemented or verified. Batch 9 is complete;
Batch 10 is active with F359-F361 queued. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### F358 accepted / F359 selected - 2026-10-06

F358, committed as
`6bca33113e0e37c568dee2f3a66f85634052ab6a` (`Phase2 - Retire unused My Classes
single-read contract (F358)`), removes the orphan single-class read query and
port, Platform adapter, two tests, and registrations. Shared schedule/fields
DTOs, generic `loadClassInfo` and its read metric, the
`ClassInfoRepository` read record and method, and all F291 batch APIs and tests
remain.

Independent fresh current and pinned-F357-baseline builds each passed the same
three focused CTests for the My Classes page and F291 Application/Platform
batch. One-owner audits covered 1,348 current and 1,353 baseline handwritten
sources, matching the five deletions. No full suite ran.

F359 is selected to retire the unused Schedule Import compatibility helper and
stale DataService include while retaining the shared review-request type.
F359 is selected, not implemented or verified. Batch 9 is complete; Batch 10
is active with F360-F361 queued. Phase 2 remains In Progress/Open; Gates 1 and
2 remain Partial.

### F359 accepted / F360 selected - 2026-10-06

F359, committed as
`25f5520ed861ba8453b633c33cf736b0449a81df` (`Phase2 - Retire Schedule Import
compatibility helper (F359)`), removes unused `openScheduleImportService` and
its helper-only `.cpp`/CMake entries, plus stale production DataService and
presentation includes. `ScheduleImportReviewRequest` remains byte-for-byte;
the test-fixture DataService include remains.

Fresh current and pinned-baseline configure and dialog-target builds succeeded.
The focused current CTest timed out at 300.49 seconds (exit `0xc0000409`) in
`compactFlowAndReviewPresentation`; the exact F358-baseline CTest was stopped
at about 320 seconds. Independent direct-slot runs on both revisions passed
`reviewModelBuildsTypedApplyRequest` and
`reviewPrepareClosedSessionUsesSnapshotWarning`; the slots
`reviewRefreshUsesFreshTypedStateSnapshot`,
`suppliedWorkbookBuildsStagedReview`, and
`compactFlowAndReviewPresentation` stalled at the same database-not-open /
offscreen warning point. This is a baseline-equivalent focused-test limitation;
the dialog target built, but its full CTest did not pass. No full suite ran.

F360 is selected for Testing Classes cancel/failure page parity: add the missing
`loadTestingTeacherChoiceRecords` test-support read and compare current/baseline
warning and page outcomes for F145-F147. F360 is selected, not implemented or
verified; F361 remains queued. Batch 9 is complete; Batch 10 is active. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### F360 accepted / F361 selected - 2026-10-06

F360, committed as
`94431d8120bef45463e395e5712a32bd14afd46c` (`Phase2 - Add Testing Classes
cancel/failure parity (F360)`), adds the typed
`loadTestingTeacherChoiceRecords()` test-support read using existing teacher 7/8
fixtures. Its parity harness covers F145 update failure, F146 create
failure/pending slot, and F147 cancel and delete failure, including exact
warnings, draft state, and prompt state.

Independent current and exact-F359-baseline builds passed the same five focused
CTests. Both ownership audits covered 1,348 handwritten sources. Four ASCII
JSON transcripts matched byte-for-byte; LF-normalized SHA-256:
`20B126D8675FB5C0B6184CEA04A79A639B3AE055F8316E2D563E70AFA9954DD9`. The
baseline overlay contained exactly three test-only paths and no production
changes. No full suite ran.

F361 is selected for Class Analytics full-page baseline parity across summary,
class-shape, and YTD visible mappings; keep accepted query work unchanged. F361
is selected, not implemented or verified. Batch 9 is complete; Batch 10 is
active. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F361 accepted / Batch 11 recorded; F362 selected - 2026-10-06

F361, committed as
`c1ebeb8f193a4b66067fc5129f331ad634c95e3e` (`Phase2 - Add Class Analytics
page mapping parity (F361)`), adds only
`tests/class_analytics_page_read_parity_tests.cpp` and its registration in
`cmake/tests/pages_and_output.cmake`.

Fresh Windows x64 Ninja Debug builds with MSVC 19.51 x64 and Qt 6.12.0 passed
for current and exact F360 baseline
`ef2ef6fcd1986d8dbbdcedf663419e5fc6e9aa33`. The focused
`ClassMngrClassAnalyticsPageReadParityTests` CTest passed 1/1 on each revision;
QtTest reported 4 passed, 0 failed, and 0 skipped on each. The baseline overlay
contained exactly the two F361 test/CMake paths, byte-identical to current, with
no production changes. Ownership counted 1,348 handwritten sources at the
predecessor; CMake reported 1,349 sources for both current and overlaid
baseline. `git diff --check` passed.

`F361_TRANSCRIPT` matched after CRLF-to-LF normalization; SHA-256:
`6590C38A885FF3551D9123FBEA3CCD4E501A893630D37EC3F26CFC7CDBE4DA95`. Verified
mappings include All summary A · 3.5, 2 / 3, and Summer shape A:1/B+:1; Winter
summary B+ · 3.0 and Winter shape A:1/B:1; both retain YTD Winter B+/3.0,
Speech Contest A/4.0, and Summer A/3.5. Empty state covers the hidden empty
label and charts. Qt emitted offscreen resource/font and
`propagateSizeHints` warnings while assertions passed. Baseline archive revision
was `unknown` and unused by the test. No full suite ran.

F362 is selected for the Sidebar class deletion Application boundary, preserving
the pre-confirm read warning, confirmation and page-leave guard order, cascade,
and rollback. It is selected, not implemented or verified. Two independent
read-only gate scans identified at least eleven candidates without establishing
that discovery is exhausted. The lower-ranked Schedule testing-assignment gate
remains for later discovery.
Batch 10 is complete; Batch 11 is active. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### F362 accepted / F363 selected - 2026-10-06

F362, committed as
`88f02d36099b0be4705d4dd105d46e1744319a79` (`Phase2 - Migrate sidebar
class deletion boundary (F362)`), moves regular Sidebar class deletion to the
typed `ClassDeleteRequest`, Application use case/port, and
`ApplicationServicesClassDeletePort`. The Platform port uses the active open
session's `ClassRepository` directly, without a ClassService/DataService
fallback. The controller retains its chooser, warning, confirmation,
leave-guard, and refresh/navigation order; deletion-only chooser and
confirmation-label reads no longer depend on the compatibility-service
availability gate.

Current focused CTests passed 4/4:
`ClassMngrNextApplicationClassDeleteUseCaseTests`,
`ClassMngrNextPlatformApplicationServicesClassDeletePortTests`,
`ClassMngrSidebarClassDeleteParityTests`, and
`ClassMngrNavigationTeacherReadTests`. The parity scenarios cover confirmation
cancel, leave-guard cancel, write failure, success/navigation; the existing
Navigation target covers reload failure and a missing selected class. Current
targets used an existing incremental Debug/Ninja tree and reported no work.

The exact F361 baseline
`c1ebeb8f193a4b66067fc5129f331ad634c95e3e` used a fresh separate Debug/Ninja
tree; its two focused CTests passed 2/2. The baseline overlay contained only
`tests/sidebar_class_delete_parity_tests.cpp` and
`cmake/tests/pages_and_output.cmake`, both byte-identical to current; no F362
production or Application/Platform test paths were overlaid. Ownership audit
counted 1,355 current vs. 1,350 baseline-overlay source owners. Four ASCII
`F362_TRANSCRIPT` records matched after LF normalization; SHA-256:
`72F163E0E49F945B5A1A5B5E69EDE7334FDF6C10AD293C694B43394DC777BB69`. `git
diff --check` passed; no full suite ran.

F363 is selected for the Sidebar regular-teacher deletion Application boundary.
Preserve chooser, profile-read, warning, confirmation, write, and sidebar-refresh
behavior. Do not add a leave guard, page clearing, or navigation; these differ
from baseline and require a separate product decision. F363 is selected, not
implemented or verified. Batch 10 is complete; Batch 11 remains active. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### F363 accepted / F364 selected - 2026-10-06

F363, committed as
`93f2c2f7be3ddaa20414fbc78679d3202fdf1cd6` (`Phase2 - Migrate sidebar
teacher deletion boundary (F363)`), completes the Sidebar regular-teacher
deletion Application boundary. Preserve the chooser, profile-read, warning,
confirmation, write, and sidebar-refresh behavior; do not add a leave guard,
page clearing, or navigation, which differ from baseline and require a separate
product decision.

Five current focused CTests passed; the pinned F362 baseline passed 2/2. The
baseline used an exact two-file overlay. The normalized `F363_TRANSCRIPT`
matched; SHA-256:
`65E80007F19AA29BB5C58C375587D2E6C9559B8AF94DE9ABF98788B809A10891`. Diff and
ownership audits passed.

F364 is selected for the Clear Testing Layout command transition. Use a
dedicated stateless typed Application use case/port; the active-session
Platform adapter calls
`TestingBlockRepository::clearTestingAssignments()` directly, without a
ScheduleService/DataService fallback. Preserve the current availability
warning before confirmation; cancel performs no write or refresh; write failure
warns, skips refresh, and retains rows atomically; success clears all
`schedule_testing_blocks` while preserving saved testing classes/rosters and
unrelated class data, then refreshes Schedule/Workspace views. Avoid new UX.
F364 is selected, not implemented or verified. Batch 11 remains active with
F365-F371 queued. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F364 accepted / F365 selected - 2026-10-06

F364, committed as
`f3afb54f98808bf84c20cf29593e0b4a7c795529` (`Phase2 - Migrate clear testing
layout command boundary (F364)`), implements the stateless typed Application
clear-layout use case/port and direct active-session Platform call to
`TestingBlockRepository::clearTestingAssignments()`. It preserves the warning,
cancel, write-failure, atomic-retention, success-clearing, data-preservation, and
Schedule/Workspace refresh behavior recorded in the preceding selection entry.

Current focused CTests passed 7/8; pinned F363 baseline tests passed 5/6. All
F364 targets plus repository, Workspace, menu, and navigation targets passed.
`ClassMngrScheduleWidgetTests` reported the same 24 passed / 16 failed with
identical failure lines and locations on current and baseline; this was
confirmed pre-existing. Four LF-normalized parity transcript rows were
identical; SHA-256:
`A9406D44B9D9EB2B8C2C64E18B2F05E155CA9A36EEFCCFA7BD6CC1791CE25DDB`. The exact
F363 baseline source
`93f2c2f76d4077377019a5d07564ad0b060e7b84` used a two-file overlay containing
only the parity source and `cmake/tests/pages_and_output.cmake`. One-owner, diff,
and whitespace audits passed.

F365 is selected for Initial Setup Wizard class create/save. It is selected,
not implemented or verified. Batch 11 remains active with F366-F371 queued.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F365 accepted / F366 selected - 2026-10-06

F365, committed as
`2797e8bc687b7e5f3e6f4db7722f01723cf4e352` (`Phase2 - Migrate Initial Setup
Wizard class create/save boundary (F365)`), completes the Initial Setup Wizard
class create/save Application boundary.

Fresh current and pinned F364 baseline builds reported 355/355 and 339/339,
respectively. Current focused CTest passed 10/10; pinned F364 baseline passed
6/6. The parity harness ran five QtTest cases on each. Three LF-normalized
`F365_TRANSCRIPT` rows matched byte-for-byte; SHA-256:
`355A013E4C8326327DD699BBD3CF395D1C5E4CEDD252EBD71321BDC97E9C6F06`. The exact
two-file baseline overlay and owner/diff/whitespace audits passed. Identical
environment QWARNs were non-fatal.

F366 is selected for Initial Setup Wizard validated teacher create. It is
selected, not implemented or verified. Batch 11 remains active with F367-F371
queued. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F366 accepted / F367 selected - 2026-10-06

F366, committed as
`cb8bd18813a2e2676f81782755e3de60757c2c5f` (`Phase2 - Migrate Initial Setup
Wizard validated teacher create boundary (F366)`), completes the validated
teacher-create Application boundary.

Fresh current Ninja Debug build completed 347 steps and focused CTest passed
8/8. The exact pinned F365 source baseline
`2797e8bc687b7e5f3e6f4db7722f01723cf4e352` used a fresh build of 339 steps and
overlay CTest passed 6/6. Each parity run reported six QtTest cases. Four
LF-normalized transcript rows matched byte-for-byte; SHA-256:
`FF2D291196B7249F655AA780EBF2FEACF34EC1634F3B65AA206624FFBA7BCEC6`.

The exact two-file baseline overlay contained
`tests/initial_setup_wizard_teacher_create_parity_tests.cpp` (SHA-256
`4BBE12F30399BE96EF18FDB6C754242B96F5A397653D37284DAF9D12D656E215`) and
`cmake/tests/pages_and_output.cmake` (SHA-256
`95F9EC070532ADDCED37631640340DB49C4F727DA41E116A9256AD5EB5EB5F82`). Ownership
audit found one explicit owner for 1,379 handwritten source files; diff and
whitespace audits passed with only LF-to-CRLF notices. The same Qt font warning
appeared in both environments and was non-fatal.

F367 is selected for the Class Transfer persisted apply boundary; F348 request
validation was already accepted. F367 is selected, not implemented or verified.
Batch 11 remains active with F368-F371 queued. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### F367 accepted / F368 selected - 2026-10-07

F367, committed as
`11f0ce622aa6f18e2d243a32fe0594776a357966` (`Phase2 - Migrate Class Transfer
typed apply boundary`), completes the Class Transfer persisted apply boundary.
F348 request validation was already accepted.

Independent current Ninja/MSVC 19.51/Qt 6.12 verification passed CTest 6/6;
the pinned F366 baseline passed 4/4. The same seven selected legacy transfer
cases ran on both, with QtTest reporting 9 passed / 0 failed including
init/cleanup. F367 parity passed 7/7 on each side: five rows were identical
with no byte differences. Normalized SHA-256:
`755A9B7B5B2F4A3D18CAA0B24E6A9C2F3A3F90070E186476C0A2B395E61369D2`. The
baseline overlay contained only `cmake/tests/features.cmake` and
`tests/sidebar_class_transfer_apply_parity_tests.cpp`. Source-owner counts were
1,387 current / 1,380 baseline. `git diff --check` was clean. Both environments
showed matching nonfatal Qt resource/font warnings. No full suite ran.

F368 is selected: “Teacher Import UI apply integration using the existing Next
use case.” Batch 11 remains active with F369-F371 queued. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### F368 accepted / F369 selected - 2026-10-07

F368, committed as
`cb0622efe4933808a7335c5a6739142bbce20de3` (`Phase2 - Migrate Teacher Import
UI apply integration`), is accepted. The Tester report passed focused CTests
10/10 and confirmed byte identity between the current and F367-baseline UI
transcripts. Normalized SHA-256:
`10144DFCCCDC22716AEBBDA4FE00454513EB31EF4CBDB71A448AF07171AD1862`.

F369 is selected in Batch 11 order for “Schedule Editor baseline parity.” Its
discovery found no exact acceptance matrix, so its scope is not fully pinned.
Batch 11 remains active with F370-F371 queued. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### F369 accepted / F370 selected - 2026-10-07

F369, committed as
`a4d4cd98866bdd680b6d68bba30a48792f235533` (`Phase2 - Migrate Schedule Editor
baseline parity`), is accepted. The Tester built the shared overlay at pinned
baseline `cc15eced5a8d60ea119aa94c841d1833c1ec10da`; baseline parity CTest
passed 1/1 and current focused CTests passed 6/6. Six normalized parity rows
matched byte-for-byte. SHA-256:
`419AB7D2777A4B690F9FC0FED408125207228C5362C7801F4B7F62818CD90E58`. The
transient initial-read failure now matches baseline warning/open/no-write
behavior.

F370 is selected in Batch 11 order for “Class Export picker baseline parity.”
Its acceptance matrix is not yet established. Batch 11 remains active with
F371 queued. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F370 accepted / F371 selected - 2026-10-07

F370, committed as
`f0ce74dbe633f58ac40c3d58cd032c432f3090a4` (`Phase2 - Add Class Export picker
baseline parity`), is accepted. The Tester built the shared overlay at pinned
baseline `a5dea57f13ce0c1f5c5f80dcf870c558e8383872`; baseline parity CTest
passed 1/1 and current focused CTests passed 7/7. Eight normalized JSON rows
matched byte-for-byte. SHA-256:
`9F7EC10B625A2AE62D35A61AA8D0801B892A7472DD271F548A7DB30C8CA3A528`.

F371 is selected in Batch 11 order for “Roster Transfer remaining legacy
availability gates.” Discovery is in progress and its acceptance matrix is not
yet established. Batch 11 remains active. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### Historical remote handoff / F372-F380 accepted / F381 selected - 2026-10-07

At the remote-branch handoff, F367-F370 were absent from that remote
branch; their acceptance evidence had not yet been synced into this checkout.
F371 was skipped on that device because work had already begun here; it was
not independently accepted there. At the time, local HEAD was `2614e196`, a
documentation-pruning commit after F366 source commit `cb8bd188`; local
progress recorded F367 as selected and Batch 11 evidence ended at F366. This
describes that handoff only: first-parent history through `bedaf490` now
contains F367-F370 acceptance evidence; this checkout later accepted F371 in
`fdad5c79`.

Two independent discovery passes identified ten viable Batch 12 candidates;
discovery is not exhaustive. The ordered active list is in the Phase 2 plan.
F372, the Schedule Testing assignment availability gate, is accepted in source
commit `95e3a19e930ef8556aead19bbe58cb116398facc` (`Phase2 - Remove Schedule
Testing assignment service gate (F372)`). The executor built
`ClassMngrScheduleWidgetTests`; the independent Tester ran the four guard/action
cases together, with 6 passed and 0 failed including init/cleanup. The full
target reported 28 passed and 13 failed. Those failures are outside F372's
gate/action cases, but no baseline comparison was completed, so they remain
unclassified. `git diff --check` passed.

The selection rationale was:
`schedule_widget.cpp` lines 526-543 checks `ScheduleService::isAvailable()`
before the typed class-choice read and dialog, while accepted actions map to
the typed save request/use case at lines 569-616. The read and save Platform
ports use `DatabaseSession` directly. Existing `schedule_widget_tests.cpp`
covers assignment action mapping, cancel/manage, and choice-read/write
failures, but not the outer service gate.

F373 is selected for Class Notes save-page baseline parity. The save path is in
`class_notes_page.cpp` around line 206; current page-save tests exist, while
existing page parity covers reads and discard only. F373 is accepted in source
commit `f28a17f0a1ab39cb862de5061e3b6382e1d86bbd`. Independent current and exact
F372 source baseline overlay runs each passed 4/4; Tester canonical JSONL
transcript records matched byte-for-byte, SHA-256
`F954D1AF69E1B7A28E5746DA93CB1C322669854C81F0EF200B65A35E10A7ADD5`. The
executor's alternate hash normalization was not reproduced; use the Tester
canonical hash. Exact SQLite error suffix and save-button text were verified
only on Windows/Qt 6.12.

F374, My Classes full visible summary mapping parity, is accepted in source
commit `17cc8e2b88391f2c54fd092ac862470cc3089644`. Acceptance covers ordered
class-card/tab summary values, counts, schedules, notes/filler, empty state, and
class-info failure; teacher-profile projection remains covered by F351. The
executor's manual focused runs passed 4/4 twice. Three transcript records
(1,075 bytes) matched the F373 test-only overlay using the unchanged F373
Runtime.lib; SHA-256: `FF947F798C124D64EDB756ED07F56A2DA09DB76CD1545D28098865E33E2B16FB`.
CMake regeneration stopped after 11m34s without further output, so CTest on the
registered target remained unverified. The resource-pack warning was non-fatal.

F375, ordinary nonrepeat CalendarPage create/edit/delete parity, is accepted in
local commit `8932303d3da7722eeddd6073e8a3c347c31c743a`. It verifies exact
persisted event sets and preservation of an unrelated event; the production
path includes target-local Calendar QML and keyboard SVG resources. CMake
configure/generation and a 1,382-source ownership audit completed. Focused CTest
passed 1/1, and the independent CTest passed 1/1. Current and exact F374-overlay
transcript SHA-256 matched:
`D145C70AFA3EEFE09F36978548CEDAC7885E47AF89D179F8FC23C88342BA138D`. No full
suite ran.

F376, ClassesPage selection/filter baseline parity, is accepted in commit
`1b719113`. It adds only one canonical six-snapshot QtTest slot to the existing
`tests/classes_page_tests.cpp`, with no production or CMake changes. Coverage
includes initial All/E4, Thursday fallback to All, class 44 selected off-filter
with E4 visible, Intensive-to-Testing using regular days, and null serialization
when filter controls are absent. The current focused VS build succeeded; the
selected method passed twice, with QtTest reporting 3 passed including
init/cleanup per run. Current and exact predecessor test-source overlay
transcripts matched; SHA-256: `0789CC333287C22A354C121BC75DB5F231B2861419B3E8F66CF747077AE60F63`. No
separate baseline worktree or full suite ran.

F377, Speaking Evaluation save-page parity, is accepted in commit
`f84fc41aac31b3c5b4a98437d081a74f744a53d3`. It adds the test-only
`SpeakingEvalPageSaveParity` target and source. Manual saves verified visible
and persisted English `After`, unchanged Korean and note sentinel, clean state,
and exactly one information `Saved` notice. CMake configure succeeded in
592.8 seconds followed by 33.9 seconds of generation; the focused VS x64 build
succeeded, CTest passed 1/1, and direct QtTest passed 3/3. Transcript SHA-256:
`658C147D6A3D0DB4C6BD61C2090B864B6BD8B6CA82DE9307B94EB5FADB9B1A1B`; it is
ASCII-safe and contains the Korean UTF-8 hex `eab980ebafbceca780`. The
`git diff --check` result was clean. Existing resource-pack, font, and
keyboard-asset warnings were non-fatal. No historical runtime overlay was run;
predecessor API/fake compatibility was checked statically.

F378, the Schedule Testing-mode suppression integration slice, is accepted in
commit `773efe39b96b75b89e468a24a22d8a04ee862266` as test-only in
`tests/schedule_testing_layout_clear_parity_tests.cpp`; no CMake or production
files changed. The focused VS x64 build succeeded, and independent direct
QtTest passed 3/3. `F378_TRANSCRIPT`:
```json
{"case":"testing_grade_preference","before":{"m1_visible":true,"m2_visible":false,"testing_affects_m1":false,"banner":"m2_m3_hidden_m1_remains"},"after":{"m1_visible":false,"m2_visible":false,"testing_affects_m1":true,"preference_persisted":true,"banner":"m1_m2_m3_hidden"}}
```
`git diff --check` was clean. No full suite ran.

F379, Calendar repeat-series page parity, is accepted in commit
`1f82706c12977961b60f7299146df3f2dc58fb8b`. The test-only source is
`tests/calendar_page_event_mutation_parity_tests.cpp`. The focused target build
succeeded; implementation and independent direct QtTest runs each passed 3/3.
`F379_TRANSCRIPT`:
```json
{"calendar_projection_refreshed":true,"earlier_unchanged":true,"ids":[2,3,4],"middle_and_following_updated":true,"old_suffix_dates_empty":true,"series_id":"F379-fixed-repeat-series","suffix_ids_and_series_preserved":true,"unrelated_unchanged":true}
```
No CMake or production changes were made, and no broader suite ran. The
documents-resource warning was non-blocking.

F380, Sub Prep information-sheet output parity, is accepted in commit
`a1855996c43ede5117efd8e000b94e6b81a58eb1`. The focused
`ClassMngrSubPrepPageTests` slot passed 3/3 on both implementation and
independent Tester runs; the build succeeded. Acceptance asserts the page-
selected class/day/regular typed request; a loadable, nonempty `Sub Prep.pdf`
with selected class/teacher facts and no unselected sentinel; package and
selected-class directories; and zero generation warnings. QtPdf text
extraction required `QT_QPA_PLATFORM=offscreen` and
`QT_QPA_FONTDIR=C:\Windows\Fonts`. The roster writer stub reports success
without writing a roster PDF, so roster rendering remains covered lower down.
`git diff --check` was clean.

F381 is selected for the Classes landing route availability gate. It is
selected, not implemented or verified. Phase 2 remains In Progress/Open; Gates
1 and 2 remain Partial.

### Batch 12 complete / F382 accepted / requested stop point - 2026-10-07

F381, the Classes landing route availability gate, is accepted in source commit
`e4df4a40`. Batch 12 is complete. The earlier handoff snapshot above is
historical and has been superseded by the first-parent merge and later F371
acceptance in this checkout.

Two independent discovery reports supported selecting F382, Calendar month-
grid visible projection parity. F382 is accepted in source commit `8eadd8bb`.
Its acceptance scope was bounded to checking actual QML day-cell event-list
content and order against `CalendarEventModel` for seeded in-month dates on a
fixed displayed month, including multi-day coverage and Holiday marking. No
separate legacy grid was found, so no comparison against one is claimed.

F383 Calendar upcoming-events panel parity, F384 Calendar Preferences reset UI
parity, F385 Schedule Editor persisted-save parity, and F386 Staff Directory
navigation parity remain provisional, exploratory candidates—not selected or
accepted. They group Calendar visible behavior and preferences with Schedule
Editor persistence and Staff Directory navigation. At the user's earlier
requested stop point after F382's commit, Batch 13 began with F382; no later
slice had been selected or started at that time. That stop point is superseded
by the later F371 acceptance and F383 selection below. Phase 2 remains
In Progress/Open, and Gates 1 and 2 remain Partial.

### Post-merge reconciliation / F371 accepted here / F383 selected - 2026-10-07

F371, accepted in this checkout as source commit `fdad5c79` (`Phase2 - Replace
Roster navigation availability gate (F371)`), covers the `class_roster` route
gate. It preserves closed-session silence and dirty-page state, routes an
open session to Roster after confirmation, and leaves an invalid ID as a no-op.

Independent current and pinned-`bedaf49059a1da21af27cb9c16ab89b7dd69507` verification each
passed the focused parity CTest 1/1. Three normalized JSONL rows were
byte-identical. SHA-256:
`fddb1ff4e541fd8a34a3e58c374215a72199e79efa68ecdae47f31c79928fc22`.

F371 was skipped on the other device because work had already begun here and
was not independently accepted there; this checkout accepted it in the commit
above. F367-F370 acceptance evidence is now present through first-parent
`bedaf490`. F370 acceptance details remain in the earlier log entry.

At this reconciliation, F383 was selected/current for Calendar upcoming-events
panel parity. Discovery was in progress and its acceptance matrix was not
established; F384-F386 were provisional/unselected. Batch 11 (F362-F371) and
Batch 12 (F372-F381) were complete; Batch 13 was active with F382 accepted
and F383 selected/current. The bounded discovery update below supersedes this
point-in-time note. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### Batch 13 bounded discovery complete / F383 remains selected - 2026-10-07

Batch 13 begins with accepted F382, Calendar month-grid visible projection
parity. F383, Calendar upcoming-events panel parity, remains selected/current;
its acceptance matrix is established below; implementation has not started.

Two independent scans found fewer than ten candidates in the bounded
Calendar and NavigationController parity review. They did not establish
repository-wide discovery exhaustion. The ordered findings and dispositions are:

1. F383 - Calendar upcoming-events panel parity (selected/current).
2. F384 - Calendar Preferences term-default restoration UI parity.
3. F385 - Schedule Editor persisted-save parity; retired as a duplicate of
   accepted F369 and excluded from active tracking.
4. F386 - Staff Directory parity for the uncovered gate that covers a closed
   session before leave confirmation.
5. F387 - Calendar Preferences event-reset confirmation/delete/failure/refresh
   UI parity. This is distinct from F384 term-default restoration.
6. F388 - Class Details/Notes/student-Evaluation route availability matrix.
7. F389 - My Info route navigation gate parity.
8. F390 - Sub Prep route gate parity.

No other slices were found.

Batch 13 remains active; F384 and F386-F390 are provisional/unselected. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### F383 acceptance matrix established / implementation pending - 2026-10-07

F383, Calendar upcoming-events panel parity, remains selected/current. The
matrix verifies exact visible row tuples (stable event ID, date span/text,
time/all-day/unknown mapping, title, event-type badge) and order for:

1. Current Month for the displayed month, including a representative multi-day event and a Holiday event,
   while excluding an outside-month sentinel.
2. Next 30 Days from one captured `QDate::currentDate()`, including today and
   +30 and excluding +31, and one event-type filter toggle.
3. Next 10 Events with more than ten rows, deterministic date/time/title/ID
   ordering, and a ten-row cap.
4. An empty completed range displaying its empty label.

Wait for cache range completion; do not assert transient loading or redesign
failed-load behavior. No query-count assertions or legacy-panel comparison.

Pin pre-slice baseline
`fdad5c797e407241731d113dbd1a451ad37fcfdb`; use the existing
CalendarPageEventMutationParity test source/target with a test-only overlay and
require a byte-identical transcript. This records planned verification, not a
claim that implementation or tests have run. F383 remains selected/current;
Phase 2 remains In Progress/Open and Gates 1 and 2 remain Partial.

### F383 accepted / F384 selected - 2026-10-08

F383, Calendar upcoming-events panel parity, is accepted in source commit
`fe24d66bbc9045f71d8af7f43b092501e831a60f` (`Phase2 - Add Calendar upcoming
panel parity (F383)`). The independent Tester built the current and pinned
F371-source-baseline overlay; focused CTest passed 1/1 on each, and direct
QtTest passed 9/9 on each. The single-file overlay was
`tests/calendar_page_event_mutation_parity_tests.cpp`. Four normalized visible
row transcripts matched byte-for-byte; SHA-256:
`a59dd9089a6191950f70a35728c00eb281e691dec418043313086ef0540118a0`.
The matrix covers Current Month, Next 30 Days boundaries and event-type
filtering, Next 10 Events ordering/cap, and the completed empty-range label.

F384, Calendar Preferences term-default restoration UI parity, is selected.
Two independent discovery passes found that Restore Term Defaults stages the
selected academic year's default schedules in the editors, enables linked
school schedules, and disables the linked Middle School fields; persistence is
a separate Save action. F387's event-reset flow is outside this slice.

The F384 acceptance matrix seeds non-default schedules for both schools,
selects academic year 2026, and clicks `preferencesCalendarRestoreDefaults`.
It checks each school's exact displayed term start dates and week counts:
Elementary (2025-12-29/11, 2026-03-16/19, 2026-07-27/11,
2026-10-12/11) and Middle School (2025-12-29/11, 2026-03-16/19,
2026-07-27/4, 2026-08-24/18). It also checks the link option is on, linked
Middle School Winter/Spring week and Winter/Spring/Summer date fields are
disabled, and restore alone leaves the provider's persisted schedules
unchanged without emitting `calendarPreferencesChanged`. Do not click Save or
exercise event reset. Pin pre-slice baseline
`fe24d66bbc9045f71d8af7f43b092501e831a60f`; use an offscreen widget test with
a test-only overlay and require a byte-identical transcript. This records the
acceptance plan, not completed implementation or verification. F385 remains
retired as a duplicate of F369. Phase 2 remains In Progress/Open; Gates 1 and 2
remain Partial.

### F384 accepted / requested stop point - 2026-10-08

F384, Calendar Preferences term-default restoration UI parity, is accepted in
this slice commit. It adds the offscreen target
`ClassMngrCalendarPreferencesRestoreDefaultsTests` in
`tests/calendar_preferences_restore_defaults_tests.cpp` and its registration
in `cmake/tests/pages_and_output.cmake`; no production files changed.

The independent Tester accepted the exact `fe24d66bbc9045f71d8af7f43b092501e831a60f`
baseline overlay. Configure and focused target builds succeeded on current and
baseline; focused CTest passed 1/1 and direct QtTest passed 3/3 on each. The
overlay contained exactly the new test source and CMake registration. All eight
2026 date/week values, linked-state controls, unchanged provider/persisted
schedules, zero writes, and zero `calendarPreferencesChanged` emissions were
verified. Current and baseline canonical transcripts matched byte-for-byte;
SHA-256: `5a9eff69b7c24c261eef4e24d6d4a59fcc47ee285ac3d5128219039daffbad7c`.
`git diff --check` passed. No full suite ran.

Executor and Tester logs are under
`C:\\Users\\wfelt\\AppData\\Local\\Temp\\F384-VERIFY-1-20261007162332\\`;
the executor transcript is `build/f384_calendar_preferences_qtest_20261008.txt`.
Non-fatal warnings covered the optional documents resource pack, Qt font path,
and offscreen size-hint support. The first executor build lacked the MSVC
include environment; the supported `VsDevCmd.bat` retry passed.

F385 remains retired as a duplicate of F369. F386-F390 remain provisional and
unselected. At the user's request, stop after this acceptance commit without
starting another slice. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### F386 selected/current / acceptance matrix established - 2026-10-08

F386, Staff Directory navigation parity for the closed-session-before-leave-
confirmation gate, is selected/current in Batch 13. Its pre-slice source is
`90e18680ebd3496cc5991dbfb9e823404e1df44a` (F384 accepted). The fixed matrix
covers both `native_english_teachers` and `gs_team`: start with a dirty
`TeacherInfo` page and exact unsaved notes content, close the workspace before
dispatch, then assert no leave confirmation, warning, or other prompt; the
current page, teacher identity, notes, and dirty state remain unchanged; and
the requested Staff Directory page is neither created nor shown. Sidebar
highlight behavior is excluded, and production routing remains unchanged.

The current-source self-check used `build/windows-x64-debug`, configured
with the available MSVC x64 environment and Qt 6.12 using NMake. The Windows
preset was not usable in this runner because its NMake generator rejects the
preset x64 platform; direct configuration in the shared directory succeeded.
The focused `ClassMngrStaffDirectoryClosedSessionNavigationParityTests`
target built, and focused CTest passed 1/1. Direct QtTest emitted two
normalized JSONL rows, one per route; both recorded the unchanged dirty
`TeacherInfo` page and notes, no requested page creation or display, and zero
prompts, leave confirmations, and Qt warnings. The raw current transcript is
`build/windows-x64-debug/f386-current-normalized.jsonl`; SHA-256:
`c99738b62b11916539fd74dd8b523bb053960ac94534e0fcfd11924249d8e349`. No
full suite ran.

Independent verification remains pending. Apply only the new test source and
its registration in `cmake/tests/pages_and_output.cmake` to the pinned source,
run the focused target, and require byte-identical normalized JSONL
transcripts. F386 remains selected/current, pending independent verification
and main acceptance. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

### F386 accepted / F387 selected and matrix established - 2026-10-08

F386, Staff Directory closed-session navigation parity, is accepted in this
source commit. The focused `ClassMngrStaffDirectoryClosedSessionNavigationParityTests`
target passed CTest 1/1 on the current source; direct QtTest passed with one
row for each route. The independent Tester passed both route keys and the
assigned matrix on current and baseline. On pre-slice source
`90e18680ebd3496cc5991dbfb9e823404e1df44a`, the only build-relevant overlay
was the F386 test source and its CMake registration. Current, baseline, and
Executor transcripts were each 718 bytes and byte-identical; SHA-256:
`c99738b62b11916539fd74dd8b523bb053960ac94534e0fcfd11924249d8e349`.
`git diff --check` passed. No production or unrelated files changed, and no
full suite ran. The current-source transcript is
`build/windows-x64-debug/f386-current-normalized.jsonl`.

F387, Calendar Preferences event-reset confirmation, deletion, failure, and
refresh parity, is selected/current. Its fixed matrix is:

1. Closed/unavailable session: seed an event, close the session, and click
   Reset. Assert no confirmation, mutation, success status, or
   `calendarPreferencesChanged` signal; reopen and verify the event remains.
2. Cancel: verify the destructive confirmation presents Reset and Cancel
   choices, then choose Cancel. Assert the event persists with no success status
   or signal.
3. Delete failure after confirmation: close the session while the prompt is open,
   then choose Reset. Assert a warning, verify the event remains after
   reopening, and assert no success status or signal. This exercises the
   apply-time session recheck without production injection.
4. Success and refresh: with an event visible in the Calendar month, confirm
   Reset. Assert the event is deleted, success status appears,
   `calendarPreferencesChanged(true)` emits once, and after asynchronous cache
   completion the visible event disappears. Do not assert request counts; an
   empty event list is the existing reset behavior.

F387 may extend the existing Calendar Preferences restore-defaults test
target. The immediate pre-slice source is the F386 acceptance commit that
contains this F387 matrix. F387 implementation has not started; the current
tree changes remain test-only unless bounded discovery proves otherwise.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F387 accepted - 2026-10-08

F387, Calendar Preferences event-reset confirmation, deletion, failure, and
refresh parity, is accepted on pre-slice source
`b3f9105e7c9160b2d862683d324f9dfd3cffc91d` (F386). The only build-relevant
overlay is `tests/calendar_preferences_restore_defaults_tests.cpp` and its
CMake registration/resources in `cmake/tests/pages_and_output.cmake`; no
production sources changed.

An incremental build of
`ClassMngrCalendarPreferencesRestoreDefaultsTests` and focused CTest passed
1/1 in `build/windows-x64-debug`. The canonical transcript verifies no prompt,
mutation, success status, or signal with a closed session; preserved events
and no success effects on Cancel; the warning and event preservation after the
session closes during confirmation; and successful deletion, one
`calendarPreferencesChanged(true)` emission, success status, and disappearance
from the visible Calendar month after cache completion. Two repeated final
runs produced byte-identical transcript JSON; SHA-256:
`92e809f1c75833bfdc181eb10e1587ae6ef8c3a9e9d0eb030afa2d08d59d9a80`.
`git diff --check` passed. No full suite ran.

F388-F390 remain provisional and unselected. Phase 2 remains In Progress/Open;
Gates 1 and 2 remain Partial.

### F388 selected/current / acceptance matrix established - 2026-10-08

F388, Class Details/Notes/student Evaluation route availability parity, is
selected/current in Batch 13. Its immediate pre-slice source is F387 commit
`32455d85` (`Phase2 - Add Calendar Preferences event-reset parity (F387)`).
The matrix crosses each of the three routes (`class_details`, `class_notes`,
and `speaking_winter` under `student_evaluations`) with an available and an
unavailable database session:

1. With the database closed and a valid seeded class ID, dispatch each route
   from a dirty Teacher Info page. Assert that the current page, teacher
   identity, exact notes content, and dirty state remain unchanged; Classes is
   not created or shown; and no prompt, leave confirmation, or Qt warning is
   emitted.
2. With the database open and the same valid route/class ID, dispatch each
   route from a dirty Teacher Info page and choose Discard. Assert exactly one
   unsaved-changes confirmation, the Classes page is current for the requested
   class, and the requested section is Details, Notes, or Evaluations. For the
   evaluation route, also assert the selected evaluation is Winter. Assert no
   warning or unrelated prompt.

The slice is navigation parity only: no sidebar-highlight assertion, invalid
ID/key case, alternate evaluation name, or production behavior change is in
scope. The evaluation case uses the existing templates resource pack. Use a
focused offscreen Qt test with canonical per-case observations. F388
implementation and verification have not started. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### F388 accepted - 2026-10-08

F388, Class Details/Notes/student Evaluation route availability parity, is
accepted on pre-slice source `32455d85` (F387). It adds
`tests/class_route_availability_parity_tests.cpp` and registers the focused
`ClassMngrClassRouteAvailabilityParityTests` target, including keyboard test
resources and the templates resource-pack dependency. No production sources
changed.

The incremental Windows x64 Debug build succeeded in
`build/windows-x64-debug`; focused CTest passed 1/1. Direct QtTest emitted six
normalized JSONL rows. All three closed-session rows preserved the dirty
Teacher Info page with exact notes, created no Classes page, and recorded zero
prompts, leave confirmations, and Qt warnings. The three open-session rows
each recorded one leave confirmation and routed to the requested class and
Details, Notes, or Evaluations section; the student Evaluation row selected
Winter. The transcript at
`build/windows-x64-debug/f388-class-route-availability.jsonl` has SHA-256
`79850613b548e3a1314e117289bebacf8b39ec8e0305bb629b55ccddf4a312ae`.
`git diff --check` passed. No full suite ran. F389 and F390 remain provisional
and unselected. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### Batch 14 bounded discovery complete / F389 selected - 2026-10-08

F389, My Info route navigation gate parity, is selected/current in Batch 13.
Its immediate pre-slice source is F388 commit `f877acac` (`Phase2 - Add class
route availability parity (F388)`). Two bounded scans reviewed the remaining
NavigationController route dispatch and existing controller tests. They found
three candidates for the following batch, without establishing repository-wide
discovery exhaustion:

1. F391 - Teacher route closed-session leave-confirmation gate parity. Teacher
   lookup failures are covered, but a closed database session with a dirty
   current page is not.
2. F392 - Campus Directory root/section navigation confirmation and destination
   parity. Page behavior is covered, but route dispatch from a dirty page is
   not.
3. F393 - Document Catalog route confirmation and PDF Viewer navigation
   parity. Catalog/resource ports and viewer lifecycle are covered, but the
   NavigationController route is not.

No other slices were found.

F389 covers the reachable My Workspace dispatcher keys `my_workspace`,
`my_info_information`, `my_info_schedule`, and `my_info_calendar`; the stale
`my_info_class_information` branch is not dispatched by the current route
switch and is excluded. The matrix crosses each route with open and closed
database sessions:

1. With the database closed and a dirty Teacher Info page, assert that the page
   and unsaved state remain unchanged; My Workspace is not newly created or
   made current during dispatch; and no prompt, leave confirmation, or Qt
   warning is emitted.
2. With the database open and the same dirty page, choose Discard. Assert one
   unsaved-changes confirmation, My Workspace is current, and the exact tab is
   Details, Schedule, or Calendar (the root route opens Schedule). Assert no
   warning or unrelated prompt.

Sidebar highlight is excluded. F389 implementation and verification status
are recorded below. F390 remains provisional in Batch 13; F391-F393 are
discovered but remain inactive until Batch 13 completes. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### F389 accepted - 2026-10-08

F389, My Info route navigation gate parity, is accepted on immediate pre-slice
source commit `f877acac` (`Phase2 - Add class route availability parity
(F388)`). It adds `tests/my_info_route_navigation_parity_tests.cpp` and its
focused registration/resources in `cmake/tests/pages_and_output.cmake`; no
production sources changed.

The focused Windows x64 Debug build passed in the standard
`build/windows-x64-debug` tree, and focused CTest passed 1/1. Independent direct
QtTest runs exited 0 and each emitted eight JSONL rows. All four reachable My
Workspace route keys (`my_workspace`, `my_info_information`,
`my_info_schedule`, and `my_info_calendar`) were checked with open and closed
database sessions. Closed rows preserved the dirty Teacher Info page and exact
notes, emitted no prompt, leave confirmation, or route warning, and caused no
page creation or activation; the initial My Workspace page already existed
before dispatch. Open rows discarded after exactly one leave confirmation and
selected the requested tab, with the root route selecting Schedule. The
transcript at `build/windows-x64-debug/f389-my-info-route-navigation.jsonl`
has SHA-256
`4d725cd4826c2913dbe97d49b1d10a4173d09c5f9a488a7634119096fe7e8c7d`;
the independent run's hash matches after normalizing Windows CRLF line endings.

The executor build reported MSVC LNK4075 (`/INCREMENTAL` ignored due to
`/FORCE`); the independent rebuild passed without warnings. No build errors
were observed in this focused verification, and no full suite ran. F390 was
selected after the separate build-error repair commit, recorded below.
F391-F393 remain inactive until Batch 13 completes. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### Teacher Profile Edit build errors fixed - 2026-10-08

The user reported seven MSVC errors in
`ClassMngrNextPlatformApplicationServicesTeacherProfileEditPersistencePortTests.vcxproj`.
The repair removes a duplicate integer-backed `TeacherId`, uses the shared
string-backed v2 ID, and validates/converts it at the UI and platform edges
before calling the legacy integer repository. Two existing test sources were
updated to the shared ID contract. The first repair exposed six stale test
compile errors; after those updates, the named target and
`cmake --build --preset windows-x64-debug --parallel 4` passed. Independent
build-only verification repeated both commands with exit code 0 and zero build
error lines. `git diff --check` passed. No test binary or CTest was run;
LNK4006 warnings are excluded per the user's instruction. Commit
`0b1286011783107047b70493b8a69fdaa5d9a792` records the fix. F390 began only
after this commit.

### F390 selected/current / acceptance matrix established - 2026-10-08

F390, Sub Prep route gate parity, is selected/current in Batch 13. Its
immediate pre-slice source is build-error repair commit `0b128601` (`Phase2 -
Fix Teacher Profile Edit typed ID build errors`). The matrix crosses the two
distinct supported controller destinations with open and closed database
sessions: the current sidebar root route and the controller-supported Notes
route.

1. For each route with the database closed, dispatch from dirty Teacher Info
   with exact unsaved notes. Assert the current page pointer, teacher, notes,
   and dirty state remain unchanged; there is no prompt, leave confirmation,
   or Qt warning; and Sub Prep is neither created nor current.
2. For each route with the database open, choose Discard. Assert exactly one
   leave confirmation, Sub Prep is current, and its exact section is selected:
   `sub_prep_important` for the root route and `sub_prep_notes` for Notes. Assert
   no warning or unrelated prompt.

The root case uses the current sidebar-generated `Page` payload for
`sub_prep`. The Notes case uses a synthetic `Page` payload with keys
`sub_prep/sub_prep_notes`; the current sidebar has no Notes child, but the
controller explicitly supports this destination. Comments maps to the same
Notes section and is excluded as a duplicate destination. The controller's
`Root`-type branch is excluded because the current sidebar defines Sub Prep as
a `Page`. Sidebar highlight, cancel behavior, repeated navigation, and the
broader Sub Prep use-case/output-memory contracts are out of scope. Use a
focused offscreen Qt test with canonical per-case observations. F390
acceptance and Batch 14 activation are recorded below.

### F390 accepted - 2026-10-08

F390 is accepted on pre-slice source commit `0b128601` (`Phase2 - Fix Teacher
Profile Edit typed ID build errors`). It adds
`tests/sub_prep_route_gate_parity_tests.cpp` and registers
`ClassMngrSubPrepRouteGateParityTests` with its offscreen resources in
`cmake/tests/pages_and_output.cmake`; no production files changed.

The focused incremental build of `ClassMngrSubPrepRouteGateParityTests` passed
in the standard `build/windows-x64-debug` tree, and focused CTest passed 1/1
with `-C Debug`. Independent direct offscreen QtTest exited 0 and verified all
four JSONL rows. Closed-session sidebar-root and synthetic-Notes rows preserved
the same dirty Teacher Info page, identity, and exact notes, with no Sub Prep
creation, prompt, leave confirmation, or Qt warning. Open-session rows each
discarded after exactly one leave confirmation and selected
`sub_prep_important` or `sub_prep_notes`; no unrelated prompt or warning was
reported. The executor and independent tester produced byte-identical
transcripts at
`build/windows-x64-debug/f390_sub_prep_route_gate_parity.jsonl`, SHA-256
`FF1CCDB4ED143C6B4955254A115EA7DF3AFD000878D5EE6CBAEF4F67F3689179`.
`git diff --check` passed. No full suite ran.

### Batch 14 activated / F391 selected - 2026-10-08

F390 completes Batch 13. Batch 14 is now active with F391, Teacher route
closed-session leave-confirmation gate parity, selected/current. F392 Campus
Directory root/section navigation confirmation and destination parity and
F393 Document Catalog route confirmation and PDF Viewer navigation parity
remain provisional and ordered after F391. The acceptance matrix will be
established from bounded source and test discovery before implementation.
Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F391 selected/current / acceptance matrix established - 2026-10-08

F391, Teacher route closed-session leave-confirmation gate parity, is
selected/current in Batch 14. Its immediate pre-slice source is F390 commit
`fdc360c3` (`Phase2 - Add Sub Prep route gate parity (F390)`). The current
`NodeType::Teacher` handler reads the valid teacher profile before asking the
PageManager to confirm leaving; a closed database makes the profile read fail
and the handler returns without prompting. The route is shared by the
Co-Teachers and Campus Staff Korean Teacher nodes. Native English and GS Team
remain separate Staff Directory routes.

Existing tests cover invalid/missing Teacher IDs, the open-session successful
read/confirmation path, and a closed-session Classes route. They do not
dispatch a valid Teacher route with the database closed and a dirty current
page. The matrix adds this one missing row:

1. Persist a valid target Teacher, load a different Teacher into a dirty
   Teacher Info page, close the database, and dispatch `NodeType::Teacher` for
   the target ID. Assert the same Teacher Info widget remains current, its
   original teacher identity and exact unsaved notes are preserved, and it
   remains dirty. Assert zero unsaved-change confirmations, other prompts,
   service warnings, or Qt warnings. The Teacher Info page already exists in
   this fixture, so page-creation absence is not asserted.

The existing `successfulReadConfirmsBeforeLoadingAndShowingTeacher` case
continues to cover open-session behavior; no duplicate open row is added.
Missing/nonpositive IDs, missing-record behavior, teacher create/edit/import,
Native English, GS Team, sidebar highlighting, and production behavior changes
are excluded. Use a focused test and preserve the existing assertions. F391
implementation is underway. F392-F393 remain provisional in Batch 14. The next
Batch 15 discovery is due when work starts on F392, the second-last slice in
Batch 14. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.

### F391 accepted - 2026-10-08

F391 is accepted on pre-slice commit `fdc360c3` (`Phase2 - Add Sub Prep route
gate parity (F390)`). It adds
`closedSessionValidTeacherIdReturnsBeforeLeaveConfirmation` to
`tests/navigation_teacher_read_tests.cpp`; no production behavior or CMake
registration changed. The case persists distinct selected and requested
Teachers, loads the selected profile into Teacher Info, makes its notes dirty,
closes the database, and dispatches the valid requested Teacher ID. It verifies
the same page, selected identity and displayed fields, exact unsaved notes,
and dirty state remain, with no prompt in any category and no captured Qt
warning. Existing open-session coverage remains in
`successfulReadConfirmsBeforeLoadingAndShowingTeacher`.

The incremental build of `ClassMngrNavigationTeacherReadTests` passed in the
standard `build/windows-x64-debug` tree. Focused CTest passed 1/1 with Debug
configuration; independent direct invocation of the new QtTest case exited 0.
`git diff --check` passed. No full suite ran.

### Batch 14 advanced / F392 selected - 2026-10-08

F391 completes the first position in Batch 14. F392 Campus Directory
root/section navigation confirmation and destination parity is now
selected/current; F393 Document Catalog route confirmation and PDF Viewer
navigation parity remains provisional. F392 is the second-last slice in
Batch 14, so bounded Batch 15 discovery begins as F392 work starts. Phase 2
remains In Progress/Open; Gates 1 and 2 remain Partial.

### F392 selected/current / acceptance matrix established - 2026-10-08

F392, Campus Directory root/section navigation confirmation and destination
parity, is selected/current after F391. The local `NavigationController`
dispatches the Campus root and its five section routes to `handleCampus`; the
handler has no database availability gate. A root route from another page
confirms leaving and opens Information. A child route from another page
confirms before opening the matching section. From Campus Dashboard, a child
route switches sections without leave confirmation, while a root click keeps
the current section and synchronizes the Sidebar. Existing page and Sidebar
tests do not exercise these controller transitions.

The focused matrix is:

1. From a dirty non-Campus page, cancel both the root and a child-section
   route. Assert the same page and dirty state remain, Campus Dashboard is not
   instantiated, and exactly one leave confirmation is recorded for each
   dispatch.
2. From a dirty non-Campus page, discard the root route and each child route.
   Assert Campus Dashboard becomes current, the root selects Information, and
   each child selects its exact section: Information, Directions, Address,
   Housing, or Maps. Assert matching Sidebar selection and exactly one leave
   confirmation per dispatch.
3. With Campus Dashboard already current and dirty, dispatch the root route
   and a different child-section route. Assert the root preserves the current
   section, the child selects its requested section, the dirty state remains,
   and neither dispatch prompts to leave.

Database open/closed cross-products are excluded because this handler does not
gate on workspace session state. Sidebar group-collapse clicks that emit no
navigation, invalid/synthetic section keys, and Campus data persistence are
out of scope. Use the existing standard Windows x64 Debug build tree and a
focused offscreen Qt controller test. Batch 15 candidates are recorded below;
they activate after F392 and F393 complete in order. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### Batch 15 discovered at F392 - 2026-10-08

Because F392 is the second-last slice in Batch 14, bounded discovery for the
next batch ran against the Phase 2 plan/work packages, legacy-application
mapping, progress history, current route dispatch, and related source/tests.
The independent audit compared candidate routes and application boundaries
with already accepted slices, producer reachability, and the stated migration
order. It identified two ordered candidates after F392 and F393:

1. **F394 - Classes landing open-session confirmation parity.** The `classes`
   route has a production Sidebar producer. Existing tests cover a closed-
   session dirty-page return and an open-session clean landing, but not an
   open-session route from a dirty current page that confirms before loading
   and showing Classes.
2. **F395 - Campus Dashboard typed save boundary.** Campus selected-campus
   reads have typed boundaries, while the dirty save still calls
   `CampusJsonRepository::saveCampus`. Scope the next slice to the write path,
   preserving validation, manual/automatic save behavior, failure feedback,
   and dirty-state retention.

Other supplied findings were excluded as aliases/duplicates, route keys with
no production Sidebar producer, behavior without an established contract, or
work already covered by accepted slices. The scan was bounded to the
above Phase 2 documents and corresponding current source/tests; it does not
claim repository-wide discovery exhaustion.
No other slices were found.

### F392 accepted - 2026-10-08

F392 is accepted on pre-slice commit `a7df1fc5` (`Phase2 - Add Teacher route
closed-session gate parity (F391)`). It adds
`tests/campus_route_navigation_parity_tests.cpp` and registers
`ClassMngrCampusRouteNavigationParityTests`, its resource packs, and offscreen
resources in `cmake/tests/pages_and_output.cmake`; no production files
changed. Ten route rows cover root/child cancellation, root plus all five
child destinations after Discard, and root/child navigation from a dirty
Campus Dashboard. Assertions verify confirmation counts, exact sections and
Sidebar selection, canceled or dirty state preservation, prompt absence on
same-page section changes, and empty route-scoped warning capture.

The incremental standard-tree build of
`ClassMngrCampusRouteNavigationParityTests` passed. Focused CTest passed 1/1;
independent direct invocation of `campusRouteLeaveGuardMatrix` exited 0 with
12 passed and 0 failed including QtTest setup and cleanup. CMake diff hygiene
passed; the new source had no trailing whitespace and a final newline. No
full suite ran. CMake regeneration reported missing `WrapVulkanHeaders` and
`Vulkan_INCLUDE_DIR`, but target generation/build and verification succeeded.
The direct output showed no setup warning, and the scoped navigation warning
capture remained empty.

### Batch 14 advanced / F393 selected - 2026-10-08

F392 completes the first position in Batch 14. F393 Document Catalog route
confirmation and PDF Viewer navigation parity is selected/current. Batch 15
was discovered while F392 started and remains provisional until F393 finishes;
its two ordered candidates are F394 Classes landing open-session confirmation
parity and F395 Campus Dashboard typed save boundary. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### F393 selected/current / acceptance matrix established - 2026-10-08

F393 covers the real Document Catalog leaf route through `NavigationController`
into the PDF Viewer. The Sidebar represents folders as expandable roots and
document entries as page leaves keyed by typed catalog IDs. Only document leaves
emit a navigation event; root/folder expansion clicks are outside this slice.
Existing catalog, resource-port, and PageManager tests cover their own layers,
but no runtime test joins a document leaf, leave confirmation, resource
resolution, and viewer navigation.

The focused matrix is:

1. From a dirty current page, cancel a valid document route. Assert the exact
   source page and dirty state remain, the PDF Viewer is not created, one leave
   confirmation is recorded, and any preflight resource lease is released.
2. From the same dirty state, discard and open a bundled document. Assert the
   PDF Viewer becomes current with the requested document loaded and its content
   session reaches Ready; verify the route-selected file and print/export
   capabilities, and confirm the source page has no remaining unsaved changes.
3. From a dirty current page, dispatch a catalog entry whose resource cannot be
   resolved. Resolve the resource before prompting to leave, so failure preserves
   the exact dirty page without prompting or creating the PDF Viewer; assert no
   resource-pack lease remains mounted.

Database-session cross-products are excluded because the document catalog is
owned independently of the workspace database and this handler has no session
availability gate. Use the standard Windows x64 Debug tree and the focused
offscreen controller test; do not run the full suite. Phase 2 remains In
Progress/Open; Gates 1 and 2 remain Partial.

### F393 accepted - 2026-10-08

F393 is accepted on pre-slice commit `41da57c55488248a6ad36907a2b1889fb05c38e9`
(`Phase2 - Add Campus Directory route confirmation parity (F392)`). It moves
document-resource resolution before the leave confirmation so a missing PDF
cannot discard the current page's dirty state. If the user cancels, the
move-only preflight result releases its lease when the route returns. The new
`tests/document_catalog_navigation_parity_tests.cpp` clicks a real localized
Sidebar document leaf and verifies its emitted route payload before dispatching
it through `NavigationController`. Its three cases cover cancellation with
dirty-state preservation and lease release; Discard followed by loading the
requested PDF to Ready with the selected path/reference and print/export
capabilities; and missing-resource failure before prompting, preserving the
dirty page without creating the viewer or retaining a mount. The target is
registered in `cmake/tests/pages_and_output.cmake` with bundled and
missing-resource document packs.

The standard `build/windows-x64-debug` build of
`ClassMngrDocumentCatalogNavigationParityTests` passed with no build errors or
LNK4006 warnings. Focused CTest `DocumentCatalogNavigationParity` passed 1/1;
independent direct invocation of all three QtTest cases exited 0. Controller
warning capture was empty in each case. Qt emitted the existing offscreen
`propagateSizeHints()` and missing-font setup warnings before the scoped route
capture. `git diff --check` and the new source's whitespace/final-newline check
passed. CMake regeneration noted unavailable `WrapVulkanHeaders` and
`Vulkan_INCLUDE_DIR`; generation and target build succeeded. No full suite ran.

### Batch 15 activated / F394 selected - 2026-10-08

F393 completes Batch 14. Batch 15 is now active with F394 Classes landing
open-session confirmation parity selected/current and F395 Campus Dashboard
typed save boundary provisional. F394 is the second-last slice in this
two-slice batch, so Batch 16 discovery begins as F394 work starts.
