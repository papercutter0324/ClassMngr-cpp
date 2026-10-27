#pragma once

#include "next/application/gs_team_directory_read_snapshot.h"
#include "next/application/native_english_teacher_directory_read_snapshot.h"

#include <QAbstractTableModel>
#include <QString>

#include <optional>
#include <QStringList>
#include <variant>
#include <vector>

namespace ClassMngr::Next::Ui
{

enum class StaffDirectoryKind
{
    NativeEnglishTeachers,
    GsTeam
};

struct NativeEnglishTeacherDirectoryRow final
{
    std::optional<Domain::NativeEnglishTeacherId> id;
    QString name;
    QString position;
    QString phoneNumber;
    QString email;
    QString birthday;
    QString nationality;
};

struct GsTeamDirectoryRow final
{
    std::optional<Domain::GsTeamMemberId> id;
    QString name;
    QString koreanName;
    QString position;
    QString phoneNumber;
    QString birthday;
};

// Each stored object represents a directory record. The view asks the model
// for individual values and creates an editor only for the active cell.
class SharedStaffDirectoryTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SharedStaffDirectoryTableModel(
        StaffDirectoryKind kind,
        QObject* parent = nullptr
        );

    [[nodiscard]] StaffDirectoryKind kind() const noexcept;
    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()
        ) const override;
    [[nodiscard]] int columnCount(
        const QModelIndex& parent = QModelIndex()
        ) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole
        ) override;
    bool insertRows(
        int row,
        int count,
        const QModelIndex& parent = QModelIndex()
        ) override;
    bool removeRows(
        int row,
        int count,
        const QModelIndex& parent = QModelIndex()
        ) override;

    void replaceRows(
        const Application::NativeEnglishTeacherDirectorySnapshot& rows
        );
    void replaceRows(const Application::GsTeamDirectorySnapshot& rows);
    [[nodiscard]] const NativeEnglishTeacherDirectoryRow& nativeRow(
        int row
        ) const;
    [[nodiscard]] const GsTeamDirectoryRow& gsRow(int row) const;
    [[nodiscard]] std::size_t storedRecordCount() const noexcept;
    void retranslateHeaders();

private:
    using Rows = std::variant<
        std::vector<NativeEnglishTeacherDirectoryRow>,
        std::vector<GsTeamDirectoryRow>>;

    [[nodiscard]] bool validCell(const QModelIndex& index) const noexcept;
    [[nodiscard]] QString cellText(int row, int column) const;
    bool setCellText(int row, int column, QString value);

    StaffDirectoryKind m_kind;
    Rows m_rows;
};

} // namespace ClassMngr::Next::Ui
