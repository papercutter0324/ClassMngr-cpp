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

Status SettingsService::save(
    const QString& key,
    const QVariant& value
    ) const
{
    if (auto* repository = session() ? session()->settingsRepository() : nullptr)
    {
        return repository->saveSetting(key, value);
    }
    if (dataService())
    {
        return dataService()->saveSetting(key, value);
    }

    return std::unexpected(unavailableError());
}

Status SettingsService::saveAll(
    const QVariantMap& values
    ) const
{
    if (auto* repository = session() ? session()->settingsRepository() : nullptr)
    {
        return repository->saveSettings(values);
    }
    if (dataService())
    {
        return dataService()->saveSettings(values);
    }

    return std::unexpected(unavailableError());
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

Result<int> TeacherService::create(const Teacher& teacher) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->createTeacher(teacher);
    }
    return dataService()
        ? dataService()->createTeacher(teacher)
        : Result<int>(std::unexpected(unavailableError()));
}

Result<int> TeacherService::save(const Teacher& teacher) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->saveTeacher(teacher);
    }
    return dataService()
        ? dataService()->saveTeacher(teacher)
        : Result<int>(std::unexpected(unavailableError()));
}

Status TeacherService::update(const Teacher& teacher) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->updateTeacher(teacher);
    }
    else if (dataService())
    {
        return dataService()->updateTeacher(teacher);
    }

    return std::unexpected(unavailableError());
}

Result<Teacher> TeacherService::teacher(int teacherId) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->getTeacher(teacherId);
    }
    return dataService()
        ? dataService()->getTeacher(teacherId)
        : Result<Teacher>(std::unexpected(unavailableError()));
}

Result<QList<Teacher>> TeacherService::teachers() const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->getAllTeachers();
    }
    return dataService()
        ? dataService()->getAllTeachers()
        : Result<QList<Teacher>>(std::unexpected(unavailableError()));
}

Status TeacherService::remove(int teacherId) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->deleteTeacher(teacherId);
    }
    else if (dataService())
    {
        return dataService()->deleteTeacher(teacherId);
    }

    return std::unexpected(unavailableError());
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

Result<int> ClassService::create(const QString& name) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->createClass(name);
    }
    return dataService()
        ? dataService()->createClass(name)
        : Result<int>(std::unexpected(unavailableError()));
}

Result<QList<Classroom>> ClassService::classes() const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->getClasses();
    }
    return dataService()
        ? dataService()->getClasses()
        : Result<QList<Classroom>>(std::unexpected(unavailableError()));
}

Result<Classroom> ClassService::classroom(int classId) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->getClassById(classId);
    }
    return dataService()
        ? dataService()->getClassById(classId)
        : Result<Classroom>(std::unexpected(unavailableError()));
}

Status ClassService::rename(int classId, const QString& name) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->updateClassName(classId, name);
    }
    else if (dataService())
    {
        return dataService()->updateClassName(classId, name);
    }

    return std::unexpected(unavailableError());
}

Status ClassService::remove(int classId) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->deleteClass(classId);
    }
    else if (dataService())
    {
        return dataService()->deleteClass(classId);
    }

    return std::unexpected(unavailableError());
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

Status ClassService::saveClassInfo(const ClassInfo& info) const
{
    if (auto* repository = session() ? session()->classInfoRepository() : nullptr)
    {
        return repository->saveClassInfo(info);
    }
    return dataService()
        ? dataService()->saveClassInfo(info)
        : Status(std::unexpected(unavailableError()));
}

Status ClassService::saveClassNotes(
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
        ? dataService()->saveClassNotes(
            classId, notes, timeFillerActivities)
        : Status(std::unexpected(unavailableError()));
}

Result<QList<ClassConflict>> ClassService::conflicts(
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
        : Result<QList<ClassConflict>>(
            std::unexpected(unavailableError())
            );
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

Status ScheduleService::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state,
    const QString& defaultState
    ) const
{
    if (auto* repository = session()
            ? session()->intensiveSlotStateRepository() : nullptr)
    {
        return repository->saveIntensiveSlotState(
            day, startTime, state, defaultState);
    }
    if (dataService())
    {
        return dataService()->saveIntensiveSlotState(
            day, startTime, state, defaultState);
    }

    return std::unexpected(unavailableError());
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

Result<int> CalendarService::saveEvent(const CalendarEvent& event) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->saveCalendarEvent(event);
    }
    return dataService()
        ? dataService()->saveCalendarEvent(event)
        : Result<int>(std::unexpected(unavailableError()));
}

Result<QList<int>> CalendarService::saveEvents(
    const QList<CalendarEvent>& events
    ) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->saveCalendarEvents(events);
    }
    return dataService()
        ? dataService()->saveCalendarEvents(events)
        : Result<QList<int>>(std::unexpected(unavailableError()));
}

Status CalendarService::deleteEvent(int eventId) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->deleteCalendarEvent(eventId);
    }
    if (dataService())
    {
        return dataService()->deleteCalendarEvent(eventId);
    }

    return std::unexpected(unavailableError());
}

Status CalendarService::deleteRepeatSeriesFromDate(
    const QString& repeatSeriesId,
    const QDate& startDate
    ) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->deleteCalendarEventsForRepeatSeriesFromDate(
            repeatSeriesId, startDate);
    }
    if (dataService())
    {
        return dataService()->deleteCalendarEventsForRepeatSeriesFromDate(
            repeatSeriesId, startDate);
    }

    return std::unexpected(unavailableError());
}

Status CalendarService::deleteAllEvents() const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->deleteAllCalendarEvents();
    }
    if (dataService())
    {
        return dataService()->deleteAllCalendarEvents();
    }

    return std::unexpected(unavailableError());
}

Status RosterService::saveRoster(int classId, const Roster& roster) const
{
    if (auto* repository = session() ? session()->rosterRepository() : nullptr)
    {
        return repository->saveRoster(classId, roster);
    }
    if (dataService())
    {
        return dataService()->saveRoster(classId, roster);
    }

    return std::unexpected(unavailableError());
}

Status RosterService::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    ) const
{
    if (auto* repository = session() ? session()->rosterRepository() : nullptr)
    {
        return repository->saveRosters(rosters);
    }
    return dataService()
        ? dataService()->saveRosters(rosters)
        : Status(std::unexpected(unavailableError()));
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

Status SpeakingEvaluationService::saveEvaluation(
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
        ? dataService()->saveSpeakingEval(
            classId, evaluationName, rows, dirtyCells)
        : Status(std::unexpected(unavailableError()));
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

SpeakingAnalytics::Snapshot SpeakingEvaluationService::analytics(
    int classId,
    const QString& evaluationName
    ) const
{
    const QString name = evaluationName.trimmed();
    QList<SpeakingEvalRows> matrices;

    if (name.isEmpty() || name.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0)
    {
        const QStringList all =
        {
            QStringLiteral("Winter"),
            QStringLiteral("Speech Contest"),
            QStringLiteral("Summer"),
            QStringLiteral("Fall")
        };
        for (const QString& n : all)
        {
            const SpeakingEvalRows one = evaluation(classId, n);
            if (!one.isEmpty())
                matrices.append(one);
        }
    }
    else
    {
        const SpeakingEvalRows one = evaluation(classId, name);
        if (!one.isEmpty())
            matrices.append(one);
    }

    // Averages are calculated against the class roster only: when a specific
    // evaluation is selected, keep the evaluation rows for roster students
    // (matched by name) before computing.
    if (name.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0)
    {
        Roster roster;
        if (auto* repository = session()
                ? session()->rosterRepository() : nullptr)
        {
            roster = repository->loadRoster(classId);
        }
        else if (dataService())
        {
            roster = dataService()->loadRoster(classId);
        }

        if (!roster.rows.isEmpty())
        {
            for (int i = 0; i < matrices.size(); ++i)
            {
                matrices[i] =
                    SpeakingAnalytics::filterMatrixByRoster(matrices.at(i), roster);
            }
        }
    }

    const int rosterCount =
        dataService() ? dataService()->getRosterStudentCount(classId) : 0;

    return SpeakingAnalytics::compute(matrices, rosterCount);
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
