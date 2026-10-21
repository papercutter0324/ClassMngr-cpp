#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/roster_repository.h"
#include "next/application/roster_row_removal.h"
#include "next/platform/application_services_roster_save_port.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

namespace
{
Application::RosterSnapshot visibleRoster()
{
    Application::RosterSnapshot snapshot;
    for (const auto& column : Roster::BaseColumns)
        snapshot.columns.push_back(column.toStdU16String());
    snapshot.columns.push_back(u"Advisory");
    snapshot.columnWidths.resize(7, Application::RosterSnapshotColumnWidth{160});
    snapshot.rows.resize(25, std::vector<std::u16string>(7));
    snapshot.rows[0] = {u"Alice", u"김민지", u"A", u"", u"", u"B", u"edited note"};
    return snapshot;
}

bool cell(ApplicationServices& services, int id, qlonglong row, const QVariant& column, QString value)
{
    QSqlQuery query(services.workspaceStore()->database());
    query.prepare(QStringLiteral("INSERT INTO roster_data VALUES (?, ?, ?, ?)"));
    query.addBindValue(id);
    query.addBindValue(row);
    query.addBindValue(column);
    query.addBindValue(value);
    return query.exec();
}

QList<QVariantList> rawState(ApplicationServices& services)
{
    QList<QVariantList> rows;
    QSqlQuery query(services.workspaceStore()->database());
    if (!query.exec(QStringLiteral(
            "SELECT 'column', id, class_id, name, position, width FROM roster_columns "
            "UNION ALL SELECT 'cell', NULL, class_id, row_index, col_index, value "
            "FROM roster_data ORDER BY 1, 2, 3, 4, 5")))
        qFatal("Raw state query failed");
    while (query.next())
        rows.append({query.value(0), query.value(1), query.value(2),
                     query.value(3), query.value(4), query.value(5)});
    return rows;
}
}

class NextPlatformRosterWindowSaveTests final : public QObject
{
    Q_OBJECT
private slots:
    void sparseTailRemapsCanonicalColumnsAndConfirmedRemoval();
    void ambiguousOrUnknownTailFailsWithoutWrites_data();
    void ambiguousOrUnknownTailFailsWithoutWrites();
    void row24RemovalKeepsAbsoluteTailPositions();
    void verySparseTailStaysSparseAndWindowInputsAreChecked();
};

void NextPlatformRosterWindowSaveTests::sparseTailRemapsCanonicalColumnsAndConfirmedRemoval()
{
    QTemporaryDir directory;
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath("window.db")));
    const auto id = services.classService()->create(QStringLiteral("Window"));
    QVERIFY(id);
    auto* repository = services.workspaceStore()->rosterRepository();
    Roster original;
    original.columns = {"Advisory", " autumn ", "KOREAN", "ENGLISH", "Winter",
                        "Speech Contest", "Summer", "Remove me"};
    original.columnWidths = {101, 102, 103, 104, 105, 106, 107, 108};
    QVERIFY(repository->saveRoster(*id, original));
    QVERIFY(cell(services, *id, 25, 0, "tail advisory"));
    QVERIFY(cell(services, *id, 25, 1, "tail fall"));
    QVERIFY(cell(services, *id, 25, 3, "tail english"));
    QVERIFY(cell(services, *id, 37, 2, "tail korean"));
    QVERIFY(cell(services, *id, 37, 7, "confirmed deletion"));

    auto snapshot = visibleRoster();
    snapshot.columns.push_back(u"Parent Contact");
    snapshot.columnWidths.push_back(Application::RosterSnapshotColumnWidth{222});
    snapshot.rows[0].push_back(u"new cell");
    const auto classId = *Domain::ClassId::fromString(std::to_string(*id));
    Platform::ApplicationServicesRosterSavePort port(services);
    const auto before = rawState(services);
    const auto unauthorized = port.saveRoster({.classId = classId, .roster = snapshot});
    QVERIFY(!unauthorized);
    QCOMPARE(rawState(services), before);
    const auto saved = port.saveRoster({.classId = classId, .roster = snapshot,
        .removedCustomColumns = {u" REMOVE   me "}});
    QVERIFY2(saved, saved ? "" : saved.error().message.c_str());
    const auto full = repository->loadRoster(*id);
    QVERIFY(full);
    QCOMPARE(full->rows.size(), 38);
    QCOMPARE(full->columns[5], QStringLiteral("Fall"));
    QCOMPARE(full->columns[6], QStringLiteral("Advisory"));
    QCOMPARE(full->columns[7], QStringLiteral("Parent Contact"));
    QCOMPARE(full->columnWidths[7], 222);
    QCOMPARE(full->rows[0][6], QStringLiteral("edited note"));
    QCOMPARE(full->rows[0][7], QStringLiteral("new cell"));
    QCOMPARE(full->rows[25][0], QStringLiteral("tail english"));
    QCOMPARE(full->rows[25][5], QStringLiteral("tail fall"));
    QCOMPARE(full->rows[25][6], QStringLiteral("tail advisory"));
    QCOMPARE(full->rows[37][1], QStringLiteral("tail korean"));
    QVERIFY(full->rows[37][7].isEmpty());
    QVERIFY(full->rows[24][0].isEmpty());
    QVERIFY(full->rows[26][0].isEmpty());
    // A second save edits only the visible window and widths again.
    snapshot.rows[0][6] = u"second edit";
    snapshot.columnWidths[6] = Application::RosterSnapshotColumnWidth{244};
    QVERIFY(port.saveRoster({.classId = classId, .roster = snapshot}));
    const auto again = repository->loadRoster(*id);
    QVERIFY(again);
    QCOMPARE(again->rows[25], full->rows[25]);
    QCOMPARE(again->rows[37], full->rows[37]);
    QCOMPARE(again->rows[0][6], QStringLiteral("second edit"));
    QCOMPARE(again->columnWidths[6], 244);
}

void NextPlatformRosterWindowSaveTests::ambiguousOrUnknownTailFailsWithoutWrites_data()
{
    QTest::addColumn<QStringList>("headers");
    QTest::addColumn<QVariant>("tailColumn");
    QTest::addColumn<QStringList>("removed");
    QTest::newRow("alias collapse") << QStringList{"English", "Autumn", "Fall"}
        << QVariant(1) << QStringList{};
    QTest::newRow("case collapse") << QStringList{"English", "Notes", " notes "}
        << QVariant(2) << QStringList{};
    QTest::newRow("ambiguous removal") << QStringList{"English", "Notes", "NOTES"}
        << QVariant(1) << QStringList{"Notes"};
    QTest::newRow("unknown index") << QStringList{"English"} << QVariant(12) << QStringList{};
    QTest::newRow("malformed index") << QStringList{"English"}
        << QVariant(QStringLiteral("legacy-index")) << QStringList{};
    QTest::newRow("empty header") << QStringList{"English", " "} << QVariant(1) << QStringList{};
    QTest::newRow("unconfirmed removal") << QStringList{"English", "Hidden note"}
        << QVariant(1) << QStringList{};
    QTest::newRow("required removal") << QStringList{"English", "Korean"}
        << QVariant(1) << QStringList{"Korean"};
}

void NextPlatformRosterWindowSaveTests::ambiguousOrUnknownTailFailsWithoutWrites()
{
    QFETCH(QStringList, headers);
    QFETCH(QVariant, tailColumn);
    QFETCH(QStringList, removed);
    QTemporaryDir directory;
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath("invalid-window.db")));
    const auto id = services.classService()->create(QStringLiteral("Ambiguous"));
    QVERIFY(id);
    auto* repository = services.workspaceStore()->rosterRepository();
    Roster original;
    original.columns = headers;
    QVERIFY(repository->saveRoster(*id, original));
    QVERIFY(cell(services, *id, 37, tailColumn, "original tail bytes"));
    const auto before = rawState(services);
    Roster replacement;
    replacement.columns = Roster::BaseColumns;
    replacement.columnWidths.fill(180, replacement.columns.size());
    const auto saved = repository->saveRosterWindowsWithParentCheck({{
        .classId = *id, .roster = replacement, .removedCustomColumns = removed}});
    QVERIFY(!saved);
    QCOMPARE(rawState(services), before);
}

void NextPlatformRosterWindowSaveTests::row24RemovalKeepsAbsoluteTailPositions()
{
    QTemporaryDir directory;
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath("remove-row.db")));
    const auto id = services.classService()->create(QStringLiteral("Remove row"));
    QVERIFY(id);
    auto* repository = services.workspaceStore()->rosterRepository();
    Roster original;
    original.columns = Roster::BaseColumns + QStringList{"Advisory"};
    QVERIFY(repository->saveRoster(*id, original));
    QVERIFY(cell(services, *id, 25, 6, "row 25"));
    QVERIFY(cell(services, *id, 37, 6, "row 37"));
    auto snapshot = visibleRoster();
    snapshot.rows[24] = {u"Last Student", u"박지민", u"", u"", u"", u"", u"remove"};
    const auto removed = Application::removeRosterRow(snapshot,
        Application::RosterRowRemovalIndex{24});
    QVERIFY(std::holds_alternative<Application::RosterSnapshot>(removed));
    Platform::ApplicationServicesRosterSavePort port(services);
    QVERIFY(port.saveRoster({
        .classId = *Domain::ClassId::fromString(std::to_string(*id)),
        .roster = std::get<Application::RosterSnapshot>(removed)}));
    const auto full = repository->loadRoster(*id);
    QVERIFY(full);
    QCOMPARE(full->rows.size(), 38);
    QVERIFY(full->rows[24][6].isEmpty());
    QCOMPARE(full->rows[25][6], QStringLiteral("row 25"));
    QCOMPARE(full->rows[37][6], QStringLiteral("row 37"));
}

void NextPlatformRosterWindowSaveTests::verySparseTailStaysSparseAndWindowInputsAreChecked()
{
    QTemporaryDir directory;
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath("sparse.db")));
    const auto id = services.classService()->create(QStringLiteral("Sparse"));
    QVERIFY(id);
    auto* repository = services.workspaceStore()->rosterRepository();
    Roster original;
    original.columns = Roster::BaseColumns;
    // Exceed the output-specific 128-column cap: editor saves keep all columns.
    for (int index = 0; index < 150; ++index)
        original.columns.append(QStringLiteral("Dynamic %1").arg(index));
    QVERIFY(repository->saveRoster(*id, original));
    QVERIFY(cell(services, *id, 1'000'000'000, original.columns.size() - 1, "sparse tail"));
    Platform::ApplicationServicesRosterSavePort port(services);
    auto snapshot = visibleRoster();
    for (int index = 0; index < 150; ++index)
    {
        snapshot.columns.push_back(QStringLiteral("Dynamic %1").arg(index).toStdU16String());
        snapshot.columnWidths.push_back(Application::RosterSnapshotColumnWidth{120});
    }
    const auto classId = *Domain::ClassId::fromString(std::to_string(*id));
    QVERIFY(port.saveRoster({.classId = classId, .roster = snapshot}));
    QSqlQuery query(services.workspaceStore()->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT row_index, col_index, value FROM roster_data WHERE row_index>=25")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toLongLong(), 1'000'000'000LL);
    QCOMPARE(query.value(1).toInt(), 156);
    QCOMPARE(query.value(2).toString(), QStringLiteral("sparse tail"));
    QVERIFY(!query.next());
    const auto before = rawState(services);
    snapshot.rows.resize(26);
    QVERIFY(!port.saveRoster({.classId = classId, .roster = snapshot}));
    QCOMPARE(rawState(services), before);
    // Window reads are still bounded even after saving a huge absolute index.
    const auto window = repository->loadRosterWindowDetailed(*id, 25);
    QVERIFY(window);
    QVERIFY(window->hasRowsOutsideWindow);
    QVERIFY(window->roster.rows.size() <= 25);
}

QTEST_MAIN(NextPlatformRosterWindowSaveTests)
#include "next_platform_roster_window_save_tests.moc"
