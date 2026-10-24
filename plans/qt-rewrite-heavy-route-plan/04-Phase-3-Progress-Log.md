# Phase 3 Progress Log

This log retains implementation, verification, and decision evidence for
Phase 3. The [current slice-batch tracker](Current-Slice-Batches.md) records
live sequencing; routine chronology and closed-batch history are omitted.

## F1318 — Schema-v6 behavior baseline — 2026-10-22

Recorded the current schema, versioning/opening policy, file suffix behavior,
connection lifecycle, and existing test coverage in the [schema baseline](../../docs/qt-rewrite/phase-3-schema-baseline.md).
Corrected the Phase 0 historical note about legacy migration: existing
schema-v0–v5 files are rejected, and the locked startup test does not establish
that the SQLite lock caused rejection.

The baseline was source-derived; no runtime database metadata inspection or
new tests were run. An independent source-reference review accepted the
corrected Phase 0 and startup-coverage passages. The docs diff passed
`git diff --check`. Commit: `991c046e`.

## F1319 — WorkspaceStore lifecycle seam — 2026-10-22

Renamed the single connection and repository owner to `WorkspaceStore` and
retained `DatabaseSession` as a compatibility alias/include. Each
`ApplicationServices` owns one store; `workspaceStore()` and
`databaseSession()` return the same owner. `DataService` and feature services
remain bound to it. Candidate-open-and-swap, schema-v6 setup, existing-only
opening, save/export behavior, and repository-before-connection teardown
remain unchanged.

In-tree forward declarations now include `workspace_store_fwd.h`. Out-of-tree
code that manually declares `class DatabaseSession;` must include that shared
forward header instead.

The lifecycle test now checks owner identity through open, failed opens, close,
and reopen; one active connection; v6 initialization; and missing-file
existing-only behavior. Executor verification passed:

- `cmake --build build/macos-clang-debug --target ClassMngrDataServiceLifecycleTests`
- `cmake --build build/macos-clang-debug --target ClassMngr`
- `ctest --test-dir build/macos-clang-debug -R '^ClassMngrDataServiceLifecycleTests$' --output-on-failure`

An independent Tester accepted the source change and ran the lifecycle,
workspace-port, and schema-manager CTests: **3/3 passed**. This was verification
in the existing macOS Debug build tree; no full suite or cross-platform build
was run. The independent CTest command was:

```sh
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrDataServiceLifecycleTests|ClassMngrNextPlatformApplicationServicesWorkspacePortTests|ClassMngrDatabaseSchemaManagerTests)$'
```

## F1320 — Calendar display preferences persistence — 2026-10-22

Moved the Calendar display-preferences adapter to
`ApplicationServices::workspaceStore()` and its `SettingsRepository`. The
existing preference keys, false defaults, QVariant boolean coercion,
key-specific read diagnostics, save errors, and atomic two-setting save remain
unchanged. Tests now set up and inspect the repository through `WorkspaceStore`.

The executor built the focused test target and `ClassMngr`; the focused CTest
passed 1/1, and `git diff --check` passed. An independent Tester reviewed the
source and tests and reran the focused CTest: **1/1 passed**. Verification used
the existing macOS Debug build tree; the full suite and cross-platform builds
were not run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests
ctest --test-dir build/macos-clang-debug -R '^ClassMngrNextPlatformApplicationServicesCalendarEventDisplayPreferencesPortTests$' --output-on-failure
cmake --build build/macos-clang-debug --target ClassMngr
```

## F1326 — Selected-class roster window read — 2026-10-22

Added a separate selected-roster window read for rows `[0, 25)`. It keeps all
ordered column names and widths, reports stored rows outside the window, and
does not expand the row vector to a high sparse index. The existing generic
roster read remains a full snapshot, including its 38-row parity case. When
tail rows exist, the editor explains the condition and disables editing,
autosave, manual save, printing, Save As, and source transfer. Row transfer
reads the target through the bounded path and refuses to rewrite a target with
tail rows. F1327 will replace this temporary guard with tail-preserving writes.

Focused coverage checks bounded sparse reads, dynamic column ordering and
widths, the full 38-row read, overflow protection for editor save and both
transfer sides, and the new window port's missing-parent, closed-store, and
repository-error mappings. The executor built the focused targets and
`ClassMngr`; an independent Tester rebuilt the platform port and reran the
focused CTests: **4/4 passed**. `git diff --check` passed. Ninja emitted a
premature-end-of-file warning during builds, recovered, and completed. These
were incremental checks in the existing macOS Debug tree; no full suite or
cross-platform build was run. The editor's total column count remains
unbounded because no compatible editor cap is established; output-specific
limits were not reused as an editor policy.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesRosterReadPortTests ClassMngrNextApplicationRosterRowTransferUseCaseTests ClassMngrRosterEditorWidgetSaveTests ClassMngrRosterTransferMenuTests ClassMngr
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrNextPlatformApplicationServicesRosterReadPortTests|ClassMngrNextApplicationRosterRowTransferUseCaseTests|ClassMngrRosterEditorWidgetSaveTests|ClassMngrRosterTransferMenuTests)$'
```

## F1321 — Teacher-profile persistence-port migration — 2026-10-22

Moved the existing teacher-profile read and edit persistence adapters and
their tests from the `databaseSession()` compatibility accessor to
`ApplicationServices::workspaceStore()` and `WorkspaceStore::teacherRepository()`.
The Phase 2 application contracts and page wiring already provided read/edit
behavior; this slice changes only the persistence owner boundary. ID parsing,
all 14 field mappings, unavailable/missing/error results, and update/reload
behavior are unchanged.

The executor built both focused test targets and `ClassMngr`; the two focused
CTests passed 2/2, and `git diff --check` passed. An independent Tester
reviewed the adapters and reran both focused CTests: **2/2 passed**. Verification
used the existing macOS Debug tree; the full suite and cross-platform builds
were not run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesTeacherProfileEditPersistencePortTests ClassMngrNextPlatformApplicationServicesTeacherProfileReadPortTests
ctest --test-dir build/macos-clang-debug -R '^ClassMngrNextPlatformApplicationServicesTeacherProfile(EditPersistence|Read)PortTests$' --output-on-failure
cmake --build build/macos-clang-debug --target ClassMngr
```

## F1322 — ClassesPage list persistence-port migration — 2026-10-22

Moved the existing ClassesPage list adapter and its test setup from the
`databaseSession()` compatibility accessor to
`ApplicationServices::workspaceStore()` and
`WorkspaceStore::classRepository()`. The Phase 2 query, UI wiring, repository
ordering, visible list behavior, and no-DataService-fallback behavior remain
unchanged. Student counts (F1323) and Class Details (F1324) remain separate.

The executor built the focused platform-test target and `ClassMngr`; the
focused CTest passed 1/1, and `git diff --check` passed. Ninja printed a
premature-end-of-file warning during the `ClassMngr` build, recovered, and
completed successfully. An independent Tester reviewed the diff and reran the
focused CTest: **1/1 passed**. Verification used the existing macOS Debug
tree; the full suite and cross-platform builds were not run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesClassesListReadPortTests
ctest --test-dir build/macos-clang-debug -R '^ClassMngrNextPlatformApplicationServicesClassesListReadPortTests$' --output-on-failure
cmake --build build/macos-clang-debug --target ClassMngr
```

## F1323 — My Classes student-count persistence-port migration — 2026-10-22

Moved the My Classes student-count platform port and its repository test setup
from the `databaseSession()` compatibility accessor to
`ApplicationServices::workspaceStore()` and
`WorkspaceStore::rosterRepository()`. The existing application batch query,
set-based repository read, per-class results, and page behavior that requests
only the selected class remain unchanged. Added focused coverage for a
nonnull-but-closed store, retaining the null-services case.

The executor built the focused port-test target and `ClassMngr`; the focused
CTest passed 1/1, and `git diff --check` passed. Ninja printed a
premature-end-of-file warning during the `ClassMngr` build, recovered, and
completed successfully. An independent Tester reviewed the diff and reran the
focused CTest: **1/1 passed**. Verification used the existing macOS Debug
tree; the full suite and cross-platform builds were not run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests
ctest --test-dir build/macos-clang-debug -R '^ClassMngrNextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests$' --output-on-failure
cmake --build build/macos-clang-debug --target ClassMngr
```

## F1324 — Class Details display persistence-port migration — 2026-10-22

Moved the existing Class Details display-read port and its platform test
setup to `ApplicationServices::workspaceStore()` and its class-info, teacher,
and roster repositories. The application query and UI remain unchanged. The
snapshot still reports class fields, teacher display name, and student count
independently, preserving partial-source results and display fallbacks.

The executor built the focused platform-test target and `ClassMngr`; the
platform port, application query, and visible read-parity CTests passed 3/3,
and `git diff --check` passed. Ninja printed a premature-end-of-file warning
during both builds, recovered, and completed successfully. An independent
Tester reviewed the diff and reran those three CTests: **3/3 passed**.
Verification used the existing macOS Debug tree; the full suite and
cross-platform builds were not run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesClassDetailsPageReadPortTests
ctest --test-dir build/macos-clang-debug -R '^(ClassMngrNextPlatformApplicationServicesClassDetailsPageReadPortTests|ClassMngrNextApplicationClassDetailsPageQueryTests|ClassMngrClassDetailsPageReadParityTests)$' --output-on-failure
cmake --build build/macos-clang-debug --target ClassMngr
```

## F1325 — Class Details save-workflow persistence-port migration — 2026-10-22

Moved the Class Details validation-context, schedule-conflict, and save
adapters and their focused test setup to
`ApplicationServices::workspaceStore()` and its class-info repository. The
application workflow, normalization, conflict policy, transactional save,
hidden teacher/notes/time-filler preservation, optional schedule handling,
wide teacher IDs, and page behavior remain unchanged.

The executor built all three focused port targets and `ClassMngr`; the three
port CTests and `ClassDetailsPageSaveParity` passed 4/4, and
`git diff --check` passed. Ninja printed a premature-end-of-file warning
during the builds, recovered, and completed successfully. An independent
Tester reviewed the adapters and reran the four CTests: **4/4 passed**.
Verification used the existing macOS Debug tree; the full suite and
cross-platform builds were not run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesClassDetailsValidationContextPortTests ClassMngrNextPlatformApplicationServicesClassDetailsScheduleConflictPortTests ClassMngrNextPlatformApplicationServicesClassDetailsSavePortTests
ctest --test-dir build/macos-clang-debug -R '^(ClassMngrNextPlatformApplicationServicesClassDetailsValidationContextPortTests|ClassMngrNextPlatformApplicationServicesClassDetailsScheduleConflictPortTests|ClassMngrNextPlatformApplicationServicesClassDetailsSavePortTests|ClassMngrClassDetailsPageSaveParityTests)$' --output-on-failure
cmake --build build/macos-clang-debug --target ClassMngr
```

## F1327 — Selected-class roster window save — 2026-10-22

Added transactional saves for selected roster windows `[0, 25)`. Sparse tail
rows retain their absolute row indices while tail cell column indices are
remapped by normalized header identity, including `Autumn`→`Fall`. Ambiguous
aliases, malformed indices, and unconfirmed tail-column removals fail before
persisted changes. Visible row removal and transfer leave hidden tail rows at
their existing indices; source and target transfer writes remain atomic. The
editor can save and transfer overflowing rosters again. Generic full reads and
whole-roster replacement APIs remain unchanged.

Focused coverage includes sparse tails, schema reorder and normalization,
authorized and rejected removals, row-24 removal, very sparse row indices,
wide dynamic schemas, editor saves/imports, transfer preservation, and
rollback. The incremental macOS Debug build succeeded; Ninja printed a
premature-end-of-file warning and recovered. The focused CTest selection passed
**16/16**, and `git diff --check` passed. Independent review found no F1327
production defect and reran the focused selection successfully.

The broader `ClassMngrClassTransferTests` selection has one teacher-fallback
assertion failure (`importDialogFallsBackToIndividualTeacherProfilesAfterBatchFailure`,
`class_transfer_tests.cpp:5192`). A clean archive of F1326 commit
`7f10f8f51f924a304a5c1280e1339fa6637bb29c`, built separately, fails the same
isolated assertion with the same actual and expected teacher names, so this is
a confirmed baseline failure and not an F1327 regression. The focused teacher
choice regression selection passes. The separate baseline build emitted a
missing `documents` resource-pack warning; it did not prevent the test from
reproducing the assertion.

```sh
cmake --build build/macos-clang-debug --target ClassMngr ClassMngrNextPlatformRosterWindowSaveTests ClassMngrNextPlatformApplicationServicesRosterSavePortTests ClassMngrNextPlatformApplicationServicesRosterReadPortTests ClassMngrNextApplicationRosterSaveUseCaseTests ClassMngrNextApplicationRosterReadQueryTests ClassMngrNextApplicationRosterRowTransferUseCaseTests ClassMngrNextApplicationRosterRowTransferPreparationTests ClassMngrNextApplicationRosterRowRemovalTests ClassMngrNextApplicationRosterReorderingTests ClassMngrNextApplicationRosterCustomColumnAppendTests ClassMngrNextApplicationRosterCustomColumnRemovalPolicyTests ClassMngrRosterEditorWidgetSaveTests ClassMngrRosterTransferMenuTests ClassMngrRosterEditorWidgetImportTests ClassMngrRosterModelTests ClassMngrClassTransferTests -j 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build/macos-clang-debug --output-on-failure -R '^ClassMngr(NextPlatformRosterWindowSave|NextPlatformApplicationServicesRoster(Save|Read)Port|NextApplicationRoster(SaveUseCase|ReadQuery|RowTransferUseCase|RowTransferPreparation|RowRemoval|RowReordering|CustomColumnAppend|CustomColumnRemovalPolicy)|RosterEditorWidget(Save|Import)|RosterTransferMenu|RosterModel|ClassImportTeacherChoiceRegression)Tests$'
```

Commit: `b705c158`.

## F1328 — Speaking-evaluation read — 2026-10-22

Migrated the selected-evaluation and batch score-import Platform read
adapters, plus their direct test setup, from the transitional
`databaseSession()` accessor to `ApplicationServices::workspaceStore()` and
its existing `SpeakingEvalRepository`. The application query contracts,
detailed repository methods, error mappings, exact-name matching, ordered
rows, 11-cell values, Unicode, and empty-success behavior remain unchanged.

The single-read port is also used as a roster score-import fallback, so this
slice deliberately does not add a 25-row limit for the page display. A future
page-specific bounded read would need a separate explicit contract while
keeping the import read behavior intact.

The focused platform port target built and its CTest passed **1/1** (12 QtTest
cases); the independent Tester reran it successfully. `git diff --check`
passed. Ninja printed a premature-end-of-file warning and recovered. The
existing macOS Debug tree was used; no broad suite or cross-platform build was
run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesSpeakingEvaluationReadPortTests
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^ClassMngrNextPlatformApplicationServicesSpeakingEvaluationReadPortTests$'
```

Commit: `fee2d6db`.

## F1329 — Speaking-evaluation save — 2026-10-22

Migrated the typed Platform save adapter and direct test setup to
`ApplicationServices::workspaceStore()` and its existing
`SpeakingEvalRepository`. The Application contract, detailed transactional
save, validation and normalization, 25×11 matrix, all columns, and dirty-cell
delta semantics remain unchanged. Legacy `SpeakingEvaluationService` and
DataService save paths remain outside this slice.

The focused Platform save-port, Application use-case, page-save parity, and
page-save CTests passed **4/4**. An independent Tester reviewed and reran the
same selection successfully. `git diff --check` passed. Ninja printed a
premature-end-of-file warning and recovered. Verification used the existing
macOS Debug tree; no broad suite or cross-platform build was run.

The null-repository guard maps to recoverable `NotFound`, but the current
fixture cannot create an open `WorkspaceStore` without its repository, so
that branch remains untested directly. Closed/unavailable store, missing
parent, rollback, delta update, Application, and page behavior are covered.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesSpeakingEvaluationSavePortTests ClassMngrNextApplicationSpeakingEvaluationSaveUseCaseTests ClassMngrSpeakingEvalPageSaveParityTests ClassMngrSpeakingEvalPageSaveTests --parallel 4
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrNextPlatformApplicationServicesSpeakingEvaluationSavePortTests|ClassMngrNextApplicationSpeakingEvaluationSaveUseCaseTests|ClassMngrSpeakingEvalPageSaveParityTests|ClassMngrSpeakingEvalPageSaveTests)$'
git diff --check
```

Commit: `e696bbf0`.

## F1330 — Schedule Builder source read — 2026-10-22

Migrated the Schedule Builder source-read Platform adapter and its direct
fixture to `ApplicationServices::workspaceStore()` and the existing
`ClassInfoRepository`. The typed query, repository projection, and builder
remain unchanged. The read includes all non-testing classes and their raw
regular and intensive schedule rows, ordered by class name/ID and schedule
row ID. The repository continues to load its class metadata and two schedule
sets in three batched queries; no per-class reads were added.

The complete projection remains uncapped because Schedule Builder displays all
non-testing classes and has no class-page selection. Mode, visible-day, and
malformed-row handling stay in the builder. Intensive slot-state and testing-
assignment reads remain separate from F1330.

The focused Application snapshot, Platform port, ScheduleBuilder, and
ScheduleWidget CTests passed **4/4**. Independent review confirmed the
three-query metrics and full ordering/projection coverage. `git diff --check`
passed. CMake regenerated stale source globs; Ninja reported an incomplete
build log and recovered. Verification used the existing macOS Debug tree; no
broad suite or cross-platform build was run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextApplicationScheduleBuilderSourceSnapshotTests ClassMngrNextPlatformApplicationServicesScheduleBuilderSourcePortTests ClassMngrScheduleBuilderTests ClassMngrScheduleWidgetTests -j2
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrNextApplicationScheduleBuilderSourceSnapshotTests|ClassMngrNextPlatformApplicationServicesScheduleBuilderSourcePortTests|ClassMngrScheduleBuilderTests|ClassMngrScheduleWidgetTests)$'
git diff --check
```

Commit: `51449b56`.

## F1331 — Intensive slot-state save — 2026-10-22

Migrated the typed slot-state save adapter and its direct test setup to
`ApplicationServices::workspaceStore()` and the existing
`IntensiveSlotStateRepository`. Weekday and minute validation, canonical
weekday and zero-padded `HH:mm` storage, and the global `(day,start_time)` key
remain unchanged. Choosing the supplied default deletes only that slot key;
other states upsert it. The same toggle path continues to support intensive,
weekend, and enabled regular weekday slots. The separate read port, legacy
service/DataService facade, and schedule-import snapshot writer remain outside
this slice.

The focused repository, Application use-case, Platform save-port, and
ScheduleWidget CTests passed **4/4**. Independent review confirmed the
single-statement repository behavior, targeted deletion, failure mapping, and
widget coverage. `git diff --check` passed. Ninja printed a premature-end-of-
file warning and recovered. Verification used the existing macOS Debug tree;
no broad suite or cross-platform build was run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrIntensiveSlotStateRepositoryTests ClassMngrNextApplicationScheduleSlotStateSaveTests ClassMngrNextPlatformApplicationServicesScheduleSlotStateSavePortTests ClassMngrScheduleWidgetTests
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrIntensiveSlotStateRepositoryTests|ClassMngrNextApplicationScheduleSlotStateSaveTests|ClassMngrNextPlatformApplicationServicesScheduleSlotStateSavePortTests|ClassMngrScheduleWidgetTests)$'
git diff --check
```

Commit: `728d90ff`.

## F1332 — Calendar page range read — 2026-10-22

Migrated the active asynchronous range and next-event-date projection queries
to create and open a worker-local `WorkspaceStore` with `openExisting()`, then
read through its `CalendarEventRepository`. The cache still runs each query
on its worker and never shares the UI-owned store. Existing query contracts,
inclusive date overlap, multi-day indexing and de-duplication, ordering,
wall-clock dates/times, stored recurrence rows, and the 4,096-event projection
cap remain unchanged. Missing files are not created; open/schema/query errors
remain Technical, invalid ranges remain InvalidInput, and capacity overflow
remains visible.

The focused Calendar repository and cache CTests passed **2/2** in the
executor's macOS Debug tree and independently in a second existing macOS
Debug tree. Coverage includes both range and next-date missing-file behavior,
schema/open failures, connection cleanup, inclusive overlap, recurrence IDs,
multi-day de-duplication, and projection overflow. `git diff --check` passed.
Ninja reported a premature-end-of-file warning and recovered.

`openExisting()` runs schema and foreign-key integrity checks and constructs
all repository wrappers for each range or next-date query. The latency of
that repeated setup on large workspaces was not benchmarked; a SQL execution
failure after successful open also lacks direct test injection. No broad suite
or cross-platform build was run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrCalendarEventCacheTests ClassMngrCalendarEventRepositoryTests --parallel 8
ctest --test-dir build/macos-clang-debug -R 'ClassMngrCalendarEvent(Cache|Repository)Tests' --output-on-failure
git diff --check
```

Commit: `c5289ae2`.

## F1333 — Single Calendar Event save — 2026-10-22

Migrated the typed single-event save adapter and its direct fixture to
`ApplicationServices::workspaceStore()` and the existing
`CalendarEventRepository`. The adapter still validates and converts the typed
request, saves through `saveCalendarEventWithOutcome()`, and clears the repeat
series ID for an occurrence-only edit. Repository transaction behavior,
create/update handling, structured failure mapping, and retry behavior remain
unchanged. Batch import, series save, and suffix deletion stay on their
separate paths.

The focused Platform save-port, Application calendar-event, CalendarEvent
repository, and Calendar Page mutation-parity CTests passed **4/4**. The
independent review rebuilt the targets and confirmed all **211 test cases**
passed, including create/update ID mapping, missing updates, SQL failures,
rollback behavior, and selected-occurrence detachment with sibling series IDs
preserved. `git diff --check` passed. The existing dialog can convert an
`Unknown` event status to `Timed` when opened and saved; this pre-existing UI
behavior was left unchanged. Verification used the existing macOS Debug tree;
no broad suite or cross-platform build was run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesCalendarEventPortTests ClassMngrNextApplicationCalendarEventTests ClassMngrCalendarEventRepositoryTests ClassMngrCalendarPageEventMutationParityTests
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrNextPlatformApplicationServicesCalendarEventPortTests|ClassMngrNextApplicationCalendarEventTests|ClassMngrCalendarEventRepositoryTests|ClassMngrCalendarPageEventMutationParityTests)$'
git diff --check
```

Commit: `8ef71240`.

## F1334 — Calendar Import batch save — 2026-10-22

Migrated the typed Calendar Import save adapter and its direct import fixtures
from the `DatabaseSession` compatibility accessor to
`ApplicationServices::workspaceStore()`. Request validation, event
normalization and mapping, create-only semantics, repeat-series clearing,
structured errors, and returned-ID validation remain unchanged. The adapter
still makes one ordered `saveCalendarEvents()` call; the repository owns the
single transaction for the batch and returns IDs in input order. An empty
batch remains a successful no-op. Duplicate detection and import counts remain
in the existing use case.

The focused Platform port, Application import use case, end-to-end import
parity, CalendarEvent repository, and typed Application event CTests passed
**5/5**. Independent review confirmed ordered typed IDs, empty and duplicate-
only batches, rejection before writes, and rollback with no partial rows for
the 4,097-event failure case. `git diff --check` passed. The first parity run
could not bind its localhost fixture in the restricted sandbox; the test
passed on rerun with loopback access. The escalation completed without
rejection or a manual approval prompt. Ninja reported a premature-end-of-file
warning and recovered. Verification used the existing macOS Debug tree; no
broad suite or cross-platform build was run. Transaction-start and commit
failure injection remain outside current test coverage.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesCalendarEventPortTests ClassMngrNextApplicationCalendarEventImportUseCaseTests ClassMngrCalendarEventImportParityTests ClassMngrCalendarEventRepositoryTests ClassMngrNextApplicationCalendarEventTests --parallel 4
ctest --test-dir build/macos-clang-debug --output-on-failure -j1 -R '^(ClassMngrNextPlatformApplicationServicesCalendarEventPortTests|ClassMngrNextApplicationCalendarEventImportUseCaseTests|ClassMngrCalendarEventImportParityTests|ClassMngrCalendarEventRepositoryTests|ClassMngrNextApplicationCalendarEventTests)$'
git diff --check
```

Commit: `abc8a278`.

## F1335 — Repeat-series suffix delete — 2026-10-22

Migrated the typed repeat-series suffix-delete adapter and its direct fixtures
from the `DatabaseSession` compatibility accessor to
`ApplicationServices::workspaceStore()`. The adapter still validates the
series ID and selected start date before store access and calls
`deleteCalendarEventsForRepeatSeriesFromDate()` once. The repository continues
to trim the ID and delete the selected date and later occurrences, while
preserving earlier and unrelated events. No-match success, recoverable
unavailable-store handling, SQL failure mapping, and the UI's existing
invalidation behavior remain unchanged.

The focused Platform port, CalendarEvent repository, and typed Application
event CTests passed **3/3**. Independent review confirmed padded-ID matching,
the inclusive suffix boundary, earlier/unrelated-event preservation,
no-match success, invalid input, closed-store handling, and trigger failure
with the target row intact. `git diff --check` passed. The existing parity
tests do not cover suffix deletion at the page level, and repository failure
tests do not inject transaction-start/commit errors or a multi-row failure.
Ninja reported a premature-end-of-file warning and recovered. Verification
used the existing macOS Debug tree; no broad suite or cross-platform build was
run.

```sh
cmake --build build/macos-clang-debug --target ClassMngrNextPlatformApplicationServicesCalendarEventPortTests ClassMngrCalendarEventRepositoryTests ClassMngrNextApplicationCalendarEventTests --parallel 4
ctest --test-dir build/macos-clang-debug --output-on-failure -R 'ClassMngrNextPlatformApplicationServicesCalendarEventPortTests|ClassMngrCalendarEventRepositoryTests|ClassMngrNextApplicationCalendarEventTests'
git diff --check
```

## F1336 — Sub Prep visible schedule summaries — 2026-10-23

Independent review accepted the change. The summary Platform port now uses
`workspaceStore()` and `ClassInfoRepository`. Selected-scope reads retain
roster counts; separate navigation records omit counts and avoid roster
aggregation. Existing teacher projection, error mapping, and fallback behavior
remain unchanged.
Source review confirmed the count-free return precedes roster aggregate SQL;
no SQL instrumentation was added.

The incremental macOS Debug build command passed:

```sh
cmake --build build/macos-clang-debug --target ClassMngr -j 4
```

The focused Platform port, Application schedule-summary query, and Application
class-summary CTests passed **3/3**. `git diff --check` passed. No full suite or
cross-platform build was run.

## F1337 — Selected Sub Prep class details — 2026-10-23

The Platform details adapter now uses `workspaceStore()` and
`ClassInfoRepository`; selected-details behavior and mappings are unchanged.
Direct details fixtures now access the repository through `WorkspaceStore`.

The incremental standard macOS Debug build of `ClassMngr`, the Platform Sub
Prep port suite, and the Application details-query suite passed. Their focused
CTests passed **2/2**; `git diff --check` passed. No full suite or
cross-platform build was run.

## Batch 1 push receipt — 2026-10-23

F1336 (`d0553b4915053e56376c0e761c157b4f0a6ff039`) and F1337
(`9b7ad2b6a2190c0bf1f71c245ba4b6429e24b339`) were pushed to
`origin/Qt-Rewrite` in range `b709aa76..9b7ad2b6`. Verification confirmed
local HEAD and remote `refs/heads/Qt-Rewrite` both at
`9b7ad2b6a2190c0bf1f71c245ba4b6429e24b339`; the worktree was clean.

## F1338 — Sub Prep information-sheet print-source read — 2026-10-23

The production print-source port now uses `ApplicationServices::workspaceStore()`
for store availability and its ClassInfo, Teacher, and Roster repositories.
Direct fixture repository handles use `WorkspaceStore`. Query selection,
projection, ordering and limits; teacher batching; roster-count mapping and
fallback; owning result; error mapping and text; and package lifecycle are
unchanged. No raw SQL was added.

The incremental build and focused CTests passed in the existing macOS Debug
tree; an independent Tester confirmed the result. `git diff --check` passed.
No page suite, full suite, or cross-platform build ran.

```sh
cmake --build build/macos-clang-debug --target ClassMngr ClassMngrNextPlatformApplicationServicesSubPrepPrintSourcePortTests ClassMngrNextApplicationSubPrepPrintSourceQueryTests ClassMngrSubPrepPrintSourceMapperTests
```

The three focused CTest suites passed **3/3**.

## F1339 — Sub Prep roster-output source read — 2026-10-23

The production port in
`src/next/platform/application_services_sub_prep_roster_output_source_port.h`
now uses `workspaceStore()` and obtains/checks the same four repositories
through `WorkspaceStore`. Its direct fixture/repository handles and SQL
fixture database handles in
`tests/next_platform_application_services_sub_prep_roster_output_source_port_tests.cpp`
also use `WorkspaceStore`. Request inputs, repository calls, output/error
behavior, ordering, bounds, and owning result are unchanged. The port has no
`DatabaseSession`/`databaseSession()` reference or SQL. Package-service source
and tests were untouched. Independent review accepted the scoped migration.

Executor and Tester builds of `ClassMngr` succeeded in the existing macOS
Debug tree; Tester also built all four F1339 test targets. The focused port,
query, parity, and class-ID projection CTests passed. `git diff --check`
passed. `ClassMngrSubPrepPackageServiceTests` had one failure:
`largePackageGeneratesOutputReferenceWhenConfigured` reported a generated PDF
page 1 text/reference mismatch (13 passed, 1 failed). Tester reproduced it in
isolation with `g_rosterOutputSourceReadPort` set to a fake, confirming the
case does not call the F1339 adapter. This does not establish whether the
failure is pre-existing or a regression. No full suite or cross-platform
build ran.

## F1340 — Sub Prep calendar-interval read — 2026-10-23

The port in
`src/next/platform/application_services_sub_prep_calendar_event_intervals_port.h`
now uses `workspaceStore()` to check availability and obtain the existing
`CalendarEventRepository`. The injected `IntervalRangeReader`, date parsing
and range checks, repository call, filtering, projection, owning result, and
exact error messages/codes remain unchanged. The port has no SQL or
`DatabaseSession` access. Direct fixtures in
`tests/next_platform_application_services_calendar_event_port_tests.cpp` now
use `WorkspaceStore`; assertions were retained, with focused coverage added
for empty/fresh reads, invalid and reversed request exact errors, trimmed and
unsupported types, and repository-error mapping.

Executor and Tester built `ClassMngr` in the existing macOS Debug tree; Tester
also built `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationSubPrepCalendarEventIntervalsQueryTests`, and
`ClassMngrNextApplicationSubPrepVacationSelectionPolicyTests`. These three
focused CTests passed **3/3**. `git diff --check` passed. Main source review
confirmed `SubPrepPage::generateSubPrep` still logs a read failure and opens
the dialog with an empty calendar selection. No page suite, full suite, or
cross-platform build ran. The separate F1339 package-service PDF reference
failure remains documented above and is not an F1340 result.

## F1341 — Calendar availability and event-by-ID reads — 2026-10-23

Availability and by-ID paths in
`src/next/platform/application_services_calendar_event_port.h` now use
`workspaceStore()`/`WorkspaceStore`. The separate date-range
`projection()` remains on `DatabaseSession`; F1341 did not migrate that path.
By-ID fixtures in
`tests/next_platform_application_services_calendar_event_port_tests.cpp` and
the stale-event fixture delete in
`tests/calendar_page_event_mutation_parity_tests.cpp` now use
`WorkspaceStore`; assertions remain. Coverage includes direct output,
wide/canonical IDs, missing/failure/invalid projection, availability, and
stale-event compatibility. Repository SQL, UI production code, and mutations
were unchanged.

Executor and Tester built `ClassMngr` in the existing macOS Debug tree. Tester
also built `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarPageEventMutationParityTests`; the three focused CTests
passed **3/3**. `git diff --check` passed. Source review confirmed the
availability/by-ID WorkspaceStore boundary and the unchanged range projection.
Stale-event page parity still checks the warning, closed editor, and unchanged
persisted state. No full suite or cross-platform build ran. The F1339
package-service PDF mismatch remains documented in the F1339 entry.

## F1342 — Single Calendar Event delete — 2026-10-23

`src/next/platform/application_services_calendar_event_delete_port.h` now
uses `ApplicationServices::workspaceStore()` to retrieve the
`CalendarEventRepository`. The one repository delete call, canonical positive
ID validation, success when a valid ID has no matching event, recoverable
unavailable-store error and exact message, and repository/exception mappings
remain unchanged.
The four single-delete fixtures in
`tests/next_platform_application_services_calendar_event_port_tests.cpp` and
the scoped sequence SQL handle in
`tests/calendar_page_event_mutation_parity_tests.cpp` now use
`WorkspaceStore`; assertions remain. Shared page helpers and other mutations
were untouched. UI failure still warns and returns before invalidation;
success invalidates calendar data.

Executor and Tester built `ClassMngr` in the existing macOS Debug tree. Tester
also built `ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarPageEventMutationParityTests`. These three focused CTests
passed **3/3** serially. `git diff --check` passed. No full suite or
cross-platform build ran.

## F1343 — Delete all Calendar Events — 2026-10-23

`src/next/platform/application_services_calendar_event_delete_all_port.h`
now uses `workspaceStore()` for availability and repository access. The
repository delete-all call, empty-calendar success/no-op, errors/messages, and
UI behavior remain unchanged. F1343 fixtures in
`tests/next_platform_application_services_calendar_event_port_tests.cpp` and
reset-specific helpers in
`tests/calendar_preferences_restore_defaults_tests.cpp` now use
`WorkspaceStore`. Added `deletesAllCalendarEventsFromEmptyCalendarSuccessfully`;
existing multi-row delete, availability/SQL failure, confirmation, cancel,
failure, no-success-signal, and success-refresh assertions remain. UI
production behavior and repository SQL were unchanged.

Executor built `ClassMngr`; Tester built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarPreferencesRestoreDefaultsTests` in the existing macOS Debug
tree. The three focused CTests passed **3/3** serially. `git diff --check`
passed. No full suite or cross-platform build ran.

## F1344 — Repeat-series creation — 2026-10-23

`src/next/platform/application_services_calendar_event_series_create_port.h`
now uses `workspaceStore()` and obtains the existing
`CalendarEventRepository`. Request validation, series-ID normalization,
occurrence count/order, the single `saveCalendarEvents()` call, ordered typed
IDs, transaction/rollback behavior, and errors remain unchanged. F1344
fixtures in
`tests/next_platform_application_services_calendar_event_port_tests.cpp` now
use `WorkspaceStore` for daily/weekly/monthly parity, invalid requests,
duplicate IDs, unavailable store, and failure/no-partial-rows; assertions
remain. One unavailable-store test was renamed from Session to Store without
behavior or assertion changes. Repository SQL, UI, and the separate F1345 edit
path were unchanged.

Executor built `ClassMngr`; Tester built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarEventRepositoryTests`. The three focused CTests passed
**3/3** serially. `git diff --check` passed. No broader suite or
cross-platform build ran.

## F1345 — Repeat-series edit — 2026-10-23

`src/next/platform/application_services_calendar_event_series_edit_port.h`
now uses `workspaceStore()` for store state and repository access. The
selected-date boundary, trimmed series ID, order, IDs/offsets/edited fields,
empty-suffix no-op, request validation, single batch save/transaction, error
mappings, and exception behavior remain unchanged. Edit-specific fixtures in
`tests/next_platform_application_services_calendar_event_port_tests.cpp` and
the two UI parity suffix-edit fixtures in
`tests/calendar_page_event_mutation_parity_tests.cpp` now use
`WorkspaceStore`; assertions remain. The neighboring occurrence-edit fixture
and all create/delete paths were unchanged.

Executor built `ClassMngr`; Tester built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventTests`, and
`ClassMngrCalendarPageEventMutationParityTests`. Three focused CTests passed
**3/3** serially. Coverage includes ordered IDs and suffix boundaries, empty
suffix, failure/rollback, and retry/cancel/refresh UI parity.
`git diff --check` passed. No full suite or cross-platform build ran.

## F1346 — Calendar Import signature lookup — 2026-10-23

`CalendarEventRepository` now owns
`CalendarEventImportSignatureSourceRecord` and
`visitCalendarEventImportSignatureSourcesInRange()`. The projection selects
only title, event type, start/end dates, all-day, and time status; it preserves
inclusive overlap and exact `ORDER BY start_date, start_time, title`, with no
row cap. Duplicate rows and the complete requested range remain available.
The repository does not depend on the Application signature type.

The Platform signature port uses `workspaceStore()` and maps each visited
record at the Qt adapter edge into existing normalized UTF-16 keys. It no
longer retains a full `QList<CalendarEvent>` alongside the signature vector.
Generic range reads, imports, the use case, and save behavior remain
unchanged. Repository tests cover selected fields, range/order, invalid
ranges, and read failure. Platform fixture access uses `WorkspaceStore`; its
normalization/order/duplicate/availability/error checks remain, with a
4,097-row duplicate-key test confirming uncapped results.

Executor built `ClassMngr`; Tester built `ClassMngr` and
`ClassMngrCalendarEventRepositoryTests`,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrNextApplicationCalendarEventImportSignatureQueryPortTests`,
`ClassMngrNextApplicationCalendarEventImportUseCaseTests`, and
`ClassMngrCalendarEventImportParityTests`. The five focused CTests passed
**5/5** serially. `git diff --check` passed. Partial row-iteration failure was
not injected; source review confirmed the adapter returns failure instead of
exposing locally accumulated keys. An initial sandboxed import-parity run
could not bind the loopback fixture; the complete five-test serial run passed
with local socket access. No full suite or cross-platform build ran.

## F1347 — Class Analytics dashboard persistence boundary — 2026-10-23

Both raw SQL reads moved from the Platform port into repository-owned
visitors: `RosterRepository::forEachClassAnalyticsRosterNameCell()` and
`SpeakingEvalRepository::forEachClassAnalyticsEvaluationRow()`. Their source-
record types are data/repository-owned and do not depend on Next::Application.
The Platform adapter uses `WorkspaceStore`, maps visitor rows into existing
Application outputs, and contains no SQL, `QSqlQuery`, or `databaseSession()`.

The roster projection preserves exact case-sensitive headers and first-match
order, row count from all in-range columns, sparse rows, strict invalid-index
checks including `>= INT_MAX`, exact diagnostics, and uncapped results. The
evaluation projection selects columns 1–8 only, preserves Winter/Speech
Contest/Summer/Fall bucket and row order, returns empty success for missing
classes/evaluations, and adds no cap or row-index validation. Analytics
behavior and UI remain unchanged.

Tester migrated F1347 fixture DB access in
`tests/next_platform_application_services_class_analytics_dashboard_read_port_tests.cpp`;
existing assertions remain. Added unknown-positive-class empty-success
checks for both reads, contiguous 4,097-row uncapped roster/evaluation checks,
and exact malformed-position error coverage.

Executor built `ClassMngr`; Tester built `ClassMngr` and
`ClassMngrNextPlatformApplicationServicesClassAnalyticsDashboardReadPortTests`,
`ClassMngrNextApplicationClassAnalyticsDashboardReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesRosterReadPortTests`,
`ClassMngrNextPlatformApplicationServicesSpeakingEvaluationRosterNamesReadPortTests`,
and `ClassMngrNextPlatformApplicationServicesSpeakingEvaluationReadPortTests`.
The five focused CTests passed **5/5** serially. `git diff --check` passed.

On 64-bit SQLite, tests cannot construct an index at or above `size_t` max;
coverage checks `INT_MAX`, values above it, and fractional positions, with the
explicit size_t guard source-reviewed. Valid sparse indices below `INT_MAX`
can still cause large allocations, preserving existing behavior. No full
suite or cross-platform build ran.

## F1348 — Roster availability batch read — 2026-10-23

The roster-availability batch-read port now obtains its open state,
`RosterRepository`, and transaction database through `WorkspaceStore`. Its
existing repository calls, single transaction, validation, and result/error
behavior remain unchanged. Availability-specific fixture access in
`tests/next_platform_application_services_roster_read_port_tests.cpp` uses
`WorkspaceStore`; assertions are preserved.

Executor and Tester built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesRosterReadPortTests`,
`ClassMngrNextApplicationRosterAvailabilityBatchReadQueryTests`, and
`ClassMngrRosterTransferMenuTests` incrementally in the existing
`build/macos-clang-debug` tree. The three focused CTests passed **3/3**
serially (Platform #394, Application #297, UI #135). `git diff --check`
passed. Transaction-start and commit-failure injection was not run; source
review confirmed the existing checks and failure mappings. Repository query
failure is covered by the focused tests. No full suite or cross-platform build
ran.

## F1349 — Teacher Import apply — 2026-10-23

`TeacherImportRepository::applyUseCase()` now constructs the existing SQL
persistence adapter, executes the existing use case, and logs rollback
failures. The Platform port uses `WorkspaceStore` and the repository; it no
longer owns SQL-adapter or database access. The legacy `importTeachers()`
method delegates to the typed operation and retains its summary and error
mapping. Direct fixture access in
`tests/next_platform_application_services_teacher_import_apply_port_tests.cpp`
now uses `WorkspaceStore`; assertions are unchanged.

Executor and Tester incrementally built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesTeacherImportApplyPortTests`,
`ClassMngrNextApplicationTeacherImportUseCaseTests`, and
`ClassMngrTeacherImportUiApplyParityTests` in the existing
`build/macos-clang-debug` tree. Serial focused CTest passed the Platform,
Application, and six individually registered Teacher Import UI parity tests
(8 passed). The aggregate `TeacherImportUiApplyParity` CTest failed because
`wizardSuccessTranscript` aborted at `wizard.show()`, before import, with an
uncaught macOS `NSInvalidArgumentException` (`-[NSBundle initWithURL:]: nil
URL argument`) in Qt's QWizard page/pixmap/restart path. Running that slot
alone reproduced the failure. No baseline comparison was run, so attribution
is unknown. `git diff --check` passed. No full suite or cross-platform build
ran.

## F1350 — Schedule slot-state read — 2026-10-23

The Schedule slot-state Platform port and its two direct fixture database
accesses in
`tests/next_platform_application_services_schedule_slot_state_read_port_tests.cpp`
now use `WorkspaceStore`. The repository API and query are unchanged. Ordering,
raw stored values, UTF-16 projection, unavailable-owner message, error codes,
and exception mappings remain unchanged. The separate `DataService` fallback
assertion remains intact.

Executor and Tester incrementally built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesScheduleSlotStateReadPortTests`,
`ClassMngrNextApplicationScheduleSlotStateReadQueryTests`, and
`ClassMngrScheduleWidgetTests` in the existing `build/macos-clang-debug` tree.
The three focused CTests passed **3/3** serially. `git diff --check` passed.
No full suite or cross-platform build ran.

## F1351 — Schedule testing-assignment board read — 2026-10-23

The Schedule testing-assignment Platform port now uses `WorkspaceStore` for
availability and `TestingBlockRepository` access. Its dedicated test fixtures
and repository metrics access also use `WorkspaceStore`; assertions remain.
The repository API/query and result mapping are unchanged, preserving ordered
rows, raw day/time/room values, joined metadata and defaults, malformed class
ID behavior, uncapped results, and existing errors.

Executor and Tester incrementally built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesScheduleTestingAssignmentReadPortTests`,
`ClassMngrNextApplicationScheduleTestingAssignmentReadQueryTests`, and
`ClassMngrScheduleWidgetTests` in the existing `build/macos-clang-debug` tree.
The three focused CTests passed **3/3** serially. `git diff --check` passed.
No full suite or cross-platform build ran.

## F1352 — Schedule testing-class choices — 2026-10-23

The testing-class choices Platform port and the pre-dialog owner-availability
guard in `ScheduleWidget::editTestingAssignment()` now use `WorkspaceStore`.
The repository API/query and choice mapping, order, fields, ID validation,
errors, and dialog behavior remain unchanged. The dedicated Platform test
fixtures and repository access use `WorkspaceStore`. The ScheduleWidget test
helper does as well; its unavailable-store case closes the actual store,
retains the legacy-service stub check, and verifies the same warning.

Executor and Tester incrementally built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesScheduleTestingClassChoicesReadPortTests`,
`ClassMngrNextApplicationScheduleTestingClassChoicesReadQueryTests`, and
`ClassMngrScheduleWidgetTests` in the existing `build/macos-clang-debug` tree.
The three focused CTests passed **3/3** serially. `git diff --check` passed.
No full suite or cross-platform build ran.

## F1353 — Schedule testing-assignment save — 2026-10-23

The Schedule testing-assignment save Platform port and its dedicated test
fixtures now use `WorkspaceStore`. Request validation order, Qt
case-insensitive weekday comparison, save/remove/class-assignment branches,
repository transaction behavior, error mappings, and UI behavior remain
unchanged.

Executor and Tester incrementally built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesScheduleTestingAssignmentSavePortTests`,
`ClassMngrNextApplicationScheduleTestingAssignmentSaveUseCaseTests`, and
`ClassMngrScheduleWidgetTests` in the existing `build/macos-clang-debug` tree.
The three focused CTests passed **3/3** serially (Platform #481, Application
#461, UI #46). `git diff --check` passed. No full suite or cross-platform
build ran.

## F1354 — Clear Schedule testing assignments — 2026-10-23

The clear port now uses `WorkspaceStore` for availability and
`TestingBlockRepository` access, including the availability recheck after a
failed delete. The repository's single-statement delete, preservation of
saved testing classes and rosters, and menu confirmation, warning, and refresh
behavior remain unchanged. Both dedicated F1354 test files now use
`WorkspaceStore` for direct fixture access; assertions remain.

Executor and Tester incrementally built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesScheduleTestingLayoutClearPortTests`,
`ClassMngrNextApplicationScheduleTestingLayoutClearUseCaseTests`, and
`ClassMngrScheduleTestingLayoutClearParityTests` in the existing
`build/macos-clang-debug` tree. In the serial focused CTest run, the Platform
and Application tests passed. Schedule parity failed: six database-backed
cases stopped at `LayoutClearFixture::databaseReady`, before the clear action.
No baseline run was performed, so attribution is unresolved. `git diff --check`
passed. No full suite or cross-platform build ran.

## F1355 — Schedule Editor selected-class information read — 2026-10-23

The selected-class information port in
`src/next/platform/application_services_schedule_editor_class_info_read_port.h`
and its dedicated fixture in
`tests/next_platform_application_services_schedule_editor_class_info_read_port_tests.cpp`
now use `WorkspaceStore` for availability and repository access. Canonical ID
validation still precedes owner access; the detailed repository projection,
all projected fields, and error behavior remain unchanged. A missing class
still returns `NotFound`, while absent metadata and a dangling teacher retain
successful defaults.

The executor incrementally built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesScheduleEditorClassInfoReadPortTests`,
`ClassMngrNextApplicationScheduleEditorClassInfoQueryTests`, and
`ClassMngrNextScheduleEditorDialogSaveTests` in the existing
`build/macos-clang-debug` tree. The three focused CTests passed serially, and
`git diff --check` passed. No full suite or cross-platform build ran.

## F1356 — Testing class teacher choices — 2026-10-23

The Testing class teacher-choice read port in
`src/next/platform/application_services_testing_teacher_choices_read_port.h`
and direct fixture accesses in
`tests/next_platform_application_services_testing_teacher_choices_read_port_tests.cpp`
now use `WorkspaceStore`. Query ordering, raw ID/Korean name/room mapping,
valid-empty results, ID validation, errors and exceptions, and UI behavior
remain unchanged.

The executor incrementally built `ClassMngr`,
`ClassMngrNextPlatformApplicationServicesTestingTeacherChoicesReadPortTests`,
`ClassMngrNextApplicationTestingTeacherChoicesReadQueryTests`, and
`ClassMngrTestingClassesPageTests` in the existing `build/macos-clang-debug`
tree. The Platform, Application, and
`ClassMngrTestingClassesPageTeacherChoiceTests` CTests passed serially. The
direct case
`testingTeacherChoicesTechnicalFailurePreservesAssignedTeacherUntilNoneSelected`
also passed with `QT_QPA_PLATFORM=offscreen`; it emitted
`QSqlQuery::prepare: database not open` warnings but no failed assertions.
`git diff --check` passed. No full suite or cross-platform build ran.

## F1357 — Testing class creation — 2026-10-23

The production port in
`src/next/platform/application_services_testing_class_create_port.h` now uses
`workspaceStore()` for availability and `TestingClassRepository`; the
repository transaction/API and create/UI semantics remain unchanged. The
dedicated Platform fixture and F1357-related mixed UI fixture accesses now use
`WorkspaceStore`. Only the F146 create-failure slot in cancel-failure parity
was migrated; F145/F147 compatibility accesses remain. F146 setup now supplies
the required grade, level, and room fields so the injected create failure is
reached. No assertions were removed or weakened.

`ClassMngr` and four test targets built incrementally in the existing
`build/macos-clang-debug` tree. Serial focused CTests passed 4/4:
`ClassMngrNextApplicationTestingClassCreateUseCaseTests`,
`ClassMngrNextPlatformApplicationServicesTestingClassCreatePortTests`,
`ClassMngrTestingClassesPageF146CreateTests`, and
`ClassMngrTestingClassesPageCancelFailureParityTests`. The parity run emitted
non-failing SQL warnings in unchanged neighboring F145/F147 slots. A direct
QtTest invocation outside CTest crashed after macOS pasteboard setup errors;
no direct-run result is claimed. `git diff --check` passed. No full suite or
cross-platform build ran.

## Batch 2 push receipt — 2026-10-23

The 20 Batch 2 slice commits, F1338–F1357, were pushed from `Qt-Rewrite` to
configured `origin/Qt-Rewrite`. Before the authorized non-force retry, local
HEAD was `c3e9cc5c639c6e1157448ab3da6b98fd7f96c990`, 20 commits ahead. The push
advanced the remote from the prior cached tracking SHA
`9b7ad2b6a2190c0bf1f71c245ba4b6429e24b339` to that HEAD; the live remote tip
now matches and the ahead count is zero. Destination-specific verification
confirmed the configured public GitHub repository and push permission.

## F1358 — Testing Class details read — 2026-10-23

The production details-read port now uses `workspaceStore()` for availability
and `TestingClassRepository`. Only its dedicated Platform fixtures migrated to
`WorkspaceStore`; assertions remain. Repository projection/API, Application
handler, and page wiring are unchanged.

`ClassMngr` and the Application, Platform, and page test targets built
incrementally in `build/macos-clang-debug`. Serial CTests passed for
`ClassMngrNextApplicationTestingClassDetailsReadQueryTests` and
`ClassMngrNextPlatformApplicationServicesTestingClassDetailsReadPortTests`.
`ClassMngrTestingClassesPageTests` failed 4/52; the relevant slot,
`testingClassDetailsReadMapsEditorRosterAndCleanState`, passed. The four failed
cases were `rosterEditorOmitsRemoveButtonAndKeepsContextAction` (menu action
absent), `rosterKeyboardTriggerOpensInAppPalette` (keyboard not visible),
`rosterKeyboardWritesToKoreanNameCell` (keyboard target absent), and
`outputAvailabilityFollowsRosterTabAndLoadedClass` (print action disabled after
tab selection). Read-only review found these cases do not invoke the changed
F1358 port. No baseline ran, so attribution is unresolved. `git diff --check`
passed. No full suite or cross-platform build ran.

## F1359 — Testing Class details update — 2026-10-23

The production details-update port in
`src/next/platform/application_services_testing_class_details_update_port.h`
now uses `workspaceStore()` for availability and repository access. The
repository transaction/API and Application/UI paths are unchanged. F1359
fixture accesses migrated in the dedicated Platform update-port tests, seven
page update-path cases plus two final autosave/roster-recovery reads, and only
the F145 cancel-failure slot. Assertions remain; F1357 F146/F147 and F1360
delete accesses are unchanged.

`ClassMngr` and the Application, Platform, page, and cancel-parity targets
built incrementally in `build/macos-clang-debug`. Six focused serial CTests
passed: `ClassMngrNextApplicationTestingClassDetailsUpdateUseCaseTests`,
`ClassMngrNextPlatformApplicationServicesTestingClassDetailsUpdatePortTests`,
`ClassMngrTestingClassesPageF145UpdateTests`,
`ClassMngrTestingClassesPageF1287SaveRefreshTests`,
`ClassMngrTestingClassesPageWideTeacherIdentityTests`, and
`ClassMngrTestingClassesPageCancelFailureParityTests`.

The full registered `ClassMngrTestingClassesPageTests` CTest separately failed
4/52. `testingClassDetailsAutomaticTechnicalFailureShowsStatusAndRetryPersists`
passed. The failures were
`rosterEditorOmitsRemoveButtonAndKeepsContextAction`,
`rosterKeyboardTriggerOpensInAppPalette`,
`rosterKeyboardWritesToKoreanNameCell`, and
`outputAvailabilityFollowsRosterTabAndLoadedClass`. Attribution is unresolved;
no baseline ran. `git diff --check` passed. No full suite or cross-platform
build ran.

## F1360 — Testing Class delete — 2026-10-23

The delete Platform port and `TestingClassesPage::deleteCurrentClass()`
clean-state `NotFound` guard now use `workspaceStore()`. Repository detailed
delete transaction/API, Application use case, confirmation, dirty-draft
handling, error mappings, `NotFound` reconciliation, selection behavior, and
signal emission are unchanged. Dedicated Platform fixture access, the ten F147
delete page cases, and two F147 cancel-failure parity slots migrated to
`WorkspaceStore`; assertions remain. F145/F146 parity fixtures and unrelated
session references were unchanged. Repository code and tests were unchanged.

`ClassMngr` and six test targets built incrementally in
`build/macos-clang-debug`. Six exact serial CTests passed:
`ClassMngrNextApplicationTestingClassDeleteUseCaseTests`,
`ClassMngrNextPlatformApplicationServicesTestingClassDeletePortTests`,
`ClassMngrTestingClassesPageF147DeleteTests`,
`ClassMngrTestingClassesPageDeleteParityTests`,
`ClassMngrTestingClassesPageCancelFailureParityTests`, and
`ClassMngrTestingClassRepositoryTests`. `git diff --check` passed. The broad
page CTest and full suite were not run; no cross-platform build ran. F1360 is
accepted.

## F1361 — Class Co-Teacher page read — 2026-10-23

The production read port in
`src/next/platform/application_services_class_co_teacher_page_read_port.h`
now obtains store availability, `ClassInfoRepository`, and
`TeacherRepository` through `workspaceStore()`. Direct fixture access and
includes in `tests/next_platform_application_services_class_co_teacher_page_read_port_tests.cpp`,
`tests/class_co_teacher_page_read_parity_tests.cpp`, and
`tests/next_feature_class_co_teacher_page_tests.cpp` now use `WorkspaceStore`;
assertions and Qt test slots are unchanged. Repository projection/API/SQL,
Application query, page/UI, teacher choices, and assignment save are
unchanged.

`ClassMngr` and four test targets built incrementally in
`build/macos-clang-debug`. Four registered serial CTests passed: `ClassMngrNextApplicationClassCoTeacherPageReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesClassCoTeacherPageReadPortTests`,
`ClassMngrClassCoTeacherPageReadParityTests`, and
`ClassMngrNextFeatureClassCoTeacherPageTests`. The separate wide-teacher CTest
was not run because its slot is covered by the feature target.
`git diff --check` passed. No full suite or cross-platform build ran.
Application query slots do not directly exercise the malformed returned
teacher-ID guard; Platform tests cover malformed persisted IDs. F1361 is
accepted and committed as `1cb73e0705d8d8b9ef62ef578a81b7521bfb99d`.

## F1362 — Class Co-Teacher teacher choices read — 2026-10-23

The production read port in
`src/next/platform/application_services_class_co_teacher_teacher_choices_read_port.h`
now includes `workspace_store.h`, checks `WorkspaceStore::isOpen()`, and obtains
`TeacherRepository` through `WorkspaceStore`. Its dedicated fixture,
`tests/next_platform_application_services_class_co_teacher_teacher_choices_read_port_tests.cpp`,
uses `WorkspaceStore` for repository and database access. All six existing
test slots and assertions are unchanged. The Application query,
repository/API/SQL, page UI, F1361 page-details adapter, and F1363 assignment
save are unchanged. Relevant existing contracts include no-fallback and
unavailable-store classification, error mapping, ID validation/conversion,
full field projection, empty result, exception mapping, and repository order.

Three targets built incrementally in `build/macos-clang-debug`:
`ClassMngrNextPlatformApplicationServicesClassCoTeacherTeacherChoicesReadPortTests`,
`ClassMngrNextApplicationClassCoTeacherTeacherChoicesReadQueryTests`, and
`ClassMngrNextFeatureClassCoTeacherPageTests`. Four serial CTests passed,
including separately registered `NextFeatureClassCoTeacherWideTeacherIdentity`.
`git diff --check` passed. No `DatabaseSession`, `databaseSession()`, or
`database_session.h` references remain in the scoped adapter and fixture.
The fixture does not directly exercise missing-repository or thrown-exception
branches. No full suite or cross-platform build ran. F1362 is accepted and
committed as `0074eb71efb3fbe6f14b6cf8c277eda8efec60c3`; F1363 is current.

## F1363 — Class Co-Teacher assignment save — 2026-10-23

The save port in
`src/next/platform/application_services_class_co_teacher_assignment_port.h`
now includes `workspace_store.h`, checks `WorkspaceStore::isOpen()`, obtains
`ClassInfoRepository` from the store, and uses store state for error
reclassification and `repositoryFailure`. No `DatabaseSession` or
`databaseSession()` references remain in the adapter. Direct fixture accessors
and includes in
`tests/next_platform_application_services_class_co_teacher_assignment_port_tests.cpp`
and `tests/class_co_teacher_assignment_persistence_parity_tests.cpp` now use
`WorkspaceStore`; assertions, SQL semantics, and test slots are unchanged.
The Application use case, repository API/SQL/transaction, page/UI, F1361
post-save readback, and F1361/F1362 adapters are unchanged. Existing contracts
remain: input/sentinel conversion and qint64 teacher IDs, validation snapshot
and model projection, regular-then-intensive conflict order, transactional
repository call, unavailable-store and missing class/teacher mappings, error
recoverability and technical failures, and metadata preservation.

Four targets built incrementally in `build/macos-clang-debug`:
`ClassMngrNextApplicationClassCoTeacherAssignmentTests`,
`ClassMngrNextPlatformApplicationServicesClassCoTeacherAssignmentPortTests`,
`ClassMngrClassCoTeacherAssignmentPersistenceParityTests`, and
`ClassMngrNextFeatureClassCoTeacherPageTests`. Five serial CTests passed,
including separately registered `NextFeatureClassCoTeacherWideTeacherIdentity`.
`git diff --check` passed. The scoped adapter and fixtures contain no
`DatabaseSession`, `databaseSession()`, or `database_session.h` references.
Existing Platform tests still have no explicit null-services,
missing-repository, injected-exception, or transaction-commit-failure slot.
No full suite or cross-platform build ran. F1363 is accepted and committed as
`2211d4f3dbc6586a7055b771eefb5e2c7da95441`.

## F1364 — Class Notes page read — 2026-10-23

The production port in
`src/next/platform/application_services_class_notes_page_read_port.h` now
includes `workspace_store.h` and uses `WorkspaceStore::isOpen()`,
`classInfoRepository()`, and `teacherRepository()`; no `DatabaseSession` APIs
remain. Its focused fixtures,
`tests/next_platform_application_services_class_notes_page_read_port_tests.cpp`
and `tests/class_notes_page_read_parity_tests.cpp`, use `WorkspaceStore`.
Assertions, SQL, test slots, and parity behavior are unchanged. The
Application query, repository projection/API/SQL, UI, and F1365 save port are
unchanged.

Preserved behavior includes canonical ID/query-response matching, independent
class fields and teacher-name results, ordered regular schedule, exact notes
and activities, missing-parent and metadata handling, teacher fallback and
error mapping, no DataService fallback, read metrics, and SQL-failure
visibility.

`cmake --build build/macos-clang-debug --parallel 4` passed on macOS arm64 with
Apple Clang and Qt 6.12. Four registered serial CTests passed:
`ClassMngrNextPlatformApplicationServicesClassNotesPageReadPortTests`,
`ClassMngrNextApplicationClassNotesPageReadQueryTests`,
`ClassMngrClassNotesPageReadParityTests`, and
`ClassMngrNextFeatureClassNotesPageTests`. Across them, including init/cleanup,
38 passed, 0 failed, and 0 skipped. `git diff --check` passed. The scoped
adapter and fixtures contain no `DatabaseSession`, `databaseSession()`, or
`database_session.h`. Feature tests emitted non-failing font-fallback and
offscreen `propagateSizeHints` warnings. No full CTest suite or cross-platform
build ran. F1364 is accepted and committed as
`b9921f7da1a0949f76e4d14b49e3b10aedec8862`.

## F1365 — Class Notes save — 2026-10-23

The production save port in
`src/next/platform/application_services_class_notes_save_port.h` now includes
`workspace_store.h` and uses `WorkspaceStore` for availability,
`ClassInfoRepository`, and post-write open-state checks; no `DatabaseSession`
API remains. Its focused fixtures,
`tests/next_platform_application_services_class_notes_save_port_tests.cpp` and
`tests/class_notes_page_save_parity_tests.cpp`, now use `WorkspaceStore`.
Assertions, SQL semantics, test slots, and parity behavior are unchanged. The
page/UI/autosave, Application use case/port, detailed repository
transaction/API/SQL, F1364 read adapter, and F1366 adapter are unchanged.

Existing contracts include request and ID validation, text trimming and
validation, transactional write, parent-missing classification,
unavailable-store recovery, technical failures, diagnostics, and failed-write
preservation.

Four targets built incrementally in `build/macos-clang-debug`. Four serial
CTests passed: `ClassMngrNextApplicationClassNotesSavePortTests`,
`ClassMngrNextPlatformApplicationServicesClassNotesSavePortTests`,
`ClassMngrNextFeatureClassNotesPageTests`, and
`ClassMngrClassNotesPageSaveParityTests`. `git diff --check` passed. The
scoped adapter and fixtures contain no `DatabaseSession`,
`databaseSession()`, or `database_session.h`. Existing Platform tests do not
separately cover transaction-start/commit failure, null services, missing
repository, or injected exceptions. No full suite or cross-platform build
ran. F1365 is accepted and committed as
`5833f388eaefdf38d1be9812a72267b8f2169d20`; F1366 is accepted and F1367
(Selected-class subtitle read) is current.

## F1366 — Selected-class grade read (Batch 3 candidate 9 of 20) — 2026-10-23

The port in
`src/next/platform/application_services_selected_class_grade_read_port.h`
now includes `workspace_store.h`, obtains the store via `workspaceStore()`,
checks `isOpen()`, and gets `ClassInfoRepository` through the store. Its
fixture, `tests/next_platform_application_services_selected_class_grade_read_port_tests.cpp`,
uses `WorkspaceStore` for repository, class-repository, metrics, and SQL
access. Assertions, SQL semantics, slots, and legacy-wrapper checks are
unchanged. The Application query, repository projection/API/SQL, Classes page
UI/stubs, and F1367 subtitle adapter are unchanged. The scoped adapter and
fixture contain no `DatabaseSession`, `databaseSession()`, or
`database_session.h` references.

Builds passed for all three targets:
`ClassMngrNextApplicationSelectedClassGradeReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesSelectedClassGradeReadPortTests`,
and `ClassMngrClassesPageTests`. The first two serial CTests passed. The
`ClassMngrClassesPageTests` CTest timed out at 300.010 s in
`nestedEditorsAreDeferredUntilTheirSectionIsOpened`, before reaching the
selected-grade tests. A separate focused run of
`selectedClassGradeFailureFailsOpenWithoutDataServiceFallback` hung for 31.7 s
and was interrupted after `QSqlQuery::prepare: database not open`.

Read-only source review found production `databaseSession()` and
`workspaceStore()` return the same `m_workspaceStore`, and the grade-page
test's repository method is stubbed to return configured data or failure. The
hang appears downstream when the test opens Details: its support stub
overrides the legacy details read, while the page path calls the detailed read
against a default unopened `QSqlDatabase`; failure can then trigger a modal
dialog. This is a source-supported explanation, not runtime-stack proof, and
no baseline run was captured. Page-suite acceptance remains unresolved; no
evidence supports attributing the timeout to F1366. `git diff --check` passed.
No full suite or cross-platform build ran. F1366 is accepted; F1367
(Selected-class subtitle read) is current. F1366 is committed as
`ad5f9cb736a6ffe4b79817bd9e1d3f5661684ef5`.

## F1367 — Selected-class subtitle read (Batch 3 candidate 10 of 20) — 2026-10-24

The port in
`src/next/platform/application_services_selected_class_subtitle_read_port.h`
now uses `WorkspaceStore` for availability, `ClassInfoRepository`, and the
assigned-teacher `TeacherRepository`; it has no `DatabaseSession` API. Its
fixture,
`tests/next_platform_application_services_selected_class_subtitle_read_port_tests.cpp`,
uses `WorkspaceStore` for all direct repository and database access. Assertions,
SQL semantics, slots, and fixture behavior are unchanged. The Application
query, repository detailed projection/API/SQL, ClassesPage and RosterEditor UI
and stubs, and F1368 batch query/adapter are unchanged.

Preserved contracts include canonical ID and identity, independent class and
teacher results, class grade/level and ordered regular schedule, wide teacher
ID, missing parent and unavailable-store handling, errors, fallback, and no
fallback.

The incremental build passed for
`ClassMngrNextApplicationSelectedClassSubtitleReadQueryTests` and
`ClassMngrNextPlatformApplicationServicesSelectedClassSubtitleReadPortTests`.
Both serial CTests passed (2/2), as did `git diff --check`. The scoped adapter
and fixture contain no `DatabaseSession`, `databaseSession()`, or
`database_session.h` references.

`ClassMngrClassesPageTests` was not rerun: F1366's run timed out at 300 seconds
before selected-grade cases. Source review found subtitle test counters and
failure controls in `ScheduleWidgetTestStubs` target legacy
`loadClassSubtitleRecord()` and `loadTeacherDisplayNameFields()`, while the
production adapter uses detailed class read and `lookupTeacherDisplayName()`.
The UI assertions therefore appear disconnected from the detailed adapter
path. This is source-level evidence, not a new runtime result; UI integration
remains unverified, and the prior timeout is not attributed to F1367. No full
suite or cross-platform validation ran. F1367 is accepted; F1368
(Selected-class subtitle batch read) is current. F1367 is committed as
`b1da9e3545ce5d7f0816b3d3880e801f264d0702`.

## F1368 — Selected-class subtitle batch read (Batch 3 candidate 11 of 20) — 2026-10-24

The port in
`src/next/platform/application_services_selected_class_subtitle_batch_read_port.h`
now includes `workspace_store.h`, checks `WorkspaceStore::isOpen()`, and gets
`ClassInfoRepository` and `TeacherRepository` from `workspaceStore()`. Its
fixture, `tests/next_platform_application_services_selected_class_subtitle_batch_read_port_tests.cpp`,
uses `WorkspaceStore` for repository and SQL access. Assertions, slots, SQL,
metrics, and outcomes are unchanged. The Application query, repository batch
APIs/SQL, roster-transfer and print callers, import/export and sidebar-prompt
callers, F1367 single-read port, and F1369 navigation are unchanged.

Preserved behavior includes empty input, canonical unique IDs, matching count
and order, independent class and teacher batch outcomes, schedule order,
missing parents, teacher-ID validation and wide IDs, and error handling.

All requested targets built. Four serial CTests passed: the selected-class
subtitle batch-read Application query and Platform port, roster-transfer menu,
and roster-print dialog tests (4/4). The F1368 import slot
`importDialogBatchesDistinctDestinationSubtitleReads` also passed in
`ClassMngrClassTransferTests`. That target had 63 passed and one failed:
`importDialogFallsBackToIndividualTeacherProfilesAfterBatchFailure` observed
`Keep local: New Teacher` instead of `Keep local: Romanized One`.

The failure is a confirmed baseline issue, not attributable to F1368: the test
has no class IDs, so the subtitle batch query returns before the F1368 adapter;
the failure follows teacher-display-name batch and teacher-profile fallback
queries after the test renames `preferred_name`, breaking both. The F1326
progress log records a clean F1326 archive reproducing the same isolated
assertion with the same actual and expected names. `git diff --check` passed.
No scoped `DatabaseSession` APIs remain in the adapter or fixture.

`ClassMngrClassesPageTests` remains unrun following F1366's 300-second timeout
before selected-grade cases. F1367 source review found its page stubs target
legacy subtitle methods while the detailed adapter uses different methods, so
UI integration remains unverified. This is source-level evidence, not a
runtime result. No full suite or cross-platform build ran. F1368 is accepted;
F1369 (Classes navigation read) is current. F1368 is committed as
`87efa3ec7e935871767786b71acec856ecb410aa`.

## F1369 — Classes navigation read (Batch 3 candidate 12 of 20) — 2026-10-24

The port in
`src/next/platform/application_services_classes_navigation_read_port.h`
now includes `workspace_store.h`, checks `WorkspaceStore::isOpen()`, and
obtains `ClassInfoRepository` through `workspaceStore()`. Its direct fixture,
`tests/next_platform_application_services_classes_navigation_read_port_tests.cpp`,
was migrated to `WorkspaceStore`; assertions, SQL, and metrics are unchanged.
Neither adapter nor fixture uses `DatabaseSession` APIs. A test-support-only
addition in `tests/classes_page_test_support.cpp` provides deterministic
class-details, teacher display-name, and roster-count stubs using existing
fixture data while preserving assertions and counters. The Application query,
repository/navigation SQL, UI, and F1370 route are unchanged.

Builds passed for the Application snapshot, Platform port, and ClassesPage
targets. The serial CTests
`ClassMngrNextApplicationClassesNavigationSnapshotTests` and
`ClassMngrNextPlatformApplicationServicesClassesNavigationReadPortTests`
passed. Five filtered ClassesPage slots also passed individually, each with
3 passed and 0 failed:
`classesNavigationReadFailureKeepsNamesAndBlankMetadata`,
`classesNavigationEntryFailureKeepsIdentityAndValidSiblingDetails`,
`classInfoSaveRefreshesNavigationSnapshot`,
`selectedClassGradeFailureFailsOpenWithoutDataServiceFallback`, and
`classesListQueryUsesActiveRepositoryOnOpenAndAfterInfoSave`. The focused page
slots emitted a nonfatal `QSqlQuery::prepare: database not open` warning from
a separate subtitle-read path; this does not establish that F1367 UI
integration is fixed.

The full registered `ClassMngrClassesPageTests` CTest had 19 passed and one
failure: `navigationReleaseCachesSnapshotAndEditorsAcrossReentry` timed out at
300.010 seconds; CTest completed at 300.54 seconds and aborted. This slot
exercises F1369 navigation as well as Notes, Details, and editor reentry. The
log at `/private/tmp/F1369-TEST-01-ClassMngrClassesPageTests-ctest.log`
contains DB-not-open and `propagateSizeHints` warnings but no per-assertion
progress; the blocked statement is unknown. Read-only triage did not establish
attribution, so broad page-target acceptance remains unresolved. Do not
attribute the timeout to F1369 or call it unrelated. No full project suite or
cross-platform build ran. `git diff --check` passed. F1369 is accepted.
Commit: `bd6a56c3f53e2512bafe36edf2615c7d5d7dddc7`.

## F1370 — Class teacher assignments read (Batch 3 candidate 13 of 20) — 2026-10-24

The platform port in
`src/next/platform/application_services_class_teacher_assignments_read_port.h`
now includes `workspace_store.h`, checks `WorkspaceStore::isOpen()`, and gets
`ClassInfoRepository` through `workspaceStore()`. Its focused fixture,
`tests/next_platform_application_services_class_teacher_assignments_read_port_tests.cpp`,
was fully migrated to `WorkspaceStore`; assertions, slots, and SQL are
unchanged, with no remaining `DatabaseSession` API references in the port or
fixture.

In `tests/navigation_teacher_read_tests.cpp`, shared helpers `persistTeacher`
and `createClassWithSubtitle`, plus SQL fixture access in two assignment-read
failure slots, now use `workspaceStore()`. Remaining `databaseSession()` calls
in that large test source belong to out-of-scope slots and were left
untouched. The Application query, repository batch API/SQL, sidebar production
callers, and F1371 create/by-ID routes are unchanged.

Incremental builds passed for the Application query, Platform port, and
`ClassMngrNavigationTeacherReadTests` targets. The two serial registered CTests
`ClassMngrNextApplicationClassTeacherAssignmentsReadQueryTests` and
`ClassMngrNextPlatformApplicationServicesClassTeacherAssignmentsReadPortTests`
passed. Five sidebar QtTest slots passed individually under offscreen, each
with 3 passed and 0 failed:
`refreshTeacherSidebarShowsAssignedAndUnassignedTeachers`,
`refreshTeacherSidebarFailureWarnsClearsNodesAndUpdatesActions`,
`refreshTeacherSidebarKeepsUnassignedClassActionEnabled`,
`refreshTeacherSidebarAssignmentFailureWarnsAndUpdatesActions`, and
`refreshTeacherSidebarReturnsSilentlyWithoutAnActiveSession`.
`git diff --check` passed. Focused tests do not directly assert testing-class
exclusion. No full NavigationTeacherRead suite, full project suite, or
cross-platform build ran. F1370 is accepted; F1371 (Regular Class creation
and post-create by-ID lookup) is current.
Commit: `88d978f91f145b09a93a9bfc08616b416a65c92e`.

## F1371 — Regular Class creation and post-create by-ID lookup (Batch 3 candidate 14 of 20) — 2026-10-24

The Regular Class create and by-ID read platform ports in
`src/next/platform/application_services_class_create_port.h` and
`src/next/platform/application_services_class_by_id_read_port.h` now include
`workspace_store.h` and use `WorkspaceStore` via `workspaceStore()` for store
availability and `ClassRepository` access. The blank create request,
create-error mapping and ID handling, canonical by-ID validation,
missing/repository/technical error mapping, identity check, and class-name
mapping are unchanged.

Focused fixtures now use `WorkspaceStore` in
`tests/next_platform_application_services_class_create_port_tests.cpp`,
`tests/next_platform_application_services_class_by_id_read_port_tests.cpp`,
`tests/initial_setup_wizard_class_create_save_parity_tests.cpp`, and the
F1371 success/readback-failure sections of
`tests/mainwindow_classes_sidebar_root_navigation_tests.cpp`. Assertions,
test slots, and SQL semantics are unchanged. Unrelated session-lifecycle slots
in the large MainWindow test remain out of scope. Application contracts,
repository API/SQL, controller order, post-create Classes/Details and sidebar
behavior, the Initial Setup consumer, and F1372 delete are unchanged. This
remains one create flow using both ports; no create/read transaction was added.

Six targets built successfully in `build/macos-clang-debug`. Six serial
registered CTests passed (6/6):
`ClassMngrNextApplicationClassCreateUseCaseTests`,
`ClassMngrNextPlatformApplicationServicesClassCreatePortTests`,
`ClassMngrNextApplicationClassByIdReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesClassByIdReadPortTests`,
`ClassMngrMainWindowClassesSidebarRootNavigationTests`, and
`ClassMngrInitialSetupWizardClassCreateSaveParityTests`. Focused coverage
includes overflow behavior; there is no dedicated create/read transaction-
state assertion because the pair remains nontransactional. `git diff --check`
passed. No full suite or cross-platform build ran. F1371 is accepted; F1372
(Class delete) is current.

Commit: `371a3776ca2a0dbe3ca91c0f42f93ab2a308a5b8`.

## F1372 — Class delete (Batch 3 candidate 15 of 20) — 2026-10-24

The only production change moved
`src/next/platform/application_services_class_delete_port.h` to
`WorkspaceStore`. The focused direct-port fixture in
`tests/next_platform_application_services_class_delete_port_tests.cpp` and
sidebar-parity fixture in `tests/sidebar_class_delete_parity_tests.cpp` now
also use `WorkspaceStore`; assertions and SQL are unchanged. Repository delete
transaction/cascade behavior, ID validation, error mapping, and controller
order are unchanged. This slice adds no behavior.

Three targets built, and three serial registered CTests passed (3/3):
`ClassMngrNextApplicationClassDeleteUseCaseTests`,
`ClassMngrNextPlatformApplicationServicesClassDeletePortTests`, and
`ClassMngrSidebarClassDeleteParityTests`. `git diff --check` passed. No full
suite or cross-platform build ran. F1372 is accepted; F1373 (Class Transfer
preview read) is current.

Commit: `9eb1191923f8f009b201c7d43b183ba6e6271c96`.

## F1373 — Class Transfer preview read (Batch 3 candidate 16 of 20) — 2026-10-24

The only production change moved
`src/next/platform/application_services_class_transfer_preview_read_port.h`
to `WorkspaceStore`. The shared fixture
`tests/sidebar_class_transfer_apply_parity_tests.cpp` now uses
`WorkspaceStore` for setup, repository access, SQL handles, and failure
helpers. Assertions and SQL semantics are unchanged. The
`ClassTransferRepository` snapshot, query/normalization/matching, controller
order, and export/apply production ports are unchanged.

Built `ClassMngrClassTransferTests` and
`ClassMngrSidebarClassTransferApplyParityTests`. Three selected serial CTests
passed: `ClassTransferPreviewAdapterRegressionTests`,
`SidebarClassTransferMatchedPreviewIntegrationTests`, and
`SidebarClassTransferPreviewReadFailureIntegrationTests`. The full registered
`ClassMngrSidebarClassTransferApplyParityTests` CTest also passed.
These tests exercise the production port through the controller/query path;
no dedicated direct Platform port test was added. `git diff --check` passed.
No full suite or cross-platform build ran. F1373 is accepted; F1374 (Class
Transfer export source read) is current.

Commit: `f7d368a52d9ac04fdbf6c18e586eb07f2fc7c16a`.

## F1374 — Class Transfer export source read (Batch 3 candidate 17 of 20) — 2026-10-24

`src/next/platform/application_services_class_transfer_export_source_read_port.h`
now uses `WorkspaceStore`. Its 13-slot focused fixture in
`tests/next_platform_class_transfer_export_source_tests.cpp` also uses
`WorkspaceStore`. Request-ID deferral, call ordering, and error/result mapping
are unchanged. Repository staged reads, transaction/rollback behavior,
selection, and serialization are unchanged.

Built `ClassMngrNextPlatformClassTransferExportSourceTests` and
`ClassMngrClassTransferExportCodecParityTests`. The exact registered CTest
selection was confirmed and passed serially (2/2). `git diff --check` passed.
No full suite or cross-platform build ran. F1374 is accepted; F1375
(Class Transfer apply) is current.

Commit: `127036ba3720e0121c59a2ebc66fb06e8d87c3e3`.

## F1375 — Class Transfer apply (Batch 3 candidate 18 of 20) — 2026-10-24

The only production change moved
`src/next/platform/application_services_class_transfer_apply_port.h` to
`WorkspaceStore`. Its 14-slot direct fixture,
`tests/next_platform_application_services_class_transfer_apply_port_tests.cpp`,
also migrated to `WorkspaceStore`; assertions and SQL are unchanged.
Repository schedule preflight, write transaction/rollback,
validation/error mapping, and controller order are unchanged.

Four requested targets built, and four serial CTests passed (4/4):
`ClassMngrNextApplicationClassTransferApplyUseCaseTests`,
`ClassMngrNextPlatformApplicationServicesClassTransferApplyPortTests`,
`ClassTransferApplyStaleChoiceRegressionTests`, and
`ClassMngrSidebarClassTransferApplyParityTests`. `git diff --check` passed.
No full suite or cross-platform build ran. F1375 is accepted; F1376 (My
Classes class-information batch read) is current.

Commit: `e6ef17cddd6c1489de7d73f73de99bdc7f0be5c4`.

## F1376 — My Classes class-information batch read (Batch 3 candidate 19 of 20) — 2026-10-24

The only production change moved
`src/next/platform/application_services_my_classes_class_information_batch_read_port.h`
to `WorkspaceStore`. In the shared test file
`tests/next_platform_application_services_my_classes_class_information_batch_read_port_tests.cpp`,
only the six F1376 class-information slots migrated their fixtures and
accessors. `database_session.h` and both F1377 summary slots remain unchanged.
IDs, ordering, repository batch reads/fallbacks/errors, UI call order, and the
repository method are unchanged; no transaction was added.

Four selected targets built, and four serial CTests passed (4/4):
`ClassMngrNextApplicationMyClassesClassInformationBatchReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests`,
`ClassMngrMyClassesPageTests`, and
`ClassMngrMyClassesPageSummaryParityTests`. The shared Platform CTest also
executes the two unchanged F1377 summary slots. `git diff --check` passed.
No full suite or cross-platform build ran. F1376 is accepted; F1377 (My
Classes class-summary batch read) is current.

Commit: `795952df3ab4e0ae2ddc39c14cd7fcabb1cb5396`.

## F1377 — My Classes class-summary batch read (Batch 3 candidate 20 of 20) — 2026-10-24

The only production change moved
`src/next/platform/application_services_my_classes_class_summary_batch_read_port.h`
to `WorkspaceStore`. The two F1377 summary slots in the shared Platform test
file, `tests/next_platform_application_services_my_classes_class_information_batch_read_port_tests.cpp`,
now use `WorkspaceStore`, removing the `DatabaseSession` include. F1376 slots
remain unchanged. Repository batch/fallback behavior, query validation and
ordering, and UI call order are unchanged; no transaction was added.

The query test now treats `2147483648` as a valid canonical positive 64-bit
teacher ID: `rejectsInvalidReturnedTeacherIds` uses `9223372036854775808` as
its overflow case, and the success case verifies preservation of
`2147483648`. This preserves validation coverage without changing production
behavior.

Built the summary-query, shared Platform-port, and My Classes page-summary
parity targets. Three serial CTests passed (3/3). The shared Platform CTest
also exercises the two F1377 summary slots. `git diff --check` passed. No full
suite or cross-platform build ran. F1377 is accepted; Batch 3 is complete and
pushed; F1378 (My Classes teacher-profile batch read) is current in Batch 4.

## Batch 3 push receipt — 2026-10-24

The 20 slice commits F1358–F1377 were pushed from `Qt-Rewrite` to the
configured `origin/Qt-Rewrite`. Before the push, local HEAD was
`018b30b04ab713769f9e69b440ba7223310a2b87`, 20 commits ahead and 0 behind;
the remote prior tip was `c3e9cc5c639c6e1157448ab3da6b98fd7f96c990`. A normal,
non-force push advanced the remote from that prior tip to
`018b30b04ab713769f9e69b440ba7223310a2b87`. Post-push verification confirmed
the remote matched local HEAD; at that check, the worktree was clean and
ahead/behind was 0/0. The receipt documentation was later committed and
pushed as `6597875f6036224767fa8db7331eac7eac856869`, the remote tip before
Batch 4 began.

## F1378 — My Classes teacher-profile batch read (Batch 4 candidate 1 of 20) — 2026-10-24

The only production change moved
`src/next/platform/application_services_my_classes_teacher_profile_batch_read_port.h`
to `WorkspaceStore`. Its six-slot direct Platform fixture,
`tests/next_platform_application_services_my_classes_teacher_profile_batch_read_port_tests.cpp`,
also migrated to `WorkspaceStore`. The repository batch query, ordering,
error mapping, per-tab sequence, and fallback are unchanged.

The query test now includes valid teacher ID `2147483648` in ordered-success
coverage; the invalid case uses `9223372036854775808`, which is beyond signed
int64. No production logic changed.

Built `ClassMngrNextApplicationMyClassesTeacherProfileBatchReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesMyClassesTeacherProfileBatchReadPortTests`,
and `ClassMngrMyClassesPageTests`. Three serial CTests passed (3/3).
`git diff --check` passed. No full suite or cross-platform build ran. F1378 is
accepted; F1379 (My Classes teacher-display-name batch read) is current.

Commit: `78693d62fbcfc8219d21221c86919268d3b36f17`.

## Batch 4 shortlist audit — 2026-10-24

The history/source audit removed three duplicate candidates. The previous
F1379 student-count candidate duplicates completed F1323 (commit
`79004e09d78e0f22c8eb8c1d3c5abcab3bb52d44`); its progress entry records the
My Classes port and fixture already using `WorkspaceStore`. Previous F1386
read and F1387 save candidates duplicate completed F1328 and F1329 (commits
`fee2d6db9610d6556e076908a5954f5ccb11cfb6` and
`e696bbf00b2f9e7eaeb04bae6b5d32b92c3cac38`); both progress entries record
their migrations to `WorkspaceStore`. The previous F1380 teacher-display-name
read moved to F1379 current; the remaining shortlist was renumbered in order.

Schedule-display-mode, calendar first-day-of-week, and academic-calendar
schedule preferences were promoted from Batch 5 overflow to F1395–F1397.
The newly promoted preferences are backed by these source and registration
references: F1395 schedule-display-mode uses
`src/next/platform/application_services_schedule_display_mode_preferences_port.h`
(`cmake/tests/next.cmake:4407`); F1396 calendar first-day-of-week uses
`src/next/platform/application_services_calendar_first_day_of_week_preferences_port.h`
(`cmake/tests/next.cmake:3879`); F1397 academic-calendar schedule uses
`src/next/platform/application_services_academic_calendar_schedule_preferences_port.h`
(`cmake/tests/next.cmake:3889`). Read-only inspection found these ports still
use `DatabaseSession`; registrations are not test results, and no tests were
run for these candidates. The remaining Batch 5 candidate families and
direct-session-check follow-ups remain in the tracker. No product code changed
in this audit.

## F1379 — My Classes teacher-display-name batch read (Batch 4 candidate 2 of 20) — 2026-10-24

The only production change moved
`src/next/platform/application_services_teacher_display_name_batch_read_port.h`
to `WorkspaceStore`; the direct three-slot fixture in
`tests/next_platform_application_services_teacher_display_name_batch_read_port_tests.cpp`
migrated as well. In
`tests/next_application_teacher_display_name_batch_read_query_tests.cpp`, the
wide success request changes from `3` to `2147483648` and the invalid boundary
from `2147483648` to `9223372036854775808`, preserving the signed 64-bit
contract. Repository query/order, partial rows, error mappings, and page call
order are unchanged.

Built the query, Platform port, My Classes page, and teacher-display parity
targets. Four selected serial CTests passed (4/4):
`NextApplicationTeacherDisplayNameBatchReadQuery`,
`NextPlatformApplicationServicesTeacherDisplayNameBatchReadPort`,
`MyClassesPage`, and `MyClassesPageTeacherDisplayParity`.
`git diff --check` passed. No full suite or cross-platform build ran. F1379 is
accepted; F1380 (Roster snapshot read) is current.

Commit: `5714a9f455a7070606d9338fcf7c7e4f34bafca0`.

## F1380 — Roster snapshot read (Batch 4 candidate 3 of 20) — 2026-10-24

The only production change moved
`src/next/platform/application_services_roster_read_port.h` to
`WorkspaceStore` for open-state and repository lookup. Only direct
snapshot-read fixture accesses in
`tests/next_platform_application_services_roster_read_port_tests.cpp` migrated;
assertions are unchanged. Repository, query, and UI semantics are unchanged.

Built the selected query, Platform-port, and roster-template-print-service
targets. Three selected serial CTests passed (3/3):
`NextApplicationRosterReadQuery`,
`NextPlatformApplicationServicesRosterReadPort`, and
`ClassMngrRosterTemplatePrintServiceTests`. `git diff --check` passed. No full
suite or cross-platform build ran. F1380 is accepted; F1381 (Roster
column-name batch read) is current.

Commit: `646790e13eb96b31f2759bdbf4b6f604a189919f`.

## F1381 — Roster column-names batch read (Batch 4 candidate 4 of 20) — 2026-10-24

The production roster-column-names batch-read port,
`src/next/platform/application_services_roster_column_names_batch_read_port.h`,
now uses `WorkspaceStore` for open-state and repository access. Direct
persistence accesses in its dedicated Platform fixture,
`tests/next_platform_application_services_roster_column_names_batch_read_port_tests.cpp`,
migrated; assertions are unchanged. The application query, repository
behavior, and UI flow are unchanged.

Built `ClassMngrNextApplicationRosterColumnNamesBatchReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests`,
and `ClassMngrRosterPrintDialogTests`. Three selected serial CTests passed
(3/3): `NextApplicationRosterColumnNamesBatchReadQuery`,
`NextPlatformApplicationServicesRosterColumnNamesBatchReadPort`, and
`RosterPrintDialog`. `git diff --check` passed. No full suite or cross-platform
build ran. F1381 is accepted; F1382 (Roster template-print source read) is
current.

Commit: `ab461a7fe0540d63eef0b1a0c7becabc83f2380d`.

## F1382 — Roster template-print source read (Batch 4 candidate 5 of 20) — 2026-10-24

The production port,
`src/next/platform/application_services_roster_template_print_source_read_port.h`,
uses `WorkspaceStore` for open-state and both repository lookups. Direct
persistence accesses in the dedicated Platform fixture,
`tests/next_platform_application_services_roster_template_print_source_read_port_tests.cpp`,
also use `WorkspaceStore`. Assertions, batch-read ordering, and service
fallback are unchanged.

Built `ClassMngrNextApplicationRosterTemplatePrintSourceReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesRosterTemplatePrintSourceReadPortTests`,
and `ClassMngrRosterTemplatePrintServiceTests`. `ctest -N` confirmed three
registered checks, and three selected serial CTests passed (3/3):
`ClassMngrNextApplicationRosterTemplatePrintSourceReadQueryTests`,
`NextPlatformApplicationServicesRosterTemplatePrintSourceReadPort`, and
`ClassMngrRosterTemplatePrintServiceTests`. `git diff --check` passed. No full
suite or cross-platform build ran. F1382 is accepted; F1383 (Roster print
class-info read) is current.

Commit: `2557098a578ee255aa791d989046de3d482cf6b9`.

## F1383 — Roster print class-info read (Batch 4 candidate 6 of 20) — 2026-10-24

The production Roster print class-info port,
`src/next/platform/application_services_roster_print_class_info_read_port.h`,
now uses `WorkspaceStore` for open-state and repository lookup. Direct
persistence accesses in its Platform fixture,
`tests/next_platform_application_services_roster_print_class_info_read_port_tests.cpp`,
migrated; all assertions are unchanged.

Built `ClassMngrNextApplicationRosterPrintClassInfoReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesRosterPrintClassInfoReadPortTests`,
and `ClassMngrRosterTemplatePrintServiceTests`. The application-query CTest
`ClassMngrNextApplicationRosterPrintClassInfoReadQueryTests` and print-service
CTest `ClassMngrRosterTemplatePrintServiceTests` passed. The Platform CTest
`NextPlatformApplicationServicesRosterPrintClassInfoReadPort` reported 8
passed and 1 failed, matching the clean F1382 baseline at commit
`2557098a578ee255aa791d989046de3d482cf6b9`:
`deletedParentSuppressesTypedResultButLegacyReadKeepsOrphanData()` still
expects `E5`, but the detailed record is empty at line 270. Baseline and
post-change outputs match; this pre-existing repository/test mismatch remains
unresolved and was not altered or hidden. `git diff --check` passed. No full
suite or cross-platform build ran. F1383 is accepted; F1384
(Speaking-evaluation roster-name read) is current.

Commit: `e38da4fdcf1fe3468e16b5f7bc15bf709d18ddf9`.

## F1384 — Speaking-evaluation roster-name read (Batch 4 candidate 7 of 20) — 2026-10-24

The production port,
`src/next/platform/application_services_speaking_evaluation_roster_names_read_port.h`,
now uses `WorkspaceStore` for open-state and roster-repository access. Direct
fixture accesses in
`tests/next_platform_application_services_speaking_evaluation_roster_names_read_port_tests.cpp`
also use `WorkspaceStore`; assertions are unchanged. The page fixture was
left unchanged.

Built the application-query, Platform-port, and SpeakingEvalPageSave targets.
Three selected serial CTests passed (3/3): #223
`ClassMngrNextApplicationSpeakingEvaluationRosterNamesReadQueryTests`, #395
`ClassMngrNextPlatformApplicationServicesSpeakingEvaluationRosterNamesReadPortTests`,
and #137 `ClassMngrSpeakingEvalPageSaveTests`. `git diff --check` passed. No
full suite or cross-platform build ran. F1384 is accepted; F1385 (GS Team
directory read) is current.

Commit: `d29a05c176ec2d3e383b15a768d236ffb069d91b`.

## F1385 — GS Team directory read (Batch 4 candidate 8 of 20) — 2026-10-24

The production GS Team directory read port,
`src/next/platform/application_services_gs_team_directory_read_port.h`, and
its dedicated Platform fixture,
`tests/next_platform_application_services_gs_team_directory_read_port_tests.cpp`,
now use `WorkspaceStore`. Assertions are unchanged, including intentional
stored-ID failure coverage for `4294967297`. The save port was untouched.

Built `ClassMngrNextApplicationGsTeamDirectoryReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesGsTeamDirectoryReadPortTests`,
`ClassMngrStaffDirectoryGsTeamReadTests`, and
`ClassMngrStaffDirectoryGsTeamReadParityTests`. Four selected serial CTests
passed (4/4): #353 `ClassMngrNextApplicationGsTeamDirectoryReadQueryTests`,
#421 `ClassMngrNextPlatformApplicationServicesGsTeamDirectoryReadPortTests`,
#130 `ClassMngrStaffDirectoryGsTeamReadTests`, and #131
`ClassMngrStaffDirectoryGsTeamReadParityTests`. `git diff --check` passed. No
full suite or cross-platform build ran. F1385 is accepted; F1386 (GS Team
directory save) is current.

Commit: `f2c150cac390903f3558272dad5d368dd4dd92e7`.

## F1386 — GS Team directory save (Batch 4 candidate 9 of 20) — 2026-10-24

The GS Team save port,
`src/next/platform/application_services_gs_team_directory_save_port.h`, now
uses `WorkspaceStore` for `hasActiveSession()` and `saveGsTeamDirectory()`.
Direct persistence accesses in the dedicated Platform fixture,
`tests/next_platform_application_services_gs_team_directory_save_port_tests.cpp`,
migrated; assertions are unchanged. Qt Unicode name-key behavior and
transaction-rollback assertions are unchanged.

Built `ClassMngrNextApplicationGsTeamDirectorySaveTests`,
`ClassMngrNextPlatformApplicationServicesGsTeamDirectorySavePortTests`,
`ClassMngrStaffDirectoryGsTeamSaveTests`, and
`ClassMngrStaffDirectoryGsTeamSaveParityTests`. Four selected serial CTests
passed (4/4): #352 `ClassMngrNextApplicationGsTeamDirectorySaveTests`, #420
`ClassMngrNextPlatformApplicationServicesGsTeamDirectorySavePortTests`, #132
`ClassMngrStaffDirectoryGsTeamSaveTests`, and #133
`ClassMngrStaffDirectoryGsTeamSaveParityTests`. `git diff --check` passed. No
full suite or cross-platform build ran. F1386 is accepted; F1387 (Native
English teacher directory read) is current.

Commit: `24247bd4e0299ffe638affdb23abf214fdad453b`.

## F1387 — Native English teacher directory read (Batch 4 candidate 10 of 20) — 2026-10-24

The production Native English teacher directory read port,
`src/next/platform/application_services_native_english_teacher_directory_read_port.h`,
and its dedicated Platform fixture,
`tests/next_platform_application_services_native_english_teacher_directory_read_port_tests.cpp`,
now use `WorkspaceStore`. Assertions are unchanged, including intentional
rejection of stored ID `2147483648`. The broad UI fixture was left unchanged.

Built `ClassMngrNextApplicationNativeEnglishTeacherDirectoryReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesNativeEnglishTeacherDirectoryReadPortTests`,
`ClassMngrStaffDirectoryNativeEnglishReadTests`, and
`ClassMngrStaffDirectoryNativeEnglishReadParityTests`. Four selected serial
CTests passed (4/4): #348
`ClassMngrNextApplicationNativeEnglishTeacherDirectoryReadQueryTests`, #416
`ClassMngrNextPlatformApplicationServicesNativeEnglishTeacherDirectoryReadPortTests`,
#127 `ClassMngrStaffDirectoryNativeEnglishReadTests`, and #128
`ClassMngrStaffDirectoryNativeEnglishReadParityTests`. `git diff --check`
passed. No full suite or cross-platform build ran. F1387 is accepted; F1388
(Native English teacher directory save) is current.

Commit: `5afc11adf83d50a9904352b6197d66c29568ac3d`.

## F1388 — Native English teacher directory save (Batch 4 candidate 11 of 20) — 2026-10-24

The production Native English teacher directory save port,
`src/next/platform/application_services_native_english_teacher_directory_save_port.h`,
uses `WorkspaceStore` for `hasActiveSession()` and the save operation. Direct
persistence accesses in its dedicated Platform fixture,
`tests/next_platform_application_services_native_english_teacher_directory_save_port_tests.cpp`,
also use `WorkspaceStore`. Insert/update/delete, stale-row, unavailable-store,
and transactional-rollback assertions are unchanged. The pinned page fixture
was left untouched.

Built the Native English save use-case, Platform-port, and StaffDirectory
save-parity targets. Three selected serial CTests passed (3/3): #351
`ClassMngrNextApplicationNativeEnglishTeacherDirectorySaveTests`, #419
`ClassMngrNextPlatformApplicationServicesNativeEnglishTeacherDirectorySavePortTests`,
and #129 `ClassMngrStaffDirectoryNativeEnglishSaveParityTests`.
`git diff --check` passed. No full suite or cross-platform build ran. F1388
is accepted; F1389 (Korean teacher birthday directory read) is current.

Commit: `ddc645c1aedf2571c6cfb4fdf6927085dd7d2209`.

## F1389 — Korean teacher birthday directory read (Batch 4 candidate 12 of 20) — 2026-10-24

The Korean teacher birthday directory read port,
`src/next/platform/application_services_korean_teacher_birthday_directory_read_port.h`,
and its dedicated Platform fixture,
`tests/next_platform_application_services_korean_teacher_birthday_directory_read_port_tests.cpp`,
now use `WorkspaceStore`. The five raw fields, ordering, and assertions are
unchanged.

Built `ClassMngrNextApplicationKoreanTeacherBirthdayDirectoryReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesKoreanTeacherBirthdayDirectoryReadPortTests`,
and `ClassMngrNavigationTeacherReadTests`. Serial CTests #350
`ClassMngrNextApplicationKoreanTeacherBirthdayDirectoryReadQueryTests` and #418
`ClassMngrNextPlatformApplicationServicesKoreanTeacherBirthdayDirectoryReadPortTests`
passed (2/2). CTest #86 `NavigationTeacherRead` reported 38 passed and 1
failed at
`selectedTeacherProfileReadFailureWarnsWithoutConfirmation()` due to an
unrelated teacher-profile warning-text mismatch. The same failure was
reproduced against isolated clean F1388 source at commit
`ddc645c1aedf2571c6cfb4fdf6927085dd7d2209`; baseline and post-change outputs
match, confirming no regression. This is separate from the F1383 orphan-data
mismatch recorded above. `git diff --check` passed. No full suite or
cross-platform build ran. F1389 is accepted; F1390 (GS Team birthday
directory read) is current.

Commit: `81eb58b97a2e15d8343473df9e606c2d123dba82`.

## F1390 — GS Team birthday directory read (Batch 4 candidate 13 of 20) — 2026-10-24

The GS Team birthday directory read port,
`src/next/platform/application_services_gs_team_birthday_directory_read_port.h`,
and its dedicated Platform fixture,
`tests/next_platform_application_services_gs_team_birthday_directory_read_port_tests.cpp`,
now use `WorkspaceStore`. The four raw fields, ordering, and assertions are
unchanged; no ID projection was added.

Built `ClassMngrNextApplicationGsTeamBirthdayDirectoryReadQueryTests` and
`ClassMngrNextPlatformApplicationServicesGsTeamBirthdayDirectoryReadPortTests`.
Serial CTests #354 `ClassMngrNextApplicationGsTeamBirthdayDirectoryReadQueryTests`
and #422
`ClassMngrNextPlatformApplicationServicesGsTeamBirthdayDirectoryReadPortTests`
passed. CTest #86 `NavigationTeacherRead` again reported 38 passed and 1 failed
at `selectedTeacherProfileReadFailureWarnsWithoutConfirmation()`, matching the
confirmed clean F1388 baseline at commit
`ddc645c1aedf2571c6cfb4fdf6927085dd7d2209`. No additional failures occurred.
This repeats the F1389 warning-text baseline failure and is distinct from the
F1383 orphan-data mismatch recorded above. `git diff --check` passed. No full
suite or cross-platform build ran. F1390 is accepted; F1391 (Native English
birthday directory read) is current.

Commit: `b86d6e01cbc5fc3bca9671a7dea39bbb85c24507`.

## F1391 — Native English birthday directory read (Batch 4 candidate 14 of 20) — 2026-10-24

The Native English birthday directory read port,
`src/next/platform/application_services_native_english_teacher_birthday_directory_read_port.h`,
and its dedicated Platform fixture,
`tests/next_platform_application_services_native_english_teacher_birthday_directory_read_port_tests.cpp`,
now use `WorkspaceStore`. The three raw fields, ordering, and assertions are
unchanged; no ID projection was added.

Built `ClassMngrNextApplicationNativeEnglishTeacherBirthdayDirectoryReadQueryTests`,
`ClassMngrNextPlatformApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPortTests`,
`ClassMngrNavigationTeacherReadTests`, and `ClassMngrUpcomingBirthdaysTests`.
Serial CTests #349
`ClassMngrNextApplicationNativeEnglishTeacherBirthdayDirectoryReadQueryTests`
and #417
`ClassMngrNextPlatformApplicationServicesNativeEnglishTeacherBirthdayDirectoryReadPortTests`
passed; #57 `ClassMngrUpcomingBirthdaysTests` passed. CTest #86
`NavigationTeacherRead` reported 38 passed and 1 failed at
`selectedTeacherProfileReadFailureWarnsWithoutConfirmation()`, matching the
confirmed clean F1388 baseline at commit
`ddc645c1aedf2571c6cfb4fdf6927085dd7d2209`. No additional failures occurred.
This repeats the F1389/F1390 warning-text baseline mismatch and remains
separate from the F1383 orphan-data failure. `git diff --check` passed. No full
suite or cross-platform build ran. F1391 is accepted; F1392 (Current-campus
preferences) is current.

Commit: `33f7fb2ea2e0827609b596a7e688dc4c5a9bedc9`.

## F1392 — Current-campus preferences (Batch 4 candidate 15 of 20) — 2026-10-24

The current-campus preferences port,
`src/next/platform/application_services_current_campus_preferences_port.h`,
and its dedicated Platform fixture,
`tests/next_platform_application_services_current_campus_preferences_port_tests.cpp`,
now use `WorkspaceStore`. Key `myInfo/campus`, byte/QVariant semantics, and
assertions are unchanged. The aggregate `PersonalDetails` writer was untouched.

Built the dedicated Platform, MyWorkspacePage, InitialSetupWizard, SubPrepPage,
and CalendarPageEventMutationParity targets. Four selected serial CTests
passed (4/4): #438, #69, #6, and #80. CTest #147
`ClassMngrSubPrepPageTests` timed out in
`unavailablePreferenceLoadsPreservePageValues()` without an assertion failure.
The same timeout reproduced against isolated clean F1391 source at commit
`33f7fb2ea2e0827609b596a7e688dc4c5a9bedc9`: baseline ran 60.02 seconds before
timing out; the modified run exceeded 3 minutes, and a focused 60-second rerun
also timed out. No causal attribution or regression is claimed; this is a
confirmed pre-existing timeout. It is separate from the F1383 orphan-data and
NavigationTeacherRead warning-text baseline failures. `git diff --check` passed.
No full suite or cross-platform build ran. F1392 is accepted; F1393
(Class-visibility preferences) is current.

Commit: `c620607653939cfdcbf1dfcece46d4c44187d7b6`.

## F1393 — Class-visibility preferences (Batch 4 candidate 16 of 20) — 2026-10-24

The class-visibility preferences port,
`src/next/platform/application_services_class_visibility_preferences_port.h`,
and its dedicated Platform fixture,
`tests/next_platform_application_services_class_visibility_preferences_port_tests.cpp`,
now use `WorkspaceStore`. Defaults, missing-value materialization, parsing,
enum-validation order, error handling, and assertions are unchanged.

Built `ClassMngrNextPlatformApplicationServicesClassVisibilityPreferencesPortTests`
and `ClassMngrMainWindowClassVisibilityPreferencesFailureTests`. Serial CTests
#492 and #113 passed (2/2), including failed-save UI rollback and retry.
`git diff --check` passed. The broad ClassesPage target, full suite, and
cross-platform build were not run. F1393 is accepted; F1394 (Schedule-display
preferences) is current.

Commit: `69b0230414305d3f62dd4a59469bf9c03670bda5`.

## F1394 — Schedule-display preferences (Batch 4 candidate 17 of 20) — 2026-10-24

The Schedule-display preferences read/write adapter and dedicated Platform
fixture now use `WorkspaceStore`. Five keys, defaults, parsing, atomic save,
and assertions are unchanged.

Built the Platform-port, ScheduleWidget, and ScheduleTestingLayoutClearParity
targets. CTests #487 and #46 passed. Schedule parity CTest #98 completed 3/9;
the same six `fixture.databaseReady == false` failures appeared as on clean
F1393 baseline, at lines 747, 783, 832, 924, 1003, and 1208, including schedule
preference recovery cases. No new failure occurred. `git diff --check` passed.
No full suite or cross-platform build ran. F1394 is accepted; F1395
(Schedule-display-mode preferences) is current.

Commit: `8a37be83ef79cd6097dd127e541986973ce43511`.

## F1395 — Schedule-display-mode preferences (Batch 4 candidate 18 of 20) — 2026-10-24

The Schedule-display-mode adapter and dedicated Platform fixture now use
`WorkspaceStore`. Canonical and legacy key parsing, migration and fallback,
unknown-value preservation, save validation and error behavior, and assertions
are unchanged.

Built `ClassMngrNextPlatformApplicationServicesScheduleDisplayModePreferencesPortTests`,
`ClassMngrScheduleWidgetTests`, and `ClassMngrMyWorkspacePageTests`. Serial CTests
#488, #46, and #69 passed (3/3). `git diff --check` passed. No full suite or
cross-platform build ran. F1395 is accepted; F1396 (Calendar first-day-of-week
preferences) is current and is the Batch 5 discovery trigger.

Commit: `ea14fb7bb75862358cc04cc6a650d936a9b5c8da`.

## F1396 — Calendar first-day-of-week preferences (Batch 4 candidate 19 of 20) — 2026-10-24

The calendar first-day-of-week port and dedicated Platform fixture now use
`WorkspaceStore`. Seven-day integer/key behavior, locale fallback,
invalid-value behavior, and assertions are unchanged. Platform CTest #433 and
Calendar preferences restore-defaults CTest #81 passed serially (2/2).
`git diff --check` passed. No full suite or cross-platform build ran. F1396 is
accepted; F1397 (Academic-calendar schedule preferences) is current.

Commit: `555abdf714caf828679ba293e6d778fe689a0025`.

### Batch 5 discovery — triggered at F1396

Two complementary scans completed. The source scan found exactly 19 distinct
active Next Platform ports after excluding the two final Batch 4 slices,
F1396 and F1397. The feature-flow scan cross-checked consumers and separately
surfaced display-name preference, latest-source-date read, and residual
Calendar Event range projection. A focused guard audit found the Speaking Eval,
Teacher Info, and Sidebar direct session checks are incidental behavior guards,
not independent persistence ports; they remain in overflow. The 19 prioritized
slices are listed in `Current-Slice-Batches.md` and were queued until the Batch
4 push receipt below was recorded. Scan registrations are candidates, not test
results.

## F1397 — Academic-calendar schedule preferences (Batch 4 candidate 20 of 20) — 2026-10-24

The academic-calendar schedule preferences port,
`src/next/platform/application_services_academic_calendar_schedule_preferences_port.h`,
and its dedicated Platform fixture,
`tests/next_platform_application_services_academic_calendar_schedule_preferences_port_tests.cpp`,
now use `WorkspaceStore`. The exact key, opaque UTF-8, missing/unavailable,
read/write error, and assertion semantics are unchanged.

Built `ClassMngrNextPlatformApplicationServicesAcademicCalendarSchedulePreferencesPortTests`
and `ClassMngrEvaluationDefaultSelectionIntegrationTests`. Serial CTests #434
and #168 passed (2/2). The older Phase 2 F193 summary conflicts with current
source/test behavior. This slice preserves the existing behavior: unavailable
or closed reads return empty success; writes and repository read errors map to
Technical. No semantics changed. `git diff --check` passed. No full suite or
cross-platform build ran. F1397 is accepted; Batch 4 is complete and pushed.
Batch 5 is active with F1398 (Middle-school analytics preferences) current.

Commit: `544b94cf97ab9fce8bb1e174ba1d9e0c9774d1c3`.

## Batch 4 push receipt — 2026-10-24

The 20 Batch 4 slice commits, F1378–F1397, were pushed from `Qt-Rewrite` to
configured `origin/Qt-Rewrite` with a normal non-force push. The remote
advanced from prior tip `6597875f6036224767fa8db7331eac7eac856869` to Batch 4
implementation tip `544b94cf97ab9fce8bb1e174ba1d9e0c9774d1c3`. Post-push
verification showed local HEAD and `origin/Qt-Rewrite` matched at
`544b94cf97ab9fce8bb1e174ba1d9e0c9774d1c3`; the worktree was clean.

Batch 4 is complete and pushed. Batch 5 discovery is complete and Batch 5 is
active with F1398 (Middle-school analytics preferences) current. Its 19-slice
priority list is in `Current-Slice-Batches.md`.

## F1398 — Middle-school analytics preferences (Batch 5 candidate 1 of 19) — 2026-10-24

The middle-school analytics preferences adapter and dedicated Platform
fixture now use `WorkspaceStore`. The exact key,
`classes_navigation_show_middle_school_analytics_and_evaluations`, false
default and default-materialization behavior, `QVariant::toBool()` coercion,
unavailable/closed-store behavior, Technical error mapping, and all fixture
assertions are unchanged.

An incremental build in `build/macos-clang-debug` succeeded for the Platform,
ClassesPage, and MainWindow failure-test targets. CTest #493, the Platform
target, passed 1/1. The direct QtTest selection
`ClassesPageTests::middleSchoolAnalyticsAndEvaluationsTabsFollowPreference`
passed 3/3 including init/cleanup. Both selected MainWindow slots passed
individually 3/3 including init/cleanup. `git diff --check` passed for source
and test changes. The full ClassesPage suite, full suite, and cross-platform
builds did not run; the previously documented full-suite timeout remains
unassessed. F1398 is accepted; F1399 (Calendar event-type color preferences)
is current. Batch 5 remains unpushed until its remaining slices are complete.

Commit: `4d19087140f1d4723087c6c460afa06f32ae0f37`.

## F1399 — Calendar event-type color preferences (Batch 5 candidate 2 of 19) — 2026-10-24

The Calendar event-type color adapter and dedicated Platform fixture, plus
only the F1399 picker-retry integration slot, now use `WorkspaceStore`. The
six exact keys (`calendar/eventTypeColor/Vacation`,
`calendar/eventTypeColor/Holiday`, `calendar/eventTypeColor/Workshop`,
`calendar/eventTypeColor/CM`, `calendar/eventTypeColor/Meeting`, and
`calendar/eventTypeColor/Other`), unknown-type Other fallback, UTF-8
pass-through, empty read fallbacks, and nonrecoverable Technical write
failures are unchanged. The UI defaults remain Vacation `#4b6f91`, Holiday
`#7a5f9e`, Workshop `#5f7f52`, CM `#9a6b3f`, Meeting `#8a4f5d`, and Other
`#66727a`; picker warning, resync, cancel, and retry behavior and all
assertions are unchanged.

An incremental build in `build/macos-clang-debug` passed for
`ClassMngrNextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests`
and `ClassMngrCalendarPageEventMutationParityTests`. CTest #435,
`ClassMngrNextPlatformApplicationServicesCalendarEventTypeColorPreferencesPortTests`,
passed 1/1. With `QT_QPA_PLATFORM=offscreen`, direct QtTest
`CalendarPageEventMutationParityTests::eventTypeColorPickerFailureResyncsAndRetries`
passed 3/3 including init/cleanup; the successful offscreen run emitted font
and size-hint warnings. The initial direct invocation without offscreen
aborted because no screen was available; this was invocation environment
setup, not a test failure. `git diff --check` passed. The full parity CTest,
full suite, and cross-platform builds did not run. F1399 is accepted; F1400
(Class-day-filter reset policy) is current. Batch 5 remains unpushed until its
remaining slices are complete.

Commit: `d6d20fa4fc804c094bbe304bfd2a90707f111014`.

## F1400 — Class-day-filter reset policy (Batch 5 candidate 3 of 19) — 2026-10-24

Only the class-day-filter reset-policy adapter and dedicated Platform fixture
now use `WorkspaceStore`. The key is
`classes_navigation_day_filter_reset_policy`. Missing values still materialize
`on_application_close`; unsupported stored values fall back without rewrite;
valid values are trimmed and lowercased. Invalid-enum saves are validated
before store availability and return nonrecoverable `InvalidInput`; repository
load errors remain recoverable `Technical`, and store/repository/write save
failures remain nonrecoverable `Technical`. UI fallback/restoration and every
assertion are unchanged.

An incremental build in `build/macos-clang-debug` passed for
`ClassMngrNextPlatformApplicationServicesClassDayFilterResetPolicyPortTests`,
`ClassMngrClassesPageTests`, and
`ClassMngrMainWindowClassVisibilityPreferencesFailureTests`. CTest #490,
`ClassMngrNextPlatformApplicationServicesClassDayFilterResetPolicyPortTests`,
passed 1/1. With `QT_QPA_PLATFORM=offscreen`, direct ClassesPage selections
`dayFiltersResetOnPageLeaveAfterHideAndShow` and
`resetPolicyReadFailuresPreserveStoredPreferencesAndPageState` passed 4/4
including init/cleanup. MainWindow slot
`dayFilterResetPolicyFailureRestoresSelectionAndRetryPersistsOnce` passed
3/3 including init/cleanup using registered CTest #113's environment with
absolute `TMP`, `TEMP`, and `TMPDIR` under the test temp directory and
`QT_QPA_PLATFORM=offscreen`. UI runs emitted database-not-open, offscreen
font/size-hint, and missing QML/icon warnings; assertions passed.
`git diff --check` passed. Full ClassesPage and MainWindow CTests, the full
suite, and cross-platform builds did not run; the historical full ClassesPage
timeout remains unassessed. F1400 is accepted; F1401 (Class-selection reset
policy) is current. Batch 5 remains unpushed until its remaining slices are
complete.

Commit: `a97957249b2dce30c14aacfe7640503a493e209d`.

## F1401 — Class-selection reset policy (Batch 5 candidate 4 of 19) — 2026-10-24

The class-selection reset-policy adapter and dedicated Platform fixture now
use `WorkspaceStore`. The exact key is
`classes_navigation_class_selection_reset_policy`. Missing values still
materialize `on_application_close`; valid-but-unsupported values fall back
without rewrite, and recognized values are trimmed and lowercased. Invalid
enum saves are validated before store availability and return nonrecoverable
`InvalidInput`; repository load errors remain recoverable `Technical`, while
store/repository/write save failures remain nonrecoverable `Technical`. UI
fallback/restoration and all assertions are unchanged. The older Phase 2
summary says unsupported settings fall back but omits the useful detail that
valid-but-unsupported stored values are not rewritten.

An incremental build in `build/macos-clang-debug` passed for
`ClassMngrNextPlatformApplicationServicesClassSelectionResetPolicyPortTests`,
`ClassMngrClassesPageTests`, and
`ClassMngrMainWindowClassVisibilityPreferencesFailureTests`. CTest #491,
`ClassMngrNextPlatformApplicationServicesClassSelectionResetPolicyPortTests`,
passed 1/1. With `QT_QPA_PLATFORM=offscreen`, selected ClassesPage slots
`classSelectionResetOnPageLeaveClearsOnlyClassStateAfterHideAndShow`,
`classSelectionResetOnApplicationCloseRetainsOnlyClassStateAfterHideAndShow`,
and `resetPolicyReadFailuresPreserveStoredPreferencesAndPageState` passed 5/5
including init/cleanup. MainWindow slot
`classSelectionResetPolicyFailureRestoresSelectionAndRetryPersistsOnce`
passed 3/3 including init/cleanup using CTest #113's temp/offscreen
environment. UI runs emitted database, offscreen, and missing QML/icon
warnings; assertions passed. `git diff --check` passed. Full ClassesPage and
MainWindow CTests, the full suite, and cross-platform builds did not run; the
prior ClassesPage timeout remains unassessed. F1401 is accepted; F1402
(Evaluation default policy) is current. Batch 5 remains unpushed until its
remaining slices are complete.

Commit: `d5b01e1eff11fa8c0f076db348e8c523a57a6d78`.

## F1402 — Evaluation default policy (Batch 5 candidate 5 of 19) — 2026-10-24

The evaluation-default adapter and dedicated Platform fixture now use
`WorkspaceStore`. The exact key is
`classes_navigation_evaluation_default_policy`; stored values remain `all`
and `current_or_previous_term`. A missing setting returns `All` and attempts
to persist `all`; failure of that default write is ignored. Valid but
unsupported stored values fall back to `all` without rewrite; recognized
values are trimmed and lowercased. An unavailable store/repository returns
successful `All`, while a repository read failure returns nonrecoverable
`Technical`. Invalid-enum saves are validated before store availability and
return nonrecoverable `InvalidInput`; store, repository, and write save
failures remain nonrecoverable `Technical`. The Phase 2 mapping omitted these
distinctions; current source and tests remain authoritative. Assertions are
unchanged.

An incremental build in `build/macos-clang-debug` passed for
`ClassMngrNextPlatformApplicationServicesEvaluationDefaultPolicyPortTests`,
`ClassMngrClassesPageTests`,
`ClassMngrMainWindowClassVisibilityPreferencesFailureTests`, and
`ClassMngrEvaluationDefaultSelectionIntegrationTests`. CTest #489,
`ClassMngrNextPlatformApplicationServicesEvaluationDefaultPolicyPortTests`,
and CTest #168,
`ClassMngrEvaluationDefaultSelectionIntegrationTests`, passed 2/2. The direct
ClassesPage slot `evaluationDefaultPolicyDefaultsToAllAndPersists` passed 3/3
including init/cleanup. MainWindow slots
`evaluationDefaultPolicyReadFailureShowsWarningAndDisablesControls`,
`evaluationDefaultPolicyFailureRestoresSelectionAndSuccessfulRetryDoesNotRefresh`,
and
`evaluationDefaultPolicyReadFailureAfterSaveFailureRestoresLastKnownSelection`
passed 5/5 including init/cleanup using CTest #113's temp/offscreen
environment. UI runs emitted font, missing QML/icon, offscreen size-hint, and
repeated `myInfo/campus` parameter-count warnings; assertions passed.
`git diff --check` passed. Full ClassesPage and MainWindow CTests, the full
suite, and cross-platform builds did not run; the prior ClassesPage timeout
remains unassessed. F1402 is accepted; F1403 (Calendar event range projection,
residual projection only) is current, with prior F1341 operations excluded.
Batch 5 remains unpushed until its remaining slices are complete.

Commit: `b2c1cf00190871f5913c7b10c685b09156f49cd9`.

## F1403 — Calendar event range projection (Batch 5 candidate 6 of 19) — 2026-10-24

Only the residual
`ApplicationServicesCalendarEventPort::projection(QDate,QDate)` helper in
`src/next/platform/application_services_calendar_event_port.h` and five
direct range-fixture SQL handles in
`tests/next_platform_application_services_calendar_event_port_tests.cpp`
moved to `WorkspaceStore`. F1341 availability/by-ID work and the already
WorkspaceStore-backed `CalendarEventProjectionQuery`/cache path were unchanged;
no production caller to this helper was found. Date validation,
unavailable/repository error mapping, exception handling, projection metadata
validation, the 4,096-event cap, and all assertions are preserved.

An incremental build in `build/macos-clang-debug` passed for
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`,
`ClassMngrCalendarEventCacheTests`, and
`ClassMngrCalendarPageEventMutationParityTests`. CTest #431,
`ClassMngrNextPlatformApplicationServicesCalendarEventPortTests`, and CTest
#23, `ClassMngrCalendarEventCacheTests`, passed 2/2. With
`QT_QPA_PLATFORM=offscreen`, direct CalendarPage slots
`currentMonthUpcomingRowsMatchVisibleCalendarRange` and
`upcomingRangeFailuresOfferRetryAndRecover` passed 4/4 including init/cleanup;
the run emitted a non-failing font warning and a diagnostic transcript.
`git diff --check` passed. The full CalendarPage parity CTest, full suite, and
cross-platform builds did not run. F1403 is accepted; F1404 (Personal
display-name preferences) is current. Batch 5 remains unpushed until its
remaining slices are complete.

Commit: `f2a57d20a116f89f737a866dbb1b21177a9820c1`.

## F1404 — Personal display-name preferences (Batch 5 candidate 7 of 19) — 2026-10-24

The `PersonalDisplayNamePreferencesPort` adapter and dedicated Platform
fixture now use `WorkspaceStore`. The exact key is `myInfo/name`. Raw
UTF-8/whitespace conversion remains unchanged, with no trimming, validation,
bounds, or default. Missing settings and unavailable/closed-store reads return
empty success; writes to unavailable/closed stores remain no-op successes.
Open-store/missing-repository reads return empty success while writes fail
with nonrecoverable `Technical`. Repository read failures log a warning and
return nonrecoverable `Technical`; repository write failures also return
nonrecoverable `Technical`.
Personal Details aggregate save, the legacy repository, and Schedule Import
SQL are separate writers and were left untouched.

Incremental build passed for the Platform, MyWorkspace, InitialSetup,
ScheduleWidget, ScheduleImportDialog, and SubPrepPage targets. CTest #444,
`ClassMngrNextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests`,
passed 1/1. All ten requested consumer slots passed directly with offscreen
and absolute `TMP`, `TEMP`, and `TMPDIR` paths: MyWorkspace's
`storedDisplayNamePrefillsWithUtf8AndWhitespace`,
`displayNameReadFailurePreservesDraftAndBlocksSaveUntilReload`, and
`missingAndUnavailableDisplayNamePrefillEmpty`; InitialSetup's
`storedDisplayNamePreservesUtf8AndWhitespace` and
`missingAndUnavailableDisplayNameStayEmpty`; ScheduleWidget's
`outputReadsDisplayNameAfterDialogAcceptanceWithoutTrimming` and
`outputUsesEmptyNameWhenPreferencesAreUnavailableOrServicesAreNull`;
ScheduleImport's `mismatchedProfileRequiresConfirmation`; and SubPrep's
`printDialogRequiresAndSavesMissingUserName` and
`printDialogAcceptsWhenRememberedNameSaveFails`. No setup reruns occurred.
The runs emitted font-alias and `propagateSizeHints` warnings only.
`git diff --check` passed. Full consumer suites, the full suite, and
cross-platform builds did not run. F1404 is accepted; F1405 (Personal details
save) is current. Batch 5 remains unpushed until its remaining slices are
complete.

Commit: `a47c6d6a6fd54581a7fb2fc7f0a989b85ae751b5`.

## F1405 — Personal details save (Batch 5 candidate 8 of 19) — 2026-10-24

The aggregate `PersonalDetailsSavePort` adapter and dedicated Platform
fixture now use `WorkspaceStore`; the write remains one transactional
`saveSettings()` call over all nine exact keys: `myInfo/name`,
`myInfo/campus`, `myInfo/zoomLoginId`, `myInfo/zoomPassword`,
`myInfo/zoomNotAvailable`, `myInfo/signatureImage`, `myInfo/signatureMode`,
`myInfo/typedSignatureText`, and `myInfo/typedSignatureFont`. The port does
not trim or validate strings. Name, Zoom text, and typed signature text retain
length-aware UTF-8 conversion; campus preserves embedded NUL bytes; Zoom
availability remains a boolean. Empty or unpreparable signature image input
stores an empty string; prepared images are Base64-encoded. Signature mode
mapping stays Draw/Type → `0`/`1`. Fonts map JustAnotherHand, DancingScript,
GreatVibes, and Caveat to `0`–`3`, respectively; unsupported values map to `0`.
Failures remain nonrecoverable `Technical`.

MyWorkspace passes name and campus as entered, preserving its draft on
failure and allowing retry. Initial Setup trims the name before the aggregate
request and remains on its page after failure; font-value preservation,
aggregate rollback, and typed-read composition remain covered. The F1404
`myInfo/name` interoperability check passed. Separate Schedule Import, legacy
repository, and other adapter/writer paths were left untouched.

Incremental build passed for
`ClassMngrNextPlatformApplicationServicesPersonalDetailsSavePortTests`,
`ClassMngrNextPlatformApplicationServicesPersonalDisplayNamePreferencesPortTests`,
`ClassMngrMyWorkspacePageTests`, and `ClassMngrInitialSetupWizardTests`. CTest
#445, the F1405 Platform target, and CTest #444, the F1404 Platform target,
passed 2/2. Six selected UI slots passed individually 3/3 including
init/cleanup with `QT_QPA_PLATFORM=offscreen` and absolute
`TMP`, `TEMP`, and `TMPDIR` under
`build/macos-clang-debug/f1405-selected-ui-tmp`: MyWorkspace's
`aggregateSavePersistsAllPersonalDetailsKeys`,
`aggregateSaveFailureRollsBackAndPreservesUnrelatedSettings`, and
`aggregateSaveNoninteractiveFailureShowsStatusAndAllowsRetry`; Initial
Setup's `aggregateSetupSavePreservesAllFourSignatureFontValues`,
`aggregateSaveFailureLeavesAllPersonalDetailsUnchanged`, and
`aggregateSaveComposesTypedReadsWithoutDataLoss`. Warnings were limited to
MyWorkspace font/`propagateSizeHints` and Initial Setup font messages. No setup
reruns occurred. `git diff --check` passed. Full MyWorkspace and Initial Setup
suites, the full suite, and cross-platform builds did not run. F1405 is
accepted; F1406 (Personal signature preferences) is current. Batch 5 remains
unpushed until its remaining slices are complete.

Commit: `9fff0ca5b96d6eabe7b2e22692f9fe6a89205edc`.

## F1406 — Personal signature preferences (Batch 5 candidate 9 of 19) — 2026-10-24

Only the `PersonalSignaturePreferencesPort` adapter and dedicated Platform
fixture moved to `WorkspaceStore`. Reads remain sequential in this exact key
order: `myInfo/signatureMode`, `myInfo/typedSignatureFont`, then
`myInfo/typedSignatureText`. Missing values default to Image, empty text, and
JustAnotherHand, with no writes. Stored mode `1` maps to Type; other values
map to Image. Font values `0`–`3` map to JustAnotherHand, DancingScript,
GreatVibes, and Caveat; invalid, noninteger, or unsupported values map to
JustAnotherHand. Text conversion remains UTF-8 and preserves whitespace.

Unavailable/closed stores or a missing repository return nonrecoverable
`Technical`. Repository read failures are warning-logged at the first failing
key and return nonrecoverable `Technical`; loading stops there, with no
partial bundle or transaction. The F1405 cross-writer interoperability test
remains covered. F1407's `myInfo/signatureImage` adapter and behavior are out
of scope.

Incremental build passed for the F1406/F1405 Platform, MyWorkspace, and
InitialSetup targets. CTest #447,
`ClassMngrNextPlatformApplicationServicesPersonalSignaturePreferencesPortTests`,
and CTest #445,
`ClassMngrNextPlatformApplicationServicesPersonalDetailsSavePortTests`,
passed 2/2. Four selected UI slots passed
individually 3/3 including init/cleanup with
`QT_QPA_PLATFORM=offscreen` and absolute `TMP`, `TEMP`, and `TMPDIR` under
`build/macos-clang-debug/f1406-selected-ui-tmp`: MyWorkspace's
`storedSignaturePreferencesPopulateModeTextAndFont` and
`signaturePreferenceReadFailurePreservesControlsAndBlocksSaveUntilReload`;
Initial Setup's `signaturePreferenceReadFailureBlocksAggregateSaveAndCanRetry`
and `aggregateSetupSavePreservesAllFourSignatureFontValues`. Only font-alias
and MyWorkspace `propagateSizeHints` warnings occurred. No setup reruns
occurred. `git diff --check` passed. Full UI suites, the full suite, and
cross-platform builds did not run; F1407 was not tested. F1406 is accepted;
F1407 (Personal signature image) is current. Batch 5 remains unpushed until
its remaining slices are complete.

Commit: `62981ee18af676b405ba1cb4b5e1bdbbac74e5b7`.

## F1407 — Personal signature image (Batch 5 candidate 10 of 19) — 2026-10-24

Only the `ApplicationServicesPersonalSignatureImagePort` adapter and its
dedicated Platform fixture moved to `WorkspaceStore`. The adapter still reads
the exact `myInfo/signatureImage` key, Base64-decodes it, and prepares the
image through `SignatureImage`. Missing, unavailable, corrupt, unsupported,
or empty images remain successful empty results; repository read failures
remain warning-logged nonrecoverable `Technical` results. F1405's aggregate
writer still uses the same Base64 key/form. F1406's mode, typed text, and font
preferences are out of scope.

The incremental build passed for
`ClassMngrNextPlatformApplicationServicesPersonalSignatureImagePortTests`,
`ClassMngrNextPlatformApplicationServicesPersonalDetailsSavePortTests`,
`ClassMngrMyWorkspacePageTests`, `ClassMngrInitialSetupWizardTests`, and
`ClassMngrSignatureImageProcessorTests`, with no compiler warnings or errors.
CTest #181 (`ClassMngrSignatureImageProcessorTests`), #445
(`ClassMngrNextPlatformApplicationServicesPersonalDetailsSavePortTests`), and
#446
(`ClassMngrNextPlatformApplicationServicesPersonalSignatureImagePortTests`)
passed 3/3. With `QT_QPA_PLATFORM=offscreen`, these MyWorkspace slots passed
3/3: `storedSignatureImageIsPreparedForPreview`,
`missingCorruptAndUnavailableSignatureImagesStayEmpty`, and
`signatureImageReadFailurePreservesImageAndBlocksSaveUntilReload`. These
Initial Setup slots passed 3/3:
`storedSignatureImagePreviewsAndSurvivesSaveWithoutReplacement`,
`missingInvalidAndUnavailableSignatureImagesStayEmpty`, and
`signatureImageReadFailureBlocksAggregateSaveAndCanRetry`. Qt
`propagateSizeHints` and Sans Serif font-alias timing warnings were non-fatal;
no setup reruns occurred. Full UI suites, the full suite, and cross-platform
builds did not run. F1407 is accepted; F1408 (Sub Prep preferences) is
current. Batch 5 remains unpushed until its remaining slices are complete.

Commit: `67474823037b3f8841f3403d9287e08e5965c0c7`.

## F1408 — Sub Prep preferences (Batch 5 candidate 11 of 19) — 2026-10-24

Only the `ApplicationServicesSubPrepPreferencesPort` header/implementation
and dedicated Platform fixture moved to `WorkspaceStore`. Reads remain
sequential in this exact order: `subPrep/classMaterials`,
`subPrep/bookReportGrading`, `subPrep/bookReportSpecialInstructions`, then
`subPrep/subComments`; UTF-8 bytes remain preserved. Missing required values
become empty, missing optional values become `nullopt`, and present-empty
optional values remain present. The first repository read failure is logged
and returns nonrecoverable `Technical` without a partial bundle. An open
store with no repository still loads defaults and warns, while save with no
repository returns `Technical`. Save remains one atomic four-setting
repository call. F1409 Zoom keys are unchanged. F1392's current-campus port
remains separate. Page defaults remain page-owned: it supplies the grading
default when grading is absent, and the special-instructions default only
when grading is also absent.

Both `ClassMngrNextPlatformApplicationServicesSubPrepPreferencesPortTests`
and `ClassMngrSubPrepPageTests` built incrementally without compiler warnings
or errors. CTest #448, the dedicated Platform target, passed 9/9. CTest #147
for the page was interrupted after 147.28 seconds: its first 16 slots passed,
then `unavailablePreferenceLoadsPreservePageValues` had run for 145.482
seconds. It was stopped as the known F1392 baseline timeout; this was not a
natural timeout result, and that slot remains unverified. The focused UI run
passed `freshAndExistingGradingSettingsResolveWithoutDataLoss`,
`unavailablePreferenceSaveHasNoSideEffects`,
`failedPreferenceSaveReportsOnlyExplicitFailureAndPreservesDraft`, and
`repositoryReadFailureBlocksWritesUntilSuccessfulReload`; the last three safe
slots were also rerun after correction and passed. Qt offscreen size/raise and
font-alias warnings and the expected injected F1079 read-failure error were
observed. Full suites and cross-platform builds did not run. F1408 is
accepted; F1409 (Sub Prep personal Zoom preferences) is current. Batch 5
remains unpushed until its remaining slices are complete.

Commit: `d8cb586497ce64ca700f1f4deb500bdc1d8375e0`.

## F1409 — Sub Prep personal Zoom preferences (Batch 5 candidate 12 of 19) — 2026-10-24

Only the `ApplicationServicesSubPrepPersonalZoomPreferencesPort`
header/implementation and dedicated Platform fixture moved to
`WorkspaceStore`. Reads remain sequential by credential: primary
`myInfo/zoomLoginId`, then fallback `subPrep/personalZoomEmail` only if the
primary is missing or invalid; primary `myInfo/zoomPassword`, then fallback
`subPrep/personalZoomPassword`; finally primary `myInfo/zoomNotAvailable`,
then fallback `subPrep/personalZoomNotAvailable`. A valid primary value,
including valid empty, wins. Credentials preserve UTF-8 bytes and embedded
NULs without trimming; defaults remain `N/A`, `N/A`, and `true`.

Read failures are logged and return nonrecoverable `Technical`. A primary
technical read failure does not trigger fallback, and any read failure
prevents migration. Legacy values migrate to primary keys only after all
reads succeed; each migration write is best-effort and save failures are
ignored so legacy values remain available. F1405 shares the primary keys but
its aggregate writer was unchanged. F1408 content keys and F1392
current-campus preferences are separate.

Built `ClassMngrNextPlatformApplicationServicesSubPrepPersonalZoomPreferencesPortTests`,
`ClassMngrNextPlatformApplicationServicesPersonalDetailsSavePortTests`, and
`ClassMngrSubPrepPageTests` incrementally without compiler warnings or
errors. CTest #449, the F1409 port, passed 11 QtTest cases; CTest #445, the
F1405 save-port regression, passed 9 cases. Two selected offscreen Sub Prep
UI slots passed, 4 cases including init/cleanup:
`zoomUnavailableHidesStoredCredentials` and
`zoomReadFailurePreservesReadOnlyFieldsAndReportsDiagnostic`. One Qt
font-alias timing warning occurred; no setup retries were needed. Full Sub
Prep page CTest #147 was not run for this slice because
`unavailablePreferenceLoadsPreservePageValues` is separately attributed to
the clean F1391 baseline in the F1392 record. The slot remains unverified;
this was not a new F1409 test failure. No full suite or cross-platform build
ran. `git diff --check` passed before commit. F1409 is accepted; F1410
(Custom color palette preferences) is current. Batch 5 remains unpushed until
its remaining slices are complete.

Commit: `d2062fac0c3bfb5148d429e0a19bb4c4b258c3c9`.

## F1410 — Custom color palette preferences (Batch 5 candidate 13 of 19) — 2026-10-24

Only the `ApplicationServicesCustomColorPalettePreferencesPort`
header/implementation and direct Platform fixture moved to `WorkspaceStore`.
The `custom_colors` key and 16 default palette entries/order remain unchanged:
`#F94144`, `#F3722C`, `#F8961E`, `#F9C74F`, `#90BE6D`, `#43AA8B`, `#577590`,
`#277DA1`, `#9B5DE5`, `#F15BB5`, `#00BBF9`, `#00F5D4`, `#FFFFFF`, `#D9D9D9`,
`#808080`, and `#000000`.

Null/closed-store reads and missing/invalid stored values return defaults;
null/closed-store writes succeed without saving. For open stores, missing
repositories and read/write failures are logged nonrecoverable `Technical`
results. Writes save all 16 supplied strings exactly as compact JSON. Reads
accept `QStringList`, JSON arrays, and legacy newline, semicolon, or comma
formats. Each of the first 16 entries is trimmed, parsed through `QColor`,
and canonicalized to `HexRgb`; missing/invalid entries default by index and
extras are ignored. ColorUtils, callers, F1399 event-color preferences,
F1408/F1409, and other code were unchanged.

Built `ClassMngrNextPlatformApplicationServicesCustomColorPalettePreferencesPortTests`,
`ClassMngrColorUtilsCustomColorPaletteTests`, and
`ClassMngrInitialSetupWizardClassCreateSaveParityTests` incrementally without
compiler warnings or errors. Offscreen CTest #436, the palette adapter, passed
12 QtTest cases; #437, ColorUtils, passed 11; and #76, Initial Setup parity,
passed 9 including class-color dialog integration. Qt font-alias timing and
`propagateSizeHints` warnings were non-fatal; no setup reruns occurred. Full
suite and cross-platform builds did not run. `git diff --check` passed before
commit. F1410 is accepted; F1411 (Initial Setup teacher choices read) is
current. Batch 5 remains unpushed until its remaining slices are complete.

Commit: `5030fdbfa6618e0a42afd578a3f704566cd360fa`.

## F1411 — Initial Setup teacher choices read (Batch 5 candidate 14 of 19) — 2026-10-24

Only the `ApplicationServicesInitialSetupTeacherChoicesReadPort` header and
its direct Platform fixture moved to `WorkspaceStore`. The port retains its
`ApplicationServices*` and resolves `workspaceStore()` dynamically for each
read. TeacherRepository query/order and all returned fields remain unchanged:
ID, Korean/English names, preferred romanization/name, and UTF-16 text.
Unavailable store/repository returns recoverable `NotFound`; repository
errors or exceptions return nonrecoverable `Technical`; invalid ID returns
nonrecoverable `Validation`. Cursor failure returns no partial results.
F1412 teacher create and F1413 teacher delete remain out of scope.

Built `ClassMngrNextPlatformApplicationServicesInitialSetupTeacherChoicesReadPortTests`,
`ClassMngrNextApplicationInitialSetupTeacherChoicesReadQueryTests`, and
`ClassMngrInitialSetupWizardTests` incrementally without compiler warnings
or errors. CTest #471, the Platform port, passed 8 QtTest cases; #469, the
Application query, passed 6. Six selected offscreen wizard slots passed, 8
cases including init/cleanup: `classDetailsShowsOrderedPreferredTeacherNamesAndSelectsSingleTeacher`,
`classDetailsShowsTeacherReadFailureAndLeavesChoicesEmpty`,
`personalDetailsRoutesToClassDetailsWhenTeachersExist`,
`personalDetailsRoutesToTeacherEntryWhenNoTeachersExist`,
`blankTeacherEntrySkipsWhenTeachersExist`, and
`teacherChoiceReadFailuresBehaveAsEmptyWithoutWarnings`. One Sans Serif
font-alias timing warning occurred; no retries or hangs occurred. Full
Initial Setup CTest and broader suite did not run. `git diff --check` passed
before commit. F1411 is accepted; F1412 (Teacher create) is current. Batch 5
remains unpushed until its remaining slices are complete.

Commit: `4b591e7cfe916b32982ece2877a22f0300fbc2e7`.

## F1412 — Teacher create (Batch 5 candidate 15 of 19) — 2026-10-24

The `ApplicationServicesTeacherCreatePort` header now resolves
`WorkspaceStore` dynamically during both preflight and create. Its direct
Platform fixture and the dedicated
`InitialSetupWizardTeacherCreateParityTests` fixture also use
`WorkspaceStore`. Validation preflight order, post-validation repository
recheck, error categories/messages, `TeacherValidator` normalization, all 14
field mappings, and `createTeacherWithWideId()` 64-bit behavior remain
unchanged. F1411 teacher read and F1413 teacher delete remain separate.

Built `ClassMngrNextPlatformApplicationServicesTeacherCreatePortTests`,
`ClassMngrNextApplicationTeacherCreateUseCaseTests`,
`ClassMngrInitialSetupWizardTeacherCreateParityTests`, and
`ClassMngrMainWindowTeacherSidebarNavigationParityTests` incrementally
without compiler warnings or errors. CTest #480, the Platform port, passed 8
QtTest cases; #460, the Application use case, passed 6. CTest #77,
`ClassMngrInitialSetupWizardTeacherCreateParityTests`, aborted after init
passed as `identityValidationKeepsFieldsAndPromptOrder` began. It raised an
uncaught macOS `NSInvalidArgumentException`: `NSBundle initWithURL: nil URL
argument`, with the stack through Qt `QWizard::page()/pixmap()` from
`WizardFixture::open()`. This was before a behavior assertion; the cause is
undetermined. No retry occurred. Remaining UI parity cases, including
failed-write retry/field retention and Add Another, were not observed. The
direct offscreen Sidebar slot
`addTeacherActionPreservesRequiredNameValidationOutcome` passed, 3 cases
including init/cleanup. Font-alias timing warnings occurred in parity and
Sidebar runs; no setup retries occurred. Full suite did not run.
`git diff --check` passed before commit. F1412 is accepted with this parity-
test limitation; F1413 (Teacher delete) is current. Batch 5 remains unpushed
until its remaining slices are complete.

Commit: `318affc4a7f651e29ae7b7b909588739891476cd`.

## F1413 — Teacher delete (Batch 5 candidate 16 of 19) — 2026-10-24

Only the `ApplicationServicesTeacherDeletePort` header, its direct Platform
fixture, and the Sidebar delete parity fixture moved to `WorkspaceStore`.
Canonical positive `qint64` IDs, dynamic store lookup, request/error
handling, and `TeacherRepository::deleteTeacher(qint64)` remain unchanged.
The repository transaction clears assignments for the target teacher, then
deletes that teacher; failures roll back, preserving classes, schedules, and
sibling teachers. Deleting a nonexistent canonical positive ID remains a
no-op success. F1411 teacher read and F1412 teacher create remain separate.

Built `ClassMngrNextPlatformApplicationServicesTeacherDeletePortTests`,
`ClassMngrNextApplicationTeacherDeleteUseCaseTests`, and
`ClassMngrSidebarTeacherDeleteParityTests` incrementally without compiler
warnings or errors. CTest #479, the Platform port, passed 6 QtTest cases;
#459, the Application use case, passed 6; and #97, Sidebar parity, passed 5.
Sidebar emitted one font-alias timing warning and three missing
`:/assets/icons/keyboard_light.svg` resource warnings; they were non-fatal and
no retries occurred. Broader suites and cross-platform builds did not run.
`git diff --check` passed before commit. F1413 is accepted; F1414 (Teacher
Import latest-source-date read) is current. Batch 5 remains unpushed until
its remaining slices are complete.

Commit: `6f0fa59fae98b51f59b4cd27a99452f3830c94f1`.

## F1414 — Teacher Import latest-source-date read (Batch 5 candidate 17 of 19) — 2026-10-24

Migrated the latest-source-date Platform adapter and direct fixture from
`DatabaseSession` to the dynamic `ApplicationServices::workspaceStore()` and
its `SettingsRepository`. `teacher_import/latest_source_date` is a stored
Gregorian calendar date in canonical `YYYY-MM-DD`, with no timezone
conversion. Missing, null, empty, malformed, noncanonical, and impossible
values remain successful absence. A null, closed, or unavailable store maps
to recoverable `NotFound`; repository failures and exceptions map to
nonrecoverable `Technical`. F1349 apply/snapshot/transaction behavior and the
UI caller are untouched.

The incremental macOS Debug build of `ClassMngr` and the focused Platform
test target succeeded. The Platform port and Application query CTests passed
**2/2** (1.02 s), with no warnings, failures, or retries. `git diff --check`
was clean. The full suite and cross-platform builds were not run.

```sh
cmake --build build/macos-clang-debug --target ClassMngr ClassMngrNextPlatformApplicationServicesTeacherImportLatestSourceDateReadPortTests
ctest --test-dir build/macos-clang-debug --output-on-failure -R '^(ClassMngrNextPlatformApplicationServicesTeacherImportLatestSourceDateReadPortTests|ClassMngrNextApplicationTeacherImportLatestSourceDateReadQueryTests)$'
```

Commit: `0dd7f9ec`.

## F1415 — Schedule Import current-state snapshot (Batch 5 candidate 18 of 19) — 2026-10-24

Migrated the Schedule Import current-state snapshot adapter to dynamically
resolve `ApplicationServices::workspaceStore()` on each read and use its
`ClassRepository`, `TeacherRepository`, and `ClassInfoRepository`. Direct
Platform fixture persistence now uses `WorkspaceStore`. Snapshot semantics
remain unchanged: all teachers, all non-testing classes, regular and intensive
schedules, existing ordering and defaults, canonical IDs including wide
teacher IDs and the `-1` unassigned assignment, preserved text, and existing
failures. No UI, repository, or application code changed.

The incremental macOS Debug build in `build/macos-clang-debug` passed for
`ClassMngr` and
`ClassMngrNextPlatformApplicationServicesScheduleImportStateSnapshotPortTests`.
CTest #329 `ClassMngrNextApplicationScheduleImportStateSnapshotTests` and
CTest #485
`ClassMngrNextPlatformApplicationServicesScheduleImportStateSnapshotPortTests`
passed 2/2, with no warnings or retries. The CTest log is
`/private/tmp/F1415-VERIFY-01-ctest.log`; `git diff --check` passed. No full
suite, UI suite, or cross-platform build ran.

Commit: `e78f2056`.

## F1416 — Schedule Import apply (Batch 5 candidate 19 of 19) — 2026-10-24

Migrated the Schedule Import apply adapter to resolve
`ApplicationServices::workspaceStore()` and its `ScheduleImportRepository`
on each apply call. Direct Platform fixture persistence now uses
`WorkspaceStore`. ApplyUseCase, repository, and UI behavior are unchanged.
Existing typed validation, qint64 teacher/int32 class IDs,
repository transaction/result mapping, the `No Teacher Profile is open.`
failure, and the legacy `ScheduleService::importSchedule` comparison remain
unchanged.

The incremental macOS Debug build in `build/macos-clang-debug` passed for
`ClassMngrNextPlatformApplicationServicesScheduleImportApplyPortTests`.
CTest #333 `ClassMngrNextApplicationScheduleImportApplyUseCaseTests` and
CTest #486
`ClassMngrNextPlatformApplicationServicesScheduleImportApplyPortTests`
passed 2/2 with no warnings or retries; the Platform QtTest had 9 passed.
The CTest log is
`build/macos-clang-debug/Testing/Temporary/LastTest.log`;
`git diff --check` passed. No broader suite or cross-platform build ran.

Batch 5 is complete locally (19/19 accepted) at implementation tip
`61d1fc9f`; it remains unpushed. Batch 6 discovery found no prioritized
candidates: `No other slices were found.` Broader unranked
`databaseSession()`/`DataService` paths remain for future discovery; this does
not exhaust the Phase 3 backlog.
