#pragma once

#include "next/application/class_summary_projection.h"

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QWidget>

#include <optional>
#include <vector>

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QPaintEvent;
class QResizeEvent;
class QEvent;
class QPoint;
class QRect;

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

// A single model-backed, custom-painted horizontal class-tab selector. It
// keeps one widget for any number of class rows while preserving the legacy
// pill-strip navigation style.
class SubPrepClassInformationTabSelector final : public QWidget
{
    Q_OBJECT

public:
    explicit SubPrepClassInformationTabSelector(QWidget* parent = nullptr);

    void setModel(SubPrepClassInformationListModel* model);
    [[nodiscard]] SubPrepClassInformationListModel* model() const noexcept;
    [[nodiscard]] int currentRow() const noexcept;
    void setCurrentRow(int row);
    [[nodiscard]] int horizontalScrollOffset() const noexcept;
    [[nodiscard]] QRect tabRectForRow(int row) const noexcept;

signals:
    void currentChanged(int row);

protected:
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updateTabMetrics();
    void ensureCurrentVisible();
    [[nodiscard]] int arrowButtonWidth() const noexcept;
    [[nodiscard]] int contentViewportWidth() const noexcept;
    [[nodiscard]] int maximumScrollOffset() const noexcept;
    [[nodiscard]] int rowAt(const QPoint& point) const noexcept;
    void scrollRows(int delta);

    SubPrepClassInformationListModel* m_model = nullptr;
    int m_currentRow = -1;
    int m_horizontalScrollOffset = 0;
    int m_contentWidth = 0;
    int m_tabWidth = 0;
    int m_tabHeight = 0;
    std::vector<int> m_tabPositions;
    int m_hoveredRow = -1;
    bool m_hasOverflow = false;
    bool m_hoveredLeftArrow = false;
    bool m_hoveredRightArrow = false;
};
