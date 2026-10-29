#pragma once

#include "next/application/classes_list_read_snapshot.h"
#include "next/application/speaking_evaluation_validation.h"

#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QHash>
#include <QStringList>
#include <QUndoStack>

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Ui
{

class SpeakingEvaluationClassListModel final : public QAbstractListModel
{
public:
    enum Role
    {
        ClassIdRole = Qt::UserRole + 1
    };

    explicit SpeakingEvaluationClassListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setClasses(Application::ClassesListSnapshot snapshot);
    void clear();
    [[nodiscard]] std::optional<Domain::ClassId> classIdAt(int row) const;
    [[nodiscard]] int rowForClassId(const Domain::ClassId& classId) const;
    [[nodiscard]] std::size_t summaryCount() const noexcept;

private:
    Application::ClassesListSnapshot m_snapshot;
};

// The view owns one fixed 25 by 11 value matrix while an evaluation is active.
// Saved values are tracked only for changed cells, so the page does not keep a
// second full matrix as a dirty-state baseline.
class SpeakingEvaluationTableModel final : public QAbstractTableModel
{
public:
    explicit SpeakingEvaluationTableModel(QObject* parent = nullptr);

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

    [[nodiscard]] bool loadRows(
        const std::vector<std::vector<std::u16string>>& rows
        );
    void clear();
    void markSaved();
    void setUndoStack(QUndoStack* stack) noexcept;
    void setValidationIssues(
        std::vector<Application::SpeakingEvaluationValidationIssue> issues
        );
    void retranslate();

    [[nodiscard]] bool hasMatrix() const noexcept;
    [[nodiscard]] bool isDirty() const noexcept;
    [[nodiscard]] Application::SpeakingEvaluationSnapshot snapshot() const;
    [[nodiscard]] QString cellText(int row, int column) const;
    [[nodiscard]] QStringList errorsForCell(int row, int column) const;

private:
    friend class SpeakingEvaluationCellEditCommand;

    [[nodiscard]] bool setCellValue(int row, int column, QString value);
    void applyCommandValue(int row, int column, const QString& value);
    [[nodiscard]] QString normalizedValue(int column, const QString& value) const;
    [[nodiscard]] static QString validationMessage(
        const Application::SpeakingEvaluationValidationIssue& issue
        );

    std::optional<std::vector<QStringList>> m_rows;
    std::map<std::pair<int, int>, QString> m_originalValues;
    QHash<QString, QStringList> m_cellErrors;
    std::vector<Application::SpeakingEvaluationValidationIssue> m_issues;
    QUndoStack* m_undoStack = nullptr;
};

} // namespace ClassMngr::Next::Ui
