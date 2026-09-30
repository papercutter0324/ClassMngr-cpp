#include "next/application/gs_team_directory_save_use_case.h"

#include <QtTest/QtTest>

#include <utility>

using namespace ClassMngr::Next;

namespace
{

Application::GsTeamDirectorySaveRow row(
    std::u16string englishKey,
    std::u16string koreanKey = {},
    const bool birthdayIsBlank = true,
    const bool birthdayIsValid = false
    )
{
    Application::GsTeamDirectorySaveRow value;
    value.name = englishKey;
    value.koreanName = koreanKey;
    value.normalizedEnglishNameKey = std::move(englishKey);
    value.normalizedKoreanNameKey = std::move(koreanKey);
    value.birthdayIsBlank = birthdayIsBlank;
    value.birthdayIsValid = birthdayIsValid;
    return value;
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
    void requiresAtLeastOneNameAndEnforcesEnglishUniqueness();
    void enforcesKoreanUniquenessWithinItsOwnNamespace();
    void allowsMatchingKeysAcrossNameNamespaces();
    void acceptsBlankOrQtValidatedBirthdayFacts();
    void reportsTheFirstInvalidRow();
    void callsThePortOnceForValidInputAndNeverForInvalidInput();
};

void NextApplicationGsTeamDirectorySaveTests::
requiresAtLeastOneNameAndEnforcesEnglishUniqueness()
{
    Application::GsTeamDirectorySaveRequest noNames;
    noNames.rows = {row(u"", u"")};
    auto validation = Application::validateGsTeamDirectorySave(noNames);
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::MissingNames);
    QCOMPARE(validation.rowIndex, std::size_t(0));

    Application::GsTeamDirectorySaveRequest duplicateEnglish;
    duplicateEnglish.rows = {
        row(u"sarah", u"\uC0AC\uB77C"),
        row(u"sarah", u"\uC774\uC601")
    };
    validation = Application::validateGsTeamDirectorySave(duplicateEnglish);
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::DuplicateEnglishName);
    QCOMPARE(validation.rowIndex, std::size_t(1));
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
        Application::validateGsTeamDirectorySave(duplicateKorean);
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::DuplicateKoreanName);
    QCOMPARE(validation.rowIndex, std::size_t(1));
}

void NextApplicationGsTeamDirectorySaveTests::
allowsMatchingKeysAcrossNameNamespaces()
{
    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        row(u"same", u"\uAC00"),
        row(u"different", u"same")
    };

    const auto validation = Application::validateGsTeamDirectorySave(request);
    QVERIFY(validation.isValid());
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::None);
}

void NextApplicationGsTeamDirectorySaveTests::
acceptsBlankOrQtValidatedBirthdayFacts()
{
    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        row(u"blank birthday", u"", true, false),
        row(u"leap day", u"", false, true)
    };
    request.rows.back().birthday = u"02-29";

    auto validation = Application::validateGsTeamDirectorySave(request);
    QVERIFY(validation.isValid());

    request.rows.back().birthday = u"02-30";
    request.rows.back().birthdayIsValid = false;
    validation = Application::validateGsTeamDirectorySave(request);
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::InvalidBirthday);
    QCOMPARE(validation.rowIndex, std::size_t(1));
}

void NextApplicationGsTeamDirectorySaveTests::reportsTheFirstInvalidRow()
{
    Application::GsTeamDirectorySaveRequest request;
    request.rows = {
        row(u"valid", u"\uAC00"),
        row(u"duplicate", u"\uB098"),
        row(u"duplicate", u"\uB2E4", false, false),
        row(u"", u"")
    };

    const auto validation = Application::validateGsTeamDirectorySave(request);
    QCOMPARE(
        validation.issue,
        Application::GsTeamDirectorySaveIssue::DuplicateEnglishName);
    QCOMPARE(validation.rowIndex, std::size_t(2));
}

void NextApplicationGsTeamDirectorySaveTests::
callsThePortOnceForValidInputAndNeverForInvalidInput()
{
    Application::GsTeamDirectorySaveRequest valid;
    valid.rows = {row(u"Alex", u"\uC54C\uB809\uC2A4")};
    valid.rows.front().id = Domain::GsTeamMemberId(42);
    valid.deletedIds = {Domain::GsTeamMemberId(31)};

    RecordingSavePort port;
    auto outcome =
        Application::GsTeamDirectorySaveUseCase::execute(valid, port);
    QVERIFY(outcome.saved);
    QVERIFY(outcome.validation.isValid());
    QVERIFY(!outcome.error.has_value());
    QCOMPARE(port.calls, 1);
    QVERIFY(port.received == valid);

    Application::GsTeamDirectorySaveRequest invalid;
    invalid.rows = {row(u"", u"")};
    outcome = Application::GsTeamDirectorySaveUseCase::execute(invalid, port);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::GsTeamDirectorySaveIssue::MissingNames);
    QCOMPARE(port.calls, 1);
}

QTEST_APPLESS_MAIN(NextApplicationGsTeamDirectorySaveTests)

#include "next_application_gs_team_directory_save_tests.moc"
