#include "next/application/recent_workspace_history_use_case.h"

#include <QtTest/QtTest>

#include <string>

using namespace ClassMngr::Next::Application;

class NextApplicationRecentWorkspaceHistoryUseCaseTests final : public QObject
{
    Q_OBJECT

private slots:
    void recordRemovesRawAndNormalizedAliases();
    void recordPrependsNewestAndCapsAtTen();
    void pruneRemovesBothAliasesAndClearsMatchingLastPath();
    void prunePreservesUnrelatedLastPath();
    void unicodePathsRemainOpaqueValues();
};

void NextApplicationRecentWorkspaceHistoryUseCaseTests::
recordRemovesRawAndNormalizedAliases()
{
    const RecentWorkspacePath raw("./data.tps");
    const RecentWorkspacePath normalized("/work/data.tps");
    const RecentWorkspacePath unrelated("/work/other.tps");

    RecentWorkspaceHistory current;
    current.paths = {raw, normalized, raw, unrelated, normalized};
    current.lastPath = unrelated;

    const RecentWorkspaceHistory updated =
        RecentWorkspaceHistoryUseCase::record(current, raw, normalized);

    QCOMPARE(updated.paths.size(), std::size_t{2});
    QVERIFY(updated.paths[0] == normalized);
    QVERIFY(updated.paths[1] == unrelated);
    QVERIFY(updated.lastPath.has_value());
    QVERIFY(*updated.lastPath == normalized);
}

void NextApplicationRecentWorkspaceHistoryUseCaseTests::
recordPrependsNewestAndCapsAtTen()
{
    RecentWorkspaceHistory current;
    for (std::size_t index = 0;
         index < kRecentWorkspaceHistoryMaximumEntries;
         ++index)
    {
        current.paths.emplace_back("/work/database-" + std::to_string(index));
    }

    const RecentWorkspacePath raw("./new-database.tps");
    const RecentWorkspacePath normalized("/work/new-database.tps");
    const RecentWorkspaceHistory updated =
        RecentWorkspaceHistoryUseCase::record(current, raw, normalized);

    QCOMPARE(updated.paths.size(), kRecentWorkspaceHistoryMaximumEntries);
    QVERIFY(updated.paths.front() == normalized);
    for (std::size_t index = 0; index + 1 < updated.paths.size(); ++index)
    {
        QVERIFY(
            updated.paths[index + 1]
            == RecentWorkspacePath("/work/database-" + std::to_string(index))
            );
    }
    QVERIFY(updated.lastPath.has_value());
    QVERIFY(*updated.lastPath == normalized);
}

void NextApplicationRecentWorkspaceHistoryUseCaseTests::
pruneRemovesBothAliasesAndClearsMatchingLastPath()
{
    const RecentWorkspacePath raw("./database.tps");
    const RecentWorkspacePath normalized("/work/database.tps");
    const RecentWorkspacePath unrelated("/work/other.tps");

    RecentWorkspaceHistory current;
    current.paths = {raw, unrelated, normalized, raw};
    current.lastPath = normalized;

    const RecentWorkspaceHistory updated =
        RecentWorkspaceHistoryUseCase::prune(current, raw, normalized);

    QCOMPARE(updated.paths.size(), std::size_t{1});
    QVERIFY(updated.paths.front() == unrelated);
    QVERIFY(!updated.lastPath.has_value());

    current.lastPath = raw;
    const RecentWorkspaceHistory rawLastPathUpdated =
        RecentWorkspaceHistoryUseCase::prune(current, raw, normalized);
    QVERIFY(!rawLastPathUpdated.lastPath.has_value());
}

void NextApplicationRecentWorkspaceHistoryUseCaseTests::
prunePreservesUnrelatedLastPath()
{
    const RecentWorkspacePath raw("./database.tps");
    const RecentWorkspacePath normalized("/work/database.tps");
    const RecentWorkspacePath unrelated("/work/other.tps");

    RecentWorkspaceHistory current;
    current.paths = {raw, unrelated, normalized};
    current.lastPath = unrelated;

    const RecentWorkspaceHistory updated =
        RecentWorkspaceHistoryUseCase::prune(current, raw, normalized);

    QCOMPARE(updated.paths.size(), std::size_t{1});
    QVERIFY(updated.paths.front() == unrelated);
    QVERIFY(updated.lastPath.has_value());
    QVERIFY(*updated.lastPath == unrelated);
}

void NextApplicationRecentWorkspaceHistoryUseCaseTests::
unicodePathsRemainOpaqueValues()
{
    const RecentWorkspacePath raw(
        std::string("./") + "\xED\x95\x99\xEC\x83\x9D/data.tps"
        );
    const RecentWorkspacePath normalized(
        std::string("/profile/") + "\xED\x95\x99\xEC\x83\x9D/data.tps"
        );
    const RecentWorkspacePath unrelated(
        std::string("/profile/") + "\xEA\xB0\x95\xEC\x9D\x98/other.tps"
        );

    RecentWorkspaceHistory current;
    current.paths = {raw, unrelated, normalized};

    const RecentWorkspaceHistory recorded =
        RecentWorkspaceHistoryUseCase::record(current, raw, normalized);

    QCOMPARE(recorded.paths.size(), std::size_t{2});
    QVERIFY(recorded.paths[0] == normalized);
    QVERIFY(recorded.paths[1] == unrelated);
    QVERIFY(recorded.lastPath.has_value());
    QVERIFY(*recorded.lastPath == normalized);

    const RecentWorkspaceHistory pruned =
        RecentWorkspaceHistoryUseCase::prune(recorded, raw, normalized);
    QCOMPARE(pruned.paths.size(), std::size_t{1});
    QVERIFY(pruned.paths.front() == unrelated);
    QVERIFY(!pruned.lastPath.has_value());
}

QTEST_APPLESS_MAIN(NextApplicationRecentWorkspaceHistoryUseCaseTests)

#include "next_application_recent_workspace_history_use_case_tests.moc"
