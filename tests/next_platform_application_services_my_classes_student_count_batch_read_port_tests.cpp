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

#include <limits>
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

bool seedFractionalRowIndexCell(
    QSqlDatabase database,
    const int classIdValue,
    const QString& value
    )
{
    QSqlQuery insertCell(database);
    if (!insertCell.prepare(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (?, 0.25, 0, ?)"
            )))
    {
        return false;
    }
    insertCell.bindValue(0, classIdValue);
    insertCell.bindValue(1, value);
    return insertCell.exec();
}

bool deleteClassParentOnly(
    QSqlDatabase database,
    const int classIdValue
    )
{
    QSqlQuery removeParent(database);
    if (!removeParent.prepare(QStringLiteral(
            "DELETE FROM classes WHERE id=?"
            )))
    {
        return false;
    }
    removeParent.addBindValue(classIdValue);
    return removeParent.exec() && removeParent.numRowsAffected() == 1;
}

int nextMissingClassId(QSqlDatabase database)
{
    QSqlQuery selectId(database);
    if (!selectId.exec(QStringLiteral(
            "SELECT COALESCE(MAX(id), 0) + 1 FROM classes"
            ))
        || !selectId.next())
    {
        return 0;
    }
    return selectId.value(0).toInt();
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
    void reportsMissingParentsInBatchWithoutCountingOrphans();
    void rejectsFractionalRowIndexInBatchAndFallbackStudentCounts();
    void reportsMissingParentsAfterPerClassFallback();
    void countsDistinctRowsAboveIntMax();
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
    QCOMPARE(loaded.value()[0].studentCount.value().rawValue(), 3);
    QCOMPARE(loaded.value()[1].studentCount.value().rawValue(), 0);
    QCOMPARE(loaded.value()[2].studentCount.value().rawValue(), 3);

    const auto after = repository->myClassesStudentCountBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 3);
    QCOMPARE(after.columnStatementCount, before.columnStatementCount + 1);
    QCOMPARE(after.dataStatementCount, before.dataStatementCount + 1);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount);
}

void NextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests::
reportsMissingParentsInBatchWithoutCountingOrphans()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int orphanId = createClass(services, QStringLiteral("Orphan roster"));
    const int siblingId = createClass(services, QStringLiteral("Valid sibling"));
    const int noColumnsId = createClass(
        services,
        QStringLiteral("Present without roster columns")
        );
    QVERIFY(orphanId > 0);
    QVERIFY(siblingId > 0);
    QVERIFY(noColumnsId > 0);
    const QStringList headers{
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    QVERIFY(saveRoster(
        services,
        orphanId,
        roster(
            headers,
            {
                {QStringLiteral("Orphan one"), QString()},
                {QString(), QStringLiteral("\uD559\uC0DD")}
            }
            )
        ));
    QVERIFY(saveRoster(
        services,
        siblingId,
        roster(
            headers,
            {
                {QStringLiteral("Sibling one"), QString()},
                {QString(), QStringLiteral("\uD559\uC0DD")}
            }
            )
        ));
    QVERIFY(deleteClassParentOnly(
        services.databaseSession()->database(),
        orphanId
        ));

    const int neverPresentId = nextMissingClassId(
        services.databaseSession()->database()
        );
    QVERIFY(neverPresentId > 0);
    QVERIFY(neverPresentId != orphanId);

    RosterRepository* const repository = services.databaseSession()
        ->rosterRepository();
    QVERIFY(repository);
    const auto before = repository->myClassesStudentCountBatchReadMetrics();
    Platform::ApplicationServicesMyClassesStudentCountBatchReadPort port(
        services
        );
    const auto loaded = port.readMyClassesStudentCounts({
        classId(orphanId),
        classId(siblingId),
        classId(neverPresentId),
        classId(noColumnsId)
    });

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), std::size_t(4));
    QVERIFY(loaded.value()[0].classId == classId(orphanId));
    QVERIFY(!loaded.value()[0].studentCount);
    QCOMPARE(loaded.value()[0].studentCount.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(loaded.value()[0].studentCount.error().recoverable);
    QVERIFY(loaded.value()[1].classId == classId(siblingId));
    QVERIFY(loaded.value()[1].studentCount);
    QCOMPARE(loaded.value()[1].studentCount.value().rawValue(), 2);
    QVERIFY(loaded.value()[2].classId == classId(neverPresentId));
    QVERIFY(!loaded.value()[2].studentCount);
    QCOMPARE(loaded.value()[2].studentCount.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(loaded.value()[2].studentCount.error().recoverable);
    QVERIFY(loaded.value()[3].classId == classId(noColumnsId));
    QVERIFY(loaded.value()[3].studentCount);
    QCOMPARE(loaded.value()[3].studentCount.value().rawValue(), 0);

    const auto after = repository->myClassesStudentCountBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 4);
    QCOMPARE(after.columnStatementCount, before.columnStatementCount + 1);
    QCOMPARE(after.dataStatementCount, before.dataStatementCount + 1);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount);
}

void NextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests::
rejectsFractionalRowIndexInBatchAndFallbackStudentCounts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int fractionalId = createClass(
        services,
        QStringLiteral("Fractional student-count row")
        );
    const int siblingId = createClass(
        services,
        QStringLiteral("Valid sibling student-count rows")
        );
    QVERIFY(fractionalId > 0);
    QVERIFY(siblingId > 0);

    const QStringList columns{
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    QVERIFY(saveRoster(
        services,
        fractionalId,
        roster(columns, {{QString(), QString()}})
        ));
    QVERIFY(saveRoster(
        services,
        siblingId,
        roster(
            columns,
            {
                {QStringLiteral("Sibling one"), QString()},
                {QString(), QStringLiteral("\uD559\uC0DD")}
            }
            )
        ));

    QSqlQuery insertEmptyRow(services.databaseSession()->database());
    QVERIFY2(insertEmptyRow.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, 0, 0, '')"
        )), qPrintable(insertEmptyRow.lastError().text()));
    insertEmptyRow.bindValue(0, fractionalId);
    QVERIFY2(insertEmptyRow.exec(), qPrintable(insertEmptyRow.lastError().text()));

    const QString fractionalValue = QStringLiteral("Fractional student");
    QVERIFY(seedFractionalRowIndexCell(
        services.databaseSession()->database(),
        fractionalId,
        fractionalValue
        ));

    QSqlQuery storedRow(services.databaseSession()->database());
    QVERIFY2(storedRow.prepare(QStringLiteral(
        "SELECT row_index, typeof(row_index) AS storage_type, "
        "col_index, typeof(col_index) AS column_storage_type, value "
        "FROM roster_data WHERE class_id=? AND value=?"
        )), qPrintable(storedRow.lastError().text()));
    storedRow.bindValue(0, fractionalId);
    storedRow.bindValue(1, fractionalValue);
    QVERIFY2(storedRow.exec(), qPrintable(storedRow.lastError().text()));
    QVERIFY(storedRow.next());
    QCOMPARE(storedRow.value("row_index").toDouble(), 0.25);
    QCOMPARE(storedRow.value("storage_type").toString(), QStringLiteral("real"));
    QCOMPARE(storedRow.value("col_index").toInt(), 0);
    QCOMPARE(storedRow.value("column_storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(storedRow.value("value").toString(), fractionalValue);
    QVERIFY(!storedRow.next());

    QSqlQuery storedEmptyRow(services.databaseSession()->database());
    QVERIFY2(storedEmptyRow.prepare(QStringLiteral(
        "SELECT typeof(row_index) AS storage_type, value "
        "FROM roster_data WHERE class_id=? AND row_index=0 AND col_index=0"
        )), qPrintable(storedEmptyRow.lastError().text()));
    storedEmptyRow.bindValue(0, fractionalId);
    QVERIFY2(storedEmptyRow.exec(), qPrintable(storedEmptyRow.lastError().text()));
    QVERIFY(storedEmptyRow.next());
    QCOMPARE(storedEmptyRow.value("storage_type").toString(), QStringLiteral("integer"));
    QVERIFY(storedEmptyRow.value("value").toString().isEmpty());
    QVERIFY(!storedEmptyRow.next());

    RosterRepository* const repository = services.databaseSession()
        ->rosterRepository();
    QVERIFY(repository);
    const auto before = repository->myClassesStudentCountBatchReadMetrics();

    Platform::ApplicationServicesMyClassesStudentCountBatchReadPort port(
        services
        );
    const auto loaded = port.readMyClassesStudentCounts({
        classId(fractionalId),
        classId(siblingId)
    });

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), std::size_t(2));
    QVERIFY(loaded.value()[0].classId == classId(fractionalId));
    QVERIFY(!loaded.value()[0].studentCount);
    QCOMPARE(
        loaded.value()[0].studentCount.error().code,
        Domain::ErrorCode::Technical
        );
    QVERIFY(loaded.value()[0].studentCount.error().message.find("row index")
        != std::string::npos);
    QVERIFY(loaded.value()[1].classId == classId(siblingId));
    QVERIFY(loaded.value()[1].studentCount);
    QCOMPARE(loaded.value()[1].studentCount.value().rawValue(), 2);

    const auto after = repository->myClassesStudentCountBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 2);
    QCOMPARE(after.columnStatementCount, before.columnStatementCount + 3);
    QCOMPARE(after.dataStatementCount, before.dataStatementCount + 3);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount + 2);
}

void NextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests::
reportsMissingParentsAfterPerClassFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int orphanId = createClass(services, QStringLiteral("Fallback orphan"));
    const int siblingId = createClass(services, QStringLiteral("Fallback sibling"));
    const int malformedId = createClass(
        services,
        QStringLiteral("Fallback malformed row")
        );
    const int noColumnsId = createClass(
        services,
        QStringLiteral("Fallback present without columns")
        );
    QVERIFY(orphanId > 0);
    QVERIFY(siblingId > 0);
    QVERIFY(malformedId > 0);
    QVERIFY(noColumnsId > 0);

    const QStringList headers{
        QStringLiteral("English"),
        QStringLiteral("Korean")
    };
    QVERIFY(saveRoster(
        services,
        orphanId,
        roster(headers, {{QStringLiteral("Orphan student"), QString()}})
        ));
    QVERIFY(saveRoster(
        services,
        siblingId,
        roster(
            headers,
            {
                {QStringLiteral("Sibling one"), QString()},
                {QString(), QStringLiteral("\uD559\uC0DD")}
            }
            )
        ));
    QVERIFY(saveRoster(
        services,
        malformedId,
        roster({QStringLiteral("English")}, {{QString()}})
        ));
    QVERIFY(seedFractionalRowIndexCell(
        services.databaseSession()->database(),
        malformedId,
        QStringLiteral("Fractional trigger")
        ));
    QVERIFY(deleteClassParentOnly(
        services.databaseSession()->database(),
        orphanId
        ));

    const int neverPresentId = nextMissingClassId(
        services.databaseSession()->database()
        );
    QVERIFY(neverPresentId > 0);
    QVERIFY(neverPresentId != orphanId);

    RosterRepository* const repository = services.databaseSession()
        ->rosterRepository();
    QVERIFY(repository);
    const auto before = repository->myClassesStudentCountBatchReadMetrics();
    Platform::ApplicationServicesMyClassesStudentCountBatchReadPort port(
        services
        );
    const auto loaded = port.readMyClassesStudentCounts({
        classId(orphanId),
        classId(siblingId),
        classId(malformedId),
        classId(noColumnsId),
        classId(neverPresentId)
    });

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), std::size_t(5));
    QVERIFY(loaded.value()[0].classId == classId(orphanId));
    QVERIFY(!loaded.value()[0].studentCount);
    QCOMPARE(loaded.value()[0].studentCount.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(loaded.value()[0].studentCount.error().recoverable);
    QVERIFY(loaded.value()[1].classId == classId(siblingId));
    QVERIFY(loaded.value()[1].studentCount);
    QCOMPARE(loaded.value()[1].studentCount.value().rawValue(), 2);
    QVERIFY(loaded.value()[2].classId == classId(malformedId));
    QVERIFY(!loaded.value()[2].studentCount);
    QCOMPARE(loaded.value()[2].studentCount.error().code,
             Domain::ErrorCode::Technical);
    QVERIFY(loaded.value()[3].classId == classId(noColumnsId));
    QVERIFY(loaded.value()[3].studentCount);
    QCOMPARE(loaded.value()[3].studentCount.value().rawValue(), 0);
    QVERIFY(loaded.value()[4].classId == classId(neverPresentId));
    QVERIFY(!loaded.value()[4].studentCount);
    QCOMPARE(loaded.value()[4].studentCount.error().code,
             Domain::ErrorCode::NotFound);
    QVERIFY(loaded.value()[4].studentCount.error().recoverable);

    const auto after = repository->myClassesStudentCountBatchReadMetrics();
    QCOMPARE(after.callCount, before.callCount + 1);
    QCOMPARE(after.requestedClassCount, before.requestedClassCount + 5);
    QCOMPARE(after.columnStatementCount, before.columnStatementCount + 6);
    QCOMPARE(after.dataStatementCount, before.dataStatementCount + 3);
    QCOMPARE(after.fallbackClassReadCount, before.fallbackClassReadCount + 5);
}

void NextPlatformApplicationServicesMyClassesStudentCountBatchReadPortTests::
countsDistinctRowsAboveIntMax()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int classIdValue = createClass(
        services,
        QStringLiteral("Wide row index class")
        );
    QVERIFY(classIdValue > 0);
    QVERIFY(saveRoster(
        services,
        classIdValue,
        roster(
            {QStringLiteral("English"), QStringLiteral("Korean")},
            {}
            )
        ));

    const qlonglong firstRowIndex =
        static_cast<qlonglong>(std::numeric_limits<int>::max()) + 1;
    const qlonglong secondRowIndex = firstRowIndex + 1;
    QSqlQuery insertCell(services.databaseSession()->database());
    QVERIFY2(
        insertCell.prepare(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (?, ?, ?, ?)"
            )),
        qPrintable(insertCell.lastError().text())
        );
    const auto insert = [
        &insertCell,
        classIdValue
        ](const qlonglong rowIndex, const int columnIndex, const QString& value)
    {
        insertCell.bindValue(0, classIdValue);
        insertCell.bindValue(1, rowIndex);
        insertCell.bindValue(2, columnIndex);
        insertCell.bindValue(3, value);
        return insertCell.exec();
    };
    QVERIFY2(
        insert(firstRowIndex, 0, QStringLiteral("Alice")),
        qPrintable(insertCell.lastError().text())
        );
    QVERIFY2(
        insert(firstRowIndex, 1, QStringLiteral("\uD559\uC0DD")),
        qPrintable(insertCell.lastError().text())
        );
    QVERIFY2(
        insert(secondRowIndex, 0, QStringLiteral(" \u2003 ")),
        qPrintable(insertCell.lastError().text())
        );
    QVERIFY2(
        insert(secondRowIndex, 1, QStringLiteral("\uD559")),
        qPrintable(insertCell.lastError().text())
        );

    QSqlQuery storedIndexes(services.databaseSession()->database());
    QVERIFY2(
        storedIndexes.prepare(QStringLiteral(
            "SELECT row_index, typeof(row_index) "
            "FROM roster_data WHERE class_id=? "
            "ORDER BY row_index"
            )),
        qPrintable(storedIndexes.lastError().text())
        );
    storedIndexes.addBindValue(classIdValue);
    QVERIFY2(
        storedIndexes.exec(),
        qPrintable(storedIndexes.lastError().text())
        );
    QVERIFY(storedIndexes.next());
    QCOMPARE(storedIndexes.value(0).toLongLong(), firstRowIndex);
    QCOMPARE(storedIndexes.value(1).toString(), QStringLiteral("integer"));
    QVERIFY(storedIndexes.next());
    QCOMPARE(storedIndexes.value(0).toLongLong(), firstRowIndex);
    QCOMPARE(storedIndexes.value(1).toString(), QStringLiteral("integer"));
    QVERIFY(storedIndexes.next());
    QCOMPARE(storedIndexes.value(0).toLongLong(), secondRowIndex);
    QCOMPARE(storedIndexes.value(1).toString(), QStringLiteral("integer"));
    QVERIFY(storedIndexes.next());
    QCOMPARE(storedIndexes.value(0).toLongLong(), secondRowIndex);
    QCOMPARE(storedIndexes.value(1).toString(), QStringLiteral("integer"));
    QVERIFY(!storedIndexes.next());

    Platform::ApplicationServicesMyClassesStudentCountBatchReadPort port(
        services
        );
    const auto loaded = port.readMyClassesStudentCounts({
        classId(classIdValue)
    });

    QVERIFY(loaded);
    QCOMPARE(loaded.value().size(), std::size_t(1));
    QVERIFY(loaded.value().front().classId == classId(classIdValue));
    QVERIFY(loaded.value().front().studentCount);
    QCOMPARE(loaded.value().front().studentCount.value().rawValue(), 2);
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
