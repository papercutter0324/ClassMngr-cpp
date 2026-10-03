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
- Last updated: 2026-10-03
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
  F164's selected-class grade read, F165's selected-class subtitle read, and
  F166's RosterEditorWidget class subtitle read, and F167's ClassDetailsPage
  display read, F168's ClassDetailsPage save port, and F169's ClassNotesPage
  save port, F170's roster read port, and F171's roster save port are accepted.
  F172's Class Co-Teacher assignment save port, F173's ScheduleWidget
  slot-state save port, and F174's speaking-evaluation read port are accepted.
  F175's ScheduleBuilder source port, F176's Sub Prep class-details read port,
  F177's Sub Prep schedule-summary port, F178's Sub Prep print source port,
  F179's Sub Prep roster-output source port, F180's Sub Prep calendar-event
  interval port, and F181's Calendar Event Import signature query port are
  accepted. F182's active Calendar event read/by-ID adapter, F183's single
  Calendar Event save port, F184's Calendar Event delete port, F185's
  Calendar Event delete-all port, F186's repeat-series suffix-delete port,
  F187's Calendar Event Import save port, F188's repeat-series creation port,
  and F189's repeat-series edit port are accepted. F190-F197's preference
  ports, F198's Personal Details save, F199's Personal Display Name, F200's
  Personal Signature preferences, F201's Personal Signature Image, F202's
  Class Visibility, F203's Evaluation Default Policy, F204's Class Day Filter
  Reset Policy, F205's Class Selection Reset Policy, F206's Custom Color
  Palette preferences, F207's Sub Prep Personal Zoom preferences, and F208's
  Sub Prep saved-content preferences and F209's speaking-evaluation save are
  accepted; F210's recent-workspace history policy, F211's Speaking
  Evaluation read-port extraction, and F212's upcoming-birthday schedule
  policy, F213's default evaluation selection, and F214's class day-filter
  matching policy, F215's automatic-update startup eligibility, F216's
  skipped-update-version policy, and F217's roster-score import are accepted.
  F218's Speaking Evaluation class-tab integration, F219's live-state
  validation, F220's plan eligibility policy, F221's current-state snapshot,
  F222's resolution choices, F223's matching projection, F224's typed-snapshot
  prepare path, F225's apply contract, and F226's readiness orchestration are
  accepted. F227's proposed-summary projection, F228's cleared-schedule count,
  F229's typed preview projection, F230's UI-built typed apply request, F231's
  apply-request decision projection, and F232's session-bound typed apply are
  accepted. F233 is accepted: the typed apply request is the shared repository
  core input and the validated v1 plan is an edge adapter. F234 is accepted:
  the UseCase and direct typed repository entry share Qt-free validation in
  which plan eligibility runs first, followed by the intensive-mode check and
  teacher target check. Localized formatting stays at the edges, and the legacy
  plan validator is unchanged. F235 is accepted: repository state validation
  preserves exact typed teacher and class target IDs, rejecting aliases such as
  `01` before writes. F236 is accepted: structured typed policy, teacher-target,
  and fresh-state errors carry through Repository -> Platform -> dialog, while
  legacy `apply(plan)` stays localized and SQL/transaction failures remain
  message-only. F237 is accepted: Qt-free Speaking Evaluation validation and
  normalization is shared by save and page feedback; Platform persists
  normalized input and invalid data is blocked. F238 is accepted: a Qt-free
  Application initial-setup lifecycle with a Platform file/workspace adapter
  retains FileController warnings/recent-file behavior and original-profile
  recovery. F239 is accepted for app-less roster row reordering over existing
  `RosterSnapshot` rows; F240 is accepted for app-less roster row removal;
  F241 is accepted for Qt-free roster-column name admission; F242 is accepted
  for custom-column removal eligibility; F243 is accepted for app-less custom-
  column append; F244 is accepted for roster-transfer destination preparation;
  F245 is accepted for Korean-name suffix suggestion policy; F246 is accepted
  for same-grade roster transfer-target eligibility; F247 is accepted for
  Speaking Evaluation roster-name import planning; F248 is accepted for
  Qt-free duplicate peer-row lookup; F249 is accepted for AI batch student
  eligibility; F250 is accepted for Qt-free first-empty roster-row lookup;
  F251 is accepted for AI batch comment-quality policy; F252 is accepted for
  Qt-free AI batch accepted-comment planning; F253 is accepted for the Qt-free
  private-notes splitter; F254 is accepted for Qt-free roster-score assignment
  planning; F255 is accepted for reusing the AI eligibility policy in the
  single-report dialog; F256 is accepted for the Speaking Evaluation roster
  read cutover; F257 is accepted for Speaking Evaluation report-context reads;
  F258 is accepted for canonical evaluation names, F259 for roster-transfer
  target metadata reads, F260 for roster evaluation-column classification,
  F261 for RosterPrintDialog class-label reads, F262 for Class Transfer dialog
  class-label reads, F263 for matched-teacher labels in ClassImportDialog,
  F264 for optional roster reads in RosterPrintDialog, F265 for transfer-menu
  target-roster reads, and F266 for ClassExportDialog class-list reads are
  accepted, and F267 for RosterPrintDialog's normal class-list read is
  accepted. F268 for MyClassesPage's class-list read, F269 for the setup
  wizard's teacher-choice list read, F270 for MyClassesPage's assigned-teacher
  profile read, F271 for the sidebar delete-prompt class display-name read,
  F272 for RosterPrintDialog's current-class-only testing-class read, F273 for
  the transfer-time target-roster read, and F274 for RosterPrintDialog's
  extra-info class-list read, and F275 for the class-delete chooser's
  class-list read, F276 for the sidebar's upcoming birthday Native English and
  GS directory reads, F277 for the roster transfer menu's class-list read,
  and F278 for the initial-setup wizard's teacher-existence reads are accepted;
  F279 for the sidebar's selected-teacher profile read is accepted. F280 is
  selected for the sidebar teacher-delete chooser's teacher-list read.
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

Earlier verified slices and cumulative exit-gate snapshots are archived in the [Phase 2 progress log](03-Phase-2-Progress-Log.md).

## Latest progress update - 2026-10-03 (F266-F275 accepted; F276 selected)

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

F267 routes only the normal class-list read in RosterPrintDialog::loadClasses()
through the accepted ClassesListReadQuery and
ApplicationServicesClassesListReadPort. ApplicationServices creates the
feature services and the list adapter from the same DatabaseSession. Existing
service-availability guards, repository order, class IDs, checked state,
selected-class label query, warning, empty-list failure behavior, and the
current-class-only TestingClass branch remain intact. The new UI test drops the
classes table, captures the warning details, and verifies the class list is
empty and enabled. Fresh Windows x64 Debug/Ninja verification built
ClassMngrFeatures, ClassMngrRosterPrintDialogTests, and the list query/adapter
test targets. The focused RosterPrintDialog CTest and both supporting CTests
passed 3/3; direct execution of the new QtTest slot passed 3/3 including setup
and cleanup. git diff --check and direct trailing-whitespace inspection passed.
The unique build tree was removed after logs were preserved under
`build/p2_f267_verify_logs/`. Non-fatal Vulkan, zlib fallback, vswhere, and
offscreen Qt resource/font/size-hint warnings occurred; the full suite and other
platforms were not run. Phase 2 remains In Progress/Open; Gates 1 and 2 remain
Partial.

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

F280 is selected to migrate only the teacher-list read in
`SidebarController::promptForTeacherToDelete()` through the accepted
`InitialSetupTeacherChoicesReadQuery` and
`ApplicationServicesInitialSetupTeacherChoicesReadPort`. Preserve repository
order, formatted labels, the existing warning (`Delete Teacher` /
`Teachers could not be loaded.`), and the early return when no usable records
remain. Keep the selected-profile read in `deleteTeacher()` and
`updateActionStates()` unchanged. On success, verify the chooser label and
selected ID; on failure, verify the warning without a chooser or confirmation.
Retain the teacher-choice query and adapter CTests. The query rejects the full
list if any ID is invalid/duplicated, where the old chooser skipped nonpositive
IDs, and the adapter requires an active session unlike the legacy DataService
fallback. Phase 2 remains In Progress/Open; Gates 1 and 2 remain Partial.
