#include "next/application/notification_port.h"
#include "next/application/upcoming_birthday_dismissal_port.h"
#include "next/ui/upcoming_birthdays_flow.h"

#include <QDate>
#include <QtTest/QtTest>

#include <string>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

template<class Snapshot>
class ReadPortStub;

template<>
class ReadPortStub<Application::KoreanTeacherBirthdayDirectorySnapshot> final
    : public Application::KoreanTeacherBirthdayDirectoryReadPort
{
public:
    Application::KoreanTeacherBirthdayDirectoryReadResult result =
        Application::KoreanTeacherBirthdayDirectoryReadResult::success({});
    mutable int calls = 0;

    Application::KoreanTeacherBirthdayDirectoryReadResult
    readKoreanTeacherBirthdayDirectory() const override
    {
        ++calls;
        return result;
    }
};

template<>
class ReadPortStub<Application::NativeEnglishTeacherBirthdayDirectorySnapshot> final
    : public Application::NativeEnglishTeacherBirthdayDirectoryReadPort
{
public:
    Application::NativeEnglishTeacherBirthdayDirectoryReadResult result =
        Application::NativeEnglishTeacherBirthdayDirectoryReadResult::success({});
    mutable int calls = 0;

    Application::NativeEnglishTeacherBirthdayDirectoryReadResult
    readNativeEnglishTeacherBirthdayDirectory() const override
    {
        ++calls;
        return result;
    }
};

template<>
class ReadPortStub<Application::GsTeamBirthdayDirectorySnapshot> final
    : public Application::GsTeamBirthdayDirectoryReadPort
{
public:
    Application::GsTeamBirthdayDirectoryReadResult result =
        Application::GsTeamBirthdayDirectoryReadResult::success({});
    mutable int calls = 0;

    Application::GsTeamBirthdayDirectoryReadResult
    readGsTeamBirthdayDirectory() const override
    {
        ++calls;
        return result;
    }
};

class DismissalStub final : public Application::UpcomingBirthdayDismissalPort
{
public:
    mutable std::vector<Application::CalendarEventDate> writes;
    Domain::Result<void> result = Domain::Result<void>::success();

    Domain::Result<void> write(
        const Application::CalendarEventDate& date
        ) const override
    {
        writes.push_back(date);
        return result;
    }
};

class NotificationStub final : public Application::NotificationPort
{
public:
    std::vector<Application::NotificationRequest> notices;

    Domain::Result<void> show(
        const Application::NotificationRequest& request
        ) override
    {
        notices.push_back(request);
        return Domain::Result<void>::success();
    }

    Domain::Result<void> clear() override
    {
        return Domain::Result<void>::success();
    }
};

template<class Stub>
void failRead(Stub& stub, Domain::ErrorCode code, const char* message)
{
    stub.result = std::remove_reference_t<decltype(stub.result)>::failure({
        .code = code,
        .message = message,
        .recoverable = false
    });
}

} // namespace

class NextUpcomingBirthdaysFlowTests final : public QObject
{
    Q_OBJECT

private slots:
    void combinesThreeTypedDirectoriesAndBuildsTheExpectedBuckets();
    void emptySourcesStillShowTheTwoEmptyWeekMessages();
    void koreanNotFoundIsSilentAndStopsBeforeOtherReads();
    void koreanReadFailureWarnsAndPreventsTheDialog();
    void nativeFailureTakesPrecedenceWhenBothSecondaryReadsFail();
    void dismissalWriteUsesTheInvocationDateOnlyWhenChecked();
    void dismissalWriteFailureWarnsAfterTheDialogCloses();
};

void NextUpcomingBirthdaysFlowTests::
combinesThreeTypedDirectoriesAndBuildsTheExpectedBuckets()
{
    ReadPortStub<Application::KoreanTeacherBirthdayDirectorySnapshot> korean;
    korean.result = Application::KoreanTeacherBirthdayDirectoryReadResult::success({
        {
            .birthday = u"10-27",
            .teacherKr = u"한국 이름",
            .teacherEn = u"English Name",
            .preferredRomanization = u"Romanized",
            .preferredName = u" Preferred Name "
        },
        {
            .birthday = u"10-31",
            .teacherKr = u"한국 이름 2",
            .teacherEn = u"Second English",
            .preferredRomanization = u"",
            .preferredName = u""
        },
        {
            .birthday = u"11-02",
            .teacherKr = u"Fallback Korean",
            .teacherEn = u"",
            .preferredRomanization = u"",
            .preferredName = u""
        },
        {
            .birthday = u"1-2",
            .teacherKr = u"Invalid birthday",
            .teacherEn = u"Invalid birthday",
            .preferredRomanization = u"",
            .preferredName = u""
        }
    });
    ReadPortStub<Application::NativeEnglishTeacherBirthdayDirectorySnapshot> native;
    native.result = Application::NativeEnglishTeacherBirthdayDirectoryReadResult::success({
        {.name = u" Native Staff ", .position = u" NET ", .birthday = u"10-31"}
    });
    ReadPortStub<Application::GsTeamBirthdayDirectorySnapshot> gsTeam;
    gsTeam.result = Application::GsTeamBirthdayDirectoryReadResult::success({
        {
            .name = u"",
            .koreanName = u" GS Korean ",
            .position = u" Coordinator ",
            .birthday = u"11-08"
        }
    });
    DismissalStub dismissal;
    NotificationStub notifications;
    std::optional<Application::UpcomingBirthdaySchedule> shown;

    Ui::UpcomingBirthdaysFlow flow(
        korean,
        native,
        gsTeam,
        dismissal,
        notifications,
        [&shown](const Application::UpcomingBirthdaySchedule& schedule) {
            shown = schedule;
            return false;
        }
        );

    flow.run(QDate(2026, 10, 27));

    QVERIFY(shown.has_value());
    QCOMPARE(shown->today.size(), std::size_t(1));
    QCOMPARE(shown->today[0].displayName, std::u16string(u"Preferred Name"));
    QCOMPARE(shown->thisWeek.size(), std::size_t(2));
    QCOMPARE(shown->thisWeek[0].date.month(), 10);
    QCOMPARE(shown->thisWeek[0].date.day(), 31);
    QCOMPARE(shown->thisWeek[0].displayName, std::u16string(u"Native Staff"));
    QCOMPARE(shown->thisWeek[0].position, std::u16string(u"NET"));
    QCOMPARE(shown->thisWeek[1].displayName, std::u16string(u"Second English"));
    QCOMPARE(shown->nextWeek.size(), std::size_t(2));
    QCOMPARE(shown->nextWeek[0].date.day(), 2);
    QCOMPARE(shown->nextWeek[0].displayName, std::u16string(u"Fallback Korean"));
    QCOMPARE(shown->nextWeek[1].date.day(), 8);
    QCOMPARE(shown->nextWeek[1].displayName, std::u16string(u"GS Korean"));
    QCOMPARE(shown->nextWeek[1].position, std::u16string(u"Coordinator"));
    QCOMPARE(korean.calls, 1);
    QCOMPARE(native.calls, 1);
    QCOMPARE(gsTeam.calls, 1);
    QVERIFY(notifications.notices.empty());
    QVERIFY(dismissal.writes.empty());
}

void NextUpcomingBirthdaysFlowTests::
emptySourcesStillShowTheTwoEmptyWeekMessages()
{
    ReadPortStub<Application::KoreanTeacherBirthdayDirectorySnapshot> korean;
    ReadPortStub<Application::NativeEnglishTeacherBirthdayDirectorySnapshot> native;
    ReadPortStub<Application::GsTeamBirthdayDirectorySnapshot> gsTeam;
    DismissalStub dismissal;
    NotificationStub notifications;
    bool shownEmpty = false;

    Ui::UpcomingBirthdaysFlow flow(
        korean, native, gsTeam, dismissal, notifications,
        [&shownEmpty](const Application::UpcomingBirthdaySchedule& schedule) {
            shownEmpty = schedule.isEmpty();
            return false;
        }
        );
    flow.run(QDate(2026, 3, 10));

    QVERIFY(shownEmpty);
    QVERIFY(notifications.notices.empty());
}

void NextUpcomingBirthdaysFlowTests::
koreanNotFoundIsSilentAndStopsBeforeOtherReads()
{
    ReadPortStub<Application::KoreanTeacherBirthdayDirectorySnapshot> korean;
    failRead(korean, Domain::ErrorCode::NotFound, "No profile");
    ReadPortStub<Application::NativeEnglishTeacherBirthdayDirectorySnapshot> native;
    ReadPortStub<Application::GsTeamBirthdayDirectorySnapshot> gsTeam;
    DismissalStub dismissal;
    NotificationStub notifications;
    bool shown = false;
    Ui::UpcomingBirthdaysFlow flow(
        korean, native, gsTeam, dismissal, notifications,
        [&shown](const auto&) { shown = true; return false; }
        );

    flow.run(QDate(2026, 3, 10));

    QCOMPARE(korean.calls, 1);
    QCOMPARE(native.calls, 0);
    QCOMPARE(gsTeam.calls, 0);
    QVERIFY(!shown);
    QVERIFY(notifications.notices.empty());
}

void NextUpcomingBirthdaysFlowTests::
koreanReadFailureWarnsAndPreventsTheDialog()
{
    ReadPortStub<Application::KoreanTeacherBirthdayDirectorySnapshot> korean;
    failRead(korean, Domain::ErrorCode::Technical, "Korean read detail");
    ReadPortStub<Application::NativeEnglishTeacherBirthdayDirectorySnapshot> native;
    ReadPortStub<Application::GsTeamBirthdayDirectorySnapshot> gsTeam;
    DismissalStub dismissal;
    NotificationStub notifications;
    bool shown = false;
    Ui::UpcomingBirthdaysFlow flow(
        korean, native, gsTeam, dismissal, notifications,
        [&shown](const auto&) { shown = true; return false; }
        );

    flow.run(QDate(2026, 3, 10));

    QCOMPARE(notifications.notices.size(), std::size_t(1));
    QCOMPARE(notifications.notices[0].severity, Application::NotificationSeverity::Warning);
    QVERIFY(QString::fromStdString(notifications.notices[0].message)
        .contains(QStringLiteral("Korean read detail")));
    QCOMPARE(native.calls, 0);
    QCOMPARE(gsTeam.calls, 0);
    QVERIFY(!shown);
}

void NextUpcomingBirthdaysFlowTests::
nativeFailureTakesPrecedenceWhenBothSecondaryReadsFail()
{
    ReadPortStub<Application::KoreanTeacherBirthdayDirectorySnapshot> korean;
    ReadPortStub<Application::NativeEnglishTeacherBirthdayDirectorySnapshot> native;
    failRead(native, Domain::ErrorCode::Technical, "Native detail");
    ReadPortStub<Application::GsTeamBirthdayDirectorySnapshot> gsTeam;
    failRead(gsTeam, Domain::ErrorCode::Technical, "GS detail");
    DismissalStub dismissal;
    NotificationStub notifications;
    bool shown = false;
    Ui::UpcomingBirthdaysFlow flow(
        korean, native, gsTeam, dismissal, notifications,
        [&shown](const auto&) { shown = true; return false; }
        );

    flow.run(QDate(2026, 3, 10));

    QCOMPARE(native.calls, 1);
    QCOMPARE(gsTeam.calls, 1);
    QCOMPARE(notifications.notices.size(), std::size_t(1));
    const QString warning = QString::fromStdString(notifications.notices[0].message);
    QVERIFY(warning.contains(QStringLiteral("Native detail")));
    QVERIFY(!warning.contains(QStringLiteral("GS detail")));
    QVERIFY(!shown);
}

void NextUpcomingBirthdaysFlowTests::
dismissalWriteUsesTheInvocationDateOnlyWhenChecked()
{
    ReadPortStub<Application::KoreanTeacherBirthdayDirectorySnapshot> korean;
    ReadPortStub<Application::NativeEnglishTeacherBirthdayDirectorySnapshot> native;
    ReadPortStub<Application::GsTeamBirthdayDirectorySnapshot> gsTeam;
    DismissalStub dismissal;
    NotificationStub notifications;
    Ui::UpcomingBirthdaysFlow checked(
        korean, native, gsTeam, dismissal, notifications,
        [](const auto&) { return true; }
        );
    checked.run(QDate(2026, 2, 28));
    QCOMPARE(dismissal.writes.size(), std::size_t(1));
    QCOMPARE(QString::fromStdString(dismissal.writes[0].value()),
             QStringLiteral("2026-02-28"));

    dismissal.writes.clear();
    Ui::UpcomingBirthdaysFlow unchecked(
        korean, native, gsTeam, dismissal, notifications,
        [](const auto&) { return false; }
        );
    unchecked.run(QDate(2026, 2, 28));
    QVERIFY(dismissal.writes.empty());
}

void NextUpcomingBirthdaysFlowTests::
dismissalWriteFailureWarnsAfterTheDialogCloses()
{
    ReadPortStub<Application::KoreanTeacherBirthdayDirectorySnapshot> korean;
    ReadPortStub<Application::NativeEnglishTeacherBirthdayDirectorySnapshot> native;
    ReadPortStub<Application::GsTeamBirthdayDirectorySnapshot> gsTeam;
    DismissalStub dismissal;
    dismissal.result = Domain::Result<void>::failure({
        .code = Domain::ErrorCode::Technical,
        .message = "settings detail",
        .recoverable = true
    });
    NotificationStub notifications;
    bool dialogReturned = false;
    Ui::UpcomingBirthdaysFlow flow(
        korean, native, gsTeam, dismissal, notifications,
        [&dialogReturned](const auto&) {
            dialogReturned = true;
            return true;
        }
        );

    flow.run(QDate(2026, 10, 27));

    QVERIFY(dialogReturned);
    QCOMPARE(dismissal.writes.size(), std::size_t(1));
    QCOMPARE(notifications.notices.size(), std::size_t(1));
    QCOMPARE(notifications.notices[0].severity, Application::NotificationSeverity::Warning);
    QVERIFY(QString::fromStdString(notifications.notices[0].message)
        .contains(QStringLiteral("settings detail")));
}

QTEST_GUILESS_MAIN(NextUpcomingBirthdaysFlowTests)
#include "upcoming_birthdays_flow_tests.moc"
