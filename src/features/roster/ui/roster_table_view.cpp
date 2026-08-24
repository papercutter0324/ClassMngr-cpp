#include "roster_table_view.h"

#include "ui/shared/styles/roles.h"
#include "features/roster/ui/roster_column_layout_controller.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>

#include <algorithm>
#include <iterator>

namespace
{

QColor destinationIndicatorColor()
{
    return QColor(
        236,
        72,
        153
        );
}

class RosterVerticalHeaderView : public QHeaderView
{
public:
    explicit RosterVerticalHeaderView(
        RosterTableView* table
        )
        : QHeaderView(Qt::Vertical, table),
          m_table(table)
    {
        setProperty(
            "role",
            UiRoles::RosterVerticalHeader
            );
    }

protected:
    void mousePressEvent(
        QMouseEvent* event
        ) override
    {
        if (event && event->button() == Qt::LeftButton)
        {
            const int row =
                sectionRowAtPosition(
                    event->position().toPoint()
                    );

            if (row >= 0 && m_table && m_table->model())
            {
                m_pressedRow = row;
                m_dropRow = row;
                m_dragStartPosition =
                    event->position().toPoint();

                m_table->setCurrentIndex(
                    m_table->model()->index(
                        row,
                        0
                        )
                    );

                event->accept();
                return;
            }
        }

        QHeaderView::mousePressEvent(event);
    }

    void mouseMoveEvent(
        QMouseEvent* event
        ) override
    {
        if (
            event
            && m_pressedRow >= 0
            && (event->buttons() & Qt::LeftButton)
            )
        {
            const QPoint position =
                event->position().toPoint();

            if (!m_dragging)
            {
                const int distance =
                    (position - m_dragStartPosition).manhattanLength();

                if (distance < QApplication::startDragDistance())
                {
                    event->accept();
                    return;
                }

                m_dragging = true;
                m_table->setDraggedSourceRow(
                    m_pressedRow
                    );
                setCursor(Qt::ClosedHandCursor);
            }

            updateDropRow(position);
            event->accept();
            return;
        }

        QHeaderView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(
        QMouseEvent* event
        ) override
    {
        if (event && event->button() == Qt::LeftButton && m_pressedRow >= 0)
        {
            const int sourceRow =
                m_pressedRow;

            const int destinationRow =
                m_dragging
                    ? dropRowAtPosition(
                        event->position().toPoint()
                        )
                    : -1;

            const bool wasDragging =
                m_dragging;

            resetDrag();

            if (
                wasDragging
                && destinationRow >= 0
                && destinationRow != sourceRow
                && m_table
                )
            {
                m_table->requestRowMove(
                    sourceRow,
                    destinationRow
                    );
            }

            event->accept();
            return;
        }

        QHeaderView::mouseReleaseEvent(event);
    }

    void paintEvent(
        QPaintEvent* event
        ) override
    {
        QHeaderView::paintEvent(event);

        if (!m_table || !m_table->viewport())
        {
            return;
        }

        const int bottomEdge =
            m_table->contentBottomEdge();

        QPainter painter(viewport());

        if (bottomEdge >= 0 && bottomEdge < viewport()->height() - 1)
        {
            painter.fillRect(
                QRect(
                    0,
                    bottomEdge + 1,
                    viewport()->width(),
                    viewport()->height() - bottomEdge - 1
                    ),
                m_table->viewport()->palette().brush(QPalette::Base)
                );
        }

        paintSourceIndicator(painter);
        paintDropIndicator(painter);
    }

private:
    int sectionRowAtPosition(
        const QPoint& position
        ) const
    {
        if (!m_table || !m_table->model())
        {
            return -1;
        }

        const int row =
            logicalIndexAt(
                position.y()
                );

        if (
            row < 0
            || row >= m_table->model()->rowCount()
            )
        {
            return -1;
        }

        return row;
    }

    int dropRowAtPosition(
        const QPoint& position
        ) const
    {
        const int row =
            sectionRowAtPosition(position);

        if (row >= 0)
        {
            return row;
        }

        if (!m_table || !m_table->model())
        {
            return -1;
        }

        const int rowCount =
            m_table->model()->rowCount();

        if (rowCount <= 0)
        {
            return -1;
        }

        if (position.y() < 0)
        {
            return 0;
        }

        const int bottomEdge =
            m_table->contentBottomEdge();

        if (bottomEdge >= 0 && position.y() > bottomEdge)
        {
            return rowCount - 1;
        }

        return -1;
    }

    void updateDropRow(
        const QPoint& position
        )
    {
        const int previousDropRow =
            m_dropRow;

        m_dropRow =
            dropRowAtPosition(position);

        if (m_table)
        {
            m_table->setDraggedDestinationRow(
                m_dropRow >= 0 && m_dropRow != m_pressedRow
                    ? m_dropRow
                    : -1
                );
        }

        if (m_dropRow != previousDropRow && viewport())
        {
            viewport()->update();
        }
    }

    void resetDrag()
    {
        if (m_table)
        {
            m_table->setDraggedSourceRow(-1);
            m_table->setDraggedDestinationRow(-1);
        }

        m_pressedRow = -1;
        m_dropRow = -1;
        m_dragging = false;
        m_dragStartPosition = QPoint();
        unsetCursor();

        if (viewport())
        {
            viewport()->update();
        }
    }

    QRect rowRect(
        int row
        ) const
    {
        if (
            row < 0
            || !m_table
            || !m_table->model()
            || row >= m_table->model()->rowCount()
            )
        {
            return {};
        }

        return QRect(
            0,
            sectionViewportPosition(row),
            viewport()->width(),
            sectionSize(row)
            );
    }

    void paintSourceIndicator(
        QPainter& painter
        )
    {
        if (!m_table)
        {
            return;
        }

        const int sourceRow =
            m_table->draggedSourceRow();

        const QRect sourceRect =
            rowRect(sourceRow);

        if (!sourceRect.isValid())
        {
            return;
        }

        const QColor highlight =
            palette().color(QPalette::Highlight);

        QColor fill =
            highlight;
        fill.setAlpha(36);

        painter.fillRect(
            sourceRect.adjusted(
                1,
                1,
                -1,
                -1
                ),
            fill
            );

        QColor edge =
            highlight;
        edge.setAlpha(180);

        painter.fillRect(
            QRect(
                sourceRect.left() + 1,
                sourceRect.top() + 1,
                3,
                sourceRect.height() - 2
                ),
            edge
            );
    }

    void paintDropIndicator(
        QPainter& painter
        )
    {
        if (!m_table)
        {
            return;
        }

        const int destinationRow =
            m_table->draggedDestinationRow();

        if (!m_dragging || destinationRow < 0)
        {
            return;
        }

        const QRect dropRect =
            rowRect(destinationRow);

        if (!dropRect.isValid())
        {
            return;
        }

        const QColor highlight =
            destinationIndicatorColor();

        QColor fill =
            highlight;
        fill.setAlpha(48);

        painter.fillRect(
            dropRect.adjusted(
                1,
                1,
                -1,
                -1
                ),
            fill
            );

        painter.setPen(
            QPen(
                highlight,
                2
                )
            );

        painter.drawRect(
            dropRect.adjusted(
                1,
                1,
                -2,
                -2
                )
            );
    }

private:
    RosterTableView* m_table = nullptr;
    QPoint m_dragStartPosition;
    int m_pressedRow = -1;
    int m_dropRow = -1;
    bool m_dragging = false;
};

bool isClipboardShortcut(
    const QKeyEvent* event
    )
{
    return event
        && (
            event->matches(QKeySequence::Copy)
            || event->matches(QKeySequence::Cut)
            || event->matches(QKeySequence::Paste)
            );
}

} // namespace

RosterTableView::RosterTableView(
    QWidget* parent
    )
    : QTableView(parent)
{
    setProperty(
        "role",
        UiRoles::RosterTable
        );

    setShowGrid(false);
    setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(
        QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed
        | QAbstractItemView::AnyKeyPressed
        | QAbstractItemView::SelectedClicked
        );

    setVerticalHeader(
        new RosterVerticalHeaderView(this)
        );

    verticalHeader()->setVisible(true);
    verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    updateVerticalHeaderTrailingBackground();
}

void RosterTableView::setLayoutController(
    RosterColumnLayoutController* controller
    )
{
    m_controller = controller;
    viewport()->update();
}

void RosterTableView::requestRowMove(
    int sourceRow,
    int destinationRow
    )
{
    if (sourceRow == destinationRow)
    {
        return;
    }

    emit rowMoveRequested(
        sourceRow,
        destinationRow
        );
}

void RosterTableView::setDraggedSourceRow(
    int row
    )
{
    if (
        row >= 0
        && (!model() || row >= model()->rowCount())
        )
    {
        row = -1;
    }

    if (m_draggedSourceRow == row)
    {
        return;
    }

    const int previousRow =
        m_draggedSourceRow;

    m_draggedSourceRow = row;

    updateRowIndicator(previousRow);
    updateRowIndicator(m_draggedSourceRow);
}

int RosterTableView::draggedSourceRow() const
{
    return m_draggedSourceRow;
}

void RosterTableView::setDraggedDestinationRow(
    int row
    )
{
    if (
        row >= 0
        && (!model() || row >= model()->rowCount())
        )
    {
        row = -1;
    }

    if (m_draggedDestinationRow == row)
    {
        return;
    }

    const int previousRow =
        m_draggedDestinationRow;

    m_draggedDestinationRow = row;

    updateRowIndicator(previousRow);
    updateRowIndicator(m_draggedDestinationRow);
}

int RosterTableView::draggedDestinationRow() const
{
    return m_draggedDestinationRow;
}

void RosterTableView::copy()
{
    const QString text =
        serializeSelection();

    if (!text.isEmpty())
    {
        QApplication::clipboard()->setText(text);
    }
}

void RosterTableView::cut()
{
    QModelIndexList indexes;

    const QString text =
        serializeSelection(&indexes);

    if (text.isEmpty())
    {
        return;
    }

    const bool hasEditableIndex =
        std::any_of(
            indexes.constBegin(),
            indexes.constEnd(),
            [this](const QModelIndex& index)
            {
                return isEditableIndex(index);
            }
            );

    if (!hasEditableIndex)
    {
        return;
    }

    QApplication::clipboard()->setText(text);

    for (const QModelIndex& index : indexes)
    {
        if (!isEditableIndex(index))
        {
            continue;
        }

        model()->setData(
            index,
            QString(),
            Qt::EditRole
            );
    }
}

void RosterTableView::paste()
{
    if (!model())
    {
        return;
    }

    QString text =
        QApplication::clipboard()->text();

    if (text.isEmpty())
    {
        return;
    }

    while (text.endsWith(QLatin1Char('\n')) || text.endsWith(QLatin1Char('\r')))
    {
        text.chop(1);
    }

    const QModelIndex start =
        currentIndex();

    if (!isEditableIndex(start))
    {
        return;
    }

    const QStringList rows =
        text.split(QLatin1Char('\n'));

    for (int rowOffset = 0; rowOffset < rows.size(); ++rowOffset)
    {
        QString rowText =
            rows[rowOffset];

        if (rowText.endsWith(QLatin1Char('\r')))
        {
            rowText.chop(1);
        }

        const QStringList values =
            rowText.split(QLatin1Char('\t'));

        for (int columnOffset = 0; columnOffset < values.size(); ++columnOffset)
        {
            const QModelIndex index =
                model()->index(
                    start.row() + rowOffset,
                    start.column() + columnOffset
                    );

            if (!isEditableIndex(index))
            {
                continue;
            }

            model()->setData(
                index,
                values[columnOffset],
                Qt::EditRole
                );
        }
    }
}

void RosterTableView::clearSelectionValues()
{
    QModelIndexList indexes =
        selectedIndexes();

    if (indexes.isEmpty() && currentIndex().isValid())
    {
        indexes.append(
            currentIndex()
            );
    }

    for (const QModelIndex& index : indexes)
    {
        if (!isEditableIndex(index))
        {
            continue;
        }

        model()->setData(
            index,
            QString(),
            Qt::EditRole
            );
    }
}

bool RosterTableView::event(
    QEvent* event
    )
{
    if (
        event->type() == QEvent::ShortcutOverride
        && state() != QAbstractItemView::EditingState
        && isClipboardShortcut(
            static_cast<QKeyEvent*>(event)
            )
        )
    {
        event->accept();
        return true;
    }

    return QTableView::event(event);
}

void RosterTableView::keyPressEvent(
    QKeyEvent* event
    )
{
    if (state() != QAbstractItemView::EditingState)
    {
        if (event->matches(QKeySequence::Copy))
        {
            copy();
            event->accept();
            return;
        }

        if (event->matches(QKeySequence::Cut))
        {
            cut();
            event->accept();
            return;
        }

        if (event->matches(QKeySequence::Paste))
        {
            paste();
            event->accept();
            return;
        }

        if (event->matches(QKeySequence::Delete))
        {
            clearSelectionValues();
            event->accept();
            return;
        }
    }

    QTableView::keyPressEvent(event);
}

void RosterTableView::changeEvent(
    QEvent* event
    )
{
    QTableView::changeEvent(event);

    if (
        event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange
        || event->type() == QEvent::StyleChange
        )
    {
        updateVerticalHeaderTrailingBackground();
    }
}

void RosterTableView::paintEvent(
    QPaintEvent* event
    )
{
    QTableView::paintEvent(event);

    if (!model())
    {
        return;
    }

    const int bottomEdge =
        contentBottomEdge();

    if (bottomEdge < 0)
    {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false);

    paintDraggedSourceRow(painter);
    paintDraggedDestinationRow(painter);

    if (!m_controller)
    {
        return;
    }

    painter.setPen(
        QPen(palette().color(QPalette::Dark), 2)
        );

    for (int column = 0; column < model()->columnCount(); ++column)
    {
        if (m_controller->isGroupBoundaryBefore(column))
        {
            const int x =
                columnViewportPosition(column);

            painter.drawLine(
                x,
                0,
                x,
                bottomEdge
                );
        }

        if (!m_controller->isGroupBoundaryAfter(column))
        {
            continue;
        }

        const int x =
            columnViewportPosition(column)
            + columnWidth(column)
            - 1;

        painter.drawLine(
            x,
            0,
            x,
            bottomEdge
            );
    }
}

int RosterTableView::contentBottomEdge() const
{
    if (!model() || model()->rowCount() <= 0)
    {
        return -1;
    }

    const int lastRow =
        model()->rowCount() - 1;

    return rowViewportPosition(lastRow)
        + rowHeight(lastRow)
        - 1;
}

bool RosterTableView::isEditableIndex(
    const QModelIndex& index
    ) const
{
    return index.isValid()
        && model()
        && index.model() == model()
        && (model()->flags(index) & Qt::ItemIsEditable);
}

QString RosterTableView::serializeSelection(
    QModelIndexList* sortedIndexes
    ) const
{
    QModelIndexList indexes =
        selectedIndexes();

    if (indexes.isEmpty() && currentIndex().isValid())
    {
        indexes.append(
            currentIndex()
            );
    }

    std::sort(
        indexes.begin(),
        indexes.end(),
        [](const QModelIndex& left, const QModelIndex& right)
        {
            if (left.row() == right.row())
            {
                return left.column() < right.column();
            }

            return left.row() < right.row();
        }
        );

    if (sortedIndexes)
    {
        *sortedIndexes = indexes;
    }

    if (indexes.isEmpty())
    {
        return {};
    }

    QMap<int, QMap<int, QString>> rows;

    for (const QModelIndex& index : indexes)
    {
        rows[index.row()][index.column()] =
            index.data(Qt::DisplayRole).toString();
    }

    QStringList lines;

    for (auto rowIt = rows.constBegin(); rowIt != rows.constEnd(); ++rowIt)
    {
        const auto& columns =
            rowIt.value();

        if (columns.isEmpty())
        {
            continue;
        }

        const int minColumn =
            columns.constBegin().key();

        const int maxColumn =
            std::prev(columns.constEnd()).key();

        QStringList values;

        for (int column = minColumn; column <= maxColumn; ++column)
        {
            values.append(
                columns.value(column)
                );
        }

        lines.append(
            values.join(QLatin1Char('\t'))
            );
    }

    return lines.join(QLatin1Char('\n'));
}

void RosterTableView::updateVerticalHeaderTrailingBackground()
{
    if (!verticalHeader() || !verticalHeader()->viewport())
    {
        return;
    }

    verticalHeader()->viewport()->update();
}

void RosterTableView::updateRowIndicator(
    int row
    )
{
    if (!model() || row < 0 || row >= model()->rowCount())
    {
        return;
    }

    const int y =
        rowViewportPosition(row);

    const int height =
        rowHeight(row);

    if (viewport())
    {
        viewport()->update(
            QRect(
                0,
                y,
                viewport()->width(),
                height
                )
            );
    }

    if (verticalHeader() && verticalHeader()->viewport())
    {
        verticalHeader()->viewport()->update(
            QRect(
                0,
                verticalHeader()->sectionViewportPosition(row),
                verticalHeader()->viewport()->width(),
                verticalHeader()->sectionSize(row)
                )
            );
    }
}

void RosterTableView::paintDraggedSourceRow(
    QPainter& painter
    ) const
{
    if (
        m_draggedSourceRow < 0
        || !model()
        || m_draggedSourceRow >= model()->rowCount()
        )
    {
        return;
    }

    const int y =
        rowViewportPosition(m_draggedSourceRow);

    const int height =
        rowHeight(m_draggedSourceRow);

    if (height <= 0 || y + height < 0 || y > viewport()->height())
    {
        return;
    }

    const QRect rowRect(
        0,
        y,
        viewport()->width(),
        height
        );

    const QColor highlight =
        palette().color(QPalette::Highlight);

    QColor fill =
        highlight;
    fill.setAlpha(36);

    painter.fillRect(
        rowRect.adjusted(
            1,
            1,
            -1,
            -1
            ),
        fill
        );

    QColor outline =
        highlight;
    outline.setAlpha(190);

    painter.setPen(
        QPen(
            outline,
            2
            )
        );

    painter.drawRect(
        rowRect.adjusted(
            1,
            1,
            -2,
            -2
            )
        );
}

void RosterTableView::paintDraggedDestinationRow(
    QPainter& painter
    ) const
{
    if (
        m_draggedDestinationRow < 0
        || !model()
        || m_draggedDestinationRow >= model()->rowCount()
        )
    {
        return;
    }

    const int y =
        rowViewportPosition(m_draggedDestinationRow);

    const int height =
        rowHeight(m_draggedDestinationRow);

    if (height <= 0 || y + height < 0 || y > viewport()->height())
    {
        return;
    }

    const QRect rowRect(
        0,
        y,
        viewport()->width(),
        height
        );

    const QColor highlight =
        destinationIndicatorColor();

    QColor fill =
        highlight;
    fill.setAlpha(46);

    painter.fillRect(
        rowRect.adjusted(
            1,
            1,
            -1,
            -1
            ),
        fill
        );

    QColor outline =
        highlight;
    outline.setAlpha(205);

    painter.setPen(
        QPen(
            outline,
            2
            )
        );

    painter.drawRect(
        rowRect.adjusted(
            2,
            2,
            -3,
            -3
            )
        );
}
