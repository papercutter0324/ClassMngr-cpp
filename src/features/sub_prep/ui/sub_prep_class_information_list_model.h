#pragma once

#include "next/application/class_summary_projection.h"

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>

#include <optional>
#include <vector>

class SubPrepClassInformationListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        ClassIdRole = Qt::UserRole + 1,
        GradeRole,
        LevelRole,
        MeetingTextRole,
        TeacherDisplayNameRole,
        StudentCountRole
    };

    explicit SubPrepClassInformationListModel(
        QObject* parent = nullptr
        );

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = {}
        ) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole
        ) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setProjection(
        ClassMngr::Next::Application::ClassSummaryProjection projection
        );
    void setCurrentGrade(
        const QString& grade
        );

    [[nodiscard]] const QStringList& grades() const noexcept;
    [[nodiscard]] const QString& currentGrade() const noexcept;
    [[nodiscard]] const ClassMngr::Next::Application::ClassSummaryProjection&
        projection() const noexcept;
    [[nodiscard]] const ClassMngr::Next::Application::ClassSummary*
        summaryAt(int row) const noexcept;
    [[nodiscard]] std::optional<ClassMngr::Next::Domain::ClassId>
        classIdAt(int row) const;
    [[nodiscard]] int rowForClassId(
        const ClassMngr::Next::Domain::ClassId& classId
        ) const noexcept;

private:
    void rebuildVisibleRows();
    [[nodiscard]] QString teacherDisplayName(
        const ClassMngr::Next::Application::ClassSummary& summary
        ) const;

    ClassMngr::Next::Application::ClassSummaryProjection m_projection;
    QStringList m_grades;
    QString m_currentGrade;
    std::vector<std::size_t> m_visibleRows;
    QStringList m_displayLabels;
};
