#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/calendar_event_repository.h"
#include "domain/models/calendar_event.h"
#include "features/calendar/ui/calendar_event_dialog.h"
#include "features/calendar/ui/calendar_page.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
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
    const QString& endTime
    )
{
    QJsonArray row;
    row.append(id);
    row.append(title);
    row.append(eventType);
    row.append(QStringLiteral("Timed"));
    row.append(QJsonValue(QJsonValue::Null));
    row.append(0);
    row.append(startDate);
    row.append(startTime);
    row.append(endDate);
    row.append(endTime);
    return QString::fromUtf8(
        QJsonDocument(row).toJson(QJsonDocument::Compact)
        );
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

bool applyDialogValues(
    CalendarEventDialog* dialog,
    const EventDialogValues& values,
    const bool deleting,
    QString* error
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
    save->click();
    return true;
}

bool activateAndHandleDialog(
    CalendarPage& page,
    const std::function<bool(QQuickItem*)>& activate,
    const EventDialogValues& values,
    const bool deleting,
    QString* error
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
                &interactionError
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

}

class CalendarPageEventMutationParityTests final : public QObject
{
    Q_OBJECT

private slots:
    void ordinaryCreateEditAndDeleteUseCalendarPageSignals();
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

QTEST_MAIN(CalendarPageEventMutationParityTests)

#include "calendar_page_event_mutation_parity_tests.moc"
