#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/class_details_save_use_case.h"
#include "next/platform/application_services_class_details_save_port.h"

#include <QTemporaryDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <limits>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("class-details-save-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

int createTeacher(ApplicationServices& services)
{
    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Preserved Teacher");
    teacher.preferredName = teacher.teacherEn;
    const auto saved = services.teacherService()->save(teacher);
    return saved ? *saved : -1;
}

Domain::ScheduleTime scheduleTime(
    const int weekday,
    const int startMinute,
    const int endMinute
    )
{
    const auto result = Domain::ScheduleTime::fromMinutes(
        weekday,
        startMinute,
        endMinute
        );
    if (!result)
    {
        qFatal("Test schedule must be valid.");
    }
    return *result;
}

Application::ClassDetailsSaveRequest requestForClass(const int classId)
{
    return {
        .classId = *Domain::ClassId::fromString(std::to_string(classId)),
        .classGrade = u"E4",
        .classLevel = u"Theseus",
        .readingBook = u"Reading Explorer 1",
        .essayBook = u"4A",
        .classColor = u"#112233",
        .fontColor = u"#445566"
    };
}

bool sameTime(
    const ClassTime& value,
    const QString& day,
    const QString& start,
    const QString& end
    )
{
    return value.day == day && value.startTime == start && value.endTime == end;
}

bool sameTimes(
    const QList<ClassTime>& left,
    const QList<ClassTime>& right
    )
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (qsizetype index = 0; index < left.size(); ++index)
    {
        if (!sameTime(
                left[index],
                right[index].day,
                right[index].startTime,
                right[index].endTime
                ))
        {
            return false;
        }
    }

    return true;
}

void verifyStaleScheduleConflictRejected(const ScheduleType type)
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString profilePath = databasePath(directory);
    ApplicationServices firstServices;
    QVERIFY(firstServices.openDatabase(profilePath));

    const QString scenario = type == ScheduleType::Regular
        ? QStringLiteral("Regular")
        : QStringLiteral("Intensive");
    const auto winnerId = firstServices.classService()->create(
        QStringLiteral("F958 %1 Winner").arg(scenario)
        );
    const auto losingId = firstServices.classService()->create(
        QStringLiteral("F958 %1 Losing").arg(scenario)
        );
    QVERIFY(winnerId);
    QVERIFY(losingId);

    auto winnerSeed = firstServices.classService()->classInfo(*winnerId);
    auto losingSeed = firstServices.classService()->classInfo(*losingId);
    QVERIFY(winnerSeed);
    QVERIFY(losingSeed);

    winnerSeed->classColor = QStringLiteral("#123456");
    winnerSeed->fontColor = QStringLiteral("#654321");
    winnerSeed->notes = QStringLiteral("Winner notes before");
    winnerSeed->timeFillerActivities = QStringLiteral("Winner filler before");
    winnerSeed->classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:50 AM")}
    };
    winnerSeed->intensiveTimes = {
        {QStringLiteral("Friday"), QStringLiteral("10:00 AM"), QStringLiteral("10:50 AM")}
    };
    const auto winnerSeedSaved =
        firstServices.classService()->saveClassInfo(*winnerSeed);
    QVERIFY2(
        winnerSeedSaved.has_value(),
        winnerSeedSaved ? "" : qPrintable(winnerSeedSaved.error())
        );

    losingSeed->classColor = QStringLiteral("#234567");
    losingSeed->fontColor = QStringLiteral("#765432");
    losingSeed->notes = QStringLiteral("Losing notes before");
    losingSeed->timeFillerActivities = QStringLiteral("Losing filler before");
    losingSeed->classTimes = {
        {QStringLiteral("Tuesday"), QStringLiteral("9:00 AM"), QStringLiteral("9:50 AM")}
    };
    losingSeed->intensiveTimes = {
        {QStringLiteral("Saturday"), QStringLiteral("10:00 AM"), QStringLiteral("10:50 AM")}
    };
    const auto losingSeedSaved =
        firstServices.classService()->saveClassInfo(*losingSeed);
    QVERIFY2(
        losingSeedSaved.has_value(),
        losingSeedSaved ? "" : qPrintable(losingSeedSaved.error())
        );

    ApplicationServices secondServices;
    QVERIFY(secondServices.openDatabase(profilePath));
    ClassInfoRepository* const firstRepository =
        firstServices.databaseSession()->classInfoRepository();
    ClassInfoRepository* const secondRepository =
        secondServices.databaseSession()->classInfoRepository();
    QVERIFY(firstRepository);
    QVERIFY(secondRepository);

    const auto winnerBefore =
        firstRepository->loadClassDetailsSaveInfo(*winnerId);
    const auto losingBefore =
        secondRepository->loadClassDetailsSaveInfo(*losingId);
    QVERIFY(winnerBefore);
    QVERIFY(losingBefore);
    QCOMPARE(losingBefore->info.classTimes.size(), 1);
    QCOMPARE(losingBefore->info.intensiveTimes.size(), 1);

    ClassInfo winnerCandidate = winnerBefore->info;
    ClassInfo losingCandidate = losingBefore->info;
    winnerCandidate.classColor = QStringLiteral("#A1B2C3");
    winnerCandidate.fontColor = QStringLiteral("#C3B2A1");
    winnerCandidate.notes = QStringLiteral("Winner notes after");
    winnerCandidate.timeFillerActivities = QStringLiteral("Winner filler after");
    losingCandidate.classColor = QStringLiteral("#AABBCC");
    losingCandidate.fontColor = QStringLiteral("#CCBBAA");
    losingCandidate.notes = QStringLiteral("Losing notes attempted");
    losingCandidate.timeFillerActivities = QStringLiteral("Losing filler attempted");

    const ClassTime conflictSlot = type == ScheduleType::Regular
        ? ClassTime{
            QStringLiteral("Wednesday"),
            QStringLiteral("6:00 PM"),
            QStringLiteral("6:50 PM")
        }
        : ClassTime{
            QStringLiteral("Thursday"),
            QStringLiteral("7:00 PM"),
            QStringLiteral("7:50 PM")
        };
    if (type == ScheduleType::Regular)
    {
        winnerCandidate.classTimes = {conflictSlot};
        losingCandidate.classTimes = {conflictSlot};
    }
    else
    {
        winnerCandidate.intensiveTimes = {conflictSlot};
        losingCandidate.intensiveTimes = {conflictSlot};
    }

    const QList<ClassTime> candidateTimes{conflictSlot};
    const auto winnerPrecheck = firstRepository->getClassTimeConflicts(
        *winnerId,
        candidateTimes,
        type
        );
    const auto losingPrecheck = secondRepository->getClassTimeConflicts(
        *losingId,
        candidateTimes,
        type
        );
    QVERIFY(winnerPrecheck);
    QVERIFY(losingPrecheck);
    QVERIFY(winnerPrecheck->isEmpty());
    QVERIFY(losingPrecheck->isEmpty());

    const auto winnerSaved = firstRepository->saveClassDetailsDetailed(
        winnerCandidate,
        winnerBefore->teacherId
        );
    QVERIFY2(
        winnerSaved.has_value(),
        winnerSaved ? "" : qPrintable(winnerSaved.error().diagnostic)
        );

    const auto losingSaved = secondRepository->saveClassDetailsDetailed(
        losingCandidate,
        losingBefore->teacherId
        );
    QVERIFY(!losingSaved);
    QCOMPARE(
        losingSaved.error().kind,
        ClassInfoRepository::ClassDetailsSaveFailure::Kind::Technical
        );
    const QString expectedDiagnostic = QStringLiteral(
        "Class schedule conflict: %1 %2\u2013%3 conflicts with %4."
        ).arg(
            conflictSlot.day,
            conflictSlot.startTime,
            conflictSlot.endTime,
            QStringLiteral("F958 %1 Winner").arg(scenario)
            );
    QCOMPARE(losingSaved.error().diagnostic, expectedDiagnostic);

    const auto losingAfter =
        secondRepository->loadClassDetailsSaveInfo(*losingId);
    QVERIFY(losingAfter);
    QCOMPARE(losingAfter->teacherId, losingBefore->teacherId);
    QCOMPARE(losingAfter->info.teacherId, losingBefore->info.teacherId);
    QCOMPARE(losingAfter->info.classGrade, losingBefore->info.classGrade);
    QCOMPARE(losingAfter->info.classLevel, losingBefore->info.classLevel);
    QCOMPARE(losingAfter->info.readingBook, losingBefore->info.readingBook);
    QCOMPARE(losingAfter->info.essayBook, losingBefore->info.essayBook);
    QCOMPARE(losingAfter->info.classColor, losingBefore->info.classColor);
    QCOMPARE(losingAfter->info.fontColor, losingBefore->info.fontColor);
    QCOMPARE(losingAfter->info.notes, losingBefore->info.notes);
    QCOMPARE(
        losingAfter->info.timeFillerActivities,
        losingBefore->info.timeFillerActivities
        );
    QVERIFY(sameTimes(losingAfter->info.classTimes, losingBefore->info.classTimes));
    QVERIFY(sameTimes(
        losingAfter->info.intensiveTimes,
        losingBefore->info.intensiveTimes
        ));
}

}

class NextPlatformApplicationServicesClassDetailsSavePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void savesEditedFieldsAndPreservesHiddenClassInfo();
    void teacherOverrideIsAppliedToSavedClassInfo();
    void savesWideTeacherIdAtomicallyWithClassDetails();
    void rejectsMalformedTeacherOverrideAtTheAdapterBoundary();
    void absentSchedulesPreservePersistedSchedulesIndependently();
    void missingParentReturnsNotFoundBeforeDetailAndConflictReads();
    void legacyMissingParentSavePreservesForeignKeyDiagnostic();
    void unavailableSessionReturnsStructuredFailure();
    void classInfoReadFailureReturnsTechnicalFailure();
    void classInfoSaveFailureReturnsTechnicalFailure();
    void invalidPreservedDataFailsMergedClassInfoValidation();
    void scheduleConflictRetainsClassInfoValidationBehavior();
    void staleRegularConflictIsRejectedInsideTypedSave();
    void staleIntensiveConflictIsRejectedInsideTypedSave();
};

void NextPlatformApplicationServicesClassDetailsSavePortTests::
savesWideTeacherIdAtomicallyWithClassDetails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QSqlQuery sql(services.databaseSession()->database());
    QVERIFY(sql.exec(QStringLiteral(
        "INSERT INTO teachers (id, teacher_kr, teacher_en) "
        "VALUES (2147483648, 'Wide Teacher', 'Wide Teacher')")));
    QVERIFY(sql.exec(QStringLiteral("SELECT id, typeof(id) FROM teachers")));
    QVERIFY(sql.next());
    QCOMPARE(sql.value(0).toLongLong(), qint64(2147483648LL));
    QCOMPARE(sql.value(1).toString(), QStringLiteral("integer"));
    const auto classId = services.classService()->create(QStringLiteral("Wide Teacher Class"));
    QVERIFY(classId);
    auto original = services.classService()->classInfo(*classId);
    QVERIFY(original);
    original->notes = QStringLiteral("Keep wide teacher notes");
    original->timeFillerActivities = QStringLiteral("Keep activities");
    QVERIFY(services.classService()->saveClassInfo(*original));

    auto request = requestForClass(*classId);
    request.teacherId = Domain::TeacherId::fromString("2147483648");
    request.regularTimes = std::vector<Domain::ScheduleTime>{scheduleTime(0, 540, 590)};
    request.intensiveTimes = std::vector<Domain::ScheduleTime>{scheduleTime(4, 600, 650)};
    Platform::ApplicationServicesClassDetailsSavePort port(services);
    const auto direct = port.saveClassDetails(request);
    QVERIFY(direct);
    const auto result = Application::ClassDetailsSaveUseCase::execute(request, port);
    QVERIFY(result);
    sql.prepare(QStringLiteral("SELECT teacher_id, typeof(teacher_id), notes, time_filler_activities FROM class_info WHERE class_id=?"));
    sql.addBindValue(*classId);
    QVERIFY(sql.exec());
    QVERIFY(sql.next());
    QCOMPARE(sql.value(0).toLongLong(), qint64(2147483648LL));
    QCOMPARE(sql.value(1).toString(), QStringLiteral("integer"));
    QCOMPARE(sql.value(2).toString(), original->notes);
    QCOMPARE(sql.value(3).toString(), original->timeFillerActivities);
    const auto saved = services.classService()->classInfo(*classId);
    QVERIFY(saved);
    QCOMPARE(saved->classTimes.size(), 1);
    QCOMPARE(saved->intensiveTimes.size(), 1);

    // A later edit without a teacher override preserves the wide assignment.
    request.teacherId.reset();
    request.classColor = u"#778899";
    request.regularTimes.reset();
    request.intensiveTimes.reset();
    const auto preserved = Application::ClassDetailsSaveUseCase::execute(request, port);
    QVERIFY2(preserved, preserved ? "" : preserved.error().message.c_str());
    sql.prepare(QStringLiteral("SELECT teacher_id, typeof(teacher_id), class_color FROM class_info WHERE class_id=?"));
    sql.addBindValue(*classId);
    QVERIFY(sql.exec());
    QVERIFY(sql.next());
    QCOMPARE(sql.value(0).toLongLong(), qint64(2147483648LL));
    QCOMPARE(sql.value(1).toString(), QStringLiteral("integer"));
    QCOMPARE(sql.value(2).toString(), QStringLiteral("#778899"));
    sql.finish();

    // A schedule failure must roll back identity, details and both schedules.
    QVERIFY(sql.exec(QStringLiteral(
        "INSERT INTO teachers (id, teacher_kr, teacher_en) "
        "VALUES (9223372036854775807, 'Max Teacher', 'Max Teacher')")));
    QVERIFY(sql.exec(QStringLiteral(
        "CREATE TRIGGER reject_wide_teacher_schedule BEFORE INSERT ON class_intensive_times "
        "BEGIN SELECT RAISE(ABORT, 'F838 schedule failure'); END")));
    request.teacherId = Domain::TeacherId::fromString("9223372036854775807");
    request.classColor = u"#AABBCC";
    request.regularTimes = std::vector<Domain::ScheduleTime>{scheduleTime(1, 660, 710)};
    request.intensiveTimes = std::vector<Domain::ScheduleTime>{scheduleTime(5, 720, 770)};
    const auto failed = Application::ClassDetailsSaveUseCase::execute(request, port);
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    sql.prepare(QStringLiteral("SELECT teacher_id, typeof(teacher_id), class_color, notes FROM class_info WHERE class_id=?"));
    sql.addBindValue(*classId);
    QVERIFY(sql.exec());
    QVERIFY(sql.next());
    QCOMPARE(sql.value(0).toLongLong(), qint64(2147483648LL));
    QCOMPARE(sql.value(1).toString(), QStringLiteral("integer"));
    QCOMPARE(sql.value(2).toString(), QStringLiteral("#778899"));
    QCOMPARE(sql.value(3).toString(), original->notes);
    sql.finish();
    const auto afterFailure = services.classService()->classInfo(*classId);
    QVERIFY(afterFailure);
    QCOMPARE(afterFailure->classTimes.size(), 1);
    QVERIFY(sameTime(afterFailure->classTimes.front(), QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"), QStringLiteral("9:50 AM")));
    QCOMPARE(afterFailure->intensiveTimes.size(), 1);
    QVERIFY(sameTime(afterFailure->intensiveTimes.front(), QStringLiteral("Friday"),
        QStringLiteral("10:00 AM"), QStringLiteral("10:50 AM")));

    QVERIFY(sql.exec(QStringLiteral("DROP TRIGGER reject_wide_teacher_schedule")));
    QVERIFY(Application::ClassDetailsSaveUseCase::execute(request, port));
    sql.prepare(QStringLiteral("SELECT teacher_id, typeof(teacher_id) FROM class_info WHERE class_id=?"));
    sql.addBindValue(*classId);
    QVERIFY(sql.exec());
    QVERIFY(sql.next());
    QCOMPARE(sql.value(0).toLongLong(), std::numeric_limits<qint64>::max());
    QCOMPARE(sql.value(1).toString(), QStringLiteral("integer"));
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
rejectsMalformedTeacherOverrideAtTheAdapterBoundary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto classId = services.classService()->create(QStringLiteral("Invalid Teacher Class"));
    QVERIFY(classId);
    Platform::ApplicationServicesClassDetailsSavePort port(services);
    auto request = requestForClass(*classId);
    for (const std::string id : {
             "0", "08", "-2", "+8", " 8", "8x", "9223372036854775808"})
    {
        request.teacherId = Domain::TeacherId::fromString(id);
        const auto result = port.saveClassDetails(request);
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
        QCOMPARE(result.error().message, std::string("Teacher ID must be a canonical positive integer."));
    }
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
savesEditedFieldsAndPreservesHiddenClassInfo()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Class Details Save Test")
        );
    QVERIFY(createdClass);
    const int teacherId = createTeacher(services);
    QVERIFY(teacherId > 0);

    const auto loaded = services.classService()->classInfo(*createdClass);
    QVERIFY(loaded);
    ClassInfo original = *loaded;
    original.teacherId = teacherId;
    original.classGrade = QStringLiteral("E4");
    original.classLevel = QStringLiteral("Theseus");
    original.classColor = QStringLiteral("#112233");
    original.fontColor = QStringLiteral("#445566");
    original.notes = QStringLiteral("Keep these notes");
    original.timeFillerActivities = QStringLiteral("Keep these activities");
    original.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"),
         QStringLiteral("9:50 AM")}
    };
    original.intensiveTimes = {
        {QStringLiteral("Friday"), QStringLiteral("10:00 AM"),
         QStringLiteral("10:50 AM")}
    };
    QVERIFY(services.classService()->saveClassInfo(original));

    Application::ClassDetailsSaveRequest request{
        .classId = *Domain::ClassId::fromString(
            std::to_string(*createdClass)
            ),
        .classGrade = u"E5",
        .classLevel = u"Artemis",
        .readingBook = u"",
        .essayBook = u"",
        .classColor = u"#123456",
        .fontColor = u"#654321",
        .regularTimes = std::vector<Domain::ScheduleTime>{
            scheduleTime(1, 18 * 60, 18 * 60 + 50),
            scheduleTime(0, 17 * 60, 17 * 60 + 50)
        },
        .intensiveTimes = std::vector<Domain::ScheduleTime>{
            scheduleTime(6, 19 * 60, 19 * 60 + 45),
            scheduleTime(4, 16 * 60, 16 * 60 + 45)
        }
    };

    Platform::ApplicationServicesClassDetailsSavePort port(services);
    const auto saved =
        Application::ClassDetailsSaveUseCase::execute(request, port);
    QVERIFY(saved);

    const auto actual = services.classService()->classInfo(*createdClass);
    QVERIFY(actual);
    QCOMPARE(actual->classId, *createdClass);
    QCOMPARE(actual->classGrade, QStringLiteral("E5"));
    QCOMPARE(actual->classLevel, QStringLiteral("Artemis"));
    QCOMPARE(actual->readingBook, QString());
    QCOMPARE(actual->essayBook, QString());
    QCOMPARE(actual->classColor, QStringLiteral("#123456"));
    QCOMPARE(actual->fontColor, QStringLiteral("#654321"));
    QCOMPARE(actual->teacherId, teacherId);
    QCOMPARE(actual->notes, QStringLiteral("Keep these notes"));
    QCOMPARE(actual->timeFillerActivities,
        QStringLiteral("Keep these activities"));
    QCOMPARE(actual->classTimes.size(), 2);
    QVERIFY(sameTime(
        actual->classTimes.front(),
        QStringLiteral("Tuesday"),
        QStringLiteral("6:00 PM"),
        QStringLiteral("6:50 PM")
        ));
    QVERIFY(sameTime(
        actual->classTimes[1],
        QStringLiteral("Monday"),
        QStringLiteral("5:00 PM"),
        QStringLiteral("5:50 PM")
        ));
    QCOMPARE(actual->intensiveTimes.size(), 2);
    QVERIFY(sameTime(
        actual->intensiveTimes.front(),
        QStringLiteral("Sunday"),
        QStringLiteral("7:00 PM"),
        QStringLiteral("7:45 PM")
        ));
    QVERIFY(sameTime(
        actual->intensiveTimes[1],
        QStringLiteral("Friday"),
        QStringLiteral("4:00 PM"),
        QStringLiteral("4:45 PM")
        ));
    QVERIFY(request.regularTimes.has_value());
    QVERIFY(request.intensiveTimes.has_value());
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
teacherOverrideIsAppliedToSavedClassInfo()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Teacher Override Save Test")
        );
    QVERIFY(createdClass);
    const int originalTeacherId = createTeacher(services);
    const int selectedTeacherId = createTeacher(services);
    QVERIFY(originalTeacherId > 0);
    QVERIFY(selectedTeacherId > 0);

    auto original = services.classService()->classInfo(*createdClass);
    QVERIFY(original);
    original->teacherId = originalTeacherId;
    QVERIFY(services.classService()->saveClassInfo(*original));

    Application::ClassDetailsSaveRequest request =
        requestForClass(*createdClass);
    request.teacherId = Domain::TeacherId::fromString(
        std::to_string(selectedTeacherId)
        );
    Platform::ApplicationServicesClassDetailsSavePort port(services);
    const auto result =
        Application::ClassDetailsSaveUseCase::execute(request, port);

    QVERIFY(result);
    const auto saved = services.classService()->classInfo(*createdClass);
    QVERIFY(saved);
    QCOMPARE(saved->teacherId, selectedTeacherId);
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
absentSchedulesPreservePersistedSchedulesIndependently()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Class Details Schedule Preservation Test")
        );
    QVERIFY(createdClass);
    auto original = services.classService()->classInfo(*createdClass);
    QVERIFY(original);
    original->classGrade = QStringLiteral("E4");
    original->classLevel = QStringLiteral("Theseus");
    original->readingBook = QStringLiteral("Reading Explorer 1");
    original->essayBook = QStringLiteral("4A");
    original->classColor = QStringLiteral("#112233");
    original->fontColor = QStringLiteral("#445566");
    original->classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
    }};
    original->intensiveTimes = {{
        QStringLiteral("Friday"),
        QStringLiteral("10:00 AM"),
        QStringLiteral("10:50 AM")
    }};
    // Seed the exact stored rows at the repository seam. The legacy service
    // normalizes accepted time text before persistence, so this fixture uses
    // its supported canonical stored representation.
    QVERIFY(services.databaseSession());
    QVERIFY(services.databaseSession()->classInfoRepository());
    QVERIFY(services.databaseSession()->classInfoRepository()
        ->saveClassInfo(*original));

    const auto seeded = services.classService()->classInfo(*createdClass);
    QVERIFY(seeded);
    QCOMPARE(seeded->classTimes.size(), 1);
    QCOMPARE(seeded->classTimes.front().day, QStringLiteral("Monday"));
    QCOMPARE(seeded->classTimes.front().startTime, QStringLiteral("9:00 AM"));
    QCOMPARE(seeded->classTimes.front().endTime, QStringLiteral("9:50 AM"));
    QCOMPARE(seeded->intensiveTimes.size(), 1);
    QCOMPARE(seeded->intensiveTimes.front().day, QStringLiteral("Friday"));
    QCOMPARE(seeded->intensiveTimes.front().startTime,
        QStringLiteral("10:00 AM"));
    QCOMPARE(seeded->intensiveTimes.front().endTime,
        QStringLiteral("10:50 AM"));

    Platform::ApplicationServicesClassDetailsSavePort port(services);
    Application::ClassDetailsSaveRequest updateIntensive =
        requestForClass(*createdClass);
    updateIntensive.intensiveTimes = std::vector<Domain::ScheduleTime>{
        scheduleTime(6, 19 * 60, 19 * 60 + 45)
    };
    QVERIFY(!updateIntensive.regularTimes.has_value());
    QVERIFY(Application::ClassDetailsSaveUseCase::execute(
        updateIntensive,
        port
        ));

    auto afterIntensiveUpdate =
        services.classService()->classInfo(*createdClass);
    QVERIFY(afterIntensiveUpdate);
    QCOMPARE(afterIntensiveUpdate->classTimes.size(), 1);
    QVERIFY(sameTime(
        afterIntensiveUpdate->classTimes.front(),
        QStringLiteral("Monday"),
        QStringLiteral("9:00 AM"),
        QStringLiteral("9:50 AM")
        ));
    QCOMPARE(afterIntensiveUpdate->intensiveTimes.size(), 1);
    QVERIFY(sameTime(
        afterIntensiveUpdate->intensiveTimes.front(),
        QStringLiteral("Sunday"),
        QStringLiteral("7:00 PM"),
        QStringLiteral("7:45 PM")
        ));

    Application::ClassDetailsSaveRequest updateRegular =
        requestForClass(*createdClass);
    updateRegular.regularTimes = std::vector<Domain::ScheduleTime>{
        scheduleTime(1, 18 * 60, 18 * 60 + 50)
    };
    QVERIFY(!updateRegular.intensiveTimes.has_value());
    QVERIFY(Application::ClassDetailsSaveUseCase::execute(
        updateRegular,
        port
        ));

    auto afterRegularUpdate =
        services.classService()->classInfo(*createdClass);
    QVERIFY(afterRegularUpdate);
    QCOMPARE(afterRegularUpdate->classTimes.size(), 1);
    QVERIFY(sameTime(
        afterRegularUpdate->classTimes.front(),
        QStringLiteral("Tuesday"),
        QStringLiteral("6:00 PM"),
        QStringLiteral("6:50 PM")
        ));
    QCOMPARE(afterRegularUpdate->intensiveTimes.size(), 1);
    QVERIFY(sameTime(
        afterRegularUpdate->intensiveTimes.front(),
        QStringLiteral("Sunday"),
        QStringLiteral("7:00 PM"),
        QStringLiteral("7:45 PM")
        ));

    Application::ClassDetailsSaveRequest clearSchedules =
        requestForClass(*createdClass);
    clearSchedules.regularTimes = std::vector<Domain::ScheduleTime>{};
    clearSchedules.intensiveTimes = std::vector<Domain::ScheduleTime>{};
    QVERIFY(Application::ClassDetailsSaveUseCase::execute(
        clearSchedules,
        port
        ));

    const auto cleared = services.classService()->classInfo(*createdClass);
    QVERIFY(cleared);
    QVERIFY(cleared->classTimes.isEmpty());
    QVERIFY(cleared->intensiveTimes.isEmpty());
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
unavailableSessionReturnsStructuredFailure()
{
    ApplicationServices services;
    Platform::ApplicationServicesClassDetailsSavePort port(services);
    const Application::ClassDetailsSaveRequest request{
        .classId = *Domain::ClassId::fromString("42")
    };

    const auto result =
        Application::ClassDetailsSaveUseCase::execute(request, port);
    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
missingParentReturnsNotFoundBeforeDetailAndConflictReads()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Application::ClassDetailsSaveRequest request = requestForClass(100001);
    Platform::ApplicationServicesClassDetailsSavePort port(services);
    const auto emptyScheduleResult =
        Application::ClassDetailsSaveUseCase::execute(request, port);

    QVERIFY(!emptyScheduleResult);
    QCOMPARE(emptyScheduleResult.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!emptyScheduleResult.error().recoverable);
    QCOMPARE(
        emptyScheduleResult.error().message,
        std::string(
            "Saving class information failed: parent class id 100001 was not found."
            )
        );

    request.regularTimes = std::vector<Domain::ScheduleTime>{
        scheduleTime(0, 9 * 60, 9 * 60 + 50)
    };
    const auto scheduledResult =
        Application::ClassDetailsSaveUseCase::execute(request, port);
    QVERIFY(!scheduledResult);
    QCOMPARE(scheduledResult.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!scheduledResult.error().recoverable);

    QSqlQuery rowCounts(services.databaseSession()->database());
    QVERIFY(rowCounts.exec(QStringLiteral(
        "SELECT "
        "(SELECT COUNT(*) FROM class_info WHERE class_id=100001), "
        "(SELECT COUNT(*) FROM class_times WHERE class_id=100001), "
        "(SELECT COUNT(*) FROM class_intensive_times WHERE class_id=100001)"
        )));
    QVERIFY(rowCounts.next());
    QCOMPARE(rowCounts.value(0).toInt(), 0);
    QCOMPARE(rowCounts.value(1).toInt(), 0);
    QCOMPARE(rowCounts.value(2).toInt(), 0);
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
legacyMissingParentSavePreservesForeignKeyDiagnostic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    QVERIFY(services.databaseSession());
    ClassInfoRepository* const repository =
        services.databaseSession()->classInfoRepository();
    QVERIFY(repository);

    ClassInfo missingParent;
    missingParent.classId = 100001;
    const Status saved = repository->saveClassDetails(missingParent, -1);

    QVERIFY(!saved);
    QCOMPARE(
        saved.error(),
        QStringLiteral(
            "Saving class information failed for class id 100001: "
            "FOREIGN KEY constraint failed Unable to fetch row "
            "(database error 787)"
            )
        );

    QSqlQuery rowCounts(services.databaseSession()->database());
    QVERIFY(rowCounts.exec(QStringLiteral(
        "SELECT "
        "(SELECT COUNT(*) FROM class_info WHERE class_id=100001), "
        "(SELECT COUNT(*) FROM class_times WHERE class_id=100001), "
        "(SELECT COUNT(*) FROM class_intensive_times WHERE class_id=100001)"
        )));
    QVERIFY(rowCounts.next());
    QCOMPARE(rowCounts.value(0).toInt(), 0);
    QCOMPARE(rowCounts.value(1).toInt(), 0);
    QCOMPARE(rowCounts.value(2).toInt(), 0);
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
classInfoReadFailureReturnsTechnicalFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Class Details Read Failure")
        );
    QVERIFY(createdClass);

    QSqlQuery damageClassRead(services.databaseSession()->database());
    QVERIFY2(
        damageClassRead.exec(QStringLiteral("DROP TABLE class_times")),
        qPrintable(damageClassRead.lastError().text())
        );

    Platform::ApplicationServicesClassDetailsSavePort port(services);
    const auto result = Application::ClassDetailsSaveUseCase::execute(
        requestForClass(*createdClass),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
classInfoSaveFailureReturnsTechnicalFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Class Details Save Failure")
        );
    QVERIFY(createdClass);
    const auto original = services.classService()->classInfo(*createdClass);
    QVERIFY(original);
    QVERIFY(services.classService()->saveClassInfo(*original));

    QSqlQuery installFailure(services.databaseSession()->database());
    QVERIFY2(
        installFailure.exec(QStringLiteral(
            "CREATE TRIGGER fail_class_details_save "
            "BEFORE UPDATE ON class_info "
            "BEGIN SELECT RAISE(FAIL, 'save denied'); END"
            )),
        qPrintable(installFailure.lastError().text())
        );

    Platform::ApplicationServicesClassDetailsSavePort port(services);
    const auto result = Application::ClassDetailsSaveUseCase::execute(
        requestForClass(*createdClass),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("save denied")
        ));
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
invalidPreservedDataFailsMergedClassInfoValidation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Class Details Invalid Hidden Data")
        );
    QVERIFY(createdClass);
    auto original = services.classService()->classInfo(*createdClass);
    QVERIFY(original);
    QVERIFY(services.classService()->saveClassInfo(*original));

    const QString invalidNotes(10001, QChar(u'x'));
    QSqlQuery damageHiddenField(services.databaseSession()->database());
    damageHiddenField.prepare(QStringLiteral(
        "UPDATE class_info SET notes=? WHERE class_id=?"
        ));
    damageHiddenField.addBindValue(invalidNotes);
    damageHiddenField.addBindValue(*createdClass);
    QVERIFY2(
        damageHiddenField.exec(),
        qPrintable(damageHiddenField.lastError().text())
        );
    QCOMPARE(damageHiddenField.numRowsAffected(), 1);

    Platform::ApplicationServicesClassDetailsSavePort port(services);
    const auto result = Application::ClassDetailsSaveUseCase::execute(
        requestForClass(*createdClass),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString message = QString::fromStdString(result.error().message);
    QVERIFY(message.contains(QStringLiteral("Class information validation failed")));
    QVERIFY(message.contains(QStringLiteral("notes")));

    const auto unchanged = services.classService()->classInfo(*createdClass);
    QVERIFY(unchanged);
    QCOMPARE(unchanged->notes, invalidNotes);
    QVERIFY(unchanged->classGrade.isEmpty());
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
scheduleConflictRetainsClassInfoValidationBehavior()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto conflictingClass = services.classService()->create(
        QStringLiteral("Existing Class Details Schedule")
        );
    QVERIFY(conflictingClass);
    const auto selectedClass = services.classService()->create(
        QStringLiteral("Selected Class Details Schedule")
        );
    QVERIFY(selectedClass);

    auto existingInfo = services.classService()->classInfo(*conflictingClass);
    QVERIFY(existingInfo);
    existingInfo->classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("6:00 PM"),
         QStringLiteral("6:50 PM")}
    };
    QVERIFY(services.classService()->saveClassInfo(*existingInfo));

    Application::ClassDetailsSaveRequest request =
        requestForClass(*selectedClass);
    request.regularTimes = std::vector<Domain::ScheduleTime>{
        scheduleTime(0, 18 * 60, 18 * 60 + 50)
    };
    Platform::ApplicationServicesClassDetailsSavePort port(services);
    const auto result =
        Application::ClassDetailsSaveUseCase::execute(request, port);

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("Class schedule conflict")
        ));

    const auto unchanged = services.classService()->classInfo(*selectedClass);
    QVERIFY(unchanged);
    QVERIFY(unchanged->classTimes.isEmpty());
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
staleRegularConflictIsRejectedInsideTypedSave()
{
    verifyStaleScheduleConflictRejected(ScheduleType::Regular);
}

void NextPlatformApplicationServicesClassDetailsSavePortTests::
staleIntensiveConflictIsRejectedInsideTypedSave()
{
    verifyStaleScheduleConflictRejected(ScheduleType::Intensive);
}

QTEST_MAIN(NextPlatformApplicationServicesClassDetailsSavePortTests)

#include "next_platform_application_services_class_details_save_port_tests.moc"
