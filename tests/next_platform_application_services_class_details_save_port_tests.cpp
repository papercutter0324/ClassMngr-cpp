#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "next/application/class_details_save_use_case.h"
#include "next/platform/application_services_class_details_save_port.h"

#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>

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
    void unavailableSessionReturnsStructuredFailure();
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
        .regularTimes = {scheduleTime(1, 18 * 60, 18 * 60 + 50)},
        .intensiveTimes = {scheduleTime(6, 19 * 60, 19 * 60 + 45)}
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
    QCOMPARE(actual->classTimes.size(), 1);
    QVERIFY(sameTime(
        actual->classTimes.front(),
        QStringLiteral("Tuesday"),
        QStringLiteral("6:00 PM"),
        QStringLiteral("6:50 PM")
        ));
    QCOMPARE(actual->intensiveTimes.size(), 1);
    QVERIFY(sameTime(
        actual->intensiveTimes.front(),
        QStringLiteral("Sunday"),
        QStringLiteral("7:00 PM"),
        QStringLiteral("7:45 PM")
        ));
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

QTEST_MAIN(NextPlatformApplicationServicesClassDetailsSavePortTests)

#include "next_platform_application_services_class_details_save_port_tests.moc"
