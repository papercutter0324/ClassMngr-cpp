#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "domain/models/class_info.h"
#include "next/platform/application_services_my_classes_class_summary_batch_read_port.h"

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
        QStringLiteral("my-classes-summary-port-%1.tps").arg(
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
    const auto created = services.workspaceStore()->classRepository()
        ->createClass(name);
    return created ? *created : -1;
}

}

class NextPlatformApplicationServicesMyClassesClassSummaryBatchReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsOrderedClassSummaries();
    void mapsMissingParentsAndRepositoryFailuresPerEntry();
};

void NextPlatformApplicationServicesMyClassesClassSummaryBatchReadPortTests::
readsOrderedClassSummaries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int firstId = createClass(services, QStringLiteral("First"));
    const int secondId = createClass(services, QStringLiteral("Second"));
    QVERIFY(firstId > 0);
    QVERIFY(secondId > 0);

    ClassInfo firstInfo;
    firstInfo.classId = firstId;
    firstInfo.classGrade = QStringLiteral("E4");
    firstInfo.classLevel = QStringLiteral("Theseus");
    firstInfo.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("9:00 AM"), QStringLiteral("9:45 AM")}
    };
    QVERIFY(services.workspaceStore()->classInfoRepository()
        ->saveClassInfo(firstInfo));

    ClassInfo secondInfo;
    secondInfo.classId = secondId;
    secondInfo.classGrade = QStringLiteral("E5");
    secondInfo.classLevel = QStringLiteral("Artemis");
    QVERIFY(services.workspaceStore()->classInfoRepository()
        ->saveClassInfo(secondInfo));

    Platform::ApplicationServicesMyClassesClassSummaryBatchReadPort port(
        &services
        );
    const auto result = port.readMyClassesClassSummaries({
        classId(secondId),
        classId(firstId)
    });

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t{2});
    QVERIFY(result.value().at(0).classId == classId(secondId));
    QVERIFY(result.value().at(0).summary);
    QVERIFY(result.value().at(0).summary.value().classGrade == u"E5");
    QVERIFY(result.value().at(0).summary.value().classLevel == u"Artemis");
    QVERIFY(result.value().at(1).classId == classId(firstId));
    QVERIFY(result.value().at(1).summary);
    QVERIFY(result.value().at(1).summary.value().classGrade == u"E4");
    QCOMPARE(
        result.value().at(1).summary.value().regularSchedule.size(),
        std::size_t{1}
        );
}

void NextPlatformApplicationServicesMyClassesClassSummaryBatchReadPortTests::
mapsMissingParentsAndRepositoryFailuresPerEntry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int deletedId = createClass(services, QStringLiteral("Deleted"));
    const int survivingId = createClass(services, QStringLiteral("Surviving"));
    QVERIFY(deletedId > 0);
    QVERIFY(survivingId > 0);
    QVERIFY(services.workspaceStore()->classInfoRepository()
        ->saveClassInfo(ClassInfo{.classId = survivingId}));

    QSqlQuery deleteParent(services.workspaceStore()->database());
    QVERIFY(deleteParent.exec(QStringLiteral("PRAGMA foreign_keys = OFF")));
    deleteParent.prepare(QStringLiteral("DELETE FROM classes WHERE id=?"));
    deleteParent.addBindValue(deletedId);
    QVERIFY(deleteParent.exec());
    QCOMPARE(deleteParent.numRowsAffected(), 1);
    QVERIFY(deleteParent.exec(QStringLiteral("PRAGMA foreign_keys = ON")));

    Platform::ApplicationServicesMyClassesClassSummaryBatchReadPort port(
        &services
        );
    const auto missing = port.readMyClassesClassSummaries({classId(deletedId)});
    QVERIFY(missing);
    QCOMPARE(missing.value().size(), std::size_t{1});
    QVERIFY(!missing.value().front().summary);
    QCOMPARE(
        missing.value().front().summary.error().code,
        Domain::ErrorCode::NotFound
        );
    QVERIFY(missing.value().front().summary.error().recoverable);

    QSqlQuery dropSchedules(services.workspaceStore()->database());
    QVERIFY(dropSchedules.exec(QStringLiteral("DROP TABLE class_times")));
    const auto failed = port.readMyClassesClassSummaries({classId(survivingId)});
    QVERIFY(failed);
    QCOMPARE(failed.value().size(), std::size_t{1});
    QVERIFY(!failed.value().front().summary);
    QCOMPARE(
        failed.value().front().summary.error().code,
        Domain::ErrorCode::Technical
        );
    QVERIFY(failed.value().front().summary.error().recoverable);
    QVERIFY(!failed.value().front().summary.error().message.empty());
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesMyClassesClassSummaryBatchReadPortTests
    )

#include "next_platform_application_services_my_classes_class_summary_batch_read_port_tests.moc"
