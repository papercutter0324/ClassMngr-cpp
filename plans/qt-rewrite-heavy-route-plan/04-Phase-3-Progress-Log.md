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
