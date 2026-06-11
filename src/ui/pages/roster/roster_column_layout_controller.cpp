#include "roster_column_layout_controller.h"

#include "ui/pages/roster/roster_constants.h"
#include "ui/pages/roster/roster_model.h"

#include <QHeaderView>
#include <QTableView>

RosterColumnLayoutController::RosterColumnLayoutController(
    QObject* parent
    )
    : QObject(parent)
{
}

void RosterColumnLayoutController::attach(
    QTableView* table,
    RosterModel* model
    )
{
    m_table = table;
    m_model = model;

    applyResizeModes();
}

void RosterColumnLayoutController::applyResizeModes()
{
    if (!m_table || !m_model)
    {
        return;
    }

    auto* header =
        m_table->horizontalHeader();

    header->setSectionsMovable(false);
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(60);

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        header->setSectionResizeMode(
            column,
            m_model->isRequiredColumn(column)
                ? QHeaderView::Fixed
                : QHeaderView::Interactive
            );
    }
}

void RosterColumnLayoutController::applyWidths(
    const QVector<int>& widths
    )
{
    if (!m_table || !m_model)
    {
        return;
    }

    applyResizeModes();

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        int width =
            column < widths.size()
                ? widths[column]
                : 0;

        if (width <= 0)
        {
            width =
                RosterUi::defaultColumnWidth(
                    m_model->columnName(column)
                    );
        }

        m_table->setColumnWidth(
            column,
            width
            );
    }

    enforceStudentInformationMinimum();
}

QVector<int> RosterColumnLayoutController::currentWidths() const
{
    QVector<int> widths;

    if (!m_table || !m_model)
    {
        return widths;
    }

    widths.reserve(
        m_model->columnCount()
        );

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        widths.append(
            m_table->columnWidth(column)
            );
    }

    return widths;
}

QString RosterColumnLayoutController::columnGroup(
    int column
    ) const
{
    if (!m_model)
    {
        return {};
    }

    return RosterUi::columnGroup(
        m_model->columnName(column)
        );
}

int RosterColumnLayoutController::groupStart(
    int column
    ) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
    {
        return column;
    }

    const QString group =
        columnGroup(column);

    int start = column;

    while (
        start > 0
        && columnGroup(start - 1) == group
        )
    {
        --start;
    }

    return start;
}

int RosterColumnLayoutController::groupEnd(
    int column
    ) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
    {
        return column;
    }

    const QString group =
        columnGroup(column);

    int end = column;

    while (
        end + 1 < m_model->columnCount()
        && columnGroup(end + 1) == group
        )
    {
        ++end;
    }

    return end;
}

bool RosterColumnLayoutController::isGroupBoundaryAfter(
    int column
    ) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount() - 1)
    {
        return false;
    }

    return columnGroup(column) != columnGroup(column + 1);
}

void RosterColumnLayoutController::enforceStudentInformationMinimum()
{
    if (!m_table || !m_model)
    {
        return;
    }

    int start = -1;
    int end = -1;
    int totalWidth = 0;

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        if (columnGroup(column) != RosterUi::studentInformationGroup())
        {
            continue;
        }

        if (start < 0)
        {
            start = column;
        }

        end = column;
        totalWidth += m_table->columnWidth(column);
    }

    if (
        start < 0
        || end < 0
        || totalWidth >= RosterUi::StudentInformationMinWidth
        )
    {
        return;
    }

    m_table->setColumnWidth(
        end,
        m_table->columnWidth(end)
            + (RosterUi::StudentInformationMinWidth - totalWidth)
        );
}
