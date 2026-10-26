#pragma once

#include <QAbstractTableModel>
#include <QStringList>
#include <QVector>

#include <limits>
#include <stdexcept>
#include <utility>

namespace ClassMngr::Ui
{

// Flat text cells with feature-owned row metadata. Row must be default
// constructible and contain the supplied QStringList member. Views paint the
// cells and create editors on demand; this model creates no per-cell QObject.
// It retains all supplied rows and does not bound backing-data memory or page
// a data source. Feature code owns loading, refresh, and persistence rules.
template <typename Row>
class EditableTextTableModel : public QAbstractTableModel
{
public:
    using CellsMember = QStringList Row::*;

    explicit EditableTextTableModel(
        const int columnCount,
        const CellsMember cells,
        const Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter,
        QObject* parent = nullptr
        )
        : QAbstractTableModel(parent)
        , m_columnCount(columnCount)
        , m_cells(cells)
        , m_alignment(alignment)
    {
        if (columnCount <= 0 || cells == nullptr)
        {
            throw std::invalid_argument("A text table needs columns and a cell member.");
        }
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_columnCount;
    }

    QVariant data(const QModelIndex& index, const int role) const override
    {
        if (!isValidCell(index))
        {
            return {};
        }
        if (role == Qt::DisplayRole || role == Qt::EditRole)
        {
            return (rowAt(index.row()).*m_cells).at(index.column());
        }
        if (role == Qt::TextAlignmentRole)
        {
            return static_cast<int>(m_alignment);
        }
        return {};
    }

    QVariant headerData(
        const int section,
        const Qt::Orientation orientation,
        const int role = Qt::DisplayRole
        ) const override
    {
        if (role != Qt::DisplayRole || section < 0
            || section >= (orientation == Qt::Horizontal ? m_columnCount : rowCount()))
        {
            return {};
        }
        if (orientation == Qt::Horizontal && section < m_headers.size())
        {
            return m_headers.at(section);
        }
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!isValidCell(index))
        {
            return Qt::NoItemFlags;
        }
        Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (m_editingEnabled)
        {
            result |= Qt::ItemIsEditable;
        }
        return result;
    }

    bool setData(
        const QModelIndex& index,
        const QVariant& value,
        const int role = Qt::EditRole
        ) override
    {
        if (!m_editingEnabled || role != Qt::EditRole || !isValidCell(index))
        {
            return false;
        }
        QStringList& cells = m_rows[index.row()].*m_cells;
        const QString text = value.toString();
        if (cells.at(index.column()) == text)
        {
            return true;
        }
        cells[index.column()] = text;
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }

    bool insertRows(
        const int row,
        const int count,
        const QModelIndex& parent = QModelIndex()
        ) override
    {
        if (!m_editingEnabled || parent.isValid() || row < 0
            || row > rowCount() || count <= 0
            || count > std::numeric_limits<int>::max() - rowCount())
        {
            return false;
        }

        beginInsertRows(QModelIndex(), row, row + count - 1);
        for (int offset = 0; offset < count; ++offset)
        {
            Row newRow;
            (newRow.*m_cells).fill(QString(), m_columnCount);
            m_rows.insert(row + offset, std::move(newRow));
        }
        endInsertRows();
        return true;
    }

    bool removeRows(
        const int row,
        const int count,
        const QModelIndex& parent = QModelIndex()
        ) override
    {
        if (!m_editingEnabled || parent.isValid() || row < 0
            || row >= rowCount() || count <= 0 || count > rowCount() - row)
        {
            return false;
        }

        beginRemoveRows(QModelIndex(), row, row + count - 1);
        m_rows.remove(row, count);
        endRemoveRows();
        return true;
    }

    // Reject a malformed rectangle before reset notification or mutation.
    // A successful replacement releases the previous generation on return.
    void setRows(QVector<Row> rows)
    {
        if (rows.size() > std::numeric_limits<int>::max())
        {
            throw std::length_error("A text table cannot expose more than INT_MAX rows.");
        }
        for (const Row& row : rows)
        {
            if ((row.*m_cells).size() != m_columnCount)
            {
                throw std::invalid_argument("Text table rows must match the column count.");
            }
        }
        beginResetModel();
        m_rows.swap(rows);
        endResetModel();
    }

    void setHeaders(QStringList headers)
    {
        if (headers.size() > m_columnCount)
        {
            throw std::invalid_argument("Text table headers exceed the column count.");
        }
        if (m_headers == headers)
        {
            return;
        }
        m_headers = std::move(headers);
        emit headerDataChanged(Qt::Horizontal, 0, m_columnCount - 1);
    }

    void setEditingEnabled(const bool enabled) noexcept
    {
        m_editingEnabled = enabled;
    }

    [[nodiscard]] const Row& rowAt(const int row) const
    {
        return m_rows.at(row);
    }

protected:
    [[nodiscard]] bool isValidCell(const QModelIndex& index) const noexcept
    {
        return index.isValid() && index.model() == this
            && index.row() >= 0 && index.row() < rowCount()
            && index.column() >= 0 && index.column() < m_columnCount;
    }

private:
    int m_columnCount;
    CellsMember m_cells;
    Qt::Alignment m_alignment;
    bool m_editingEnabled = true;
    QVector<Row> m_rows;
    QStringList m_headers;
};

}  // namespace ClassMngr::Ui
