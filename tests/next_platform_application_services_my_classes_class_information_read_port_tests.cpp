#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "next/application/my_classes_class_information_read_query.h"
#include "next/platform/application_services_my_classes_class_information_read_port.h"

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
        QStringLiteral("my-classes-class-information-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

}

class NextPlatformApplicationServicesMyClassesClassInformationReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsDisplayedFieldsTeacherAssociationAndSchedulesOnce();
    void reportsUnavailableServices();
};

void NextPlatformApplicationServicesMyClassesClassInformationReadPortTests::
readsDisplayedFieldsTeacherAssociationAndSchedulesOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("My Classes read test")
        );
    QVERIFY(createdClass);

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Read Test Teacher");
    const auto teacherId = services.databaseSession()
        ->teacherRepository()->createTeacher(teacher);
    QVERIFY(teacherId);

    ClassInfo info;
    info.classId = *createdClass;
    info.teacherId = *teacherId;
    info.classGrade = QStringLiteral("E4 \U0001F9ED");
    info.classLevel = QStringLiteral("Theseus \uD559\uAE09");
    info.classTimes = {
        {
            .day = QStringLiteral("Wednesday"),
            .startTime = QStringLiteral("04:00 pm"),
            .endTime = QStringLiteral("04:50 pm")
        },
        {
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("09:15 AM"),
            .endTime = QStringLiteral("10:05 AM")
        }
    };
    info.intensiveTimes = {
        {
            .day = QStringLiteral("Friday"),
            .startTime = QStringLiteral("02:00 PM"),
            .endTime = QStringLiteral("04:00 PM")
        }
    };
    info.notes = QStringLiteral("Class notes\n\U0001F9ED");
    info.timeFillerActivities = QStringLiteral("Filler \U0001F4DA");
    QVERIFY(services.databaseSession()->classInfoRepository()
        ->saveClassInfo(info));

    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    ClassInfoRepository* const repository = session->classInfoRepository();
    QVERIFY(repository);
    const int readsBefore = repository
        ->scheduleClassInfoReadMetrics().singleClassInfoReadCount;

    Platform::ApplicationServicesMyClassesClassInformationReadPort port(
        services
        );
    const Application::MyClassesClassInformationReadQuery query(port);
    const auto loaded = query.execute(classId(*createdClass));

    QVERIFY(loaded);
    const Application::MyClassesClassInformationFields expected{
        .classGrade = u"E4 \U0001F9ED",
        .classLevel = u"Theseus \uD559\uAE09",
        .regularSchedule = {
            {u"Wednesday", u"04:00 pm", u"04:50 pm"},
            {u"Monday", u"09:15 AM", u"10:05 AM"}
        },
        .intensiveSchedule = {
            {u"Friday", u"02:00 PM", u"04:00 PM"}
        },
        .notes = u"Class notes\n\U0001F9ED",
        .timeFillerActivities = u"Filler \U0001F4DA",
        .teacherId = Domain::TeacherId::fromString(
            std::to_string(*teacherId)
            )
    };
    QCOMPARE(loaded.value().classId, classId(*createdClass));
    QVERIFY(loaded.value().fields == expected);
    QCOMPARE(repository->scheduleClassInfoReadMetrics()
        .singleClassInfoReadCount, readsBefore + 1);
}

void NextPlatformApplicationServicesMyClassesClassInformationReadPortTests::
reportsUnavailableServices()
{
    const Domain::ClassId id = classId(7);
    Platform::ApplicationServicesMyClassesClassInformationReadPort nullPort(
        nullptr
        );

    const auto loaded = nullPort.readMyClassesClassInformation(id);

    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!loaded.error().message.empty());
}

QTEST_MAIN(
    NextPlatformApplicationServicesMyClassesClassInformationReadPortTests
    )

#include "next_platform_application_services_my_classes_class_information_read_port_tests.moc"
