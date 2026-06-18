#include "class_schedule_section.h"

#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/sectioncards/class_info_section_card.h"
#include "ui/shared/widgets/sectioncards/class_time_row.h"
#include "features/classes/config/class_info_config.h"

#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariantList>

#include <utility>

namespace
{
QLabel* createFieldLabel(
    QWidget* parent,
    const QString& text
    )
{
    auto* label = new QLabel(text, parent);

    label->setContentsMargins(
        UiConstants::ClassInfo::Form::LabelIndent,
        0,
        0,
        0
        );

    return label;
}

QLabel* createScheduleSubtitle(
    QWidget* parent,
    const QString& text
    )
{
    auto* label = new QLabel(text, parent);
    label->setObjectName("sectionSubtitle");

    QFont font = label->font();
    font.setPointSize(
        UiConstants::ClassInfo::Schedule::SubtitleFontSize
        );
    font.setWeight(QFont::DemiBold);
    font.setItalic(true);

    label->setFont(font);

    return label;
}

int dayHeaderWidth()
{
    QComboBox probe;
    probe.addItems(ClassInfoConfig::Days);

    return WidgetSizing::comboMinimumWidthForTexts(
        &probe,
        ClassInfoConfig::Days,
        UiConstants::ClassInfo::TextWidthPadding
        );
}

int startTimeHeaderWidth()
{
    return UiConstants::ClassInfo::TimeRow::StartComboWidth
        + UiConstants::ClassInfo::TimeRow::StartLayoutSpacing
        + UiConstants::ClassInfo::TimeRow::StartComboWidth
        + UiConstants::ClassInfo::TimeRow::MinuteComboExtraWidth
        + UiConstants::ClassInfo::TimeRow::StartLayoutSpacing
        + UiConstants::ClassInfo::TimeRow::StartComboWidth;
}

QFrame* createScheduleSeparator(
    QWidget* parent
    )
{
    auto* separator = new QFrame(parent);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setProperty("role", UiRoles::Separator);

    return separator;
}

void configureScheduleGrid(
    QGridLayout* grid
    )
{
    grid->setHorizontalSpacing(
        UiConstants::ClassInfo::Schedule::HorizontalSpacing
        );

    grid->setVerticalSpacing(
        UiConstants::ClassInfo::Schedule::VerticalSpacing
        );

    grid->setColumnStretch(
        0,
        UiConstants::ClassInfo::Schedule::DayColumnStretch
        );

    grid->setColumnStretch(
        1,
        UiConstants::ClassInfo::Schedule::StartTimeColumnStretch
        );

    grid->setColumnStretch(
        2,
        UiConstants::ClassInfo::Schedule::EndTimeColumnStretch
        );

    grid->setColumnStretch(
        3,
        UiConstants::ClassInfo::Schedule::RemoveColumnStretch
        );

    grid->setColumnStretch(
        4,
        UiConstants::ClassInfo::Schedule::FillerColumnStretch
        );
}
}

// =========================================================
// Constructor
// =========================================================

ClassScheduleSection::ClassScheduleSection(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(
        UiConstants::ClassInfo::Schedule::OuterMargin,
        UiConstants::ClassInfo::Schedule::OuterMargin,
        UiConstants::ClassInfo::Schedule::OuterMargin,
        UiConstants::ClassInfo::Schedule::OuterMargin
        );
    layout->setSpacing(
        UiConstants::ClassInfo::Schedule::OuterMargin
        );

    m_timesCard = new SectionCard(tr("Class Times"), this);
    auto* cardLayout = m_timesCard->contentLayout();

    m_regularGrid = new QGridLayout();
    m_intensiveGrid = new QGridLayout();

    configureScheduleGrid(m_regularGrid);
    configureScheduleGrid(m_intensiveGrid);

    m_regularGrid->addWidget(
        createScheduleHeader(m_timesCard),
        0,
        0,
        1,
        UiConstants::ClassInfo::Schedule::RowColumnSpan
        );

    m_intensiveGrid->addWidget(
        createScheduleHeader(m_timesCard),
        0,
        0,
        1,
        UiConstants::ClassInfo::Schedule::RowColumnSpan
        );

    m_addRegularButton = new QPushButton(tr("+ Add Time"), m_timesCard);
    m_addIntensiveButton = new QPushButton(tr("+ Add Intensive Time"), m_timesCard);

    m_addRegularButton->setFixedWidth(
        UiConstants::ClassInfo::Schedule::AddButtonFixedWidth
        );
    m_addIntensiveButton->setFixedWidth(
        UiConstants::ClassInfo::Schedule::AddButtonFixedWidth
        );

    connect(m_addRegularButton, &QPushButton::clicked, this, [this] {
        addRow(m_regularGrid, m_regularRows, ScheduleType::Regular);
    });

    connect(m_addIntensiveButton, &QPushButton::clicked, this, [this] {
        addRow(m_intensiveGrid, m_intensiveRows, ScheduleType::Intensive);
    });

    cardLayout->addWidget(
        m_regularSubtitle =
            createScheduleSubtitle(
                m_timesCard,
                tr("Regular Schedule")
                )
        );
    cardLayout->addLayout(m_regularGrid);
    cardLayout->addWidget(
        m_addRegularButton,
        0,
        Qt::AlignLeft
        );
    cardLayout->addWidget(
        createScheduleSeparator(m_timesCard)
        );
    cardLayout->addWidget(
        m_intensiveSubtitle =
            createScheduleSubtitle(
                m_timesCard,
                tr("Intensive Schedule")
                )
        );
    cardLayout->addLayout(m_intensiveGrid);
    cardLayout->addWidget(
        m_addIntensiveButton,
        0,
        Qt::AlignLeft
        );

    layout->addWidget(m_timesCard);
}

QWidget* ClassScheduleSection::createScheduleHeader(
    QWidget* parent
    )
{
    auto* header = new QWidget(parent);

    auto* layout = new QGridLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(
        UiConstants::ClassInfo::TimeRow::HorizontalSpacing
        );
    layout->setVerticalSpacing(
        UiConstants::ClassInfo::TimeRow::VerticalSpacing
        );

    auto* dayLabel = createFieldLabel(header, QObject::tr("Days"));
    auto* startLabel = createFieldLabel(header, QObject::tr("Start Time"));
    auto* endLabel = createFieldLabel(header, QObject::tr("End Time"));
    auto* removeLabel = createFieldLabel(header, QString());
    auto* fillerLabel = createFieldLabel(header, QString());

    m_dayHeaderLabels.append(dayLabel);
    m_startHeaderLabels.append(startLabel);
    m_endHeaderLabels.append(endLabel);

    dayLabel->setMinimumWidth(
        dayHeaderWidth()
        );
    startLabel->setMinimumWidth(
        startTimeHeaderWidth()
        );
    endLabel->setMinimumWidth(
        UiConstants::ClassInfo::TimeRow::EndComboWidth
        );
    removeLabel->setMinimumWidth(
        UiConstants::ClassInfo::TimeRow::RemoveButtonWidth
        );

    layout->addWidget(dayLabel, 0, 0, Qt::AlignLeft);
    layout->addWidget(startLabel, 0, 1, Qt::AlignLeft);
    layout->addWidget(endLabel, 0, 2, Qt::AlignLeft);
    layout->addWidget(removeLabel, 0, 3, Qt::AlignLeft);
    layout->addWidget(fillerLabel, 0, 4);

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

    return header;
}

void ClassScheduleSection::retranslateUi()
{
    if (m_timesCard)
    {
        m_timesCard->setTitle(
            tr("Class Times")
            );
    }

    if (m_regularSubtitle)
    {
        m_regularSubtitle->setText(
            tr("Regular Schedule")
            );
    }

    if (m_intensiveSubtitle)
    {
        m_intensiveSubtitle->setText(
            tr("Intensive Schedule")
            );
    }

    if (m_addRegularButton)
    {
        m_addRegularButton->setText(
            tr("+ Add Time")
            );
    }

    if (m_addIntensiveButton)
    {
        m_addIntensiveButton->setText(
            tr("+ Add Intensive Time")
            );
    }

    retranslateScheduleHeaders();

    for (ClassTimeRow* row : std::as_const(m_regularRows))
    {
        if (row)
        {
            row->retranslateUi();
        }
    }

    for (ClassTimeRow* row : std::as_const(m_intensiveRows))
    {
        if (row)
        {
            row->retranslateUi();
        }
    }
}

void ClassScheduleSection::retranslateScheduleHeaders()
{
    for (QLabel* label : std::as_const(m_dayHeaderLabels))
    {
        if (label)
        {
            label->setText(
                QObject::tr("Days")
                );
        }
    }

    for (QLabel* label : std::as_const(m_startHeaderLabels))
    {
        if (label)
        {
            label->setText(
                QObject::tr("Start Time")
                );
        }
    }

    for (QLabel* label : std::as_const(m_endHeaderLabels))
    {
        if (label)
        {
            label->setText(
                QObject::tr("End Time")
                );
        }
    }
}

// =========================================================
// Convenience API
// =========================================================

ClassTimeRow* ClassScheduleSection::addRegularRow(bool markDirty)
{
    return addRow(m_regularGrid, m_regularRows, ScheduleType::Regular, markDirty);
}

ClassTimeRow* ClassScheduleSection::addIntensiveRow(bool markDirty)
{
    return addRow(m_intensiveGrid, m_intensiveRows, ScheduleType::Intensive, markDirty);
}

// =========================================================
// Core row creation
// =========================================================

ClassTimeRow* ClassScheduleSection::addRow(
    QGridLayout* grid,
    QList<ClassTimeRow*>& container,
    ScheduleType type,
    bool markDirty
    )
{
    auto* row = new ClassTimeRow(type, this);

    container.append(row);

    connectRowSignals(row);

    const int rowIndex = container.size();

    grid->addWidget(
        row,
        rowIndex,
        0,
        1,
        UiConstants::ClassInfo::Schedule::RowColumnSpan
        );

    connect(row, &ClassTimeRow::removeRequested, this,
            [this, grid, &container](ClassTimeRow* r) {
                removeRow(grid, container, r);
            });

    if (markDirty)
        emit dataChanged();

    return row;
}

void ClassScheduleSection::connectRowSignals(
    ClassTimeRow* row
    )
{
    connect(
        row,
        &ClassTimeRow::rowChanged,
        this,
        &ClassScheduleSection::dataChanged
    );
}

// =========================================================
// Remove
// =========================================================

void ClassScheduleSection::removeRow(
    QGridLayout* grid,
    QList<ClassTimeRow*>& container,
    ClassTimeRow* row,
    bool markDirty
    )
{
    if (!container.contains(row))
        return;

    container.removeOne(row);

    grid->removeWidget(row);

    row->deleteLater();

    rebuildGrid(grid, container);

    if (markDirty)
        emit dataChanged();
}

// =========================================================
// Grid rebuild
// =========================================================

void ClassScheduleSection::rebuildGrid(
    QGridLayout* grid,
    const QList<ClassTimeRow*>& rows
    )
{
    for (int i = 0; i < rows.size(); ++i)
    {
        auto* row = rows[i];

        grid->addWidget(
            row,
            i + 1,
            0,
            1,
            UiConstants::ClassInfo::Schedule::RowColumnSpan
            );
    }
}

// =========================================================
// Serialization
// =========================================================

QVariantList ClassScheduleSection::serializeRegular() const
{
    QVariantList out;

    for (auto* row : m_regularRows)
    {
        QVariantMap m;
        m["day"] = row->day();
        m["start_time"] = row->startTime();
        m["end_time"] = row->endTime();
        out.append(m);
    }

    return out;
}

QVariantList ClassScheduleSection::serializeIntensive() const
{
    QVariantList out;

    for (auto* row : m_intensiveRows)
    {
        QVariantMap m;
        m["day"] = row->day();
        m["start_time"] = row->startTime();
        m["end_time"] = row->endTime();
        out.append(m);
    }

    return out;
}

// =========================================================
// Load
// =========================================================

void ClassScheduleSection::loadSchedules(
    const QVariantList& regular,
    const QVariantList& intensive
    )
{
    QList<ClassTime> regularTimes;
    QList<ClassTime> intensiveTimes;

    for (const auto& item : regular)
    {
        const auto map =
            item.toMap();

        ClassTime time;
        time.day = map["day"].toString();
        time.startTime = map["start_time"].toString();
        time.endTime = map["end_time"].toString();

        regularTimes.append(time);
    }

    for (const auto& item : intensive)
    {
        const auto map =
            item.toMap();

        ClassTime time;
        time.day = map["day"].toString();
        time.startTime = map["start_time"].toString();
        time.endTime = map["end_time"].toString();

        intensiveTimes.append(time);
    }

    loadSchedules(
        regularTimes,
        intensiveTimes
        );
}

void ClassScheduleSection::loadSchedules(
    const QList<ClassTime>& regular,
    const QList<ClassTime>& intensive
    )
{
    for (auto* r : m_regularRows)
    {
        m_regularGrid->removeWidget(r);
        r->deleteLater();
    }

    m_regularRows.clear();

    for (auto* r : m_intensiveRows)
    {
        m_intensiveGrid->removeWidget(r);
        r->deleteLater();
    }

    m_intensiveRows.clear();

    for (const ClassTime& time : regular)
    {
        addRow(m_regularGrid, m_regularRows, ScheduleType::Regular, false);

        auto* row = m_regularRows.last();

        row->setDay(time.day);
        row->setStartTime(time.startTime);
        row->setEndTime(time.endTime);
    }

    if (m_regularRows.isEmpty())
        addRegularRow(false);

    for (const ClassTime& time : intensive)
    {
        addRow(m_intensiveGrid, m_intensiveRows, ScheduleType::Intensive, false);

        auto* row = m_intensiveRows.last();

        row->setDay(time.day);
        row->setStartTime(time.startTime);
        row->setEndTime(time.endTime);
    }

    if (m_intensiveRows.isEmpty())
        addIntensiveRow(false);
}

QList<ClassTime> ClassScheduleSection::regularTimes() const
{
    QList<ClassTime> out;

    for (auto* row : m_regularRows)
    {
        ClassTime time;
        time.day = row->day();
        time.startTime = row->startTime();
        time.endTime = row->endTime();

        out.append(time);
    }

    return out;
}

QList<ClassTime> ClassScheduleSection::intensiveTimes() const
{
    QList<ClassTime> out;

    for (auto* row : m_intensiveRows)
    {
        ClassTime time;
        time.day = row->day();
        time.startTime = row->startTime();
        time.endTime = row->endTime();

        out.append(time);
    }

    return out;
}
