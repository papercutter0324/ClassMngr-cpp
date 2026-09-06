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
QHash<int, QString> classGrades;
QHash<QString, QString> testingBlocks;
QHash<int, TestingClass> testingClasses;
QHash<QString, int> testingClassAssignments;
QHash<int, Roster> rosters;
QHash<QString, SpeakingEvalRows> speakingEvaluations;
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
    classGrades.clear();
    testingBlocks.clear();
    testingClasses.clear();
    testingClassAssignments.clear();
    rosters.clear();
    speakingEvaluations.clear();
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

void setIncludeAlternativeMatchingClass(
    bool include
    )
{
    includeAlternativeMatchingClass = include;
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
}

ApplicationServices::ApplicationServices()
{
    m_settingsService =
        std::make_unique<SettingsService>(nullptr);
    m_teacherService =
        std::make_unique<TeacherService>(nullptr);
    m_classService =
        std::make_unique<ClassService>(nullptr);
    m_scheduleService =
        std::make_unique<ScheduleService>(nullptr);
    m_calendarService =
        std::make_unique<CalendarService>(nullptr);
    m_rosterService =
        std::make_unique<RosterService>(nullptr);
    m_speakingEvaluationService =
        std::make_unique<SpeakingEvaluationService>(nullptr);
}

ApplicationServices::~ApplicationServices() = default;

ThemeService* ApplicationServices::themeService() const
{
    return nullptr;
}

bool ApplicationServices::hasOpenDatabase() const
{
    return ScheduleWidgetTestStubs::databaseOpen;
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

Status DataService::validateScheduleImport(
    const ScheduleImportPlan&
    )
{
    return {};
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
    const QString&,
    const QString&,
    const QString&,
    const QString&
    )
{
    ++ScheduleWidgetTestStubs::savedSlotStates;
    return {};
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
    Q_UNUSED(info);
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

Status DataService::saveSpeakingEval(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells
    )
{
    Q_UNUSED(dirtyCells);
    ScheduleWidgetTestStubs::speakingEvaluations.insert(
        QStringLiteral("%1:%2").arg(classId).arg(evaluationName),
        rows
        );
    return {};
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

namespace
{
DataService& scheduleWidgetCompatibilityData()
{
    static DataService service;
    return service;
}
}

bool FeatureService::isAvailable() const
{
    return ScheduleWidgetTestStubs::databaseOpen;
}

Status SettingsService::save(
    const QString& key,
    const QVariant& value
    ) const
{
    return scheduleWidgetCompatibilityData().saveSetting(key, value);
}

Status SettingsService::saveAll(
    const QVariantMap& values
    ) const
{
    return scheduleWidgetCompatibilityData().saveSettings(values);
}

Result<QVariant> SettingsService::load(
    const QString& key
    ) const
{
    return scheduleWidgetCompatibilityData().loadSetting(key);
}

QVariant SettingsService::loadOrDefault(
    const QString& key,
    const QVariant& defaultValue
    ) const
{
    const Result<QVariant> value = load(key);
    return value && value->isValid() ? *value : defaultValue;
}

Result<int> TeacherService::create(const Teacher& teacher) const
{
    Q_UNUSED(teacher);
    return 9;
}

Result<int> TeacherService::save(const Teacher& teacher) const
{
    return create(teacher);
}

Status TeacherService::update(const Teacher& teacher) const
{
    Q_UNUSED(teacher);
    return {};
}

Result<Teacher> TeacherService::teacher(int teacherId) const
{
    return scheduleWidgetCompatibilityData().getTeacher(teacherId);
}

Result<QList<Teacher>> TeacherService::teachers() const
{
    return scheduleWidgetCompatibilityData().getAllTeachers();
}

Status TeacherService::remove(int teacherId) const
{
    Q_UNUSED(teacherId);
    return {};
}

Result<QList<NativeEnglishTeacher>> TeacherService::nativeEnglishTeachers() const
{
    return QList<NativeEnglishTeacher>{};
}

Status TeacherService::saveNativeEnglishTeacherDirectory(
    const QList<NativeEnglishTeacher>& teachers,
    const QList<int>& deletedIds
    ) const
{
    Q_UNUSED(teachers);
    Q_UNUSED(deletedIds);
    return {};
}

Result<QList<GsTeamMember>> TeacherService::gsTeamMembers() const
{
    return QList<GsTeamMember>{};
}

Status TeacherService::saveGsTeamDirectory(
    const QList<GsTeamMember>& members,
    const QList<int>& deletedIds
    ) const
{
    Q_UNUSED(members);
    Q_UNUSED(deletedIds);
    return {};
}

Result<TeacherImportSummary> TeacherService::importTeachers(
    const TeacherImportPlan& plan
    ) const
{
    Q_UNUSED(plan);
    return TeacherImportSummary{};
}

Result<TeacherService::TeacherImportDateCheck>
TeacherService::compareLatestImportDate(const QDate& sourceDate) const
{
    Q_UNUSED(sourceDate);
    return TeacherImportDateCheck{};
}

Result<QList<Classroom>> ClassService::classes() const
{
    return scheduleWidgetCompatibilityData().getClasses();
}

Result<QList<ClassTeacherAssignment>>
ClassService::classTeacherAssignments() const
{
    QList<ClassTeacherAssignment> assignments;
    const QList<Classroom> classrooms = classes().value_or(QList<Classroom>{});
    for (const Classroom& classroom : classrooms)
    {
        assignments.append({
            classroom.id,
            scheduleWidgetCompatibilityData().loadClassInfo(classroom.id)
                .value_or(ClassInfo{})
                .teacherId
        });
    }
    return assignments;
}

Result<QList<ClassInfo>> ClassService::scheduleClassInfos() const
{
    QList<ClassInfo> infos;
    const QList<Classroom> classrooms = classes().value_or(QList<Classroom>{});
    for (const Classroom& classroom : classrooms)
    {
        infos.append(
            scheduleWidgetCompatibilityData().loadClassInfo(classroom.id)
                .value_or(ClassInfo{})
            );
    }
    return infos;
}

Result<Classroom> ClassService::classroom(int classId) const
{
    return scheduleWidgetCompatibilityData().getClassById(classId);
}

Result<int> ClassService::create(const QString& name) const
{
    Q_UNUSED(name);
    return 99;
}

Status ClassService::rename(int classId, const QString& name) const
{
    Q_UNUSED(classId);
    Q_UNUSED(name);
    return {};
}

Status ClassService::remove(int classId) const
{
    Q_UNUSED(classId);
    return {};
}

Result<ClassInfo> ClassService::classInfo(int classId) const
{
    return scheduleWidgetCompatibilityData().loadClassInfo(classId);
}

Status ClassService::saveClassInfo(const ClassInfo& info) const
{
    return scheduleWidgetCompatibilityData().saveClassInfo(info);
}

Status ClassService::saveClassNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    ) const
{
    return scheduleWidgetCompatibilityData().saveClassNotes(
        classId,
        notes,
        timeFillerActivities
        );
}

Result<QList<ClassConflict>> ClassService::conflicts(
    int classId,
    const QList<ClassTime>& times,
    ScheduleType type
    ) const
{
    return scheduleWidgetCompatibilityData().getClassTimeConflicts(
        classId,
        times,
        type
        );
}

Result<ClassTransferPackage> ClassService::buildTransferPackage(
    const QList<int>& classIds
    ) const
{
    Q_UNUSED(classIds);
    return ClassTransferPackage{};
}

Result<ClassImportPreview> ClassService::previewImport(
    const ClassTransferPackage& package
    ) const
{
    Q_UNUSED(package);
    return ClassImportPreview{};
}

Result<ClassImportSummary> ClassService::importClasses(
    const ClassTransferPackage& package,
    const ClassImportPlan& plan
    ) const
{
    Q_UNUSED(package);
    Q_UNUSED(plan);
    return ClassImportSummary{};
}

Result<ScheduleImportPreview> ScheduleService::previewImport(
    const ScheduleImportUserBlock& user,
    ScheduleImportKind kind
    ) const
{
    return scheduleWidgetCompatibilityData().previewScheduleImport(user, kind);
}

Status ScheduleService::validateImport(const ScheduleImportPlan& plan) const
{
    return scheduleWidgetCompatibilityData().validateScheduleImport(plan);
}

Result<ScheduleImportSummary> ScheduleService::importSchedule(
    const ScheduleImportPlan& plan
    ) const
{
    return scheduleWidgetCompatibilityData().importSchedule(plan);
}

Result<QList<IntensiveSlotState>> ScheduleService::intensiveSlotStates() const
{
    return scheduleWidgetCompatibilityData().loadIntensiveSlotStates();
}

Status ScheduleService::saveIntensiveSlotState(
    const QString& day,
    const QString& startTime,
    const QString& state,
    const QString& defaultState
    ) const
{
    return scheduleWidgetCompatibilityData().saveIntensiveSlotState(
        day,
        startTime,
        state,
        defaultState
        );
}

Result<QList<TestingAssignment>> ScheduleService::testingAssignments() const
{
    return scheduleWidgetCompatibilityData().loadTestingAssignments();
}

Result<QList<TestingBlock>> ScheduleService::testingBlocks() const
{
    return scheduleWidgetCompatibilityData().loadTestingBlocks();
}

Status ScheduleService::saveTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room,
    bool replaceExisting
    ) const
{
    return scheduleWidgetCompatibilityData().saveTestingBlock(
        day,
        startTime,
        room,
        replaceExisting
        );
}

Status ScheduleService::assignTestingClass(
    const QString& day,
    const QString& startTime,
    int classId,
    bool replaceExisting
    ) const
{
    return scheduleWidgetCompatibilityData().assignTestingClass(
        day,
        startTime,
        classId,
        replaceExisting
        );
}

Status ScheduleService::deleteTestingAssignment(
    const QString& day,
    const QString& startTime
    ) const
{
    return scheduleWidgetCompatibilityData().deleteTestingAssignment(day, startTime);
}

Status ScheduleService::deleteTestingBlock(
    const QString& day,
    const QString& startTime
    ) const
{
    return scheduleWidgetCompatibilityData().deleteTestingBlock(day, startTime);
}

Status ScheduleService::clearTestingAssignments() const
{
    return scheduleWidgetCompatibilityData().clearTestingAssignments();
}

Status ScheduleService::clearTestingBlocks() const
{
    return scheduleWidgetCompatibilityData().clearTestingBlocks();
}

Result<int> ScheduleService::createTestingClass(
    const TestingClass& testingClass,
    const QString& assignmentDay,
    const QString& assignmentStartTime
    ) const
{
    return scheduleWidgetCompatibilityData().createTestingClass(
        testingClass,
        assignmentDay,
        assignmentStartTime
        );
}

Status ScheduleService::updateTestingClass(
    const TestingClass& testingClass
    ) const
{
    return scheduleWidgetCompatibilityData().updateTestingClass(testingClass);
}

Result<TestingClass> ScheduleService::testingClass(int classId) const
{
    return scheduleWidgetCompatibilityData().loadTestingClass(classId);
}

Result<QList<TestingClass>> ScheduleService::testingClasses() const
{
    return scheduleWidgetCompatibilityData().loadTestingClasses();
}

Status ScheduleService::deleteTestingClass(int classId) const
{
    return scheduleWidgetCompatibilityData().deleteTestingClass(classId);
}

Result<bool> ScheduleService::isTestingClass(int classId) const
{
    return scheduleWidgetCompatibilityData().isTestingClass(classId);
}

Status RosterService::saveRoster(
    int classId,
    const Roster& roster,
    bool allowQuestionableKoreanNameLengths
    ) const
{
    Q_UNUSED(allowQuestionableKoreanNameLengths);
    return scheduleWidgetCompatibilityData().saveRoster(classId, roster);
}

Status RosterService::saveRosters(
    const QList<QPair<int, Roster>>& rosters
    ) const
{
    return scheduleWidgetCompatibilityData().saveRosters(rosters);
}

Result<Roster> RosterService::roster(int classId) const
{
    return scheduleWidgetCompatibilityData().loadRoster(classId);
}

Result<int> RosterService::studentCount(int classId) const
{
    return scheduleWidgetCompatibilityData().getRosterStudentCount(classId);
}

Status SpeakingEvaluationService::saveEvaluation(
    int classId,
    const QString& evaluationName,
    const SpeakingEvalRows& rows,
    const QList<SpeakingEvalCellChange>& dirtyCells,
    bool allowQuestionableKoreanNameLengths
    ) const
{
    Q_UNUSED(dirtyCells);
    Q_UNUSED(allowQuestionableKoreanNameLengths);
    return scheduleWidgetCompatibilityData().saveSpeakingEval(
        classId,
        evaluationName,
        rows
        );
}

Result<SpeakingEvalRows> SpeakingEvaluationService::evaluation(
    int classId,
    const QString& evaluationName
    ) const
{
    return scheduleWidgetCompatibilityData().loadSpeakingEval(
        classId,
        evaluationName
        );
}

#ifndef CLASSMNGR_TEST_USE_REAL_RESOURCE_PACK_MANAGER
ResourcePackManager& ResourcePackManager::instance()
{
    static ResourcePackManager manager;
    return manager;
}

ResourcePackManager::ResourcePackManager(
    QString storageDirectory,
    QString baselineDirectory
    )
    : m_storageDirectory(std::move(storageDirectory))
    , m_baselineDirectory(std::move(baselineDirectory))
{
}

QString ResourcePackManager::activeRoot(
    const QString&
    ) const
{
    return {};
}
#endif

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
    ClassService* classService
    )
    : m_classService(classService)
{
}

Result<ScheduleBuildResult> ScheduleBuilder::build(
    bool,
    const QStringList& visibleDays
    ) const
{
    ScheduleBuildResult result;
    result.days = visibleDays;

    if (!m_classService || !m_classService->isAvailable())
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
    : DialogShell(QStringLiteral("scheduleEditor"), parent)
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
