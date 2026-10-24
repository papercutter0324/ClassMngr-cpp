#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "data/database/database_session.h"
#include "data/repositories/roster_repository.h"
#include "domain/models/roster.h"
#include "next/application/roster_availability_batch_read_query.h"
#include "next/application/roster_read_query.h"
#include "next/application/roster_window_read_query.h"
#include "next/platform/application_services_roster_availability_batch_read_port.h"
#include "next/platform/application_services_roster_read_port.h"
#include "next/platform/application_services_roster_window_read_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
#include <QUuid>
#include <QtTest/QtTest>

#include <limits>
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

bool seedMalformedColumnIndexCell(
    QSqlDatabase database,
    const int classId,
    const QString& value
    )
{
    QSqlQuery insertCell(database);
    if (!insertCell.prepare(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (?, 0, ?, ?)"
            )))
    {
        return false;
    }
    insertCell.bindValue(0, classId);
    insertCell.bindValue(1, QStringLiteral("not-a-column"));
    insertCell.bindValue(2, value);
    return insertCell.exec();
}

bool seedFractionalColumnIndexCell(
    QSqlDatabase database,
    const int classId,
    const int rowIndex,
    const QString& value
    )
{
    QSqlQuery insertCell(database);
    if (!insertCell.prepare(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (?, ?, 0.25, ?)"
            )))
    {
        return false;
    }
    insertCell.bindValue(0, classId);
    insertCell.bindValue(1, rowIndex);
    insertCell.bindValue(2, value);
    return insertCell.exec();
}

bool seedFractionalRowIndexCell(
    QSqlDatabase database,
    const int classId,
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
    insertCell.bindValue(0, classId);
    insertCell.bindValue(1, value);
    return insertCell.exec();
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
    void readsBoundedWindowWithSparseRowsAndReportsTailRows();
    void windowReadMissingParentReturnsRecoverableNotFound();
    void windowReadRepositoryFailureReturnsTechnicalError();
    void windowReadClosedStoreReturnsRecoverableNotFound();
    void rejectsOutOfRangeIntegerRosterWidth();
    void preservesNullAndBoundaryIntegerRosterWidths();
    void preservesNonIntegerTextWidthFallback();
    void rejectsMalformedColumnPosition();
    void acceptsSparsePositionBeyondIntRange();
    void rejectsMalformedColumnIndexWithoutOverwritingColumnZero();
    void rejectsFractionalColumnIndexBeforeOverwritingColumnZero();
    void rejectsFractionalAvailabilityColumnIndex();
    void rejectsFractionalAvailabilityRowIndex();
    void rejectsRowIndexBeyondIntRangeWithoutDenseExpansion();
    void readsMultipleRosterCapacitiesWithModelProjectionAndRowLimit();
    void preservesMixedParentProvenanceOutcomesInRequestOrder();
    void rejectsMalformedAvailabilityColumnIndex();
    void missingParentReturnsRecoverableNotFound();
    void deletedParentWithOrphanRosterKeepsLegacyData();
    void missingRosterIsSuccessfulEmptySnapshot();
    void repositoryErrorIsStructured();
    void rosterDataQueryErrorIsTechnical();
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
        services.workspaceStore()->database(),
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
    QCOMPARE(result.value().columnWidths,
        (std::vector<Application::RosterSnapshotColumnWidth>{
            Application::RosterSnapshotColumnWidth{321},
            Application::RosterSnapshotColumnWidth{222},
            Application::RosterSnapshotColumnWidth{210},
            Application::RosterSnapshotColumnWidth{333},
            Application::RosterSnapshotColumnWidth{177}
        }));
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
readsBoundedWindowWithSparseRowsAndReportsTailRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Bounded Roster Read Test")
        );
    QVERIFY(createdClass);
    QVERIFY(seedRawRoster(
        services.workspaceStore()->database(),
        *createdClass
        ));

    const auto classId = Domain::ClassId::fromString(
        std::to_string(*createdClass)
        );
    QVERIFY(classId);
    Platform::ApplicationServicesRosterWindowReadPort port(services);
    const auto result = Application::RosterWindowReadUseCase::execute(
        {.classId = *classId},
        port
        );

    QVERIFY2(
        result,
        qPrintable(result ? QString() : QString::fromStdString(result.error().message))
        );
    QVERIFY(result.value().hasRowsOutsideWindow);
    QCOMPARE(result.value().roster.columns,
        (std::vector<std::u16string>{
            u"Korean", u"Winter", u"English", u"Autumn", u"\u5099\u8003"
        }));
    QCOMPARE(result.value().roster.columnWidths,
        (std::vector<Application::RosterSnapshotColumnWidth>{
            Application::RosterSnapshotColumnWidth{321},
            Application::RosterSnapshotColumnWidth{222},
            Application::RosterSnapshotColumnWidth{210},
            Application::RosterSnapshotColumnWidth{333},
            Application::RosterSnapshotColumnWidth{177}
        }));
    QCOMPARE(result.value().roster.rows.size(), std::size_t(25));
    QCOMPARE(result.value().roster.rows[0][0],
        std::u16string(u"\uAE40\uBBFC\uC9C0"));
    QVERIFY(result.value().roster.rows[1][0].empty());
    QCOMPARE(result.value().roster.rows[24][2],
        std::u16string(u"Middle 25"));
}

void NextPlatformApplicationServicesRosterReadPortTests::
windowReadMissingParentReturnsRecoverableNotFound()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Platform::ApplicationServicesRosterWindowReadPort port(services);
    const auto result = Application::RosterWindowReadUseCase::execute(
        {.classId = typedClassId(987654321)},
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
    QVERIFY(result.error().message.find("not found") != std::string::npos);
}

void NextPlatformApplicationServicesRosterReadPortTests::
windowReadRepositoryFailureReturnsTechnicalError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Broken Bounded Roster Read Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.databaseSession()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("English")},
        {{0, 0, QStringLiteral("Student")}}
        ));
    QSqlQuery dropTable(database);
    QVERIFY2(
        dropTable.exec(QStringLiteral("DROP TABLE roster_data")),
        qPrintable(dropTable.lastError().text())
        );

    Platform::ApplicationServicesRosterWindowReadPort port(services);
    const auto result = Application::RosterWindowReadUseCase::execute(
        {.classId = typedClassId(*createdClass)},
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(result.error().recoverable);
    QVERIFY(!result.error().message.empty());
}

void NextPlatformApplicationServicesRosterReadPortTests::
windowReadClosedStoreReturnsRecoverableNotFound()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Closed Bounded Roster Read Test")
        );
    QVERIFY(createdClass);

    services.closeDatabase();
    QVERIFY(!services.hasOpenDatabase());
    QVERIFY(services.dataService());
    QVERIFY(!services.dataService()->isOpen());

    Platform::ApplicationServicesRosterWindowReadPort port(services);
    const auto result = Application::RosterWindowReadUseCase::execute(
        {.classId = typedClassId(*createdClass)},
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
    QVERIFY(result.error().message.find("unavailable") != std::string::npos);
}

void NextPlatformApplicationServicesRosterReadPortTests::
rejectsOutOfRangeIntegerRosterWidth()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Out of range roster width")
        );
    QVERIFY(createdClass);
    QSqlDatabase database = services.workspaceStore()->database();

    QSqlQuery insertColumn(database);
    QVERIFY(insertColumn.prepare(QStringLiteral(
        "INSERT INTO roster_columns (class_id, name, position, width) "
        "VALUES (?, 'Student', 0, ?)"
        )));
    insertColumn.addBindValue(*createdClass);
    insertColumn.addBindValue(QVariant::fromValue<qlonglong>(4294967297LL));
    QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));

    QSqlQuery storedWidth(database);
    QVERIFY(storedWidth.prepare(QStringLiteral(
        "SELECT width, typeof(width) AS storage_type FROM roster_columns "
        "WHERE class_id=?"
        )));
    storedWidth.addBindValue(*createdClass);
    QVERIFY2(storedWidth.exec(), qPrintable(storedWidth.lastError().text()));
    QVERIFY(storedWidth.next());
    QCOMPARE(storedWidth.value("storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(storedWidth.value("width").toLongLong(), qlonglong(4294967297LL));
    QVERIFY(!storedWidth.next());

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(result.error().message);
    QVERIFY2(
        diagnostic.contains(QStringLiteral("class id %1").arg(*createdClass))
            && diagnostic.contains(QStringLiteral("width"), Qt::CaseInsensitive)
            && diagnostic.contains(QStringLiteral("4294967297")),
        qPrintable(diagnostic)
        );
}

void NextPlatformApplicationServicesRosterReadPortTests::
preservesNullAndBoundaryIntegerRosterWidths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Boundary roster widths")
        );
    QVERIFY(createdClass);
    QSqlDatabase database = services.workspaceStore()->database();

    QSqlQuery insertColumn(database);
    QVERIFY(insertColumn.prepare(QStringLiteral(
        "INSERT INTO roster_columns (class_id, name, position, width) "
        "VALUES (?, ?, ?, ?)"
        )));
    const QVector<QPair<QString, QVariant>> columns{
        {QStringLiteral("Minimum"), QVariant::fromValue<qlonglong>(
            std::numeric_limits<int>::min())},
        {QStringLiteral("Negative one"), QVariant::fromValue<qlonglong>(-1)},
        {QStringLiteral("Null"), QVariant{}},
        {QStringLiteral("Maximum"), QVariant::fromValue<qlonglong>(
            std::numeric_limits<int>::max())}
    };
    for (int index = 0; index < columns.size(); ++index)
    {
        insertColumn.bindValue(0, *createdClass);
        insertColumn.bindValue(1, columns[index].first);
        insertColumn.bindValue(2, index);
        insertColumn.bindValue(3, columns[index].second);
        QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));
    }

    QSqlQuery storedWidths(database);
    QVERIFY(storedWidths.prepare(QStringLiteral(
        "SELECT width, typeof(width) AS storage_type FROM roster_columns "
        "WHERE class_id=? ORDER BY position"
        )));
    storedWidths.addBindValue(*createdClass);
    QVERIFY2(storedWidths.exec(), qPrintable(storedWidths.lastError().text()));
    QVERIFY(storedWidths.next());
    QCOMPARE(storedWidths.value("storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(storedWidths.value("width").toLongLong(),
        qlonglong(std::numeric_limits<int>::min()));
    QVERIFY(storedWidths.next());
    QCOMPARE(storedWidths.value("storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(storedWidths.value("width").toLongLong(), qlonglong(-1));
    QVERIFY(storedWidths.next());
    QCOMPARE(storedWidths.value("storage_type").toString(), QStringLiteral("null"));
    QVERIFY(storedWidths.value("width").isNull());
    QVERIFY(storedWidths.next());
    QCOMPARE(storedWidths.value("storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(storedWidths.value("width").toLongLong(),
        qlonglong(std::numeric_limits<int>::max()));
    QVERIFY(!storedWidths.next());

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY(result);
    QCOMPARE(result.value().columnWidths, (std::vector<Application::RosterSnapshotColumnWidth>{
        Application::RosterSnapshotColumnWidth{std::numeric_limits<int>::min()},
        Application::RosterSnapshotColumnWidth{-1},
        Application::RosterSnapshotColumnWidth{0},
        Application::RosterSnapshotColumnWidth{std::numeric_limits<int>::max()}
    }));
}

void NextPlatformApplicationServicesRosterReadPortTests::
preservesNonIntegerTextWidthFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Text roster width fallback")
        );
    QVERIFY(createdClass);
    QSqlDatabase database = services.workspaceStore()->database();

    QSqlQuery insertColumn(database);
    QVERIFY(insertColumn.prepare(QStringLiteral(
        "INSERT INTO roster_columns (class_id, name, position, width) "
        "VALUES (?, 'Student', 0, ?)"
        )));
    insertColumn.addBindValue(*createdClass);
    insertColumn.addBindValue(QStringLiteral("not-a-width"));
    QVERIFY2(insertColumn.exec(), qPrintable(insertColumn.lastError().text()));

    QSqlQuery storedWidth(database);
    QVERIFY(storedWidth.prepare(QStringLiteral(
        "SELECT width, typeof(width) AS storage_type FROM roster_columns "
        "WHERE class_id=?"
        )));
    storedWidth.addBindValue(*createdClass);
    QVERIFY2(storedWidth.exec(), qPrintable(storedWidth.lastError().text()));
    QVERIFY(storedWidth.next());
    QCOMPARE(storedWidth.value("storage_type").toString(), QStringLiteral("text"));
    QCOMPARE(storedWidth.value("width").toString(), QStringLiteral("not-a-width"));
    const int legacyWidth = storedWidth.value("width").toInt();
    QCOMPARE(legacyWidth, 0);
    QVERIFY(!storedWidth.next());

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY(result);
    QCOMPARE(result.value().columnWidths,
        (std::vector<Application::RosterSnapshotColumnWidth>{
            Application::RosterSnapshotColumnWidth{legacyWidth}
        }));
}

void NextPlatformApplicationServicesRosterReadPortTests::
rejectsMalformedColumnPosition()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Malformed Column Position Roster Read Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("First"), QStringLiteral("Second")},
        {
            {0, 0, QStringLiteral("First column value")},
            {0, 1, QStringLiteral("Second column value")}
        }
        ));

    QSqlQuery updatePosition(database);
    QVERIFY2(updatePosition.prepare(QStringLiteral(
        "UPDATE roster_columns SET position=? WHERE class_id=? AND name=?"
        )), qPrintable(updatePosition.lastError().text()));
    updatePosition.bindValue(0, QStringLiteral("not-a-position"));
    updatePosition.bindValue(1, *createdClass);
    updatePosition.bindValue(2, QStringLiteral("First"));
    QVERIFY2(updatePosition.exec(), qPrintable(updatePosition.lastError().text()));
    QCOMPARE(updatePosition.numRowsAffected(), 1);

    QSqlQuery storedType(database);
    QVERIFY2(storedType.prepare(QStringLiteral(
        "SELECT typeof(position) AS storage_type FROM roster_columns "
        "WHERE class_id=? AND name=?"
        )), qPrintable(storedType.lastError().text()));
    storedType.bindValue(0, *createdClass);
    storedType.bindValue(1, QStringLiteral("First"));
    QVERIFY2(storedType.exec(), qPrintable(storedType.lastError().text()));
    QVERIFY(storedType.next());
    QCOMPARE(storedType.value("storage_type").toString(), QStringLiteral("text"));
    QVERIFY(!storedType.next());

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(result.error().message);
    QVERIFY2(
        diagnostic.contains(QStringLiteral("position"), Qt::CaseInsensitive),
        qPrintable(diagnostic)
        );
}

void NextPlatformApplicationServicesRosterReadPortTests::
acceptsSparsePositionBeyondIntRange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Sparse Large Column Position Roster Read Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("First"), QStringLiteral("Second")},
        {
            {0, 0, QStringLiteral("First column value")},
            {0, 1, QStringLiteral("Second column value")}
        }
        ));

    const qlonglong secondPosition =
        static_cast<qlonglong>(std::numeric_limits<int>::max()) + 1;
    QSqlQuery updatePosition(database);
    QVERIFY2(updatePosition.prepare(QStringLiteral(
        "UPDATE roster_columns SET position=? WHERE class_id=? AND name=?"
        )), qPrintable(updatePosition.lastError().text()));
    updatePosition.bindValue(
        0, QVariant::fromValue<qlonglong>(secondPosition));
    updatePosition.bindValue(1, *createdClass);
    updatePosition.bindValue(2, QStringLiteral("Second"));
    QVERIFY2(updatePosition.exec(), qPrintable(updatePosition.lastError().text()));
    QCOMPARE(updatePosition.numRowsAffected(), 1);

    QSqlQuery storedPosition(database);
    QVERIFY2(storedPosition.prepare(QStringLiteral(
        "SELECT typeof(position) AS storage_type, position "
        "FROM roster_columns WHERE class_id=? AND name=?"
        )), qPrintable(storedPosition.lastError().text()));
    storedPosition.bindValue(0, *createdClass);
    storedPosition.bindValue(1, QStringLiteral("Second"));
    QVERIFY2(storedPosition.exec(), qPrintable(storedPosition.lastError().text()));
    QVERIFY(storedPosition.next());
    QCOMPARE(
        storedPosition.value("storage_type").toString(),
        QStringLiteral("integer")
        );
    bool storedPositionValid = false;
    QCOMPARE(
        storedPosition.value("position").toLongLong(&storedPositionValid),
        secondPosition
        );
    QVERIFY(storedPositionValid);
    QVERIFY(!storedPosition.next());

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
        (std::vector<std::u16string>{u"First", u"Second"})
        );
    QCOMPARE(result.value().rows.size(), std::size_t(1));
    QCOMPARE(
        result.value().rows[0],
        (std::vector<std::u16string>{
            u"First column value", u"Second column value"
        })
        );
}

void NextPlatformApplicationServicesRosterReadPortTests::
rejectsMalformedColumnIndexWithoutOverwritingColumnZero()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Malformed Column Index Roster Read Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("English")},
        {{0, 0, QStringLiteral("Valid column-zero value")}}
        ));
    const QString malformedValue = QStringLiteral("Malformed column text");
    QVERIFY(seedMalformedColumnIndexCell(
        database,
        *createdClass,
        malformedValue
        ));

    QSqlQuery storedType(database);
    QVERIFY2(storedType.prepare(QStringLiteral(
        "SELECT typeof(col_index) AS storage_type FROM roster_data "
        "WHERE class_id=? AND value=?"
        )), qPrintable(storedType.lastError().text()));
    storedType.bindValue(0, *createdClass);
    storedType.bindValue(1, malformedValue);
    QVERIFY2(storedType.exec(), qPrintable(storedType.lastError().text()));
    QVERIFY(storedType.next());
    QCOMPARE(storedType.value("storage_type").toString(), QStringLiteral("text"));
    QVERIFY(!storedType.next());

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(result.error().message);
    QVERIFY2(
        diagnostic.contains(QStringLiteral("column index"), Qt::CaseInsensitive),
        qPrintable(diagnostic)
        );
}

void NextPlatformApplicationServicesRosterReadPortTests::
rejectsFractionalColumnIndexBeforeOverwritingColumnZero()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Fractional Column Index Projection Rejection Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("English")},
        {{0, 0, QStringLiteral("Integer column zero")}}
        ));
    const QString fractionalValue = QStringLiteral("Fractional column value");
    QVERIFY(seedFractionalColumnIndexCell(
        database,
        *createdClass,
        0,
        fractionalValue
        ));

    QSqlQuery storedType(database);
    QVERIFY2(storedType.prepare(QStringLiteral(
        "SELECT typeof(col_index) AS storage_type FROM roster_data "
        "WHERE class_id=? AND value=?"
        )), qPrintable(storedType.lastError().text()));
    storedType.bindValue(0, *createdClass);
    storedType.bindValue(1, fractionalValue);
    QVERIFY2(storedType.exec(), qPrintable(storedType.lastError().text()));
    QVERIFY(storedType.next());
    QCOMPARE(storedType.value("storage_type").toString(), QStringLiteral("real"));
    QVERIFY(!storedType.next());

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(result.error().message);
    QVERIFY2(
        diagnostic.contains(QStringLiteral("column index"), Qt::CaseInsensitive),
        qPrintable(diagnostic)
        );
}

void NextPlatformApplicationServicesRosterReadPortTests::
rejectsFractionalAvailabilityColumnIndex()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Fractional Column Index Availability Rejection Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("English")},
        {{0, 0, QString()}}
        ));
    const QString fractionalValue = QStringLiteral("Occupies no valid column");
    QVERIFY(seedFractionalColumnIndexCell(
        database,
        *createdClass,
        0,
        fractionalValue
        ));

    QSqlQuery storedType(database);
    QVERIFY2(storedType.prepare(QStringLiteral(
        "SELECT typeof(col_index) AS storage_type FROM roster_data "
        "WHERE class_id=? AND value=?"
        )), qPrintable(storedType.lastError().text()));
    storedType.bindValue(0, *createdClass);
    storedType.bindValue(1, fractionalValue);
    QVERIFY2(storedType.exec(), qPrintable(storedType.lastError().text()));
    QVERIFY(storedType.next());
    QCOMPARE(storedType.value("storage_type").toString(), QStringLiteral("real"));
    QVERIFY(!storedType.next());

    Platform::ApplicationServicesRosterAvailabilityBatchReadPort port(services);
    const Application::RosterAvailabilityBatchReadQuery availabilityQuery(port);
    const auto result = availabilityQuery.execute(
        {typedClassId(*createdClass)},
        baseColumnNames()
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(result.error().message);
    QVERIFY2(
        diagnostic.contains(QStringLiteral("column index"), Qt::CaseInsensitive),
        qPrintable(diagnostic)
        );
}

void NextPlatformApplicationServicesRosterReadPortTests::
rejectsFractionalAvailabilityRowIndex()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Fractional Row Index Availability Rejection Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("English")},
        {{0, 0, QString()}}
        ));
    const QString fractionalValue = QStringLiteral("Fractional student cell");
    QVERIFY(seedFractionalRowIndexCell(
        database,
        *createdClass,
        fractionalValue
        ));

    QSqlQuery storedType(database);
    QVERIFY2(storedType.prepare(QStringLiteral(
        "SELECT row_index, typeof(row_index) AS row_storage_type, "
        "col_index, typeof(col_index) AS column_storage_type, value "
        "FROM roster_data WHERE class_id=? ORDER BY row_index"
        )), qPrintable(storedType.lastError().text()));
    storedType.bindValue(0, *createdClass);
    QVERIFY2(storedType.exec(), qPrintable(storedType.lastError().text()));
    QVERIFY(storedType.next());
    bool emptyRowIndexValid = false;
    QCOMPARE(
        storedType.value("row_index").toLongLong(&emptyRowIndexValid),
        qlonglong(0)
        );
    QVERIFY(emptyRowIndexValid);
    QCOMPARE(storedType.value("row_storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(storedType.value("col_index").toInt(), 0);
    QCOMPARE(storedType.value("column_storage_type").toString(), QStringLiteral("integer"));
    QVERIFY(storedType.value("value").toString().isEmpty());
    QVERIFY(storedType.next());
    QCOMPARE(storedType.value("row_index").toDouble(), 0.25);
    QCOMPARE(storedType.value("row_storage_type").toString(), QStringLiteral("real"));
    QCOMPARE(storedType.value("col_index").toInt(), 0);
    QCOMPARE(storedType.value("column_storage_type").toString(), QStringLiteral("integer"));
    QCOMPARE(storedType.value("value").toString(), fractionalValue);
    QVERIFY(!storedType.next());

    Platform::ApplicationServicesRosterAvailabilityBatchReadPort port(services);
    const Application::RosterAvailabilityBatchReadQuery availabilityQuery(port);
    const auto result = availabilityQuery.execute(
        {typedClassId(*createdClass)},
        baseColumnNames()
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(result.error().message);
    QVERIFY2(
        diagnostic.contains(QStringLiteral("row index"), Qt::CaseInsensitive),
        qPrintable(diagnostic)
        );
}

void NextPlatformApplicationServicesRosterReadPortTests::
rejectsRowIndexBeyondIntRangeWithoutDenseExpansion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Out of Range Roster Read Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("English")},
        {}
        ));

    const qlonglong outOfRangeRowIndex =
        static_cast<qlonglong>(std::numeric_limits<int>::max()) + 1;
    QSqlQuery insertCell(database);
    QVERIFY2(insertCell.prepare(QStringLiteral(
        "INSERT INTO roster_data (class_id, row_index, col_index, value) "
        "VALUES (?, ?, ?, ?)"
        )), qPrintable(insertCell.lastError().text()));
    insertCell.bindValue(0, *createdClass);
    insertCell.bindValue(1, QVariant::fromValue<qlonglong>(outOfRangeRowIndex));
    insertCell.bindValue(2, 0);
    insertCell.bindValue(3, QStringLiteral("out of range"));
    QVERIFY2(insertCell.exec(), qPrintable(insertCell.lastError().text()));

    QSqlQuery storedCell(database);
    QVERIFY2(storedCell.prepare(QStringLiteral(
        "SELECT typeof(row_index) AS storage_type, row_index "
        "FROM roster_data WHERE class_id=?"
        )), qPrintable(storedCell.lastError().text()));
    storedCell.bindValue(0, *createdClass);
    QVERIFY2(storedCell.exec(), qPrintable(storedCell.lastError().text()));
    QVERIFY(storedCell.next());
    QCOMPARE(storedCell.value("storage_type").toString(), QStringLiteral("integer"));
    bool storedRowIndexValid = false;
    const qlonglong storedRowIndex =
        storedCell.value("row_index").toLongLong(&storedRowIndexValid);
    QVERIFY(storedRowIndexValid);
    QCOMPARE(storedRowIndex, outOfRangeRowIndex);
    QVERIFY(!storedCell.next());

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(result.error().message);
    QVERIFY2(
        diagnostic.contains(QStringLiteral("row index"), Qt::CaseInsensitive),
        qPrintable(diagnostic)
        );
    QVERIFY2(
        diagnostic.contains(QStringLiteral("range"), Qt::CaseInsensitive),
        qPrintable(diagnostic)
        );
    QVERIFY2(
        diagnostic.contains(QString::number(outOfRangeRowIndex)),
        qPrintable(diagnostic)
        );
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

    QSqlDatabase database = services.workspaceStore()->database();
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
    QVERIFY(result.value()[0].firstEmptyRowResult);
    QVERIFY(result.value()[0].firstEmptyRowResult.value().isFull());
    QCOMPARE(result.value()[1].classId.value(), std::to_string(emptyId));
    QVERIFY(result.value()[1].firstEmptyRowResult);
    QCOMPARE(result.value()[1].firstEmptyRowResult.value().rawValue(), 0);
    QCOMPARE(result.value()[2].classId.value(), std::to_string(aliasId));
    QVERIFY(result.value()[2].firstEmptyRowResult);
    QCOMPARE(result.value()[2].firstEmptyRowResult.value().rawValue(), 1);
    QCOMPARE(result.value()[3].classId.value(), std::to_string(customId));
    QVERIFY(result.value()[3].firstEmptyRowResult);
    QCOMPARE(result.value()[3].firstEmptyRowResult.value().rawValue(), 1);
    QCOMPARE(result.value()[4].classId.value(), std::to_string(overflowId));
    QVERIFY(result.value()[4].firstEmptyRowResult);
    QCOMPARE(result.value()[4].firstEmptyRowResult.value().rawValue(), 0);

    std::vector<std::tuple<int, int, int>> streamedOrder;
    const Status streamed =
        services.workspaceStore()->rosterRepository()
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
preservesMixedParentProvenanceOutcomesInRequestOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto populated = services.classService()->create(
        QStringLiteral("Populated Availability Class")
        );
    const auto empty = services.classService()->create(
        QStringLiteral("Empty Availability Class")
        );
    const auto deletedWithOrphans = services.classService()->create(
        QStringLiteral("Deleted Availability Class With Orphans")
        );
    const auto deletedWithoutChildren = services.classService()->create(
        QStringLiteral("Deleted Availability Class Without Children")
        );
    const auto fullSibling = services.classService()->create(
        QStringLiteral("Full Availability Sibling")
        );
    QVERIFY(populated);
    QVERIFY(empty);
    QVERIFY(deletedWithOrphans);
    QVERIFY(deletedWithoutChildren);
    QVERIFY(fullSibling);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *populated,
        {QStringLiteral("English")},
        {{0, 0, QStringLiteral("Student")}}
        ));
    QVERIFY(seedSparseRoster(
        database,
        *deletedWithOrphans,
        {QStringLiteral("English")},
        {{0, 0, QStringLiteral("Orphaned student")}}
        ));
    std::vector<CellValue> fullCells;
    fullCells.reserve(25);
    for (int row = 0; row < 25; ++row)
    {
        fullCells.push_back({row, 0, QStringLiteral("Student")});
    }
    QVERIFY(seedSparseRoster(
        database,
        *fullSibling,
        {QStringLiteral("English")},
        fullCells
        ));

    QSqlQuery disableForeignKeys(database);
    QVERIFY2(
        disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
        qPrintable(disableForeignKeys.lastError().text())
        );
    QSqlQuery deleteParent(database);
    QVERIFY2(deleteParent.prepare(QStringLiteral(
        "DELETE FROM classes WHERE id=?"
        )), qPrintable(deleteParent.lastError().text()));
    for (const int classId : {*deletedWithOrphans, *deletedWithoutChildren})
    {
        deleteParent.bindValue(0, classId);
        QVERIFY2(deleteParent.exec(), qPrintable(deleteParent.lastError().text()));
        QCOMPARE(deleteParent.numRowsAffected(), 1);
    }

    const QList<int> orderedIds{
        *populated,
        *empty,
        *deletedWithOrphans,
        *deletedWithoutChildren,
        987654321,
        *fullSibling
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
    QCOMPARE(result.value().size(), std::size_t(6));

    for (std::size_t index = 0; index < typedIds.size(); ++index)
    {
        QCOMPARE(result.value()[index].classId, typedIds[index]);
    }
    QVERIFY(result.value()[0].firstEmptyRowResult);
    QCOMPARE(result.value()[0].firstEmptyRowResult.value().rawValue(), 1);
    QVERIFY(result.value()[1].firstEmptyRowResult);
    QCOMPARE(result.value()[1].firstEmptyRowResult.value().rawValue(), 0);
    for (const std::size_t missingIndex : {2U, 3U, 4U})
    {
        QVERIFY(!result.value()[missingIndex].firstEmptyRowResult);
        QCOMPARE(
            result.value()[missingIndex].firstEmptyRowResult.error().code,
            Domain::ErrorCode::NotFound
            );
        QVERIFY(result.value()[missingIndex].firstEmptyRowResult.error().recoverable);
    }
    QVERIFY(result.value()[5].firstEmptyRowResult);
    QVERIFY(result.value()[5].firstEmptyRowResult.value().isFull());
}

void NextPlatformApplicationServicesRosterReadPortTests::
rejectsMalformedAvailabilityColumnIndex()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Malformed Availability Column Index Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("English")},
        {}
        ));
    const QString malformedValue = QStringLiteral("Occupies no valid column");
    QVERIFY(seedMalformedColumnIndexCell(
        database,
        *createdClass,
        malformedValue
        ));

    QSqlQuery storedType(database);
    QVERIFY2(storedType.prepare(QStringLiteral(
        "SELECT typeof(col_index) AS storage_type FROM roster_data "
        "WHERE class_id=? AND value=?"
        )), qPrintable(storedType.lastError().text()));
    storedType.bindValue(0, *createdClass);
    storedType.bindValue(1, malformedValue);
    QVERIFY2(storedType.exec(), qPrintable(storedType.lastError().text()));
    QVERIFY(storedType.next());
    QCOMPARE(storedType.value("storage_type").toString(), QStringLiteral("text"));
    QVERIFY(!storedType.next());

    Platform::ApplicationServicesRosterAvailabilityBatchReadPort port(services);
    const Application::RosterAvailabilityBatchReadQuery availabilityQuery(port);
    const auto result = availabilityQuery.execute(
        {typedClassId(*createdClass)},
        baseColumnNames()
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    const QString diagnostic = QString::fromStdString(result.error().message);
    QVERIFY2(
        diagnostic.contains(QStringLiteral("column index"), Qt::CaseInsensitive),
        qPrintable(diagnostic)
        );
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

    const auto detailed = services.workspaceStore()->rosterRepository()
        ->loadRosterDetailed(*createdClass);
    QVERIFY(detailed);
    QVERIFY(!detailed->parentMissing);
    QVERIFY(detailed->roster.columns.isEmpty());

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
missingParentReturnsRecoverableNotFound()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto result = Application::RosterReadUseCase::execute(
        query(987654321),
        port
        );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(result.error().recoverable);
}

void NextPlatformApplicationServicesRosterReadPortTests::
deletedParentWithOrphanRosterKeepsLegacyData()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Deleted Parent Orphan Roster Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("English"), QStringLiteral("Korean")},
        {{0, 0, QStringLiteral("Legacy cell")}}
        ));

    QSqlQuery disableForeignKeys(database);
    QVERIFY2(
        disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
        qPrintable(disableForeignKeys.lastError().text())
        );
    QSqlQuery deleteParent(database);
    QVERIFY(deleteParent.prepare(QStringLiteral(
        "DELETE FROM classes WHERE id=?"
        )));
    deleteParent.addBindValue(*createdClass);
    QVERIFY2(deleteParent.exec(), qPrintable(deleteParent.lastError().text()));
    QCOMPARE(deleteParent.numRowsAffected(), 1);

    Platform::ApplicationServicesRosterReadPort port(services);
    const auto typed = Application::RosterReadUseCase::execute(
        query(*createdClass),
        port
        );
    QVERIFY(!typed);
    QCOMPARE(typed.error().code, Domain::ErrorCode::NotFound);
    QVERIFY(typed.error().recoverable);

    RosterRepository* const repository =
        services.workspaceStore()->rosterRepository();
    QVERIFY(repository);
    const auto detailed = repository->loadRosterDetailed(*createdClass);
    QVERIFY(detailed);
    QVERIFY(detailed->parentMissing);
    QCOMPARE(detailed->roster.columns,
        (QStringList{QStringLiteral("English"), QStringLiteral("Korean")}));
    QCOMPARE(detailed->roster.rows.size(), 1);
    QCOMPARE(detailed->roster.rows[0].size(), 2);
    QCOMPARE(detailed->roster.rows[0][0], QStringLiteral("Legacy cell"));
    QVERIFY(detailed->roster.rows[0][1].isEmpty());

    const auto legacy = repository->loadRoster(*createdClass);
    QVERIFY(legacy);
    QCOMPARE(legacy->columns, detailed->roster.columns);
    QCOMPARE(legacy->columnWidths, detailed->roster.columnWidths);
    QCOMPARE(legacy->rows, detailed->roster.rows);
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

    QSqlQuery dropTable(services.workspaceStore()->database());
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
rosterDataQueryErrorIsTechnical()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const auto createdClass = services.classService()->create(
        QStringLiteral("Broken Roster Data Read Test")
        );
    QVERIFY(createdClass);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(seedSparseRoster(
        database,
        *createdClass,
        {QStringLiteral("English")},
        {}
        ));
    QSqlQuery dropTable(database);
    QVERIFY2(
        dropTable.exec(QStringLiteral("DROP TABLE roster_data")),
        qPrintable(dropTable.lastError().text())
        );

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
