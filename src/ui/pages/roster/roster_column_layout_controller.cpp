#include "roster_column_layout_controller.h"

#include "ui/pages/roster/roster_constants.h"
#include "ui/pages/roster/roster_model.h"

#include <QHeaderView>
#include <QTableView>

#include <algorithm>

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
    if (!m_model || column < 0 || column >= m_model->columnCount())
    {
        return false;
    }

    if (column == m_model->columnCount() - 1)
    {
        return true;
    }

    return columnGroup(column) != columnGroup(column + 1);
}

bool RosterColumnLayoutController::isGroupBoundaryBefore(
    int column
    ) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
    {
        return false;
    }

    if (column == 0)
    {
        return true;
    }

    return columnGroup(column) != columnGroup(column - 1);
}

bool RosterColumnLayoutController::isCustomColumn(
    int column
    ) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
    {
        return false;
    }

    return columnGroup(column) == RosterUi::studentInformationGroup();
}

int RosterColumnLayoutController::customColumnMinimumWidth() const
{
    return RosterUi::CustomColumnDefaultWidth;
}

int RosterColumnLayoutController::studentInformationMinimumWidth() const
{
    return std::max(
        RosterUi::StudentInformationMinWidth,
        customColumnCount() * customColumnMinimumWidth()
        );
}

void RosterColumnLayoutController::initializeAddedCustomColumn(
    int column
    )
{
    if (!m_table || !m_model || !isCustomColumn(column))
    {
        return;
    }

    const int initialWidth =
        customColumnCount() <= 1
            ? std::max(
                  RosterUi::StudentInformationMinWidth,
                  customColumnMinimumWidth()
                  )
            : customColumnMinimumWidth();

    m_enforcingWidths = true;

    m_table->setColumnWidth(
        column,
        initialWidth
        );

    m_enforcingWidths = false;

    enforceStudentInformationMinimum();
}

void RosterColumnLayoutController::handleCustomColumnRemoved(
    int removedWidth
    )
{
    Q_UNUSED(removedWidth);

    enforceStudentInformationMinimum();
}

void RosterColumnLayoutController::handleSectionResized(
    int logicalIndex
    )
{
    if (
        m_enforcingWidths
        || !m_table
        || !m_model
        || !isCustomColumn(logicalIndex)
        )
    {
        return;
    }

    enforceStudentInformationMinimum();
}

void RosterColumnLayoutController::enforceStudentInformationMinimum()
{
    if (!m_table || !m_model || m_enforcingWidths)
    {
        return;
    }

    m_enforcingWidths = true;
    enforceStudentInformationMinimumInternal();
    m_enforcingWidths = false;
    updateAttachedViews();
}

int RosterColumnLayoutController::customColumnCount() const
{
    if (!m_model)
    {
        return 0;
    }

    int count = 0;

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        if (isCustomColumn(column))
        {
            ++count;
        }
    }

    return count;
}

int RosterColumnLayoutController::studentInformationStart() const
{
    if (!m_model)
    {
        return -1;
    }

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        if (isCustomColumn(column))
        {
            return column;
        }
    }

    return -1;
}

int RosterColumnLayoutController::studentInformationEnd() const
{
    if (!m_model)
    {
        return -1;
    }

    for (int column = m_model->columnCount() - 1; column >= 0; --column)
    {
        if (isCustomColumn(column))
        {
            return column;
        }
    }

    return -1;
}

int RosterColumnLayoutController::studentInformationWidth() const
{
    if (!m_table || !m_model)
    {
        return 0;
    }

    int totalWidth = 0;

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        if (isCustomColumn(column))
        {
            totalWidth += m_table->columnWidth(column);
        }
    }

    return totalWidth;
}

void RosterColumnLayoutController::enforceStudentInformationMinimumInternal()
{
    const int start =
        studentInformationStart();

    const int end =
        studentInformationEnd();

    if (start < 0 || end < 0)
    {
        return;
    }

    enforceCustomColumnMinimumsInternal();

    const int totalWidth =
        studentInformationWidth();

    const int minimumWidth =
        studentInformationMinimumWidth();

    if (totalWidth >= minimumWidth)
    {
        return;
    }

    m_table->setColumnWidth(
        end,
        m_table->columnWidth(end)
            + (minimumWidth - totalWidth)
        );
}

void RosterColumnLayoutController::enforceCustomColumnMinimumsInternal()
{
    if (!m_table || !m_model)
    {
        return;
    }

    const int minimumWidth =
        customColumnMinimumWidth();

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        if (
            isCustomColumn(column)
            && m_table->columnWidth(column) < minimumWidth
            )
        {
            m_table->setColumnWidth(
                column,
                minimumWidth
                );
        }
    }
}

void RosterColumnLayoutController::updateAttachedViews()
{
    if (!m_table)
    {
        return;
    }

    m_table->viewport()->update();

    if (m_table->horizontalHeader())
    {
        m_table->horizontalHeader()->viewport()->update();
    }
}
