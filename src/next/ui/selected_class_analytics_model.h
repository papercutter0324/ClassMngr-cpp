#pragma once

#include "next/application/class_analytics_dashboard_read_query.h"

#include <QAbstractTableModel>
#include <QStringList>
#include <QWidget>

namespace ClassMngr::Next::Ui
{

class SelectedClassAnalyticsModel final : public QAbstractTableModel
{
public:
    enum Column
    {
        Rank,
        EnglishName,
        KoreanName,
        Average,
        Grammar,
        Pronunciation,
        Fluency,
        Manner,
        Content,
        OverallEffort,
        ColumnCount
    };

    explicit SelectedClassAnalyticsModel(QObject* parent = nullptr);

    void setRankings(
        const std::vector<Application::ClassAnalyticsStudentRank>* rankings
        );
    void setHeaderLabels(const QStringList& labels);

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
    const std::vector<Application::ClassAnalyticsStudentRank>* m_rankings = nullptr;
    QStringList m_headers;
};

// Paints one selected-class summary and its criterion, class-shape, and YTD
// charts without allocating per-student or per-criterion widget trees.
class SelectedClassAnalyticsSummaryWidget final : public QWidget
{
public:
    explicit SelectedClassAnalyticsSummaryWidget(QWidget* parent = nullptr);

    void setDashboard(const Application::ClassAnalyticsDashboard* dashboard);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    const Application::ClassAnalyticsDashboard* m_dashboard = nullptr;
};

} // namespace ClassMngr::Next::Ui
