#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "domain/models/calendar_event.h"
#include "features/calendar/academic_calendar_schedule.h"
#include "features/calendar/ui/calendar_event_model.h"
#include "features/calendar/ui/academic_calendar_provider.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/calendar/ui/calendar_preferences_panel.h"
#include "next/application/academic_calendar_schedule_preferences.h"
#include "next/application/calendar_first_day_of_week_preferences.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateEdit>
#include <QDateTime>
#include <QDebug>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSqlError>
#include <QSqlQuery>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest/QtTest>

#include <array>
#include <cstdio>
#include <functional>
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

struct CalendarResetWorkspace final
{
    QTemporaryDir directory;
    ApplicationServices services;
    QString databasePath;
    int eventId = -1;
};

struct ResetPromptAudit final
{
    bool promptObservationCompleted = false;
    bool confirmationSeen = false;
    QString confirmationTitle;
    QString confirmationText;
    QString resetText;
    QString cancelText;
    bool resetIsDestructive = false;
    bool cancelIsReject = false;
    bool cancelIsDefault = false;
    bool selectedButtonClicked = false;
    bool followupPromptSeen = false;
    QString followupTitle;
    QString followupText;
    bool followupIsWarning = false;
    bool failSafeDismissedPrompt = false;
};

enum class ResetPromptChoice
{
    ObserveOnly,
    Cancel,
    Reset
};

bool initializeCalendarResetWorkspace(
    CalendarResetWorkspace& workspace,
    const QDate& eventDate,
    QString* error
    )
{
    if (!workspace.directory.isValid())
    {
        if (error)
        {
            *error = QStringLiteral("Temporary directory is unavailable.");
        }
        return false;
    }

    workspace.databasePath = workspace.directory.filePath(
        QStringLiteral("f387-calendar-preferences-reset-%1.tps")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        );
    const Status opened = workspace.services.openDatabase(
        workspace.databasePath
        );
    if (!opened)
    {
        if (error)
        {
            *error = opened.error();
        }
        return false;
    }

    CalendarEvent event;
    event.title = QStringLiteral("F387 Reset Fixture Event");
    event.eventType = QStringLiteral("Holiday");
    event.timeStatus = QStringLiteral("Timed");
    event.startDate = eventDate;
    event.startTime = QTime(9, 0);
    event.endDate = eventDate;
    event.endTime = QTime(10, 0);

    const auto saved = workspace.services.databaseSession()
        ->calendarEventRepository()
        ->saveCalendarEvent(event);
    if (!saved)
    {
        if (error)
        {
            *error = saved.error();
        }
        return false;
    }

    workspace.eventId = *saved;
    return true;
}

bool calendarEventExists(
    ApplicationServices& services,
    const int eventId,
    bool* exists,
    QString* error
    )
{
    if (!services.databaseSession())
    {
        if (error)
        {
            *error = QStringLiteral("Database session is not open.");
        }
        return false;
    }

    QSqlQuery query(services.databaseSession()->database());
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM calendar_events WHERE id=?"
        ));
    query.addBindValue(eventId);
    const bool queryExecuted = query.exec();
    if (!queryExecuted || !query.next())
    {
        if (error)
        {
            *error = query.lastError().text();
        }
        return false;
    }

    if (exists)
    {
        *exists = query.value(0).toInt() > 0;
    }
    return true;
}

QPushButton* calendarResetButton(CalendarPreferencesPanel& panel)
{
    return panel.findChild<QPushButton*>(
        QStringLiteral("preferencesCalendarResetEvents")
        );
}

QLabel* calendarResetStatusLabel(CalendarPreferencesPanel& panel)
{
    return panel.findChild<QLabel*>(QStringLiteral("sectionSubtitle"));
}

void clickResetAndRespond(
    CalendarPreferencesPanel& panel,
    const ResetPromptChoice choice,
    ResetPromptAudit* audit,
    const std::function<void()>& beforeReset = {}
    )
{
    IPromptTestDriver& driver = DialogServices::promptTestDriver();
    auto* const resetButton = calendarResetButton(panel);
    if (!resetButton)
    {
        if (audit)
        {
            audit->failSafeDismissedPrompt = true;
        }
        return;
    }

    QTimer promptObserver;
    promptObserver.setSingleShot(true);
    QObject::connect(
        &promptObserver,
        &QTimer::timeout,
        &panel,
        [&panel, audit, choice, beforeReset, &driver]()
        {
            const auto prompt = driver.activePrompt();
            if (audit)
            {
                audit->promptObservationCompleted = true;
            }
            if (!prompt)
            {
                return;
            }

            if (audit)
            {
                audit->confirmationSeen = true;
                audit->confirmationTitle = prompt->title;
                audit->confirmationText = prompt->text;
            }

            auto* const messageBox = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget()
                );
            if (!messageBox)
            {
                if (audit)
                {
                    audit->failSafeDismissedPrompt = true;
                }
                driver.accept(prompt->id);
                return;
            }

            auto* const destructiveButton = messageBox->findChild<QPushButton*>(
                QStringLiteral("promptDestructiveButton")
                );
            auto* const rejectButton = messageBox->findChild<QPushButton*>(
                QStringLiteral("promptRejectButton")
                );
            if (audit)
            {
                audit->resetText = destructiveButton
                    ? destructiveButton->text()
                    : QString();
                audit->cancelText = rejectButton
                    ? rejectButton->text()
                    : QString();
                audit->resetIsDestructive = destructiveButton
                    && messageBox->buttonRole(destructiveButton)
                        == QMessageBox::DestructiveRole;
                audit->cancelIsReject = rejectButton
                    && messageBox->buttonRole(rejectButton)
                        == QMessageBox::RejectRole;
                audit->cancelIsDefault = rejectButton
                    && messageBox->defaultButton() == rejectButton;
            }

            QPushButton* selectedButton = nullptr;
            if (choice == ResetPromptChoice::Cancel)
            {
                selectedButton = rejectButton;
            }
            else if (choice == ResetPromptChoice::Reset)
            {
                selectedButton = destructiveButton;
            }

            if (!selectedButton)
            {
                if (audit)
                {
                    audit->failSafeDismissedPrompt = true;
                }
                driver.accept(prompt->id);
                return;
            }

            if (choice == ResetPromptChoice::Reset && beforeReset)
            {
                beforeReset();
            }

            // If the selected action unexpectedly produces another modal
            // prompt, capture and dismiss it while the action call is blocked.
            QTimer::singleShot(
                0,
                &panel,
                [&panel, audit, &driver]()
                {
                    const auto followup = driver.activePrompt();
                    if (!followup)
                    {
                        return;
                    }

                    if (audit)
                    {
                        audit->followupPromptSeen = true;
                        audit->followupTitle = followup->title;
                        audit->followupText = followup->text;
                    }

                    auto* const followupBox = qobject_cast<QMessageBox*>(
                        QApplication::activeModalWidget()
                        );
                    if (!followupBox)
                    {
                        driver.accept(followup->id);
                        return;
                    }

                    if (audit)
                    {
                        audit->followupIsWarning =
                            followupBox->icon() == QMessageBox::Warning;
                    }
                    if (QPushButton* const acknowledge =
                            followupBox->findChild<QPushButton*>(
                                QStringLiteral("promptAcceptButton")
                                ))
                    {
                        acknowledge->click();
                    }
                    else
                    {
                        driver.accept(followup->id);
                    }
                }
                );

            selectedButton->click();
            if (audit)
            {
                audit->selectedButtonClicked = true;
            }
        }
        );

    QTimer failSafe;
    failSafe.setSingleShot(true);
    QObject::connect(
        &failSafe,
        &QTimer::timeout,
        &panel,
        [&driver, audit]()
        {
            const auto prompt = driver.activePrompt();
            if (prompt)
            {
                if (audit)
                {
                    audit->failSafeDismissedPrompt = true;
                }
                driver.accept(prompt->id);
                return;
            }

            if (QWidget* const modal = QApplication::activeModalWidget())
            {
                if (audit)
                {
                    audit->failSafeDismissedPrompt = true;
                }
                modal->close();
            }
        }
        );

    promptObserver.start(0);
    failSafe.start(5000);
    resetButton->click();
    QCoreApplication::processEvents();
    promptObserver.stop();
    failSafe.stop();
}

QQuickItem* calendarRoot(CalendarPage& page)
{
    const QList<QQuickWidget*> calendarViews =
        page.findChildren<QQuickWidget*>();
    if (calendarViews.size() != 1)
    {
        return nullptr;
    }
    return calendarViews.constFirst()->rootObject();
}

CalendarEventModel* calendarModel(CalendarPage& page)
{
    const QList<QQuickWidget*> calendarViews =
        page.findChildren<QQuickWidget*>();
    if (calendarViews.size() != 1)
    {
        return nullptr;
    }

    QObject* const provider = calendarViews.constFirst()
        ->rootContext()
        ->contextProperty(QStringLiteral("calendarEventProvider"))
        .value<QObject*>();
    return qobject_cast<CalendarEventModel*>(provider);
}

void appendQuickItems(QQuickItem* parent, QList<QQuickItem*>& items)
{
    if (!parent)
    {
        return;
    }

    for (QQuickItem* const child : parent->childItems())
    {
        items.append(child);
        appendQuickItems(child, items);
    }
}

QQuickItem* monthGridCell(QQuickItem* root, const QDate& date)
{
    QList<QQuickItem*> items;
    appendQuickItems(root, items);

    for (QQuickItem* const item : items)
    {
        const QMetaObject* const metaObject = item->metaObject();
        if (
            metaObject->indexOfProperty("dayEvents") < 0
            || metaObject->indexOfProperty("activeMonth") < 0
            || metaObject->indexOfProperty("year") < 0
            || metaObject->indexOfProperty("month") < 0
            || metaObject->indexOfProperty("day") < 0
            || !item->property("activeMonth").toBool()
            )
        {
            continue;
        }

        const QDate cellDate(
            item->property("year").toInt(),
            item->property("month").toInt() + 1,
            item->property("day").toInt()
            );
        if (cellDate == date)
        {
            return item;
        }
    }

    return nullptr;
}

bool containsEventTitle(const QVariantList& events, const QString& title)
{
    for (const QVariant& event : events)
    {
        if (event.toMap().value(QStringLiteral("title")).toString() == title)
        {
            return true;
        }
    }
    return false;
}

}

class CalendarPreferencesRestoreDefaultsTests final : public QObject
{
    Q_OBJECT

private slots:
    void restoreDefaultsStagesVisibleSchedulesWithoutPersistence();
    void resetEventsMatchesConfirmationDeleteFailureAndRefreshParity();
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

void CalendarPreferencesRestoreDefaultsTests::
resetEventsMatchesConfirmationDeleteFailureAndRefreshParity()
{
    const QString eventTitle = QStringLiteral("F387 Reset Fixture Event");
    const QDate eventDate = QDate::currentDate();
    const QDate firstOfMonth(eventDate.year(), eventDate.month(), 1);

    QJsonObject transcript;

    CalendarResetWorkspace unavailableWorkspace;
    QString setupError;
    QVERIFY2(
        initializeCalendarResetWorkspace(
            unavailableWorkspace,
            eventDate,
            &setupError
            ),
        qPrintable(setupError)
        );
    CalendarPreferencesPanel unavailablePanel(
        nullptr,
        &unavailableWorkspace.services
        );
    QSignalSpy unavailableChanged(
        &unavailablePanel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(unavailableChanged.isValid());

    unavailableWorkspace.services.closeDatabase();
    ResetPromptAudit unavailablePrompt;
    clickResetAndRespond(
        unavailablePanel,
        ResetPromptChoice::ObserveOnly,
        &unavailablePrompt
        );
    QVERIFY(unavailablePrompt.promptObservationCompleted);
    QVERIFY(!unavailablePrompt.confirmationSeen);
    QVERIFY(!unavailablePrompt.followupPromptSeen);
    QVERIFY(!unavailablePrompt.failSafeDismissedPrompt);
    QLabel* const unavailableStatus =
        calendarResetStatusLabel(unavailablePanel);
    QVERIFY(unavailableStatus);
    QVERIFY(unavailableStatus->text().isEmpty());
    QCOMPARE(unavailableChanged.count(), 0);

    const Status unavailableReopened =
        unavailableWorkspace.services.openDatabase(
            unavailableWorkspace.databasePath
            );
    if (!unavailableReopened)
    {
        QFAIL(qPrintable(unavailableReopened.error()));
    }
    bool unavailableEventPersisted = false;
    QVERIFY2(
        calendarEventExists(
            unavailableWorkspace.services,
            unavailableWorkspace.eventId,
            &unavailableEventPersisted,
            &setupError
            ),
        qPrintable(setupError)
        );
    QVERIFY(unavailableEventPersisted);

    transcript.insert(QStringLiteral("unavailable"), QJsonObject{
        {QStringLiteral("eventPersistedAfterReopen"), unavailableEventPersisted},
        {QStringLiteral("promptSeen"), unavailablePrompt.confirmationSeen},
        {QStringLiteral("signalCount"), unavailableChanged.count()},
        {QStringLiteral("successStatusEmpty"), unavailableStatus->text().isEmpty()}
    });

    CalendarResetWorkspace cancelWorkspace;
    setupError.clear();
    QVERIFY2(
        initializeCalendarResetWorkspace(
            cancelWorkspace,
            eventDate,
            &setupError
            ),
        qPrintable(setupError)
        );
    CalendarPreferencesPanel cancelPanel(nullptr, &cancelWorkspace.services);
    QSignalSpy cancelChanged(
        &cancelPanel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(cancelChanged.isValid());

    ResetPromptAudit cancelPrompt;
    clickResetAndRespond(
        cancelPanel,
        ResetPromptChoice::Cancel,
        &cancelPrompt
        );
    QVERIFY(cancelPrompt.promptObservationCompleted);
    QVERIFY(cancelPrompt.confirmationSeen);
    QCOMPARE(cancelPrompt.confirmationTitle, QStringLiteral("Reset Calendar?"));
    QVERIFY(cancelPrompt.confirmationText.contains(
        QStringLiteral("permanently delete all calendar events")
        ));
    QVERIFY(cancelPrompt.confirmationText.contains(
        QStringLiteral("cannot be undone")
        ));
    QCOMPARE(cancelPrompt.resetText, QStringLiteral("Reset"));
    QCOMPARE(cancelPrompt.cancelText, QStringLiteral("Cancel"));
    QVERIFY(cancelPrompt.resetIsDestructive);
    QVERIFY(cancelPrompt.cancelIsReject);
    QVERIFY(cancelPrompt.cancelIsDefault);
    QVERIFY(cancelPrompt.selectedButtonClicked);
    QVERIFY(!cancelPrompt.followupPromptSeen);
    QVERIFY(!cancelPrompt.failSafeDismissedPrompt);
    QLabel* const cancelStatus = calendarResetStatusLabel(cancelPanel);
    QVERIFY(cancelStatus);
    QVERIFY(cancelStatus->text().isEmpty());
    QCOMPARE(cancelChanged.count(), 0);

    bool cancelEventPersisted = false;
    QVERIFY2(
        calendarEventExists(
            cancelWorkspace.services,
            cancelWorkspace.eventId,
            &cancelEventPersisted,
            &setupError
            ),
        qPrintable(setupError)
        );
    QVERIFY(cancelEventPersisted);

    transcript.insert(QStringLiteral("cancel"), QJsonObject{
        {QStringLiteral("cancelButton"), cancelPrompt.cancelText},
        {QStringLiteral("cancelIsDefault"), cancelPrompt.cancelIsDefault},
        {QStringLiteral("cancelIsReject"), cancelPrompt.cancelIsReject},
        {QStringLiteral("eventPersisted"), cancelEventPersisted},
        {QStringLiteral("promptTitle"), cancelPrompt.confirmationTitle},
        {QStringLiteral("resetButton"), cancelPrompt.resetText},
        {QStringLiteral("resetIsDestructive"), cancelPrompt.resetIsDestructive},
        {QStringLiteral("signalCount"), cancelChanged.count()},
        {QStringLiteral("successStatusEmpty"), cancelStatus->text().isEmpty()},
        {QStringLiteral("warningSeen"), cancelPrompt.followupPromptSeen}
    });

    CalendarResetWorkspace failureWorkspace;
    setupError.clear();
    QVERIFY2(
        initializeCalendarResetWorkspace(
            failureWorkspace,
            eventDate,
            &setupError
            ),
        qPrintable(setupError)
        );
    CalendarPreferencesPanel failurePanel(nullptr, &failureWorkspace.services);
    QSignalSpy failureChanged(
        &failurePanel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(failureChanged.isValid());

    ResetPromptAudit failurePrompt;
    clickResetAndRespond(
        failurePanel,
        ResetPromptChoice::Reset,
        &failurePrompt,
        [&failureWorkspace]()
        {
            failureWorkspace.services.closeDatabase();
        }
        );
    QVERIFY(failurePrompt.promptObservationCompleted);
    QVERIFY(failurePrompt.confirmationSeen);
    QCOMPARE(failurePrompt.confirmationTitle, QStringLiteral("Reset Calendar?"));
    QVERIFY(failurePrompt.selectedButtonClicked);
    QVERIFY(failurePrompt.followupPromptSeen);
    QCOMPARE(failurePrompt.followupTitle, QStringLiteral("Reset Calendar"));
    QVERIFY(failurePrompt.followupIsWarning);
    QVERIFY(!failurePrompt.followupText.isEmpty());
    QVERIFY(!failurePrompt.failSafeDismissedPrompt);
    QLabel* const failureStatus = calendarResetStatusLabel(failurePanel);
    QVERIFY(failureStatus);
    QVERIFY(failureStatus->text().isEmpty());
    QCOMPARE(failureChanged.count(), 0);

    const Status failureReopened =
        failureWorkspace.services.openDatabase(failureWorkspace.databasePath);
    if (!failureReopened)
    {
        QFAIL(qPrintable(failureReopened.error()));
    }
    bool failureEventPersisted = false;
    QVERIFY2(
        calendarEventExists(
            failureWorkspace.services,
            failureWorkspace.eventId,
            &failureEventPersisted,
            &setupError
            ),
        qPrintable(setupError)
        );
    QVERIFY(failureEventPersisted);

    transcript.insert(QStringLiteral("applyFailure"), QJsonObject{
        {QStringLiteral("eventPersistedAfterReopen"), failureEventPersisted},
        {QStringLiteral("signalCount"), failureChanged.count()},
        {QStringLiteral("successStatusEmpty"), failureStatus->text().isEmpty()},
        {QStringLiteral("warningIcon"), failurePrompt.followupIsWarning},
        {QStringLiteral("warningSeen"), failurePrompt.followupPromptSeen},
        {QStringLiteral("warningTitle"), failurePrompt.followupTitle}
    });

    CalendarResetWorkspace successWorkspace;
    setupError.clear();
    QVERIFY2(
        initializeCalendarResetWorkspace(
            successWorkspace,
            eventDate,
            &setupError
            ),
        qPrintable(setupError)
        );
    CalendarPage calendarPage(&successWorkspace.services);
    QQuickItem* const root = calendarRoot(calendarPage);
    QVERIFY(root);
    QVERIFY(root->setProperty(
        "shownDate",
        QDateTime(firstOfMonth, QTime(0, 0))
        ));
    QCOMPARE(
        root->property("shownDate").toDateTime().date(),
        firstOfMonth
        );
    calendarPage.show();

    CalendarEventModel* const model = calendarModel(calendarPage);
    QVERIFY(model);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month()),
        10000
        );
    const auto visibleEvent = [&]()
    {
        QQuickItem* const cell = monthGridCell(root, eventDate);
        return cell
            && containsEventTitle(
                cell->property("dayEvents").toList(),
                eventTitle
                );
    };
    QTRY_VERIFY_WITH_TIMEOUT(visibleEvent(), 10000);
    QCOMPARE(
        model->eventsForDate(
            eventDate.year(),
            eventDate.month(),
            eventDate.day()
            ).size(),
        1
        );

    CalendarPreferencesPanel successPanel(
        calendarPage.academicCalendarProvider(),
        &successWorkspace.services
        );
    QObject::connect(
        &successPanel,
        &CalendarPreferencesPanel::calendarPreferencesChanged,
        &calendarPage,
        &CalendarPage::calendarPreferencesChanged
        );
    QSignalSpy successChanged(
        &successPanel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(successChanged.isValid());
    QLabel* const successStatus = calendarResetStatusLabel(successPanel);
    QVERIFY(successStatus);
    const int revisionBeforeReset = model->revision();

    ResetPromptAudit successPrompt;
    clickResetAndRespond(
        successPanel,
        ResetPromptChoice::Reset,
        &successPrompt
        );
    QVERIFY(successPrompt.promptObservationCompleted);
    QVERIFY(successPrompt.confirmationSeen);
    QCOMPARE(successPrompt.confirmationTitle, QStringLiteral("Reset Calendar?"));
    QVERIFY(successPrompt.selectedButtonClicked);
    QVERIFY(!successPrompt.followupPromptSeen);
    QVERIFY(!successPrompt.failSafeDismissedPrompt);

    bool successEventPersisted = true;
    QVERIFY2(
        calendarEventExists(
            successWorkspace.services,
            successWorkspace.eventId,
            &successEventPersisted,
            &setupError
            ),
        qPrintable(setupError)
        );
    QVERIFY(!successEventPersisted);
    QCOMPARE(
        successStatus->text(),
        QStringLiteral("Calendar events reset to defaults.")
        );
    QCOMPARE(successChanged.count(), 1);
    QCOMPARE(successChanged.at(0).at(0).toBool(), true);

    QTRY_VERIFY_WITH_TIMEOUT(model->revision() > revisionBeforeReset, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month())
            && !model->isLoading()
            && model->eventsForDate(
                eventDate.year(),
                eventDate.month(),
                eventDate.day()
                ).isEmpty()
            && !visibleEvent(),
        10000
        );

    transcript.insert(QStringLiteral("success"), QJsonObject{
        {QStringLiteral("eventDeleted"), !successEventPersisted},
        {QStringLiteral("eventVisibleBefore"), true},
        {QStringLiteral("eventVisibleAfterRefresh"), visibleEvent()},
        {QStringLiteral("pageVisible"), calendarPage.isVisible()},
        {QStringLiteral("signalCount"), successChanged.count()},
        {QStringLiteral("signalArgument"), successChanged.at(0).at(0).toBool()},
        {QStringLiteral("successStatus"), successStatus->text()},
        {QStringLiteral("visibleMonthLoadedAfterRefresh"),
            model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month())
            && !model->isLoading()
        }
    });

    const QByteArray canonicalTranscript =
        QJsonDocument(transcript).toJson(QJsonDocument::Compact);
    const QByteArray transcriptHash = QCryptographicHash::hash(
        canonicalTranscript,
        QCryptographicHash::Sha256
        ).toHex();
    std::printf(
        "F387_CALENDAR_PREFERENCES_EVENT_RESET_V1 %s\n",
        canonicalTranscript.constData()
        );
    std::printf(
        "F387_CALENDAR_PREFERENCES_EVENT_RESET_SHA256 %s\n",
        transcriptHash.constData()
        );
    std::fflush(stdout);
}

QTEST_MAIN(CalendarPreferencesRestoreDefaultsTests)

#include "calendar_preferences_restore_defaults_tests.moc"
