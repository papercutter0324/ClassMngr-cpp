#include "next/application/native_english_teacher_directory_save_use_case.h"

#include <QDate>
#include <QtTest/QtTest>

#include <array>
#include <string_view>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

Application::NativeEnglishTeacherDirectorySaveRow row(
    std::u16string key,
    std::u16string birthday = {}
    )
{
    Application::NativeEnglishTeacherDirectorySaveRow value;
    value.name = key;
    value.normalizedNameKey = std::move(key);
    value.birthday = std::move(birthday);
    return value;
}

class RecordingSavePort final
    : public Application::NativeEnglishTeacherDirectorySavePort
{
public:
    [[nodiscard]] Domain::Result<void> saveNativeEnglishTeacherDirectory(
        const Application::NativeEnglishTeacherDirectorySaveRequest& request
        ) const override
    {
        ++calls;
        received = request;
        return result;
    }

    mutable int calls = 0;
    mutable Application::NativeEnglishTeacherDirectorySaveRequest received;
    Domain::Result<void> result = Domain::Result<void>::success();
};

}

class NextApplicationNativeEnglishTeacherDirectorySaveTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void acceptsBlankAndLeapDayBirthdayText();
    void preservesQtBirthdayParsingBehavior();
    void rejectsEmptyAndDuplicateComparisonKeys();
    void rejectsInvalidCalendarDay();
    void reportsTheFirstInvalidRow();
    void useCaseCallsThePortOnceForValidInputAndNeverForInvalidInput();
};

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
acceptsBlankAndLeapDayBirthdayText()
{
    Application::NativeEnglishTeacherDirectorySaveRequest request;
    request.rows = {
        row(u"coordinator", u" \t\u00a0"),
        row(u"leap-year-birthday", u"02-29")
    };

    const auto validation =
        Application::validateNativeEnglishTeacherDirectorySave(request);

    QVERIFY(validation.isValid());
    QCOMPARE(
        validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::None);
}

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
preservesQtBirthdayParsingBehavior()
{
    const std::array<std::u16string_view, 15> birthdays{
        u"01-01",
        u"02-29",
        u"02-30",
        u"04-30",
        u"04-31",
        u"06-30",
        u"06-31",
        u"13-01",
        u"2-9",
        u"02-9",
        u"2-09",
        u" 02-29 \t",
        u"\u00a002-29\u3000",
        u"\u0660\u0662-\u0662\u0669",
        u"02/29"
    };

    for (const std::u16string_view birthday : birthdays)
    {
        const QString trimmed =
            QString::fromStdU16String(std::u16string(birthday)).trimmed();
        const bool qtAccepts = QDate::fromString(
            QStringLiteral("2000-%1").arg(trimmed),
            QStringLiteral("yyyy-MM-dd")
            ).isValid();
        QCOMPARE(Application::isValidMonthDayBirthday(birthday), qtAccepts);
    }
}

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
rejectsEmptyAndDuplicateComparisonKeys()
{
    Application::NativeEnglishTeacherDirectorySaveRequest emptyName;
    emptyName.rows = {row(u"")};
    auto validation =
        Application::validateNativeEnglishTeacherDirectorySave(emptyName);
    QCOMPARE(
        validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::EmptyName);
    QCOMPARE(validation.rowIndex, std::size_t(0));

    Application::NativeEnglishTeacherDirectorySaveRequest duplicateName;
    duplicateName.rows = {row(u"strasse"), row(u"strasse")};
    validation =
        Application::validateNativeEnglishTeacherDirectorySave(duplicateName);
    QCOMPARE(
        validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::DuplicateName);
    QCOMPARE(validation.rowIndex, std::size_t(1));
}

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
rejectsInvalidCalendarDay()
{
    Application::NativeEnglishTeacherDirectorySaveRequest request;
    request.rows = {row(u"teacher", u"02-30")};

    const auto validation =
        Application::validateNativeEnglishTeacherDirectorySave(request);

    QCOMPARE(
        validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::InvalidBirthday);
    QCOMPARE(validation.rowIndex, std::size_t(0));
}

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
reportsTheFirstInvalidRow()
{
    Application::NativeEnglishTeacherDirectorySaveRequest request;
    request.rows = {
        row(u"valid"),
        row(u"", u"02-30"),
        row(u"later-invalid-date", u"02-30")
    };

    const auto validation =
        Application::validateNativeEnglishTeacherDirectorySave(request);

    QCOMPARE(
        validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::EmptyName);
    QCOMPARE(validation.rowIndex, std::size_t(1));
}

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
useCaseCallsThePortOnceForValidInputAndNeverForInvalidInput()
{
    Application::NativeEnglishTeacherDirectorySaveRequest valid;
    valid.rows = {row(u"teacher")};
    valid.rows.front().id = Domain::NativeEnglishTeacherId(42);
    valid.deletedIds = {Domain::NativeEnglishTeacherId(31)};

    RecordingSavePort port;
    auto outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
        execute(valid, port);
    QVERIFY(outcome.saved);
    QVERIFY(outcome.validation.isValid());
    QVERIFY(!outcome.error.has_value());
    QCOMPARE(port.calls, 1);
    QVERIFY(port.received == valid);

    Application::NativeEnglishTeacherDirectorySaveRequest invalid;
    invalid.rows = {row(u"")};
    outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
        execute(invalid, port);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::EmptyName);
    QCOMPARE(port.calls, 1);

    invalid.rows = {row(u"teacher", u"02-30")};
    outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
        execute(invalid, port);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::InvalidBirthday);
    QCOMPARE(port.calls, 1);
}

QTEST_APPLESS_MAIN(NextApplicationNativeEnglishTeacherDirectorySaveTests)

#include "next_application_native_english_teacher_directory_save_tests.moc"
