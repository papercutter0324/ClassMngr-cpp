#include "calendar_event_dialog.h"

#include "core/fontmanager.h"
#include "ui/constants/gui_constants.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpacerItem>
#include <QStringList>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace
{

constexpr int DateTimeFieldWidth = 140;

QStringList eventTypes()
{
    return {
        QStringLiteral("Vacation"),
        QStringLiteral("Holiday"),
        QStringLiteral("Workshop"),
        QStringLiteral("CM"),
        QStringLiteral("Meeting"),
        QStringLiteral("Other")
    };
}

QString normalizedEventType(
    const QString& eventType
    )
{
    const QString trimmed =
        eventType.trimmed();

    return eventTypes().contains(trimmed)
        ? trimmed
        : QStringLiteral("Other");
}

} // namespace

CalendarEventDialog::CalendarEventDialog(
    const CalendarEvent& event,
    bool existingEvent,
    bool use24h,
    QWidget* parent
    )
    : QDialog(parent)
    , m_event(event)
    , m_existingEvent(existingEvent)
    , m_use24h(use24h)
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
    event.allDay =
        m_allDayCheck
        && m_allDayCheck->isChecked();

    if (event.allDay)
    {
        event.startTime =
            QTime(0, 0);
        event.endTime =
            QTime(23, 59);
    }

    event.eventType =
        QStringLiteral("Other");

    if (m_eventTypeGroup)
    {
        if (const auto* checkedButton =
                m_eventTypeGroup->checkedButton())
        {
            event.eventType =
                checkedButton
                    ->property("eventType")
                    .toString();
        }
    }

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
            ? tr("Edit Event")
            : tr("Add Event")
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
    title->setAlignment(Qt::AlignCenter);
    title->setFont(
        FontManager::getUiFont(
            16,
            QFont::DemiBold
            )
        );
    mainLayout->addWidget(title);

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
    m_allDayCheck =
        new QCheckBox(
            tr("All Day Event"),
            this
            );
    m_eventTypeGroup =
        new QButtonGroup(this);

    for (auto* edit : {m_startDateEdit, m_endDateEdit})
    {
        edit->setFixedWidth(DateTimeFieldWidth);
        edit->setCalendarPopup(true);
        edit->setDisplayFormat(
            QStringLiteral("yyyy-MM-dd")
            );
    }

    for (auto* edit : {m_startTimeEdit, m_endTimeEdit})
    {
        edit->setFixedWidth(DateTimeFieldWidth);
        edit->setDisplayFormat(
            m_use24h
                ? QStringLiteral("HH:mm")
                : QStringLiteral("h:mm AP")
            );
    }

    auto* fieldsLayout =
        new QVBoxLayout;
    fieldsLayout->setSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    auto createLabel =
        [this](const QString& text)
        {
            auto* label =
                new QLabel(text, this);
            label->setFont(
                FontManager::getUiFont(
                    10,
                    QFont::Medium
                    )
                );

            return label;
        };

    auto createFieldGroup =
        [this, createLabel](
            const QString& labelText,
            QWidget* field
            )
        {
            auto* group =
                new QVBoxLayout;
            group->setSpacing(4);

            group->addWidget(
                createLabel(labelText)
                );
            group->addWidget(field);

            return group;
        };

    fieldsLayout->addWidget(
        createLabel(
            tr("Title")
            )
        );
    fieldsLayout->addWidget(
        m_titleEdit
        );

    auto* dateLayout =
        new QHBoxLayout;
    dateLayout->setSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );
    dateLayout->addLayout(
        createFieldGroup(
            tr("Start Date"),
            m_startDateEdit
            )
        );
    dateLayout->addSpacerItem(
        new QSpacerItem(
            32,
            0,
            QSizePolicy::Expanding,
            QSizePolicy::Minimum
            )
        );
    dateLayout->addLayout(
        createFieldGroup(
            tr("End Date"),
            m_endDateEdit
            )
        );

    auto* timeLayout =
        new QGridLayout;
    timeLayout->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );
    timeLayout->setVerticalSpacing(4);

    timeLayout->addWidget(
        createLabel(
            tr("Start Time")
            ),
        0,
        0
        );
    timeLayout->addWidget(
        createLabel(
            tr("End Time")
            ),
        0,
        2
        );

    timeLayout->addWidget(
        m_startTimeEdit,
        1,
        0
        );
    timeLayout->addWidget(
        m_endTimeEdit,
        1,
        2
        );

    timeLayout->addWidget(
        m_allDayCheck,
        2,
        0,
        1,
        3
        );

    timeLayout->setColumnMinimumWidth(
        1,
        32
        );
    timeLayout->setColumnStretch(
        1,
        1
        );

    fieldsLayout->addSpacing(6);
    fieldsLayout->addLayout(dateLayout);
    fieldsLayout->addSpacing(6);
    fieldsLayout->addLayout(timeLayout);

    mainLayout->addLayout(fieldsLayout);
    mainLayout->addStretch(1);

    mainLayout->addWidget(
        createLabel(
            tr("Event Type")
            )
        );

    auto* eventTypeLayout =
        new QGridLayout;
    eventTypeLayout->setHorizontalSpacing(24);
    eventTypeLayout->setVerticalSpacing(8);

    int eventTypeIndex = 0;
    for (const QString& eventType : eventTypes())
    {
        auto* button =
            new QRadioButton(
                eventType,
                this
                );
        button->setProperty(
            "eventType",
            eventType
            );
        m_eventTypeGroup->addButton(button);
        eventTypeLayout->addWidget(
            button,
            eventTypeIndex / 3,
            eventTypeIndex % 3
            );
        ++eventTypeIndex;
    }

    mainLayout->addLayout(eventTypeLayout);

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
    connect(
        m_allDayCheck,
        &QCheckBox::toggled,
        this,
        &CalendarEventDialog::updateTimeFieldAvailability
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
    m_allDayCheck->setChecked(
        m_event.allDay
        );
    updateTimeFieldAvailability();

    const QString selectedEventType =
        normalizedEventType(
            m_event.eventType
            );

    for (auto* button : m_eventTypeGroup->buttons())
    {
        if (
            button
                ->property("eventType")
                .toString()
            == selectedEventType
            )
        {
            button->setChecked(true);
            break;
        }
    }
}

void CalendarEventDialog::updateTimeFieldAvailability()
{
    const bool allDay =
        m_allDayCheck
        && m_allDayCheck->isChecked();

    if (allDay)
    {
        m_startTimeEdit->setTime(
            QTime(0, 0)
            );
        m_endTimeEdit->setTime(
            QTime(23, 59)
            );
    }

    m_startTimeEdit->setEnabled(
        !allDay
        );
    m_endTimeEdit->setEnabled(
        !allDay
        );
}
