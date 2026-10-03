#include "roster_model.h"

#include "features/roster/ui/roster_constants.h"
#include "ui/shared/qt_text_adapter.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
std::vector<std::u16string> toUtf16Strings(const QStringList& values)
{
    std::vector<std::u16string> result;
    result.reserve(static_cast<std::size_t>(values.size()));
    for (const QString& value : values)
    {
        result.push_back(Ui::QtTextAdapter::toUtf16String(value));
    }
    return result;
}

bool qtCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return Ui::QtTextAdapter::fromUtf16String(left).compare(
        Ui::QtTextAdapter::fromUtf16String(right),
        Qt::CaseInsensitive
        ) == 0;
}

QString domainValidationMessage(const ValidationIssue& issue)
{
    if (issue.code.endsWith(QStringLiteral(".required")))
    {
        return RosterModel::tr("This field is required.");
    }

    if (issue.code == QStringLiteral("validation.length.out_of_bounds"))
    {
        return RosterModel::tr("This value is too long.");
    }

    if (issue.code == QStringLiteral("student_name.korean.too_short")
        || issue.code == QStringLiteral("student_name.korean.too_long"))
    {
        return RosterModel::tr(
            "Korean name has 1 or 5+ syllables. Verify it is correct."
            );
    }

    if (issue.code.contains(QStringLiteral("duplicate")))
    {
        return RosterModel::tr("Duplicate student name pair.");
    }

    return RosterModel::tr("Enter a valid value.");
}
}

RosterModel::RosterModel(
    QObject* parent
    )
    : QAbstractTableModel(parent)
{
    Roster roster;
    setRoster(roster);
}

int RosterModel::rowCount(
    const QModelIndex& parent
    ) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return RosterUi::RowCount;
}

int RosterModel::columnCount(
    const QModelIndex& parent
    ) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_columns.size();
}

QVariant RosterModel::data(
    const QModelIndex& index,
    int role
    ) const
{
    if (!index.isValid())
    {
        return {};
    }

    if (
        index.row() < 0
        || index.row() >= m_rows.size()
        || index.column() < 0
        || index.column() >= m_columns.size()
        )
    {
        return {};
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return m_rows[index.row()][index.column()];
    }

    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignCenter;
    }

    if (role == Qt::ToolTipRole)
    {
        const QStringList errors =
            errorsForCell(
                index.row(),
                index.column()
                );

        if (!errors.isEmpty())
        {
            return errors.join(QLatin1Char('\n'));
        }
    }

    return {};
}

bool RosterModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    int role
    )
{
    if (
        role != Qt::EditRole
        || !index.isValid()
        || index.row() < 0
        || index.row() >= m_rows.size()
        || index.column() < 0
        || index.column() >= m_columns.size()
        )
    {
        return false;
    }

    const QHash<QString, QStringList> oldErrors =
        m_validationErrors;

    const QString normalized =
        normalizeCell(
            value.toString(),
            index.column()
            );

    const bool changed =
        m_rows[index.row()][index.column()] != normalized;

    if (changed)
    {
        m_rows[index.row()][index.column()] =
            normalized;

        m_dirtyCells.insert(
            cellKey(
                index.row(),
                index.column()
                )
            );
    }

    validateCellAt(
        index.row(),
        index.column()
        );
    validateRawInput(
        index.row(),
        index.column(),
        value.toString()
        );
    validateDuplicateNames();

    const bool validationChanged =
        oldErrors != m_validationErrors;

    if (!changed && !validationChanged)
    {
        return false;
    }

    emit dataChanged(
        index,
        index,
        {
            Qt::DisplayRole,
            Qt::EditRole,
            Qt::ToolTipRole
        }
        );

    if (changed)
    {
        setDirty(true);
    }

    return true;
}

Qt::ItemFlags RosterModel::flags(
    const QModelIndex& index
    ) const
{
    Qt::ItemFlags itemFlags =
        QAbstractTableModel::flags(index);

    if (index.isValid())
    {
        const QString column =
            columnName(
                index.column()
                );

        if (!RosterUi::isEvaluationColumn(column))
        {
            itemFlags |= Qt::ItemIsEditable;
        }
    }

    return itemFlags;
}

QVariant RosterModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role
    ) const
{
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    if (orientation == Qt::Horizontal)
    {
        const QString name = columnName(section);

        if (name.compare(QStringLiteral("English"), Qt::CaseInsensitive) == 0)
        {
            return tr("English");
        }
        if (name.compare(QStringLiteral("Korean"), Qt::CaseInsensitive) == 0)
        {
            return tr("Korean");
        }
        if (name.compare(QStringLiteral("Winter"), Qt::CaseInsensitive) == 0)
        {
            return tr("Winter");
        }
        if (name.compare(QStringLiteral("Speech Contest"), Qt::CaseInsensitive) == 0)
        {
            return tr("Speech Contest");
        }
        if (name.compare(QStringLiteral("Summer"), Qt::CaseInsensitive) == 0)
        {
            return tr("Summer");
        }
        if (name.compare(QStringLiteral("Fall"), Qt::CaseInsensitive) == 0)
        {
            return tr("Fall");
        }
        if (name.compare(QStringLiteral("Autumn"), Qt::CaseInsensitive) == 0)
        {
            return tr("Autumn");
        }

        return name;
    }

    return section + 1;
}

void RosterModel::setRoster(
    const Roster& roster
    )
{
    beginResetModel();

    const auto projection =
        ClassMngr::Next::Application::RosterColumnProjection::create(
            toUtf16Strings(roster.columns),
            toUtf16Strings(Roster::BaseColumns),
            qtCaseInsensitiveEquals
            );
    m_columns.clear();
    m_columns.reserve(static_cast<qsizetype>(projection.columns().size()));
    for (const auto& column : projection.columns())
    {
        m_columns.append(Ui::QtTextAdapter::fromUtf16String(column.name));
    }

    rebuildRows(roster, projection);
    validateAll();
    m_domainValidationErrors.clear();

    m_dirtyCells.clear();
    m_dirty = false;

    endResetModel();

    emit dirtyChanged(false);
}

Roster RosterModel::toRoster() const
{
    Roster roster;
    roster.columns = m_columns;
    roster.rows = m_rows;

    return roster;
}

QStringList RosterModel::columnNames() const
{
    return m_columns;
}

bool RosterModel::isDirty() const
{
    return m_dirty;
}

void RosterModel::clearDirty()
{
    m_dirtyCells.clear();
    setDirty(false);
}

QStringList RosterModel::errorsForCell(
    int row,
    int column
    ) const
{
    QStringList errors = m_validationErrors.value(
        cellKey(row, column)
        );

    const QStringList domainErrors = m_domainValidationErrors.value(
        cellKey(row, column)
        );
    for (const QString& error : domainErrors)
    {
        if (!errors.contains(error))
        {
            errors.append(error);
        }
    }

    return errors;
}

void RosterModel::setDomainValidation(const ValidationResult& validation)
{
    QHash<QString, QStringList> errors;
    for (const ValidationIssue& issue : validation.issues())
    {
        if (!issue.isError()
            || issue.row < 0
            || issue.row >= rowCount()
            || issue.column < 0
            || issue.column >= columnCount())
        {
            continue;
        }

        QStringList messages = errors.value(cellKey(issue.row, issue.column));
        const QString message = domainValidationMessage(issue);
        if (!messages.contains(message))
        {
            messages.append(message);
        }
        errors.insert(cellKey(issue.row, issue.column), messages);
    }

    if (m_domainValidationErrors == errors)
    {
        return;
    }

    m_domainValidationErrors = std::move(errors);
    if (rowCount() > 0 && columnCount() > 0)
    {
        emit dataChanged(
            index(0, 0),
            index(rowCount() - 1, columnCount() - 1),
            {Qt::ToolTipRole}
            );
    }
}

QString RosterModel::cellKey(
    int row,
    int column
    ) const
{
    return QStringLiteral("%1:%2")
        .arg(row)
        .arg(column);
}

void RosterModel::setDirty(
    bool dirty
    )
{
    if (m_dirty == dirty)
    {
        return;
    }

    m_dirty = dirty;
    emit dirtyChanged(m_dirty);
}
