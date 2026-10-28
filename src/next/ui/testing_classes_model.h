#pragma once

#include "next/application/schedule_testing_class_choices_query.h"

#include <QAbstractListModel>
#include <QString>

#include <optional>
#include <vector>

namespace ClassMngr::Next::Ui
{

class TestingClassesModel final : public QAbstractListModel
{
public:
    enum Role
    {
        ClassIdRole = Qt::UserRole + 1,
        ClassNameRole,
        GradeRole,
        LevelRole,
        RoomRole
    };

    explicit TestingClassesModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;

    void setSnapshot(
        const Application::ScheduleTestingClassChoicesSnapshot& snapshot
        );
    void clear();
    [[nodiscard]] int classCount() const noexcept;
    [[nodiscard]] std::optional<Domain::ClassId> classIdAt(
        const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexForClassId(
        const Domain::ClassId& classId) const;

private:
    struct Row final
    {
        Domain::ClassId classId;
        QString name;
        QString grade;
        QString level;
        QString room;
    };

    std::vector<Row> m_rows;
};

} // namespace ClassMngr::Next::Ui
