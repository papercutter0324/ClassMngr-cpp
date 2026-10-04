#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/roster_repository.h"
#include "domain/models/roster.h"
#include "next/application/my_classes_student_count_batch_read_query.h"
#include "next/platform/application_services_my_classes_student_count_batch_read_port.h"

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
        QStringLiteral("my-classes-student-count-batch-%1.tps").arg(
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

bool saveRoster(
    ApplicationServices& services,
    const int classIdValue,
    const Roster& roster
    )
{
    RosterRepository* const repository = services.databaseSession()
        ? services.databaseSession()->rosterRepository()
        : nullptr;
    return repository && repository->saveRoster(classIdValue, roster).has_value();
}

Roster roster(
    const QStringList& columns,
    const QList<QStringList>& rows
    )
{
    Roster result;
    result.columns = columns;
    result.rows = rows;
    return result;
}

}

class NextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void readsCountsInRequestedOrderWithExactHeadersAndQtWhitespace();
    void fallsBackPerClassWhenBatchCellReadFails();
    void emptyInputAvoidsRepositoryWork();
    void unavailableSessionReturnsPerClassFailures();
};

void NextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests::
readsCountsInRequestedOrderWithExactHeadersAndQtWhitespace()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int duplicateHeadersId = createClass(
        services,
        QStringLiteral("Duplicate header class")
        );
    const int missingHeadersId = createClass(
        services,
        QStringLiteral("Missing exact header class")
        );
    const int sparseId = createClass(services, QStringLiteral("Sparse class"));
    QVERIFY(duplicateHeadersId > 0);
    QVERIFY(missingHeadersId > 0);
    QVERIFY(sparseId > 0);

    QVERIFY(saveRoster(
        services,
        duplicateHeadersId,
        roster(
            {
                QStringLiteral("English"),
                QStringLiteral("English"),
                QStringLiteral("Korean"),
                QStringLiteral("Korean"),
                QStringLiteral("Other")
            },
            {
                {
                    QStringLiteral("\u2003"),
                    QStringLiteral("ignored duplicate English"),
                    QStringLiteral("\u2003"),
                    QStringLiteral("ignored duplicate Korean"),
                    QStringLiteral("unrelated")
                },
                {
                    QStringLiteral("Alice"),
                    QStringLiteral("ignored duplicate English"),
                    QString(), QString(), QString()
                },
                {
                    QString(), QString(), QStringLiteral("\u2003"),
                    QStringLiteral("ignored duplicate Korean"),
                    QStringLiteral("unrelated")
                },
                {
                    QString(), QStringLiteral("ignored duplicate English"),
                    QStringLiteral("\uD55C"), QString(), QString()
                },
                {
                    QStringLiteral("Both"), QString(), QStringLiteral("\uAD6D"),
                    QString(), QString()
                },
                {
                    QString(), QString(), QStringLiteral(" \u2003 ")
                }
            }
            )
        ));
    QVERIFY(saveRoster(
        services,
        missingHeadersId,
        roster(
            {QStringLiteral("english"), QStringLiteral("Korean ")},
            {{QStringLiteral("ignored"), QStringLiteral("ignored")}}
            )
        ));
    Roster sparseRoster = roster(
        {
            QStringLiteral("Other"),
            QStringLiteral("English"),
            QStringLiteral("Korean")
        },
        {
            {QStringLiteral("unrelated")},
            {QStringLiteral("unrelated"), QStringLiteral("Valid")},
            {QStringLiteral("unrelated"), QString(), QStringLiteral("\u2003")},
            {QStringLiteral("unrelated"), QString(), QStringLiteral("\uD559")}
        }
        );
    while (sparseRoster.rows.size() < 26)
    {
        sparseRoster.rows.append(QStringList{});
    }
    sparseRoster.rows.append(QStringList{
        QStringLiteral("unrelated"), QString(), QStringLiteral("\uD559\uC0DD")
    });
    QVERIFY(saveRoster(services, sparseId, sparseRoster));

    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    RosterRepository* const repository = session->rosterRepository();
    QVERIFY(repository);
    const auto before = repository->myClassesStudentCountBatchReadMetrics();

    Platform::ApplicationServicesMyClassesStudentCountBatchReadPort port(
        services
        );
    const Application::MyClassesStudentCountBatchReadQuery query(port);
    const std::vector<Domain::ClassId> requested{
        classId(sparseId), classId(missingHeadersId), classId(duplicateHeadersId)
    };
    const auto loaded = query.execute(requested);

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), requested.size());
    for (std::size_t index = 0; index < requested.size(); ++index)
    {
        QVERIFY(loaded.value()[index].classId == requested[index]);
        QVERIFY(loaded.value()[index].studentCount);
    }
    QCOMPARE(loaded.value()[0].studentCount.value(), 3);
    QCOMPARE(loaded.value()[1].studentCount.value(), 0);
    QCOMPARE(loaded.value()[2].studentCount.value(), 3);

    const auto after = repository->myClassesStudentCountBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 3);
    QCOMPARE(after.columnStatementCount, before.columnStatementCount + 1);
    QCOMPARE(after.dataStatementCount, before.dataStatementCount + 1);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount);
}

void NextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests::
fallsBackPerClassWhenBatchCellReadFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int firstId = createClass(services, QStringLiteral("First failed class"));
    const int secondId = createClass(services, QStringLiteral("Second failed class"));
    QVERIFY(firstId > 0);
    QVERIFY(secondId > 0);
    QVERIFY(saveRoster(
        services,
        firstId,
        roster({QStringLiteral("English")}, {{QStringLiteral("Student")}})
        ));
    QVERIFY(saveRoster(
        services,
        secondId,
        roster({QStringLiteral("Korean")}, {{QStringLiteral("\uD559\uC0DD")}})
        ));

    RosterRepository* const repository = services.databaseSession()
        ->rosterRepository();
    QVERIFY(repository);
    const auto before = repository->myClassesStudentCountBatchReadMetrics();
    QSqlQuery dropData(services.databaseSession()->database());
    QVERIFY2(dropData.exec(QStringLiteral("DROP TABLE roster_data")),
             qPrintable(dropData.lastError().text()));

    Platform::ApplicationServicesMyClassesStudentCountBatchReadPort port(
        services
        );
    const auto loaded = port.readMyClassesStudentCounts({
        classId(firstId), classId(secondId)
    });

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), std::size_t(2));
    for (std::size_t index = 0; index < loaded.value().size(); ++index)
    {
        QVERIFY(loaded.value()[index].classId == classId(
            index == 0 ? firstId : secondId));
        QVERIFY(!loaded.value()[index].studentCount);
        QVERIFY(!loaded.value()[index].studentCount.error().message.empty());
    }
    const auto after = repository->myClassesStudentCountBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 2);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount + 2);
}

void NextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests::
emptyInputAvoidsRepositoryWork()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    RosterRepository* const repository = services.databaseSession()
        ->rosterRepository();
    QVERIFY(repository);
    const auto before = repository->myClassesStudentCountBatchReadMetrics();

    Platform::ApplicationServicesMyClassesStudentCountBatchReadPort port(
        services
        );
    const auto loaded = port.readMyClassesStudentCounts({});

    QVERIFY(loaded);
    QVERIFY(loaded.value().empty());
    const auto after = repository->myClassesStudentCountBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount);
    QCOMPARE(after.columnStatementCount, before.columnStatementCount);
    QCOMPARE(after.dataStatementCount, before.dataStatementCount);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount);
}

void NextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests::
unavailableSessionReturnsPerClassFailures()
{
    Platform::ApplicationServicesMyClassesStudentCountBatchReadPort port(
        nullptr
        );
    const auto loaded = port.readMyClassesStudentCounts({classId(7)});

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), std::size_t(1));
    QVERIFY(loaded.value().front().classId == classId(7));
    QVERIFY(!loaded.value().front().studentCount);
    QCOMPARE(loaded.value().front().studentCount.error().code,
             Domain::ErrorCode::NotFound);
}

QTEST_MAIN(
    NextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests
    )

#include "next_platform_application_services_my_classes_student_count_batch_read_port_tests.moc"
