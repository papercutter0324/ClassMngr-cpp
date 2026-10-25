#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/roster_repository.h"
#include "next/application/roster_row_removal.h"
#include "next/platform/application_services_roster_save_port.h"

#include <QSqlDriver>
#include <QSqlError>
#include <QSqlField>
#include <QSqlRecord>
#include <QSqlResult>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;

namespace
{
// A real SQLite query may materialize a sort before the first row. This driver
// guarantees one delivered row followed by a terminal fetch failure.
class RosterCursorFailureResult final : public QSqlResult
{
public:
    explicit RosterCursorFailureResult(const QSqlDriver* driver) : QSqlResult(driver) {}
protected:
    bool prepare(const QString& query) override
    {
        setQuery(query);
        return true;
    }
    bool exec() override
    {
        setActive(true);
        setSelect(true);
        setAt(QSql::BeforeFirstRow);
        setLastError({});
        return true;
    }
    bool reset(const QString& query) override { return prepare(query) && exec(); }
    bool fetch(const int index) override
    {
        if (index == 0)
        {
            setAt(0);
            return true;
        }
        setAt(QSql::AfterLastRow);
        setLastError(QSqlError(QStringLiteral("Roster cursor"),
            QStringLiteral("injected roster cursor failure"), QSqlError::StatementError));
        return false;
    }
    bool fetchFirst() override { return fetch(0); }
    bool fetchLast() override { return fetch(1); }
    QVariant data(const int index) override
    {
        const QVariantList values{1, 0, QStringLiteral("integer"), 0,
            QStringLiteral("integer"), QStringLiteral("Visible")};
        return at() == 0 && index >= 0 && index < values.size()
            ? values[index] : QVariant{};
    }
    bool isNull(const int index) override { return data(index).isNull(); }
    int size() override { return -1; }
    int numRowsAffected() override { return 0; }
    QSqlRecord record() const override
    {
        QSqlRecord fields;
        for (const auto* name : {"class_id", "row_index", "row_index_storage_type",
                                "col_index", "col_index_storage_type", "value"})
            fields.append(QSqlField(QString::fromLatin1(name)));
        return fields;
    }
};

class RosterCursorFailureDriver final : public QSqlDriver
{
public:
    bool hasFeature(const DriverFeature feature) const override
    {
        return feature == PreparedQueries;
    }
    bool open(const QString&, const QString&, const QString&, const QString&,
              int, const QString&) override
    {
        setOpen(true);
        setOpenError(false);
        return true;
    }
    void close() override { setOpen(false); }
    QSqlResult* createResult() const override { return new RosterCursorFailureResult(this); }
};

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
    void structuredSaveErrorsAndCheckedRollback();
    void repositoryInputErrorsAreTyped();
    void availabilityVisitorReportsTerminalCursorFailure();
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
    QCOMPARE(unauthorized.error().code, Domain::ErrorCode::Validation);
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
    QCOMPARE(saved.error().code, removed.contains(QStringLiteral("Korean"))
        ? RepositoryErrorCode::InvalidInput : RepositoryErrorCode::Validation);
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


void NextPlatformRosterWindowSaveTests::structuredSaveErrorsAndCheckedRollback()
{
    QTemporaryDir directory;
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath("structured-errors.db")));
    const auto id = services.classService()->create(QStringLiteral("Structured errors"));
    QVERIFY(id);
    auto* repository = services.workspaceStore()->rosterRepository();
    QVERIFY(repository);
    Roster original;
    original.columns = {QStringLiteral("English")};
    original.columnWidths = {160};
    original.rows = {{QStringLiteral("Original")}};
    QVERIFY(repository->saveRoster(*id, original));
    const auto before = rawState(services);

    const auto invalidId = repository->saveRoster(0, original);
    QVERIFY(!invalidId);
    QCOMPARE(invalidId.error().code, RepositoryErrorCode::InvalidInput);
    QCOMPARE(invalidId.error().diagnostic,
        QStringLiteral("Saving roster failed: invalid class id 0."));
    const auto missing = repository->saveRosterWithParentCheck(*id + 1, original);
    QVERIFY(!missing);
    QCOMPARE(missing.error().code, RepositoryErrorCode::NotFound);
    QVERIFY(missing.error().diagnostic.contains(QStringLiteral("was not found")));
    const auto emptyWindows = repository->saveRosterWindowsWithParentCheck({});
    QVERIFY(!emptyWindows);
    QCOMPARE(emptyWindows.error().code, RepositoryErrorCode::InvalidInput);
    QVERIFY(repository->saveRosters({}));

    Roster duplicateHeaders = original;
    duplicateHeaders.columns.append(QStringLiteral(" english "));
    duplicateHeaders.columnWidths.append(160);
    const auto invalidMapping = repository->saveRosterWindowsWithParentCheck({{
        .classId = *id, .roster = duplicateHeaders}});
    QVERIFY(!invalidMapping);
    QCOMPARE(invalidMapping.error().code, RepositoryErrorCode::InvalidInput);
    QCOMPARE(rawState(services), before);

    Roster replacement = original;
    replacement.rows[0][0] = QStringLiteral("Replacement");
    const auto save = [&](const int mode) -> RepositoryStatus
    {
        switch (mode)
        {
        case 0: return repository->saveRoster(*id, replacement);
        case 1: return repository->saveRosterWithParentCheck(*id, replacement);
        case 2: return repository->saveRosters({{*id, replacement}});
        default: return repository->saveRosterWindowsWithParentCheck({{
            .classId = *id, .roster = replacement}});
        }
    };

    QSqlDatabase database = services.workspaceStore()->database();
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys=ON")));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE deferred_roster_check (class_id INTEGER, "
        "FOREIGN KEY(class_id) REFERENCES classes(id) DEFERRABLE INITIALLY DEFERRED)")));
    for (const QString& failure : {QStringLiteral("ABORT"), QStringLiteral("ROLLBACK"),
                                  QStringLiteral("COMMIT")})
    {
        const QString trigger = failure == QStringLiteral("COMMIT")
            ? QStringLiteral("CREATE TRIGGER reject_roster_write AFTER INSERT ON roster_columns "
                "BEGIN INSERT INTO deferred_roster_check VALUES (2147483647); END")
            : QStringLiteral("CREATE TRIGGER reject_roster_write BEFORE INSERT ON roster_columns "
                "BEGIN SELECT RAISE(%1, 'injected roster write failure'); END").arg(failure);
        QVERIFY2(query.exec(trigger), qPrintable(query.lastError().text()));
        for (const int mode : {0, 1, 2, 3})
        {
            const auto failed = save(mode);
            QVERIFY(!failed);
            QCOMPARE(failed.error().code, RepositoryErrorCode::Technical);
            QVERIFY2(failed.error().diagnostic.contains(failure == QStringLiteral("COMMIT")
                    ? QStringLiteral("FOREIGN KEY") : QStringLiteral("injected roster write failure")),
                qPrintable(failed.error().diagnostic));
            QCOMPARE(failed.error().diagnostic.contains(QStringLiteral("Rolling back")),
                failure == QStringLiteral("ROLLBACK"));
            QCOMPARE(rawState(services), before);
            QVERIFY(database.transaction());
            QVERIFY(database.rollback());
            QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM deferred_roster_check")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 0);
            query.finish();
        }
        QVERIFY(query.exec(QStringLiteral("DROP TRIGGER reject_roster_write")));
    }
}


void NextPlatformRosterWindowSaveTests::repositoryInputErrorsAreTyped()
{
    QTemporaryDir directory;
    ApplicationServices services;
    QVERIFY(services.openDatabase(directory.filePath("input-errors.db")));
    auto* repository = services.workspaceStore()->rosterRepository();
    QVERIFY(repository);
    const auto invalid = [](const auto& result)
    {
        QVERIFY(!result);
        QCOMPARE(result.error().code, RepositoryErrorCode::InvalidInput);
        QVERIFY(!result.error().diagnostic.isEmpty());
        QVERIFY(!result.error().recoverableOverride.has_value());
    };
    invalid(repository->saveRosterWithParentCheck(0, {}));
    invalid(repository->saveRosters({{0, {}}}));
    invalid(repository->loadRoster(0));
    invalid(repository->loadRosterDetailed(0));
    invalid(repository->loadRosterWindowDetailed(1, 0));
    invalid(repository->loadSpeakingEvaluationRosterNames(0));
    invalid(repository->loadSpeakingEvaluationRosterNamesDetailed(0));
    invalid(repository->loadFullRosters({1, 1}));
    invalid(repository->loadRostersForTemplatePrint({0}));
    invalid(repository->loadRosterColumnNamesForClasses({0}));
    invalid(repository->loadRosterColumnNamesForClassesWithParentProvenance({0}));
    invalid(repository->loadMyClassesStudentCountRecords({0}));
    invalid(repository->loadSubPrepStudentCountRecords({0}));
    invalid(repository->forEachRosterDataCellForClasses({1}, 25, {}));
    invalid(repository->forEachRosterDataCellForExistingClasses({1}, 25, {}));
    invalid(repository->loadRosterForOutput(0, {}, 0, 0, 0));
    invalid(repository->loadRosterForOutput(1, {}, kRosterRepositoryOutputMaxRows + 1, 0, 0));
    invalid(repository->loadRosterForOutput(1, {QStringLiteral(" ")}, 0, 0, 0));
    invalid(repository->getRosterStudentCount(0));
    // Empty requests retain their existing early success, even without a callback.
    QVERIFY(repository->forEachRosterDataCellForClasses({}, 0, {}));
    QVERIFY(repository->forEachRosterDataCellForExistingClasses({}, 0, {}));
}


void NextPlatformRosterWindowSaveTests::availabilityVisitorReportsTerminalCursorFailure()
{
    const QString connection = QStringLiteral("roster-terminal-cursor-failure");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(new RosterCursorFailureDriver, connection);
        QVERIFY(database.open());
        RosterRepository repository(database);
        for (const bool parentRooted : {false, true})
        {
            int delivered = 0;
            const auto consume = [&](int classId, int row, int column, const QString& value)
            {
                ++delivered;
                QCOMPARE(classId, 1);
                QCOMPARE(row, 0);
                QCOMPARE(column, 0);
                QCOMPARE(value, QStringLiteral("Visible"));
            };
            const auto failed = parentRooted
                ? repository.forEachRosterDataCellForExistingClasses({1}, 25, consume)
                : repository.forEachRosterDataCellForClasses({1}, 25, consume);
            QCOMPARE(delivered, 1);
            QVERIFY(!failed);
            QCOMPARE(failed.error().code, RepositoryErrorCode::Technical);
            QVERIFY(failed.error().diagnostic.contains(QStringLiteral("injected roster cursor failure")));
        }
    }
    QSqlDatabase::removeDatabase(connection);
}

QTEST_MAIN(NextPlatformRosterWindowSaveTests)
#include "next_platform_roster_window_save_tests.moc"
