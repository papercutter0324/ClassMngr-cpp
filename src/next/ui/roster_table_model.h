#pragma once

#include "next/application/roster_save_preparation.h"
#include "next/application/roster_snapshot.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>
#include <vector>

namespace ClassMngr::Next::Ui
{

class RosterTableModel final : public QAbstractTableModel
{
public:
    static constexpr int ModeledRowCount = 25;

    explicit RosterTableModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole
        ) override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole
        ) const override;

    void setRoster(const Application::RosterSnapshot& roster);
    void clear();
    void setEditable(bool editable);
    void setDirtyChangedCallback(std::function<void(bool)> callback);
    void retranslate();
    void setColumnWidth(int column, int width);
    [[nodiscard]] Application::RosterSnapshot snapshot() const;
    [[nodiscard]] const std::vector<Application::RosterSaveIssue>&
        validationIssues() const noexcept;
    [[nodiscard]] std::optional<QModelIndex> firstErrorIndex() const;
    [[nodiscard]] QString columnName(int column) const;
    [[nodiscard]] bool isRequiredColumn(int column) const;
    [[nodiscard]] bool isEvaluationColumn(int column) const;
    [[nodiscard]] int columnWidth(int column) const noexcept;
    [[nodiscard]] bool isDirty() const noexcept;
    void clearDirty();

    [[nodiscard]] bool appendCustomColumn(
        const QString& name,
        QString* rejection = nullptr
        );
    [[nodiscard]] std::optional<QString> removeCustomColumn(int column);
    [[nodiscard]] bool removeRowData(int row);
    [[nodiscard]] bool moveRowData(int sourceRow, int destinationRow);

private:
    void replaceFromSnapshot(
        const Application::RosterSnapshot& roster,
        bool markDirty
        );
    void revalidate();
    void setDirty(bool dirty);
    [[nodiscard]] bool isNameColumn(int column, const QString& name) const;

    QVector<QString> m_columns;
    QVector<int> m_widths;
    QVector<QStringList> m_rows;
    std::vector<Application::RosterSaveIssue> m_issues;
    QHash<QString, QStringList> m_cellErrors;
    QHash<QString, QStringList> m_cellWarnings;
    bool m_editable = false;
    bool m_dirty = false;
    std::function<void(bool)> m_dirtyChanged;
};

} // namespace ClassMngr::Next::Ui
