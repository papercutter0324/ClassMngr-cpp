ClassMngr Refactoring Plan — Remaining Work

Repository: papercutter0324/ClassMngr-cpp

Current Status

The major architectural refactoring is complete.

Plan 1 — Consolidate Bloat and Duplicate Code: Complete.
Modular CMake and production libraries
Shared policies and utilities
Shared page/autosave components
Narrow feature services replacing production DataService use
Decomposition of oversized UI, import, and output units
Plan 2 — Unified Dialog System: Complete.
UserPromptService
FileDialogService
DialogShell
Migration of all production dialogs
CI and contract-test enforcement
Plan 3 — Data and Input Validation: Complete.
Repository mutation and significant read contracts now expose `Status` or
`Result<T>` failures explicitly. Structured validation, domain validators,
inline/autosave-aware UI validation, numbered schema migrations, and their
regression coverage are complete.

The current Debug build and all 59 CTest targets pass.

Plan 3 Implementation Record — Data and Input Validation
Phase 1 — Finish Failure Observability
Completed
DatabaseSchemaManager::ensureSchema() returns Status.
Schema setup is transactional.
Database opening fails cleanly after schema/setup failure.
SQL failures preserve driver, database, native-code, query, action, and record context.
All repository mutations return checked Status / Result<T>.
Multi-statement writes use DatabaseTransaction.
No unchecked repository SQL writes remain.

Read contracts for:

campus records
class-time conflict checks
classes
teachers

now distinguish successful-empty, missing-record, unavailable-service, and SQL-failure cases.

Completed

Compound class-information, roster, speaking-evaluation, settings, and
calendar reads now use checked `Result<T>` contracts. The repository,
DataService, and feature-service public read APIs were audited to ensure
SQL failures are not returned as empty collections or fabricated defaults.
Exit Criteria
Repository query failures cannot be mistaken for missing or empty data.
No repository read silently returns a fabricated default after SQL failure.
Services and UI callers explicitly handle read failures.
Phase 2 — Shared Validation Model: Complete

Introduce one structured validation system used across features.

enum class ValidationSeverity
{
    Warning,
    Error
};

struct ValidationIssue
{
    QString code;
    QString field;

    int row = -1;
    int column = -1;

    ValidationSeverity severity = ValidationSeverity::Error;
    QVariantMap arguments;
};

using ValidationIssues = QList<ValidationIssue>;

Add ValidationResult helpers for:

hasErrors()
hasWarnings()
warnings()
forField()
merge()
Complete Existing Foundations

Reuse the already-created shared policies for:

student-name normalization
weekday/time parsing
colors
filenames

Add shared enum validation and ensure normalizers never replace invalid values with unrelated defaults.

Tests

Add table-driven coverage for:

Unicode and whitespace
Korean/English name rules
time formats and ordering
color formats
duplicate name pairs
enum values
reserved/invalid filenames
Exit Criteria

All validation features can return stable, field-addressed issues without depending on UI widgets or translated messages.

Phase 3 — Domain Validators and Service Enforcement: Complete

Create:

TeacherValidator
ClassInfoValidator
ClassTimeValidator
CalendarEventValidator
RosterValidator
SpeakingEvalValidator

Validators should run in application/feature services before persistence.

Imports must use the same validators as manually entered data.

Teacher

Validate:

at least one usable name
allowed English/Korean names
preferred-name selection
birthday as a typed QDate/optional date
normalized phone numbers
internetType and projectionType enum values
maximum lengths for names, room, credentials, notes, etc.
Class and Schedule

Validate:

valid class identifiers
recognized grade/level combinations
books appropriate for the grade
valid/canonical colors
typed weekdays/times
end > start
duplicate class slots
class conflicts through ClassService
Roster and Speaking Evaluation

Validate:

shared name rules and duplicate pairs
feature-specific required columns
allowed speaking-evaluation scores
comment/note lengths
pasted/imported ranges before mutation
cell-addressed errors and warnings
Calendar

Move validation currently performed by dialogs into CalendarEventValidator:

title
dates
timed/all-day/unconfirmed consistency
start/end ordering
recurrence bounds
occurrence limits

Validate both single events and generated recurrence series.

Imports

Standardize the flow:

read -> parse -> normalize -> validate -> review -> commit

Diagnostics should retain workbook/sheet/cell or JSON-path locations.

Exit Criteria

No caller can bypass domain validation simply by avoiding a particular page or dialog.

Phase 4 — UI Validation Experience: Complete

Add a shared FormValidationBinder.

It should:

map ValidationIssue.field values to widgets
display concise inline errors
apply common error styling
set accessible error descriptions
focus and scroll to the first invalid field on submission

Use QValidator, input masks, combo restrictions, and maximum lengths only for rules safe to enforce while typing.

Save / Autosave Behavior
Disable manual Save while blocking errors exist.
Keep a page dirty when autosave is blocked.
Pause autosave while invalid.
Automatically resume autosave after the data becomes valid.
Do not show one message box per invalid field.
Use a summary prompt only when useful.
Allow warnings to continue after explicit confirmation where product policy requires it.
Exit Criteria

Validation errors are visible, accessible, consistent, and compatible with the existing autosave system.

Phase 5 — Database Constraints and Migrations: Complete
Foreign Keys

Enable and verify:

PRAGMA foreign_keys = ON;

for every database connection.

Add appropriate foreign-key relationships and deliberate ON DELETE behavior for:

classes
class information
class times
roster data
speaking evaluations
speaking-evaluation data
other stable parent/child relationships
Constraints

Add appropriate:

NOT NULL
CHECK
UNIQUE

constraints for stable database invariants such as:

boolean/state values
required IDs
valid row indexes
recognized enum-like values

Do not rely on database constraints for rules likely to change with product behavior.

Migration System

Replace ad-hoc ensureTableColumn() schema evolution with numbered migrations using:

PRAGMA user_version;

Each migration must be transactional.

Before introducing stricter constraints:

inspect existing profiles
report invalid legacy rows
repair or quarantine invalid data according to defined rules
back up profiles before destructive migration
Exit Criteria
Schema changes have explicit versions.
Migrations can fail safely and roll back.
Referential integrity is enforced by SQLite.
Existing profiles have a defined upgrade/preflight path.
Phase 6 — Regression and Failure Testing: Complete
Validators

Unit-test every validator and normalizer.

Repositories

Cover:

SQL failures
constraint failures
transaction rollback
foreign-key cascade/restrict behavior
migration from representative older schemas
UI

Cover:

first-error focus
inline error text
Save disabled while invalid
autosave pause/resume
warning confirmation
accessibility descriptions
Imports

Cover:

mixed valid/invalid rows
duplicate students
invalid times
unsupported schema versions
source-location diagnostics
partial failures without partial commits

Add fuzz/property-style testing where practical for:

workbook parsing
JSON parsing
filename normalization
name normalization
Exit Criteria

Validation, persistence failure, migration, import, and UI-error behavior are protected against regression.

Recommended Remaining PR Sequence
PR 1 — Finish Repository Read Contracts: Complete

Convert:

class-information reads
roster reads
speaking-evaluation reads
settings/calendar reads

Then audit all repository/service reads for ambiguous default or empty results.

PR 2 — Validation Infrastructure: Complete

Add:

ValidationIssue
ValidationResult
shared enum/range validation
structured validation tests
PR 3 — Teacher and Class Validation: Complete

Implement:

TeacherValidator
ClassInfoValidator
ClassTimeValidator

Enforce them through TeacherService and ClassService.

PR 4 — Calendar Validation: Complete

Extract CalendarEventValidator and apply it to manual editing, recurrence creation, and imports.

PR 5 — Roster and Speaking Evaluation Validation: Complete

Implement shared structured validation while retaining feature-specific rules.

PR 6 — UI Validation Binder: Complete

Introduce FormValidationBinder and integrate it incrementally with teacher, class, calendar, roster, and speaking-evaluation surfaces.

PR 7 — Numbered Database Migrations: Complete

Introduce:

migration runner
PRAGMA user_version
foreign-key enforcement
schema constraints
legacy-data preflight
PR 8 — Validation and Migration Regression Suite: Complete

Complete repository, validator, UI, import, migration, and property/fuzz-style coverage.

Final Acceptance Criteria

Completed. The final audit confirmed the contracts and validation paths above,
and the freshly rebuilt Debug regression suite passes all 59 tests.

The refactoring initiative is complete when:

Every persistence mutation and significant read exposes failure through Status or Result<T>.
SQL failures cannot be confused with valid empty or missing data.
Hard validation rules live outside widgets.
Manual entry, imports, and programmatic callers use the same domain validators.
UI validation is inline, accessible, and autosave-aware.
SQLite enforces stable relational and data invariants.
Schema changes use numbered transactional migrations.
Legacy profiles have a defined validation and migration path.
Validation, persistence, imports, and migration failures are covered by regression tests.
