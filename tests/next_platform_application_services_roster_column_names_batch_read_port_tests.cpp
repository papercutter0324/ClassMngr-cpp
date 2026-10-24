#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "next/application/roster_column_names_batch_read_query.h"
#include "next/platform/application_services_roster_column_names_batch_read_port.h"

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
        QStringLiteral("roster-column-names-batch-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Domain::ClassId typedClassId(const int classId)
{
    return *Domain::ClassId::fromString(std::to_string(classId));
}

std::vector<Domain::ClassId> typedClassIds(const QList<int>& classIds)
{
    std::vector<Domain::ClassId> result;
    result.reserve(static_cast<std::size_t>(classIds.size()));
    for (const int classId : classIds)
    {
        result.push_back(typedClassId(classId));
    }
    return result;
}

bool insertColumns(
    QSqlDatabase database,
    const int classId,
    const QList<QPair<QString, int>>& columns
    )
{
    QSqlQuery insert(database);
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (?, ?, ?, ?)"
            )))
    {
        return false;
    }

    for (const auto& column : columns)
    {
        insert.bindValue(0, classId);
        insert.bindValue(1, column.first);
        insert.bindValue(2, column.second);
        insert.bindValue(3, 100 + column.second);
        if (!insert.exec())
        {
            return false;
        }
    }
    return true;
}

int createClass(ApplicationServices& services, const QString& name)
{
    const auto created = services.classService()->create(name);
    return created.value_or(-1);
}

}

class NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests
    final : public QObject
{
    Q_OBJECT

private slots:
    void mapsOneOrderedBatchOfFourClassesAndReadsNamesOnly();
    void returnsSuccessfulEmptySlotsForMissingRosters();
    void retainsParentAbsencePerEntryAndPreservesSiblingColumns();
    void doesNotApplyOutputColumnLimitToDiscovery();
    void emptyAndInvalidRequestsAvoidSessionAccess();
    void rosterColumnsStatementFailureIsStructured();
};

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
mapsOneOrderedBatchOfFourClassesAndReadsNamesOnly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int zuluId = createClass(services, QStringLiteral("Zulu"));
    const int alphaId = createClass(services, QStringLiteral("Alpha"));
    const int betaId = createClass(services, QStringLiteral("Beta"));
    const int emptyId = createClass(services, QStringLiteral("Empty"));
    QVERIFY(zuluId > 0);
    QVERIFY(alphaId > 0);
    QVERIFY(betaId > 0);
    QVERIFY(emptyId > 0);

    QSqlDatabase database = services.workspaceStore()->database();
    QVERIFY(insertColumns(database, zuluId, {
        {QStringLiteral("Last"), 2},
        {QStringLiteral("First"), 0},
        {QStringLiteral("Middle"), 1}
    }));
    QVERIFY(insertColumns(database, alphaId, {
        {QStringLiteral("Korean"), 0},
        {QStringLiteral("Memo Books"), 1}
    }));
    QVERIFY(insertColumns(database, betaId, {
        {QStringLiteral("Beta Note"), 0}
    }));

    // Removing roster_data proves this discovery path does not materialize
    // the full roster snapshot or its student rows.
    QSqlQuery dropRosterData(database);
    QVERIFY2(dropRosterData.exec(QStringLiteral("DROP TABLE roster_data")),
        qPrintable(dropRosterData.lastError().text()));

    const QList<int> orderedIds{zuluId, emptyId, alphaId, betaId};
    const auto ids = typedClassIds(orderedIds);
    Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(services);
    const Application::RosterColumnNamesBatchReadQuery query(port);
    const auto result = query.execute(ids);

    QVERIFY2(result, result ? "" : qPrintable(
        QString::fromStdString(result.error().message)));
    QCOMPARE(result.value().size(), std::size_t(4));
    QCOMPARE(result.value().at(0).classId.value(), std::to_string(zuluId));
    QVERIFY(result.value().at(0).columnsResult.hasValue());
    QVERIFY(result.value().at(0).columnsResult.value() ==
        (std::vector<std::u16string>{u"First", u"Middle", u"Last"}));
    QCOMPARE(result.value().at(1).classId.value(), std::to_string(emptyId));
    QVERIFY(result.value().at(1).columnsResult.hasValue());
    QVERIFY(result.value().at(1).columnsResult.value().empty());
    QCOMPARE(result.value().at(2).classId.value(), std::to_string(alphaId));
    QVERIFY(result.value().at(2).columnsResult.hasValue());
    QVERIFY(result.value().at(2).columnsResult.value() ==
        (std::vector<std::u16string>{u"Korean", u"Memo Books"}));
    QCOMPARE(result.value().at(3).classId.value(), std::to_string(betaId));
    QVERIFY(result.value().at(3).columnsResult.hasValue());
    QVERIFY(result.value().at(3).columnsResult.value() ==
        (std::vector<std::u16string>{u"Beta Note"}));
}

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
returnsSuccessfulEmptySlotsForMissingRosters()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int withRosterId = createClass(
        services,
        QStringLiteral("With Roster")
        );
    const int withoutRosterId = createClass(
        services,
        QStringLiteral("Without Roster")
        );
    QVERIFY(withRosterId > 0);
    QVERIFY(withoutRosterId > 0);
    QVERIFY(insertColumns(
        services.workspaceStore()->database(),
        withRosterId,
        {{QStringLiteral("Notes"), 0}}
        ));

    Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(services);
    const auto result = port.readRosterColumnNames(
        typedClassIds({withoutRosterId, withRosterId})
        );

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(2));
    QCOMPARE(result.value().at(0).classId.value(), std::to_string(withoutRosterId));
    QVERIFY(result.value().at(0).columnsResult.hasValue());
    QVERIFY(result.value().at(0).columnsResult.value().empty());
    QCOMPARE(result.value().at(1).classId.value(), std::to_string(withRosterId));
    QVERIFY(result.value().at(1).columnsResult.hasValue());
    QVERIFY(result.value().at(1).columnsResult.value() ==
            std::vector<std::u16string>{u"Notes"});
}

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
retainsParentAbsencePerEntryAndPreservesSiblingColumns()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));

    const int populatedId = createClass(
        services,
        QStringLiteral("Populated")
        );
    const int presentEmptyId = createClass(
        services,
        QStringLiteral("Present Empty")
        );
    const int deletedEmptyId = createClass(
        services,
        QStringLiteral("Deleted Empty")
        );
    const int orphanParentId = createClass(
        services,
        QStringLiteral("Deleted With Orphan")
        );
    const int siblingId = createClass(
        services,
        QStringLiteral("Sibling")
        );
    QVERIFY(populatedId > 0);
    QVERIFY(presentEmptyId > 0);
    QVERIFY(deletedEmptyId > 0);
    QVERIFY(orphanParentId > 0);
    QVERIFY(siblingId > 0);
    QVERIFY(insertColumns(services.workspaceStore()->database(), populatedId, {
        {QStringLiteral("First"), 0},
        {QString::fromUtf8("\xED\x95\x99\xEC\x83\x9D"), 1}
    }));
    QVERIFY(insertColumns(services.workspaceStore()->database(), orphanParentId, {
        {QStringLiteral("Orphan Secret"), 0}
    }));
    QVERIFY(insertColumns(services.workspaceStore()->database(), siblingId, {
        {QStringLiteral("Sibling Note"), 0}
    }));

    QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery disableForeignKeys(database);
    QVERIFY2(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
        qPrintable(disableForeignKeys.lastError().text()));
    disableForeignKeys.finish();
    QSqlQuery deleteParents(database);
    QVERIFY(deleteParents.prepare(QStringLiteral("DELETE FROM classes WHERE id=?")));
    deleteParents.addBindValue(deletedEmptyId);
    QVERIFY2(deleteParents.exec(), qPrintable(deleteParents.lastError().text()));
    deleteParents.finish();
    deleteParents.bindValue(0, orphanParentId);
    QVERIFY2(deleteParents.exec(), qPrintable(deleteParents.lastError().text()));
    deleteParents.finish();
    QSqlQuery enableForeignKeys(database);
    QVERIFY2(enableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=ON")),
        qPrintable(enableForeignKeys.lastError().text()));
    enableForeignKeys.finish();

    const int neverPresentId = 987654321;
    const QList<int> orderedIds{
        populatedId,
        presentEmptyId,
        deletedEmptyId,
        orphanParentId,
        neverPresentId,
        siblingId
    };
    const Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(services);
    const Application::RosterColumnNamesBatchReadQuery readQuery(port);
    const auto result = readQuery.execute(typedClassIds(orderedIds));

    QVERIFY2(result, result ? "" : qPrintable(
        QString::fromStdString(result.error().message)));
    QCOMPARE(result.value().size(), std::size_t(6));
    for (std::size_t index = 0; index < orderedIds.size(); ++index)
    {
        QCOMPARE(
            result.value().at(index).classId.value(),
            std::to_string(orderedIds.at(static_cast<qsizetype>(index)))
            );
    }

    QVERIFY(result.value().at(0).columnsResult.hasValue());
    QVERIFY(result.value().at(0).columnsResult.value() ==
        (std::vector<std::u16string>{u"First", u"\uD559\uC0DD"}));
    QVERIFY(result.value().at(1).columnsResult.hasValue());
    QVERIFY(result.value().at(1).columnsResult.value().empty());
    for (const std::size_t missingIndex : {2U, 3U, 4U})
    {
        QVERIFY(!result.value().at(missingIndex).columnsResult.hasValue());
        QCOMPARE(
            result.value().at(missingIndex).columnsResult.error().code,
            Domain::ErrorCode::NotFound
            );
        QVERIFY(result.value().at(missingIndex).columnsResult.error().recoverable);
    }
    QVERIFY(result.value().at(3).columnsResult.error().message.find(
                "not found") != std::string::npos);
    QVERIFY(result.value().at(5).columnsResult.hasValue());
    QVERIFY(result.value().at(5).columnsResult.value() ==
        (std::vector<std::u16string>{u"Sibling Note"}));
}

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
doesNotApplyOutputColumnLimitToDiscovery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(
        services,
        QStringLiteral("Roster With Many Columns")
        );
    QVERIFY(classId > 0);

    QList<QPair<QString, int>> columns;
    columns.reserve(129);
    for (int index = 0; index < 129; ++index)
    {
        columns.append(qMakePair(
            QStringLiteral("Column %1").arg(index),
            index
            ));
    }
    QVERIFY(insertColumns(
        services.workspaceStore()->database(),
        classId,
        columns
        ));

    Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(services);
    const auto result = port.readRosterColumnNames({typedClassId(classId)});

    QVERIFY(result);
    QCOMPARE(result.value().size(), std::size_t(1));
    QVERIFY(result.value().at(0).columnsResult.hasValue());
    QCOMPARE(result.value().at(0).columnsResult.value().size(), std::size_t(129));
    QCOMPARE(result.value().at(0).columnsResult.value().front(), std::u16string(u"Column 0"));
    QCOMPARE(result.value().at(0).columnsResult.value().back(), std::u16string(u"Column 128"));
}

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
emptyAndInvalidRequestsAvoidSessionAccess()
{
    Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(
        static_cast<ApplicationServices*>(nullptr)
        );

    const auto empty = port.readRosterColumnNames({});
    QVERIFY(empty);
    QVERIFY(empty.value().empty());

    const auto invalid = port.readRosterColumnNames({
        *Domain::ClassId::fromString("01")
    });
    QVERIFY(!invalid);
    QCOMPARE(invalid.error().code, Domain::ErrorCode::InvalidInput);
}

void NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests::
rosterColumnsStatementFailureIsStructured()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const int classId = createClass(services, QStringLiteral("Broken Roster"));
    QVERIFY(classId > 0);

    QSqlQuery dropRosterColumns(services.workspaceStore()->database());
    QVERIFY2(dropRosterColumns.exec(QStringLiteral("DROP TABLE roster_columns")),
        qPrintable(dropRosterColumns.lastError().text()));

    Platform::ApplicationServicesRosterColumnNamesBatchReadPort port(services);
    const Application::RosterColumnNamesBatchReadQuery query(port);
    const auto result = query.execute({typedClassId(classId)});

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
    QVERIFY(!result.error().message.empty());
    QVERIFY(!result.error().recoverable);
}

QTEST_MAIN(NextPlatformApplicationServicesRosterColumnNamesBatchReadPortTests)

#include "next_platform_application_services_roster_column_names_batch_read_port_tests.moc"
