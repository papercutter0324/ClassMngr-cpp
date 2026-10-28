#include "next/ui/calendar_event_dialog.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace ClassMngr::Next::Ui
{
namespace
{

QString eventDialogText(const char* source)
{
    return QCoreApplication::translate("CalendarEventDialog", source);
}

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString initialText(const std::optional<std::string>& value, const char* fallback)
{
    return value ? fromUtf8(*value) : QString::fromLatin1(fallback);
}

QDate parseDate(const std::string& value)
{
    const QDate date = QDate::fromString(fromUtf8(value), Qt::ISODate);
    return date.isValid() ? date : QDate::currentDate();
}

QTime parseTime(const std::optional<std::string>& value, const QTime fallback)
{
    if (!value)
    {
        return fallback;
    }
    const QString text = fromUtf8(*value);
    const QTime time = QTime::fromString(text, QStringLiteral("HH:mm"));
    return time.isValid() && time.toString(QStringLiteral("HH:mm")) == text
        ? time
        : fallback;
}

} // namespace

CalendarEventDialog::CalendarEventDialog(
    Application::CalendarEventEditDraft draft,
    const bool existingEvent,
    QWidget* parent,
    const CalendarEventSeriesEditScope initialScope
    )
    : QDialog(parent),
      m_draft(std::move(draft)),
      m_existingEvent(existingEvent)
{
    setObjectName(QStringLiteral("nextCalendarEventDialog"));
    setWindowTitle(existingEvent ? eventDialogText("Edit Calendar Event")
                                 : eventDialogText("New Calendar Event"));
    setModal(true);
    setMinimumWidth(440);

    auto* outer = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_title = new QLineEdit(this);
    m_title->setObjectName(QStringLiteral("calendarEventTitle"));
    m_title->setText(fromUtf8(m_draft.title));
    m_title->setMaxLength(static_cast<int>(
        Application::kCalendarEventEditDraftMaxTitleLength));
    form->addRow(eventDialogText("Title"), m_title);

    m_startDate = new QDateEdit(this);
    m_startDate->setObjectName(QStringLiteral("calendarEventStartDate"));
    m_startDate->setCalendarPopup(true);
    m_startDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_startDate->setDate(parseDate(m_draft.startDate));
    form->addRow(eventDialogText("Start date"), m_startDate);

    m_startTime = new QTimeEdit(this);
    m_startTime->setObjectName(QStringLiteral("calendarEventStartTime"));
    m_startTime->setDisplayFormat(QStringLiteral("HH:mm"));
    m_startTime->setTime(parseTime(m_draft.startTime, QTime(9, 0)));
    form->addRow(eventDialogText("Start time"), m_startTime);

    m_endDate = new QDateEdit(this);
    m_endDate->setObjectName(QStringLiteral("calendarEventEndDate"));
    m_endDate->setCalendarPopup(true);
    m_endDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_endDate->setDate(parseDate(m_draft.endDate));
    form->addRow(eventDialogText("End date"), m_endDate);

    m_endTime = new QTimeEdit(this);
    m_endTime->setObjectName(QStringLiteral("calendarEventEndTime"));
    m_endTime->setDisplayFormat(QStringLiteral("HH:mm"));
    m_endTime->setTime(parseTime(m_draft.endTime, QTime(10, 0)));
    form->addRow(eventDialogText("End time"), m_endTime);

    m_allDay = new QCheckBox(eventDialogText("All day"), this);
    m_allDay->setObjectName(QStringLiteral("calendarEventAllDay"));
    m_allDay->setChecked(m_draft.allDay);
    form->addRow(QString(), m_allDay);

    m_timeStatus = new QComboBox(this);
    m_timeStatus->setObjectName(QStringLiteral("calendarEventTimeStatus"));
    m_timeStatus->addItem(eventDialogText("Timed"),
        static_cast<int>(Domain::CalendarEventTimeStatus::Timed));
    m_timeStatus->addItem(eventDialogText("Time unknown"),
        static_cast<int>(Domain::CalendarEventTimeStatus::Unknown));
    m_timeStatus->addItem(eventDialogText("Time unconfirmed"),
        static_cast<int>(Domain::CalendarEventTimeStatus::Unconfirmed));
    m_timeStatus->setCurrentIndex(m_timeStatus->findData(
        static_cast<int>(m_draft.timeStatus)));
    form->addRow(eventDialogText("Time status"), m_timeStatus);

    m_type = new QComboBox(this);
    m_type->setObjectName(QStringLiteral("calendarEventType"));
    for (const auto& [name, label] : {
             std::pair{"Vacation", eventDialogText("Vacation")},
             std::pair{"Holiday", eventDialogText("Holiday")},
             std::pair{"Workshop", eventDialogText("Workshop")},
             std::pair{"CM", eventDialogText("CM")},
             std::pair{"Meeting", eventDialogText("Meeting")},
             std::pair{"Other", eventDialogText("Other")}})
    {
        m_type->addItem(label, QString::fromLatin1(name));
    }
    const int typeIndex = m_type->findData(fromUtf8(m_draft.eventType));
    m_type->setCurrentIndex(typeIndex >= 0 ? typeIndex : m_type->findData(
        QStringLiteral("Other")));
    form->addRow(eventDialogText("Event type"), m_type);

    outer->addLayout(form);

    m_repeat = new QCheckBox(eventDialogText("Repeat event"), this);
    m_repeat->setObjectName(QStringLiteral("calendarEventRepeat"));
    m_repeat->setVisible(!existingEvent);
    outer->addWidget(m_repeat);

    auto* repeatForm = new QFormLayout;
    m_repeatFrequency = new QComboBox(this);
    m_repeatFrequency->setObjectName(QStringLiteral("calendarEventRepeatFrequency"));
    m_repeatFrequency->addItem(eventDialogText("Daily"),
        static_cast<int>(Application::CalendarEventRepeatFrequency::Daily));
    m_repeatFrequency->addItem(eventDialogText("Weekly"),
        static_cast<int>(Application::CalendarEventRepeatFrequency::Weekly));
    m_repeatFrequency->addItem(eventDialogText("Monthly"),
        static_cast<int>(Application::CalendarEventRepeatFrequency::Monthly));
    m_repeatFrequency->setCurrentIndex(m_repeatFrequency->findData(
        static_cast<int>(Application::CalendarEventRepeatFrequency::Weekly)));
    repeatForm->addRow(eventDialogText("Repeat frequency"), m_repeatFrequency);

    m_repeatUntil = new QDateEdit(this);
    m_repeatUntil->setObjectName(QStringLiteral("calendarEventRepeatUntil"));
    m_repeatUntil->setCalendarPopup(true);
    m_repeatUntil->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_repeatUntil->setDate(m_startDate->date());
    repeatForm->addRow(eventDialogText("Repeat through"), m_repeatUntil);
    outer->addLayout(repeatForm);

    const bool isRepeatSeries = m_draft.repeatSeriesId.has_value();
    m_seriesScope = new QComboBox(this);
    m_seriesScope->setObjectName(QStringLiteral("calendarEventSeriesScope"));
    m_seriesScope->addItem(eventDialogText("This event only"),
        static_cast<int>(CalendarEventSeriesEditScope::ThisEventOnly));
    m_seriesScope->addItem(eventDialogText("This and following events"),
        static_cast<int>(CalendarEventSeriesEditScope::ThisAndFollowingEvents));
    m_seriesScope->setCurrentIndex(m_seriesScope->findData(
        static_cast<int>(initialScope)));
    m_seriesScope->setVisible(isRepeatSeries);
    outer->addWidget(m_seriesScope);

    m_validationMessage = new QLabel(this);
    m_validationMessage->setObjectName(QStringLiteral("calendarEventValidation"));
    m_validationMessage->setWordWrap(true);
    m_validationMessage->setStyleSheet(QStringLiteral("color: #a33;"));
    m_validationMessage->hide();
    outer->addWidget(m_validationMessage);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        this);
    buttons->setObjectName(QStringLiteral("calendarEventButtons"));
    connect(buttons, &QDialogButtonBox::accepted, this, &CalendarEventDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if (existingEvent)
    {
        m_deleteButton = buttons->addButton(
            eventDialogText("Delete"), QDialogButtonBox::DestructiveRole);
        m_deleteButton->setObjectName(QStringLiteral("calendarEventDelete"));
        connect(m_deleteButton, &QPushButton::clicked, this, [this]
        {
            m_operation = CalendarEventDialogOperation::Delete;
            QDialog::accept();
        });
    }
    outer->addWidget(buttons);

    connect(m_allDay, &QCheckBox::toggled, this, [this](const bool checked)
    {
        if (checked)
        {
            m_timeStatus->setCurrentIndex(m_timeStatus->findData(
                static_cast<int>(Domain::CalendarEventTimeStatus::Timed)));
        }
        updateTimeAvailability();
    });
    connect(m_timeStatus, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this] { updateTimeAvailability(); });
    connect(m_repeat, &QCheckBox::toggled, this,
        [this] { updateRepeatAvailability(); });
    connect(m_startDate, &QDateEdit::dateChanged, this, [this](const QDate& date)
    {
        if (m_endDate->date() < date)
        {
            m_endDate->setDate(date);
        }
        if (m_repeatUntil->date() < date)
        {
            m_repeatUntil->setDate(date);
        }
    });

    updateTimeAvailability();
    updateRepeatAvailability();
}

Application::CalendarEventEditDraft CalendarEventDialog::eventData() const
{
    Application::CalendarEventEditDraft draft = m_draft;
    draft.title = m_title->text().toUtf8().toStdString();
    draft.startDate = m_startDate->date().toString(Qt::ISODate).toStdString();
    draft.endDate = m_endDate->date().toString(Qt::ISODate).toStdString();
    draft.allDay = m_allDay->isChecked();
    draft.eventType = m_type->currentData().toString().toStdString();
    draft.timeStatus = static_cast<Domain::CalendarEventTimeStatus>(
        m_timeStatus->currentData().toInt());
    if (draft.allDay || draft.timeStatus != Domain::CalendarEventTimeStatus::Timed)
    {
        draft.startTime.reset();
        draft.endTime.reset();
    }
    else
    {
        draft.startTime = m_startTime->time().toString(
            QStringLiteral("HH:mm")).toStdString();
        draft.endTime = m_endTime->time().toString(
            QStringLiteral("HH:mm")).toStdString();
    }
    return draft;
}

CalendarEventDialogOperation CalendarEventDialog::operation() const noexcept
{
    return m_operation;
}

bool CalendarEventDialog::repeatEnabled() const
{
    return !m_existingEvent && m_repeat->isChecked();
}

Application::CalendarEventRepeatFrequency
CalendarEventDialog::repeatFrequency() const
{
    return static_cast<Application::CalendarEventRepeatFrequency>(
        m_repeatFrequency->currentData().toInt());
}

QString CalendarEventDialog::repeatUntilDate() const
{
    return m_repeatUntil->date().toString(Qt::ISODate);
}

CalendarEventSeriesEditScope CalendarEventDialog::seriesEditScope() const
{
    return static_cast<CalendarEventSeriesEditScope>(
        m_seriesScope->currentData().toInt());
}

void CalendarEventDialog::accept()
{
    if (m_operation == CalendarEventDialogOperation::Delete)
    {
        QDialog::accept();
        return;
    }

    const Application::CalendarEventEditDraft draft = eventData();
    const Domain::Result<void> valid = draft.validate();
    if (!valid)
    {
        m_validationMessage->setText(
            eventDialogText("Check the title, dates, and times, then try again."));
        m_validationMessage->show();
        return;
    }
    if (repeatEnabled() && m_repeatUntil->date() < m_startDate->date())
    {
        m_validationMessage->setText(
            eventDialogText("The repeat end date must be on or after the start date."));
        m_validationMessage->show();
        return;
    }
    m_validationMessage->hide();
    QDialog::accept();
}

void CalendarEventDialog::updateTimeAvailability()
{
    const bool timed = !m_allDay->isChecked()
        && m_timeStatus->currentData().toInt()
            == static_cast<int>(Domain::CalendarEventTimeStatus::Timed);
    m_startTime->setEnabled(timed);
    m_endTime->setEnabled(timed);
}

void CalendarEventDialog::updateRepeatAvailability()
{
    const bool enabled = !m_existingEvent && m_repeat->isChecked();
    m_repeatFrequency->setEnabled(enabled);
    m_repeatUntil->setEnabled(enabled);
}

} // namespace ClassMngr::Next::Ui
