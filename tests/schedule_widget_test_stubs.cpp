#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/fontmanager.h"
#include "core/theme_service.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "data/repositories/campus_record_repository.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/class_transfer_repository.h"
#include "data/repositories/intensive_slot_state_repository.h"
#include "data/repositories/gs_team_repository.h"
#include "data/repositories/native_english_teacher_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/schedule_import_repository.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/teacher_import_repository.h"
#include "data/repositories/testing_block_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/schedule_testing_assignment_read_query.h"
#include "next/application/schedule_testing_assignment_save.h"
#include "features/schedule/ui/schedule_editor_dialog.h"
#include "features/schedule/ui/schedule_print_dialog.h"
#include "features/schedule/services/schedule_print_service.h"
#include "features/schedule/ui/schedule_view_model.h"
#include "features/roster/services/roster_template_print_service.h"
#include "ui/shared/printing/pdf_print_service.h"

#include <QHash>
#include <QLabel>
#include <QSet>
#include <QTime>
#include <QTimer>

#include <utility>

namespace ScheduleWidgetTestStubs
{
QHash<QString, QVariant> settings;
QHash<int, QString> classGrades;
QHash<QString, QString> testingBlocks;
QHash<int, TestingClass> testingClasses;
QHash<QString, int> testingClassAssignments;
QHash<int, Roster> rosters;
QHash<QString, SpeakingEvalRows> speakingEvaluations;
int savedSlotStates = 0;
int scheduleClassInfoReadCount = 0;
int slotStateReadCount = 0;
int testingAssignmentsReadCount = 0;
int testingAssignmentWriteCount = 0;
ClassMngr::Next::Application::ScheduleTestingAssignmentMutation
    lastTestingAssignmentMutation =
        ClassMngr::Next::Application::
            ScheduleTestingAssignmentMutation::SavePlainTesting;
QString lastTestingAssignmentWriteDay;
QString lastTestingAssignmentWriteStartTime;
QString lastTestingAssignmentWriteRoom;
int lastTestingAssignmentWriteClassId = -1;
bool lastTestingAssignmentWriteReplaceExisting = false;
TestingAssignmentDisplayReadMetrics testingAssignmentDisplayReadMetrics;
QList<IntensiveSlotState> intensiveSlotStates;
QString intensiveSlotStateReadFailure;
int savedTestingBlocks = 0;
int printRequestCount = 0;
bool lastPrintRequestShowsEnglishNames = false;
Theme lastPrintRequestTheme = Theme::Dark;
QString lastPrintRequestUserName;
QString lastSavedSlotDay;
QString lastSavedSlotStartTime;
QString lastSavedSlotState;
QString lastSavedSlotDefaultState;
Status slotSaveResult;
Theme configuredTheme = Theme::Dark;
bool themeAvailable = false;
bool databaseOpen = true;
bool intensiveSlotStateRepositoryAvailable = true;
bool testingAssignmentRepositoryAvailable = true;
QString testingAssignmentReadFailure;
QString testingAssignmentWriteFailure;
bool includeAdditionalClass = false;
bool includeMiddleSchoolClasses = false;
bool matchImportedClasses = false;
bool possibleImportedClasses = false;
bool existingIntensiveHours = false;
bool distinctIntensiveDays = false;
bool includeAlternativeMatchingClass = false;
bool classesNavigationReadFailure = false;
bool scheduleClassInfoReadFailure = false;

void reset()
{
    settings.clear();
    classGrades.clear();
    testingBlocks.clear();
    testingClasses.clear();
    testingClassAssignments.clear();
    rosters.clear();
    speakingEvaluations.clear();
    savedSlotStates = 0;
    scheduleClassInfoReadCount = 0;
    slotStateReadCount = 0;
    testingAssignmentsReadCount = 0;
    testingAssignmentWriteCount = 0;
    lastTestingAssignmentMutation =
        ClassMngr::Next::Application::
            ScheduleTestingAssignmentMutation::SavePlainTesting;
    lastTestingAssignmentWriteDay.clear();
    lastTestingAssignmentWriteStartTime.clear();
    lastTestingAssignmentWriteRoom.clear();
    lastTestingAssignmentWriteClassId = -1;
    lastTestingAssignmentWriteReplaceExisting = false;
    testingAssignmentDisplayReadMetrics = {};
    intensiveSlotStates.clear();
    intensiveSlotStateReadFailure.clear();
    savedTestingBlocks = 0;
    printRequestCount = 0;
    lastPrintRequestShowsEnglishNames = false;
    lastPrintRequestTheme = Theme::Dark;
    lastPrintRequestUserName.clear();
    lastSavedSlotDay.clear();
    lastSavedSlotStartTime.clear();
    lastSavedSlotState.clear();
    lastSavedSlotDefaultState.clear();
    slotSaveResult = {};
    configuredTheme = Theme::Dark;
    themeAvailable = false;
    databaseOpen = true;
    intensiveSlotStateRepositoryAvailable = true;
    testingAssignmentRepositoryAvailable = true;
    testingAssignmentReadFailure.clear();
    testingAssignmentWriteFailure.clear();
    includeAdditionalClass = false;
    includeMiddleSchoolClasses = false;
    matchImportedClasses = false;
    possibleImportedClasses = false;
    existingIntensiveHours = false;
    distinctIntensiveDays = false;
    includeAlternativeMatchingClass = false;
    classesNavigationReadFailure = false;
    scheduleClassInfoReadFailure = false;
}

void setDatabaseOpen(
    bool open
    )
{
    databaseOpen = open;
}

void setIntensiveSlotStates(QList<IntensiveSlotState> states)
{
    intensiveSlotStates = std::move(states);
}

void setIntensiveSlotStateReadFailure(const QString& error)
{
    intensiveSlotStateReadFailure = error;
}

void setIntensiveSlotStateRepositoryAvailable(const bool available)
{
    intensiveSlotStateRepositoryAvailable = available;
}

void setTestingAssignmentReadFailure(const QString& error)
{
    testingAssignmentReadFailure = error;
}

void setTestingAssignmentRepositoryAvailable(const bool available)
{
    testingAssignmentRepositoryAvailable = available;
}

void setTestingAssignmentWriteFailure(const QString& error)
{
    testingAssignmentWriteFailure = error;
}

void setSlotSaveFailure(
    const QString& error
    )
{
    slotSaveResult = std::unexpected(error);
}

void setScheduleClassInfoReadFailure(const bool fail)
{
    scheduleClassInfoReadFailure = fail;
}

void setIncludeAdditionalClass(
    bool include
    )
{
    includeAdditionalClass = include;
}

void setIncludeMiddleSchoolClasses(
    bool include
    )
{
    includeMiddleSchoolClasses = include;
}

void setClassGrade(
    int classId,
    const QString& grade
    )
{
    classGrades.insert(classId, grade);
}

void setMatchImportedClasses(
    bool match
    )
{
    matchImportedClasses = match;
}

void setPossibleImportedClasses(
    bool match
    )
{
    possibleImportedClasses = match;
}

void setExistingIntensiveHours(
    bool exists
    )
{
    existingIntensiveHours = exists;
}

void setDistinctIntensiveDays(
    bool distinct
    )
{
    distinctIntensiveDays = distinct;
}

void setClassesNavigationReadFailure(
    bool fails
    )
{
    classesNavigationReadFailure = fails;
}

void setIncludeAlternativeMatchingClass(
    bool include
    )
{
    includeAlternativeMatchingClass = include;
}

void setCurrentTheme(
    Theme theme
    )
{
    configuredTheme = theme;
    themeAvailable = true;
}

void setSpeakingEvaluation(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows
    )
{
    speakingEvaluations.insert(
        QStringLiteral("%1:%2").arg(classId).arg(evaluationName),
        rows);
}

QString settingValue(
    const QString& key
    )
{
    return settings.value(key).toString();
}

void setTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room
    )
{
    testingBlocks.insert(
        day + QLatin1Char('\x1f') + startTime,
        room
        );
}

void setTestingClassAssignment(
    const QString& day,
    const QString& startTime,
    const TestingClass& testingClass
    )
{
    testingClasses.insert(
        testingClass.classId,
        testingClass
        );
    testingClassAssignments.insert(
        day + QLatin1Char('\x1f') + startTime,
        testingClass.classId
        );
}

void setTestingClass(const TestingClass& testingClass)
{
    testingClasses.insert(testingClass.classId, testingClass);
}

void setUnresolvedTestingClassAssignment(
    const QString& day,
    const QString& startTime,
    const int classId
    )
{
    testingClassAssignments.insert(
        day + QLatin1Char('\x1f') + startTime,
        classId
        );
}
}

ApplicationServices::ApplicationServices()
{
    m_databaseSession = std::make_unique<DatabaseSession>();
    m_dataService =
        std::make_unique<DataService>();
}

ApplicationServices::~ApplicationServices() = default;

bool DatabaseSession::isOpen() const
{
    return ScheduleWidgetTestStubs::databaseOpen;
}

IntensiveSlotStateRepository* DatabaseSession::
intensiveSlotStateRepository() const
{
    if (!ScheduleWidgetTestStubs::intensiveSlotStateRepositoryAvailable)
    {
        return nullptr;
    }

    static QSqlDatabase database;
    static IntensiveSlotStateRepository repository(database);
    return &repository;
}

IntensiveSlotStateRepository::IntensiveSlotStateRepository(
    QSqlDatabase& database
    )
    : m_database(database)
{
}

Result<QList<IntensiveSlotState>>
IntensiveSlotStateRepository::loadIntensiveSlotStates()
{
    ++ScheduleWidgetTestStubs::slotStateReadCount;
    if (!ScheduleWidgetTestStubs::intensiveSlotStateReadFailure.isEmpty())
    {
        return std::unexpected(
            ScheduleWidgetTestStubs::intensiveSlotStateReadFailure
            );
    }

    return ScheduleWidgetTestStubs::intensiveSlotStates;
}

TestingBlockRepository* DatabaseSession::testingBlockRepository() const
{
    if (!ScheduleWidgetTestStubs::testingAssignmentRepositoryAvailable)
    {
        return nullptr;
    }

    static QSqlDatabase database;
    static TestingBlockRepository repository(database);
    return &repository;
}

TestingBlockRepository::TestingBlockRepository(QSqlDatabase& database)
    : m_database(database)
{
}

Result<QList<TestingAssignmentDisplayRecord>>
TestingBlockRepository::loadTestingAssignmentDisplayRecords()
{
    ++ScheduleWidgetTestStubs::testingAssignmentsReadCount;
    ++ScheduleWidgetTestStubs::testingAssignmentDisplayReadMetrics.callCount;
    ++ScheduleWidgetTestStubs::testingAssignmentDisplayReadMetrics.statementCount;
    if (!ScheduleWidgetTestStubs::testingAssignmentReadFailure.isEmpty())
    {
        return std::unexpected(
            ScheduleWidgetTestStubs::testingAssignmentReadFailure
            );
    }

    QList<TestingAssignmentDisplayRecord> rows;
    for (
        auto iterator = ScheduleWidgetTestStubs::testingBlocks.cbegin();
        iterator != ScheduleWidgetTestStubs::testingBlocks.cend();
        ++iterator
        )
    {
        const QStringList keyParts =
            iterator.key().split(QLatin1Char('\x1f'));
        if (keyParts.size() != 2)
        {
            continue;
        }

        TestingAssignmentDisplayRecord row;
        row.day = keyParts.at(0);
        row.startTime = keyParts.at(1);
        row.room = iterator.value();
        rows.append(std::move(row));
    }

    for (
        auto iterator =
            ScheduleWidgetTestStubs::testingClassAssignments.cbegin();
        iterator !=
            ScheduleWidgetTestStubs::testingClassAssignments.cend();
        ++iterator
        )
    {
        const QStringList keyParts =
            iterator.key().split(QLatin1Char('\x1f'));
        if (keyParts.size() != 2)
        {
            continue;
        }

        TestingAssignmentDisplayRecord row;
        row.day = keyParts.at(0);
        row.startTime = keyParts.at(1);
        row.classId = iterator.value();
        const auto testingClass =
            ScheduleWidgetTestStubs::testingClasses.constFind(row.classId);
        if (testingClass != ScheduleWidgetTestStubs::testingClasses.cend())
        {
            row.hasSpecialClass = true;
            row.className = testingClass->name;
            row.testingClassRoom = testingClass->room;
            row.grade = testingClass->grade;
            row.level = testingClass->level;
            row.classColor = testingClass->classColor;
            row.fontColor = testingClass->fontColor;
            if (testingClass->teacherId > 0)
            {
                const Teacher teacher = DataService().getTeacher(
                    testingClass->teacherId
                    ).value_or(Teacher{});
                row.teacherKoreanName = teacher.teacherKr;
                row.teacherEnglishName = teacher.teacherEn;
                row.teacherPreferredName =
                    teacher.preferredDisplayName();
            }
        }
        rows.append(std::move(row));
    }

    return rows;
}

const TestingAssignmentDisplayReadMetrics&
TestingBlockRepository::testingAssignmentDisplayReadMetrics() const noexcept
{
    return ScheduleWidgetTestStubs::testingAssignmentDisplayReadMetrics;
}

ClassInfoRepository* DatabaseSession::classInfoRepository() const
{
    static QSqlDatabase database;
    static ClassInfoRepository repository(database);
    return &repository;
}

ClassInfoRepository::ClassInfoRepository(QSqlDatabase& database)
    : m_database(database)
{
}

Result<QList<ClassNavigationReadRecord>>
ClassInfoRepository::loadClassesNavigationRecords(
    const QList<int>& classIds
    )
{
    if (ScheduleWidgetTestStubs::classesNavigationReadFailure)
    {
        return std::unexpected(
            QStringLiteral("Classes navigation read failed.")
            );
    }

    QList<ClassNavigationReadRecord> records;
    records.reserve(classIds.size());
    for (const int classId : classIds)
    {
        ClassNavigationReadRecord record;
        record.classId = classId;
        record.hasClassInfo = true;

        const TestingClass testingClass =
            ScheduleWidgetTestStubs::testingClasses.value(classId);
        if (ScheduleWidgetTestStubs::testingClasses.contains(classId))
        {
            record.grade = testingClass.grade;
            record.level = testingClass.level;
            if (testingClass.teacherId > 0)
            {
                const Teacher teacher = DataService().getTeacher(
                    testingClass.teacherId
                    ).value_or(Teacher{});
                record.teacherEnglishName = teacher.teacherEn;
                record.teacherKoreanName = teacher.teacherKr;
            }
        }
        else
        {
            record.grade = ScheduleWidgetTestStubs::classGrades.value(
                classId,
                classId == 43
                    ? QStringLiteral("E5")
                    : QStringLiteral("E4")
                );
            record.level = record.grade == QStringLiteral("E5")
                ? QStringLiteral("Athena")
                : QStringLiteral("Hercules");
            const int teacherId = classId == 43 ? 8 : 7;
            const Teacher teacher = DataService().getTeacher(teacherId)
                .value_or(Teacher{});
            record.teacherEnglishName = teacher.teacherEn;
            record.teacherKoreanName = teacher.teacherKr;

            ClassTime regular;
            regular.day = classId == 44
                ? QStringLiteral("Monday")
                : classId == 43
                ? QStringLiteral("Thursday")
                : QStringLiteral("Tuesday");
            regular.startTime = classId == 43 || classId == 44
                ? QStringLiteral("5:00 PM")
                : QStringLiteral("4:00 PM");
            regular.endTime = classId == 43 || classId == 44
                ? QStringLiteral("5:50 PM")
                : QStringLiteral("4:50 PM");
            record.regularTimes.append(regular);

            if (ScheduleWidgetTestStubs::existingIntensiveHours)
            {
                if (ScheduleWidgetTestStubs::distinctIntensiveDays)
                {
                    regular.day = classId == 43
                        ? QStringLiteral("Monday")
                        : QStringLiteral("Friday");
                }
                regular.startTime = QStringLiteral("9:00 AM");
                regular.endTime = QStringLiteral("9:50 AM");
                record.intensiveTimes.append(regular);
            }
        }

        records.append(std::move(record));
    }
    return records;
}

Result<QList<ClassInfo>> ClassInfoRepository::loadScheduleClassInfos()
{
    ++ScheduleWidgetTestStubs::scheduleClassInfoReadCount;
    if (ScheduleWidgetTestStubs::scheduleClassInfoReadFailure)
    {
        return std::unexpected(
            QStringLiteral("injected schedule read failure")
            );
    }

    QList<ClassInfo> infos;
    const auto appendClass = [&infos](const int classId)
    {
        const auto loaded = DataService().loadClassInfo(classId);
        if (loaded)
        {
            ClassInfo info = *loaded;
            if (info.teacherId > 0)
            {
                const Teacher teacher = DataService().getTeacher(
                    info.teacherId
                    ).value_or(Teacher{});
                info.teacherKr = teacher.teacherKr;
                info.teacherEn = teacher.teacherEn;
                info.teacherPreferredName =
                    teacher.preferredDisplayName();
                info.roomNumber = teacher.roomNumber;
            }
            infos.append(std::move(info));
        }
    };

    if (ScheduleWidgetTestStubs::includeAdditionalClass)
    {
        appendClass(43);
    }
    appendClass(42);

    if (ScheduleWidgetTestStubs::includeMiddleSchoolClasses)
    {
        ClassInfo m1 = DataService().loadClassInfo(45).value_or(ClassInfo{});
        m1.classGrade = QStringLiteral("M1");
        m1.classLevel = QStringLiteral("Solis");
        m1.classTimes = {
            {
                .day = QStringLiteral("Wednesday"),
                .startTime = QStringLiteral("4:00 PM"),
                .endTime = QStringLiteral("4:50 PM")
            }
        };
        infos.append(std::move(m1));

        ClassInfo m2 = DataService().loadClassInfo(44).value_or(ClassInfo{});
        m2.classGrade = QStringLiteral("M2");
        m2.classLevel = QStringLiteral("Ursa");
        m2.classTimes = {
            {
                .day = QStringLiteral("Monday"),
                .startTime = QStringLiteral("4:00 PM"),
                .endTime = QStringLiteral("4:50 PM")
            }
        };
        infos.append(std::move(m2));
    }

    return infos;
}

DatabaseSession* ApplicationServices::databaseSession() const
{
    return m_databaseSession.get();
}

DataService* ApplicationServices::dataService() const
{
    return m_dataService.get();
}

SettingsService* ApplicationServices::settingsService() const
{
    if (!m_settingsService)
    {
        m_settingsService = std::make_unique<SettingsService>(
            m_dataService.get()
            );
    }
    return m_settingsService.get();
}

ClassService* ApplicationServices::classService() const
{
    if (!m_classService)
    {
        m_classService = std::make_unique<ClassService>(
            m_dataService.get()
            );
    }
    return m_classService.get();
}

TeacherService* ApplicationServices::teacherService() const
{
    if (!m_teacherService)
    {
        m_teacherService = std::make_unique<TeacherService>(
            m_dataService.get()
            );
    }
    return m_teacherService.get();
}

SpeakingEvaluationService* ApplicationServices::speakingEvaluationService() const
{
    if (!m_speakingEvaluationService)
    {
        m_speakingEvaluationService =
            std::make_unique<SpeakingEvaluationService>(m_dataService.get());
    }
    return m_speakingEvaluationService.get();
}

ScheduleService* ApplicationServices::scheduleService() const
{
    if (!m_scheduleService)
    {
        m_scheduleService = std::make_unique<ScheduleService>(
            m_dataService.get()
            );
    }
    return m_scheduleService.get();
}

ThemeService* ApplicationServices::themeService() const
{
    if (!ScheduleWidgetTestStubs::themeAvailable)
    {
        return nullptr;
    }

    static ThemeService themeService;
    return &themeService;
}

DataService::DataService(
    const QString&
    )
{
}

DataService::~DataService() = default;

bool DataService::isOpen() const
{
    return ScheduleWidgetTestStubs::databaseOpen;
}

DatabaseSession* DataService::databaseSession() const
{
    return nullptr;
}

Status DataService::saveSetting(
    const QString& key,
    const QVariant& value
    )
{
    ScheduleWidgetTestStubs::settings.insert(
        key,
        value
        );
    return {};
}

Status DataService::saveSettings(
    const QVariantMap& values
    )
{
    for (auto setting = values.cbegin(); setting != values.cend(); ++setting)
    {
        ScheduleWidgetTestStubs::settings.insert(
            setting.key(),
            setting.value()
            );
    }
    return {};
}

Result<QVariant> DataService::loadSetting(
    const QString& key
    )
{
    return ScheduleWidgetTestStubs::settings.value(
        key
        );
}

Result<ScheduleImportPreview> DataService::previewScheduleImport(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    )
{
    ScheduleImportPreview preview;
    preview.kind = kind;
    preview.user = user;
    const QList<Classroom> classrooms = getClasses().value_or(
        QList<Classroom>{});
    preview.inventory.classCount = classrooms.size();
    for (const Classroom& classroom : classrooms)
    {
        const ClassInfo info =
            loadClassInfo(classroom.id).value_or(ClassInfo{});
        preview.inventory.hasRegularHours =
            preview.inventory.hasRegularHours
            || !info.classTimes.isEmpty();
        preview.inventory.hasIntensiveHours =
            preview.inventory.hasIntensiveHours
            || !info.intensiveTimes.isEmpty();
    }
    QSet<QString> teacherKeys;

    for (int index = 0; index < user.classes.size(); ++index)
    {
        const ScheduleImportClassCandidate& candidate =
            user.classes[index];
        if (!teacherKeys.contains(candidate.teacherKey))
        {
            teacherKeys.insert(candidate.teacherKey);
            ScheduleImportTeacherPreview teacher;
            teacher.teacherKey = candidate.teacherKey;
            teacher.teacherKr = candidate.teacherKr;
            teacher.importedRooms = candidate.rooms;
            preview.teachers.append(teacher);
        }

        ScheduleImportClassPreview classroom;
        classroom.candidateIndex = index;
        if (
            (
                ScheduleWidgetTestStubs::matchImportedClasses
                || ScheduleWidgetTestStubs::possibleImportedClasses
                )
            && candidate.classGrade == QStringLiteral("E4")
            && candidate.classLevel == QStringLiteral("Hercules")
            )
        {
            classroom.matchingClassIds = {43};
            classroom.suggestedClassId = 43;
            classroom.exactMatch =
                ScheduleWidgetTestStubs::matchImportedClasses;
            classroom.matchConfidence =
                ScheduleWidgetTestStubs::matchImportedClasses
                    ? ScheduleImportClassMatchConfidence::Confident
                    : ScheduleImportClassMatchConfidence::Possible;
            classroom.matchExplanation =
                ScheduleWidgetTestStubs::matchImportedClasses
                    ? QStringLiteral("Confident existing class match.")
                    : QStringLiteral("Possible existing class match.");
        }
        else
        {
            classroom.matchExplanation =
                QStringLiteral("No existing class match.");
        }
        preview.classes.append(classroom);
    }

    return preview;
}

Result<ScheduleImportSummary> DataService::importSchedule(
    const ScheduleImportPlan& plan
    )
{
    ScheduleImportSummary summary;
    summary.classesCreated = plan.candidates.size();
    summary.ignoredCells = plan.diagnostics.size();
    summary.profileNameUpdated =
        plan.saveProfileNameIfBlank
        || plan.updateProfileName;
    return summary;
}

Result<QList<IntensiveSlotState>> DataService::loadIntensiveSlotStates()
{
    ++ScheduleWidgetTestStubs::slotStateReadCount;
    return {};
}

Result<QList<CalendarEvent>> DataService::loadCalendarEventsInRange(
    const QDate&,
    const QDate&
    )
{
    return {};
}

Status DataService::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state,
    const QString& defaultState
    )
{
    ++ScheduleWidgetTestStubs::savedSlotStates;
    ScheduleWidgetTestStubs::lastSavedSlotDay = day;
    ScheduleWidgetTestStubs::lastSavedSlotStartTime = startTime;
    ScheduleWidgetTestStubs::lastSavedSlotState = state;
    ScheduleWidgetTestStubs::lastSavedSlotDefaultState = defaultState;
    return ScheduleWidgetTestStubs::slotSaveResult;
}

Result<QList<TestingBlock>> DataService::loadTestingBlocks()
{
    QList<TestingBlock> blocks;

    for (
        auto iterator =
            ScheduleWidgetTestStubs::testingBlocks.cbegin();
        iterator !=
            ScheduleWidgetTestStubs::testingBlocks.cend();
        ++iterator
        )
    {
        const QStringList keyParts =
            iterator.key().split(QLatin1Char('\x1f'));
        if (keyParts.size() != 2)
        {
            continue;
        }

        blocks.append(
            {
                keyParts.at(0),
                keyParts.at(1),
                iterator.value()
            }
            );
    }

    return blocks;
}

Result<QList<TestingAssignment>>
DataService::loadTestingAssignments()
{
    ++ScheduleWidgetTestStubs::testingAssignmentsReadCount;
    QList<TestingAssignment> assignments;

    for (
        auto iterator =
            ScheduleWidgetTestStubs::testingBlocks.cbegin();
        iterator !=
            ScheduleWidgetTestStubs::testingBlocks.cend();
        ++iterator
        )
    {
        const QStringList keyParts =
            iterator.key().split(QLatin1Char('\x1f'));
        if (keyParts.size() != 2)
        {
            continue;
        }

        TestingAssignment assignment;
        assignment.day = keyParts.at(0);
        assignment.startTime = keyParts.at(1);
        assignment.room = iterator.value();
        assignments.append(assignment);
    }

    for (
        auto iterator =
            ScheduleWidgetTestStubs::testingClassAssignments.cbegin();
        iterator !=
            ScheduleWidgetTestStubs::testingClassAssignments.cend();
        ++iterator
        )
    {
        const QStringList keyParts =
            iterator.key().split(QLatin1Char('\x1f'));
        if (keyParts.size() != 2)
        {
            continue;
        }

        TestingAssignment assignment;
        assignment.day = keyParts.at(0);
        assignment.startTime = keyParts.at(1);
        assignment.kind = TestingAssignmentKind::SpecialClass;
        assignment.classId = iterator.value();
        assignments.append(assignment);
    }

    return assignments;
}

Status TestingBlockRepository::saveTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room,
    const bool replaceExisting
    )
{
    using namespace ClassMngr::Next::Application;
    ++ScheduleWidgetTestStubs::testingAssignmentWriteCount;
    ScheduleWidgetTestStubs::lastTestingAssignmentMutation =
        ScheduleTestingAssignmentMutation::SavePlainTesting;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteDay = day;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteStartTime = startTime;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteRoom = room;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteClassId = -1;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteReplaceExisting =
        replaceExisting;

    if (!ScheduleWidgetTestStubs::testingAssignmentWriteFailure.isEmpty())
    {
        return std::unexpected(
            ScheduleWidgetTestStubs::testingAssignmentWriteFailure
            );
    }

    const QString key = day + QLatin1Char('\x1f') + startTime;
    if (
        !replaceExisting
        && ScheduleWidgetTestStubs::testingClassAssignments.contains(key)
        )
    {
        return std::unexpected(
            QStringLiteral(
                "This slot is assigned to a testing class. Confirm replacement first."
                )
            );
    }

    ScheduleWidgetTestStubs::testingBlocks.insert(key, room.trimmed());
    ScheduleWidgetTestStubs::testingClassAssignments.remove(key);
    return {};
}

Status TestingBlockRepository::assignTestingClass(
    const QString& day,
    const QString& startTime,
    const int classId,
    const bool replaceExisting
    )
{
    using namespace ClassMngr::Next::Application;
    ++ScheduleWidgetTestStubs::testingAssignmentWriteCount;
    ScheduleWidgetTestStubs::lastTestingAssignmentMutation =
        ScheduleTestingAssignmentMutation::AssignTestingClass;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteDay = day;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteStartTime = startTime;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteRoom.clear();
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteClassId = classId;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteReplaceExisting =
        replaceExisting;

    if (!ScheduleWidgetTestStubs::testingAssignmentWriteFailure.isEmpty())
    {
        return std::unexpected(
            ScheduleWidgetTestStubs::testingAssignmentWriteFailure
            );
    }

    const QString key = day + QLatin1Char('\x1f') + startTime;
    const auto existingClass =
        ScheduleWidgetTestStubs::testingClassAssignments.constFind(key);
    const bool hasAssignment =
        existingClass != ScheduleWidgetTestStubs::testingClassAssignments.cend()
        || ScheduleWidgetTestStubs::testingBlocks.contains(key);
    const bool sameClass =
        existingClass != ScheduleWidgetTestStubs::testingClassAssignments.cend()
        && existingClass.value() == classId;
    if (hasAssignment && !sameClass && !replaceExisting)
    {
        return std::unexpected(
            QStringLiteral(
                "This slot already has a testing assignment. Confirm replacement first."
                )
            );
    }

    ScheduleWidgetTestStubs::testingBlocks.remove(key);
    ScheduleWidgetTestStubs::testingClassAssignments.insert(key, classId);
    return {};
}

Status TestingBlockRepository::deleteTestingAssignment(
    const QString& day,
    const QString& startTime
    )
{
    using namespace ClassMngr::Next::Application;
    ++ScheduleWidgetTestStubs::testingAssignmentWriteCount;
    ScheduleWidgetTestStubs::lastTestingAssignmentMutation =
        ScheduleTestingAssignmentMutation::RemoveAssignment;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteDay = day;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteStartTime = startTime;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteRoom.clear();
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteClassId = -1;
    ScheduleWidgetTestStubs::lastTestingAssignmentWriteReplaceExisting = false;

    if (!ScheduleWidgetTestStubs::testingAssignmentWriteFailure.isEmpty())
    {
        return std::unexpected(
            ScheduleWidgetTestStubs::testingAssignmentWriteFailure
            );
    }

    const QString key = day + QLatin1Char('\x1f') + startTime;
    ScheduleWidgetTestStubs::testingBlocks.remove(key);
    ScheduleWidgetTestStubs::testingClassAssignments.remove(key);
    return {};
}

Status DataService::saveTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room,
    bool
    )
{
    ScheduleWidgetTestStubs::setTestingBlock(
        day,
        startTime,
        room
        );
    ++ScheduleWidgetTestStubs::savedTestingBlocks;
    return {};
}

Status DataService::assignTestingClass(
    const QString& day,
    const QString& startTime,
    int classId,
    bool
    )
{
    const QString key =
        day + QLatin1Char('\x1f') + startTime;
    ScheduleWidgetTestStubs::testingBlocks.remove(key);
    ScheduleWidgetTestStubs::testingClassAssignments.insert(
        key,
        classId
        );
    return {};
}

Status DataService::deleteTestingAssignment(
    const QString& day,
    const QString& startTime
    )
{
    return deleteTestingBlock(day, startTime);
}

Status DataService::deleteTestingBlock(
    const QString& day,
    const QString& startTime
    )
{
    const QString key =
        day + QLatin1Char('\x1f') + startTime;
    ScheduleWidgetTestStubs::testingBlocks.remove(key);
    ScheduleWidgetTestStubs::testingClassAssignments.remove(key);
    return {};
}

Status DataService::clearTestingBlocks()
{
    ScheduleWidgetTestStubs::testingBlocks.clear();
    ScheduleWidgetTestStubs::testingClassAssignments.clear();
    return {};
}

Status DataService::clearTestingAssignments()
{
    return clearTestingBlocks();
}

Result<int> DataService::createTestingClass(
    const TestingClass& testingClass,
    const QString& assignmentDay,
    const QString& assignmentStartTime
    )
{
    const int classId =
        ScheduleWidgetTestStubs::testingClasses.isEmpty()
            ? 100
            : ScheduleWidgetTestStubs::testingClasses.keys().last() + 1;
    TestingClass stored = testingClass;
    stored.classId = classId;
    ScheduleWidgetTestStubs::testingClasses.insert(classId, stored);
    if (
        !assignmentDay.isEmpty()
        && !assignmentStartTime.isEmpty()
        )
    {
        const QString key =
            assignmentDay
            + QLatin1Char('\x1f')
            + assignmentStartTime;
        ScheduleWidgetTestStubs::testingClassAssignments.insert(
            key,
            classId
            );
    }
    return classId;
}

Status DataService::updateTestingClass(
    const TestingClass& testingClass
    )
{
    ScheduleWidgetTestStubs::testingClasses.insert(
        testingClass.classId,
        testingClass
        );
    return {};
}

Result<TestingClass> DataService::loadTestingClass(
    int classId
    )
{
    if (!ScheduleWidgetTestStubs::testingClasses.contains(classId))
    {
        return std::unexpected(QStringLiteral("Missing testing class"));
    }
    return ScheduleWidgetTestStubs::testingClasses.value(classId);
}

Result<QList<TestingClass>> DataService::loadTestingClasses()
{
    return ScheduleWidgetTestStubs::testingClasses.values();
}

Status DataService::deleteTestingClass(
    int classId
    )
{
    ScheduleWidgetTestStubs::testingClasses.remove(classId);
    return {};
}

Result<bool> DataService::isTestingClass(
    int classId
    )
{
    return ScheduleWidgetTestStubs::testingClasses.contains(classId);
}

Result<QList<Classroom>> DataService::getClasses()
{
    Classroom classroom;
    classroom.id = 42;
    classroom.name = QStringLiteral("Hercules");

    QList<Classroom> classrooms{classroom};

    if (ScheduleWidgetTestStubs::includeAdditionalClass)
    {
        Classroom additionalClass;
        additionalClass.id = 43;
        additionalClass.name = QStringLiteral("Athena");
        classrooms.append(additionalClass);
    }

    if (ScheduleWidgetTestStubs::includeAlternativeMatchingClass)
    {
        Classroom alternativeClass;
        alternativeClass.id = 44;
        alternativeClass.name = QStringLiteral("Hercules Evening");
        classrooms.append(alternativeClass);
    }

    return classrooms;
}

Result<Classroom> DataService::getClassById(
    int classId
    )
{
    for (const Classroom& classroom : getClasses().value_or(
             QList<Classroom>{}))
    {
        if (classroom.id == classId)
        {
            return classroom;
        }
    }

    return std::unexpected(QStringLiteral("Class not found."));
}

Result<ClassInfo> DataService::loadClassInfo(
    int classId
    )
{
    ClassInfo info;
    info.classId = classId;
    if (ScheduleWidgetTestStubs::testingClasses.contains(classId))
    {
        const TestingClass testingClass =
            ScheduleWidgetTestStubs::testingClasses.value(classId);
        info.teacherId = testingClass.teacherId;
        info.classGrade = testingClass.grade;
        info.classLevel = testingClass.level;
        if (testingClass.teacherId > 0)
        {
            const Teacher teacher = getTeacher(testingClass.teacherId)
                .value_or(Teacher{});
            info.teacherKr = teacher.teacherKr;
            info.teacherEn = teacher.teacherEn;
            info.teacherPreferredName =
                teacher.preferredDisplayName();
        }
        return info;
    }

    info.teacherId = classId == 43 ? 8 : 7;
    info.classGrade =
        ScheduleWidgetTestStubs::classGrades.value(
            classId,
            classId == 43
                ? QStringLiteral("E5")
                : QStringLiteral("E4")
            );
    info.classLevel =
        classId == 43
            ? QStringLiteral("Athena")
            : QStringLiteral("Hercules");
    info.notes =
        classId == 43
            ? QStringLiteral("Review the vocabulary list.")
            : QStringLiteral("Read chapter three.");

    ClassTime meeting;
    meeting.day =
        classId == 44
            ? QStringLiteral("Monday")
            : classId == 43
            ? QStringLiteral("Thursday")
            : QStringLiteral("Tuesday");
    meeting.startTime =
        classId == 44
            ? QStringLiteral("5:00 PM")
            : classId == 43
            ? QStringLiteral("5:00 PM")
            : QStringLiteral("4:00 PM");
    meeting.endTime =
        classId == 44
            ? QStringLiteral("5:50 PM")
            : classId == 43
            ? QStringLiteral("5:50 PM")
            : QStringLiteral("4:50 PM");
    info.classTimes.append(meeting);
    if (ScheduleWidgetTestStubs::existingIntensiveHours)
    {
        if (ScheduleWidgetTestStubs::distinctIntensiveDays)
        {
            meeting.day =
                classId == 43
                    ? QStringLiteral("Monday")
                    : QStringLiteral("Friday");
        }

        meeting.startTime = QStringLiteral("9:00 AM");
        meeting.endTime = QStringLiteral("9:50 AM");
        info.intensiveTimes.append(meeting);
    }

    return info;
}

Status DataService::saveClassInfo(
    const ClassInfo& info
    )
{
    ScheduleWidgetTestStubs::classGrades.insert(
        info.classId,
        info.classGrade
        );
    return {};
}

Status DataService::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    Q_UNUSED(classId);
    Q_UNUSED(notes);
    Q_UNUSED(timeFillerActivities);
    return {};
}

Result<QList<ClassConflict>> DataService::getClassTimeConflicts(
    int classId,
    const QList<ClassTime>& times,
    ScheduleType type
    )
{
    Q_UNUSED(classId);
    Q_UNUSED(times);
    Q_UNUSED(type);
    return {};
}

Result<Roster> DataService::loadRoster(
    int classId
    )
{
    if (ScheduleWidgetTestStubs::rosters.contains(classId))
    {
        return ScheduleWidgetTestStubs::rosters.value(classId);
    }

    Roster roster;
    roster.columns = {
        QStringLiteral("English"),
        QStringLiteral("Korean"),
        QStringLiteral("Winter"),
        QStringLiteral("Speech Contest"),
        QStringLiteral("Summer"),
        QStringLiteral("Fall")
    };
    return roster;
}

Status DataService::saveRoster(
    int classId,
    const Roster& roster
    )
{
    ScheduleWidgetTestStubs::rosters.insert(
        classId,
        roster
        );
    return {};
}

Status DataService::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    )
{
    for (const auto& [classId, roster] : rosters)
    {
        const Status saved = saveRoster(classId, roster);
        if (!saved)
        {
            return saved;
        }
    }

    return {};
}

Result<SpeakingEvalRows> DataService::loadSpeakingEval(
    int classId,
    const QString& evaluationName
    )
{
    return ScheduleWidgetTestStubs::speakingEvaluations.value(
        QStringLiteral("%1:%2").arg(classId).arg(evaluationName));
}

Result<int> DataService::getRosterStudentCount(
    int classId
    )
{
    if (classId == 42)
    {
        return 12;
    }

    return classId == 43
        ? 9
        : 0;
}

Result<Teacher> DataService::getTeacher(
    int teacherId
    )
{
    Teacher teacher;
    teacher.id = teacherId;
    teacher.teacherEn =
        teacherId == 8
            ? QStringLiteral("Thomas")
            : QStringLiteral("Susan");
    teacher.teacherKr =
        teacherId == 8
            ? QStringLiteral("이선생")
            : QStringLiteral("김선생");
    teacher.roomNumber =
        teacherId == 8
            ? QStringLiteral("512")
            : QStringLiteral("413");
    teacher.wifiName =
        teacherId == 8
            ? QStringLiteral("Thomas WiFi")
            : QStringLiteral("Susan WiFi");
    teacher.wifiPassword = QStringLiteral("wifi secret");
    teacher.zoomId =
        teacherId == 8
            ? QStringLiteral("thomas.zoom")
            : QStringLiteral("susan.zoom");
    teacher.zoomPassword = QStringLiteral("zoom secret");
    teacher.internetType = QStringLiteral("WiFi");
    teacher.projectionType = QStringLiteral("HDMI");
    teacher.notes =
        teacherId == 8
            ? QStringLiteral("Use the classroom projector.")
            : QStringLiteral("Call before class.");
    return teacher;
}

Result<QList<Teacher>> DataService::getAllTeachers()
{
    return QList<Teacher>{
        getTeacher(7).value_or(Teacher{}),
        getTeacher(8).value_or(Teacher{})
    };
}

Theme ThemeService::currentTheme() const
{
    return ScheduleWidgetTestStubs::configuredTheme;
}

QFont FontManager::getUiFont(
    int size,
    int weight,
    bool italic
    )
{
    QFont font;
    if (size > 0)
    {
        font.setPointSize(size);
    }
    font.setWeight(static_cast<QFont::Weight>(weight));
    font.setItalic(italic);
    return font;
}

QFont FontManager::getKoreanFont(
    int size,
    int weight,
    bool italic
    )
{
    return getUiFont(size, weight, italic);
}

int FontManager::adjustedPointSize(
    int baseSize
    )
{
    return baseSize;
}

int FontManager::sizeOffset()
{
    return 0;
}

void FontManager::setManagedRichText(
    QLabel* label,
    const QString& html
    )
{
    if (label)
    {
        label->setTextFormat(Qt::RichText);
        label->setText(html);
    }
}

QString scheduleEmptySlotState()
{
    return QStringLiteral("empty");
}

QString scheduleEssaySlotState()
{
    return QStringLiteral("essay");
}

QString scheduleLunchSlotState()
{
    return QStringLiteral("lunch");
}

QString scheduleTestingSlotState()
{
    return QStringLiteral("testing");
}

bool scheduleModeUsesIntensiveTimes(
    ScheduleDisplayMode mode
    )
{
    return mode == ScheduleDisplayMode::Intensive;
}

QString nextScheduleSlotState(
    const QString& currentState
    )
{
    return currentState == scheduleEssaySlotState()
        ? scheduleLunchSlotState()
        : scheduleEssaySlotState();
}

QString scheduleSlotKey(
    const QString& day,
    const QString& timeLabel
    )
{
    return day + QLatin1Char('\x1f') + timeLabel;
}

QStringList visibleScheduleDays(
    bool includeWeekends
    )
{
    QStringList days{
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"),
        QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };

    if (includeWeekends)
    {
        days.append(QStringLiteral("Saturday"));
        days.append(QStringLiteral("Sunday"));
    }

    return days;
}

bool isScheduleWeekendDay(
    const QString& day
    )
{
    return day == QStringLiteral("Saturday")
        || day == QStringLiteral("Sunday");
}

QString scheduleDefaultSlotState(
    const QString&,
    const QString&,
    bool useIntensive
    )
{
    return useIntensive
        ? scheduleEssaySlotState()
        : scheduleEmptySlotState();
}

bool scheduleSlotTogglingEnabled(
    const QString&,
    bool useIntensive,
    bool regularWeekdaySlotTogglingEnabled
    )
{
    return useIntensive
        || regularWeekdaySlotTogglingEnabled;
}

QString scheduleSlotState(
    const QString& day,
    const QString& timeLabel,
    const QString& defaultState,
    const QMap<QString, QString>& slotStateOverrides
    )
{
    const auto iterator = slotStateOverrides.constFind(
        day + QLatin1Char('\x1f') + timeLabel
        );
    return iterator == slotStateOverrides.cend()
        ? defaultState
        : iterator.value();
}

ScheduleViewModel buildScheduleViewModel(
    const ScheduleBuildResult& result,
    const ScheduleViewRequest& request
    )
{
    ScheduleViewModel model;
    model.days = request.days;

    for (const ScheduleRow& sourceRow : result.rows)
    {
        ScheduleRowView row;
        row.timeLabel = sourceRow.label;
        row.timeRangeLabel = QStringLiteral("4pm - 4:50pm");

        for (const QString& day : request.days)
        {
            ScheduleCellView cell;
            cell.day = day;
            cell.timeLabel = sourceRow.label;
            cell.entries =
                result.schedule
                    .value(day)
                    .value(sourceRow.label);

            const QString key =
                scheduleSlotKey(
                    day,
                    sourceRow.label
                    );
            const auto assignment =
                request.testingAssignments.constFind(key);
            const bool hasExplicitAssignment =
                request.displayMode == ScheduleDisplayMode::Testing
                && assignment != request.testingAssignments.cend();
            bool removedAffectedEntry = false;
            if (
                request.displayMode
                    == ScheduleDisplayMode::Testing
                && !hasExplicitAssignment
                )
            {
                for (
                    int entryIndex = cell.entries.size() - 1;
                    entryIndex >= 0;
                    --entryIndex
                    )
                {
                    const QString grade =
                        cell.entries
                            .at(entryIndex)
                            .classGrade
                            .trimmed()
                            .toUpper();
                    if (
                        grade == QStringLiteral("M2")
                        || grade == QStringLiteral("M3")
                        || (
                            request.testingAffectsM1
                            && grade == QStringLiteral("M1")
                            )
                        )
                    {
                        cell.entries.removeAt(entryIndex);
                        removedAffectedEntry = true;
                    }
                }
            }

            cell.defaultSlotState =
                scheduleDefaultSlotState(
                    day,
                    sourceRow.label,
                    scheduleModeUsesIntensiveTimes(
                        request.displayMode
                        )
                    );
            cell.slotState = scheduleSlotState(
                day,
                sourceRow.label,
                cell.defaultSlotState,
                request.slotStateOverrides
                );
            cell.slotTogglingEnabled =
                scheduleSlotTogglingEnabled(
                    day,
                    scheduleModeUsesIntensiveTimes(
                        request.displayMode
                        ),
                    request.regularWeekdaySlotTogglingEnabled
                    );

            if (hasExplicitAssignment)
            {
                cell.entries.clear();
                if (
                    assignment->assignment.kind
                        == TestingAssignmentKind::SpecialClass
                    )
                {
                    cell.entries.append(
                        assignment->testingClassEntry
                        );
                    cell.testingClassAssignment = true;
                    cell.testingClassId =
                        assignment->assignment.classId;
                }
                else
                {
                    cell.slotState =
                        scheduleTestingSlotState();
                    cell.testingRoom =
                        assignment->assignment.room;
                }
            }

            if (
                request.displayMode
                    == ScheduleDisplayMode::Testing
                && cell.entries.isEmpty()
                && !hasExplicitAssignment
                )
            {
                if (removedAffectedEntry)
                {
                    cell.slotState =
                        scheduleEssaySlotState();
                }
                cell.testingBlockCreationEnabled =
                    cell.slotState == scheduleEssaySlotState();
            }
            row.cells.append(cell);
        }

        model.rows.append(row);
    }

    return model;
}

ScheduleEditorDialog::ScheduleEditorDialog(
    ApplicationServices* services,
    int classId,
    QWidget* parent,
    ClassMngr::Next::Application::ClassDetailsSavePort* savePort,
    ClassMngr::Next::Application::ScheduleEditorClassInfoReadPort* readPort
    )
    : DialogShell(QStringLiteral("scheduleEditor"), parent)
    , m_services(services)
    , m_savePort(savePort)
    , m_readPort(readPort)
    , m_classId(classId)
{
}

void ScheduleEditorDialog::updateLevelOptions()
{
}

void ScheduleEditorDialog::chooseClassColor()
{
}

void ScheduleEditorDialog::chooseFontColor()
{
}

void ScheduleEditorDialog::saveChanges()
{
}

SchedulePrintDialog::SchedulePrintDialog(
    Action action,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("schedulePrint"), parent)
{
    Q_UNUSED(action);
    QTimer::singleShot(
        0,
        this,
        &QDialog::accept
        );
}

SchedulePrintDialog::Action SchedulePrintDialog::selectedAction() const
{
    return Action::Print;
}

QString SchedulePrintDialog::selectedSavePath() const
{
    return {};
}

SchedulePrintStyle SchedulePrintDialog::selectedStyle() const
{
    return SchedulePrintStyle::CurrentAppearance;
}

QPageLayout::Orientation
SchedulePrintDialog::selectedOrientation() const
{
    return QPageLayout::Landscape;
}

SchedulePrintService::Result SchedulePrintService::printSchedule(
    const Request& request
    )
{
    ++ScheduleWidgetTestStubs::printRequestCount;
    ScheduleWidgetTestStubs::lastPrintRequestShowsEnglishNames =
        request.showEnglishNames;
    ScheduleWidgetTestStubs::lastPrintRequestTheme =
        request.currentTheme;
    ScheduleWidgetTestStubs::lastPrintRequestUserName =
        request.userName;

    return {Status::Canceled, {}};
}

SchedulePrintService::Result SchedulePrintService::saveSchedulePdf(
    const Request&,
    const QString&
    )
{
    return {Status::Canceled, {}};
}

PdfPrintService::Result PdfPrintService::printPdfDocument(
    const Request&
    )
{
    return {
        Status::Canceled,
        QString()
    };
}

PdfPrintService::Result PdfPrintService::printPdfDocuments(
    const BatchRequest&
    )
{
    return {
        Status::Canceled,
        QString()
    };
}

QList<RosterTemplatePrintService::TemplateId>
RosterTemplatePrintService::availableTemplateIds()
{
    return {
        TemplateId::ByDay,
        TemplateId::Daily,
        TemplateId::PerClassWithExtraInfo
    };
}

QString RosterTemplatePrintService::templateDisplayName(
    TemplateId templateId
    )
{
    switch (templateId)
    {
    case TemplateId::Daily:
        return QStringLiteral("Daily");

    case TemplateId::PerClassWithExtraInfo:
        return QStringLiteral("Per Class with Extra Info");

    case TemplateId::ByDay:
    default:
        return QStringLiteral("By Day");
    }
}

int RosterTemplatePrintService::perClassExtraInfoMaxExtraColumns(
    QPageLayout::Orientation orientation
    )
{
    return orientation == QPageLayout::Landscape ? 6 : 3;
}

QStringList RosterTemplatePrintService::availablePerClassExtraInfoColumns(
    const QList<RosterTemplatePrintService::RosterClassData>&
    )
{
    return {};
}

RosterTemplatePrintService::Result
RosterTemplatePrintService::saveRostersPdf(
    const QList<RosterTemplatePrintService::RosterClassData>&,
    const QString&,
    RosterTemplatePrintService::TemplateId,
    const QStringList&,
    QPageLayout::Orientation
    )
{
    return {
        Status::Sent,
        QString()
    };
}
