#include "calendar_event_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "core/fontmanager.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/text_fit_dialog_button_box.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace
{
constexpr int DateTimeFieldWidth = 140;
constexpr int RepeatFieldWidth = 180;
constexpr int RepeatOptionToFieldsSpacing = 24;
constexpr int MaximumRepeatOccurrences = 366;

int repeatIntervalDays(
    CalendarEventRepeatFrequency frequency
    )
{
    switch (frequency)
    {
    case CalendarEventRepeatFrequency::Daily:
        return 1;

    case CalendarEventRepeatFrequency::Weekly:
        return 7;

    case CalendarEventRepeatFrequency::Monthly:
        return 31;
    }

    return 7;
}

int estimatedRepeatOccurrences(
    const QDate& startDate,
    const QDate& untilDate,
    CalendarEventRepeatFrequency frequency
    )
{
    if (
        !startDate.isValid()
        || !untilDate.isValid()
        || untilDate < startDate
        )
    {
        return 0;
    }

    const int intervalDays =
        qMax(
            1,
            repeatIntervalDays(frequency)
            );

    return (startDate.daysTo(untilDate) / intervalDays) + 1;
}

QDate finalMatchingWeekdayInYear(
    const QDate& date
    )
{
    if (!date.isValid())
    {
        return QDate::currentDate();
    }

    const QDate finalDay(
        date.year(),
        12,
        31
        );
    const int daysBack =
        (finalDay.dayOfWeek() - date.dayOfWeek() + 7) % 7;

    return finalDay.addDays(
        -daysBack
        );
}

bool isRepeatSeriesEvent(
    const CalendarEvent& event
    )
{
    return !event.repeatSeriesId.trimmed().isEmpty();
}
}

CalendarEventDialog::CalendarEventDialog(
    const CalendarEvent& event,
    bool existingEvent,
    bool use24h,
    QWidget* parent
    )
    : DialogShell(QStringLiteral("calendarEvent"), parent)
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
    event.timeStatus =
        !event.allDay
        && m_unconfirmedTimeCheck
        && m_unconfirmedTimeCheck->isChecked()
            ? QStringLiteral("Unconfirmed")
            : QStringLiteral("Timed");

    if (event.allDay)
    {
        event.startTime =
            QTime(0, 0);
        event.endTime =
            QTime(23, 59);
    }
    else if (event.timeStatus == QStringLiteral("Unconfirmed"))
    {
        event.startTime =
            QTime();
        event.endTime =
            QTime();
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

bool CalendarEventDialog::repeatEnabled() const
{
    return !m_existingEvent
        && m_repeatCheck
        && m_repeatCheck->isChecked();
}

CalendarEventRepeatFrequency CalendarEventDialog::repeatFrequency() const
{
    if (!m_repeatFrequencyCombo)
    {
        return CalendarEventRepeatFrequency::Weekly;
    }

    return static_cast<CalendarEventRepeatFrequency>(
        m_repeatFrequencyCombo->currentData().toInt()
        );
}

QDate CalendarEventDialog::repeatUntilDate() const
{
    return m_repeatUntilDateEdit
        ? m_repeatUntilDateEdit->date()
        : QDate();
}

CalendarEventSeriesEditScope CalendarEventDialog::seriesEditScope() const
{
    if (!m_seriesScopeGroup)
    {
        return CalendarEventSeriesEditScope::ThisEventOnly;
    }

    if (m_seriesScopeGroup->checkedId() < 0)
    {
        return CalendarEventSeriesEditScope::ThisEventOnly;
    }

    return static_cast<CalendarEventSeriesEditScope>(
        m_seriesScopeGroup->checkedId()
        );
}

void CalendarEventDialog::accept()
{
    const CalendarEvent event =
        eventData();

    if (event.title.trimmed().isEmpty())
    {
        DialogServices::showWarning(
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
        DialogServices::showWarning(
            this,
            tr("Invalid Time Range"),
            tr("The event end must be after the event start.")
            );
        return;
    }

    if (repeatEnabled())
    {
        const QDate untilDate =
            repeatUntilDate();

        if (
            !untilDate.isValid()
            || untilDate < event.startDate
            )
        {
            DialogServices::showWarning(
                this,
                tr("Invalid Repeat Range"),
                tr("The repeat end date must be on or after the event start date.")
                );
            return;
        }

        const int occurrenceCount =
            estimatedRepeatOccurrences(
                event.startDate,
                untilDate,
                repeatFrequency()
                );

        if (occurrenceCount > MaximumRepeatOccurrences)
        {
            DialogServices::showWarning(
                this,
                tr("Too Many Events"),
                tr("Choose a shorter repeat range. Repeating events can create up to %1 events at once.")
                    .arg(MaximumRepeatOccurrences)
                );
            return;
        }
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
    setSizeGripEnabled(false);

    auto* mainLayout =
        contentLayout();
    mainLayout->setSizeConstraint(
        QLayout::SetFixedSize
        );

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
    m_unconfirmedTimeCheck =
        new QCheckBox(
            tr("Unconfirmed Time"),
            this
            );
    m_repeatCheck =
        new QCheckBox(
            tr("Repeating"),
            this
            );
    m_repeatFrequencyCombo =
        new QComboBox(this);
    m_repeatUntilDateEdit =
        new QDateEdit(this);
    m_seriesScopeGroup =
        new QButtonGroup(this);
    m_eventTypeGroup =
        new QButtonGroup(this);

    for (auto* edit : {m_startDateEdit, m_endDateEdit})
    {
        edit->setCalendarPopup(true);
        edit->setDisplayFormat(
            QStringLiteral("yyyy-MM-dd")
            );
        WidgetSizing::installTextAwareFieldWidth(
            edit,
            DateTimeFieldWidth,
            QSizePolicy::Maximum,
            true
            );
    }

    m_repeatFrequencyCombo->addItem(
        tr("Daily"),
        static_cast<int>(CalendarEventRepeatFrequency::Daily)
        );
    m_repeatFrequencyCombo->addItem(
        tr("Weekly"),
        static_cast<int>(CalendarEventRepeatFrequency::Weekly)
        );
    m_repeatFrequencyCombo->addItem(
        tr("Monthly"),
        static_cast<int>(CalendarEventRepeatFrequency::Monthly)
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_repeatFrequencyCombo,
        RepeatFieldWidth,
        QSizePolicy::Maximum,
        true
        );

    m_repeatUntilDateEdit->setCalendarPopup(true);
    m_repeatUntilDateEdit->setDisplayFormat(
        QStringLiteral("yyyy-MM-dd")
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_repeatUntilDateEdit,
        RepeatFieldWidth,
        QSizePolicy::Maximum,
        true
        );

    if (m_existingEvent)
    {
        m_repeatCheck->hide();
        m_repeatFrequencyCombo->hide();
        m_repeatUntilDateEdit->hide();
    }

    for (auto* edit : {m_startTimeEdit, m_endTimeEdit})
    {
        edit->setDisplayFormat(
            m_use24h
                ? QStringLiteral("HH:mm")
                : QStringLiteral("h:mm AP")
            );
        WidgetSizing::installTextAwareFieldWidth(
            edit,
            DateTimeFieldWidth,
            QSizePolicy::Maximum,
            true
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
        m_unconfirmedTimeCheck,
        2,
        0,
        1,
        1
        );
    timeLayout->addWidget(
        m_allDayCheck,
        2,
        2,
        1,
        1
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

    if (!m_existingEvent)
    {
        auto* repeatLayout =
            new QGridLayout;
        repeatLayout->setHorizontalSpacing(
            UiConstants::ClassInfo::Form::HorizontalSpacing
            );
        repeatLayout->setVerticalSpacing(4);

        repeatLayout->addWidget(
            m_repeatCheck,
            0,
            0,
            1,
            3
            );
        repeatLayout->addItem(
            new QSpacerItem(
                0,
                qMax(
                    0,
                    RepeatOptionToFieldsSpacing
                        - (repeatLayout->verticalSpacing() * 2)
                    ),
                QSizePolicy::Minimum,
                QSizePolicy::Fixed
                ),
            1,
            0,
            1,
            3
            );
        repeatLayout->addWidget(
            createLabel(
                tr("Repeats")
                ),
            2,
            0
            );
        repeatLayout->addWidget(
            createLabel(
                tr("Until")
                ),
            2,
            2
            );
        repeatLayout->addWidget(
            m_repeatFrequencyCombo,
            3,
            0
            );
        repeatLayout->addWidget(
            m_repeatUntilDateEdit,
            3,
            2
            );
        repeatLayout->setColumnMinimumWidth(
            1,
            32
            );
        repeatLayout->setColumnStretch(
            1,
            1
            );

        fieldsLayout->addSpacing(6);
        fieldsLayout->addLayout(repeatLayout);
    }

    if (
        m_existingEvent
        && isRepeatSeriesEvent(m_event)
        )
    {
        auto* seriesScopeLayout =
            new QVBoxLayout;
        seriesScopeLayout->setSpacing(4);
        seriesScopeLayout->addWidget(
            createLabel(
                tr("Apply To")
                )
            );

        auto* thisEventOnlyButton =
            new QRadioButton(
                tr("This event only"),
                this
                );
        auto* thisAndFollowingButton =
            new QRadioButton(
                tr("This and following events"),
                this
                );

        m_seriesScopeGroup->addButton(
            thisEventOnlyButton,
            static_cast<int>(
                CalendarEventSeriesEditScope::ThisEventOnly
                )
            );
        m_seriesScopeGroup->addButton(
            thisAndFollowingButton,
            static_cast<int>(
                CalendarEventSeriesEditScope::ThisAndFollowingEvents
                )
            );

        thisEventOnlyButton->setChecked(true);

        seriesScopeLayout->addWidget(thisEventOnlyButton);
        seriesScopeLayout->addWidget(thisAndFollowingButton);

        fieldsLayout->addSpacing(6);
        fieldsLayout->addLayout(seriesScopeLayout);
    }

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
    for (const QString& eventType : calendarEventTypes())
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

    m_buttons = addButtonBox(
        QDialogButtonBox::Save
        | QDialogButtonBox::Cancel
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
        m_allDayCheck,
        &QCheckBox::toggled,
        this,
        &CalendarEventDialog::updateTimeFieldAvailability
        );
    connect(
        m_unconfirmedTimeCheck,
        &QCheckBox::toggled,
        this,
        &CalendarEventDialog::updateTimeFieldAvailability
        );
    connect(
        m_repeatCheck,
        &QCheckBox::toggled,
        this,
        &CalendarEventDialog::updateRepeatFieldAvailability
        );
    connect(
        m_startDateEdit,
        &QDateEdit::dateChanged,
        this,
        [this](const QDate& date)
        {
            if (!m_repeatUntilDateEdit)
            {
                return;
            }

            if (
                !repeatEnabled()
                || m_repeatUntilDateEdit->date() < date
                )
            {
                m_repeatUntilDateEdit->setDate(
                    finalMatchingWeekdayInYear(date)
                    );
            }
        }
        );
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
    m_unconfirmedTimeCheck->setChecked(
        !m_event.allDay
        && normalizedCalendarEventTimeStatus(m_event.timeStatus)
            == QStringLiteral("Unconfirmed")
        );
    m_repeatFrequencyCombo->setCurrentIndex(1);
    m_repeatUntilDateEdit->setDate(
        finalMatchingWeekdayInYear(
            m_startDateEdit->date()
            )
        );
    m_repeatCheck->setChecked(false);
    updateTimeFieldAvailability();
    updateRepeatFieldAvailability();

    const QString selectedEventType =
        normalizedCalendarEventType(
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
    const bool unconfirmed =
        m_unconfirmedTimeCheck
        && m_unconfirmedTimeCheck->isChecked();

    if (allDay && unconfirmed)
    {
        const auto* senderObject =
            sender();

        if (senderObject == m_allDayCheck)
        {
            const QSignalBlocker blocker(m_unconfirmedTimeCheck);
            m_unconfirmedTimeCheck->setChecked(false);
        }
        else
        {
            const QSignalBlocker blocker(m_allDayCheck);
            m_allDayCheck->setChecked(false);
        }
    }

    const bool effectiveAllDay =
        m_allDayCheck
        && m_allDayCheck->isChecked();
    const bool effectiveUnconfirmed =
        m_unconfirmedTimeCheck
        && m_unconfirmedTimeCheck->isChecked();

    if (effectiveAllDay)
    {
        m_startTimeEdit->setTime(
            QTime(0, 0)
            );
        m_endTimeEdit->setTime(
            QTime(23, 59)
            );
    }

    m_startTimeEdit->setEnabled(
        !effectiveAllDay
        && !effectiveUnconfirmed
        );
    m_endTimeEdit->setEnabled(
        !effectiveAllDay
        && !effectiveUnconfirmed
        );
}

void CalendarEventDialog::updateRepeatFieldAvailability()
{
    const bool repeat =
        repeatEnabled();

    if (m_repeatFrequencyCombo)
    {
        m_repeatFrequencyCombo->setEnabled(repeat);
    }

    if (m_repeatUntilDateEdit)
    {
        m_repeatUntilDateEdit->setEnabled(repeat);
        m_repeatUntilDateEdit->setMinimumDate(
            m_startDateEdit
                ? m_startDateEdit->date()
                : QDate::currentDate()
            );
    }
}
