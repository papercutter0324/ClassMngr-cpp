#include "app/services/feature_services.h"
#include "features/schedule/ui/schedule_builder.h"

#include <QtTest>

class ScheduleBuilderTests : public QObject
{
    Q_OBJECT

private slots:
    void intensiveModeAlwaysBuildsFullRange();
    void regularModeRetainsDefaultRange();
};

void ScheduleBuilderTests::intensiveModeAlwaysBuildsFullRange()
{
    ClassService classService(nullptr);
    const ScheduleBuilder builder(&classService);

    const ScheduleBuildResult result =
        builder.build(
            true,
            {QStringLiteral("Monday")}
            );

    QCOMPARE(result.rows.size(), 13);
    QCOMPARE(result.rows.first().label, QStringLiteral("09:00"));
    QCOMPARE(result.rows.last().label, QStringLiteral("21:00"));
    QCOMPARE(result.scheduleOffset, 0);
    QVERIFY(!result.uses55Endings);
}

void ScheduleBuilderTests::regularModeRetainsDefaultRange()
{
    ClassService classService(nullptr);
    const ScheduleBuilder builder(&classService);

    const ScheduleBuildResult result =
        builder.build(
            false,
            {QStringLiteral("Monday")}
            );

    QCOMPARE(result.rows.size(), 6);
    QCOMPARE(result.rows.first().label, QStringLiteral("16:00"));
    QCOMPARE(result.rows.last().label, QStringLiteral("21:00"));
}

QTEST_APPLESS_MAIN(ScheduleBuilderTests)

#include "schedule_builder_tests.moc"
