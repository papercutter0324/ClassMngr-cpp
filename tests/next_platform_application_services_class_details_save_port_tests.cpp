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

}

class NextPlatformApplicationServicesClassDetailsSavePortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void savesEditedFieldsAndPreservesHiddenClassInfo();
    void absentSchedulesPreservePersistedSchedulesIndependently();
    void unavailableSessionReturnsStructuredFailure();
    void classInfoReadFailureReturnsTechnicalFailure();
    void classInfoSaveFailureReturnsTechnicalFailure();
    void invalidPreservedDataFailsMergedClassInfoValidation();
    void scheduleConflictRetainsClassInfoValidationBehavior();
};

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
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("unavailable")
        ));
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

QTEST_MAIN(NextPlatformApplicationServicesClassDetailsSavePortTests)

#include "next_platform_application_services_class_details_save_port_tests.moc"
