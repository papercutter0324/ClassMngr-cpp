# DataService API Migration Map

Status: F1420 accepted as a documentation-only inventory, 2026-10-24. This is a
source snapshot and migration guide; it does not approve deleting `DataService`
or any declaration.

## Composition and use classification

[`data_service.h`](../../src/data/data_service.h#L40-L460) exposes 77 public
operational declarations (the two `importClasses` overloads count separately).
The constructor overloads and destructor are documented below as ownership
contracts, not included in that count.

The ownership declarations are `explicit DataService(const QString& dbPath =
QString())`, `explicit DataService(DatabaseSession& session)`, and
`~DataService()`.

`DataService(const QString&)` creates and owns a standalone `DatabaseSession`;
its `open()` uses the constructor path, and its destructor closes that owned
session. `DataService(DatabaseSession&)` borrows its session, which must outlive
the facade; its destructor does not close it. In normal application composition,
[`ApplicationServices`](../../src/core/application_services.cpp#L20-L21) owns a
`WorkspaceStore` first and constructs `DataService` over that same object by
reference. The store therefore outlives the facade.

The typed feature services are constructed with `(m_workspaceStore, nullptr)`
([SettingsService through SpeakingEvaluationService](../../src/core/application_services.cpp#L161-L207)).
`FeatureService::dataService()` returns null whenever a session exists, so
those normal application services use the session repositories. The
`FeatureService(DataService*)` constructor and its DataService fallback remain
compatibility paths ([constructor and selector](../../src/app/services/feature_services.cpp#L114-L145));
the source search found no production construction of that path. One focused
test constructs a legacy `ScheduleService` this way
([test](../../tests/next_platform_application_services_schedule_slot_state_read_port_tests.cpp#L168)).
Production source also has no direct call through `ApplicationServices::dataService()`;
tests use the accessor for fixtures, assertions, and repository-level setup.
The accessor remains public and returns the live facade
([declaration](../../src/core/application_services.h#L58),
[definition](../../src/core/application_services.cpp#L146-L149)), so its
presence is a removal dependency rather than evidence that the facade is
unused.

In the tables, **v2 owner** names the ordinary production-facing service,
lifecycle, or repository boundary. **DataService route** classifies this
facade's own call path: only methods with a defined matching dispatch are
available through the legacy `FeatureService` fallback; the four SQL
`CampusRecord` methods have no such fallback. The current application
composition bypasses the fallback. Many methods are also called directly by
tests and fixture helpers.
Exact public declarations are linked by header line. The
repository accessors are listed in
[`WorkspaceStore`](../../src/data/database/workspace_store.h#L67-L82).

## Public operational declarations

### Lifecycle and settings

| Public declaration | v2 owner | DataService route and prerequisite |
|---|---|---|
| `bool open()` ([L60](../../src/data/data_service.h#L60)) | `ApplicationServices::openDatabase()` → `WorkspaceStore::open()` | Standalone legacy lifecycle; no ordinary production call through this facade. Retire only after standalone-session clients and tests move to the workspace lifecycle. |
| `Status openDatabase(const QString& dbPath)` ([L62](../../src/data/data_service.h#L62)) | `ApplicationServices::openDatabase()` / `openExistingDatabase()` → `WorkspaceStore` | Legacy lifecycle; production opening is owned by `ApplicationServices`. Preserve create-vs-existing semantics when removing callers. |
| `void closeDatabase()` ([L66](../../src/data/data_service.h#L66)) | `ApplicationServices::closeDatabase()` → `WorkspaceStore::close()` | Legacy lifecycle and owned-session destructor path; migrate any direct callers before removal. |
| `bool isOpen() const` ([L68](../../src/data/data_service.h#L68)) | `WorkspaceStore::isOpen()` / `ApplicationServices::hasOpenDatabase()` | Compatibility query; production service availability checks the session. Tests also inspect it through the accessor. |
| `QString currentDatabasePath() const` ([L70](../../src/data/data_service.h#L70)) | `ApplicationServices::currentDatabasePath()` → `WorkspaceStore::databasePath()` | Compatibility query; production callers use `ApplicationServices`. |
| `DatabaseSession* databaseSession() const` ([L74](../../src/data/data_service.h#L74)) | `ApplicationServices::databaseSession()` / `workspaceStore()` | Transitional low-level accessor. Tests use it for repository and SQL fixture setup; migrate those tests before removal. UI/controller code should use narrow feature services. |
| `Status saveSetting(const QString& key, const QVariant& value)` ([L80](../../src/data/data_service.h#L80)) | `SettingsService::save()` → `SettingsRepository::saveSetting()` | Legacy fallback and direct test setup; ordinary app services use the session repository. |
| `Status saveSettings(const QVariantMap& values)` ([L85](../../src/data/data_service.h#L85)) | `SettingsService::saveAll()` → `SettingsRepository::saveSettings()` | Legacy fallback and direct test setup; ordinary app services use the session repository. |
| `Result<QVariant> loadSetting(const QString& key)` ([L89](../../src/data/data_service.h#L89)) | `SettingsService::load()` → `SettingsRepository::loadSetting()` | Legacy fallback and test assertions; ordinary app services use the session repository. |

### Teachers and staff directories

`TeacherService` owns teacher operations; its session path uses
`TeacherRepository`, `NativeEnglishTeacherRepository`, `GsTeamRepository`, or
`TeacherImportRepository`. The compatibility fallback is in
[`feature_services.cpp`](../../src/app/services/feature_services.cpp#L220-L402).

| Public declaration | v2 owner | DataService route and prerequisite |
|---|---|---|
| `Result<int> createTeacher(const Teacher& teacher)` ([L99](../../src/data/data_service.h#L99)) | `TeacherService::create()` → `TeacherRepository` | Legacy fallback; remove after callers and tests use `TeacherService`. |
| `Result<int> saveTeacher(const Teacher& teacher)` ([L103](../../src/data/data_service.h#L103)) | `TeacherService::save()` → `TeacherRepository` | Legacy fallback; remove after callers and tests use `TeacherService`. |
| `Status updateTeacher(const Teacher& teacher)` ([L107](../../src/data/data_service.h#L107)) | `TeacherService::update()` → `TeacherRepository` | Legacy fallback; remove after callers and tests use `TeacherService`. |
| `Result<QList<Teacher>> getAllTeachers()` ([L111](../../src/data/data_service.h#L111)) | `TeacherService::teachers()` → `TeacherRepository` | Legacy fallback; tests also use the facade in support code. |
| `Result<Teacher> getTeacher(int teacherId)` ([L113](../../src/data/data_service.h#L113)) | `TeacherService::teacher()` → `TeacherRepository` | Legacy fallback; tests also use direct fixture reads. |
| `Status deleteTeacher(int teacherId)` ([L117](../../src/data/data_service.h#L117)) | `TeacherService::remove()` → `TeacherRepository` | Legacy fallback; remove after callers and tests use `TeacherService`. |
| `Result<QList<NativeEnglishTeacher>> getNativeEnglishTeachers()` ([L121](../../src/data/data_service.h#L121)) | `TeacherService::nativeEnglishTeachers()` → `NativeEnglishTeacherRepository` | Legacy fallback; tests also seed/read through the facade. |
| `Status saveNativeEnglishTeacherDirectory(const QList<NativeEnglishTeacher>& teachers, const QList<int>& deletedIds)` ([L123](../../src/data/data_service.h#L123)) | `TeacherService::saveNativeEnglishTeacherDirectory()` → `NativeEnglishTeacherRepository` | Legacy fallback; parity tests still exercise facade behavior. |
| `Result<QList<GsTeamMember>> getGsTeamMembers()` ([L128](../../src/data/data_service.h#L128)) | `TeacherService::gsTeamMembers()` → `GsTeamRepository` | Legacy fallback; tests also seed/read through the facade. |
| `Status saveGsTeamDirectory(const QList<GsTeamMember>& members, const QList<int>& deletedIds)` ([L130](../../src/data/data_service.h#L130)) | `TeacherService::saveGsTeamDirectory()` → `GsTeamRepository` | Legacy fallback; parity tests still exercise facade behavior. |
| `Result<TeacherImportSummary> importTeachers(const TeacherImportPlan& plan)` ([L135](../../src/data/data_service.h#L135)) | `TeacherService::importTeachers()` → `TeacherImportRepository` and import use case | Legacy fallback; remove after production callers and import tests use the typed service. |

### Classes, class information, and imports

`ClassService` owns the class, class-info, conflict, and class-transfer
operations; session paths use `ClassRepository`, `ClassInfoRepository`, and
`ClassTransferRepository`. Its fallback is in
[`feature_services.cpp`](../../src/app/services/feature_services.cpp#L400-L716).
The two `importClasses` declarations are distinct compatibility overloads.

| Public declaration | v2 owner | DataService route and prerequisite |
|---|---|---|
| `Result<int> createClass(const QString& name)` ([L145](../../src/data/data_service.h#L145)) | `ClassService::create()` → `ClassRepository` | Legacy fallback; remove after service callers and tests migrate. |
| `Result<QList<Classroom>> getClasses()` ([L149](../../src/data/data_service.h#L149)) | `ClassService::classes()` → `ClassRepository` | Legacy fallback; remove after service callers and tests migrate. |
| `Result<Classroom> getClassById(int classId)` ([L151](../../src/data/data_service.h#L151)) | `ClassService::classroom()` → `ClassRepository` | Legacy fallback; remove after service callers and tests migrate. |
| `Status updateClassName(int classId, const QString& name)` ([L155](../../src/data/data_service.h#L155)) | `ClassService::rename()` → `ClassRepository` | Legacy fallback; remove after service callers and tests migrate. |
| `Status deleteClass(int classId)` ([L160](../../src/data/data_service.h#L160)) | `ClassService::remove()` → `ClassRepository` | Legacy fallback; remove after service callers and tests migrate. |
| `Result<ClassTransferPackage> buildClassTransferPackage(const QList<int>& classIds)` ([L164](../../src/data/data_service.h#L164)) | `ClassService::buildTransferPackage()` → `ClassTransferRepository` | Legacy fallback; preserve codec and export callers at the assigned Phase 3/8 boundary before removal. |
| `Result<ClassImportPreview> previewClassImport(const ClassTransferPackage& package)` ([L168](../../src/data/data_service.h#L168)) | `ClassService::previewImport()` → `ClassTransferRepository` | Legacy fallback; migrate review callers/tests to the service before removal. |
| `Result<ClassImportSummary> importClasses(const ClassTransferPackage& package, const ClassImportPlan& plan)` ([L172](../../src/data/data_service.h#L172)) | `ClassService::importClasses(package, plan)` adapts the plan to the request overload | Compatibility adapter overload; the feature-service fallback dispatches the request overload. Remove only after callers of this exact signature migrate. |
| `Result<ClassImportSummary> importClasses(const ClassTransferPackage& package, const ClassMngr::Next::Application::ClassTransferApplyRequest& request)` ([L177](../../src/data/data_service.h#L177)) | `ClassService::importClasses(package, request)` → `ClassTransferRepository` | Legacy fallback; preserve Phase 3 codec and Phase 8 operation ownership through migration. |
| `Result<ScheduleImportPreview> previewScheduleImport(const ScheduleImportUserBlock& user, ScheduleImportKind kind)` ([L182](../../src/data/data_service.h#L182)) | `ScheduleService::previewImport()` → `ScheduleImportRepository` | Legacy fallback; migrate review flows and parity tests to the service. |
| `Result<ScheduleImportSummary> importSchedule(const ScheduleImportPlan& plan)` ([L187](../../src/data/data_service.h#L187)) | `ScheduleService::importSchedule()` → `ScheduleImportRepository` | Legacy fallback; migrate apply flows and persistence tests to the service. |
| `Status saveClassInfo(const ClassInfo& info)` ([L197](../../src/data/data_service.h#L197)) | `ClassService::saveClassInfo()` → `ClassInfoRepository` | Legacy fallback; keep service validation/conflict policy as the production contract. |
| `Status saveClassNotes(int classId, const QString& notes, const QString& timeFillerActivities)` ([L201](../../src/data/data_service.h#L201)) | `ClassService::saveClassNotes()` → `ClassInfoRepository` | Legacy fallback; migrate note editors and tests to the service. |
| `Result<ClassInfo> loadClassInfo(int classId)` ([L207](../../src/data/data_service.h#L207)) | `ClassService::classInfo()` → `ClassInfoRepository` | Legacy fallback; migrate callers to a bounded typed query where required. |
| `Result<QList<ClassConflict>> getClassTimeConflicts(int classId, const QList<ClassTime>& times, ScheduleType type)` ([L354](../../src/data/data_service.h#L354)) | `ClassService::conflicts()` → `ClassInfoRepository` | Legacy fallback; remove after conflict-validation clients and tests use `ClassService`. |

### Schedule and testing data

`ScheduleService` owns these operations. Its session path uses
`IntensiveSlotStateRepository`, `TestingBlockRepository`, and
`TestingClassRepository`; the fallback implementations are in
[`feature_services.cpp`](../../src/app/services/feature_services.cpp#L756-L973).

| Public declaration | v2 owner | DataService route and prerequisite |
|---|---|---|
| `Result<QList<IntensiveSlotState>> loadIntensiveSlotStates()` ([L221](../../src/data/data_service.h#L221)) | `ScheduleService::intensiveSlotStates()` → `IntensiveSlotStateRepository` | Legacy fallback; remove after schedule clients and tests use the service. |
| `Status saveIntensiveSlotState(const QString& day, const QString& startTime, const QString& state, const QString& defaultState = QStringLiteral("essay"))` ([L223](../../src/data/data_service.h#L223)) | `ScheduleService::saveIntensiveSlotState()` → `IntensiveSlotStateRepository` | Legacy fallback; preserve default-state behavior in migrated callers. |
| `Result<QList<TestingAssignment>> loadTestingAssignments()` ([L234](../../src/data/data_service.h#L234)) | `ScheduleService::testingAssignments()` → `TestingBlockRepository` | Legacy fallback; migrate assignment readers and tests to the service. |
| `Result<QList<TestingBlock>> loadTestingBlocks()` ([L237](../../src/data/data_service.h#L237)) | `ScheduleService::testingBlocks()` → `TestingBlockRepository` | Legacy fallback; migrate block readers and tests to the service. |
| `Status saveTestingBlock(const QString& day, const QString& startTime, const QString& room, bool replaceExisting = false)` ([L239](../../src/data/data_service.h#L239)) | `ScheduleService::saveTestingBlock()` → `TestingBlockRepository` | Legacy fallback; preserve replacement semantics during migration. |
| `Status assignTestingClass(const QString& day, const QString& startTime, int classId, bool replaceExisting = false)` ([L246](../../src/data/data_service.h#L246)) | `ScheduleService::assignTestingClass()` → `TestingBlockRepository` | Legacy fallback; preserve replacement semantics during migration. |
| `Status deleteTestingAssignment(const QString& day, const QString& startTime)` ([L253](../../src/data/data_service.h#L253)) | `ScheduleService::deleteTestingAssignment()` → `TestingBlockRepository` | Legacy fallback; migrate schedule clients and tests. |
| `Status deleteTestingBlock(const QString& day, const QString& startTime)` ([L258](../../src/data/data_service.h#L258)) | `ScheduleService::deleteTestingBlock()` → `TestingBlockRepository` | Legacy fallback; migrate schedule clients and tests. |
| `Status clearTestingAssignments()` ([L263](../../src/data/data_service.h#L263)) | `ScheduleService::clearTestingAssignments()` → `TestingBlockRepository` | Legacy fallback; migrate schedule clients and tests. |
| `Status clearTestingBlocks()` ([L265](../../src/data/data_service.h#L265)) | `ScheduleService::clearTestingBlocks()` → `TestingBlockRepository` | Legacy fallback; migrate schedule clients and tests. |
| `Result<int> createTestingClass(const TestingClass& testingClass, const QString& assignmentDay = {}, const QString& assignmentStartTime = {})` ([L271](../../src/data/data_service.h#L271)) | `ScheduleService::createTestingClass()` → `TestingClassRepository` | Legacy fallback; migrate class creation and assignment callers together. |
| `Status updateTestingClass(const TestingClass& testingClass)` ([L277](../../src/data/data_service.h#L277)) | `ScheduleService::updateTestingClass()` → `TestingClassRepository` | Legacy fallback; migrate details editors and tests. |
| `Result<TestingClass> loadTestingClass(int classId)` ([L281](../../src/data/data_service.h#L281)) | `ScheduleService::testingClass()` → `TestingClassRepository` | Legacy fallback; migrate details readers and tests. |
| `Result<QList<TestingClass>> loadTestingClasses()` ([L285](../../src/data/data_service.h#L285)) | `ScheduleService::testingClasses()` → `TestingClassRepository` | Legacy fallback; migrate list readers and tests. |
| `Status deleteTestingClass(int classId)` ([L287](../../src/data/data_service.h#L287)) | `ScheduleService::deleteTestingClass()` → `TestingClassRepository` | Legacy fallback; migrate details actions and tests. |
| `Result<bool> isTestingClass(int classId)` ([L291](../../src/data/data_service.h#L291)) | `ScheduleService::isTestingClass()` → `TestingClassRepository` | Legacy fallback; migrate checks and tests. |

### Calendar events

`CalendarService` owns the typed API; its session path uses
`CalendarEventRepository`. The DataService dispatches are in
[`feature_services.cpp`](../../src/app/services/feature_services.cpp#L987-L1199).

| Public declaration | v2 owner | DataService route and prerequisite |
|---|---|---|
| `Result<QList<CalendarEvent>> loadCalendarEventsForDate(const QDate& date)` ([L301](../../src/data/data_service.h#L301)) | `CalendarService::eventsForDate()` → `CalendarEventRepository` | Legacy fallback; migrate calendar readers and tests to the service. |
| `Result<QList<CalendarEvent>> loadCalendarEventsInRange(const QDate& startDate, const QDate& endDate)` ([L305](../../src/data/data_service.h#L305)) | `CalendarService::eventsInRange()` → `CalendarEventRepository` | Legacy fallback; migrate readers and tests to the service. |
| `Result<QList<CalendarEventDateInterval>> loadCalendarEventDateIntervalsInRange(const QDate& startDate, const QDate& endDate)` ([L309](../../src/data/data_service.h#L309)) | `CalendarService::eventDateIntervalsInRange()` → `CalendarEventRepository` | Legacy fallback; retain bounded interval semantics for Sub Prep before removing. |
| `Result<QList<CalendarEvent>> loadUpcomingCalendarEvents(const QDate& fromDate, int limit)` ([L315](../../src/data/data_service.h#L315)) | `CalendarService::upcomingEvents()` → `CalendarEventRepository` | Legacy fallback; migrate dashboard readers and tests. |
| `Result<CalendarEvent> getCalendarEvent(qint64 eventId)` ([L320](../../src/data/data_service.h#L320)) | `CalendarService::event()` → `CalendarEventRepository` | Legacy fallback; migrate event-detail callers and tests. |
| `Result<QList<CalendarEvent>> loadCalendarEventsForRepeatSeriesFromDate(const QString& repeatSeriesId, const QDate& startDate)` ([L324](../../src/data/data_service.h#L324)) | `CalendarService::repeatSeriesFromDate()` → `CalendarEventRepository` | Legacy fallback; preserve repeat-series behavior in migrated flows. |
| `Result<qint64> saveCalendarEvent(const CalendarEvent& event)` ([L329](../../src/data/data_service.h#L329)) | `CalendarService::saveEvent()` → `CalendarEventRepository` | Legacy fallback; migrate calendar writes and error-mapping tests. |
| `Result<QList<qint64>> saveCalendarEvents(const QList<CalendarEvent>& events)` ([L333](../../src/data/data_service.h#L333)) | `CalendarService::saveEvents()` → `CalendarEventRepository` | Legacy fallback; preserve batch semantics and migrate batch tests. |
| `Status deleteCalendarEvent(qint64 eventId)` ([L337](../../src/data/data_service.h#L337)) | `CalendarService::deleteEvent()` → `CalendarEventRepository` | Legacy fallback; migrate delete actions and tests. |
| `Status deleteCalendarEventsForRepeatSeriesFromDate(const QString& repeatSeriesId, const QDate& startDate)` ([L341](../../src/data/data_service.h#L341)) | `CalendarService::deleteRepeatSeriesFromDate()` → `CalendarEventRepository` | Legacy fallback; preserve recurrence boundaries and migrate tests. |
| `Status deleteAllCalendarEvents()` ([L346](../../src/data/data_service.h#L346)) | `CalendarService::deleteAllEvents()` → `CalendarEventRepository` | Legacy fallback; migrate clear-all workflow and tests. |

### Rosters and speaking evaluations

`RosterService` owns roster persistence and bounded output reads through
`RosterRepository`. `SpeakingEvaluationService` owns evaluation persistence and
score import through `SpeakingEvalRepository`; fallback dispatch is in
[`feature_services.cpp`](../../src/app/services/feature_services.cpp#L1220-L1546).

| Public declaration | v2 owner | DataService route and prerequisite |
|---|---|---|
| `Status saveRoster(int classId, const Roster& roster)` ([L369](../../src/data/data_service.h#L369)) | `RosterService::saveRoster()` → `RosterRepository` | Legacy fallback; direct test fixture use remains. Migrate callers/tests and retain the service's validation policy. |
| `Status saveRosters(const QList<QPair<int, Roster>>& rosters)` ([L374](../../src/data/data_service.h#L374)) | `RosterService::saveRosters()` → `RosterRepository` | Legacy fallback; migrate batch callers/tests and preserve transaction behavior. |
| `Result<Roster> loadRoster(int classId)` ([L378](../../src/data/data_service.h#L378)) | `RosterService::roster()` → `RosterRepository` | Legacy fallback; migrate readers and tests to the service. |
| `Result<Roster> loadRosterForOutput(int classId, const QStringList& requestedColumns, std::size_t maxRows, std::size_t maxCells, std::size_t maxTextBytes)` ([L382](../../src/data/data_service.h#L382)) | `RosterService::rosterForOutput()` → bounded `RosterRepository` query | Legacy fallback; keep output bounds and memory lifetime evidence at the Phase 3/consumer boundary. |
| `Result<int> getRosterStudentCount(int classId)` ([L390](../../src/data/data_service.h#L390)) | `RosterService::studentCount()` → `RosterRepository` | Legacy fallback; migrate count readers and test helpers. |
| `Status saveSpeakingEval(int classId, const QString& evaluationName, const SpeakingEvalRows& rows, const QList<SpeakingEvalCellChange>& dirtyCells = {})` ([L402](../../src/data/data_service.h#L402)) | `SpeakingEvaluationService::saveEvaluation()` → `SpeakingEvalRepository` | Legacy fallback; preserve dirty-cell update behavior and migrate callers/tests. |
| `Result<SpeakingEvalRows> loadSpeakingEval(int classId, const QString& evaluationName)` ([L409](../../src/data/data_service.h#L409)) | `SpeakingEvaluationService::evaluation()` → `SpeakingEvalRepository` | Legacy fallback; migrate readers and parity tests. |
| `Result<QList<SpeakingEvalScore>> buildRosterScoreImport(int classId, const QString& evaluationName)` ([L414](../../src/data/data_service.h#L414)) | `SpeakingEvaluationService::buildRosterScoreImport()` → evaluation/roster query path | Legacy fallback; preserve score matching and bounded-read behavior before removal. |

### SQL campus records: unresolved typed-service gap

| Public declaration | v2 owner | DataService route and prerequisite |
|---|---|---|
| `Result<int> saveCampus(const CampusRecord& campus)` ([L425](../../src/data/data_service.h#L425)) | `WorkspaceStore::campusRecordRepository()` → `CampusRecordRepository` | Test-only direct facade use is visible in lifecycle tests. There is no typed `CampusService` or ordinary feature-service caller. Add/assign a typed service boundary and migrate tests/callers before retiring this SQL API. |
| `Result<CampusRecord> getCampus(int campusId)` ([L429](../../src/data/data_service.h#L429)) | `WorkspaceStore::campusRecordRepository()` → `CampusRecordRepository` | Test-only direct facade use; no typed SQL-campuses service exists. Preserve the SQL record contract or explicitly resolve its owner before removal. |
| `Result<QList<CampusRecord>> getAllCampuses()` ([L433](../../src/data/data_service.h#L433)) | `WorkspaceStore::campusRecordRepository()` → `CampusRecordRepository` | Test-only direct facade use; no typed SQL-campuses service exists. Inventory all schema consumers and migrate tests before removal. |
| `Status deleteCampus(int campusId)` ([L435](../../src/data/data_service.h#L435)) | `WorkspaceStore::campusRecordRepository()` → `CampusRecordRepository` | Test-only direct facade use; no typed SQL-campuses service exists. Establish the service owner and migrate callers/tests before removal. |

The existing `CampusDashboardCampusSavePort` and
`CampusDashboardCampusRepositoryAdapter` persist `CampusInfo` JSON files; they
are not replacements for the SQL `CampusRecord` repository. Campus dashboard,
calendar, My Info, and Sub Prep campus-directory ports are feature-specific
contracts, not a single typed service for these four SQL methods. Evidence:
[`CampusRecordRepository`](../../src/data/repositories/campus_record_repository.h#L8-L28),
[`CampusDashboardCampusSavePort`](../../src/next/application/campus_dashboard_campus_save_port.h#L13-L32),
and its [JSON-backed adapter](../../src/next/platform/campus_dashboard_campus_repository_adapter.h#L23-L108).

### Save, Save As, and export

| Public declaration | v2 owner | DataService route and prerequisite |
|---|---|---|
| `void save()` ([L445](../../src/data/data_service.h#L445)) | `ApplicationServices::saveDatabase()` → `WorkspaceStore::commit()` | No ordinary production caller was found. F1418 moved live save to `WorkspaceStore::commit()`; `ApplicationServices::saveDatabase()` remains void, no-ops for a closed store, and ignores commit failure. The dormant facade method was out of scope; resolve direct consumers and required tests before removal. |
| `Status saveAs(const QString& destinationPath)` ([L447](../../src/data/data_service.h#L447)) | `ApplicationServices::saveDatabaseAs()` → `DatabaseFileOperations` | No ordinary production caller was found. Migrate direct consumers and preserve destination validation/copy behavior before removal. |
| `Status exportAs(const QString& destinationPath)` ([L451](../../src/data/data_service.h#L451)) | `ApplicationServices::exportDatabaseAs()` → `DatabaseFileOperations` | No ordinary production caller was found. Migrate direct consumers and preserve export behavior before removal. |

## Removal prerequisites and open gaps

- Keep the `ApplicationServices::dataService()` accessor until all direct
  production and test uses have been rechecked and migrated. Its present
  production callsite search was empty, but it is a public accessor and many
  tests use it.
- Remove a feature method only after production callers use the named typed
  service, its DataService fallback branch is removed, and required tests and
  support fixtures no longer depend on direct facade calls. Keep behavior and
  failure mapping covered at the owning v2 boundary.
- Keep lifecycle ownership in `WorkspaceStore`/`ApplicationServices`, including
  the distinction between creating a new database and opening an existing one.
  `DataService`'s owning and borrowed constructor semantics must not leak into
  v2 composition.
- Resolve the SQL `CampusRecord` owner. `CampusRecordRepository` exists on
  `WorkspaceStore`, but no aggregate typed service currently exposes these
  operations; the JSON-backed campus dashboard adapter is a separate model and
  persistence path.
- Do not infer whole-facade removability from this inventory. It records the
  public API and current source paths; later slices must verify callers,
  fixtures, file/schema contracts, and owner boundaries when each operation is
  migrated.

## Evidence references

- Declarations and facade ownership: [`data_service.h`](../../src/data/data_service.h#L40-L460)
  and [`data_service.cpp`](../../src/data/data_service.cpp#L26-L1161).
- Legacy fallback selection: [`FeatureService` constructors and selector](../../src/app/services/feature_services.cpp#L114-L145).
- Normal feature composition: [`ApplicationServices` service construction](../../src/core/application_services.cpp#L161-L207).
- Workspace repository surface: [`WorkspaceStore`](../../src/data/database/workspace_store.h#L67-L82).
- Lifecycle/save ownership: [`ApplicationServices`](../../src/core/application_services.cpp#L43-L144).
- Explicit legacy fallback test: [`ScheduleService` DataService construction](../../tests/next_platform_application_services_schedule_slot_state_read_port_tests.cpp#L168-L173).
- SQL campus operations: [`CampusRecordRepository`](../../src/data/repositories/campus_record_repository.h#L8-L28).
