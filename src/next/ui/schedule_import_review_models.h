#pragma once

#include "next/ui/schedule_import_request_mapper.h"

#include <QAbstractTableModel>
#include <QStyledItemDelegate>

#include <optional>
#include <vector>

namespace ClassMngr::Next::Ui
{

class ScheduleImportTeacherReviewModel final : public QAbstractTableModel
{
public:
    enum Column
    {
        TeacherColumn,
        MatchesColumn,
        ActionColumn,
        TargetColumn,
        RoomColumn,
        ColumnCount
    };

    explicit ScheduleImportTeacherReviewModel(QObject* parent = nullptr);

    void setMapping(ScheduleImportReviewMapping* mapping);
    void retranslateHeaders();
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
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

    [[nodiscard]] Application::ScheduleImportReviewDecisionRequest decisions() const;
    [[nodiscard]] std::vector<std::optional<Domain::TeacherId>> teacherIds() const;

private:
    ScheduleImportReviewMapping* m_mapping = nullptr;
};

class ScheduleImportClassReviewModel final : public QAbstractTableModel
{
public:
    enum Column
    {
        ClassColumn,
        MatchesColumn,
        ActionColumn,
        TargetColumn,
        ColumnCount
    };

    explicit ScheduleImportClassReviewModel(QObject* parent = nullptr);

    void setMapping(ScheduleImportReviewMapping* mapping);
    void retranslateHeaders();
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
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

    [[nodiscard]] Application::ScheduleImportReviewDecisionRequest decisions() const;

private:
    ScheduleImportReviewMapping* m_mapping = nullptr;
};

class ScheduleImportCandidatePreviewModel final : public QAbstractTableModel
{
public:
    enum Column
    {
        TeacherColumn,
        ClassColumn,
        TimesColumn,
        ColumnCount
    };

    explicit ScheduleImportCandidatePreviewModel(QObject* parent = nullptr);

    void setUser(const ScheduleImportUserBlock* user);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole
        ) const override;

private:
    const ScheduleImportUserBlock* m_user = nullptr;
};

class ScheduleImportChoiceDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    [[nodiscard]] QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
        ) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(
        QWidget* editor,
        QAbstractItemModel* model,
        const QModelIndex& index
        ) const override;
};

} // namespace ClassMngr::Next::Ui
