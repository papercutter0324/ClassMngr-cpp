#include "class_time_row.h"

#include "features/classes/config/class_info_config.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <algorithm>
#include <QComboBox>
#include "ui/shared/widgets/no_wheel_combobox.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSizePolicy>

#include <array>

namespace
{
constexpr int DefaultClassDurationMinutes = 55;
constexpr int LatestEndTimeMinutes = 21 * 60 + 55;

constexpr std::array<int, 5> EndTimeDurations{
    DefaultClassDurationMinutes,
    85,
    115,
    175,
    235
};

QString translatedDay(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return ClassTimeRow::tr("Monday");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return ClassTimeRow::tr("Tuesday");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return ClassTimeRow::tr("Wednesday");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return ClassTimeRow::tr("Thursday");
    }
    if (day == QStringLiteral("Friday"))
    {
        return ClassTimeRow::tr("Friday");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return ClassTimeRow::tr("Saturday");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return ClassTimeRow::tr("Sunday");
    }

    return day;
}

QStringList translatedDays()
{
    QStringList days;
    days.reserve(ClassInfoConfig::Days.size());

    for (const QString& day : ClassInfoConfig::Days)
    {
        days.append(translatedDay(day));
    }

    return days;
}

QStringList periodOptions()
{
    return {
        QStringLiteral("PM"),
        QStringLiteral("AM")
    };
}

QStringList endTimeWidthOptions()
{
    QStringList values;

    for (const QString& period : periodOptions())
    {
        for (int hour = 1; hour <= 12; ++hour)
        {
            for (int minute = 0; minute < 60; minute += 5)
            {
                values.append(
                    QStringLiteral("%1:%2 %3")
                        .arg(hour)
                        .arg(minute, 2, 10, QLatin1Char('0'))
                        .arg(period)
                    );
            }
        }
    }

    return values;
}

void applyComboWidth(
    QComboBox* combo,
    const QStringList& texts,
    int preferredWidth,
    int maximumWidth = 0
    )
{
    Q_UNUSED(maximumWidth);

    if (!combo)
    {
        return;
    }

    const int minimumWidth =
        WidgetSizing::comboMinimumWidthForTexts(
            combo,
            texts,
            UiConstants::ClassInfo::TextWidthPadding
            );

    WidgetSizing::installTextAwareFieldWidth(
        combo,
        std::max(
            preferredWidth,
            minimumWidth
            ),
        QSizePolicy::Minimum
        );
}

void applyButtonWidth(
    QPushButton* button,
    int preferredWidth
    )
{
    if (!button)
    {
        return;
    }

    const int minimumWidth =
        std::max(
            button->minimumSizeHint().width(),
            WidgetSizing::textWidth(
                button,
                button->text()
                )
            + UiConstants::ClassInfo::TextWidthPadding
            );

    if (preferredWidth >= minimumWidth)
    {
        button->setMinimumWidth(preferredWidth);
        button->setSizePolicy(
            QSizePolicy::Fixed,
            QSizePolicy::Preferred
            );
        return;
    }

    button->setMinimumWidth(minimumWidth);
    button->setSizePolicy(
        QSizePolicy::Minimum,
        QSizePolicy::Preferred
        );
}
}

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
    setObjectName("classTimeRow");

    // -------------------------
    // Day
    // -------------------------
    m_dayCombo = new NoWheelComboBox(this);
    for (const QString& day : ClassInfoConfig::Days)
    {
        m_dayCombo->addItem(
            translatedDay(day),
            day
            );
    }
    applyComboWidth(
        m_dayCombo,
        translatedDays(),
        0,
        UiConstants::ClassInfo::TimeRow::DayComboMaxWidth
        );

    // -------------------------
    // Start widget container
    // -------------------------
    m_startWidget = new QWidget(this);
    m_startWidget->setObjectName("startTimeControls");
    m_startWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    auto* startLayout = new QHBoxLayout(m_startWidget);
    startLayout->setContentsMargins(0, 0, 0, 0);
    startLayout->setSpacing(
        UiConstants::ClassInfo::TimeRow::StartLayoutSpacing
        );

    // -------------------------
    // Start time controls
    // -------------------------
    m_startHourCombo = new NoWheelComboBox(this);
    m_startHourCombo->addItems(
        type == ScheduleType::Regular
            ? ClassInfoConfig::RegularHours
            : ClassInfoConfig::IntensiveHours
        );
    applyComboWidth(
        m_startHourCombo,
        type == ScheduleType::Regular
            ? ClassInfoConfig::RegularHours
            : ClassInfoConfig::IntensiveHours,
        UiConstants::ClassInfo::TimeRow::StartComboWidth
        );

    if (m_startHourCombo->findText("4") >= 0)
        m_startHourCombo->setCurrentText("4");

    m_startMinuteCombo = new NoWheelComboBox(this);
    m_startMinuteCombo->addItems(ClassInfoConfig::StartMinutes);
    applyComboWidth(
        m_startMinuteCombo,
        ClassInfoConfig::StartMinutes,
        UiConstants::ClassInfo::TimeRow::StartComboWidth
        + UiConstants::ClassInfo::TimeRow::MinuteComboExtraWidth
        );
    m_startMinuteCombo->setCurrentText(":00");

    m_startPeriodCombo = new NoWheelComboBox(this);
    m_startPeriodCombo->addItems(periodOptions());
    applyComboWidth(
        m_startPeriodCombo,
        periodOptions(),
        UiConstants::ClassInfo::TimeRow::StartComboWidth
        );
    m_startPeriodCombo->setCurrentText("PM");

    startLayout->addWidget(m_startHourCombo);
    startLayout->addWidget(m_startMinuteCombo);
    startLayout->addWidget(m_startPeriodCombo);
    startLayout->setSizeConstraint(QLayout::SetMinimumSize);

    // -------------------------
    // End + remove
    // -------------------------
    m_endCombo = new NoWheelComboBox(this);
    applyComboWidth(
        m_endCombo,
        endTimeWidthOptions(),
        UiConstants::ClassInfo::TimeRow::EndComboWidth
        );

    m_removeButton =
        new TextFitPushButton(tr("Remove"), this);
    applyButtonWidth(
        m_removeButton,
        UiConstants::ClassInfo::TimeRow::RemoveButtonWidth
        );

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
    layout->setHorizontalSpacing(
        UiConstants::ClassInfo::TimeRow::HorizontalSpacing
        );
    layout->setVerticalSpacing(
        UiConstants::ClassInfo::TimeRow::VerticalSpacing
        );

    layout->addWidget(m_dayCombo, 0, 0, Qt::AlignLeft);
    layout->addWidget(m_startWidget, 0, 1, Qt::AlignLeft);
    layout->addWidget(m_endCombo, 0, 2, Qt::AlignLeft);
    layout->addWidget(m_removeButton, 0, 3, Qt::AlignLeft);

    layout->setColumnStretch(
        0,
        UiConstants::ClassInfo::TimeRow::DayColumnStretch
        );
    layout->setColumnStretch(
        1,
        UiConstants::ClassInfo::TimeRow::StartColumnStretch
        );
    layout->setColumnStretch(
        2,
        UiConstants::ClassInfo::TimeRow::EndColumnStretch
        );
    layout->setColumnStretch(
        3,
        UiConstants::ClassInfo::TimeRow::RemoveColumnStretch
        );
    layout->setColumnStretch(
        4,
        UiConstants::ClassInfo::TimeRow::FillerColumnStretch
        );
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    m_previousStartMinutes = currentStartMinutes();
    updateEndTimes();
}

QString ClassTimeRow::day() const
{
    return m_dayCombo->currentData().toString();
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
    const int index = m_dayCombo->findData(day);
    if (index >= 0)
    {
        m_dayCombo->setCurrentIndex(index);
    }
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

void ClassTimeRow::retranslateUi()
{
    if (!m_removeButton)
    {
        return;
    }

    m_removeButton->setText(
        tr("Remove")
        );

    m_dayCombo->blockSignals(true);
    for (int index = 0; index < m_dayCombo->count(); ++index)
    {
        m_dayCombo->setItemText(
            index,
            translatedDay(
                m_dayCombo->itemData(index).toString()
                )
            );
    }
    m_dayCombo->blockSignals(false);

    applyComboWidth(
        m_dayCombo,
        translatedDays(),
        0,
        UiConstants::ClassInfo::TimeRow::DayComboMaxWidth
        );
    applyButtonWidth(
        m_removeButton,
        UiConstants::ClassInfo::TimeRow::RemoveButtonWidth
        );
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

int ClassTimeRow::currentStartMinutes() const
{
    return toTotalMinutes(
        m_startHourCombo->currentText(),
        m_startMinuteCombo->currentText().mid(1),
        m_startPeriodCombo->currentText()
        );
}

int ClassTimeRow::durationForEndTime(
    const QString& endTime
    ) const
{
    const QStringList parts = endTime.split(' ');

    if (parts.size() != 2)
    {
        return DefaultClassDurationMinutes;
    }

    const QStringList timeParts = parts.first().split(':');

    if (timeParts.size() != 2)
    {
        return DefaultClassDurationMinutes;
    }

    int duration = toTotalMinutes(
        timeParts.first(),
        timeParts.last(),
        parts.last()
        ) - m_previousStartMinutes;

    if (duration <= 0)
    {
        duration += 24 * 60;
    }

    return std::ranges::contains(
        EndTimeDurations,
        duration
        )
        ? duration
        : DefaultClassDurationMinutes;
}

QStringList ClassTimeRow::endTimeOptions(
    int startMinutes
    ) const
{
    QStringList options;
    options.reserve(static_cast<qsizetype>(EndTimeDurations.size()));

    for (const int duration : EndTimeDurations)
    {
        const int endMinutes = startMinutes + duration;

        if (endMinutes <= LatestEndTimeMinutes)
        {
            options.append(fromTotalMinutes(endMinutes));
        }
    }

    return options;
}

void ClassTimeRow::updateEndTimes()
{
    m_endCombo->blockSignals(true);

    const int duration = durationForEndTime(
        m_endCombo->currentText()
        );
    const int startMinutes = currentStartMinutes();
    const QStringList validEndTimes = endTimeOptions(startMinutes);
    const QString matchingEndTime = fromTotalMinutes(
        startMinutes + duration
        );

    m_endCombo->clear();
    m_endCombo->addItems(
        validEndTimes
        );

    if (validEndTimes.contains(matchingEndTime))
    {
        m_endCombo->setCurrentText(
            matchingEndTime
            );
    }
    else if (!validEndTimes.isEmpty())
    {
        m_endCombo->setCurrentIndex(0);
    }

    m_previousStartMinutes = startMinutes;
    m_endCombo->blockSignals(false);

    emit rowChanged();
}
