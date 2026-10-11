#include "features/calendar/academic_calendar_schedule.h"
#include "features/calendar/ui/academic_calendar_provider.h"
#include "next/application/academic_calendar_schedule_preferences.h"
#include "next/application/calendar_first_day_of_week_preferences.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

#include <memory>
#include <string>
#include <utility>

using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next;

namespace
{

struct AcademicCalendarPreferenceState final
{
    std::string scheduleJson;
    CalendarFirstDayOfWeek firstDayOfWeek = CalendarFirstDayOfWeek::Sunday;
    int scheduleWriteCount = 0;
    int scheduleWriteAttemptCount = 0;
    bool failScheduleWrites = false;
    std::string scheduleWriteFailure =
        "Academic calendar schedule test failure.";
    int firstDayWriteCount = 0;
    int firstDayWriteAttemptCount = 0;
    bool failFirstDayWrites = false;
    std::string firstDayWriteFailure =
        "Academic calendar first-day test failure.";
};

class TestAcademicCalendarSchedulePreferencesPort final
    : public AcademicCalendarSchedulePreferencesPort
{
public:
    explicit TestAcademicCalendarSchedulePreferencesPort(
        std::shared_ptr<AcademicCalendarPreferenceState> state
        )
        : m_state(std::move(state))
    {
    }

    [[nodiscard]] std::string read() const override
    {
        return m_state->scheduleJson;
    }

    [[nodiscard]] AcademicCalendarSchedulePreferencesSaveResult write(
        const std::string& payload
        ) const override
    {
        ++m_state->scheduleWriteAttemptCount;
        if (m_state->failScheduleWrites)
        {
            return AcademicCalendarSchedulePreferencesSaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = m_state->scheduleWriteFailure,
                .recoverable = false
            });
        }

        m_state->scheduleJson = payload;
        ++m_state->scheduleWriteCount;
        return AcademicCalendarSchedulePreferencesSaveResult::success();
    }

private:
    std::shared_ptr<AcademicCalendarPreferenceState> m_state;
};

class TestCalendarFirstDayOfWeekPreferencesPort final
    : public CalendarFirstDayOfWeekPreferencesPort
{
public:
    explicit TestCalendarFirstDayOfWeekPreferencesPort(
        std::shared_ptr<AcademicCalendarPreferenceState> state
        )
        : m_state(std::move(state))
    {
    }

    [[nodiscard]] CalendarFirstDayOfWeek load() const override
    {
        return m_state->firstDayOfWeek;
    }

    [[nodiscard]] SaveResult save(
        const CalendarFirstDayOfWeek firstDayOfWeek
        ) const override
    {
        ++m_state->firstDayWriteAttemptCount;
        if (m_state->failFirstDayWrites)
        {
            return SaveResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = m_state->firstDayWriteFailure,
                .recoverable = false
            });
        }

        m_state->firstDayOfWeek = firstDayOfWeek;
        ++m_state->firstDayWriteCount;
        return SaveResult::success();
    }

private:
    std::shared_ptr<AcademicCalendarPreferenceState> m_state;
};

std::unique_ptr<AcademicCalendarProvider> makeProvider(
    const std::shared_ptr<AcademicCalendarPreferenceState>& state =
        std::make_shared<AcademicCalendarPreferenceState>()
    )
{
    return std::make_unique<AcademicCalendarProvider>(
        std::make_unique<TestAcademicCalendarSchedulePreferencesPort>(state),
        std::make_unique<TestCalendarFirstDayOfWeekPreferencesPort>(state)
        );
}

}

class AcademicCalendarTests : public QObject
{
    Q_OBJECT

private slots:
    void defaultBoundariesMatchSchoolSchedules();
    void termLookupResetsWeeksAtBoundaries();
    void providerFormatsCompactAndDualTitles();
    void providerBuildsLocaleAlignedWeekRows();
    void providerOverridesFirstDayOfWeek();
    void providerReadsAndWritesInjectedPreferencePorts();
    void providerReloadSignalsOnlyPersistedChanges();
    void providerFirstDaySaveFailureDoesNotPublishAndRetrySucceeds();
    void providerWithoutFirstDayPortDoesNotPublishChange();
    void providerSaveFailureDoesNotPublishScheduleOrRevision();
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
    const auto provider = makeProvider();

    QCOMPARE(
        provider->monthTitle(2026, 5),
        QStringLiteral("June 2026 — Spring Wk 12")
        );
    QCOMPARE(
        provider->monthTitle(2026, 8),
        QStringLiteral(
            "September 2026 — Elem: Summer Wk 7 · MS: Fall Wk 3"
            )
        );
    QCOMPARE(
        provider->termYearForDate(QDate(2026, 1, 5)),
        2026
        );
}

void AcademicCalendarTests::providerBuildsLocaleAlignedWeekRows()
{
    QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    const auto provider = makeProvider();

    const QVariantList sundayFirst =
        provider->weekRows(2026, 8, 0);
    const QVariantList mondayFirst =
        provider->weekRows(2026, 8, 1);

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

void AcademicCalendarTests::providerOverridesFirstDayOfWeek()
{
    QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    const auto provider = makeProvider();

    QVERIFY(provider->setFirstDayOfWeek(0));
    QCOMPARE(provider->firstDayOfWeek(), 0);

    const QVariantMap sundayRow =
        provider->weekRows(
                2026,
                2,
                provider->firstDayOfWeek()
                )
            .first()
            .toMap();
    QCOMPARE(sundayRow.value(QStringLiteral("elementaryWeek")).toInt(), 10);

    QVERIFY(provider->setFirstDayOfWeek(1));
    QCOMPARE(provider->firstDayOfWeek(), 1);

    const QVariantMap mondayRow =
        provider->weekRows(
                2026,
                2,
                provider->firstDayOfWeek()
                )
            .first()
            .toMap();
    QCOMPARE(mondayRow.value(QStringLiteral("elementaryWeek")).toInt(), 9);
}

void AcademicCalendarTests::providerReadsAndWritesInjectedPreferencePorts()
{
    auto state = std::make_shared<AcademicCalendarPreferenceState>();
    auto provider = makeProvider(state);

    AcademicYearSchedule elementary =
        provider->schedule().yearSchedule(SchoolLevel::Elementary, 2026);
    AcademicYearSchedule middle =
        provider->schedule().yearSchedule(SchoolLevel::Middle, 2026);
    elementary.weeks[2] = 12;
    QVERIFY(provider->saveYearSchedules(2026, elementary, middle));
    QVERIFY(provider->setFirstDayOfWeek(1));

    QCOMPARE(state->scheduleWriteCount, 1);
    QVERIFY(!state->scheduleJson.empty());
    QCOMPARE(state->firstDayWriteCount, 1);
    QCOMPARE(state->firstDayOfWeek, CalendarFirstDayOfWeek::Monday);

    provider->reload();
    QCOMPARE(
        provider->schedule()
            .yearSchedule(SchoolLevel::Elementary, 2026)
            .weeks[2],
        12
        );
    QCOMPARE(provider->firstDayOfWeek(), 1);
}

void AcademicCalendarTests::providerReloadSignalsOnlyPersistedChanges()
{
    auto state = std::make_shared<AcademicCalendarPreferenceState>();
    auto provider = makeProvider(state);
    const int revisionBefore = provider->revision();
    QSignalSpy firstDayChanged(
        provider.get(),
        &AcademicCalendarProvider::firstDayOfWeekChanged
        );
    QVERIFY(firstDayChanged.isValid());
    QSignalSpy revisionChanged(
        provider.get(),
        &AcademicCalendarProvider::revisionChanged
        );
    QVERIFY(revisionChanged.isValid());

    provider->reload();

    QCOMPARE(provider->revision(), revisionBefore);
    QCOMPARE(firstDayChanged.count(), 0);
    QCOMPARE(revisionChanged.count(), 0);

    state->firstDayOfWeek = CalendarFirstDayOfWeek::Monday;
    provider->reload();

    QCOMPARE(provider->firstDayOfWeek(), 1);
    QCOMPARE(provider->revision(), revisionBefore + 1);
    QCOMPARE(firstDayChanged.count(), 1);
    QCOMPARE(revisionChanged.count(), 1);

    AcademicCalendarSchedule changedSchedule;
    AcademicYearSchedule elementary =
        changedSchedule.yearSchedule(SchoolLevel::Elementary, 2026);
    AcademicYearSchedule middle =
        changedSchedule.yearSchedule(SchoolLevel::Middle, 2026);
    ++elementary.weeks[2];
    changedSchedule.setYearSchedules(2026, elementary, middle);
    state->scheduleJson = QJsonDocument(changedSchedule.toJson())
        .toJson(QJsonDocument::Compact)
        .toStdString();
    provider->reload();

    QCOMPARE(
        provider->schedule()
            .yearSchedule(SchoolLevel::Elementary, 2026)
            .weeks[2],
        elementary.weeks[2]
        );
    QCOMPARE(provider->revision(), revisionBefore + 2);
    QCOMPARE(firstDayChanged.count(), 1);
    QCOMPARE(revisionChanged.count(), 2);

    provider->reload();

    QCOMPARE(provider->revision(), revisionBefore + 2);
    QCOMPARE(firstDayChanged.count(), 1);
    QCOMPARE(revisionChanged.count(), 2);
}

void AcademicCalendarTests::
providerFirstDaySaveFailureDoesNotPublishAndRetrySucceeds()
{
    auto state = std::make_shared<AcademicCalendarPreferenceState>();
    auto provider = makeProvider(state);
    const int revisionBefore = provider->revision();
    QSignalSpy firstDayChanged(
        provider.get(),
        &AcademicCalendarProvider::firstDayOfWeekChanged
        );
    QVERIFY(firstDayChanged.isValid());
    QSignalSpy revisionChanged(
        provider.get(),
        &AcademicCalendarProvider::revisionChanged
        );
    QVERIFY(revisionChanged.isValid());

    state->failFirstDayWrites = true;
    const auto failed = provider->setFirstDayOfWeek(1);

    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QCOMPARE(
        failed.error().message,
        state->firstDayWriteFailure
        );
    QCOMPARE(state->firstDayWriteAttemptCount, 1);
    QCOMPARE(state->firstDayWriteCount, 0);
    QCOMPARE(state->firstDayOfWeek, CalendarFirstDayOfWeek::Sunday);
    QCOMPARE(provider->firstDayOfWeek(), 0);
    QCOMPARE(provider->revision(), revisionBefore);
    QCOMPARE(firstDayChanged.count(), 0);
    QCOMPARE(revisionChanged.count(), 0);

    state->failFirstDayWrites = false;
    const auto retried = provider->setFirstDayOfWeek(1);

    QVERIFY(retried);
    QCOMPARE(state->firstDayWriteAttemptCount, 2);
    QCOMPARE(state->firstDayWriteCount, 1);
    QCOMPARE(state->firstDayOfWeek, CalendarFirstDayOfWeek::Monday);
    QCOMPARE(provider->firstDayOfWeek(), 1);
    QCOMPARE(provider->revision(), revisionBefore + 1);
    QCOMPARE(firstDayChanged.count(), 1);
    QCOMPARE(revisionChanged.count(), 1);
}

void AcademicCalendarTests::
providerWithoutFirstDayPortDoesNotPublishChange()
{
    auto state = std::make_shared<AcademicCalendarPreferenceState>();
    AcademicCalendarProvider provider(
        std::make_unique<TestAcademicCalendarSchedulePreferencesPort>(state),
        std::unique_ptr<CalendarFirstDayOfWeekPreferencesPort>{}
        );
    const int revisionBefore = provider.revision();
    QSignalSpy firstDayChanged(
        &provider,
        &AcademicCalendarProvider::firstDayOfWeekChanged
        );
    QVERIFY(firstDayChanged.isValid());
    QSignalSpy revisionChanged(
        &provider,
        &AcademicCalendarProvider::revisionChanged
        );
    QVERIFY(revisionChanged.isValid());

    const auto failed = provider.setFirstDayOfWeek(1);

    QVERIFY(!failed);
    QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
    QVERIFY(QString::fromStdString(failed.error().message).contains(
        QStringLiteral("persistence is unavailable")
        ));
    QCOMPARE(provider.firstDayOfWeek(), 0);
    QCOMPARE(provider.revision(), revisionBefore);
    QCOMPARE(firstDayChanged.count(), 0);
    QCOMPARE(revisionChanged.count(), 0);
}

void AcademicCalendarTests::
providerSaveFailureDoesNotPublishScheduleOrRevision()
{
    auto state = std::make_shared<AcademicCalendarPreferenceState>();
    auto provider = makeProvider(state);
    const AcademicCalendarSchedule scheduleBefore = provider->schedule();
    const std::string payloadBefore = state->scheduleJson;
    const int revisionBefore = provider->revision();
    QSignalSpy revisionChanged(
        provider.get(),
        &AcademicCalendarProvider::revisionChanged
        );
    QVERIFY(revisionChanged.isValid());

    AcademicYearSchedule proposedElementary =
        scheduleBefore.yearSchedule(SchoolLevel::Elementary, 2026);
    AcademicYearSchedule proposedMiddle =
        scheduleBefore.yearSchedule(SchoolLevel::Middle, 2026);
    ++proposedElementary.weeks[2];
    QVERIFY(proposedElementary.isValid());

    state->failScheduleWrites = true;
    const auto failed = provider->saveYearSchedules(
        2026,
        proposedElementary,
        proposedMiddle
        );
    QVERIFY(!failed);
    QCOMPARE(
        failed.error().code,
        Domain::ErrorCode::Technical
        );
    QCOMPARE(
        QString::fromStdString(failed.error().message),
        QString::fromStdString(state->scheduleWriteFailure)
        );
    QCOMPARE(state->scheduleWriteAttemptCount, 1);
    QCOMPARE(state->scheduleWriteCount, 0);
    QCOMPARE(state->scheduleJson, payloadBefore);
    QCOMPARE(provider->schedule().toJson(), scheduleBefore.toJson());
    QCOMPARE(provider->revision(), revisionBefore);
    QCOMPARE(revisionChanged.count(), 0);

    state->failScheduleWrites = false;
    const auto saved = provider->saveYearSchedules(
        2026,
        proposedElementary,
        proposedMiddle
        );
    QVERIFY(saved);
    QCOMPARE(state->scheduleWriteAttemptCount, 2);
    QCOMPARE(state->scheduleWriteCount, 1);
    QCOMPARE(
        provider->schedule()
            .yearSchedule(SchoolLevel::Elementary, 2026)
            .weeks[2],
        proposedElementary.weeks[2]
        );
    QCOMPARE(provider->revision(), revisionBefore + 1);
    QCOMPARE(revisionChanged.count(), 1);
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

#include "academic_calendar_tests.moc"
