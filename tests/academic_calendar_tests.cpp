#include "features/my_info/academic_calendar_schedule.h"
#include "features/my_info/ui/academic_calendar_provider.h"
#include "data/data_service.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

class AcademicCalendarTests : public QObject
{
    Q_OBJECT

private slots:
    void defaultBoundariesMatchSchoolSchedules();
    void termLookupResetsWeeksAtBoundaries();
    void providerFormatsCompactAndDualTitles();
    void providerBuildsLocaleAlignedWeekRows();
    void customizedYearShiftsRolloverButFutureUsesDefaults();
    void winterEditKeepsPreviousFallContinuous();
    void replacingEarlierYearInvalidatesLaterCustomization();
    void jsonRoundTripAndMalformedFallback();
};

void AcademicCalendarTests::defaultBoundariesMatchSchoolSchedules()
{
    AcademicCalendarSchedule calendar;
    const AcademicYearSchedule elementary =
        calendar.yearSchedule(SchoolLevel::Elementary, 2026);
    const AcademicYearSchedule middle =
        calendar.yearSchedule(SchoolLevel::Middle, 2026);

    QCOMPARE(elementary.winterStart, QDate(2025, 12, 29));
    QCOMPARE(elementary.termStart(AcademicTerm::Spring), QDate(2026, 3, 16));
    QCOMPARE(elementary.termStart(AcademicTerm::Summer), QDate(2026, 7, 27));
    QCOMPARE(elementary.termStart(AcademicTerm::Fall), QDate(2026, 10, 12));
    QCOMPARE(elementary.endDate(), QDate(2026, 12, 28));

    QCOMPARE(middle.termStart(AcademicTerm::Spring), QDate(2026, 3, 16));
    QCOMPARE(middle.termStart(AcademicTerm::Summer), QDate(2026, 7, 27));
    QCOMPARE(middle.termStart(AcademicTerm::Fall), QDate(2026, 8, 24));
    QCOMPARE(middle.endDate(), QDate(2026, 12, 28));
}

void AcademicCalendarTests::termLookupResetsWeeksAtBoundaries()
{
    AcademicCalendarSchedule calendar;

    const AcademicTermPosition june =
        calendar.termAt(
            SchoolLevel::Elementary,
            QDate(2026, 6, 1)
            );
    QVERIFY(june.valid);
    QCOMPARE(june.term, AcademicTerm::Spring);
    QCOMPARE(june.week, 12);
    QCOMPARE(june.weekStart, QDate(2026, 6, 1));

    const AcademicTermPosition elementarySeptember =
        calendar.termAt(
            SchoolLevel::Elementary,
            QDate(2026, 9, 7)
            );
    QCOMPARE(elementarySeptember.term, AcademicTerm::Summer);
    QCOMPARE(elementarySeptember.week, 7);

    const AcademicTermPosition middleSeptember =
        calendar.termAt(
            SchoolLevel::Middle,
            QDate(2026, 9, 7)
            );
    QCOMPARE(middleSeptember.term, AcademicTerm::Fall);
    QCOMPARE(middleSeptember.week, 3);
}

void AcademicCalendarTests::providerFormatsCompactAndDualTitles()
{
    QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    AcademicCalendarProvider provider(nullptr);

    QCOMPARE(
        provider.monthTitle(2026, 5),
        QStringLiteral("June 2026 — Spring Wk 12")
        );
    QCOMPARE(
        provider.monthTitle(2026, 8),
        QStringLiteral(
            "September 2026 — Elem: Summer Wk 7 · MS: Fall Wk 3"
            )
        );
    QCOMPARE(
        provider.termYearForDate(QDate(2026, 1, 5)),
        2026
        );
}

void AcademicCalendarTests::providerBuildsLocaleAlignedWeekRows()
{
    QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    AcademicCalendarProvider provider(nullptr);

    const QVariantList sundayFirst =
        provider.weekRows(2026, 8, 0);
    const QVariantList mondayFirst =
        provider.weekRows(2026, 8, 1);

    QCOMPARE(sundayFirst.size(), 6);
    QCOMPARE(mondayFirst.size(), 6);

    const QVariantMap sundayRow = sundayFirst.first().toMap();
    QCOMPARE(sundayRow.value(QStringLiteral("elementaryWeek")).toInt(), 6);
    QCOMPARE(sundayRow.value(QStringLiteral("middleWeek")).toInt(), 2);
    QVERIFY(
        sundayRow
            .value(QStringLiteral("elementaryTooltip"))
            .toString()
            .contains(QStringLiteral("Summer, Week 6"))
        );

    const QVariantMap mondayRow = mondayFirst.first().toMap();
    QCOMPARE(mondayRow.value(QStringLiteral("elementaryWeek")).toInt(), 6);
    QCOMPARE(mondayRow.value(QStringLiteral("middleWeek")).toInt(), 2);
}

void AcademicCalendarTests::customizedYearShiftsRolloverButFutureUsesDefaults()
{
    AcademicCalendarSchedule calendar;
    AcademicYearSchedule elementary =
        calendar.yearSchedule(SchoolLevel::Elementary, 2026);
    AcademicYearSchedule middle =
        calendar.yearSchedule(SchoolLevel::Middle, 2026);

    elementary.weeks[2] = 12;
    calendar.setYearSchedules(2026, elementary, middle);

    const AcademicYearSchedule elementary2027 =
        calendar.yearSchedule(SchoolLevel::Elementary, 2027);
    const AcademicYearSchedule middle2027 =
        calendar.yearSchedule(SchoolLevel::Middle, 2027);

    QCOMPARE(elementary2027.winterStart, QDate(2027, 1, 4));
    QCOMPARE(
        elementary2027.weeks,
        AcademicCalendarSchedule::defaultWeeks(SchoolLevel::Elementary)
        );
    QCOMPARE(middle2027.winterStart, QDate(2026, 12, 28));
    QCOMPARE(
        middle2027.weeks,
        AcademicCalendarSchedule::defaultWeeks(SchoolLevel::Middle)
        );
}

void AcademicCalendarTests::winterEditKeepsPreviousFallContinuous()
{
    AcademicCalendarSchedule calendar;
    AcademicYearSchedule elementary2027 =
        calendar.yearSchedule(SchoolLevel::Elementary, 2027);
    AcademicYearSchedule middle2027 =
        calendar.yearSchedule(SchoolLevel::Middle, 2027);

    elementary2027.winterStart = QDate(2027, 1, 4);
    middle2027.winterStart = QDate(2027, 1, 4);
    calendar.setYearSchedules(2027, elementary2027, middle2027);

    const AcademicYearSchedule elementary2026 =
        calendar.yearSchedule(SchoolLevel::Elementary, 2026);
    const AcademicYearSchedule middle2026 =
        calendar.yearSchedule(SchoolLevel::Middle, 2026);

    QCOMPARE(elementary2026.weeks[3], 12);
    QCOMPARE(middle2026.weeks[3], 19);
    QCOMPARE(elementary2026.endDate(), QDate(2027, 1, 4));
    QCOMPARE(middle2026.endDate(), QDate(2027, 1, 4));
}

void AcademicCalendarTests::replacingEarlierYearInvalidatesLaterCustomization()
{
    AcademicCalendarSchedule calendar;
    const AcademicYearSchedule elementary2026 =
        calendar.yearSchedule(SchoolLevel::Elementary, 2026);
    const AcademicYearSchedule middle2026 =
        calendar.yearSchedule(SchoolLevel::Middle, 2026);

    AcademicYearSchedule elementary2027 =
        calendar.yearSchedule(SchoolLevel::Elementary, 2027);
    AcademicYearSchedule middle2027 =
        calendar.yearSchedule(SchoolLevel::Middle, 2027);
    elementary2027.weeks[0] = 12;
    middle2027.weeks[0] = 12;
    calendar.setYearSchedules(2027, elementary2027, middle2027);
    QVERIFY(calendar.hasCustomYearAfter(2026));

    AcademicYearSchedule revisedElementary2026 = elementary2026;
    revisedElementary2026.weeks[3] = 12;
    calendar.setYearSchedules(
        2026,
        revisedElementary2026,
        middle2026
        );

    QVERIFY(!calendar.hasCustomYearAfter(2026));
    const AcademicYearSchedule regenerated2027 =
        calendar.yearSchedule(SchoolLevel::Elementary, 2027);
    QCOMPARE(regenerated2027.winterStart, QDate(2027, 1, 4));
    QCOMPARE(regenerated2027.weeks[0], 11);
}

void AcademicCalendarTests::jsonRoundTripAndMalformedFallback()
{
    AcademicCalendarSchedule source;
    AcademicYearSchedule elementary =
        source.yearSchedule(SchoolLevel::Elementary, 2026);
    AcademicYearSchedule middle =
        source.yearSchedule(SchoolLevel::Middle, 2026);
    elementary.weeks[2] = 12;
    source.setYearSchedules(2026, elementary, middle);

    AcademicCalendarSchedule restored;
    QVERIFY(restored.fromJson(source.toJson()));
    QCOMPARE(
        restored.yearSchedule(SchoolLevel::Elementary, 2026).weeks[2],
        12
        );

    QJsonObject malformed = source.toJson();
    QJsonObject profiles = malformed.value(QStringLiteral("profiles")).toObject();
    QJsonObject elementaryProfile =
        profiles.value(QStringLiteral("elementary")).toObject();
    QJsonObject year =
        elementaryProfile.value(QStringLiteral("2026")).toObject();
    year.insert(
        QStringLiteral("weeks"),
        QJsonArray{11, 0, 11, 11}
        );
    elementaryProfile.insert(QStringLiteral("2026"), year);
    profiles.insert(QStringLiteral("elementary"), elementaryProfile);
    malformed.insert(QStringLiteral("profiles"), profiles);

    QVERIFY(!restored.fromJson(malformed));
    QCOMPARE(
        restored.yearSchedule(SchoolLevel::Elementary, 2026).weeks[1],
        19
        );
}

QTEST_APPLESS_MAIN(AcademicCalendarTests)

bool DataService::isOpen() const
{
    return false;
}

void DataService::saveSetting(
    const QString& key,
    const QVariant& value
    )
{
    Q_UNUSED(key);
    Q_UNUSED(value);
}

QVariant DataService::loadSetting(
    const QString& key,
    const QVariant& defaultValue
    )
{
    Q_UNUSED(key);
    return defaultValue;
}

#include "academic_calendar_tests.moc"
