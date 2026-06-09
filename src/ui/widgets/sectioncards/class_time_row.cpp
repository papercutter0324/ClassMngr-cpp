#include "class_time_row.h"

#include "config/class_info_config.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSizePolicy>
// =========================================================
// Constructor
// =========================================================

ClassTimeRow::ClassTimeRow(
    ScheduleType type,
    QWidget* parent
    )
    : QWidget(parent)
    , m_type(type)
{
    // -------------------------
    // Day
    // -------------------------
    m_dayCombo = new QComboBox(this);
    m_dayCombo->addItems(ClassInfoConfig::Days);

    // -------------------------
    // Start widget container
    // -------------------------
    m_startWidget = new QWidget(this);
    m_startWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    auto* startLayout = new QHBoxLayout(m_startWidget);
    startLayout->setContentsMargins(0, 0, 0, 0);
    startLayout->setSpacing(4);

    // -------------------------
    // Start time controls
    // -------------------------
    m_startHourCombo = new QComboBox(this);
    m_startHourCombo->addItems(
        type == ScheduleType::Regular
            ? ClassInfoConfig::RegularHours
            : ClassInfoConfig::IntensiveHours
        );
    m_startHourCombo->setFixedWidth(70);

    if (m_startHourCombo->findText("4") >= 0)
        m_startHourCombo->setCurrentText("4");

    m_startMinuteCombo = new QComboBox(this);
    m_startMinuteCombo->addItems(ClassInfoConfig::StartMinutes);
    m_startMinuteCombo->setFixedWidth(70);
    m_startMinuteCombo->setCurrentText(":00");

    m_startPeriodCombo = new QComboBox(this);
    m_startPeriodCombo->addItems({"PM", "AM"});
    m_startPeriodCombo->setFixedWidth(70);
    m_startPeriodCombo->setCurrentText("PM");

    startLayout->addWidget(m_startHourCombo);
    startLayout->addWidget(m_startMinuteCombo);
    startLayout->addWidget(m_startPeriodCombo);

    // -------------------------
    // End + remove
    // -------------------------
    m_endCombo = new QComboBox(this);
    m_endCombo->setFixedWidth(120);

    m_removeButton = new QPushButton(tr("Remove"), this);
    m_removeButton->setFixedWidth(90);

    // -------------------------
    // Signals
    // -------------------------
    connect(m_startHourCombo,   &QComboBox::currentTextChanged, this, &ClassTimeRow::updateEndTimes);
    connect(m_startMinuteCombo, &QComboBox::currentTextChanged, this, &ClassTimeRow::updateEndTimes);
    connect(m_startPeriodCombo, &QComboBox::currentTextChanged, this, &ClassTimeRow::updateEndTimes);
    connect(m_dayCombo,         &QComboBox::currentTextChanged, this, &ClassTimeRow::rowChanged);
    connect(m_endCombo,         &QComboBox::currentTextChanged, this, &ClassTimeRow::rowChanged);
    connect(m_removeButton,     &QPushButton::clicked,          this, &ClassTimeRow::onRemoveClicked);

    auto notifyChanged = [this]
    {
        emit dataChanged();
    };

    connect(m_dayCombo,         &QComboBox::currentTextChanged, this, notifyChanged);
    connect(m_startHourCombo,   &QComboBox::currentTextChanged, this, notifyChanged);
    connect(m_startMinuteCombo, &QComboBox::currentTextChanged, this, notifyChanged);
    connect(m_startPeriodCombo, &QComboBox::currentTextChanged, this, notifyChanged);
    connect(m_endCombo,         &QComboBox::currentTextChanged, this, notifyChanged);

    // -------------------------
    // Layout
    // -------------------------
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(16);
    layout->setVerticalSpacing(0);

    layout->addWidget(m_dayCombo, 0, 0);
    layout->addWidget(m_startWidget, 0, 1);
    layout->addWidget(m_endCombo, 0, 2);
    layout->addWidget(m_removeButton, 0, 3);

    layout->setColumnStretch(0, 2);
    layout->setColumnStretch(1, 2);
    layout->setColumnStretch(2, 2);
    layout->setColumnStretch(3, 0);

    updateEndTimes();
}

QString ClassTimeRow::day() const
{
    return m_dayCombo->currentText();
}

QString ClassTimeRow::startTime() const
{
    return QString("%1%2 %3")
    .arg(m_startHourCombo->currentText())
        .arg(m_startMinuteCombo->currentText())
        .arg(m_startPeriodCombo->currentText());
}

QString ClassTimeRow::endTime() const
{
    return m_endCombo->currentText();
}

void ClassTimeRow::setDay(
    const QString& day
    )
{
    m_dayCombo->setCurrentText(day);
}

void ClassTimeRow::setStartTime(
    const QString& value
    )
{
    if (value.isEmpty())
        return;

    const auto parts =
        value.split(' ');

    if (parts.size() != 2)
        return;

    const QString timePart =
        parts[0];

    const QString period =
        parts[1];

    const auto timeParts =
        timePart.split(':');

    if (timeParts.size() != 2)
        return;

    m_startHourCombo->setCurrentText(
        timeParts[0]
        );

    m_startMinuteCombo->setCurrentText(
        ":" + timeParts[1]
        );

    m_startPeriodCombo->setCurrentText(
        period
        );
}

void ClassTimeRow::setEndTime(
    const QString& value
    )
{
    if (!value.isEmpty())
        m_endCombo->setCurrentText(value);
}

void ClassTimeRow::onRemoveClicked()
{
    emit removeRequested(this);
}

int ClassTimeRow::toTotalMinutes(
    const QString& hour,
    const QString& minute,
    const QString& period
    )
{
    int h = hour.toInt();
    int m = minute.toInt();

    if (period == "AM")
    {
        if (h == 12)
            h = 0;
    }
    else
    {
        if (h != 12)
            h += 12;
    }

    return h * 60 + m;
}

QString ClassTimeRow::fromTotalMinutes(
    int totalMinutes
    )
{
    totalMinutes %= 1440;

    int hour24 = totalMinutes / 60;
    int minute = totalMinutes % 60;

    int hour{};
    QString period;

    if (hour24 == 0)
    {
        hour = 12;
        period = "AM";
    }
    else if (hour24 < 12)
    {
        hour = hour24;
        period = "AM";
    }
    else if (hour24 == 12)
    {
        hour = 12;
        period = "PM";
    }
    else
    {
        hour = hour24 - 12;
        period = "PM";
    }

    return QString("%1:%2 %3")
        .arg(hour)
        .arg(minute, 2, 10, QLatin1Char('0'))
        .arg(period);
}

QString ClassTimeRow::computeDefaultEndTime() const
{
    const int start =
        toTotalMinutes(
            m_startHourCombo->currentText(),
            m_startMinuteCombo->currentText().mid(1),
            m_startPeriodCombo->currentText()
            );

    return fromTotalMinutes(
        start + 50
        );
}

void ClassTimeRow::updateEndTimes()
{
    m_endCombo->blockSignals(true);

    const QString currentEnd =
        m_endCombo->currentText();

    m_endCombo->clear();

    const int start =
        toTotalMinutes(
            m_startHourCombo->currentText(),
            m_startMinuteCombo->currentText().mid(1),
            m_startPeriodCombo->currentText()
            );

    QStringList validEndTimes;

    for (int offset = 10;
         offset <= 240;
         offset += 10)
    {
        validEndTimes.append(
            fromTotalMinutes(
                start + offset
                )
            );
    }

    m_endCombo->addItems(
        validEndTimes
        );

    if (validEndTimes.contains(currentEnd))
    {
        m_endCombo->setCurrentText(
            currentEnd
            );
    }
    else
    {
        const QString defaultEnd =
            computeDefaultEndTime();

        if (validEndTimes.contains(defaultEnd))
        {
            m_endCombo->setCurrentText(
                defaultEnd
                );
        }
        else if (!validEndTimes.isEmpty())
        {
            m_endCombo->setCurrentIndex(0);
        }
    }

    m_endCombo->blockSignals(false);

    emit rowChanged();
}
