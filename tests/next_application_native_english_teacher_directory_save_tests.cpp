#include "next/application/native_english_teacher_directory_save_use_case.h"

#include <QtTest/QtTest>

#include <utility>

using namespace ClassMngr::Next;

namespace
{

Application::NativeEnglishTeacherDirectorySaveRow row(
    std::u16string key,
    const bool birthdayIsBlank = true,
    const bool birthdayIsValid = false
    )
{
    Application::NativeEnglishTeacherDirectorySaveRow value;
    value.name = key;
    value.normalizedNameKey = std::move(key);
    value.birthdayIsBlank = birthdayIsBlank;
    value.birthdayIsValid = birthdayIsValid;
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
    void acceptsBlankAndQtValidatedBirthdayFacts();
    void rejectsEmptyAndDuplicateComparisonKeys();
    void rejectsInvalidNonblankBirthday();
    void reportsTheFirstInvalidRow();
    void useCaseCallsThePortOnceForValidInputAndNeverForInvalidInput();
};

void NextApplicationNativeEnglishTeacherDirectorySaveTests::
acceptsBlankAndQtValidatedBirthdayFacts()
{
    Application::NativeEnglishTeacherDirectorySaveRequest request;
    request.rows = {
        row(u"coordinator", true, false),
        row(u"leap-year-birthday", false, true)
    };

    const auto validation =
        Application::validateNativeEnglishTeacherDirectorySave(request);

    QVERIFY(validation.isValid());
    QCOMPARE(
        validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::None);
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
rejectsInvalidNonblankBirthday()
{
    Application::NativeEnglishTeacherDirectorySaveRequest request;
    auto invalid = row(u"teacher", false, false);
    invalid.birthday = u"02-30";
    request.rows = {invalid};

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
        row(u"valid", true, false),
        row(u"", false, false),
        row(u"later-invalid-date", false, false)
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
    invalid.rows = {row(u"", false, false)};
    outcome = Application::NativeEnglishTeacherDirectorySaveUseCase::
        execute(invalid, port);
    QVERIFY(!outcome.saved);
    QCOMPARE(
        outcome.validation.issue,
        Application::NativeEnglishTeacherDirectorySaveIssue::EmptyName);
    QCOMPARE(port.calls, 1);
}

QTEST_APPLESS_MAIN(NextApplicationNativeEnglishTeacherDirectorySaveTests)

#include "next_application_native_english_teacher_directory_save_tests.moc"
