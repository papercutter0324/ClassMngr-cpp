#include "next/ui/roster_table_model.h"

#include "next/application/roster_column_projection.h"
#include "next/application/roster_custom_column_append.h"
#include "next/application/roster_custom_column_removal_policy.h"
#include "next/application/roster_evaluation_column_policy.h"
#include "next/application/roster_row_availability.h"
#include "next/application/roster_row_removal.h"
#include "next/application/roster_row_reordering.h"

#include <QBrush>
#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QPalette>
#include <QStringView>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr std::array<std::u16string_view, 6> BaseColumns{
    u"English", u"Korean", u"Winter", u"Speech Contest", u"Summer", u"Fall"
};

QString text(const char* source)
{
    return QCoreApplication::translate("NextRosterEditor", source);
}

QString fromLogicalText(const std::u16string& source)
{
    QString result;
    result.reserve(static_cast<qsizetype>(source.size()));
    for (const char16_t unit : source)
    {
        result.append(QChar(unit));
    }
    return result;
}

bool caseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    )
{
    return QStringView(left.data(), static_cast<qsizetype>(left.size())).compare(
        QStringView(right.data(), static_cast<qsizetype>(right.size())),
        Qt::CaseInsensitive) == 0;
}

QString issueMessage(const Application::RosterSaveIssue& issue)
{
    using Code = Application::RosterSaveIssueCode;
    switch (issue.code)
    {
    case Code::ValidationLengthOutOfBounds:
        return text("This value is longer than the supported limit.");
    case Code::StudentNameEnglishTooLong:
        return text("English names must be 20 characters or fewer.");
    case Code::StudentNameEnglishNonAscii:
        return text("Use English characters for the English name.");
    case Code::StudentNameEnglishInvalidCharacters:
        return text("The English name contains an unsupported character.");
    case Code::StudentNameKoreanInvalidCharacters:
        return text("Use Korean characters for the Korean name.");
    case Code::StudentNameKoreanTooShort:
        return text("The Korean name has one syllable. Check that it is correct.");
    case Code::StudentNameKoreanTooLong:
        return text("The Korean name has five or more syllables. Check that it is correct.");
    case Code::StudentNameKoreanUnusualLength:
        return text("Check the Korean name length.");
    case Code::RosterColumnRequired:
        return text("A required roster column is missing.");
    case Code::RosterColumnDuplicate:
        return text("Column names must be unique.");
    case Code::RosterColumnWidthsInvalidCount:
        return text("Roster column widths do not match the columns.");
    case Code::RosterRowsTooMany:
        return text("The roster contains too many modeled rows.");
    case Code::RosterRowTooManyCells:
        return text("This row contains more cells than roster columns.");
    case Code::RosterStudentNameRequired:
        return text("Enter both an English and a Korean name for this student.");
    case Code::StudentNameDuplicatePair:
        return text("This English and Korean name pair is duplicated.");
    }
    return text("Enter a valid value.");
}

QString cellKey(const int row, const int column)
{
    return QStringLiteral("%1:%2").arg(row).arg(column);
}

double colorLuminance(const QColor& color)
{
    const auto linear = [](const double component) {
        return component <= 0.04045
            ? component / 12.92
            : std::pow((component + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF())
        + 0.7152 * linear(color.greenF())
        + 0.0722 * linear(color.blueF());
}

QColor validationBackground(QColor background)
{
    const QColor textColor = QApplication::palette().color(QPalette::Text);
    const auto contrastRatio = [&textColor](const QColor& candidate) {
        const double backgroundLuminance = colorLuminance(candidate);
        const double textLuminance = colorLuminance(textColor);
        return (std::max(backgroundLuminance, textLuminance) + 0.05)
            / (std::min(backgroundLuminance, textLuminance) + 0.05);
    };
    constexpr double MinimumTextContrast = 4.5;
    if (!background.isValid() || !textColor.isValid()
        || contrastRatio(background) >= MinimumTextContrast)
    {
        return background;
    }

    float hue = -1.0F;
    float saturation = 0.0F;
    float lightness = 0.0F;
    float alpha = 1.0F;
    background.getHslF(&hue, &saturation, &lightness, &alpha);
    const float direction = colorLuminance(textColor)
            > colorLuminance(background)
        ? -1.0F : 1.0F;
    for (int step = 1; step <= 100; ++step)
    {
        QColor candidate;
        const float adjustedLightness = std::clamp(
            lightness + direction * static_cast<float>(step) / 100.0F,
            0.0F, 1.0F);
        candidate.setHslF(hue, saturation, adjustedLightness, alpha);
        if (contrastRatio(candidate) >= MinimumTextContrast)
        {
            return candidate;
        }
    }
    return background;
}

QString normalizedEditedText(
    const QString& value,
    const QString& columnName
    )
{
    const auto utf16 = value.toStdU16String();
    if (columnName.compare(QStringLiteral("English"), Qt::CaseInsensitive) == 0)
    {
        return fromLogicalText(
            Application::RosterSavePreparationDetail::Names::normalizeEnglishName(utf16));
    }
    if (columnName.compare(QStringLiteral("Korean"), Qt::CaseInsensitive) == 0)
    {
        return fromLogicalText(
            Application::RosterSavePreparationDetail::normalizeKoreanName(utf16));
    }
    return fromLogicalText(Application::simplifyQtWhitespace(utf16));
}

} // namespace

RosterTableModel::RosterTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    clear();
}

int RosterTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ModeledRowCount;
}

int RosterTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_columns.size();
}

QVariant RosterTableModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()
        || index.column() < 0 || index.column() >= m_columns.size())
    {
        return {};
    }

    const QString key = cellKey(index.row(), index.column());
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return m_rows.at(index.row()).at(index.column());
    }
    if (role == Qt::TextAlignmentRole)
    {
        return static_cast<int>(Qt::AlignCenter);
    }
    if (role == Qt::ToolTipRole)
    {
        QStringList messages = m_cellErrors.value(key);
        messages.append(m_cellWarnings.value(key));
        return messages.isEmpty() ? QVariant{} : QVariant(messages.join(QLatin1Char('\n')));
    }
    if (role == Qt::BackgroundRole)
    {
        if (!m_cellErrors.value(key).isEmpty())
        {
            return QBrush(validationBackground(QColor(255, 218, 218)));
        }
        if (!m_cellWarnings.value(key).isEmpty())
        {
            return QBrush(validationBackground(QColor(255, 243, 205)));
        }
    }
    return {};
}

bool RosterTableModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    const int role
    )
{
    if (role != Qt::EditRole || !m_editable || !index.isValid()
        || index.row() < 0 || index.row() >= m_rows.size()
        || index.column() < 0 || index.column() >= m_columns.size()
        || isEvaluationColumn(index.column()))
    {
        return false;
    }

    const QString normalized = normalizedEditedText(
        value.toString(), m_columns.at(index.column()));
    if (m_rows[index.row()][index.column()] == normalized)
    {
        return false;
    }

    m_rows[index.row()][index.column()] = normalized;
    setDirty(true);
    revalidate();
    emit dataChanged(index, index, {
        Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole, Qt::BackgroundRole});
    return true;
}

Qt::ItemFlags RosterTableModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags result = QAbstractTableModel::flags(index);
    if (index.isValid() && m_editable && !isEvaluationColumn(index.column()))
    {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

QVariant RosterTableModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role
    ) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Vertical)
    {
        return section + 1;
    }
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal
        && section >= 0 && section < m_columns.size())
    {
        const QString name = m_columns.at(section);
        for (const char* const base : {
                 "English", "Korean", "Winter", "Speech Contest", "Summer", "Fall"})
        {
            if (name.compare(QString::fromLatin1(base), Qt::CaseInsensitive) == 0)
            {
                return text(base);
            }
        }
        return name;
    }
    if (role == Qt::ToolTipRole && orientation == Qt::Horizontal)
    {
        for (const Application::RosterSaveIssue& issue : m_issues)
        {
            if (issue.severity == Application::RosterSaveIssueSeverity::Error
                && issue.row.rawValue() < 0
                && issue.column.rawValue() == section)
            {
                return issueMessage(issue);
            }
        }
    }
    return {};
}

void RosterTableModel::setRoster(const Application::RosterSnapshot& roster)
{
    replaceFromSnapshot(roster, false);
    setDirty(false);
}

void RosterTableModel::clear()
{
    Application::RosterSnapshot empty;
    replaceFromSnapshot(empty, false);
    m_editable = false;
    setDirty(false);
}

void RosterTableModel::setEditable(const bool editable)
{
    if (m_editable == editable)
    {
        return;
    }
    m_editable = editable;
    if (rowCount() > 0 && columnCount() > 0)
    {
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
            {Qt::BackgroundRole, Qt::ToolTipRole});
    }
}

void RosterTableModel::setDirtyChangedCallback(
    std::function<void(bool)> callback
    )
{
    m_dirtyChanged = std::move(callback);
}

void RosterTableModel::retranslate()
{
    revalidate();
}

void RosterTableModel::setColumnWidth(const int column, const int width)
{
    if (column >= 0 && column < m_widths.size())
    {
        m_widths[column] = width;
    }
}

Application::RosterSnapshot RosterTableModel::snapshot() const
{
    Application::RosterSnapshot result;
    result.columns.reserve(static_cast<std::size_t>(m_columns.size()));
    for (const QString& column : m_columns)
    {
        result.columns.push_back(column.toStdU16String());
    }
    result.columnWidths.reserve(static_cast<std::size_t>(m_widths.size()));
    for (const int width : m_widths)
    {
        result.columnWidths.push_back(Application::RosterSnapshotColumnWidth{width});
    }
    result.rows.reserve(static_cast<std::size_t>(m_rows.size()));
    for (const QStringList& sourceRow : m_rows)
    {
        std::vector<std::u16string> row;
        row.reserve(static_cast<std::size_t>(sourceRow.size()));
        for (const QString& cell : sourceRow)
        {
            row.push_back(cell.toStdU16String());
        }
        result.rows.push_back(std::move(row));
    }
    return result;
}

const std::vector<Application::RosterSaveIssue>&
RosterTableModel::validationIssues() const noexcept
{
    return m_issues;
}

std::optional<QModelIndex> RosterTableModel::firstErrorIndex() const
{
    for (const Application::RosterSaveIssue& issue : m_issues)
    {
        if (issue.severity != Application::RosterSaveIssueSeverity::Error)
        {
            continue;
        }
        const int row = issue.row.rawValue();
        const int column = issue.column.rawValue();
        if (row >= 0 && row < rowCount() && column >= 0 && column < columnCount())
        {
            return index(row, column);
        }
        if (row >= 0 && row < rowCount() && column < 0 && columnCount() > 0)
        {
            return index(row, 0);
        }
    }
    return std::nullopt;
}

QString RosterTableModel::columnName(const int column) const
{
    return column >= 0 && column < m_columns.size() ? m_columns.at(column) : QString{};
}

int RosterTableModel::columnWidth(const int column) const noexcept
{
    return column >= 0 && column < m_widths.size() ? m_widths.at(column) : 0;
}

bool RosterTableModel::isRequiredColumn(const int column) const
{
    const QString name = columnName(column);
    return std::ranges::any_of(BaseColumns, [&name](const std::u16string_view required) {
        return caseInsensitiveEquals(name.toStdU16String(), required);
    });
}

bool RosterTableModel::isEvaluationColumn(const int column) const
{
    const QString name = columnName(column);
    return Application::isRosterEvaluationColumnName(name,
        [](const QString& candidate, const std::u16string_view evaluationName) {
            return candidate.compare(fromLogicalText(std::u16string(evaluationName)),
                Qt::CaseInsensitive) == 0;
        });
}

bool RosterTableModel::isDirty() const noexcept
{
    return m_dirty;
}

void RosterTableModel::clearDirty()
{
    setDirty(false);
}

bool RosterTableModel::appendCustomColumn(
    const QString& name,
    QString* rejection
    )
{
    std::vector<std::u16string> required(BaseColumns.begin(), BaseColumns.end());
    auto result = Application::appendRosterCustomColumn(
        snapshot(), name.toStdU16String(), required, caseInsensitiveEquals);
    if (auto* const error = std::get_if<Application::RosterCustomColumnAppendError>(&result))
    {
        if (rejection)
        {
            *rejection = text(error->rejection
                == Application::RosterCustomColumnNameRejection::Empty
                    ? "Enter a column name."
                    : error->rejection
                            == Application::RosterCustomColumnNameRejection::RequiredColumn
                        ? "That name is reserved for a required roster column."
                        : error->rejection
                                == Application::RosterCustomColumnNameRejection::Duplicate
                            ? "A column with that name already exists."
                            : "The column name is longer than the supported limit.");
        }
        return false;
    }
    const auto& updated = std::get<Application::RosterSnapshot>(result);
    replaceFromSnapshot(updated, true);
    return true;
}

std::optional<QString> RosterTableModel::removeCustomColumn(const int column)
{
    const Application::RosterSnapshot before = snapshot();
    const std::vector<std::u16string> required(BaseColumns.begin(), BaseColumns.end());
    const auto eligibility = Application::canRemoveRosterCustomColumn(
        before, Application::RosterCustomColumnRemovalIndex{column}, required,
        caseInsensitiveEquals);
    if (!eligibility.accepted())
    {
        return std::nullopt;
    }

    Application::RosterSnapshot updated = before;
    const QString removedName = m_columns.at(column);
    updated.columns.erase(updated.columns.begin() + column);
    if (static_cast<std::size_t>(column) < updated.columnWidths.size())
    {
        updated.columnWidths.erase(updated.columnWidths.begin() + column);
    }
    for (auto& row : updated.rows)
    {
        if (static_cast<std::size_t>(column) < row.size())
        {
            row.erase(row.begin() + column);
        }
    }
    replaceFromSnapshot(updated, true);
    return removedName;
}

bool RosterTableModel::removeRowData(const int row)
{
    auto result = Application::removeRosterRow(
        snapshot(), Application::RosterRowRemovalIndex{row});
    auto* const updated = std::get_if<Application::RosterSnapshot>(&result);
    if (!updated)
    {
        return false;
    }
    replaceFromSnapshot(*updated, true);
    return true;
}

bool RosterTableModel::moveRowData(
    const int sourceRow,
    const int destinationRow
    )
{
    auto result = Application::reorderRosterRows(
        snapshot(), Application::RosterRowReorderingSourceIndex{sourceRow},
        Application::RosterRowReorderingDestinationIndex{destinationRow});
    auto* const updated = std::get_if<Application::RosterSnapshot>(&result);
    if (!updated)
    {
        return false;
    }
    replaceFromSnapshot(*updated, true);
    return true;
}

void RosterTableModel::replaceFromSnapshot(
    const Application::RosterSnapshot& roster,
    const bool markDirty
    )
{
    beginResetModel();
    const auto projection = Application::RosterColumnProjection::create(
        roster.columns, std::vector<std::u16string>(BaseColumns.begin(), BaseColumns.end()),
        caseInsensitiveEquals);
    m_columns.clear();
    m_widths.clear();
    m_rows.clear();

    m_columns.reserve(static_cast<qsizetype>(projection.columns().size()));
    m_widths.reserve(static_cast<qsizetype>(projection.columns().size()));
    for (const Application::RosterProjectedColumn& column : projection.columns())
    {
        m_columns.append(fromLogicalText(column.name));
        int width = 0;
        if (column.sourceColumnIndex)
        {
            const std::size_t sourceIndex = column.sourceColumnIndex->rawValue();
            if (sourceIndex < roster.columnWidths.size())
            {
                width = roster.columnWidths[sourceIndex].rawValue();
            }
        }
        m_widths.append(width);
    }

    m_rows.reserve(ModeledRowCount);
    for (int rowIndex = 0; rowIndex < ModeledRowCount; ++rowIndex)
    {
        QStringList row;
        row.reserve(m_columns.size());
        for (const Application::RosterProjectedColumn& column : projection.columns())
        {
            QString value;
            const std::size_t sourceColumn = column.sourceColumnIndex
                ? column.sourceColumnIndex->rawValue()
                : roster.columns.size();
            if (static_cast<std::size_t>(rowIndex) < roster.rows.size()
                && sourceColumn < roster.rows[static_cast<std::size_t>(rowIndex)].size())
            {
                value = fromLogicalText(
                    roster.rows[static_cast<std::size_t>(rowIndex)][sourceColumn]);
            }
            row.append(value);
        }
        m_rows.append(std::move(row));
    }
    m_cellErrors.clear();
    m_cellWarnings.clear();
    m_issues.clear();
    m_editable = true;
    m_dirty = markDirty;
    revalidate();
    endResetModel();
    if (m_dirtyChanged)
    {
        m_dirtyChanged(m_dirty);
    }
}

void RosterTableModel::revalidate()
{
    m_cellErrors.clear();
    m_cellWarnings.clear();
    const Application::RosterSavePreparation prepared =
        Application::prepareRosterSaveText(snapshot(), false, caseInsensitiveEquals);
    m_issues = prepared.issues;
    for (const Application::RosterSaveIssue& issue : m_issues)
    {
        if (issue.row.rawValue() < 0 || issue.column.rawValue() < 0
            || issue.row.rawValue() >= rowCount()
            || issue.column.rawValue() >= columnCount())
        {
            continue;
        }
        QHash<QString, QStringList>& destination =
            issue.severity == Application::RosterSaveIssueSeverity::Error
                ? m_cellErrors : m_cellWarnings;
        const QString key = cellKey(issue.row.rawValue(), issue.column.rawValue());
        QStringList messages = destination.value(key);
        const QString message = issueMessage(issue);
        if (!messages.contains(message))
        {
            messages.append(message);
        }
        destination.insert(key, std::move(messages));
    }
    if (rowCount() > 0 && columnCount() > 0)
    {
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
            {Qt::ToolTipRole, Qt::BackgroundRole});
    }
    emit headerDataChanged(Qt::Horizontal, 0, std::max(0, columnCount() - 1));
}

void RosterTableModel::setDirty(const bool dirty)
{
    if (m_dirty == dirty)
    {
        return;
    }
    m_dirty = dirty;
    if (m_dirtyChanged)
    {
        m_dirtyChanged(dirty);
    }
}

bool RosterTableModel::isNameColumn(const int column, const QString& name) const
{
    return column >= 0 && column < m_columns.size()
        && m_columns.at(column).compare(name, Qt::CaseInsensitive) == 0;
}

} // namespace ClassMngr::Next::Ui
