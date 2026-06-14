#include "roster_table_view.h"

#include "ui/pages/roster/roster_column_layout_controller.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMap>
#include <QPainter>
#include <QPalette>

#include <algorithm>
#include <iterator>

namespace
{

class RosterVerticalHeaderView : public QHeaderView
{
public:
    explicit RosterVerticalHeaderView(
        RosterTableView* table
        )
        : QHeaderView(Qt::Vertical, table),
          m_table(table)
    {
    }

protected:
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

        if (bottomEdge < 0 || bottomEdge >= viewport()->height() - 1)
        {
            return;
        }

        QPainter painter(viewport());
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

private:
    RosterTableView* m_table = nullptr;
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
    setShowGrid(false);
    setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::SingleSelection);
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

    if (!model() || !m_controller)
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
    painter.setPen(
        QPen(QColor(55, 65, 81), 2)
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
