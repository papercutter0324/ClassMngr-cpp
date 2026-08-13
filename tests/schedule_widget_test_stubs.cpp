#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/fontmanager.h"
#include "core/resource_packs/resource_pack_manager.h"
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
#include "features/schedule/ui/schedule_builder.h"
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
QHash<QString, QString> testingBlocks;
QHash<int, TestingClass> testingClasses;
QHash<QString, int> testingClassAssignments;
QHash<int, Roster> rosters;
int savedSlotStates = 0;
int savedTestingBlocks = 0;
int printRequestCount = 0;
bool lastPrintRequestShowsEnglishNames = false;
bool databaseOpen = true;
bool includeAdditionalClass = false;
bool includeMiddleSchoolClasses = false;
bool matchImportedClasses = false;
bool possibleImportedClasses = false;
bool existingIntensiveHours = false;
bool distinctIntensiveDays = false;
bool includeAlternativeMatchingClass = false;

void reset()
{
    settings.clear();
    testingBlocks.clear();
    testingClasses.clear();
    testingClassAssignments.clear();
    rosters.clear();
    savedSlotStates = 0;
    savedTestingBlocks = 0;
    printRequestCount = 0;
    lastPrintRequestShowsEnglishNames = false;
    databaseOpen = true;
    includeAdditionalClass = false;
    includeMiddleSchoolClasses = false;
    matchImportedClasses = false;
    possibleImportedClasses = false;
    existingIntensiveHours = false;
    distinctIntensiveDays = false;
    includeAlternativeMatchingClass = false;
}

void setDatabaseOpen(
    bool open
    )
{
    databaseOpen = open;
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

void setIncludeAlternativeMatchingClass(
    bool include
    )
{
    includeAlternativeMatchingClass = include;
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
}

ApplicationServices::ApplicationServices()
{
    m_dataService =
        std::make_unique<DataService>();
}

ApplicationServices::~ApplicationServices() = default;

DataService* ApplicationServices::dataService() const
{
    return m_dataService.get();
}

ThemeService* ApplicationServices::themeService() const
{
    return nullptr;
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

void DataService::saveSetting(
    const QString& key,
    const QVariant& value
    )
{
    ScheduleWidgetTestStubs::settings.insert(
        key,
        value
        );
}

QVariant DataService::loadSetting(
    const QString& key,
    const QVariant& defaultValue
    )
{
    return ScheduleWidgetTestStubs::settings.value(
        key,
        defaultValue
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
    const QList<Classroom> classrooms = getClasses();
    preview.inventory.classCount = classrooms.size();
    for (const Classroom& classroom : classrooms)
    {
        const ClassInfo info = loadClassInfo(classroom.id);
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

QList<IntensiveSlotState> DataService::loadIntensiveSlotStates()
{
    return {};
}

QList<CalendarEvent> DataService::loadCalendarEventsInRange(
    const QDate&,
    const QDate&
    )
{
    return {};
}

void DataService::saveIntensiveSlotState(
    const QString&,
    const QString&,
    const QString&,
    const QString&
    )
{
    ++ScheduleWidgetTestStubs::savedSlotStates;
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

QList<Classroom> DataService::getClasses()
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

Classroom DataService::getClassById(
    int classId
    )
{
    for (const Classroom& classroom : getClasses())
    {
        if (classroom.id == classId)
        {
            return classroom;
        }
    }

    return {};
}

ClassInfo DataService::loadClassInfo(
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
            const Teacher teacher =
                getTeacher(testingClass.teacherId);
            info.teacherKr = teacher.teacherKr;
            info.teacherEn = teacher.teacherEn;
            info.teacherPreferredName =
                teacher.preferredDisplayName();
        }
        return info;
    }

    info.teacherId = classId == 43 ? 8 : 7;
    info.classGrade =
        classId == 43
            ? QStringLiteral("E5")
            : QStringLiteral("E4");
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

bool DataService::saveClassInfo(
    const ClassInfo& info
    )
{
    Q_UNUSED(info);
    return true;
}

bool DataService::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    Q_UNUSED(classId);
    Q_UNUSED(notes);
    Q_UNUSED(timeFillerActivities);
    return true;
}

QList<ClassConflict> DataService::getClassTimeConflicts(
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

Roster DataService::loadRoster(
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

void DataService::saveRoster(
    int classId,
    const Roster& roster
    )
{
    ScheduleWidgetTestStubs::rosters.insert(
        classId,
        roster
        );
}

bool DataService::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    )
{
    for (const auto& [classId, roster] : rosters)
    {
        saveRoster(classId, roster);
    }

    return true;
}

int DataService::getRosterStudentCount(
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

Teacher DataService::getTeacher(
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

QList<Teacher> DataService::getAllTeachers()
{
    return {
        getTeacher(7),
        getTeacher(8)
    };
}

ResourcePackManager& ResourcePackManager::instance()
{
    static ResourcePackManager manager;
    return manager;
}

ResourcePackManager::ResourcePackManager(
    QString storageDirectory
    )
    : m_storageDirectory(std::move(storageDirectory))
{
}

QString ResourcePackManager::activeRoot(
    const QString&
    ) const
{
    return {};
}

Theme ThemeService::currentTheme() const
{
    return Theme::Dark;
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

ScheduleBuilder::ScheduleBuilder(
    DataService* dataService
    )
    : m_dataService(dataService)
{
}

ScheduleBuildResult ScheduleBuilder::build(
    bool,
    const QStringList& visibleDays
    ) const
{
    ScheduleBuildResult result;
    result.days = visibleDays;

    if (!m_dataService || !m_dataService->isOpen())
    {
        return result;
    }

    result.rows.append(
        {QStringLiteral("16:00")}
        );

    if (ScheduleWidgetTestStubs::includeAdditionalClass)
    {
        result.rows.append(
            {QStringLiteral("17:00")}
            );
    }

    for (const QString& day : visibleDays)
    {
        result.schedule.insert(day, {});
    }

    if (visibleDays.contains(QStringLiteral("Tuesday")))
    {
        ScheduleEntry entry;
        entry.classId = 42;
        entry.teacherKr = QStringLiteral("김선생");
        entry.teacherEn = QStringLiteral("Susan");
        entry.roomNumber = QStringLiteral("413");
        entry.classGrade = QStringLiteral("E4");
        entry.classLevel = QStringLiteral("Hercules");

        result.schedule[QStringLiteral("Tuesday")]
            [QStringLiteral("16:00")]
                .append(entry);
    }

    if (
        ScheduleWidgetTestStubs::includeAdditionalClass
        && visibleDays.contains(QStringLiteral("Thursday"))
        )
    {
        ScheduleEntry entry;
        entry.classId = 43;
        entry.teacherKr = QStringLiteral("이선생");
        entry.teacherEn = QStringLiteral("Thomas");
        entry.roomNumber = QStringLiteral("512");
        entry.classGrade = QStringLiteral("E5");
        entry.classLevel = QStringLiteral("Athena");

        result.schedule[QStringLiteral("Thursday")]
            [QStringLiteral("17:00")]
                .append(entry);
    }

    if (ScheduleWidgetTestStubs::includeMiddleSchoolClasses)
    {
        if (visibleDays.contains(QStringLiteral("Monday")))
        {
            ScheduleEntry entry;
            entry.classId = 44;
            entry.teacherEn = QStringLiteral("M2 Teacher");
            entry.classGrade = QStringLiteral("M2");
            entry.classLevel = QStringLiteral("Ursa");
            result.schedule[QStringLiteral("Monday")]
                [QStringLiteral("16:00")]
                    .append(entry);
        }

        if (visibleDays.contains(QStringLiteral("Wednesday")))
        {
            ScheduleEntry entry;
            entry.classId = 45;
            entry.teacherEn = QStringLiteral("M1 Teacher");
            entry.classGrade = QStringLiteral("M1");
            entry.classLevel = QStringLiteral("Solis");
            result.schedule[QStringLiteral("Wednesday")]
                [QStringLiteral("16:00")]
                    .append(entry);
        }
    }

    return result;
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
    const QString&,
    const QString&,
    const QString& defaultState,
    const QMap<QString, QString>&
    )
{
    return defaultState;
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
            cell.slotState = cell.defaultSlotState;
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
    QWidget* parent
    )
    : QDialog(parent)
    , m_services(services)
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
