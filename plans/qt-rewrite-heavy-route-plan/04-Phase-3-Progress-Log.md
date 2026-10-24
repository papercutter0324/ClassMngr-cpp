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
