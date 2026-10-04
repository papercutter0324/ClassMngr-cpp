#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "next/application/my_classes_class_information_batch_read_query.h"
#include "next/platform/application_services_my_classes_class_information_batch_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
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
        QStringLiteral("my-classes-class-information-batch-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto created = services.classService()->create(name);
    return created ? *created : 0;
}

}

class NextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsCompactFieldsInOneOrderedBatchAndKeepsMissingRowDefault();
    void keepsPerClassRepositoryFailuresAlongsideSuccessfulSiblings();
    void batchSqlFailureReturnsIndependentEntriesForEveryClass();
    void emptyInputAvoidsRepositoryWork();
    void reportsUnavailableServices();
};

void NextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests::
readsCompactFieldsInOneOrderedBatchAndKeepsMissingRowDefault()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int firstId = createClass(services, QStringLiteral("First batch class"));
    const int secondId = createClass(services, QStringLiteral("Second batch class"));
    const int missingInfoId = createClass(
        services,
        QStringLiteral("Missing information batch class")
        );
    QVERIFY(firstId > 0);
    QVERIFY(secondId > 0);
    QVERIFY(missingInfoId > 0);

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Batch Teacher");
    teacher.roomNumber = QStringLiteral("Full profile remains separate");
    const auto createdTeacher = services.databaseSession()
        ->teacherRepository()->createTeacher(teacher);
    QVERIFY(createdTeacher);

    ClassInfo firstInfo;
    firstInfo.classId = firstId;
    firstInfo.teacherId = *createdTeacher;
    firstInfo.classGrade = QStringLiteral("E4");
    firstInfo.classLevel = QStringLiteral("Theseus");
    firstInfo.classTimes = {
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
    firstInfo.intensiveTimes = {
        {
            .day = QStringLiteral("Saturday"),
            .startTime = QStringLiteral("08:00 AM"),
            .endTime = QStringLiteral("08:45 AM")
        },
        {
            .day = QStringLiteral("Friday"),
            .startTime = QStringLiteral("02:00 PM"),
            .endTime = QStringLiteral("04:00 PM")
        }
    };
    firstInfo.notes = QStringLiteral("Class notes \U0001F9ED");
    firstInfo.timeFillerActivities = QStringLiteral("Filler \U0001F4DA");
    QVERIFY(services.databaseSession()->classInfoRepository()
        ->saveClassInfo(firstInfo));

    ClassInfo secondInfo;
    secondInfo.classId = secondId;
    secondInfo.classGrade = QStringLiteral("E5");
    secondInfo.classLevel = QStringLiteral("Artemis");
    secondInfo.classTimes = {
        {
            .day = QStringLiteral("Tuesday"),
            .startTime = QStringLiteral("10:00 AM"),
            .endTime = QStringLiteral("10:50 AM")
        }
    };
    QVERIFY(services.databaseSession()->classInfoRepository()
        ->saveClassInfo(secondInfo));

    QSqlQuery deleteMissingInformation(
        services.databaseSession()->database());
    deleteMissingInformation.prepare(QStringLiteral(
        "DELETE FROM class_info WHERE class_id=?"));
    deleteMissingInformation.addBindValue(missingInfoId);
    QVERIFY2(deleteMissingInformation.exec(),
             qPrintable(deleteMissingInformation.lastError().text()));

    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    ClassInfoRepository* const repository = session->classInfoRepository();
    QVERIFY(repository);
    const MyClassesClassInformationBatchReadMetrics before =
        repository->myClassesClassInformationBatchReadMetrics();
    const int singleReadsBefore = repository
        ->scheduleClassInfoReadMetrics().singleClassInfoReadCount;

    Platform::ApplicationServicesMyClassesClassInformationBatchReadPort port(
        services
        );
    const Application::MyClassesClassInformationBatchReadQuery query(port);
    const std::vector<Domain::ClassId> requested{
        classId(secondId), classId(missingInfoId), classId(firstId)
    };
    const auto loaded = query.execute(requested);

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), requested.size());
    for (std::size_t index = 0; index < requested.size(); ++index)
    {
        QVERIFY(loaded.value()[index].classId == requested[index]);
        QVERIFY(loaded.value()[index].information);
    }
    const auto& secondFields = loaded.value()[0].information.value();
    QVERIFY(secondFields.classGrade == u"E5");
    QVERIFY(secondFields.classLevel == u"Artemis");
    QCOMPARE(secondFields.regularSchedule.size(), std::size_t(1));
    QVERIFY(secondFields.teacherId == std::nullopt);

    const auto& missingFields = loaded.value()[1].information.value();
    QVERIFY(missingFields.classGrade.empty());
    QVERIFY(missingFields.classLevel.empty());
    QVERIFY(missingFields.regularSchedule.empty());
    QVERIFY(missingFields.intensiveSchedule.empty());
    QVERIFY(missingFields.notes.empty());
    QVERIFY(missingFields.timeFillerActivities.empty());
    QVERIFY(missingFields.teacherId == std::nullopt);

    const auto& firstFields = loaded.value()[2].information.value();
    QVERIFY(firstFields.classGrade == u"E4");
    QVERIFY(firstFields.classLevel == u"Theseus");
    QCOMPARE(firstFields.regularSchedule.size(), std::size_t(2));
    QVERIFY(firstFields.regularSchedule[0].day == u"Wednesday");
    QVERIFY(firstFields.regularSchedule[1].day == u"Monday");
    QCOMPARE(firstFields.intensiveSchedule.size(), std::size_t(2));
    QVERIFY(firstFields.intensiveSchedule[0].day == u"Saturday");
    QVERIFY(firstFields.intensiveSchedule[1].day == u"Friday");
    QVERIFY(firstFields.notes == u"Class notes \U0001F9ED");
    QVERIFY(firstFields.timeFillerActivities == u"Filler \U0001F4DA");
    QVERIFY(firstFields.teacherId == Domain::TeacherId::fromString(
        std::to_string(*createdTeacher)));

    const MyClassesClassInformationBatchReadMetrics after =
        repository->myClassesClassInformationBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 3);
    QCOMPARE(after.metadataStatementCount, before.metadataStatementCount + 1);
    QCOMPARE(after.regularScheduleStatementCount,
             before.regularScheduleStatementCount + 1);
    QCOMPARE(after.intensiveScheduleStatementCount,
             before.intensiveScheduleStatementCount + 1);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount);
    QCOMPARE(repository->scheduleClassInfoReadMetrics()
        .singleClassInfoReadCount, singleReadsBefore);
}

void NextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests::
keepsPerClassRepositoryFailuresAlongsideSuccessfulSiblings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int classIdValue = createClass(
        services,
        QStringLiteral("Valid batch class")
        );
    QVERIFY(classIdValue > 0);
    ClassInfo info;
    info.classId = classIdValue;
    info.classGrade = QStringLiteral("E4");
    info.classLevel = QStringLiteral("Theseus");
    QVERIFY(services.databaseSession()->classInfoRepository()
        ->saveClassInfo(info));

    ClassInfoRepository* const repository = services.databaseSession()
        ->classInfoRepository();
    QVERIFY(repository);
    const auto loaded = repository->loadMyClassesClassInformationRecords({
        classIdValue,
        0
    });

    QVERIFY(loaded);
    QCOMPARE(loaded->size(), std::size_t(2));
    QCOMPARE(loaded->at(0).classId, classIdValue);
    QVERIFY(loaded->at(0).information);
    QCOMPARE(loaded->at(0).information->classGrade, QStringLiteral("E4"));
    QCOMPARE(loaded->at(1).classId, 0);
    QVERIFY(!loaded->at(1).information);
    QVERIFY(!loaded->at(1).information.error().trimmed().isEmpty());
}

void NextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests::
batchSqlFailureReturnsIndependentEntriesForEveryClass()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int firstId = createClass(services, QStringLiteral("First failure class"));
    const int secondId = createClass(services, QStringLiteral("Second failure class"));
    QVERIFY(firstId > 0);
    QVERIFY(secondId > 0);

    QSqlQuery dropSchedule(
        services.databaseSession()->database());
    QVERIFY2(dropSchedule.exec(QStringLiteral("DROP TABLE class_intensive_times")),
             qPrintable(dropSchedule.lastError().text()));

    Platform::ApplicationServicesMyClassesClassInformationBatchReadPort port(
        services
        );
    const auto loaded = port.readMyClassesClassInformationBatch({
        classId(firstId),
        classId(secondId)
    });

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), std::size_t(2));
    for (std::size_t index = 0; index < loaded.value().size(); ++index)
    {
        QVERIFY(loaded.value()[index].classId == classId(
            index == 0 ? firstId : secondId));
        QVERIFY(!loaded.value()[index].information);
        QVERIFY(!loaded.value()[index].information.error().message.empty());
    }
}

void NextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests::
emptyInputAvoidsRepositoryWork()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    ClassInfoRepository* const repository = services.databaseSession()
        ->classInfoRepository();
    QVERIFY(repository);
    const MyClassesClassInformationBatchReadMetrics before =
        repository->myClassesClassInformationBatchReadMetrics();

    Platform::ApplicationServicesMyClassesClassInformationBatchReadPort port(
        services
        );
    const auto loaded =
        port.readMyClassesClassInformationBatch({});

    QVERIFY(loaded);
    QVERIFY(loaded.value().empty());
    const MyClassesClassInformationBatchReadMetrics after =
        repository->myClassesClassInformationBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount);
    QCOMPARE(after.metadataStatementCount, before.metadataStatementCount);
    QCOMPARE(after.regularScheduleStatementCount,
             before.regularScheduleStatementCount);
    QCOMPARE(after.intensiveScheduleStatementCount,
             before.intensiveScheduleStatementCount);
}

void NextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests::
reportsUnavailableServices()
{
    Platform::ApplicationServicesMyClassesClassInformationBatchReadPort port(
        nullptr
        );
    const auto loaded = port.readMyClassesClassInformationBatch({classId(7)});

    QVERIFY(!loaded);
    QCOMPARE(loaded.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(!loaded.error().message.empty());
}

QTEST_MAIN(
    NextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests
    )

#include "next_platform_application_services_my_classes_class_information_batch_read_port_tests.moc"
