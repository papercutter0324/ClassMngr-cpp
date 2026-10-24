#include "data/database/sql_query_utils.h"
#include "data/repositories/repository_error.h"

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

QTEST_GUILESS_MAIN(RepositoryErrorContractTests)

#include "repository_error_contract_tests.moc"
