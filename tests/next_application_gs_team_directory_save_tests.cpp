#include "next/application/gs_team_directory_save_use_case.h"

#include <QtTest/QtTest>

#include <array>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace ClassMngr::Next;

using GsTeamDirectorySaveValidationRowIndex =
    Application::GsTeamDirectorySaveValidationRowIndex;

static_assert(std::is_same_v<
    decltype(Application::GsTeamDirectorySaveValidation{}.rowIndex),
    GsTeamDirectorySaveValidationRowIndex
    >);
static_assert(std::is_same_v<
    decltype(GsTeamDirectorySaveValidationRowIndex{}.rawValue()),
    std::size_t
    >);
static_assert(std::is_constructible_v<
    GsTeamDirectorySaveValidationRowIndex,
    std::size_t
    >);
static_assert(!std::is_convertible_v<
    std::size_t,
    GsTeamDirectorySaveValidationRowIndex
    >);
static_assert(!std::is_convertible_v<
    GsTeamDirectorySaveValidationRowIndex,
    std::size_t
    >);
static_assert(!std::is_aggregate_v<GsTeamDirectorySaveValidationRowIndex>);

namespace
{

Application::GsTeamDirectorySaveRow row(
    std::u16string englishName,
    std::u16string koreanName = {},
    std::u16string birthday = {}
    )
{
    Application::GsTeamDirectorySaveRow value;
    value.name = std::move(englishName);
    value.koreanName = std::move(koreanName);
    value.birthday = std::move(birthday);
    return value;
}

std::u16string testNameKey(const std::u16string_view rawName)
{
    std::u16string key;
    bool appendSpace = false;
    for (char16_t character : rawName)
    {
        if (character == u' ' || character == u'\t'
            || character == u'\r' || character == u'\n')
        {
            appendSpace = !key.empty();
            continue;
        }
        if (appendSpace)
        {
            key.push_back(u' ');
            appendSpace = false;
        }
        if (character >= u'A' && character <= u'Z')
        {
            character = static_cast<char16_t>(character + (u'a' - u'A'));
        }
        key.push_back(character);
    }
    return key;
}

class RecordingSavePort final : public Application::GsTeamDirectorySavePort
{
public:
    [[nodiscard]] Domain::Result<void> saveGsTeamDirectory(
        const Application::GsTeamDirectorySaveRequest& request
        ) const override
    {
        ++calls;
        received = request;
        return result;
    }

    mutable int calls = 0;
    mutable Application::GsTeamDirectorySaveRequest received;
    Domain::Result<void> result = Domain::Result<void>::success();
};

}

class NextApplicationGsTeamDirectorySaveTests final : public QObject
{
    Q_OBJECT

private slots:
    void validationRowIndexHasTypedValueSemantics();
    void requiresAtLeastOneNameAndEnforcesEnglishUniqueness();
    void enforcesKoreanUniquenessWithinItsOwnNamespace();
    void allowsMatchingKeysAcrossNameNamespaces();
    void acceptsBlankAndLeapDayBirthdayText();
    void rejectsInvalidCalendarDay();
    void reportsTheFirstInvalidRow();
    void callsThePortOnceForValidInputAndNeverForInvalidInput();
    void rejectsInvalidRowAndDeletedIdsBeforeCallingPort();
    void usesRawNamesForValidationBeforeCallingPort();
};

void NextApplicationGsTeamDirectorySaveTests::
validationRowIndexHasTypedValueSemantics()
{
    const GsTeamDirectorySaveValidationRowIndex zero;
    const GsTeamDirectorySaveValidationRowIndex positive{7};

    QCOMPARE(zero.rawValue(), std::size_t(0));
    QCOMPARE(positive.rawValue(), std::size_t(7));
    QVERIFY(zero == GsTeamDirectorySaveValidationRowIndex{});
    QVERIFY(positive == GsTeamDirectorySaveValidationRowIndex{7});
    QVERIFY(zero != positive);
}

void NextApplicationGsTeamDirectorySaveTests::
requiresAtLeastOneNameAndEnforcesEnglishUniqueness()
{
    Application::GsTeamDirectorySaveRequest noNames;
    noNames.rows = {row(u"", u"")};
    auto validation = Application::validateGsTeamDirectorySave(
        noNames,
        testNameKey);
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::MissingNames);
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(0));

    Application::GsTeamDirectorySaveRequest duplicateEnglish;
    duplicateEnglish.rows = {
        row(u"sarah", u"\uC0AC\uB77C"),
        row(u"sarah", u"\uC774\uC601")
    };
    validation = Application::validateGsTeamDirectorySave(
        duplicateEnglish,
        testNameKey);
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::DuplicateEnglishName);
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(1));
}

void NextApplicationGsTeamDirectorySaveTests::
enforcesKoreanUniquenessWithinItsOwnNamespace()
{
    Application::GsTeamDirectorySaveRequest duplicateKorean;
    duplicateKorean.rows = {
        row(u"Alex", u"\uC11C\uC5F0"),
        row(u"Bea", u"\uC11C\uC5F0")
    };

    const auto validation =
        Application::validateGsTeamDirectorySave(
            duplicateKorean,
            testNameKey);
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::DuplicateKoreanName);
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(1));
}

void NextApplicationGsTeamDirectorySaveTests::
allowsMatchingKeysAcrossNameNamespaces()
{
    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        row(u"same", u"\uAC00"),
        row(u"different", u"same")
    };

    const auto validation = Application::validateGsTeamDirectorySave(
        request,
        testNameKey);
    QVERIFY(validation.isValid());
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::None);
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(0));
}

void NextApplicationGsTeamDirectorySaveTests::
acceptsBlankAndLeapDayBirthdayText()
{
    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        row(u"blank birthday", u"", u"\u00a0 \t"),
        row(u"leap day", u"", u"02-29")
    };

    auto validation = Application::validateGsTeamDirectorySave(
        request,
        testNameKey);
    QVERIFY(validation.isValid());
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(0));
}

void NextApplicationGsTeamDirectorySaveTests::
rejectsInvalidCalendarDay()
{
    Application::GsTeamDirectorySaveRequest request;
    request.rows = {row(u"member", u"", u"02-30")};

    const auto validation = Application::validateGsTeamDirectorySave(
        request,
        testNameKey);
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::InvalidBirthday);
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(0));
}

void NextApplicationGsTeamDirectorySaveTests::reportsTheFirstInvalidRow()
{
    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        row(u"valid", u"\uAC00"),
        row(u"duplicate", u"\uB098"),
        row(u"duplicate", u"\uB2E4", u"02-30"),
        row(u"", u"")
    };

    const auto validation = Application::validateGsTeamDirectorySave(
        request,
        testNameKey);
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::DuplicateEnglishName);
    QCOMPARE(validation.rowIndex.rawValue(), std::size_t(2));
}

void NextApplicationGsTeamDirectorySaveTests::
callsThePortOnceForValidInputAndNeverForInvalidInput()
{
    Application::GsTeamDirectorySaveRequest valid;
    valid.rows = {
        row(u"Alex", u"\uC54C\uB809\uC2A4"),
        row(u"Bea", u"\uBE44\uC544")
    };
    valid.rows.front().id = Domain::GsTeamMemberId(42);
    valid.deletedIds = {
        Domain::GsTeamMemberId(31),
        Domain::GsTeamMemberId(31)
    };

    RecordingSavePort port;
    auto outcome =
        Application::GsTeamDirectorySaveUseCase::execute(
            valid,
            port,
            testNameKey);
    QVERIFY(outcome.saved);
    QVERIFY(outcome.validation.isValid());
    QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(0));
    QVERIFY(!outcome.error.has_value());
    QCOMPARE(port.calls, 1);
    QVERIFY(port.received == valid);

    Application::GsTeamDirectorySaveRequest invalid;
    invalid.rows = {row(u"", u"")};
    outcome = Application::GsTeamDirectorySaveUseCase::execute(
        invalid,
        port,
        testNameKey);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::GsTeamDirectorySaveIssue::MissingNames);
    QCOMPARE(port.calls, 1);

    invalid.rows = {row(u"Alex", u"\uC54C\uB809\uC2A4", u"02-30")};
    outcome = Application::GsTeamDirectorySaveUseCase::execute(
        invalid,
        port,
        testNameKey);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::GsTeamDirectorySaveIssue::InvalidBirthday);
    QCOMPARE(port.calls, 1);
}

void NextApplicationGsTeamDirectorySaveTests::
rejectsInvalidRowAndDeletedIdsBeforeCallingPort()
{
    RecordingSavePort port;
    Application::GsTeamDirectorySaveRequest invalid;
    Application::GsTeamDirectorySaveOutcome outcome;

    for (const int id : std::array<int, 2>{0, -7})
    {
        invalid.rows = {row(u"valid row", u"\uD658\uC601")};
        invalid.rows.front().id = Domain::GsTeamMemberId(id);
        outcome = Application::GsTeamDirectorySaveUseCase::execute(
            invalid,
            port,
            testNameKey);
        QVERIFY(!outcome.saved);
        QVERIFY(!outcome.error.has_value());
        QCOMPARE(
            outcome.validation.issue,
            Application::GsTeamDirectorySaveIssue::InvalidRowId);
        QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(0));
        QCOMPARE(port.calls, 0);
    }

    invalid.rows = {
        row(u"first row", u"\uCCAB\uC9F8"),
        row(u"second row", u"\uB458\uC9F8")
    };
    invalid.rows[0].id = Domain::GsTeamMemberId(42);
    invalid.rows[1].id = Domain::GsTeamMemberId(42);
    outcome = Application::GsTeamDirectorySaveUseCase::execute(
        invalid,
        port,
        testNameKey);
    QVERIFY(!outcome.saved);
    QVERIFY(!outcome.error.has_value());
    QCOMPARE(
        outcome.validation.issue,
        Application::GsTeamDirectorySaveIssue::DuplicateRowId);
    QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(1));
    QCOMPARE(port.calls, 0);

    invalid.rows = {row(u"valid insert", u"\uC0C8\uB85C")};
    for (const int id : std::array<int, 2>{0, -7})
    {
        invalid.deletedIds = {Domain::GsTeamMemberId(id)};
        outcome = Application::GsTeamDirectorySaveUseCase::execute(
            invalid,
            port,
            testNameKey);
        QVERIFY(!outcome.saved);
        QVERIFY(!outcome.error.has_value());
        QCOMPARE(
            outcome.validation.issue,
            Application::GsTeamDirectorySaveIssue::InvalidDeletedId);
        QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(0));
        QCOMPARE(port.calls, 0);
    }
}

void NextApplicationGsTeamDirectorySaveTests::
usesRawNamesForValidationBeforeCallingPort()
{
    RecordingSavePort port;
    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        row(u"  Taylor  James ", u"\uC11C\uC5F0"),
        row(u"taylor james", u"\uC774\uC601")
    };

    auto outcome = Application::GsTeamDirectorySaveUseCase::execute(
        request,
        port,
        testNameKey);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::GsTeamDirectorySaveIssue::DuplicateEnglishName);
    QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(1));
    QCOMPARE(port.calls, 0);

    request.rows = {
        row(u"Alex", u"  \uC11C\uC5F0   "),
        row(u"Bea", u"\uC11C\uC5F0")
    };
    outcome = Application::GsTeamDirectorySaveUseCase::execute(
        request,
        port,
        testNameKey);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::GsTeamDirectorySaveIssue::DuplicateKoreanName);
    QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(1));
    QCOMPARE(port.calls, 0);

    request.rows = {row(u" \t ", u"\r\n ")};
    outcome = Application::GsTeamDirectorySaveUseCase::execute(
        request,
        port,
        testNameKey);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::GsTeamDirectorySaveIssue::MissingNames);
    QCOMPARE(outcome.validation.rowIndex.rawValue(), std::size_t(0));
    QCOMPARE(port.calls, 0);
}

QTEST_APPLESS_MAIN(NextApplicationGsTeamDirectorySaveTests)

#include "next_application_gs_team_directory_save_tests.moc"
