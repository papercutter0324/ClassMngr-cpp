#include "calendar_event_dialog.h"

#include "core/fontmanager.h"
#include "ui/constants/gui_constants.h"

#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTimeEdit>
#include <QVBoxLayout>

CalendarEventDialog::CalendarEventDialog(
    const CalendarEvent& event,
    bool existingEvent,
    QWidget* parent
    )
    : QDialog(parent)
    , m_event(event)
    , m_existingEvent(existingEvent)
{
    buildUi();
    loadEvent();
}

CalendarEvent CalendarEventDialog::eventData() const
{
    CalendarEvent event =
        m_event;

    event.title =
        m_titleEdit->text().trimmed();
    event.startDate =
        m_startDateEdit->date();
    event.startTime =
        m_startTimeEdit->time();
    event.endDate =
        m_endDateEdit->date();
    event.endTime =
        m_endTimeEdit->time();

    return event;
}

bool CalendarEventDialog::deleteRequested() const
{
    return m_deleteRequested;
}

void CalendarEventDialog::accept()
{
    const CalendarEvent event =
        eventData();

    if (event.title.trimmed().isEmpty())
    {
        QMessageBox::warning(
            this,
            tr("Missing Title"),
            tr("Enter a title for the calendar event.")
            );
        return;
    }

    const QDateTime start(
        event.startDate,
        event.startTime
        );
    const QDateTime end(
        event.endDate,
        event.endTime
        );

    if (end < start)
    {
        QMessageBox::warning(
            this,
            tr("Invalid Time Range"),
            tr("The event end must be after the event start.")
            );
        return;
    }

    QDialog::accept();
}

void CalendarEventDialog::requestDelete()
{
    m_deleteRequested = true;
    QDialog::accept();
}

void CalendarEventDialog::buildUi()
{
    setWindowTitle(
        m_existingEvent
            ? tr("Edit Calendar Event")
            : tr("Add Calendar Event")
        );

    auto* mainLayout =
        new QVBoxLayout(this);

    mainLayout->setContentsMargins(
        20,
        20,
        20,
        20
        );
    mainLayout->setSpacing(16);

    auto* title =
        new QLabel(
            windowTitle(),
            this
            );
    title->setObjectName("pageTitle");
    title->setFont(
        FontManager::getUiFont(
            16,
            QFont::DemiBold
            )
        );
    mainLayout->addWidget(title);

    auto* form =
        new QFormLayout;

    form->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );
    form->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    m_titleEdit =
        new QLineEdit(this);
    m_startDateEdit =
        new QDateEdit(this);
    m_startTimeEdit =
        new QTimeEdit(this);
    m_endDateEdit =
        new QDateEdit(this);
    m_endTimeEdit =
        new QTimeEdit(this);

    for (auto* edit : {m_startDateEdit, m_endDateEdit})
    {
        edit->setCalendarPopup(true);
        edit->setDisplayFormat(
            QStringLiteral("yyyy-MM-dd")
            );
    }

    for (auto* edit : {m_startTimeEdit, m_endTimeEdit})
    {
        edit->setDisplayFormat(
            QStringLiteral("HH:mm")
            );
    }

    form->addRow(
        tr("Title"),
        m_titleEdit
        );
    form->addRow(
        tr("Start Date"),
        m_startDateEdit
        );
    form->addRow(
        tr("Start Time"),
        m_startTimeEdit
        );
    form->addRow(
        tr("End Date"),
        m_endDateEdit
        );
    form->addRow(
        tr("End Time"),
        m_endTimeEdit
        );

    mainLayout->addLayout(form);

    m_buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Save
            | QDialogButtonBox::Cancel,
            this
            );

    if (m_existingEvent)
    {
        m_deleteButton =
            m_buttons->addButton(
                tr("Delete"),
                QDialogButtonBox::DestructiveRole
                );
        connect(
            m_deleteButton,
            &QPushButton::clicked,
            this,
            &CalendarEventDialog::requestDelete
            );
    }

    connect(
        m_buttons,
        &QDialogButtonBox::accepted,
        this,
        &CalendarEventDialog::accept
        );
    connect(
        m_buttons,
        &QDialogButtonBox::rejected,
        this,
        &CalendarEventDialog::reject
        );

    mainLayout->addWidget(m_buttons);
}

void CalendarEventDialog::loadEvent()
{
    const QDate today =
        QDate::currentDate();

    m_titleEdit->setText(
        m_event.title
        );
    m_startDateEdit->setDate(
        m_event.startDate.isValid()
            ? m_event.startDate
            : today
        );
    m_startTimeEdit->setTime(
        m_event.startTime.isValid()
            ? m_event.startTime
            : QTime(9, 0)
        );
    m_endDateEdit->setDate(
        m_event.endDate.isValid()
            ? m_event.endDate
            : m_startDateEdit->date()
        );
    m_endTimeEdit->setTime(
        m_event.endTime.isValid()
            ? m_event.endTime
            : QTime(10, 0)
        );
}
