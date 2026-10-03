#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/roster_repository.h"
#include "domain/models/roster.h"
#include "next/application/roster_availability_batch_read_query.h"
#include "next/application/roster_read_query.h"
#include "next/platform/application_services_roster_availability_batch_read_port.h"
#include "next/platform/application_services_roster_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>

#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("roster-read-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Application::RosterReadQuery query(const int classId)
{
    const auto typedId = Domain::ClassId::fromString(std::to_string(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    return {.classId = *typedId};
}

struct CellValue final
{
    int row;
    int column;
    QString value;
};

bool seedRawRoster(
    QSqlDatabase database,
    const int classId
    )
{
    const QStringList columns{
        QStringLiteral("Korean"),
        QStringLiteral("Winter"),
        QStringLiteral("English"),
        QStringLiteral("Autumn"),
        QStringLiteral("\u5099\u8003")
    };
    const QVector<int> widths{321, 222, 210, 333, 177};

    QSqlQuery insertColumn(database);
    insertColumn.prepare(QStringLiteral(
        "INSERT INTO roster_columns (class_id, name, position, width) "
        "VALUES (?, ?, ?, ?)"
        ));
    for (int column = 0; column < columns.size(); ++column)
    {
        insertColumn.bindValue(0, classId);
        insertColumn.bindValue(1, columns[column]);
        insertColumn.bindValue(2, column);
        insertColumn.bindValue(3, widths[column]);
        if (!insertColumn.exec())
        {
            return false;
        }
    }

    const std::vector<CellValue> cells{
        {0, 0, QStringLiteral("\uAE40\uBBFC\uC9C0")},
        {0, 1, QStringLiteral("A+")},
        {0, 2, QStringLiteral("First")},
        {0, 3, QStringLiteral("Fall A")},
        {0, 4, QStringLiteral("Review \U0001F4DA")},
        {24, 2, QStringLiteral("Middle 25")},
        {37, 4, QStringLiteral("\uC218\uC5C5 \U0001F4DA")}
    };
    QSqlQuery insertCell(database);
    insertCell.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, ?, ?, ?)"
        ));
    for (const CellValue& cell : cells)
    {
        insertCell.bindValue(0, classId);
        insertCell.bindValue(1, cell.row);
        insertCell.bindValue(2, cell.column);
        insertCell.bindValue(3, cell.value);
        if (!insertCell.exec())
        {
            return false;
        }
    }

    return true;
}

std::vector<std::u16string> baseColumnNames()
{
    std::vector<std::u16string> result;
    result.reserve(static_cast<std::size_t>(Roster::BaseColumns.size()));
    for (const QString& column : Roster::BaseColumns)
    {
        result.push_back(column.toStdU16String());
    }
    return result;
}

bool seedSparseRoster(
    QSqlDatabase database,
    const int classId,
    const QStringList& columns,
    const std::vector<CellValue>& cells
    )
{
    QSqlQuery insertColumn(database);
    if (!insertColumn.prepare(QStringLiteral(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (?, ?, ?, ?)"
            )))
    {
        return false;
    }
    for (int index = 0; index < columns.size(); ++index)
    {
        insertColumn.bindValue(0, classId);
        insertColumn.bindValue(1, columns[index]);
        insertColumn.bindValue(2, index);
        insertColumn.bindValue(3, 100);
        if (!insertColumn.exec())
        {
            return false;
        }
    }

    QSqlQuery insertCell(database);
    if (!insertCell.prepare(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (?, ?, ?, ?)"
            )))
    {
        return false;
    }
    for (const CellValue& cell : cells)
    {
        insertCell.bindValue(0, classId);
        insertCell.bindValue(1, cell.row);
        insertCell.bindValue(2, cell.column);
        insertCell.bindValue(3, cell.value);
        if (!insertCell.exec())
        {
            return false;
        }
    }
    return true;
}

Domain::ClassId typedClassId(const int classId)
{
    return *Domain::ClassId::fromString(std::to_string(classId));
}

}

class NextPlatformApplicationServicesRosterReadPortTests final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsNonCanonicalIdsBeforeSessionAccess();
    void readsCompleteRawSnapshotWithoutUiRowLimit();
    void readsMultipleRosterCapacitiesWithModelProjectionAndRowLimit();
    void missingRosterIsSuccessfulEmptySnapshot();
    void repositoryErrorIsStructured();
    void closedSessionFailsWithoutDataServiceFallback();
};

void NextPlatformApplicationServicesRosterReadPortTests::
rejectsNonCanonicalIdsBeforeSessionAccess()
{
    ApplicationServices services;
    Platform::ApplicationServicesRosterReadPort port(services);

    for (const std::string classId : {
             "0",
             "-2",
             "+42",
             "class-42",
             " 42",
             "42 ",
             "01",
             "00042",
             "999999999999999999999"
         })
    {
        const auto typedId = Domain::ClassId::fromString(classId);
        QVERIFY(typedId);

        const Application::RosterReadResult result = port.readRoster({
            .classId = *typedId
        });
        QVERIFY(!result);
        QCOMPARE(result.error().code, Domain::ErrorCode::InvalidInput);
    }
}

void NextPlatformApplicationServicesRosterReadPortTests::
readsCompleteRawSnapshotWithoutUiRowLimit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Roster Read Test")
        );
    QVERIFY(createdClass);
    QVERIFY(seedRawRoster(
        services.databaseSession()->database(),
        *createdClass
        ));

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY2(
        result,
        qPrintable(result ? QString() : QString::fromStdString(result.error().message))
        );
    QCOMPARE(
        result.value().columns,
        (std::vector<std::u16string>{
            u"Korean", u"Winter", u"English", u"Autumn", u"\u5099\u8003"
        })
        );
    QCOMPARE(result.value().columnWidths, (std::vector<int>{321, 222, 210, 333, 177}));
    QCOMPARE(result.value().rows.size(), std::size_t(38));
    QCOMPARE(result.value().rows[0].size(), std::size_t(5));
    QCOMPARE(result.value().rows[0][0], std::u16string(u"\uAE40\uBBFC\uC9C0"));
    QCOMPARE(result.value().rows[0][1], std::u16string(u"A+"));
    QCOMPARE(result.value().rows[0][2], std::u16string(u"First"));
    QCOMPARE(result.value().rows[0][3], std::u16string(u"Fall A"));
    QCOMPARE(result.value().rows[0][4], std::u16string(u"Review \U0001F4DA"));
    QVERIFY(result.value().rows[23][4].empty());
    QCOMPARE(result.value().rows[24][2], std::u16string(u"Middle 25"));
    QCOMPARE(result.value().rows[37][4], std::u16string(u"\uC218\uC5C5 \U0001F4DA"));
}

void NextPlatformApplicationServicesRosterReadPortTests::
readsMultipleRosterCapacitiesWithModelProjectionAndRowLimit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int fullId = services.classService()->create(
        QStringLiteral("Full Capacity")
        ).value_or(-1);
    const int emptyId = services.classService()->create(
        QStringLiteral("Empty Capacity")
        ).value_or(-1);
    const int aliasId = services.classService()->create(
        QStringLiteral("Alias Capacity")
        ).value_or(-1);
    const int customId = services.classService()->create(
        QStringLiteral("Custom Capacity")
        ).value_or(-1);
    const int overflowId = services.classService()->create(
        QStringLiteral("Overflow Capacity")
        ).value_or(-1);
    QVERIFY(fullId > 0);
    QVERIFY(emptyId > 0);
    QVERIFY(aliasId > 0);
    QVERIFY(customId > 0);
    QVERIFY(overflowId > 0);

    QSqlDatabase database = services.databaseSession()->database();
    std::vector<CellValue> fullCells;
    fullCells.reserve(26);
    for (int row = 0; row < 25; ++row)
    {
        fullCells.push_back({row, 0, QStringLiteral("Student")});
    }
    fullCells.push_back({25, 0, QStringLiteral("outside model")});
    QVERIFY(seedSparseRoster(
        database,
        fullId,
        {QStringLiteral("English")},
        fullCells
        ));
    QVERIFY(seedSparseRoster(
        database,
        aliasId,
        {
            QStringLiteral("Autumn"),
            QStringLiteral("Fall"),
            QStringLiteral("English"),
            QStringLiteral("English"),
            QStringLiteral("   ")
        },
        {
            {0, 0, QStringLiteral("Fall alias")},
            {1, 1, QStringLiteral("duplicate Fall")},
            {1, 3, QStringLiteral("duplicate English")},
            {1, 4, QStringLiteral("blank header")},
            {1, 99, QStringLiteral("invalid source column")}
        }
        ));
    QVERIFY(seedSparseRoster(
        database,
        customId,
        {QStringLiteral("Memo")},
        {{0, 0, QStringLiteral("custom data occupies the row")}}
        ));
    QVERIFY(seedSparseRoster(
        database,
        overflowId,
        {QStringLiteral("English")},
        {{25, 0, QStringLiteral("outside modeled rows")}}
        ));

    const QList<int> orderedIds{
        fullId,
        emptyId,
        aliasId,
        customId,
        overflowId
    };
    std::vector<Domain::ClassId> typedIds;
    typedIds.reserve(static_cast<std::size_t>(orderedIds.size()));
    for (const int classId : orderedIds)
    {
        typedIds.push_back(typedClassId(classId));
    }

    Platform::ApplicationServicesRosterAvailabilityBatchReadPort port(services);
    const Application::RosterAvailabilityBatchReadQuery query(port);
    const auto result = query.execute(typedIds, baseColumnNames());
    QVERIFY2(
        result,
        result ? "" : qPrintable(QString::fromStdString(result.error().message))
        );
    QCOMPARE(result.value().size(), std::size_t(5));
    QCOMPARE(result.value()[0].classId.value(), std::to_string(fullId));
    QCOMPARE(result.value()[0].firstEmptyRow, -1);
    QCOMPARE(result.value()[1].classId.value(), std::to_string(emptyId));
    QCOMPARE(result.value()[1].firstEmptyRow, 0);
    QCOMPARE(result.value()[2].classId.value(), std::to_string(aliasId));
    QCOMPARE(result.value()[2].firstEmptyRow, 1);
    QCOMPARE(result.value()[3].classId.value(), std::to_string(customId));
    QCOMPARE(result.value()[3].firstEmptyRow, 1);
    QCOMPARE(result.value()[4].classId.value(), std::to_string(overflowId));
    QCOMPARE(result.value()[4].firstEmptyRow, 0);

    std::vector<std::tuple<int, int, int>> streamedOrder;
    const Status streamed =
        services.databaseSession()->rosterRepository()
            ->forEachRosterDataCellForClasses(
                orderedIds,
                static_cast<int>(Application::RosterModeledRowCount),
                [&streamedOrder](
                    const int classId,
                    const int rowIndex,
                    const int columnIndex,
                    const QString&
                    )
                {
                    streamedOrder.emplace_back(classId, rowIndex, columnIndex);
                }
                );
    QVERIFY(streamed);
    QVERIFY(!streamedOrder.empty());
    QCOMPARE(std::get<0>(streamedOrder.front()), fullId);
    QCOMPARE(std::get<1>(streamedOrder.front()), 0);
    QCOMPARE(std::get<0>(streamedOrder.back()), customId);
    QCOMPARE(std::get<1>(streamedOrder.back()), 0);
    for (const auto& [classId, rowIndex, columnIndex] : streamedOrder)
    {
        Q_UNUSED(columnIndex);
        QVERIFY(classId != overflowId);
        QVERIFY(rowIndex >= 0 && rowIndex < 25);
    }

    QSqlQuery dropRosterData(database);
    QVERIFY2(dropRosterData.exec(QStringLiteral("DROP TABLE roster_data")),
        qPrintable(dropRosterData.lastError().text()));
    const auto failed = port.readRosterAvailability(typedIds, baseColumnNames());
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
}

void NextPlatformApplicationServicesRosterReadPortTests::
missingRosterIsSuccessfulEmptySnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Empty Roster Read Test")
        );
    QVERIFY(createdClass);

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY(result);
    QVERIFY(result.value().columns.empty());
    QVERIFY(result.value().columnWidths.empty());
    QVERIFY(result.value().rows.empty());
}

void NextPlatformApplicationServicesRosterReadPortTests::
repositoryErrorIsStructured()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Broken Roster Read Test")
        );
    QVERIFY(createdClass);

    QSqlQuery dropTable(services.databaseSession()->database());
    QVERIFY2(dropTable.exec(QStringLiteral("DROP TABLE roster_columns")),
        qPrintable(dropTable.lastError().text()));

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
}

void NextPlatformApplicationServicesRosterReadPortTests::
closedSessionFailsWithoutDataServiceFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Closed Roster Read Test")
        );
    QVERIFY(createdClass);

    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.dataService());
    QVERIFY(!services.dataService()->isOpen());

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(QString::fromStdString(result.error().message).contains(
        QStringLiteral("unavailable")
        ));
}

QTEST_MAIN(NextPlatformApplicationServicesRosterReadPortTests)

#include "next_platform_application_services_roster_read_port_tests.moc"
