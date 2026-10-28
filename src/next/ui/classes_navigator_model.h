#pragma once

#include "next/application/class_day_filter_policy.h"
#include "next/application/classes_list_read_snapshot.h"
#include "next/application/classes_navigation_snapshot.h"

#include <QAbstractListModel>
#include <QModelIndex>
#include <QString>

#include <optional>
#include <vector>

namespace ClassMngr::Next::Ui
{

class ClassesNavigatorModel final : public QAbstractListModel
{
public:
    enum Role
    {
        ClassIdRole = Qt::UserRole + 1
    };

    struct Group final
    {
        QString key;
        QString label;
    };

    explicit ClassesNavigatorModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setSnapshot(
        Application::ClassesListSnapshot classes,
        std::optional<Application::ClassesNavigationSnapshot> navigation,
        Application::ClassDayFilterPolicy filter
        );
    void setCurrentGroup(const QString& key);
    void clear();
    void retranslate();

    [[nodiscard]] const std::vector<Group>& groups() const noexcept;
    [[nodiscard]] const QString& currentGroup() const noexcept;
    [[nodiscard]] int classCount() const noexcept;
    [[nodiscard]] int visibleClassCount() const noexcept;
    [[nodiscard]] int gradeGroupCount() const noexcept;
    [[nodiscard]] bool hasNavigationMetadata() const noexcept;
    [[nodiscard]] std::optional<Domain::ClassId> classIdAt(
        const QModelIndex& index
        ) const;
    [[nodiscard]] std::optional<Domain::ClassId> classIdAtRow(int row) const;
    [[nodiscard]] QModelIndex indexForClassId(
        const Domain::ClassId& classId
        ) const;
    [[nodiscard]] QString labelForClassId(
        const Domain::ClassId& classId
        ) const;
    [[nodiscard]] std::vector<Domain::ClassId> classIdsForGroup(
        const QString& key
        ) const;

    [[nodiscard]] static QString allClassesGroupKey();

private:
    struct Row final
    {
        std::size_t classIndex = 0;
        QString label;
    };

    struct GradeGroup final
    {
        Group group;
        std::vector<Row> rows;
    };

    [[nodiscard]] const std::vector<Row>* rowsForGroup(
        const QString& key
        ) const noexcept;
    [[nodiscard]] std::vector<Row>* rowsForGroup(
        const QString& key
        ) noexcept;
    void rebuild();

    Application::ClassesListSnapshot m_classes;
    std::optional<Application::ClassesNavigationSnapshot> m_navigation;
    Application::ClassDayFilterPolicy m_filter;
    std::vector<GradeGroup> m_gradeGroups;
    GradeGroup m_allGroup;
    std::vector<Group> m_groups;
    QString m_currentGroupKey;
};

} // namespace ClassMngr::Next::Ui
