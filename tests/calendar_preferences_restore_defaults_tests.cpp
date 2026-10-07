#include "core/application_services.h"
#include "features/calendar/academic_calendar_schedule.h"
#include "features/calendar/ui/academic_calendar_provider.h"
#include "features/calendar/ui/calendar_preferences_panel.h"
#include "next/application/academic_calendar_schedule_preferences.h"
#include "next/application/calendar_first_day_of_week_preferences.h"

#include <QApplication>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QDateEdit>
#include <QDebug>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtTest/QtTest>

#include <array>
#include <memory>
#include <string>
#include <utility>

namespace
{

using ClassMngr::Next::Application::AcademicCalendarSchedulePreferencesPort;
using ClassMngr::Next::Application::CalendarFirstDayOfWeek;
using ClassMngr::Next::Application::CalendarFirstDayOfWeekPreferencesPort;

struct CalendarPreferenceState final
{
    std::string scheduleJson;
    int scheduleWriteCount = 0;
    CalendarFirstDayOfWeek firstDayOfWeek = CalendarFirstDayOfWeek::Sunday;
};

class TestAcademicCalendarSchedulePreferencesPort final
    : public AcademicCalendarSchedulePreferencesPort
{
public:
    explicit TestAcademicCalendarSchedulePreferencesPort(
        std::shared_ptr<CalendarPreferenceState> state
        )
        : m_state(std::move(state))
    {
    }

    [[nodiscard]] std::string read() const override
    {
        return m_state->scheduleJson;
    }

    void write(const std::string& payload) const override
    {
        m_state->scheduleJson = payload;
        ++m_state->scheduleWriteCount;
    }

private:
    std::shared_ptr<CalendarPreferenceState> m_state;
};

class TestCalendarFirstDayOfWeekPreferencesPort final
    : public CalendarFirstDayOfWeekPreferencesPort
{
public:
    explicit TestCalendarFirstDayOfWeekPreferencesPort(
        std::shared_ptr<CalendarPreferenceState> state
        )
        : m_state(std::move(state))
    {
    }

    [[nodiscard]] CalendarFirstDayOfWeek load() const override
    {
        return m_state->firstDayOfWeek;
    }

    void save(const CalendarFirstDayOfWeek firstDayOfWeek) const override
    {
        m_state->firstDayOfWeek = firstDayOfWeek;
    }

private:
    std::shared_ptr<CalendarPreferenceState> m_state;
};

std::unique_ptr<AcademicCalendarProvider> makeProvider(
    const std::shared_ptr<CalendarPreferenceState>& state
    )
{
    return std::make_unique<AcademicCalendarProvider>(
        std::make_unique<TestAcademicCalendarSchedulePreferencesPort>(state),
        std::make_unique<TestCalendarFirstDayOfWeekPreferencesPort>(state)
        );
}

QWidget* widgetAt(
    QGridLayout* layout,
    const int row,
    const int column
    )
{
    if (!layout)
    {
        return nullptr;
    }

    QLayoutItem* const item = layout->itemAtPosition(row, column);
    return item ? item->widget() : nullptr;
}

QString utf8String(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

}

class CalendarPreferencesRestoreDefaultsTests final : public QObject
{
    Q_OBJECT

private slots:
    void restoreDefaultsStagesVisibleSchedulesWithoutPersistence();
};

void CalendarPreferencesRestoreDefaultsTests::
restoreDefaultsStagesVisibleSchedulesWithoutPersistence()
{
    AcademicCalendarSchedule seededSchedules;
    AcademicYearSchedule seededElementary =
        seededSchedules.defaultYearSchedule(SchoolLevel::Elementary, 2026);
    AcademicYearSchedule seededMiddle =
        seededSchedules.defaultYearSchedule(SchoolLevel::Middle, 2026);
    seededElementary.winterStart = QDate(2025, 12, 22);
    seededElementary.weeks = {12, 17, 10, 12};
    seededMiddle.winterStart = QDate(2025, 12, 15);
    seededMiddle.weeks = {13, 18, 5, 19};
    QVERIFY(seededElementary.isValid());
    QVERIFY(seededMiddle.isValid());
    seededSchedules.setYearSchedules(2026, seededElementary, seededMiddle);
    QVERIFY(seededSchedules.hasSavedSchedules());

    const QByteArray seededJson =
        QJsonDocument(seededSchedules.toJson())
            .toJson(QJsonDocument::Compact);
    auto state = std::make_shared<CalendarPreferenceState>();
    state->scheduleJson = seededJson.toStdString();
    const std::string persistedBefore = state->scheduleJson;

    auto provider = makeProvider(state);
    QCOMPARE(
        provider->schedule()
            .yearSchedule(SchoolLevel::Elementary, 2026)
            .winterStart,
        seededElementary.winterStart
        );
    QCOMPARE(
        provider->schedule()
            .yearSchedule(SchoolLevel::Middle, 2026)
            .winterStart,
        seededMiddle.winterStart
        );

    ApplicationServices services;
    CalendarPreferencesPanel panel(provider.get(), &services);
    auto* const termYear = panel.findChild<QSpinBox*>(
        QStringLiteral("preferencesCalendarTermYear")
        );
    auto* const linkOption = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarLinkWinterSpring")
        );
    auto* const restoreButton = panel.findChild<QPushButton*>(
        QStringLiteral("preferencesCalendarRestoreDefaults")
        );
    QVERIFY(termYear);
    QVERIFY(linkOption);
    QVERIFY(restoreButton);

    termYear->setValue(2026);
    QCOMPARE(termYear->value(), 2026);
    panel.show();
    QApplication::processEvents();

    QGridLayout* scheduleFields = nullptr;
    for (QGridLayout* const candidate : panel.findChildren<QGridLayout*>())
    {
        if (
            qobject_cast<QDateEdit*>(widgetAt(candidate, 2, 1))
            && qobject_cast<QSpinBox*>(widgetAt(candidate, 2, 2))
            && qobject_cast<QDateEdit*>(widgetAt(candidate, 2, 4))
            && qobject_cast<QSpinBox*>(widgetAt(candidate, 2, 5))
            )
        {
            scheduleFields = candidate;
            break;
        }
    }
    QVERIFY2(scheduleFields, "The visible term schedule grid was not found.");

    auto* const elementaryHeader = qobject_cast<QLabel*>(
        widgetAt(scheduleFields, 0, 1)
        );
    auto* const middleHeader = qobject_cast<QLabel*>(
        widgetAt(scheduleFields, 0, 4)
        );
    QVERIFY(elementaryHeader);
    QVERIFY(middleHeader);
    QCOMPARE(elementaryHeader->text(), QStringLiteral("Elementary"));
    QCOMPARE(middleHeader->text(), QStringLiteral("Middle School"));

    const QStringList expectedTermNames{
        QStringLiteral("Winter"),
        QStringLiteral("Spring"),
        QStringLiteral("Summer"),
        QStringLiteral("Fall")
    };
    const std::array<int, 2> dateColumns{1, 4};
    const std::array<int, 2> weekColumns{2, 5};
    std::array<std::array<QDateEdit*, AcademicTermCount>, 2> dateEdits{};
    std::array<std::array<QSpinBox*, AcademicTermCount>, 2> weekEdits{};

    for (int term = 0; term < AcademicTermCount; ++term)
    {
        auto* const termLabel = qobject_cast<QLabel*>(
            widgetAt(scheduleFields, term + 2, 0)
            );
        QVERIFY(termLabel);
        QCOMPARE(termLabel->text(), expectedTermNames.at(term));

        for (int school = 0; school < 2; ++school)
        {
            dateEdits[school][term] = qobject_cast<QDateEdit*>(
                widgetAt(scheduleFields, term + 2, dateColumns[school])
                );
            weekEdits[school][term] = qobject_cast<QSpinBox*>(
                widgetAt(scheduleFields, term + 2, weekColumns[school])
                );
            QVERIFY(dateEdits[school][term]);
            QVERIFY(weekEdits[school][term]);
        }
    }

    QVERIFY(!linkOption->isChecked());
    for (int term = 0; term < AcademicTermCount; ++term)
    {
        QCOMPARE(
            dateEdits[0][term]->date(),
            seededElementary.termStart(static_cast<AcademicTerm>(term))
            );
        QCOMPARE(weekEdits[0][term]->value(), seededElementary.weeks[term]);
        QCOMPARE(
            dateEdits[1][term]->date(),
            seededMiddle.termStart(static_cast<AcademicTerm>(term))
            );
        QCOMPARE(weekEdits[1][term]->value(), seededMiddle.weeks[term]);
    }

    QSignalSpy changed(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(changed.isValid());
    restoreButton->click();

    const std::array<std::pair<QDate, int>, AcademicTermCount>
        expectedElementary{{
            {QDate(2025, 12, 29), 11},
            {QDate(2026, 3, 16), 19},
            {QDate(2026, 7, 27), 11},
            {QDate(2026, 10, 12), 11}
        }};
    const std::array<std::pair<QDate, int>, AcademicTermCount>
        expectedMiddle{{
            {QDate(2025, 12, 29), 11},
            {QDate(2026, 3, 16), 19},
            {QDate(2026, 7, 27), 4},
            {QDate(2026, 8, 24), 18}
        }};
    const std::array<
        std::array<std::pair<QDate, int>, AcademicTermCount>,
        2
        > expectedSchedules{expectedElementary, expectedMiddle};

    for (int school = 0; school < 2; ++school)
    {
        for (int term = 0; term < AcademicTermCount; ++term)
        {
            QCOMPARE(
                dateEdits[school][term]->date(),
                expectedSchedules[school][term].first
                );
            QCOMPARE(
                weekEdits[school][term]->value(),
                expectedSchedules[school][term].second
                );
        }
    }

    QVERIFY(linkOption->isChecked());
    QStringList disabledMiddleControls;
    for (int term = 0; term < AcademicTermCount; ++term)
    {
        if (!dateEdits[1][term]->isEnabled())
        {
            disabledMiddleControls.append(
                QStringLiteral("middle.%1.start")
                    .arg(expectedTermNames.at(term).toLower())
                );
        }
        if (!weekEdits[1][term]->isEnabled())
        {
            disabledMiddleControls.append(
                QStringLiteral("middle.%1.weeks")
                    .arg(expectedTermNames.at(term).toLower())
                );
        }
    }
    const QStringList expectedDisabledMiddleControls{
        QStringLiteral("middle.winter.start"),
        QStringLiteral("middle.winter.weeks"),
        QStringLiteral("middle.spring.start"),
        QStringLiteral("middle.spring.weeks"),
        QStringLiteral("middle.summer.start")
    };
    QCOMPARE(disabledMiddleControls, expectedDisabledMiddleControls);

    const std::string persistedAfter = state->scheduleJson;
    QCOMPARE(state->scheduleWriteCount, 0);
    QCOMPARE(persistedAfter, persistedBefore);
    QCOMPARE(changed.count(), 0);

    const AcademicYearSchedule providerElementaryAfter =
        provider->schedule().yearSchedule(SchoolLevel::Elementary, 2026);
    const AcademicYearSchedule providerMiddleAfter =
        provider->schedule().yearSchedule(SchoolLevel::Middle, 2026);
    const bool providerSchedulesUnchanged =
        providerElementaryAfter.winterStart == seededElementary.winterStart
        && providerElementaryAfter.weeks == seededElementary.weeks
        && providerMiddleAfter.winterStart == seededMiddle.winterStart
        && providerMiddleAfter.weeks == seededMiddle.weeks;
    QVERIFY(providerSchedulesUnchanged);

    QJsonArray scheduleRows;
    const std::array<QString, 2> schoolNames{
        elementaryHeader->text(),
        middleHeader->text()
    };
    for (int school = 0; school < 2; ++school)
    {
        QJsonArray terms;
        for (int term = 0; term < AcademicTermCount; ++term)
        {
            terms.append(QJsonObject{
                {QStringLiteral("term"), expectedTermNames.at(term)},
                {
                    QStringLiteral("start"),
                    dateEdits[school][term]->date().toString(Qt::ISODate)
                },
                {
                    QStringLiteral("weeks"),
                    weekEdits[school][term]->value()
                }
            });
        }
        scheduleRows.append(QJsonObject{
            {QStringLiteral("school"), schoolNames[school]},
            {QStringLiteral("terms"), terms}
        });
    }

    QJsonArray disabledControls;
    for (const QString& control : disabledMiddleControls)
    {
        disabledControls.append(control);
    }

    QJsonObject persistence;
    persistence.insert(QStringLiteral("scheduleJsonBefore"), utf8String(persistedBefore));
    persistence.insert(QStringLiteral("scheduleJsonAfter"), utf8String(persistedAfter));
    persistence.insert(
        QStringLiteral("scheduleWriteCountAfter"),
        state->scheduleWriteCount
        );
    persistence.insert(QStringLiteral("unchanged"), persistedAfter == persistedBefore);
    persistence.insert(
        QStringLiteral("providerSchedulesUnchanged"),
        providerSchedulesUnchanged
        );

    QJsonObject transcript;
    transcript.insert(QStringLiteral("academicYear"), 2026);
    transcript.insert(
        QStringLiteral("calendarPreferencesChangedSignalCount"),
        changed.count()
        );
    transcript.insert(QStringLiteral("disabledMiddleControls"), disabledControls);
    transcript.insert(QStringLiteral("linkOptionChecked"), linkOption->isChecked());
    transcript.insert(QStringLiteral("persistence"), persistence);
    transcript.insert(QStringLiteral("schedules"), scheduleRows);

    const QByteArray canonicalTranscript =
        QJsonDocument(transcript).toJson(QJsonDocument::Compact);
    const QByteArray transcriptHash = QCryptographicHash::hash(
        canonicalTranscript,
        QCryptographicHash::Sha256
        ).toHex();
    qInfo().noquote()
        << "F384_CALENDAR_PREFERENCES_RESTORE_DEFAULTS_V1"
        << QString::fromUtf8(canonicalTranscript);
    qInfo().noquote()
        << "F384_CALENDAR_PREFERENCES_RESTORE_DEFAULTS_SHA256"
        << QString::fromLatin1(transcriptHash);
}

QTEST_MAIN(CalendarPreferencesRestoreDefaultsTests)

#include "calendar_preferences_restore_defaults_tests.moc"
