#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "domain/models/calendar_event.h"
#include "features/calendar/ui/calendar_event_dialog.h"
#include "features/calendar/ui/calendar_event_model.h"
#include "features/calendar/ui/calendar_page.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDateEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QQmlContext>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWidget>
#include <QRadioButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimeEdit>
#include <QTimer>
#include <QWidget>
#include <QUuid>
#include <QtTest/QtTest>

#include <functional>

namespace
{

struct CalendarWorkspace final
{
    QTemporaryDir directory;
    ApplicationServices services;
    int preservedEventId = -1;
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

    const Status opened = workspace.services.openDatabase(
        databasePath(workspace.directory)
        );
    if (!opened)
    {
        if (error)
        {
            *error = opened.error();
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
            *error = saved.error();
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
        row.append(query.value(0).toInt());
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
    const int id,
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

int eventIdForTitle(ApplicationServices& services, const QString& title)
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
    return query.value(0).toInt();
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

}

class CalendarPageEventMutationParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void ordinaryCreateEditAndDeleteUseCalendarPageSignals();
    void repeatSeriesSuffixEditUsesCalendarPageSignalAndRefreshesProjection();
    void monthGridCellsMatchCalendarEventModelDateProjection();
};

void CalendarPageEventMutationParityTests::
ordinaryCreateEditAndDeleteUseCalendarPageSignals()
{
    CalendarWorkspace workspace;
    QString setupError;
    QVERIFY2(
        initializeWorkspace(workspace, &setupError),
        qPrintable(setupError)
        );

    CalendarPage page(&workspace.services);
    QQuickItem* const root = calendarRoot(page);
    QVERIFY(root);
    QVERIFY(
        root->metaObject()->indexOfSignal("dayActivated(int,int,int)") >= 0
        );
    QVERIFY(
        root->metaObject()->indexOfSignal("eventActivated(int)") >= 0
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
    const int createdEventId = eventIdForTitle(
        workspace.services,
        createdValues.title
        );
    QVERIFY(createdEventId > 0);
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
                    Q_ARG(int, createdEventId)
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
                    Q_ARG(int, createdEventId)
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
            )
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
                setupError = saved.error();
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
        root->metaObject()->indexOfSignal("eventActivated(int)") >= 0
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
                    Q_ARG(int, selectedId)
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
        setupError = multiDaySaved.error();
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
        setupError = holidaySaved.error();
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

QTEST_MAIN(CalendarPageEventMutationParityTests)

#include "calendar_page_event_mutation_parity_tests.moc"
