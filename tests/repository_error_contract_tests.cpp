#include "data/database/sql_query_utils.h"
#include "data/repositories/repository_error.h"
#include "data/repositories/settings_repository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

#include <array>
#include <concepts>
#include <expected>

static_assert(std::same_as<
    RepositoryResult<int>,
    std::expected<int, RepositoryError>
    >);
static_assert(std::same_as<
    RepositoryStatus,
    std::expected<void, RepositoryError>
    >);

class RepositoryErrorContractTests : public QObject
{
    Q_OBJECT

private slots:
    void stableCodesHaveDistinctValues();
    void allCodesPreserveDiagnosticsAndLegacyText();
    void sqlFailuresDefaultToTechnicalAndPreserveUserMessage();
    void settingsRepositoryPreservesTypedResultsAndEmptySemantics();
    void settingsRepositoryMapsSqlFailuresToTechnical();
    void settingsRepositoryRollsBackFailedBatchWrites();
    void settingsRepositoryReportsTransactionStartAndCommitFailures();
};

void RepositoryErrorContractTests::stableCodesHaveDistinctValues()
{
    constexpr std::array codes{
        RepositoryErrorCode::InvalidInput,
        RepositoryErrorCode::NotFound,
        RepositoryErrorCode::Conflict,
        RepositoryErrorCode::Validation,
        RepositoryErrorCode::Technical
    };

    QCOMPARE(static_cast<int>(codes[0]), 1);
    QCOMPARE(static_cast<int>(codes[1]), 2);
    QCOMPARE(static_cast<int>(codes[2]), 3);
    QCOMPARE(static_cast<int>(codes[3]), 4);
    QCOMPARE(static_cast<int>(codes[4]), 5);

    for (std::size_t i = 0; i < codes.size(); ++i)
    {
        for (std::size_t j = i + 1; j < codes.size(); ++j)
        {
            QVERIFY(codes[i] != codes[j]);
        }
    }
}

void RepositoryErrorContractTests::allCodesPreserveDiagnosticsAndLegacyText()
{
    constexpr std::array codes{
        RepositoryErrorCode::InvalidInput,
        RepositoryErrorCode::NotFound,
        RepositoryErrorCode::Conflict,
        RepositoryErrorCode::Validation,
        RepositoryErrorCode::Technical
    };
    const QString diagnostic =
        QStringLiteral("Repository diagnostic is preserved verbatim.");

    const RepositoryError unclassifiedError{diagnostic};
    QCOMPARE(unclassifiedError.code, RepositoryErrorCode::Technical);
    QCOMPARE(unclassifiedError.diagnostic, diagnostic);

    for (const RepositoryErrorCode code : codes)
    {
        const RepositoryError error{code, diagnostic};
        QCOMPARE(error.code, code);
        QCOMPARE(error.diagnostic, diagnostic);
        QCOMPARE(toLegacyDiagnostic(error), diagnostic);
    }

    const RepositoryResult<int> success{42};
    QVERIFY(success);
    QCOMPARE(*success, 42);

    const RepositoryStatus failure{
        std::unexpected(RepositoryError{
            RepositoryErrorCode::Validation,
            diagnostic
        })
    };
    QVERIFY(!failure);
    QCOMPARE(failure.error().code, RepositoryErrorCode::Validation);
}

void RepositoryErrorContractTests::sqlFailuresDefaultToTechnicalAndPreserveUserMessage()
{
    const QString connectionName =
        QStringLiteral("repository_error_contract_tests");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(!query.exec(QStringLiteral(
            "SELECT value FROM missing_repository_error_table"
            )));

        const SqlQueryUtils::ExecutionError sqlError =
            SqlQueryUtils::errorFor(
                query,
                QStringLiteral("Loading repository values")
                );
        const QString previousUserMessage = sqlError.userMessage();
        const RepositoryError repositoryError =
            SqlQueryUtils::repositoryErrorFor(sqlError);

        QCOMPARE(repositoryError.code, RepositoryErrorCode::Technical);
        QCOMPARE(repositoryError.diagnostic, previousUserMessage);
        QCOMPARE(toLegacyDiagnostic(repositoryError), previousUserMessage);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void RepositoryErrorContractTests::
settingsRepositoryPreservesTypedResultsAndEmptySemantics()
{
    const QString connectionName =
        QStringLiteral("settings_repository_success_tests");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE app_settings (key TEXT PRIMARY KEY, value)"
            )));

        SettingsRepository repository(database);

        // An empty batch is a no-op even when a transaction is already open.
        QVERIFY(database.transaction());
        const RepositoryStatus emptyBatch = repository.saveSettings({});
        QVERIFY(emptyBatch);
        QVERIFY(database.commit());

        const RepositoryStatus saved = repository.saveSetting(
            QStringLiteral("settings/known"),
            QStringLiteral("saved value")
            );
        QVERIFY(saved);

        const RepositoryResult<QVariant> loaded = repository.loadSetting(
            QStringLiteral("settings/known")
            );
        QVERIFY(loaded);
        QCOMPARE(loaded->toString(), QStringLiteral("saved value"));

        const RepositoryResult<QVariant> missing = repository.loadSetting(
            QStringLiteral("settings/missing")
            );
        QVERIFY(missing);
        QVERIFY(!missing->isValid());

        const RepositoryStatus batch = repository.saveSettings({
            {QStringLiteral("settings/first"), QStringLiteral("one")},
            {QStringLiteral("settings/second"), QStringLiteral("two")}
        });
        QVERIFY(batch);
        const RepositoryResult<QVariant> first = repository.loadSetting(
            QStringLiteral("settings/first")
            );
        const RepositoryResult<QVariant> second = repository.loadSetting(
            QStringLiteral("settings/second")
            );
        QVERIFY(first);
        QVERIFY(second);
        QCOMPARE(first->toString(), QStringLiteral("one"));
        QCOMPARE(second->toString(), QStringLiteral("two"));

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void RepositoryErrorContractTests::settingsRepositoryMapsSqlFailuresToTechnical()
{
    const QString connectionName =
        QStringLiteral("settings_repository_sql_failure_tests");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        SettingsRepository repository(database);
        const RepositoryStatus saved = repository.saveSetting(
            QStringLiteral("settings/missing-table"),
            QStringLiteral("value")
            );
        QVERIFY(!saved);
        QCOMPARE(saved.error().code, RepositoryErrorCode::Technical);
        QCOMPARE(
            saved.error().diagnostic,
            QStringLiteral(
                "Saving application setting failed for setting key "
                "'settings/missing-table': Parameter count mismatch"
                )
            );

        const RepositoryResult<QVariant> loaded = repository.loadSetting(
            QStringLiteral("settings/missing-table")
            );
        QVERIFY(!loaded);
        QCOMPARE(loaded.error().code, RepositoryErrorCode::Technical);
        QCOMPARE(
            loaded.error().diagnostic,
            QStringLiteral(
                "Loading application setting failed for setting key "
                "'settings/missing-table': Parameter count mismatch"
                )
            );

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void RepositoryErrorContractTests::settingsRepositoryRollsBackFailedBatchWrites()
{
    const QString connectionName =
        QStringLiteral("settings_repository_batch_rollback_tests");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE app_settings ("
            "key TEXT PRIMARY KEY, value TEXT CHECK(value != 'reject'))"
            )));

        SettingsRepository repository(database);
        QVERIFY(repository.saveSetting(
            QStringLiteral("settings/existing"),
            QStringLiteral("before")
            ));

        const RepositoryStatus expectedFailure = repository.saveSetting(
            QStringLiteral("settings/rejected"),
            QStringLiteral("reject")
            );
        QVERIFY(!expectedFailure);
        QCOMPARE(
            expectedFailure.error().code,
            RepositoryErrorCode::Technical
            );

        const RepositoryStatus failedBatch = repository.saveSettings({
            {QStringLiteral("settings/first"), QStringLiteral("committed only on success")},
            {QStringLiteral("settings/rejected"), QStringLiteral("reject")}
        });
        QVERIFY(!failedBatch);
        QCOMPARE(
            failedBatch.error().code,
            expectedFailure.error().code
            );
        QCOMPARE(
            failedBatch.error().diagnostic,
            expectedFailure.error().diagnostic
            );

        const RepositoryResult<QVariant> existing = repository.loadSetting(
            QStringLiteral("settings/existing")
            );
        const RepositoryResult<QVariant> first = repository.loadSetting(
            QStringLiteral("settings/first")
            );
        const RepositoryResult<QVariant> rejected = repository.loadSetting(
            QStringLiteral("settings/rejected")
            );
        QVERIFY(existing);
        QVERIFY(first);
        QVERIFY(rejected);
        QCOMPARE(existing->toString(), QStringLiteral("before"));
        QVERIFY(!first->isValid());
        QVERIFY(!rejected->isValid());

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void RepositoryErrorContractTests::
settingsRepositoryReportsTransactionStartAndCommitFailures()
{
    const QString connectionName =
        QStringLiteral("settings_repository_transaction_failure_tests");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE app_settings (key TEXT PRIMARY KEY, value TEXT)"
            )));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE settings_parent (id TEXT PRIMARY KEY)"
            )));

        SettingsRepository repository(database);

        QVERIFY(database.transaction());
        const RepositoryStatus failedStart = repository.saveSettings({
            {QStringLiteral("settings/start-failure"), QStringLiteral("value")}
        });
        QVERIFY(!failedStart);
        QCOMPARE(failedStart.error().code, RepositoryErrorCode::Technical);
        QCOMPARE(
            failedStart.error().diagnostic,
            QStringLiteral("Starting application settings transaction failed: %1")
                .arg(database.lastError().text())
            );
        QVERIFY(database.rollback());

        QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys = ON")));
        QVERIFY(query.exec(QStringLiteral("DROP TABLE app_settings")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE app_settings ("
            "key TEXT PRIMARY KEY, value TEXT REFERENCES settings_parent(id) "
            "DEFERRABLE INITIALLY DEFERRED)"
            )));

        const RepositoryStatus failedCommit = repository.saveSettings({
            {QStringLiteral("settings/commit-failure"), QStringLiteral("orphan")}
        });
        QVERIFY(!failedCommit);
        QCOMPARE(failedCommit.error().code, RepositoryErrorCode::Technical);
        QVERIFY(failedCommit.error().diagnostic.startsWith(
            QStringLiteral("Committing application settings failed: ")
            ));

        const RepositoryResult<QVariant> rolledBack = repository.loadSetting(
            QStringLiteral("settings/commit-failure")
            );
        QVERIFY(rolledBack);
        QVERIFY(!rolledBack->isValid());

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_GUILESS_MAIN(RepositoryErrorContractTests)

#include "repository_error_contract_tests.moc"
