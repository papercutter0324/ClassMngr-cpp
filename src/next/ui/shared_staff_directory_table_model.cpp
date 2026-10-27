#include "next/ui/shared_staff_directory_table_model.h"

#include <QCoreApplication>

#include <stdexcept>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

QString displayPosition(const QString& position)
{
    if (position == QStringLiteral("Co-ordinator"))
    {
        return QCoreApplication::translate(
            "StaffDirectoryPage",
            "Coordinator"
            );
    }
    return position;
}

QStringList nativeHeaders()
{
    return {
        QCoreApplication::translate("StaffDirectoryPage", "Name"),
        QCoreApplication::translate("StaffDirectoryPage", "Position"),
        QCoreApplication::translate("StaffDirectoryPage", "Phone Number"),
        QCoreApplication::translate("StaffDirectoryPage", "Email"),
        QCoreApplication::translate("StaffDirectoryPage", "Birthday"),
        QCoreApplication::translate("StaffDirectoryPage", "Nationality")
    };
}

QStringList gsHeaders()
{
    return {
        QCoreApplication::translate("StaffDirectoryPage", "Name"),
        QCoreApplication::translate("StaffDirectoryPage", "Korean Name"),
        QCoreApplication::translate("StaffDirectoryPage", "Position"),
        QCoreApplication::translate("StaffDirectoryPage", "Phone Number"),
        QCoreApplication::translate("StaffDirectoryPage", "Birthday")
    };
}

} // namespace

SharedStaffDirectoryTableModel::SharedStaffDirectoryTableModel(
    const StaffDirectoryKind kind,
    QObject* parent
    )
    : QAbstractTableModel(parent)
    , m_kind(kind)
    , m_rows(kind == StaffDirectoryKind::NativeEnglishTeachers
          ? Rows(std::vector<NativeEnglishTeacherDirectoryRow>{})
          : Rows(std::vector<GsTeamDirectoryRow>{}))
{
}

StaffDirectoryKind SharedStaffDirectoryTableModel::kind() const noexcept
{
    return m_kind;
}

int SharedStaffDirectoryTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(std::visit(
        [](const auto& rows) { return rows.size(); },
        m_rows));
}

int SharedStaffDirectoryTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_kind == StaffDirectoryKind::NativeEnglishTeachers ? 6 : 5;
}

QVariant SharedStaffDirectoryTableModel::data(
    const QModelIndex& index,
    const int role
    ) const
{
    if (!validCell(index))
    {
        return {};
    }
    if (role == Qt::TextAlignmentRole)
    {
        return static_cast<int>(Qt::AlignCenter);
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole)
    {
        return {};
    }

    const QString value = cellText(index.row(), index.column());
    if (role == Qt::DisplayRole && index.column() == 1
        && m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        return displayPosition(value);
    }
    return value;
}

QVariant SharedStaffDirectoryTableModel::headerData(
    const int section,
    const Qt::Orientation orientation,
    const int role
    ) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }
    const QStringList headers = m_kind == StaffDirectoryKind::NativeEnglishTeachers
        ? nativeHeaders()
        : gsHeaders();
    return section >= 0 && section < headers.size()
        ? QVariant(headers.at(section))
        : QVariant{};
}

Qt::ItemFlags SharedStaffDirectoryTableModel::flags(
    const QModelIndex& index
    ) const
{
    if (!validCell(index))
    {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

bool SharedStaffDirectoryTableModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    const int role
    )
{
    if (role != Qt::EditRole || !validCell(index))
    {
        return false;
    }
    if (!setCellText(index.row(), index.column(), value.toString()))
    {
        return false;
    }
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

bool SharedStaffDirectoryTableModel::insertRows(
    const int row,
    const int count,
    const QModelIndex& parent
    )
{
    if (parent.isValid() || count <= 0 || row < 0 || row > rowCount())
    {
        return false;
    }
    beginInsertRows({}, row, row + count - 1);
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        auto& rows = std::get<std::vector<NativeEnglishTeacherDirectoryRow>>(m_rows);
        rows.insert(
            rows.begin() + row,
            static_cast<std::size_t>(count),
            NativeEnglishTeacherDirectoryRow{});
    }
    else
    {
        auto& rows = std::get<std::vector<GsTeamDirectoryRow>>(m_rows);
        rows.insert(rows.begin() + row, static_cast<std::size_t>(count), GsTeamDirectoryRow{});
    }
    endInsertRows();
    return true;
}

bool SharedStaffDirectoryTableModel::removeRows(
    const int row,
    const int count,
    const QModelIndex& parent
    )
{
    if (parent.isValid() || count <= 0 || row < 0 || row + count > rowCount())
    {
        return false;
    }
    beginRemoveRows({}, row, row + count - 1);
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        auto& rows = std::get<std::vector<NativeEnglishTeacherDirectoryRow>>(m_rows);
        rows.erase(rows.begin() + row, rows.begin() + row + count);
    }
    else
    {
        auto& rows = std::get<std::vector<GsTeamDirectoryRow>>(m_rows);
        rows.erase(rows.begin() + row, rows.begin() + row + count);
    }
    endRemoveRows();
    return true;
}

void SharedStaffDirectoryTableModel::replaceRows(
    const Application::NativeEnglishTeacherDirectorySnapshot& rows
    )
{
    if (m_kind != StaffDirectoryKind::NativeEnglishTeachers)
    {
        throw std::logic_error("Native English rows cannot replace GS Team rows.");
    }
    std::vector<NativeEnglishTeacherDirectoryRow> replacement;
    replacement.reserve(rows.size());
    for (const auto& row : rows)
    {
        replacement.push_back({
            .id = row.id,
            .name = QString::fromStdU16String(row.name),
            .position = QString::fromStdU16String(row.position),
            .phoneNumber = QString::fromStdU16String(row.phoneNumber),
            .email = QString::fromStdU16String(row.email),
            .birthday = QString::fromStdU16String(row.birthday),
            .nationality = QString::fromStdU16String(row.nationality)
        });
    }
    beginResetModel();
    m_rows = std::move(replacement);
    endResetModel();
}

void SharedStaffDirectoryTableModel::replaceRows(
    const Application::GsTeamDirectorySnapshot& rows
    )
{
    if (m_kind != StaffDirectoryKind::GsTeam)
    {
        throw std::logic_error("GS Team rows cannot replace Native English rows.");
    }
    std::vector<GsTeamDirectoryRow> replacement;
    replacement.reserve(rows.size());
    for (const auto& row : rows)
    {
        replacement.push_back({
            .id = row.id,
            .name = QString::fromStdU16String(row.name),
            .koreanName = QString::fromStdU16String(row.koreanName),
            .position = QString::fromStdU16String(row.position),
            .phoneNumber = QString::fromStdU16String(row.phoneNumber),
            .birthday = QString::fromStdU16String(row.birthday)
        });
    }
    beginResetModel();
    m_rows = std::move(replacement);
    endResetModel();
}

const NativeEnglishTeacherDirectoryRow&
SharedStaffDirectoryTableModel::nativeRow(const int row) const
{
    if (m_kind != StaffDirectoryKind::NativeEnglishTeachers)
    {
        throw std::logic_error("This is not a Native English directory model.");
    }
    return std::get<std::vector<NativeEnglishTeacherDirectoryRow>>(m_rows).at(
        static_cast<std::size_t>(row));
}

const GsTeamDirectoryRow& SharedStaffDirectoryTableModel::gsRow(
    const int row
    ) const
{
    if (m_kind != StaffDirectoryKind::GsTeam)
    {
        throw std::logic_error("This is not a GS Team directory model.");
    }
    return std::get<std::vector<GsTeamDirectoryRow>>(m_rows).at(
        static_cast<std::size_t>(row));
}

std::size_t SharedStaffDirectoryTableModel::storedRecordCount() const noexcept
{
    return std::visit([](const auto& rows) { return rows.size(); }, m_rows);
}

void SharedStaffDirectoryTableModel::retranslateHeaders()
{
    emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
}

bool SharedStaffDirectoryTableModel::validCell(
    const QModelIndex& index
    ) const noexcept
{
    return index.isValid()
        && index.model() == this
        && index.row() >= 0
        && index.row() < rowCount()
        && index.column() >= 0
        && index.column() < columnCount();
}

QString SharedStaffDirectoryTableModel::cellText(
    const int row,
    const int column
    ) const
{
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        const auto& record = std::get<
            std::vector<NativeEnglishTeacherDirectoryRow>>(m_rows).at(
                static_cast<std::size_t>(row));
        switch (column)
        {
        case 0: return record.name;
        case 1: return record.position;
        case 2: return record.phoneNumber;
        case 3: return record.email;
        case 4: return record.birthday;
        case 5: return record.nationality;
        }
    }
    else
    {
        const auto& record = std::get<std::vector<GsTeamDirectoryRow>>(m_rows).at(
            static_cast<std::size_t>(row));
        switch (column)
        {
        case 0: return record.name;
        case 1: return record.koreanName;
        case 2: return record.position;
        case 3: return record.phoneNumber;
        case 4: return record.birthday;
        }
    }
    return {};
}

bool SharedStaffDirectoryTableModel::setCellText(
    const int row,
    const int column,
    QString value
    )
{
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        auto& record = std::get<
            std::vector<NativeEnglishTeacherDirectoryRow>>(m_rows).at(
                static_cast<std::size_t>(row));
        switch (column)
        {
        case 0: record.name = std::move(value); return true;
        case 1: record.position = std::move(value); return true;
        case 2: record.phoneNumber = std::move(value); return true;
        case 3: record.email = std::move(value); return true;
        case 4: record.birthday = std::move(value); return true;
        case 5: record.nationality = std::move(value); return true;
        }
    }
    else
    {
        auto& record = std::get<std::vector<GsTeamDirectoryRow>>(m_rows).at(
            static_cast<std::size_t>(row));
        switch (column)
        {
        case 0: record.name = std::move(value); return true;
        case 1: record.koreanName = std::move(value); return true;
        case 2: record.position = std::move(value); return true;
        case 3: record.phoneNumber = std::move(value); return true;
        case 4: record.birthday = std::move(value); return true;
        }
    }
    return false;
}

} // namespace ClassMngr::Next::Ui
