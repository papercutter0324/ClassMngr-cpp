#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/database/workspace_store.h"
#include "data/repositories/calendar_event_repository.h"
#include "data/repositories/settings_repository.h"
#include "domain/models/calendar_event.h"
#include "features/calendar/ui/calendar_event_dialog.h"
#include "features/calendar/ui/calendar_event_model.h"
#include "features/calendar/ui/calendar_page.h"
#include "features/calendar/ui/academic_calendar_provider.h"
#include "features/my_info/ui/my_workspace_page.h"
#include "next/platform/application_services_schedule_display_preferences_port.h"
#include "ui/shared/widgets/navigation_tab_widget.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <cstdio>

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QLineEdit>
#include <QMessageBox>
#include <QQmlContext>
#include <QPalette>
#include <QPushButton>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWidget>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimeEdit>
#include <QTimer>
#include <QTranslator>
#include <QWidget>
#include <QUuid>
#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <utility>

namespace
{

struct CalendarWorkspace final
{
    QTemporaryDir directory;
    ApplicationServices services;
    int preservedEventId = -1;
};

class CalendarPreferenceNoticeTranslator final : public QTranslator
{
public:
    QString translate(
        const char* context,
        const char* sourceText,
        const char* disambiguation = nullptr,
        const int n = -1
        ) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        if (!context || !sourceText
            || qstrcmp(context, "CalendarPage") != 0)
        {
            return {};
        }

        if (qstrcmp(
                sourceText,
                "Some calendar display preferences could not be loaded. "
                "Default values are being used."
                ) == 0)
        {
            return QStringLiteral("Localized calendar preference notice.");
        }
        return {};
    }
};

class ScopedTranslatorInstallation final
{
public:
    explicit ScopedTranslatorInstallation(QTranslator* translator)
        : m_translator(translator)
        , m_installed(QCoreApplication::installTranslator(m_translator))
    {
    }

    ~ScopedTranslatorInstallation()
    {
        if (m_installed)
        {
            QCoreApplication::removeTranslator(m_translator);
        }
    }

    [[nodiscard]] bool installed() const
    {
        return m_installed;
    }

private:
    QTranslator* m_translator = nullptr;
    bool m_installed = false;
};

struct PersistedEventRows final
{
    bool succeeded = false;
    QString error;
    QStringList rows;
};

struct EventDialogValues final
{
    QString title;
    QDate startDate;
    QTime startTime;
    QDate endDate;
    QTime endTime;
    QString eventType;
};

struct RepeatSeriesCreateDialogValues final
{
    EventDialogValues event;
    CalendarEventRepeatFrequency frequency =
        CalendarEventRepeatFrequency::Weekly;
    QDate repeatUntilDate;
};

QString databasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("calendar-page-event-mutation-parity-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool initializeWorkspace(CalendarWorkspace& workspace, QString* error)
{
    if (!workspace.directory.isValid())
    {
        if (error)
        {
            *error = QStringLiteral("Temporary directory is unavailable.");
        }
        return false;
    }

    const RepositoryStatus opened= workspace.services.openDatabase(
        databasePath(workspace.directory)
        );
    if (!opened)
    {
        if (error)
        {
            *error = toLegacyDiagnostic(opened.error());
        }
        return false;
    }

    CalendarEvent preserved;
    preserved.title = QStringLiteral("F375 Preserved Ordinary");
    preserved.eventType = QStringLiteral("Holiday");
    preserved.timeStatus = QStringLiteral("Timed");
    preserved.startDate = QDate(2026, 10, 22);
    preserved.startTime = QTime(11, 0);
    preserved.endDate = QDate(2026, 10, 22);
    preserved.endTime = QTime(12, 0);

    const auto saved = workspace.services.databaseSession()
        ->calendarEventRepository()
        ->saveCalendarEvent(preserved);
    if (!saved)
    {
        if (error)
        {
            *error = toLegacyDiagnostic(saved.error());
        }
        return false;
    }

    workspace.preservedEventId = *saved;
    return true;
}

PersistedEventRows persistedEventRows(ApplicationServices& services)
{
    PersistedEventRows result;
    QSqlQuery query(services.databaseSession()->database());
    if (!query.exec(QStringLiteral(
            "SELECT id, title, event_type, time_status, repeat_series_id, "
            "all_day, start_date, start_time, end_date, end_time "
            "FROM calendar_events ORDER BY id"
            )))
    {
        result.error = query.lastError().text();
        return result;
    }

    while (query.next())
    {
        QJsonArray row;
        row.append(query.value(0).toLongLong());
        row.append(query.value(1).toString());
        row.append(query.value(2).toString());
        row.append(query.value(3).toString());
        row.append(
            query.value(4).isNull()
                ? QJsonValue(QJsonValue::Null)
                : QJsonValue(query.value(4).toString())
            );
        row.append(query.value(5).toInt());
        row.append(query.value(6).toString());
        row.append(query.value(7).toString());
        row.append(query.value(8).toString());
        row.append(query.value(9).toString());
        result.rows.append(
            QString::fromUtf8(
                QJsonDocument(row).toJson(QJsonDocument::Compact)
                )
            );
    }

    result.succeeded = true;
    return result;
}

QString expectedEventRow(
    const qint64 id,
    const QString& title,
    const QString& eventType,
    const QString& startDate,
    const QString& startTime,
    const QString& endDate,
    const QString& endTime,
    const QString& repeatSeriesId = QString()
    )
{
    QJsonArray row;
    row.append(id);
    row.append(title);
    row.append(eventType);
    row.append(QStringLiteral("Timed"));
    row.append(
        repeatSeriesId.isEmpty()
            ? QJsonValue(QJsonValue::Null)
            : QJsonValue(repeatSeriesId)
        );
    row.append(0);
    row.append(startDate);
    row.append(startTime);
    row.append(endDate);
    row.append(endTime);
    return QString::fromUtf8(
        QJsonDocument(row).toJson(QJsonDocument::Compact)
        );
}

void appendQuickItems(
    QQuickItem* parent,
    QList<QQuickItem*>& items
    )
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

QQuickItem* monthGridCell(
    QQuickItem* root,
    const QDate& date
    )
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
            )
        {
            continue;
        }

        if (!item->property("activeMonth").toBool())
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

QStringList eventProjectionRows(const QVariantList& events)
{
    QStringList rows;
    for (const QVariant& event : events)
    {
        const QVariantMap values = event.toMap();
        QJsonArray row;
        row.append(values.value(QStringLiteral("id")).toInt());
        row.append(values.value(QStringLiteral("title")).toString());
        row.append(values.value(QStringLiteral("eventType")).toString());
        rows.append(
            QString::fromUtf8(
                QJsonDocument(row).toJson(QJsonDocument::Compact)
                )
            );
    }

    return rows;
}

qint64 eventIdForTitle(ApplicationServices& services, const QString& title)
{
    QSqlQuery query(services.databaseSession()->database());
    query.prepare(QStringLiteral(
        "SELECT id FROM calendar_events WHERE title=? ORDER BY id"
        ));
    query.addBindValue(title);
    if (!query.exec() || !query.next())
    {
        return -1;
    }
    return query.value(0).toLongLong();
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

bool applyDialogValues(
    CalendarEventDialog* dialog,
    const EventDialogValues& values,
    const bool deleting,
    QString* error,
    const bool thisAndFollowing = false
    )
{
    auto* buttonBox = dialog->findChild<QDialogButtonBox*>();
    if (!buttonBox)
    {
        if (error)
        {
            *error = QStringLiteral("Calendar event button box was not found.");
        }
        return false;
    }

    if (deleting)
    {
        for (QAbstractButton* button : buttonBox->buttons())
        {
            if (buttonBox->buttonRole(button)
                == QDialogButtonBox::DestructiveRole)
            {
                button->click();
                return true;
            }
        }

        if (error)
        {
            *error = QStringLiteral("Calendar event delete button was not found.");
        }
        return false;
    }

    auto* title = dialog->findChild<QLineEdit*>(
        QStringLiteral("calendarEventTitleEdit")
        );
    auto* startDate = dialog->findChild<QDateEdit*>(
        QStringLiteral("calendarEventStartDateEdit")
        );
    auto* startTime = dialog->findChild<QTimeEdit*>(
        QStringLiteral("calendarEventStartTimeEdit")
        );
    auto* endDate = dialog->findChild<QDateEdit*>(
        QStringLiteral("calendarEventEndDateEdit")
        );
    auto* endTime = dialog->findChild<QTimeEdit*>(
        QStringLiteral("calendarEventEndTimeEdit")
        );
    auto* save = buttonBox->button(QDialogButtonBox::Save);
    if (!title || !startDate || !startTime || !endDate || !endTime || !save)
    {
        if (error)
        {
            *error = QStringLiteral("Calendar event editor controls were incomplete.");
        }
        return false;
    }

    title->setText(values.title);
    startDate->setDate(values.startDate);
    startTime->setTime(values.startTime);
    endDate->setDate(values.endDate);
    endTime->setTime(values.endTime);

    QRadioButton* selectedType = nullptr;
    for (QRadioButton* button : dialog->findChildren<QRadioButton*>())
    {
        if (button->property("eventType").toString() == values.eventType)
        {
            selectedType = button;
            break;
        }
    }
    if (!selectedType)
    {
        if (error)
        {
            *error = QStringLiteral("Calendar event type option was not found.");
        }
        return false;
    }

    selectedType->click();

    if (thisAndFollowing)
    {
        QRadioButton* scopeButton = nullptr;
        for (QRadioButton* button : dialog->findChildren<QRadioButton*>())
        {
            if (button->text()
                == QStringLiteral("This and following events"))
            {
                scopeButton = button;
                break;
            }
        }
        if (!scopeButton)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Calendar repeat-series scope option was not found."
                    );
            }
            return false;
        }

        scopeButton->click();
        if (dialog->seriesEditScope()
            != CalendarEventSeriesEditScope::ThisAndFollowingEvents)
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Calendar repeat-series scope option was not selected."
                    );
            }
            return false;
        }
    }

    save->click();
    return true;
}

bool dialogMatchesValues(
    CalendarEventDialog* dialog,
    const EventDialogValues& values
    )
{
    if (!dialog)
    {
        return false;
    }

    auto* const title = dialog->findChild<QLineEdit*>(
        QStringLiteral("calendarEventTitleEdit")
        );
    auto* const startDate = dialog->findChild<QDateEdit*>(
        QStringLiteral("calendarEventStartDateEdit")
        );
    auto* const startTime = dialog->findChild<QTimeEdit*>(
        QStringLiteral("calendarEventStartTimeEdit")
        );
    auto* const endDate = dialog->findChild<QDateEdit*>(
        QStringLiteral("calendarEventEndDateEdit")
        );
    auto* const endTime = dialog->findChild<QTimeEdit*>(
        QStringLiteral("calendarEventEndTimeEdit")
        );
    if (
        !title
        || !startDate
        || !startTime
        || !endDate
        || !endTime
        || title->text() != values.title
        || startDate->date() != values.startDate
        || startTime->time() != values.startTime
        || endDate->date() != values.endDate
        || endTime->time() != values.endTime
    )
    {
        return false;
    }

    for (QRadioButton* const button : dialog->findChildren<QRadioButton*>())
    {
        if (
            button->isChecked()
            && button->property("eventType").toString() == values.eventType
        )
        {
            return true;
        }
    }

    return false;
}

bool applyRepeatSeriesCreateDialogValues(
    CalendarEventDialog* dialog,
    const RepeatSeriesCreateDialogValues& values,
    QString* error
    )
{
    auto* const buttonBox = dialog->findChild<QDialogButtonBox*>();
    auto* const title = dialog->findChild<QLineEdit*>(
        QStringLiteral("calendarEventTitleEdit")
        );
    auto* const startDate = dialog->findChild<QDateEdit*>(
        QStringLiteral("calendarEventStartDateEdit")
        );
    auto* const startTime = dialog->findChild<QTimeEdit*>(
        QStringLiteral("calendarEventStartTimeEdit")
        );
    auto* const endDate = dialog->findChild<QDateEdit*>(
        QStringLiteral("calendarEventEndDateEdit")
        );
    auto* const endTime = dialog->findChild<QTimeEdit*>(
        QStringLiteral("calendarEventEndTimeEdit")
        );
    auto* const frequency = dialog->findChild<QComboBox*>(
        QStringLiteral("calendarEventRepeatFrequencyCombo")
        );
    auto* const repeatUntilDate = dialog->findChild<QDateEdit*>(
        QStringLiteral("calendarEventRepeatUntilDateEdit")
        );
    QCheckBox* repeat = nullptr;
    for (QCheckBox* const check : dialog->findChildren<QCheckBox*>())
    {
        if (check->text() == QStringLiteral("Repeating"))
        {
            repeat = check;
            break;
        }
    }
    QPushButton* const save = buttonBox
        ? buttonBox->button(QDialogButtonBox::Save)
        : nullptr;
    if (
        !buttonBox
        || !title
        || !startDate
        || !startTime
        || !endDate
        || !endTime
        || !frequency
        || !repeatUntilDate
        || !repeat
        || !save
    )
    {
        if (error)
        {
            *error = QStringLiteral(
                "Calendar repeat-series editor controls were incomplete."
                );
        }
        return false;
    }

    title->setText(values.event.title);
    startDate->setDate(values.event.startDate);
    startTime->setTime(values.event.startTime);
    endDate->setDate(values.event.endDate);
    endTime->setTime(values.event.endTime);

    QRadioButton* selectedType = nullptr;
    for (QRadioButton* const button : dialog->findChildren<QRadioButton*>())
    {
        if (button->property("eventType").toString()
            == values.event.eventType)
        {
            selectedType = button;
            break;
        }
    }
    if (!selectedType)
    {
        if (error)
        {
            *error = QStringLiteral("Calendar event type option was not found.");
        }
        return false;
    }
    selectedType->click();

    repeat->setChecked(true);
    const int frequencyIndex = frequency->findData(
        static_cast<int>(values.frequency)
        );
    if (frequencyIndex < 0)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Calendar repeat frequency option was not found."
                );
        }
        return false;
    }
    frequency->setCurrentIndex(frequencyIndex);
    repeatUntilDate->setDate(values.repeatUntilDate);
    if (!dialogMatchesValues(dialog, values.event))
    {
        if (error)
        {
            *error = QStringLiteral(
                "Calendar repeat-series event fields were not applied."
                );
        }
        return false;
    }

    save->click();
    return true;
}

bool dialogMatchesRepeatSeriesCreateValues(
    CalendarEventDialog* dialog,
    const RepeatSeriesCreateDialogValues& values
    )
{
    return dialogMatchesValues(dialog, values.event)
        && dialog->repeatEnabled()
        && dialog->repeatFrequency() == values.frequency
        && dialog->repeatUntilDate() == values.repeatUntilDate;
}

bool activateAndHandleDialog(
    CalendarPage& page,
    const std::function<bool(QQuickItem*)>& activate,
    const EventDialogValues& values,
    const bool deleting,
    QString* error,
    const bool thisAndFollowing = false
    )
{
    QQuickItem* const root = calendarRoot(page);
    if (!root)
    {
        if (error)
        {
            *error = QStringLiteral("Calendar QML root was not found.");
        }
        return false;
    }

    bool dialogHandled = false;
    QString interactionError;
    QTimer interactionTimer(&page);
    interactionTimer.setSingleShot(true);
    QObject::connect(
        &interactionTimer,
        &QTimer::timeout,
        &page,
        [&]
        {
            auto* dialog = qobject_cast<CalendarEventDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                interactionError = QStringLiteral(
                    "Calendar event dialog was not opened by page activation."
                    );
                return;
            }

            dialogHandled = applyDialogValues(
                dialog,
                values,
                deleting,
                &interactionError,
                thisAndFollowing
                );
        }
        );

    QTimer failSafeTimer(&page);
    failSafeTimer.setSingleShot(true);
    QObject::connect(
        &failSafeTimer,
        &QTimer::timeout,
        &page,
        [&]
        {
            QWidget* const activeModal = QApplication::activeModalWidget();
            if (auto* dialog = qobject_cast<CalendarEventDialog*>(activeModal))
            {
                interactionError = QStringLiteral(
                    "Calendar event dialog did not close after the requested action."
                    );
                dialog->reject();
            }
            else if (activeModal)
            {
                interactionError = QStringLiteral(
                    "An unexpected modal prompt appeared after the calendar event action."
                    );
                activeModal->close();
            }
        }
        );

    interactionTimer.start(0);
    failSafeTimer.start(3000);
    const bool activated = activate(root);
    interactionTimer.stop();
    failSafeTimer.stop();

    if (!activated || !dialogHandled || !interactionError.isEmpty())
    {
        if (error)
        {
            *error = interactionError.isEmpty()
                ? QStringLiteral("Calendar page signal could not be invoked.")
                : interactionError;
        }
        return false;
    }
    return true;
}

QJsonObject transcriptEvent(
    const QString& role,
    const QString& title,
    const QString& eventType,
    const QString& startDate,
    const QString& startTime,
    const QString& endDate,
    const QString& endTime
    )
{
    return {
        {QStringLiteral("role"), role},
        {QStringLiteral("title"), title},
        {QStringLiteral("type"), eventType},
        {QStringLiteral("start_date"), startDate},
        {QStringLiteral("start_time"), startTime},
        {QStringLiteral("end_date"), endDate},
        {QStringLiteral("end_time"), endTime}
    };
}

void emitTranscript(const QJsonObject& transcript)
{
    qInfo().noquote()
        << QStringLiteral("F375_TRANSCRIPT ")
            + QString::fromUtf8(
                QJsonDocument(transcript).toJson(QJsonDocument::Compact)
                );
}

void emitF379Transcript(const QJsonObject& transcript)
{
    qInfo().noquote()
        << QStringLiteral("F379_TRANSCRIPT=")
            + QString::fromUtf8(
                QJsonDocument(transcript).toJson(QJsonDocument::Compact)
                );
}


QString upcomingRowJson(
    const int id,
    const QString& date,
    const QString& time,
    const QString& title,
    const QString& eventType
    )
{
    return QString::fromUtf8(
        QJsonDocument(QJsonArray{
            id,
            date,
            time,
            title,
            eventType
        }).toJson(QJsonDocument::Compact)
        );
}

QStringList visibleUpcomingRows(QWidget* scopePage)
{
    QStringList rows;
    if (!scopePage || !scopePage->layout())
    {
        return rows;
    }

    auto* const pageLayout =
        qobject_cast<QVBoxLayout*>(scopePage->layout());
    if (!pageLayout || pageLayout->count() < 2)
    {
        return rows;
    }

    QWidget* const listWidget =
        pageLayout->itemAt(1)->widget();
    auto* const listLayout =
        listWidget
            ? qobject_cast<QVBoxLayout*>(listWidget->layout())
            : nullptr;
    if (!listLayout)
    {
        return rows;
    }

    for (int index = 0; index < listLayout->count(); ++index)
    {
        QWidget* const widget =
            listLayout->itemAt(index)->widget();
        auto* const row =
            qobject_cast<QFrame*>(widget);
        if (
            !row
            || row->objectName()
                != QStringLiteral("upcomingCalendarEventRow")
        )
        {
            continue;
        }

        auto* const rowLayout =
            qobject_cast<QHBoxLayout*>(row->layout());
        if (!rowLayout || rowLayout->count() != 4)
        {
            rows.append(QStringLiteral("<malformed-visible-row>"));
            continue;
        }

        auto* const date =
            qobject_cast<QLabel*>(rowLayout->itemAt(0)->widget());
        auto* const time =
            qobject_cast<QLabel*>(rowLayout->itemAt(1)->widget());
        auto* const title =
            qobject_cast<QLabel*>(rowLayout->itemAt(2)->widget());
        auto* const eventType =
            qobject_cast<QPushButton*>(rowLayout->itemAt(3)->widget());
        if (!date || !time || !title || !eventType)
        {
            rows.append(QStringLiteral("<malformed-visible-row>"));
            continue;
        }

        rows.append(upcomingRowJson(
            row->property("calendarEventId").toInt(),
            date->text(),
            time->text(),
            title->text(),
            eventType->text()
            ));
    }

    return rows;
}

QString visibleUpcomingEmptyLabel(QWidget* scopePage)
{
    if (!scopePage || !scopePage->layout())
    {
        return {};
    }

    auto* const pageLayout =
        qobject_cast<QVBoxLayout*>(scopePage->layout());
    if (!pageLayout || pageLayout->count() < 2)
    {
        return {};
    }

    QWidget* const listWidget =
        pageLayout->itemAt(1)->widget();
    if (!listWidget)
    {
        return {};
    }

    for (QLabel* const label : listWidget->findChildren<QLabel*>())
    {
        if (label->objectName() == QStringLiteral("sectionSubtitle"))
        {
            return label->text();
        }
    }

    return {};
}

QString visibleUpcomingFailure(QWidget* scopePage)
{
    if (!scopePage)
    {
        return {};
    }

    auto* const label = scopePage->findChild<QLabel*>(
        QStringLiteral("calendarUpcomingEventsError")
        );
    return label ? label->text() : QString();
}

QLabel* displayPreferencesNotice(CalendarPage& page)
{
    return page.findChild<QLabel*>(
        QStringLiteral("calendarDisplayPreferencesNotice")
        );
}

QPushButton* upcomingRetryButton(QWidget* scopePage)
{
    return scopePage
        ? scopePage->findChild<QPushButton*>(
            QStringLiteral("calendarUpcomingEventsRetry")
            )
        : nullptr;
}

NavigationTabWidget* upcomingTabs(CalendarPage& page)
{
    return page.findChild<NavigationTabWidget*>(
        QStringLiteral("calendarUpcomingTabs")
        );
}

int saveFixtureEvent(
    CalendarWorkspace& workspace,
    const CalendarEvent& event,
    QString* error
    )
{
    const auto saved = workspace.services.databaseSession()
        ->calendarEventRepository()
        ->saveCalendarEvent(event);
    if (!saved)
    {
        if (error)
        {
            *error = toLegacyDiagnostic(saved.error());
        }
        return -1;
    }

    return *saved;
}

bool removePreservedFixtureEvent(
    CalendarWorkspace& workspace,
    QString* error
    )
{
    QSqlQuery query(
        workspace.services.databaseSession()->database()
        );
    query.prepare(QStringLiteral(
        "DELETE FROM calendar_events WHERE id=?"
        ));
    query.addBindValue(workspace.preservedEventId);
    if (!query.exec())
    {
        if (error)
        {
            *error = query.lastError().text();
        }
        return false;
    }

    return true;
}

bool executeWorkspaceSql(
    CalendarWorkspace& workspace,
    const QString& sql,
    QString* error
    )
{
    QSqlQuery query(
        workspace.services.databaseSession()->database()
        );
    if (query.exec(sql))
    {
        return true;
    }

    if (error)
    {
        *error = query.lastError().text();
    }
    return false;
}

struct SeriesEditFailureInteraction final
{
    QString error;
    int stage = 0;
    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    bool warningIcon = false;
    bool fieldsRetainedWhileWarningWasShown = false;
    bool failedSaveLeftRowsUnchanged = false;
    bool failureTriggerRemoved = false;
    bool sameDialogWasReopened = false;
    bool fieldsRetainedBeforeRetry = false;
    bool eventScopeRetained = false;
    bool retrySubmitted = false;
    bool cancelSubmitted = false;
    bool activated = false;
};

SeriesEditFailureInteraction interactWithSeriesEditFailure(
    CalendarWorkspace& workspace,
    CalendarPage& page,
    const qint64 selectedId,
    const EventDialogValues& values,
    const QStringList& expectedRowsBeforeFailure,
    const QString& triggerName,
    const CalendarEventSeriesEditScope expectedScope,
    const bool cancelAfterFailure
    )
{
    SeriesEditFailureInteraction result;
    QQuickItem* const root = calendarRoot(page);
    if (!root)
    {
        result.error = QStringLiteral("Calendar QML root was not found.");
        return result;
    }

    QPointer<CalendarEventDialog> originalDialog;
    QTimer interactionTimer;
    interactionTimer.setInterval(10);
    QObject::connect(
        &interactionTimer,
        &QTimer::timeout,
        &page,
        [&]
        {
            QWidget* const activeModal = QApplication::activeModalWidget();
            if (result.stage == 0)
            {
                auto* const dialog = qobject_cast<CalendarEventDialog*>(
                    activeModal
                    );
                if (!dialog)
                {
                    return;
                }

                originalDialog = dialog;
                result.stage = 1;
                const bool thisAndFollowing = expectedScope
                    == CalendarEventSeriesEditScope::ThisAndFollowingEvents;
                if (!applyDialogValues(
                        dialog,
                        values,
                        false,
                        &result.error,
                        thisAndFollowing
                        ))
                {
                    result.stage = 4;
                    dialog->reject();
                    return;
                }
                result.eventScopeRetained =
                    dialog->seriesEditScope() == expectedScope;
                return;
            }

            if (result.stage == 1)
            {
                auto* const warning = qobject_cast<QMessageBox*>(activeModal);
                if (!warning)
                {
                    if (!activeModal)
                    {
                        return;
                    }
                    result.error = QStringLiteral(
                        "The failed repeat-series save did not show a warning."
                        );
                    result.stage = 4;
                    activeModal->close();
                    return;
                }

                ++result.warningCount;
                result.warningTitle = warning->windowTitle();
                result.warningText = warning->text();
                result.warningIcon = warning->icon() == QMessageBox::Warning;
                result.fieldsRetainedWhileWarningWasShown =
                    dialogMatchesValues(originalDialog, values);
                result.eventScopeRetained = result.eventScopeRetained
                    && originalDialog
                    && originalDialog->seriesEditScope() == expectedScope;
                const PersistedEventRows afterFailure = persistedEventRows(
                    workspace.services
                    );
                result.failedSaveLeftRowsUnchanged =
                    afterFailure.succeeded
                    && afterFailure.rows == expectedRowsBeforeFailure;

                QString sqlError;
                result.failureTriggerRemoved = executeWorkspaceSql(
                    workspace,
                    QStringLiteral("DROP TRIGGER %1").arg(triggerName),
                    &sqlError
                    );
                if (!result.failureTriggerRemoved)
                {
                    result.error = sqlError;
                    result.stage = 4;
                }
                else
                {
                    result.stage = 2;
                }
                warning->accept();
                return;
            }

            if (result.stage == 2)
            {
                auto* const dialog = qobject_cast<CalendarEventDialog*>(
                    activeModal
                    );
                if (!dialog)
                {
                    if (!activeModal)
                    {
                        return;
                    }
                    result.error = QStringLiteral(
                        "A different modal appeared before repeat-series retry."
                        );
                    result.stage = 4;
                    activeModal->close();
                    return;
                }

                result.sameDialogWasReopened = dialog == originalDialog.data();
                result.fieldsRetainedBeforeRetry =
                    dialogMatchesValues(dialog, values);
                result.eventScopeRetained = result.eventScopeRetained
                    && dialog->seriesEditScope() == expectedScope;
                auto* const buttonBox = dialog->findChild<QDialogButtonBox*>();
                QPushButton* const cancel = buttonBox
                    ? buttonBox->button(QDialogButtonBox::Cancel)
                    : nullptr;
                QPushButton* const save = buttonBox
                    ? buttonBox->button(QDialogButtonBox::Save)
                    : nullptr;
                if (cancelAfterFailure && !cancel)
                {
                    result.error = QStringLiteral(
                        "Calendar series cancel button was unavailable after failure."
                        );
                    result.stage = 4;
                    dialog->reject();
                    return;
                }
                if (!cancelAfterFailure && !save)
                {
                    result.error = QStringLiteral(
                        "Calendar series save button was unavailable on retry."
                        );
                    result.stage = 4;
                    dialog->reject();
                    return;
                }

                result.stage = 3;
                if (cancelAfterFailure)
                {
                    result.cancelSubmitted = true;
                    cancel->click();
                }
                else
                {
                    result.retrySubmitted = true;
                    save->click();
                }
                return;
            }

            if (result.stage == 3)
            {
                if (qobject_cast<QMessageBox*>(activeModal))
                {
                    result.error = QStringLiteral(
                        "The repeat-series retry failed after its trigger was removed."
                        );
                    result.stage = 4;
                    activeModal->close();
                }
                return;
            }

            if (result.stage == 4 && activeModal)
            {
                interactionTimer.stop();
                activeModal->close();
            }
        }
        );

    QTimer failSafeTimer;
    failSafeTimer.setInterval(3000);
    QObject::connect(
        &failSafeTimer,
        &QTimer::timeout,
        &page,
        [&]
        {
            if (result.stage != 3)
            {
                if (result.error.isEmpty())
                {
                    result.error = QStringLiteral(
                        "Repeat-series save retry interaction timed out."
                        );
                }
                if (QWidget* const activeModal = QApplication::activeModalWidget())
                {
                    activeModal->close();
                }
            }
        }
        );

    interactionTimer.start();
    failSafeTimer.start();
    result.activated = QMetaObject::invokeMethod(
        root,
        "eventActivated",
        Qt::DirectConnection,
        Q_ARG(QString, QString::number(selectedId))
        );
    interactionTimer.stop();
    failSafeTimer.stop();
    return result;
}

struct RepeatSeriesCreateFailureInteraction final
{
    QString error;
    int stage = 0;
    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    bool warningIcon = false;
    bool fieldsRetainedWhileWarningWasShown = false;
    bool failedCreateLeftRowsUnchanged = false;
    bool failureTriggerRemoved = false;
    bool sameDialogWasReopened = false;
    bool fieldsRetainedBeforeRetry = false;
    bool retrySubmitted = false;
    bool cancelSubmitted = false;
    bool activated = false;
};

RepeatSeriesCreateFailureInteraction interactWithRepeatSeriesCreateFailure(
    CalendarWorkspace& workspace,
    CalendarPage& page,
    const QDate& activationDate,
    const RepeatSeriesCreateDialogValues& values,
    const QStringList& expectedRowsBeforeFailure,
    const QString& triggerName,
    const bool cancelAfterFailure
    )
{
    RepeatSeriesCreateFailureInteraction result;
    QQuickItem* const root = calendarRoot(page);
    if (!root)
    {
        result.error = QStringLiteral("Calendar QML root was not found.");
        return result;
    }

    QPointer<CalendarEventDialog> originalDialog;
    QTimer interactionTimer;
    interactionTimer.setInterval(10);
    QObject::connect(
        &interactionTimer,
        &QTimer::timeout,
        &page,
        [&]
        {
            QWidget* const activeModal = QApplication::activeModalWidget();
            if (result.stage == 0)
            {
                auto* const dialog = qobject_cast<CalendarEventDialog*>(
                    activeModal
                    );
                if (!dialog)
                {
                    return;
                }

                originalDialog = dialog;
                result.stage = 1;
                if (!applyRepeatSeriesCreateDialogValues(
                        dialog,
                        values,
                        &result.error
                        ))
                {
                    result.stage = 4;
                    dialog->reject();
                }
                return;
            }

            if (result.stage == 1)
            {
                auto* const warning = qobject_cast<QMessageBox*>(activeModal);
                if (!warning)
                {
                    if (!activeModal)
                    {
                        return;
                    }
                    result.error = QStringLiteral(
                        "The failed repeat-series create did not show a warning."
                        );
                    result.stage = 4;
                    activeModal->close();
                    return;
                }

                ++result.warningCount;
                result.warningTitle = warning->windowTitle();
                result.warningText = warning->text();
                result.warningIcon = warning->icon() == QMessageBox::Warning;
                result.fieldsRetainedWhileWarningWasShown =
                    dialogMatchesRepeatSeriesCreateValues(originalDialog, values);
                const PersistedEventRows afterFailure = persistedEventRows(
                    workspace.services
                    );
                result.failedCreateLeftRowsUnchanged =
                    afterFailure.succeeded
                    && afterFailure.rows == expectedRowsBeforeFailure;

                QString sqlError;
                result.failureTriggerRemoved = executeWorkspaceSql(
                    workspace,
                    QStringLiteral("DROP TRIGGER %1").arg(triggerName),
                    &sqlError
                    );
                if (!result.failureTriggerRemoved)
                {
                    result.error = sqlError;
                    result.stage = 4;
                }
                else
                {
                    result.stage = 2;
                }
                warning->accept();
                return;
            }

            if (result.stage == 2)
            {
                auto* const dialog = qobject_cast<CalendarEventDialog*>(
                    activeModal
                    );
                if (!dialog)
                {
                    if (!activeModal)
                    {
                        return;
                    }
                    result.error = QStringLiteral(
                        "A different modal appeared before repeat-series create retry."
                        );
                    result.stage = 4;
                    activeModal->close();
                    return;
                }

                result.sameDialogWasReopened = dialog == originalDialog.data();
                result.fieldsRetainedBeforeRetry =
                    dialogMatchesRepeatSeriesCreateValues(dialog, values);
                auto* const buttonBox = dialog->findChild<QDialogButtonBox*>();
                QPushButton* const cancel = buttonBox
                    ? buttonBox->button(QDialogButtonBox::Cancel)
                    : nullptr;
                QPushButton* const save = buttonBox
                    ? buttonBox->button(QDialogButtonBox::Save)
                    : nullptr;
                if (cancelAfterFailure && !cancel)
                {
                    result.error = QStringLiteral(
                        "Calendar repeat-series cancel button was unavailable after failure."
                        );
                    result.stage = 4;
                    dialog->reject();
                    return;
                }
                if (!cancelAfterFailure && !save)
                {
                    result.error = QStringLiteral(
                        "Calendar repeat-series save button was unavailable on retry."
                        );
                    result.stage = 4;
                    dialog->reject();
                    return;
                }

                result.stage = 3;
                if (cancelAfterFailure)
                {
                    result.cancelSubmitted = true;
                    cancel->click();
                }
                else
                {
                    result.retrySubmitted = true;
                    save->click();
                }
                return;
            }

            if (result.stage == 3
                && qobject_cast<QMessageBox*>(activeModal))
            {
                result.error = QStringLiteral(
                    "The repeat-series retry failed after its trigger was removed."
                    );
                result.stage = 4;
                activeModal->close();
            }
            else if (result.stage == 4 && activeModal)
            {
                interactionTimer.stop();
                activeModal->close();
            }
        }
        );

    QTimer failSafeTimer;
    failSafeTimer.setInterval(3000);
    QObject::connect(
        &failSafeTimer,
        &QTimer::timeout,
        &page,
        [&]
        {
            if (result.stage != 3)
            {
                if (result.error.isEmpty())
                {
                    result.error = QStringLiteral(
                        "Repeat-series create retry interaction timed out."
                        );
                }
                if (QWidget* const activeModal = QApplication::activeModalWidget())
                {
                    activeModal->close();
                }
            }
        }
        );

    interactionTimer.start();
    failSafeTimer.start();
    result.activated = QMetaObject::invokeMethod(
        root,
        "dayActivated",
        Qt::DirectConnection,
        Q_ARG(int, activationDate.year()),
        Q_ARG(int, activationDate.month()),
        Q_ARG(int, activationDate.day())
        );
    interactionTimer.stop();
    failSafeTimer.stop();
    return result;
}

QPushButton* upcomingEventTypeButton(
    CalendarPage& page,
    const int eventId
    )
{
    for (QFrame* const row : page.findChildren<QFrame*>())
    {
        if (row->objectName()
                != QStringLiteral("upcomingCalendarEventRow")
            || row->property("calendarEventId").toInt() != eventId)
        {
            continue;
        }

        const QList<QPushButton*> buttons = row->findChildren<QPushButton*>();
        return buttons.isEmpty() ? nullptr : buttons.constFirst();
    }

    return nullptr;
}

QList<QPushButton*> eventTypeFilterButtons(
    CalendarPage& page,
    const QString& eventType
    )
{
    QList<QPushButton*> matching;
    for (QPushButton* const button : page.findChildren<QPushButton*>())
    {
        if (button->property("eventType").toString() == eventType)
        {
            matching.append(button);
        }
    }
    return matching;
}

int calendarColorUpdateAuditCount(
    CalendarWorkspace& workspace,
    QString* error
    )
{
    QSqlQuery query(
        workspace.services.databaseSession()->database()
        );
    if (!query.exec(QStringLiteral(
            "SELECT successful_updates "
            "FROM calendar_event_type_color_write_audit"
            ))
        || !query.next())
    {
        if (error)
        {
            *error = query.lastError().text();
        }
        return -1;
    }
    return query.value(0).toInt();
}

bool enable24HourFixtureTime(
    CalendarWorkspace& workspace,
    QString* error
    )
{
    ClassMngr::Next::Platform::
        ApplicationServicesScheduleDisplayPreferencesPort preferences(
            workspace.services
            );
    const auto saved = preferences.save({
        .use24HourTime = true,
        .showEnglishNames = false,
        .showWeekends = false,
        .showAllIntensiveHours = false,
        .testingAffectsM1 = false
    });
    if (!saved)
    {
        if (error)
        {
            *error = QStringLiteral(
                "Could not set 24-hour calendar display preference."
                );
        }
        return false;
    }

    return true;
}

QJsonArray transcriptRows(const QStringList& rows)
{
    QJsonArray result;
    for (const QString& row : rows)
    {
        result.append(row);
    }
    return result;
}

void emitF383Transcript(const QJsonObject& transcript)
{
    const QByteArray line =
        QByteArrayLiteral("F383_TRANSCRIPT=")
        + QJsonDocument(transcript).toJson(QJsonDocument::Compact);
    std::fwrite(line.constData(), 1, static_cast<std::size_t>(line.size()), stdout);
    std::fputc(10, stdout);
    std::fflush(stdout);
}

}

class CalendarPageEventMutationParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void workspaceCalendarResourcesReleaseAndRestore_data();
    void workspaceCalendarResourcesReleaseAndRestore();
    void capturesLegacyFullCalendarPageWhenConfigured();
    void ordinaryCreateEditAndDeleteUseCalendarPageSignals_data();
    void ordinaryCreateEditAndDeleteUseCalendarPageSignals();
    void staleEventActivationShowsWarningWithoutMutation();
    void ordinaryCreateAndEditKeepDialogOpenAfterSaveFailure_data();
    void ordinaryCreateAndEditKeepDialogOpenAfterSaveFailure();
    void repeatSeriesCreateKeepsDialogOpenAfterSaveFailure_data();
    void repeatSeriesCreateKeepsDialogOpenAfterSaveFailure();
    void repeatSeriesOccurrenceEditKeepsDialogOpenAfterSaveFailure_data();
    void repeatSeriesOccurrenceEditKeepsDialogOpenAfterSaveFailure();
    void repeatSeriesSuffixEditKeepsDialogOpenAfterSaveFailure_data();
    void repeatSeriesSuffixEditKeepsDialogOpenAfterSaveFailure();
    void repeatSeriesSuffixEditUsesCalendarPageSignalAndRefreshesProjection();
    void monthGridCellsMatchCalendarEventModelDateProjection();
    void currentMonthUpcomingRowsMatchVisibleCalendarRange();
    void currentMonthNavigationEvictsPriorMonthResources();
    void next30DaysIncludesBoundariesAndFiltersEventTypes();
    void campusReadFailureKeepsAllEventsVisible();
    void calendarDisplayPreferenceFailuresShowNoticeAndRecover();
    void calendarDisplayPreferenceFailuresAreDetectedIndependently();
    void calendarDisplayPreferenceDefaultsDoNotShowNotice();
    void unknownNonemptyCampusDoesNotUseEmptyFallback();
    void currentCampusIdAliasShowsMatchingCampusEvents();
    void nextTenUpcomingEventsUseStableVisibleOrderAndLimit();
    void completedEmptyNextTenSearchShowsItsEmptyLabel();
    void completedEmptyUpcomingRangeShowsItsEmptyLabel();
    void eventTypeColorPickerFailureResyncsAndRetries();
    void upcomingRangeFailuresOfferRetryAndRecover();
    void nextTenLookupFailuresOfferRetryAndRecover();
};

void CalendarPageEventMutationParityTests::workspaceCalendarResourcesReleaseAndRestore_data()
{
    QTest::addColumn<int>("scope");
    QTest::addColumn<bool>("leaveWorkspace");
    QTest::newRow("current-month-tab-leave") << 0 << false;
    QTest::newRow("next-30-workspace-leave") << 1 << true;
    QTest::newRow("next-10-tab-leave") << 2 << false;
}

void CalendarPageEventMutationParityTests::workspaceCalendarResourcesReleaseAndRestore()
{
    QFETCH(int, scope);
    QFETCH(bool, leaveWorkspace);
    CalendarWorkspace workspace;
    QString error;
    QVERIFY2(initializeWorkspace(workspace, &error), qPrintable(error));
    QVERIFY2(removePreservedFixtureEvent(workspace, &error), qPrintable(error));
    const QDate today = QDate::currentDate();
    const QDate visibleMonth = QDate(qMax(today.year(), 2026), today.month(), 1)
        .addMonths(2);
    const QDate eventDate = visibleMonth.addDays(9);
    CalendarEvent event;
    event.title = QStringLiteral("Retained Calendar event");
    event.eventType = QStringLiteral("Holiday");
    event.startDate = eventDate;
    event.endDate = eventDate;
    QVERIFY2(saveFixtureEvent(workspace, event, &error) > 0, qPrintable(error));

    MyWorkspacePage workspacePage(&workspace.services);
    workspacePage.setDatabaseOpen(true);
    workspacePage.show();
    workspacePage.openTab(WorkspaceTab::Calendar);
    CalendarPage* const page = workspacePage.calendarPage();
    QVERIFY(page);
    CalendarEventCache* const cache = page->findChild<CalendarEventCache*>();
    CalendarEventModel* const model = calendarModel(*page);
    AcademicCalendarProvider* const provider = page->academicCalendarProvider();
    QVERIFY(cache);
    QVERIFY(model);
    QVERIFY(provider);
    auto* root = calendarRoot(*page);
    QVERIFY(root);
    QVERIFY(root->setProperty("shownDate", QDateTime(visibleMonth, QTime(0, 0))));
    auto* tabs = upcomingTabs(*page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(scope);
    const auto buttons = page->findChildren<QPushButton*>();
    auto filter = std::find_if(buttons.cbegin(), buttons.cend(), [](QPushButton* button) {
        return button->property("eventType").toString() == QStringLiteral("Workshop");
    });
    QVERIFY(filter != buttons.cend());
    (*filter)->setChecked(false);

    for (int cycle = 0; cycle < 3; ++cycle)
    {
        QTRY_VERIFY_WITH_TIMEOUT(model->isMonthLoaded(visibleMonth.year(), visibleMonth.month()), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!cache->isLoading(), 5000);
        QCOMPARE(model->eventsForDate(eventDate.year(), eventDate.month(), eventDate.day()).size(), 1);
        const auto populated = page->runtimeMetrics();
        QVERIFY(populated.cacheEventCount > 0);
        QVERIFY(populated.loadedMonthCount > 0);
        QVERIFY(populated.cacheLoadedRangeCount > 0);
        QVERIFY(populated.cacheRetainedRangeCount > 0);
        QVERIFY(!page->findChildren<QWidget*>(QStringLiteral("upcomingCalendarEventRow")).isEmpty());
        if (scope == static_cast<int>(UpcomingEventsScope::Next10Events))
            QVERIFY(populated.nextTenSearchEnd.isValid());
        QPointer<QQuickWidget> oldView = page->findChild<QQuickWidget*>();
        QPointer<QQuickItem> oldRoot = calendarRoot(*page);
        QVERIFY(oldView);
        QVERIFY(oldRoot);

        if (leaveWorkspace)
            workspacePage.deactivate();
        else
            workspacePage.openTab(WorkspaceTab::Schedule);
        QVERIFY(oldView.isNull());
        QVERIFY(oldRoot.isNull());
        QCOMPARE(workspacePage.calendarPage(), page);
        QCOMPARE(page->findChild<CalendarEventCache*>(), cache);
        QCOMPARE(page->findChild<CalendarEventModel*>(), model);
        QCOMPARE(page->academicCalendarProvider(), provider);
        QVERIFY(page->needsRefresh());
        QVERIFY(page->findChildren<QQuickWidget*>().isEmpty());
        QVERIFY(!upcomingTabs(*page));
        QVERIFY(page->findChildren<QWidget*>(QStringLiteral("upcomingCalendarEventRow")).isEmpty());
        const auto released = page->runtimeMetrics();
        QCOMPARE(released.cacheEventCount, 0);
        QCOMPARE(released.cacheDateBucketCount, 0);
        QCOMPARE(released.cacheLoadedRangeCount, 0);
        QCOMPARE(released.cacheRetainedRangeCount, 0);
        QCOMPARE(released.loadedMonthCount, 0);
        QCOMPARE(released.onDemandRetainedRangeCount, 0);
        QCOMPARE(released.calendarViewObjectCount, 0);
        QVERIFY(released.pageWidgetCount < populated.pageWidgetCount);
        QVERIFY(!released.nextTenSearchEnd.isValid());
        QVERIFY(!released.nextTenSearchComplete);
        QVERIFY(!released.nextTenLookupPending);
        QVERIFY(model->eventsForDate(eventDate.year(), eventDate.month(), eventDate.day()).isEmpty());
        page->deactivate(); // Repeated release is safe and does not rebuild anything.
        QTRY_VERIFY_WITH_TIMEOUT(!cache->isLoading(), 5000);
        QCOMPARE(cache->eventCount(), 0);
        QCOMPARE(cache->loadedRangeCount(), 0);

        event.title = QStringLiteral("Fresh Calendar event %1").arg(cycle);
        event.startDate = eventDate.addDays(1);
        event.endDate = event.startDate;
        QVERIFY2(saveFixtureEvent(workspace, event, &error) > 0, qPrintable(error));
        if (leaveWorkspace)
            workspacePage.activate();
        else
            workspacePage.openTab(WorkspaceTab::Calendar);
        QCOMPARE(workspacePage.calendarPage(), page);
        QCOMPARE(page->findChildren<QQuickWidget*>().size(), 1);
        QCOMPARE(page->findChildren<CalendarEventCache*>().size(), 1);
        QCOMPARE(page->findChildren<CalendarEventModel*>().size(), 1);
        QCOMPARE(page->findChildren<AcademicCalendarProvider*>().size(), 1);
        QCOMPARE(calendarModel(*page), model);
        QCOMPARE(page->academicCalendarProvider(), provider);
        root = calendarRoot(*page);
        QVERIFY(root);
        QCOMPARE(root->property("shownDate").toDateTime().date(), visibleMonth);
        QCOMPARE(upcomingTabs(*page)->currentIndex(), scope);
        int disabledFilters = 0;
        for (auto* button : page->findChildren<QPushButton*>())
        {
            if (button->property("eventType").toString() == QStringLiteral("Workshop"))
            {
                QVERIFY(!button->isChecked());
                ++disabledFilters;
            }
        }
        QCOMPARE(disabledFilters, UpcomingEventsScopeCount);
        QTRY_VERIFY_WITH_TIMEOUT(model->isMonthLoaded(visibleMonth.year(), visibleMonth.month()), 5000);
        const QDate freshDate = eventDate.addDays(1);
        QCOMPARE(model->eventsForDate(freshDate.year(), freshDate.month(), freshDate.day()).size(), cycle + 1);
    }
}

void CalendarPageEventMutationParityTests::
capturesLegacyFullCalendarPageWhenConfigured()
{
    const QString outputDirectory = qEnvironmentVariable(
        "CLASSMNGR_LEGACY_CALENDAR_CAPTURE_DIR");
    if (outputDirectory.trimmed().isEmpty())
    {
        QSKIP("Set CLASSMNGR_LEGACY_CALENDAR_CAPTURE_DIR to write reference captures.");
    }
    QVERIFY(QDir().mkpath(outputDirectory));

    const QLocale previousLocale;
    const QPalette previousPalette = QApplication::palette();
    const QString previousStyleSheet = qApp->styleSheet();
    struct RestoreVisualEnvironment final
    {
        QLocale locale;
        QPalette palette;
        QString styleSheet;
        ~RestoreVisualEnvironment()
        {
            QLocale::setDefault(locale);
            QApplication::setPalette(palette);
                qApp->setStyleSheet(styleSheet);
        }
    } restoreEnvironment{
        previousLocale,
        previousPalette,
        previousStyleSheet
    };

    const QDate visibleMonth(2026, 10, 1);
    const QStringList locales{
        QStringLiteral("en_US"),
        QStringLiteral("ko_KR")
    };
    const QStringList themes{
        QStringLiteral("light"),
        QStringLiteral("dark")
    };
    QJsonArray captures;

    for (const QString& locale : locales)
    {
        const bool korean = locale == QStringLiteral("ko_KR");
        QLocale::setDefault(QLocale(locale));

        for (const QString& theme : themes)
        {
            const bool dark = theme == QStringLiteral("dark");
            QPalette palette = QApplication::style()->standardPalette();
            if (dark)
            {
                palette.setColor(QPalette::Window, QColor(QStringLiteral("#20242a")));
                palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#f3f4f6")));
                palette.setColor(QPalette::Base, QColor(QStringLiteral("#272c33")));
                palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#343b44")));
                palette.setColor(QPalette::Text, QColor(QStringLiteral("#f3f4f6")));
                palette.setColor(QPalette::Button, QColor(QStringLiteral("#343b44")));
                palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#f3f4f6")));
                palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#6487a5")));
                palette.setColor(QPalette::HighlightedText, Qt::white);
            }
            QApplication::setPalette(palette);

            const QString sourceDirectory = QString::fromUtf8(
                CLASSMNGR_CALENDAR_CAPTURE_SOURCE_DIR);
            QFile stylesheetFile(
                QDir(sourceDirectory).filePath(
                    QStringLiteral("resources/assets/styles/%1.qss").arg(theme))
                );
            QVERIFY2(stylesheetFile.open(QIODevice::ReadOnly | QIODevice::Text),
                qPrintable(QStringLiteral("Could not open %1 theme stylesheet.")
                    .arg(theme)));
            const QByteArray stylesheet = stylesheetFile.readAll();
            QVERIFY(!stylesheet.isEmpty());
            qApp->setStyleSheet(QString::fromUtf8(stylesheet));

            QTranslator koreanTranslator;
            std::unique_ptr<ScopedTranslatorInstallation> translatorInstallation;
            if (korean)
            {
                QVERIFY2(koreanTranslator.load(
                    QStringLiteral(":/i18n/ClassMngr_ko_KR.qm")),
                    "Could not load the compiled Korean application translation.");
                translatorInstallation = std::make_unique<ScopedTranslatorInstallation>(
                    &koreanTranslator);
                QVERIFY(translatorInstallation->installed());
            }

            CalendarWorkspace workspace;
            QString setupError;
            QVERIFY2(initializeWorkspace(workspace, &setupError),
                qPrintable(setupError));
            QVERIFY2(removePreservedFixtureEvent(workspace, &setupError),
                qPrintable(setupError));

            CalendarEvent meeting;
            meeting.title = QStringLiteral("Campus meeting");
            meeting.eventType = QStringLiteral("Meeting");
            meeting.timeStatus = QStringLiteral("Timed");
            meeting.startDate = QDate(2026, 10, 6);
            meeting.startTime = QTime(9, 30);
            meeting.endDate = meeting.startDate;
            meeting.endTime = QTime(10, 30);
            QVERIFY2(saveFixtureEvent(workspace, meeting, &setupError) > 0,
                qPrintable(setupError));

            CalendarEvent planning;
            planning.title = QStringLiteral("Term planning");
            planning.eventType = QStringLiteral("Workshop");
            planning.timeStatus = QStringLiteral("Timed");
            planning.startDate = QDate(2026, 10, 15);
            planning.startTime = QTime(9, 30);
            planning.endDate = planning.startDate;
            planning.endTime = QTime(10, 30);
            QVERIFY2(saveFixtureEvent(workspace, planning, &setupError) > 0,
                qPrintable(setupError));

            CalendarEvent holiday;
            holiday.title = QStringLiteral("School holiday");
            holiday.eventType = QStringLiteral("Holiday");
            holiday.timeStatus = QStringLiteral("All Day");
            holiday.allDay = true;
            holiday.startDate = QDate(2026, 10, 26);
            holiday.endDate = holiday.startDate;
            QVERIFY2(saveFixtureEvent(workspace, holiday, &setupError) > 0,
                qPrintable(setupError));

            CalendarPage page(&workspace.services);
            page.resize(1280, 920);
            auto* const root = calendarRoot(page);
            QVERIFY(root);
            QVERIFY(root->setProperty(
                "shownDate",
                QDateTime(visibleMonth, QTime(0, 0))));
            QCOMPARE(root->property("shownDate").toDateTime().date(), visibleMonth);
            QVERIFY(page.academicCalendarProvider());
            QVERIFY(page.academicCalendarProvider()->setFirstDayOfWeek(1));
            QCOMPARE(page.academicCalendarProvider()->firstDayOfWeek(), 1);

            NavigationTabWidget* const tabs = upcomingTabs(page);
            QVERIFY(tabs);
            tabs->setCurrentIndex(static_cast<int>(UpcomingEventsScope::CurrentMonth));
            QCOMPARE(tabs->currentIndex(),
                static_cast<int>(UpcomingEventsScope::CurrentMonth));

            page.show();
            QApplication::processEvents();
            QVERIFY(page.isVisible());

            CalendarEventModel* const model = calendarModel(page);
            CalendarEventCache* const cache = page.findChild<CalendarEventCache*>();
            QVERIFY(model);
            QVERIFY(cache);
            QTRY_VERIFY_WITH_TIMEOUT(
                model->isMonthLoaded(visibleMonth.year(), visibleMonth.month()),
                10000);
            QTRY_VERIFY_WITH_TIMEOUT(!cache->isLoading(), 10000);
            QCOMPARE(page.runtimeMetrics().cacheEventCount, 3);
            QCOMPARE(model->eventsForDate(2026, 10, 6).size(), 1);
            QCOMPARE(model->eventsForDate(2026, 10, 15).size(), 1);
            QCOMPARE(model->eventsForDate(2026, 10, 26).size(), 1);

            const QString expectedTitle = korean
                ? QStringLiteral("캘린더")
                : QStringLiteral("Calendar");
            bool titleFound = false;
            for (QLabel* const label : page.findChildren<QLabel*>())
            {
                if (label->text() == expectedTitle)
                {
                    titleFound = true;
                    break;
                }
            }
            QVERIFY2(titleFound,
                qPrintable(QStringLiteral("Calendar title did not render in %1.")
                    .arg(locale)));

            int eventFilterCount = 0;
            for (QPushButton* const button : page.findChildren<QPushButton*>())
            {
                if (button->property("eventType").toString().isEmpty())
                {
                    continue;
                }
                QVERIFY(button->isChecked());
                ++eventFilterCount;
            }
            QCOMPARE(eventFilterCount, 18);

            const QStringList upcomingRows = visibleUpcomingRows(tabs->currentWidget());
            QCOMPARE(upcomingRows.size(), 3);
            const QString joinedRows = upcomingRows.join(QLatin1Char('\n'));
            QVERIFY(joinedRows.contains(meeting.title));
            QVERIFY(joinedRows.contains(planning.title));
            QVERIFY(joinedRows.contains(holiday.title));

            auto* const scrollArea = page.findChild<QScrollArea*>();
            QVERIFY(scrollArea);
            const QString mode = QStringLiteral("%1-%2")
                .arg(korean ? QStringLiteral("ko") : QStringLiteral("en"), theme);

            scrollArea->verticalScrollBar()->setValue(0);
            QApplication::processEvents();
            const QImage gridImage = page.grab().toImage();
            QCOMPARE(gridImage.size(), QSize(1280, 920));
            const QString gridFile = QStringLiteral("legacy-calendar-%1-grid-top.png")
                .arg(mode);
            QVERIFY(gridImage.save(QDir(outputDirectory).filePath(gridFile)));
            captures.append(QJsonObject{
                {QStringLiteral("image"), gridFile},
                {QStringLiteral("locale"), locale},
                {QStringLiteral("theme"), theme},
                {QStringLiteral("state"), QStringLiteral("grid-top")},
                {QStringLiteral("width"), gridImage.width()},
                {QStringLiteral("height"), gridImage.height()}
            });

            scrollArea->verticalScrollBar()->setValue(
                scrollArea->verticalScrollBar()->maximum());
            QApplication::processEvents();
            const QImage upcomingImage = page.grab().toImage();
            QCOMPARE(upcomingImage.size(), QSize(1280, 920));
            const QString upcomingFile = QStringLiteral(
                "legacy-calendar-%1-upcoming.png").arg(mode);
            QVERIFY(upcomingImage.save(
                QDir(outputDirectory).filePath(upcomingFile)));
            captures.append(QJsonObject{
                {QStringLiteral("image"), upcomingFile},
                {QStringLiteral("locale"), locale},
                {QStringLiteral("theme"), theme},
                {QStringLiteral("state"), QStringLiteral("upcoming-scroll")},
                {QStringLiteral("width"), upcomingImage.width()},
                {QStringLiteral("height"), upcomingImage.height()}
            });

            translatorInstallation.reset();
        }
    }

    const QJsonObject manifest{
        {QStringLiteral("source"), QStringLiteral("Legacy CalendarPage")},
        {QStringLiteral("fixture"), QJsonObject{
            {QStringLiteral("visibleMonth"), QStringLiteral("2026-10")},
            {QStringLiteral("firstWeekday"), QStringLiteral("Monday")},
            {QStringLiteral("upcomingScope"), QStringLiteral("This month")},
            {QStringLiteral("eventTypeFilters"), QStringLiteral("all enabled")},
            {QStringLiteral("events"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("title"), QStringLiteral("Campus meeting")},
                    {QStringLiteral("eventType"), QStringLiteral("Meeting")},
                    {QStringLiteral("startDate"), QStringLiteral("2026-10-06")},
                    {QStringLiteral("startTime"), QStringLiteral("09:30")},
                    {QStringLiteral("endDate"), QStringLiteral("2026-10-06")},
                    {QStringLiteral("endTime"), QStringLiteral("10:30")},
                    {QStringLiteral("allDay"), false},
                    {QStringLiteral("color"), QStringLiteral("#8a4f5d")}
                },
                QJsonObject{
                    {QStringLiteral("title"), QStringLiteral("Term planning")},
                    {QStringLiteral("eventType"), QStringLiteral("Workshop")},
                    {QStringLiteral("startDate"), QStringLiteral("2026-10-15")},
                    {QStringLiteral("startTime"), QStringLiteral("09:30")},
                    {QStringLiteral("endDate"), QStringLiteral("2026-10-15")},
                    {QStringLiteral("endTime"), QStringLiteral("10:30")},
                    {QStringLiteral("allDay"), false},
                    {QStringLiteral("color"), QStringLiteral("#5f7f52")}
                },
                QJsonObject{
                    {QStringLiteral("title"), QStringLiteral("School holiday")},
                    {QStringLiteral("eventType"), QStringLiteral("Holiday")},
                    {QStringLiteral("startDate"), QStringLiteral("2026-10-26")},
                    {QStringLiteral("endDate"), QStringLiteral("2026-10-26")},
                    {QStringLiteral("allDay"), true},
                    {QStringLiteral("color"), QStringLiteral("#7a5f9e")}
                }
            }}
        }},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), 1280},
            {QStringLiteral("height"), 920}
        }},
        {QStringLiteral("captures"), captures},
        {QStringLiteral("limitations"), QStringLiteral(
            "The two scroll positions are separate Legacy viewports. These images "
            "are visual references, not pixel-parity assertions or current-date query evidence.")}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("legacy-calendar-captures.json")));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray manifestBytes = QJsonDocument(manifest).toJson(
        QJsonDocument::Indented);
    QCOMPARE(manifestFile.write(manifestBytes), qint64(manifestBytes.size()));
}

void CalendarPageEventMutationParityTests::ordinaryCreateEditAndDeleteUseCalendarPageSignals_data()
{
    QTest::addColumn<qint64>("sequence");
    QTest::newRow("small-id") << qint64(1);
    QTest::newRow("above-int-max") << qint64(2147483647LL);
    QTest::newRow("above-javascript-exact-range") << qint64(9007199254740992LL);
    QTest::newRow("signed-64-max") << qint64(9223372036854775806LL);
}

void CalendarPageEventMutationParityTests::
ordinaryCreateEditAndDeleteUseCalendarPageSignals()
{
    QFETCH(qint64, sequence);
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );

    WorkspaceStore* const store = workspace.services.workspaceStore();
    QVERIFY(store);
    QVERIFY(store->isOpen());
    QSqlQuery sequenceQuery(store->database());
    sequenceQuery.prepare("UPDATE sqlite_sequence SET seq=? WHERE name='calendar_events'");
    sequenceQuery.addBindValue(sequence);
    QVERIFY(sequenceQuery.exec());
    CalendarPage page(&workspace.services);
    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);
    QVERIFY(
        root->metaObject()->indexOfSignal("dayActivated(int,int,int)") >= 0
        );
    QVERIFY(
        root->metaObject()->indexOfSignal("eventActivated(QString)") >= 0
        );

    QString interactionError;
    const EventDialogValues createdValues{
        QStringLiteral("F375 Created Workshop"),
        QDate(2026, 10, 20),
        QTime(9, 30),
        QDate(2026, 10, 20),
        QTime(11, 0),
        QStringLiteral("Workshop")
    };
    QVERIFY2(
        activateAndHandleDialog(
            page,
            [](QQuickItem* calendarRootObject)
            {
                return QMetaObject::invokeMethod(
                    calendarRootObject,
                    "dayActivated",
                    Qt::DirectConnection,
                    Q_ARG(int, 2026),
                    Q_ARG(int, 10),
                    Q_ARG(int, 20)
                    );
            },
            createdValues,
            false,
            &interactionError
            ),
        qPrintable(interactionError)
        );

    const PersistedEventRows afterCreate = persistedEventRows(workspace.services);
    QVERIFY2(afterCreate.succeeded, qPrintable(afterCreate.error));
    const qint64 createdEventId = eventIdForTitle(
        workspace.services,
        createdValues.title
        );
    QCOMPARE(createdEventId, sequence + 1);
    QCOMPARE(
        afterCreate.rows,
        QStringList({
            expectedEventRow(
                workspace.preservedEventId,
                QStringLiteral("F375 Preserved Ordinary"),
                QStringLiteral("Holiday"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("11:00"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("12:00")
                ),
            expectedEventRow(
                createdEventId,
                QStringLiteral("F375 Created Workshop"),
                QStringLiteral("Workshop"),
                QStringLiteral("2026-10-20"),
                QStringLiteral("09:30"),
                QStringLiteral("2026-10-20"),
                QStringLiteral("11:00")
                )
        })
        );

    const EventDialogValues updatedValues{
        QStringLiteral("F375 Updated Meeting"),
        QDate(2026, 10, 21),
        QTime(13, 15),
        QDate(2026, 10, 21),
        QTime(14, 45),
        QStringLiteral("Meeting")
    };
    interactionError.clear();
    QVERIFY2(
        activateAndHandleDialog(
            page,
            [createdEventId](QQuickItem* calendarRootObject)
            {
                return QMetaObject::invokeMethod(
                    calendarRootObject,
                    "eventActivated",
                    Qt::DirectConnection,
                    Q_ARG(QString, QString::number(createdEventId))
                    );
            },
            updatedValues,
            false,
            &interactionError
            ),
        qPrintable(interactionError)
        );

    const PersistedEventRows afterUpdate = persistedEventRows(workspace.services);
    QVERIFY2(afterUpdate.succeeded, qPrintable(afterUpdate.error));
    QCOMPARE(
        afterUpdate.rows,
        QStringList({
            expectedEventRow(
                workspace.preservedEventId,
                QStringLiteral("F375 Preserved Ordinary"),
                QStringLiteral("Holiday"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("11:00"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("12:00")
                ),
            expectedEventRow(
                createdEventId,
                QStringLiteral("F375 Updated Meeting"),
                QStringLiteral("Meeting"),
                QStringLiteral("2026-10-21"),
                QStringLiteral("13:15"),
                QStringLiteral("2026-10-21"),
                QStringLiteral("14:45")
                )
        })
        );

    interactionError.clear();
    QVERIFY2(
        activateAndHandleDialog(
            page,
            [createdEventId](QQuickItem* calendarRootObject)
            {
                return QMetaObject::invokeMethod(
                    calendarRootObject,
                    "eventActivated",
                    Qt::DirectConnection,
                    Q_ARG(QString, QString::number(createdEventId))
                    );
            },
            updatedValues,
            true,
            &interactionError
            ),
        qPrintable(interactionError)
        );

    const PersistedEventRows afterDelete = persistedEventRows(workspace.services);
    QVERIFY2(afterDelete.succeeded, qPrintable(afterDelete.error));
    QCOMPARE(
        afterDelete.rows,
        QStringList({
            expectedEventRow(
                workspace.preservedEventId,
                QStringLiteral("F375 Preserved Ordinary"),
                QStringLiteral("Holiday"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("11:00"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("12:00")
                )
        })
        );

    QJsonArray afterCreateTranscript;
    afterCreateTranscript.append(transcriptEvent(
        QStringLiteral("preserved"),
        QStringLiteral("F375 Preserved Ordinary"),
        QStringLiteral("Holiday"),
        QStringLiteral("2026-10-22"),
        QStringLiteral("11:00"),
        QStringLiteral("2026-10-22"),
        QStringLiteral("12:00")
        ));
    afterCreateTranscript.append(transcriptEvent(
        QStringLiteral("created"),
        QStringLiteral("F375 Created Workshop"),
        QStringLiteral("Workshop"),
        QStringLiteral("2026-10-20"),
        QStringLiteral("09:30"),
        QStringLiteral("2026-10-20"),
        QStringLiteral("11:00")
        ));

    QJsonArray afterUpdateTranscript;
    afterUpdateTranscript.append(afterCreateTranscript.at(0));
    afterUpdateTranscript.append(transcriptEvent(
        QStringLiteral("updated"),
        QStringLiteral("F375 Updated Meeting"),
        QStringLiteral("Meeting"),
        QStringLiteral("2026-10-21"),
        QStringLiteral("13:15"),
        QStringLiteral("2026-10-21"),
        QStringLiteral("14:45")
        ));

    QJsonArray afterDeleteTranscript;
    afterDeleteTranscript.append(afterCreateTranscript.at(0));

    emitTranscript({
        {QStringLiteral("after_create"), afterCreateTranscript},
        {QStringLiteral("after_update"), afterUpdateTranscript},
        {QStringLiteral("after_delete"), afterDeleteTranscript}
    });
}

void CalendarPageEventMutationParityTests::
staleEventActivationShowsWarningWithoutMutation()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate staleEventDate(2026, 10, 24);
    CalendarEvent staleEvent;
    staleEvent.title = QStringLiteral("F1282 Stale Activation");
    staleEvent.eventType = QStringLiteral("Workshop");
    staleEvent.timeStatus = QStringLiteral("Timed");
    staleEvent.startDate = staleEventDate;
    staleEvent.startTime = QTime(9, 30);
    staleEvent.endDate = staleEventDate;
    staleEvent.endTime = QTime(11, 0);
    const int staleEventId = saveFixtureEvent(
        workspace,
        staleEvent,
        &setupError
        );
    QVERIFY2(staleEventId > 0, qPrintable(setupError));

    CalendarPage page(&workspace.services);
    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);
    QVERIFY(
        root->metaObject()->indexOfSignal(
            "eventActivated(QString)"
            ) >= 0
        );
    QVERIFY(
        root->metaObject()->indexOfSignal(
            "displayedMonthChanged(int,int)"
            ) >= 0
        );
    QVERIFY(QMetaObject::invokeMethod(
        root,
        "displayedMonthChanged",
        Qt::DirectConnection,
        Q_ARG(int, staleEventDate.year()),
        Q_ARG(int, staleEventDate.month())
        ));

    CalendarEventModel* const model = calendarModel(page);
    QVERIFY(model);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(staleEventDate.year(), staleEventDate.month()),
        5000
        );
    const QVariantList projectedEvents = model->eventsForDate(
        staleEventDate.year(),
        staleEventDate.month(),
        staleEventDate.day()
        );
    QCOMPARE(projectedEvents.size(), 1);
    QCOMPARE(
        projectedEvents.first().toMap()
            .value(QStringLiteral("id")).toLongLong(),
        staleEventId
        );

    QSqlQuery removeStaleEvent(
        workspace.services.workspaceStore()->database()
        );
    removeStaleEvent.prepare(QStringLiteral(
        "DELETE FROM calendar_events WHERE id=?"
        ));
    removeStaleEvent.addBindValue(staleEventId);
    QVERIFY2(
        removeStaleEvent.exec(),
        qPrintable(removeStaleEvent.lastError().text())
        );
    QCOMPARE(removeStaleEvent.numRowsAffected(), 1);

    const PersistedEventRows beforeActivation = persistedEventRows(
        workspace.services
        );
    QVERIFY2(beforeActivation.succeeded, qPrintable(beforeActivation.error));
    QCOMPARE(
        beforeActivation.rows,
        QStringList({
            expectedEventRow(
                workspace.preservedEventId,
                QStringLiteral("F375 Preserved Ordinary"),
                QStringLiteral("Holiday"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("11:00"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("12:00")
                )
        })
        );

    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    bool warningIcon = false;
    bool editorRemainedClosed = false;
    bool persistedStateUnchangedWhileWarningWasShown = false;
    QString interactionError;
    QTimer warningTimer;
    warningTimer.setSingleShot(true);
    QObject::connect(
        &warningTimer,
        &QTimer::timeout,
        &page,
        [&]
        {
            QWidget* const activeModal = QApplication::activeModalWidget();
            auto* const warning = qobject_cast<QMessageBox*>(activeModal);
            if (!warning)
            {
                interactionError = activeModal
                    ? QStringLiteral(
                        "Stale event activation opened an unexpected modal dialog."
                        )
                    : QStringLiteral(
                        "Stale event activation did not show a warning."
                        );
                if (activeModal)
                {
                    activeModal->close();
                }
                return;
            }

            ++warningCount;
            warningTitle = warning->windowTitle();
            warningText = warning->text();
            warningIcon = warning->icon() == QMessageBox::Warning;
            editorRemainedClosed = page.findChildren<CalendarEventDialog*>()
                .isEmpty();
            const PersistedEventRows whileWarningIsShown = persistedEventRows(
                workspace.services
                );
            persistedStateUnchangedWhileWarningWasShown =
                whileWarningIsShown.succeeded
                && whileWarningIsShown.rows == beforeActivation.rows;
            warning->accept();
        }
        );
    warningTimer.start(0);
    QVERIFY(QMetaObject::invokeMethod(
        root,
        "eventActivated",
        Qt::DirectConnection,
        Q_ARG(QString, QString::number(staleEventId))
        ));
    QApplication::processEvents();

    QVERIFY2(interactionError.isEmpty(), qPrintable(interactionError));
    QCOMPARE(warningCount, 1);
    QCOMPARE(warningTitle, QStringLiteral("Load Calendar Event"));
    QCOMPARE(warningText, QStringLiteral("The calendar event was not found."));
    QVERIFY(warningIcon);
    QVERIFY(editorRemainedClosed);
    QVERIFY(persistedStateUnchangedWhileWarningWasShown);
    QVERIFY(!QApplication::activeModalWidget());
    QVERIFY(page.findChildren<CalendarEventDialog*>().isEmpty());

    const PersistedEventRows afterActivation = persistedEventRows(
        workspace.services
        );
    QVERIFY2(afterActivation.succeeded, qPrintable(afterActivation.error));
    QCOMPARE(afterActivation.rows, beforeActivation.rows);
}

void CalendarPageEventMutationParityTests::
ordinaryCreateAndEditKeepDialogOpenAfterSaveFailure_data()
{
    QTest::addColumn<bool>("editing");
    QTest::addColumn<bool>("cancelAfterFailure");
    QTest::newRow("create-retry") << false << false;
    QTest::newRow("edit-retry") << true << false;
    QTest::newRow("create-cancel-after-failure") << false << true;
}

void CalendarPageEventMutationParityTests::
ordinaryCreateAndEditKeepDialogOpenAfterSaveFailure()
{
    QFETCH(bool, editing);
    QFETCH(bool, cancelAfterFailure);

    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );

    const EventDialogValues values{
        editing
            ? QStringLiteral("F1078 Retried Edit")
            : QStringLiteral("F1078 Retried Create"),
        QDate(2026, 10, 24),
        QTime(9, 30),
        QDate(2026, 10, 24),
        QTime(11, 0),
        QStringLiteral("Workshop")
    };
    qint64 editedEventId = -1;
    if (editing)
    {
        CalendarEvent existing;
        existing.title = QStringLiteral("F1078 Original Edit");
        existing.eventType = QStringLiteral("Meeting");
        existing.timeStatus = QStringLiteral("Timed");
        existing.startDate = QDate(2026, 10, 19);
        existing.startTime = QTime(8, 0);
        existing.endDate = QDate(2026, 10, 19);
        existing.endTime = QTime(9, 0);
        editedEventId = saveFixtureEvent(workspace, existing, &setupError);
        QVERIFY2(editedEventId > 0, qPrintable(setupError));
    }

    const QString triggerName = editing
        ? QStringLiteral("fail_f1078_calendar_event_update")
        : QStringLiteral("fail_f1078_calendar_event_insert");
    const QString triggerSql = editing
        ? QStringLiteral(R"(
            CREATE TRIGGER fail_f1078_calendar_event_update
            BEFORE UPDATE ON calendar_events
            WHEN OLD.id = %1 AND NEW.title = 'F1078 Retried Edit'
            BEGIN
                SELECT RAISE(ABORT, 'forced F1078 calendar event save failure');
            END
        )").arg(editedEventId)
        : QStringLiteral(R"(
            CREATE TRIGGER fail_f1078_calendar_event_insert
            BEFORE INSERT ON calendar_events
            WHEN NEW.title = 'F1078 Retried Create'
            BEGIN
                SELECT RAISE(ABORT, 'forced F1078 calendar event save failure');
            END
        )");
    QString sqlError;
    QVERIFY2(
        executeWorkspaceSql(workspace, triggerSql, &sqlError),
        qPrintable(sqlError)
        );

    CalendarPage page(&workspace.services);
    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);

    int interactionStage = 0;
    QString interactionError;
    QPointer<CalendarEventDialog> originalDialog;
    int warningCount = 0;
    QString warningTitle;
    QString warningText;
    bool warningIcon = false;
    bool fieldsRetainedWhileWarningWasShown = false;
    bool failedSaveLeftRowsUnchanged = false;
    bool failureTriggerRemoved = false;
    bool sameDialogWasReopened = false;
    bool fieldsRetainedBeforeRetry = false;
    bool retrySubmitted = false;
    bool cancelSubmitted = false;

    QTimer interactionTimer;
    interactionTimer.setInterval(10);
    QObject::connect(
        &interactionTimer,
        &QTimer::timeout,
        &page,
        [&]
        {
            QWidget* const activeModal = QApplication::activeModalWidget();
            if (interactionStage == 0)
            {
                auto* const dialog = qobject_cast<CalendarEventDialog*>(
                    activeModal
                    );
                if (!dialog)
                {
                    return;
                }

                originalDialog = dialog;
                interactionStage = 1;
                if (!applyDialogValues(
                        dialog,
                        values,
                        false,
                        &interactionError
                        ))
                {
                    interactionStage = 4;
                    dialog->reject();
                }
                return;
            }

            if (interactionStage == 1)
            {
                auto* const warning = qobject_cast<QMessageBox*>(activeModal);
                if (!warning)
                {
                    if (!activeModal)
                    {
                        return;
                    }
                    interactionError = QStringLiteral(
                        "The failed calendar save did not show a warning."
                        );
                    interactionStage = 4;
                    activeModal->close();
                    return;
                }

                ++warningCount;
                warningTitle = warning->windowTitle();
                warningText = warning->text();
                warningIcon = warning->icon() == QMessageBox::Warning;
                fieldsRetainedWhileWarningWasShown =
                    dialogMatchesValues(originalDialog, values);
                const PersistedEventRows afterFailure = persistedEventRows(
                    workspace.services
                    );
                if (editing)
                {
                    failedSaveLeftRowsUnchanged =
                        afterFailure.succeeded
                        && afterFailure.rows == QStringList({
                            expectedEventRow(
                                workspace.preservedEventId,
                                QStringLiteral("F375 Preserved Ordinary"),
                                QStringLiteral("Holiday"),
                                QStringLiteral("2026-10-22"),
                                QStringLiteral("11:00"),
                                QStringLiteral("2026-10-22"),
                                QStringLiteral("12:00")
                                ),
                            expectedEventRow(
                                editedEventId,
                                QStringLiteral("F1078 Original Edit"),
                                QStringLiteral("Meeting"),
                                QStringLiteral("2026-10-19"),
                                QStringLiteral("08:00"),
                                QStringLiteral("2026-10-19"),
                                QStringLiteral("09:00")
                                )
                        });
                }
                else
                {
                    failedSaveLeftRowsUnchanged =
                        afterFailure.succeeded
                        && afterFailure.rows == QStringList({
                            expectedEventRow(
                                workspace.preservedEventId,
                                QStringLiteral("F375 Preserved Ordinary"),
                                QStringLiteral("Holiday"),
                                QStringLiteral("2026-10-22"),
                                QStringLiteral("11:00"),
                                QStringLiteral("2026-10-22"),
                                QStringLiteral("12:00")
                                )
                        });
                }

                failureTriggerRemoved = executeWorkspaceSql(
                    workspace,
                    QStringLiteral("DROP TRIGGER %1").arg(triggerName),
                    &sqlError
                    );
                if (!failureTriggerRemoved)
                {
                    interactionError = sqlError;
                    interactionStage = 4;
                }
                else
                {
                    interactionStage = 2;
                }
                warning->accept();
                return;
            }

            if (interactionStage == 2)
            {
                auto* const dialog = qobject_cast<CalendarEventDialog*>(
                    activeModal
                    );
                if (!dialog)
                {
                    if (!activeModal)
                    {
                        return;
                    }
                    interactionError = QStringLiteral(
                        "A different modal dialog appeared before calendar save retry."
                        );
                    interactionStage = 4;
                    activeModal->close();
                    return;
                }

                sameDialogWasReopened = dialog == originalDialog.data();
                fieldsRetainedBeforeRetry =
                    dialogMatchesValues(dialog, values);
                auto* const buttonBox = dialog->findChild<QDialogButtonBox*>();
                QPushButton* const cancel = buttonBox
                    ? buttonBox->button(QDialogButtonBox::Cancel)
                    : nullptr;
                QPushButton* const save = buttonBox
                    ? buttonBox->button(QDialogButtonBox::Save)
                    : nullptr;
                if (cancelAfterFailure && !cancel)
                {
                    interactionError = QStringLiteral(
                        "Calendar event cancel button was unavailable after failure."
                        );
                    interactionStage = 4;
                    dialog->reject();
                    return;
                }
                if (!cancelAfterFailure && !save)
                {
                    interactionError = QStringLiteral(
                        "Calendar event save button was unavailable on retry."
                        );
                    interactionStage = 4;
                    dialog->reject();
                    return;
                }

                interactionStage = 3;
                if (cancelAfterFailure)
                {
                    cancelSubmitted = true;
                    cancel->click();
                }
                else
                {
                    retrySubmitted = true;
                    save->click();
                }
                return;
            }

            if (interactionStage == 4)
            {
                interactionTimer.stop();
                if (activeModal)
                {
                    activeModal->close();
                }
            }
        }
        );

    QTimer failSafeTimer;
    failSafeTimer.setInterval(3000);
    QObject::connect(
        &failSafeTimer,
        &QTimer::timeout,
        &page,
        [&]
        {
            if (interactionStage != 3)
            {
                if (interactionError.isEmpty())
                {
                    interactionError = QStringLiteral(
                        "Calendar save failure retry interaction timed out."
                        );
                }
                if (QWidget* const activeModal = QApplication::activeModalWidget())
                {
                    activeModal->close();
                }
            }
        }
        );

    interactionTimer.start();
    failSafeTimer.start();
    const bool activated = editing
        ? QMetaObject::invokeMethod(
            root,
            "eventActivated",
            Qt::DirectConnection,
            Q_ARG(QString, QString::number(editedEventId))
            )
        : QMetaObject::invokeMethod(
            root,
            "dayActivated",
            Qt::DirectConnection,
            Q_ARG(int, 2026),
            Q_ARG(int, 10),
            Q_ARG(int, 24)
            );
    interactionTimer.stop();
    failSafeTimer.stop();

    QVERIFY2(activated, "Calendar page activation signal could not be invoked.");
    QVERIFY2(interactionError.isEmpty(), qPrintable(interactionError));
    QCOMPARE(interactionStage, 3);
    QCOMPARE(warningCount, 1);
    QCOMPARE(warningTitle, QStringLiteral("Save Calendar Event"));
    QVERIFY(warningIcon);
    QVERIFY(warningText.contains(
        QStringLiteral("forced F1078 calendar event save failure")
        ));
    QVERIFY(fieldsRetainedWhileWarningWasShown);
    QVERIFY(failedSaveLeftRowsUnchanged);
    QVERIFY(failureTriggerRemoved);
    QVERIFY(sameDialogWasReopened);
    QVERIFY(fieldsRetainedBeforeRetry);
    QCOMPARE(retrySubmitted, !cancelAfterFailure);
    QCOMPARE(cancelSubmitted, cancelAfterFailure);

    const PersistedEventRows afterRetry = persistedEventRows(workspace.services);
    QVERIFY2(afterRetry.succeeded, qPrintable(afterRetry.error));
    if (cancelAfterFailure)
    {
        QCOMPARE(
            eventIdForTitle(workspace.services, values.title),
            qint64(-1)
            );
        QCOMPARE(
            afterRetry.rows,
            QStringList({
                expectedEventRow(
                    workspace.preservedEventId,
                    QStringLiteral("F375 Preserved Ordinary"),
                    QStringLiteral("Holiday"),
                    QStringLiteral("2026-10-22"),
                    QStringLiteral("11:00"),
                    QStringLiteral("2026-10-22"),
                    QStringLiteral("12:00")
                    )
            })
            );
    }
    else if (editing)
    {
        QCOMPARE(
            afterRetry.rows,
            QStringList({
                expectedEventRow(
                    workspace.preservedEventId,
                    QStringLiteral("F375 Preserved Ordinary"),
                    QStringLiteral("Holiday"),
                    QStringLiteral("2026-10-22"),
                    QStringLiteral("11:00"),
                    QStringLiteral("2026-10-22"),
                    QStringLiteral("12:00")
                    ),
                expectedEventRow(
                    editedEventId,
                    values.title,
                    values.eventType,
                    QStringLiteral("2026-10-24"),
                    QStringLiteral("09:30"),
                    QStringLiteral("2026-10-24"),
                    QStringLiteral("11:00")
                    )
            })
            );
    }
    else
    {
        const qint64 createdEventId = eventIdForTitle(
            workspace.services,
            values.title
            );
        QVERIFY(createdEventId > workspace.preservedEventId);
        QCOMPARE(
            afterRetry.rows,
            QStringList({
                expectedEventRow(
                    workspace.preservedEventId,
                    QStringLiteral("F375 Preserved Ordinary"),
                    QStringLiteral("Holiday"),
                    QStringLiteral("2026-10-22"),
                    QStringLiteral("11:00"),
                    QStringLiteral("2026-10-22"),
                    QStringLiteral("12:00")
                    ),
                expectedEventRow(
                    createdEventId,
                    values.title,
                    values.eventType,
                    QStringLiteral("2026-10-24"),
                    QStringLiteral("09:30"),
                    QStringLiteral("2026-10-24"),
                    QStringLiteral("11:00")
                    )
            })
            );
    }
}

void CalendarPageEventMutationParityTests::
repeatSeriesCreateKeepsDialogOpenAfterSaveFailure_data()
{
    QTest::addColumn<bool>("cancelAfterFailure");
    QTest::newRow("retry") << false;
    QTest::newRow("cancel-after-failure") << true;
}

void CalendarPageEventMutationParityTests::
repeatSeriesCreateKeepsDialogOpenAfterSaveFailure()
{
    QFETCH(bool, cancelAfterFailure);

    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate firstOfMonth(
        QDate::currentDate().year(),
        QDate::currentDate().month(),
        1
        );
    const QDate repeatUntilDate = firstOfMonth.addDays(21);
    const RepeatSeriesCreateDialogValues values{
        {
            QStringLiteral("F1083 Repeat Create"),
            firstOfMonth,
            QTime(9, 30),
            firstOfMonth,
            QTime(11, 0),
            QStringLiteral("Workshop")
        },
        CalendarEventRepeatFrequency::Weekly,
        repeatUntilDate
    };
    const QStringList beforeRows{
        expectedEventRow(
            workspace.preservedEventId,
            QStringLiteral("F375 Preserved Ordinary"),
            QStringLiteral("Holiday"),
            QStringLiteral("2026-10-22"),
            QStringLiteral("11:00"),
            QStringLiteral("2026-10-22"),
            QStringLiteral("12:00")
            )
    };
    const PersistedEventRows beforeCreate = persistedEventRows(
        workspace.services
        );
    QVERIFY2(beforeCreate.succeeded, qPrintable(beforeCreate.error));
    QCOMPARE(beforeCreate.rows, beforeRows);

    const QString triggerName = QStringLiteral(
        "fail_f1083_repeat_series_create"
        );
    const QString triggerSql = QStringLiteral(R"(
        CREATE TRIGGER fail_f1083_repeat_series_create
        BEFORE INSERT ON calendar_events
        WHEN NEW.title = 'F1083 Repeat Create'
            AND NEW.start_date = '%1'
        BEGIN
            SELECT RAISE(ABORT, 'forced F1083 repeat-series create failure');
        END
    )").arg(firstOfMonth.addDays(7).toString(Qt::ISODate));
    QString sqlError;
    QVERIFY2(
        executeWorkspaceSql(workspace, triggerSql, &sqlError),
        qPrintable(sqlError)
        );

    CalendarPage page(&workspace.services);
    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);
    QVERIFY(
        root->metaObject()->indexOfSignal("dayActivated(int,int,int)") >= 0
        );
    QVERIFY(
        root->metaObject()->indexOfSignal("displayedMonthChanged(int,int)") >= 0
        );
    QVERIFY(QMetaObject::invokeMethod(
        root,
        "displayedMonthChanged",
        Qt::DirectConnection,
        Q_ARG(int, firstOfMonth.year()),
        Q_ARG(int, firstOfMonth.month())
        ));
    CalendarEventModel* const model = calendarModel(page);
    QVERIFY(model);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month()),
        5000
        );
    const int modelRevisionBeforeCreate = model->revision();

    const RepeatSeriesCreateFailureInteraction interaction =
        interactWithRepeatSeriesCreateFailure(
            workspace,
            page,
            values.event.startDate,
            values,
            beforeRows,
            triggerName,
            cancelAfterFailure
            );

    QVERIFY2(interaction.activated, "Calendar day signal could not be invoked.");
    QVERIFY2(interaction.error.isEmpty(), qPrintable(interaction.error));
    QCOMPARE(interaction.stage, 3);
    QCOMPARE(interaction.warningCount, 1);
    QCOMPARE(interaction.warningTitle, QStringLiteral("Save Calendar Event"));
    QVERIFY(interaction.warningIcon);
    QVERIFY(interaction.warningText.contains(
        QStringLiteral("forced F1083 repeat-series create failure")
        ));
    QVERIFY(interaction.fieldsRetainedWhileWarningWasShown);
    QVERIFY(interaction.failedCreateLeftRowsUnchanged);
    QVERIFY(interaction.failureTriggerRemoved);
    QVERIFY(interaction.sameDialogWasReopened);
    QVERIFY(interaction.fieldsRetainedBeforeRetry);
    QCOMPARE(interaction.retrySubmitted, !cancelAfterFailure);
    QCOMPARE(interaction.cancelSubmitted, cancelAfterFailure);
    QVERIFY(!QApplication::activeModalWidget());

    const PersistedEventRows afterAction = persistedEventRows(workspace.services);
    QVERIFY2(afterAction.succeeded, qPrintable(afterAction.error));
    if (cancelAfterFailure)
    {
        QCOMPARE(afterAction.rows, beforeRows);
        QCOMPARE(model->revision(), modelRevisionBeforeCreate);
        return;
    }

    QCOMPARE(afterAction.rows.size(), beforeRows.size() + 4);
    QString repeatSeriesId;
    for (int index = 0; index < 4; ++index)
    {
        const QJsonDocument rowDocument = QJsonDocument::fromJson(
            afterAction.rows.at(beforeRows.size() + index).toUtf8()
            );
        QVERIFY(rowDocument.isArray());
        const QJsonArray row = rowDocument.array();
        QCOMPARE(row.size(), 10);
        QVERIFY(row.at(0).toVariant().toLongLong() > workspace.preservedEventId);
        QCOMPARE(row.at(1).toString(), values.event.title);
        QCOMPARE(row.at(2).toString(), values.event.eventType);
        QCOMPARE(row.at(3).toString(), QStringLiteral("Timed"));
        if (index == 0)
        {
            repeatSeriesId = row.at(4).toString();
            QVERIFY(!repeatSeriesId.isEmpty());
        }
        else
        {
            QCOMPARE(row.at(4).toString(), repeatSeriesId);
        }
        QCOMPARE(row.at(5).toInt(), 0);
        const QString expectedDate = firstOfMonth
            .addDays(index * 7)
            .toString(Qt::ISODate);
        QCOMPARE(row.at(6).toString(), expectedDate);
        QCOMPARE(row.at(7).toString(), QStringLiteral("09:30"));
        QCOMPARE(row.at(8).toString(), expectedDate);
        QCOMPARE(row.at(9).toString(), QStringLiteral("11:00"));
    }

    QTRY_VERIFY_WITH_TIMEOUT(
        model->revision() > modelRevisionBeforeCreate,
        5000
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month()),
        5000
        );
    for (int index = 0; index < 4; ++index)
    {
        const QDate occurrenceDate = firstOfMonth.addDays(index * 7);
        const QVariantList projection = model->eventsForDate(
            occurrenceDate.year(),
            occurrenceDate.month(),
            occurrenceDate.day()
            );
        const bool occurrenceWasProjected = std::any_of(
            projection.cbegin(),
            projection.cend(),
            [&values](const QVariant& value)
            {
                return value.toMap()
                    .value(QStringLiteral("title")).toString()
                    == values.event.title;
            }
            );
        QVERIFY(occurrenceWasProjected);
    }
}

void CalendarPageEventMutationParityTests::
repeatSeriesOccurrenceEditKeepsDialogOpenAfterSaveFailure_data()
{
    QTest::addColumn<bool>("cancelAfterFailure");
    QTest::newRow("retry") << false;
    QTest::newRow("cancel-after-failure") << true;
}

void CalendarPageEventMutationParityTests::
repeatSeriesOccurrenceEditKeepsDialogOpenAfterSaveFailure()
{
    QFETCH(bool, cancelAfterFailure);

    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate today = QDate::currentDate();
    const QDate firstOfMonth(today.year(), today.month(), 1);
    const QDate earlierDate = firstOfMonth;
    const QDate selectedDate = firstOfMonth.addDays(7);
    const QDate followingDate = firstOfMonth.addDays(14);
    const QString seriesId = QStringLiteral("F1081-fixed-repeat-series");
    const auto dateText = [](const QDate& date)
    {
        return date.toString(Qt::ISODate);
    };

    const auto saveOccurrence =
        [&workspace, &setupError, &seriesId](
            const QString& title,
            const QDate& date
            ) -> qint64
        {
            CalendarEvent event;
            event.title = title;
            event.eventType = QStringLiteral("Meeting");
            event.timeStatus = QStringLiteral("Timed");
            event.repeatSeriesId = seriesId;
            event.startDate = date;
            event.startTime = QTime(9, 0);
            event.endDate = date;
            event.endTime = QTime(10, 0);

            const auto saved = workspace.services.databaseSession()
                ->calendarEventRepository()
                ->saveCalendarEvent(event);
            if (!saved)
            {
                setupError = toLegacyDiagnostic(saved.error());
                return -1;
            }

            return *saved;
        };

    const qint64 earlierId = saveOccurrence(
        QStringLiteral("F1081 Earlier Occurrence"),
        earlierDate
        );
    const qint64 selectedId = saveOccurrence(
        QStringLiteral("F1081 Selected Occurrence"),
        selectedDate
        );
    const qint64 followingId = saveOccurrence(
        QStringLiteral("F1081 Following Occurrence"),
        followingDate
        );
    QVERIFY2(earlierId > 0, qPrintable(setupError));
    QVERIFY2(selectedId > 0, qPrintable(setupError));
    QVERIFY2(followingId > 0, qPrintable(setupError));
    QCOMPARE(workspace.preservedEventId, 1);
    QCOMPARE(earlierId, 2);
    QCOMPARE(selectedId, 3);
    QCOMPARE(followingId, 4);

    const PersistedEventRows beforeEdit = persistedEventRows(
        workspace.services
        );
    QVERIFY2(beforeEdit.succeeded, qPrintable(beforeEdit.error));
    QCOMPARE(
        beforeEdit.rows,
        QStringList({
            expectedEventRow(
                workspace.preservedEventId,
                QStringLiteral("F375 Preserved Ordinary"),
                QStringLiteral("Holiday"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("11:00"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("12:00")
                ),
            expectedEventRow(
                earlierId,
                QStringLiteral("F1081 Earlier Occurrence"),
                QStringLiteral("Meeting"),
                dateText(earlierDate),
                QStringLiteral("09:00"),
                dateText(earlierDate),
                QStringLiteral("10:00"),
                seriesId
                ),
            expectedEventRow(
                selectedId,
                QStringLiteral("F1081 Selected Occurrence"),
                QStringLiteral("Meeting"),
                dateText(selectedDate),
                QStringLiteral("09:00"),
                dateText(selectedDate),
                QStringLiteral("10:00"),
                seriesId
                ),
            expectedEventRow(
                followingId,
                QStringLiteral("F1081 Following Occurrence"),
                QStringLiteral("Meeting"),
                dateText(followingDate),
                QStringLiteral("09:00"),
                dateText(followingDate),
                QStringLiteral("10:00"),
                seriesId
                )
        })
        );

    const EventDialogValues values{
        QStringLiteral("F1081 Retried Selected Occurrence"),
        selectedDate.addDays(1),
        QTime(10, 15),
        selectedDate.addDays(1),
        QTime(11, 45),
        QStringLiteral("Workshop")
    };
    const QString triggerName = QStringLiteral(
        "fail_f1081_repeat_occurrence_update"
        );
    const QString triggerSql = QStringLiteral(R"(
        CREATE TRIGGER fail_f1081_repeat_occurrence_update
        BEFORE UPDATE ON calendar_events
        WHEN OLD.id = %1 AND NEW.title = 'F1081 Retried Selected Occurrence'
        BEGIN
            SELECT RAISE(ABORT, 'forced F1081 repeat occurrence save failure');
        END
    )").arg(selectedId);
    QString sqlError;
    QVERIFY2(
        executeWorkspaceSql(workspace, triggerSql, &sqlError),
        qPrintable(sqlError)
        );

    CalendarPage page(&workspace.services);
    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);
    QVERIFY(
        root->metaObject()->indexOfSignal("eventActivated(QString)") >= 0
        );
    QVERIFY(
        root->metaObject()->indexOfSignal(
            "displayedMonthChanged(int,int)"
            ) >= 0
        );
    QVERIFY(QMetaObject::invokeMethod(
        root,
        "displayedMonthChanged",
        Qt::DirectConnection,
        Q_ARG(int, firstOfMonth.year()),
        Q_ARG(int, firstOfMonth.month())
        ));
    CalendarEventModel* const model = calendarModel(page);
    QVERIFY(model);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month()),
        5000
        );
    const int modelRevisionBeforeEdit = model->revision();

    const SeriesEditFailureInteraction interaction = interactWithSeriesEditFailure(
        workspace,
        page,
        selectedId,
        values,
        beforeEdit.rows,
        triggerName,
        CalendarEventSeriesEditScope::ThisEventOnly,
        cancelAfterFailure
        );

    QVERIFY2(interaction.activated, "Calendar page occurrence signal could not be invoked.");
    QVERIFY2(interaction.error.isEmpty(), qPrintable(interaction.error));
    QCOMPARE(interaction.stage, 3);
    QCOMPARE(interaction.warningCount, 1);
    QCOMPARE(interaction.warningTitle, QStringLiteral("Save Calendar Event"));
    QVERIFY(interaction.warningIcon);
    QVERIFY(interaction.warningText.contains(
        QStringLiteral("forced F1081 repeat occurrence save failure")
        ));
    QVERIFY(interaction.fieldsRetainedWhileWarningWasShown);
    QVERIFY(interaction.failedSaveLeftRowsUnchanged);
    QVERIFY(interaction.failureTriggerRemoved);
    QVERIFY(interaction.sameDialogWasReopened);
    QVERIFY(interaction.fieldsRetainedBeforeRetry);
    QVERIFY(interaction.eventScopeRetained);
    QCOMPARE(interaction.retrySubmitted, !cancelAfterFailure);
    QCOMPARE(interaction.cancelSubmitted, cancelAfterFailure);

    const PersistedEventRows afterAction = persistedEventRows(workspace.services);
    QVERIFY2(afterAction.succeeded, qPrintable(afterAction.error));
    if (cancelAfterFailure)
    {
        QCOMPARE(afterAction.rows, beforeEdit.rows);
        QCOMPARE(model->revision(), modelRevisionBeforeEdit);
    }
    else
    {
        QCOMPARE(
            afterAction.rows,
            QStringList({
                expectedEventRow(
                    workspace.preservedEventId,
                    QStringLiteral("F375 Preserved Ordinary"),
                    QStringLiteral("Holiday"),
                    QStringLiteral("2026-10-22"),
                    QStringLiteral("11:00"),
                    QStringLiteral("2026-10-22"),
                    QStringLiteral("12:00")
                    ),
                expectedEventRow(
                    earlierId,
                    QStringLiteral("F1081 Earlier Occurrence"),
                    QStringLiteral("Meeting"),
                    dateText(earlierDate),
                    QStringLiteral("09:00"),
                    dateText(earlierDate),
                    QStringLiteral("10:00"),
                    seriesId
                    ),
                expectedEventRow(
                    selectedId,
                    values.title,
                    values.eventType,
                    dateText(values.startDate),
                    QStringLiteral("10:15"),
                    dateText(values.endDate),
                    QStringLiteral("11:45")
                    ),
                expectedEventRow(
                    followingId,
                    QStringLiteral("F1081 Following Occurrence"),
                    QStringLiteral("Meeting"),
                    dateText(followingDate),
                    QStringLiteral("09:00"),
                    dateText(followingDate),
                    QStringLiteral("10:00"),
                    seriesId
                    )
            })
            );
        QTRY_VERIFY_WITH_TIMEOUT(
            model->revision() > modelRevisionBeforeEdit,
            5000
            );
        QTRY_VERIFY_WITH_TIMEOUT(
            model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month()),
            5000
            );
        const QVariantList selectedProjection = model->eventsForDate(
            values.startDate.year(),
            values.startDate.month(),
            values.startDate.day()
            );
        QCOMPARE(selectedProjection.size(), 1);
        QCOMPARE(
            selectedProjection.first().toMap()
                .value(QStringLiteral("id")).toLongLong(),
            selectedId
            );
        QCOMPARE(
            selectedProjection.first().toMap()
                .value(QStringLiteral("title")).toString(),
            values.title
            );
        const QVariantList earlierProjection = model->eventsForDate(
            earlierDate.year(),
            earlierDate.month(),
            earlierDate.day()
            );
        const QVariantList followingProjection = model->eventsForDate(
            followingDate.year(),
            followingDate.month(),
            followingDate.day()
            );
        QCOMPARE(earlierProjection.size(), 1);
        QCOMPARE(followingProjection.size(), 1);
        QCOMPARE(
            earlierProjection.first().toMap()
                .value(QStringLiteral("id")).toLongLong(),
            earlierId
            );
        QCOMPARE(
            earlierProjection.first().toMap()
                .value(QStringLiteral("title")).toString(),
            QStringLiteral("F1081 Earlier Occurrence")
            );
        QCOMPARE(
            followingProjection.first().toMap()
                .value(QStringLiteral("id")).toLongLong(),
            followingId
            );
        QCOMPARE(
            followingProjection.first().toMap()
                .value(QStringLiteral("title")).toString(),
            QStringLiteral("F1081 Following Occurrence")
            );
    }
}

void CalendarPageEventMutationParityTests::
repeatSeriesSuffixEditKeepsDialogOpenAfterSaveFailure_data()
{
    QTest::addColumn<bool>("cancelAfterFailure");
    QTest::newRow("retry") << false;
    QTest::newRow("cancel-after-failure") << true;
}

void CalendarPageEventMutationParityTests::
repeatSeriesSuffixEditKeepsDialogOpenAfterSaveFailure()
{
    QFETCH(bool, cancelAfterFailure);

    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate today = QDate::currentDate();
    const QDate firstOfMonth(today.year(), today.month(), 1);
    const QDate earlierDate = firstOfMonth;
    const QDate selectedDate = firstOfMonth.addDays(7);
    const QDate followingDate = firstOfMonth.addDays(14);
    const QString seriesId = QStringLiteral("F1082-fixed-repeat-series");
    const auto dateText = [](const QDate& date)
    {
        return date.toString(Qt::ISODate);
    };

    const auto saveOccurrence =
        [&workspace, &setupError, &seriesId](
            const QString& title,
            const QDate& date
            ) -> qint64
        {
            CalendarEvent event;
            event.title = title;
            event.eventType = QStringLiteral("Meeting");
            event.timeStatus = QStringLiteral("Timed");
            event.repeatSeriesId = seriesId;
            event.startDate = date;
            event.startTime = QTime(9, 0);
            event.endDate = date;
            event.endTime = QTime(10, 0);

            const auto saved = workspace.services.workspaceStore()
                ->calendarEventRepository()
                ->saveCalendarEvent(event);
            if (!saved)
            {
                setupError = toLegacyDiagnostic(saved.error());
                return -1;
            }

            return *saved;
        };

    const qint64 earlierId = saveOccurrence(
        QStringLiteral("F1082 Earlier Occurrence"),
        earlierDate
        );
    const qint64 selectedId = saveOccurrence(
        QStringLiteral("F1082 Selected Occurrence"),
        selectedDate
        );
    const qint64 followingId = saveOccurrence(
        QStringLiteral("F1082 Following Occurrence"),
        followingDate
        );
    QVERIFY2(earlierId > 0, qPrintable(setupError));
    QVERIFY2(selectedId > 0, qPrintable(setupError));
    QVERIFY2(followingId > 0, qPrintable(setupError));
    QCOMPARE(workspace.preservedEventId, 1);
    QCOMPARE(earlierId, 2);
    QCOMPARE(selectedId, 3);
    QCOMPARE(followingId, 4);

    const QStringList beforeRows{
        expectedEventRow(
            workspace.preservedEventId,
            QStringLiteral("F375 Preserved Ordinary"),
            QStringLiteral("Holiday"),
            QStringLiteral("2026-10-22"),
            QStringLiteral("11:00"),
            QStringLiteral("2026-10-22"),
            QStringLiteral("12:00")
            ),
        expectedEventRow(
            earlierId,
            QStringLiteral("F1082 Earlier Occurrence"),
            QStringLiteral("Meeting"),
            dateText(earlierDate),
            QStringLiteral("09:00"),
            dateText(earlierDate),
            QStringLiteral("10:00"),
            seriesId
            ),
        expectedEventRow(
            selectedId,
            QStringLiteral("F1082 Selected Occurrence"),
            QStringLiteral("Meeting"),
            dateText(selectedDate),
            QStringLiteral("09:00"),
            dateText(selectedDate),
            QStringLiteral("10:00"),
            seriesId
            ),
        expectedEventRow(
            followingId,
            QStringLiteral("F1082 Following Occurrence"),
            QStringLiteral("Meeting"),
            dateText(followingDate),
            QStringLiteral("09:00"),
            dateText(followingDate),
            QStringLiteral("10:00"),
            seriesId
            )
    };
    const PersistedEventRows beforeEdit = persistedEventRows(
        workspace.services
        );
    QVERIFY2(beforeEdit.succeeded, qPrintable(beforeEdit.error));
    QCOMPARE(beforeEdit.rows, beforeRows);

    const EventDialogValues values{
        QStringLiteral("F1082 Retried Suffix"),
        selectedDate.addDays(1),
        QTime(10, 15),
        selectedDate.addDays(1),
        QTime(11, 45),
        QStringLiteral("Workshop")
    };
    const QString triggerName = QStringLiteral(
        "fail_f1082_repeat_series_suffix_update"
        );
    const QString triggerSql = QStringLiteral(R"(
        CREATE TRIGGER fail_f1082_repeat_series_suffix_update
        BEFORE UPDATE ON calendar_events
        WHEN OLD.id = %1 AND NEW.title = 'F1082 Retried Suffix'
        BEGIN
            SELECT RAISE(ABORT, 'forced F1082 repeat-series suffix save failure');
        END
    )").arg(followingId);
    QString sqlError;
    QVERIFY2(
        executeWorkspaceSql(workspace, triggerSql, &sqlError),
        qPrintable(sqlError)
        );

    CalendarPage page(&workspace.services);
    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);
    QVERIFY(
        root->metaObject()->indexOfSignal("eventActivated(QString)") >= 0
        );
    QVERIFY(
        root->metaObject()->indexOfSignal(
            "displayedMonthChanged(int,int)"
            ) >= 0
        );
    QVERIFY(QMetaObject::invokeMethod(
        root,
        "displayedMonthChanged",
        Qt::DirectConnection,
        Q_ARG(int, firstOfMonth.year()),
        Q_ARG(int, firstOfMonth.month())
        ));
    CalendarEventModel* const model = calendarModel(page);
    QVERIFY(model);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month()),
        5000
        );
    const int modelRevisionBeforeEdit = model->revision();

    const SeriesEditFailureInteraction interaction = interactWithSeriesEditFailure(
        workspace,
        page,
        selectedId,
        values,
        beforeRows,
        triggerName,
        CalendarEventSeriesEditScope::ThisAndFollowingEvents,
        cancelAfterFailure
        );

    QVERIFY2(interaction.activated, "Calendar page suffix signal could not be invoked.");
    QVERIFY2(interaction.error.isEmpty(), qPrintable(interaction.error));
    QCOMPARE(interaction.stage, 3);
    QCOMPARE(interaction.warningCount, 1);
    QCOMPARE(interaction.warningTitle, QStringLiteral("Save Calendar Event"));
    QVERIFY(interaction.warningIcon);
    QVERIFY(interaction.warningText.contains(
        QStringLiteral("forced F1082 repeat-series suffix save failure")
        ));
    QVERIFY(interaction.fieldsRetainedWhileWarningWasShown);
    QVERIFY(interaction.failedSaveLeftRowsUnchanged);
    QVERIFY(interaction.failureTriggerRemoved);
    QVERIFY(interaction.sameDialogWasReopened);
    QVERIFY(interaction.fieldsRetainedBeforeRetry);
    QVERIFY(interaction.eventScopeRetained);
    QCOMPARE(interaction.retrySubmitted, !cancelAfterFailure);
    QCOMPARE(interaction.cancelSubmitted, cancelAfterFailure);

    const PersistedEventRows afterAction = persistedEventRows(workspace.services);
    QVERIFY2(afterAction.succeeded, qPrintable(afterAction.error));
    if (cancelAfterFailure)
    {
        QCOMPARE(afterAction.rows, beforeRows);
        QCOMPARE(model->revision(), modelRevisionBeforeEdit);
        return;
    }

    const QDate shiftedFollowingDate = followingDate.addDays(1);
    QCOMPARE(
        afterAction.rows,
        QStringList({
            beforeRows.at(0),
            beforeRows.at(1),
            expectedEventRow(
                selectedId,
                values.title,
                values.eventType,
                dateText(values.startDate),
                QStringLiteral("10:15"),
                dateText(values.endDate),
                QStringLiteral("11:45"),
                seriesId
                ),
            expectedEventRow(
                followingId,
                values.title,
                values.eventType,
                dateText(shiftedFollowingDate),
                QStringLiteral("10:15"),
                dateText(shiftedFollowingDate),
                QStringLiteral("11:45"),
                seriesId
                )
        })
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        model->revision() > modelRevisionBeforeEdit,
        5000
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month()),
        5000
        );

    const auto eventsAtDate = [model](const QDate& date)
    {
        return model->eventsForDate(date.year(), date.month(), date.day());
    };
    const QVariantList earlierProjection = eventsAtDate(earlierDate);
    const QVariantList oldSelectedProjection = eventsAtDate(selectedDate);
    const QVariantList selectedProjection = eventsAtDate(values.startDate);
    const QVariantList oldFollowingProjection = eventsAtDate(followingDate);
    const QVariantList followingProjection = eventsAtDate(shiftedFollowingDate);
    QCOMPARE(earlierProjection.size(), 1);
    QCOMPARE(
        earlierProjection.first().toMap()
            .value(QStringLiteral("id")).toLongLong(),
        earlierId
        );
    QCOMPARE(
        earlierProjection.first().toMap()
            .value(QStringLiteral("title")).toString(),
        QStringLiteral("F1082 Earlier Occurrence")
        );
    QVERIFY(oldSelectedProjection.isEmpty());
    QVERIFY(oldFollowingProjection.isEmpty());
    QCOMPARE(selectedProjection.size(), 1);
    QCOMPARE(followingProjection.size(), 1);
    QCOMPARE(
        selectedProjection.first().toMap()
            .value(QStringLiteral("id")).toLongLong(),
        selectedId
        );
    QCOMPARE(
        selectedProjection.first().toMap()
            .value(QStringLiteral("title")).toString(),
        values.title
        );
    QCOMPARE(
        followingProjection.first().toMap()
            .value(QStringLiteral("id")).toLongLong(),
        followingId
        );
    QCOMPARE(
        followingProjection.first().toMap()
            .value(QStringLiteral("title")).toString(),
        values.title
        );
}

void CalendarPageEventMutationParityTests::
repeatSeriesSuffixEditUsesCalendarPageSignalAndRefreshesProjection()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate today = QDate::currentDate();
    const QDate firstOfMonth(today.year(), today.month(), 1);
    const QDate earlierDate = firstOfMonth;
    const QDate selectedDate = firstOfMonth.addDays(7);
    const QDate followingDate = firstOfMonth.addDays(14);
    const QString seriesId = QStringLiteral("F379-fixed-repeat-series");

    const auto saveOccurrence =
        [&workspace, &setupError, &seriesId](
            const QString& title,
            const QDate& date
            ) -> qint64
        {
            CalendarEvent event;
            event.title = title;
            event.eventType = QStringLiteral("Meeting");
            event.timeStatus = QStringLiteral("Timed");
            event.repeatSeriesId = seriesId;
            event.startDate = date;
            event.startTime = QTime(9, 0);
            event.endDate = date;
            event.endTime = QTime(10, 0);

            const auto saved = workspace.services.workspaceStore()
                ->calendarEventRepository()
                ->saveCalendarEvent(event);
            if (!saved)
            {
                setupError = toLegacyDiagnostic(saved.error());
                return -1;
            }

            return *saved;
        };

    const int earlierId = saveOccurrence(
        QStringLiteral("F379 Earlier Occurrence"),
        earlierDate
        );
    const int selectedId = saveOccurrence(
        QStringLiteral("F379 Selected Occurrence"),
        selectedDate
        );
    const int followingId = saveOccurrence(
        QStringLiteral("F379 Following Occurrence"),
        followingDate
        );
    QVERIFY2(earlierId > 0, qPrintable(setupError));
    QVERIFY2(selectedId > 0, qPrintable(setupError));
    QVERIFY2(followingId > 0, qPrintable(setupError));
    QCOMPARE(workspace.preservedEventId, 1);
    QCOMPARE(earlierId, 2);
    QCOMPARE(selectedId, 3);
    QCOMPARE(followingId, 4);

    const auto dateText = [](const QDate& date)
    {
        return date.toString(Qt::ISODate);
    };
    const PersistedEventRows beforeEdit = persistedEventRows(
        workspace.services
        );
    QVERIFY2(beforeEdit.succeeded, qPrintable(beforeEdit.error));
    QCOMPARE(
        beforeEdit.rows,
        QStringList({
            expectedEventRow(
                workspace.preservedEventId,
                QStringLiteral("F375 Preserved Ordinary"),
                QStringLiteral("Holiday"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("11:00"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("12:00")
                ),
            expectedEventRow(
                earlierId,
                QStringLiteral("F379 Earlier Occurrence"),
                QStringLiteral("Meeting"),
                dateText(earlierDate),
                QStringLiteral("09:00"),
                dateText(earlierDate),
                QStringLiteral("10:00"),
                seriesId
                ),
            expectedEventRow(
                selectedId,
                QStringLiteral("F379 Selected Occurrence"),
                QStringLiteral("Meeting"),
                dateText(selectedDate),
                QStringLiteral("09:00"),
                dateText(selectedDate),
                QStringLiteral("10:00"),
                seriesId
                ),
            expectedEventRow(
                followingId,
                QStringLiteral("F379 Following Occurrence"),
                QStringLiteral("Meeting"),
                dateText(followingDate),
                QStringLiteral("09:00"),
                dateText(followingDate),
                QStringLiteral("10:00"),
                seriesId
                )
        })
        );

    CalendarPage page(&workspace.services);
    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);
    QVERIFY(
        root->metaObject()->indexOfSignal("eventActivated(QString)") >= 0
        );
    QVERIFY(
        root->metaObject()->indexOfSignal(
            "displayedMonthChanged(int,int)"
            ) >= 0
        );
    QVERIFY(QMetaObject::invokeMethod(
        root,
        "displayedMonthChanged",
        Qt::DirectConnection,
        Q_ARG(int, firstOfMonth.year()),
        Q_ARG(int, firstOfMonth.month())
        ));

    CalendarEventModel* const model = calendarModel(page);
    QVERIFY(model);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month()),
        5000
        );
    QCOMPARE(
        model->eventsForDate(
            earlierDate.year(),
            earlierDate.month(),
            earlierDate.day()
            ).size(),
        1
        );

    const QDate editedDate = selectedDate.addDays(1);
    const QDate shiftedFollowingDate = followingDate.addDays(1);
    const EventDialogValues editedValues{
        QStringLiteral("F379 Revised Workshop"),
        editedDate,
        QTime(10, 15),
        editedDate,
        QTime(11, 45),
        QStringLiteral("Workshop")
    };
    const int modelRevisionBeforeEdit = model->revision();
    QString interactionError;
    QVERIFY2(
        activateAndHandleDialog(
            page,
            [selectedId](QQuickItem* calendarRootObject)
            {
                return QMetaObject::invokeMethod(
                    calendarRootObject,
                    "eventActivated",
                    Qt::DirectConnection,
                    Q_ARG(QString, QString::number(selectedId))
                    );
            },
            editedValues,
            false,
            &interactionError,
            true
            ),
        qPrintable(interactionError)
        );

    QTRY_VERIFY_WITH_TIMEOUT(
        model->revision() > modelRevisionBeforeEdit,
        5000
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month()),
        5000
        );

    const PersistedEventRows afterEdit = persistedEventRows(
        workspace.services
        );
    QVERIFY2(afterEdit.succeeded, qPrintable(afterEdit.error));
    QCOMPARE(
        afterEdit.rows,
        QStringList({
            expectedEventRow(
                workspace.preservedEventId,
                QStringLiteral("F375 Preserved Ordinary"),
                QStringLiteral("Holiday"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("11:00"),
                QStringLiteral("2026-10-22"),
                QStringLiteral("12:00")
                ),
            expectedEventRow(
                earlierId,
                QStringLiteral("F379 Earlier Occurrence"),
                QStringLiteral("Meeting"),
                dateText(earlierDate),
                QStringLiteral("09:00"),
                dateText(earlierDate),
                QStringLiteral("10:00"),
                seriesId
                ),
            expectedEventRow(
                selectedId,
                QStringLiteral("F379 Revised Workshop"),
                QStringLiteral("Workshop"),
                dateText(editedDate),
                QStringLiteral("10:15"),
                dateText(editedDate),
                QStringLiteral("11:45"),
                seriesId
                ),
            expectedEventRow(
                followingId,
                QStringLiteral("F379 Revised Workshop"),
                QStringLiteral("Workshop"),
                dateText(shiftedFollowingDate),
                QStringLiteral("10:15"),
                dateText(shiftedFollowingDate),
                QStringLiteral("11:45"),
                seriesId
                )
        })
        );

    const auto eventAtDate = [model](const QDate& date)
    {
        return model->eventsForDate(
            date.year(),
            date.month(),
            date.day()
            );
    };
    const QVariantList earlierProjection = eventAtDate(earlierDate);
    const QVariantList selectedOldDateProjection = eventAtDate(selectedDate);
    const QVariantList selectedNewDateProjection = eventAtDate(editedDate);
    const QVariantList followingOldDateProjection = eventAtDate(followingDate);
    const QVariantList followingNewDateProjection =
        eventAtDate(shiftedFollowingDate);

    QCOMPARE(earlierProjection.size(), 1);
    QCOMPARE(
        earlierProjection.first().toMap().value(QStringLiteral("id")).toInt(),
        earlierId
        );
    QCOMPARE(
        earlierProjection.first().toMap().value(QStringLiteral("title")).toString(),
        QStringLiteral("F379 Earlier Occurrence")
        );
    QVERIFY(selectedOldDateProjection.isEmpty());
    QVERIFY(followingOldDateProjection.isEmpty());
    QCOMPARE(selectedNewDateProjection.size(), 1);
    QCOMPARE(followingNewDateProjection.size(), 1);

    const QVariantMap selectedProjection =
        selectedNewDateProjection.first().toMap();
    QCOMPARE(selectedProjection.value(QStringLiteral("id")).toInt(), selectedId);
    QCOMPARE(
        selectedProjection.value(QStringLiteral("title")).toString(),
        QStringLiteral("F379 Revised Workshop")
        );
    QCOMPARE(
        selectedProjection.value(QStringLiteral("eventType")).toString(),
        QStringLiteral("Workshop")
        );
    QCOMPARE(
        selectedProjection.value(QStringLiteral("start")).toDateTime().date(),
        editedDate
        );
    QCOMPARE(
        selectedProjection.value(QStringLiteral("start")).toDateTime().time(),
        QTime(10, 15)
        );
    QCOMPARE(
        selectedProjection.value(QStringLiteral("end")).toDateTime().date(),
        editedDate
        );
    QCOMPARE(
        selectedProjection.value(QStringLiteral("end")).toDateTime().time(),
        QTime(11, 45)
        );

    const QVariantMap followingProjection =
        followingNewDateProjection.first().toMap();
    QCOMPARE(
        followingProjection.value(QStringLiteral("id")).toInt(),
        followingId
        );
    QCOMPARE(
        followingProjection.value(QStringLiteral("title")).toString(),
        QStringLiteral("F379 Revised Workshop")
        );
    QCOMPARE(
        followingProjection.value(QStringLiteral("eventType")).toString(),
        QStringLiteral("Workshop")
        );
    QCOMPARE(
        followingProjection.value(QStringLiteral("start")).toDateTime().date(),
        shiftedFollowingDate
        );
    QCOMPARE(
        followingProjection.value(QStringLiteral("start")).toDateTime().time(),
        QTime(10, 15)
        );
    QCOMPARE(
        followingProjection.value(QStringLiteral("end")).toDateTime().date(),
        shiftedFollowingDate
        );
    QCOMPARE(
        followingProjection.value(QStringLiteral("end")).toDateTime().time(),
        QTime(11, 45)
        );

    emitF379Transcript({
        {QStringLiteral("series_id"), seriesId},
        {
            QStringLiteral("ids"),
            QJsonArray({earlierId, selectedId, followingId})
        },
        {QStringLiteral("earlier_unchanged"), true},
        {QStringLiteral("middle_and_following_updated"), true},
        {QStringLiteral("unrelated_unchanged"), true},
        {QStringLiteral("suffix_ids_and_series_preserved"), true},
        {QStringLiteral("calendar_projection_refreshed"), true},
        {QStringLiteral("old_suffix_dates_empty"), true}
    });
}

void CalendarPageEventMutationParityTests::
monthGridCellsMatchCalendarEventModelDateProjection()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate firstOfMonth(2026, 11, 1);
    const QDate multiDayStart(2026, 11, 10);
    const QDate holidayDate(2026, 11, 11);
    const QDate multiDayEnd(2026, 11, 12);

    CalendarEvent multiDayEvent;
    multiDayEvent.title = QStringLiteral("F382 Multi Day Workshop");
    multiDayEvent.eventType = QStringLiteral("Workshop");
    multiDayEvent.timeStatus = QStringLiteral("Timed");
    multiDayEvent.startDate = multiDayStart;
    multiDayEvent.startTime = QTime(9, 0);
    multiDayEvent.endDate = multiDayEnd;
    multiDayEvent.endTime = QTime(10, 0);
    const auto multiDaySaved = workspace.services.databaseSession()
        ->calendarEventRepository()
        ->saveCalendarEvent(multiDayEvent);
    if (!multiDaySaved.has_value())
    {
        setupError = toLegacyDiagnostic(multiDaySaved.error());
    }
    QVERIFY2(multiDaySaved.has_value(), qPrintable(setupError));
    const int multiDayId = *multiDaySaved;

    CalendarEvent holidayEvent;
    holidayEvent.title = QStringLiteral("F382 In Month Holiday");
    holidayEvent.eventType = QStringLiteral("Holiday");
    holidayEvent.timeStatus = QStringLiteral("Timed");
    holidayEvent.startDate = holidayDate;
    holidayEvent.startTime = QTime(13, 0);
    holidayEvent.endDate = holidayDate;
    holidayEvent.endTime = QTime(14, 0);
    const auto holidaySaved = workspace.services.databaseSession()
        ->calendarEventRepository()
        ->saveCalendarEvent(holidayEvent);
    if (!holidaySaved.has_value())
    {
        setupError = toLegacyDiagnostic(holidaySaved.error());
    }
    QVERIFY2(holidaySaved.has_value(), qPrintable(setupError));
    const int holidayId = *holidaySaved;

    CalendarPage page(&workspace.services);
    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);
    QVERIFY(root->setProperty(
        "shownDate",
        QDateTime(firstOfMonth, QTime(0, 0))
        ));
    QCOMPARE(
        root->property("shownDate").toDateTime().date(),
        firstOfMonth
        );

    CalendarEventModel* const model = calendarModel(page);
    QVERIFY(model);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(firstOfMonth.year(), firstOfMonth.month()),
        5000
        );

    const QList<QDate> selectedDates{
        multiDayStart,
        holidayDate,
        multiDayEnd
    };
    for (const QDate& date : selectedDates)
    {
        QQuickItem* const cell = monthGridCell(root, date);
        QVERIFY2(
            cell,
            qPrintable(QStringLiteral("No active month-grid cell for %1")
                .arg(date.toString(Qt::ISODate)))
            );

        const QVariantList expectedProjection = model->eventsForDate(
            date.year(),
            date.month(),
            date.day()
            );
        QTRY_COMPARE_WITH_TIMEOUT(
            eventProjectionRows(cell->property("dayEvents").toList()),
            eventProjectionRows(expectedProjection),
            5000
            );
    }

    QQuickItem* const startCell = monthGridCell(root, multiDayStart);
    QQuickItem* const holidayCell = monthGridCell(root, holidayDate);
    QQuickItem* const endCell = monthGridCell(root, multiDayEnd);
    QVERIFY(startCell);
    QVERIFY(holidayCell);
    QVERIFY(endCell);

    const QStringList multiDayRow{
        QString::fromUtf8(QJsonDocument(QJsonArray{
            multiDayId,
            QStringLiteral("F382 Multi Day Workshop"),
            QStringLiteral("Workshop")
        }).toJson(QJsonDocument::Compact))
    };
    const QStringList holidayDayRows{
        multiDayRow.first(),
        QString::fromUtf8(QJsonDocument(QJsonArray{
            holidayId,
            QStringLiteral("F382 In Month Holiday"),
            QStringLiteral("Holiday")
        }).toJson(QJsonDocument::Compact))
    };
    QCOMPARE(
        eventProjectionRows(startCell->property("dayEvents").toList()),
        multiDayRow
        );
    QCOMPARE(
        eventProjectionRows(holidayCell->property("dayEvents").toList()),
        holidayDayRows
        );
    QCOMPARE(
        eventProjectionRows(endCell->property("dayEvents").toList()),
        multiDayRow
        );
    QCOMPARE(holidayCell->property("redDay").toBool(), true);
    QCOMPARE(startCell->property("redDay").toBool(), false);
    QCOMPARE(endCell->property("redDay").toBool(), false);
}


void CalendarPageEventMutationParityTests::
currentMonthUpcomingRowsMatchVisibleCalendarRange()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        enable24HourFixtureTime(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate today = QDate::currentDate();
    const QDate firstOfDisplayedMonth =
        QDate(today.year(), today.month(), 1).addMonths(1);
    const QDate multiDayStart =
        firstOfDisplayedMonth.addDays(3);
    const QDate multiDayEnd =
        firstOfDisplayedMonth.addDays(5);
    const QDate holidayDate =
        firstOfDisplayedMonth.addDays(8);
    const QDate unknownTimeDate =
        firstOfDisplayedMonth.addDays(11);
    const QDate outsideMonthDate =
        firstOfDisplayedMonth.addMonths(1).addDays(2);

    CalendarEvent multiDay;
    multiDay.title = QStringLiteral("F383 Multi Day Timed");
    multiDay.eventType = QStringLiteral("Workshop");
    multiDay.timeStatus = QStringLiteral("Timed");
    multiDay.startDate = multiDayStart;
    multiDay.startTime = QTime(9, 15);
    multiDay.endDate = multiDayEnd;
    multiDay.endTime = QTime(10, 45);
    const int multiDayId =
        saveFixtureEvent(workspace, multiDay, &setupError);
    QVERIFY2(multiDayId > 0, qPrintable(setupError));

    CalendarEvent holiday;
    holiday.title = QStringLiteral("F383 All Day Holiday");
    holiday.eventType = QStringLiteral("Holiday");
    holiday.timeStatus = QStringLiteral("Timed");
    holiday.allDay = true;
    holiday.startDate = holidayDate;
    holiday.endDate = holidayDate;
    const int holidayId =
        saveFixtureEvent(workspace, holiday, &setupError);
    QVERIFY2(holidayId > 0, qPrintable(setupError));

    CalendarEvent unknownTime;
    unknownTime.title = QStringLiteral("F383 Unknown Time Event");
    unknownTime.eventType = QStringLiteral("Other");
    unknownTime.timeStatus = QStringLiteral("Unknown");
    unknownTime.startDate = unknownTimeDate;
    unknownTime.endDate = unknownTimeDate;
    const int unknownTimeId =
        saveFixtureEvent(workspace, unknownTime, &setupError);
    QVERIFY2(unknownTimeId > 0, qPrintable(setupError));

    CalendarEvent outsideMonth;
    outsideMonth.title = QStringLiteral("F383 Outside Month Sentinel");
    outsideMonth.eventType = QStringLiteral("Meeting");
    outsideMonth.timeStatus = QStringLiteral("Timed");
    outsideMonth.startDate = outsideMonthDate;
    outsideMonth.startTime = QTime(8, 0);
    outsideMonth.endDate = outsideMonthDate;
    outsideMonth.endTime = QTime(9, 0);
    const int outsideMonthId =
        saveFixtureEvent(workspace, outsideMonth, &setupError);
    QVERIFY2(outsideMonthId > 0, qPrintable(setupError));

    CalendarPage page(&workspace.services);
    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);
    QVERIFY(root->setProperty(
        "shownDate",
        QDateTime(firstOfDisplayedMonth, QTime(0, 0))
        ));
    QCOMPARE(
        root->property("shownDate").toDateTime().date(),
        firstOfDisplayedMonth
        );

    CalendarEventModel* const model = calendarModel(page);
    QVERIFY(model);
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(
            firstOfDisplayedMonth.year(),
            firstOfDisplayedMonth.month()
            ),
        10000
        );

    NavigationTabWidget* const tabs = upcomingTabs(page);
    QVERIFY(tabs);
    QCOMPARE(tabs->currentIndex(), 0);

    const QStringList expectedRows{
        upcomingRowJson(
            multiDayId,
            QStringLiteral("%1 - %2")
                .arg(
                    multiDayStart.toString(QStringLiteral("MMM d")),
                    multiDayEnd.toString(QStringLiteral("MMM d yyyy"))
                    ),
            QStringLiteral("09:15 - 10:45"),
            multiDay.title,
            QStringLiteral("Workshop")
            ),
        upcomingRowJson(
            holidayId,
            holidayDate.toString(QStringLiteral("MMM d")),
            QStringLiteral("All day"),
            holiday.title,
            QStringLiteral("Holiday")
            ),
        upcomingRowJson(
            unknownTimeId,
            unknownTimeDate.toString(QStringLiteral("MMM d")),
            QStringLiteral("Unknown Time"),
            unknownTime.title,
            QStringLiteral("Other")
            )
    };
    QTRY_COMPARE_WITH_TIMEOUT(
        visibleUpcomingRows(tabs->currentWidget()),
        expectedRows,
        10000
        );
    const QStringList actualRows =
        visibleUpcomingRows(tabs->currentWidget());
    QCOMPARE(actualRows, expectedRows);
    const QString outsideMonthRow = upcomingRowJson(
        outsideMonthId,
        outsideMonthDate.toString(QStringLiteral("MMM d")),
        QStringLiteral("08:00 - 09:00"),
        outsideMonth.title,
        QStringLiteral("Meeting")
        );
    QVERIFY(!actualRows.contains(outsideMonthRow));

    emitF383Transcript({
        {QStringLiteral("scenario"), QStringLiteral("current_month")},
        {
            QStringLiteral("displayed_month"),
            firstOfDisplayedMonth.toString(QStringLiteral("yyyy-MM"))
        },
        {QStringLiteral("rows"), transcriptRows(actualRows)}
    });
}

void CalendarPageEventMutationParityTests::
currentMonthNavigationEvictsPriorMonthResources()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );

    constexpr int NavigatedMonthCount = 4;
    const QDate today = QDate::currentDate();
    const QDate firstMonth = QDate(
        qMax(today.year(), 2026),
        today.month(),
        1
        ).addMonths(2);
    QList<QDate> eventDates;
    QStringList eventTitles;
    for (int monthIndex = 0; monthIndex < NavigatedMonthCount; ++monthIndex)
    {
        const QDate eventDate = firstMonth.addMonths(monthIndex).addDays(7);
        const QString title = QStringLiteral("F784 Retained Month %1")
            .arg(monthIndex + 1);
        CalendarEvent event;
        event.title = title;
        event.eventType = QStringLiteral("Holiday");
        event.timeStatus = QStringLiteral("All Day");
        event.allDay = true;
        event.startDate = eventDate;
        event.endDate = eventDate;
        const int eventId = saveFixtureEvent(workspace, event, &setupError);
        QVERIFY2(eventId > 0, qPrintable(setupError));
        eventDates.append(eventDate);
        eventTitles.append(title);
    }

    CalendarPage page(&workspace.services);
    auto* const tabs = upcomingTabs(page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(
        static_cast<int>(UpcomingEventsScope::CurrentMonth)
        );
    QCOMPARE(
        tabs->currentIndex(),
        static_cast<int>(UpcomingEventsScope::CurrentMonth)
        );

    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);
    CalendarEventModel* const model = calendarModel(page);
    QVERIFY(model);
    CalendarEventCache* const cache =
        page.findChild<CalendarEventCache*>();
    QVERIFY(cache);

    for (int monthIndex = 0; monthIndex < NavigatedMonthCount; ++monthIndex)
    {
        const QDate month = firstMonth.addMonths(monthIndex);
        QVERIFY(root->setProperty(
            "shownDate",
            QDateTime(month, QTime(0, 0))
            ));
        QCOMPARE(
            root->property("shownDate").toDateTime().date(),
            month
            );

        QTRY_VERIFY_WITH_TIMEOUT(
            model->isMonthLoaded(month.year(), month.month()),
            10000
            );
        QTRY_VERIFY_WITH_TIMEOUT(!cache->isLoading(), 10000);

        const CalendarPageRuntimeMetrics metrics = page.runtimeMetrics();
        QCOMPARE(metrics.loadedMonthCount, 1);
        QCOMPARE(metrics.cacheRetainedRangeCount, 1);
        QCOMPARE(metrics.cacheLoadedRangeCount, 1);
        QCOMPARE(metrics.cacheEventCount, 1);
        QCOMPARE(metrics.onDemandRetainedRangeCount, 0);
        const QVariantList currentEvents = model->eventsForDate(
            eventDates.at(monthIndex).year(),
            eventDates.at(monthIndex).month(),
            eventDates.at(monthIndex).day()
            );
        QCOMPARE(currentEvents.size(), 1);
        QCOMPARE(
            currentEvents.constFirst().toMap()
                .value(QStringLiteral("title")).toString(),
            eventTitles.at(monthIndex)
            );

        if (monthIndex > 0)
        {
            const QDate previousMonth = firstMonth.addMonths(monthIndex - 1);
            const QDate previousEventDate = eventDates.at(monthIndex - 1);
            QVERIFY(!model->isMonthLoaded(
                previousMonth.year(),
                previousMonth.month()
                ));
            QVERIFY(model->eventsForDate(
                previousEventDate.year(),
                previousEventDate.month(),
                previousEventDate.day()
                ).isEmpty());
        }
    }

    const QDate evictedMonth = firstMonth;
    const QDate evictedEventDate = eventDates.constFirst();
    QVERIFY(!model->isMonthLoaded(
        evictedMonth.year(),
        evictedMonth.month()
        ));
    QVERIFY(model->eventsForDate(
        evictedEventDate.year(),
        evictedEventDate.month(),
        evictedEventDate.day()
        ).isEmpty());

    QVERIFY(root->setProperty(
        "shownDate",
        QDateTime(evictedMonth, QTime(0, 0))
        ));
    QTRY_VERIFY_WITH_TIMEOUT(
        model->isMonthLoaded(evictedMonth.year(), evictedMonth.month()),
        10000
        );
    QTRY_VERIFY_WITH_TIMEOUT(!cache->isLoading(), 10000);

    const QVariantList reloadedEvents = model->eventsForDate(
        evictedEventDate.year(),
        evictedEventDate.month(),
        evictedEventDate.day()
        );
    QCOMPARE(reloadedEvents.size(), 1);
    QCOMPARE(
        reloadedEvents.constFirst().toMap()
            .value(QStringLiteral("title")).toString(),
        eventTitles.constFirst()
        );
    const CalendarPageRuntimeMetrics reloadedMetrics = page.runtimeMetrics();
    QCOMPARE(reloadedMetrics.loadedMonthCount, 1);
    QCOMPARE(reloadedMetrics.cacheRetainedRangeCount, 1);
    QCOMPARE(reloadedMetrics.cacheLoadedRangeCount, 1);
    QCOMPARE(reloadedMetrics.cacheEventCount, 1);
    QCOMPARE(reloadedMetrics.onDemandRetainedRangeCount, 0);
}

void CalendarPageEventMutationParityTests::
next30DaysIncludesBoundariesAndFiltersEventTypes()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        enable24HourFixtureTime(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate today = QDate::currentDate();
    const QDate lastIncluded = today.addDays(30);
    const QDate excluded = today.addDays(31);

    CalendarEvent todayEvent;
    todayEvent.title = QStringLiteral("F383 Today Boundary");
    todayEvent.eventType = QStringLiteral("Workshop");
    todayEvent.timeStatus = QStringLiteral("Timed");
    todayEvent.startDate = today;
    todayEvent.startTime = QTime(8, 30);
    todayEvent.endDate = today;
    todayEvent.endTime = QTime(9, 0);
    const int todayId =
        saveFixtureEvent(workspace, todayEvent, &setupError);
    QVERIFY2(todayId > 0, qPrintable(setupError));

    CalendarEvent holiday;
    holiday.title = QStringLiteral("F383 Filtered Holiday");
    holiday.eventType = QStringLiteral("Holiday");
    holiday.timeStatus = QStringLiteral("Timed");
    holiday.startDate = today.addDays(5);
    holiday.startTime = QTime(12, 0);
    holiday.endDate = holiday.startDate;
    holiday.endTime = QTime(13, 0);
    const int holidayId =
        saveFixtureEvent(workspace, holiday, &setupError);
    QVERIFY2(holidayId > 0, qPrintable(setupError));

    CalendarEvent lastIncludedEvent;
    lastIncludedEvent.title = QStringLiteral("F383 Day Thirty Boundary");
    lastIncludedEvent.eventType = QStringLiteral("Workshop");
    lastIncludedEvent.timeStatus = QStringLiteral("Timed");
    lastIncludedEvent.allDay = true;
    lastIncludedEvent.startDate = lastIncluded;
    lastIncludedEvent.endDate = lastIncluded;
    const int lastIncludedId =
        saveFixtureEvent(workspace, lastIncludedEvent, &setupError);
    QVERIFY2(lastIncludedId > 0, qPrintable(setupError));

    CalendarEvent outsideRange;
    outsideRange.title = QStringLiteral("F383 Day Thirty One Sentinel");
    outsideRange.eventType = QStringLiteral("Workshop");
    outsideRange.timeStatus = QStringLiteral("Timed");
    outsideRange.startDate = excluded;
    outsideRange.startTime = QTime(7, 0);
    outsideRange.endDate = excluded;
    outsideRange.endTime = QTime(8, 0);
    const int excludedId =
        saveFixtureEvent(workspace, outsideRange, &setupError);
    QVERIFY2(excludedId > 0, qPrintable(setupError));

    CalendarPage page(&workspace.services);
    NavigationTabWidget* const tabs = upcomingTabs(page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(1);
    QCOMPARE(tabs->currentIndex(), 1);

    const QStringList expectedBeforeFilter{
        upcomingRowJson(
            todayId,
            today.toString(QStringLiteral("MMM d")),
            QStringLiteral("08:30 - 09:00"),
            todayEvent.title,
            QStringLiteral("Workshop")
            ),
        upcomingRowJson(
            holidayId,
            holiday.startDate.toString(QStringLiteral("MMM d")),
            QStringLiteral("12:00 - 13:00"),
            holiday.title,
            QStringLiteral("Holiday")
            ),
        upcomingRowJson(
            lastIncludedId,
            lastIncluded.toString(QStringLiteral("MMM d")),
            QStringLiteral("All day"),
            lastIncludedEvent.title,
            QStringLiteral("Workshop")
            )
    };
    QTRY_COMPARE_WITH_TIMEOUT(
        visibleUpcomingRows(tabs->currentWidget()),
        expectedBeforeFilter,
        15000
        );
    const QStringList actualBeforeFilter =
        visibleUpcomingRows(tabs->currentWidget());
    QCOMPARE(actualBeforeFilter, expectedBeforeFilter);
    const QString dayThirtyOneRow = upcomingRowJson(
        excludedId,
        excluded.toString(QStringLiteral("MMM d")),
        QStringLiteral("07:00 - 08:00"),
        outsideRange.title,
        QStringLiteral("Workshop")
        );
    QVERIFY(!actualBeforeFilter.contains(dayThirtyOneRow));

    QPushButton* holidayFilter = nullptr;
    for (QPushButton* const button : tabs->currentWidget()
             ->findChildren<QPushButton*>())
    {
        if (
            button->property("eventType").toString()
                == QStringLiteral("Holiday")
        )
        {
            holidayFilter = button;
            break;
        }
    }
    QVERIFY(holidayFilter);
    QVERIFY(holidayFilter->isChecked());
    holidayFilter->click();
    QVERIFY(!holidayFilter->isChecked());

    const QStringList expectedAfterFilter{
        expectedBeforeFilter.at(0),
        expectedBeforeFilter.at(2)
    };
    QTRY_COMPARE_WITH_TIMEOUT(
        visibleUpcomingRows(tabs->currentWidget()),
        expectedAfterFilter,
        5000
        );
    const QStringList actualAfterFilter =
        visibleUpcomingRows(tabs->currentWidget());
    QCOMPARE(actualAfterFilter, expectedAfterFilter);

    emitF383Transcript({
        {QStringLiteral("scenario"), QStringLiteral("next_30_days")},
        {QStringLiteral("today"), today.toString(Qt::ISODate)},
        {QStringLiteral("last_included"), lastIncluded.toString(Qt::ISODate)},
        {QStringLiteral("excluded"), excluded.toString(Qt::ISODate)},
        {QStringLiteral("filter_toggled_off"), QStringLiteral("Holiday")},
        {QStringLiteral("before_filter"), transcriptRows(actualBeforeFilter)},
        {QStringLiteral("after_filter"), transcriptRows(actualAfterFilter)}
    });
}

void CalendarPageEventMutationParityTests::
campusReadFailureKeepsAllEventsVisible()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );

    SettingsRepository* const settings =
        workspace.services.databaseSession()->settingsRepository();
    QVERIFY(settings);
    QVERIFY(settings->saveSetting(
        QStringLiteral("myInfo/campus"),
        QStringLiteral("j")
        ));

    const QDate eventDate = QDate::currentDate().addDays(2);
    CalendarEvent otherCampusEvent;
    otherCampusEvent.title = QStringLiteral("F1066 Bundang Campus Event");
    otherCampusEvent.eventType = QStringLiteral("Workshop");
    otherCampusEvent.timeStatus = QStringLiteral("Timed");
    otherCampusEvent.startDate = eventDate;
    otherCampusEvent.startTime = QTime(9, 0);
    otherCampusEvent.endDate = eventDate;
    otherCampusEvent.endTime = QTime(10, 0);
    QVERIFY2(
        saveFixtureEvent(workspace, otherCampusEvent, &setupError) > 0,
        qPrintable(setupError)
        );

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "ALTER TABLE app_settings "
                "RENAME TO app_settings_temporarily_unavailable"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );

    CalendarPage page(&workspace.services);
    NavigationTabWidget* const tabs = upcomingTabs(page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(1);

    QTRY_VERIFY_WITH_TIMEOUT(
        visibleUpcomingRows(tabs->currentWidget())
            .join(QLatin1Char('\n'))
            .contains(otherCampusEvent.title),
        15000
        );
}

void CalendarPageEventMutationParityTests::
calendarDisplayPreferenceFailuresShowNoticeAndRecover()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        enable24HourFixtureTime(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate eventDate = QDate::currentDate().addDays(2);
    CalendarEvent event;
    event.title = QStringLiteral("F1301 Preference fallback event");
    event.eventType = QStringLiteral("Workshop");
    event.timeStatus = QStringLiteral("Timed");
    event.startDate = eventDate;
    event.startTime = QTime(9, 0);
    event.endDate = eventDate;
    event.endTime = QTime(10, 0);
    QVERIFY2(
        saveFixtureEvent(workspace, event, &setupError) > 0,
        qPrintable(setupError)
        );

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "ALTER TABLE app_settings "
                "RENAME TO app_settings_temporarily_unavailable"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );

    CalendarPreferenceNoticeTranslator translator;
    CalendarPage page(&workspace.services);
    page.show();
    auto* const tabs = upcomingTabs(page);
    QLabel* const notice = displayPreferencesNotice(page);
    QVERIFY(tabs);
    QVERIFY(notice);
    tabs->setCurrentIndex(1);
    QTRY_VERIFY_WITH_TIMEOUT(!notice->isHidden(), 5000);
    QCOMPARE(
        notice->text(),
        QStringLiteral(
            "Some calendar display preferences could not be loaded. "
            "Default values are being used."
            )
        );
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    ScopedTranslatorInstallation translatorScope(&translator);
    QVERIFY(translatorScope.installed());
    page.retranslateUi();
    QCOMPARE(
        notice->text(),
        QStringLiteral("Localized calendar preference notice.")
        );

    QTRY_VERIFY_WITH_TIMEOUT(
        visibleUpcomingRows(tabs->currentWidget())
            .join(QLatin1Char('\n'))
            .contains(event.title),
        15000
        );
    QStringList rows = visibleUpcomingRows(tabs->currentWidget());
    QCOMPARE(rows.size(), 1);
    QVERIFY(rows.constFirst().contains(QStringLiteral("9:00 AM - 10:00 AM")));
    QVERIFY(visibleUpcomingFailure(tabs->currentWidget()).isEmpty());

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "ALTER TABLE app_settings_temporarily_unavailable "
                "RENAME TO app_settings"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );
    page.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(notice->isHidden(), 5000);
    QVERIFY(notice->text().isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(
        visibleUpcomingRows(tabs->currentWidget())
            .join(QLatin1Char('\n'))
            .contains(event.title),
        15000
        );
    rows = visibleUpcomingRows(tabs->currentWidget());
    QCOMPARE(rows.size(), 1);
    QVERIFY(rows.constFirst().contains(QStringLiteral("09:00 - 10:00")));
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void CalendarPageEventMutationParityTests::
calendarDisplayPreferenceDefaultsDoNotShowNotice()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );

    CalendarPage page(&workspace.services);
    QLabel* const notice = displayPreferencesNotice(page);
    QVERIFY(notice);
    QVERIFY(notice->isHidden());
    QVERIFY(notice->text().isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    workspace.services.closeDatabase();
    page.refresh();
    QVERIFY(notice->isHidden());
    QVERIFY(notice->text().isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void CalendarPageEventMutationParityTests::
calendarDisplayPreferenceFailuresAreDetectedIndependently()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        enable24HourFixtureTime(workspace, &setupError),
        qPrintable(setupError)
        );

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "INSERT OR IGNORE INTO app_settings (key, value) "
                "VALUES ('calendar/firstDayOfWeek', '1')"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "ALTER TABLE app_settings "
                "RENAME TO app_settings_base"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );
    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "INSERT OR IGNORE INTO app_settings_base (key, value) "
                "VALUES ('calendar/showEventsAtAllCampuses', 'false')"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );
    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "INSERT OR IGNORE INTO app_settings_base (key, value) "
                "VALUES ('myInfo/campus', 'j')"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );
    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "CREATE TABLE app_settings_read_fault (key TEXT NOT NULL)"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );
    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "INSERT INTO app_settings_read_fault (key) "
                "VALUES ('myInfo/campus')"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );
    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "CREATE VIEW app_settings AS "
                "SELECT key, CASE WHEN key = ("
                "SELECT key FROM app_settings_read_fault LIMIT 1) "
                "THEN json_extract('not-json', '$.value') "
                "ELSE value END AS value FROM app_settings_base"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );

    CalendarPage page(&workspace.services);
    QLabel* const notice = displayPreferencesNotice(page);
    QVERIFY(notice);
    QVERIFY(notice->isHidden());
    QVERIFY(notice->text().isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "UPDATE app_settings_read_fault "
                "SET key = 'calendar/firstDayOfWeek'"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );
    page.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!notice->isHidden(), 5000);
    QCOMPARE(
        notice->text(),
        QStringLiteral(
            "Some calendar display preferences could not be loaded. "
            "Default values are being used."
            )
        );
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "UPDATE app_settings_read_fault "
                "SET key = 'unused/calendarPreference'"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );
    page.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(notice->isHidden(), 5000);
    QVERIFY(notice->text().isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "UPDATE app_settings_read_fault "
                "SET key = 'calendar/showEventsAtAllCampuses'"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );
    page.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!notice->isHidden(), 5000);
    QCOMPARE(
        notice->text(),
        QStringLiteral(
            "Some calendar display preferences could not be loaded. "
            "Default values are being used."
            )
        );
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "UPDATE app_settings_read_fault "
                "SET key = 'schedule_use_24h'"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );
    page.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!notice->isHidden(), 5000);
    QCOMPARE(
        notice->text(),
        QStringLiteral(
            "Some calendar display preferences could not be loaded. "
            "Default values are being used."
            )
        );
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral("DELETE FROM app_settings_read_fault"),
            &setupError
            ),
        qPrintable(setupError)
        );
    page.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(notice->isHidden(), 5000);
    QVERIFY(notice->text().isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void CalendarPageEventMutationParityTests::
unknownNonemptyCampusDoesNotUseEmptyFallback()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );

    const QString storedPreference = QStringLiteral("unknown legacy campus");
    SettingsRepository* const settings =
        workspace.services.databaseSession()->settingsRepository();
    QVERIFY(settings);
    QVERIFY(settings->saveSetting(
        QStringLiteral("myInfo/campus"),
        storedPreference
        ));

    const QDate eventDate = QDate::currentDate().addDays(2);
    CalendarEvent otherCampusEvent;
    otherCampusEvent.title = QStringLiteral("F1101 DAE Other Campus");
    otherCampusEvent.eventType = QStringLiteral("Workshop");
    otherCampusEvent.timeStatus = QStringLiteral("Timed");
    otherCampusEvent.startDate = eventDate;
    otherCampusEvent.startTime = QTime(9, 0);
    otherCampusEvent.endDate = eventDate;
    otherCampusEvent.endTime = QTime(10, 0);
    QVERIFY2(
        saveFixtureEvent(workspace, otherCampusEvent, &setupError) > 0,
        qPrintable(setupError)
        );

    CalendarEvent campusIndependentEvent;
    campusIndependentEvent.title = QStringLiteral("Workshop with no campus code");
    campusIndependentEvent.eventType = QStringLiteral("Workshop");
    campusIndependentEvent.timeStatus = QStringLiteral("Timed");
    campusIndependentEvent.startDate = eventDate;
    campusIndependentEvent.startTime = QTime(11, 0);
    campusIndependentEvent.endDate = eventDate;
    campusIndependentEvent.endTime = QTime(12, 0);
    QVERIFY2(
        saveFixtureEvent(workspace, campusIndependentEvent, &setupError) > 0,
        qPrintable(setupError)
        );

    CalendarPage page(&workspace.services);
    NavigationTabWidget* const tabs = upcomingTabs(page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(1);

    QTRY_VERIFY_WITH_TIMEOUT(
        visibleUpcomingRows(tabs->currentWidget())
            .join(QLatin1Char('\n'))
            .contains(campusIndependentEvent.title),
        15000
        );
    QVERIFY(!visibleUpcomingRows(tabs->currentWidget())
        .join(QLatin1Char('\n'))
        .contains(otherCampusEvent.title));

    const auto stored = settings->loadSetting(QStringLiteral("myInfo/campus"));
    QVERIFY(stored);
    QCOMPARE(stored->toString(), storedPreference);
}

void CalendarPageEventMutationParityTests::
currentCampusIdAliasShowsMatchingCampusEvents()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );

    SettingsRepository* const settings =
        workspace.services.databaseSession()->settingsRepository();
    QVERIFY(settings);
    QVERIFY(settings->saveSetting(
        QStringLiteral("myInfo/campus"),
        QStringLiteral("Dongtan 2")
        ));

    const QDate eventDate = QDate::currentDate().addDays(2);
    const QString selectedCampusAliasTitle =
        QStringLiteral("F1101 dongtan_2 DAE Campus ID Alias");
    CalendarEvent selectedCampusAliasEvent;
    selectedCampusAliasEvent.title = selectedCampusAliasTitle;
    selectedCampusAliasEvent.eventType = QStringLiteral("Workshop");
    selectedCampusAliasEvent.timeStatus = QStringLiteral("Timed");
    selectedCampusAliasEvent.startDate = eventDate;
    selectedCampusAliasEvent.startTime = QTime(9, 0);
    selectedCampusAliasEvent.endDate = eventDate;
    selectedCampusAliasEvent.endTime = QTime(10, 0);
    QVERIFY2(
        saveFixtureEvent(workspace, selectedCampusAliasEvent, &setupError) > 0,
        qPrintable(setupError)
        );

    const QString otherCampusTitle =
        QStringLiteral("F1101 DAE Other Campus");
    CalendarEvent otherCampusEvent;
    otherCampusEvent.title = otherCampusTitle;
    otherCampusEvent.eventType = QStringLiteral("Workshop");
    otherCampusEvent.timeStatus = QStringLiteral("Timed");
    otherCampusEvent.startDate = eventDate;
    otherCampusEvent.startTime = QTime(11, 0);
    otherCampusEvent.endDate = eventDate;
    otherCampusEvent.endTime = QTime(12, 0);
    QVERIFY2(
        saveFixtureEvent(workspace, otherCampusEvent, &setupError) > 0,
        qPrintable(setupError)
        );

    CalendarPage page(&workspace.services);
    NavigationTabWidget* const tabs = upcomingTabs(page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(1);

    QTRY_VERIFY_WITH_TIMEOUT(
        visibleUpcomingRows(tabs->currentWidget())
            .join(QLatin1Char('\n'))
            .contains(selectedCampusAliasTitle),
        15000
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        !visibleUpcomingRows(tabs->currentWidget())
             .join(QLatin1Char('\n'))
             .contains(otherCampusTitle),
        15000
        );
}

void CalendarPageEventMutationParityTests::
nextTenUpcomingEventsUseStableVisibleOrderAndLimit()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        enable24HourFixtureTime(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate today = QDate::currentDate();
    struct InputEvent final
    {
        int dayOffset;
        QTime startTime;
        QString title;
    };
    const QList<InputEvent> inputEvents{
        {1, QTime(10, 0), QStringLiteral("F383 Zulu")},
        {1, QTime(9, 0), QStringLiteral("F383 Alpha")},
        {1, QTime(9, 0), QStringLiteral("F383 Alpha")},
        {1, QTime(9, 0), QStringLiteral("F383 Gamma")},
        {2, QTime(8, 0), QStringLiteral("F383 Beta")},
        {2, QTime(9, 0), QStringLiteral("F383 Delta")},
        {2, QTime(9, 0), QStringLiteral("F383 Zeta")},
        {2, QTime(12, 0), QStringLiteral("F383 Alpha")},
        {3, QTime(10, 0), QStringLiteral("F383 Solo")},
        {3, QTime(10, 0), QStringLiteral("F383 Aardvark")},
        {3, QTime(8, 0), QStringLiteral("F383 Second")},
        {3, QTime(9, 0), QStringLiteral("F383 Last")}
    };

    QList<int> eventIds;
    for (const InputEvent& input : inputEvents)
    {
        CalendarEvent event;
        event.title = input.title;
        event.eventType = QStringLiteral("Workshop");
        event.timeStatus = QStringLiteral("Timed");
        event.startDate = today.addDays(input.dayOffset);
        event.startTime = input.startTime;
        event.endDate = event.startDate;
        event.endTime = input.startTime.addSecs(3600);
        const int id = saveFixtureEvent(workspace, event, &setupError);
        QVERIFY2(id > 0, qPrintable(setupError));
        eventIds.append(id);
    }
    QCOMPARE(eventIds.size(), 12);

    CalendarPage page(&workspace.services);
    NavigationTabWidget* const tabs = upcomingTabs(page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(2);
    QCOMPARE(tabs->currentIndex(), 2);

    const QList<int> expectedOrder{
        1, 2, 3, 0, 4, 5, 6, 7, 10, 11
    };
    QStringList expectedRows;
    for (const int inputIndex : expectedOrder)
    {
        const InputEvent& input = inputEvents.at(inputIndex);
        const QDate eventDate = today.addDays(input.dayOffset);
        const QTime endTime = input.startTime.addSecs(3600);
        expectedRows.append(upcomingRowJson(
            eventIds.at(inputIndex),
            eventDate.toString(QStringLiteral("MMM d")),
            QStringLiteral("%1 - %2")
                .arg(
                    input.startTime.toString(QStringLiteral("HH:mm")),
                    endTime.toString(QStringLiteral("HH:mm"))
                    ),
            input.title,
            QStringLiteral("Workshop")
            ));
    }

    QTRY_COMPARE_WITH_TIMEOUT(
        visibleUpcomingRows(tabs->currentWidget()),
        expectedRows,
        20000
        );
    const QStringList actualRows =
        visibleUpcomingRows(tabs->currentWidget());
    QCOMPARE(actualRows, expectedRows);
    QCOMPARE(actualRows.size(), 10);

    emitF383Transcript({
        {QStringLiteral("scenario"), QStringLiteral("next_10_events")},
        {QStringLiteral("today"), today.toString(Qt::ISODate)},
        {QStringLiteral("seed_count"), inputEvents.size()},
        {QStringLiteral("visible_count"), expectedRows.size()},
        {QStringLiteral("rows"), transcriptRows(actualRows)}
    });
}

void CalendarPageEventMutationParityTests::
completedEmptyUpcomingRangeShowsItsEmptyLabel()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate today = QDate::currentDate();
    CalendarPage page(&workspace.services);
    NavigationTabWidget* const tabs = upcomingTabs(page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(1);
    QCOMPARE(tabs->currentIndex(), 1);

    QTRY_COMPARE_WITH_TIMEOUT(
        visibleUpcomingEmptyLabel(tabs->currentWidget()),
        QStringLiteral("No upcoming events."),
        15000
        );
    const QStringList rows =
        visibleUpcomingRows(tabs->currentWidget());
    QVERIFY(rows.isEmpty());

    emitF383Transcript({
        {QStringLiteral("scenario"), QStringLiteral("empty_completed_range")},
        {QStringLiteral("today"), today.toString(Qt::ISODate)},
        {QStringLiteral("rows"), transcriptRows(rows)},
        {
            QStringLiteral("empty_label"),
            visibleUpcomingEmptyLabel(tabs->currentWidget())
        }
    });
}

void CalendarPageEventMutationParityTests::
completedEmptyNextTenSearchShowsItsEmptyLabel()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );

    CalendarPage page(&workspace.services);
    NavigationTabWidget* const tabs = upcomingTabs(page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(2);

    QTRY_COMPARE_WITH_TIMEOUT(
        visibleUpcomingEmptyLabel(tabs->currentWidget()),
        QStringLiteral("No upcoming events."),
        15000
        );
    QVERIFY(visibleUpcomingRows(tabs->currentWidget()).isEmpty());
    QVERIFY(visibleUpcomingFailure(tabs->currentWidget()).isEmpty());
    QVERIFY(!upcomingRetryButton(tabs->currentWidget()));
}

void CalendarPageEventMutationParityTests::
eventTypeColorPickerFailureResyncsAndRetries()
{
    const std::array normalizationCases = {
        std::pair{
            QStringLiteral("Vacation"),
            QStringLiteral("Vacation")
            },
        std::pair{
            QStringLiteral("Holiday"),
            QStringLiteral("Holiday")
            },
        std::pair{
            QStringLiteral("Workshop"),
            QStringLiteral("Workshop")
            },
        std::pair{
            QStringLiteral("CM"),
            QStringLiteral("CM")
            },
        std::pair{
            QStringLiteral("Meeting"),
            QStringLiteral("Meeting")
            },
        std::pair{
            QStringLiteral("Other"),
            QStringLiteral("Other")
            }
    };
    for (const auto& [input, expected] : normalizationCases)
    {
        QCOMPARE(normalizedCalendarEventType(input), expected);
    }
    QCOMPARE(
        normalizedCalendarEventType(QStringLiteral(" \tHoliday \n")),
        QStringLiteral("Holiday")
        );
    QCOMPARE(
        normalizedCalendarEventType(QStringLiteral("holiday")),
        QStringLiteral("Other")
        );
    QCOMPARE(
        normalizedCalendarEventType(QStringLiteral(" Conference ")),
        QStringLiteral("Other")
        );

    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );

    WorkspaceStore* const session =
        workspace.services.workspaceStore();
    QVERIFY(session);
    QVERIFY(session->isOpen());
    SettingsRepository* const settings = session->settingsRepository();
    QVERIFY(settings);

    const QString eventType = QStringLiteral("Holiday");
    const QString colorKey =
        QStringLiteral("calendar/eventTypeColor/") + eventType;
    const QString unrelatedKey =
        QStringLiteral("calendar/unrelatedColorPreference");
    const QColor originalColor(QStringLiteral("#1a2b3c"));
    const QColor attemptedColor(QStringLiteral("#de4567"));
    QVERIFY(originalColor.isValid());
    QVERIFY(attemptedColor.isValid());
    QVERIFY(settings->saveSetting(
        colorKey,
        originalColor.name(QColor::HexRgb)
        ));
    QVERIFY(settings->saveSetting(
        unrelatedKey,
        QStringLiteral("preserved")
        ));

    QString sqlError;
    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "CREATE TABLE calendar_event_type_color_write_audit ("
                "successful_updates INTEGER NOT NULL)"
                ),
            &sqlError
            ),
        qPrintable(sqlError)
        );
    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "INSERT INTO calendar_event_type_color_write_audit "
                "(successful_updates) VALUES (0)"
                ),
            &sqlError
            ),
        qPrintable(sqlError)
        );
    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(R"(
                CREATE TRIGGER audit_calendar_event_type_color_update
                AFTER UPDATE ON app_settings
                WHEN NEW.key = 'calendar/eventTypeColor/Holiday'
                BEGIN
                    UPDATE calendar_event_type_color_write_audit
                    SET successful_updates = successful_updates + 1;
                END
            )"),
            &sqlError
            ),
        qPrintable(sqlError)
        );

    const QDate eventDate = QDate::currentDate().addDays(2);
    CalendarEvent event;
    event.title = QStringLiteral("F668 Color Picker Failure Event");
    event.eventType = eventType;
    event.timeStatus = QStringLiteral("All Day");
    event.startDate = eventDate;
    event.endDate = eventDate;
    const int eventId = saveFixtureEvent(workspace, event, &setupError);
    QVERIFY2(eventId > 0, qPrintable(setupError));

    QColor pickerResult;
    QColor lastPickerInitialColor;
    QString lastPickerTitle;
    int pickerCallCount = 0;
    CalendarPage directPage(
        &workspace.services,
        nullptr,
        [&](const QColor& initialColor, const QString& title)
        {
            ++pickerCallCount;
            lastPickerInitialColor = initialColor;
            lastPickerTitle = title;
            return pickerResult;
        }
        );
    CalendarPage* const page = &directPage;
    page->show();
    page->refresh();
    NavigationTabWidget* const tabs = upcomingTabs(*page);
    QVERIFY(tabs);
    tabs->setCurrentIndex(1);

    QTRY_VERIFY_WITH_TIMEOUT(
        upcomingEventTypeButton(*page, eventId),
        5000
        );
    QQuickItem* const root = calendarRoot(*page);
    QVERIFY(root);
    const auto displayedColor = [root, eventType]
    {
        return root->property("eventTypeColors")
            .toMap()
            .value(eventType)
            .toString();
    };
    const auto buttonStylesMatch = [page, eventType](const QColor& color)
    {
        const QList<QPushButton*> filters =
            eventTypeFilterButtons(*page, eventType);
        const QString expected = QStringLiteral("background-color: %1;")
            .arg(color.name(QColor::HexRgb));
        return filters.size() == 3
            && std::all_of(
                filters.cbegin(),
                filters.cend(),
                [&expected](QPushButton* button)
                {
                    return button->styleSheet().contains(expected);
                }
                );
    };
    const auto badgeStyleMatches =
        [page, eventId](const QColor& color)
    {
        QPushButton* const badge =
            upcomingEventTypeButton(*page, eventId);
        return badge
            && badge->styleSheet().contains(
                QStringLiteral("background-color: %1;")
                    .arg(color.name(QColor::HexRgb))
                );
    };

    QTRY_COMPARE_WITH_TIMEOUT(
        displayedColor(),
        originalColor.name(QColor::HexRgb),
        5000
        );
    QVERIFY(buttonStylesMatch(originalColor));
    QVERIFY(badgeStyleMatches(originalColor));

    pickerResult = QColor();
    QPushButton* const cancelButton =
        upcomingEventTypeButton(*page, eventId);
    QVERIFY(cancelButton);
    cancelButton->click();
    QCOMPARE(pickerCallCount, 1);
    QCOMPARE(lastPickerInitialColor, originalColor);
    QCOMPARE(lastPickerTitle, QStringLiteral("Choose Holiday Color"));
    QCOMPARE(calendarColorUpdateAuditCount(workspace, &sqlError), 0);
    QVERIFY2(sqlError.isEmpty(), qPrintable(sqlError));
    const auto afterCancel = settings->loadSetting(colorKey);
    QVERIFY(afterCancel);
    QCOMPARE(afterCancel->toString(), originalColor.name(QColor::HexRgb));

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(R"(
                CREATE TRIGGER fail_calendar_event_type_color_save
                BEFORE INSERT ON app_settings
                WHEN NEW.key = 'calendar/eventTypeColor/Holiday'
                BEGIN
                    SELECT RAISE(ABORT, 'forced calendar event color failure');
                END
            )"),
            &sqlError
            ),
        qPrintable(sqlError)
        );

    int warningCount = 0;
    QString warningError;
    QString warningTitle;
    QString warningText;
    bool warningIcon = false;
    QTimer::singleShot(
        0,
        page,
        [&]
        {
            auto* const warning = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget()
                );
            if (!warning)
            {
                warningError = QStringLiteral(
                    "Calendar color save failure did not show a warning dialog."
                    );
                if (QWidget* const modal = QApplication::activeModalWidget())
                {
                    modal->close();
                }
                return;
            }

            ++warningCount;
            warningTitle = warning->windowTitle();
            warningText = warning->text();
            warningIcon = warning->icon() == QMessageBox::Warning;
            warning->accept();
        }
        );
    pickerResult = attemptedColor;
    QPushButton* const failedButton =
        upcomingEventTypeButton(*page, eventId);
    QVERIFY(failedButton);
    failedButton->click();
    QCOMPARE(pickerCallCount, 2);
    QVERIFY2(warningError.isEmpty(), qPrintable(warningError));
    QCOMPARE(warningCount, 1);
    QCOMPARE(
        warningTitle,
        QStringLiteral("Unable to Save Preference")
        );
    QVERIFY(warningIcon);
    QVERIFY(warningText.contains(
        QStringLiteral("forced calendar event color failure")
        ));
    QCOMPARE(lastPickerInitialColor, originalColor);
    QCOMPARE(calendarColorUpdateAuditCount(workspace, &sqlError), 0);
    QVERIFY2(sqlError.isEmpty(), qPrintable(sqlError));
    const auto afterFailure = settings->loadSetting(colorKey);
    QVERIFY(afterFailure);
    QCOMPARE(afterFailure->toString(), originalColor.name(QColor::HexRgb));
    const auto unrelatedAfterFailure = settings->loadSetting(unrelatedKey);
    QVERIFY(unrelatedAfterFailure);
    QCOMPARE(unrelatedAfterFailure->toString(), QStringLiteral("preserved"));
    QCOMPARE(displayedColor(), originalColor.name(QColor::HexRgb));
    QVERIFY(buttonStylesMatch(originalColor));
    QVERIFY(badgeStyleMatches(originalColor));

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "DROP TRIGGER fail_calendar_event_type_color_save"
                ),
            &sqlError
            ),
        qPrintable(sqlError)
        );

    QPushButton* const retryButton =
        upcomingEventTypeButton(*page, eventId);
    QVERIFY(retryButton);
    retryButton->click();
    QCOMPARE(pickerCallCount, 3);
    QCOMPARE(lastPickerInitialColor, originalColor);
    QCOMPARE(calendarColorUpdateAuditCount(workspace, &sqlError), 1);
    QVERIFY2(sqlError.isEmpty(), qPrintable(sqlError));
    const auto afterRetry = settings->loadSetting(colorKey);
    QVERIFY(afterRetry);
    QCOMPARE(afterRetry->toString(), attemptedColor.name(QColor::HexRgb));
    const auto unrelatedAfterRetry = settings->loadSetting(unrelatedKey);
    QVERIFY(unrelatedAfterRetry);
    QCOMPARE(unrelatedAfterRetry->toString(), QStringLiteral("preserved"));
    QCOMPARE(displayedColor(), attemptedColor.name(QColor::HexRgb));
    QVERIFY(buttonStylesMatch(attemptedColor));
    QVERIFY(badgeStyleMatches(attemptedColor));
}

void CalendarPageEventMutationParityTests::
upcomingRangeFailuresOfferRetryAndRecover()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate today = QDate::currentDate();
    CalendarEvent event;
    event.title = QStringLiteral("Retry recovered calendar event");
    event.eventType = QStringLiteral("Holiday");
    event.startDate = today.addDays(2);
    event.endDate = event.startDate;
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    QVERIFY2(
        saveFixtureEvent(workspace, event, &setupError) > 0,
        qPrintable(setupError)
        );

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "ALTER TABLE calendar_events "
                "RENAME TO calendar_events_temporarily_unavailable"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );

    CalendarPage page(&workspace.services);
    CalendarEventCache* const cache =
        page.findChild<CalendarEventCache*>();
    NavigationTabWidget* const tabs = upcomingTabs(page);
    QLabel* const displayNotice = displayPreferencesNotice(page);
    QVERIFY(cache);
    QVERIFY(tabs);
    QVERIFY(displayNotice);
    QVERIFY(displayNotice->isHidden());

    const QDate visibleMonth(
        qMax(today.year(), 2026),
        today.year() < 2026 ? 1 : today.month(),
        1
        );
    const QDate visibleMonthEnd = visibleMonth.addMonths(1).addDays(-1);
    QTRY_VERIFY_WITH_TIMEOUT(
        !cache->rangeError(visibleMonth, visibleMonthEnd).isEmpty(),
        15000
        );
    QTRY_VERIFY_WITH_TIMEOUT(!cache->isLoading(), 5000);

    const QString currentMonthDiagnostic =
        cache->rangeError(visibleMonth, visibleMonthEnd);
    const QString currentMonthFailure =
        visibleUpcomingFailure(tabs->widget(0));
    QVERIFY(!currentMonthDiagnostic.isEmpty());
    QVERIFY(currentMonthFailure.contains(currentMonthDiagnostic));
    QVERIFY(upcomingRetryButton(tabs->widget(0)));
    QVERIFY(visibleUpcomingEmptyLabel(tabs->widget(0)).isEmpty());
    QVERIFY(!currentMonthFailure.contains(QStringLiteral("Loading events")));
    QVERIFY(!currentMonthFailure.contains(QStringLiteral("No upcoming events")));
    QVERIFY(displayNotice->isHidden());

    tabs->setCurrentIndex(1);
    const QDate next30End = today.addDays(30);
    QTRY_VERIFY_WITH_TIMEOUT(
        !cache->rangeError(today, next30End).isEmpty(),
        15000
        );
    QTRY_VERIFY_WITH_TIMEOUT(!cache->isLoading(), 5000);

    const QString next30Diagnostic = cache->rangeError(today, next30End);
    const QString next30Failure = visibleUpcomingFailure(tabs->currentWidget());
    QVERIFY(!next30Diagnostic.isEmpty());
    QVERIFY(next30Failure.contains(next30Diagnostic));
    QVERIFY(upcomingRetryButton(tabs->currentWidget()));
    QVERIFY(visibleUpcomingEmptyLabel(tabs->currentWidget()).isEmpty());
    QVERIFY(!next30Failure.contains(QStringLiteral("Loading events")));
    QVERIFY(!next30Failure.contains(QStringLiteral("No upcoming events")));
    QVERIFY(displayNotice->isHidden());

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "ALTER TABLE calendar_events_temporarily_unavailable "
                "RENAME TO calendar_events"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );

    QPushButton* const retryButton = upcomingRetryButton(tabs->currentWidget());
    QVERIFY(retryButton);
    retryButton->click();

    QTRY_VERIFY_WITH_TIMEOUT(
        visibleUpcomingFailure(tabs->currentWidget()).isEmpty(),
        15000
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        !visibleUpcomingRows(tabs->currentWidget()).isEmpty(),
        15000
        );
    QVERIFY(!upcomingRetryButton(tabs->currentWidget()));
    QVERIFY(
        visibleUpcomingRows(tabs->currentWidget())
            .join(QString())
            .contains(event.title)
        );
    QVERIFY(cache->rangeError(today, next30End).isEmpty());
    QVERIFY(displayNotice->isHidden());
}

void CalendarPageEventMutationParityTests::
nextTenLookupFailuresOfferRetryAndRecover()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );
    QVERIFY2(
        removePreservedFixtureEvent(workspace, &setupError),
        qPrintable(setupError)
        );

    const QDate today = QDate::currentDate();
    CalendarEvent event;
    event.title = QStringLiteral("Retry recovered Next 10 event");
    event.eventType = QStringLiteral("Holiday");
    event.startDate = today.addMonths(1).addDays(2);
    event.endDate = event.startDate;
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    QVERIFY2(
        saveFixtureEvent(workspace, event, &setupError) > 0,
        qPrintable(setupError)
        );

    CalendarPage page(&workspace.services);
    CalendarEventCache* const cache =
        page.findChild<CalendarEventCache*>();
    NavigationTabWidget* const tabs = upcomingTabs(page);
    QVERIFY(cache);
    QVERIFY(tabs);

    const QDate visibleMonth(
        qMax(today.year(), 2026),
        today.year() < 2026 ? 1 : today.month(),
        1
        );
    const QDate visibleMonthEnd = visibleMonth.addMonths(1).addDays(-1);
    QTRY_VERIFY_WITH_TIMEOUT(
        cache->isRangeLoaded(visibleMonth, visibleMonthEnd),
        15000
        );
    QTRY_VERIFY_WITH_TIMEOUT(!cache->isLoading(), 5000);

    const QDate currentSearchEnd =
        QDate(today.year(), today.month(), 1)
            .addMonths(1)
            .addDays(-1);
    cache->setRetainedRanges({
        {visibleMonth, visibleMonthEnd},
        {today, currentSearchEnd}
    });
    cache->requestRange(today, currentSearchEnd);
    QTRY_VERIFY_WITH_TIMEOUT(
        cache->isRangeLoaded(today, currentSearchEnd),
        15000
        );
    QTRY_VERIFY_WITH_TIMEOUT(!cache->isLoading(), 5000);

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "ALTER TABLE calendar_events "
                "RENAME TO calendar_events_temporarily_unavailable"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );

    tabs->setCurrentIndex(2);
    const QDate failedLookupAfter = currentSearchEnd.addDays(1);
    QTRY_VERIFY_WITH_TIMEOUT(
        !cache->nextEventError(failedLookupAfter).isEmpty(),
        15000
        );
    QTRY_VERIFY_WITH_TIMEOUT(!cache->isLoading(), 5000);

    const QString nextTenDiagnostic =
        cache->nextEventError(failedLookupAfter);
    const QString nextTenFailure =
        visibleUpcomingFailure(tabs->currentWidget());
    QVERIFY(!nextTenDiagnostic.isEmpty());
    QVERIFY(nextTenFailure.contains(nextTenDiagnostic));
    QVERIFY(upcomingRetryButton(tabs->currentWidget()));
    QVERIFY(visibleUpcomingRows(tabs->currentWidget()).isEmpty());
    QVERIFY(visibleUpcomingEmptyLabel(tabs->currentWidget()).isEmpty());
    QVERIFY(!nextTenFailure.contains(QStringLiteral("Loading events")));
    QVERIFY(!nextTenFailure.contains(QStringLiteral("No upcoming events")));
    QVERIFY(!page.runtimeMetrics().nextTenLookupPending);

    QVERIFY2(
        executeWorkspaceSql(
            workspace,
            QStringLiteral(
                "ALTER TABLE calendar_events_temporarily_unavailable "
                "RENAME TO calendar_events"
                ),
            &setupError
            ),
        qPrintable(setupError)
        );

    QPushButton* const retryButton =
        upcomingRetryButton(tabs->currentWidget());
    QVERIFY(retryButton);
    retryButton->click();

    QTRY_VERIFY_WITH_TIMEOUT(
        cache->nextEventError(failedLookupAfter).isEmpty(),
        15000
        );
    QTRY_VERIFY_WITH_TIMEOUT(
        visibleUpcomingRows(tabs->currentWidget())
            .join(QString())
            .contains(event.title),
        15000
        );
    QVERIFY(visibleUpcomingFailure(tabs->currentWidget()).isEmpty());
    QVERIFY(!upcomingRetryButton(tabs->currentWidget()));
}

QTEST_MAIN(CalendarPageEventMutationParityTests)

#include "calendar_page_event_mutation_parity_tests.moc"
