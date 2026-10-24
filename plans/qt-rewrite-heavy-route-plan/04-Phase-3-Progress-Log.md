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
suite or cross-platform build ran. F1377 is accepted; Batch 3 is complete;
F1378 (My Classes teacher-profile batch read) is current in Batch 4. Batch 3
has not been pushed.
