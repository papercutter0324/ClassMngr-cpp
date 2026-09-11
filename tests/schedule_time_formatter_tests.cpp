#include "features/schedule/ui/schedule_time_formatter.h"

#include <classmngr/engine/schedule_report.h>

#include <QtTest>

#include <string>

namespace
{
QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}
}

class ScheduleTimeFormatterTests : public QObject
{
    Q_OBJECT

private slots:
    void displayTimeMatchesEngine_data();
    void displayTimeMatchesEngine();
    void rangeLabelMatchesEngine_data();
    void rangeLabelMatchesEngine();
};

void ScheduleTimeFormatterTests::displayTimeMatchesEngine_data()
{
    QTest::addColumn<QString>("timeLabel");
    QTest::addColumn<bool>("use24h");
    QTest::addColumn<QString>("expected");

    QTest::newRow("midnight 12-hour")
        << QStringLiteral("00:00")
        << false
        << QStringLiteral("12:00 AM");
    QTest::newRow("noon 12-hour")
        << QStringLiteral("12:00")
        << false
        << QStringLiteral("12:00 PM");
    QTest::newRow("morning 24-hour")
        << QStringLiteral("09:05")
        << true
        << QStringLiteral("09:05");
    QTest::newRow("afternoon 12-hour")
        << QStringLiteral("16:00")
        << false
        << QStringLiteral("4:00 PM");
    QTest::newRow("invalid text preserves UTF-8")
        << QString::fromUtf8("\xEC\x8B\x9C\xEA\xB0\x84")
        << false
        << QString::fromUtf8("\xEC\x8B\x9C\xEA\xB0\x84");
}

void ScheduleTimeFormatterTests::displayTimeMatchesEngine()
{
    QFETCH(QString, timeLabel);
    QFETCH(bool, use24h);
    QFETCH(QString, expected);

    const std::string engineOutput =
        classmngr::engine::ScheduleReportService::displayTime(
            timeLabel.toUtf8().toStdString(),
            use24h
            );

    QCOMPARE(fromUtf8(engineOutput), expected);
    QCOMPARE(
        ScheduleTimeFormatter::displayTime(timeLabel, use24h),
        fromUtf8(engineOutput)
        );
}

void ScheduleTimeFormatterTests::rangeLabelMatchesEngine_data()
{
    QTest::addColumn<QString>("startLabel");
    QTest::addColumn<bool>("uses55Endings");
    QTest::addColumn<bool>("use24h");
    QTest::addColumn<QString>("expected");

    QTest::newRow("same period 50-minute ending")
        << QStringLiteral("16:00")
        << false
        << false
        << QStringLiteral("4:00 -\n4:50 PM");
    QTest::newRow("same period 55-minute ending")
        << QStringLiteral("16:00")
        << true
        << false
        << QStringLiteral("4:00 -\n4:55 PM");
    QTest::newRow("24-hour 55-minute ending")
        << QStringLiteral("16:00")
        << true
        << true
        << QStringLiteral("16:00 - 16:55");
    QTest::newRow("different period")
        << QStringLiteral("11:05")
        << true
        << false
        << QStringLiteral("11:05 AM\n- 12:00 PM");
    QTest::newRow("cross midnight")
        << QStringLiteral("23:30")
        << false
        << false
        << QStringLiteral("11:30 PM\n- 12:20 AM");
    QTest::newRow("invalid text preserves UTF-8")
        << QString::fromUtf8("\xEC\x8B\x9C\xEA\xB0\x84")
        << true
        << true
        << QString::fromUtf8("\xEC\x8B\x9C\xEA\xB0\x84");
}

void ScheduleTimeFormatterTests::rangeLabelMatchesEngine()
{
    QFETCH(QString, startLabel);
    QFETCH(bool, uses55Endings);
    QFETCH(bool, use24h);
    QFETCH(QString, expected);

    const std::string engineOutput =
        classmngr::engine::ScheduleReportService::rangeLabel(
            startLabel.toUtf8().toStdString(),
            uses55Endings,
            use24h
            );

    QCOMPARE(fromUtf8(engineOutput), expected);
    QCOMPARE(
        ScheduleTimeFormatter::rangeLabel(
            startLabel,
            uses55Endings,
            use24h
            ),
        fromUtf8(engineOutput)
        );
}

QTEST_MAIN(ScheduleTimeFormatterTests)

#include "schedule_time_formatter_tests.moc"
