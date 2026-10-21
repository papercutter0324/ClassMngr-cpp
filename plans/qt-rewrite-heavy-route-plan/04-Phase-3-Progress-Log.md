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
