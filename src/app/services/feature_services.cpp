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
#include "domain/validation/calendar_event_validator.h"
#include "domain/validation/class_info_validator.h"
#include "domain/validation/roster_validator.h"
#include "domain/validation/speaking_eval_validator.h"
#include "domain/validation/teacher_validator.h"
#include "domain/validation/validation_rules.h"

#include <QDebug>
#include <QStringList>

namespace
{
QString unavailableError()
{
    return QStringLiteral("No Teacher Profile service is available.");
}

QString validationError(
    const QString& subject,
    const ValidationResult& validation
    )
{
    QStringList details;
    for (const ValidationIssue& issue : validation.errors())
    {
        QString detail = issue.field.isEmpty()
            ? issue.code
            : QStringLiteral("%1: %2").arg(issue.field, issue.code);
        if (issue.row >= 0 && !issue.field.contains(QChar(u'[')))
        {
            detail.prepend(QStringLiteral("row %1, ").arg(issue.row + 1));
        }
        details.append(detail);
    }

    return QStringLiteral("%1 validation failed: %2")
        .arg(subject, details.join(QStringLiteral("; ")));
}

void appendImportedValidation(
    ValidationResult& destination,
    const ValidationResult& source,
    const QString& prefix
    )
{
    for (ValidationIssue issue : source.issues())
    {
        issue.field = issue.field.isEmpty()
            ? prefix
            : QStringLiteral("%1.%2").arg(prefix, issue.field);
        destination.add(std::move(issue));
    }
}

Status validateClassScheduleConflicts(
    const ClassService& service,
    const ClassInfo& info,
    const QList<ClassTime>& times,
    ScheduleType type
    )
{
    if (times.isEmpty())
    {
        return {};
    }

    const Result<QList<ClassConflict>> conflicts =
        service.conflicts(info.classId, times, type);
    if (!conflicts)
    {
        return std::unexpected(conflicts.error());
    }
    if (conflicts->isEmpty())
    {
        return {};
    }

    const ClassConflict& first = conflicts->first();
    return std::unexpected(
        QStringLiteral(
            "Class schedule conflict: %1 %2–%3 conflicts with %4."
            ).arg(
                first.day,
                first.startTime,
                first.endTime,
                first.conflictingClassName
                )
        );
}
}

FeatureService::FeatureService(DataService* dataService)
    : FeatureService(
        dataService ? dataService->databaseSession() : nullptr,
        dataService
        )
{
}

FeatureService::FeatureService(DatabaseSession* session)
    : m_session(session)
{
}

FeatureService::FeatureService(
    DatabaseSession* session,
    DataService* legacyDataService
    )
    : m_session(session ? session
                        : (legacyDataService
                            ? legacyDataService->databaseSession()
                            : nullptr))
{
}

bool FeatureService::isAvailable() const
{
    return m_session && m_session->isOpen();
}

DatabaseSession* FeatureService::session() const
{
    return m_session;
}
DatabaseSession* SettingsService::databaseSession() const
{
    return session();
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

    return std::unexpected(unavailableError());
}

Result<QVariant> SettingsService::load(
    const QString& key
    ) const
{
    if (auto* repository = session() ? session()->settingsRepository() : nullptr)
    {
        return repository->loadSetting(key);
    }
    return Result<QVariant>(std::unexpected(unavailableError()));
}

QVariant SettingsService::loadOrDefault(
    const QString& key,
    const QVariant& defaultValue
    ) const
{
    const Result<QVariant> value = load(key);
    if (!value)
    {
        qWarning() << "Failed to load setting" << key << ':' << value.error();
        return defaultValue;
    }

    return value->isValid() ? *value : defaultValue;
}

Result<int> TeacherService::create(const Teacher& teacher) const
{
    const Teacher normalized = TeacherValidator::normalized(teacher);
    const ValidationResult validation = TeacherValidator::validate(normalized);
    if (validation.hasErrors())
    {
        return std::unexpected(validationError(QStringLiteral("Teacher"), validation));
    }

    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->createTeacher(normalized);
    }
    return Result<int>(std::unexpected(unavailableError()));
}

Result<int> TeacherService::save(const Teacher& teacher) const
{
    const Teacher normalized = TeacherValidator::normalized(teacher);
    const ValidationResult validation = TeacherValidator::validate(normalized);
    if (validation.hasErrors())
    {
        return std::unexpected(validationError(QStringLiteral("Teacher"), validation));
    }

    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->saveTeacher(normalized);
    }
    return Result<int>(std::unexpected(unavailableError()));
}

Status TeacherService::update(const Teacher& teacher) const
{
    const Teacher normalized = TeacherValidator::normalized(teacher);
    const ValidationResult validation = TeacherValidator::validate(normalized);
    if (validation.hasErrors())
    {
        return std::unexpected(validationError(QStringLiteral("Teacher"), validation));
    }
    if (normalized.id <= 0)
    {
        return std::unexpected(
            QStringLiteral("Teacher validation failed: id must be positive.")
            );
    }

    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->updateTeacher(normalized);
    }

    return std::unexpected(unavailableError());
}

Result<Teacher> TeacherService::teacher(int teacherId) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->getTeacher(teacherId);
    }
    return Result<Teacher>(std::unexpected(unavailableError()));
}

Result<QList<Teacher>> TeacherService::teachers() const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->getAllTeachers();
    }
    return Result<QList<Teacher>>(std::unexpected(unavailableError()));
}

Status TeacherService::remove(int teacherId) const
{
    if (auto* repository = session() ? session()->teacherRepository() : nullptr)
    {
        return repository->deleteTeacher(teacherId);
    }

    return std::unexpected(unavailableError());
}

Result<QList<NativeEnglishTeacher>> TeacherService::nativeEnglishTeachers() const
{
    if (auto* repository = session()
            ? session()->nativeEnglishTeacherRepository() : nullptr)
    {
        return repository->getAll();
    }
    return Result<QList<NativeEnglishTeacher>>(std::unexpected(unavailableError()));
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
    return Status(std::unexpected(unavailableError()));
}

Result<QList<GsTeamMember>> TeacherService::gsTeamMembers() const
{
    if (auto* repository = session() ? session()->gsTeamRepository() : nullptr)
    {
        return repository->getAll();
    }
    return Result<QList<GsTeamMember>>(std::unexpected(unavailableError()));
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
    return Status(std::unexpected(unavailableError()));
}

Result<TeacherImportSummary> TeacherService::importTeachers(
    const TeacherImportPlan& plan
    ) const
{
    TeacherImportPlan normalizedPlan = plan;
    ValidationResult validation;
    for (int index = 0; index < normalizedPlan.koreanTeachers.size(); ++index)
    {
        Teacher& teacher = normalizedPlan.koreanTeachers[index];
        teacher = TeacherValidator::normalized(teacher);
        appendImportedValidation(
            validation,
            TeacherValidator::validate(teacher),
            QStringLiteral("koreanTeachers[%1]").arg(index)
            );
    }
    if (validation.hasErrors())
    {
        return std::unexpected(
            validationError(QStringLiteral("Teacher import"), validation)
            );
    }

    if (auto* repository = session() ? session()->teacherImportRepository() : nullptr)
    {
        return repository->importTeachers(normalizedPlan);
    }
    return Result<TeacherImportSummary>(std::unexpected(unavailableError()));
}

Result<QDate> TeacherService::latestImportDate() const
{
    const QString key =
        QString::fromLatin1(TeacherImportRepository::LatestSourceDateSetting);
    if (auto* repository = session() ? session()->settingsRepository() : nullptr)
    {
        const Result<QVariant> value = repository->loadSetting(key);
        if (!value)
        {
            return std::unexpected(value.error());
        }

        return QDate::fromString(value->toString(), Qt::ISODate);
    }
    return Result<QDate>(std::unexpected(unavailableError()));
}

Result<int> ClassService::create(const QString& name) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->createClass(name);
    }
    return Result<int>(std::unexpected(unavailableError()));
}

Result<QList<Classroom>> ClassService::classes() const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->getClasses();
    }
    return Result<QList<Classroom>>(std::unexpected(unavailableError()));
}

Result<QList<ClassTeacherAssignment>>
ClassService::classTeacherAssignments() const
{
    if (auto* repository = session() ? session()->classInfoRepository() : nullptr)
    {
        return repository->loadClassTeacherAssignments();
    }
    return std::unexpected(unavailableError());
}

Result<QList<ClassInfo>> ClassService::scheduleClassInfos() const
{
    if (auto* repository = session() ? session()->classInfoRepository() : nullptr)
    {
        return repository->loadScheduleClassInfos();
    }
    return std::unexpected(unavailableError());
}

Result<Classroom> ClassService::classroom(int classId) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->getClassById(classId);
    }
    return Result<Classroom>(std::unexpected(unavailableError()));
}

Status ClassService::rename(int classId, const QString& name) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->updateClassName(classId, name);
    }

    return std::unexpected(unavailableError());
}

Status ClassService::remove(int classId) const
{
    if (auto* repository = session() ? session()->classRepository() : nullptr)
    {
        return repository->deleteClass(classId);
    }

    return std::unexpected(unavailableError());
}

Result<ClassInfo> ClassService::classInfo(int classId) const
{
    if (auto* repository = session() ? session()->classInfoRepository() : nullptr)
    {
        return repository->loadClassInfo(classId);
    }
    return std::unexpected(unavailableError());
}

Status ClassService::saveClassInfo(const ClassInfo& info) const
{
    const ClassInfo normalized = ClassInfoValidator::normalized(info);
    const ValidationResult validation = ClassInfoValidator::validate(normalized);
    if (validation.hasErrors())
    {
        return std::unexpected(
            validationError(QStringLiteral("Class information"), validation)
            );
    }

    const Status regularConflicts = validateClassScheduleConflicts(
        *this,
        normalized,
        normalized.classTimes,
        ScheduleType::Regular
        );
    if (!regularConflicts)
    {
        return regularConflicts;
    }

    const Status intensiveConflicts = validateClassScheduleConflicts(
        *this,
        normalized,
        normalized.intensiveTimes,
        ScheduleType::Intensive
        );
    if (!intensiveConflicts)
    {
        return intensiveConflicts;
    }

    if (auto* repository = session() ? session()->classInfoRepository() : nullptr)
    {
        return repository->saveClassInfo(normalized);
    }
    return Status(std::unexpected(unavailableError()));
}

Status ClassService::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    ) const
{
    const QString normalizedNotes = notes.trimmed();
    const QString normalizedActivities = timeFillerActivities.trimmed();
    const ValidationResult validation = ClassInfoValidator::validateNotes(
        classId,
        normalizedNotes,
        normalizedActivities
        );
    if (validation.hasErrors())
    {
        return std::unexpected(
            validationError(QStringLiteral("Class notes"), validation)
            );
    }

    if (auto* repository = session() ? session()->classInfoRepository() : nullptr)
    {
        return repository->saveClassNotes(
            classId, normalizedNotes, normalizedActivities);
    }
    return Status(std::unexpected(unavailableError()));
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
    return Result<QList<ClassConflict>>(
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
    return Result<ClassTransferPackage>(std::unexpected(unavailableError()));
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
    return Result<ClassImportPreview>(std::unexpected(unavailableError()));
}

Result<ClassImportSummary> ClassService::importClasses(
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    ) const
{
    ClassTransferPackage normalizedPackage = package;
    ValidationResult validation;

    for (int index = 0; index < normalizedPackage.teachers.size(); ++index)
    {
        ClassTransferTeacher& teacher = normalizedPackage.teachers[index];
        teacher.teacher = TeacherValidator::normalized(teacher.teacher);
        appendImportedValidation(
            validation,
            TeacherValidator::validate(teacher.teacher),
            QStringLiteral("teachers[%1]").arg(index)
            );
    }

    for (int index = 0; index < normalizedPackage.classes.size(); ++index)
    {
        ClassTransferClass& transferredClass = normalizedPackage.classes[index];
        transferredClass.info = ClassInfoValidator::normalized(
            transferredClass.info
            );
        appendImportedValidation(
            validation,
            ClassInfoValidator::validate(transferredClass.info),
            QStringLiteral("classes[%1].info").arg(index)
            );
    }

    if (validation.hasErrors())
    {
        return std::unexpected(
            validationError(QStringLiteral("Class import"), validation)
            );
    }

    if (auto* repository = session()
            ? session()->classTransferRepository() : nullptr)
    {
        return repository->importClasses(normalizedPackage, plan);
    }
    return Result<ClassImportSummary>(std::unexpected(unavailableError()));
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
    return Result<ScheduleImportPreview>(std::unexpected(unavailableError()));
}

Status ScheduleService::validateImport(
    const ScheduleImportPlan& plan
    ) const
{
    if (auto* repository = session()
            ? session()->scheduleImportRepository() : nullptr)
    {
        return repository->validateImport(plan);
    }
    return Status(std::unexpected(unavailableError()));
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
    return Result<ScheduleImportSummary>(std::unexpected(unavailableError()));
}

Result<QList<IntensiveSlotState>> ScheduleService::intensiveSlotStates() const
{
    if (auto* repository = session()
            ? session()->intensiveSlotStateRepository() : nullptr)
    {
        return repository->loadIntensiveSlotStates();
    }
    return Result<QList<IntensiveSlotState>>(std::unexpected(unavailableError()));
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

    return std::unexpected(unavailableError());
}

Result<QList<TestingAssignment>> ScheduleService::testingAssignments() const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->loadTestingAssignments();
    }
    return Result<QList<TestingAssignment>>(std::unexpected(unavailableError()));
}

Result<QList<TestingBlock>> ScheduleService::testingBlocks() const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->loadTestingBlocks();
    }
    return Result<QList<TestingBlock>>(std::unexpected(unavailableError()));
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
    return Status(std::unexpected(unavailableError()));
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
    return Status(std::unexpected(unavailableError()));
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
    return Status(std::unexpected(unavailableError()));
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
    return Status(std::unexpected(unavailableError()));
}

Status ScheduleService::clearTestingAssignments() const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->clearTestingAssignments();
    }
    return Status(std::unexpected(unavailableError()));
}

Status ScheduleService::clearTestingBlocks() const
{
    if (auto* repository = session()
            ? session()->testingBlockRepository() : nullptr)
    {
        return repository->clearTestingBlocks();
    }
    return Status(std::unexpected(unavailableError()));
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
    return Result<int>(std::unexpected(unavailableError()));
}

Status ScheduleService::updateTestingClass(const TestingClass& testingClass) const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->updateTestingClass(testingClass);
    }
    return Status(std::unexpected(unavailableError()));
}

Result<TestingClass> ScheduleService::testingClass(int classId) const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->loadTestingClass(classId);
    }
    return Result<TestingClass>(std::unexpected(unavailableError()));
}

Result<QList<TestingClass>> ScheduleService::testingClasses() const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->loadTestingClasses();
    }
    return Result<QList<TestingClass>>(std::unexpected(unavailableError()));
}

Status ScheduleService::deleteTestingClass(int classId) const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->deleteTestingClass(classId);
    }
    return Status(std::unexpected(unavailableError()));
}

Result<bool> ScheduleService::isTestingClass(int classId) const
{
    if (auto* repository = session()
            ? session()->testingClassRepository() : nullptr)
    {
        return repository->isTestingClass(classId);
    }
    return Result<bool>(std::unexpected(unavailableError()));
}

Result<QList<CalendarEvent>> CalendarService::eventsForDate(const QDate& date) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->loadCalendarEventsForDate(date);
    }
    return Result<QList<CalendarEvent>>(std::unexpected(unavailableError()));
}

Result<QList<CalendarEvent>> CalendarService::eventsInRange(
    const QDate& startDate,
    const QDate& endDate
    ) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->loadCalendarEventsInRange(startDate, endDate);
    }
    return Result<QList<CalendarEvent>>(std::unexpected(unavailableError()));
}

Result<QList<CalendarEvent>> CalendarService::upcomingEvents(
    const QDate& fromDate,
    int limit
    ) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->loadUpcomingCalendarEvents(fromDate, limit);
    }
    return Result<QList<CalendarEvent>>(std::unexpected(unavailableError()));
}

Result<CalendarEvent> CalendarService::event(int eventId) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->getCalendarEvent(eventId);
    }
    return Result<CalendarEvent>(std::unexpected(unavailableError()));
}

Result<QList<CalendarEvent>> CalendarService::repeatSeriesFromDate(
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
    return Result<QList<CalendarEvent>>(std::unexpected(unavailableError()));
}

Result<int> CalendarService::saveEvent(const CalendarEvent& event) const
{
    const CalendarEvent normalized = CalendarEventValidator::normalized(event);
    const ValidationResult validation = CalendarEventValidator::validate(normalized);
    if (validation.hasErrors())
    {
        return std::unexpected(
            validationError(QStringLiteral("Calendar event"), validation)
            );
    }

    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->saveCalendarEvent(normalized);
    }
    return Result<int>(std::unexpected(unavailableError()));
}

Result<QList<int>> CalendarService::saveEvents(
    const QList<CalendarEvent>& events
    ) const
{
    QList<CalendarEvent> normalizedEvents;
    normalizedEvents.reserve(events.size());
    for (const CalendarEvent& event : events)
    {
        normalizedEvents.append(CalendarEventValidator::normalized(event));
    }

    const ValidationResult validation = CalendarEventValidator::validateSeries(
        normalizedEvents
        );
    if (validation.hasErrors())
    {
        return std::unexpected(
            validationError(QStringLiteral("Calendar events"), validation)
            );
    }

    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->saveCalendarEvents(normalizedEvents);
    }
    return Result<QList<int>>(std::unexpected(unavailableError()));
}

Result<CalendarEventImportSummary> CalendarService::importEvents(
    const QList<CalendarEvent>& events,
    int parserSkippedCount
    ) const
{
    if (parserSkippedCount < 0)
    {
        return std::unexpected(
            QStringLiteral("Calendar import skipped count is invalid.")
            );
    }

    if (events.isEmpty())
    {
        return CalendarEventImportSummary{
            0,
            parserSkippedCount
        };
    }

    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->importCalendarEvents(events, parserSkippedCount);
    }
    return Result<CalendarEventImportSummary>(
            std::unexpected(unavailableError())
            );
}

Status CalendarService::deleteEvent(int eventId) const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->deleteCalendarEvent(eventId);
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

    return std::unexpected(unavailableError());
}

Status CalendarService::deleteAllEvents() const
{
    if (auto* repository = session()
            ? session()->calendarEventRepository() : nullptr)
    {
        return repository->deleteAllCalendarEvents();
    }

    return std::unexpected(unavailableError());
}

Status RosterService::saveRoster(
    int classId,
    const Roster& roster,
    bool allowQuestionableKoreanNameLengths
    ) const
{
    const Roster normalized = RosterValidator::normalized(roster);
    ValidationResult validation = RosterValidator::validate(
        normalized,
        allowQuestionableKoreanNameLengths
        );
    if (classId <= 0)
    {
        validation.add(ValidationRules::issue(
            QStringLiteral("roster.class_id.invalid"),
            {.field = QStringLiteral("classId")},
            ValidationSeverity::Error,
            {{QStringLiteral("value"), classId}}
            ));
    }
    if (validation.hasErrors())
    {
        return std::unexpected(
            validationError(QStringLiteral("Roster"), validation)
            );
    }

    if (auto* repository = session() ? session()->rosterRepository() : nullptr)
    {
        return repository->saveRoster(classId, normalized);
    }

    return std::unexpected(unavailableError());
}

Status RosterService::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    ) const
{
    QList<QPair<int, Roster>> normalizedRosters;
    normalizedRosters.reserve(rosters.size());
    ValidationResult validation;
    for (int index = 0; index < rosters.size(); ++index)
    {
        const int classId = rosters[index].first;
        const Roster normalized = RosterValidator::normalized(
            rosters[index].second
            );
        normalizedRosters.append({classId, normalized});

        if (classId <= 0)
        {
            validation.add(ValidationRules::issue(
                QStringLiteral("roster.class_id.invalid"),
                {.field = QStringLiteral("rosters[%1].classId").arg(index)},
                ValidationSeverity::Error,
                {{QStringLiteral("value"), classId}}
                ));
        }
        appendImportedValidation(
            validation,
            RosterValidator::validate(normalized),
            QStringLiteral("rosters[%1]").arg(index)
            );
    }
    if (validation.hasErrors())
    {
        return std::unexpected(
            validationError(QStringLiteral("Roster batch"), validation)
            );
    }

    if (auto* repository = session() ? session()->rosterRepository() : nullptr)
    {
        return repository->saveRosters(normalizedRosters);
    }
    return Status(std::unexpected(unavailableError()));
}

Result<Roster> RosterService::roster(int classId) const
{
    if (auto* repository = session() ? session()->rosterRepository() : nullptr)
    {
        return repository->loadRoster(classId);
    }
    return Result<Roster>(std::unexpected(unavailableError()));
}

Result<int> RosterService::studentCount(int classId) const
{
    if (auto* repository = session() ? session()->rosterRepository() : nullptr)
    {
        return repository->getRosterStudentCount(classId);
    }
    return Result<int>(std::unexpected(unavailableError()));
}

Status SpeakingEvaluationService::saveEvaluation(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells,
    bool allowQuestionableKoreanNameLengths
    ) const
{
    const QString normalizedName = evaluationName.trimmed();
    const SpeakingEvalRows normalizedRows = SpeakingEvalValidator::normalized(
        rows
        );
    const ValidationResult validation = SpeakingEvalValidator::validate(
        classId,
        normalizedName,
        normalizedRows,
        allowQuestionableKoreanNameLengths
        );
    if (validation.hasErrors())
    {
        return std::unexpected(
            validationError(QStringLiteral("Speaking evaluation"), validation)
            );
    }

    if (auto* repository = session()
            ? session()->speakingEvalRepository() : nullptr)
    {
        return repository->saveSpeakingEval(
            classId, normalizedName, normalizedRows, dirtyCells);
    }
    return Status(std::unexpected(unavailableError()));
}

Result<SpeakingEvalRows> SpeakingEvaluationService::evaluation(
    int classId,
    const QString& evaluationName
    ) const
{
    if (auto* repository = session()
            ? session()->speakingEvalRepository() : nullptr)
    {
        return repository->loadSpeakingEval(classId, evaluationName);
    }
    return Result<SpeakingEvalRows>(std::unexpected(unavailableError()));
}

Result<SpeakingAnalytics::Snapshot> SpeakingEvaluationService::analytics(
    int classId,
    const QString& evaluationName
    ) const
{
    const Result<SpeakingEvaluationDashboard> dashboard =
        analyticsDashboard(classId, evaluationName);
    if (!dashboard)
    {
        return std::unexpected(dashboard.error());
    }

    return dashboard->selectedSnapshot;
}

Result<SpeakingEvaluationDashboard> SpeakingEvaluationService::analyticsDashboard(
    int classId,
    const QString& evaluationName
    ) const
{
    Result<Roster> loadedRoster =
        std::unexpected(unavailableError());
    if (auto* repository = session()
            ? session()->rosterRepository() : nullptr)
    {
        loadedRoster = repository->loadRoster(classId);
    }
    if (!loadedRoster)
    {
        return std::unexpected(loadedRoster.error());
    }

    QList<SpeakingAnalytics::Evaluation> evaluations;
    evaluations.reserve(SpeakingAnalytics::evaluationNames().size());
    for (const QString& evaluationName : SpeakingAnalytics::evaluationNames())
    {
        const Result<SpeakingEvalRows> raw =
            evaluation(classId, evaluationName);
        if (!raw)
        {
            return std::unexpected(raw.error());
        }

        evaluations.append({evaluationName, *raw});
    }

    SpeakingAnalytics::Dashboard portableDashboard =
        SpeakingAnalytics::buildDashboard(
            evaluationName.trimmed(),
            *loadedRoster,
            evaluations
            );

    SpeakingEvaluationDashboard dashboard;
    dashboard.selectedSnapshot = std::move(
        portableDashboard.selectedSnapshot);
    dashboard.classShapeEvaluationName = std::move(
        portableDashboard.classShapeEvaluationName);
    dashboard.classShapeSnapshot = std::move(
        portableDashboard.classShapeSnapshot);
    dashboard.yearToDatePoints = std::move(
        portableDashboard.yearToDatePoints);
    return dashboard;
}

Result<QList<SpeakingEvalScore>> SpeakingEvaluationService::rosterScoreImport(
    int classId,
    const QString& evaluationName
    ) const
{
    if (auto* repository = session()
            ? session()->speakingEvalRepository() : nullptr)
    {
        return repository->buildRosterScoreImport(classId, evaluationName);
    }
    return Result<QList<SpeakingEvalScore>>(std::unexpected(unavailableError()));
}
