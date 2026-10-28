#pragma once

#include "next/application/my_classes_page_read.h"

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QString>

#include <optional>
#include <vector>

namespace ClassMngr::Next::Ui
{

class MyClassesSummaryModel final : public QAbstractItemModel
{
public:
    enum Role
    {
        ClassIdRole = Qt::UserRole + 1,
        IsGradeGroupRole
    };

    explicit MyClassesSummaryModel(QObject* parent = nullptr);

    [[nodiscard]] QModelIndex index(
        int row,
        int column,
        const QModelIndex& parent = {}
        ) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setSummaries(Application::MyClassesPageSummarySnapshot summaries);
    void clear();
    void retranslate();

    [[nodiscard]] const Application::MyClassesPageClassSummary* summaryForIndex(
        const QModelIndex& index
        ) const noexcept;
    [[nodiscard]] QModelIndex indexForClassId(
        const Domain::ClassId& classId
        ) const;
    [[nodiscard]] const Application::MyClassesPageSummarySnapshot& summaries()
        const noexcept;
    [[nodiscard]] int summaryCount() const noexcept;
    [[nodiscard]] bool isGrouped() const noexcept;
    [[nodiscard]] int gradeGroupCount() const noexcept;
    [[nodiscard]] int gradeGroupChildCount(int row) const noexcept;
    [[nodiscard]] QString classLabel(int summaryIndex) const;

private:
    struct Item final
    {
        bool gradeGroup = false;
        int summaryIndex = -1;
        int groupIndex = -1;
    };

    [[nodiscard]] int summaryIndex(const QModelIndex& index) const noexcept;
    [[nodiscard]] int groupIndexForGrade(const QString& grade) const noexcept;
    void rebuildHierarchy();

    Application::MyClassesPageSummarySnapshot m_summaries;
    std::vector<Item> m_groupItems;
    std::vector<Item> m_classItems;
    std::vector<QString> m_groupLabels;
    std::vector<QString> m_classLabels;
    std::vector<std::vector<int>> m_groupChildren;
    bool m_grouped = false;
};

} // namespace ClassMngr::Next::Ui
