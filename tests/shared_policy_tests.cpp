#include "core/network/http_request_policy.h"
#include "core/utils/file_name_utils.h"
#include "core/utils/student_name_utils.h"
#include "data/database/sql_query_utils.h"
#include "domain/models/document_output_result.h"
#include "domain/rules/schedule_value_parser.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTest>
#include <QUrl>
#include <QUuid>

class SharedPolicyTests final : public QObject
{
    Q_OBJECT

private slots:
    void weekdayParsing_data();
    void weekdayParsing();
    void timeParsing_data();
    void timeParsing();
    void sqlFailurePreservesContext_data();
    void sqlFailurePreservesContext();
    void allowedUrl_data();
    void allowedUrl();
    void successfulStatus_data();
    void successfulStatus();
    void safeFileName_data();
    void safeFileName();
    void englishNameNormalization_data();
    void englishNameNormalization();
    void englishNameValidation_data();
    void englishNameValidation();
    void koreanNameValidation_data();
    void koreanNameValidation();
    void duplicatePairValidation();
    void documentOutputStatus_data();
    void documentOutputStatus();
};

void SharedPolicyTests::weekdayParsing_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("canonical");

    QTest::newRow("canonical") << QStringLiteral("Monday") << true
        << QStringLiteral("Monday");
    QTest::newRow("trimmed-case-folded") << QStringLiteral("  tUeSdAy ")
        << true << QStringLiteral("Tuesday");
    QTest::newRow("unknown") << QStringLiteral("Funday") << false << QString();
    QTest::newRow("empty") << QString() << false << QString();
}

void SharedPolicyTests::weekdayParsing()
{
    QFETCH(QString, input);
    QFETCH(bool, valid);
    QFETCH(QString, canonical);
    const auto result = ScheduleValueParser::parseWeekday(input);
    QCOMPARE(result.has_value(), valid);
    if (result)
    {
        QCOMPARE(result->text, canonical);
    }
}

void SharedPolicyTests::timeParsing_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("canonical");

    QTest::newRow("midnight") << QStringLiteral("00:00") << true
        << QStringLiteral("00:00");
    QTest::newRow("trimmed") << QStringLiteral(" 09:05 ") << true
        << QStringLiteral("09:05");
    QTest::newRow("requires-leading-zero") << QStringLiteral("9:05") << false
        << QString();
    QTest::newRow("invalid-hour") << QStringLiteral("24:00") << false
        << QString();
}

void SharedPolicyTests::timeParsing()
{
    QFETCH(QString, input);
    QFETCH(bool, valid);
    QFETCH(QString, canonical);
    const auto result = ScheduleValueParser::parseTime(input);
    QCOMPARE(result.has_value(), valid);
    if (result)
    {
        QCOMPARE(result->text, canonical);
    }
}

void SharedPolicyTests::sqlFailurePreservesContext_data()
{
    QTest::addColumn<QString>("sql");
    QTest::newRow("missing-table") << QStringLiteral("SELECT * FROM missing_table");
    QTest::newRow("invalid-syntax") << QStringLiteral("SELECT FROM");
}

void SharedPolicyTests::sqlFailurePreservesContext()
{
    QFETCH(QString, sql);
    const QString connectionName = QUuid::createUuid().toString();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        const QString action = QStringLiteral("Reading policy test data");
        const auto result = SqlQueryUtils::execute(query, sql, action);
        QVERIFY(!result);
        QCOMPARE(result.error().action, action);
        QCOMPARE(result.error().queryText, sql);
        QVERIFY(result.error().sqlError.isValid());
        QVERIFY(result.error().userMessage().contains(action));
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void SharedPolicyTests::allowedUrl_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<bool>("allowed");
    QTest::newRow("https") << QStringLiteral("https://example.com/update.json") << true;
    QTest::newRow("localhost") << QStringLiteral("http://localhost/update.json") << true;
    QTest::newRow("loopback-v4") << QStringLiteral("http://127.0.0.1/a") << true;
    QTest::newRow("insecure-remote") << QStringLiteral("http://example.com/a") << false;
    QTest::newRow("relative") << QStringLiteral("update.json") << false;
}

void SharedPolicyTests::allowedUrl()
{
    QFETCH(QString, url);
    QFETCH(bool, allowed);
    QCOMPARE(HttpRequestPolicy::isAllowedSecureUrl(QUrl(url)), allowed);
}

void SharedPolicyTests::successfulStatus_data()
{
    QTest::addColumn<QVariant>("status");
    QTest::addColumn<bool>("successful");
    QTest::newRow("not-http") << QVariant() << true;
    QTest::newRow("ok") << QVariant(200) << true;
    QTest::newRow("last-success") << QVariant(299) << true;
    QTest::newRow("redirect") << QVariant(302) << false;
    QTest::newRow("server-error") << QVariant(500) << false;
}

void SharedPolicyTests::successfulStatus()
{
    QFETCH(QVariant, status);
    QFETCH(bool, successful);
    QCOMPARE(HttpRequestPolicy::isSuccessfulStatus(status), successful);
}

void SharedPolicyTests::safeFileName_data()
{
    QTest::addColumn<QString>("base");
    QTest::addColumn<QString>("extension");
    QTest::addColumn<QString>("fallback");
    QTest::addColumn<QChar>("replacement");
    QTest::addColumn<QString>("expected");
    QTest::newRow("unsafe") << QStringLiteral("Kim: Mina") << QStringLiteral("pdf")
        << QStringLiteral("Student") << QChar(u'-') << QStringLiteral("Kim- Mina.pdf");
    QTest::newRow("existing-suffix") << QStringLiteral("Report.PDF")
        << QStringLiteral(".pdf") << QStringLiteral("Document") << QChar(u'_')
        << QStringLiteral("Report.pdf");
    QTest::newRow("reserved") << QStringLiteral("CON") << QStringLiteral(".json")
        << QStringLiteral("Class") << QChar(u'_') << QStringLiteral("_CON.json");
    QTest::newRow("fallback") << QString() << QStringLiteral(".pdf")
        << QStringLiteral("Student") << QChar(u'-') << QStringLiteral("Student.pdf");
}

void SharedPolicyTests::safeFileName()
{
    QFETCH(QString, base);
    QFETCH(QString, extension);
    QFETCH(QString, fallback);
    QFETCH(QChar, replacement);
    QFETCH(QString, expected);
    QCOMPARE(
        FileNameUtils::filesystemSafeFileName(base, extension, fallback, replacement),
        expected
        );
}

void SharedPolicyTests::englishNameNormalization_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("capitalization") << QStringLiteral("jANE DOE")
        << QStringLiteral("Jane Doe");
    QTest::newRow("hyphen-spacing") << QStringLiteral("mary - jane")
        << QStringLiteral("Mary-jane");
    QTest::newRow("initials") << QStringLiteral("j. p. kim")
        << QStringLiteral("J.P.Kim");
    QTest::newRow("filters-non-english")
        << QString::fromUtf8("A\xEB\xAF\xBC" "B")
        << QStringLiteral("Ab");
}

void SharedPolicyTests::englishNameNormalization()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(StudentNameUtils::normalizeEnglishName(input), expected);
}

void SharedPolicyTests::englishNameValidation_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<int>("issue");
    QTest::newRow("valid") << QStringLiteral("Mary-Jane") << -1;
    QTest::newRow("too-long") << QString(21, QChar(u'A'))
        << int(StudentNameUtils::ValidationIssue::EnglishTooLong);
    QTest::newRow("non-ascii") << QStringLiteral("Jos\u00E9")
        << int(StudentNameUtils::ValidationIssue::EnglishContainsNonAscii);
}

void SharedPolicyTests::englishNameValidation()
{
    QFETCH(QString, value);
    QFETCH(int, issue);
    const auto issues = StudentNameUtils::validateEnglishName(value);
    QCOMPARE(issues.isEmpty(), issue < 0);
    if (issue >= 0)
    {
        QVERIFY(issues.contains(StudentNameUtils::ValidationIssue(issue)));
    }
}

void SharedPolicyTests::koreanNameValidation_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<int>("issue");
    QTest::newRow("empty") << QString() << -1;
    QTest::newRow("three-syllables") << QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98") << -1;
    QTest::newRow("too-short") << QString::fromUtf8("\xEA\xB9\x80")
        << int(StudentNameUtils::ValidationIssue::KoreanTooShort);
    QTest::newRow("unusual") << QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC")
        << int(StudentNameUtils::ValidationIssue::KoreanUnusualLength);
}

void SharedPolicyTests::koreanNameValidation()
{
    QFETCH(QString, value);
    QFETCH(int, issue);
    const auto issues = StudentNameUtils::validateKoreanName(value);
    QCOMPARE(issues.isEmpty(), issue < 0);
    if (issue >= 0)
    {
        QVERIFY(issues.contains(StudentNameUtils::ValidationIssue(issue)));
    }
}

void SharedPolicyTests::duplicatePairValidation()
{
    const QList<QStringList> rows{
        {QStringLiteral("Alex"), QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98")},
        {QStringLiteral("Alex"), QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98")},
        {QStringLiteral("Jamie"), QString::fromUtf8("\xEB\xB0\x95\xEC\xA7\x80\xEB\xAF\xBC")}
    };
    const auto duplicates = StudentNameUtils::duplicateRowsByNamePair(rows, 0, 1);
    QCOMPARE(duplicates.size(), 1);
    QCOMPARE(duplicates.constBegin().value(), QList<int>({0, 1}));
}

void SharedPolicyTests::documentOutputStatus_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<bool>("succeeded");
    QTest::newRow("completed") << int(DocumentOutputStatus::Completed) << true;
    QTest::newRow("sent-alias") << int(DocumentOutputStatus::Sent) << true;
    QTest::newRow("canceled") << int(DocumentOutputStatus::Canceled) << false;
    QTest::newRow("failed") << int(DocumentOutputStatus::Failed) << false;
}

void SharedPolicyTests::documentOutputStatus()
{
    QFETCH(int, status);
    QFETCH(bool, succeeded);
    DocumentOutputResult result;
    result.status = DocumentOutputStatus(status);
    QCOMPARE(result.succeeded(), succeeded);
}

QTEST_MAIN(SharedPolicyTests)
#include "shared_policy_tests.moc"
