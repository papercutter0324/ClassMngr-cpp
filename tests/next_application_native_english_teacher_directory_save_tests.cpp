#include "next/application/native_english_teacher_directory_save_use_case.h"

#include <QDate>
#include <QtTest/QtTest>

#include <array>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;

using NativeEnglishTeacherDirectorySaveValidationRowIndex =
    Application::NativeEnglishTeacherDirectorySaveValidationRowIndex;

static_assert(std::is_same_v<
    decltype(Application::NativeEnglishTeacherDirectorySaveValidation{}.rowIndex),
    NativeEnglishTeacherDirectorySaveValidationRowIndex
    >);
static_assert(std::is_same_v<
    decltype(NativeEnglishTeacherDirectorySaveValidationRowIndex{}.rawValue()),
    std::size_t
    >);
static_assert(std::is_constructible_v<
    NativeEnglishTeacherDirectorySaveValidationRowIndex,
    std::size_t
    >);
static_assert(!std::is_convertible_v<
    std::size_t,
    NativeEnglishTeacherDirectorySaveValidationRowIndex
    >);
static_assert(!std::is_convertible_v<
    NativeEnglishTeacherDirectorySaveValidationRowIndex,
    std::size_t
    >);
static_assert(!std::is_aggregate_v<
    NativeEnglishTeacherDirectorySaveValidationRowIndex
    >);

namespace
{

std::u16string normalizedTestNameKey(const std::u16string_view value)
{
    std::u16string key;
    bool previousWasSpace = true;
    for (const char16_t character : value)
    {
        const bool isSpace = character == u' '
            || character == u'\t'
            || character == u'\r'
            || character == u'\n';
        if (isSpace)
        {
            if (!previousWasSpace)
            {
                key.push_back(u' ');
            }
            previousWasSpace = true;
            continue;
        }

        key.push_back(character >= u'A' && character <= u'Z'
            ? static_cast<char16_t>(character + (u'a' - u'A'))
            : character);
        previousWasSpace = false;
    }
    if (!key.empty() && key.back() == u' ')
    {
        key.pop_back();
    }
    return key;
}

Application::NativeEnglishTeacherDirectorySaveRow row(
    std::u16string name,
    std::u16string birthday = {}
    )
{
    Application::NativeEnglishTeacherDirectorySaveRow value;
    value.name = std::move(name);
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
    void validationRowIndexHasTypedValueSemantics();
    void acceptsBlankAndLeapDayBirthdayText();
    void preservesQtBirthdayParsingBehavior();
    void derivesEmptyAndDuplicateIdentityFromRawNames();
    void rejectsInvalidCalendarDay();
    void reportsTheFirstInvalidRow();
    void useCaseCallsThePortOnceForValidInputAndNeverForInvalidInput();
    void rejectsInvalidRowAndDeletedIdsBeforeCallingPort();
};

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
validationRowIndexHasTypedValueSemantics()
{
    const NativeEnglishTeacherDirectorySaveValidationRowIndex zero;
    const NativeEnglishTeacherDirectorySaveValidationRowIndex positive{7};

    QCOMPARE(zero.rawValue(), std::size_t(0));
    QCOMPARE(positive.rawValue(), std::size_t(7));
    QVERIFY(zero == NativeEnglishTeacherDirectorySaveValidationRowIndex{});
    QVERIFY(
        positive == NativeEnglishTeacherDirectorySaveValidationRowIndex{7}
        );
    QVERIFY(zero != positive);
}

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
acceptsBlankAndLeapDayBirthdayText()
{
    Application::NativeEnglishTeacherDirectorySaveRequest request;
    request.rows = {
        row(u"coordinator", u" \t\u00a0"),
        row(u"leap-year-birthday", u"02-29")
    };

    const auto validation =
        Application::validateNativeEnglishTeacherDirectorySave(
            request,
            normalizedTestNameKey);

    QVERIFY(validation.isValid());
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(0));
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
        const auto parsed = Application::parseMonthDayBirthday(birthday);
        QCOMPARE(parsed.has_value(), qtAccepts);
        if (qtAccepts)
        {
            QVERIFY(parsed.has_value());
            QCOMPARE(
                parsed->month(),
                (trimmed[0].unicode() - u'0') * 10
                    + (trimmed[1].unicode() - u'0')
                );
            QCOMPARE(
                parsed->day(),
                (trimmed[3].unicode() - u'0') * 10
                    + (trimmed[4].unicode() - u'0')
                );
        }
    }
}

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
derivesEmptyAndDuplicateIdentityFromRawNames()
{
    Application::NativeEnglishTeacherDirectorySaveRequest emptyName;
    emptyName.rows = {row(u" \t\r\n ")};
    auto validation =
        Application::validateNativeEnglishTeacherDirectorySave(
            emptyName,
            normalizedTestNameKey);
    QCOMPARE(
        validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::EmptyName);
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(0));

    Application::NativeEnglishTeacherDirectorySaveRequest duplicateName;
    duplicateName.rows = {
        row(u"  Teacher \t Name  "),
        row(u"teacher name")
    };
    validation =
        Application::validateNativeEnglishTeacherDirectorySave(
            duplicateName,
            normalizedTestNameKey);
    QCOMPARE(
        validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::DuplicateName);
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(1));
}

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
rejectsInvalidCalendarDay()
{
    Application::NativeEnglishTeacherDirectorySaveRequest request;
    request.rows = {row(u"teacher", u"02-30")};

    const auto validation =
        Application::validateNativeEnglishTeacherDirectorySave(
            request,
            normalizedTestNameKey);

    QCOMPARE(
        validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::InvalidBirthday);
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(0));
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
        Application::validateNativeEnglishTeacherDirectorySave(
            request,
            normalizedTestNameKey);

    QCOMPARE(
        validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::EmptyName);
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(1));
}

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
useCaseCallsThePortOnceForValidInputAndNeverForInvalidInput()
{
    Application::NativeEnglishTeacherDirectorySaveRequest valid;
    valid.rows = {row(u"teacher"), row(u"co-teacher")};
    valid.rows.front().id = Domain::NativeEnglishTeacherId(42);
    valid.deletedIds = {
        Domain::NativeEnglishTeacherId(31),
        Domain::NativeEnglishTeacherId(31)
    };

    RecordingSavePort port;
    auto outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
        execute(valid, port, normalizedTestNameKey);
    QVERIFY(outcome.saved);
    QVERIFY(outcome.validation.isValid());
    QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(0));
    QVERIFY(!outcome.error.has_value());
    QCOMPARE(port.calls, 1);
    QVERIFY(port.received == valid);

    Application::NativeEnglishTeacherDirectorySaveRequest invalid;
    invalid.rows = {row(u" \t\r\n ")};
    outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
        execute(invalid, port, normalizedTestNameKey);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::EmptyName);
    QCOMPARE(port.calls, 1);

    invalid.rows = {
        row(u"  Teacher \t Name  "),
        row(u"teacher name")
    };
    outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
        execute(invalid, port, normalizedTestNameKey);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::DuplicateName);
    QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(1));
    QCOMPARE(port.calls, 1);

    invalid.rows = {row(u"teacher", u"02-30")};
    outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
        execute(invalid, port, normalizedTestNameKey);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::InvalidBirthday);
    QCOMPARE(port.calls, 1);
}

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
rejectsInvalidRowAndDeletedIdsBeforeCallingPort()
{
    RecordingSavePort port;
    Application::NativeEnglishTeacherDirectorySaveRequest invalid;
    Application::NativeEnglishTeacherDirectorySaveOutcome outcome;

    for (const int id : std::array<int, 2>{0, -7})
    {
        invalid.rows = {row(u"valid teacher")};
        invalid.rows.front().id = Domain::NativeEnglishTeacherId(id);
        outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
            execute(invalid, port, normalizedTestNameKey);
        QVERIFY(!outcome.saved);
        QVERIFY(!outcome.error.has_value());
        QCOMPARE(
            outcome.validation.issue,
            Application::NativeEnglishTeacherDirectorySaveIssue::InvalidRowId);
        QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(0));
        QCOMPARE(port.calls, 0);
    }

    invalid.rows = {row(u"first teacher"), row(u"second teacher")};
    invalid.rows[0].id = Domain::NativeEnglishTeacherId(42);
    invalid.rows[1].id = Domain::NativeEnglishTeacherId(42);
    outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
        execute(invalid, port, normalizedTestNameKey);
    QVERIFY(!outcome.saved);
    QVERIFY(!outcome.error.has_value());
    QCOMPARE(
        outcome.validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::DuplicateRowId);
    QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(1));
    QCOMPARE(port.calls, 0);

    invalid.rows = {row(u"valid insert")};
    for (const int id : std::array<int, 2>{0, -7})
    {
        invalid.deletedIds = {Domain::NativeEnglishTeacherId(id)};
        outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
            execute(invalid, port, normalizedTestNameKey);
        QVERIFY(!outcome.saved);
        QVERIFY(!outcome.error.has_value());
        QCOMPARE(
            outcome.validation.issue,
            Application::NativeEnglishTeacherDirectorySaveIssue::
                InvalidDeletedId);
        QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(0));
        QCOMPARE(port.calls, 0);
    }
}

QTEST_APPLESS_MAIN(NextApplicationNativeEnglishTeacherDirectorySaveTests)

#include "next_application_native_english_teacher_directory_save_tests.moc"
