#include "class_schedule_section.h"
#include "ui/widgets/sectioncards/class_time_row.h"

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

    m_addRegularButton = new QPushButton("+ Add Time", this);
    m_addIntensiveButton = new QPushButton("+ Add Intensive Time", this);

    connect(m_addRegularButton, &QPushButton::clicked, this, [this] {
        addRow(m_regularGrid, m_regularRows, ScheduleType::Regular);
    });

    connect(m_addIntensiveButton, &QPushButton::clicked, this, [this] {
        addRow(m_intensiveGrid, m_intensiveRows, ScheduleType::Intensive);
    });

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(m_regularGrid);
    layout->addWidget(m_addRegularButton);
    layout->addLayout(m_intensiveGrid);
    layout->addWidget(m_addIntensiveButton);
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

    grid->addWidget(row->dayCombo(), rowIndex, 0);
    grid->addWidget(row->startWidget(), rowIndex, 1);
    grid->addWidget(row->endCombo(), rowIndex, 2);
    grid->addWidget(row->removeButton(), rowIndex, 3);

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

    grid->removeWidget(row->dayCombo());
    grid->removeWidget(row->startWidget());
    grid->removeWidget(row->endCombo());
    grid->removeWidget(row->removeButton());

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

        grid->addWidget(row->dayCombo(), i + 1, 0);
        grid->addWidget(row->startWidget(), i + 1, 1);
        grid->addWidget(row->endCombo(), i + 1, 2);
        grid->addWidget(row->removeButton(), i + 1, 3);
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
    for (auto* r : m_regularRows)
        r->deleteLater();
    m_regularRows.clear();

    for (auto* r : m_intensiveRows)
        r->deleteLater();
    m_intensiveRows.clear();

    for (const auto& item : regular)
    {
        addRow(m_regularGrid, m_regularRows, ScheduleType::Regular, false);

        auto* row = m_regularRows.last();

        auto map = item.toMap();
        row->setDay(map["day"].toString());
        row->setStartTime(map["start_time"].toString());
        row->setEndTime(map["end_time"].toString());
    }

    if (m_regularRows.isEmpty())
        addRegularRow();

    for (const auto& item : intensive)
    {
        addRow(m_intensiveGrid, m_intensiveRows, ScheduleType::Intensive, false);

        auto* row = m_intensiveRows.last();

        auto map = item.toMap();
        row->setDay(map["day"].toString());
        row->setStartTime(map["start_time"].toString());
        row->setEndTime(map["end_time"].toString());
    }

    if (m_intensiveRows.isEmpty())
        addIntensiveRow();
}