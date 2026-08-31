#include "app/services/feature_services.h"
#include "core/network/http_request_policy.h"
#include "core/utils/colorutils.h"
#include "core/utils/file_name_utils.h"
#include "core/utils/student_name_utils.h"
#include "data/data_service.h"
#include "data/database/database_schema_manager.h"
#include "data/database/database_session.h"
#include "data/database/sql_query_utils.h"
#include "domain/models/document_output_result.h"
#include "domain/rules/schedule_value_parser.h"
#include "domain/validation/calendar_event_validator.h"
#include "domain/validation/class_info_validator.h"
#include "domain/validation/roster_validator.h"
#include "domain/validation/speaking_eval_validator.h"
#include "domain/validation/teacher_validator.h"
#include "domain/validation/shared_validation.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QUuid>

#include <algorithm>

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
    void schemaFailureRollsBack();
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
    void invalidEnglishNameNormalizationPreservesErrors();
    void koreanNameValidation_data();
    void koreanNameValidation();
    void invalidKoreanNameNormalizationPreservesErrors();
    void duplicatePairValidation();
    void generatedFileNamesAreSafeAndIdempotent();
    void validationResultHelpers();
    void structuredNameValidation_data();
    void structuredNameValidation();
    void structuredTimeValidation_data();
    void structuredTimeValidation();
    void structuredColorAndFileNameValidation_data();
    void structuredColorAndFileNameValidation();
    void enumAndRangeValidation_data();
    void enumAndRangeValidation();
    void structuredDuplicateNamePairs();
    void teacherValidatorNormalizesAndReportsFieldErrors();
    void classInfoValidatorChecksCurriculumAndSchedule();
    void featureServicesRejectInvalidTeacherAndClassMutations();
    void calendarEventValidatorNormalizesAndChecksTimeConsistency();
    void calendarEventValidatorChecksRecurrenceBounds();
    void featureServicesRejectInvalidCalendarMutations();
    void rosterValidatorNormalizesAndReportsCellIssues();
    void speakingEvalValidatorNormalizesAndReportsCellIssues();
    void featureServicesRejectInvalidRosterAndSpeakingEvaluationMutations();
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
        const QString identity = QStringLiteral("policy-test record 42");
        const auto result = SqlQueryUtils::execute(
            query,
            sql,
            action,
            identity
            );
        QVERIFY(!result);
        QCOMPARE(result.error().action, action);
        QCOMPARE(result.error().queryText, sql);
        QVERIFY(result.error().sqlError.isValid());
        QCOMPARE(
            result.error().driverError,
            result.error().sqlError.driverText()
            );
        QCOMPARE(
            result.error().databaseError,
            result.error().sqlError.databaseText()
            );
        QCOMPARE(
            result.error().nativeErrorCode,
            result.error().sqlError.nativeErrorCode()
            );
        QCOMPARE(result.error().recordIdentity, identity);
        QVERIFY(result.error().userMessage().contains(action));
        QVERIFY(result.error().userMessage().contains(identity));
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void SharedPolicyTests::schemaFailureRollsBack()
{
    const QString connectionName = QUuid::createUuid().toString();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName
            );
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE VIEW campuses AS SELECT 1 AS id"
            )));

        const Status status = DatabaseSchemaManager::ensureSchema(database);
        QVERIFY(!status);
        QVERIFY(status.error().contains(QStringLiteral("campuses")));

        QVERIFY(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master "
            "WHERE type='table' AND name='app_settings'"
            )));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
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
    QTest::newRow("preserves-invalid-unicode")
        << QString::fromUtf8("A\xEB\xAF\xBC" "B")
        << QString::fromUtf8("A\xEB\xAF\xBC" "B");
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
    QTest::newRow("invalid-ascii-character") << QStringLiteral("Alex2")
        << int(StudentNameUtils::ValidationIssue::EnglishContainsInvalidCharacters);
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

void SharedPolicyTests::invalidEnglishNameNormalizationPreservesErrors()
{
    for (ushort code = 0; code <= 0x7f; ++code)
    {
        const QString input = QStringLiteral("A") + QChar(code)
            + QStringLiteral("B");
        const auto originalIssues = StudentNameUtils::validateEnglishName(input);
        if (!originalIssues.contains(
                StudentNameUtils::ValidationIssue::EnglishContainsInvalidCharacters
                ))
        {
            continue;
        }

        const QString normalized = StudentNameUtils::normalizeEnglishName(input);
        const auto normalizedIssues = StudentNameUtils::validateEnglishName(
            normalized
            );
        QVERIFY2(
            normalizedIssues.contains(
                StudentNameUtils::ValidationIssue::EnglishContainsInvalidCharacters
                ),
            qPrintable(QStringLiteral("Normalization hid an invalid ASCII character: %1")
                .arg(code))
            );
    }

    const QString nonAscii = QStringLiteral("A\u00E9B");
    const QString normalized = StudentNameUtils::normalizeEnglishName(nonAscii);
    QCOMPARE(normalized, nonAscii);
    QVERIFY(StudentNameUtils::validateEnglishName(normalized).contains(
        StudentNameUtils::ValidationIssue::EnglishContainsNonAscii
        ));
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
    QTest::newRow("four-syllables")
        << QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98\xEC\xA7\x80")
        << int(StudentNameUtils::ValidationIssue::KoreanUnusualLength);
    QTest::newRow("five-syllables")
        << QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98\xEC\xA7\x80\xEC\x9B\x90")
        << int(StudentNameUtils::ValidationIssue::KoreanTooLong);
    QTest::newRow("invalid-character")
        << QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98" "1")
        << int(StudentNameUtils::ValidationIssue::KoreanContainsInvalidCharacters);
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

void SharedPolicyTests::invalidKoreanNameNormalizationPreservesErrors()
{
    const QString prefix = QString::fromUtf8("\xEA\xB9\x80");
    const QString suffix = QString::fromUtf8("\xEB\xAF\xBC\xEC\x88\x98");
    for (ushort code = 0; code <= 0x7f; ++code)
    {
        const QString input = prefix + QChar(code) + suffix;
        const auto originalIssues = StudentNameUtils::validateKoreanName(input);
        if (!originalIssues.contains(
                StudentNameUtils::ValidationIssue::KoreanContainsInvalidCharacters
                ))
        {
            continue;
        }

        const QString normalized = StudentNameUtils::normalizeKoreanName(input);
        QCOMPARE(normalized, input.trimmed());
        QVERIFY2(
            StudentNameUtils::validateKoreanName(normalized).contains(
                StudentNameUtils::ValidationIssue::KoreanContainsInvalidCharacters
                ),
            qPrintable(QStringLiteral("Normalization hid an invalid ASCII character: %1")
                .arg(code))
            );
    }

    const QString hanCharacter = prefix + QString::fromUtf8("\xE4\xB8\xAD") + suffix;
    const QString normalized = StudentNameUtils::normalizeKoreanName(hanCharacter);
    QCOMPARE(normalized, hanCharacter);
    QVERIFY(StudentNameUtils::validateKoreanName(normalized).contains(
        StudentNameUtils::ValidationIssue::KoreanContainsInvalidCharacters
        ));
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

void SharedPolicyTests::generatedFileNamesAreSafeAndIdempotent()
{
    QString unsafeName = QStringLiteral("Report");
    for (ushort code = 0; code <= 0x1f; ++code)
    {
        unsafeName.append(QChar(code));
    }
    unsafeName += QStringLiteral("\\\\/:*?\"<>|. ");

    const QString safeName = FileNameUtils::filesystemSafeFileName(
        unsafeName,
        QStringLiteral(".json"),
        QStringLiteral("Fallback")
        );
    QVERIFY(safeName.endsWith(QStringLiteral(".json")));
    for (const QChar character : safeName)
    {
        QVERIFY(character.unicode() > 0x1f);
        QVERIFY(!QStringLiteral("\\\\/:*?\"<>|").contains(character));
    }

    const auto normalized = FileNameUtils::normalizedFilesystemSafeFileName(
        safeName,
        QStringLiteral(".json")
        );
    QVERIFY(normalized);
    QCOMPARE(*normalized, safeName);
}

void SharedPolicyTests::validationResultHelpers()
{
    ValidationResult result(ValidationRules::issue(
        QStringLiteral("teacher.name.required"),
        {.field = QStringLiteral("name"), .row = 2, .column = 1}
        ));
    result.add(ValidationRules::issue(
        QStringLiteral("teacher.notes.long"),
        {.field = QStringLiteral("notes")},
        ValidationSeverity::Warning
        ));

    QVERIFY(result.hasErrors());
    QVERIFY(result.hasWarnings());
    QVERIFY(!result.isValid());
    QCOMPARE(result.errors().size(), 1);
    QCOMPARE(result.warnings().size(), 1);
    QCOMPARE(result.forField(QStringLiteral("name")).size(), 1);
    QCOMPARE(result.forField(QStringLiteral("missing")).size(), 0);

    ValidationResult other(ValidationRules::issue(
        QStringLiteral("teacher.room.required"),
        {.field = QStringLiteral("room")}
        ));
    QVERIFY(&result.merge(other) == &result);
    QCOMPARE(result.issues().size(), 3);
}

void SharedPolicyTests::structuredNameValidation_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("code");
    QTest::addColumn<int>("severity");

    QTest::newRow("english-whitespace") << QStringLiteral("english")
        << QStringLiteral("  Mary-Jane  ") << QString() << -1;
    QTest::newRow("english-unicode") << QStringLiteral("english")
        << QStringLiteral("Jos\u00E9")
        << QStringLiteral("student_name.english.non_ascii")
        << int(ValidationSeverity::Error);
    QTest::newRow("english-invalid-ascii") << QStringLiteral("english")
        << QStringLiteral("Alex2")
        << QStringLiteral("student_name.english.invalid_characters")
        << int(ValidationSeverity::Error);
    QTest::newRow("korean-standard") << QStringLiteral("korean")
        << QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98")
        << QString() << -1;
    QTest::newRow("korean-unusual-length") << QStringLiteral("korean")
        << QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC")
        << QStringLiteral("student_name.korean.unusual_length")
        << int(ValidationSeverity::Warning);
    QTest::newRow("korean-five-syllable") << QStringLiteral("korean")
        << QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98\xEC\xA7\x80\xEC\x9B\x90")
        << QStringLiteral("student_name.korean.too_long")
        << int(ValidationSeverity::Error);
    QTest::newRow("korean-invalid-character") << QStringLiteral("korean")
        << QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98" "1")
        << QStringLiteral("student_name.korean.invalid_characters")
        << int(ValidationSeverity::Error);
}

void SharedPolicyTests::structuredNameValidation()
{
    QFETCH(QString, kind);
    QFETCH(QString, value);
    QFETCH(QString, code);
    QFETCH(int, severity);

    const ValidationLocation location{
        .field = QStringLiteral("name"),
        .row = 4,
        .column = 2
    };
    const ValidationResult result = kind == QStringLiteral("english")
        ? SharedValidation::englishName(value, location)
        : SharedValidation::koreanName(value, location);

    QCOMPARE(result.hasErrors() || result.hasWarnings(), !code.isEmpty());
    if (!code.isEmpty())
    {
        const ValidationIssue& issue = result.issues().first();
        QCOMPARE(issue.code, code);
        QCOMPARE(issue.field, location.field);
        QCOMPARE(issue.row, location.row);
        QCOMPARE(issue.column, location.column);
        QCOMPARE(int(issue.severity), severity);
    }
}

void SharedPolicyTests::structuredTimeValidation_data()
{
    QTest::addColumn<QString>("start");
    QTest::addColumn<QString>("end");
    QTest::addColumn<QString>("code");

    QTest::newRow("ordered") << QStringLiteral("09:00")
        << QStringLiteral("09:45") << QString();
    QTest::newRow("trimmed") << QStringLiteral(" 09:00 ")
        << QStringLiteral(" 09:45 ") << QString();
    QTest::newRow("invalid-format") << QStringLiteral("9:00")
        << QStringLiteral("09:45")
        << QStringLiteral("schedule.time.invalid_format");
    QTest::newRow("equal-times") << QStringLiteral("09:45")
        << QStringLiteral("09:45")
        << QStringLiteral("schedule.time.end_not_after_start");
    QTest::newRow("reversed") << QStringLiteral("10:00")
        << QStringLiteral("09:45")
        << QStringLiteral("schedule.time.end_not_after_start");
}

void SharedPolicyTests::structuredTimeValidation()
{
    QFETCH(QString, start);
    QFETCH(QString, end);
    QFETCH(QString, code);

    const ValidationResult result = SharedValidation::timeOrder(
        start,
        end,
        {.field = QStringLiteral("startTime")},
        {.field = QStringLiteral("endTime")}
        );

    QCOMPARE(result.hasErrors(), !code.isEmpty());
    if (!code.isEmpty())
    {
        QVERIFY(std::any_of(
            result.issues().cbegin(),
            result.issues().cend(),
            [&code](const ValidationIssue& issue) { return issue.code == code; }
            ));
    }
}

void SharedPolicyTests::structuredColorAndFileNameValidation_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("value");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("canonical");

    QTest::newRow("color-canonical") << QStringLiteral("color")
        << QStringLiteral(" #12ab34 ") << true << QStringLiteral("#12AB34");
    QTest::newRow("color-short") << QStringLiteral("color")
        << QStringLiteral("#abc") << false << QString();
    QTest::newRow("color-name") << QStringLiteral("color")
        << QStringLiteral("red") << false << QString();
    QTest::newRow("file-valid") << QStringLiteral("file")
        << QStringLiteral("Student Report.pdf") << true
        << QStringLiteral("Student Report.pdf");
    QTest::newRow("file-reserved") << QStringLiteral("file")
        << QStringLiteral("CON") << false << QStringLiteral("_CON");
    QTest::newRow("file-unsafe") << QStringLiteral("file")
        << QStringLiteral("Class: Report") << false
        << QStringLiteral("Class_ Report");
    QTest::newRow("file-empty") << QStringLiteral("file")
        << QString() << false << QString();
}

void SharedPolicyTests::structuredColorAndFileNameValidation()
{
    QFETCH(QString, kind);
    QFETCH(QString, value);
    QFETCH(bool, valid);
    QFETCH(QString, canonical);

    const ValidationLocation location{.field = QStringLiteral("value")};
    if (kind == QStringLiteral("color"))
    {
        const auto normalized = ColorUtils::canonicalHexColor(value);
        QCOMPARE(normalized.has_value(), valid);
        if (normalized)
        {
            QCOMPARE(*normalized, canonical);
        }
        QCOMPARE(SharedValidation::color(value, location).isValid(), valid);
        return;
    }

    const auto normalized = FileNameUtils::normalizedFilesystemSafeFileName(
        value,
        {}
        );
    QCOMPARE(normalized.has_value(), !canonical.isEmpty());
    if (normalized)
    {
        QCOMPARE(*normalized, canonical);
    }
    QCOMPARE(SharedValidation::fileName(value, location).isValid(), valid);
}

void SharedPolicyTests::enumAndRangeValidation_data()
{
    QTest::addColumn<int>("enumValue");
    QTest::addColumn<int>("rangeValue");
    QTest::addColumn<QString>("enumCode");
    QTest::addColumn<QString>("rangeCode");

    QTest::newRow("valid") << 1 << 5 << QString() << QString();
    QTest::newRow("invalid-enum") << 9 << 5
        << QStringLiteral("validation.enum.invalid_value") << QString();
    QTest::newRow("out-of-range") << 2 << 11 << QString()
        << QStringLiteral("validation.range.out_of_bounds");
}

void SharedPolicyTests::enumAndRangeValidation()
{
    enum class TestMode
    {
        First = 1,
        Second = 2
    };

    QFETCH(int, enumValue);
    QFETCH(int, rangeValue);
    QFETCH(QString, enumCode);
    QFETCH(QString, rangeCode);

    const ValidationResult enumResult = ValidationRules::enumValue(
        static_cast<TestMode>(enumValue),
        {TestMode::First, TestMode::Second},
        {.field = QStringLiteral("mode")}
        );
    QCOMPARE(enumResult.hasErrors(), !enumCode.isEmpty());
    if (!enumCode.isEmpty())
    {
        QCOMPARE(enumResult.issues().first().code, enumCode);
        QCOMPARE(
            enumResult.issues().first().arguments.value(
                QStringLiteral("allowedValues")
                ).toList().size(),
            2
            );
    }

    const ValidationResult rangeResult = ValidationRules::inclusiveRange(
        rangeValue,
        1,
        10,
        {.field = QStringLiteral("count")}
        );
    QCOMPARE(rangeResult.hasErrors(), !rangeCode.isEmpty());
    if (!rangeCode.isEmpty())
    {
        QCOMPARE(rangeResult.issues().first().code, rangeCode);
        QCOMPARE(
            rangeResult.issues().first().arguments.value(
                QStringLiteral("maximum")
                ).toLongLong(),
            10LL
            );
    }
}

void SharedPolicyTests::structuredDuplicateNamePairs()
{
    const QList<QStringList> rows{
        {QStringLiteral("Alex"), QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98")},
        {QStringLiteral("Alex"), QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98")},
        {QStringLiteral("Jamie"), QString::fromUtf8("\xEB\xB0\x95\xEC\xA7\x80\xEB\xAF\xBC")}
    };

    const ValidationResult result = SharedValidation::duplicateNamePairs(
        rows,
        0,
        1,
        QStringLiteral("englishName"),
        QStringLiteral("koreanName")
        );

    QVERIFY(result.hasErrors());
    QCOMPARE(result.forField(QStringLiteral("englishName")).size(), 2);
    QCOMPARE(result.forField(QStringLiteral("koreanName")).size(), 2);
    const ValidationIssue& first = result.issues().first();
    QCOMPARE(first.row, 0);
    QCOMPARE(first.column, 0);
    QCOMPARE(first.code, QStringLiteral("student_name.duplicate_pair"));
    QCOMPARE(
        first.arguments.value(QStringLiteral("duplicateRows")).toList(),
        QVariantList({0, 1})
        );
}

void SharedPolicyTests::teacherValidatorNormalizesAndReportsFieldErrors()
{
    Teacher valid;
    valid.teacherEn = QStringLiteral("  mARY-jANE  ");
    valid.teacherKr = QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98");
    valid.preferredRomanization = QStringLiteral("mary jane");
    valid.preferredName = QStringLiteral("MARY JANE");
    valid.phoneNumber = QStringLiteral("010 1234 5678");
    valid.birthday = QStringLiteral("02-29");
    valid.internetType = QStringLiteral("wifi");
    valid.projectionType = QStringLiteral("zoom");

    const Teacher normalized = TeacherValidator::normalized(valid);
    QCOMPARE(normalized.teacherEn, QStringLiteral("Mary-jane"));
    QCOMPARE(normalized.preferredName, QStringLiteral("Mary Jane"));
    QCOMPARE(normalized.phoneNumber, QStringLiteral("010-1234-5678"));
    QCOMPARE(normalized.internetType, QStringLiteral("WiFi"));
    QCOMPARE(normalized.projectionType, QStringLiteral("Zoom"));
    QVERIFY(TeacherValidator::validate(normalized).isValid());

    Teacher invalid;
    invalid.teacherEn = QStringLiteral("Alex2");
    invalid.preferredName = QStringLiteral("Not a name");
    invalid.birthday = QStringLiteral("13-40");
    invalid.phoneNumber = QStringLiteral("123");
    invalid.internetType = QStringLiteral("Satellite");
    invalid.projectionType = QStringLiteral("Projector");

    const ValidationResult errors = TeacherValidator::validate(invalid);
    QVERIFY(errors.hasErrors());
    QVERIFY(!errors.forField(QStringLiteral("teacherEn")).isEmpty());
    QVERIFY(!errors.forField(QStringLiteral("preferredName")).isEmpty());
    QVERIFY(!errors.forField(QStringLiteral("birthday")).isEmpty());
    QVERIFY(!errors.forField(QStringLiteral("phoneNumber")).isEmpty());
    QCOMPARE(errors.forField(QStringLiteral("internetType")).first().code,
             QStringLiteral("validation.enum.invalid_value"));
    QCOMPARE(errors.forField(QStringLiteral("projectionType")).first().code,
             QStringLiteral("validation.enum.invalid_value"));
}

void SharedPolicyTests::classInfoValidatorChecksCurriculumAndSchedule()
{
    ClassInfo valid;
    valid.classId = 7;
    valid.classGrade = QStringLiteral(" e4 ");
    valid.classLevel = QStringLiteral("theseus");
    valid.readingBook = QStringLiteral("reading explorer 1");
    valid.essayBook = QStringLiteral("4a");
    valid.classColor = QStringLiteral("#aabbcc");
    valid.fontColor = QStringLiteral("#000000");
    valid.classTimes = {{
        QStringLiteral(" monday "),
        QStringLiteral("4:00 pm"),
        QStringLiteral("4:50 PM")
    }};

    const ClassInfo normalized = ClassInfoValidator::normalized(valid);
    QCOMPARE(normalized.classGrade, QStringLiteral("E4"));
    QCOMPARE(normalized.classLevel, QStringLiteral("Theseus"));
    QCOMPARE(normalized.readingBook, QStringLiteral("Reading Explorer 1"));
    QCOMPARE(normalized.essayBook, QStringLiteral("4A"));
    QCOMPARE(normalized.classColor, QStringLiteral("#AABBCC"));
    QCOMPARE(normalized.classTimes.first().day, QStringLiteral("Monday"));
    QCOMPARE(normalized.classTimes.first().startTime, QStringLiteral("4:00 PM"));
    QVERIFY(ClassInfoValidator::validate(normalized).isValid());

    ClassInfo invalid = normalized;
    invalid.classLevel = QStringLiteral("Unknown");
    invalid.classColor = QStringLiteral("blue");
    invalid.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("4:00 PM"), QStringLiteral("3:50 PM")},
        {QStringLiteral("Monday"), QStringLiteral("4:00 PM"), QStringLiteral("3:50 PM")}
    };

    const ValidationResult errors = ClassInfoValidator::validate(invalid);
    QVERIFY(errors.hasErrors());
    QCOMPARE(errors.forField(QStringLiteral("classLevel")).first().code,
             QStringLiteral("class_info.value.not_allowed"));
    QCOMPARE(errors.forField(QStringLiteral("classColor")).first().code,
             QStringLiteral("color.invalid_hex"));
    QVERIFY(!errors.forField(QStringLiteral("classTimes[0].endTime")).isEmpty());
    QVERIFY(!errors.forField(QStringLiteral("classTimes[1].startTime")).isEmpty());
}

void SharedPolicyTests::featureServicesRejectInvalidTeacherAndClassMutations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService dataService;
    QVERIFY(dataService.openDatabase(
        directory.filePath(QStringLiteral("validation.db"))
        ));

    TeacherService teachers(dataService.databaseSession(), &dataService);
    Teacher invalidTeacher;
    invalidTeacher.teacherEn = QStringLiteral("Alex2");
    const Result<int> rejectedTeacher = teachers.create(invalidTeacher);
    QVERIFY(!rejectedTeacher);
    QVERIFY(rejectedTeacher.error().contains(
        QStringLiteral("teacherEn: student_name.english.invalid_characters")
        ));

    Teacher validTeacher;
    validTeacher.teacherEn = QStringLiteral("Alex");
    const Result<int> createdTeacher = teachers.create(validTeacher);
    QVERIFY(createdTeacher);

    QSqlQuery legacyTeacherUpdate(dataService.databaseSession()->database());
    legacyTeacherUpdate.prepare(QStringLiteral(
        "UPDATE teachers SET internet_type=?, projection_type=? WHERE id=?"
        ));
    legacyTeacherUpdate.addBindValue(QStringLiteral("Satellite"));
    legacyTeacherUpdate.addBindValue(QStringLiteral("Laser"));
    legacyTeacherUpdate.addBindValue(*createdTeacher);
    QVERIFY(!legacyTeacherUpdate.exec());

    const Result<Teacher> storedTeacher = teachers.teacher(*createdTeacher);
    QVERIFY(storedTeacher);
    QCOMPARE(storedTeacher->internetType, QStringLiteral("WiFi"));
    QCOMPARE(storedTeacher->projectionType, QStringLiteral("HDMI"));

    ClassService classes(dataService.databaseSession(), &dataService);
    const Result<int> firstClass = classes.create(QString());
    const Result<int> secondClass = classes.create(QString());
    QVERIFY(firstClass);
    QVERIFY(secondClass);

    ClassInfo invalidInfo;
    invalidInfo.classId = *firstClass;
    invalidInfo.classTimes = {{
        QStringLiteral("Funday"),
        QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")
    }};
    const Status rejectedInfo = classes.saveClassInfo(invalidInfo);
    QVERIFY(!rejectedInfo);
    QVERIFY(rejectedInfo.error().contains(
        QStringLiteral("classTimes[0].day: schedule.weekday.invalid")
        ));

    ClassInfo firstInfo;
    firstInfo.classId = *firstClass;
    firstInfo.classTimes = {{
        QStringLiteral("Monday"),
        QStringLiteral("4:00 PM"),
        QStringLiteral("4:50 PM")
    }};
    QVERIFY(classes.saveClassInfo(firstInfo));

    firstInfo.teacherId = *createdTeacher;
    QVERIFY(classes.saveClassInfo(firstInfo));
    QVERIFY(teachers.remove(*createdTeacher));
    const Result<ClassInfo> unassignedInfo = classes.classInfo(*firstClass);
    QVERIFY(unassignedInfo);
    QCOMPARE(unassignedInfo->teacherId, -1);

    ClassInfo conflictingInfo = firstInfo;
    conflictingInfo.classId = *secondClass;
    conflictingInfo.teacherId = -1;
    const Status rejectedConflict = classes.saveClassInfo(conflictingInfo);
    QVERIFY(!rejectedConflict);
    QVERIFY(rejectedConflict.error().contains(
        QStringLiteral("Class schedule conflict")
        ));
}

void SharedPolicyTests::calendarEventValidatorNormalizesAndChecksTimeConsistency()
{
    CalendarEvent valid;
    valid.title = QStringLiteral("  Curriculum   Workshop  ");
    valid.eventType = QStringLiteral(" Workshop ");
    valid.timeStatus = QStringLiteral(" Timed ");
    valid.repeatSeriesId = QStringLiteral(" series-42 ");
    valid.startDate = QDate(2026, 9, 7);
    valid.startTime = QTime(9, 0);
    valid.endDate = valid.startDate;
    valid.endTime = QTime(10, 0);

    const CalendarEvent normalized = CalendarEventValidator::normalized(valid);
    QCOMPARE(normalized.title, QStringLiteral("Curriculum Workshop"));
    QCOMPARE(normalized.eventType, QStringLiteral("Workshop"));
    QCOMPARE(normalized.timeStatus, QStringLiteral("Timed"));
    QCOMPARE(normalized.repeatSeriesId, QStringLiteral("series-42"));
    QVERIFY(CalendarEventValidator::validate(normalized).isValid());

    CalendarEvent imported;
    imported.title = QStringLiteral("Imported event");
    imported.eventType = QStringLiteral("Other");
    imported.timeStatus = QStringLiteral("Unknown");
    imported.startDate = QDate(2026, 9, 8);
    imported.endDate = imported.startDate;
    QVERIFY(CalendarEventValidator::validate(imported).isValid());

    CalendarEvent invalid = normalized;
    invalid.title.clear();
    invalid.eventType = QStringLiteral("Assembly");
    invalid.timeStatus = QStringLiteral("Unconfirmed");
    invalid.allDay = true;
    invalid.endDate = invalid.startDate.addDays(-1);
    const ValidationResult errors = CalendarEventValidator::validate(invalid);
  QVERIFY(errors.hasErrors());
  QCOMPARE(errors.forField(QStringLiteral("title")).first().code,
             QStringLiteral("calendar.title.required"));
    QCOMPARE(errors.forField(QStringLiteral("eventType")).first().code,
             QStringLiteral("validation.enum.invalid_value"));
    QCOMPARE(errors.forField(QStringLiteral("endDate")).first().code,
             QStringLiteral("calendar.date.end_before_start"));
    QCOMPARE(errors.forField(QStringLiteral("timeStatus")).last().code,
             QStringLiteral("calendar.time_status.all_day_requires_timed"));

    CalendarEvent unconfirmed = normalized;
    unconfirmed.timeStatus = QStringLiteral("Unconfirmed");
    QVERIFY(CalendarEventValidator::validate(unconfirmed).hasErrors());
    unconfirmed.startTime = {};
    unconfirmed.endTime = {};
    QVERIFY(CalendarEventValidator::validate(unconfirmed).isValid());
}

void SharedPolicyTests::calendarEventValidatorChecksRecurrenceBounds()
{
    CalendarEvent event;
    event.title = QStringLiteral("Weekly staff meeting");
    event.eventType = QStringLiteral("Meeting");
    event.timeStatus = QStringLiteral("Timed");
    event.startDate = QDate(2026, 1, 5);
    event.startTime = QTime(9, 0);
    event.endDate = event.startDate;
    event.endTime = QTime(10, 0);

    QVERIFY(CalendarEventValidator::validateRecurrence(
        event,
        CalendarEventRepeatFrequency::Weekly,
        event.startDate.addDays(14)
        ).isValid());

    const ValidationResult tooEarly = CalendarEventValidator::validateRecurrence(
        event,
        CalendarEventRepeatFrequency::Daily,
        event.startDate.addDays(-1)
        );
    QCOMPARE(tooEarly.forField(QStringLiteral("repeat.untilDate")).first().code,
             QStringLiteral("calendar.repeat.until_before_start"));

    const ValidationResult tooMany = CalendarEventValidator::validateRecurrence(
        event,
        CalendarEventRepeatFrequency::Daily,
        event.startDate.addDays(CalendarEventValidator::MaximumRepeatOccurrences)
        );
    QCOMPARE(tooMany.forField(QStringLiteral("repeat.untilDate")).first().code,
             QStringLiteral("calendar.repeat.too_many_occurrences"));

    const ValidationResult invalidFrequency =
        CalendarEventValidator::validateRecurrence(
            event,
            static_cast<CalendarEventRepeatFrequency>(99),
            event.startDate
            );
    QCOMPARE(invalidFrequency.forField(QStringLiteral("repeat.frequency")).first().code,
             QStringLiteral("validation.enum.invalid_value"));

    QList<CalendarEvent> generatedSeries;
    generatedSeries.reserve(
        CalendarEventValidator::MaximumRepeatOccurrences + 1
        );
    for (int index = 0;
         index <= CalendarEventValidator::MaximumRepeatOccurrences;
         ++index)
    {
        CalendarEvent occurrence = event;
        occurrence.repeatSeriesId = QStringLiteral("series-limit");
        occurrence.startDate = event.startDate.addDays(index);
        occurrence.endDate = occurrence.startDate;
        generatedSeries.append(occurrence);
    }

    const ValidationResult oversizedSeries =
        CalendarEventValidator::validateSeries(generatedSeries);
    QCOMPARE(
        oversizedSeries.forField(
            QStringLiteral("events[0].repeatSeriesId")
            ).first().code,
        QStringLiteral("calendar.repeat.too_many_occurrences")
        );
}

void SharedPolicyTests::featureServicesRejectInvalidCalendarMutations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService dataService;
    QVERIFY(dataService.openDatabase(
        directory.filePath(QStringLiteral("calendar-validation.db"))
        ));
    CalendarService calendar(dataService.databaseSession(), &dataService);

    CalendarEvent invalid;
    invalid.title = QStringLiteral("Bad event");
    invalid.eventType = QStringLiteral("Other");
    invalid.timeStatus = QStringLiteral("Timed");
    invalid.startDate = QDate(2026, 10, 1);
    invalid.endDate = invalid.startDate;
    invalid.endTime = QTime(10, 0);

    const Result<QList<int>> rejected = calendar.saveEvents({invalid});
    QVERIFY(!rejected);
    QVERIFY(rejected.error().contains(
        QStringLiteral("events[0].startTime: calendar.time.invalid")
        ));

    const Result<int> singleRejected = calendar.saveEvent(invalid);
    QVERIFY(!singleRejected);
    QVERIFY(singleRejected.error().contains(
        QStringLiteral("startTime: calendar.time.invalid")
        ));

    CalendarEvent imported;
    imported.title = QStringLiteral("  Imported  holiday  ");
    imported.eventType = QStringLiteral("Holiday");
    imported.timeStatus = QStringLiteral("Timed");
    imported.allDay = true;
    imported.startDate = QDate(2026, 10, 3);
    imported.endDate = imported.startDate;

    const Result<QList<int>> saved = calendar.saveEvents({imported});
    QVERIFY(saved);
    const Result<CalendarEvent> loaded = calendar.event(saved->first());
    QVERIFY(loaded);
    QCOMPARE(loaded->title, QStringLiteral("Imported holiday"));
}

void SharedPolicyTests::rosterValidatorNormalizesAndReportsCellIssues()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {{
        QStringLiteral("  aLiCe  "),
        QStringLiteral(" 김민수 ")
    }};

    const Roster normalized = RosterValidator::normalized(roster);
    QCOMPARE(normalized.rows.first().first(), QStringLiteral("Alice"));
    QCOMPARE(normalized.rows.first().at(1), QStringLiteral("김민수"));
    QVERIFY(RosterValidator::validate(normalized).isValid());

    Roster invalid = normalized;
    invalid.rows.first()[0] = QStringLiteral("Alice1");
    invalid.rows.append({QStringLiteral("Alice1"), QStringLiteral("김민수")});

    const ValidationResult errors = RosterValidator::validate(invalid);
    QCOMPARE(errors.forField(QStringLiteral("rows[0].English")).first().code,
             QStringLiteral("student_name.english.invalid_characters"));
    QVERIFY(errors.hasErrors());
    QVERIFY(std::any_of(
        errors.issues().cbegin(),
        errors.issues().cend(),
        [](const ValidationIssue& issue)
        {
            return issue.code == QStringLiteral("student_name.duplicate_pair")
                && issue.row == 1
                && issue.column == 0;
        }
        ));
}

void SharedPolicyTests::speakingEvalValidatorNormalizesAndReportsCellIssues()
{
    SpeakingEvalRows rows = SpeakingEval::emptyRows();
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("  aLiCe  ");
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("김민수");
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::Grammar)] =
        QStringLiteral(" ㅁ ");

    const SpeakingEvalRows normalized = SpeakingEvalValidator::normalized(rows);
    QCOMPARE(normalized[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)],
             QStringLiteral("Alice"));
    QCOMPARE(normalized[0][SpeakingEval::toInt(SpeakingEvalColumn::Grammar)],
             QStringLiteral("A"));
    QVERIFY(SpeakingEvalValidator::validate(
        1, QStringLiteral(" Winter "), normalized).isValid());

    SpeakingEvalRows invalid = normalized;
    invalid[0][SpeakingEval::toInt(SpeakingEvalColumn::Grammar)] =
        QStringLiteral("D");
    invalid[1][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Bob");
    invalid[1][SpeakingEval::toInt(SpeakingEvalColumn::Comments)] =
        QString(SpeakingEval::CommentMaxLength + 1, QChar(u'x'));
    invalid[1].append(QStringLiteral("out-of-range cell"));
    invalid.append(QStringList(SpeakingEval::ColumnCount, QString()));

    const ValidationResult errors = SpeakingEvalValidator::validate(
        1, QStringLiteral("Winter"), invalid);
    QCOMPARE(errors.forField(QStringLiteral("rows[0].Grammar")).first().code,
             QStringLiteral("validation.enum.invalid_value"));
    QCOMPARE(errors.forField(QStringLiteral("rows[1].Korean Name")).first().code,
             QStringLiteral("speaking_evaluation.student_name.required"));
    QCOMPARE(errors.forField(QStringLiteral("rows[1].Comments")).first().code,
             QStringLiteral("validation.length.out_of_bounds"));
    QCOMPARE(errors.forField(QStringLiteral("rows[1]")).first().code,
             QStringLiteral("speaking_evaluation.row.too_many_cells"));
    QCOMPARE(errors.forField(QStringLiteral("rows")).first().code,
             QStringLiteral("speaking_evaluation.rows.too_many"));
}

void SharedPolicyTests::featureServicesRejectInvalidRosterAndSpeakingEvaluationMutations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DataService dataService;
    QVERIFY(dataService.openDatabase(
        directory.filePath(QStringLiteral("roster-speaking-validation.db"))
        ));

    ClassService classes(dataService.databaseSession(), &dataService);
    RosterService rosters(dataService.databaseSession(), &dataService);
    SpeakingEvaluationService evaluations(
        dataService.databaseSession(), &dataService);
    const Result<int> classId = classes.create(QStringLiteral("Validation"));
    QVERIFY(classId);

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {{QStringLiteral("Alex1"), QStringLiteral("김민수")}};
    const Status rejectedRoster = rosters.saveRoster(*classId, roster);
    QVERIFY(!rejectedRoster);
    QVERIFY(rejectedRoster.error().contains(
        QStringLiteral("rows[0].English: student_name.english.invalid_characters")
        ));

    roster.rows.first()[0] = QStringLiteral("  aLiCe  ");
    QVERIFY(rosters.saveRoster(*classId, roster));
    const Result<Roster> savedRoster = rosters.roster(*classId);
    QVERIFY(savedRoster);
    QCOMPARE(savedRoster->rows.first().first(), QStringLiteral("Alice"));

    roster.rows.first()[1] = QStringLiteral("김");
    const Status rejectedQuestionableRoster = rosters.saveRoster(*classId, roster);
    QVERIFY(!rejectedQuestionableRoster);
    QVERIFY(rejectedQuestionableRoster.error().contains(
        QStringLiteral("rows[0].Korean: student_name.korean.too_short")
        ));
    QVERIFY(rosters.saveRoster(*classId, roster, true));

    SpeakingEvalRows rows = SpeakingEval::emptyRows();
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Alice");
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("김민수");
    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::Grammar)] =
        QStringLiteral("D");
    const Status rejectedEvaluation = evaluations.saveEvaluation(
        *classId, QStringLiteral("Winter"), rows);
    QVERIFY(!rejectedEvaluation);
    QVERIFY(rejectedEvaluation.error().contains(
        QStringLiteral("rows[0].Grammar: validation.enum.invalid_value")
        ));

    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::Grammar)] =
        QStringLiteral(" 4 ");
    QVERIFY(evaluations.saveEvaluation(*classId, QStringLiteral(" Winter "), rows));

    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)] =
        QStringLiteral("김민수지원가");
    const Status rejectedQuestionableEvaluation = evaluations.saveEvaluation(
        *classId, QStringLiteral("Winter"), rows);
    QVERIFY(!rejectedQuestionableEvaluation);
    QVERIFY(rejectedQuestionableEvaluation.error().contains(
        QStringLiteral("rows[0].Korean Name: student_name.korean.too_long")
        ));
    QVERIFY(evaluations.saveEvaluation(
        *classId, QStringLiteral("Winter"), rows, {}, true));
    const Result<SpeakingEvalRows> savedEvaluation = evaluations.evaluation(
        *classId, QStringLiteral("Winter"));
    QVERIFY(savedEvaluation);
    QCOMPARE(
        savedEvaluation->first()[SpeakingEval::toInt(SpeakingEvalColumn::Grammar)],
        QStringLiteral("A")
        );
    QCOMPARE(
        savedEvaluation->first()[SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)],
        QStringLiteral("김민수지원가")
        );
}

void SharedPolicyTests::documentOutputStatus_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<bool>("succeeded");
    QTest::newRow("completed") << int(DocumentOutputStatus::Completed) << true;
    QTest::newRow("sent-alias") << int(DocumentOutputStatus::Sent) << true;
    QTest::newRow("canceled") << int(DocumentOutputStatus::Canceled) << false;
    QTest::newRow("failed") << int(DocumentOutputStatus::Failed) << false;
    QTest::newRow("internal-renderer-failed")
        << int(DocumentOutputStatus::InternalRendererFailed) << false;
}

void SharedPolicyTests::documentOutputStatus()
{
    QFETCH(int, status);
    QFETCH(bool, succeeded);
    DocumentOutputResult result;
    result.status = DocumentOutputStatus(status);
    QCOMPARE(result.succeeded(), succeeded);

    result.message = QString::fromUtf8("출력 결과");
    result.savedPdfPaths = {
        QString::fromUtf8("reports/학생-1.pdf"),
        QString::fromUtf8("reports/학생-2.pdf")
    };
    result.savedArchivePath = QString::fromUtf8("reports/월말.zip");
    const DocumentOutputResult roundTrip =
        DocumentOutputResult::fromEngine(result.toEngine());
    QCOMPARE(roundTrip.status, result.status);
    QCOMPARE(roundTrip.message, result.message);
    QCOMPARE(roundTrip.savedPdfPaths, result.savedPdfPaths);
    QCOMPARE(roundTrip.savedArchivePath, result.savedArchivePath);
}

QTEST_MAIN(SharedPolicyTests)
#include "shared_policy_tests.moc"
