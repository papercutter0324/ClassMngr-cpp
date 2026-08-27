# Phase 0 database compatibility fixture contract

ClassMngr currently treats `.tps` as the preferred extension and `.db` as a
legacy extension (`src/core/database_file_format.cpp`). Both are SQLite-backed
profiles; the on-disk contract is schema version 6 at source baseline
`48fc5c5` (`DatabaseSchemaManager::LatestSchemaVersion`). A Windows-native UI
may not change that contract.

## Required checked-in corpus

Place deterministic, non-production fixtures under
`tests/fixtures/database-port/`. The checked-in
[`manifest.json`](../../../../tests/fixtures/database-port/manifest.json)
names every required input, its SHA-256, and the initial semantic oracle. Build
and run `ClassMngrDatabasePortFixtureGenerator` with
`--output-directory tests/fixtures/database-port` to materialize a missing
corpus; it refuses to overwrite an existing fixture. The `semantic` object
contains only stable assertions—schema version, key row counts/values, expected
migration backups, and outcomes—and no file paths, machine identifiers, or
sensitive values. Later cross-platform runs may emit richer
`<name>.semantic.json` result digests, but those are not baseline inputs.

Validate the generated inputs before committing them:

```powershell
.\build\windows-x64-debug\Debug\ClassMngrDatabasePortFixtureGenerator.exe `
  --verify-directory .\tests\fixtures\database-port
```

The registered `ClassMngrDatabasePortFixtureTests` runs the same verifier. It
opens each input only in a temporary copy, checks the current-profile semantic
values (including Korean text and roster sizes), verifies v2/v5 migration and
backup behavior, and confirms failure cases remain unchanged except for the
documented rollback v3/data-preservation outcome.

| Fixture | Input | Required scenario | Oracle |
| --- | --- | --- | --- |
| `empty.tps` | new/empty database | create and open, then initialize schema | v6 schema, foreign keys enabled, empty semantic digest |
| `typical.tps` | current profile | read/write teacher, class, schedule, roster, calendar, campus, and speaking-evaluation data including Korean | semantic round trip after Windows write and Linux Qt write |
| `large.tps` | current profile | virtualized roster/evaluation/schedule reads and save | fixed row counts, digest, latency/memory samples |
| `legacy-v2.db` | legacy profile | migrate v2 through v6 and repair the historical unassigned-teacher value | v6 digest plus `.pre-schema-v4-backup` |
| `legacy-v5.db` | profile at v5 | migrate row-index constraint change | v6 digest plus `.pre-schema-v6-backup` |
| `migration-invalid.db` | invalid legacy data | reject preflight without advancing `user_version` | error class/text fragment and unchanged digest |
| `migration-rollback.db` | forced table-rebuild collision | roll back partial v4 rebuild | v3 version and unchanged legacy table/data digest |
| `newer-schema.tps` | `user_version > 6` | reject unsupported future schema | error class/text fragment and no mutation |
| `corrupt.tps` | malformed/non-SQLite bytes | fail open safely | error category, no output/backup mutation |

Fixtures are portable SQLite bytes: do not compare raw database bytes after a
write because SQLite page layout, journal state, and timestamps may vary. The
semantic digest is authoritative, except that the corrupt fixture is compared
byte-for-byte. Rejection fixtures also compare their input bytes unchanged,
except `migration-rollback.db`: it is expected to preserve the legacy table
data while recording schema v3 before reporting the migration-4 failure.

## Existing test evidence to preserve while materializing the corpus

| Contract area | Existing test evidence |
| --- | --- |
| extension/path policy | `tests/database_file_format_tests.cpp` |
| latest schema, foreign keys, v2 migration, v5 migration, invalid data, partial migration rollback, future schema | `tests/database_schema_manager_tests.cpp` |
| database session lifecycle, save/export and write rollback | `tests/data_service_lifecycle_tests.cpp` |
| calendar/repository persistence | `tests/calendar_event_*_tests.cpp` |
| class transfers and roster persistence | `tests/class_transfer_tests.cpp`, `tests/roster_*_tests.cpp` |
| feature imports | `tests/calendar_import_tests.cpp`, `tests/schedule_import_tests.cpp`, `tests/teacher_import_tests.cpp` |

The initial Phase 0 source pass found that these tests construct their own
temporary inputs rather than consuming one shared corpus. The generator and
manifest now make the first common corpus reproducible and pin its initial
semantic expectations. Phase 0 completion still requires running the same
committed bytes through Linux Qt and the Windows-native engine, recording a
semantic result digest for each implementation.

## Cross-platform execution rules

1. Copy the source fixture to a fresh temp directory for every run.
2. Open/migrate with implementation A, verify the semantic digest, then open
   the same copy with implementation B and verify again. Run both directions.
3. For a successful write, compare semantic digests and SQLite integrity/foreign
   key checks. For failure cases, compare unchanged input bytes and expected
   backups as specified in the manifest, except for the explicit
   `migration-rollback.db` v3/data-preservation outcome.
4. Record implementation ID, platform, architecture, schema version, and
   fixture SHA-256 with the result.
5. Never put a teacher profile or personally identifiable student data in this
   corpus.
