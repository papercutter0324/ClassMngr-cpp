#include "features/teacher/ui/upcoming_birthdays_dialog.h"
#include "features/teacher/upcoming_birthday_schedule.h"
#include "ui/shared/actions/action_registry.h"

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QtTest>

namespace
{
Teacher teacher(const QString& name, const QString& birthday)
{
    Teacher result;
    result.teacherEn = name;
    result.birthday = birthday;
    return result;
}

NativeEnglishTeacher nativeTeacher(
    const QString& name,
    const QString& position,
    const QString& birthday
    )
{
    NativeEnglishTeacher result;
    result.name = name;
    result.position = position;
    result.birthday = birthday;
    return result;
}

GsTeamMember gsTeamMember(
    const QString& name,
    const QString& koreanName,
    const QString& position,
    const QString& birthday
    )
{
    GsTeamMember result;
    result.name = name;
    result.koreanName = koreanName;
    result.position = position;
    result.birthday = birthday;
    return result;
}
}

class UpcomingBirthdaysTests final : public QObject
{
    Q_OBJECT

private slots:
    void combinesStaffAndSeparatesToday();
    void ignoresInvalidAndPastBirthdaysAndSortsNames();
    void crossesCalendarYears();
    void mapsLeapDayToFebruaryTwentyEighthInNonLeapYears();
    void dialogCentersTodayAndShowsWeeklyEmptyState();
    void actionIsCreatedWithTeacherMenuText();
};

void UpcomingBirthdaysTests::combinesStaffAndSeparatesToday()
{
    const UpcomingBirthdaySchedule schedule = UpcomingBirthdaySchedule::build(
        {teacher(QStringLiteral("Alex"), QStringLiteral("08-21"))},
        {nativeTeacher(
            QStringLiteral("Blair"),
            QStringLiteral("NET"),
            QStringLiteral("08-22"))},
        {gsTeamMember(
            QStringLiteral("Casey"),
            QStringLiteral("케이시"),
            QStringLiteral("M3"),
            QStringLiteral("08-24"))},
        QDate(2026, 8, 21)
        );

    QCOMPARE(schedule.today.size(), 1);
    QCOMPARE(schedule.today.first().displayName, QStringLiteral("Alex"));
    QCOMPARE(
        static_cast<int>(schedule.today.first().group),
        static_cast<int>(UpcomingBirthdayGroup::KoreanTeacher)
        );

    QCOMPARE(schedule.thisWeek.size(), 1);
    QCOMPARE(schedule.thisWeek.first().date, QDate(2026, 8, 22));
    QCOMPARE(
        static_cast<int>(schedule.thisWeek.first().group),
        static_cast<int>(UpcomingBirthdayGroup::NativeEnglishTeacher)
        );
    QCOMPARE(schedule.thisWeek.first().position, QStringLiteral("NET"));

    QCOMPARE(schedule.nextWeek.size(), 1);
    QCOMPARE(schedule.nextWeek.first().date, QDate(2026, 8, 24));
    QCOMPARE(
        static_cast<int>(schedule.nextWeek.first().group),
        static_cast<int>(UpcomingBirthdayGroup::GsTeam)
        );
    QCOMPARE(schedule.nextWeek.first().position, QStringLiteral("M3"));
}

void UpcomingBirthdaysTests::ignoresInvalidAndPastBirthdaysAndSortsNames()
{
    const UpcomingBirthdaySchedule schedule = UpcomingBirthdaySchedule::build(
        {
            teacher(QStringLiteral("Zara"), QStringLiteral("08-18")),
            teacher(QStringLiteral("Bella"), QStringLiteral("08-19")),
            teacher(QStringLiteral("Alex"), QStringLiteral("08-19")),
            teacher(QString(), QStringLiteral("08-20")),
            teacher(QStringLiteral("Invalid"), QStringLiteral("02-30"))
        },
        {},
        {
            gsTeamMember(
                QString(),
                QStringLiteral("한국 이름"),
                QStringLiteral("Branch Manager"),
                QStringLiteral("08-20"))
        },
        QDate(2026, 8, 19)
        );

    QCOMPARE(schedule.today.size(), 2);
    QCOMPARE(schedule.today.at(0).displayName, QStringLiteral("Alex"));
    QCOMPARE(schedule.today.at(1).displayName, QStringLiteral("Bella"));

    QCOMPARE(schedule.thisWeek.size(), 1);
    QCOMPARE(schedule.thisWeek.first().displayName, QStringLiteral("한국 이름"));
    QCOMPARE(schedule.thisWeek.first().date, QDate(2026, 8, 20));
    QVERIFY(schedule.nextWeek.isEmpty());
}

void UpcomingBirthdaysTests::crossesCalendarYears()
{
    const UpcomingBirthdaySchedule schedule = UpcomingBirthdaySchedule::build(
        {
            teacher(QStringLiteral("This Week"), QStringLiteral("01-03")),
            teacher(QStringLiteral("Next Week"), QStringLiteral("01-04"))
        },
        {},
        {},
        QDate(2026, 12, 28)
        );

    QCOMPARE(schedule.thisWeek.size(), 1);
    QCOMPARE(schedule.thisWeek.first().date, QDate(2027, 1, 3));
    QCOMPARE(schedule.nextWeek.size(), 1);
    QCOMPARE(schedule.nextWeek.first().date, QDate(2027, 1, 4));
}

void UpcomingBirthdaysTests::mapsLeapDayToFebruaryTwentyEighthInNonLeapYears()
{
    const UpcomingBirthdaySchedule schedule = UpcomingBirthdaySchedule::build(
        {teacher(QStringLiteral("Leap Day"), QStringLiteral("02-29"))},
        {},
        {},
        QDate(2027, 2, 22)
        );

    QCOMPARE(schedule.thisWeek.size(), 1);
    QCOMPARE(schedule.thisWeek.first().date, QDate(2027, 2, 28));
}

void UpcomingBirthdaysTests::dialogCentersTodayAndShowsWeeklyEmptyState()
{
    UpcomingBirthdaySchedule schedule;
    schedule.today.append({
        QDate(2026, 8, 21),
        QStringLiteral("Alex"),
        {},
        UpcomingBirthdayGroup::KoreanTeacher
    });
    schedule.nextWeek.append({
        QDate(2026, 8, 24),
        QStringLiteral("Casey"),
        QStringLiteral("M3"),
        UpcomingBirthdayGroup::GsTeam
    });

    UpcomingBirthdaysDialog dialog(schedule);

    auto* todaySection = dialog.findChild<QWidget*>(
        QStringLiteral("upcomingBirthdaysTodaySection"));
    auto* todayName = dialog.findChild<QLabel*>(
        QStringLiteral("upcomingBirthdaysTodayEntry0Name"));
    auto* thisWeekTitle = dialog.findChild<QLabel*>(
        QStringLiteral("upcomingBirthdaysThisWeekTitle"));
    auto* nextWeekTitle = dialog.findChild<QLabel*>(
        QStringLiteral("upcomingBirthdaysNextWeekTitle"));
    auto* thisWeekEmpty = dialog.findChild<QLabel*>(
        QStringLiteral("upcomingBirthdaysThisWeekEmpty"));
    auto* nextWeekDetail = dialog.findChild<QLabel*>(
        QStringLiteral("upcomingBirthdaysNextWeekEntry0Detail"));
    auto* nextWeekName = dialog.findChild<QLabel*>(
        QStringLiteral("upcomingBirthdaysNextWeekEntry0Name"));
    auto* close = dialog.findChild<QPushButton*>(
        QStringLiteral("upcomingBirthdaysCloseButton"));

    QVERIFY(todaySection);
    QVERIFY(!todaySection->isHidden());
    QVERIFY(todayName);
    QCOMPARE(todayName->alignment(), Qt::AlignCenter);
    QVERIFY(nextWeekName);
    QCOMPARE(
        todayName->font().pointSize(),
        nextWeekName->font().pointSize() + 6
        );
    QVERIFY(thisWeekTitle);
    QCOMPARE(thisWeekTitle->text(), QStringLiteral("This Week"));
    QVERIFY(nextWeekTitle);
    QCOMPARE(nextWeekTitle->text(), QStringLiteral("Next Week"));
    QVERIFY(thisWeekEmpty);
    QCOMPARE(thisWeekEmpty->text(), QStringLiteral("No birthdays this week."));
    QVERIFY(nextWeekDetail);
    QVERIFY(nextWeekDetail->text().contains(QStringLiteral("GS Team")));
    QVERIFY(nextWeekDetail->text().contains(QStringLiteral("M3")));
    QVERIFY(close);

    QSignalSpy finished(&dialog, &QDialog::finished);
    dialog.show();
    QTRY_VERIFY(close->isVisible());
    close->click();
    QTRY_COMPARE(finished.count(), 1);
}

void UpcomingBirthdaysTests::actionIsCreatedWithTeacherMenuText()
{
    ActionRegistry actions;
    actions.createActions();

    QVERIFY(actions.upcomingBirthdays);
    QCOMPARE(
        actions.upcomingBirthdays->text(),
        QStringLiteral("Upcoming Birthdays...")
        );
}

QTEST_MAIN(UpcomingBirthdaysTests)

#include "upcoming_birthdays_tests.moc"
