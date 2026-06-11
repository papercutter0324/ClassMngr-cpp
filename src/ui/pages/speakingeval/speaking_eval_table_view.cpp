#include "speaking_eval_table_view.h"

#include "models/speaking_evaluation.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QKeySequence>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QUndoCommand>
#include <QUndoStack>

#include <algorithm>
#include <iterator>

namespace
{

class EditCellsCommand : public QUndoCommand
{
public:
    EditCellsCommand(
        QAbstractItemModel* model,
        const QList<SpeakingEvalCellEdit>& changes,
        const QString& description
        )
        : QUndoCommand(description)
        , m_model(model)
        , m_changes(changes)
    {
    }

    void undo() override
    {
        apply(false);
    }

    void redo() override
    {
        apply(true);
    }

private:
    void apply(
        bool useNewValue
        )
    {
        if (!m_model)
        {
            return;
        }

        for (const SpeakingEvalCellEdit& change : m_changes)
        {
            const QModelIndex index =
                m_model->index(
                    change.row,
                    change.column
                    );

            if (!index.isValid())
            {
                continue;
            }

            m_model->setData(
                index,
                useNewValue
                    ? change.newValue
                    : change.oldValue,
                Qt::EditRole
                );
        }
    }

private:
    QAbstractItemModel* m_model = nullptr;
    QList<SpeakingEvalCellEdit> m_changes;
};

} // namespace

SpeakingEvalTableView::SpeakingEvalTableView(
    QWidget* parent
    )
    : QTableView(parent)
{
    setShowGrid(false);
    setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(
        QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed
        | QAbstractItemView::SelectedClicked
        );

    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(SpeakingEval::RowHeight);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    setupShortcuts();
}

void SpeakingEvalTableView::setUndoStack(
    QUndoStack* stack
    )
{
    m_undoStack = stack;
}

void SpeakingEvalTableView::applyChanges(
    const QList<SpeakingEvalCellEdit>& changes,
    const QString& description
    )
{
    if (changes.isEmpty() || !model())
    {
        return;
    }

    if (m_undoStack)
    {
        m_undoStack->push(
            new EditCellsCommand(
                model(),
                changes,
                description
                )
            );

        return;
    }

    for (const SpeakingEvalCellEdit& change : changes)
    {
        const QModelIndex index =
            model()->index(
                change.row,
                change.column
                );

        model()->setData(
            index,
            change.newValue,
            Qt::EditRole
            );
    }
}

void SpeakingEvalTableView::copySelection()
{
    const QString text =
        serializeSelection();

    if (!text.isEmpty())
    {
        QApplication::clipboard()->setText(text);
    }
}

void SpeakingEvalTableView::cutSelection()
{
    QModelIndexList indexes;

    const QString text =
        serializeSelection(&indexes);

    if (text.isEmpty())
    {
        return;
    }

    QApplication::clipboard()->setText(text);

    QList<SpeakingEvalCellEdit> changes;

    for (const QModelIndex& index : indexes)
    {
        addChangeIfValid(
            index,
            QString(),
            changes
            );
    }

    applyChanges(
        changes,
        tr("Cut")
        );
}

void SpeakingEvalTableView::pasteSelection()
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

    if (!start.isValid())
    {
        return;
    }

    QList<SpeakingEvalCellEdit> changes;

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

            addChangeIfValid(
                index,
                values[columnOffset],
                changes
                );
        }
    }

    applyChanges(
        changes,
        tr("Paste")
        );
}

void SpeakingEvalTableView::clearSelectionValues()
{
    QList<SpeakingEvalCellEdit> changes;

    for (const QModelIndex& index : selectedIndexes())
    {
        addChangeIfValid(
            index,
            QString(),
            changes
            );
    }

    applyChanges(
        changes,
        tr("Clear")
        );
}

void SpeakingEvalTableView::fillDown()
{
    if (!model())
    {
        return;
    }

    QMap<int, QModelIndexList> columns;

    for (const QModelIndex& index : selectedIndexes())
    {
        columns[index.column()].append(index);
    }

    QList<SpeakingEvalCellEdit> changes;

    for (auto it = columns.begin(); it != columns.end(); ++it)
    {
        QModelIndexList indexes =
            it.value();

        std::sort(
            indexes.begin(),
            indexes.end(),
            [](const QModelIndex& left, const QModelIndex& right)
            {
                return left.row() < right.row();
            }
            );

        if (indexes.size() < 2)
        {
            continue;
        }

        const QString sourceValue =
            indexes.first().data(Qt::EditRole).toString();

        for (int index = 1; index < indexes.size(); ++index)
        {
            addChangeIfValid(
                indexes[index],
                sourceValue,
                changes
                );
        }
    }

    applyChanges(
        changes,
        tr("Fill Down")
        );
}

void SpeakingEvalTableView::undo()
{
    if (m_undoStack)
    {
        m_undoStack->undo();
    }
}

void SpeakingEvalTableView::redo()
{
    if (m_undoStack)
    {
        m_undoStack->redo();
    }
}

void SpeakingEvalTableView::mousePressEvent(
    QMouseEvent* event
    )
{
    const QModelIndex index =
        indexAt(
            event->position().toPoint()
            );

    if (
        event->button() == Qt::LeftButton
        && index.isValid()
        && isOnFillHandle(
            event->position().toPoint(),
            index
            )
        )
    {
        m_draggingFill = true;
        m_fillStartIndex = index;
        event->accept();
        return;
    }

    QTableView::mousePressEvent(event);
}

void SpeakingEvalTableView::mouseMoveEvent(
    QMouseEvent* event
    )
{
    if (m_draggingFill)
    {
        event->accept();
        return;
    }

    QTableView::mouseMoveEvent(event);
}

void SpeakingEvalTableView::mouseReleaseEvent(
    QMouseEvent* event
    )
{
    if (m_draggingFill)
    {
        const QModelIndex endIndex =
            indexAt(
                event->position().toPoint()
                );

        if (
            m_fillStartIndex.isValid()
            && endIndex.isValid()
            )
        {
            applyDragFill(
                m_fillStartIndex,
                endIndex
                );
        }

        m_draggingFill = false;
        m_fillStartIndex = QModelIndex();
        event->accept();
        return;
    }

    QTableView::mouseReleaseEvent(event);
}

void SpeakingEvalTableView::paintEvent(
    QPaintEvent* event
    )
{
    QTableView::paintEvent(event);

    QPainter painter(viewport());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0));

    constexpr int HandleSize = 6;

    for (const QModelIndex& index : selectedIndexes())
    {
        if (!index.isValid())
        {
            continue;
        }

        const QRect rect =
            visualRect(index);

        if (!rect.isValid())
        {
            continue;
        }

        painter.drawRect(
            QRect(
                rect.right() - HandleSize,
                rect.bottom() - HandleSize,
                HandleSize,
                HandleSize
                )
            );
    }
}

void SpeakingEvalTableView::setupShortcuts()
{
    auto addShortcut =
        [this](const QKeySequence& sequence, auto slot)
        {
            auto* shortcut =
                new QShortcut(
                    sequence,
                    this
                    );

            shortcut->setContext(
                Qt::WidgetWithChildrenShortcut
                );

            connect(
                shortcut,
                &QShortcut::activated,
                this,
                slot
                );
        };

    addShortcut(QKeySequence::Copy, &SpeakingEvalTableView::copySelection);
    addShortcut(QKeySequence::Cut, &SpeakingEvalTableView::cutSelection);
    addShortcut(QKeySequence::Paste, &SpeakingEvalTableView::pasteSelection);
    addShortcut(QKeySequence::Delete, &SpeakingEvalTableView::clearSelectionValues);
    addShortcut(QKeySequence::Undo, &SpeakingEvalTableView::undo);
    addShortcut(QKeySequence::Redo, &SpeakingEvalTableView::redo);
    addShortcut(QKeySequence(QStringLiteral("Ctrl+D")), &SpeakingEvalTableView::fillDown);
}

QList<SpeakingEvalCellEdit> SpeakingEvalTableView::changesForIndexes(
    const QModelIndexList& indexes,
    const QString& newValue
    ) const
{
    QList<SpeakingEvalCellEdit> changes;

    for (const QModelIndex& index : indexes)
    {
        addChangeIfValid(
            index,
            newValue,
            changes
            );
    }

    return changes;
}

bool SpeakingEvalTableView::addChangeIfValid(
    const QModelIndex& index,
    const QString& newValue,
    QList<SpeakingEvalCellEdit>& changes
    ) const
{
    if (
        !index.isValid()
        || !model()
        || !(model()->flags(index) & Qt::ItemIsEditable)
        )
    {
        return false;
    }

    const QString oldValue =
        index.data(Qt::EditRole).toString();

    if (oldValue == newValue)
    {
        return false;
    }

    changes.append(
        {
            index.row(),
            index.column(),
            oldValue,
            newValue
        }
        );

    return true;
}

QString SpeakingEvalTableView::serializeSelection(
    QModelIndexList* sortedIndexes
    ) const
{
    QModelIndexList indexes =
        selectedIndexes();

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

bool SpeakingEvalTableView::isOnFillHandle(
    const QPoint& position,
    const QModelIndex& index
    ) const
{
    const QRect rect =
        visualRect(index);

    constexpr int HandleSize = 6;

    const QRect handleRect(
        rect.right() - HandleSize,
        rect.bottom() - HandleSize,
        HandleSize,
        HandleSize
        );

    return handleRect.contains(position);
}

void SpeakingEvalTableView::applyDragFill(
    const QModelIndex& start,
    const QModelIndex& end
    )
{
    if (!model())
    {
        return;
    }

    QList<SpeakingEvalCellEdit> changes;

    const QString value =
        start.data(Qt::EditRole).toString();

    const int rowStep =
        end.row() >= start.row()
            ? 1
            : -1;

    const int columnStep =
        end.column() >= start.column()
            ? 1
            : -1;

    if (start.column() == end.column())
    {
        for (int row = start.row(); row != end.row() + rowStep; row += rowStep)
        {
            addChangeIfValid(
                model()->index(
                    row,
                    start.column()
                    ),
                value,
                changes
                );
        }
    }
    else if (start.row() == end.row())
    {
        const auto startColumn =
            SpeakingEval::columnFromInt(
                start.column()
                );

        if (!SpeakingEval::isScoringColumn(startColumn))
        {
            return;
        }

        for (
            int column = start.column();
            column != end.column() + columnStep;
            column += columnStep
            )
        {
            const auto targetColumn =
                SpeakingEval::columnFromInt(column);

            if (!SpeakingEval::isScoringColumn(targetColumn))
            {
                continue;
            }

            addChangeIfValid(
                model()->index(
                    start.row(),
                    column
                    ),
                value,
                changes
                );
        }
    }

    applyChanges(
        changes,
        tr("Fill")
        );
}
