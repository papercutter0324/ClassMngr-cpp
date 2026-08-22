#pragma once

#include "domain/models/classroom.h"
#include "features/classes/services/speaking_analytics.h"
#include "ui/shared/pages/basepage.h"

#include <QList>
#include <QString>

class ApplicationServices;
class CriterionDistributionBar;
class ClassAnalyticsRankingModel;
class GradeHistogram;
class YearToDateChart;
class QComboBox;
class QEvent;
class QGridLayout;
class QLabel;
class QShowEvent;
class QTableView;
class SectionCard;
class QWidget;
struct SpeakingEvaluationDashboard;

// Read-only dashboard for speaking-evaluation results in a class.
class ClassAnalyticsPage : public BasePage
{
    Q_OBJECT

public:
    explicit ClassAnalyticsPage(
        ApplicationServices* services,
        bool embedded = false,
        QWidget* parent = nullptr
        );

    void loadClass(const Classroom& classroom);

    void clearDatabaseState() override;
    void refresh() override;
    void retranslateUi() override;
    bool eventFilter(QObject* object, QEvent* event) override;

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onEvaluationChanged();

private:
    void buildUi();
    void populateEvaluationSelector(const QString& selectedName = {});
    void rebuild();
    void clearDisplay();
    void showEmpty(bool empty);
    void applySnapshot(const SpeakingAnalytics::Snapshot& snapshot);
    void applyDashboard(const SpeakingEvaluationDashboard& dashboard);
    void applyClassShape(
        const SpeakingAnalytics::Snapshot& snapshot,
        const QString& evaluationName
    );
    void applyResponsiveLayout();
    void layoutSummaryCards(int columns);
    void layoutChartCards(bool horizontal);
    void refreshAreaValueTexts();
    void synchronizeCriterionBarStarts();
    void resizeRankingColumnsToContents();
    void setRankingHeaders();

    [[nodiscard]] QString selectedEvaluationName() const;
    [[nodiscard]] QString areaText(
        const QList<QString>& labels,
        const QLabel* value
        ) const;

    ApplicationServices* m_services = nullptr;
    bool m_embedded = false;
    int m_classId = -1;
    bool m_rebuilding = false;

    QWidget* m_dashboardBody = nullptr;
    QLabel* m_heading = nullptr;
    QLabel* m_evaluationLabel = nullptr;
    QComboBox* m_evaluationCombo = nullptr;

    QWidget* m_summaryContainer = nullptr;
    QGridLayout* m_summaryLayout = nullptr;
    QList<SectionCard*> m_summaryCards;
    SectionCard* m_averageCard = nullptr;
    SectionCard* m_assessedCard = nullptr;
    SectionCard* m_strongestCard = nullptr;
    SectionCard* m_focusCard = nullptr;
    QLabel* m_averageValue = nullptr;
    QLabel* m_assessedValue = nullptr;
    QLabel* m_strongestValue = nullptr;
    QLabel* m_focusValue = nullptr;
    QList<QString> m_strongestLabels;
    QList<QString> m_focusLabels;
    int m_summaryColumns = 0;

    QWidget* m_chartsContainer = nullptr;
    QGridLayout* m_chartsLayout = nullptr;
    SectionCard* m_criteriaCard = nullptr;
    SectionCard* m_shapeCard = nullptr;
    QWidget* m_criteriaContainer = nullptr;
    QGridLayout* m_criteriaLayout = nullptr;
    QList<CriterionDistributionBar*> m_criterionBars;
    QLabel* m_histogramCaption = nullptr;
    GradeHistogram* m_histogram = nullptr;
    QLabel* m_yearToDateHeading = nullptr;
    YearToDateChart* m_yearToDateChart = nullptr;
    QString m_classShapeEvaluationName;
    int m_chartsHorizontal = -1;

    SectionCard* m_rankingCard = nullptr;
    QTableView* m_rankingTable = nullptr;
    ClassAnalyticsRankingModel* m_rankingModel = nullptr;
    QLabel* m_emptyLabel = nullptr;
};
