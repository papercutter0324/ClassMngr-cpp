# Phase 2 local validation record

Date: 2026-08-30 (Asia/Seoul)

Scope: Phase 2 portable-engine slices on the clean Phase 1 working tree.

## Extracted contract

`classmngr::engine::DatabaseFileFormat` now owns the `.tps`/legacy `.db`
path rules using UTF-8 `std::string_view` inputs and `std::string` outputs.
The retained Qt `DatabaseFileFormat` namespace is a conversion adapter only;
it no longer contains the extension, case, or suffix rules.

The Qt-free `ClassMngrEngine` target includes the contract and has a dedicated
headless test executable covering native and legacy paths, case-insensitive
extensions, UTF-8 names, directory names containing dots, blank paths, and
extensionless inputs.

`classmngr::engine::Error` and `Result<T>` now provide typed error codes,
UTF-8 messages, and optional native codes for persistence-facing APIs.
`classmngr::engine::SqliteDatabase` keeps SQLite handles private and provides
prepared parameter binding, typed integer/real/text/blob/null rows, a
configurable `std::chrono` busy timeout, foreign-key initialization, schema
version primitives, and RAII transaction rollback/commit. Windows links the
architecture-matched Windows SDK `winsqlite3` library; non-Windows builds use
the CMake SQLite3 package/target.

`classmngr::engine::DatabaseSchemaManager` now owns the six-version schema
sequence: initial tables, legacy columns, legacy-data preflight, constrained
foreign-key tables, indexes, and valid persisted row indexes. It preserves
transaction rollback and file-backed pre-constraint backups. `OpenDatabase`
normalizes UTF-8 filesystem paths, creates missing parent directories, opens
SQLite, and migrates the schema before returning the handle.

The first product CRUD slice is Qt-free `Classroom` plus `ClassRepository`.
It covers UTF-8 create/list/get/rename/delete behavior, testing-class
filtering, cascade cleanup, and typed invalid/not-found errors.

The next product slice is Qt-free `Teacher` plus `TeacherValidator` and the
validated `TeacherService` use-case boundary. It preserves the existing
English/Korean name normalization rules, preferred-name derivation, Korean
phone formatting, birthday and enum validation, UTF-8 persistence, typed
errors, and transactional cleanup of `class_info.teacher_id` on deletion.

The teacher-directory slice now also includes Qt-free
`NativeEnglishTeacherService` and `GsTeamService` boundaries. They preserve
the existing directory normalization, position-priority ordering,
case-insensitive duplicate-name rules, parameterized upserts/deletes, and
atomic save behavior for the native-English and GS-team tables.

The class-information slice now includes Qt-free `ClassTime` and `ClassInfo`
models, the canonical grade/level/book configuration, schedule and class-info
validators, and a validated `ClassInfoService` boundary. It preserves
case-insensitive choice normalization, strict 24-hour and 12-hour schedule
parsing, duplicate-slot and interval-order diagnostics, color and note-length
rules, teacher-join read fields, empty-class defaults, and one transaction for
the class row plus regular and intensive time rows.

## Local validation

| Lane | Result |
| --- | --- |
| Windows x64 Debug engine tests | Passed: `ClassMngrEngineTests` and `ClassMngrEngineDatabaseFileFormatTests` |
| Windows x64 Release engine tests | Passed: `ClassMngrEngineTests` and `ClassMngrEngineDatabaseFileFormatTests` |
| Windows x86 Debug engine tests | Passed: `ClassMngrEngineTests` and `ClassMngrEngineDatabaseFileFormatTests` |
| Windows x86 Release engine tests | Passed: `ClassMngrEngineTests` and `ClassMngrEngineDatabaseFileFormatTests` |
| Retained Windows Qt file-format test | Passed: `ClassMngrDatabaseFileFormatTests` |
| Windows x64 Debug SQLite foundation test | Passed: `ClassMngrEngineSqliteDatabaseTests` |
| Windows x64 Release SQLite foundation test | Passed: `ClassMngrEngineSqliteDatabaseTests` |
| Windows x86 Debug SQLite foundation test | Passed: `ClassMngrEngineSqliteDatabaseTests` |
| Windows x86 Release SQLite foundation test | Passed: `ClassMngrEngineSqliteDatabaseTests` |
| Windows x64 Debug schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x64 Release schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x86 Debug schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x86 Release schema/OpenDatabase test | Passed: `ClassMngrEngineDatabaseSchemaTests` |
| Windows x64 Debug class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x64 Release class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x86 Debug class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x86 Release class CRUD test | Passed: `ClassMngrEngineClassRepositoryTests` |
| Windows x64 Debug teacher model/validator/use-case test | Passed: `ClassMngrEngineTeacherServiceTests` |
| Windows x64 Release teacher model/validator/use-case test | Passed: `ClassMngrEngineTeacherServiceTests` |
| Windows x86 Debug teacher model/validator/use-case test | Passed: `ClassMngrEngineTeacherServiceTests` |
| Windows x86 Release teacher model/validator/use-case test | Passed: `ClassMngrEngineTeacherServiceTests` |
| Windows x64 Debug native-English directory test | Passed: `ClassMngrEngineNativeEnglishTeacherServiceTests` |
| Windows x64 Release native-English directory test | Passed: `ClassMngrEngineNativeEnglishTeacherServiceTests` |
| Windows x86 Debug native-English directory test | Passed: `ClassMngrEngineNativeEnglishTeacherServiceTests` |
| Windows x86 Release native-English directory test | Passed: `ClassMngrEngineNativeEnglishTeacherServiceTests` |
| Windows x64 Debug GS-team directory test | Passed: `ClassMngrEngineGsTeamServiceTests` |
| Windows x64 Release GS-team directory test | Passed: `ClassMngrEngineGsTeamServiceTests` |
| Windows x86 Debug GS-team directory test | Passed: `ClassMngrEngineGsTeamServiceTests` |
| Windows x86 Release GS-team directory test | Passed: `ClassMngrEngineGsTeamServiceTests` |
| Windows x64 Debug class-information service test | Passed: `ClassMngrEngineClassInfoServiceTests` |
| Windows x64 Release class-information service test | Passed: `ClassMngrEngineClassInfoServiceTests` |
| Windows x86 Debug class-information service test | Passed: `ClassMngrEngineClassInfoServiceTests` |
| Windows x86 Release class-information service test | Passed: `ClassMngrEngineClassInfoServiceTests` |
| Retained Windows Qt teacher-import regression | Passed: `ClassMngrTeacherImportTests` |
| Retained Windows Qt class-information lifecycle regression | Passed: `ClassMngrDataServiceLifecycleTests` |

All four engine lanes configured or regenerated successfully after the engine
source addition, compiled the new implementation, and passed the targeted
CTest selections with no Qt-dependent test process. Each lane's integrated
sweep passed all nine engine suites plus both WinUI staging and manifest checks.
The new class-information implementation compiled and passed in all four
engine lanes, and all four staged WinUI targets rebuilt successfully with the
new engine dependency. The retained Qt class-information lifecycle regression
also passed, alongside the existing schema-manager, updater, and
teacher-import coverage. A narrowed source audit found no Qt, WinUI, WinRT,
Direct2D/DirectWrite, or Win32 UI dependency in `src/engine`.

## Remaining Phase 2 work

This is an in-progress record, not the Phase 2 exit gate. The next work is
extracting schedule conflict and aggregate read models, migrating imports and
report models, connecting retained Qt adapters to the new use-case boundaries,
and producing cross-platform fixture round trips.
