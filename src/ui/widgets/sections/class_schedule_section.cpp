#include "class_schedule_section.h"
#include "ui/widgets/sectioncards/class_time_row.h"
#include "ui/constants/gui_constants.h"

#include <QComboBox>
#include <QGridLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariantList>

// =========================================================
// Constructor
// =========================================================

ClassScheduleSection::ClassScheduleSection(QWidget* parent)
    : QWidget(parent)
{
    m_regularGrid = new QGridLayout();
    m_intensiveGrid = new QGridLayout();

    const auto configureScheduleGrid =
        [](QGridLayout* grid)
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
        };

    configureScheduleGrid(m_regularGrid);
    configureScheduleGrid(m_intensiveGrid);

    m_addRegularButton = new QPushButton("+ Add Time", this);
    m_addIntensiveButton = new QPushButton("+ Add Intensive Time", this);

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

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(m_regularGrid);
    layout->addWidget(
        m_addRegularButton,
        0,
        Qt::AlignLeft
        );
    layout->addLayout(m_intensiveGrid);
    layout->addWidget(
        m_addIntensiveButton,
        0,
        Qt::AlignLeft
        );
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
