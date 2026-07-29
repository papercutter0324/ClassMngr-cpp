#include "ui/shared/widgets/sectioncards/class_time_row.h"

#include <QComboBox>
#include <QtTest>

namespace
{
QStringList comboItems(
    const QComboBox* combo
    )
{
    QStringList items;
    items.reserve(combo->count());

    for (int index = 0; index < combo->count(); ++index)
    {
        items.append(combo->itemText(index));
    }

    return items;
}
}

class ClassTimeRowTests : public QObject
{
    Q_OBJECT

private slots:
    void defaultsToFiftyFiveMinutesLater();
    void endOptionsUseSupportedDurationsAndRespectLatestEndTime();
    void changingStartHourPreservesSelectedDuration();
    void changingStartMinutesPreservesSelectedDuration();
    void changingEndTimeDoesNotChangeStartTime();
};

void ClassTimeRowTests::defaultsToFiftyFiveMinutesLater()
{
    ClassTimeRow row(ScheduleType::Regular);
    const QStringList expectedOptions{
        QStringLiteral("4:55 PM"),
        QStringLiteral("5:25 PM"),
        QStringLiteral("5:55 PM"),
        QStringLiteral("6:55 PM"),
        QStringLiteral("7:55 PM")
    };

    QCOMPARE(row.startTime(), QStringLiteral("4:00 PM"));
    QCOMPARE(row.endTime(), QStringLiteral("4:55 PM"));
    QCOMPARE(
        comboItems(row.endCombo()),
        expectedOptions
        );
}

void ClassTimeRowTests
    ::endOptionsUseSupportedDurationsAndRespectLatestEndTime()
{
    ClassTimeRow row(ScheduleType::Regular);
    row.setStartTime(QStringLiteral("8:00 PM"));
    const QStringList expectedOptions{
        QStringLiteral("8:55 PM"),
        QStringLiteral("9:25 PM"),
        QStringLiteral("9:55 PM")
    };

    QCOMPARE(
        comboItems(row.endCombo()),
        expectedOptions
        );
}

void ClassTimeRowTests::changingStartHourPreservesSelectedDuration()
{
    ClassTimeRow row(ScheduleType::Regular);
    row.setEndTime(QStringLiteral("6:55 PM"));

    row.setStartTime(QStringLiteral("5:00 PM"));

    QCOMPARE(row.endTime(), QStringLiteral("7:55 PM"));
}

void ClassTimeRowTests::changingStartMinutesPreservesSelectedDuration()
{
    ClassTimeRow row(ScheduleType::Regular);
    row.setEndTime(QStringLiteral("6:55 PM"));

    row.setStartTime(QStringLiteral("4:05 PM"));

    QCOMPARE(row.endTime(), QStringLiteral("7:00 PM"));
}

void ClassTimeRowTests::changingEndTimeDoesNotChangeStartTime()
{
    ClassTimeRow row(ScheduleType::Regular);

    row.setEndTime(QStringLiteral("6:55 PM"));

    QCOMPARE(row.startTime(), QStringLiteral("4:00 PM"));
    QCOMPARE(row.endTime(), QStringLiteral("6:55 PM"));
}

QTEST_MAIN(ClassTimeRowTests)

#include "class_time_row_tests.moc"
