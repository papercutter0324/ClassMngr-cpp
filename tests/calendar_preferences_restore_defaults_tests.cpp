#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "data/repositories/settings_repository.h"
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
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSqlError>
#include <QSqlQuery>
#include <QSignalBlocker>
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

class DefaultLocaleRestorer final
{
public:
    DefaultLocaleRestorer()
        : m_originalLocale(QLocale())
    {
    }

    ~DefaultLocaleRestorer()
    {
        QLocale::setDefault(m_originalLocale);
    }

private:
    QLocale m_originalLocale;
};

using ClassMngr::Next::Application::AcademicCalendarSchedulePreferencesPort;
using ClassMngr::Next::Application::AcademicCalendarScheduleJsonText;
using ClassMngr::Next::Application::CalendarFirstDayOfWeek;
using ClassMngr::Next::Application::CalendarFirstDayOfWeekPreferencesPort;

struct CalendarPreferenceState final
{
    std::string scheduleJson;
    int scheduleWriteCount = 0;
    int scheduleWriteAttemptCount = 0;
    bool failScheduleReads = false;
    std::string scheduleReadFailure =
        "Academic calendar schedule read test failure.";
    bool failScheduleWrites = false;
    std::string scheduleWriteFailure =
        "Academic calendar schedule test failure.";
    CalendarFirstDayOfWeek firstDayOfWeek = CalendarFirstDayOfWeek::Sunday;
    bool failFirstDayReads = false;
    std::string firstDayReadFailure =
        "Calendar first-day read test failure.";
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
        std::shared_ptr<CalendarPreferenceState> state
        )
        : m_state(std::move(state))
    {
    }

    [[nodiscard]]
    ClassMngr::Next::Application::
        AcademicCalendarSchedulePreferencesReadResult read() const override
    {
        if (m_state->failScheduleReads)
        {
            return ClassMngr::Next::Application::
                AcademicCalendarSchedulePreferencesReadResult::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                    .message = m_state->scheduleReadFailure,
                    .recoverable = false
                });
        }

        return ClassMngr::Next::Application::
            AcademicCalendarSchedulePreferencesReadResult::success(
                AcademicCalendarScheduleJsonText{m_state->scheduleJson}
                );
    }

    [[nodiscard]]
    ClassMngr::Next::Application::
        AcademicCalendarSchedulePreferencesSaveResult write(
            const AcademicCalendarScheduleJsonText& payload
            ) const override
    {
        ++m_state->scheduleWriteAttemptCount;
        if (m_state->failScheduleWrites)
        {
            return ClassMngr::Next::Application::
                AcademicCalendarSchedulePreferencesSaveResult::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                    .message = m_state->scheduleWriteFailure,
                    .recoverable = false
                });
        }

        m_state->scheduleJson = payload.value();
        ++m_state->scheduleWriteCount;
        return ClassMngr::Next::Application::
            AcademicCalendarSchedulePreferencesSaveResult::success();
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

    [[nodiscard]]
        ClassMngr::Next::Application::
            CalendarFirstDayOfWeekPreferencesLoadResult load()
            const override
    {
        if (m_state->failFirstDayReads)
        {
            return ClassMngr::Next::Application::
                CalendarFirstDayOfWeekPreferencesLoadResult::failure({
                    .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                    .message = m_state->firstDayReadFailure,
                    .recoverable = false
                });
        }

        return ClassMngr::Next::Application::
            CalendarFirstDayOfWeekPreferencesLoadResult::success(
                m_state->firstDayOfWeek
                );
    }

    [[nodiscard]] SaveResult save(
        const CalendarFirstDayOfWeek firstDayOfWeek
        ) const override
    {
        ++m_state->firstDayWriteAttemptCount;
        if (m_state->failFirstDayWrites)
        {
            return SaveResult::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                .message = m_state->firstDayWriteFailure,
                .recoverable = false
            });
        }

        m_state->firstDayOfWeek = firstDayOfWeek;
        ++m_state->firstDayWriteCount;
        return SaveResult::success();
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

QGridLayout* calendarScheduleGrid(CalendarPreferencesPanel& panel)
{
    for (QGridLayout* const candidate : panel.findChildren<QGridLayout*>())
    {
        if (
            qobject_cast<QDateEdit*>(widgetAt(candidate, 2, 1))
            && qobject_cast<QSpinBox*>(widgetAt(candidate, 2, 2))
            && qobject_cast<QDateEdit*>(widgetAt(candidate, 2, 4))
            && qobject_cast<QSpinBox*>(widgetAt(candidate, 2, 5))
            )
        {
            return candidate;
        }
    }

    return nullptr;
}

bool executeCalendarPreferenceSql(
    ApplicationServices& services,
    const QString& statement
    )
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }

    QSqlQuery query(session->database());
    return query.exec(statement);
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
    void rejectsOutOfRangeWinterStartWithoutPersistence();
    void rejectsOutOfRangeInternalTermStartAndRefreshesWithoutPersistence();
    void failedScheduleSaveKeepsDraftAndRetrySucceeds();
    void scheduleReadFailureWarnsAndKeepsDraftUntilReloadSucceeds();
    void eventDisplayReadFailureBlocksOnlyItsGroup();
    void failedEventDisplaySaveRestoresPersistedPairAndRetrySucceeds();
    void unavailableEventDisplaySaveRestoresDefaultsAndDoesNotEmit();
    void firstDayReadFailureBlocksOnlyItsGroup();
    void failedFirstDaySaveRestoresPersistedValueAndRetrySucceeds();
    void resetEventsMatchesConfirmationDeleteFailureAndRefreshParity();
};

namespace
{

class RecordingCalendarPromptService final : public IUserPromptService
{
public:
    void showMessage(const PromptRequest& request) override
    {
        messages.append(request);
    }

    void showMessageAsync(const PromptRequest& request) override
    {
        messages.append(request);
    }

    PromptChoice confirm(const PromptRequest&) override
    {
        return PromptChoice::Rejected;
    }

    UnsavedChangesChoice confirmUnsavedChanges(
        const UnsavedChangesRequest& request
        ) override
    {
        Q_UNUSED(request);
        return UnsavedChangesChoice::Cancel;
    }

    QString chooseAction(const ActionPromptRequest&) override
    {
        return {};
    }

    QVector<PromptRequest> messages;
};

class CalendarPromptServiceOverride final
{
public:
    explicit CalendarPromptServiceOverride(IUserPromptService* service)
    {
        DialogServices::setUserPromptServiceForTesting(service);
    }

    ~CalendarPromptServiceOverride()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};

}

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
rejectsOutOfRangeWinterStartWithoutPersistence()
{
    AcademicCalendarSchedule initialSchedule;
    auto state = std::make_shared<CalendarPreferenceState>();
    state->scheduleJson = QJsonDocument(initialSchedule.toJson())
        .toJson(QJsonDocument::Compact)
        .toStdString();
    const std::string persistedBefore = state->scheduleJson;
    auto provider = makeProvider(state);
    const QJsonObject providerScheduleBefore = provider->schedule().toJson();
    const int revisionBefore = provider->revision();
    const QDate providerWinterStartBefore =
        provider->schedule()
            .yearSchedule(SchoolLevel::Elementary, 2027)
            .winterStart;
    QSignalSpy revisionChanged(
        provider.get(),
        &AcademicCalendarProvider::revisionChanged
        );
    QVERIFY(revisionChanged.isValid());

    ApplicationServices services;
    CalendarPreferencesPanel panel(provider.get(), &services);
    auto* const termYear = panel.findChild<QSpinBox*>(
        QStringLiteral("preferencesCalendarTermYear")
        );
    auto* const linkOption = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarLinkWinterSpring")
        );
    auto* const saveButton = panel.findChild<QPushButton*>(
        QStringLiteral("preferencesCalendarSaveTermSchedule")
        );
    QVERIFY(termYear);
    QVERIFY(linkOption);
    QVERIFY(saveButton);
    termYear->setValue(2027);
    linkOption->setChecked(false);

    QGridLayout* const scheduleFields = calendarScheduleGrid(panel);
    QVERIFY2(scheduleFields, "The visible term schedule grid was not found.");
    auto* const elementaryWinterStart = qobject_cast<QDateEdit*>(
        widgetAt(scheduleFields, 2, 1)
        );
    QVERIFY(elementaryWinterStart);

    const QDate previousFallStart =
        provider->schedule()
            .yearSchedule(SchoolLevel::Elementary, 2026)
            .termStart(AcademicTerm::Fall);
    const QDate proposedWinterStart = previousFallStart.addDays(54 * 7);
    QCOMPARE(proposedWinterStart.dayOfWeek(), static_cast<int>(Qt::Monday));
    QCOMPARE(
        static_cast<int>(previousFallStart.daysTo(proposedWinterStart)),
        54 * 7
        );
    elementaryWinterStart->setDate(proposedWinterStart);
    QCOMPARE(elementaryWinterStart->date(), proposedWinterStart);

    QSignalSpy preferencesChanged(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(preferencesChanged.isValid());
    RecordingCalendarPromptService prompts;
    CalendarPromptServiceOverride promptOverride(&prompts);

    saveButton->click();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Invalid Winter Start")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral(
            "Winter must begin 1 to 53 complete weeks after the preceding Fall term starts."
            )
        );
    QCOMPARE(state->scheduleWriteAttemptCount, 0);
    QCOMPARE(state->scheduleWriteCount, 0);
    QCOMPARE(state->scheduleJson, persistedBefore);
    QCOMPARE(provider->schedule().toJson(), providerScheduleBefore);
    QCOMPARE(
        provider->schedule()
            .yearSchedule(SchoolLevel::Elementary, 2027)
            .winterStart,
        providerWinterStartBefore
        );
    QCOMPARE(provider->revision(), revisionBefore);
    QCOMPARE(revisionChanged.count(), 0);
    QCOMPARE(preferencesChanged.count(), 0);
    QCOMPARE(elementaryWinterStart->date(), proposedWinterStart);
}

void CalendarPreferencesRestoreDefaultsTests::
rejectsOutOfRangeInternalTermStartAndRefreshesWithoutPersistence()
{
    AcademicCalendarSchedule initialSchedule;
    auto state = std::make_shared<CalendarPreferenceState>();
    state->scheduleJson = QJsonDocument(initialSchedule.toJson())
        .toJson(QJsonDocument::Compact)
        .toStdString();
    const std::string persistedBefore = state->scheduleJson;
    auto provider = makeProvider(state);
    const QJsonObject providerScheduleBefore = provider->schedule().toJson();
    const int revisionBefore = provider->revision();
    const AcademicYearSchedule scheduleBefore =
        provider->schedule().yearSchedule(SchoolLevel::Elementary, 2026);
    QSignalSpy revisionChanged(
        provider.get(),
        &AcademicCalendarProvider::revisionChanged
        );
    QVERIFY(revisionChanged.isValid());

    ApplicationServices services;
    CalendarPreferencesPanel panel(provider.get(), &services);
    auto* const termYear = panel.findChild<QSpinBox*>(
        QStringLiteral("preferencesCalendarTermYear")
        );
    QVERIFY(termYear);
    termYear->setValue(2026);

    QGridLayout* const scheduleFields = calendarScheduleGrid(panel);
    QVERIFY2(scheduleFields, "The visible term schedule grid was not found.");
    auto* const elementarySpringStart = qobject_cast<QDateEdit*>(
        widgetAt(scheduleFields, 3, 1)
        );
    auto* const elementarySpringWeeks = qobject_cast<QSpinBox*>(
        widgetAt(scheduleFields, 3, 2)
        );
    QVERIFY(elementarySpringStart);
    QVERIFY(elementarySpringWeeks);

    const QDate originalSpringStart =
        scheduleBefore.termStart(AcademicTerm::Spring);
    const QDate proposedSpringStart =
        scheduleBefore.termStart(AcademicTerm::Winter).addDays(54 * 7);
    QCOMPARE(proposedSpringStart.dayOfWeek(), static_cast<int>(Qt::Monday));
    QCOMPARE(
        static_cast<int>(
            scheduleBefore.termStart(AcademicTerm::Winter)
                .daysTo(proposedSpringStart)
            ),
        54 * 7
        );

    QSignalSpy preferencesChanged(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(preferencesChanged.isValid());
    RecordingCalendarPromptService prompts;
    CalendarPromptServiceOverride promptOverride(&prompts);

    elementarySpringStart->setDate(proposedSpringStart);

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Invalid Term Start")
        );
    QCOMPARE(
        prompts.messages.constFirst().message,
        QStringLiteral(
            "The start must be 1 to 53 complete weeks after the preceding term starts."
            )
        );
    QCOMPARE(elementarySpringStart->date(), originalSpringStart);
    QCOMPARE(elementarySpringWeeks->value(), scheduleBefore.weeks[1]);
    QCOMPARE(state->scheduleWriteAttemptCount, 0);
    QCOMPARE(state->scheduleWriteCount, 0);
    QCOMPARE(state->scheduleJson, persistedBefore);
    QCOMPARE(provider->schedule().toJson(), providerScheduleBefore);
    QCOMPARE(provider->revision(), revisionBefore);
    QCOMPARE(revisionChanged.count(), 0);
    QCOMPARE(preferencesChanged.count(), 0);
}

void CalendarPreferencesRestoreDefaultsTests::
failedScheduleSaveKeepsDraftAndRetrySucceeds()
{
    AcademicCalendarSchedule initialSchedule;
    auto state = std::make_shared<CalendarPreferenceState>();
    state->scheduleJson = QJsonDocument(initialSchedule.toJson())
        .toJson(QJsonDocument::Compact)
        .toStdString();
    const std::string persistedBefore = state->scheduleJson;
    auto provider = makeProvider(state);
    const QJsonObject providerScheduleBefore = provider->schedule().toJson();
    const int revisionBefore = provider->revision();
    QSignalSpy revisionChanged(
        provider.get(),
        &AcademicCalendarProvider::revisionChanged
        );
    QVERIFY(revisionChanged.isValid());

    ApplicationServices services;
    CalendarPreferencesPanel panel(provider.get(), &services);
    auto* const termYear = panel.findChild<QSpinBox*>(
        QStringLiteral("preferencesCalendarTermYear")
        );
    auto* const saveButton = panel.findChild<QPushButton*>(
        QStringLiteral("preferencesCalendarSaveTermSchedule")
        );
    QVERIFY(termYear);
    QVERIFY(saveButton);
    termYear->setValue(2026);

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
    auto* const elementarySummerWeeks = qobject_cast<QSpinBox*>(
        widgetAt(scheduleFields, 4, 2)
        );
    QVERIFY(elementarySummerWeeks);

    const int initialWeeks = elementarySummerWeeks->value();
    elementarySummerWeeks->setValue(initialWeeks + 1);
    const int proposedWeeks = elementarySummerWeeks->value();
    QCOMPARE(proposedWeeks, initialWeeks + 1);

    QSignalSpy preferencesChanged(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(preferencesChanged.isValid());
    RecordingCalendarPromptService prompts;
    CalendarPromptServiceOverride promptOverride(&prompts);

    state->failScheduleWrites = true;
    saveButton->click();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Academic Schedule Not Saved")
        );
    QCOMPARE(
        prompts.messages.constFirst().details,
        QString::fromStdString(state->scheduleWriteFailure)
        );
    QCOMPARE(preferencesChanged.count(), 0);
    QCOMPARE(state->scheduleWriteAttemptCount, 1);
    QCOMPARE(state->scheduleWriteCount, 0);
    QCOMPARE(state->scheduleJson, persistedBefore);
    QCOMPARE(provider->schedule().toJson(), providerScheduleBefore);
    QCOMPARE(provider->revision(), revisionBefore);
    QCOMPARE(revisionChanged.count(), 0);
    QCOMPARE(elementarySummerWeeks->value(), proposedWeeks);

    state->failScheduleWrites = false;
    saveButton->click();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(preferencesChanged.count(), 1);
    QCOMPARE(preferencesChanged.at(0).at(0).toBool(), false);
    QCOMPARE(state->scheduleWriteAttemptCount, 2);
    QCOMPARE(state->scheduleWriteCount, 1);
    QVERIFY(state->scheduleJson != persistedBefore);
    QCOMPARE(
        provider->schedule()
            .yearSchedule(SchoolLevel::Elementary, 2026)
            .weeks[2],
        proposedWeeks
        );
    QCOMPARE(elementarySummerWeeks->value(), proposedWeeks);
    QCOMPARE(provider->revision(), revisionBefore + 1);
    QCOMPARE(revisionChanged.count(), 1);
}

void CalendarPreferencesRestoreDefaultsTests::
scheduleReadFailureWarnsAndKeepsDraftUntilReloadSucceeds()
{
    auto state = std::make_shared<CalendarPreferenceState>();
    state->failScheduleReads = true;
    auto provider = makeProvider(state);
    const QJsonObject defaultSchedule = provider->schedule().toJson();
    const int revisionBefore = provider->revision();
    QSignalSpy revisionChanged(
        provider.get(),
        &AcademicCalendarProvider::revisionChanged
        );
    QVERIFY(revisionChanged.isValid());

    ApplicationServices services;
    CalendarPreferencesPanel panel(provider.get(), &services);
    auto* const termYear = panel.findChild<QSpinBox*>(
        QStringLiteral("preferencesCalendarTermYear")
        );
    auto* const saveButton = panel.findChild<QPushButton*>(
        QStringLiteral("preferencesCalendarSaveTermSchedule")
        );
    QVERIFY(termYear);
    QVERIFY(saveButton);
    termYear->setValue(2026);

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
    auto* const elementarySummerWeeks = qobject_cast<QSpinBox*>(
        widgetAt(scheduleFields, 4, 2)
        );
    QVERIFY(elementarySummerWeeks);

    const int initialWeeks = elementarySummerWeeks->value();
    elementarySummerWeeks->setValue(initialWeeks + 1);
    const int proposedWeeks = elementarySummerWeeks->value();
    QCOMPARE(proposedWeeks, initialWeeks + 1);

    QSignalSpy preferencesChanged(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(preferencesChanged.isValid());
    RecordingCalendarPromptService prompts;
    CalendarPromptServiceOverride promptOverride(&prompts);

    saveButton->click();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Academic Schedule Not Saved")
        );
    QCOMPARE(
        prompts.messages.constFirst().details,
        QString::fromStdString(state->scheduleReadFailure)
        );
    QCOMPARE(preferencesChanged.count(), 0);
    QCOMPARE(state->scheduleWriteAttemptCount, 0);
    QCOMPARE(state->scheduleWriteCount, 0);
    QCOMPARE(provider->schedule().toJson(), defaultSchedule);
    QCOMPARE(provider->revision(), revisionBefore);
    QCOMPARE(revisionChanged.count(), 0);
    QCOMPARE(elementarySummerWeeks->value(), proposedWeeks);

    state->failScheduleReads = false;
    provider->reload();
    saveButton->click();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(preferencesChanged.count(), 1);
    QCOMPARE(preferencesChanged.at(0).at(0).toBool(), false);
    QCOMPARE(state->scheduleWriteAttemptCount, 1);
    QCOMPARE(state->scheduleWriteCount, 1);
    QCOMPARE(
        provider->schedule()
            .yearSchedule(SchoolLevel::Elementary, 2026)
            .weeks[2],
        proposedWeeks
        );
    QCOMPARE(elementarySummerWeeks->value(), proposedWeeks);
    QCOMPARE(provider->revision(), revisionBefore + 1);
    QCOMPARE(revisionChanged.count(), 1);
}

void CalendarPreferencesRestoreDefaultsTests::
eventDisplayReadFailureBlocksOnlyItsGroup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const QString databasePath = directory.filePath(
        QStringLiteral("calendar-event-display-read-failure.tps")
        );
    QVERIFY(services.openDatabase(databasePath).has_value());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    SettingsRepository* const settings = session->settingsRepository();
    QVERIFY(settings);

    const QString showAllCampusesKey =
        QStringLiteral("calendar/showEventsAtAllCampuses");
    const QString hideStartOfTermKey =
        QStringLiteral("calendar/hideStartOfTermEvents");
    const QString firstDayKey = QStringLiteral("calendar/firstDayOfWeek");
    const QString unrelatedKey = QStringLiteral("calendar/unrelatedPreference");
    QVERIFY(settings->saveSettings({
        {showAllCampusesKey, false},
        {hideStartOfTermKey, true},
        {firstDayKey, 0},
        {unrelatedKey, QStringLiteral("preserved")}
    }));
    QVERIFY(executeCalendarPreferenceSql(
        services,
        QStringLiteral("ALTER TABLE app_settings RENAME TO preserved_app_settings")
        ));
    QVERIFY(executeCalendarPreferenceSql(
        services,
        QStringLiteral(
            "CREATE VIEW app_settings AS "
            "SELECT key, value FROM missing_calendar_settings"
            )
        ));

    auto preferenceState = std::make_shared<CalendarPreferenceState>();
    auto provider = makeProvider(preferenceState);
    CalendarPreferencesPanel panel(provider.get(), &services);
    auto* const showAllCampuses = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarShowAllCampuses")
        );
    auto* const hideStartOfTerm = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarHideStartOfTermEvents")
        );
    auto* const startWeekOnMonday = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarStartWeekOnMonday")
        );
    auto* const warning = panel.findChild<QLabel*>(
        QStringLiteral("preferencesCalendarLoadWarning")
        );
    QVERIFY(showAllCampuses);
    QVERIFY(hideStartOfTerm);
    QVERIFY(startWeekOnMonday);
    QVERIFY(warning);
    QVERIFY(!showAllCampuses->isEnabled());
    QVERIFY(!hideStartOfTerm->isEnabled());
    QVERIFY(startWeekOnMonday->isEnabled());
    QVERIFY(!warning->isHidden());
    QVERIFY(warning->text().contains(showAllCampusesKey));
    QVERIFY(!warning->text().trimmed().isEmpty());

    QSignalSpy preferencesChanged(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(preferencesChanged.isValid());
    RecordingCalendarPromptService prompts;
    CalendarPromptServiceOverride promptOverride(&prompts);

    showAllCampuses->setChecked(true);
    QVERIFY(showAllCampuses->isChecked());
    {
        const QSignalBlocker blocker(showAllCampuses);
        showAllCampuses->setChecked(false);
    }
    QCOMPARE(prompts.messages.size(), 0);
    QCOMPARE(preferenceState->firstDayWriteAttemptCount, 0);

    startWeekOnMonday->setChecked(true);
    QCOMPARE(preferenceState->firstDayOfWeek, CalendarFirstDayOfWeek::Monday);
    QCOMPARE(preferenceState->firstDayWriteAttemptCount, 1);
    QCOMPARE(preferenceState->firstDayWriteCount, 1);
    QCOMPARE(preferencesChanged.count(), 1);
    QCOMPARE(preferencesChanged.at(0).at(0).toBool(), false);
    QCOMPARE(prompts.messages.size(), 0);

    QSqlQuery query(session->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT key, value FROM preserved_app_settings ORDER BY key"
        )));
    QVariantMap preserved;
    while (query.next())
    {
        preserved.insert(query.value(0).toString(), query.value(1));
    }
    QCOMPARE(preserved.value(showAllCampusesKey).toBool(), false);
    QCOMPARE(preserved.value(hideStartOfTermKey).toBool(), true);
    QCOMPARE(preserved.value(firstDayKey).toInt(), 0);
    QCOMPARE(preserved.value(unrelatedKey).toString(), QStringLiteral("preserved"));
}

void CalendarPreferencesRestoreDefaultsTests::
failedEventDisplaySaveRestoresPersistedPairAndRetrySucceeds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const QString databasePath = directory.filePath(
        QStringLiteral("f661-calendar-event-display-preferences.tps")
        );
    QVERIFY(services.openDatabase(databasePath).has_value());

    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const settings = session->settingsRepository();
    QVERIFY(settings);

    const QString showAllCampusesKey =
        QStringLiteral("calendar/showEventsAtAllCampuses");
    const QString hideStartOfTermKey =
        QStringLiteral("calendar/hideStartOfTermEvents");
    QVERIFY(settings->saveSettings({
        {showAllCampusesKey, false},
        {hideStartOfTermKey, true}
    }));

    auto preferenceState = std::make_shared<CalendarPreferenceState>();
    auto provider = makeProvider(preferenceState);
    CalendarPreferencesPanel panel(provider.get(), &services);
    auto* const showAllCampuses = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarShowAllCampuses")
        );
    auto* const hideStartOfTerm = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarHideStartOfTermEvents")
        );
    auto* const startWeekOnMonday = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarStartWeekOnMonday")
        );
    QVERIFY(showAllCampuses);
    QVERIFY(hideStartOfTerm);
    QVERIFY(startWeekOnMonday);
    QVERIFY(!showAllCampuses->isChecked());
    QVERIFY(hideStartOfTerm->isChecked());
    QVERIFY(!startWeekOnMonday->isChecked());

    CalendarPage calendarPage(&services);
    QQuickItem* const calendarRootItem = calendarRoot(calendarPage);
    QVERIFY(calendarRootItem);
    const QDate visibleMonth(
        QDate::currentDate().year(),
        QDate::currentDate().month(),
        1
        );
    QVERIFY(calendarRootItem->setProperty(
        "shownDate",
        QDateTime(visibleMonth, QTime(0, 0))
        ));
    calendarPage.show();
    calendarPage.activate();
    CalendarEventModel* const model = calendarModel(calendarPage);
    QVERIFY(model);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(visibleMonth.year(), visibleMonth.month()),
        10000
        );
    QSignalSpy modelRefreshes(
        model,
        &CalendarEventModel::revisionChanged
        );
    QVERIFY(modelRefreshes.isValid());

    QSignalSpy preferencesChanged(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(preferencesChanged.isValid());
    QObject::connect(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged,
        &calendarPage,
        &CalendarPage::calendarPreferencesChanged
        );
    RecordingCalendarPromptService prompts;
    CalendarPromptServiceOverride promptOverride(&prompts);

    QVERIFY(executeCalendarPreferenceSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_calendar_event_display_preferences
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'calendar/showEventsAtAllCampuses'
            BEGIN
                SELECT RAISE(ABORT, 'forced calendar preference failure');
            END
        )")
        ));

    showAllCampuses->setChecked(true);

    QVERIFY(!showAllCampuses->isChecked());
    QVERIFY(hideStartOfTerm->isChecked());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Calendar Preferences Not Saved")
        );
    QVERIFY(prompts.messages.constFirst().details.contains(
        QStringLiteral("forced calendar preference failure")
        ));
    QCOMPARE(preferencesChanged.count(), 0);
    QCOMPARE(modelRefreshes.count(), 0);

    const auto persistedShow = settings->loadSetting(showAllCampusesKey);
    const auto persistedHide = settings->loadSetting(hideStartOfTermKey);
    QVERIFY(persistedShow);
    QVERIFY(persistedHide);
    QVERIFY(!persistedShow.value().toBool());
    QVERIFY(persistedHide.value().toBool());

    startWeekOnMonday->setChecked(true);

    QVERIFY(!startWeekOnMonday->isChecked());
    QCOMPARE(preferenceState->firstDayOfWeek, CalendarFirstDayOfWeek::Sunday);
    QCOMPARE(preferenceState->firstDayWriteAttemptCount, 0);
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(prompts.messages.constLast().severity, PromptSeverity::Warning);
    QVERIFY(prompts.messages.constLast().details.contains(
        QStringLiteral("forced calendar preference failure")
        ));
    QCOMPARE(preferencesChanged.count(), 0);
    QCOMPARE(modelRefreshes.count(), 0);

    QVERIFY(executeCalendarPreferenceSql(
        services,
        QStringLiteral("DROP TRIGGER fail_calendar_event_display_preferences")
        ));
    showAllCampuses->setChecked(true);

    QVERIFY(showAllCampuses->isChecked());
    QVERIFY(hideStartOfTerm->isChecked());
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(preferencesChanged.count(), 1);
    QCOMPARE(preferencesChanged.at(0).at(0).toBool(), false);
    QCOMPARE(modelRefreshes.count(), 1);

    const auto retriedShow = settings->loadSetting(showAllCampusesKey);
    const auto retriedHide = settings->loadSetting(hideStartOfTermKey);
    QVERIFY(retriedShow);
    QVERIFY(retriedHide);
    QVERIFY(retriedShow.value().toBool());
    QVERIFY(retriedHide.value().toBool());

    startWeekOnMonday->setChecked(true);
    QCOMPARE(
        preferenceState->firstDayOfWeek,
        CalendarFirstDayOfWeek::Monday
        );
    QVERIFY(showAllCampuses->isChecked());
    QVERIFY(hideStartOfTerm->isChecked());
    QCOMPARE(preferencesChanged.count(), 2);
    QCOMPARE(preferencesChanged.at(1).at(0).toBool(), false);
    QCOMPARE(modelRefreshes.count(), 1);
}

void CalendarPreferencesRestoreDefaultsTests::
unavailableEventDisplaySaveRestoresDefaultsAndDoesNotEmit()
{
    ApplicationServices services;
    QVERIFY(services.databaseSession());
    QVERIFY(!services.databaseSession()->isOpen());

    auto preferenceState = std::make_shared<CalendarPreferenceState>();
    auto provider = makeProvider(preferenceState);
    CalendarPreferencesPanel panel(provider.get(), &services);
    auto* const showAllCampuses = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarShowAllCampuses")
        );
    auto* const hideStartOfTerm = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarHideStartOfTermEvents")
        );
    QVERIFY(showAllCampuses);
    QVERIFY(hideStartOfTerm);
    QVERIFY(!showAllCampuses->isChecked());
    QVERIFY(!hideStartOfTerm->isChecked());

    QSignalSpy preferencesChanged(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(preferencesChanged.isValid());
    RecordingCalendarPromptService prompts;
    CalendarPromptServiceOverride promptOverride(&prompts);

    showAllCampuses->setChecked(true);
    QVERIFY(!showAllCampuses->isChecked());
    QVERIFY(!hideStartOfTerm->isChecked());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constLast().severity, PromptSeverity::Warning);
    QCOMPARE(
        prompts.messages.constLast().title,
        QStringLiteral("Calendar Preferences Not Saved")
        );
    QVERIFY(prompts.messages.constLast().details.contains(
        QStringLiteral("no database session is open")
        ));
    QCOMPARE(preferencesChanged.count(), 0);

    hideStartOfTerm->setChecked(true);
    QVERIFY(!showAllCampuses->isChecked());
    QVERIFY(!hideStartOfTerm->isChecked());
    QCOMPARE(prompts.messages.size(), 2);
    QCOMPARE(prompts.messages.constLast().severity, PromptSeverity::Warning);
    QCOMPARE(preferencesChanged.count(), 0);

    {
        const QSignalBlocker blocker(showAllCampuses);
        showAllCampuses->setChecked(true);
    }
    hideStartOfTerm->setChecked(true);
    QVERIFY(!showAllCampuses->isChecked());
    QVERIFY(!hideStartOfTerm->isChecked());
    QCOMPARE(prompts.messages.size(), 3);
    QCOMPARE(prompts.messages.constLast().severity, PromptSeverity::Warning);
    QCOMPARE(preferencesChanged.count(), 0);
}

void CalendarPreferencesRestoreDefaultsTests::
firstDayReadFailureBlocksOnlyItsGroup()
{
    const DefaultLocaleRestorer restoreLocale;
    QLocale::setDefault(QLocale(QStringLiteral("en_GB")));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const QString databasePath = directory.filePath(
        QStringLiteral("calendar-first-day-read-failure.tps")
        );
    QVERIFY(services.openDatabase(databasePath).has_value());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const settings = session->settingsRepository();
    QVERIFY(settings);

    const QString showAllCampusesKey =
        QStringLiteral("calendar/showEventsAtAllCampuses");
    const QString hideStartOfTermKey =
        QStringLiteral("calendar/hideStartOfTermEvents");
    const QString firstDayKey = QStringLiteral("calendar/firstDayOfWeek");
    const QString unrelatedKey = QStringLiteral("calendar/unrelatedPreference");
    QVERIFY(settings->saveSettings({
        {showAllCampusesKey, false},
        {hideStartOfTermKey, true},
        {firstDayKey, 0},
        {unrelatedKey, QStringLiteral("preserved")}
    }));

    auto preferenceState = std::make_shared<CalendarPreferenceState>();
    preferenceState->failFirstDayReads = true;
    auto provider = makeProvider(preferenceState);
    QCOMPARE(provider->firstDayOfWeek(), 1);
    QVERIFY(provider->firstDayOfWeekLoadError());
    CalendarPreferencesPanel panel(provider.get(), &services);
    auto* const showAllCampuses = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarShowAllCampuses")
        );
    auto* const hideStartOfTerm = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarHideStartOfTermEvents")
        );
    auto* const startWeekOnMonday = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarStartWeekOnMonday")
        );
    auto* const warning = panel.findChild<QLabel*>(
        QStringLiteral("preferencesCalendarLoadWarning")
        );
    QVERIFY(showAllCampuses);
    QVERIFY(hideStartOfTerm);
    QVERIFY(startWeekOnMonday);
    QVERIFY(warning);
    QVERIFY(showAllCampuses->isEnabled());
    QVERIFY(hideStartOfTerm->isEnabled());
    QVERIFY(!startWeekOnMonday->isEnabled());
    QVERIFY(startWeekOnMonday->isChecked());
    QVERIFY(!warning->isHidden());
    QVERIFY(warning->text().contains(QString::fromStdString(
        preferenceState->firstDayReadFailure
        )));

    QSignalSpy preferencesChanged(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(preferencesChanged.isValid());
    RecordingCalendarPromptService prompts;
    CalendarPromptServiceOverride promptOverride(&prompts);

    startWeekOnMonday->setChecked(false);
    {
        const QSignalBlocker blocker(startWeekOnMonday);
        startWeekOnMonday->setChecked(true);
    }
    QCOMPARE(preferenceState->firstDayWriteAttemptCount, 0);
    QCOMPARE(preferenceState->firstDayOfWeek, CalendarFirstDayOfWeek::Sunday);
    QCOMPARE(prompts.messages.size(), 0);
    QCOMPARE(preferencesChanged.count(), 0);

    showAllCampuses->setChecked(true);
    QVERIFY(showAllCampuses->isChecked());
    QVERIFY(hideStartOfTerm->isChecked());
    QCOMPARE(preferencesChanged.count(), 1);
    QCOMPARE(preferencesChanged.at(0).at(0).toBool(), false);
    QCOMPARE(prompts.messages.size(), 0);

    const auto persistedShow = settings->loadSetting(showAllCampusesKey);
    const auto persistedHide = settings->loadSetting(hideStartOfTermKey);
    const auto persistedFirstDay = settings->loadSetting(firstDayKey);
    const auto persistedUnrelated = settings->loadSetting(unrelatedKey);
    QVERIFY(persistedShow);
    QVERIFY(persistedHide);
    QVERIFY(persistedFirstDay);
    QVERIFY(persistedUnrelated);
    QCOMPARE(persistedShow->toBool(), true);
    QCOMPARE(persistedHide->toBool(), true);
    QCOMPARE(persistedFirstDay->toInt(), 0);
    QCOMPARE(persistedUnrelated->toString(), QStringLiteral("preserved"));

}

void CalendarPreferencesRestoreDefaultsTests::
failedFirstDaySaveRestoresPersistedValueAndRetrySucceeds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ApplicationServices services;
    const QString databasePath = directory.filePath(
        QStringLiteral("f662-calendar-first-day-preferences.tps")
        );
    QVERIFY(services.openDatabase(databasePath).has_value());
    DatabaseSession* const session = services.databaseSession();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const settings = session->settingsRepository();
    QVERIFY(settings);

    const QString firstDayKey =
        QStringLiteral("calendar/firstDayOfWeek");
    QVERIFY(settings->saveSetting(
        firstDayKey,
        static_cast<int>(CalendarFirstDayOfWeek::Sunday)
        ));

    CalendarPage calendarPage(&services);
    AcademicCalendarProvider* const provider =
        calendarPage.academicCalendarProvider();
    QVERIFY(provider);
    QCOMPARE(provider->firstDayOfWeek(), 0);
    const int revisionBefore = provider->revision();
    QSignalSpy firstDayChanged(
        provider,
        &AcademicCalendarProvider::firstDayOfWeekChanged
        );
    QVERIFY(firstDayChanged.isValid());
    QSignalSpy providerRevisionChanged(
        provider,
        &AcademicCalendarProvider::revisionChanged
        );
    QVERIFY(providerRevisionChanged.isValid());

    QQuickItem* const calendarRootItem = calendarRoot(calendarPage);
    QVERIFY(calendarRootItem);
    const QDate visibleMonth(
        QDate::currentDate().year(),
        QDate::currentDate().month(),
        1
        );
    QVERIFY(calendarRootItem->setProperty(
        "shownDate",
        QDateTime(visibleMonth, QTime(0, 0))
        ));
    calendarPage.show();
    calendarPage.activate();
    CalendarEventModel* const model = calendarModel(calendarPage);
    QVERIFY(model);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(visibleMonth.year(), visibleMonth.month()),
        10000
        );
    QSignalSpy modelRefreshes(
        model,
        &CalendarEventModel::revisionChanged
        );
    QVERIFY(modelRefreshes.isValid());

    CalendarPreferencesPanel panel(provider, &services);
    auto* const startWeekOnMonday = panel.findChild<QCheckBox*>(
        QStringLiteral("preferencesCalendarStartWeekOnMonday")
        );
    QVERIFY(startWeekOnMonday);
    QVERIFY(!startWeekOnMonday->isChecked());
    QSignalSpy preferencesChanged(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged
        );
    QVERIFY(preferencesChanged.isValid());
    QObject::connect(
        &panel,
        &CalendarPreferencesPanel::calendarPreferencesChanged,
        &calendarPage,
        &CalendarPage::calendarPreferencesChanged
        );
    RecordingCalendarPromptService prompts;
    CalendarPromptServiceOverride promptOverride(&prompts);

    QVERIFY(executeCalendarPreferenceSql(
        services,
        QStringLiteral(R"(
            CREATE TRIGGER fail_calendar_first_day_preference
            BEFORE INSERT ON app_settings
            WHEN NEW.key = 'calendar/firstDayOfWeek'
            BEGIN
                SELECT RAISE(ABORT, 'forced calendar first-day failure');
            END
        )")
        ));

    startWeekOnMonday->setChecked(true);

    QVERIFY(!startWeekOnMonday->isChecked());
    QCOMPARE(provider->firstDayOfWeek(), 0);
    QCOMPARE(provider->revision(), revisionBefore);
    QCOMPARE(firstDayChanged.count(), 0);
    QCOMPARE(providerRevisionChanged.count(), 0);
    QCOMPARE(preferencesChanged.count(), 0);
    QCOMPARE(modelRefreshes.count(), 0);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Calendar Preferences Not Saved")
        );
    QVERIFY(prompts.messages.constFirst().details.contains(
        QStringLiteral("forced calendar first-day failure")
        ));
    const auto persistedFailure = settings->loadSetting(firstDayKey);
    QVERIFY(persistedFailure);
    QCOMPARE(
        persistedFailure->toInt(),
        static_cast<int>(CalendarFirstDayOfWeek::Sunday)
        );

    QVERIFY(executeCalendarPreferenceSql(
        services,
        QStringLiteral("DROP TRIGGER fail_calendar_first_day_preference")
        ));
    startWeekOnMonday->setChecked(true);

    QVERIFY(startWeekOnMonday->isChecked());
    QCOMPARE(provider->firstDayOfWeek(), 1);
    QCOMPARE(provider->revision(), revisionBefore + 1);
    QCOMPARE(firstDayChanged.count(), 1);
    QCOMPARE(providerRevisionChanged.count(), 1);
    QCOMPARE(preferencesChanged.count(), 1);
    QCOMPARE(preferencesChanged.at(0).at(0).toBool(), false);
    QCOMPARE(prompts.messages.size(), 1);
    const auto persistedRetry = settings->loadSetting(firstDayKey);
    QVERIFY(persistedRetry);
    QCOMPARE(
        persistedRetry->toInt(),
        static_cast<int>(CalendarFirstDayOfWeek::Monday)
        );
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
