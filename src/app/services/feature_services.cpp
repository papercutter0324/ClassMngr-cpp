#include "feature_services.h"

#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/class_transfer_repository.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/schedule_import_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "data/repositories/teacher_import_repository.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/testing_block_repository.h"
#include "data/repositories/testing_class_repository.h"

namespace
{
QString unavailableError()
{
    return QStringLiteral("No Teacher Profile service is available.");
}
}

FeatureService::FeatureService(DataService* dataService)
    : m_legacyDataService(dataService)
{
}

FeatureService::FeatureService(
    DatabaseSession* session,
    DataService* legacyDataService
    )
    : m_session(session)
    , m_legacyDataService(legacyDataService)
{
}

bool FeatureService::isAvailable() const
{
    return (m_session && m_session->isOpen())
        || (m_legacyDataService && m_legacyDataService->isOpen());
}

DatabaseSession* FeatureService::session() const
{
    return m_session;
}

DataService* FeatureService::dataService() const
{
    return m_legacyDataService;
}

void SettingsService::save(const QString& key, const QVariant& value) const
{
    if (auto* repository = session() ? session()->settingsRepository() : nullptr)
    {
        repository->saveSetting(key, value);
    }
    else if (dataService())
    {
        dataService()->saveSetting(key, value);
    }
}

QVariant SettingsService::load(
    const QString& key,
    const QVariant& defaultValue
    ) const
{
    if (auto* repository = session() ? session()->settingsRepository() : nullptr)
    {
        return repository->loadSetting(key, defaultValue);
    }
    return dataService()
        ? dataService()->loadSetting(key, defaultValue)
        : defaultValue;
}

int TeacherService::create(const Teacher& teacher) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->createTeacher(teacher);
    }
    return dataService() ? dataService()->createTeacher(teacher) : 0;
}

int TeacherService::save(const Teacher& teacher) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->saveTeacher(teacher);
    }
    return dataService() ? dataService()->saveTeacher(teacher) : 0;
}

void TeacherService::update(const Teacher& teacher) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        repository->updateTeacher(teacher);
    }
    else if (dataService())
    {
        dataService()->updateTeacher(teacher);
    }
}

Teacher TeacherService::teacher(int teacherId) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->getTeacher(teacherId);
    }
    return dataService() ? dataService()->getTeacher(teacherId) : Teacher{};
}

QList<Teacher> TeacherService::teachers() const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->getAllTeachers();
    }
    return dataService() ? dataService()->getAllTeachers() : QList<Teacher>{};
}

void TeacherService::remove(int teacherId) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        repository->deleteTeacher(teacherId);
    }
    else if (dataService())
    {
        dataService()->deleteTeacher(teacherId);
    }
}

QList<NativeEnglishTeacher> TeacherService::nativeEnglishTeachers() const
{
    if (auto* repository = session()
            ? session()->nativeEnglishTeacherRepository() : nullptr)
    {
        return repository->getAll();
    }
    return dataService()
        ? dataService()->getNativeEnglishTeachers()
        : QList<NativeEnglishTeacher>{};
}

Status TeacherService::saveNativeEnglishTeacherDirectory(
    const QList<NativeEnglishTeacher>& teachers,
    const QList<int>& deletedIds
    ) const
{
    if (auto* repository = session()
            ? session()->nativeEnglishTeacherRepository() : nullptr)
    {
        return repository->saveDirectory(teachers, deletedIds);
    }
    return dataService()
        ? dataService()->saveNativeEnglishTeacherDirectory(teachers, deletedIds)
        : Status(std::unexpected(unavailableError()));
}

QList<GsTeamMember> TeacherService::gsTeamMembers() const
{
    if (auto* repository = session() ? session()->gsTeamRepository() : nullptr)
    {
        return repository->getAll();
    }
    return dataService() ? dataService()->getGsTeamMembers() : QList<GsTeamMember>{};
}

Status TeacherService::saveGsTeamDirectory(
    const QList<GsTeamMember>& members,
    const QList<int>& deletedIds
    ) const
{
    if (auto* repository = session() ? session()->gsTeamRepository() : nullptr)
    {
        return repository->saveDirectory(members, deletedIds);
    }
    return dataService()
        ? dataService()->saveGsTeamDirectory(members, deletedIds)
        : Status(std::unexpected(unavailableError()));
}

Result<TeacherImportSummary> TeacherService::importTeachers(
    const TeacherImportPlan& plan
    ) const
{
    if (auto* repository = session() ? session()->teacherImportRepository() : nullptr)
    {
        return repository->importTeachers(plan);
    }
    return dataService()
        ? dataService()->importTeachers(plan)
        : Result<TeacherImportSummary>(std::unexpected(unavailableError()));
}

QDate TeacherService::latestImportDate() const
{
    const QString key =
        QString::fromLatin1(TeacherImportRepository::LatestSourceDateSetting);
    if (auto* repository = session() ? session()->settingsRepository() : nullptr)
    {
        return QDate::fromString(
            repository->loadSetting(key, QString()).toString(),
            Qt::ISODate
            );
    }
    return dataService() ? dataService()->latestTeacherImportDate() : QDate{};
}

int ClassService::create(const QString& name) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->createClass(name);
    }
    return dataService() ? dataService()->createClass(name) : 0;
}

QList<Classroom> ClassService::classes() const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->getClasses();
    }
    return dataService() ? dataService()->getClasses() : QList<Classroom>{};
}

Classroom ClassService::classroom(int classId) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->getClassById(classId);
    }
    return dataService() ? dataService()->getClassById(classId) : Classroom{};
}

void ClassService::rename(int classId, const QString& name) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        repository->updateClassName(classId, name);
    }
    else if (dataService())
    {
        dataService()->updateClassName(classId, name);
    }
}

void ClassService::remove(int classId) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        repository->deleteClass(classId);
    }
    else if (dataService())
    {
        dataService()->deleteClass(classId);
    }
}

ClassInfo ClassService::classInfo(int classId) const
{
    if (auto* repository = session() ? session()->classInfoRepository() : nullptr)
    {
        return repository->loadClassInfo(classId);
    }
    if (dataService())
    {
        return dataService()->loadClassInfo(classId);
    }
    ClassInfo info;
    info.classId = classId;
    return info;
}

bool ClassService::saveClassInfo(const ClassInfo& info) const
{
    if (auto* repository = session() ? session()->classInfoRepository() : nullptr)
    {
        return repository->saveClassInfo(info);
    }
    return dataService() && dataService()->saveClassInfo(info);
}

bool ClassService::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    ) const
{
    if (auto* repository = session() ? session()->classInfoRepository() : nullptr)
    {
        return repository->saveClassNotes(
            classId, notes, timeFillerActivities);
    }
    return dataService()
        && dataService()->saveClassNotes(
            classId, notes, timeFillerActivities);
}

QList<ClassConflict> ClassService::conflicts(
    int classId,
    const QList<ClassTime>& times,
    ScheduleType type
    ) const
{
    if (auto* repository = session() ? session()->classInfoRepository() : nullptr)
    {
        return repository->getClassTimeConflicts(classId, times, type);
    }
    return dataService()
        ? dataService()->getClassTimeConflicts(classId, times, type)
        : QList<ClassConflict>{};
}

Result<ClassTransferPackage> ClassService::buildTransferPackage(
    const QList<int>& classIds
    ) const
{
    if (auto* repository = session()
            ? session()->classTransferRepository() : nullptr)
    {
        return repository->buildPackage(classIds);
    }
    return dataService()
        ? dataService()->buildClassTransferPackage(classIds)
        : Result<ClassTransferPackage>(std::unexpected(unavailableError()));
}

Result<ClassImportPreview> ClassService::previewImport(
    const ClassTransferPackage& package
    ) const
{
    if (auto* repository = session()
            ? session()->classTransferRepository() : nullptr)
    {
        return repository->previewImport(package);
    }
    return dataService()
        ? dataService()->previewClassImport(package)
        : Result<ClassImportPreview>(std::unexpected(unavailableError()));
}

Result<ClassImportSummary> ClassService::importClasses(
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    ) const
{
    if (auto* repository = session()
            ? session()->classTransferRepository() : nullptr)
    {
        return repository->importClasses(package, plan);
    }
    return dataService()
        ? dataService()->importClasses(package, plan)
        : Result<ClassImportSummary>(std::unexpected(unavailableError()));
}

Result<ScheduleImportPreview> ScheduleService::previewImport(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    ) const
{
    if (auto* repository = session()
            ? session()->scheduleImportRepository() : nullptr)
    {
        return repository->preview(user, kind);
    }
    return dataService()
        ? dataService()->previewScheduleImport(user, kind)
        : Result<ScheduleImportPreview>(std::unexpected(unavailableError()));
}

Result<ScheduleImportSummary> ScheduleService::importSchedule(
    const ScheduleImportPlan& plan
    ) const
{
    if (auto* repository = session()
            ? session()->scheduleImportRepository() : nullptr)
    {
        return repository->apply(plan);
    }
    return dataService()
        ? dataService()->importSchedule(plan)
        : Result<ScheduleImportSummary>(std::unexpected(unavailableError()));
}

QList<IntensiveSlotState> ScheduleService::intensiveSlotStates() const
{
    if (auto* repository = session()
            ? session()->intensiveSlotStateRepository() : nullptr)
    {
        return repository->loadIntensiveSlotStates();
    }
    return dataService()
        ? dataService()->loadIntensiveSlotStates()
        : QList<IntensiveSlotState>{};
}

void ScheduleService::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state,
    const QString& defaultState
    ) const
{
    if (auto* repository = session()
            ? session()->intensiveSlotStateRepository() : nullptr)
    {
        repository->saveIntensiveSlotState(
            day, startTime, state, defaultState);
    }
    else if (dataService())
    {
        dataService()->saveIntensiveSlotState(
            day, startTime, state, defaultState);
    }
}

Result<QList<TestingAssignment>> ScheduleService::testingAssignments() const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->loadTestingAssignments();
    }
    return dataService()
        ? dataService()->loadTestingAssignments()
        : Result<QList<TestingAssignment>>(std::unexpected(unavailableError()));
}

Result<QList<TestingBlock>> ScheduleService::testingBlocks() const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->loadTestingBlocks();
    }
    return dataService()
        ? dataService()->loadTestingBlocks()
        : Result<QList<TestingBlock>>(std::unexpected(unavailableError()));
}

Status ScheduleService::saveTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room,
    bool replaceExisting
    ) const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->saveTestingBlock(
            day, startTime, room, replaceExisting);
    }
    return dataService()
        ? dataService()->saveTestingBlock(
            day, startTime, room, replaceExisting)
        : Status(std::unexpected(unavailableError()));
}

Status ScheduleService::assignTestingClass(
    const QString& day,
    const QString& startTime,
    int classId,
    bool replaceExisting
    ) const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->assignTestingClass(
            day, startTime, classId, replaceExisting);
    }
    return dataService()
        ? dataService()->assignTestingClass(
            day, startTime, classId, replaceExisting)
        : Status(std::unexpected(unavailableError()));
}

Status ScheduleService::deleteTestingAssignment(
    const QString& day,
    const QString& startTime
    ) const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->deleteTestingAssignment(day, startTime);
    }
    return dataService()
        ? dataService()->deleteTestingAssignment(day, startTime)
        : Status(std::unexpected(unavailableError()));
}

Status ScheduleService::deleteTestingBlock(
    const QString& day,
    const QString& startTime
    ) const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->deleteTestingBlock(day, startTime);
    }
    return dataService()
        ? dataService()->deleteTestingBlock(day, startTime)
        : Status(std::unexpected(unavailableError()));
}

Status ScheduleService::clearTestingAssignments() const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->clearTestingAssignments();
    }
    return dataService()
        ? dataService()->clearTestingAssignments()
        : Status(std::unexpected(unavailableError()));
}

Status ScheduleService::clearTestingBlocks() const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->clearTestingBlocks();
    }
    return dataService()
        ? dataService()->clearTestingBlocks()
        : Status(std::unexpected(unavailableError()));
}

Result<int> ScheduleService::createTestingClass(
    const TestingClass& testingClass,
    const QString& assignmentDay,
    const QString& assignmentStartTime
    ) const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->createTestingClass(
            testingClass, assignmentDay, assignmentStartTime);
    }
    return dataService()
        ? dataService()->createTestingClass(
            testingClass, assignmentDay, assignmentStartTime)
        : Result<int>(std::unexpected(unavailableError()));
}

Status ScheduleService::updateTestingClass(const TestingClass& testingClass) const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->updateTestingClass(testingClass);
    }
    return dataService()
        ? dataService()->updateTestingClass(testingClass)
        : Status(std::unexpected(unavailableError()));
}

Result<TestingClass> ScheduleService::testingClass(int classId) const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->loadTestingClass(classId);
    }
    return dataService()
        ? dataService()->loadTestingClass(classId)
        : Result<TestingClass>(std::unexpected(unavailableError()));
}

Result<QList<TestingClass>> ScheduleService::testingClasses() const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->loadTestingClasses();
    }
    return dataService()
        ? dataService()->loadTestingClasses()
        : Result<QList<TestingClass>>(std::unexpected(unavailableError()));
}

Status ScheduleService::deleteTestingClass(int classId) const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->deleteTestingClass(classId);
    }
    return dataService()
        ? dataService()->deleteTestingClass(classId)
        : Status(std::unexpected(unavailableError()));
}

Result<bool> ScheduleService::isTestingClass(int classId) const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->isTestingClass(classId);
    }
    return dataService()
        ? dataService()->isTestingClass(classId)
        : Result<bool>(std::unexpected(unavailableError()));
}

QList<CalendarEvent> CalendarService::eventsForDate(const QDate& date) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->loadCalendarEventsForDate(date);
    }
    return dataService()
        ? dataService()->loadCalendarEventsForDate(date)
        : QList<CalendarEvent>{};
}

QList<CalendarEvent> CalendarService::eventsInRange(
    const QDate& startDate,
    const QDate& endDate
    ) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->loadCalendarEventsInRange(startDate, endDate);
    }
    return dataService()
        ? dataService()->loadCalendarEventsInRange(startDate, endDate)
        : QList<CalendarEvent>{};
}

QList<CalendarEvent> CalendarService::upcomingEvents(
    const QDate& fromDate,
    int limit
    ) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->loadUpcomingCalendarEvents(fromDate, limit);
    }
    return dataService()
        ? dataService()->loadUpcomingCalendarEvents(fromDate, limit)
        : QList<CalendarEvent>{};
}

CalendarEvent CalendarService::event(int eventId) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->getCalendarEvent(eventId);
    }
    return dataService()
        ? dataService()->getCalendarEvent(eventId)
        : CalendarEvent{};
}

QList<CalendarEvent> CalendarService::repeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    ) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->loadCalendarEventsForRepeatSeriesFromDate(
            repeatSeriesId, startDate);
    }
    return dataService()
        ? dataService()->loadCalendarEventsForRepeatSeriesFromDate(
            repeatSeriesId, startDate)
        : QList<CalendarEvent>{};
}

int CalendarService::saveEvent(const CalendarEvent& event) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->saveCalendarEvent(event);
    }
    return dataService() ? dataService()->saveCalendarEvent(event) : -1;
}

void CalendarService::deleteEvent(int eventId) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        repository->deleteCalendarEvent(eventId);
    }
    else if (dataService())
    {
        dataService()->deleteCalendarEvent(eventId);
    }
}

void CalendarService::deleteRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    ) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        repository->deleteCalendarEventsForRepeatSeriesFromDate(
            repeatSeriesId, startDate);
    }
    else if (dataService())
    {
        dataService()->deleteCalendarEventsForRepeatSeriesFromDate(
            repeatSeriesId, startDate);
    }
}

void CalendarService::deleteAllEvents() const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        repository->deleteAllCalendarEvents();
    }
    else if (dataService())
    {
        dataService()->deleteAllCalendarEvents();
    }
}

void RosterService::saveRoster(int classId, const Roster& roster) const
{
    if (auto* repository = session() ? session()->rosterRepository() : nullptr)
    {
        repository->saveRoster(classId, roster);
    }
    else if (dataService())
    {
        dataService()->saveRoster(classId, roster);
    }
}

bool RosterService::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    ) const
{
    if (auto* repository = session() ? session()->rosterRepository() : nullptr)
    {
        return repository->saveRosters(rosters);
    }
    return dataService() && dataService()->saveRosters(rosters);
}

Roster RosterService::roster(int classId) const
{
    if (auto* repository = session() ? session()->rosterRepository() : nullptr)
    {
        return repository->loadRoster(classId);
    }
    return dataService() ? dataService()->loadRoster(classId) : Roster{};
}

int RosterService::studentCount(int classId) const
{
    if (auto* repository = session() ? session()->rosterRepository() : nullptr)
    {
        return repository->getRosterStudentCount(classId);
    }
    return dataService() ? dataService()->getRosterStudentCount(classId) : 0;
}

bool SpeakingEvaluationService::saveEvaluation(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells
    ) const
{
    if (auto* repository = session()
            ? session()->speakingEvalRepository() : nullptr)
    {
        return repository->saveSpeakingEval(
            classId, evaluationName, rows, dirtyCells);
    }
    return dataService()
        && dataService()->saveSpeakingEval(
            classId, evaluationName, rows, dirtyCells);
}

SpeakingEvalRows SpeakingEvaluationService::evaluation(
    int classId,
    const QString& evaluationName
    ) const
{
    if (auto* repository = session()
            ? session()->speakingEvalRepository() : nullptr)
    {
        return repository->loadSpeakingEval(classId, evaluationName);
    }
    return dataService()
        ? dataService()->loadSpeakingEval(classId, evaluationName)
        : SpeakingEvalRows{};
}

QList<SpeakingEvalScore> SpeakingEvaluationService::rosterScoreImport(
    int classId,
    const QString& evaluationName
    ) const
{
    if (auto* repository = session()
            ? session()->speakingEvalRepository() : nullptr)
    {
        return repository->buildRosterScoreImport(classId, evaluationName);
    }
    return dataService()
        ? dataService()->buildRosterScoreImport(classId, evaluationName)
        : QList<SpeakingEvalScore>{};
}
