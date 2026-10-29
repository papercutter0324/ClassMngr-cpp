#include "next/ui/speaking_evaluation_model.h"

#include "next/application/speaking_evaluation_validation.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QPalette>
#include <QUndoCommand>
#include <QStringView>

#include <algorithm>
#include <array>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

QString fromUtf16(const std::u16string& value)
{
    return QString::fromUtf16(
        value.data(), static_cast<qsizetype>(value.size()));
}

QString cellKey(const int row, const int column)
{
    return QStringLiteral("%1:%2").arg(row).arg(column);
}

QString translatedModelText(const char* source)
{
    return QCoreApplication::translate("SpeakingEvalModel", source);
}

QColor validationColor()
{
    const QColor base = QApplication::palette().color(QPalette::Base);
    QColor tint(220, 38, 38);
    tint.setAlpha(32);
    QColor result = base;
    result.setRed((base.red() * 7 + tint.red()) / 8);
    result.setGreen((base.green() * 7 + tint.green()) / 8);
    result.setBlue((base.blue() * 7 + tint.blue()) / 8);
    return result;
}

} // namespace

class SpeakingEvaluationCellEditCommand final : public QUndoCommand
{
public:
    SpeakingEvaluationCellEditCommand(
        SpeakingEvaluationTableModel& model,
        const int row,
        const int column,
        QString before,
        QString after
        )
        : m_model(model)
        , m_row(row)
        , m_column(column)
        , m_before(std::move(before))
        , m_after(std::move(after))
    {
        setText(QStringLiteral("Edit evaluation cell"));
    }

    void undo() override
    {
        m_model.applyCommandValue(m_row, m_column, m_before);
    }

    void redo() override
    {
        m_model.applyCommandValue(m_row, m_column, m_after);
    }

private:
    SpeakingEvaluationTableModel& m_model;
    int m_row;
    int m_column;
    QString m_before;
    QString m_after;
};

namespace
{

} // namespace

SpeakingEvaluationClassListModel::SpeakingEvaluationClassListModel(
    QObject* parent
    )
    : QAbstractListModel(parent)
{
}

int SpeakingEvaluationClassListModel::rowCount(
    const QModelIndex& parent
    ) const
{
    return parent.isValid()
        ? 0
        : static_cast<int>(m_snapshot.classes.size());
}

QVariant SpeakingEvaluationClassListModel::data(
    const QModelIndex& index,
    const int role
    ) const
{
    if (!index.isValid() || index.row() < 0
        || static_cast<std::size_t>(index.row()) >= m_snapshot.classes.size())
    {
        return {};
    }
    const auto& entry = m_snapshot.classes[static_cast<std::size_t>(index.row())];
    if (role == Qt::DisplayRole)
    {
        return fromUtf16(entry.className);
    }
    if (role == ClassIdRole)
    {
        return QString::fromStdString(entry.classId.value());
    }
    return {};
}

Qt::ItemFlags SpeakingEvaluationClassListModel::flags(
    const QModelIndex& index
    ) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    return QAbstractListModel::flags(index) | Qt::ItemIsSelectable;
}

void SpeakingEvaluationClassListModel::setClasses(
    Application::ClassesListSnapshot snapshot
    )
{
    beginResetModel();
    m_snapshot = std::move(snapshot);
    endResetModel();
}

void SpeakingEvaluationClassListModel::clear()
{
    setClasses({});
}

std::optional<Domain::ClassId> SpeakingEvaluationClassListModel::classIdAt(
    const int row
    ) const
{
    if (row < 0 || static_cast<std::size_t>(row) >= m_snapshot.classes.size())
    {
        return std::nullopt;
    }
    return m_snapshot.classes[static_cast<std::size_t>(row)].classId;
}

int SpeakingEvaluationClassListModel::rowForClassId(
    const Domain::ClassId& classId
    ) const
{
    for (std::size_t index = 0; index < m_snapshot.classes.size(); ++index)
    {
        if (m_snapshot.classes[index].classId == classId)
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

std::size_t SpeakingEvaluationClassListModel::summaryCount() const noexcept
{
    return m_snapshot.classes.size();
}

SpeakingEvaluationTableModel::SpeakingEvaluationTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int SpeakingEvaluationTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() || !m_rows
        ? 0
        : Application::SpeakingEvaluationRowCount;
}

int SpeakingEvaluationTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() || !m_rows
        ? 0
        : Application::SpeakingEvaluationColumnCount;
}

QVariant SpeakingEvaluationTableModel::data(
    const QModelIndex& index,
    const int role
    ) const
{
    if (!m_rows || !index.isValid() || index.row() < 0
        || index.row() >= Application::SpeakingEvaluationRowCount
        || index.column() < 0
        || index.column() >= Application::SpeakingEvaluationColumnCount)
    {
        return {};
    }
    if (role == Qt::DisplayRole && index.column() == 0)
    {
        return index.row() + 1;
    }
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return m_rows->at(static_cast<std::size_t>(index.row()))
            .at(index.column());
    }
    if (role == Qt::TextAlignmentRole)
    {
        return static_cast<int>(index.column() == 1 || index.column() == 9
                || index.column() == 10
            ? Qt::AlignLeft | Qt::AlignVCenter
            : Qt::AlignCenter);
    }
    const QStringList errors = errorsForCell(index.row(), index.column());
    if (role == Qt::ToolTipRole && !errors.isEmpty())
    {
        return errors.join(QLatin1Char('\n'));
    }
    if (role == Qt::BackgroundRole && !errors.isEmpty())
    {
        return validationColor();
    }
    return {};
}

bool SpeakingEvaluationTableModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    const int role
    )
{
    if (role != Qt::EditRole || !index.isValid())
    {
        return false;
    }
    return setCellValue(index.row(), index.column(), value.toString());
}

Qt::ItemFlags SpeakingEvaluationTableModel::flags(
    const QModelIndex& index
    ) const
{
    Qt::ItemFlags itemFlags = QAbstractTableModel::flags(index);
    if (!m_rows || !index.isValid())
    {
        return itemFlags & ~Qt::ItemIsEditable;
    }
    if (index.column() > 0)
    {
        itemFlags |= Qt::ItemIsEditable;
    }
    return itemFlags;
}

QVariant SpeakingEvaluationTableModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role
    ) const
{
    if (role != Qt::DisplayRole)
    {
        return {};
    }
    if (orientation == Qt::Vertical)
    {
        return section >= 0 && section < Application::SpeakingEvaluationRowCount
            ? QVariant(section + 1)
            : QVariant{};
    }
    if (section < 0 || section >= Application::SpeakingEvaluationColumnCount)
    {
        return {};
    }
    static constexpr std::array<const char*,
        Application::SpeakingEvaluationColumnCount> headers{
        "", "English Name", "Korean Name", "Grammar", "Pronunciation",
        "Fluency", "Manner", "Content", "Overall Effort", "Comments",
        "Notes"
    };
    return translatedModelText(headers[static_cast<std::size_t>(section)]);
}

bool SpeakingEvaluationTableModel::loadRows(
    const std::vector<std::vector<std::u16string>>& rows
    )
{
    if (rows.size() > static_cast<std::size_t>(Application::SpeakingEvaluationRowCount)
        || std::ranges::any_of(rows, [](const auto& row) {
            return row.size()
                > static_cast<std::size_t>(Application::SpeakingEvaluationColumnCount);
        }))
    {
        return false;
    }

    beginResetModel();
    std::vector<QStringList> matrix;
    matrix.reserve(Application::SpeakingEvaluationRowCount);
    for (int rowIndex = 0; rowIndex < Application::SpeakingEvaluationRowCount;
         ++rowIndex)
    {
        QStringList row;
        row.reserve(Application::SpeakingEvaluationColumnCount);
        for (int column = 0; column < Application::SpeakingEvaluationColumnCount;
             ++column)
        {
            QString value;
            if (static_cast<std::size_t>(rowIndex) < rows.size()
                && static_cast<std::size_t>(column)
                    < rows[static_cast<std::size_t>(rowIndex)].size())
            {
                value = fromUtf16(rows[static_cast<std::size_t>(rowIndex)]
                    [static_cast<std::size_t>(column)]);
            }
            row.append(std::move(value));
        }
        matrix.push_back(std::move(row));
    }
    m_rows.emplace(std::move(matrix));
    m_originalValues.clear();
    m_cellErrors.clear();
    m_issues.clear();
    endResetModel();
    return true;
}

void SpeakingEvaluationTableModel::clear()
{
    beginResetModel();
    m_rows.reset();
    m_originalValues.clear();
    m_cellErrors.clear();
    m_issues.clear();
    endResetModel();
}

void SpeakingEvaluationTableModel::markSaved()
{
    m_originalValues.clear();
}

void SpeakingEvaluationTableModel::setUndoStack(QUndoStack* stack) noexcept
{
    m_undoStack = stack;
}

void SpeakingEvaluationTableModel::setValidationIssues(
    std::vector<Application::SpeakingEvaluationValidationIssue> issues
    )
{
    m_issues = std::move(issues);
    m_cellErrors.clear();
    for (const auto& issue : m_issues)
    {
        const int row = issue.row.rawValue();
        const int column = issue.column.rawValue();
        if (row < 0 || row >= Application::SpeakingEvaluationRowCount
            || column < 0 || column >= Application::SpeakingEvaluationColumnCount)
        {
            continue;
        }
        m_cellErrors[cellKey(row, column)].append(validationMessage(issue));
    }
    if (m_rows)
    {
        emit dataChanged(
            index(0, 0),
            index(Application::SpeakingEvaluationRowCount - 1,
                Application::SpeakingEvaluationColumnCount - 1),
            {Qt::ToolTipRole, Qt::BackgroundRole});
    }
}

void SpeakingEvaluationTableModel::retranslate()
{
    emit headerDataChanged(
        Qt::Horizontal,
        0,
        Application::SpeakingEvaluationColumnCount - 1);
    if (m_rows)
    {
        emit dataChanged(
            index(0, 0),
            index(Application::SpeakingEvaluationRowCount - 1,
                Application::SpeakingEvaluationColumnCount - 1),
            {Qt::ToolTipRole});
    }
}

bool SpeakingEvaluationTableModel::hasMatrix() const noexcept
{
    return m_rows.has_value();
}

bool SpeakingEvaluationTableModel::isDirty() const noexcept
{
    return !m_originalValues.empty();
}

Application::SpeakingEvaluationSnapshot
SpeakingEvaluationTableModel::snapshot() const
{
    Application::SpeakingEvaluationSnapshot result;
    if (!m_rows)
    {
        return result;
    }
    result.rows.reserve(m_rows->size());
    for (const QStringList& sourceRow : *m_rows)
    {
        std::vector<std::u16string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& value : sourceRow)
        {
            row.push_back(value.toStdU16String());
        }
        result.rows.push_back(std::move(row));
    }
    result.changedCells.reserve(m_originalValues.size());
    for (const auto& [coordinate, original] : m_originalValues)
    {
        static_cast<void>(original);
        result.changedCells.push_back({
            Application::SpeakingEvaluationCellChangeRowIndex{coordinate.first},
            Application::SpeakingEvaluationCellChangeColumnIndex{coordinate.second}
        });
    }
    return result;
}

QString SpeakingEvaluationTableModel::cellText(
    const int row,
    const int column
    ) const
{
    if (!m_rows || row < 0 || row >= Application::SpeakingEvaluationRowCount
        || column < 0 || column >= Application::SpeakingEvaluationColumnCount)
    {
        return {};
    }
    return m_rows->at(static_cast<std::size_t>(row)).at(column);
}

QStringList SpeakingEvaluationTableModel::errorsForCell(
    const int row,
    const int column
    ) const
{
    return m_cellErrors.value(cellKey(row, column));
}

bool SpeakingEvaluationTableModel::setCellValue(
    const int row,
    const int column,
    QString value
    )
{
    if (!m_rows || row < 0 || row >= Application::SpeakingEvaluationRowCount
        || column <= 0 || column >= Application::SpeakingEvaluationColumnCount)
    {
        return false;
    }
    value = normalizedValue(column, value);
    const QString before = cellText(row, column);
    if (before == value)
    {
        return false;
    }
    if (m_undoStack)
    {
        m_undoStack->push(new SpeakingEvaluationCellEditCommand(
            *this, row, column, before, std::move(value)));
    }
    else
    {
        applyCommandValue(row, column, value);
    }
    return true;
}

void SpeakingEvaluationTableModel::applyCommandValue(
    const int row,
    const int column,
    const QString& value
    )
{
    if (!m_rows || row < 0 || row >= Application::SpeakingEvaluationRowCount
        || column <= 0 || column >= Application::SpeakingEvaluationColumnCount)
    {
        return;
    }
    QStringList& cells = m_rows->at(static_cast<std::size_t>(row));
    const QString before = cells.at(column);
    if (before == value)
    {
        return;
    }

    const auto coordinate = std::pair{row, column};
    auto original = m_originalValues.find(coordinate);
    if (original == m_originalValues.end())
    {
        original = m_originalValues.emplace(coordinate, before).first;
    }
    cells[column] = value;
    if (original->second == value)
    {
        m_originalValues.erase(original);
    }
    emit dataChanged(
        index(row, column),
        index(row, column),
        {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole, Qt::BackgroundRole});
}

QString SpeakingEvaluationTableModel::normalizedValue(
    const int column,
    const QString& value
    ) const
{
    const std::u16string source = value.toStdU16String();
    std::u16string normalized = source;
    if (column == 1)
    {
        normalized = Application::SpeakingEvaluationValidationDetail::
            normalizeEnglishName(source);
    }
    else if (column == 2)
    {
        normalized = Application::SpeakingEvaluationValidationDetail::
            normalizeSpeakingEvaluationKoreanName(source);
    }
    else if (column >= 3 && column <= 8)
    {
        normalized = Application::SpeakingEvaluationValidationDetail::
            normalizeScore(source);
    }
    return fromUtf16(normalized);
}

QString SpeakingEvaluationTableModel::validationMessage(
    const Application::SpeakingEvaluationValidationIssue& issue
    )
{
    using Code = Application::SpeakingEvaluationValidationCode;
    switch (issue.code)
    {
    case Code::StudentNameEnglishTooLong:
        return translatedModelText("English name must be 20 characters or fewer.");
    case Code::StudentNameEnglishNonAscii:
        return translatedModelText("Only standard English letters are allowed.");
    case Code::StudentNameEnglishInvalidCharacters:
        return translatedModelText("English name contains unsupported characters.");
    case Code::StudentNameKoreanInvalidCharacters:
        return translatedModelText("Korean name contains unsupported characters.");
    case Code::StudentNameKoreanTooShort:
    case Code::StudentNameKoreanTooLong:
        return translatedModelText("Invalid Korean name length.");
    case Code::StudentNameKoreanUnusualLength:
        return translatedModelText("Uncommon Korean name length. Please verify.");
    case Code::StudentNameDuplicatePair:
        return translatedModelText("This student name pair is duplicated.");
    case Code::SpeakingEvaluationStudentNameRequired:
        return translatedModelText("Enter both an English and a Korean name.");
    case Code::ValidationEnumInvalidValue:
        return translatedModelText("Enter a valid score (A+, A, B+, B, or C).");
    case Code::ValidationLengthOutOfBounds:
        if (issue.column.rawValue() == 9)
        {
            return translatedModelText("Comment must be 450 characters or fewer.");
        }
        if (issue.column.rawValue() == 10)
        {
            return translatedModelText("Notes must be 10000 characters or fewer.");
        }
        return translatedModelText("This value is longer than the supported limit.");
    case Code::SpeakingEvaluationClassIdInvalid:
    case Code::SpeakingEvaluationRowsTooMany:
    case Code::SpeakingEvaluationRowTooManyCells:
        return translatedModelText("This evaluation data is invalid.");
    }
    return translatedModelText("This value is invalid.");
}

} // namespace ClassMngr::Next::Ui
