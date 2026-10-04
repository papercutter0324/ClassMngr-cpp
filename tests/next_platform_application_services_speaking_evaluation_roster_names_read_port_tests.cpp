#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "next/application/speaking_evaluation_roster_names_read_query.h"
#include "next/platform/application_services_speaking_evaluation_roster_names_read_port.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>
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
        QStringLiteral("speaking-eval-roster-names-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

Application::SpeakingEvaluationRosterNamesReadRequest request(
    const int classId
    )
{
    const auto typedId = Domain::ClassId::fromString(std::to_string(classId));
    if (!typedId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    return {.classId = *typedId};
}

struct RawCell final
{
    int row;
    int column;
    QVariant value;
};

bool seedRoster(
    QSqlDatabase database,
    const int classId,
    const QStringList& columns,
    const std::vector<RawCell>& cells,
    QString* error = nullptr
    )
{
    QString ignoredError;
    if (!error)
    {
        error = &ignoredError;
    }

    QSqlQuery insertColumn(database);
    if (!insertColumn.prepare(QStringLiteral(
            "INSERT INTO roster_columns (class_id, name, position, width) "
            "VALUES (?, ?, ?, ?)"
            )))
    {
        *error = insertColumn.lastError().text();
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
            *error = insertColumn.lastError().text();
            return false;
        }
    }

    QSqlQuery insertCell(database);
    if (!insertCell.prepare(QStringLiteral(
            "INSERT INTO roster_data (class_id, row_index, col_index, value) "
            "VALUES (?, ?, ?, ?)"
            )))
    {
        *error = insertCell.lastError().text();
        return false;
    }
    for (const RawCell& cell : cells)
    {
        insertCell.bindValue(0, classId);
        insertCell.bindValue(1, cell.row);
        insertCell.bindValue(2, cell.column);
        insertCell.bindValue(3, cell.value);
        if (!insertCell.exec())
        {
            *error = insertCell.lastError().text();
            return false;
        }
    }
    return true;
}

}

class NextPlatformApplicationServicesSpeakingEvaluationRosterNamesReadPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void readsFirstCaseInsensitiveHeadersAndLegacySparseRows();
    void preservesMissingHeadersAndRowsMaterializedByUnrelatedCells();
    void skipsCellReadWhenNoColumnsExist();
    void emptyRosterWithNameColumnsIsSuccessful();
    void repositoryFailureIsStructured();
};

void NextPlatformApplicationServicesSpeakingEvaluationRosterNamesReadPortTests::
readsFirstCaseInsensitiveHeadersAndLegacySparseRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Name Projection")
        );
    QVERIFY(createdClass);

    const QStringList columns{
        QStringLiteral(" English "),
        QStringLiteral("english"),
        QStringLiteral("English"),
        QStringLiteral("KOREAN"),
        QStringLiteral("korean"),
        QStringLiteral("Notes")
    };
    const std::vector<RawCell> cells{
        {0, 0, QStringLiteral("Whitespace header is not selected")},
        {0, 1, QStringLiteral("  First English  ")},
        {0, 2, QStringLiteral("Ignored duplicate English")},
        {0, 3, QStringLiteral("  김민지  ")},
        {0, 4, QStringLiteral("Ignored duplicate Korean")},
        {0, 5, QStringLiteral("")},
        {1, -1, QStringLiteral("negative column")},
        {2, 5, QVariant{}},
        {4, 6, QStringLiteral("out-of-range column")},
        {-1, 1, QStringLiteral("negative row")}
    };
    QString error;
    QSqlQuery constraints(services.databaseSession()->database());
    QVERIFY2(
        constraints.exec(QStringLiteral("PRAGMA ignore_check_constraints=ON")),
        qPrintable(constraints.lastError().text())
        );
    const bool seeded = seedRoster(
        services.databaseSession()->database(),
        *createdClass,
        columns,
        cells,
        &error
        );
    QVERIFY2(
        constraints.exec(QStringLiteral("PRAGMA ignore_check_constraints=OFF")),
        qPrintable(constraints.lastError().text())
        );
    QVERIFY2(
        seeded,
        qPrintable(error)
        );

    Platform::ApplicationServicesSpeakingEvaluationRosterNamesReadPort port(
        services);
    const auto result = Application::
        SpeakingEvaluationRosterNamesReadQuery::execute(
            request(*createdClass),
            port
            );

    QVERIFY2(
        result,
        result ? "" : qPrintable(QString::fromStdString(result.error().message))
        );
    QVERIFY(result.value().hasEnglishColumn);
    QVERIFY(result.value().hasKoreanColumn);
    QCOMPARE(result.value().rows.size(), std::size_t(3));
    QCOMPARE(
        result.value().rows[0].englishName,
        std::u16string(u"  First English  ")
        );
    QCOMPARE(
        result.value().rows[0].koreanName,
        std::u16string(u"  김민지  ")
        );
    QVERIFY(result.value().rows[1].englishName.empty());
    QVERIFY(result.value().rows[1].koreanName.empty());
    QVERIFY(result.value().rows[2].englishName.empty());
    QVERIFY(result.value().rows[2].koreanName.empty());
}

void NextPlatformApplicationServicesSpeakingEvaluationRosterNamesReadPortTests::
preservesMissingHeadersAndRowsMaterializedByUnrelatedCells()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Missing Name Columns")
        );
    QVERIFY(createdClass);
    QString error;
    QVERIFY2(
        seedRoster(
            services.databaseSession()->database(),
            *createdClass,
            {QStringLiteral("Notes")},
            {
                {0, 0, QVariant{}},
                {2, 0, QStringLiteral("")},
                {3, 1, QStringLiteral("out-of-range column")}
            },
            &error
            ),
        qPrintable(error)
        );

    Platform::ApplicationServicesSpeakingEvaluationRosterNamesReadPort port(
        services);
    const auto result = Application::
        SpeakingEvaluationRosterNamesReadQuery::execute(
            request(*createdClass),
            port
            );

    QVERIFY(result);
    QVERIFY(!result.value().hasEnglishColumn);
    QVERIFY(!result.value().hasKoreanColumn);
    QCOMPARE(result.value().rows.size(), std::size_t(3));
    for (const auto& row : result.value().rows)
    {
        QVERIFY(row.englishName.empty());
        QVERIFY(row.koreanName.empty());
    }
}

void NextPlatformApplicationServicesSpeakingEvaluationRosterNamesReadPortTests::
skipsCellReadWhenNoColumnsExist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation No Columns")
        );
    QVERIFY(createdClass);

    QSqlQuery dropData(services.databaseSession()->database());
    QVERIFY2(
        dropData.exec(QStringLiteral("DROP TABLE roster_data")),
        qPrintable(dropData.lastError().text())
        );

    Platform::ApplicationServicesSpeakingEvaluationRosterNamesReadPort port(
        services);
    const auto result = Application::
        SpeakingEvaluationRosterNamesReadQuery::execute(
            request(*createdClass),
            port
            );

    QVERIFY(result);
    QVERIFY(!result.value().hasEnglishColumn);
    QVERIFY(!result.value().hasKoreanColumn);
    QVERIFY(result.value().rows.empty());
}

void NextPlatformApplicationServicesSpeakingEvaluationRosterNamesReadPortTests::
emptyRosterWithNameColumnsIsSuccessful()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Empty Named Roster")
        );
    QVERIFY(createdClass);
    QVERIFY(seedRoster(
        services.databaseSession()->database(),
        *createdClass,
        {QStringLiteral("English"), QStringLiteral("Korean")},
        {}
        ));

    Platform::ApplicationServicesSpeakingEvaluationRosterNamesReadPort port(
        services);
    const auto result = Application::
        SpeakingEvaluationRosterNamesReadQuery::execute(
            request(*createdClass),
            port
            );

    QVERIFY(result);
    QVERIFY(result.value().hasEnglishColumn);
    QVERIFY(result.value().hasKoreanColumn);
    QVERIFY(result.value().rows.empty());
}

void NextPlatformApplicationServicesSpeakingEvaluationRosterNamesReadPortTests::
repositoryFailureIsStructured()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(databasePath(directory)));
    const auto createdClass = services.classService()->create(
        QStringLiteral("Speaking Evaluation Failed Roster")
        );
    QVERIFY(createdClass);
    QVERIFY(seedRoster(
        services.databaseSession()->database(),
        *createdClass,
        {QStringLiteral("English"), QStringLiteral("Korean")},
        {}
        ));
    QSqlQuery dropData(services.databaseSession()->database());
    QVERIFY(dropData.exec(QStringLiteral("DROP TABLE roster_data")));

    Platform::ApplicationServicesSpeakingEvaluationRosterNamesReadPort port(
        services);
    const auto result = Application::
        SpeakingEvaluationRosterNamesReadQuery::execute(
            request(*createdClass),
            port
            );

    QVERIFY(!result);
    QCOMPARE(result.error().code, Domain::ErrorCode::Technical);
}

QTEST_GUILESS_MAIN(
    NextPlatformApplicationServicesSpeakingEvaluationRosterNamesReadPortTests
    )

#include "next_platform_application_services_speaking_evaluation_roster_names_read_port_tests.moc"
