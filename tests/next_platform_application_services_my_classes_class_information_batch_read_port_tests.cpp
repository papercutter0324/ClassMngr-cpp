#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/class_info.h"
#include "next/application/my_classes_class_information_batch_read_query.h"
#include "next/application/my_classes_class_summary_batch_read_query.h"
#include "next/platform/application_services_my_classes_class_information_batch_read_port.h"
#include "next/platform/application_services_my_classes_class_summary_batch_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <utility>
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
    void distinguishesMissingParentsAndSuppressesOrphanChildren();
    void readsWideTeacherIdsAndKeepsSiblingsInBatch();
    void keepsPerClassRepositoryFailuresAlongsideSuccessfulSiblings();
    void batchSqlFailureReturnsIndependentEntriesForEveryClass();
    void classSummaryBatchDistinguishesMissingParentsAndKeepsPresentSchedules();
    void classSummaryScheduleFailureUsesParentAwareFallback();
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
distinguishesMissingParentsAndSuppressesOrphanChildren()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int deletedId = createClass(
        services,
        QStringLiteral("Deleted class with orphan information")
        );
    const int populatedId = createClass(
        services,
        QStringLiteral("Populated sibling class")
        );
    const int noInformationId = createClass(
        services,
        QStringLiteral("Existing class without information")
        );
    QVERIFY(deletedId > 0);
    QVERIFY(populatedId > 0);
    QVERIFY(noInformationId > 0);

    ClassInfoRepository* const repository = services.databaseSession()
        ->classInfoRepository();
    QVERIFY(repository);

    ClassInfo deletedInfo;
    deletedInfo.classId = deletedId;
    deletedInfo.classGrade = QStringLiteral("Orphan grade");
    deletedInfo.classLevel = QStringLiteral("Orphan level");
    deletedInfo.notes = QStringLiteral("Orphan notes");
    deletedInfo.timeFillerActivities = QStringLiteral("Orphan filler");
    deletedInfo.classTimes = {
        {
            .day = QStringLiteral("Thursday"),
            .startTime = QStringLiteral("06:00 PM"),
            .endTime = QStringLiteral("06:50 PM")
        }
    };
    deletedInfo.intensiveTimes = {
        {
            .day = QStringLiteral("Saturday"),
            .startTime = QStringLiteral("09:00 AM"),
            .endTime = QStringLiteral("09:45 AM")
        }
    };
    QVERIFY(repository->saveClassInfo(deletedInfo));

    ClassInfo populatedInfo;
    populatedInfo.classId = populatedId;
    populatedInfo.classGrade = QStringLiteral("E5");
    populatedInfo.classLevel = QStringLiteral("Healthy level");
    populatedInfo.notes = QStringLiteral("Healthy notes");
    populatedInfo.timeFillerActivities = QStringLiteral("Healthy filler");
    populatedInfo.classTimes = {
        {
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("03:00 PM"),
            .endTime = QStringLiteral("03:50 PM")
        }
    };
    populatedInfo.intensiveTimes = {
        {
            .day = QStringLiteral("Sunday"),
            .startTime = QStringLiteral("10:00 AM"),
            .endTime = QStringLiteral("10:45 AM")
        }
    };
    QVERIFY(repository->saveClassInfo(populatedInfo));

    ClassInfo noInformationSchedule;
    noInformationSchedule.classId = noInformationId;
    noInformationSchedule.classTimes = {
        {
            .day = QStringLiteral("Tuesday"),
            .startTime = QStringLiteral("11:00 AM"),
            .endTime = QStringLiteral("11:50 AM")
        }
    };
    noInformationSchedule.intensiveTimes = {
        {
            .day = QStringLiteral("Friday"),
            .startTime = QStringLiteral("01:00 PM"),
            .endTime = QStringLiteral("01:45 PM")
        }
    };
    QVERIFY(repository->saveClassInfo(noInformationSchedule));

    QSqlQuery deleteOptionalMetadata(services.databaseSession()->database());
    deleteOptionalMetadata.prepare(QStringLiteral(
        "DELETE FROM class_info WHERE class_id=?"));
    deleteOptionalMetadata.addBindValue(noInformationId);
    QVERIFY2(deleteOptionalMetadata.exec(),
             qPrintable(deleteOptionalMetadata.lastError().text()));
    QCOMPARE(deleteOptionalMetadata.numRowsAffected(), 1);
    deleteOptionalMetadata.finish();

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY2(disableForeignKeys.exec(QStringLiteral(
                 "PRAGMA foreign_keys = OFF")),
             qPrintable(disableForeignKeys.lastError().text()));
    disableForeignKeys.finish();
    QSqlQuery deleteParent(services.databaseSession()->database());
    deleteParent.prepare(QStringLiteral("DELETE FROM classes WHERE id=?"));
    deleteParent.addBindValue(deletedId);
    QVERIFY2(deleteParent.exec(), qPrintable(deleteParent.lastError().text()));
    QCOMPARE(deleteParent.numRowsAffected(), 1);
    deleteParent.finish();

    QSqlQuery verifyOrphanChildren(services.databaseSession()->database());
    verifyOrphanChildren.prepare(QStringLiteral(
        "SELECT "
        "(SELECT COUNT(*) FROM class_info WHERE class_id=?), "
        "(SELECT COUNT(*) FROM class_times WHERE class_id=?), "
        "(SELECT COUNT(*) FROM class_intensive_times WHERE class_id=?)"));
    verifyOrphanChildren.addBindValue(deletedId);
    verifyOrphanChildren.addBindValue(deletedId);
    verifyOrphanChildren.addBindValue(deletedId);
    QVERIFY2(verifyOrphanChildren.exec(),
             qPrintable(verifyOrphanChildren.lastError().text()));
    QVERIFY(verifyOrphanChildren.next());
    QCOMPARE(verifyOrphanChildren.value(0).toInt(), 1);
    QCOMPARE(verifyOrphanChildren.value(1).toInt(), 1);
    QCOMPARE(verifyOrphanChildren.value(2).toInt(), 1);
    QVERIFY(!verifyOrphanChildren.next());
    verifyOrphanChildren.finish();

    QSqlQuery enableForeignKeys(services.databaseSession()->database());
    QVERIFY2(enableForeignKeys.exec(QStringLiteral(
                 "PRAGMA foreign_keys = ON")),
             qPrintable(enableForeignKeys.lastError().text()));
    enableForeignKeys.finish();

    const auto repositoryRead = repository->loadMyClassesClassInformationRecords({
        deletedId,
        populatedId,
        noInformationId,
        987654321
    });
    QVERIFY(repositoryRead);
    QCOMPARE(repositoryRead->size(), std::size_t(4));
    QCOMPARE(repositoryRead->at(0).classId, deletedId);
    QVERIFY(repositoryRead->at(0).parentMissing);
    QVERIFY(repositoryRead->at(0).information);
    QVERIFY(repositoryRead->at(0).information->classGrade.isEmpty());
    QVERIFY(repositoryRead->at(0).information->classLevel.isEmpty());
    QVERIFY(repositoryRead->at(0).information->notes.isEmpty());
    QVERIFY(repositoryRead->at(0).information->timeFillerActivities.isEmpty());
    QVERIFY(repositoryRead->at(0).information->regularTimes.isEmpty());
    QVERIFY(repositoryRead->at(0).information->intensiveTimes.isEmpty());
    QCOMPARE(repositoryRead->at(1).classId, populatedId);
    QVERIFY(!repositoryRead->at(1).parentMissing);
    QVERIFY(repositoryRead->at(1).information);
    QCOMPARE(repositoryRead->at(1).information->classGrade,
             QStringLiteral("E5"));
    QCOMPARE(repositoryRead->at(1).information->regularTimes.size(), 1);
    QCOMPARE(repositoryRead->at(1).information->intensiveTimes.size(), 1);
    QCOMPARE(repositoryRead->at(2).classId, noInformationId);
    QVERIFY(!repositoryRead->at(2).parentMissing);
    QVERIFY(repositoryRead->at(2).information);
    QVERIFY(repositoryRead->at(2).information->classGrade.isEmpty());
    QCOMPARE(repositoryRead->at(2).information->regularTimes.size(), 1);
    QCOMPARE(repositoryRead->at(2).information->intensiveTimes.size(), 1);
    QCOMPARE(repositoryRead->at(3).classId, 987654321);
    QVERIFY(repositoryRead->at(3).parentMissing);

    const auto before = repository->myClassesClassInformationBatchReadMetrics();
    const int singleReadsBefore = repository->scheduleClassInfoReadMetrics()
        .singleClassInfoReadCount;
    Platform::ApplicationServicesMyClassesClassInformationBatchReadPort port(
        services
        );
    const Application::MyClassesClassInformationBatchReadQuery query(port);
    const std::vector<Domain::ClassId> requested{
        classId(deletedId),
        classId(populatedId),
        classId(noInformationId),
        classId(987654321)
    };
    const auto loaded = query.execute(requested);

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), requested.size());
    for (std::size_t index = 0; index < requested.size(); ++index)
    {
        QVERIFY(loaded.value()[index].classId == requested[index]);
    }

    const auto& deleted = loaded.value()[0].information;
    QVERIFY(!deleted);
    QCOMPARE(deleted.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(deleted.error().recoverable);

    const auto& populated = loaded.value()[1].information;
    QVERIFY(populated);
    QVERIFY(populated.value().classGrade == u"E5");
    QVERIFY(populated.value().classLevel == u"Healthy level");
    QVERIFY(populated.value().notes == u"Healthy notes");
    QVERIFY(populated.value().timeFillerActivities == u"Healthy filler");
    QCOMPARE(populated.value().regularSchedule.size(), std::size_t(1));
    QVERIFY(populated.value().regularSchedule[0].day == u"Monday");
    QCOMPARE(populated.value().intensiveSchedule.size(), std::size_t(1));
    QVERIFY(populated.value().intensiveSchedule[0].day == u"Sunday");

    const auto& noInformation = loaded.value()[2].information;
    QVERIFY(noInformation);
    QVERIFY(noInformation.value().classGrade.empty());
    QVERIFY(noInformation.value().classLevel.empty());
    QVERIFY(noInformation.value().notes.empty());
    QVERIFY(noInformation.value().timeFillerActivities.empty());
    QCOMPARE(noInformation.value().regularSchedule.size(), std::size_t(1));
    QVERIFY(noInformation.value().regularSchedule[0].day == u"Tuesday");
    QCOMPARE(noInformation.value().intensiveSchedule.size(), std::size_t(1));
    QVERIFY(noInformation.value().intensiveSchedule[0].day == u"Friday");

    const auto& neverPresent = loaded.value()[3].information;
    QVERIFY(!neverPresent);
    QCOMPARE(neverPresent.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(neverPresent.error().recoverable);

    const auto after = repository->myClassesClassInformationBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 4);
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
readsWideTeacherIdsAndKeepsSiblingsInBatch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int wideId = createClass(services, QStringLiteral("Wide teacher ID class"));
    const int healthyId = createClass(services, QStringLiteral("Healthy sibling class"));
    const int unassignedId = createClass(services, QStringLiteral("Unassigned sibling class"));
    QVERIFY(wideId > 0);
    QVERIFY(healthyId > 0);
    QVERIFY(unassignedId > 0);

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Ordinary sibling teacher");
    const auto createdTeacher = services.databaseSession()
        ->teacherRepository()->createTeacher(teacher);
    QVERIFY(createdTeacher);

    auto* const repository = services.databaseSession()->classInfoRepository();
    QVERIFY(repository);
    for (const auto [classIdValue, grade] : {
             std::pair{wideId, QStringLiteral("Wide")},
             std::pair{healthyId, QStringLiteral("Healthy")},
             std::pair{unassignedId, QStringLiteral("Unassigned")}
         })
    {
        ClassInfo info;
        info.classId = classIdValue;
        info.classGrade = grade;
        info.classLevel = QStringLiteral("Batch regression");
        QVERIFY(repository->saveClassInfo(info));
    }

    QSqlQuery insertWideTeacher(services.databaseSession()->database());
    QVERIFY2(insertWideTeacher.exec(QStringLiteral(
                 "INSERT INTO teachers (id) VALUES (2147483648)"
                 )),
             qPrintable(insertWideTeacher.lastError().text()));

    QSqlQuery storeWideTeacherId(services.databaseSession()->database());
    QVERIFY2(storeWideTeacherId.exec(QStringLiteral(
                 "UPDATE class_info SET teacher_id=2147483648 WHERE class_id=%1"
                 ).arg(wideId)),
             qPrintable(storeWideTeacherId.lastError().text()));
    QCOMPARE(storeWideTeacherId.numRowsAffected(), 1);

    QSqlQuery storeHealthyTeacherId(services.databaseSession()->database());
    storeHealthyTeacherId.prepare(QStringLiteral(
        "UPDATE class_info SET teacher_id=? WHERE class_id=?"));
    storeHealthyTeacherId.addBindValue(*createdTeacher);
    storeHealthyTeacherId.addBindValue(healthyId);
    QVERIFY2(storeHealthyTeacherId.exec(),
             qPrintable(storeHealthyTeacherId.lastError().text()));
    QCOMPARE(storeHealthyTeacherId.numRowsAffected(), 1);

    QSqlQuery storeUnassignedTeacherId(services.databaseSession()->database());
    QVERIFY2(storeUnassignedTeacherId.exec(QStringLiteral(
                 "UPDATE class_info SET teacher_id=NULL WHERE class_id=%1"
                 ).arg(unassignedId)),
             qPrintable(storeUnassignedTeacherId.lastError().text()));
    QCOMPARE(storeUnassignedTeacherId.numRowsAffected(), 1);

    QSqlQuery verifyWideTeacher(services.databaseSession()->database());
    QVERIFY2(verifyWideTeacher.exec(QStringLiteral(
                 "SELECT typeof(id), id FROM teachers WHERE id=2147483648"
                 )),
             qPrintable(verifyWideTeacher.lastError().text()));
    QVERIFY(verifyWideTeacher.next());
    QCOMPARE(verifyWideTeacher.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyWideTeacher.value(1).toLongLong(), qint64(2147483648LL));
    QCOMPARE(verifyWideTeacher.value(1).toString(), QStringLiteral("2147483648"));
    QVERIFY(!verifyWideTeacher.next());

    QSqlQuery verifyStoredTeacherId(services.databaseSession()->database());
    verifyStoredTeacherId.prepare(QStringLiteral(
        "SELECT typeof(teacher_id), teacher_id FROM class_info WHERE class_id=?"));
    verifyStoredTeacherId.addBindValue(wideId);
    QVERIFY2(verifyStoredTeacherId.exec(),
             qPrintable(verifyStoredTeacherId.lastError().text()));
    QVERIFY(verifyStoredTeacherId.next());
    QCOMPARE(verifyStoredTeacherId.value(0).toString(), QStringLiteral("integer"));
    QCOMPARE(verifyStoredTeacherId.value(1).toLongLong(), qint64(2147483648LL));
    QCOMPARE(verifyStoredTeacherId.value(1).toString(), QStringLiteral("2147483648"));
    QVERIFY(!verifyStoredTeacherId.next());

    QSqlQuery verifyUnassignedId(services.databaseSession()->database());
    verifyUnassignedId.prepare(QStringLiteral(
        "SELECT typeof(teacher_id) FROM class_info WHERE class_id=?"));
    verifyUnassignedId.addBindValue(unassignedId);
    QVERIFY2(verifyUnassignedId.exec(),
             qPrintable(verifyUnassignedId.lastError().text()));
    QVERIFY(verifyUnassignedId.next());
    QCOMPARE(verifyUnassignedId.value(0).toString(), QStringLiteral("null"));
    QVERIFY(!verifyUnassignedId.next());

    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    const auto before = repository->myClassesClassInformationBatchReadMetrics();
    const int singleReadsBefore = repository->scheduleClassInfoReadMetrics()
        .singleClassInfoReadCount;

    Platform::ApplicationServicesMyClassesClassInformationBatchReadPort port(
        services
        );
    const Application::MyClassesClassInformationBatchReadQuery query(port);
    const auto loaded = query.execute({
        classId(wideId),
        classId(unassignedId),
        classId(healthyId)
    });

    const auto after = repository->myClassesClassInformationBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 3);
    QCOMPARE(after.metadataStatementCount, before.metadataStatementCount + 1);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount);
    QCOMPARE(repository->scheduleClassInfoReadMetrics()
        .singleClassInfoReadCount, singleReadsBefore);

    if (!loaded)
    {
        QFAIL(qPrintable(QString::fromStdString(loaded.error().message)));
    }
    QCOMPARE(loaded.value().size(), std::size_t(3));
    QVERIFY(loaded.value()[0].classId == classId(wideId));
    QVERIFY(loaded.value()[0].information);
    QVERIFY(loaded.value()[0].information.value().classGrade == u"Wide");
    QVERIFY(loaded.value()[0].information.value().teacherId
        == Domain::TeacherId::fromString("2147483648"));

    QVERIFY(loaded.value()[1].classId == classId(unassignedId));
    QVERIFY(loaded.value()[1].information);
    QVERIFY(loaded.value()[1].information.value().classGrade == u"Unassigned");
    QVERIFY(loaded.value()[1].information.value().teacherId == std::nullopt);

    QVERIFY(loaded.value()[2].classId == classId(healthyId));
    QVERIFY(loaded.value()[2].information);
    QVERIFY(loaded.value()[2].information.value().classGrade == u"Healthy");
    QVERIFY(loaded.value()[2].information.value().teacherId
        == Domain::TeacherId::fromString(std::to_string(*createdTeacher)));
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
    const int deletedId = createClass(
        services,
        QStringLiteral("Deleted fallback class with orphan children")
        );
    QVERIFY(firstId > 0);
    QVERIFY(secondId > 0);
    QVERIFY(deletedId > 0);

    ClassInfoRepository* const repository = services.databaseSession()
        ->classInfoRepository();
    QVERIFY(repository);

    ClassInfo deletedInfo;
    deletedInfo.classId = deletedId;
    deletedInfo.classGrade = QStringLiteral("Fallback orphan grade");
    deletedInfo.classLevel = QStringLiteral("Fallback orphan level");
    deletedInfo.notes = QStringLiteral("Fallback orphan notes");
    deletedInfo.timeFillerActivities = QStringLiteral("Fallback orphan filler");
    deletedInfo.classTimes = {
        {
            .day = QStringLiteral("Thursday"),
            .startTime = QStringLiteral("06:00 PM"),
            .endTime = QStringLiteral("06:50 PM")
        }
    };
    deletedInfo.intensiveTimes = {
        {
            .day = QStringLiteral("Saturday"),
            .startTime = QStringLiteral("09:00 AM"),
            .endTime = QStringLiteral("09:45 AM")
        }
    };
    QVERIFY(repository->saveClassInfo(deletedInfo));

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY2(disableForeignKeys.exec(QStringLiteral(
                 "PRAGMA foreign_keys = OFF")),
             qPrintable(disableForeignKeys.lastError().text()));
    disableForeignKeys.finish();
    QSqlQuery deleteParent(services.databaseSession()->database());
    deleteParent.prepare(QStringLiteral("DELETE FROM classes WHERE id=?"));
    deleteParent.addBindValue(deletedId);
    QVERIFY2(deleteParent.exec(), qPrintable(deleteParent.lastError().text()));
    QCOMPARE(deleteParent.numRowsAffected(), 1);
    deleteParent.finish();

    QSqlQuery verifyOrphanChildren(services.databaseSession()->database());
    verifyOrphanChildren.prepare(QStringLiteral(
        "SELECT "
        "(SELECT COUNT(*) FROM class_info WHERE class_id=?), "
        "(SELECT COUNT(*) FROM class_times WHERE class_id=?), "
        "(SELECT COUNT(*) FROM class_intensive_times WHERE class_id=?)"));
    verifyOrphanChildren.addBindValue(deletedId);
    verifyOrphanChildren.addBindValue(deletedId);
    verifyOrphanChildren.addBindValue(deletedId);
    QVERIFY2(verifyOrphanChildren.exec(),
             qPrintable(verifyOrphanChildren.lastError().text()));
    QVERIFY(verifyOrphanChildren.next());
    QCOMPARE(verifyOrphanChildren.value(0).toInt(), 1);
    QCOMPARE(verifyOrphanChildren.value(1).toInt(), 1);
    QCOMPARE(verifyOrphanChildren.value(2).toInt(), 1);
    QVERIFY(!verifyOrphanChildren.next());
    verifyOrphanChildren.finish();

    QSqlQuery enableForeignKeys(services.databaseSession()->database());
    QVERIFY2(enableForeignKeys.exec(QStringLiteral(
                 "PRAGMA foreign_keys = ON")),
             qPrintable(enableForeignKeys.lastError().text()));
    enableForeignKeys.finish();

    QSqlQuery dropSchedule(
        services.databaseSession()->database());
    QVERIFY2(dropSchedule.exec(QStringLiteral("DROP TABLE class_intensive_times")),
             qPrintable(dropSchedule.lastError().text()));
    dropSchedule.finish();

    const auto before = repository->myClassesClassInformationBatchReadMetrics();
    const QList<int> requested{firstId, deletedId, secondId};
    const auto repositoryRead =
        repository->loadMyClassesClassInformationRecords(requested);

    QVERIFY(repositoryRead);
    QCOMPARE(repositoryRead->size(), std::size_t(3));
    QCOMPARE(repositoryRead->at(0).classId, firstId);
    QVERIFY(!repositoryRead->at(0).information);
    QVERIFY(!repositoryRead->at(0).information.error().trimmed().isEmpty());
    QCOMPARE(repositoryRead->at(1).classId, deletedId);
    QVERIFY(repositoryRead->at(1).parentMissing);
    QVERIFY(repositoryRead->at(1).information);
    QVERIFY(repositoryRead->at(1).information->classGrade.isEmpty());
    QVERIFY(repositoryRead->at(1).information->classLevel.isEmpty());
    QVERIFY(repositoryRead->at(1).information->notes.isEmpty());
    QVERIFY(repositoryRead->at(1).information->timeFillerActivities.isEmpty());
    QVERIFY(repositoryRead->at(1).information->regularTimes.isEmpty());
    QVERIFY(repositoryRead->at(1).information->intensiveTimes.isEmpty());
    QCOMPARE(repositoryRead->at(2).classId, secondId);
    QVERIFY(!repositoryRead->at(2).information);
    QVERIFY(!repositoryRead->at(2).information.error().trimmed().isEmpty());

    Platform::ApplicationServicesMyClassesClassInformationBatchReadPort port(
        services
        );
    const auto loaded = port.readMyClassesClassInformationBatch({
        classId(firstId),
        classId(deletedId),
        classId(secondId)
    });

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), std::size_t(3));
    QVERIFY(loaded.value()[0].classId == classId(firstId));
    QVERIFY(!loaded.value()[0].information);
    QCOMPARE(loaded.value()[0].information.error().code,
             Domain::ErrorCode::Technical);
    QVERIFY(loaded.value()[0].information.error().recoverable);
    QVERIFY(!loaded.value()[0].information.error().message.empty());

    QVERIFY(loaded.value()[1].classId == classId(deletedId));
    QVERIFY(!loaded.value()[1].information);
    QCOMPARE(loaded.value()[1].information.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(loaded.value()[1].information.error().recoverable);

    QVERIFY(loaded.value()[2].classId == classId(secondId));
    QVERIFY(!loaded.value()[2].information);
    QCOMPARE(loaded.value()[2].information.error().code,
             Domain::ErrorCode::Technical);
    QVERIFY(loaded.value()[2].information.error().recoverable);
    QVERIFY(!loaded.value()[2].information.error().message.empty());

    const auto after = repository->myClassesClassInformationBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 2);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 6);
    QCOMPARE(after.metadataStatementCount, before.metadataStatementCount + 2);
    QCOMPARE(after.regularScheduleStatementCount,
             before.regularScheduleStatementCount + 2);
    QCOMPARE(after.intensiveScheduleStatementCount,
             before.intensiveScheduleStatementCount + 2);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount + 6);
}

void NextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests::
classSummaryBatchDistinguishesMissingParentsAndKeepsPresentSchedules()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int deletedId = createClass(
        services,
        QStringLiteral("Deleted class with orphan summary children")
        );
    const int populatedId = createClass(
        services,
        QStringLiteral("Populated summary sibling")
        );
    const int noInformationId = createClass(
        services,
        QStringLiteral("Summary class without optional metadata")
        );
    QVERIFY(deletedId > 0);
    QVERIFY(populatedId > 0);
    QVERIFY(noInformationId > 0);

    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Summary batch teacher");
    const auto createdTeacher = services.databaseSession()
        ->teacherRepository()->createTeacher(teacher);
    QVERIFY(createdTeacher);

    ClassInfoRepository* const repository = services.databaseSession()
        ->classInfoRepository();
    QVERIFY(repository);

    ClassInfo deletedInfo;
    deletedInfo.classId = deletedId;
    deletedInfo.teacherId = *createdTeacher;
    deletedInfo.classGrade = QStringLiteral("Orphan summary grade");
    deletedInfo.classLevel = QStringLiteral("Orphan summary level");
    deletedInfo.classTimes = {
        {
            .day = QStringLiteral("Thursday"),
            .startTime = QStringLiteral("06:00 PM"),
            .endTime = QStringLiteral("06:50 PM")
        }
    };
    deletedInfo.intensiveTimes = {
        {
            .day = QStringLiteral("Saturday"),
            .startTime = QStringLiteral("09:00 AM"),
            .endTime = QStringLiteral("09:45 AM")
        }
    };
    QVERIFY(repository->saveClassInfo(deletedInfo));

    ClassInfo populatedInfo;
    populatedInfo.classId = populatedId;
    populatedInfo.teacherId = *createdTeacher;
    populatedInfo.classGrade = QStringLiteral("E5");
    populatedInfo.classLevel = QStringLiteral("Healthy summary level");
    populatedInfo.classTimes = {
        {
            .day = QStringLiteral("Monday"),
            .startTime = QStringLiteral("03:00 PM"),
            .endTime = QStringLiteral("03:50 PM")
        }
    };
    populatedInfo.intensiveTimes = {
        {
            .day = QStringLiteral("Sunday"),
            .startTime = QStringLiteral("10:00 AM"),
            .endTime = QStringLiteral("10:45 AM")
        }
    };
    QVERIFY(repository->saveClassInfo(populatedInfo));

    ClassInfo noInformationInfo;
    noInformationInfo.classId = noInformationId;
    noInformationInfo.teacherId = *createdTeacher;
    noInformationInfo.classGrade = QStringLiteral("Deleted optional grade");
    noInformationInfo.classTimes = {
        {
            .day = QStringLiteral("Tuesday"),
            .startTime = QStringLiteral("11:00 AM"),
            .endTime = QStringLiteral("11:50 AM")
        }
    };
    noInformationInfo.intensiveTimes = {
        {
            .day = QStringLiteral("Friday"),
            .startTime = QStringLiteral("01:00 PM"),
            .endTime = QStringLiteral("01:45 PM")
        }
    };
    QVERIFY(repository->saveClassInfo(noInformationInfo));

    QSqlQuery deleteOptionalMetadata(services.databaseSession()->database());
    deleteOptionalMetadata.prepare(QStringLiteral(
        "DELETE FROM class_info WHERE class_id=?"));
    deleteOptionalMetadata.addBindValue(noInformationId);
    QVERIFY2(deleteOptionalMetadata.exec(),
             qPrintable(deleteOptionalMetadata.lastError().text()));
    QCOMPARE(deleteOptionalMetadata.numRowsAffected(), 1);
    deleteOptionalMetadata.finish();

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY2(disableForeignKeys.exec(QStringLiteral(
                 "PRAGMA foreign_keys = OFF")),
             qPrintable(disableForeignKeys.lastError().text()));
    disableForeignKeys.finish();
    QSqlQuery deleteParent(services.databaseSession()->database());
    deleteParent.prepare(QStringLiteral("DELETE FROM classes WHERE id=?"));
    deleteParent.addBindValue(deletedId);
    QVERIFY2(deleteParent.exec(), qPrintable(deleteParent.lastError().text()));
    QCOMPARE(deleteParent.numRowsAffected(), 1);
    deleteParent.finish();

    QSqlQuery verifyOrphanChildren(services.databaseSession()->database());
    verifyOrphanChildren.prepare(QStringLiteral(
        "SELECT "
        "(SELECT COUNT(*) FROM class_info WHERE class_id=?), "
        "(SELECT COUNT(*) FROM class_times WHERE class_id=?), "
        "(SELECT COUNT(*) FROM class_intensive_times WHERE class_id=?)"));
    verifyOrphanChildren.addBindValue(deletedId);
    verifyOrphanChildren.addBindValue(deletedId);
    verifyOrphanChildren.addBindValue(deletedId);
    QVERIFY2(verifyOrphanChildren.exec(),
             qPrintable(verifyOrphanChildren.lastError().text()));
    QVERIFY(verifyOrphanChildren.next());
    QCOMPARE(verifyOrphanChildren.value(0).toInt(), 1);
    QCOMPARE(verifyOrphanChildren.value(1).toInt(), 1);
    QCOMPARE(verifyOrphanChildren.value(2).toInt(), 1);
    QVERIFY(!verifyOrphanChildren.next());
    verifyOrphanChildren.finish();

    QSqlQuery enableForeignKeys(services.databaseSession()->database());
    QVERIFY2(enableForeignKeys.exec(QStringLiteral(
                 "PRAGMA foreign_keys = ON")),
             qPrintable(enableForeignKeys.lastError().text()));
    enableForeignKeys.finish();

    const auto before = repository->myClassesClassSummaryBatchReadMetrics();
    Platform::ApplicationServicesMyClassesClassSummaryBatchReadPort port(
        &services
        );
    const Application::MyClassesClassSummaryBatchReadQuery query(port);
    const std::vector<Domain::ClassId> requested{
        classId(deletedId),
        classId(populatedId),
        classId(987654321),
        classId(noInformationId)
    };
    const auto loaded = query.execute(requested);

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), requested.size());
    for (std::size_t index = 0; index < requested.size(); ++index)
    {
        QVERIFY(loaded.value()[index].classId == requested[index]);
    }

    const auto& deleted = loaded.value()[0].summary;
    QVERIFY(!deleted);
    QCOMPARE(deleted.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(deleted.error().recoverable);

    const auto& populated = loaded.value()[1].summary;
    QVERIFY(populated);
    QVERIFY(populated.value().classGrade == u"E5");
    QVERIFY(populated.value().classLevel == u"Healthy summary level");
    QVERIFY(populated.value().teacherId
        == Domain::TeacherId::fromString(std::to_string(*createdTeacher)));
    QCOMPARE(populated.value().regularSchedule.size(), std::size_t(1));
    QVERIFY(populated.value().regularSchedule[0].day == u"Monday");
    QCOMPARE(populated.value().intensiveSchedule.size(), std::size_t(1));
    QVERIFY(populated.value().intensiveSchedule[0].day == u"Sunday");

    const auto& neverPresent = loaded.value()[2].summary;
    QVERIFY(!neverPresent);
    QCOMPARE(neverPresent.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(neverPresent.error().recoverable);

    const auto& noInformation = loaded.value()[3].summary;
    QVERIFY(noInformation);
    QVERIFY(noInformation.value().classGrade.empty());
    QVERIFY(noInformation.value().classLevel.empty());
    QVERIFY(!noInformation.value().teacherId);
    QCOMPARE(noInformation.value().regularSchedule.size(), std::size_t(1));
    QVERIFY(noInformation.value().regularSchedule[0].day == u"Tuesday");
    QCOMPARE(noInformation.value().intensiveSchedule.size(), std::size_t(1));
    QVERIFY(noInformation.value().intensiveSchedule[0].day == u"Friday");

    const auto after = repository->myClassesClassSummaryBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 4);
    QCOMPARE(after.metadataStatementCount, before.metadataStatementCount + 1);
    QCOMPARE(after.scheduleStatementCount, before.scheduleStatementCount + 1);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount);
}

void NextPlatformApplicationServicesMyClassesClassInformationBatchReadPortTests::
classSummaryScheduleFailureUsesParentAwareFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int deletedId = createClass(
        services,
        QStringLiteral("Deleted summary fallback class")
        );
    const int firstId = createClass(
        services,
        QStringLiteral("First summary fallback sibling")
        );
    const int secondId = createClass(
        services,
        QStringLiteral("Second summary fallback sibling")
        );
    QVERIFY(deletedId > 0);
    QVERIFY(firstId > 0);
    QVERIFY(secondId > 0);

    ClassInfoRepository* const repository = services.databaseSession()
        ->classInfoRepository();
    QVERIFY(repository);
    int intensiveStartHour = 9;
    for (const auto [classIdValue, grade] : {
             std::pair{deletedId, QStringLiteral("Orphan fallback grade")},
             std::pair{firstId, QStringLiteral("First fallback grade")},
             std::pair{secondId, QStringLiteral("Second fallback grade")}
         })
    {
        const int scheduleHour = intensiveStartHour++;
        ClassInfo info;
        info.classId = classIdValue;
        info.classGrade = grade;
        info.classLevel = QStringLiteral("Fallback summary fixture");
        info.classTimes = {
            {
                .day = QStringLiteral("Thursday"),
                .startTime = QStringLiteral("06:00 PM"),
                .endTime = QStringLiteral("06:50 PM")
            }
        };
        info.intensiveTimes = {
            {
                .day = QStringLiteral("Saturday"),
                .startTime = QStringLiteral("%1:00 AM").arg(
                    scheduleHour,
                    2,
                    10,
                    QChar('0')),
                .endTime = QStringLiteral("%1:45 AM").arg(
                    scheduleHour,
                    2,
                    10,
                    QChar('0'))
            }
        };
        QVERIFY(repository->saveClassInfo(info));
    }

    QSqlQuery disableForeignKeys(services.databaseSession()->database());
    QVERIFY2(disableForeignKeys.exec(QStringLiteral(
                 "PRAGMA foreign_keys = OFF")),
             qPrintable(disableForeignKeys.lastError().text()));
    disableForeignKeys.finish();
    QSqlQuery deleteParent(services.databaseSession()->database());
    deleteParent.prepare(QStringLiteral("DELETE FROM classes WHERE id=?"));
    deleteParent.addBindValue(deletedId);
    QVERIFY2(deleteParent.exec(), qPrintable(deleteParent.lastError().text()));
    QCOMPARE(deleteParent.numRowsAffected(), 1);
    deleteParent.finish();
    QSqlQuery enableForeignKeys(services.databaseSession()->database());
    QVERIFY2(enableForeignKeys.exec(QStringLiteral(
                 "PRAGMA foreign_keys = ON")),
             qPrintable(enableForeignKeys.lastError().text()));
    enableForeignKeys.finish();

    QSqlQuery dropIntensiveSchedules(
        services.databaseSession()->database());
    QVERIFY2(dropIntensiveSchedules.exec(QStringLiteral(
                 "DROP TABLE class_intensive_times")),
             qPrintable(dropIntensiveSchedules.lastError().text()));
    dropIntensiveSchedules.finish();

    const auto before = repository->myClassesClassSummaryBatchReadMetrics();
    Platform::ApplicationServicesMyClassesClassSummaryBatchReadPort port(
        &services
        );
    const auto loaded = port.readMyClassesClassSummaries({
        classId(deletedId),
        classId(firstId),
        classId(secondId)
    });

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), std::size_t(3));
    QVERIFY(loaded.value()[0].classId == classId(deletedId));
    QVERIFY(!loaded.value()[0].summary);
    QCOMPARE(loaded.value()[0].summary.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(loaded.value()[0].summary.error().recoverable);

    for (std::size_t index = 1; index < loaded.value().size(); ++index)
    {
        QVERIFY(loaded.value()[index].classId == classId(
            index == 1 ? firstId : secondId));
        QVERIFY(!loaded.value()[index].summary);
        QCOMPARE(loaded.value()[index].summary.error().code,
                 Domain::ErrorCode::Technical);
        QVERIFY(loaded.value()[index].summary.error().recoverable);
        QVERIFY(!loaded.value()[index].summary.error().message.empty());
    }

    const auto after = repository->myClassesClassSummaryBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 3);
    QCOMPARE(after.metadataStatementCount, before.metadataStatementCount + 1);
    QCOMPARE(after.scheduleStatementCount, before.scheduleStatementCount + 1);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount + 3);
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
